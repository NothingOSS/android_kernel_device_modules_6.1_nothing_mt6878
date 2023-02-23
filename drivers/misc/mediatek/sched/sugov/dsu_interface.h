/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2022 MediaTek Inc.
 */
#ifndef _DSU_INTERFACE_H
#define _DSU_INTERFACE_H

//SYSRAM
#define CLKG_SRAM_BASE      0x00114400//for leakage
#define L3CTL_SRAM_BASE     0x00113400//for l3ctl

//tcm
#define WLC_SRAM_BASE       0x0003f000//for wl calssifier
#define AP_VIEW_TCM_BASE    0x0c080000//ap view tcm base
#define WLC_SRAM_ADDR       (AP_VIEW_TCM_BASE+WLC_SRAM_BASE)

#define CLKG_SRAM_SIZE  0xC00
#define WLC_SRAM_SIZE   0x10
#define L3CTL_SRAM_SIZE 0x400
#define PELT_DSU_BW_OFFSET 0x54
#define PELT_EMI_BW_OFFSET 0x5c
#define PELT_SUM_OFFSET 0x60
#define PELT_WET_OFFSET 0x64
#define PELT_SUM_ADDR   (L3CTL_SRAM_BASE+PELT_SUM_OFFSET)
#define PELT_WET_ADDR   (L3CTL_SRAM_BASE+PELT_WET_OFFSET)
#define DSU_DVFS_VOTE_EAS_1 0x20

void __iomem *get_clkg_sram_base_addr(void);
void __iomem *get_l3ctl_sram_base_addr(void);
unsigned int get_wl(unsigned int wl_idx);
void update_pelt_data(unsigned int pelt_weight, unsigned int pelt_sum);
void dsu_pwr_swpm_init(void);
unsigned int get_pelt_dsu_bw(void);
unsigned int get_pelt_emi_bw(void);
#endif
