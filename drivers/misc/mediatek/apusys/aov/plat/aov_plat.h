/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2022 MediaTek Inc.
 */

#ifndef __AOV_PLAT_H__
#define __AOV_PLAT_H__

#include <linux/types.h>

int aov_plat_init(unsigned int version);

void aov_plat_exit(unsigned int version);

#endif // __AOV_PLAT_H__
