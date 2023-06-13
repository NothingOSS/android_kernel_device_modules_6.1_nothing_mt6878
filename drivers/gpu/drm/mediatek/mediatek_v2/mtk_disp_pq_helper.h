/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2022 MediaTek Inc.
 */

#ifndef _MTK_DISP_PQ_HELPER_H_
#define _MTK_DISP_PQ_HELPER_H_

#include <uapi/drm/mediatek_drm.h>

enum mtk_pq_persist_property {
	DISP_PQ_COLOR_BYPASS,
	DISP_PQ_CCORR_BYPASS,
	DISP_PQ_GAMMA_BYPASS,
	DISP_PQ_DITHER_BYPASS,
	DISP_PQ_AAL_BYPASS,
	DISP_PQ_C3D_BYPASS,
	DISP_PQ_TDSHP_BYPASS,
	DISP_PQ_CCORR_SILKY_BRIGHTNESS,
	DISP_PQ_GAMMA_SILKY_BRIGHTNESS,
	DISP_PQ_DITHER_COLOR_DETECT,
	DISP_CLARITY_SUPPORT,
	DISP_DRE_CAPABILITY,
	DISP_PQ_PROPERTY_MAX,
};

int mtk_drm_ioctl_pq_frame_config(struct drm_device *dev, void *data,
	struct drm_file *file_priv);
int mtk_drm_ioctl_pq_proxy(struct drm_device *dev, void *data,
	struct drm_file *file_priv);
int mtk_pq_helper_frame_config(struct drm_crtc *crtc, struct cmdq_pkt *cmdq_handle,
	void *data, bool user_lock);
int mtk_pq_helper_fill_comp_pipe_info(struct mtk_ddp_comp *comp, int *path_order,
	bool *is_right_pipe, struct mtk_ddp_comp **companion);
int mtk_drm_ioctl_pq_get_irq(struct drm_device *dev, void *data, struct drm_file *file_priv);
void mtk_disp_pq_on_start_of_frame(struct mtk_drm_crtc *mtk_crtc);
int mtk_drm_ioctl_pq_get_persist_property(struct drm_device *dev, void *data,
	struct drm_file *file_priv);
struct drm_crtc *get_crtc_from_connector(int connector_id, struct drm_device *drm_dev);

#endif /* _MTK_DISP_PQ_HELPER_H_ */
