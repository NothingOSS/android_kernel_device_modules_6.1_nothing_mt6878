// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#include <linux/list.h>
#include <linux/of.h>
#include <linux/pm_runtime.h>

#include <media/v4l2-event.h>
#include <media/v4l2-subdev.h>

#include "mtk_cam.h"
#include "mtk_cam-feature.h"

#include "mtk_cam-ctrl.h"
#include "mtk_cam-debug.h"
#include "mtk_cam-dvfs_qos.h"
#include "mtk_cam-hsf.h"
#include "mtk_cam-pool.h"
#include "mtk_cam-raw.h"
#include "mtk_cam-regs.h"
//#include "mtk_cam-raw_debug.h"
#include "mtk_cam-sv-regs.h"
#include "mtk_cam-mraw-regs.h"
//#include "mtk_cam-tg-flash.h"
#include "mtk_camera-v4l2-controls.h"
#include "mtk_camera-videodev2.h"
#include "mtk_cam-trace.h"

#include "imgsys/mtk_imgsys-cmdq-ext.h"

#include "frame_sync_camsys.h"


static int mtk_cam_ctrl_get(struct mtk_cam_ctrl *cam_ctrl)
{
	atomic_inc(&cam_ctrl->ref_cnt);

	if (unlikely(atomic_read(&cam_ctrl->stopped))) {
		if (atomic_dec_and_test(&cam_ctrl->ref_cnt))
			wake_up_interruptible(&cam_ctrl->stop_wq);
		return -1;
	}

	return 0;
}

static int mtk_cam_ctrl_put(struct mtk_cam_ctrl *cam_ctrl)
{
	if (atomic_dec_and_test(&cam_ctrl->ref_cnt))
		if (unlikely(atomic_read(&cam_ctrl->stopped)))
			wake_up_interruptible(&cam_ctrl->stop_wq);
	return 0;
}

/*
 * this function is blocked until no one could successfully do ctrl_get
 */
static int mtk_cam_ctrl_wait_all_released(struct mtk_cam_ctrl *cam_ctrl)
{
	struct mtk_cam_ctx *ctx = cam_ctrl->ctx;

	dev_info(ctx->cam->dev, "[%s] ctx:%d waiting\n",
		 __func__, ctx->stream_id);

	wait_event_interruptible(cam_ctrl->stop_wq,
				 !atomic_read(&cam_ctrl->ref_cnt));
	return 0;
}

bool cond_first_job(struct mtk_cam_job *job, void *arg)
{
	return 1;
}

bool cond_job_no_eq(struct mtk_cam_job *job, void *arg)
{
	int no = *(int *)arg;

	return job->frame_seq_no == no;
}

static struct mtk_cam_job *
mtk_cam_ctrl_get_job(struct mtk_cam_ctrl *ctrl,
		     bool (*cond_func)(struct mtk_cam_job *, void *arg),
		     void *arg)
{
	struct mtk_cam_job *job;
	struct mtk_cam_job_state *state;
	bool found = 0;

	read_lock(&ctrl->list_lock);
	list_for_each_entry(state, &ctrl->camsys_state_list, list) {
		job = container_of(state, struct mtk_cam_job, job_state);

		if ((found = cond_func(job, arg)))
			break;
	}
	read_unlock(&ctrl->list_lock);

	return found ? job : NULL;
}

static void mtk_cam_event_eos(struct mtk_cam_ctrl *cam_ctrl)
{
	struct v4l2_event event = {
		.type = V4L2_EVENT_EOS,
	};
	if (cam_ctrl->ctx->has_raw_subdev)
		mtk_cam_ctx_send_raw_event(cam_ctrl->ctx, &event);
	else
		mtk_cam_ctx_send_sv_event(cam_ctrl->ctx, &event);

	if (CAM_DEBUG_ENABLED(EVENT))
		pr_info("%s\n", __func__);
}

void mtk_cam_event_frame_sync(struct mtk_cam_ctrl *cam_ctrl,
			      unsigned int frame_seq_no)
{
	struct v4l2_event event = {
		.type = V4L2_EVENT_FRAME_SYNC,
		.u.frame_sync.frame_sequence = frame_seq_no,
	};
	if (cam_ctrl->ctx->has_raw_subdev)
		mtk_cam_ctx_send_raw_event(cam_ctrl->ctx, &event);
	else
		mtk_cam_ctx_send_sv_event(cam_ctrl->ctx, &event);

	if (CAM_DEBUG_ENABLED(EVENT))
		pr_info("%s: %u\n", __func__, frame_seq_no);
}

static void dump_runtime_info(struct mtk_cam_ctrl_runtime_info *info)
{
	if (!info->apply_hw_by_statemachine)
		pr_info("runtime: by_statemachine is off\n");

	pr_info("runtime: ack %d out/in %d/%d\n",
		info->ack_seq_no, info->outer_seq_no, info->inner_seq_no);
}

static void _ctrl_send_event_locked(struct mtk_cam_ctrl *ctrl,
				    struct transition_param *p)
{
	struct mtk_cam_job_state *state;

	MTK_CAM_TRACE_FUNC_BEGIN(BASIC);
	/* note: make sure read_lock(&ctrl->list_lock) is held */
	list_for_each_entry(state, &ctrl->camsys_state_list, list) {
		state->ops->send_event(state, p);
	}
	MTK_CAM_TRACE_END(BASIC);
}

static int _ctrl_apply_locked(struct mtk_cam_ctrl *ctrl)
{
	struct mtk_cam_job *job;
	struct mtk_cam_job_state *state;

	/* note: make sure read_lock(&ctrl->list_lock) is held */

	list_for_each_entry(state, &ctrl->camsys_state_list, list) {
		job = container_of(state, struct mtk_cam_job, job_state);

		mtk_cam_job_apply_pending_action(job);
	}

	return 0;
}
static int mtk_cam_ctrl_send_event(struct mtk_cam_ctrl *ctrl, int event)
{
	struct mtk_cam_ctrl_runtime_info local_info;
	struct transition_param p;

	MTK_CAM_TRACE_FUNC_BEGIN(BASIC);

	spin_lock(&ctrl->info_lock);
	local_info = ctrl->r_info;
	spin_unlock(&ctrl->info_lock);

	p.head = &ctrl->camsys_state_list;
	p.info = &local_info;
	p.event = event;

	if (CAM_DEBUG_ENABLED(STATE))
		dump_runtime_info(p.info);

	if (CAM_DEBUG_ENABLED(STATE))
		dev_info(ctrl->ctx->cam->dev, "[%s] event:%d, out/in:%d/%d\n",
		__func__, event, p.info->outer_seq_no, p.info->inner_seq_no);

	spin_lock(&ctrl->send_lock);

	read_lock(&ctrl->list_lock);
	_ctrl_send_event_locked(ctrl, &p);
	_ctrl_apply_locked(ctrl);
	read_unlock(&ctrl->list_lock);

	spin_unlock(&ctrl->send_lock);

	MTK_CAM_TRACE_END(BASIC);
	return 0;
}

/* sw irq - hrtimer context */
static enum hrtimer_restart
sensor_deadline_timer_handler(struct hrtimer *t)
{
	struct mtk_cam_ctrl *cam_ctrl =
		container_of(t, struct mtk_cam_ctrl,
			     sensor_deadline_timer);
	int time_after_sof = ktime_get_boottime_ns() / 1000000 -
			   cam_ctrl->sof_time;

	if (mtk_cam_ctrl_get(cam_ctrl))
		return HRTIMER_NORESTART;
	/* handle V4L2_EVENT_REQUEST_DRAINED event */
	// drained_res = mtk_cam_request_drained(cam_ctrl);

	mtk_cam_ctrl_send_event(cam_ctrl, CAMSYS_EVENT_TIMER_SENSOR);

	if (CAM_DEBUG_ENABLED(STATE))
		dev_info(cam_ctrl->ctx->cam->dev, "[%s][sof+%dms]\n",
		__func__, time_after_sof);
	mtk_cam_ctrl_put(cam_ctrl);

	return HRTIMER_NORESTART;
}

static void
mtk_cam_sof_timer_setup(struct mtk_cam_ctrl *cam_ctrl)
{
	ktime_t m_kt;
	struct mtk_seninf_sof_notify_param param;
	int after_sof_ms = ktime_get_boottime_ns() / 1000000
			- cam_ctrl->sof_time;

	/*notify sof to sensor*/
	param.sd = cam_ctrl->ctx->seninf;
	/* TODO(AY): latest applied frame_no */
	param.sof_cnt = 0;
	mtk_cam_seninf_sof_notify(&param);

	cam_ctrl->sensor_deadline_timer.function =
		sensor_deadline_timer_handler;
	if (after_sof_ms < 0)
		after_sof_ms = 0;
	else if (after_sof_ms > cam_ctrl->timer_req_event)
		after_sof_ms = cam_ctrl->timer_req_event;
	m_kt = ktime_set(0, cam_ctrl->timer_req_event * 1000000
			- after_sof_ms * 1000000);
	hrtimer_start(&cam_ctrl->sensor_deadline_timer, m_kt,
		      HRTIMER_MODE_REL);
}

static void handle_setting_done(struct mtk_cam_ctrl *cam_ctrl)
{
	mtk_cam_ctrl_send_event(cam_ctrl, CAMSYS_EVENT_IRQ_SETTING_DONE);
}

static void handle_meta1_done(struct mtk_cam_ctrl *cam_ctrl)
{
	mtk_cam_ctrl_send_event(cam_ctrl, CAMSYS_EVENT_IRQ_AFO_DONE);
}

static void handle_frame_done(struct mtk_cam_ctrl *cam_ctrl)
{
	mtk_cam_ctrl_send_event(cam_ctrl, CAMSYS_EVENT_IRQ_FRAME_DONE);
}

static void handle_raw_frame_start(struct mtk_cam_ctrl *ctrl)
{
	struct mtk_cam_job *processing_job = NULL;
	int frame_sync_no;

	spin_lock(&ctrl->info_lock);
	ctrl->sof_time = ctrl->r_info.sof_ts_ns / 1000000;
	frame_sync_no = ctrl->r_info.inner_seq_no;
	spin_unlock(&ctrl->info_lock);

	processing_job =
		mtk_cam_ctrl_get_job(ctrl, cond_job_no_eq, &frame_sync_no);

	/* no continuosly enque case */
	if (processing_job) {
		ctrl->sensor_set_ref = mtk_cam_job_get_sensor_set_ref(processing_job);
		ctrl->state_trans_ref = mtk_cam_job_get_state_trans_ref(processing_job);
	}

	if (ctrl->sensor_set_ref == ctrl->r_info.event_engine) {
		mtk_cam_event_frame_sync(ctrl, frame_sync_no);
		mtk_cam_sof_timer_setup(ctrl);
	}

	if (ctrl->state_trans_ref == ctrl->r_info.event_engine) {
		mtk_cam_ctrl_send_event(ctrl, CAMSYS_EVENT_IRQ_SOF);
	}

}

static int mtk_cam_event_handle_raw(struct mtk_cam_ctrl *ctrl,
				       unsigned int engine_id,
				       struct mtk_camsys_irq_info *irq_info)
{
	unsigned int seq_nearby =
		atomic_read(&ctrl->enqueued_frame_seq_no);

	MTK_CAM_TRACE_FUNC_BEGIN(BASIC);
	// struct mtk_cam_job_event_info event_info;
	//unsigned int ctx_id =
		//decode_fh_reserved_data_to_ctx(irq_info->frame_idx);

	// event_info.engine = (CAMSYS_ENGINE_RAW << 8) + engine_id;
	// event_info.ctx_id = ctx_id;

	/* raw's CQ done */
	if (irq_info->irq_type & BIT(CAMSYS_IRQ_SETTING_DONE)) {
		spin_lock(&ctrl->info_lock);
		ctrl->r_info.outer_seq_no =
			decode_fh_reserved_data_to_seq(seq_nearby, irq_info->frame_idx);
		spin_unlock(&ctrl->info_lock);

		handle_setting_done(ctrl);
	}

	/* raw's DMA done, we only allow AFO done here */
	if (irq_info->irq_type & BIT(CAMSYS_IRQ_AFO_DONE))
		handle_meta1_done(ctrl);

	/* raw's SW done */
	if (irq_info->irq_type & BIT(CAMSYS_IRQ_FRAME_DONE))
		handle_frame_done(ctrl);

	/* raw's SOF (proc engine frame start) */
	if (irq_info->irq_type & BIT(CAMSYS_IRQ_FRAME_START)) {
		spin_lock(&ctrl->info_lock);
		ctrl->r_info.sof_ts_ns = irq_info->ts_ns;
		ctrl->r_info.outer_seq_no =
			decode_fh_reserved_data_to_seq(seq_nearby, irq_info->frame_idx);
		ctrl->r_info.inner_seq_no =
			decode_fh_reserved_data_to_seq(seq_nearby, irq_info->frame_idx_inner);
		ctrl->r_info.event_engine = CAMSYS_EVENT_SOURCE_RAW;
		spin_unlock(&ctrl->info_lock);

		handle_raw_frame_start(ctrl);
	}

	/* DCIF' SOF (dc link engine frame start (first exposure) ) */
	//if (irq_info->irq_type & (1 << CAMSYS_IRQ_FRAME_START_DCIF_MAIN)) {
		// handle_dcif_frame_start(); - TBC
	//}

	MTK_CAM_TRACE_END(BASIC);
	return 0;
}

static int mtk_camsys_event_handle_camsv(struct mtk_cam_ctrl *ctrl,
				       unsigned int engine_id,
				       struct mtk_camsys_irq_info *irq_info)
{

	unsigned int seq_nearby =
		atomic_read(&ctrl->enqueued_frame_seq_no);

	// struct mtk_cam_job_event_info event_info;
	//unsigned int ctx_id =
		//decode_fh_reserved_data_to_ctx(irq_info->frame_idx);
	// event_info.engine = (CAMSYS_ENGINE_RAW << 8) + engine_id;
	// event_info.ctx_id = ctx_id;
	// to be removed for camsv enable irq but wrong inner/outer no
	if (ctrl->sensor_set_ref != CAMSYS_EVENT_SOURCE_CAMSV &&
		ctrl->state_trans_ref != CAMSYS_EVENT_SOURCE_CAMSV)
		return 0;
	/* camsv's CQ done */
	if (irq_info->irq_type & BIT(CAMSYS_IRQ_SETTING_DONE)) {
		spin_lock(&ctrl->info_lock);
		ctrl->r_info.outer_seq_no =
			decode_fh_reserved_data_to_seq(seq_nearby, irq_info->frame_idx);
		spin_unlock(&ctrl->info_lock);
		handle_setting_done(ctrl);
	}

	/* camsv's SW done */
	if (irq_info->irq_type & BIT(CAMSYS_IRQ_FRAME_DONE))
		handle_frame_done(ctrl);

	/* camsv's SOF (proc engine frame start) */
	if (irq_info->irq_type & BIT(CAMSYS_IRQ_FRAME_START)){
		spin_lock(&ctrl->info_lock);
		ctrl->r_info.sof_ts_ns = irq_info->ts_ns;
		ctrl->r_info.outer_seq_no =
			decode_fh_reserved_data_to_seq(seq_nearby, irq_info->frame_idx);
		ctrl->r_info.inner_seq_no =
			decode_fh_reserved_data_to_seq(seq_nearby, irq_info->frame_idx_inner);
		ctrl->r_info.event_engine = CAMSYS_EVENT_SOURCE_CAMSV;
		spin_unlock(&ctrl->info_lock);
		handle_raw_frame_start(ctrl);
	}

	return 0;
}

static int mtk_camsys_event_handle_mraw(struct mtk_cam_ctrl *ctrl,
					unsigned int engine_id,
					struct mtk_camsys_irq_info *irq_info)
{
	unsigned int seq_nearby =
		atomic_read(&ctrl->enqueued_frame_seq_no);
	// struct mtk_cam_job_event_info event_info;
	//unsigned int ctx_id =
		//decode_fh_reserved_data_to_ctx(irq_info->frame_idx);


	// event_info.engine = (CAMSYS_ENGINE_RAW << 8) + engine_id;
	// event_info.ctx_id = ctx_id;
	/* mraw's CQ done */
	if (irq_info->irq_type & BIT(CAMSYS_IRQ_SETTING_DONE)) {
		spin_lock(&ctrl->info_lock);
		ctrl->r_info.outer_seq_no =
			decode_fh_reserved_data_to_seq(seq_nearby, irq_info->frame_idx);
		spin_unlock(&ctrl->info_lock);
		handle_setting_done(ctrl);
	}

	/* mraw's SW done */
	if (irq_info->irq_type & BIT(CAMSYS_IRQ_FRAME_DONE))
		handle_frame_done(ctrl);

	return 0;
}

int mtk_cam_ctrl_isr_event(struct mtk_cam_device *cam,
			 enum MTK_CAMSYS_ENGINE_TYPE engine_type,
			 unsigned int engine_id,
			 struct mtk_camsys_irq_info *irq_info)
{
	unsigned int ctx_id =
		decode_fh_reserved_data_to_ctx(irq_info->frame_idx);
	struct mtk_cam_ctrl *cam_ctrl = &cam->ctxs[ctx_id].cam_ctrl;
	int ret = 0;

	/* TBC
	MTK_CAM_TRACE_BEGIN(BASIC, "irq_type %d, inner %d",
			    irq_info->irq_type, irq_info->frame_idx_inner);
	*/
	/**
	 * Here it will be implemented dispatch rules for some scenarios
	 * like twin/stagger/m-stream,
	 * such cases that camsys will collect all coworked sub-engine's
	 * signals and trigger some engine of them to do some job
	 * individually.
	 * twin - rawx2
	 * stagger - rawx1, camsv x2
	 * m-stream - rawx1 , camsv x2
	 */

	switch (engine_type) {
	case CAMSYS_ENGINE_RAW:
		if (mtk_cam_ctrl_get(cam_ctrl))
			return 0;
		ret = mtk_cam_event_handle_raw(cam_ctrl, engine_id, irq_info);
		mtk_cam_ctrl_put(cam_ctrl);
		break;
	case CAMSYS_ENGINE_MRAW:
		if (mtk_cam_ctrl_get(cam_ctrl))
			return 0;
		ret = mtk_camsys_event_handle_mraw(cam_ctrl, engine_id, irq_info);
		mtk_cam_ctrl_put(cam_ctrl);
		break;
	case CAMSYS_ENGINE_CAMSV:
		if (mtk_cam_ctrl_get(cam_ctrl))
			return 0;
		ret = mtk_camsys_event_handle_camsv(cam_ctrl, engine_id, irq_info);
		mtk_cam_ctrl_put(cam_ctrl);
		break;
	case CAMSYS_ENGINE_SENINF:
		/* ToDo - cam mux setting delay handling */
		if (irq_info->irq_type & BIT(CAMSYS_IRQ_FRAME_DROP))
			dev_info(cam->dev, "MTK_CAMSYS_ENGINE_SENINF_TAG engine:%d type:0x%x\n",
				engine_id, irq_info->irq_type);
		break;
	default:
		break;
	}
	/* TBC
	MTK_CAM_TRACE_END(BASIC);
	*/
	return ret;
}

static void mtk_cam_ctrl_stream_on_work(struct work_struct *work)
{
	struct mtk_cam_ctrl *ctrl =
		container_of(work, struct mtk_cam_ctrl, stream_on_work);
	struct mtk_cam_job *job;
	unsigned long timeout = msecs_to_jiffies(1000);
	int next_job_no;

	dev_info(ctrl->ctx->cam->dev, "[%s] begin\n", __func__);

	job = mtk_cam_ctrl_get_job(ctrl, cond_first_job, 0);
	if (!job)
		return;

	if (!wait_for_completion_timeout(&job->compose_completion, timeout))
	{
		pr_info("[%s] error: wait for job composed timeout\n",
			__func__);
		return;
	}

	mtk_cam_job_state_set(&job->job_state, SENSOR_STATE, S_SENSOR_APPLYING);
	call_jobop(job, apply_sensor);

	mtk_cam_job_state_set(&job->job_state, ISP_STATE, S_ISP_APPLYING);
	call_jobop(job, apply_isp);

	if (!wait_for_completion_timeout(&job->cq_exe_completion, timeout))
	{
		pr_info("[%s] error: wait for job cq exe\n",
			__func__);
		return;
	}

	ctrl->timer_req_event = mtk_cam_job_get_sensor_margin(job);

	call_jobop(job, stream_on, true);

	next_job_no = job->frame_seq_no + 1;

	job = mtk_cam_ctrl_get_job(ctrl, cond_job_no_eq, &next_job_no);
	if (job)
		call_jobop(job, apply_sensor);

	mtk_cam_ctrl_apply_by_state(ctrl, 1);

	dev_info(ctrl->ctx->cam->dev, "[%s] finish\n", __func__);
}

/* request queue */
void mtk_cam_ctrl_job_enque(struct mtk_cam_ctrl *cam_ctrl,
			    struct mtk_cam_job *job)
{
	struct mtk_cam_ctx *ctx;
	u32 next_frame_seq;

	if (mtk_cam_ctrl_get(cam_ctrl))
		return;

	ctx = cam_ctrl->ctx;
	next_frame_seq = atomic_inc_return(&cam_ctrl->enqueued_frame_seq_no);

	/* get before adding to list */
	mtk_cam_job_get(job);

	/* EnQ this request's state element to state_list (STATE:READY) */
	write_lock(&cam_ctrl->list_lock);
	list_add_tail(&job->job_state.list, &cam_ctrl->camsys_state_list);
	write_unlock(&cam_ctrl->list_lock);

	job->frame_seq_no = next_frame_seq;
	// to be removed
	if (next_frame_seq == 1) {
		cam_ctrl->sensor_set_ref = mtk_cam_job_get_sensor_set_ref(job);
		cam_ctrl->state_trans_ref = mtk_cam_job_get_state_trans_ref(job);
	}

	/* TODO(AY): refine this */
	if (job->job_scen.id == MTK_CAM_SCEN_M2M_NORMAL ||
	    job->job_scen.id == MTK_CAM_SCEN_ODT_NORMAL)
		mtk_cam_ctrl_apply_by_state(cam_ctrl, 1);

	call_jobop(job, compose);
	mtk_cam_ctrl_send_event(cam_ctrl, CAMSYS_EVENT_ENQUE);
	dev_dbg(ctx->cam->dev, "[%s] ctx:%d, frame_no:%d, next_frame_seq:%d\n",
		__func__, ctx->stream_id, job->frame_seq_no, next_frame_seq);

	if (job->stream_on_seninf) {
		/*
		 * Note: assume this function is called from user's context,
		 *       thus, no need to consider race condition for
		 *       stream_on_work (between enque & stream off).
		 */
		INIT_WORK(&cam_ctrl->stream_on_work, mtk_cam_ctrl_stream_on_work);

		/* note: not sure if using system_highpri_wq is suitable */
		queue_work(system_highpri_wq, &cam_ctrl->stream_on_work);
	}

	mtk_cam_ctrl_put(cam_ctrl);
}

void mtk_cam_ctrl_job_composed(struct mtk_cam_ctrl *cam_ctrl,
			       int frame_seq,
			       struct mtkcam_ipi_frame_ack_result *cq_ret)
{
	//struct mtk_cam_job_state *job_s;
	//struct mtk_cam_job *job, *job_composed = NULL;
	struct mtk_cam_job *job_composed;
	struct mtk_cam_device *cam;
	int fh_temp_ctx_id, fh_temp_seq;

	if (mtk_cam_ctrl_get(cam_ctrl))
		return;
	cam = cam_ctrl->ctx->cam;
	fh_temp_ctx_id = decode_fh_reserved_data_to_ctx(frame_seq);
	/* TODO(AY): why not use frame_seq? */
	fh_temp_seq = decode_fh_reserved_data_to_seq(
		atomic_read(&cam_ctrl->enqueued_frame_seq_no), frame_seq);

	job_composed = mtk_cam_ctrl_get_job(cam_ctrl,
					    cond_job_no_eq, &fh_temp_seq);

	/* TODO(AY): backend update cq_ret directly on ipi buffer */
	if (job_composed) {
		if (job_composed->ctx_id == fh_temp_ctx_id) {
			/* assign job->cq_rst */
			call_jobop(job_composed, compose_done, cq_ret);
		} else {
			dev_info(cam->dev, "[%s] job->ctx_id/ fh_temp_ctx_id = %d/%d\n",
			__func__, job_composed->ctx_id, fh_temp_ctx_id);
		}
	} else {
		dev_info(cam->dev, "[%s] not found, ctx_id/ frame_id = %d/%d\n",
		__func__, fh_temp_ctx_id, fh_temp_seq);
	}

	spin_lock(&cam_ctrl->info_lock);
	cam_ctrl->r_info.ack_seq_no = fh_temp_seq;
	spin_unlock(&cam_ctrl->info_lock);

	mtk_cam_ctrl_send_event(cam_ctrl, CAMSYS_EVENT_ACK);

	mtk_cam_ctrl_put(cam_ctrl);
}

static void reset_runtime_info(struct mtk_cam_ctrl_runtime_info *info)
{
	memset(info, 0, sizeof(*info));
}

void mtk_cam_ctrl_start(struct mtk_cam_ctrl *cam_ctrl, struct mtk_cam_ctx *ctx)
{
	cam_ctrl->ctx = ctx;
	INIT_WORK(&cam_ctrl->stream_on_work, NULL);

	atomic_set(&cam_ctrl->stopped, 0);
	atomic_set(&cam_ctrl->enqueued_frame_seq_no, 0);
	cam_ctrl->sof_time = ktime_get_boottime_ns() / 1000000;
	cam_ctrl->timer_req_event = 0;

	spin_lock_init(&cam_ctrl->send_lock);
	rwlock_init(&cam_ctrl->list_lock);
	INIT_LIST_HEAD(&cam_ctrl->camsys_state_list);

	spin_lock_init(&cam_ctrl->info_lock);
	reset_runtime_info(&cam_ctrl->r_info);

	init_waitqueue_head(&cam_ctrl->stop_wq);
	if (ctx->sensor) {
		hrtimer_init(&cam_ctrl->sensor_deadline_timer,
			     CLOCK_MONOTONIC, HRTIMER_MODE_REL);
		cam_ctrl->sensor_deadline_timer.function =
			sensor_deadline_timer_handler;
	}

	dev_info(ctx->cam->dev, "[%s] ctx:%d\n", __func__, ctx->stream_id);
}

void mtk_cam_ctrl_stop(struct mtk_cam_ctrl *cam_ctrl)
{
	struct mtk_cam_ctx *ctx = cam_ctrl->ctx;
	struct mtk_cam_job_state *job_s, *job_s_prev;
	struct mtk_cam_job *job;
	struct mtk_raw_device *raw_dev;
	struct mtk_camsv_device *sv_dev;
	struct mtk_mraw_device *mraw_dev;
	int i, j;

	/* stop procedure
	 * 1. mark 'stopped' status to skip further processing
	 * 2. stop all working context
	 *   a. disable_irq for threaded_irq
	 *   b. workqueue: cancel_work_sync & (drain/flush)_workqueue
	 *   c. kthread: cancel_work_sync & flush_worker
	 * 3. Now, all contexts are stopped. return resources
	 */
	atomic_set(&cam_ctrl->stopped, 1);

	if (cam_ctrl->stream_on_work.func)
		cancel_work_sync(&cam_ctrl->stream_on_work);

	/* disable irq first */
	if (ctx->hw_raw) {
		raw_dev = dev_get_drvdata(ctx->hw_raw);
		disable_irq(raw_dev->irq);
	}
	if (ctx->hw_sv) {
		sv_dev = dev_get_drvdata(ctx->hw_sv);
		for (j = 0; j < ARRAY_SIZE(sv_dev->irq); j++)
			disable_irq(sv_dev->irq[j]);
	}
	for (i = 0; i < ARRAY_SIZE(ctx->hw_mraw); i++) {
		if (ctx->hw_mraw[i]) {
			mraw_dev = dev_get_drvdata(ctx->hw_mraw[i]);
			disable_irq(mraw_dev->irq);
		}
	}

	mtk_cam_ctrl_wait_all_released(cam_ctrl);

	/* reset hw */
	if (ctx->hw_raw)
		reset(raw_dev);
	if (ctx->hw_sv)
		sv_reset(sv_dev);
	for (i = 0; i < ARRAY_SIZE(ctx->hw_mraw); i++) {
		if (ctx->hw_mraw[i]) {
			mraw_dev = dev_get_drvdata(ctx->hw_mraw[i]);
			mraw_reset(mraw_dev);
		}
	}
	mtk_cam_event_eos(cam_ctrl);

	drain_workqueue(ctx->frame_done_wq);

	if (ctx->sensor) {
		hrtimer_cancel(&cam_ctrl->sensor_deadline_timer);
	}
	/* using func. kthread_cancel_work_sync, which contains kthread_flush_work func.*/
	//kthread_cancel_work_sync(&cam_ctrl->work);
	kthread_flush_worker(&ctx->sensor_worker);

	write_lock(&cam_ctrl->list_lock);
	list_for_each_entry_safe(job_s, job_s_prev,
				 &cam_ctrl->camsys_state_list, list) {
		job = container_of(job_s, struct mtk_cam_job, job_state);

		call_jobop(job, cancel);
		mtk_cam_ctx_job_finish(job);

		/* note: call list_del directly here to avoid deadlock in
		 * _on_job_last_ref */
		list_del(&job->job_state.list);
	}
	write_unlock(&cam_ctrl->list_lock);


	dev_info(ctx->cam->dev, "[%s] ctx:%d, stop status:%d\n",
		__func__, ctx->stream_id, atomic_read(&cam_ctrl->stopped));
}

