// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/rbtree.h>
#include <linux/preempt.h>
#include <linux/proc_fs.h>
#include <linux/vmalloc.h>
#include <linux/module.h>
#include <linux/sched/clock.h>
#include <linux/sched.h>
#include <linux/cpumask.h>
#include <linux/kallsyms.h>
#include <linux/tracepoint.h>
#include <linux/sched/task.h>
#include <linux/kernel.h>
#include <linux/sched/rt.h>
#include <linux/sched/deadline.h>
#include <trace/trace.h>
#include <trace/events/sched.h>
#include <trace/events/irq.h>
#include <trace/events/timer.h>
#include <mt-plat/fpsgo_common.h>

#include "xgf.h"
#include "fpsgo_base.h"
#include "fpsgo_sysfs.h"
#include "fpsgo_usedext.h"
#include "fstb.h"

static DEFINE_MUTEX(xgf_main_lock);
static DEFINE_MUTEX(xgff_frames_lock);
static DEFINE_MUTEX(xgf_policy_cmd_lock);
static atomic_t xgf_ko_enable;
static atomic_t xgf_event_buffer_idx;
static atomic_t fstb_event_buffer_idx;
static int xgf_enable;
int xgf_trace_enable;
static int xgf_log_trace_enable;
static int xgf_ko_ready;
static int xgf_nr_cpus __read_mostly;
static struct kobject *xgf_kobj;
static struct rb_root xgf_policy_cmd_tree;
static unsigned long long last_update2spid_ts;
static char *xgf_sp_name = SP_ALLOW_NAME;
static int xgf_extra_sub;
static int xgf_force_no_extra_sub;
static int xgf_latest_dep_frames = XGF_DEFAULT_DEP_FRAMES;
static int xgf_dep_frames = XGF_DEFAULT_DEP_FRAMES;
static int xgf_ema_dividend = XGF_DEFAULT_EMA_DIVIDEND;
static int xgf_spid_ck_period = NSEC_PER_SEC;
static int xgf_sp_name_id;
static int xgf_spid_list_length;
static int xgf_wspid_list_length;
static int xgf_cfg_spid;
static int xgf_ema2_enable;
static int is_xgff_mips_exp_enable;
static int total_xgf_policy_cmd_num;
static int xgf_max_dep_path_num = DEFAULT_MAX_DEP_PATH_NUM;
static int xgf_max_dep_task_num = DEFAULT_MAX_DEP_TASK_NUM;
static int cam_hal_pid;
static int cam_server_pid;

struct fpsgo_trace_event *xgf_event_buffer;
EXPORT_SYMBOL(xgf_event_buffer);
int xgf_event_buffer_size;
EXPORT_SYMBOL(xgf_event_buffer_size);
struct fpsgo_trace_event *fstb_event_buffer;
EXPORT_SYMBOL(fstb_event_buffer);
int fstb_event_buffer_size;
EXPORT_SYMBOL(fstb_event_buffer_size);

HLIST_HEAD(xgf_render_if_list);
HLIST_HEAD(xgff_frames);
HLIST_HEAD(xgf_spid_list);
HLIST_HEAD(xgf_wspid_list);

int (*xgf_est_runtime_fp)(
	int pid, unsigned long long bufID, int tgid, int spid,
	int max_dep_path_num, int max_dep_task_num,
	int ema2_enable, int latest_dep_frames, int ema1_dividend, int do_ex_sub,
	int *raw_dep_list, int *raw_dep_list_num, int max_raw_dep_list_num,
	int *dep_list, int *dep_list_num, int max_dep_list_num,
	unsigned long long *raw_t_cpu, unsigned long long *ema_t_cpu,
	unsigned long long *deq_t_cpu, unsigned long long *enq_t_cpu,
	unsigned long long t_dequeue_start, unsigned long long t_dequeue_end,
	unsigned long long t_enqueue_start, unsigned long long t_enqueue_end,
	unsigned long long prev_queue_end_ts, unsigned long long cur_queue_end_ts
	);
EXPORT_SYMBOL(xgf_est_runtime_fp);
void (*xgf_delete_render_info_fp)(int pid, unsigned long long bufID);
EXPORT_SYMBOL(xgf_delete_render_info_fp);

static int xgf_tracepoint_probe_register(struct tracepoint *tp,
					void *probe,
					void *data)
{
	return tracepoint_probe_register(tp, probe, data);
}

static int xgf_tracepoint_probe_unregister(struct tracepoint *tp,
					void *probe,
					void *data)
{
	return tracepoint_probe_unregister(tp, probe, data);
}

void xgf_trace(const char *fmt, ...)
{
	char log[256];
	va_list args;
	int len;

	if (!xgf_trace_enable)
		return;

	va_start(args, fmt);
	len = vsnprintf(log, sizeof(log), fmt, args);
	if (unlikely(len == 256))
		log[255] = '\0';
	va_end(args);

	trace_printk(log);
}
EXPORT_SYMBOL(xgf_trace);

int xgf_num_possible_cpus(void)
{
	return num_possible_cpus();
}

int xgf_get_task_wake_cpu(struct task_struct *t)
{
	return t->wake_cpu;
}

int xgf_get_task_pid(struct task_struct *t)
{
	return t->pid;
}

long xgf_get_task_state(struct task_struct *t)
{
	return t->__state;
}

static inline int xgf_ko_is_ready(void)
{
	return xgf_ko_ready;
}

int xgf_atomic_read(int op)
{
	int ret = -1;

	if (op == 0)
		ret = atomic_read(&xgf_ko_enable);
	else if (op == 1)
		ret = atomic_read(&xgf_event_buffer_idx);
	else if (op == 2)
		ret = atomic_read(&fstb_event_buffer_idx);

	return ret;
}
EXPORT_SYMBOL(xgf_atomic_read);

unsigned long long xgf_do_div(unsigned long long a, unsigned long long b)
{
	unsigned long long ret;

	do_div(a, b);
	ret = a;

	return ret;
}
EXPORT_SYMBOL(xgf_do_div);

int xgf_get_process_id(int pid)
{
	int process_id = 0;
	struct task_struct *tsk;

	rcu_read_lock();
	tsk = find_task_by_vpid(pid);
	if (tsk) {
		get_task_struct(tsk);
		process_id = tsk->tgid;
		put_task_struct(tsk);
	}
	rcu_read_unlock();

	return process_id;
}
EXPORT_SYMBOL(xgf_get_process_id);

int xgf_check_main_sf_pid(int pid, int process_id)
{
	int ret = 0;
	int tmp_process_id;
	char tmp_process_name[16];
	char tmp_thread_name[16];
	struct task_struct *gtsk, *tsk;

	tmp_process_id = xgf_get_process_id(pid);
	if (tmp_process_id < 0)
		return ret;

	rcu_read_lock();
	gtsk = find_task_by_vpid(tmp_process_id);
	if (gtsk) {
		get_task_struct(gtsk);
		strncpy(tmp_process_name, gtsk->comm, 16);
		tmp_process_name[15] = '\0';
		put_task_struct(gtsk);
	} else
		tmp_process_name[0] = '\0';
	rcu_read_unlock();

	if ((tmp_process_id == process_id) ||
		strstr(tmp_process_name, "surfaceflinger"))
		ret = 1;

	if (ret) {
		rcu_read_lock();
		tsk = find_task_by_vpid(pid);
		if (tsk) {
			get_task_struct(tsk);
			strncpy(tmp_thread_name, tsk->comm, 16);
			tmp_thread_name[15] = '\0';
			put_task_struct(tsk);
		} else
			tmp_thread_name[0] = '\0';
		rcu_read_unlock();

		if (strstr(tmp_thread_name, "RTHeartBeat") ||
			strstr(tmp_thread_name, "mali-"))
			ret = 0;
	}

	return ret;
}
EXPORT_SYMBOL(xgf_check_main_sf_pid);

int xgf_check_specific_pid(int pid)
{
	int ret = 0;
	char thread_name[16];
	struct task_struct *tsk;

	rcu_read_lock();
	tsk = find_task_by_vpid(pid);
	if (tsk) {
		get_task_struct(tsk);
		strncpy(thread_name, tsk->comm, 16);
		thread_name[15] = '\0';
		put_task_struct(tsk);
	} else
		thread_name[0] = '\0';
	rcu_read_unlock();

	if (strstr(thread_name, SP_ALLOW_NAME))
		ret = 1;

	return ret;
}
EXPORT_SYMBOL(xgf_check_specific_pid);

static inline int xgf_is_enable(void)
{
	return xgf_enable;
}

unsigned long long xgf_calculate_sqrt(unsigned long long x)
{
	unsigned long long b, m, y = 0;

	if (x <= 1)
		return x;

	m = 1ULL << (__fls(x) & ~1ULL);
	while (m != 0) {
		b = y + m;
		y >>= 1;

		if (x >= b) {
			x -= b;
			y += m;
		}
		m >>= 2;
	}

	return y;
}
EXPORT_SYMBOL(xgf_calculate_sqrt);

void *xgf_alloc(int size)
{
	void *pvBuf = NULL;

	if (size <= PAGE_SIZE)
		pvBuf = kzalloc(size, GFP_ATOMIC);
	else
		pvBuf = vzalloc(size);

	return pvBuf;
}
EXPORT_SYMBOL(xgf_alloc);

void *xgf_alloc_array(int num, int size)
{
	void *pvBuf = NULL;

	pvBuf = kcalloc(num, size, GFP_KERNEL);

	return pvBuf;
}
EXPORT_SYMBOL(xgf_alloc_array);

void xgf_free(void *pvBuf)
{
	kvfree(pvBuf);
}
EXPORT_SYMBOL(xgf_free);

static void xgf_delete_policy_cmd(struct xgf_policy_cmd *iter)
{
	unsigned long long min_ts = ULLONG_MAX;
	struct xgf_policy_cmd *tmp_iter = NULL, *min_iter = NULL;
	struct rb_node *rbn = NULL;

	if (iter) {
		min_iter = iter;
		goto delete;
	}

	if (RB_EMPTY_ROOT(&xgf_policy_cmd_tree))
		return;

	rbn = rb_first(&xgf_policy_cmd_tree);
	while (rbn) {
		tmp_iter = rb_entry(rbn, struct xgf_policy_cmd, rb_node);
		if (tmp_iter->ts < min_ts) {
			min_ts = tmp_iter->ts;
			min_iter = tmp_iter;
		}
		rbn = rb_next(rbn);
	}

	if (!min_iter)
		return;

delete:
	rb_erase(&min_iter->rb_node, &xgf_policy_cmd_tree);
	xgf_free(min_iter);
	total_xgf_policy_cmd_num--;
}

static struct xgf_policy_cmd *xgf_get_policy_cmd(int tgid, int ema2_enable,
	unsigned long long ts, int force)
{
	struct rb_node **p = &xgf_policy_cmd_tree.rb_node;
	struct rb_node *parent = NULL;
	struct xgf_policy_cmd *iter = NULL;

	while (*p) {
		parent = *p;
		iter = rb_entry(parent, struct xgf_policy_cmd, rb_node);

		if (tgid < iter->tgid)
			p = &(*p)->rb_left;
		else if (tgid > iter->tgid)
			p = &(*p)->rb_right;
		else
			return iter;
	}

	if (!force)
		return NULL;

	iter = xgf_alloc(sizeof(struct xgf_policy_cmd));
	if (!iter)
		return NULL;

	iter->tgid = tgid;
	iter->ema2_enable = ema2_enable;
	iter->ts = ts;

	rb_link_node(&iter->rb_node, parent, p);
	rb_insert_color(&iter->rb_node, &xgf_policy_cmd_tree);
	total_xgf_policy_cmd_num++;

	if (total_xgf_policy_cmd_num > XGF_MAX_POLICY_CMD_NUM)
		xgf_delete_policy_cmd(NULL);

	return iter;
}

int set_xgf_spid_list(char *proc_name,
		char *thrd_name, int action)
{
	struct xgf_spid *iter, *new_xgf_spid;
	struct hlist_node *t;
	int retval = 0;

	mutex_lock(&xgf_main_lock);

	if (!strncmp("0", proc_name, 1) &&
			!strncmp("0", thrd_name, 1)) {

		hlist_for_each_entry_safe(iter, t, &xgf_spid_list, hlist) {
			hlist_del(&iter->hlist);
			xgf_free(iter);
		}

		xgf_spid_list_length = 0;
		goto out;
	}

	if (xgf_spid_list_length >= XGF_MAX_SPID_LIST_LENGTH) {
		retval = -ENOMEM;
		goto out;
	}

	new_xgf_spid = xgf_alloc(sizeof(struct xgf_spid));
	if (!new_xgf_spid) {
		retval = -ENOMEM;
		goto out;
	}

	if (!strncpy(new_xgf_spid->process_name, proc_name, 16)) {
		xgf_free(new_xgf_spid);
		retval = -ENOMEM;
		goto out;
	}
	new_xgf_spid->process_name[15] = '\0';

	if (!strncpy(new_xgf_spid->thread_name,	thrd_name, 16)) {
		xgf_free(new_xgf_spid);
		retval = -ENOMEM;
		goto out;
	}
	new_xgf_spid->thread_name[15] = '\0';

	new_xgf_spid->pid = 0;
	new_xgf_spid->rpid = 0;
	new_xgf_spid->tid = 0;
	new_xgf_spid->bufID = 0;
	new_xgf_spid->action = action;

	hlist_add_head(&new_xgf_spid->hlist, &xgf_spid_list);

	xgf_spid_list_length++;

out:
	mutex_unlock(&xgf_main_lock);
	return retval;
}

static void xgf_render_reset_wspid_list(int rpid, unsigned long long bufID)
{
	struct xgf_spid *xgf_spid_iter;
	struct hlist_node *t;

	hlist_for_each_entry_safe(xgf_spid_iter, t, &xgf_wspid_list, hlist) {
		if (xgf_spid_iter->rpid == rpid &&
			xgf_spid_iter->bufID == bufID) {
			hlist_del(&xgf_spid_iter->hlist);
			xgf_free(xgf_spid_iter);
			xgf_wspid_list_length--;
		}
	}
}

static int xgf_render_setup_wspid_list(int tgid, int rpid, unsigned long long bufID)
{
	int tlen = 0;
	int ret = 1;
	struct xgf_spid *xgf_spid_iter, *new_xgf_spid;
	struct task_struct *gtsk, *sib;

	rcu_read_lock();
	gtsk = find_task_by_vpid(tgid);
	if (gtsk) {
		get_task_struct(gtsk);
		list_for_each_entry(sib, &gtsk->thread_group, thread_group) {

			get_task_struct(sib);

			hlist_for_each_entry(xgf_spid_iter, &xgf_spid_list, hlist) {
				if (strncmp(gtsk->comm, xgf_spid_iter->process_name, 16))
					continue;

				tlen = strlen(xgf_spid_iter->thread_name);

				if (!strncmp(sib->comm, xgf_spid_iter->thread_name, tlen)) {
					new_xgf_spid = xgf_alloc(sizeof(struct xgf_spid));
					if (!new_xgf_spid) {
						ret = -ENOMEM;
						put_task_struct(sib);
						goto out;
					}

					if (!strncpy(new_xgf_spid->process_name,
							xgf_spid_iter->process_name, 16)) {
						xgf_free(new_xgf_spid);
						ret = -ENOMEM;
						put_task_struct(sib);
						goto out;
					}
					new_xgf_spid->process_name[15] = '\0';

					if (!strncpy(new_xgf_spid->thread_name,
							xgf_spid_iter->thread_name, 16)) {
						xgf_free(new_xgf_spid);
						ret = -ENOMEM;
						put_task_struct(sib);
						goto out;
					}
					new_xgf_spid->thread_name[15] = '\0';

					new_xgf_spid->pid = tgid;
					new_xgf_spid->rpid = rpid;
					new_xgf_spid->tid = sib->pid;
					new_xgf_spid->bufID = bufID;
					new_xgf_spid->action = xgf_spid_iter->action;
					hlist_add_head(&new_xgf_spid->hlist, &xgf_wspid_list);
					xgf_wspid_list_length++;
				}
			}
			put_task_struct(sib);
		}
out:
		put_task_struct(gtsk);
	}
	rcu_read_unlock();
	return ret;
}

void xgf_ema2_systrace(int pid, unsigned long long bufID,
	unsigned long long xgf_ema_mse, unsigned long long xgf_ema2_mse)
{
	fpsgo_systrace_c_fbt(pid, bufID, xgf_ema_mse, "mse_alpha");
	fpsgo_systrace_c_fbt(pid, bufID, xgf_ema2_mse, "mse_ema2");
}
EXPORT_SYMBOL(xgf_ema2_systrace);

void xgf_ema2_dump_rho(long long *rho, long long *L, int n, char *buffer)
{
	int i;

	for (i = 0; i < n; i++)
		buffer += sprintf(buffer, " %lld", rho[i]);
	buffer += sprintf(buffer, " -");
	for (i = 0; i < n; i++)
		buffer += sprintf(buffer, " %lld", L[i]);
}
EXPORT_SYMBOL(xgf_ema2_dump_rho);

void xgf_ema2_dump_info_frames(long long *L, int n, char *buffer)
{
	int i;

	for (i = 0; i < n; i++)
		buffer += sprintf(buffer, " %lld", L[i]);
}
EXPORT_SYMBOL(xgf_ema2_dump_info_frames);

static struct xgf_render_if *xgf_get_render_if(int pid, unsigned long long bufID,
	unsigned long long ts, int create)
{
	int tgid;
	struct xgf_render_if *iter = NULL;
	struct hlist_node *h = NULL;

	hlist_for_each_entry_safe(iter, h, &xgf_render_if_list, hlist) {
		if (iter->pid == pid && iter->bufid == bufID)
			break;
	}

	if (iter || !create)
		goto out;

	iter = xgf_alloc(sizeof(struct xgf_render_if));
	if (!iter)
		goto out;

	tgid = xgf_get_process_id(pid);

	iter->tgid = tgid;
	iter->pid = pid;
	iter->spid = 0;
	iter->bufid = bufID;
	iter->prev_queue_end_ts = ts;
	iter->cur_queue_end_ts = ts;
	iter->raw_t_cpu = 0;
	iter->ema_t_cpu = 0;
	iter->dep_list = RB_ROOT;
	iter->dep_list_size = 0;
	iter->ema2_enable = 0;

	hlist_add_head(&iter->hlist, &xgf_render_if_list);

out:
	return iter;
}

static void xgf_reset_render_dep_list(struct xgf_render_if *render)
{
	struct xgf_dep *iter;

	while (!RB_EMPTY_ROOT(&render->dep_list)) {
		iter = rb_entry(render->dep_list.rb_node,
			struct xgf_dep, rb_node);
		rb_erase(&iter->rb_node, &render->dep_list);
		xgf_free(iter);
	}
	render->dep_list_size = 0;
}

static struct xgf_dep *xgf_add_dep_task(int tid, struct xgf_render_if *render, int create)
{
	struct xgf_dep *iter;
	struct rb_root *r = &render->dep_list;
	struct rb_node **p = &render->dep_list.rb_node;
	struct rb_node *parent;

	while (*p) {
		parent = *p;
		iter = rb_entry(parent, struct xgf_dep, rb_node);

		if (tid < iter->tid)
			p = &(*p)->rb_left;
		else if (tid > iter->tid)
			p = &(*p)->rb_right;
		else
			return iter;
	}

	if (!create)
		return NULL;

	iter = xgf_alloc(sizeof(struct xgf_dep));
	if (!iter)
		return NULL;

	iter->tid = tid;
	iter->action = 0;

	rb_link_node(&iter->rb_node, parent, p);
	rb_insert_color(&iter->rb_node, r);
	render->dep_list_size++;

	return iter;
}

static void xgf_setup_render_dep_list(int *dep_list, int dep_list_num,
	struct xgf_render_if *render)
{
	int i;
	struct xgf_dep *iter;

	for (i = 0; i < dep_list_num; i++)
		iter = xgf_add_dep_task(dep_list[i], render, 1);
}

static void xgf_del_pid2prev_dep(struct xgf_render_if *render, int tid)
{
	struct xgf_dep *iter;

	iter = xgf_add_dep_task(tid, render, 0);
	if (iter) {
		rb_erase(&iter->rb_node, &render->dep_list);
		xgf_free(iter);
		render->dep_list_size--;
	}
}

static void xgf_add_pid2prev_dep(struct xgf_render_if *render, int tid, int action)
{
	struct xgf_dep *iter;

	iter = xgf_add_dep_task(tid, render, 1);
	if (iter)
		iter->action = action;
}

static void xgf_wspid_list_add2prev(struct xgf_render_if *render)
{
	struct xgf_spid *xgf_spid_iter;
	struct hlist_node *t;

	hlist_for_each_entry_safe(xgf_spid_iter, t, &xgf_wspid_list, hlist) {
		if (xgf_spid_iter->rpid == render->pid
			&& xgf_spid_iter->bufID == render->bufid) {
			if (xgf_spid_iter->action == -1)
				xgf_del_pid2prev_dep(render, xgf_spid_iter->tid);
			else
				xgf_add_pid2prev_dep(render, xgf_spid_iter->tid,
					xgf_spid_iter->action);
		}
	}
}

int has_xgf_dep(pid_t tid)
{
	struct xgf_dep *out_xd;
	struct xgf_render_if *render_iter;
	struct hlist_node *n;
	pid_t query_tid;
	int ret = 0;

	mutex_lock(&xgf_main_lock);

	query_tid = tid;

	hlist_for_each_entry_safe(render_iter, n, &xgf_render_if_list, hlist) {
		out_xd = xgf_add_dep_task(query_tid, render_iter, 0);
		if (out_xd) {
			ret = 1;
			break;
		}
	}

	mutex_unlock(&xgf_main_lock);
	return ret;
}

static int xgf_non_normal_dep_task(int tid)
{
	int ret = 0;
	struct task_struct *tsk = NULL;

	rcu_read_lock();

	tsk = find_task_by_vpid(tid);
	if (!tsk) {
		ret = 1;
		goto out;
	}

	get_task_struct(tsk);
	if ((tsk->flags & PF_KTHREAD) || rt_task(tsk) || dl_task(tsk))
		ret = 1;
	put_task_struct(tsk);

out:
	rcu_read_unlock();
	return ret;
}

static int xgf_filter_dep_task(int tid)
{
	int ret = 0;
	int local_tgid = 0;

	ret = xgf_non_normal_dep_task(tid);
	if (ret)
		goto out;

	local_tgid = xgf_get_process_id(tid);
	if (cam_hal_pid > 0 || cam_server_pid > 0) {
		if (local_tgid == cam_hal_pid ||
			local_tgid == cam_server_pid) {
			ret = local_tgid;
			goto out;
		}
	}

out:
	return ret;
}

int fpsgo_fbt2xgf_get_dep_list(int pid, int count,
	struct fpsgo_loading *arr, unsigned long long bufID)
{
	int index = 0;
	struct xgf_render_if *render_iter = NULL;
	struct xgf_dep *xd_iter = NULL;
	struct rb_node *rbn = NULL;

	if (pid <= 0 || count <= 0 || !arr)
		return 0;

	mutex_lock(&xgf_main_lock);

	render_iter = xgf_get_render_if(pid, bufID, 0, 0);
	if (!render_iter) {
		mutex_unlock(&xgf_main_lock);
		return index;
	}

	if (render_iter->spid > 0)
		xgf_add_pid2prev_dep(render_iter, render_iter->spid, 0);

	if (xgf_cfg_spid)
		xgf_wspid_list_add2prev(render_iter);

	for (rbn = rb_first(&render_iter->dep_list); rbn; rbn = rb_next(rbn)) {
		xd_iter = rb_entry(rbn, struct xgf_dep, rb_node);
		if (index < count && !xgf_filter_dep_task(xd_iter->tid)) {
			arr[index].pid = xd_iter->tid;
			arr[index].action = xd_iter->action;
			index++;
		}
	}

	mutex_unlock(&xgf_main_lock);

	return index;
}

int fpsgo_comp2xgf_get_dep_list_num(int pid, unsigned long long bufID)
{
	int ret = 0;
	struct xgf_render_if *render_iter = NULL;

	mutex_lock(&xgf_main_lock);

	render_iter = xgf_get_render_if(pid, bufID, 0, 0);
	if (!render_iter) {
		mutex_unlock(&xgf_main_lock);
		return 0;
	}

	ret = render_iter->dep_list_size;

	mutex_unlock(&xgf_main_lock);

	return ret;
}

int fpsgo_comp2xgf_get_dep_list(int pid, int count,
	int *arr, unsigned long long bufID)
{
	int index = 0;
	struct xgf_render_if *render_iter = NULL;
	struct xgf_dep *xd_iter = NULL;
	struct rb_node *rbn = NULL;

	if (count <= 0 || !arr)
		return 0;

	mutex_lock(&xgf_main_lock);

	render_iter = xgf_get_render_if(pid, bufID, 0, 0);
	if (!render_iter) {
		mutex_unlock(&xgf_main_lock);
		return index;
	}

	for (rbn = rb_first(&render_iter->dep_list); rbn; rbn = rb_next(rbn)) {
		xd_iter = rb_entry(rbn, struct xgf_dep, rb_node);
		if (index < count)
			arr[index] = xd_iter->tid;
		index++;
	}

	mutex_unlock(&xgf_main_lock);

	return index;
}

static int xgff_get_dep_list(int pid, int count,
	unsigned int *arr, struct xgf_render_if *render_iter)
{
	int index = 0;
	struct xgf_dep *xd_iter = NULL;
	struct rb_node *rbn = NULL;

	if (pid <= 0 || count <= 0 || !arr)
		return 0;

	for (rbn = rb_first(&render_iter->dep_list); rbn; rbn = rb_next(rbn)) {
		xd_iter = rb_entry(rbn, struct xgf_dep, rb_node);
		if (index < count) {
			arr[index] = xd_iter->tid;
			index++;
		}
	}

	return index;
}

static void xgf_enter_delete_render_info(int pid, unsigned long long bufID)
{
	if (xgf_delete_render_info_fp)
		xgf_delete_render_info_fp(pid, bufID);
}

static void xgf_reset_render(struct xgf_render_if *iter)
{
	hlist_del(&iter->hlist);
	xgf_enter_delete_render_info(iter->pid, iter->bufid);
	xgf_render_reset_wspid_list(iter->pid, iter->bufid);
	xgf_reset_render_dep_list(iter);
	xgf_free(iter);
}

static void xgff_reset_render(struct xgff_frame *iter)
{
	mutex_lock(&xgf_main_lock);
	xgf_enter_delete_render_info(iter->xgfrender.pid, iter->xgfrender.bufid);
	mutex_unlock(&xgf_main_lock);

	hlist_del(&iter->hlist);
	xgf_free(iter);
}

void xgf_reset_all_renders(void)
{
	struct xgf_render_if *r_iter;
	struct hlist_node *r_tmp;

	mutex_lock(&xgf_main_lock);

	hlist_for_each_entry_safe(r_iter, r_tmp, &xgf_render_if_list, hlist) {
		xgf_reset_render(r_iter);
	}

	mutex_unlock(&xgf_main_lock);
}

static void xgff_reset_all_renders(void)
{
	struct xgff_frame *r;
	struct hlist_node *h;

	mutex_lock(&xgff_frames_lock);

	hlist_for_each_entry_safe(r, h, &xgff_frames, hlist) {
		xgff_reset_render(r);
	}

	mutex_unlock(&xgff_frames_lock);
}

int fpsgo_comp2xgf_do_recycle(void)
{
	int ret = 0;
	unsigned long long now_ts = fpsgo_get_time();
	long long diff;
	struct xgf_render_if *r_iter;
	struct xgff_frame *rr_iter;
	struct hlist_node *r_t;

	mutex_lock(&xgf_main_lock);

	if (hlist_empty(&xgf_render_if_list)) {
		ret++;
		goto recycle_xgf_done;
	}

	if (xgf_latest_dep_frames != xgf_dep_frames) {
		if (xgf_dep_frames < XGF_DEP_FRAMES_MIN
			|| xgf_dep_frames > XGF_DEP_FRAMES_MAX)
			xgf_dep_frames = XGF_DEFAULT_DEP_FRAMES;
		xgf_latest_dep_frames = xgf_dep_frames;
		mutex_unlock(&xgf_main_lock);
		xgf_reset_all_renders();
		xgff_reset_all_renders();
		ret = 2;
		goto out;
	}

	hlist_for_each_entry_safe(r_iter, r_t, &xgf_render_if_list, hlist) {
		diff = now_ts - r_iter->cur_queue_end_ts;
		if (diff >= NSEC_PER_SEC)
			xgf_reset_render(r_iter);
	}

recycle_xgf_done:
	mutex_unlock(&xgf_main_lock);

	mutex_lock(&xgff_frames_lock);

	if (hlist_empty(&xgff_frames)) {
		ret++;
		goto recycle_xgff_done;
	}

	hlist_for_each_entry_safe(rr_iter, r_t, &xgff_frames, hlist) {
		diff = now_ts - rr_iter->ts;
		if (diff >= NSEC_PER_SEC)
			xgff_reset_render(rr_iter);
	}

recycle_xgff_done:
	mutex_unlock(&xgff_frames_lock);

out:
	return (ret == 2 ? 1 : 0);
}

static char *xgf_strcat(char *dest, const char *src,
	size_t buffersize, int *overflow)
{
	int i, j;
	int bufferbound = buffersize - 1;

	for (i = 0; dest[i] != '\0'; i++)
		;
	for (j = 0; src[j] != '\0'; j++) {
		if ((i+j) < bufferbound)
			dest[i+j] = src[j];

		if ((i+j) == bufferbound) {
			*overflow = 1;
			break;
		}
	}

	if (*overflow)
		dest[bufferbound] = '\0';
	else
		dest[i+j] = '\0';

	return dest;
}

static void xgf_log_trace(const char *fmt, ...)
{
	char log[1024];
	va_list args;
	int len;

	if (!xgf_log_trace_enable)
		return;

	va_start(args, fmt);
	len = vsnprintf(log, sizeof(log), fmt, args);

	if (unlikely(len == 1024))
		log[1023] = '\0';
	va_end(args);
	trace_printk(log);
}

static void xgf_print_critical_path_info(int rpid, unsigned long long bufID,
	int *raw_dep_list, int raw_dep_list_num)
{
	char total_pid_list[1024] = {"\0"};
	char pid[20] = {"\0"};
	int i, count = 0;
	int overflow = 0;
	int len = 0;

	if (!xgf_log_trace_enable)
		return;

	if (!raw_dep_list || raw_dep_list_num < 0 ||
		raw_dep_list_num > xgf_max_dep_path_num * xgf_max_dep_task_num)
		goto error;

	for (i = 0; i < raw_dep_list_num; i++) {
		if (raw_dep_list[i] == -100) {
			count++;
			if (strlen(total_pid_list) == 0)
				len = snprintf(pid, sizeof(pid), "%dth", count);
			else
				len = snprintf(pid, sizeof(pid), "-%dth", count);
			if (len < 0 || len >= sizeof(pid))
				goto error;

			overflow = 0;
			xgf_strcat(total_pid_list, pid,
				sizeof(total_pid_list), &overflow);
			if (overflow)
				goto out;

			continue;
		}

		len = snprintf(pid, sizeof(pid), ",%d", raw_dep_list[i]);
		if (len < 0 || len >= sizeof(pid))
			goto error;

		overflow = 0;
		xgf_strcat(total_pid_list, pid,
			sizeof(total_pid_list), &overflow);
		if (overflow)
			goto out;
	}

out:
	if (overflow)
		xgf_log_trace("[xgf][%d][0x%llx] | (of) %s",
		rpid, bufID, total_pid_list);
	else
		xgf_log_trace("[xgf][%d][0x%llx] | %s",
		rpid, bufID, total_pid_list);

	return;

error:
	xgf_log_trace("[xgf][%d][0x%llx] | %s error",
		rpid, bufID, __func__);
	return;
}

static int xgf_enter_est_runtime(int pid, unsigned long long bufID, int tgid, int spid,
	int max_dep_path_num, int max_dep_task_num,
	int ema2_enable, int latest_dep_frames, int ema1_dividend, int do_ex_sub,
	int *raw_dep_list, int *raw_dep_list_num, int max_raw_dep_list_num,
	int *dep_list, int *dep_list_num, int max_dep_list_num,
	unsigned long long *raw_t_cpu, unsigned long long *ema_t_cpu,
	unsigned long long *deq_t_cpu, unsigned long long *enq_t_cpu,
	unsigned long long t_dequeue_start, unsigned long long t_dequeue_end,
	unsigned long long t_enqueue_start, unsigned long long t_enqueue_end,
	unsigned long long prev_queue_end_ts, unsigned long long cur_queue_end_ts)
{
	int ret;

	if (tgid <= 0 || pid <= 0 || bufID == 0)
		goto param_err;

	if (!dep_list || !dep_list_num ||
		*dep_list_num < 0 || *dep_list_num > max_dep_list_num ||
		!raw_t_cpu || !ema_t_cpu || !deq_t_cpu || !enq_t_cpu)
		goto param_err;

	if (t_dequeue_end < t_dequeue_start ||
		t_enqueue_end < t_enqueue_start ||
		t_dequeue_end > t_enqueue_start ||
		prev_queue_end_ts > cur_queue_end_ts)
		goto param_err;

	if (xgf_est_runtime_fp)
		ret = xgf_est_runtime_fp(pid, bufID, tgid, spid,
				max_dep_path_num, max_dep_task_num,
				ema2_enable, latest_dep_frames, ema1_dividend, do_ex_sub,
				raw_dep_list, raw_dep_list_num, max_raw_dep_list_num,
				dep_list, dep_list_num, max_dep_list_num,
				raw_t_cpu, ema_t_cpu,
				deq_t_cpu, enq_t_cpu,
				t_dequeue_start, t_dequeue_end,
				t_enqueue_start, t_enqueue_end,
				prev_queue_end_ts, cur_queue_end_ts);
	else
		ret = -ENOENT;

	return ret;

param_err:
	return -EINVAL;
}

static int xgff_enter_est_runtime(struct xgf_render_if *render,
	unsigned long long *runtime, unsigned long long prev_queue_end_ts,
	unsigned long long cur_queue_end_ts)
{
	int ret = -ENOMEM;
	int *local_dep_list = NULL, *raw_dep_list = NULL;
	int local_dep_list_num = 0, raw_dep_list_num = 0;
	int max_local_dep_list_num = MAX_DEP_NUM, max_raw_dep_list_num = 0;
	unsigned long long local_raw_t_cpu = 0, local_ema_t_cpu = 0;
	unsigned long long local_deq_t_cpu = 0, local_enq_t_cpu = 0;

	local_dep_list = xgf_alloc_array(max_local_dep_list_num, sizeof(int));
	if (!local_dep_list)
		goto out;

	if (xgf_log_trace_enable) {
		max_raw_dep_list_num = xgf_max_dep_path_num * xgf_max_dep_task_num;
		raw_dep_list = xgf_alloc_array(max_raw_dep_list_num, sizeof(int));
		if (!raw_dep_list)
			goto out;
	}

	mutex_lock(&xgf_main_lock);

	ret = xgf_enter_est_runtime(render->pid, render->bufid, render->tgid, render->spid,
				xgf_max_dep_path_num, xgf_max_dep_task_num,
				render->ema2_enable, xgf_latest_dep_frames, xgf_ema_dividend, 0,
				raw_dep_list, &raw_dep_list_num, max_raw_dep_list_num,
				local_dep_list, &local_dep_list_num, max_local_dep_list_num,
				&local_raw_t_cpu, &local_ema_t_cpu,
				&local_deq_t_cpu, &local_enq_t_cpu,
				0, 0, 0, 0, prev_queue_end_ts, cur_queue_end_ts);
	fpsgo_systrace_c_fbt(render->pid, render->bufid, ret, "xgff_ret");

	mutex_unlock(&xgf_main_lock);

	xgf_reset_render_dep_list(render);
	xgf_setup_render_dep_list(local_dep_list, local_dep_list_num, render);

	*runtime = local_raw_t_cpu;

out:
	xgf_free(raw_dep_list);
	xgf_free(local_dep_list);
	return ret;
}

void xgf_get_runtime(int tid, unsigned long long *runtime)
{
	struct task_struct *p;

	if (unlikely(!tid))
		return;

	rcu_read_lock();
	p = find_task_by_vpid(tid);
	if (!p) {
		xgf_trace(" %5d not found to erase", tid);
		rcu_read_unlock();
		return;
	}
	get_task_struct(p);
	rcu_read_unlock();

	*runtime = (u64)fpsgo_task_sched_runtime(p);
	put_task_struct(p);
}
EXPORT_SYMBOL(xgf_get_runtime);

int xgf_get_logical_tid(int rpid, int tgid, int *l_tid,
	unsigned long long prev_ts, unsigned long long last_ts)
{
	int max_tid = -1;
	unsigned long long tmp_runtime, max_runtime = 0;
	struct task_struct *gtsk, *sib;

	if (last_ts - prev_ts < NSEC_PER_SEC)
		return 0;

	rcu_read_lock();
	gtsk = find_task_by_vpid(tgid);
	if (gtsk) {
		get_task_struct(gtsk);
		list_for_each_entry(sib, &gtsk->thread_group, thread_group) {
			tmp_runtime = 0;

			get_task_struct(sib);

			if (sib->pid == rpid) {
				put_task_struct(sib);
				continue;
			}

			tmp_runtime = (u64)fpsgo_task_sched_runtime(sib);
			if (tmp_runtime > max_runtime) {
				max_runtime = tmp_runtime;
				max_tid = sib->pid;
			}

			put_task_struct(sib);
		}
		put_task_struct(gtsk);
	}
	rcu_read_unlock();

	if (max_tid > 0 && max_runtime > 0)
		*l_tid = max_tid;
	else
		*l_tid = -1;

	return 1;
}
EXPORT_SYMBOL(xgf_get_logical_tid);

static int xgf_get_spid(struct xgf_render_if *render)
{
	int len, ret = 0;
	long long diff;
	unsigned long long now_ts = fpsgo_get_time();
	unsigned long long spid_runtime = 0, t_spid_runtime = 0;
	struct xgf_dep *iter;
	struct task_struct *tsk;
	struct rb_node *rbn;

	diff = (long long)now_ts - (long long)last_update2spid_ts;

	if (diff < 0LL || diff < xgf_spid_ck_period)
		return -1;

	if (xgf_cfg_spid) {
		xgf_render_reset_wspid_list(render->pid, render->bufid);
		xgf_render_setup_wspid_list(render->tgid, render->pid, render->bufid);
	}

	if (!xgf_sp_name_id)
		xgf_sp_name = SP_ALLOW_NAME;
	else
		xgf_sp_name = SP_ALLOW_NAME2;

	len = strlen(xgf_sp_name);

	if (!len)
		return 0;

	if (xgf_sp_name[len - 1] == '\n') {
		len--;
		xgf_trace("xgf_sp_name len:%d has a change line terminal", len);
	}

	for (rbn = rb_first(&render->dep_list); rbn; rbn = rb_next(rbn)) {
		iter = rb_entry(rbn, struct xgf_dep, rb_node);
		rcu_read_lock();
		tsk = find_task_by_vpid(iter->tid);
		if (tsk) {
			get_task_struct(tsk);
			if (tsk->tgid != render->tgid ||
				tsk->pid == render->pid) {
				goto tsk_out;
			}
			if (!strncmp(tsk->comm, xgf_sp_name, len)) {
				t_spid_runtime = (u64)fpsgo_task_sched_runtime(tsk);
				if (t_spid_runtime > spid_runtime) {
					spid_runtime = t_spid_runtime;
					ret = tsk->pid;
				}
			}
tsk_out:
			put_task_struct(tsk);
		}
		rcu_read_unlock();
	}

	last_update2spid_ts = now_ts;

	if (!ret && render->spid && xgf_sp_name_id && spid_runtime)
		ret = render->spid;

	return ret;
}

void fpsgo_comp2xgf_qudeq_notify(int pid, unsigned long long bufID,
	unsigned long long *run_time, unsigned long long *enq_running_time,
	unsigned long long def_start_ts, unsigned long long def_end_ts,
	unsigned long long t_dequeue_start, unsigned long long t_dequeue_end,
	unsigned long long t_enqueue_start, unsigned long long t_enqueue_end,
	int skip)
{
	int ret = 0;
	int new_spid;
	int do_extra_sub = 0;
	int *local_dep_list = NULL, *raw_dep_list = NULL;
	int local_dep_list_num = 0, raw_dep_list_num = 0;
	int max_local_dep_list_num = MAX_DEP_NUM;
	int max_raw_dep_list_num = 0;
	unsigned long long t_dequeue_time = 0;
	unsigned long long local_raw_t_cpu = 0;
	unsigned long long local_ema_t_cpu = 0;
	unsigned long long local_deq_t_cpu = 0;
	unsigned long long local_enq_t_cpu = 0;
	struct xgf_render_if *iter;
	struct xgf_policy_cmd *policy;

	if (pid <= 0 || bufID == 0 ||
		def_start_ts > def_end_ts ||
		t_dequeue_end < t_dequeue_start ||
		t_enqueue_end < t_enqueue_start ||
		t_dequeue_end > t_enqueue_start)
		return;

	mutex_lock(&xgf_main_lock);
	if (!xgf_is_enable()) {
		mutex_unlock(&xgf_main_lock);
		return;
	}

	iter = xgf_get_render_if(pid, bufID, t_enqueue_end, 1);
	if (!iter)
		return;

	iter->prev_queue_end_ts = def_start_ts > 0 ? def_start_ts : iter->cur_queue_end_ts;
	iter->cur_queue_end_ts = def_end_ts;

	if (skip)
		goto by_pass_skip;

	local_dep_list = xgf_alloc_array(max_local_dep_list_num, sizeof(int));
	if (!local_dep_list)
		goto by_pass_skip;

	if (xgf_log_trace_enable) {
		max_raw_dep_list_num = xgf_max_dep_path_num * xgf_max_dep_task_num;
		raw_dep_list = xgf_alloc_array(max_raw_dep_list_num, sizeof(int));
		if (!raw_dep_list)
			goto by_pass_skip;
	}

	mutex_lock(&xgf_policy_cmd_lock);
	policy = xgf_get_policy_cmd(iter->tgid, iter->ema2_enable, t_enqueue_end, 0);
	if (policy) {
		policy->ts = t_enqueue_end;
		iter->ema2_enable = policy->ema2_enable;
	} else
		iter->ema2_enable = xgf_ema2_enable ? 1 : 0;
	mutex_unlock(&xgf_policy_cmd_lock);

	new_spid = xgf_get_spid(iter);
	if (new_spid > 0) {
		xgf_trace("[xgf][%d][0x%llx] spid:%d => %d",
			iter->pid, iter->bufid, iter->spid, new_spid);
		iter->spid = new_spid;
	}

	do_extra_sub = xgf_extra_sub;
	t_dequeue_time = t_dequeue_end - t_dequeue_start;
	if (t_dequeue_time > 2500000 && !xgf_extra_sub && !xgf_force_no_extra_sub) {
		do_extra_sub = 1;
		xgf_trace("[xgf][%d][0x%llx] do_extra_sub deq_time:%llu",
			iter->pid, iter->bufid, t_dequeue_time);
	}

	ret = xgf_enter_est_runtime(iter->pid, iter->bufid,
				iter->tgid, iter->spid,
				xgf_max_dep_path_num, xgf_max_dep_task_num,
				iter->ema2_enable, xgf_latest_dep_frames,
				xgf_ema_dividend, do_extra_sub,
				raw_dep_list, &raw_dep_list_num, max_raw_dep_list_num,
				local_dep_list, &local_dep_list_num, max_local_dep_list_num,
				&local_raw_t_cpu, &local_ema_t_cpu,
				&local_deq_t_cpu, &local_enq_t_cpu,
				t_dequeue_start, t_dequeue_end,
				t_enqueue_start, t_enqueue_end,
				iter->prev_queue_end_ts, iter->cur_queue_end_ts);
	fpsgo_systrace_c_fbt(iter->pid, iter->bufid, ret, "xgf_ret");
	iter->raw_t_cpu = local_raw_t_cpu;
	iter->ema_t_cpu = local_ema_t_cpu;
	xgf_reset_render_dep_list(iter);
	xgf_setup_render_dep_list(local_dep_list, local_dep_list_num, iter);

	if (run_time)
		*run_time = local_ema_t_cpu;
	if (enq_running_time)
		*enq_running_time = local_enq_t_cpu > 0 ? local_enq_t_cpu : 0;

	fpsgo_systrace_c_fbt(iter->pid, iter->bufid, local_raw_t_cpu, "raw_t_cpu");
	if (iter->ema2_enable)
		fpsgo_systrace_c_fbt(iter->pid, iter->bufid, local_ema_t_cpu, "ema2_t_cpu");

	xgf_print_critical_path_info(iter->pid, iter->bufid, raw_dep_list, raw_dep_list_num);

by_pass_skip:
	xgf_free(raw_dep_list);
	xgf_free(local_dep_list);

	mutex_unlock(&xgf_main_lock);
}

int fpsgo_ktf2xgf_atomic_set(int op, int value)
{
	int ret = 0;

	switch (op) {
	case 1:
		atomic_set(&xgf_event_buffer_idx, value);
		ret = 1;
		break;

	case 2:
		atomic_set(&fstb_event_buffer_idx, value);
		ret = 1;
		break;

	default:
		break;
	}

	return ret;
}
EXPORT_SYMBOL(fpsgo_ktf2xgf_atomic_set);

int fpsgo_ktf2xgf_add_delete_render_info(int mode, int pid, unsigned long long bufID)
{
	int ret = 0;
	unsigned long long ts = fpsgo_get_time();
	struct xgf_render_if *r_iter = NULL;
	struct xgf_dep *xd_iter = NULL;

	mutex_lock(&xgf_main_lock);

	switch (mode) {
	case 0:
		r_iter = xgf_get_render_if(pid, bufID, ts, 1);
		if (r_iter) {
			xd_iter = xgf_add_dep_task(pid, r_iter, 1);
			if (xd_iter)
				ret = 1;
		}
		break;

	case 1:
		r_iter = xgf_get_render_if(pid, bufID, ts, 0);
		if (r_iter) {
			xgf_reset_render(r_iter);
			ret = 1;
		}
		break;

	default:
		break;
	}

	mutex_unlock(&xgf_main_lock);

	return ret;
}

static int xgff_find_frame(pid_t tID, unsigned long long bufID,
	unsigned long long  frameID, struct xgff_frame **ret)
{
	struct xgff_frame *iter;

	hlist_for_each_entry(iter, &xgff_frames, hlist) {
		if (iter->tid != tID)
			continue;

		if (iter->bufid != bufID)
			continue;

		if (iter->frameid != frameID)
			continue;

		if (ret)
			*ret = iter;
		return 0;
	}

	return -EINVAL;
}

static int xgff_new_frame(pid_t tID, unsigned long long bufID,
	unsigned long long frameID, struct xgff_frame **ret, int force,
	unsigned long long ts)
{
	struct xgff_frame *iter;
	struct xgf_render_if *xriter;
	struct task_struct *tsk;

	iter = xgf_alloc(sizeof(struct xgff_frame));

	if (!iter)
		return -ENOMEM;

	rcu_read_lock();
	tsk = find_task_by_vpid(tID);
	if (tsk)
		get_task_struct(tsk);
	rcu_read_unlock();

	if (!tsk) {
		xgf_free(iter);
		return -EINVAL;
	}

	// init xgff_frame
	iter->parent = tsk->tgid;
	put_task_struct(tsk);

	iter->tid = tID;
	iter->bufid = bufID;
	iter->frameid = frameID;
	iter->ts = ts;
	iter->count_dep_runtime = 0;
	iter->is_start_dep = 0;

	xriter = &iter->xgfrender;
	xriter->tgid = iter->parent;
	xriter->pid = iter->tid;
	xriter->bufid = iter->bufid;
	xriter->spid = 0;
	xriter->prev_queue_end_ts = ts;
	xriter->cur_queue_end_ts = ts;
	xriter->raw_t_cpu = 0;
	xriter->ema_t_cpu = 0;
	xriter->dep_list = RB_ROOT;
	xriter->dep_list_size = 0;
	xriter->ema2_enable = 0;

	if (ret)
		*ret = iter;
	return 0;
}

static int xgff_get_start_runtime(int rpid, unsigned long long queueid,
	unsigned int deplist_size, unsigned int *deplist,
	struct xgff_runtime *dep_runtime, int *count_dep_runtime,
	unsigned long frameid)
{
	int ret = 0;
	int i;
	unsigned long long runtime = 0;
	struct task_struct *p;

	if (!dep_runtime || !count_dep_runtime) {
		ret = -EINVAL;
		goto out;
	}

	if (!deplist && deplist_size > 0) {
		ret = -EINVAL;
		goto out;
	}

	*count_dep_runtime = 0;
	for (i = 0; i < deplist_size; i++) {
		rcu_read_lock();
		p = find_task_by_vpid(deplist[i]);
		if (!p) {
			rcu_read_unlock();
		} else {
			get_task_struct(p);
			rcu_read_unlock();
			runtime = fpsgo_task_sched_runtime(p);
			put_task_struct(p);

			dep_runtime[*count_dep_runtime].pid = deplist[i];
			dep_runtime[*count_dep_runtime].loading = runtime;
			(*count_dep_runtime)++;
			xgf_trace("[XGFF] start_dep_runtime: %llu, pid: %d", runtime, deplist[i]);
		}
	}
	xgf_trace("[XGFF][%s] ret=%d, frame_id=%lu, count_dep=%d", __func__, ret,
		frameid, *count_dep_runtime);
out:
	return ret;
}

static int _xgff_frame_start(
		unsigned int tid,
		unsigned long long queueid,
		unsigned long long frameid,
		unsigned int *pdeplistsize,
		unsigned int *pdeplist,
		unsigned long long ts)
{
	int ret = 0, is_start_dep = 0;
	struct xgff_frame *r, **rframe;

	mutex_lock(&xgf_main_lock);
	if (!xgf_is_enable()) {
		mutex_unlock(&xgf_main_lock);
		return ret;
	}
	mutex_unlock(&xgf_main_lock);

	mutex_lock(&xgff_frames_lock);
	rframe = &r;

	ret = xgff_find_frame(tid, queueid, frameid, rframe);
	if (ret == 0)
		goto qudeq_notify_err;

	ret = xgff_new_frame(tid, queueid, frameid, rframe, 1, ts);
	if (ret)
		goto qudeq_notify_err;

	r->ploading = fbt_xgff_list_loading_add(tid, queueid, ts);

	hlist_add_head(&r->hlist, &xgff_frames);

	if (!pdeplist || !pdeplistsize) {
		xgf_trace("[%s] !pdeplist || !pdeplistsize", __func__);
		is_start_dep = 0;
		r->is_start_dep = 0;
		ret = -EINVAL;
		goto qudeq_notify_err;
	}

	is_start_dep = 1;
	r->is_start_dep = 1;
	ret = xgff_get_start_runtime(tid, queueid, *pdeplistsize, pdeplist,
		r->dep_runtime, &r->count_dep_runtime, r->frameid);

qudeq_notify_err:
	xgf_trace("xgff result:%d at rpid:%d cmd:xgff_frame_start", ret, tid);
	xgf_trace("[XGFF] tid=%d, queueid=%llu, frameid=%llu, is_start_dep=%d",
		tid, queueid, frameid, is_start_dep);

	mutex_unlock(&xgff_frames_lock);

	return ret;
}

static int xgff_enter_est_runtime_dep_from_start(int rpid, unsigned long long queueid,
	struct xgff_runtime *dep_runtime, int count_dep_runtime, unsigned long long *runtime,
	unsigned long frameid)
{
	int ret = 0;
	int i, dep_runtime_size;
	unsigned long long runtime_tmp, total_runtime = 0;
	struct task_struct *p;

	if (!dep_runtime) {
		ret = -EINVAL;
		goto out;
	}

	dep_runtime_size = count_dep_runtime;

	for (i = 0; i < dep_runtime_size; i++) {
		rcu_read_lock();
		p = find_task_by_vpid(dep_runtime[i].pid);
		if (!p) {
			rcu_read_unlock();
			xgf_trace("[XGFF] Error: Can't get runtime of pid=%d at frame end.",
				dep_runtime[i].pid);
			ret = -EINVAL;
		} else {
			get_task_struct(p);
			rcu_read_unlock();
			runtime_tmp = fpsgo_task_sched_runtime(p);
			put_task_struct(p);
			if (runtime_tmp && dep_runtime[i].loading &&
				runtime_tmp >= dep_runtime[i].loading)
				dep_runtime[i].loading = runtime_tmp - dep_runtime[i].loading;
			else
				dep_runtime[i].loading = 0;

			total_runtime += dep_runtime[i].loading;
			xgf_trace("[XGFF] dep_runtime: %llu, now: %llu, pid: %d",
				dep_runtime[i].loading, runtime_tmp, dep_runtime[i].pid);
		}
	}
	*runtime = total_runtime;
out:
	xgf_trace("[XGFF][%s] ret=%d, frame_id=%lu, count_dep=%d", __func__, ret,
		frameid, count_dep_runtime);
	return ret;
}

void print_dep(unsigned int deplist_size, unsigned int *deplist)
{
	char *dep_str = NULL;
	char temp[7] = {"\0"};
	int i = 0;
	int ret;

	dep_str = kcalloc(deplist_size + 1, 7 * sizeof(char),
				GFP_KERNEL);
	if (!dep_str)
		return;

	dep_str[0] = '\0';
	for (i = 0; i < deplist_size; i++) {
		if (strlen(dep_str) == 0)
			ret = snprintf(temp, sizeof(temp), "%d", deplist[i]);
		else
			ret = snprintf(temp, sizeof(temp), ",%d", deplist[i]);

		if (ret < 0 || ret >= sizeof(temp))
			goto out;

		if (strlen(dep_str) + strlen(temp) < 256 &&
			strlen(dep_str) + strlen(temp) < (deplist_size + 1) *
			7 * sizeof(char))
			strncat(dep_str, temp, strlen(temp));
	}
out:
	xgf_trace("[XGFF] EXP_deplist: %s", dep_str);
	kfree(dep_str);
}

static int _xgff_frame_end(
		unsigned int tid,
		unsigned long long queueid,
		unsigned long long frameid,
		unsigned long long *cputime,
		unsigned int *area,
		unsigned int *pdeplistsize,
		unsigned int *pdeplist,
		unsigned long long ts)
{
	int ret = 0, ret_exp = 0;
	struct xgff_frame *r = NULL, **rframe = NULL;
	int iscancel = 0;
	unsigned long long raw_runtime = 0, raw_runtime_exp = 0;
	unsigned int newdepsize = 0, deplist_size_exp = 0;
	unsigned int deplist_exp[XGF_DEP_FRAMES_MAX] = {0};

	mutex_lock(&xgf_main_lock);
	if (!xgf_is_enable()) {
		mutex_unlock(&xgf_main_lock);
		return ret;
	}
	mutex_unlock(&xgf_main_lock);

	if (pdeplistsize && *pdeplistsize == 0)
		iscancel = 1;

	mutex_lock(&xgff_frames_lock);
	rframe = &r;
	ret = xgff_find_frame(tid, queueid, frameid, rframe);
	if (ret) {
		mutex_unlock(&xgff_frames_lock);
		goto qudeq_notify_err;
	}

	hlist_del(&r->hlist);

	mutex_unlock(&xgff_frames_lock);

	if (!iscancel) {

		*area = fbt_xgff_get_loading_by_cluster(&(r->ploading), ts, 0, 0, NULL);

		// Sum up all PELT sched_runtime in deplist to reduce MIPS.
		if (r->is_start_dep) {
			ret = xgff_enter_est_runtime_dep_from_start(tid, queueid, r->dep_runtime,
				r->count_dep_runtime, &raw_runtime, r->frameid);
			fpsgo_systrace_c_fbt(tid, queueid, raw_runtime, "xgff_runtime");
		} else
			ret = xgff_enter_est_runtime(&r->xgfrender, &raw_runtime,
					r->xgfrender.prev_queue_end_ts, ts);

		*cputime = raw_runtime;

		// post handle est time
		if (is_xgff_mips_exp_enable && r->is_start_dep) {
			deplist_size_exp = XGF_DEP_FRAMES_MAX;

			ret_exp = xgff_enter_est_runtime(&r->xgfrender, &raw_runtime_exp,
					r->xgfrender.prev_queue_end_ts, ts);

			if (ret_exp != 1) {
				deplist_size_exp = 0;
			} else {
				newdepsize = xgff_get_dep_list(tid, deplist_size_exp,
							deplist_exp, &r->xgfrender);
				print_dep(deplist_size_exp, deplist_exp);
			}
			xgf_trace("[XGFF][EXP] xgf_ret: %d at rpid:%d, runtime:%llu", ret_exp, tid,
				raw_runtime_exp);
			fpsgo_systrace_c_fbt_debug(tid, queueid, raw_runtime_exp,
				"xgff_runtime_original");
		}
	}

	mutex_lock(&xgf_main_lock);
	xgf_enter_delete_render_info(r->xgfrender.pid, r->xgfrender.bufid);
	mutex_unlock(&xgf_main_lock);

	xgf_free(r);

qudeq_notify_err:
	xgf_trace("[XGFF] non_xgf_ret: %d at rpid:%d, runtime:%llu", ret, tid, raw_runtime);

	return ret;
}

static int xgff_frame_startend(unsigned int startend,
		unsigned int tid,
		unsigned long long queueid,
		unsigned long long frameid,
		unsigned long long *cputime,
		unsigned int *area,
		unsigned int *pdeplistsize,
		unsigned int *pdeplist)
{
	unsigned long long cur_ts;

	if (!fpsgo_is_enable())
		return -EINVAL;

	cur_ts = fpsgo_get_time();

	fpsgo_systrace_c_xgf(tid, queueid, startend, "xgffs_queueid");
	fpsgo_systrace_c_xgf(tid, queueid, frameid, "xgffs_frameid");

	if (startend)
		return _xgff_frame_start(tid, queueid, frameid, pdeplistsize, pdeplist, cur_ts);

	return _xgff_frame_end(tid, queueid, frameid, cputime, area, pdeplistsize,
				pdeplist, cur_ts);
}

static void xgff_frame_getdeplist_maxsize(unsigned int *pdeplistsize)
{
	if (!pdeplistsize)
		return;
	*pdeplistsize = XGF_DEP_FRAMES_MAX;
}

#define MAX_XGF_EVENT_NUM xgf_event_buffer_size
#define MAX_FSTB_EVENT_NUM fstb_event_buffer_size

static void xgf_buffer_record_irq_waking_switch(int cpu, int event,
	int data, int note, int state, unsigned long long ts)
{
	int index;
	struct fpsgo_trace_event *fte;

	if (!atomic_read(&xgf_ko_enable))
		return;

Reget:
	index = atomic_inc_return(&xgf_event_buffer_idx);

	if (unlikely(index <= 0)) {
		atomic_set(&xgf_event_buffer_idx, 0);
		return;
	}

	if (unlikely(index > (MAX_XGF_EVENT_NUM + (xgf_nr_cpus << 1)))) {
		atomic_set(&xgf_event_buffer_idx, 0);
		return;
	}

	if (unlikely(index == MAX_XGF_EVENT_NUM))
		atomic_set(&xgf_event_buffer_idx, 0);
	else if (unlikely(index > MAX_XGF_EVENT_NUM))
		goto Reget;

	index -= 1;

	if (unlikely(!xgf_event_buffer || !MAX_XGF_EVENT_NUM))
		return;

	fte = &xgf_event_buffer[index];
	fte->ts = ts;
	fte->cpu = cpu;
	fte->event = event;
	fte->note = note;
	fte->state = state;
	fte->pid = data;
	fte->addr = 0;
}

static void fstb_buffer_record_waking_timer(int cpu, int event,
	int data, int note, unsigned long long ts, unsigned long long addr)
{
	int index;
	struct fpsgo_trace_event *fte;

	if (!atomic_read(&xgf_ko_enable))
		return;

Reget:
	index = atomic_inc_return(&fstb_event_buffer_idx);

	if (unlikely(index <= 0)) {
		atomic_set(&fstb_event_buffer_idx, 0);
		return;
	}

	if (unlikely(index > (MAX_FSTB_EVENT_NUM + (xgf_nr_cpus << 1)))) {
		atomic_set(&fstb_event_buffer_idx, 0);
		return;
	}

	if (unlikely(index == MAX_FSTB_EVENT_NUM))
		atomic_set(&fstb_event_buffer_idx, 0);
	else if (unlikely(index > MAX_FSTB_EVENT_NUM))
		goto Reget;

	index -= 1;

	if (unlikely(!fstb_event_buffer || !MAX_FSTB_EVENT_NUM))
		return;

	fte = &fstb_event_buffer[index];
	fte->ts = ts;
	fte->cpu = cpu;
	fte->event = event;
	fte->note = note;
	fte->pid = data;
	fte->state = 0;
	fte->addr = addr;
}

/* fake trace event for fpsgo */
void Test_fake_trace_event(int buffer, int cpu, int event, int data,
		int note, int state, unsigned long long ts, unsigned long long addr)
{
	switch (buffer) {
	case XGF_BUFFER:
		xgf_buffer_record_irq_waking_switch(cpu, event, data,
			note, state, ts);
		break;
	case FSTB_BUFFER:
		fstb_buffer_record_waking_timer(cpu, event, data,
			note, ts, addr);
		break;
	default:
		break;
	}
}
EXPORT_SYMBOL(Test_fake_trace_event);

static void xgf_irq_handler_entry_tracer(void *ignore,
					int irqnr,
					struct irqaction *irq_action)
{
	unsigned long long ts = fpsgo_get_time();
	int c_wake_cpu = xgf_get_task_wake_cpu(current);
	int c_pid = xgf_get_task_pid(current);

	xgf_buffer_record_irq_waking_switch(c_wake_cpu, IRQ_ENTRY,
		0, c_pid, irqnr, ts);
}

static void xgf_irq_handler_exit_tracer(void *ignore,
					int irqnr,
					struct irqaction *irq_action,
					int ret)
{
	unsigned long long ts = fpsgo_get_time();
	int c_wake_cpu = xgf_get_task_wake_cpu(current);
	int c_pid = xgf_get_task_pid(current);

	xgf_buffer_record_irq_waking_switch(c_wake_cpu, IRQ_EXIT,
		0, c_pid, irqnr, ts);
}

static inline long xgf_trace_sched_switch_state(bool preempt,
				struct task_struct *p)
{
	long state = 0;

	if (!p)
		goto out;

	state = xgf_get_task_state(p);

	if (preempt)
		state = TASK_RUNNING | TASK_STATE_MAX;

out:
	return state;
}

static void xgf_sched_switch_tracer(void *ignore,
				bool preempt,
				struct task_struct *prev,
				struct task_struct *next,
				unsigned int prev_state)
{
	long local_prev_state;
	long temp_state;
	unsigned long long ts = fpsgo_get_time();
	int c_wake_cpu = xgf_get_task_wake_cpu(current);
	int prev_pid;
	int next_pid;

	if (!prev || !next)
		return;

	prev_pid = xgf_get_task_pid(prev);
	next_pid = xgf_get_task_pid(next);
	local_prev_state = xgf_trace_sched_switch_state(preempt, prev);
	temp_state = local_prev_state & (TASK_STATE_MAX-1);

	if (temp_state)
		xgf_buffer_record_irq_waking_switch(c_wake_cpu, SCHED_SWITCH,
			next_pid, prev_pid, 1, ts);
	else
		xgf_buffer_record_irq_waking_switch(c_wake_cpu, SCHED_SWITCH,
			next_pid, prev_pid, 0, ts);
}

static void xgf_sched_waking_tracer(void *ignore, struct task_struct *p)
{
	unsigned long long ts = fpsgo_get_time();
	int c_wake_cpu = xgf_get_task_wake_cpu(current);
	int c_pid = xgf_get_task_pid(current);
	int p_pid;

	if (!p)
		return;

	p_pid = xgf_get_task_pid(p);

	xgf_buffer_record_irq_waking_switch(c_wake_cpu, SCHED_WAKING,
		p_pid, c_pid, 512, ts);
	fstb_buffer_record_waking_timer(c_wake_cpu, SCHED_WAKING,
		p_pid, c_pid, ts, 0);
}

static void xgf_hrtimer_expire_entry_tracer(void *ignore,
	struct hrtimer *hrtimer, ktime_t *now)
{
	unsigned long long ts = fpsgo_get_time();
	unsigned long long timer_addr = (unsigned long long)hrtimer;
	int c_wake_cpu = xgf_get_task_wake_cpu(current);
	int c_pid = xgf_get_task_pid(current);

	fstb_buffer_record_waking_timer(c_wake_cpu, HRTIMER_ENTRY,
		0, c_pid, ts, timer_addr);
}

static void xgf_hrtimer_expire_exit_tracer(void *ignore, struct hrtimer *hrtimer)
{
	unsigned long long ts = fpsgo_get_time();
	unsigned long long timer_addr = (unsigned long long)hrtimer;
	int c_wake_cpu = xgf_get_task_wake_cpu(current);
	int c_pid = xgf_get_task_pid(current);

	fstb_buffer_record_waking_timer(c_wake_cpu, HRTIMER_EXIT,
		0, c_pid, ts, timer_addr);
}

struct tracepoints_table {
	const char *name;
	void *func;
	struct tracepoint *tp;
	bool registered;
};

static struct tracepoints_table xgf_tracepoints[] = {
	{.name = "irq_handler_entry", .func = xgf_irq_handler_entry_tracer},
	{.name = "irq_handler_exit", .func = xgf_irq_handler_exit_tracer},
	{.name = "sched_switch", .func = xgf_sched_switch_tracer},
	{.name = "sched_waking", .func = xgf_sched_waking_tracer},
	{.name = "hrtimer_expire_entry", .func = xgf_hrtimer_expire_entry_tracer},
	{.name = "hrtimer_expire_exit", .func = xgf_hrtimer_expire_exit_tracer},
};

static void __nocfi xgf_tracing_register(void)
{
	int ret;

	xgf_nr_cpus = xgf_num_possible_cpus();

	/* xgf_irq_handler_entry_tracer */
	ret = xgf_tracepoint_probe_register(xgf_tracepoints[0].tp,
						xgf_tracepoints[0].func,  NULL);

	if (ret) {
		pr_info("irq trace: Couldn't activate tracepoint probe to irq_handler_entry\n");
		goto fail_reg_irq_handler_entry;
	}
	xgf_tracepoints[0].registered = true;

	/* xgf_irq_handler_exit_tracer */
	ret = xgf_tracepoint_probe_register(xgf_tracepoints[1].tp,
						xgf_tracepoints[1].func,  NULL);

	if (ret) {
		pr_info("irq trace: Couldn't activate tracepoint probe to irq_handler_exit\n");
		goto fail_reg_irq_handler_exit;
	}
	xgf_tracepoints[1].registered = true;

	/* xgf_sched_switch_tracer */
	ret = xgf_tracepoint_probe_register(xgf_tracepoints[2].tp,
						xgf_tracepoints[2].func,  NULL);

	if (ret) {
		pr_info("sched trace: Couldn't activate tracepoint probe to sched_switch\n");
		goto fail_reg_sched_switch;
	}
	xgf_tracepoints[2].registered = true;

	/* xgf_sched_waking_tracer */
	ret = xgf_tracepoint_probe_register(xgf_tracepoints[3].tp,
						xgf_tracepoints[3].func,  NULL);

	if (ret) {
		pr_info("sched trace: Couldn't activate tracepoint probe to sched_waking\n");
		goto fail_reg_sched_waking;
	}
	xgf_tracepoints[3].registered = true;

	/* xgf_hrtimer_expire_entry_tracer */
	ret = xgf_tracepoint_probe_register(xgf_tracepoints[4].tp,
						xgf_tracepoints[4].func,  NULL);

	if (ret) {
		pr_info("hrtimer trace: Couldn't activate tracepoint probe to hrtimer_expire_entry\n");
		goto fail_reg_hrtimer_expire_entry;
	}
	xgf_tracepoints[4].registered = true;

	/* xgf_hrtimer_expire_exit_tracer */
	ret = xgf_tracepoint_probe_register(xgf_tracepoints[5].tp,
						xgf_tracepoints[5].func,  NULL);

	if (ret) {
		pr_info("hrtimer trace: Couldn't activate tracepoint probe to hrtimer_expire_exit\n");
		goto fail_reg_hrtimer_expire_exit;
	}
	xgf_tracepoints[5].registered = true;

	atomic_set(&xgf_event_buffer_idx, 0);
	atomic_set(&fstb_event_buffer_idx, 0);
	return; /* successful registered all */

fail_reg_hrtimer_expire_exit:
	xgf_tracepoint_probe_unregister(xgf_tracepoints[5].tp,
					xgf_tracepoints[5].func,  NULL);
	xgf_tracepoints[5].registered = false;
fail_reg_hrtimer_expire_entry:
	xgf_tracepoint_probe_unregister(xgf_tracepoints[4].tp,
					xgf_tracepoints[4].func,  NULL);
	xgf_tracepoints[4].registered = false;
fail_reg_sched_waking:
	xgf_tracepoint_probe_unregister(xgf_tracepoints[3].tp,
					xgf_tracepoints[3].func,  NULL);
	xgf_tracepoints[3].registered = false;
fail_reg_sched_switch:
	xgf_tracepoint_probe_unregister(xgf_tracepoints[2].tp,
					xgf_tracepoints[2].func,  NULL);
	xgf_tracepoints[2].registered = false;
fail_reg_irq_handler_exit:
	xgf_tracepoint_probe_unregister(xgf_tracepoints[1].tp,
					xgf_tracepoints[1].func,  NULL);
	xgf_tracepoints[1].registered = false;
fail_reg_irq_handler_entry:
	xgf_tracepoint_probe_unregister(xgf_tracepoints[0].tp,
					xgf_tracepoints[0].func,  NULL);
	xgf_tracepoints[0].registered = false;

	atomic_set(&xgf_ko_enable, 0);
	atomic_set(&xgf_event_buffer_idx, 0);
	atomic_set(&fstb_event_buffer_idx, 0);
}

static void __nocfi xgf_tracing_unregister(void)
{
	xgf_tracepoint_probe_unregister(xgf_tracepoints[0].tp,
					xgf_tracepoints[0].func,  NULL);
	xgf_tracepoints[0].registered = false;
	xgf_tracepoint_probe_unregister(xgf_tracepoints[1].tp,
					xgf_tracepoints[1].func,  NULL);
	xgf_tracepoints[1].registered = false;
	xgf_tracepoint_probe_unregister(xgf_tracepoints[2].tp,
					xgf_tracepoints[2].func,  NULL);
	xgf_tracepoints[2].registered = false;
	xgf_tracepoint_probe_unregister(xgf_tracepoints[3].tp,
					xgf_tracepoints[3].func,  NULL);
	xgf_tracepoints[3].registered = false;
	xgf_tracepoint_probe_unregister(xgf_tracepoints[4].tp,
					xgf_tracepoints[4].func,  NULL);
	xgf_tracepoints[4].registered = false;
	xgf_tracepoint_probe_unregister(xgf_tracepoints[5].tp,
					xgf_tracepoints[5].func,  NULL);
	xgf_tracepoints[5].registered = false;

	atomic_set(&xgf_ko_enable, 0);
	atomic_set(&xgf_event_buffer_idx, 0);
	atomic_set(&fstb_event_buffer_idx, 0);
}

static int xgf_stat_xchg(int xgf_enable)
{
	int ret = -1;

	if (xgf_enable) {
		xgf_tracing_register();
		ret = 1;
		atomic_set(&xgf_ko_enable, 1);
	} else {
		xgf_tracing_unregister();
		ret = 0;
		atomic_set(&xgf_ko_enable, 0);
	}

	return ret;
}

static void xgf_enter_state_xchg(int enable)
{
	int ret = 0;

	if (enable != 0 && enable != 1) {
		ret = -1;
		goto out;
	}

	ret = xgf_stat_xchg(enable);

out:
	xgf_trace("xgf k2ko xchg ret:%d enable:%d", ret, enable);
}

void fpsgo_ctrl2xgf_switch_xgf(int val)
{
	mutex_lock(&xgf_main_lock);
	if (val != xgf_enable) {
		xgf_enable = val;

		if (xgf_ko_is_ready())
			xgf_enter_state_xchg(xgf_enable);

		mutex_unlock(&xgf_main_lock);

		xgf_reset_all_renders();
		xgff_reset_all_renders();
	} else
		mutex_unlock(&xgf_main_lock);
}

int notify_xgf_ko_ready(void)
{
	int ret = 0;

	if (unlikely(!xgf_event_buffer || !xgf_event_buffer_size)) {
		pr_info("%s: get xgf_event_buffer fail\n", __func__);
		goto out;
	}
	if (unlikely(!fstb_event_buffer || !fstb_event_buffer_size)) {
		pr_info("%s: get fstb_event_buffer fail\n", __func__);
		goto out;
	}

	mutex_lock(&xgf_main_lock);

	xgf_ko_ready = 1;
	if (xgf_is_enable()) {
		xgf_enter_state_xchg(xgf_enable);
		ret = 1;
	}
	mutex_unlock(&xgf_main_lock);
out:
	return ret;
}
EXPORT_SYMBOL(notify_xgf_ko_ready);

#define FOR_EACH_INTEREST_MAX \
	(sizeof(xgf_tracepoints) / sizeof(struct tracepoints_table))

#define FOR_EACH_INTEREST(i) \
	for (i = 0; i < FOR_EACH_INTEREST_MAX; i++)

static void lookup_tracepoints(struct tracepoint *tp, void *ignore)
{
	int i;

	FOR_EACH_INTEREST(i) {
		if (strcmp(xgf_tracepoints[i].name, tp->name) == 0)
			xgf_tracepoints[i].tp = tp;
	}
}

static void clean_xgf_tp(void)
{
	int i;

	FOR_EACH_INTEREST(i) {
		if (xgf_tracepoints[i].registered) {
			xgf_tracepoint_probe_unregister(xgf_tracepoints[i].tp,
						xgf_tracepoints[i].func, NULL);
			xgf_tracepoints[i].registered = false;
		}
	}
}

int __init init_xgf_ko(void)
{
	int i;

	for_each_kernel_tracepoint(lookup_tracepoints, NULL);

	FOR_EACH_INTEREST(i) {
		if (xgf_tracepoints[i].tp == NULL) {
			pr_debug("XGF KO Error, %s not found\n",
					xgf_tracepoints[i].name);
			clean_xgf_tp();
			return -1;
		}
	}

	atomic_set(&xgf_ko_enable, 1);
	atomic_set(&xgf_event_buffer_idx, 0);
	atomic_set(&fstb_event_buffer_idx, 0);

	return 0;
}

#define XGF_SYSFS_READ(name, show, variable); \
static ssize_t name##_show(struct kobject *kobj, \
		struct kobj_attribute *attr, \
		char *buf) \
{ \
	if ((show)) \
		return scnprintf(buf, PAGE_SIZE, "%d\n", (variable)); \
	else \
		return 0; \
}

#define XGF_SYSFS_WRITE_VALUE(name, lock, variable, min, max); \
static ssize_t name##_store(struct kobject *kobj, \
		struct kobj_attribute *attr, \
		const char *buf, size_t count) \
{ \
	char *acBuffer = NULL; \
	int arg; \
\
	acBuffer = kcalloc(FPSGO_SYSFS_MAX_BUFF_SIZE, sizeof(char), GFP_KERNEL); \
	if (!acBuffer) \
		goto out; \
\
	if ((count > 0) && (count < FPSGO_SYSFS_MAX_BUFF_SIZE)) { \
		if (scnprintf(acBuffer, FPSGO_SYSFS_MAX_BUFF_SIZE, "%s", buf)) { \
			if (kstrtoint(acBuffer, 0, &arg) == 0) { \
				if (arg >= (min) && arg <= (max)) { \
					mutex_lock(&(lock)); \
					(variable) = arg; \
					mutex_unlock(&(lock)); \
				} \
			} \
		} \
	} \
\
out: \
	kfree(acBuffer); \
	return count; \
}

XGF_SYSFS_READ(xgf_trace_enable, 1, xgf_trace_enable);
XGF_SYSFS_WRITE_VALUE(xgf_trace_enable, xgf_main_lock, xgf_trace_enable, 0, 1);
static KOBJ_ATTR_RW(xgf_trace_enable);

XGF_SYSFS_READ(xgf_log_trace_enable, 1, xgf_log_trace_enable);
XGF_SYSFS_WRITE_VALUE(xgf_log_trace_enable, xgf_main_lock, xgf_log_trace_enable, 0, 1);
static KOBJ_ATTR_RW(xgf_log_trace_enable);

XGF_SYSFS_READ(xgf_cfg_spid, 1, xgf_cfg_spid);
XGF_SYSFS_WRITE_VALUE(xgf_cfg_spid, xgf_main_lock, xgf_cfg_spid, 0, 1);
static KOBJ_ATTR_RW(xgf_cfg_spid);

XGF_SYSFS_READ(xgf_dep_frames, 1, xgf_dep_frames);
XGF_SYSFS_WRITE_VALUE(xgf_dep_frames, xgf_main_lock, xgf_dep_frames,
			XGF_DEP_FRAMES_MIN, XGF_DEP_FRAMES_MAX);
static KOBJ_ATTR_RW(xgf_dep_frames);

XGF_SYSFS_READ(xgf_extra_sub, 1, xgf_extra_sub);
XGF_SYSFS_WRITE_VALUE(xgf_extra_sub, xgf_main_lock, xgf_extra_sub, 0, 1);
static KOBJ_ATTR_RW(xgf_extra_sub);

XGF_SYSFS_READ(xgf_force_no_extra_sub, 1, xgf_force_no_extra_sub);
XGF_SYSFS_WRITE_VALUE(xgf_force_no_extra_sub, xgf_main_lock, xgf_force_no_extra_sub, 0, 1);
static KOBJ_ATTR_RW(xgf_force_no_extra_sub);

XGF_SYSFS_READ(xgf_ema_dividend, 1, xgf_ema_dividend);
XGF_SYSFS_WRITE_VALUE(xgf_ema_dividend, xgf_main_lock, xgf_ema_dividend, 1, 9);
static KOBJ_ATTR_RW(xgf_ema_dividend);

XGF_SYSFS_READ(xgf_spid_ck_period, 1, xgf_spid_ck_period);
XGF_SYSFS_WRITE_VALUE(xgf_spid_ck_period, xgf_main_lock, xgf_spid_ck_period, 0, NSEC_PER_SEC);
static KOBJ_ATTR_RW(xgf_spid_ck_period);

XGF_SYSFS_READ(xgf_sp_name_id, 1, xgf_sp_name_id);
XGF_SYSFS_WRITE_VALUE(xgf_sp_name_id, xgf_main_lock, xgf_sp_name_id, 0, 1);
static KOBJ_ATTR_RW(xgf_sp_name_id);

XGF_SYSFS_READ(xgf_ema2_enable, 1, xgf_ema2_enable);
XGF_SYSFS_WRITE_VALUE(xgf_ema2_enable, xgf_main_lock, xgf_ema2_enable, 0, 1);
static KOBJ_ATTR_RW(xgf_ema2_enable);

XGF_SYSFS_READ(xgff_mips_exp_enable, 1, is_xgff_mips_exp_enable);
XGF_SYSFS_WRITE_VALUE(xgff_mips_exp_enable, xgff_frames_lock, is_xgff_mips_exp_enable, 0, 1);
static KOBJ_ATTR_RW(xgff_mips_exp_enable);

XGF_SYSFS_READ(set_cam_hal_pid, 1, cam_hal_pid);
XGF_SYSFS_WRITE_VALUE(set_cam_hal_pid, xgf_main_lock, cam_hal_pid, 0, INT_MAX);
static KOBJ_ATTR_RW(set_cam_hal_pid);

XGF_SYSFS_READ(set_cam_server_pid, 1, cam_server_pid);
XGF_SYSFS_WRITE_VALUE(set_cam_server_pid, xgf_main_lock, cam_server_pid, 0, INT_MAX);
static KOBJ_ATTR_RW(set_cam_server_pid);

static ssize_t xgf_ema2_enable_by_pid_show(struct kobject *kobj,
		struct kobj_attribute *attr,
		char *buf)
{
	char *temp = NULL;
	int i = 1;
	int pos = 0;
	int length = 0;
	struct xgf_policy_cmd *iter;
	struct rb_root *rbr;
	struct rb_node *rbn;

	temp = kcalloc(FPSGO_SYSFS_MAX_BUFF_SIZE, sizeof(char), GFP_KERNEL);
	if (!temp)
		goto out;

	mutex_lock(&xgf_policy_cmd_lock);

	rbr = &xgf_policy_cmd_tree;
	for (rbn = rb_first(rbr); rbn; rbn = rb_next(rbn)) {
		iter = rb_entry(rbn, struct xgf_policy_cmd, rb_node);
		length = scnprintf(temp + pos,
			FPSGO_SYSFS_MAX_BUFF_SIZE - pos,
			"%dth\ttgid:%d\tema2_enable:%d\tts:%llu\n",
			i, iter->tgid, iter->ema2_enable, iter->ts);
		pos += length;
		i++;
	}

	mutex_unlock(&xgf_policy_cmd_lock);

	length = scnprintf(buf, PAGE_SIZE, "%s", temp);

out:
	kfree(temp);
	return length;
}

static ssize_t xgf_ema2_enable_by_pid_store(struct kobject *kobj,
		struct kobj_attribute *attr,
		const char *buf, size_t count)
{
	char *acBuffer = NULL;
	int tgid;
	int ema2_enable;
	unsigned long long ts = fpsgo_get_time();
	struct xgf_policy_cmd *iter;

	acBuffer = kcalloc(FPSGO_SYSFS_MAX_BUFF_SIZE, sizeof(char), GFP_KERNEL);
	if (!acBuffer)
		goto out;

	if ((count > 0) && (count < FPSGO_SYSFS_MAX_BUFF_SIZE)) {
		if (scnprintf(acBuffer, FPSGO_SYSFS_MAX_BUFF_SIZE, "%s", buf)) {
			if (sscanf(acBuffer, "%d %d", &tgid, &ema2_enable) == 2) {
				mutex_lock(&xgf_policy_cmd_lock);
				if (ema2_enable > 0)
					iter = xgf_get_policy_cmd(tgid, !!ema2_enable, ts, 1);
				else {
					iter = xgf_get_policy_cmd(tgid, ema2_enable, ts, 0);
					if (iter)
						xgf_delete_policy_cmd(iter);
				}
				mutex_unlock(&xgf_policy_cmd_lock);
			}
		}
	}

out:
	kfree(acBuffer);
	return count;
}

static KOBJ_ATTR_RW(xgf_ema2_enable_by_pid);

static ssize_t xgf_spid_list_show(struct kobject *kobj,
	struct kobj_attribute *attr, char *buf)
{
	struct xgf_spid *xgf_spid_iter = NULL;
	char *temp = NULL;
	int pos = 0;
	int length = 0;

	temp = kcalloc(FPSGO_SYSFS_MAX_BUFF_SIZE, sizeof(char), GFP_KERNEL);
	if (!temp)
		goto out;

	length = scnprintf(temp + pos, FPSGO_SYSFS_MAX_BUFF_SIZE - pos,
		"%s\t%s\t%s\n",
		"process_name",
		"thread_name",
		"action");
	pos += length;

	hlist_for_each_entry(xgf_spid_iter, &xgf_spid_list, hlist) {
		length = scnprintf(temp + pos, FPSGO_SYSFS_MAX_BUFF_SIZE - pos,
			"%s\t%s\t%d\n",
			xgf_spid_iter->process_name,
			xgf_spid_iter->thread_name,
			xgf_spid_iter->action);
		pos += length;
	}

	length = scnprintf(temp + pos, FPSGO_SYSFS_MAX_BUFF_SIZE - pos,
		"\n%s\t%s\t%s\t%s\t%s\t%s\n",
		"process_name",
		"thread_name",
		"render",
		"pid",
		"tid",
		"action");
	pos += length;

	hlist_for_each_entry(xgf_spid_iter, &xgf_wspid_list, hlist) {
		length = scnprintf(temp + pos, FPSGO_SYSFS_MAX_BUFF_SIZE - pos,
			"%s\t%s\t%d\t%d\t%d\t%d\n",
			xgf_spid_iter->process_name,
			xgf_spid_iter->thread_name,
			xgf_spid_iter->rpid,
			xgf_spid_iter->pid,
			xgf_spid_iter->tid,
			xgf_spid_iter->action);
		pos += length;
	}

	length = scnprintf(buf, PAGE_SIZE, "%s", temp);

out:
	kfree(temp);
	return length;
}

static ssize_t xgf_spid_list_store(struct kobject *kobj,
	struct kobj_attribute *attr, const char *buf, size_t count)
{
	char *acBuffer = NULL;
	char proc_name[16], thrd_name[16];
	int action;

	acBuffer = kcalloc(FPSGO_SYSFS_MAX_BUFF_SIZE, sizeof(char), GFP_KERNEL);
	if (!acBuffer)
		goto out;

	if ((count > 0) && (count < FPSGO_SYSFS_MAX_BUFF_SIZE)) {
		if (scnprintf(acBuffer, FPSGO_SYSFS_MAX_BUFF_SIZE, "%s", buf)) {
			acBuffer[count] = '\0';
			if (sscanf(acBuffer, "%15s %15s %d",
				proc_name, thrd_name, &action) != 3)
				goto out;

			if (set_xgf_spid_list(proc_name, thrd_name, action))
				goto out;
		}
	}

out:
	kfree(acBuffer);
	return count;
}

static KOBJ_ATTR_RW(xgf_spid_list);

static ssize_t xgf_deplist_show(struct kobject *kobj,
		struct kobj_attribute *attr,
		char *buf)
{
	char *temp = NULL;
	int pos = 0;
	int length = 0;
	struct xgf_render_if *r_iter;
	struct xgf_dep *xd;
	struct hlist_node *r_tmp;
	struct rb_node *rbn;

	temp = kcalloc(FPSGO_SYSFS_MAX_BUFF_SIZE, sizeof(char), GFP_KERNEL);
	if (!temp)
		goto out;

	mutex_lock(&xgf_main_lock);

	hlist_for_each_entry_safe(r_iter, r_tmp, &xgf_render_if_list, hlist) {
		length = scnprintf(temp + pos, FPSGO_SYSFS_MAX_BUFF_SIZE - pos,
			"pid:%d bufID:0x%llx size:%d\n",
			r_iter->pid, r_iter->bufid, r_iter->dep_list_size);
		pos += length;
		for (rbn = rb_first(&r_iter->dep_list); rbn; rbn = rb_next(rbn)) {
			xd = rb_entry(rbn, struct xgf_dep, rb_node);
			length = scnprintf(temp + pos,
				FPSGO_SYSFS_MAX_BUFF_SIZE - pos,
				" %d(%d)", xd->tid, xd->action);
			pos += length;
		}
		length = scnprintf(temp + pos, FPSGO_SYSFS_MAX_BUFF_SIZE - pos,
			"\n");
		pos += length;
	}

	mutex_unlock(&xgf_main_lock);

	length = scnprintf(buf, PAGE_SIZE, "%s", temp);

out:
	kfree(temp);
	return length;
}

static KOBJ_ATTR_RO(xgf_deplist);

static ssize_t xgf_runtime_show(struct kobject *kobj,
		struct kobj_attribute *attr,
		char *buf)
{
	struct xgf_render_if *r_iter;
	struct hlist_node *r_tmp;
	char *temp = NULL;
	int pos = 0;
	int length = 0;

	temp = kcalloc(FPSGO_SYSFS_MAX_BUFF_SIZE, sizeof(char), GFP_KERNEL);
	if (!temp)
		goto out;

	mutex_lock(&xgf_main_lock);

	hlist_for_each_entry_safe(r_iter, r_tmp, &xgf_render_if_list, hlist) {
		length = scnprintf(temp + pos,
			FPSGO_SYSFS_MAX_BUFF_SIZE - pos,
			"rtid:%d bid:0x%llx cpu_runtime:%llu (%llu)\n",
			r_iter->pid, r_iter->bufid,
			r_iter->ema_t_cpu,
			r_iter->raw_t_cpu);
		pos += length;
	}

	mutex_unlock(&xgf_main_lock);

	length = scnprintf(buf, PAGE_SIZE, "%s", temp);

out:
	kfree(temp);
	return length;
}

static KOBJ_ATTR_RO(xgf_runtime);

int __init init_xgf(void)
{
	init_xgf_ko();

	if (!fpsgo_sysfs_create_dir(NULL, "xgf", &xgf_kobj)) {
		fpsgo_sysfs_create_file(xgf_kobj, &kobj_attr_xgf_trace_enable);
		fpsgo_sysfs_create_file(xgf_kobj, &kobj_attr_xgf_log_trace_enable);
		fpsgo_sysfs_create_file(xgf_kobj, &kobj_attr_xgf_cfg_spid);
		fpsgo_sysfs_create_file(xgf_kobj, &kobj_attr_xgf_dep_frames);
		fpsgo_sysfs_create_file(xgf_kobj, &kobj_attr_xgf_extra_sub);
		fpsgo_sysfs_create_file(xgf_kobj, &kobj_attr_xgf_force_no_extra_sub);
		fpsgo_sysfs_create_file(xgf_kobj, &kobj_attr_xgf_ema_dividend);
		fpsgo_sysfs_create_file(xgf_kobj, &kobj_attr_xgf_spid_ck_period);
		fpsgo_sysfs_create_file(xgf_kobj, &kobj_attr_xgf_sp_name_id);
		fpsgo_sysfs_create_file(xgf_kobj, &kobj_attr_xgf_ema2_enable);
		fpsgo_sysfs_create_file(xgf_kobj, &kobj_attr_xgf_ema2_enable_by_pid);
		fpsgo_sysfs_create_file(xgf_kobj, &kobj_attr_xgf_spid_list);
		fpsgo_sysfs_create_file(xgf_kobj, &kobj_attr_xgf_deplist);
		fpsgo_sysfs_create_file(xgf_kobj, &kobj_attr_xgf_runtime);
		fpsgo_sysfs_create_file(xgf_kobj, &kobj_attr_xgff_mips_exp_enable);
		fpsgo_sysfs_create_file(xgf_kobj, &kobj_attr_set_cam_hal_pid);
		fpsgo_sysfs_create_file(xgf_kobj, &kobj_attr_set_cam_server_pid);
	}

	xgf_policy_cmd_tree = RB_ROOT;
	xgff_frame_startend_fp = xgff_frame_startend;
	xgff_frame_getdeplist_maxsize_fp = xgff_frame_getdeplist_maxsize;

	return 0;
}

int __exit exit_xgf(void)
{
	xgf_reset_all_renders();
	xgff_reset_all_renders();

	clean_xgf_tp();

	fpsgo_sysfs_remove_file(xgf_kobj, &kobj_attr_xgf_trace_enable);
	fpsgo_sysfs_remove_file(xgf_kobj, &kobj_attr_xgf_log_trace_enable);
	fpsgo_sysfs_remove_file(xgf_kobj, &kobj_attr_xgf_cfg_spid);
	fpsgo_sysfs_remove_file(xgf_kobj, &kobj_attr_xgf_dep_frames);
	fpsgo_sysfs_remove_file(xgf_kobj, &kobj_attr_xgf_extra_sub);
	fpsgo_sysfs_remove_file(xgf_kobj, &kobj_attr_xgf_force_no_extra_sub);
	fpsgo_sysfs_remove_file(xgf_kobj, &kobj_attr_xgf_ema_dividend);
	fpsgo_sysfs_remove_file(xgf_kobj, &kobj_attr_xgf_spid_ck_period);
	fpsgo_sysfs_remove_file(xgf_kobj, &kobj_attr_xgf_sp_name_id);
	fpsgo_sysfs_remove_file(xgf_kobj, &kobj_attr_xgf_ema2_enable);
	fpsgo_sysfs_remove_file(xgf_kobj, &kobj_attr_xgf_ema2_enable_by_pid);
	fpsgo_sysfs_remove_file(xgf_kobj, &kobj_attr_xgf_spid_list);
	fpsgo_sysfs_remove_file(xgf_kobj, &kobj_attr_xgf_deplist);
	fpsgo_sysfs_remove_file(xgf_kobj, &kobj_attr_xgf_runtime);
	fpsgo_sysfs_remove_file(xgf_kobj, &kobj_attr_xgff_mips_exp_enable);
	fpsgo_sysfs_remove_file(xgf_kobj, &kobj_attr_set_cam_hal_pid);
	fpsgo_sysfs_remove_file(xgf_kobj, &kobj_attr_set_cam_server_pid);

	fpsgo_sysfs_remove_dir(&xgf_kobj);

	return 0;
}
