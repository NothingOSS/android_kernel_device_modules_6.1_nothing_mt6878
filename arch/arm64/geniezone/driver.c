// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2023 MediaTek Inc.
 */

#include <linux/clocksource.h>
#include <linux/gzvm_drv.h>
#include "gzvm_arch_common.h"

struct timecycle clock_scale_factor;

int gzvm_arch_drv_init(void)
{
	/* clock_scale_factor init mult shift */
	clocks_calc_mult_shift(&clock_scale_factor.mult,
			       &clock_scale_factor.shift,
			       arch_timer_get_cntfrq(),
			       NSEC_PER_SEC,
			       10);

	return 0;
}

void gzvm_arch_drv_exit(void)
{
}
