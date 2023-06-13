// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2023 MediaTek Inc.
 */
#if IS_ENABLED(CONFIG_MTK_SCHED_GROUP_AWARE)
static int grp_awr_init_finished;
static int **pcpu_pgrp_u;
static int **pger_pgrp_u;
static int **pcpu_pgrp_adpt_rto;
static int **pcpu_pgrp_tar_u;
static int **pcpu_pgrp_marg;
static int **pcpu_pgrp_act_rto_cap;
static int *pgrp_hint;
static int *pcpu_o_u;
static int *pgrp_tar_act_rto_cap;
static int *map_cpu_ger;

void (*grp_awr_update_group_util_hook)(int nr_cpus,
	int nr_grps, int **pcpu_pgrp_u, int **pger_pgrp_u,
	int *pgrp_hint, int **pcpu_pgrp_marg, int **pcpu_pgrp_adpt_rto,
	int **pcpu_pgrp_tar_u, int *map_cpu_ger);
EXPORT_SYMBOL(grp_awr_update_group_util_hook);
void (*grp_awr_update_cpu_tar_util_hook)(int cpu, int nr_grps, int *pcpu_tar_u,
	int *group_nr_running, int **pcpu_pgrp_tar_u, int *pcpu_o_u);
EXPORT_SYMBOL(grp_awr_update_cpu_tar_util_hook);

void grp_awr_update_grp_awr_util(void)
{
	int cpu_idx, grp_idx, tmp = -1;

	if (grp_awr_init_finished == false)
		return;
	for (cpu_idx = 0; cpu_idx < FLT_NR_CPUS; cpu_idx++) {
		pcpu_pgrp_act_rto_cap[cpu_idx][0] =
			get_gear_max_active_ratio_cap(map_cpu_ger[cpu_idx]);
		if (map_cpu_ger[cpu_idx] == tmp)
			continue;
		tmp = map_cpu_ger[cpu_idx];
		for (grp_idx = 0; grp_idx < GROUP_ID_RECORD_MAX; grp_idx++)
			pger_pgrp_u[map_cpu_ger[cpu_idx]][grp_idx] = 0;
	}

	for (cpu_idx = 0; cpu_idx < FLT_NR_CPUS; cpu_idx++) {
		for (grp_idx = 0; grp_idx < GROUP_ID_RECORD_MAX; grp_idx++) {
			pcpu_pgrp_u[cpu_idx][grp_idx] =
				flt_sched_get_cpu_group_eas(cpu_idx, grp_idx);

			pger_pgrp_u[map_cpu_ger[cpu_idx]][grp_idx] +=
				pcpu_pgrp_u[cpu_idx][grp_idx];

			pcpu_pgrp_adpt_rto[cpu_idx][grp_idx] =
				((pcpu_pgrp_act_rto_cap[cpu_idx][grp_idx] << SCHED_CAPACITY_SHIFT)
				/ pgrp_tar_act_rto_cap[grp_idx]);
		}
	}

	if (trace_sugov_ext_pger_pgrp_u_enabled()) {
		for (cpu_idx = 0; cpu_idx < FLT_NR_CPUS; cpu_idx++) {
			if (map_cpu_ger[cpu_idx] == tmp)
				continue;
			tmp = map_cpu_ger[cpu_idx];
			trace_sugov_ext_pger_pgrp_u(map_cpu_ger[cpu_idx],
				cpu_idx, pger_pgrp_u[map_cpu_ger[cpu_idx]]);
		}
	}

	for (grp_idx = 0; grp_idx < GROUP_ID_RECORD_MAX; grp_idx++)
		pgrp_hint[grp_idx] = flt_get_gp_hint(grp_idx);

	if (trace_sugov_ext_pgrp_hint_enabled())
		trace_sugov_ext_pgrp_hint(pgrp_hint);

	for (cpu_idx = 0; cpu_idx < FLT_NR_CPUS; cpu_idx++)
		pcpu_o_u[cpu_idx] = flt_get_cpu_o(cpu_idx);

	if (grp_awr_update_group_util_hook)
		grp_awr_update_group_util_hook(FLT_NR_CPUS, GROUP_ID_RECORD_MAX,
			pcpu_pgrp_u, pger_pgrp_u, pgrp_hint,
			pcpu_pgrp_marg, pcpu_pgrp_adpt_rto, pcpu_pgrp_tar_u, map_cpu_ger);

	if (trace_sugov_ext_pcpu_pgrp_u_rto_marg_enabled()) {
		for (cpu_idx = 0; cpu_idx < FLT_NR_CPUS; cpu_idx++)
			trace_sugov_ext_pcpu_pgrp_u_rto_marg(cpu_idx, pcpu_pgrp_u[cpu_idx],
				pcpu_pgrp_adpt_rto[cpu_idx],
				pcpu_pgrp_marg[cpu_idx], pcpu_o_u[cpu_idx]);
	}
}

void grp_awr_update_cpu_tar_util(int cpu)
{
	struct flt_rq *fsrq;

	if (grp_awr_init_finished == false)
		return;
	fsrq = &per_cpu(flt_rq, cpu);

	if (grp_awr_update_cpu_tar_util_hook)
		grp_awr_update_cpu_tar_util_hook(cpu, GROUP_ID_RECORD_MAX, &fsrq->cpu_tar_util,
			fsrq->group_nr_running, pcpu_pgrp_tar_u, pcpu_o_u);

	if (trace_sugov_ext_tar_cal_enabled())
		trace_sugov_ext_tar_cal(cpu, fsrq->cpu_tar_util, pcpu_pgrp_tar_u[cpu],
			fsrq->group_nr_running, pcpu_o_u[cpu]);
}

void set_group_target_active_ratio_pct(int grp_idx, int val)
{
	if (grp_awr_init_finished == false)
		return;
	pgrp_tar_act_rto_cap[grp_idx] = ((clamp_val(val, 1, 100) << SCHED_CAPACITY_SHIFT) / 100);
}
EXPORT_SYMBOL(set_group_target_active_ratio_pct);

void set_group_target_active_ratio_cap(int grp_idx, int val)
{
	if (grp_awr_init_finished == false)
		return;
	pgrp_tar_act_rto_cap[grp_idx] = clamp_val(val, 1, SCHED_CAPACITY_SCALE);
}
EXPORT_SYMBOL(set_group_target_active_ratio_cap);

void set_cpu_group_active_ratio_pct(int cpu, int grp_idx, int val)
{
	if (grp_awr_init_finished == false)
		return;
	pcpu_pgrp_act_rto_cap[cpu][grp_idx] =
		((clamp_val(val, 1, 100) << SCHED_CAPACITY_SHIFT) / 100);
}
EXPORT_SYMBOL(set_cpu_group_active_ratio_pct);

void set_cpu_group_active_ratio_cap(int cpu, int grp_idx, int val)
{
	if (grp_awr_init_finished == false)
		return;
	pcpu_pgrp_act_rto_cap[cpu][grp_idx] = clamp_val(val, 1, SCHED_CAPACITY_SCALE);
}
EXPORT_SYMBOL(set_cpu_group_active_ratio_cap);

void set_group_active_ratio_pct(int grp_idx, int val)
{
	int cpu_idx;

	if (grp_awr_init_finished == false)
		return;
	for (cpu_idx = 0; cpu_idx < FLT_NR_CPUS; cpu_idx++)
		pcpu_pgrp_act_rto_cap[cpu_idx][grp_idx] =
			((clamp_val(val, 1, 100) << SCHED_CAPACITY_SHIFT) / 100);
}
EXPORT_SYMBOL(set_group_active_ratio_pct);

void set_group_active_ratio_cap(int grp_idx, int val)
{
	int cpu_idx;

	if (grp_awr_init_finished == false)
		return;
	for (cpu_idx = 0; cpu_idx < FLT_NR_CPUS; cpu_idx++)
		pcpu_pgrp_act_rto_cap[cpu_idx][grp_idx] = clamp_val(val, 1, SCHED_CAPACITY_SCALE);
}
EXPORT_SYMBOL(set_group_active_ratio_cap);

int grp_awr_init(void)
{
	int cpu_idx, grp_idx;

	pr_info("group aware init\n");
	/* per cpu per group data*/
	pcpu_pgrp_u = kcalloc(FLT_NR_CPUS, sizeof(int *), GFP_KERNEL);
	pger_pgrp_u = kcalloc(FLT_NR_CPUS, sizeof(int *), GFP_KERNEL);
	pcpu_pgrp_adpt_rto = kcalloc(FLT_NR_CPUS, sizeof(int *), GFP_KERNEL);
	pcpu_pgrp_tar_u = kcalloc(FLT_NR_CPUS, sizeof(int *), GFP_KERNEL);
	pcpu_pgrp_marg = kcalloc(FLT_NR_CPUS, sizeof(int *), GFP_KERNEL);
	pcpu_pgrp_act_rto_cap = kcalloc(FLT_NR_CPUS, sizeof(int *), GFP_KERNEL);

	/* per cpu data*/
	pcpu_o_u = kcalloc(FLT_NR_CPUS, sizeof(int), GFP_KERNEL);
	map_cpu_ger = kcalloc(FLT_NR_CPUS, sizeof(int), GFP_KERNEL);

	/* per group data*/
	pgrp_hint = kcalloc(GROUP_ID_RECORD_MAX, sizeof(int), GFP_KERNEL);
	pgrp_tar_act_rto_cap = kcalloc(GROUP_ID_RECORD_MAX, sizeof(int), GFP_KERNEL);

	/* per cpu per group data*/
	for (cpu_idx = 0; cpu_idx < FLT_NR_CPUS; cpu_idx++) {
		pcpu_pgrp_u[cpu_idx] = kcalloc(GROUP_ID_RECORD_MAX, sizeof(int), GFP_KERNEL);
		pger_pgrp_u[cpu_idx] = kcalloc(GROUP_ID_RECORD_MAX, sizeof(int), GFP_KERNEL);
		pcpu_pgrp_adpt_rto[cpu_idx] =
			kcalloc(GROUP_ID_RECORD_MAX, sizeof(int), GFP_KERNEL);
		pcpu_pgrp_tar_u[cpu_idx] = kcalloc(GROUP_ID_RECORD_MAX, sizeof(int), GFP_KERNEL);
		pcpu_pgrp_marg[cpu_idx] = kcalloc(GROUP_ID_RECORD_MAX, sizeof(int), GFP_KERNEL);
		pcpu_pgrp_act_rto_cap[cpu_idx] =
			kcalloc(GROUP_ID_RECORD_MAX, sizeof(int), GFP_KERNEL);
		map_cpu_ger[cpu_idx] = per_cpu(gear_id, cpu_idx);
	}

	for (grp_idx = 0; grp_idx < GROUP_ID_RECORD_MAX; grp_idx++)
		pgrp_tar_act_rto_cap[grp_idx] = ((85 << SCHED_CAPACITY_SHIFT) / 100);

	sugov_grp_awr_update_cpu_tar_util_hook = grp_awr_update_cpu_tar_util;

	grp_awr_init_finished = true;
	return 0;
}
#endif
