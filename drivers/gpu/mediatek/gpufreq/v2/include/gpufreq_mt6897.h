/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2022 MediaTek Inc.
 */

#ifndef __GPUFREQ_MT6897_H__
#define __GPUFREQ_MT6897_H__

/**************************************************
 * GPUFREQ Config
 **************************************************/
/* 0 -> power on once then never off and disable DDK power on/off callback */
#define GPUFREQ_POWER_CTRL_ENABLE           (1)
/* 0 -> disable DDK runtime active-idle callback */
#define GPUFREQ_ACTIVE_SLEEP_CTRL_ENABLE    (0)
/*
 * (DVFS_ENABLE, CUST_INIT)
 * (1, 1) -> DVFS enable and init to CUST_INIT_OPPIDX
 * (1, 0) -> DVFS enable
 * (0, 1) -> DVFS disable but init to CUST_INIT_OPPIDX (do DVFS only onces)
 * (0, 0) -> DVFS disable
 */
#define GPUFREQ_DVFS_ENABLE                 (1)
#define GPUFREQ_CUST_INIT_ENABLE            (0)
#define GPUFREQ_CUST_INIT_OPPIDX            (0)
/* MFGSYS Feature */
#define GPUFREQ_HWDCM_ENABLE                (1)
#define GPUFREQ_ACP_ENABLE                  (1)
#define GPUFREQ_PDCA_ENABLE                 (1)
#define GPUFREQ_GPM1_ENABLE                 (1)
#define GPUFREQ_GPM3_ENABLE                 (0)
#define GPUFREQ_MERGER_ENABLE               (1)
#define GPUFREQ_AXUSER_PREULTRA_ENABLE      (1)
#define GPUFREQ_AXUSER_SLC_ENABLE           (0)
#define GPUFREQ_DFD_ENABLE                  (0)
#define GPUFREQ_AVS_ENABLE                  (1)
#define GPUFREQ_ASENSOR_ENABLE              (0)
#define GPUFREQ_IPS_ENABLE                  (0)
#define GPUFREQ_SHARED_STATUS_REG           (0)
#define GPUFREQ_PDCA_PIPELINE_ENABLE        (0)
#define GPUFREQ_POWER_TRACKER_ENABLE        (0)

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
#define MFGSC_SEL_MFGPSCLL_MASK             (BIT(17))         /* [17] */
#define MFG_REF_SEL_MASK                    (GENMASK(1, 0))   /* [1:0] */
#define MFGSC_REF_SEL_MASK                  (GENMASK(9, 8))   /* [9:8] */
#define FREQ_ROUNDUP_TO_10(freq)            ((freq % 10) ? (freq - (freq % 10) + 10) : freq)

/**************************************************
 * Frequency Hopping Setting
 **************************************************/
#define GPUFREQ_FHCTL_ENABLE                (0)
#define MFG_PLL_NAME                        "mfg_ao_mfgpll"
#define MFGSC_PLL_NAME                      "mfgsc_ao_mfgscpll"

/**************************************************
 * MTCMOS Setting
 **************************************************/
#define GPUFREQ_CHECK_MFG_PWR_STATUS        (0)
#define MFG_0_1_PWR_MASK                    (GENMASK(1, 0))
#define MFG_0_14_PWR_MASK                   (GENMASK(14, 9) | GENMASK(7, 6) | GENMASK(4, 0))
#define MFG_1_14_PWR_MASK                   (GENMASK(14, 9) | GENMASK(7, 6) | GENMASK(4, 1))
#define MFG_0_1_PWR_STATUS \
	(((readl(SPM_XPU_PWR_STATUS) & BIT(1)) >> 1) | \
	(readl(MFG_RPC_PWR_CON_STATUS) & BIT(1)))
#define MFG_0_14_PWR_STATUS \
	(((readl(SPM_XPU_PWR_STATUS) & BIT(1)) >> 1) | \
	((readl(MFG_RPC_PWR_CON_STATUS) & MFG_1_14_PWR_MASK)))

/**************************************************
 * Shader Core Setting
 **************************************************/
#define MFG3_SHADER_STACK0                  (T0C0 | T0C1)   /* MFG9,  MFG11 */
#define MFG4_SHADER_STACK1                  (T1C0)          /* MFG10 */
#define MFG6_SHADER_STACK4                  (T4C0 | T4C1)   /* MFG12, MFG14 */
#define MFG7_SHADER_STACK5                  (T5C0)          /* MFG13 */

#define GPU_SHADER_PRESENT_1 \
	(T0C0)
#define GPU_SHADER_PRESENT_2 \
	(T0C0 | MFG4_SHADER_STACK1)
#define GPU_SHADER_PRESENT_3 \
	(MFG3_SHADER_STACK0 | MFG4_SHADER_STACK1)
#define GPU_SHADER_PRESENT_4 \
	(MFG3_SHADER_STACK0 | MFG4_SHADER_STACK1 | T4C0)
#define GPU_SHADER_PRESENT_5 \
	(MFG3_SHADER_STACK0 | MFG4_SHADER_STACK1 | T4C0 | MFG7_SHADER_STACK5)
#define GPU_SHADER_PRESENT_6 \
	(MFG3_SHADER_STACK0 | MFG4_SHADER_STACK1 | MFG6_SHADER_STACK4 | MFG7_SHADER_STACK5)

#define SHADER_CORE_NUM                 (6)
struct gpufreq_core_mask_info g_core_mask_table[] = {
	{6, GPU_SHADER_PRESENT_6},
	{5, GPU_SHADER_PRESENT_5},
	{4, GPU_SHADER_PRESENT_4},
	{3, GPU_SHADER_PRESENT_3},
	{2, GPU_SHADER_PRESENT_2},
	{1, GPU_SHADER_PRESENT_1},
};

/**************************************************
 * Dynamic Power Setting
 **************************************************/
#define GPU_DYN_REF_POWER                   (4953)          /* mW  */
#define GPU_DYN_REF_POWER_FREQ              (1400000)       /* KHz */
#define GPU_DYN_REF_POWER_VOLT              (90000)         /* mV x 100 */

/**************************************************
 * PMIC Setting
 **************************************************/
/*
 * PMIC hardware range:
 * VGPUSTACK  0.4  - 1.19375 V (MT6368_VBUCK2)
 * VSRAM      0.55 - 0.95 V    (MT6363_VSRAM_MDFE)
 */
#define VGPU_MAX_VOLT                       (119375)        /* mV x 100 */
#define VGPU_MIN_VOLT                       (40000)         /* mV x 100 */
#define VSRAM_MAX_VOLT                      (95000)         /* mV x 100 */
#define VSRAM_MIN_VOLT                      (55000)         /* mV x 100 */
#define VSRAM_THRESH                        (75000)         /* mV x 100 */
#define PMIC_STEP                           (625)           /* mV x 100 */
/*
 * (0)mv <= (VSRAM - VGPU) <= (200)mV
 */
#define VSRAM_VLOGIC_DIFF                   (20000)         /* mV x 100 */
#define VOLT_NORMALIZATION(volt)            ((volt % 625) ? (volt - (volt % 625) + 625) : volt)

/**************************************************
 * Power Throttling Setting
 **************************************************/
#define GPUFREQ_BATT_OC_ENABLE              (1)
#define GPUFREQ_BATT_PERCENT_ENABLE         (0)
#define GPUFREQ_LOW_BATT_ENABLE             (1)
#define GPUFREQ_BATT_OC_FREQ                (681000)
#define GPUFREQ_BATT_PERCENT_LV2_FREQ       (780000)
#define GPUFREQ_BATT_PERCENT_LV3_FREQ       (681000)
#define GPUFREQ_BATT_PERCENT_LV4_FREQ       (483000)
#define GPUFREQ_BATT_PERCENT_LV5_FREQ       (284000)
#define GPUFREQ_LOW_BATT_LV1_FREQ           (1190000)
#define GPUFREQ_LOW_BATT_LV2_FREQ           (681000)
#define GPUFREQ_LOW_BATT_LV3_FREQ           (284000)

/**************************************************
 * Aging Sensor Setting
 **************************************************/
#define GPUFREQ_AGING_KEEP_FGPU             (945000)
#define GPUFREQ_AGING_KEEP_VGPU             (82500)
#define GPUFREQ_AGING_KEEP_FSTACK           (600000)
#define GPUFREQ_AGING_KEEP_VSTACK           (65000)
#define GPUFREQ_AGING_KEEP_VSRAM            (82500)
#define GPUFREQ_AGING_LKG_VSTACK            (70000)
#define GPUFREQ_AGING_GAP_MIN               (-3)
#define GPUFREQ_AGING_GAP_1                 (2)
#define GPUFREQ_AGING_GAP_2                 (4)
#define GPUFREQ_AGING_GAP_3                 (6)
#define GPUFREQ_AGING_MAX_TABLE_IDX         (1)
#define GPUFREQ_AGING_MOST_AGRRESIVE        (0)

/**************************************************
 * DVFS Constraint Setting
 **************************************************/
#define SRAM_DEL_SEL_OPP                    (52)

/**************************************************
 * DFD Setting
 **************************************************/
#define MFG_DEBUGMON_CON_00_ENABLE          (0xFFFFFFFF)
#define MFG_DFD_CON_0_ENABLE                (0x0F101100)
#define MFG_DFD_CON_1_ENABLE                (0x00015E14)
#define MFG_DFD_CON_3_ENABLE                (0x00110063)
#define MFG_DFD_CON_4_ENABLE                (0x00000000)
#define MFG_DFD_CON_17_ENABLE               (0x00000000)
#define MFG_DFD_CON_18_ENABLE               (0x00000000)
#define MFG_DFD_CON_19_ENABLE               (0x00000000)

/**************************************************
 * Leakage Power Setting
 **************************************************/
#define GPU_LKG_POWER                       (30)

/**************************************************
 * Enumeration
 **************************************************/
enum gpufreq_segment {
	MT6897_SEGMENT = 0,
};

enum gpufreq_clk_src {
	CLOCK_SUB = 0,
	CLOCK_MAIN,
};

/**************************************************
 * Structure
 **************************************************/
struct gpufreq_pmic_info {
	struct regulator *reg_vgpu;
	struct regulator *reg_vsram;
};

struct gpufreq_clk_info {
	struct clk *clk_mux;
	struct clk *clk_main_parent;
	struct clk *clk_sub_parent;
	struct clk *clk_sc_mux;
	struct clk *clk_sc_main_parent;
	struct clk *clk_sc_sub_parent;
};

struct gpufreq_status {
	struct gpufreq_opp_info *signed_table;
	struct gpufreq_opp_info *working_table;
	int buck_count;
	int mtcmos_count;
	int cg_count;
	int power_count;
	int active_count;
	unsigned int segment_id;
	int signed_opp_num;
	int segment_upbound;
	int segment_lowbound;
	int opp_num;
	int max_oppidx;
	int min_oppidx;
	int cur_oppidx;
	unsigned int cur_freq;
	unsigned int cur_volt;
	unsigned int cur_vsram;
	unsigned int lkg_rt_info;
	unsigned int lkg_ht_info;
	unsigned int lkg_rt_info_sram;
	unsigned int lkg_ht_info_sram;
};

/**************************************************
 * GPU Platform OPP Table Definition
 **************************************************/
#define GPU_SIGNED_OPP_0                    (0)
#define GPU_SIGNED_OPP_1                    (36)
#define GPU_SIGNED_OPP_2                    (64)
#define NUM_GPU_SIGNED_IDX                  ARRAY_SIZE(g_gpu_signed_idx)
#define NUM_GPU_SIGNED_OPP                  ARRAY_SIZE(g_gpu_default_opp_table)
static const int g_gpu_signed_idx[] = {
	GPU_SIGNED_OPP_0,
	GPU_SIGNED_OPP_1,
	GPU_SIGNED_OPP_2,
};
static struct gpufreq_opp_info g_gpu_default_opp_table[] = {
	GPUOP(1400000, 90000, 90000, POSDIV_POWER_2, 0, 0), /*  0 sign off, binning point */
	GPUOP(1383000, 89375, 89375, POSDIV_POWER_2, 0, 0), /*  1 */
	GPUOP(1367000, 88750, 88750, POSDIV_POWER_2, 0, 0), /*  2 */
	GPUOP(1351000, 88125, 88125, POSDIV_POWER_2, 0, 0), /*  3 */
	GPUOP(1335000, 87500, 87500, POSDIV_POWER_2, 0, 0), /*  4 */
	GPUOP(1319000, 86875, 86875, POSDIV_POWER_2, 0, 0), /*  5 */
	GPUOP(1303000, 86250, 86250, POSDIV_POWER_2, 0, 0), /*  6 */
	GPUOP(1287000, 85625, 85625, POSDIV_POWER_2, 0, 0), /*  7 */
	GPUOP(1271000, 85000, 85000, POSDIV_POWER_2, 0, 0), /*  8 */
	GPUOP(1255000, 84375, 84375, POSDIV_POWER_2, 0, 0), /*  9 */
	GPUOP(1238000, 83750, 83750, POSDIV_POWER_2, 0, 0), /* 10 */
	GPUOP(1222000, 83125, 83125, POSDIV_POWER_2, 0, 0), /* 11 */
	GPUOP(1206000, 82500, 82500, POSDIV_POWER_2, 0, 0), /* 12 */
	GPUOP(1190000, 81875, 81875, POSDIV_POWER_2, 0, 0), /* 13 */
	GPUOP(1174000, 81250, 81250, POSDIV_POWER_2, 0, 0), /* 14 */
	GPUOP(1158000, 80625, 80625, POSDIV_POWER_2, 0, 0), /* 15 */
	GPUOP(1142000, 80000, 80000, POSDIV_POWER_2, 0, 0), /* 16 sign off */
	GPUOP(1126000, 79375, 79375, POSDIV_POWER_2, 0, 0), /* 17 */
	GPUOP(1110000, 78750, 78750, POSDIV_POWER_2, 0, 0), /* 18 */
	GPUOP(1093000, 78125, 78125, POSDIV_POWER_2, 0, 0), /* 19 */
	GPUOP(1077000, 77500, 77500, POSDIV_POWER_2, 0, 0), /* 20 */
	GPUOP(1061000, 76875, 76875, POSDIV_POWER_2, 0, 0), /* 21 */
	GPUOP(1045000, 76250, 76250, POSDIV_POWER_2, 0, 0), /* 22 */
	GPUOP(1029000, 75625, 75625, POSDIV_POWER_2, 0, 0), /* 23 */
	GPUOP(1013000, 75000, 75000, POSDIV_POWER_2, 0, 0), /* 24 */
	GPUOP(997000,  74375, 75000, POSDIV_POWER_2, 0, 0), /* 25 */
	GPUOP(981000,  73750, 75000, POSDIV_POWER_2, 0, 0), /* 26 */
	GPUOP(965000,  73125, 75000, POSDIV_POWER_2, 0, 0), /* 27 */
	GPUOP(948000,  72500, 75000, POSDIV_POWER_4, 0, 0), /* 28 */
	GPUOP(932000,  71875, 75000, POSDIV_POWER_4, 0, 0), /* 29 */
	GPUOP(916000,  71250, 75000, POSDIV_POWER_4, 0, 0), /* 30 */
	GPUOP(900000,  70625, 75000, POSDIV_POWER_4, 0, 0), /* 31 */
	GPUOP(884000,  70000, 75000, POSDIV_POWER_4, 0, 0), /* 32 */
	GPUOP(868000,  69375, 75000, POSDIV_POWER_4, 0, 0), /* 33 */
	GPUOP(852000,  68750, 75000, POSDIV_POWER_4, 0, 0), /* 34 */
	GPUOP(836000,  68125, 75000, POSDIV_POWER_4, 0, 0), /* 35 */
	GPUOP(820000,  67500, 75000, POSDIV_POWER_4, 0, 0), /* 36 sign off, binning point */
	GPUOP(800000,  66875, 75000, POSDIV_POWER_4, 0, 0), /* 37 */
	GPUOP(780000,  66250, 75000, POSDIV_POWER_4, 0, 0), /* 38 */
	GPUOP(760000,  65625, 75000, POSDIV_POWER_4, 0, 0), /* 39 */
	GPUOP(740000,  65000, 75000, POSDIV_POWER_4, 0, 0), /* 40 */
	GPUOP(720000,  64375, 75000, POSDIV_POWER_4, 0, 0), /* 41 */
	GPUOP(701000,  63750, 75000, POSDIV_POWER_4, 0, 0), /* 42 */
	GPUOP(681000,  63125, 75000, POSDIV_POWER_4, 0, 0), /* 43 */
	GPUOP(661000,  62500, 75000, POSDIV_POWER_4, 0, 0), /* 44 */
	GPUOP(641000,  61875, 75000, POSDIV_POWER_4, 0, 0), /* 45 */
	GPUOP(621000,  61250, 75000, POSDIV_POWER_4, 0, 0), /* 46 */
	GPUOP(601000,  60625, 75000, POSDIV_POWER_4, 0, 0), /* 47 */
	GPUOP(582000,  60000, 75000, POSDIV_POWER_4, 0, 0), /* 48 */
	GPUOP(562000,  59375, 75000, POSDIV_POWER_4, 0, 0), /* 49 */
	GPUOP(542000,  58750, 75000, POSDIV_POWER_4, 0, 0), /* 50 */
	GPUOP(522000,  58125, 75000, POSDIV_POWER_4, 0, 0), /* 51 */
	GPUOP(502000,  57500, 75000, POSDIV_POWER_4, 0, 0), /* 52 sign off */
	GPUOP(483000,  56875, 75000, POSDIV_POWER_4, 0, 0), /* 53 */
	GPUOP(463000,  56250, 75000, POSDIV_POWER_8, 0, 0), /* 54 */
	GPUOP(443000,  55625, 75000, POSDIV_POWER_8, 0, 0), /* 55 */
	GPUOP(423000,  55000, 75000, POSDIV_POWER_8, 0, 0), /* 56 */
	GPUOP(403000,  54375, 75000, POSDIV_POWER_8, 0, 0), /* 57 */
	GPUOP(383000,  53750, 75000, POSDIV_POWER_8, 0, 0), /* 58 */
	GPUOP(364000,  53125, 75000, POSDIV_POWER_8, 0, 0), /* 59 */
	GPUOP(344000,  52500, 75000, POSDIV_POWER_8, 0, 0), /* 60 */
	GPUOP(324000,  51875, 75000, POSDIV_POWER_8, 0, 0), /* 61 */
	GPUOP(304000,  51250, 75000, POSDIV_POWER_8, 0, 0), /* 62 */
	GPUOP(284000,  50625, 75000, POSDIV_POWER_8, 0, 0), /* 63 */
	GPUOP(265000,  50000, 75000, POSDIV_POWER_8, 0, 0), /* 64 sign off, binning point */
};

/**************************************************
 * OPP Adjustment
 **************************************************/
static struct gpufreq_adj_info g_gpu_avs_table[NUM_GPU_SIGNED_IDX] = {
	ADJOP(GPU_SIGNED_OPP_0, 0, 0, 0),
	ADJOP(GPU_SIGNED_OPP_1, 0, 0, 0),
	ADJOP(GPU_SIGNED_OPP_2, 0, 0, 0),
};

static struct gpufreq_adj_info g_gpu_aging_table[][NUM_GPU_SIGNED_IDX] = {
	{ /* aging table 0 */
		ADJOP(GPU_SIGNED_OPP_0, 0, 625, 0),
		ADJOP(GPU_SIGNED_OPP_1, 0, 625, 0),
		ADJOP(GPU_SIGNED_OPP_2, 0, 625, 0),
	},
	{ /* aging table 1 */
		ADJOP(GPU_SIGNED_OPP_0, 0, 0, 0),
		ADJOP(GPU_SIGNED_OPP_1, 0, 0, 0),
		ADJOP(GPU_SIGNED_OPP_2, 0, 0, 0),
	},
	/* aging table 2: remove for code size */
	/* aging table 3: remove for code size */
};

#endif /* __GPUFREQ_MT6897_H__ */
