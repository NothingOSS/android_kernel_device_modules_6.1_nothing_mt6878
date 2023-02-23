// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#include <linux/of_irq.h>
#include <linux/interrupt.h>
#include <linux/workqueue.h>
#include <linux/iommu.h>
#include <linux/delay.h>
#include <mt-plat/aee.h>

#include "apu.h"
#include "apu_ce_excep.h"
#include "apu_config.h"
#include "apusys_secure.h"
#include "hw_logger.h"
#include "apu_regdump.h"

enum apusys_assert_module {
	//job id
	assert_apusys_ce_TPPA_plus_BW_acc = 0,
	assert_apusys_ce_norm2lp,
	assert_apusys_ce_lp2norm,
	assert_apusys_ce_RCX_Wakeup,
	assert_apusys_ce_RCX_Sleep,
	assert_apusys_ce_TPPA_plus_PSC,
	assert_apusys_ce_BW_Prediction,
	assert_apusys_ce_QoS_event_driven,
	assert_apusys_ce_sMMU_restore,
	assert_apusys_ce_RCX_NoC_BW_acc,
	assert_apusys_ce_ACX0_NoC_BW_acc,
	assert_apusys_ce_ACX1_NoC_BW_acc,
	assert_apusys_ce_NCX_NoC_BW_acc,
	assert_apusys_ce_DVFS,
	assert_apusys_ce_no_module,

	assert_ce_module_max,
};

static const char * const apusys_ce_assert_module_name[assert_ce_module_max] = {
	"APUSYS_CE_TPPA_PLUS_BW_ACC",
	"APUSYS_CE_NORM2LP",
	"APUSYS_CE_LP2NORM",
	"APUSYS_CE_RCX_WAKEUP",
	"APUSYS_CE_RCX_SLEEP",
	"APUSYS_CE_TPPA_PLUS_PSC",
	"APUSYS_CE_BW_PREDICTION",
	"APUSYS_CE_QOS_EVENT_DRIVEN",
	"APUSYS_CE_SMMU_RESTORE",
	"APUSYS_CE_RCX_NOC_BW_ACC",
	"APUSYS_CE_ACX0_NOC_BW_ACC",
	"APUSYS_CE_ACX1_NOC_BW_ACC",
	"APUSYS_CE_NCX_NOC_BW_ACC",
	"APUSYS_CE_DVFS",
	"APUSYS_CE_NO_MODULE",
};

struct apu_coredump_work_struct {
	struct mtk_apu *apu;
	struct work_struct work;
};

static struct apu_coredump_work_struct apu_ce_coredump_work;

static uint32_t apusys_rv_smc_call(struct device *dev, uint32_t smc_id,
	uint32_t a2)
{
	struct arm_smccc_res res;

	dev_info(dev, "%s: smc call %d\n",
			__func__, smc_id);

	arm_smccc_smc(MTK_SIP_APUSYS_CONTROL, smc_id,
				a2, 0, 0, 0, 0, 0, &res);
	if (smc_id == MTK_APUSYS_KERNEL_OP_APUSYS_RV_DBG_APB_ATTACH)
		dev_info(dev, "%s: smc call return(0x%lx)\n",
			__func__, res.a0);
	else if (((int) res.a0) < 0)
		dev_info(dev, "%s: smc call %d return error(%ld)\n",
			__func__, smc_id, res.a0);

	return res.a0;
}

static const char *job_id_mapping(uint32_t job_id)
{
	switch (job_id) {
	case 0:
		return apusys_ce_assert_module_name[assert_apusys_ce_TPPA_plus_BW_acc];
	case 1:
		return apusys_ce_assert_module_name[assert_apusys_ce_norm2lp];
	case 2:
		return apusys_ce_assert_module_name[assert_apusys_ce_lp2norm];
	case 16:
		return apusys_ce_assert_module_name[assert_apusys_ce_RCX_Wakeup];
	case 17:
		return apusys_ce_assert_module_name[assert_apusys_ce_RCX_Sleep];
	case 18:
		return apusys_ce_assert_module_name[assert_apusys_ce_TPPA_plus_PSC];
	case 22:
		return apusys_ce_assert_module_name[assert_apusys_ce_BW_Prediction];
	case 23:
		return apusys_ce_assert_module_name[assert_apusys_ce_QoS_event_driven];
	case 26:
		return apusys_ce_assert_module_name[assert_apusys_ce_sMMU_restore];
	case 27:
		return apusys_ce_assert_module_name[assert_apusys_ce_RCX_NoC_BW_acc];
	case 28:
		return apusys_ce_assert_module_name[assert_apusys_ce_ACX0_NoC_BW_acc];
	case 29:
		return apusys_ce_assert_module_name[assert_apusys_ce_ACX1_NoC_BW_acc];
	case 30:
		return apusys_ce_assert_module_name[assert_apusys_ce_NCX_NoC_BW_acc];
	case 31:
		return apusys_ce_assert_module_name[assert_apusys_ce_DVFS];
	default:
		return apusys_ce_assert_module_name[assert_apusys_ce_no_module];
	}

}

static void apu_ce_coredump_work_func(struct work_struct *p_work)
{
	struct apu_coredump_work_struct *apu_coredump_work =
			container_of(p_work, struct apu_coredump_work_struct, work);
	struct mtk_apu *apu = apu_coredump_work->apu;
	struct device *dev = apu->dev;
	uint32_t ret =0;

	dev_info(dev, "%s +\n", __func__);

	apu_regdump();//dump CE reg 0x190B0400 to 0x190B09b4

    //return CE job id
	ret = apusys_rv_smc_call(dev,
				MTK_APUSYS_KERNEL_OP_APUSYS_CE_RESET, 0);

	apusys_ce_exception_aee_warn(job_id_mapping(ret));

	apusys_rv_smc_call(dev,
				MTK_APUSYS_KERNEL_OP_APUSYS_CE_MASK_INIT, 0);

}

static irqreturn_t apu_ce_isr(int irq, void *private_data)
{
	struct mtk_apu *apu = (struct mtk_apu *) private_data;
	struct device *dev = apu->dev;

    uint32_t ret =0;

    //add smc call to mask on CE exception
	ret = apusys_rv_smc_call(dev, MTK_APUSYS_KERNEL_OP_APUSYS_CE_MASKON_EXP, 0);

	dev_info(dev, "%s +\n", __func__);

	schedule_work(&(apu_ce_coredump_work.work));

	return IRQ_HANDLED;
}

static int apu_ce_irq_register(struct platform_device *pdev,
	struct mtk_apu *apu)
{
	struct device *dev = apu->dev;
	int ret = 0;

	apu->ce_exp_irq_number = platform_get_irq_byname(pdev, "ce_exp_irq");
	dev_info(dev, "%s: ce_exp_irq_number = %d\n", __func__, apu->ce_exp_irq_number);

	ret = devm_request_irq(&pdev->dev, apu->ce_exp_irq_number, apu_ce_isr,
			irq_get_trigger_type(apu->ce_exp_irq_number),
			"apusys_ce_excep", apu);
	if (ret < 0)
		dev_info(dev, "%s: devm_request_irq Failed to request irq %d: %d\n",
				__func__, apu->ce_exp_irq_number, ret);

	return ret;
}

int apu_ce_excep_init(struct platform_device *pdev, struct mtk_apu *apu)
{
	int ret = 0;

	struct device *dev = apu->dev;
	apusys_rv_smc_call(dev,
				MTK_APUSYS_KERNEL_OP_APUSYS_CE_MASK_INIT, 0);

	INIT_WORK(&(apu_ce_coredump_work.work), &apu_ce_coredump_work_func);
	apu_ce_coredump_work.apu = apu;
	ret = apu_ce_irq_register(pdev, apu);
	if (ret < 0)
		return ret;

	return ret;
}

void apu_ce_excep_remove(struct platform_device *pdev, struct mtk_apu *apu)
{
	struct device *dev = apu->dev;
	disable_irq(apu->ce_exp_irq_number);
	dev_info(dev, "%s: disable ce_exp_irq (%d)\n", __func__,
		apu->ce_exp_irq_number);

	cancel_work_sync(&(apu_ce_coredump_work.work));
}
