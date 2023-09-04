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
//#include "mtk_drm_ddp_comp.h"
//#include "mtk_dump.h"
//#include "mtk_drm_mmp.h"
#include "mtk_drm_trace.h"
#include "mtk_drm_drv.h"
#include "mtk_drm_crtc.h"
#include "platform/mtk_drm_platform.h"

static atomic_t g_ff_enabled = ATOMIC_INIT(0);

struct mtk_disp_vidle_para mtk_disp_vidle_flag = {
	0,	/* vidle_en */
	0,	/* vidle_init */
	0,	/* vidle_stop */
	0,	/* rc_en */
	0,	/* wdt_en */
};

struct dpc_funcs disp_dpc_driver;
struct mtk_vdisp_funcs vdisp_func;

static atomic_t g_vidle_pq_ref = ATOMIC_INIT(0);
static DEFINE_MUTEX(g_vidle_pq_ref_lock);

struct mtk_disp_vidle {
	u8 level;
	u32 hrt_bw;
	u32 srt_bw;
};

static struct mtk_disp_vidle vidle_data = {0xff, 0xffffffff, 0xffffffff};

void mtk_vidle_flag_init(void *_crtc)
{
	struct mtk_drm_private *priv = NULL;
	struct mtk_ddp_comp *output_comp = NULL;
	struct mtk_drm_crtc *mtk_crtc = NULL;
	struct drm_crtc *crtc = NULL;

	mtk_disp_vidle_flag.vidle_en = 0;

	if (_crtc == NULL)
		return;
	crtc = (struct drm_crtc *)_crtc;
	mtk_crtc = to_mtk_crtc(crtc);
	output_comp = mtk_ddp_comp_request_output(mtk_crtc);
	priv = crtc->dev->dev_private;
	if (priv == NULL || output_comp == NULL)
		return;

	/* video mode no V-Idle */
	if (mtk_dsi_is_cmd_mode(output_comp) == 0)
		return;

	if (mtk_drm_helper_get_opt(priv->helper_opt, MTK_DRM_OPT_VIDLE_TOP_EN))
		mtk_disp_vidle_flag.vidle_en |= DISP_VIDLE_TOP_EN;
	if (mtk_drm_helper_get_opt(priv->helper_opt, MTK_DRM_OPT_VIDLE_MMINFRA_DT_EN))
		mtk_disp_vidle_flag.vidle_en |= DISP_VIDLE_MMINFRA_DT_EN;

	/* TODO: CHECK LCM_IS_CONNECTED, if not, auto mtcmos cannot be enabled */
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

void mtk_vidle_user_power_keep(enum mtk_vidle_voter_user user)
{
	if (disp_dpc_driver.dpc_vidle_power_keep == NULL)
		return;

	if (mtk_vidle_enable_check(DISP_VIDLE_MTCMOS_DT_EN) ||
		mtk_vidle_enable_check(DISP_VIDLE_MMINFRA_DT_EN))
		disp_dpc_driver.dpc_vidle_power_keep(user);
}

void mtk_vidle_user_power_release(enum mtk_vidle_voter_user user)
{
	if (disp_dpc_driver.dpc_vidle_power_release == NULL)
		return;

	if (mtk_vidle_enable_check(DISP_VIDLE_MTCMOS_DT_EN) ||
		mtk_vidle_enable_check(DISP_VIDLE_MMINFRA_DT_EN))
		disp_dpc_driver.dpc_vidle_power_release(user);
}

void mtk_vidle_pq_power_get(const char *caller)
{
	s32 ref;

	mutex_lock(&g_vidle_pq_ref_lock);
	ref = atomic_inc_return(&g_vidle_pq_ref);
	if (ref == 1)
		mtk_vidle_user_power_keep(DISP_VIDLE_USER_PQ);
	mutex_unlock(&g_vidle_pq_ref_lock);
	if (ref <= 0)
		DDPPR_ERR("%s  get invalid cnt %d\n", caller, ref);
}

void mtk_vidle_pq_power_put(const char *caller)
{
	s32 ref;

	mutex_lock(&g_vidle_pq_ref_lock);
	ref = atomic_dec_return(&g_vidle_pq_ref);
	if (!ref)
		mtk_vidle_user_power_release(DISP_VIDLE_USER_PQ);
	mutex_unlock(&g_vidle_pq_ref_lock);
	if (ref < 0)
		DDPPR_ERR("%s  put invalid cnt %d\n", caller, ref);
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
	// mtk_vidle_power_keep();
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

void mtk_set_dt_configure(u8 dt, unsigned int us)
{
	if (disp_dpc_driver.dpc_dt_set)
		disp_dpc_driver.dpc_dt_set(dt, us);
}

void mtk_vidle_update_dt_by_period(void *_crtc)
{
	struct drm_crtc *crtc = NULL;
	struct mtk_panel_params *panel_ext = NULL;
	unsigned int duration;
	unsigned int fps;

	if (_crtc == NULL || !disp_dpc_driver.dpc_dt_set)
		return;

	crtc = (struct drm_crtc *)_crtc;
	panel_ext = mtk_drm_get_lcm_ext_params(crtc);

	if (panel_ext && panel_ext->real_te_duration) {
		duration = panel_ext->real_te_duration;
	} else {
		fps = drm_mode_vrefresh(&crtc->state->adjusted_mode);
		duration = (fps == 0) ? 16666 : 1000000 / fps;
	}

	/* update DTs affected by TE duration */
	disp_dpc_driver.dpc_dt_set(1, duration - DT_OVL_OFFSET);
	disp_dpc_driver.dpc_dt_set(5, duration - DT_DISP1_OFFSET);
	disp_dpc_driver.dpc_dt_set(6, duration - DT_DISP1TE_OFFSET);
	disp_dpc_driver.dpc_dt_set(12, duration - DT_MMINFRA_OFFSET);
	disp_dpc_driver.dpc_dt_set(33, duration - DT_OVL_OFFSET);
	disp_dpc_driver.dpc_dt_set(40, duration - DT_MMINFRA_OFFSET);
}

bool mtk_vidle_is_ff_enabled(void)
{
	return (bool)atomic_read(&g_ff_enabled);
}

void mtk_vidle_enable(bool en, void *_drm_priv)
{
	if (!disp_dpc_driver.dpc_enable)
		return;

	if (!mtk_disp_vidle_flag.vidle_en)
		return;

	/* some case, like multi crtc we need to stop V-idle */
	if (mtk_disp_vidle_flag.vidle_stop) {
		mtk_vidle_stop();
		return;
	}

	if (en == mtk_vidle_is_ff_enabled())
		return;
	atomic_set(&g_ff_enabled, en);

	if (_drm_priv) {
		struct mtk_drm_private *drm_priv = _drm_priv;

		if (drm_priv->dpc_dev) {
			if (en)
				pm_runtime_put_sync(drm_priv->dpc_dev);
			else
				pm_runtime_get_sync(drm_priv->dpc_dev);
		}
	}
	disp_dpc_driver.dpc_enable(en);
	/* TODO: enable timestamp */
}

void mtk_vidle_hrt_bw_set(const u32 bw_in_mb)
{
	vidle_data.hrt_bw = bw_in_mb;
	if (disp_dpc_driver.dpc_hrt_bw_set)
		disp_dpc_driver.dpc_hrt_bw_set(DPC_SUBSYS_DISP, bw_in_mb,
					       !atomic_read(&g_ff_enabled));
	else
		DDPINFO("%s NOT SET:%d\n", __func__, bw_in_mb);

}
void mtk_vidle_srt_bw_set(const u32 bw_in_mb)
{
	vidle_data.srt_bw = bw_in_mb;
	if (disp_dpc_driver.dpc_srt_bw_set)
		disp_dpc_driver.dpc_srt_bw_set(DPC_SUBSYS_DISP, bw_in_mb,
					       !atomic_read(&g_ff_enabled));
	else
		DDPINFO("%s NOT SET:%d\n", __func__, bw_in_mb);
}
void mtk_vidle_dvfs_set(const u8 level)
{
	vidle_data.level = level;
	if (disp_dpc_driver.dpc_dvfs_set)
		disp_dpc_driver.dpc_dvfs_set(DPC_SUBSYS_DISP, level,
					     !atomic_read(&g_ff_enabled));
	else
		DDPINFO("%s NOT SET:%d\n", __func__, level);
}
void mtk_vidle_dvfs_bw_set(const u32 bw_in_mb)
{
	if (disp_dpc_driver.dpc_dvfs_bw_set)
		disp_dpc_driver.dpc_dvfs_bw_set(DPC_SUBSYS_DISP, bw_in_mb);
}
void mtk_vidle_config_ff(bool en)
{
	if (en && !mtk_disp_vidle_flag.vidle_en)
		return;

	if (disp_dpc_driver.dpc_config)
		disp_dpc_driver.dpc_config(DPC_SUBSYS_DISP, en);
}

void mtk_vidle_dpc_analysis(void)
{
	if (disp_dpc_driver.dpc_analysis)
		disp_dpc_driver.dpc_analysis();
}

void mtk_vidle_register(const struct dpc_funcs *funcs)
{
	disp_dpc_driver.dpc_enable = funcs->dpc_enable;
	disp_dpc_driver.dpc_config = funcs->dpc_config;
	disp_dpc_driver.dpc_dt_set = funcs->dpc_dt_set;
	disp_dpc_driver.dpc_group_enable = funcs->dpc_group_enable;
	disp_dpc_driver.dpc_vidle_power_keep = funcs->dpc_vidle_power_keep;
	disp_dpc_driver.dpc_vidle_power_release =
		funcs->dpc_vidle_power_release;
	disp_dpc_driver.dpc_hrt_bw_set = funcs->dpc_hrt_bw_set;
	disp_dpc_driver.dpc_srt_bw_set = funcs->dpc_srt_bw_set;
	disp_dpc_driver.dpc_dvfs_set = funcs->dpc_dvfs_set;
	disp_dpc_driver.dpc_dvfs_bw_set = funcs->dpc_dvfs_bw_set;
	disp_dpc_driver.dpc_analysis = funcs->dpc_analysis;

	if(vidle_data.level != 0xff) {
		DDPINFO("%s need set level:%d\n", __func__, vidle_data.level);
		mtk_vidle_dvfs_set(vidle_data.level);
	}

	if(vidle_data.hrt_bw != 0xffffffff) {
		DDPINFO("%s need set hrt bw:%d\n", __func__, vidle_data.hrt_bw);
		mtk_vidle_dvfs_set(vidle_data.hrt_bw);
	}

	if(vidle_data.srt_bw != 0xffffffff) {
		DDPINFO("%s need set srt bw:%d\n", __func__, vidle_data.srt_bw);
		mtk_vidle_dvfs_set(vidle_data.srt_bw);
	}
}
EXPORT_SYMBOL(mtk_vidle_register);

void mtk_vdisp_register(const struct mtk_vdisp_funcs *fp)
{
	vdisp_func.genpd_put = fp->genpd_put;
}
EXPORT_SYMBOL(mtk_vdisp_register);
