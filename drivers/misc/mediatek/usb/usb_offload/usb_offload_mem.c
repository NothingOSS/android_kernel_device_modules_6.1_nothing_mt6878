// SPDX-License-Identifier: GPL-2.0
/*
 * MTK USB Offload Memory Management API
 * *
 * Copyright (c) 2023 MediaTek Inc.
 * Author: Yu-chen.Liu <yu-chen.liu@mediatek.com>
 */

#include <linux/printk.h>
#include <linux/genalloc.h>

#if IS_ENABLED(CONFIG_MTK_AUDIODSP_SUPPORT)
#include <adsp_helper.h>
#endif

#ifdef MTK_AUDIO_INTERFACE_READY
#include "mtk-usb-offload-ops.h"
#endif
#include "usb_offload.h"

/* mtk_sram_pwr - sram power manager
 * @type: sram type (ex reserved or allocated)
 * @cnt: how many region are allocated on
 */
struct mtk_sram_pwr {
	u32 type;
	atomic_t cnt;
};

#ifdef MTK_AUDIO_INTERFACE_READY
static struct mtk_audio_usb_offload *aud_intf;
static struct mtk_sram_pwr sram_pwr[MEM_TYPE_NUM];
#define RESERVED_SRAM_TYPE	MEM_TYPE_SRAM_AFE
#else
#define RESERVED_SRAM_TYPE	1
#endif


struct usb_offload_mem_info usb_offload_mem_buffer[USB_OFFLOAD_MEM_NUM];



static void reset_buffer(struct usb_offload_buffer *buf)
{
	buf->dma_addr = 0;
	buf->dma_area = NULL;
	buf->dma_bytes = 0;
	buf->allocated = false;
	buf->is_sram = false;
	buf->is_rsv = false;
	buf->type = 0;
}

static char *memory_type(u8 buf_type)
{
	char *type_name;
#ifdef MTK_AUDIO_INTERFACE_READY
	switch (buf_type) {
	case MEM_TYPE_SRAM_AFE:
		type_name = "afe sram";
		break;
	case MEM_TYPE_SRAM_ADSP:
		type_name = "adsp L2 sram";
		break;
	case MEM_TYPE_SRAM_SLB:
		type_name = "slb";
		break;
	case MEM_TYPE_DRAM:
		type_name = "adsp shared dram";
		break;
	default:
		type_name = "unknown type";
		break;
	}
#else
	type_name = "adsp shared dram";
#endif
	return type_name;
}

int mtk_offload_get_rsv_mem_info(enum usb_offload_mem_id mem_id,
	unsigned int *phys, unsigned int *size)
{
	if (mem_id >= USB_OFFLOAD_MEM_NUM) {
		USB_OFFLOAD_ERR("Invalid id: %d\n", mem_id);
		return -EINVAL;
	}

	if (!usb_offload_mem_buffer[mem_id].is_valid) {
		USB_OFFLOAD_ERR("Not Support Reserved %s\n",
			is_sram(mem_id) ? "Sram" : "Dram");
		return -EOPNOTSUPP;
	}

	*phys = usb_offload_mem_buffer[mem_id].phy_addr;
	*size = usb_offload_mem_buffer[mem_id].size;

	return 0;
}
EXPORT_SYMBOL_GPL(mtk_offload_get_rsv_mem_info);

bool mtk_offload_is_sram_mode(void)
{
#ifdef MTK_AUDIO_INTERFACE_READY
	u32 i;

	for (i = 0; i < MEM_TYPE_NUM; i++) {
		if (atomic_read(&sram_pwr[i].cnt))
			return true;
	}
#endif
	return false;
}
EXPORT_SYMBOL_GPL(mtk_offload_is_sram_mode);

bool is_sram(enum usb_offload_mem_id id)
{
	return id == USB_OFFLOAD_MEM_SRAM_ID ? true : false;
}
EXPORT_SYMBOL_GPL(is_sram);

#ifdef MTK_AUDIO_INTERFACE_READY
static bool soc_init_aud_intf(void)
{
	int ret = 0;
	u32 i;

	/* init audio interface from afe soc */
	aud_intf = mtk_audio_register_usb_offload_ops(uodev->dev);
	if (!aud_intf)
		return -EOPNOTSUPP;

	/* init sram power state */
	for (i = 0; i < MEM_TYPE_NUM; i++) {
		sram_pwr[i].type = i;
		atomic_set(&sram_pwr[i].cnt, 0);
	}

	return ret;
}

static int soc_get_basic_sram(void)
{
	if (!aud_intf || !aud_intf->ops->get_rsv_basic_sram)
		return -EOPNOTSUPP;

	return aud_intf->ops->get_rsv_basic_sram(aud_intf);
}

static int soc_alloc_sram(struct usb_offload_buffer *buf, unsigned int size)
{
	int ret = 0;
	struct mtk_audio_usb_mem *audio_sram;

	if (!aud_intf || !aud_intf->ops->allocate_sram)
		return -EOPNOTSUPP;

	audio_sram = aud_intf->ops->allocate_sram(aud_intf, size);

	if (audio_sram->phys_addr) {
		buf->dma_addr = audio_sram->phys_addr;
		buf->dma_area = (unsigned char *)ioremap_wc(
			(phys_addr_t)audio_sram->phys_addr, (unsigned long)size);
		buf->dma_bytes = size;
		buf->allocated = true;
		buf->is_sram = true;
		buf->is_rsv = false;
		buf->type = audio_sram->type;
	} else {
		reset_buffer(buf);
		ret = -ENOMEM;
	}

	return ret;
}

static int soc_free_sram(struct usb_offload_buffer *buf)
{
	int ret = 0;

	if (!aud_intf || !aud_intf->ops->free_sram)
		return -EOPNOTSUPP;

	ret = aud_intf->ops->free_sram(aud_intf, buf->dma_addr);
	if (!ret)
		reset_buffer(buf);

	return ret;
}

static int sram_runtime_pm_ctrl(unsigned int sram_type, bool pwr_on)
{
	int ret;

	if (!aud_intf->ops || !aud_intf->ops->pm_runtime_control)
		return -EOPNOTSUPP;

	ret = aud_intf->ops->pm_runtime_control(sram_type, pwr_on);
	if (ret)
		USB_OFFLOAD_ERR("error controlling sram pwr, sram_type:%d pwr:%d\n",
			sram_type, pwr_on);
	return ret;
}

static void sram_power_ctrl(unsigned int sram_type, bool power)
{
	if (sram_type >= MEM_TYPE_NUM) {
		USB_OFFLOAD_ERR("wrong sram_type\n");
		return;
	}

	if (sram_runtime_pm_ctrl(sram_type, power))
		return;

	if (power)
		atomic_inc(&sram_pwr[sram_type].cnt);
	else
		atomic_dec(&sram_pwr[sram_type].cnt);

	USB_OFFLOAD_INFO("sram_type:%d cnt:%d\n",
		sram_type, atomic_read(&sram_pwr[sram_type].cnt));
}
#else
static bool soc_init_aud_intf(void)
{
	return false;
}
static int soc_get_basic_sram(void)
{
	return -EOPNOTSUPP;
}
static int soc_alloc_sram(struct usb_offload_buffer *buf, unsigned int size)
{
	return -EOPNOTSUPP;
}

static int soc_free_sram(struct usb_offload_buffer *buf)
{
	return -EOPNOTSUPP;
}

static void sram_power_ctrl(unsigned int sram_type, bool power)
{
}
#endif

static int dump_mtk_usb_offload_gen_pool(void)
{
	int i = 0;
	struct gen_pool *pool;

	for (i = 0; i < USB_OFFLOAD_MEM_NUM; i++) {
		if (!usb_offload_mem_buffer[i].is_valid)
			continue;
		pool = usb_offload_mem_buffer[i].pool;
		USB_OFFLOAD_INFO("gen_pool[%d]: avail: %zu, size: %zu\n",
				i, gen_pool_avail(pool), gen_pool_size(pool));
	}
	return 0;
}

static struct gen_pool *mtk_get_gen_pool(enum usb_offload_mem_id mem_id)
{
	if (mem_id < USB_OFFLOAD_MEM_NUM)
		return usb_offload_mem_buffer[mem_id].pool;
	USB_OFFLOAD_ERR("Invalid id: %d\n", mem_id);
	return NULL;
}

static bool is_rsv_mem_valid(enum usb_offload_mem_id mem_id)
{
	if (mem_id < USB_OFFLOAD_MEM_NUM) {
		USB_OFFLOAD_MEM_DBG("mem_id%d is %s\n", mem_id,
			usb_offload_mem_buffer[mem_id].is_valid ? "valid" : "invalid");
		return usb_offload_mem_buffer[mem_id].is_valid;
	}
	return false;
}

/* init reserved memory on both DRAM and SRAM
 * note that at least we should get dram region successfully
 */
int mtk_usb_offload_init_rsv_mem(int min_alloc_order, bool adv_lowpwr)
{
	int ret = 0, i;
	unsigned long va_start;
	size_t va_chunk;
	uint32_t mem_id;
#ifdef MTK_AUDIO_INTERFACE_READY
	dma_addr_t phy_addr;
	unsigned int size;
#endif

	/* get reserved dram region */
	mem_id = USB_OFFLOAD_MEM_DRAM_ID;
	if (!adsp_get_reserve_mem_phys(ADSP_XHCI_MEM_ID)) {
		USB_OFFLOAD_ERR("fail to get reserved dram\n");
		usb_offload_mem_buffer[mem_id].is_valid = false;
		return -EPROBE_DEFER;
	}
	usb_offload_mem_buffer[mem_id].phy_addr = adsp_get_reserve_mem_phys(ADSP_XHCI_MEM_ID);
	usb_offload_mem_buffer[mem_id].va_addr =
			(unsigned long long) adsp_get_reserve_mem_virt(ADSP_XHCI_MEM_ID);
	usb_offload_mem_buffer[mem_id].vir_addr = adsp_get_reserve_mem_virt(ADSP_XHCI_MEM_ID);
	usb_offload_mem_buffer[mem_id].size = adsp_get_reserve_mem_size(ADSP_XHCI_MEM_ID);
	usb_offload_mem_buffer[mem_id].is_valid = true;
	USB_OFFLOAD_INFO("[reserved dram] phy:0x%llx vir:%p\n",
		usb_offload_mem_buffer[mem_id].phy_addr, usb_offload_mem_buffer[mem_id].vir_addr);

	/* get reserved sram region */
	mem_id = USB_OFFLOAD_MEM_SRAM_ID;
	if (!adv_lowpwr || soc_init_aud_intf() || soc_get_basic_sram()) {
		usb_offload_mem_buffer[mem_id].is_valid = false;
		goto INIT_GEN_POOL;
	}
#ifdef MTK_AUDIO_INTERFACE_READY
	phy_addr = aud_intf->rsv_basic_sram.phys_addr;
	size = aud_intf->rsv_basic_sram.size;

	usb_offload_mem_buffer[mem_id].phy_addr = (unsigned long long)phy_addr;
	usb_offload_mem_buffer[mem_id].va_addr = (unsigned long long) ioremap_wc(
				(phys_addr_t)phy_addr, (unsigned long)size);
	usb_offload_mem_buffer[mem_id].vir_addr = (unsigned char *)phy_addr;
	usb_offload_mem_buffer[mem_id].size = (unsigned long long)size;
	usb_offload_mem_buffer[mem_id].is_valid = true;
	USB_OFFLOAD_INFO("[reserved sram] phy:0x%llx vir:%p\n",
		usb_offload_mem_buffer[mem_id].phy_addr, usb_offload_mem_buffer[mem_id].vir_addr);
#endif

INIT_GEN_POOL:
	/* init gen pool for both dram and sram*/
	if (min_alloc_order <= 0)
		return -ENOMEM;

	for (i = 0; i < USB_OFFLOAD_MEM_NUM; i++) {
		struct gen_pool *pool = usb_offload_mem_buffer[i].pool;

		if (!usb_offload_mem_buffer[i].is_valid)
			continue;

		pool = gen_pool_create(min_alloc_order, -1);

		if (!pool)
			return -ENOMEM;

		va_start = usb_offload_mem_buffer[i].va_addr;
		va_chunk = usb_offload_mem_buffer[i].size;
		if ((!va_start) || (!va_chunk)) {
			ret = -ENOMEM;
			break;
		}
		if (gen_pool_add_virt(pool, (unsigned long)va_start,
				usb_offload_mem_buffer[i].phy_addr, va_chunk, -1)) {
			USB_OFFLOAD_ERR("idx: %d failed, va_start: 0x%lx, va_chunk: %zu\n",
					i, va_start, va_chunk);
		}
		usb_offload_mem_buffer[i].pool = pool;

		USB_OFFLOAD_MEM_DBG("idx:%d success, va_start:0x%lx, va_chunk:%zu, pool[%d]:%p\n",
					i, va_start, va_chunk, i, pool);
	}
	dump_mtk_usb_offload_gen_pool();
	return ret;
}
EXPORT_SYMBOL_GPL(mtk_usb_offload_init_rsv_mem);

int mtk_usb_offload_deinit_rsv_sram(void)
{
	if (usb_offload_mem_buffer[USB_OFFLOAD_MEM_SRAM_ID].is_valid)
		iounmap((void *) usb_offload_mem_buffer[USB_OFFLOAD_MEM_SRAM_ID].va_addr);

	return 0;
}
EXPORT_SYMBOL_GPL(mtk_usb_offload_deinit_rsv_sram);

static int mtk_usb_offload_genpool_allocate_memory(unsigned char **vaddr,
	dma_addr_t *paddr, unsigned int size, enum usb_offload_mem_id mem_id, int align)
{
	/* gen pool related */
	struct gen_pool *gen_pool_usb_offload = mtk_get_gen_pool(mem_id);

	if (gen_pool_usb_offload == NULL) {
		USB_OFFLOAD_ERR("gen_pool_usb_offload == NULL\n");
		return -1;
	}

	/* allocate VA with gen pool */
	if (*vaddr == NULL) {
		*vaddr = (unsigned char *)gen_pool_dma_zalloc_align(gen_pool_usb_offload,
									size, paddr, align);
		*paddr = gen_pool_virt_to_phys(gen_pool_usb_offload,
					(unsigned long)*vaddr);
	}
	USB_OFFLOAD_MEM_DBG("size: %u, id: %d, vaddr: %p, DMA paddr: 0x%llx\n",
			size, mem_id, vaddr, (unsigned long long)*paddr);

	return 0;
}

static int mtk_usb_offload_genpool_free_memory(unsigned char **vaddr,
	size_t *size, enum usb_offload_mem_id mem_id)
{
	/* gen pool related */
	struct gen_pool *gen_pool_usb_offload = mtk_get_gen_pool(mem_id);

	if (gen_pool_usb_offload == NULL) {
		USB_OFFLOAD_ERR("gen_pool_usb_offload == NULL\n");
		return -1;
	}

	if (!gen_pool_has_addr(gen_pool_usb_offload, (unsigned long)*vaddr, *size)) {
		USB_OFFLOAD_ERR("vaddr is not in genpool\n");
		return -1;
	}

	/* allocate VA with gen pool */
	if (*vaddr) {
		USB_OFFLOAD_MEM_DBG("size: %zu, id: %d, vaddr: %p\n",
				*size, mem_id, vaddr);
		gen_pool_free(gen_pool_usb_offload, (unsigned long)*vaddr, *size);
		*vaddr = NULL;
		*size = 0;
	}

	return 0;
}

static int mtk_usb_offload_alloc_rsv_mem(struct usb_offload_buffer *buf,
	unsigned int size, int align, enum usb_offload_mem_id mem_id)
{
	int ret = 0;

	if (!is_rsv_mem_valid(mem_id))
		return -EBADRQC;

	if (buf->dma_area) {
		ret = mtk_usb_offload_genpool_free_memory(
					&buf->dma_area,
					&buf->dma_bytes,
					mem_id);
		if (ret)
			USB_OFFLOAD_ERR("Fail to free memoroy\n");
	}
	ret =  mtk_usb_offload_genpool_allocate_memory
				(&buf->dma_area,
				&buf->dma_addr,
				size,
				mem_id,
				align);
	if (!ret) {
		buf->dma_bytes = size;
		buf->allocated = true;
		buf->is_sram = is_sram(mem_id);
		buf->is_rsv = true;
		buf->type = buf->is_sram ? RESERVED_SRAM_TYPE : 0;
	} else
		reset_buffer(buf);

	return ret;
}

static int mtk_usb_offload_free_rsv_mem(struct usb_offload_buffer *buf,
	enum usb_offload_mem_id mem_id)
{
	int ret = 0;

	ret = mtk_usb_offload_genpool_free_memory(
				&buf->dma_area,
				&buf->dma_bytes,
				mem_id);

	if (!ret)
		reset_buffer(buf);

	return ret;
}

/* Allocated Memory
 * @mem_id: indicate that allocation is on sram or dram.
 * @is_rsv: indicate that allocation is on reserved or allocated.
 *
 * If either "reserved sram" or "allocated sram" is not enough or
 * not supported, allocating it on "reserved dram" instead.
 */

int mtk_offload_alloc_mem(struct usb_offload_buffer *buf,
	unsigned int size, int align,
	enum usb_offload_mem_id mem_id, bool is_rsv)
{
	int ret;

	if (!buf) {
		USB_OFFLOAD_ERR("buf:%p is NULL\n", buf);
		return -1;
	}

	USB_OFFLOAD_MEM_DBG("buf:%p size:%d align:%d is_sram:%d is_rsv:%d\n",
		buf, size, align, is_sram(mem_id), is_rsv);

	if (uodev->adv_lowpwr && mem_id == USB_OFFLOAD_MEM_SRAM_ID) {
		/* for reserved sram, powering on should be prior to allocating*/
		if (is_rsv)
			sram_power_ctrl(RESERVED_SRAM_TYPE, true);

		ret = is_rsv ?
			mtk_usb_offload_alloc_rsv_mem(buf, size, align, mem_id) :
			soc_alloc_sram(buf, size);

		if (ret == 0) {
			if (!is_rsv)
				sram_power_ctrl(buf->type, true);
			goto ALLOC_SUCCESS;
		}

		/* not able to get sram, decrease counter */
		if (is_rsv)
			sram_power_ctrl(RESERVED_SRAM_TYPE, false);
	}

	/* allocate on reserved dram*/
	ret = mtk_usb_offload_alloc_rsv_mem(
		buf, size, align, USB_OFFLOAD_MEM_DRAM_ID);
	if (ret)
		goto ALLOC_FAIL;

ALLOC_SUCCESS:
	USB_OFFLOAD_INFO("va:%p phy:0x%llx size:%zu is_sram:%d is_rsv:%d type:%s\n",
		buf->dma_area, (unsigned long long)buf->dma_addr,
		buf->dma_bytes, buf->is_sram, buf->is_rsv, memory_type(buf->type));
	return 0;
ALLOC_FAIL:
	USB_OFFLOAD_ERR("FAIL!!!!!!!!\n");
	return ret;
}

/* Free memory on both allocated and reserved memory
 *
 * Calling api based on buf->is_sram and buf->is_rsv.
 * User wouldn't need to concern memory type.
 */
int mtk_offload_free_mem(struct usb_offload_buffer *buf)
{
	int ret = 0;
	u8 type;
	bool is_sram;

	if (!buf || !buf->allocated) {
		USB_OFFLOAD_INFO("buf:%p has alreadt freed\n", buf);
		return 0;
	}

	USB_OFFLOAD_INFO("va:%p phy:0x%llx size:%zu is_sram:%d is_rsv:%d type:%s\n",
		buf->dma_area, (unsigned long long)buf->dma_addr,
		buf->dma_bytes, buf->is_sram, buf->is_rsv, memory_type(buf->type));

	type = buf->type;
	is_sram = buf->is_sram;

	if (!buf->is_sram)
		ret = mtk_usb_offload_free_rsv_mem(buf, USB_OFFLOAD_MEM_DRAM_ID);
	else if (uodev->adv_lowpwr) {
		if (buf->is_rsv)
			ret = mtk_usb_offload_free_rsv_mem(buf, USB_OFFLOAD_MEM_SRAM_ID);
		else
			ret = soc_free_sram(buf);
	}

	if (!ret && is_sram)
		sram_power_ctrl(type, false);

	return ret;
}
EXPORT_SYMBOL_GPL(mtk_offload_free_mem);
