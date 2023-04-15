/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2023 MediaTek Inc.
 */

#ifndef __MTK_CAM_RAW_CTRL_H
#define __MTK_CAM_RAW_CTRL_H

#include "mtk_camera-v4l2-controls-7sp.h"

static inline
bool res_raw_is_dc_mode(const struct mtk_cam_resource_raw_v2 *res_raw)
{
	return res_raw->hw_mode == MTK_CAM_HW_MODE_DIRECT_COUPLED;
}

static inline bool scen_is_normal(const struct mtk_cam_scen *scen)
{
	if (scen->id == MTK_CAM_SCEN_NORMAL ||
	    scen->id == MTK_CAM_SCEN_ODT_NORMAL ||
	    scen->id == MTK_CAM_SCEN_M2M_NORMAL)
		return true;

	return false;
}

static inline bool scen_is_dcg_sensor_merge(const struct mtk_cam_scen *scen)
{
	if (scen_is_normal(scen))
		return (scen->scen.normal.stagger_type ==
				MTK_CAM_STAGGER_DCG_SENSOR_MERGE);

	return false;
}

static inline bool scen_is_vhdr(const struct mtk_cam_scen *scen)
{
	switch (scen->id) {
	case MTK_CAM_SCEN_NORMAL:
	case MTK_CAM_SCEN_M2M_NORMAL:
	case MTK_CAM_SCEN_ODT_NORMAL:
		return (scen->scen.normal.max_exp_num > 1 ||
			scen_is_dcg_sensor_merge(scen));
	case MTK_CAM_SCEN_MSTREAM:
	case MTK_CAM_SCEN_ODT_MSTREAM:
		return 1;
	default:
		break;
	}
	return 0;
}

static inline bool scen_is_rgbw(const struct mtk_cam_scen *scen)
{
	if (scen_is_normal(scen))
		return !!(scen->scen.normal.w_chn_enabled);

	return false;
}

static inline bool scen_is_m2m(const struct mtk_cam_scen *scen)
{
	return (scen->id == MTK_CAM_SCEN_ODT_NORMAL ||
		scen->id == MTK_CAM_SCEN_ODT_MSTREAM ||
		scen->id == MTK_CAM_SCEN_M2M_NORMAL);
}

static inline bool scen_is_m2m_apu(const struct mtk_cam_scen *scen,
				   const struct mtk_cam_apu_info *apu_info)
{
	return scen->id == MTK_CAM_SCEN_M2M_NORMAL &&
		(apu_info->apu_path != APU_NONE);
}

static inline bool apu_info_is_dc(const struct mtk_cam_apu_info *apu_info)
{
	return apu_info->apu_path == APU_DC_RAW;
}

#endif /*__MTK_CAM_RAW_CTRL_H*/
