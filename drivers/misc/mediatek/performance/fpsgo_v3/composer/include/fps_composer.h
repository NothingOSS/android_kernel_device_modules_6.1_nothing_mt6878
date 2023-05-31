/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */
#ifndef __FPS_COMPOSER_H__
#define __FPS_COMPOSER_H__

#include <linux/rbtree.h>

enum FPSGO_COM_ERROR {
	FPSGO_COM_IS_RENDER,
	FPSGO_COM_TASK_NOT_EXIST,
	FPSGO_COM_IS_SF,
};

enum FPSGO_SBE_MASK {
	FPSGO_CONTROL,
	FPSGO_MAX_TARGET_FPS,
	FPSGO_RESCUE_ENABLE,
	FPSGO_RL_ENABLE,
	FPSGO_GCC_DISABLE,
	FPSGO_QUOTA_DISABLE,
};

struct connect_api_info {
	struct rb_node rb_node;
	struct list_head render_list;
	int pid;
	int tgid;
	unsigned long long buffer_id;
	unsigned long long buffer_key;
	int api;
};

struct fpsgo_com_policy_cmd {
	int tgid;
	int bypass_non_SF_by_pid;
	int control_api_mask_by_pid;
	int control_hwui_by_pid;
	unsigned long long ts;
	struct rb_node rb_node;
};

int fpsgo_composer_init(void);
void fpsgo_composer_exit(void);

void fpsgo_ctrl2comp_dequeue_end(int pid,
			unsigned long long dequeue_end_time,
			unsigned long long identifier);
void fpsgo_ctrl2comp_dequeue_start(int pid,
			unsigned long long dequeue_start_time,
			unsigned long long identifier);
void fpsgo_ctrl2comp_enqueue_end(int pid,
			unsigned long long enqueue_end_time,
			unsigned long long identifier);
void fpsgo_ctrl2comp_enqueue_start(int pid,
			unsigned long long enqueue_start_time,
			unsigned long long identifier);
void fpsgo_ctrl2comp_bqid(int pid, unsigned long long buffer_id,
			int queue_SF, unsigned long long identifier,
			int create);
void fpsgo_ctrl2comp_hint_frame_start(int pid,
			unsigned long long frameID,
			unsigned long long enqueue_start_time,
			unsigned long long identifier);
void fpsgo_ctrl2comp_hint_frame_end(int pid,
			unsigned long long frameID,
			unsigned long long enqueue_start_time,
			unsigned long long identifier);
void fpsgo_ctrl2comp_hint_frame_err(int pid,
			unsigned long long frameID,
			unsigned long long time,
			unsigned long long identifier);

void fpsgo_ctrl2comp_connect_api(int pid, int api,
	unsigned long long identifier);
void fpsgo_ctrl2comp_disconnect_api(int pid, int api,
			unsigned long long identifier);
void fpsgo_ctrl2comp_acquire(int p_pid, int c_pid, int c_tid,
	int api, unsigned long long buffer_id);
int fpsgo_ctrl2comp_set_sbe_policy(int tgid, char *name,
	unsigned long mask, int start);
void fpsgo_base2comp_check_connect_api(void);
int fpsgo_base2comp_check_connect_api_tree_empty(void);
int switch_ui_ctrl(int pid, int set_ctrl);
void fpsgo_set_fpsgo_is_boosting(int is_boosting);
int fpsgo_get_fpsgo_is_boosting(void);
int register_get_fpsgo_is_boosting(fpsgo_notify_is_boost_cb func_cb);
int unregister_get_fpsgo_is_boosting(fpsgo_notify_is_boost_cb func_cb);
int fpsgo_com2other_notify_fpsgo_is_boosting(int boost);
void fpsgo_com_notify_fpsgo_is_boost(int enable);

int fpsgo_ctrl2comp_adpf_create_session(int tgid, int render_tid, unsigned long long buffer_id,
	int *dep_arr, int dep_num, unsigned long long target_time);
int fpsgo_ctrl2comp_report_workload(int tgid, int render_tid, unsigned long long buffer_id,
	unsigned long long *tcpu_arr, unsigned long long *ts_arr, int num);
void fpsgo_ctrl2comp_adpf_resume(int render_tid, unsigned long long buffer_id);
void fpsgo_ctrl2comp_adpf_pause(int render_tid, unsigned long long buffer_id);
void fpsgo_ctrl2comp_adpf_close(int tgid, int render_tid, unsigned long long buffer_id);
int fpsgo_ctrl2comp_set_target_time(int tgid, int render_tid, unsigned long long buffer_id,
	unsigned long long target_time);
int fpsgo_ctrl2comp_adpf_set_dep_list(int tgid, int render_tid, unsigned long long buffer_id,
	int *dep_arr, int dep_num);

#endif

