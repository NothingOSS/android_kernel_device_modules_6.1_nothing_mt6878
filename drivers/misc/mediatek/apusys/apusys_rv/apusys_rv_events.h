/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#undef TRACE_SYSTEM
#define TRACE_SYSTEM apusys_rv_events
#if !defined(__APUSYS_RV_EVENTS_H__) || defined(TRACE_HEADER_MULTI_READ)
#define __APUSYS_RV_EVENTS_H__
#include <linux/tracepoint.h>

#define APUSYS_RV_TAG_IPI_SEND_PRINT \
	"ipi_send:id=%d,len=%d,serial_no=%d,csum=0x%x,elapse=%llu"
#define APUSYS_RV_TAG_IPI_HANDLE_PRINT \
	"ipi_handle:id=%d,len=%d,serial_no=%d,csum=0x%x," \
	"top_start_time=%llu,bottom_start_time=%llu,latency=%llu,elapse=%llu"
#define APUSYS_RV_TAG_PWR_CTRL_PRINT \
	"pwr_ctrl:id=%d,on=%d,off=%d,latency=%llu"

TRACE_EVENT(apusys_rv_ipi_send,
	TP_PROTO(unsigned int id,
			unsigned int len,
			unsigned int serial_no,
			unsigned int csum,
			uint64_t elapse
		),
	TP_ARGS(id, len, serial_no, csum, elapse
		),
	TP_STRUCT__entry(
		__field(unsigned int, id)
		__field(unsigned int, len)
		__field(unsigned int, serial_no)
		__field(unsigned int, csum)
		__field(uint64_t, elapse)
	),
	TP_fast_assign(
		__entry->id = id;
		__entry->len = len;
		__entry->serial_no = serial_no;
		__entry->csum = csum;
		__entry->elapse = elapse;
	),
	TP_printk(
		APUSYS_RV_TAG_IPI_SEND_PRINT,
		__entry->id,
		__entry->len,
		__entry->serial_no,
		__entry->csum,
		__entry->elapse
	)
);

TRACE_EVENT(apusys_rv_ipi_handle,
	TP_PROTO(unsigned int id,
			unsigned int len,
			unsigned int serial_no,
			unsigned int csum,
			uint64_t top_start_time,
			uint64_t bottom_start_time,
			uint64_t latency,
			uint64_t elapse
		),
	TP_ARGS(id, len, serial_no, csum, top_start_time,
		bottom_start_time, latency, elapse
		),
	TP_STRUCT__entry(
		__field(unsigned int, id)
		__field(unsigned int, len)
		__field(unsigned int, serial_no)
		__field(unsigned int, csum)
		__field(uint64_t, top_start_time)
		__field(uint64_t, bottom_start_time)
		__field(uint64_t, latency)
		__field(uint64_t, elapse)
	),
	TP_fast_assign(
		__entry->id = id;
		__entry->len = len;
		__entry->serial_no = serial_no;
		__entry->csum = csum;
		__entry->top_start_time = top_start_time;
		__entry->bottom_start_time = bottom_start_time;
		__entry->latency = latency;
		__entry->elapse = elapse;
	),
	TP_printk(
		APUSYS_RV_TAG_IPI_HANDLE_PRINT,
		__entry->id,
		__entry->len,
		__entry->serial_no,
		__entry->csum,
		__entry->top_start_time,
		__entry->bottom_start_time,
		__entry->latency,
		__entry->elapse
	)
);

TRACE_EVENT(apusys_rv_pwr_ctrl,
	TP_PROTO(unsigned int id,
			unsigned int on,
			unsigned int off,
			uint64_t latency
		),
	TP_ARGS(id, on, off, latency
		),
	TP_STRUCT__entry(
		__field(unsigned int, id)
		__field(unsigned int, on)
		__field(unsigned int, off)
		__field(uint64_t, latency)
	),
	TP_fast_assign(
		__entry->id = id;
		__entry->on = on;
		__entry->off = off;
		__entry->latency = latency;
	),
	TP_printk(
		APUSYS_RV_TAG_PWR_CTRL_PRINT,
		__entry->id,
		__entry->on,
		__entry->off,
		__entry->latency
	)
);


#endif /* #if !defined(__APUSYS_RV_EVENTS_H__) || defined(TRACE_HEADER_MULTI_READ) */

/* This part must be outside protection */
#undef TRACE_INCLUDE_PATH
#define TRACE_INCLUDE_PATH .
#undef TRACE_INCLUDE_FILE
#define TRACE_INCLUDE_FILE apusys_rv_events
#include <trace/define_trace.h>
