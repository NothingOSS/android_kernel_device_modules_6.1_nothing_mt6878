// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#include <linux/errno.h>
#include <linux/slab.h>
#include "apusys_device.h"
#include "apummu_cmn.h"
#include "apummu_import.h"
#include "slbc_ops.h"

int apummu_alloc_slb(uint32_t type, uint32_t size, uint64_t *ret_addr,
						uint64_t *ret_size, uint32_t slb_wait_time)
{
	int ret = 0;
	struct slbc_data slb;

	slb.paddr = 0;
	slb.size = 0;

	switch (type) {
	case APUMMU_MEM_TYPE_EXT:
		slb.uid = UID_SH_APU;
		slb.type = TP_BUFFER;
		break;
	case APUMMU_MEM_TYPE_RSV_S:
		slb.uid = UID_AINR;
		slb.type = TP_BUFFER;
		slb.timeout = slb_wait_time;
		break;
	default:
		AMMU_LOG_ERR("Invalid type %u\n", type);
		ret = -EINVAL;
		goto out;
	}

	// TODO: open me for slb, mark for ep
	// ret = slbc_request(&slb);
	if (ret < 0) {
		AMMU_LOG_ERR("slbc_request Fail %d\n", ret);
		goto out;
	}


	*ret_addr = (size_t) slb.paddr;
	*ret_size = slb.size;
out:
	return ret;
}
int apummu_free_slb(uint32_t type, uint64_t addr)
{
	int ret = 0;
	struct slbc_data slb;

	switch (type) {
	case APUMMU_MEM_TYPE_EXT:
		slb.uid = UID_SH_APU;
		slb.type = TP_BUFFER;
		break;
	case APUMMU_MEM_TYPE_RSV_S:
		slb.uid = UID_AINR;
		slb.type = TP_BUFFER;
		break;
	default:
		AMMU_LOG_ERR("Invalid type %u\n", type);
		ret = -EINVAL;
		goto out;
	}

	// TODO: open me for slb, mark for ep
	// ret = slbc_release(&slb);
	if (ret < 0) {
		AMMU_LOG_ERR("slbc_release Fail %d\n", ret);
		goto out;
	}
out:
	return ret;
}
