// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2023 MediaTek Inc.
 */
#include <linux/sched.h>
#include <linux/sched/cputime.h>
#include <sched/sched.h>
#include <trace/hooks/sched.h>
#include "vip.h"
#include "common.h"

bool vip_enable = true;
bool allow_VIP_task_group;
bool allow_VIP_ls;

DEFINE_PER_CPU(struct list_head *, vip_tasks_per_cpu);

static inline unsigned int vip_task_limit(struct task_struct *p)
{
	struct vip_task_struct *vts = &((struct mtk_task *) p->android_vendor_data1)->vip_task;

	/* Binder VIP tasks are high prio but have only single slice */
	if (vts->vip_prio == BINDER_VIP)
		return VIP_TIME_SLICE;

	return VIP_TIME_LIMIT;
}


static void insert_vip_task(struct rq *rq, struct vip_task_struct *vts,
				     bool at_front)
{
	struct list_head *pos;
	struct list_head *vip_tasks = per_cpu(vip_tasks_per_cpu, cpu_of(rq));

	list_for_each(pos, vip_tasks) {
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
}

int VIP_task_group[FLT_GROUP_NUM] = {0};
void set_task_group_VIP(int group_id)
{
	VIP_task_group[group_id] = 1;
}

void deactivate_task_group_VIP(int group_id)
{
	VIP_task_group[group_id] = 0;
}

bool is_VIP_task_group(struct task_struct *p)
{
	int group_id = -1;

	if (!allow_VIP_task_group)
		return false;

	if (group_id >= 0 && VIP_task_group[group_id])
		return true;
	return false;
}

bool is_VIP_latency_sensitive(struct task_struct *p)
{
	if (!allow_VIP_ls)
		return false;

	if (uclamp_latency_sensitive(p))
		return true;
	return false;
}

static inline int get_vip_task_prio(struct task_struct *p)
{
	if (is_VIP_task_group(p) || is_VIP_latency_sensitive(p))
		return WORKER_VIP;

	return NOT_VIP;
}

void vip_enqueue_task(struct rq *rq, struct task_struct *p)
{
	struct vip_task_struct *vts = &((struct mtk_task *) p->android_vendor_data1)->vip_task;
	int vip_prio = get_vip_task_prio(p);

	if (unlikely(!vip_enable))
		return;

	if (vip_prio == NOT_VIP)
		return;

	/*
	 * This can happen during migration or enq/deq for prio/class change.
	 * it was once VIP but got demoted, it will not be VIP until
	 * it goes to sleep again.
	 */
	if (vts->total_exec > vip_task_limit(p))
		return;

	vts->vip_prio = vip_prio;
	insert_vip_task(rq, vts, task_on_cpu(rq, p));

	/*
	 * We inserted the task at the appropriate position. Take the
	 * task runtime snapshot. From now onwards we use this point as a
	 * baseline to enforce the slice and demotion.
	 */
	if (!vts->total_exec) /* queue after sleep */
		vts->sum_exec_snapshot = p->se.sum_exec_runtime;
}

static void deactivate_vip_task(struct task_struct *p)
{
	struct vip_task_struct *vts = &((struct mtk_task *) p->android_vendor_data1)->vip_task;

	list_del_init(&vts->vip_list);
	vts->vip_prio = NOT_VIP;
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
		deactivate_vip_task(curr);
		return;
	}
	/* slice expired. re-queue the task */
	list_del(&vts->vip_list);
	insert_vip_task(rq, vts, false);
}

void vip_check_preempt_wakeup(void *unused, struct rq *rq, struct task_struct *p,
				bool *preempt, bool *nopreempt, int wake_flags,
				struct sched_entity *se, struct sched_entity *pse,
				int next_buddy_marked, unsigned int granularity)
{
	struct list_head *vip_tasks = per_cpu(vip_tasks_per_cpu, cpu_of(rq));
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
	resched = (vip_tasks->next != &vts_c->vip_list);
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
	struct list_head *vip_tasks = per_cpu(vip_tasks_per_cpu, cpu_of(rq));
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
	if ((vip_tasks->next != &vts->vip_list) && rq->cfs.h_nr_running > 1)
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

void vip_replace_next_task_fair(void *unused, struct rq *rq, struct task_struct **p,
				struct sched_entity **se, bool *repick, bool simple,
				struct task_struct *prev)
{
	struct list_head *vip_tasks = per_cpu(vip_tasks_per_cpu, cpu_of(rq));
	struct vip_task_struct *vts;
	struct task_struct *vip;

	if (unlikely(!vip_enable))
		return;

	/* We don't have VIP tasks queued */
	if (list_empty(vip_tasks))
		return;

	/* Return the first task from VIP queue */
	vts = list_first_entry(vip_tasks, struct vip_task_struct, vip_list);
	vip = vts_to_ts(vts);

	*p = vip;
	*se = &vip->se;
	*repick = true;
}

void vip_dequeue_task(void *unused, struct rq *rq, struct task_struct *p, int flags)
{
	struct vip_task_struct *vts = &((struct mtk_task *) p->android_vendor_data1)->vip_task;

	if (unlikely(!vip_enable))
		return;

	if (!list_empty(&vts->vip_list) && vts->vip_list.next)
		deactivate_vip_task(p);

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
void register_vip_hooks(void)
{
	int ret = 0;

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

void init_vip_task_struct(struct task_struct *p)
{
	struct vip_task_struct *vts = &((struct mtk_task *) p->android_vendor_data1)->vip_task;

	INIT_LIST_HEAD(&vts->vip_list);
	vts->sum_exec_snapshot = 0;
	vts->total_exec = 0;
	vts->vip_prio = NOT_VIP;
}

void vip_init(void)
{
	struct task_struct *g, *p;
	int cpu;

	allow_VIP_task_group = false;
	allow_VIP_ls = false;

	/* init vip related value to exist tasks */
	read_lock(&tasklist_lock);
	do_each_thread(g, p) {
		init_vip_task_struct(p);
	} while_each_thread(g, p);
	read_unlock(&tasklist_lock);

	/* init vip related value to idle thread */
	for_each_possible_cpu(cpu) {
		struct list_head *vip_tasks_data;

		vip_tasks_data = kcalloc(1, sizeof(struct list_head), GFP_KERNEL);
		per_cpu(vip_tasks_per_cpu, cpu) = vip_tasks_data;
		INIT_LIST_HEAD(per_cpu(vip_tasks_per_cpu, cpu));
	}

	/* init vip related value to newly forked tasks */
	register_vip_hooks();
}
