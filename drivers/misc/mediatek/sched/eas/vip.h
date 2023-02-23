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

#define is_vip(ftsk) (ftsk->vip_prio != NOT_VIP)
#define vts_to_ts(vts) ({ \
		void *__mptr = (void *)(vts); \
		((struct task_struct *)(__mptr - \
			offsetof(struct task_struct, android_vendor_data1))); })

extern void vip_enqueue_task(struct rq *rq, struct task_struct *p);

extern void vip_init(void);

extern inline bool vip_fair_task(struct task_struct *p);

#endif /* _VIP_H */
