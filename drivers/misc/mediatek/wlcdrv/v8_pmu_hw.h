/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2022 MediaTek Inc.
 */

#ifndef __V8_PMU_HW_H__
#define __V8_PMU_HW_H__

#include <linux/device.h>
#include <linux/perf_event.h>

#if (IS_ENABLED(CONFIG_ARM64) || IS_ENABLED(CONFIG_ARM))
#include <linux/platform_device.h>
#include <linux/perf/arm_pmu.h>
#endif

#define MODE_DISABLED	0
#define MODE_INTERRUPT	1
#define MODE_POLLING	2

/* max number of pmu counter for armv9 is 20+1 */
#define	MXNR_PMU_EVENTS          22
/* a roughly large enough size for pmu events buffers,       */
/* if an input length is rediculously too many, we drop them */
#define MXNR_PMU_EVENT_BUFFER_SZ ((MXNR_PMU_EVENTS) + 16)

struct pmu_data_info {
	unsigned char mode;
	unsigned short event;
};

struct cpu_pmu_hw {
	const char *name;
	const char *cpu_name;
	int nr_cnt;
	int (*get_event_desc)(int idx, int event, char *event_desc);
	int (*check_event)(struct pmu_data_info *pmu, int idx, int event);
	void (*start)(struct pmu_data_info *pmu, int count);
	void (*stop)(int count);
	unsigned int (*polling)(struct pmu_data_info *pmu, int count, unsigned int *pmu_value);
	unsigned long (*perf_event_get_evttype)(struct perf_event *ev);
	u32 (*pmu_read_clear_overflow_flag)(void);
	void (*write_counter)(unsigned int idx,
			      unsigned int val, int is_cyc_cnt);
	void (*disable_intr)(unsigned int idx);
	void (*disable_cyc_intr)(void);

	struct pmu_data_info *pmu[NR_CPUS];
	int event_count[NR_CPUS];
	/*
	 * used for compensation of pmu counter loss
	 * between end of polling and start of cpu pm
	 */
	unsigned int cpu_pm_unpolled_loss[NR_CPUS][MXNR_PMU_EVENTS];
};

struct cpu_pmu_hw *cpu_pmu_hw_init(void);
void update_pmu_event_count(unsigned int cpu);
extern struct cpu_pmu_hw *g_cpu_pmu;

#endif /*__V8_PMU_HW_H__*/
