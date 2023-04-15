/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2023 MediaTek Inc.
 */

#ifndef __MTK_DPC_H__
#define __MTK_DPC_H__

#define DPCFUNC(fmt, args...) \
	pr_info("[dpc] %s:%d " fmt "\n", __func__, __LINE__, ##args)

#define DPCERR(fmt, args...) \
	pr_info("[dpc][err] %s:%d " fmt "\n", __func__, __LINE__, ##args)

enum mtk_dpc_subsys {
	DPC_SUBSYS_DISP = 0,
	DPC_SUBSYS_DISP0 = 0,
	DPC_SUBSYS_DISP1 = 1,
	DPC_SUBSYS_OVL0 = 2,
	DPC_SUBSYS_OVL1 = 3,
	DPC_SUBSYS_MML = 4,
	DPC_SUBSYS_MML1 = 4,
	DPC_SUBSYS_TOTAL,
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

void dpc_group_enable(const u16 group, bool en);
void dpc_config(const enum mtk_dpc_subsys subsys);
void dpc_mtcmos_vote(const enum mtk_dpc_subsys subsys, const u8 thread, const bool en);

#endif
