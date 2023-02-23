/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2020 MediaTek Inc.
 */

#ifndef __SCP_CHRE_MANAGER_H__
#define __SCP_CHRE_MANAGER_H__

void scp_chre_manager_init(void);
void scp_chre_manager_exit(void);

#define SCP_CHRE_MAGIC 0x67728269
#define SCP_CHRE_MANAGER_PAYLOAD_MAXIMUM 0x1000

/* ipi msg */
struct scp_chre_ipi_msg {
	uint32_t magic;
	uint32_t size;
};

/* payload for exchanging CHRE data*/
struct scp_chre_manager_payload {
	struct scp_chre_ipi_msg msg;
	uint64_t ptr;
};

#endif
