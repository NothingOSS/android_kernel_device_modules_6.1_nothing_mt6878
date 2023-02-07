// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#include <linux/errno.h>
#include <linux/slab.h>
#include "apusys_device.h"

#include "apummu_cmn.h"
#include "apummu_export.h"
#include "apummu_drv.h"
#include "apummu_remote_cmd.h"
#include "apummu_import.h"

#include "apummu_mgt.h"

extern struct apummu_dev_info *g_adv;

int apummu_alloc_mem(uint32_t type, uint32_t size, uint64_t *addr, uint32_t *sid)
{
	int ret = 0, check = 0;
	uint64_t input_addr = 0, ret_addr = 0, input_size = 0;
	uint32_t ret_id = 0;


	if (g_adv == NULL) {
		AMMU_LOG_ERR("Invalid apummu_device\n");
		ret = -EINVAL;
		return ret;
	}
	AMMU_LOG_INFO("[Alloc] Mem (%u/0x%x)\n", type, size);

	switch (type) {
	case APUMMU_MEM_TYPE_EXT:
	case APUMMU_MEM_TYPE_RSV_S:
		ret = apummu_alloc_slb(type, size, &input_addr, &input_size,
								g_adv->plat.slb_wait_time);
		if (ret)
			goto out;
		break;
	case APUMMU_MEM_TYPE_RSV_T:
		input_addr = 0;
		input_size = size;
		break;

	case APUMMU_MEM_TYPE_VLM:
		input_addr = 0;
		input_size = size;
		break;
	default:
		AMMU_LOG_ERR("Invalid type %u\n", type);
		ret = -EINVAL;
		goto out;
	}
	AMMU_LOG_INFO("[Alloc] Mem (%u/0x%x/0x%llx/0x%llx)\n",
				type, size, input_addr, input_size);

	ret = apummu_remote_alloc_mem(g_adv, type, input_addr, input_size, &ret_addr, &ret_id);
	if (ret) {
		AMMU_LOG_ERR("Remote Handshake fail %d\n", ret);

		// Free SLB if fail
		if ((type == APUMMU_MEM_TYPE_EXT) || (type == APUMMU_MEM_TYPE_RSV_S)) {
			check = apummu_free_slb(type, input_addr);
			if (check)
				goto out;
		}

		goto out;
	}

	*addr = ret_addr;
	*sid = ret_id;

	AMMU_LOG_INFO("[Alloc][Done] Mem (%u/0x%x/0x%llx/0x%x)\n",
			type, size, ret_addr, ret_id);

	return ret;
out:
	AMMU_LOG_ERR("[Alloc][Fail] Mem (%u/0x%x/0x%llx/0x%x)\n", type, size, ret_addr, ret_id);
	return ret;
}

int apummu_free_mem(uint32_t sid)
{
	int ret = 0;
	uint32_t out_type = 0, out_size = 0;
	uint64_t out_addr = 0;

	if (g_adv == NULL) {
		AMMU_LOG_ERR("Invalid apummu_device\n");
		ret = -EINVAL;
		return ret;
	}
	AMMU_LOG_INFO("[Free] Mem (0x%x)\n", sid);

	ret = apummu_remote_free_mem(g_adv, sid, &out_type, &out_addr, &out_size);
	if (ret) {
		AMMU_LOG_ERR("Remote Handshake fail %d\n", ret);
		goto out;
	}

	switch (out_type) {
	case APUMMU_MEM_TYPE_VLM:
	case APUMMU_MEM_TYPE_RSV_T:
		break;
	case APUMMU_MEM_TYPE_EXT:
	case APUMMU_MEM_TYPE_RSV_S:
		ret = apummu_free_slb(out_type, out_addr);
		if (ret)
			goto out;
		break;
	default:
		AMMU_LOG_ERR("Invalid type %u\n", out_type);
		ret = -EINVAL;
		goto out;
	}

	AMMU_LOG_INFO("[Free][Done] Mem (0x%x) (%u/0x%llx/0x%x)\n",
				sid, out_type, out_addr, out_size);
	return ret;
out:
	AMMU_LOG_ERR("[Free][Fail] Mem (0x%x) (%u/0x%llx/0x%x)\n",
			sid, out_type, out_addr, out_size);

	return ret;
}

int apummu_import_mem(uint64_t session, uint32_t sid)
{
	int ret = 0;

	if (g_adv == NULL) {
		AMMU_LOG_ERR("Invalid apummu_device\n");
		ret = -EINVAL;
		return ret;
	}
	AMMU_LOG_INFO("[Import] Mem (0x%llx/0x%x)\n", session, sid);

	ret = apummu_remote_import_mem(g_adv, session, sid);
	if (ret) {
		AMMU_LOG_ERR("Remote Handshake fail %d\n", ret);
		goto out;
	}

	AMMU_LOG_INFO("[Import][Done] Mem (0x%llx/0x%x)\n", session, sid);

	return ret;
out:
	AMMU_LOG_ERR("[Import][Fail] Mem (0x%llx/0x%x)\n", session, sid);
	return ret;
}

int apummu_unimport_mem(uint64_t session, uint32_t sid)
{
	int ret = 0;

	if (g_adv == NULL) {
		AMMU_LOG_ERR("Invalid apummu_device\n");
		ret = -EINVAL;
		return ret;
	}

	AMMU_LOG_INFO("[UnImport] Mem (0x%llx/0x%x)\n", session, sid);

	ret = apummu_remote_unimport_mem(g_adv, session, sid);
	if (ret) {
		AMMU_LOG_ERR("Remote Handshake fail %d\n", ret);
		goto out;
	}

	AMMU_LOG_INFO("[UnImport][Done] Mem (0x%llx/0x%x)\n", session, sid);
	return ret;
out:
	AMMU_LOG_ERR("[UnImport][Fail] Mem (0x%llx/0x%x)\n", session, sid);
	return ret;
}

int apummu_map_mem(uint64_t session, uint32_t sid, uint64_t *addr)
{
	int ret = 0;
	uint64_t ret_addr = 0;

	if (g_adv == NULL) {
		AMMU_LOG_ERR("Invalid apummu_device\n");
		ret = -EINVAL;
		return ret;
	}

	AMMU_LOG_INFO("[Map] mem (0x%llx/0x%x/0x%llx)\n", session, sid, ret_addr);

	ret = apummu_remote_map_mem(g_adv, session, sid, &ret_addr);
	if (ret) {
		AMMU_LOG_ERR("Remote Handshake fail %d\n", ret);
		goto out;
	}
	*addr = ret_addr;

	AMMU_LOG_INFO("[Map][Done] Mem (0x%llx/0x%x/0x%llx)\n", session, sid, ret_addr);
	return ret;
out:

	AMMU_LOG_ERR("[Map][Fail] Mem (0x%llx/0x%x/0x%llx)\n", session, sid, ret_addr);
	return ret;
}

int apummu_unmap_mem(uint64_t session, uint32_t sid)
{
	int ret = 0;

	if (g_adv == NULL) {
		AMMU_LOG_ERR("Invalid apummu_device\n");
		ret = -EINVAL;
		return ret;
	}

	AMMU_LOG_INFO("[Unmap] mem (0x%llx/0x%x)\n", session, sid);

	ret = apummu_remote_unmap_mem(g_adv, session, sid);
	if (ret) {
		AMMU_LOG_ERR("Remote Handshake fail %d\n", ret);
		goto out;
	}

	AMMU_LOG_INFO("[Unmap][Done] Mem (0x%llx/0x%x)\n", session, sid);
	return ret;
out:
	AMMU_LOG_ERR("[Unmap][Fail] Mem (0x%llx/0x%x)\n", session, sid);
	return ret;
}

/**
 * @para:
 *  type		-> input buffer type, plz refer to enum AMMU_BUF_TYPE
 *  session		-> input session
 *  device_va	-> input device_va (gonna encode to eva)
 *  eva			-> output eva
 * @description:
 *  encode input addr to eva according to buffer type
 *  for apummu, we also record translate info into session table
 */
int apummu_iova2eva(uint32_t type, uint64_t session, uint64_t device_va,
		uint32_t buf_size, uint32_t *eva)
{
	return addr_encode_and_write_stable(type, session, device_va, buf_size, eva);
}

/**
 * @para:
 *  session		-> hint for searching target session table
 *  tbl_kva		-> returned addr for session rable
 *  size		-> returned session table size
 * @description:
 *  return the session table accroding to session
 */
int apummu_table_get(uint64_t session, void **tbl_kva, uint32_t *size)
{
	return get_session_table(session, tbl_kva, size);
}

/**
 * @para:
 *  session		-> hint for searching target session table
 * @description:
 *  delete the session table accroding to session
 */
int apummu_table_free(uint64_t session)
{
	return session_table_free(session);
}
