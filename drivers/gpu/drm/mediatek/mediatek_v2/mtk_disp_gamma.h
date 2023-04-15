/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#ifndef __MTK_DISP_GAMMA_H__
#define __MTK_DISP_GAMMA_H__

#include <linux/uaccess.h>
#include <uapi/drm/mediatek_drm.h>
struct gamma_color_protect {
	unsigned int gamma_color_protect_support;
	unsigned int gamma_color_protect_lsb;
};

struct gamma_color_protect_mode {
	unsigned int red_support;
	unsigned int green_support;
	unsigned int blue_support;
	unsigned int black_support;
	unsigned int white_support;
};

/* TODO */
/* static ddp_module_notify g_gamma_ddp_notify; */

enum GAMMA_USER_CMD {
	SET_GAMMALUT = 0,
	SET_12BIT_GAMMALUT,
	BYPASS_GAMMA,
	SET_GAMMAGAIN,
	DISABLE_MUL_EN
};

enum GAMMA_MODE {
	HW_8BIT = 0,
	HW_12BIT_MODE_8BIT,
	HW_12BIT_MODE_12BIT,
};

struct mtk_disp_gamma_data {
	bool support_gammagain;
};

struct mtk_disp_gamma_tile_overhead {
	unsigned int width;
	unsigned int comp_overhead;
};

struct mtk_disp_gamma_sb_param {
	unsigned int gain[3];
	unsigned int bl;
};

struct mtk_disp_gamma_primary {
	struct mtk_disp_gamma_sb_param sb_param;
	struct gamma_color_protect color_protect;
	struct DISP_GAMMA_LUT_T *gamma_lut;
	struct DISP_GAMMA_12BIT_LUT_T *gamma_12bit_lut;
	struct DISP_GAMMA_LUT_T gamma_lut_db;
	struct DISP_GAMMA_12BIT_LUT_T gamma_12bit_lut_db;
	struct DISP_GAMMA_12BIT_LUT_T ioctl_data;

	atomic_t irq_event;
	struct wait_queue_head sof_irq_wq;
	struct task_struct *sof_irq_event_task;

	spinlock_t power_lock;
	struct mutex global_lock;
	struct mutex sram_lock;

	atomic_t clock_on;
	atomic_t sof_filp;
	atomic_t sof_irq_available;
	atomic_t force_delay_check_trig;

	unsigned int back_up_cfg;
	unsigned int relay_value;
	unsigned int data_mode;
};

struct mtk_disp_gamma {
	struct mtk_ddp_comp ddp_comp;
	const struct mtk_disp_gamma_data *data;
	bool is_right_pipe;
	int path_order;
	struct mtk_disp_gamma_tile_overhead tile_overhead;
	struct mtk_ddp_comp *companion;
	struct mtk_disp_gamma_primary *primary_data;
};

static inline struct mtk_disp_gamma *comp_to_gamma(struct mtk_ddp_comp *comp)
{
	return container_of(comp, struct mtk_disp_gamma, ddp_comp);
}


#define GAMMA_ENTRY(r10, g10, b10) (((r10) << 20) | ((g10) << 10) | (b10))

int mtk_drm_ioctl_set_gammalut(struct drm_device *dev, void *data,
	struct drm_file *file_priv);
int mtk_drm_ioctl_set_12bit_gammalut(struct drm_device *dev, void *data,
	struct drm_file *file_priv);
int mtk_drm_ioctl_bypass_disp_gamma(struct drm_device *dev, void *data,
	struct drm_file *file_priv);
int mtk_drm_ioctl_gamma_mul_disable(struct drm_device *dev, void *data,
	struct drm_file *file_priv);
void mtk_trans_gain_to_gamma(struct mtk_ddp_comp *comp,
	unsigned int gain[3], unsigned int bl, void *param);
int mtk_cfg_trans_gain_to_gamma(struct mtk_drm_crtc *mtk_crtc,
	struct cmdq_pkt *handle, unsigned int gain[3], unsigned int bl, void *param);
void disp_gamma_set_bypass(struct drm_crtc *crtc, int bypass);
void mtk_gamma_regdump(struct mtk_ddp_comp *comp);

#endif

