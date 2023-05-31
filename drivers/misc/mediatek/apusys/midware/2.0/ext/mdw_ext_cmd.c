// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2023 MediaTek Inc.
 */

#include "mdw_ext.h"
#include "mdw_ext_cmn.h"
#include "mdw_ext_ioctl.h"

static int mdw_ext_cmd_v4(union mdw_ext_cmd_args *args)
{
	return 0;
}

int mdw_ext_cmd_ioctl(void *data)
{
	union mdw_ext_cmd_args *args = (union mdw_ext_cmd_args *)data;
	int fence = -1, ret = 0;

	mdwext_cmd_debug("extid(0x%llx)\n", args->in.ext_id);
	ret = mdw_ext_cmd_v4(args);
	mdwext_cmd_debug("extid(0x%llx) fence(%d)\n", args->in.ext_id, fence);

	return ret;
}
