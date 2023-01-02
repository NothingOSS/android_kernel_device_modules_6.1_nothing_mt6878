/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2021 MediaTek Inc.
 */

#ifndef __GPUFREQ_REG_MT6985_H__
#define __GPUFREQ_REG_MT6985_H__

#include <linux/io.h>
#include <linux/bits.h>

/**************************************************
 * GPUFREQ Register Operation
 **************************************************/
/* HW limit: MFG_TOP_CONFIG need to be continuously read twice */
#define readl_mfg readl_mfg
static inline u32 readl_mfg(const void __iomem *addr)
{
	int i = 0;
	u32 val[2] = {0, 0};

	do {
		if (likely(i < 100)) {
			val[i % 2] = readl(addr);
			val[++i % 2] = readl(addr);
		} else
			__gpufreq_abort("read MFG_TOP_CFG (0x%08x)=(0x%08x, 0x%08x) timeout",
				addr, val[0], val[1]);
	} while (val[0] != val[1]);

	return val[0];
}

/**************************************************
 * GPUFREQ Register Definition
 **************************************************/
#define MALI_BASE                       (g_mali_base)                         /* 0x13000000 */
#define MALI_GPU_ID                     (MALI_BASE + 0x000)                   /* 0x13000000 */

#define MFG_TOP_CFG_BASE                (g_mfg_top_base)                      /* 0x13FBF000 */
#define MFG_CG_CON                      (MFG_TOP_CFG_BASE + 0x000)            /* 0x13FBF000 */
#define MFG_DCM_CON_0                   (MFG_TOP_CFG_BASE + 0x010)            /* 0x13FBF010 */
#define MFG_ASYNC_CON                   (MFG_TOP_CFG_BASE + 0x020)            /* 0x13FBF020 */
#define MFG_GLOBAL_CON                  (MFG_TOP_CFG_BASE + 0x0B0)            /* 0x13FBF0B0 */
#define MFG_AXCOHERENCE_CON             (MFG_TOP_CFG_BASE + 0x168)            /* 0x13FBF168 */
#define MFG_1TO2AXI_CON_00              (MFG_TOP_CFG_BASE + 0x8E0)            /* 0x13FBF8E0 */
#define MFG_1TO2AXI_CON_02              (MFG_TOP_CFG_BASE + 0x8E8)            /* 0x13FBF8E8 */
#define MFG_1TO2AXI_CON_04              (MFG_TOP_CFG_BASE + 0x910)            /* 0x13FBF910 */
#define MFG_1TO2AXI_CON_06              (MFG_TOP_CFG_BASE + 0x918)            /* 0x13FBF918 */
#define MFG_OUT_1TO2AXI_CON_00          (MFG_TOP_CFG_BASE + 0x900)            /* 0x13FBF900 */
#define MFG_OUT_1TO2AXI_CON_02          (MFG_TOP_CFG_BASE + 0x908)            /* 0x13FBF908 */
#define MFG_OUT_1TO2AXI_CON_04          (MFG_TOP_CFG_BASE + 0x920)            /* 0x13FBF920 */
#define MFG_OUT_1TO2AXI_CON_06          (MFG_TOP_CFG_BASE + 0x928)            /* 0x13FBF928 */
#define MFG_ACTIVE_POWER_CON_CG         (MFG_TOP_CFG_BASE + 0x100)            /* 0x13FBF100 */
#define MFG_ACTIVE_POWER_CON_ST0        (MFG_TOP_CFG_BASE + 0x120)            /* 0x13FBF120 */
#define MFG_ACTIVE_POWER_CON_ST1        (MFG_TOP_CFG_BASE + 0x140)            /* 0x13FBF140 */
#define MFG_ACTIVE_POWER_CON_ST2        (MFG_TOP_CFG_BASE + 0x118)            /* 0x13FBF118 */
#define MFG_ACTIVE_POWER_CON_ST4        (MFG_TOP_CFG_BASE + 0x0C0)            /* 0x13FBF0C0 */
#define MFG_ACTIVE_POWER_CON_ST5        (MFG_TOP_CFG_BASE + 0x098)            /* 0x13FBF098 */
#define MFG_ACTIVE_POWER_CON_ST6        (MFG_TOP_CFG_BASE + 0x1C0)            /* 0x13FBF1C0 */
#define MFG_ACTIVE_POWER_CON_00         (MFG_TOP_CFG_BASE + 0x400)            /* 0x13FBF400 */
#define MFG_ACTIVE_POWER_CON_01         (MFG_TOP_CFG_BASE + 0x404)            /* 0x13FBF404 */
#define MFG_ACTIVE_POWER_CON_06         (MFG_TOP_CFG_BASE + 0x418)            /* 0x13FBF418 */
#define MFG_ACTIVE_POWER_CON_07         (MFG_TOP_CFG_BASE + 0x41C)            /* 0x13FBF41C */
#define MFG_ACTIVE_POWER_CON_12         (MFG_TOP_CFG_BASE + 0x430)            /* 0x13FBF430 */
#define MFG_ACTIVE_POWER_CON_13         (MFG_TOP_CFG_BASE + 0x434)            /* 0x13FBF434 */
#define MFG_ACTIVE_POWER_CON_18         (MFG_TOP_CFG_BASE + 0x448)            /* 0x13FBF448 */
#define MFG_ACTIVE_POWER_CON_19         (MFG_TOP_CFG_BASE + 0x44C)            /* 0x13FBF44C */
#define MFG_ACTIVE_POWER_CON_24         (MFG_TOP_CFG_BASE + 0x460)            /* 0x13FBF460 */
#define MFG_ACTIVE_POWER_CON_25         (MFG_TOP_CFG_BASE + 0x464)            /* 0x13FBF464 */
#define MFG_ACTIVE_POWER_CON_30         (MFG_TOP_CFG_BASE + 0x478)            /* 0x13FBF478 */
#define MFG_ACTIVE_POWER_CON_31         (MFG_TOP_CFG_BASE + 0x47C)            /* 0x13FBF47C */
#define MFG_ACTIVE_POWER_CON_36         (MFG_TOP_CFG_BASE + 0x490)            /* 0x13FBF490 */
#define MFG_ACTIVE_POWER_CON_37         (MFG_TOP_CFG_BASE + 0x494)            /* 0x13FBF494 */
#define MFG_ACTIVE_POWER_CON_42         (MFG_TOP_CFG_BASE + 0x4A8)            /* 0x13FBF4A8 */
#define MFG_ACTIVE_POWER_CON_43         (MFG_TOP_CFG_BASE + 0x4AC)            /* 0x13FBF4AC */
#define MFG_ACTIVE_POWER_CON_48         (MFG_TOP_CFG_BASE + 0x4C0)            /* 0x13FBF4C0 */
#define MFG_ACTIVE_POWER_CON_49         (MFG_TOP_CFG_BASE + 0x4C4)            /* 0x13FBF4C4 */
#define MFG_ACTIVE_POWER_CON_54         (MFG_TOP_CFG_BASE + 0x4D8)            /* 0x13FBF4D8 */
#define MFG_ACTIVE_POWER_CON_55         (MFG_TOP_CFG_BASE + 0x4DC)            /* 0x13FBF4DC */
#define MFG_ACTIVE_POWER_CON_60         (MFG_TOP_CFG_BASE + 0x4F0)            /* 0x13FBF4F0 */
#define MFG_ACTIVE_POWER_CON_61         (MFG_TOP_CFG_BASE + 0x4F4)            /* 0x13FBF4F4 */
#define MFG_SENSOR_BCLK_CG              (MFG_TOP_CFG_BASE + 0xF98)            /* 0x13FBFF98 */
#define MFG_I2M_PROTECTOR_CFG_00        (MFG_TOP_CFG_BASE + 0xF60)            /* 0x13FBFF60 */
#define MFG_I2M_PROTECTOR_CFG_01        (MFG_TOP_CFG_BASE + 0xF64)            /* 0x13FBFF64 */
#define MFG_I2M_PROTECTOR_CFG_02        (MFG_TOP_CFG_BASE + 0xF68)            /* 0x13FBFF68 */
#define MFG_I2M_PROTECTOR_CFG_03        (MFG_TOP_CFG_BASE + 0xFA8)            /* 0x13FBFFA8 */
#define MFG_DUMMY_REG                   (MFG_TOP_CFG_BASE + 0x500)            /* 0x13FBF500 */
#define MFG_SRAM_FUL_SEL_ULV            (MFG_TOP_CFG_BASE + 0x080)            /* 0x13FBF080 */
#define MFG_QCHANNEL_CON                (MFG_TOP_CFG_BASE + 0x0B4)            /* 0x13FBF0B4 */
#define MFG_DEBUG_SEL                   (MFG_TOP_CFG_BASE + 0x170)            /* 0x13FBF170 */
#define MFG_DEBUG_TOP                   (MFG_TOP_CFG_BASE + 0x178)            /* 0x13FBF178 */
#define MFG_TIMESTAMP                   (MFG_TOP_CFG_BASE + 0x130)            /* 0x13FBF130 */
#define MFG_DEBUGMON_CON_00             (MFG_TOP_CFG_BASE + 0x8F8)            /* 0x13FBF8F8 */
#define MFG_DFD_CON_0                   (MFG_TOP_CFG_BASE + 0xA00)            /* 0x13FBFA00 */
#define MFG_DFD_CON_1                   (MFG_TOP_CFG_BASE + 0xA04)            /* 0x13FBFA04 */
#define MFG_DFD_CON_2                   (MFG_TOP_CFG_BASE + 0xA08)            /* 0x13FBFA08 */
#define MFG_DFD_CON_3                   (MFG_TOP_CFG_BASE + 0xA0C)            /* 0x13FBFA0C */
#define MFG_DFD_CON_4                   (MFG_TOP_CFG_BASE + 0xA10)            /* 0x13FBFA10 */
#define MFG_DFD_CON_5                   (MFG_TOP_CFG_BASE + 0xA14)            /* 0x13FBFA14 */
#define MFG_DFD_CON_6                   (MFG_TOP_CFG_BASE + 0xA18)            /* 0x13FBFA18 */
#define MFG_DFD_CON_7                   (MFG_TOP_CFG_BASE + 0xA1C)            /* 0x13FBFA1C */
#define MFG_DFD_CON_8                   (MFG_TOP_CFG_BASE + 0xA20)            /* 0x13FBFA20 */
#define MFG_DFD_CON_9                   (MFG_TOP_CFG_BASE + 0xA24)            /* 0x13FBFA24 */
#define MFG_DFD_CON_10                  (MFG_TOP_CFG_BASE + 0xA28)            /* 0x13FBFA28 */
#define MFG_DFD_CON_11                  (MFG_TOP_CFG_BASE + 0xA2C)            /* 0x13FBFA2C */

#define MFG_PLL_BASE                    (g_mfg_pll_base)                      /* 0x13FA0000 */
#define MFG_PLL_CON0                    (MFG_PLL_BASE + 0x008)                /* 0x13FA0008 */
#define MFG_PLL_CON1                    (MFG_PLL_BASE + 0x00C)                /* 0x13FA000C */
#define MFG_PLL_FQMTR_CON0              (MFG_PLL_BASE + 0x040)                /* 0x13FA0040 */
#define MFG_PLL_FQMTR_CON1              (MFG_PLL_BASE + 0x044)                /* 0x13FA0044 */

#define MFGSC_PLL_BASE                  (g_mfgsc_pll_base)                    /* 0x13FA0C00 */
#define MFGSC_PLL_CON0                  (MFGSC_PLL_BASE + 0x008)              /* 0x13FA0C08 */
#define MFGSC_PLL_CON1                  (MFGSC_PLL_BASE + 0x00C)              /* 0x13FA0C0C */
#define MFGSC_PLL_FQMTR_CON0            (MFGSC_PLL_BASE + 0x040)              /* 0x13FA0C40 */
#define MFGSC_PLL_FQMTR_CON1            (MFGSC_PLL_BASE + 0x044)              /* 0x13FA0C44 */

#define MFG_RPC_BASE                    (g_mfg_rpc_base)                      /* 0x13F90000 */
#define MFG_RPC_AO_CLK_CFG              (MFG_RPC_BASE + 0x1034)               /* 0x13F91034 */
#define MFG_RPC_MFG1_PWR_CON            (MFG_RPC_BASE + 0x1070)               /* 0x13F91070 */
#define MFG_RPC_MFG2_PWR_CON            (MFG_RPC_BASE + 0x10A0)               /* 0x13F910A0 */
#define MFG_RPC_MFG3_PWR_CON            (MFG_RPC_BASE + 0x10A4)               /* 0x13F910A4 */
#define MFG_RPC_MFG4_PWR_CON            (MFG_RPC_BASE + 0x10A8)               /* 0x13F910A8 */
#define MFG_RPC_MFG5_PWR_CON            (MFG_RPC_BASE + 0x10AC)               /* 0x13F910AC */
#define MFG_RPC_MFG6_PWR_CON            (MFG_RPC_BASE + 0x10B0)               /* 0x13F910B0 */
#define MFG_RPC_MFG7_PWR_CON            (MFG_RPC_BASE + 0x10B4)               /* 0x13F910B4 */
#define MFG_RPC_MFG8_PWR_CON            (MFG_RPC_BASE + 0x10B8)               /* 0x13F910B8 */
#define MFG_RPC_MFG9_PWR_CON            (MFG_RPC_BASE + 0x10BC)               /* 0x13F910BC */
#define MFG_RPC_MFG10_PWR_CON           (MFG_RPC_BASE + 0x10C0)               /* 0x13F910C0 */
#define MFG_RPC_MFG11_PWR_CON           (MFG_RPC_BASE + 0x10C4)               /* 0x13F910C4 */
#define MFG_RPC_MFG12_PWR_CON           (MFG_RPC_BASE + 0x10C8)               /* 0x13F910C8 */
#define MFG_RPC_MFG13_PWR_CON           (MFG_RPC_BASE + 0x10CC)               /* 0x13F910CC */
#define MFG_RPC_MFG14_PWR_CON           (MFG_RPC_BASE + 0x10D0)               /* 0x13F910D0 */
#define MFG_RPC_MFG15_PWR_CON           (MFG_RPC_BASE + 0x10D4)               /* 0x13F910D4 */
#define MFG_RPC_MFG16_PWR_CON           (MFG_RPC_BASE + 0x10D8)               /* 0x13F910D8 */
#define MFG_RPC_MFG17_PWR_CON           (MFG_RPC_BASE + 0x10DC)               /* 0x13F910DC */
#define MFG_RPC_MFG18_PWR_CON           (MFG_RPC_BASE + 0x10E0)               /* 0x13F910E0 */
#define MFG_RPC_MFG19_PWR_CON           (MFG_RPC_BASE + 0x10E4)               /* 0x13F910E4 */
#define MFG_RPC_SLP_PROT_EN_STA         (MFG_RPC_BASE + 0x1048)               /* 0x13F91048 */
#define MFG_RPC_MFGIPS_PWR_CON          (MFG_RPC_BASE + 0x10FC)               /* 0x13F910FC */

#define SPM_BASE                        (g_sleep)                             /* 0x1C001000 */
#define SPM_SPM2GPUPM_CON               (SPM_BASE + 0x410)                    /* 0x1C001410 */
#define SPM_MFG0_PWR_CON                (SPM_BASE + 0xEE8)                    /* 0x1C001EE8 */
#define SPM_XPU_PWR_STATUS              (SPM_BASE + 0xF94)                    /* 0x1C001F94 */
#define SPM_XPU_PWR_STATUS_2ND          (SPM_BASE + 0xF98)                    /* 0x1C001F98 */
#define SPM_SEMA_M3                     (SPM_BASE + 0x6A8)                    /* 0x1C0016A8 */
#define SPM_SEMA_M4                     (SPM_BASE + 0x6AC)                    /* 0x1C0016AC */
#define SPM_SOC_BUCK_ISO_CON            (SPM_BASE + 0xF78)                    /* 0x1C001F78 */
#define SPM_SOC_BUCK_ISO_CON_SET        (SPM_BASE + 0xF7C)                    /* 0x1C001F7C */
#define SPM_SOC_BUCK_ISO_CON_CLR        (SPM_BASE + 0xF80)                    /* 0x1C001F80 */

#define TOPCKGEN_BASE                   (g_topckgen_base)                     /* 0x10000000 */
#define TOPCK_CLK_CFG_3                 (TOPCKGEN_BASE + 0x040)               /* 0x10000040 */
#define TOPCK_CLK_CFG_30                (TOPCKGEN_BASE + 0x1F0)               /* 0x100001F0 */

#define NTH_EMICFG_BASE                 (g_nth_emicfg_base)                   /* 0x1021C000 */
#define NTH_MFG_EMI1_GALS_SLV_DBG       (NTH_EMICFG_BASE + 0x82C)             /* 0x1021C82C */
#define NTH_MFG_EMI0_GALS_SLV_DBG       (NTH_EMICFG_BASE + 0x830)             /* 0x1021C830 */

#define STH_EMICFG_BASE                 (g_sth_emicfg_base)                   /* 0x1021E000 */
#define STH_MFG_EMI1_GALS_SLV_DBG       (STH_EMICFG_BASE + 0x82C)             /* 0x1021E82C */
#define STH_MFG_EMI0_GALS_SLV_DBG       (STH_EMICFG_BASE + 0x830)             /* 0x1021E830 */

#define NTH_EMICFG_AO_MEM_BASE          (g_nth_emicfg_ao_mem_base)            /* 0x10270000 */
#define NTH_M6M7_IDLE_BIT_EN_1          (NTH_EMICFG_AO_MEM_BASE + 0x228)      /* 0x10270228 */
#define NTH_M6M7_IDLE_BIT_EN_0          (NTH_EMICFG_AO_MEM_BASE + 0x22C)      /* 0x1027022C */

#define STH_EMICFG_AO_MEM_BASE          (g_sth_emicfg_ao_mem_base)            /* 0x1030E000 */
#define STH_M6M7_IDLE_BIT_EN_1          (STH_EMICFG_AO_MEM_BASE + 0x228)      /* 0x1030E228 */
#define STH_M6M7_IDLE_BIT_EN_0          (STH_EMICFG_AO_MEM_BASE + 0x22C)      /* 0x1030E22C */

#define IFRBUS_AO_BASE                  (g_ifrbus_ao_base)                    /* 0x1002C000 */
#define IFR_MFGSYS_PROT_EN_STA_0        (IFRBUS_AO_BASE + 0x1A0)              /* 0x1002C1A0 */
#define IFR_MFGSYS_PROT_EN_W1S_0        (IFRBUS_AO_BASE + 0x1A4)              /* 0x1002C1A4 */
#define IFR_MFGSYS_PROT_EN_W1C_0        (IFRBUS_AO_BASE + 0x1A8)              /* 0x1002C1A8 */
#define IFR_MFGSYS_PROT_RDY_STA_0       (IFRBUS_AO_BASE + 0x1AC)              /* 0x1002C1AC */

#define INFRA_AO_DEBUG_CTRL_BASE        (g_infra_ao_debug_ctrl)               /* 0x10023000 */
#define INFRA_AO_BUS0_U_DEBUG_CTRL0     (INFRA_AO_DEBUG_CTRL_BASE + 0x000)    /* 0x10023000 */

#define INFRA_AO1_DEBUG_CTRL_BASE       (g_infra_ao1_debug_ctrl)              /* 0x1002B000 */
#define INFRA_AO1_BUS1_U_DEBUG_CTRL0    (INFRA_AO1_DEBUG_CTRL_BASE + 0x000)   /* 0x1002B000 */

#define NTH_EMI_AO_DEBUG_CTRL_BASE      (g_nth_emi_ao_debug_ctrl)             /* 0x10042000 */
#define NTH_EMI_AO_DEBUG_CTRL0          (NTH_EMI_AO_DEBUG_CTRL_BASE + 0x000)  /* 0x10042000 */

#define STH_EMI_AO_DEBUG_CTRL_BASE      (g_sth_emi_ao_debug_ctrl)             /* 0x10028000 */
#define STH_EMI_AO_DEBUG_CTRL0          (STH_EMI_AO_DEBUG_CTRL_BASE + 0x000)  /* 0x10028000 */

#define EFUSE_BASE                      (g_efuse_base)                        /* 0x11E80000 */
#define EFUSE_ASENSOR_RT                (EFUSE_BASE + 0x5CC)                  /* 0x11E805CC */
#define EFUSE_ASENSOR_HT                (EFUSE_BASE + 0x5D0)                  /* 0x11E805D0 */
#define EFUSE_ASENSOR_TEMPER            (EFUSE_BASE + 0x5DC)                  /* 0x11E805DC */
#define EFUSE_PTPOD21_SN                (EFUSE_BASE + 0x5D4)                  /* 0x11E805D4 */
#define EFUSE_PTPOD22_AVS               (EFUSE_BASE + 0x5D8)                  /* 0x11E805D8 */
#define EFUSE_PTPOD23_AVS               (EFUSE_BASE + 0x5DC)                  /* 0x11E805DC */
#define EFUSE_PTPOD24_AVS               (EFUSE_BASE + 0x5E0)                  /* 0x11E805E0 */
#define EFUSE_PTPOD25_AVS               (EFUSE_BASE + 0x5E4)                  /* 0x11E805E4 */
#define EFUSE_PTPOD26_AVS               (EFUSE_BASE + 0x5E8)                  /* 0x11E805E8 */

#define MFG_CPE_CTRL_MCU_BASE           (g_mfg_cpe_ctrl_mcu_base)             /* 0x13FB9C00 */
#define MFG_CPE_CTRL_MCU_REG_CPEMONCTL  (MFG_CPE_CTRL_MCU_BASE + 0x000)       /* 0x13FB9C00 */
#define MFG_CPE_CTRL_MCU_REG_CEPEN      (MFG_CPE_CTRL_MCU_BASE + 0x004)       /* 0x13FB9C04 */
#define MFG_CPE_CTRL_MCU_REG_CPEIRQSTS  (MFG_CPE_CTRL_MCU_BASE + 0x010)       /* 0x13FB9C10 */
#define MFG_CPE_CTRL_MCU_REG_CPEINTSTS  (MFG_CPE_CTRL_MCU_BASE + 0x028)       /* 0x13FB9C28 */

#define MFG_CPE_SENSOR0_BASE            (g_mfg_cpe_sensor0_base)              /* 0x13FCF000 */
#define MFG_CPE_SENSOR_C0ASENSORDATA2   (MFG_CPE_SENSOR0_BASE + 0x008)        /* 0x13FCF008 */
#define MFG_CPE_SENSOR_C0ASENSORDATA3   (MFG_CPE_SENSOR0_BASE + 0x00C)        /* 0x13FCF00C */

#define MFG_SECURE_BASE                 (g_mfg_secure_base)                   /* 0x13FBC000 */
#define MFG_SECURE_REG                  (MFG_SECURE_BASE + 0xFE0)             /* 0x13FBCFE0 */

#define DRM_DEBUG_BASE                  (g_drm_debug_base)                    /* 0x1000D000 */
#define DRM_DEBUG_MFG_REG               (DRM_DEBUG_BASE + 0x060)              /* 0x1000D060 */

#endif /* __GPUFREQ_REG_MT6985_H__ */
