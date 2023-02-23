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
static inline bool mtk_rt_task_fits_capacity(struct task_struct *p, int cpu,
					unsigned long min_cap, unsigned long max_cap)
{
	unsigned int cpu_cap;
	unsigned long util;
	struct rq *rq;

	/* Only heterogeneous systems can benefit from this check */
	if (!likely(mtk_sched_asym_cpucapacity))
		return true;

	rq = cpu_rq(cpu);
	/* refer code from sched.h: effective_cpu_util -> cpu_util_rt */
	util = cpu_util_rt(rq);
	util = mtk_uclamp_rq_util_with(rq, util, p, min_cap, max_cap);
	cpu_cap = capacity_orig_of(cpu);

	return cpu_cap >= clamp_val(util, min_cap, max_cap);
}
#else
static inline bool mtk_rt_task_fits_capacity(struct task_struct *p, int cpu,
					unsigned long min_cap, unsigned long max_cap)
{
	return true;
}
#endif

static inline unsigned int mtk_task_cap(struct task_struct *p, int cpu,
					unsigned long min_cap, unsigned long max_cap)
{
	return  mtk_cpu_util(cpu, cpu_util_cfs(cpu), FREQUENCY_UTIL, p, min_cap, max_cap);
}

void mtk_select_task_rq_rt(void *data, struct task_struct *p, int source_cpu,
				int sd_flag, int flags, int *target_cpu)
{
	struct task_struct *curr;
	struct rq *rq;
	int lowest_cpu = -1, rt_lowest_cpu = -1;
	int lowest_prio = 0, rt_lowest_prio = p->prio;
	int cpu;
	int select_reason = -1;
	bool sync = !!(flags & WF_SYNC);
	struct rq *this_cpu_rq;
	int this_cpu;
	bool test, may_not_preempt;
	struct cpuidle_state *idle;
	unsigned int min_exit_lat;
	struct root_domain *rd = cpu_rq(smp_processor_id())->rd;
	struct perf_domain *pd;
	unsigned int cpu_util;
	unsigned long occupied_cap, occupied_cap_per_gear;
	int best_idle_cpu_per_gear;
	int best_idle_cpu = -1;
	unsigned long pwr_eff = ULONG_MAX;
	unsigned long this_pwr_eff = ULONG_MAX;
	unsigned long min_cap = uclamp_eff_value(p, UCLAMP_MIN);
	unsigned long max_cap = uclamp_eff_value(p, UCLAMP_MAX);
	unsigned int cfs_cpus = 0;
	unsigned int idle_cpus = 0;

	*target_cpu = -1;
	/* For anything but wake ups, just return the task_cpu */
	if (!(flags & (WF_TTWU | WF_FORK))) {
		if (!cpu_paused(source_cpu)) {
			select_reason = LB_RT_FAIL;
			*target_cpu = source_cpu;
			goto out;
		}
	}

	rcu_read_lock();
	this_cpu = smp_processor_id();
	this_cpu_rq = cpu_rq(this_cpu);

	/*
	 * Respect the sync flag as long as the task can run on this CPU.
	 */
	if (should_honor_rt_sync(this_cpu_rq, p, sync) &&
	    cpumask_test_cpu(this_cpu, p->cpus_ptr)) {
		*target_cpu = this_cpu;
		select_reason = LB_RT_SYNC;
		goto unlock;
	}

	/*
	 * Select one CPU from each cluster and
	 * compare its power / capacity.
	 */
	pd = rcu_dereference(rd->pd);
	if (!pd) {
		select_reason = LB_RT_FAIL_PD;
		goto source;
	}

	for (; pd; pd = pd->next) {
		min_exit_lat = UINT_MAX;
		occupied_cap_per_gear = ULONG_MAX;
		best_idle_cpu_per_gear = -1;
		for_each_cpu_and(cpu, perf_domain_span(pd), cpu_active_mask) {
			if (!cpumask_test_cpu(cpu, p->cpus_ptr))
				continue;

			if (cpu_paused(cpu))
				continue;

			if (!mtk_rt_task_fits_capacity(p, cpu, min_cap, max_cap))
				continue;

			if (available_idle_cpu(cpu)) {
				/* WFI > non-WFI */
				idle_cpus = (idle_cpus | (1 << cpu));
				idle = idle_get_state(cpu_rq(cpu));
				occupied_cap = mtk_task_cap(p, cpu, min_cap, max_cap);
				if (idle) {
					/* non WFI, find shortest exit_latency */
					if (idle->exit_latency < min_exit_lat) {
						min_exit_lat = idle->exit_latency;
						best_idle_cpu_per_gear = cpu;
						occupied_cap_per_gear = occupied_cap;
					} else if ((idle->exit_latency == min_exit_lat)
						&& (occupied_cap_per_gear > occupied_cap)) {
						best_idle_cpu_per_gear = cpu;
						occupied_cap_per_gear = occupied_cap;
					}
				} else {
					/* WFI, find max_spare_cap (least occupied_cap) */
					if (min_exit_lat > 0) {
						min_exit_lat = 0;
						best_idle_cpu_per_gear = cpu;
						occupied_cap_per_gear = occupied_cap;
					} else if (occupied_cap_per_gear > occupied_cap) {
						best_idle_cpu_per_gear = cpu;
						occupied_cap_per_gear = occupied_cap;
					}
				}
				continue;
			}
			rq = cpu_rq(cpu);
			curr = rq->curr;
			if (curr && (curr->policy == SCHED_NORMAL)
					&& (curr->prio > lowest_prio)
					&& (!task_may_not_preempt(curr, cpu))) {
				lowest_prio = curr->prio;
				lowest_cpu = cpu;
				cfs_cpus = (cfs_cpus | (1 << cpu));
			}
		}
		if (best_idle_cpu_per_gear != -1) {
			cpu_util = occupied_cap_per_gear;
			this_pwr_eff = calc_pwr_eff(best_idle_cpu_per_gear, cpu_util);

			if (trace_sched_aware_energy_rt_enabled()) {
				trace_sched_aware_energy_rt(best_idle_cpu_per_gear, this_pwr_eff,
						pwr_eff, cpu_util);
			}

			if (this_pwr_eff < pwr_eff) {
				pwr_eff = this_pwr_eff;
				best_idle_cpu = best_idle_cpu_per_gear;
			}
		}
	}

	if (best_idle_cpu != -1) {
		*target_cpu = best_idle_cpu;
		select_reason = LB_RT_IDLE;
		goto unlock;
	}

	if (lowest_cpu != -1) {
		*target_cpu =  lowest_cpu;
		select_reason = LB_RT_LOWEST_PRIO_NORMAL;
		goto unlock;
	}

source:
	rq = cpu_rq(source_cpu);
	/* unlocked access */
	curr = READ_ONCE(rq->curr);
	/* check source_cpu status */
	may_not_preempt = task_may_not_preempt(curr, source_cpu);
	test = (curr && (may_not_preempt ||
			 (unlikely(rt_task(curr)) &&
			  (curr->nr_cpus_allowed < 2 || curr->prio <= p->prio))));

	if (!test && mtk_rt_task_fits_capacity(p, source_cpu, min_cap, max_cap)) {
		select_reason = ((select_reason == -1) ? LB_RT_SOURCE_CPU : select_reason);
		*target_cpu = source_cpu;
	}

unlock:
	rcu_read_unlock();

	/* if no cpu fufill condition above,
	 * then select cpu with lowest prioity
	 */
	if (-1 == *target_cpu) {
		for_each_cpu(cpu, p->cpus_ptr) {

			if (cpu_paused(cpu))
				continue;

			if (!mtk_rt_task_fits_capacity(p, cpu, min_cap, max_cap))
				continue;

			rq = cpu_rq(cpu);
			curr = rq->curr;
			if (curr && rt_task(curr)
					&& (curr->prio > rt_lowest_prio)) {
				rt_lowest_prio = curr->prio;
				rt_lowest_cpu = cpu;
			}
		}
		*target_cpu =  rt_lowest_cpu;
		select_reason = LB_RT_LOWEST_PRIO_RT;
	}

out:
	if (trace_sched_select_task_rq_rt_enabled())
		trace_sched_select_task_rq_rt(p, select_reason, *target_cpu, idle_cpus, cfs_cpus,
					sd_flag, sync);
}

void mtk_find_lowest_rq(void *data, struct task_struct *p, struct cpumask *lowest_mask,
			int ret, int *lowest_cpu)
{
	int cpu = -1;
	int this_cpu = smp_processor_id();
	cpumask_t avail_lowest_mask;
	int lowest_prio_cpu = -1, lowest_prio = 0;
	int select_reason = -1;
	unsigned int gear_id, nr_gear;
	struct cpumask *gear_cpus;

	cpumask_andnot(&avail_lowest_mask, lowest_mask, cpu_pause_mask);
	if (!ret) {
		select_reason = LB_RT_NO_LOWEST_RQ;
		goto out; /* No targets found */
	}

	cpu = task_cpu(p);

	/*
	 * At this point we have built a mask of CPUs representing the
	 * lowest priority tasks in the system.  Now we want to elect
	 * the best one based on our affinity and topology.
	 *
	 * We prioritize the last CPU that the task executed on since
	 * it is most likely cache-hot in that location.
	 */
	if (cpumask_test_cpu(cpu, &avail_lowest_mask)) {
		*lowest_cpu = cpu;
		select_reason = LB_RT_SOURCE_CPU;
		goto out;
	}

	/*
	 * Otherwise, we consult the sched_domains span maps to figure
	 * out which CPU is logically closest to our hot cache data.
	 */
	if (!cpumask_test_cpu(this_cpu, &avail_lowest_mask))
		this_cpu = -1; /* Skip this_cpu opt if not among lowest */

	nr_gear = get_nr_gears();

	/* Find best_cpu on same cluster with task_cpu(p) */
	for (gear_id = 0; gear_id < nr_gear; gear_id++) {
		gear_cpus = get_gear_cpumask(gear_id);

		/*
		 * "this_cpu" is cheaper to preempt than a
		 * remote processor.
		 */
		if (this_cpu != -1 && cpumask_test_cpu(this_cpu, gear_cpus)) {
			*lowest_cpu = this_cpu;
			select_reason = LB_RT_SYNC;
			goto out;
		}

		for_each_cpu_and(cpu, &avail_lowest_mask, gear_cpus) {
			struct task_struct *curr;

			if (available_idle_cpu(cpu)) {
				*lowest_cpu = cpu;
				select_reason = LB_RT_IDLE;
				goto out;
			}
			curr = cpu_curr(cpu);
			/* &fair_sched_class undefined in scheduler.ko */
			if (fair_policy(curr->policy) && (curr->prio > lowest_prio)) {
				lowest_prio = curr->prio;
				lowest_prio_cpu = cpu;
			}
		}
	}

	if (lowest_prio_cpu != -1) {
		*lowest_cpu = lowest_prio_cpu;
		select_reason = LB_RT_LOWEST_PRIO;
		goto out;
	}

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

	cpu = cpumask_any_and_distribute(&avail_lowest_mask, cpu_possible_mask);
	if (cpu < nr_cpu_ids) {
		*lowest_cpu = cpu;
		select_reason = LB_RT_FAIL_RANDOM;
		goto out;
	}

	/* Let find_lowest_rq not to choose dst_cpu */
	*lowest_cpu = -1;
	select_reason = LB_RT_FAIL;
	cpumask_clear(lowest_mask);

out:
	if (trace_sched_find_lowest_rq_enabled())
		trace_sched_find_lowest_rq(p, select_reason, *lowest_cpu,
				&avail_lowest_mask, lowest_mask);
}
