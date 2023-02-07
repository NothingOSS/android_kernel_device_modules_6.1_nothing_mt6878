// SPDX-License-Identifier: GPL-2.0
//
// Copyright (c) 2022 MediaTek Inc.

#include "mtk_cam-job_state_impl.h"

static struct state_transition STATE_TRANS(m2m, S_ISP_COMPOSING)[] = {
	{
		S_ISP_APPLYING, CAMSYS_EVENT_ACK,
		guard_ack_apply_m2m_directly, ACTION_APPLY_ISP
	},
	{
		S_ISP_COMPOSED, CAMSYS_EVENT_ACK,
		guard_ack_eq, 0
	},
};

static struct state_transition STATE_TRANS(m2m, S_ISP_COMPOSED)[] = {
	{
		S_ISP_APPLYING, CAMSYS_EVENT_IRQ_FRAME_DONE,
		guard_apply_m2m, ACTION_APPLY_ISP,
	},
};

static struct state_transition STATE_TRANS(m2m, S_ISP_APPLYING)[] = {
	{
		S_ISP_PROCESSING, CAMSYS_EVENT_IRQ_L_CQ_DONE,
		guard_outer_eq, ACTION_TRIGGER
	},
};

static struct state_transition STATE_TRANS(m2m, S_ISP_PROCESSING)[] = {
	{
		S_ISP_DONE, CAMSYS_EVENT_IRQ_FRAME_DONE,
		NULL, ACTION_BUFFER_DONE
	},
};
static struct transitions_entry m2m_isp_entries[NR_S_ISP_STATE] = {
	ADD_TRANS_ENTRY(m2m, S_ISP_COMPOSING),
	ADD_TRANS_ENTRY(m2m, S_ISP_COMPOSED),
	ADD_TRANS_ENTRY(m2m, S_ISP_APPLYING),
	ADD_TRANS_ENTRY(m2m, S_ISP_PROCESSING),
};
DECL_STATE_TABLE(m2m_isp_tbl, m2m_isp_entries);

static int m2m_send_event(struct mtk_cam_job_state *s,
			    struct transition_param *p)
{
	int ret;

	ret = loop_each_transition(&m2m_isp_tbl, s, ISP_STATE, p);

	return ret < 0 ? -1 : 0;
}

static struct mtk_cam_job_state_ops m2m_state_ops = {
	.send_event = m2m_send_event,
	//.is_sensor_updated
	//.is_next_sensor_applicable
	//.is_next_isp_applicable
};

int mtk_cam_job_state_init_m2m(struct mtk_cam_job_state *s,
			       const struct mtk_cam_job_state_cb *cb)
{
	s->ops = &m2m_state_ops;

	mtk_cam_job_state_set(s, ISP_STATE, S_ISP_NOT_SET);

	s->cb = cb;
	return 0;
}

