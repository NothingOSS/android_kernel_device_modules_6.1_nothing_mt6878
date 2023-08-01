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
	unsigned short powerratiogpu_permille;
	unsigned short powerratiocpu_permille;
	unsigned short powerratiomultiscene_permille;
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
	unsigned short dvcsratio_permille;
	unsigned short pmiceff;
	unsigned short vol_mv;
	unsigned short leak105c_mw;
	unsigned short peakdynout_mw;
	unsigned short peaklkgout_mw;
	unsigned short peakpowerout_mw;
	unsigned short rloss_mw;
	unsigned short peakpowerin_mw;
};

/*
 * -----------------------------------------------
 * C/G Peak Power Combo Table
 * -----------------------------------------------
 */
enum GPU_PEAK_POWER_COMBO_TABLE_IDX {
	GPU_COMBO_0_IDX,     /* combo 0 */
	GPU_COMBO_1_IDX,     /* combo 1 */
	GPU_COMBO_2_IDX,     /* combo 2 */
	GPU_COMBO_3_IDX,     /* combo 3 */
	GPU_COMBO_4_IDX,     /* combo 4 */
	GPU_COMBO_5_IDX,     /* combo 5 */
	GPU_COMBO_6_IDX,     /* combo 6 */
	GPU_COMBO_7_IDX,     /* combo 7 */
	GPU_COMBO_8_IDX,     /* combo 8 */
	GPU_COMBO_9_IDX,     /* combo 9 */
	GPU_PEAK_POWER_COMBO_TABLE_IDX_ROW_COUNT
};

enum CPU_PEAK_POWER_COMBO_TABLE_IDX {
	CPU_COMBO_0_IDX,     /* combo 0 */
	CPU_COMBO_1_IDX,     /* combo 1 */
	CPU_COMBO_2_IDX,     /* combo 2 */
	CPU_COMBO_3_IDX,     /* combo 3 */
	CPU_COMBO_4_IDX,     /* combo 4 */
	CPU_COMBO_5_IDX,     /* combo 5 */
	CPU_COMBO_6_IDX,     /* combo 6 */
	CPU_COMBO_7_IDX,     /* combo 7 */
	CPU_COMBO_8_IDX,     /* combo 8 */
	CPU_COMBO_9_IDX,     /* combo 9 */
	CPU_PEAK_POWER_COMBO_TABLE_IDX_ROW_COUNT
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
	peak_power_combo_table_gpu[GPU_PEAK_POWER_COMBO_TABLE_IDX_ROW_COUNT];
extern struct peakpowercombotableDataRow
	peak_power_combo_table_cpu[CPU_PEAK_POWER_COMBO_TABLE_IDX_ROW_COUNT];

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

struct moInfo {
	short mo_favor_cpu;
	short mo_favor_gpu;
	short mo_favor_multiscene;
	short mo_cpu_avs;
	short mo_gpu_avs;
	short mo_gpu_curr_freq_power_calc;
	unsigned int mo_status;
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
	peak_power_combo_table_gpu[GPU_PEAK_POWER_COMBO_TABLE_IDX_ROW_COUNT]
	__ppt_table_alignment__;
	struct peakpowercombotableDataRow
	peak_power_combo_table_cpu[CPU_PEAK_POWER_COMBO_TABLE_IDX_ROW_COUNT]
	__ppt_table_alignment__;

	/*misc info*/
	struct cswrunInfo cswrun_info;
	struct gswrunInfo gswrun_info;
	struct moInfo mo_info;
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

