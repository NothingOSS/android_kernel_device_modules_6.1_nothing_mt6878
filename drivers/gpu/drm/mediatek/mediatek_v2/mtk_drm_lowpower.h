/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#ifndef _MTK_DRM_LOWPOWER_H_
#define _MTK_DRM_LOWPOWER_H_

#include <drm/drm_crtc.h>
#include "mtk_drm_crtc.h"

struct mtk_idle_private_data {
	//the target cpu bind to idlemgr
	int cpu_id;
	//min freq settings, unit of HZ
	int cpu_freq;
	//vblank off async is supported or not
	bool vblank_async;
};

struct mtk_drm_idlemgr_context {
	unsigned long long idle_check_interval;
	unsigned long long idlemgr_last_kick_time;
	unsigned int enterulps;
	int session_mode_before_enter_idle;
	int is_idle;
	int cur_lp_cust_mode;
	struct mtk_idle_private_data priv;
};

struct mtk_drm_idlemgr {
	struct task_struct *idlemgr_task;
	struct task_struct *kick_task;
	struct task_struct *async_vblank_task;
	struct task_struct *async_handler_task;
	wait_queue_head_t idlemgr_wq;
	wait_queue_head_t kick_wq;
	wait_queue_head_t async_vblank_wq;
	wait_queue_head_t async_handler_wq;
	wait_queue_head_t async_event_wq;
	atomic_t idlemgr_task_active;
	atomic_t kick_task_active;
	atomic_t async_vblank_active;
	//async is only enabled when enter/leave idle
	atomic_t async_enabled;
	//async event reference count
	atomic_t async_ref;
	//lock protection of async_cb_list management
	spinlock_t async_lock;
	//maintain cmdq_pkt to be complete and free
	struct list_head async_cb_list;
	//async_cb_list length
	unsigned int async_cb_count;
	struct mtk_drm_idlemgr_context *idlemgr_ctx;
};

struct mtk_drm_async_cb_data {
	struct drm_crtc *crtc;
	struct cmdq_pkt *handle;
	char *master;
};

struct mtk_drm_async_cb {
	struct mtk_drm_async_cb_data *data;
	struct list_head link;
};

//check if async is enabled
bool mtk_drm_idlemgr_get_async_status(struct drm_crtc *crtc);

/* flush cmdq pkt with async wait,
 * this is required if user wants to free cmdq pkt by async task,
 */
void mtk_drm_idle_async_flush(struct drm_crtc *crtc,
	char *master, struct cmdq_pkt *cmdq_handle);

/* maintain async event reference count,
 * only when reference count is 0, async wait can be finished.
 */
void mtk_drm_idlemgr_async_get(struct drm_crtc *crtc, char *master);
void mtk_drm_idlemgr_async_put(struct drm_crtc *_crtc, char *master);

void mtk_drm_idlemgr_kick(const char *source, struct drm_crtc *crtc,
			  int need_lock);
bool mtk_drm_is_idle(struct drm_crtc *crtc);

int mtk_drm_idlemgr_init(struct drm_crtc *crtc, int index);
unsigned int mtk_drm_set_idlemgr(struct drm_crtc *crtc, unsigned int flag,
				 bool need_lock);
unsigned long long
mtk_drm_set_idle_check_interval(struct drm_crtc *crtc,
				unsigned long long new_interval);
unsigned long long
mtk_drm_get_idle_check_interval(struct drm_crtc *crtc);

void mtk_drm_idlemgr_kick_async(struct drm_crtc *crtc);


#endif
