/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2022 MediaTek Inc.
 */

#ifndef __MTK_DCM_INTERNAL_H__
#define __MTK_DCM_INTERNAL_H__

#include <mtk_dcm_common.h>
#include "mt6985_dcm_autogen.h"

/* #define DCM_DEFAULT_ALL_OFF */
/* #define DCM_BRINGUP */

/* Note: ENABLE_DCM_IN_LK is used in kernel if DCM is enabled in LK */
#define ENABLE_DCM_IN_LK
#ifdef ENABLE_DCM_IN_LK
#define INIT_DCM_TYPE_BY_K	0
#endif

/* #define CTRL_BIGCORE_DCM_IN_KERNEL */

/* #define reg_read(addr)	__raw_readl(IOMEM(addr)) */
#define reg_read(addr) readl((void *)addr)
/*#define reg_write(addr, val)	mt_reg_sync_writel((val), ((void *)addr))*/
#define reg_write(addr, val) \
	do { writel(val, (void *)addr); wmb(); } while (0) /* sync write */

#define REG_DUMP(addr) \
	dcm_pr_info("%-60s(0x%08lx): 0x%08x\n", #addr, addr, reg_read(addr))
#define SECURE_REG_DUMP(addr) \
	dcm_pr_info("%-60s(0x%08lx): 0x%08x\n", \
	#addr, addr, mcsi_reg_read(addr##_PHYS & 0xFFFF))

int mt_dcm_dts_map(void);
short is_dcm_bringup(void);
void dcm_array_register(void);

#endif /* #ifndef __MTK_DCM_INTERNAL_H__ */

