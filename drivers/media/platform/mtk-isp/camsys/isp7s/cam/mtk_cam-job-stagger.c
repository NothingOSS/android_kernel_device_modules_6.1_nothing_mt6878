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

int fill_imgo_img_buffer_to_ipi_frame_stagger(
	struct req_buffer_helper *helper, struct mtk_cam_buffer *buf,
	struct mtk_cam_video_device *node)
{
	struct mtkcam_ipi_frame_param *fp = helper->fp;
	struct mtkcam_ipi_img_output *out;
	struct mtk_cam_job *job = helper->job;
	bool is_w = is_rgbw(job);
	bool is_otf = !is_dc_mode(job);
	int index = 0, ii_inc = 0, ret = 0;
	bool bypass_imgo;

	helper->filled_hdr_buffer = true;

	bypass_imgo =
		(node->desc.id == MTK_RAW_MAIN_STREAM_OUT) &&
		is_sv_pure_raw(job);

	ii_inc = helper->ii_idx;
	fill_img_in_by_exposure(helper, buf, node);
	ii_inc = helper->ii_idx - ii_inc;
	index += ii_inc;

	if (is_otf && !bypass_imgo) {
		// OTF, raw outputs last exp
		out = &fp->img_outs[helper->io_idx++];
		ret = fill_img_out_hdr(out, buf, node,
				index++, MTKCAM_IPI_RAW_IMGO);

		if (!ret && is_w) {
			out = &fp->img_outs[helper->io_idx++];
			ret = fill_img_out_hdr(out, buf, node,
					index++, raw_video_id_w_port(MTKCAM_IPI_RAW_IMGO));
		}
	}

	if (bypass_imgo && CAM_DEBUG_ENABLED(JOB))
		pr_info("%s:req:%s bypass raw imgo\n",
			__func__, job->req->req.debug_str);
	/* fill sv image fp */
	ret = ret || fill_sv_img_fp(helper, buf, node);

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
	unsigned int pipe_id, exp_no, buf_cnt, buf_ofset = 0;
	int tag_idx, i, j;
	bool is_w;
	int ret = 0;

	if (node->desc.dma_port != MTKCAM_IPI_RAW_IMGO)
		goto EXIT;

	if (ctx->hw_sv == NULL)
		goto EXIT;

	sv_dev = dev_get_drvdata(ctx->hw_sv);
	pipe_id = sv_dev->id + MTKCAM_SUBDEV_CAMSV_START;

	if (is_stagger_2_exposure(scen)) {
		exp_no = 2;
		buf_cnt = is_rgbw(job) ? 2 : 1;
	} else if (is_stagger_3_exposure(scen)) {
		exp_no = 3;
		if (is_rgbw(job)) {
			ret = -1;
			pr_info("%s: rgbw not supported under 3-exp stagger case",
				__func__);
			goto EXIT;
		}
	} else {
		exp_no = 1;
		buf_cnt = is_rgbw(job) ? 2 : 1;
	}

	for (i = 0; i < exp_no; i++) {
		if (!is_sv_pure_raw(job) &&
			!is_dc_mode(job) &&
			(i + 1) == exp_no)
			continue;
		for (j = 0; j < buf_cnt; j++) {
			is_w = (j % 2) ? true : false;
			tag_idx = (exp_no > 1 && (i + 1) == exp_no) ?
				get_sv_tag_idx(exp_no, MTKCAM_IPI_ORDER_LAST_TAG, is_w) :
				get_sv_tag_idx(exp_no, i, is_w);
			if (tag_idx == -1) {
				ret = -1;
				pr_info("%s: tag_idx not found(exp_no:%d is_w:%d)",
					__func__, exp_no, (is_w) ? 1 : 0);
				goto EXIT;
			}
			ret = fill_sv_fp(helper, buf, node, tag_idx, pipe_id, buf_ofset);
			buf_ofset += buf->image_info.size[0];
		}
	}

EXIT:
	return ret;
}

void update_stagger_job_exp(struct mtk_cam_job *job)
{
	struct mtk_cam_scen *scen = &job->job_scen;

	job->exp_num_cur = scen->scen.normal.exp_num;

	switch (job->switch_type) {
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

int apply_cam_mux_switch_stagger(struct mtk_cam_job *job)
{
	struct mtk_cam_ctx *ctx = job->src_ctx;
	struct mtk_camsv_device *sv_dev = dev_get_drvdata(ctx->hw_sv);
	struct mtk_cam_seninf_mux_param param;
	struct mtk_cam_seninf_mux_setting settings[4];
	int type = job->switch_type;
	int config_exposure_num = job->job_scen.scen.normal.max_exp_num;
	int is_dc = is_dc_mode(job);
	int raw_id = _get_master_raw_id(job->used_engine);
	int raw_tg_idx = raw_to_tg_idx(raw_id);
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
			first_tag_idx =
				get_sv_tag_idx(2, MTKCAM_IPI_ORDER_FIRST_TAG, false);
			last_tag_idx =
				get_sv_tag_idx(2, MTKCAM_IPI_ORDER_LAST_TAG, false);
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

			settings[3].seninf = ctx->seninf;
			settings[3].source = PAD_SRC_RAW1;
			settings[3].camtg  =
				mtk_cam_get_sv_cammux_id(sv_dev, last_tag_idx);
			settings[3].tag_id = last_tag_idx;
			settings[3].enable = 1;
			break;
		case EXPOSURE_CHANGE_3_to_1:
		case EXPOSURE_CHANGE_2_to_1:
			first_tag_idx =
				get_sv_tag_idx(1, MTKCAM_IPI_ORDER_FIRST_TAG, false);
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

			settings[3].seninf = ctx->seninf;
			settings[3].source = PAD_SRC_RAW0;
			settings[3].camtg  =
				mtk_cam_get_sv_cammux_id(sv_dev, first_tag_idx);
			settings[3].tag_id = first_tag_idx;
			settings[3].enable = 1;
			break;
		case EXPOSURE_CHANGE_2_to_3:
		case EXPOSURE_CHANGE_1_to_3:
			first_tag_idx =
				get_sv_tag_idx(3, MTKCAM_IPI_ORDER_FIRST_TAG, false);
			second_tag_idx =
				get_sv_tag_idx(3, MTKCAM_IPI_ORDER_NORMAL_TAG, false);
			last_tag_idx =
				get_sv_tag_idx(3, MTKCAM_IPI_ORDER_LAST_TAG, false);
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

			settings[3].seninf = ctx->seninf;
			settings[3].source = PAD_SRC_RAW2;
			settings[3].camtg  =
				mtk_cam_get_sv_cammux_id(sv_dev, last_tag_idx);
			settings[3].tag_id = last_tag_idx;
			settings[3].enable = 1;
			break;
		default:
			break;
		}
		param.settings = &settings[0];
		param.num = 4;
		mtk_cam_seninf_streaming_mux_change(&param);
		dev_info(ctx->cam->dev,
			"[%s] switch Req:%d type:%d cam_mux[0-3]:[%d/%d/%d][%d/%d/%d][%d/%d/%d][%d/%d/%d]\n",
			__func__, job->frame_seq_no, type,
			settings[0].source, settings[0].camtg, settings[0].enable,
			settings[1].source, settings[1].camtg, settings[1].enable,
			settings[2].source, settings[2].camtg, settings[2].enable,
			settings[3].source, settings[3].camtg, settings[3].enable);
	} else if (type != EXPOSURE_CHANGE_NONE && config_exposure_num == 2) {
		switch (type) {
		case EXPOSURE_CHANGE_2_to_1:
			first_tag_idx =
				get_sv_tag_idx(1, MTKCAM_IPI_ORDER_FIRST_TAG, false);
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
			settings[2].source = PAD_SRC_RAW0;
			settings[2].camtg  =
				mtk_cam_get_sv_cammux_id(sv_dev, first_tag_idx);
			settings[2].tag_id = first_tag_idx;
			settings[2].enable = 1;
			break;
		case EXPOSURE_CHANGE_1_to_2:
			first_tag_idx =
				get_sv_tag_idx(2, MTKCAM_IPI_ORDER_FIRST_TAG, false);
			last_tag_idx =
				get_sv_tag_idx(2, MTKCAM_IPI_ORDER_LAST_TAG, false);
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
			settings[2].source = PAD_SRC_RAW1;
			settings[2].camtg  =
				mtk_cam_get_sv_cammux_id(sv_dev, last_tag_idx);
			settings[2].tag_id = last_tag_idx;
			settings[2].enable = 1;
			break;
		default:
			break;
		}
		param.settings = &settings[0];
		param.num = 3;
		mtk_cam_seninf_streaming_mux_change(&param);
		dev_info(ctx->cam->dev,
			"[%s] switch Req:%d type:%d cam_mux[0-2]:[%d/%d/%d][%d/%d/%d][%d/%d/%d] ts:%llu\n",
			__func__, job->frame_seq_no, type,
			settings[0].source, settings[0].camtg, settings[0].enable,
			settings[1].source, settings[1].camtg, settings[1].enable,
			settings[2].source, settings[2].camtg, settings[2].enable,
			ktime_get_boottime_ns() / 1000);
	}

	return 0;
}


int handle_sv_tag(struct mtk_cam_job *job)
{
	struct mtk_cam_ctx *ctx = job->src_ctx;
	struct mtk_raw_sink_data *raw_sink;
	struct mtk_camsv_device *sv_dev;
	struct mtk_camsv_pipeline *sv_pipe;
	struct mtk_camsv_sink_data *sv_sink;
	struct mtk_camsv_tag_param img_tag_param[SVTAG_IMG_END];
	struct mtk_camsv_tag_param meta_tag_param;
	unsigned int tag_idx, hw_scen;
	unsigned int exp_no, req_amount;
	int ret = 0, i, raw_pipe_idx, sv_pipe_idx;

	/* reset tag info */
	sv_dev = dev_get_drvdata(ctx->hw_sv);
	mtk_cam_sv_reset_tag_info(sv_dev);

	/* img tag(s) */
	if (is_stagger_2_exposure(&job->job_scen)) {
		exp_no = req_amount = 2;
		req_amount *= is_rgbw(job) ? 2 : 1;
		hw_scen = is_dc_mode(job) ?
			(1 << HWPATH_ID(MTKCAM_IPI_HW_PATH_DC_STAGGER)) :
			(1 << HWPATH_ID(MTKCAM_IPI_HW_PATH_STAGGER));
	} else if (is_stagger_3_exposure(&job->job_scen)) {
		exp_no = req_amount = 3;
		if (is_rgbw(job)) {
			pr_info("[%s] rgbw not supported under 3-exp stagger case",
				__func__);
			return 1;
		}
		hw_scen = is_dc_mode(job) ?
			(1 << HWPATH_ID(MTKCAM_IPI_HW_PATH_DC_STAGGER)) :
			(1 << HWPATH_ID(MTKCAM_IPI_HW_PATH_STAGGER));
	} else {
		exp_no = req_amount = 1;
		req_amount *= is_rgbw(job) ? 2 : 1;
		hw_scen = is_dc_mode(job) ?
			(1 << HWPATH_ID(MTKCAM_IPI_HW_PATH_DC_STAGGER)) :
			(1 << HWPATH_ID(MTKCAM_IPI_HW_PATH_ON_THE_FLY));
	}
	pr_info("[%s] hw_scen:%d exp_no:%d req_amount:%d",
			__func__, hw_scen, exp_no, req_amount);
	if (mtk_cam_sv_get_tag_param(img_tag_param, hw_scen, exp_no, req_amount))
		return 1;

	raw_pipe_idx = ctx->raw_subdev_idx;
	raw_sink = &job->req->raw_data[raw_pipe_idx].sink;
	for (i = 0; i < req_amount; i++) {
		mtk_cam_sv_fill_tag_info(sv_dev->tag_info,
					 &img_tag_param[i], hw_scen, 3,
					 job->sub_ratio,
					 raw_sink->width, raw_sink->height,
					 raw_sink->mbus_code, NULL);

		sv_dev->used_tag_cnt++;
		sv_dev->enabled_tags |= (1 << img_tag_param[i].tag_idx);
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
		sv_sink = &job->req->sv_data[sv_pipe_idx].sink;
		meta_tag_param.tag_idx = tag_idx;
		meta_tag_param.seninf_padidx = sv_pipe->seninf_padidx;
		meta_tag_param.tag_order = mtk_cam_seninf_get_tag_order(
			ctx->seninf, sv_pipe->seninf_padidx);
		mtk_cam_sv_fill_tag_info(sv_dev->tag_info,
			&meta_tag_param, 1, 3, job->sub_ratio,
			sv_sink->width, sv_sink->height,
			sv_sink->mbus_code, sv_pipe);

		sv_dev->used_tag_cnt++;
		sv_dev->enabled_tags |= (1 << tag_idx);
		tag_idx++;
	}

	return ret;
}

bool is_sv_img_tag_used(struct mtk_cam_job *job)
{
	bool rst = false;

	/* HS_TODO: check all features */
	if (is_stagger_multi_exposure(job))
		rst = !is_hw_offline(job);
	if (is_dc_mode(job))
		rst = true;
	if (is_sv_pure_raw(job))
		rst = true;

	return rst;
}

