/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2022 MediaTek Inc.
 */

#ifndef __MTK_DCM_AUTOGEN_H__
#define __MTK_DCM_AUTOGEN_H__

#include <mtk_dcm.h>

#if IS_ENABLED(CONFIG_OF)
/* TODO: Fix all base addresses. */
extern unsigned long dcm_mcusys_par_wrap_base;
extern unsigned long dcm_mcusys_cpc_base;
extern unsigned long dcm_mcupm_base;
extern unsigned long dcm_mpsys_base;
extern unsigned long dcm_mcusys_complex0_base;
extern unsigned long dcm_mcusys_complex1_base;
extern unsigned long dcm_mcusys_cpu4_base;
extern unsigned long dcm_mcusys_cpu5_base;
extern unsigned long dcm_mcusys_cpu6_base;
extern unsigned long dcm_mcusys_cpu7_base;
extern unsigned long dcm_ifrbus_ao_base;
extern unsigned long dcm_peri_ao_bcrm_base;
extern unsigned long dcm_vlp_ao_bcrm_base;

#ifndef USE_DRAM_API_INSTEAD
extern unsigned long dcm_ddrphy1_ao_base;
#endif /* #ifndef USE_DRAM_API_INSTEAD */
#if !defined(MCUSYS_PAR_WRAP_BASE)
#define MCUSYS_PAR_WRAP_BASE (dcm_mcusys_par_wrap_base)
#endif /* !defined(MCUSYS_PAR_WRAP_BASE) */
#if !defined(MCUSYS_CPC_BASE)
#define MCUSYS_CPC_BASE (dcm_mcusys_cpc_base)
#endif /* !defined(MCUSYS_CPC_BASE) */
#if !defined(MCUPM_BASE)
#define MCUPM_BASE (dcm_mcupm_base)
#endif /* !defined(MCUPM_BASE) */
#if !defined(MPSYS_BASE)
#define MPSYS_BASE (dcm_mpsys_base)
#endif /* !defined(MPSYS_BASE) */
#if !defined(MCUSYS_COMPLEX0_BASE)
#define MCUSYS_COMPLEX0_BASE (dcm_mcusys_complex0_base)
#endif /* !defined(MCUSYS_COMPLEX0_BASE) */
#if !defined(MCUSYS_COMPLEX1_BASE)
#define MCUSYS_COMPLEX1_BASE (dcm_mcusys_complex1_base)
#endif /* !defined(MCUSYS_COMPLEX1_BASE) */
#if !defined(MCUSYS_CPU4_BASE)
#define MCUSYS_CPU4_BASE (dcm_mcusys_cpu4_base)
#endif /* !defined(MCUSYS_CPU4_BASE) */
#if !defined(MCUSYS_CPU5_BASE)
#define MCUSYS_CPU5_BASE (dcm_mcusys_cpu5_base)
#endif /* !defined(MCUSYS_CPU5_BASE) */
#if !defined(MCUSYS_CPU6_BASE)
#define MCUSYS_CPU6_BASE (dcm_mcusys_cpu6_base)
#endif /* !defined(MCUSYS_CPU6_BASE) */
#if !defined(MCUSYS_CPU7_BASE)
#define MCUSYS_CPU7_BASE (dcm_mcusys_cpu7_base)
#endif /* !defined(MCUSYS_CPU7_BASE) */
#if !defined(IFRBUS_AO_BASE)
#define IFRBUS_AO_BASE (dcm_ifrbus_ao_base)
#endif /* !defined(IFRBUS_AO_BASE) */
#if !defined(PERI_AO_BCRM_BASE)
#define PERI_AO_BCRM_BASE (dcm_peri_ao_bcrm_base)
#endif /* !defined(PERI_AO_BCRM_BASE) */
#if !defined(VLP_AO_BCRM_BASE)
#define VLP_AO_BCRM_BASE (dcm_vlp_ao_bcrm_base)
#endif /* !defined(VLP_AO_BCRM_BASE) */

#else /* !IS_ENABLED(CONFIG_OF)) */

/* Here below used in CTP and pl for references. */
#undef MCUSYS_PAR_WRAP_BASE
#undef MCUSYS_CPC_BASE
#undef MCUPM_BASE
#undef MPSYS_BASE
#undef MCUSYS_COMPLEX0_BASE
#undef MCUSYS_COMPLEX1_BASE
#undef MCUSYS_CPU4_BASE
#undef MCUSYS_CPU5_BASE
#undef MCUSYS_CPU6_BASE
#undef MCUSYS_CPU7_BASE
#undef IFRBUS_AO_BASE
#undef PERI_AO_BCRM_BASE
#undef VLP_AO_BCRM_BASE

/* Base */
#define MCUSYS_PAR_WRAP_BASE 0xc000200
#define MCUSYS_CPC_BASE      0xc040000
#define MCUPM_BASE           0xc070000
#define MPSYS_BASE           0xc100000
#define MCUSYS_COMPLEX0_BASE 0xc18c000
#define MCUSYS_COMPLEX1_BASE 0xc1ac000
#define MCUSYS_CPU4_BASE     0xc1c0000
#define MCUSYS_CPU5_BASE     0xc1d0000
#define MCUSYS_CPU6_BASE     0xc1e0000
#define MCUSYS_CPU7_BASE     0xc1f0000
#define IFRBUS_AO_BASE       0x1002c000
#define PERI_AO_BCRM_BASE    0x11035000
#define VLP_AO_BCRM_BASE     0x1c017000
#endif /* if IS_ENABLED(CONFIG_OF)) */

/* Register Definition */
#define DCM_SET_RW_0                                     (IFRBUS_AO_BASE + 0xb00)
#define VDNR_DCM_TOP_PERI_PAR_BUS_u_PERI_PAR_BUS_CTRL_0  (PERI_AO_BCRM_BASE + 0x18)
#define VDNR_DCM_TOP_PERI_PAR_BUS_u_PERI_PAR_BUS_CTRL_1  (PERI_AO_BCRM_BASE + 0x1c)
#define VDNR_DCM_TOP_PERI_PAR_BUS_u_PERI_PAR_BUS_CTRL_2  (PERI_AO_BCRM_BASE + 0x20)
#define VDNR_DCM_TOP_VLP_PAR_BUS_u_VLP_PAR_BUS_CTRL_1    (VLP_AO_BCRM_BASE + 0xc4)
#define MCUSYS_PAR_WRAP_CPC_DCM_Enable                   (MCUSYS_CPC_BASE + 0x19c)
#define MCUSYS_PAR_WRAP_MP_ADB_DCM_CFG0                  (MCUSYS_PAR_WRAP_BASE + 0x70)
#define MCUSYS_PAR_WRAP_ADB_FIFO_DCM_EN                  (MCUSYS_PAR_WRAP_BASE + 0x78)
#define MCUSYS_PAR_WRAP_MP0_DCM_CFG0                     (MCUSYS_PAR_WRAP_BASE + 0x7c)
#define MCUSYS_PAR_WRAP_CI700_DCM_CTRL                   (MCUSYS_PAR_WRAP_BASE + 0x98)
#define MCUSYS_PAR_WRAP_QDCM_CONFIG0                     (MCUSYS_PAR_WRAP_BASE + 0x80)
#define MCUSYS_PAR_WRAP_QDCM_CONFIG1                     (MCUSYS_PAR_WRAP_BASE + 0x84)
#define MCUSYS_PAR_WRAP_CBIP_CABGEN_3TO1_CONFIG          (MCUSYS_PAR_WRAP_BASE + 0xa0)
#define MCUSYS_PAR_WRAP_CBIP_CABGEN_2TO1_CONFIG          (MCUSYS_PAR_WRAP_BASE + 0xa4)
#define MCUSYS_PAR_WRAP_CBIP_CABGEN_4TO2_CONFIG          (MCUSYS_PAR_WRAP_BASE + 0xa8)
#define MCUSYS_PAR_WRAP_CBIP_CABGEN_1TO2_CONFIG          (MCUSYS_PAR_WRAP_BASE + 0xac)
#define MCUSYS_PAR_WRAP_CBIP_CABGEN_2TO5_CONFIG          (MCUSYS_PAR_WRAP_BASE + 0xb0)
#define MCUSYS_PAR_WRAP_CBIP_P2P_CONFIG0                 (MCUSYS_PAR_WRAP_BASE + 0xb4)
#define MCUSYS_PAR_WRAP_QDCM_CONFIG2                     (MCUSYS_PAR_WRAP_BASE + 0x88)
#define MCUSYS_PAR_WRAP_QDCM_CONFIG3                     (MCUSYS_PAR_WRAP_BASE + 0x8c)
#define MCUSYS_PAR_WRAP_L3GIC_ARCH_CG_CONFIG             (MCUSYS_PAR_WRAP_BASE + 0x94)
#define MCUSYS_PAR_WRAP_MP_CENTRAL_FABRIC_SUB_CHANNEL_CG (MCUSYS_PAR_WRAP_BASE + 0xb8)
#define MCUSYS_COMPLEX0_STALL_DCM_CONF0                  (MCUSYS_COMPLEX0_BASE + 0x210)
#define MCUSYS_COMPLEX1_STALL_DCM_CONF1                  (MCUSYS_COMPLEX1_BASE + 0x210)
#define MCUPM_CFGREG_DCM_EN                              (MCUPM_BASE + 0x20)
#define MPSYS_ACP_SLAVE_DCM_EN                           (MPSYS_BASE + 0xd0)
#define MCUSYS_CPU4_BCPU_SYS_CON1                        (MCUSYS_CPU4_BASE + 0x18)
#define MCUSYS_CPU5_BCPU_SYS_CON2                        (MCUSYS_CPU5_BASE + 0x18)
#define MCUSYS_CPU6_BCPU_SYS_CON3                        (MCUSYS_CPU6_BASE + 0x18)
#define MCUSYS_CPU7_BCPU_SYS_CON4                        (MCUSYS_CPU7_BASE + 0x18)

bool dcm_ifrbus_ao_infra_bus_dcm_is_on(void);
void dcm_ifrbus_ao_infra_bus_dcm(int on);
bool dcm_peri_ao_bcrm_peri_bus_dcm_is_on(void);
void dcm_peri_ao_bcrm_peri_bus_dcm(int on);
bool dcm_vlp_ao_bcrm_vlp_bus_dcm_is_on(void);
void dcm_vlp_ao_bcrm_vlp_bus_dcm(int on);
bool dcm_mcusys_par_wrap_cpc_pbi_dcm_is_on(void);
void dcm_mcusys_par_wrap_cpc_pbi_dcm(int on);
bool dcm_mcusys_par_wrap_cpc_turbo_dcm_is_on(void);
void dcm_mcusys_par_wrap_cpc_turbo_dcm(int on);
bool dcm_mcusys_par_wrap_mcu_acp_dcm_is_on(void);
void dcm_mcusys_par_wrap_mcu_acp_dcm(int on);
bool dcm_mcusys_par_wrap_mcu_adb_dcm_is_on(void);
void dcm_mcusys_par_wrap_mcu_adb_dcm(int on);
bool dcm_mcusys_par_wrap_mcu_apb_dcm_is_on(void);
void dcm_mcusys_par_wrap_mcu_apb_dcm(int on);
bool dcm_mcusys_par_wrap_mcu_bkr_ldcm_is_on(void);
void dcm_mcusys_par_wrap_mcu_bkr_ldcm(int on);
bool dcm_mcusys_par_wrap_mcu_bus_qdcm_is_on(void);
void dcm_mcusys_par_wrap_mcu_bus_qdcm(int on);
bool dcm_mcusys_par_wrap_mcu_cbip_dcm_is_on(void);
void dcm_mcusys_par_wrap_mcu_cbip_dcm(int on);
bool dcm_mcusys_par_wrap_mcu_core_qdcm_is_on(void);
void dcm_mcusys_par_wrap_mcu_core_qdcm(int on);
bool dcm_mcusys_par_wrap_mcu_io_dcm_is_on(void);
void dcm_mcusys_par_wrap_mcu_io_dcm(int on);
bool dcm_mcusys_par_wrap_mcu_misc_dcm_is_on(void);
void dcm_mcusys_par_wrap_mcu_misc_dcm(int on);
bool dcm_mcusys_par_wrap_cpu0_mcu_stalldcm_is_on(void);
void dcm_mcusys_par_wrap_cpu0_mcu_stalldcm(int on);
bool dcm_mcusys_par_wrap_cpu1_mcu_stalldcm_is_on(void);
void dcm_mcusys_par_wrap_cpu1_mcu_stalldcm(int on);
bool dcm_mcusys_par_wrap_cpu2_mcu_stalldcm_is_on(void);
void dcm_mcusys_par_wrap_cpu2_mcu_stalldcm(int on);
bool dcm_mcusys_par_wrap_cpu3_mcu_stalldcm_is_on(void);
void dcm_mcusys_par_wrap_cpu3_mcu_stalldcm(int on);
bool dcm_mcusys_par_wrap_cpu4_mcu_stalldcm_is_on(void);
void dcm_mcusys_par_wrap_cpu4_mcu_stalldcm(int on);
bool dcm_mcusys_par_wrap_cpu5_mcu_stalldcm_is_on(void);
void dcm_mcusys_par_wrap_cpu5_mcu_stalldcm(int on);
bool dcm_mcusys_par_wrap_cpu6_mcu_stalldcm_is_on(void);
void dcm_mcusys_par_wrap_cpu6_mcu_stalldcm(int on);
bool dcm_mcusys_par_wrap_cpu7_mcu_stalldcm_is_on(void);
void dcm_mcusys_par_wrap_cpu7_mcu_stalldcm(int on);
bool dcm_mcupm_adb_dcm_is_on(void);
void dcm_mcupm_adb_dcm(int on);
bool dcm_mcupm_apb_dcm_is_on(void);
void dcm_mcupm_apb_dcm(int on);
bool dcm_mpsys_acp_slave_is_on(void);
void dcm_mpsys_acp_slave(int on);
bool dcm_mcusys_cpu4_apb_dcm_is_on(void);
void dcm_mcusys_cpu4_apb_dcm(int on);
bool dcm_mcusys_cpu5_apb_dcm_is_on(void);
void dcm_mcusys_cpu5_apb_dcm(int on);
bool dcm_mcusys_cpu6_apb_dcm_is_on(void);
void dcm_mcusys_cpu6_apb_dcm(int on);
bool dcm_mcusys_cpu7_apb_dcm_is_on(void);
void dcm_mcusys_cpu7_apb_dcm(int on);
#endif /* __MTK_DCM_AUTOGEN_H__ */
