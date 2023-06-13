/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2023 MediaTek Inc.
 */

#ifndef __MTK_DPC_INTERNAL_H__
#define __MTK_DPC_INTERNAL_H__

#ifdef IF_ZERO
#define DPCFUNC(fmt, args...) \
	pr_info("[dpc] %s:%d " fmt "\n", __func__, __LINE__, ##args)
#else
#define DPCFUNC(args...)
#endif

#define DPCERR(fmt, args...) \
	pr_info("[dpc][err] %s:%d " fmt "\n", __func__, __LINE__, ##args)

#define VLP_DISP_SW_VOTE_SET 0x414
#define VLP_DISP_SW_VOTE_CLR 0x418
#define SPM_DIS0_PWR_CON 0xE98
#define SPM_DIS1_PWR_CON 0xE9C
#define SPM_OVL0_PWR_CON 0xEA0
#define SPM_OVL1_PWR_CON 0xEA4
#define SPM_MML1_PWR_CON 0xE94

#define DISP_REG_DPC_EN                                  0x000UL
#define DISP_REG_DPC_RESET                               0x004UL
#define DISP_REG_DPC_MERGE_DISP_INT_CFG                  0x008UL
#define DISP_REG_DPC_MERGE_MML_INT_CFG                   0x00CUL
#define DISP_REG_DPC_MERGE_DISP_INTSTA                   0x010UL
#define DISP_REG_DPC_MERGE_MML_INTSTA                    0x014UL
#define DISP_REG_DPC_MERGE_DISP_UP_INTSTA                0x018UL
#define DISP_REG_DPC_MERGE_MML_UP_INTSTA                 0x01CUL
#define DISP_REG_DPC_DISP_INTEN                          0x030UL
#define DISP_REG_DPC_DISP_INTSTA                         0x034UL
#define DISP_REG_DPC_DISP_UP_INTEN                       0x038UL
#define DISP_REG_DPC_DISP_UP_INTSTA                      0x03CUL
#define DISP_REG_DPC_MML_INTEN                           0x040UL
#define DISP_REG_DPC_MML_INTSTA                          0x044UL
#define DISP_REG_DPC_MML_UP_INTEN                        0x048UL
#define DISP_REG_DPC_MML_UP_INTSTA                       0x04CUL
#define DISP_REG_DPC_DISP_POWER_STATE_CFG                0x050UL
#define DISP_REG_DPC_MML_POWER_STATE_CFG                 0x054UL
#define DISP_REG_DPC_DISP_MASK_CFG                       0x060UL
#define DISP_REG_DPC_MML_MASK_CFG                        0x064UL
#define DISP_REG_DPC_DISP_DDRSRC_EMIREQ_CFG              0x068UL
#define DISP_REG_DPC_MML_DDRSRC_EMIREQ_CFG               0x06CUL
#define DISP_REG_DPC_DISP_HRTBW_SRTBW_CFG                0x070UL
#define DISP_REG_DPC_MML_HRTBW_SRTBW_CFG                 0x074UL
#define DISP_REG_DPC_DISP_HIGH_HRT_BW                    0x078UL
#define DISP_REG_DPC_DISP_LOW_HRT_BW                     0x07CUL
#define DISP_REG_DPC_DISP_SW_SRT_BW                      0x080UL
#define DISP_REG_DPC_MML_SW_HRT_BW                       0x084UL
#define DISP_REG_DPC_MML_SW_SRT_BW                       0x088UL
#define DISP_REG_DPC_DISP_VDISP_DVFS_CFG                 0x090UL
#define DISP_REG_DPC_MML_VDISP_DVFS_CFG                  0x094UL
#define DISP_REG_DPC_DISP_VDISP_DVFS_VAL                 0x098UL
#define DISP_REG_DPC_MML_VDISP_DVFS_VAL                  0x09CUL
#define DISP_REG_DPC_DISP_INFRA_PLL_OFF_CFG              0x0A0UL
#define DISP_REG_DPC_MML_INFRA_PLL_OFF_CFG               0x0A4UL
#define DISP_REG_DPC_EVENT_TYPE                          0x0B0UL
#define DISP_REG_DPC_EVENT_EN                            0x0B4UL
#define DISP_REG_DPC_HW_DCM                              0x0B8UL
#define DISP_REG_DPC_ACT_SWITCH_CFG                      0x0BCUL
#define DISP_REG_DPC_DDREN_ACK_SEL                       0x0C0UL
#define DISP_REG_DPC_DISP_EXT_INPUT_EN                   0x0C4UL
#define DISP_REG_DPC_MML_EXT_INPUT_EN                    0x0C8UL
#define DISP_REG_DPC_DISP_DT_CFG                         0x0D0UL
#define DISP_REG_DPC_MML_DT_CFG                          0x0D4UL
#define DISP_REG_DPC_DISP_DT_EN                          0x0D8UL
#define DISP_REG_DPC_DISP_DT_SW_TRIG_EN                  0x0DCUL
#define DISP_REG_DPC_MML_DT_EN                           0x0E0UL
#define DISP_REG_DPC_MML_DT_SW_TRIG_EN                   0x0E4UL
#define DISP_REG_DPC_DISP_DT_FOLLOW_CFG                  0x0E8UL
#define DISP_REG_DPC_MML_DT_FOLLOW_CFG                   0x0ECUL
#define DISP_REG_DPC_DTx_COUNTER(n)                      (0x100UL + 0x4 * (n))	// n = 0 ~ 56
#define DISP_REG_DPC_DTx_SW_TRIG(n)                      (0x200UL + 0x4 * (n))	// n = 0 ~ 56
#define DISP_REG_DPC_DISP0_MTCMOS_CFG                    0x300UL
#define DISP_REG_DPC_DISP0_MTCMOS_ON_DELAY_CFG           0x304UL
#define DISP_REG_DPC_DISP0_MTCMOS_STA                    0x308UL
#define DISP_REG_DPC_DISP0_MTCMOS_STATE_STA              0x30CUL
#define DISP_REG_DPC_DISP0_MTCMOS_OFF_PROT_CFG           0x310UL
#define DISP_REG_DPC_DISP0_THREADx_SET(n)                (0x320UL + 0x4 * (n))	// n = 0 ~ 7
#define DISP_REG_DPC_DISP0_THREADx_CLR(n)                (0x340UL + 0x4 * (n))	// n = 0 ~ 7
#define DISP_REG_DPC_DISP0_THREADx_CFG(n)                (0x360UL + 0x4 * (n))	// n = 0 ~ 7
#define DISP_REG_DPC_DISP1_MTCMOS_CFG                    0x400UL
#define DISP_REG_DPC_DISP1_MTCMOS_ON_DELAY_CFG           0x404UL
#define DISP_REG_DPC_DISP1_MTCMOS_STA                    0x408UL
#define DISP_REG_DPC_DISP1_MTCMOS_STATE_STA              0x40CUL
#define DISP_REG_DPC_DISP1_MTCMOS_OFF_PROT_CFG           0x410UL
#define DISP_REG_DPC_DISP1_DSI_PLL_READY_TIME            0x414UL
#define DISP_REG_DPC_DISP1_THREADx_SET(n)                (0x420UL + 0x4 * (n))	// n = 0 ~ 7
#define DISP_REG_DPC_DISP1_THREADx_CLR(n)                (0x440UL + 0x4 * (n))	// n = 0 ~ 7
#define DISP_REG_DPC_DISP1_THREADx_CFG(n)                (0x460UL + 0x4 * (n))	// n = 0 ~ 7
#define DISP_REG_DPC_OVL0_MTCMOS_CFG                     0x500UL
#define DISP_REG_DPC_OVL0_MTCMOS_ON_DELAY_CFG            0x504UL
#define DISP_REG_DPC_OVL0_MTCMOS_STA                     0x508UL
#define DISP_REG_DPC_OVL0_MTCMOS_STATE_STA               0x50CUL
#define DISP_REG_DPC_OVL0_MTCMOS_OFF_PROT_CFG            0x510UL
#define DISP_REG_DPC_OVL0_THREADx_SET(n)                 (0x520UL + 0x4 * (n))	// n = 0 ~ 7
#define DISP_REG_DPC_OVL0_THREADx_CLR(n)                 (0x540UL + 0x4 * (n))	// n = 0 ~ 7
#define DISP_REG_DPC_OVL0_THREADx_CFG(n)                 (0x560UL + 0x4 * (n))	// n = 0 ~ 7
#define DISP_REG_DPC_OVL1_MTCMOS_CFG                     0x600UL
#define DISP_REG_DPC_OVL1_MTCMOS_ON_DELAY_CFG            0x604UL
#define DISP_REG_DPC_OVL1_MTCMOS_STA                     0x608UL
#define DISP_REG_DPC_OVL1_MTCMOS_STATE_STA               0x60CUL
#define DISP_REG_DPC_OVL1_MTCMOS_OFF_PROT_CFG            0x610UL
#define DISP_REG_DPC_OVL1_THREADx_SET(n)                 (0x620UL + 0x4 * (n))	// n = 0 ~ 7
#define DISP_REG_DPC_OVL1_THREADx_CLR(n)                 (0x640UL + 0x4 * (n))	// n = 0 ~ 7
#define DISP_REG_DPC_OVL1_THREADx_CFG(n)                 (0x660UL + 0x4 * (n))	// n = 0 ~ 7
#define DISP_REG_DPC_MML1_MTCMOS_CFG                     0x700UL
#define DISP_REG_DPC_MML1_MTCMOS_ON_DELAY_CFG            0x704UL
#define DISP_REG_DPC_MML1_MTCMOS_STA                     0x708UL
#define DISP_REG_DPC_MML1_MTCMOS_STATE_STA               0x70CUL
#define DISP_REG_DPC_MML1_MTCMOS_OFF_PROT_CFG            0x710UL
#define DISP_REG_DPC_MML1_THREADx_SET(n)                 (0x720UL + 0x4 * (n))	// n = 0 ~ 7
#define DISP_REG_DPC_MML1_THREADx_CLR(n)                 (0x740UL + 0x4 * (n))	// n = 0 ~ 7
#define DISP_REG_DPC_MML1_THREADx_CFG(n)                 (0x760UL + 0x4 * (n))	// n = 0 ~ 7
#define DISP_REG_DPC_DUMMY0                              0x800UL
#define DISP_REG_DPC_HW_SEMA0                            0x804UL
#define DISP_REG_DPC_DUMMY1                              0x808UL
#define DISP_REG_DPC_DT_STA0                             0x810UL	// TE Trigger
#define DISP_REG_DPC_DT_STA1                             0x814UL	// DSI SOF Trigger
#define DISP_REG_DPC_DT_STA2                             0x818UL	// DSI Frame Done Trigger
#define DISP_REG_DPC_DT_STA3                             0x81CUL	// Frame Done + Read Done
#define DISP_REG_DPC_POWER_STATE_STATUS                  0x820UL
#define DISP_REG_DPC_MTCMOS_STATUS                       0x824UL
#define DISP_REG_DPC_MTCMOS_CHECK_STATUS                 0x828UL
#define DISP_REG_DPC_DISP0_DEBUG0                        0x840UL
#define DISP_REG_DPC_DISP0_DEBUG1                        0x844UL
#define DISP_REG_DPC_DISP1_DEBUG0                        0x848UL
#define DISP_REG_DPC_DISP1_DEBUG1                        0x84CUL
#define DISP_REG_DPC_OVL0_DEBUG0                         0x850UL
#define DISP_REG_DPC_OVL0_DEBUG1                         0x854UL
#define DISP_REG_DPC_OVL1_DEBUG0                         0x858UL
#define DISP_REG_DPC_OVL1_DEBUG1                         0x85CUL
#define DISP_REG_DPC_MML1_DEBUG0                         0x860UL
#define DISP_REG_DPC_MML1_DEBUG1                         0x864UL
#define DISP_REG_DPC_DT_DEBUG0                           0x868UL
#define DISP_REG_DPC_DT_DEBUG1                           0x86CUL
#define DISP_REG_DPC_DEBUG_SEL                           0x870UL
#define DISP_REG_DPC_DEBUG_STA                           0x874UL

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

#define VOTE_SET 1
#define VOTE_CLR 0

#define DPC_DISP_DT_CNT 32
#define DPC_MML_DT_CNT 25

enum mtk_dpc_sp_type {
	DPC_SP_TE,
	DPC_SP_SOF,
	DPC_SP_FRAME_DONE,
	DPC_SP_RROT_DONE,
};

struct mtk_dpc_dt_usage {
	s16 index;
	enum mtk_dpc_sp_type sp;		/* start point */
	u16 ep;					/* end point in us */
	u16 group;
};

static void dpc_dt_enable(u16 dt, bool en);
static void dpc_dt_set(u16 dt, u32 counter);
static void dpc_dt_sw_trig(u16 dt);

#endif
