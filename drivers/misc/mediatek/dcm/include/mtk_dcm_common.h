/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2022 MediaTek Inc.
 */

#ifndef __MTK_DCM_COMMON_H__
#define __MTK_DCM_COMMON_H__

#include <linux/ratelimit.h>

#define DCM_OFF (0)
#define DCM_ON (1)
#define DCM_INIT (-1)

#define TAG	"[Power/dcm] "
#define dcm_pr_notice(fmt, args...)			\
	pr_notice(TAG fmt, ##args)
#define dcm_pr_info_limit(fmt, args...)			\
	pr_info_ratelimited(TAG fmt, ##args)
#define dcm_pr_info(fmt, args...)			\
	pr_info(TAG fmt, ##args)
#define dcm_pr_dbg(fmt, args...)			\
	do {						\
		if (dcm_debug)				\
			pr_info(TAG fmt, ##args);	\
	} while (0)

#define DCM_BASE_INFO(_name) \
{ \
	.name = #_name, \
	.base = &_name, \
}


enum {
	ARMCORE_DCM_OFF = DCM_OFF,
	ARMCORE_DCM_MODE1 = DCM_ON,
	ARMCORE_DCM_MODE2 = DCM_ON+1,
};

enum {
	INFRA_DCM_OFF = DCM_OFF,
	INFRA_DCM_ON = DCM_ON,
};

enum {
	PERI_DCM_OFF = DCM_OFF,
	PERI_DCM_ON = DCM_ON,
};

enum {
	MCUSYS_ACP_DCM_OFF = DCM_OFF,
	MCUSYS_ACP_DCM_ON = DCM_ON,
};

enum {
	MCUSYS_ADB_DCM_OFF = DCM_OFF,
	MCUSYS_ADB_DCM_ON = DCM_ON,
};

enum {
	MCUSYS_BUS_DCM_OFF = DCM_OFF,
	MCUSYS_BUS_DCM_ON = DCM_ON,
};

enum {
	MCUSYS_CBIP_DCM_OFF = DCM_OFF,
	MCUSYS_CBIP_DCM_ON = DCM_ON,
};

enum {
	MCUSYS_CORE_DCM_OFF = DCM_OFF,
	MCUSYS_CORE_DCM_ON = DCM_ON,
};

enum {
	MCUSYS_IO_DCM_OFF = DCM_OFF,
	MCUSYS_IO_DCM_ON = DCM_ON,
};

enum {
	MCUSYS_CPC_PBI_DCM_OFF = DCM_OFF,
	MCUSYS_CPC_PBI_DCM_ON = DCM_ON,
};

enum {
	MCUSYS_CPC_TURBO_DCM_OFF = DCM_OFF,
	MCUSYS_CPC_TURBO_DCM_ON = DCM_ON,
};

enum {
	MCUSYS_STALL_DCM_OFF = DCM_OFF,
	MCUSYS_STALL_DCM_ON = DCM_ON,
};

enum {
	MCUSYS_BKR_DCM_OFF = DCM_OFF,
	MCUSYS_BKR_DCM_ON = DCM_ON,
};

enum {
	MCUSYS_DSU_STALL_DCM_OFF = DCM_OFF,
	MCUSYS_DSU_STALL_DCM_ON = DCM_ON,
};

enum {
	MCUSYS_MISC_DCM_OFF = DCM_OFF,
	MCUSYS_MISC_DCM_ON = DCM_ON,
};

enum {
	MCUSYS_APB_DCM_OFF = DCM_OFF,
	MCUSYS_APB_DCM_ON = DCM_ON,
};

enum {
	MCUSYS_DCM_OFF = DCM_OFF,
	MCUSYS_DCM_ON = DCM_ON,
};

enum {
	MCUSYS_MCUPM_DCM_OFF = DCM_OFF,
	MCUSYS_MCUPM_DCM_ON = DCM_ON,
};
enum {
	VLP_DCM_OFF = DCM_OFF,
	VLP_DCM_ON = DCM_ON,
};

enum {
	UFS0_DCM_OFF = DCM_OFF,
	UFS0_DCM_ON = DCM_ON,
};

enum {
	PEXTP_DCM_OFF = DCM_OFF,
	PEXTP_DCM_ON = DCM_ON,
};

enum {
	ARMCORE_DCM = 0,
	MCUSYS_DCM,
	INFRA_DCM,
	PERI_DCM,
	MCUSYS_ACP_DCM,
	MCUSYS_ADB_DCM = 5,
	MCUSYS_BUS_DCM,
	MCUSYS_CBIP_DCM,
	MCUSYS_CORE_DCM,
	MCUSYS_IO_DCM,
	MCUSYS_CPC_PBI_DCM = 10,
	MCUSYS_CPC_TURBO_DCM,
	MCUSYS_STALL_DCM,
	MCUSYS_BKR_DCM,
	MCUSYS_DSU_STALL_DCM,
	MCUSYS_MISC_DCM = 15,
	MCUSYS_APB_DCM,
	MCUSYS_MCUPM_DCM,
	MCUSYS_L3C_DCM,
	MCUSYS_DSU_ACP_DCM,
	MCUSYS_CHI_MON_DCM = 20,
	MCUSYS_GIC_SPI_DCM,
	MCUSYS_EBG_DCM,
	VLP_DCM,
	UFS0_DCM,
	PEXTP_DCM = 25,
	NR_DCM,
};

enum {
	ARMCORE_DCM_TYPE   = BIT(ARMCORE_DCM),
	MCUSYS_DCM_TYPE	   = BIT(MCUSYS_DCM),
	INFRA_DCM_TYPE	   = BIT(INFRA_DCM),
	PERI_DCM_TYPE	   = BIT(PERI_DCM),
	MCUSYS_ACP_DCM_TYPE = BIT(MCUSYS_ACP_DCM),
	MCUSYS_ADB_DCM_TYPE = BIT(MCUSYS_ADB_DCM),
	MCUSYS_BUS_DCM_TYPE = BIT(MCUSYS_BUS_DCM),
	MCUSYS_CBIP_DCM_TYPE = BIT(MCUSYS_CBIP_DCM),
	MCUSYS_CORE_DCM_TYPE = BIT(MCUSYS_CORE_DCM),
	MCUSYS_IO_DCM_TYPE	= BIT(MCUSYS_IO_DCM),
	MCUSYS_CPC_PBI_DCM_TYPE = BIT(MCUSYS_CPC_PBI_DCM),
	MCUSYS_CPC_TURBO_DCM_TYPE = BIT(MCUSYS_CPC_TURBO_DCM),
	MCUSYS_STALL_DCM_TYPE	= BIT(MCUSYS_STALL_DCM),
	MCUSYS_BKR_DCM_TYPE		= BIT(MCUSYS_BKR_DCM),
	MCUSYS_DSU_STALL_DCM_TYPE = BIT(MCUSYS_DSU_STALL_DCM),
	MCUSYS_MISC_DCM_TYPE	= BIT(MCUSYS_MISC_DCM),
	MCUSYS_APB_DCM_TYPE		= BIT(MCUSYS_APB_DCM),
	MCUSYS_MCUPM_DCM_TYPE	   = BIT(MCUSYS_MCUPM_DCM),
	MCUSYS_L3C_DCM_TYPE = BIT(MCUSYS_L3C_DCM),
	MCUSYS_DSU_ACP_DCM_TYPE = BIT(MCUSYS_DSU_ACP_DCM),
	MCUSYS_CHI_MON_DCM_TYPE = BIT(MCUSYS_CHI_MON_DCM),
	MCUSYS_GIC_SPI_DCM_TYPE = BIT(MCUSYS_GIC_SPI_DCM),
	MCUSYS_EBG_DCM_TYPE = BIT(MCUSYS_EBG_DCM),
	VLP_DCM_TYPE	   = BIT(VLP_DCM),
	UFS0_DCM_TYPE	   = BIT(UFS0_DCM),
	PEXTP_DCM_TYPE	   = BIT(PEXTP_DCM),
	ALL_DCM_TYPE	   = BIT(NR_DCM) - 1,
};

/*****************************************************/
typedef int (*DCM_FUNC)(int);
typedef int (*DCM_ISON_FUNC)(void);
typedef void (*DCM_FUNC_VOID_VOID)(void);
typedef void (*DCM_FUNC_VOID_UINTR)(unsigned int *);
typedef void (*DCM_FUNC_VOID_UINTR_INTR)(unsigned int *, int *);
typedef void (*DCM_PRESET_FUNC)(void);
typedef void (*DCM_FUNC_VOID_UINT)(unsigned int);

struct DCM_OPS {
	DCM_FUNC_VOID_VOID dump_regs;
	DCM_FUNC_VOID_UINTR_INTR get_init_state_and_type;
	DCM_FUNC_VOID_UINT set_debug_mode;
};

struct DCM_BASE {
	char *name;
	unsigned long *base;
};

struct DCM {
	bool force_disable;
	int default_state;
	DCM_FUNC func;
	DCM_ISON_FUNC is_on_func;
	DCM_PRESET_FUNC preset_func;
	int typeid;
	char *name;
};

#endif /* #ifndef __MTK_DCM_COMMON_H__ */
