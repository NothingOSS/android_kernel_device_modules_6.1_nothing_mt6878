/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2022 MediaTek Inc.
 */

#ifndef __MTK_CAM_JOB_STATE_IMPL_GUARD_H
#define __MTK_CAM_JOB_STATE_IMPL_GUARD_H

static inline struct mtk_cam_job_state *prev_state(struct mtk_cam_job_state *s)
{
	return list_prev_entry(s, list);
}

static inline int guard_ack_eq(struct mtk_cam_job_state *s,
			       struct transition_param *p)
{
	return p->info->ack_seq_no == s->seq_no;
}

static inline int prev_isp_state_ge(struct mtk_cam_job_state *s,
				    struct list_head *list_head,
				    int state)
{
	/* TODO(AY): use func to access prev state */
	return list_is_first(&s->list, list_head)
		|| (mtk_cam_job_state_get(prev_state(s), ISP_STATE) >= state);
}

static inline bool allow_applying_hw(struct transition_param *p)
{
	return p->info->apply_hw_by_statemachine;
}

static inline int guard_apply_sensor(struct mtk_cam_job_state *s,
			      struct transition_param *p)
{
	/* TODO: add ts check */
	return allow_applying_hw(p) &&
		prev_isp_state_ge(s, p->head, S_ISP_OUTER);
}

static inline int guard_apply_isp(struct mtk_cam_job_state *s,
				  struct transition_param *p)
{
	return allow_applying_hw(p) &&
		prev_isp_state_ge(s, p->head, S_ISP_PROCESSING) &&
		mtk_cam_job_state_get(s, ISP_STATE) >= S_SENSOR_APPLYING;
}

static inline int guard_apply_m2m(struct mtk_cam_job_state *s,
				  struct transition_param *p)
{
	return allow_applying_hw(p) &&
		prev_isp_state_ge(s, p->head, S_ISP_DONE);
}

static inline int guard_ack_apply_directly(struct mtk_cam_job_state *s,
					   struct transition_param *p)
{
	/* TODO: check timer? */
	return guard_ack_eq(s, p) && guard_apply_isp(s, p);
}

static inline int guard_ack_apply_m2m_directly(struct mtk_cam_job_state *s,
					       struct transition_param *p)
{
	return guard_ack_eq(s, p) && guard_apply_m2m(s, p);
}

static inline int guard_outer_eq(struct mtk_cam_job_state *s,
				 struct transition_param *p)
{
	return p->info->outer_seq_no == s->seq_no;
}

static inline int guard_inner_eq(struct mtk_cam_job_state *s,
				 struct transition_param *p)
{
	return p->info->inner_seq_no == s->seq_no;
}

static inline int guard_inner_ge(struct mtk_cam_job_state *s,
				 struct transition_param *p)
{
	return p->info->inner_seq_no >= s->seq_no;
}

static inline int guard_hw_retry_mismatched(struct mtk_cam_job_state *s,
					    struct transition_param *p)
{
	/* TODO(AY): use func to access prev state */
	return !list_is_last(&s->list, p->head)
		&& (mtk_cam_job_state_get(list_next_entry(s, list), SENSOR_STATE)
		    >= S_SENSOR_APPLYING);
}


#endif //__MTK_CAM_JOB_STATE_IMPL_GUARD_H
