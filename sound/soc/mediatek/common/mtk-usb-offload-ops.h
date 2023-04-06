/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2023 MediaTek Inc.
 * Author: Yu-Syuan Cai <yusyuan.cai@mediatek.com>
 */

#ifndef _MTK_USB_OFFLOAD_OPS_H_
#define _MTK_USB_OFFLOAD_OPS_H_

#include <sound/soc.h>
#include <linux/notifier.h>

#define MAX_ALLOCATED_AFE_SRAM_COUNT 10

extern unsigned int audio_usb_offload_log;

#define AUDIO_USB_OFFLOAD_DBG(fmt, args...) do { \
	if (usb_offload_log > 1) \
		pr_info("[AUO]%s " fmt, __func__, ## args); \
	} while (0)

#define AUDIO_USB_OFFLOAD_INFO(fmt, args...) do { \
	if (1) \
		pr_info("[AUO]%s " fmt, __func__, ## args); \
	} while (0)

#define AUDIO_USB_OFFLOAD_ERR(fmt, args...) do { \
	if (1) \
		pr_info("[AUO]%s " fmt, __func__, ## args); \
	} while (0)

/* uaudio notify event */
enum AUDIO_USB_OFFLOAD_NOTIFY_EVENT {
	/* AEF SRAM */
	EVENT_AFE_SRAM_ALLOCATE = 0,
	EVENT_AFE_SRAM_FREE,
	EVENT_DSP_SRAM_ALLOCATE,
	EVENT_DSP_SRAM_FREE,
	/* PM */
	EVENT_PM_AFE_SRAM_ON,
	EVENT_PM_AFE_SRAM_OFF,
	EVENT_PM_DSP_SRAM_ON,
	EVENT_PM_DSP_SRAM_OFF,
	USB_OFFLOAD_EVENT_NUM,
};

enum mtk_audio_usb_offload_mem_type {
	MEM_TYPE_SRAM_AFE = 0,
	MEM_TYPE_SRAM_ADSP,
	MEM_TYPE_SRAM_SLB,
	MEM_TYPE_DRAM,
	MEM_TYPE_NUM,
};

struct mtk_audio_usb_mem {
	dma_addr_t phys_addr;
	void *virt_addr;
	enum mtk_audio_usb_offload_mem_type type;

	unsigned int size;
	bool use_dram_only;
	bool sram_inited;
};

struct mtk_audio_usb_offload;

struct mtk_audio_usb_offload_sram_ops {
	/* Reserved SRAM */
	int (*get_rsv_basic_sram)(struct mtk_audio_usb_offload *sram);
	/* AFE SRAM */
	struct mtk_audio_usb_mem *(*allocate_sram)(struct mtk_audio_usb_offload *sram,
						   unsigned int size);
	int (*free_sram)(struct mtk_audio_usb_offload *sram,
			 dma_addr_t addr);
	/* PM */
	int (*pm_runtime_control)(enum mtk_audio_usb_offload_mem_type type,
				   bool on);
};

struct mtk_audio_usb_offload {
	struct device *dev;

	struct mtk_audio_usb_mem rsv_basic_sram;
	struct mtk_audio_usb_mem rsv_urb_sram;

	struct mtk_audio_usb_mem afe_alloc_sram[MAX_ALLOCATED_AFE_SRAM_COUNT];
	const struct mtk_audio_usb_offload_sram_ops *ops;
};

extern const struct mtk_audio_usb_offload_sram_ops mtk_usb_offload_ops;

/* ops */
struct mtk_audio_usb_offload *mtk_audio_register_usb_offload_ops(struct device *dev);

/* Reserved SRAM */
int mtk_audio_usb_offload_get_basic_sram(struct mtk_audio_usb_offload *sram);
/* Allocate/Free SRAM */
struct mtk_audio_usb_mem *mtk_audio_usb_offload_allocate_sram(struct mtk_audio_usb_offload *sram,
							      unsigned int size);
int mtk_audio_usb_offload_free_sram(struct mtk_audio_usb_offload *sram,
				    dma_addr_t addr);
/* PM */
int mtk_audio_usb_offload_pm_runtime_control(enum mtk_audio_usb_offload_mem_type type,
					     bool on);

/* notify event for afe PM runtime suspend/resume */
extern int mtk_audio_usb_offload_register_notify(struct notifier_block *nb);
extern int mtk_audio_usb_offload_unregister_notify(struct notifier_block *nb);
#endif

