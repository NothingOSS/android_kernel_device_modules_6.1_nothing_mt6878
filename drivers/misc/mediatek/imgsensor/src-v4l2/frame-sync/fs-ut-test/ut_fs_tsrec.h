/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2022 MediaTek Inc.
 */

#ifndef __UT_FS_TSREC_H__
#define __UT_FS_TSREC_H__


/******************************************************************************
 * for unit test (struct/enum) - TSREC
 *****************************************************************************/
/* sync from imgsensor-user.h */
enum {
	PAD_SINK = 0,
	PAD_SRC_RAW0,
	PAD_SRC_RAW1,
	PAD_SRC_RAW2,
	PAD_SRC_RAW_W0,
	PAD_SRC_RAW_W1,
	PAD_SRC_RAW_W2,
	PAD_SRC_RAW_EXT0,
	PAD_SRC_PDAF0,
	PAD_SRC_PDAF1,
	PAD_SRC_PDAF2,
	PAD_SRC_PDAF3,
	PAD_SRC_PDAF4,
	PAD_SRC_PDAF5,
	PAD_SRC_PDAF6,
	PAD_SRC_HDR0,
	PAD_SRC_HDR1,
	PAD_SRC_HDR2,
	PAD_SRC_GENERAL0,
	PAD_MAXCNT,
	PAD_ERR = 0xffff,
};

enum mtk_cam_seninf_tsrec_exp_id {
	TSREC_EXP_ID_AUTO = 0,
	TSREC_EXP_ID_1,
	TSREC_EXP_ID_2,
	TSREC_EXP_ID_3,
};


/* sync from linux/irqreturn.h for UT testing */
enum irqreturn {
	IRQ_NONE		= (0 << 0),
	IRQ_HANDLED		= (1 << 0),
	IRQ_WAKE_THREAD		= (1 << 1),
};
// typedef enum irqreturn irqreturn_t; // check service will fail.
#define irqreturn_t enum irqreturn


#define __u32 unsigned int
#define __u64 unsigned long long
/* sync from mtk_camera-v4l2-controls.h */
#define TSREC_TS_REC_MAX_CNT (4)
#define TSREC_EXP_MAX_CNT    (3)

struct mtk_cam_seninf_tsrec_vsync_info {
	/* source info */
	__u32 tsrec_no;
	__u32 seninf_idx;

	__u64 irq_sys_time_ns; // ktime_get_boottime_ns()
	__u64 irq_tsrec_ts_us;
};


/* sync from mtk_camera-v4l2-controls.h */
struct mtk_cam_seninf_tsrec_timestamp_exp {
	__u64 ts_us[TSREC_TS_REC_MAX_CNT];
};

struct mtk_cam_seninf_tsrec_timestamp_info {
	/* source info */
	__u32 tsrec_no;
	__u32 seninf_idx;

	/* basic info */
	__u32 tick_factor; // MHz

	/* record when receive a interrupt (top-half) */
	__u64 irq_sys_time_ns; // ktime_get_boottime_ns()
	__u64 irq_tsrec_ts_us;

	/* current tick when query/send tsrec timestamp info */
	__u64 tsrec_curr_tick;
	struct mtk_cam_seninf_tsrec_timestamp_exp exp_recs[TSREC_EXP_MAX_CNT];
};


/******************************************************************************
 * for unit test (fake function of linux APIs)
 *****************************************************************************/
unsigned long long ktime_get_boottime_ns(void);


/******************************************************************************
 * for unit test (function) - TSREC
 *****************************************************************************/
void ut_fs_tsrec_write_reg(const unsigned int addr, const unsigned int val);
unsigned int ut_fs_tsrec_read_reg(const unsigned int addr);

unsigned int ut_fs_tsrec_chk_fifo_not_empty(void);
void ut_fs_tsrec_fifo_out(
	unsigned int *p_intr_status, unsigned int *p_intr_status_2,
	unsigned long long *p_tsrec_ts_us);
unsigned int ut_fs_tsrec_fifo_in(
	const unsigned int intr_status, const unsigned int intr_status_2,
	const unsigned long long tsrec_ts_us);

void ut_fs_tsrec(void);

#endif
