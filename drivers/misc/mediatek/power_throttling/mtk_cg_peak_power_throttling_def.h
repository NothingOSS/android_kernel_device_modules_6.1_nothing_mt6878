/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2023 MediaTek Inc.
 * Author: Clouds Lee <clouds.lee@mediatek.com>
 */

#ifndef _MTK_CG_PEAK_POWER_THROTTLING_DEF_H_
#define _MTK_CG_PEAK_POWER_THROTTLING_DEF_H_

/*
 * ========================================================
 * Definitions
 * ========================================================
 */
#define PPT_SRAM_INIT_VALUE (0xD903D903) //55555(short) & 55555(short)

/*
 * ========================================================
 * Thermal SRAM (temparary)
 * ========================================================
 */
#define THERMAL_CSRAM_BASE (0x00114000)
#define THERMAL_CSRAM_SIZE (0x400)
#define THERMAL_CSRAM_CTRL_BASE (THERMAL_CSRAM_BASE + 0x360)

#define DLPT_CSRAM_BASE (0x00116400)
#define DLPT_CSRAM_SIZE (0x1400) //5KB
#define DLPT_CSRAM_CTRL_RESERVED_SIZE                                          \
	(128) //reserve last 128B for control purpose
#define DLPT_CSRAM_CTRL_BASE                                                   \
	(DLPT_CSRAM_BASE + DLPT_CSRAM_SIZE - DLPT_CSRAM_CTRL_RESERVED_SIZE)

#define DLPT_DRAM_BASE (0x8C01FE08)

/*
 * ========================================================
 * [Kernel]
 * ========================================================
 */
#if defined(__KERNEL__)
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/printk.h>
#include <linux/string.h>
#include "mtk_cg_peak_power_throttling_plat_kl.h"

extern uintptr_t THERMAL_CSRAM_BASE_REMAP;
extern uintptr_t THERMAL_CSRAM_CTRL_BASE_REMAP;
extern uintptr_t DLPT_CSRAM_BASE_REMAP;
extern uintptr_t DLPT_CSRAM_CTRL_BASE_REMAP;

extern void cg_ppt_thermal_sram_remap(uintptr_t virtual_addr);
extern void cg_ppt_dlpt_sram_remap(uintptr_t virtual_addr);

#define pp_print(fmt, ...) pr_info(fmt, ##__VA_ARGS__)

/*
 * ========================================================
 * [EB]
 * ========================================================
 */
#elif defined(CFG_CPU_PEAKPOWERTHROTTLING) ||                                  \
	defined(CFG_GPU_PEAKPOWERTHROTTLING)

#include "mtk_cg_peak_power_throttling_plat_eb.h"
#include "mt_printf.h"

#define THERMAL_CSRAM_BASE_REMAP (THERMAL_CSRAM_BASE | 0x10000000)
#define THERMAL_CSRAM_CTRL_BASE_REMAP (THERMAL_CSRAM_CTRL_BASE | 0x10000000)
#define DLPT_CSRAM_BASE_REMAP (DLPT_CSRAM_BASE | 0x10000000)
#define DLPT_CSRAM_CTRL_BASE_REMAP (DLPT_CSRAM_CTRL_BASE | 0x10000000)


#define pp_print(fmt, ...) PRINTF_I(fmt, ##__VA_ARGS__)

/*
 * ========================================================
 * [UNKNOWN]
 * ========================================================
 */
#else
#pragma message("pp def compiled as UNKNOWN")

#endif

/*
 * ========================================================
 * Common
 * ========================================================
 */

/*
 * ...................................
 * Thermal SRAM variables
 * ...................................
 */

#define G2C_B_PP_LMT_FREQ (THERMAL_CSRAM_BASE_REMAP + 0x360)
#define G2C_B_PP_LMT_FREQ_ACK (THERMAL_CSRAM_BASE_REMAP + 0x364)
#define G2C_M_PP_LMT_FREQ (THERMAL_CSRAM_BASE_REMAP + 0x368)
#define G2C_M_PP_LMT_FREQ_ACK (THERMAL_CSRAM_BASE_REMAP + 0x36C)
#define G2C_L_PP_LMT_FREQ (THERMAL_CSRAM_BASE_REMAP + 0x370)
#define G2C_L_PP_LMT_FREQ_ACK (THERMAL_CSRAM_BASE_REMAP + 0x374)
#define CPU_LOW_KEY (THERMAL_CSRAM_BASE_REMAP + 0x378)
#define G2C_PP_LMT_FREQ_ACK_TIMEOUT (THERMAL_CSRAM_BASE_REMAP + 0x37C)

struct ThermalCsramCtrlBlock {
	int g2c_b_pp_lmt_freq;
	int g2c_b_pp_lmt_freq_ack;
	int g2c_m_pp_lmt_freq;
	int g2c_m_pp_lmt_freq_ack;
	int g2c_l_pp_lmt_freq;
	int g2c_l_pp_lmt_freq_ack;
	int cpu_low_key;
	int g2c_pp_lmt_freq_ack_timeout;
};

/*
 * ...................................
 * DLPT DRAM Control Block
 * ...................................
 */
struct DlptDramCtrlBlock {
	int modem_peak_power_mw;
	int ap2md_ack;
	int wifi_peak_power_mw;
	int ap2wifi_ack;
};

/*
 * ...................................
 * DLPT SRAM Control Block
 * ...................................
 */
struct DlptCsramCtrlBlock {
	/* mode 0:CG no peak at same time 1:peak power budget 2:OFF*/
	int peak_power_budget_mode; /* 1*/
	int cg_min_power_mw; /* 2*/
	int vsys_power_budget_mw; /* 3*/ /*include modem, wifi*/
	int vsys_power_budget_ack; /* 4*/
	int flash_peak_power_mw; /* 5*/
	int audio_peak_power_mw; /* 6*/
	int camera_peak_power_mw; /* 7*/
	int apu_peak_power_mw; /* 8*/
	int display_lcd_peak_power_mw; /* 9*/
	int dram_peak_power_mw; /*10*/
	int modem_peak_power_mw_shadow; /*11*/
	int wifi_peak_power_mw_shadow; /*12*/
	int reserved_4; /*13*/
	int reserved_5; /*14*/
	int apu_peak_power_ack; /*15*/
};

/*
 * ========================================================
 * Macros
 * ========================================================
 */
// #ifndef DRV_WriteReg32
// #define DRV_WriteReg32(addr, data) \
// ((*(unsigned int *)(addr)) = (unsigned int)(data))
// #endif
// #ifndef DRV_Reg32
// #define DRV_Reg32(addr) (*(unsigned int *)(addr))
// #endif

/*
 * ========================================================
 * Functions
 * ========================================================
 */
struct ThermalCsramCtrlBlock *thermal_csram_ctrl_block_get(void);
void thermal_csram_ctrl_block_release(
	struct ThermalCsramCtrlBlock *remap_status_base);
struct DlptDramCtrlBlock *dlpt_dram_ctrl_block_get(void);
void dlpt_dram_ctrl_block_release(struct DlptDramCtrlBlock *remap_status_base);
struct DlptCsramCtrlBlock *dlpt_csram_ctrl_block_get(void);
void dlpt_csram_ctrl_block_release(
	struct DlptCsramCtrlBlock *remap_status_base);

#endif /*_MTK_CG_PEAK_POWER_THROTTLING_DEF_H_*/
