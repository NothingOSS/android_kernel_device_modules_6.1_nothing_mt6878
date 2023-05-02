/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2022 MediaTek Inc.
 */
#ifndef MBRAINK_IOCTL_STRUCT_H
#define MBRAINK_IOCTL_STRUCT_H

#include <linux/kallsyms.h>

#define MAX_STRUCT_SZ				64
#define MAX_MEM_STRUCT_SZ			4
#define MAX_MONITOR_PROCESSNAME_SZ		64
#define MAX_MONITOR_PROCESS_NUM			16
#define MAX_DDR_FREQ_NUM			9
#define MAX_TRACE_PID_NUM			32
#define MAX_VCORE_NUM				5
#define MAX_IP_NAME_LENGTH			(16)
#define MAX_NOTIFY_CPUFREQ_NUM			8
#define MAX_SUSPEND_INFO_SZ			128
#define MAX_FREQ_SZ				64
#define MAX_WAKEUP_SOURCE_NUM			12
#define MAX_NAME_SZ						64

#define NETLINK_EVENT_Q2QTIMEOUT		"NLEvent_Q2QTimeout"
#define NETLINK_EVENT_UDMFETCH			"M&"
#define NETLINK_EVENT_MESSAGE_SIZE		1024

#define MBRAINK_LANDING_PONSOT_CHECK 1

#define MBRAINK_FEATURE_GPU_EN		(1<<0UL)
#define MBRAINK_FEATURE_AUDIO_EN	(1<<1UL)

enum MBRAINK_VCORE_IP {
	MBRAINK_VCORE_IP_MDP,
	MBRAINK_VCORE_IP_DISP,
	MBRAINK_VCORE_IP_VENC,
	MBRAINK_VCORE_IP_VDEC,
	MBRAINK_VCORE_IP_SCP,
	MBRAINK_VCORE_IP_MAX,
};

struct mbraink_process_stat_struct {
	unsigned short pid;
	unsigned short uid;
	int priority;
	u64 process_jiffies;
};

struct mbraink_process_stat_data {
	unsigned short pid;
	unsigned short pid_count;
	struct mbraink_process_stat_struct drv_data[MAX_STRUCT_SZ];
};

struct mbraink_process_memory_struct {
	unsigned short pid;
	unsigned long pss;
	unsigned long uss;
	unsigned long rss;
	unsigned long swap;
	unsigned long java_heap;
	unsigned long native_heap;
};

struct mbraink_process_memory_data {
	unsigned short pid;
	unsigned short pid_count;
	struct mbraink_process_memory_struct drv_data[MAX_MEM_STRUCT_SZ];
};

struct mbraink_thread_stat_struct {
	unsigned short pid;
	unsigned short tid;
	unsigned short uid;
	int priority;
	u64 thread_jiffies;
};

struct mbraink_thread_stat_data {
	unsigned short pid_idx;
	unsigned short tid;
	unsigned short tid_count;
	struct mbraink_thread_stat_struct drv_data[MAX_STRUCT_SZ];
};

struct mbraink_monitor_processlist {
	unsigned short monitor_process_count;
	char process_name[MAX_MONITOR_PROCESS_NUM][MAX_MONITOR_PROCESSNAME_SZ];
};

struct mbraink_memory_ddrActiveInfo {
	int32_t freqInMhz;
	int64_t totalActiveTimeInMs;
	uint64_t totalReadActiveTimeInMs;
	uint64_t totalWriteActiveTimeInMs;
	uint64_t totalCpuActiveTimeInMs;
	uint64_t totalGpuActiveTimeInMs;
	uint64_t totalMmActiveTimeInMs;
	uint64_t totalMdActiveTimeInMs;
};

struct mbraink_memory_ddrInfo {
	struct mbraink_memory_ddrActiveInfo ddrActiveInfo[MAX_DDR_FREQ_NUM];
	int64_t srTimeInMs;
	int64_t pdTimeInMs;
	int32_t totalDdrFreqNum;
};

struct mbraink_audio_idleRatioInfo {
	int64_t timestamp;
	int64_t s0_time;
	int64_t s1_time;
	int64_t mcusys_active_time;
	int64_t mcusys_pd_time;
	int64_t cluster_active_time;
	int64_t cluster_idle_time;
	int64_t cluster_pd_time;
	int64_t adsp_active_time;
	int64_t adsp_wfi_time;
	int64_t adsp_pd_time;
	int64_t audio_hw_time;
};

struct mbraink_tracing_pid {
	unsigned short pid;
	unsigned short tgid;
	unsigned short uid;
	int priority;
	char name[TASK_COMM_LEN];
	long long start;
	long long end;
	u64 jiffies;
};

struct mbraink_tracing_pid_data {
	unsigned short tracing_idx;
	unsigned short tracing_count;
	struct mbraink_tracing_pid drv_data[MAX_TRACE_PID_NUM];
};

struct mbraink_power_vcoreDurationInfo {
	int32_t vol;
	int64_t duration;
};

struct mbraink_power_vcoreIpDurationInfo {
	int32_t vol;
	int64_t active_time;
	int64_t idle_time;
	int64_t off_time;
};

struct mbraink_power_vcoreIpStats {
	char ip_name[MAX_IP_NAME_LENGTH];
	struct mbraink_power_vcoreIpDurationInfo vol_times[MAX_VCORE_NUM];
};

struct mbraink_power_vcoreInfo {
	struct mbraink_power_vcoreDurationInfo vcoreDurationInfo[MAX_VCORE_NUM];
	struct mbraink_power_vcoreIpStats vcoreIpDurationInfo[MBRAINK_VCORE_IP_MAX];
};

struct mbraink_cpufreq_notify_struct {
	long long timestamp;
	int cid;
	unsigned short qos_type;
	unsigned int freq_limit;
	char caller[MAX_FREQ_SZ];
};

struct mbraink_cpufreq_notify_struct_data {
	unsigned short notify_cluster_idx;
	unsigned short notify_idx;
	unsigned short notify_count;
	struct mbraink_cpufreq_notify_struct drv_data[MAX_NOTIFY_CPUFREQ_NUM];
};

struct mbraink_suspend_info_struct {
	unsigned short datatype;
	long long timestamp;
};

struct mbraink_suspend_info_struct_data {
	unsigned short count;
	bool is_continue;
	struct mbraink_suspend_info_struct drv_data[MAX_SUSPEND_INFO_SZ];
};

struct mbraink_battery_data {
	int quse;
	int qmaxt;
	int precise_soc;
	int precise_uisoc;
};

struct mbraink_feature_en {
	unsigned int feature_en;
};

struct mbraink_power_wakeup_struct {
	char name[MAX_NAME_SZ];
	unsigned long  active_count;
	unsigned long event_count;
	unsigned long wakeup_count;
	unsigned long expire_count;
	s64 active_time;
	s64 total_time;
	s64 max_time;
	s64 last_time;
	s64 prevent_sleep_time;
};

struct mbraink_power_wakeup_data {
	uint8_t is_has_data;
	unsigned short next_pos;
	struct mbraink_power_wakeup_struct drv_data[MAX_WAKEUP_SOURCE_NUM];
};


#endif
