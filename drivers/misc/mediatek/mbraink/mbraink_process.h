/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2022 MediaTek Inc.
 */
#ifndef MBRAINK_PROCESS_H
#define MBRAINK_PROCESS_H
#include <linux/string_helpers.h>
#include <linux/sched.h>
#include <linux/sched/task.h>
#include <linux/mm_types.h>
#include <linux/pid.h>

#include "mbraink_ioctl_struct_def.h"

#define PSS_SHIFT			12
#define MAX_RT_PRIO			100
#define MAX_TRACE_NUM			3072

struct mbraink_monitor_pidlist {
	unsigned short is_set;
	unsigned short monitor_process_count;
	unsigned short monitor_pid[MAX_MONITOR_PROCESS_NUM];
};

struct mbraink_tracing_pidlist {
	unsigned short pid;
	unsigned short tgid;
	unsigned short uid;
	int priority;
	char name[TASK_COMM_LEN];
	long long start;
	long long end;
	u64 jiffies;
	bool dirty;
};

struct mem_size_stats {
	unsigned long resident;
	unsigned long shared_clean;
	unsigned long shared_dirty;
	unsigned long private_clean;
	unsigned long private_dirty;
	unsigned long referenced;
	unsigned long anonymous;
	unsigned long lazyfree;
	unsigned long anonymous_thp;
	unsigned long shmem_thp;
	unsigned long file_thp;
	unsigned long swap;
	unsigned long shared_hugetlb;
	unsigned long private_hugetlb;
	u64 pss;
	u64 pss_anon;
	u64 pss_file;
	u64 pss_shmem;
	u64 pss_locked;
	u64 swap_pss;
	bool check_shmem_swap;
};

void mbraink_show_process_info(void);
void mbraink_get_process_stat_info(pid_t *current_pid,
			struct mbraink_process_stat_data *process_stat_buffer);
void mbraink_get_thread_stat_info(pid_t *current_pid_idx, pid_t *current_tid,
			struct mbraink_thread_stat_data *thread_stat_buffer);
void mbraink_processname_to_pid(struct mbraink_monitor_processlist *processname_inputlist);
void mbraink_get_process_memory_info(pid_t *current_pid,
			struct mbraink_process_memory_data *process_memory_buffer);
int mbraink_process_tracer_init(void);
void mbraink_process_tracer_exit(void);
void mbraink_get_tracing_pid_info(unsigned short *current_idx,
			struct mbraink_tracing_pid_data *tracing_pid_buffer);
void smap_gather_stats(struct vm_area_struct *vma,
			struct mem_size_stats *mss, unsigned long start);
struct vm_area_struct *find_vma(struct mm_struct *mm, unsigned long vma_index);
struct vm_area_struct *get_gate_vma(struct mm_struct *mm);
char *kstrdup_quotable_cmdline(struct task_struct *task, gfp_t gfp);
void task_cputime_adjusted(struct task_struct *p, u64 *ut, u64 *st);
void thread_group_cputime_adjusted(struct task_struct *p, u64 *ut, u64 *st);
u64 nsec_to_clock_t(u64 x);
#endif
