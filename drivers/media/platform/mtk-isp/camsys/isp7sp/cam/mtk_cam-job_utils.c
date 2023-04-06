// SPDX-License-Identifier: GPL-2.0
//
// Copyright (c) 2019 MediaTek Inc.


#include "mtk_cam.h"
#include "mtk_cam-fmt_utils.h"
#include "mtk_cam-job_utils.h"
#include "mtk_cam-ufbc-def.h"
#include "mtk_cam-raw_ctrl.h"

static unsigned int sv_pure_raw = 1;
module_param(sv_pure_raw, uint, 0644);
MODULE_PARM_DESC(sv_pure_raw, "enable pure raw dump with casmsv");

#define buf_printk(fmt, arg...)					\
	do {							\
		if (unlikely(CAM_DEBUG_ENABLED(IPI_BUF)))	\
			pr_info("%s: " fmt, __func__, ##arg);	\
	} while (0)


static struct mtk_cam_resource_v2 *_get_job_res(struct mtk_cam_job *job)
{
	struct mtk_cam_ctx *ctx = job->src_ctx;
	struct mtk_cam_resource_v2 *res = NULL;

	if (ctx->has_raw_subdev) {
		struct mtk_raw_ctrl_data *ctrl;

		ctrl = get_raw_ctrl_data(job);
		if (!ctrl)
			return NULL;

		res = &ctrl->resource.user_data;
	}

	return res;
}

static struct mtk_cam_resource_sensor_v2 *
_get_job_sensor_res(struct mtk_cam_job *job)
{
	struct mtk_cam_ctx *ctx = job->src_ctx;
	struct mtk_cam_resource_sensor_v2 *sensor_res = NULL;

	if (ctx->has_raw_subdev) {
		struct mtk_raw_ctrl_data *ctrl;

		ctrl = get_raw_ctrl_data(job);
		if (!ctrl)
			return NULL;

		sensor_res = &ctrl->resource.user_data.sensor_res;
	} else {
		struct mtk_camsv_device *sv_dev;

		if (ctx->hw_sv == NULL)
			return NULL;
		sv_dev = dev_get_drvdata(ctx->hw_sv);

		sensor_res = &sv_dev->sensor_res;
	}

	return sensor_res;
}

u32 get_used_raw_num(struct mtk_cam_job *job)
{
	struct mtk_cam_ctx *ctx = job->src_ctx;
	struct mtk_cam_engines *eng = &ctx->cam->engines;
	unsigned long mask;
	u32 raw_cnt = 0;
	int i;

	mask = bit_map_subset_of(MAP_HW_RAW, ctx->used_engine);
	for (i = 0; i < eng->num_raw_devices && mask; i++, mask >>= 1)
		if (mask & 0x1)
			++raw_cnt;

	return raw_cnt;
}

u64 get_line_time(struct mtk_cam_job *job)
{
	struct mtk_cam_resource_sensor_v2 *sensor_res;
	u64 linet = 0;

	sensor_res = _get_job_sensor_res(job);
	if (sensor_res) {
		linet = 1000000000L * sensor_res->interval.numerator
			/ sensor_res->interval.denominator
			/ (sensor_res->height + sensor_res->vblank);
	}

	return linet;
}

u32 get_sensor_h(struct mtk_cam_job *job)
{
	struct mtk_cam_resource_sensor_v2 *sensor_res;

	sensor_res = _get_job_sensor_res(job);
	if (sensor_res)
		return sensor_res->height;

	return 0;
}

u32 get_sensor_vb(struct mtk_cam_job *job)
{
	struct mtk_cam_resource_sensor_v2 *sensor_res;

	sensor_res = _get_job_sensor_res(job);
	if (sensor_res)
		return sensor_res->vblank;

	return 0;
}

u32 get_sensor_fps(struct mtk_cam_job *job)
{
	struct mtk_cam_resource_sensor_v2 *sensor_res;
	u32 fps = 0;

	sensor_res = _get_job_sensor_res(job);
	if (sensor_res) {
		fps = sensor_res->interval.denominator /
			sensor_res->interval.numerator;
		return fps;
	}

	return 0;
}

u8 get_sensor_data_pattern(struct mtk_cam_job *job)
{
	struct mtk_cam_resource_sensor_v2 *sensor_res;

	sensor_res = _get_job_sensor_res(job);
	if (sensor_res)
		return sensor_res->pattern;

	return MTK_CAM_PATTERN_BAYER;
}

void _set_timestamp(struct mtk_cam_job *job,
	u64 time_boot, u64 time_mono)
{
	job->timestamp = time_boot;
	job->timestamp_mono = time_mono;
}

int get_raw_subdev_idx(unsigned long used_pipe)
{
	unsigned long used_raw = bit_map_subset_of(MAP_SUBDEV_RAW, used_pipe);

	return ffs(used_raw) - 1;
}

int get_sv_subdev_idx(unsigned long used_pipe)
{
	unsigned long used_sv = bit_map_subset_of(MAP_SUBDEV_CAMSV, used_pipe);

	return ffs(used_sv) - 1;
}

int get_sv_tag_idx_hdr(unsigned int exp_no, unsigned int tag_order, bool is_w)
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
			if (img_tag_param[i].tag_order == tag_order &&
				img_tag_param[i].is_w == is_w) {
				tag_idx = img_tag_param[i].tag_idx;
				break;
			}
		}
	}

EXIT:
	return tag_idx;
}

int get_hw_scenario(struct mtk_cam_job *job)
{
	struct mtk_cam_scen *scen = &job->job_scen;
	int is_dc = is_dc_mode(job);
	int is_w = is_rgbw(job);
	int hard_scenario = MTKCAM_IPI_HW_PATH_ON_THE_FLY;
	int is_sv_only = job->job_type == JOB_TYPE_ONLY_SV;

	if (is_sv_only) {
		hard_scenario = MTKCAM_IPI_HW_PATH_ON_THE_FLY;
		goto END;
	}

	switch (scen->id) {
	case MTK_CAM_SCEN_NORMAL:
		if (is_w) {
			hard_scenario = (is_dc) ? MTKCAM_IPI_HW_PATH_DC_RGBW :
				MTKCAM_IPI_HW_PATH_OTF_RGBW;
		} else if (scen->scen.normal.exp_num > 1)
			hard_scenario = is_dc ?
				MTKCAM_IPI_HW_PATH_DC_STAGGER :
				MTKCAM_IPI_HW_PATH_STAGGER;
		else
			hard_scenario = is_dc ?
				MTKCAM_IPI_HW_PATH_DC_STAGGER :
				MTKCAM_IPI_HW_PATH_ON_THE_FLY;
		break;
	case MTK_CAM_SCEN_MSTREAM:
		hard_scenario = MTKCAM_IPI_HW_PATH_MSTREAM;
		break;
	case MTK_CAM_SCEN_ODT_NORMAL:
	case MTK_CAM_SCEN_M2M_NORMAL:
		if (is_m2m_apu(job)) {
			struct mtk_raw_ctrl_data *ctrl;

			ctrl = get_raw_ctrl_data(job);
			if (WARN_ON(!ctrl))
				return -1;

			if (ctrl->apu_info.apu_path == APU_FRAME_MODE)
				hard_scenario = MTKCAM_IPI_HW_PATH_OFFLINE_ADL;
			else if (ctrl->apu_info.apu_path == APU_DC_RAW)
				hard_scenario = MTKCAM_IPI_HW_PATH_DC_ADL;
			else {
				pr_info("%s: error. apu_path = %d\n",
					__func__, ctrl->apu_info.apu_path);
				return -1;
			}
		} else if (is_w) {
			hard_scenario = MTKCAM_IPI_HW_PATH_OFFLINE_RGBW;
		} else if (is_vhdr(job))
			hard_scenario = MTKCAM_IPI_HW_PATH_OFFLINE_STAGGER;
		else
			hard_scenario = MTKCAM_IPI_HW_PATH_OFFLINE;
		break;
	case MTK_CAM_SCEN_ODT_MSTREAM:
		hard_scenario = MTKCAM_IPI_HW_PATH_OFFLINE_STAGGER;
		break;
	case MTK_CAM_SCEN_SMVR:
		hard_scenario = MTKCAM_IPI_HW_PATH_ON_THE_FLY;
		break;
	default:
		pr_info("[%s] failed. un-support scen id:%d",
			__func__, scen->id);
		break;
	}

END:
	return hard_scenario;
}

int get_sw_feature(struct mtk_cam_job *job)
{
	return is_vhdr(job) ?
		MTKCAM_IPI_SW_FEATURE_VHDR : MTKCAM_IPI_SW_FEATURE_NORMAL;
}

static int scen_exp_num(struct mtk_cam_scen *scen)
{
	int exp = 1;

	switch (scen->id) {
	case MTK_CAM_SCEN_NORMAL:
	case MTK_CAM_SCEN_ODT_NORMAL:
	case MTK_CAM_SCEN_M2M_NORMAL:
		if (scen->scen.normal.exp_num == 0)
			pr_info("%s: error: NORMAL SCEN(%d) w/o setting exp_num",
					__func__, scen->id);
		else
			exp = scen->scen.normal.exp_num;
		break;
	case MTK_CAM_SCEN_MSTREAM:
	case MTK_CAM_SCEN_ODT_MSTREAM:
		switch (scen->scen.mstream.type) {
		case MTK_CAM_MSTREAM_NE_SE:
		case MTK_CAM_MSTREAM_SE_NE:
			exp = 2;
			break;
		case MTK_CAM_MSTREAM_1_EXPOSURE:
			exp = 1;
			break;
		default:
			break;
		}
		break;
	case MTK_CAM_SCEN_SMVR:
	default:
		break;
	}

	return exp;
}

int job_prev_exp_num(struct mtk_cam_job *job)
{
	struct mtk_cam_scen *scen = &job->prev_scen;

	return scen_exp_num(scen);
}

int job_exp_num(struct mtk_cam_job *job)
{
	struct mtk_cam_scen *scen = &job->job_scen;

	return scen_exp_num(scen);
}

int scen_max_exp_num(struct mtk_cam_scen *scen)
{
	int exp = 1;

	switch (scen->id) {
	case MTK_CAM_SCEN_NORMAL:
	case MTK_CAM_SCEN_ODT_NORMAL:
	case MTK_CAM_SCEN_M2M_NORMAL:
		exp = scen->scen.normal.max_exp_num;
		break;
	case MTK_CAM_SCEN_MSTREAM:
	case MTK_CAM_SCEN_ODT_MSTREAM:
		exp = 2;
		break;
	//case MTK_CAM_SCEN_SMVR:
	default:
		break;
	}
	return exp;
}

int get_subsample_ratio(struct mtk_cam_scen *scen)
{
	if (scen->id == MTK_CAM_SCEN_SMVR) {
		int sub_num = scen->scen.smvr.subsample_num;

		if (sub_num > 32 || (sub_num & (sub_num - 1))) {
			pr_info("%s: error. wrong subsample_num %d\n",
				__func__, sub_num);
			return 1;
		}
		return sub_num;
	}
	return 1;
}

#define SENSOR_I2C_TIME_NS		(6 * 1000000ULL)
#define SENSOR_I2C_TIME_NS_60FPS	(6 * 1000000ULL)
#define SENSOR_I2C_TIME_NS_HIGH_FPS	(3 * 1000000ULL)

#define INTERVAL_NS(fps)	(1000000000ULL / fps)

static u64 reserved_i2c_time(u64 frame_interval_ns)
{
	u64 i2c_time;

	/* > 60fps */
	if (frame_interval_ns < INTERVAL_NS(60))
		i2c_time = SENSOR_I2C_TIME_NS_HIGH_FPS;
	else if (INTERVAL_NS(60) <= frame_interval_ns &&
		 frame_interval_ns < INTERVAL_NS(30))
		i2c_time = SENSOR_I2C_TIME_NS_60FPS;
	else
		i2c_time = SENSOR_I2C_TIME_NS;
	return i2c_time;
}

u64 infer_i2c_deadline_ns(struct mtk_cam_scen *scen, u64 frame_interval_ns)
{
	if (scen->id != MTK_CAM_SCEN_SMVR)
		return frame_interval_ns - reserved_i2c_time(frame_interval_ns);

	/* consider vsync is subsampled */
	return frame_interval_ns * (scen->scen.smvr.subsample_num - 1);
}

unsigned int _get_master_engines(unsigned int used_engine)
{
	unsigned int master_engine = used_engine & ~bit_map_subset_mask(MAP_HW_RAW);
	int master_raw_id = _get_master_raw_id(used_engine);

	if (master_raw_id != -1)
		master_engine |= bit_map_bit(MAP_HW_RAW, master_raw_id);

	return master_engine;
}

unsigned int
_get_master_raw_id(unsigned int used_engine)
{
	used_engine = bit_map_subset_of(MAP_HW_RAW, used_engine);

	return ffs(used_engine) - 1;
}

unsigned int
_get_master_sv_id(unsigned int used_engine)
{
	used_engine = bit_map_subset_of(MAP_HW_CAMSV, used_engine);

	return ffs(used_engine) - 1;
}

static int mtk_cam_fill_img_in_buf(struct mtkcam_ipi_img_input *ii,
				    struct mtk_cam_buffer *buf)
{
	struct mtk_cam_cached_image_info *img_info = &buf->image_info;
	dma_addr_t daddr;
	int i;

	ii->buf[0].ccd_fd = buf->vbb.vb2_buf.planes[0].m.fd;

	daddr = buf->daddr;
	for (i = 0; i < ARRAY_SIZE(img_info->bytesperline); i++) {
		unsigned int size = img_info->size[i];

		if (!size)
			break;

		ii->buf[i].iova = daddr;
		ii->buf[i].size = size;
		daddr += size;
	}

	return 0;
}

static int fill_img_in_driver_buf(struct mtkcam_ipi_img_input *ii,
				  struct mtkcam_ipi_uid uid,
				  struct mtk_cam_driver_buf_desc *desc,
				  struct mtk_cam_pool_buffer *buf)
{
	int i;
	struct mtk_cam_buf_fmt_desc *fmt_desc = get_fmt_desc(desc);

	/* uid */
	ii->uid = uid;

	/* fmt */
	ii->fmt.format = fmt_desc->ipi_fmt;
	ii->fmt.s = (struct mtkcam_ipi_size) {
		.w = fmt_desc->width,
		.h = fmt_desc->height,
	};

	for (i = 0; i < ARRAY_SIZE(ii->fmt.stride); i++)
		ii->fmt.stride[i] = i < ARRAY_SIZE(fmt_desc->stride) ?
			fmt_desc->stride[i] : 0;

	/* buf */
	ii->buf[0].size = fmt_desc->size;
	ii->buf[0].iova = buf->daddr;
	ii->buf[0].ccd_fd = desc->fd; /* TODO: ufo : desc->fd; */

	buf_printk("%dx%d sz %zu/%d iova %pad\n",
		   fmt_desc->width, fmt_desc->height,
		   fmt_desc->size, buf->size, &buf->daddr);

	return 0;
}

static int fill_img_out_driver_buf(struct mtkcam_ipi_img_output *io,
				  struct mtkcam_ipi_uid uid,
				  struct mtk_cam_driver_buf_desc *desc,
				  struct mtk_cam_pool_buffer *buf)
{
	int i;
	struct mtk_cam_buf_fmt_desc *fmt_desc = get_fmt_desc(desc);

	/* uid */
	io->uid = uid;

	/* fmt */
	io->fmt.format = fmt_desc->ipi_fmt;
	io->fmt.s = (struct mtkcam_ipi_size) {
		.w = fmt_desc->width,
		.h = fmt_desc->height,
	};

	for (i = 0; i < ARRAY_SIZE(io->fmt.stride); i++)
		io->fmt.stride[i] = i < ARRAY_SIZE(fmt_desc->stride) ?
			fmt_desc->stride[i] : 0;

	/* buf */
	io->buf[0][0].size = fmt_desc->size;
	io->buf[0][0].iova = buf->daddr;
	io->buf[0][0].ccd_fd = desc->fd; /* TODO: ufo : desc->fd; */

	/* crop */
	io->crop = (struct mtkcam_ipi_crop) {
		.p = (struct mtkcam_ipi_point) {
			.x = 0,
			.y = 0,
		},
		.s = (struct mtkcam_ipi_size) {
			.w = fmt_desc->width,
			.h = fmt_desc->height,
		},
	};

	buf_printk("%dx%d sz %zu/%d iova %pad\n",
		   fmt_desc->width, fmt_desc->height,
		   fmt_desc->size, buf->size, &buf->daddr);

	return 0;
}

static int fill_sv_img_fp_working_buffer(struct req_buffer_helper *helper,
	struct mtk_cam_driver_buf_desc *desc,
	struct mtk_cam_pool_buffer *buf, int exp_no, bool is_w)
{
	struct mtkcam_ipi_frame_param *fp = helper->fp;
	struct mtk_cam_job *job = helper->job;
	struct mtk_cam_ctx *ctx = job->src_ctx;
	struct mtk_camsv_device *sv_dev;
	struct mtkcam_ipi_img_output *out;
	struct mtkcam_ipi_uid uid;
	unsigned int tag_idx;
	unsigned int job_exp_no = 0;
	int ret = 0;

	if (ctx->hw_sv == NULL)
		goto EXIT;

	sv_dev = dev_get_drvdata(ctx->hw_sv);

	job_exp_no = job_exp_num(job);
	if (job_exp_no > 3) {
		ret = -1;
		pr_info("%s: over maximum exposure number(exp_no:%d)",
			__func__, job_exp_no);
		goto EXIT;
	}

	tag_idx = (is_dc_mode(job) && job_exp_no > 1 && (exp_no + 1) == job_exp_no) ?
		get_sv_tag_idx_hdr(job_exp_no, MTKCAM_IPI_ORDER_LAST_TAG, is_w) :
		get_sv_tag_idx_hdr(job_exp_no, exp_no, is_w);
	if (tag_idx == -1) {
		ret = -1;
		pr_info("%s: tag_idx not found(exp_no:%d)", __func__, job_exp_no);
		goto EXIT;
	}

	uid.pipe_id = sv_dev->id + MTKCAM_SUBDEV_CAMSV_START;
	uid.id = MTKCAM_IPI_CAMSV_MAIN_OUT;

	fp->camsv_param[0][tag_idx].pipe_id = uid.pipe_id;
	fp->camsv_param[0][tag_idx].tag_id = tag_idx;
	fp->camsv_param[0][tag_idx].hardware_scenario = get_hw_scenario(job);

	out = &fp->camsv_param[0][tag_idx].camsv_img_outputs[0];
	ret = fill_img_out_driver_buf(out, uid, desc, buf);

EXIT:
	return ret;
}

static const int otf_2exp_rawi[1] = {
	MTKCAM_IPI_RAW_RAWI_2
};
static const int otf_3exp_rawi[2] = {
	MTKCAM_IPI_RAW_RAWI_2, MTKCAM_IPI_RAW_RAWI_3
};
static const int dc_1exp_rawi[1] = {
	MTKCAM_IPI_RAW_RAWI_5
};
static const int dc_2exp_rawi[2] = {
	MTKCAM_IPI_RAW_RAWI_2, MTKCAM_IPI_RAW_RAWI_5
};
static const int dc_3exp_rawi[3] = {
	MTKCAM_IPI_RAW_RAWI_2, MTKCAM_IPI_RAW_RAWI_3, MTKCAM_IPI_RAW_RAWI_5
};

int raw_video_id_w_port(int rawi_id)
{
	switch (rawi_id) {
	case MTKCAM_IPI_RAW_IMGO:
		return MTKCAM_IPI_RAW_IMGO_W;
	case MTKCAM_IPI_RAW_RAWI_2:
		return MTKCAM_IPI_RAW_RAWI_2_W;
	case MTKCAM_IPI_RAW_RAWI_3:
		return MTKCAM_IPI_RAW_RAWI_3_W;
	case MTKCAM_IPI_RAW_RAWI_5:
		return MTKCAM_IPI_RAW_RAWI_5_W;
	default:
		WARN_ON(1);
		return MTKCAM_IPI_RAW_RAWI_2_W;
	}
}

static int fill_sv_to_rawi_wbuf(struct req_buffer_helper *helper,
		__u8 pipe_id, __u8 ipi, int exp_no, bool is_w,
		struct mtk_cam_driver_buf_desc *buf_desc,
		struct mtk_cam_pool_buffer *buf)
{
	int ret = 0;
	struct mtkcam_ipi_frame_param *fp = helper->fp;
	struct mtkcam_ipi_img_input *ii;
	struct mtkcam_ipi_uid uid;

	uid.pipe_id = pipe_id;
	uid.id = ipi;
	ii = &fp->img_ins[helper->ii_idx];
	++helper->ii_idx;

	ret = fill_img_in_driver_buf(ii, uid, buf_desc, buf);

	if (helper->job->job_type != JOB_TYPE_MSTREAM) {
		/* HS_TODO: dc? */
		ret = ret || fill_sv_img_fp_working_buffer(helper, buf_desc, buf, exp_no, is_w);
	}

	return ret;
}

void get_stagger_rawi_table(struct mtk_cam_job *job,
		const int **rawi_table, int *cnt)
{
	int exp_num_cur = job_exp_num(job);
	bool without_tg = is_dc_mode(job) || is_m2m(job);

	switch (exp_num_cur) {
	case 1:
		(*rawi_table) = without_tg ? dc_1exp_rawi : NULL;
		*cnt = without_tg ? ARRAY_SIZE(dc_1exp_rawi) : 0;
		break;
	case 2:
		(*rawi_table) = without_tg ? dc_2exp_rawi : otf_2exp_rawi;
		*cnt = without_tg ?
			ARRAY_SIZE(dc_2exp_rawi) : ARRAY_SIZE(otf_2exp_rawi);
		break;
	case 3:
		(*rawi_table) = without_tg ? dc_3exp_rawi : otf_3exp_rawi;
		*cnt = without_tg ?
			ARRAY_SIZE(dc_3exp_rawi) : ARRAY_SIZE(otf_3exp_rawi);
		break;
	default:
		break;
	}
}

int update_work_buffer_to_ipi_frame(struct req_buffer_helper *helper)
{
	struct mtk_cam_job *job = helper->job;
	struct mtk_cam_ctx *ctx = job->src_ctx;
	const int *rawi_table = NULL;
	int raw_table_size = 0;
	int ret = 0;
	int i;

	if (helper->filled_hdr_buffer)
		return 0;

	get_stagger_rawi_table(job, &rawi_table, &raw_table_size);

	/* no need img working buffer */
	if (!rawi_table)
		return ret;

	for (i = 0 ; i < raw_table_size; i++) {
		ret = mtk_cam_buffer_pool_fetch(&ctx->img_work_pool, &job->img_work_buf);
		if (ret) {
			pr_info("[%s] fail to fetch\n", __func__);
			return ret;
		}

		ret = fill_sv_to_rawi_wbuf(helper, get_raw_subdev_idx(ctx->used_pipe),
				rawi_table[i], i, false,
				&ctx->img_work_buf_desc, &job->img_work_buf);

		mtk_cam_buffer_pool_return(&job->img_work_buf);

		if (!ret && job->job_scen.scen.normal.w_chn_enabled) {
			ret = mtk_cam_buffer_pool_fetch(&ctx->img_work_pool, &job->img_work_buf);
			if (ret) {
				pr_info("[%s] fail to fetch\n", __func__);
				return ret;
			}

			ret = fill_sv_to_rawi_wbuf(helper, get_raw_subdev_idx(ctx->used_pipe),
					raw_video_id_w_port(rawi_table[i]), i, true,
					&ctx->img_work_buf_desc, &job->img_work_buf);

			mtk_cam_buffer_pool_return(&job->img_work_buf);
		}
	}

	return ret;
}

static int fill_img_fmt(struct mtkcam_ipi_pix_fmt *ipi_pfmt,
			struct mtk_cam_buffer *buf)
{
	struct mtk_cam_cached_image_info *info = &buf->image_info;
	int i;

	ipi_pfmt->format = mtk_cam_get_img_fmt(info->v4l2_pixelformat);
	ipi_pfmt->s = (struct mtkcam_ipi_size) {
		.w = info->width,
		.h = info->height,
	};

	for (i = 0; i < ARRAY_SIZE(ipi_pfmt->stride); i++)
		ipi_pfmt->stride[i] = i < ARRAY_SIZE(info->bytesperline) ?
			info->bytesperline[i] : 0;
	return 0;
}

int fill_img_in_hdr(struct mtkcam_ipi_img_input *ii,
			struct mtk_cam_buffer *buf,
			struct mtk_cam_video_device *node, int index, int id)
{
	/* uid */
	ii->uid.pipe_id = node->uid.pipe_id;
	ii->uid.id = id;
	/* fmt */
	fill_img_fmt(&ii->fmt, buf);

	/* FIXME: porting workaround */
	ii->buf[0].size = buf->image_info.size[0];
	ii->buf[0].iova = buf->daddr + index * (dma_addr_t)buf->image_info.size[0];
	ii->buf[0].ccd_fd = buf->vbb.vb2_buf.planes[0].m.fd;

	buf_printk("id:%d idx:%d buf->daddr:0x%llx, io->buf[0][0].iova:0x%llx, size:%d",
		   id, index, buf->daddr, ii->buf[0].iova, ii->buf[0].size);

	return 0;
}

int fill_img_in_by_exposure(struct req_buffer_helper *helper,
	struct mtk_cam_buffer *buf,
	struct mtk_cam_video_device *node)
{
	int ret = 0;
	struct mtkcam_ipi_frame_param *fp = helper->fp;
	struct mtkcam_ipi_img_input *in;
	struct mtk_cam_job *job = helper->job;
	bool is_w = is_rgbw(job);
	const int *rawi_table = NULL;
	int i = 0, rawi_cnt = 0;
	int index = 0;

	get_stagger_rawi_table(job, &rawi_table, &rawi_cnt);
	for (i = 0; i < rawi_cnt; i++) {
		in = &fp->img_ins[helper->ii_idx++];

		ret = fill_img_in_hdr(in, buf, node, index++, rawi_table[i]);

		if (!ret && is_w) {
			in = &fp->img_ins[helper->ii_idx++];
			ret = fill_img_in_hdr(in, buf, node, index++,
					raw_video_id_w_port(rawi_table[i]));
		}
	}

	return ret;
}

int fill_m2m_rawi_to_img_in_ipi(struct req_buffer_helper *helper,
	struct mtk_cam_buffer *buf,
	struct mtk_cam_video_device *node)
{
	int ret = 0;
	struct mtk_cam_job *job = helper->job;

	if (is_m2m_apu(job)) {
		struct mtkcam_ipi_frame_param *fp = helper->fp;
		struct mtkcam_ipi_img_input *in;

		in = &fp->img_ins[helper->ii_idx++];

		ret = fill_img_in(in, buf, node, MTKCAM_IPI_RAW_IPUI);
	} else
		ret = fill_img_in_by_exposure(helper, buf, node);

	return ret;
}

struct mtkcam_ipi_crop
v4l2_rect_to_ipi_crop(const struct v4l2_rect *r)
{
	return (struct mtkcam_ipi_crop) {
		.p = (struct mtkcam_ipi_point) {
			.x = r->left,
			.y = r->top,
		},
		.s = (struct mtkcam_ipi_size) {
			.w = r->width,
			.h = r->height,
		},
	};
}

bool ipi_crop_eq(const struct mtkcam_ipi_crop *s,
				 const struct mtkcam_ipi_crop *d)
{
	return ((s->p.x == d->p.x) && (s->p.y == d->p.y) &&
		(s->s.w == d->s.w) && (s->s.h == d->s.h));
}

int fill_imgo_out_subsample(struct mtkcam_ipi_img_output *io,
			struct mtk_cam_buffer *buf,
			struct mtk_cam_video_device *node,
			int subsample_ratio)
{
	int i;

	/* uid */
	io->uid = node->uid;

	/* fmt */
	fill_img_fmt(&io->fmt, buf);

	for (i = 0; i < subsample_ratio; i++) {
		/* FIXME: porting workaround */
		io->buf[i][0].size = buf->image_info.size[0];
		io->buf[i][0].iova = buf->daddr + i * (dma_addr_t) io->buf[i][0].size;
		io->buf[i][0].ccd_fd = buf->vbb.vb2_buf.planes[0].m.fd;
		buf_printk("i=%d: buf->daddr:0x%llx, io->buf[i][0].iova:0x%llx, size:%d",
			   i, buf->daddr, io->buf[i][0].iova, io->buf[i][0].size);
	}

	/* crop */
	io->crop = v4l2_rect_to_ipi_crop(&buf->image_info.crop);

	buf_printk("%s %dx%d @%d,%d-%dx%d\n",
		   node->desc.name,
		   io->fmt.s.w, io->fmt.s.h,
		   io->crop.p.x, io->crop.p.y, io->crop.s.w, io->crop.s.h);

	return 0;
}

int fill_img_out_hdr(struct mtkcam_ipi_img_output *io,
		     struct mtk_cam_buffer *buf,
		     struct mtk_cam_video_device *node,
		     int index, int id)
{
	/* uid */
	io->uid.pipe_id = node->uid.pipe_id;
	io->uid.id = id;

	/* fmt */
	fill_img_fmt(&io->fmt, buf);

	/* FIXME: porting workaround */
	io->buf[0][0].size = buf->image_info.size[0];
	io->buf[0][0].iova = buf->daddr + index * (dma_addr_t)io->buf[0][0].size;
	io->buf[0][0].ccd_fd = buf->vbb.vb2_buf.planes[0].m.fd;

	/* crop */
	io->crop = v4l2_rect_to_ipi_crop(&buf->image_info.crop);

	buf_printk("buf->daddr:0x%llx, io->buf[0][0].iova:0x%llx, size:%d",
		   buf->daddr, io->buf[0][0].iova, io->buf[0][0].size);
	buf_printk("%s %dx%d @%d,%d-%dx%d\n",
		   node->desc.name,
		   io->fmt.s.w, io->fmt.s.h,
		   io->crop.p.x, io->crop.p.y, io->crop.s.w, io->crop.s.h);

	return 0;
}

static int mtk_cam_fill_img_out_buf(struct mtkcam_ipi_img_output *io,
				    struct mtk_cam_buffer *buf, int index)
{
	struct mtk_cam_cached_image_info *img_info = &buf->image_info;
	dma_addr_t daddr;
	int i;

	io->buf[0][0].ccd_fd = buf->vbb.vb2_buf.planes[0].m.fd;

	daddr = buf->daddr;
	for (i = 0; i < ARRAY_SIZE(img_info->bytesperline); i++) {
		unsigned int size = img_info->size[i];

		if (!size)
			break;

		daddr += index * (dma_addr_t)size;

		io->buf[0][i].iova = daddr;
		io->buf[0][i].size = size;
		daddr += size;
	}

	return 0;
}
static int mtk_cam_fill_img_out_buf_subsample(struct mtkcam_ipi_img_output *io,
					      struct mtk_cam_buffer *buf,
					      int sub_ratio)
{
	struct mtk_cam_cached_image_info *img_info = &buf->image_info;
	dma_addr_t daddr;
	int i;
	int j;

	io->buf[0][0].ccd_fd = buf->vbb.vb2_buf.planes[0].m.fd;

	daddr = buf->daddr;
	for (j = 0; j < sub_ratio; j++) {
		for (i = 0; i < ARRAY_SIZE(img_info->bytesperline); i++) {
			unsigned int size = img_info->size[i];

			if (!size)
				break;

			io->buf[j][i].iova = daddr;
			io->buf[j][i].size = size;
			io->buf[j][i].ccd_fd = buf->vbb.vb2_buf.planes[0].m.fd;
			daddr += size;
#ifdef DEBUG_SUBSAMPLE_INFO
			buf_printk("sub/plane:%d/%d (iova,size):(0x%x/0x%x)\n",
				j, i,
				io->buf[j][i].iova, io->buf[j][i].size);
#endif
		}
	}

	return 0;
}
int fill_yuvo_out_subsample(struct mtkcam_ipi_img_output *io,
			struct mtk_cam_buffer *buf,
			struct mtk_cam_video_device *node,
			int sub_ratio)
{
	/* uid */
	io->uid = node->uid;

	/* fmt */
	fill_img_fmt(&io->fmt, buf);

	mtk_cam_fill_img_out_buf_subsample(io, buf, sub_ratio);

	/* crop */
	io->crop = v4l2_rect_to_ipi_crop(&buf->image_info.crop);

	buf_printk("%s %dx%d @%d,%d-%dx%d\n",
		   node->desc.name,
		   io->fmt.s.w, io->fmt.s.h,
		   io->crop.p.x, io->crop.p.y, io->crop.s.w, io->crop.s.h);
	return 0;
}

int fill_img_in(struct mtkcam_ipi_img_input *ii,
		struct mtk_cam_buffer *buf,
		struct mtk_cam_video_device *node,
		int id_overwite)
{
	/* uid */
	ii->uid = node->uid;

	if (id_overwite >= 0)
		ii->uid.id = (u8)id_overwite;

	/* fmt */
	fill_img_fmt(&ii->fmt, buf);

	mtk_cam_fill_img_in_buf(ii, buf);

	buf_printk("%s %dx%d id_overwrite=%d\n",
		   node->desc.name,
		   ii->fmt.s.w, ii->fmt.s.h,
		   id_overwite);
	return 0;
}

static int _fill_img_out(struct mtkcam_ipi_img_output *io,
			struct mtk_cam_buffer *buf,
			struct mtk_cam_video_device *node, int index)
{
	/* uid */
	io->uid = node->uid;

	/* fmt */
	fill_img_fmt(&io->fmt, buf);

	mtk_cam_fill_img_out_buf(io, buf, index);

	/* crop */
	io->crop = v4l2_rect_to_ipi_crop(&buf->image_info.crop);

	buf_printk("%s %dx%d @%d,%d-%dx%d\n index %d, iova %llx",
		   node->desc.name,
		   io->fmt.s.w, io->fmt.s.h,
		   io->crop.p.x, io->crop.p.y, io->crop.s.w, io->crop.s.h,
		   index, io->buf[0][0].iova);
	return 0;
}

int fill_img_out(struct mtkcam_ipi_img_output *io,
			struct mtk_cam_buffer *buf,
			struct mtk_cam_video_device *node)
{
	return _fill_img_out(io, buf, node, 0);
}

int fill_img_out_w(struct mtkcam_ipi_img_output *io,
			struct mtk_cam_buffer *buf,
			struct mtk_cam_video_device *node)
{
	int ret = _fill_img_out(io, buf, node, 1);

	io->uid.id = raw_video_id_w_port(io->uid.id);

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
	out->buf[0][0].iova = buf->daddr + buf_ofset;

	pr_info("%s: tag_idx %d, iova %llx",
			__func__, tag_idx, out->buf[0][0].iova);

	return ret;
}

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

int fill_sv_img_fp(
	struct req_buffer_helper *helper, struct mtk_cam_buffer *buf,
	struct mtk_cam_video_device *node)
{
	struct mtk_cam_job *job = helper->job;
	struct mtk_cam_ctx *ctx = job->src_ctx;
	struct mtk_cam_scen *scen = &job->job_scen;
	struct mtk_camsv_device *sv_dev;
	unsigned int pipe_id, exp_no, buf_cnt = 0, buf_ofset = 0;
	int tag_idx, i, j;
	bool is_w;
	int ret = 0;

	if (node->desc.id != MTK_RAW_PURE_RAW_OUT)
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

int fill_imgo_buf_as_working_buf(
	struct req_buffer_helper *helper, struct mtk_cam_buffer *buf,
	struct mtk_cam_video_device *node)
{
	struct mtkcam_ipi_frame_param *fp = helper->fp;
	struct mtkcam_ipi_img_output *out;
	struct mtk_cam_job *job = helper->job;
	bool is_w = is_rgbw(job);
	bool is_otf = !is_dc_mode(job);
	int index = 0, ii_inc = 0, ret = 0;
	bool sv_pure_raw;

	if (node->desc.id != MTK_RAW_PURE_RAW_OUT) {
		pr_info("%s: expect PURE-RAW node only", __func__);
		WARN_ON(1);
		return -1;
	}

	helper->filled_hdr_buffer = true;

	sv_pure_raw = is_sv_pure_raw(job);

	ii_inc = helper->ii_idx;
	fill_img_in_by_exposure(helper, buf, node);
	ii_inc = helper->ii_idx - ii_inc;
	index += ii_inc;

	if (is_otf && !sv_pure_raw) {
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

	if (sv_pure_raw && CAM_DEBUG_ENABLED(JOB))
		pr_info("%s:req:%s bypass pure raw node\n",
			__func__, job->req->req.debug_str);
	/* fill sv image fp */
	ret = ret || fill_sv_img_fp(helper, buf, node);

	return ret;
}

int get_sv_tag_idx(unsigned int exp_no, unsigned int tag_order, bool is_w)
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
			if (img_tag_param[i].tag_order == tag_order &&
				img_tag_param[i].is_w == is_w) {
				tag_idx = img_tag_param[i].tag_idx;
				break;
			}
		}
	}

EXIT:
	return tag_idx;
}

bool is_sv_pure_raw(struct mtk_cam_job *job)
{
	if (!job)
		return false;

	return job->is_sv_pure_raw && sv_pure_raw;
}

bool is_vhdr(struct mtk_cam_job *job)
{
	return scen_is_vhdr(&job->job_scen);
}

bool is_dc_mode(struct mtk_cam_job *job)
{
	struct mtk_cam_resource_v2 *res;

	res = _get_job_res(job);
	if (!res)
		return false;

	return scen_is_dc_mode(&res->raw_res);
}

bool is_rgbw(struct mtk_cam_job *job)
{
	return scen_is_rgbw(&job->job_scen);
}

bool is_m2m(struct mtk_cam_job *job)
{
	return scen_is_m2m(&job->job_scen);
}

bool is_m2m_apu(struct mtk_cam_job *job)
{
	struct mtk_raw_ctrl_data *ctrl;

	ctrl = get_raw_ctrl_data(job);
	if (!ctrl)
		return 0;

	return scen_is_m2m_apu(&job->job_scen, &ctrl->apu_info);
}

bool is_m2m_apu_dc(struct mtk_cam_job *job)
{
	struct mtk_raw_ctrl_data *ctrl;

	ctrl = get_raw_ctrl_data(job);
	if (!ctrl)
		return 0;

	return scen_is_m2m_apu(&job->job_scen, &ctrl->apu_info)
		&& apu_info_is_dc(&ctrl->apu_info);
}

int map_ipi_vpu_point(int vpu_point)
{
	switch (vpu_point) {
	case AFTER_SEP_R1: return MTKCAM_IPI_ADL_AFTER_SEP_R1;
	case AFTER_BPC: return MTKCAM_IPI_ADL_AFTER_BPC;
	case AFTER_LTM: return MTKCAM_IPI_ADL_AFTER_LTM;
	default:
		pr_info("%s: error. not supported point %d\n",
			__func__, vpu_point);
		break;
	}
	return -1;
}

int map_ipi_imgo_path(int v4l2_raw_path)
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

bool find_video_node(struct mtk_cam_job *job, int node_id)
{
	struct mtk_cam_video_device *node;
	struct mtk_cam_request *req = job->req;
	struct mtk_cam_buffer *buf;
	unsigned long ctx_used_pipe = job->src_ctx->used_pipe;

	list_for_each_entry(buf, &req->buf_list, list) {
		node = mtk_cam_buf_to_vdev(buf);

		if (node->desc.id == node_id &&
		    ctx_used_pipe & ipi_pipe_id_to_bit(node->uid.pipe_id)) {
			return true;
		}
	}

	return false;
}

bool is_pure_raw_node(struct mtk_cam_job *job,
		      struct mtk_cam_video_device *node)
{
	(void) job;
	return node->desc.id == MTK_RAW_PURE_RAW_OUT;
}

bool is_processed_raw_node(struct mtk_cam_job *job,
			    struct mtk_cam_video_device *node)
{
	struct mtk_raw_ctrl_data *ctrl = get_raw_ctrl_data(job);

	if (ctrl &&
	    map_ipi_imgo_path(ctrl->raw_path) == MTKCAM_IPI_IMGO_UNPROCESSED)
		pr_info("%s unexpected raw path", __func__);

	return node->desc.id == MTK_RAW_MAIN_STREAM_OUT;
}

struct mtk_raw_ctrl_data *get_raw_ctrl_data(struct mtk_cam_job *job)
{
	struct mtk_cam_request *req = job->req;
	int raw_pipe_idx;

	raw_pipe_idx = get_raw_subdev_idx(job->src_ctx->used_pipe);
	if (raw_pipe_idx < 0)
		return NULL;

	return &req->raw_data[raw_pipe_idx].ctrl;
}

struct mtk_raw_sink_data *get_raw_sink_data(struct mtk_cam_job *job)
{
	struct mtk_cam_request *req = job->req;
	int raw_pipe_idx;

	raw_pipe_idx = get_raw_subdev_idx(job->src_ctx->used_pipe);
	if (raw_pipe_idx < 0)
		return NULL;

	return &req->raw_data[raw_pipe_idx].sink;
}

void mtk_cam_sv_reset_tag_info(struct mtk_cam_job *job)
{
	struct mtk_camsv_tag_info *tag_info;
	int i;

	job->used_tag_cnt = 0;
	job->enabled_tags = 0;
	for (i = SVTAG_START; i < SVTAG_END; i++) {
		tag_info = &job->tag_info[i];
		tag_info->sv_pipe = NULL;
		tag_info->seninf_padidx = 0;
		tag_info->hw_scen = 0;
		tag_info->tag_order = MTKCAM_IPI_ORDER_FIRST_TAG;
	}
}

int handle_sv_tag(struct mtk_cam_job *job)
{
	struct mtk_cam_ctx *ctx = job->src_ctx;
	struct mtk_raw_sink_data *raw_sink;
	struct mtk_camsv_pipeline *sv_pipe;
	struct mtk_camsv_sink_data *sv_sink;
	struct mtk_camsv_tag_param img_tag_param[SVTAG_IMG_END];
	struct mtk_camsv_tag_param meta_tag_param;
	unsigned int tag_idx, sv_pipe_idx, hw_scen;
	unsigned int exp_no, req_amount;
	int ret = 0, i;

	/* reset tag info */
	mtk_cam_sv_reset_tag_info(job);

	/* img tag(s) */
	if (job->job_scen.scen.normal.max_exp_num == 2) {
		exp_no = req_amount = 2;
		req_amount *= is_rgbw(job) ? 2 : 1;
		hw_scen = is_dc_mode(job) ?
			(1 << HWPATH_ID(MTKCAM_IPI_HW_PATH_DC_STAGGER)) :
			(1 << HWPATH_ID(MTKCAM_IPI_HW_PATH_STAGGER));
	} else if (job->job_scen.scen.normal.max_exp_num == 3) {
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

	raw_sink = get_raw_sink_data(job);
	if (WARN_ON(!raw_sink))
		return -1;

	for (i = 0; i < req_amount; i++) {
		mtk_cam_sv_fill_tag_info(job->tag_info,
			&job->ipi_config,
			&img_tag_param[i], hw_scen, 3,
			job->sub_ratio,
			raw_sink->width, raw_sink->height,
			raw_sink->mbus_code, NULL);

		job->used_tag_cnt++;
		job->enabled_tags |= (1 << img_tag_param[i].tag_idx);

		pr_info("[%s] tag_idx:%d seninf_padidx:%d tag_order:%d width/height/mbus_code:0x%x_0x%x_0x%x\n",
			__func__,
			img_tag_param[i].tag_idx,
			img_tag_param[i].seninf_padidx,
			img_tag_param[i].tag_order,
			raw_sink->width,
			raw_sink->height,
			raw_sink->mbus_code);
	}

	/* meta tag(s) */
	tag_idx = SVTAG_META_START;
	for (i = 0; i < ctx->num_sv_subdevs; i++) {
		if (tag_idx >= SVTAG_END)
			return 1;
		sv_pipe_idx = ctx->sv_subdev_idx[i];
		if (sv_pipe_idx >= ctx->cam->pipelines.num_camsv)
			return 1;

		sv_pipe = &ctx->cam->pipelines.camsv[sv_pipe_idx];
		sv_sink = &job->req->sv_data[sv_pipe_idx].sink;
		meta_tag_param.tag_idx = tag_idx;
		meta_tag_param.seninf_padidx = sv_pipe->seninf_padidx;
		meta_tag_param.tag_order = mtk_cam_seninf_get_tag_order(
			job->seninf, sv_pipe->seninf_padidx);
		mtk_cam_sv_fill_tag_info(job->tag_info,
			&job->ipi_config,
			&meta_tag_param, 1, 3, job->sub_ratio,
			sv_sink->width, sv_sink->height,
			sv_sink->mbus_code, sv_pipe);

		job->used_tag_cnt++;
		job->enabled_tags |= (1 << tag_idx);
		tag_idx++;

		pr_info("[%s] tag_idx:%d seninf_padidx:%d tag_order:%d width/height/mbus_code:0x%x_0x%x_0x%x\n",
			__func__,
			meta_tag_param.tag_idx,
			meta_tag_param.seninf_padidx,
			meta_tag_param.tag_order,
			sv_sink->width,
			sv_sink->height,
			sv_sink->mbus_code);
	}

	ctx->used_tag_cnt = job->used_tag_cnt;
	ctx->enabled_tags = job->enabled_tags;
	memcpy(ctx->tag_info, job->tag_info,
		sizeof(struct mtk_camsv_tag_info) * CAMSV_MAX_TAGS);

	return ret;
}

int handle_sv_tag_display_ic(struct mtk_cam_job *job)
{
	struct mtk_cam_ctx *ctx = job->src_ctx;
	struct mtk_camsv_pipeline *sv_pipe;
	struct mtk_camsv_tag_param tag_param[3];
	struct v4l2_format *img_fmt;
	unsigned int width, height, mbus_code;
	unsigned int hw_scen;
	int ret = 0, i, sv_pipe_idx;

	/* reset tag info */
	mtk_cam_sv_reset_tag_info(job);

	if (ctx->num_sv_subdevs != 1)
		return 1;

	sv_pipe_idx = ctx->sv_subdev_idx[0];
	sv_pipe = &ctx->cam->pipelines.camsv[sv_pipe_idx];
	hw_scen = (1 << MTKCAM_SV_SPECIAL_SCENARIO_DISPLAY_IC);
	mtk_cam_sv_get_tag_param(tag_param, hw_scen, 1, 3);

	for (i = 0; i < ARRAY_SIZE(tag_param); i++) {
		if (tag_param[i].tag_idx == SVTAG_0) {
			img_fmt = &sv_pipe->vdev_nodes[
				MTK_CAMSV_MAIN_STREAM_OUT - MTK_CAMSV_SINK_NUM].active_fmt;
			width = img_fmt->fmt.pix_mp.width;
			height = img_fmt->fmt.pix_mp.height;
			if (img_fmt->fmt.pix_mp.pixelformat == V4L2_PIX_FMT_NV21)
				mbus_code = MEDIA_BUS_FMT_SBGGR8_1X8;
			else
				mbus_code = MEDIA_BUS_FMT_SBGGR10_1X10;
		} else if (tag_param[i].tag_idx == SVTAG_1) {
			img_fmt = &sv_pipe->vdev_nodes[
				MTK_CAMSV_MAIN_STREAM_OUT - MTK_CAMSV_SINK_NUM].active_fmt;
			width = img_fmt->fmt.pix_mp.width;
			height = img_fmt->fmt.pix_mp.height / 2;
			if (img_fmt->fmt.pix_mp.pixelformat == V4L2_PIX_FMT_NV21)
				mbus_code = MEDIA_BUS_FMT_SBGGR8_1X8;
			else
				mbus_code = MEDIA_BUS_FMT_SBGGR10_1X10;
		} else {
			img_fmt = &sv_pipe->vdev_nodes[
				MTK_CAMSV_EXT_STREAM_OUT - MTK_CAMSV_SINK_NUM].active_fmt;
			width = img_fmt->fmt.pix_mp.width;
			height = img_fmt->fmt.pix_mp.height;
			mbus_code = MEDIA_BUS_FMT_SBGGR8_1X8;
		}
		mtk_cam_sv_fill_tag_info(job->tag_info,
			&job->ipi_config,
			&tag_param[i], 1, 3, job->sub_ratio,
			width, height,
			mbus_code, sv_pipe);

		job->used_tag_cnt++;
		job->enabled_tags |= (1 << tag_param[i].tag_idx);
	}

	ctx->used_tag_cnt = job->used_tag_cnt;
	ctx->enabled_tags = job->enabled_tags;
	memcpy(ctx->tag_info, job->tag_info,
		sizeof(struct mtk_camsv_tag_info) * CAMSV_MAX_TAGS);

	return ret;
}

int handle_sv_tag_only_sv(struct mtk_cam_job *job)
{
	struct mtk_cam_ctx *ctx = job->src_ctx;
	struct mtk_camsv_pipeline *sv_pipe;
	struct mtk_camsv_sink_data *sv_sink;
	struct mtk_camsv_tag_param tag_param;
	unsigned int tag_idx, sv_pipe_idx;
	int ret = 0, i;

	/* reset tag info */
	mtk_cam_sv_reset_tag_info(job);

	/* img tag(s) */
	tag_idx = SVTAG_START;
	for (i = 0; i < ctx->num_sv_subdevs; i++) {
		sv_pipe_idx = ctx->sv_subdev_idx[i];
		if (sv_pipe_idx >= ctx->cam->pipelines.num_camsv)
			return 1;
		sv_pipe = &ctx->cam->pipelines.camsv[sv_pipe_idx];
		sv_sink = &job->req->sv_data[sv_pipe_idx].sink;
		tag_param.tag_idx = tag_idx;
		tag_param.seninf_padidx = sv_pipe->seninf_padidx;
		tag_param.tag_order = mtk_cam_seninf_get_tag_order(
			job->seninf, sv_pipe->seninf_padidx);
		mtk_cam_sv_fill_tag_info(job->tag_info,
			&job->ipi_config,
			&tag_param, 1, 3, job->sub_ratio,
			sv_sink->width, sv_sink->height,
			sv_sink->mbus_code, sv_pipe);

		job->used_tag_cnt++;
		job->enabled_tags |= (1 << tag_idx);
		tag_idx++;
	}

	ctx->used_tag_cnt = job->used_tag_cnt;
	ctx->enabled_tags = job->enabled_tags;
	memcpy(ctx->tag_info, job->tag_info,
		sizeof(struct mtk_camsv_tag_info) * CAMSV_MAX_TAGS);

	return ret;
}

bool is_sv_img_tag_used(struct mtk_cam_job *job)
{
	bool rst = false;

	/* HS_TODO: check all features */
	/* FIXME(AY): should check scen id before accessing */
	if (job->job_scen.scen.normal.exp_num > 1)
		rst = !is_m2m(job);
	if (is_dc_mode(job))
		rst = true;
	if (is_sv_pure_raw(job))
		rst = true;

	return rst;
}

