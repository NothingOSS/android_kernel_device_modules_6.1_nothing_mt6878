/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef _FRAME_SYNC_LOG_H
#define _FRAME_SYNC_LOG_H


/******************************************************************************/
// log message concurrency lock
/******************************************************************************/
#ifndef FS_UT
#include <linux/spinlock.h>
extern spinlock_t fs_log_concurrency_lock;
#endif // FS_UT

/******************************************************************************/
// Special Debug Log Enable
/******************************************************************************/
#if defined(FS_UT)
#define TRACE_FS_FREC_LOG
#else
// #define TRACE_FS_FREC_LOG
#endif


/******************************************************************************/
// Log message
/******************************************************************************/
#define LOG_BUF_STR_LEN 1024

#ifdef FS_UT
#include <stdio.h>
#define LOG_INF(format, args...) printf(PFX "[%s] " format, __func__, ##args)
#define LOG_PF_INF(format, args...) printf(PFX "[%s] " format, __func__, ##args)
#define LOG_MUST(format, args...) printf(PFX "[%s] " format, __func__, ##args)
#define LOG_PR_WARN(format, args...) printf(PFX "[%s] " format, __func__, ##args)
#define LOG_PR_ERR(format, args...) printf(PFX "[%s] " format, __func__, ##args)

#else // FS_UT
#include <linux/printk.h>  /* for kernel log reduction */

#define LOG_TRACER_DEF 0
#define PF_LOG_TRACER_DEF 0

/* declare in frame_sync_console.c */
extern unsigned int log_tracer;
extern unsigned int pf_log_tracer;


#define DY_INFO(tracer, format, args...) \
do { \
	if (unlikely(tracer)) { \
		pr_info(PFX "[%s] " format, __func__, ##args); \
	} \
} while (0)

#define LOG_INF(format, args...) DY_INFO(log_tracer, format, args)
#define LOG_PF_INF(format, args...) DY_INFO(pf_log_tracer, format, args)
#define LOG_MUST(format, args...) pr_info(PFX "[%s] " format, __func__, ##args)


#define LOG_PR_WARN(format, args...) pr_warn(PFX "[%s] " format, __func__, ##args)
#define LOG_PR_ERR(format, args...) pr_err(PFX "[%s] " format, __func__, ##args)
#endif // FS_UT


#define FS_SNPRINTF(buf, len, fmt, ...) { \
	len += snprintf(buf + len, LOG_BUF_STR_LEN - len, fmt, ##__VA_ARGS__); \
}
/******************************************************************************/


#endif
