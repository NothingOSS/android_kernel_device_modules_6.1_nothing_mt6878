// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2023 MediaTek Inc.
 *
 */

#include <asm/page.h>
#include <linux/dma-mapping.h>
#include <linux/err.h>
#include <linux/highmem.h>
#include <linux/mm.h>
#include <linux/scatterlist.h>
#include <linux/seq_file.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/mutex.h>
#include <linux/kthread.h>
#include <uapi/linux/sched/types.h>
#include <linux/dma-buf.h>
#include <linux/dma-heap.h>
#include <uapi/linux/dma-heap.h>

#include "mtk_heap.h"
#include "mtk_page_pool.h"

static int PREFIll_MAX_SIZE = SZ_512M;

struct prefill_data {
	const char	*heap_name;
	unsigned long	req_size;
};

struct task_struct *dma_pool_fill_kthread;

DECLARE_WAIT_QUEUE_HEAD(dma_pool_wq);
static struct prefill_data req_data;
static DEFINE_MUTEX(prefill_mutex);

static bool pending_prefill_req(void)
{
	bool req_pending;

	mutex_lock(&prefill_mutex);
	req_pending = req_data.heap_name != NULL && req_data.req_size > 0;
	mutex_unlock(&prefill_mutex);

	return req_pending;
}

/* get current prefill data and clear */
static void get_prefill_data(struct prefill_data *reqest_data)
{
	mutex_lock(&prefill_mutex);
	reqest_data->heap_name = req_data.heap_name;
	reqest_data->req_size = req_data.req_size;
	req_data.heap_name = NULL;
	req_data.req_size = 0;
	mutex_unlock(&prefill_mutex);
}

static int fill_dma_heap_pool(void *data)
{
	unsigned long req_cache_size = 0;
	unsigned long cached_size = 0;
	struct dma_buf *buffer = NULL;
	struct dma_heap *heap;
	struct prefill_data request_data;
	int ret = 0;

	while (1) {
		if (kthread_should_stop()) {
			pr_info("%s, stopping cache pool thread\n", __func__);
			break;
		}

		ret = wait_event_interruptible(dma_pool_wq,
					       pending_prefill_req());
		if (ret) {
			pr_info("%s, wait event error:%d\n", __func__, ret);
			continue;
		}

		get_prefill_data(&request_data);

		req_cache_size = request_data.req_size;
		if (request_data.heap_name == NULL || req_cache_size == 0)
			continue;

		heap = dma_heap_find(request_data.heap_name);
		if (!heap)
			return -1;

		if (req_cache_size > PREFIll_MAX_SIZE) {
			pr_info("%s, invalid buffer size %lu\n",
				__func__, req_cache_size);
			continue;
		}

		cached_size = mtk_dmabuf_page_pool_size(heap);
		if (cached_size >= req_cache_size) {
			pr_info("%s, skip alloc buffer: size %lu, cached %lu\n",
				__func__, req_cache_size, cached_size);
			continue;
		}

		pr_info("%s, alloc buffer(%lu) to heap(%s) pools\n", __func__, req_cache_size,
			request_data.heap_name);
		buffer = dma_heap_buffer_alloc(heap,
					       req_cache_size,
					       O_CLOEXEC | O_RDWR,
					       DMA_HEAP_VALID_HEAP_FLAGS);

		if (IS_ERR(buffer)) {
			pr_info("%s, err alloc buffer: size %lu, cached %lu\n",
				__func__, req_cache_size, cached_size);
			continue;
		}

		pr_info("%s, push buffer(%lu) to heap(%s) pools\n", __func__, req_cache_size,
			request_data.heap_name);
		dma_heap_buffer_free(buffer);
		buffer = NULL;

		pr_info("%s, alloc done\n", __func__);
	}

	dma_heap_put(heap);

	return 0;
}

int mtk_cache_pool_init(void)
{
	struct sched_param param = { .sched_priority = 0 };

	dma_pool_fill_kthread = kthread_run(fill_dma_heap_pool, NULL, "%s",
				       "dma_pool_fill_thread");
	if (IS_ERR(dma_pool_fill_kthread)) {
		pr_info("%s, creating thread fail\n", __func__);
		return PTR_ERR_OR_ZERO(dma_pool_fill_kthread);
	}

	sched_setscheduler(dma_pool_fill_kthread, SCHED_IDLE, &param);
	wake_up_process(dma_pool_fill_kthread);

	return 0;
}

void dma_heap_pool_prefill(unsigned long size, const char *heap_name)
{
	struct dma_heap *target_heap;

	if (!heap_name || size <= 0 || size > PREFIll_MAX_SIZE)
		return;

	// Only support normal heap.
	if (strncmp(heap_name, "system", strlen("system")) &&
	    strncmp(heap_name, "mtk_mm", strlen("mtk_mm")) &&
	    strncmp(heap_name, "mtk_camera", strlen("mtk_camera")))
		return;

	target_heap = dma_heap_find(heap_name);

	if (!target_heap)
		return;

	mutex_lock(&prefill_mutex);
	req_data.heap_name = dma_heap_get_name(target_heap);
	req_data.req_size = size;
	mutex_unlock(&prefill_mutex);

	pr_info("%s, request size %lu, heap:%s\n",
		__func__, size, dma_heap_get_name(target_heap));

	wake_up_interruptible(&dma_pool_wq);

	dma_heap_put(target_heap);
}
EXPORT_SYMBOL_GPL(dma_heap_pool_prefill);
