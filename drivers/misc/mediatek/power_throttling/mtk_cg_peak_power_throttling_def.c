// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2023 MediaTek Inc.
 * Author: Clouds Lee <clouds.lee@mediatek.com>
 */

#include "mtk_cg_peak_power_throttling_def.h"

/*
 * ========================================================
 * Kernel
 * ========================================================
 */
#if defined(__KERNEL__)

uintptr_t THERMAL_CSRAM_BASE_REMAP;
uintptr_t THERMAL_CSRAM_CTRL_BASE_REMAP;
uintptr_t DLPT_CSRAM_BASE_REMAP;
uintptr_t DLPT_CSRAM_CTRL_BASE_REMAP;

void cg_ppt_thermal_sram_remap(uintptr_t virtual_addr)
{
	THERMAL_CSRAM_BASE_REMAP = virtual_addr;
	THERMAL_CSRAM_CTRL_BASE_REMAP = THERMAL_CSRAM_BASE_REMAP + 0x360;
}

void cg_ppt_dlpt_sram_remap(uintptr_t virtual_addr)
{
	DLPT_CSRAM_BASE_REMAP = virtual_addr;
	DLPT_CSRAM_CTRL_BASE_REMAP = DLPT_CSRAM_BASE_REMAP + DLPT_CSRAM_SIZE -
				     DLPT_CSRAM_CTRL_RESERVED_SIZE;
}

#endif /*__KERNEL__*/

/*
 * ========================================================
 * CPU SW Runner
 * ========================================================
 */
#if defined(CFG_CPU_PEAKPOWERTHROTTLING)
#endif /*CFG_GPU_PEAKPOWERTHROTTLING*/

/*
 * ========================================================
 * GPU SW Runner
 * ========================================================
 */
#if defined(CFG_GPU_PEAKPOWERTHROTTLING)
#include <tinysys_reg.h>
#include <remap.h>
#include <mt_mpu.h>

/*
 * ...................................
 * DLPT DRAM Ctrl Block
 * ...................................
 */
static int b_init_dlpt_dram_ctrl_block; /*= 0*/
static void init_dlpt_dram_ctrl_block(void)
{
	unsigned int remap_status_base;
	/* declare memory address 0x8c01fe08 ~ 0x8c01fe18 to pass MPU */
	while ((remap_status_base = dram_remap(DRAM_ID_PEAK_POWER_BUDGET,
					       DLPT_DRAM_BASE)) == 0) {
	};
	mpu_set_sc(MPU_REGION_PEAK_POWER_BUDGET_MPU, remap_status_base,
		   remap_status_base + 0x1000);
	mpu_region_enable(MPU_REGION_PEAK_POWER_BUDGET_MPU);
	dram_unremap(DRAM_ID_PEAK_POWER_BUDGET, remap_status_base);
	//indicate one-time initial ok
	b_init_dlpt_dram_ctrl_block = 1;
}

struct DlptDramCtrlBlock *dlpt_dram_ctrl_block_get(void)
{
	unsigned int remap_status_base;

	if (b_init_dlpt_dram_ctrl_block != 1)
		init_dlpt_dram_ctrl_block();
	while ((remap_status_base = dram_remap(DRAM_ID_PEAK_POWER_BUDGET,
					       DLPT_DRAM_BASE)) == 0) {
	};
	return (struct DlptDramCtrlBlock *)remap_status_base;
}

void dlpt_dram_ctrl_block_release(struct DlptDramCtrlBlock *remap_status_base)
{
	dram_unremap(DRAM_ID_PEAK_POWER_BUDGET,
		     (unsigned int)remap_status_base);
}

#endif /*CFG_GPU_PEAKPOWERTHROTTLING*/

/*
 * ...................................
 * Thermal CSRAM Ctrl Block
 * ...................................
 */
struct ThermalCsramCtrlBlock *thermal_csram_ctrl_block_get(void)
{
	void *remap_status_base = (void *)THERMAL_CSRAM_CTRL_BASE_REMAP;

	return (struct ThermalCsramCtrlBlock *)remap_status_base;
}

void thermal_csram_ctrl_block_release(
	struct ThermalCsramCtrlBlock *remap_status_base)
{
}

/*
 * ...................................
 * DLTP CSRAM Ctrl Block
 * ...................................
 */
struct DlptCsramCtrlBlock *dlpt_csram_ctrl_block_get(void)
{
	void *remap_status_base = (void *)DLPT_CSRAM_CTRL_BASE_REMAP;

	return (struct DlptCsramCtrlBlock *)remap_status_base;
}

void dlpt_csram_ctrl_block_release(struct DlptCsramCtrlBlock *remap_status_base)
{
}
