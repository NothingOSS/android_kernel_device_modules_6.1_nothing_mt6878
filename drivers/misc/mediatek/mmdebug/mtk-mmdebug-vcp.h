/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2022 MediaTek Inc.
 * Author: Anthony Huang <anthony.huang@mediatek.com>
 */

#ifndef __MMDEBUG_VCP_H
#define __MMDEBUG_VCP_H

#define MMDEBUG_DBG(fmt, args...) \
	pr_notice("[mmdebug][dbg]%s: "fmt"\n", __func__, ##args)
#define MMDEBUG_ERR(fmt, args...) \
	pr_notice("[mmdebug][err]%s: "fmt"\n", __func__, ##args)

/* vcp/.../mmdebug_public.h */
enum MMDEBUG_FUNC {
	MMDEBUG_FUNC_SMI_DUMP,
	MMDEBUG_FUNC_NUM
};

/* vcp/.../mmdebug_private.h */
struct mmdebug_ipi_data {
	uint8_t func;
	uint8_t idx;
	uint8_t ack;
	uint32_t base;
};

#endif /* __MMDEBUG_VCP_H */
