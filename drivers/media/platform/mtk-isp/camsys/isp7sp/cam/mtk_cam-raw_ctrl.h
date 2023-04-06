/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2023 MediaTek Inc.
 */

#ifndef __MTK_CAM_RAW_CTRL_H
#define __MTK_CAM_RAW_CTRL_H

#include "mtk_camera-v4l2-controls-7sp.h"

static inline bool scen_is_dc_mode(struct mtk_cam_resource_raw_v2 *res_raw)
{
	return res_raw->hw_mode == HW_MODE_DIRECT_COUPLED;
}

static inline bool scen_is_vhdr(struct mtk_cam_scen *scen)
{
	switch (scen->id) {
	case MTK_CAM_SCEN_NORMAL:
	case MTK_CAM_SCEN_M2M_NORMAL:
	case MTK_CAM_SCEN_ODT_NORMAL:
		return scen->scen.normal.max_exp_num > 1;
	case MTK_CAM_SCEN_MSTREAM:
	case MTK_CAM_SCEN_ODT_MSTREAM:
		return 1;
	default:
		break;
	}
	return 0;
}

static inline bool scen_is_rgbw(struct mtk_cam_scen *scen)
{
	if (scen->id == MTK_CAM_SCEN_NORMAL ||
	    scen->id == MTK_CAM_SCEN_ODT_NORMAL ||
	    scen->id == MTK_CAM_SCEN_M2M_NORMAL)
		return !!(scen->scen.normal.w_chn_enabled);

	return false;
}

static inline bool scen_is_m2m(struct mtk_cam_scen *scen)
{
	return (scen->id == MTK_CAM_SCEN_ODT_NORMAL ||
		scen->id == MTK_CAM_SCEN_ODT_MSTREAM ||
		scen->id == MTK_CAM_SCEN_M2M_NORMAL);
}

static inline bool scen_is_m2m_apu(struct mtk_cam_scen *scen,
				   struct mtk_cam_apu_info *apu_info)
{
	return scen->id == MTK_CAM_SCEN_M2M_NORMAL &&
		(apu_info->apu_path != APU_NONE);
}

#endif /*__MTK_CAM_RAW_CTRL_H*/
