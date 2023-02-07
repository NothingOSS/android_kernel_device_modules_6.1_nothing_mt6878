// SPDX-License-Identifier: GPL-2.0
//
// Copyright (c) 2022 MediaTek Inc.

#include "mtk_cam-job_state_impl.h"

static struct state_transition STATE_TRANS(basic_sensor, S_SENSOR_NOT_SET)[] = {
	{
		S_SENSOR_APPLYING, CAMSYS_EVENT_ENQUE,
		guard_apply_sensor, ACTION_APPLY_SENSOR
	},
	{
		S_SENSOR_APPLYING, CAMSYS_EVENT_IRQ_L_CQ_DONE,
		guard_apply_sensor, ACTION_APPLY_SENSOR
	},
	{
		S_SENSOR_APPLYING, CAMSYS_EVENT_IRQ_L_SOF,
		guard_apply_sensor, ACTION_APPLY_SENSOR
	},
};

static struct state_transition STATE_TRANS(basic, S_ISP_COMPOSING)[] = {
	{
		S_ISP_APPLYING, CAMSYS_EVENT_ACK,
		guard_ack_apply_directly, ACTION_APPLY_ISP
	},
	{
		S_ISP_COMPOSED, CAMSYS_EVENT_ACK,
		guard_ack_eq, 0
	},
};

static struct state_transition STATE_TRANS(basic, S_ISP_COMPOSED)[] = {
	{
		S_ISP_APPLYING, CAMSYS_EVENT_IRQ_L_SOF,
		guard_apply_isp, ACTION_APPLY_ISP,
	},
};

static struct state_transition STATE_TRANS(basic, S_ISP_APPLYING)[] = {
#ifdef DO_WE_NEED_THIS /* is it possible to miss cq_done? */
	{
		S_ISP_PROCESSING, CAMSYS_EVENT_IRQ_L_SOF,
		guard_inner_eq, 0
	},
#endif
	{
		S_ISP_OUTER, CAMSYS_EVENT_IRQ_L_CQ_DONE,
		guard_outer_eq, 0
	},
};

static struct state_transition STATE_TRANS(basic, S_ISP_OUTER)[] = {
	{
		S_ISP_PROCESSING, CAMSYS_EVENT_IRQ_L_SOF,
		guard_inner_eq, 0
	},
};

static struct state_transition STATE_TRANS(basic, S_ISP_PROCESSING)[] = {
	{
		S_ISP_DONE, CAMSYS_EVENT_IRQ_FRAME_DONE,
		guard_inner_eq, ACTION_BUFFER_DONE
	},
	{ /* note: should handle frame_done first if sof/p1done come together */
		S_ISP_SENSOR_MISMATCHED, CAMSYS_EVENT_IRQ_L_SOF,
		guard_hw_retry_mismatched, 0
	},
	{
		S_ISP_PROCESSING, CAMSYS_EVENT_IRQ_L_SOF,
		guard_hw_retry_matched, 0
	},
#ifdef TO_REMOVE
	{
		S_ISP_DONE, CAMSYS_EVENT_IRQ_SOF,
		guard_inner_ge, ACTION_BUFFER_DONE
	},
#endif
};

static struct state_transition STATE_TRANS(basic, S_ISP_SENSOR_MISMATCHED)[] = {
	{
		S_ISP_DONE_MISMATCHED, CAMSYS_EVENT_IRQ_FRAME_DONE,
		guard_inner_eq, ACTION_BUFFER_DONE
	},
#ifdef TO_REMOVE
	{
		S_ISP_DONE_MISMATCHED, CAMSYS_EVENT_IRQ_SOF,
		guard_inner_ge, ACTION_BUFFER_DONE
	},
#endif
};

static struct transitions_entry basic_sensor_entries[NR_S_SENSOR_STATE] = {
	ADD_TRANS_ENTRY(basic_sensor, S_SENSOR_NOT_SET),
};
DECL_STATE_TABLE(basic_sensor_tbl, basic_sensor_entries);

static struct transitions_entry basic_isp_entries[NR_S_ISP_STATE] = {
	ADD_TRANS_ENTRY(basic, S_ISP_COMPOSING),
	ADD_TRANS_ENTRY(basic, S_ISP_COMPOSED),
	ADD_TRANS_ENTRY(basic, S_ISP_APPLYING),
	ADD_TRANS_ENTRY(basic, S_ISP_OUTER),
	ADD_TRANS_ENTRY(basic, S_ISP_PROCESSING),
	ADD_TRANS_ENTRY(basic, S_ISP_SENSOR_MISMATCHED),
	//ADD_TRANS_ENTRY(basic, S_ISP_DONE),
	//ADD_TRANS_ENTRY(basic, S_ISP_DONE_MISMATCHED),
};
DECL_STATE_TABLE(basic_isp_tbl, basic_isp_entries);

static int basic_send_event(struct mtk_cam_job_state *s,
			    struct transition_param *p)
{
	int ret;

	ret = loop_each_transition(&basic_sensor_tbl, s, SENSOR_STATE, p);

	ret = ret || loop_each_transition(&basic_isp_tbl, s, ISP_STATE, p);

	return ret < 0 ? -1 : 0;
}

static struct mtk_cam_job_state_ops basic_state_ops = {
	.send_event = basic_send_event,
	//.is_sensor_updated
	//.is_next_sensor_applicable
	//.is_next_isp_applicable
};

int mtk_cam_job_state_init_basic(struct mtk_cam_job_state *s,
				 const struct mtk_cam_job_state_cb *cb,
				 int with_sensor_ctrl)
{
	s->ops = &basic_state_ops;

	mtk_cam_job_state_set(s, SENSOR_STATE,
			      with_sensor_ctrl ?
			      S_SENSOR_NOT_SET : S_SENSOR_NONE);
	mtk_cam_job_state_set(s, ISP_STATE, S_ISP_NOT_SET);

	s->cb = cb;
	return 0;
}

