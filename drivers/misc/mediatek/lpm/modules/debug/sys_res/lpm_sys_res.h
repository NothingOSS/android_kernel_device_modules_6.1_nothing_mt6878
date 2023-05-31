/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2023 MediaTek Inc.
 */

#ifndef __MTK_LPM_SYS_RES__
#define __MTK_LPM_SYS_RES__


#include <linux/types.h>
#include <linux/spinlock.h>
#include <swpm_module_ext.h>

enum _sys_res_scene{
	SYS_RES_SCENE_COMMON = 0,
	SYS_RES_SCENE_SUSPEND,
	SYS_RES_SCENE_LAST_SUSPEND_DIFF,
	SYS_RES_SCENE_LAST_DIFF,
	SYS_RES_SCENE_LAST_SYNC,
	SYS_RES_SCENE_TEMP,
	SYS_RES_SCENE_NUM,
};

enum _sys_res_system_resource {
	SYS_RES_SYS_VCORE = 0,
	SYS_RES_SYS_26M,
	SYS_RES_SYS_PMIC,
	SYS_RES_SYS_INFRA,
	SYS_RES_SYS_BUSPLL,
	SYS_RES_SYS_EMI,
	SYS_RES_SYS_APSRC,
	SYS_RES_SYS_RESOURCE_NUM,
};

struct sys_res_record {
	struct res_sig_stats *spm_res_sig_stats_ptr;
};

#define SYS_RES_SYS_NAME_LEN (10)
struct sys_res_group_info {
	int group_id;
	char name[SYS_RES_SYS_NAME_LEN];
	unsigned int sys_index;
	unsigned int sig_table_index;
	unsigned int group_num;
	unsigned int threshold;
};


struct lpm_sys_res_ops {
	struct sys_res_record* (*get)(unsigned int scene);
	void (*update)(void);
	struct sys_res_record* (*get_last_suspend)(void);
	uint64_t (*get_detail)(struct sys_res_record *record, int op, unsigned int val);
	unsigned int (*get_threshold)(void);
	void (*set_threshold)(unsigned int val);
	spinlock_t lock;
};

int lpm_sys_res_init(void);
void lpm_sys_res_exit(void);

int register_lpm_sys_res_ops(struct lpm_sys_res_ops *ops);
void unregister_lpm_sys_res_ops(void);
struct lpm_sys_res_ops *get_lpm_sys_res_ops(void);

#endif
