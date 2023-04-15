// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2023 MediaTek Inc.
 */

#include <linux/clk.h>
#include <linux/component.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/of_irq.h>
#include <linux/platform_device.h>

#if IS_ENABLED(CONFIG_DEBUG_FS)
#include <linux/debugfs.h>
#endif

#include "mtk_dpc.h"

int dpc_debug1;
module_param(dpc_debug1, int, 0644);
int dpc_debug2;
module_param(dpc_debug2, int, 0644);

static void __iomem *dpc_base;
#define DISP_REG_DPC_EN                                  (0x000UL)
#define DISP_REG_DPC_RESET                               (0x004UL)
#define DISP_REG_DPC_MERGE_DISP_INT_CFG                  (0x008UL)
#define DISP_REG_DPC_MERGE_MML_INT_CFG                   (0x00CUL)
#define DISP_REG_DPC_MERGE_DISP_INTSTA                   (0x010UL)
#define DISP_REG_DPC_MERGE_MML_INTSTA                    (0x014UL)
#define DISP_REG_DPC_MERGE_DISP_UP_INTSTA                (0x018UL)
#define DISP_REG_DPC_MERGE_MML_UP_INTSTA                 (0x01CUL)
#define DISP_REG_DPC_DISP_INTEN                          (0x030UL)
#define DISP_REG_DPC_DISP_INTSTA                         (0x034UL)
#define DISP_REG_DPC_DISP_UP_INTEN                       (0x038UL)
#define DISP_REG_DPC_DISP_UP_INTSTA                      (0x03CUL)
#define DISP_REG_DPC_MML_INTEN                           (0x040UL)
#define DISP_REG_DPC_MML_INTSTA                          (0x044UL)
#define DISP_REG_DPC_MML_UP_INTEN                        (0x048UL)
#define DISP_REG_DPC_MML_UP_INTSTA                       (0x04CUL)
#define DISP_REG_DPC_DISP_POWER_STATE_CFG                (0x050UL)
#define DISP_REG_DPC_MML_POWER_STATE_CFG                 (0x054UL)
#define DISP_REG_DPC_DISP_MASK_CFG                       (0x060UL)
#define DISP_REG_DPC_MML_MASK_CFG                        (0x064UL)
#define DISP_REG_DPC_DISP_DDRSRC_EMIREQ_CFG              (0x068UL)
#define DISP_REG_DPC_MML_DDRSRC_EMIREQ_CFG               (0x06CUL)
#define DISP_REG_DPC_DISP_HRTBW_SRTBW_CFG                (0x070UL)
#define DISP_REG_DPC_MML_HRTBW_SRTBW_CFG                 (0x074UL)
#define DISP_REG_DPC_DISP_HIGH_HRT_BW                    (0x078UL)
#define DISP_REG_DPC_DISP_LOW_HRT_BW                     (0x07CUL)
#define DISP_REG_DPC_DISP_SW_SRT_BW                      (0x080UL)
#define DISP_REG_DPC_MML_SW_HRT_BW                       (0x084UL)
#define DISP_REG_DPC_MML_SW_SRT_BW                       (0x088UL)
#define DISP_REG_DPC_DISP_VDISP_DVFS_CFG                 (0x090UL)
#define DISP_REG_DPC_MML_VDISP_DVFS_CFG                  (0x094UL)
#define DISP_REG_DPC_DISP_VDISP_DVFS_VAL                 (0x098UL)
#define DISP_REG_DPC_MML_VDISP_DVFS_VAL                  (0x09CUL)
#define DISP_REG_DPC_DISP_INFRA_PLL_OFF_CFG              (0x0A0UL)
#define DISP_REG_DPC_MML_INFRA_PLL_OFF_CFG               (0x0A4UL)
#define DISP_REG_DPC_EVENT_TYPE                          (0x0B0UL)
#define DISP_REG_DPC_EVENT_EN                            (0x0B4UL)
#define DISP_REG_DPC_HW_DCM                              (0x0B8UL)
#define DISP_REG_DPC_ACT_SWITCH_CFG                      (0x0BCUL)
#define DISP_REG_DPC_DDREN_ACK_SEL                       (0x0C0UL)
#define DISP_REG_DPC_DISP_EXT_INPUT_EN                   (0x0C4UL)
#define DISP_REG_DPC_MML_EXT_INPUT_EN                    (0x0C8UL)
#define DISP_REG_DPC_DISP_DT_CFG                         (0x0D0UL)
#define DISP_REG_DPC_MML_DT_CFG                          (0x0D4UL)
#define DISP_REG_DPC_DISP_DT_EN                          (0x0D8UL)
#define DISP_REG_DPC_DISP_DT_SW_TRIG_EN                  (0x0DCUL)
#define DISP_REG_DPC_MML_DT_EN                           (0x0E0UL)
#define DISP_REG_DPC_MML_DT_SW_TRIG_EN                   (0x0E4UL)
#define DISP_REG_DPC_DISP_DT_FOLLOW_CFG                  (0x0E8UL)
#define DISP_REG_DPC_MML_DT_FOLLOW_CFG                   (0x0ECUL)
#define DISP_REG_DPC_DTx_COUNTER(n)                      (0x100UL + 0x4 * (n))	// n = 0 ~ 56
#define DISP_REG_DPC_DTx_SW_TRIG(n)                      (0x200UL + 0x4 * (n))	// n = 0 ~ 56
#define DISP_REG_DPC_DISP0_MTCMOS_CFG                    (0x300UL)
#define DISP_REG_DPC_DISP0_MTCMOS_ON_DELAY_CFG           (0x304UL)
#define DISP_REG_DPC_DISP0_MTCMOS_STA                    (0x308UL)
#define DISP_REG_DPC_DISP0_MTCMOS_STATE_STA              (0x30CUL)
#define DISP_REG_DPC_DISP0_MTCMOS_OFF_PROT_CFG           (0x310UL)
#define DISP_REG_DPC_DISP0_THREADx_SET(n)                (0x320UL + 0x4 * (n))	// n = 0 ~ 7
#define DISP_REG_DPC_DISP0_THREADx_CLR(n)                (0x340UL + 0x4 * (n))	// n = 0 ~ 7
#define DISP_REG_DPC_DISP0_THREADx_CFG(n)                (0x360UL + 0x4 * (n))	// n = 0 ~ 7
#define DISP_REG_DPC_DISP1_MTCMOS_CFG                    (0x400UL)
#define DISP_REG_DPC_DISP1_MTCMOS_ON_DELAY_CFG           (0x404UL)
#define DISP_REG_DPC_DISP1_MTCMOS_STA                    (0x408UL)
#define DISP_REG_DPC_DISP1_MTCMOS_STATE_STA              (0x40CUL)
#define DISP_REG_DPC_DISP1_MTCMOS_OFF_PROT_CFG           (0x410UL)
#define DISP_REG_DPC_DISP1_DSI_PLL_READY_TIME            (0x414UL)
#define DISP_REG_DPC_DISP1_THREADx_SET(n)                (0x420UL + 0x4 * (n))	// n = 0 ~ 7
#define DISP_REG_DPC_DISP1_THREADx_CLR(n)                (0x440UL + 0x4 * (n))	// n = 0 ~ 7
#define DISP_REG_DPC_DISP1_THREADx_CFG(n)                (0x460UL + 0x4 * (n))	// n = 0 ~ 7
#define DISP_REG_DPC_OVL0_MTCMOS_CFG                     (0x500UL)
#define DISP_REG_DPC_OVL0_MTCMOS_ON_DELAY_CFG            (0x504UL)
#define DISP_REG_DPC_OVL0_MTCMOS_STA                     (0x508UL)
#define DISP_REG_DPC_OVL0_MTCMOS_STATE_STA               (0x50CUL)
#define DISP_REG_DPC_OVL0_MTCMOS_OFF_PROT_CFG            (0x510UL)
#define DISP_REG_DPC_OVL0_THREADx_SET(n)                 (0x520UL + 0x4 * (n))	// n = 0 ~ 7
#define DISP_REG_DPC_OVL0_THREADx_CLR(n)                 (0x540UL + 0x4 * (n))	// n = 0 ~ 7
#define DISP_REG_DPC_OVL0_THREADx_CFG(n)                 (0x560UL + 0x4 * (n))	// n = 0 ~ 7
#define DISP_REG_DPC_OVL1_MTCMOS_CFG                     (0x600UL)
#define DISP_REG_DPC_OVL1_MTCMOS_ON_DELAY_CFG            (0x604UL)
#define DISP_REG_DPC_OVL1_MTCMOS_STA                     (0x608UL)
#define DISP_REG_DPC_OVL1_MTCMOS_STATE_STA               (0x60CUL)
#define DISP_REG_DPC_OVL1_MTCMOS_OFF_PROT_CFG            (0x610UL)
#define DISP_REG_DPC_OVL1_THREADx_SET(n)                 (0x620UL + 0x4 * (n))	// n = 0 ~ 7
#define DISP_REG_DPC_OVL1_THREADx_CLR(n)                 (0x640UL + 0x4 * (n))	// n = 0 ~ 7
#define DISP_REG_DPC_OVL1_THREADx_CFG(n)                 (0x660UL + 0x4 * (n))	// n = 0 ~ 7
#define DISP_REG_DPC_MML1_MTCMOS_CFG                     (0x700UL)
#define DISP_REG_DPC_MML1_MTCMOS_ON_DELAY_CFG            (0x704UL)
#define DISP_REG_DPC_MML1_MTCMOS_STA                     (0x708UL)
#define DISP_REG_DPC_MML1_MTCMOS_STATE_STA               (0x70CUL)
#define DISP_REG_DPC_MML1_MTCMOS_OFF_PROT_CFG            (0x710UL)
#define DISP_REG_DPC_MML1_THREADx_SET(n)                 (0x720UL + 0x4 * (n))	// n = 0 ~ 7
#define DISP_REG_DPC_MML1_THREADx_CLR(n)                 (0x740UL + 0x4 * (n))	// n = 0 ~ 7
#define DISP_REG_DPC_MML1_THREADx_CFG(n)                 (0x760UL + 0x4 * (n))	// n = 0 ~ 7
#define DISP_REG_DPC_DUMMY0                              (0x800UL)
#define DISP_REG_DPC_HW_SEMA0                            (0x804UL)
#define DISP_REG_DPC_DUMMY1                              (0x808UL)
#define DISP_REG_DPC_DT_STA0                             (0x810UL)	// TE Trigger
#define DISP_REG_DPC_DT_STA1                             (0x814UL)	// DSI SOF Trigger
#define DISP_REG_DPC_DT_STA2                             (0x818UL)	// DSI Frame Done Trigger
#define DISP_REG_DPC_DT_STA3                             (0x81CUL)	// Frame Done + Read Done
#define DISP_REG_DPC_POWER_STATE_STATUS                  (0x820UL)
#define DISP_REG_DPC_MTCMOS_STATUS                       (0x824UL)
#define DISP_REG_DPC_MTCMOS_CHECK_STATUS                 (0x828UL)
#define DISP_REG_DPC_DISP0_DEBUG0                        (0x840UL)
#define DISP_REG_DPC_DISP0_DEBUG1                        (0x844UL)
#define DISP_REG_DPC_DISP1_DEBUG0                        (0x848UL)
#define DISP_REG_DPC_DISP1_DEBUG1                        (0x84CUL)
#define DISP_REG_DPC_OVL0_DEBUG0                         (0x850UL)
#define DISP_REG_DPC_OVL0_DEBUG1                         (0x854UL)
#define DISP_REG_DPC_OVL1_DEBUG0                         (0x858UL)
#define DISP_REG_DPC_OVL1_DEBUG1                         (0x85CUL)
#define DISP_REG_DPC_MML1_DEBUG0                         (0x860UL)
#define DISP_REG_DPC_MML1_DEBUG1                         (0x864UL)
#define DISP_REG_DPC_DT_DEBUG0                           (0x868UL)
#define DISP_REG_DPC_DT_DEBUG1                           (0x86CUL)
#define DISP_REG_DPC_DEBUG_SEL                           (0x870UL)
#define DISP_REG_DPC_DEBUG_STA                           (0x874UL)

#define DISP_DPC_INT_DISP1_ON                            BIT(31)
#define DISP_DPC_INT_DISP1_OFF                           BIT(30)
#define DISP_DPC_INT_DISP0_ON                            BIT(29)
#define DISP_DPC_INT_DISP0_OFF                           BIT(28)
#define DISP_DPC_INT_OVL1_ON                             BIT(27)
#define DISP_DPC_INT_OVL1_OFF                            BIT(26)
#define DISP_DPC_INT_OVL0_ON                             BIT(25)
#define DISP_DPC_INT_OVL0_OFF                            BIT(24)
#define DISP_DPC_INT_DT31                                BIT(23)
#define DISP_DPC_INT_DT30                                BIT(22)
#define DISP_DPC_INT_DT29                                BIT(21)
#define DISP_DPC_INT_DSI_DONE                            BIT(20)
#define DISP_DPC_INT_DSI_START                           BIT(19)
#define DISP_DPC_INT_DT_TRIG_FRAME_DONE                  BIT(18)
#define DISP_DPC_INT_DT_TRIG_SOF                         BIT(17)
#define DISP_DPC_INT_DT_TRIG_TE                          BIT(16)
#define DISP_DPC_INT_INFRA_OFF_END                       BIT(15)
#define DISP_DPC_INT_INFRA_OFF_START                     BIT(14)
#define DISP_DPC_INT_MMINFRA_OFF_END                     BIT(13)
#define DISP_DPC_INT_MMINFRA_OFF_START                   BIT(12)
#define DISP_DPC_INT_DISP1_ACK_TIMEOUT                   BIT(11)
#define DISP_DPC_INT_DISP0_ACK_TIMEOUT                   BIT(10)
#define DISP_DPC_INT_OVL1_ACK_TIMEOUT                    BIT(9)
#define DISP_DPC_INT_OVL0_ACK_TIMEOUT                    BIT(8)
#define DISP_DPC_INT_DT7                                 BIT(7)
#define DISP_DPC_INT_DT6                                 BIT(6)
#define DISP_DPC_INT_DT5                                 BIT(5)
#define DISP_DPC_INT_DT4                                 BIT(4)
#define DISP_DPC_INT_DT3                                 BIT(3)
#define DISP_DPC_INT_DT2                                 BIT(2)
#define DISP_DPC_INT_DT1                                 BIT(1)
#define DISP_DPC_INT_DT0                                 BIT(0)

#define DISP_DPC_VDO_MODE                                BIT(16)
#define DISP_DPC_DT_EN                                   BIT(1)
#define DISP_DPC_EN                                      BIT(0)

struct mtk_dpc {
	struct platform_device *pdev;
	struct device *dev;
	int disp_irq;
	int mml_irq;
#if IS_ENABLED(CONFIG_DEBUG_FS)
	struct dentry *fs;
#endif
};

enum mtk_dpc_sp_type {
	DPC_SP_TE,
	DPC_SP_SOF,
	DPC_SP_FRAME_DONE,
	DPC_SP_RROT_READ_DONE,
};

struct mtk_dpc_dt_usage {
	s16 index;
	enum mtk_dpc_sp_type sp;		/* start point */
	u16 ep;					/* end point in us */
	u16 group;
};

static struct mtk_dpc_dt_usage mt6989_disp_dt_usage[32] = {
	/* OVL0/OVL1/DISP0 */
	{0, DPC_SP_FRAME_DONE,	50,	DPC_DISP_VIDLE_MTCMOS},		/* OFF Time 0 */
	{1, DPC_SP_TE,		7837,	DPC_DISP_VIDLE_MTCMOS},		/* ON Time */
	{2, DPC_SP_TE,		8137,	DPC_DISP_VIDLE_MTCMOS},		/* Pre-TE */
	{3, DPC_SP_TE,		50,	DPC_DISP_VIDLE_MTCMOS},		/* OFF Time 1 */

	/* DISP1 */
	{4, DPC_SP_FRAME_DONE,	100,	DPC_DISP_VIDLE_MTCMOS_DISP1},
	{5, DPC_SP_TE,		7787,	DPC_DISP_VIDLE_MTCMOS_DISP1},
	{6, DPC_SP_TE,		8137,	DPC_DISP_VIDLE_MTCMOS_DISP1},	/* DISP1-TE */
	{7, DPC_SP_TE,		100,	DPC_DISP_VIDLE_MTCMOS_DISP1},

	/* VDISP DVFS, can follow DPC_DISP_VIDLE_MTCMOS_DISP1 or DPC_VIDLE_HRT_BW */
	{8, DPC_SP_FRAME_DONE,	100,	DPC_DISP_VIDLE_VDISP_DVFS},
	{9, DPC_SP_TE,		100,	DPC_DISP_VIDLE_VDISP_DVFS},
	{10, DPC_SP_TE,		100,	DPC_DISP_VIDLE_VDISP_DVFS},

	/* HRT BW */
	{11, DPC_SP_FRAME_DONE,	100,	DPC_DISP_VIDLE_HRT_BW},		/* OFF Time 0 */
	{12, DPC_SP_TE,		100,	DPC_DISP_VIDLE_HRT_BW},		/* ON Time */
	{13, DPC_SP_TE,		100,	DPC_DISP_VIDLE_HRT_BW},		/* OFF Time 1 */

	/* SRT BW, can follow DPC_DISP_VIDLE_MTCMOS_DISP1 or DPC_VIDLE_HRT_BW */
	{14, DPC_SP_FRAME_DONE,	100,	DPC_DISP_VIDLE_SRT_BW},
	{15, DPC_SP_TE,		100,	DPC_DISP_VIDLE_SRT_BW},
	{16, DPC_SP_TE,		100,	DPC_DISP_VIDLE_SRT_BW},

	/* MMINFRA OFF, can follow DPC_DISP_VIDLE_MTCMOS_DISP1 or DPC_VIDLE_HRT_BW */
	{17, DPC_SP_FRAME_DONE,	50,	DPC_DISP_VIDLE_MMINFRA_OFF},
	{18, DPC_SP_TE,		7837,	DPC_DISP_VIDLE_MMINFRA_OFF},	/* Pre-TE - 300 */
	{19, DPC_SP_TE,		50,	DPC_DISP_VIDLE_MMINFRA_OFF},

	/* INFRA OFF, can follow DPC_DISP_VIDLE_MTCMOS_DISP1 or DPC_VIDLE_HRT_BW */
	{20, DPC_SP_FRAME_DONE,	100,	DPC_DISP_VIDLE_INFRA_OFF},
	{21, DPC_SP_TE,		100,	DPC_DISP_VIDLE_INFRA_OFF},
	{22, DPC_SP_TE,		100,	DPC_DISP_VIDLE_INFRA_OFF},

	/* MAINPLL OFF */
	{23, DPC_SP_FRAME_DONE,	100,	DPC_DISP_VIDLE_MAINPLL_OFF},
	{24, DPC_SP_TE,		100,	DPC_DISP_VIDLE_MAINPLL_OFF},
	{25, DPC_SP_TE,		100,	DPC_DISP_VIDLE_MAINPLL_OFF},

	/* MSYNC 2.0 */
	{26, DPC_SP_TE,		100,	DPC_DISP_VIDLE_MSYNC2_0},
	{27, DPC_SP_TE,		100,	DPC_DISP_VIDLE_MSYNC2_0},
	{28, DPC_SP_TE,		100,	DPC_DISP_VIDLE_MSYNC2_0},

	/* RESERVED */
	{29, DPC_SP_FRAME_DONE,	100,	DPC_DISP_VIDLE_RESERVED},
	{30, DPC_SP_TE,		100,	DPC_DISP_VIDLE_RESERVED},
	{31, DPC_SP_TE,		100,	DPC_DISP_VIDLE_RESERVED},
};

static struct mtk_dpc_dt_usage mt6989_mml_dt_usage[25] = {
	{32, DPC_SP_RROT_READ_DONE,	100,	DPC_MML_VIDLE_MTCMOS},	/* OFF Time 0 */
	{33, DPC_SP_TE,			100,	DPC_MML_VIDLE_MTCMOS},	/* ON Time */
	{34, DPC_SP_TE,			100,	DPC_MML_VIDLE_MTCMOS},	/* Pre-TE */
	{35, DPC_SP_TE,			100,	DPC_MML_VIDLE_MTCMOS},	/* OFF Time 1 */
};

static void dpc_enable(bool en);
static void dpc_dt_enable(u16 dt, bool en);
static void dpc_dt_set(u16 dt, u32 counter);
static void dpc_dt_sw_trig(u16 dt);
static void dpc_bw_set(const enum mtk_dpc_subsys subsys,
		       const bool is_hrt, const u32 high, const u32 low, const bool keep);
static void dpc_dvfs_set(const u8 level);
static void dpc_ddr_force_enable(const bool force);
static void dpc_infra_force_enable(const u8 infra);
static void dpc_smi_force_enable(const bool is_hrt, const bool force);

static void dpc_enable(bool en)
{
	if (en)
		writel(DISP_DPC_EN | DISP_DPC_DT_EN, dpc_base + DISP_REG_DPC_EN);
	else
		writel(0, dpc_base + DISP_REG_DPC_EN);

	/* enable gce event */
	writel(en, dpc_base + DISP_REG_DPC_EVENT_EN);

	DPCFUNC("en(%u)", en);
}

static void dpc_dt_enable(u16 dt, bool en)
{
	u32 value = readl(dpc_base + DISP_REG_DPC_DISP_DT_EN);

	if (en)
		writel(BIT(dt) | value, dpc_base + DISP_REG_DPC_DISP_DT_EN);
	else
		writel(~BIT(dt) & value, dpc_base + DISP_REG_DPC_DISP_DT_EN);
	DPCFUNC("dt(%u) en(%u)", dt, en);
}

static void dpc_dt_set(u16 dt, u32 us)
{
	u32 value = us * 100000 / 3846;	/* 26M base */

	writel(value, dpc_base + DISP_REG_DPC_DTx_COUNTER(dt));
	DPCFUNC("dt(%u) counter(%u)us", dt, us);
}

static void dpc_dt_sw_trig(u16 dt)
{
	DPCFUNC("dt(%u)", dt);
	writel(1, dpc_base + DISP_REG_DPC_DTx_SW_TRIG(dt));
}

static void dpc_disp_group_enable(const enum mtk_dpc_disp_vidle group, bool en)
{
	int i;
	u32 value = 0;

	DPCFUNC("group(%u) en(%u)", group, en);
	if (!en) {
		for (i = 0; i < 32; ++i) {
			if (group == mt6989_disp_dt_usage[i].group)
				dpc_dt_enable(mt6989_disp_dt_usage[i].index, false);
		}
		return;
	}

	switch (group) {
	case DPC_DISP_VIDLE_MTCMOS:
		/* enable pre-te int */
		value = readl(dpc_base + DISP_REG_DPC_DISP_INTEN);
		writel(DISP_DPC_INT_DT2 | value, dpc_base + DISP_REG_DPC_DISP_INTEN);

		/* MTCMOS auto_on_off enable, both ack */
		writel(BIT(0) | BIT(4), dpc_base + DISP_REG_DPC_DISP0_MTCMOS_CFG);
		writel(BIT(0) | BIT(4), dpc_base + DISP_REG_DPC_OVL0_MTCMOS_CFG);
		writel(BIT(0) | BIT(4), dpc_base + DISP_REG_DPC_OVL1_MTCMOS_CFG);
		break;
	case DPC_DISP_VIDLE_MTCMOS_DISP1:
		/* enable disp1-te int */
		value = readl(dpc_base + DISP_REG_DPC_DISP_INTEN);
		writel(DISP_DPC_INT_DT6 | value, dpc_base + DISP_REG_DPC_DISP_INTEN);

		/* MTCMOS auto_on_off enable, both ack, pwr off dependency */
		writel(BIT(0) | BIT(4) | BIT(6), dpc_base + DISP_REG_DPC_DISP1_MTCMOS_CFG);
		break;
	case DPC_DISP_VIDLE_MMINFRA_OFF:
		/* TODO: mask release*/
		break;
	default:
		break;
	}

	for (i = 0; i < 32; ++i) {
		if (group == mt6989_disp_dt_usage[i].group) {
			dpc_dt_set(mt6989_disp_dt_usage[i].index, mt6989_disp_dt_usage[i].ep);
			dpc_dt_enable(mt6989_disp_dt_usage[i].index, true);
		}
	}
}

static void dpc_mml_group_enable(const enum mtk_dpc_mml_vidle group, bool en)
{
	int i;

	DPCFUNC("group(%u) en(%u)", group, en);
	for (i = 0; i < 25; ++i) {
		if (group == mt6989_mml_dt_usage[i].group) {
			dpc_dt_enable(mt6989_mml_dt_usage[i].index, true);
			dpc_dt_set(mt6989_mml_dt_usage[i].index, mt6989_mml_dt_usage[i].ep);
		}
	}
}

static void dpc_bw_set(const enum mtk_dpc_subsys subsys,
		       const bool is_hrt, const u32 high, const u32 low, const bool keep)
{

}

static void dpc_dvfs_set(const u8 level)
{

}

static void dpc_ddr_force_enable(const bool force)
{
	/* EMIREQ, DDRSRC */
}

static void dpc_infra_force_enable(const u8 infra)
{
	/* MAINPLL, MMINFRA, INFRA */
}

static void dpc_smi_force_enable(const bool is_hrt, const bool force)
{

}

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

void dpc_config(const enum mtk_dpc_subsys subsys)
{
	if (subsys == DPC_SUBSYS_DISP) {
		dpc_group_enable(DPC_DISP_VIDLE_MTCMOS, true);
		dpc_group_enable(DPC_DISP_VIDLE_MMINFRA_OFF, true);
	} else if (subsys == DPC_SUBSYS_MML) {
		dpc_group_enable(DPC_MML_VIDLE_MTCMOS, true);
	}
}
EXPORT_SYMBOL(dpc_config);

void dpc_mtcmos_vote(const enum mtk_dpc_subsys subsys, const u8 thread, const bool en)
{
	u32 addr = 0;

	switch (subsys) {
	case DPC_SUBSYS_DISP0:
		addr = en ? DISP_REG_DPC_DISP0_THREADx_SET(thread)
			  : DISP_REG_DPC_DISP0_THREADx_CLR(thread);
		break;
	case DPC_SUBSYS_DISP1:
		addr = en ? DISP_REG_DPC_DISP1_THREADx_SET(thread)
			  : DISP_REG_DPC_DISP1_THREADx_CLR(thread);
		break;
	case DPC_SUBSYS_OVL0:
		addr = en ? DISP_REG_DPC_OVL0_THREADx_SET(thread)
			  : DISP_REG_DPC_OVL0_THREADx_CLR(thread);
		break;
	case DPC_SUBSYS_OVL1:
		addr = en ? DISP_REG_DPC_OVL1_THREADx_SET(thread)
			  : DISP_REG_DPC_OVL1_THREADx_CLR(thread);
		break;
	case DPC_SUBSYS_MML1:
		addr = en ? DISP_REG_DPC_MML1_THREADx_SET(thread)
			  : DISP_REG_DPC_MML1_THREADx_CLR(thread);
		break;
	default:
		break;
	}

	writel(1, dpc_base + addr);
	DPCFUNC("subsys(%u:%u) addr(0x%x) vote %s", subsys, thread, addr, en ? "SET" : "CLR");
}
EXPORT_SYMBOL(dpc_mtcmos_vote);

irqreturn_t mtk_dpc_disp_irq_handler(int irq, void *dev_id)
{
	struct mtk_dpc *priv = dev_id;
	u32 status;

	if (IS_ERR_OR_NULL(priv))
		return IRQ_NONE;

	status = readl(dpc_base + DISP_REG_DPC_DISP_INTSTA);
	if (!status)
		return IRQ_NONE;

	writel(~status, dpc_base + DISP_REG_DPC_DISP_INTSTA);

	if (dpc_debug1) {
		DPCFUNC("%x", status);
		--dpc_debug1;
	}
	if (status & DISP_DPC_INT_DT2) {
		if (dpc_debug2) {
			DPCFUNC("Pre-TE");
			--dpc_debug2;
		}
	}

	return IRQ_HANDLED;
}

irqreturn_t mtk_dpc_mml_irq_handler(int irq, void *dev_id)
{
	return IRQ_HANDLED;
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
		// writel(0, dpc_base + DISP_REG_DPC_DISP_INTEN);
		// writel(0, dpc_base + DISP_REG_DPC_DISP_INTSTA);
		writel(0, dpc_base + DISP_REG_DPC_MERGE_DISP_INT_CFG);
		writel(0, dpc_base + DISP_REG_DPC_MERGE_DISP_INTSTA);
		ret = devm_request_irq(priv->dev, priv->disp_irq, mtk_dpc_disp_irq_handler,
				       IRQF_TRIGGER_NONE | IRQF_SHARED, dev_name(priv->dev), priv);
		if (ret)
			DPCERR("devm_request_irq %d fail: %d", priv->disp_irq, ret);
	}
	if (priv->mml_irq > 0) {
		// writel(0, dpc_base + DISP_REG_DPC_MML_INTEN);
		// writel(0, dpc_base + DISP_REG_DPC_MML_INTSTA);
		writel(0, dpc_base + DISP_REG_DPC_MERGE_MML_INT_CFG);
		writel(0, dpc_base + DISP_REG_DPC_MERGE_MML_INTSTA);
		ret = devm_request_irq(priv->dev, priv->mml_irq, mtk_dpc_mml_irq_handler,
				       IRQF_TRIGGER_NONE | IRQF_SHARED, dev_name(priv->dev), priv);
		if (ret)
			DPCERR("devm_request_irq %d fail: %d", priv->mml_irq, ret);
	}
	DPCFUNC("disp irq %d, mml irq %d, ret %d", priv->disp_irq, priv->mml_irq, ret);

	return ret;
}

#if IS_ENABLED(CONFIG_DEBUG_FS)
static void process_dbg_opt(const char *opt)
{
	int ret = 0;

	if (strncmp(opt, "en:", 3) == 0) {
		u32 val = 0;

		ret = sscanf(opt, "en:%u\n", &val);
		if (ret != 1)
			goto err;
		dpc_enable((bool)val);
	} else if (strncmp(opt, "cfg", 3) == 0) {
		dpc_config(DPC_SUBSYS_DISP);
		dpc_config(DPC_SUBSYS_MML);
	} else if (strncmp(opt, "trig:", 5) == 0) {
		u32 val = 0;

		ret = sscanf(opt, "trig:%u\n", &val);
		if (ret != 1)
			goto err;
		dpc_dt_sw_trig(val);
	} else if (strncmp(opt, "vote:", 5) == 0) {
		u32 v1 = 0, v2 = 0;

		ret = sscanf(opt, "vote:%u,%u\n", &v1, &v2);
		if (ret != 2)
			goto err;
		dpc_mtcmos_vote(DPC_SUBSYS_OVL0, v1, (bool)v2);
	} else if (strncmp(opt, "vdo", 3) == 0) {
		writel(DISP_DPC_EN|DISP_DPC_DT_EN|DISP_DPC_VDO_MODE, dpc_base + DISP_REG_DPC_EN);
	} else if (strncmp(opt, "no warning", 10) == 0) {
		dpc_bw_set(DPC_SUBSYS_OVL0, true, 2000, 1000, true);
		dpc_dvfs_set(0x5);
		dpc_ddr_force_enable(false);
		dpc_infra_force_enable(0);
		dpc_smi_force_enable(true, false);
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

static int mtk_dpc_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct mtk_dpc *priv;
	struct resource regs;
	int ret = 0;

	DPCFUNC("+");
	priv = devm_kzalloc(dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	platform_set_drvdata(pdev, priv);
	priv->pdev = pdev;
	priv->dev = dev;

	if (of_address_to_resource(dev->of_node, 0, &regs) != 0) {
		DPCERR("miss reg in node");
		return ret;
	}
	DPCFUNC("base address %llx", (u64)regs.start);
	dpc_base = of_iomap(dev->of_node, 0);
	if (!dpc_base) {
		DPCERR("iomap failed");
		return ret;
	}

	ret = dpc_irq_init(priv);
	if (ret)
		return ret;

	/* enable external signal from DSI and TE */
	writel(0x1F, dpc_base + DISP_REG_DPC_DISP_EXT_INPUT_EN);
	writel(0x3, dpc_base + DISP_REG_DPC_MML_EXT_INPUT_EN);

	/* SW_CTRL and SW_VAL=1 */
	writel(0x000C000C, dpc_base + DISP_REG_DPC_DISP_DDRSRC_EMIREQ_CFG);
	writel(0x000C000C, dpc_base + DISP_REG_DPC_MML_DDRSRC_EMIREQ_CFG);
	writel(0x00181818, dpc_base + DISP_REG_DPC_DISP_INFRA_PLL_OFF_CFG);
	writel(0x00181818, dpc_base + DISP_REG_DPC_MML_INFRA_PLL_OFF_CFG);

	/* TODO: default setting fow Lower SMI DVFS, Lower VDISP DVFS */

#if IS_ENABLED(CONFIG_DEBUG_FS)
	priv->fs = debugfs_create_file("dpc_ctrl", S_IFREG | 0440, NULL, NULL, &debug_fops);
	if (IS_ERR(priv->fs))
		DPCERR("debugfs_create_file failed:%ld", PTR_ERR(priv->fs));
#endif

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
