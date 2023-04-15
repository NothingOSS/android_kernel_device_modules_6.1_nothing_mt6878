// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#include <linux/clk.h>
#include <linux/component.h>
#include <linux/of_device.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>
#include <linux/delay.h>

#ifndef DRM_CMDQ_DISABLE
#include <linux/soc/mediatek/mtk-cmdq-ext.h>
#else
#include "mtk-cmdq-ext.h"
#endif

#include "mtk_drm_crtc.h"
#include "mtk_drm_ddp_comp.h"
#include "mtk_drm_drv.h"
#include "mtk_disp_color.h"
#include "mtk_dump.h"
#include "platform/mtk_drm_platform.h"
#include "mtk_disp_ccorr.h"
#include "mtk_disp_pq_helper.h"

#define UNUSED(expr) (void)(expr)
#define PQ_MODULE_NUM 9

#define CCORR_REG(idx) (idx * 4 + 0x80)

#define C1_OFFSET (0)
#define color_get_offset(module) (0)
#define is_color1_module(module) (0)

enum COLOR_USER_CMD {
	SET_PQPARAM = 0,
	SET_COLOR_REG,
	WRITE_REG,
	BYPASS_COLOR,
	PQ_SET_WINDOW,
};

#if defined(DISP_COLOR_ON)
#define COLOR_MODE			(1)
#elif defined(MDP_COLOR_ON)
#define COLOR_MODE			(2)
#elif defined(DISP_MDP_COLOR_ON)
#define COLOR_MODE			(3)
#else
#define COLOR_MODE			(0)	/*color feature off */
#endif

struct mtk_disp_color_tile_overhead {
	unsigned int left_in_width;
	unsigned int left_overhead;
	unsigned int left_comp_overhead;
	unsigned int right_in_width;
	unsigned int right_overhead;
	unsigned int right_comp_overhead;
};

struct color_backup {
	unsigned int COLOR_CFG_MAIN;
};

struct mtk_disp_color_primary {
	struct DISP_PQ_PARAM color_param;
	int ncs_tuning_mode;
	unsigned int split_en;
	unsigned int split_window_x_start;
	unsigned int split_window_y_start;
	unsigned int split_window_x_end;
	unsigned int split_window_y_end;
	int color_bypass;
	struct DISPLAY_COLOR_REG color_reg;
	int color_reg_valid;
	unsigned int width;
	//for DISP_COLOR_TUNING
	bool legacy_color_cust;
	struct MDP_COLOR_CAP mdp_color_cap;
	struct DISP_PQ_DC_PARAM pq_dc_param;
	struct DISP_PQ_DS_PARAM pq_ds_param;
	int tdshp_flag;	/* 0: normal, 1: tuning mode */
	struct MDP_TDSHP_REG tdshp_reg;
	struct mtk_disp_color_tile_overhead tile_overhead;
	struct DISPLAY_PQ_T color_index;
	struct color_backup color_backup;
	struct DISP_AAL_DRECOLOR_PARAM drecolor_sgy;
	struct mutex reg_lock;
	struct mtk_ddp_comp *gamma_comp;
	struct mtk_ddp_comp *aal_comp;
	struct mtk_ddp_comp *tdshp_comp;
	struct mtk_ddp_comp *ccorr_comp;
};

/**
 * struct mtk_disp_color - DISP_COLOR driver structure
 * @ddp_comp - structure containing type enum and hardware resources
 * @crtc - associated crtc to report irq events to
 */
struct mtk_disp_color {
	struct mtk_ddp_comp ddp_comp;
	struct drm_crtc *crtc;
	const struct mtk_disp_color_data *data;
	bool is_right_pipe;
	int path_order;
	struct mtk_ddp_comp *companion;
	struct mtk_disp_color_primary *primary_data;
	unsigned long color_dst_w;
	unsigned long color_dst_h;
	atomic_t color_is_clock_on;
	spinlock_t clock_lock;
};

static inline struct mtk_disp_color *comp_to_color(struct mtk_ddp_comp *comp)
{
	return container_of(comp, struct mtk_disp_color, ddp_comp);
}

static void ddp_color_cal_split_window(struct mtk_ddp_comp *comp,
	unsigned int *p_split_window_x, unsigned int *p_split_window_y)
{
	unsigned int split_window_x = 0xFFFF0000;
	unsigned int split_window_y = 0xFFFF0000;
	struct mtk_disp_color *color = comp_to_color(comp);
	struct mtk_disp_color_primary *primary =
		color->primary_data;

	/* save to global, can be applied on following PQ param updating. */
	if (comp->mtk_crtc->is_dual_pipe) {
		if (color->color_dst_w == 0 || color->color_dst_h == 0) {
			DDPINFO("color_dst_w/h not init, return default settings\n");
		} else if (primary->split_en) {
			/* TODO: CONFIG_MTK_LCM_PHYSICAL_ROTATION other case */
			if (!color->is_right_pipe) {
				if (primary->split_window_x_start > color->color_dst_w)
					primary->split_en = 0;
				if (primary->split_window_x_start <= color->color_dst_w) {
					if (primary->split_window_x_end >= color->color_dst_w)
						split_window_x = (color->color_dst_w << 16) |
							primary->split_window_x_start;
					else
						split_window_x =
							(primary->split_window_x_end << 16) |
							primary->split_window_x_start;
					split_window_y = (primary->split_window_y_end << 16) |
						primary->split_window_y_start;
				}
			} else {
				if (primary->split_window_x_start > color->color_dst_w) {
					split_window_x =
					    ((primary->split_window_x_end - color->color_dst_w)
					     << 16) |
					    (primary->split_window_x_start - color->color_dst_w);
				} else if (primary->split_window_x_start <= color->color_dst_w &&
						primary->split_window_x_end > color->color_dst_w){
					split_window_x = ((primary->split_window_x_end -
								color->color_dst_w) << 16) | 0;
				}
				split_window_y =
				    (primary->split_window_y_end << 16) |
				    primary->split_window_y_start;

				if (primary->split_window_x_end <= color->color_dst_w)
					primary->split_en = 0;
			}
		}
	} else if (color->color_dst_w == 0 || color->color_dst_h == 0) {
		DDPINFO("g_color0_dst_w/h not init, return default settings\n");
	} else if (primary->split_en) {
		/* TODO: CONFIG_MTK_LCM_PHYSICAL_ROTATION other case */
		split_window_y =
		    (primary->split_window_y_end << 16) | primary->split_window_y_start;
		split_window_x = (primary->split_window_x_end << 16) |
			primary->split_window_x_start;
	}

	*p_split_window_x = split_window_x;
	*p_split_window_y = split_window_y;
}

bool disp_color_reg_get(struct mtk_ddp_comp *comp,
	unsigned long addr, int *value)
{
	struct mtk_disp_color *color_data = comp_to_color(comp);
	unsigned long flags;

	DDPDBG("%s @ %d......... spin_trylock_irqsave ++ ",
		__func__, __LINE__);
	if (spin_trylock_irqsave(&color_data->clock_lock, flags)) {
		DDPDBG("%s @ %d......... spin_trylock_irqsave -- ",
			__func__, __LINE__);
		*value = readl(comp->regs + addr);
		spin_unlock_irqrestore(&color_data->clock_lock, flags);
	} else {
		DDPINFO("%s @ %d......... Failed to spin_trylock_irqsave ",
			__func__, __LINE__);
	}

	return true;
}

static void ddp_color_set_window(struct mtk_ddp_comp *comp,
	struct DISP_PQ_WIN_PARAM *win_param, struct cmdq_pkt *handle)
{
	unsigned int split_window_x, split_window_y;
	struct mtk_disp_color_primary *primary_data =
		comp_to_color(comp)->primary_data;

	/* save to global, can be applied on following PQ param updating. */
	if (win_param->split_en) {
		primary_data->split_en = 1;
		primary_data->split_window_x_start = win_param->start_x;
		primary_data->split_window_y_start = win_param->start_y;
		primary_data->split_window_x_end = win_param->end_x;
		primary_data->split_window_y_end = win_param->end_y;
	} else {
		primary_data->split_en = 0;
		primary_data->split_window_x_start = 0x0000;
		primary_data->split_window_y_start = 0x0000;
		primary_data->split_window_x_end = 0xFFFF;
		primary_data->split_window_y_end = 0xFFFF;
	}

	DDPINFO("%s: input: id[%d], en[%d], x[0x%x], y[0x%x]\n",
		__func__, comp->id, primary_data->split_en,
		((win_param->end_x << 16) | win_param->start_x),
		((win_param->end_y << 16) | win_param->start_y));

	ddp_color_cal_split_window(comp, &split_window_x, &split_window_y);

	DDPINFO("%s: current window setting: en[%d], x[0x%x], y[0x%x]",
		__func__,
		(readl(comp->regs+DISP_COLOR_DBG_CFG_MAIN)&0x00000008)>>3,
		readl(comp->regs+DISP_COLOR_WIN_X_MAIN),
		readl(comp->regs+DISP_COLOR_WIN_Y_MAIN));

	DDPINFO("%s: output: x[0x%x], y[0x%x]",
		__func__, split_window_x, split_window_y);

	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + DISP_COLOR_DBG_CFG_MAIN,
		(primary_data->split_en << 3), 0x00000008);
	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + DISP_COLOR_WIN_X_MAIN, split_window_x, ~0);
	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + DISP_COLOR_WIN_Y_MAIN, split_window_y, ~0);
}

struct DISPLAY_PQ_T *get_Color_index(struct mtk_ddp_comp *comp)
{
	struct mtk_disp_color_primary *primary_data =
		comp_to_color(comp)->primary_data;

	primary_data->legacy_color_cust = true;
	return &primary_data->color_index;
}

void DpEngine_COLORonInit(struct mtk_ddp_comp *comp, struct cmdq_pkt *handle)
{
	unsigned int split_window_x, split_window_y;
	struct mtk_disp_color *color = comp_to_color(comp);
	struct mtk_disp_color_primary *primary_data = color->primary_data;

	ddp_color_cal_split_window(comp, &split_window_x, &split_window_y);

	DDPINFO("%s: id[%d], en[%d], x[0x%x], y[0x%x]\n",
		__func__, comp->id, primary_data->split_en, split_window_x, split_window_y);

	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + DISP_COLOR_DBG_CFG_MAIN,
		(primary_data->split_en << 3), 0x00000008);
	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + DISP_COLOR_WIN_X_MAIN, split_window_x, ~0);
	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + DISP_COLOR_WIN_Y_MAIN, split_window_y, ~0);

	/* enable interrupt */
	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + DISP_COLOR_INTEN(color),
		0x00000007, 0x00000007);

	/* Set 10bit->8bit Rounding */
	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + DISP_COLOR_OUT_SEL(color), 0x333, 0x00000333);
}

void DpEngine_COLORonConfig(struct mtk_ddp_comp *comp, struct cmdq_pkt *handle)
{
	int index = 0;
	unsigned int u4Temp = 0;
	unsigned int u4SatAdjPurp, u4SatAdjSkin, u4SatAdjGrass, u4SatAdjSky;
	unsigned char h_series[20] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
			0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
	struct mtk_disp_color_primary *primary_data =
		comp_to_color(comp)->primary_data;

	struct mtk_disp_color *color = comp_to_color(comp);
	struct DISP_PQ_PARAM *pq_param_p = &primary_data->color_param;
	struct pq_common_data *pq_data = comp->mtk_crtc->pq_data;
	int i, j, reg_index;
	unsigned int pq_index;
	int wide_gamut_en = 0;
	/* mask s_gain_by_y when drecolor enable */
	int s_gain_by_y = !(pq_data->new_persist_property[DISP_DRE_CAPABILITY] & 0x1);

	if (pq_param_p->u4Brightness >= BRIGHTNESS_SIZE ||
		pq_param_p->u4Contrast >= CONTRAST_SIZE ||
		pq_param_p->u4SatGain >= GLOBAL_SAT_SIZE ||
		pq_param_p->u4HueAdj[PURP_TONE] >= COLOR_TUNING_INDEX ||
		pq_param_p->u4HueAdj[SKIN_TONE] >= COLOR_TUNING_INDEX ||
		pq_param_p->u4HueAdj[GRASS_TONE] >= COLOR_TUNING_INDEX ||
		pq_param_p->u4HueAdj[SKY_TONE] >= COLOR_TUNING_INDEX ||
		pq_param_p->u4SatAdj[PURP_TONE] >= COLOR_TUNING_INDEX ||
		pq_param_p->u4SatAdj[SKIN_TONE] >= COLOR_TUNING_INDEX ||
		pq_param_p->u4SatAdj[GRASS_TONE] >= COLOR_TUNING_INDEX ||
		pq_param_p->u4SatAdj[SKY_TONE] >= COLOR_TUNING_INDEX ||
		pq_param_p->u4ColorLUT >= COLOR_3D_CNT) {
		DRM_ERROR("[PQ][COLOR] Tuning index range error !\n");
		return;
	}

	if (primary_data->color_bypass == 0) {
		if (color->data->support_color21 == true) {
			cmdq_pkt_write(handle, comp->cmdq_base,
				comp->regs_pa + DISP_COLOR_CFG_MAIN,
				(1 << 21)
				| (primary_data->color_index.LSP_EN << 20)
				| (primary_data->color_index.S_GAIN_BY_Y_EN << 15)
				| (wide_gamut_en << 8)
				| (0 << 7),
				0x003001FF | s_gain_by_y << 15);
		} else {
			/* disable wide_gamut */
			cmdq_pkt_write(handle, comp->cmdq_base,
				comp->regs_pa + DISP_COLOR_CFG_MAIN,
				(0 << 8) | (0 << 7), 0x00001FF);
		}

		/* color start */
		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DISP_COLOR_START(color), 0x1, 0x3);

		/* enable R2Y/Y2R in Color Wrapper */
		if (color->data->support_color21 == true) {
			/* RDMA & OVL will enable wide-gamut function */
			/* disable rgb clipping function in CM1 */
			/* to keep the wide-gamut range */
			cmdq_pkt_write(handle, comp->cmdq_base,
				comp->regs_pa + DISP_COLOR_CM1_EN(color),
				0x03, 0x03);
		} else {
			cmdq_pkt_write(handle, comp->cmdq_base,
				comp->regs_pa + DISP_COLOR_CM1_EN(color),
				0x03, 0x03);
		}

		/* also set no rounding on Y2R */
		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DISP_COLOR_CM2_EN(color), 0x01, 0x01);
	} else {
		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DISP_COLOR_CFG_MAIN,
			(0x1 << 29), 0x20000000);
		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DISP_COLOR_START(color), 0x1, 0x1);
	}

	/* for partial Y contour issue */
	if (wide_gamut_en == 0)
		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DISP_COLOR_LUMA_ADJ, 0x40, 0x7F);
	else if (wide_gamut_en == 1)
		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DISP_COLOR_LUMA_ADJ, 0x0, 0x7F);

	/* config parameter from customer color_index.h */
	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + DISP_COLOR_G_PIC_ADJ_MAIN_1,
		(primary_data->color_index.BRIGHTNESS[pq_param_p->u4Brightness] << 16) |
		primary_data->color_index.CONTRAST[pq_param_p->u4Contrast], 0x07FF03FF);
	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + DISP_COLOR_G_PIC_ADJ_MAIN_2,
		primary_data->color_index.GLOBAL_SAT[pq_param_p->u4SatGain], 0x000003FF);

	/* Partial Y Function */
	for (index = 0; index < 8; index++) {
		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DISP_COLOR_Y_SLOPE_1_0_MAIN + 4 * index,
			(primary_data->color_index.PARTIAL_Y
				[pq_param_p->u4PartialY][2 * index] |
			 primary_data->color_index.PARTIAL_Y
				[pq_param_p->u4PartialY][2 * index + 1]
			 << 16), 0x00FF00FF);
	}

	if (color->data->support_color21 == false)
		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DISP_COLOR_C_BOOST_MAIN,
			0 << 13, 0x00002000);
	else
		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DISP_COLOR_C_BOOST_MAIN_2,
			0x40 << 24,	0xFF000000);

	/* Partial Saturation Function */
	u4SatAdjPurp = pq_param_p->u4SatAdj[PURP_TONE];
	u4SatAdjSkin = pq_param_p->u4SatAdj[SKIN_TONE];
	u4SatAdjGrass = pq_param_p->u4SatAdj[GRASS_TONE];
	u4SatAdjSky = pq_param_p->u4SatAdj[SKY_TONE];

	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + DISP_COLOR_PART_SAT_GAIN1_0,
		(primary_data->color_index.PURP_TONE_S[u4SatAdjPurp][SG1][0] |
		primary_data->color_index.PURP_TONE_S[u4SatAdjPurp][SG1][1] << 8 |
		primary_data->color_index.PURP_TONE_S[u4SatAdjPurp][SG1][2] << 16 |
		primary_data->color_index.SKIN_TONE_S[u4SatAdjSkin][SG1][0] << 24), ~0);

	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + DISP_COLOR_PART_SAT_GAIN1_1,
		(primary_data->color_index.SKIN_TONE_S[u4SatAdjSkin][SG1][1] |
		primary_data->color_index.SKIN_TONE_S[u4SatAdjSkin][SG1][2] << 8 |
		primary_data->color_index.SKIN_TONE_S[u4SatAdjSkin][SG1][3] << 16 |
		primary_data->color_index.SKIN_TONE_S[u4SatAdjSkin][SG1][4] << 24), ~0);

	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + DISP_COLOR_PART_SAT_GAIN1_2,
		(primary_data->color_index.SKIN_TONE_S[u4SatAdjSkin][SG1][5] |
		primary_data->color_index.SKIN_TONE_S[u4SatAdjSkin][SG1][6] << 8 |
		primary_data->color_index.SKIN_TONE_S[u4SatAdjSkin][SG1][7] << 16 |
		primary_data->color_index.GRASS_TONE_S[u4SatAdjGrass][SG1][0] << 24), ~0);

	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + DISP_COLOR_PART_SAT_GAIN1_3,
		(primary_data->color_index.GRASS_TONE_S[u4SatAdjGrass][SG1][1] |
		primary_data->color_index.GRASS_TONE_S[u4SatAdjGrass][SG1][2] << 8 |
		primary_data->color_index.GRASS_TONE_S[u4SatAdjGrass][SG1][3] << 16 |
		primary_data->color_index.GRASS_TONE_S[u4SatAdjGrass][SG1][4] << 24), ~0);

	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + DISP_COLOR_PART_SAT_GAIN1_4,
		(primary_data->color_index.GRASS_TONE_S[u4SatAdjGrass][SG1][5] |
		primary_data->color_index.SKY_TONE_S[u4SatAdjSky][SG1][0] << 8 |
		primary_data->color_index.SKY_TONE_S[u4SatAdjSky][SG1][1] << 16 |
		primary_data->color_index.SKY_TONE_S[u4SatAdjSky][SG1][2] << 24), ~0);

	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + DISP_COLOR_PART_SAT_GAIN2_0,
		(primary_data->color_index.PURP_TONE_S[u4SatAdjPurp][SG2][0] |
		primary_data->color_index.PURP_TONE_S[u4SatAdjPurp][SG2][1] << 8 |
		primary_data->color_index.PURP_TONE_S[u4SatAdjPurp][SG2][2] << 16 |
		primary_data->color_index.SKIN_TONE_S[u4SatAdjSkin][SG2][0] << 24), ~0);

	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + DISP_COLOR_PART_SAT_GAIN2_1,
		(primary_data->color_index.SKIN_TONE_S[u4SatAdjSkin][SG2][1] |
		primary_data->color_index.SKIN_TONE_S[u4SatAdjSkin][SG2][2] << 8 |
		primary_data->color_index.SKIN_TONE_S[u4SatAdjSkin][SG2][3] << 16 |
		primary_data->color_index.SKIN_TONE_S[u4SatAdjSkin][SG2][4] << 24), ~0);

	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + DISP_COLOR_PART_SAT_GAIN2_2,
		(primary_data->color_index.SKIN_TONE_S[u4SatAdjSkin][SG2][5] |
		primary_data->color_index.SKIN_TONE_S[u4SatAdjSkin][SG2][6] << 8 |
		primary_data->color_index.SKIN_TONE_S[u4SatAdjSkin][SG2][7] << 16 |
		primary_data->color_index.GRASS_TONE_S[u4SatAdjGrass][SG2][0] << 24), ~0);

	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + DISP_COLOR_PART_SAT_GAIN2_3,
		(primary_data->color_index.GRASS_TONE_S[u4SatAdjGrass][SG2][1] |
		primary_data->color_index.GRASS_TONE_S[u4SatAdjGrass][SG2][2] << 8 |
		primary_data->color_index.GRASS_TONE_S[u4SatAdjGrass][SG2][3] << 16 |
		primary_data->color_index.GRASS_TONE_S[u4SatAdjGrass][SG2][4] << 24), ~0);

	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + DISP_COLOR_PART_SAT_GAIN2_4,
		(primary_data->color_index.GRASS_TONE_S[u4SatAdjGrass][SG2][5] |
		primary_data->color_index.SKY_TONE_S[u4SatAdjSky][SG2][0] << 8 |
		primary_data->color_index.SKY_TONE_S[u4SatAdjSky][SG2][1] << 16 |
		primary_data->color_index.SKY_TONE_S[u4SatAdjSky][SG2][2] << 24), ~0);

	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + DISP_COLOR_PART_SAT_GAIN3_0,
		(primary_data->color_index.PURP_TONE_S[u4SatAdjPurp][SG3][0] |
		primary_data->color_index.PURP_TONE_S[u4SatAdjPurp][SG3][1] << 8 |
		primary_data->color_index.PURP_TONE_S[u4SatAdjPurp][SG3][2] << 16 |
		primary_data->color_index.SKIN_TONE_S[u4SatAdjSkin][SG3][0] << 24), ~0);

	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + DISP_COLOR_PART_SAT_GAIN3_1,
		(primary_data->color_index.SKIN_TONE_S[u4SatAdjSkin][SG3][1] |
		primary_data->color_index.SKIN_TONE_S[u4SatAdjSkin][SG3][2] << 8 |
		primary_data->color_index.SKIN_TONE_S[u4SatAdjSkin][SG3][3] << 16 |
		primary_data->color_index.SKIN_TONE_S[u4SatAdjSkin][SG3][4] << 24), ~0);

	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + DISP_COLOR_PART_SAT_GAIN3_2,
		(primary_data->color_index.SKIN_TONE_S[u4SatAdjSkin][SG3][5] |
		primary_data->color_index.SKIN_TONE_S[u4SatAdjSkin][SG3][6] << 8 |
		primary_data->color_index.SKIN_TONE_S[u4SatAdjSkin][SG3][7] << 16 |
		primary_data->color_index.GRASS_TONE_S[u4SatAdjGrass][SG3][0] << 24), ~0);

	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + DISP_COLOR_PART_SAT_GAIN3_3,
		(primary_data->color_index.GRASS_TONE_S[u4SatAdjGrass][SG3][1] |
		primary_data->color_index.GRASS_TONE_S[u4SatAdjGrass][SG3][2] << 8 |
		primary_data->color_index.GRASS_TONE_S[u4SatAdjGrass][SG3][3] << 16 |
		primary_data->color_index.GRASS_TONE_S[u4SatAdjGrass][SG3][4] << 24), ~0);

	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + DISP_COLOR_PART_SAT_GAIN3_4,
		(primary_data->color_index.GRASS_TONE_S[u4SatAdjGrass][SG3][5] |
		primary_data->color_index.SKY_TONE_S[u4SatAdjSky][SG3][0] << 8 |
		primary_data->color_index.SKY_TONE_S[u4SatAdjSky][SG3][1] << 16 |
		primary_data->color_index.SKY_TONE_S[u4SatAdjSky][SG3][2] << 24), ~0);

	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + DISP_COLOR_PART_SAT_POINT1_0,
		(primary_data->color_index.PURP_TONE_S[u4SatAdjPurp][SP1][0] |
		primary_data->color_index.PURP_TONE_S[u4SatAdjPurp][SP1][1] << 8 |
		primary_data->color_index.PURP_TONE_S[u4SatAdjPurp][SP1][2] << 16 |
		primary_data->color_index.SKIN_TONE_S[u4SatAdjSkin][SP1][0] << 24), ~0);

	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + DISP_COLOR_PART_SAT_POINT1_1,
		(primary_data->color_index.SKIN_TONE_S[u4SatAdjSkin][SP1][1] |
		primary_data->color_index.SKIN_TONE_S[u4SatAdjSkin][SP1][2] << 8 |
		primary_data->color_index.SKIN_TONE_S[u4SatAdjSkin][SP1][3] << 16 |
		primary_data->color_index.SKIN_TONE_S[u4SatAdjSkin][SP1][4] << 24), ~0);

	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + DISP_COLOR_PART_SAT_POINT1_2,
		(primary_data->color_index.SKIN_TONE_S[u4SatAdjSkin][SP1][5] |
		primary_data->color_index.SKIN_TONE_S[u4SatAdjSkin][SP1][6] << 8 |
		primary_data->color_index.SKIN_TONE_S[u4SatAdjSkin][SP1][7] << 16 |
		primary_data->color_index.GRASS_TONE_S[u4SatAdjGrass][SP1][0] << 24), ~0);

	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + DISP_COLOR_PART_SAT_POINT1_3,
		(primary_data->color_index.GRASS_TONE_S[u4SatAdjGrass][SP1][1] |
		primary_data->color_index.GRASS_TONE_S[u4SatAdjGrass][SP1][2] << 8 |
		primary_data->color_index.GRASS_TONE_S[u4SatAdjGrass][SP1][3] << 16 |
		primary_data->color_index.GRASS_TONE_S[u4SatAdjGrass][SP1][4] << 24), ~0);

	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + DISP_COLOR_PART_SAT_POINT1_4,
		(primary_data->color_index.GRASS_TONE_S[u4SatAdjGrass][SP1][5] |
		primary_data->color_index.SKY_TONE_S[u4SatAdjSky][SP1][0] << 8 |
		primary_data->color_index.SKY_TONE_S[u4SatAdjSky][SP1][1] << 16 |
		primary_data->color_index.SKY_TONE_S[u4SatAdjSky][SP1][2] << 24), ~0);

	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + DISP_COLOR_PART_SAT_POINT2_0,
		(primary_data->color_index.PURP_TONE_S[u4SatAdjPurp][SP2][0] |
		primary_data->color_index.PURP_TONE_S[u4SatAdjPurp][SP2][1] << 8 |
		primary_data->color_index.PURP_TONE_S[u4SatAdjPurp][SP2][2] << 16 |
		primary_data->color_index.SKIN_TONE_S[u4SatAdjSkin][SP2][0] << 24), ~0);

	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + DISP_COLOR_PART_SAT_POINT2_1,
		(primary_data->color_index.SKIN_TONE_S[u4SatAdjSkin][SP2][1] |
		primary_data->color_index.SKIN_TONE_S[u4SatAdjSkin][SP2][2] << 8 |
		primary_data->color_index.SKIN_TONE_S[u4SatAdjSkin][SP2][3] << 16 |
		primary_data->color_index.SKIN_TONE_S[u4SatAdjSkin][SP2][4] << 24), ~0);

	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + DISP_COLOR_PART_SAT_POINT2_2,
		(primary_data->color_index.SKIN_TONE_S[u4SatAdjSkin][SP2][5] |
		primary_data->color_index.SKIN_TONE_S[u4SatAdjSkin][SP2][6] << 8 |
		primary_data->color_index.SKIN_TONE_S[u4SatAdjSkin][SP2][7] << 16 |
		primary_data->color_index.GRASS_TONE_S[u4SatAdjGrass][SP2][0] << 24), ~0);

	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + DISP_COLOR_PART_SAT_POINT2_3,
		(primary_data->color_index.GRASS_TONE_S[u4SatAdjGrass][SP2][1] |
		primary_data->color_index.GRASS_TONE_S[u4SatAdjGrass][SP2][2] << 8 |
		primary_data->color_index.GRASS_TONE_S[u4SatAdjGrass][SP2][3] << 16 |
		primary_data->color_index.GRASS_TONE_S[u4SatAdjGrass][SP2][4] << 24), ~0);


	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + DISP_COLOR_PART_SAT_POINT2_4,
		(primary_data->color_index.GRASS_TONE_S[u4SatAdjGrass][SP2][5] |
		primary_data->color_index.SKY_TONE_S[u4SatAdjSky][SP2][0] << 8 |
		primary_data->color_index.SKY_TONE_S[u4SatAdjSky][SP2][1] << 16 |
		primary_data->color_index.SKY_TONE_S[u4SatAdjSky][SP2][2] << 24), ~0);

	for (index = 0; index < 3; index++) {
		u4Temp = pq_param_p->u4HueAdj[PURP_TONE];
		h_series[index + PURP_TONE_START] =
			primary_data->color_index.PURP_TONE_H[u4Temp][index];
	}

	for (index = 0; index < 8; index++) {
		u4Temp = pq_param_p->u4HueAdj[SKIN_TONE];
		h_series[index + SKIN_TONE_START] =
		    primary_data->color_index.SKIN_TONE_H[u4Temp][index];
	}

	for (index = 0; index < 6; index++) {
		u4Temp = pq_param_p->u4HueAdj[GRASS_TONE];
		h_series[index + GRASS_TONE_START] =
			primary_data->color_index.GRASS_TONE_H[u4Temp][index];
	}

	for (index = 0; index < 3; index++) {
		u4Temp = pq_param_p->u4HueAdj[SKY_TONE];
		h_series[index + SKY_TONE_START] =
		    primary_data->color_index.SKY_TONE_H[u4Temp][index];
	}

	for (index = 0; index < 5; index++) {
		u4Temp = (h_series[4 * index]) +
		    (h_series[4 * index + 1] << 8) +
		    (h_series[4 * index + 2] << 16) +
		    (h_series[4 * index + 3] << 24);
		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DISP_COLOR_LOCAL_HUE_CD_0 + 4 * index,
			u4Temp, ~0);
	}

	if (color->data->support_color21 == true) {
		/* S Gain By Y */
		u4Temp = 0;

		reg_index = 0;
		for (i = 0; i < S_GAIN_BY_Y_CONTROL_CNT && s_gain_by_y; i++) {
			for (j = 0; j < S_GAIN_BY_Y_HUE_PHASE_CNT; j += 4) {
				u4Temp = (primary_data->color_index.S_GAIN_BY_Y[i][j]) +
					(primary_data->color_index.S_GAIN_BY_Y[i][j + 1]
					<< 8) +
					(primary_data->color_index.S_GAIN_BY_Y[i][j + 2]
					<< 16) +
					(primary_data->color_index.S_GAIN_BY_Y[i][j + 3]
					<< 24);

				cmdq_pkt_write(handle, comp->cmdq_base,
					comp->regs_pa +
					DISP_COLOR_S_GAIN_BY_Y0_0 + reg_index,
					u4Temp, ~0);
				reg_index += 4;
			}
		}
		/* LSP */
		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DISP_COLOR_LSP_1,
			(primary_data->color_index.LSP[3] << 0) |
			(primary_data->color_index.LSP[2] << 7) |
			(primary_data->color_index.LSP[1] << 14) |
			(primary_data->color_index.LSP[0] << 22), 0x1FFFFFFF);
		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DISP_COLOR_LSP_2,
			(primary_data->color_index.LSP[7] << 0) |
			(primary_data->color_index.LSP[6] << 8) |
			(primary_data->color_index.LSP[5] << 16) |
			(primary_data->color_index.LSP[4] << 23), 0x3FFF7F7F);
	}

	/* color window */
	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + DISP_COLOR_TWO_D_WINDOW_1,
		color->data->color_window, ~0);

	if (color->data->support_color30 == true) {
		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DISP_COLOR_CM_CONTROL,
			0x0 |
			(0x3 << 1) |	/* enable window 1 */
			(0x3 << 4) |	/* enable window 2 */
			(0x3 << 7)		/* enable window 3 */
			, 0x1B7);

		pq_index = pq_param_p->u4ColorLUT;
		for (i = 0; i < WIN_TOTAL; i++) {
			reg_index = i * 4 * (LUT_TOTAL * 5);
			for (j = 0; j < LUT_TOTAL; j++) {
				cmdq_pkt_write(handle, comp->cmdq_base,
					comp->regs_pa +
						DISP_COLOR_CM_W1_HUE_0 +
						reg_index,
					primary_data->color_index.COLOR_3D[pq_index]
					[i][j*LUT_REG_TOTAL+REG_L] |
					(primary_data->color_index.COLOR_3D[pq_index]
					[i][j*LUT_REG_TOTAL+REG_U] << 10) |
					(primary_data->color_index.COLOR_3D[pq_index]
					[i][j*LUT_REG_TOTAL+REG_POINT0] << 20),
						~0);
				cmdq_pkt_write(handle, comp->cmdq_base,
					comp->regs_pa + DISP_COLOR_CM_W1_HUE_1
						+ reg_index,
					primary_data->color_index.COLOR_3D[pq_index]
					[i][j*LUT_REG_TOTAL+REG_POINT1] |
					(primary_data->color_index.COLOR_3D[pq_index]
					[i][j*LUT_REG_TOTAL+REG_POINT2] << 10) |
					(primary_data->color_index.COLOR_3D[pq_index]
					[i][j*LUT_REG_TOTAL+REG_POINT3] << 20),
					~0);
				cmdq_pkt_write(handle, comp->cmdq_base,
					comp->regs_pa + DISP_COLOR_CM_W1_HUE_2
						+ reg_index,
					primary_data->color_index.COLOR_3D[pq_index]
					[i][j*LUT_REG_TOTAL+REG_POINT4] |
					(primary_data->color_index.COLOR_3D[pq_index]
					[i][j*LUT_REG_TOTAL+REG_SLOPE0] << 10) |
					(primary_data->color_index.COLOR_3D[pq_index]
					[i][j*LUT_REG_TOTAL+REG_SLOPE1] << 20),
					~0);
				cmdq_pkt_write(handle, comp->cmdq_base,
					comp->regs_pa + DISP_COLOR_CM_W1_HUE_3
						+ reg_index,
					primary_data->color_index.COLOR_3D[pq_index]
					[i][j*LUT_REG_TOTAL+REG_SLOPE2] |
					(primary_data->color_index.COLOR_3D[pq_index]
					[i][j*LUT_REG_TOTAL+REG_SLOPE3] << 8) |
					(primary_data->color_index.COLOR_3D[pq_index]
					[i][j*LUT_REG_TOTAL+REG_SLOPE4] << 16) |
					(primary_data->color_index.COLOR_3D[pq_index]
					[i][j*LUT_REG_TOTAL+REG_SLOPE5] << 24),
					~0);
				cmdq_pkt_write(handle, comp->cmdq_base,
					comp->regs_pa + DISP_COLOR_CM_W1_HUE_4
						+ reg_index,
					primary_data->color_index.COLOR_3D[pq_index]
					[i][j*LUT_REG_TOTAL+REG_WGT_LSLOPE] |
					(primary_data->color_index.COLOR_3D[pq_index]
					[i][j*LUT_REG_TOTAL+REG_WGT_USLOPE]
					<< 16),	~0);

				reg_index += (4 * 5);
			}
		}
	}
}

static void disp_color_set_sgy(struct mtk_ddp_comp *comp, struct cmdq_pkt *handle,
				void *sgy_gain)
{
	int i, cnt = DRECOLOR_SGY_Y_ENTRY * DRECOLOR_SGY_HUE_NUM / 4;
	unsigned int *param = sgy_gain;
	uint32_t value;

	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + DISP_COLOR_CFG_MAIN, 1 << 15, 1 << 15);
	for (i = 0; i < cnt; i++) {
		value = param[4 * i] |
			(param[4 * i + 1]  << 8) |
			(param[4 * i + 2] << 16) |
			(param[4 * i + 3] << 24);
		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DISP_COLOR_S_GAIN_BY_Y0_0 + i * 4, value, ~0);
	}
}

static void color_write_hw_reg(struct mtk_ddp_comp *comp,
	const struct DISPLAY_COLOR_REG *color_reg, struct cmdq_pkt *handle)
{
	int index = 0;
	unsigned char h_series[20] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
		, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
	unsigned int u4Temp = 0;
	struct mtk_disp_color *color = comp_to_color(comp);
	struct mtk_disp_color_primary *primary_data = color->primary_data;
	int i, j, reg_index;
	int wide_gamut_en = 0;
	/* mask s_gain_by_y when drecolor enable */
	struct pq_common_data *pq_data = comp->mtk_crtc->pq_data;
	int s_gain_by_y = !(pq_data->new_persist_property[DISP_DRE_CAPABILITY] & 0x1);

	DDPINFO("%s,SET COLOR REG id(%d) sgy %d\n", __func__, comp->id, s_gain_by_y);

	if (primary_data->color_bypass == 0) {
		if (color->data->support_color21 == true) {
			if (primary_data->legacy_color_cust)
				cmdq_pkt_write(handle, comp->cmdq_base,
					comp->regs_pa + DISP_COLOR_CFG_MAIN,
					(1 << 21)
					| (primary_data->color_index.LSP_EN << 20)
					| (primary_data->color_index.S_GAIN_BY_Y_EN << 15)
					| (wide_gamut_en << 8)
					| (0 << 7),
					0x003001FF | s_gain_by_y << 15);
			else
				cmdq_pkt_write(handle, comp->cmdq_base,
					comp->regs_pa + DISP_COLOR_CFG_MAIN,
					(1 << 21)
					| (color_reg->LSP_EN << 20)
					| (color_reg->S_GAIN_BY_Y_EN << 15)
					| (wide_gamut_en << 8)
					| (0 << 7),
					0x003001FF | s_gain_by_y << 15);
		} else {
			/* disable wide_gamut */
			cmdq_pkt_write(handle, comp->cmdq_base,
				comp->regs_pa + DISP_COLOR_CFG_MAIN,
				(0 << 8) | (0 << 7), 0x00001FF);
		}

		/* color start */
		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DISP_COLOR_START(color), 0x1, 0x3);

		/* enable R2Y/Y2R in Color Wrapper */
		if (color->data->support_color21 == true) {
			/* RDMA & OVL will enable wide-gamut function */
			/* disable rgb clipping function in CM1 */
			/* to keep the wide-gamut range */
			cmdq_pkt_write(handle, comp->cmdq_base,
				comp->regs_pa + DISP_COLOR_CM1_EN(color),
				0x03, 0x03);
		} else {
			cmdq_pkt_write(handle, comp->cmdq_base,
				comp->regs_pa + DISP_COLOR_CM1_EN(color),
				0x03, 0x03);
		}

		/* also set no rounding on Y2R */
		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DISP_COLOR_CM2_EN(color), 0x01, 0x01);
	} else {
		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DISP_COLOR_CFG_MAIN,
			(0x1 << 29), 0x20000000);
		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DISP_COLOR_START(color), 0x1, 0x1);
	}

	/* for partial Y contour issue */
	if (wide_gamut_en == 0)
		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DISP_COLOR_LUMA_ADJ, 0x40, 0x7F);
	else if (wide_gamut_en == 1)
		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DISP_COLOR_LUMA_ADJ, 0x0, 0x7F);

	/* config parameter from customer color_index.h */
	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + DISP_COLOR_G_PIC_ADJ_MAIN_1,
		(color_reg->BRIGHTNESS << 16) | color_reg->CONTRAST,
		0x07FF03FF);
	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + DISP_COLOR_G_PIC_ADJ_MAIN_2,
		color_reg->GLOBAL_SAT, 0x000003FF);

	/* Partial Y Function */
	for (index = 0; index < 8; index++) {
		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DISP_COLOR_Y_SLOPE_1_0_MAIN + 4 * index,
			(color_reg->PARTIAL_Y[2 * index] |
			 color_reg->PARTIAL_Y[2 * index + 1] << 16),
			 0x00FF00FF);
	}

	if (color->data->support_color21 == false)
		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DISP_COLOR_C_BOOST_MAIN,
			0 << 13, 0x00002000);
	else
		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DISP_COLOR_C_BOOST_MAIN_2,
			0x40 << 24,	0xFF000000);

	/* Partial Saturation Function */

	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + DISP_COLOR_PART_SAT_GAIN1_0,
		(color_reg->PURP_TONE_S[SG1][0] |
		color_reg->PURP_TONE_S[SG1][1] << 8 |
		color_reg->PURP_TONE_S[SG1][2] << 16 |
		color_reg->SKIN_TONE_S[SG1][0] << 24), ~0);

	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + DISP_COLOR_PART_SAT_GAIN1_1,
		(color_reg->SKIN_TONE_S[SG1][1] |
		color_reg->SKIN_TONE_S[SG1][2] << 8 |
		color_reg->SKIN_TONE_S[SG1][3] << 16 |
		color_reg->SKIN_TONE_S[SG1][4] << 24), ~0);

	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + DISP_COLOR_PART_SAT_GAIN1_2,
		(color_reg->SKIN_TONE_S[SG1][5] |
		color_reg->SKIN_TONE_S[SG1][6] << 8 |
		color_reg->SKIN_TONE_S[SG1][7] << 16 |
		color_reg->GRASS_TONE_S[SG1][0] << 24), ~0);

	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + DISP_COLOR_PART_SAT_GAIN1_3,
		(color_reg->GRASS_TONE_S[SG1][1] |
		color_reg->GRASS_TONE_S[SG1][2] << 8 |
		color_reg->GRASS_TONE_S[SG1][3] << 16 |
		color_reg->GRASS_TONE_S[SG1][4] << 24), ~0);

	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + DISP_COLOR_PART_SAT_GAIN1_4,
		(color_reg->GRASS_TONE_S[SG1][5] |
		color_reg->SKY_TONE_S[SG1][0] << 8 |
		color_reg->SKY_TONE_S[SG1][1] << 16 |
		color_reg->SKY_TONE_S[SG1][2] << 24), ~0);

	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + DISP_COLOR_PART_SAT_GAIN2_0,
		(color_reg->PURP_TONE_S[SG2][0] |
		color_reg->PURP_TONE_S[SG2][1] << 8 |
		color_reg->PURP_TONE_S[SG2][2] << 16 |
		color_reg->SKIN_TONE_S[SG2][0] << 24), ~0);

	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + DISP_COLOR_PART_SAT_GAIN2_1,
		(color_reg->SKIN_TONE_S[SG2][1] |
		color_reg->SKIN_TONE_S[SG2][2] << 8 |
		color_reg->SKIN_TONE_S[SG2][3] << 16 |
		color_reg->SKIN_TONE_S[SG2][4] << 24), ~0);

	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + DISP_COLOR_PART_SAT_GAIN2_2,
		(color_reg->SKIN_TONE_S[SG2][5] |
		color_reg->SKIN_TONE_S[SG2][6] << 8 |
		color_reg->SKIN_TONE_S[SG2][7] << 16 |
		color_reg->GRASS_TONE_S[SG2][0] << 24), ~0);

	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + DISP_COLOR_PART_SAT_GAIN2_3,
		(color_reg->GRASS_TONE_S[SG2][1] |
		color_reg->GRASS_TONE_S[SG2][2] << 8 |
		color_reg->GRASS_TONE_S[SG2][3] << 16 |
		color_reg->GRASS_TONE_S[SG2][4] << 24), ~0);

	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + DISP_COLOR_PART_SAT_GAIN2_4,
		(color_reg->GRASS_TONE_S[SG2][5] |
		color_reg->SKY_TONE_S[SG2][0] << 8 |
		color_reg->SKY_TONE_S[SG2][1] << 16 |
		color_reg->SKY_TONE_S[SG2][2] << 24), ~0);

	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + DISP_COLOR_PART_SAT_GAIN3_0,
		(color_reg->PURP_TONE_S[SG3][0] |
		color_reg->PURP_TONE_S[SG3][1] << 8 |
		color_reg->PURP_TONE_S[SG3][2] << 16 |
		color_reg->SKIN_TONE_S[SG3][0] << 24), ~0);

	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + DISP_COLOR_PART_SAT_GAIN3_1,
		(color_reg->SKIN_TONE_S[SG3][1] |
		color_reg->SKIN_TONE_S[SG3][2] << 8 |
		color_reg->SKIN_TONE_S[SG3][3] << 16 |
		color_reg->SKIN_TONE_S[SG3][4] << 24), ~0);

	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + DISP_COLOR_PART_SAT_GAIN3_2,
		(color_reg->SKIN_TONE_S[SG3][5] |
		color_reg->SKIN_TONE_S[SG3][6] << 8 |
		color_reg->SKIN_TONE_S[SG3][7] << 16 |
		color_reg->GRASS_TONE_S[SG3][0] << 24), ~0);

	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + DISP_COLOR_PART_SAT_GAIN3_3,
		(color_reg->GRASS_TONE_S[SG3][1] |
		color_reg->GRASS_TONE_S[SG3][2] << 8 |
		color_reg->GRASS_TONE_S[SG3][3] << 16 |
		color_reg->GRASS_TONE_S[SG3][4] << 24), ~0);

	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + DISP_COLOR_PART_SAT_GAIN3_4,
		(color_reg->GRASS_TONE_S[SG3][5] |
		color_reg->SKY_TONE_S[SG3][0] << 8 |
		color_reg->SKY_TONE_S[SG3][1] << 16 |
		color_reg->SKY_TONE_S[SG3][2] << 24), ~0);

	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + DISP_COLOR_PART_SAT_POINT1_0,
		(color_reg->PURP_TONE_S[SP1][0] |
		color_reg->PURP_TONE_S[SP1][1] << 8 |
		color_reg->PURP_TONE_S[SP1][2] << 16 |
		color_reg->SKIN_TONE_S[SP1][0] << 24), ~0);

	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + DISP_COLOR_PART_SAT_POINT1_1,
		(color_reg->SKIN_TONE_S[SP1][1] |
		color_reg->SKIN_TONE_S[SP1][2] << 8 |
		color_reg->SKIN_TONE_S[SP1][3] << 16 |
		color_reg->SKIN_TONE_S[SP1][4] << 24), ~0);

	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + DISP_COLOR_PART_SAT_POINT1_2,
		(color_reg->SKIN_TONE_S[SP1][5] |
		color_reg->SKIN_TONE_S[SP1][6] << 8 |
		color_reg->SKIN_TONE_S[SP1][7] << 16 |
		color_reg->GRASS_TONE_S[SP1][0] << 24), ~0);

	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + DISP_COLOR_PART_SAT_POINT1_3,
		(color_reg->GRASS_TONE_S[SP1][1] |
		color_reg->GRASS_TONE_S[SP1][2] << 8 |
		color_reg->GRASS_TONE_S[SP1][3] << 16 |
		color_reg->GRASS_TONE_S[SP1][4] << 24), ~0);

	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + DISP_COLOR_PART_SAT_POINT1_4,
		(color_reg->GRASS_TONE_S[SP1][5] |
		color_reg->SKY_TONE_S[SP1][0] << 8 |
		color_reg->SKY_TONE_S[SP1][1] << 16 |
		color_reg->SKY_TONE_S[SP1][2] << 24), ~0);

	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + DISP_COLOR_PART_SAT_POINT2_0,
		(color_reg->PURP_TONE_S[SP2][0] |
		color_reg->PURP_TONE_S[SP2][1] << 8 |
		color_reg->PURP_TONE_S[SP2][2] << 16 |
		color_reg->SKIN_TONE_S[SP2][0] << 24), ~0);

	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + DISP_COLOR_PART_SAT_POINT2_1,
		(color_reg->SKIN_TONE_S[SP2][1] |
		color_reg->SKIN_TONE_S[SP2][2] << 8 |
		color_reg->SKIN_TONE_S[SP2][3] << 16 |
		color_reg->SKIN_TONE_S[SP2][4] << 24), ~0);

	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + DISP_COLOR_PART_SAT_POINT2_2,
		(color_reg->SKIN_TONE_S[SP2][5] |
		color_reg->SKIN_TONE_S[SP2][6] << 8 |
		color_reg->SKIN_TONE_S[SP2][7] << 16 |
		color_reg->GRASS_TONE_S[SP2][0] << 24), ~0);

	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + DISP_COLOR_PART_SAT_POINT2_3,
		(color_reg->GRASS_TONE_S[SP2][1] |
		color_reg->GRASS_TONE_S[SP2][2] << 8 |
		color_reg->GRASS_TONE_S[SP2][3] << 16 |
		color_reg->GRASS_TONE_S[SP2][4] << 24), ~0);


	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + DISP_COLOR_PART_SAT_POINT2_4,
		(color_reg->GRASS_TONE_S[SP2][5] |
		color_reg->SKY_TONE_S[SP2][0] << 8 |
		color_reg->SKY_TONE_S[SP2][1] << 16 |
		color_reg->SKY_TONE_S[SP2][2] << 24), ~0);

	for (index = 0; index < 3; index++) {
		h_series[index + PURP_TONE_START] =
			color_reg->PURP_TONE_H[index];
	}

	for (index = 0; index < 8; index++) {
		h_series[index + SKIN_TONE_START] =
		    color_reg->SKIN_TONE_H[index];
	}

	for (index = 0; index < 6; index++) {
		h_series[index + GRASS_TONE_START] =
			color_reg->GRASS_TONE_H[index];
	}

	for (index = 0; index < 3; index++) {
		h_series[index + SKY_TONE_START] =
		    color_reg->SKY_TONE_H[index];
	}

	for (index = 0; index < 5; index++) {
		u4Temp = (h_series[4 * index]) +
		    (h_series[4 * index + 1] << 8) +
		    (h_series[4 * index + 2] << 16) +
		    (h_series[4 * index + 3] << 24);
		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DISP_COLOR_LOCAL_HUE_CD_0 + 4 * index,
			u4Temp, ~0);
	}

	if (color->data->support_color21 == true) {
		/* S Gain By Y */
		u4Temp = 0;

		reg_index = 0;
		for (i = 0; i < S_GAIN_BY_Y_CONTROL_CNT && s_gain_by_y; i++) {
			for (j = 0; j < S_GAIN_BY_Y_HUE_PHASE_CNT; j += 4) {
				if (primary_data->legacy_color_cust)
					u4Temp = (primary_data->color_index.S_GAIN_BY_Y[i][j]) +
						(primary_data->color_index.S_GAIN_BY_Y[i][j + 1]
						<< 8) +
						(primary_data->color_index.S_GAIN_BY_Y[i][j + 2]
						<< 16) +
						(primary_data->color_index.S_GAIN_BY_Y[i][j + 3]
						<< 24);
				else
					u4Temp = (color_reg->S_GAIN_BY_Y[i][j]) +
						(color_reg->S_GAIN_BY_Y[i][j + 1]
						<< 8) +
						(color_reg->S_GAIN_BY_Y[i][j + 2]
						<< 16) +
						(color_reg->S_GAIN_BY_Y[i][j + 3]
						<< 24);

				cmdq_pkt_write(handle, comp->cmdq_base,
					comp->regs_pa +
					DISP_COLOR_S_GAIN_BY_Y0_0 +
					reg_index,
					u4Temp, ~0);
				reg_index += 4;
			}
		}
		/* LSP */
		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DISP_COLOR_LSP_1,
			(primary_data->color_index.LSP[3] << 0) |
			(primary_data->color_index.LSP[2] << 7) |
			(primary_data->color_index.LSP[1] << 14) |
			(primary_data->color_index.LSP[0] << 22), 0x1FFFFFFF);
		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DISP_COLOR_LSP_2,
			(primary_data->color_index.LSP[7] << 0) |
			(primary_data->color_index.LSP[6] << 8) |
			(primary_data->color_index.LSP[5] << 16) |
			(primary_data->color_index.LSP[4] << 23), 0x3FFF7F7F);
	}

	/* color window */
	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + DISP_COLOR_TWO_D_WINDOW_1,
		color->data->color_window, ~0);

	if (color->data->support_color30 == true) {
		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DISP_COLOR_CM_CONTROL,
			0x0 |
			(0x3 << 1) |	/* enable window 1 */
			(0x3 << 4) |	/* enable window 2 */
			(0x3 << 7)		/* enable window 3 */
			, 0x1B7);

		for (i = 0; i < WIN_TOTAL; i++) {
			reg_index = i * 4 * (LUT_TOTAL * 5);
			for (j = 0; j < LUT_TOTAL; j++) {
				cmdq_pkt_write(handle, comp->cmdq_base,
					comp->regs_pa + DISP_COLOR_CM_W1_HUE_0 +
					reg_index,
					color_reg->COLOR_3D[i]
					[j*LUT_REG_TOTAL+REG_L] |
					(color_reg->COLOR_3D[i]
					[j*LUT_REG_TOTAL+REG_U] << 10) |
					(color_reg->COLOR_3D[i]
					[j*LUT_REG_TOTAL+REG_POINT0] << 20),
					~0);
				cmdq_pkt_write(handle, comp->cmdq_base,
					comp->regs_pa + DISP_COLOR_CM_W1_HUE_1 +
					reg_index,
					color_reg->COLOR_3D[i]
					[j*LUT_REG_TOTAL+REG_POINT1] |
					(color_reg->COLOR_3D[i]
					[j*LUT_REG_TOTAL+REG_POINT2] << 10) |
					(color_reg->COLOR_3D[i]
					[j*LUT_REG_TOTAL+REG_POINT3] << 20),
					~0);
				cmdq_pkt_write(handle, comp->cmdq_base,
					comp->regs_pa + DISP_COLOR_CM_W1_HUE_2 +
					reg_index,
					color_reg->COLOR_3D[i]
					[j*LUT_REG_TOTAL+REG_POINT4] |
					(color_reg->COLOR_3D[i]
					[j*LUT_REG_TOTAL+REG_SLOPE0] << 10) |
					(color_reg->COLOR_3D[i]
					[j*LUT_REG_TOTAL+REG_SLOPE1] << 20),
					~0);
				cmdq_pkt_write(handle, comp->cmdq_base,
					comp->regs_pa + DISP_COLOR_CM_W1_HUE_3 +
					reg_index,
					color_reg->COLOR_3D[i]
					[j*LUT_REG_TOTAL+REG_SLOPE2] |
					(color_reg->COLOR_3D[i]
					[j*LUT_REG_TOTAL+REG_SLOPE3] << 8) |
					(color_reg->COLOR_3D[i]
					[j*LUT_REG_TOTAL+REG_SLOPE4] << 16) |
					(color_reg->COLOR_3D[i]
					[j*LUT_REG_TOTAL+REG_SLOPE5] << 24),
					~0);
				cmdq_pkt_write(handle, comp->cmdq_base,
					comp->regs_pa + DISP_COLOR_CM_W1_HUE_4 +
					reg_index,
					color_reg->COLOR_3D[i]
					[j*LUT_REG_TOTAL+REG_WGT_LSLOPE] |
					(color_reg->COLOR_3D[i]
					[j*LUT_REG_TOTAL+REG_WGT_USLOPE] << 16),
					~0);

				reg_index += (4 * 5);
			}
		}
	}
}

static void mtk_disp_color_config_overhead(struct mtk_ddp_comp *comp,
	struct mtk_ddp_config *cfg)
{
	struct mtk_disp_color *color = comp_to_color(comp);
	struct mtk_disp_color_primary *primary_data = color->primary_data;

	DDPINFO("line: %d\n", __LINE__);
	if (cfg->tile_overhead.is_support) {
		if (!color->is_right_pipe) {
			primary_data->tile_overhead.left_comp_overhead = 0;
			/*add component overhead on total overhead*/
			cfg->tile_overhead.left_overhead +=
				primary_data->tile_overhead.left_comp_overhead;
			cfg->tile_overhead.left_in_width +=
				primary_data->tile_overhead.left_comp_overhead;
			/*copy from total overhead info*/
			primary_data->tile_overhead.left_in_width =
				cfg->tile_overhead.left_in_width;
			primary_data->tile_overhead.left_overhead =
				cfg->tile_overhead.left_overhead;
		} else {
			/*set component overhead*/
			primary_data->tile_overhead.right_comp_overhead = 0;
			/*add component overhead on total overhead*/
			cfg->tile_overhead.right_overhead +=
				primary_data->tile_overhead.right_comp_overhead;
			cfg->tile_overhead.right_in_width +=
				primary_data->tile_overhead.right_comp_overhead;
			/*copy from total overhead info*/
			primary_data->tile_overhead.right_in_width =
				cfg->tile_overhead.right_in_width;
			primary_data->tile_overhead.right_overhead =
				cfg->tile_overhead.right_overhead;
		}
	}
}

static void mtk_color_config(struct mtk_ddp_comp *comp,
			     struct mtk_ddp_config *cfg,
			     struct cmdq_pkt *handle)
{
	struct mtk_disp_color *color = comp_to_color(comp);
	struct mtk_disp_color_primary *primary_data = color->primary_data;
	unsigned int width;
	struct DISP_AAL_DRECOLOR_PARAM *drecolor_sgy = &color->primary_data->drecolor_sgy;
	struct pq_common_data *pq_data = comp->mtk_crtc->pq_data;

	if (comp->mtk_crtc->is_dual_pipe && cfg->tile_overhead.is_support)
		width = primary_data->tile_overhead.left_in_width;
	else {
		if (comp->mtk_crtc->is_dual_pipe)
			width = cfg->w / 2;
		else
			width = cfg->w;
	}

	if (comp->mtk_crtc->is_dual_pipe) {
		primary_data->width = width;
	}

	if (comp->mtk_crtc->is_dual_pipe)
		color->color_dst_w = cfg->w / 2;
	else
		color->color_dst_w = cfg->w;
	color->color_dst_h = cfg->h;

	cmdq_pkt_write(handle, comp->cmdq_base,
		       comp->regs_pa + DISP_COLOR_WIDTH(color), width, ~0);
	cmdq_pkt_write(handle, comp->cmdq_base,
		       comp->regs_pa + DISP_COLOR_HEIGHT(color), cfg->h, ~0);
	mutex_lock(&primary_data->reg_lock);
	if ((pq_data->new_persist_property[DISP_DRE_CAPABILITY] & 0x1) &&
		drecolor_sgy->sgy_trans_trigger) {
		DDPINFO("%s set sgy\n", __func__);
		disp_color_set_sgy(comp, handle, drecolor_sgy->sgy_out_gain);
	}
	mutex_unlock(&primary_data->reg_lock);

	// set color_8bit_switch register
	if (cfg->source_bpc == 8)
		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DISP_COLOR_CFG_MAIN, (0x1 << 25), (0x1 << 25));
	else if (cfg->source_bpc == 10)
		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DISP_COLOR_CFG_MAIN, (0x0 << 25), (0x1 << 25));
	else
		DDPINFO("Disp COLOR's bit is : %u\n", cfg->bpc);
}

void ddp_color_bypass_color(struct mtk_ddp_comp *comp, int bypass,
		struct cmdq_pkt *handle)
{
	struct mtk_disp_color_primary *primary_data = NULL;

	DDPINFO("%s, bypass:%d\n", __func__, bypass);
	if (comp == NULL) {
		DDPPR_ERR("%s, null pointer!", __func__);
		return;
	}
	primary_data = comp_to_color(comp)->primary_data;
	primary_data->color_bypass = bypass;

	if (bypass) {
		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DISP_COLOR_CFG_MAIN,
			(1 << 7), 0xFF); /* bypass all */
	} else {
		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DISP_COLOR_CFG_MAIN,
			(0 << 7), 0xFF); /* resume all */
	}
}

static bool color_get_TDSHP0_REG(struct resource *res)
{
	int rc = 0;
	struct device_node *node = NULL;

	node = of_find_compatible_node(NULL, NULL, "mediatek,mdp-tuning-mdp_tdshp0");
	rc = of_address_to_resource(node, 0, res);

	// check if fail to get reg.
	if (rc) {
		DDPINFO("Fail to get TDSHP0 REG\n");
		return false;
	}

	DDPDBG("TDSHP0 REG: 0x%llx ~ 0x%llx\n", res->start, res->end);

	return true;
}

static bool color_get_MML_TDSHP0_REG(struct resource *res)
{
	int rc = 0;
	struct device_node *node = NULL;

	node = of_find_compatible_node(NULL, NULL, "mediatek,mml-tuning-mml_tdshp0");
	rc = of_address_to_resource(node, 0, res);

	// check if fail to get reg.
	if (rc) {
		DDPINFO("Fail to get MML TDSHP0 REG\n");
		return false;
	}

	DDPDBG("MML TDSHP0 REG: 0x%llx ~ 0x%llx\n", res->start, res->end);

	return true;
}

#if defined(SUPPORT_ULTRA_RESOLUTION)
static bool color_get_MDP_RSZ0_REG(struct resource *res)
{
	int rc = 0;
	struct device_node *node = NULL;

	node = of_find_compatible_node(NULL, NULL, "mediatek,mdp_rsz0");
	rc = of_address_to_resource(node, 0, res);

	// check if fail to get reg.
	if (rc) {
		DDPINFO("Fail to get MDP_RSZ0 REG\n");
		return false
	}

	DDPDBG("MDP_RSZ0 REG: 0x%llx ~ 0x%llx\n", res->start, res->end);

	return true;
}

static bool color_get_MDP_RSZ1_REG(struct resource *res)
{
	int rc = 0;
	struct device_node *node = NULL;

	node = of_find_compatible_node(NULL, NULL, "mediatek,mdp_rsz1");
	rc = of_address_to_resource(node, 0, res);

	// check if fail to get reg.
	if (rc) {
		DDPINFO("Fail to get MDP_RSZ1 REG\n");
		return false;
	}

	DDPDBG("MDP_RSZ1 REG: 0x%lx ~ 0x%lx\n", res->start, res->end);

	return true;
}
#endif

static bool color_get_MDP_RDMA0_REG(struct resource *res)
{
	int rc = 0;
	struct device_node *node = NULL;

	node = of_find_compatible_node(NULL, NULL, "mediatek,mdp_rdma0");
	rc = of_address_to_resource(node, 0, res);

	// check if fail to get reg.
	if (rc) {
		DDPINFO("Fail to get MDP_RDMA0 REG\n");
		return false;
	}

	DDPDBG("MDP_RDMA0 REG: 0x%llx ~ 0x%llx\n", res->start, res->end);

	return true;
}

static bool color_get_MML_HDR0_REG(struct resource *res)
{
	int rc = 0;
	struct device_node *node = NULL;

	node = of_find_compatible_node(NULL, NULL, "mediatek,mml-tuning-mml_hdr0");
	rc = of_address_to_resource(node, 0, res);

	// check if fail to get reg.
	if (rc) {
		DDPINFO("Fail to get MML_HDR0 REG\n");
		return false;
	}

	DDPDBG("MML_HDR0 REG: 0x%llx ~ 0x%llx\n", res->start, res->end);

	return true;
}

static bool color_get_MDP_HDR0_REG(struct resource *res)
{
	int rc = 0;
	struct device_node *node = NULL;

	node = of_find_compatible_node(NULL, NULL, "mediatek,mdp-tuning-mdp_hdr0");
	rc = of_address_to_resource(node, 0, res);

	// check if fail to get reg.
	if (rc) {
		DDPINFO("Fail to get MDP_HDR0 REG\n");
		return false;
	}

	DDPDBG("MDP_HDR0 REG: 0x%llx ~ 0x%llx\n", res->start, res->end);

	return true;
}

static bool color_get_MML_COLOR0_REG(struct resource *res)
{
	int rc = 0;
	struct device_node *node = NULL;

	node = of_find_compatible_node(NULL, NULL, "mediatek,mml-tuning-mml_color0");
	rc = of_address_to_resource(node, 0, res);

	// check if fail to get reg.
	if (rc) {
		DDPINFO("Fail to get MML_COLOR0 REG\n");
		return false;
	}

	DDPDBG("MML_COLOR0 REG: 0x%llx ~ 0x%llx\n", res->start, res->end);

	return true;
}

static bool color_get_MDP_COLOR0_REG(struct resource *res)
{
	int rc = 0;
	struct device_node *node = NULL;

	node = of_find_compatible_node(NULL, NULL, "mediatek,mdp-tuning-mdp_color0");
	rc = of_address_to_resource(node, 0, res);

	// check if fail to get reg.
	if (rc) {
		DDPINFO("Fail to get MDP_COLOR0 REG\n");
		return false;
	}

	DDPDBG("MDP_COLOR0 REG: 0x%llx ~ 0x%llx\n", res->start, res->end);

	return true;
}

static bool color_get_MML_AAL0_REG(struct resource *res)
{
	int rc = 0;
	struct device_node *node = NULL;

	node = of_find_compatible_node(NULL, NULL, "mediatek,mml-tuning-mml_aal0");
	rc = of_address_to_resource(node, 0, res);

	// check if fail to get reg.
	if (rc) {
		DDPINFO("Fail to get MML_AAL0 REG\n");
		return false;
	}

	DDPDBG("MML_AAL0 REG: 0x%llx ~ 0x%llx\n", res->start, res->end);

	return true;
}

static bool color_get_MDP_AAL0_REG(struct resource *res)
{
	int rc = 0;
	struct device_node *node = NULL;

	node = of_find_compatible_node(NULL, NULL, "mediatek,mdp-tuning-mdp_aal0");
	rc = of_address_to_resource(node, 0, res);

	// check if fail to get reg.
	if (rc)	{
		DDPINFO("Fail to get MDP_AAL0 REG\n");
		return false;
	}

	DDPDBG("MDP_AAL0 REG: 0x%llx ~ 0x%llx\n", res->start, res->end);

	return true;
}

static bool color_get_DISP1_COLOR0_REG(struct resource *res)
{
	int rc = 0;
	struct device_node *node = NULL;

	node = of_find_compatible_node(NULL, NULL, "mediatek,disp1_color0");
	rc = of_address_to_resource(node, 0, res);

	// check if fail to get reg.
	if (rc)	{
		DDPINFO("Fail to get disp1_color0 REG\n");
		return false;
	}

	DDPDBG("disp1_color0 REG: 0x%llx ~ 0x%llx\n", res->start, res->end);

	return true;
}

static bool color_get_DISP1_CCORR0_REG(struct resource *res)
{
	int rc = 0;
	struct device_node *node = NULL;

	node = of_find_compatible_node(NULL, NULL, "mediatek,disp1_ccorr0");
	rc = of_address_to_resource(node, 0, res);

	// check if fail to get reg.
	if (rc)	{
		DDPINFO("Fail to get disp1_ccorr0 REG\n");
		return false;
	}

	DDPDBG("disp1_ccorr0 REG: 0x%llx ~ 0x%llx\n", res->start, res->end);

	return true;
}

static bool color_get_DISP1_AAL0_REG(struct resource *res)
{
	int rc = 0;
	struct device_node *node = NULL;

	node = of_find_compatible_node(NULL, NULL, "mediatek,disp1_aal0");
	rc = of_address_to_resource(node, 0, res);

	// check if fail to get reg.
	if (rc)	{
		DDPINFO("Fail to get disp1_aal0 REG\n");
		return false;
	}

	DDPDBG("disp1_aal0 REG: 0x%llx ~ 0x%llx\n", res->start, res->end);

	return true;
}

static bool color_get_DISP1_GAMMA0_REG(struct resource *res)
{
	int rc = 0;
	struct device_node *node = NULL;

	node = of_find_compatible_node(NULL, NULL, "mediatek,disp1_gamma0");
	rc = of_address_to_resource(node, 0, res);

	// check if fail to get reg.
	if (rc)	{
		DDPINFO("Fail to get disp1_gamma0 REG\n");
		return false;
	}

	DDPDBG("disp1_gamma0 REG: 0x%llx ~ 0x%llx\n", res->start, res->end);

	return true;
}

static bool color_get_DISP1_DITHER0_REG(struct resource *res)
{
	int rc = 0;
	struct device_node *node = NULL;

	node = of_find_compatible_node(NULL, NULL, "mediatek,disp1_dither0");
	rc = of_address_to_resource(node, 0, res);

	// check if fail to get reg.
	if (rc)	{
		DDPINFO("Fail to get disp1_dither0 REG\n");
		return false;
	}

	DDPDBG("disp1_dither0 REG: 0x%llx ~ 0x%llx\n", res->start, res->end);

	return true;
}

static bool color_get_DISP1_CCORR1_REG(struct resource *res)
{
	int rc = 0;
	struct device_node *node = NULL;

	node = of_find_compatible_node(NULL, NULL, "mediatek,disp1_ccorr1");
	rc = of_address_to_resource(node, 0, res);

	// check if fail to get reg.
	if (rc)	{
		DDPINFO("Fail to get disp1_ccorr1 REG\n");
		return false;
	}

	DDPDBG("disp1_ccorr1 REG: 0x%llx ~ 0x%llx\n", res->start, res->end);

	return true;
}

static bool color_get_DISP1_TDSHP0_REG(struct resource *res)
{
	int rc = 0;
	struct device_node *node = NULL;

	node = of_find_compatible_node(NULL, NULL, "mediatek,disp1_tdshp0");
	rc = of_address_to_resource(node, 0, res);

	// check if fail to get reg.
	if (rc)	{
		DDPINFO("Fail to get disp1_tdshp0 REG\n");
		return false;
	}

	DDPDBG("disp1_tdshp0 REG: 0x%llx ~ 0x%llx\n", res->start, res->end);

	return true;
}

static bool color_get_DISP1_C3D0_REG(struct resource *res)
{
	int rc = 0;
	struct device_node *node = NULL;

	node = of_find_compatible_node(NULL, NULL, "mediatek,disp1_c3d0");
	rc = of_address_to_resource(node, 0, res);

	// check if fail to get reg.
	if (rc)	{
		DDPINFO("Fail to get disp1_c3d0 REG\n");
		return false;
	}

	DDPDBG("disp1_c3d0 REG: 0x%llx ~ 0x%llx\n", res->start, res->end);

	return true;
}

static bool color_get_DISP1_DMDP_AAL0_REG(struct resource *res)
{
	int rc = 0;
	struct device_node *node = NULL;

	node = of_find_compatible_node(NULL, NULL, "mediatek,disp1_mdp_aal0");
	rc = of_address_to_resource(node, 0, res);

	// check if fail to get reg.
	if (rc)	{
		DDPINFO("Fail to get disp1_mdp_aal0 REG\n");
		return false;
	}

	DDPDBG("disp1_mdp_aal0 REG: 0x%llx ~ 0x%llx\n", res->start, res->end);

	return true;
}

static bool color_get_DISP1_ODDMR0_REG(struct resource *res)
{
	int rc = 0;
	struct device_node *node = NULL;

	node = of_find_compatible_node(NULL, NULL, "mediatek,disp1_oddmr0");
	rc = of_address_to_resource(node, 0, res);

	// check if fail to get reg.
	if (rc)	{
		DDPINFO("Fail to get disp1_oddmr0 REG\n");
		return false;
	}

	DDPDBG("disp1_oddmr0 REG: 0x%llx ~ 0x%llx\n", res->start, res->end);

	return true;
}

static int get_tuning_reg_table_idx_and_offset(struct mtk_ddp_comp *comp,
	unsigned long addr, unsigned int *offset)
{
	unsigned int i = 0;
	unsigned long reg_addr;
	struct mtk_disp_color *color = comp_to_color(comp);

	if (addr == 0) {
		DDPPR_ERR("addr is NULL\n");
		return -1;
	}

	for (i = 0; i < TUNING_REG_MAX; i++) {
		reg_addr = color->data->reg_table[i];
		if (addr >= reg_addr && addr < reg_addr + 0x1000) {
			*offset = addr - reg_addr;
			return i;
		}
	}

	return -1;
}

static int color_is_reg_addr_valid(struct mtk_ddp_comp *comp,
	unsigned long addr)
{
	unsigned int i = 0;
	unsigned long reg_addr;
	struct mtk_disp_color *color = comp_to_color(comp);
	struct resource res;
	unsigned int regTableSize = sizeof(color->data->reg_table) /
				sizeof(unsigned long);
	DDPDBG("regTableSize: %d", regTableSize);

	if (addr == 0) {
		DDPPR_ERR("addr is NULL\n");
		return -1;
	}

	if ((addr & 0x3) != 0) {
		DDPPR_ERR("addr is not 4-byte aligned!\n");
		return -1;
	}

	for (i = 0; i < regTableSize; i++) {
		reg_addr = color->data->reg_table[i];
		if (addr >= reg_addr && addr < reg_addr + 0x1000)
			break;
	}

	if (i < regTableSize) {
		DDPINFO("addr valid, addr=0x%08lx\n", addr);
		return i;
	}

	/*Check if MDP RSZ base address*/
#if defined(SUPPORT_ULTRA_RESOLUTION)
	if (color_get_MDP_RSZ0_REG(&res) &&
		addr >= res.start && addr < res.end) {
		DDPDBG("addr=0x%lx, module=MDP_RSZ0\n", addr);
		return 2;
	}

	if (color_get_MDP_RSZ1_REG(&res) &&
		addr >= res.start && addr < res.end) {
		DDPDBG("addr=0x%lx, module=MDP_RSZ1\n", addr);
		return 2;
	}
#endif

	if (color_get_MDP_RDMA0_REG(&res) &&
		addr >= res.start && addr < res.end) {
		DDPDBG("addr=0x%lx, module=MDP_RDMA0\n", addr);
		return 2;
	}

	if (color_get_MDP_HDR0_REG(&res) &&
		addr >= res.start && addr < res.end) {
		DDPDBG("addr=0x%lx, module=MDP_HDR0\n", addr);
		return 2;
	}

	if (color_get_MML_HDR0_REG(&res) &&
		addr >= res.start && addr < res.end) {
		DDPDBG("addr=0x%lx, module=MML_HDR0\n", addr);
		return 2;
	}

	if (color_get_MDP_COLOR0_REG(&res) &&
		addr >= res.start && addr < res.end) {
		DDPDBG("addr=0x%lx, module=MDP_COLOR0\n", addr);
		return 2;
	}

	if (color_get_MML_COLOR0_REG(&res) &&
		addr >= res.start && addr < res.end) {
		DDPDBG("addr=0x%lx, module=MML_COLOR0\n", addr);
		return 2;
	}

	/*Check if MDP AAL base address*/
	if (color_get_MDP_AAL0_REG(&res) &&
		addr >= res.start && addr < res.end) {
		DDPDBG("addr=0x%lx, module=MDP_AAL0\n", addr);
		return 2;
	}

	if (color_get_MML_AAL0_REG(&res) &&
		addr >= res.start && addr < res.end) {
		DDPDBG("addr=0x%lx, module=MML_AAL0\n", addr);
		return 2;
	}

	if (color_get_TDSHP0_REG(&res) &&
		addr >= res.start && addr < res.end) {
		DDPDBG("addr=0x%lx, module=TDSHP0\n", addr);
		return 2;
	}

	if (color_get_MML_TDSHP0_REG(&res) &&
		addr >= res.start && addr < res.end) {
		DDPDBG("addr=0x%lx, module=MML_TDSHP0\n", addr);
		return 2;
	}

	DDPPR_ERR("invalid address! addr=0x%lx!\n", addr);
	return -1;
}

int mtk_drm_color_cfg_set_pqparam(struct mtk_ddp_comp *comp,
	struct cmdq_pkt *handle, void *data, unsigned int data_size)
{
	int ret = 0;
	struct DISP_PQ_PARAM *pq_param;
	struct mtk_disp_color *color = comp_to_color(comp);
	struct mtk_disp_color_primary *primary_data = color->primary_data;

	pq_param = &primary_data->color_param;
	memcpy(pq_param, (struct DISP_PQ_PARAM *)data,
		sizeof(struct DISP_PQ_PARAM));

	if (primary_data->ncs_tuning_mode == 0) {
		/* normal mode */
		/* normal mode */
		DpEngine_COLORonInit(comp, handle);
		DpEngine_COLORonConfig(comp, handle);
		if (comp->mtk_crtc->is_dual_pipe) {
			struct mtk_ddp_comp *comp_color1 = color->companion;

			DpEngine_COLORonInit(comp_color1, handle);
			DpEngine_COLORonConfig(comp_color1, handle);
		}

		DDPINFO("SET_PQ_PARAM\n");
	} else {
		/* ncs_tuning_mode = 0; */
		ret = -EINVAL;
		DDPINFO("SET_PQ_PARAM, bypassed by ncs_tuning_mode = 1\n");
	}

	return ret;

}

int mtk_drm_ioctl_set_pqparam_impl(struct mtk_ddp_comp *comp, void *data)
{
	int ret = 0;
	struct mtk_drm_crtc *mtk_crtc = comp->mtk_crtc;
	struct mtk_disp_color *color = comp_to_color(comp);
	struct mtk_disp_color_primary *primary_data = color->primary_data;
	struct DISP_PQ_PARAM *pq_param;

	pq_param = &primary_data->color_param;
	memcpy(pq_param, (struct DISP_PQ_PARAM *)data,
		sizeof(struct DISP_PQ_PARAM));

	if (primary_data->ncs_tuning_mode == 0) {
		/* normal mode */
		ret = mtk_crtc_user_cmd(&mtk_crtc->base, comp, SET_PQPARAM, data);
		mtk_crtc_check_trigger(mtk_crtc, true, true);

		DDPINFO("SET_PQ_PARAM\n");
	} else {
		/* ncs_tuning_mode = 0; */
		DDPINFO
		 ("SET_PQ_PARAM, bypassed by ncs_tuning_mode = 1\n");
	}

	return ret;
}

int mtk_drm_ioctl_set_pqparam(struct drm_device *dev, void *data,
		struct drm_file *file_priv)
{
	struct mtk_drm_private *private = dev->dev_private;
	struct drm_crtc *crtc = private->crtc[0];
	struct mtk_ddp_comp *comp = mtk_ddp_comp_sel_in_cur_crtc_path(
			to_mtk_crtc(crtc), MTK_DISP_COLOR, 0);

	return mtk_drm_ioctl_set_pqparam_impl(comp, data);
}

int mtk_drm_color_cfg_set_pqindex(struct mtk_ddp_comp *comp,
	struct cmdq_pkt *handle, void *data, unsigned int data_size)
{
	int ret = 0;
	struct DISPLAY_PQ_T *pq_index;

	pq_index = get_Color_index(comp);
	memcpy(pq_index, (struct DISPLAY_PQ_T *)data,
		sizeof(struct DISPLAY_PQ_T));

	return ret;
}

int mtk_drm_ioctl_set_pqindex_impl(struct mtk_ddp_comp *comp, void *data)
{
	int ret = 0;
	struct DISPLAY_PQ_T *pq_index;

	DDPINFO("%s...", __func__);

	pq_index = get_Color_index(comp);
	memcpy(pq_index, (struct DISPLAY_PQ_T *)data,
		sizeof(struct DISPLAY_PQ_T));

	return ret;
}

int mtk_drm_ioctl_set_pqindex(struct drm_device *dev, void *data,
		struct drm_file *file_priv)
{
	struct mtk_drm_private *private = dev->dev_private;
	struct drm_crtc *crtc = private->crtc[0];
	struct mtk_ddp_comp *comp = mtk_ddp_comp_sel_in_cur_crtc_path(
			to_mtk_crtc(crtc), MTK_DISP_COLOR, 0);

	return mtk_drm_ioctl_set_pqindex_impl(comp, data);
}

int mtk_color_cfg_set_color_reg(struct mtk_ddp_comp *comp,
	struct cmdq_pkt *handle, void *data, unsigned int data_size)
{
	int ret = 0;
	struct mtk_disp_color *color = comp_to_color(comp);
	struct mtk_disp_color_primary *primary_data = color->primary_data;

	DDPINFO("%s,SET COLOR REG id(%d)\n", __func__, comp->id);

	if (data != NULL) {
		memcpy(&primary_data->color_reg, (struct DISPLAY_COLOR_REG *)data,
			sizeof(struct DISPLAY_COLOR_REG));

		color_write_hw_reg(comp, &primary_data->color_reg, handle);
		if (comp->mtk_crtc->is_dual_pipe) {
			struct mtk_ddp_comp *comp_color1 = color->companion;

			DDPINFO("%s,SET COLOR REG id(%d)\n", __func__, comp_color1->id);
			color_write_hw_reg(comp_color1, &primary_data->color_reg, handle);
		}
	} else {
		ret = -EINVAL;
		DDPINFO("%s: data is NULL", __func__);
	}
	primary_data->color_reg_valid = 1;

	return ret;
}

int mtk_drm_ioctl_set_color_reg_impl(struct mtk_ddp_comp *comp, void *data)
{
	struct mtk_drm_crtc *mtk_crtc = comp->mtk_crtc;
	int ret;

	ret = mtk_crtc_user_cmd(&mtk_crtc->base, comp, SET_COLOR_REG, data);
	mtk_crtc_check_trigger(mtk_crtc, true, true);

	return ret;
}

int mtk_drm_ioctl_set_color_reg(struct drm_device *dev, void *data,
		struct drm_file *file_priv)
{
	struct mtk_drm_private *private = dev->dev_private;
	struct drm_crtc *crtc = private->crtc[0];
	struct mtk_ddp_comp *comp = mtk_ddp_comp_sel_in_cur_crtc_path(
			to_mtk_crtc(crtc), MTK_DISP_COLOR, 0);

	return mtk_drm_ioctl_set_color_reg_impl(comp, data);
}

int mtk_color_cfg_mutex_control(struct mtk_ddp_comp *comp,
	struct cmdq_pkt *handle, void *data, unsigned int data_size)
{
	struct mtk_disp_color *color = comp_to_color(comp);
	struct mtk_disp_color_primary *primary_data = color->primary_data;
	int ret = 0;
	unsigned int *value = data;

	DDPINFO("%s...value:%d", __func__, *value);

	if (*value == 1) {
		primary_data->ncs_tuning_mode = 1;
		DDPINFO("ncs_tuning_mode = 1\n");
	} else if (*value == 2) {
		primary_data->ncs_tuning_mode = 0;
		DDPINFO("ncs_tuning_mode = 0\n");
	} else {
		DDPPR_ERR("DISP_IOCTL_MUTEX_CONTROL invalid control\n");
		return -EFAULT;
	}

	return ret;

}

int mtk_drm_ioctl_mutex_control_impl(struct mtk_ddp_comp *comp, void *data)
{
	int ret = 0;
	unsigned int *value = data;
	struct mtk_drm_crtc *mtk_crtc = comp->mtk_crtc;
	struct mtk_disp_color_primary *primary_data =
		comp_to_color(comp)->primary_data;

	DDPINFO("%s...value:%d", __func__, *value);

	if (*value == 1) {
		primary_data->ncs_tuning_mode = 1;
		DDPINFO("ncs_tuning_mode = 1\n");
	} else if (*value == 2) {
		primary_data->ncs_tuning_mode = 0;
		DDPINFO("ncs_tuning_mode = 0\n");
		mtk_crtc_check_trigger(mtk_crtc, true, true);
	} else {
		DDPPR_ERR("DISP_IOCTL_MUTEX_CONTROL invalid control\n");
		return -EFAULT;
	}

	return ret;
}

int mtk_drm_ioctl_mutex_control(struct drm_device *dev, void *data,
		struct drm_file *file_priv)
{
	struct mtk_drm_private *private = dev->dev_private;
	struct drm_crtc *crtc = private->crtc[0];
	struct mtk_ddp_comp *comp = mtk_ddp_comp_sel_in_cur_crtc_path(
			to_mtk_crtc(crtc), MTK_DISP_COLOR, 0);

	return mtk_drm_ioctl_mutex_control_impl(comp, data);
}

int mtk_drm_ioctl_read_sw_reg_impl(struct mtk_ddp_comp *comp, void *data)
{
	struct DISP_READ_REG *rParams = data;
	/* TODO: dual pipe */
	struct mtk_disp_color_primary *primary_data =
			comp_to_color(comp)->primary_data;
	unsigned int ret = 0;
	unsigned int reg_id = rParams->reg;
	struct resource res;

	if (reg_id >= SWREG_PQDS_DS_EN && reg_id <= SWREG_PQDS_GAIN_0) {
		ret = (unsigned int)primary_data->pq_ds_param.param
			[reg_id - SWREG_PQDS_DS_EN];
		DDPDBG("%s @ %d. ret = 0x%08x", __func__, __LINE__, ret);
		return ret;
	}
	if (reg_id >= SWREG_PQDC_BLACK_EFFECT_ENABLE &&
		reg_id <= SWREG_PQDC_DC_ENABLE) {
		ret = (unsigned int)primary_data->pq_dc_param.param
			[reg_id - SWREG_PQDC_BLACK_EFFECT_ENABLE];
		DDPDBG("%s @ %d. ret = 0x%08x", __func__, __LINE__, ret);
		return ret;
	}

	switch (reg_id) {
	case SWREG_COLOR_BASE_ADDRESS:
		{
			ret = comp->regs_pa;
			break;
		}

	case SWREG_GAMMA_BASE_ADDRESS:
		{
			ret = primary_data->gamma_comp->regs_pa;
			break;
		}

	case SWREG_AAL_BASE_ADDRESS:
		{
			ret = primary_data->aal_comp->regs_pa;
			break;
		}

#if defined(CCORR_SUPPORT)
	case SWREG_CCORR_BASE_ADDRESS:
		{
			ret = primary_data->ccorr_comp->regs_pa;
			break;
		}
#endif
	case SWREG_DISP_TDSHP_BASE_ADDRESS:
		{
			ret = primary_data->tdshp_comp->regs_pa;
			break;
		}
	case SWREG_MML_HDR_BASE_ADDRESS:
		{
			if (color_get_MML_HDR0_REG(&res))
				ret = res.start;
			break;
		}
	case SWREG_MML_AAL_BASE_ADDRESS:
		{
			if (color_get_MML_AAL0_REG(&res))
				ret = res.start;
			break;
		}
	case SWREG_MML_TDSHP_BASE_ADDRESS:
		{
			if (color_get_MML_TDSHP0_REG(&res))
				ret = res.start;
			break;
		}
	case SWREG_MML_COLOR_BASE_ADDRESS:
		{
			if (color_get_MML_COLOR0_REG(&res))
				ret = res.start;
			break;
		}
	case SWREG_TDSHP_BASE_ADDRESS:
		{
			if (color_get_TDSHP0_REG(&res))
				ret = res.start;
			break;
		}
	case SWREG_MDP_COLOR_BASE_ADDRESS:
		{
			if (color_get_MDP_COLOR0_REG(&res))
				ret = res.start;
			break;
		}
	case SWREG_COLOR_MODE:
		{
			ret = COLOR_MODE;
			break;
		}

	case SWREG_RSZ_BASE_ADDRESS:
		{
#if defined(SUPPORT_ULTRA_RESOLUTION)
			ret = MDP_RSZ0_PA_BASE;
#endif
			break;
		}

	case SWREG_MDP_RDMA_BASE_ADDRESS:
		{
			if (!color_get_MDP_HDR0_REG(&res) &&
				color_get_MDP_RDMA0_REG(&res))
				ret = res.start;
			break;
		}

	case SWREG_MDP_AAL_BASE_ADDRESS:
		{
			if (color_get_MDP_AAL0_REG(&res))
				ret = res.start;

			break;
		}

	case SWREG_MDP_HDR_BASE_ADDRESS:
		{
			if (color_get_MDP_HDR0_REG(&res))
				ret = res.start;
			break;
		}

	case SWREG_TDSHP_TUNING_MODE:
		{
			ret = (unsigned int)primary_data->tdshp_flag;
			break;
		}

	case SWREG_MIRAVISION_VERSION:
		{
			ret = MIRAVISION_VERSION;
			break;
		}

	case SWREG_SW_VERSION_VIDEO_DC:
		{
			ret = SW_VERSION_VIDEO_DC;
			break;
		}

	case SWREG_SW_VERSION_AAL:
		{
			ret = SW_VERSION_AAL;
			break;
		}

	default:
		DDPINFO("%s @ %d. ret = 0x%08x. unknown reg_id: 0x%08x",
				__func__, __LINE__, ret, reg_id);
		break;

	}

	rParams->val = ret;

	DDPDBG("%s @ %d. read sw reg 0x%x = 0x%x",
			__func__, __LINE__, rParams->reg,
		rParams->val);

	return ret;
}

int mtk_drm_ioctl_read_sw_reg(struct drm_device *dev, void *data,
		struct drm_file *file_priv)
{
	struct mtk_drm_private *private = dev->dev_private;
	struct drm_crtc *crtc = private->crtc[0];
	struct mtk_ddp_comp *comp = mtk_ddp_comp_sel_in_cur_crtc_path(
			to_mtk_crtc(crtc), MTK_DISP_COLOR, 0);

	return mtk_drm_ioctl_read_sw_reg_impl(comp, data);
}

int mtk_drm_ioctl_write_sw_reg_impl(struct mtk_ddp_comp *comp, void *data)
{
	struct DISP_WRITE_REG *wParams = data;
	//void __iomem *va = 0;
	//unsigned int pa;
	/* TODO: dual pipe */
	struct mtk_disp_color_primary *primary_data =
		comp_to_color(comp)->primary_data;
	int ret = 0;
	unsigned int reg_id = wParams->reg;
	unsigned int value = wParams->val;


	if (reg_id >= SWREG_PQDC_BLACK_EFFECT_ENABLE &&
		reg_id <= SWREG_PQDC_DC_ENABLE) {
		primary_data->pq_dc_param.param[reg_id - SWREG_PQDC_BLACK_EFFECT_ENABLE] =
			(int)value;
		DDPDBG("%s @ %d. value: 0x%08x", __func__, __LINE__, value);
		return ret;
	}

	if (reg_id >= SWREG_PQDS_DS_EN && reg_id <= SWREG_PQDS_GAIN_0) {
		primary_data->pq_ds_param.param[reg_id - SWREG_PQDS_DS_EN] = (int)value;
		DDPDBG("%s @ %d. value: 0x%08x", __func__, __LINE__, value);
		return ret;
	}

	switch (reg_id) {
	case SWREG_TDSHP_TUNING_MODE:
		{
			primary_data->tdshp_flag = (int)value;
			break;
		}
	case SWREG_MDP_COLOR_CAPTURE_EN:
		{
			primary_data->mdp_color_cap.en = value;
			break;
		}
	case SWREG_MDP_COLOR_CAPTURE_POS_X:
		{
			primary_data->mdp_color_cap.pos_x = value;
			break;
		}
	case SWREG_MDP_COLOR_CAPTURE_POS_Y:
		{
			primary_data->mdp_color_cap.pos_y = value;
			break;
		}
	case SWREG_TDSHP_GAIN_MID:
		{
			primary_data->tdshp_reg.TDS_GAIN_MID = value;
			break;
		}
	case SWREG_TDSHP_GAIN_HIGH:
		{
			primary_data->tdshp_reg.TDS_GAIN_HIGH = value;
			break;
		}
	case SWREG_TDSHP_COR_GAIN:
		{
			primary_data->tdshp_reg.TDS_COR_GAIN = value;
			break;
		}
	case SWREG_TDSHP_COR_THR:
		{
			primary_data->tdshp_reg.TDS_COR_THR = value;
			break;
		}
	case SWREG_TDSHP_COR_ZERO:
		{
			primary_data->tdshp_reg.TDS_COR_ZERO = value;
			break;
		}
	case SWREG_TDSHP_GAIN:
		{
			primary_data->tdshp_reg.TDS_GAIN = value;
			break;
		}
	case SWREG_TDSHP_COR_VALUE:
		{
			primary_data->tdshp_reg.TDS_COR_VALUE = value;
			break;
		}

	default:
		DDPINFO("%s @ %d. value = 0x%08x. unknown reg_id: 0x%08x",
				__func__, __LINE__, value, reg_id);
		break;

	}

	return ret;
}

int mtk_drm_ioctl_write_sw_reg(struct drm_device *dev, void *data,
		struct drm_file *file_priv)
{
	struct mtk_drm_private *private = dev->dev_private;
	struct drm_crtc *crtc = private->crtc[0];
	struct mtk_ddp_comp *comp = mtk_ddp_comp_sel_in_cur_crtc_path(
			to_mtk_crtc(crtc), MTK_DISP_COLOR, 0);

	return mtk_drm_ioctl_write_sw_reg_impl(comp, data);
}

int mtk_drm_ioctl_read_reg_impl(struct mtk_ddp_comp *comp, void *data)
{
	int ret = 0;
	struct DISP_READ_REG *rParams = data;
	void __iomem *va = 0;
	unsigned int pa;
	/* TODO: dual pipe */
	struct mtk_disp_color *color_data = comp_to_color(comp);
	unsigned long flags;

	pa = (unsigned int)rParams->reg;

	if (color_is_reg_addr_valid(comp, pa) < 0) {
		DDPPR_ERR("reg read, addr invalid, pa:0x%x\n", pa);
		return -EFAULT;
	}

	va = ioremap(pa, sizeof(va));

	DDPDBG("%s @ %d......... spin_trylock_irqsave ++ ",
		__func__, __LINE__);
	if (spin_trylock_irqsave(&color_data->clock_lock, flags)) {
		DDPDBG("%s @ %d......... spin_trylock_irqsave -- ",
			__func__, __LINE__);
		rParams->val = readl(va) & rParams->mask;

		spin_unlock_irqrestore(&color_data->clock_lock, flags);
	} else {
		DDPINFO("%s @ %d......... Failed to spin_trylock_irqsave ",
			__func__, __LINE__);
	}

	DDPINFO("read pa:0x%x(va:0x%lx) = 0x%x (0x%x)\n",
		pa,
		(long)va,
		rParams->val,
		rParams->mask);

	iounmap(va);

	return ret;
}

int mtk_drm_ioctl_read_reg(struct drm_device *dev, void *data,
		struct drm_file *file_priv)
{
	struct mtk_drm_private *private = dev->dev_private;
	struct drm_crtc *crtc = private->crtc[0];
	struct mtk_ddp_comp *comp = mtk_ddp_comp_sel_in_cur_crtc_path(
			to_mtk_crtc(crtc), MTK_DISP_COLOR, 0);

	return mtk_drm_ioctl_read_reg_impl(comp, data);
}

int mtk_drm_ioctl_write_reg_impl(struct mtk_ddp_comp *comp, void *data)
{
	struct mtk_drm_crtc *mtk_crtc = comp->mtk_crtc;
	struct DISP_WRITE_REG *wParams = data;
	unsigned int pa;

	pa = (unsigned int)wParams->reg;

	if (color_is_reg_addr_valid(comp, pa) < 0) {
		DDPPR_ERR("reg write, addr invalid, pa:0x%x\n", pa);
		return -EFAULT;
	}

	return mtk_crtc_user_cmd(&mtk_crtc->base, comp, WRITE_REG, data);
}

int mtk_drm_ioctl_write_reg(struct drm_device *dev, void *data,
		struct drm_file *file_priv)
{
	struct mtk_drm_private *private = dev->dev_private;
	struct drm_crtc *crtc = private->crtc[0];
	struct mtk_ddp_comp *comp = mtk_ddp_comp_sel_in_cur_crtc_path(
			to_mtk_crtc(crtc), MTK_DISP_COLOR, 0);

	return mtk_drm_ioctl_write_reg_impl(comp, data);
}

int mtk_color_cfg_bypass(struct mtk_ddp_comp *comp,
	struct cmdq_pkt *handle, void *data, unsigned int data_size)
{
	int ret = 0;
	unsigned int *value = data;
	struct mtk_disp_color *color = comp_to_color(comp);

	ddp_color_bypass_color(comp, *value, handle);
	if (comp->mtk_crtc->is_dual_pipe) {
		struct mtk_ddp_comp *comp_color1 = color->companion;

		ddp_color_bypass_color(comp_color1, *value, handle);
	}

	return ret;
}

int mtk_drm_ioctl_bypass_color_impl(struct mtk_ddp_comp *comp, void *data)
{
	int ret = 0;
	struct mtk_drm_crtc *mtk_crtc = comp->mtk_crtc;

	ret = mtk_crtc_user_cmd(&mtk_crtc->base, comp, BYPASS_COLOR, data);
	mtk_crtc_check_trigger(mtk_crtc, true, true);

	return ret;
}

int mtk_drm_ioctl_bypass_color(struct drm_device *dev, void *data,
		struct drm_file *file_priv)
{
	struct mtk_drm_private *private = dev->dev_private;
	struct drm_crtc *crtc = private->crtc[0];
	struct mtk_ddp_comp *comp = mtk_ddp_comp_sel_in_cur_crtc_path(
			to_mtk_crtc(crtc), MTK_DISP_COLOR, 0);

	return mtk_drm_ioctl_bypass_color_impl(comp, data);
}

int mtk_drm_color_cfg_pq_set_window(struct mtk_ddp_comp *comp,
	struct cmdq_pkt *handle, void *data, unsigned int data_size)
{
	int ret = 0;
	struct DISP_PQ_WIN_PARAM *win_param = data;
	struct mtk_disp_color *color = comp_to_color(comp);
	struct mtk_disp_color_primary *primary_data = color->primary_data;

	ddp_color_set_window(comp, win_param, handle);
	DDPINFO("%s..., id=%d, en=%d, x=0x%x, y=0x%x\n",
		__func__, comp->id, primary_data->split_en,
		((primary_data->split_window_x_end << 16) |
		 primary_data->split_window_x_start),
		((primary_data->split_window_y_end << 16) |
		 primary_data->split_window_y_start));

	if (comp->mtk_crtc->is_dual_pipe) {
		struct mtk_ddp_comp *comp_color1 = color->companion;

		ddp_color_set_window(comp_color1, win_param, handle);
		DDPINFO("%s..., id=%d, en=%d, x=0x%x, y=0x%x\n",
			__func__, comp_color1->id, primary_data->split_en,
			((primary_data->split_window_x_end << 16) |
			 primary_data->split_window_x_start),
			((primary_data->split_window_y_end << 16) |
			 primary_data->split_window_y_start));
	}

	return ret;
}

int mtk_drm_ioctl_pq_set_window_impl(struct mtk_ddp_comp *comp, void *data)
{
	int ret = 0;
	struct mtk_drm_crtc *mtk_crtc = comp->mtk_crtc;
	struct mtk_disp_color *color = comp_to_color(comp);
	struct mtk_disp_color_primary *primary =
		color->primary_data;
	struct DISP_PQ_WIN_PARAM *win_param = data;

	unsigned int split_window_x, split_window_y;

	/* save to global, can be applied on following PQ param updating. */
	if (win_param->split_en) {
		primary->split_en = 1;
		primary->split_window_x_start = win_param->start_x;
		primary->split_window_y_start = win_param->start_y;
		primary->split_window_x_end = win_param->end_x;
		primary->split_window_y_end = win_param->end_y;
	} else {
		primary->split_en = 0;
		primary->split_window_x_start = 0x0000;
		primary->split_window_y_start = 0x0000;
		primary->split_window_x_end = 0xFFFF;
		primary->split_window_y_end = 0xFFFF;
	}

	DDPINFO("%s: input: id[%d], en[%d], x[0x%x], y[0x%x]\n",
		__func__, comp->id, primary->split_en,
		((win_param->end_x << 16) | win_param->start_x),
		((win_param->end_y << 16) | win_param->start_y));

	ddp_color_cal_split_window(comp, &split_window_x, &split_window_y);

	DDPINFO("%s: output: x[0x%x], y[0x%x]", __func__,
		split_window_x, split_window_y);

	DDPINFO("%s..., id=%d, en=%d, x=0x%x, y=0x%x\n",
		__func__, comp->id, primary->split_en,
		((primary->split_window_x_end << 16) | primary->split_window_x_start),
		((primary->split_window_y_end << 16) | primary->split_window_y_start));

	ret = mtk_crtc_user_cmd(&mtk_crtc->base, comp, PQ_SET_WINDOW, data);
	if (comp->mtk_crtc->is_dual_pipe) {
		struct mtk_ddp_comp *comp_color1 = color->companion;

		ddp_color_cal_split_window(comp_color1, &split_window_x, &split_window_y);
		ret = mtk_crtc_user_cmd(&mtk_crtc->base, comp_color1, PQ_SET_WINDOW, data);
		DDPINFO("%s: output: x[0x%x], y[0x%x]", __func__,
			split_window_x, split_window_y);

		DDPINFO("%s..., id=%d, en=%d, x=0x%x, y=0x%x\n",
			__func__, comp_color1->id, primary->split_en,
			((primary->split_window_x_end << 16) | primary->split_window_x_start),
			((primary->split_window_y_end << 16) | primary->split_window_y_start));
	}
	mtk_crtc_check_trigger(mtk_crtc, true, true);

	return ret;
}

int mtk_drm_ioctl_pq_set_window(struct drm_device *dev, void *data,
		struct drm_file *file_priv)
{
	struct mtk_drm_private *private = dev->dev_private;
	struct drm_crtc *crtc = private->crtc[0];
	struct mtk_ddp_comp *comp = mtk_ddp_comp_sel_in_cur_crtc_path(
			to_mtk_crtc(crtc), MTK_DISP_COLOR, 0);

	return mtk_drm_ioctl_pq_set_window_impl(comp, data);
}

static int mtk_color_io_cmd(struct mtk_ddp_comp *comp, struct cmdq_pkt *handle,
							enum mtk_ddp_io_cmd cmd, void *params)
{
	switch (cmd) {
	case PQ_FILL_COMP_PIPE_INFO:
	{
		struct mtk_disp_color *data = comp_to_color(comp);
		bool *is_right_pipe = &data->is_right_pipe;
		int ret, *path_order = &data->path_order;
		struct mtk_ddp_comp **companion = &data->companion;
		struct mtk_disp_color *companion_data;

		DDPMSG("%s,color pipe info comp id(%d)\n", __func__, comp->id);

		if (data->is_right_pipe)
			break;
		ret = mtk_pq_helper_fill_comp_pipe_info(comp, path_order, is_right_pipe, companion);
		if (!ret && comp->mtk_crtc->is_dual_pipe && data->companion) {
			DDPMSG("%s,color dual pipe info comp id(%d)\n", __func__, comp->id);
			companion_data = comp_to_color(data->companion);
			companion_data->path_order = data->path_order;
			companion_data->is_right_pipe = !data->is_right_pipe;
			companion_data->companion = comp;
		}
	}
		break;
	default:
		break;
	}
	return 0;
}

static void mtk_color_start(struct mtk_ddp_comp *comp, struct cmdq_pkt *handle)
{
	//struct mtk_disp_color *color = comp_to_color(comp);
	struct mtk_disp_color_primary *primary_data =
		comp_to_color(comp)->primary_data;

	DpEngine_COLORonInit(comp, handle);

	mutex_lock(&primary_data->reg_lock);
	if (primary_data->color_reg_valid) {
		color_write_hw_reg(comp, &primary_data->color_reg, handle);
		mutex_unlock(&primary_data->reg_lock);
	} else {
		mutex_unlock(&primary_data->reg_lock);
		DpEngine_COLORonConfig(comp, handle);
	}
	/*
	 *cmdq_pkt_write(handle, comp->cmdq_base,
	 *	       comp->regs_pa + DISP_COLOR_CFG_MAIN,
	 *	       COLOR_BYPASS_ALL | COLOR_SEQ_SEL, ~0);
	 *cmdq_pkt_write(handle, comp->cmdq_base,
	 *	       comp->regs_pa + DISP_COLOR_START(color), 0x1, ~0);
	 */
}

static void mtk_color_stop(struct mtk_ddp_comp *comp, struct cmdq_pkt *handle)
{

}

static void mtk_color_bypass(struct mtk_ddp_comp *comp, int bypass,
	struct cmdq_pkt *handle)
{
	struct mtk_disp_color *color = comp_to_color(comp);

	DDPINFO("%s: bypass: %d\n", __func__, bypass);
	cmdq_pkt_write(handle, comp->cmdq_base,
		       comp->regs_pa + DISP_COLOR_CFG_MAIN,
		       COLOR_BYPASS_ALL | COLOR_SEQ_SEL, ~0);

	/* disable R2Y/Y2R in Color Wrapper */
	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + DISP_COLOR_CM1_EN(color), 0, 0x1);
	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + DISP_COLOR_CM2_EN(color), 0, 0x1);
	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + DISP_COLOR_START(color), 0x3, 0x3);

	/*
	 * writel(0, comp->regs + DISP_COLOR_CM1_EN);
	 * writel(0, comp->regs + DISP_COLOR_CM2_EN);
	 * writel(0x1, comp->regs + DISP_COLOR_START(color));
	 */
}

void disp_color_write_pos_main_for_dual_pipe(struct mtk_ddp_comp *comp,
	struct cmdq_pkt *handle, struct DISP_WRITE_REG *wParams,
	unsigned int pa, unsigned int pa1)
{
	unsigned int pos_x, pos_y, val, val1, mask;
	struct mtk_disp_color_primary *primary_data =
		comp_to_color(comp)->primary_data;

	val = wParams->val;
	mask = wParams->mask;
	pos_x = (wParams->val & 0xffff);
	pos_y = ((wParams->val & (0xffff0000)) >> 16);
	DDPINFO("write POS_MAIN: pos_x[%d] pos_y[%d]\n",
		pos_x, pos_y);
	if (pos_x < primary_data->width) {
		cmdq_pkt_write(handle, comp->cmdq_base,
			pa, val, mask);
		DDPINFO("dual pipe write pa:0x%x(va:0) = 0x%x (0x%x)\n"
			, pa, val, mask);
		val1 = ((pos_x + primary_data->width) | ((pos_y << 16)));
		cmdq_pkt_write(handle, comp->cmdq_base,
			pa1, val1, mask);
		DDPINFO("dual pipe write pa1:0x%x(va:0) = 0x%x (0x%x)\n"
			, pa1, val1, mask);
	} else {
		cmdq_pkt_write(handle, comp->cmdq_base,
			pa, val, mask);
		DDPINFO("dual pipe write pa:0x%x(va:0) = 0x%x (0x%x)\n"
			, pa, val, mask);
		val1 = ((pos_x - primary_data->width) | ((pos_y << 16)));
		cmdq_pkt_write(handle, comp->cmdq_base,
			pa1, val1, mask);
		DDPINFO("dual pipe write pa1:0x%x(va:0) = 0x%x (0x%x)\n"
			, pa1, val1, mask);
	}
}

static int mtk_color_cfg_drecolor_set_sgy(struct mtk_ddp_comp *comp,
	struct cmdq_pkt *handle, void *data, unsigned int data_size)
{
	struct mtk_disp_color *priv_data = comp_to_color(comp);
	struct mtk_disp_color_primary *primary_data =
			comp_to_color(comp)->primary_data;
	struct DISP_AAL_DRECOLOR_PARAM *param = data;
	struct DISP_AAL_DRECOLOR_PARAM *drecolor_sgy = &priv_data->primary_data->drecolor_sgy;

	if (sizeof(struct DISP_AAL_DRECOLOR_PARAM) < data_size) {
		DDPPR_ERR("%s param size error %lu, %u\n", __func__, sizeof(*param), data_size);
		return -EFAULT;
	}
	mutex_lock(&primary_data->reg_lock);
	memcpy(drecolor_sgy, param, sizeof(struct DISP_AAL_DRECOLOR_PARAM));
	if (!drecolor_sgy->sgy_trans_trigger) {
		DDPINFO("%s set skip\n", __func__);
		mutex_unlock(&primary_data->reg_lock);
		return 0;
	}
	DDPINFO("%s set now\n", __func__);
	disp_color_set_sgy(comp, handle, drecolor_sgy->sgy_out_gain);
	if (comp->mtk_crtc->is_dual_pipe)
		disp_color_set_sgy(priv_data->companion, handle, drecolor_sgy->sgy_out_gain);
	mutex_unlock(&primary_data->reg_lock);
	return 0;
}

static int mtk_color_pq_frame_config(struct mtk_ddp_comp *comp,
	struct cmdq_pkt *handle, unsigned int cmd, void *data, unsigned int data_size)
{
	int ret = -1;

	DDPINFO("%s,SET COLOR REG id(%d) cmd = %d\n", __func__, comp->id, cmd);
	/* will only call left path */
	switch (cmd) {
	/* TYPE1 no user cmd */
	case PQ_COLOR_MUTEX_CONTROL:
		/*set ncs mode*/
		ret = mtk_color_cfg_mutex_control(comp, handle, data, data_size);
		break;
	case PQ_COLOR_BYPASS:
		ret = mtk_color_cfg_bypass(comp, handle, data, data_size);
		break;
	case PQ_COLOR_SET_PQINDEX:
		/*just memcpy user data*/
		ret = mtk_drm_color_cfg_set_pqindex(comp, handle, data, data_size);
		break;
	case PQ_COLOR_SET_PQPARAM:
		ret = mtk_drm_color_cfg_set_pqparam(comp, handle, data, data_size);
		break;
	case PQ_COLOR_SET_COLOR_REG:
		ret = mtk_color_cfg_set_color_reg(comp, handle, data, data_size);
		break;
	case PQ_COLOR_SET_WINDOW:
		ret = mtk_drm_color_cfg_pq_set_window(comp, handle, data, data_size);
		break;
	case PQ_COLOR_DRECOLOR_SET_SGY:
		ret = mtk_color_cfg_drecolor_set_sgy(comp, handle, data, data_size);
		break;
	default:
		break;
	}
	return ret;
}

static int mtk_color_user_cmd(struct mtk_ddp_comp *comp,
	struct cmdq_pkt *handle, unsigned int cmd, void *data)
{
	struct mtk_disp_color *color = comp_to_color(comp);
	struct mtk_disp_color_primary *primary_data = color->primary_data;

	DDPINFO("%s: cmd: %d\n", __func__, cmd);
	switch (cmd) {
	case SET_PQPARAM:
	{
		/* normal mode */
		DpEngine_COLORonInit(comp, handle);
		DpEngine_COLORonConfig(comp, handle);
		if (comp->mtk_crtc->is_dual_pipe) {
			struct mtk_ddp_comp *comp_color1 = color->companion;

			DpEngine_COLORonInit(comp_color1, handle);
			DpEngine_COLORonConfig(comp_color1, handle);
		}
	}
	break;
	case SET_COLOR_REG:
	{
		mutex_lock(&primary_data->reg_lock);

		if (data != NULL) {
			memcpy(&primary_data->color_reg, (struct DISPLAY_COLOR_REG *)data,
				sizeof(struct DISPLAY_COLOR_REG));

			color_write_hw_reg(comp, &primary_data->color_reg, handle);
			if (comp->mtk_crtc->is_dual_pipe) {
				struct mtk_ddp_comp *comp_color1 = color->companion;

				color_write_hw_reg(comp_color1, &primary_data->color_reg, handle);
			}
		} else {
			DDPINFO("%s: data is NULL", __func__);
		}

		primary_data->color_reg_valid = 1;
		mutex_unlock(&primary_data->reg_lock);
	}
	break;
	case WRITE_REG:
	{
		struct DISP_WRITE_REG *wParams = data;
		void __iomem *va = 0;
		unsigned int pa = (unsigned int)wParams->reg;

		if (comp->mtk_crtc->is_dual_pipe) {
			int tablet_index = -1;
			unsigned int offset = 0;
			struct resource res;
			unsigned int pa1 = 0;

			tablet_index = get_tuning_reg_table_idx_and_offset(comp, pa, &offset);
			do {
				if (tablet_index == TUNING_DISP_COLOR) {
					if (color_get_DISP1_COLOR0_REG(&res))
						pa1 =  res.start + offset;
					if (offset == DISP_COLOR_POS_MAIN) {
						disp_color_write_pos_main_for_dual_pipe(comp,
							handle, wParams, pa, pa1);
						break;
					}
				} else if (tablet_index == TUNING_DISP_CCORR) {
					if (color_get_DISP1_CCORR0_REG(&res))
						pa1 = res.start + offset;
				} else if (tablet_index == TUNING_DISP_AAL) {
					if (color_get_DISP1_AAL0_REG(&res))
						pa1 =  res.start + offset;
				} else if (tablet_index == TUNING_DISP_GAMMA) {
					if (color_get_DISP1_GAMMA0_REG(&res))
						pa1 =  res.start + offset;
				} else if (tablet_index == TUNING_DISP_DITHER) {
					if (color_get_DISP1_DITHER0_REG(&res))
						pa1 =  res.start + offset;
				} else if (tablet_index == TUNING_DISP_TDSHP) {
					if (color_get_DISP1_TDSHP0_REG(&res))
						pa1 =  res.start + offset;
				} else if (tablet_index == TUNING_DISP_C3D) {
					if (color_get_DISP1_C3D0_REG(&res))
						pa1 =  res.start + offset;
				} else if (tablet_index == TUNING_DISP_CCORR1) {
					if (color_get_DISP1_CCORR1_REG(&res))
						pa1 = res.start + offset;
				} else if (tablet_index == TUNING_DISP_MDP_AAL) {
					if (color_get_DISP1_DMDP_AAL0_REG(&res))
						pa1 =  res.start + offset;
				} else if (tablet_index == TUNING_DISP_ODDMR_TOP) {
					if (color_get_DISP1_ODDMR0_REG(&res))
						pa1 =  res.start + offset;
				} else if (tablet_index == TUNING_DISP_ODDMR_OD) {
					if (color_get_DISP1_ODDMR0_REG(&res))
						pa1 =  res.start + offset + 0x1000;
				}
				if (pa) {
					cmdq_pkt_write(handle, comp->cmdq_base,
						pa, wParams->val, wParams->mask);
				}
				DDPINFO("dual pipe write pa:0x%x(va:0x%lx) = 0x%x (0x%x)\n",
					pa, (long)va, wParams->val, wParams->mask);
				if (pa1) {
					cmdq_pkt_write(handle, comp->cmdq_base,
						pa1, wParams->val, wParams->mask);
				}
				DDPINFO("dual pipe write pa1:0x%x(va:0x%lx) = 0x%x (0x%x)\n",
					pa1, (long)va, wParams->val, wParams->mask);
			} while (0);
		} else {
			if (pa) {
				cmdq_pkt_write(handle, comp->cmdq_base,
					pa, wParams->val, wParams->mask);
			}
			DDPINFO("single pipe write pa:0x%x(va:0x%lx) = 0x%x (0x%x)\n",
				pa, (long)va, wParams->val, wParams->mask);
		}
	}
	break;
	case BYPASS_COLOR:
	{
		unsigned int *value = data;

		ddp_color_bypass_color(comp, *value, handle);
		if (comp->mtk_crtc->is_dual_pipe) {
			struct mtk_ddp_comp *comp_color1 = color->companion;

			ddp_color_bypass_color(comp_color1, *value, handle);
		}
	}
	break;
	case PQ_SET_WINDOW:
	{
		struct DISP_PQ_WIN_PARAM *win_param = data;

		ddp_color_set_window(comp, win_param, handle);
	}
	break;
	default:
		DDPPR_ERR("%s: error cmd: %d\n", __func__, cmd);
		return -EINVAL;
	}
	return 0;
}

static void ddp_color_backup(struct mtk_ddp_comp *comp)
{
	struct mtk_disp_color_primary *primary_data =
		comp_to_color(comp)->primary_data;

	primary_data->color_backup.COLOR_CFG_MAIN =
		readl(comp->regs + DISP_COLOR_CFG_MAIN);
}

static void ddp_color_restore(struct mtk_ddp_comp *comp)
{
	struct mtk_disp_color_primary *primary_data =
		comp_to_color(comp)->primary_data;

	writel(primary_data->color_backup.COLOR_CFG_MAIN, comp->regs + DISP_COLOR_CFG_MAIN);
}

static void mtk_color_prepare(struct mtk_ddp_comp *comp)
{
	struct mtk_disp_color *color = comp_to_color(comp);

	mtk_ddp_comp_clk_prepare(comp);
	atomic_set(&color->color_is_clock_on, 1);

	/* Bypass shadow register and read shadow register */
	if (color->data->need_bypass_shadow)
		mtk_ddp_write_mask_cpu(comp, COLOR_BYPASS_SHADOW,
			DISP_COLOR_SHADOW_CTRL, COLOR_BYPASS_SHADOW);

	// restore DISP_COLOR_CFG_MAIN register
	ddp_color_restore(comp);
}

static void mtk_color_unprepare(struct mtk_ddp_comp *comp)
{
	struct mtk_disp_color *color_data = comp_to_color(comp);
	unsigned long flags;

	DDPINFO("%s @ %d......... spin_lock_irqsave ++ ", __func__, __LINE__);
	spin_lock_irqsave(&color_data->clock_lock, flags);
	DDPINFO("%s @ %d......... spin_lock_irqsave -- ", __func__, __LINE__);
	atomic_set(&color_data->color_is_clock_on, 0);
	spin_unlock_irqrestore(&color_data->clock_lock, flags);
	DDPINFO("%s @ %d......... spin_unlock_irqrestore ", __func__, __LINE__);
	// backup DISP_COLOR_CFG_MAIN register
	ddp_color_backup(comp);
	mtk_ddp_comp_clk_unprepare(comp);
}

static void mtk_color_data_init(struct mtk_ddp_comp *comp)
{
	struct mtk_disp_color *color_data = comp_to_color(comp);

	spin_lock_init(&color_data->clock_lock);
}

static void mtk_color_primary_data_init(struct mtk_ddp_comp *comp)
{
	int i;
	struct mtk_drm_crtc *mtk_crtc = comp->mtk_crtc;
	struct mtk_disp_color *color_data = comp_to_color(comp);
	struct mtk_disp_color *companion_data = comp_to_color(color_data->companion);
	struct mtk_disp_color_primary *primary_data = color_data->primary_data;
	struct DISP_PQ_DC_PARAM dc_param_init = {
param:
			{1, 1, 0, 0, 0, 0, 0, 0, 0, 0x0A,
			 0x30, 0x40, 0x06, 0x12, 40, 0x40, 0x80, 0x40, 0x40, 1,
			 0x80, 0x60, 0x80, 0x10, 0x34, 0x40, 0x40, 1, 0x80, 0xa,
			 0x19, 0x00, 0x20, 0, 0, 1, 2, 1, 80, 1}
		};
	struct DISP_PQ_DS_PARAM ds_param_init = {
param:
			{1, -4, 1024, -4, 1024, 1, 400, 200, 1600, 800, 128, 8,
			 4, 12, 16, 8, 24, -8, -4, -12, 0, 0, 0}
		};
	/* initialize index */
	/* (because system default is 0, need fill with 0x80) */
	struct DISPLAY_PQ_T color_index_init = {
GLOBAL_SAT:	/* 0~9 */
			{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
CONTRAST :	/* 0~9 */
			{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
BRIGHTNESS :	/* 0~9 */
			{0x400, 0x400, 0x400, 0x400, 0x400, 0x400, 0x400, 0x400, 0x400, 0x400},
PARTIAL_Y :
			{
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
				 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
				 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
				 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
				 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
				 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
				 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
				 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
				 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
				 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
				 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80}
			},
PURP_TONE_S :
		{			/* hue 0~10 */
			{			/* 0 disable */
				{0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80}
			},
			{			/* 1 */
				{0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80}
			},
			{			/* 2 */
				{0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80}
			},
			{			/* 3 */
				{0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80}
			},
			{			/* 4 */
				{0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80}
			},
			{			/* 5 */
				{0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80}
			},
			{			/* 6 */
				{0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80}
			},
			{			/* 7 */
				{0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80}
			},
			{			/* 8 */
				{0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80}
			},
			{			/* 9 */
				{0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80}
			},
			{			/* 10 */
				{0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80}
			},
			{			/* 11 */
				{0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80}
			},
			{			/* 12 */
				{0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80}
			},
			{			/* 13 */
				{0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80}
			},
			{			/* 14 */
				{0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80}
			},
			{			/* 15 */
				{0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80}
			},
			{			/* 16 */
				{0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80}
			},
			{			/* 17 */
				{0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80}
			},
			{			/* 18 */
				{0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80}
			}
		},
SKIN_TONE_S :
		{
			{			/* 0 disable */
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80}
			},
			{			/* 1 */
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80}
			},
			{			/* 2 */
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80}
			},
			{			/* 3 */
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80}
			},
			{			/* 4 */
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80}
			},
			{			/* 5 */
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80}
			},
			{			/* 6 */
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80}
			},
			{			/* 7 */
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80}
			},
			{			/* 8 */
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80}
			},
			{			/* 9 */
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80}
			},
			{			/* 10 */
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80}
			},
			{			/* 11 */
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80}
			},
			{			/* 12 */
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80}
			},
			{			/* 13 */
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80}
			},
			{			/* 14 */
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80}
			},
			{			/* 15 */
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80}
			},
			{			/* 16 */
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80}
			},
			{			/* 17 */
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80}
			},
			{			/* 18 */
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80}
			}
		},
GRASS_TONE_S :
		{
			{			/* 0 disable */
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80}
			},
			{			/* 1 */
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80}
			},
			{			/* 2 */
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80}
			},
			{			/* 3 */
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80}
			},
			{			/* 4 */
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80}
			},
			{			/* 5 */
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80}
			},
			{			/* 6 */
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80}
			},
			{			/* 7 */
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80}
			},
			{			/* 8 */
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80}
			},
			{			/* 9 */
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80}
			},
			{			/* 10 */
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80}
			},
			{			/* 11 */
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80}
			},
			{			/* 12 */
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80}
			},
			{			/* 13 */
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80}
			},
			{			/* 14 */
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80}
			},
			{			/* 15 */
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80}
			},
			{			/* 16 */
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80}
			},
			{			/* 17 */
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80}
			},
			{			/* 18 */
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80}
			}
		},
SKY_TONE_S :
		{			/* hue 0~10 */
			{			/* 0 disable */
				{0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80}
			},
			{			/* 1 */
				{0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80}
			},
			{			/* 2 */
				{0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80}
			},
			{			/* 3 */
				{0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80}
			},
			{			/* 4 */
				{0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80}
			},
			{			/* 5 */
				{0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80}
			},
			{			/* 6 */
				{0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80}
			},
			{			/* 7 */
				{0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80}
			},
			{			/* 8 */
				{0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80}
			},
			{			/* 9 */
				{0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80}
			},
			{			/* 10 */
				{0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80}
			},
			{			/* 11 */
				{0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80}
			},
			{			/* 12 */
				{0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80}
			},
			{			/* 13 */
				{0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80}
			},
			{			/* 14 */
				{0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80}
			},
			{			/* 15 */
				{0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80}
			},
			{			/* 16 */
				{0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80}
			},
			{			/* 17 */
				{0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80}
			},
			{			/* 18 */
				{0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80}
			}
		},
PURP_TONE_H :
		{
			/* hue 0~2 */
			{0x80, 0x80, 0x80},	/* 3 */
			{0x80, 0x80, 0x80},	/* 4 */
			{0x80, 0x80, 0x80},
			{0x80, 0x80, 0x80},	/* 3 */
			{0x80, 0x80, 0x80},	/* 4 */
			{0x80, 0x80, 0x80},
			{0x80, 0x80, 0x80},
			{0x80, 0x80, 0x80},
			{0x80, 0x80, 0x80},
			{0x80, 0x80, 0x80},
			{0x80, 0x80, 0x80},	/* 4 */
			{0x80, 0x80, 0x80},
			{0x80, 0x80, 0x80},	/* 3 */
			{0x80, 0x80, 0x80},	/* 4 */
			{0x80, 0x80, 0x80},
			{0x80, 0x80, 0x80},
			{0x80, 0x80, 0x80},
			{0x80, 0x80, 0x80},
			{0x80, 0x80, 0x80}
		},
SKIN_TONE_H :
		{
			/* hue 3~16 */
			{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
			{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
			{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
			{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
			{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
			{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
			{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
			{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
			{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
			{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
			{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
			{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
			{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
			{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
			{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
			{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
			{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
			{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
			{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80}
		},
GRASS_TONE_H :
		{
		/* hue 17~24 */
			{0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
			{0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
			{0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
			{0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
			{0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
			{0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
			{0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
			{0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
			{0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
			{0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
			{0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
			{0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
			{0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
			{0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
			{0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
			{0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
			{0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
			{0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
			{0x80, 0x80, 0x80, 0x80, 0x80, 0x80}
		},
SKY_TONE_H :
		{
			{0x80, 0x80, 0x80},
			{0x80, 0x80, 0x80},
			{0x80, 0x80, 0x80},
			{0x80, 0x80, 0x80},
			{0x80, 0x80, 0x80},
			{0x80, 0x80, 0x80},
			{0x80, 0x80, 0x80},
			{0x80, 0x80, 0x80},
			{0x80, 0x80, 0x80},
			{0x80, 0x80, 0x80},
			{0x80, 0x80, 0x80},
			{0x80, 0x80, 0x80},
			{0x80, 0x80, 0x80},
			{0x80, 0x80, 0x80},
			{0x80, 0x80, 0x80},
			{0x80, 0x80, 0x80},
			{0x80, 0x80, 0x80},
			{0x80, 0x80, 0x80},
			{0x80, 0x80, 0x80}
		},
CCORR_COEF : /* ccorr feature */
		{
			{
				{0x400, 0x0, 0x0},
				{0x0, 0x400, 0x0},
				{0x0, 0x0, 0x400},
			},
			{
				{0x400, 0x0, 0x0},
				{0x0, 0x400, 0x0},
				{0x0, 0x0, 0x400},
			},
			{
				{0x400, 0x0, 0x0},
				{0x0, 0x400, 0x0},
				{0x0, 0x0, 0x400},
			},
			{
				{0x400, 0x0, 0x0},
				{0x0, 0x400, 0x0},
				{0x0, 0x0, 0x400}
			}
		},
S_GAIN_BY_Y :
		{
			{0x80, 0x80, 0x80, 0x80,
			 0x80, 0x80, 0x80, 0x80,
			 0x80, 0x80, 0x80, 0x80,
			 0x80, 0x80, 0x80, 0x80,
			 0x80, 0x80, 0x80, 0x80
			},
			{0x80, 0x80, 0x80, 0x80,
			 0x80, 0x80, 0x80, 0x80,
			 0x80, 0x80, 0x80, 0x80,
			 0x80, 0x80, 0x80, 0x80,
			 0x80, 0x80, 0x80, 0x80
			},
			{0x80, 0x80, 0x80, 0x80,
			 0x80, 0x80, 0x80, 0x80,
			 0x80, 0x80, 0x80, 0x80,
			 0x80, 0x80, 0x80, 0x80,
			 0x80, 0x80, 0x80, 0x80
			},
			{0x80, 0x80, 0x80, 0x80,
			 0x80, 0x80, 0x80, 0x80,
			 0x80, 0x80, 0x80, 0x80,
			 0x80, 0x80, 0x80, 0x80,
			 0x80, 0x80, 0x80, 0x80
			},
			{0x80, 0x80, 0x80, 0x80,
			 0x80, 0x80, 0x80, 0x80,
			 0x80, 0x80, 0x80, 0x80,
			 0x80, 0x80, 0x80, 0x80,
			 0x80, 0x80, 0x80, 0x80
			}
		},
S_GAIN_BY_Y_EN:0,
LSP_EN:0,
LSP :
		{0x0, 0x0, 0x7F, 0x7F, 0x7F, 0x0, 0x7F, 0x7F},
COLOR_3D :
		{
			{			/* 0 */
				/* Windows  1 */
				{ 0x80,  0x80,  0x80,  0x80,  0x80,  0x80, 0x3FF, 0x3FF,
				 0x000, 0x050, 0x100, 0x200, 0x300, 0x350, 0x3FF,
				  0x80,  0x80,  0x80,  0x80,  0x80,  0x80, 0x3FF, 0x3FF,
				 0x000, 0x050, 0x100, 0x200, 0x300, 0x350, 0x3FF,
				  0x80,  0x80,  0x80,  0x80,  0x80,  0x80, 0x3FF, 0x3FF,
				 0x000, 0x050, 0x100, 0x200, 0x300, 0x350, 0x3FF},
				/* Windows  2 */
				{ 0x80,  0x80,  0x80,  0x80,  0x80,  0x80, 0x3FF, 0x3FF,
				 0x000, 0x050, 0x100, 0x200, 0x300, 0x350, 0x3FF,
				  0x80,  0x80,  0x80,  0x80,  0x80,  0x80, 0x3FF, 0x3FF,
				 0x000, 0x050, 0x100, 0x200, 0x300, 0x350, 0x3FF,
				  0x80,  0x80,  0x80,  0x80,  0x80,  0x80, 0x3FF, 0x3FF,
				 0x000, 0x050, 0x100, 0x200, 0x300, 0x350, 0x3FF},
				/* Windows  3 */
				{ 0x80,  0x80,  0x80,  0x80,  0x80,  0x80, 0x3FF, 0x3FF,
				 0x000, 0x050, 0x100, 0x200, 0x300, 0x350, 0x3FF,
				  0x80,  0x80,  0x80,  0x80,  0x80,  0x80, 0x3FF, 0x3FF,
				 0x000, 0x050, 0x100, 0x200, 0x300, 0x350, 0x3FF,
				  0x80,  0x80,  0x80,  0x80,  0x80,  0x80, 0x3FF, 0x3FF,
				 0x000, 0x050, 0x100, 0x200, 0x300, 0x350, 0x3FF},
			},
			{			/* 1 */
				/* Windows  1 */
				{ 0x80,  0x80,  0x80,  0x80,  0x80,  0x80, 0x3FF, 0x3FF,
				 0x000, 0x050, 0x100, 0x200, 0x300, 0x350, 0x3FF,
				  0x80,  0x80,  0x80,  0x80,  0x80,  0x80, 0x3FF, 0x3FF,
				 0x000, 0x050, 0x100, 0x200, 0x300, 0x350, 0x3FF,
				  0x80,  0x80,  0x80,  0x80,  0x80,  0x80, 0x3FF, 0x3FF,
				 0x000, 0x050, 0x100, 0x200, 0x300, 0x350, 0x3FF},
				/* Windows  2 */
				{ 0x80,  0x80,  0x80,  0x80,  0x80,  0x80, 0x3FF, 0x3FF,
				 0x000, 0x050, 0x100, 0x200, 0x300, 0x350, 0x3FF,
				  0x80,  0x80,  0x80,  0x80,  0x80,  0x80, 0x3FF, 0x3FF,
				 0x000, 0x050, 0x100, 0x200, 0x300, 0x350, 0x3FF,
				  0x80,  0x80,  0x80,  0x80,  0x80,  0x80, 0x3FF, 0x3FF,
				 0x000, 0x050, 0x100, 0x200, 0x300, 0x350, 0x3FF},
				/* Windows  3 */
				{ 0x80,  0x80,  0x80,  0x80,  0x80,  0x80, 0x3FF, 0x3FF,
				 0x000, 0x050, 0x100, 0x200, 0x300, 0x350, 0x3FF,
				  0x80,  0x80,  0x80,  0x80,  0x80,  0x80, 0x3FF, 0x3FF,
				 0x000, 0x050, 0x100, 0x200, 0x300, 0x350, 0x3FF,
				  0x80,  0x80,  0x80,  0x80,  0x80,  0x80, 0x3FF, 0x3FF,
				 0x000, 0x050, 0x100, 0x200, 0x300, 0x350, 0x3FF},
			},
			{			/* 2 */
				/* Windows  1 */
				{ 0x80,  0x80,  0x80,  0x80,  0x80,  0x80, 0x3FF, 0x3FF,
				 0x000, 0x050, 0x100, 0x200, 0x300, 0x350, 0x3FF,
				  0x80,  0x80,  0x80,  0x80,  0x80,  0x80, 0x3FF, 0x3FF,
				 0x000, 0x050, 0x100, 0x200, 0x300, 0x350, 0x3FF,
				  0x80,  0x80,  0x80,  0x80,  0x80,  0x80, 0x3FF, 0x3FF,
				 0x000, 0x050, 0x100, 0x200, 0x300, 0x350, 0x3FF},
				/* Windows  2 */
				{ 0x80,  0x80,  0x80,  0x80,  0x80,  0x80, 0x3FF, 0x3FF,
				 0x000, 0x050, 0x100, 0x200, 0x300, 0x350, 0x3FF,
				  0x80,  0x80,  0x80,  0x80,  0x80,  0x80, 0x3FF, 0x3FF,
				 0x000, 0x050, 0x100, 0x200, 0x300, 0x350, 0x3FF,
				  0x80,  0x80,  0x80,  0x80,  0x80,  0x80, 0x3FF, 0x3FF,
				 0x000, 0x050, 0x100, 0x200, 0x300, 0x350, 0x3FF},
				/* Windows  3 */
				{ 0x80,  0x80,  0x80,  0x80,  0x80,  0x80, 0x3FF, 0x3FF,
				 0x000, 0x050, 0x100, 0x200, 0x300, 0x350, 0x3FF,
				  0x80,  0x80,  0x80,  0x80,  0x80,  0x80, 0x3FF, 0x3FF,
				 0x000, 0x050, 0x100, 0x200, 0x300, 0x350, 0x3FF,
				  0x80,  0x80,  0x80,  0x80,  0x80,  0x80, 0x3FF, 0x3FF,
				 0x000, 0x050, 0x100, 0x200, 0x300, 0x350, 0x3FF},
			},
			{			/* 3 */
				/* Windows  1 */
				{ 0x80,  0x80,  0x80,  0x80,  0x80,  0x80, 0x3FF, 0x3FF,
				 0x000, 0x050, 0x100, 0x200, 0x300, 0x350, 0x3FF,
				  0x80,  0x80,  0x80,  0x80,  0x80,  0x80, 0x3FF, 0x3FF,
				 0x000, 0x050, 0x100, 0x200, 0x300, 0x350, 0x3FF,
				  0x80,  0x80,  0x80,  0x80,  0x80,  0x80, 0x3FF, 0x3FF,
				 0x000, 0x050, 0x100, 0x200, 0x300, 0x350, 0x3FF},
				/* Windows  2 */
				{ 0x80,  0x80,  0x80,  0x80,  0x80,  0x80, 0x3FF, 0x3FF,
				 0x000, 0x050, 0x100, 0x200, 0x300, 0x350, 0x3FF,
				  0x80,  0x80,  0x80,  0x80,  0x80,  0x80, 0x3FF, 0x3FF,
				 0x000, 0x050, 0x100, 0x200, 0x300, 0x350, 0x3FF,
				  0x80,  0x80,  0x80,  0x80,  0x80,  0x80, 0x3FF, 0x3FF,
				 0x000, 0x050, 0x100, 0x200, 0x300, 0x350, 0x3FF},
				/* Windows  3 */
				{ 0x80,  0x80,  0x80,  0x80,  0x80,  0x80, 0x3FF, 0x3FF,
				 0x000, 0x050, 0x100, 0x200, 0x300, 0x350, 0x3FF,
				  0x80,  0x80,  0x80,  0x80,  0x80,  0x80, 0x3FF, 0x3FF,
				 0x000, 0x050, 0x100, 0x200, 0x300, 0x350, 0x3FF,
				  0x80,  0x80,  0x80,  0x80,  0x80,  0x80, 0x3FF, 0x3FF,
				 0x000, 0x050, 0x100, 0x200, 0x300, 0x350, 0x3FF},
			},
		}
	};


	if (color_data->is_right_pipe) {
		kfree(color_data->primary_data);
		color_data->primary_data = companion_data->primary_data;
		return;
	}
	primary_data->gamma_comp = mtk_ddp_comp_sel_in_cur_crtc_path(
			mtk_crtc, MTK_DISP_GAMMA, 0);
	primary_data->aal_comp = mtk_ddp_comp_sel_in_cur_crtc_path(
			mtk_crtc, MTK_DISP_AAL, 0);
	primary_data->tdshp_comp = mtk_ddp_comp_sel_in_cur_crtc_path(
			mtk_crtc, MTK_DISP_TDSHP, 0);
	primary_data->ccorr_comp = mtk_ddp_comp_sel_in_cur_crtc_path(
			mtk_crtc, MTK_DISP_CCORR, 1);
	if (primary_data->ccorr_comp == NULL)
		primary_data->ccorr_comp = mtk_ddp_comp_sel_in_cur_crtc_path(
				mtk_crtc, MTK_DISP_CCORR, 0);
	primary_data->legacy_color_cust = false;
	primary_data->color_param.u4SHPGain = 2;
	primary_data->color_param.u4SatGain = 4;
	for (i = 0; i < PQ_HUE_ADJ_PHASE_CNT; i++)
		primary_data->color_param.u4HueAdj[i] = 9;
	primary_data->color_param.u4Contrast = 4;
	primary_data->color_param.u4Brightness = 4;
	primary_data->split_window_x_end = 0xFFFF;
	primary_data->split_window_y_end = 0xFFFF;
	memcpy(&primary_data->pq_dc_param, &dc_param_init,
			sizeof(struct DISP_PQ_DC_PARAM));
	memcpy(&primary_data->pq_ds_param, &ds_param_init,
			sizeof(struct DISP_PQ_DS_PARAM));
	primary_data->tdshp_reg.TDS_GAIN_MID = 0x10;
	primary_data->tdshp_reg.TDS_GAIN_HIGH = 0x20;
	primary_data->tdshp_reg.TDS_COR_GAIN = 0x10;
	primary_data->tdshp_reg.TDS_COR_THR = 0x4;
	primary_data->tdshp_reg.TDS_COR_ZERO = 0x2;
	primary_data->tdshp_reg.TDS_GAIN = 0x20;
	primary_data->tdshp_reg.TDS_COR_VALUE = 0x3;
	memcpy(&primary_data->color_index, &color_index_init,
			sizeof(struct DISPLAY_PQ_T));
	mutex_init(&primary_data->reg_lock);
}

void mtk_color_first_cfg(struct mtk_ddp_comp *comp,
	       struct mtk_ddp_config *cfg, struct cmdq_pkt *handle)
{
	mtk_color_primary_data_init(comp);
	mtk_color_config(comp, cfg, handle);
}

static int mtk_color_ioctl_transact(struct mtk_ddp_comp *comp,
		unsigned int cmd, void *data, unsigned int data_size)
{
	int ret = -1;

	switch (cmd) {
	case PQ_COLOR_SET_PQPARAM:
		ret = mtk_drm_ioctl_set_pqparam_impl(comp, data);
		break;
	case PQ_COLOR_SET_PQINDEX:
		ret = mtk_drm_ioctl_set_pqindex_impl(comp, data);
		break;
	case PQ_COLOR_SET_COLOR_REG:
		ret = mtk_drm_ioctl_set_color_reg_impl(comp, data);
		break;
	case PQ_COLOR_MUTEX_CONTROL:
		ret = mtk_drm_ioctl_mutex_control_impl(comp, data);
		break;
	case PQ_COLOR_READ_REG:
		ret = mtk_drm_ioctl_read_reg_impl(comp, data);
		break;
	case PQ_COLOR_WRITE_REG:
		ret = mtk_drm_ioctl_write_reg_impl(comp, data);
		break;
	case PQ_COLOR_BYPASS:
		ret = mtk_drm_ioctl_bypass_color_impl(comp, data);
		break;
	case PQ_COLOR_SET_WINDOW:
		ret = mtk_drm_ioctl_pq_set_window_impl(comp, data);
		break;
	case PQ_COLOR_READ_SW_REG:
		ret = mtk_drm_ioctl_read_sw_reg_impl(comp, data);
		break;
	case PQ_COLOR_WRITE_SW_REG:
		ret = mtk_drm_ioctl_write_sw_reg_impl(comp, data);
		break;
	default:
		break;
	}
	return ret;
}

static const struct mtk_ddp_comp_funcs mtk_disp_color_funcs = {
	.config = mtk_color_config,
	.first_cfg = mtk_color_first_cfg,
	.start = mtk_color_start,
	.stop = mtk_color_stop,
	.bypass = mtk_color_bypass,
	.user_cmd = mtk_color_user_cmd,
	.prepare = mtk_color_prepare,
	.unprepare = mtk_color_unprepare,
	.config_overhead = mtk_disp_color_config_overhead,
	.io_cmd = mtk_color_io_cmd,
	.pq_frame_config = mtk_color_pq_frame_config,
	.pq_ioctl_transact = mtk_color_ioctl_transact,
};

void mtk_color_dump(struct mtk_ddp_comp *comp)
{
	void __iomem *baddr = comp->regs;

	if (!baddr) {
		DDPDUMP("%s, %s is NULL!\n", __func__, mtk_dump_comp_str(comp));
		return;
	}

	DDPDUMP("== %s REGS:0x%llx ==\n", mtk_dump_comp_str(comp), comp->regs_pa);
	mtk_serial_dump_reg(baddr, 0x400, 3);
	mtk_serial_dump_reg(baddr, 0xC50, 2);
}

void mtk_color_regdump(struct mtk_ddp_comp *comp)
{
	struct mtk_disp_color *color = comp_to_color(comp);
	void __iomem *baddr = comp->regs;
	int k;

	DDPDUMP("== %s REGS:0x%llx ==\n", mtk_dump_comp_str(comp),
			comp->regs_pa);
	DDPDUMP("[%s REGS Start Dump]\n", mtk_dump_comp_str(comp));
	for (k = 0x400; k <= 0xd5c; k += 16) {
		DDPDUMP("0x%04x: 0x%08x 0x%08x 0x%08x 0x%08x\n", k,
			readl(baddr + k),
			readl(baddr + k + 0x4),
			readl(baddr + k + 0x8),
			readl(baddr + k + 0xc));
	}
	DDPDUMP("[%s REGS End Dump]\n", mtk_dump_comp_str(comp));
	if (comp->mtk_crtc->is_dual_pipe && color->companion) {
		baddr = color->companion->regs;
		DDPDUMP("== %s REGS:0x%llx ==\n", mtk_dump_comp_str(color->companion),
				color->companion->regs_pa);
		DDPDUMP("[%s REGS Start Dump]\n", mtk_dump_comp_str(color->companion));
		for (k = 0x400; k <= 0xd5c; k += 16) {
			DDPDUMP("0x%04x: 0x%08x 0x%08x 0x%08x 0x%08x\n", k,
				readl(baddr + k),
				readl(baddr + k + 0x4),
				readl(baddr + k + 0x8),
				readl(baddr + k + 0xc));
		}
		DDPDUMP("[%s REGS End Dump]\n", mtk_dump_comp_str(color->companion));
	}
}

static int mtk_disp_color_bind(struct device *dev, struct device *master,
			       void *data)
{
	struct mtk_disp_color *priv = dev_get_drvdata(dev);
	struct drm_device *drm_dev = data;
	int ret;

	ret = mtk_ddp_comp_register(drm_dev, &priv->ddp_comp);
	if (ret < 0) {
		dev_err(dev, "Failed to register component %s: %d\n",
			dev->of_node->full_name, ret);
		return ret;
	}

	return 0;
}

static void mtk_disp_color_unbind(struct device *dev, struct device *master,
				  void *data)
{
	struct mtk_disp_color *priv = dev_get_drvdata(dev);
	struct drm_device *drm_dev = data;

	mtk_ddp_comp_unregister(drm_dev, &priv->ddp_comp);
}

static const struct component_ops mtk_disp_color_component_ops = {
	.bind	= mtk_disp_color_bind,
	.unbind = mtk_disp_color_unbind,
};

static int mtk_disp_color_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct mtk_disp_color *priv;
	enum mtk_ddp_comp_id comp_id;
	int ret = -1;

	DDPINFO("%s+\n", __func__);
	priv = devm_kzalloc(dev, sizeof(*priv), GFP_KERNEL);
	if (priv == NULL)
		goto error_dev_init;

	priv->primary_data = kzalloc(sizeof(*priv->primary_data), GFP_KERNEL);
	if (priv->primary_data == NULL) {
		ret = -ENOMEM;
		dev_err(dev, "Failed to alloc primary_data %d\n", ret);
		goto error_dev_init;
	}

	comp_id = mtk_ddp_comp_get_id(dev->of_node, MTK_DISP_COLOR);
	if ((int)comp_id < 0) {
		dev_err(dev, "Failed to identify by alias: %d\n", comp_id);
		goto error_primary;
	}

	ret = mtk_ddp_comp_init(dev, dev->of_node, &priv->ddp_comp, comp_id,
				&mtk_disp_color_funcs);
	if (ret != 0) {
		dev_err(dev, "Failed to initialize component: %d\n", ret);
		goto error_primary;
	}
	mtk_color_data_init(&priv->ddp_comp);

	priv->data = of_device_get_match_data(dev);

	platform_set_drvdata(pdev, priv);

	mtk_ddp_comp_pm_enable(&priv->ddp_comp);

	ret = component_add(dev, &mtk_disp_color_component_ops);
	if (ret != 0) {
		dev_err(dev, "Failed to add component: %d\n", ret);
		mtk_ddp_comp_pm_disable(&priv->ddp_comp);
	}

	DDPINFO("%s-\n", __func__);

error_primary:
	if (ret < 0)
		kfree(priv->primary_data);
error_dev_init:
	if (ret < 0)
		devm_kfree(dev, priv);

	return ret;
}

static int mtk_disp_color_remove(struct platform_device *pdev)
{
	struct mtk_disp_color *priv = dev_get_drvdata(&pdev->dev);

	component_del(&pdev->dev, &mtk_disp_color_component_ops);
	mtk_ddp_comp_pm_disable(&priv->ddp_comp);

	return 0;
}

static const struct mtk_disp_color_data mt2701_color_driver_data = {
	.color_offset = DISP_COLOR_START_MT2701,
	.support_color21 = false,
	.support_color30 = false,
	.color_window = 0x40106051,
	.support_shadow = false,
	.need_bypass_shadow = false,
};

static const struct mtk_disp_color_data mt6779_color_driver_data = {
	.color_offset = DISP_COLOR_START_MT6779,
	.support_color21 = true,
	.support_color30 = true,
	.reg_table = {0x1400E000, 0x1400F000, 0x14001000,
			0x14011000, 0x14012000},
	.color_window = 0x40185E57,
	.support_shadow = false,
	.need_bypass_shadow = false,
};

static const struct mtk_disp_color_data mt8173_color_driver_data = {
	.color_offset = DISP_COLOR_START_MT8173,
	.support_color21 = false,
	.support_color30 = false,
	.color_window = 0x40106051,
	.support_shadow = false,
	.need_bypass_shadow = false,
};

static const struct mtk_disp_color_data mt6885_color_driver_data = {
	.color_offset = DISP_COLOR_START_MT6885,
	.support_color21 = true,
	.support_color30 = true,
	.reg_table = {0x14007000, 0x14008000, 0x14009000,
			0x1400A000, 0x1400B000},
	.color_window = 0x40185E57,
	.support_shadow = false,
	.need_bypass_shadow = false,
};

static const struct mtk_disp_color_data mt6873_color_driver_data = {
	.color_offset = DISP_COLOR_START_MT6873,
	.support_color21 = true,
	.support_color30 = true,
	.reg_table = {0x14009000, 0x1400A000, 0x1400B000,
			0x1400C000, 0x1400E000},
	.color_window = 0x40185E57,
	.support_shadow = false,
	.need_bypass_shadow = true,
};

static const struct mtk_disp_color_data mt6853_color_driver_data = {
	.color_offset = DISP_COLOR_START_MT6873,
	.support_color21 = true,
	.support_color30 = false,
	.reg_table = {0x14009000, 0x1400B000, 0x1400C000,
			0x1400D000, 0x1400F000, 0x1400A000},
	.color_window = 0x40185E57,
	.support_shadow = false,
	.need_bypass_shadow = true,
};

static const struct mtk_disp_color_data mt6833_color_driver_data = {
	.color_offset = DISP_COLOR_START_MT6873,
	.support_color21 = true,
	.support_color30 = false,
	.reg_table = {0x14009000, 0x1400A000, 0x1400B000,
			0x1400C000, 0x1400E000},
	.color_window = 0x40185E57,
	.support_shadow = false,
	.need_bypass_shadow = true,
};

static const struct mtk_disp_color_data mt6983_color_driver_data = {
	.color_offset = DISP_COLOR_START_MT6873,
	.support_color21 = true,
	.support_color30 = true,
	.reg_table = {0x14009000, 0x1400A000, 0x1400D000, 0x1400E000,
			0x14010000, 0x1400B000, 0x14007000, 0x14008000},
	.color_window = 0x40185E57,
	.support_shadow = false,
	.need_bypass_shadow = true,
};

static const struct mtk_disp_color_data mt6895_color_driver_data = {
	.color_offset = DISP_COLOR_START_MT6873,
	.support_color21 = true,
	.support_color30 = true,
	.reg_table = {0x14009000, 0x1400A000, 0x1400D000, 0x1400E000,
			0x14010000, 0x1400B000, 0x14007000, 0x14008000},
	.color_window = 0x40185E57,
	.support_shadow = false,
	.need_bypass_shadow = true,
};

static const struct mtk_disp_color_data mt6879_color_driver_data = {
	.color_offset = DISP_COLOR_START_MT6873,
	.support_color21 = true,
	.support_color30 = false,
	.reg_table = {0x14009000, 0x1400A000, 0x1400D000, 0x1400E000,
			0x14010000, 0x0, 0x14007000, 0x14008000},
	.color_window = 0x40185E57,
	.support_shadow = false,
	.need_bypass_shadow = true,
};

static const struct mtk_disp_color_data mt6855_color_driver_data = {
	.color_offset = DISP_COLOR_START_MT6873,
	.support_color21 = true,
	.support_color30 = true,
	.reg_table = {0x14009000, 0x1400A000, 0x1400D000,
			0x1400E000, 0x14010000, -1UL, 0x14007000, -1UL},
	.color_window = 0x40185E57,
	.support_shadow = false,
	.need_bypass_shadow = true,
};

static const struct mtk_disp_color_data mt6985_color_driver_data = {
	.color_offset = DISP_COLOR_START_MT6873,
	.support_color21 = true,
	.support_color30 = true,
	.reg_table = {0x14008000, 0x14004000, 0x14002000, 0x1400E000,
			0x14009000, 0x14005000, 0x14018000, 0x14003000,
			0x1400F000, 0x14013000, 0x14014000},
	.color_window = 0x40185E57,
	.support_shadow = false,
	.need_bypass_shadow = true,
};

static const struct mtk_disp_color_data mt6897_color_driver_data = {
	.color_offset = DISP_COLOR_START_MT6873,
	.support_color21 = true,
	.support_color30 = true,
	.reg_table = {0x14008000, 0x14004000, 0x14002000, 0x1400E000,
			0x14009000, 0x14005000, 0x14018000, 0x14003000,
			0x1400F000, 0x14013000, 0x14014000},
	.color_window = 0x40185E57,
	.support_shadow = false,
	.need_bypass_shadow = true,
};

static const struct mtk_disp_color_data mt6886_color_driver_data = {
	.color_offset = DISP_COLOR_START_MT6873,
	.support_color21 = true,
	.support_color30 = true,
	.reg_table = {0x14009000, 0x1400A000, 0x1400D000, 0x1400E000,
			0x14010000, 0x1400B000, -1UL, 0x14008000},
	.color_window = 0x40185E57,
	.support_shadow = false,
	.need_bypass_shadow = true,
};


static const struct of_device_id mtk_disp_color_driver_dt_match[] = {
	{.compatible = "mediatek,mt2701-disp-color",
	 .data = &mt2701_color_driver_data},
	{.compatible = "mediatek,mt6779-disp-color",
	 .data = &mt6779_color_driver_data},
	{.compatible = "mediatek,mt6885-disp-color",
	 .data = &mt6885_color_driver_data},
	{.compatible = "mediatek,mt8173-disp-color",
	 .data = &mt8173_color_driver_data},
	{.compatible = "mediatek,mt6873-disp-color",
	 .data = &mt6873_color_driver_data},
	{.compatible = "mediatek,mt6853-disp-color",
	 .data = &mt6853_color_driver_data},
	{.compatible = "mediatek,mt6833-disp-color",
	 .data = &mt6833_color_driver_data},
	{.compatible = "mediatek,mt6983-disp-color",
	 .data = &mt6983_color_driver_data},
	{.compatible = "mediatek,mt6895-disp-color",
	 .data = &mt6895_color_driver_data},
	{.compatible = "mediatek,mt6879-disp-color",
	 .data = &mt6879_color_driver_data},
	{.compatible = "mediatek,mt6855-disp-color",
	 .data = &mt6855_color_driver_data},
	{.compatible = "mediatek,mt6985-disp-color",
	 .data = &mt6985_color_driver_data},
	{.compatible = "mediatek,mt6886-disp-color",
	 .data = &mt6886_color_driver_data},
	{.compatible = "mediatek,mt6835-disp-color",
	 .data = &mt6835_color_driver_data},
	{.compatible = "mediatek,mt6897-disp-color",
	 .data = &mt6897_color_driver_data},
	{},
};
MODULE_DEVICE_TABLE(of, mtk_disp_color_driver_dt_match);

struct platform_driver mtk_disp_color_driver = {
	.probe = mtk_disp_color_probe,
	.remove = mtk_disp_color_remove,
	.driver = {
			.name = "mediatek-disp-color",
			.owner = THIS_MODULE,
			.of_match_table = mtk_disp_color_driver_dt_match,
		},
};

void disp_color_set_bypass(struct drm_crtc *crtc, int bypass)
{
	int ret;
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	struct mtk_ddp_comp *comp = mtk_ddp_comp_sel_in_cur_crtc_path(
			mtk_crtc, MTK_DISP_COLOR, 0);

	ret = mtk_crtc_user_cmd(crtc, comp, BYPASS_COLOR, &bypass);

	DDPINFO("%s : ret = %d", __func__, ret);
}
