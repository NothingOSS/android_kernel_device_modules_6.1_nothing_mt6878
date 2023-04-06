// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2023 MediaTek Inc.
 */
#include <linux/sched.h>
#include <linux/sched/cputime.h>
#include <sched/sched.h>
#include <trace/hooks/sched.h>
#include "common.h"
#include "vip.h"
#include "eas_trace.h"

bool vip_enable = true;

unsigned int ls_vip_threshold          =  DEFAULT_VIP_PRIO_THRESHOLD;
struct VIP_task_group vtg;

DEFINE_PER_CPU(struct vip_rq, vip_rq);

inline unsigned int num_vip_in_cpu(int cpu)
{
	struct vip_rq *vrq = &per_cpu(vip_rq, cpu);

	return vrq->num_vip_tasks;
}

struct task_struct *vts_to_ts(struct vip_task_struct *vts)
{
	struct mtk_task *mts = container_of(vts, struct mtk_task, vip_task);
	struct task_struct *ts = mts_to_ts(mts);
	return ts;
}

pid_t list_head_to_pid(struct list_head *lh)
{
	pid_t pid = vts_to_ts(container_of(lh, struct vip_task_struct, vip_list))->pid;

	/* means list_head is from rq */
	if (!pid)
		pid = 0;
	return pid;
}

bool task_is_vip(struct task_struct *p)
{
	struct vip_task_struct *vts = &((struct mtk_task *) p->android_vendor_data1)->vip_task;

	return (vts->vip_prio != NOT_VIP);
}

static inline unsigned int vip_task_limit(struct task_struct *p)
{
	return VIP_TIME_LIMIT;
}

static void insert_vip_task(struct rq *rq, struct vip_task_struct *vts,
					bool at_front, bool requeue)
{
	struct list_head *pos;
	struct vip_rq *vrq = &per_cpu(vip_rq, cpu_of(rq));

	list_for_each(pos, &vrq->vip_tasks) {
		struct vip_task_struct *tmp_vts = container_of(pos, struct vip_task_struct,
								vip_list);
		if (at_front) {
			if (vts->vip_prio >= tmp_vts->vip_prio)
				break;
		} else {
			if (vts->vip_prio > tmp_vts->vip_prio)
				break;
		}
	}
	list_add(&vts->vip_list, pos->prev);
	if (!requeue)
		vrq->num_vip_tasks += 1;

	/* vip inserted trace event */
	if (trace_sched_insert_vip_task_enabled()) {
		pid_t prev_pid = list_head_to_pid(vts->vip_list.prev);
		pid_t next_pid = list_head_to_pid(vts->vip_list.next);
		bool is_first_entry = (prev_pid == 0) ? true : false;

		trace_sched_insert_vip_task(vts_to_ts(vts)->pid, cpu_of(rq), vts->vip_prio,
			at_front, prev_pid, next_pid, requeue, is_first_entry);
	}
}

/* top-app interface */
void set_top_app_vip(unsigned int prio)
{
	vtg.threshold[VIP_GROUP_TOPAPP] = prio;
	vtg.enable[VIP_GROUP_TOPAPP] = 1;
}
EXPORT_SYMBOL_GPL(set_top_app_vip);

void unset_top_app_vip(void)
{
	vtg.threshold[VIP_GROUP_TOPAPP] = DEFAULT_VIP_PRIO_THRESHOLD;
	vtg.enable[VIP_GROUP_TOPAPP] = 0;
}
EXPORT_SYMBOL_GPL(unset_top_app_vip);
/* end of top-app interface */

/* foreground interface */
void set_foreground_vip(unsigned int prio)
{
	vtg.threshold[VIP_GROUP_FOREGROUND] = prio;
	vtg.enable[VIP_GROUP_FOREGROUND] = 1;
}
EXPORT_SYMBOL_GPL(set_foreground_vip);

void unset_foreground_vip(void)
{
	vtg.threshold[VIP_GROUP_FOREGROUND] = DEFAULT_VIP_PRIO_THRESHOLD;
	vtg.enable[VIP_GROUP_FOREGROUND] = 0;
}
EXPORT_SYMBOL_GPL(unset_foreground_vip);
/* end of foreground interface */

/* background interface */
void set_background_vip(unsigned int prio)
{
	vtg.threshold[VIP_GROUP_BACKGROUND] = prio;
	vtg.enable[VIP_GROUP_BACKGROUND] = 1;
}
EXPORT_SYMBOL_GPL(set_background_vip);

void unset_background_vip(void)
{
	vtg.threshold[VIP_GROUP_BACKGROUND] = DEFAULT_VIP_PRIO_THRESHOLD;
	vtg.enable[VIP_GROUP_BACKGROUND] = 0;
}
EXPORT_SYMBOL_GPL(unset_background_vip);
/* end of background interface */

int get_group_id(struct task_struct *p)
{
	struct cgroup_subsys_state *css = task_css(p, cpu_cgrp_id);
	const char *group_name = css->cgroup->kn->name;

	if (!strcmp(group_name, "top-app"))
		return VIP_GROUP_TOPAPP;
	else if (!strcmp(group_name, "foreground"))
		return VIP_GROUP_FOREGROUND;
	else if (!strcmp(group_name, "background"))
		return VIP_GROUP_BACKGROUND;

	return -1;
}

bool is_VIP_task_group(struct task_struct *p)
{
	int group_id = get_group_id(p);

	if (group_id >= 0 && vtg.enable[group_id] &&
			p->prio <= vtg.threshold[group_id])
		return true;

	return false;
}

/* ls vip interface */
void set_ls_task_vip(unsigned int prio)
{
	ls_vip_threshold = prio;
}
EXPORT_SYMBOL_GPL(set_ls_task_vip);

void unset_ls_task_vip(void)
{
	ls_vip_threshold = DEFAULT_VIP_PRIO_THRESHOLD;
}
EXPORT_SYMBOL_GPL(unset_ls_task_vip);
/* end of ls vip interface */

bool is_VIP_latency_sensitive(struct task_struct *p)
{
	if (is_task_latency_sensitive(p) && p->prio <= ls_vip_threshold)
		return true;

	return false;
}

int is_VVIP(struct task_struct *p)
{
	return 0;
}

inline int get_vip_task_prio(struct task_struct *p)
{
	/* prio = 1 */
	if (is_VVIP(p))
		return VVIP;

	/* prio = 0 */
	if (is_VIP_task_group(p) || is_VIP_latency_sensitive(p))
		return WORKER_VIP;

	return NOT_VIP;
}

void vip_enqueue_task(struct rq *rq, struct task_struct *p)
{
	struct vip_task_struct *vts = &((struct mtk_task *) p->android_vendor_data1)->vip_task;

	if (unlikely(!vip_enable))
		return;

	if (vts->vip_prio == NOT_VIP)
		return;

	/*
	 * This can happen during migration or enq/deq for prio/class change.
	 * it was once VIP but got demoted, it will not be VIP until
	 * it goes to sleep again.
	 */
	if (vts->total_exec > vip_task_limit(p))
		return;

	insert_vip_task(rq, vts, task_on_cpu(rq, p), false);

	/*
	 * We inserted the task at the appropriate position. Take the
	 * task runtime snapshot. From now onwards we use this point as a
	 * baseline to enforce the slice and demotion.
	 */
	if (!vts->total_exec) /* queue after sleep */
		vts->sum_exec_snapshot = p->se.sum_exec_runtime;
}

static void deactivate_vip_task(struct task_struct *p, struct rq *rq)
{
	struct vip_task_struct *vts = &((struct mtk_task *) p->android_vendor_data1)->vip_task;
	struct vip_rq *vrq = &per_cpu(vip_rq, cpu_of(rq));
	struct list_head *prev = vts->vip_list.prev;
	struct list_head *next = vts->vip_list.next;

	list_del_init(&vts->vip_list);
	vts->vip_prio = NOT_VIP;
	vrq->num_vip_tasks -= 1;

	if (trace_sched_deactivate_vip_task_enabled()) {
		pid_t prev_pid = list_head_to_pid(prev);
		pid_t next_pid = list_head_to_pid(next);

		trace_sched_deactivate_vip_task(p->pid, task_cpu(p), prev_pid, next_pid);
	}
}

/*
 * VIP task runtime update happens here. Three possibilities:
 *
 * de-activated: The VIP consumed its runtime. Non VIP can preempt.
 * slice expired: VIP slice is expired and other VIP can preempt.
 * slice not expired: This VIP task can continue to run.
 */
static void account_vip_runtime(struct rq *rq, struct task_struct *curr)
{
	struct vip_task_struct *vts = &((struct mtk_task *) curr->android_vendor_data1)->vip_task;
	struct vip_rq *vrq = &per_cpu(vip_rq, cpu_of(rq));
	s64 delta;
	unsigned int limit;

	lockdep_assert_held(&rq->__lock);

	/*
	 * RQ clock update happens in tick path in the scheduler.
	 * Since we drop the lock in the scheduler before calling
	 * into vendor hook, it is possible that update flags are
	 * reset by another rq lock and unlock. Do the update here
	 * if required.
	 */
	if (!(rq->clock_update_flags & RQCF_UPDATED))
		update_rq_clock(rq);

	/* sum_exec_snapshot can be ahead. See below increment */
	delta = curr->se.sum_exec_runtime - vts->sum_exec_snapshot;
	if (delta < 0)
		delta = 0;
	else
		delta += rq_clock_task(rq) - curr->se.exec_start;
	/* slice is not expired */
	if (delta < VIP_TIME_SLICE)
		return;

	/*
	 * slice is expired, check if we have to deactivate the
	 * VIP task, otherwise requeue the task in the list so
	 * that other VIP tasks gets a chance.
	 */
	vts->sum_exec_snapshot += delta;
	vts->total_exec += delta;

	limit = vip_task_limit(curr);
	if (vts->total_exec > limit) {
		deactivate_vip_task(curr, rq);
		return;
	}

	/* only this vip task in rq, skip re-queue section */
	if (vrq->num_vip_tasks == 1)
		return;

	/* slice expired. re-queue the task */
	list_del(&vts->vip_list);
	insert_vip_task(rq, vts, false, true);
}

void vip_check_preempt_wakeup(void *unused, struct rq *rq, struct task_struct *p,
				bool *preempt, bool *nopreempt, int wake_flags,
				struct sched_entity *se, struct sched_entity *pse,
				int next_buddy_marked, unsigned int granularity)
{
	struct vip_rq *vrq = &per_cpu(vip_rq, cpu_of(rq));
	struct vip_task_struct *vts_p = &((struct mtk_task *) p->android_vendor_data1)->vip_task;
	struct task_struct *c = rq->curr;
	struct vip_task_struct *vts_c;
	bool resched = false;
	bool p_is_vip, curr_is_vip;

	vts_c = &((struct mtk_task *) rq->curr->android_vendor_data1)->vip_task;

	if (unlikely(!vip_enable))
		return;

	p_is_vip = !list_empty(&vts_p->vip_list) && vts_p->vip_list.next;
	curr_is_vip = !list_empty(&vts_c->vip_list) && vts_c->vip_list.next;
	/*
	 * current is not VIP, so preemption decision
	 * is simple.
	 */
	if (!curr_is_vip) {
		if (p_is_vip)
			goto preempt;
		return; /* CFS decides preemption */
	}

	/*
	 * current is VIP. update its runtime before deciding the
	 * preemption.
	 */
	account_vip_runtime(rq, c);
	resched = (vrq->vip_tasks.next != &vts_c->vip_list);
	/*
	 * current is no longer eligible to run. It must have been
	 * picked (because of VIP) ahead of other tasks in the CFS
	 * tree, so drive preemption to pick up the next task from
	 * the tree, which also includes picking up the first in
	 * the VIP queue.
	 */
	if (resched)
		goto preempt;

	/* current is the first in the queue, so no preemption */
	*nopreempt = true;
	return;
preempt:
	*preempt = true;
}

void vip_cfs_tick(struct rq *rq)
{
	struct vip_rq *vrq = &per_cpu(vip_rq, cpu_of(rq));
	struct vip_task_struct *vts;
	struct rq_flags rf;

	vts = &((struct mtk_task *) rq->curr->android_vendor_data1)->vip_task;

	if (unlikely(!vip_enable))
		return;

	rq_lock(rq, &rf);

	if (list_empty(&vts->vip_list) || (vts->vip_list.next == NULL))
		goto out;
	account_vip_runtime(rq, rq->curr);
	/*
	 * If the current is not VIP means, we have to re-schedule to
	 * see if we can run any other task including VIP tasks.
	 */
	if ((vrq->vip_tasks.next != &vts->vip_list) && rq->cfs.h_nr_running > 1)
		resched_curr(rq);

out:
	rq_unlock(rq, &rf);
}

void vip_lb_tick(struct rq *rq)
{
	vip_cfs_tick(rq);
}

void vip_scheduler_tick(void *unused, struct rq *rq)
{
	struct task_struct *p = rq->curr;

	if (unlikely(!vip_enable))
		return;

	if (!vip_fair_task(p))
		return;

	vip_lb_tick(rq);
}
#if IS_ENABLED(CONFIG_FAIR_GROUP_SCHED)
/* Walk up scheduling entities hierarchy */
#define for_each_sched_entity(se) \
	for (; se; se = se->parent)
#else
#define for_each_sched_entity(se) \
	for (; se; se = NULL)
#endif

extern void set_next_entity(struct cfs_rq *cfs_rq, struct sched_entity *se);
void vip_replace_next_task_fair(void *unused, struct rq *rq, struct task_struct **p,
				struct sched_entity **se, bool *repick, bool simple,
				struct task_struct *prev)
{
	struct vip_rq *vrq = &per_cpu(vip_rq, cpu_of(rq));
	struct vip_task_struct *vts;
	struct task_struct *vip;


	if (unlikely(!vip_enable))
		return;

	/* We don't have VIP tasks queued */
	if (list_empty(&vrq->vip_tasks))
		return;

	/* Return the first task from VIP queue */
	vts = list_first_entry(&vrq->vip_tasks, struct vip_task_struct, vip_list);
	vip = vts_to_ts(vts);

	*p = vip;
	*se = &vip->se;
	*repick = true;

	if (simple) {
		for_each_sched_entity((*se))
			set_next_entity(cfs_rq_of(*se), *se);
	}
}

void vip_dequeue_task(void *unused, struct rq *rq, struct task_struct *p, int flags)
{
	struct vip_task_struct *vts = &((struct mtk_task *) p->android_vendor_data1)->vip_task;

	if (unlikely(!vip_enable))
		return;

	if (!list_empty(&vts->vip_list) && vts->vip_list.next)
		deactivate_vip_task(p, rq);

	/*
	 * Reset the exec time during sleep so that it starts
	 * from scratch upon next wakeup. total_exec should
	 * be preserved when task is enq/deq while it is on
	 * runqueue.
	 */

	if (p->__state != TASK_RUNNING)
		vts->total_exec = 0;
}

inline bool vip_fair_task(struct task_struct *p)
{
	return p->prio >= MAX_RT_PRIO && !is_idle_task(p);
}

void init_vip_task_struct(struct task_struct *p)
{
	struct vip_task_struct *vts = &((struct mtk_task *) p->android_vendor_data1)->vip_task;

	INIT_LIST_HEAD(&vts->vip_list);
	vts->sum_exec_snapshot = 0;
	vts->total_exec = 0;
	vts->vip_prio = NOT_VIP;
}

void init_task_gear_hints(struct task_struct *p)
{
	struct task_gear_hints *ghts = &((struct mtk_task *) p->android_vendor_data1)->gear_hints;

	ghts->gear_start = -1;
	ghts->num_gear   = num_sched_clusters;
}

static void vip_new_tasks(void *unused, struct task_struct *new)
{
	init_vip_task_struct(new);
	init_task_gear_hints(new);
}

void register_vip_hooks(void)
{
	int ret = 0;

	ret = register_trace_android_rvh_wake_up_new_task(vip_new_tasks, NULL);
	if (ret)
		pr_info("register wake_up_new_task hooks failed, returned %d\n", ret);

	ret = register_trace_android_rvh_check_preempt_wakeup(vip_check_preempt_wakeup, NULL);
	if (ret)
		pr_info("register check_preempt_wakeup hooks failed, returned %d\n", ret);

	ret = register_trace_android_vh_scheduler_tick(vip_scheduler_tick, NULL);
	if (ret)
		pr_info("register scheduler_tick failed\n");

	ret = register_trace_android_rvh_replace_next_task_fair(vip_replace_next_task_fair, NULL);
	if (ret)
		pr_info("register replace_next_task_fair hooks failed, returned %d\n", ret);

	ret = register_trace_android_rvh_after_dequeue_task(vip_dequeue_task, NULL);
	if (ret)
		pr_info("register after_dequeue_task hooks failed, returned %d\n", ret);
}

void vip_init(void)
{
	struct task_struct *g, *p;
	int cpu;
	int i;

	for (i = 0; i < VIP_GROUP_NUM; i++) {
		vtg.enable[i] = 0;
		vtg.threshold[i] = DEFAULT_VIP_PRIO_THRESHOLD;
	}

	/* init vip related value to exist tasks */
	read_lock(&tasklist_lock);
	do_each_thread(g, p) {
		init_vip_task_struct(p);
	} while_each_thread(g, p);
	read_unlock(&tasklist_lock);

	/* init vip related value to idle thread */
	for_each_possible_cpu(cpu) {
		struct vip_rq *vrq = &per_cpu(vip_rq, cpu);

		INIT_LIST_HEAD(&vrq->vip_tasks);
		vrq->num_vip_tasks = 0;
	}

	/* init vip related value to newly forked tasks */
	register_vip_hooks();
}
