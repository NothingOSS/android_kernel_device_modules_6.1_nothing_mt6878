/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2023 MediaTek Inc.
 */

#ifndef __MTK_DPC_MMP_H__
#define __MTK_DPC_MMP_H__

#if IS_ENABLED(CONFIG_FPGA_EARLY_PORTING)
#define dpc_mmp(args...)
#define dpc_mmp2(args...)

static inline void dpc_mmp_init(void) {}
#else

#include <mmprofile.h>
#include <mmprofile_function.h>

#define dpc_mmp(event, flag, v1, v2) \
	mmprofile_log_ex(dpc_mmp_get_event()->event, flag, v1, v2)
#define dpc_mmp2(event, flag, v1h, v1l, v2h, v2l) \
	mmprofile_log_ex(dpc_mmp_get_event()->event, flag, v1h << 16 | v1l, v2h << 16 | v2l)

struct dpc_mmp_events_t {
	mmp_event folder;
	// mmp_event dt_te;
	// mmp_event dt_sof;
	// mmp_event dsi_start;
	mmp_event idle_off;
	mmp_event prete;
	mmp_event mminfra;
	mmp_event vlp_vote;
	mmp_event mml_rrot_done;
	mmp_event mml_sof;
	// mmp_event mml_mutex40;
	// mmp_event mml_mutex48;
	// mmp_event mml_mutex50;
	mmp_event mtcmos_ovl0;
	// mmp_event mtcmos_ovl1;
	// mmp_event mtcmos_disp0;
	mmp_event mtcmos_disp1;
	mmp_event mtcmos_mml1;
};


struct dpc_mmp_events_t *dpc_mmp_get_event(void);
void dpc_mmp_init(void);
#endif

#endif
