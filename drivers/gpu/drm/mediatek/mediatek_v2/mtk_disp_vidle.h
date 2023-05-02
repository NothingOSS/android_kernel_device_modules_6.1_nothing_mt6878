/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2023 MediaTek Inc.
 */

#ifndef __MTK_DISP_VIDLE_H__
#define __MTK_DISP_VIDLE_H__

#include "mtk_drm_crtc.h"

struct mtk_disp_vidle_para {
	unsigned int vidle_en;
	unsigned int rc_en;
	unsigned int wdt_en;
};

#define DISP_VIDLE_TOP_EN BIT(0)	/* total V-Idle on/off */
#define DISP_VIDLE_MTCMOS_DT_EN BIT(1)
#define DISP_VIDLE_MMINFRA_DT_EN BIT(2)
#define DISP_VIDLE_DVFS_DT_EN BIT(3)
#define DISP_VIDLE_QOS_DT_EN BIT(4)
#define DISP_VIDLE_GCE_TS_EN BIT(5)

struct mtk_disp_dpc_data {
	struct mtk_disp_vidle_para *mtk_disp_vidle_flag;
};

struct dpc_driver {
	void (*dpc_enable)(bool en);
	void (*dpc_group_enable)(const u16 group, bool en);
	//void (*dpc_config)(const enum mtk_dpc_subsys subsys);
	//void (*dpc_mtcmos_vote)(const enum mtk_dpc_subsys subsys, const u8 thread, const bool en);
	int (*vidle_power_keep)(void);
	void (*vidle_power_release)(void);
};


void mtk_vidle_sync_mmdvfsrc_status_rc(unsigned int rc_en);
void mtk_vidle_sync_mmdvfsrc_status_wdt(unsigned int wdt_en);
void mtk_vidle_enable(struct mtk_drm_private *priv);
void mtk_vidle_power_keep(void);
void mtk_vidle_power_release(void);
__weak void dpc_enable(bool en)
{
	return;
};
__weak void dpc_group_enable(const u16 group, bool en)
{
	return;
};
__weak int mtk_disp_vidle_power_keep(void)
{
	return 0;
};
__weak void mtk_disp_vidle_power_release(void)
{
	return;
};
#endif
