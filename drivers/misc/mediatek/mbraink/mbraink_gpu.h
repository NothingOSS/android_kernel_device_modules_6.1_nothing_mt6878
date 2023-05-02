/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */
#ifndef MBRAINK_GPU_H
#define MBRAINK_GPU_H
#include "mbraink_ioctl_struct_def.h"

extern int mbraink_netlink_send_msg(const char *msg);

int mbraink_gpu_init(void);
int mbraink_gpu_deinit(void);
void mbraink_gpu_setQ2QTimeoutInNS(unsigned long long q2qTimeoutInNS);
unsigned long long mbraink_gpu_getQ2QTimeoutInNS(void);

#if IS_ENABLED(CONFIG_MTK_FPSGO_V3) || IS_ENABLED(CONFIG_MTK_FPSGO)

void fpsgo2mbrain_hint_frameinfo(int pid, unsigned long long bufID,
	int fps, unsigned long long time);
#endif

#endif /*end of MBRAINK_GPU_H*/
