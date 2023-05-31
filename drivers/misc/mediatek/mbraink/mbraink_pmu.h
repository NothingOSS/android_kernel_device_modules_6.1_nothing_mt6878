/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */
#ifndef MBRAINK_PMU_H
#define MBRAINK_PMU_H

#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/io.h>
#include "mbraink_ioctl_struct_def.h"
//int mbraink_memory_getDdrInfo(struct mbraink_memory_ddrInfo *pMemoryDdrInfo);

//#define WLC_SRAM_BASE       0x0003f000//for wl calssifier
//#define AP_VIEW_TCM_BASE    0x0c080000//ap view tcm base
//#define WLC_SRAM_ADDR       (AP_VIEW_TCM_BASE+WLC_SRAM_BASE)

//#define CLKG_SRAM_SIZE  0xC00
//#define WLC_SRAM_SIZE   0x10
//#define L3CTL_SRAM_SIZE 0x400

//tcm
#define AP_VIEW_TCM_BASE    0x0c080000  //ap view tcm base
#define GENERAL_TCM_BASE    0x0005d350  //tcm base
#define PMU_DATA_TCM_BASE   0x0005bf50  //pmu data offset base in tcm

#define GENERAL_TCM_ADDR    (AP_VIEW_TCM_BASE+GENERAL_TCM_BASE)
#define PMU_DATA_TCM_ADDR   (AP_VIEW_TCM_BASE+PMU_DATA_TCM_BASE)

#define GENERAL_TCM_SIZE    0x1FFF
#define PMU_DATA_TCM_SIZE   0xFF

#define MBRAIN_ENABLE_TCM_OFFSET    0x1330

void init_pmu_keep_data(void);
void uninit_pmu_keep_data(void);
int mbraink_pmu_init(void);
int mbraink_pmu_uninit(void);
int mbraink_enable_pmu_inst_spec(bool enable);
int mbraink_update_pmu_inst_spec(void);
int mbraink_get_pmu_inst_spec(struct mbraink_pmu_info *pmuInfo);

#endif /*end of MBRAINK_PMU_H*/
