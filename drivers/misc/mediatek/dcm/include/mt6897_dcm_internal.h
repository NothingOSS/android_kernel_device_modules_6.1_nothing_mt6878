/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2022 MediaTek Inc.
 */

#ifndef __MTK_DCM_INTERNAL_H__
#define __MTK_DCM_INTERNAL_H__

#include <mtk_dcm_common.h>
#include "mt6897_dcm_autogen.h"

/* #define DCM_DEFAULT_ALL_OFF */
/* #define DCM_BRINGUP */

/* Note: ENABLE_DCM_IN_LK is used in kernel if DCM is enabled in LK */
#define ENABLE_DCM_IN_LK
#ifdef ENABLE_DCM_IN_LK
#define INIT_DCM_TYPE_BY_K	0
#endif

/* #define CTRL_BIGCORE_DCM_IN_KERNEL */

#if IS_ENABLED(CONFIG_ARM_PSCI) || IS_ENABLED(CONFIG_MTK_PSCI)
#define MCUSYS_SMC_WRITE(addr, val)  mcusys_smc_write_phy(addr##_PHYS, val)
#ifndef mcsi_reg_read
#define mcsi_reg_read(offset) \
	mt_secure_call(MTK_SIP_KERENL_MCSI_NS_ACCESS, 0, offset, 0)
#endif
#ifndef mcsi_reg_write
#define mcsi_reg_write(val, offset) \
	mt_secure_call(MTK_SIP_KERENL_MCSI_NS_ACCESS, 1, offset, val)
#endif
#define MCSI_SMC_WRITE(addr, val)  mcsi_reg_write(val, (addr##_PHYS & 0xFFFF))
#define MCSI_SMC_READ(addr)  mcsi_reg_read(addr##_PHYS & 0xFFFF)
#else
#define MCUSYS_SMC_WRITE(addr, val)  mcusys_smc_write(addr, val)
#define MCSI_SMC_WRITE(addr, val)  reg_write(addr, val)
#define MCSI_SMC_READ(addr)  reg_read(addr)
#endif

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
};

/* Todo: Local function, actually can be static and remove from header */
int dcm_armcore(int mode);
int dcm_infra(int on);
int dcm_peri(int on);
int dcm_mcusys_acp(int on);
int dcm_mcusys_adb(int on);
int dcm_mcusys_bus(int on);
int dcm_mcusys_cbip(int on);
int dcm_mcusys_core(int on);
int dcm_mcusys_io(int on);
int dcm_mcusys_cpc_pbi(int on);
int dcm_mcusys_cpc_turbo(int on);
int dcm_mcusys_stall(int on);
int dcm_mcusys_apb(int on);
int dcm_vlp(int on);
int dcm_mcusys(int on);
int dcm_stall(int on);

int mt_dcm_dts_map(void);

// Todo: Check if needed
void dcm_set_hotplug_nb(void);
short dcm_get_cpu_cluster_stat(void);
/* unit of frequency is MHz */
int sync_dcm_set_cpu_freq(
unsigned int cci, unsigned int mp0, unsigned int mp1, unsigned int mp2);
int sync_dcm_set_cpu_div(
unsigned int cci, unsigned int mp0, unsigned int mp1, unsigned int mp2);

short is_dcm_bringup(void);

void dcm_array_register(void);

#endif /* #ifndef __MTK_DCM_INTERNAL_H__ */

