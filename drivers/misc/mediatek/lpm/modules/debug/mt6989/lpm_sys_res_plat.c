// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2023 MediaTek Inc.
 */


#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#include <linux/slab.h>
#include <linux/io.h>
#include <linux/rtc.h>
#include <linux/wakeup_reason.h>
#include <linux/syscore_ops.h>
#include <linux/suspend.h>
#include <linux/spinlock.h>

#include <lpm.h>
#include <lpm_module.h>
#include <lpm_spm_comm.h>
#include <lpm_dbg_common_v2.h>
#include <lpm_dbg_fs_common.h>
#include <lpm_dbg_trace_event.h>
#include <lpm_dbg_logger.h>
#include <lpm_trace_event/lpm_trace_event.h>
#include <spm_reg.h>
#include <pwr_ctrl.h>
#include <mt-plat/mtk_ccci_common.h>
#include <lpm_timer.h>
#include <mtk_lpm_sysfs.h>
#include <mtk_cpupm_dbg.h>
#include <lpm_sys_res.h>
#include <lpm_sys_res_plat.h>

#include <swpm_module_ext.h>
#include <swpm_v6989_ext.h>

static struct sys_res_record sys_res_record[SYS_RES_SCENE_NUM];
static unsigned int sys_res_last_buffer_index;
static unsigned int sys_res_temp_buffer_index;
static unsigned int sys_res_last_suspend_diff_buffer_index;
static unsigned int sys_res_last_diff_buffer_index;

struct sys_res_group_info sys_res_group_info[NR_SPM_GRP] = {
	{DDREN_REQ,	  "DDREN",   286,   0,  32, 30},
	{APSRC_REQ,   "APSRC",   288,  32,  33, 30},
	{EMI_REQ,     "EMI",     289,  65,  33, 30},
	{MAINPLL_REQ, "MAINPLL", 290,  98,  34, 30},
	{INFRA_REQ,   "INFRA",   291, 132,  35, 30},
	{F26M_REQ,    "26M",     292, 167,  36, 30},
	{PMIC_REQ,    "PMIC",    293, 203,  33, 30},
	{VCORE_REQ,   "VCORE",   294, 236,  11, 30},
	{PWR_ACT,     "PWR_ACT",   0, 247,  37, 30},
	{SYS_STA,     "SYS_STA",   0, 284,  11, 30},
};


static int lpm_sys_res_alloc(struct sys_res_record *record)
{
	struct res_sig_stats *spm_res_sig_stats_ptr;

	if (!record)
		return -1;


	spm_res_sig_stats_ptr =
	kmalloc_array(1, sizeof(struct res_sig_stats), GFP_KERNEL);
	if (!spm_res_sig_stats_ptr)
		goto RES_SIG_ALLOC_ERROR;

	get_res_sig_stats(spm_res_sig_stats_ptr);
	spm_res_sig_stats_ptr->res_sig_tbl =
	kmalloc_array(spm_res_sig_stats_ptr->res_sig_num,
			sizeof(struct res_sig), GFP_KERNEL);
	if (!spm_res_sig_stats_ptr->res_sig_tbl)
		goto RES_SIG_ALLOC_TABLE_ERROR;

	get_res_sig_stats(spm_res_sig_stats_ptr);
	record->spm_res_sig_stats_ptr = spm_res_sig_stats_ptr;

	return 0;

RES_SIG_ALLOC_TABLE_ERROR:
	kfree(spm_res_sig_stats_ptr);
	record->spm_res_sig_stats_ptr = NULL;
RES_SIG_ALLOC_ERROR:
	return -1;

}

static void lpm_sys_res_free(struct sys_res_record *record)
{
	if(record && record->spm_res_sig_stats_ptr) {
		kfree(record->spm_res_sig_stats_ptr->res_sig_tbl);
		kfree(record->spm_res_sig_stats_ptr);
		record->spm_res_sig_stats_ptr = NULL;
	}
}


static void __sync_lastest_lpm_sys_res_record(struct sys_res_record *record)
{
	if (!record)
		return;
#if IS_ENABLED(CONFIG_MTK_SWPM_MODULE)
	sync_latest_data();
	get_res_sig_stats(record->spm_res_sig_stats_ptr);
#endif

}
static void __lpm_sys_res_record_diff(struct sys_res_record *result,
				   struct sys_res_record *prev,
				   struct sys_res_record *cur)
{
	int i;

	if (!result || !prev || !cur)
		return;

	result->spm_res_sig_stats_ptr->suspend_time =
		prev->spm_res_sig_stats_ptr->suspend_time -
		cur->spm_res_sig_stats_ptr->suspend_time;

	result->spm_res_sig_stats_ptr->duration_time =
		prev->spm_res_sig_stats_ptr->duration_time -
		cur->spm_res_sig_stats_ptr->duration_time;

	for (i = 0; i < result->spm_res_sig_stats_ptr->res_sig_num; i++) {
		result->spm_res_sig_stats_ptr->res_sig_tbl[i].time =
		prev->spm_res_sig_stats_ptr->res_sig_tbl[i].time -
		cur->spm_res_sig_stats_ptr->res_sig_tbl[i].time;
	}

}

static void __lpm_sys_res_record_add(struct sys_res_record *result,
				   struct sys_res_record *delta)
{
	int i;

	if (!result || !delta)
		return;

	result->spm_res_sig_stats_ptr->suspend_time +=
		delta->spm_res_sig_stats_ptr->suspend_time;

	result->spm_res_sig_stats_ptr->duration_time +=
		delta->spm_res_sig_stats_ptr->duration_time;

	for (i = 0; i < result->spm_res_sig_stats_ptr->res_sig_num; i++) {
		result->spm_res_sig_stats_ptr->res_sig_tbl[i].time +=
		delta->spm_res_sig_stats_ptr->res_sig_tbl[i].time;
	}
}

static void update_lpm_sys_res_record(void)
{
	unsigned int temp;

	__sync_lastest_lpm_sys_res_record(&sys_res_record[sys_res_temp_buffer_index]);

	__lpm_sys_res_record_diff(&sys_res_record[sys_res_last_diff_buffer_index],
			&sys_res_record[sys_res_temp_buffer_index],
			&sys_res_record[sys_res_last_buffer_index]);

	if (sys_res_record[sys_res_last_diff_buffer_index].spm_res_sig_stats_ptr->suspend_time > 0) {
		__lpm_sys_res_record_add(&sys_res_record[SYS_RES_SCENE_SUSPEND],
				      &sys_res_record[sys_res_last_diff_buffer_index]);

		temp = sys_res_last_diff_buffer_index;
		sys_res_last_diff_buffer_index = sys_res_last_suspend_diff_buffer_index;
		sys_res_last_suspend_diff_buffer_index = temp;

	} else {
		__lpm_sys_res_record_add(&sys_res_record[SYS_RES_SCENE_COMMON],
				      &sys_res_record[sys_res_last_diff_buffer_index]);
	}

	temp = sys_res_temp_buffer_index;
	sys_res_temp_buffer_index = sys_res_last_buffer_index;
	sys_res_last_buffer_index = temp;
}

static struct sys_res_record *get_lpm_sys_res_record(unsigned int scene)
{
	if (scene >= SYS_RES_SCENE_NUM)
		return NULL;

	return &sys_res_record[scene];
}

static struct sys_res_record *get_last_suspend(void)
{
	return &sys_res_record[sys_res_last_suspend_diff_buffer_index];
}



static uint64_t lpm_sys_res_get_detail(struct sys_res_record *record, int op, unsigned int val)
{
	uint64_t ret = 0;
	uint64_t total_time = 0, sig_time = 0;

	if (!record)
		return 0;

	switch (op) {
	case SYS_RES_DURATION:
		ret = record->spm_res_sig_stats_ptr->duration_time;
		break;
	case SYS_RES_SUSPEND_TIME:
		ret = record->spm_res_sig_stats_ptr->suspend_time;
		break;
	case SYS_RES_SIG_TIME:
		if (val >= record->spm_res_sig_stats_ptr->res_sig_num)
			return 0;
		ret = record->spm_res_sig_stats_ptr->res_sig_tbl[val].time;
		break;
	case SYS_RES_SIG_ID:
		if (val >= record->spm_res_sig_stats_ptr->res_sig_num)
			return 0;
		ret = record->spm_res_sig_stats_ptr->res_sig_tbl[val].sig_id;
		break;
	case SYS_RES_SIG_GROUP_ID:
		if (val >= record->spm_res_sig_stats_ptr->res_sig_num)
			return 0;
		ret = record->spm_res_sig_stats_ptr->res_sig_tbl[val].grp_id;
		break;
	case SYS_RES_SIG_OVERALL_RATIO:
		if (val >= record->spm_res_sig_stats_ptr->res_sig_num)
			return 0;
		total_time = record->spm_res_sig_stats_ptr->duration_time;
		sig_time = record->spm_res_sig_stats_ptr->res_sig_tbl[val].time;
		ret = sig_time < total_time ?
			(sig_time * 100) / total_time : 100;
		break;
	case SYS_RES_SIG_SUSPEND_RATIO:
		if (val >= record->spm_res_sig_stats_ptr->res_sig_num)
			return 0;
		total_time = record->spm_res_sig_stats_ptr->suspend_time;
		sig_time = record->spm_res_sig_stats_ptr->res_sig_tbl[val].time;
		ret = sig_time < total_time ?
			(sig_time * 100) / total_time : 100;
		break;
	case SYS_RES_SIG_ADDR:
		if (val >= record->spm_res_sig_stats_ptr->res_sig_num)
			return 0;
		ret = (uint64_t)(&record->spm_res_sig_stats_ptr->res_sig_tbl[val]);
		break;
	default:
		break;
	};
	return ret;
}

static unsigned int  lpm_sys_res_get_threshold(void)
{
	return sys_res_group_info[0].threshold;
}

static void lpm_sys_res_set_threshold(unsigned int val)
{
	int i;

	if (val > 100)
		val = 100;

	for (i = 0; i < NR_SPM_GRP; i++)
		sys_res_group_info[i].threshold = val;
}

static struct lpm_sys_res_ops sys_res_ops = {
	.get = get_lpm_sys_res_record,
	.update = update_lpm_sys_res_record,
	.get_last_suspend = get_last_suspend,
	.get_detail = lpm_sys_res_get_detail,
	.get_threshold = lpm_sys_res_get_threshold,
	.set_threshold = lpm_sys_res_set_threshold,
};

int lpm_sys_res_plat_init(void)
{
	int ret, i, j;

	for (i = 0; i < SYS_RES_SCENE_NUM; i++) {
		ret = lpm_sys_res_alloc(&sys_res_record[i]);
		if(ret) {
			for (j = i - 1; j >= 0; j--)
				lpm_sys_res_free(&sys_res_record[i]);
			pr_info("[LPM] sys_res alloc fail\n");
			return ret;
		}
	}

	sys_res_last_buffer_index = SYS_RES_SCENE_LAST_SYNC;
	sys_res_temp_buffer_index = SYS_RES_SCENE_TEMP;

	sys_res_last_suspend_diff_buffer_index = SYS_RES_SCENE_LAST_SUSPEND_DIFF;
	sys_res_last_diff_buffer_index = SYS_RES_SCENE_LAST_DIFF;

	ret = register_lpm_sys_res_ops(&sys_res_ops);

	return 0;
}


void lpm_sys_res_plat_deinit(void)
{
	int i;

	for (i = 0; i < SYS_RES_SCENE_NUM; i++)
		lpm_sys_res_free(&sys_res_record[i]);

	unregister_lpm_sys_res_ops();
}

