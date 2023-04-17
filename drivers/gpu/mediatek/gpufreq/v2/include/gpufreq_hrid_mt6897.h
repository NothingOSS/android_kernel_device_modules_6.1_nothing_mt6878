/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2022 MediaTek Inc.
 */

#ifndef __GPUFREQ_HRID_H__
#define __GPUFREQ_HRID_H__

/**************************************************
 * Definition
 **************************************************/
#define GPUFREQ_HRID_LOOKUP_ENABLE      (0)
/*
 * VGPU_P1  : 1400 MHz
 * VGPU_P2  : 820  MHz
 * VGPU_P3  : 265  MHz
 */
#define HRIDOP(_hrid_0, _hrid_1, _vgpu_p1, _vgpu_p2, _vgpu_p3) \
	{                                  \
		.hrid_0 = _hrid_0,             \
		.hrid_1 = _hrid_1,             \
		.vgpu_p1 = _vgpu_p1,           \
		.vgpu_p2 = _vgpu_p2,           \
		.vgpu_p3 = _vgpu_p3,           \
	}

/**************************************************
 * Structure
 **************************************************/
struct gpufreq_hird_info {
	unsigned int hrid_0;
	unsigned int hrid_1;
	unsigned int vgpu_p1;
	unsigned int vgpu_p2;
	unsigned int vgpu_p3;
};

/**************************************************
 * HRID Table
 **************************************************/
#define NUM_HRID				ARRAY_SIZE(g_hrid_table)
struct gpufreq_hird_info g_hrid_table[] = {
	HRIDOP(0xF825823A, 0x0EA1FF75, 80000, 65000, 62500),
};

#endif /* __GPUFREQ_HRID_H__ */
