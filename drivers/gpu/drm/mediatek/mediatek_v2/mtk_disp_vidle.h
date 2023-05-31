/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2023 MediaTek Inc.
 */

#ifndef __MTK_DISP_VIDLE_H__
#define __MTK_DISP_VIDLE_H__

#include "mtk_dpc.h"

extern void dpc_enable(bool en);
extern void dpc_group_enable(const u16 group, bool en);
extern void dpc_config(const enum mtk_dpc_subsys subsys, bool en);
extern void dpc_mtcmos_vote(const enum mtk_dpc_subsys subsys, const u8 thread, const bool en);
extern void dpc_hrt_bw_set(const enum mtk_dpc_subsys subsys, const u32 bw_in_mb);
extern void dpc_srt_bw_set(const enum mtk_dpc_subsys subsys, const u32 bw_in_mb);
extern void dpc_dvfs_set(const enum mtk_dpc_subsys subsys, const u8 level);
extern int mtk_disp_vidle_power_keep(void);
extern void mtk_disp_vidle_power_release(void);

struct mtk_disp_vidle_para {
	unsigned int vidle_en;
	unsigned int vidle_init;
	unsigned int vidle_stop;
	unsigned int rc_en;
	unsigned int wdt_en;
};

#define DISP_VIDLE_TOP_EN BIT(0)	/* total V-Idle on/off */
#define DISP_VIDLE_MTCMOS_DT_EN BIT(1)
#define DISP_VIDLE_MMINFRA_DT_EN BIT(2)
#define DISP_VIDLE_DVFS_DT_EN BIT(3)
#define DISP_VIDLE_QOS_DT_EN BIT(4)
#define DISP_VIDLE_GCE_TS_EN BIT(5)
#define DISP_DPC_PRE_TE_EN BIT(6)

/* V-idle stop case */
#define VIDLE_STOP_DEBUGE BIT(0)
#define VIDLE_STOP_MULTI_CRTC BIT(1)
#define VIDLE_STOP_LCM_DISCONNECT BIT(2)

struct mtk_disp_dpc_data {
	struct mtk_disp_vidle_para *mtk_disp_vidle_flag;
};

struct dpc_driver {
	void (*dpc_enable)(bool en);
	void (*dpc_group_enable)(const u16 group, bool en);
	void (*dpc_config)(const enum mtk_dpc_subsys subsys, bool en);
	void (*dpc_mtcmos_vote)(const enum mtk_dpc_subsys subsys, const u8 thread, const bool en);
	int (*vidle_power_keep)(void);
	void (*vidle_power_release)(void);
	void (*dpc_hrt_bw_set)(const enum mtk_dpc_subsys subsys, const u32 bw_in_mb);
	void (*dpc_srt_bw_set)(const enum mtk_dpc_subsys subsys, const u32 bw_in_mb);
	void (*dpc_dvfs_set)(const enum mtk_dpc_subsys subsys, const u8 level);
};

void mtk_vidle_sync_mmdvfsrc_status_rc(unsigned int rc_en);
void mtk_vidle_sync_mmdvfsrc_status_wdt(unsigned int wdt_en);
void mtk_vidle_enable(void *crtc);
void mtk_vidle_power_keep(void);
void mtk_vidle_power_release(void);
void mtk_set_vidle_stop_flag(unsigned int flag, unsigned int stop);
void mtk_vidle_set_all_flag(unsigned int en, unsigned int stop);
void mtk_vidle_get_all_flag(unsigned int *en, unsigned int *stop);
void mtk_vidle_hrt_bw_set(const u32 bw_in_mb);
void mtk_vidle_srt_bw_set(const u32 bw_in_mb);
void mtk_vidle_dvfs_set(const u8 level);
void mtk_vidle_register(const struct dpc_driver *funcs);

#endif
