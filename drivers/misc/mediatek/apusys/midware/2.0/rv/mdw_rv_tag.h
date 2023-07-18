/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef __MTK_APU_MDW_APTAG_H__
#define __MTK_APU_MDW_APTAG_H__

#include "mdw_rv.h"
#define MDW_TAGS_CNT (3000)

enum mdw_cmd_status {
	MDW_CMD_ENQUE,
	MDW_CMD_START,
	MDW_CMD_DONE,
	MDW_CMD_SCHED,
};

struct mdw_rv_tag {
	int type;

	union mdw_tag_data {
		struct mdw_tag_cmd {
			uint32_t status;
			pid_t pid;
			uint64_t uid;
			uint64_t rvid;
			uint32_t num_subcmds;
			uint32_t priority;
			uint32_t softlimit;
			uint32_t pwr_dtime;
			uint64_t sc_rets;
			uint32_t pwr_plcy;
			uint32_t tolerance;
			uint64_t start_ts;
		} cmd;
		struct mdw_tag_subcmd {
			uint32_t status;
			uint64_t rvid;
			uint32_t sc_type;
			uint32_t sc_idx;
			uint32_t ipstart_ts;
			uint32_t ipend_ts;
			uint32_t was_preempted;
			uint32_t executed_core_bmp;
			uint32_t tcm_usage;
			uint32_t history_iptime;
		} subcmd;
	} d;
};

#if IS_ENABLED(CONFIG_MTK_APUSYS_DEBUG)
int mdw_rv_tag_init(void);
void mdw_rv_tag_deinit(void);
void mdw_rv_tag_show(struct seq_file *s);
void mdw_cmd_trace(struct mdw_cmd *c, uint32_t status);
void mdw_subcmd_trace(struct mdw_cmd *c, uint32_t sc_idx,
		uint32_t history_iptime, uint32_t status);
#else
static inline int mdw_rv_tag_init(void)
{
	return 0;
}

static inline void mdw_rv_tag_deinit(void)
{
}
static inline void mdw_rv_tag_show(struct seq_file *s)
{
}
void mdw_cmd_trace(struct mdw_cmd *c, uint32_t status)
{
}
void mdw_subcmd_trace(struct mdw_cmd *c, uint32_t sc_idx,
		uint32_t history_iptime, uint32_t status);
{
}
#endif

#endif

