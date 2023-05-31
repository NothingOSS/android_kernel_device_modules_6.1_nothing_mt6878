// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2023 MediaTek Inc.
 */
#if IS_ENABLED(CONFIG_MTK_SCHED_GROUP_AWARE)
static int **pcpu_pgrp_u;
static int **pger_pgrp_u;
static int **pcpu_pgrp_adpt_rto;
static int **pcpu_pgrp_tar_u;
static int **pcpu_pgrp_marg;
static int *pgrp_hint;
static int *pcpu_o_u;
static int *pgrp_tar_act_rto;
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

	for (cpu_idx = 0; cpu_idx < FLT_NR_CPUS; cpu_idx++) {
		if (map_cpu_ger[cpu_idx] == tmp)
			continue;
		tmp = map_cpu_ger[cpu_idx];
		for (grp_idx = 0; grp_idx < GROUP_ID_RECORD_MAX; grp_idx++)
			pger_pgrp_u[map_cpu_ger[cpu_idx]][grp_idx] = 0;
	}

	for (cpu_idx = 0; cpu_idx < FLT_NR_CPUS; cpu_idx++) {
		for (grp_idx = 0; grp_idx < GROUP_ID_RECORD_MAX; grp_idx++) {
			pcpu_pgrp_u[cpu_idx][grp_idx] = flt_sched_get_cpu_group_eas(cpu_idx, grp_idx);

			pger_pgrp_u[map_cpu_ger[cpu_idx]][grp_idx] +=
				pcpu_pgrp_u[cpu_idx][grp_idx];

			pcpu_pgrp_adpt_rto[cpu_idx][grp_idx] =
				get_adaptive_ratio(cpu_idx, pgrp_tar_act_rto[grp_idx]);
		}
	}

	for (grp_idx = 0; grp_idx < GROUP_ID_RECORD_MAX; grp_idx++)
		pgrp_hint[grp_idx] = flt_get_gp_hint(grp_idx);
	for (cpu_idx = 0; cpu_idx < FLT_NR_CPUS; cpu_idx++)
		pcpu_o_u[cpu_idx] = flt_get_cpu_o(cpu_idx);

	if (grp_awr_update_group_util_hook)
		grp_awr_update_group_util_hook(FLT_NR_CPUS, GROUP_ID_RECORD_MAX,
			pcpu_pgrp_u, pger_pgrp_u, pgrp_hint,
			pcpu_pgrp_marg, pcpu_pgrp_adpt_rto, pcpu_pgrp_tar_u, map_cpu_ger);
}

void grp_awr_update_cpu_tar_util(int cpu)
{
	struct flt_rq *fsrq;

	fsrq = &per_cpu(flt_rq, cpu);

	if (grp_awr_update_cpu_tar_util_hook)
		grp_awr_update_cpu_tar_util_hook(cpu, GROUP_ID_RECORD_MAX, &fsrq->cpu_tar_util,
		fsrq->group_nr_running, pcpu_pgrp_tar_u, pcpu_o_u);
}

void set_group_target_active_ratio(int grp_idx, int val)
{
	pgrp_tar_act_rto[grp_idx] = clamp_val(val, 1, 100);
}
EXPORT_SYMBOL(set_group_target_active_ratio);

int grp_awr_init(void)
{
	int cpu_idx, grp_idx;

	/* per cpu per group data*/
	pcpu_pgrp_u = kcalloc(FLT_NR_CPUS, sizeof(int *), GFP_KERNEL);
	pger_pgrp_u = kcalloc(FLT_NR_CPUS, sizeof(int *), GFP_KERNEL);
	pcpu_pgrp_adpt_rto = kcalloc(FLT_NR_CPUS, sizeof(int *), GFP_KERNEL);
	pcpu_pgrp_tar_u = kcalloc(FLT_NR_CPUS, sizeof(int *), GFP_KERNEL);
	pcpu_pgrp_marg = kcalloc(FLT_NR_CPUS, sizeof(int *), GFP_KERNEL);

	/* per cpu data*/
	pcpu_o_u = kcalloc(FLT_NR_CPUS, sizeof(int), GFP_KERNEL);
	map_cpu_ger = kcalloc(FLT_NR_CPUS, sizeof(int), GFP_KERNEL);

	/* per group data*/
	pgrp_hint = kcalloc(GROUP_ID_RECORD_MAX, sizeof(int), GFP_KERNEL);
	pgrp_tar_act_rto = kcalloc(GROUP_ID_RECORD_MAX, sizeof(int), GFP_KERNEL);

	/* per cpu per group data*/
	for (cpu_idx = 0; cpu_idx < FLT_NR_CPUS; cpu_idx++) {
		pcpu_pgrp_u[cpu_idx] = kcalloc(GROUP_ID_RECORD_MAX, sizeof(int), GFP_KERNEL);
		pger_pgrp_u[cpu_idx] = kcalloc(GROUP_ID_RECORD_MAX, sizeof(int), GFP_KERNEL);
		pcpu_pgrp_adpt_rto[cpu_idx] = kcalloc(GROUP_ID_RECORD_MAX, sizeof(int), GFP_KERNEL);
		pcpu_pgrp_tar_u[cpu_idx] = kcalloc(GROUP_ID_RECORD_MAX, sizeof(int), GFP_KERNEL);
		pcpu_pgrp_marg[cpu_idx] = kcalloc(GROUP_ID_RECORD_MAX, sizeof(int), GFP_KERNEL);
		map_cpu_ger[cpu_idx] = per_cpu(gear_id, cpu_idx);
	}

	for (grp_idx = 0; grp_idx < GROUP_ID_RECORD_MAX; grp_idx++)
		pgrp_tar_act_rto[grp_idx] = 85;

	return 0;
}
#endif
