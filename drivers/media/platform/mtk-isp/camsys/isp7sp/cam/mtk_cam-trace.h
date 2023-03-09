/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */
#undef TRACE_SYSTEM
#define TRACE_SYSTEM mtk_camsys

#if !defined(_MTK_CAM_TRACE_H) || defined(TRACE_HEADER_MULTI_READ)
#define _MTK_CAM_TRACE_H

#include <linux/tracepoint.h>
#include <linux/trace_events.h>

TRACE_EVENT(tracing_mark_write,
	TP_PROTO(const char *fmt, va_list *va),
	TP_ARGS(fmt, va),
	TP_STRUCT__entry(
		__vstring(vstr, fmt, va)
	),
	TP_fast_assign(
		__assign_vstr(vstr, fmt, va);
	),
	TP_printk("%s", __get_str(vstr))
);

TRACE_EVENT_CONDITION(raw_irq,
	TP_PROTO(struct device *dev,
		 unsigned int cookie,
		 unsigned int irq,
		 unsigned int dmao_done,
		 unsigned int dmai_done,
		 unsigned int cq_done,
		 unsigned int dcif_status
		),
	TP_ARGS(dev,
		cookie,
		irq,
		dmao_done,
		dmai_done,
		cq_done,
		dcif_status
	       ),
	TP_CONDITION(irq || dmao_done || dmai_done || cq_done || dcif_status),
	TP_STRUCT__entry(
		__string(device, dev_name(dev))
		__field(unsigned int, cookie)
		__field(unsigned int, irq)
		__field(unsigned int, dmao_done)
		__field(unsigned int, dmai_done)
		__field(unsigned int, cq_done)
		__field(unsigned int, dcif_status)
	),
	TP_fast_assign(
		__assign_str(device, dev_name(dev));
		__entry->cookie = cookie;
		__entry->irq = irq;
		__entry->dmao_done = dmao_done;
		__entry->dmai_done = dmai_done;
		__entry->cq_done = cq_done;
		__entry->dcif_status = dcif_status;
	),
	TP_printk("%s c=0x%x irq=0x%08x dmao=0x%08x dmai=0x%08x cq=0x%08x dcif=0x%08x %s",
		  __get_str(device),
		  __entry->cookie,
		  __entry->irq,
		  __entry->dmao_done,
		  __entry->dmai_done,
		  __entry->cq_done,
		  __entry->dcif_status,
		  __print_flags(__entry->irq & 0x21fe0c0, "|",
				{ BIT(6),	"TG_OVERRUN" },
				{ BIT(7),	"TG_GRABERR" },
				{ BIT(13),	"CQ_DB_LOAD_ERR" },
				{ BIT(14),	"MAX_START_SMALL" },
				{ BIT(15),	"MAX_START_DLY_ERR" },
				{ BIT(16),	"CQ_MAIN_CODE_ERR" },
				{ BIT(17),	"CQ_MAIN_VS_ERR" },
				{ BIT(18),	"CQ_MAIN_TRIG_DLY" },
				{ BIT(19),	"CQ_SUB_CODE_ERR" },
				{ BIT(20),	"CQ_SUB_VS_ERR" },
				{ BIT(25),	"DMA_ERR" })
	)
);

TRACE_EVENT_CONDITION(yuv_irq,
	TP_PROTO(struct device *dev,
		 unsigned int irq,
		 unsigned int dmao_done,
		 unsigned int dmai_done),
	TP_ARGS(dev,
		irq,
		dmao_done,
		dmai_done),
	TP_CONDITION(irq || dmao_done || dmai_done),
	TP_STRUCT__entry(
		__string(device, dev_name(dev))
		__field(unsigned int, irq)
		__field(unsigned int, dmao_done)
		__field(unsigned int, dmai_done)
	),
	TP_fast_assign(
		__assign_str(device, dev_name(dev));
		__entry->irq = irq;
		__entry->dmao_done = dmao_done;
		__entry->dmai_done = dmai_done;
	),
	TP_printk("%s irq=0x%08x dmao=0x%08x dmai=0x%08x %s",
		  __get_str(device),
		  __entry->irq,
		  __entry->dmao_done,
		  __entry->dmai_done,
		  __print_flags(__entry->irq & 0x4, "|",
				{ BIT(2),	"DMA_ERR" })
	)
);

TRACE_EVENT_CONDITION(raw_dma_status,
	TP_PROTO(struct device *dev,
		 unsigned int drop,
		 unsigned int overflow,
		 unsigned int underflow
		),
	TP_ARGS(dev,
		drop,
		overflow,
		underflow),
	TP_CONDITION(drop || overflow || underflow),
	TP_STRUCT__entry(
		__string(device, dev_name(dev))
		__field(unsigned int, drop)
		__field(unsigned int, overflow)
		__field(unsigned int, underflow)
	),
	TP_fast_assign(
		__assign_str(device, dev_name(dev));
		__entry->drop = drop;
		__entry->overflow = overflow;
		__entry->underflow = underflow;
	),
	TP_printk("%s drop=0x%08x overflow=0x%08x underflow=0x%08x",
		  __get_str(device),
		  __entry->drop,
		  __entry->overflow,
		  __entry->underflow
	)
);

TRACE_EVENT_CONDITION(raw_otf_overflow,
	TP_PROTO(struct device *dev, unsigned int otf_overflow),
	TP_ARGS(dev, otf_overflow),
	TP_CONDITION(otf_overflow),
	TP_STRUCT__entry(
		__string(device, dev_name(dev))
		__field(unsigned int, otf_overflow)
	),
	TP_fast_assign(
		__assign_str(device, dev_name(dev));
		__entry->otf_overflow = otf_overflow;
	),
	TP_printk("%s otf_overflow=0x%08x",
		  __get_str(device),
		  __entry->otf_overflow
	)
);

#endif /*_MTK_CAM_TRACE_H */

#undef TRACE_INCLUDE_PATH
#undef TRACE_INCLUDE_FILE
#define TRACE_INCLUDE_PATH .
#define TRACE_INCLUDE_FILE mtk_cam-trace
/* This part must be outside protection */
#include <trace/define_trace.h>


#ifndef __MTK_CAM_TRACE_H
#define __MTK_CAM_TRACE_H

#if IS_ENABLED(CONFIG_TRACING) && defined(MTK_CAM_TRACE_SUPPORT)

#include <linux/sched.h>
#include <linux/kernel.h>

int mtk_cam_trace_enabled_tags(void);

#define _MTK_CAM_TRACE_ENABLED(category)	\
	(mtk_cam_trace_enabled_tags() & (1UL << category))

__printf(1, 2)
void mtk_cam_trace(const char *fmt, ...);

#define _MTK_CAM_TRACE(category, fmt, args...)			\
do {								\
	if (unlikely(_MTK_CAM_TRACE_ENABLED(category)))		\
		mtk_cam_trace(fmt, ##args);			\
} while (0)

#else

#define _MTK_CAM_TRACE_ENABLED(category)	0
#define _MTK_CAM_TRACE(category, fmt, args...)

#endif

enum trace_category {
	TRACE_BASIC,
	TRACE_HW_IRQ,
	TRACE_BUFFER,
	TRACE_FBC,
};

#define _TRACE_CAT(cat)		TRACE_ ## cat

#define MTK_CAM_TRACE_ENABLED(category)	\
	_MTK_CAM_TRACE_ENABLED(_TRACE_CAT(category))

#define MTK_CAM_TRACE(category, fmt, args...)				\
	_MTK_CAM_TRACE(_TRACE_CAT(category), "camsys:" fmt, ##args)

/*
 * systrace format
 */

#define MTK_CAM_TRACE_BEGIN(category, fmt, args...)			\
	_MTK_CAM_TRACE(_TRACE_CAT(category), "B|%d|camsys:" fmt,	\
		      task_tgid_nr(current), ##args)

#define MTK_CAM_TRACE_END(category)					\
	_MTK_CAM_TRACE(_TRACE_CAT(category), "E|%d",			\
		      task_tgid_nr(current))

#define MTK_CAM_TRACE_FUNC_BEGIN(category)				\
	MTK_CAM_TRACE_BEGIN(category, "%s", __func__)

#endif /* __MTK_CAM_TRACE_H */
