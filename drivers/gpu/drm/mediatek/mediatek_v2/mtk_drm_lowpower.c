// SPDX-License-Identifier: GPL-2.0
/*
* Copyright (c) 2021 MediaTek Inc.
*/

#include <linux/kthread.h>
#include <linux/wait.h>
#include <linux/mutex.h>
#include <linux/cpufreq.h>
#include <linux/pm_qos.h>
#include <linux/sched.h>
#include <linux/sched/clock.h>
#include <uapi/linux/sched/types.h>
#include <linux/delay.h>
#include <uapi/drm/mediatek_drm.h>
#include <drm/drm_vblank.h>
#include "mtk_drm_lowpower.h"
#include "mtk_drm_drv.h"
#include "mtk_drm_ddp.h"
#include "mtk_drm_ddp_comp.h"
#include "mtk_drm_mmp.h"
#include "mtk_drm_trace.h"

#define MAX_ENTER_IDLE_RSZ_RATIO 300

static void mtk_drm_idlemgr_get_private_data(struct drm_crtc *crtc,
		struct mtk_idle_private_data *data)
{
	struct mtk_drm_private *priv = NULL;

	if (data == NULL || crtc == NULL)
		return;

	priv = crtc->dev->dev_private;
	if (!mtk_drm_helper_get_opt(priv->helper_opt,
				MTK_DRM_OPT_IDLEMGR_ASYNC)) {
		data->cpu_id = -1;
		data->cpu_freq = 0;
		data->vblank_async = false;
		return;
	}

	switch (priv->data->mmsys_id) {
	case MMSYS_MT6985:
		data->cpu_id = 7;
		data->cpu_freq = 1000000; // >=1GHZ
		data->vblank_async = false;
		break;
	default:
		data->cpu_id = -1;
		data->cpu_freq = 0;
		data->vblank_async = false;
		break;
	}
}

static bool mtk_drm_adjust_cpu_freq(struct drm_crtc *crtc, bool bind, struct freq_qos_request *req)
{
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	struct mtk_drm_idlemgr *idlemgr = mtk_crtc->idlemgr;
	struct mtk_drm_idlemgr_context *idlemgr_ctx = idlemgr->idlemgr_ctx;
	struct cpufreq_policy *policy = NULL;
	int ret = 0;

	if (req == NULL || idlemgr_ctx->priv.cpu_id < 0 ||
		idlemgr_ctx->priv.cpu_freq == 0)
		return false;

	if (bind == true) {
		memset(req, 0, sizeof(struct freq_qos_request));
		policy = cpufreq_cpu_get(idlemgr_ctx->priv.cpu_id);
		if (policy != NULL) {
			ret = freq_qos_add_request(&policy->constraints, req,
						FREQ_QOS_MIN, idlemgr_ctx->priv.cpu_freq);
			cpufreq_cpu_put(policy);

			if (ret < 0) {
				DDPMSG("%s, failed to enhance cpu freq, ret:%d\n",
					__func__, ret);
				return false;
			}
		} else {
			DDPMSG("%s, failed to get cpu policy\n", __func__);
			return false;
		}
	}

	if (bind == false) {
		ret = freq_qos_remove_request(req);
		if (ret < 0)
			DDPMSG("%s, failed to rollback cpu freq, ret:%d\n",
				__func__, ret);
	}

	return true;
}

static void mtk_drm_idlemgr_enable_crtc(struct drm_crtc *crtc);
static void mtk_drm_idlemgr_disable_crtc(struct drm_crtc *crtc);

static void mtk_drm_vdo_mode_enter_idle(struct drm_crtc *crtc)
{
	struct mtk_drm_private *priv = crtc->dev->dev_private;
	struct mtk_crtc_state *state = to_mtk_crtc_state(crtc->state);
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	int i, j;
	struct cmdq_pkt *handle;
	struct cmdq_client *client = mtk_crtc->gce_obj.client[CLIENT_CFG];
	struct mtk_ddp_comp *comp;

	mtk_crtc_pkt_create(&handle, crtc, client);

	if (mtk_drm_helper_get_opt(priv->helper_opt,
				   MTK_DRM_OPT_IDLEMGR_BY_REPAINT) &&
	    atomic_read(&state->plane_enabled_num) > 1) {
		atomic_set(&priv->idle_need_repaint, 1);
		drm_trigger_repaint(DRM_REPAINT_FOR_IDLE, crtc->dev);
	}

	if (mtk_drm_helper_get_opt(priv->helper_opt,
				   MTK_DRM_OPT_IDLEMGR_DISABLE_ROUTINE_IRQ)) {
		mtk_disp_mutex_inten_disable_cmdq(mtk_crtc->mutex[0], handle);
		for_each_comp_in_cur_crtc_path(comp, mtk_crtc, i, j)
			mtk_ddp_comp_io_cmd(comp, handle, IRQ_LEVEL_IDLE, NULL);
	}

	comp = mtk_ddp_comp_request_output(mtk_crtc);
	if (comp) {
		int en = 0;
		mtk_ddp_comp_io_cmd(comp, handle, DSI_VFP_IDLE_MODE, NULL);
		mtk_ddp_comp_io_cmd(comp, handle, DSI_LFR_SET, &en);
	}

	cmdq_pkt_flush(handle);
	cmdq_pkt_destroy(handle);
}

static void mtk_drm_cmd_mode_enter_idle(struct drm_crtc *crtc)
{
	bool adjusted = false;
	struct freq_qos_request	req;

	adjusted = mtk_drm_adjust_cpu_freq(crtc, true, &req);

	mtk_drm_idlemgr_disable_crtc(crtc);
	lcm_fps_ctx_reset(crtc);

	if (adjusted == true)
		mtk_drm_adjust_cpu_freq(crtc, false, &req);
}

static void mtk_drm_vdo_mode_leave_idle(struct drm_crtc *crtc)
{
	struct mtk_drm_private *priv = crtc->dev->dev_private;
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	int i, j;
	struct cmdq_pkt *handle;
	struct cmdq_client *client = mtk_crtc->gce_obj.client[CLIENT_CFG];
	struct mtk_ddp_comp *comp;

	mtk_crtc_pkt_create(&handle, crtc, client);

	if (mtk_drm_helper_get_opt(priv->helper_opt,
				   MTK_DRM_OPT_IDLEMGR_DISABLE_ROUTINE_IRQ)) {
		mtk_disp_mutex_inten_enable_cmdq(mtk_crtc->mutex[0], handle);
		for_each_comp_in_cur_crtc_path(comp, mtk_crtc, i, j)
			mtk_ddp_comp_io_cmd(comp, handle, IRQ_LEVEL_NORMAL, NULL);
	}

	comp = mtk_ddp_comp_request_output(mtk_crtc);
	if (comp) {
		int en = 1;
		mtk_ddp_comp_io_cmd(comp, handle, DSI_VFP_DEFAULT_MODE, NULL);
		mtk_ddp_comp_io_cmd(comp, handle, DSI_LFR_SET, &en);
	}

	cmdq_pkt_flush(handle);
	cmdq_pkt_destroy(handle);
}

static void mtk_drm_cmd_mode_leave_idle(struct drm_crtc *crtc)
{
	bool adjusted = false;
	struct freq_qos_request	req;

	adjusted = mtk_drm_adjust_cpu_freq(crtc, true, &req);

	mtk_drm_idlemgr_enable_crtc(crtc);
	lcm_fps_ctx_reset(crtc);

	if (adjusted == true)
		mtk_drm_adjust_cpu_freq(crtc, false, &req);
}

static void mtk_drm_idlemgr_enter_idle_nolock(struct drm_crtc *crtc)
{
	struct mtk_ddp_comp *output_comp;
	int index = drm_crtc_index(crtc);
	unsigned int idle_interval;
	bool mode;
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);

	output_comp = mtk_ddp_comp_request_output(mtk_crtc);

	if (!output_comp)
		return;

	mode = mtk_dsi_is_cmd_mode(output_comp);
	idle_interval = mtk_drm_get_idle_check_interval(crtc);
	CRTC_MMP_EVENT_START(index, enter_idle, mode, idle_interval);

	if (mode)
		mtk_drm_cmd_mode_enter_idle(crtc);
	else
		mtk_drm_vdo_mode_enter_idle(crtc);

	CRTC_MMP_EVENT_END(index, enter_idle, mode, idle_interval);
}

static void mtk_drm_idlemgr_leave_idle_nolock(struct drm_crtc *crtc)
{
	struct mtk_ddp_comp *output_comp;
	int index = drm_crtc_index(crtc);
	bool mode;
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);

	output_comp = mtk_ddp_comp_request_output(mtk_crtc);

	if (!output_comp)
		return;

	mode = mtk_dsi_is_cmd_mode(output_comp);
	CRTC_MMP_EVENT_START(index, leave_idle, mode, 0);
	drm_trace_tag_start("Kick idle");


	if (mode)
		mtk_drm_cmd_mode_leave_idle(crtc);
	else
		mtk_drm_vdo_mode_leave_idle(crtc);

	CRTC_MMP_EVENT_END(index, leave_idle, mode, 0);
	drm_trace_tag_end("Kick idle");
}

bool mtk_drm_is_idle(struct drm_crtc *crtc)
{
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	struct mtk_drm_idlemgr *idlemgr = mtk_crtc->idlemgr;

	if (!idlemgr)
		return false;

	return idlemgr->idlemgr_ctx->is_idle;
}

bool mtk_drm_idlemgr_get_async_status(struct drm_crtc *crtc)
{
	struct mtk_drm_idlemgr *idlemgr;
	struct mtk_drm_private *priv = NULL;
	struct mtk_drm_crtc *mtk_crtc = NULL;

	if (crtc == NULL)
		return false;

	priv = crtc->dev->dev_private;
	if (priv == NULL ||
		!mtk_drm_helper_get_opt(priv->helper_opt,
			MTK_DRM_OPT_IDLEMGR_ASYNC))
		return false;

	mtk_crtc = to_mtk_crtc(crtc);
	if (mtk_crtc && mtk_crtc->idlemgr)
		idlemgr = mtk_crtc->idlemgr;
	else
		return false;

	if (atomic_read(&idlemgr->async_enabled) != 0)
		return true;

	return false;
}

void mtk_drm_idlemgr_async_get(struct drm_crtc *crtc, char *master)
{
	struct mtk_drm_idlemgr *idlemgr = NULL;
	struct mtk_drm_crtc *mtk_crtc = NULL;

	if (mtk_drm_idlemgr_get_async_status(crtc) == false)
		return;

	mtk_crtc = to_mtk_crtc(crtc);
	idlemgr = mtk_crtc->idlemgr;
	atomic_inc(&idlemgr->async_ref);
	DDPINFO("%s, active:%d count:%d from:%s\n", __func__,
		atomic_read(&idlemgr->async_enabled),
		atomic_read(&idlemgr->async_ref),
		master == NULL ? "unknown" : master);
}

// gce irq handler will do async put to let idle task go
void mtk_drm_idlemgr_async_put(struct drm_crtc *crtc, char *master)
{
	struct mtk_drm_idlemgr *idlemgr = NULL;
	struct mtk_drm_crtc *mtk_crtc = NULL;

	if (mtk_drm_idlemgr_get_async_status(crtc) == false)
		return;

	mtk_crtc = to_mtk_crtc(crtc);
	idlemgr = mtk_crtc->idlemgr;
	if (atomic_dec_return(&idlemgr->async_ref) == 0)
		wake_up_interruptible(&idlemgr->async_event_wq);

	DDPINFO("%s, active:%d count:%d from:%s\n", __func__,
		atomic_read(&idlemgr->async_enabled),
		atomic_read(&idlemgr->async_ref),
		master == NULL ? "unknown" : master);
}

// cmdq pkt is wait and destroyed in the async handler thread
void mtk_drm_idlemgr_async_complete(struct drm_crtc *crtc, char *master,
	struct mtk_drm_async_cb_data *cb_data)
{
	struct mtk_drm_async_cb *cb = NULL;
	struct mtk_drm_idlemgr *idlemgr = NULL;
	struct mtk_drm_crtc *mtk_crtc = NULL;
	unsigned long flags = 0;

	if (mtk_drm_idlemgr_get_async_status(crtc) == false)
		return;

	mtk_crtc = to_mtk_crtc(crtc);
	idlemgr = mtk_crtc->idlemgr;
	if (cb_data != NULL) {
		cb = kmalloc(sizeof(struct mtk_drm_async_cb), GFP_KERNEL);
		if (cb == NULL) {
			DDPPR_ERR("%s, failed to allocate cb node\n", __func__);
			return;
		}

		cb->data = cb_data;
		spin_lock_irqsave(&idlemgr->async_lock, flags);
		list_add_tail(&cb->link, &idlemgr->async_cb_list);
		idlemgr->async_cb_count++;
		spin_unlock_irqrestore(&idlemgr->async_lock, flags);
		wake_up_interruptible(&idlemgr->async_handler_wq);
	}
}

void mtk_drm_idle_async_cb(struct cmdq_cb_data data)
{
	struct mtk_drm_async_cb_data *cb_data = data.data;
	struct drm_crtc *crtc = NULL;

	if (cb_data == NULL) {
		DDPMSG("%s: invalid cb data\n", __func__);
		return;
	}

	crtc = cb_data->crtc;
	if (cb_data->master != NULL)
		mtk_drm_idlemgr_async_put(crtc, cb_data->master);
	else
		mtk_drm_idlemgr_async_put(crtc, "unknown");
}

void mtk_drm_idle_async_flush(struct drm_crtc *crtc,
	char *master, struct cmdq_pkt *cmdq_handle)
{
	struct mtk_drm_async_cb_data *cb_data = NULL;
	bool async = mtk_drm_idlemgr_get_async_status(crtc);
	int len = 0;

	if (cmdq_handle == NULL)
		return;

	/* DDPINFO("%s, master:%s, flush cmdq pkt:0x%lx, async:%d\n", __func__,
	 *	master == NULL ? "unknown" : master,
	 *	(unsigned long)cmdq_handle, async);
	 */

	if (async == true)
		cb_data = kmalloc(sizeof(struct mtk_drm_async_cb_data), GFP_KERNEL);

	if (cb_data == NULL) {
		cmdq_pkt_flush(cmdq_handle);
		cmdq_pkt_destroy(cmdq_handle);
	} else {
		cb_data->crtc = crtc;
		cb_data->handle = cmdq_handle;
		cb_data->master = kmalloc(50, GFP_KERNEL);
		if (master != NULL)
			len = snprintf(cb_data->master, 50, master);
		mtk_drm_idlemgr_async_get(crtc, master);
		cmdq_pkt_flush_async(cmdq_handle, mtk_drm_idle_async_cb, cb_data);
		mtk_drm_idlemgr_async_complete(crtc, master, cb_data);
	}
}

static void mtk_drm_idle_async_wait(struct drm_crtc *crtc,
	unsigned int time, char *name)
{
	struct mtk_drm_idlemgr *idlemgr = NULL;
	struct mtk_drm_crtc *mtk_crtc = NULL;
	int ret = 0;

	if (mtk_drm_idlemgr_get_async_status(crtc) == false)
		return;

	mtk_crtc = to_mtk_crtc(crtc);
	idlemgr = mtk_crtc->idlemgr;

	//avoid of cpu schedule out by waiting last gce job done
	if (time > 0)
		udelay(time);

	ret = wait_event_interruptible(idlemgr->async_event_wq,
					 !atomic_read(&idlemgr->async_ref));
}

void mtk_drm_idlemgr_kick_async(struct drm_crtc *crtc)
{
	struct mtk_drm_crtc *mtk_crtc = NULL;
	struct mtk_drm_idlemgr *idlemgr;

	if (crtc)
		mtk_crtc = to_mtk_crtc(crtc);

	if (mtk_crtc && mtk_crtc->idlemgr)
		idlemgr = mtk_crtc->idlemgr;
	else
		return;

	atomic_set(&idlemgr->kick_task_active, 1);
	wake_up_interruptible(&idlemgr->kick_wq);
}

void mtk_drm_idlemgr_kick(const char *source, struct drm_crtc *crtc,
			  int need_lock)
{
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	struct mtk_drm_idlemgr *idlemgr;
	struct mtk_drm_idlemgr_context *idlemgr_ctx;
	struct mtk_drm_private *priv = crtc->dev->dev_private;

	if (!mtk_drm_helper_get_opt(priv->helper_opt, MTK_DRM_OPT_IDLE_MGR))
		return;

	if (!mtk_crtc->idlemgr)
		return;
	idlemgr = mtk_crtc->idlemgr;
	idlemgr_ctx = idlemgr->idlemgr_ctx;

	/* get lock to protect idlemgr_last_kick_time and is_idle */
	if (need_lock)
		DDP_MUTEX_LOCK(&mtk_crtc->lock, __func__, __LINE__);

	if (idlemgr_ctx->is_idle) {
		DDPINFO("[LP] kick idle from [%s]\n", source);
		if (mtk_crtc->esd_ctx)
			atomic_set(&mtk_crtc->esd_ctx->target_time, 0);
		mtk_drm_idlemgr_leave_idle_nolock(crtc);
		idlemgr_ctx->is_idle = 0;

		/* wake up idlemgr process to monitor next idle state */
		wake_up_interruptible(&idlemgr->idlemgr_wq);
	}

	/* update kick timestamp */
	idlemgr_ctx->idlemgr_last_kick_time = sched_clock();

	if (need_lock)
		DDP_MUTEX_UNLOCK(&mtk_crtc->lock, __func__, __LINE__);
}

unsigned int mtk_drm_set_idlemgr(struct drm_crtc *crtc, unsigned int flag,
				 bool need_lock)
{
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	struct mtk_drm_idlemgr *idlemgr = mtk_crtc->idlemgr;
	unsigned int old_flag;

	if (!idlemgr)
		return 0;

	old_flag = atomic_read(&idlemgr->idlemgr_task_active);

	if (flag) {
		DDPINFO("[LP] enable idlemgr\n");
		atomic_set(&idlemgr->idlemgr_task_active, 1);
		wake_up_interruptible(&idlemgr->idlemgr_wq);
	} else {
		DDPINFO("[LP] disable idlemgr\n");
		atomic_set(&idlemgr->idlemgr_task_active, 0);
		mtk_drm_idlemgr_kick(__func__, crtc, need_lock);
	}

	return old_flag;
}

unsigned long long
mtk_drm_set_idle_check_interval(struct drm_crtc *crtc,
				unsigned long long new_interval)
{
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	unsigned long long old_interval = 0;

	if (!(mtk_crtc && mtk_crtc->idlemgr && mtk_crtc->idlemgr->idlemgr_ctx))
		return 0;

	old_interval = mtk_crtc->idlemgr->idlemgr_ctx->idle_check_interval;
	mtk_crtc->idlemgr->idlemgr_ctx->idle_check_interval = new_interval;

	return old_interval;
}

unsigned long long
mtk_drm_get_idle_check_interval(struct drm_crtc *crtc)
{
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);

	if (!(mtk_crtc && mtk_crtc->idlemgr && mtk_crtc->idlemgr->idlemgr_ctx))
		return 0;

	return mtk_crtc->idlemgr->idlemgr_ctx->idle_check_interval;
}

static int mtk_drm_idlemgr_get_rsz_ratio(struct mtk_crtc_state *state)
{
	int src_w = state->rsz_src_roi.width;
	int src_h = state->rsz_src_roi.height;
	int dst_w = state->rsz_dst_roi.width;
	int dst_h = state->rsz_dst_roi.height;
	int ratio_w, ratio_h;

	if (src_w == 0 || src_h == 0)
		return 100;

	ratio_w = dst_w * 100 / src_w;
	ratio_h = dst_h * 100 / src_h;

	return ((ratio_w > ratio_h) ? ratio_w : ratio_h);
}

static bool is_yuv(uint32_t format)
{
	switch (format) {
	case DRM_FORMAT_YUV420:
	case DRM_FORMAT_YVU420:
	case DRM_FORMAT_NV12:
	case DRM_FORMAT_NV21:
	case DRM_FORMAT_YUYV:
	case DRM_FORMAT_YVYU:
	case DRM_FORMAT_UYVY:
	case DRM_FORMAT_VYUY:
		return true;
	default:
		break;
	}

	return false;
}

static bool mtk_planes_is_yuv_fmt(struct drm_crtc *crtc)
{
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	int i;

	for (i = 0; i < mtk_crtc->layer_nr; i++) {
		struct drm_plane *plane = &mtk_crtc->planes[i].base;
		struct mtk_plane_state *plane_state =
			to_mtk_plane_state(plane->state);
		struct mtk_plane_pending_state *pending = &plane_state->pending;
		unsigned int fmt = pending->format;

		if (pending->enable && is_yuv(fmt))
			return true;

		if (plane_state->comp_state.layer_caps & MTK_DISP_SRC_YUV_LAYER)
			return true;
	}

	return false;
}

static int mtk_drm_async_handler_thread(void *data)
{
	struct drm_crtc *crtc = (struct drm_crtc *)data;
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	struct mtk_drm_idlemgr *idlemgr = mtk_crtc->idlemgr;
	struct mtk_drm_async_cb_data *cb_data = NULL;
	struct mtk_drm_async_cb *cb = NULL;
	unsigned long flags = 0;
	int ret = 0;

	while (!kthread_should_stop()) {
		ret = wait_event_interruptible(idlemgr->async_handler_wq,
			idlemgr->async_cb_count > 0 || kthread_should_stop());

		spin_lock_irqsave(&idlemgr->async_lock, flags);
		if (list_empty(&idlemgr->async_cb_list)) {
			spin_unlock_irqrestore(&idlemgr->async_lock, flags);
			DDPPR_ERR("%s: async list is empty\n", __func__);
			break;
		}
		cb = list_first_entry(&idlemgr->async_cb_list, struct mtk_drm_async_cb, link);
		list_del(&cb->link);
		idlemgr->async_cb_count--;
		spin_unlock_irqrestore(&idlemgr->async_lock, flags);

		if (cb->data != NULL) {
			cb_data = cb->data;
			DDPINFO("%s,async handling of master:%s\n", __func__,
				cb_data->master == NULL ? "unknown" : cb_data->master);
			cmdq_pkt_wait_complete(cb_data->handle);
			cmdq_pkt_destroy(cb_data->handle);

			if (cb_data->master != NULL)
				kfree(cb_data->master);
			cb_data->master = NULL;

			kfree(cb_data);
			cb_data = NULL;

			kfree(cb);
			cb = NULL;
		} else {
			DDPMSG("%s, invalid async callback data\n", __func__);
		}
	}

	return 0;
}

static int mtk_drm_async_vblank_thread(void *data)
{
	struct sched_param param = {.sched_priority = 87 };
	struct drm_crtc *crtc = (struct drm_crtc *)data;
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	struct mtk_drm_idlemgr *idlemgr = mtk_crtc->idlemgr;
	int ret = 0;

	sched_setscheduler(current, SCHED_RR, &param);

	while (!kthread_should_stop()) {
		ret = wait_event_interruptible(
			idlemgr->async_vblank_wq,
			atomic_read(&idlemgr->async_vblank_active));

		atomic_set(&idlemgr->async_vblank_active, 0);
		drm_crtc_vblank_off(crtc);
		mtk_crtc_vblank_irq(&mtk_crtc->base);
		mtk_drm_idlemgr_async_put(crtc, "vblank_off");
	}

	return 0;
}

static int mtk_drm_async_kick_idlemgr_thread(void *data)
{
	struct sched_param param = {.sched_priority = 87 };
	struct drm_crtc *crtc = (struct drm_crtc *)data;
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	struct mtk_drm_idlemgr *idlemgr = mtk_crtc->idlemgr;
	int ret = 0;

	sched_setscheduler(current, SCHED_RR, &param);

	while (!kthread_should_stop()) {
		ret = wait_event_interruptible(
			idlemgr->kick_wq,
			atomic_read(&idlemgr->kick_task_active));

		atomic_set(&idlemgr->kick_task_active, 0);
		mtk_drm_idlemgr_kick(__func__, crtc, true);
	}

	return 0;
}

static int mtk_drm_idlemgr_monitor_thread(void *data)
{
	int ret = 0;
	unsigned long long t_idle;
	struct drm_crtc *crtc = (struct drm_crtc *)data;
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	struct mtk_drm_idlemgr *idlemgr = mtk_crtc->idlemgr;
	struct mtk_drm_idlemgr_context *idlemgr_ctx = idlemgr->idlemgr_ctx;
	struct mtk_drm_private *priv = crtc->dev->dev_private;
	struct mtk_crtc_state *mtk_state = NULL;
	struct drm_vblank_crtc *vblank = NULL;
	int crtc_id = drm_crtc_index(crtc);

	msleep(16000);
	while (1) {
		ret = wait_event_interruptible(
			idlemgr->idlemgr_wq,
			atomic_read(&idlemgr->idlemgr_task_active));

		msleep_interruptible(idlemgr_ctx->idle_check_interval);

		DDP_MUTEX_LOCK(&mtk_crtc->lock, __func__, __LINE__);

		if (!mtk_crtc->enabled) {
			DDP_MUTEX_UNLOCK(&mtk_crtc->lock, __func__, __LINE__);
			mtk_crtc_wait_status(crtc, 1, MAX_SCHEDULE_TIMEOUT);
			continue;
		}

		if (mtk_crtc_is_frame_trigger_mode(crtc) &&
				atomic_read(&priv->crtc_rel_present[crtc_id]) <
				atomic_read(&priv->crtc_present[crtc_id])) {
			DDP_MUTEX_UNLOCK(&mtk_crtc->lock, __func__, __LINE__);
			continue;
		}

		if (crtc->state) {
			mtk_state = to_mtk_crtc_state(crtc->state);
			if (mtk_state->prop_val[CRTC_PROP_DOZE_ACTIVE]) {
				DDP_MUTEX_UNLOCK(&mtk_crtc->lock, __func__,
						__LINE__);
				continue;
			}
			/* do not enter VDO idle when rsz ratio >= 2.5;
			 * And When layer fmt is YUV in VP scenario, it
			 * will flicker into idle repaint, so let it not
			 * into idle repaint as workaround.
			 */
			if (mtk_crtc_is_frame_trigger_mode(crtc) == 0 &&
				((mtk_drm_idlemgr_get_rsz_ratio(mtk_state) >=
				MAX_ENTER_IDLE_RSZ_RATIO) ||
				mtk_planes_is_yuv_fmt(crtc))) {
				DDP_MUTEX_UNLOCK(&mtk_crtc->lock, __func__,
						__LINE__);
				continue;
			}
		}

		if (idlemgr_ctx->is_idle
			|| mtk_crtc_is_dc_mode(crtc)
			|| mtk_crtc->sec_on
			|| !priv->already_first_config) {
			DDP_MUTEX_UNLOCK(&mtk_crtc->lock, __func__, __LINE__);
			continue;
		}

		t_idle = local_clock() - idlemgr_ctx->idlemgr_last_kick_time;
		if (t_idle < idlemgr_ctx->idle_check_interval * 1000 * 1000) {
			/* kicked in idle_check_interval msec, it's not idle */
			DDP_MUTEX_UNLOCK(&mtk_crtc->lock, __func__, __LINE__);
			continue;
		}
		/* double check if dynamic switch on/off */
		if (atomic_read(&idlemgr->idlemgr_task_active)) {
			crtc_id = drm_crtc_index(crtc);
			vblank = &crtc->dev->vblank[crtc_id];

			/* enter idle state */
			if (!vblank || atomic_read(&vblank->refcount) == 0) {
				DDPINFO("[LP] enter idle\n");
				mtk_drm_idlemgr_enter_idle_nolock(crtc);
				idlemgr_ctx->is_idle = 1;
			} else {
				idlemgr_ctx->idlemgr_last_kick_time =
					sched_clock();
			}
		}

		DDP_MUTEX_UNLOCK(&mtk_crtc->lock, __func__, __LINE__);

		wait_event_interruptible(idlemgr->idlemgr_wq,
					 !idlemgr_ctx->is_idle);

		if (kthread_should_stop())
			break;
	}

	return 0;
}

int mtk_drm_idlemgr_init(struct drm_crtc *crtc, int index)
{
#define LEN 50
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	struct mtk_drm_idlemgr *idlemgr = NULL;
	struct mtk_drm_idlemgr_context *idlemgr_ctx = NULL;
	struct mtk_drm_private *priv =
				mtk_crtc->base.dev->dev_private;
	char name[LEN] = {0};

	idlemgr = kzalloc(sizeof(*idlemgr), GFP_KERNEL);
	idlemgr_ctx = kzalloc(sizeof(*idlemgr_ctx), GFP_KERNEL);

	if (!idlemgr || !idlemgr_ctx) {
		DDPPR_ERR("idlemgr or idlemgr_ctx allocate fail\n");
		kfree(idlemgr);
		kfree(idlemgr_ctx);
		return -ENOMEM;
	}

	idlemgr->idlemgr_ctx = idlemgr_ctx;
	mtk_crtc->idlemgr = idlemgr;

	idlemgr_ctx->session_mode_before_enter_idle = MTK_DRM_SESSION_INVALID;
	idlemgr_ctx->is_idle = 0;
	idlemgr_ctx->enterulps = 0;
	idlemgr_ctx->idlemgr_last_kick_time = ~(0ULL);
	idlemgr_ctx->cur_lp_cust_mode = 0;
	idlemgr_ctx->idle_check_interval = 50;

	mtk_drm_idlemgr_get_private_data(crtc, &idlemgr_ctx->priv);

	snprintf(name, LEN, "mtk_drm_disp_idlemgr-%d", index);
	if (idlemgr_ctx->priv.cpu_id > 0)
		idlemgr->idlemgr_task =
			kthread_create_on_cpu(mtk_drm_idlemgr_monitor_thread,
				crtc, idlemgr_ctx->priv.cpu_id, name);
	else
		idlemgr->idlemgr_task =
			kthread_create(mtk_drm_idlemgr_monitor_thread, crtc, name);
	init_waitqueue_head(&idlemgr->idlemgr_wq);
	atomic_set(&idlemgr->idlemgr_task_active, 1);

	wake_up_process(idlemgr->idlemgr_task);

	snprintf(name, LEN, "dis_ki-%d", index);
	if (idlemgr_ctx->priv.cpu_id > 0)
		idlemgr->kick_task =
			kthread_create_on_cpu(mtk_drm_async_kick_idlemgr_thread,
					crtc, idlemgr_ctx->priv.cpu_id, name);
	else
		idlemgr->kick_task =
			kthread_create(mtk_drm_async_kick_idlemgr_thread, crtc, name);
	init_waitqueue_head(&idlemgr->kick_wq);
	atomic_set(&idlemgr->kick_task_active, 0);
	wake_up_process(idlemgr->kick_task);

	atomic_set(&idlemgr->async_enabled, 0);
	if (mtk_drm_helper_get_opt(priv->helper_opt,
				MTK_DRM_OPT_IDLEMGR_ASYNC)) {
		DDPMSG("%s, %d, init idle async\n", __func__, __LINE__);
		init_waitqueue_head(&idlemgr->async_event_wq);
		atomic_set(&idlemgr->async_ref, 0);

		snprintf(name, LEN, "dis_async-%d", index);
		idlemgr->async_handler_task =
			kthread_create(mtk_drm_async_handler_thread, crtc, name);
		init_waitqueue_head(&idlemgr->async_handler_wq);
		idlemgr->async_cb_count = 0;
		INIT_LIST_HEAD(&idlemgr->async_cb_list);
		spin_lock_init(&idlemgr->async_lock);
		wake_up_process(idlemgr->async_handler_task);

		if (idlemgr_ctx->priv.vblank_async == true) {
			DDPMSG("%s, %d, init vblank async\n", __func__, __LINE__);
			snprintf(name, LEN, "dis_vblank-%d", index);
			idlemgr->async_vblank_task =
				kthread_create(mtk_drm_async_vblank_thread, crtc, name);
			init_waitqueue_head(&idlemgr->async_vblank_wq);
			atomic_set(&idlemgr->async_vblank_active, 0);
			wake_up_process(idlemgr->async_vblank_task);
		}
	}
	return 0;
}

static void mtk_drm_idlemgr_poweroff_connector(struct drm_crtc *crtc)
{
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	struct mtk_ddp_comp *output_comp;

	output_comp = mtk_ddp_comp_request_output(mtk_crtc);
	if (output_comp)
		mtk_ddp_comp_io_cmd(output_comp, NULL, CONNECTOR_POWEROFF, NULL);
}

static void mtk_drm_idlemgr_disable_connector(struct drm_crtc *crtc)
{
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	struct mtk_ddp_comp *output_comp;
	bool async = false;

	async = mtk_drm_idlemgr_get_async_status(crtc);
	output_comp = mtk_ddp_comp_request_output(mtk_crtc);
	if (output_comp)
		mtk_ddp_comp_io_cmd(output_comp, NULL, CONNECTOR_DISABLE, &async);
}

static void mtk_drm_idlemgr_enable_connector(struct drm_crtc *crtc)
{
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	struct mtk_ddp_comp *output_comp;
	bool async = false;

	async = mtk_drm_idlemgr_get_async_status(crtc);
	output_comp = mtk_ddp_comp_request_output(mtk_crtc);
	if (output_comp)
		mtk_ddp_comp_io_cmd(output_comp, NULL, CONNECTOR_ENABLE, &async);
}

static void mtk_drm_idlemgr_disable_crtc(struct drm_crtc *crtc)
{
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	unsigned int crtc_id = drm_crtc_index(&mtk_crtc->base);
	bool mode = mtk_crtc_is_dc_mode(crtc);
	struct mtk_drm_private *priv =
				mtk_crtc->base.dev->dev_private;
	struct mtk_drm_idlemgr *idlemgr = mtk_crtc->idlemgr;
	struct mtk_ddp_comp *output_comp = NULL;
	int en = 0;
	struct cmdq_pkt *cmdq_handle1, *cmdq_handle2;
	struct mtk_drm_idlemgr_context *idlemgr_ctx = idlemgr->idlemgr_ctx;

	DDPINFO("%s, crtc%d+\n", __func__, crtc_id);

	if (mode) {
		DDPINFO("crtc%d mode:%d bypass enter idle\n", crtc_id, mode);
		DDPINFO("crtc%d do %s-\n", crtc_id, __func__);
		return;
	}

	if (mtk_drm_helper_get_opt(priv->helper_opt,
				MTK_DRM_OPT_IDLEMGR_ASYNC))
		atomic_set(&idlemgr->async_enabled, 1);

	/* 0. Waiting CLIENT_DSI_CFG/CLIENT_CFG thread done */
	mtk_crtc_pkt_create(&cmdq_handle1, crtc,
		mtk_crtc->gce_obj.client[CLIENT_DSI_CFG]);

	if (cmdq_handle1) {
		cmdq_pkt_flush(cmdq_handle1);
		cmdq_pkt_destroy(cmdq_handle1);
		cmdq_handle1 = NULL;
	}

	mtk_crtc_pkt_create(&cmdq_handle2, crtc, mtk_crtc->gce_obj.client[CLIENT_CFG]);

	if (cmdq_handle2) {
		cmdq_pkt_clear_event(cmdq_handle2,
				     mtk_crtc->gce_obj.event[EVENT_STREAM_EOF]);
		cmdq_pkt_clear_event(cmdq_handle2,
				     mtk_crtc->gce_obj.event[EVENT_STREAM_BLOCK]);
		cmdq_pkt_wfe(cmdq_handle2,
				     mtk_crtc->gce_obj.event[EVENT_CABC_EOF]);

		cmdq_pkt_flush(cmdq_handle2);

		if (mtk_crtc->is_mml) {
			mtk_crtc->mml_link_state = MML_IR_IDLE;
			CRTC_MMP_MARK(0, mml_dbg, (unsigned long)cmdq_handle2, MMP_MML_IDLE);
		}

		cmdq_pkt_destroy(cmdq_handle2);
		cmdq_handle2 = NULL;
	}

	/* 1. stop connector */
	mtk_drm_idlemgr_disable_connector(crtc);

	/* 2. stop CRTC */
	mtk_crtc_stop(mtk_crtc, false);
	CRTC_MMP_MARK((int)crtc_id, enter_idle, 1, 0);

	/* 3. disconnect addon module and recover config */
	mtk_crtc_disconnect_addon_module(crtc);
	CRTC_MMP_MARK((int)crtc_id, enter_idle, 2, 0);

	mtk_drm_idle_async_wait(crtc, 50, "stop_crtc_async");

	if (atomic_read(&idlemgr->async_enabled) == 1) {
		/* hrt bw has been cleared when mtk_crtc_stop,
		 * do dsi power off after enter ulps mode,
		 */
		mtk_drm_idlemgr_poweroff_connector(crtc);
		// trigger vblank async task
		if (idlemgr_ctx->priv.vblank_async == true) {
			mtk_drm_idlemgr_async_get(crtc, "vblank_off");
			atomic_set(&idlemgr->async_vblank_active, 1);
			wake_up_interruptible(&idlemgr->async_vblank_wq);
		}
	} else {
		/* 4. set HRT BW to 0 */
		if (mtk_drm_helper_get_opt(priv->helper_opt,
					MTK_DRM_OPT_MMQOS_SUPPORT))
			mtk_disp_set_hrt_bw(mtk_crtc, 0);
	}

	/* 5. Release MMCLOCK request */
	output_comp = mtk_ddp_comp_request_output(mtk_crtc);
	if (output_comp)
		mtk_ddp_comp_io_cmd(output_comp, NULL, SET_MMCLK_BY_DATARATE,
				&en);

	/* 6. disconnect path */
	mtk_crtc_disconnect_default_path(mtk_crtc);

	/* 7. power off all modules in this CRTC */
	mtk_crtc_ddp_unprepare(mtk_crtc);

	if (idlemgr_ctx->priv.vblank_async == false) {
		drm_crtc_vblank_off(crtc);
		mtk_crtc_vblank_irq(&mtk_crtc->base);
	}

	/* 8. power off MTCMOS */
	mtk_drm_top_clk_disable_unprepare(crtc->dev);
	CRTC_MMP_MARK((int)crtc_id, enter_idle, 3, 0);

	if (idlemgr_ctx->priv.vblank_async == true)
		mtk_drm_idle_async_wait(crtc, 0, "vblank_async");

	/* 9. disable fake vsync if need */
	mtk_drm_fake_vsync_switch(crtc, false);

	/* 10. CMDQ power off */
	cmdq_mbox_disable(mtk_crtc->gce_obj.client[CLIENT_CFG]->chan);

	if (mtk_drm_helper_get_opt(priv->helper_opt,
				MTK_DRM_OPT_IDLEMGR_ASYNC))
		atomic_set(&idlemgr->async_enabled, 0);
	DDPINFO("crtc%d do %s-\n", crtc_id, __func__);
}

/* TODO: we should restore the current setting rather than default setting */
static void mtk_drm_idlemgr_enable_crtc(struct drm_crtc *crtc)
{
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	unsigned int crtc_id = drm_crtc_index(crtc);
	struct mtk_drm_private *priv =
			mtk_crtc->base.dev->dev_private;
	bool mode = mtk_crtc_is_dc_mode(crtc);
	struct mtk_ddp_comp *comp;
	unsigned int i, j;
	struct mtk_ddp_comp *output_comp = NULL;
	int en = 1;
	struct mtk_crtc_state *crtc_state = to_mtk_crtc_state(crtc->state);
	struct mtk_drm_idlemgr *idlemgr = mtk_crtc->idlemgr;

	DDPINFO("crtc%d do %s+\n", crtc_id, __func__);

	if (mode) {
		DDPINFO("crtc%d mode:%d bypass exit idle\n", crtc_id, mode);
		DDPINFO("crtc%d do %s-\n", crtc_id, __func__);
		return;
	}

	if (mtk_drm_helper_get_opt(priv->helper_opt,
				MTK_DRM_OPT_IDLEMGR_ASYNC))
		atomic_set(&idlemgr->async_enabled, 1);

	/* 0. CMDQ power on */
	cmdq_mbox_enable(mtk_crtc->gce_obj.client[CLIENT_CFG]->chan);

	/* 1. power on mtcmos & init apsrc*/
	mtk_drm_top_clk_prepare_enable(crtc->dev);
	mtk_crtc_v_idle_apsrc_control(crtc, NULL, true, true,
		MTK_APSRC_CRTC_DEFAULT, false);

	/* 2. prepare modules would be used in this CRTC */
	mtk_drm_idlemgr_enable_connector(crtc);

	/* 3. start event loop first */
	if (crtc_id == 0) {
		if (mtk_crtc_with_event_loop(crtc) &&
			(mtk_crtc_is_frame_trigger_mode(crtc)))
			mtk_crtc_start_event_loop(crtc);
	}

	/* 4. prepare modules would be used in this CRTC */
	mtk_crtc_ddp_prepare(mtk_crtc);

	mtk_drm_idle_async_wait(crtc, 50, "prepare_async");

	mtk_gce_backup_slot_init(mtk_crtc);

#ifndef DRM_CMDQ_DISABLE
	if (mtk_drm_helper_get_opt(priv->helper_opt, MTK_DRM_OPT_USE_M4U))
		mtk_crtc_prepare_instr(crtc);
#endif

	/* 5. start trigger loop first to keep gce alive */
	output_comp = mtk_ddp_comp_request_output(mtk_crtc);
	if (!IS_ERR_OR_NULL(output_comp) &&
		mtk_ddp_comp_get_type(output_comp->id) == MTK_DSI) {
		if (mtk_crtc_with_sodi_loop(crtc) &&
			(!mtk_crtc_is_frame_trigger_mode(crtc)))
			mtk_crtc_start_sodi_loop(crtc);

		mtk_crtc_start_trig_loop(crtc);
		mtk_crtc_hw_block_ready(crtc);
	}

	mtk_drm_idle_async_wait(crtc, 80, "gce_thread_async");

	/* 6. connect path */
	mtk_crtc_connect_default_path(mtk_crtc);

	/* 7. config ddp engine & set dirty for cmd mode */
	mtk_crtc_config_default_path(mtk_crtc);

	/* 8. conect addon module and config
	 *    skip mml addon connect if kick idle by atomic commit
	 */
	if (crtc_state->lye_state.mml_ir_lye || crtc_state->lye_state.mml_dl_lye)
		mtk_crtc_addon_connector_connect(crtc, NULL);
	else
		mtk_crtc_connect_addon_module(crtc);

	/* 9. restore OVL setting */
	mtk_crtc_restore_plane_setting(mtk_crtc);

	/* 10. Set QOS BW */
	for_each_comp_in_cur_crtc_path(comp, mtk_crtc, i, j)
		mtk_ddp_comp_io_cmd(comp, NULL, PMQOS_SET_BW, NULL);

	/* 11. restore HRT BW */
	if (mtk_drm_helper_get_opt(priv->helper_opt,
			MTK_DRM_OPT_MMQOS_SUPPORT))
		mtk_disp_set_hrt_bw(mtk_crtc,
			mtk_crtc->qos_ctx->last_hrt_req);

	/* 12. Request MMClock */
	mtk_crtc_attach_ddp_comp(crtc, mtk_crtc->ddp_mode, true);
	if (output_comp)
		mtk_ddp_comp_io_cmd(output_comp, NULL, SET_MMCLK_BY_DATARATE,
				&en);

	mtk_drm_idle_async_wait(crtc, 0, "conifg_async");

	/* 13. set vblank */
	drm_crtc_vblank_on(crtc);

	/* 14. enable fake vsync if need */
	mtk_drm_fake_vsync_switch(crtc, true);

	if (mtk_drm_helper_get_opt(priv->helper_opt,
				MTK_DRM_OPT_IDLEMGR_ASYNC))
		atomic_set(&idlemgr->async_enabled, 0);

	DDPINFO("crtc%d do %s-\n", crtc_id, __func__);
}
