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

#include "usb_offload.h"
#include "mtk-usb-offload-ops.h"

/* mtk_sram_pwr - sram power manager
 * @type: sram type (ex reserved or allocated)
 * @cnt: how many region are allocated on
 */
struct mtk_sram_pwr {
	u32 type;
	atomic_t cnt;
};

static struct mtk_audio_usb_offload *aud_intf;
static struct mtk_sram_pwr sram_pwr[MEM_TYPE_NUM];
#define RESERVED_SRAM_TYPE	MEM_TYPE_SRAM_AFE

static struct usb_offload_mem_info usb_offload_mem_buffer[USB_OFFLOAD_MEM_NUM];
static struct usb_offload_buffer rsv_sram;
static DEFINE_MUTEX(rsv_sram_lock);

static int dump_mtk_usb_offload_gen_pool(void);
static int mtk_usb_offload_init_pool(int min_alloc_order, uint32_t mem_id);

/* a list to store buffer which downgrade from sram to dram */
LIST_HEAD(downgrade_list);

static bool is_buf_downgrade(struct usb_offload_buffer *buf)
{
	struct usb_offload_buffer *pos;
	bool found = false;

	list_for_each_entry(pos, &downgrade_list, list) {
		if (pos == buf) {
			found = true;
			break;
		}
	}

	return found;
}

bool mtk_offload_is_advlowpwr(struct usb_offload_dev *udev)
{
	/* if adv_lowpwr is false, it means that either sram feature is
	 * disabled in dts or basic sram is not supported in this platform.
	 */
	if (!udev->adv_lowpwr)
		return false;

	/* if list is empty, it means no structure falls to dram,
	 * so it's in advanced mode, in an other hands, it's basic
	 */
	return list_empty(&downgrade_list);
}
EXPORT_SYMBOL_GPL(mtk_offload_is_advlowpwr);

static void reset_mem_info(struct usb_offload_mem_info *mem_info)
{
	mem_info->phy_addr = 0;
	mem_info->va_addr = 0;
	mem_info->size = 0;
	mem_info->is_valid = false;
	mem_info->pool = NULL;
}

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

static char *memory_type(bool is_sram, u8 sub_type)
{
	char *type_name;

	if (!is_sram) {
		type_name = "adsp shared dram";
		return type_name;
	}

	switch (sub_type) {
	case MEM_TYPE_SRAM_AFE:
		type_name = "afe sram";
		break;
	case MEM_TYPE_SRAM_ADSP:
		type_name = "adsp L2 sram";
		break;
	case MEM_TYPE_SRAM_SLB:
		type_name = "slb";
		break;
	default:
		type_name = "unknown type";
		break;
	}
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
		*phys = 0;
		*size = 0;
		return -EOPNOTSUPP;
	}

	*phys = usb_offload_mem_buffer[mem_id].phy_addr;
	*size = usb_offload_mem_buffer[mem_id].size;

	return 0;
}
EXPORT_SYMBOL_GPL(mtk_offload_get_rsv_mem_info);

bool is_sram(enum usb_offload_mem_id id)
{
	return id == USB_OFFLOAD_MEM_SRAM_ID ? true : false;
}
EXPORT_SYMBOL_GPL(is_sram);

int soc_init_aud_intf(void)
{
	int ret = 0;
	u32 i;

	/* init audio interface from afe soc */
	aud_intf = mtk_audio_usb_offload_register_ops(uodev->dev);
	if (!aud_intf)
		return -EOPNOTSUPP;

	/* init sram power state */
	for (i = 0; i < MEM_TYPE_NUM; i++) {
		sram_pwr[i].type = i;
		atomic_set(&sram_pwr[i].cnt, 0);
	}

	return ret;
}
EXPORT_SYMBOL_GPL(soc_init_aud_intf);

static int soc_alloc_sram(struct usb_offload_buffer *buf, unsigned int size)
{
	int ret = 0;
	struct mtk_audio_usb_mem *audio_sram;

	if (!aud_intf || !aud_intf->ops->allocate_sram)
		return -EOPNOTSUPP;

	audio_sram = aud_intf->ops->allocate_sram(size);

	if (audio_sram && audio_sram->phys_addr) {
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

	ret = aud_intf->ops->free_sram(buf->dma_addr);
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

	/* decrease cnt prior to sram power if power=0 */
	if (!power) {
		if (atomic_read(&sram_pwr[sram_type].cnt))
			atomic_dec(&sram_pwr[sram_type].cnt);
		else {
			USB_OFFLOAD_ERR("%s's cnt is already 0\n",
				memory_type(true, sram_type));
			goto DONE_SRAM_PWR_CTRL;
		}
	}

	/* only control sram power if cnt=0 */
	if (!atomic_read(&sram_pwr[sram_type].cnt))
		sram_runtime_pm_ctrl(sram_type, power);

	/* increase cnt after setting sram power if power=1 */
	if (power)
		atomic_inc(&sram_pwr[sram_type].cnt);

DONE_SRAM_PWR_CTRL:
	USB_OFFLOAD_INFO("sram_type:%s cnt:%d\n",
		memory_type(true, sram_type),
		atomic_read(&sram_pwr[sram_type].cnt));
}

int mtk_offload_rsv_sram_pwr_ctrl(bool power)
{
	return sram_runtime_pm_ctrl(rsv_sram.type, power);
}

int mtk_offload_init_rsv_sram(int min_alloc_order)
{
	uint32_t mem_id;
	dma_addr_t phy_addr;
	unsigned int size = 16384;
	int ret = 0;

	mutex_lock(&rsv_sram_lock);
	USB_OFFLOAD_MEM_DBG("++\n");
	mem_id = USB_OFFLOAD_MEM_SRAM_ID;

	if (usb_offload_mem_buffer[mem_id].is_valid) {
		USB_OFFLOAD_MEM_DBG("rsv_sram is already inited\n");
		ret = 0;
		goto INIT_RSV_SRAM_DONE;
	}

	/* we use allocated sram to pretend resered sram */
	ret = soc_alloc_sram(&rsv_sram, size);
	if (ret) {
		USB_OFFLOAD_ERR("Fail to allcoate rsv_sram\n");
		usb_offload_mem_buffer[mem_id].is_valid = false;
		ret = -ENOMEM;
		goto INIT_RSV_SRAM_DONE;
	}

	sram_power_ctrl(rsv_sram.type, true);

	phy_addr = rsv_sram.dma_addr;
	usb_offload_mem_buffer[mem_id].phy_addr = (unsigned long long)phy_addr;
	usb_offload_mem_buffer[mem_id].va_addr = (unsigned long long) ioremap_wc(
			(phys_addr_t)phy_addr, (unsigned long)size);
	usb_offload_mem_buffer[mem_id].vir_addr = (unsigned char *)phy_addr;
	usb_offload_mem_buffer[mem_id].size = (unsigned long long)size;
	usb_offload_mem_buffer[mem_id].is_valid = true;
	USB_OFFLOAD_INFO("[reserved sram] phy:0x%llx vir:%p size:%llu\n",
		usb_offload_mem_buffer[mem_id].phy_addr,
		usb_offload_mem_buffer[mem_id].vir_addr,
		usb_offload_mem_buffer[mem_id].size);

	ret = mtk_usb_offload_init_pool(min_alloc_order, mem_id);
	if (!ret)
		dump_mtk_usb_offload_gen_pool();

INIT_RSV_SRAM_DONE:
	USB_OFFLOAD_MEM_DBG("--\n");
	mutex_unlock(&rsv_sram_lock);
	return ret;
}
EXPORT_SYMBOL_GPL(mtk_offload_init_rsv_sram);

int mtk_offload_deinit_rsv_sram(void)
{
	uint32_t mem_id = USB_OFFLOAD_MEM_SRAM_ID;
	int ret;

	mutex_lock(&rsv_sram_lock);
	USB_OFFLOAD_MEM_DBG("++\n");
	if (!usb_offload_mem_buffer[mem_id].is_valid) {
		USB_OFFLOAD_INFO("%s: Not support sram or it's already freed\n",
			__func__);
		ret = 0;
		goto DEINIT_RSV_SRAM_DONE;
	}

	USB_OFFLOAD_INFO("phy:0x%llx vir:%p size:%llu\n",
		usb_offload_mem_buffer[mem_id].phy_addr,
		usb_offload_mem_buffer[mem_id].vir_addr,
		usb_offload_mem_buffer[mem_id].size);

	ret = soc_free_sram(&rsv_sram);
	if (!ret) {
		sram_power_ctrl(rsv_sram.type, false);
		iounmap((void *) usb_offload_mem_buffer[mem_id].va_addr);
		reset_mem_info(&usb_offload_mem_buffer[mem_id]);
	}

DEINIT_RSV_SRAM_DONE:
	USB_OFFLOAD_MEM_DBG("--\n");
	mutex_unlock(&rsv_sram_lock);
	return ret;
}
EXPORT_SYMBOL_GPL(mtk_offload_deinit_rsv_sram);

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

static int mtk_usb_offload_init_pool(int min_alloc_order, uint32_t mem_id)
{
	struct gen_pool *pool;
	unsigned long va_start;
	size_t va_chunk;

	if (mem_id >= USB_OFFLOAD_MEM_NUM)
		return -EINVAL;

	if (!usb_offload_mem_buffer[mem_id].is_valid)
		return 0;

	pool = gen_pool_create(min_alloc_order, -1);
	if (!pool)
		return -ENOMEM;

	va_start = usb_offload_mem_buffer[mem_id].va_addr;
	va_chunk = usb_offload_mem_buffer[mem_id].size;
	if ((!va_start) || (!va_chunk))
		return -ENOMEM;

	if (gen_pool_add_virt(pool, (unsigned long)va_start,
		usb_offload_mem_buffer[mem_id].phy_addr, va_chunk, -1)) {
		USB_OFFLOAD_ERR("idx: %d failed, va_start: 0x%lx, va_chunk: %zu\n",
			mem_id, va_start, va_chunk);
	}
	usb_offload_mem_buffer[mem_id].pool = pool;
	USB_OFFLOAD_MEM_DBG("idx:%d success, va_start:0x%lx, va_chunk:%zu, pool[%d]:%p\n",
		mem_id, va_start, va_chunk, mem_id, pool);

	return 0;
}

int mtk_offload_init_rsv_dram(int min_alloc_order)
{
	uint32_t mem_id;
	int ret;

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

	ret = mtk_usb_offload_init_pool(min_alloc_order, mem_id);
	if (!ret)
		dump_mtk_usb_offload_gen_pool();
	return ret;
}
EXPORT_SYMBOL_GPL(mtk_offload_init_rsv_dram);

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

/* @mem_id: indicate that allocation is on sram or dram.
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
		ret = is_rsv ?
			mtk_usb_offload_alloc_rsv_mem(buf, size, align, mem_id) :
			soc_alloc_sram(buf, size);

		if (ret == 0) {
			if (!is_rsv)
				sram_power_ctrl(buf->type, true);
			goto ALLOC_SUCCESS;
		}
	}

	/* allocate on reserved dram*/
	ret = mtk_usb_offload_alloc_rsv_mem(
		buf, size, align, USB_OFFLOAD_MEM_DRAM_ID);
	if (ret)
		goto ALLOC_FAIL;

ALLOC_SUCCESS:
	if (is_sram(mem_id) && !buf->is_sram) {
		/* we requeset for sram, but turn out to be dram */
		USB_OFFLOAD_MEM_DBG("buf:%p falls from sram to dram\n", buf);
		list_add_tail(&buf->list, &downgrade_list);
	}

	USB_OFFLOAD_MEM_DBG("va:%p phy:0x%llx size:%zu is_sram:%d is_rsv:%d type:%s\n",
		buf->dma_area, (unsigned long long)buf->dma_addr,
		buf->dma_bytes, buf->is_sram, buf->is_rsv,
		memory_type(buf->is_sram, buf->type));
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

	if (!buf || !buf->allocated) {
		USB_OFFLOAD_MEM_DBG("buf:%p has already freed\n", buf);
		return 0;
	}

	USB_OFFLOAD_MEM_DBG("va:%p phy:0x%llx size:%zu is_sram:%d is_rsv:%d type:%s\n",
		buf->dma_area, (unsigned long long)buf->dma_addr,
		buf->dma_bytes, buf->is_sram, buf->is_rsv,
		memory_type(buf->is_sram, buf->type));

	type = buf->type;
	if (!buf->is_sram) {
		ret = mtk_usb_offload_free_rsv_mem(buf, USB_OFFLOAD_MEM_DRAM_ID);

		if (is_buf_downgrade(buf))
			list_del(&buf->list);

	}
	else if (uodev->adv_lowpwr) {
		if (buf->is_rsv)
			ret = mtk_usb_offload_free_rsv_mem(buf, USB_OFFLOAD_MEM_SRAM_ID);
		else {
			ret = soc_free_sram(buf);
			if (!ret)
				sram_power_ctrl(type, false);
		}
	}

	return ret;
}
EXPORT_SYMBOL_GPL(mtk_offload_free_mem);
