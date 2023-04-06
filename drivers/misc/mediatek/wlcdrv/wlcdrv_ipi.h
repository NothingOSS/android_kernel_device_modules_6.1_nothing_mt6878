/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2022 MediaTek Inc.
 */

#ifndef __WLCDRV_IPI_H__
#define __WLCDRV_IPI_H__

/* the following command is used for AP to Tinysys MCUPM */
/* init	  : 0x01
 * suspend: 0x02
 * resume : 0x03
 */

#define WLCIPI_CMD_DEFAULT				0xFFFFFFFF
#define WLCIPI_CMD_RESET				0x00000001
#define WLCIPI_CMD_SUSPEND				0x00000002
#define WLCIPI_CMD_RESUME				0x00000003
#define WLCIPI_CMD_RANDOM_VECTOR_STAGE1 0x00000004
#define WLCIPI_CMD_RANDOM_VECTOR_STAGE2 0x00000005
#define WLCIPI_CMD_VECTOR_STAGE1_USER	0x00000006
#define WLCIPI_CMD_VECTOR_STAGE2_USER	0x00000007
#define WLCIPI_CMD_DBG_INFTIMER			0x00000008

/* IPI Parameter Enum (from ap-side to mcupm wlc) */
enum {
	IPI_WLC_RESET	= 0xA1, // magic enum init to avoid conflict with other feature
	IPI_WLC_SUSPEND = 0xA2,
	IPI_WLC_RESUME	= 0xA3,
	IPI_WLC_RANDOM_VECTOR_STAGE1 = 0xA4,   // random input pmu to cached buffer
	IPI_WLC_RANDOM_VECTOR_STAGE2 = 0xA5,   // random input pmu ratio vector for wlc classifier
	IPI_WLC_VECTOR_STAGE1_USER = 0xA6,	   // user provide input vector for stage1 directly
	IPI_WLC_VECTOR_STAGE2_USER = 0xA7,	   // user provide input vector for stage2 directly
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
