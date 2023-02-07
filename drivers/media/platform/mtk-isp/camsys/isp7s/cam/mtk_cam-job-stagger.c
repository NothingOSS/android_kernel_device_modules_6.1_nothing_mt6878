// SPDX-License-Identifier: GPL-2.0
//
// Copyright (c) 2019 MediaTek Inc.

#include "mtk_cam.h"
#include "mtk_cam-job-stagger.h"
#include "mtk_cam-job_utils.h"

static bool
is_stagger_2_exposure(struct mtk_cam_scen *scen)
{
	return scen->scen.normal.exp_num == 2;
}

static bool
is_stagger_3_exposure(struct mtk_cam_scen *scen)
{
	return scen->scen.normal.exp_num == 3;
}
bool
is_stagger_multi_exposure(struct mtk_cam_job *job)
{
	return job->job_scen.scen.normal.exp_num > 1;
}

/* FIXME(AY): this function has no guarantee that job is in type of stagger_job */
static bool
is_stagger_dc(struct mtk_cam_job *job)
{
	struct mtk_cam_stagger_job *stagger_job =
		(struct mtk_cam_stagger_job *)job;
	bool ret = false;

	if (stagger_job->is_dc_stagger)
		ret = true;

	return ret;
}

int get_first_sv_tag_idx(unsigned int exp_no, bool is_w)
{
	struct mtk_camsv_tag_param img_tag_param[SVTAG_IMG_END];
	unsigned int hw_scen, req_amount;
	int i, tag_idx = -1;

	hw_scen = 1 << HWPATH_ID(MTKCAM_IPI_HW_PATH_STAGGER);
	req_amount = (exp_no < 3) ? exp_no * 2 : exp_no;
	if (mtk_cam_sv_get_tag_param(img_tag_param, hw_scen, exp_no, req_amount))
		goto EXIT;
	else {
		for (i = 0; i < req_amount; i++) {
			if (img_tag_param[i].tag_order == MTKCAM_IPI_ORDER_FIRST_TAG &&
				img_tag_param[i].is_w == is_w) {
				tag_idx = img_tag_param[i].tag_idx;
				break;
			}
		}
	}

EXIT:
	return tag_idx;
}

int get_second_sv_tag_idx(unsigned int exp_no, bool is_w)
{
	struct mtk_camsv_tag_param img_tag_param[SVTAG_IMG_END];
	unsigned int hw_scen, req_amount;
	int i, tag_idx = -1;

	hw_scen = 1 << HWPATH_ID(MTKCAM_IPI_HW_PATH_STAGGER);
	req_amount = (exp_no < 3) ? exp_no * 2 : exp_no;
	if (mtk_cam_sv_get_tag_param(img_tag_param, hw_scen, exp_no, req_amount))
		goto EXIT;
	else {
		for (i = 0; i < req_amount; i++) {
			if (img_tag_param[i].tag_order == MTKCAM_IPI_ORDER_NORMAL_TAG &&
				img_tag_param[i].is_w == is_w) {
				tag_idx = img_tag_param[i].tag_idx;
				break;
			}
		}
	}

EXIT:
	return tag_idx;
}

int get_last_sv_tag_idx(unsigned int exp_no, bool is_w)
{
	struct mtk_camsv_tag_param img_tag_param[SVTAG_IMG_END];
	unsigned int hw_scen, req_amount;
	int i, tag_idx = -1;

	hw_scen = 1 << HWPATH_ID(MTKCAM_IPI_HW_PATH_STAGGER);
	req_amount = (exp_no < 3) ? exp_no * 2 : exp_no;
	if (mtk_cam_sv_get_tag_param(img_tag_param, hw_scen, exp_no, req_amount))
		goto EXIT;
	else {
		for (i = 0; i < req_amount; i++) {
			if (img_tag_param[i].tag_order == MTKCAM_IPI_ORDER_LAST_TAG &&
				img_tag_param[i].is_w == is_w) {
				tag_idx = img_tag_param[i].tag_idx;
				break;
			}
		}
	}

EXIT:
	return tag_idx;
}

int get_hard_scenario_stagger(struct mtk_cam_job *job)
{
	struct mtk_cam_scen *scen = &job->job_scen;
	int isDC = is_stagger_dc(job);
	int hard_scenario;

	if (is_stagger_2_exposure(scen))
		hard_scenario = isDC ?
				MTKCAM_IPI_HW_PATH_DC_STAGGER :
				MTKCAM_IPI_HW_PATH_STAGGER;
	else if (is_stagger_3_exposure(scen))
		hard_scenario = isDC ?
				MTKCAM_IPI_HW_PATH_DC_STAGGER :
				MTKCAM_IPI_HW_PATH_STAGGER;
	else
		hard_scenario = isDC ?
				MTKCAM_IPI_HW_PATH_DC :
				MTKCAM_IPI_HW_PATH_ON_THE_FLY;

	return hard_scenario;
}

int fill_imgo_img_buffer_to_ipi_frame_stagger(
	struct req_buffer_helper *helper, struct mtk_cam_buffer *buf,
	struct mtk_cam_video_device *node)
{
	struct mtkcam_ipi_frame_param *fp = helper->fp;
	struct mtkcam_ipi_img_output *out;
	struct mtkcam_ipi_img_input *in;
	int isneedrawi = is_stagger_multi_exposure(helper->job);
	int ret = -1;

	out = &fp->img_outs[helper->io_idx];
	++helper->io_idx;
	if (isneedrawi) {
		ret = fill_img_out_hdr(out, buf, node, 1); /* TODO: by exp-order */
		in = &fp->img_ins[helper->ii_idx];
		++helper->ii_idx;
		ret = fill_img_in_hdr(in, buf, node);

		helper->filled_hdr_buffer = true;
	} else {
		ret = fill_img_out(out, buf, node);
	}

	/* fill sv image fp */
	ret = fill_sv_img_fp(helper, buf, node);

	return ret;

}

int fill_sv_fp(
	struct req_buffer_helper *helper, struct mtk_cam_buffer *buf,
	struct mtk_cam_video_device *node, unsigned int tag_idx,
	unsigned int pipe_id, unsigned int buf_ofset)
{
	struct mtkcam_ipi_frame_param *fp = helper->fp;
	struct mtkcam_ipi_img_output *out =
		&fp->camsv_param[0][tag_idx].camsv_img_outputs[0];
	int ret = -1;

	ret = fill_img_out(out, buf, node);

	fp->camsv_param[0][tag_idx].pipe_id = pipe_id;
	fp->camsv_param[0][tag_idx].tag_id = tag_idx;
	fp->camsv_param[0][tag_idx].hardware_scenario = 0;
	out->uid.id = MTKCAM_IPI_CAMSV_MAIN_OUT;
	out->uid.pipe_id = pipe_id;
	out->buf[0][tag_idx].iova = buf->daddr + buf_ofset;

	return ret;
}

int fill_sv_img_fp(
	struct req_buffer_helper *helper, struct mtk_cam_buffer *buf,
	struct mtk_cam_video_device *node)
{
	struct mtk_cam_job *job = helper->job;
	struct mtk_cam_ctx *ctx = job->src_ctx;
	struct mtk_cam_scen *scen = &job->job_scen;
	struct mtk_camsv_device *sv_dev;
	int (*func_ptr_arr[3])(unsigned int, bool);
	unsigned int pipe_id, exp_no, buf_ofset;
	int tag_idx, i;
	int ret = -1;

	if (ctx->hw_sv == NULL)
		goto EXIT;

	sv_dev = dev_get_drvdata(ctx->hw_sv);
	pipe_id = sv_dev->id + MTKCAM_SUBDEV_CAMSV_START;

	func_ptr_arr[0] = get_first_sv_tag_idx;
	func_ptr_arr[1] = get_second_sv_tag_idx;
	func_ptr_arr[2] = get_last_sv_tag_idx;

	if (is_stagger_2_exposure(scen))
		exp_no = 2;
	else if (is_stagger_3_exposure(scen))
		exp_no = 3;
	else
		exp_no = 1;

	for(i = 0; i < exp_no; i++) {
		/* remove this check for supporting pure raw dump */
		if (!is_stagger_dc(job) && (i + 1) == exp_no)
			continue;
		tag_idx = (is_stagger_dc(job) && exp_no > 1 && (i + 1) == exp_no) ?
			(*func_ptr_arr[2])(exp_no, false) :
			(*func_ptr_arr[i])(exp_no, false);
		if (tag_idx == -1) {
			pr_info("%s: tag_idx not found(exp_no:%d)", __func__, exp_no);
			goto EXIT;
		}
		buf_ofset = buf->image_info.size[0] * i;
		ret = fill_sv_fp(helper, buf, node, tag_idx, pipe_id, buf_ofset);
	}

EXIT:
	return ret;
}

int get_switch_type_stagger(struct mtk_cam_job *job)
{
	struct mtk_cam_stagger_job *stagger_job =
			(struct mtk_cam_stagger_job *)job;

	int cur = job->job_scen.scen.normal.exp_num;
	int prev = stagger_job->prev_scen.scen.normal.exp_num;
	int res = EXPOSURE_CHANGE_NONE;

	if (cur == prev)
		return EXPOSURE_CHANGE_NONE;
	if (prev == 3) {
		if (cur == 1)
			res = EXPOSURE_CHANGE_3_to_1;
		else if (cur == 2)
			res = EXPOSURE_CHANGE_3_to_2;
	} else if (prev == 2) {
		if (cur == 1)
			res = EXPOSURE_CHANGE_2_to_1;
		else if (cur == 3)
			res = EXPOSURE_CHANGE_2_to_3;
	} else if (prev == 1)  {
		if (cur == 2)
			res = EXPOSURE_CHANGE_1_to_2;
		else if (cur == 3)
			res = EXPOSURE_CHANGE_1_to_3;
	}
	pr_info("[%s] switch_type:%d (cur:%d prev:%d)",
			__func__, res, cur, prev);

	return res;
}

void update_stagger_job_exp(struct mtk_cam_job *job)
{
	struct mtk_cam_stagger_job *stagger_job =
			(struct mtk_cam_stagger_job *)job;
	struct mtk_cam_scen *scen = &job->job_scen;

	job->exp_num_cur = scen->scen.normal.exp_num;

	switch (stagger_job->switch_type) {
	case EXPOSURE_CHANGE_NONE:
		job->exp_num_prev = job->exp_num_cur;
		break;
	case EXPOSURE_CHANGE_3_to_2:
	case EXPOSURE_CHANGE_3_to_1:
		job->exp_num_prev = 3;
		break;
	case EXPOSURE_CHANGE_2_to_1:
	case EXPOSURE_CHANGE_2_to_3:
		job->exp_num_prev = 2;
		break;
	case EXPOSURE_CHANGE_1_to_2:
	case EXPOSURE_CHANGE_1_to_3:
		job->exp_num_prev = 1;
		break;
	default:
		break;
	}
	//pr_info("[%s] prev:%d-exp -> cur:%d-exp\n",
	//	__func__, job->feature->exp_num_prev, job->feature->exp_num_cur);
}
#if 0
void update_event_setting_done_stagger(struct mtk_cam_job *job,
		struct mtk_cam_job_event_info *event_info, int *action)
{
	struct mtk_cam_ctx *ctx = job->src_ctx;
	struct mtk_cam_device *cam = ctx->cam;
	struct mtk_raw_device *raw_dev =
		dev_get_drvdata(cam->engines.raw_devs[job->proc_engine & 0xF]);
	struct mtk_cam_stagger_job *stagger_job =
			(struct mtk_cam_stagger_job *)job;
	unsigned int frame_seq_no_outer = event_info->frame_idx;
	int type;

	if ((job->frame_seq_no == frame_seq_no_outer) &&
		((frame_seq_no_outer - event_info->isp_request_seq_no) > 0)) {
		/**
		 * outer number is 1 more from last SOF's
		 * inner number
		 */
		if (frame_seq_no_outer == 1) {
			job->state = E_STATE_OUTER;
			*action |= BIT(CAM_JOB_STREAM_ON);
		}
		_state_trans(job, E_STATE_CQ, E_STATE_OUTER);
		_state_trans(job, E_STATE_CQ_SCQ_DELAY, E_STATE_OUTER);
		_state_trans(job, E_STATE_SENINF, E_STATE_OUTER);
		type = stagger_job->switch_feature_type;
		if (type) {
			if (type == EXPOSURE_CHANGE_3_to_1 ||
				type == EXPOSURE_CHANGE_2_to_1) {
				stagger_disable(raw_dev);
				stagger_job->dcif_enable = 0;
			} else if (type == EXPOSURE_CHANGE_1_to_2 ||
				type == EXPOSURE_CHANGE_1_to_3) {
				stagger_enable(raw_dev);
				stagger_job->dcif_enable = 1;
			}
			dbload_force(raw_dev);
			dev_dbg(raw_dev->dev,
				"[CQD-switch] req:%d type:%d\n",
				job->frame_seq_no, type);
		}
		dev_info(raw_dev->dev,
			"[%s] req:%d, CQ->OUTER state:%d\n", __func__,
			job->frame_seq_no, job->state);
		// TBC - mtk_cam_handle_seamless_switch(job);
		// TBC - mtk_cam_handle_mux_switch(raw_dev, ctx, job->req);
	}
}

void update_event_sensor_try_set_stagger(struct mtk_cam_job *job,
		struct mtk_cam_job_event_info *event_info, int *action)
{
	struct mtk_cam_ctx *ctx = job->src_ctx;
	struct mtk_cam_stagger_job *stagger_job =
			(struct mtk_cam_stagger_job *)job;
	int cur_sen_seq_no = event_info->frame_idx_inner;
	u64 aftersof_ms = (ktime_get_boottime_ns() - event_info->ts_ns) / 1000000;

	if (job->frame_seq_no <= 2) {
		dev_info(ctx->cam->dev,
				 "[%s] initial setup sensor job:%d cur/next:%d/%d\n",
			__func__, job->frame_seq_no, event_info->frame_idx_inner,
			event_info->frame_idx);
		if (job->frame_seq_no == cur_sen_seq_no + 1) {
			*action |= BIT(CAM_JOB_APPLY_SENSOR);
			return;
		}
	}

	if (job->frame_seq_no == cur_sen_seq_no - 1) {
		if (job->state < E_STATE_INNER) {
			dev_info(ctx->cam->dev,
				 "[%s] req:%d isn't arrive inner (sen_seq_no:%d)\n",
				 __func__, job->frame_seq_no, cur_sen_seq_no);
			*action = BIT(CAM_JOB_HW_DELAY);
			return;
		}
	}
	if (job->frame_seq_no == cur_sen_seq_no) {
		if (job->state == E_STATE_CAMMUX_OUTER_CFG) {
			job->state = E_STATE_CAMMUX_OUTER_CFG_DELAY;
			dev_info(ctx->cam->dev,
				"[%s] CAMMUX OUTTER CFG DELAY STATE\n", __func__);
			*action = BIT(CAM_JOB_SENSOR_DELAY);
			return;
		} else if (job->state <= E_STATE_SENSOR) {
			dev_info(ctx->cam->dev,
				 "[%s] wrong state:%d (sensor delay)\n",
				 __func__, job->state);
			*action = BIT(CAM_JOB_SENSOR_DELAY);
			return;
		}
	}
	if (job->frame_seq_no == cur_sen_seq_no + 1) {
		if (aftersof_ms > job->sensor_set_margin) {
			dev_info(ctx->cam->dev,
				 "[%s] req:%d over setting margin (%d>%d)\n",
				 __func__, job->frame_seq_no, aftersof_ms,
				 job->sensor_set_margin);
			*action = 0;
			return;
		}
		if (*action & BIT(CAM_JOB_HW_DELAY) ||
			*action & BIT(CAM_JOB_CQ_DELAY) ||
			*action & BIT(CAM_JOB_SENSOR_DELAY))
			return;
		if (stagger_job->switch_feature_type && job->frame_seq_no > 1) {
			dev_info(ctx->cam->dev,
				 "[%s] switch type:%d request:%d - pass sensor\n",
				 __func__, stagger_job->switch_feature_type,
				 job->frame_seq_no);
			*action |= BIT(CAM_JOB_SENSOR_EXPNUM_CHANGE);
			return;
		}

		*action |= BIT(CAM_JOB_APPLY_SENSOR);
	}
	if (job->frame_seq_no > cur_sen_seq_no + 1)
		*action = 0;
}

static void
_update_event_frame_start_stagger(struct mtk_cam_job *job,
		struct mtk_cam_job_event_info *event_info, int *action)
{
	struct mtk_cam_stagger_job *stagger_job =
			(struct mtk_cam_stagger_job *)job;
	struct mtk_cam_ctx *ctx = job->src_ctx;
	int frame_idx_inner = event_info->frame_idx_inner;
	int write_cnt_offset, write_cnt;
	u64 time_boot = event_info->ts_ns;
	u64 time_mono = ktime_get_ns();
	int switch_type = stagger_job->switch_feature_type;

	if (job->state == E_STATE_INNER ||
		job->state == E_STATE_INNER_HW_DELAY) {
		write_cnt_offset = event_info->reset_seq_no - 1;
		write_cnt = ((event_info->isp_request_seq_no - write_cnt_offset) / 256)
					* 256 + event_info->write_cnt;
		/* job - should be dequeued or re-reading out */
		if (frame_idx_inner > event_info->isp_request_seq_no ||
			atomic_read(&job->frame_done_work.is_queued) == 1) {
			dev_info_ratelimited(ctx->cam->dev,
				"[SOF] frame done work delay, req(%d),ts(%lu)\n",
				job->frame_seq_no, event_info->ts_ns / 1000);
		} else if (write_cnt >= job->frame_seq_no - write_cnt_offset) {
			dev_info_ratelimited(ctx->cam->dev,
				"[SOF] frame done sw reading lost %d frames, req(%d),ts(%lu)\n",
				write_cnt - (job->frame_seq_no - write_cnt_offset) + 1,
				job->frame_seq_no, event_info->ts_ns / 1000);
			_set_timestamp(job, time_boot - 1000, time_mono - 1000);
		} else if ((write_cnt >= job->frame_seq_no - write_cnt_offset - 1)
			&& event_info->fbc_cnt == 0) {
			dev_info_ratelimited(ctx->cam->dev,
				"[SOF] frame done sw reading lost frames, req(%d),ts(%lu)\n",
				job->frame_seq_no, event_info->ts_ns / 1000);
			_set_timestamp(job, time_boot - 1000, time_mono - 1000);
		} else {
			_state_trans(job, E_STATE_INNER, E_STATE_INNER_HW_DELAY);
			dev_info_ratelimited(ctx->cam->dev,
				"[SOF] HW_IMCOMPLETE state cnt(%d,%d),req(%d),ts(%lu)\n",
				write_cnt, event_info->write_cnt, job->frame_seq_no,
				event_info->ts_ns / 1000);
			*action |= BIT(CAM_JOB_HW_DELAY);
		}
	} else if (job->state == E_STATE_CQ ||
		job->state == E_STATE_OUTER ||
		job->state == E_STATE_CAMMUX_OUTER ||
		job->state == E_STATE_OUTER_HW_DELAY) {
		/* job - reading out */
		_set_timestamp(job, time_boot, time_mono);
		if (*action & BIT(CAM_JOB_HW_DELAY)) {
			_state_trans(job, E_STATE_OUTER,
			 E_STATE_OUTER_HW_DELAY);
			_state_trans(job, E_STATE_CAMMUX_OUTER,
			 E_STATE_OUTER_HW_DELAY);
			return;
		}
		if (job->frame_seq_no > frame_idx_inner) {
			dev_info(ctx->cam->dev,
				"[SOF-noDBLOAD] outer_no:%d, inner_idx:%d <= processing_idx:%d,ts:%lu\n",
				job->frame_seq_no, frame_idx_inner, event_info->isp_request_seq_no,
				event_info->ts_ns / 1000);
			*action |= BIT(CAM_JOB_CQ_DELAY);
			return;
		}

		if (job->frame_seq_no == frame_idx_inner) {
			if (frame_idx_inner > event_info->isp_request_seq_no) {
				_state_trans(job, E_STATE_OUTER_HW_DELAY,
						 E_STATE_INNER_HW_DELAY);
				_state_trans(job, E_STATE_OUTER, E_STATE_INNER);
				_state_trans(job, E_STATE_CAMMUX_OUTER,
						 E_STATE_INNER);
				*action |= BIT(CAM_JOB_READ_DEQ_NO);
				dev_dbg(ctx->cam->dev,
					"[SOF-DBLOAD][%s] frame_seq_no:%d, OUTER->INNER state:%d,ts:%lu\n",
					__func__, job->frame_seq_no, job->state,
					event_info->ts_ns / 1000);
			}
		}
		if (job->frame_seq_no == 1)
			_state_trans(job, E_STATE_SENSOR, E_STATE_INNER);

	} else if (job->state == E_STATE_SENSOR ||
		job->state == E_STATE_SENINF) {
		if (*action & BIT(CAM_JOB_HW_DELAY) ||
			*action & BIT(CAM_JOB_CQ_DELAY))
			return;
		/* job - to be set */
		if (job->state == E_STATE_SENINF) {
			dev_info(ctx->cam->dev, "[SOF] sensor switch delay\n");
			*action |= BIT(CAM_JOB_SENSOR_DELAY);
		} else if (job->state == E_STATE_SENSOR) {
			*action |= BIT(CAM_JOB_APPLY_CQ);
		}

	} else if (job->state == E_STATE_READY) {
		if (*action & BIT(CAM_JOB_HW_DELAY) ||
			*action & BIT(CAM_JOB_CQ_DELAY))
			return;
		if (switch_type && job->frame_seq_no > 1 &&
			job->frame_seq_no == frame_idx_inner + 1) {
			*action |= BIT(CAM_JOB_EXP_NUM_SWITCH);
			*action |= BIT(CAM_JOB_APPLY_CQ);
			_state_trans(job, E_STATE_READY, E_STATE_SENSOR);
		} else {
			dev_info(ctx->cam->dev,
			"[%s] need check, req:%d, state:%d\n", __func__,
			job->frame_seq_no, job->state);
			*action = 0;
		}
	}

}

static void
_update_event_sensor_vsync_stagger(struct mtk_cam_job *job,
	struct mtk_cam_job_event_info *event_info, int *action)
{
	unsigned int frame_seq_no_inner = event_info->frame_idx_inner;
#ifdef NOT_READY
	/* touch watchdog*/
	if (watchdog_scenario(ctx))
		mtk_ctx_watchdog_kick(ctx);
#endif
	if (frame_seq_no_inner == job->frame_seq_no) {
		*action |= BIT(CAM_JOB_VSYNC);
		if ((*action & BIT(CAM_JOB_HW_DELAY)) == 0)
			*action |= BIT(CAM_JOB_SETUP_TIMER);
	} else {
		*action &= ~BIT(CAM_JOB_VSYNC);
	}
}

void update_frame_start_event_stagger(struct mtk_cam_job *job,
		struct mtk_cam_job_event_info *event_info, int *action)
{
	struct mtk_cam_stagger_job *stagger_job =
			(struct mtk_cam_stagger_job *)job;
	struct mtk_cam_device *cam = job->src_ctx->cam;
	int engine_type = (event_info->engine >> 8) & 0xFF;

	if (stagger_job->dcif_enable) {
		if (engine_type == CAMSYS_ENGINE_CAMSV)
			_update_event_sensor_vsync_stagger(job, event_info, action);
		else if (engine_type == CAMSYS_ENGINE_RAW)
			_update_event_frame_start_stagger(job, event_info, action);
	} else {
		_update_event_frame_start_stagger(job, event_info, action);
		_update_event_sensor_vsync_stagger(job, event_info, action);
	}

	dev_dbg(cam->dev,
		"[%s] engine_type:%d, job:%d, out/in:%d/%d, ts:%lld, dc_en:%d, action:0x%x\n",
		__func__, engine_type, job->frame_seq_no, event_info->frame_idx,
		event_info->frame_idx_inner, event_info->ts_ns,
		stagger_job->dcif_enable, *action);
}
#endif
int wait_apply_sensor_stagger(struct mtk_cam_job *job)
{
	struct mtk_cam_stagger_job *stagger_job =
			(struct mtk_cam_stagger_job *)job;

	atomic_set(&stagger_job->expnum_change, 0);
	wait_event_interruptible(stagger_job->expnum_change_wq,
		atomic_read(&stagger_job->expnum_change) > 0);
	job->ops->apply_sensor(job);
	atomic_dec_return(&stagger_job->expnum_change);

	return 0;
}

int apply_cam_mux_stagger(struct mtk_cam_job *job)
{
	struct mtk_cam_stagger_job *stagger_job =
			(struct mtk_cam_stagger_job *)job;
	struct mtk_cam_ctx *ctx = job->src_ctx;
	struct mtk_camsv_device *sv_dev = dev_get_drvdata(ctx->hw_sv);
	struct mtk_cam_seninf_mux_param param;
	struct mtk_cam_seninf_mux_setting settings[3];
	int type = stagger_job->switch_type;
	int config_exposure_num = job->job_scen.scen.normal.max_exp_num;
	int is_dc = is_stagger_dc(job);
	int raw_id = _get_master_raw_id(ctx->cam->engines.num_raw_devices,
			job->used_engine);
	int raw_tg_idx = raw_id + GET_PLAT_V4L2(cammux_id_raw_start);
	int first_tag_idx, second_tag_idx, last_tag_idx;

	/**
	 * To identify the "max" exposure_num, we use
	 * feature_active, not job->feature.raw_feature
	 * since the latter one stores the exposure_num information,
	 * not the max one.
	 */

	if (type != EXPOSURE_CHANGE_NONE && config_exposure_num == 3) {
		switch (type) {
		case EXPOSURE_CHANGE_3_to_2:
		case EXPOSURE_CHANGE_1_to_2:
			first_tag_idx = get_first_sv_tag_idx(2, false);
			last_tag_idx = get_last_sv_tag_idx(2, false);
			settings[0].seninf = ctx->seninf;
			settings[0].source = PAD_SRC_RAW0;
			settings[0].camtg  =
				mtk_cam_get_sv_cammux_id(sv_dev, first_tag_idx);
			settings[0].tag_id = first_tag_idx;
			settings[0].enable = 1;

			settings[1].seninf = ctx->seninf;
			settings[1].source = PAD_SRC_RAW1;
			settings[1].camtg  = (is_dc) ?
				mtk_cam_get_sv_cammux_id(sv_dev, last_tag_idx) :
				raw_tg_idx;
			settings[1].tag_id = (is_dc) ? last_tag_idx : -1;
			settings[1].enable = 1;

			settings[2].seninf = ctx->seninf;
			settings[2].source = PAD_SRC_RAW2;
			settings[2].camtg  = -1;
			settings[2].tag_id = -1;
			settings[2].enable = 0;
			break;
		case EXPOSURE_CHANGE_3_to_1:
		case EXPOSURE_CHANGE_2_to_1:
			first_tag_idx = get_first_sv_tag_idx(1, false);
			settings[0].seninf = ctx->seninf;
			settings[0].source = PAD_SRC_RAW0;
			settings[0].camtg  = (is_dc) ?
				mtk_cam_get_sv_cammux_id(sv_dev, first_tag_idx) :
				raw_tg_idx;
			settings[0].tag_id = (is_dc) ? first_tag_idx : -1;
			settings[0].enable = 1;

			settings[1].seninf = ctx->seninf;
			settings[1].source = PAD_SRC_RAW1;
			settings[1].camtg  = -1;
			settings[1].tag_id = -1;
			settings[1].enable = 0;

			settings[2].seninf = ctx->seninf;
			settings[2].source = PAD_SRC_RAW2;
			settings[2].camtg  = -1;
			settings[2].tag_id = -1;
			settings[2].enable = 0;
			break;
		case EXPOSURE_CHANGE_2_to_3:
		case EXPOSURE_CHANGE_1_to_3:
			first_tag_idx = get_first_sv_tag_idx(3, false);
			second_tag_idx = get_second_sv_tag_idx(3, false);
			last_tag_idx = get_last_sv_tag_idx(3, false);
			settings[0].seninf = ctx->seninf;
			settings[0].source = PAD_SRC_RAW0;
			settings[0].camtg  =
				mtk_cam_get_sv_cammux_id(sv_dev, first_tag_idx);
			settings[0].tag_id = first_tag_idx;
			settings[0].enable = 1;

			settings[1].seninf = ctx->seninf;
			settings[1].source = PAD_SRC_RAW1;
			settings[1].camtg  =
				mtk_cam_get_sv_cammux_id(sv_dev, second_tag_idx);
			settings[1].tag_id = second_tag_idx;
			settings[1].enable = 1;

			settings[2].seninf = ctx->seninf;
			settings[2].source = PAD_SRC_RAW2;
			settings[2].camtg  = (is_dc) ?
				mtk_cam_get_sv_cammux_id(sv_dev, last_tag_idx) :
				raw_tg_idx;
			settings[2].tag_id = (is_dc) ? last_tag_idx : -1;
			settings[2].enable = 1;
			break;
		default:
			break;
		}
		param.settings = &settings[0];
		param.num = 3;
		mtk_cam_seninf_streaming_mux_change(&param);
		dev_info(ctx->cam->dev,
			"[%s] switch Req:%d, type:%d, cam_mux[0][1][2]:[%d/%d/%d][%d/%d/%d][%d/%d/%d]\n",
			__func__, job->frame_seq_no, type,
			settings[0].source, settings[0].camtg, settings[0].enable,
			settings[1].source, settings[1].camtg, settings[1].enable,
			settings[2].source, settings[2].camtg, settings[2].enable);
	} else if (type != EXPOSURE_CHANGE_NONE && config_exposure_num == 2) {
		switch (type) {
		case EXPOSURE_CHANGE_2_to_1:
			first_tag_idx = get_first_sv_tag_idx(1, false);
			settings[0].seninf = ctx->seninf;
			settings[0].source = PAD_SRC_RAW0;
			settings[0].camtg  = (is_dc) ?
				mtk_cam_get_sv_cammux_id(sv_dev, first_tag_idx) :
				raw_tg_idx;
			settings[0].tag_id = (is_dc) ? first_tag_idx : -1;
			settings[0].enable = 1;

			settings[1].seninf = ctx->seninf;
			settings[1].source = PAD_SRC_RAW1;
			settings[1].camtg  = -1;
			settings[1].tag_id = -1;
			settings[1].enable = 0;
			break;
		case EXPOSURE_CHANGE_1_to_2:
			first_tag_idx = get_first_sv_tag_idx(2, false);
			last_tag_idx = get_last_sv_tag_idx(2, false);
			settings[0].seninf = ctx->seninf;
			settings[0].source = PAD_SRC_RAW0;
			settings[0].camtg  =
				mtk_cam_get_sv_cammux_id(sv_dev, first_tag_idx);
			settings[0].tag_id = first_tag_idx;
			settings[0].enable = 1;

			settings[1].seninf = ctx->seninf;
			settings[1].source = PAD_SRC_RAW1;
			settings[1].camtg  = (is_dc) ?
				mtk_cam_get_sv_cammux_id(sv_dev, last_tag_idx) :
				raw_tg_idx;
			settings[1].tag_id = (is_dc) ? last_tag_idx : -1;
			settings[1].enable = 1;
			break;
		default:
			break;
		}
		param.settings = &settings[0];
		param.num = 2;
		mtk_cam_seninf_streaming_mux_change(&param);
		dev_info(ctx->cam->dev,
			"[%s] switch Req:%d, type:%d, cam_mux[0][1]:[%d/%d/%d][%d/%d/%d] ts:%llu\n",
			__func__, job->frame_seq_no, type,
			settings[0].source, settings[0].camtg, settings[0].enable,
			settings[1].source, settings[1].camtg, settings[1].enable,
			ktime_get_boottime_ns() / 1000);
	}

	return 0;
}

/* threaded irq context */
int wakeup_apply_sensor(struct mtk_cam_job *job)
{
	struct mtk_cam_stagger_job *stagger_job =
			(struct mtk_cam_stagger_job *)job;

	atomic_set(&stagger_job->expnum_change, 1);
	wake_up_interruptible(&stagger_job->expnum_change_wq);

	return 0;
}

int stream_on_otf_stagger(struct mtk_cam_job *job, bool on)
{
	struct mtk_cam_ctx *ctx = job->src_ctx;
	struct mtk_cam_device *cam = ctx->cam;
	int raw_id = _get_master_raw_id(cam->engines.num_raw_devices,
			job->used_engine);
	struct mtk_raw_device *raw_dev =
		dev_get_drvdata(cam->engines.raw_devs[raw_id]);
	struct mtk_camsv_device *sv_dev;
	struct mtk_mraw_device *mraw_dev;
	int seninf_pad, pixel_mode, tg_idx;
	int i;

#ifdef NOT_READY
	int scq_ms = SCQ_DEADLINE_MS * 3;

	stream_on(raw_dev, on, scq_ms, 0);
#else
	stream_on(raw_dev, on);
#endif
	pr_info("[%s] on:%d",
			__func__, on);
	if (ctx->hw_sv) {
		sv_dev = dev_get_drvdata(ctx->hw_sv);
		mtk_cam_sv_dev_stream_on(sv_dev, on);
	}

	for (i = 0; i < ctx->num_mraw_subdevs; i++) {
		if (ctx->hw_mraw[i]) {
			mraw_dev = dev_get_drvdata(ctx->hw_mraw[i]);
			mtk_cam_mraw_dev_stream_on(mraw_dev, on);
		}
	}

	if (job->stream_on_seninf) {
		if (job->job_scen.scen.normal.max_exp_num == 2)
			seninf_pad = PAD_SRC_RAW1;
		else if (job->job_scen.scen.normal.max_exp_num == 3)
			seninf_pad = PAD_SRC_RAW2;
		else
			seninf_pad = PAD_SRC_RAW0;
		pixel_mode = 3;
		tg_idx = raw_id + GET_PLAT_V4L2(cammux_id_raw_start);
		ctx_stream_on_seninf_sensor_hdr(job->src_ctx, on,
			seninf_pad, pixel_mode, tg_idx);
		/* exp. switch at first frame */
		// apply_cam_mux_stagger(job);
	}

	return 0;
}

int handle_sv_tag_hdr(struct mtk_cam_job *job)
{
	struct mtk_cam_ctx *ctx = job->src_ctx;
	struct mtk_raw_pipeline *raw_pipe;
	struct mtk_camsv_device *sv_dev;
	struct mtk_camsv_pipeline *sv_pipe;
	struct mtk_camsv_tag_param img_tag_param[SVTAG_IMG_END];
	struct mtk_camsv_tag_param meta_tag_param;
	struct v4l2_format img_fmt;
	unsigned int tag_idx, mbus_code, hw_scen;
	unsigned int exp_no, req_amount;
	int ret = 0, i, raw_pipe_idx, sv_pipe_idx;

	/* reset tag info */
	sv_dev = dev_get_drvdata(ctx->hw_sv);
	mtk_cam_sv_reset_tag_info(sv_dev);

	/* img tag(s) */
	if (is_stagger_2_exposure(&job->job_scen)) {
		exp_no = req_amount = 2;
		hw_scen = is_stagger_dc(job) ?
			(1 << HWPATH_ID(MTKCAM_IPI_HW_PATH_DC_STAGGER)) :
			(1 << HWPATH_ID(MTKCAM_IPI_HW_PATH_STAGGER));
	} else if (is_stagger_3_exposure(&job->job_scen)) {
		exp_no = req_amount = 3;
		hw_scen = is_stagger_dc(job) ?
			(1 << HWPATH_ID(MTKCAM_IPI_HW_PATH_DC_STAGGER)) :
			(1 << HWPATH_ID(MTKCAM_IPI_HW_PATH_STAGGER));
	} else {
		exp_no = req_amount = 1;
		hw_scen = is_stagger_dc(job) ?
			(1 << HWPATH_ID(MTKCAM_IPI_HW_PATH_ON_THE_FLY)) :
			(1 << HWPATH_ID(MTKCAM_IPI_HW_PATH_DC));
	}
	pr_info("[%s] hw_scen:%d exp_no:%d req_amount:%d",
			__func__, hw_scen, exp_no, req_amount);
	if (mtk_cam_sv_get_tag_param(img_tag_param, hw_scen, exp_no, req_amount))
		return 1;
	else {
		raw_pipe_idx = ctx->raw_subdev_idx;
		raw_pipe = &ctx->cam->pipelines.raw[raw_pipe_idx];
		mbus_code = raw_pipe->pad_cfg[MTK_RAW_SINK].mbus_fmt.code;
		set_dcif_fmt(&img_fmt,
			raw_pipe->pad_cfg[MTK_RAW_SINK].mbus_fmt.width,
			raw_pipe->pad_cfg[MTK_RAW_SINK].mbus_fmt.height,
			raw_pipe->pad_cfg[MTK_RAW_SINK].mbus_fmt.code);
		for (i = 0; i < req_amount; i++) {
			mtk_cam_sv_fill_tag_info(sv_dev->tag_info,
				&img_tag_param[i], hw_scen, 3, job->sub_ratio,
				mbus_code, NULL, &img_fmt);

			sv_dev->used_tag_cnt++;
			sv_dev->enabled_tags |= (1 << img_tag_param[i].tag_idx);
		}
	}
	for (i = 0; i < req_amount; i++)
		pr_info("[%s] tag_param:%d tag_idx:%d seninf_padidx:%d tag_order:%d",
				__func__, i, img_tag_param[i].tag_idx,
				img_tag_param[i].seninf_padidx, img_tag_param[i].tag_order);
	/* meta tag(s) */
	tag_idx = SVTAG_META_START;
	for (i = 0; i < ctx->num_sv_subdevs; i++) {
		if (tag_idx >= SVTAG_END)
			return 1;
		sv_pipe_idx = ctx->sv_subdev_idx[i];
		sv_pipe = &ctx->cam->pipelines.camsv[sv_pipe_idx];
		mbus_code = sv_pipe->pad_cfg[MTK_CAMSV_SINK].mbus_fmt.code;
		img_fmt = sv_pipe->vdev_nodes[
			MTK_CAMSV_MAIN_STREAM_OUT - MTK_CAMSV_SINK_NUM].active_fmt;
		meta_tag_param.tag_idx = tag_idx;
		meta_tag_param.seninf_padidx = sv_pipe->seninf_padidx;
		meta_tag_param.tag_order = mtk_cam_seninf_get_tag_order(
			ctx->seninf, sv_pipe->seninf_padidx);
		mtk_cam_sv_fill_tag_info(sv_dev->tag_info,
			&meta_tag_param, 1, 3, job->sub_ratio,
			mbus_code, sv_pipe, &img_fmt);

		sv_dev->used_tag_cnt++;
		sv_dev->enabled_tags |= (1 << tag_idx);
		tag_idx++;
	}

	return ret;
}

