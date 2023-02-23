/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2022 MediaTek Inc.
 * Author: Wendy-ST Lin <wendy-st.lin@mediatek.com>
 */
#ifndef MMQOS_GLOBAL_H
#define MMQOS_GLOBAL_H

#define MMQOS_DBG(fmt, args...) \
	pr_notice("%s:%d: "fmt"\n", __func__, __LINE__, ##args)
#define MMQOS_ERR(fmt, args...) \
	pr_notice("error: %s:%d: "fmt"\n", __func__, __LINE__, ##args)

enum mmqos_state_level {
	MMQOS_DISABLE = 0,
	OSTD_ENABLE = BIT(0),
	BWL_ENABLE = BIT(1),
	DVFSRC_ENABLE = BIT(2),
	COMM_OSTDL_ENABLE = BIT(3),
	DISP_BY_LARB_ENABLE = BIT(4),
	VCP_ENABLE = BIT(5),
	MMQOS_ENABLE = BIT(0) | BIT(2),
};
extern u32 mmqos_state;

enum mmqos_log_level {
	log_bw = 0,
	log_comm_freq,
	log_v2_dbg,
	log_vcp_pwr,
	log_ipi,
};
extern u32 log_level;

#endif /* MMQOS_GLOBAL_H */
