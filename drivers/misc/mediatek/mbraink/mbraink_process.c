// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2022 MediaTek Inc.
 */
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/rtc.h>
#include <linux/sched/clock.h>
#include <linux/kernel.h>
#include <linux/uaccess.h>
#include <linux/cdev.h>
#include <linux/proc_fs.h>
#include <linux/sched/signal.h>
#include <linux/pid_namespace.h>
#include <linux/mm.h>
#include <linux/sched/mm.h>
#include <linux/sched/task.h>
#include <linux/pagewalk.h>
#include <linux/shmem_fs.h>
#include <linux/pagemap.h>
#include <linux/mempolicy.h>
#include <linux/rmap.h>
#include <linux/sched/cputime.h>
#include <linux/math64.h>
#include <linux/refcount.h>
#include <linux/ctype.h>
#include <linux/stddef.h>
#include <linux/cred.h>
#include <linux/spinlock.h>
#include <linux/rtc.h>
#include <linux/sched/clock.h>
#include <trace/hooks/sched.h>
#include <linux/mm_inline.h>

#include "mbraink_process.h"

#define PROCESS_INFO_STR	\
	"pid=%-10u:uid=%u,priority=%d,utime=%llu,stime=%llu,cutime=%llu,cstime=%llu,name=%s\n"

#define THREAD_INFO_STR		\
	"--> pid=%-10u:uid=%u,priority=%d,utime=%llu,stime=%llu,cutime=%llu,cstime=%llu,name=%s\n"

/*spinlock for mbraink monitored pidlist*/
static DEFINE_SPINLOCK(monitor_pidlist_lock);
/*Please make sure that monitor pidlist is protected by spinlock*/
struct mbraink_monitor_pidlist mbraink_monitor_pidlist_data;

/*spinlock for mbraink tracing pidlist*/
static DEFINE_SPINLOCK(tracing_pidlist_lock);
/*Please make sure that tracing pidlist is protected by spinlock*/
struct mbraink_tracing_pidlist mbraink_tracing_pidlist_data[MAX_TRACE_NUM];

#if (MBRAINK_LANDING_PONSOT_CHECK == 1)
static int register_trace_android_vh_do_fork(void *t, void *p)
{
	pr_info("%s: not support yet...", __func__);
	return 0;
}

static int register_trace_android_vh_do_exit(void *t, void *p)
{
	pr_info("%s: not support yet...", __func__);
	return 0;
}

static int unregister_trace_android_vh_do_fork(void *t, void *p)
{
	pr_info("%s: not support yet...", __func__);
	return 0;
}
static int unregister_trace_android_vh_do_exit(void *t, void *p)
{
	pr_info("%s: not support yet...", __func__);
	return 0;
}
#endif

#if (MBRAINK_LANDING_PONSOT_CHECK == 1)
void mbraink_get_process_memory_info(pid_t current_pid,
					struct mbraink_process_memory_data *process_memory_buffer)
{
	pr_info("%s: not support yet...", __func__);
}
#else
void mbraink_map_vma(struct vm_area_struct *vma, unsigned long cur_pss,
			unsigned long *native_heap, unsigned long *java_heap)
{
	struct mm_struct *mm = vma->vm_mm;
	const char *name = NULL;

	/*
	 * Print the dentry name for named mappings, and a
	 * special [heap] marker for the heap:
	 */

	if (vma->vm_ops && vma->vm_ops->name) {
		name = vma->vm_ops->name(vma);
		if (name) {
			if (strncmp(name, "dev/ashmem/libc malloc", 23) == 0)
				(*native_heap) += cur_pss;
			return;
		}
	}

	name = arch_vma_name(vma);
	if (!name) {
		struct anon_vma_name *anon_name;

		if (!mm)
			return;

		if (vma->vm_start <= mm->brk && vma->vm_end >= mm->start_brk) {
			(*native_heap) += cur_pss;
			return;
		}

		if (vma->vm_start <= vma->vm_mm->start_stack &&
			vma->vm_end >= vma->vm_mm->start_stack)
			return;

		anon_name = anon_vma_name(vma);
		if (anon_name) {
			if (strstr(anon_name->name, "scudo"))
				(*native_heap) += cur_pss;
			else if (strstr(anon_name->name, "libc_malloc"))
				(*native_heap) += cur_pss;
			else if (strstr(anon_name->name, "GWP-ASan"))
				(*native_heap) += cur_pss;
			else if (strstr(anon_name->name, "dalvik-alloc space"))
				(*java_heap) += cur_pss;
			else if (strstr(anon_name->name, "dalvik-main space"))
				(*java_heap) += cur_pss;
			else if (strstr(anon_name->name, "dalvik-large object space"))
				(*java_heap) += cur_pss;
			else if (strstr(anon_name->name, "dalvik-free list large object space"))
				(*java_heap) += cur_pss;
			else if (strstr(anon_name->name, "dalvik-non moving space"))
				(*java_heap) += cur_pss;
			else if (strstr(anon_name->name, "dalvik-zygote space"))
				(*java_heap) += cur_pss;
		}
	}
}

void mbraink_get_process_memory_info(pid_t current_pid,
				struct mbraink_process_memory_data *process_memory_buffer)
{
	struct task_struct *t = NULL;
	struct mm_struct *mm = NULL;
	struct vm_area_struct *vma = NULL;
	struct mem_size_stats mss;
	unsigned short pid_count = 0;
	unsigned long pss, uss, rss, swap, cur_pss;
	unsigned long java_heap = 0, native_heap = 0;
	int ret = 0;

	memset(process_memory_buffer, 0, sizeof(struct mbraink_process_memory_data));
	process_memory_buffer->pid = 0;

	read_lock(&tasklist_lock);
	for_each_process(t) {
		if (t->pid < current_pid)
			continue;

		mm = t->mm;
		if (mm) {
			vma = find_vma(mm, 0);
			if (!vma)
				vma =  get_gate_vma(mm);

			if (vma) {
				java_heap = 0;
				native_heap = 0;

				memset(&mss, 0, sizeof(mss));
				while (vma) {
					cur_pss = (unsigned long)(mss.pss >> PSS_SHIFT);
					smap_gather_stats(vma, &mss, 0);
					cur_pss =
						((unsigned long)(mss.pss >> PSS_SHIFT)) - cur_pss;
					cur_pss = cur_pss / 1024;
					mbraink_map_vma(vma, cur_pss, &native_heap, &java_heap);

					vma = vma->vm_next;
				}

				pss = (unsigned long)(mss.pss >> PSS_SHIFT)/1024;
				uss = (mss.private_clean+mss.private_dirty)/1024;
				rss = (mss.resident) / 1024;
				swap = (mss.swap) / 1024;
				pid_count = process_memory_buffer->pid_count;

				if (pid_count < MAX_MEM_STRUCT_SZ) {
					process_memory_buffer->drv_data[pid_count].pid =
									(unsigned short)(t->pid);
					process_memory_buffer->drv_data[pid_count].pss = pss;
					process_memory_buffer->drv_data[pid_count].uss = uss;
					process_memory_buffer->drv_data[pid_count].rss = rss;
					process_memory_buffer->drv_data[pid_count].swap = swap;
					process_memory_buffer->drv_data[pid_count].java_heap =
										java_heap;
					process_memory_buffer->drv_data[pid_count].native_heap =
										native_heap;
					process_memory_buffer->pid_count++;
				} else {
					ret = -1;
					process_memory_buffer->pid =
						(unsigned short)(t->pid);
					break;
				}
			} else {
				pr_notice("no vma is mapped.\n");
			}
		} else {
			/*pr_info("kthread case ...\n");*/
		}
	}

	pr_info("%s: current_pid = %u, count = %u\n",
		__func__, process_memory_buffer->pid, process_memory_buffer->pid_count);
	read_unlock(&tasklist_lock);
}
#endif

void mbraink_get_process_stat_info(pid_t current_pid,
		struct mbraink_process_stat_data *process_stat_buffer)
{
	struct task_struct *t = NULL;
	u64 stime = 0, utime = 0, cutime = 0, cstime = 0;
	int ret = 0;
	u64 process_jiffies = 0;
	int priority = 0;
	const struct cred *cred = NULL;
	unsigned short pid_count = 0;

	memset(process_stat_buffer, 0, sizeof(struct mbraink_process_stat_data));
	process_stat_buffer->pid = 0;

	read_lock(&tasklist_lock);
	for_each_process(t) {
		if (t->pid < current_pid)
			continue;

		stime = utime = 0;
		cutime = t->signal->cutime;
		cstime = t->signal->cstime;

		if (t->mm)
			thread_group_cputime_adjusted(t, &utime, &stime);
		else
			task_cputime_adjusted(t, &utime, &stime);

		process_jiffies = nsec_to_clock_t(utime) +
				nsec_to_clock_t(stime) +
				nsec_to_clock_t(cutime) +
				nsec_to_clock_t(cstime);

		cred = get_task_cred(t);
		priority = t->prio - MAX_RT_PRIO;
		pid_count = process_stat_buffer->pid_count;

		if (pid_count < MAX_STRUCT_SZ) {
			process_stat_buffer->drv_data[pid_count].pid = (unsigned short)(t->pid);
			process_stat_buffer->drv_data[pid_count].uid = cred->uid.val;
			process_stat_buffer->drv_data[pid_count].process_jiffies = process_jiffies;
			process_stat_buffer->drv_data[pid_count].priority = priority;
			process_stat_buffer->pid_count++;
			put_cred(cred);
		} else {
			ret = -1;
			process_stat_buffer->pid = (unsigned short)(t->pid);
			put_cred(cred);
			break;
		}
	}

	pr_info("%s: current_pid = %u, count = %u\n",
		__func__, process_stat_buffer->pid, process_stat_buffer->pid_count);
	read_unlock(&tasklist_lock);
}

void mbraink_get_thread_stat_info(pid_t current_pid_idx, pid_t current_tid,
				struct mbraink_thread_stat_data *thread_stat_buffer)
{
	struct task_struct *t = NULL;
	struct task_struct *s = NULL;
	struct pid *parent_pid = NULL;
	u64 stime = 0, utime = 0, cutime = 0, cstime = 0;
	int ret = 0;
	u64 thread_jiffies = 0;
	int priority = 0;
	const struct cred *cred = NULL;
	int index = 0;
	unsigned long flags;
	int count = 0;
	unsigned short tid_count = 0;
	unsigned short processlist_temp[MAX_MONITOR_PROCESS_NUM];

	/*Check if there is a config to set montor process pid list*/
	spin_lock_irqsave(&monitor_pidlist_lock, flags);
	if (mbraink_monitor_pidlist_data.is_set == 0) {
		spin_unlock_irqrestore(&monitor_pidlist_lock, flags);
		pr_notice("the monitor pid list is unavailable now !!!\n");
		ret = -1;
		return;
	}

	count = mbraink_monitor_pidlist_data.monitor_process_count;
	for (index = 0; index < count; index++)
		processlist_temp[index] = mbraink_monitor_pidlist_data.monitor_pid[index];

	spin_unlock_irqrestore(&monitor_pidlist_lock, flags);

	read_lock(&tasklist_lock);
	memset(thread_stat_buffer, 0, sizeof(struct mbraink_thread_stat_data));
	thread_stat_buffer->tid = 0;
	thread_stat_buffer->tid_count = 0;

	for (index = current_pid_idx; index < count; index++) {
		parent_pid = find_get_pid(processlist_temp[index]);

		if (parent_pid == NULL) {
			pr_info("%s: parent_pid %u = NULL\n",
				__func__, processlist_temp[index]);
			continue;
		} else {
			t = get_pid_task(parent_pid, PIDTYPE_PID);
			if (t == NULL) {
				put_pid(parent_pid);
				pr_info("%s: task pid %u = NULL\n",
					__func__, processlist_temp[index]);
				continue;
			}
		}

		if (t && t->mm) {
			for_each_thread(t, s) {
				if (s->pid < current_tid)
					continue;

				stime = utime = 0;
				cutime = s->signal->cutime;
				cstime = s->signal->cstime;
				task_cputime_adjusted(s, &utime, &stime);
				/***********************************************
				 *cutime and cstime is to wait for child process
				 *or the exiting child process time, so we did
				 *not include it in thread jiffies
				 ***********************************************/
				thread_jiffies = nsec_to_clock_t(utime) + nsec_to_clock_t(stime);
				cred = get_task_cred(s);
				priority = s->prio - MAX_RT_PRIO;
				tid_count = thread_stat_buffer->tid_count;

				if (tid_count < MAX_STRUCT_SZ) {
					thread_stat_buffer->drv_data[tid_count].pid =
									processlist_temp[index];
					thread_stat_buffer->drv_data[tid_count].tid =
									(unsigned short)(s->pid);
					thread_stat_buffer->drv_data[tid_count].uid =
									cred->uid.val;
					thread_stat_buffer->drv_data[tid_count].thread_jiffies =
									thread_jiffies;
					thread_stat_buffer->drv_data[tid_count].priority =
									priority;
					/*******************************************************
					 *pr_info("tid_count=%u,  pid=%u, tid=%u, uid=%u,	\
					 * jiffies=%llu, priority=%d, name=%s\n",
					 * tid_count,
					 * thread_stat_buffer->drv_data[tid_count].pid,
					 * thread_stat_buffer->drv_data[tid_count].tid,
					 * thread_stat_buffer->drv_data[tid_count].uid,
					 * thread_stat_buffer->drv_data[tid_count].thread_jiffies,
					 * thread_stat_buffer->drv_data[tid_count].priority,
					 * s->comm);
					 ********************************************************/
					thread_stat_buffer->tid_count++;

					put_cred(cred);
				} else {
					ret = -1;
					thread_stat_buffer->tid = (unsigned short)(s->pid);
					thread_stat_buffer->pid_idx = index;
					put_cred(cred);
					break;
				}
			}
		} else {
			pr_info("This pid of task is kernel thread and has no children thread.\n");
		}
		put_task_struct(t);
		put_pid(parent_pid);

		if (ret == -1) {
			/*buffer is full this time*/
			break;
		}
		/*move to the next pid and reset the current tid*/
		current_tid = 1;
	}

	pr_info("%s: current_tid = %u, current_pid_idx = %u, count = %u\n",
			__func__, thread_stat_buffer->tid, thread_stat_buffer->pid_idx,
			thread_stat_buffer->tid_count);

	read_unlock(&tasklist_lock);
}

char *strcasestr(const char *s1, const char *s2)
{
	const char *s = s1;
	const char *p = s2;

	do {
		if (!*p)
			return (char *) s1;
		if ((*p == *s) || (tolower(*p) == tolower(*s))) {
			++p;
			++s;
		} else {
			p = s2;
			if (!*s)
				return NULL;
			s = ++s1;
		}
	} while (1);

	return *p ? NULL : (char *) s1;
}

void mbraink_processname_to_pid(unsigned short monitor_process_count,
				const struct mbraink_monitor_processlist *processname_inputlist)
{
	struct task_struct *t = NULL;
	char *cmdline = NULL;
	int index = 0;
	int count = 0;
	unsigned short processlist_temp[MAX_MONITOR_PROCESS_NUM];
	unsigned long flags;

	spin_lock_irqsave(&monitor_pidlist_lock, flags);
	mbraink_monitor_pidlist_data.is_set = 0;
	mbraink_monitor_pidlist_data.monitor_process_count = 0;
	spin_unlock_irqrestore(&monitor_pidlist_lock, flags);

	read_lock(&tasklist_lock);
	for_each_process(t) {
		if (t->mm) {
			if (count >= MAX_MONITOR_PROCESS_NUM)
				break;

			read_unlock(&tasklist_lock);
			/*This function might sleep*/
			cmdline = kstrdup_quotable_cmdline(t, GFP_KERNEL);
			read_lock(&tasklist_lock);

			if (!cmdline) {
				pr_info("cmdline is NULL\n");
				continue;
			}

			for (index = 0; index < monitor_process_count; index++) {
				if (strcasestr(cmdline,
					processname_inputlist->process_name[index])) {
					if (count < MAX_MONITOR_PROCESS_NUM) {
						processlist_temp[count] = (unsigned short)(t->pid);
						count++;
					}
					break;
				}
			}
			kfree(cmdline);
		}
	}
	read_unlock(&tasklist_lock);

	spin_lock_irqsave(&monitor_pidlist_lock, flags);
	mbraink_monitor_pidlist_data.monitor_process_count = count;
	for (index = 0; index < mbraink_monitor_pidlist_data.monitor_process_count; index++) {
		mbraink_monitor_pidlist_data.monitor_pid[index] = processlist_temp[index];
		pr_info("mbraink_monitor_pidlist_data.monitor_pid[%d] = %u, total count = %u\n",
			index, processlist_temp[index],
			mbraink_monitor_pidlist_data.monitor_process_count);
	}
	mbraink_monitor_pidlist_data.is_set = 1;
	spin_unlock_irqrestore(&monitor_pidlist_lock, flags);
}

void mbraink_show_process_info(void)
{
	struct task_struct *t = NULL;
	struct task_struct *s = NULL;
	struct mm_struct *mm = NULL;
	const struct cred *cred = NULL;
	int priority = 0;
	u64 stime = 0, utime = 0, cutime = 0, cstime = 0;
	char *cmdline = NULL;
	unsigned int counter = 0;

	read_lock(&tasklist_lock);
	for_each_process(t) {
		stime = utime = 0;
		mm = t->mm;
		counter++;
		if (mm) {
			read_unlock(&tasklist_lock);
			/*This function might sleep, cannot be called during atomic context*/
			cmdline = kstrdup_quotable_cmdline(t, GFP_KERNEL);
			read_lock(&tasklist_lock);

			cutime = t->signal->cutime;
			cstime = t->signal->cstime;
			thread_group_cputime_adjusted(t, &utime, &stime);
			cred = get_task_cred(t);
			priority = t->prio - MAX_RT_PRIO;

			if (cmdline) {
				pr_info(PROCESS_INFO_STR,
					t->pid, cred->uid.val, priority, nsec_to_clock_t(utime),
					nsec_to_clock_t(stime), nsec_to_clock_t(cutime),
					nsec_to_clock_t(cstime), cmdline);
				kfree(cmdline);
			} else {
				pr_info(PROCESS_INFO_STR,
					t->pid, cred->uid.val, priority, nsec_to_clock_t(utime),
					nsec_to_clock_t(stime), nsec_to_clock_t(cutime),
					nsec_to_clock_t(cstime), "NULL");
			}

			put_cred(cred);
			for_each_thread(t, s) {
				cred = get_task_cred(s);
				cutime = cstime = stime = utime = 0;
				cutime = s->signal->cutime;
				cstime = s->signal->cstime;
				task_cputime_adjusted(s, &utime, &stime);
				priority = s->prio - MAX_RT_PRIO;

				pr_info(THREAD_INFO_STR,
					s->pid, cred->uid.val, priority, nsec_to_clock_t(utime),
					nsec_to_clock_t(stime), nsec_to_clock_t(cutime),
					nsec_to_clock_t(cstime), s->comm);
				put_cred(cred);
			}
		} else {
			cred = get_task_cred(t);
			cutime = t->signal->cutime;
			cstime = t->signal->cstime;
			task_cputime_adjusted(t, &utime, &stime);
			priority = t->prio - MAX_RT_PRIO;
			pr_info(PROCESS_INFO_STR,
				t->pid, cred->uid.val, priority, nsec_to_clock_t(utime),
				nsec_to_clock_t(stime), nsec_to_clock_t(cutime),
				nsec_to_clock_t(cstime), t->comm);
			put_cred(cred);
		}
	}
	read_unlock(&tasklist_lock);
	pr_info("total task list element number = %u\n", counter);
}

#if IS_ENABLED(CONFIG_ANDROID_VENDOR_HOOKS)
/*****************************************************************
 * Note: this function can only be used during tracing function
 * This function is only used in tracing function so that there
 * is no need for task t spinlock protection
 *****************************************************************/
static u64 mbraink_get_specific_process_jiffies(struct task_struct *t)
{
	u64 stime = 0, utime = 0, cutime = 0, cstime = 0;
	u64 process_jiffies = 0;

	if (t->pid == t->tgid) {
		cutime = t->signal->cutime;
		cstime = t->signal->cstime;
		if (t->flags & PF_KTHREAD)
			task_cputime_adjusted(t, &utime, &stime);
		else
			thread_group_cputime_adjusted(t, &utime, &stime);

		process_jiffies = nsec_to_clock_t(utime) +
				nsec_to_clock_t(stime) +
				nsec_to_clock_t(cutime) +
				nsec_to_clock_t(cstime);
	} else {
		task_cputime_adjusted(t, &utime, &stime);
		process_jiffies = nsec_to_clock_t(utime) + nsec_to_clock_t(stime);
	}

	return process_jiffies;
}

/***************************************************************
 * Note: this function can only be used during tracing function
 * This function is only used in tracing function so that there
 * is no need for task t spinlock protection
 **************************************************************/
static u16 mbraink_get_specific_process_uid(struct task_struct *t)
{
	const struct cred *cred = NULL;
	u16 val = 0;

	cred = get_task_cred(t);
	val = cred->uid.val;
	put_cred(cred);

	return val;
}

static int is_monitor_process(unsigned short pid)
{
	int ret = 0, index = 0;
	unsigned short monitor_process_count = 0;
	unsigned long flags;

	spin_lock_irqsave(&monitor_pidlist_lock, flags);
	if (mbraink_monitor_pidlist_data.is_set == 0) {
		spin_unlock_irqrestore(&monitor_pidlist_lock, flags);
		return ret;
	}

	monitor_process_count = mbraink_monitor_pidlist_data.monitor_process_count;

	for (index = 0; index < monitor_process_count; index++) {
		if (mbraink_monitor_pidlist_data.monitor_pid[index] == pid) {
			ret = 1;
			break;
		}
	}

	spin_unlock_irqrestore(&monitor_pidlist_lock, flags);

	return ret;
}

static void mbraink_trace_android_vh_do_exit(void *data, struct task_struct *t)
{
	int i = 0;
	struct timespec64 tv = { 0 };
	unsigned long flags;

	if (t->pid == t->tgid || is_monitor_process((unsigned short)(t->tgid))) {
		spin_lock_irqsave(&tracing_pidlist_lock, flags);
		for (i = 0; i < MAX_TRACE_NUM; i++) {
			if (mbraink_tracing_pidlist_data[i].pid == (unsigned short)(t->pid)) {
				ktime_get_real_ts64(&tv);
				mbraink_tracing_pidlist_data[i].end =
					(tv.tv_sec*1000)+(tv.tv_nsec/1000000);
				mbraink_tracing_pidlist_data[i].jiffies =
						mbraink_get_specific_process_jiffies(t);
				mbraink_tracing_pidlist_data[i].dirty = true;
				/*************************************************************
				 * pr_info("pid=%s:%u, tgid=%u, pidlist[%d].start=%-10lld,	\
				 * pidlist[%d].end=%-10lld, pidlist[%d].jiffies=%llu\n",
				 * t->comm, t->pid, t->tgid, i,
				 * mbraink_tracing_pidlist_data[i].start, i,
				 * mbraink_tracing_pidlist_data[i].end, i,
				 * mbraink_tracing_pidlist_data[i].jiffies);
				 **************************************************************/
				break;
			}
		}
		if (i == MAX_TRACE_NUM) {
			for (i = 0; i < MAX_TRACE_NUM; i++) {
				if (mbraink_tracing_pidlist_data[i].pid == 0) {
					mbraink_tracing_pidlist_data[i].pid =
							(unsigned short)(t->pid);
					mbraink_tracing_pidlist_data[i].tgid =
							(unsigned short)(t->tgid);
					mbraink_tracing_pidlist_data[i].uid =
							mbraink_get_specific_process_uid(t);
					mbraink_tracing_pidlist_data[i].priority =
							t->prio - MAX_RT_PRIO;
					memcpy(mbraink_tracing_pidlist_data[i].name,
									t->comm, TASK_COMM_LEN);
					ktime_get_real_ts64(&tv);
					mbraink_tracing_pidlist_data[i].end =
							(tv.tv_sec*1000)+(tv.tv_nsec/1000000);
					mbraink_tracing_pidlist_data[i].jiffies =
							mbraink_get_specific_process_jiffies(t);
					mbraink_tracing_pidlist_data[i].dirty = true;
					/******************************************************
					 * pr_info("pid=%s:%u, tgid=%u pidlist[%d].	\
					 * start=%-10lld,pidlist[%d].end=%-10lld,	\
					 * pidlist[%d].jiffies=%llu\n",
					 * t->comm, t->pid, t->tgid, i,
					 * mbraink_tracing_pidlist_data[i].start, i,
					 * mbraink_tracing_pidlist_data[i].end, i,
					 * mbraink_tracing_pidlist_data[i].jiffies);
					 ********************************************************/
					break;
				}
			}
			if (i == MAX_TRACE_NUM)
				pr_info("tracing pid list is not enough, pid=%u:%s !!!\n",
					t->pid, t->comm);
		}
		spin_unlock_irqrestore(&tracing_pidlist_lock, flags);
	}
}

static void mbraink_trace_android_vh_do_fork(void *data, struct task_struct *p)
{
	int i = 0;
	struct timespec64 tv = { 0 };
	unsigned long flags;

	if (p->pid == p->tgid || is_monitor_process((unsigned short)(p->tgid))) {
		spin_lock_irqsave(&tracing_pidlist_lock, flags);
		for (i = 0; i < MAX_TRACE_NUM; i++) {
			if (mbraink_tracing_pidlist_data[i].pid == 0) {
				mbraink_tracing_pidlist_data[i].pid = (unsigned short)(p->pid);
				mbraink_tracing_pidlist_data[i].tgid = (unsigned short)(p->tgid);
				mbraink_tracing_pidlist_data[i].uid =
						mbraink_get_specific_process_uid(p);
				mbraink_tracing_pidlist_data[i].priority = p->prio - MAX_RT_PRIO;
				memcpy(mbraink_tracing_pidlist_data[i].name,
					p->comm, TASK_COMM_LEN);
				ktime_get_real_ts64(&tv);
				mbraink_tracing_pidlist_data[i].start =
						(tv.tv_sec*1000)+(tv.tv_nsec/1000000);
				mbraink_tracing_pidlist_data[i].dirty = true;
				break;
			}
		}
		spin_unlock_irqrestore(&tracing_pidlist_lock, flags);
		if (i == MAX_TRACE_NUM)
			pr_info("tracing pid list is not enough, child_pid=%u:%s !!!\n",
				p->pid, p->comm);
	}
}

int mbraink_process_tracer_init(void)
{
	int ret = 0;

	memset(mbraink_tracing_pidlist_data, 0,
			sizeof(struct mbraink_tracing_pidlist) * MAX_TRACE_NUM);

	ret = register_trace_android_vh_do_fork(mbraink_trace_android_vh_do_fork, NULL);
	if (ret) {
		pr_notice("register_trace_android_vh_do_fork failed.\n");
		goto register_trace_android_vh_do_fork;
	}
	ret = register_trace_android_vh_do_exit(mbraink_trace_android_vh_do_exit, NULL);
	if (ret) {
		pr_notice("register register_trace_android_vh_do_exit failed.\n");
		goto register_trace_android_vh_do_exit;
	}
	return ret;

register_trace_android_vh_do_exit:
	unregister_trace_android_vh_do_fork(mbraink_trace_android_vh_do_fork, NULL);
register_trace_android_vh_do_fork:
	return ret;
}

void mbraink_process_tracer_exit(void)
{
	unregister_trace_android_vh_do_fork(mbraink_trace_android_vh_do_fork, NULL);
	unregister_trace_android_vh_do_exit(mbraink_trace_android_vh_do_exit, NULL);
}

void mbraink_get_tracing_pid_info(unsigned short current_idx,
				struct mbraink_tracing_pid_data *tracing_pid_buffer)
{
	int i = 0;
	int ret = 0;
	unsigned long flags;
	unsigned short tracing_count = 0;

	spin_lock_irqsave(&tracing_pidlist_lock, flags);

	memset(tracing_pid_buffer, 0, sizeof(struct mbraink_tracing_pid_data));

	for (i = current_idx; i < MAX_TRACE_NUM; i++) {
		if (mbraink_tracing_pidlist_data[i].dirty == false)
			continue;
		else {
			tracing_count = tracing_pid_buffer->tracing_count;
			if (tracing_count < MAX_TRACE_PID_NUM) {
				tracing_pid_buffer->drv_data[tracing_count].pid =
						mbraink_tracing_pidlist_data[i].pid;
				tracing_pid_buffer->drv_data[tracing_count].tgid =
						mbraink_tracing_pidlist_data[i].tgid;
				tracing_pid_buffer->drv_data[tracing_count].uid =
						mbraink_tracing_pidlist_data[i].uid;
				tracing_pid_buffer->drv_data[tracing_count].priority =
						mbraink_tracing_pidlist_data[i].priority;
				memcpy(tracing_pid_buffer->drv_data[tracing_count].name,
					mbraink_tracing_pidlist_data[i].name, TASK_COMM_LEN);
				tracing_pid_buffer->drv_data[tracing_count].start =
						mbraink_tracing_pidlist_data[i].start;
				tracing_pid_buffer->drv_data[tracing_count].end =
						mbraink_tracing_pidlist_data[i].end;
				tracing_pid_buffer->drv_data[tracing_count].jiffies =
						mbraink_tracing_pidlist_data[i].jiffies;
				tracing_pid_buffer->tracing_count++;
				/*Deal with the end process record*/
				if (mbraink_tracing_pidlist_data[i].end != 0) {
					mbraink_tracing_pidlist_data[i].pid = 0;
					mbraink_tracing_pidlist_data[i].tgid = 0;
					mbraink_tracing_pidlist_data[i].uid = 0;
					mbraink_tracing_pidlist_data[i].priority = 0;
					memset(mbraink_tracing_pidlist_data[i].name,
						0, TASK_COMM_LEN);
					mbraink_tracing_pidlist_data[i].start = 0;
					mbraink_tracing_pidlist_data[i].end = 0;
					mbraink_tracing_pidlist_data[i].jiffies = 0;
					mbraink_tracing_pidlist_data[i].dirty = false;
				} else {
					mbraink_tracing_pidlist_data[i].dirty = false;
				}
			} else {
				ret = -1;
				tracing_pid_buffer->tracing_idx = i;
				break;
			}
		}
	}
	pr_info("%s: current_idx = %u, count = %u\n",
		__func__, tracing_pid_buffer->tracing_idx, tracing_pid_buffer->tracing_count);
	spin_unlock_irqrestore(&tracing_pidlist_lock, flags);
}
#else
int mbraink_process_tracer_init(void)
{
	pr_info("%s: Do not support mbraink tracing...\n", __func__);
	return 0;
}

void mbraink_process_tracer_exit(void)
{
	pr_info("%s: Do not support mbraink tracing...\n", __func__);
}

int mbraink_get_tracing_pid_info(unsigned short *current_idx,
				struct mbraink_tracing_pid_data *tracing_pid_buffer)
{
	pr_info("%s: Do not support mbraink tracing...\n", __func__);
	return 0;
}
#endif
