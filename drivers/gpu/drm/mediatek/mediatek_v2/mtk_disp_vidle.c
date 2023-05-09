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
	0,	/* vidle_en */
	0,	/* vidle_init */
	0,	/* vidle_stop */
	0,	/* rc_en */
	0,	/* wdt_en */
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
	if (mtk_drm_helper_get_opt(priv->helper_opt, MTK_DRM_OPT_DPC_PRE_TE_EN))
		mtk_disp_vidle_flag.vidle_en = mtk_disp_vidle_flag.vidle_en | DISP_DPC_PRE_TE_EN;
}

static unsigned int mtk_vidle_enable_check(unsigned int vidle_item)
{
	return mtk_disp_vidle_flag.vidle_en & vidle_item;
}

static void mtk_vidle_dt_enable(unsigned int en)
{
	if (disp_dpc_driver.dpc_group_enable == NULL)
		return;

	disp_dpc_driver.dpc_group_enable(DPC_DISP_VIDLE_MTCMOS,
		(en && mtk_vidle_enable_check(DISP_VIDLE_MTCMOS_DT_EN)));
	disp_dpc_driver.dpc_group_enable(DPC_DISP_VIDLE_MTCMOS_DISP1,
		(en && mtk_vidle_enable_check(DISP_VIDLE_MTCMOS_DT_EN)));

	disp_dpc_driver.dpc_group_enable(DPC_DISP_VIDLE_MMINFRA_OFF,
		(en && mtk_vidle_enable_check(DISP_VIDLE_MMINFRA_DT_EN)));
	disp_dpc_driver.dpc_group_enable(DPC_DISP_VIDLE_INFRA_OFF,
		(en && mtk_vidle_enable_check(DISP_VIDLE_MMINFRA_DT_EN)));

	disp_dpc_driver.dpc_group_enable(DPC_DISP_VIDLE_VDISP_DVFS,
		(en && mtk_vidle_enable_check(DISP_VIDLE_DVFS_DT_EN)));

	disp_dpc_driver.dpc_group_enable(DPC_DISP_VIDLE_HRT_BW,
		(en && mtk_vidle_enable_check(DISP_VIDLE_QOS_DT_EN)));
	disp_dpc_driver.dpc_group_enable(DPC_DISP_VIDLE_SRT_BW,
		(en && mtk_vidle_enable_check(DISP_VIDLE_QOS_DT_EN)));
}

void mtk_vidle_power_keep(void)
{
	if (disp_dpc_driver.vidle_power_keep == NULL)
		return;

	if (mtk_vidle_enable_check(DISP_VIDLE_MTCMOS_DT_EN) ||
		mtk_vidle_enable_check(DISP_VIDLE_MMINFRA_DT_EN))
		disp_dpc_driver.vidle_power_keep();
}

void mtk_vidle_power_release(void)
{
	if (disp_dpc_driver.vidle_power_release == NULL)
		return;

	if (mtk_vidle_enable_check(DISP_VIDLE_MTCMOS_DT_EN) ||
		mtk_vidle_enable_check(DISP_VIDLE_MMINFRA_DT_EN))
		disp_dpc_driver.vidle_power_release();
}

void mtk_vidle_sync_mmdvfsrc_status_rc(unsigned int rc_en)
{
	mtk_disp_vidle_flag.rc_en = rc_en;
	/* TODO: action for mmdvfsrc_status_rc */
}

void mtk_vidle_sync_mmdvfsrc_status_wdt(unsigned int wdt_en)
{
	mtk_disp_vidle_flag.wdt_en = wdt_en;
	/* TODO: action for mmdvfsrc_status_wdt */
}

/* for debug only, DONT use in flow */
void mtk_vidle_set_all_flag(unsigned int en, unsigned int stop)
{
	mtk_disp_vidle_flag.vidle_en = en;
	mtk_disp_vidle_flag.vidle_stop = stop;
}

void mtk_vidle_get_all_flag(unsigned int *en, unsigned int *stop)
{
	*en = mtk_disp_vidle_flag.vidle_en;
	*stop = mtk_disp_vidle_flag.vidle_stop;
}

static void mtk_vidle_stop(void)
{
	mtk_vidle_power_keep();
	mtk_vidle_dt_enable(0);
	/* TODO: stop timestamp */
}

void mtk_set_vidle_stop_flag(unsigned int flag, unsigned int stop)
{
	if (stop)
		mtk_disp_vidle_flag.vidle_stop =
			mtk_disp_vidle_flag.vidle_stop | flag;
	else
		mtk_disp_vidle_flag.vidle_stop =
			mtk_disp_vidle_flag.vidle_stop & ~flag;

	if (mtk_disp_vidle_flag.vidle_stop)
		mtk_vidle_stop();
}

void mtk_vidle_enable(struct mtk_drm_private *priv)
{
	if (priv == NULL)
		return;
	if (mtk_vidle_enable_check(DISP_VIDLE_TOP_EN))
		DDPINFO("vidle en(0x%x), stop(0x%x)\n",
			mtk_disp_vidle_flag.vidle_en, mtk_disp_vidle_flag.vidle_stop);

	if (mtk_disp_vidle_flag.vidle_init == 0) {
		mtk_vidle_flag_init(priv);
		mtk_disp_vidle_flag.vidle_init = 1;
	}

	/* some case, like multi crtc we need to stop V-idle */
	if (mtk_disp_vidle_flag.vidle_stop) {
		mtk_vidle_stop();
		return;
	}


	if (disp_dpc_driver.dpc_enable && mtk_vidle_enable_check(DISP_VIDLE_TOP_EN)) {
		mtk_vidle_dt_enable(1);
		disp_dpc_driver.dpc_enable(1);
	}

	/* TODO: enable timestamp */
}


