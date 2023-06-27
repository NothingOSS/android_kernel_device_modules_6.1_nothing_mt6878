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
#define TUNING_COMPS_MAX_COUNT 15

#if defined(DISP_COLOR_ON)
#define COLOR_MODE			(1)
#elif defined(MDP_COLOR_ON)
#define COLOR_MODE			(2)
#elif defined(DISP_MDP_COLOR_ON)
#define COLOR_MODE			(3)
#else
#define COLOR_MODE			(0)	/*color feature off */
#endif

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

static const char *const mtk_tuning_mdp_comps_name[TUNING_COMPS_MAX_COUNT] = {
	"mediatek,mdp_rsz0",              // 0
	"mediatek,mdp_rsz1",
	"mediatek,mdp_rdma0",
	"mediatek,mdp-tuning-mdp_hdr0",
	"mediatek,mml-tuning-mml_hdr0",
	"mediatek,mdp-tuning-mdp_color0", // 5
	"mediatek,mml-tuning-mml_color0",
	"mediatek,mdp-tuning-mdp_aal0",
	"mediatek,mml-tuning-mml_aal0",
	"mediatek,mdp-tuning-mdp_tdshp0",
	"mediatek,mml-tuning-mml_tdshp0", // 10
	"mediatek,disp_oddmr0",
	"mediatek,disp1_oddmr0",
};

static int mtk_drm_ioctl_pq_get_irq_impl(struct drm_crtc *crtc, void *data);
static int mtk_drm_ioctl_pq_get_persist_property_impl(struct drm_crtc *crtc, void *data);

static bool mtk_drm_get_resource_from_dts(struct resource *res, const char *node_name)
{
	int rc = 0;
	struct device_node *node = NULL;

	if (!node_name)
		return false;

	node = of_find_compatible_node(NULL, NULL, node_name);
	rc = of_address_to_resource(node, 0, res);

	// check if fail to get reg.
	if (rc) {
		DDPINFO("Fail to get %s\n", node_name);
		return false;
	}

	DDPDBG("%s REG: 0x%llx ~ 0x%llx\n", node_name, res->start, res->end);
	return true;
}

int mtk_drm_ioctl_sw_read_impl(struct drm_crtc *crtc, void *data)
{
	struct DISP_READ_REG *rParams = data;
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	struct pq_common_data *pq_data = mtk_crtc->pq_data;
	unsigned int ret = 0;
	unsigned int reg_id = rParams->reg;
	struct resource res;
	int index = drm_crtc_index(crtc);

	switch (reg_id) {
	case SWREG_COLOR_BASE_ADDRESS:
		ret = pq_data->tuning_pa_table[TUNING_DISP_COLOR].pa_base;
		break;
	case SWREG_GAMMA_BASE_ADDRESS:
		ret = pq_data->tuning_pa_table[TUNING_DISP_GAMMA].pa_base;
		break;
	case SWREG_AAL_BASE_ADDRESS:
		ret = pq_data->tuning_pa_table[TUNING_DISP_AAL].pa_base;
		break;
#if defined(CCORR_SUPPORT)
	case SWREG_CCORR_BASE_ADDRESS:
		ret = pq_data->tuning_pa_table[TUNING_DISP_CCORR].pa_base;
		break;
#endif
	case SWREG_DISP_TDSHP_BASE_ADDRESS:
		ret = pq_data->tuning_pa_table[TUNING_DISP_TDSHP].pa_base;
		break;
	case SWREG_MML_HDR_BASE_ADDRESS:
		if (mtk_drm_get_resource_from_dts(&res, "mediatek,mml-tuning-mml_hdr0"))
			ret = res.start;
		break;
	case SWREG_MML_AAL_BASE_ADDRESS:
		if (mtk_drm_get_resource_from_dts(&res, "mediatek,mml-tuning-mml_aal0"))
			ret = res.start;
		break;
	case SWREG_MML_TDSHP_BASE_ADDRESS:
		if (mtk_drm_get_resource_from_dts(&res, "mediatek,mml-tuning-mml_tdshp0"))
			ret = res.start;
		break;
	case SWREG_MML_COLOR_BASE_ADDRESS:
		if (mtk_drm_get_resource_from_dts(&res, "mediatek,mml-tuning-mml_color0"))
			ret = res.start;
		break;
	case SWREG_TDSHP_BASE_ADDRESS:
		if (mtk_drm_get_resource_from_dts(&res, "mediatek,mdp-tuning-mdp_tdshp0"))
			ret = res.start;
		break;
	case SWREG_MDP_COLOR_BASE_ADDRESS:
		if (mtk_drm_get_resource_from_dts(&res, "mediatek,mdp-tuning-mdp_color0"))
			ret = res.start;
		break;
	case SWREG_COLOR_MODE:
		ret = COLOR_MODE;
		break;
	case SWREG_RSZ_BASE_ADDRESS:
#if defined(SUPPORT_ULTRA_RESOLUTION)
		ret = MDP_RSZ0_PA_BASE;
#endif
		break;
	case SWREG_MDP_RDMA_BASE_ADDRESS:
		if (!mtk_drm_get_resource_from_dts(&res, "mediatek,mdp-tuning-mdp_hdr0") &&
			mtk_drm_get_resource_from_dts(&res, "mediatek,mdp_rdma0"))
			ret = res.start;
		break;
	case SWREG_MDP_AAL_BASE_ADDRESS:
		if (mtk_drm_get_resource_from_dts(&res, "mediatek,mdp-tuning-mdp_aal0"))
			ret = res.start;
		break;
	case SWREG_MDP_HDR_BASE_ADDRESS:
		if (mtk_drm_get_resource_from_dts(&res, "mediatek,mdp-tuning-mdp_hdr0"))
			ret = res.start;
		break;
	case SWREG_TDSHP_TUNING_MODE:
		ret = pq_data->tdshp_flag;
		break;
	case SWREG_MIRAVISION_VERSION:
		ret = MIRAVISION_VERSION;
		break;
	case SWREG_SW_VERSION_VIDEO_DC:
		ret = SW_VERSION_VIDEO_DC;
		break;
	case SWREG_SW_VERSION_AAL:
		ret = SW_VERSION_AAL;
		break;
	default:
		DDPINFO("%s, ret = 0x%08x. unknown reg_id: 0x%08x",
				__func__, ret, reg_id);
	}

	rParams->val = ret;
	DDPDBG("%s, crtc_idx:%d, read sw reg 0x%x = 0x%x",
			__func__, index, rParams->reg, rParams->val);
	return ret;
}

int mtk_drm_ioctl_sw_write_impl(struct drm_crtc *crtc, void *data)
{
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	struct pq_common_data *pq_data = mtk_crtc->pq_data;
	struct DISP_WRITE_REG *wParams = data;
	unsigned int reg_id = wParams->reg;
	unsigned int value = wParams->val;

	if (reg_id == SWREG_TDSHP_TUNING_MODE)
		pq_data->tdshp_flag = value;
	return 0;
}

static int mtk_drm_get_table_index(struct drm_crtc *crtc, unsigned int pa)
{
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	struct pq_common_data *pq_data = mtk_crtc->pq_data;
	unsigned int pa_base = pa & 0xFFFFF000;
	int i;

	for (i = 0; i < TUNING_REG_MAX; i++) {
		if (pq_data->tuning_pa_table[i].pa_base == pa_base)
			return i;
	}
	return -1;
}

static bool mtk_drm_tuning_pa_valid(struct drm_crtc *crtc, unsigned int pa)
{
	int i;
	struct resource res;

	if (!pa) {
		DDPPR_ERR("addr is NULL\n");
		return false;
	}
	if ((pa & 0x3) != 0) {
		DDPPR_ERR("addr is not 4-byte aligned!\n");
		return false;
	}
	if (mtk_drm_get_table_index(crtc, pa) >= 0)
		return true;

	for (i = 0; i < TUNING_COMPS_MAX_COUNT; i ++) {
		if (mtk_drm_get_resource_from_dts(&res, mtk_tuning_mdp_comps_name[i])) {
			if (pa >= res.start && pa <= res.end)
				return true;
		}
	}
	return false;
}

int mtk_drm_ioctl_hw_read_impl(struct drm_crtc *crtc, void *data)
{
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	int ret = 0;
	struct DISP_READ_REG *rParams = data;
	void __iomem *va = 0;
	unsigned int pa;

	pa = (unsigned int)rParams->reg;

	if (!mtk_drm_tuning_pa_valid(crtc, pa)) {
		DDPPR_ERR("reg read, addr invalid, pa:0x%x\n", pa);
		return -EFAULT;
	}

	va = ioremap(pa, sizeof(*va));

	DDP_MUTEX_LOCK(&mtk_crtc->lock, __func__, __LINE__);
	rParams->val = readl(va) & rParams->mask;
	DDP_MUTEX_UNLOCK(&mtk_crtc->lock, __func__, __LINE__);
	DDPINFO("%s, read pa:0x%x(va:0x%lx) = 0x%x (0x%x)\n",
		__func__, pa, (long)va, rParams->val, rParams->mask);

	iounmap(va);
	return ret;
}

static void frame_cmdq_cb(struct cmdq_cb_data data)
{
	struct mtk_cmdq_cb_data *cb_data = data.data;

	cmdq_pkt_destroy(cb_data->cmdq_handle);
	kfree(cb_data);
}

int mtk_drm_ioctl_hw_write_impl(struct drm_crtc *crtc, void *data)
{
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	struct DISP_WRITE_REG *wParams = data;
	unsigned int pa = (unsigned int)wParams->reg;
	struct pq_common_data *pq_data = mtk_crtc->pq_data;
	struct mtk_cmdq_cb_data *cb_data = NULL;
	struct cmdq_pkt *cmdq_handle = NULL;

	if (!mtk_drm_tuning_pa_valid(crtc, pa)) {
		DDPPR_ERR("reg write, addr invalid, pa:0x%x\n", pa);
		return -EFAULT;
	}

	cmdq_handle = cmdq_pkt_create(mtk_crtc->gce_obj.client[CLIENT_CFG]);
	if (!cmdq_handle) {
		DDPPR_ERR("%s:%d NULL cmdq handle\n", __func__, __LINE__);
		return -EFAULT;
	}
	if (mtk_crtc_with_sub_path(crtc, mtk_crtc->ddp_mode))
		mtk_crtc_wait_frame_done(mtk_crtc, cmdq_handle, DDP_SECOND_PATH, 0);
	else
		mtk_crtc_wait_frame_done(mtk_crtc, cmdq_handle, DDP_FIRST_PATH, 0);

	cmdq_pkt_write(cmdq_handle, mtk_crtc->gce_obj.base,
						pa, wParams->val, wParams->mask);

	if (mtk_crtc->is_dual_pipe) {
		unsigned int companion_pa = 0;
		int offset = pa & 0xfff;
		int index_table = mtk_drm_get_table_index(crtc, pa);

		if (index_table >= 0) {
			companion_pa = pq_data->tuning_pa_table[index_table].companion_pa_base;
			if (companion_pa)
				companion_pa += offset;
		}
		if (companion_pa) {
			cmdq_pkt_write(cmdq_handle, mtk_crtc->gce_obj.base,
						companion_pa, wParams->val, wParams->mask);
			if (index_table == TUNING_DISP_COLOR && offset == DISP_COLOR_POS_MAIN) {
				struct mtk_ddp_comp *comp = mtk_ddp_comp_sel_in_cur_crtc_path(
						mtk_crtc, MTK_DISP_COLOR, 0);

				if (comp)
					disp_color_write_pos_main_for_dual_pipe(comp,
						cmdq_handle, wParams, pa, companion_pa);
			}
		}
	}

	cb_data = kmalloc(sizeof(*cb_data), GFP_KERNEL);
	if (!cb_data) {
		DDPPR_ERR("cb data creation failed\n");
		cmdq_pkt_destroy(cmdq_handle);
		return -EFAULT;
	}

	cb_data->crtc = crtc;
	cb_data->cmdq_handle = cmdq_handle;

	DDP_MUTEX_LOCK(&mtk_crtc->lock, __func__, __LINE__);
	if (cmdq_pkt_flush_threaded(cmdq_handle, frame_cmdq_cb, cb_data) < 0) {
		DDPPR_ERR("failed to flush %s\n", __func__);
		kfree(cb_data);
	}
	DDP_MUTEX_UNLOCK(&mtk_crtc->lock, __func__, __LINE__);

	return 0;
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
	case PQ_COLOR_WRITE_REG:
		ret = mtk_drm_ioctl_hw_write_impl(crtc, kdata);
		break;
	case PQ_COLOR_WRITE_SW_REG:
		ret = mtk_drm_ioctl_sw_write_impl(crtc, kdata);
		break;
	case PQ_COLOR_READ_REG:
		ret = mtk_drm_ioctl_hw_read_impl(crtc, kdata);
		break;
	case PQ_COLOR_READ_SW_REG:
		ret = mtk_drm_ioctl_sw_read_impl(crtc, kdata);
		break;
	default:
		DDPPR_ERR("%s, unknown cmd:%d\n", __func__, cmd);
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

	if (!cmds_len || cmds_len > REQUEST_MAX_COUNT || params->data == NULL) {
		DDPPR_ERR("%s:%d, invalid requests for pq config\n",
			__func__, __LINE__);
		mtk_drm_trace_end();

		return -1;
	}

	if (copy_from_user(&requests, params->data, sizeof(struct mtk_drm_pq_param) * cmds_len)) {
		mtk_drm_trace_end();

		return -1;
	}
	if (index) {
		DDPPR_ERR("%s:%d, invalid crtc:0x%p, index:%d\n",
				__func__, __LINE__, crtc, index);
		mtk_drm_trace_end();
		return -1;
	}

	if (!(mtk_crtc->enabled)) {
		DDPINFO("%s:%d, slepted\n", __func__, __LINE__);
		mtk_drm_trace_end();

		return -1;
	}

	if (is_atomic_commit)
		pq_cmdq_handle = cmdq_handle;
	else {
		pq_cmdq_handle = cmdq_pkt_create(mtk_crtc->gce_obj.client[CLIENT_CFG]);
		if (!pq_cmdq_handle) {
			DDPPR_ERR("%s:%d NULL cmdq handle\n", __func__, __LINE__);
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
			cmdq_pkt_destroy(pq_cmdq_handle);
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
			cmdq_pkt_destroy(pq_cmdq_handle);
			kfree(cb_data);
			if (user_lock)
				DDP_MUTEX_UNLOCK(&mtk_crtc->lock, __func__, __LINE__);
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
	DDPINFO("%s:%d --\n", __func__, __LINE__);
	mtk_drm_trace_end();

	return 0;
}

static void mtk_pq_helper_fill_tuning_table(struct mtk_drm_crtc *mtk_crtc,
	int comp_type, int path_order, resource_size_t pa, resource_size_t companion_pa)
{
	struct pq_common_data *pq_data = mtk_crtc->pq_data;
	unsigned int table_index = TUNING_REG_MAX;

	switch (comp_type) {
	case MTK_DISP_COLOR:
		table_index = TUNING_DISP_COLOR;
		break;
	case MTK_DISP_CCORR:
		if (path_order)
			table_index = TUNING_DISP_CCORR1;
		else
			table_index = TUNING_DISP_CCORR;
		break;
	case MTK_DISP_AAL:
		table_index = TUNING_DISP_AAL;
		break;
	case MTK_DISP_GAMMA:
		table_index = TUNING_DISP_GAMMA;
		break;
	case MTK_DISP_DITHER:
		table_index = TUNING_DISP_DITHER;
		break;
	case MTK_DISP_TDSHP:
		table_index = TUNING_DISP_TDSHP;
		break;
	case MTK_DISP_C3D:
		table_index = TUNING_DISP_C3D;
		break;
	case MTK_DMDP_AAL:
		table_index = TUNING_DISP_MDP_AAL;
		break;
	case MTK_DISP_ODDMR:
		table_index = TUNING_DISP_ODDMR_TOP;
		break;
	default:
		DDPPR_ERR("%s, unknown comp_type:%d\n", __func__, comp_type);
	}

	if (table_index < TUNING_REG_MAX) {
		pq_data->tuning_pa_table[table_index].type = comp_type;
		pq_data->tuning_pa_table[table_index].pa_base = pa;
		pq_data->tuning_pa_table[table_index].companion_pa_base = companion_pa;
	}
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
	comp_type = mtk_ddp_comp_get_type(comp->id);
	if (comp_type < 0) {
		DDPPR_ERR("%s comp id %d is invalid\n", __func__, comp->id);
		return comp_type;
	}
	if (comp->mtk_crtc->is_dual_pipe && companion) {
		if (!_is_right_pipe)
			_companion = mtk_ddp_comp_sel_in_dual_pipe(comp->mtk_crtc,
						comp_type, _path_order);
		else
			_companion = mtk_ddp_comp_sel_in_cur_crtc_path(comp->mtk_crtc,
						comp_type, _path_order);
		if (!_companion)
			ret = -1;
		if (_companion)
			*companion = _companion;
		DDPMSG("%s companion %s\n", __func__, mtk_dump_comp_str(_companion));
	}

	if (!_is_right_pipe) {
		resource_size_t companion_regs_pa = 0;

		if (_companion)
			companion_regs_pa = _companion->regs_pa;
		mtk_pq_helper_fill_tuning_table(comp->mtk_crtc, comp_type, _path_order,
					comp->regs_pa, companion_regs_pa);
	}
	return ret;
}

void mtk_disp_pq_on_start_of_frame(struct mtk_drm_crtc *mtk_crtc)
{
	struct pq_common_data *pq_data = mtk_crtc->pq_data;
	struct mtk_ddp_comp *ccorr_comp = mtk_ddp_comp_sel_in_cur_crtc_path(
			mtk_crtc, MTK_DISP_CCORR, 0);
	struct mtk_ddp_comp *c3d_comp = mtk_ddp_comp_sel_in_cur_crtc_path(
			mtk_crtc, MTK_DISP_C3D, 0);
	struct mtk_disp_ccorr *ccorr_data;
	struct mtk_disp_ccorr_primary *ccorr_primary;
	struct mtk_disp_c3d *c3d_data;
	struct mtk_disp_c3d_primary *c3d_primary;

	if (!ccorr_comp || !c3d_comp) {
		DDPINFO("%s, no ccorr or c3d in crtc %d\n",
				__func__, drm_crtc_index(&mtk_crtc->base));
		return;
	}
	ccorr_data = comp_to_ccorr(ccorr_comp);
	ccorr_primary = ccorr_data->primary_data;
	c3d_data = comp_to_c3d(c3d_comp);
	c3d_primary = c3d_data->primary_data;

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

struct drm_crtc *get_crtc_from_connector(int connector_id, struct drm_device *drm_dev)
{
	struct drm_crtc *crtc = NULL;
	struct mtk_ddp_comp *output_comp = NULL;
	unsigned int cur_connector_id = 0;

	if (!drm_dev) {
		DDPPR_ERR("%s: failed to get drm_dev!\n", __func__);
		return NULL;
	}
	drm_for_each_crtc(crtc, drm_dev) {
		output_comp = mtk_ddp_comp_request_output(to_mtk_crtc(crtc));
		if (output_comp == NULL)
			continue;
		mtk_ddp_comp_io_cmd(output_comp, NULL, GET_CONNECTOR_ID, &cur_connector_id);
		if (cur_connector_id == connector_id)
			return crtc;
	}
	return NULL;
}

