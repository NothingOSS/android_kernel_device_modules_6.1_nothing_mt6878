/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2022 MediaTek Inc.
 */

#ifndef __MTK_CAM_JOB_STATE_IMPL_H
#define __MTK_CAM_JOB_STATE_IMPL_H

#include "mtk_cam-job.h"
#include "mtk_cam-job_state.h"
#include "mtk_cam-job_state_impl_guard.h"

struct state_transition {
	int dst_state;
	int on_event;
	int (*guard)(struct mtk_cam_job_state *s, struct transition_param *p);
	int action;
};

struct transitions_entry {
	struct state_transition *trans;
	int size;
	int cached_event_mask;
};

struct state_table {
	struct transitions_entry *entries;
	int size;
	int cached_tbl_event_mask;
};

#define STATE_TRANS(prefix, state)	prefix ## _ ##state

#define _ADD_TRANS_ENTRY(s, name)	\
	[s] = {name, ARRAY_SIZE(name)}
#define ADD_TRANS_ENTRY(prefix, state)	\
	_ADD_TRANS_ENTRY(state, STATE_TRANS(prefix, state))

#define STATE_TABLE(_entries)
#define DECL_STATE_TABLE(tbl_name, _entries)	\
static struct state_table tbl_name = {		\
	.entries = _entries,			\
	.size = ARRAY_SIZE(_entries),		\
}

int loop_each_transition(struct state_table *tbl,
			 struct mtk_cam_job_state *s, int state_type,
			 struct transition_param *p);

#endif //__MTK_CAM_JOB_STATE_IMPL_H
