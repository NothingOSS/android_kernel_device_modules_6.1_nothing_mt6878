/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2023 MediaTek Inc.
 */


#ifndef __LPM_SPM_COMM_H__
#define __LPM_SPM_COMM_H__

#include <lpm_dbg_common_v2.h>

struct lpm_spm_wake_status {
	u32 r12;			/* SPM_BK_WAKE_EVENT */
	u32 r12_ext;		/* SPM_WAKEUP_EXT_STA */
	u32 raw_sta;		/* SPM_WAKEUP_STA */
	u32 raw_ext_sta;	/* SPM_WAKEUP_EXT_STA */
	u32 md32pcm_wakeup_sta;/* MD32CPM_WAKEUP_STA */
	u32 md32pcm_event_sta;/* MD32PCM_EVENT_STA */
	u32 wake_misc;		/* SPM_BK_WAKE_MISC */
	u32 timer_out;		/* SPM_BK_PCM_TIMER */
	u32 r13;			/* PCM_REG13_DATA */
	u32 idle_sta;		/* SUBSYS_IDLE_STA */
	u32 req_sta0;		/* SRC_REQ_STA_0 */
	u32 req_sta1;		/* SRC_REQ_STA_1 */
	u32 req_sta2;		/* SRC_REQ_STA_2 */
	u32 req_sta3;		/* SRC_REQ_STA_3 */
	u32 req_sta4;		/* SRC_REQ_STA_4 */
	u32 req_sta5;		/* SRC_REQ_STA_5 */
	u32 req_sta6;		/* SRC_REQ_STA_6 */
	u32 req_sta7;		/* SRC_REQ_STA_7 */
	u32 req_sta8;		/* SRC_REQ_STA_7 */
	u32 req_sta9;		/* SRC_REQ_STA_9 */
	u32 req_sta10;		/* SRC_REQ_STA_10 */
	u32 req_sta11;		/* SRC_REQ_STA_11 */
	u32 cg_check_sta;	/* SPM_CG_CHECK_STA */
	u32 debug_flag;		/* PCM_WDT_LATCH_SPARE_0 */
	u32 debug_flag1;	/* PCM_WDT_LATCH_SPARE_1 */
	u32 debug_spare5;	/* PCM_WDT_LATCH_SPARE_5 */
	u32 debug_spare6;	/* PCM_WDT_LATCH_SPARE_6 */
	u32 b_sw_flag0;		/* SPM_SW_RSV_7 */
	u32 b_sw_flag1;		/* SPM_SW_RSV_8 */
	u32 isr;			/* SPM_IRQ_STA */
	u32 sw_flag0;		/* SPM_SW_FLAG_0 */
	u32 sw_flag1;		/* SPM_SW_FLAG_1 */
	u32 clk_settle;		/* SPM_CLK_SETTLE */
	u32 src_req;	/* SPM_SRC_REQ */
	u32 log_index;
	u32 is_abort;
	u32 sw_rsv_0; /* SPM_SW_RSV_0 */
	u32 sw_rsv_1; /* SPM_SW_RSV_1 */
	u32 sw_rsv_2; /* SPM_SW_RSV_2 */
	u32 sw_rsv_3; /* SPM_SW_RSV_3 */
	u32 sw_rsv_4; /* SPM_SW_RSV_4 */
	u32 sw_rsv_5; /* SPM_SW_RSV_5 */
	u32 sw_rsv_6; /* SPM_SW_RSV_6 */
	u32 sw_rsv_7; /* SPM_SW_RSV_7 */
	u32 sw_rsv_8; /* SPM_SW_RSV_8 */
};

#endif
