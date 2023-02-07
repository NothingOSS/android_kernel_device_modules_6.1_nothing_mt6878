// SPDX-License-Identifier: GPL-2.0
//
// Copyright (c) 2022 MediaTek Inc.

#include <linux/bug.h>
#include <linux/list.h>

//#define TEST_MODULE
#ifdef TEST_MODULE
#include <linux/module.h>
#endif

#ifdef TEST_MODULE
enum mtk_cam_sensor_state {
	S_SENSOR_NOT_SET,
	S_SENSOR_APPLYING,
	S_SENSOR_DONE,
};

enum mtk_cam_isp_state {
	S_ISP_NOT_SET,
	S_ISP_COMPOSING = S_ISP_NOT_SET,
	S_ISP_COMPOSED,
	S_ISP_APPLYING,
	S_ISP_OUTER,
	S_ISP_PROCESSING,
	S_ISP_SENSOR_MISMATCHED,
	S_ISP_DONE,
	S_ISP_DONE_MISMATCHED,
	NR_S_ISP_STATE,
};

enum mtk_cam_job_action {
	ACTION_APPLY_SENSOR = 1,
	ACTION_APPLY_ISP = 2,
	//ACTION_VSYNC_EVENT,
	ACTION_BUFFER_DONE = 4,
};

enum mtk_camsys_event_type {

	CAMSYS_EVENT_IRQ_SETTING_DONE,
	CAMSYS_EVENT_IRQ_SOF,
	CAMSYS_EVENT_IRQ_AFO_DONE,
	CAMSYS_EVENT_IRQ_FRAME_DONE,

	CAMSYS_EVENT_TIMER_SENSOR,

	CAMSYS_EVENT_ENQUE,
	CAMSYS_EVENT_ACK,
};

struct mtk_cam_ctrl_runtime_info {

	int ack_seq_no;
	int outer_seq_no;
	int inner_seq_no;

	u64 sof_ts_ns;
	u64 sof_hw_ts;

	/* do we need these? */
	/* already applied in current frame */
	bool sensor_is_applied;
	bool isp_is_applied;
};

struct mtk_cam_job_state {
	struct list_head list;

	int seq_no;

	/*  states */
	int state_sensor;
	int state_isp;

	int todo_action;

	//int (*send_event)(struct mtk_cam_job_state *s,
	//		  struct transition_param *p);

	int (*apply_sensor)(struct mtk_cam_job_state *s);
	int (*apply_isp)(struct mtk_cam_job_state *s);
	int (*handle_buffer_done)(struct mtk_cam_job_state *s);
};

struct mtk_cam_ctrl {
	spinlock_t state_lock;
	struct list_head state_list;

	struct mtk_cam_ctrl_runtime_info info;
};

int mtk_cam_ctrl_send_event(struct mtk_cam_ctrl *ctrl, int event);

struct transition_param {
	struct list_head *head;
	struct mtk_cam_ctrl_runtime_info *info;
	int event;
};

struct state_transition {
	int dst_state;
	int on_event;
	int (*guard)(struct mtk_cam_job_state *s, struct transition_param *p);
	int action;
};

struct transitions_entry {
	struct state_transition *trans;
	int			size;
};

static const char *str_sensor_state(int state)
{
	static const char *str[] = {
		[S_SENSOR_NOT_SET] = "not-set",
		[S_SENSOR_APPLYING] = "applying",
		[S_SENSOR_DONE] = "done",
	};

	return state >= 0 && state < ARRAY_SIZE(str) ? str[state] : "(null)";
}

static const char *str_isp_state(int state)
{
	static const char *str[] = {
		//[S_ISP_NOT_SET] = "not-set",
		[S_ISP_COMPOSING] = "composing",
		[S_ISP_COMPOSED] = "composed",
		[S_ISP_APPLYING] = "applying",
		[S_ISP_OUTER] = "outer",
		[S_ISP_PROCESSING] = "processing",
		[S_ISP_SENSOR_MISMATCHED] = "s_mismatched",
		[S_ISP_DONE] = "done",
		[S_ISP_DONE_MISMATCHED] = "done-mismatched",
	};

	return state >= 0 && state < ARRAY_SIZE(str) ? str[state] : "(null)";
}

static void dump_runtime_info(struct mtk_cam_ctrl_runtime_info *info)
{
	pr_info("runtime: ack %d out/in %d/%d\n",
		info->ack_seq_no, info->outer_seq_no, info->inner_seq_no);
}

static inline void transit_sensor_state(struct mtk_cam_job_state *s,
					int new_state, int act)
{
	pr_info("%s: #%d %s -> %s\n", __func__, s->seq_no,
		str_sensor_state(s->state_sensor),
		str_sensor_state(new_state));

	s->state_sensor = new_state;
	s->todo_action |= act;
}

static inline void transit_isp_state(struct mtk_cam_job_state *s,
				     int new_state, int act)
{
	pr_info("%s: #%d %s -> %s\n", __func__, s->seq_no,
		str_isp_state(s->state_isp),
		str_isp_state(new_state));

	s->state_isp = new_state;
	s->todo_action |= act;
}

static inline
struct mtk_cam_job_state *prev_state(struct mtk_cam_job_state *s)
{
	return list_prev_entry(s, list);
}

static int guard_ack_eq(struct mtk_cam_job_state *s,
			struct transition_param *p)
{
	return p->info->ack_seq_no == s->seq_no;
}

static int prev_isp_state_ge(struct mtk_cam_job_state *s,
			     struct list_head *list_head,
			     int state)
{
	return list_is_first(&s->list, list_head)
		|| (prev_state(s)->state_isp >= state);
}

static int prev_sensor_state_ge(struct mtk_cam_job_state *s,
				struct list_head *list_head,
				int state)
{
	return list_is_first(&s->list, list_head)
		|| (prev_state(s)->state_sensor >= state);
}

static int guard_apply_isp(struct mtk_cam_job_state *s,
			   struct transition_param *p)
{
	return prev_sensor_state_ge(s, p->head, S_SENSOR_APPLYING) &&
		prev_isp_state_ge(s, p->head, S_ISP_PROCESSING);
}

static int guard_ack_apply_directly(struct mtk_cam_job_state *s,
				    struct transition_param *p)
{
	/* TODO: check timer? */
	return guard_ack_eq(s, p) && guard_apply_isp(s, p);
}

static int guard_outer_eq(struct mtk_cam_job_state *s,
			       struct transition_param *p)
{
	return p->info->outer_seq_no == s->seq_no;
}

static int guard_inner_eq(struct mtk_cam_job_state *s,
			  struct transition_param *p)
{
	return p->info->inner_seq_no == s->seq_no;
}

static int guard_inner_ge(struct mtk_cam_job_state *s,
			  struct transition_param *p)
{
	return p->info->inner_seq_no >= s->seq_no;
}

static int guard_hw_retry_mismatched(struct mtk_cam_job_state *s,
				     struct transition_param *p)
{
	return !list_is_last(&s->list, p->head)
		&& list_next_entry(s, list)->state_sensor >= S_SENSOR_APPLYING;
}

#define STATE_TRANS(prefix, state)	prefix ## _ ##state

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
		S_ISP_APPLYING, CAMSYS_EVENT_IRQ_SOF,
		guard_apply_isp, ACTION_APPLY_ISP,
	},
};

static struct state_transition STATE_TRANS(basic, S_ISP_APPLYING)[] = {
	{
		S_ISP_OUTER, CAMSYS_EVENT_IRQ_SETTING_DONE,
		guard_outer_eq, 0
	},
	{
		S_ISP_PROCESSING, CAMSYS_EVENT_IRQ_SOF,
		guard_inner_eq, 0
	},
};

static struct state_transition STATE_TRANS(basic, S_ISP_OUTER)[] = {
	{
		S_ISP_PROCESSING, CAMSYS_EVENT_IRQ_SOF,
		guard_inner_eq, 0
	},
};

static struct state_transition STATE_TRANS(basic, S_ISP_PROCESSING)[] = {
	{
		S_ISP_DONE, CAMSYS_EVENT_IRQ_FRAME_DONE,
		guard_inner_eq, ACTION_BUFFER_DONE
	},
	{ /* note: should handle frame_done first if sof/p1done come together */
		S_ISP_SENSOR_MISMATCHED, CAMSYS_EVENT_IRQ_SOF,
		guard_hw_retry_mismatched, 0
	},
	{
		S_ISP_DONE, CAMSYS_EVENT_IRQ_SOF,
		guard_inner_ge, ACTION_BUFFER_DONE
	},
};

static struct state_transition STATE_TRANS(basic, S_ISP_SENSOR_MISMATCHED)[] = {
	{
		S_ISP_DONE_MISMATCHED, CAMSYS_EVENT_IRQ_FRAME_DONE,
		guard_inner_eq, ACTION_BUFFER_DONE
	},
	{
		S_ISP_DONE_MISMATCHED, CAMSYS_EVENT_IRQ_SOF,
		guard_inner_ge, ACTION_BUFFER_DONE
	},
};

#define _ADD_TRANS_ENTRY(s, name)	\
	[s] = {name, ARRAY_SIZE(name)}
#define ADD_TRANS_ENTRY(prefix, state)	\
	_ADD_TRANS_ENTRY(state, STATE_TRANS(prefix, state))

static struct transitions_entry basic_trans_tbl[NR_S_ISP_STATE] = {
	ADD_TRANS_ENTRY(basic, S_ISP_COMPOSING),
	ADD_TRANS_ENTRY(basic, S_ISP_COMPOSED),
	ADD_TRANS_ENTRY(basic, S_ISP_APPLYING),
	ADD_TRANS_ENTRY(basic, S_ISP_OUTER),
	ADD_TRANS_ENTRY(basic, S_ISP_PROCESSING),
	ADD_TRANS_ENTRY(basic, S_ISP_SENSOR_MISMATCHED),
	//ADD_TRANS_ENTRY(basic, S_ISP_DONE),
	//ADD_TRANS_ENTRY(basic, S_ISP_DONE_MISMATCHED),
};

static int loop_each_isp_transition(struct transitions_entry *tbl, int tbl_size,
				    struct mtk_cam_job_state *s,
				    struct transition_param *p)
{
	struct state_transition *trans;
	int trans_size;
	int state, event;
	int ret = 0;

	event = p->event;
	state = s->state_isp;

	if (WARN_ON(state >= tbl_size))
		return -1;

	trans = tbl[state].trans;
	trans_size = tbl[state].size;

	for (; trans_size; ++trans, --trans_size) {
		if (trans->on_event != event)
			continue;

		//pr_info("...#%d isp state %s, trying for event %d\n",
		//	s->seq_no, str_isp_state(state), event);
		ret = trans->guard ? trans->guard(s, p) : 1;
		if (ret > 0) {
			transit_isp_state(s, trans->dst_state, trans->action);
			break;
		}
	}

	return ret;
}

int mtk_cam_job_state_sensor_send(struct mtk_cam_job_state *s,
				  struct transition_param *p)
{
	//struct mtk_cam_ctrl_runtime_info *info = p->info;
	int current_state = s->state_sensor;
	int event = p->event;

	//if (info->sensor_is_applied)
	//	return 0;

	if (current_state != S_SENSOR_NOT_SET)
		return 0;

	switch (event) {
	case CAMSYS_EVENT_TIMER_SENSOR:
	case CAMSYS_EVENT_ENQUE:
		{
			int do_apply =
				prev_isp_state_ge(s, p->head, S_ISP_OUTER);

			if (do_apply) {
				transit_sensor_state(s, S_SENSOR_APPLYING,
						     ACTION_APPLY_SENSOR);

				//info->sensor_is_applied = 1;
			}
		}
		break;
	default:
		break;
	};

	return 0;
}

int mtk_cam_job_state_send_event(struct mtk_cam_job_state *s,
				 struct transition_param *p)
{
	int ret;

	ret = mtk_cam_job_state_sensor_send(s, p);
	if (ret)
		return -1;

	ret = loop_each_isp_transition(basic_trans_tbl,
				       ARRAY_SIZE(basic_trans_tbl),
				       s, p);
	return ret < 0 ? -1 : 0;
}

#define CALL_ACTION(state, func, ...) \
({\
	typeof(state) _s  = (state);\
	_s->func ? _s->func(_s, ##__VA_ARGS__) : -1;\
})

static int apply_action(struct mtk_cam_job_state *s)
{
	int action, ret = 0;

	action = s->todo_action;
	s->todo_action = 0;

	if (action & ACTION_APPLY_SENSOR)
		ret = ret || CALL_ACTION(s, apply_sensor);
	if (action & ACTION_APPLY_ISP)
		ret = ret || CALL_ACTION(s, apply_isp);
	if (action & ACTION_BUFFER_DONE)
		ret = ret || CALL_ACTION(s, handle_buffer_done);

	return ret;
}

int mtk_cam_ctrl_apply(struct mtk_cam_ctrl *ctrl)
{
	struct mtk_cam_job_state *state;

	/* todo: remove lock */
	//spin_lock(&ctrl->state_lock);
	list_for_each_entry(state, &ctrl->state_list, list) {
		apply_action(state);
	}
	//spin_unlock(&ctrl->state_lock);

	return 0;
}

int mtk_cam_ctrl_send_event(struct mtk_cam_ctrl *ctrl, int event)
{
	struct mtk_cam_job_state *state;
	struct transition_param p;

	p.head = &ctrl->state_list;
	p.info = &ctrl->info;
	p.event = event;

	if (0)
		dump_runtime_info(p.info);

	//spin_lock(&ctrl->state_lock);
	list_for_each_entry(state, &ctrl->state_list, list) {
		mtk_cam_job_state_send_event(state, &p);
	}
	//spin_unlock(&ctrl->state_lock);

	return 0;
}

static struct mtk_cam_job_state jobs[10];

static struct mtk_state_ctrl_test {
	struct list_head state_list;

	struct mtk_cam_ctrl_runtime_info info;
} test_ctrl;

static int mtk_state_test_apply_sensor(struct mtk_cam_job_state *s)
{
	pr_info("#%d applying sensor\n", s->seq_no);
	return 0;
}

static int mtk_state_test_apply_isp(struct mtk_cam_job_state *s)
{
	pr_info("#%d applying isp\n", s->seq_no);
	return 0;
}

static int mtk_state_test_buffer_done(struct mtk_cam_job_state *s)
{
	pr_info("#%d buffer_done\n", s->seq_no);
	return 0;
}

static int mtk_state_test_init(void)
{
	int i;

	pr_info("%s\n", __func__);

	memset(&test_ctrl, 0, sizeof(test_ctrl));
	INIT_LIST_HEAD(&test_ctrl.state_list);

	memset(jobs, 0, sizeof(jobs));
	for (i = 0; i < ARRAY_SIZE(jobs); i++) {
		jobs[i].seq_no = i + 1;

		jobs[i].apply_sensor = mtk_state_test_apply_sensor;
		jobs[i].apply_isp = mtk_state_test_apply_isp;
		jobs[i].handle_buffer_done = mtk_state_test_buffer_done;

		list_add_tail(&jobs[i].list, &test_ctrl.state_list);
	}

	return 0;
}

static int mtk_state_test_apply(struct mtk_state_ctrl_test *ctrl)
{
	struct mtk_cam_job_state *state;

	list_for_each_entry(state, &ctrl->state_list, list) {
		apply_action(state);
	}

	return 0;
}

static int mtk_state_test_send_event(struct mtk_state_ctrl_test *ctrl, int event)
{
	struct mtk_cam_job_state *state;
	struct transition_param p;

	p.head = &ctrl->state_list;
	p.info = &ctrl->info;
	p.event = event;

	dump_runtime_info(p.info);

	list_for_each_entry(state, &ctrl->state_list, list) {
		mtk_cam_job_state_send_event(state, &p);
	}

	return 0;
}

static int mtk_state_test_run(void)
{
	struct mtk_state_ctrl_test *ctrl = &test_ctrl;
	int loop = ARRAY_SIZE(jobs) + 1;
	int cq_seq = 0;

	pr_info("%s\n", __func__);
	transit_sensor_state(&jobs[0], S_SENSOR_APPLYING, ACTION_APPLY_SENSOR);
	mtk_state_test_apply(ctrl);

	transit_isp_state(&jobs[0], S_ISP_APPLYING, ACTION_APPLY_ISP);
	mtk_state_test_apply(ctrl);

	ctrl->info.outer_seq_no = ++cq_seq;
	mtk_state_test_send_event(ctrl, CAMSYS_EVENT_IRQ_SETTING_DONE);
	mtk_state_test_apply(ctrl);

	// toggle db
	// enable vf

	transit_sensor_state(&jobs[1], S_SENSOR_APPLYING, ACTION_APPLY_SENSOR);
	mtk_state_test_apply(ctrl);

	while (loop--) {

		dump_runtime_info(&ctrl->info);

		++cq_seq;
		pr_info("=== ack %d\n", cq_seq);
		ctrl->info.ack_seq_no = cq_seq;
		mtk_state_test_send_event(ctrl, CAMSYS_EVENT_ACK);
		mtk_state_test_apply(ctrl);

		// sof
		pr_info("=== sof\n");
		ctrl->info.inner_seq_no = ctrl->info.outer_seq_no;
		ctrl->info.inner_seq_no = ctrl->info.outer_seq_no;
		mtk_state_test_send_event(ctrl, CAMSYS_EVENT_IRQ_SOF);
		mtk_state_test_apply(ctrl);

		pr_info("    cq_done\n");
		ctrl->info.outer_seq_no = ctrl->info.ack_seq_no;
		mtk_state_test_send_event(ctrl, CAMSYS_EVENT_IRQ_SETTING_DONE);
		mtk_state_test_apply(ctrl);

		pr_info("    timer callback\n");
		mtk_state_test_send_event(ctrl, CAMSYS_EVENT_TIMER_SENSOR);
		mtk_state_test_apply(ctrl);

		pr_info("    frame done\n");
		mtk_state_test_send_event(ctrl, CAMSYS_EVENT_IRQ_FRAME_DONE);
		mtk_state_test_apply(ctrl);
	}

	return 0;
}

static int __init mtk_state_init(void)
{
	//int ret;
	pr_info("%s\n", __func__);

	mtk_state_test_init();
	mtk_state_test_run();

	return 0;
}

static void __exit mtk_state_exit(void)
{
	pr_info("%s\n", __func__);
}

module_init(mtk_state_init);
module_exit(mtk_state_exit);

MODULE_DESCRIPTION("camera state test module");
MODULE_LICENSE("GPL");
#endif

