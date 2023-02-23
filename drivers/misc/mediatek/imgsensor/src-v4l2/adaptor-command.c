// SPDX-License-Identifier: GPL-2.0
// Copyright (c) 2022 MediaTek Inc.


#include "mtk_camera-v4l2-controls.h"
#include "adaptor.h"
#include "adaptor-fsync-ctrls.h"
#include "adaptor-common-ctrl.h"

#include "adaptor-command.h"


#define sd_to_ctx(__sd) container_of(__sd, struct adaptor_ctx, sd)

static int get_sensor_mode_info(struct adaptor_ctx *ctx, u32 mode_id,
				struct mtk_sensor_mode_info *info)
{
	int ret = 0;

	/* Test arguments */
	if (unlikely(info == NULL)) {
		ret = -EINVAL;
		adaptor_logi(ctx, "ERROR: invalid argumet info is nullptr\n");
		return ret;
	}
	if (unlikely(mode_id >= SENSOR_SCENARIO_ID_MAX)) {
		ret = -EINVAL;
		adaptor_logi(ctx, "ERROR: invalid argumet scenario %u\n", mode_id);
		return ret;
	}

	info->scenario_id = mode_id;
	info->mode_exposure_num = g_scenario_exposure_cnt(ctx, mode_id);

	return 0;
}

/*---------------------------------------------------------------------------*/
// functions that called by in-kernel drivers.
/*---------------------------------------------------------------------------*/
/* GET */
static int g_cmd_sensor_mode_config_info(struct adaptor_ctx *ctx, void *arg)
{
	int i;
	int ret = 0;
	int mode_cnt = 0;
	struct mtk_sensor_mode_config_info *p_info = NULL;

	/* error handling (unexpected case) */
	if (unlikely(arg == NULL)) {
		ret = -ENOIOCTLCMD;
		adaptor_logi(ctx,
			"ERROR: V4L2_CMD_GET_SENSOR_MODE_CONFIG_INFO, idx:%d, input arg is nullptr, return:%d\n",
			ctx->idx, ret);
		return ret;
	}

	p_info = arg;

	memset(p_info, 0, sizeof(struct mtk_sensor_mode_config_info));

	p_info->current_scenario_id = ctx->cur_mode->id;
	if (!get_sensor_mode_info(ctx, ctx->cur_mode->id, p_info->seamless_scenario_infos))
		++mode_cnt;

	for (i = 0; i < SENSOR_SCENARIO_ID_MAX; i++) {
		if (ctx->seamless_scenarios[i] == SENSOR_SCENARIO_ID_NONE)
			break;
		else if (ctx->seamless_scenarios[i] == ctx->cur_mode->id)
			continue;
		else if (!get_sensor_mode_info(ctx, ctx->seamless_scenarios[i],
					       p_info->seamless_scenario_infos + mode_cnt))
			++mode_cnt;
	}

	p_info->count = mode_cnt;

	return ret;
}

static int g_cmd_sensor_in_reset(struct adaptor_ctx *ctx, void *arg)
{
	int ret = 0;
	bool *in_reset = NULL;

	/* error handling (unexpected case) */
	if (unlikely(arg == NULL)) {
		ret = -ENOIOCTLCMD;
		adaptor_logi(ctx,
			"ERROR: V4L2_CMD_GET_SENSOR_MODE_CONFIG_INFO, idx:%d, input arg is nullptr, return:%d\n",
			ctx->idx, ret);
		return ret;
	}

	in_reset = arg;

	*in_reset = !!(ctx->is_sensor_reset_stream_off);

	return ret;
}

/* SET */
static int s_cmd_fsync_sync_frame_start_end(struct adaptor_ctx *ctx, void *arg)
{
	int *p_flag = NULL;
	int ret = 0;

	/* error handling (unexpected case) */
	if (unlikely(ctx == NULL))
		return -EINVAL;
	if (unlikely(arg == NULL)) {
		ret = -EINVAL;
		adaptor_logi(ctx,
			"ERROR: V4L2_CMD_FSYNC_SYNC_FRAME_START_END, idx:%d, input arg is nullptr, return:%d\n",
			ctx->idx, ret);
		return ret;
	}

	/* casting arg to int-pointer for using */
	p_flag = arg;

	adaptor_logd(ctx,
		"V4L2_CMD_FSYNC_SYNC_FRAME_START_END, idx:%d, flag:%d\n",
		ctx->idx, *p_flag);

	notify_fsync_mgr_sync_frame(ctx, *p_flag);

	return ret;
}


static int s_cmd_tsrec_notify_vsync(struct adaptor_ctx *ctx, void *arg)
{
	struct mtk_cam_seninf_tsrec_vsync_info *buf = NULL;
	int ret = 0;

	/* error handling (unexpected case) */
	if (unlikely(ctx == NULL))
		return -EINVAL;
	if (unlikely(arg == NULL)) {
		ret = -EINVAL;
		adaptor_logi(ctx,
			"ERROR: V4L2_CMD_TSREC_NOTIFY_VSYNC, idx:%d, input arg is nullptr, return:%d\n",
			ctx->idx, ret);
		return ret;
	}

	buf = (struct mtk_cam_seninf_tsrec_vsync_info *)arg;
	/* TODO: check source/caller is valid or not by checking seninf idx */

	memcpy(&ctx->vsync_info, buf, sizeof(ctx->vsync_info));

	adaptor_logd(ctx,
		"V4L2_CMD_TSREC_NOTIFY_VSYNC, idx:%d, vsync_info(tsrec_no:%u, seninf_idx:%u, sys_ts%llu(ns), tsrec_ts:%llu(us))\n",
		ctx->idx,
		ctx->vsync_info.tsrec_no,
		ctx->vsync_info.seninf_idx,
		ctx->vsync_info.irq_sys_time_ns,
		ctx->vsync_info.irq_tsrec_ts_us);


	/* tsrec notify vsync, call all APIs that needed this info */
	// TODO: add functions here !
	notify_fsync_mgr_vsync_by_tsrec(ctx);

	return ret;
}


static int s_cmd_tsrec_notify_sensor_hw_pre_latch(
	struct adaptor_ctx *ctx, void *arg)
{
	struct mtk_cam_seninf_tsrec_vsync_info *buf = NULL;
	int ret = 0;

	/* error handling (unexpected case) */
	if (unlikely(ctx == NULL))
		return -EINVAL;
	if (unlikely(arg == NULL)) {
		ret = -EINVAL;
		adaptor_logi(ctx,
			"ERROR: V4L2_CMD_TSREC_NOTIFY_SENSOR_HW_PRE_LATCH, idx:%d, input arg is nullptr, return:%d\n",
			ctx->idx, ret);
		return ret;
	}

	buf = (struct mtk_cam_seninf_tsrec_vsync_info *)arg;

	adaptor_logd(ctx,
		"V4L2_CMD_TSREC_NOTIFY_SENSOR_HW_PRE_LATCH, idx:%d, vsync_info(tsrec_no:%u, seninf_idx:%u, sys_ts%llu(ns), tsrec_ts:%llu(us))\n",
		ctx->idx,
		buf->tsrec_no,
		buf->seninf_idx,
		buf->irq_sys_time_ns,
		buf->irq_tsrec_ts_us);


	/* tsrec notify sensor hw pre-latch, call all APIs that needed this info */
	// TODO: add functions here !
	notify_fsync_mgr_sensor_hw_pre_latch_by_tsrec(ctx);

	return ret;
}


static int s_cmd_tsrec_send_timestamp_info(struct adaptor_ctx *ctx, void *arg)
{
	struct mtk_cam_seninf_tsrec_timestamp_info *buf = NULL;
	int ret = 0;

	/* error handling (unexpected case) */
	if (unlikely(ctx == NULL))
		return -EINVAL;
	if (unlikely(arg == NULL)) {
		ret = -EINVAL;
		adaptor_logi(ctx,
			"ERROR: V4L2_CMD_TSREC_SEND_TIMESTAMP_INFO, idx:%d, input arg is nullptr, return:%d\n",
			ctx->idx, ret);
		return ret;
	}

	buf = (struct mtk_cam_seninf_tsrec_timestamp_info *)arg;
	/* TODO: check source/caller is valid or not by checking seninf idx */

	memcpy(&ctx->ts_info, buf, sizeof(ctx->ts_info));

	adaptor_logd(ctx,
		"V4L2_CMD_TSREC_SEND_TIMESTAMP_INFO, idx:%d, ts_info(tsrec_no:%u, seninf_idx:%u, tick_factor:%u, sys_ts:%llu(ns), tsrec_ts:%llu(us), tick:%llu, ts(0:(%llu/%llu/%llu/%llu), 1:(%llu/%llu/%llu/%llu), 2:(%llu/%llu/%llu/%llu))\n",
		ctx->idx,
		ctx->ts_info.tsrec_no,
		ctx->ts_info.seninf_idx,
		ctx->ts_info.tick_factor,
		ctx->ts_info.irq_sys_time_ns,
		ctx->ts_info.irq_tsrec_ts_us,
		ctx->ts_info.tsrec_curr_tick,
		ctx->ts_info.exp_recs[0].ts_us[0],
		ctx->ts_info.exp_recs[0].ts_us[1],
		ctx->ts_info.exp_recs[0].ts_us[2],
		ctx->ts_info.exp_recs[0].ts_us[3],
		ctx->ts_info.exp_recs[1].ts_us[0],
		ctx->ts_info.exp_recs[1].ts_us[1],
		ctx->ts_info.exp_recs[1].ts_us[2],
		ctx->ts_info.exp_recs[1].ts_us[3],
		ctx->ts_info.exp_recs[2].ts_us[0],
		ctx->ts_info.exp_recs[2].ts_us[1],
		ctx->ts_info.exp_recs[2].ts_us[2],
		ctx->ts_info.exp_recs[2].ts_us[3]);


	/* tsrec send timestamp info, call all APIs that needed this info */
	// TODO: add functions here !
	notify_fsync_mgr_receive_tsrec_timestamp_info(ctx, &ctx->ts_info);

	return ret;
}


/*---------------------------------------------------------------------------*/
// adaptor command framework/entry
/*---------------------------------------------------------------------------*/

struct command_entry {
	unsigned int cmd;
	int (*func)(struct adaptor_ctx *ctx, void *arg);
};

static const struct command_entry command_list[] = {
	/* GET */
	{V4L2_CMD_GET_SENSOR_MODE_CONFIG_INFO, g_cmd_sensor_mode_config_info},
	{V4L2_CMD_SENSOR_IN_RESET, g_cmd_sensor_in_reset},

	/* SET */
	{V4L2_CMD_FSYNC_SYNC_FRAME_START_END, s_cmd_fsync_sync_frame_start_end},
	{V4L2_CMD_TSREC_NOTIFY_VSYNC, s_cmd_tsrec_notify_vsync},
	{V4L2_CMD_TSREC_NOTIFY_SENSOR_HW_PRE_LATCH,
		s_cmd_tsrec_notify_sensor_hw_pre_latch},
	{V4L2_CMD_TSREC_SEND_TIMESTAMP_INFO, s_cmd_tsrec_send_timestamp_info},
};

long adaptor_command(struct v4l2_subdev *sd, unsigned int cmd, void *arg)
{
	struct adaptor_ctx *ctx = NULL;
	int i, ret = -ENOIOCTLCMD;

	/* error handling (unexpected case) */
	if (unlikely(sd == NULL)) {
		ret = -ENOIOCTLCMD;
		pr_info(
			"[%s] ERROR: get nullptr of v4l2_subdev (sd), return:%d   [cmd id:%#x]\n",
			__func__, ret, cmd);
		return ret;
	}

	ctx = sd_to_ctx(sd);

	adaptor_logd(ctx,
		"dispatch command request, idx:%d, cmd id:%#x\n",
		ctx->idx, cmd);

	/* dispatch command request */
	for (i = 0; i < ARRAY_SIZE(command_list); i++) {
		if (command_list[i].cmd == cmd) {
			ret = command_list[i].func(ctx, arg);
			break;
		}
	}

	return ret;
}

