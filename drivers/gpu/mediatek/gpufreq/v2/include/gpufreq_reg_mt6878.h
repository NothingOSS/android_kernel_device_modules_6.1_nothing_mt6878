/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2023 MediaTek Inc.
 */

#ifndef __GPUFREQ_REG_MT6878_H__
#define __GPUFREQ_REG_MT6878_H__

#include <linux/io.h>
#include <linux/bits.h>

/**************************************************
 * GPUFREQ Register Operation
 **************************************************/

/**************************************************
 * GPUFREQ Register Definition
 **************************************************/
#define MALI_BASE                       (g_mali_base)                         /* 0x13000000 */
#define MALI_GPU_ID                     (g_mali_base + 0x000)                 /* 0x13000000 */
#define MALI_GPU_IRQ_CLEAR              (g_mali_base + 0x024)                 /* 0x13000024 */
#define MALI_GPU_IRQ_MASK               (g_mali_base + 0x028)                 /* 0x13000028 */
#define MALI_GPU_IRQ_STATUS             (g_mali_base + 0x02C)                 /* 0x1300002C */
#define MALI_PWR_KEY                    (g_mali_base + 0x050)                 /* 0x13000050 */
#define MALI_PWR_OVERRIDE0              (g_mali_base + 0x054)                 /* 0x13000054 */
#define MALI_PWR_OVERRIDE1              (g_mali_base + 0x058)                 /* 0x13000058 */
#define MALI_SHADER_READY_LO            (g_mali_base + 0x140)                 /* 0x13000140 */
#define MALI_TILER_READY_LO             (g_mali_base + 0x150)                 /* 0x13000150 */
#define MALI_L2_READY_LO                (g_mali_base + 0x160)                 /* 0x13000160 */
#define MALI_SHADER_PWRON_LO            (g_mali_base + 0x180)                 /* 0x13000180 */
#define MALI_TILER_PWRON_LO             (g_mali_base + 0x190)                 /* 0x13000190 */
#define MALI_L2_PWRON_LO                (g_mali_base + 0x1A0)                 /* 0x130001A0 */
#define MALI_L2_PWRON_HI                (g_mali_base + 0x1A4)                 /* 0x130001A4 */
#define MALI_SHADER_PWROFF_LO           (g_mali_base + 0x1C0)                 /* 0x130001C0 */
#define MALI_TILER_PWROFF_LO            (g_mali_base + 0x1D0)                 /* 0x130001D0 */
#define MALI_L2_PWROFF_LO               (g_mali_base + 0x1E0)                 /* 0x130001E0 */
#define MALI_L2_PWROFF_HI               (g_mali_base + 0x1E4)                 /* 0x130001E4 */

#define MFG_TOP_CFG_BASE                (g_mfg_top_base)                      /* 0x13FBF000 */
#define MFG_CG_CON                      (g_mfg_top_base + 0x000)              /* 0x13FBF000 */
#define MFG_CG_CLR                      (g_mfg_top_base + 0x008)              /* 0x13FBF008 */
#define MFG_DCM_CON_0                   (g_mfg_top_base + 0x010)              /* 0x13FBF010 */
#define MFG_ASYNC_CON                   (g_mfg_top_base + 0x020)              /* 0x13FBF020 */
#define MFG_ASYNC_CON3                  (g_mfg_top_base + 0x02C)              /* 0x13FBF02C */
#define MFG_SRAM_FUL_SEL_ULV            (g_mfg_top_base + 0x080)              /* 0x13FBF080 */
#define MFG_GLOBAL_CON                  (g_mfg_top_base + 0x0B0)              /* 0x13FBF0B0 */
#define MFG_QCHANNEL_CON                (g_mfg_top_base + 0x0B4)              /* 0x13FBF0B4 */
#define MFG_TIMESTAMP                   (g_mfg_top_base + 0x130)              /* 0x13FBF130 */
#define MFG_AXCOHERENCE_CON             (g_mfg_top_base + 0x168)              /* 0x13FBF168 */
#define MFG_DEBUG_SEL                   (g_mfg_top_base + 0x170)              /* 0x13FBF170 */
#define MFG_DEBUG_TOP                   (g_mfg_top_base + 0x178)              /* 0x13FBF178 */
#define MFG_PDCA_BACKDOOR               (g_mfg_top_base + 0x210)              /* 0x13FBF210 */
#define MFG_ACTIVE_POWER_CON_CG         (g_mfg_top_base + 0x100)              /* 0x13FBF100 */
#define MFG_ACTIVE_POWER_CON_ST0        (g_mfg_top_base + 0x120)              /* 0x13FBF120 */
#define MFG_ACTIVE_POWER_CON_ST2        (g_mfg_top_base + 0x118)              /* 0x13FBF118 */
#define MFG_ACTIVE_POWER_CON_00         (g_mfg_top_base + 0x400)              /* 0x13FBF400 */
#define MFG_ACTIVE_POWER_CON_01         (g_mfg_top_base + 0x404)              /* 0x13FBF404 */
#define MFG_ACTIVE_POWER_CON_02         (g_mfg_top_base + 0x408)              /* 0x13FBF408 */
#define MFG_ACTIVE_POWER_CON_03         (g_mfg_top_base + 0x40C)              /* 0x13FBF40C */
#define MFG_ACTIVE_POWER_CON_04         (g_mfg_top_base + 0x410)              /* 0x13FBF410 */
#define MFG_ACTIVE_POWER_CON_05         (g_mfg_top_base + 0x414)              /* 0x13FBF414 */
#define MFG_ACTIVE_POWER_CON_06         (g_mfg_top_base + 0x418)              /* 0x13FBF418 */
#define MFG_ACTIVE_POWER_CON_07         (g_mfg_top_base + 0x41C)              /* 0x13FBF41C */
#define MFG_ACTIVE_POWER_CON_08         (g_mfg_top_base + 0x420)              /* 0x13FBF420 */
#define MFG_ACTIVE_POWER_CON_10         (g_mfg_top_base + 0x428)              /* 0x13FBF428 */
#define MFG_ACTIVE_POWER_CON_11         (g_mfg_top_base + 0x42C)              /* 0x13FBF42C */
#define MFG_MALI_AXUSER_M0_CFG1         (g_mfg_top_base + 0x704)              /* 0x13FBF704 */
#define MFG_MALI_AXUSER_M0_CFG2         (g_mfg_top_base + 0x708)              /* 0x13FBF708 */
#define MFG_MALI_AXUSER_M0_CFG3         (g_mfg_top_base + 0x70C)              /* 0x13FBF70C */
#define MFG_MALI_AXUSER_SLC_CFG10       (g_mfg_top_base + 0x728)              /* 0x13FBF728 */
#define MFG_MALI_AXUSER_SLC_CFG11       (g_mfg_top_base + 0x72C)              /* 0x13FBF72C */
#define MFG_MALI_AXUSER_SLC_CFG12       (g_mfg_top_base + 0x730)              /* 0x13FBF730 */
#define MFG_MALI_AXUSER_SLC_CFG13       (g_mfg_top_base + 0x734)              /* 0x13FBF734 */
#define MFG_MALI_AXUSER_SLC_CFG14       (g_mfg_top_base + 0x738)              /* 0x13FBF738 */
#define MFG_MALI_AXUSER_SLC_CFG15       (g_mfg_top_base + 0x73C)              /* 0x13FBF73C */
#define MFG_MALI_AXUSER_SLC_CFG16       (g_mfg_top_base + 0x740)              /* 0x13FBF740 */
#define MFG_MALI_AXUSER_SLC_CFG17       (g_mfg_top_base + 0x744)              /* 0x13FBF744 */
#define MFG_MALI_AXUSER_SLC_CFG18       (g_mfg_top_base + 0x748)              /* 0x13FBF748 */
#define MFG_MALI_AXUSER_SLC_CFG19       (g_mfg_top_base + 0x74C)              /* 0x13FBF74C */
#define MFG_MALI_AXUSER_SLC_CFG20       (g_mfg_top_base + 0x750)              /* 0x13FBF750 */
#define MFG_MALI_AXUSER_SLC_CFG21       (g_mfg_top_base + 0x754)              /* 0x13FBF754 */
#define MFG_MALI_AXUSER_SLC_CFG22       (g_mfg_top_base + 0x758)              /* 0x13FBF758 */
#define MFG_MALI_AXUSER_SLC_CFG23       (g_mfg_top_base + 0x75C)              /* 0x13FBF75C */
#define MFG_MALI_AXUSER_SLC_CFG24       (g_mfg_top_base + 0x760)              /* 0x13FBF760 */
#define MFG_MALI_AXUSER_SLC_CFG25       (g_mfg_top_base + 0x764)              /* 0x13FBF764 */
#define MFG_MALI_AXUSER_SLC_CFG26       (g_mfg_top_base + 0x768)              /* 0x13FBF768 */
#define MFG_MALI_AXUSER_SLC_CFG27       (g_mfg_top_base + 0x76C)              /* 0x13FBF76C */
#define MFG_MERGE_R_CON_00              (g_mfg_top_base + 0x8A0)              /* 0x13FBF8A0 */
#define MFG_MERGE_R_CON_01              (g_mfg_top_base + 0x8A4)              /* 0x13FBF8A4 */
#define MFG_MERGE_R_CON_02              (g_mfg_top_base + 0x8A8)              /* 0x13FBF8A8 */
#define MFG_MERGE_R_CON_03              (g_mfg_top_base + 0x8AC)              /* 0x13FBF8AC */
#define MFG_MERGE_W_CON_00              (g_mfg_top_base + 0x8B0)              /* 0x13FBF8B0 */
#define MFG_MERGE_W_CON_01              (g_mfg_top_base + 0x8B4)              /* 0x13FBF8B4 */
#define MFG_MERGE_W_CON_02              (g_mfg_top_base + 0x8B8)              /* 0x13FBF8B8 */
#define MFG_MERGE_W_CON_03              (g_mfg_top_base + 0x8BC)              /* 0x13FBF8BC */
#define MFG_DEBUGMON_CON_00             (g_mfg_top_base + 0x8F8)              /* 0x13FBF8F8 */
#define MFG_1TO2AXI_CON_00              (g_mfg_top_base + 0x8E0)              /* 0x13FBF8E0 */
#define MFG_1TO2AXI_CON_02              (g_mfg_top_base + 0x8E8)              /* 0x13FBF8E8 */
#define MFG_1TO2AXI_CON_04              (g_mfg_top_base + 0x910)              /* 0x13FBF910 */
#define MFG_1TO2AXI_CON_06              (g_mfg_top_base + 0x918)              /* 0x13FBF918 */
#define MFG_OUT_1TO2AXI_CON_00          (g_mfg_top_base + 0x900)              /* 0x13FBF900 */
#define MFG_OUT_1TO2AXI_CON_02          (g_mfg_top_base + 0x908)              /* 0x13FBF908 */
#define MFG_OUT_1TO2AXI_CON_04          (g_mfg_top_base + 0x920)              /* 0x13FBF920 */
#define MFG_OUT_1TO2AXI_CON_06          (g_mfg_top_base + 0x928)              /* 0x13FBF928 */
#define MFG_DFD_CON_0                   (g_mfg_top_base + 0xA00)              /* 0x13FBFA00 */
#define MFG_DFD_CON_1                   (g_mfg_top_base + 0xA04)              /* 0x13FBFA04 */
#define MFG_DFD_CON_3                   (g_mfg_top_base + 0xA0C)              /* 0x13FBFA0C */
#define MFG_DFD_CON_4                   (g_mfg_top_base + 0xA10)              /* 0x13FBFA10 */
#define MFG_DFD_CON_17                  (g_mfg_top_base + 0xA44)              /* 0x13FBFA44 */
#define MFG_DFD_CON_18                  (g_mfg_top_base + 0xA48)              /* 0x13FBFA48 */
#define MFG_DFD_CON_19                  (g_mfg_top_base + 0xA4C)              /* 0x13FBFA4C */
#define MFG_I2M_PROTECTOR_CFG_00        (g_mfg_top_base + 0xF60)              /* 0x13FBFF60 */
#define MFG_I2M_PROTECTOR_CFG_01        (g_mfg_top_base + 0xF64)              /* 0x13FBFF64 */
#define MFG_I2M_PROTECTOR_CFG_02        (g_mfg_top_base + 0xF68)              /* 0x13FBFF68 */
#define MFG_I2M_PROTECTOR_CFG_03        (g_mfg_top_base + 0xFA8)              /* 0x13FBFFA8 */
#define MFG_POWER_TRACKER_SETTING       (g_mfg_top_base + 0xFE0)              /* 0x13FBFFE0 */
#define MFG_POWER_TRACKER_PDC_STATUS0   (g_mfg_top_base + 0xFE4)              /* 0x13FBFFE4 */
#define MFG_POWER_TRACKER_PDC_STATUS1   (g_mfg_top_base + 0xFE8)              /* 0x13FBFFE8 */

#define MFG_PLL_BASE                    (g_mfg_pll_base)                      /* 0x13FA0000 */
#define MFG_PLL_CON0                    (g_mfg_pll_base + 0x008)              /* 0x13FA0008 */
#define MFG_PLL_CON1                    (g_mfg_pll_base + 0x00C)              /* 0x13FA000C */
#define MFG_PLL_FQMTR_CON0              (g_mfg_pll_base + 0x040)              /* 0x13FA0040 */
#define MFG_PLL_FQMTR_CON1              (g_mfg_pll_base + 0x044)              /* 0x13FA0044 */

#define MFGSC_PLL_BASE                  (g_mfgsc_pll_base)                    /* 0x13FA0C00 */
#define MFGSC_PLL_CON0                  (g_mfgsc_pll_base + 0x008)            /* 0x13FA0C08 */
#define MFGSC_PLL_CON1                  (g_mfgsc_pll_base + 0x00C)            /* 0x13FA0C0C */
#define MFGSC_PLL_FQMTR_CON0            (g_mfgsc_pll_base + 0x040)            /* 0x13FA0C40 */
#define MFGSC_PLL_FQMTR_CON1            (g_mfgsc_pll_base + 0x044)            /* 0x13FA0C44 */

#define MFG_RPC_BASE                    (g_mfg_rpc_base)                      /* 0x13F90000 */
#define MFG_RPC_AO_CLK_CFG              (g_mfg_rpc_base + 0x1034)             /* 0x13F91034 */
#define MFG_RPC_SLP_PROT_EN_SET         (g_mfg_rpc_base + 0x1040)             /* 0x13F91040 */
#define MFG_RPC_SLP_PROT_EN_CLR         (g_mfg_rpc_base + 0x1044)             /* 0x13F91044 */
#define MFG_RPC_SLP_PROT_EN_STA         (g_mfg_rpc_base + 0x1048)             /* 0x13F91048 */
#define MFG_RPC_MFG1_PWR_CON            (g_mfg_rpc_base + 0x1070)             /* 0x13F91070 */
#define MFG_RPC_MFG2_PWR_CON            (g_mfg_rpc_base + 0x10A0)             /* 0x13F910A0 */
#define MFG_RPC_MFG3_PWR_CON            (g_mfg_rpc_base + 0x10A4)             /* 0x13F910A4 */
#define MFG_RPC_MFG4_PWR_CON            (g_mfg_rpc_base + 0x10A8)             /* 0x13F910A8 */
#define MFG_RPC_MFG5_PWR_CON            (g_mfg_rpc_base + 0x10AC)             /* 0x13F910AC */
#define MFG_RPC_MFG6_PWR_CON            (g_mfg_rpc_base + 0x10B0)             /* 0x13F910B0 */
#define MFG_RPC_MFG7_PWR_CON            (g_mfg_rpc_base + 0x10B4)             /* 0x13F910B4 */
#define MFG_RPC_MFG9_PWR_CON            (g_mfg_rpc_base + 0x10BC)             /* 0x13F910BC */
#define MFG_RPC_MFG10_PWR_CON           (g_mfg_rpc_base + 0x10C0)             /* 0x13F910C0 */
#define MFG_RPC_MFG11_PWR_CON           (g_mfg_rpc_base + 0x10C4)             /* 0x13F910C4 */
#define MFG_RPC_MFG12_PWR_CON           (g_mfg_rpc_base + 0x10C8)             /* 0x13F910C8 */
#define MFG_RPC_MFG13_PWR_CON           (g_mfg_rpc_base + 0x10CC)             /* 0x13F910CC */
#define MFG_RPC_MFG14_PWR_CON           (g_mfg_rpc_base + 0x10D0)             /* 0x13F910D0 */
#define MFG_RPC_IPS_SES_PWR_CON         (g_mfg_rpc_base + 0x1300)             /* 0x13F91300 */
#define MFG_RPC_PWR_CON_STATUS          (g_mfg_rpc_base + 0x1200)             /* 0x13F91200 */
#define MFG_RPC_PWR_CON_2ND_STATUS      (g_mfg_rpc_base + 0x1204)             /* 0x13F91204 */

#define SPM_BASE                        (g_sleep)                             /* 0x1C001000 */
#define SPM_SPM2GPUPM_CON               (g_sleep + 0x410)                     /* 0x1C001410 */
#define SPM_SRC_REQ                     (g_sleep + 0x818)                     /* 0x1C001818 */
#define SPM_MFG0_PWR_CON                (g_sleep + 0xEB4)                     /* 0x1C001EB4 */
#define SPM_XPU_PWR_STATUS              (g_sleep + 0xF50)                     /* 0x1C001F50 */
#define SPM_XPU_PWR_STATUS_2ND          (g_sleep + 0xF54)                     /* 0x1C001F54 */
#define SPM_SEMA_M3                     (g_sleep + 0x6A8)                     /* 0x1C0016A8 */
#define SPM_SEMA_M4                     (g_sleep + 0x6AC)                     /* 0x1C0016AC */
#define SPM_SOC_BUCK_ISO_CON            (g_sleep + 0xF28)                     /* 0x1C001F28 */
#define SPM_SOC_BUCK_ISO_CON_SET        (g_sleep + 0xF2C)                     /* 0x1C001F2C */
#define SPM_SOC_BUCK_ISO_CON_CLR        (g_sleep + 0xF30)                     /* 0x1C001F30 */

#define TOPCKGEN_BASE                   (g_topckgen_base)                     /* 0x10000000 */
#define TOPCK_CLK_CFG_18                (g_topckgen_base + 0x190)             /* 0x10000190 */
#define TOPCK_CLK_CFG_20                (g_topckgen_base + 0x120)             /* 0x10000120 */

#define NTH_EMICFG_BASE                 (g_nth_emicfg_base)                   /* 0x1021C000 */
#define NTH_APU_ACP_GALS_SLV_CTRL       (g_nth_emicfg_base + 0x600)           /* 0x1021C600 */
#define NTH_APU_EMI1_GALS_SLV_CTRL      (g_nth_emicfg_base + 0x624)           /* 0x1021C624 */
#define NTH_APU_EMI1_GALS_SLV_DBG       (g_nth_emicfg_base + 0x824)           /* 0x1021C824 */
#define NTH_APU_EMI0_GALS_SLV_DBG       (g_nth_emicfg_base + 0x828)           /* 0x1021C828 */
#define NTH_MFG_EMI1_GALS_SLV_DBG       (g_nth_emicfg_base + 0x82C)           /* 0x1021C82C */
#define NTH_MFG_EMI0_GALS_SLV_DBG       (g_nth_emicfg_base + 0x830)           /* 0x1021C830 */

#define NTH_EMICFG_AO_MEM_BASE          (g_nth_emicfg_ao_mem_base)            /* 0x10270000 */
#define NTH_SLEEP_PROT_MASK             (g_nth_emicfg_ao_mem_base + 0x000)    /* 0x10270000 */
#define NTH_GLITCH_PROT_RDY             (g_nth_emicfg_ao_mem_base + 0x08C)    /* 0x1027008C */
#define NTH_M6M7_IDLE_BIT_EN_1          (g_nth_emicfg_ao_mem_base + 0x228)    /* 0x10270228 */
#define NTH_M6M7_IDLE_BIT_EN_0          (g_nth_emicfg_ao_mem_base + 0x22C)    /* 0x1027022C */

#define INFRACFG_AO_BASE                (g_infracfg_ao_base)                  /* 0x10001000 */
#define EMISYS_PROTECT_EN_STA_0         (g_infracfg_ao_base + 0xC60)          /* 0x10001C60 */
#define EMISYS_PROTECT_EN_SET_0         (g_infracfg_ao_base + 0xC64)          /* 0x10001C64 */
#define EMISYS_PROTECT_EN_CLR_0         (g_infracfg_ao_base + 0xC68)          /* 0x10001C68 */
#define EMISYS_PROTECT_RDY_STA_0        (g_infracfg_ao_base + 0xC6C)          /* 0x10001C6C */
#define EMISYS_PROTECT_EN_STA_1         (g_infracfg_ao_base + 0xC70)          /* 0x10001C70 */
#define EMISYS_PROTECT_EN_SET_1         (g_infracfg_ao_base + 0xC74)          /* 0x10001C74 */
#define EMISYS_PROTECT_EN_CLR_1         (g_infracfg_ao_base + 0xC78)          /* 0x10001C78 */
#define EMISYS_PROTECT_RDY_STA_1        (g_infracfg_ao_base + 0xC7C)          /* 0x10001C7C */
#define MD_MFGSYS_PROTECT_EN_STA_0      (g_infracfg_ao_base + 0xCA0)          /* 0x10001CA0 */
#define MD_MFGSYS_PROTECT_EN_SET_0      (g_infracfg_ao_base + 0xCA4)          /* 0x10001CA4 */
#define MD_MFGSYS_PROTECT_EN_CLR_0      (g_infracfg_ao_base + 0xCA8)          /* 0x10001CA8 */
#define MD_MFGSYS_PROTECT_RDY_STA_0     (g_infracfg_ao_base + 0xCAC)          /* 0x10001CAC */

#define INFRA_AO_DEBUG_CTRL_BASE        (g_infra_ao_debug_ctrl)               /* 0x10023000 */
#define INFRA_AO_BUS0_U_DEBUG_CTRL0     (g_infra_ao_debug_ctrl + 0x000)       /* 0x10023000 */

#define INFRA_AO1_DEBUG_CTRL_BASE       (g_infra_ao1_debug_ctrl)              /* 0x1002B000 */
#define INFRA_AO1_BUS1_U_DEBUG_CTRL0    (g_infra_ao1_debug_ctrl + 0x000)      /* 0x1002B000 */

#define NTH_EMI_AO_DEBUG_CTRL_BASE      (g_nth_emi_ao_debug_ctrl)             /* 0x10042000 */
#define NTH_EMI_AO_DEBUG_CTRL0          (g_nth_emi_ao_debug_ctrl + 0x000)     /* 0x10042000 */

#define NEMI_MI32_SMI_SUB_BASE          (g_nemi_mi32_smi_sub)                 /* 0x1025E000 */
#define NEMI_MI32_SMI_SUB_DEBUG_S0      (NEMI_MI32_SMI_SUB_BASE + 0x400)      /* 0x1025E400 */
#define NEMI_MI32_SMI_SUB_DEBUG_S1      (NEMI_MI32_SMI_SUB_BASE + 0x404)      /* 0x1025E404 */
#define NEMI_MI32_SMI_SUB_DEBUG_S2      (NEMI_MI32_SMI_SUB_BASE + 0x408)      /* 0x1025E408 */
#define NEMI_MI32_SMI_SUB_DEBUG_M0      (NEMI_MI32_SMI_SUB_BASE + 0x430)      /* 0x1025E430 */

#define NEMI_MI33_SMI_SUB_BASE          (g_nemi_mi33_smi_sub)                 /* 0x1025F000 */
#define NEMI_MI33_SMI_SUB_DEBUG_S0      (NEMI_MI33_SMI_SUB_BASE + 0x400)      /* 0x1025F400 */
#define NEMI_MI33_SMI_SUB_DEBUG_S1      (NEMI_MI33_SMI_SUB_BASE + 0x404)      /* 0x1025F404 */
#define NEMI_MI33_SMI_SUB_DEBUG_M0      (NEMI_MI33_SMI_SUB_BASE + 0x430)      /* 0x1025F430 */

#endif /* __GPUFREQ_REG_MT6878_H__ */
