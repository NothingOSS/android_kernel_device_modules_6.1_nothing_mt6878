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

void fpsgo_ctrl2comp_connect_api(int pid, int api,
	unsigned long long identifier);
void fpsgo_ctrl2comp_disconnect_api(int pid, int api,
			unsigned long long identifier);
void fpsgo_ctrl2comp_acquire(int p_pid, int c_pid, int c_tid,
	int api, unsigned long long buffer_id);
void fpsgo_base2comp_check_connect_api(void);
int fpsgo_base2comp_check_connect_api_tree_empty(void);

#endif

