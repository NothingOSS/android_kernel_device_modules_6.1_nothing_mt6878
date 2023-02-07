/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2022 MediaTek Inc.
 */

#ifndef __MTK_CAM_ENGINE_FSM_H
#define __MTK_CAM_ENGINE_FSM_H

#include <linux/compiler.h>

enum {
	STATE_NO_REQ,
	STATE_PROCESSING,
};

struct engine_fsm {
	int state;
	int cookie_inner;
};

static inline void engine_fsm_reset(struct engine_fsm *fsm)
{
	fsm->state = STATE_NO_REQ;
	fsm->cookie_inner = 0;
}

static inline void engine_fsm_sof(struct engine_fsm *fsm, int cookie_inner)
{
	bool inner_update;

	inner_update = cookie_inner != fsm->cookie_inner;

	fsm->state = inner_update ? STATE_PROCESSING : STATE_NO_REQ;
	fsm->cookie_inner = cookie_inner;
}

static inline int engine_fsm_hw_done(struct engine_fsm *fsm, int *cookie_done)
{
	int ret = 0;

	if (!cookie_done)
		return 0;

#ifdef TODO_RWFBC
	/* note: for rwfbc fake sw_p1_done issue */
	if (fsm->state == STATE_NO_REQ) {
		*cookie_done = 0;
		ret = -1;
	} else
		*cookie_done = fsm->cookie_inner;
#else
	*cookie_done = fsm->cookie_inner;
#endif
	return ret;
}

#endif /* __MTK_CAM_ENGINE_FSM_H */
