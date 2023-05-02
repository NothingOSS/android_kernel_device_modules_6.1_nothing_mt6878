// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#include <linux/module.h>
#include <linux/types.h>

#include "npu_scp_ipi.h"
#include "aov_mem_service_v2.h"

enum npu_scp_mem_service_action {
	NPU_SCP_REQUEST_IOVA = 1,
};

static int mem_service_handler(struct npu_scp_ipi_param *recv_msg)
{
	int ret = 0;

	if (!recv_msg)
		return -EINVAL;

	switch (recv_msg->act) {
	case NPU_SCP_REQUEST_IOVA:
		// set up IOVA
		break;
	default:
		pr_info("%s Not supported act %d\n", __func__, recv_msg->act);
		ret = -EINVAL;
		break;
	}

	return ret;
}

int aov_mem_service_v2_init(void)
{
	// setup IOVA information

	npu_scp_ipi_register_handler(NPU_SCP_MEM_SERVICE, mem_service_handler, NULL);

	return 0;
}

void aov_mem_service_v2_exit(void)
{
	npu_scp_ipi_unregister_handler(NPU_SCP_MEM_SERVICE);
}
