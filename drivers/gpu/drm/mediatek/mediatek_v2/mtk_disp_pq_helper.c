// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2022 MediaTek Inc.
 */

#include <linux/clk.h>
#include <linux/iopoll.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>

#ifndef DRM_CMDQ_DISABLE
#include <linux/soc/mediatek/mtk-cmdq-ext.h>
#else
#include "mtk-cmdq-ext.h"
#endif

#include "mtk_drm_ddp_comp.h"
#include "mtk_drm_drv.h"
#include "mtk_drm_crtc.h"
#include "mtk_drm_mmp.h"
#include "mtk_disp_pq_helper.h"
#include "mtk_log.h"
#include "mtk_dump.h"
#include "mtk_drm_trace.h"
#include "mtk_disp_ccorr.h"
#include "mtk_disp_c3d.h"
#include "mtk_disp_tdshp.h"
#include "mtk_disp_aal.h"
#include "mtk_disp_color.h"
#include "mtk_disp_dither.h"
#include "mtk_disp_gamma.h"

#define REQUEST_MAX_COUNT 20
#define CHECK_TRIGGER_DELAY 1

struct pq_module_match {
	enum mtk_pq_module_type pq_type;
	enum mtk_ddp_comp_type type;
};

static struct pq_module_match pq_module_matches[MTK_DISP_PQ_TYPE_MAX] = {
	{MTK_DISP_PQ_COLOR, MTK_DISP_COLOR}, // 0
	{MTK_DISP_PQ_DITHER, MTK_DISP_DITHER},
	{MTK_DISP_PQ_CCORR, MTK_DISP_CCORR},
	{MTK_DISP_PQ_AAL, MTK_DISP_AAL},
	{MTK_DISP_PQ_GAMMA, MTK_DISP_GAMMA},

	{MTK_DISP_PQ_CHIST, MTK_DISP_CHIST}, // 5
	{MTK_DISP_PQ_C3D, MTK_DISP_C3D},
	{MTK_DISP_PQ_TDSHP, MTK_DISP_TDSHP},
};

static int mtk_drm_ioctl_pq_get_irq_impl(struct drm_crtc *crtc, void *data);
static int mtk_drm_ioctl_pq_get_persist_property_impl(struct drm_crtc *crtc, void *data);

int mtk_drm_ioctl_pq_frame_config(struct drm_device *dev, void *data,
	struct drm_file *file_priv)
{
	struct drm_crtc *crtc;
	struct mtk_drm_pq_config_ctl *params = data;
	int ret;

	if (data == NULL) {
		DDPPR_ERR("%s, null data!\n", __func__);
		return -1;
	}

	crtc = drm_crtc_find(dev, file_priv, params->crtc_id);
	if (!crtc) {
		DDPPR_ERR("%s, invalid crtc id:%d!\n", __func__, params->crtc_id);
		return -1;
	}

	ret = mtk_pq_helper_frame_config(crtc, NULL, data, true);

	return ret;
}

int mtk_drm_virtual_type_impl(struct drm_crtc *crtc, struct drm_device *dev,
		unsigned int cmd, char *kdata, struct drm_file *file_priv)
{
	int ret = -1;

	switch (cmd) {
	case PQ_VIRTUAL_SET_PROPERTY:
		ret = mtk_drm_ioctl_pq_get_persist_property_impl(crtc, kdata);
		break;
	case PQ_VIRTUAL_GET_MASTER_INFO:
		ret = mtk_drm_get_master_info_ioctl(dev, kdata, file_priv);
		break;
	case PQ_VIRTUAL_GET_IRQ:
		ret = mtk_drm_ioctl_pq_get_irq_impl(crtc, kdata);
		break;
	default:
		break;
	}
	return ret;
}

int mtk_drm_ioctl_pq_proxy(struct drm_device *dev, void *data, struct drm_file *file_priv)
{
	struct drm_crtc *crtc;
	struct mtk_drm_pq_proxy_ctl *params = data;
	struct mtk_ddp_comp *comp;
	unsigned int pq_type;
	unsigned int cmd;
	unsigned int i, j;
	char stack_kdata[128];
	char *kdata = NULL;
	unsigned long long time;
	int ret = -1;

	if (!params || !params->size || !params->data) {
		DDPPR_ERR("%s, null pointer!\n", __func__);
		return -1;
	}

	crtc = drm_crtc_find(dev, file_priv, params->crtc_id);
	if (!crtc) {
		DDPPR_ERR("%s, invalid crtc id:%d!\n", __func__, params->crtc_id);
		return -1;
	}

	pq_type = params->cmd >> 16;
	cmd = params->cmd & 0xffff;
	time = sched_clock();

	if (params->size <= sizeof(stack_kdata))
		kdata = stack_kdata;
	else
		kdata = kmalloc(params->size, GFP_KERNEL);

	if (!kdata) {
		DDPPR_ERR("%s:%d, kdata alloc failed pq_type:%d, cmd:%d\n", __func__,
				__LINE__, pq_type, cmd);
		return -1;
	}

	if (copy_from_user(kdata, (void __user *)params->data, params->size) != 0)
		goto err;

	if (pq_type == MTK_DISP_VIRTUAL_TYPE) {
		ret = mtk_drm_virtual_type_impl(crtc, dev, cmd, kdata, file_priv);
	} else {
		for_each_comp_in_cur_crtc_path(comp, to_mtk_crtc(crtc), i, j) {
			if (pq_module_matches[pq_type].type == mtk_ddp_comp_get_type(comp->id)) {
				ret = mtk_ddp_comp_pq_ioctl_transact(comp, cmd, kdata,
									params->size);
				if (ret < 0)
					DDPPR_ERR("%s:%d, ioctl transact failed, comp:%d,%d\n",
						__func__, __LINE__, comp->id, cmd);
			}
		}
	}

	if (cmd > PQ_GET_CMD_START) {
		if (copy_to_user((void __user *)params->data, kdata,  params->size) != 0)
			goto err;
	}
	if (cmd != PQ_AAL_EVENTCTL && cmd < PQ_GET_CMD_START)
		DDPMSG("%s, crtc index:%d, pq_type:%d, cmd:%d, use %llu us\n", __func__,
				drm_crtc_index(crtc), pq_type, cmd, (sched_clock() - time) / 1000);
	else
		DDPINFO("%s, crtc index:%d, pq_type:%d, cmd:%d, use %llu us\n", __func__,
				drm_crtc_index(crtc), pq_type, cmd, (sched_clock() - time) / 1000);
err:
	if (kdata != stack_kdata)
		kfree(kdata);
	return ret;
}

static void frame_cmdq_cb(struct cmdq_cb_data data)
{
	struct mtk_cmdq_cb_data *cb_data = data.data;

	cmdq_pkt_destroy(cb_data->cmdq_handle);
	kfree(cb_data);
}


int mtk_pq_helper_frame_config(struct drm_crtc *crtc, struct cmdq_pkt *cmdq_handle,
	void *data, bool user_lock)
{
	struct mtk_drm_pq_config_ctl *params = data;
	unsigned int cmds_len = params->len;
	struct mtk_cmdq_cb_data *cb_data = NULL;
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	struct cmdq_pkt *pq_cmdq_handle;
	struct mtk_ddp_comp *comp;
	struct mtk_drm_pq_param requests[REQUEST_MAX_COUNT];
	unsigned int check_trigger = params->check_trigger;
	unsigned int i, j;
	int index = drm_crtc_index(crtc);
	bool is_atomic_commit = cmdq_handle;

	DDPINFO("%s:%d ++, crtc index:%d\n", __func__, __LINE__, index);
	mtk_drm_trace_begin("mtk_pq_helper_frame_config");
	CRTC_MMP_EVENT_START(index, pq_frame_config, (unsigned long)crtc, 0);

	if (!cmds_len || cmds_len > REQUEST_MAX_COUNT || params->data == NULL) {
		DDPPR_ERR("%s:%d, invalid requests for pq config\n",
			__func__, __LINE__);
		CRTC_MMP_MARK(index, pq_frame_config, 0, 1);
		mtk_drm_trace_end();

		return -1;
	}

	if (copy_from_user(&requests, params->data, sizeof(struct mtk_drm_pq_param) * cmds_len)) {
		CRTC_MMP_MARK(index, pq_frame_config, 0, 2);
		mtk_drm_trace_end();

		return -1;
	}
	if (index) {
		DDPPR_ERR("%s:%d, invalid crtc:0x%p, index:%d\n",
				__func__, __LINE__, crtc, index);
		CRTC_MMP_MARK(index, pq_frame_config, 0, 3);
		mtk_drm_trace_end();
		return -1;
	}

	if (!(mtk_crtc->enabled)) {
		DDPINFO("%s:%d, slepted\n", __func__, __LINE__);
		CRTC_MMP_MARK(index, pq_frame_config, 0, 4);
		mtk_drm_trace_end();

		return -1;
	}

	if (is_atomic_commit)
		pq_cmdq_handle = cmdq_handle;
	else {
		pq_cmdq_handle = cmdq_pkt_create(mtk_crtc->gce_obj.client[CLIENT_CFG]);
		if (!pq_cmdq_handle) {
			DDPPR_ERR("%s:%d NULL cmdq handle\n", __func__, __LINE__);
			CRTC_MMP_MARK(index, pq_frame_config, 0, 5);
			return -1;
		}

		if (mtk_crtc_with_sub_path(crtc, mtk_crtc->ddp_mode))
			mtk_crtc_wait_frame_done(mtk_crtc, pq_cmdq_handle,
				DDP_SECOND_PATH, 0);
		else
			mtk_crtc_wait_frame_done(mtk_crtc, pq_cmdq_handle, DDP_FIRST_PATH, 0);
	}

	/* call comp frame config */
	for (index = 0; index < cmds_len; index++) {
		unsigned int pq_type = requests[index].cmd >> 16;
		unsigned int cmd = requests[index].cmd & 0xffff;

		if (pq_type >= MTK_DISP_PQ_TYPE_MAX || !requests[index].size)
			continue;

		DDPINFO("%s, pq_type:%d, cmd:%d\n", __func__, pq_type, cmd);
		for_each_comp_in_cur_crtc_path(comp, mtk_crtc, i, j) {
			if (pq_module_matches[pq_type].type == mtk_ddp_comp_get_type(comp->id)) {
				char stack_kdata[128];
				char *kdata = NULL;

				if (requests[index].size <= sizeof(stack_kdata))
					kdata = stack_kdata;
				else
					kdata = kmalloc(requests[index].size, GFP_KERNEL);

				if (!kdata) {
					DDPPR_ERR("%s:%d, kdata alloc failed comp:%d,%d\n",
							__func__, __LINE__, comp->id, cmd);
					continue;
				}
				if (copy_from_user(kdata, (void __user *)requests[index].data,
						requests[index].size) == 0) {
					mtk_drm_trace_begin("frame_config(compId: %d)", comp->id);

					if (mtk_ddp_comp_pq_frame_config(comp, pq_cmdq_handle,
							cmd, kdata, requests[index].size) < 0)
						DDPPR_ERR("%s:%d, config failed, comp:%d,%d\n",
							__func__, __LINE__, comp->id, cmd);

					mtk_drm_trace_end();
				}

				if (kdata != stack_kdata)
					kfree(kdata);
			}
		}
	}

	/* atomic commit will flush in crtc */
	if (!is_atomic_commit) {
		cb_data = kmalloc(sizeof(*cb_data), GFP_KERNEL);
		if (!cb_data) {
			DDPPR_ERR("cb data creation failed\n");
			CRTC_MMP_MARK(index, pq_frame_config, 0, 6);
			return -1;
		}

		cb_data->crtc = crtc;
		cb_data->cmdq_handle = pq_cmdq_handle;
		if (user_lock)
			DDP_MUTEX_LOCK(&mtk_crtc->lock, __func__, __LINE__);

		mtk_drm_trace_begin("mtk_drm_idlemgr_kick");
		mtk_drm_idlemgr_kick(__func__, crtc, !user_lock);
		mtk_drm_trace_end();

		if (!(mtk_crtc->enabled)) {
			DDPINFO("%s:%d, slepted\n", __func__, __LINE__);
			kfree(cb_data);
			if (user_lock)
				DDP_MUTEX_UNLOCK(&mtk_crtc->lock, __func__, __LINE__);
			CRTC_MMP_MARK(index, pq_frame_config, 0, 7);
			return -1;
		}

		mtk_drm_trace_begin("flush+check_trigger");
		if (cmdq_pkt_flush_threaded(pq_cmdq_handle, frame_cmdq_cb, cb_data) < 0) {
			DDPPR_ERR("failed to flush %s\n", __func__);
			kfree(cb_data);
		} else if (check_trigger)
			mtk_crtc_check_trigger(mtk_crtc, check_trigger == CHECK_TRIGGER_DELAY
						|| mtk_crtc->msync2.msync_frame_status, !user_lock);
		mtk_drm_trace_end();

		if (user_lock)
			DDP_MUTEX_UNLOCK(&mtk_crtc->lock, __func__, __LINE__);

	}
	CRTC_MMP_EVENT_END(index, pq_frame_config, 0, 0);
	DDPINFO("%s:%d --\n", __func__, __LINE__);
	mtk_drm_trace_end();

	return 0;
}

int mtk_pq_helper_fill_comp_pipe_info(struct mtk_ddp_comp *comp, int *path_order,
	bool *is_right_pipe, struct mtk_ddp_comp **companion)
{
	int _path_order, ret;
	bool _is_right_pipe;
	struct mtk_ddp_comp *_companion = NULL;
	int comp_type;

	ret = mtk_ddp_comp_locate_in_cur_crtc_path(comp->mtk_crtc, comp->id,
					&_is_right_pipe, &_path_order);
	if (ret < 0)
		return ret;
	if (is_right_pipe)
		*is_right_pipe = _is_right_pipe;
	if (path_order)
		*path_order = _path_order;
	DDPMSG("%s %s order %d pipe %d\n", __func__,
					mtk_dump_comp_str(comp), _path_order, _is_right_pipe);
	if (!comp->mtk_crtc->is_dual_pipe || !companion)
		return ret;

	comp_type = mtk_ddp_comp_get_type(comp->id);
	if (comp_type < 0) {
		DDPPR_ERR("%s comp id %d is invalid\n", __func__, comp->id);
		return comp_type;
	}
	if (!_is_right_pipe)
		_companion = mtk_ddp_comp_sel_in_dual_pipe(comp->mtk_crtc,
					comp_type, _path_order);
	else
		_companion = mtk_ddp_comp_sel_in_cur_crtc_path(comp->mtk_crtc,
					comp_type, _path_order);
	if (!_companion)
		ret = -1;
	if (_companion && companion)
		*companion = _companion;
	DDPMSG("%s companion %s\n", __func__, mtk_dump_comp_str(_companion));
	return ret;
}

void mtk_disp_pq_on_start_of_frame(struct mtk_drm_crtc *mtk_crtc)
{
	struct pq_common_data *pq_data = mtk_crtc->pq_data;
	struct mtk_ddp_comp *ccorr_comp = mtk_ddp_comp_sel_in_cur_crtc_path(
			mtk_crtc, MTK_DISP_CCORR, 0);
	struct mtk_disp_ccorr *ccorr_data = comp_to_ccorr(ccorr_comp);
	struct mtk_disp_ccorr_primary *ccorr_primary = ccorr_data->primary_data;
	struct mtk_ddp_comp *c3d_comp = mtk_ddp_comp_sel_in_cur_crtc_path(
			mtk_crtc, MTK_DISP_C3D, 0);
	struct mtk_disp_c3d *c3d_data = comp_to_c3d(c3d_comp);
	struct mtk_disp_c3d_primary *c3d_primary = c3d_data->primary_data;

	if ((atomic_read(&ccorr_primary->ccorr_irq_en) == 1) ||
			(atomic_read(&c3d_primary->c3d_eventctl) == 1)) {
		if (atomic_read(&ccorr_primary->ccorr_irq_en) == 1)
			atomic_set(&pq_data->pq_get_irq, 1);
		if (atomic_read(&c3d_primary->c3d_eventctl) == 1)
			atomic_set(&pq_data->pq_get_irq, 2);

		wake_up_interruptible(&pq_data->pq_get_irq_wq);
	}
}

static int mtk_disp_pq_wait_irq(struct mtk_drm_crtc *mtk_crtc)
{
	int ret = 0;
	struct pq_common_data *pq_data = mtk_crtc->pq_data;

	if (atomic_read(&pq_data->pq_get_irq) == 0) {
		DDPDBG("%s: wait_event_interruptible ++\n", __func__);
		ret = wait_event_interruptible(pq_data->pq_get_irq_wq,
			(atomic_read(&pq_data->pq_get_irq) == 1)
			|| (atomic_read(&pq_data->pq_get_irq) == 2));
		if (ret >= 0)
			DDPDBG("%s: wait_event_interruptible --\n", __func__);
		else
			DDPDBG("%s: interrupted unexpected\n", __func__);
	} else {
		DDPDBG("%s: irq_status = %d\n", __func__, atomic_read(&pq_data->pq_get_irq));
	}

	atomic_set(&pq_data->pq_irq_trig_src, atomic_read(&pq_data->pq_get_irq));
	atomic_set(&pq_data->pq_get_irq, 0);

	return ret;
}

static int mtk_disp_pq_copy_data_to_user(struct mtk_drm_crtc *mtk_crtc,
		struct mtk_disp_pq_irq_data *data)
{
	int ret = 0;
	struct mtk_ddp_comp *ccorr_comp = mtk_ddp_comp_sel_in_cur_crtc_path(
			mtk_crtc, MTK_DISP_CCORR, 0);
	struct mtk_disp_ccorr *ccorr_data = comp_to_ccorr(ccorr_comp);
	struct mtk_disp_ccorr_primary *ccorr_primary = ccorr_data->primary_data;
	struct pq_common_data *pq_data = mtk_crtc->pq_data;

	ccorr_primary->old_pq_backlight = ccorr_primary->pq_backlight;
	data->backlight = ccorr_primary->pq_backlight;

	if (atomic_read(&pq_data->pq_irq_trig_src) == 1)
		data->irq_src = TRIG_BY_CCORR;
	else if (atomic_read(&pq_data->pq_irq_trig_src) == 2)
		data->irq_src = TRIG_BY_C3D;
	else
		DDPMSG("%s: trig flag error!\n", __func__);

	return ret;
}

static int mtk_drm_ioctl_pq_get_irq_impl(struct drm_crtc *crtc, void *data)
{
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	int ret = 0;

	mtk_disp_pq_wait_irq(mtk_crtc);
	if (mtk_disp_pq_copy_data_to_user(mtk_crtc, (struct mtk_disp_pq_irq_data *)data) < 0) {
		DDPMSG("%s: failed!\n", __func__);
		ret = -EFAULT;
	}

	return ret;
}

int mtk_drm_ioctl_pq_get_irq(struct drm_device *dev, void *data,
				struct drm_file *file_priv)
{
	struct mtk_drm_private *private = dev->dev_private;
	struct drm_crtc *crtc = private->crtc[0];

	return mtk_drm_ioctl_pq_get_irq_impl(crtc, data);
}

static int mtk_drm_ioctl_pq_get_persist_property_impl(struct drm_crtc *crtc, void *data)
{
	int i;
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	struct pq_common_data *pq_data = mtk_crtc->pq_data;
	unsigned int pq_persist_property[32];

	memset(pq_persist_property, 0, sizeof(pq_persist_property));
	memcpy(pq_persist_property, (unsigned int *)data, sizeof(pq_persist_property));

	for (i = 0; i < DISP_PQ_PROPERTY_MAX; i++) {
		pq_data->old_persist_property[i] = pq_data->new_persist_property[i];
		pq_data->new_persist_property[i] = pq_persist_property[i];
	}

	DDPFUNC("+");

	if (pq_data->old_persist_property[DISP_PQ_COLOR_BYPASS] !=
		pq_data->new_persist_property[DISP_PQ_COLOR_BYPASS])
		disp_color_set_bypass(crtc, pq_data->new_persist_property[DISP_PQ_COLOR_BYPASS]);

	if (pq_data->old_persist_property[DISP_PQ_CCORR_BYPASS] !=
		pq_data->new_persist_property[DISP_PQ_CCORR_BYPASS])
		disp_ccorr_set_bypass(crtc, pq_data->new_persist_property[DISP_PQ_CCORR_BYPASS]);

	if (pq_data->old_persist_property[DISP_PQ_GAMMA_BYPASS] !=
		pq_data->new_persist_property[DISP_PQ_GAMMA_BYPASS])
		disp_gamma_set_bypass(crtc, pq_data->new_persist_property[DISP_PQ_GAMMA_BYPASS]);

	if (pq_data->old_persist_property[DISP_PQ_DITHER_BYPASS] !=
		pq_data->new_persist_property[DISP_PQ_DITHER_BYPASS])
		disp_dither_set_bypass(crtc, pq_data->new_persist_property[DISP_PQ_DITHER_BYPASS]);

	if (pq_data->old_persist_property[DISP_PQ_AAL_BYPASS] !=
		pq_data->new_persist_property[DISP_PQ_AAL_BYPASS])
		disp_aal_set_bypass(crtc, pq_data->new_persist_property[DISP_PQ_AAL_BYPASS]);

	if (pq_data->old_persist_property[DISP_PQ_C3D_BYPASS] !=
		pq_data->new_persist_property[DISP_PQ_C3D_BYPASS])
		disp_c3d_set_bypass(crtc, pq_data->new_persist_property[DISP_PQ_C3D_BYPASS]);

	if (pq_data->old_persist_property[DISP_PQ_TDSHP_BYPASS] !=
		pq_data->new_persist_property[DISP_PQ_TDSHP_BYPASS])
		disp_tdshp_set_bypass(crtc, pq_data->new_persist_property[DISP_PQ_TDSHP_BYPASS]);

	if (pq_data->old_persist_property[DISP_PQ_DITHER_COLOR_DETECT] !=
		pq_data->new_persist_property[DISP_PQ_DITHER_COLOR_DETECT])
		disp_dither_set_color_detect(crtc,
			pq_data->new_persist_property[DISP_PQ_DITHER_COLOR_DETECT]);

	DDPFUNC("-");

	return 0;
}

int mtk_drm_ioctl_pq_get_persist_property(struct drm_device *dev, void *data,
	struct drm_file *file_priv)
{
	struct mtk_drm_private *private = dev->dev_private;
	struct drm_crtc *crtc = private->crtc[0];

	return mtk_drm_ioctl_pq_get_persist_property_impl(crtc, data);
}
