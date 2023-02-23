// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2022 MediaTek Inc.
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <asm/io.h>
#include <linux/cpufreq.h>
#include <linux/kthread.h>
#include <linux/irq_work.h>
#include "sugov/cpufreq.h"
#include "dsu_interface.h"

static void __iomem *clkg_sram_base_addr;
static void __iomem *wlc_sram_base_addr;
static void __iomem *l3ctl_sram_base_addr;

void __iomem *get_clkg_sram_base_addr(void)
{
	return clkg_sram_base_addr;
}
EXPORT_SYMBOL_GPL(get_clkg_sram_base_addr);

void __iomem *get_l3ctl_sram_base_addr(void)
{
	return l3ctl_sram_base_addr;
}
EXPORT_SYMBOL_GPL(get_l3ctl_sram_base_addr);

void dsu_pwr_swpm_init(void)
{
	l3ctl_sram_base_addr = ioremap(L3CTL_SRAM_BASE, L3CTL_SRAM_SIZE);
	wlc_sram_base_addr = ioremap(WLC_SRAM_ADDR, WLC_SRAM_SIZE);
	clkg_sram_base_addr = ioremap(CLKG_SRAM_BASE, CLKG_SRAM_SIZE);
}

unsigned int get_pelt_dsu_bw(void)
{
	unsigned int pelt_dsu_bw;

	pelt_dsu_bw = ioread32(l3ctl_sram_base_addr+PELT_DSU_BW_OFFSET);

	return pelt_dsu_bw;
}
EXPORT_SYMBOL_GPL(get_pelt_dsu_bw);

unsigned int get_pelt_emi_bw(void)
{
	unsigned int pelt_emi_bw;

	pelt_emi_bw = ioread32(l3ctl_sram_base_addr+PELT_EMI_BW_OFFSET);

	return pelt_emi_bw;
}
EXPORT_SYMBOL_GPL(get_pelt_emi_bw);

/* write pelt weight and pelt sum */
void update_pelt_data(unsigned int pelt_weight, unsigned int pelt_sum)
{
	iowrite32(pelt_sum, l3ctl_sram_base_addr+PELT_SUM_OFFSET);
	iowrite32(pelt_weight, l3ctl_sram_base_addr+PELT_WET_OFFSET);
}
EXPORT_SYMBOL_GPL(update_pelt_data);

/* get workload type */
unsigned int get_wl(unsigned int wl_idx)
{
	unsigned int wl_type;
	unsigned int offset;

	wl_type = ioread32(wlc_sram_base_addr);
	offset = wl_idx * 0x8;
	wl_type = (wl_type >> offset) & 0xff;

	return wl_type;
}

