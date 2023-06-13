/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2023 MediaTek Inc.
 */
#if IS_ENABLED(CONFIG_MTK_SCHED_GROUP_AWARE)
extern int flt_sched_get_cpu_group_eas(int cpu, int grp_id);
extern int flt_get_cpu_o(int cpu);
extern int flt_get_gp_hint(int grp_id);
extern void (*sugov_grp_awr_update_cpu_tar_util_hook)(int cpu);
void grp_awr_update_grp_awr_util(void);
int grp_awr_init(void);
#endif
