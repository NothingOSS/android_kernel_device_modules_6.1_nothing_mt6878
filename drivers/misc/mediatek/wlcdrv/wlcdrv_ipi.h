/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2022 MediaTek Inc.
 */

#ifndef __WLCDRV_IPI_H__
#define __WLCDRV_IPI_H__

/* the following command is used for AP to Tinysys MCUPM */
/* init   : 0x01
 * suspend: 0x02
 * resume : 0x03
 */

#define WLCIPI_CMD_DEFAULT         0xFFFFFFFF
#define WLCIPI_CMD_INIT            0x00000001
#define WLCIPI_CMD_SUSPEND         0x00000002
#define WLCIPI_CMD_RESUME          0x00000003

/* IPI Parameter Enum (from ap-side to mcupm wlc) */
enum {
	IPI_WLC_INIT    = 0xA1, //magic enum init to avoid conflict with other feature
	IPI_WLC_SUSPEND = 0xA2,
	IPI_WLC_RESUME  = 0xA3,
	NR_WLC_IPI,
};


struct wlc_ipi_data {
	unsigned int cmd;
	union {
		struct {
			unsigned int arg[3];
		} data;
	} u;
};


int wlc_ipi_to_mcupm_init(void);
int wlc_ipi_to_mcupm_deinit(void);
int wlc_ipi_to_mcupm_send(int ctrl_cmd);

#endif /* __WLCDRV_IPI_H__ */
