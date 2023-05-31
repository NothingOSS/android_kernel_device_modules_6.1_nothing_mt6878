/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */
#ifndef _SCHED_COMMON_H
#define _SCHED_COMMON_H

#define MTK_VENDOR_DATA_SIZE_TEST(mstruct, kstruct)		\
	BUILD_BUG_ON(sizeof(mstruct) > (sizeof(u64) *		\
		ARRAY_SIZE(((kstruct *)0)->android_vendor_data1)))

#define MTK_TASK_GROUP_FLAG 1
#define MTK_TASK_FLAG 9

struct task_gear_hints {
	int gear_start;
	int num_gear;
};

struct vip_task_struct {
	struct list_head		vip_list;
	u64				sum_exec_snapshot;
	u64				total_exec;
	int				vip_prio;
	bool			basic_vip;
};

struct soft_affinity_task {
	bool latency_sensitive;
	struct cpumask soft_cpumask;
};

struct gp_task_struct {
	struct grp __rcu	*grp;
};

struct sbb_task_struct {
	int set_task;
	int set_group;
};

struct curr_uclamp_hint {
	int hint;
};

struct rot_task_struct {
	u64 ktime_ns;
};

struct cc_task_struct {
	u64 over_type;
};

struct task_turbo_t {
	unsigned char turbo:1;
	unsigned char render:1;
	unsigned short inherit_cnt:14;
	short nice_backup;
	atomic_t inherit_types;
};

struct mtk_task {
	u64 reserved0[MTK_TASK_FLAG];
	struct vip_task_struct	vip_task;
	struct soft_affinity_task sa_task;
	struct gp_task_struct	gp_task;
	struct task_gear_hints  gear_hints;
	struct sbb_task_struct sbb_task;
	struct curr_uclamp_hint cu_hint;
	struct rot_task_struct rot_task;
	struct cc_task_struct cc_task;
	struct task_turbo_t turbo_data;
};

struct soft_affinity_tg {
	struct cpumask soft_cpumask;
};

struct cgrp_tg {
	bool colocate;
	int groupid;
};

struct vip_task_group {
	unsigned int threshold;
};

struct mtk_tg {
	u64 reserved[MTK_TASK_GROUP_FLAG];
	struct soft_affinity_tg	sa_tg;
	struct cgrp_tg		cgrp_tg;
	struct vip_task_group vtg;
};

extern int num_sched_clusters;
extern cpumask_t __read_mostly **cpu_array;
extern void init_cpu_array(void);
extern void build_cpu_array(void);
extern void free_cpu_array(void);
extern void mtk_get_gear_indicies(struct task_struct *p, int *order_index, int *end_index);

struct util_rq {
	unsigned long util_cfs;
	unsigned long dl_util;
	unsigned long irq_util;
	unsigned long rt_util;
	unsigned long bw_dl_util;
	bool base;
};

#if IS_ENABLED(CONFIG_NONLINEAR_FREQ_CTL)
extern void mtk_map_util_freq(void *data, unsigned long util, unsigned long freq,
			struct cpumask *cpumask, unsigned long *next_freq, int wl_type);
#else
#define mtk_map_util_freq(data, util, freq, cap, next_freq, wl_type)
#endif /* CONFIG_NONLINEAR_FREQ_CTL */

#if IS_ENABLED(CONFIG_MTK_CPUFREQ_SUGOV_EXT)
DECLARE_PER_CPU(int, cpufreq_idle_cpu);
DECLARE_PER_CPU(spinlock_t, cpufreq_idle_cpu_lock);
unsigned long mtk_cpu_util(int cpu, unsigned long util_rq,
				enum cpu_util_type type,
				struct task_struct *p,
				unsigned long min_cap, unsigned long max_cap);
int dequeue_idle_cpu(int cpu);
#endif
__always_inline
unsigned long mtk_uclamp_rq_util_with(struct rq *rq, unsigned long util,
				  struct task_struct *p,
				  unsigned long min_cap, unsigned long max_cap);

#if IS_ENABLED(CONFIG_RT_GROUP_SCHED)
static inline int rt_rq_throttled(struct rt_rq *rt_rq)
{
	return rt_rq->rt_throttled && !rt_rq->rt_nr_boosted;
}
#else /* !CONFIG_RT_GROUP_SCHED */
static inline int rt_rq_throttled(struct rt_rq *rt_rq)
{
	return rt_rq->rt_throttled;
}
#endif

extern int set_target_margin(int gearid, int margin);
extern int set_turn_point_freq(int gearid, unsigned long freq);

#if IS_ENABLED(CONFIG_MTK_SCHEDULER)
extern bool sysctl_util_est;
#endif

static inline bool is_util_est_enable(void)
{
#if IS_ENABLED(CONFIG_MTK_SCHEDULER)
	return sysctl_util_est;
#else
	return true;
#endif
}

static inline unsigned long mtk_cpu_util_cfs(struct rq *rq)
{
	unsigned long util = READ_ONCE(rq->cfs.avg.util_avg);

	if (sched_feat(UTIL_EST) && is_util_est_enable()) {
		util = max_t(unsigned long, util,
			     READ_ONCE(rq->cfs.avg.util_est.enqueued));
	}

	return util;
}

#endif /* _SCHED_COMMON_H */
