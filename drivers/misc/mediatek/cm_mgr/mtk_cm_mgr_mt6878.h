/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2023 MediaTek Inc.
 */

#ifndef __MTK_CM_MGR_PLATFORM_H__
#define __MTK_CM_MGR_PLATFORM_H__

#define CREATE_TRACE_POINTS
#include "mtk_cm_mgr_events_mt6878.h"

#if IS_ENABLED(CONFIG_MTK_DRAMC)
#include <soc/mediatek/dramc.h>
#endif /* CONFIG_MTK_DRAMC */

#define PERF_TIME 100
#define CM_IS_DRAM_TYPE_LP_FOUR_ALL(d) ((d == TYPE_LPDDR4) || (d == TYPE_LPDDR4X) || (d == TYPE_LPDDR4P))
#define CM_IS_DRAM_TYPE_LP_FIVE_ALL(d) ((d == TYPE_LPDDR5) || (d == TYPE_LPDDR5X))
enum {
	CM_MGR_LP4X = 0,
	CM_MGR_LP5 = 1,
	CM_MGR_MAX,
};

enum cm_mgr_cpu_cluster {
	CM_MGR_L = 0,
	CM_MGR_B,
	CM_MGR_CPU_CLUSTER,
};

#endif /* __MTK_CM_MGR_PLATFORM_H__ */
