// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2023 MediaTek Inc.
 * Author: Clouds Lee <clouds.lee@mediatek.com>
 */

#include "mtk_cg_peak_power_throttling_table.h"


#if   defined(__KERNEL__)
/*
 * -----------------------------------------------
 * IP Peak Power Table
 * -----------------------------------------------
 */
struct ippeakpowertableDataRow
ip_peak_power_table[IP_PEAK_POWER_TABLE_IDX_ROW_COUNT]
__ppt_table_alignment__ = {
    /* PP_BCPU */ {3250, 1100, 5658, 760, 10, 10},
    /* PP_MCPU */ {2950, 1000, 11710, 760, 7, 15},
    /* PP_LCPU */ {2200, 850, 5286, 550, 10, 8},
    /* PP_DSU */ {1650, 1100, 1456, 590, 10, 10},
    /* PP_GPU Stack */ {1300, 815, 15635, 1000, 5, 20},
    /* PP_GPU Top */ {1352, 831, 1527, 1000, 20, 4},
};
/*
 * -----------------------------------------------
 * Leakage Scale Table
 * -----------------------------------------------
 */
struct leakagescaletableDataRow
leakage_scale_table[LEAKAGE_SCALE_TABLE_IDX_ROW_COUNT]
__ppt_table_alignment__ = {
    /* tz0 */ {105, 1000 },
    /* tz1 */ {85, 527 },
    /* tz2 */ {65, 267 },
    /* tz3 */ {45, 130 },
    /* tz4 */ {25, 62},
};

/*
 * -----------------------------------------------
 * IP Peak Power Params (IpPeakPowerParams)
 * -----------------------------------------------
 */

/*
 * -----------------------------------------------
 * Peak Power Combo Table
 * -----------------------------------------------
 */

struct peakpowercombotableDataRow
peak_power_combo_table_gpu[PEAK_POWER_COMBO_TABLE_GPU_IDX_ROW_COUNT]
__ppt_table_alignment__ = {
	/* combo 0 */
	{1300, 1352, 2100, 2100, 1800, 1480,
	{0, 0, 0, 0, 0, 0, 727, 0}, {0, 0, 0, 0, 0, 0, 811, 0},
	{0, 0, 0, 0, 0, 0, 822, 0}, {0, 0, 0, 0, 0, 0, 825, 0},
	{0, 0, 0, 0, 0, 0, 862, 0}, {0, 0, 0, 0, 0, 0, 822, 0}, 0},
	/* combo 1 */
	{1196, 1352, 2100, 2100, 1800, 1480,
	{0, 0, 0, 0, 0, 0, 723, 0}, {0, 0, 0, 0, 0, 0, 811, 0},
	{0, 0, 0, 0, 0, 0, 822, 0}, {0, 0, 0, 0, 0, 0, 825, 0},
	{0, 0, 0, 0, 0, 0, 862, 0}, {0, 0, 0, 0, 0, 0, 822, 0}, 0},
	/* combo 2 */
	{1092, 1352, 2100, 2100, 1800, 1480,
	{0, 0, 0, 0, 0, 0, 739, 0}, {0, 0, 0, 0, 0, 0, 811, 0},
	{0, 0, 0, 0, 0, 0, 822, 0}, {0, 0, 0, 0, 0, 0, 825, 0},
	{0, 0, 0, 0, 0, 0, 862, 0}, {0, 0, 0, 0, 0, 0, 822, 0}, 0},
	/* combo 3 */
	{1014, 1352, 2000, 2000, 1800, 1410,
	{0, 0, 0, 0, 0, 0, 753, 0}, {0, 0, 0, 0, 0, 0, 811, 0},
	{0, 0, 0, 0, 0, 0, 822, 0}, {0, 0, 0, 0, 0, 0, 827, 0},
	{0, 0, 0, 0, 0, 0, 862, 0}, {0, 0, 0, 0, 0, 0, 822, 0}, 0},
	/* combo 4 */
	{910, 1352, 2000, 2000, 1700, 1410,
	{0, 0, 0, 0, 0, 0, 765, 0}, {0, 0, 0, 0, 0, 0, 811, 0},
	{0, 0, 0, 0, 0, 0, 822, 0}, {0, 0, 0, 0, 0, 0, 827, 0},
	{0, 0, 0, 0, 0, 0, 861, 0}, {0, 0, 0, 0, 0, 0, 822, 0}, 0},
	/* combo 5 */
	{702, 1352, 1700, 1700, 1500, 1130,
	{0, 0, 0, 0, 0, 0, 789, 0}, {0, 0, 0, 0, 0, 0, 811, 0},
	{0, 0, 0, 0, 0, 0, 822, 0}, {0, 0, 0, 0, 0, 0, 850, 0},
	{0, 0, 0, 0, 0, 0, 859, 0}, {0, 0, 0, 0, 0, 0, 822, 0}, 0},
};

struct peakpowercombotableDataRow
peak_power_combo_table_cpu[PEAK_POWER_COMBO_TABLE_CPU_IDX_ROW_COUNT]
__ppt_table_alignment__ = {
	/* combo 0 */
	{702, 1352, 3250, 2850, 2000, 1900,
	{0, 0, 0, 0, 0, 0, 789, 0}, {0, 0, 0, 0, 0, 0, 811, 0},
	{0, 0, 0, 0, 0, 0, 805, 0}, {0, 0, 0, 0, 0, 0, 808, 0},
	{0, 0, 0, 0, 0, 0, 862, 0}, {0, 0, 0, 0, 0, 0, 805, 0}, 0},
	/* combo 1 */
	{702, 1352, 2900, 2800, 1800, 1900,
	{0, 0, 0, 0, 0, 0, 789, 0}, {0, 0, 0, 0, 0, 0, 811, 0},
	{0, 0, 0, 0, 0, 0, 811, 0}, {0, 0, 0, 0, 0, 0, 810, 0},
	{0, 0, 0, 0, 0, 0, 862, 0}, {0, 0, 0, 0, 0, 0, 811, 0}, 0},
	/* combo 2 */
	{702, 1352, 2700, 2500, 1800, 1900,
	{0, 0, 0, 0, 0, 0, 789, 0}, {0, 0, 0, 0, 0, 0, 811, 0},
	{0, 0, 0, 0, 0, 0, 812, 0}, {0, 0, 0, 0, 0, 0, 817, 0},
	{0, 0, 0, 0, 0, 0, 862, 0}, {0, 0, 0, 0, 0, 0, 812, 0}, 0},
	/* combo 3 */
	{702, 1352, 2400, 2100, 1800, 1690,
	{0, 0, 0, 0, 0, 0, 789, 0}, {0, 0, 0, 0, 0, 0, 811, 0},
	{0, 0, 0, 0, 0, 0, 820, 0}, {0, 0, 0, 0, 0, 0, 825, 0},
	{0, 0, 0, 0, 0, 0, 862, 0}, {0, 0, 0, 0, 0, 0, 820, 0}, 0},
	/* combo 4 */
	{702, 1352, 2100, 1900, 1800, 1480,
	{0, 0, 0, 0, 0, 0, 789, 0}, {0, 0, 0, 0, 0, 0, 811, 0},
	{0, 0, 0, 0, 0, 0, 822, 0}, {0, 0, 0, 0, 0, 0, 836, 0},
	{0, 0, 0, 0, 0, 0, 862, 0}, {0, 0, 0, 0, 0, 0, 822, 0}, 0},
};

#endif /*#if   defined(__KERNEL__)*/



/*
 * ========================================================
 * Helper Function
 * ========================================================
 */
void print_peak_power_combo_table(
	struct peakpowercombotableDataRow combo_table[],
	int count)
{
	pp_print("Peak Power Combo Table:\n");
	pp_print(
		"  %-10s | GPUSFreq | GPUTFreq | BCPUFreq | MCPUFreq | LCPUFreq | DSUFreq | ComboPowerIn\n",
		"Combo");
	for (int i = 0; i < count; i++) {
		pp_print(
			"  %-8s %d | %7d | %7d | %7d | %7d | %7d | %6d | %12d\n",
			"Combo ", i, combo_table[i].gpustackfreq_m,
			combo_table[i].gputopfreq_m, combo_table[i].bcpufreq_m,
			combo_table[i].mcpufreq_m, combo_table[i].lcpufreq_m,
			combo_table[i].dsufreq_m,
			combo_table[i].combopeakpowerin_mw);
	}

	//IpPeakPowerParams
	pp_print("\nIpPeakPowerParams - Table:\n");
	pp_print(
		"  %-10s |  vol_mv | leak105c | peakdynout | peaklkgout | peakpowerout | rloss_mw | pmiceff | peakpowerin\n",
		"Part/Index");
	for (int i = 0; i < count; i++) {
		struct IpPeakPowerParams *params[] = {
			&combo_table[i].gpustackparams,
			&combo_table[i].gputopparams,
			&combo_table[i].bcpuparams,
			&combo_table[i].mcpuparams,
			&combo_table[i].lcpuparams,
			&combo_table[i].dsuparams };
		static const char * const part_names[] = {
			"GPUS Stack",
			"GPU Top",
			"BCPU",
			"MCPU",
			"LCPU",
			"DSU" };

		pp_print("  combo %d:\n", i);
		for (int j = 0;
		     j <
		     (int)(sizeof(params)/sizeof(struct IpPeakPowerParams *));
		     j++) {
			pp_print(
				"  %-10s | %6d | %7d | %9d | %9d | %11d | %6d | %6d | %10d\n",
				part_names[j], params[j]->vol_mv,
				params[j]->leak105c_mw,
				params[j]->peakdynout_mw,
				params[j]->peaklkgout_mw,
				params[j]->peakpowerout_mw, params[j]->rloss_mw,
				params[j]->pmiceff, params[j]->peakpowerin_mw);
		}
		pp_print("\n");
	}
}

void print_structure_values(void *structure_ptr, unsigned int size)
{
	unsigned short *data_ptr = (unsigned short *)structure_ptr;
	unsigned int num_values = size / sizeof(unsigned short);
	unsigned int i;
	char buffer[64] = { 0 };
	unsigned int buf_len = (unsigned int)sizeof(buffer);
	unsigned int offset = 0;

	pp_print("[CG peak power] table address = %llx , size = %u\n",
		 (unsigned long long)structure_ptr, size);

	for (i = 0; i < num_values; i++) {
		offset += snprintf(buffer + offset, buf_len - offset, "%hu ",
				   data_ptr[i]);
		if (i % 8 == 7) {
			pp_print("%s\n", buffer);
			memset(buffer, 0, buf_len);
			offset = 0;
		}
	}
	if (offset > 0)
		pp_print("%s\n", buffer);
}

