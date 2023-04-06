// SPDX-License-Identifier: GPL-2.0
//
// Copyright (c) 2019 MediaTek Inc.

#include "mtk_cam.h"
#include "mtk_cam-job-stagger.h"
#include "mtk_cam-job_utils.h"

int fill_imgo_buf_to_ipi_stagger(
	struct req_buffer_helper *helper, struct mtk_cam_buffer *buf,
	struct mtk_cam_video_device *node)
{
	int ret = 0;
	struct mtk_cam_job *job = helper->job;

	if (is_processed_raw_node(job, node)) {  /* main stream */
		struct mtkcam_ipi_frame_param *fp = helper->fp;
		struct mtkcam_ipi_img_output *out;

		out = &fp->img_outs[helper->io_idx++];

		ret = fill_img_out(out, buf, node);
	} else {  /* pure raw */
		ret = fill_imgo_buf_as_working_buf(helper, buf, node);
	}

	return ret;
}

int apply_cam_mux_switch_stagger(struct mtk_cam_job *job)
{
	struct mtk_cam_ctx *ctx = job->src_ctx;
	struct mtk_camsv_device *sv_dev = dev_get_drvdata(ctx->hw_sv);
	struct mtk_cam_seninf_mux_param param;
	struct mtk_cam_seninf_mux_setting settings[4];
	int type = job->switch_type;
	int config_exposure_num = job->job_scen.scen.normal.max_exp_num;
	int raw_id = _get_master_raw_id(job->used_engine);
	int raw_tg_idx = raw_to_tg_idx(raw_id);
	int first_tag_idx, second_tag_idx, last_tag_idx;
	int first_tag_idx_w, last_tag_idx_w;
	bool is_dc = is_dc_mode(job) ? true : false;

	/**
	 * To identify the "max" exposure_num, we use
	 * feature_active, not job->feature.raw_feature
	 * since the latter one stores the exposure_num information,
	 * not the max one.
	 */

	memset(settings, 0,
		sizeof(struct mtk_cam_seninf_mux_setting) * ARRAY_SIZE(settings));

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
			first_tag_idx_w =
				get_sv_tag_idx(1, MTKCAM_IPI_ORDER_FIRST_TAG, true);
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

			if (is_rgbw(job)) {
				settings[2].seninf = ctx->seninf;
				settings[2].source = PAD_SRC_RAW_W0;
				settings[2].camtg  =
					mtk_cam_get_sv_cammux_id(sv_dev, first_tag_idx_w);
				settings[2].tag_id = first_tag_idx_w;
				settings[2].enable = 1;

				settings[3].seninf = ctx->seninf;
				settings[3].source = PAD_SRC_RAW_W1;
				settings[3].camtg  = -1;
				settings[3].tag_id = -1;
				settings[3].enable = 0;
			} else {
				settings[2].seninf = ctx->seninf;
				settings[2].source = PAD_SRC_RAW0;
				settings[2].camtg  =
					mtk_cam_get_sv_cammux_id(sv_dev, first_tag_idx);
				settings[2].tag_id = first_tag_idx;
				settings[2].enable = 1;
			}
			break;
		case EXPOSURE_CHANGE_1_to_2:
			first_tag_idx =
				get_sv_tag_idx(2, MTKCAM_IPI_ORDER_FIRST_TAG, false);
			first_tag_idx_w =
				get_sv_tag_idx(2, MTKCAM_IPI_ORDER_FIRST_TAG, true);
			last_tag_idx =
				get_sv_tag_idx(2, MTKCAM_IPI_ORDER_LAST_TAG, false);
			last_tag_idx_w =
				get_sv_tag_idx(2, MTKCAM_IPI_ORDER_LAST_TAG, true);
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

			if (is_rgbw(job)) {
				settings[2].seninf = ctx->seninf;
				settings[2].source = PAD_SRC_RAW_W0;
				settings[2].camtg  =
					mtk_cam_get_sv_cammux_id(sv_dev, first_tag_idx_w);
				settings[2].tag_id = first_tag_idx_w;
				settings[2].enable = 1;

				settings[3].seninf = ctx->seninf;
				settings[3].source = PAD_SRC_RAW_W1;
				settings[3].camtg  =
					mtk_cam_get_sv_cammux_id(sv_dev, last_tag_idx_w);
				settings[3].tag_id = last_tag_idx_w;
				settings[3].enable = 1;
			} else {
				settings[2].seninf = ctx->seninf;
				settings[2].source = PAD_SRC_RAW1;
				settings[2].camtg  =
					mtk_cam_get_sv_cammux_id(sv_dev, last_tag_idx);
				settings[2].tag_id = last_tag_idx;
				settings[2].enable = 1;
			}
			break;
		default:
			break;
		}
		param.settings = &settings[0];
		param.num = (is_rgbw(job)) ? 4 : 3;
		mtk_cam_seninf_streaming_mux_change(&param);
		dev_info(ctx->cam->dev,
			"[%s] switch Req:%d type:%d cam_mux[0-3]:[%d/%d/%d][%d/%d/%d][%d/%d/%d][%d/%d/%d]\n",
			__func__, job->frame_seq_no, type,
			settings[0].source, settings[0].camtg, settings[0].enable,
			settings[1].source, settings[1].camtg, settings[1].enable,
			settings[2].source, settings[2].camtg, settings[2].enable,
			settings[3].source, settings[3].camtg, settings[3].enable);
	}

	return 0;
}

