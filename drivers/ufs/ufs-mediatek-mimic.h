/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2023 MediaTek Inc.
 */
#ifndef _UFS_MEDIATEK_MIMIC_
#define _UFS_MEDIATEK_MIMIC_

#include <linux/types.h>
#include <ufs/ufshcd.h>

/* Error handling flags */
enum {
	UFSM_EH_IN_PROGRESS = (1 << 0),
};

#define ufsm_eh_in_progress(h) \
	((h)->eh_flags & UFSM_EH_IN_PROGRESS)

int ufsm_wait_for_doorbell_clr(struct ufs_hba *hba,
				 u64 wait_timeout_us);

void ufsm_scsi_unblock_requests(struct ufs_hba *hba);
void ufsm_scsi_block_requests(struct ufs_hba *hba);

#endif /* _UFS_MEDIATEK_MIMIC_ */
