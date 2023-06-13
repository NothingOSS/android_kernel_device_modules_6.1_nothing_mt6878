/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2023 MediaTek Inc.
 */

#ifndef __MTK_DPC_H__
#define __MTK_DPC_H__

enum mtk_dpc_subsys {
	DPC_SUBSYS_DISP = 0,
	DPC_SUBSYS_DISP0 = 0,
	DPC_SUBSYS_DISP1 = 1,
	DPC_SUBSYS_OVL0 = 2,
	DPC_SUBSYS_OVL1 = 3,
	DPC_SUBSYS_MML = 4,
	DPC_SUBSYS_MML1 = 4,
};

/* NOTE: user 0 to 7 only */
enum mtk_vidle_voter_user {
	DISP_VIDLE_USER_DISP = 0,
	DISP_VIDLE_USER_PQ,
	DISP_VIDLE_USER_MML,
	DISP_VIDLE_USER_OTHER = 7
};

enum mtk_dpc_disp_vidle {
	DPC_DISP_VIDLE_MTCMOS = 0,
	DPC_DISP_VIDLE_MTCMOS_DISP1 = 4,
	DPC_DISP_VIDLE_VDISP_DVFS = 8,
	DPC_DISP_VIDLE_HRT_BW = 11,
	DPC_DISP_VIDLE_SRT_BW = 14,
	DPC_DISP_VIDLE_MMINFRA_OFF = 17,
	DPC_DISP_VIDLE_INFRA_OFF = 20,
	DPC_DISP_VIDLE_MAINPLL_OFF = 23,
	DPC_DISP_VIDLE_MSYNC2_0 = 26,
	DPC_DISP_VIDLE_RESERVED = 29,
	DPC_DISP_VIDLE_MAX = 32,
};

enum mtk_dpc_mml_vidle {
	DPC_MML_VIDLE_MTCMOS = 32,
	DPC_MML_VIDLE_VDISP_DVFS = 36,
	DPC_MML_VIDLE_HRT_BW = 39,
	DPC_MML_VIDLE_SRT_BW = 42,
	DPC_MML_VIDLE_MMINFRA_OFF = 45,
	DPC_MML_VIDLE_INFRA_OFF = 48,
	DPC_MML_VIDLE_MAINPLL_OFF = 51,
	DPC_MML_VIDLE_RESERVED = 54,
};

void dpc_enable(bool en);
void dpc_ddr_force_enable(const enum mtk_dpc_subsys subsys, const bool en);
void dpc_infra_force_enable(const enum mtk_dpc_subsys subsys, const bool en);
void dpc_dc_force_enable(const bool en);
void dpc_group_enable(const u16 group, bool en);
void dpc_config(const enum mtk_dpc_subsys subsys, bool en);
void dpc_mtcmos_vote(const enum mtk_dpc_subsys subsys, const u8 thread, const bool en);
void dpc_hrt_bw_set(const enum mtk_dpc_subsys subsys, const u32 bw_in_mb, bool force);
void dpc_srt_bw_set(const enum mtk_dpc_subsys subsys, const u32 bw_in_mb, bool force);
void dpc_dvfs_set(const enum mtk_dpc_subsys subsys, const u8 level, bool force);
int dpc_vidle_power_keep(const enum mtk_vidle_voter_user);
void dpc_vidle_power_release(const enum mtk_vidle_voter_user);

struct dpc_funcs {
	void (*dpc_enable)(bool en);
	void (*dpc_ddr_force_enable)(const enum mtk_dpc_subsys subsys, const bool en);
	void (*dpc_infra_force_enable)(const enum mtk_dpc_subsys subsys, const bool en);
	void (*dpc_dc_force_enable)(const bool en);
	void (*dpc_group_enable)(const u16 group, bool en);
	void (*dpc_config)(const enum mtk_dpc_subsys subsys, bool en);
	void (*dpc_mtcmos_vote)(const enum mtk_dpc_subsys subsys, const u8 thread, const bool en);
	int (*vidle_power_keep)(const enum mtk_vidle_voter_user);
	void (*vidle_power_release)(const enum mtk_vidle_voter_user);
	void (*dpc_hrt_bw_set)(const enum mtk_dpc_subsys subsys, const u32 bw_in_mb, bool force);
	void (*dpc_srt_bw_set)(const enum mtk_dpc_subsys subsys, const u32 bw_in_mb, bool force);
	void (*dpc_dvfs_set)(const enum mtk_dpc_subsys subsys, const u8 level, bool force);
};

#endif
