/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2023 MediaTek Inc.
 */

#ifndef _VIP_H
#define _VIP_H

extern bool vip_enable;

#define VIP_TIME_SLICE     3000000U
#define VIP_TIME_LIMIT     (4 * VIP_TIME_SLICE)

#define WORKER_VIP         0
#define BINDER_VIP         1
#define TASK_BOOST_VIP     2
#define NOT_VIP           -1

#define DEFAULT_VIP_PRIO_THRESHOLD  99

#define mts_to_ts(mts) ({ \
		void *__mptr = (void *)(mts); \
		((struct task_struct *)(__mptr - \
			offsetof(struct task_struct, android_vendor_data1))); })

struct vip_rq {
	struct list_head vip_tasks;
	int num_vip_tasks;
};

enum vip_group {
	VIP_GROUP_TOPAPP,
	VIP_GROUP_FOREGROUND,
	VIP_GROUP_BACKGROUND,
	VIP_GROUP_NUM
};

struct VIP_task_group {
	int enable[VIP_GROUP_NUM];
	int threshold[VIP_GROUP_NUM];
};

extern inline int get_vip_task_prio(struct task_struct *p);
extern bool task_is_vip(struct task_struct *p);
extern inline unsigned int num_vip_in_cpu(int cpu);
extern inline bool is_task_latency_sensitive(struct task_struct *p);

extern void vip_enqueue_task(struct rq *rq, struct task_struct *p);

extern void vip_init(void);

extern inline bool vip_fair_task(struct task_struct *p);

#endif /* _VIP_H */
