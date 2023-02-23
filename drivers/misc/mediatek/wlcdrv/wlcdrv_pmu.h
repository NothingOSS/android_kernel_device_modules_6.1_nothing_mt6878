/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2022 MediaTek Inc.
 */


#ifndef __WLCDRV_PMU_H__
#define __WLCDRV_PMU_H__

int wlc_mcu_pmu_init(void);
int wlc_mcu_pmu_deinit(void);
int wlc_mcu_pmu_start(void);
int wlc_mcu_pmu_stop(void);

int wlc_sampler_start(void);
int wlc_sampler_stop(void);

#endif /* __WLCDRV_PMU_H__ */
