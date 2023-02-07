/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */


#ifndef __APUSYS_APUMMU_DRV_H__
#define __APUSYS_APUMMU_DRV_H__

#include <linux/ioctl.h>
#include <linux/types.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/wait.h>

/* for RV data*/
struct apummu_remote_data {
	uint64_t dram_IOVA_base;
	uint64_t dram[32];
	uint32_t dram_max;

	uint32_t vlm_size;
	uint32_t vlm_addr;
	bool is_dram_IOVA_alloc;
};

/* for plat data */
struct apummu_platform {
	uint32_t slb_wait_time;
	uint32_t boundary;
};

struct apummu_resource {
	unsigned int addr;
	unsigned int size;
	void *base;
};

struct apummu_resource_mgt {
	struct apummu_resource dram;
};

/* apummu driver's private structure */
struct apummu_dev_info {
	bool init_done;
	struct device *dev;
	dev_t apummu_devt;
	struct cdev apummu_cdev;
	struct rpmsg_device *rpdev;

	struct apummu_resource_mgt rsc;

	struct apummu_platform plat;
	struct apummu_remote_data remote;
};

#endif
