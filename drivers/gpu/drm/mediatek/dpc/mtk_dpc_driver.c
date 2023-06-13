// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2023 MediaTek Inc.
 */

#include <linux/clk.h>
#include <linux/iopoll.h>
#include <linux/component.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/of_irq.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>
#if IS_ENABLED(CONFIG_DEBUG_FS)
#include <linux/debugfs.h>
#endif

#include <linux/soc/mediatek/mtk-cmdq-ext.h>
#include <soc/mediatek/mmdvfs_v3.h>
#include "mtk-mmdvfs-v3-memory.h"

#include "mtk_dpc.h"
#include "mtk_dpc_mmp.h"
#include "mtk_dpc_internal.h"

#include "mtk_disp_vidle.h"
#include "mtk-mml-dpc.h"
#include "mdp_dpc.h"

int debug_mmp = 1;
module_param(debug_mmp, int, 0644);
int debug_force_power = 1;
module_param(debug_force_power, int, 0644);
int debug_dvfs;
module_param(debug_dvfs, int, 0644);
int debug_check_reg;
module_param(debug_check_reg, int, 0644);
int debug_check_rtff;
module_param(debug_check_rtff, int, 0644);
int debug_check_event;
module_param(debug_check_event, int, 0644);
int debug_mtcmos_off;
module_param(debug_mtcmos_off, int, 0644);
int debug_irq_handler;
module_param(debug_irq_handler, int, 0644);

/* TODO: move to mtk_dpc_test.c */
#define SPM_REQ_STA_4 0x85C	/* D1: BIT30 APSRC_REQ, DDRSRC_REQ */
#define SPM_REQ_STA_5 0x860	/* D2: BIT0 EMI_REQ, D3: BIT4 MAINPLL_REQ, D4: MMINFRA_REQ */
#define SPM_MMINFRA_PWR_CON 0xEA8
#define SPM_DISP_VCORE_PWR_CON 0xE8C
#define SPM_PWR_ACK BIT(30)	/* mt_spm_reg.h */

#define DPC_DEBUG_RTFF_CNT 10
static void __iomem *debug_rtff[DPC_DEBUG_RTFF_CNT];

#define DPC_SYS_REGS_CNT 7
static const char *reg_names[DPC_SYS_REGS_CNT] = {
	"DPC_BASE",
	"VLP_BASE",
	"SPM_BASE",
	"hw_vote_status",
	"vdisp_dvsrc_debug_sta_7",
	"dvfsrc_en",
	"dvfsrc_debug_sta_1",
};
/* TODO: move to mtk_dpc_test.c */

static void __iomem *dpc_base;

struct mtk_dpc {
	struct platform_device *pdev;
	struct device *dev;
	int disp_irq;
	int mml_irq;
	resource_size_t dpc_pa;
	void __iomem *spm_base;
	void __iomem *vlp_base;
	void __iomem *mminfra_voter_check;
	void __iomem *mminfra_hfrp_pwr;
	void __iomem *vdisp_dvfsrc_check;
	void __iomem *vcore_dvfsrc_check;
	void __iomem *dvfsrc_en;
	struct cmdq_client *cmdq_client;
#if IS_ENABLED(CONFIG_DEBUG_FS)
	struct dentry *fs;
#endif
};
static struct mtk_dpc *g_priv;

static struct mtk_dpc_dt_usage mt6989_disp_dt_usage[DPC_DISP_DT_CNT] = {
	/* OVL0/OVL1/DISP0 */
	{0, DPC_SP_FRAME_DONE,	2500,	DPC_DISP_VIDLE_MTCMOS},		/* OFF Time 0 */
	{1, DPC_SP_TE,		15500,	DPC_DISP_VIDLE_MTCMOS},		/* ON Time */
	{2, DPC_SP_TE,		15000,	DPC_DISP_VIDLE_MTCMOS},		/* Pre-TE */
	{3, DPC_SP_TE,		500,	DPC_DISP_VIDLE_MTCMOS},		/* OFF Time 1 */

	/* DISP1 */
	{4, DPC_SP_FRAME_DONE,	2700,	DPC_DISP_VIDLE_MTCMOS_DISP1},
	{5, DPC_SP_TE,		15300,	DPC_DISP_VIDLE_MTCMOS_DISP1},
	{6, DPC_SP_TE,		15000,	DPC_DISP_VIDLE_MTCMOS_DISP1},	/* DISP1-TE */
	{7, DPC_SP_TE,		700,	DPC_DISP_VIDLE_MTCMOS_DISP1},

	/* VDISP DVFS, follow DISP1 by default, or HRT BW */
	{8, DPC_SP_FRAME_DONE,	2700,	DPC_DISP_VIDLE_VDISP_DVFS},
	{9, DPC_SP_TE,		15300,	DPC_DISP_VIDLE_VDISP_DVFS},
	{10, DPC_SP_TE,		700,	DPC_DISP_VIDLE_VDISP_DVFS},

	/* HRT BW */
	{11, DPC_SP_FRAME_DONE,	2500,	DPC_DISP_VIDLE_HRT_BW},		/* OFF Time 0 */
	{12, DPC_SP_TE,		15500,	DPC_DISP_VIDLE_HRT_BW},		/* ON Time */
	{13, DPC_SP_TE,		500,	DPC_DISP_VIDLE_HRT_BW},		/* OFF Time 1 */

	/* SRT BW, follow HRT BW by default, or follow DISP1 */
	{14, DPC_SP_FRAME_DONE,	2500,	DPC_DISP_VIDLE_SRT_BW},
	{15, DPC_SP_TE,		15500,	DPC_DISP_VIDLE_SRT_BW},
	{16, DPC_SP_TE,		500,	DPC_DISP_VIDLE_SRT_BW},

	/* MMINFRA OFF, follow DISP1 by default, or HRT BW */
	{17, DPC_SP_FRAME_DONE,	2700,	DPC_DISP_VIDLE_MMINFRA_OFF},
	{18, DPC_SP_TE,		15300,	DPC_DISP_VIDLE_MMINFRA_OFF},	/* Pre-TE - 300 */
	{19, DPC_SP_TE,		700,	DPC_DISP_VIDLE_MMINFRA_OFF},

	/* INFRA OFF, follow DISP1 by default, or HRT BW */
	{20, DPC_SP_FRAME_DONE,	2700,	DPC_DISP_VIDLE_INFRA_OFF},
	{21, DPC_SP_TE,		15300,	DPC_DISP_VIDLE_INFRA_OFF},
	{22, DPC_SP_TE,		700,	DPC_DISP_VIDLE_INFRA_OFF},

	/* MAINPLL OFF, follow DISP1 by default, or HRT BW */
	{23, DPC_SP_FRAME_DONE,	2700,	DPC_DISP_VIDLE_MAINPLL_OFF},
	{24, DPC_SP_TE,		15300,	DPC_DISP_VIDLE_MAINPLL_OFF},
	{25, DPC_SP_TE,		700,	DPC_DISP_VIDLE_MAINPLL_OFF},

	/* MSYNC 2.0 */
	{26, DPC_SP_TE,		40329,	DPC_DISP_VIDLE_MSYNC2_0},
	{27, DPC_SP_TE,		40329,	DPC_DISP_VIDLE_MSYNC2_0},
	{28, DPC_SP_TE,		40329,	DPC_DISP_VIDLE_MSYNC2_0},

	/* RESERVED */
	{29, DPC_SP_FRAME_DONE,	3000,	DPC_DISP_VIDLE_RESERVED},
	{30, DPC_SP_TE,		16000,	DPC_DISP_VIDLE_RESERVED},
	{31, DPC_SP_TE,		40329,	DPC_DISP_VIDLE_RESERVED},
};

static struct mtk_dpc_dt_usage mt6989_mml_dt_usage[DPC_MML_DT_CNT] = {
	/* MML1 */
	{32, DPC_SP_RROT_DONE,	2500,	DPC_MML_VIDLE_MTCMOS},		/* OFF Time 0 */
	{33, DPC_SP_TE,		15500,	DPC_MML_VIDLE_MTCMOS},		/* ON Time */
	{34, DPC_SP_TE,		15000,	DPC_MML_VIDLE_MTCMOS},		/* MML-TE */
	{35, DPC_SP_TE,		500,	DPC_MML_VIDLE_MTCMOS},		/* OFF Time 1 */

	/* VDISP DVFS, follow MML1 by default, or HRT BW */
	{36, DPC_SP_RROT_DONE,	2700,	DPC_MML_VIDLE_VDISP_DVFS},
	{37, DPC_SP_TE,		15300,	DPC_MML_VIDLE_VDISP_DVFS},
	{38, DPC_SP_TE,		700,	DPC_MML_VIDLE_VDISP_DVFS},

	/* HRT BW */
	{39, DPC_SP_RROT_DONE,	2500,	DPC_MML_VIDLE_HRT_BW},		/* OFF Time 0 */
	{40, DPC_SP_TE,		15500,	DPC_MML_VIDLE_HRT_BW},		/* ON Time */
	{41, DPC_SP_TE,		500,	DPC_MML_VIDLE_HRT_BW},		/* OFF Time 1 */

	/* SRT BW, follow HRT BW by default, or follow MML1 */
	{42, DPC_SP_RROT_DONE,	2500,	DPC_MML_VIDLE_SRT_BW},
	{43, DPC_SP_TE,		15500,	DPC_MML_VIDLE_SRT_BW},
	{44, DPC_SP_TE,		500,	DPC_MML_VIDLE_SRT_BW},

	/* MMINFRA OFF, follow MML1 by default, or HRT BW */
	{45, DPC_SP_RROT_DONE,	2700,	DPC_MML_VIDLE_MMINFRA_OFF},
	{46, DPC_SP_TE,		15300,	DPC_MML_VIDLE_MMINFRA_OFF},
	{47, DPC_SP_TE,		700,	DPC_MML_VIDLE_MMINFRA_OFF},

	/* INFRA OFF, follow MML1 by default, or HRT BW */
	{48, DPC_SP_RROT_DONE,	2700,	DPC_MML_VIDLE_INFRA_OFF},
	{49, DPC_SP_TE,		15300,	DPC_MML_VIDLE_INFRA_OFF},
	{50, DPC_SP_TE,		700,	DPC_MML_VIDLE_INFRA_OFF},

	/* MAINPLL OFF, follow MML1 by default, or HRT BW */
	{51, DPC_SP_RROT_DONE,	2700,	DPC_MML_VIDLE_MAINPLL_OFF},
	{52, DPC_SP_TE,		15300,	DPC_MML_VIDLE_MAINPLL_OFF},
	{53, DPC_SP_TE,		700,	DPC_MML_VIDLE_MAINPLL_OFF},

	/* RESERVED */
	{54, DPC_SP_RROT_DONE,	3000,	DPC_MML_VIDLE_RESERVED},
	{55, DPC_SP_TE,		16000,	DPC_MML_VIDLE_RESERVED},
	{56, DPC_SP_TE,		40329,	DPC_MML_VIDLE_RESERVED},
};

static void dpc_dt_enable(u16 dt, bool en)
{
	u32 value = 0;
	u32 addr = 0;

	if (dt < DPC_DISP_DT_CNT) {
		DPCFUNC("DISP dt(%u) en(%u)", dt, en);
		addr = DISP_REG_DPC_DISP_DT_EN;
	} else {
		DPCFUNC("MML dt(%u) en(%u)", dt, en);
		addr = DISP_REG_DPC_MML_DT_EN;
		dt -= DPC_DISP_DT_CNT;
	}

	value = readl(dpc_base + addr);
	if (en)
		writel(BIT(dt) | value, dpc_base + addr);
	else
		writel(~BIT(dt) & value, dpc_base + addr);
}

static void dpc_dt_set(u16 dt, u32 us)
{
	u32 value = us * 26;	/* 26M base, 20 bits range, 38.46 ns ~ 38.46 ms*/

	writel(value, dpc_base + DISP_REG_DPC_DTx_COUNTER(dt));
	DPCFUNC("dt(%u) counter(%u)us", dt, us);
}

static void dpc_dt_sw_trig(u16 dt)
{
	DPCFUNC("dt(%u)", dt);
	writel(1, dpc_base + DISP_REG_DPC_DTx_SW_TRIG(dt));
}

void dpc_ddr_force_enable(const enum mtk_dpc_subsys subsys, const bool en)
{
	u32 addr = 0;
	u32 value = en ? 0x000D000D : 0x00050005;

	if (subsys == DPC_SUBSYS_DISP)
		addr = DISP_REG_DPC_DISP_DDRSRC_EMIREQ_CFG;
	else if (subsys == DPC_SUBSYS_MML)
		addr = DISP_REG_DPC_MML_DDRSRC_EMIREQ_CFG;

	writel(value, dpc_base + addr);
}
EXPORT_SYMBOL(dpc_ddr_force_enable);

void dpc_infra_force_enable(const enum mtk_dpc_subsys subsys, const bool en)
{
	u32 addr = 0;
	u32 value = en ? 0x00181818 : 0x00080808;

	if (subsys == DPC_SUBSYS_DISP)
		addr = DISP_REG_DPC_DISP_INFRA_PLL_OFF_CFG;
	else if (subsys == DPC_SUBSYS_MML)
		addr = DISP_REG_DPC_MML_INFRA_PLL_OFF_CFG;

	writel(value, dpc_base + addr);
}
EXPORT_SYMBOL(dpc_infra_force_enable);

void dpc_dc_force_enable(const bool en)
{
	if (en) {
		writel(0x0, dpc_base + DISP_REG_DPC_MML_MASK_CFG);
		writel(0x00010001,
			dpc_base + DISP_REG_DPC_MML_DDRSRC_EMIREQ_CFG);
		writel(0x0, dpc_base + DISP_REG_DPC_MML_INFRA_PLL_OFF_CFG);
		writel(0xFFFFFFFF, dpc_base + DISP_REG_DPC_MML_DT_EN);
		writel(0x3, dpc_base + DISP_REG_DPC_MML_DT_SW_TRIG_EN);
		writel(0x1, dpc_base + DISP_REG_DPC_DTx_SW_TRIG(33));
	} else {
		writel(0x1, dpc_base + DISP_REG_DPC_DTx_SW_TRIG(32));
		writel(0x0, dpc_base + DISP_REG_DPC_MML_DT_SW_TRIG_EN);
	}
}
EXPORT_SYMBOL(dpc_dc_force_enable);

static void dpc_disp_group_enable(const enum mtk_dpc_disp_vidle group, bool en)
{
	int i;
	u32 value = 0;

	DPCFUNC("group(%u) en(%u)", group, en);

	switch (group) {
	case DPC_DISP_VIDLE_MTCMOS:
		/* MTCMOS auto_on_off enable, both ack */
		value = en ? (BIT(0) | BIT(4)) : 0;
		writel(value, dpc_base + DISP_REG_DPC_DISP0_MTCMOS_CFG);
		writel(value, dpc_base + DISP_REG_DPC_OVL0_MTCMOS_CFG);
		writel(value, dpc_base + DISP_REG_DPC_OVL1_MTCMOS_CFG);
		break;
	case DPC_DISP_VIDLE_MTCMOS_DISP1:
		/* MTCMOS auto_on_off enable, both ack */
		/* disable pwr off dependency for MML is not on */
		value = en ? (BIT(0) | BIT(4)) : 0;
		writel(value, dpc_base + DISP_REG_DPC_DISP1_MTCMOS_CFG);

		/* DDR_SRC and EMI_REQ DT is follow DISP1 */
		value = en ? 0x00010001 : 0x000D000D;
		writel(value, dpc_base + DISP_REG_DPC_DISP_DDRSRC_EMIREQ_CFG);
		break;
	case DPC_DISP_VIDLE_VDISP_DVFS:
		value = en ? 0 : 1;
		writel(value, dpc_base + DISP_REG_DPC_DISP_VDISP_DVFS_CFG);
		break;
	case DPC_DISP_VIDLE_HRT_BW:
	case DPC_DISP_VIDLE_SRT_BW:
		value = en ? 0 : 0x00010001;
		writel(value, dpc_base + DISP_REG_DPC_DISP_HRTBW_SRTBW_CFG);
		break;
	case DPC_DISP_VIDLE_MMINFRA_OFF:
	case DPC_DISP_VIDLE_INFRA_OFF:
	case DPC_DISP_VIDLE_MAINPLL_OFF:
		/* TODO: check SEL is 0b00 or 0b10 for ALL_PWR_ACK */
		value = en ? 0 : 0x181818;
		writel(value, dpc_base + DISP_REG_DPC_DISP_INFRA_PLL_OFF_CFG);
		break;
	default:
		break;
	}

	if (!en) {
		for (i = 0; i < DPC_DISP_DT_CNT; ++i) {
			if (group == mt6989_disp_dt_usage[i].group)
				dpc_dt_enable(mt6989_disp_dt_usage[i].index, false);
		}
		return;
	}
	for (i = 0; i < DPC_DISP_DT_CNT; ++i) {
		if (group == mt6989_disp_dt_usage[i].group) {
			dpc_dt_set(mt6989_disp_dt_usage[i].index, mt6989_disp_dt_usage[i].ep);
			dpc_dt_enable(mt6989_disp_dt_usage[i].index, true);
		}
	}
}

static void dpc_mml_group_enable(const enum mtk_dpc_mml_vidle group, bool en)
{
	int i;
	u32 value = 0;

	DPCFUNC("group(%u) en(%u)", group, en);

	switch (group) {
	case DPC_MML_VIDLE_MTCMOS:
		/* MTCMOS auto_on_off enable, both ack */
		value = en ? (BIT(0) | BIT(4)) : 0;
		writel(value, dpc_base + DISP_REG_DPC_MML1_MTCMOS_CFG);

		/* DDR_SRC and EMI_REQ DT is follow MML1 */
		value = en ? 0x00010001 : 0x000D000D;
		writel(value, dpc_base + DISP_REG_DPC_MML_DDRSRC_EMIREQ_CFG);

		/* DISP1 pwr off dependency */
		// value = readl(dpc_base + DISP_REG_DPC_DISP1_MTCMOS_CFG);
		// writel(BIT(6) | value, dpc_base + DISP_REG_DPC_DISP1_MTCMOS_CFG);
		break;
	case DPC_MML_VIDLE_VDISP_DVFS:
		value = en ? 0 : 1;
		writel(value, dpc_base + DISP_REG_DPC_MML_VDISP_DVFS_CFG);
		break;
	case DPC_MML_VIDLE_HRT_BW:
	case DPC_MML_VIDLE_SRT_BW:
		value = en ? 0 : 0x00010001;
		writel(value, dpc_base + DISP_REG_DPC_MML_HRTBW_SRTBW_CFG);
		break;
	case DPC_MML_VIDLE_MMINFRA_OFF:
	case DPC_MML_VIDLE_INFRA_OFF:
	case DPC_MML_VIDLE_MAINPLL_OFF:
		/* TODO: check SEL is 0b00 or 0b10 for ALL_PWR_ACK */
		value = en ? 0 : 0x181818;
		writel(value, dpc_base + DISP_REG_DPC_MML_INFRA_PLL_OFF_CFG);
		break;
	default:
		break;
	}

	if (!en) {
		for (i = 0; i < DPC_MML_DT_CNT; ++i) {
			if (group == mt6989_mml_dt_usage[i].group)
				dpc_dt_enable(mt6989_mml_dt_usage[i].index, false);
		}
		return;
	}
	for (i = 0; i < DPC_MML_DT_CNT; ++i) {
		if (group == mt6989_mml_dt_usage[i].group) {
			dpc_dt_set(mt6989_mml_dt_usage[i].index, mt6989_mml_dt_usage[i].ep);
			dpc_dt_enable(mt6989_mml_dt_usage[i].index, true);
		}
	}
}


void dpc_enable(bool en)
{
	if (en)
		writel(DISP_DPC_EN | DISP_DPC_DT_EN, dpc_base + DISP_REG_DPC_EN);
	else
		writel(0, dpc_base + DISP_REG_DPC_EN);

	/* enable gce event */
	writel(en, dpc_base + DISP_REG_DPC_EVENT_EN);

	DPCFUNC("en(%u)", en);
}
EXPORT_SYMBOL(dpc_enable);

void dpc_hrt_bw_set(const enum mtk_dpc_subsys subsys, const u32 bw_in_mb, bool force)
{
	u32 addr1 = 0, addr2 = 0;

	if (subsys == DPC_SUBSYS_DISP) {
		addr1 = DISP_REG_DPC_DISP_HIGH_HRT_BW;
		addr2 = DISP_REG_DPC_DISP_HRTBW_SRTBW_CFG;
	} else if (subsys == DPC_SUBSYS_MML) {
		addr1 = DISP_REG_DPC_MML_SW_HRT_BW;
		addr2 = DISP_REG_DPC_MML_HRTBW_SRTBW_CFG;
	}
	writel(bw_in_mb / 30 + 1, dpc_base + addr1); /* 30MB unit */
	writel(force ? 0x00010001 : 0, dpc_base + addr2);

	if (unlikely(debug_dvfs))
		DPCFUNC("subsys(%u) hrt bw(%u)MB force(%u)", subsys, bw_in_mb, force);
}
EXPORT_SYMBOL(dpc_hrt_bw_set);

void dpc_srt_bw_set(const enum mtk_dpc_subsys subsys, const u32 bw_in_mb, bool force)
{
	u32 addr1 = 0, addr2 = 0;

	if (subsys == DPC_SUBSYS_DISP) {
		addr1 = DISP_REG_DPC_DISP_SW_SRT_BW;
		addr2 = DISP_REG_DPC_DISP_HRTBW_SRTBW_CFG;
	} else if (subsys == DPC_SUBSYS_MML) {
		addr1 = DISP_REG_DPC_MML_SW_SRT_BW;
		addr2 = DISP_REG_DPC_MML_HRTBW_SRTBW_CFG;
	}
	writel(bw_in_mb / 100 + 1, dpc_base + addr1); /* 100MB unit */
	writel(force ? 0x00010001 : 0, dpc_base + addr2);

	if (unlikely(debug_dvfs))
		DPCFUNC("subsys(%u) srt bw(%u)MB force(%u)", subsys, bw_in_mb, force);
}
EXPORT_SYMBOL(dpc_srt_bw_set);

void dpc_dvfs_set(const enum mtk_dpc_subsys subsys, const u8 level, bool force)
{
	u32 addr1 = 0, addr2 = 0;

	if (subsys == DPC_SUBSYS_DISP) {
		addr1 = DISP_REG_DPC_DISP_VDISP_DVFS_VAL;
		addr2 = DISP_REG_DPC_DISP_VDISP_DVFS_CFG;
	} else if (subsys == DPC_SUBSYS_MML) {
		addr1 = DISP_REG_DPC_MML_VDISP_DVFS_VAL;
		addr2 = DISP_REG_DPC_MML_VDISP_DVFS_CFG;
	}

	/* support 575, 600, 650, 700, 750 mV */
	if (level > 4) {
		DPCERR("vdisp support only 5 levels");
		return;
	}
	writel(level, dpc_base + addr1);
	writel(force ? 1 : 0, dpc_base + addr2);

	if (unlikely(debug_dvfs))
		DPCFUNC("subsys(%u) vdisp level(%u) force(%u)", subsys, level, force);
}
EXPORT_SYMBOL(dpc_dvfs_set);

void dpc_group_enable(const u16 group, bool en)
{
	if (group <= DPC_DISP_VIDLE_RESERVED)
		dpc_disp_group_enable((enum mtk_dpc_disp_vidle)group, en);
	else if (group <= DPC_MML_VIDLE_RESERVED)
		dpc_mml_group_enable((enum mtk_dpc_mml_vidle)group, en);
	else
		DPCERR("group(%u) is not defined", group);
}
EXPORT_SYMBOL(dpc_group_enable);

void dpc_config(const enum mtk_dpc_subsys subsys, bool en)
{
	if (subsys == DPC_SUBSYS_DISP) {
		dpc_group_enable(DPC_DISP_VIDLE_MTCMOS, en);
		dpc_group_enable(DPC_DISP_VIDLE_MTCMOS_DISP1, en);
		dpc_group_enable(DPC_DISP_VIDLE_VDISP_DVFS, en);
		dpc_group_enable(DPC_DISP_VIDLE_HRT_BW, en);
		dpc_group_enable(DPC_DISP_VIDLE_MMINFRA_OFF, en);
		writel(en ? 0 : U32_MAX, dpc_base + DISP_REG_DPC_DISP_MASK_CFG);
		writel(0, dpc_base + DISP_REG_DPC_MERGE_DISP_INT_CFG);
		writel(0x1F, dpc_base + DISP_REG_DPC_DISP_EXT_INPUT_EN);
	} else if (subsys == DPC_SUBSYS_MML) {
		dpc_group_enable(DPC_MML_VIDLE_MTCMOS, en);
		dpc_group_enable(DPC_MML_VIDLE_VDISP_DVFS, en);
		dpc_group_enable(DPC_MML_VIDLE_HRT_BW, en);
		dpc_group_enable(DPC_MML_VIDLE_MMINFRA_OFF, en);
		writel(en ? 0 : U32_MAX, dpc_base + DISP_REG_DPC_MML_MASK_CFG);
		writel(0, dpc_base + DISP_REG_DPC_MERGE_MML_INT_CFG);
		writel(0x3, dpc_base + DISP_REG_DPC_MML_EXT_INPUT_EN);
	}

	/* wla ddren ack */
	writel(1, dpc_base + DISP_REG_DPC_DDREN_ACK_SEL);
}
EXPORT_SYMBOL(dpc_config);

void dpc_mtcmos_vote(const enum mtk_dpc_subsys subsys, const u8 thread, const bool en)
{
	u32 addr = 0;

	/* CLR : execute SW threads, disable auto MTCMOS */
	switch (subsys) {
	case DPC_SUBSYS_DISP0:
		addr = en ? DISP_REG_DPC_DISP0_THREADx_CLR(thread)
			  : DISP_REG_DPC_DISP0_THREADx_SET(thread);
		break;
	case DPC_SUBSYS_DISP1:
		addr = en ? DISP_REG_DPC_DISP1_THREADx_CLR(thread)
			  : DISP_REG_DPC_DISP1_THREADx_SET(thread);
		break;
	case DPC_SUBSYS_OVL0:
		addr = en ? DISP_REG_DPC_OVL0_THREADx_CLR(thread)
			  : DISP_REG_DPC_OVL0_THREADx_SET(thread);
		break;
	case DPC_SUBSYS_OVL1:
		addr = en ? DISP_REG_DPC_OVL1_THREADx_CLR(thread)
			  : DISP_REG_DPC_OVL1_THREADx_SET(thread);
		break;
	case DPC_SUBSYS_MML1:
		addr = en ? DISP_REG_DPC_MML1_THREADx_CLR(thread)
			  : DISP_REG_DPC_MML1_THREADx_SET(thread);
		break;
	default:
		break;
	}

	writel(1, dpc_base + addr);
	// DPCFUNC("subsys(%u:%u) addr(0x%x) vote %s", subsys, thread, addr, en ? "SET" : "CLR");
}
EXPORT_SYMBOL(dpc_mtcmos_vote);

irqreturn_t mtk_dpc_disp_irq_handler(int irq, void *dev_id)
{
	struct mtk_dpc *priv = dev_id;
	u32 status;

	if (!debug_irq_handler)
		return IRQ_NONE;

	if (IS_ERR_OR_NULL(priv))
		return IRQ_NONE;

	status = readl(dpc_base + DISP_REG_DPC_DISP_INTSTA);
	if (!status)
		return IRQ_NONE;

	writel(~status, dpc_base + DISP_REG_DPC_DISP_INTSTA);

	if (debug_mmp) {
		if (status & DISP_DPC_INT_DT2)
			dpc_mmp(prete, MMPROFILE_FLAG_PULSE, 0, 0);
		if (status & DISP_DPC_INT_DT3)
			dpc_mmp(disp_off, MMPROFILE_FLAG_PULSE, 0, 3);

		if (status & DISP_DPC_INT_MMINFRA_OFF_END)
			dpc_mmp(mminfra, MMPROFILE_FLAG_START, 0, 0);
		if (status & DISP_DPC_INT_MMINFRA_OFF_START)
			dpc_mmp(mminfra, MMPROFILE_FLAG_END, 0, 0);

		if (status & DISP_DPC_INT_OVL0_ON)
			dpc_mmp(mtcmos_ovl0, MMPROFILE_FLAG_START, 0, 0);
		if (status & DISP_DPC_INT_OVL0_OFF)
			dpc_mmp(mtcmos_ovl0, MMPROFILE_FLAG_END, 0, 0);

		if (status & DISP_DPC_INT_OVL1_ON)
			dpc_mmp(mtcmos_ovl1, MMPROFILE_FLAG_START, 0, 0);
		if (status & DISP_DPC_INT_OVL1_OFF)
			dpc_mmp(mtcmos_ovl1, MMPROFILE_FLAG_END, 0, 0);

		if (status & DISP_DPC_INT_DISP0_ON)
			dpc_mmp(mtcmos_disp0, MMPROFILE_FLAG_START, 0, 0);
		if (status & DISP_DPC_INT_DISP0_OFF)
			dpc_mmp(mtcmos_disp0, MMPROFILE_FLAG_END, 0, 0);

		if (status & DISP_DPC_INT_DISP1_ON)
			dpc_mmp(mtcmos_disp1, MMPROFILE_FLAG_START, 0, 0);
		if (status & DISP_DPC_INT_DISP1_OFF)
			dpc_mmp(mtcmos_disp1, MMPROFILE_FLAG_END, 0, 0);
	}

	if (unlikely(debug_check_reg)) {
		if (status & DISP_DPC_INT_DT29) {	/* should be the last off irq */
			DPCFUNC("\tOFF MMINFRA(%u) VDISP(%u) SRT&HRT(%#x) D1(%u) D234(%u)",
				readl(priv->mminfra_voter_check) & BIT(6) ? 1 : 0,
				(readl(priv->vdisp_dvfsrc_check) & 0x1C) >> 2,
				readl(priv->vcore_dvfsrc_check) & 0xFFFFF,
				(readl(priv->spm_base + SPM_REQ_STA_4) & BIT(30)) ? 1 : 0,
				((readl(priv->spm_base + SPM_REQ_STA_5) & 0x13) == 0x13) ? 1 : 0);
		}
		if (status & DISP_DPC_INT_DT30) {	/* should be the last on irq */
			DPCFUNC("\tON MMINFRA(%u) VDISP(%u) SRT&HRT(%#x) D1(%u) D234(%u)",
				readl(priv->mminfra_voter_check) & BIT(6) ? 1 : 0,
				(readl(priv->vdisp_dvfsrc_check) & 0x1C) >> 2,
				readl(priv->vcore_dvfsrc_check) & 0xFFFFF,
				(readl(priv->spm_base + SPM_REQ_STA_4) & BIT(30)) ? 1 : 0,
				((readl(priv->spm_base + SPM_REQ_STA_5) & 0x13) == 0x13) ? 1 : 0);
		}
	}

	if (unlikely(debug_check_rtff && (status & 0x600000))) {
		int i, sum = 0;

		for (i = 0; i < DPC_DEBUG_RTFF_CNT; i++)
			sum += readl(debug_rtff[i]);

		if (status & DISP_DPC_INT_DT29)			/* should be the last off irq */
			DPCFUNC("\tOFF rtff(%#x)", sum);
		if (status & DISP_DPC_INT_DT30)			/* should be the last on irq */
			DPCFUNC("\tON rtff(%#x)", sum);
	}

	if (unlikely(debug_check_event)) {
		if (status & DISP_DPC_INT_DT29) {	/* should be the last off irq */
			DPCFUNC("\tOFF event(%#06x)", readl(dpc_base + DISP_REG_DPC_DUMMY0));
			writel(0, dpc_base + DISP_REG_DPC_DUMMY0);
		}
		if (status & DISP_DPC_INT_DT30) {	/* should be the last on irq */
			DPCFUNC("\tON event(%#06x)", readl(dpc_base + DISP_REG_DPC_DUMMY0));
			writel(0, dpc_base + DISP_REG_DPC_DUMMY0);
		}
	}

	if (unlikely(debug_mtcmos_off && (status & 0xFF000000))) {
		if (status & DISP_DPC_INT_OVL0_OFF)
			DPCFUNC("OVL0 OFF");
		if (status & DISP_DPC_INT_OVL1_OFF)
			DPCFUNC("OVL1 OFF");
		if (status & DISP_DPC_INT_DISP0_OFF)
			DPCFUNC("DISP0 OFF");
		if (status & DISP_DPC_INT_DISP1_OFF)
			DPCFUNC("DISP1 OFF");
		if (status & DISP_DPC_INT_OVL0_ON)
			DPCFUNC("OVL0 ON");
		if (status & DISP_DPC_INT_OVL1_ON)
			DPCFUNC("OVL1 ON");
		if (status & DISP_DPC_INT_DISP0_ON)
			DPCFUNC("DISP0 ON");
		if (status & DISP_DPC_INT_DISP1_ON)
			DPCFUNC("DISP1 ON");
	}

	return IRQ_HANDLED;
}

irqreturn_t mtk_dpc_mml_irq_handler(int irq, void *dev_id)
{
	struct mtk_dpc *priv = dev_id;
	u32 status;

	if (!debug_irq_handler)
		return IRQ_NONE;

	if (IS_ERR_OR_NULL(priv))
		return IRQ_NONE;

	status = readl(dpc_base + DISP_REG_DPC_MML_INTSTA);
	if (!status)
		return IRQ_NONE;

	writel(~status, dpc_base + DISP_REG_DPC_MML_INTSTA);

	if (debug_mmp) {
		if (status & BIT(17))
			dpc_mmp(mml_rrot_done, MMPROFILE_FLAG_PULSE, 0, 0);

		if (status & BIT(13))
			dpc_mmp(mtcmos_mml1, MMPROFILE_FLAG_START, 0, 0);
		if (status & BIT(12))
			dpc_mmp(mtcmos_mml1, MMPROFILE_FLAG_END, 0, 0);
	}

	if (debug_mtcmos_off) {
		if (status & BIT(13))
			DPCFUNC("MML1 ON");
		if (status & BIT(12))
			DPCFUNC("MML1 OFF");
	}

	return IRQ_HANDLED;
}

static int dpc_res_init(struct mtk_dpc *priv)
{
	int i;
	int ret = 0;
	struct resource *res;
	static void __iomem *sys_va[DPC_SYS_REGS_CNT];

	for (i = 0; i < DPC_SYS_REGS_CNT; i++) {
		res = platform_get_resource_byname(priv->pdev, IORESOURCE_MEM, reg_names[i]);
		if (res == NULL) {
			DPCERR("miss reg in node");
			return ret;
		}
		sys_va[i] = devm_ioremap_resource(priv->dev, res);

		if (!priv->dpc_pa)
			priv->dpc_pa = res->start;
	}

	dpc_base = sys_va[0];
	priv->vlp_base = sys_va[1];
	priv->spm_base = sys_va[2];
	priv->mminfra_voter_check = sys_va[3];
	priv->vdisp_dvfsrc_check = sys_va[4];
	priv->vcore_dvfsrc_check = sys_va[5];
	priv->dvfsrc_en = sys_va[6];

	/* FIXME: can't request region for resource */
	if (IS_ERR_OR_NULL(priv->spm_base))
		priv->spm_base = ioremap(0x1c001000, 0x1000);
	if (IS_ERR_OR_NULL(priv->vdisp_dvfsrc_check))
		priv->vdisp_dvfsrc_check = ioremap(0x1ec352b8, 0x4);
	if (IS_ERR_OR_NULL(priv->vcore_dvfsrc_check))
		priv->vcore_dvfsrc_check = ioremap(0x1c00f2a0, 0x4);
	if (IS_ERR_OR_NULL(priv->dvfsrc_en))
		priv->dvfsrc_en = ioremap(0x1c00f000, 0x4);

	priv->mminfra_hfrp_pwr = ioremap(0x1ec3eea8, 0x4);

	return ret;
}

static int dpc_irq_init(struct mtk_dpc *priv)
{
	int ret = 0;
	int num_irqs;

	num_irqs = platform_irq_count(priv->pdev);
	if (num_irqs <= 0) {
		DPCERR("unable to count IRQs");
		return -EPROBE_DEFER;
	} else if (num_irqs == 1) {
		priv->disp_irq = platform_get_irq(priv->pdev, 0);
	} else if (num_irqs == 2) {
		priv->disp_irq = platform_get_irq(priv->pdev, 0);
		priv->mml_irq = platform_get_irq(priv->pdev, 1);
	}

	if (priv->disp_irq > 0) {
		ret = devm_request_irq(priv->dev, priv->disp_irq, mtk_dpc_disp_irq_handler,
				       IRQF_TRIGGER_NONE | IRQF_SHARED, dev_name(priv->dev), priv);
		if (ret)
			DPCERR("devm_request_irq %d fail: %d", priv->disp_irq, ret);
	}
	if (priv->mml_irq > 0) {
		ret = devm_request_irq(priv->dev, priv->mml_irq, mtk_dpc_mml_irq_handler,
				       IRQF_TRIGGER_NONE | IRQF_SHARED, dev_name(priv->dev), priv);
		if (ret)
			DPCERR("devm_request_irq %d fail: %d", priv->mml_irq, ret);
	}
	DPCFUNC("disp irq %d, mml irq %d, ret %d", priv->disp_irq, priv->mml_irq, ret);

	/* enable irq for DT0~7 DISP0 OVL0 OVL1 DISP1 */
	writel(0xFF0000FF, dpc_base + DISP_REG_DPC_DISP_INTEN);

	/* enable irq for MML1 RROT_DONE */
	writel(0x23000, dpc_base + DISP_REG_DPC_MML_INTEN);

	return ret;
}

static void dpc_debug_event(void)
{
	u16 event_ovl0_on, event_ovl0_off, event_disp1_on, event_disp1_off;
	struct cmdq_pkt *pkt;

	of_property_read_u16(g_priv->dev->of_node, "event-ovl0-off", &event_ovl0_off);
	of_property_read_u16(g_priv->dev->of_node, "event-ovl0-on", &event_ovl0_on);
	of_property_read_u16(g_priv->dev->of_node, "event-disp1-off", &event_disp1_off);
	of_property_read_u16(g_priv->dev->of_node, "event-disp1-on", &event_disp1_on);

	if (!event_ovl0_off || !event_ovl0_on || !event_disp1_off || !event_disp1_on) {
		DPCERR("read event fail");
		return;
	}

	g_priv->cmdq_client = cmdq_mbox_create(g_priv->dev, 0);
	if (!g_priv->cmdq_client) {
		DPCERR("cmdq_mbox_create fail");
		return;
	}

	cmdq_mbox_enable(g_priv->cmdq_client->chan);
	pkt = cmdq_pkt_create(g_priv->cmdq_client);
	if (!pkt) {
		DPCERR("cmdq_handle is NULL");
		return;
	}

	cmdq_pkt_wfe(pkt, event_ovl0_off);
	cmdq_pkt_write(pkt, NULL, g_priv->dpc_pa + DISP_REG_DPC_DUMMY0, 0x1000, 0x1000);
	cmdq_pkt_wfe(pkt, event_disp1_off);
	cmdq_pkt_write(pkt, NULL, g_priv->dpc_pa + DISP_REG_DPC_DUMMY0, 0x0100, 0x0100);

	/* DT29 done, off irq handler read and clear */

	cmdq_pkt_wfe(pkt, event_disp1_on);
	cmdq_pkt_write(pkt, NULL, g_priv->dpc_pa + DISP_REG_DPC_DUMMY0, 0x0010, 0x0010);
	cmdq_pkt_wfe(pkt, event_ovl0_on);
	cmdq_pkt_write(pkt, NULL, g_priv->dpc_pa + DISP_REG_DPC_DUMMY0, 0x0001, 0x0001);

	/* DT30 done, on irq handler read and clear */

	cmdq_pkt_finalize_loop(pkt);
	cmdq_pkt_flush_threaded(pkt, NULL, (void *)pkt);
}

static void mtk_disp_vlp_vote(unsigned int vote_set, unsigned int thread)
{
	if (vote_set) {
		/* any bit, need write twice */
		writel(1 << thread, g_priv->vlp_base + VLP_DISP_SW_VOTE_SET);
		writel(1 << thread, g_priv->vlp_base + VLP_DISP_SW_VOTE_SET);
	} else {
		/* any bit, need write twice */
		writel(1 << thread, g_priv->vlp_base + VLP_DISP_SW_VOTE_CLR);
		writel(1 << thread, g_priv->vlp_base + VLP_DISP_SW_VOTE_CLR);
	}
	dpc_mmp(vlp_vote, MMPROFILE_FLAG_PULSE, thread, vote_set);
}

#ifdef IF_ZERO
static int mtk_disp_wait_pwr_ack(const enum mtk_dpc_subsys subsys)
{
	unsigned int value;
	unsigned int addr = 0;
	int ret = 0;
	#define TIMEOUT_CNT 10000	/* ms */

	switch (subsys) {
	case DPC_SUBSYS_DISP0:
		addr = SPM_DIS0_PWR_CON;
		break;
	case DPC_SUBSYS_DISP1:
		addr = SPM_DIS1_PWR_CON;
		break;
	case DPC_SUBSYS_OVL0:
		addr = SPM_OVL0_PWR_CON;
		break;
	case DPC_SUBSYS_OVL1:
		addr = SPM_OVL1_PWR_CON;
		break;
	case DPC_SUBSYS_MML1:
		addr = SPM_MML1_PWR_CON;
		break;
	default:
		/* unknown subsys type */
		return ret;
	}

	if (readl_poll_timeout_atomic(g_priv->spm_base + addr,
			value, value & SPM_PWR_ACK, 1, TIMEOUT_CNT)) {
		DPCERR("wait subsys%d power on timeout\n", subsys);
		ret = -1;
	}

	return ret;
}
#endif

int dpc_vidle_power_keep(const enum mtk_vidle_voter_user user)
{
	if (unlikely(!debug_force_power))
		return 0;

	pm_runtime_get_sync(g_priv->dev);
	mtk_disp_vlp_vote(VOTE_SET, user);

	return 0;
}
EXPORT_SYMBOL(dpc_vidle_power_keep);

void dpc_vidle_power_release(const enum mtk_vidle_voter_user user)
{
	if (unlikely(!debug_force_power))
		return;

	mtk_disp_vlp_vote(VOTE_CLR, user);
	pm_runtime_put_sync(g_priv->dev);
}
EXPORT_SYMBOL(dpc_vidle_power_release);

#if IS_ENABLED(CONFIG_DEBUG_FS)
static void process_dbg_opt(const char *opt)
{
	int ret = 0;
	u32 val = 0, v1 = 0, v2 = 0;

	if (strncmp(opt, "en:", 3) == 0) {
		ret = sscanf(opt, "en:%u\n", &val);
		if (ret != 1)
			goto err;
		dpc_enable((bool)val);
	} else if (strncmp(opt, "cfg:", 4) == 0) {
		ret = sscanf(opt, "cfg:%u\n", &val);
		if (ret != 1)
			goto err;
		if (val == 1) {
			const u32 dummy_regs[DPC_DEBUG_RTFF_CNT] = {
				0x14000400,
				0x1402040c,
				0x14200400,
				0x14206220,
				0x14402200,
				0x14403200,
				0x14602200,
				0x14603200,
				0x1f800400,
				0x1f81a100,
			};

			for (val = 0; val < DPC_DEBUG_RTFF_CNT; val++)
				debug_rtff[val] = ioremap(dummy_regs[val], 0x4);

			/* TODO: remove me after vcore dvfsrc ready */
			writel(0x1, g_priv->dvfsrc_en);

			/* default value for HRT and SRT */
			writel(0x66, dpc_base + DISP_REG_DPC_DISP_HIGH_HRT_BW);
			writel(0x154, dpc_base + DISP_REG_DPC_DISP_SW_SRT_BW);

			/* debug only, skip RROT Read done */
			writel(BIT(4), dpc_base + DISP_REG_DPC_MML_DT_CFG);

			dpc_config(DPC_SUBSYS_DISP, true);
			dpc_group_enable(DPC_DISP_VIDLE_RESERVED, true);
		} else {
			dpc_config(DPC_SUBSYS_DISP, false);
		}
	} else if (strncmp(opt, "mmlcfg:", 7) == 0) {
		ret = sscanf(opt, "mmlcfg:%u\n", &val);
		if (ret != 1)
			goto err;
		dpc_config(DPC_SUBSYS_MML, (bool)val);
	} else if (strncmp(opt, "event", 5) == 0) {
		dpc_debug_event();
	} else if (strncmp(opt, "irq", 3) == 0) {
		dpc_irq_init(g_priv);
	} else if (strncmp(opt, "swmode:", 7) == 0) {
		ret = sscanf(opt, "swmode:%u\n", &val);
		if (ret != 1)
			goto err;
		if (val) {
			writel(0xFFFFFFFF, dpc_base + DISP_REG_DPC_DISP_DT_EN);
			writel(0xFFFFFFFF, dpc_base + DISP_REG_DPC_DISP_DT_SW_TRIG_EN);
		} else
			writel(0, dpc_base + DISP_REG_DPC_DISP_DT_SW_TRIG_EN);
	} else if (strncmp(opt, "trig:", 5) == 0) {
		ret = sscanf(opt, "trig:%u\n", &val);
		if (ret != 1)
			goto err;
		dpc_dt_sw_trig(val);
	} else if (strncmp(opt, "vdisp:", 6) == 0) {
		ret = sscanf(opt, "vdisp:%u\n", &val);
		if (ret != 1)
			goto err;
		dpc_dvfs_set(DPC_SUBSYS_DISP, val, true);
	} else if (strncmp(opt, "dt:", 3) == 0) {
		ret = sscanf(opt, "dt:%u,%u\n", &v1, &v2);
		if (ret != 2)
			goto err;
		dpc_dt_set((u16)v1, v2);
	} else if (strncmp(opt, "vote:", 5) == 0) {
		ret = sscanf(opt, "vote:%u,%u\n", &v1, &v2);
		if (ret != 2)
			goto err;
		dpc_mtcmos_vote(DPC_SUBSYS_OVL0, v1, (bool)v2);
	} else if (strncmp(opt, "wr:", 3) == 0) {
		ret = sscanf(opt, "wr:0x%x=0x%x\n", &v1, &v2);
		if (ret != 2)
			goto err;
		DPCFUNC("(%#llx)=(%x)", (u64)(dpc_base + v1), v2);
		writel(v2, dpc_base + v1);
	} else if (strncmp(opt, "avs:", 4) == 0) {
		ret = sscanf(opt, "avs:%u,%u\n", &v1, &v2);
		if (ret != 2)
			goto err;
		writel(v2, MEM_VDISP_AVS_STEP(v1));
		mtk_mmdvfs_v3_set_force_step(2, v1);
	} else if (strncmp(opt, "vdo", 3) == 0) {
		writel(DISP_DPC_EN|DISP_DPC_DT_EN|DISP_DPC_VDO_MODE, dpc_base + DISP_REG_DPC_EN);
	}

	return;
err:
	DPCERR();
}
static ssize_t fs_write(struct file *file, const char __user *ubuf, size_t count, loff_t *ppos)
{
	const u32 debug_bufmax = 512 - 1;
	size_t ret;
	char cmd_buffer[512] = {0};
	char *tok, *buf;

	ret = count;

	if (count > debug_bufmax)
		count = debug_bufmax;

	if (copy_from_user(&cmd_buffer, ubuf, count))
		return -EFAULT;

	cmd_buffer[count] = 0;
	buf = cmd_buffer;
	DPCFUNC("%s", cmd_buffer);

	while ((tok = strsep(&buf, " ")) != NULL)
		process_dbg_opt(tok);

	return ret;
}

static const struct file_operations debug_fops = {
	.write = fs_write,
};
#endif

static const struct dpc_funcs funcs = {
	.dpc_enable = dpc_enable,
	.dpc_ddr_force_enable = dpc_ddr_force_enable,
	.dpc_infra_force_enable = dpc_infra_force_enable,
	.dpc_dc_force_enable = dpc_dc_force_enable,
	.dpc_group_enable = dpc_group_enable,
	.dpc_config = dpc_config,
	.dpc_mtcmos_vote = dpc_mtcmos_vote,
	.vidle_power_keep = dpc_vidle_power_keep,
	.vidle_power_release = dpc_vidle_power_release,
	.dpc_hrt_bw_set = dpc_hrt_bw_set,
	.dpc_srt_bw_set = dpc_srt_bw_set,
	.dpc_dvfs_set = dpc_dvfs_set,
};

static int mtk_dpc_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct mtk_dpc *priv;
	int ret = 0;

	DPCFUNC("+");
	priv = devm_kzalloc(dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	g_priv = priv;

	platform_set_drvdata(pdev, priv);
	priv->pdev = pdev;
	priv->dev = dev;

	pm_runtime_enable(dev);
	// pm_runtime_irq_safe(dev);

	ret = dpc_res_init(priv);
	if (ret)
		return ret;

	/* disable merge irq */
	writel(0, dpc_base + DISP_REG_DPC_MERGE_DISP_INT_CFG);
	writel(0, dpc_base + DISP_REG_DPC_MERGE_DISP_INTSTA);
	writel(0, dpc_base + DISP_REG_DPC_MERGE_MML_INT_CFG);
	writel(0, dpc_base + DISP_REG_DPC_MERGE_MML_INTSTA);
	// ret = dpc_irq_init(priv);
	// if (ret)
		// return ret;

	/* enable external signal from DSI and TE */
	writel(0x1F, dpc_base + DISP_REG_DPC_DISP_EXT_INPUT_EN);
	writel(0x3, dpc_base + DISP_REG_DPC_MML_EXT_INPUT_EN);

	/* SW_CTRL and SW_VAL=1 */
	dpc_ddr_force_enable(DPC_SUBSYS_DISP, true);
	dpc_ddr_force_enable(DPC_SUBSYS_MML, true);
	writel(0x1A1A1A, dpc_base + DISP_REG_DPC_DISP_INFRA_PLL_OFF_CFG);
	writel(0x1A1A1A, dpc_base + DISP_REG_DPC_MML_INFRA_PLL_OFF_CFG);

	/* keep vdisp opp */
	writel(0x1, dpc_base + DISP_REG_DPC_DISP_VDISP_DVFS_CFG);
	writel(0x1, dpc_base + DISP_REG_DPC_MML_VDISP_DVFS_CFG);

	/* keep HRT and SRT BW */
	writel(0x00010001, dpc_base + DISP_REG_DPC_DISP_HRTBW_SRTBW_CFG);
	writel(0x00010001, dpc_base + DISP_REG_DPC_MML_HRTBW_SRTBW_CFG);

#if IS_ENABLED(CONFIG_DEBUG_FS)
	priv->fs = debugfs_create_file("dpc_ctrl", S_IFREG | 0440, NULL, NULL, &debug_fops);
	if (IS_ERR(priv->fs))
		DPCERR("debugfs_create_file failed:%ld", PTR_ERR(priv->fs));
#endif

	dpc_mmp_init();

	mtk_vidle_register(&funcs);
	mml_dpc_register(&funcs);
	mdp_dpc_register(&funcs);

	DPCFUNC("-");
	return ret;
}

static int mtk_dpc_remove(struct platform_device *pdev)
{
	DPCFUNC();
	return 0;
}

static const struct of_device_id mtk_dpc_driver_dt_match[] = {
	{.compatible = "mediatek,mt6989-disp-dpc"},
	{.compatible = "mediatek,mt6985-disp-dpc"},
	{},
};
MODULE_DEVICE_TABLE(of, mtk_dpc_driver_dt_match);

struct platform_driver mtk_dpc_driver = {
	.probe = mtk_dpc_probe,
	.remove = mtk_dpc_remove,
	.driver = {
		.name = "mediatek-disp-dpc",
		.owner = THIS_MODULE,
		.of_match_table = mtk_dpc_driver_dt_match,
	},
};

static int __init mtk_dpc_init(void)
{
	DPCFUNC("+");
	platform_driver_register(&mtk_dpc_driver);
	DPCFUNC("-");
	return 0;
}

static void __exit mtk_dpc_exit(void)
{
	DPCFUNC();
}

module_init(mtk_dpc_init);
module_exit(mtk_dpc_exit);

MODULE_AUTHOR("William Yang <William-tw.Yang@mediatek.com>");
MODULE_DESCRIPTION("MTK Display Power Controller");
MODULE_LICENSE("GPL");
