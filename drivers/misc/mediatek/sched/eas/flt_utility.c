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
#include <sugov/cpufreq.h>
#include "sched_sys_common.h"
#include "eas_plus.h"
#include "common.h"
#include "flt_init.h"
#include "flt_api.h"
#include "group.h"
#include "flt_utility.h"
#include "eas_trace.h"

void flt_update_data(unsigned int data, unsigned int offset)
{
	void __iomem *flt_adr;
	unsigned long long len;

	if (!IS_ALIGNED(offset, PER_ENTRY))
		return;
	len = get_flt_xrg_size();
	flt_adr = get_flt_xrg();
	if (flt_adr && offset <= len)
		iowrite32(data, flt_adr + offset);
}

unsigned int flt_get_data(unsigned int offset)
{
	void __iomem *flt_adr;
	unsigned int res = 0;
	unsigned long long len;

	if (!IS_ALIGNED(offset, PER_ENTRY))
		return res;
	len = get_flt_xrg_size();
	flt_adr = get_flt_xrg();
	if (flt_adr && offset <= len)
		res = ioread32(flt_adr + offset);

	return res;
}

static int flt_is_valid(void)
{
	int res = 0;

	res = flt_get_data(FLT_VALID);
	return res;
}

static int flt_get_window_size_mode2(void)
{
	int res = 0;

	res = flt_get_data(AP_WS_CTL);

	return res;
}

static int flt_set_window_size_mode2(int ws)
{
	int res = 0;

	flt_update_data(ws, AP_WS_CTL);

	return res;
}

static int flt_sched_set_group_policy_eas_mode2(int grp_id, int ws, int wp, int wc)
{
	int res = 0;
	unsigned int offset, update_data;

	if (grp_id >= GROUP_ID_RECORD_MAX || grp_id < 0)
		return -1;

	offset = grp_id * PER_ENTRY;
	update_data = (wp << WP_LEN) | wc;
	flt_update_data(ws, AP_WS_CTL);
	flt_update_data(update_data, AP_GP_SETTING_STA_ADDR + offset);

	return res;
}

static int flt_sched_get_group_policy_eas_mode2(int grp_id, int *ws, int *wp, int *wc)
{
	int res = 0;
	unsigned int offset, update_data;

	if (grp_id >= GROUP_ID_RECORD_MAX || grp_id < 0)
		return -1;
	offset = grp_id * PER_ENTRY;
	update_data = flt_get_data(AP_GP_SETTING_STA_ADDR + offset);
	*ws = flt_get_data(AP_WS_CTL);
	*wp = update_data >> WP_LEN;
	*wc = update_data & WC_MASK;

	return res;
}

static int flt_sched_set_cpu_policy_eas_mode2(int cpu, int ws, int wp, int wc)
{
	int res = 0;
	unsigned int offset, update_data;

	if (!cpumask_test_cpu(cpu, cpu_possible_mask))
		return -1;
	offset = cpu * PER_ENTRY;
	update_data = (wp << WP_LEN) | wc;
	flt_update_data(ws, AP_WS_CTL);
	flt_update_data(update_data, AP_CPU_SETTING_ADDR + offset);
	return res;
}

static int flt_sched_get_cpu_policy_eas_mode2(int cpu, int *ws, int *wp, int *wc)
{
	int res = 0;
	unsigned int offset, update_data;

	if (!cpumask_test_cpu(cpu, cpu_possible_mask))
		return -1;
	offset = cpu * PER_ENTRY;
	update_data = flt_get_data(AP_CPU_SETTING_ADDR + offset);
	*ws = flt_get_data(AP_WS_CTL);
	*wp = update_data >> WP_LEN;
	*wc = update_data & WC_MASK;

	return res;
}

static int flt_get_sum_group_mode2(int grp_id)
{
	int res = 0;
	unsigned int offset;

	if (grp_id >= GROUP_ID_RECORD_MAX || grp_id < 0 || flt_is_valid() != 1)
		return -1;
	offset = grp_id * PER_ENTRY;
	res = flt_get_data(GP_DATA_START + offset);

	return res;
}

static int flt_get_gear_sum_pelt_group_mode2(unsigned int gear_id, int group_id)
{
		struct cpumask *gear_cpus;
		struct rq_group *fsrq;
		int cpu = -1;
		unsigned long res = 0;
		unsigned int nr_gear;

		nr_gear = get_nr_gears();
		if (gear_id >= nr_gear ||
			group_id >= GROUP_ID_RECORD_MAX ||
			group_id < 0)
			return -1;

		gear_cpus = get_gear_cpumask(gear_id);
		for_each_cpu_and(cpu, gear_cpus, cpu_active_mask)  {
			fsrq = &per_cpu(rq_group, cpu);
			res += READ_ONCE(fsrq->pelt_group_util[group_id]);
		}
		return res;
}

int flt_sched_get_gear_sum_group_eas_mode2(int gear_id, int group_id)
{
	unsigned int nr_gear, gear_idx;
	int pelt_util = 0, res = 0;
	u64 flt_util = 0, gear_util = 0, total_util = 0;

	if (group_id >= GROUP_ID_RECORD_MAX || group_id < 0)
		return -1;

	nr_gear = get_nr_gears();

	if (gear_id >= nr_gear || gear_id < 0)
		return -1;

	flt_util = flt_get_sum_group(group_id);

	for (gear_idx = 0; gear_idx < nr_gear; gear_idx++) {
		pelt_util = flt_get_gear_sum_pelt_group(gear_idx, group_id);
		if (gear_idx == gear_id)
			gear_util	= pelt_util;
		total_util += pelt_util;
	}
	if (total_util)
		res = (int)div64_u64(flt_util * gear_util, total_util);
	return res;
}

static int flt_get_cpu_by_wp_mode2(int cpu)
{
	int res = 0;
	unsigned int offset;

	if (!cpumask_test_cpu(cpu, cpu_possible_mask) || flt_is_valid() != 1)
		return -1;

	offset = cpu * PER_ENTRY;
	res = flt_get_data(offset);
	return res;
}

static int flt_sched_get_cpu_group_eas_mode2(int cpu_idx, int group_id)
{
	int pelt_util = 0, res = 0, cpu = 0;
	struct rq_group *fsrq;
	u64 flt_util = 0, total_util = 0, cpu_util = 0;

	if (group_id >= GROUP_ID_RECORD_MAX ||
		group_id < 0 ||
		!cpumask_test_cpu(cpu_idx, cpu_possible_mask))
		return -1;

	flt_util = flt_get_sum_group_mode2(group_id);
	for_each_possible_cpu(cpu)  {
		fsrq = &per_cpu(rq_group, cpu);
		pelt_util = READ_ONCE(fsrq->pelt_group_util[group_id]);
		if (cpu_idx == cpu)
			cpu_util = pelt_util;
		total_util += pelt_util;
	}

	if (total_util)
		res = (int)div64_u64(flt_util * cpu_util, total_util);

	return res;
}

void flt_mode2_register_api_hooks(void)
{
	flt_get_ws_api = flt_get_window_size_mode2;
	flt_set_ws_api = flt_set_window_size_mode2;
	flt_sched_set_group_policy_eas_api = flt_sched_set_group_policy_eas_mode2;
	flt_sched_get_group_policy_eas_api = flt_sched_get_group_policy_eas_mode2;
	flt_sched_set_cpu_policy_eas_api = flt_sched_set_cpu_policy_eas_mode2;
	flt_sched_get_cpu_policy_eas_api = flt_sched_get_cpu_policy_eas_mode2;
	flt_get_sum_group_api = flt_get_sum_group_mode2;
	flt_get_gear_sum_pelt_group_api = flt_get_gear_sum_pelt_group_mode2;
	flt_sched_get_gear_sum_group_eas_api = flt_sched_get_gear_sum_group_eas_mode2;
	flt_get_cpu_by_wp_api = flt_get_cpu_by_wp_mode2;
	flt_sched_get_cpu_group_eas_api = flt_sched_get_cpu_group_eas_mode2;
}

void flt_mode2_init_res(void)
{
	int cpu, i;

	flt_set_ws(DEFAULT_WS);
	for_each_possible_cpu(cpu)
		flt_sched_set_cpu_policy_eas(cpu, DEFAULT_WS, CPU_DEFAULT_WP, CPU_DEFAULT_WC);
	for (i = 0; i < GROUP_ID_RECORD_MAX; i++)
		flt_sched_set_group_policy_eas(i, DEFAULT_WS, GRP_DEFAULT_WP, GRP_DEFAULT_WC);
	flt_update_data(FLT_MODE2_EN, AP_FLT_CTL);
}
