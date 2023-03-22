/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#ifndef __MTK_DISP_GAMMA_H__
#define __MTK_DISP_GAMMA_H__

#include <linux/uaccess.h>
#include <uapi/drm/mediatek_drm.h>


#define GAMMA_ENTRY(r10, g10, b10) (((r10) << 20) | ((g10) << 10) | (b10))

int mtk_drm_ioctl_set_gammalut(struct drm_device *dev, void *data,
	struct drm_file *file_priv);
int mtk_drm_ioctl_set_12bit_gammalut(struct drm_device *dev, void *data,
	struct drm_file *file_priv);
int mtk_drm_ioctl_bypass_disp_gamma(struct drm_device *dev, void *data,
	struct drm_file *file_priv);
int mtk_drm_ioctl_gamma_mul_disable(struct drm_device *dev, void *data,
	struct drm_file *file_priv);

void mtk_trans_gain_to_gamma(struct drm_crtc *crtc,
	unsigned int gain[3], unsigned int bl, void *param);
int mtk_cfg_trans_gain_to_gamma(struct mtk_drm_crtc *mtk_crtc,
	struct cmdq_pkt *handle, unsigned int gain[3], unsigned int bl, void *param);

#endif

