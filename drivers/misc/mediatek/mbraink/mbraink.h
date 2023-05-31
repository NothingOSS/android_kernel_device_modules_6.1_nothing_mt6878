/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2022 MediaTek Inc.
 */

#ifndef MBRAINK_H
#define MBRAINK_H

#include <linux/ioctl.h>
#include <linux/cdev.h>
#include <net/sock.h>
#include <linux/pid.h>

#include "mbraink_ioctl_struct_def.h"

#define IOC_MAGIC	'k'
#define MBRAINK_NETLINK 30

#define MAX_BUF_SZ			1024

/*Mbrain Delegate Info List*/
#define POWER_INFO				'1'
#define VIDEO_INFO				'2'
#define POWER_SUSPEND_EN		'3'
#define PROCESS_MEMORY_INFO		'4'
#define PROCESS_STAT_INFO		'5'
#define THREAD_STAT_INFO		'6'
#define SET_MINITOR_PROCESS		'7'
#define MEMORY_DDR_INFO			'8'
#define IDLE_RATIO_INFO         '9'
#define TRACE_PROCESS_INFO      'a'
#define VCORE_INFO              'b'
#define CPUFREQ_NOTIFY_INFO		'c'
#define SUSPEND_INFO			'd'
#define BATTERY_INFO			'e'
#define FEATURE_EN				'f'
#define WAKEUP_INFO				'g'
#define POWER_SPM_RAW			'j'


/*Mbrain Delegate IOCTL List*/
#define RO_POWER				_IOR(IOC_MAGIC, POWER_INFO, char*)
#define RO_VIDEO				_IOR(IOC_MAGIC, VIDEO_INFO, char*)
#define WO_SUSPEND_POWER_EN		_IOW(IOC_MAGIC, POWER_SUSPEND_EN, char*)
#define RO_PROCESS_MEMORY		_IOR(IOC_MAGIC, PROCESS_MEMORY_INFO, \
							struct mbraink_process_memory_data*)
#define RO_PROCESS_STAT			_IOR(IOC_MAGIC, PROCESS_STAT_INFO, \
							struct mbraink_process_stat_data*)
#define RO_THREAD_STAT			_IOR(IOC_MAGIC, THREAD_STAT_INFO, \
							struct mbraink_thread_stat_data*)
#define WO_MONITOR_PROCESS		_IOW(IOC_MAGIC, SET_MINITOR_PROCESS, \
							struct mbraink_monitor_processlist*)
#define RO_MEMORY_DDR_INFO		_IOR(IOC_MAGIC, MEMORY_DDR_INFO, \
							struct mbraink_memory_ddrInfo*)
#define RO_IDLE_RATIO                  _IOR(IOC_MAGIC, IDLE_RATIO_INFO, \
							struct mbraink_audio_idleRatioInfo*)
#define RO_TRACE_PROCESS            _IOR(IOC_MAGIC, TRACE_PROCESS_INFO, \
							struct mbraink_tracing_pid_data*)
#define RO_VCORE_INFO                 _IOR(IOC_MAGIC, VCORE_INFO, \
							struct mbraink_power_vcoreInfo*)
#define RO_CPUFREQ_NOTIFY		_IOR(IOC_MAGIC, CPUFREQ_NOTIFY_INFO, \
							struct mbraink_cpufreq_notify_struct_data*)
#define RO_SUSPEND_INFO			_IOR(IOC_MAGIC, SUSPEND_INFO, \
							struct mbraink_suspend_info_struct_data*)
#define RO_BATTERY_INFO			_IOR(IOC_MAGIC, BATTERY_INFO, \
							struct mbraink_battery_data*)
#define WO_FEATURE_EN		_IOW(IOC_MAGIC, FEATURE_EN, \
							struct mbraink_feature_en*)
#define RO_WAKEUP_INFO			_IOR(IOC_MAGIC, WAKEUP_INFO, \
							struct mbraink_power_wakeup_data*)

#define RO_POWER_SPM_RAW			_IOR(IOC_MAGIC, POWER_SPM_RAW, \
								struct mbraink_power_spm_raw*)

#define SUSPEND_DATA	0
#define RESUME_DATA		1
#define CURRENT_DATA	2

struct mbraink_data {
#define CHRDEV_NAME     "mbraink_chrdev"
	struct cdev mbraink_cdev;
	char power_buffer[MAX_BUF_SZ * 3];
	char suspend_power_buffer[MAX_BUF_SZ];
	char resume_power_buffer[MAX_BUF_SZ];
	char suspend_power_info_en[2];
	int suspend_power_data_size;
	int resume_power_data_size;
	struct sock *mbraink_sock;
	int client_pid;
	unsigned int feature_en;
};

int mbraink_netlink_send_msg(const char *msg);

#endif /*end of MBRAINK_H*/
