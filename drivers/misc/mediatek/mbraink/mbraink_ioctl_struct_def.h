/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2022 MediaTek Inc.
 */
#ifndef MBRAINK_IOCTL_STRUCT_H
#define MBRAINK_IOCTL_STRUCT_H

#include <linux/kallsyms.h>

#define MAX_STRUCT_SZ				64
#define MAX_MEM_LIST_SZ				512
#define MAX_MONITOR_PROCESSNAME_SZ		64
#define MAX_MONITOR_PROCESS_NUM			16
#define MAX_DDR_FREQ_NUM			12
#define MAX_DDR_IP_NUM				8
#define MAX_TRACE_PID_NUM			32
#define MAX_VCORE_NUM				8
#define MAX_VCORE_IP_NUM			16
#define MAX_IP_NAME_LENGTH			(16)
#define MAX_NOTIFY_CPUFREQ_NUM			8
#define MAX_SUSPEND_INFO_SZ			128
#define MAX_FREQ_SZ				64
#define MAX_WAKEUP_SOURCE_NUM			12
#define MAX_NAME_SZ						64

#define NETLINK_EVENT_Q2QTIMEOUT		"NLEvent_Q2QTimeout"
#define NETLINK_EVENT_UDMFETCH			"M&"
#define NETLINK_EVENT_MESSAGE_SIZE		1024

#define MBRAINK_LANDING_FEATURE_CHECK 1

#define MBRAINK_PMU_INST_SPEC_EN	(1<<0UL)

#define MBRAINK_FEATURE_GPU_EN		(1<<0UL)
#define MBRAINK_FEATURE_AUDIO_EN	(1<<1UL)


#define MAX_POWER_SPM_TBL_SEC_SZ (928)

#define MAX_MD_TOTAL_SZ 200

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

struct mbraink_process_memory_data {
	unsigned short pid;
	unsigned short pid_count;
	unsigned short drv_data[MAX_MEM_LIST_SZ];
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
	uint64_t totalIPActiveTimeInMs[MAX_DDR_IP_NUM];
};

struct mbraink_memory_ddrInfo {
	struct mbraink_memory_ddrActiveInfo ddrActiveInfo[MAX_DDR_FREQ_NUM];
	int64_t srTimeInMs;
	int64_t pdTimeInMs;
	int32_t totalDdrFreqNum;
	int32_t totalDdrIpNum;
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
	int64_t active_time;
	int64_t idle_time;
	int64_t off_time;
};

struct mbraink_power_vcoreIpStats {
	char ip_name[MAX_IP_NAME_LENGTH];
	struct mbraink_power_vcoreIpDurationInfo times;
};

struct mbraink_power_vcoreInfo {
	struct mbraink_power_vcoreDurationInfo vcoreDurationInfo[MAX_VCORE_NUM];
	struct mbraink_power_vcoreIpStats vcoreIpDurationInfo[MAX_VCORE_IP_NUM];
	int32_t totalVCNum;
	int32_t totalVCIpNum;
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
	int reason;
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

struct mbraink_pmu_en {
	unsigned int pmu_en;
};

struct mbraink_pmu_info {
	unsigned long cpu0_pmu_data_inst_spec;
	unsigned long cpu1_pmu_data_inst_spec;
	unsigned long cpu2_pmu_data_inst_spec;
	unsigned long cpu3_pmu_data_inst_spec;
	unsigned long cpu4_pmu_data_inst_spec;
	unsigned long cpu5_pmu_data_inst_spec;
	unsigned long cpu6_pmu_data_inst_spec;
	unsigned long cpu7_pmu_data_inst_spec;
};

struct mbraink_power_spm_raw {
	uint8_t type;
	unsigned short pos;
	unsigned short size;
	unsigned char spm_data[MAX_POWER_SPM_TBL_SEC_SZ];
};

struct mbraink_modem_raw {
	unsigned char md_data[MAX_MD_TOTAL_SZ];
};


#endif
