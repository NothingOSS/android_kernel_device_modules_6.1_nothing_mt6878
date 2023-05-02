// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2023 MediaTek Inc.
 */
#include <linux/sched/cputime.h>
#include <sched/sched.h>
#include <linux/cpuidle.h>
#include "../sugov/cpufreq.h"
#include "common.h"
#include "eas_plus.h"
#include "eas_trace.h"
#include "vip.h"
#include <mt-plat/mtk_irq_mon.h>

#if IS_ENABLED(CONFIG_RT_SOFTINT_OPTIMIZATION)
/*
 * Return whether the task on the given cpu is currently non-preemptible
 * while handling a potentially long softint, or if the task is likely
 * to block preemptions soon because it is a ksoftirq thread that is
 * handling slow softints.
 */
bool task_may_not_preempt(struct task_struct *task, int cpu)
{
	__u32 softirqs = per_cpu(active_softirqs, cpu) |
			local_softirq_pending();

	struct task_struct *cpu_ksoftirqd = per_cpu(ksoftirqd, cpu);

	return ((softirqs & LONG_SOFTIRQ_MASK) &&
		(task == cpu_ksoftirqd ||
		 task_thread_info(task)->preempt_count & SOFTIRQ_MASK));
}
#endif /* CONFIG_RT_SOFTINT_OPTIMIZATION */

#if IS_ENABLED(CONFIG_SMP)
static inline bool should_honor_rt_sync(struct rq *rq, struct task_struct *p,
					bool sync)
{
	/*
	 * If the waker is CFS, then an RT sync wakeup would preempt the waker
	 * and force it to run for a likely small time after the RT wakee is
	 * done. So, only honor RT sync wakeups from RT wakers.
	 */
	return sync && task_has_rt_policy(rq->curr) &&
		p->prio <= rq->rt.highest_prio.next &&
		rq->rt.rt_nr_running <= 2;
}
#else
static inline bool should_honor_rt_sync(struct rq *rq, struct task_struct *p,
					bool sync)
{
	return 0;
}
#endif

#if IS_ENABLED(CONFIG_UCLAMP_TASK)
/*
 * Verify the fitness of task @p to run on @cpu taking into account the uclamp
 * settings.
 *
 * This check is only important for heterogeneous systems where uclamp_min value
 * is higher than the capacity of a @cpu. For non-heterogeneous system this
 * function will always return true.
 *
 * The function will return true if the capacity of the @cpu is >= the
 * uclamp_min and false otherwise.
 *
 * Note that uclamp_min will be clamped to uclamp_max if uclamp_min
 * > uclamp_max.
 */
static inline bool rt_task_fits_capacity(struct task_struct *p, int cpu)
{
	unsigned int min_cap;
	unsigned int max_cap;
	unsigned int cpu_cap;

	/* Only heterogeneous systems can benefit from this check */
	if (!likely(mtk_sched_asym_cpucapacity))
		return true;

	min_cap = uclamp_eff_value(p, UCLAMP_MIN);
	max_cap = uclamp_eff_value(p, UCLAMP_MAX);

	cpu_cap = capacity_orig_of(cpu);

	return cpu_cap >= min(min_cap, max_cap);
}
#else
static inline bool rt_task_fits_capacity(struct task_struct *p, int cpu)
{
	return true;
}
#endif

static inline unsigned int mtk_task_cap(struct task_struct *p, int cpu,
					unsigned long min_cap, unsigned long max_cap)
{
	return  mtk_cpu_util(cpu, cpu_util_cfs(cpu), FREQUENCY_UTIL, p, min_cap, max_cap);
}

unsigned int min_highirq_load[NR_CPUS] = {
	[0 ... NR_CPUS-1] = SCHED_CAPACITY_SCALE /* default 1024 */
};

unsigned int inv_irq_ratio[NR_CPUS] = {
	[0 ... NR_CPUS-1] = 1 /* default irq=cpu */
};

inline int cpu_high_irqload(int cpu, unsigned long cpu_util)
{
	unsigned long irq_util;

	irq_util = cpu_util_irq(cpu_rq(cpu));
	if (irq_util < min_highirq_load[cpu])
		return 0;

	if (irq_util * inv_irq_ratio[cpu] < cpu_util)
		return 0;

	return 1;
}

inline unsigned int mtk_get_idle_exit_latency(int cpu)
{
	struct cpuidle_state *idle;

	/* CPU is idle */
	if (available_idle_cpu(cpu)) {

		idle = idle_get_state(cpu_rq(cpu));
		if (idle)
			return idle->exit_latency;

		/* CPU is in WFI */
		return 0;
	}

	/* CPU is not idle */
	return UINT_MAX;
}

static void mtk_rt_energy_aware_wake_cpu(struct task_struct *p,
			struct cpumask *lowest_mask, int ret, int *best_cpu, bool energy_eval)
{
	int cpu, best_idle_cpu_cluster;
	unsigned long util_cum[NR_CPUS] = {[0 ... NR_CPUS-1] = ULONG_MAX};
	unsigned long cpu_util_cum, best_cpu_util_cum = ULONG_MAX;
	unsigned long min_cap = uclamp_eff_value(p, UCLAMP_MIN);
	unsigned long max_cap = uclamp_eff_value(p, UCLAMP_MAX);
	unsigned long best_idle_exit_latency = UINT_MAX;
	unsigned long cpu_idle_exit_latency = UINT_MAX;
	int cluster, weight;
	int order_index, end_index;
	cpumask_t candidates;
	bool best_cpu_has_lt, cpu_has_lt;
	unsigned long pwr_eff, this_pwr_eff;

	mtk_get_gear_indicies(p, &order_index, &end_index);
	end_index = energy_eval ? end_index : 0;

	cpumask_copy(&candidates, lowest_mask);
	cpumask_clear(&candidates);

	/* No targets found */
	if (!ret)
		return;

	rcu_read_lock();
	for (cluster = 0; cluster < num_sched_clusters; cluster++) {
		best_idle_exit_latency = UINT_MAX;
		best_idle_cpu_cluster = -1;
		best_cpu_has_lt = true;

		for_each_cpu_and(cpu, lowest_mask, &cpu_array[order_index][cluster]) {

			cpu_util_cum = mtk_task_cap(p, cpu, min_cap, max_cap);

			trace_sched_cpu_util(cpu, cpu_util_cum);

			if (!cpumask_test_cpu(cpu, p->cpus_ptr))
				continue;

			if (cpu_paused(cpu))
				continue;

			if (cpu_high_irqload(cpu, cpu_util_cum))
				continue;

			/* RT task skips cpu that runs latency_sensitive or vip tasks */
#if IS_ENABLED(CONFIG_MTK_SCHED_VIP_TASK)
			cpu_has_lt = is_task_latency_sensitive(cpu_rq(cpu)->curr)
				|| task_is_vip(cpu_rq(cpu)->curr);
#else
			cpu_has_lt = is_task_latency_sensitive(cpu_rq(cpu)->curr);
#endif

			/*
			 * When the best cpu is suitable and the current is not,
			 * skip it
			 */
			if (cpu_has_lt && !best_cpu_has_lt)
				continue;

			/*
			 * If candidate CPU is the previous CPU, select it.
			 * Otherwise, if its load is same with best_cpu and in
			 * a shallow C-State, select it. If all above
			 * conditions are same, select the least cumulative
			 * window demand CPU.
			 */
			cpu_idle_exit_latency = mtk_get_idle_exit_latency(cpu);

			if (best_idle_exit_latency < cpu_idle_exit_latency)
				continue;

			if (best_idle_exit_latency == cpu_idle_exit_latency &&
					best_cpu_util_cum < cpu_util_cum)
				continue;

			best_idle_exit_latency = cpu_idle_exit_latency;
			best_cpu_util_cum = cpu_util_cum;
			util_cum[cpu] = cpu_util_cum;
			best_idle_cpu_cluster = cpu;
			best_cpu_has_lt = cpu_has_lt;
		}
		if (best_idle_cpu_cluster != -1)
			cpumask_set_cpu(best_idle_cpu_cluster, &candidates);

		if ((cluster >= end_index) && (!cpumask_empty(&candidates)))
			break;
	}

	weight = cpumask_weight(&candidates);
	if (!weight)
		goto unlock;

	if (weight == 1) {
		/* fast path */
		*best_cpu = cpumask_first(&candidates);
		goto unlock;
	} else {
		pwr_eff = ULONG_MAX;
		/* compare pwr_eff among clusters */
		for_each_cpu(cpu, &candidates) {
			cpu_util_cum = util_cum[cpu];
			this_pwr_eff = calc_pwr_eff(cpu, cpu_util_cum);

			if (trace_sched_aware_energy_rt_enabled()) {
				trace_sched_aware_energy_rt(cpu, this_pwr_eff,
						pwr_eff, cpu_util_cum);
			}

			if (this_pwr_eff < pwr_eff) {
				pwr_eff = this_pwr_eff;
				*best_cpu = cpu;
			}
		}
	}

unlock:
	rcu_read_unlock();
}

DEFINE_PER_CPU(cpumask_var_t, mtk_select_rq_rt_mask);

void mtk_select_task_rq_rt(void *data, struct task_struct *p, int source_cpu,
				int sd_flag, int flags, int *target_cpu)
{
	struct task_struct *curr;
	struct rq *rq, *this_cpu_rq;
	bool may_not_preempt;
	bool sync = !!(flags & WF_SYNC);
	int ret, target = -1, this_cpu, select_reason = -1;
	struct cpumask *lowest_mask = this_cpu_cpumask_var_ptr(mtk_select_rq_rt_mask);
	/* remove in next update */
	unsigned int cfs_cpus = 0;
	unsigned int idle_cpus = 0;

	irq_log_store();

	/* For anything but wake ups, just return the task_cpu */
	if (!(flags & (WF_TTWU | WF_FORK))) {
		if (!cpu_paused(source_cpu)) {
			select_reason = LB_RT_FAIL;
			*target_cpu = source_cpu;
			goto out;
		}
	}

	irq_log_store();

	this_cpu = smp_processor_id();
	this_cpu_rq = cpu_rq(this_cpu);

	/*
	 * Respect the sync flag as long as the task can run on this CPU.
	 */
	if (should_honor_rt_sync(this_cpu_rq, p, sync) &&
	    cpumask_test_cpu(this_cpu, p->cpus_ptr)) {
		*target_cpu = this_cpu;
		select_reason = LB_RT_SYNC;
		goto out;
	}

	irq_log_store();

	rq = cpu_rq(source_cpu);

	rcu_read_lock();
	/* unlocked access */
	curr = READ_ONCE(rq->curr);

	/* check source_cpu status */
	may_not_preempt = task_may_not_preempt(curr, source_cpu);

	ret = cpupri_find_fitness(&task_rq(p)->rd->cpupri, p,
				lowest_mask, rt_task_fits_capacity);

	cpumask_andnot(lowest_mask, lowest_mask, cpu_pause_mask);
	mtk_rt_energy_aware_wake_cpu(p, lowest_mask, ret, &target, true);

	if (target != -1 &&
		(may_not_preempt || p->prio < cpu_rq(target)->rt.highest_prio.curr)) {
		*target_cpu = target;
		select_reason = LB_RT_IDLE;
	} else {
		/* previous CPU as backup */
		*target_cpu = source_cpu;
		select_reason = LB_RT_SOURCE_CPU;
	}

	rcu_read_unlock();
	irq_log_store();
out:
	if (trace_sched_select_task_rq_rt_enabled())
		trace_sched_select_task_rq_rt(p, select_reason, *target_cpu, idle_cpus, cfs_cpus,
					lowest_mask, sd_flag, sync);
	irq_log_store();
}

void mtk_find_lowest_rq(void *data, struct task_struct *p, struct cpumask *lowest_mask,
			int ret, int *lowest_cpu)
{
	int cpu, source_cpu;
	int this_cpu = smp_processor_id();
	cpumask_t avail_lowest_mask;
	int target = -1, select_reason = -1;

	irq_log_store();

	cpumask_andnot(&avail_lowest_mask, lowest_mask, cpu_pause_mask);
	if (!ret) {
		select_reason = LB_RT_NO_LOWEST_RQ;
		goto out; /* No targets found */
	}
	irq_log_store();

	source_cpu = task_cpu(p);

	 /* Skip source_cpu if it is not among lowest */
	if (!cpumask_test_cpu(source_cpu, &avail_lowest_mask))
		source_cpu = -1;

	/* Skip this_cpu if it is not among lowest */
	if (!cpumask_test_cpu(this_cpu, &avail_lowest_mask))
		this_cpu = -1;

	mtk_rt_energy_aware_wake_cpu(p, &avail_lowest_mask, ret, &target, false);

	irq_log_store();

	/* best energy cpu found */
	if (target != -1) {
		*lowest_cpu = target;
		select_reason = LB_RT_IDLE;
		goto out;
	}

	irq_log_store();

	/* use prev cpu as target */
	if (source_cpu !=  -1) {
		*lowest_cpu = source_cpu;
		select_reason = LB_RT_SOURCE_CPU;
		goto out;
	}

	irq_log_store();

	/*
	 * And finally, if there were no matches within the domains
	 * just give the caller *something* to work with from the compatible
	 * locations.
	 */
	if (this_cpu != -1) {
		*lowest_cpu = this_cpu;
		select_reason = LB_RT_FAIL_SYNC;
		goto out;
	}

	irq_log_store();

	cpu = cpumask_any_and_distribute(&avail_lowest_mask, cpu_possible_mask);
	if (cpu < nr_cpu_ids) {
		*lowest_cpu = cpu;
		select_reason = LB_RT_FAIL_RANDOM;
		goto out;
	}

	irq_log_store();

	/* Let find_lowest_rq not to choose dst_cpu */
	*lowest_cpu = -1;
	select_reason = LB_RT_FAIL;
	cpumask_clear(lowest_mask);

out:
	irq_log_store();

	if (trace_sched_find_lowest_rq_enabled())
		trace_sched_find_lowest_rq(p, select_reason, *lowest_cpu,
				&avail_lowest_mask, lowest_mask);

	irq_log_store();
}
