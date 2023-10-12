/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2023 MediaTek Inc.
 */

#ifndef __MTK_DCM_INTERNAL_H__
#define __MTK_DCM_INTERNAL_H__

#include <mtk_dcm_common.h>
#include "mt6878_dcm_autogen.h"

/* #define DCM_DEFAULT_ALL_OFF */

/* Note: ENABLE_DCM_IN_LK is used in kernel if DCM is enabled in LK */
#define ENABLE_DCM_IN_LK
#ifdef ENABLE_DCM_IN_LK
#define INIT_DCM_TYPE_BY_K	0
#endif

/* Note: If DCM has states other than DCM_OFF/DCM_ON, define here */
enum {
	ARMCORE_DCM_OFF = DCM_OFF,
	ARMCORE_DCM_MODE1 = DCM_ON,
	ARMCORE_DCM_MODE2 = DCM_ON+1,
};

/* Note: DCM_TYPE enums is used in DCM init & SMC calls */
enum {
	ARMCORE_DCM = 0,
	BUSDVT_DCM = 1,
	INFRA_DCM = 2,
	MCUSYS_DCM = 3,
	MCUSYS_ADB_DCM = 4,
	MCUSYS_APB_DCM = 5,
	MCUSYS_STALL_DCM = 6,
	PERI_DCM = 7,
	VLP_DCM = 8,
};

enum {
	ARMCORE_DCM_TYPE = BIT(ARMCORE_DCM),
	BUSDVT_DCM_TYPE = BIT(BUSDVT_DCM),
	INFRA_DCM_TYPE = BIT(INFRA_DCM),
	MCUSYS_DCM_TYPE = BIT(MCUSYS_DCM),
	MCUSYS_ADB_DCM_TYPE = BIT(MCUSYS_ADB_DCM),
	MCUSYS_APB_DCM_TYPE = BIT(MCUSYS_APB_DCM),
	MCUSYS_STALL_DCM_TYPE = BIT(MCUSYS_STALL_DCM),
	PERI_DCM_TYPE = BIT(PERI_DCM),
	VLP_DCM_TYPE = BIT(VLP_DCM),
};

int mt_dcm_dts_map(void);

#endif /* #ifndef __MTK_DCM_INTERNAL_H__ */
