/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2023 MediaTek Inc.
 * Author: Clouds Lee <clouds.lee@mediatek.com>
 */

#ifndef _MTK_CG_PEAK_POWER_THROTTLING_TABLE_H_
#define _MTK_CG_PEAK_POWER_THROTTLING_TABLE_H_

#include "mtk_cg_peak_power_throttling_def.h"

/*
 * -----------------------------------------------
 * IP Peak Power Table
 * -----------------------------------------------
 */
enum IP_PEAK_POWER_TABLE_IDX {
	PP_BCPU_IDX, /* PP_BCPU */
	PP_MCPU_IDX, /* PP_MCPU */
	PP_LCPU_IDX, /* PP_LCPU */
	PP_DSU_IDX, /* PP_DSU */
	PP_GPU_STACK_IDX, /* PP_GPU Stack */
	PP_GPU_TOP_IDX, /* PP_GPU Top */
	IP_PEAK_POWER_TABLE_IDX_ROW_COUNT
};
struct ippeakpowertableDataRow {
	unsigned short freq_m;
	unsigned short voltage_mv;
	unsigned short dynpwr_mw;
	unsigned short powerratio_permille;
	unsigned short rloss_permille;
	unsigned short preoc_a;
};

extern struct ippeakpowertableDataRow
	ip_peak_power_table[IP_PEAK_POWER_TABLE_IDX_ROW_COUNT];
/*
 * -----------------------------------------------
 * Leakage Scale Table
 * -----------------------------------------------
 */
enum LEAKAGE_SCALE_TABLE_IDX {
	TZ0_IDX, /* tz0 */
	TZ1_IDX, /* tz1 */
	TZ2_IDX, /* tz2 */
	TZ3_IDX, /* tz3 */
	TZ4_IDX, /* tz4 */
	LEAKAGE_SCALE_TABLE_IDX_ROW_COUNT
};
struct leakagescaletableDataRow {
	short temperature;
	unsigned short scale_permille;
};

extern struct leakagescaletableDataRow
	leakage_scale_table[LEAKAGE_SCALE_TABLE_IDX_ROW_COUNT];
/*
 * -----------------------------------------------
 * IP Peak Power Params (IpPeakPowerParams)
 * -----------------------------------------------
 */
struct IpPeakPowerParams {
	unsigned short vol_mv;
	unsigned short leak105c_mw;
	unsigned short peakdynout_mw;
	unsigned short peaklkgout_mw;
	unsigned short peakpowerout_mw;
	unsigned short rloss_mw;
	unsigned short pmiceff;
	unsigned short peakpowerin_mw;
};

/*
 * -----------------------------------------------
 * CPU Peak Power Combo Table (cpu_peak_power_combo_table)
 * -----------------------------------------------
 */
enum PEAK_POWER_COMBO_TABLE_GPU_IDX {
	GPU_COMBO_0_IDX, /* combo 0 */
	GPU_COMBO_1_IDX, /* combo 1 */
	GPU_COMBO_2_IDX, /* combo 2 */
	GPU_COMBO_3_IDX, /* combo 3 */
	GPU_COMBO_4_IDX, /* combo 4 */
	GPU_COMBO_5_IDX, /* combo 5 */
	PEAK_POWER_COMBO_TABLE_GPU_IDX_ROW_COUNT
};

enum PEAK_POWER_COMBO_TABLE_CPU_IDX {
	CPU_COMBO_0_IDX, /* combo 0 */
	CPU_COMBO_1_IDX, /* combo 1 */
	CPU_COMBO_2_IDX, /* combo 2 */
	CPU_COMBO_3_IDX, /* combo 3 */
	CPU_COMBO_4_IDX, /* combo 4 */
	PEAK_POWER_COMBO_TABLE_CPU_IDX_ROW_COUNT
};

struct peakpowercombotableDataRow {
	unsigned short gpustackfreq_m;
	unsigned short gputopfreq_m;
	unsigned short bcpufreq_m;
	unsigned short mcpufreq_m;
	unsigned short lcpufreq_m;
	unsigned short dsufreq_m;
	struct IpPeakPowerParams gpustackparams;
	struct IpPeakPowerParams gputopparams;
	struct IpPeakPowerParams bcpuparams;
	struct IpPeakPowerParams mcpuparams;
	struct IpPeakPowerParams lcpuparams;
	struct IpPeakPowerParams dsuparams;
	unsigned int combopeakpowerin_mw;
};

extern struct peakpowercombotableDataRow
	peak_power_combo_table_gpu[PEAK_POWER_COMBO_TABLE_GPU_IDX_ROW_COUNT];
extern struct peakpowercombotableDataRow
	peak_power_combo_table_cpu[PEAK_POWER_COMBO_TABLE_CPU_IDX_ROW_COUNT];

/*
 * -----------------------------------------------
 * SRAM Layout
 * -----------------------------------------------
 */
struct cswrunInfo {
	short cg_sync_enable; /*1:enable*/
	short is_fastdvfs_enabled;
};

struct gswrunInfo {
	int cgppb_mw;
	int gpu_preboost_time_us;

	short cgsync_action;
	short is_gpu_favor;
	short combo_idx;
	unsigned short gpu_limit_freq_m;
};

struct DlptSramLayout {
	/*meta-data (status)*/
	unsigned short data_moved;
	unsigned short cpu_data_valid;
	unsigned short gpu_data_valid;
	/*table*/
	struct ippeakpowertableDataRow
	ip_peak_power_table[IP_PEAK_POWER_TABLE_IDX_ROW_COUNT]
	__ppt_table_alignment__;
	struct leakagescaletableDataRow
	leakage_scale_table[LEAKAGE_SCALE_TABLE_IDX_ROW_COUNT]
	__ppt_table_alignment__;
	struct peakpowercombotableDataRow
	peak_power_combo_table_gpu[PEAK_POWER_COMBO_TABLE_GPU_IDX_ROW_COUNT]
	__ppt_table_alignment__;
	struct peakpowercombotableDataRow
	peak_power_combo_table_cpu[PEAK_POWER_COMBO_TABLE_CPU_IDX_ROW_COUNT]
	__ppt_table_alignment__;

	/*misc info*/
	struct cswrunInfo cswrun_info;
	struct gswrunInfo gswrun_info;
}  __ppt_table_alignment__;

#endif /*_MTK_CG_PEAK_POWER_THROTTLING_TABLE_H_*/

/*
 * -----------------------------------------------
 * Helper Function
 * -----------------------------------------------
 */
extern void
print_peak_power_combo_table(
	struct peakpowercombotableDataRow combo_table[],
	int count);
extern void print_structure_values(void *structure_ptr, unsigned int size);

