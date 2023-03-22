/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2022 MediaTek Inc.
 */

#ifndef _MTK_DISP_PQ_HELPER_H_
#define _MTK_DISP_PQ_HELPER_H_

#include <uapi/drm/mediatek_drm.h>

extern atomic_t g_ccorr_irq_en;
extern atomic_t g_c3d_eventctl;
extern int g_pq_backlight;
extern int g_old_pq_backlight;

int mtk_drm_ioctl_pq_frame_config(struct drm_device *dev, void *data,
	struct drm_file *file_priv);

int mtk_drm_ioctl_pq_proxy(struct drm_device *dev, void *data,
	struct drm_file *file_priv);

int mtk_pq_helper_frame_config(struct drm_crtc *crtc, struct cmdq_pkt *cmdq_handle,
	void *data, bool user_lock);

int mtk_pq_helper_fill_comp_pipe_info(struct mtk_ddp_comp *comp, int *path_order,
	bool *is_right_pipe, struct mtk_ddp_comp **companion);

int mtk_drm_ioctl_pq_get_irq(struct drm_device *dev, void *data, struct drm_file *file_priv);

#endif /* _MTK_DISP_PQ_HELPER_H_ */
