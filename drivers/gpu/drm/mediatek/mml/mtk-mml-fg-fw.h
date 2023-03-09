/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */
#ifndef __MTK_MML_FG_ALG_H__
#define __MTK_MML_FG_ALG_H__

#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/dma-mapping.h>
#include "mtk-mml-core.h"
#include "mtk-mml-pq.h"
#include "mtk-mml-pq-core.h"

struct mml_pq_fg_alg_data {
	struct mml_pq_film_grain_params *metadata;
	bool is_yuv_444;
	s32 bit_depth;
	u32 random_register;
	s32 grain_center;
	s32 grain_min;
	s32 grain_max;
	s32 luma_grain_width;
	s32 luma_grain_height;
	s32 chroma_grain_width;
	s32 chroma_grain_height;
	s32 luma_grain_size;
	s32 cb_grain_size;
	s32 cr_grain_size;

	u32 pps0_setting;
	u32 pps1_setting;
	u32 pps2_setting;
	u32 pps3_setting;
	s32 apply_grain;
	s32 update_grain;
	char *allocated_va;
};

void mml_pq_fg_calc(struct mml_pq_dma_buffer *lut,
	struct mml_pq_film_grain_params *metadata, bool isYUV444, s32 bitDepth);

#endif	/* __MTK_MML_FG_ALG_H__ */
