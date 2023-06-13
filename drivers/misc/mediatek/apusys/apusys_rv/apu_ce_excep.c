// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#include <linux/of_irq.h>
#include <linux/interrupt.h>
#include <linux/workqueue.h>
#include <linux/iommu.h>
#include <linux/delay.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/timer.h>

#include <mt-plat/aee.h>

#include "apu.h"
#include "apu_ce_excep.h"
#include "apu_config.h"
#include "apusys_secure.h"
#include "hw_logger.h"
#include "apu_regdump.h"

#define BYTE_WIDTH 8

enum apusys_assert_module {
	//job id
	assert_apusys_ce_TPPA_plus_BW_acc = 0,
	assert_apusys_ce_norm2lp,
	assert_apusys_ce_lp2norm,
	assert_apusys_ce_acx_mdla_mtcmos_off,
	assert_apusys_ce_rcx_mdla_mtcmos_off,
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
	"APUSYS_CE_ACX_MDLA_MTCMOS_OFF",
	"APUSYS_CE_RCX_MDLA_MTCMOS_OFF",
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

static struct delayed_work timeout_work;
static struct workqueue_struct *apu_workq;
static struct mtk_apu *ce_apu;

static struct apu_coredump_work_struct apu_ce_coredump_work;
#define APU_CE_DUMP_TIMEOUT_MS (1)

static void apu_ce_timer_dump_reg(struct work_struct *work);

static uint32_t apusys_rv_smc_call(struct device *dev, uint32_t smc_id,
	uint32_t param, uint32_t *ret0, uint32_t *ret1, uint32_t *ret2)
{
	struct arm_smccc_res res;

	dev_info(dev, "%s: smc call %d, param %d\n", __func__, smc_id, param);

	arm_smccc_smc(MTK_SIP_APUSYS_CONTROL, smc_id,
				param, 0, 0, 0, 0, 0, &res);

	if (res.a0 != 0) {
		dev_info(dev, "%s: smc call %d return error(%ld)\n",
			__func__, smc_id, res.a0);
	} else {
		if (ret0 != NULL)
			*ret0 = res.a1;
		if (ret1 != NULL)
			*ret1 = res.a2;
		if (ret2 != NULL)
			*ret2 = res.a3;

		dev_info(dev, "%s: smc call %d return (%ld %ld %ld %ld)\n",
			__func__, smc_id, res.a0, res.a1, res.a2, res.a3);
	}

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
	case 14:
		return apusys_ce_assert_module_name[assert_apusys_ce_acx_mdla_mtcmos_off];
	case 15:
		return apusys_ce_assert_module_name[assert_apusys_ce_rcx_mdla_mtcmos_off];
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
	uint32_t ret, job_id = 0;

	dev_info(dev, "%s +\n", __func__);

	apu_regdump();

	ret = apusys_rv_smc_call(dev, MTK_APUSYS_KERNEL_OP_APUSYS_CE_RESET,
		0, &job_id, NULL, NULL);

	if (ret == 0)
		apusys_ce_exception_aee_warn(job_id_mapping(job_id));
}

static irqreturn_t apu_ce_isr(int irq, void *private_data)
{
	struct mtk_apu *apu = (struct mtk_apu *) private_data;
	struct device *dev = apu->dev;
	struct mtk_apu_hw_ops *hw_ops = &apu->platdata->ops;
	uint32_t op, res0 = 0, res1 = 0, res2 = 0;

	if (!hw_ops->check_apu_exp_irq) {
		dev_info(dev, "%s ,not support handle apu exception\n", __func__);
		return -EINVAL;
	}

	if (hw_ops->check_apu_exp_irq(apu, "are_abnormal_irq")) {
		dev_info(dev, "%s ,are_abnormal_irq\n", __func__);

		op = (
			SMC_OP_APU_ACE_ABN_IRQ_FLAG_CE |
			SMC_OP_APU_ACE_ABN_IRQ_FLAG_ACE_SW << (BYTE_WIDTH * 1) |
			SMC_OP_APU_ACE_ABN_IRQ_FLAG_USER << (BYTE_WIDTH * 2)
		);
		if (apusys_rv_smc_call(
				dev, MTK_APUSYS_KERNEL_OP_APUSYS_CE_REGDUMP, op, &res0, &res1, &res2) == 0) {
			dev_info(dev, "APU_ACE_ABN_IRQ_FLAG_CE: 0x%08x\n", res0);
			dev_info(dev, "APU_ACE_ABN_IRQ_FLAG_ACE_SW: 0x%08x\n", res1);
			dev_info(dev, "APU_ACE_ABN_IRQ_FLAG_USER: 0x%08x\n", res2);
		}

		op = (
			SMC_OP_APU_ACE_CE0_TASK_ING |
			SMC_OP_APU_ACE_CE1_TASK_ING << (BYTE_WIDTH * 1) |
			SMC_OP_APU_ACE_CE2_TASK_ING << (BYTE_WIDTH * 2)
		);
		if (apusys_rv_smc_call(
				dev, MTK_APUSYS_KERNEL_OP_APUSYS_CE_REGDUMP, op, &res0, &res1, &res2) == 0) {
			dev_info(dev, "APU_ACE_CE0_TASK_ING: 0x%08x\n", res0);
			dev_info(dev, "APU_ACE_CE1_TASK_ING: 0x%08x\n", res1);
			dev_info(dev, "APU_ACE_CE2_TASK_ING: 0x%08x\n", res2);
		}

		op = (
			SMC_OP_APU_ACE_CE3_TASK_ING |
			SMC_OP_APU_CE0_RUN_INSTR << (BYTE_WIDTH * 1) |
			SMC_OP_APU_CE1_RUN_INSTR << (BYTE_WIDTH * 2)
		);
		if (apusys_rv_smc_call(
				dev, MTK_APUSYS_KERNEL_OP_APUSYS_CE_REGDUMP, op, &res0, &res1, &res2) == 0) {
			dev_info(dev, "APU_ACE_CE3_TASK_ING: 0x%08x\n", res0);
			dev_info(dev, "APU_CE0_RUN_INSTR: 0x%08x\n", res1);
			dev_info(dev, "APU_CE1_RUN_INSTR: 0x%08x\n", res2);
		}

		op = (
			SMC_OP_APU_CE2_RUN_INSTR |
			SMC_OP_APU_CE3_RUN_INSTR << (BYTE_WIDTH * 1) |
			SMC_OP_APU_CE0_TIMEOUT_INSTR << (BYTE_WIDTH * 2)
		);
		if (apusys_rv_smc_call(
				dev, MTK_APUSYS_KERNEL_OP_APUSYS_CE_REGDUMP, op, &res0, &res1, &res2) == 0) {
			dev_info(dev, "APU_CE2_RUN_INSTR: 0x%08x\n", res0);
			dev_info(dev, "APU_CE3_RUN_INSTR: 0x%08x\n", res1);
			dev_info(dev, "APU_CE0_TIMEOUT_INSTR: 0x%08x\n", res2);
		}

		op = (
			SMC_OP_APU_CE1_TIMEOUT_INSTR |
			SMC_OP_APU_CE2_TIMEOUT_INSTR << (BYTE_WIDTH * 1) |
			SMC_OP_APU_CE3_TIMEOUT_INSTR << (BYTE_WIDTH * 2)
		);
		if (apusys_rv_smc_call(
				dev, MTK_APUSYS_KERNEL_OP_APUSYS_CE_REGDUMP, op, &res0, &res1, &res2) == 0) {
			dev_info(dev, "APU_CE1_TIMEOUT_INSTR: 0x%08x\n", res0);
			dev_info(dev, "APU_CE2_TIMEOUT_INSTR: 0x%08x\n", res1);
			dev_info(dev, "APU_CE3_TIMEOUT_INSTR: 0x%08x\n", res2);
		}

		op = (
			SMC_OP_APU_ACE_APB_MST_OUT_STATUS_ERR |
			SMC_OP_APU_ACE_APB_MST_IN_STATUS_ERR << (BYTE_WIDTH * 1)
		);
		if (apusys_rv_smc_call(
				dev, MTK_APUSYS_KERNEL_OP_APUSYS_CE_REGDUMP, op, &res0, &res1, NULL) == 0) {
			dev_info(dev, "APU_ACE_APB_MST_OUT_STATUS_ERR: 0x%08x\n", res0);
			dev_info(dev, "APU_ACE_APB_MST_IN_STATUS_ERR: 0x%08x\n", res1);
		}

		schedule_work(&(apu_ce_coredump_work.work));
		disable_irq_nosync(apu->ce_exp_irq_number);
	}
	return IRQ_HANDLED;
}

void apu_ce_start_timer_dump_reg(void)
{
	/* init delay worker for power off detection */
	queue_delayed_work(apu_workq,
							&timeout_work,
							msecs_to_jiffies(APU_CE_DUMP_TIMEOUT_MS));
}

void apu_ce_stop_timer_dump_reg(void)
{
	cancel_delayed_work_sync(&timeout_work);
}

void apu_ce_timer_dump_reg_init(void)
{
	char wq_name[] = "apusys_ce_timer";

	/* init delay worker for power off detection */
	INIT_DELAYED_WORK(&timeout_work, apu_ce_timer_dump_reg);
	apu_workq = alloc_ordered_workqueue(wq_name, WQ_MEM_RECLAIM);
}

void apu_ce_timer_dump_reg(struct work_struct *work)
{
	if (ce_apu != NULL) {
		struct device *dev = ce_apu->dev;
		uint32_t ret =0;

		dev_info(dev, "%s +\n", __func__);

		ret = apusys_rv_smc_call(dev,
					MTK_APUSYS_KERNEL_OP_APUSYS_CE_DEBUG_REGDUMP, 0, NULL, NULL, NULL);
	}
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

	apusys_rv_smc_call(dev, MTK_APUSYS_KERNEL_OP_APUSYS_CE_MASK_INIT,
		0, NULL, NULL, NULL);

	INIT_WORK(&(apu_ce_coredump_work.work), &apu_ce_coredump_work_func);
	apu_ce_coredump_work.apu = apu;
	ret = apu_ce_irq_register(pdev, apu);
	if (ret < 0)
		return ret;
	ce_apu = apu;
	apu_ce_timer_dump_reg_init();

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
