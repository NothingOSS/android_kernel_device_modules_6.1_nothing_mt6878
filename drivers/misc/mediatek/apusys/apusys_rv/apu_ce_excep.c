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
	assert_apusys_ce_1 = 0,
	assert_apusys_ce_2,
	assert_apusys_ce_3,

	assert_ce_module_max,
};

static const char * const apusys_ce_assert_module_name[assert_ce_module_max] = {
	"APUSYS_CE_1",
	"APUSYS_CE_2",
	"APUSYS_CE_3",
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
	if (ret >= assert_ce_module_max) {
		dev_info(dev, "%s error, ret >= assert_ce_module_max\n", __func__);
		return;
	}

	apusys_ce_exception_aee_warn(apusys_ce_assert_module_name[ret]);

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
