/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2021 MediaTek Inc.
 */
#undef TRACE_SYSTEM
#define TRACE_SYSTEM scheduler

#if !defined(_TRACE_SCHEDULER_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_SCHEDULER_H
#include <linux/string.h>
#include <linux/types.h>
#include <linux/tracepoint.h>
#include <linux/sched/clock.h>

#include "eas_plus.h"

#if IS_ENABLED(CONFIG_MTK_SCHED_FAST_LOAD_TRACKING)
#include "common.h"
#include "group.h"
#include "flt_cal.h"
extern const char *task_event_names[];
extern const char *add_task_demand_names[];
extern const char *history_event_names[];
extern const char *cpu_event_names[];
struct rq;
struct flt_task_struct;
struct flt_rq;
#endif

#if IS_ENABLED(CONFIG_MTK_SCHED_BIG_TASK_ROTATE)
/*
 * Tracepoint for big task rotation
 */
TRACE_EVENT(sched_big_task_rotation,

	TP_PROTO(int src_cpu, int dst_cpu, int src_pid, int dst_pid,
		int fin),

	TP_ARGS(src_cpu, dst_cpu, src_pid, dst_pid, fin),

	TP_STRUCT__entry(
		__field(int, src_cpu)
		__field(int, dst_cpu)
		__field(int, src_pid)
		__field(int, dst_pid)
		__field(int, fin)
	),

	TP_fast_assign(
		__entry->src_cpu	= src_cpu;
		__entry->dst_cpu	= dst_cpu;
		__entry->src_pid	= src_pid;
		__entry->dst_pid	= dst_pid;
		__entry->fin		= fin;
	),

	TP_printk("src_cpu=%d dst_cpu=%d src_pid=%d dst_pid=%d fin=%d",
		__entry->src_cpu, __entry->dst_cpu,
		__entry->src_pid, __entry->dst_pid,
		__entry->fin)
);
#endif

TRACE_EVENT(sched_leakage,

	TP_PROTO(int cpu, int opp, unsigned int temp,
		unsigned long cpu_static_pwr, unsigned long static_pwr, unsigned long sum_cap),

	TP_ARGS(cpu, opp, temp, cpu_static_pwr, static_pwr, sum_cap),

	TP_STRUCT__entry(
		__field(int, cpu)
		__field(int, opp)
		__field(unsigned int, temp)
		__field(unsigned long, cpu_static_pwr)
		__field(unsigned long, static_pwr)
		__field(unsigned long, sum_cap)
		),

	TP_fast_assign(
		__entry->cpu       = cpu;
		__entry->opp        = opp;
		__entry->temp       = temp;
		__entry->cpu_static_pwr = cpu_static_pwr;
		__entry->static_pwr = static_pwr;
		__entry->sum_cap = sum_cap;
		),

	TP_printk("cpu=%d opp=%d temp=%u lkg=%lu sum_lkg=%lu, sum_cap=%lu",
		__entry->cpu,
		__entry->opp,
		__entry->temp,
		__entry->cpu_static_pwr,
		__entry->static_pwr,
		__entry->sum_cap)
);

TRACE_EVENT(sched_dsu_freq,

	TP_PROTO(int gear_id, int dsu_freq_new, int dsu_volt_new, unsigned long cpu_freq,
			unsigned long freq),

	TP_ARGS(gear_id, dsu_freq_new, dsu_volt_new, cpu_freq, freq),

	TP_STRUCT__entry(
		__field(int, gear_id)
		__field(int, dsu_freq_new)
		__field(int, dsu_volt_new)
		__field(unsigned long, cpu_freq)
		__field(unsigned long, freq)
		),

	TP_fast_assign(
		__entry->gear_id    = gear_id;
		__entry->dsu_freq_new   = dsu_freq_new;
		__entry->dsu_volt_new   = dsu_volt_new;
		__entry->cpu_freq  = cpu_freq;
		__entry->freq      = freq;
		),

	TP_printk("gear_id=%d dsu_freq_new=%d dsu_volt_new=%d cpu_freq=%lu freq=%lu",
		__entry->gear_id,
		__entry->dsu_freq_new,
		__entry->dsu_volt_new,
		__entry->cpu_freq,
		__entry->freq)
);

TRACE_EVENT(sched_em_cpu_energy,

	TP_PROTO(int idx, unsigned long freq, const char *cost_type, unsigned long cost,
		unsigned long scale_cpu, unsigned long dyn_pwr, unsigned long static_pwr),

	TP_ARGS(idx, freq, cost_type, cost, scale_cpu, dyn_pwr, static_pwr),

	TP_STRUCT__entry(
		__field(int, idx)
		__field(unsigned long, freq)
		__string(cost_type, cost_type)
		__field(unsigned long, cost)
		__field(unsigned long, scale_cpu)
		__field(unsigned long, dyn_pwr)
		__field(unsigned long, static_pwr)
		),

	TP_fast_assign(
		__entry->idx        = idx;
		__entry->freq       = freq;
		__assign_str(cost_type, cost_type);
		__entry->cost       = cost;
		__entry->scale_cpu  = scale_cpu;
		__entry->dyn_pwr    = dyn_pwr;
		__entry->static_pwr = static_pwr;
		),

	TP_printk("idx=%d freq=%lu %s=%lu scale_cpu=%lu dyn_pwr=%lu static_pwr=%lu",
		__entry->idx,
		__entry->freq,
		__get_str(cost_type),
		__entry->cost,
		__entry->scale_cpu,
		__entry->dyn_pwr,
		__entry->static_pwr)
);

TRACE_EVENT(sched_calc_pwr_eff,

	TP_PROTO(int cpu, unsigned long cpu_util, int opp, unsigned long cap,
		unsigned long dyn_pwr_eff, unsigned long static_pwr_eff, unsigned long pwr_eff),

	TP_ARGS(cpu, cpu_util, opp, cap, dyn_pwr_eff, static_pwr_eff, pwr_eff),

	TP_STRUCT__entry(
		__field(int, cpu)
		__field(unsigned long, cpu_util)
		__field(int, opp)
		__field(unsigned long, cap)
		__field(unsigned long, dyn_pwr_eff)
		__field(unsigned long, static_pwr_eff)
		__field(unsigned long, pwr_eff)
		),

	TP_fast_assign(
		__entry->cpu            = cpu;
		__entry->cpu_util       = cpu_util;
		__entry->opp            = opp;
		__entry->cap            = cap;
		__entry->dyn_pwr_eff    = dyn_pwr_eff;
		__entry->static_pwr_eff = static_pwr_eff;
		__entry->pwr_eff        = pwr_eff;
		),

	TP_printk("cpu=%d cpu_util=%lu opp=%d cap=%lu dyn_pwr_eff=%lu static_pwr_eff=%lu pwr_eff=%lu",
		__entry->cpu,
		__entry->cpu_util,
		__entry->opp,
		__entry->cap,
		__entry->dyn_pwr_eff,
		__entry->static_pwr_eff,
		__entry->pwr_eff)
);

TRACE_EVENT(sched_find_busiest_group,

	TP_PROTO(int src_cpu, int dst_cpu,
		int out_balance, int reason),

	TP_ARGS(src_cpu, dst_cpu, out_balance, reason),

	TP_STRUCT__entry(
		__field(int, src_cpu)
		__field(int, dst_cpu)
		__field(int, out_balance)
		__field(int, reason)
		),

	TP_fast_assign(
		__entry->src_cpu	= src_cpu;
		__entry->dst_cpu	= dst_cpu;
		__entry->out_balance	= out_balance;
		__entry->reason		= reason;
		),

	TP_printk("src_cpu=%d dst_cpu=%d out_balance=%d reason=0x%x",
		__entry->src_cpu,
		__entry->dst_cpu,
		__entry->out_balance,
		__entry->reason)
);

TRACE_EVENT(sched_cpu_overutilized,

	TP_PROTO(int cpu, struct cpumask *pd_mask,
		unsigned long sum_util, unsigned long sum_cap,
		int overutilized),

	TP_ARGS(cpu, pd_mask, sum_util, sum_cap,
		overutilized),

	TP_STRUCT__entry(
		__field(int, cpu)
		__field(long, cpu_mask)
		__field(unsigned long, sum_util)
		__field(unsigned long, sum_cap)
		__field(int, overutilized)
		),

	TP_fast_assign(
		__entry->cpu		= cpu;
		__entry->cpu_mask	= pd_mask->bits[0];
		__entry->sum_util	= sum_util;
		__entry->sum_cap	= sum_cap;
		__entry->overutilized	= overutilized;
		),

	TP_printk("cpu=%d mask=0x%lx sum_util=%lu sum_cap=%lu overutilized=%d",
		__entry->cpu,
		__entry->cpu_mask,
		__entry->sum_util,
		__entry->sum_cap,
		__entry->overutilized)
);

/*
 * Tracepoint for task force migrations.
 */
TRACE_EVENT(sched_frequency_limits,

	TP_PROTO(int cpu_id, int freq_thermal),

	TP_ARGS(cpu_id, freq_thermal),

	TP_STRUCT__entry(
		__field(int,  cpu_id)
		__field(int,  freq_thermal)
		),

	TP_fast_assign(
		__entry->cpu_id = cpu_id;
		__entry->freq_thermal = freq_thermal;
		),

	TP_printk("cpu=%d thermal=%d",
		__entry->cpu_id, __entry->freq_thermal)
);

TRACE_EVENT(sched_queue_task,
	TP_PROTO(int cpu, int pid, int enqueue,
		unsigned long cfs_util,
		unsigned int min, unsigned int max,
		unsigned int task_min, unsigned int task_max),
	TP_ARGS(cpu, pid, enqueue, cfs_util, min, max, task_min, task_max),
	TP_STRUCT__entry(
		__field(int, cpu)
		__field(int, pid)
		__field(int, enqueue)
		__field(unsigned long, cfs_util)
		__field(unsigned int, min)
		__field(unsigned int, max)
		__field(unsigned int, task_min)
		__field(unsigned int, task_max)
	),
	TP_fast_assign(
		__entry->cpu = cpu;
		__entry->pid = pid;
		__entry->enqueue = enqueue;
		__entry->cfs_util = cfs_util;
		__entry->min = min;
		__entry->max = max;
		__entry->task_min = task_min;
		__entry->task_max = task_max;
	),
	TP_printk(
		"cpu=%d pid=%d enqueue=%d cfs_util=%lu min=%u max=%u task_min=%u task_max=%u",
		__entry->cpu,
		__entry->pid,
		__entry->enqueue,
		__entry->cfs_util,
		__entry->min,
		__entry->max,
		__entry->task_min,
		__entry->task_max)
);

TRACE_EVENT(sched_task_util,
	TP_PROTO(int pid,
		unsigned long util,
		unsigned int util_enqueued, unsigned int util_ewma),
	TP_ARGS(pid, util, util_enqueued, util_ewma),
	TP_STRUCT__entry(
		__field(int, pid)
		__field(unsigned long, util)
		__field(unsigned int, util_enqueued)
		__field(unsigned int, util_ewma)
	),
	TP_fast_assign(
		__entry->pid = pid;
		__entry->util = util;
		__entry->util_enqueued = util_enqueued;
		__entry->util_ewma = util_ewma;
	),
	TP_printk(
		"pid=%d util=%lu util_enqueued=%u util_ewma=%u",
		__entry->pid,
		__entry->util,
		__entry->util_enqueued,
		__entry->util_ewma)
);

TRACE_EVENT(sched_task_uclamp,
	TP_PROTO(int pid, unsigned long util,
		unsigned int active,
		unsigned int min, unsigned int max,
		unsigned int min_ud, unsigned int min_req,
		unsigned int max_ud, unsigned int max_req),
	TP_ARGS(pid, util, active,
		min, max,
		min_ud, min_req,
		max_ud, max_req),
	TP_STRUCT__entry(
		__field(int, pid)
		__field(unsigned long, util)
		__field(unsigned int, active)
		__field(unsigned int, min)
		__field(unsigned int, max)
		__field(unsigned int, min_ud)
		__field(unsigned int, min_req)
		__field(unsigned int, max_ud)
		__field(unsigned int, max_req)
	),
	TP_fast_assign(
		__entry->pid = pid;
		__entry->util = util;
		__entry->active = active;
		__entry->min = min;
		__entry->max = max;
		__entry->min_ud = min_ud;
		__entry->min_req = min_req;
		__entry->max_ud = max_ud;
		__entry->max_req = max_req;
	),
	TP_printk(
		"pid=%d util=%lu active=%u min=%u max=%u min_ud=%u min_req=%u max_ud=%u max_req=%u",
		__entry->pid,
		__entry->util,
		__entry->active,
		__entry->min,
		__entry->max,
		__entry->min_ud,
		__entry->min_req,
		__entry->max_ud,
		__entry->max_req)
);

#ifdef CREATE_TRACE_POINTS
int sched_cgroup_state_rt(struct task_struct *p, int subsys_id)
{
#ifdef CONFIG_CGROUPS
	int cgrp_id = -1;
	struct cgroup_subsys_state *css;

	rcu_read_lock();
	css = task_css(p, subsys_id);
	if (!css)
		goto out;

	cgrp_id = css->id;

out:
		rcu_read_unlock();

	return cgrp_id;
#else
	return -1;
#endif
}
#endif

TRACE_EVENT(sched_cpu_util,
	TP_PROTO(int cpu, unsigned long cpu_util),
	TP_ARGS(cpu, cpu_util),
	TP_STRUCT__entry(
		__field(unsigned int,	cpu)
		__field(unsigned int,	nr_running)
		__field(long,		cpu_util)
		__field(long,		cpu_util_flt)
		__field(unsigned int,	capacity)
		__field(unsigned int,	capacity_orig)
		__field(unsigned int,	idle_exit_latency)
		__field(long,		irqload)
		__field(int,		online)
		__field(int,		paused)
		__field(int,		high_irq_load)
		__field(unsigned int,	nr_rtg_high_prio_tasks)
	),
	TP_fast_assign(
		__entry->cpu		= cpu;
		__entry->nr_running = cpu_rq(cpu)->nr_running;
		__entry->cpu_util	= cpu_util;
		__entry->cpu_util_flt	= 0; //cpu_util_flt(cpu);
		__entry->capacity	= capacity_of(cpu);
		__entry->capacity_orig	= capacity_orig_of(cpu);
		__entry->idle_exit_latency	= mtk_get_idle_exit_latency(cpu, NULL);
		__entry->irqload		= cpu_util_irq(cpu_rq(cpu));
		__entry->online			= cpu_online(cpu);
		__entry->paused			= cpu_paused(cpu);
		__entry->high_irq_load	= cpu_high_irqload(cpu, cpu_util);
		__entry->nr_rtg_high_prio_tasks = 0; //mtk_nr_rtg_high_prio(cpu);
	),
	TP_printk("cpu=%d nr_running=%d cpu_util=%ld cpu_util_flt=%ld capacity=%u capacity_orig=%u idle_exit_latency=%u irqload=%ld online=%u paused=%u high_irq_load=%u nr_rtg_hp=%u",
		__entry->cpu,
		__entry->nr_running,
		__entry->cpu_util,
		__entry->cpu_util_flt,
		__entry->capacity,
		__entry->capacity_orig,
		__entry->idle_exit_latency,
		__entry->irqload,
		__entry->online,
		__entry->paused,
		__entry->high_irq_load,
		__entry->nr_rtg_high_prio_tasks)
);

TRACE_EVENT(sched_select_task_rq_rt,
	TP_PROTO(struct task_struct *tsk, int policy,
		int target_cpu, struct rt_energy_aware_output *rt_ea_output,
		struct cpumask *lowest_mask, int sd_flag, bool sync),
	TP_ARGS(tsk, policy, target_cpu, rt_ea_output, lowest_mask,
		sd_flag, sync),
	TP_STRUCT__entry(
		__field(pid_t, pid)
		__field(int, policy)
		__field(int, target_cpu)
		__field(long, lowest_mask)
		__field(unsigned int,  idle_cpus)
		__field(unsigned int,  cfs_cpus)
		__field(int, cfs_lowest_cpu)
		__field(int, cfs_lowest_prio)
		__field(int, cfs_lowest_pid)
		__field(int, rt_lowest_cpu)
		__field(int, rt_lowest_prio)
		__field(int, rt_lowest_pid)
		__field(unsigned long, uclamp_min)
		__field(unsigned long, uclamp_max)
		__field(int, sd_flag)
		__field(bool, sync)
		__field(long, task_mask)
		__field(int, cpuctl_grp_id)
		__field(int, cpuset_grp_id)
		__field(long, act_mask)
	),
	TP_fast_assign(
		__entry->pid = tsk->pid;
		__entry->policy = policy;
		__entry->target_cpu = target_cpu;
		__entry->lowest_mask = lowest_mask->bits[0];
		__entry->idle_cpus  = rt_ea_output->idle_cpus;
		__entry->cfs_cpus   = rt_ea_output->cfs_cpus;
		__entry->cfs_lowest_cpu  = rt_ea_output->cfs_lowest_cpu;
		__entry->cfs_lowest_prio   = rt_ea_output->cfs_lowest_prio;
		__entry->cfs_lowest_pid   = rt_ea_output->cfs_lowest_pid;
		__entry->rt_lowest_cpu  = rt_ea_output->rt_lowest_cpu;
		__entry->rt_lowest_prio   = rt_ea_output->rt_lowest_prio;
		__entry->rt_lowest_pid   = rt_ea_output->rt_lowest_pid;
		__entry->uclamp_min = uclamp_eff_value(tsk, UCLAMP_MIN);
		__entry->uclamp_max = uclamp_eff_value(tsk, UCLAMP_MAX);
		__entry->sd_flag = sd_flag;
		__entry->sync = sync;
		__entry->task_mask = tsk->cpus_ptr->bits[0];
		__entry->cpuctl_grp_id = sched_cgroup_state_rt(tsk, cpu_cgrp_id);
		__entry->cpuset_grp_id = sched_cgroup_state_rt(tsk, cpuset_cgrp_id);
		__entry->act_mask = cpu_active_mask->bits[0];
	),
	TP_printk(
		"pid=%4d policy=0x%08x target=%d lowest_mask=0x%lx idle_cpus=0x%x cfs_cpus=0x%x cfs_lowest_cpu=%d cfs_lowest_prio=%d cfs_lowest_pid=%d rt_lowest_cpu=%d rt_lowest_prio=%d rt_lowest_pid=%d uclamp_min=%lu uclamp_max=%lu sd_flag=%d sync=%d mask=0x%lx cpuctl=%d cpuset=%d act_mask=0x%lx",
		__entry->pid,
		__entry->policy,
		__entry->target_cpu,
		__entry->lowest_mask,
		__entry->idle_cpus,
		__entry->cfs_cpus,
		__entry->cfs_lowest_cpu,
		__entry->cfs_lowest_prio,
		__entry->cfs_lowest_pid,
		__entry->rt_lowest_cpu,
		__entry->rt_lowest_prio,
		__entry->rt_lowest_pid,
		__entry->uclamp_min,
		__entry->uclamp_max,
		__entry->sd_flag,
		__entry->sync,
		__entry->task_mask,
		__entry->cpuctl_grp_id,
		__entry->cpuset_grp_id,
		__entry->act_mask)
);

TRACE_EVENT(sched_aware_energy_rt,
	TP_PROTO(int target_cpu, unsigned long this_pwr_eff, unsigned long pwr_eff,
			unsigned int task_util),
	TP_ARGS(target_cpu, this_pwr_eff, pwr_eff, task_util),
	TP_STRUCT__entry(
		__field(int, target_cpu)
		__field(unsigned long, this_pwr_eff)
		__field(unsigned long, pwr_eff)
		__field(unsigned int, task_util)
	),
	TP_fast_assign(
		__entry->target_cpu	= target_cpu;
		__entry->this_pwr_eff	= this_pwr_eff;
		__entry->pwr_eff	= pwr_eff;
		__entry->task_util	= task_util;
	),
	TP_printk("target=%d this_pwr_eff=%lu pwr_eff=%lu util=%u",
		__entry->target_cpu,
		__entry->this_pwr_eff,
		__entry->pwr_eff,
		__entry->task_util)
);

TRACE_EVENT(sched_next_update_thermal_headroom,
	TP_PROTO(unsigned long now, unsigned long next_update_thermal),
	TP_ARGS(now, next_update_thermal),
	TP_STRUCT__entry(
		__field(unsigned long, now)
		__field(unsigned long, next_update_thermal)
	),
	TP_fast_assign(
		__entry->now = now;
		__entry->next_update_thermal = next_update_thermal;
	),
	TP_printk(
		"now_tick=%lu next_update_thermal=%lu",
		__entry->now,
		__entry->next_update_thermal)
);

TRACE_EVENT(sched_newly_idle_balance_interval,
	TP_PROTO(unsigned int interval_us),
	TP_ARGS(interval_us),
	TP_STRUCT__entry(
		__field(unsigned int, interval_us)
	),
	TP_fast_assign(
		__entry->interval_us = interval_us;
	),
	TP_printk(
		"interval_us=%u",
		__entry->interval_us)
);

TRACE_EVENT(sched_headroom_interval_tick,
	TP_PROTO(unsigned int tick),
	TP_ARGS(tick),
	TP_STRUCT__entry(
		__field(unsigned int, tick)
	),
	TP_fast_assign(
		__entry->tick = tick;
	),
	TP_printk(
		"interval =%u",
		__entry->tick)
);

#if IS_ENABLED(CONFIG_MTK_SCHED_VIP_TASK)
TRACE_EVENT(sched_get_vip_task_prio,
	TP_PROTO(struct task_struct *p, int vip_prio, bool is_ls, unsigned int ls_vip_threshold,
			unsigned int group_threshold, bool is_basic_vip),

	TP_ARGS(p, vip_prio, is_ls, ls_vip_threshold, group_threshold, is_basic_vip),

	TP_STRUCT__entry(
		__field(int, pid)
		__field(int, vip_prio)
		__field(int, prio)
		__field(bool, is_ls)
		__field(unsigned int, ls_vip_threshold)
		__field(int, cpuctl)
		__field(unsigned int, group_threshold)
		__field(bool, is_basic_vip)
	),

	TP_fast_assign(
		__entry->pid               = p->pid;
		__entry->vip_prio          = vip_prio;
		__entry->prio              = p->prio;
		__entry->is_ls             = is_ls;
		__entry->ls_vip_threshold  = ls_vip_threshold;
		__entry->cpuctl            = sched_cgroup_state(p, cpu_cgrp_id);
		__entry->group_threshold   = group_threshold;
		__entry->is_basic_vip      = is_basic_vip;
	),

	TP_printk("pid=%d vip_prio=%d prio=%d is_ls=%d ls_vip_threshold=%d cpuctl=%d group_threshold=%d is_basic_vip=%d",
		  __entry->pid, __entry->vip_prio, __entry->prio, __entry->is_ls,
		  __entry->ls_vip_threshold, __entry->cpuctl, __entry->group_threshold,
		  __entry->is_basic_vip)
);

TRACE_EVENT(sched_insert_vip_task,
	TP_PROTO(struct task_struct *p, int cpu, int vip_prio, bool at_front,
			pid_t prev_pid, pid_t next_pid, bool requeue, bool is_first_entry),

	TP_ARGS(p, cpu, vip_prio, at_front, prev_pid, next_pid, requeue,
		is_first_entry),

	TP_STRUCT__entry(
		__field(int, pid)
		__field(int, cpu)
		__field(int, vip_prio)
		__field(bool, at_front)
		__field(int, prev_pid)
		__field(int, next_pid)
		__field(bool, requeue)
		__field(bool, is_first_entry)
		__field(int, prio)
		__field(int, cpuctl)
	),

	TP_fast_assign(
		__entry->pid       = p->pid;
		__entry->cpu       = cpu;
		__entry->vip_prio  = vip_prio;
		__entry->at_front  = at_front;
		__entry->prev_pid  = prev_pid;
		__entry->next_pid  = next_pid;
		__entry->requeue   = requeue;
		__entry->is_first_entry = is_first_entry;
		__entry->prio    = p->prio;
		__entry->cpuctl  = sched_cgroup_state(p, cpu_cgrp_id);
	),

	TP_printk("pid=%d cpu=%d vip_prio=%d at_front=%d prev_pid=%d next_pid=%d requeue=%d, is_first_entry=%d, prio=%d, cpuctl=%d",
		  __entry->pid, __entry->cpu, __entry->vip_prio, __entry->at_front,
		  __entry->prev_pid, __entry->next_pid, __entry->requeue, __entry->is_first_entry,
		__entry->prio, __entry->cpuctl)
);

TRACE_EVENT(sched_deactivate_vip_task,
	TP_PROTO(pid_t pid, int cpu, pid_t prev_pid,
			pid_t next_pid),

	TP_ARGS(pid, cpu, prev_pid, next_pid),

	TP_STRUCT__entry(
		__field(int, pid)
		__field(int, cpu)
		__field(int, prev_pid)
		__field(int, next_pid)
	),

	TP_fast_assign(
		__entry->pid       = pid;
		__entry->cpu       = cpu;
		__entry->prev_pid  = prev_pid;
		__entry->next_pid  = next_pid;
	),

	TP_printk("pid=%d cpu=%d orig_prev_pid=%d orig_next_pid=%d",
		  __entry->pid, __entry->cpu, __entry->prev_pid,
		  __entry->next_pid)
);
#endif

#if IS_ENABLED(CONFIG_MTK_CORE_PAUSE)
TRACE_EVENT(sched_pause_cpus,
	TP_PROTO(struct cpumask *req_cpus, struct cpumask *last_cpus,
			u64 start_time, unsigned char pause,
			int err, struct cpumask *pause_cpus),

	TP_ARGS(req_cpus, last_cpus, start_time, pause, err, pause_cpus),

	TP_STRUCT__entry(
		__field(unsigned int, req_cpus)
		__field(unsigned int, last_cpus)
		__field(unsigned int, time)
		__field(unsigned char, pause)
		__field(int, err)
		__field(unsigned int, pause_cpus)
		__field(unsigned int, online_cpus)
		__field(unsigned int, active_cpus)
	),

	TP_fast_assign(
		__entry->req_cpus    = cpumask_bits(req_cpus)[0];
		__entry->last_cpus = cpumask_bits(last_cpus)[0];
		__entry->time        = div64_u64(sched_clock() - start_time, 1000);
		__entry->pause	     = pause;
		__entry->err         = err;
		__entry->pause_cpus    = cpumask_bits(pause_cpus)[0];
		__entry->online_cpus    = cpumask_bits(cpu_online_mask)[0];
		__entry->active_cpus    = cpumask_bits(cpu_active_mask)[0];
	),

	TP_printk("req=0x%x cpus=0x%x time=%u us paused=%d, err=%d, pause=0x%x, online=0x%x, active=0x%x",
		  __entry->req_cpus, __entry->last_cpus, __entry->time, __entry->pause,
		  __entry->err, __entry->pause_cpus, __entry->online_cpus, __entry->active_cpus)
);

TRACE_EVENT(sched_set_cpus_allowed,
	TP_PROTO(struct task_struct *p, unsigned int *dest_cpu,
		struct cpumask *new_mask, struct cpumask *valid_mask,
		struct cpumask *pause_cpus),

	TP_ARGS(p, dest_cpu, new_mask, valid_mask, pause_cpus),

	TP_STRUCT__entry(
		__field(pid_t, pid)
		__field(unsigned int, dest_cpu)
		__field(bool, kthread)
		__field(unsigned int, new_mask)
		__field(unsigned int, valid_mask)
		__field(unsigned int, pause_cpus)
	),

	TP_fast_assign(
		__entry->pid = p->pid;
		__entry->dest_cpu = *dest_cpu;
		__entry->kthread = p->flags & PF_KTHREAD;
		__entry->new_mask = cpumask_bits(new_mask)[0];
		__entry->valid_mask = cpumask_bits(valid_mask)[0];
		__entry->pause_cpus = cpumask_bits(pause_cpus)[0];
	),

	TP_printk("p=%d, dest_cpu=%d, k=%d, new_mask=0x%x, valid=0x%x, pause=0x%x",
		  __entry->pid, __entry->dest_cpu, __entry->kthread,
		  __entry->new_mask, __entry->valid_mask,
		  __entry->pause_cpus)
);

TRACE_EVENT(sched_find_lowest_rq,
	TP_PROTO(struct task_struct *tsk, int policy, int target_cpu,
		struct cpumask *avail_lowest_mask, struct cpumask *lowest_mask,
		const struct cpumask *active_mask),

	TP_ARGS(tsk, policy, target_cpu,
		avail_lowest_mask, lowest_mask, active_mask),

	TP_STRUCT__entry(
		__field(pid_t, pid)
		__field(int, policy)
		__field(int, target_cpu)
		__field(unsigned int, avail_lowest_mask)
		__field(unsigned int, lowest_mask)
		__field(unsigned int, active_mask)
	),

	TP_fast_assign(
		__entry->pid = tsk->pid;
		__entry->policy = policy;
		__entry->target_cpu = target_cpu;
		__entry->avail_lowest_mask = cpumask_bits(avail_lowest_mask)[0];
		__entry->lowest_mask = cpumask_bits(lowest_mask)[0];
		__entry->active_mask = cpumask_bits(active_mask)[0];
	),

	TP_printk(
		"pid=%4d policy=0x%08x target=%d avail_lowest_mask=0x%x lowest_mask=0x%x, active_mask:0x%x",
		__entry->pid,
		__entry->policy,
		__entry->target_cpu,
		__entry->avail_lowest_mask,
		__entry->lowest_mask,
		__entry->active_mask)
);
#endif

#if IS_ENABLED(CONFIG_MTK_SCHED_FAST_LOAD_TRACKING)
TRACE_EVENT(sched_flt_set_cpu,

	TP_PROTO(int cpu, int util),

	TP_ARGS(cpu, util),

	TP_STRUCT__entry(
		__field(int,		cpu)
		__field(int,		util)
	),

	TP_fast_assign(
		__entry->cpu		= cpu;
		__entry->util		= util;
	),

	TP_printk("cpu=%d util=%d",
		__entry->cpu, __entry->util)
);

TRACE_EVENT(sched_flt_get_cpu,

	TP_PROTO(int cpu, int util),

	TP_ARGS(cpu, util),

	TP_STRUCT__entry(
		__field(int,		cpu)
		__field(int,		util)
	),

	TP_fast_assign(
		__entry->cpu		= cpu;
		__entry->util		= util;
	),

	TP_printk("cpu=%d util=%d",
		__entry->cpu, __entry->util)
);

TRACE_EVENT(sched_flt_get_cpu_group,

	TP_PROTO(int cpu, int grp_id, int util),

	TP_ARGS(cpu, grp_id, util),

	TP_STRUCT__entry(
		__field(int,		cpu)
		__field(int,		grp_id)
		__field(int,		util)
	),

	TP_fast_assign(
		__entry->cpu		= cpu;
		__entry->grp_id		= grp_id;
		__entry->util		= util;
	),

	TP_printk("cpu=%d gp=%d util=%d",
		__entry->cpu, __entry->grp_id, __entry->util)
);

TRACE_EVENT(sched_get_pelt_group_util,

	TP_PROTO(int cpu, unsigned long long delta,
			unsigned long gp0_util, unsigned long gp1_util,
			unsigned long gp2_util, unsigned long gp3_util),

	TP_ARGS(cpu, delta, gp0_util, gp1_util, gp2_util, gp3_util),

	TP_STRUCT__entry(
		__field(int,		cpu)
		__field(unsigned long long,	delta)
		__field(unsigned long,	gp0_util)
		__field(unsigned long,	gp1_util)
		__field(unsigned long,	gp2_util)
		__field(unsigned long,	gp3_util)
	),

	TP_fast_assign(
		__entry->cpu		= cpu;
		__entry->delta		= delta;
		__entry->gp0_util		= gp0_util;
		__entry->gp1_util		= gp1_util;
		__entry->gp2_util		= gp2_util;
		__entry->gp3_util		= gp3_util;
	),

	TP_printk("cpu=%d= delta=%llu= gp_util[0]=%lu gp_util[1]=%lu gp_util[2] =%lu gp_util[3]=%lu",
		__entry->cpu, __entry->delta,
		__entry->gp0_util, __entry->gp1_util,
		__entry->gp2_util, __entry->gp3_util)
);
TRACE_EVENT(sched_gather_pelt_group_util,

	TP_PROTO(struct task_struct *p, unsigned long tsk_util, int grp_id, int cpu),

	TP_ARGS(p, tsk_util, grp_id, cpu),

	TP_STRUCT__entry(
		__array(char,		comm, TASK_COMM_LEN)
		__field(pid_t,		pid)
		__field(unsigned long,	tsk_util)
		__field(int,		grp_id)
		__field(int,		cpu)
	),

	TP_fast_assign(
		memcpy(__entry->comm, p->comm, TASK_COMM_LEN);
		__entry->pid	= p->pid;
		__entry->tsk_util	= tsk_util;
		__entry->grp_id	= grp_id;
		__entry->cpu	= cpu;
	),

	TP_printk("comm=%s pid=%d util=%lu grp_id=%d cpu=%d",
			__entry->comm, __entry->pid,
			__entry->tsk_util, __entry->grp_id,
			__entry->cpu)

);

TRACE_EVENT(sched_task_to_grp,

	TP_PROTO(struct task_struct *p, int grp_id, int ret, int type),

	TP_ARGS(p, grp_id, ret, type),

	TP_STRUCT__entry(
		__array(char,		comm, TASK_COMM_LEN)
		__field(pid_t,		pid)
		__field(int,		grp_id)
		__field(int,		ret)
		__field(int,		type)
	),

	TP_fast_assign(
		memcpy(__entry->comm, p->comm, TASK_COMM_LEN);
		__entry->pid	= p->pid;
		__entry->grp_id	= grp_id;
		__entry->ret	= ret;
		__entry->type	= type;
	),

	TP_printk("comm=%s pid=%d grp_id=%d ret=%d type=%d",
			__entry->comm, __entry->pid,
			__entry->grp_id, __entry->ret,
			__entry->type)

);

TRACE_EVENT(sched_update_history,

	TP_PROTO(struct rq *rq, struct task_struct *p, u32 runtime, int samples,
		enum task_event evt, struct flt_rq *fsrq, struct flt_task_struct *fts,
		enum history_event hisevt),

	TP_ARGS(rq, p, runtime, samples, evt, fsrq, fts, hisevt),

	TP_STRUCT__entry(
		__array(char,			comm, TASK_COMM_LEN)
		__field(pid_t,			pid)
		__field(unsigned long,		util_avg)
		__field(unsigned int,		ewma)
		__field(unsigned int,		enqueued)
		__field(unsigned int,		demand)
		__field(u32,			util_demand)
		__field(unsigned int,		runtime)
		__field(int,			samples)
		__field(enum task_event,		evt)
		__field(enum history_event,		hisevt)
		__field(u32,			hist0)
		__field(u32,			hist1)
		__field(u32,			hist2)
		__field(u32,			hist3)
		__field(u32,			hist4)
		__field(u32,			util_sum_hist0)
		__field(u32,			util_sum_hist1)
		__field(u32,			util_sum_hist2)
		__field(u32,			util_sum_hist3)
		__field(u32,			util_sum_hist4)
		__field(u32,			util_avg_history0)
		__field(u32,			util_avg_history1)
		__field(u32,			util_avg_history2)
		__field(u32,			util_avg_history3)
		__field(u32,			util_avg_history4)
	),

	TP_fast_assign(
		memcpy(__entry->comm, p->comm, TASK_COMM_LEN);
		__entry->pid		= p->pid;
		__entry->util_avg		= p->se.avg.util_avg;
		__entry->ewma		= p->se.avg.util_est.ewma;
		__entry->enqueued	= p->se.avg.util_est.enqueued;
		__entry->demand		= fts->demand;
		__entry->util_demand	= fts->util_demand;
		__entry->runtime		= runtime;
		__entry->samples		= samples;
		__entry->evt		= evt;
		__entry->hisevt		= hisevt;
		__entry->hist0	= fts->sum_history[0];
		__entry->hist1	= fts->sum_history[1];
		__entry->hist2	= fts->sum_history[2];
		__entry->hist3	= fts->sum_history[3];
		__entry->hist4	= fts->sum_history[4];
		__entry->util_sum_hist0	= fts->util_sum_history[0];
		__entry->util_sum_hist1	= fts->util_sum_history[1];
		__entry->util_sum_hist2	= fts->util_sum_history[2];
		__entry->util_sum_hist3	= fts->util_sum_history[3];
		__entry->util_sum_hist4	= fts->util_sum_history[4];
		__entry->util_avg_history0	= fts->util_avg_history[0];
		__entry->util_avg_history1	= fts->util_avg_history[1];
		__entry->util_avg_history2	= fts->util_avg_history[2];
		__entry->util_avg_history3	= fts->util_avg_history[3];
		__entry->util_avg_history4	= fts->util_avg_history[4];
	),

	TP_printk("pid=%d name=%s: runtime=%u samples=%d event=%s hisevt=%s demand=%u util_demand=%u (hist[0]=%u hist[1]=%u hist[2]=%u hist[3]=%u hist[4]=%u) (util_sum_hist[0]=%u util_sum_hist[1]=%u util_sum_hist[2]=%u util_sum_hist[3]=%u util_sum_hist[4]=%u) (util_avg_history[0]=%u util_avg_history[1]=%u util_avg_history[2]=%u util_avg_history[3]=%u util_avg_history[4]=%u) pelt(util=%lu util_enqueued=%u util_ewma=%u)",
		__entry->pid, __entry->comm,
		__entry->runtime, __entry->samples,
		task_event_names[__entry->evt], history_event_names[__entry->hisevt],
		__entry->demand, __entry->util_demand,
		__entry->hist0, __entry->hist1,
		__entry->hist2, __entry->hist3,
		__entry->hist4,
		__entry->util_sum_hist0, __entry->util_sum_hist1,
		__entry->util_sum_hist2, __entry->util_sum_hist3,
		__entry->util_sum_hist4,
		__entry->util_avg_history0, __entry->util_avg_history1,
		__entry->util_avg_history2, __entry->util_avg_history3,
		__entry->util_avg_history4,
		__entry->util_avg,
		__entry->enqueued,
		__entry->ewma)
);

TRACE_EVENT(sched_update_task_ravg,

	TP_PROTO(struct rq *rq, struct task_struct *p, u64 wallclock,
		enum task_event evt, struct flt_rq *fsrq,
		struct flt_task_struct *fts, int group_id, u64 irqtime),

	TP_ARGS(rq, p, wallclock, evt, fsrq, fts, group_id, irqtime),

	TP_STRUCT__entry(
		__field(pid_t,		pid)
		__array(char,		comm, TASK_COMM_LEN)
		__field(enum task_event,	evt)
		__field(u64,		irqtime)
		__field(int,		group_id)
		__field(u64,		wallclock)
		__field(u64,		mark_start)
		__field(u64,		window_start)
		__field(int,		cpu)
		__field(u64,		prev_runnable_sum)
		__field(u64,		curr_runnable_sum)
	),

	TP_fast_assign(
		__entry->pid			= p->pid;
		memcpy(__entry->comm, p->comm, TASK_COMM_LEN);
		__entry->evt			= evt;
		__entry->irqtime			= irqtime;
		__entry->group_id			= group_id;
		__entry->wallclock		= wallclock;
		__entry->mark_start		= fts->mark_start;
		__entry->window_start		= fsrq->window_start;
		__entry->cpu			= rq->cpu;
		__entry->prev_runnable_sum	= fsrq->prev_runnable_sum;
		__entry->curr_runnable_sum	= fsrq->curr_runnable_sum;
	),

	TP_printk("pid=%d name =%s event=%s flt_groupid=%d irqtime=%llu wc=%llu ms=%llu ws = %llu cpu=%d prev=%llu curr=%llu",
		__entry->pid, __entry->comm, task_event_names[__entry->evt],
		__entry->group_id, __entry->irqtime,
		__entry->wallclock, __entry->mark_start, __entry->window_start,
		__entry->cpu, __entry->prev_runnable_sum, __entry->curr_runnable_sum)
);

TRACE_EVENT(sched_add_to_task_demand,

	TP_PROTO(struct rq *rq, struct task_struct *p,
		struct flt_task_struct *fts, u64 delta,
		unsigned long cie, unsigned long fie,
		u64 util_sum, enum win_event evt),

	TP_ARGS(rq, p, fts, delta, cie, fie, util_sum, evt),

	TP_STRUCT__entry(
		__field(pid_t,			pid)
		__array(char,			comm, TASK_COMM_LEN)
		__field(enum win_event,		evt)
		__field(u64,			delta)
		__field(u64,			util_sum)
		__field(unsigned long,		cie)
		__field(unsigned long,		fie)
	),

	TP_fast_assign(
		__entry->pid		= p->pid;
		memcpy(__entry->comm, p->comm, TASK_COMM_LEN);
		__entry->evt		= evt;
		__entry->delta		= delta;
		__entry->util_sum		= util_sum;
		__entry->cie		= cie;
		__entry->fie		= fie;
	),

	TP_printk("pid=%d name=%s: event=%s delta = %llu util_sum = %llu CIE = %lu FIE = %lu",
		__entry->pid, __entry->comm,
		add_task_demand_names[__entry->evt], __entry->delta,
		__entry->util_sum, __entry->cie, __entry->fie)
);

TRACE_EVENT(sched_update_cpu_busy_time,

	TP_PROTO(struct rq *rq, struct task_struct *p,
		struct flt_task_struct *fts, enum cpu_update_event evt,
		u64 prev_delta, u64 curr_delta),

	TP_ARGS(rq, p, fts, evt, prev_delta, curr_delta),

	TP_STRUCT__entry(
		__field(int,			cpu)
		__field(pid_t,			pid)
		__array(char,			comm, TASK_COMM_LEN)
		__field(enum cpu_update_event,	evt)
		__field(u64,			prev_delta)
		__field(u64,			curr_delta)

	),

	TP_fast_assign(
		__entry->cpu		= rq->cpu;
		__entry->pid		= p->pid;
		memcpy(__entry->comm, p->comm, TASK_COMM_LEN);
		__entry->evt		= evt;
		__entry->prev_delta	= prev_delta;
		__entry->curr_delta	= curr_delta;
	),

	TP_printk("cpu=%d pid=%d name=%s event=%s prev_delta = %llu curr_delta = %llu",
		__entry->cpu, __entry->pid, __entry->comm,
		cpu_event_names[__entry->evt], __entry->prev_delta, __entry->curr_delta)
);

TRACE_EVENT(sched_update_cpu_history,

	TP_PROTO(struct rq *rq, struct flt_rq *fsrq, u32 samples, u64 runtime),

	TP_ARGS(rq, fsrq, samples, runtime),

	TP_STRUCT__entry(
		__field(int,			cpu)
		__field(u32,			samples)
		__field(u64,			runtime)
		__field(u32,			sum_hist0)
		__field(u32,			sum_hist1)
		__field(u32,			sum_hist2)
		__field(u32,			sum_hist3)
		__field(u32,			sum_hist4)
		__field(u32,			util_hist0)
		__field(u32,			util_hist1)
		__field(u32,			util_hist2)
		__field(u32,			util_hist3)
		__field(u32,			util_hist4)
		__field(u32,			group_sum_hist0)
		__field(u32,			group_sum_hist1)
		__field(u32,			group_sum_hist2)
		__field(u32,			group_sum_hist3)
		__field(u32,			group_sum_hist4)
		__field(u32,			group_sum_hist5)
		__field(u32,			group_sum_hist6)
		__field(u32,			group_sum_hist7)
		__field(u32,			group_sum_hist8)
		__field(u32,			group_sum_hist9)
		__field(u32,			group_sum_hist10)
		__field(u32,			group_sum_hist11)
		__field(u32,			group_sum_hist12)
		__field(u32,			group_sum_hist13)
		__field(u32,			group_sum_hist14)
		__field(u32,			group_sum_hist15)
		__field(u32,			group_sum_hist16)
		__field(u32,			group_sum_hist17)
		__field(u32,			group_sum_hist18)
		__field(u32,			group_sum_hist19)
		__field(u32,			grp_util_his0)
		__field(u32,			grp_util_his1)
		__field(u32,			grp_util_his2)
		__field(u32,			grp_util_his3)
		__field(u32,			grp_util_his4)
		__field(u32,			grp_util_his5)
		__field(u32,			grp_util_his6)
		__field(u32,			grp_util_his7)
		__field(u32,			grp_util_his8)
		__field(u32,			grp_util_his9)
		__field(u32,			grp_util_his10)
		__field(u32,			grp_util_his11)
		__field(u32,			grp_util_his12)
		__field(u32,			grp_util_his13)
		__field(u32,			grp_util_his14)
		__field(u32,			grp_util_his15)
		__field(u32,			grp_util_his16)
		__field(u32,			grp_util_his17)
		__field(u32,			grp_util_his18)
		__field(u32,			grp_util_his19)
		__field(u32,			grp_util_act_his0)
		__field(u32,			grp_util_act_his1)
		__field(u32,			grp_util_act_his2)
		__field(u32,			grp_util_act_his3)
		__field(u32,			grp_util_act_his4)
		__field(u32,			grp_util_act_his5)
		__field(u32,			grp_util_act_his6)
		__field(u32,			grp_util_act_his7)
		__field(u32,			grp_util_act_his8)
		__field(u32,			grp_util_act_his9)
		__field(u32,			grp_util_act_his10)
		__field(u32,			grp_util_act_his11)
		__field(u32,			grp_util_act_his12)
		__field(u32,			grp_util_act_his13)
		__field(u32,			grp_util_act_his14)
		__field(u32,			grp_util_act_his15)
		__field(u32,			grp_util_act_his16)
		__field(u32,			grp_util_act_his17)
		__field(u32,			grp_util_act_his18)
		__field(u32,			grp_util_act_his19)
	),

	TP_fast_assign(
		__entry->cpu		= rq->cpu;
		__entry->samples		= samples;
		__entry->runtime		= runtime;
		__entry->sum_hist0	= fsrq->sum_history[0];
		__entry->sum_hist1	= fsrq->sum_history[1];
		__entry->sum_hist2	= fsrq->sum_history[2];
		__entry->sum_hist3	= fsrq->sum_history[3];
		__entry->sum_hist4	= fsrq->sum_history[4];
		__entry->util_hist0	= fsrq->util_history[0];
		__entry->util_hist1	= fsrq->util_history[1];
		__entry->util_hist2	= fsrq->util_history[2];
		__entry->util_hist3	= fsrq->util_history[3];
		__entry->util_hist4	= fsrq->util_history[4];
		__entry->group_sum_hist0	= fsrq->group_sum_history[0][0];
		__entry->group_sum_hist1	= fsrq->group_sum_history[0][1];
		__entry->group_sum_hist2	= fsrq->group_sum_history[0][2];
		__entry->group_sum_hist3	= fsrq->group_sum_history[0][3];
		__entry->group_sum_hist4	= fsrq->group_sum_history[1][0];
		__entry->group_sum_hist5	= fsrq->group_sum_history[1][1];
		__entry->group_sum_hist6	= fsrq->group_sum_history[1][2];
		__entry->group_sum_hist7	= fsrq->group_sum_history[1][3];
		__entry->group_sum_hist8	= fsrq->group_sum_history[2][0];
		__entry->group_sum_hist9	= fsrq->group_sum_history[2][1];
		__entry->group_sum_hist10	= fsrq->group_sum_history[2][2];
		__entry->group_sum_hist11	= fsrq->group_sum_history[2][3];
		__entry->group_sum_hist12	= fsrq->group_sum_history[3][0];
		__entry->group_sum_hist13	= fsrq->group_sum_history[3][1];
		__entry->group_sum_hist14	= fsrq->group_sum_history[3][2];
		__entry->group_sum_hist15	= fsrq->group_sum_history[3][3];
		__entry->group_sum_hist16	= fsrq->group_sum_history[4][0];
		__entry->group_sum_hist17	= fsrq->group_sum_history[4][1];
		__entry->group_sum_hist18	= fsrq->group_sum_history[4][2];
		__entry->group_sum_hist19	= fsrq->group_sum_history[4][3];
		__entry->grp_util_his0	= fsrq->group_util_history[0][0];
		__entry->grp_util_his1	= fsrq->group_util_history[0][1];
		__entry->grp_util_his2	= fsrq->group_util_history[0][2];
		__entry->grp_util_his3	= fsrq->group_util_history[0][3];
		__entry->grp_util_his4	= fsrq->group_util_history[1][0];
		__entry->grp_util_his5	= fsrq->group_util_history[1][1];
		__entry->grp_util_his6	= fsrq->group_util_history[1][2];
		__entry->grp_util_his7	= fsrq->group_util_history[1][3];
		__entry->grp_util_his8	= fsrq->group_util_history[2][0];
		__entry->grp_util_his9	= fsrq->group_util_history[2][1];
		__entry->grp_util_his10	= fsrq->group_util_history[2][2];
		__entry->grp_util_his11	= fsrq->group_util_history[2][3];
		__entry->grp_util_his12	= fsrq->group_util_history[3][0];
		__entry->grp_util_his13	= fsrq->group_util_history[3][1];
		__entry->grp_util_his14	= fsrq->group_util_history[3][2];
		__entry->grp_util_his15	= fsrq->group_util_history[3][3];
		__entry->grp_util_his16	= fsrq->group_util_history[4][0];
		__entry->grp_util_his17	= fsrq->group_util_history[4][1];
		__entry->grp_util_his18	= fsrq->group_util_history[4][2];
		__entry->grp_util_his19	= fsrq->group_util_history[4][3];
		__entry->grp_util_act_his0	= fsrq->group_util_active_history[0][0];
		__entry->grp_util_act_his1	= fsrq->group_util_active_history[0][1];
		__entry->grp_util_act_his2	= fsrq->group_util_active_history[0][2];
		__entry->grp_util_act_his3	= fsrq->group_util_active_history[0][3];
		__entry->grp_util_act_his4	= fsrq->group_util_active_history[0][4];
		__entry->grp_util_act_his5	= fsrq->group_util_active_history[1][0];
		__entry->grp_util_act_his6	= fsrq->group_util_active_history[1][1];
		__entry->grp_util_act_his7	= fsrq->group_util_active_history[1][2];
		__entry->grp_util_act_his8	= fsrq->group_util_active_history[1][3];
		__entry->grp_util_act_his9	= fsrq->group_util_active_history[1][4];
		__entry->grp_util_act_his10	= fsrq->group_util_active_history[2][0];
		__entry->grp_util_act_his11	= fsrq->group_util_active_history[2][1];
		__entry->grp_util_act_his12	= fsrq->group_util_active_history[2][2];
		__entry->grp_util_act_his13	= fsrq->group_util_active_history[2][3];
		__entry->grp_util_act_his14	= fsrq->group_util_active_history[2][4];
		__entry->grp_util_act_his15	= fsrq->group_util_active_history[3][0];
		__entry->grp_util_act_his16	= fsrq->group_util_active_history[3][1];
		__entry->grp_util_act_his17	= fsrq->group_util_active_history[3][2];
		__entry->grp_util_act_his18	= fsrq->group_util_active_history[3][3];
		__entry->grp_util_act_his19	= fsrq->group_util_active_history[3][4];
	),

	TP_printk("cpu=%d, sa=%u rt=%llu rqs[0]=%u rqs[1]=%u rqs[2]=%u rqs[3]=%u rqs[4]=%u  rqu[0]=%u rqu[1]=%u rqu[2]=%u rqu[3]=%u rqu[4]=%u grp1s[0]=%u grp1s[1]=%u grp1s[2]=%u grp1s[3]=%u grp1s[4]=%u grp2s[0]=%u grp2s[1]=%u grp2s[2]=%u grp2s[3]=%u grp2s[4]=%u grp3s[0]=%u grp3s[1]=%u grp3s[2]=%u grp3s[3]=%u grp3s[4]=%u grp4s[0]=%u grp4s[1]=%u grp4s[2]=%u grp4s[3]=%u grp4s[4]=%u  grp1u[0]=%u grp1u[1]=%u grp1u[2]=%u grp1u[3]=%u grp1u[4]=%u grp2u[0]=%u grp2u[1]=%u grp2u[2]=%u grp2u[3]=%u grp2u[4]=%u grp3u[0]=%u grp3u[1]=%u grp3u[2]=%u grp3u[3]=%u grp3u[4]=%u grp4u[0]=%u grp4u[1]=%u grp4u[2]=%u grp4u[3]=%u grp4u[4]=%u grp1a[0]=%u grp1a[1]=%u grp1a[2]=%u grp1a[3]=%u grp1a[4]=%u grp2a[0]=%u grp2a[1]=%u grp2a[2]=%u grp2a[3]=%u grp2a[4]=%u grp3a[0]=%u grp3a[1]=%u grp3a[2]=%u grp3a[3]=%u grp3a[4]=%u grp4a[0]=%u grp4a[1]=%u grp4a[2]=%u grp4a[3]=%u grp4a[4]=%u",
		__entry->cpu, __entry->samples,  __entry->runtime,
		__entry->sum_hist0, __entry->sum_hist1,
		__entry->sum_hist2, __entry->sum_hist3,
		__entry->sum_hist4,
		__entry->util_hist0, __entry->util_hist1,
		__entry->util_hist2, __entry->util_hist3,
		__entry->util_hist4,
		__entry->group_sum_hist0, __entry->group_sum_hist4,
		__entry->group_sum_hist8, __entry->group_sum_hist12,
		__entry->group_sum_hist16,
		__entry->group_sum_hist1, __entry->group_sum_hist5,
		__entry->group_sum_hist9, __entry->group_sum_hist13,
		__entry->group_sum_hist17,
		__entry->group_sum_hist2, __entry->group_sum_hist6,
		__entry->group_sum_hist10, __entry->group_sum_hist14,
		__entry->group_sum_hist18,
		__entry->group_sum_hist3, __entry->group_sum_hist7,
		__entry->group_sum_hist11, __entry->group_sum_hist15,
		__entry->group_sum_hist19,
		__entry->grp_util_his0, __entry->grp_util_his4,
		__entry->grp_util_his8, __entry->grp_util_his12,
		__entry->grp_util_his16,
		__entry->grp_util_his1, __entry->grp_util_his5,
		__entry->grp_util_his9, __entry->grp_util_his13,
		__entry->grp_util_his17,
		__entry->grp_util_his2, __entry->grp_util_his6,
		__entry->grp_util_his10, __entry->grp_util_his14,
		__entry->grp_util_his18,
		__entry->grp_util_his3, __entry->grp_util_his7,
		__entry->grp_util_his11, __entry->grp_util_his15,
		__entry->grp_util_his19,
		__entry->grp_util_act_his0, __entry->grp_util_act_his1,
		__entry->grp_util_act_his2, __entry->grp_util_act_his3,
		__entry->grp_util_act_his4,
		__entry->grp_util_act_his5, __entry->grp_util_act_his6,
		__entry->grp_util_act_his7, __entry->grp_util_act_his8,
		__entry->grp_util_act_his9,
		__entry->grp_util_act_his10, __entry->grp_util_act_his11,
		__entry->grp_util_act_his12, __entry->grp_util_act_his13,
		__entry->grp_util_act_his14,
		__entry->grp_util_act_his15, __entry->grp_util_act_his16,
		__entry->grp_util_act_his17, __entry->grp_util_act_his18,
		__entry->grp_util_act_his19)
);

TRACE_EVENT(sched_rollover_task_window,

	TP_PROTO(struct task_struct *p, struct flt_task_struct *fts, bool full_window),

	TP_ARGS(p, fts, full_window),

	TP_STRUCT__entry(
		__field(pid_t,			pid)
		__array(char,			comm, TASK_COMM_LEN)
		__field(bool,			full_window)
		__field(u32,			prev_window)
		__field(u32,			curr_window)
	),

	TP_fast_assign(
		__entry->pid			= p->pid;
		memcpy(__entry->comm, p->comm, TASK_COMM_LEN);
		__entry->full_window		= full_window;
		__entry->prev_window		= fts->prev_window;
		__entry->curr_window		= fts->curr_window;
	),

	TP_printk("pid=%d name=%s  full_window=%d prev_window=%u curr_window=%u",
		__entry->pid, __entry->comm, __entry->full_window,
		__entry->prev_window, __entry->curr_window)
);

TRACE_EVENT(sched_enq_deq_task,

	TP_PROTO(struct task_struct *p, bool enqueue,
			unsigned int cpus_allowed, struct flt_rq *fsrq),

	TP_ARGS(p, enqueue, cpus_allowed, fsrq),

	TP_STRUCT__entry(
		__array(char,		comm, TASK_COMM_LEN)
		__field(pid_t,		pid)
		__field(int,		prio)
		__field(int,		cpu)
		__field(int,		gp0_cnt)
		__field(int,		gp1_cnt)
		__field(int,		gp2_cnt)
		__field(int,		gp3_cnt)
		__field(bool,		enqueue)
		__field(unsigned int,	nr_running)
		__field(unsigned int,	rt_nr_running)
		__field(unsigned int,	cpus_allowed)
		__field(u64,		flt_cpu_util)
		__field(unsigned long,	cfs_util)
		__field(unsigned long,	rt_util)
	),

	TP_fast_assign(
		memcpy(__entry->comm, p->comm, TASK_COMM_LEN);
		__entry->pid		= p->pid;
		__entry->prio		= p->prio;
		__entry->cpu		= task_cpu(p);
		__entry->enqueue		= enqueue;
		__entry->nr_running	= task_rq(p)->nr_running;
		__entry->rt_nr_running	= task_rq(p)->rt.rt_nr_running;
		__entry->cpus_allowed	= cpus_allowed;
		__entry->gp0_cnt		= fsrq->group_nr_running[0];
		__entry->gp1_cnt		= fsrq->group_nr_running[1];
		__entry->gp2_cnt		= fsrq->group_nr_running[2];
		__entry->gp3_cnt		= fsrq->group_nr_running[3];
		__entry->cfs_util		= cpu_util_cfs(task_rq(p)->cpu);
		__entry->rt_util		= cpu_util_rt(task_rq(p));
	),

	TP_printk("cpu=%d name=%s comm=%s pid=%d prio=%d nr_running=%u rt_nr_running=%u affine=%x cfs_util=%lu rt_util=%lu gp0=[%d] gp1=[%d] gp2=[%d] gp3=[%d]",
		__entry->cpu,
		__entry->enqueue ? "enqueue" : "dequeue",
		__entry->comm, __entry->pid,
		__entry->prio, __entry->nr_running,
		__entry->rt_nr_running, __entry->cpus_allowed,
		__entry->cfs_util, __entry->rt_util,
		__entry->gp0_cnt, __entry->gp1_cnt,
		__entry->gp2_cnt, __entry->gp3_cnt)
);

TRACE_EVENT(sched_get_group_running_task_cnt,

	TP_PROTO(int group_id, int win_idx, int cnt),

	TP_ARGS(group_id, win_idx, cnt),

	TP_STRUCT__entry(
		__field(int,		group_id)
		__field(int,		win_idx)
		__field(int,		cnt)
	),

	TP_fast_assign(
		__entry->group_id		= group_id;
		__entry->win_idx		= win_idx;
		__entry->cnt		= cnt;
	),

	TP_printk("grp_id=%d wc=%d  res=%d",
		__entry->group_id, __entry->win_idx, __entry->cnt)
);

TRACE_EVENT(sched_get_group_util,

	TP_PROTO(int group_id, int window_count, int weight_policy, int res, int type, int hint),

	TP_ARGS(group_id, window_count, weight_policy, res, type, hint),

	TP_STRUCT__entry(
		__field(int,		type)
		__field(int,		group_id)
		__field(int,		window_count)
		__field(int,		weight_policy)
		__field(int,		res)
		__field(int,		hint)
	),

	TP_fast_assign(
		__entry->type		= type;
		__entry->group_id		= group_id;
		__entry->window_count	= window_count;
		__entry->weight_policy	= weight_policy;
		__entry->res		= res;
		__entry->hint		= hint;
	),

	TP_printk("type=%d grp_id=%d wc=%d wp=%d res=%d hint %d",
		__entry->type, __entry->group_id, __entry->window_count,
		__entry->weight_policy, __entry->res, __entry->hint)
);

TRACE_EVENT(sched_get_cpu_group_util,

	TP_PROTO(int cpu, int group_id, int util),

	TP_ARGS(cpu, group_id, util),

	TP_STRUCT__entry(
		__field(int,		cpu)
		__field(int,		group_id)
		__field(int,		util)
	),

	TP_fast_assign(
		__entry->cpu		= cpu;
		__entry->group_id		= group_id;
		__entry->util		= util;
	),

	TP_printk("cpu =%d gp=%d gp_util=%d",
		__entry->cpu, __entry->group_id, __entry->util)
);
#endif

#endif /* _TRACE_SCHEDULER_H */

#undef TRACE_INCLUDE_PATH
#define TRACE_INCLUDE_PATH eas
#undef TRACE_INCLUDE_FILE
#define TRACE_INCLUDE_FILE eas_trace
/* This part must be outside protection */
#include <trace/define_trace.h>
