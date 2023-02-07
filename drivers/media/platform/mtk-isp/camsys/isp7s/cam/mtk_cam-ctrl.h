/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef __MTK_CAM_CTRL_H
#define __MTK_CAM_CTRL_H

#include <linux/hrtimer.h>
#include <linux/timer.h>

#include "mtk_cam-job.h"
struct mtk_cam_device;
struct mtk_raw_device;
struct mtk_camsv_device;

enum MTK_CAMSYS_IRQ_EVENT {
	/* with normal_data */
	CAMSYS_IRQ_SETTING_DONE = 0,
	CAMSYS_IRQ_FRAME_START,
	CAMSYS_IRQ_AFO_DONE,
	CAMSYS_IRQ_FRAME_DONE,
	CAMSYS_IRQ_TRY_SENSOR_SET,
	CAMSYS_IRQ_FRAME_DROP,
	CAMSYS_IRQ_FRAME_START_DCIF_MAIN,
	CAMSYS_IRQ_FRAME_SKIPPED,
	/* with error_data */
	CAMSYS_IRQ_ERROR,
};

enum MTK_CAMSYS_ENGINE_TYPE {
	CAMSYS_ENGINE_RAW,
	CAMSYS_ENGINE_MRAW,
	CAMSYS_ENGINE_CAMSV,
	CAMSYS_ENGINE_SENINF,
};

unsigned long engine_idx_to_bit(int engine_type, int idx);

struct vsync_result {
	unsigned char is_first : 1;
	unsigned char is_last  : 1;
};

struct vsync_collector {
	unsigned int desired;
	unsigned int collected;
};

static inline void vsync_reset(struct vsync_collector *c)
{
	c->desired = c->collected = 0;
}

static inline void vsync_set_desired(struct vsync_collector *c,
				     unsigned int desried)
{
	c->desired = desried;
	c->collected = 0;
}

void vsync_update(struct vsync_collector *c,
		  int engine_type, int idx,
		  struct vsync_result *res);

/*per stream (sensor) */
struct mtk_cam_ctrl {
	struct mtk_cam_ctx *ctx;
	struct work_struct stream_on_work;

	struct sensor_apply_params s_params;

	atomic_t enqueued_frame_seq_no;		/* enque job counter - ctrl maintain */

	atomic_t stopped;
	atomic_t ref_cnt;

	/* note:
	 *   this send_lock is only used in send_event func to guarantee that send_event
	 *   is executed in an exclusive manner.
	 */
	spinlock_t send_lock;
	rwlock_t list_lock;
	struct list_head camsys_state_list;

	spinlock_t info_lock;
	struct mtk_cam_ctrl_runtime_info r_info;
	wait_queue_head_t stop_wq;
	int sensor_set_ref;
	int state_trans_ref;

	struct vsync_collector vsync_col;
};

struct mtk_camsys_irq_normal_data {
};

struct mtk_camsys_irq_error_data {
	int err_status;
};

struct mtk_camsys_irq_info {
	enum MTK_CAMSYS_IRQ_EVENT irq_type;
	u64 ts_ns;
	int frame_idx;
	int frame_idx_inner;
	int cookie_done;
	int write_cnt;
	int fbc_cnt;
	unsigned int sof_tags;
	unsigned int done_tags;
	unsigned int err_tags;
	union {
		struct mtk_camsys_irq_normal_data	n;
		struct mtk_camsys_irq_error_data	e;
	};
};

int mtk_cam_ctrl_isr_event(struct mtk_cam_device *cam,
	enum MTK_CAMSYS_ENGINE_TYPE engine_type, unsigned int engine_id,
	struct mtk_camsys_irq_info *irq_info);

/* ctx_stream_on */
void mtk_cam_ctrl_start(struct mtk_cam_ctrl *cam_ctrl,
	struct mtk_cam_ctx *ctx);
/* ctx_stream_off */
void mtk_cam_ctrl_stop(struct mtk_cam_ctrl *cam_ctrl);
/* enque job */
void mtk_cam_ctrl_job_enque(struct mtk_cam_ctrl *cam_ctrl,
	struct mtk_cam_job *job);
/* inform job composed */
void mtk_cam_ctrl_job_composed(struct mtk_cam_ctrl *cam_ctrl,
			       unsigned int fh_cookie,
			       struct mtkcam_ipi_frame_ack_result *cq_ret);

void mtk_cam_event_frame_sync(struct mtk_cam_ctrl *cam_ctrl,
			      unsigned int frame_seq_no);

static inline
void mtk_cam_ctrl_apply_by_state(struct mtk_cam_ctrl *ctrl, int enable)
{
	spin_lock(&ctrl->info_lock);
	ctrl->r_info.apply_hw_by_FSM = !!enable;
	spin_unlock(&ctrl->info_lock);
}

#endif
