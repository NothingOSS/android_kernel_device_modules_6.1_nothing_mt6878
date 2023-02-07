/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef __APUSYS_APUMMU_MEM_H__
#define __APUSYS_APUMMU_MEM_H__
#include <linux/types.h>

#include "apummu_mem_def.h"

/* alloc DRAM fallback dram */
int apummu_dram_remap_alloc(void *drvinfo);
/* free the allocated DRAM fallback dram */
int apummu_dram_remap_free(void *drvinfo);


int apummu_dram_remap_runtime_alloc(void *drvinfo);
int apummu_dram_remap_runtime_free(void *drvinfo);

#endif
