// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2023 MediaTek Inc.
 */

#include <linux/io.h>

#include <mt6878_dcm_internal.h>
#include <mt6878_dcm_autogen.h>
#include <mtk_dcm.h>
#define MCUSYS_PAR_WRAP_CPU_PLL_DIV_0_DCM_REG0_MASK ((0x3U << 24))
#define MCUSYS_PAR_WRAP_CPU_PLL_DIV_0_DCM_REG0_ON ((0x3U << 24))
#define MCUSYS_PAR_WRAP_CPU_PLL_DIV_0_DCM_REG0_OFF ((0x0U << 0))

bool dcm_mcusys_par_wrap_cpu_pll_div_0_dcm_is_on(void)
{
	bool ret = true;

	ret &= ((reg_read(CPU_PLLDIV_CFG0) &
		MCUSYS_PAR_WRAP_CPU_PLL_DIV_0_DCM_REG0_MASK) ==
		MCUSYS_PAR_WRAP_CPU_PLL_DIV_0_DCM_REG0_ON);

	return ret;
}

void dcm_mcusys_par_wrap_cpu_pll_div_0_dcm(int on)
{
	if (on) {
		reg_write(CPU_PLLDIV_CFG0,
			(reg_read(CPU_PLLDIV_CFG0) &
			~MCUSYS_PAR_WRAP_CPU_PLL_DIV_0_DCM_REG0_MASK) |
			MCUSYS_PAR_WRAP_CPU_PLL_DIV_0_DCM_REG0_ON);
	} else {
		reg_write(CPU_PLLDIV_CFG0,
			(reg_read(CPU_PLLDIV_CFG0) &
			~MCUSYS_PAR_WRAP_CPU_PLL_DIV_0_DCM_REG0_MASK) |
			MCUSYS_PAR_WRAP_CPU_PLL_DIV_0_DCM_REG0_OFF);
	}
}

#define MCUSYS_PAR_WRAP_CPU_PLL_DIV_1_DCM_REG0_MASK ((0x3U << 24))
#define MCUSYS_PAR_WRAP_CPU_PLL_DIV_1_DCM_REG0_ON ((0x3U << 24))
#define MCUSYS_PAR_WRAP_CPU_PLL_DIV_1_DCM_REG0_OFF ((0x0U << 0))

bool dcm_mcusys_par_wrap_cpu_pll_div_1_dcm_is_on(void)
{
	bool ret = true;

	ret &= ((reg_read(CPU_PLLDIV_CFG1) &
		MCUSYS_PAR_WRAP_CPU_PLL_DIV_1_DCM_REG0_MASK) ==
		MCUSYS_PAR_WRAP_CPU_PLL_DIV_1_DCM_REG0_ON);

	return ret;
}

void dcm_mcusys_par_wrap_cpu_pll_div_1_dcm(int on)
{
	if (on) {
		reg_write(CPU_PLLDIV_CFG1,
			(reg_read(CPU_PLLDIV_CFG1) &
			~MCUSYS_PAR_WRAP_CPU_PLL_DIV_1_DCM_REG0_MASK) |
			MCUSYS_PAR_WRAP_CPU_PLL_DIV_1_DCM_REG0_ON);
	} else {
		reg_write(CPU_PLLDIV_CFG1,
			(reg_read(CPU_PLLDIV_CFG1) &
			~MCUSYS_PAR_WRAP_CPU_PLL_DIV_1_DCM_REG0_MASK) |
			MCUSYS_PAR_WRAP_CPU_PLL_DIV_1_DCM_REG0_OFF);
	}
}

#define MCUSYS_PAR_WRAP_LAST_COR_IDLE_DCM_REG0_MASK ((0x1U << 31))
#define MCUSYS_PAR_WRAP_LAST_COR_IDLE_DCM_REG0_ON ((0x1U << 31))
#define MCUSYS_PAR_WRAP_LAST_COR_IDLE_DCM_REG0_OFF ((0x0U << 0))

bool dcm_mcusys_par_wrap_last_cor_idle_dcm_is_on(void)
{
	bool ret = true;

	ret &= ((reg_read(BUS_PLLDIV_CFG) &
		MCUSYS_PAR_WRAP_LAST_COR_IDLE_DCM_REG0_MASK) ==
		MCUSYS_PAR_WRAP_LAST_COR_IDLE_DCM_REG0_ON);

	return ret;
}

void dcm_mcusys_par_wrap_last_cor_idle_dcm(int on)
{
	if (on) {
		reg_write(BUS_PLLDIV_CFG,
			(reg_read(BUS_PLLDIV_CFG) &
			~MCUSYS_PAR_WRAP_LAST_COR_IDLE_DCM_REG0_MASK) |
			MCUSYS_PAR_WRAP_LAST_COR_IDLE_DCM_REG0_ON);
	} else {
		reg_write(BUS_PLLDIV_CFG,
			(reg_read(BUS_PLLDIV_CFG) &
			~MCUSYS_PAR_WRAP_LAST_COR_IDLE_DCM_REG0_MASK) |
			MCUSYS_PAR_WRAP_LAST_COR_IDLE_DCM_REG0_OFF);
	}
}

#define MCUSYS_PAR_WRAP_CPUBIU_DCM_REG0_MASK ((0xffffU << 0))
#define MCUSYS_PAR_WRAP_CPUBIU_DCM_REG0_ON ((0xffffU << 0))
#define MCUSYS_PAR_WRAP_CPUBIU_DCM_REG0_OFF ((0x0U << 0))

bool dcm_mcusys_par_wrap_cpubiu_dcm_is_on(void)
{
	bool ret = true;

	ret &= ((reg_read(MCSI_DCM0) &
		MCUSYS_PAR_WRAP_CPUBIU_DCM_REG0_MASK) ==
		MCUSYS_PAR_WRAP_CPUBIU_DCM_REG0_ON);

	return ret;
}

void dcm_mcusys_par_wrap_cpubiu_dcm(int on)
{
	if (on) {
		reg_write(MCSI_DCM0,
			(reg_read(MCSI_DCM0) &
			~MCUSYS_PAR_WRAP_CPUBIU_DCM_REG0_MASK) |
			MCUSYS_PAR_WRAP_CPUBIU_DCM_REG0_ON);
	} else {
		reg_write(MCSI_DCM0,
			(reg_read(MCSI_DCM0) &
			~MCUSYS_PAR_WRAP_CPUBIU_DCM_REG0_MASK) |
			MCUSYS_PAR_WRAP_CPUBIU_DCM_REG0_OFF);
	}
}

#define MCUSYS_PAR_WRAP_ADB_DCM_REG0_MASK ((0x1U << 17))
#define MCUSYS_PAR_WRAP_ADB_DCM_REG1_MASK ((0x4fU << 15))
#define MCUSYS_PAR_WRAP_ADB_DCM_REG2_MASK ((0xfU << 15))
#define MCUSYS_PAR_WRAP_ADB_DCM_REG0_ON ((0x1U << 17))
#define MCUSYS_PAR_WRAP_ADB_DCM_REG1_ON ((0x4fU << 15))
#define MCUSYS_PAR_WRAP_ADB_DCM_REG2_ON ((0xfU << 15))
#define MCUSYS_PAR_WRAP_ADB_DCM_REG0_OFF ((0x0U << 0))
#define MCUSYS_PAR_WRAP_ADB_DCM_REG1_OFF ((0x0U << 0))
#define MCUSYS_PAR_WRAP_ADB_DCM_REG2_OFF ((0x0U << 0))

bool dcm_mcusys_par_wrap_adb_dcm_is_on(void)
{
	bool ret = true;

	ret &= ((reg_read(MP_ADB_DCM_CFG0) &
		MCUSYS_PAR_WRAP_ADB_DCM_REG0_MASK) ==
		MCUSYS_PAR_WRAP_ADB_DCM_REG0_ON);
	ret &= ((reg_read(MP_ADB_DCM_CFG4) &
		MCUSYS_PAR_WRAP_ADB_DCM_REG1_MASK) ==
		MCUSYS_PAR_WRAP_ADB_DCM_REG1_ON);
	ret &= ((reg_read(MCUSYS_DCM_CFG0) &
		MCUSYS_PAR_WRAP_ADB_DCM_REG2_MASK) ==
		MCUSYS_PAR_WRAP_ADB_DCM_REG2_ON);

	return ret;
}

void dcm_mcusys_par_wrap_adb_dcm(int on)
{
	if (on) {
		reg_write(MP_ADB_DCM_CFG0,
			(reg_read(MP_ADB_DCM_CFG0) &
			~MCUSYS_PAR_WRAP_ADB_DCM_REG0_MASK) |
			MCUSYS_PAR_WRAP_ADB_DCM_REG0_ON);
		reg_write(MP_ADB_DCM_CFG4,
			(reg_read(MP_ADB_DCM_CFG4) &
			~MCUSYS_PAR_WRAP_ADB_DCM_REG1_MASK) |
			MCUSYS_PAR_WRAP_ADB_DCM_REG1_ON);
		reg_write(MCUSYS_DCM_CFG0,
			(reg_read(MCUSYS_DCM_CFG0) &
			~MCUSYS_PAR_WRAP_ADB_DCM_REG2_MASK) |
			MCUSYS_PAR_WRAP_ADB_DCM_REG2_ON);
	} else {
		reg_write(MP_ADB_DCM_CFG0,
			(reg_read(MP_ADB_DCM_CFG0) &
			~MCUSYS_PAR_WRAP_ADB_DCM_REG0_MASK) |
			MCUSYS_PAR_WRAP_ADB_DCM_REG0_OFF);
		reg_write(MP_ADB_DCM_CFG4,
			(reg_read(MP_ADB_DCM_CFG4) &
			~MCUSYS_PAR_WRAP_ADB_DCM_REG1_MASK) |
			MCUSYS_PAR_WRAP_ADB_DCM_REG1_OFF);
		reg_write(MCUSYS_DCM_CFG0,
			(reg_read(MCUSYS_DCM_CFG0) &
			~MCUSYS_PAR_WRAP_ADB_DCM_REG2_MASK) |
			MCUSYS_PAR_WRAP_ADB_DCM_REG2_OFF);
	}
}

#define MCUSYS_PAR_WRAP_MISC_DCM_REG0_MASK ((0x9U << 1))
#define MCUSYS_PAR_WRAP_MISC_DCM_REG0_ON ((0x9U << 1))
#define MCUSYS_PAR_WRAP_MISC_DCM_REG0_OFF ((0x0U << 0))

bool dcm_mcusys_par_wrap_misc_dcm_is_on(void)
{
	bool ret = true;

	ret &= ((reg_read(MP_MISC_DCM_CFG0) &
		MCUSYS_PAR_WRAP_MISC_DCM_REG0_MASK) ==
		MCUSYS_PAR_WRAP_MISC_DCM_REG0_ON);

	return ret;
}

void dcm_mcusys_par_wrap_misc_dcm(int on)
{
	if (on) {
		reg_write(MP_MISC_DCM_CFG0,
			(reg_read(MP_MISC_DCM_CFG0) &
			~MCUSYS_PAR_WRAP_MISC_DCM_REG0_MASK) |
			MCUSYS_PAR_WRAP_MISC_DCM_REG0_ON);
	} else {
		reg_write(MP_MISC_DCM_CFG0,
			(reg_read(MP_MISC_DCM_CFG0) &
			~MCUSYS_PAR_WRAP_MISC_DCM_REG0_MASK) |
			MCUSYS_PAR_WRAP_MISC_DCM_REG0_OFF);
	}
}

#define MCUSYS_PAR_WRAP_MP0_QDCM_REG0_MASK ((0x1U << 3))
#define MCUSYS_PAR_WRAP_MP0_QDCM_REG1_MASK ((0xfU << 0))
#define MCUSYS_PAR_WRAP_MP0_QDCM_REG0_ON ((0x1U << 3))
#define MCUSYS_PAR_WRAP_MP0_QDCM_REG1_ON ((0xfU << 0))
#define MCUSYS_PAR_WRAP_MP0_QDCM_REG0_OFF ((0x0U << 0))
#define MCUSYS_PAR_WRAP_MP0_QDCM_REG1_OFF ((0x0U << 0))

bool dcm_mcusys_par_wrap_mp0_qdcm_is_on(void)
{
	bool ret = true;

	ret &= ((reg_read(MP_MISC_DCM_CFG0) &
		MCUSYS_PAR_WRAP_MP0_QDCM_REG0_MASK) ==
		MCUSYS_PAR_WRAP_MP0_QDCM_REG0_ON);
	ret &= ((reg_read(MP0_DCM_CFG0) &
		MCUSYS_PAR_WRAP_MP0_QDCM_REG1_MASK) ==
		MCUSYS_PAR_WRAP_MP0_QDCM_REG1_ON);

	return ret;
}

void dcm_mcusys_par_wrap_mp0_qdcm(int on)
{
	if (on) {
		reg_write(MP_MISC_DCM_CFG0,
			(reg_read(MP_MISC_DCM_CFG0) &
			~MCUSYS_PAR_WRAP_MP0_QDCM_REG0_MASK) |
			MCUSYS_PAR_WRAP_MP0_QDCM_REG0_ON);
		reg_write(MP0_DCM_CFG0,
			(reg_read(MP0_DCM_CFG0) &
			~MCUSYS_PAR_WRAP_MP0_QDCM_REG1_MASK) |
			MCUSYS_PAR_WRAP_MP0_QDCM_REG1_ON);
	} else {
		reg_write(MP_MISC_DCM_CFG0,
			(reg_read(MP_MISC_DCM_CFG0) &
			~MCUSYS_PAR_WRAP_MP0_QDCM_REG0_MASK) |
			MCUSYS_PAR_WRAP_MP0_QDCM_REG0_OFF);
		reg_write(MP0_DCM_CFG0,
			(reg_read(MP0_DCM_CFG0) &
			~MCUSYS_PAR_WRAP_MP0_QDCM_REG1_MASK) |
			MCUSYS_PAR_WRAP_MP0_QDCM_REG1_OFF);
	}
}

#define MCUSYS_PAR_WRAP_APB_DCM_REG0_MASK ((0x3U << 5))
#define MCUSYS_PAR_WRAP_APB_DCM_REG1_MASK ((0x1U << 8))
#define MCUSYS_PAR_WRAP_APB_DCM_REG2_MASK ((0x1U << 16))
#define MCUSYS_PAR_WRAP_APB_DCM_REG0_ON ((0x3U << 5))
#define MCUSYS_PAR_WRAP_APB_DCM_REG1_ON ((0x1U << 8))
#define MCUSYS_PAR_WRAP_APB_DCM_REG2_ON ((0x1U << 16))
#define MCUSYS_PAR_WRAP_APB_DCM_REG0_OFF ((0x0U << 0))
#define MCUSYS_PAR_WRAP_APB_DCM_REG1_OFF ((0x0U << 0))
#define MCUSYS_PAR_WRAP_APB_DCM_REG2_OFF ((0x0U << 0))

bool dcm_mcusys_par_wrap_apb_dcm_is_on(void)
{
	bool ret = true;

	ret &= ((reg_read(MP_MISC_DCM_CFG0) &
		MCUSYS_PAR_WRAP_APB_DCM_REG0_MASK) ==
		MCUSYS_PAR_WRAP_APB_DCM_REG0_ON);
	ret &= ((reg_read(MCUSYS_DCM_CFG0) &
		MCUSYS_PAR_WRAP_APB_DCM_REG1_MASK) ==
		MCUSYS_PAR_WRAP_APB_DCM_REG1_ON);
	ret &= ((reg_read(MP0_DCM_CFG0) &
		MCUSYS_PAR_WRAP_APB_DCM_REG2_MASK) ==
		MCUSYS_PAR_WRAP_APB_DCM_REG2_ON);

	return ret;
}

void dcm_mcusys_par_wrap_apb_dcm(int on)
{
	if (on) {
		reg_write(MP_MISC_DCM_CFG0,
			(reg_read(MP_MISC_DCM_CFG0) &
			~MCUSYS_PAR_WRAP_APB_DCM_REG0_MASK) |
			MCUSYS_PAR_WRAP_APB_DCM_REG0_ON);
		reg_write(MCUSYS_DCM_CFG0,
			(reg_read(MCUSYS_DCM_CFG0) &
			~MCUSYS_PAR_WRAP_APB_DCM_REG1_MASK) |
			MCUSYS_PAR_WRAP_APB_DCM_REG1_ON);
		reg_write(MP0_DCM_CFG0,
			(reg_read(MP0_DCM_CFG0) &
			~MCUSYS_PAR_WRAP_APB_DCM_REG2_MASK) |
			MCUSYS_PAR_WRAP_APB_DCM_REG2_ON);
	} else {
		reg_write(MP_MISC_DCM_CFG0,
			(reg_read(MP_MISC_DCM_CFG0) &
			~MCUSYS_PAR_WRAP_APB_DCM_REG0_MASK) |
			MCUSYS_PAR_WRAP_APB_DCM_REG0_OFF);
		reg_write(MCUSYS_DCM_CFG0,
			(reg_read(MCUSYS_DCM_CFG0) &
			~MCUSYS_PAR_WRAP_APB_DCM_REG1_MASK) |
			MCUSYS_PAR_WRAP_APB_DCM_REG1_OFF);
		reg_write(MP0_DCM_CFG0,
			(reg_read(MP0_DCM_CFG0) &
			~MCUSYS_PAR_WRAP_APB_DCM_REG2_MASK) |
			MCUSYS_PAR_WRAP_APB_DCM_REG2_OFF);
	}
}

#define MCUSYS_PAR_WRAP_EMI_WFIFO_REG0_MASK ((0xfU << 0))
#define MCUSYS_PAR_WRAP_EMI_WFIFO_REG0_ON ((0xfU << 0))
#define MCUSYS_PAR_WRAP_EMI_WFIFO_REG0_OFF ((0x0U << 0))

bool dcm_mcusys_par_wrap_emi_wfifo_is_on(void)
{
	bool ret = true;

	ret &= ((reg_read(EMI_WFIFO) &
		MCUSYS_PAR_WRAP_EMI_WFIFO_REG0_MASK) ==
		MCUSYS_PAR_WRAP_EMI_WFIFO_REG0_ON);

	return ret;
}

void dcm_mcusys_par_wrap_emi_wfifo(int on)
{
	if (on) {
		reg_write(EMI_WFIFO,
			(reg_read(EMI_WFIFO) &
			~MCUSYS_PAR_WRAP_EMI_WFIFO_REG0_MASK) |
			MCUSYS_PAR_WRAP_EMI_WFIFO_REG0_ON);
	} else {
		reg_write(EMI_WFIFO,
			(reg_read(EMI_WFIFO) &
			~MCUSYS_PAR_WRAP_EMI_WFIFO_REG0_MASK) |
			MCUSYS_PAR_WRAP_EMI_WFIFO_REG0_OFF);
	}
}

#define MCUSYS_PAR_WRAP_CORE_STALL_DCM_REG0_MASK ((0x1U << 0))
#define MCUSYS_PAR_WRAP_CORE_STALL_DCM_REG0_ON ((0x1U << 0))
#define MCUSYS_PAR_WRAP_CORE_STALL_DCM_REG0_OFF ((0x0U << 0))

bool dcm_mcusys_par_wrap_core_stall_dcm_is_on(void)
{
	bool ret = true;

	ret &= ((reg_read(MP0_DCM_CFG7) &
		MCUSYS_PAR_WRAP_CORE_STALL_DCM_REG0_MASK) ==
		MCUSYS_PAR_WRAP_CORE_STALL_DCM_REG0_ON);

	return ret;
}

void dcm_mcusys_par_wrap_core_stall_dcm(int on)
{
	if (on) {
		reg_write(MP0_DCM_CFG7,
			(reg_read(MP0_DCM_CFG7) &
			~MCUSYS_PAR_WRAP_CORE_STALL_DCM_REG0_MASK) |
			MCUSYS_PAR_WRAP_CORE_STALL_DCM_REG0_ON);
	} else {
		reg_write(MP0_DCM_CFG7,
			(reg_read(MP0_DCM_CFG7) &
			~MCUSYS_PAR_WRAP_CORE_STALL_DCM_REG0_MASK) |
			MCUSYS_PAR_WRAP_CORE_STALL_DCM_REG0_OFF);
	}
}

#define MCUSYS_PAR_WRAP_FCM_STALL_DCM_REG0_MASK ((0x1U << 4))
#define MCUSYS_PAR_WRAP_FCM_STALL_DCM_REG0_ON ((0x1U << 4))
#define MCUSYS_PAR_WRAP_FCM_STALL_DCM_REG0_OFF ((0x0U << 0))

bool dcm_mcusys_par_wrap_fcm_stall_dcm_is_on(void)
{
	bool ret = true;

	ret &= ((reg_read(MP0_DCM_CFG7) &
		MCUSYS_PAR_WRAP_FCM_STALL_DCM_REG0_MASK) ==
		MCUSYS_PAR_WRAP_FCM_STALL_DCM_REG0_ON);

	return ret;
}

void dcm_mcusys_par_wrap_fcm_stall_dcm(int on)
{
	if (on) {
		reg_write(MP0_DCM_CFG7,
			(reg_read(MP0_DCM_CFG7) &
			~MCUSYS_PAR_WRAP_FCM_STALL_DCM_REG0_MASK) |
			MCUSYS_PAR_WRAP_FCM_STALL_DCM_REG0_ON);
	} else {
		reg_write(MP0_DCM_CFG7,
			(reg_read(MP0_DCM_CFG7) &
			~MCUSYS_PAR_WRAP_FCM_STALL_DCM_REG0_MASK) |
			MCUSYS_PAR_WRAP_FCM_STALL_DCM_REG0_OFF);
	}
}

#define INFRACFG_AO_INFRA_BUS_DCM_REG0_MASK ((0x40d07ffbU << 0))
#define INFRACFG_AO_INFRA_BUS_DCM_REG1_MASK ((0x1U << 0))
#define INFRACFG_AO_INFRA_BUS_DCM_REG0_ON ((0x40d00603U << 0))
#define INFRACFG_AO_INFRA_BUS_DCM_REG1_ON ((0x1U << 0))
#define INFRACFG_AO_INFRA_BUS_DCM_REG0_OFF ((0x61U << 4))
#define INFRACFG_AO_INFRA_BUS_DCM_REG1_OFF ((0x0U << 0))

bool dcm_infracfg_ao_infra_bus_dcm_is_on(void)
{
	bool ret = true;

	ret &= ((reg_read(INFRA_BUS_DCM_CTRL) &
		INFRACFG_AO_INFRA_BUS_DCM_REG0_MASK) ==
		INFRACFG_AO_INFRA_BUS_DCM_REG0_ON);
	ret &= ((reg_read(PERI_BUS_DCM_CTRL) &
		INFRACFG_AO_INFRA_BUS_DCM_REG1_MASK) ==
		INFRACFG_AO_INFRA_BUS_DCM_REG1_ON);

	return ret;
}

void dcm_infracfg_ao_infra_bus_dcm(int on)
{
	if (on) {
		reg_write(INFRA_BUS_DCM_CTRL,
			(reg_read(INFRA_BUS_DCM_CTRL) &
			~INFRACFG_AO_INFRA_BUS_DCM_REG0_MASK) |
			INFRACFG_AO_INFRA_BUS_DCM_REG0_ON);
		reg_write(PERI_BUS_DCM_CTRL,
			(reg_read(PERI_BUS_DCM_CTRL) &
			~INFRACFG_AO_INFRA_BUS_DCM_REG1_MASK) |
			INFRACFG_AO_INFRA_BUS_DCM_REG1_ON);

	} else {
		reg_write(INFRA_BUS_DCM_CTRL,
			(reg_read(INFRA_BUS_DCM_CTRL) &
			~INFRACFG_AO_INFRA_BUS_DCM_REG0_MASK) |
			INFRACFG_AO_INFRA_BUS_DCM_REG0_OFF);
		reg_write(PERI_BUS_DCM_CTRL,
			(reg_read(PERI_BUS_DCM_CTRL) &
			~INFRACFG_AO_INFRA_BUS_DCM_REG1_MASK) |
			INFRACFG_AO_INFRA_BUS_DCM_REG1_OFF);
	}
}

#define INFRACFG_AO_PERI_BUS_DCM_REG0_MASK ((0x1ffffdU << 1))
#define INFRACFG_AO_PERI_BUS_DCM_REG0_ON ((0x1fc1f1U << 1))
#define INFRACFG_AO_PERI_BUS_DCM_REG0_OFF ((0x7ffU << 4))

bool dcm_infracfg_ao_peri_bus_dcm_is_on(void)
{
	bool ret = true;

	ret &= ((reg_read(PERI_BUS_DCM_CTRL) &
		INFRACFG_AO_PERI_BUS_DCM_REG0_MASK) ==
		INFRACFG_AO_PERI_BUS_DCM_REG0_ON);

	return ret;
}

void dcm_infracfg_ao_peri_bus_dcm(int on)
{
	if (on) {
		reg_write(PERI_BUS_DCM_CTRL,
			(reg_read(PERI_BUS_DCM_CTRL) &
			~INFRACFG_AO_PERI_BUS_DCM_REG0_MASK) |
			INFRACFG_AO_PERI_BUS_DCM_REG0_ON);
	} else {
		reg_write(PERI_BUS_DCM_CTRL,
			(reg_read(PERI_BUS_DCM_CTRL) &
			~INFRACFG_AO_PERI_BUS_DCM_REG0_MASK) |
			INFRACFG_AO_PERI_BUS_DCM_REG0_OFF);
	}
}

#define INFRACFG_AO_PERI_MODULE_DCM_REG0_MASK ((0x5U << 29))
#define INFRACFG_AO_PERI_MODULE_DCM_REG0_ON ((0x5U << 29))
#define INFRACFG_AO_PERI_MODULE_DCM_REG0_OFF ((0x0U << 0))

bool dcm_infracfg_ao_peri_module_dcm_is_on(void)
{
	bool ret = true;

	ret &= ((reg_read(PERI_BUS_DCM_CTRL) &
		INFRACFG_AO_PERI_MODULE_DCM_REG0_MASK) ==
		INFRACFG_AO_PERI_MODULE_DCM_REG0_ON);

	return ret;
}

void dcm_infracfg_ao_peri_module_dcm(int on)
{
	if (on) {
		reg_write(PERI_BUS_DCM_CTRL,
			(reg_read(PERI_BUS_DCM_CTRL) &
			~INFRACFG_AO_PERI_MODULE_DCM_REG0_MASK) |
			INFRACFG_AO_PERI_MODULE_DCM_REG0_ON);
	} else {
		reg_write(PERI_BUS_DCM_CTRL,
			(reg_read(PERI_BUS_DCM_CTRL) &
			~INFRACFG_AO_PERI_MODULE_DCM_REG0_MASK) |
			INFRACFG_AO_PERI_MODULE_DCM_REG0_OFF);
	}
}

#define INFRACFG_AO_INFRA_RX_P2P_DCM_REG0_MASK ((0xfU << 0))
#define INFRACFG_AO_INFRA_RX_P2P_DCM_REG0_ON ((0x0U << 0))
#define INFRACFG_AO_INFRA_RX_P2P_DCM_REG0_OFF ((0xfU << 0))

bool dcm_infracfg_ao_infra_rx_p2p_dcm_is_on(void)
{
	bool ret = true;

	ret &= ((reg_read(P2P_RX_CLK_ON) &
		INFRACFG_AO_INFRA_RX_P2P_DCM_REG0_MASK) ==
		INFRACFG_AO_INFRA_RX_P2P_DCM_REG0_ON);

	return ret;
}

void dcm_infracfg_ao_infra_rx_p2p_dcm(int on)
{
	if (on) {
		reg_write(P2P_RX_CLK_ON,
			(reg_read(P2P_RX_CLK_ON) &
			~INFRACFG_AO_INFRA_RX_P2P_DCM_REG0_MASK) |
			INFRACFG_AO_INFRA_RX_P2P_DCM_REG0_ON);
	} else {
		reg_write(P2P_RX_CLK_ON,
			(reg_read(P2P_RX_CLK_ON) &
			~INFRACFG_AO_INFRA_RX_P2P_DCM_REG0_MASK) |
			INFRACFG_AO_INFRA_RX_P2P_DCM_REG0_OFF);
	}
}

#define INFRACFG_AO_AXIMEM_BUS_DCM_REG0_MASK ((0x7fU << 14))
#define INFRACFG_AO_AXIMEM_BUS_DCM_REG0_ON ((0x3U << 18))
#define INFRACFG_AO_AXIMEM_BUS_DCM_REG0_OFF ((0x5U << 18))

bool dcm_infracfg_ao_aximem_bus_dcm_is_on(void)
{
	bool ret = true;

	ret &= ((reg_read(INFRA_AXIMEM_IDLE_BIT_EN_0) &
		INFRACFG_AO_AXIMEM_BUS_DCM_REG0_MASK) ==
		INFRACFG_AO_AXIMEM_BUS_DCM_REG0_ON);

	return ret;
}

void dcm_infracfg_ao_aximem_bus_dcm(int on)
{
	if (on) {
		reg_write(INFRA_AXIMEM_IDLE_BIT_EN_0,
			(reg_read(INFRA_AXIMEM_IDLE_BIT_EN_0) &
			~INFRACFG_AO_AXIMEM_BUS_DCM_REG0_MASK) |
			INFRACFG_AO_AXIMEM_BUS_DCM_REG0_ON);
	} else {
		reg_write(INFRA_AXIMEM_IDLE_BIT_EN_0,
			(reg_read(INFRA_AXIMEM_IDLE_BIT_EN_0) &
			~INFRACFG_AO_AXIMEM_BUS_DCM_REG0_MASK) |
			INFRACFG_AO_AXIMEM_BUS_DCM_REG0_OFF);
	}
}

#define INFRA_AO_BCRM_INFRA_BUS_DCM_REG0_MASK ((0x1f0083U << 2))
#define INFRA_AO_BCRM_INFRA_BUS_DCM_REG0_ON ((0x83U << 2))
#define INFRA_AO_BCRM_INFRA_BUS_DCM_REG0_OFF ((0x3U << 2))

bool dcm_infra_ao_bcrm_infra_bus_dcm_is_on(void)
{
	bool ret = true;

	ret &= ((reg_read(VDNR_DCM_TOP_INFRA_PAR_BUS_u_INFRA_PAR_BUS_CTRL_0) &
		INFRA_AO_BCRM_INFRA_BUS_DCM_REG0_MASK) ==
		INFRA_AO_BCRM_INFRA_BUS_DCM_REG0_ON);

	return ret;
}

void dcm_infra_ao_bcrm_infra_bus_dcm(int on)
{
	if (on) {
		reg_write(VDNR_DCM_TOP_INFRA_PAR_BUS_u_INFRA_PAR_BUS_CTRL_0,
			(reg_read(VDNR_DCM_TOP_INFRA_PAR_BUS_u_INFRA_PAR_BUS_CTRL_0) &
			~INFRA_AO_BCRM_INFRA_BUS_DCM_REG0_MASK) |
			INFRA_AO_BCRM_INFRA_BUS_DCM_REG0_ON);
	} else {
		reg_write(VDNR_DCM_TOP_INFRA_PAR_BUS_u_INFRA_PAR_BUS_CTRL_0,
			(reg_read(VDNR_DCM_TOP_INFRA_PAR_BUS_u_INFRA_PAR_BUS_CTRL_0) &
			~INFRA_AO_BCRM_INFRA_BUS_DCM_REG0_MASK) |
			INFRA_AO_BCRM_INFRA_BUS_DCM_REG0_OFF);
	}
}

#define INFRA_AO_BCRM_INFRA_BUS_FMEM_SUB_DCM_REG0_MASK ((0x3U << 6))
#define INFRA_AO_BCRM_INFRA_BUS_FMEM_SUB_DCM_REG1_MASK ((0x7c01U << 5))
#define INFRA_AO_BCRM_INFRA_BUS_FMEM_SUB_DCM_REG0_ON ((0x3U << 6))
#define INFRA_AO_BCRM_INFRA_BUS_FMEM_SUB_DCM_REG1_ON ((0x1U << 5))
#define INFRA_AO_BCRM_INFRA_BUS_FMEM_SUB_DCM_REG0_OFF ((0x3U << 6))
#define INFRA_AO_BCRM_INFRA_BUS_FMEM_SUB_DCM_REG1_OFF ((0x0U << 0))

bool dcm_infra_ao_bcrm_infra_bus_fmem_sub_dcm_is_on(void)
{
	bool ret = true;

	ret &= ((reg_read(VDNR_DCM_TOP_INFRA_PAR_BUS_u_INFRA_PAR_BUS_CTRL_0) &
		INFRA_AO_BCRM_INFRA_BUS_FMEM_SUB_DCM_REG0_MASK) ==
		INFRA_AO_BCRM_INFRA_BUS_FMEM_SUB_DCM_REG0_ON);
	ret &= ((reg_read(VDNR_DCM_TOP_INFRA_PAR_BUS_u_INFRA_PAR_BUS_CTRL_3) &
		INFRA_AO_BCRM_INFRA_BUS_FMEM_SUB_DCM_REG1_MASK) ==
		INFRA_AO_BCRM_INFRA_BUS_FMEM_SUB_DCM_REG1_ON);

	return ret;
}

void dcm_infra_ao_bcrm_infra_bus_fmem_sub_dcm(int on)
{
	if (on) {
		reg_write(VDNR_DCM_TOP_INFRA_PAR_BUS_u_INFRA_PAR_BUS_CTRL_0,
			(reg_read(VDNR_DCM_TOP_INFRA_PAR_BUS_u_INFRA_PAR_BUS_CTRL_0) &
			~INFRA_AO_BCRM_INFRA_BUS_FMEM_SUB_DCM_REG0_MASK) |
			INFRA_AO_BCRM_INFRA_BUS_FMEM_SUB_DCM_REG0_ON);
		reg_write(VDNR_DCM_TOP_INFRA_PAR_BUS_u_INFRA_PAR_BUS_CTRL_3,
			(reg_read(VDNR_DCM_TOP_INFRA_PAR_BUS_u_INFRA_PAR_BUS_CTRL_3) &
			~INFRA_AO_BCRM_INFRA_BUS_FMEM_SUB_DCM_REG1_MASK) |
			INFRA_AO_BCRM_INFRA_BUS_FMEM_SUB_DCM_REG1_ON);
	} else {
		reg_write(VDNR_DCM_TOP_INFRA_PAR_BUS_u_INFRA_PAR_BUS_CTRL_0,
			(reg_read(VDNR_DCM_TOP_INFRA_PAR_BUS_u_INFRA_PAR_BUS_CTRL_0) &
			~INFRA_AO_BCRM_INFRA_BUS_FMEM_SUB_DCM_REG0_MASK) |
			INFRA_AO_BCRM_INFRA_BUS_FMEM_SUB_DCM_REG0_OFF);
		reg_write(VDNR_DCM_TOP_INFRA_PAR_BUS_u_INFRA_PAR_BUS_CTRL_3,
			(reg_read(VDNR_DCM_TOP_INFRA_PAR_BUS_u_INFRA_PAR_BUS_CTRL_3) &
			~INFRA_AO_BCRM_INFRA_BUS_FMEM_SUB_DCM_REG1_MASK) |
			INFRA_AO_BCRM_INFRA_BUS_FMEM_SUB_DCM_REG1_OFF);
	}
}

#define PERI_AO_BCRM_PERI_BUS_DCM_REG0_MASK ((0x149U << 4))
#define PERI_AO_BCRM_PERI_BUS_DCM_REG1_MASK ((0x1U << 17))
#define PERI_AO_BCRM_PERI_BUS_DCM_REG2_MASK ((0x1U << 13))
#define PERI_AO_BCRM_PERI_BUS_DCM_REG0_ON ((0x149U << 4))
#define PERI_AO_BCRM_PERI_BUS_DCM_REG1_ON ((0x1U << 17))
#define PERI_AO_BCRM_PERI_BUS_DCM_REG2_ON ((0x1U << 13))
#define PERI_AO_BCRM_PERI_BUS_DCM_REG0_OFF ((0x0U << 0))
#define PERI_AO_BCRM_PERI_BUS_DCM_REG1_OFF ((0x0U << 0))
#define PERI_AO_BCRM_PERI_BUS_DCM_REG2_OFF ((0x0U << 0))

bool dcm_peri_ao_bcrm_peri_bus_dcm_is_on(void)
{
	bool ret = true;

	ret &= ((reg_read(VDNR_DCM_TOP_PERI_PAR_BUS_u_PERI_PAR_BUS_CTRL_0) &
		PERI_AO_BCRM_PERI_BUS_DCM_REG0_MASK) ==
		PERI_AO_BCRM_PERI_BUS_DCM_REG0_ON);
	ret &= ((reg_read(VDNR_DCM_TOP_PERI_PAR_BUS_u_PERI_PAR_BUS_CTRL_1) &
		PERI_AO_BCRM_PERI_BUS_DCM_REG1_MASK) ==
		PERI_AO_BCRM_PERI_BUS_DCM_REG1_ON);
	ret &= ((reg_read(VDNR_DCM_TOP_PERI_PAR_BUS_u_PERI_PAR_BUS_CTRL_2) &
		PERI_AO_BCRM_PERI_BUS_DCM_REG2_MASK) ==
		PERI_AO_BCRM_PERI_BUS_DCM_REG2_ON);

	return ret;
}

void dcm_peri_ao_bcrm_peri_bus_dcm(int on)
{
	if (on) {
		reg_write(VDNR_DCM_TOP_PERI_PAR_BUS_u_PERI_PAR_BUS_CTRL_0,
			(reg_read(VDNR_DCM_TOP_PERI_PAR_BUS_u_PERI_PAR_BUS_CTRL_0) &
			~PERI_AO_BCRM_PERI_BUS_DCM_REG0_MASK) |
			PERI_AO_BCRM_PERI_BUS_DCM_REG0_ON);
		reg_write(VDNR_DCM_TOP_PERI_PAR_BUS_u_PERI_PAR_BUS_CTRL_1,
			(reg_read(VDNR_DCM_TOP_PERI_PAR_BUS_u_PERI_PAR_BUS_CTRL_1) &
			~PERI_AO_BCRM_PERI_BUS_DCM_REG1_MASK) |
			PERI_AO_BCRM_PERI_BUS_DCM_REG1_ON);
		reg_write(VDNR_DCM_TOP_PERI_PAR_BUS_u_PERI_PAR_BUS_CTRL_2,
			(reg_read(VDNR_DCM_TOP_PERI_PAR_BUS_u_PERI_PAR_BUS_CTRL_2) &
			~PERI_AO_BCRM_PERI_BUS_DCM_REG2_MASK) |
			PERI_AO_BCRM_PERI_BUS_DCM_REG2_ON);
	} else {
		reg_write(VDNR_DCM_TOP_PERI_PAR_BUS_u_PERI_PAR_BUS_CTRL_0,
			(reg_read(VDNR_DCM_TOP_PERI_PAR_BUS_u_PERI_PAR_BUS_CTRL_0) &
			~PERI_AO_BCRM_PERI_BUS_DCM_REG0_MASK) |
			PERI_AO_BCRM_PERI_BUS_DCM_REG0_OFF);
		reg_write(VDNR_DCM_TOP_PERI_PAR_BUS_u_PERI_PAR_BUS_CTRL_1,
			(reg_read(VDNR_DCM_TOP_PERI_PAR_BUS_u_PERI_PAR_BUS_CTRL_1) &
			~PERI_AO_BCRM_PERI_BUS_DCM_REG1_MASK) |
			PERI_AO_BCRM_PERI_BUS_DCM_REG1_OFF);
		reg_write(VDNR_DCM_TOP_PERI_PAR_BUS_u_PERI_PAR_BUS_CTRL_2,
			(reg_read(VDNR_DCM_TOP_PERI_PAR_BUS_u_PERI_PAR_BUS_CTRL_2) &
			~PERI_AO_BCRM_PERI_BUS_DCM_REG2_MASK) |
			PERI_AO_BCRM_PERI_BUS_DCM_REG2_OFF);
	}
}

#define VLP_AO_BCRM_VLP_BUS_DCM_REG0_MASK ((0x3e013U << 1))
#define VLP_AO_BCRM_VLP_BUS_DCM_REG0_ON ((0x13U << 1))
#define VLP_AO_BCRM_VLP_BUS_DCM_REG0_OFF ((0x3U << 1))

bool dcm_vlp_ao_bcrm_vlp_bus_dcm_is_on(void)
{
	bool ret = true;

	ret &= ((reg_read(VDNR_PWR_PROT_VLP_PAR_BUS_u_spm_CTRL_0) &
		VLP_AO_BCRM_VLP_BUS_DCM_REG0_MASK) ==
		VLP_AO_BCRM_VLP_BUS_DCM_REG0_ON);

	return ret;
}

void dcm_vlp_ao_bcrm_vlp_bus_dcm(int on)
{
	if (on) {
		reg_write(VDNR_PWR_PROT_VLP_PAR_BUS_u_spm_CTRL_0,
			(reg_read(VDNR_PWR_PROT_VLP_PAR_BUS_u_spm_CTRL_0) &
			~VLP_AO_BCRM_VLP_BUS_DCM_REG0_MASK) |
			VLP_AO_BCRM_VLP_BUS_DCM_REG0_ON);
	} else {
		reg_write(VDNR_PWR_PROT_VLP_PAR_BUS_u_spm_CTRL_0,
			(reg_read(VDNR_PWR_PROT_VLP_PAR_BUS_u_spm_CTRL_0) &
			~VLP_AO_BCRM_VLP_BUS_DCM_REG0_MASK) |
			VLP_AO_BCRM_VLP_BUS_DCM_REG0_OFF);
	}
}

