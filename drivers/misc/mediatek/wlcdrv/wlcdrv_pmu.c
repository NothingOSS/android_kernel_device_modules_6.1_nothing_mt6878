// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2022 MediaTek Inc.
 */

#include <linux/cpu.h>
#include <linux/cpu_pm.h>

#include "wlcdrv_pmu.h"
#include "v8_pmu_hw.h"

struct cpu_pmu_hw *g_cpu_pmu;

static int counter_cnt[MAX_NR_CPUS];
static int nr_arg[MAX_NR_CPUS];
static int nr_ignored_arg[MAX_NR_CPUS];
int wlc_perf_cpupmu_status;

/* max number of pmu counter for armv9 is 20+1 */
#define MXNR_PMU_EVENTS			 22
/* a roughly large enough size for pmu events buffers,		 */
/* if an input length is rediculously too many, we drop them */
#define MXNR_PMU_EVT_SZ ((MXNR_PMU_EVENTS) + 16)

#if IS_ENABLED(CONFIG_CPU_PM)
int use_cpu_pm_pmu_notifier;

/* helper notifier for maintaining pmu states before cpu state transition */
static int cpu_pm_pmu_notify(struct notifier_block *b,
				 unsigned long cmd,
				 void *p)
{
	unsigned int cpu;
	int ii, count;
	unsigned int pmu_value[MXNR_PMU_EVENTS];

	if (!wlc_perf_cpupmu_status)
		return NOTIFY_OK;

	cpu = raw_smp_processor_id();

	switch (cmd) {
	case CPU_PM_ENTER:
		count = g_cpu_pmu->polling(g_cpu_pmu->pmu[cpu],
				g_cpu_pmu->event_count[cpu], pmu_value);

		for (ii = 0; ii < count; ii++)
			g_cpu_pmu->cpu_pm_unpolled_loss[cpu][ii] += pmu_value[ii];

		g_cpu_pmu->stop(g_cpu_pmu->event_count[cpu]);
		break;
	case CPU_PM_ENTER_FAILED:
	case CPU_PM_EXIT:
		g_cpu_pmu->start(g_cpu_pmu->pmu[cpu], g_cpu_pmu->event_count[cpu]);
		break;
	default:
		return NOTIFY_DONE;
	}
	return NOTIFY_OK;
}

struct notifier_block cpu_pm_pmu_notifier = {
	.notifier_call = cpu_pm_pmu_notify,
};
#endif


int wlc_mcu_pmu_init(void)
{
	/* CPU X2 Trained PMU list
	 * --pmu-cpu-evt=4,5,6,7:0x1B, 0x70,0x71,0x73,0x74,0x75,0x76,0x78,0x79
	 * --pmu-cpu-evt=0,1,2,3:0x70,0x71,0x73,0x76,0x78,0x79
	 */

	/* CPU X3 Trained PMU list
	 * model v1p0
	 * --pmu-cpu-evt=4,5,6,7:0x1B,0x70,0x71,0x73,0x74,0x75,0x76
	 * --pmu-cpu-evt=0,1,2,3:0x70,0x71,0x73,0x76,0x78,0x79
	 * model v2p1
	 * --pmu-cpu-evt=4,5,6,7:0x1B,0x70,0x71,0x73,0x74,0x75,0x76
	 * --pmu-cpu-evt=0,1,2,3:0x1B,0x70,0x71,0x73,0x75,0x76
	 * model v3p0
	 * --pmu-cpu-evt=4,5,6,7:0x1B,0x70,0x71,0x73,0x74,0x75,0x76
	 * --pmu-cpu-evt=0,1,2,3:0x1B,0x70,0x71,0x73,0x75,0x76
	 */

	int cpu;
	int nr_events;
	int nr_events_cpu_l = 6 + 1;
	int nr_events_cpu_b = 7 + 1;
	int trained_evt_l[MXNR_PMU_EVT_SZ] = {0x1B, 0x70, 0x71, 0x73, 0x75, 0x76, 0x11};
	int trained_evt_b[MXNR_PMU_EVT_SZ] = {0x1B, 0x70, 0x71, 0x73, 0x74, 0x75, 0x76, 0x11};
	int *event_list;
	int i;
	int nr_counters;
	int offset;
	unsigned int arg_nr;
	int		event_no;
	int		is_cpu_cycle_evt;

	struct pmu_data_info *pmu;

	/* init */
	g_cpu_pmu = cpu_pmu_hw_init();
	if (g_cpu_pmu == NULL) {
		pr_debug("[WLCDrv]Failed to init CPU PMU HW!!\n");
		return -1;
	}

	memset(g_cpu_pmu->cpu_pm_unpolled_loss, 0, sizeof(g_cpu_pmu->cpu_pm_unpolled_loss));
	cpu_pm_register_notifier(&cpu_pm_pmu_notifier);
	use_cpu_pm_pmu_notifier = 1;

	/* for each cpu in cpu_list, add all the events in event_list */
	for_each_online_cpu(cpu) {
		if (cpu < 0 || cpu >= MAX_NR_CPUS)
			continue;
		pmu = g_cpu_pmu->pmu[cpu];
		/*
		 * restore `nr_arg' from previous iteration,
		 * for cases when certain core's arguments consists more than one clauses
		 * e.g.,
		 *	   --pmu-cpu-evt=0:0x2b
		 *	   --pmu-cpu-evt=0:0x08
		 *	   --pmu-cpu-evt=0:0x16
		 */
		arg_nr = nr_arg[cpu];

		if (g_cpu_pmu->event_count[cpu] == 0)
			update_pmu_event_count(cpu);

		nr_counters = g_cpu_pmu->event_count[cpu];

		/* get event_list */
		if (cpu <= 3) { /*0, 1, 2, 3*/
			nr_events  = nr_events_cpu_l;
			event_list = trained_evt_l;
		} else {  /*4, 5, 6, 7*/
			nr_events = nr_events_cpu_b;
			event_list = trained_evt_b;
		}

		pr_info("[WLCDrv]evts:%d, cpu:%d, slot cnt=%d\n", nr_events, cpu, nr_counters);

		for (i = 0; i < nr_events; i++) {
			event_no = event_list[i];

			/*
			 * there're three possible cause of a failed pmu allocation:
			 *	   1. user asked more events than chip's capability
			 *	   2. part of the pmu registers was occupied by other users
			 *	   3. the requested cpu has been offline
			 *
			 * (we treat 1 and 2 as different cases for easier trouble shooting)
			 */

			/*
			 * skip duplicate events
			 * not treated as warning/error, this event was already
			 * registered anyway
			 */
			/* check cycle count event */
			if (event_no == 0x11 || event_no == 0xff) {
				/* allocate onto cycle count register (pmccntr_el0) ? */
				if (pmu[nr_counters-1].mode == MODE_POLLING)
					continue;

				/* allocated onto regular register as 0x11 ? */
				if (g_cpu_pmu->check_event(pmu, arg_nr, 0x11) < 0)
					continue;

				/* allocated onto regular register as 0xff ? */
				if (g_cpu_pmu->check_event(pmu, arg_nr, 0xff) < 0)
					continue;

			} else if (g_cpu_pmu->check_event(pmu, arg_nr, event_no) < 0) {
				/*
				 * check regular registers (pmccntr_el0 not checked)
				 */
				continue;
			}

			/*
			 * handle case (1) user asked more events than chip's capability.
			 *
			 * we just ignore it and display warning message in
			 * trace header
			 *
			 * as we removed all duplicate events, so we could never have a
			 * failed cycle counter allocation due to case (1), but only
			 * case (2)
			 */
			if (event_no != 0xff && event_no != 0x11 &&
				pmu[nr_counters-2].mode != MODE_DISABLED) {
				nr_ignored_arg[cpu]++;
				continue;
			}

			is_cpu_cycle_evt = 0;

			is_cpu_cycle_evt = (event_no == 0x11 || event_no == 0xff);

			if (is_cpu_cycle_evt) {
				offset = nr_counters-1;
			} else {
				offset = arg_nr;
				arg_nr++;
			}

			pmu[offset].mode = MODE_POLLING;
			pmu[offset].event = event_no;

			counter_cnt[cpu]++;
		} /* for i: 0 -> nr_events */

		nr_arg[cpu] = arg_nr;
	} /* for_each_possible_cpu(cpu) */

	return 0;
}


int wlc_mcu_pmu_deinit(void)
{
	int		cpu, i;
	int		event_count;
	struct pmu_data_info *pmu;

#if IS_ENABLED(CONFIG_CPU_PM)
	if (use_cpu_pm_pmu_notifier) {
		cpu_pm_unregister_notifier(&cpu_pm_pmu_notifier);
		use_cpu_pm_pmu_notifier = 0;
	}
#endif

	if (g_cpu_pmu == NULL) {
		pr_debug("[WLCDrv]CPU PMU HW is not initialized!!\n");
		return 0;
	}

	for_each_possible_cpu(cpu) {
		if (cpu < 0 || cpu >= MAX_NR_CPUS)
			continue;

		event_count = g_cpu_pmu->event_count[cpu];
		pmu = g_cpu_pmu->pmu[cpu];
		counter_cnt[cpu] = 0;
		nr_arg[cpu] = 0;
		nr_ignored_arg[cpu] = 0;
		for (i = 0; i < event_count; i++) {
			pmu[i].mode = MODE_DISABLED;
			pmu[i].event = 0;
		}
	}

	return 0;
}


int wlc_mcu_pmu_start(void)
{
	int cpu = raw_smp_processor_id();

	pr_info("[wlc]%s(%d)\n", __func__, cpu);
	if (g_cpu_pmu)
		g_cpu_pmu->start(g_cpu_pmu->pmu[cpu], g_cpu_pmu->event_count[cpu]);

	wlc_perf_cpupmu_status = 1;
	return 0;
}

int wlc_mcu_pmu_stop(void)
{
	int cpu = raw_smp_processor_id();

	wlc_perf_cpupmu_status = 0;

	pr_info("[wlc]%s(%d)\n", __func__, cpu);
	if (g_cpu_pmu)
		g_cpu_pmu->stop(g_cpu_pmu->event_count[cpu]);

	return 0;
}


/*=======================================================================*/
static int start;
static unsigned int online_cpu_map;

static void __wlc_hrtimer_register(void *unused)
{
	pr_info("[wlc] %s\n", __func__);
}

static void __wlc_hrtimer_stop(void *unused)
{
	pr_info("[wlc] %s\n", __func__);
	wlc_mcu_pmu_stop();
}


static void __wlc_init_cpu_related_device(void *unused)
{

	pr_info("[wlc] %s, c->ondiemet_start\n", __func__);
	wlc_mcu_pmu_start();
}

static int _wlc_pmu_cpu_notify_online(unsigned int cpu)
{
	pr_info("[wlc] %s\n", __func__);
	if (start == 0)
		return NOTIFY_OK;

	smp_call_function_single(cpu,
		__wlc_init_cpu_related_device, NULL, 1);
	smp_call_function_single(cpu,
		__wlc_hrtimer_register, NULL, 1);

	return 0;
}

static int _wlc_pmu_cpu_notify_offline(unsigned int cpu)
{
	pr_info("[wlc] %s\n", __func__);
	smp_call_function_single(cpu,
		__wlc_hrtimer_stop, NULL, 1);

	return 0;
}


int wlc_sampler_start(void)
{
	int ret, cpu;

	start = 0;

	ret = cpuhp_setup_state_nocalls(CPUHP_AP_ONLINE_DYN,
						   "start wlc sampler",
						   _wlc_pmu_cpu_notify_online,
						   _wlc_pmu_cpu_notify_offline);

	cpus_read_lock();
	online_cpu_map = 0;
	for_each_online_cpu(cpu) {
		online_cpu_map |= (1 << cpu);
	}

	start = 1;

	for_each_online_cpu(cpu) {
		smp_call_function_single(cpu,
			__wlc_init_cpu_related_device, NULL, 1);
	}


	for_each_online_cpu(cpu) {
		smp_call_function_single(cpu,
				__wlc_hrtimer_register, NULL, 1);
	}

	cpus_read_unlock();
	return 0;
}


int wlc_sampler_stop(void)
{
	int cpu;

	pr_info("[wlc] sampler_stop\n");
	cpus_read_lock();

	for_each_online_cpu(cpu) {
		smp_call_function_single(cpu,
			__wlc_hrtimer_stop, NULL, 1);
	}

	start = 0;
	cpuhp_remove_state_nocalls(CPUHP_AP_ONLINE_DYN);
	cpus_read_unlock();
	return 0;
}
