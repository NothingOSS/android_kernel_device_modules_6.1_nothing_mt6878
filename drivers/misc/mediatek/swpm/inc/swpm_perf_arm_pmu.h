/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#ifndef __SWPM_PERF_ARM_PMU_H__
#define __SWPM_PERF_ARM_PMU_H__

enum swpm_perf_evt_id {
	L3DC_EVT,
	INST_SPEC_EVT,
	CYCLES_EVT,
	DSU_CYCLES_EVT,
	PMU_3_EVT,
	PMU_4_EVT,
	PMU_5_EVT,
	PMU_6_EVT,
	PMU_7_EVT,
	PMU_8_EVT,
	PMU_9_EVT,
	PMU_10_EVT,
	PMU_11_EVT,
};

extern unsigned int swpm_arm_pmu_get_status(void);
extern unsigned int swpm_arm_dsu_pmu_get_status(void);
extern int swpm_arm_pmu_get_idx(unsigned int evt_id,
				unsigned int cpu);
extern int swpm_arm_pmu_enable_all(unsigned int enable);
extern int swpm_arm_dsu_pmu_enable(unsigned int enable);
extern unsigned int swpm_arm_dsu_pmu_get_type(void);
extern int swpm_arm_dsu_pmu_set_type(unsigned int type);

#endif
