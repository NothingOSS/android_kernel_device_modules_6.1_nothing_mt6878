/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __MTK_CAM_REQUEST_H
#define __MTK_CAM_REQUEST_H

#include <linux/list.h>
#include <media/media-request.h>
#include <mtk_cam-defs.h>

#include "mtk_cam-raw_pipeline.h"
#include "mtk_cam-sv_pipeline.h"
#include "mtk_cam-mraw_pipeline.h"

/**
 * mtk_cam_frame_sync: the frame sync state of one request
 *
 * @target: the num of ctx(sensor) which should be synced
 * @on_cnt: the count of frame sync on called by ctx
 * @off_cnt: the count of frame sync off called by ctx
 * @op_lock: protect frame sync state variables
 */
struct mtk_cam_frame_sync {
	unsigned int target;
	unsigned int on_cnt;
	unsigned int off_cnt;
	struct mutex op_lock;
};

static inline void frame_sync_init(struct mtk_cam_frame_sync *fs, int ctx_cnt)
{
	fs->target = ctx_cnt > 1 ? ctx_cnt : 0;
	fs->on_cnt = 0;
	fs->off_cnt = 0;
}

static inline bool need_frame_sync(struct mtk_cam_frame_sync *fs)
{
	return !!fs->target;  /* check multi sensor stream */
}

static inline void frame_sync_dec_target(struct mtk_cam_frame_sync *fs)
{
	mutex_lock(&fs->op_lock);
	if (!fs->target) {
		mutex_unlock(&fs->op_lock);
		return;
	}

	fs->target--;
	if (fs->target <= 1) {
		fs->target = 0;
		fs->on_cnt = 0;
		fs->off_cnt = 0;
	}
	mutex_unlock(&fs->op_lock);
}

struct v4l2_ctrl_handler;
/*
 * struct mtk_cam_request - MTK camera request.
 *
 * @req: Embedded struct media request.
 */
struct mtk_cam_request {
	struct media_request req;
	struct list_head list; /* entry in pending_job_list */

	int used_ctx;
	unsigned int used_pipe;

	spinlock_t buf_lock;
	struct list_head buf_list;

	struct mtk_cam_frame_sync fs;

	struct mtk_raw_request_data raw_data[MTKCAM_SUBDEV_RAW_NUM]; /* TODO: count */
	struct mtk_camsv_request_data sv_data[MTKCAM_SUBDEV_CAMSV_NUM];
	struct mtk_mraw_request_data mraw_data[MTKCAM_SUBDEV_MRAW_NUM];
};

static inline struct mtk_cam_request *
to_mtk_cam_req(struct media_request *__req)
{
	return container_of(__req, struct mtk_cam_request, req);
}

static inline int mtk_cam_req_used_ctx(struct media_request *req)
{
	struct mtk_cam_request *cam_req = to_mtk_cam_req(req);

	return cam_req->used_ctx;
}

struct media_request_object *
mtk_cam_req_find_ctrl_obj(struct mtk_cam_request *req, void *ctrl_hdl);

struct mtk_cam_buffer *
mtk_cam_req_find_buffer(struct mtk_cam_request *req, int pipe_id, int dma_port);

int mtk_cam_req_complete_ctrl_obj(struct media_request_object *obj);

void mtk_cam_req_dump_incomplete_ctrl(struct mtk_cam_request *req);
void mtk_cam_req_dump_incomplete_buffer(struct mtk_cam_request *req);

#endif //__MTK_CAM_REQUEST_H
