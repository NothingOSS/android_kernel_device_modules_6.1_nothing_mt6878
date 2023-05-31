/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2022 MediaTek Inc.
 * Author: Anthony Huang <anthony.huang@mediatek.com>
 */

#ifndef _MMQOS_VCP_MEMORY_H_
#define _MMQOS_VCP_MEMORY_H_

#if IS_ENABLED(CONFIG_MTK_MMQOS_VCP)
void *mmqos_get_vcp_base(phys_addr_t *pa);
#else
static inline void *mmqos_get_vcp_base(phys_addr_t *pa)
{
	if (pa)
		*pa = 0;
	return NULL;
}
#endif

#define MEM_BASE		mmqos_get_vcp_base(NULL)
#define MEM_LOG_FLAG		(MEM_BASE + 0x0)
#define MEM_MMQOS_STATE		(MEM_BASE + 0x4)
#define MEM_TEST		(MEM_BASE + 0x8)
#define MEM_IPI_SYNC_FUNC	(MEM_BASE + 0xC)
#define MEM_IPI_SYNC_DATA	(MEM_BASE + 0x10)
/* skip : 0x14 */
#define MEM_TOTAL_BW		(MEM_BASE + 0x50)

#endif

