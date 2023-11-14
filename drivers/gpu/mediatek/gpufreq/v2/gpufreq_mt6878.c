// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2022 MediaTek Inc.
 */

/**
 * @file    mtk_gpufreq_core.c
 * @brief   GPU-DVFS Driver Platform Implementation
 */

/**
 * ===============================================
 * Include
 * ===============================================
 */
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/ioport.h>
#include <linux/err.h>
#include <linux/regulator/consumer.h>
#include <linux/clk.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/printk.h>
#include <linux/pm_runtime.h>
#include <linux/pm_domain.h>
#include <linux/timekeeping.h>

#include <gpufreq_v2.h>
#include <gpufreq_mssv.h>
#include <gpuppm.h>
#include <gpufreq_common.h>
#include <gpufreq_mt6878.h>
#include <gpufreq_reg_mt6878.h>
#include <mtk_gpu_utility.h>

/**
 * ===============================================
 * Local Function Declaration
 * ===============================================
 */
/* get function */
static unsigned int __gpufreq_get_pll_fgpu(void);
static unsigned int __gpufreq_get_pll_fstack(void);
static unsigned int __gpufreq_get_fmeter_main_fgpu(void);
static unsigned int __gpufreq_get_fmeter_main_fstack(void);
/* misc function */
static void __iomem *__gpufreq_of_ioremap(const char *node_name, int idx);
/* bringup function */
static unsigned int __gpufreq_bringup(void);
static void __gpufreq_dump_bringup_status(struct platform_device *pdev);
/* init function */
static int __gpufreq_init_pmic(struct platform_device *pdev);
static int __gpufreq_init_platform_info(struct platform_device *pdev);
static int __gpufreq_pdrv_probe(struct platform_device *pdev);
static int __gpufreq_pdrv_remove(struct platform_device *pdev);

/**
 * ===============================================
 * Local Variable Definition
 * ===============================================
 */
static const struct of_device_id g_gpufreq_of_match[] = {
	{ .compatible = "mediatek,gpufreq" },
	{ /* sentinel */ }
};
static struct platform_driver g_gpufreq_pdrv = {
	.probe = __gpufreq_pdrv_probe,
	.remove = __gpufreq_pdrv_remove,
	.driver = {
		.name = "gpufreq",
		.owner = THIS_MODULE,
		.of_match_table = g_gpufreq_of_match,
	},
};

static void __iomem *g_mfg_top_base;
static void __iomem *g_mfg_pll_base;
static void __iomem *g_mfgsc_pll_base;
static void __iomem *g_mfg_rpc_base;
static void __iomem *g_sleep;
static void __iomem *g_topckgen_base;
static void __iomem *g_nth_emicfg_base;
static void __iomem *g_nth_emicfg_ao_mem_base;
static void __iomem *g_infracfg_ao_base;
static void __iomem *g_infra_ao_debug_ctrl;
static void __iomem *g_infra_ao1_debug_ctrl;
static void __iomem *g_nth_emi_ao_debug_ctrl;
static void __iomem *g_mali_base;
static void __iomem *g_nemi_mi32_smi_sub;
static void __iomem *g_nemi_mi33_smi_sub;
static struct gpufreq_pmic_info *g_pmic;

static unsigned int g_gpueb_support;
static unsigned int g_gpufreq_ready;

static struct gpufreq_platform_fp platform_eb_fp = {
	.dump_infra_status = __gpufreq_dump_infra_status,
	.dump_power_tracker_status = __gpufreq_dump_power_tracker_status,
	.get_dyn_pgpu = __gpufreq_get_dyn_pgpu,
	.get_core_mask_table = __gpufreq_get_core_mask_table,
	.get_core_num = __gpufreq_get_core_num,
};

/**
 * ===============================================
 * External Function Definition
 * ===============================================
 */
/* API: get dynamic Power of GPU */
unsigned int __gpufreq_get_dyn_pgpu(unsigned int freq, unsigned int volt)
{
	unsigned long long p_dynamic = GPU_DYN_REF_POWER;
	unsigned int ref_freq = GPU_DYN_REF_POWER_FREQ;
	unsigned int ref_volt = GPU_DYN_REF_POWER_VOLT;

	p_dynamic = p_dynamic *
		((freq * 100) / ref_freq) *
		((volt * 100) / ref_volt) *
		((volt * 100) / ref_volt) /
		(100 * 100 * 100);

	return (unsigned int)p_dynamic;
}

void __gpufreq_dump_infra_status(char *log_buf, int *log_len, int log_size)
{
	if (!g_gpufreq_ready)
		return;

	GPUFREQ_LOGB(log_buf, log_len, log_size,
		"== [GPUFREQ INFRA STATUS] ==");
	GPUFREQ_LOGB(log_buf, log_len, log_size,
		"[Clk] MFG_PLL: %d, MFG_SEL: 0x%lx, MFGSC_PLL: %d, MFGSC_SEL: 0x%lx",
		__gpufreq_get_pll_fgpu(), DRV_Reg32(TOPCK_CLK_CFG_20) & MFG_SEL_MFGPLL_MASK,
		__gpufreq_get_pll_fstack(), DRV_Reg32(TOPCK_CLK_CFG_20) & MFGSC_SEL_MFGSCPLL_MASK);

	/* MFG_QCHANNEL_CON 0x13FBF0B4 [0] MFG_ACTIVE_SEL = 1'b1 */
	DRV_WriteReg32(MFG_QCHANNEL_CON, (DRV_Reg32(MFG_QCHANNEL_CON) | BIT(0)));
	/* MFG_DEBUG_SEL 0x13FBF170 [1:0] MFG_DEBUG_TOP_SEL = 2'b11 */
	DRV_WriteReg32(MFG_DEBUG_SEL, (DRV_Reg32(MFG_DEBUG_SEL) | GENMASK(1, 0)));

	/* MFG_DEBUG_SEL 0x13FBF170 */
	/* MFG_DEBUG_TOP 0x13FBF178 */
	GPUFREQ_LOGB(log_buf, log_len, log_size,
		"%-11s (0x%x): 0x%08x, (0x%x): 0x%08x",
		"[MFG]",
		0x13FBF170, DRV_Reg32(MFG_DEBUG_SEL),
		0x13FBF178, DRV_Reg32(MFG_DEBUG_TOP));

	/* MFG_RPC_SLP_PROT_EN_SET 0x13F91040 */
	/* MFG_RPC_SLP_PROT_EN_CLR 0x13F91044 */
	/* MFG_RPC_SLP_PROT_EN_STA 0x13F91048 */
	GPUFREQ_LOGB(log_buf, log_len, log_size,
		"%-11s (0x%x): 0x%08x, (0x%x): 0x%08x, (0x%x): 0x%08x",
		"[MFG]",
		0x13F91040, DRV_Reg32(MFG_RPC_SLP_PROT_EN_SET),
		0x13F91044, DRV_Reg32(MFG_RPC_SLP_PROT_EN_CLR),
		0x13F91048, DRV_Reg32(MFG_RPC_SLP_PROT_EN_STA));

	/* MFG_RPC_AO_CLK_CFG 0x13F91034 */
	/* MFG_RPC_IPS_SES_PWR_CON 0x13F91300 */
	GPUFREQ_LOGB(log_buf, log_len, log_size,
		"%-11s (0x%x): 0x%08x, (0x%x): 0x%08x",
		"[MFG]",
		0x13F91034, DRV_Reg32(MFG_RPC_AO_CLK_CFG),
		0x13F91300, DRV_Reg32(MFG_RPC_IPS_SES_PWR_CON));

	/* NTH_MFG_EMI1_GALS_SLV_DBG 0x1021C82C */
	/* NTH_MFG_EMI0_GALS_SLV_DBG 0x1021C830 */
	GPUFREQ_LOGB(log_buf, log_len, log_size,
		"%-11s (0x%x): 0x%08x, (0x%x): 0x%08x",
		"[EMI]",
		0x1021C82C, DRV_Reg32(NTH_MFG_EMI1_GALS_SLV_DBG),
		0x1021C830, DRV_Reg32(NTH_MFG_EMI0_GALS_SLV_DBG));

	/* NTH_APU_EMI1_GALS_SLV_DBG */
	/* NTH_APU_EMI0_GALS_SLV_DBG */
	GPUFREQ_LOGB(log_buf, log_len, log_size,
		"%-11s (0x%x): 0x%08x, (0x%x): 0x%08x",
		"[EMI]",
		0x1021C824, DRV_Reg32(NTH_APU_EMI1_GALS_SLV_DBG),
		0x1021C828, DRV_Reg32(NTH_APU_EMI0_GALS_SLV_DBG));

	/* NTH_M6M7_IDLE_BIT_EN_1 0x10270228 */
	/* NTH_M6M7_IDLE_BIT_EN_0 0x1027022C */
	/* SLEEP_PROT_MASK 0x10270000 */
	GPUFREQ_LOGB(log_buf, log_len, log_size,
		"%-11s (0x%x): 0x%08x, (0x%x): 0x%08x, (0x%x): 0x%08x",
		"[EMI]",
		0x10270228, DRV_Reg32(NTH_M6M7_IDLE_BIT_EN_1),
		0x1027022C, DRV_Reg32(NTH_M6M7_IDLE_BIT_EN_0),
		0x10270000, DRV_Reg32(NTH_SLEEP_PROT_MASK));

	/* MD_MFGSYS_PROTECT_EN_STA_0 0x10001CA0 */
	/* MD_MFGSYS_PROTECT_EN_SET_0 0x10001CA4 */
	/* MD_MFGSYS_PROTECT_EN_CLR_0 0x10001CA8 */
	/* MD_MFGSYS_PROTECT_RDY_STA_0 0x10001CAC */
	GPUFREQ_LOGB(log_buf, log_len, log_size,
		"%-11s (0x%x): 0x%08x, (0x%x): 0x%08x, (0x%x): 0x%08x, (0x%x): 0x%08x",
		"[EMI_SMI]",
		0x10001CA0, DRV_Reg32(MD_MFGSYS_PROTECT_EN_STA_0),
		0x10001CA4, DRV_Reg32(MD_MFGSYS_PROTECT_EN_SET_0),
		0x10001CA8, DRV_Reg32(MD_MFGSYS_PROTECT_EN_CLR_0),
		0x10001CAC, DRV_Reg32(MD_MFGSYS_PROTECT_RDY_STA_0));

	/* INFRA_AO_BUS0_U_DEBUG_CTRL0 0x10023000 */
	/* INFRA_AO1_BUS1_U_DEBUG_CTRL0 0x1002B000 */
	/* NTH_EMI_AO_DEBUG_CTRL0 0x10042000 */
	GPUFREQ_LOGB(log_buf, log_len, log_size,
		"%-11s (0x%x): 0x%08x, (0x%x): 0x%08x, (0x%x): 0x%08x",
		"[EMI]",
		0x10023000, DRV_Reg32(INFRA_AO_BUS0_U_DEBUG_CTRL0),
		0x1002B000, DRV_Reg32(INFRA_AO1_BUS1_U_DEBUG_CTRL0),
		0x10042000, DRV_Reg32(NTH_EMI_AO_DEBUG_CTRL0));

	/* NTH_SLEEP_PROT_MASK */
	/* NTH_GLITCH_PROT_RDY */
	GPUFREQ_LOGB(log_buf, log_len, log_size,
		"%-11s (0x%x): 0x%08x, (0x%x): 0x%08x",
		"[EMI]",
		0x10270000, DRV_Reg32(NTH_SLEEP_PROT_MASK),
		0x1027008C, DRV_Reg32(NTH_GLITCH_PROT_RDY));

	/* NEMI_MI32_SMI_SUB_DEBUG_S0 */
	/* NEMI_MI32_SMI_SUB_DEBUG_S1 */
	/* NEMI_MI32_SMI_SUB_DEBUG_S2 */
	/* NEMI_MI32_SMI_SUB_DEBUG_M0 */
	GPUFREQ_LOGB(log_buf, log_len, log_size,
		"%-11s (0x%x): 0x%08x, (0x%x): 0x%08x, (0x%x): 0x%08x, (0x%x): 0x%08x",
		"[EMI_SMI]",
		0x1025E400, DRV_Reg32(NEMI_MI32_SMI_SUB_DEBUG_S0),
		0x1025E404, DRV_Reg32(NEMI_MI32_SMI_SUB_DEBUG_S1),
		0x1025E408, DRV_Reg32(NEMI_MI32_SMI_SUB_DEBUG_S2),
		0x1025E430, DRV_Reg32(NEMI_MI32_SMI_SUB_DEBUG_M0));

	/* NEMI_MI33_SMI_SUB_DEBUG_S0 */
	/* NEMI_MI33_SMI_SUB_DEBUG_S1 */
	/* NEMI_MI33_SMI_SUB_DEBUG_M0 */
	GPUFREQ_LOGB(log_buf, log_len, log_size,
		"%-11s (0x%x): 0x%08x, (0x%x): 0x%08x, (0x%x): 0x%08x",
		"[EMI_SMI]",
		0x1025F400, DRV_Reg32(NEMI_MI33_SMI_SUB_DEBUG_S0),
		0x1025F404, DRV_Reg32(NEMI_MI33_SMI_SUB_DEBUG_S1),
		0x1025F430, DRV_Reg32(NEMI_MI33_SMI_SUB_DEBUG_M0));

	/* SPM_SRC_REQ */
	GPUFREQ_LOGB(log_buf, log_len, log_size,
		"%-11s (0x%x): 0x%08x",
		"[SPM]",
		0x1C001818, DRV_Reg32(SPM_SRC_REQ));

	GPUFREQ_LOGB(log_buf, log_len, log_size,
		"%-11s 0x%08x, 0x%08x",
		"[PWR_STATUS]",
		DRV_Reg32(SPM_XPU_PWR_STATUS), DRV_Reg32(SPM_XPU_PWR_STATUS_2ND));

	GPUFREQ_LOGB(log_buf, log_len, log_size,
		"%-11s 0x%08x, 0x%08x, 0x%08x, 0x%08x, 0x%08x",
		"[MFG0-3,5]", DRV_Reg32(SPM_MFG0_PWR_CON),
		DRV_Reg32(MFG_RPC_MFG1_PWR_CON), DRV_Reg32(MFG_RPC_MFG2_PWR_CON),
		DRV_Reg32(MFG_RPC_MFG3_PWR_CON), DRV_Reg32(MFG_RPC_MFG5_PWR_CON));
	GPUFREQ_LOGB(log_buf, log_len, log_size,
		"%-11s 0x%08x, 0x%08x",
		"[MFG9-10]",
		DRV_Reg32(MFG_RPC_MFG9_PWR_CON), DRV_Reg32(MFG_RPC_MFG10_PWR_CON));
}

void __gpufreq_dump_power_tracker_status(void)
{
	unsigned int val = 0;
	int i = 0;

	if (!g_gpufreq_ready)
		return;

	GPUFREQ_LOGI("== [PDC Power Tracker STATUS] ==");
	GPUFREQ_LOGI("Current Pointer: %d",
		(int)((DRV_Reg32(MFG_POWER_TRACKER_SETTING) >> 9) & GENMASK(4, 0)));

	for (i = 0; i < 32; i++) {
		val = DRV_Reg32(MFG_POWER_TRACKER_SETTING);
		val &= ~GENMASK(8, 4);
		val |= i << 4;
		DRV_WriteReg32(MFG_POWER_TRACKER_SETTING, val);
		udelay(1);

		GPUFREQ_LOGI("[SLOT %d] TIME_STAMP: 0x%08x, STATUS1: 0x%08x",
			i, DRV_Reg32(MFG_POWER_TRACKER_PDC_STATUS0),
			DRV_Reg32(MFG_POWER_TRACKER_PDC_STATUS1));
	}
}

/* API: get core_mask table */
struct gpufreq_core_mask_info *__gpufreq_get_core_mask_table(void)
{
	return g_core_mask_table;
}

/* API: get max number of shader cores */
unsigned int __gpufreq_get_core_num(void)
{
	return SHADER_CORE_NUM;
}

/* API: get working OPP index of STACK via Freq */
int __gpufreq_get_idx_by_fstack(unsigned int freq)
{
	GPUFREQ_UNREFERENCED(freq);

	return -1;
}

/* API: get working OPP index of STACK via Power */
int __gpufreq_get_idx_by_pstack(unsigned int power)
{
	GPUFREQ_UNREFERENCED(power);

	return -1;
}

/* API: get working OPP index of GPU via Freq */
int __gpufreq_get_idx_by_fgpu(unsigned int freq)
{
	GPUFREQ_UNREFERENCED(freq);

	return -1;
}

/* API: get working OPP index of GPU via Power */
int __gpufreq_get_idx_by_pgpu(unsigned int power)
{
	GPUFREQ_UNREFERENCED(power);

	return -1;
}

/* API: get working OPP index of GPU via Volt */
int __gpufreq_get_idx_by_vgpu(unsigned int volt)
{
	GPUFREQ_UNREFERENCED(volt);

	return -1;
}

/* API: get working OPP index of STACK via Volt */
int __gpufreq_get_idx_by_vstack(unsigned int volt)
{
	GPUFREQ_UNREFERENCED(volt);

	return -1;
}

/* API: get current working OPP index of GPU */
int __gpufreq_get_cur_idx_gpu(void)
{
	return -1;
}

/* API: get current working OPP index of STACK */
int __gpufreq_get_cur_idx_stack(void)
{
	return -1;
}

/*
 * API: commit DVFS to GPU by given OPP index
 * this is the main entrance of generic DVFS
 */
int __gpufreq_generic_commit_gpu(int target_oppidx, enum gpufreq_dvfs_state key)
{
	GPUFREQ_UNREFERENCED(target_oppidx);
	GPUFREQ_UNREFERENCED(key);

	return GPUFREQ_EINVAL;
}

/*
 * API: commit DVFS to STACK by given OPP index
 * this is the main entrance of generic DVFS
 */
int __gpufreq_generic_commit_stack(int target_oppidx, enum gpufreq_dvfs_state key)
{
	GPUFREQ_UNREFERENCED(target_oppidx);
	GPUFREQ_UNREFERENCED(key);

	return GPUFREQ_EINVAL;
}

int __gpufreq_generic_commit_dual(int target_oppidx_gpu, int target_oppidx_stack,
	enum gpufreq_dvfs_state key)
{
	GPUFREQ_UNREFERENCED(target_oppidx_gpu);
	GPUFREQ_UNREFERENCED(target_oppidx_stack);
	GPUFREQ_UNREFERENCED(key);

	return GPUFREQ_EINVAL;
}

/* API: set target oppidx */
void gpufreq_set_history_target_opp(enum gpufreq_target target, int oppidx)
{
	GPUFREQ_UNREFERENCED(target);
	GPUFREQ_UNREFERENCED(oppidx);
}

/* API: get number of working OPP of STACK */
int __gpufreq_get_opp_num_stack(void)
{
	return -1;
}

/* API: get number of working OPP of GPU */
int __gpufreq_get_opp_num_gpu(void)
{
	return -1;
}

/**
 * ===============================================
 * Internal Function Definition
 * ===============================================
 */
/*
 * API: get real current frequency from CON1 (khz)
 * Freq = ((PLL_CON1[21:0] * 26M) / 2^14) / 2^PLL_CON1[26:24]
 */
static unsigned int __gpufreq_get_pll_fgpu(void)
{
	u32 con1 = 0;
	unsigned int posdiv = 0;
	unsigned long long freq = 0, pcw = 0;

	con1 = DRV_Reg32(MFG_PLL_CON1);

	pcw = con1 & GENMASK(21, 0);

	posdiv = (con1 & GENMASK(26, 24)) >> POSDIV_SHIFT;

	freq = (((pcw * 1000) * MFGPLL_FIN) >> DDS_SHIFT) / (1 << posdiv);

	return FREQ_ROUNDUP_TO_10((unsigned int)freq);
}

/*
 * API: get real current frequency from CON1 (khz)
 * Freq = ((PLL_CON1[21:0] * 26M) / 2^14) / 2^PLL_CON1[26:24]
 */
static unsigned int __gpufreq_get_pll_fstack(void)
{
	u32 con1 = 0;
	unsigned int posdiv = 0;
	unsigned long long freq = 0, pcw = 0;

	con1 = DRV_Reg32(MFGSC_PLL_CON1);

	pcw = con1 & GENMASK(21, 0);

	posdiv = (con1 & GENMASK(26, 24)) >> POSDIV_SHIFT;

	freq = (((pcw * 1000) * MFGPLL_FIN) >> DDS_SHIFT) / (1 << posdiv);

	return FREQ_ROUNDUP_TO_10((unsigned int)freq);
}

static unsigned int __gpufreq_get_fmeter_main_fgpu(void)
{
	u32 val = 0, ckgen_load_cnt = 0, ckgen_k1 = 0;
	int i = 0;
	unsigned int freq = 0;

	/* Enable clock PLL_TST_CK */
	val = DRV_Reg32(MFG_PLL_CON0);
	DRV_WriteReg32(MFG_PLL_CON0, (val | BIT(12)));

	DRV_WriteReg32(MFG_PLL_FQMTR_CON1, GENMASK(23, 16));
	val = DRV_Reg32(MFG_PLL_FQMTR_CON0);
	DRV_WriteReg32(MFG_PLL_FQMTR_CON0, (val & GENMASK(23, 0)));
	/* Enable fmeter & select measure clock PLL_TST_CK */
	DRV_WriteReg32(MFG_PLL_FQMTR_CON0, (BIT(12) | BIT(15)));

	ckgen_load_cnt = DRV_Reg32(MFG_PLL_FQMTR_CON1) >> 16;
	ckgen_k1 = DRV_Reg32(MFG_PLL_FQMTR_CON0) >> 24;

	val = DRV_Reg32(MFG_PLL_FQMTR_CON0);
	DRV_WriteReg32(MFG_PLL_FQMTR_CON0, (val | BIT(4) | BIT(12)));

	/* wait fmeter finish */
	while (DRV_Reg32(MFG_PLL_FQMTR_CON0) & BIT(4)) {
		udelay(10);
		i++;
		if (i > 1000) {
			GPUFREQ_LOGE("wait MFGPLL Fmeter timeout");
			break;
		}
	}

	val = DRV_Reg32(MFG_PLL_FQMTR_CON1) & GENMASK(15, 0);
	/* KHz */
	freq = (val * 26000 * (ckgen_k1 + 1)) / (ckgen_load_cnt + 1);

	return freq;
}

static unsigned int __gpufreq_get_fmeter_main_fstack(void)
{
	u32 val = 0, ckgen_load_cnt = 0, ckgen_k1 = 0;
	int i = 0;
	unsigned int freq = 0;

	/* Enable clock PLL_TST_CK */
	val = DRV_Reg32(MFGSC_PLL_CON0);
	DRV_WriteReg32(MFGSC_PLL_CON0, (val | BIT(12)));

	DRV_WriteReg32(MFGSC_PLL_FQMTR_CON1, GENMASK(23, 16));
	val = DRV_Reg32(MFGSC_PLL_FQMTR_CON0);
	DRV_WriteReg32(MFGSC_PLL_FQMTR_CON0, (val & GENMASK(23, 0)));
	/* Enable fmeter & select measure clock PLL_TST_CK */
	DRV_WriteReg32(MFGSC_PLL_FQMTR_CON0, (BIT(12) | BIT(15)));

	ckgen_load_cnt = DRV_Reg32(MFGSC_PLL_FQMTR_CON1) >> 16;
	ckgen_k1 = DRV_Reg32(MFGSC_PLL_FQMTR_CON0) >> 24;

	val = DRV_Reg32(MFGSC_PLL_FQMTR_CON0);
	DRV_WriteReg32(MFGSC_PLL_FQMTR_CON0, (val | BIT(4) | BIT(12)));

	/* wait fmeter finish */
	while (DRV_Reg32(MFGSC_PLL_FQMTR_CON0) & BIT(4)) {
		udelay(10);
		i++;
		if (i > 1000) {
			GPUFREQ_LOGE("wait MFGSCPLL Fmeter timeout");
			break;
		}
	}

	val = DRV_Reg32(MFGSC_PLL_FQMTR_CON1) & GENMASK(15, 0);
	/* KHz */
	freq = (val * 26000 * (ckgen_k1 + 1)) / (ckgen_load_cnt + 1);

	return freq;
}

static void __gpufreq_dump_bringup_status(struct platform_device *pdev)
{
	struct device *gpufreq_dev = &pdev->dev;
	struct resource *res = NULL;

	if (unlikely(!gpufreq_dev)) {
		GPUFREQ_LOGE("fail to find gpufreq device (ENOENT)");
		return;
	}

	/* 0x13000000 */
	g_mali_base = __gpufreq_of_ioremap("mediatek,mali", 0);
	if (unlikely(!g_mali_base)) {
		GPUFREQ_LOGE("fail to ioremap MALI");
		return;
	}

	/* 0x13FBF000 */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "mfg_top_config");
	if (unlikely(!res)) {
		GPUFREQ_LOGE("fail to get resource MFG_TOP_CONFIG");
		return;
	}
	g_mfg_top_base = devm_ioremap(gpufreq_dev, res->start, resource_size(res));
	if (unlikely(!g_mfg_top_base)) {
		GPUFREQ_LOGE("fail to ioremap MFG_TOP_CONFIG: 0x%llx", res->start);
		return;
	}

	/* 0x13FA0000 */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "mfg_pll");
	if (unlikely(!res)) {
		GPUFREQ_LOGE("fail to get resource MFG_PLL");
		return;
	}
	g_mfg_pll_base = devm_ioremap(gpufreq_dev, res->start, resource_size(res));
	if (unlikely(!g_mfg_pll_base)) {
		GPUFREQ_LOGE("fail to ioremap MFG_PLL: 0x%llx", res->start);
		return;
	}

	/* 0x13FA0C00 */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "mfgsc_pll");
	if (unlikely(!res)) {
		GPUFREQ_LOGE("fail to get resource MFGSC_PLL");
		return;
	}
	g_mfgsc_pll_base = devm_ioremap(gpufreq_dev, res->start, resource_size(res));
	if (unlikely(!g_mfgsc_pll_base)) {
		GPUFREQ_LOGE("fail to ioremap MFGSC_PLL: 0x%llx", res->start);
		return;
	}

	/* 0x13F90000 */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "mfg_rpc");
	if (unlikely(!res)) {
		GPUFREQ_LOGE("fail to get resource MFG_RPC");
		return;
	}
	g_mfg_rpc_base = devm_ioremap(gpufreq_dev, res->start, resource_size(res));
	if (unlikely(!g_mfg_rpc_base)) {
		GPUFREQ_LOGE("fail to ioremap MFG_RPC: 0x%llx", res->start);
		return;
	}

	/* 0x1C001000 */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "sleep");
	if (unlikely(!res)) {
		GPUFREQ_LOGE("fail to get resource SLEEP");
		return;
	}
	g_sleep = devm_ioremap(gpufreq_dev, res->start, resource_size(res));
	if (unlikely(!g_sleep)) {
		GPUFREQ_LOGE("fail to ioremap SLEEP: 0x%llx", res->start);
		return;
	}

	/* 0x10000000 */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "topckgen");
	if (unlikely(!res)) {
		GPUFREQ_LOGE("fail to get resource TOPCKGEN");
		return;
	}
	g_topckgen_base = devm_ioremap(gpufreq_dev, res->start, resource_size(res));
	if (unlikely(!g_topckgen_base)) {
		GPUFREQ_LOGE("fail to ioremap TOPCKGEN: 0x%llx", res->start);
		return;
	}

	GPUFREQ_LOGI("[SPM] SPM_SPM2GPUPM_CON: 0x%08x", DRV_Reg32(SPM_SPM2GPUPM_CON));

	GPUFREQ_LOGI("[RPC] MFG_0_10_PWR_STATUS: 0x%08lx, MFG_RPC_MFG1_PWR_CON: 0x%08x",
		MFG_0_10_PWR_STATUS,
		DRV_Reg32(MFG_RPC_MFG1_PWR_CON));

	GPUFREQ_LOGI("[TOP] CON0: 0x%08x, CON1: %d, FMETER: %d, SEL: 0x%08lx, REF_SEL: 0x%08lx",
		DRV_Reg32(MFG_PLL_CON0),
		__gpufreq_get_pll_fgpu(),
		__gpufreq_get_fmeter_main_fgpu(),
		DRV_Reg32(TOPCK_CLK_CFG_20) & MFG_SEL_MFGPLL_MASK,
		DRV_Reg32(TOPCK_CLK_CFG_18) & MFG_REF_SEL_MASK);

	GPUFREQ_LOGI("[STK] CON0: 0x%08x, CON1: %d, FMETER: %d, SEL: 0x%08lx, REF_SEL: 0x%08lx",
		DRV_Reg32(MFGSC_PLL_CON0),
		__gpufreq_get_pll_fstack(),
		__gpufreq_get_fmeter_main_fstack(),
		DRV_Reg32(TOPCK_CLK_CFG_20) & MFGSC_SEL_MFGSCPLL_MASK,
		DRV_Reg32(TOPCK_CLK_CFG_18) & MFGSC_REF_SEL_MASK);

	GPUFREQ_LOGI("[GPU] MALI_GPU_ID: 0x%08x", DRV_Reg32(MALI_GPU_ID));
}

static void __iomem *__gpufreq_of_ioremap(const char *node_name, int idx)
{
	struct device_node *node;
	void __iomem *base;

	node = of_find_compatible_node(NULL, NULL, node_name);

	if (node)
		base = of_iomap(node, idx);
	else
		base = NULL;

	return base;
}

static int __gpufreq_init_pmic(struct platform_device *pdev)
{
	int ret = GPUFREQ_SUCCESS;

	GPUFREQ_TRACE_START("pdev=0x%lx", (unsigned long)pdev);

	g_pmic = kzalloc(sizeof(struct gpufreq_pmic_info), GFP_KERNEL);
	if (!g_pmic) {
		__gpufreq_abort("fail to alloc g_pmic (%dB)",
			sizeof(struct gpufreq_pmic_info));
		ret = GPUFREQ_ENOMEM;
		goto done;
	}

	g_pmic->reg_vgpu = regulator_get_optional(&pdev->dev, "vgpu");
	if (IS_ERR(g_pmic->reg_vgpu)) {
		ret = PTR_ERR(g_pmic->reg_vgpu);
		__gpufreq_abort("fail to get VGPU (%ld)", ret);
		goto done;
	}

	g_pmic->reg_vsram = regulator_get_optional(&pdev->dev, "vsram");
	if (IS_ERR(g_pmic->reg_vsram)) {
		ret = PTR_ERR(g_pmic->reg_vsram);
		__gpufreq_abort("fail to get VSRAM (%ld)", ret);
		goto done;
	}

done:
	GPUFREQ_TRACE_END();

	return ret;
}

/* API: init reg base address and flavor config of the platform */
static int __gpufreq_init_platform_info(struct platform_device *pdev)
{
	struct device *gpufreq_dev = &pdev->dev;
	struct device_node *of_wrapper = NULL;
	struct resource *res = NULL;
	int ret = GPUFREQ_ENOENT;

	if (unlikely(!gpufreq_dev)) {
		GPUFREQ_LOGE("fail to find gpufreq device (ENOENT)");
		goto done;
	}

	of_wrapper = of_find_compatible_node(NULL, NULL, "mediatek,gpufreq_wrapper");
	if (unlikely(!of_wrapper)) {
		GPUFREQ_LOGE("fail to find gpufreq_wrapper of_node");
		goto done;
	}

	/* ignore return error and use default value if property doesn't exist */
	of_property_read_u32(of_wrapper, "gpueb-support", &g_gpueb_support);

	/* 0x13000000 */
	g_mali_base = __gpufreq_of_ioremap("mediatek,mali", 0);
	if (unlikely(!g_mali_base)) {
		GPUFREQ_LOGE("fail to ioremap MALI");
		goto done;
	}

	/* 0x13FBF000 */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "mfg_top_config");
	if (unlikely(!res)) {
		GPUFREQ_LOGE("fail to get resource MFG_TOP_CONFIG");
		goto done;
	}
	g_mfg_top_base = devm_ioremap(gpufreq_dev, res->start, resource_size(res));
	if (unlikely(!g_mfg_top_base)) {
		GPUFREQ_LOGE("fail to ioremap MFG_TOP_CONFIG: 0x%llx", res->start);
		goto done;
	}

	/* 0x13FA0000 */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "mfg_pll");
	if (unlikely(!res)) {
		GPUFREQ_LOGE("fail to get resource MFG_PLL");
		goto done;
	}
	g_mfg_pll_base = devm_ioremap(gpufreq_dev, res->start, resource_size(res));
	if (unlikely(!g_mfg_pll_base)) {
		GPUFREQ_LOGE("fail to ioremap MFG_PLL: 0x%llx", res->start);
		goto done;
	}

	/* 0x13FA0C00 */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "mfgsc_pll");
	if (unlikely(!res)) {
		GPUFREQ_LOGE("fail to get resource MFGSC_PLL");
		goto done;
	}
	g_mfgsc_pll_base = devm_ioremap(gpufreq_dev, res->start, resource_size(res));
	if (unlikely(!g_mfgsc_pll_base)) {
		GPUFREQ_LOGE("fail to ioremap MFGSC_PLL: 0x%llx", res->start);
		goto done;
	}

	/* 0x13F90000 */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "mfg_rpc");
	if (unlikely(!res)) {
		GPUFREQ_LOGE("fail to get resource MFG_RPC");
		goto done;
	}
	g_mfg_rpc_base = devm_ioremap(gpufreq_dev, res->start, resource_size(res));
	if (unlikely(!g_mfg_rpc_base)) {
		GPUFREQ_LOGE("fail to ioremap MFG_RPC: 0x%llx", res->start);
		goto done;
	}

	/* 0x1C001000 */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "sleep");
	if (unlikely(!res)) {
		GPUFREQ_LOGE("fail to get resource SLEEP");
		goto done;
	}
	g_sleep = devm_ioremap(gpufreq_dev, res->start, resource_size(res));
	if (unlikely(!g_sleep)) {
		GPUFREQ_LOGE("fail to ioremap SLEEP: 0x%llx", res->start);
		goto done;
	}

	/* 0x10000000 */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "topckgen");
	if (unlikely(!res)) {
		GPUFREQ_LOGE("fail to get resource TOPCKGEN");
		goto done;
	}
	g_topckgen_base = devm_ioremap(gpufreq_dev, res->start, resource_size(res));
	if (unlikely(!g_topckgen_base)) {
		GPUFREQ_LOGE("fail to ioremap TOPCKGEN: 0x%llx", res->start);
		goto done;
	}

	/* 0x1021C000 */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "nth_emicfg");
	if (unlikely(!res)) {
		GPUFREQ_LOGE("fail to get resource NTH_EMICFG");
		goto done;
	}
	g_nth_emicfg_base = devm_ioremap(gpufreq_dev, res->start, resource_size(res));
	if (unlikely(!g_nth_emicfg_base)) {
		GPUFREQ_LOGE("fail to ioremap NTH_EMICFG: 0x%llx", res->start);
		goto done;
	}

	/* 0x10270000 */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "nth_emicfg_ao_mem");
	if (unlikely(!res)) {
		GPUFREQ_LOGE("fail to get resource NTH_EMICFG_AO_MEM");
		goto done;
	}
	g_nth_emicfg_ao_mem_base = devm_ioremap(gpufreq_dev, res->start, resource_size(res));
	if (unlikely(!g_nth_emicfg_ao_mem_base)) {
		GPUFREQ_LOGE("fail to ioremap NTH_EMICFG_AO_MEM: 0x%llx", res->start);
		goto done;
	}

	/* 0x10001000 */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "ifrcfg_ao");
	if (unlikely(!res)) {
		GPUFREQ_LOGE("fail to get resource IFRBUS_AO");
		goto done;
	}
	g_infracfg_ao_base = devm_ioremap(gpufreq_dev, res->start, resource_size(res));
	if (unlikely(!g_infracfg_ao_base)) {
		GPUFREQ_LOGE("fail to ioremap IFRBUS_AO: 0x%llx", res->start);
		goto done;
	}

	/* 0x10023000 */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "infra_ao_debug_ctrl");
	if (unlikely(!res)) {
		GPUFREQ_LOGE("fail to get resource INFRA_AO_DEBUG_CTRL");
		goto done;
	}
	g_infra_ao_debug_ctrl = devm_ioremap(gpufreq_dev, res->start, resource_size(res));
	if (unlikely(!g_infra_ao_debug_ctrl)) {
		GPUFREQ_LOGE("fail to ioremap INFRA_AO_DEBUG_CTRL: 0x%llx", res->start);
		goto done;
	}

	/* 0x1002B000 */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "infra_ao1_debug_ctrl");
	if (unlikely(!res)) {
		GPUFREQ_LOGE("fail to get resource INFRA_AO1_DEBUG_CTRL");
		goto done;
	}
	g_infra_ao1_debug_ctrl = devm_ioremap(gpufreq_dev, res->start, resource_size(res));
	if (unlikely(!g_infra_ao1_debug_ctrl)) {
		GPUFREQ_LOGE("fail to ioremap INFRA_AO1_DEBUG_CTRL: 0x%llx", res->start);
		goto done;
	}

	/* 0x10042000 */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "nth_emi_ao_debug_ctrl");
	if (unlikely(!res)) {
		GPUFREQ_LOGE("fail to get resource NTH_EMI_AO_DEBUG_CTRL");
		goto done;
	}
	g_nth_emi_ao_debug_ctrl = devm_ioremap(gpufreq_dev, res->start, resource_size(res));
	if (unlikely(!g_nth_emi_ao_debug_ctrl)) {
		GPUFREQ_LOGE("fail to ioremap NTH_EMI_AO_DEBUG_CTRL: 0x%llx", res->start);
		goto done;
	}

	/* 0x1025E000 */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "nemi_mi32_smi_sub");
	if (unlikely(!res)) {
		GPUFREQ_LOGE("fail to get resource NEMI_MI32_SMI_SUB");
		goto done;
	}
	g_nemi_mi32_smi_sub = devm_ioremap(gpufreq_dev, res->start, resource_size(res));
	if (unlikely(!g_nemi_mi32_smi_sub)) {
		GPUFREQ_LOGE("fail to ioremap NEMI_MI32_SMI_SUB: 0x%llx", res->start);
		goto done;
	}

	/* 0x1025F000 */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "nemi_mi33_smi_sub");
	if (unlikely(!res)) {
		GPUFREQ_LOGE("fail to get resource NEMI_MI33_SMI_SUB");
		goto done;
	}
	g_nemi_mi33_smi_sub = devm_ioremap(gpufreq_dev, res->start, resource_size(res));
	if (unlikely(!g_nemi_mi33_smi_sub)) {
		GPUFREQ_LOGE("fail to ioremap NEMI_MI33_SMI_SUB: 0x%llx", res->start);
		goto done;
	}

	ret = GPUFREQ_SUCCESS;

done:
	return ret;
}

/* API: skip gpufreq driver probe if in bringup state */
static unsigned int __gpufreq_bringup(void)
{
	struct device_node *of_wrapper = NULL;
	unsigned int bringup_state = false;

	of_wrapper = of_find_compatible_node(NULL, NULL, "mediatek,gpufreq_wrapper");
	if (unlikely(!of_wrapper)) {
		GPUFREQ_LOGE("fail to find gpufreq_wrapper of_node, treat as bringup");
		return true;
	}

	/* check bringup state by dts */
	of_property_read_u32(of_wrapper, "gpufreq-bringup", &bringup_state);

	return bringup_state;
}

/* API: gpufreq driver probe */
static int __gpufreq_pdrv_probe(struct platform_device *pdev)
{
	int ret = GPUFREQ_SUCCESS;

	GPUFREQ_LOGI("start to probe gpufreq platform driver");

	/* keep probe successful but do nothing when bringup */
	if (__gpufreq_bringup()) {
		GPUFREQ_LOGI("skip gpufreq platform driver probe when bringup");
		__gpufreq_dump_bringup_status(pdev);
		goto done;
	}

	/* init footprint */
	__gpufreq_reset_footprint();

	/* init reg base address and flavor config of the platform in both AP and EB mode */
	ret = __gpufreq_init_platform_info(pdev);
	if (unlikely(ret)) {
		GPUFREQ_LOGE("fail to init platform info (%d)", ret);
		goto done;
	}

	/* init pmic regulator */
	ret = __gpufreq_init_pmic(pdev);
	if (unlikely(ret)) {
		GPUFREQ_LOGE("fail to init pmic (%d)", ret);
		goto done;
	}

	/*
	 * GPUFREQ PLATFORM INIT DONE
	 * register differnet platform fp to wrapper depending on AP or EB mode
	 */
	if (g_gpueb_support)
		gpufreq_register_gpufreq_fp(&platform_eb_fp);
	else
		GPUFREQ_LOGE("no support on AP mode");

	/* init gpu ppm */
	ret = gpuppm_init(TARGET_GPU, g_gpueb_support);
	if (unlikely(ret)) {
		GPUFREQ_LOGE("fail to init gpuppm (%d)", ret);
		goto done;
	}

	g_gpufreq_ready = true;
	GPUFREQ_LOGI("gpufreq platform driver probe done");

done:
	return ret;
}

/* API: gpufreq driver remove */
static int __gpufreq_pdrv_remove(struct platform_device *pdev)
{
	kfree(g_pmic);

	return GPUFREQ_SUCCESS;
}

/* API: register gpufreq platform driver */
static int __init __gpufreq_init(void)
{
	int ret = GPUFREQ_SUCCESS;

	GPUFREQ_LOGI("start to init gpufreq platform driver");

	/* register gpufreq platform driver */
	ret = platform_driver_register(&g_gpufreq_pdrv);
	if (unlikely(ret)) {
		GPUFREQ_LOGE("fail to register gpufreq platform driver (%d)", ret);
		goto done;
	}

	GPUFREQ_LOGI("gpufreq platform driver init done");

done:
	return ret;
}

/* API: unregister gpufreq driver */
static void __exit __gpufreq_exit(void)
{
	platform_driver_unregister(&g_gpufreq_pdrv);
}

module_init(__gpufreq_init);
module_exit(__gpufreq_exit);

MODULE_DEVICE_TABLE(of, g_gpufreq_of_match);
MODULE_DESCRIPTION("MediaTek GPU-DVFS platform driver");
MODULE_LICENSE("GPL");
