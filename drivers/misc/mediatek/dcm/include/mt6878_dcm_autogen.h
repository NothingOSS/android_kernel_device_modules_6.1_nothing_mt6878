/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2023 MediaTek Inc.
 */

#ifndef __MTK_DCM_AUTOGEN_H__
#define __MTK_DCM_AUTOGEN_H__

#include <mtk_dcm.h>

#if IS_ENABLED(CONFIG_OF)
/* adjust following base addresses which is already assign by iomap value */
extern unsigned long dcm_mcusys_par_wrap_base;
extern unsigned long dcm_infracfg_ao_base;
extern unsigned long dcm_infra_ao_bcrm_base;
extern unsigned long dcm_peri_ao_bcrm_base;
extern unsigned long dcm_vlp_ao_bcrm_base;

#if !defined(MCUSYS_PAR_WRAP_BASE)
#define MCUSYS_PAR_WRAP_BASE (dcm_mcusys_par_wrap_base)
#endif /* !defined(MCUSYS_PAR_WRAP_BASE) */
#if !defined(INFRACFG_AO_BASE)
#define INFRACFG_AO_BASE (dcm_infracfg_ao_base)
#endif /* !defined(INFRACFG_AO_BASE) */
#if !defined(INFRA_AO_BCRM_BASE)
#define INFRA_AO_BCRM_BASE (dcm_infra_ao_bcrm_base)
#endif /* !defined(INFRA_AO_BCRM_BASE) */
#if !defined(PERI_AO_BCRM_BASE)
#define PERI_AO_BCRM_BASE (dcm_peri_ao_bcrm_base)
#endif /* !defined(PERI_AO_BCRM_BASE) */
#if !defined(VLP_AO_BCRM_BASE)
#define VLP_AO_BCRM_BASE (dcm_vlp_ao_bcrm_base)
#endif /* !defined(VLP_AO_BCRM_BASE) */

#else /* !IS_ENABLED(CONFIG_OF)) */

/* Here below used in CTP and pl for references. */
#undef MCUSYS_PAR_WRAP_BASE
#undef INFRACFG_AO_BASE
#undef INFRA_AO_BCRM_BASE
#undef PERI_AO_BCRM_BASE
#undef VLP_AO_BCRM_BASE

/* Base */
#define MCUSYS_PAR_WRAP_BASE 0xc530000
#define INFRACFG_AO_BASE     0x10001000
#define INFRA_AO_BCRM_BASE   0x10022000
#define PERI_AO_BCRM_BASE    0x11035000
#define VLP_AO_BCRM_BASE     0x1c017000
#endif /* if IS_ENABLED(CONFIG_OF)) */

/* Register Definition */
#define CPU_PLLDIV_CFG0                                   (MCUSYS_PAR_WRAP_BASE + 0xa2a0)
#define CPU_PLLDIV_CFG1                                   (MCUSYS_PAR_WRAP_BASE + 0xa2a4)
#define BUS_PLLDIV_CFG                                    (MCUSYS_PAR_WRAP_BASE + 0xa2e0)
#define MCSI_DCM0                                         (MCUSYS_PAR_WRAP_BASE + 0xa440)
#define MP_ADB_DCM_CFG0                                   (MCUSYS_PAR_WRAP_BASE + 0xa500)
#define MP_ADB_DCM_CFG4                                   (MCUSYS_PAR_WRAP_BASE + 0xa510)
#define MCUSYS_DCM_CFG0                                   (MCUSYS_PAR_WRAP_BASE + 0xa5c0)
#define MP_MISC_DCM_CFG0                                  (MCUSYS_PAR_WRAP_BASE + 0xa518)
#define MP0_DCM_CFG0                                      (MCUSYS_PAR_WRAP_BASE + 0xc880)
#define EMI_WFIFO                                         (MCUSYS_PAR_WRAP_BASE + 0xa900)
#define MP0_DCM_CFG7                                      (MCUSYS_PAR_WRAP_BASE + 0xc89c)
#define INFRA_BUS_DCM_CTRL                                (INFRACFG_AO_BASE + 0x70)
#define PERI_BUS_DCM_CTRL                                 (INFRACFG_AO_BASE + 0x74)
#define P2P_RX_CLK_ON                                     (INFRACFG_AO_BASE + 0xa0)
#define INFRA_AXIMEM_IDLE_BIT_EN_0                        (INFRACFG_AO_BASE + 0xa30)
#define VDNR_DCM_TOP_INFRA_PAR_BUS_u_INFRA_PAR_BUS_CTRL_0 (INFRA_AO_BCRM_BASE + 0x30)
#define VDNR_DCM_TOP_INFRA_PAR_BUS_u_INFRA_PAR_BUS_CTRL_3 (INFRA_AO_BCRM_BASE + 0x3c)
#define VDNR_DCM_TOP_PERI_PAR_BUS_u_PERI_PAR_BUS_CTRL_0   (PERI_AO_BCRM_BASE + 0x18)
#define VDNR_DCM_TOP_PERI_PAR_BUS_u_PERI_PAR_BUS_CTRL_1   (PERI_AO_BCRM_BASE + 0x1c)
#define VDNR_DCM_TOP_PERI_PAR_BUS_u_PERI_PAR_BUS_CTRL_2   (PERI_AO_BCRM_BASE + 0x20)
#define VDNR_PWR_PROT_VLP_PAR_BUS_u_spm_CTRL_0            (VLP_AO_BCRM_BASE + 0xb4)

void dcm_mcusys_par_wrap_cpu_pll_div_0_dcm(int on);
bool dcm_mcusys_par_wrap_cpu_pll_div_0_dcm_is_on(void);
void dcm_mcusys_par_wrap_cpu_pll_div_1_dcm(int on);
bool dcm_mcusys_par_wrap_cpu_pll_div_1_dcm_is_on(void);
void dcm_mcusys_par_wrap_last_cor_idle_dcm(int on);
bool dcm_mcusys_par_wrap_last_cor_idle_dcm_is_on(void);
void dcm_mcusys_par_wrap_cpubiu_dcm(int on);
bool dcm_mcusys_par_wrap_cpubiu_dcm_is_on(void);
void dcm_mcusys_par_wrap_adb_dcm(int on);
bool dcm_mcusys_par_wrap_adb_dcm_is_on(void);
void dcm_mcusys_par_wrap_misc_dcm(int on);
bool dcm_mcusys_par_wrap_misc_dcm_is_on(void);
void dcm_mcusys_par_wrap_mp0_qdcm(int on);
bool dcm_mcusys_par_wrap_mp0_qdcm_is_on(void);
void dcm_mcusys_par_wrap_apb_dcm(int on);
bool dcm_mcusys_par_wrap_apb_dcm_is_on(void);
void dcm_mcusys_par_wrap_emi_wfifo(int on);
bool dcm_mcusys_par_wrap_emi_wfifo_is_on(void);
void dcm_mcusys_par_wrap_core_stall_dcm(int on);
bool dcm_mcusys_par_wrap_core_stall_dcm_is_on(void);
void dcm_mcusys_par_wrap_fcm_stall_dcm(int on);
bool dcm_mcusys_par_wrap_fcm_stall_dcm_is_on(void);
void dcm_infracfg_ao_infra_bus_dcm(int on);
bool dcm_infracfg_ao_infra_bus_dcm_is_on(void);
void dcm_infracfg_ao_peri_bus_dcm(int on);
bool dcm_infracfg_ao_peri_bus_dcm_is_on(void);
void dcm_infracfg_ao_peri_module_dcm(int on);
bool dcm_infracfg_ao_peri_module_dcm_is_on(void);
void dcm_infracfg_ao_infra_rx_p2p_dcm(int on);
bool dcm_infracfg_ao_infra_rx_p2p_dcm_is_on(void);
void dcm_infracfg_ao_aximem_bus_dcm(int on);
bool dcm_infracfg_ao_aximem_bus_dcm_is_on(void);
void dcm_infra_ao_bcrm_infra_bus_dcm(int on);
bool dcm_infra_ao_bcrm_infra_bus_dcm_is_on(void);
void dcm_infra_ao_bcrm_infra_bus_fmem_sub_dcm(int on);
bool dcm_infra_ao_bcrm_infra_bus_fmem_sub_dcm_is_on(void);
void dcm_peri_ao_bcrm_peri_bus_dcm(int on);
bool dcm_peri_ao_bcrm_peri_bus_dcm_is_on(void);
void dcm_vlp_ao_bcrm_vlp_bus_dcm(int on);
bool dcm_vlp_ao_bcrm_vlp_bus_dcm_is_on(void);
#endif /* __MTK_DCM_AUTOGEN_H__ */
