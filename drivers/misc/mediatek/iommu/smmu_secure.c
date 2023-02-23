// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2023 MediaTek Inc.
 */

#define pr_fmt(fmt)    "mtk_smmu: sec " fmt

#include <linux/soc/mediatek/mtk_sip_svc.h>
#include <linux/arm-smccc.h>

#include "mtk-smmu-v3.h"
#include "smmu_secure.h"

enum smmu_atf_cmd {
	SMMU_SECURE_INIT,
	SMMU_SECURE_DEINIT,
	SMMU_SECURE_IRQ_EN,
	SMMU_SECURE_TF_DUMP,
	SMMU_CMD_NUM
};

static int mtk_smmu_hw_is_valid(uint32_t smmu_type)
{
	if (smmu_type >= SMMU_TYPE_NUM) {
		pr_info("%s type is invalid, %u\n", __func__, smmu_type);
		return SMC_SMMU_FAIL;
	}

	return SMC_SMMU_SUCCESS;
}

/*
 * a0/in0 = MTK_IOMMU_SECURE_CONTROL(IOMMU SMC ID)
 * a1/in1 = SMMU TF-A SMC cmd
 * a2/in2 = smmu_type (smmu id in TF-A)
 * a3/in3 ~ a7/in7: user defined
 */
static int mtk_smmu_atf_call(uint32_t smmu_type, unsigned long cmd,
			     unsigned long in2, unsigned long in3, unsigned long in4,
			     unsigned long in5, unsigned long in6, unsigned long in7)
{
	struct arm_smccc_res res;
	int ret;

	ret = mtk_smmu_hw_is_valid(smmu_type);
	if (ret) {
		pr_info("%s, SMMU HW type is invalid, type:%u\n", __func__, smmu_type);
		return SMC_SMMU_FAIL;
	}
	arm_smccc_smc(MTK_IOMMU_SECURE_CONTROL, cmd, smmu_type,
		      in3, in4, in5, in6, in7, &res);

	return res.a0;
}

int mtk_smmu_sec_init(u32 smmu_type)
{
	int ret;

	ret = mtk_smmu_atf_call(smmu_type, SMMU_SECURE_INIT, 0, 0, 0, 0, 0, 0);
	if (ret) {
		pr_info("%s, smc call fail, type:%u\n", __func__, smmu_type);
		return SMC_SMMU_FAIL;
	}

	return SMC_SMMU_SUCCESS;
}
EXPORT_SYMBOL_GPL(mtk_smmu_sec_init);

MODULE_DESCRIPTION("MediaTek SMMUv3 Secure Implement");
MODULE_LICENSE("GPL");
