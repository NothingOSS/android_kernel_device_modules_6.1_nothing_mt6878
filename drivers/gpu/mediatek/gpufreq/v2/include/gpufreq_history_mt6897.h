/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2022 MediaTek Inc.
 */

#ifndef __GPUFREQ_HISTORY_MT6897_H__
#define __GPUFREQ_HISTORY_MT6897_H__

/**************************************************
 * Definition
 **************************************************/
#define GPUFREQ_HISTORY_SIZE \
	sizeof(struct gpu_dvfs_history_log)

/* Log Record to SYS SRAM           */
/* 1 log = 4(bytes) * 8 = 32(bytes) */
/* 32(bytes) * 100 = 3200 = C80h    */
#define GPUFREQ_HISTORY_LOG_NUM         100
#define GPUFREQ_HISTORY_LOG_ENTRY       8
#define GPUFREQ_HISTORY_SYSRAM_BASE     0x00118800
#define GPUFREQ_HISTORY_SYSRAM_SIZE    ((GPUFREQ_HISTORY_LOG_NUM * GPUFREQ_HISTORY_LOG_ENTRY) << 2)
#define GPUFREQ_HISTORY_OFFS_LOG_S     (GPUFREQ_HISTORY_SYSRAM_BASE)
#define GPUFREQ_HISTORY_OFFS_LOG_E     (GPUFREQ_HISTORY_OFFS_LOG_S + GPUFREQ_HISTORY_SYSRAM_SIZE)

/**************************************************
 * Structure
 **************************************************/
struct gpu_dvfs_source {
	unsigned int cur_volt:18;
	int cur_oppidx:7;
	int target_oppidx:7;
	unsigned int cur_vsram:18;
	int ceiling_oppidx:7;
	int floor_oppidx:7;
	unsigned int cur_freq:12;
	unsigned int park_flag:12;
	unsigned int c_limiter:4;
	unsigned int f_limiter:4;
	// (sel:1 + sram_delsel:1 + park_flag:10) or (temperature:8 + reserve:4)
};

struct gpu_dvfs_history_log {
	unsigned int time_stamp_h_log:32;
	unsigned int time_stamp_l_log:32;
	struct gpu_dvfs_source gpu_db_top;
	struct gpu_dvfs_source gpu_db_stack;

};

/**************************************************
 * Common Function
 **************************************************/
void __gpufreq_record_history_entry(enum gpufreq_history_state history_state);
void __gpufreq_history_memory_init(void);
void __gpufreq_history_memory_reset(void);
void __gpufreq_history_memory_uninit(void);
void __gpufreq_set_delsel_bit(unsigned int delsel);
unsigned int __gpufreq_get_delsel_bit(void);
void __gpufreq_set_parking_vtop(unsigned int V);
unsigned int __gpufreq_get_parking_vtop(void);
void __gpufreq_set_parking_vsram(unsigned int V);
unsigned int __gpufreq_get_parking_vsram(void);

/**************************************************
 * Variable
 **************************************************/

#endif /* __GPUFREQ_HISTORY_MT6897_H__ */
