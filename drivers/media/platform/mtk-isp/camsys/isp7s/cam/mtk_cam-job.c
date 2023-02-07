// SPDX-License-Identifier: GPL-2.0
//
// Copyright (c) 2019 MediaTek Inc.

#include <linux/rpmsg/mtk_ccd_rpmsg.h>
#include <linux/pm_runtime.h>

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

#include "frame_sync_camsys.h"

#define SENSOR_SET_MARGIN_MS  25
#define SENSOR_SET_MARGIN_MS_STAGGER  27

enum MTK_CAMSYS_JOB_TYPE {
	RAW_JOB_ON_THE_FLY = 0x0,
	RAW_JOB_DC,
	RAW_JOB_M2M,
	RAW_JOB_MSTREAM,
	//RAW_JOB_DC_MSTREAM,
	RAW_JOB_STAGGER,
	//RAW_JOB_DC_STAGGER,
	RAW_JOB_OFFLINE_STAGGER,
	//RAW_JOB_OTF_RGBW,
	//RAW_JOB_DC_RGBW,
	//RAW_JOB_OFFLINE_RGBW,
	//RAW_JOB_HW_TIMESHARED,
	//RAW_JOB_HW_SUBSAMPLE,
	RAW_JOB_HW_PREISP,
	RAW_JOB_ONLY_SV = 0x100,
	//RAW_JOB_ONLY_MRAW = 0x200,
};

#define FH_SEQ_BIT_MASK 0x00FFFFFF
#define FH_CTX_ID_SHIFT_BIT_NUM 24

unsigned int
decode_fh_reserved_data_to_ctx(u32 data_in)
{
	return (data_in & ~FH_SEQ_BIT_MASK) >> FH_CTX_ID_SHIFT_BIT_NUM;
}

unsigned int
encode_fh_reserved_data(u32 ctx_id_in, u32 seq_no_in)
{
	u32 ctx_id_data = ctx_id_in << FH_CTX_ID_SHIFT_BIT_NUM;
	u32 seq_no_data = seq_no_in & FH_SEQ_BIT_MASK;

	return ctx_id_data | seq_no_data;
}

unsigned int
decode_fh_reserved_data_to_seq(u32 ref_near_by, u32 data_in)
{
	u32 ctx_id_data = decode_fh_reserved_data_to_ctx(data_in);
	u32 seq_no_data = data_in & FH_SEQ_BIT_MASK;
	u32 seq_no_nearby = ref_near_by;
	u32 seq_no_candidate = seq_no_data + (seq_no_nearby & ~FH_SEQ_BIT_MASK);
	bool dbg = false;

	if (seq_no_nearby > 10) {
		if (seq_no_candidate > seq_no_nearby + 10)
			seq_no_candidate = seq_no_candidate - BIT(FH_CTX_ID_SHIFT_BIT_NUM);
		else if (seq_no_candidate < seq_no_nearby - 10)
			seq_no_candidate = seq_no_candidate + BIT(FH_CTX_ID_SHIFT_BIT_NUM);
	}
	if (dbg)
		pr_info("[%s]: %d/%d <= %d",
			__func__, ctx_id_data, seq_no_candidate, data_in);

	return seq_no_candidate;
}

void _on_job_last_ref(struct mtk_cam_job *job)
{
	struct mtk_cam_ctrl *ctrl = &job->src_ctx->cam_ctrl;

	if (CAM_DEBUG_ENABLED(STATE))
		pr_info("%s: job #%d\n", __func__, job->frame_seq_no);

	write_lock(&ctrl->list_lock);
	list_del(&job->job_state.list);
	write_unlock(&ctrl->list_lock);

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

static void
mtk_cam_frame_done_work(struct work_struct *work)
{
	struct mtk_cam_job *job =
		container_of(work, struct mtk_cam_job, frame_done_work);

	call_jobop(job, handle_buffer_done);
	mtk_cam_job_put(job);
}

static void mtk_cam_meta1_done_work(struct work_struct *work)
{
	struct mtk_cam_job *job =
		container_of(work, struct mtk_cam_job, meta1_done_work);

	call_jobop(job, handle_afo_done);
	mtk_cam_job_put(job);
}

static int handle_done_async(struct mtk_cam_job *job, struct work_struct *work)
{
	struct mtk_cam_ctx *ctx = job->src_ctx;

	return mtk_cam_ctx_queue_done_wq(ctx, work);
}

static int apply_sensor_async(struct mtk_cam_job *job)
{
	struct mtk_cam_ctx *ctx = job->src_ctx;

	return mtk_cam_ctx_queue_sensor_worker(ctx, &job->sensor_work);
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

	if (action & ACTION_AFO_DONE) {
		mtk_cam_job_get(job);
		ret = ret || handle_done_async(job, &job->meta1_done_work);
	}

	if (action & ACTION_BUFFER_DONE) {
		/* no need to call mtk_cam_job_get here.
		 * Already did it just before queueing into ctrl's list.
		 */
		ret = ret || handle_done_async(job, &job->frame_done_work);
	}

	if (action & ACTION_TRIGGER)
		ret = ret || call_jobop(job, trigger_isp);

	return ret;
}

static int map_job_type(const struct mtk_cam_scen *scen)
{
	enum mtk_cam_scen_id scen_id = scen->id;
	int job_type;

	switch (scen_id) {
	case MTK_CAM_SCEN_NORMAL:
		if (scen->scen.normal.max_exp_num == 1)
			job_type = RAW_JOB_ON_THE_FLY;
		else
			job_type = RAW_JOB_STAGGER;
		break;
	case MTK_CAM_SCEN_M2M_NORMAL:
	case MTK_CAM_SCEN_ODT_NORMAL:
		job_type = RAW_JOB_M2M;
		break;

	case MTK_CAM_SCEN_MSTREAM:
		job_type = RAW_JOB_MSTREAM;
		break;
	case MTK_CAM_SCEN_EXT_ISP:
		job_type = RAW_JOB_HW_PREISP;
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

	atomic_set(&job->refs, 0);

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

	kthread_init_work(&job->sensor_work, mtk_cam_sensor_work);
	INIT_WORK(&job->frame_done_work, mtk_cam_frame_done_work);
	INIT_WORK(&job->meta1_done_work, mtk_cam_meta1_done_work);

	return ret;
}

static int mtk_cam_select_hw_only_sv(struct mtk_cam_job *job)
{
	struct mtk_cam_ctx *ctx = job->src_ctx;
	struct mtk_cam_device *cam = ctx->cam;
	struct device *sv = NULL;
	int available, sv_available;
	int selected;
	int rsv_id = GET_PLAT_V4L2(reserved_camsv_dev_id);
	int i;

	selected = 0;
	available = mtk_cam_get_available_engine(cam);
	sv_available = USED_MASK_GET_SUBMASK(&available, camsv);

	/* todo: more rules */
	if (SUBMASK_HAS(&sv_available, camsv, rsv_id)) {
		USED_MASK_SET(&selected, camsv, rsv_id);
		sv = cam->engines.sv_devs[rsv_id];
	} else {
		dev_info(cam->dev, "select hw failed\n");
		return -1;
	}

	ctx->hw_raw = NULL;
	ctx->hw_sv = sv;
	for (i = 0; i < ARRAY_SIZE(ctx->hw_mraw); i++)
		ctx->hw_mraw[i] = NULL;

	return selected;
}

static int mtk_cam_select_hw(struct mtk_cam_job *job)
{
	struct mtk_cam_ctx *ctx = job->src_ctx;
	struct mtk_cam_device *cam = ctx->cam;
	struct device *raw = NULL;
	int available, raw_available, sv_available, mraw_available;
	int selected;
	int i = 0;
	int raw_idx = -1, mraw_idx;

	selected = 0;
	available = mtk_cam_get_available_engine(cam);
	raw_available = USED_MASK_GET_SUBMASK(&available, raw);
	sv_available = USED_MASK_GET_SUBMASK(&available, camsv);
	mraw_available = USED_MASK_GET_SUBMASK(&available, mraw);

	/* todo: more rules */
	for (i = 0; i < cam->engines.num_raw_devices; i++)
		if (SUBMASK_HAS(&raw_available, raw, i)) {
			USED_MASK_SET(&selected, raw, i);
			raw = cam->engines.raw_devs[i];
			raw_idx = i;
			break;
		}

	if (!selected) {
		dev_info(cam->dev, "select hw failed\n");
		return -1;
	}

	ctx->hw_raw = raw;

	/* camsv */
	ctx->hw_sv = NULL;
	if (ctx->hw_raw) {
		dev_info(cam->dev, "select sv hw start (raw_idx:%d/sv_available:0x%x)\n",
				raw_idx, sv_available);
		if (SUBMASK_HAS(&sv_available, camsv, raw_idx)) {
			USED_MASK_SET(&selected, camsv, raw_idx);
			ctx->hw_sv = cam->engines.sv_devs[raw_idx];
			dev_info(cam->dev, "select sv hw end (raw_idx:%d/sv_available:0x%x/selected:0x%x)\n",
				raw_idx, sv_available, selected);
		} else {
			dev_info(cam->dev, "select sv hw failed(raw_idx:%d/sv_available:0x%x)\n",
				raw_idx, sv_available);
			return -1;
		}
	}

	/* mraw */
	for (i = 0; i < MAX_MRAW_PIPES_PER_STREAM; i++) {
		if (i < ctx->num_mraw_subdevs) {
			struct mtk_mraw_device *mraw_dev;

			mraw_idx = ctx->mraw_subdev_idx[i];
			if (SUBMASK_HAS(&mraw_available, mraw, mraw_idx)) {
				USED_MASK_SET(&selected, mraw, mraw_idx);
				ctx->hw_mraw[i] = cam->engines.mraw_devs[mraw_idx];
				mraw_dev = dev_get_drvdata(ctx->hw_mraw[i]);
				mraw_dev->pipeline = &cam->pipelines.mraw[mraw_idx];
			}
		} else
			ctx->hw_mraw[i] = NULL;
	}

	return selected;
}

static int
update_job_type_feature(struct mtk_cam_job *job)
{
	struct mtk_cam_ctx *ctx = job->src_ctx;
	int pipe_idx;

	if (ctx->has_raw_subdev) {
		pipe_idx = get_raw_subdev_idx(ctx->used_pipe);
		if (pipe_idx == -1)
			return -1;
		job->job_scen = job->req->raw_data[pipe_idx].ctrl.resource.user_data.raw_res.scen;
		job->job_type = map_job_type(&job->job_scen);
	} else {
		pipe_idx = get_sv_subdev_idx(ctx->used_pipe);
		if (pipe_idx == -1)
			return -1;
		/* FIXME(AY): does sv have this? */
		job->job_scen = job->req->raw_data[pipe_idx].ctrl.resource.user_data.raw_res.scen;
		job->job_type = RAW_JOB_ONLY_SV;
	}

	return 0;
}

/* workqueue context */
static int
_meta1_done(struct mtk_cam_job *job)
{
	struct mtk_cam_ctx *ctx = job->src_ctx;
	struct mtk_cam_device *cam = ctx->cam;
	int pipe_id = get_raw_subdev_idx(job->req->used_pipe);

	if (pipe_id < 0)
		return 0;

	dev_dbg(cam->dev, "%s:%s:ctx(%d): seq_no:%d, state:0x%x\n",
			__func__, job->req->req.debug_str, job->src_ctx->stream_id,
			job->frame_seq_no,
			mtk_cam_job_state_get(&job->job_state, ISP_STATE));

	mtk_cam_req_buffer_done(job->req, pipe_id, MTKCAM_IPI_RAW_META_STATS_1,
				VB2_BUF_STATE_DONE, job->timestamp);

	return 0;
}
#define TIMESTAMP_LOG
static void convert_fho_timestamp_to_meta(struct mtk_cam_job *job)
{
	u32 *fho_va;
	int subsample;
	int i;
	u64 hw_timestamp;

	/* skip if meta0 does not exist */
	if (!job->timestamp_buf)
		return;

	// using cpu's timestamp
	// (*job->timestamp_buf)[0] = job->timestamp_mono;
	// (*job->timestamp_buf)[1] = job->timestamp;

	subsample = get_subsample_ratio(job) + 1;
	fho_va = (u32 *)(job->cq.vaddr + job->cq.size - 64 * subsample);

	for (i = 0; i < subsample; i++) {
		hw_timestamp = (u64) *(fho_va + i*16);
		hw_timestamp += ((u64)*(fho_va + i*16 + 1) << 32);

		/* timstamp_LSB + timestamp_MSB << 32 */
		(*job->timestamp_buf)[i*2] =
			mtk_cam_timesync_to_monotonic(hw_timestamp) /1000;
		(*job->timestamp_buf)[i*2 + 1] =
			mtk_cam_timesync_to_boot(hw_timestamp) /1000;
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
_frame_done(struct mtk_cam_job *job)
{
	struct mtk_cam_ctx *ctx = job->src_ctx;
	struct mtk_cam_device *cam = ctx->cam;
	unsigned int used_pipe = job->req->used_pipe & job->src_ctx->used_pipe;
	bool is_normal =
		(mtk_cam_job_state_get(&job->job_state, ISP_STATE) == S_ISP_DONE);
	int i;

	/* TODO(AY): should handle each pipe's done separately */
	if (used_pipe == 0)
		return 0;
	if (ctx->has_raw_subdev)
		convert_fho_timestamp_to_meta(job);
	for (i = 0; i < MTKCAM_SUBDEV_MAX; i++) {
		if (used_pipe & (1 << i)) {
			if (is_normal)
				mtk_cam_req_buffer_done(job->req, i, -1,
					VB2_BUF_STATE_DONE, job->timestamp);
			else
				mtk_cam_req_buffer_done(job->req, i, -1,
					VB2_BUF_STATE_ERROR, job->timestamp);
		}
	}
	/* FIXME(AY): should not access req after buffer_done */
	dev_info(cam->dev, "%s:%s:ctx(%d): seq_no:%d, state:0x%x, is_normal:%d, B/M ts:%lld/%lld\n",
		__func__, job->req->req.debug_str, job->src_ctx->stream_id,
		job->frame_seq_no,
		mtk_cam_job_state_get(&job->job_state, ISP_STATE),
		is_normal, job->timestamp, job->timestamp_mono);

	return 0;
}

static int job_afo_done(struct mtk_cam_job *job)
{
	_meta1_done(job);
	return 0;
}

static int job_buffer_done(struct mtk_cam_job *job)
{
	_frame_done(job);
	return 0;
}

static int
_stream_on(struct mtk_cam_job *job, bool on)
{
	struct mtk_cam_ctx *ctx = job->src_ctx;
	struct mtk_cam_device *cam = ctx->cam;
	int raw_id = _get_master_raw_id(cam->engines.num_raw_devices,
			job->used_engine);
	struct mtk_raw_device *raw_dev =
		dev_get_drvdata(cam->engines.raw_devs[raw_id]);
	struct mtk_camsv_device *sv_dev;
	struct mtk_mraw_device *mraw_dev;
	int i;

	stream_on(raw_dev, on);

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

	if (job->stream_on_seninf)
		ctx_stream_on_seninf_sensor(job->src_ctx, on);

	return 0;
}

static int
_stream_on_only_sv(struct mtk_cam_job *job, bool on)
{
	struct mtk_cam_ctx *ctx = job->src_ctx;
	struct mtk_camsv_device *sv_dev;

	if (ctx->hw_sv) {
		sv_dev = dev_get_drvdata(ctx->hw_sv);
		mtk_cam_sv_dev_stream_on(sv_dev, on);
	}

	if (job->stream_on_seninf)
		ctx_stream_on_seninf_sensor(job->src_ctx, on);

	return 0;
}

static bool frame_sync_start(struct mtk_cam_request *req)
{
#ifdef NOT_READY

	/* All ctx with sensor is in ready state */
	struct mtk_cam_device *cam =
		container_of(req->req.mdev, struct mtk_cam_device, media_dev);
	struct mtk_cam_ctx *ctx;
	struct mtk_cam_ctx *sync_ctx[MTKCAM_SUBDEV_MAX];
	unsigned int pipe_id;
	int i, ctx_cnt = 0, synced_cnt = 0;
	bool ret = false;

	/* pick out the used ctxs */
	for (i = 0; i < cam->max_stream_num; i++) {
		if (!(1 << i & req->ctx_used))
			continue;

		sync_ctx[ctx_cnt] = &cam->ctxs[i];
		ctx_cnt++;
	}

	mutex_lock(&req->fs.op_lock);
	if (ctx_cnt > 1) {  /* multi sensor case */
		req->fs.on_cnt++;
		if (req->fs.on_cnt != 1)  /* not first time */
			goto EXIT;

		for (i = 0; i < ctx_cnt; i++) {
			ctx = sync_ctx[i];
			spin_lock(&ctx->streaming_lock);
			if (!ctx->streaming) {
				spin_unlock(&ctx->streaming_lock);
				dev_info(cam->dev,
					 "%s: ctx(%d): is streamed off\n",
					 __func__, ctx->stream_id);
				continue;
			}
			pipe_id = ctx->stream_id;
			spin_unlock(&ctx->streaming_lock);

			/* update sensor frame sync */
			if (!ctx->synced) {
				if (mtk_cam_req_frame_sync_set(req, pipe_id, 1))
					ctx->synced = 1;
			}
			/* TODO: user fs */

			if (ctx->synced)
				synced_cnt++;
		}

		/* the prepared sensor is no enough, skip */
		/* frame sync set failed or stream off */
		if (synced_cnt < 2) {
			mtk_cam_fs_reset(&req->fs);
			dev_info(cam->dev, "%s:%s: sensor is not ready\n",
				 __func__, req->req.debug_str);
			goto EXIT;
		}

		dev_dbg(cam->dev, "%s:%s:fs_sync_frame(1): ctxs: 0x%x\n",
			__func__, req->req.debug_str, req->ctx_used);

#ifdef TO_BE_UPDATED
		fs_sync_frame(1);
#endif

		ret = true;
		goto EXIT;

	} else if (ctx_cnt == 1) {  /* single sensor case */
		ctx = sync_ctx[0];
		spin_lock(&ctx->streaming_lock);
		if (!ctx->streaming) {
			spin_unlock(&ctx->streaming_lock);
			dev_info(cam->dev,
				 "%s: ctx(%d): is streamed off\n",
				 __func__, ctx->stream_id);
			goto EXIT;
		}
		pipe_id = ctx->stream_id;
		spin_unlock(&ctx->streaming_lock);

		if (ctx->synced) {
			if (mtk_cam_req_frame_sync_set(req, pipe_id, 0))
				ctx->synced = 0;
		}
	}
EXIT:
	dev_dbg(cam->dev, "%s:%s:target/on/off(%d/%d/%d)\n", __func__,
		req->req.debug_str, req->fs.target, req->fs.on_cnt,
		req->fs.off_cnt);
	mutex_unlock(&req->fs.op_lock);
	return ret;
#endif
	return 0;
}


static bool frame_sync_end(struct mtk_cam_request *req)
{
	/* All ctx with sensor is not in ready state */
	struct mtk_cam_device *cam =
		container_of(req->req.mdev, struct mtk_cam_device, media_dev);
	bool ret = false;

	mutex_lock(&req->fs.op_lock);
	if (req->fs.target && req->fs.on_cnt) { /* check fs on */
		req->fs.off_cnt++;
		if (req->fs.on_cnt != req->fs.target ||
		    req->fs.off_cnt != req->fs.target) { /* not the last */
			goto EXIT;
		}
		dev_dbg(cam->dev,
			 "%s:%s:fs_sync_frame(0)\n",
			 __func__, req->req.debug_str);

#ifdef TO_BE_UPDATED
		fs_sync_frame(0);
#endif

		ret = true;
		goto EXIT;
	}
EXIT:
	dev_dbg(cam->dev, "%s:%s:target/on/off(%d/%d/%d)\n", __func__,
		req->req.debug_str, req->fs.target, req->fs.on_cnt,
		req->fs.off_cnt);
	mutex_unlock(&req->fs.op_lock);
	return ret;
}


/* kthread context */
static int
_apply_sensor(struct mtk_cam_job *job)
{
	struct mtk_cam_ctx *ctx = job->src_ctx;
	struct mtk_cam_device *cam = ctx->cam;
	struct mtk_cam_request *req = job->req;

	//MTK_CAM_TRACE_BEGIN(BASIC, "frame_sync_start");
	if (frame_sync_start(req))
		dev_dbg(cam->dev, "%s:%s:ctx(%d): sensor ctrl with frame sync - start\n",
			__func__, req->req.debug_str, ctx->stream_id);
	//MTK_CAM_TRACE_END(BASIC); /* frame_sync_start */
	if (job->sensor_hdl_obj) {
		v4l2_ctrl_request_setup(&req->req,
					job->sensor->ctrl_handler);
		dev_info(cam->dev,
			"[%s] ctx:%d, job:%d\n",
			__func__, ctx->stream_id, job->frame_seq_no);
	}
	//MTK_CAM_TRACE_BEGIN(BASIC, "frame_sync_end");
	if (frame_sync_end(req))
		dev_dbg(cam->dev, "%s:ctx(%d): sensor ctrl with frame sync - stop\n",
				__func__, ctx->stream_id);
	//MTK_CAM_TRACE_END(BASIC); /* frame_sync_end */

	/* TBC */
	/* mtk_cam_tg_flash_req_setup(ctx, s_data); */
	mtk_cam_req_complete_ctrl_obj(job->sensor_hdl_obj);

	dev_dbg(cam->dev, "%s:%s:ctx(%d)req(%d):sensor done\n",
		__func__, req->req.debug_str, ctx->stream_id, job->frame_seq_no);
	return 0;
}

static int ipi_config(struct mtk_cam_job *job)
{
	struct mtk_cam_ctx *ctx = job->src_ctx;
	struct mtkcam_ipi_event event;
	struct mtkcam_ipi_session_cookie *session = &event.cookie;
	struct mtkcam_ipi_config_param *config = &event.config_data;
	struct mtkcam_ipi_config_param *src_config = &job->ipi_config;

	memset(&event, 0, sizeof(event));
	event.cmd_id = CAM_CMD_CONFIG;
	session->session_id = ctx->stream_id;
	memcpy(config, src_config, sizeof(*src_config));

	rpmsg_send(ctx->rpmsg_dev->rpdev.ept, &event, sizeof(event));

	dev_info(job->src_ctx->cam->dev, "%s: rpmsg_send id: %d\n",
		 __func__, event.cmd_id);
	return 0;
}

static int _compose(struct mtk_cam_job *job)
{
	struct mtkcam_ipi_event event;
	struct mtkcam_ipi_session_cookie *session = &event.cookie;
	struct mtkcam_ipi_frame_info *frame_info = &event.frame_data;
	struct mtk_cam_pool_buffer *ipi = &job->ipi;
	int ret;

	if (job->do_ipi_config) {
		ret = ipi_config(job);
		if (ret)
			return ret;
	}

	memset(&event, 0, sizeof(event));
	event.cmd_id = CAM_CMD_FRAME;
	session->session_id = job->src_ctx->stream_id;
	session->frame_no =
		encode_fh_reserved_data(job->src_ctx->stream_id, job->frame_seq_no);
	frame_info->cur_msgbuf_offset = ipi->size * ipi->priv.index;
	frame_info->cur_msgbuf_size = ipi->size;

	if (WARN_ON(!job->src_ctx->rpmsg_dev))
		return -1;

	/* FIXME(AY): remove, move to pack */
	job->job_state.seq_no = job->frame_seq_no;

	//MTK_CAM_TRACE_BEGIN(BASIC, "ipi_cmd_frame:%d",
	//req_stream_data->frame_seq_no);

	rpmsg_send(job->src_ctx->rpmsg_dev->rpdev.ept, &event, sizeof(event));

	//MTK_CAM_TRACE_END(BASIC);

	dev_info(job->src_ctx->cam->dev,
		 "%s: req:%s: rpmsg_send id: %d, ctx:%d, seq:%d\n",
		 __func__, job->req->req.debug_str,
		 event.cmd_id, session->session_id,
		 job->frame_seq_no);

	return 0;
}

static int _apply_sv_cq(struct mtk_cam_job *job)
{
	struct mtk_cam_ctx *ctx = job->src_ctx;
	struct mtk_cam_device *cam = ctx->cam;
	struct mtk_camsv_device *sv_dev;
	int i, used_engine;
	int ret = 0;

	if (job->composed) {
		used_engine = USED_MASK_GET_SUBMASK(&job->used_engine, camsv);
		for (i = 0; i < cam->engines.num_camsv_devices; i++) {
			if (used_engine & (1 << i)) {
				sv_dev = dev_get_drvdata(cam->engines.sv_devs[i]);
				apply_camsv_cq(sv_dev, job->cq.daddr,
					job->cq_rst.camsv[0].size,
					job->cq_rst.camsv[0].offset, 0);
				dev_info(sv_dev->dev,
					"SOF[ctx:%d], CQ-%d triggered, cq_addr:0x%x\n",
					ctx->stream_id, job->frame_seq_no, job->cq.daddr);
			}
		}
	}

	return ret;
}

static int _apply_mraw_cq(struct mtk_cam_job *job)
{
	struct mtk_cam_ctx *ctx = job->src_ctx;
	struct mtk_cam_device *cam = ctx->cam;
	struct mtk_mraw_device *mraw_dev;
	int i, mraw_idx;
	int ret = 0;

	if (job->composed) {
		for (i = 0; i < ctx->num_mraw_subdevs; i++) {
			mraw_idx = ctx->mraw_subdev_idx[i];
			mraw_dev = dev_get_drvdata(cam->engines.mraw_devs[mraw_idx]);
			apply_mraw_cq(mraw_dev, job->cq.daddr,
				job->cq_rst.mraw[i].size,
				job->cq_rst.mraw[i].offset, 0);
			dev_info(mraw_dev->dev,
				"SOF[ctx:%d], CQ-%d triggered, cq_addr:0x%x\n",
				ctx->stream_id, job->frame_seq_no, job->cq.daddr);
		}
	}

	return ret;
}

static int _apply_cq(struct mtk_cam_job *job)
{
	struct mtk_cam_ctx *ctx = job->src_ctx;
	struct mtk_cam_device *cam = ctx->cam;
	int raw_id = _get_master_raw_id(cam->engines.num_raw_devices,
			job->used_engine);
	struct mtk_raw_device *raw_dev =
		dev_get_drvdata(cam->engines.raw_devs[raw_id]);
	dma_addr_t base_addr = job->cq.daddr;
	int ret = 0;

	if (WARN_ON(!job->composed))
		return -1;

	apply_cq(raw_dev, 0, base_addr,
		 job->cq_rst.main.size, job->cq_rst.main.offset,
		 job->cq_rst.sub.size, job->cq_rst.sub.offset);

	dev_info(raw_dev->dev,
		 "[ctx:%d], CQ-%d triggered, cq_addr:0x%x\n",
		 ctx->stream_id, job->frame_seq_no, base_addr);

	// to be confirmed
	_apply_sv_cq(job);
	// to be confirmed
	_apply_mraw_cq(job);

	return ret;
}

static int trigger_m2m(struct mtk_cam_job *job)
{
	struct mtk_cam_ctx *ctx = job->src_ctx;
	struct mtk_cam_device *cam = ctx->cam;
	int raw_id = _get_master_raw_id(cam->engines.num_raw_devices,
			job->used_engine);
	struct mtk_raw_device *raw_dev =
		dev_get_drvdata(cam->engines.raw_devs[raw_id]);
	int ret = 0;

	mtk_cam_event_frame_sync(&ctx->cam_ctrl, job->frame_seq_no);

	trigger_rawi_r2(raw_dev);

	dev_info(raw_dev->dev, "%s [ctx:%d] %d\n",
		 __func__, ctx->stream_id, job->frame_seq_no);

	return ret;
}

static void
_compose_done(struct mtk_cam_job *job,
	struct mtkcam_ipi_frame_ack_result *cq_ret)
{
	job->composed = true;
	job->cq_rst = *cq_ret;
}

int mtk_cam_job_get_sensor_margin(struct mtk_cam_job *job)
{
	return get_apply_sensor_margin_ms(job);
}
static int apply_raw_target_clk(struct mtk_cam_ctx *ctx,
				struct mtk_cam_request *req)
{
	struct mtk_raw_request_data *raw_data;
	struct mtk_cam_resource_driver *res;

	raw_data = &req->raw_data[ctx->raw_subdev_idx];
	res = &raw_data->ctrl.resource;

	return mtk_cam_dvfs_update(&ctx->cam->dvfs, ctx->stream_id,
				   res->clk_target);
}
#ifdef NOT_READY
static int
alloc_image_work_buffer(struct mtk_cam_device_buf *buf, int size,
				   struct device *dev)
{
	struct dma_buf *dbuf;
	int ret;

	WARN_ON(!dev);

	dbuf = mtk_cam_noncached_buffer_alloc(size);

	ret = mtk_cam_device_buf_init(buf, dbuf, dev, size);
	dma_heap_buffer_free(dbuf);
	return  ret;
}

static int
alloc_hdr_buffer(struct mtk_cam_ctx *ctx,
			    struct mtk_cam_request *req)
{
	struct mtk_cam_driver_buf_desc *desc = &ctx->hdr_buf_desc;
	struct mtk_cam_device_buf *buf = &ctx->hdr_buffer;
	struct device *dev;
	struct mtk_raw_request_data *d;
	int ret;

	/* FIXME */
	d = &req->raw_data[ctx->raw_subdev_idx];

	/* desc */
	desc->ipi_fmt = sensor_mbus_to_ipi_fmt(d->sink.mbus_code);
	if (WARN_ON_ONCE(desc->ipi_fmt == MTKCAM_IPI_BAYER_PXL_ID_UNKNOWN))
		return -1;

	desc->width = d->sink.width;
	desc->height = d->sink.height;
	desc->stride[0] = mtk_cam_dmao_xsize(d->sink.width, desc->ipi_fmt, 4);
	desc->stride[1] = 0;
	desc->stride[2] = 0;
	desc->size = desc->stride[0] * desc->height;

	/* FIXME: */
	dev = ctx->hw_raw;

	ret = alloc_image_work_buffer(buf, desc->size, dev);
	if (ret)
		return ret;

	desc->daddr = buf->daddr;
	desc->fd = 0; /* TODO: for UFO */

	dev_info(ctx->cam->dev, "%s: fmt %d %dx%d str %d size %zu da 0x%x\n",
		 __func__, desc->ipi_fmt, desc->width, desc->height,
		 desc->stride[0], desc->size, desc->daddr);
	return 0;
}
#endif

#ifdef SUBSAMPLE
static int
_job_pack_subsample(struct mtk_cam_job *job,
	 struct pack_job_ops_helper *job_helper)
{
	struct mtk_cam_ctx *ctx = job->src_ctx;
	struct mtk_cam_device *cam = ctx->cam;
	int ret;

	job->exp_num_cur = 1;
	job->exp_num_prev = 1;
	job->hardware_scenario = MTKCAM_IPI_HW_PATH_ON_THE_FLY;
	job->sw_feature = MTKCAM_IPI_SW_FEATURE_NORMAL;
	job->sub_ratio = get_subsample_ratio(job);
	dev_info(cam->dev, "[%s] ctx:%d, type:%d, scen_id:%d, ratio:%d, sw/scene:%d/%d",
		__func__, ctx->stream_id, job->job_type, job->job_scen.id,
		job->sub_ratio, job->sw_feature, job->hardware_scenario);
	job->stream_on_seninf = false;
	if (!ctx->used_engine) {
		int selected;

		selected = mtk_cam_select_hw(job);
		if (!selected)
			return -1;

		if (mtk_cam_occupy_engine(ctx->cam, selected))
			return -1;

		mtk_cam_pm_runtime_engines(&ctx->cam->engines, selected, 1);
		ctx->used_engine = selected;
		if (ctx->hw_raw) {
			struct mtk_raw_device *raw = dev_get_drvdata(ctx->hw_raw);

			initialize(raw, 0);
			subsample_enable(raw, job->sub_ratio);
		}

		job->stream_on_seninf = true;
	}
	/* config_flow_by_job_type */
	job->used_engine = ctx->used_engine;
	job->hw_raw = ctx->hw_raw;

	job->do_ipi_config = false;
	if (!ctx->configured) {
		/* if has raw */
		if (USED_MASK_GET_SUBMASK(&ctx->used_engine, raw)) {
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

		apply_raw_target_clk(ctx, job->req);
	}
	ret = mtk_cam_job_fill_ipi_frame(job, job_helper);

	return ret;
}
#endif

static int
_job_pack_otf_stagger(struct mtk_cam_job *job,
	 struct pack_job_ops_helper *job_helper)
{
	struct mtk_cam_ctx *ctx = job->src_ctx;
	struct mtk_cam_device *cam = ctx->cam;
	struct mtk_cam_stagger_job *stagger_job =
			(struct mtk_cam_stagger_job *)job;
	int ret, i;

	/* stagger job needed */
	stagger_job->prev_scen = ctx->ctldata_stored.resource.user_data.raw_res.scen;
	stagger_job->switch_type = get_switch_type_stagger(job);
	update_stagger_job_exp(job);
	job->hardware_scenario = get_hard_scenario_stagger(job);
	job->sw_feature = MTKCAM_IPI_SW_FEATURE_VHDR;
	job->sub_ratio = get_subsample_ratio(job);
	stagger_job->dcif_enable = job->exp_num_cur > 1 ? 1 : 0;
	stagger_job->need_drv_buffer_check = is_stagger_multi_exposure(job);
	dev_info(cam->dev, "[%s] ctx/seq:%d/%d, type:%d, scen exp:%d->%d, swi:%d,  expN:%d->%d, sw/scene:%d/0x%x",
		__func__, ctx->stream_id, job->job_type, job->frame_seq_no, stagger_job->prev_scen.scen.normal.exp_num,
		job->job_scen.scen.normal.exp_num, stagger_job->switch_type, job->exp_num_prev,
		job->exp_num_cur, job->sw_feature, job->hardware_scenario);
	job->stream_on_seninf = false;
	if (!ctx->used_engine) {
		int selected;

		selected = mtk_cam_select_hw(job);
		if (!selected)
			return -1;

		if (mtk_cam_occupy_engine(ctx->cam, selected))
			return -1;

		mtk_cam_pm_runtime_engines(&ctx->cam->engines, selected, 1);
		ctx->used_engine = selected;
		if (ctx->hw_raw) {
			struct mtk_raw_device *raw = dev_get_drvdata(ctx->hw_raw);

			initialize(raw, 0);
			stagger_enable(raw);
		}

		/* camsv */
		if (ctx->hw_sv) {
			struct mtk_camsv_device *sv = dev_get_drvdata(ctx->hw_sv);

			mtk_cam_sv_dev_config(sv);
		}

		/* mraw */
		for (i = 0 ; i < ARRAY_SIZE(ctx->hw_mraw); i++) {
			if (ctx->hw_mraw[i]) {
				struct mtk_mraw_device *mraw =
					dev_get_drvdata(ctx->hw_mraw[i]);

				mtk_cam_mraw_dev_config(mraw);
			}
		}

		job->stream_on_seninf = true;
	}
	/* config_flow_by_job_type */
	job->used_engine = ctx->used_engine;
	job->hw_raw = ctx->hw_raw;

	job->do_ipi_config = false;
	if (!ctx->configured) {
		/* handle camsv tags */
		if (handle_sv_tag_hdr(job)) {
			dev_info(cam->dev, "tag handle failed");
			return -1;
		}

		/* if has raw */
		if (USED_MASK_GET_SUBMASK(&ctx->used_engine, raw)) {
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
#ifdef drv_alloc_buffer
		ret = alloc_hdr_buffer(ctx, job->req);
		if (ret)
			return ret;
#endif
		ctx->not_first_job = true;

		apply_raw_target_clk(ctx, job->req);
	}
	ret = mtk_cam_job_fill_ipi_frame(job, job_helper);
	return ret;
}

static int
_job_pack_normal(struct mtk_cam_job *job,
	 struct pack_job_ops_helper *job_helper)
{
	struct mtk_cam_ctx *ctx = job->src_ctx;
	struct mtk_cam_device *cam = ctx->cam;
	int ret, i;

	job->exp_num_cur = 1;
	job->exp_num_prev = 1;
	job->hardware_scenario = MTKCAM_IPI_HW_PATH_ON_THE_FLY;
	job->sw_feature = MTKCAM_IPI_SW_FEATURE_NORMAL;
	job->sub_ratio = get_subsample_ratio(job);
	dev_dbg(cam->dev, "[%s] ctx:%d, job_type:%d, scen:%d, expnum:%d->%d, sw/scene:%d/%d",
		__func__, ctx->stream_id, job->job_type, job->job_scen.id,
		job->exp_num_prev, job->exp_num_cur, job->sw_feature, job->hardware_scenario);
	job->stream_on_seninf = false;
	if (!ctx->used_engine) {
		int selected;

		selected = mtk_cam_select_hw(job);
		if (!selected)
			return -1;

		if (mtk_cam_occupy_engine(ctx->cam, selected))
			return -1;

		mtk_cam_pm_runtime_engines(&ctx->cam->engines, selected, 1);
		ctx->used_engine = selected;

		/* raw */
		if (ctx->hw_raw) {
			struct mtk_raw_device *raw = dev_get_drvdata(ctx->hw_raw);

			initialize(raw, 0);
		}

		/* camsv */
		if (ctx->hw_sv) {
			struct mtk_camsv_device *sv = dev_get_drvdata(ctx->hw_sv);

			mtk_cam_sv_dev_config(sv);
		}

		/* mraw */
		for (i = 0 ; i < ARRAY_SIZE(ctx->hw_mraw); i++) {
			if (ctx->hw_mraw[i]) {
				struct mtk_mraw_device *mraw =
					dev_get_drvdata(ctx->hw_mraw[i]);

				mtk_cam_mraw_dev_config(mraw);
			}
		}

		job->stream_on_seninf = true;
	}
	/* config_flow_by_job_type */
	job->used_engine = ctx->used_engine;
	job->hw_raw = ctx->hw_raw;

	job->do_ipi_config = false;
	if (!ctx->configured) {
		/* handle camsv tags */
		if (handle_sv_tag_hdr(job)) {
			dev_info(cam->dev, "tag handle failed");
			return -1;
		}

		/* if has raw */
		if (USED_MASK_GET_SUBMASK(&ctx->used_engine, raw)) {
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

		apply_raw_target_clk(ctx, job->req);
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
	int ret, i;

	job->exp_num_cur = 1;
	job->exp_num_prev = 1;
	job->hardware_scenario = MTKCAM_IPI_HW_PATH_OFFLINE;
	job->sw_feature = MTKCAM_IPI_SW_FEATURE_NORMAL;
	job->sub_ratio = get_subsample_ratio(job);
	dev_dbg(cam->dev, "[%s] ctx:%d, job_type:%d, scen:%d, expnum:%d->%d, sw/scene:%d/%d",
		__func__, ctx->stream_id, job->job_type, job->job_scen.id,
		job->exp_num_prev, job->exp_num_cur, job->sw_feature, job->hardware_scenario);
	job->stream_on_seninf = false;
	if (!ctx->used_engine) {
		int selected;

		selected = mtk_cam_select_hw(job);
		if (!selected)
			return -1;

		if (mtk_cam_occupy_engine(ctx->cam, selected))
			return -1;

		mtk_cam_pm_runtime_engines(&ctx->cam->engines, selected, 1);
		ctx->used_engine = selected;

		/* raw */
		if (ctx->hw_raw) {
			struct mtk_raw_device *raw = dev_get_drvdata(ctx->hw_raw);

			initialize(raw, 0);
		}

		/* camsv */
		if (ctx->hw_sv) {
			struct mtk_camsv_device *sv = dev_get_drvdata(ctx->hw_sv);

			mtk_cam_sv_dev_config(sv);
		}

		/* mraw */
		for (i = 0 ; i < ARRAY_SIZE(ctx->hw_mraw); i++) {
			if (ctx->hw_mraw[i]) {
				struct mtk_mraw_device *mraw =
					dev_get_drvdata(ctx->hw_mraw[i]);

				mtk_cam_mraw_dev_config(mraw);
			}
		}

		//job->stream_on_seninf = true;
	}
	/* config_flow_by_job_type */
	job->used_engine = ctx->used_engine;
	job->hw_raw = ctx->hw_raw;

	job->do_ipi_config = false;
	if (!ctx->configured) {
		/* handle camsv tags */
		if (handle_sv_tag_hdr(job)) {
			dev_info(cam->dev, "tag handle failed");
			return -1;
		}

		/* if has raw */
		if (USED_MASK_GET_SUBMASK(&ctx->used_engine, raw)) {
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

		apply_raw_target_clk(ctx, job->req);
	}
	ret = mtk_cam_job_fill_ipi_frame(job, job_helper);

	return ret;
}

static int
_handle_sv_tag_only_sv(struct mtk_cam_job *job)
{
	struct mtk_cam_ctx *ctx = job->src_ctx;
	struct mtk_camsv_device *sv_dev;
	struct mtk_camsv_pipeline *sv_pipe;
	struct mtk_camsv_tag_param tag_param;
	struct v4l2_format img_fmt;
	unsigned int tag_idx, mbus_code;
	int ret = 0, i, sv_pipe_idx;

	/* reset tag info */
	sv_dev = dev_get_drvdata(ctx->hw_sv);
	mtk_cam_sv_reset_tag_info(sv_dev);

	/* img tag(s) */
	tag_idx = SVTAG_START;
	for (i = 0; i < ctx->num_sv_subdevs; i++) {
		if (tag_idx >= SVTAG_END)
			return 1;
		sv_pipe_idx = ctx->sv_subdev_idx[i];
		sv_pipe = &ctx->cam->pipelines.camsv[sv_pipe_idx];
		mbus_code = sv_pipe->pad_cfg[MTK_CAMSV_SINK].mbus_fmt.code;
		img_fmt = sv_pipe->vdev_nodes[
			MTK_CAMSV_MAIN_STREAM_OUT - MTK_CAMSV_SINK_NUM].active_fmt;
		tag_param.tag_idx = tag_idx;
		tag_param.seninf_padidx = sv_pipe->seninf_padidx;
		tag_param.tag_order = MTKCAM_IPI_ORDER_FIRST_TAG;
		mtk_cam_sv_fill_tag_info(sv_dev->tag_info,
			&tag_param, 1, 3, job->sub_ratio,
			mbus_code, sv_pipe, &img_fmt);

		sv_dev->used_tag_cnt++;
		sv_dev->enabled_tags |= (1 << tag_idx);
		tag_idx++;
	}

	return ret;
}

static int
_job_pack_only_sv(struct mtk_cam_job *job,
	 struct pack_job_ops_helper *job_helper)
{
	struct mtk_cam_ctx *ctx = job->src_ctx;
	struct mtk_cam_device *cam = ctx->cam;
	int ret;

	job->exp_num_cur = 1;
	job->exp_num_prev = 1;
	job->hardware_scenario = MTKCAM_IPI_HW_PATH_ON_THE_FLY;
	job->sw_feature = MTKCAM_IPI_SW_FEATURE_NORMAL;
	job->sub_ratio = 0;
	dev_dbg(cam->dev, "[%s] ctx:%d, job_type:%d, scen:%d, expnum:%d->%d, sw/scene:%d/%d",
		__func__, ctx->stream_id, job->job_type, job->job_scen,
		job->exp_num_prev, job->exp_num_cur, job->sw_feature, job->hardware_scenario);
	job->stream_on_seninf = false;
	if (!ctx->used_engine) {
		int selected;

		selected = mtk_cam_select_hw_only_sv(job);
		if (!selected)
			return -1;

		if (mtk_cam_occupy_engine(ctx->cam, selected))
			return -1;

		mtk_cam_pm_runtime_engines(&ctx->cam->engines, selected, 1);
		ctx->used_engine = selected;
		if (ctx->hw_sv) {
			struct mtk_camsv_device *sv = dev_get_drvdata(ctx->hw_sv);

			mtk_cam_sv_dev_config(sv);
		}

		job->stream_on_seninf = true;
	}
	/* config_flow_by_job_type */
	job->used_engine = ctx->used_engine;

	job->do_ipi_config = false;
	if (!ctx->configured) {
		/* handle camsv tags */
		if (_handle_sv_tag_only_sv(job)) {
			dev_info(cam->dev, "tag handle failed");
			return -1;
		}

		/* if has sv */
		if (USED_MASK_GET_SUBMASK(&ctx->used_engine, camsv)) {
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
	if (!ctx->not_first_job)
		ctx->not_first_job = true;
	ret = mtk_cam_job_fill_ipi_frame(job, job_helper);

	return ret;
}

static int fill_raw_img_buffer_to_ipi_frame(
	struct req_buffer_helper *helper, struct mtk_cam_buffer *buf,
	struct mtk_cam_video_device *node)
{
	struct mtkcam_ipi_frame_param *fp = helper->fp;
	int ret = -1;

	if (V4L2_TYPE_IS_CAPTURE(buf->vbb.vb2_buf.type)) {
		struct mtkcam_ipi_img_output *out;
		out = &fp->img_outs[helper->io_idx];
		++helper->io_idx;

		ret = fill_img_out(out, buf, node);
	} else {
		struct mtkcam_ipi_img_input *in;
		in = &fp->img_ins[helper->ii_idx];
		++helper->ii_idx;

		ret = fill_img_in(in, buf, node);
	}

	return ret;

}

static int fill_sv_imgo_img_buffer_to_ipi_frame(
	struct req_buffer_helper *helper, struct mtk_cam_buffer *buf,
	struct mtk_cam_video_device *node)
{
	struct mtk_cam_ctx *ctx = helper->job->src_ctx;
	struct mtkcam_ipi_frame_param *fp = helper->fp;
	struct mtkcam_ipi_img_output *out;
	struct mtk_camsv_device *sv_dev;
	struct vb2_buffer *vb;
	struct dma_info info;
	unsigned int tag_idx;
	void *vaddr;
	int ret = -1;

	if (ctx->hw_sv == NULL)
		return ret;

	sv_dev = dev_get_drvdata(ctx->hw_sv);
	tag_idx = mtk_cam_get_sv_tag_index(sv_dev, node->uid.pipe_id);

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
		((((buf->daddr + GET_PLAT_V4L2(meta_sv_ext_size)) + 15) >> 4) << 4);

	/* update meta header */
	vb = &buf->vbb.vb2_buf;
	vaddr = vb2_plane_vaddr(vb, 0);
	info.width = buf->image_info.width;
	info.height = buf->image_info.height;
	info.stride = buf->image_info.bytesperline[0];
	CALL_PLAT_V4L2(
		set_sv_meta_stats_info, node->desc.dma_port, vaddr, &info);

	return ret;
}

static void job_cancel(struct mtk_cam_job *job)
{
	int i, used_pipe = 0;

	if (job->req)
		used_pipe = job->req->used_pipe & job->src_ctx->used_pipe;

	pr_info("%s: #%d\n", __func__, job->frame_seq_no);

	kthread_cancel_work_sync(&job->sensor_work);
	cancel_work_sync(&job->meta1_done_work);
	cancel_work_sync(&job->frame_done_work);

	for (i = 0; i < MTKCAM_SUBDEV_MAX; i++) {
		if (SUBMASK_HAS(&used_pipe, all, i))
			mtk_cam_req_buffer_done(job->req, i, -1,
				VB2_BUF_STATE_ERROR, job->timestamp);
	}

}

static void job_finalize(struct mtk_cam_job *job)
{
	mtk_cam_buffer_pool_return(&job->cq);
	mtk_cam_buffer_pool_return(&job->ipi);
}

static void otf_on_transit(struct mtk_cam_job_state *s, int state_type,
			   int old_state, int new_state, int act,
			   struct mtk_cam_ctrl_runtime_info *info)
{
	struct mtk_cam_job *job =
		container_of(s, struct mtk_cam_job, job_state);

	if (CAM_DEBUG_ENABLED(STATE))
		pr_info("%s: #%d %s: %s -> %s, act %d\n",
			__func__, s->seq_no,
			str_state_type(state_type),
			str_state(state_type, old_state),
			str_state(state_type, new_state),
			act);

	if (state_type == ISP_STATE)
	{
		switch (new_state)
		{
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
			}
			break;
		}
	}
}

static struct mtk_cam_job_ops otf_job_ops = {
	.cancel = job_cancel,
	//.dump
	.finalize = job_finalize,
	.compose_done = _compose_done,
	.compose = _compose,
	.stream_on = _stream_on,
	//.reset
	.apply_sensor = _apply_sensor,
	.apply_isp = _apply_cq,
	.handle_afo_done = job_afo_done,
	.handle_buffer_done = job_buffer_done,
};

static struct mtk_cam_job_ops otf_stagger_job_ops = {
	.cancel = job_cancel,
	//.dump
	.finalize = job_finalize,
	.compose_done = _compose_done,
	.compose = _compose,
	.stream_on = stream_on_otf_stagger,
	//.reset
	.apply_sensor = _apply_sensor,
	.apply_isp = _apply_cq,
	.handle_buffer_done = job_buffer_done,
};

static struct mtk_cam_job_ops m2m_job_ops = {
	.cancel = job_cancel,
	//.dump
	.finalize = job_finalize,
	.compose_done = _compose_done,
	.compose = _compose,
	.stream_on = 0,
	//.reset
	.apply_sensor = 0,
	.apply_isp = _apply_cq,
	.trigger_isp = trigger_m2m,
	.handle_afo_done = job_afo_done,
	.handle_buffer_done = job_buffer_done,
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
	.apply_isp = _apply_sv_cq,
	.handle_buffer_done = job_buffer_done,
};

static struct mtk_cam_job_state_cb otf_state_cb = {
	.on_transit = otf_on_transit,
};

static struct pack_job_ops_helper otf_pack_helper = {
	.update_raw_bufs_to_ipi = fill_raw_img_buffer_to_ipi_frame,
	.update_raw_imgo_to_ipi = NULL,
	.update_sv_imgo_to_ipi = fill_sv_imgo_img_buffer_to_ipi_frame,
	.pack_job_check_ipi_buffer = NULL,
	.pack_job = _job_pack_normal,
};

static struct pack_job_ops_helper stagger_pack_helper = {
	.update_raw_bufs_to_ipi = fill_raw_img_buffer_to_ipi_frame,
	.update_raw_imgo_to_ipi = fill_imgo_img_buffer_to_ipi_frame_stagger,
	.update_sv_imgo_to_ipi = fill_sv_imgo_img_buffer_to_ipi_frame,
	.pack_job_check_ipi_buffer = update_work_buffer_to_ipi_frame,
	.pack_job = _job_pack_otf_stagger,
};

static struct pack_job_ops_helper m2m_pack_helper = {
	.update_raw_bufs_to_ipi = fill_raw_img_buffer_to_ipi_frame,
	.update_raw_imgo_to_ipi = NULL,
	.update_sv_imgo_to_ipi = fill_sv_imgo_img_buffer_to_ipi_frame,
	.pack_job_check_ipi_buffer = NULL,
	.pack_job = _job_pack_m2m,
};

static struct pack_job_ops_helper only_sv_pack_helper = {
	.update_sv_imgo_to_ipi = fill_sv_imgo_img_buffer_to_ipi_frame,
	.pack_job = _job_pack_only_sv,
};

static int job_factory(struct mtk_cam_job *job)
{
	struct mtk_cam_ctx *ctx = job->src_ctx;
	struct pack_job_ops_helper *pack_helper = NULL;
	int ret;

	/* only job used data */
	job->ctx_id = ctx->stream_id;
	job->sensor = ctx->sensor;

	job->sensor_hdl_obj = ctx->sensor ?
		mtk_cam_req_find_ctrl_obj(job->req, ctx->sensor->ctrl_handler) :
		NULL;
	job->composed = false;

	init_completion(&job->compose_completion);
	init_completion(&job->cq_exe_completion);

	switch (job->job_type) {
	case RAW_JOB_ON_THE_FLY:
		pack_helper = &otf_pack_helper;

		mtk_cam_job_state_init_basic(&job->job_state, &otf_state_cb);
		job->ops = &otf_job_ops;

		job->sensor_set_margin = SENSOR_SET_MARGIN_MS;
		job->sensor_set_ref = CAMSYS_EVENT_SOURCE_RAW;
		job->state_trans_ref = CAMSYS_EVENT_SOURCE_RAW;
		break;
	case RAW_JOB_STAGGER:
		pack_helper = &stagger_pack_helper;

		mtk_cam_job_state_init_basic(&job->job_state, &otf_state_cb);
		job->ops = &otf_stagger_job_ops;

		job->sensor_set_margin = SENSOR_SET_MARGIN_MS_STAGGER;
		job->sensor_set_ref = CAMSYS_EVENT_SOURCE_CAMSV;
		job->state_trans_ref = CAMSYS_EVENT_SOURCE_RAW;
		break;
	case RAW_JOB_M2M:
		pack_helper = &m2m_pack_helper;

		mtk_cam_job_state_init_m2m(&job->job_state, NULL);
		job->ops = &m2m_job_ops;

		job->sensor_set_margin = 0;
		job->sensor_set_ref = CAMSYS_EVENT_SOURCE_RAW;
		job->state_trans_ref = CAMSYS_EVENT_SOURCE_RAW;
		break;
#if 0
	case RAW_JOB_HW_SUBSAMPLE:
		pack_helper->update_imgo_to_ipi = fill_imgo_img_buffer_to_ipi_frame_subsample;
		pack_helper->update_yuvo_to_ipi = fill_yuvo_img_buffer_to_ipi_frame_subsample;
		pack_helper->update_sv_imgo_to_ipi = fill_sv_imgo_img_buffer_to_ipi_frame;
		pack_helper->pack_job_check_ipi_buffer = NULL;
		pack_helper->pack_job = _job_pack_subsample;
		job->sensor_set_margin = SENSOR_SET_MARGIN_MS;
		job->sensor_set_ref = CAMSYS_EVENT_SOURCE_RAW;
		job->state_trans_ref = CAMSYS_EVENT_SOURCE_RAW;
		break;
#endif
	case RAW_JOB_ONLY_SV:
		pack_helper = &only_sv_pack_helper;

		mtk_cam_job_state_init_basic(&job->job_state, &otf_state_cb);
		job->ops = &otf_only_sv_job_ops;

		job->sensor_set_margin = SENSOR_SET_MARGIN_MS;
		job->sensor_set_ref = CAMSYS_EVENT_SOURCE_CAMSV;
		job->state_trans_ref = CAMSYS_EVENT_SOURCE_CAMSV;
		break;
	default:
		break;
	}

	if (WARN_ON(!pack_helper))
		return -1;

	ret = pack_helper->pack_job(job, pack_helper);

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
	int n_maps = config->n_maps;

	WARN_ON(n_maps >= ARRAY_SIZE(config->maps));
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
	input->subsample = subsample;
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
	input->subsample = subsample;
	input->in_crop = v4l2_rect_to_ipi_crop(&sink->crop);

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
	int raw_pipe_idx;
	int i;

	memset(config, 0, sizeof(*config));

	/* assume: at most one raw-subdev is used */
	raw_pipe_idx = get_raw_subdev_idx(req->used_pipe);
	if (raw_pipe_idx != -1) {
		struct mtk_raw_sink_data *sink =
			&req->raw_data[raw_pipe_idx].sink;
		int raw_dev;

		config->flags = MTK_CAM_IPI_CONFIG_TYPE_INIT;
		config->sw_feature = job->sw_feature;

		raw_set_ipi_input_param(input, sink, 1, 1, job->sub_ratio); /* TODO */

		raw_dev = USED_MASK_GET_SUBMASK(&used_engine, raw);
		ipi_add_hw_map(config, MTKCAM_SUBDEV_RAW_0, raw_dev);
	}

	/* camsv */
	if (ctx->hw_sv) {
		struct mtk_camsv_device *sv_dev = dev_get_drvdata(ctx->hw_sv);
		for (i = SVTAG_START; i < SVTAG_END; i++) {
			if (sv_dev->enabled_tags & (1 << i)) {
				sv_input = &config->sv_input[0][i];

				sv_input->pipe_id = sv_dev->id + MTKCAM_SUBDEV_CAMSV_START;
				sv_input->tag_id = i;
				sv_input->tag_order = sv_dev->tag_info[i].tag_order;;
				sv_input->is_first_frame = (ctx->not_first_job) ? 0 : 1;
				sv_input->input = sv_dev->tag_info[i].cfg_in_param;
			}
		}
	}

	/* mraw */
	for (i = 0; i < ctx->num_mraw_subdevs; i++) {
		struct mtk_mraw_sink_data *sink =
			&req->mraw_data[ctx->mraw_subdev_idx[i]].sink;

		mraw_input = &config->mraw_input[i];
		mraw_input->pipe_id =
			ctx->mraw_subdev_idx[i] + MTKCAM_SUBDEV_MRAW_START;

		mraw_set_ipi_input_param(&mraw_input->input, sink, 3, 1, job->sub_ratio);
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
	config->sw_feature = job->sw_feature;

	for (i = SVTAG_START; i < SVTAG_END; i++) {
		if (sv_dev->enabled_tags & (1 << i)) {
			sv_input = &config->sv_input[0][i];

			sv_input->pipe_id = sv_dev->id + MTKCAM_SUBDEV_CAMSV_START;
			sv_input->tag_id = i;
			sv_input->tag_order = sv_dev->tag_info[i].tag_order;
			sv_input->is_first_frame = (ctx->not_first_job) ? 0 : 1;
			sv_input->input = sv_dev->tag_info[i].cfg_in_param;
		}
	}

	return 0;
}

static int update_job_cq_buffer_to_ipi_frame(struct mtk_cam_job *job,
					     struct mtkcam_ipi_frame_param *fp)
{
	struct mtk_cam_pool_buffer *cq = &job->cq;

	/* cq offset */
	fp->cur_workbuf_offset = cq->size * cq->priv.index;
	fp->cur_workbuf_size = cq->size;
	return 0;
}


static int map_ipi_imgo_path(int v4l2_raw_path)
{
	switch (v4l2_raw_path) {
	case V4L2_MTK_CAM_RAW_PATH_SELECT_BPC: return MTKCAM_IPI_IMGO_AFTER_BPC;
	case V4L2_MTK_CAM_RAW_PATH_SELECT_FUS: return MTKCAM_IPI_IMGO_AFTER_FUS;
	case V4L2_MTK_CAM_RAW_PATH_SELECT_DGN: return MTKCAM_IPI_IMGO_AFTER_DGN;
	case V4L2_MTK_CAM_RAW_PATH_SELECT_LSC: return MTKCAM_IPI_IMGO_AFTER_LSC;
	case V4L2_MTK_CAM_RAW_PATH_SELECT_LTM: return MTKCAM_IPI_IMGO_AFTER_LTM;
	default:
		break;
	}
	/* un-processed raw frame */
	return MTKCAM_IPI_IMGO_UNPROCESSED;
}

static int update_job_raw_param_to_ipi_frame(struct mtk_cam_job *job,
					     struct mtkcam_ipi_frame_param *fp)
{
	struct mtkcam_ipi_raw_frame_param *p = &fp->raw_param;
	struct mtk_cam_request *req = job->req;
	struct mtk_raw_ctrl_data *ctrl;
	int raw_pipe_idx;

	/* assume: at most one raw-subdev is used */
	raw_pipe_idx = get_raw_subdev_idx(job->src_ctx->used_pipe);
	if (raw_pipe_idx == -1)
		return 0;

	ctrl = &req->raw_data[raw_pipe_idx].ctrl;

	p->imgo_path_sel = map_ipi_imgo_path(ctrl->raw_path);
	p->hardware_scenario = job->hardware_scenario;
	p->bin_flag = BIN_OFF;
	p->exposure_num = job->exp_num_cur;
	p->previous_exposure_num = job->exp_num_prev;

	dev_info(job->src_ctx->cam->dev, "[%s] job_type:%d scen:%d exp:%d/%d", __func__,
			job->job_type, ctrl->resource.user_data.raw_res.scen.id,
			p->exposure_num, p->previous_exposure_num);
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
		break;
	case MTKCAM_IPI_RAW_IMGO:
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
	//case MTKCAM_IPI_RAW_DRZS4NO_2:
	case MTKCAM_IPI_RAW_DRZS4NO_3:
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
	int ret = -1;

	switch (node->desc.dma_port) {
	case MTKCAM_IPI_CAMSV_MAIN_OUT:
		ret = job_helper->update_sv_imgo_to_ipi(helper, buf, node);
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
	int ret = 0, i, param_idx;

	for (i = 0; i < ctx->num_mraw_subdevs; i++) {
		mraw_pipe = &ctx->cam->pipelines.mraw[ctx->mraw_subdev_idx[i]];
		if (mraw_pipe->id == node->uid.pipe_id) {
			param_idx = i;
			break;
		}
	}
	if (i == ctx->num_mraw_subdevs) {
		ret = -1;
		pr_info("%s %s: mraw subdev idx not found(pipe_id:%d)",
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
		mtk_cam_mraw_cal_cfg_info(ctx->cam,
			node->uid.pipe_id, &fp->mraw_param[param_idx]);
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
		}
		break;
	case MTKCAM_IPI_RAW_META_STATS_0:
	case MTKCAM_IPI_RAW_META_STATS_1:
		{
			struct mtkcam_ipi_meta_output *out;
			void *vaddr;

			out = &fp->meta_outputs[helper->mo_idx];
			++helper->mo_idx;

			FILL_META_IN_OUT(out, buf, node->uid);

			vaddr = vb2_plane_vaddr(&buf->vbb.vb2_buf, 0);
			ret = CALL_PLAT_V4L2(set_meta_stats_info,
					     node->desc.dma_port,
					     vaddr);

			if (node->desc.dma_port == MTKCAM_IPI_RAW_META_STATS_0) {
				struct mtk_cam_job *job = helper->job;

				job->timestamp_buf = vaddr +
					GET_PLAT_V4L2(timestamp_buffer_ofst);
			}
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
static bool belong_to_current_ctx(struct mtk_cam_job *job, int ipi_pipe_id)
{
	int ctx_used_pipe;
	int idx;
	bool ret = false;

	WARN_ON(!job->src_ctx);

	ctx_used_pipe = job->src_ctx->used_pipe;

	/* TODO: update for 7s */
	if (is_raw_subdev(ipi_pipe_id)) {
		idx = ipi_pipe_id;
		ret = USED_MASK_HAS(&ctx_used_pipe, raw, idx);
	} else if (is_camsv_subdev(ipi_pipe_id)) {
		idx = ipi_pipe_id - MTKCAM_SUBDEV_CAMSV_START;
		ret = USED_MASK_HAS(&ctx_used_pipe, camsv, idx);
	} else if (is_mraw_subdev(ipi_pipe_id)) {
		idx = ipi_pipe_id - MTKCAM_SUBDEV_MRAW_START;
		ret = USED_MASK_HAS(&ctx_used_pipe, mraw, idx);
	} else {
		WARN_ON(1);
	}

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

	if (CAM_DEBUG_ENABLED(IPI_BUF))
		pr_info("%s pipe %x buf %s\n",
			__func__, pipe_id, node->desc.name);

	/* skip if it does not belong to current ctx */
	if (!belong_to_current_ctx(helper->job, pipe_id))
		return 0;

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

static int update_job_buffer_to_ipi_frame(struct mtk_cam_job *job,
	struct mtkcam_ipi_frame_param *fp, struct pack_job_ops_helper *job_helper)
{
	struct req_buffer_helper helper;
	struct mtk_cam_request *req = job->req;
	struct mtk_cam_buffer *buf;
	int ret;

	memset(&helper, 0, sizeof(helper));
	helper.job = job;
	helper.fp = fp;

	list_for_each_entry(buf, &req->buf_list, list) {
		ret = ret || update_cam_buf_to_ipi_frame(&helper, buf, job_helper);
	}

	/* update necessary working buffer */
	if (job_helper->pack_job_check_ipi_buffer)
		ret = ret || job_helper->pack_job_check_ipi_buffer(&helper);

	reset_unused_io_of_ipi_frame(&helper);

	return ret;
}

static int mtk_cam_job_fill_ipi_frame(struct mtk_cam_job *job,
	struct pack_job_ops_helper *job_helper)
{
	struct mtkcam_ipi_frame_param *fp;
	int ret;

	fp = (struct mtkcam_ipi_frame_param *)job->ipi.vaddr;

	ret = update_job_cq_buffer_to_ipi_frame(job, fp)
		|| update_job_raw_param_to_ipi_frame(job, fp)
		|| update_job_buffer_to_ipi_frame(job, fp, job_helper);

	if (ret)
		pr_info("%s: failed.", __func__);

	return ret;
}

