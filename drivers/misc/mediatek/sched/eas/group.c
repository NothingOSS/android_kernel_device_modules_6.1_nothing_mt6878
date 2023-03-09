// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */
#include <linux/init.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/io.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/cpumask.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/timekeeping.h>
#include <linux/cgroup.h>
#include <trace/hooks/sched.h>
#include <trace/hooks/cgroup.h>
#include <linux/sched/cputime.h>
#include <sched/sched.h>
#include "eas_plus.h"
#include "common.h"
#include "group.h"
#include "eas_trace.h"

#define GRP_DEFAULT_WS DEFAULT_SCHED_RAVG_WINDOW
#define GRP_DEFAULT_WC GROUP_RAVG_HIST_SIZE_MAX
#define GRP_DEFAULT_WP WP_MODE_4

static struct grp *related_thread_groups[GROUP_ID_RECORD_MAX];
static u32 GP_mode = GP_MODE_0;

static LIST_HEAD(active_related_thread_groups);
static DEFINE_RWLOCK(related_thread_group_lock);
DEFINE_PER_CPU(struct rq_group, rq_group);
EXPORT_PER_CPU_SYMBOL(rq_group);

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
	return max(task_util(p), _task_util_est(p));
}

inline struct grp *lookup_grp(int grp_id)
{
	if (grp_id >= GROUP_ID_RECORD_MAX || grp_id < 0)
		return NULL;
	else
		return related_thread_groups[grp_id];
}
EXPORT_SYMBOL(lookup_grp);

inline struct grp *task_grp(struct task_struct *p)
{
	struct gp_task_struct *gts = &((struct mtk_task *)p->android_vendor_data1)->gp_task;

	return rcu_dereference(gts->grp);
}

static int alloc_related_thread_groups(void)
{
	int i;
	struct grp *grp;

	for (i = 0; i < GROUP_ID_RECORD_MAX; i++) {
		grp = kzalloc(sizeof(*grp), GFP_ATOMIC | GFP_NOWAIT);
		if (!grp)
			return -1;

		grp->id = i;
		grp->ws = GRP_DEFAULT_WS;
		grp->wc = GRP_DEFAULT_WC;
		grp->wp = GRP_DEFAULT_WP;

		INIT_LIST_HEAD(&grp->tasks);
		INIT_LIST_HEAD(&grp->list);
		raw_spin_lock_init(&grp->lock);

		related_thread_groups[i] = grp;
	}

	return 0;
}

static void free_related_thread_groups(void)
{
	int grp_idx;
	struct grp *grp = NULL;

	for (grp_idx = 0; grp_idx < GROUP_ID_RECORD_MAX; ++grp_idx) {
		grp = lookup_grp(grp_idx);
			kfree(grp);
	}
}

static inline int cgrp_to_grpid(struct task_struct *p)
{
	struct cgroup_subsys_state *css;
	struct task_group *tg;
	int groupid = -1;
	struct cgrp_tg *cgrptg;

	rcu_read_lock();
	css = task_css(p, cpu_cgrp_id);
	if (!css)
		goto unlock;

	tg = container_of(css, struct task_group, css);
	cgrptg = &((struct mtk_tg *)tg->android_vendor_data1)->cgrp_tg;
	if (cgrptg->colocate)
		groupid = cgrptg->groupid;

unlock:
	rcu_read_unlock();
	return groupid;
}

static void remove_task_from_group(struct task_struct *p)
{
	struct gp_task_struct *gts = &((struct mtk_task *)p->android_vendor_data1)->gp_task;
	struct grp *grp = NULL;
	int empty_group = 1;
	struct rq *rq;
	struct rq_flags rf;

	rcu_read_lock();
	grp = task_grp(p);
	rcu_read_unlock();

	raw_spin_lock(&grp->lock);

	rq = __task_rq_lock(p, &rf);
	list_del_init(&gts->grp_list);
	rcu_assign_pointer(gts->grp, NULL);
	__task_rq_unlock(rq, &rf);

	if (!list_empty(&grp->tasks))
		empty_group = 0;

	raw_spin_unlock(&grp->lock);
}

static int
add_task_to_group(struct task_struct *p, struct grp *grp)
{
	struct rq *rq;
	struct rq_flags rf;
	struct gp_task_struct *gts = &((struct mtk_task *)p->android_vendor_data1)->gp_task;

	raw_spin_lock(&grp->lock);

	/*
	 * Change gts->grp under rq->lock. Will prevent races with read-side
	 * reference of gts->grp in various hot-paths
	 */
	rq = __task_rq_lock(p, &rf);
	list_add(&gts->grp_list, &grp->tasks);
	rcu_assign_pointer(gts->grp, grp);
	__task_rq_unlock(rq, &rf);

	raw_spin_unlock(&grp->lock);

	return 0;
}

int __sched_set_grp_id(struct task_struct *p, int group_id)
{
	int rc = 0;
	unsigned long flags;
	struct grp *grp = NULL;
	struct gp_task_struct *gts = &((struct mtk_task *)p->android_vendor_data1)->gp_task;

	if (group_id >= GROUP_ID_RECORD_MAX)
		return -1;

	raw_spin_lock_irqsave(&p->pi_lock, flags);
	write_lock(&related_thread_group_lock);

	/* Switching from one group to another directly is not permitted */
	if ((!gts->grp && group_id < 0) || (gts->grp && group_id >= 0))
		goto done;

	/* remove */
	if (group_id < 0) {
		remove_task_from_group(p);
		goto done;
	}

	grp = lookup_grp(group_id);
	if (list_empty(&grp->list))
		list_add(&grp->list, &active_related_thread_groups);
	rc = add_task_to_group(p, grp);

done:
	write_unlock(&related_thread_group_lock);
	raw_spin_unlock_irqrestore(&p->pi_lock, flags);

	return rc;
}

static void init_tg(struct task_group *tg)
{
	struct cgrp_tg *cgrptg;

	cgrptg = &((struct mtk_tg *)tg->android_vendor_data1)->cgrp_tg;

	cgrptg->colocate = false;
	cgrptg->groupid = -1;
}

static inline struct task_group *css_tg(struct cgroup_subsys_state *css)
{
	return css ? container_of(css, struct task_group, css) : NULL;
}

static void group_update_tg_pointer(struct cgroup_subsys_state *css)
{
	init_tg(css_tg(css));
}

static void group_init_tg_pointers(void)
{
	struct cgroup_subsys_state *css = &root_task_group.css;
	struct cgroup_subsys_state *top_css = css;

	rcu_read_lock();
	css_for_each_child(css, top_css)
		group_update_tg_pointer(css);
	rcu_read_unlock();
}

static void add_new_task_to_grp(struct task_struct *new)
{
	int groupid;

	groupid = cgrp_to_grpid(new);
	__sched_set_grp_id(new, groupid);
}

static void group_init_new_task_load(struct task_struct *p)
{
	struct gp_task_struct *gts = &((struct mtk_task *)p->android_vendor_data1)->gp_task;

	rcu_assign_pointer(gts->grp, NULL);
	INIT_LIST_HEAD(&gts->grp_list);
}

static void group_init_existing_task_load(struct task_struct *p)
{
	group_init_new_task_load(p);
}

static void group_android_rvh_wake_up_new_task(void *unused, struct task_struct *new)
{
	/* init new task grp & list */
	group_init_new_task_load(new);

	if (unlikely(group_get_mode() == GP_MODE_0))
		return;
	add_new_task_to_grp(new);
}

static void group_android_rvh_cpu_cgroup_online(void *unused, struct cgroup_subsys_state *css)
{
	if (unlikely(group_get_mode() == GP_MODE_0))
		return;

	group_update_tg_pointer(css);
}

static void group_android_rvh_cpu_cgroup_attach(void *unused,
						struct cgroup_taskset *tset)
{
	struct task_struct *task;
	struct cgroup_subsys_state *css;
	struct task_group *tg;
	struct cgrp_tg *cgrptg;
	int ret, grp_id;

	if (unlikely(group_get_mode() == GP_MODE_0))
		return;

	cgroup_taskset_first(tset, &css);
	if (!css)
		return;

	tg = container_of(css, struct task_group, css);
	cgrptg = &((struct mtk_tg *)tg->android_vendor_data1)->cgrp_tg;

	cgroup_taskset_for_each(task, css, tset) {
		grp_id = cgrptg->colocate ? cgrptg->groupid : -1;
		ret = __sched_set_grp_id(task, grp_id);
		if (trace_sched_cgroup_attach_enabled())
			trace_sched_cgroup_attach(task, grp_id, ret);
	}
}

int snapshot_pelt_group_status(void)
{
	struct task_struct *p;
	struct mtk_task *mts;
	struct gp_task_struct *gts;
	struct rq *rq;
	struct rq_group *gprq;
	struct grp *grp = NULL;
	int grp_idx, cpu;
	unsigned long pelt_utilest;
	unsigned long long t1, t2;

	/* reset per cpu pelt_group_status */
	for_each_possible_cpu(cpu) {
		rq = cpu_rq(cpu);
		gprq = &per_cpu(rq_group, cpu);
		for (grp_idx = 0; grp_idx < GROUP_ID_RECORD_MAX; ++grp_idx)
			WRITE_ONCE(gprq->pelt_group_util[grp_idx], 0);
	}

	for (grp_idx = 0; grp_idx < GROUP_ID_RECORD_MAX; ++grp_idx) {
		grp = lookup_grp(grp_idx);
		if (!grp || list_empty(&grp->tasks))
			continue;
		t1 = sched_clock();
		list_for_each_entry(gts, &grp->tasks, grp_list) {
			mts = container_of(gts, struct mtk_task, gp_task);
			p = mts_to_ts(mts);
			if (likely(!task_has_dl_policy(p) || !p->dl.dl_throttled)) {
				if (unlikely(task_on_rq_queued(p))) {
					rq = task_rq(p);
					gprq = &per_cpu(rq_group, rq->cpu);
					pelt_utilest = READ_ONCE(gprq->pelt_group_util[grp_idx]) +
								task_util_est(p);
					WRITE_ONCE(gprq->pelt_group_util[grp_idx], pelt_utilest);
				}
			}
		}
		t2 = sched_clock();
		if (trace_sched_get_pelt_group_util_enabled())
			trace_sched_get_pelt_group_util(grp_idx, t2 - t1);
	}

	return 0;
}
EXPORT_SYMBOL(snapshot_pelt_group_status);

int get_grp_id(struct task_struct *p)
{
	int grp_id;
	struct grp *grp;

	rcu_read_lock();
	grp = task_grp(p);
	grp_id = grp ? grp->id : -1;
	rcu_read_unlock();

	return grp_id;
}
EXPORT_SYMBOL(get_grp_id);

inline bool check_and_get_grp_id(struct task_struct *p, int *grp_id)
{
	bool ret = false;
	*grp_id = get_grp_id(p);
	if (*grp_id >= GROUP_ID_1 && *grp_id <= GROUP_ID_4)
		ret = true;

	return ret;
}
EXPORT_SYMBOL(check_and_get_grp_id);

int get_group_count(int grp_id)
{
	int task_cnt = 0;
	struct gp_task_struct *gts;
	struct grp *grp = NULL;

	if (grp_id >= GROUP_ID_RECORD_MAX || grp_id < 0)
		return -1;

	grp = lookup_grp(grp_id);
	raw_spin_lock(&grp->lock);
	if (list_empty(&grp->tasks)) {
		raw_spin_unlock(&grp->lock);
		return -1;
	}

	list_for_each_entry(gts, &grp->tasks, grp_list) {
		++task_cnt;
	}
	raw_spin_unlock(&grp->lock);

	return task_cnt;
}

int set_task_to_group(int pid, int grp_id)
{
	struct task_struct *p;
	int ret = -1;

	if (grp_id >= GROUP_ID_RECORD_MAX)
		return ret;
	p = get_pid_task(find_vpid(pid), PIDTYPE_PID);
	if (!p)
		return ret;
	ret = __sched_set_grp_id(p, grp_id);
	put_task_struct(p);
	return ret;
}
EXPORT_SYMBOL(set_task_to_group);

static void group_task_dead(struct task_struct *p)
{
	__sched_set_grp_id(p, -1);
}

static void group_android_rvh_flush_task(void *unused, struct task_struct *p)
{
	group_task_dead(p);
}

static void group_register_hooks(void)
{
	int ret = 0;

	ret = register_trace_android_rvh_wake_up_new_task(
		group_android_rvh_wake_up_new_task, NULL);
	if (ret)
		pr_info("register wake_up_new_task hooks failed, returned %d\n", ret);

	register_trace_android_rvh_flush_task(
		group_android_rvh_flush_task, NULL);
	if (ret)
		pr_info("register flush_task hooks failed, returned %d\n", ret);

	ret = register_trace_android_rvh_cpu_cgroup_attach(
		group_android_rvh_cpu_cgroup_attach, NULL);
	if (ret)
		pr_info("register cpu_cgroup_attach hooks failed, returned %d\n", ret);

	ret = register_trace_android_rvh_cpu_cgroup_online(
		group_android_rvh_cpu_cgroup_online, NULL);
	if (ret)
		pr_info("register cpu_cgroup_online hooks failed, returned %d\n", ret);
}

void group_init(void)
{
	struct task_struct *g, *p;
	int cpu;

	if (alloc_related_thread_groups() != 0)
		return;

	/* for existing thread */
	read_lock(&tasklist_lock);
	do_each_thread(g, p) {
		group_init_existing_task_load(p);
	} while_each_thread(g, p);
	read_unlock(&tasklist_lock);

	/* init for each cpu */
	for_each_possible_cpu(cpu) {
		struct rq *rq = cpu_rq(cpu);

		/* Create task members for idle thread */
		group_init_new_task_load(rq->idle);
	}

	group_init_tg_pointers();
	group_register_hooks();
}

void group_exit(void)
{
	free_related_thread_groups();
}

void  group_set_mode(u32 mode)
{
	GP_mode = mode;
}
EXPORT_SYMBOL(group_set_mode);

u32 group_get_mode(void)
{
	return GP_mode;
}
EXPORT_SYMBOL(group_get_mode);

