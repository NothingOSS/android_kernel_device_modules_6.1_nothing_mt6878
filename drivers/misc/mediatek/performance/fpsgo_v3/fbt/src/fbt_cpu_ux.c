// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2023 MediaTek Inc.
 */
#include <linux/rbtree.h>
#include <linux/module.h>
#include <linux/slab.h>

#include <mt-plat/fpsgo_common.h>

#include "fpsgo_usedext.h"
#include "fpsgo_base.h"
#include "fpsgo_sysfs.h"
#include "fbt_usedext.h"
#include "fbt_cpu.h"
#include "fbt_cpu_platform.h"
#include "../fstb/fstb.h"
#include "xgf.h"
#include "mini_top.h"
#include "fps_composer.h"
#include "fpsgo_cpu_policy.h"
#include "fbt_cpu_ctrl.h"
#include "fbt_cpu_ux.h"

#define TARGET_UNLIMITED_FPS 240
#define NSEC_PER_HUSEC 100000

static DEFINE_MUTEX(fbt_mlock);

static struct kmem_cache *frame_info_cachep __ro_after_init;

static int fpsgo_ux_gcc_enable;
static int sbe_rescue_enable;
static int sbe_rescuing_frame_id;
static int sbe_enhance_f;

module_param(fpsgo_ux_gcc_enable, int, 0644);
module_param(sbe_enhance_f, int, 0644);

/* main function*/
static int nsec_to_100usec(unsigned long long nsec)
{
	unsigned long long husec;

	husec = div64_u64(nsec, (unsigned long long)NSEC_PER_HUSEC);

	return (int)husec;
}

static int fbt_ux_cal_perf(
	long long t_cpu_cur,
	long long target_time,
	unsigned int target_fps,
	unsigned int fps_margin,
	struct render_info *thread_info,
	unsigned long long ts,
	long aa, unsigned int target_fpks, int cooler_on)
{
	unsigned int blc_wt = 0U;
	unsigned long long cur_ts;
	struct fbt_boost_info *boost_info;
	int pid;
	unsigned long long buffer_id;
	unsigned long long t1, t2, t_Q2Q;
	long aa_n;

	if (!thread_info) {
		FPSGO_LOGE("ERROR %d\n", __LINE__);
		return 0;
	}

	cur_ts = fpsgo_get_time();
	pid = thread_info->pid;
	buffer_id = thread_info->buffer_id;
	boost_info = &(thread_info->boost_info);

	mutex_lock(&fbt_mlock);

	t1 = (unsigned long long)t_cpu_cur;
	t1 = nsec_to_100usec(t1);
	t2 = target_time;
	t_Q2Q = thread_info->Q2Q_time;
	t_Q2Q = nsec_to_100usec(t_Q2Q);
	aa_n = aa;

	if (fpsgo_ux_gcc_enable == 2) {
		fbt_cal_target_time_ns(thread_info->pid, thread_info->buffer_id,
			fbt_get_rl_ko_is_ready(), 2, target_fps, cooler_on, target_fpks,
			target_time, boost_info->last_target_time_ns, thread_info->Q2Q_time,
			thread_info->t_last_start + target_time, 0,
			thread_info->attr.expected_fps_margin_by_pid, 10, 10,
			0, 0, aa_n, aa_n, aa_n, 100, 100, 100, &t2);
		boost_info->last_target_time_ns = t2;
	}

	t2 = nsec_to_100usec(t2);

	if (aa_n < 0) {
		fpsgo_get_blc_mlock(__func__);
		if (thread_info->p_blc)
			blc_wt = thread_info->p_blc->blc;
		fpsgo_put_blc_mlock(__func__);
		aa_n = 0;
	} else {
		fbt_cal_aa(aa, t1, t_Q2Q, &aa_n);
		fbt_cal_blc(aa_n, t2, thread_info->p_blc->blc, t_Q2Q, 0, &blc_wt);
	}

	fpsgo_systrace_c_fbt(pid, buffer_id, aa_n, "[ux]aa");

	blc_wt = clamp(blc_wt, 1U, 100U);

	boost_info->target_fps = target_fps;
	boost_info->target_time = target_time;
	boost_info->last_blc = blc_wt;
	boost_info->last_normal_blc = blc_wt;
	//boost_info->cur_stage = FPSGO_JERK_INACTIVE;
	mutex_unlock(&fbt_mlock);
	return blc_wt;
}

static int fbt_ux_get_max_cap(int pid, unsigned long long bufID, int min_cap)
{
	int bhr_local = fbt_cpu_get_bhr();

	return fbt_get_max_cap(min_cap, 0, bhr_local, pid, bufID);
}

static void fbt_ux_set_cap(struct render_info *thr, int min_cap, int max_cap)
{
	int i;
	int local_dep_size = 0;
	char temp[7] = {"\0"};
	char *local_dep_str = NULL;
	struct fpsgo_loading *local_dep_arr = NULL;

	local_dep_str = kcalloc(MAX_DEP_NUM + 1, 7 * sizeof(char), GFP_KERNEL);
	if (!local_dep_str)
		goto out;

	local_dep_arr = kcalloc(MAX_DEP_NUM, sizeof(struct fpsgo_loading), GFP_KERNEL);
	if (!local_dep_arr)
		goto out;

	local_dep_size = fbt_determine_final_dep_list(thr, local_dep_arr);

	for (i = 0; i < local_dep_size; i++) {
		if (local_dep_arr[i].pid <= 0)
			continue;

		fbt_set_per_task_cap(local_dep_arr[i].pid, min_cap, max_cap);

		if (strlen(local_dep_str) == 0)
			snprintf(temp, sizeof(temp), "%d", local_dep_arr[i].pid);
		else
			snprintf(temp, sizeof(temp), ",%d", local_dep_arr[i].pid);

		if (strlen(local_dep_str) + strlen(temp) < 256)
			strncat(local_dep_str, temp, strlen(temp));
	}

	fpsgo_main_trace("[%d] dep-list %s", thr->pid, local_dep_str);

out:
	kfree(local_dep_str);
	kfree(local_dep_arr);
}

static void fbt_ux_set_cap_with_sbe(struct render_info *thr)
{
	int set_blc_wt = 0;
	int local_min_cap = 0;
	int local_max_cap = 100;

	set_blc_wt = thr->ux_blc_cur + thr->sbe_enhance;
	set_blc_wt = clamp(set_blc_wt, 0, 100);

	fpsgo_get_blc_mlock(__func__);
	if (thr->p_blc)
		thr->p_blc->blc = set_blc_wt;
	fpsgo_put_blc_mlock(__func__);

	fpsgo_get_fbt_mlock(__func__);
	local_min_cap = set_blc_wt;
	if (local_min_cap != 0)
		local_max_cap = fbt_ux_get_max_cap(thr->pid, thr->buffer_id,
					set_blc_wt);

	if (local_min_cap == 0 && local_max_cap == 100)
		fbt_check_max_blc_locked(thr->pid);
	else
		fbt_set_limit(thr->pid, local_min_cap, thr->pid, thr->buffer_id,
			thr->dep_valid_size, thr->dep_arr, thr, 0);
	fpsgo_put_fbt_mlock(__func__);

	fbt_ux_set_cap(thr, local_min_cap, local_max_cap);
	fpsgo_systrace_c_fbt(thr->pid, thr->buffer_id, set_blc_wt, "[ux]perf_idx");
}

void fbt_ux_frame_start(struct render_info *thr, unsigned long long ts)
{
	if (!thr)
		return;

	thr->ux_blc_cur = thr->ux_blc_next;
	fbt_ux_set_cap_with_sbe(thr);
}

void fbt_ux_frame_end(struct render_info *thr,
		unsigned long long start_ts, unsigned long long end_ts)
{
	struct fbt_boost_info *boost;
	long long runtime;
	int targettime, targetfps, targetfpks, fps_margin, cooler_on;
	int loading = 0L;
	int q_c_time, q_g_time;
	int ret;

	if (!thr)
		return;

	boost = &(thr->boost_info);

	runtime = thr->running_time;
	boost->frame_info[boost->f_iter].running_time = runtime;
	// fstb_query_dfrc
	fpsgo_fbt2fstb_query_fps(thr->pid, thr->buffer_id,
			&targetfps, &targettime, &fps_margin,
			&q_c_time, &q_g_time, &targetfpks, &cooler_on);
	boost->quantile_cpu_time = q_c_time;
	boost->quantile_gpu_time = q_g_time;	// [ux] unavailable, for statistic only.
	fpsgo_fbt_ux2fstb_query_dfrc(&targetfps, &targettime);

	if (!targetfps)
		targetfps = TARGET_UNLIMITED_FPS;

	if (start_ts == 0)
		goto EXIT;

	thr->Q2Q_time = end_ts - start_ts;

	fpsgo_systrace_c_fbt(thr->pid, thr->buffer_id, targetfps, "[ux]target_fps");
	fpsgo_systrace_c_fbt(thr->pid, thr->buffer_id,
		targettime, "[ux]target_time");
	fpsgo_systrace_c_fbt(thr->pid, thr->buffer_id,
		runtime, "[ux]running_time");

	fpsgo_get_fbt_mlock(__func__);
	ret = fbt_get_dep_list(thr);
	if (ret) {
		fpsgo_systrace_c_fbt(thr->pid, thr->buffer_id,
			ret, "[UX] fail dep-list");
		fpsgo_systrace_c_fbt(thr->pid, thr->buffer_id,
			0, "[UX] fail dep-list");
	}
	fpsgo_put_fbt_mlock(__func__);

	fpsgo_get_blc_mlock(__func__);
	if (thr->p_blc) {
		thr->p_blc->dep_num = thr->dep_valid_size;
		if (thr->dep_arr)
			memcpy(thr->p_blc->dep, thr->dep_arr,
					thr->dep_valid_size * sizeof(struct fpsgo_loading));
		else
			thr->p_blc->dep_num = 0;
	}
	fpsgo_put_blc_mlock(__func__);

	fbt_set_render_boost_attr(thr);
	loading = fbt_get_loading(thr, start_ts, end_ts);
	fpsgo_systrace_c_fbt(thr->pid, thr->buffer_id, loading, "[ux]compute_loading");

	/* unreliable targetfps */
	if (targetfps == -1) {
		fbt_reset_boost(thr);
		runtime = -1;
		goto EXIT;
	}

	thr->ux_blc_next = fbt_ux_cal_perf(runtime,
			targettime, targetfps, fps_margin,
			thr, end_ts, loading, targetfpks, cooler_on);

EXIT:
	thr->ux_blc_cur = 0;
	fbt_ux_set_cap_with_sbe(thr);
	fpsgo_fbt2fstb_update_cpu_frame_info(thr->pid, thr->buffer_id,
		thr->tgid, thr->frame_type,
		0, thr->running_time, targettime,
		thr->ux_blc_next, 100, 0, 0);
}

void fbt_ux_frame_err(struct render_info *thr,
unsigned long long ts)
{
	if (!thr)
		return;

	thr->ux_blc_cur = 0;
	fbt_ux_set_cap_with_sbe(thr);
}

void fpsgo_ux_delete_frame_info(struct render_info *thr, struct ux_frame_info *info)
{
	if (!info)
		return;
	rb_erase(&info->entry, &(thr->ux_frame_info_tree));
	kmem_cache_free(frame_info_cachep, info);
}

struct ux_frame_info *fpsgo_ux_search_and_add_frame_info(struct render_info *thr,
		unsigned long long frameID, unsigned long long start_ts, int action)
{
	struct rb_node **p = &(thr->ux_frame_info_tree).rb_node;
	struct rb_node *parent = NULL;
	struct ux_frame_info *tmp = NULL;

	fpsgo_lockprove(__func__);

	while (*p) {
		parent = *p;
		tmp = rb_entry(parent, struct ux_frame_info, entry);
		if (frameID < tmp->frameID)
			p = &(*p)->rb_left;
		else if (frameID > tmp->frameID)
			p = &(*p)->rb_right;
		else
			return tmp;
	}
	if (action == 0)
		return NULL;
	if (frame_info_cachep)
		tmp = kmem_cache_alloc(frame_info_cachep,
			GFP_KERNEL | __GFP_ZERO);
	if (!tmp)
		return NULL;

	tmp->frameID = frameID;
	tmp->start_ts = start_ts;
	rb_link_node(&tmp->entry, parent, p);
	rb_insert_color(&tmp->entry, &(thr->ux_frame_info_tree));

	return tmp;
}

void fpsgo_ux_reset(struct render_info *thr)
{
	struct rb_node *cur;
	struct rb_node *next;
	struct ux_frame_info *tmp = NULL;

	fpsgo_lockprove(__func__);

	cur = rb_first(&(thr->ux_frame_info_tree));

	while (cur) {
		next = rb_next(cur);
		tmp = rb_entry(cur, struct ux_frame_info, entry);
		rb_erase(&tmp->entry, &(thr->ux_frame_info_tree));
		kmem_cache_free(frame_info_cachep, tmp);
		cur = next;
	}

}

void fpsgo_sbe2fbt_rescue(struct render_info *thr, int start, int enhance,
		unsigned long long frame_id)
{

	if (!thr || !sbe_rescue_enable)	//thr must find the 5566 one.
		return;

	mutex_lock(&fbt_mlock);
	if (start) {
		if (frame_id)
			sbe_rescuing_frame_id = frame_id;
		if (thr->boost_info.sbe_rescue != 0)
			goto leave;
		thr->boost_info.sbe_rescue = 1;
		thr->sbe_enhance = enhance < 0 ?  sbe_enhance_f : enhance;
		fbt_ux_set_cap_with_sbe(thr);
		fpsgo_systrace_c_fbt(thr->pid, thr->buffer_id, thr->sbe_enhance, "sbe rescue");
	} else {
		if (thr->boost_info.sbe_rescue == 0)
			goto leave;
		sbe_rescuing_frame_id = -1;
		thr->boost_info.sbe_rescue = 0;
		thr->sbe_enhance = 0;
		fbt_ux_set_cap_with_sbe(thr);
		fpsgo_systrace_c_fbt(thr->pid, thr->buffer_id, thr->sbe_enhance, "sbe rescue");
	}

leave:
	mutex_unlock(&fbt_mlock);
}

void __exit fbt_cpu_ux_exit(void)
{
	kmem_cache_destroy(frame_info_cachep);
}

int __init fbt_cpu_ux_init(void)
{
	fpsgo_ux_gcc_enable = 0;
	sbe_rescue_enable = fbt_get_default_sbe_rescue_enable();
	sbe_rescuing_frame_id = -1;
	sbe_enhance_f = 50;
	frame_info_cachep = kmem_cache_create("ux_frame_info",
		sizeof(struct ux_frame_info), 0, SLAB_HWCACHE_ALIGN, NULL);
	if (!frame_info_cachep)
		return -1;

	return 0;
}
