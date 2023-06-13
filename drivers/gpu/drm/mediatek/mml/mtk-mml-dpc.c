// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 * Author: Chirs-YC Chen <chris-yc.chen@mediatek.com>
 */

#include "mtk-mml-dpc.h"
#include "mtk-mml-core.h"

struct dpc_funcs mml_dpc_driver;

void mml_dpc_register(const struct dpc_funcs *funcs)
{
	mml_dpc_driver.dpc_enable = funcs->dpc_enable;
	mml_dpc_driver.dpc_ddr_force_enable = funcs->dpc_ddr_force_enable;
	mml_dpc_driver.dpc_infra_force_enable = funcs->dpc_infra_force_enable;
	mml_dpc_driver.dpc_dc_force_enable = funcs->dpc_dc_force_enable;
	mml_dpc_driver.dpc_group_enable = funcs->dpc_group_enable;
	mml_dpc_driver.dpc_config = funcs->dpc_config;
	mml_dpc_driver.dpc_mtcmos_vote = funcs->dpc_mtcmos_vote;
	mml_dpc_driver.vidle_power_keep = funcs->vidle_power_keep;
	mml_dpc_driver.vidle_power_release = funcs->vidle_power_release;
	mml_dpc_driver.dpc_hrt_bw_set = funcs->dpc_hrt_bw_set;
	mml_dpc_driver.dpc_srt_bw_set = funcs->dpc_srt_bw_set;
	mml_dpc_driver.dpc_dvfs_set = funcs->dpc_dvfs_set;
}
EXPORT_SYMBOL_GPL(mml_dpc_register);

void mml_dpc_enable(bool en)
{
	if (mml_dpc_driver.dpc_enable == NULL) {
		mml_err("%s dpc_enable not exist", __func__);
		return;
	}

	mml_dpc_driver.dpc_enable(en);
}

void mml_dpc_dc_force_enable(bool en)
{
	if (mml_dpc_driver.dpc_dc_force_enable == NULL) {
		mml_err("%s dpc_dc_force_enable not exist", __func__);
		return;
	}

	mml_dpc_driver.dpc_dc_force_enable(en);
}

void mml_dpc_group_enable(const u16 group, bool en)
{
	if (mml_dpc_driver.dpc_group_enable == NULL) {
		mml_err("%s dpc_group_enable not exist", __func__);
		return;
	}

	mml_dpc_driver.dpc_group_enable(group, en);
}

void mml_dpc_config(const enum mtk_dpc_subsys subsys, bool en)
{
	if (mml_dpc_driver.dpc_config == NULL) {
		mml_err("%s dpc_config not exist", __func__);
		return;
	}

	mml_dpc_driver.dpc_config(subsys, en);
}

void mml_dpc_mtcmos_vote(const enum mtk_dpc_subsys subsys,
			 const u8 thread, const bool en)
{
	if (mml_dpc_driver.dpc_mtcmos_vote == NULL) {
		mml_err("%s dpc_mtcmos_vote not exist", __func__);
		return;
	}

	mml_dpc_driver.dpc_mtcmos_vote(subsys, thread, en);
}

int mml_dpc_power_keep(void)
{
	if (mml_dpc_driver.vidle_power_keep == NULL) {
		mml_err("%s dpc_power_keep not exist", __func__);
		return -1;
	}

	mml_msg_dpc("%s exception flow keep", __func__);
	return mml_dpc_driver.vidle_power_keep(DISP_VIDLE_USER_MML);
}

void mml_dpc_power_release(void)
{
	if (mml_dpc_driver.vidle_power_release == NULL) {
		mml_err("%s dpc_power_release not exist", __func__);
		return;
	}

	mml_msg_dpc("%s exception flow release", __func__);
	mml_dpc_driver.vidle_power_release(DISP_VIDLE_USER_MML);
}

void mml_dpc_hrt_bw_set(const enum mtk_dpc_subsys subsys,
			const u32 bw_in_mb)
{
	if (mml_dpc_driver.dpc_hrt_bw_set == NULL) {
		mml_err("%s dpc_hrt_bw_set not exist", __func__);
		return;
	}

	mml_dpc_driver.dpc_hrt_bw_set(subsys, bw_in_mb, true);
}

void mml_dpc_srt_bw_set(const enum mtk_dpc_subsys subsys,
			const u32 bw_in_mb)
{
	if (mml_dpc_driver.dpc_srt_bw_set == NULL) {
		mml_err("%s dpc_srt_bw_set not exist", __func__);
		return;
	}

	mml_dpc_driver.dpc_srt_bw_set(subsys, bw_in_mb, true);
}
void mml_dpc_dvfs_set(const enum mtk_dpc_subsys subsys,
		      const u8 level)
{
	if (mml_dpc_driver.dpc_dvfs_set == NULL) {
		mml_err("%s dpc_dvfs_set not exist", __func__);
		return;
	}

	mml_dpc_driver.dpc_dvfs_set(subsys, level, true);
}
