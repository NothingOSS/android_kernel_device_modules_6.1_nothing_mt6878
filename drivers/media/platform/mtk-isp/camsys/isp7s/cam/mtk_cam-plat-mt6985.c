// SPDX-License-Identifier: GPL-2.0
//
// Copyright (c) 2019 MediaTek Inc.

#include <linux/types.h>

#include "mtk_cam-plat.h"

static const struct plat_data_meta mt6985_plat_meta = {
	.get_version = NULL,
	.get_size = NULL,
};

struct camsys_platform_data mt6985_data = {
	.platform = "mt6985",
	.meta = &mt6985_plat_meta,
	.hw = NULL,
};
