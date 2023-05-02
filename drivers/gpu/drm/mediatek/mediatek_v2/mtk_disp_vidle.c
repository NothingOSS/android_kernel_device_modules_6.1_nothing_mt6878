// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2023 MediaTek Inc.
 */

#include <linux/clk.h>
#include <linux/component.h>
#include <linux/of_device.h>
#include <linux/of_irq.h>
#include <linux/platform_device.h>

#ifndef DRM_CMDQ_DISABLE
#include <linux/soc/mediatek/mtk-cmdq-ext.h>
#else
#include "mtk-cmdq-ext.h"
#endif

#include "mtk_disp_vidle.h"
#include "../drivers/gpu/drm/mediatek/dpc/mtk_dpc.h"
//#include "mtk_drm_ddp_comp.h"
//#include "mtk_dump.h"
//#include "mtk_drm_mmp.h"
#include "mtk_drm_trace.h"
#include "mtk_drm_drv.h"
#include "platform/mtk_drm_platform.h"

struct mtk_disp_vidle_para mtk_disp_vidle_flag = {
	0,	//unsigned int vidle_en;
	0,	//unsigned int rc_en;
	0,	//unsigned int wdt_en;
};

struct dpc_driver disp_dpc_driver = {
	.dpc_enable = dpc_enable,
	.dpc_group_enable = dpc_group_enable,
	.vidle_power_keep = mtk_disp_vidle_power_keep,
	.vidle_power_release = mtk_disp_vidle_power_release,
};

struct mtk_disp_vidle {
	const struct mtk_disp_vidle_data *data;
};


static void mtk_vidle_flag_init(struct mtk_drm_private *priv)
{
	DDPFUNC();
	if (priv == NULL)
		return;

	mtk_disp_vidle_flag.vidle_en = 0;	//init
	if (mtk_drm_helper_get_opt(priv->helper_opt, MTK_DRM_OPT_VIDLE_TOP_EN))
		mtk_disp_vidle_flag.vidle_en = mtk_disp_vidle_flag.vidle_en | DISP_VIDLE_TOP_EN;
	if (mtk_drm_helper_get_opt(priv->helper_opt, MTK_DRM_OPT_VIDLE_MTCMOS_DT_EN))
		mtk_disp_vidle_flag.vidle_en =
			mtk_disp_vidle_flag.vidle_en | DISP_VIDLE_MTCMOS_DT_EN;
	if (mtk_drm_helper_get_opt(priv->helper_opt, MTK_DRM_OPT_VIDLE_MMINFRA_DT_EN))
		mtk_disp_vidle_flag.vidle_en =
			mtk_disp_vidle_flag.vidle_en | DISP_VIDLE_MMINFRA_DT_EN;
	if (mtk_drm_helper_get_opt(priv->helper_opt, MTK_DRM_OPT_VIDLE_DVFS_DT_EN))
		mtk_disp_vidle_flag.vidle_en = mtk_disp_vidle_flag.vidle_en | DISP_VIDLE_DVFS_DT_EN;
	if (mtk_drm_helper_get_opt(priv->helper_opt, MTK_DRM_OPT_VIDLE_QOS_DT_EN))
		mtk_disp_vidle_flag.vidle_en = mtk_disp_vidle_flag.vidle_en | DISP_VIDLE_QOS_DT_EN;
	if (mtk_drm_helper_get_opt(priv->helper_opt, MTK_DRM_OPT_VIDLE_GCE_TS_EN))
		mtk_disp_vidle_flag.vidle_en = mtk_disp_vidle_flag.vidle_en | DISP_VIDLE_GCE_TS_EN;
}

static unsigned int mtk_vidle_enable_check(unsigned int vidle_item)
{
	if (mtk_disp_vidle_flag.vidle_en & vidle_item)
		DDPINFO("vidle(0x%x) ON\n", vidle_item);
	else
		DDPINFO("vidle(0x%x) OFF\n", vidle_item);

	return mtk_disp_vidle_flag.vidle_en & vidle_item;
}

static void mtk_vidle_dt_enable(void)
{
	DDPFUNC();
	if (disp_dpc_driver.dpc_group_enable == NULL)
		return;

	if (mtk_vidle_enable_check(DISP_VIDLE_MTCMOS_DT_EN)) {
		disp_dpc_driver.dpc_group_enable(DPC_DISP_VIDLE_MTCMOS, true);
		disp_dpc_driver.dpc_group_enable(DPC_DISP_VIDLE_MTCMOS_DISP1, true);
	}
	if (mtk_vidle_enable_check(DISP_VIDLE_MMINFRA_DT_EN))
		disp_dpc_driver.dpc_group_enable(DPC_DISP_VIDLE_MMINFRA_OFF, true);
	if (mtk_vidle_enable_check(DISP_VIDLE_DVFS_DT_EN))
		disp_dpc_driver.dpc_group_enable(DPC_DISP_VIDLE_VDISP_DVFS, true);
	if (mtk_vidle_enable_check(DISP_VIDLE_QOS_DT_EN)) {
		disp_dpc_driver.dpc_group_enable(DPC_DISP_VIDLE_HRT_BW, true);
		disp_dpc_driver.dpc_group_enable(DPC_DISP_VIDLE_SRT_BW, true);
	}
}

void mtk_vidle_power_keep(void)
{
	//DDPFUNC();
	if (disp_dpc_driver.vidle_power_keep == NULL)
		return;

	if (mtk_vidle_enable_check(DISP_VIDLE_MTCMOS_DT_EN) ||
		mtk_vidle_enable_check(DISP_VIDLE_MMINFRA_DT_EN))
		disp_dpc_driver.vidle_power_keep();
}

void mtk_vidle_power_release(void)
{
	//DDPFUNC();
	if (disp_dpc_driver.vidle_power_release == NULL)
		return;

	if (mtk_vidle_enable_check(DISP_VIDLE_MTCMOS_DT_EN) ||
		mtk_vidle_enable_check(DISP_VIDLE_MMINFRA_DT_EN))
		disp_dpc_driver.vidle_power_release();
}

void mtk_vidle_sync_mmdvfsrc_status_rc(unsigned int rc_en)
{
	DDPFUNC();
	mtk_disp_vidle_flag.rc_en = rc_en;
	/* TODO: action for mmdvfsrc_status_rc */
}

void mtk_vidle_sync_mmdvfsrc_status_wdt(unsigned int wdt_en)
{
	DDPFUNC();
	mtk_disp_vidle_flag.wdt_en = wdt_en;
	/* TODO: action for mmdvfsrc_status_wdt */
}

void mtk_vidle_enable(struct mtk_drm_private *priv)
{
	DDPFUNC();
	if (priv == NULL)
		return;

	DDPINFO("vidle init SW status\n");
	mtk_vidle_flag_init(priv);

	DDPINFO("vidle set dt\n");
	mtk_vidle_dt_enable();

	DDPINFO("vidle enable\n");
	if (disp_dpc_driver.dpc_enable)
		disp_dpc_driver.dpc_enable(mtk_vidle_enable_check(DISP_VIDLE_TOP_EN));
}

