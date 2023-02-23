// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */
#include <linux/cpuidle.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <trace/hooks/sched.h>
#include <linux/sched/cputime.h>
#include <sched/sched.h>
#include <linux/sched/clock.h>
#include "eas/eas_plus.h"
#include "sugov/cpufreq.h"
#include "sugov/dsu_interface.h"
#if IS_ENABLED(CONFIG_MTK_GEARLESS_SUPPORT)
#include "mtk_energy_model/v2/energy_model.h"
#else
#include "mtk_energy_model/v1/energy_model.h"
#endif
#include "common.h"
#include <sched/pelt.h>
#include <linux/stop_machine.h>
#include <linux/kthread.h>
#if IS_ENABLED(CONFIG_MTK_THERMAL_INTERFACE)
#include <thermal_interface.h>
#endif

#define CREATE_TRACE_POINTS
#include "sched_trace.h"

MODULE_LICENSE("GPL");

/*
 * Unsigned subtract and clamp on underflow.
 *
 * Explicitly do a load-store to ensure the intermediate value never hits
 * memory. This allows lockless observations without ever seeing the negative
 * values.
 */
#define sub_positive(_ptr, _val) do {				\
	typeof(_ptr) ptr = (_ptr);				\
	typeof(*ptr) val = (_val);				\
	typeof(*ptr) res, var = READ_ONCE(*ptr);		\
	res = var - val;					\
	if (res > var)						\
		res = 0;					\
	WRITE_ONCE(*ptr, res);					\
} while (0)

/*
 * Remove and clamp on negative, from a local variable.
 *
 * A variant of sub_positive(), which does not use explicit load-store
 * and is thus optimized for local variable updates.
 */
#define lsub_positive(_ptr, _val) do {				\
	typeof(_ptr) ptr = (_ptr);				\
	*ptr -= min_t(typeof(*ptr), *ptr, _val);		\
} while (0)

#ifdef CONFIG_SMP
static inline unsigned long task_util(struct task_struct *p)
{
	return READ_ONCE(p->se.avg.util_avg);
}

static inline unsigned long _task_util_est(struct task_struct *p)
{
	struct util_est ue = READ_ONCE(p->se.avg.util_est);

	return max(ue.ewma, (ue.enqueued & ~UTIL_AVG_UNCHANGED));
}

static inline unsigned long task_util_est(struct task_struct *p)
{
	if (sched_feat(UTIL_EST) && is_util_est_enable())
		return max(task_util(p), _task_util_est(p));
	return task_util(p);
}

#ifdef CONFIG_UCLAMP_TASK
static inline unsigned long uclamp_task_util(struct task_struct *p)
{
	return clamp(task_util_est(p),
		     uclamp_eff_value(p, UCLAMP_MIN),
		     uclamp_eff_value(p, UCLAMP_MAX));
}
#else
static inline unsigned long uclamp_task_util(struct task_struct *p)
{
	return task_util_est(p);
}
#endif

int task_fits_capacity(struct task_struct *p, long capacity)
{
	return fits_capacity(uclamp_task_util(p), capacity);
}

unsigned long capacity_of(int cpu)
{
	return cpu_rq(cpu)->cpu_capacity;

}

unsigned long cpu_util(int cpu)
{
	struct cfs_rq *cfs_rq;
	unsigned int util;

	cfs_rq = &cpu_rq(cpu)->cfs;
	util = READ_ONCE(cfs_rq->avg.util_avg);

	if (sched_feat(UTIL_EST) && is_util_est_enable())
		util = max(util, READ_ONCE(cfs_rq->avg.util_est.enqueued));

	return min_t(unsigned long, util, capacity_orig_of(cpu));
}

#if IS_ENABLED(CONFIG_MTK_EAS)
/*
 * Predicts what cpu_util(@cpu) would return if @p was migrated (and enqueued)
 * to @dst_cpu.
 */
static unsigned long cpu_util_next(int cpu, struct task_struct *p, int dst_cpu)
{
	struct cfs_rq *cfs_rq = &cpu_rq(cpu)->cfs;
	unsigned long util_est, util = READ_ONCE(cfs_rq->avg.util_avg);

	/*
	 * If @p migrates from @cpu to another, remove its contribution. Or,
	 * if @p migrates from another CPU to @cpu, add its contribution. In
	 * the other cases, @cpu is not impacted by the migration, so the
	 * util_avg should already be correct.
	 */
	if (task_cpu(p) == cpu && dst_cpu != cpu)
		lsub_positive(&util, task_util(p));
	else if (task_cpu(p) != cpu && dst_cpu == cpu)
		util += task_util(p);

	if (sched_feat(UTIL_EST) && is_util_est_enable()) {
		util_est = READ_ONCE(cfs_rq->avg.util_est.enqueued);

		/*
		 * During wake-up, the task isn't enqueued yet and doesn't
		 * appear in the cfs_rq->avg.util_est.enqueued of any rq,
		 * so just add it (if needed) to "simulate" what will be
		 * cpu_util() after the task has been enqueued.
		 */
		if (dst_cpu == cpu)
			util_est += _task_util_est(p);

		util = max(util, util_est);
	}

	return min(util, capacity_orig_of(cpu));
}

/*
 * Compute the task busy time for compute_energy(). This time cannot be
 * injected directly into effective_cpu_util() because of the IRQ scaling.
 * The latter only makes sense with the most recent CPUs where the task has
 * run.
 */
static inline void eenv_task_busy_time(struct energy_env *eenv,
				       struct task_struct *p, int prev_cpu)
{
	unsigned long busy_time, max_cap = arch_scale_cpu_capacity(prev_cpu);
	unsigned long irq = cpu_util_irq(cpu_rq(prev_cpu));

	if (unlikely(irq >= max_cap))
		busy_time = max_cap;
	else
		busy_time = scale_irq_capacity(task_util_est(p), irq, max_cap);

	eenv->task_busy_time = busy_time;
}

DEFINE_PER_CPU(cpumask_var_t, mtk_select_rq_mask);
static inline void eenv_init(struct energy_env *eenv,
			struct task_struct *p, int prev_cpu, struct perf_domain *pd)
{
	struct cpumask *cpus = this_cpu_cpumask_var_ptr(mtk_select_rq_mask);
	unsigned int cpu, pd_idx, pd_cnt;
	struct perf_domain *pd_ptr = pd;
	unsigned int gear_idx;
	struct dsu_info *dsu;
	unsigned int dsu_opp;
	struct dsu_state *dsu_ps;

	eenv_task_busy_time(eenv, p, prev_cpu);

	pd_cnt = get_nr_gears();
	for (pd_idx = 0; pd_idx < pd_cnt; pd_idx++) {
		eenv->pds_busy_time[pd_idx] =  -1;
		eenv->pds_max_util[pd_idx][0] =  -1;
		eenv->pds_max_util[pd_idx][1] =  -1;
		eenv->pds_cpu_cap[pd_idx] = -1;
		eenv->pds_cap[pd_idx] = -1;
	}

	for (; pd_ptr; pd_ptr = pd_ptr->next) {
		unsigned long cpu_thermal_cap;

		cpumask_and(cpus, perf_domain_span(pd_ptr), cpu_active_mask);
		if (cpumask_empty(cpus))
			continue;

		/* Account thermal pressure for the energy estimation */
		cpu = cpumask_first(cpus);
		cpu_thermal_cap = arch_scale_cpu_capacity(cpu);
		cpu_thermal_cap -= arch_scale_thermal_pressure(cpu);

		gear_idx = per_cpu(gear_id, cpu);
		eenv->pds_cpu_cap[gear_idx] = cpu_thermal_cap;
		eenv->pds_cap[gear_idx] = 0;
		for_each_cpu(cpu, cpus) {
			eenv->pds_cap[gear_idx] += cpu_thermal_cap;
		}

		if (trace_sched_energy_init_enabled()) {
			trace_sched_energy_init(cpus, gear_idx, eenv->pds_cpu_cap[gear_idx],
				eenv->pds_cap[gear_idx]);
		}
	}

	for_each_cpu(cpu, cpu_possible_mask) {
		eenv->cpu_temp[cpu] = get_cpu_temp(cpu);
		eenv->cpu_temp[cpu] /= 1000;
	}

	eenv->wl_support = is_wl_support();
	if (eenv->wl_support) {
		eenv->wl_type = get_em_wl();

		dsu = &(eenv->dsu);
		dsu->dsu_bw = get_pelt_dsu_bw();
		dsu->emi_bw = get_pelt_emi_bw();
	/*	dsu->temp = get_dsu_temp()/1000; */
		dsu->temp = (eenv->cpu_temp[1] + eenv->cpu_temp[3])/2000;
		eenv->dsu_freq_base = mtk_get_dsu_freq();
		dsu_opp = dsu_get_freq_opp(eenv->wl_type, eenv->dsu_freq_base);
		dsu_ps = dsu_get_opp_ps(eenv->wl_type, dsu_opp);
		eenv->dsu_volt_base = dsu_ps->volt;

		if (trace_sched_eenv_init_enabled())
			trace_sched_eenv_init(eenv->dsu_freq_base, eenv->dsu_volt_base,
					share_buck.gear_idx);
	}
}

/*
 * Compute the perf_domain (PD) busy time for compute_energy(). Based on the
 * utilization for each @pd_cpus, it however doesn't take into account
 * clamping since the ratio (utilization / cpu_capacity) is already enough to
 * scale the EM reported power consumption at the (eventually clamped)
 * cpu_capacity.
 *
 * The contribution of the task @p for which we want to estimate the
 * energy cost is removed (by cpu_util_next()) and must be calculated
 * separately (see eenv_task_busy_time). This ensures:
 *
 *   - A stable PD utilization, no matter which CPU of that PD we want to place
 *     the task on.
 *
 *   - A fair comparison between CPUs as the task contribution (task_util())
 *     will always be the same no matter which CPU utilization we rely on
 *     (util_avg or util_est).
 *
 * Set @eenv busy time for the PD that spans @pd_cpus. This busy time can't
 * exceed @eenv->pd_cap.
 */
static inline void eenv_pd_busy_time(int gear_idx, struct energy_env *eenv,
				struct cpumask *pd_cpus,
				struct task_struct *p)
{
	unsigned long busy_time = 0;
	int cpu;

	if (eenv->pds_busy_time[gear_idx] != -1) {
		eenv->pd_busy_time = eenv->pds_busy_time[gear_idx];
		return;
	}

	for_each_cpu(cpu, pd_cpus) {
		unsigned long util = cpu_util_next(cpu, p, -1);

#if IS_ENABLED(CONFIG_MTK_CPUFREQ_SUGOV_EXT)
		busy_time = mtk_cpu_util(cpu, util, ENERGY_UTIL,
					NULL, eenv->min_cap, eenv->max_cap);
#else
		busy_time += effective_cpu_util(cpu, util, ENERGY_UTIL, NULL);
#endif
	}

	eenv->pd_busy_time = min(eenv->pds_cap[gear_idx], busy_time);
	eenv->pds_busy_time[gear_idx] = eenv->pd_busy_time;
}

/*
 * Compute the maximum utilization for compute_energy() when the task @p
 * is placed on the cpu @dst_cpu.
 *
 * Returns the maximum utilization among @eenv->cpus. This utilization can't
 * exceed @eenv->cpu_cap.
 */
static inline unsigned long
eenv_pd_max_util(int gear_idx, struct energy_env *eenv, struct cpumask *pd_cpus,
		 struct task_struct *p, int dst_cpu)
{
	unsigned long max_util = 0, max_util_base = 0;
	int cpu, dst_idx = 0;

	if (dst_cpu != -1)
		dst_idx = 1;

	if (eenv->pds_max_util[gear_idx][dst_idx] != -1)
		return  eenv->pds_max_util[gear_idx][dst_idx];

	for_each_cpu(cpu, pd_cpus) {
		struct task_struct *tsk = (cpu == dst_cpu) ? p : NULL;
		unsigned long util = cpu_util_next(cpu, p, dst_cpu);
		unsigned long cpu_util;

		/*
		 * Performance domain frequency: utilization clamping
		 * must be considered since it affects the selection
		 * of the performance domain frequency.
		 * NOTE: in case RT tasks are running, by default the
		 * FREQUENCY_UTIL's utilization can be max OPP.
		 */
#if IS_ENABLED(CONFIG_MTK_CPUFREQ_SUGOV_EXT)
		cpu_util = mtk_cpu_util(cpu, util, FREQUENCY_UTIL, tsk, eenv->min_cap,
				eenv->max_cap);
#else
		cpu_util = effective_cpu_util(cpu, util, FREQUENCY_UTIL, tsk);
#endif
		if (dst_cpu != -1) {
			unsigned long util_base = cpu_util_next(cpu, p, -1);
			unsigned long cpu_util_base;

			cpu_util_base = mtk_cpu_util(cpu, util_base, FREQUENCY_UTIL, tsk,
						eenv->min_cap, eenv->max_cap);
			max_util_base = max(max_util_base, cpu_util_base);
		}

		max_util = max(max_util, cpu_util);

		if (trace_sched_max_util_enabled())
			trace_sched_max_util(gear_idx, dst_cpu, max_util, cpu, util, cpu_util);
	}

	eenv->pds_max_util[gear_idx][dst_idx] =  min(max_util, eenv->pds_cpu_cap[gear_idx]);
	if (dst_cpu != -1)
		eenv->pds_max_util[gear_idx][0] =  min(max_util_base, eenv->pds_cpu_cap[gear_idx]);
	trace_sched_max_util(gear_idx, dst_cpu, eenv->pds_max_util[gear_idx][dst_idx], -1, 0, 0);

	return eenv->pds_max_util[gear_idx][dst_idx];
}

static inline unsigned long
mtk_compute_energy_cpu(int gear_idx, struct energy_env *eenv, struct perf_domain *pd,
		       struct cpumask *pd_cpus, struct task_struct *p, int dst_cpu)
{
	unsigned long max_util = eenv_pd_max_util(gear_idx, eenv, pd_cpus, p, dst_cpu);
	unsigned long busy_time = eenv->pd_busy_time;
	unsigned long energy;

	if (dst_cpu >= 0)
		busy_time = min(eenv->pds_cap[gear_idx], busy_time + eenv->task_busy_time);

	energy =  mtk_em_cpu_energy(gear_idx, pd->em_pd, max_util, busy_time,
			eenv->pds_cpu_cap[gear_idx], eenv);

	if (trace_sched_compute_energy_enabled())
		trace_sched_compute_energy(dst_cpu, gear_idx, pd_cpus, energy, max_util, busy_time);

	return energy;
}

struct share_buck_info share_buck;
int init_share_buck(void)
{
	struct root_domain *rd;
	struct perf_domain *pd;
	int ret;
	struct device_node *eas_node;

	eas_node = of_find_node_by_name(NULL, "eas_info");
	if (eas_node == NULL) {
		pr_info("failed to find node @ %s\n", __func__);
		return -ENODEV;
	}

	share_buck.gear_idx = 0;
	ret = of_property_read_u32(eas_node, "share-buck", &share_buck.gear_idx);
	if (ret < 0)
		pr_info("no share_buck err_code=%d %s\n", ret,  __func__);

	preempt_disable();
	rd = cpu_rq(smp_processor_id())->rd;
	preempt_enable();
	rcu_read_lock();
	pd = rcu_dereference(rd->pd);
	if (!pd)
		goto unlock;

	share_buck.cpus = get_gear_cpumask(share_buck.gear_idx);
	for (; pd; pd = pd->next) {
		struct cpumask *pd_mask = perf_domain_span(pd);
		int cpu = cpumask_first(pd_mask);

		if (share_buck.gear_idx == per_cpu(gear_id, cpu)) {
			share_buck.pd = pd;
			break;
		}
	}
unlock:
	rcu_read_unlock();

	return 0;
}

static inline int shared_gear(int gear_idx)
{
	return gear_idx == share_buck.gear_idx;
}

static inline unsigned long
mtk_compute_energy_cpu_dsu(struct energy_env *eenv, struct perf_domain *pd,
	       struct cpumask *pd_cpus, struct task_struct *p, int dst_cpu)
{
	unsigned long cpu_pwr = 0, dsu_pwr = 0;
	unsigned long shared_pwr_base, shared_pwr_new, delta_share_pwr = 0;
	struct dsu_info *dsu = &eenv->dsu;

	dsu->dsu_freq = eenv->dsu_freq_base;
	dsu->dsu_volt = eenv->dsu_volt_base;
	cpu_pwr = mtk_compute_energy_cpu(eenv->gear_idx, eenv, pd, pd_cpus, p, dst_cpu);

	if ((eenv->dsu_freq_new  > eenv->dsu_freq_base) && !(shared_gear(eenv->gear_idx))
			&& share_buck.gear_idx != -1) {

		eenv_pd_busy_time(share_buck.gear_idx, eenv, share_buck.cpus, p);

		/* calculate share_buck gear pwr with new DSU freq */
		dsu->dsu_freq  = eenv->dsu_freq_new;
		dsu->dsu_volt = eenv->dsu_volt_new;
		shared_pwr_new = mtk_compute_energy_cpu(share_buck.gear_idx, eenv, share_buck.pd,
							share_buck.cpus, p, -1);

		/* calculate share_buck gear pwr with new old freq */
		dsu->dsu_freq = eenv->dsu_freq_base;
		dsu->dsu_volt = eenv->dsu_volt_base;
		shared_pwr_base = mtk_compute_energy_cpu(share_buck.gear_idx, eenv, share_buck.pd,
							share_buck.cpus, p, -1);

		delta_share_pwr = shared_pwr_new - shared_pwr_base;
	}

	if (dst_cpu != -1) {
		if (trace_sched_compute_energy_dsu_enabled())
			trace_sched_compute_energy_dsu(dst_cpu, eenv->task_busy_time,
				eenv->pd_busy_time, dsu->dsu_bw, dsu->emi_bw, dsu->temp,
				dsu->dsu_freq, dsu->dsu_volt);

		dsu_pwr = get_dsu_pwr(eenv->wl_type, dst_cpu, eenv->task_busy_time,
					eenv->pd_busy_time, dsu);
		if (trace_sched_compute_energy_cpu_dsu_enabled())
			trace_sched_compute_energy_cpu_dsu(dst_cpu, cpu_pwr, delta_share_pwr,
						dsu_pwr, cpu_pwr + delta_share_pwr + dsu_pwr);
	}

	return cpu_pwr + delta_share_pwr + dsu_pwr;
}

/*
 * compute_energy(): Use the Energy Model to estimate the energy that @pd would
 * consume for a given utilization landscape @eenv. When @dst_cpu < 0, the task
 * contribution is ignored.
 */
static inline unsigned long
mtk_compute_energy(struct energy_env *eenv, struct perf_domain *pd,
	       struct cpumask *pd_cpus, struct task_struct *p, int dst_cpu)
{

	if (eenv->wl_support)
		return mtk_compute_energy_cpu_dsu(eenv, pd, pd_cpus, p, dst_cpu);
	else
		return mtk_compute_energy_cpu(eenv->gear_idx, eenv, pd, pd_cpus, p, dst_cpu);
}
#endif

static unsigned int uclamp_min_ls;
void set_uclamp_min_ls(unsigned int val)
{
	uclamp_min_ls = val;
}
EXPORT_SYMBOL_GPL(set_uclamp_min_ls);

unsigned int get_uclamp_min_ls(void)
{
	return uclamp_min_ls;
}
EXPORT_SYMBOL_GPL(get_uclamp_min_ls);

/*
 * attach_task() -- attach the task detached by detach_task() to its new rq.
 */
static void attach_task(struct rq *rq, struct task_struct *p)
{
	lockdep_assert_rq_held(rq);

	BUG_ON(task_rq(p) != rq);
	activate_task(rq, p, ENQUEUE_NOCLOCK);
	check_preempt_curr(rq, p, 0);
}

/*
 * attach_one_task() -- attaches the task returned from detach_one_task() to
 * its new rq.
 */
static void attach_one_task(struct rq *rq, struct task_struct *p)
{
	struct rq_flags rf;

	rq_lock(rq, &rf);
	update_rq_clock(rq);
	attach_task(rq, p);
	rq_unlock(rq, &rf);
}

#if IS_ENABLED(CONFIG_MTK_EAS)
struct cpumask system_cpumask;

void init_system_cpumask(void)
{
	cpumask_copy(&system_cpumask, cpu_possible_mask);
}

void set_system_cpumask(const struct cpumask *srcp)
{
	cpumask_copy(&system_cpumask, srcp);
}
EXPORT_SYMBOL_GPL(set_system_cpumask);

void set_system_cpumask_int(unsigned int cpumask_val)
{
	struct cpumask cpumask_setting;
	unsigned long cpumask_ulval = cpumask_val;
	int cpu;

	cpumask_clear(&cpumask_setting);
	for_each_possible_cpu(cpu) {
		if (test_bit(cpu, &cpumask_ulval))
			cpumask_set_cpu(cpu, &cpumask_setting);
	}

	cpumask_copy(&system_cpumask, &cpumask_setting);
}
EXPORT_SYMBOL_GPL(set_system_cpumask_int);

struct cpumask *get_system_cpumask(void)
{
	return &system_cpumask;
}
EXPORT_SYMBOL_GPL(get_system_cpumask);

static struct cpumask bcpus;
static unsigned long util_Th;

void get_most_powerful_pd_and_util_Th(void)
{
	unsigned int nr_gear = get_nr_gears();

	/* no mutliple pd */
	if (WARN_ON(nr_gear <= 1)) {
		util_Th = 0;
		return;
	}

	/* pd_capacity_tbl is sorted by ascending order,
	 * so nr_gear-1 is most powerful gear and
	 * nr_gear is the second powerful gear.
	 */
	cpumask_copy(&bcpus, get_gear_cpumask(nr_gear-1));
	/* threshold is set to large capacity in mcpus */
	util_Th = pd_get_opp_capacity(
		cpumask_first(get_gear_cpumask(nr_gear-2)), 0);

}

static inline bool task_can_skip_this_cpu(struct task_struct *p, unsigned long p_uclamp_min,
		bool latency_sensitive, int cpu, struct cpumask *bcpus)
{
	bool cpu_in_bcpus;
	unsigned long task_util;

	if (latency_sensitive)
		return 0;

	if (p_uclamp_min > 0)
		return 0;

	if (cpumask_empty(bcpus))
		return 0;

	cpu_in_bcpus = cpumask_test_cpu(cpu, bcpus);
	task_util = task_util_est(p);
	if (!cpu_in_bcpus || !fits_capacity(task_util, util_Th))
		return 0;

	return 1;
}

int mtk_find_energy_efficient_cpu_in_interrupt(struct task_struct *p, bool latency_sensitive,
		struct perf_domain *pd, unsigned long min_cap, unsigned long max_cap)
{
	int target_cpu = -1, cpu;
	unsigned long cpu_util;
	unsigned long pwr, best_pwr = ULONG_MAX, best_idle_pwr = ULONG_MAX;
	unsigned long cpu_cap = 0;
	unsigned int fit_cpus = 0;
	unsigned int idle_cpus = 0;
	long max_spare_cap = LONG_MIN, spare_cap, max_spare_cap_per_gear;
	int max_spare_cap_cpu = -1, max_spare_cap_cpu_per_gear;
	long sys_max_spare_cap = LONG_MIN, idle_max_spare_cap = LONG_MIN;
	int sys_max_spare_cap_cpu = -1, idle_max_spare_cap_cpu = -1;
	unsigned long util;
	bool not_in_softmask;
	unsigned int min_exit_lat = UINT_MAX, min_exit_lat_per_gear;
	struct cpuidle_state *idle;
	int best_idle_cpu = -1, best_idle_cpu_per_gear;
	long best_idle_max_spare_cap = LONG_MIN, best_idle_cpu_cap_per_gear;
	int this_cpu = smp_processor_id();
	int prev_cpu = task_cpu(p);
	int select_reason = -1;
	struct cpumask allowed_cpu_mask;
#if IS_ENABLED(CONFIG_MTK_IRQ_MONITOR_DEBUG)
	u64 ts[9] = {0};

	ts[0] = sched_clock();
#endif

	for (; pd; pd = pd->next) {
		max_spare_cap_cpu_per_gear = -1;
		max_spare_cap_per_gear = LONG_MIN;
		min_exit_lat_per_gear = UINT_MAX;
		best_idle_cpu_per_gear = -1;
		best_idle_cpu_cap_per_gear = LONG_MIN;

		for_each_cpu_and(cpu, perf_domain_span(pd), cpu_active_mask) {

			if (!cpumask_test_cpu(cpu, p->cpus_ptr))
				continue;

			if (cpu_paused(cpu))
				continue;

			cpumask_set_cpu(cpu, &allowed_cpu_mask);

			if (task_can_skip_this_cpu(p, min_cap, latency_sensitive, cpu, &bcpus))
				continue;

			if (cpu_rq(cpu)->rt.rt_nr_running >= 1 &&
					!rt_rq_throttled(&(cpu_rq(cpu)->rt)))
				continue;

			util = cpu_util_next(cpu, p, cpu);
			cpu_cap = capacity_of(cpu);
			spare_cap = cpu_cap;
			lsub_positive(&spare_cap, util);
			not_in_softmask = (latency_sensitive &&
						!cpumask_test_cpu(cpu, &system_cpumask));

			if (not_in_softmask)
				continue;

			/* record sys_max_spare_cap_cpu */
			if (spare_cap > sys_max_spare_cap) {
				sys_max_spare_cap = spare_cap;
				sys_max_spare_cap_cpu = cpu;
			}

			/*
			 * if there is no best idle cpu, then select max spare cap
			 * and idle cpu for latency_sensitive task to avoid runnable.
			 * Because this is just a backup option, we do not take care
			 * of exit latency.
			 */
			if (latency_sensitive && available_idle_cpu(cpu) &&
					spare_cap > idle_max_spare_cap) {
				idle_max_spare_cap = spare_cap;
				idle_max_spare_cap_cpu = cpu;
			}

			/*
			 * Skip CPUs that cannot satisfy the capacity request.
			 * IOW, placing the task there would make the CPU
			 * overutilized. Take uclamp into account to see how
			 * much capacity we can get out of the CPU; this is
			 * aligned with effective_cpu_util().
			 */
			cpu_util = mtk_uclamp_rq_util_with(cpu_rq(cpu), util, p, min_cap, max_cap);
			if (!fits_capacity(cpu_util, cpu_cap))
				continue;

			fit_cpus = (fit_cpus | (1 << cpu));

			/*
			 * Find the CPU with the maximum spare capacity in
			 * the performance domain
			 */
			if (spare_cap > max_spare_cap_per_gear) {
				max_spare_cap_per_gear = spare_cap;
				max_spare_cap_cpu_per_gear = cpu;
			}

			if (!latency_sensitive)
				continue;

			if (available_idle_cpu(cpu)) {
				idle_cpus = (idle_cpus | (1 << cpu));
				idle = idle_get_state(cpu_rq(cpu));
				if (idle) {
					/* non WFI, find shortest exit_latency */
					if (idle->exit_latency < min_exit_lat_per_gear) {
						min_exit_lat_per_gear = idle->exit_latency;
						best_idle_cpu_per_gear = cpu;
						best_idle_cpu_cap_per_gear = spare_cap;
					} else if ((idle->exit_latency == min_exit_lat_per_gear)
						&& (best_idle_cpu_cap_per_gear < spare_cap)) {
						best_idle_cpu_per_gear = cpu;
						best_idle_cpu_cap_per_gear = spare_cap;
					}
				} else {
					/* WFI, find max_spare_cap */
					if (min_exit_lat_per_gear > 0) {
						min_exit_lat_per_gear = 0;
						best_idle_cpu_per_gear = cpu;
						best_idle_cpu_cap_per_gear = spare_cap;
					} else if (best_idle_cpu_cap_per_gear < spare_cap) {
						best_idle_cpu_per_gear = cpu;
						best_idle_cpu_cap_per_gear = spare_cap;
					}
				}
			}
		}

		/* no latency_sensitive task, select max_spare_cpu */
		if (!latency_sensitive && max_spare_cap_cpu_per_gear >= 0) {
			/* calculate power consumption of candidate cpu per gear */
			pwr = calc_pwr_eff(max_spare_cap_cpu_per_gear, cpu_util);
			/* if cpu power is better, select it as candidate */
			if (best_pwr > pwr) {
				best_pwr = pwr;
				max_spare_cap_cpu = max_spare_cap_cpu_per_gear;
				max_spare_cap = max_spare_cap_per_gear;
			}
			/* if power of two cpus are identical, select larger capacity */
			else if ((best_pwr == pwr) && (max_spare_cap < max_spare_cap_per_gear)) {
				max_spare_cap_cpu = max_spare_cap_cpu_per_gear;
				max_spare_cap = max_spare_cap_per_gear;
			}
		}

		/* latency_sensitive task, select best_idle_cpu (lightest sleep) */
		if (latency_sensitive && best_idle_cpu_per_gear >= 0) {
			pwr = calc_pwr_eff(best_idle_cpu_per_gear, cpu_util);
			if (best_idle_pwr > pwr) {
				best_idle_pwr = pwr;
				best_idle_cpu = best_idle_cpu_per_gear;
				best_idle_max_spare_cap = best_idle_cpu_cap_per_gear;
				min_exit_lat = min_exit_lat_per_gear;
			}
			/* if power of two cpus are identical, select larger capacity */
			else if ((best_idle_pwr == pwr)
				&& (best_idle_max_spare_cap < best_idle_cpu_cap_per_gear)) {
				best_idle_cpu = best_idle_cpu_per_gear;
				best_idle_max_spare_cap = best_idle_cpu_cap_per_gear;
				min_exit_lat = min_exit_lat_per_gear;
			}
		}
	}

#if IS_ENABLED(CONFIG_MTK_IRQ_MONITOR_DEBUG)
	ts[1] = sched_clock();
#endif

	if (latency_sensitive) {
		if (best_idle_cpu >= 0) {
			/* best idle cpu existed */
			target_cpu = best_idle_cpu;
			select_reason = LB_LATENCY_SENSITIVE_BEST_IDLE_CPU;
		} else if (idle_max_spare_cap_cpu >= 0) {
			target_cpu = idle_max_spare_cap_cpu;
			select_reason = LB_LATENCY_SENSITIVE_IDLE_MAX_SPARE_CPU;
		} else {
			target_cpu = sys_max_spare_cap_cpu;
			select_reason = LB_LATENCY_SENSITIVE_MAX_SPARE_CPU;
		}
		goto out;
	}

#if IS_ENABLED(CONFIG_MTK_IRQ_MONITOR_DEBUG)
	ts[2] = sched_clock();
#endif

	if (max_spare_cap_cpu != -1) {
		target_cpu = max_spare_cap_cpu;
		select_reason = LB_BEST_ENERGY_CPU;
		goto out;
	}

#if IS_ENABLED(CONFIG_MTK_IRQ_MONITOR_DEBUG)
	ts[3] = sched_clock();
#endif

	/* All cpu failed on !fit_capacity, use sys_max_spare_cap_cpu */
	if (sys_max_spare_cap_cpu != -1) {
		target_cpu = sys_max_spare_cap_cpu;
		select_reason = LB_MAX_SPARE_CPU;
		goto out;
	}

#if IS_ENABLED(CONFIG_MTK_IRQ_MONITOR_DEBUG)
	ts[4] = sched_clock();
#endif

	/*no best_idle_cpu and max_spare_cpu available,
	 *select this_cpu or prev_cpu with cpu_allowed_mask
	 */
	if (target_cpu == -1) {
		if (cpumask_test_cpu(this_cpu, &allowed_cpu_mask)) {
			target_cpu = this_cpu;
			select_reason = LB_IRQ_BACKUP_CURR;
			goto out;
		}
#if IS_ENABLED(CONFIG_MTK_IRQ_MONITOR_DEBUG)
		ts[5] = sched_clock();
#endif
		if (cpumask_test_cpu(prev_cpu, &allowed_cpu_mask)) {
			target_cpu = prev_cpu;
			select_reason = LB_IRQ_BACKUP_PREV;
			goto out;
		}
#if IS_ENABLED(CONFIG_MTK_IRQ_MONITOR_DEBUG)
		ts[6] = sched_clock();
#endif
		/*select cpu in allowed_cpu_mask, not paused, and no rt running */
		target_cpu = cpumask_any(&allowed_cpu_mask);
		select_reason = LB_IRQ_BACKUP_ALLOWED;
	}

out:
#if IS_ENABLED(CONFIG_MTK_IRQ_MONITOR_DEBUG)
	ts[7] = sched_clock();
#endif

	if (trace_sched_find_cpu_in_irq_enabled())
		trace_sched_find_cpu_in_irq(p, select_reason, target_cpu,
				prev_cpu, fit_cpus, idle_cpus,
				best_idle_cpu, best_idle_pwr, min_exit_lat,
				max_spare_cap_cpu, best_pwr, max_spare_cap);

#if IS_ENABLED(CONFIG_MTK_IRQ_MONITOR_DEBUG)
	ts[8] = sched_clock();

	if ((ts[8] - ts[0] > 500000ULL) && in_hardirq()) {
		int i, i_prev;
		u64 prev, curr;

		printk_deferred("%s duration %llu, ts[0]=%llu\n", __func__, ts[8] - ts[0], ts[0]);
		i_prev = 0;
		for (i = 0; i < 8; i++) {
			if (ts[i+1]) {
				prev = ts[i_prev];
				curr = ts[i+1];
				printk_deferred("%s ts[%d]=%llu, ts[%d]=%llu, duration=%llu\n",
						__func__, i_prev, prev, i+1, curr, curr - prev);
				i_prev = i+1;
			}
		}
	}
#endif

	return target_cpu;
}

DEFINE_PER_CPU(cpumask_var_t, mtk_select_rq_mask);

void mtk_find_energy_efficient_cpu(void *data, struct task_struct *p, int prev_cpu, int sync,
					int *new_cpu)
{
	struct cpumask *cpus = this_cpu_cpumask_var_ptr(mtk_select_rq_mask);
	unsigned long best_delta = ULONG_MAX;
	struct root_domain *rd = cpu_rq(smp_processor_id())->rd;
	int best_idle_cpu = -1;
	long sys_max_spare_cap = LONG_MIN, idle_max_spare_cap = LONG_MIN;
	int sys_max_spare_cap_cpu = -1;
	int idle_max_spare_cap_cpu = -1;
	unsigned long target_cap = 0;
	unsigned long cpu_cap, util;
	bool latency_sensitive = false;
	unsigned int min_exit_lat = UINT_MAX;
	int cpu, best_energy_cpu = -1;
	struct cpuidle_state *idle;
	struct perf_domain *pd;
	int select_reason = -1;
	unsigned long min_cap = uclamp_eff_value(p, UCLAMP_MIN);
	unsigned long max_cap = uclamp_eff_value(p, UCLAMP_MAX);
	struct energy_env eenv;

	rcu_read_lock();
	if (!uclamp_min_ls)
		latency_sensitive = uclamp_latency_sensitive(p);
	else {
		latency_sensitive = (p->uclamp_req[UCLAMP_MIN].value > 0 ? 1 : 0) ||
					uclamp_latency_sensitive(p);
	}

	if (!latency_sensitive)
		latency_sensitive = get_task_idle_prefer_by_task(p);

	pd = rcu_dereference(rd->pd);
	if (!pd || READ_ONCE(rd->overutilized)) {
		select_reason = LB_FAIL;
		goto unlock;
	}

	cpu = smp_processor_id();
	if (sync && cpu_rq(cpu)->nr_running == 1 &&
	    cpumask_test_cpu(cpu, p->cpus_ptr) &&
	    task_fits_capacity(p, capacity_of(cpu)) &&
	    !(latency_sensitive && !cpumask_test_cpu(cpu, &system_cpumask))) {
		rcu_read_unlock();
		*new_cpu = cpu;
		select_reason = LB_SYNC;
		goto done;
	}

	if (unlikely(in_interrupt())) {
		*new_cpu = mtk_find_energy_efficient_cpu_in_interrupt(p, latency_sensitive, pd,
					min_cap, max_cap);
		rcu_read_unlock();
		select_reason = LB_IN_INTERRUPT;
		goto done;
	}

	if (!task_util_est(p)) {
		select_reason = LB_ZERO_UTIL;
		goto unlock;
	}

	eenv_task_busy_time(&eenv, p, prev_cpu);
	eenv.min_cap = min_cap;
	eenv.max_cap = max_cap;

	eenv_init(&eenv, p, prev_cpu, pd);

	for (; pd; pd = pd->next) {
		unsigned long cur_delta, base_energy;
		long spare_cap, max_spare_cap = LONG_MIN;
		unsigned long max_spare_cap_ls_idle = 0;
		int max_spare_cap_cpu = -1;
		int max_spare_cap_cpu_ls_idle = -1;
		int gear_idx;
#if IS_ENABLED(CONFIG_MTK_THERMAL_AWARE_SCHEDULING)
		int cpu_order[NR_CPUS]  ____cacheline_aligned, cnt, i;

#endif

		cpumask_and(cpus, perf_domain_span(pd), cpu_active_mask);

		if (cpumask_empty(cpus))
			continue;

		/* Account thermal pressure for the energy estimation */
		cpu = cpumask_first(cpus);
		gear_idx = eenv.gear_idx = per_cpu(gear_id, cpu);

#if IS_ENABLED(CONFIG_MTK_THERMAL_AWARE_SCHEDULING)
		cnt = sort_thermal_headroom(cpus, cpu_order);

		for (i = 0; i < cnt; i++) {
			cpu = cpu_order[i];
#else
		for_each_cpu(cpu, cpus) {
#endif
			if (!cpumask_test_cpu(cpu, p->cpus_ptr))
				continue;

			if (cpu_paused(cpu))
				continue;

			if (cpu_rq(cpu)->rt.rt_nr_running >= 1 &&
						!rt_rq_throttled(&(cpu_rq(cpu)->rt)))
				continue;

			util = cpu_util_next(cpu, p, cpu);
			cpu_cap = capacity_of(cpu);
			spare_cap = cpu_cap;
			lsub_positive(&spare_cap, util);

			if ((spare_cap > sys_max_spare_cap) &&
			    !(latency_sensitive && !cpumask_test_cpu(cpu, &system_cpumask))) {
				sys_max_spare_cap = spare_cap;
				sys_max_spare_cap_cpu = cpu;
			}

			if (latency_sensitive && !cpumask_test_cpu(cpu, &system_cpumask))
				continue;

			/*
			 * if there is no best idle cpu, then select max spare cap
			 * and idle cpu for latency_sensitive task to avoid runnable.
			 * Because this is just a backup option, we do not take care
			 * of exit latency.
			 */
			if (latency_sensitive && available_idle_cpu(cpu) &&
					spare_cap > idle_max_spare_cap) {
				idle_max_spare_cap = spare_cap;
				idle_max_spare_cap_cpu = cpu;
			}

			/*
			 * Skip CPUs that cannot satisfy the capacity request.
			 * IOW, placing the task there would make the CPU
			 * overutilized. Take uclamp into account to see how
			 * much capacity we can get out of the CPU; this is
			 * aligned with effective_cpu_util().
			 */
			util = mtk_uclamp_rq_util_with(cpu_rq(cpu), util, p, min_cap, max_cap);
			if (!fits_capacity(util, cpu_cap))
				continue;

			/*
			 * Find the CPU with the maximum spare capacity in
			 * the performance domain
			 */
			if (spare_cap > max_spare_cap) {
				max_spare_cap = spare_cap;
				max_spare_cap_cpu = cpu;
			}

			if (!latency_sensitive)
				continue;

			if (available_idle_cpu(cpu)) {
				cpu_cap = capacity_orig_of(cpu);
				idle = idle_get_state(cpu_rq(cpu));
#if IS_ENABLED(CONFIG_MTK_THERMAL_AWARE_SCHEDULING)
				if (idle && idle->exit_latency >= min_exit_lat &&
						cpu_cap == target_cap)
					continue;
#else
				if (idle && idle->exit_latency > min_exit_lat &&
						cpu_cap == target_cap)
					continue;
#endif

				if (spare_cap < max_spare_cap_ls_idle)
					continue;

				if (idle)
					min_exit_lat = idle->exit_latency;

				max_spare_cap_ls_idle = spare_cap;
				target_cap = cpu_cap;
				max_spare_cap_cpu_ls_idle = cpu;
			}
		}

		/* Evaluate the energy impact of using this CPU. */
		if (!latency_sensitive && max_spare_cap_cpu >= 0) {
			eenv_pd_busy_time(gear_idx, &eenv, cpus, p);
			cur_delta = mtk_compute_energy(&eenv, pd, cpus, p, max_spare_cap_cpu);
			base_energy = mtk_compute_energy(&eenv, pd, cpus, p, -1);
			cur_delta = cur_delta - base_energy;
			if (cur_delta <= best_delta) {
				best_delta = cur_delta;
				best_energy_cpu = max_spare_cap_cpu;
			}
		}

		if (latency_sensitive) {
			if (max_spare_cap_cpu_ls_idle >= 0) {
				eenv_pd_busy_time(gear_idx, &eenv, cpus, p);
				cur_delta = mtk_compute_energy(&eenv, pd, cpus, p,
						max_spare_cap_cpu_ls_idle);
				base_energy = mtk_compute_energy(&eenv, pd, cpus, p, -1);
				cur_delta = cur_delta - base_energy;
				if (cur_delta <= best_delta) {
					best_delta = cur_delta;
					best_idle_cpu = max_spare_cap_cpu_ls_idle;
				}
			}
		}
	}

	rcu_read_unlock();

	if (latency_sensitive) {
		if (best_idle_cpu >= 0) {
			*new_cpu = best_idle_cpu;
			select_reason = LB_LATENCY_SENSITIVE_BEST_IDLE_CPU;
		} else if (idle_max_spare_cap_cpu >= 0) {
			*new_cpu = idle_max_spare_cap_cpu;
			select_reason = LB_LATENCY_SENSITIVE_IDLE_MAX_SPARE_CPU;
		} else {
			*new_cpu = sys_max_spare_cap_cpu;
			select_reason = LB_LATENCY_SENSITIVE_MAX_SPARE_CPU;
		}
		goto done;
	}

	/* All cpu failed on !fit_capacity, use sys_max_spare_cap_cpu */
	if (best_energy_cpu != -1) {
		*new_cpu = best_energy_cpu;
		select_reason = LB_BEST_ENERGY_CPU;
		goto done;
	} else {
		*new_cpu = sys_max_spare_cap_cpu;
		select_reason = LB_MAX_SPARE_CPU;
		goto done;
	}

	*new_cpu = prev_cpu;
	select_reason = LB_PREV;
	goto done;


unlock:
	rcu_read_unlock();

	*new_cpu = -1;
done:
	if (trace_sched_find_energy_efficient_cpu_enabled())
		trace_sched_find_energy_efficient_cpu(best_delta, best_energy_cpu,
				best_idle_cpu, idle_max_spare_cap_cpu, sys_max_spare_cap_cpu);
	if (trace_sched_select_task_rq_enabled())
		trace_sched_select_task_rq(p, select_reason, prev_cpu, *new_cpu,
				task_util(p), task_util_est(p), uclamp_task_util(p),
				latency_sensitive, sync);

}
#endif

#endif

#if IS_ENABLED(CONFIG_MTK_EAS)
/* must hold runqueue lock for queue se is currently on */
static struct task_struct *detach_a_hint_task(struct rq *src_rq, int dst_cpu)
{
	struct task_struct *p, *best_task = NULL, *backup = NULL;
	int dst_capacity;
	unsigned int task_util;
	bool latency_sensitive = false;

	lockdep_assert_rq_held(src_rq);

	rcu_read_lock();
	dst_capacity = capacity_orig_of(dst_cpu);
	list_for_each_entry_reverse(p,
			&src_rq->cfs_tasks, se.group_node) {

		if (!cpumask_test_cpu(dst_cpu, p->cpus_ptr))
			continue;

		if (task_on_cpu(src_rq, p))
			continue;

		task_util = uclamp_task_util(p);

		if (!uclamp_min_ls)
			latency_sensitive = uclamp_latency_sensitive(p);
		else {
			latency_sensitive = (p->uclamp_req[UCLAMP_MIN].value > 0 ? 1 : 0) ||
					uclamp_latency_sensitive(p);
		}

		if (!latency_sensitive)
			latency_sensitive = get_task_idle_prefer_by_task(p);

		if (latency_sensitive && !cpumask_test_cpu(dst_cpu, &system_cpumask))
			continue;

		if (latency_sensitive &&
			task_util <= dst_capacity) {
			best_task = p;
			break;
		} else if (latency_sensitive && !backup) {
			backup = p;
		}
	}
	p = best_task ? best_task : backup;
	if (p) {
		/* detach_task */
		deactivate_task(src_rq, p, DEQUEUE_NOCLOCK);
		set_task_cpu(p, dst_cpu);
	}
	rcu_read_unlock();
	return p;
}
#endif

inline bool is_task_latency_sensitive(struct task_struct *p)
{
	bool latency_sensitive = false;

	rcu_read_lock();
	if (!uclamp_min_ls)
		latency_sensitive = uclamp_latency_sensitive(p);
	else {
		latency_sensitive = (p->uclamp_req[UCLAMP_MIN].value > 0 ? 1 : 0) ||
					uclamp_latency_sensitive(p);
	}
	if (!latency_sensitive)
		latency_sensitive = get_task_idle_prefer_by_task(p);

	rcu_read_unlock();

	return latency_sensitive;
}

static int mtk_active_load_balance_cpu_stop(void *data)
{
	struct task_struct *target_task = data;
	int busiest_cpu = smp_processor_id();
	struct rq *busiest_rq = cpu_rq(busiest_cpu);
	int target_cpu = busiest_rq->push_cpu;
	struct rq *target_rq = cpu_rq(target_cpu);
	struct rq_flags rf;
	int deactivated = 0;

	local_irq_disable();
	raw_spin_lock(&target_task->pi_lock);
	rq_lock(busiest_rq, &rf);

	if (task_cpu(target_task) != busiest_cpu ||
		(!cpumask_test_cpu(target_cpu, target_task->cpus_ptr)) ||
		task_on_cpu(busiest_rq, target_task) ||
		target_rq == busiest_rq)
		goto out_unlock;

	if (!task_on_rq_queued(target_task))
		goto out_unlock;

	if (!cpu_active(busiest_cpu) || !cpu_active(target_cpu))
		goto out_unlock;

	if (cpu_paused(busiest_cpu) || cpu_paused(target_cpu))
		goto out_unlock;

	/* Make sure the requested CPU hasn't gone down in the meantime: */
	if (unlikely(!busiest_rq->active_balance))
		goto out_unlock;

	/* Is there any task to move? */
	if (busiest_rq->nr_running <= 1)
		goto out_unlock;

	update_rq_clock(busiest_rq);
	deactivate_task(busiest_rq, target_task, DEQUEUE_NOCLOCK);
	set_task_cpu(target_task, target_cpu);
	deactivated = 1;
out_unlock:
	busiest_rq->active_balance = 0;
	rq_unlock(busiest_rq, &rf);

	if (deactivated)
		attach_one_task(target_rq, target_task);

	raw_spin_unlock(&target_task->pi_lock);
	put_task_struct(target_task);

	local_irq_enable();
	return 0;
}

int migrate_running_task(int this_cpu, struct task_struct *p, struct rq *target, int reason)
{
	int active_balance = false;
	unsigned long flags;

	raw_spin_rq_lock_irqsave(target, flags);
	if (!target->active_balance &&
		(task_rq(p) == target) && p->__state != TASK_DEAD &&
		 !(is_task_latency_sensitive(p) && !cpumask_test_cpu(this_cpu, &system_cpumask))) {
		target->active_balance = 1;
		target->push_cpu = this_cpu;
		active_balance = true;
		get_task_struct(p);
	}
	raw_spin_rq_unlock_irqrestore(target, flags);
	if (active_balance) {
		trace_sched_force_migrate(p, this_cpu, reason);
		stop_one_cpu_nowait(cpu_of(target),
				mtk_active_load_balance_cpu_stop,
				p, &target->active_balance_work);
	}

	return active_balance;
}

#if IS_ENABLED(CONFIG_MTK_EAS)
static DEFINE_PER_CPU(u64, next_update_new_balance_time_ns);
void mtk_sched_newidle_balance(void *data, struct rq *this_rq, struct rq_flags *rf,
		int *pulled_task, int *done)
{
	int cpu;
	struct rq *src_rq, *misfit_task_rq = NULL;
	struct task_struct *p = NULL, *best_running_task = NULL;
	struct rq_flags src_rf;
	int this_cpu = this_rq->cpu;
	unsigned long misfit_load = 0;
	u64 now_ns;

	if (cpu_paused(this_cpu)) {
		*done = 1;
		return;
	}

	/*
	 * There is a task waiting to run. No need to search for one.
	 * Return 0; the task will be enqueued when switching to idle.
	 */
	if (this_rq->ttwu_pending)
		return;

	/*
	 * We must set idle_stamp _before_ calling idle_balance(), such that we
	 * measure the duration of idle_balance() as idle time.
	 */
	this_rq->idle_stamp = rq_clock(this_rq);

	/*
	 * Do not pull tasks towards !active CPUs...
	 */
	if (!cpu_active(this_cpu))
		return;

	now_ns = ktime_get_real_ns();

	if (now_ns < per_cpu(next_update_new_balance_time_ns, this_cpu))
		return;

	per_cpu(next_update_new_balance_time_ns, this_cpu) =
		now_ns + new_idle_balance_interval_ns;

	trace_sched_next_new_balance(now_ns, per_cpu(next_update_new_balance_time_ns, this_cpu));

	/*
	 * This is OK, because current is on_cpu, which avoids it being picked
	 * for load-balance and preemption/IRQs are still disabled avoiding
	 * further scheduler activity on it and we're being very careful to
	 * re-start the picking loop.
	 */
	rq_unpin_lock(this_rq, rf);
	raw_spin_rq_unlock(this_rq);

	this_cpu = this_rq->cpu;
	for_each_cpu(cpu, cpu_active_mask) {
		if (cpu == this_cpu)
			continue;

		src_rq = cpu_rq(cpu);
		rq_lock_irqsave(src_rq, &src_rf);
		update_rq_clock(src_rq);
		if (src_rq->active_balance) {
			rq_unlock_irqrestore(src_rq, &src_rf);
			continue;
		}
		if (src_rq->misfit_task_load > misfit_load &&
			capacity_orig_of(this_cpu) > capacity_orig_of(cpu)) {
			p = src_rq->curr;
			if (p && p->policy == SCHED_NORMAL &&
				cpumask_test_cpu(this_cpu, p->cpus_ptr) &&
				!(is_task_latency_sensitive(p) &&
				!cpumask_test_cpu(this_cpu, &system_cpumask))) {

				misfit_task_rq = src_rq;
				misfit_load = src_rq->misfit_task_load;
				if (best_running_task)
					put_task_struct(best_running_task);
				best_running_task = p;
				get_task_struct(best_running_task);
			}
			p = NULL;
		}

		if (src_rq->nr_running <= 1) {
			rq_unlock_irqrestore(src_rq, &src_rf);
			continue;
		}

		p = detach_a_hint_task(src_rq, this_cpu);

		rq_unlock_irqrestore(src_rq, &src_rf);

		if (p) {
			trace_sched_force_migrate(p, this_cpu, MIGR_IDLE_BALANCE);
			attach_one_task(this_rq, p);
			break;
		}
	}

	/*
	 * If p is null meaning that we have not pull a runnable task, we try to
	 * pull a latency sensitive running task.
	 */
	if (!p && misfit_task_rq)
		*done = migrate_running_task(this_cpu, best_running_task,
					misfit_task_rq, MIGR_IDLE_PULL_MISFIT_RUNNING);
	if (best_running_task)
		put_task_struct(best_running_task);
	raw_spin_rq_lock(this_rq);
	/*
	 * While browsing the domains, we released the rq lock, a task could
	 * have been enqueued in the meantime. Since we're not going idle,
	 * pretend we pulled a task.
	 */
	if (this_rq->cfs.h_nr_running && !*pulled_task)
		*pulled_task = 1;

	/* Is there a task of a high priority class? */
	if (this_rq->nr_running != this_rq->cfs.h_nr_running)
		*pulled_task = -1;

	if (*pulled_task)
		this_rq->idle_stamp = 0;

	if (*pulled_task != 0)
		*done = 1;

	rq_repin_lock(this_rq, rf);

}
#endif
