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
#include <linux/energy_model.h>
#include <linux/cgroup.h>
#include <trace/hooks/sched.h>
#include <trace/hooks/cgroup.h>
#include <linux/sched/cputime.h>
#include <sched/sched.h>
#include "sched_sys_common.h"
#include "eas_plus.h"
#include "common.h"
#include "flt_init.h"

/* API Function pointer*/
int (*flt_get_ws_api)(void);
EXPORT_SYMBOL(flt_get_ws_api);
int (*flt_set_ws_api)(int ws);
EXPORT_SYMBOL(flt_set_ws_api);
int (*flt_sched_set_group_policy_eas_api)(int grp_id, int ws, int wp, int wc);
EXPORT_SYMBOL(flt_sched_set_group_policy_eas_api);
int (*flt_sched_get_group_policy_eas_api)(int grp_id, int *ws, int *wp, int *wc);
EXPORT_SYMBOL(flt_sched_get_group_policy_eas_api);
int (*flt_sched_set_cpu_policy_eas_api)(int cpu, int ws, int wp, int wc);
EXPORT_SYMBOL(flt_sched_set_cpu_policy_eas_api);
int (*flt_sched_get_cpu_policy_eas_api)(int cpu, int *ws, int *wp, int *wc);
EXPORT_SYMBOL(flt_sched_get_cpu_policy_eas_api);
int (*flt_get_sum_group_api)(int grp_id);
EXPORT_SYMBOL(flt_get_sum_group_api);
int (*flt_get_max_group_api)(int grp_id);
EXPORT_SYMBOL(flt_get_max_group_api);
int (*flt_get_gear_sum_pelt_group_api)(unsigned int gear_id, int grp_id);
EXPORT_SYMBOL(flt_get_gear_sum_pelt_group_api);
int (*flt_get_gear_max_pelt_group_api)(unsigned int gear_id, int grp_id);
EXPORT_SYMBOL(flt_get_gear_max_pelt_group_api);
int (*flt_get_gear_sum_pelt_group_cnt_api)(unsigned int gear_id, int grp_id);
EXPORT_SYMBOL(flt_get_gear_sum_pelt_group_cnt_api);
int (*flt_get_gear_max_pelt_group_cnt_api)(unsigned int gear_id, int grp_id);
EXPORT_SYMBOL(flt_get_gear_max_pelt_group_cnt_api);
int (*flt_sched_get_gear_sum_group_eas_api)(int gear_id, int grp_id);
EXPORT_SYMBOL(flt_sched_get_gear_sum_group_eas_api);
int (*flt_sched_get_gear_max_group_eas_api)(int gear_id, int grp_id);
EXPORT_SYMBOL(flt_sched_get_gear_max_group_eas_api);
int (*flt_get_cpu_by_wp_api)(int cpu);
EXPORT_SYMBOL(flt_get_cpu_by_wp_api);
int (*flt_get_task_by_wp_api)(struct task_struct *p, int wc, int task_wp);
EXPORT_SYMBOL(flt_get_task_by_wp_api);

int flt_get_ws(void)
{
	if (unlikely(flt_get_mode() == FLT_MODE_0))
		return -1;
	else if (flt_get_ws_api)
		return flt_get_ws_api();
	else
		return -1;
}

int flt_set_ws(int ws)
{
	if (unlikely(flt_get_mode() == FLT_MODE_0))
		return -1;
	else if (flt_set_ws_api)
		return flt_set_ws_api(ws);
	else
		return -1;
}

int flt_sched_set_group_policy_eas(int grp_id, int ws,
					int wp, int wc)
{
	if (unlikely(flt_get_mode() == FLT_MODE_0))
		return -1;
	else if (flt_sched_set_group_policy_eas_api)
		return flt_sched_set_group_policy_eas_api(grp_id, ws, wp, wc);
	else
		return -1;
}

int flt_sched_get_group_policy_eas(int grp_id, int *ws,
					int *wp, int *wc)
{
	if (unlikely(flt_get_mode() == FLT_MODE_0))
		return -1;
	else if (flt_sched_get_group_policy_eas_api)
		return flt_sched_get_group_policy_eas_api(grp_id, ws, wp, wc);
	else
		return -1;
}

int flt_sched_set_cpu_policy_eas(int cpu, int ws,
					int wp, int wc)
{
	if (unlikely(flt_get_mode() == FLT_MODE_0))
		return -1;
	else if (flt_sched_set_cpu_policy_eas_api)
		return flt_sched_set_cpu_policy_eas_api(cpu, ws, wp, wc);
	else
		return -1;
}

int flt_sched_get_cpu_policy_eas(int cpu, int *ws,
					int *wp, int *wc)
{
	if (unlikely(flt_get_mode() == FLT_MODE_0))
		return -1;
	else if (flt_sched_get_cpu_policy_eas_api)
		return flt_sched_get_cpu_policy_eas_api(cpu, ws, wp, wc);
	else
		return -1;
}

int flt_get_sum_group(int grp_id)
{
	if (unlikely(flt_get_mode() == FLT_MODE_0))
		return -1;
	else if (flt_get_sum_group_api)
		return flt_get_sum_group_api(grp_id);
	else
		return -1;
}
EXPORT_SYMBOL_GPL(flt_get_sum_group);

int flt_get_max_group(int grp_id)
{
	if (unlikely(flt_get_mode() == FLT_MODE_0))
		return -1;
	else if (flt_get_max_group_api)
		return flt_get_max_group_api(grp_id);
	else
		return -1;
}
EXPORT_SYMBOL_GPL(flt_get_max_group);

int flt_get_gear_sum_pelt_group(unsigned int gear_id, int grp_id)
{
	if (unlikely(flt_get_mode() == FLT_MODE_0))
		return -1;
	else if (flt_get_gear_sum_pelt_group_api)
		return flt_get_gear_sum_pelt_group_api(gear_id, grp_id);
	else
		return -1;
}
EXPORT_SYMBOL_GPL(flt_get_gear_sum_pelt_group);

int flt_get_gear_max_pelt_group(unsigned int gear_id, int grp_id)
{
	if (unlikely(flt_get_mode() == FLT_MODE_0))
		return -1;
	else if (flt_get_gear_max_pelt_group_api)
		return flt_get_gear_max_pelt_group_api(gear_id, grp_id);
	else
		return -1;
}
EXPORT_SYMBOL_GPL(flt_get_gear_max_pelt_group);

int flt_get_gear_sum_pelt_group_cnt(unsigned int gear_id, int grp_id)
{
	if (unlikely(flt_get_mode() == FLT_MODE_0))
		return -1;
	else if (flt_get_gear_sum_pelt_group_cnt_api)
		return flt_get_gear_sum_pelt_group_cnt_api(gear_id, grp_id);
	else
		return -1;
}
EXPORT_SYMBOL_GPL(flt_get_gear_sum_pelt_group_cnt);

int flt_get_gear_max_pelt_group_cnt(unsigned int gear_id, int grp_id)
{
	if (unlikely(flt_get_mode() == FLT_MODE_0))
		return -1;
	else if (flt_get_gear_max_pelt_group_cnt_api)
		return flt_get_gear_max_pelt_group_cnt_api(gear_id, grp_id);
	else
		return -1;
}
EXPORT_SYMBOL_GPL(flt_get_gear_max_pelt_group_cnt);

int flt_sched_get_gear_sum_group_eas(int gear_id, int grp_id)
{
	if (unlikely(flt_get_mode() == FLT_MODE_0))
		return -1;
	else if (flt_sched_get_gear_sum_group_eas_api)
		return flt_sched_get_gear_sum_group_eas_api(gear_id, grp_id);
	else
		return -1;
}
EXPORT_SYMBOL_GPL(flt_sched_get_gear_sum_group_eas);

int flt_sched_get_gear_max_group_eas(int gear_id, int grp_id)
{
	if (unlikely(flt_get_mode() == FLT_MODE_0))
		return -1;
	else if (flt_sched_get_gear_max_group_eas_api)
		return flt_sched_get_gear_max_group_eas_api(gear_id, grp_id);
	else
		return -1;
}
EXPORT_SYMBOL_GPL(flt_sched_get_gear_max_group_eas);

int flt_get_cpu_by_wp(int cpu)
{
	if (unlikely(flt_get_mode() == FLT_MODE_0))
		return -1;
	else if (flt_get_cpu_by_wp_api)
		return flt_get_cpu_by_wp_api(cpu);
	else
		return -1;
}
EXPORT_SYMBOL(flt_get_cpu_by_wp);

int flt_get_task_by_wp(struct task_struct *p, int wc, int task_wp)
{
	if (unlikely(flt_get_mode() == FLT_MODE_0))
		return -1;
	else if (flt_get_task_by_wp_api)
		return flt_get_task_by_wp_api(p, wc, task_wp);
	else
		return -1;
}
EXPORT_SYMBOL_GPL(flt_get_task_by_wp);

unsigned long flt_get_cpu(int cpu)
{
	int cpu_dmand = 0;

	cpu_dmand = flt_get_cpu_by_wp(cpu);
	/* sanity check */
	if (unlikely(cpu_dmand < 0))
		return 0;
	else
		return cpu_dmand;
}
EXPORT_SYMBOL(flt_get_cpu);
