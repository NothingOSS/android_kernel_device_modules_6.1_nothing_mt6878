/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */
 #ifndef _FLT_API_H
#define _FLT_API_H

struct flt_pm {
	ktime_t ktime_last;
	bool ktime_suspended;
};

/* Note: ws setting related API */
int flt_get_ws(void);
int flt_set_ws(int ws);

/* Note: Group/CPU setting related API */
int flt_sched_set_group_policy_eas(int grp_id, int ws, int wp, int wc);
int flt_sched_get_group_policy_eas(int grp_id, int *ws, int *wp, int *wc);
int flt_sched_set_cpu_policy_eas(int cpu, int ws, int wp, int wc);
int flt_sched_get_cpu_policy_eas(int cpu, int *ws, int *wp, int *wc);

/* Note: group related API */
int flt_get_sum_group(int grp_id);
int flt_get_max_group(int grp_id);
int flt_get_gear_sum_pelt_group(unsigned int gear_id, int grp_id);
int flt_get_gear_max_pelt_group(unsigned int gear_id, int grp_id);
int flt_get_gear_sum_pelt_group_cnt(unsigned int gear_id, int grp_id);
int flt_get_gear_max_pelt_group_cnt(unsigned int gear_id, int grp_id);
int flt_sched_get_gear_max_group_eas(int gear_id, int grp_id);
int flt_sched_get_gear_sum_group_eas(int gear_id, int grp_id);

/* Note: cpu related API */
int flt_get_cpu_by_wp(int cpu);
unsigned long flt_get_cpu(int cpu);

/* Note: task related API */
int flt_get_task_by_wp(struct task_struct *p, int wc, int task_wp);

#if IS_ENABLED(CONFIG_MTK_CPUFREQ_SUGOV_EXT)
void register_sugov_hooks(void);
#endif

/* suspend/resume api */
void flt_resume_notify(void);
void flt_suspend_notify(void);
void flt_get_pm_status(struct flt_pm *fltpm);
#endif /* _FLT_API_H */
