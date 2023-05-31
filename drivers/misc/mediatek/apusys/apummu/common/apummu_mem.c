// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#include <linux/dma-mapping.h>

#include "apummu_cmn.h"
#include "apummu_mem.h"
#include "apummu_import.h"


static struct apummu_mem *g_mem_sys;
static uint32_t general_SLB_attempt_cnt;

void apummu_mem_free(struct device *dev, struct apummu_mem *mem)
{
	dma_free_coherent(dev, mem->size, (void *)mem->kva, mem->iova);
}

int apummu_mem_alloc(struct device *dev, struct apummu_mem *mem)
{
	int ret = 0;
	void *kva;
	dma_addr_t iova = 0;

	/* TODO: using other API */
	kva = dma_alloc_coherent(dev, mem->size, &iova, GFP_KERNEL);
	if (!kva) {
		AMMU_LOG_ERR("dma_alloc_coherent fail (0x%x)\n", mem->size);
		ret = -ENOMEM;
		goto out;
	}

#ifndef MODULE
	/*
	 * Avoid a kmemleak false positive.
	 * The pointer is using for debugging,
	 * but it will be used by other apusys HW
	 */
	kmemleak_no_scan(kva);
#endif
	mem->kva = (uint64_t)kva;
	mem->iova = (uint64_t)iova;

	AMMU_LOG_INFO("DRAM alloc mem(0x%llx/0x%x/0x%llx)\n",
			mem->iova, mem->size, mem->kva);

out:
	return ret;
}

#if !(DRAM_FALL_BACK_IN_RUNTIME)
int apummu_dram_remap_alloc(void *drvinfo)
{
	struct apummu_dev_info *adv = NULL;
	unsigned int i = 0;
	int ret = 0;

	if (drvinfo == NULL) {
		AMMU_LOG_ERR("invalid argument\n");
		ret = -EINVAL;
		goto out;
	}
	adv = (struct apummu_dev_info *)drvinfo;

	g_mem_sys.size = (uint64_t) adv->remote.vlm_size * adv->remote.dram_max;
	ret = apummu_mem_alloc(adv->dev, &g_mem_sys);
	if (ret) {
		AMMU_LOG_ERR("DRAM FB mem alloc fail\n");
		goto out;
	}

	adv->rsc.vlm_dram.base = (void *) g_mem_sys.kva;
	adv->rsc.vlm_dram.size = g_mem_sys.size;
	for (i = 0; i < adv->remote.dram_max; i++)
		adv->remote.dram[i] = g_mem_sys.iova + adv->remote.vlm_size * (uint64_t) i;

out:
	return ret;
}

int apummu_dram_remap_free(void *drvinfo)
{
	struct apummu_dev_info *adv = NULL;

	if (drvinfo == NULL) {
		AMMU_LOG_ERR("invalid argument\n");
		return -EINVAL;
	}
	adv = (struct apummu_dev_info *)drvinfo;

	apummu_mem_free(adv->dev, &g_mem_sys);
	adv->rsc.vlm_dram.base = NULL;
	return 0;
}
#else
int apummu_dram_remap_runtime_alloc(void *drvinfo)
{
	struct apummu_dev_info *adv = NULL;
	int i, j, ret = 0;
	void *vlm_dram_base = NULL;

	if (drvinfo == NULL) {
		AMMU_LOG_ERR("invalid argument\n");
		ret = -EINVAL;
		goto out;
	}
	adv = (struct apummu_dev_info *)drvinfo;

	if (adv->remote.is_dram_IOVA_alloc) {
		AMMU_LOG_ERR("Error DRAM FB already alloc\n");
		ret = -EINVAL;
		goto out;
	}

	vlm_dram_base = kvzalloc(
		sizeof(struct apummu_resource) * adv->remote.dram_max,
		GFP_KERNEL);
	if (!vlm_dram_base) {
		AMMU_LOG_ERR("vlm_dram_base alloc fail\n");
		ret = -ENOMEM;
		goto out;
	}

	g_mem_sys = kvzalloc(
		sizeof(struct apummu_mem) * adv->remote.dram_max,
		GFP_KERNEL);
	if (!g_mem_sys) {
		AMMU_LOG_ERR("g_mem_sys alloc fail\n");
		ret = -ENOMEM;
		goto free_vlm;
	}

	adv->rsc.vlm_dram = vlm_dram_base;

	// g_mem_sys.size = (uint64_t) adv->remote.vlm_size * adv->remote.dram_max;
	for (i = 0; i < adv->remote.dram_max; i++) {
		g_mem_sys[i].size = adv->remote.vlm_size;
		ret = apummu_mem_alloc(adv->dev, &g_mem_sys[i]);

		if (ret) {
			for (j = 0; j <= i; j++)
				apummu_mem_free(adv->dev, &g_mem_sys[j]);

			AMMU_LOG_ERR("DRAM FB mem alloc fail\n");
			goto free_mem;
		} else {
			adv->rsc.vlm_dram[i].base = (void *) g_mem_sys[i].kva;
			adv->rsc.vlm_dram[i].size = g_mem_sys[i].size;
			adv->rsc.vlm_dram[i].iova = g_mem_sys[i].iova;
		}
	}

	adv->remote.is_dram_IOVA_alloc = true;

out:
	return ret;
free_mem:
	kvfree(g_mem_sys);
free_vlm:
	kvfree(vlm_dram_base);
	return ret;
}

int apummu_dram_remap_runtime_free(void *drvinfo)
{
	int i, ret = 0;

	struct apummu_dev_info *adv = NULL;

	if (drvinfo == NULL) {
		AMMU_LOG_ERR("invalid argument\n");
		ret = -EINVAL;
		goto out;
	}
	adv = (struct apummu_dev_info *)drvinfo;

	for (i = 0; i < adv->remote.dram_max; i++)
		apummu_mem_free(adv->dev, &g_mem_sys[i]);
	kvfree(adv->rsc.vlm_dram);
	adv->remote.is_dram_IOVA_alloc = false;

out:
	return ret;
}
#endif

int apummu_alloc_general_SLB(void *drvinfo)
{
	struct apummu_dev_info *adv = NULL;
	uint64_t ret_addr, ret_size;
	uint32_t size = 0;
	int ret = 0;

	if (drvinfo == NULL) {
		AMMU_LOG_ERR("invalid argument\n");
		ret = -EINVAL;
		goto out;
	}
	adv = (struct apummu_dev_info *)drvinfo;

	if (adv->remote.is_general_SLB_alloc) {
		AMMU_LOG_ERR("general SLB already added\n");
		ret = -EINVAL;
		goto out;
	}

	if (!(adv->plat.is_general_SLB_support)) {
		AMMU_LOG_INFO("No General SLB Support\n");
		goto out;
	}

	ret = apummu_alloc_slb(APUMMU_MEM_TYPE_GENERAL_S, size, adv->plat.slb_wait_time,
					&ret_addr, &ret_size);
	if (ret) {
		AMMU_LOG_WRN("general SLB alloc fail...\n");
		general_SLB_attempt_cnt += 1;
		goto out;
	}

	adv->remote.is_general_SLB_alloc = true;
	adv->rsc.genernal_SLB.iova = ret_addr;
	adv->rsc.genernal_SLB.size = (uint32_t) ret_size;

	AMMU_LOG_INFO("General SLB alloced after %u times. (addr, size) = (0x%llx, 0x%llx)\n",
		general_SLB_attempt_cnt, ret_addr, ret_size);
	general_SLB_attempt_cnt = 0;

out:
	return ret;
}

int apummu_free_general_SLB(void *drvinfo)
{
	struct apummu_dev_info *adv = NULL;
	int ret = 0;

	if (drvinfo == NULL) {
		AMMU_LOG_ERR("invalid argument\n");
		ret = -EINVAL;
		goto out;
	}
	adv = (struct apummu_dev_info *)drvinfo;

	if (!adv->remote.is_general_SLB_alloc) {
		AMMU_LOG_ERR("No general SLB is alloced\n");
		ret = -EINVAL;
		goto out;
	}

	ret = apummu_free_slb(APUMMU_MEM_TYPE_GENERAL_S);
	if (ret) {
		AMMU_LOG_WRN("general SLB free fail...\n");
		goto out;
	}

	adv->remote.is_general_SLB_alloc = false;
	adv->rsc.genernal_SLB.iova = 0;
	adv->rsc.genernal_SLB.size = 0;

	AMMU_LOG_VERBO("General SLB freeed\n");

out:
	return ret;
}

void apummu_mem_init(void)
{
	general_SLB_attempt_cnt = 0;
}
