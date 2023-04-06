// SPDX-License-Identifier: GPL-2.0
//
// Copyright (c) 2019 MediaTek Inc.

#include <linux/rpmsg/mtk_ccd_rpmsg.h>
#include <linux/pm_runtime.h>

#include <linux/soc/mediatek/mtk-cmdq-ext.h>

#include "mtk_cam-fmt_utils.h"
#include "mtk_cam.h"
#include "mtk_cam-ipi.h"
#include "mtk_cam-job.h"
#include "mtk_cam-job_state.h"
#include "mtk_cam-job_utils.h"
#include "mtk_cam-job-stagger.h"
#include "mtk_cam-job-subsample.h"
#include "mtk_cam-plat.h"
#include "mtk_cam-debug.h"
#include "mtk_cam-timesync.h"
#include "mtk_cam-hsf.h"
#include "mtk_cam-trace.h"

#define SENSOR_SET_MARGIN_MS  25
#define SENSOR_SET_MARGIN_MS_STAGGER  27

static unsigned int debug_buf_fmt_sel = -1;
module_param(debug_buf_fmt_sel, int, 0644);
MODULE_PARM_DESC(sv_pure_raw, "working fmt select: 0->bayer, 1->ufbc");


/* forward declarations */
static void reset_unused_io_of_ipi_frame(struct req_buffer_helper *helper);
static int update_cq_buffer_to_ipi_frame(struct mtk_cam_pool_buffer *cq,
					 struct mtkcam_ipi_frame_param *fp);
static int job_debug_dump(struct mtk_cam_job *job, const char *desc,
			  bool is_exception, int raw_pipe_idx);
static void job_dump_engines_debug_status(struct mtk_cam_job *job);

static inline int job_debug_exception_dump(struct mtk_cam_job *job,
					   const char *desc)
{
	/* don't care raw_pipe_idx */
	return job_debug_dump(job, desc, 1, -1);
}

static struct mtk_raw_request_data *req_get_raw_data(struct mtk_cam_ctx *ctx,
						     struct mtk_cam_request *req);
static int get_exp_switch_type(struct mtk_cam_job *job);
static bool do_seamless_switch(struct mtk_cam_job *job);
static void mtk_cam_aa_dump_work(struct work_struct *work);

static int subdev_set_fmt(struct v4l2_subdev *sd, int pad,
						  const struct v4l2_mbus_framefmt *format)
{
	struct v4l2_subdev_format fmt;
	int ret = 0;

	fmt.which = V4L2_SUBDEV_FORMAT_ACTIVE;
	fmt.pad = pad;
	fmt.format = *format;
	ret = v4l2_subdev_call(sd, pad, set_fmt, NULL, &fmt);

	pr_info("%s: %s:pad(%d) %dx%d 0x%x",
			__func__, sd->entity.name, pad,
			fmt.format.width, fmt.format.height,
			fmt.format.code);

	return ret;
}

void _on_job_last_ref(struct mtk_cam_job *job)
{
	struct mtk_cam_ctx *ctx = job->src_ctx;
	struct mtk_cam_ctrl *ctrl = &job->src_ctx->cam_ctrl;
	bool is_last;

	write_lock(&ctrl->list_lock);

	list_del(&job->job_state.list);
	is_last = list_empty(&ctrl->camsys_state_list);

	write_unlock(&ctrl->list_lock);

	if (CAM_DEBUG_ENABLED(CTRL) || is_last)
		pr_info("%s: ctx %d job #%d %s%s\n", __func__,
			ctx->stream_id, job->req_seq, job->req->req.debug_str,
			is_last ? "(last)" : "");

	mtk_cam_ctx_job_finish(job);
}

void mtk_cam_ctx_job_finish(struct mtk_cam_job *job)
{
	call_jobop(job, finalize);
	mtk_cam_job_return(job);
}

static void mtk_cam_sensor_work(struct kthread_work *work)
{
	struct mtk_cam_job *job =
		container_of(work, struct mtk_cam_job, sensor_work);

	call_jobop(job, apply_sensor);
	mtk_cam_job_put(job);
}

static int apply_sensor_async(struct mtk_cam_job *job)
{
	struct mtk_cam_ctx *ctx = job->src_ctx;

	return mtk_cam_ctx_queue_sensor_worker(ctx, &job->sensor_work);
}

bool mtk_cam_job_has_pending_action(struct mtk_cam_job *job)
{
	return mtk_cam_job_state_has_action(&job->job_state);
}

int mtk_cam_job_apply_pending_action(struct mtk_cam_job *job)
{
	int action, ret = 0;

	action = mtk_cam_job_state_fetch_and_clear_action(&job->job_state);

	if (action & ACTION_APPLY_SENSOR) {
		mtk_cam_job_get(job);
		ret = ret || apply_sensor_async(job);
	}

	if (action & ACTION_APPLY_ISP)
		ret = ret || call_jobop(job, apply_isp);

	if (action & ACTION_TRIGGER)
		ret = ret || call_jobop(job, trigger_isp);

	if (action & ACTION_COMPOSE_CQ)
		ret = ret || call_jobop(job, compose);

	return ret;
}

static int map_job_type(const struct mtk_cam_scen *scen)
{
	enum mtk_cam_scen_id scen_id = scen->id;
	int job_type;

	switch (scen_id) {
	case MTK_CAM_SCEN_NORMAL:
		if (scen->scen.normal.max_exp_num == 1)
			job_type = JOB_TYPE_BASIC;
		else
			job_type = JOB_TYPE_STAGGER;
		break;
	case MTK_CAM_SCEN_M2M_NORMAL:
	case MTK_CAM_SCEN_ODT_NORMAL:
	case MTK_CAM_SCEN_ODT_MSTREAM:
		job_type = JOB_TYPE_M2M;
		break;

	case MTK_CAM_SCEN_MSTREAM:
		if (scen->scen.mstream.type == MTK_CAM_MSTREAM_1_EXPOSURE)
			job_type = JOB_TYPE_BASIC;
		else
			job_type = JOB_TYPE_MSTREAM;
		break;
	case MTK_CAM_SCEN_SMVR:
		job_type = JOB_TYPE_HW_SUBSAMPLE;
		break;
	case MTK_CAM_SCEN_EXT_ISP:
		job_type = JOB_TYPE_HW_PREISP;
		break;
	default:
		job_type = -1;
		break;
	}

	if (job_type == -1)
		pr_info("%s: failed to map scen_id %d to job\n",
			__func__, scen_id);

	return job_type;
}

/* support pure raw node only */
static bool update_sv_pure_raw(struct mtk_cam_job *job)
{
	bool has_pure_raw, has_processed_raw, is_supported_scen, is_sv_pure_raw;

	has_pure_raw = find_video_node(job, MTK_RAW_PURE_RAW_OUT);
	has_processed_raw = find_video_node(job, MTK_RAW_MAIN_STREAM_OUT);

	/* TODO: scen help func */
	is_supported_scen =
		(job->job_scen.id == MTK_CAM_SCEN_NORMAL);

	is_sv_pure_raw = has_pure_raw && is_supported_scen;

	/**
	 * main-stream: raw hw
	 * pure-raw: raw hw or sv hw
	 *     otf: use sv pure raw flow
	 *     dc: replace sv working buffer
	 */

	if (CAM_DEBUG_ENABLED(JOB))
		pr_info("%s has_pure/processed_raw:%d/%d is_supported_scen: %d sv_pure_raw:%d",
			__func__, has_pure_raw, has_processed_raw,
			is_supported_scen, is_sv_pure_raw);

	if (!is_sv_pure_raw && (has_pure_raw && has_processed_raw)) {
		/* only one type of imgo could be handled */
		mtk_cam_req_buffer_done(job,
					get_raw_subdev_idx(job->src_ctx->used_pipe),
					MTK_RAW_PURE_RAW_OUT,
					VB2_BUF_STATE_ERROR,
					job->timestamp, true);
		pr_info("%s [warn] force return pure raw node", __func__);
	}

	return is_sv_pure_raw;
}

static bool is_scen_support_ufbc(struct mtk_cam_job *job)
{
	bool support = false;

	switch (job->job_scen.id) {
	case MTK_CAM_SCEN_NORMAL:
	case MTK_CAM_SCEN_MSTREAM:
		support = true;
		break;
	default:
		break;
	}

	return support;
}

static bool is_4cell_sensor(struct mtk_cam_job *job)
{
	return get_sensor_data_pattern(job) == MTK_CAM_PATTERN_4CELL;
}

static bool is_sv_support_ufbc(struct mtk_cam_job *job)
{
	bool use_ufbc = !job->is_sv_pure_raw;
	// TODO: 16 pixel mode not support UFBC

	return use_ufbc;
}

static void update_buf_fmt_sel(struct mtk_cam_job *job)
{
	// NOTE: this will change ctx->img_work_buf_desc->fmt_sel
	// everytime packing a new job
	struct mtk_cam_driver_buf_desc *desc =
		&job->src_ctx->img_work_buf_desc;
	bool use_ufbc = true;

	use_ufbc = use_ufbc
		&& is_sv_support_ufbc(job)
		&& !is_4cell_sensor(job)
		&& is_scen_support_ufbc(job);

	if (use_ufbc)
		set_fmt_select(MTKCAM_BUF_FMT_TYPE_UFBC, desc);
	else
		set_fmt_select(MTKCAM_BUF_FMT_TYPE_BAYER, desc);

	if (debug_buf_fmt_sel != -1)
		set_fmt_select(debug_buf_fmt_sel, desc);

	if (CAM_DEBUG_ENABLED(JOB))
		pr_info("%s: use_ufbc %d, desc->fmt_sel %d",
			__func__, use_ufbc, desc->fmt_sel);
}

static int mtk_cam_job_fill_ipi_config(struct mtk_cam_job *job,
	struct mtkcam_ipi_config_param *config);
static int mtk_cam_job_fill_ipi_config_only_sv(struct mtk_cam_job *job,
	struct mtkcam_ipi_config_param *config);
struct pack_job_ops_helper;
static int mtk_cam_job_fill_ipi_frame(struct mtk_cam_job *job,
	struct pack_job_ops_helper *job_helper);

static int mtk_cam_job_pack_init(struct mtk_cam_job *job,
				 struct mtk_cam_ctx *ctx,
				 struct mtk_cam_request *req)
{
	struct device *dev = ctx->cam->dev;
	int ret;

	atomic_set(&job->refs, 1);
	INIT_LIST_HEAD(&job->list);

	job->req = req;
	job->src_ctx = ctx;

	ret = mtk_cam_buffer_pool_fetch(&ctx->cq_pool, &job->cq);
	if (ret) {
		dev_info(dev, "ctx %d failed to fetch cq buffer\n",
			 ctx->stream_id);
		return ret;
	}

	ret = mtk_cam_buffer_pool_fetch(&ctx->ipi_pool, &job->ipi);
	if (ret) {
		dev_info(dev, "ctx %d failed to fetch ipi buffer\n",
			 ctx->stream_id);
		mtk_cam_buffer_pool_return(&job->cq);
		return ret;
	}
	memset(job->ipi.vaddr, 0, sizeof(struct mtkcam_ipi_frame_param));

	INIT_LIST_HEAD(&job->job_state.list);
	apply_cq_ref_reset(&job->cq_ref);

	kthread_init_work(&job->sensor_work, mtk_cam_sensor_work);
	atomic_long_set(&job->afo_done, 0);
	atomic_long_set(&job->done_set, 0);
	job->done_handled = 0;

	/* aa dump info */
	INIT_WORK(&job->aa_dump_work, mtk_cam_aa_dump_work);

	job->frame_cnt = 1;

	job->composed = 0;
	job->timestamp = 0;
	job->timestamp_mono = 0;
	job->timestamp_buf = NULL;
	job->raw_switch = false;

	return ret;
}

static unsigned long mtk_cam_select_hw(struct mtk_cam_job *job)
{
	struct mtk_cam_ctx *ctx = job->src_ctx;
	struct mtk_cam_device *cam = ctx->cam;
	unsigned long available, raw_available, sv_available, mraw_available;
	unsigned long selected;
	int i = 0;

	selected = 0;
	available = mtk_cam_get_available_engine(cam);
	raw_available = bit_map_subset_of(MAP_HW_RAW, available);
	sv_available = bit_map_subset_of(MAP_HW_CAMSV, available);
	mraw_available = bit_map_subset_of(MAP_HW_MRAW, available);

	/* todo: more rules */
	if (ctx->has_raw_subdev) {
		struct mtk_raw_ctrl_data *ctrl;
		int raws;

		ctrl = get_raw_ctrl_data(job);
		if (WARN_ON(!ctrl))
			goto SELECT_HW_FAILED;

		raws = ctrl->resource.user_data.raw_res.raws;

		if (!raws) {
			dev_info(cam->dev, "%s: no raws\n", __func__);
			goto SELECT_HW_FAILED;
		}

		for (i = 0; i < cam->engines.num_raw_devices; i++)
			if (raws & BIT(i))
				selected |= bit_map_bit(MAP_HW_RAW, i);
	}

	/* camsv */
	if (is_m2m(job)) {
		dev_info(cam->dev, "skip camsv select in hw offline scen(%d)\n",
				 job->job_scen.id);
	} else if (selected) {
		/* if has raw */
		int raw_idx = _get_master_raw_id(selected);

		dev_info(cam->dev,
			 "select sv hw start (raw_idx:%d/sv_available:0x%lx)\n",
			 raw_idx, sv_available);

		/* if failed to find corresponding camsv */
		if (!(sv_available & BIT(raw_idx))) {
			dev_info(cam->dev, "select sv hw failed(raw_idx:%d/sv_available:0x%lx)\n",
				raw_idx, sv_available);
			selected = 0;
			goto SELECT_HW_FAILED;
		}

		selected |= bit_map_bit(MAP_HW_CAMSV, raw_idx);
		dev_info(cam->dev,
			 "select sv hw end (raw_idx:%d/sv_available:0x%lx/selected:0x%lx)\n",
			 raw_idx, sv_available, selected);
	} else {
		int rsv_id = GET_PLAT_V4L2(reserved_camsv_dev_id);

		if (!(sv_available & BIT(rsv_id))) {
			dev_info(cam->dev,
				 "only_sv select hw failed: sv_available 0x%lx rsv_id %d\n",
				 sv_available, rsv_id);
			selected = 0;
			goto SELECT_HW_FAILED;
		}

		selected |= bit_map_bit(MAP_HW_CAMSV, rsv_id);
	}

	/* mraw */
	for (i =  0; i < ctx->num_mraw_subdevs; i++) {
		int mraw_idx;

		mraw_idx = ctx->mraw_subdev_idx[i];
		if (mraw_available & BIT(mraw_idx)) {
			struct device *dev;
			struct mtk_mraw_device *mraw_dev;

			selected |= bit_map_bit(MAP_HW_MRAW, mraw_idx);

			dev = cam->engines.mraw_devs[mraw_idx];
			mraw_dev = dev_get_drvdata(dev);
			mraw_dev->pipeline = &cam->pipelines.mraw[mraw_idx];
		}
	}

SELECT_HW_FAILED:

	/* update ctx's hw devs */
	if (mtk_cam_ctx_fetch_devices(ctx, selected))
		return 0;

	return selected;
}

static int update_job_used_engine(struct mtk_cam_job *job)
{
	struct mtk_cam_ctx *ctx = job->src_ctx;
	struct mtk_raw_device *raw_dev;
	struct mtk_camsv_device *sv_dev;
	struct mtk_mraw_device *mraw_dev;
	unsigned long used_engine = 0;
	unsigned long used_pipe = job->req->used_pipe & ctx->used_pipe;
	int i;

	for (i = 0; i < ARRAY_SIZE(ctx->hw_raw); i++) {
		if (ctx->hw_raw[i]) {
			raw_dev = dev_get_drvdata(ctx->hw_raw[i]);
			used_engine |= bit_map_bit(MAP_HW_RAW, raw_dev->id);
		}
	}

	if (ctx->hw_sv) {
		sv_dev = dev_get_drvdata(ctx->hw_sv);
		if (is_sv_img_tag_used(job))
			used_engine |= bit_map_bit(MAP_HW_CAMSV, sv_dev->id);
		for (i = 0; i < ctx->num_sv_subdevs; i++) {
			if (used_pipe &
			    bit_map_bit(MAP_SUBDEV_CAMSV, ctx->sv_subdev_idx[i]))
				used_engine |= bit_map_bit(MAP_HW_CAMSV,
							   sv_dev->id);
		}
	}

	for (i = 0; i < ctx->num_mraw_subdevs; i++) {
		mraw_dev = dev_get_drvdata(ctx->hw_mraw[i]);
		if (used_pipe &
		    bit_map_bit(MAP_SUBDEV_MRAW, ctx->mraw_subdev_idx[i]))
			used_engine |= bit_map_bit(MAP_HW_MRAW, mraw_dev->id);
	}

	job->used_engine = used_engine;

	return 0;
}

static struct engine_callback engine_cb = {
	.isr_event = mtk_cam_ctrl_isr_event,
	.reset_sensor = mtk_cam_ctrl_reset_sensor,
	.dump_request = mtk_cam_ctrl_dump_request,
};

int
mtk_cam_job_initialize_engines(struct mtk_cam_ctx *ctx,
			       struct mtk_cam_job *job,
			       const struct initialize_params *opt)
{
	unsigned long engines;
	int raw_master_id;
	int i;

	engines = ctx->used_engine;

	/* raw */
	raw_master_id = _get_master_raw_id(engines);
	if (raw_master_id >= 0) {
		for (i = 0 ; i < ARRAY_SIZE(ctx->hw_raw); i++) {
			struct mtk_raw_device *raw;
			int is_master;

			if (!ctx->hw_raw[i])
				continue;

			raw = dev_get_drvdata(ctx->hw_raw[i]);
			is_master = !!(raw_master_id == raw->id);

			initialize(raw, !is_master,
				is_dc_mode(job) ? 1 : 0, &engine_cb);

			if (is_master && opt && opt->master_raw_init)
				opt->master_raw_init(ctx->hw_raw[i], job);
		}

		if (job->enable_hsf_raw)
			mtk_cam_hsf_init(ctx);
	}


	/* camsv */
	if (ctx->hw_sv) {
		struct mtk_camsv_device *sv = dev_get_drvdata(ctx->hw_sv);

		/* HS_TODO: to support camsv subsample mode */
		mtk_cam_sv_dev_config(sv, 0);
	}

	/* mraw */
	for (i = 0 ; i < ARRAY_SIZE(ctx->hw_mraw); i++) {
		if (ctx->hw_mraw[i]) {
			struct mtk_mraw_device *mraw =
				dev_get_drvdata(ctx->hw_mraw[i]);

			mtk_cam_mraw_dev_config(mraw, job->sub_ratio - 1); /* TODO(AY): remove -1 */
		}
	}

	return 0;
}

static int
update_job_type_feature(struct mtk_cam_job *job)
{
	struct mtk_cam_ctx *ctx = job->src_ctx;

	if (ctx->has_raw_subdev) {
		struct mtk_raw_ctrl_data *ctrl;

		ctrl = get_raw_ctrl_data(job);
		if (!ctrl)
			return -1;

		job->job_scen = ctrl->resource.user_data.raw_res.scen;
		if (ctx->ctrldata_stored)
			job->prev_scen = ctx->ctrldata.resource.user_data.raw_res.scen;
		else
			job->prev_scen = ctrl->resource.user_data.raw_res.scen;
		job->job_type = map_job_type(&job->job_scen);

		// TODO: remove update_buf_fmt_sel and update_sv_pure_raw dependency in
		// which is_sv_pure_raw is read during update_buf_fmt_sel
		job->is_sv_pure_raw = update_sv_pure_raw(job);
		job->enable_hsf_raw = ctrl->enable_hsf_raw;
		update_buf_fmt_sel(job);
	} else
		job->job_type = JOB_TYPE_ONLY_SV;

	return 0;
}

/* workqueue context */
static int
_meta1_done(struct mtk_cam_job *job)
{
	struct mtk_cam_ctx *ctx = job->src_ctx;
	struct mtk_cam_device *cam = ctx->cam;
	int pipe_id = get_raw_subdev_idx(ctx->used_pipe);

	if (pipe_id < 0)
		return 0;

	if (CAM_DEBUG_ENABLED(JOB))
		dev_info(cam->dev, "%s:%s:ctx(%d): seq_no:0x%x, state:0x%x\n",
			 __func__, job->req->req.debug_str, job->src_ctx->stream_id,
			 job->frame_seq_no,
			 mtk_cam_job_state_get(&job->job_state, ISP_STATE));

	mtk_cam_req_buffer_done(job, pipe_id, MTK_RAW_META_OUT_1,
				VB2_BUF_STATE_DONE, job->timestamp, true);

	return 0;
}

#define TIMESTAMP_LOG
static void cpu_timestamp_to_meta(struct mtk_cam_job *job)
{
	(*job->timestamp_buf)[0] = job->timestamp_mono / 1000;
	(*job->timestamp_buf)[1] = job->timestamp / 1000;

#ifdef TIMESTAMP_LOG
	dev_info(job->src_ctx->cam->dev, /*FIXME*/
		"timestamp TS:momo %llu us boot %llu us\n",
		(*job->timestamp_buf)[0],
		(*job->timestamp_buf)[1]);
#endif
}

static void convert_fho_timestamp_to_meta(struct mtk_cam_job *job)
{
	u32 *fho_va;
	int subsample;
	int i;
	u64 hw_timestamp;

	subsample = job->sub_ratio;
	fho_va = (u32 *)(job->cq.vaddr + job->cq.size - 64 * subsample);

	for (i = 0; i < subsample; i++) {
		hw_timestamp = (u64) *(fho_va + i*16);
		hw_timestamp += ((u64)*(fho_va + i*16 + 1) << 32);

		/* timstamp_LSB + timestamp_MSB << 32 */
		(*job->timestamp_buf)[i*2] =
			mtk_cam_timesync_to_monotonic(hw_timestamp) / 1000;
		(*job->timestamp_buf)[i*2 + 1] =
			mtk_cam_timesync_to_boot(hw_timestamp) / 1000;
#ifdef TIMESTAMP_LOG
		dev_dbg(job->src_ctx->cam->dev,
			"timestamp TS:momo %llu us boot %llu us, hw ts:%llu\n",
			(*job->timestamp_buf)[i*2],
			(*job->timestamp_buf)[i*2 + 1],
			hw_timestamp);
#endif
	}
}

/* workqueue context */
static int
handle_raw_frame_done(struct mtk_cam_job *job)
{
	struct mtk_cam_ctx *ctx = job->src_ctx;
	struct mtk_cam_device *cam = ctx->cam;
	unsigned int used_pipe = job->req->used_pipe & job->src_ctx->used_pipe;
	bool is_normal = true;
	int i;

	if (used_pipe == 0)
		return 0;

	/* skip if meta0 does not exist */
	if (ctx->has_raw_subdev && job->timestamp_buf) {
		if (job->job_type == JOB_TYPE_M2M)
			cpu_timestamp_to_meta(job);
		else
			convert_fho_timestamp_to_meta(job);
	}

	if (ctx->has_raw_subdev) {
		if (job->job_type == JOB_TYPE_STAGGER ||
			job->job_type == JOB_TYPE_MSTREAM) {
			struct mtk_raw_pipeline *pipe =
				&ctx->cam->pipelines.raw[ctx->raw_subdev_idx];

			mtk_raw_hdr_tsfifo_push(pipe, &job->hdr_ts_cache);
		}
	}

	if (CAM_DEBUG_ENABLED(JOB))
		dev_info(cam->dev, "%s:%s:ctx(%d): seq_no:0x%x, state:0x%x, is_normal:%d, B/M ts:%lld/%lld\n",
			 __func__, job->req->req.debug_str, job->src_ctx->stream_id,
			 job->frame_seq_no,
			 mtk_cam_job_state_get(&job->job_state, ISP_STATE),
			 is_normal, job->timestamp, job->timestamp_mono);

	for (i = MTKCAM_SUBDEV_RAW_START; i < MTKCAM_SUBDEV_RAW_END; i++) {
		if (used_pipe & (1 << i)) {
			if (is_normal)
				mtk_cam_req_buffer_done(job, i, -1,
					VB2_BUF_STATE_DONE, job->timestamp, true);
			else
				mtk_cam_req_buffer_done(job, i, -1,
					VB2_BUF_STATE_ERROR, job->timestamp, true);
		}
	}

	return 0;
}

static int
handle_sv_frame_done(struct mtk_cam_job *job)
{
	struct mtk_cam_ctx *ctx = job->src_ctx;
	struct mtk_cam_device *cam = ctx->cam;
	unsigned int used_pipe = job->req->used_pipe & job->src_ctx->used_pipe;
	bool is_normal = true;
	int i, pipe_id;

	if (used_pipe == 0)
		return 0;

	if (CAM_DEBUG_ENABLED(JOB))
		dev_info(cam->dev, "%s:%s:ctx(%d): seq_no:0x%x, state:0x%x, is_normal:%d, B/M ts:%lld/%lld\n",
			 __func__, job->req->req.debug_str, job->src_ctx->stream_id,
			 job->frame_seq_no,
			 mtk_cam_job_state_get(&job->job_state, ISP_STATE),
			 is_normal, job->timestamp, job->timestamp_mono);

	/* sv pure raw */
	if (ctx->has_raw_subdev && is_sv_pure_raw(job)) {
		pipe_id = get_raw_subdev_idx(ctx->used_pipe);
		mtk_cam_req_buffer_done(job, pipe_id, MTK_RAW_PURE_RAW_OUT,
			is_normal ? VB2_BUF_STATE_DONE : VB2_BUF_STATE_ERROR,
			job->timestamp, true);
	}

	for (i = MTKCAM_SUBDEV_CAMSV_START; i < MTKCAM_SUBDEV_CAMSV_END; i++) {
		if (used_pipe & (1 << i)) {
			if (is_normal)
				mtk_cam_req_buffer_done(job, i, -1,
					VB2_BUF_STATE_DONE, job->timestamp, true);
			else
				mtk_cam_req_buffer_done(job, i, -1,
					VB2_BUF_STATE_ERROR, job->timestamp, true);
		}
	}

	return 0;
}

static int
handle_mraw_frame_done(struct mtk_cam_job *job, unsigned int pipe_id)
{
	struct mtk_cam_ctx *ctx = job->src_ctx;
	struct mtk_cam_device *cam = ctx->cam;
	unsigned int used_pipe = job->req->used_pipe & job->src_ctx->used_pipe;
	bool is_normal = true;

	if ((used_pipe & (1 << pipe_id)) == 0) {
		dev_info(cam->dev, "%s: done but not found in req(used_pipe:0x%x/pipe_id:0x%x)",
			__func__, used_pipe, pipe_id);
		return 0;
	}

	if (CAM_DEBUG_ENABLED(JOB))
		dev_info(cam->dev, "%s:%s:ctx(%d): seq_no:0x%x, state:0x%x, is_normal:%d, B/M ts:%lld/%lld\n",
			 __func__, job->req->req.debug_str, job->src_ctx->stream_id,
			 job->frame_seq_no,
			 mtk_cam_job_state_get(&job->job_state, ISP_STATE),
			 is_normal, job->timestamp, job->timestamp_mono);

	if (is_normal)
		mtk_cam_req_buffer_done(job, pipe_id, -1,
			VB2_BUF_STATE_DONE, job->timestamp, true);
	else
		mtk_cam_req_buffer_done(job, pipe_id, -1,
			VB2_BUF_STATE_ERROR, job->timestamp, true);

	return 0;
}

static int job_mark_afo_done(struct mtk_cam_job *job, int seq_no)
{
	struct mtk_cam_ctx *ctx = job->src_ctx;
	struct mtk_cam_ctrl *ctrl = &ctx->cam_ctrl;

	if (!atomic_long_fetch_or(BIT(0), &job->afo_done))
		wake_up_interruptible(&ctrl->done_wq);

	return 0;
}

static int job_mark_engine_done(struct mtk_cam_job *job,
				int engine_type, int engine_id,
				int seq_no)
{
	struct mtk_cam_ctx *ctx = job->src_ctx;
	struct mtk_cam_ctrl *ctrl = &ctx->cam_ctrl;
	unsigned long coming;
	unsigned long old;
	unsigned int master_engine;

	master_engine = _get_master_engines(job->used_engine);
	coming = engine_idx_to_bit(engine_type, engine_id);

	if (!(coming & master_engine))
		return 0;

	if (CAM_DEBUG_ENABLED(STATE))
		pr_info("%s: no 0x%x eng 0x%08x done 0x%lx coming 0x%lx\n",
			__func__, job->frame_seq_no, master_engine,
			atomic_long_read(&job->done_set), coming);

	coming &= master_engine;
	old = atomic_long_fetch_or(coming, &job->done_set);

	wake_up_interruptible(&ctrl->done_wq);

	if (CAM_DEBUG_ENABLED(AA))
		call_jobop(job, dump_aa_info, engine_type);

	return (old | coming) == master_engine;
}

static int get_tg_seninf_pad(struct mtk_cam_job *job)
{
	int tg_exp_num = 1;

	if (job->job_scen.id == MTK_CAM_SCEN_NORMAL)
		tg_exp_num = scen_max_exp_num(&job->job_scen);
	else
		tg_exp_num = 1;

	switch (tg_exp_num) {
	case 2:
		return PAD_SRC_RAW1;
	case 3:
		return PAD_SRC_RAW2;
	case 1:
	default:
		return PAD_SRC_RAW0;
	}
}

static int
_stream_on(struct mtk_cam_job *job, bool on)
{
	struct mtk_cam_ctx *ctx = job->src_ctx;
	struct mtk_raw_device *raw_dev;
	struct mtk_camsv_device *sv_dev;
	struct mtk_mraw_device *mraw_dev;
	int seninf_pad = get_tg_seninf_pad(job), raw_tg_idx = -1;
	int i;

	for (i = 0; i < ARRAY_SIZE(ctx->hw_raw); i++) {
		if (ctx->hw_raw[i]) {
			raw_dev = dev_get_drvdata(ctx->hw_raw[i]);
			if (raw_tg_idx == -1)
				raw_tg_idx = raw_to_tg_idx(raw_dev->id);
		}
	}

	if (is_dc_mode(job)) {
		seninf_pad = 0;
		raw_tg_idx = -1;
	}

	/* TODO: separate seninf api to cammux setting and enable */
	if (job->stream_on_seninf || job->raw_switch) {
		ctx_stream_on_seninf_sensor(job, seninf_pad, raw_tg_idx);

		if (job->first_frm_switch)
			apply_cam_mux_switch_stagger(job);
	}

	for (i = 0; i < ARRAY_SIZE(ctx->hw_raw); i++) {
		if (ctx->hw_raw[i]) {
			raw_dev = dev_get_drvdata(ctx->hw_raw[i]);

			if (job->enable_hsf_raw) {
				ccu_stream_on(ctx, on);
			} else {
				update_scq_start_period(raw_dev, job->scq_period);
				stream_on(raw_dev, on);
			}
		}
	}

	if (ctx->hw_sv) {
		sv_dev = dev_get_drvdata(ctx->hw_sv);
			mtk_cam_sv_update_start_period(sv_dev, job->scq_period);
		mtk_cam_sv_dev_stream_on(sv_dev, on,
			job->enabled_tags, job->used_tag_cnt);
	}

	for (i = 0; i < ctx->num_mraw_subdevs; i++) {
		if (ctx->hw_mraw[i]) {
			mraw_dev = dev_get_drvdata(ctx->hw_mraw[i]);
			mtk_cam_mraw_update_start_period(mraw_dev, job->scq_period);
			mtk_cam_mraw_dev_stream_on(mraw_dev, on);
		}
	}

	return 0;
}

static int
_stream_on_only_sv(struct mtk_cam_job *job, bool on)
{
	struct mtk_cam_ctx *ctx = job->src_ctx;
	struct mtk_camsv_device *sv_dev;

	if (ctx->hw_sv) {
		sv_dev = dev_get_drvdata(ctx->hw_sv);
		mtk_cam_sv_dev_stream_on(sv_dev, on,
			job->enabled_tags, job->used_tag_cnt);
	}

	if (job->stream_on_seninf)
		ctx_stream_on_seninf_sensor(job, 0, -1);

	return 0;
}

static bool mtk_cam_fs_sync_frame(struct mtk_cam_job *job, int state)
{
	struct mtk_cam_request *req = job->req;
	bool ret = false;

	if (job->sensor &&
	    job->sensor->ops &&
	    job->sensor->ops->core &&
	    job->sensor->ops->core->command) {
		job->sensor->ops->core->command(job->sensor,
						V4L2_CMD_FSYNC_SYNC_FRAME_START_END,
						&state);
		ret = true;
	} else {
		pr_info("%s:%s: find sensor command failed, state(%d)\n",
			__func__, req->req.debug_str, state);
	}

	return ret;
}

static bool frame_sync_start(struct mtk_cam_job *job)
{
	struct mtk_cam_device *cam = job->src_ctx->cam;
	struct mtk_cam_request *req = job->req;
	struct mtk_cam_frame_sync *fs = &req->fs;
	bool ret = false;

	if (!need_frame_sync(fs))
		return ret;

	mutex_lock(&fs->op_lock);
	if (is_first_sensor(fs)) /* 1st sensor setting of request */
		ret = mtk_cam_fs_sync_frame(job, 1);
	mutex_unlock(&fs->op_lock);

	if (ret)
		dev_dbg(cam->dev, "req(%s) %s by ctx:%d\n",
			req->req.debug_str, __func__, job->ctx_id);
	return ret;
}

static bool frame_sync_end(struct mtk_cam_job *job)
{
	struct mtk_cam_device *cam = job->src_ctx->cam;
	struct mtk_cam_request *req = job->req;
	struct mtk_cam_frame_sync *fs = &req->fs;
	bool ret = false;

	if (!need_frame_sync(fs))
		return ret;

	mutex_lock(&fs->op_lock);
	if (is_last_sensor(fs)) /* the last sensor setting of request */
		ret = mtk_cam_fs_sync_frame(job, 0);
	mutex_unlock(&fs->op_lock);

	if (ret)
		dev_dbg(cam->dev, "req(%s) %s by ctx:%d\n",
			req->req.debug_str, __func__, job->ctx_id);

	return ret;
}

static void job_complete_sensor_ctrl_obj(struct mtk_cam_job *job)
{
	if (job->sensor_hdl_obj) {
		mtk_cam_req_complete_ctrl_obj(job->sensor_hdl_obj);
		job->sensor_hdl_obj = NULL;
	}
}

static int update_sensor_fmt(struct mtk_cam_job *job)
{
	struct mtk_cam_ctx *ctx = job->src_ctx;
	struct mtk_cam_device *cam = ctx->cam;
	struct mtk_cam_request *req = job->req;
	struct mtk_raw_request_data *raw_data = req_get_raw_data(ctx, req);
	struct v4l2_mbus_framefmt sink_mfmt;
	bool update_fmt = job->seamless_switch;

	if (!update_fmt)
		return 0;

	if (!raw_data) {
		dev_info(cam->dev, "%s: raw_data not found: ctx-%d job %d\n",
			 __func__, ctx->stream_id, job->frame_seq_no);
		return 0;
	}

	sink_mfmt.width = raw_data->sink.width;
	sink_mfmt.height = raw_data->sink.height;
	sink_mfmt.code = raw_data->sink.mbus_code;
	sink_mfmt.field = V4L2_FIELD_NONE,
	sink_mfmt.colorspace = V4L2_COLORSPACE_SRGB,
	sink_mfmt.ycbcr_enc = V4L2_YCBCR_ENC_DEFAULT,
	sink_mfmt.quantization = V4L2_QUANTIZATION_DEFAULT,
	sink_mfmt.xfer_func = V4L2_XFER_FUNC_DEFAULT,
	sink_mfmt.flags = 0,
	memset(sink_mfmt.reserved, 0, sizeof(sink_mfmt.reserved));

	subdev_set_fmt(job->sensor, 0, &sink_mfmt);

	return 0;
}

static void mtk_cam_set_sensor_mstream_mode(struct mtk_cam_ctx *ctx, bool on)
{
	struct v4l2_ctrl *mstream_mode_ctrl;

	/* TODO(AY): cache v4l2_ctrl in ctx */
	mstream_mode_ctrl = v4l2_ctrl_find(ctx->sensor->ctrl_handler,
			V4L2_CID_MTK_MSTREAM_MODE);

	if (!mstream_mode_ctrl) {
		dev_info(ctx->cam->dev,
			"%s: ctx(%d): no sensor mstream mode control found\n",
			__func__, ctx->stream_id);
		return;
	}

	if (on)
		v4l2_ctrl_s_ctrl(mstream_mode_ctrl, 1);
	else
		v4l2_ctrl_s_ctrl(mstream_mode_ctrl, 0);

	dev_info(ctx->cam->dev, "%s mstream mode:%d\n", __func__, on);
}

static bool check_update_mstream_mode(struct mtk_cam_job *job)
{
	if (job->job_scen.id == MTK_CAM_SCEN_MSTREAM) {
		bool exp_switch =
			job_exp_num(job) != job_prev_exp_num(job);

		return ((job->frame_seq_no == 0) || exp_switch);
	}

	return false;
}

/* kthread context */
static int
_apply_sensor(struct mtk_cam_job *job)
{
	struct mtk_cam_ctx *ctx = job->src_ctx;
	struct mtk_cam_device *cam = ctx->cam;
	struct mtk_cam_request *req = job->req;

	if (!job->sensor_hdl_obj) {
		dev_info(cam->dev, "[%s] warn. no sensor_hdl_obj to apply: ctx-%d job 0x%x\n",
			 __func__, ctx->stream_id, job->frame_seq_no);
		return 0;
	}

	frame_sync_start(job);

	if (check_update_mstream_mode(job))
		mtk_cam_set_sensor_mstream_mode(ctx, 0);

	update_sensor_fmt(job);

	v4l2_ctrl_request_setup(&req->req, job->sensor->ctrl_handler);
	dev_info(cam->dev, "[%s] ctx:%d, job: 0x%x\n",
		 __func__, ctx->stream_id, job->frame_seq_no);

	frame_sync_end(job);

	/* TBC */
	/* mtk_cam_tg_flash_req_setup(ctx, s_data); */

	mtk_cam_job_state_set(&job->job_state, SENSOR_STATE, S_SENSOR_APPLIED);

	job_complete_sensor_ctrl_obj(job);

	return 0;
}
#define STAGGER_SEAMLESS_DBLOAD_FORCE 1
static int apply_cam_switch_stagger(struct mtk_cam_job *job)
{
	struct mtk_cam_ctx *ctx = job->src_ctx;
	struct mtk_cam_device *cam = ctx->cam;
	int raw_id = _get_master_raw_id(job->used_engine);
	struct mtk_raw_device *raw_dev = NULL;
	int type = job->switch_type;
	bool stagger_mode_updated = false;
	bool is_dc = is_dc_mode(job) ? true : false;

	if (raw_id < 0)
		return -1;
	raw_dev = dev_get_drvdata(cam->engines.raw_devs[raw_id]);
	if (type == EXPOSURE_CHANGE_3_to_1 ||
		type == EXPOSURE_CHANGE_2_to_1) {
		stagger_disable(raw_dev);
		stagger_mode_updated = true;
		dev_info(cam->dev,
			"[%s] ctx:%d, job:0x%x, stagger_disable\n",
			__func__, ctx->stream_id, job->frame_seq_no);
	} else if (type == EXPOSURE_CHANGE_1_to_2 ||
		type == EXPOSURE_CHANGE_1_to_3) {
		stagger_enable(raw_dev, is_dc);
		stagger_mode_updated = true;
		dev_info(cam->dev,
			"[%s] ctx:%d, job:0x%x, stagger_enable\n",
			__func__, ctx->stream_id, job->frame_seq_no);
	}

	if (stagger_mode_updated && STAGGER_SEAMLESS_DBLOAD_FORCE)
		dbload_force(raw_dev);

	return 0;
}

static int update_seninf_fmt(struct mtk_cam_job *job)
{
	struct mtk_cam_ctx *ctx = job->src_ctx;
	struct mtk_cam_device *cam = ctx->cam;
	struct mtk_cam_request *req = job->req;
	struct mtk_raw_request_data *raw_data = req_get_raw_data(ctx, req);
	struct v4l2_mbus_framefmt sink_mfmt;
	bool update_fmt = job->seamless_switch;

	if (!update_fmt)
		return 0;

	if (!raw_data) {
		dev_info(cam->dev, "%s: raw_data not found: ctx-%d job %d\n",
			 __func__, ctx->stream_id, job->frame_seq_no);
		return 0;
	}

	sink_mfmt.width = raw_data->sink.width;
	sink_mfmt.height = raw_data->sink.height;
	sink_mfmt.code = raw_data->sink.mbus_code;
	sink_mfmt.field = V4L2_FIELD_NONE,
	sink_mfmt.colorspace = V4L2_COLORSPACE_SRGB,
	sink_mfmt.ycbcr_enc = V4L2_YCBCR_ENC_DEFAULT,
	sink_mfmt.quantization = V4L2_QUANTIZATION_DEFAULT,
	sink_mfmt.xfer_func = V4L2_XFER_FUNC_DEFAULT,
	sink_mfmt.flags = 0,
	memset(sink_mfmt.reserved, 0, sizeof(sink_mfmt.reserved));

	subdev_set_fmt(job->seninf, PAD_SINK, &sink_mfmt);
	subdev_set_fmt(job->seninf, PAD_SRC_RAW0, &sink_mfmt);

	return 0;
}

/* kthread context */
static int
_apply_switch(struct mtk_cam_job *job)
{
	switch (job->job_type) {
	case JOB_TYPE_BASIC:
	case JOB_TYPE_STAGGER:
		update_seninf_fmt(job);
		apply_cam_mux_switch_stagger(job);
		apply_cam_switch_stagger(job);
		break;
	default:
		pr_info("%s: job type %d not ready\n", __func__, job->job_type);
		break;
	}
	pr_info("%s: job type:%d, seq:0x%x\n", __func__, job->job_type, job->frame_seq_no);
	return 0;
}

static int ipi_config(struct mtk_cam_job *job)
{
	struct mtk_cam_ctx *ctx = job->src_ctx;
	struct mtkcam_ipi_event event;
	struct mtkcam_ipi_session_cookie *session = &event.cookie;
	struct mtkcam_ipi_config_param *config = &event.config_data;
	struct mtkcam_ipi_config_param *src_config = &job->ipi_config;

	event.cmd_id = CAM_CMD_CONFIG;
	session->session_id = ctx->stream_id;
	memcpy(config, src_config, sizeof(*src_config));

	rpmsg_send(ctx->rpmsg_dev->rpdev.ept, &event, sizeof(event));

	dev_info(job->src_ctx->cam->dev, "%s: rpmsg_send id: %d\n",
		 __func__, event.cmd_id);
	return 0;
}

static int send_ipi_frame(struct mtk_cam_job *job,
			  struct mtk_cam_pool_buffer *ipi,
			  int frame_seq_no)
{
	struct mtk_cam_ctx *ctx = job->src_ctx;
	struct mtkcam_ipi_event event;
	struct mtkcam_ipi_session_cookie *session = &event.cookie;
	struct mtkcam_ipi_frame_info *frame_info = &event.frame_data;

	event.cmd_id = CAM_CMD_FRAME;
	session->session_id = ctx->stream_id;
	session->frame_no = to_fh_cookie(ctx->stream_id, frame_seq_no);

	frame_info->cur_msgbuf_offset = ipi->size * ipi->priv.index;
	frame_info->cur_msgbuf_size = ipi->size;

	if (WARN_ON(!job->src_ctx->rpmsg_dev))
		return -1;

	rpmsg_send(ctx->rpmsg_dev->rpdev.ept, &event, sizeof(event));

	if (CAM_DEBUG_ENABLED(JOB))
		dev_info(ctx->cam->dev,
			 "[%s id:%d] req:%s ctx:%d seq:0x%x\n",
			 __func__, session->session_id,
			 job->req->req.debug_str, ctx->stream_id, frame_seq_no);
	return 0;
}

static int _compose(struct mtk_cam_job *job)
{
	if (job->do_ipi_config && ipi_config(job))
		return -1;

	return send_ipi_frame(job, &job->ipi, job->frame_seq_no);
}

static bool is_valid_cq(struct mtkcam_ipi_cq_desc_entry *cq_entry)
{
	return !!cq_entry->size;
}

static
unsigned long engines_to_trigger_cq(struct mtk_cam_job *job,
				    struct mtkcam_ipi_frame_ack_result *cq_ret)
{
	struct mtk_cam_ctx *ctx = job->src_ctx;
	unsigned long used_engine, subset;
	unsigned long cq_engine;
	int dev_idx;
	int i;

	used_engine = ctx->used_engine;
	cq_engine = 0;

	/* raw */
	subset = bit_map_subset_of(MAP_HW_RAW, used_engine);
	if (subset)
		if (is_valid_cq(&cq_ret->main) && is_valid_cq(&cq_ret->sub)) {
			dev_idx = find_first_bit_set(subset);
			cq_engine |= bit_map_bit(MAP_HW_RAW, dev_idx);
		}

	/* mraw */
	if (bit_map_subset_of(MAP_HW_MRAW, used_engine))
		for (i = 0; i < ARRAY_SIZE(cq_ret->mraw); ++i)
			if (is_valid_cq(&cq_ret->mraw[i])) {
				dev_idx = ctx->mraw_subdev_idx[i];
				cq_engine |= bit_map_bit(MAP_HW_MRAW, dev_idx);
			}

	/* camsv */
	subset = bit_map_subset_of(MAP_HW_CAMSV, used_engine);
	if (subset)
		for (i = 0; i < ARRAY_SIZE(cq_ret->camsv); ++i)
			if (is_valid_cq(&cq_ret->camsv[i])) {
				dev_idx = find_first_bit_set(subset);
				cq_engine |= bit_map_bit(MAP_HW_CAMSV, dev_idx);

				/* only single sv device */
				break;
			}

	return cq_engine;
}

static int _apply_raw_cq(struct mtk_cam_job *job,
			 unsigned long raw_engines,
			 struct mtk_cam_pool_buffer *cq,
			 struct mtkcam_ipi_frame_ack_result *cq_rst)
{
	struct mtk_cam_device *cam = job->src_ctx->cam;
	int raw_id;
	struct mtk_raw_device *raw_dev;

	raw_id = find_first_bit_set(raw_engines);
	if (raw_id < 0)
		return -1;

	raw_dev = dev_get_drvdata(cam->engines.raw_devs[raw_id]);

	if (job->enable_hsf_raw)
		ccu_apply_cq(job, raw_engines,
			cq->daddr, cq_rst->main.size,
			cq_rst->main.offset, cq_rst->sub.size,
			cq_rst->sub.offset);
	else
		apply_cq(raw_dev, &job->cq_ref,
			cq->daddr,
			cq_rst->main.size, cq_rst->main.offset,
			cq_rst->sub.size, cq_rst->sub.offset);

	return 0;
}

static int _apply_sv_cq(struct mtk_cam_job *job,
			unsigned long sv_engines,
			struct mtk_cam_pool_buffer *cq,
			struct mtkcam_ipi_frame_ack_result *cq_rst)
{
	struct mtk_cam_device *cam = job->src_ctx->cam;
	struct mtk_camsv_device *sv_dev;
	int sv_dev_id;

	sv_dev_id = find_first_bit_set(sv_engines);
	if (sv_dev_id < 0)
		return -1;

	sv_dev = dev_get_drvdata(cam->engines.sv_devs[sv_dev_id]);

	apply_camsv_cq(sv_dev, &job->cq_ref,
		       cq->daddr,
		       cq_rst->camsv[0].size,
		       cq_rst->camsv[0].offset, 0);
	return 0;
}

static int _apply_mraw_cq(struct mtk_cam_job *job,
			  unsigned long mraw_engines,
			  struct mtk_cam_pool_buffer *cq,
			  struct mtkcam_ipi_frame_ack_result *cq_rst)
{
	struct mtk_cam_ctx *ctx = job->src_ctx;
	struct mtk_cam_device *cam = ctx->cam;
	struct mtk_mraw_device *mraw_dev;
	int i, mraw_idx;

	for (i = 0; i < ctx->num_mraw_subdevs; i++) {
		mraw_idx = ctx->mraw_subdev_idx[i];
		if (!(mraw_engines & BIT(mraw_idx)))
			continue;

		mraw_dev = dev_get_drvdata(cam->engines.mraw_devs[mraw_idx]);
		apply_mraw_cq(mraw_dev, &job->cq_ref,
			      cq->daddr,
			      cq_rst->mraw[i].size,
			      cq_rst->mraw[i].offset, 0);
	}
	return 0;
}

static int apply_engines_cq(struct mtk_cam_job *job,
			    int frame_seq_no,
			    struct mtk_cam_pool_buffer *cq,
			    struct mtkcam_ipi_frame_ack_result *cq_rst)
{
	struct mtk_cam_ctx *ctx = job->src_ctx;
	unsigned long cq_engine;
	unsigned long subset;

	cq_engine = engines_to_trigger_cq(job, cq_rst);

	apply_cq_ref_init(&job->cq_ref,
			  to_fh_cookie(ctx->stream_id, frame_seq_no),
			  cq_engine);
	WRITE_ONCE(ctx->cam_ctrl.cur_cq_ref, &job->cq_ref);

	subset = bit_map_subset_of(MAP_HW_RAW, cq_engine);
	if (subset)
		_apply_raw_cq(job, subset, cq, cq_rst);

	subset = bit_map_subset_of(MAP_HW_CAMSV, cq_engine);
	if (subset)
		_apply_sv_cq(job, subset, cq, cq_rst);

	subset = bit_map_subset_of(MAP_HW_MRAW, cq_engine);
	if (subset)
		_apply_mraw_cq(job, subset, cq, cq_rst);

	mtk_cam_apply_qos(job);

	dev_info(ctx->cam->dev, "[%s] ctx:%d CQ-0x%x eng 0x%lx cq_addr: %pad\n",
		 __func__, ctx->stream_id, frame_seq_no, cq_engine,
		 &cq->daddr);
	return 0;
}

static int _apply_cq(struct mtk_cam_job *job)
{
	if (WARN_ON(!job->composed))
		return -1;

	apply_engines_cq(job, job->frame_seq_no, &job->cq, &job->cq_rst);
	return 0;
}

static void adl_cmdq_worker(struct work_struct *work)
{
	struct mtk_cam_adl_work *adl_work =
		container_of(work, struct mtk_cam_adl_work, work);
	struct mtk_cam_ctx *ctx =
		container_of(adl_work, struct mtk_cam_ctx, adl_work);
	struct cmdq_client *client = NULL;
	struct cmdq_pkt *pkt;

	client = ctx->cam->cmdq_clt;
	if (WARN_ON_ONCE(!client))
		return;

	pkt = cmdq_pkt_create(client);

	if (WARN_ON(!pkt))
		return;

	if (adl_work->is_dc)
		write_pkt_trigger_apu_dc(adl_work->raw_dev, pkt);
	else
		write_pkt_trigger_apu_frame_mode(adl_work->raw_dev, pkt);

	cmdq_pkt_flush(pkt);
	cmdq_pkt_destroy(pkt);
}

static void trigger_adl_by_work(struct mtk_cam_ctx *ctx,
				struct mtk_raw_device *raw_dev,
				bool is_apu_dc)
{
	struct mtk_cam_adl_work *adl_work = &ctx->adl_work;

	mtk_cam_ctx_flush_adl_work(ctx);

	INIT_WORK(&adl_work->work, adl_cmdq_worker);
	adl_work->raw_dev = raw_dev;
	adl_work->is_dc = is_apu_dc;

	queue_work(system_highpri_wq, &adl_work->work);
}

//#define ADL_FRAME_MODE_BY_CMDQ
static int trigger_m2m(struct mtk_cam_job *job)
{
	struct mtk_cam_ctx *ctx = job->src_ctx;
	struct mtk_cam_device *cam = ctx->cam;
	int raw_id = _get_master_raw_id(job->used_engine);
	struct mtk_raw_device *raw_dev =
		dev_get_drvdata(cam->engines.raw_devs[raw_id]);
	bool is_apu;
	bool is_apu_dc;

	is_apu = is_m2m_apu(job);

	mtk_cam_event_frame_sync(&ctx->cam_ctrl, job->req_seq);

	if (is_apu) {
		is_apu_dc = is_m2m_apu_dc(job);

#ifdef ADL_FRAME_MODE_BY_CMDQ
		trigger_adl_by_work(ctx, raw_dev, is_apu_dc);
#else
		if (is_apu_dc)
			trigger_adl_by_work(ctx, raw_dev, 1);
		else
			trigger_adl(raw_dev);
#endif
	} else
		trigger_rawi_r5(raw_dev);

	dev_info(raw_dev->dev, "%s [ctx:%d] seq 0x%x%s\n",
		 __func__, ctx->stream_id, job->frame_seq_no,
		 is_apu ? (is_apu_dc ? " apu_dc" : " apu") : "");

	return 0;
}

static int job_print_warn_desc(struct mtk_cam_job *job, const char *desc,
			       char warn_desc[64])
{
	struct mtk_cam_ctx *ctx = job->src_ctx;

	return snprintf(warn_desc, 64, "%s:ctx-%d:req-%d:seq-0x%x:%s",
			job->req->req.debug_str, ctx->stream_id,
			job->req_seq, job->frame_seq_no, desc);
}

static void trigger_error_dump(struct mtk_cam_job *job,
			       const char *desc)
{
	struct mtk_cam_ctx *ctx = job->src_ctx;
	char warn_desc[64];

	job_print_warn_desc(job, desc, warn_desc);
	pr_info("%s: desc=%s warn_desc=%s\n", __func__, desc, warn_desc);

	if (!job_debug_exception_dump(job, desc)) {

		job_dump_engines_debug_status(job);

		mtk_cam_event_error(&ctx->cam_ctrl, desc);
		WRAP_AEE_EXCEPTION(desc, warn_desc);
	}
}

static void dump_job_info(struct mtk_cam_job *job)
{
	struct mtk_cam_ctx *ctx = job->src_ctx;
	struct device *dev = ctx->cam->dev;

	dev_info(dev, "%s: ctx-%d pipe %x job type %d req-%d-0x%x eng %x\n",
		 __func__,
		 ctx->stream_id, ctx->used_pipe,
		 job->job_type,
		 job->req_seq, job->frame_seq_no,
		 job->used_engine);

	dev_info(dev, "%s: done status: %lx(handled %lx)/afo %lx\n",
		 __func__,
		 atomic_long_read(&job->done_set), job->done_handled,
		 atomic_long_read(&job->afo_done));
}

static void job_dump(struct mtk_cam_job *job, int seq_no, const char *desc)
{
	struct mtk_cam_ctx *ctx = job->src_ctx;
	struct device *dev = ctx->cam->dev;
	int isp_state;

	if (atomic_read(&ctx->cam_ctrl.stopped)) {
		dev_info(dev, "%s: stopped, skip dump\n", __func__);
		return;
	}

	isp_state = mtk_cam_job_state_get(&job->job_state, ISP_STATE);
	if (isp_state < S_ISP_COMPOSED) {
		dev_info(dev, "%s: job %d not composed yet. skip dump\n",
			 __func__, job->req_seq);
		return;
	}

	dump_job_info(job);
	dev_info(dev, "%s: (dump seq 0x%x) ISP_STATE %s\n",
		 __func__, seq_no,
		 str_isp_state(isp_state));

	if (isp_in_done_state(isp_state))
		return;

	trigger_error_dump(job, desc);
}

static void job_dump_mstream(struct mtk_cam_job *job,
				int seq_no, const char *desc)
{
	struct mtk_cam_ctx *ctx = job->src_ctx;
	struct device *dev = ctx->cam->dev;
	int isp_state;

	if (atomic_read(&ctx->cam_ctrl.stopped)) {
		dev_info(dev, "%s: stopped, skip dump\n", __func__);
		return;
	}

	isp_state = mtk_cam_job_state_get(&job->job_state, ISP_2ND_STATE);
	if (isp_state < S_ISP_COMPOSED) {
		dev_info(dev, "%s: job %d not composed yet. skip dump\n",
			 __func__, job->req_seq);
		return;
	}

	dump_job_info(job);
	dev_info(dev, "%s: (dump seq 0x%x) ISP_STATE = %s/%s\n",
		 __func__, seq_no,
		 mtk_cam_job_state_str(&job->job_state, ISP_1ST_STATE),
		 mtk_cam_job_state_str(&job->job_state, ISP_2ND_STATE));

	if (isp_in_done_state(isp_state))
		return;

	trigger_error_dump(job, desc);
}

static void normal_dump_if_enable(struct mtk_cam_job *job)
{
	struct mtk_cam_ctx *ctx = job->src_ctx;
	struct mtk_cam_debug *dbg = &ctx->cam->dbg;
	int pipe_idx;

	if (!ctx->has_raw_subdev)
		return;

	pipe_idx = get_raw_subdev_idx(ctx->used_pipe);
	if (!mtk_cam_debug_dump_enabled(dbg, pipe_idx))
		return;

	job_debug_dump(job, MSG_NORMAL_DUMP, 0, pipe_idx);
}

static void
_compose_done(struct mtk_cam_job *job,
	      struct mtkcam_ipi_frame_ack_result *cq_ret, int compose_ret)
{
	job->composed = !compose_ret;
	job->cq_rst = *cq_ret;

	if (compose_ret)
		trigger_error_dump(job, MSG_COMPOSE_ERROR);
	else
		normal_dump_if_enable(job);
}

static int job_dump_aa_info(struct mtk_cam_job *job, int engine_type)
{
	struct mtk_cam_ctx *ctx = job->src_ctx;

	if (engine_type != CAMSYS_ENGINE_RAW)
		return 0;

	mtk_cam_job_get(job);

	return mtk_cam_ctx_queue_aa_dump_wq(ctx, &job->aa_dump_work);
}

int master_raw_set_subsample(struct device *dev, struct mtk_cam_job *job)
{
	struct mtk_raw_device *raw;

	raw = dev_get_drvdata(dev);
	subsample_enable(raw, job->sub_ratio);

	return 0;
}

static int job_related_hw_init(struct mtk_cam_job *job)
{
	struct mtk_cam_ctx *ctx = job->src_ctx;
	unsigned long selected;

	selected = mtk_cam_select_hw(job);
	if (!selected)
		return -1;

	if (mtk_cam_occupy_engine(ctx->cam, selected))
		return -1;

	ctx->used_engine = selected;
	mtk_cam_pm_runtime_engines(&ctx->cam->engines, selected, 1);

	/* original initialize_engines(), only rename, no change */
	mtk_cam_job_initialize_engines(ctx, job, job->init_params);

	return 0;
}

static int
_job_pack_subsample(struct mtk_cam_job *job,
	 struct pack_job_ops_helper *job_helper)
{
	struct mtk_cam_ctx *ctx = job->src_ctx;
	struct mtk_cam_device *cam = ctx->cam;
	int first_frame_only_cur = job->job_scen.scen.smvr.output_first_frame_only;
	int first_frame_only_prev = job->prev_scen.scen.smvr.output_first_frame_only;
	int ret, subsof_fps;

	job->sub_ratio = get_subsample_ratio(&job->job_scen);
	subsof_fps = get_sensor_fps(job) / job->sub_ratio;
	job->scq_period = subsof_fps > 0 ? SCQ_DEADLINE_MS / (subsof_fps / 30) : SCQ_DEADLINE_MS;
	dev_info(cam->dev, "[%s] ctx:%d, type:%d, scen_id:%d, 1stonly:%d, ratio:%d, subsof_fps:%d",
		__func__, ctx->stream_id, job->job_type, job->job_scen.id, first_frame_only_cur,
		job->sub_ratio, subsof_fps);
	job->stream_on_seninf = false;

	if (!ctx->used_engine) {
		if (job_related_hw_init(job))
			return -1;

		job->stream_on_seninf = true;
	}

	/* config_flow_by_job_type */
	update_job_used_engine(job);

	job->do_ipi_config = false;
	if (first_frame_only_cur != first_frame_only_prev) {
		dev_info(ctx->cam->dev, "%s Subsample reconfig 1stframe change %d->%d\n",
		 __func__, first_frame_only_prev, first_frame_only_cur);
		ctx->configured = false;
	}
	if (!ctx->configured) {
		/* if has raw */
		if (bit_map_subset_of(MAP_HW_RAW, ctx->used_engine)) {
			/* ipi_config_param */
			ret = mtk_cam_job_fill_ipi_config(job, &ctx->ipi_config);
			if (ret)
				return ret;
		}
		if (first_frame_only_cur)
			ctx->ipi_config.flags |= MTK_CAM_IPI_CONFIG_TYPE_SMVR_PREVIEW;
		job->do_ipi_config = true;
		ctx->configured = true;
	}
	/* clone into job for debug dump */
	job->ipi_config = ctx->ipi_config;
	if (!ctx->not_first_job) {
		ctx->not_first_job = true;

		mtk_cam_job_update_clk(job);
	}
	ret = mtk_cam_job_fill_ipi_frame(job, job_helper);

	return ret;
}

int master_raw_set_stagger(struct device *dev, struct mtk_cam_job *job)
{
	struct mtk_raw_device *raw;
	bool is_dc = is_dc_mode(job);

	raw = dev_get_drvdata(dev);

	if (job_exp_num(job) > 1)
		stagger_enable(raw, is_dc ? true : false);

	return 0;
}

static bool is_sensor_changed(struct mtk_cam_job *job)
{
	struct mtk_raw_ctrl_data *ctrl_data = get_raw_ctrl_data(job);

	if (!ctrl_data || !ctrl_data->rc_data.sensor_update)
		return false;

	if (job->seninf_prev != job->seninf)
		return true;

	return false;
}

/**
 * To check and update job->raw_switch.
 * Must be called after job->stream_on_seninf is determined and
 * before ipi config/ input change parameter is prepared
 */
static void update_job_raw_switch(struct mtk_cam_job *job)
{
	struct mtk_cam_ctx *ctx = job->src_ctx;
	bool raw_switch = false;
	int r;

	if (!is_sensor_changed(job))
		goto EXIT_SET_RAW_SWITCH;

	dev_info(ctx->cam->dev,
		 "%s:ctx(%d): change sensor:(%s) --> (%s/%s)\n", __func__,
		 ctx->stream_id, job->seninf_prev->entity.name,
		 job->sensor->entity.name, job->seninf->entity.name);
	/* update pipeline cached pads and v4l2 internal data here */
	media_pipeline_stop(&job->seninf_prev->entity.pads[0]);
	r = media_pipeline_start(&job->seninf->entity.pads[0], &ctx->pipeline);
	if (r)
		dev_info(ctx->cam->dev,
			 "%s:failed in media_pipeline_start:%d\n",
			 __func__, r);

	/* The user changed the sensor in the first request */
	if (job->stream_on_seninf) {
		/* It is not real raw switch, just update the ctx' sensor */
		ctx->sensor = job->sensor;
		ctx->seninf = job->seninf;
		goto EXIT_SET_RAW_SWITCH;
	}

	raw_switch = true;

EXIT_SET_RAW_SWITCH:
	job->raw_switch = raw_switch;
}

bool check_if_need_configure(bool ctx_has_configured,
			     bool seamless_switch, bool raw_switch)
{
	if (!ctx_has_configured)
		return true;

	/**
	 * In raw switch case, the camsv restart flow needs the config
	 * ipi, so we return true directly.
	 */
	if (seamless_switch || raw_switch)
		return true;

	return false;
}

static int
_job_pack_otf_stagger(struct mtk_cam_job *job,
	 struct pack_job_ops_helper *job_helper)
{
	struct mtk_cam_ctx *ctx = job->src_ctx;
	struct mtk_cam_device *cam = ctx->cam;
	struct mtk_cam_scen *prev_scen = &job->prev_scen;
	bool sensor_change = is_sensor_changed(job);
	int ret;

	job->switch_type = get_exp_switch_type(job);

	/**
	 * If sensor_change happened, we also run the
	 * mtk_cam_ctrl_stream_on_job() flow. (In this case,
	 * we stop the previous stream and start a new stream of
	 * the updated sensor.)
	 */
	job->first_frm_switch =
		(!ctx->not_first_job || sensor_change) && do_seamless_switch(job);
	job->seamless_switch =
		(ctx->not_first_job && !sensor_change) && do_seamless_switch(job);
	job->sub_ratio = get_subsample_ratio(&job->job_scen);

	dev_info(cam->dev, "[%s] ctx:%d, type:%d, scen exp:%d->%d, swi:%d",
		__func__, ctx->stream_id, job->job_type,
		prev_scen->scen.normal.exp_num,
		job->job_scen.scen.normal.exp_num, job->switch_type);
	job->stream_on_seninf = false;
	job->scq_period = SCQ_DEADLINE_MS_STAGGER;

	if (!ctx->used_engine) {
		if (job_related_hw_init(job))
			return -1;

		job->stream_on_seninf = true;
	}

	/* determine if it is a raw switch job */
	update_job_raw_switch(job);

	/* config_flow_by_job_type */
	update_job_used_engine(job);

	job->do_ipi_config = false;
	if (check_if_need_configure(ctx->configured, job->seamless_switch,
				    job->raw_switch)) {
		/* handle camsv tags */
		if (handle_sv_tag(job)) {
			dev_info(cam->dev, "tag handle failed");
			return -1;
		}

		/* if has raw */
		if (bit_map_subset_of(MAP_HW_RAW, ctx->used_engine)) {
			/* ipi_config_param */
			ret = mtk_cam_job_fill_ipi_config(job, &ctx->ipi_config);
			if (ret)
				return ret;
		}
		job->do_ipi_config = true;
		ctx->configured = true;
	}
	/* clone into job for debug dump */
	job->ipi_config = ctx->ipi_config;

	job->used_tag_cnt = ctx->used_tag_cnt;
	job->enabled_tags = ctx->enabled_tags;
	memcpy(job->tag_info, ctx->tag_info,
		sizeof(struct mtk_camsv_tag_info) * CAMSV_MAX_TAGS);

	if (!ctx->not_first_job) {
		ctx->not_first_job = true;

		mtk_cam_job_update_clk(job);
	}
	ret = mtk_cam_job_fill_ipi_frame(job, job_helper);
	return ret;
}

static int job_init_mstream(struct mtk_cam_job *job)
{
	struct mtk_cam_mstream_job *mjob =
		container_of(job, struct mtk_cam_mstream_job, job);
	struct mtk_cam_ctx *ctx = job->src_ctx;
	int ret;

	job->frame_cnt = 2; /* TODO(AY): not always */

	mjob->composed_idx = 0;
	mjob->apply_sensor_idx = 0;
	mjob->apply_isp_idx = 0;
	mjob->composed_1st = 0;

	ret = mtk_cam_buffer_pool_fetch(&ctx->cq_pool, &mjob->cq);
	if (ret) {
		pr_info("%s: ctx %d failed to fetch cq buffer\n",
			__func__, ctx->stream_id);
		return ret;
	}

	ret = mtk_cam_buffer_pool_fetch(&ctx->ipi_pool, &mjob->ipi);
	if (ret) {
		pr_info("%s: ctx %d failed to fetch ipi buffer\n",
			__func__, ctx->stream_id);
		mtk_cam_buffer_pool_return(&job->cq);
		return ret;
	}
	memset(mjob->ipi.vaddr, 0, sizeof(struct mtkcam_ipi_frame_param));

	return ret;
}

static int update_buffer_to_ipi_mstream_1st(struct mtk_cam_job *job,
					struct mtkcam_ipi_frame_param *fp_1st,
					struct mtkcam_ipi_frame_param *fp_2nd)
{
	struct req_buffer_helper helper;
	struct mtkcam_ipi_img_input *img_in;
	struct mtkcam_ipi_img_output *img_out;
	int i;

	memset(&helper, 0, sizeof(helper));
	helper.fp = fp_1st;

	/* find 2nd frame's rawi => 1st frame's imgo */
	img_in = &fp_2nd->img_ins[0];
	i = 0;
	while (i < ARRAY_SIZE(fp_2nd->img_ins)) {

		if (img_in->uid.id == MTKCAM_IPI_RAW_RAWI_2)
			break;
		++img_in;
		++i;
	}

	if (i == ARRAY_SIZE(fp_2nd->img_ins)) {
		pr_info("%s: failed to find rawi_2 in 2nd ipi\n", __func__);
		return -1;
	}

	img_out = &fp_1st->img_outs[helper.io_idx];
	++helper.io_idx;

	img_out->uid = (struct mtkcam_ipi_uid) {
		.pipe_id = img_in->uid.pipe_id,
		.id = MTKCAM_IPI_RAW_IMGO,
	};
	img_out->fmt = img_in->fmt;
	for (i = 0; i < ARRAY_SIZE(img_in->buf); ++i)
		img_out->buf[0][i] = img_in->buf[i];

	/* copy 2nd camsv frame's param to 1st */
	memcpy(fp_1st->camsv_param, fp_2nd->camsv_param,
		sizeof(struct mtkcam_ipi_camsv_frame_param) *
		CAMSV_MAX_PIPE_USED * CAMSV_MAX_TAGS);

	/* copy 2nd mraw frame's param to 1st */
	memcpy(fp_1st->mraw_param, fp_2nd->mraw_param,
		sizeof(struct mtkcam_ipi_mraw_frame_param) *
		MRAW_MAX_PIPE_USED);

	reset_unused_io_of_ipi_frame(&helper);
	return 0;
}

static int fill_1st_ipi_mstream(struct mtk_cam_job *job)
{
	struct mtkcam_ipi_frame_param *fp_1st, *fp_2nd;
	struct mtk_cam_mstream_job *mjob;

	mjob = container_of(job, struct mtk_cam_mstream_job, job);

	fp_2nd = (struct mtkcam_ipi_frame_param *)job->ipi.vaddr;
	fp_1st = (struct mtkcam_ipi_frame_param *)mjob->ipi.vaddr;

	update_cq_buffer_to_ipi_frame(&mjob->cq, fp_1st);

	fp_1st->raw_param = fp_2nd->raw_param;
	fp_1st->raw_param.imgo_path_sel = MTKCAM_IPI_IMGO_UNPROCESSED;

	return update_buffer_to_ipi_mstream_1st(job, fp_1st, fp_2nd);
}


/* TODO(AY): too many duplicated codes in _job_pack_xxx */
static int
_job_pack_mstream(struct mtk_cam_job *job,
	 struct pack_job_ops_helper *job_helper)
{
	struct mtk_cam_ctx *ctx = job->src_ctx;
	struct mtk_cam_device *cam = ctx->cam;
	int ret;

	job->sub_ratio = get_subsample_ratio(&job->job_scen);

	dev_info(cam->dev, "[%s] ctx/seq:%d/0x%x, type:%d",
		__func__, ctx->stream_id, job->frame_seq_no, job->job_type);
	job->stream_on_seninf = false;

	if (!ctx->used_engine) {
		if (job_related_hw_init(job))
			return -1;

		job->stream_on_seninf = true;
	}

	/* determine if it is a raw switch job */
	update_job_raw_switch(job);

	/* config_flow_by_job_type */
	update_job_used_engine(job);

	job->do_ipi_config = false;
	if (check_if_need_configure(ctx->configured, false, job->raw_switch)) {
		/* handle camsv tags */
		if (handle_sv_tag(job)) {
			dev_info(cam->dev, "tag handle failed");
			return -1;
		}

		/* if has raw */
		if (bit_map_subset_of(MAP_HW_RAW, ctx->used_engine)) {
			/* ipi_config_param */
			ret = mtk_cam_job_fill_ipi_config(job, &ctx->ipi_config);
			if (ret)
				return ret;
		}
		job->do_ipi_config = true;
		ctx->configured = true;
	}
	/* clone into job for debug dump */
	job->ipi_config = ctx->ipi_config;

	job->used_tag_cnt = ctx->used_tag_cnt;
	job->enabled_tags = ctx->enabled_tags;
	memcpy(job->tag_info, ctx->tag_info,
		sizeof(struct mtk_camsv_tag_info) * CAMSV_MAX_TAGS);

	if (!ctx->not_first_job) {
		ctx->not_first_job = true;

		mtk_cam_job_update_clk(job);
	}

	if (mtk_cam_job_fill_ipi_frame(job, job_helper))
		return -1;

	return fill_1st_ipi_mstream(job);
}

static bool seamless_config_changed(struct mtk_cam_job *job)
{
	struct mtk_cam_ctx *ctx = job->src_ctx;
	struct mtk_raw_sink_data *raw_sink;
	struct mtk_camsv_sink_data *sv_sink;
	struct mtk_mraw_sink_data *mraw_sink;
	struct mtkcam_ipi_crop sink_crop;
	struct mtkcam_ipi_input_param *in;
	struct mtk_camsv_device *sv_dev;
	unsigned int tag_idx;
	int i, j;

	if (!job->seamless_switch || !ctx)
		return false;

	/* raw */
	raw_sink = get_raw_sink_data(job);
	if (raw_sink) {
		in = &ctx->ipi_config.input;
		sink_crop = v4l2_rect_to_ipi_crop(&raw_sink->crop);

		if (!ipi_crop_eq(&in->in_crop, &sink_crop) ||
			in->fmt != sensor_mbus_to_ipi_fmt(raw_sink->mbus_code))
			return true;
	}

	/* camsv */
	for (i = 0; i < ctx->num_sv_subdevs; i++) {
		/* sink data not updated under no enque case, so bypass check */
		if (!(job->req->used_pipe &
			bit_map_bit(MAP_HW_CAMSV, ctx->sv_subdev_idx[i])))
			continue;

		sv_dev = dev_get_drvdata(ctx->hw_sv);
		tag_idx = mtk_cam_get_sv_tag_index(ctx->tag_info,
			ctx->sv_subdev_idx[i] + MTKCAM_SUBDEV_CAMSV_START);
		in = &ctx->ipi_config.sv_input[0][tag_idx].input;
		sv_sink = &job->req->sv_data[ctx->sv_subdev_idx[i]].sink;
		sink_crop = v4l2_rect_to_ipi_crop(&sv_sink->crop);

		if (!ipi_crop_eq(&in->in_crop, &sink_crop) ||
			in->fmt != sensor_mbus_to_ipi_fmt(sv_sink->mbus_code))
			return true;
	}

	/* mraw */
	for (i = 0; i < ctx->num_mraw_subdevs; i++) {
		/* sink data not updated under no enque case, so bypass check */
		if (!(job->req->used_pipe &
			bit_map_bit(MAP_HW_MRAW, ctx->mraw_subdev_idx[i])))
			continue;

		for (j = 0; j < MRAW_MAX_PIPE_USED; j++) {
			if (ctx->ipi_config.mraw_input[j].pipe_id ==
				ctx->mraw_subdev_idx[i] + MTKCAM_SUBDEV_MRAW_START) {
				in = &ctx->ipi_config.mraw_input[j].input;
				break;
			}
		}
		if (j == MRAW_MAX_PIPE_USED)
			continue;

		mraw_sink = &job->req->mraw_data[ctx->mraw_subdev_idx[i]].sink;
		sink_crop = v4l2_rect_to_ipi_crop(&mraw_sink->crop);
		if (!ipi_crop_eq(&in->in_crop, &sink_crop) ||
			in->fmt != sensor_mbus_to_ipi_fmt(mraw_sink->mbus_code))
			return true;
	}

	return false;
}

static int
_job_pack_normal(struct mtk_cam_job *job,
	 struct pack_job_ops_helper *job_helper)
{
	struct mtk_cam_ctx *ctx = job->src_ctx;
	struct mtk_cam_device *cam = ctx->cam;
	int ret;

	job->seamless_switch = do_seamless_switch(job);
	job->sub_ratio = get_subsample_ratio(&job->job_scen);
	dev_dbg(cam->dev, "[%s] ctx:%d, job_type:%d, scen:%d",
		__func__, ctx->stream_id, job->job_type, job->job_scen.id);
	job->stream_on_seninf = false;

	if (!ctx->used_engine) {
		if (job_related_hw_init(job))
			return -1;

		job->stream_on_seninf = true;
	}

	/* determine if it is a raw switch job */
	update_job_raw_switch(job);

	/* config_flow_by_job_type */
	update_job_used_engine(job);

	job->do_ipi_config = false;
	if (check_if_need_configure(ctx->configured,
				    seamless_config_changed(job),
				    job->raw_switch)) {
		/* handle camsv tags */
		if (handle_sv_tag(job)) {
			dev_info(cam->dev, "tag handle failed");
			return -1;
		}

		/* if has raw */
		if (bit_map_subset_of(MAP_HW_RAW, ctx->used_engine)) {
			/* ipi_config_param */
			ret = mtk_cam_job_fill_ipi_config(job, &ctx->ipi_config);
			if (ret)
				return ret;
		}
		job->do_ipi_config = true;
		ctx->configured = true;
	}
	/* clone into job for debug dump */
	job->ipi_config = ctx->ipi_config;

	job->used_tag_cnt = ctx->used_tag_cnt;
	job->enabled_tags = ctx->enabled_tags;
	memcpy(job->tag_info, ctx->tag_info,
		sizeof(struct mtk_camsv_tag_info) * CAMSV_MAX_TAGS);

	if (!ctx->not_first_job) {

		ctx->not_first_job = true;

		mtk_cam_job_update_clk(job);
	}
	ret = mtk_cam_job_fill_ipi_frame(job, job_helper);

	return ret;
}

static int
_job_pack_m2m(struct mtk_cam_job *job,
	      struct pack_job_ops_helper *job_helper)
{
	struct mtk_cam_ctx *ctx = job->src_ctx;
	struct mtk_cam_device *cam = ctx->cam;
	int ret;

	job->sub_ratio = get_subsample_ratio(&job->job_scen);
	dev_dbg(cam->dev, "[%s] ctx:%d, job_type:%d, scen:%d",
		__func__, ctx->stream_id, job->job_type, job->job_scen.id);
	job->stream_on_seninf = false;
	if (!ctx->used_engine) {
		if (job_related_hw_init(job))
			return -1;
		//job->stream_on_seninf = true;
	}

	/* config_flow_by_job_type */
	update_job_used_engine(job);

	job->do_ipi_config = false;
	if (!ctx->configured) {
		/* if has raw */
		if (bit_map_subset_of(MAP_HW_RAW, ctx->used_engine)) {
			/* ipi_config_param */
			ret = mtk_cam_job_fill_ipi_config(job, &ctx->ipi_config);
			if (ret)
				return ret;
		}
		job->do_ipi_config = true;
		ctx->configured = true;
	}
	/* clone into job for debug dump */
	job->ipi_config = ctx->ipi_config;
	if (!ctx->not_first_job) {

		ctx->not_first_job = true;

		mtk_cam_job_update_clk(job);
	}
	ret = mtk_cam_job_fill_ipi_frame(job, job_helper);

	return ret;
}

static int
_job_pack_only_sv(struct mtk_cam_job *job,
	 struct pack_job_ops_helper *job_helper)
{
	struct mtk_cam_ctx *ctx = job->src_ctx;
	struct mtk_cam_device *cam = ctx->cam;
	int ret;

	job->sub_ratio = get_subsample_ratio(&job->job_scen);
	dev_dbg(cam->dev, "[%s] ctx:%d, job_type:%d, scen:%d",
		__func__, ctx->stream_id, job->job_type, job->job_scen.id);
	job->stream_on_seninf = false;
	if (!ctx->used_engine) {
		if (job_related_hw_init(job))
			return -1;

		/* update sensor resource */
		mtk_cam_update_sensor_resource(ctx);

		job->stream_on_seninf = true;
	}

	/* config_flow_by_job_type */
	update_job_used_engine(job);

	job->do_ipi_config = false;
	if (!ctx->configured) {
		/* handle camsv tags */
		if (mtk_cam_is_display_ic(ctx)) {
			if (handle_sv_tag_display_ic(job)) {
				dev_info(cam->dev, "tag handle failed");
				return -1;
			}
		} else {
			if (handle_sv_tag_only_sv(job)) {
				dev_info(cam->dev, "tag handle failed");
				return -1;
			}
		}

		/* if has sv */
		if (bit_map_subset_of(MAP_HW_CAMSV, ctx->used_engine)) {
			/* ipi_config_param */
			ret = mtk_cam_job_fill_ipi_config_only_sv(job, &ctx->ipi_config);
			if (ret)
				return ret;
		}
		job->do_ipi_config = true;
		ctx->configured = true;
	}
	/* clone into job for debug dump */
	job->ipi_config = ctx->ipi_config;

	job->used_tag_cnt = ctx->used_tag_cnt;
	job->enabled_tags = ctx->enabled_tags;
	memcpy(job->tag_info, ctx->tag_info,
		sizeof(struct mtk_camsv_tag_info) * CAMSV_MAX_TAGS);

	if (!ctx->not_first_job)
		ctx->not_first_job = true;
	ret = mtk_cam_job_fill_ipi_frame(job, job_helper);

	return ret;
}

static int fill_raw_img_buffer_to_ipi_frame(
	struct req_buffer_helper *helper, struct mtk_cam_buffer *buf,
	struct mtk_cam_video_device *node)
{
	struct mtk_cam_job *job = helper->job;
	struct mtkcam_ipi_frame_param *fp = helper->fp;
	int ret = -1;
	bool is_pure_imgo = is_pure_raw_node(job, node);

	if (is_pure_imgo && is_sv_pure_raw(job)) {
		/* pure raw */
		if (CAM_DEBUG_ENABLED(JOB))
			pr_info("%s:req:%s bypass pure raw node\n",
				__func__, job->req->req.debug_str);
	} else if (V4L2_TYPE_IS_CAPTURE(buf->vbb.vb2_buf.type)) {
		struct mtkcam_ipi_img_output *out;
		/* main-stream + pure raw + others*/
		out = &fp->img_outs[helper->io_idx++];

		ret = fill_img_out(out, buf, node);
	} else {
		struct mtkcam_ipi_img_input *in;

		in = &fp->img_ins[helper->ii_idx];
		++helper->ii_idx;

		ret = fill_img_in(in, buf, node, -1);
	}

	/* fill sv image fp */
	ret = fill_sv_img_fp(helper, buf, node);

	if (ret)
		pr_info("%s: failed\n", __func__);

	return ret;
}

static int fill_imgo_buf_to_ipi_normal(struct req_buffer_helper *helper,
			struct mtk_cam_buffer *buf,
			struct mtk_cam_video_device *node)
{
	int ret = 0;
	struct mtk_cam_job *job = helper->job;

	if (is_dc_mode(job) && is_pure_raw_node(job, node))  /* pure raw only */
		ret = fill_imgo_buf_as_working_buf(helper, buf, node);
	else  /* main-stream + pure raw */
		ret = fill_raw_img_buffer_to_ipi_frame(helper, buf, node);

	return ret;
}

static int fill_m2m_imgo_to_img_out_ipi(struct req_buffer_helper *helper,
	struct mtk_cam_buffer *buf,
	struct mtk_cam_video_device *node)
{
	struct mtk_cam_job *job = helper->job;
	bool is_w = is_rgbw(job);
	int ret = 0;

	ret = fill_raw_img_buffer_to_ipi_frame(helper, buf, node);

	if (!ret && is_w) {
		struct mtkcam_ipi_frame_param *fp = helper->fp;
		struct mtkcam_ipi_img_output *out;

		out = &fp->img_outs[helper->io_idx++];

		ret = fill_img_out_w(out, buf, node);
	}

	return ret;
}

int fill_imgo_buf_to_ipi_mstream(
	struct req_buffer_helper *helper, struct mtk_cam_buffer *buf,
	struct mtk_cam_video_device *node)
{
	struct mtkcam_ipi_frame_param *fp = helper->fp;
	struct mtk_cam_job *job = helper->job;
	struct mtkcam_ipi_img_output *out;
	struct mtkcam_ipi_img_input *in;

	int exp_order = get_exp_order(&job->job_scen);

	helper->filled_hdr_buffer = true;

	if (CAM_DEBUG_ENABLED(JOB))
		pr_info("%s: exp_order: %d\n",
				__func__, exp_order);

	// RAWI is always the first exp
	in = &fp->img_ins[helper->ii_idx++];
	fill_img_in_hdr(in, buf, node,
		get_buf_offset_idx(exp_order, 0, 0, 0),
		MTKCAM_IPI_RAW_RAWI_2);

	// IMGO is used as the second exp
	out = &fp->img_outs[helper->io_idx++];
	fill_img_out_hdr(out, buf, node,
		get_buf_offset_idx(exp_order, 1, 0, 0),
		MTKCAM_IPI_RAW_IMGO);

	return 0;
}

static int fill_sv_img_buffer_to_ipi_frame(
	struct req_buffer_helper *helper, struct mtk_cam_buffer *buf,
	struct mtk_cam_video_device *node)
{
	struct mtk_cam_job *job = helper->job;
	struct mtk_cam_ctx *ctx = job->src_ctx;
	struct mtkcam_ipi_frame_param *fp = helper->fp;
	struct mtkcam_ipi_img_output *out;
	struct mtk_camsv_device *sv_dev;
	struct vb2_buffer *vb;
	struct dma_info info;
	unsigned int tag_idx, img_fmt;
	void *vaddr;
	int ret = -1;

	if (ctx->hw_sv == NULL)
		return ret;

	sv_dev = dev_get_drvdata(ctx->hw_sv);
	tag_idx = mtk_cam_get_sv_tag_index(job->tag_info, node->uid.pipe_id);

	out = &fp->camsv_param[0][tag_idx].camsv_img_outputs[0];
	ret = fill_img_out(out, buf, node);

	fp->camsv_param[0][tag_idx].pipe_id =
		sv_dev->id + MTKCAM_SUBDEV_CAMSV_START;
	fp->camsv_param[0][tag_idx].tag_id = tag_idx;
	fp->camsv_param[0][tag_idx].hardware_scenario = 0;
	out->uid.id = MTKCAM_IPI_CAMSV_MAIN_OUT;
	out->uid.pipe_id =
		sv_dev->id + MTKCAM_SUBDEV_CAMSV_START;

	img_fmt = mtk_cam_get_img_fmt(buf->image_info.v4l2_pixelformat);
	if (img_fmt == MTKCAM_IPI_IMG_FMT_UNKNOWN)
		pr_info("[%s] unknown image format: 0x%x\n",
			__func__, buf->image_info.v4l2_pixelformat);

	if (is_raw_ufo(img_fmt))
		out->buf[0][0].iova = buf->daddr;
	else {
		out->buf[0][0].iova =
			((((buf->daddr + GET_PLAT_V4L2(meta_sv_ext_size)) + 15)
			>> 4) << 4);

		/* update meta header */
		vb = &buf->vbb.vb2_buf;
		vaddr = vb2_plane_vaddr(vb, 0);
		info.width = buf->image_info.width;
		info.height = buf->image_info.height;
		info.stride = buf->image_info.bytesperline[0];
		CALL_PLAT_V4L2(
			set_sv_meta_stats_info, node->desc.dma_port, vaddr, &info);
	}

	return ret;
}

static int fill_sv_img_buffer_to_ipi_frame_display_ic(
	struct req_buffer_helper *helper, struct mtk_cam_buffer *buf,
	struct mtk_cam_video_device *node)
{
	struct mtk_cam_ctx *ctx = helper->job->src_ctx;
	struct mtkcam_ipi_frame_param *fp = helper->fp;
	struct mtkcam_ipi_img_output *out;
	struct mtk_camsv_device *sv_dev;
	const unsigned int proc_tag[2] = {SVTAG_0, SVTAG_1};
	unsigned int tag_idx, buf_offset = 0;
	int i, ret = -1;

	if (ctx->hw_sv == NULL)
		return ret;

	sv_dev = dev_get_drvdata(ctx->hw_sv);

	for (i = 0; i < ARRAY_SIZE(proc_tag); i++) {
		tag_idx = proc_tag[i];

		out = &fp->camsv_param[0][tag_idx].camsv_img_outputs[0];
		ret = fill_img_out(out, buf, node);

		fp->camsv_param[0][tag_idx].pipe_id =
			sv_dev->id + MTKCAM_SUBDEV_CAMSV_START;
		fp->camsv_param[0][tag_idx].tag_id = tag_idx;
		fp->camsv_param[0][tag_idx].hardware_scenario = 0;
		out->uid.id = MTKCAM_IPI_CAMSV_MAIN_OUT;
		out->uid.pipe_id =
			sv_dev->id + MTKCAM_SUBDEV_CAMSV_START;
		out->buf[0][0].iova =
			((((buf->daddr + buf_offset) + 15) >> 4) << 4);

		/* override fmt */
		if (node->active_fmt.fmt.pix_mp.pixelformat == V4L2_PIX_FMT_NV21)
			out->fmt.format = MTKCAM_IPI_IMG_FMT_BAYER8;
		else
			out->fmt.format = MTKCAM_IPI_IMG_FMT_BAYER10;

		if (tag_idx == SVTAG_1)
			out->fmt.s.h = out->fmt.s.h / 2;

		buf_offset = out->fmt.stride[0] * out->fmt.s.h;
	}

	return ret;
}

static int fill_sv_ext_img_buffer_to_ipi_frame_display_ic(
	struct req_buffer_helper *helper, struct mtk_cam_buffer *buf,
	struct mtk_cam_video_device *node)
{
	struct mtk_cam_ctx *ctx = helper->job->src_ctx;
	struct mtkcam_ipi_frame_param *fp = helper->fp;
	struct mtkcam_ipi_img_output *out;
	struct mtk_camsv_device *sv_dev;
	unsigned int tag_idx;
	int ret = -1;

	if (ctx->hw_sv == NULL)
		return ret;

	sv_dev = dev_get_drvdata(ctx->hw_sv);
	tag_idx = SVTAG_2;

	out = &fp->camsv_param[0][tag_idx].camsv_img_outputs[0];
	ret = fill_img_out(out, buf, node);

	fp->camsv_param[0][tag_idx].pipe_id =
		sv_dev->id + MTKCAM_SUBDEV_CAMSV_START;
	fp->camsv_param[0][tag_idx].tag_id = tag_idx;
	fp->camsv_param[0][tag_idx].hardware_scenario = 0;
	out->uid.id = MTKCAM_IPI_CAMSV_MAIN_OUT;
	out->uid.pipe_id =
		sv_dev->id + MTKCAM_SUBDEV_CAMSV_START;
	out->buf[0][0].iova = buf->daddr;

	return ret;
}

/*
 *  Note: this function will be called with spin_lock held. Can't sleep.
 */
static void job_cancel(struct mtk_cam_job *job)
{
	int i, used_pipe = 0;

	if (!job->req)
		return;

	pr_info("%s: #%d\n", __func__, job->req_seq);

	used_pipe = job->req->used_pipe & job->src_ctx->used_pipe;

	frame_sync_dec_target(&job->req->fs);

	for (i = 0; i < MTKCAM_SUBDEV_MAX; i++) {
		if (used_pipe & ipi_pipe_id_to_bit(i))
			mtk_cam_req_buffer_done(job, i, -1,
				VB2_BUF_STATE_ERROR, job->timestamp, false);
	}
}

static void job_finalize(struct mtk_cam_job *job)
{
	/* complete it if not applied yet */
	job_complete_sensor_ctrl_obj(job);

	mtk_cam_buffer_pool_return(&job->cq);
	mtk_cam_buffer_pool_return(&job->ipi);
}

static void update_mstream_ufd_offset(struct mtk_cam_pool_buffer *fir_ipi,
		struct mtk_cam_pool_buffer *sec_ipi)
{
	struct mtkcam_ipi_frame_param *fir_fp;
	struct mtkcam_ipi_frame_param *sec_fp;

	fir_fp = (struct mtkcam_ipi_frame_param *)fir_ipi->vaddr;
	sec_fp = (struct mtkcam_ipi_frame_param *)sec_ipi->vaddr;

	sec_fp->img_ufdi_params.rawi2 = fir_fp->img_ufdo_params.imgo;
}

static int compose_mstream(struct mtk_cam_job *job)
{
	struct mtk_cam_mstream_job *mjob =
		container_of(job, struct mtk_cam_mstream_job, job);
	int ret = 0;

	if (mjob->composed_idx == 0) {
		// first exp
		if (job->do_ipi_config && ipi_config(job))
			return -1;

		ret = send_ipi_frame(job, &mjob->ipi, job->frame_seq_no);
	} else {
		update_mstream_ufd_offset(&mjob->ipi, &job->ipi);

		ret = send_ipi_frame(job, &job->ipi,
			next_frame_seq(job->frame_seq_no));
	}

	return ret;
}

static void compose_done_mstream(struct mtk_cam_job *job,
				 struct mtkcam_ipi_frame_ack_result *cq_ret,
				 int compose_ret)
{
	struct mtk_cam_mstream_job *mjob =
		container_of(job, struct mtk_cam_mstream_job, job);

	/* 1st frame */
	if (mjob->composed_idx == 0) {
		mjob->composed_1st = !compose_ret;
		mjob->cq_rst = *cq_ret;
		++mjob->composed_idx;
		return;
	}

	/* 2nd frame */
	job->composed = !compose_ret;
	job->cq_rst = *cq_ret;
	++mjob->composed_idx;

	/* TODO: add compose failed dump for 1st frame */
	if (compose_ret)
		trigger_error_dump(job, MSG_COMPOSE_ERROR);
	else
		normal_dump_if_enable(job);
}

static int apply_sensor_mstream_exp_gain(struct mtk_cam_ctx *ctx,
					 struct mtk_cam_mstream_job *mjob,
					 u8 index)
{
	struct mtk_raw_ctrl_data *ctrl;
	u32 shutter, gain;
	int req_id;
	struct v4l2_ctrl *ae_ctrl;

	// NOTE: idx order of mstream_exp.exposure is fixed
	// due to an agreement with MW
	const int ne_idx = 0;
	const int se_idx = 1;
	int idx, mstream_type =
		mjob->job.job_scen.scen.mstream.type;

	// TODO(Will): refactor
	if (mstream_type == MTK_CAM_MSTREAM_NE_SE) {
		idx = (index == 0) ? ne_idx : se_idx;
	} else if (mstream_type == MTK_CAM_MSTREAM_SE_NE) {
		idx = (index == 0) ? se_idx : ne_idx;
	} else {
		pr_info("%s: unknown mstream type %d", __func__, mstream_type);
		return -1;
	}

	if (WARN_ON(!ctx->sensor))
		return -1;

	ctrl = get_raw_ctrl_data(&mjob->job);
	if (!ctrl) {
		pr_info("failed to get ctrl data\n");
		return -1;
	}

	/* TODO(AY): cache v4l2_ctrl in ctx */
	ae_ctrl = v4l2_ctrl_find(ctx->sensor->ctrl_handler,
				 V4L2_CID_MTK_STAGGER_AE_CTRL);
	if (!ae_ctrl) {
		pr_info("no stagger ae ctrl id in %s\n",
			ctx->sensor->name);
		return -1;
	}

	shutter = ctrl->mstream_exp.exposure[idx].shutter;
	gain = ctrl->mstream_exp.exposure[idx].gain;
	req_id = ctrl->mstream_exp.req_id;

	if (shutter > 0 && gain > 0) {
		struct mtk_hdr_ae ae;

		ae.exposure.le_exposure = shutter;
		ae.gain.le_gain = gain;
		ae.req_id = req_id;
		ae.subsample_tags = index + 1;

		v4l2_ctrl_s_ctrl_compound(ae_ctrl, V4L2_CTRL_TYPE_U32, &ae);
	}

	//pr_info("%s-%u: shutter %u gain %u req_id %d\n",
	//  __func__, index, shutter, gain, req_id);
	return 0;
}

static int apply_sensor_mstream(struct mtk_cam_job *job)
{
	struct mtk_cam_mstream_job *mjob =
		container_of(job, struct mtk_cam_mstream_job, job);
	struct mtk_cam_ctx *ctx = job->src_ctx;
	struct mtk_cam_device *cam = ctx->cam;
	struct mtk_cam_request *req = job->req;
	u8 cur_idx;
	bool do_request_setup;

	cur_idx = mjob->apply_sensor_idx;
	do_request_setup = (cur_idx == 0) && job->sensor_hdl_obj;

	frame_sync_start(job);

	if (check_update_mstream_mode(job) && (cur_idx == 0))
		mtk_cam_set_sensor_mstream_mode(ctx, 1);

	apply_sensor_mstream_exp_gain(ctx, mjob, cur_idx);

	if (do_request_setup)
		v4l2_ctrl_request_setup(&req->req, job->sensor->ctrl_handler);

	frame_sync_end(job);

	mtk_cam_job_state_set(&job->job_state,
			cur_idx == 0 ? SENSOR_1ST_STATE : SENSOR_2ND_STATE,
			S_SENSOR_APPLIED);

	if (do_request_setup)
		job_complete_sensor_ctrl_obj(job);

	dev_info(cam->dev, "[%s] ctx:%d, job: seq 0x%x\n",
		 __func__, ctx->stream_id, job->frame_seq_no + cur_idx);
	++mjob->apply_sensor_idx;
	return 0;
}

static int apply_cq_mstream(struct mtk_cam_job *job)
{
	struct mtk_cam_mstream_job *mjob =
		container_of(job, struct mtk_cam_mstream_job, job);
	int ret;

	if (mjob->apply_isp_idx == 0)
		ret = apply_engines_cq(job, job->frame_seq_no,
				       &mjob->cq, &mjob->cq_rst);
	else
		ret = apply_engines_cq(job, next_frame_seq(job->frame_seq_no),
				       &job->cq, &job->cq_rst);

	++mjob->apply_isp_idx;
	return ret;
}

static int job_mark_engine_done_mstream(struct mtk_cam_job *job,
					int engine_type, int engine_id,
					int seq_no)
{
	/* 1st frame */
	if (seq_no == job->frame_seq_no) {
		if (engine_type != CAMSYS_ENGINE_RAW) {

			/* Note:
			 * disable this log since we run camsv/mraw for both
			 * frames now
			 *
			 * pr_info("%s: warn. why does engine %d-%d has done\n",
			 *     __func__, engine_type, engine_id);
			 */
			return 0;
		}
		return 1;
	}

	/* 2nd frame */
	return job_mark_engine_done(job, engine_type, engine_id, seq_no);
}

static void job_finalize_mstream(struct mtk_cam_job *job)
{
	struct mtk_cam_mstream_job *mjob =
		container_of(job, struct mtk_cam_mstream_job, job);

	job_finalize(job);
	mtk_cam_buffer_pool_return(&mjob->cq);
	mtk_cam_buffer_pool_return(&mjob->ipi);
}

static void log_transit(struct mtk_cam_job_state *s, int state_type,
			int old_state, int new_state, int act)
{
	if (CAM_DEBUG_ENABLED(STATE))
		pr_info("%s: #%d %s: %s -> %s, act %d\n",
			__func__, s->seq_no,
			str_state_type(state_type),
			str_state(state_type, old_state),
			str_state(state_type, new_state),
			act);
}

static void singleframe_on_transit(struct mtk_cam_job_state *s, int state_type,
				   int old_state, int new_state, int act,
				   struct mtk_cam_ctrl_runtime_info *info)
{
	struct mtk_cam_job *job =
		container_of(s, struct mtk_cam_job, job_state);

	log_transit(s, state_type, old_state, new_state, act);

	if (state_type == ISP_STATE) {

		switch (new_state) {

		case S_ISP_COMPOSED:
			complete(&job->compose_completion);
			break;

		case S_ISP_OUTER:
			complete(&job->cq_exe_completion);
			break;

		case S_ISP_PROCESSING:
			if (old_state != S_ISP_PROCESSING) {
				job->timestamp = info->sof_ts_ns;
				job->timestamp_mono = ktime_get_ns(); /* FIXME */

				job->hdr_ts_cache.le = info->sof_ts_ns;
				job->hdr_ts_cache.le_mono = info->sof_ts_mono_ns;
				job->hdr_ts_cache.se = info->sof_l_ts_ns;
				job->hdr_ts_cache.se_mono = info->sof_l_ts_mono_ns;
			}
			break;
		}
	}
}

static void mstream_on_transit(struct mtk_cam_job_state *s, int state_type,
				   int old_state, int new_state, int act,
				   struct mtk_cam_ctrl_runtime_info *info)
{
	struct mtk_cam_job *job =
		container_of(s, struct mtk_cam_job, job_state);

	log_transit(s, state_type, old_state, new_state, act);

	if (state_type == ISP_STATE) {

		switch (new_state) {

		case S_ISP_COMPOSED:
			complete(&job->compose_completion);
			break;

		case S_ISP_OUTER:
			complete(&job->cq_exe_completion);
			break;

		case S_ISP_PROCESSING:
			if (old_state != S_ISP_PROCESSING) {
				job->timestamp = info->sof_ts_ns;
				job->timestamp_mono = ktime_get_ns(); /* FIXME */

				if (job->hdr_ts_cache.le) {
					job->hdr_ts_cache.se = info->sof_ts_ns;
					job->hdr_ts_cache.se_mono = info->sof_ts_mono_ns;
				} else {
					job->hdr_ts_cache.le = info->sof_ts_ns;
					job->hdr_ts_cache.le_mono = info->sof_ts_mono_ns;
				}
			}
			break;
		}
	}
}

static void m2m_on_transit(struct mtk_cam_job_state *s, int state_type,
			   int old_state, int new_state, int act,
			   struct mtk_cam_ctrl_runtime_info *info)
{
	struct mtk_cam_job *job =
		container_of(s, struct mtk_cam_job, job_state);

	log_transit(s, state_type, old_state, new_state, act);

	if (act == ACTION_TRIGGER) {
		job->timestamp = ktime_get_boottime_ns();
		job->timestamp_mono = ktime_get_ns(); /* FIXME */
	}
}

static struct mtk_cam_job_ops otf_job_ops = {
	.cancel = job_cancel,
	.dump = job_dump,
	.finalize = job_finalize,
	.compose_done = _compose_done,
	.compose = _compose,
	.stream_on = _stream_on,
	//.reset
	.apply_sensor = _apply_sensor,
	.apply_isp = _apply_cq,
	.mark_afo_done = job_mark_afo_done,
	.mark_engine_done = job_mark_engine_done,
	.apply_switch = _apply_switch,
	.dump_aa_info = job_dump_aa_info,
};

static struct mtk_cam_job_ops otf_stagger_job_ops = {
	.cancel = job_cancel,
	.dump = job_dump,
	.finalize = job_finalize,
	.compose_done = _compose_done,
	.compose = _compose,
	.stream_on = _stream_on,
	//.reset
	.apply_sensor = _apply_sensor,
	.apply_isp = _apply_cq,
	.mark_afo_done = job_mark_afo_done,
	.mark_engine_done = job_mark_engine_done,
	.apply_switch = _apply_switch,
	.dump_aa_info = job_dump_aa_info,
};

static struct mtk_cam_job_ops m2m_job_ops = {
	.cancel = job_cancel,
	.dump = job_dump,
	.finalize = job_finalize,
	.compose_done = _compose_done,
	.compose = _compose,
	.stream_on = 0,
	//.reset
	.apply_sensor = 0,
	.apply_isp = _apply_cq,
	.trigger_isp = trigger_m2m,
	.mark_afo_done = job_mark_afo_done,
	.mark_engine_done = job_mark_engine_done,
	.dump_aa_info = 0,
};

static struct mtk_cam_job_ops mstream_job_ops = {
	.cancel = job_cancel,
	.dump = job_dump_mstream,
	.finalize = job_finalize_mstream,
	.compose_done = compose_done_mstream,
	.compose = compose_mstream,
	.stream_on = _stream_on,
	//.reset
	.apply_sensor = apply_sensor_mstream,
	.apply_isp = apply_cq_mstream,
	.mark_afo_done = job_mark_afo_done,
	.mark_engine_done = job_mark_engine_done_mstream,
	.dump_aa_info = job_dump_aa_info,
};

static struct mtk_cam_job_ops otf_only_sv_job_ops = {
	.cancel = job_cancel,
	//.dump
	.finalize = job_finalize,
	.compose_done = _compose_done,
	.compose = _compose,
	.stream_on = _stream_on_only_sv,
	//.reset
	.apply_sensor = _apply_sensor,
	.apply_isp = _apply_cq,
	.mark_engine_done = job_mark_engine_done,
	.dump_aa_info = 0,
};

static struct mtk_cam_job_state_cb sf_state_cb = {
	.on_transit = singleframe_on_transit,
};

static struct mtk_cam_job_state_cb mstream_state_cb = {
	.on_transit = mstream_on_transit,
};

static struct mtk_cam_job_state_cb m2m_state_cb = {
	.on_transit = m2m_on_transit,
};

static struct pack_job_ops_helper subsample_pack_helper = {
	.pack_job = _job_pack_subsample,
	.update_raw_bufs_to_ipi = fill_raw_img_buffer_to_ipi_frame,
	.update_raw_rawi_to_ipi = NULL,
	.update_raw_imgo_to_ipi = fill_imgo_img_buffer_to_ipi_frame_subsample,
	.update_raw_yuvo_to_ipi = fill_yuvo_img_buffer_to_ipi_frame_subsample,
	.append_work_buf_to_ipi = NULL,
};

static struct pack_job_ops_helper otf_pack_helper = {
	.pack_job = _job_pack_normal,
	.update_raw_bufs_to_ipi = fill_raw_img_buffer_to_ipi_frame,
	.update_raw_rawi_to_ipi = NULL,
	.update_raw_imgo_to_ipi = fill_imgo_buf_to_ipi_normal,
	.update_raw_yuvo_to_ipi = NULL,
	.append_work_buf_to_ipi = update_work_buffer_to_ipi_frame,
};

static struct pack_job_ops_helper stagger_pack_helper = {
	.pack_job = _job_pack_otf_stagger,
	.update_raw_bufs_to_ipi = fill_raw_img_buffer_to_ipi_frame,
	.update_raw_rawi_to_ipi = fill_img_in_by_exposure,
	.update_raw_imgo_to_ipi = fill_imgo_buf_to_ipi_stagger,
	.update_raw_yuvo_to_ipi = NULL,
	.append_work_buf_to_ipi = update_work_buffer_to_ipi_frame,
};

static struct pack_job_ops_helper m2m_pack_helper = {
	.pack_job = _job_pack_m2m,
	.update_raw_bufs_to_ipi = fill_raw_img_buffer_to_ipi_frame,
	.update_raw_rawi_to_ipi = fill_m2m_rawi_to_img_in_ipi,
	.update_raw_imgo_to_ipi = fill_m2m_imgo_to_img_out_ipi,
	.update_raw_yuvo_to_ipi = NULL,
	.append_work_buf_to_ipi = NULL,
};

/* TODO(AY): on-going */
static struct pack_job_ops_helper mstream_pack_helper = {
	.job_init = job_init_mstream,
	.pack_job = _job_pack_mstream,
	.update_raw_bufs_to_ipi = fill_raw_img_buffer_to_ipi_frame,
	.update_raw_rawi_to_ipi = NULL,
	.update_raw_imgo_to_ipi = fill_imgo_buf_to_ipi_mstream,
	.update_raw_yuvo_to_ipi = NULL,
	.append_work_buf_to_ipi = update_work_buffer_to_ipi_frame,
};

static struct pack_job_ops_helper only_sv_pack_helper = {
	.pack_job = _job_pack_only_sv,
};

static void update_job_sensor(struct mtk_cam_job *job)
{
	struct mtk_cam_ctx *ctx = job->src_ctx;
	struct mtk_raw_pipeline *raw;
	struct mtk_raw_ctrl_data *ctrl_data = get_raw_ctrl_data(job);

	if (!ctx->sensor) {
		job->sensor = NULL;
		job->seninf = NULL;
		job->seninf_prev = NULL;
		return;
	}

	if (ctx->raw_subdev_idx < 0) {
		/* Non-raw's engine case, we use the ctx's sensor */
		job->sensor = ctx->sensor;
		job->seninf = ctx->seninf;
		job->seninf_prev = ctx->seninf;
		return;
	}

	raw = &ctx->cam->pipelines.raw[ctx->raw_subdev_idx];
	if (!raw->sensor || !raw->seninf) {
		/* TODO: warn this case */
		dev_info(ctx->cam->dev,
			 "%s:pipe(%d): cached sensor not found, but ctx has sensor\n",
			 __func__, raw->id);
		job->sensor = ctx->sensor;
		job->seninf = ctx->seninf;
		job->seninf_prev = ctx->seninf;
		return;
	}

	job->seninf_prev = raw->seninf;
	/* update the cached sensor and seninf */
	if (ctrl_data && ctrl_data->sensor && ctrl_data->seninf) {
		raw->seninf = ctrl_data->seninf;
		raw->sensor = ctrl_data->sensor;
	}

	job->sensor = raw->sensor;
	job->seninf = raw->seninf;
}

struct initialize_params stagger_init = {
	.master_raw_init = master_raw_set_stagger,
};

struct initialize_params subsample_init = {
	.master_raw_init = master_raw_set_subsample,
};

static int job_factory(struct mtk_cam_job *job)
{
	struct mtk_cam_ctx *ctx = job->src_ctx;
	struct pack_job_ops_helper *pack_helper = NULL;
	int ret;

	/* only job used data */
	job->ctx_id = ctx->stream_id;

	/**
	 * set job->senosor, job->seninf, job->seninf_prev
	 * and job->job->raw_switch
	 */
	update_job_sensor(job);

	job->sensor_hdl_obj = job->sensor ?
		mtk_cam_req_find_ctrl_obj(job->req, job->sensor->ctrl_handler) :
		NULL;
	job->composed = false;
	job->seamless_switch = false;
	job->first_frm_switch = false;
	job->switch_type = EXPOSURE_CHANGE_NONE;
	job->scq_period = SCQ_DEADLINE_MS;
	init_completion(&job->compose_completion);
	init_completion(&job->cq_exe_completion);

	memset(&job->hdr_ts_cache, 0, sizeof(job->hdr_ts_cache));
	job->init_params = NULL;

	switch (job->job_type) {
	case JOB_TYPE_BASIC:
		pack_helper = &otf_pack_helper;

		mtk_cam_job_state_init_basic(&job->job_state, &sf_state_cb,
					     !!job->sensor_hdl_obj);
		job->ops = &otf_job_ops;
		break;
	case JOB_TYPE_STAGGER:
		pack_helper = &stagger_pack_helper;

		mtk_cam_job_state_init_basic(&job->job_state, &sf_state_cb,
					     !!job->sensor_hdl_obj);
		job->ops = &otf_stagger_job_ops;
		job->init_params = &stagger_init;
		break;
	case JOB_TYPE_M2M:
		pack_helper = &m2m_pack_helper;

		mtk_cam_job_state_init_m2m(&job->job_state, &m2m_state_cb);
		job->ops = &m2m_job_ops;
		break;
	case JOB_TYPE_MSTREAM:
		pack_helper = &mstream_pack_helper;

		mtk_cam_job_state_init_mstream(&job->job_state, &mstream_state_cb);
		job->ops = &mstream_job_ops;
		break;
	case JOB_TYPE_HW_SUBSAMPLE:
		pack_helper = &subsample_pack_helper;

		mtk_cam_job_state_init_subsample(&job->job_state, &sf_state_cb,
					     !!job->sensor_hdl_obj);
		job->ops = &otf_job_ops;
		job->init_params = &subsample_init;
		break;
	case JOB_TYPE_ONLY_SV:
		pack_helper = &only_sv_pack_helper;

		mtk_cam_job_state_init_basic(&job->job_state, &sf_state_cb,
					     !!job->sensor_hdl_obj);
		job->ops = &otf_only_sv_job_ops;
		break;
	default:
		pr_info("%s: job type %d not ready\n", __func__, job->job_type);
		break;
	}

	if (WARN_ON(!pack_helper))
		return -1;

	if (pack_helper->job_init && pack_helper->job_init(job))
		return -1;

	ret = pack_helper->pack_job(job, pack_helper);

	if (CAM_DEBUG_ENABLED(JOB))
		pr_info("[%s] ctx:%d|type:%d|scen_id:%d|exp(cur:%d,prev:%d)|sw/scene:%d/%d",
				__func__,
				ctx->stream_id, job->job_type, job->job_scen.id,
				job_exp_num(job), job_prev_exp_num(job),
				get_sw_feature(job), get_hw_scenario(job));

	return ret;
}

int mtk_cam_job_pack(struct mtk_cam_job *job, struct mtk_cam_ctx *ctx,
		     struct mtk_cam_request *req)
{
	int ret;

	ret = mtk_cam_job_pack_init(job, ctx, req);
	if (ret)
		return ret;
	// update job's feature
	ret = update_job_type_feature(job);
	if (ret)
		return ret;

	ret = job_factory(job);

	return ret;
}

static void ipi_add_hw_map(struct mtkcam_ipi_config_param *config,
				   int pipe_id, int dev_mask)
{
	unsigned int n_maps = config->n_maps;

	if (WARN_ON(n_maps >= ARRAY_SIZE(config->maps)))
		return;

	WARN_ON(!dev_mask);

	config->maps[n_maps] = (struct mtkcam_ipi_hw_mapping) {
		.pipe_id = pipe_id,
		.dev_mask = dev_mask,
#ifdef CHECK_LATER
		.exp_order = 0
#endif
	};
	config->n_maps++;
}

static int raw_set_ipi_input_param(struct mtkcam_ipi_input_param *input,
				   struct mtk_raw_sink_data *sink,
				   int pixel_mode, int dc_sv_pixel_mode,
				   int subsample)
{
	input->fmt = sensor_mbus_to_ipi_fmt(sink->mbus_code);
	input->raw_pixel_id = sensor_mbus_to_ipi_pixel_id(sink->mbus_code);
	input->data_pattern = MTKCAM_IPI_SENSOR_PATTERN_NORMAL;
	input->pixel_mode = pixel_mode;
	input->pixel_mode_before_raw = dc_sv_pixel_mode;
	input->subsample = subsample - 1; /* TODO(AY): remove -1 */
	input->in_crop = v4l2_rect_to_ipi_crop(&sink->crop);

	return 0;
}

static int mraw_set_ipi_input_param(struct mtkcam_ipi_input_param *input,
				   struct mtk_mraw_sink_data *sink,
				   int pixel_mode, int dc_sv_pixel_mode,
				   int subsample)
{
	input->fmt = sensor_mbus_to_ipi_fmt(sink->mbus_code);
	input->raw_pixel_id = sensor_mbus_to_ipi_pixel_id(sink->mbus_code);
	input->data_pattern = MTKCAM_IPI_SENSOR_PATTERN_NORMAL;
	input->pixel_mode = pixel_mode;
	input->pixel_mode_before_raw = dc_sv_pixel_mode;
	input->subsample = subsample - 1; /* TODO(AY): remove -1 */
	input->in_crop = v4l2_rect_to_ipi_crop(&sink->crop);

	return 0;
}

static int update_frame_order_to_config(struct mtk_cam_scen *scen,
				       struct mtkcam_ipi_config_param *config)
{
	switch (scen->scen.normal.frame_order) {
	case MTK_CAM_FRAME_BAYER_W:
		config->frame_order = MTKCAM_IPI_ORDER_BAYER_FIRST;
		break;
	case MTK_CAM_FRAME_W_BAYER:
	default:
		config->frame_order = MTKCAM_IPI_ORDER_W_FIRST;
		break;
	}

	if (CAM_DEBUG_ENABLED(IPI_BUF))
		pr_info("%s: scen id %d frame order: %d\n",
			__func__, scen->id,
			config->frame_order);

	return 0;
}

// TODO: should get vsync order from user param rather
// than from seninf subdev?
static int update_vsync_order_to_config(struct mtk_cam_ctx *ctx,
						struct mtk_cam_scen *scen,
						struct mtkcam_ipi_config_param *config)
{
	config->vsync_order = (ctx->seninf) ?
		mtk_cam_seninf_get_vsync_order(ctx->seninf) :
		MTKCAM_IPI_ORDER_BAYER_FIRST;

	if (CAM_DEBUG_ENABLED(IPI_BUF))
		pr_info("%s: scen id %d vsync order: %d\n",
			__func__, scen->id,
			config->vsync_order);

	return 0;
}

static int update_scen_order_to_config(struct mtk_cam_scen *scen,
				       struct mtkcam_ipi_config_param *config)
{
	config->exp_order = get_exp_order(scen);

	if (CAM_DEBUG_ENABLED(IPI_BUF))
		pr_info("%s: scen id %d exp order: %d\n",
			__func__, scen->id,
			config->exp_order);

	return 0;
}

static int mtk_cam_job_fill_ipi_config(struct mtk_cam_job *job,
				       struct mtkcam_ipi_config_param *config)
{
	struct mtk_cam_request *req = job->req;
	struct mtk_cam_ctx *ctx = job->src_ctx;
	int used_engine = ctx->used_engine;
	struct mtkcam_ipi_input_param *input = &config->input;
	struct mtkcam_ipi_sv_input_param *sv_input;
	struct mtkcam_ipi_mraw_input_param *mraw_input;
	int i;

	memset(config, 0, sizeof(*config));

	/* assume: at most one raw-subdev is used */
	if (ctx->has_raw_subdev) {
		struct mtk_raw_sink_data *sink = get_raw_sink_data(job);
		struct mtk_raw_ctrl_data *ctrl = get_raw_ctrl_data(job);
		int raw_dev;

		if (WARN_ON(!sink || !ctrl))
			return -1;

		// TODO(Will): seamless at first frame?
		config->flags = (job->seamless_switch || job->raw_switch) ?
			MTK_CAM_IPI_CONFIG_TYPE_INPUT_CHANGE :
			MTK_CAM_IPI_CONFIG_TYPE_INIT;

		config->sw_feature = get_sw_feature(job);

		update_scen_order_to_config(&job->job_scen, config);
		update_frame_order_to_config(&job->job_scen, config);
		update_vsync_order_to_config(ctx, &job->job_scen, config);

		if (job->job_scen.scen.normal.w_chn_supported) {
			config->w_cac_table.iova = ctx->w_caci_buf.daddr;
			config->w_cac_table.size = ctx->w_caci_buf.size;
		}

		raw_set_ipi_input_param(input, sink,
			ctrl->resource.tgo_pxl_mode,
			ctrl->resource.tgo_pxl_mode_before_raw,
			job->sub_ratio); /* TODO */

		raw_dev = (int)bit_map_subset_of(MAP_HW_RAW, used_engine);
		ipi_add_hw_map(config, MTKCAM_SUBDEV_RAW_0, raw_dev);
	}

	/* camsv */
	if (ctx->hw_sv) {
		struct mtk_camsv_device *sv_dev = dev_get_drvdata(ctx->hw_sv);

		for (i = SVTAG_START; i < SVTAG_END; i++) {
			if (job->enabled_tags & (1 << i)) {
				sv_input = &config->sv_input[0][i];

				sv_input->pipe_id = sv_dev->id + MTKCAM_SUBDEV_CAMSV_START;
				sv_input->tag_id = i;
				sv_input->tag_order = job->tag_info[i].tag_order;
				sv_input->is_first_frame =
					(!ctx->not_first_job || job->raw_switch) ? 1 : 0;
				sv_input->input = job->ipi_config.sv_input[0][i].input;
			}
		}
	}

	/* mraw */
	for (i = 0; i < ctx->num_mraw_subdevs; i++) {
		struct mtk_mraw_sink_data *sink =
			&req->mraw_data[ctx->mraw_subdev_idx[i]].sink;
		struct mtk_mraw_pipeline *pipe =
			&ctx->cam->pipelines.mraw[ctx->mraw_subdev_idx[i]];

		mraw_input = &config->mraw_input[i];
		mraw_input->pipe_id =
			ctx->mraw_subdev_idx[i] + MTKCAM_SUBDEV_MRAW_START;

		pipe->res_config.tg_crop = v4l2_rect_to_ipi_crop(&sink->crop);
		pipe->res_config.tg_fmt = sensor_mbus_to_ipi_pixel_id(sink->mbus_code);
		pipe->res_config.pixel_mode = MRAW_TG_PIXEL_MODE;
		atomic_set(&pipe->res_config.is_fmt_change, 1);

		mraw_set_ipi_input_param(&mraw_input->input,
			sink, MRAW_TG_PIXEL_MODE, 1, job->sub_ratio);
	}

	return 0;
}

static int mtk_cam_job_fill_ipi_config_only_sv(struct mtk_cam_job *job,
				       struct mtkcam_ipi_config_param *config)
{
	struct mtk_cam_ctx *ctx = job->src_ctx;
	struct mtk_camsv_device *sv_dev = dev_get_drvdata(ctx->hw_sv);
	struct mtkcam_ipi_sv_input_param *sv_input;
	int i;

	memset(config, 0, sizeof(*config));

	config->flags = MTK_CAM_IPI_CONFIG_TYPE_INIT;
	config->sw_feature = get_sw_feature(job);

	for (i = SVTAG_START; i < SVTAG_END; i++) {
		if (job->enabled_tags & (1 << i)) {
			sv_input = &config->sv_input[0][i];

			sv_input->pipe_id = sv_dev->id + MTKCAM_SUBDEV_CAMSV_START;
			sv_input->tag_id = i;
			sv_input->tag_order = job->tag_info[i].tag_order;
			sv_input->is_first_frame = (ctx->not_first_job) ? 0 : 1;
			sv_input->input = job->ipi_config.sv_input[0][i].input;
		}
	}

	return 0;
}

static int update_cq_buffer_to_ipi_frame(struct mtk_cam_pool_buffer *cq,
					 struct mtkcam_ipi_frame_param *fp)
{
	/* cq offset */
	fp->cur_workbuf_offset = cq->size * cq->priv.index;
	fp->cur_workbuf_size = cq->size;
	return 0;
}

static int map_ipi_bin_flag(int bin)
{
	int bin_flag;

	switch (bin) {
	case MTK_CAM_BIN_OFF:
		bin_flag = BIN_OFF;
		break;
	case MTK_CAM_BIN_ON:
		bin_flag = BIN_ON;
		break;
	case MTK_CAM_CBN_2X2_ON:
		bin_flag = CBN_2X2_ON;
		break;
	case MTK_CAM_CBN_3X3_ON:
		bin_flag = CBN_3X3_ON;
		break;
	case MTK_CAM_CBN_4X4_ON:
		bin_flag = CBN_4X4_ON;
		break;
	case MTK_CAM_QBND_ON:
		bin_flag = QBND_ON;
		break;
	default:
		WARN_ON(1);
		pr_info("%s: failed to map bin flag: bin(%d)",
				__func__, bin);
		bin_flag = BIN_OFF;
		break;
	}

	return bin_flag;
}

static int update_adl_param(struct mtk_cam_job *job,
			    struct mtk_raw_ctrl_data *ctrl,
			    struct mtkcam_ipi_adl_frame_param *adl_fp)
{
	struct mtk_cam_ctx *ctx = job->src_ctx;
	struct mtk_cam_apu_info *apu_info = &ctrl->apu_info;

	adl_fp->vpu_i_point = map_ipi_vpu_point(apu_info->vpu_i_point);
	adl_fp->vpu_o_point = map_ipi_vpu_point(apu_info->vpu_o_point);
	adl_fp->sysram_en = apu_info->sysram_en;
	adl_fp->block_y_size = apu_info->block_y_size;
	adl_fp->slb_addr = (__u64)ctx->slb_addr;
	adl_fp->slb_size = ctx->slb_size;

	pr_info("%s: vpu i/o %d/%d sram %d ysize %d\n",
		__func__,
		adl_fp->vpu_i_point,
		adl_fp->vpu_o_point,
		adl_fp->sysram_en,
		adl_fp->block_y_size);
	return 0;
}

static int update_job_raw_param_to_ipi_frame(struct mtk_cam_job *job,
					     struct mtkcam_ipi_frame_param *fp)
{
	struct mtkcam_ipi_raw_frame_param *p = &fp->raw_param;
	struct mtk_raw_ctrl_data *ctrl;

	ctrl = get_raw_ctrl_data(job);

	if (!ctrl)
		return 0;

	p->imgo_path_sel = map_ipi_imgo_path(ctrl->raw_path);
	p->hardware_scenario = get_hw_scenario(job);
	p->bin_flag = map_ipi_bin_flag(ctrl->resource.user_data.raw_res.bin);
	p->exposure_num = job_exp_num(job);
	p->previous_exposure_num = job_prev_exp_num(job);

	if (is_m2m_apu(job))
		update_adl_param(job, ctrl, &fp->adl_param);

	if (CAM_DEBUG_ENABLED(IPI_BUF))
		pr_info("[%s] job_type:%d scen:%d exp:%d/%d raw_path:%d", __func__,
			job->job_type,
			ctrl->resource.user_data.raw_res.scen.id,
			p->exposure_num, p->previous_exposure_num,
			p->imgo_path_sel);
	return 0;
}

static int update_raw_image_buf_to_ipi_frame(struct req_buffer_helper *helper,
		struct mtk_cam_buffer *buf, struct mtk_cam_video_device *node,
		struct pack_job_ops_helper *job_helper)
{
	int (*update_fn)(struct req_buffer_helper *helper,
			 struct mtk_cam_buffer *buf,
			 struct mtk_cam_video_device *node);

	update_fn = job_helper->update_raw_bufs_to_ipi;

	switch (node->desc.dma_port) {
	case MTKCAM_IPI_RAW_RAWI_2:
		if (job_helper->update_raw_rawi_to_ipi)
			update_fn = job_helper->update_raw_rawi_to_ipi;
		break;
	case MTKCAM_IPI_RAW_IMGO:  /* main stream + pure raw */
		if (job_helper->update_raw_imgo_to_ipi)
			update_fn = job_helper->update_raw_imgo_to_ipi;
		break;
	case MTKCAM_IPI_RAW_YUVO_1:
	case MTKCAM_IPI_RAW_YUVO_2:
	case MTKCAM_IPI_RAW_YUVO_3:
	case MTKCAM_IPI_RAW_YUVO_4:
	case MTKCAM_IPI_RAW_YUVO_5:
	case MTKCAM_IPI_RAW_RZH1N2TO_1:
	case MTKCAM_IPI_RAW_RZH1N2TO_2:
	case MTKCAM_IPI_RAW_RZH1N2TO_3:
	case MTKCAM_IPI_RAW_DRZS4NO_1:
	case MTKCAM_IPI_RAW_DRZS4NO_3:
	case MTKCAM_IPI_RAW_DRZB2NO_1:
		if (job_helper->update_raw_yuvo_to_ipi)
			update_fn = job_helper->update_raw_yuvo_to_ipi;
		break;
	default:
		pr_info("%s %s: not supported port: %d\n",
			__FILE__, __func__, node->desc.dma_port);
	}

	return update_fn(helper, buf, node);
}

static int update_sv_image_buf_to_ipi_frame(struct req_buffer_helper *helper,
		struct mtk_cam_buffer *buf, struct mtk_cam_video_device *node,
		struct pack_job_ops_helper *job_helper)
{
	struct mtk_cam_ctx *ctx = helper->job->src_ctx;
	int ret = -1;

	switch (node->desc.id) {
	case MTK_CAMSV_MAIN_STREAM_OUT:
		if (mtk_cam_is_display_ic(ctx))
			ret = fill_sv_img_buffer_to_ipi_frame_display_ic(helper, buf, node);
		else
			ret = fill_sv_img_buffer_to_ipi_frame(helper, buf, node);
		break;
	case MTK_CAMSV_EXT_STREAM_OUT:
		ret = fill_sv_ext_img_buffer_to_ipi_frame_display_ic(helper, buf, node);
		break;
	default:
		pr_info("%s %s: not supported port: %d\n",
			__FILE__, __func__, node->desc.dma_port);
	}

	return ret;
}

#define FILL_META_IN_OUT(_ipi_meta, _cam_buf, _uid)		\
{								\
	typeof(_ipi_meta) _m = (_ipi_meta);			\
	typeof(_cam_buf) _b = (_cam_buf);			\
								\
	_m->buf.ccd_fd = _b->vbb.vb2_buf.planes[0].m.fd;	\
	_m->buf.size = _b->meta_info.buffersize;		\
	_m->buf.iova = _b->daddr;				\
	_m->uid = _uid;					\
}

static int update_mraw_meta_buf_to_ipi_frame(
		struct req_buffer_helper *helper,
		struct mtk_cam_buffer *buf,
		struct mtk_cam_video_device *node,
		struct pack_job_ops_helper *job_helper)
{
	struct mtk_cam_ctx *ctx = helper->job->src_ctx;
	struct mtkcam_ipi_frame_param *fp = helper->fp;
	struct mtk_mraw_pipeline *mraw_pipe = NULL;
	int ret = 0, i, param_idx = -1;

	for (i = 0; i < ctx->num_mraw_subdevs; i++) {
		mraw_pipe = &ctx->cam->pipelines.mraw[ctx->mraw_subdev_idx[i]];
		if (mraw_pipe->id == node->uid.pipe_id) {
			param_idx = i;
			break;
		}
	}

	if (param_idx < 0 || param_idx >= ARRAY_SIZE(fp->mraw_param)) {
		ret = -1;
		pr_info("%s %s: mraw subdev idx not found(pipe_id:%d)\n",
			__FILE__, __func__, node->uid.pipe_id);
		goto EXIT;
	}

	switch (node->desc.dma_port) {
	case MTKCAM_IPI_MRAW_META_STATS_CFG:
		{
			struct mtkcam_ipi_meta_input *in;
			void *vaddr;

			in = &fp->mraw_param[param_idx].mraw_meta_inputs;
			FILL_META_IN_OUT(in, buf, node->uid);

			vaddr = vb2_plane_vaddr(&buf->vbb.vb2_buf, 0);
			mraw_pipe->res_config.vaddr[MTKCAM_IPI_MRAW_META_STATS_CFG
				- MTKCAM_IPI_MRAW_ID_START] = vaddr;
			mraw_pipe->res_config.daddr[MTKCAM_IPI_MRAW_META_STATS_CFG
				- MTKCAM_IPI_MRAW_ID_START] = buf->daddr;
			mtk_cam_mraw_copy_user_input_param(ctx->cam, vaddr, mraw_pipe);
			mraw_pipe->res_config.enque_num++;
		}
		break;
	case MTKCAM_IPI_MRAW_META_STATS_0:
		{
			void *vaddr;

			vaddr = vb2_plane_vaddr(&buf->vbb.vb2_buf, 0);
			mraw_pipe->res_config.vaddr[MTKCAM_IPI_MRAW_META_STATS_0
				- MTKCAM_IPI_MRAW_ID_START] = vaddr;
			mraw_pipe->res_config.daddr[MTKCAM_IPI_MRAW_META_STATS_0
				- MTKCAM_IPI_MRAW_ID_START] = buf->daddr;
			mraw_pipe->res_config.enque_num++;
		}
		break;
	default:
		pr_info("%s %s: not supported port: %d\n",
			__FILE__, __func__, node->desc.dma_port);
	}

	if (mraw_pipe->res_config.enque_num == MTK_MRAW_TOTAL_NODES) {
		struct mtk_mraw_sink_data *sink =
			&helper->job->req->mraw_data[ctx->mraw_subdev_idx[param_idx]].sink;
		mtk_cam_mraw_cal_cfg_info(ctx->cam,
			node->uid.pipe_id, &fp->mraw_param[param_idx],
			sensor_mbus_to_ipi_fmt(sink->mbus_code));
		mraw_pipe->res_config.enque_num = 0;
	}
EXIT:
	return ret;
}

static int update_raw_meta_buf_to_ipi_frame(struct req_buffer_helper *helper,
					    struct mtk_cam_buffer *buf,
					    struct mtk_cam_video_device *node)
{
	struct mtkcam_ipi_frame_param *fp = helper->fp;
	int ret = 0;

	switch (node->desc.dma_port) {
	case MTKCAM_IPI_RAW_META_STATS_CFG:
		{
			struct mtkcam_ipi_meta_input *in;

			in = &fp->meta_inputs[helper->mi_idx];
			++helper->mi_idx;

			FILL_META_IN_OUT(in, buf, node->uid);

			/* cache info for later */
			helper->meta_cfg_buf = buf;
		}
		break;
	case MTKCAM_IPI_RAW_META_STATS_0:
	case MTKCAM_IPI_RAW_META_STATS_1:
		{
			struct mtkcam_ipi_meta_output *out;

			out = &fp->meta_outputs[helper->mo_idx];
			++helper->mo_idx;

			FILL_META_IN_OUT(out, buf, node->uid);

			/* cache info for later */
			if (node->desc.dma_port == MTKCAM_IPI_RAW_META_STATS_0) {
				helper->meta_stats0_buf = buf;
			} else
				helper->meta_stats1_buf = buf;
		}
		break;
	default:
		pr_info("%s %s: not supported port: %d\n",
			__FILE__, __func__, node->desc.dma_port);
		ret = -1;
	}

	WARN_ON(ret);
	return ret;
}

static int update_cam_buf_to_ipi_frame(struct req_buffer_helper *helper,
	struct mtk_cam_buffer *buf, struct pack_job_ops_helper *job_helper)
{
	struct mtk_cam_video_device *node;
	int pipe_id;
	int ret = -1;

	node = mtk_cam_buf_to_vdev(buf);
	pipe_id = node->uid.pipe_id;

	/* skip if it does not belong to current ctx */
	if (!belong_to_current_ctx(helper->job, pipe_id))
		return 0;

	if (CAM_DEBUG_ENABLED(IPI_BUF))
		pr_info("%s pipe %x buf %s\n",
			__func__, pipe_id, node->desc.name);

	if (is_raw_subdev(pipe_id)) {
		if (node->desc.image)
			ret = update_raw_image_buf_to_ipi_frame(helper,
								buf, node, job_helper);
		else
			ret = update_raw_meta_buf_to_ipi_frame(helper,
							       buf, node);
	}

	if (is_camsv_subdev(pipe_id)) {
		ret = update_sv_image_buf_to_ipi_frame(helper,
							buf, node, job_helper);
	}

	if (is_mraw_subdev(pipe_id)) {
		ret = update_mraw_meta_buf_to_ipi_frame(helper,
							buf, node, job_helper);
	}

	if (ret)
		pr_info("failed to update pipe %x buf %s\n",
			pipe_id, node->desc.name);

	return ret;
}
static void reset_unused_io_of_ipi_frame(struct req_buffer_helper *helper)
{
	struct mtkcam_ipi_frame_param *fp;
	int i;

	fp = helper->fp;

	for (i = helper->ii_idx; i < ARRAY_SIZE(fp->img_ins); i++) {
		struct mtkcam_ipi_img_input *io = &fp->img_ins[i];

		io->uid = (struct mtkcam_ipi_uid) {0, 0};
	}

	for (i = helper->io_idx; i < ARRAY_SIZE(fp->img_outs); i++) {
		struct mtkcam_ipi_img_output *io = &fp->img_outs[i];

		io->uid = (struct mtkcam_ipi_uid) {0, 0};
	}

	for (i = helper->mi_idx; i < ARRAY_SIZE(fp->meta_inputs); i++) {
		struct mtkcam_ipi_meta_input *io = &fp->meta_inputs[i];

		io->uid = (struct mtkcam_ipi_uid) {0, 0};
	}

	for (i = helper->mo_idx; i < ARRAY_SIZE(fp->meta_outputs); i++) {
		struct mtkcam_ipi_meta_output *io = &fp->meta_outputs[i];

		io->uid = (struct mtkcam_ipi_uid) {0, 0};
	}
}

/* TODO: cache raw_data in helper */
static struct mtk_raw_request_data *req_get_raw_data(struct mtk_cam_ctx *ctx,
						     struct mtk_cam_request *req)
{
	if (ctx->raw_subdev_idx < 0)
		return NULL;

	return &req->raw_data[ctx->raw_subdev_idx];
}

static int fill_raw_stats_header(struct mtk_cam_buffer *buf,
				 struct set_meta_stats_info_param *p,
				 void **addr)
{
	struct mtk_cam_video_device *node;
	void *vaddr;
	int ret;

	node = mtk_cam_buf_to_vdev(buf);

	vaddr = vb2_plane_vaddr(&buf->vbb.vb2_buf, 0);
	if (WARN_ON(!vaddr))
		return -1;

	ret = CALL_PLAT_V4L2(set_meta_stats_info,
			     node->desc.dma_port,
			     vaddr, buf->meta_info.buffersize,
			     p);

	if (addr)
		*addr = vaddr;

	return ret;
}

static int fill_raw_meta_header(struct req_buffer_helper *helper)
{
	struct mtk_cam_job *job = helper->job;
	struct set_meta_stats_info_param p;
	struct mtk_cam_buffer *buf;
	int ret = 0;

	memset(&p, 0, sizeof(p));

	if (helper->meta_cfg_buf) {
		struct mtk_raw_request_data *raw_data;
		struct mtk_cam_resource_v2 *res;

		buf = helper->meta_cfg_buf;
		p.cfg_dataformat = buf->meta_info.v4l2_pixelformat;
		p.meta_cfg = vb2_plane_vaddr(&buf->vbb.vb2_buf, 0);
		p.meta_cfg_size = buf->meta_info.buffersize;

		raw_data = req_get_raw_data(job->src_ctx, job->req);
		if (!raw_data) {
			pr_info("%s: failed to get raw_data\n", __func__);
			return -1;
		}

		p.width = raw_data->sink.width;
		p.height = raw_data->sink.height;

		res = &raw_data->ctrl.resource.user_data;
		p.bin_ratio = bin_ratio(res->raw_res.bin);

		p.rgbw = is_rgbw(job);

		helper->meta_cfg_buf_va = p.meta_cfg;
	}

	if (helper->meta_stats0_buf) {
		ret = ret || fill_raw_stats_header(helper->meta_stats0_buf, &p,
						   &helper->meta_stats0_buf_va);

		job->timestamp_buf = helper->meta_stats0_buf_va ?
			(helper->meta_stats0_buf_va +
				GET_PLAT_V4L2(timestamp_buffer_ofst)) : NULL;
	}

	if (helper->meta_stats1_buf)
		ret = ret || fill_raw_stats_header(helper->meta_stats1_buf, &p,
						   &helper->meta_stats1_buf_va);

	if (ret)
		pr_info("%s: failed. ret = %d\n", __func__, ret);

	return ret;
}

static int update_job_buffer_to_ipi_frame(struct mtk_cam_job *job,
	struct mtkcam_ipi_frame_param *fp, struct pack_job_ops_helper *job_helper)
{
	struct req_buffer_helper helper;
	struct mtk_cam_request *req = job->req;
	struct mtk_cam_buffer *buf;
	int ret = 0;

	memset(&helper, 0, sizeof(helper));
	helper.job = job;
	helper.fp = fp;

	list_for_each_entry(buf, &req->buf_list, list) {
		ret = ret || update_cam_buf_to_ipi_frame(&helper, buf, job_helper);
	}

	/* update raw metadata header */
	ret = ret || fill_raw_meta_header(&helper);

	/* update necessary working buffer */
	if (job_helper->append_work_buf_to_ipi)
		ret = ret || job_helper->append_work_buf_to_ipi(&helper);

	reset_unused_io_of_ipi_frame(&helper);

	mtk_cam_fill_qos(&helper);

	return ret;
}

static int reset_img_ufd_io_param(struct mtkcam_ipi_frame_param *fp)
{
	memset(&fp->img_ufdi_params, 0, sizeof(fp->img_ufdi_params));
	memset(&fp->img_ufdo_params, 0, sizeof(fp->img_ufdo_params));

	return 0;
}

static int mtk_cam_job_fill_ipi_frame(struct mtk_cam_job *job,
	struct pack_job_ops_helper *job_helper)
{
	struct mtkcam_ipi_frame_param *fp;
	int ret;

	fp = (struct mtkcam_ipi_frame_param *)job->ipi.vaddr;

	ret = update_cq_buffer_to_ipi_frame(&job->cq, fp)
		|| update_job_raw_param_to_ipi_frame(job, fp)
		|| update_job_buffer_to_ipi_frame(job, fp, job_helper)
		|| reset_img_ufd_io_param(fp);

	if (ret)
		pr_info("%s: failed.\n", __func__);

	return ret;
}

int mtk_cam_job_fill_dump_param(struct mtk_cam_job *job,
				struct mtk_cam_dump_param *p,
				const char *desc)
{
	struct mtk_cam_request *req;
	struct mtk_cam_buffer *buf;
	int pipe_id;

	req = job->req;
	if (!req) {
		pr_info("%s: failed to get req", __func__);
		return -1;
	}

	pipe_id = get_raw_subdev_idx(job->src_ctx->used_pipe);
	if (pipe_id < 0) {
		pr_info("%s: failed to get pipe_id from %x\n",
			__func__, job->src_ctx->used_pipe);
		return -1;
	}

	memset(p, 0, sizeof(*p));

	/* Common Debug Information*/
	strncpy(p->desc, desc, sizeof(p->desc) - 1);

	p->request_fd = -1; /* TODO */
	p->stream_id = job->src_ctx->stream_id;
	p->timestamp = job->timestamp;
	p->sequence = job->req_seq;

	/* CQ dump */
	p->cq_cpu_addr	= job->cq.vaddr;
	p->cq_size	= job->cq.size;
	p->cq_iova	= job->cq.daddr;
	p->cq_desc_offset	= job->cq_rst.main.offset;
	p->cq_desc_size		= job->cq_rst.main.size;
	p->sub_cq_desc_offset	= job->cq_rst.sub.offset;
	p->sub_cq_desc_size	= job->cq_rst.sub.size;

	/* meta in */
	buf = mtk_cam_req_find_buffer(req, pipe_id, MTK_RAW_META_IN);
	if (buf) {
		p->meta_in_cpu_addr = vb2_plane_vaddr(&buf->vbb.vb2_buf, 0);
		p->meta_in_dump_buf_size = buf->meta_info.buffersize;
		p->meta_in_iova = buf->daddr;
	} else
		pr_info("%s: meta_in not found\n", __func__);

#ifdef DUMP_META_STATS
	/* meta out 0 */
	buf = mtk_cam_req_find_buffer(req, pipe_id, MTK_RAW_META_OUT_0);
	if (buf) {
		p->meta_out_0_cpu_addr = vb2_plane_vaddr(&buf->vbb.vb2_buf, 0);
		p->meta_out_0_dump_buf_size = buf->meta_info.buffersize;
		p->meta_out_0_iova = buf->daddr;
	} else
		pr_info("%s: meta_out_0 not found\n", __func__);

	/* meta out 1 */
	buf = mtk_cam_req_find_buffer(req, pipe_id, MTK_RAW_META_OUT_1);
	if (buf) {
		p->meta_out_1_cpu_addr = vb2_plane_vaddr(&buf->vbb.vb2_buf, 0);
		p->meta_out_1_dump_buf_size = buf->meta_info.buffersize;
		p->meta_out_1_iova = buf->daddr;
	} else
		pr_info("%s: meta_out_1 not found\n", __func__);

	/* meta out 2 */
	p->meta_out_2_cpu_addr		= NULL;
	p->meta_out_2_dump_buf_size	= 0;
	p->meta_out_2_iova		= 0;
#endif

	/* ipi frame param */
	p->frame_params		= job->ipi.vaddr;
	p->frame_param_size	= sizeof(*p->frame_params);

	/* ipi config param */
	p->config_params	= &job->ipi_config;
	p->config_param_size	= sizeof(job->ipi_config);

	return 0;
}

static int job_debug_dump(struct mtk_cam_job *job, const char *desc,
			  bool is_exception, int raw_pipe_idx)
{
	struct mtk_cam_dump_param p;
	struct mtk_cam_ctx *ctx;
	struct mtk_cam_debug *dbg;

	ctx = job->src_ctx;
	if (WARN_ON(!ctx))
		return -1;

	if (mtk_cam_job_fill_dump_param(job, &p, desc))
		goto DUMP_FAILED;

	dbg = &job->src_ctx->cam->dbg;
	if (is_exception) {
		if (mtk_cam_debug_exp_dump(dbg, &p))
			goto DUMP_FAILED;
	} else {
		if (mtk_cam_debug_dump(dbg, raw_pipe_idx, &p))
			goto DUMP_FAILED;
		else
			mtk_cam_event_request_dumped(&ctx->cam_ctrl,
						     p.sequence);
	}

	return 0;

DUMP_FAILED:
	pr_info("%s: failed. ctx %d pipe %x job req_seq %i desc %s\n",
		__func__, ctx->stream_id, ctx->used_pipe,
		job->req_seq, desc);
	return -1;
}

static int get_exp_switch_type(struct mtk_cam_job *job)
{
	struct mtk_cam_ctx *ctx = job->src_ctx;
	int res = EXPOSURE_CHANGE_NONE;
	int cur, prev;

	cur = job_exp_num(job);

	if (ctx->not_first_job)
		prev = job_prev_exp_num(job);
	else
		prev = scen_max_exp_num(&job->job_scen);

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

static bool do_seamless_switch(struct mtk_cam_job *job)
{
	struct mtk_raw_ctrl_data *ctrl_data = get_raw_ctrl_data(job);
	bool ret;

	/* exposure change */
	if (job->switch_type != EXPOSURE_CHANGE_NONE)
		return true;

	/* sensor change */
	ret = (ctrl_data) ?
		ctrl_data->rc_data.sensor_mode_update : false;

	return ret;
}

static void job_dump_engines_debug_status(struct mtk_cam_job *job)
{
	struct mtk_cam_ctx *ctx = job->src_ctx;
	struct mtk_cam_device *cam = ctx->cam;

	mtk_engine_dump_debug_status(cam, job->used_engine);
	if (ctx->seninf)
		mtk_cam_seninf_dump(ctx->seninf, job->frame_seq_no, false);
}

static void mtk_cam_aa_dump_work(struct work_struct *work)
{
	struct mtk_cam_job *job =
		container_of(work, struct mtk_cam_job, aa_dump_work);
	struct mtk_cam_ctx *ctx = job->src_ctx;
	struct mtk_cam_engines *eng = &ctx->cam->engines;
	struct mtk_raw_sink_data *sink = get_raw_sink_data(job);
	struct mtk_raw_device *raw_dev;
	struct mtk_ae_debug_data ae_data, ae_data_w;
	unsigned long submask;
	int i;

	if (WARN_ON(!sink))
		goto PUT_JOB;

	memset(&ae_data, 0, sizeof(ae_data));
	memset(&ae_data_w, 0, sizeof(ae_data_w));

	if (is_rgbw(job)) {
		submask = bit_map_subset_of(MAP_HW_RAW, ctx->used_engine);
		for (i = 0; i < eng->num_raw_devices && submask;
				i++, submask >>= 1) {
			if (!(submask & 0x1))
				continue;

			raw_dev = dev_get_drvdata(eng->raw_devs[i]);
			if (raw_dev->is_slave)
				fill_aa_info(raw_dev, &ae_data_w);
			else
				fill_aa_info(raw_dev, &ae_data);
		}
	} else {
		submask = bit_map_subset_of(MAP_HW_RAW, ctx->used_engine);
		for (i = 0; i < eng->num_raw_devices && submask;
				i++, submask >>= 1) {
			if (!(submask & 0x1))
				continue;

			raw_dev = dev_get_drvdata(eng->raw_devs[i]);
			fill_aa_info(raw_dev, &ae_data);
		}
	}

	pr_info("%s:%s:ctx(%d),seq(%d),size(%d,%d),(0x%llx,0x%llx,0x%llx,0x%llx)(0x%llx,0x%llx,0x%llx,0x%llx)(0x%llx,0x%llx,0x%llx,0x%llx)(0x%llx,0x%llx,0x%llx,0x%llx)(0x%llx,0x%llx,0x%llx,0x%llx)|(0x%llx,0x%llx,0x%llx,0x%llx)(0x%llx,0x%llx,0x%llx,0x%llx)(0x%llx,0x%llx,0x%llx,0x%llx)(0x%llx,0x%llx,0x%llx,0x%llx)(0x%llx,0x%llx,0x%llx,0x%llx)\n",
		__func__, job->req->req.debug_str,
		ctx->stream_id, job->req_seq,
		sink->width, sink->height,
		ae_data.OBC_R1_Sum[0], ae_data.OBC_R1_Sum[1],
		ae_data.OBC_R1_Sum[2], ae_data.OBC_R1_Sum[3],
		ae_data.OBC_R2_Sum[0], ae_data.OBC_R2_Sum[1],
		ae_data.OBC_R2_Sum[2], ae_data.OBC_R2_Sum[3],
		ae_data.OBC_R3_Sum[0], ae_data.OBC_R3_Sum[1],
		ae_data.OBC_R3_Sum[2], ae_data.OBC_R3_Sum[3],
		ae_data.AA_Sum[0], ae_data.AA_Sum[1],
		ae_data.AA_Sum[2], ae_data.AA_Sum[3],
		ae_data.LTM_Sum[0], ae_data.LTM_Sum[1],
		ae_data.LTM_Sum[2], ae_data.LTM_Sum[3],
		ae_data_w.OBC_R1_Sum[0], ae_data_w.OBC_R1_Sum[1],
		ae_data_w.OBC_R1_Sum[2], ae_data_w.OBC_R1_Sum[3],
		ae_data_w.OBC_R2_Sum[0], ae_data_w.OBC_R2_Sum[1],
		ae_data_w.OBC_R2_Sum[2], ae_data_w.OBC_R2_Sum[3],
		ae_data_w.OBC_R3_Sum[0], ae_data_w.OBC_R3_Sum[1],
		ae_data_w.OBC_R3_Sum[2], ae_data_w.OBC_R3_Sum[3],
		ae_data_w.AA_Sum[0], ae_data_w.AA_Sum[1],
		ae_data_w.AA_Sum[2], ae_data_w.AA_Sum[3],
		ae_data_w.LTM_Sum[0], ae_data_w.LTM_Sum[1],
		ae_data_w.LTM_Sum[2], ae_data_w.LTM_Sum[3]);

PUT_JOB:
	mtk_cam_job_put(job);
}

bool job_has_done_pending(struct mtk_cam_job *job)
{
	return atomic_long_read(&job->done_set) != job->done_handled ||
		atomic_long_read(&job->afo_done) == 1;
}

int job_handle_done(struct mtk_cam_job *job)
{
	unsigned long cur_handle, engine_to_handle;
	int i;
	int ret;

	engine_to_handle = _get_master_engines(job->used_engine);
	cur_handle = atomic_long_read(&job->done_set) & ~job->done_handled;

	MTK_CAM_TRACE_BEGIN(BASIC, "%s #%d cur=0x%lx", __func__,
			    job->req_seq, cur_handle);

	if (atomic_long_read(&job->afo_done) == BIT(0)) {
		_meta1_done(job);
		atomic_long_fetch_or(BIT(1), &job->afo_done);
	}

	/* handle_raw */
	if (bit_map_subset_of(MAP_HW_RAW, cur_handle))
		handle_raw_frame_done(job);

	/* handle_camsv */
	if (bit_map_subset_of(MAP_HW_CAMSV, cur_handle))
		handle_sv_frame_done(job);

	/* handle_mraw */
	if (bit_map_subset_of(MAP_HW_MRAW, cur_handle)) {
		unsigned long submask_mraw =
			bit_map_subset_of(MAP_HW_MRAW, cur_handle);

		for (i = 0; submask_mraw; i++, submask_mraw >>= 1) {
			if (!(submask_mraw & 0x1))
				continue;

			handle_mraw_frame_done(job,
					       i + MTKCAM_SUBDEV_MRAW_START);
		}
	}

	job->done_handled |= cur_handle;

	/* note: return 1 for success, 0 for continue */
	ret = (job->done_handled == engine_to_handle) ? 1 : 0;

	MTK_CAM_TRACE_END(BASIC);
	return ret;
}

int mtk_cam_job_manually_apply_sensor(struct mtk_cam_job *job,
				      bool transit_latched)
{
	int sensor_state;

	sensor_state = mtk_cam_job_state_get(&job->job_state, SENSOR_STATE);
	if (sensor_state == S_SENSOR_NONE) {
		pr_info("%s: without sensor setting to apply\n", __func__);
		return 0;
	}

	mtk_cam_job_state_set(&job->job_state, SENSOR_STATE, S_SENSOR_APPLYING);
	call_jobop(job, apply_sensor);

	if (transit_latched)
		mtk_cam_job_state_set(&job->job_state, SENSOR_STATE,
				      S_SENSOR_LATCHED);

	return 0;
}

int mtk_cam_job_manually_apply_isp(struct mtk_cam_job *job, bool wait_completion)
{
	unsigned long timeout = msecs_to_jiffies(1000);

	if (!wait_for_completion_timeout(&job->compose_completion, timeout)) {
		pr_info("[%s] error: wait for job composed timeout\n",
			__func__);
		return -1;
	}

	mtk_cam_job_state_set(&job->job_state, ISP_STATE, S_ISP_APPLYING);
	call_jobop(job, apply_isp);

	if (!wait_completion)
		return 0;

	if (!wait_for_completion_timeout(&job->cq_exe_completion, timeout)) {
		pr_info("[%s] error: wait for job cq exe\n", __func__);
		return -1;
	}

	return 0;
}

static int job_fetch_freq(struct mtk_cam_job *job,
			  unsigned int *freq_hz, bool *boostable)
{
	struct mtk_raw_ctrl_data *ctrl;
	struct mtk_cam_resource_driver *res;
	unsigned int freq;

	ctrl = get_raw_ctrl_data(job);
	if (!ctrl) {
		pr_info("%s: warn. should not be called\n", __func__);
		return -1;
	}

	res = &ctrl->resource;
	freq = res->user_data.raw_res.freq;

	if (is_m2m_apu(job)) {
		struct mtk_cam_ctx *ctx = job->src_ctx;
		struct mtk_cam_device *cam = ctx->cam;
		int opp_idx = ctrl->apu_info.opp_index;
		unsigned int adj_freq;

		adj_freq = mtk_cam_dvfs_query(&cam->dvfs, opp_idx);

		if (adj_freq == 0)
			adj_freq = 1;

		if (CAM_DEBUG_ENABLED(JOB) && freq != adj_freq)
			pr_info("%s: adjust by apu opp_index %d freq %u to %u\n",
				__func__, opp_idx, freq, adj_freq);

		freq = adj_freq;
	}

	*freq_hz = freq;
	/* boost isp clk during switching */
	*boostable = res->user_data.raw_res.hw_mode == HW_MODE_DIRECT_COUPLED;

	return 0;
}

int mtk_cam_job_update_clk(struct mtk_cam_job *job)
{
	struct mtk_cam_ctx *ctx = job->src_ctx;
	struct mtk_cam_device *cam = ctx->cam;
	unsigned int freq_hz;
	bool boostable;

	if (job_fetch_freq(job, &freq_hz, &boostable))
		return -1;

	return mtk_cam_dvfs_update(&cam->dvfs, ctx->stream_id,
				   freq_hz, boostable);
}

int mtk_cam_job_update_clk_switching(struct mtk_cam_job *job, bool begin)
{
	struct mtk_cam_ctx *ctx = job->src_ctx;
	struct mtk_cam_device *cam = ctx->cam;
	unsigned int freq_hz;
	bool boostable;

	if (!begin)
		return mtk_cam_dvfs_switch_end(&cam->dvfs, ctx->stream_id);

	if (job_fetch_freq(job, &freq_hz, &boostable))
		return -1;

	return mtk_cam_dvfs_switch_begin(&cam->dvfs, ctx->stream_id,
					 freq_hz, boostable);
}
