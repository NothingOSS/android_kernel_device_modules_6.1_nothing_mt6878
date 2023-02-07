/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2023 MediaTek Inc.
 */

#ifndef __MTK_CAM_ENGINE_H
#define __MTK_CAM_ENGINE_H

#include <linux/atomic.h>

#include "mtk_cam-engine_fsm.h"

struct apply_cq_ref {
	atomic_t cnt;
};

static inline void apply_cq_ref_reset(struct apply_cq_ref *ref)
{
	atomic_set(&ref->cnt, 0);
}

static inline void apply_cq_ref_set_cnt(struct apply_cq_ref *ref, int cnt)
{
	/* it's abnormal if cnt is not zero */
	WARN_ON(atomic_read(&ref->cnt));
	atomic_set(&ref->cnt, cnt);
}

static inline int apply_cq_ref_set(struct apply_cq_ref **ref,
				   struct apply_cq_ref *target)
{
	if (*ref)
		return -1;
	*ref = target;
	return 0;
}

static inline bool apply_cq_ref_handle_cq_done(struct apply_cq_ref *ref)
{
	return atomic_dec_and_test(&ref->cnt);
}

static inline bool engine_handle_cq_done(struct apply_cq_ref **ref)
{
	struct apply_cq_ref *_ref = *ref;

	*ref = NULL;
	return apply_cq_ref_handle_cq_done(_ref);
}

#endif //__MTK_CAM_ENGINE_H
