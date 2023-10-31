/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2023 MediaTek Inc.
 */

#ifndef __GPUFREQ_MT6878_H__
#define __GPUFREQ_MT6878_H__

/**************************************************
 * Clock Setting
 **************************************************/
#define POSDIV_2_MAX_FREQ                   (1900000)         /* KHz */
#define POSDIV_2_MIN_FREQ                   (750000)          /* KHz */
#define POSDIV_4_MAX_FREQ                   (950000)          /* KHz */
#define POSDIV_4_MIN_FREQ                   (375000)          /* KHz */
#define POSDIV_8_MAX_FREQ                   (475000)          /* KHz */
#define POSDIV_8_MIN_FREQ                   (187500)          /* KHz */
#define POSDIV_16_MAX_FREQ                  (237500)          /* KHz */
#define POSDIV_16_MIN_FREQ                  (125000)          /* KHz */
#define POSDIV_SHIFT                        (24)              /* bit */
#define DDS_SHIFT                           (14)              /* bit */
#define MFGPLL_FIN                          (26)              /* MHz */
#define MFG_SEL_MFGPLL_MASK                 (BIT(16))         /* [16] */
#define MFGSC_SEL_MFGSCPLL_MASK             (BIT(17))         /* [17] */
#define MFG_REF_SEL_MASK                    (GENMASK(9, 8))   /* [9:8] */
#define MFGSC_REF_SEL_MASK                  (GENMASK(17, 16)) /* [17:16] */
#define FREQ_ROUNDUP_TO_10(freq)            ((freq % 10) ? (freq - (freq % 10) + 10) : freq)

/**************************************************
 * MTCMOS Setting
 **************************************************/
#define GPUFREQ_CHECK_MFG_PWR_STATUS        (0)
#define MFG_0_1_PWR_MASK                    (GENMASK(1, 0))
#define MFG_0_10_PWR_MASK                   (GENMASK(10, 9) | BIT(5) | GENMASK(3, 0))
#define MFG_1_10_PWR_MASK                   (GENMASK(10, 9) | BIT(5) | GENMASK(3, 1))
#define MFG_0_1_PWR_STATUS \
	(((DRV_Reg32(SPM_XPU_PWR_STATUS) & BIT(1)) >> 1) | \
	(DRV_Reg32(MFG_RPC_PWR_CON_STATUS) & BIT(1)))
#define MFG_0_10_PWR_STATUS \
	(((DRV_Reg32(SPM_XPU_PWR_STATUS) & BIT(1)) >> 1) | \
	((DRV_Reg32(MFG_RPC_PWR_CON_STATUS) & MFG_1_10_PWR_MASK)))

/**************************************************
 * Shader Core Setting
 **************************************************/
#define MFG3_SHADER_STACK0                  (T0C0)          /* MFG9 */
#define MFG5_SHADER_STACK2                  (T2C0)          /* MFG10 */

#define GPU_SHADER_PRESENT_1 \
	(T0C0)
#define GPU_SHADER_PRESENT_2 \
	(MFG3_SHADER_STACK0 | MFG5_SHADER_STACK2)

#define SHADER_CORE_NUM                     (2)
struct gpufreq_core_mask_info g_core_mask_table[] = {
	{2, GPU_SHADER_PRESENT_2},
	{1, GPU_SHADER_PRESENT_1},
};

/**************************************************
 * Dynamic Power Setting
 **************************************************/
#define GPU_DYN_REF_POWER                   (1693)          /* mW  */
#define GPU_DYN_REF_POWER_FREQ              (1400000)       /* KHz */
#define GPU_DYN_REF_POWER_VOLT              (91250)         /* mV x 100 */

/**************************************************
 * Structure
 **************************************************/
struct gpufreq_pmic_info {
	struct regulator *reg_vgpu;
	struct regulator *reg_vsram;
};

#endif /* __GPUFREQ_MT6878_H__ */
