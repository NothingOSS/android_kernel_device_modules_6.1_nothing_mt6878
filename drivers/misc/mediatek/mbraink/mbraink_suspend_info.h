/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2022 MediaTek Inc.
 */

#ifndef MBRAINK_SUSPEND_INFO_H
#define MBRAINK_SUSPEND_INFO_H

#include "mbraink_ioctl_struct_def.h"

#define SUSPEND_INFO_SZ   128

struct mbraink_suspend_info_list {
	long long timestamp;
	unsigned short datatype;
	int reason;
	bool dirty;
};

struct mbraink_suspend_info_list_p {
	unsigned short r_idx;
	unsigned short w_idx;
};

void mbraink_set_suspend_info_list_record(unsigned short datatype);
void mbraink_get_suspend_info_list_record(struct mbraink_suspend_info_struct_data *buffer, int max);
void mbraink_suspend_info_list_init(void);
ssize_t last_resume_reason_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf);
#endif
