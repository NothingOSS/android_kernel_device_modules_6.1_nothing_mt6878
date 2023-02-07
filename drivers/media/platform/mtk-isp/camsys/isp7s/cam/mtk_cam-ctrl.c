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
#include "mtk_cam-job_utils.h"

#include "imgsys/mtk_imgsys-cmdq-ext.h"

#include "frame_sync_camsys.h"

unsigned long engine_idx_to_bit(int engine_type, int idx)
{
	unsigned int map_hw = 0;

	if (engine_type == CAMSYS_ENGINE_RAW)
		map_hw = MAP_HW_RAW;
	else if (engine_type == CAMSYS_ENGINE_MRAW)
		map_hw = MAP_HW_MRAW;
	else if (engine_type == CAMSYS_ENGINE_CAMSV)
		map_hw = MAP_HW_CAMSV;

	return bit_map_bit(map_hw, idx);
}

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

		found = cond_func(job, arg);
		if (found)
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
		pr_info("%s: ctx %d\n", __func__, cam_ctrl->ctx->stream_id);
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
	if (!info->apply_hw_by_FSM)
		pr_info("runtime: by_FSM is off\n");

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

static void debug_send_event(const struct transition_param *p)
{
	struct mtk_cam_ctrl_runtime_info *info;
	bool print_ts;

	info = p->info;

	print_ts = (p->event == CAMSYS_EVENT_ENQUE);

	if (!info->apply_hw_by_FSM)
		pr_info("[%s] runtime: by_FSM is off\n", __func__);

	if (print_ts)
		pr_info("[%s] out/in:%d/%d event: %s@%llu (sof %llu)\n",
			__func__,
			info->outer_seq_no, info->inner_seq_no,
			str_event(p->event),
			p->event_ts, info->sof_ts_ns);
	else
		pr_info("[%s] out/in:%d/%d event: %s\n",
			__func__,
			info->outer_seq_no, info->inner_seq_no,
			str_event(p->event));
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
	p.event_ts = ktime_get_boottime_ns();
	p.s_params = &ctrl->s_params;

	if (0 && CAM_DEBUG_ENABLED(STATE))
		dump_runtime_info(p.info);

	if (CAM_DEBUG_ENABLED(STATE))
		debug_send_event(&p);

	spin_lock(&ctrl->send_lock);

	read_lock(&ctrl->list_lock);
	_ctrl_send_event_locked(ctrl, &p);
	_ctrl_apply_locked(ctrl);
	read_unlock(&ctrl->list_lock);

	spin_unlock(&ctrl->send_lock);

	MTK_CAM_TRACE_END(BASIC);
	return 0;
}

static void handle_setting_done(struct mtk_cam_ctrl *cam_ctrl)
{
	mtk_cam_ctrl_send_event(cam_ctrl, CAMSYS_EVENT_IRQ_L_CQ_DONE);
}

static void handle_meta1_done(struct mtk_cam_ctrl *ctrl, int seq_no)
{
	struct mtk_cam_job *job;

	job = mtk_cam_ctrl_get_job(ctrl, cond_job_no_eq, &seq_no);

	if (!job) {
		pr_info("%s: warn. job not found seq %d\n",
			__func__,  seq_no);
		return;
	}

	call_jobop(job, mark_afo_done, seq_no);
}

static void handle_frame_done(struct mtk_cam_ctrl *ctrl,
			      int engine_type, int engine_id,
			      int seq_no)
{
	struct mtk_cam_job *job;

	job = mtk_cam_ctrl_get_job(ctrl, cond_job_no_eq, &seq_no);

	/*
	 *
	 * 1. handle each done => mark_engine_done
	 *      TODO: check state
	 *      if in wrong state, force transit?
	 * 2. check if is last done,
	 * 3. send_event(IRQ_FRAME_DONE)
	 *    need to send_event IRQ_FRAME_DONE for m2m trigger
	 */
	if (!job) {
		pr_info("%s: warn. job not found seq %d\n",
			__func__,  seq_no);
		return;
	}

	if (call_jobop(job, mark_engine_done,
		       engine_type, engine_id, seq_no)) {

		/* last done: trigger FSM */
		mtk_cam_ctrl_send_event(ctrl, CAMSYS_EVENT_IRQ_FRAME_DONE);
	}
}
static void handle_ss_try_set_sensor(struct mtk_cam_ctrl *cam_ctrl)
{
	mtk_cam_ctrl_send_event(cam_ctrl, CAMSYS_EVENT_TIMER_SENSOR);
}
static void ctrl_vsync_preprocess(struct mtk_cam_ctrl *ctrl,
				  enum MTK_CAMSYS_ENGINE_TYPE engine_type,
				  unsigned int engine_id,
				  struct mtk_camsys_irq_info *irq_info,
				  struct vsync_result *vsync_res)
{

	vsync_update(&ctrl->vsync_col,
		     engine_type, engine_id, vsync_res);

	spin_lock(&ctrl->info_lock);

	/*
	 * note:
	 *   this is used to handle for case that some engine is not enqueued,
	 *   so fh_cookie won't be updated.
	 */
	ctrl->r_info.tmp_inner_seq_no =
		max(ctrl->r_info.tmp_inner_seq_no,
		    (int)seq_from_fh_cookie(irq_info->frame_idx_inner));

	if (vsync_res->is_first)
		ctrl->r_info.sof_ts_ns = irq_info->ts_ns;

	if (vsync_res->is_last) {
		ctrl->r_info.inner_seq_no =
			ctrl->r_info.tmp_inner_seq_no;
	}

	spin_unlock(&ctrl->info_lock);
}

static void handle_engine_frame_start(struct mtk_cam_ctrl *ctrl,
				      struct mtk_camsys_irq_info *irq_info,
				      bool is_first, bool is_last)
{

	if (is_first) {
		int frame_sync_no;

		if (CAM_DEBUG_ENABLED(CTRL))
			pr_info("%s: first vsync\n", __func__);

		frame_sync_no = seq_from_fh_cookie(irq_info->frame_idx_inner);

		mtk_cam_event_frame_sync(ctrl, frame_sync_no);
	}

	if (is_last) {
		if (CAM_DEBUG_ENABLED(CTRL))
			pr_info("%s: last vsync\n", __func__);
		mtk_cam_ctrl_send_event(ctrl, CAMSYS_EVENT_IRQ_L_SOF);
	}
}

static int mtk_cam_event_handle_raw(struct mtk_cam_ctrl *ctrl,
				       unsigned int engine_id,
				       struct mtk_camsys_irq_info *irq_info)
{

	MTK_CAM_TRACE_FUNC_BEGIN(BASIC);

	/* raw's CQ done */
	if (irq_info->irq_type & BIT(CAMSYS_IRQ_SETTING_DONE)) {
		spin_lock(&ctrl->info_lock);
		ctrl->r_info.outer_seq_no =
			seq_from_fh_cookie(irq_info->frame_idx);
		spin_unlock(&ctrl->info_lock);
		handle_setting_done(ctrl);
	}

	/* raw's DMA done, we only allow AFO done here */
	if (irq_info->irq_type & BIT(CAMSYS_IRQ_AFO_DONE))
		handle_meta1_done(ctrl,
				  seq_from_fh_cookie(irq_info->cookie_done));

	/* raw's SW done */
	if (irq_info->irq_type & BIT(CAMSYS_IRQ_FRAME_DONE))
		handle_frame_done(ctrl,
				  CAMSYS_ENGINE_RAW, engine_id,
				  seq_from_fh_cookie(irq_info->cookie_done));

	/* raw's subsample n-2 vsync coming */
	if (irq_info->irq_type & BIT(CAMSYS_IRQ_TRY_SENSOR_SET))
		handle_ss_try_set_sensor(ctrl);

	/* raw's SOF (proc engine frame start) */
	if (irq_info->irq_type & BIT(CAMSYS_IRQ_FRAME_START)) {
		struct vsync_result vsync_res;

		ctrl_vsync_preprocess(ctrl,
				      CAMSYS_ENGINE_RAW, engine_id, irq_info,
				      &vsync_res);

		handle_engine_frame_start(ctrl, irq_info,
					  vsync_res.is_first,
					  vsync_res.is_last);
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

	/* camsv's CQ done */
	if (irq_info->irq_type & BIT(CAMSYS_IRQ_SETTING_DONE)) {
		spin_lock(&ctrl->info_lock);
		ctrl->r_info.outer_seq_no =
			seq_from_fh_cookie(irq_info->frame_idx);
		spin_unlock(&ctrl->info_lock);
		handle_setting_done(ctrl);
	}

	/* camsv's SW done */
	if (irq_info->irq_type & BIT(CAMSYS_IRQ_FRAME_DONE))
		handle_frame_done(ctrl,
				  CAMSYS_ENGINE_CAMSV, engine_id,
				  seq_from_fh_cookie(irq_info->cookie_done));

	/* camsv's SOF (proc engine frame start) */
	if (irq_info->irq_type & BIT(CAMSYS_IRQ_FRAME_START)) {
		struct vsync_result vsync_res;

		ctrl_vsync_preprocess(ctrl,
				      CAMSYS_ENGINE_CAMSV, engine_id, irq_info,
				      &vsync_res);

		handle_engine_frame_start(ctrl, irq_info,
					  vsync_res.is_first,
					  vsync_res.is_last);
	}

	return 0;
}

static int mtk_camsys_event_handle_mraw(struct mtk_cam_ctrl *ctrl,
					unsigned int engine_id,
					struct mtk_camsys_irq_info *irq_info)
{

	/* mraw's CQ done */
	if (irq_info->irq_type & BIT(CAMSYS_IRQ_SETTING_DONE)) {
		spin_lock(&ctrl->info_lock);
		ctrl->r_info.outer_seq_no =
			seq_from_fh_cookie(irq_info->frame_idx);
		spin_unlock(&ctrl->info_lock);
		handle_setting_done(ctrl);
	}

	/* mraw's SW done */
	if (irq_info->irq_type & BIT(CAMSYS_IRQ_FRAME_DONE))
		handle_frame_done(ctrl,
				  CAMSYS_ENGINE_MRAW, engine_id,
				  seq_from_fh_cookie(irq_info->cookie_done));

	/* mraw's SOF (proc engine frame start) */
	if (irq_info->irq_type & BIT(CAMSYS_IRQ_FRAME_START)) {
		struct vsync_result vsync_res;

		ctrl_vsync_preprocess(ctrl,
				      CAMSYS_ENGINE_MRAW, engine_id, irq_info,
				      &vsync_res);

		handle_engine_frame_start(ctrl, irq_info,
					  vsync_res.is_first,
					  vsync_res.is_last);
	}
	return 0;
}

int mtk_cam_ctrl_isr_event(struct mtk_cam_device *cam,
			 enum MTK_CAMSYS_ENGINE_TYPE engine_type,
			 unsigned int engine_id,
			 struct mtk_camsys_irq_info *irq_info)
{
	unsigned int ctx_id = ctx_from_fh_cookie(irq_info->frame_idx);
	struct mtk_cam_ctrl *cam_ctrl = &cam->ctxs[ctx_id].cam_ctrl;
	int ret = 0;

	if (mtk_cam_ctrl_get(cam_ctrl))
		return 0;

	/* TBC
	 *  MTK_CAM_TRACE_BEGIN(BASIC, "irq_type %d, inner %d",
	 *  irq_info->irq_type, irq_info->frame_idx_inner);
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
		ret = mtk_cam_event_handle_raw(cam_ctrl, engine_id, irq_info);
		break;
	case CAMSYS_ENGINE_MRAW:
		ret = mtk_camsys_event_handle_mraw(cam_ctrl, engine_id, irq_info);
		break;
	case CAMSYS_ENGINE_CAMSV:
		ret = mtk_camsys_event_handle_camsv(cam_ctrl, engine_id, irq_info);
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

	mtk_cam_ctrl_put(cam_ctrl);

	/* TBC
	 * MTK_CAM_TRACE_END(BASIC);
	 */
	return ret;
}

static u64 query_interval_from_sensor(struct v4l2_subdev *sensor)
{
	struct v4l2_subdev_frame_interval fi; /* in seconds */
	u64 frame_interval_ns;

	if (!sensor) {
		pr_info("%s: warn. without sensor\n", __func__);
		return 0;
	}

	fi.pad = 0;
	v4l2_subdev_call(sensor, video, g_frame_interval, &fi);

	if (fi.interval.denominator)
		frame_interval_ns = (fi.interval.numerator * 1000000000ULL) /
			fi.interval.denominator;
	else {
		pr_info("%s: warn. wrong fi (%u/%u)\n", __func__,
			fi.interval.numerator,
			fi.interval.denominator);
		frame_interval_ns = 1000000000ULL / 30ULL;
	}

	pr_info("%s: fi %llu ns\n", __func__, frame_interval_ns);
	return frame_interval_ns;
}

static void mtk_cam_ctrl_stream_on_work(struct work_struct *work)
{
	struct mtk_cam_ctrl *ctrl =
		container_of(work, struct mtk_cam_ctrl, stream_on_work);
	struct mtk_cam_job *job;
	struct mtk_cam_ctx *ctx = ctrl->ctx;
	struct device *dev = ctx->cam->dev;
	unsigned long timeout = msecs_to_jiffies(1000);
	int next_job_no;

	dev_info(dev, "[%s] ctx %d begin\n", __func__, ctrl->ctx->stream_id);

	job = mtk_cam_ctrl_get_job(ctrl, cond_first_job, 0);
	if (!job)
		return;

	if (!wait_for_completion_timeout(&job->compose_completion, timeout)) {
		pr_info("[%s] error: wait for job composed timeout\n",
			__func__);
		return;
	}

	mtk_cam_job_state_set(&job->job_state, SENSOR_STATE, S_SENSOR_APPLYING);
	call_jobop(job, apply_sensor);

	mtk_cam_job_state_set(&job->job_state, ISP_STATE, S_ISP_APPLYING);
	call_jobop(job, apply_isp);

	if (!wait_for_completion_timeout(&job->cq_exe_completion, timeout)) {
		pr_info("[%s] error: wait for job cq exe\n",
			__func__);
		return;
	}

	ctrl->s_params.i2c_thres_ns =
		infer_i2c_deadline_ns(&job->job_scen,
				      query_interval_from_sensor(ctx->sensor));
	dev_info(dev, "%s: i2c thres %llu\n",
		 __func__, ctrl->s_params.i2c_thres_ns);

	/* should set ts for second job's apply_sensor */
	ctrl->r_info.sof_ts_ns = ktime_get_boottime_ns();

	call_jobop(job, stream_on, true);

	next_job_no = job->frame_seq_no + 1;

	job = mtk_cam_ctrl_get_job(ctrl, cond_job_no_eq, &next_job_no);
	if (job)
		call_jobop(job, apply_sensor);

	mtk_cam_ctrl_apply_by_state(ctrl, 1);

	dev_info(dev, "[%s] ctx %d finish\n", __func__, ctrl->ctx->stream_id);
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

	mtk_cam_job_set_no(job, next_frame_seq);

	// to be removed
	if (next_frame_seq == 1) {
		vsync_set_desired(&cam_ctrl->vsync_col,
				  _get_master_engines(job->used_engine));

		/* TODO(AY): refine this */
		if (job->job_scen.id == MTK_CAM_SCEN_M2M_NORMAL ||
		    job->job_scen.id == MTK_CAM_SCEN_ODT_NORMAL)
			mtk_cam_ctrl_apply_by_state(cam_ctrl, 1);
	}


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
			       unsigned int fh_cookie,
			       struct mtkcam_ipi_frame_ack_result *cq_ret)
{
	struct mtk_cam_job *job_composed;
	struct mtk_cam_device *cam;
	int ctx_id, seq;

	if (mtk_cam_ctrl_get(cam_ctrl))
		return;

	cam = cam_ctrl->ctx->cam;
	ctx_id = ctx_from_fh_cookie(fh_cookie);
	seq = seq_from_fh_cookie(fh_cookie);

	job_composed = mtk_cam_ctrl_get_job(cam_ctrl, cond_job_no_eq, &seq);

	if (WARN_ON(!job_composed)) {
		dev_info(cam->dev, "%s: failed to find job ctx_id/frame = %d/%d\n",
			 __func__, ctx_id, seq);
		goto PUT_CTRL;
	}

	call_jobop(job_composed, compose_done, cq_ret);

	spin_lock(&cam_ctrl->info_lock);
	cam_ctrl->r_info.ack_seq_no = seq;
	spin_unlock(&cam_ctrl->info_lock);

	mtk_cam_ctrl_send_event(cam_ctrl, CAMSYS_EVENT_ACK);

PUT_CTRL:
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

	spin_lock_init(&cam_ctrl->send_lock);
	rwlock_init(&cam_ctrl->list_lock);
	INIT_LIST_HEAD(&cam_ctrl->camsys_state_list);

	spin_lock_init(&cam_ctrl->info_lock);
	reset_runtime_info(&cam_ctrl->r_info);

	init_waitqueue_head(&cam_ctrl->stop_wq);

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
	for (i = 0; i < ARRAY_SIZE(ctx->hw_raw); i++) {
		if (ctx->hw_raw[i]) {
			raw_dev = dev_get_drvdata(ctx->hw_raw[i]);
			disable_irq(raw_dev->irq);
		}
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
	for (i = 0; i < ARRAY_SIZE(ctx->hw_raw); i++) {
		if (ctx->hw_raw[i]) {
			raw_dev = dev_get_drvdata(ctx->hw_raw[i]);
			reset(raw_dev);
		}
	}
	if (ctx->hw_sv)
		sv_reset(sv_dev);
	for (i = 0; i < ARRAY_SIZE(ctx->hw_mraw); i++) {
		if (ctx->hw_mraw[i]) {
			mraw_dev = dev_get_drvdata(ctx->hw_mraw[i]);
			mraw_reset(mraw_dev);
		}
	}
	mtk_cam_event_eos(cam_ctrl);

	read_lock(&cam_ctrl->list_lock);
	list_for_each_entry(job_s, &cam_ctrl->camsys_state_list, list) {
		job = container_of(job_s, struct mtk_cam_job, job_state);

		mtk_cam_job_mark_cancelled(job);
	}
	read_unlock(&cam_ctrl->list_lock);

	drain_workqueue(ctx->frame_done_wq);


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
		 * _on_job_last_ref
		 */
		list_del(&job->job_state.list);
	}
	write_unlock(&cam_ctrl->list_lock);


	dev_info(ctx->cam->dev, "[%s] ctx:%d, stop status:%d\n",
		__func__, ctx->stream_id, atomic_read(&cam_ctrl->stopped));
}

void vsync_update(struct vsync_collector *c,
		  int engine_type, int idx,
		  struct vsync_result *res)
{
	unsigned int coming;

	if (!res)
		return;

	coming = engine_idx_to_bit(engine_type, idx);

	c->collected |= (coming & c->desired);

	if (CAM_DEBUG_ENABLED(CTRL))
		pr_info("%s: vsync desired/collected %x/%x\n",
			__func__, c->desired, c->collected);

	res->is_first = !(c->collected & (c->collected - 1));
	res->is_last = c->collected == c->desired;

	if (res->is_last)
		c->collected = 0;
}
