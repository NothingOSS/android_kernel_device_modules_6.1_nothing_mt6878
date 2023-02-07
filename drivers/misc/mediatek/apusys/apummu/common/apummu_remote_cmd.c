// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#include <linux/delay.h>
#include <linux/mutex.h>

#include "apummu_cmn.h"
#include "apummu_drv.h"
#include "apummu_remote.h"
// #include "apummu_table_mgt.h"
#include "apummu_msg.h"
#include "apummu_remote_cmd.h"


int apummu_remote_check_reply(void *reply)
{
	struct apummu_msg *msg;

	if (reply == NULL) {
		AMMU_LOG_ERR("Reply Null\n");
		return -EINVAL;
	}

	msg = (struct apummu_msg *)reply;
	if (msg->ack != 0) {
		AMMU_LOG_ERR("Reply Ack Error %x\n", msg->ack);
		return -EINVAL;
	}

	return 0;
}

int apummu_remote_set_op(void *drvinfo, uint32_t *argv, uint32_t argc)
{
	struct apummu_dev_info *rdv = NULL;
	struct apummu_msg req, reply;
	uint32_t i = 0;
	int ret = 0;
	uint32_t max_data = 0;

	if (drvinfo == NULL) {
		AMMU_LOG_ERR("invalid argument\n");
		return -EINVAL;
	}

	max_data = ARRAY_SIZE(req.data);
	if (argc > max_data) {
		AMMU_LOG_ERR("invalid argc %d / %d\n", argc, max_data);
		return -EINVAL;
	}

	rdv = (struct apummu_dev_info *)drvinfo;

	req.cmd = APUMMU_CMD_DBG_OP;
	req.option = APUMMU_OPTION_SET;

	for (i = 0; i < argc; i++)
		req.data[i] = argv[i];


	ret = apummu_remote_send_cmd_sync(drvinfo, (void *) &req, (void *) &reply, 0);
	if (ret) {
		AMMU_LOG_ERR("Send Msg Fail %d\n", ret);
		goto out;
	}
	ret = apummu_remote_check_reply((void *) &reply);
	if (ret) {
		AMMU_LOG_ERR("Check Msg Fail %d\n", ret);
		goto out;
	}

out:
	return ret;
}

int apummu_remote_handshake(void *drvinfo, void *remote)
{
	struct apummu_dev_info *rdv = NULL;
	struct apummu_msg req, reply;
	int ridx = 0;
	int ret = 0;

	if (drvinfo == NULL) {
		AMMU_LOG_ERR("invalid argument\n");
		return -EINVAL;
	}

	if (!apummu_is_remote()) {
		AMMU_LOG_ERR("Remote Not Init\n");
		return -EINVAL;
	}

	rdv = (struct apummu_dev_info *)drvinfo;

	memset(&req, 0, sizeof(struct apummu_msg));
	memset(&reply, 0, sizeof(struct apummu_msg));


	req.cmd = APUMMU_CMD_HANDSHAKE;
	req.option = APUMMU_OPTION_GET;

	AMMU_LOG_INFO("Remote Handshake...\n");
	ret = apummu_remote_send_cmd_sync(drvinfo, (void *) &req, (void *) &reply, 0);
	if (ret) {
		AMMU_LOG_ERR("Remote Handshake Fail %d\n", ret);
		goto out;
	}

	ret = apummu_remote_check_reply((void *) &reply);
	if (ret) {
		AMMU_LOG_ERR("Check Msg Fail %d\n", ret);
		goto out;
	}

	/* Init Remote Info */
	AMMU_RPMSG_RAED(&rdv->remote.dram_max, reply.data, sizeof(rdv->remote.dram_max), ridx);
	AMMU_RPMSG_RAED(&rdv->remote.vlm_addr, reply.data, sizeof(rdv->remote.vlm_addr), ridx);
	AMMU_RPMSG_RAED(&rdv->remote.vlm_size, reply.data, sizeof(rdv->remote.vlm_size), ridx);

	apummu_remote_sync_sn(drvinfo, reply.sn);
out:
	return ret;
}

int apummu_remote_set_hw_default_iova(void *drvinfo, uint32_t ctx, uint64_t iova)
{
	struct apummu_dev_info *rdv = NULL;
	struct apummu_msg req, reply;
	int ret = 0;
	int widx = 0;

	if (drvinfo == NULL) {
		AMMU_LOG_ERR("invalid argument\n");
		return -EINVAL;
	}

	if (!apummu_is_remote()) {
		AMMU_LOG_ERR("Remote Not Init\n");
		return -EINVAL;
	}

	rdv = (struct apummu_dev_info *)drvinfo;

	memset(&req, 0, sizeof(struct apummu_msg));
	memset(&reply, 0, sizeof(struct apummu_msg));

	req.cmd = APUMMU_CMD_HW_DEFAULT_IOVA;
	req.option = APUMMU_OPTION_SET;

	AMMU_RPMSG_write(&ctx, req.data, sizeof(ctx), widx);
	AMMU_RPMSG_write(&iova, req.data, sizeof(iova), widx);

	ret = apummu_remote_send_cmd_sync(drvinfo, (void *) &req, (void *) &reply, 0);
	if (ret) {
		AMMU_LOG_ERR("Send Msg Fail %d\n", ret);
		goto out;
	}
	ret = apummu_remote_check_reply((void *) &reply);
	if (ret) {
		AMMU_LOG_ERR("Check Msg Fail %d\n", ret);
		goto out;
	}
out:
	return ret;
}

int apummu_remote_set_hw_default_iova_one_shot(void *drvinfo)
{
	struct apummu_dev_info *rdv = NULL;
	struct apummu_msg req, reply;
	int ret = 0;
	int widx = 0;

	if (drvinfo == NULL) {
		AMMU_LOG_ERR("invalid argument\n");
		return -EINVAL;
	}

	if (!apummu_is_remote()) {
		AMMU_LOG_ERR("Remote Not Init\n");
		return -EINVAL;
	}

	rdv = (struct apummu_dev_info *)drvinfo;

	memset(&req, 0, sizeof(struct apummu_msg));
	memset(&reply, 0, sizeof(struct apummu_msg));

	req.cmd = APUMMU_CMD_HW_DEFAULT_IOVA_ONE_SHOT;
	req.option = APUMMU_OPTION_SET;

	AMMU_RPMSG_write(&rdv->remote.dram_IOVA_base, req.data,
			sizeof(rdv->remote.dram_IOVA_base), widx);

	ret = apummu_remote_send_cmd_sync(drvinfo, (void *) &req, (void *) &reply, 0);
	if (ret) {
		AMMU_LOG_ERR("Send Msg Fail %d\n", ret);
		goto out;
	}
	ret = apummu_remote_check_reply((void *) &reply);
	if (ret) {
		AMMU_LOG_ERR("Check Msg Fail %d\n", ret);
		goto out;
	}
out:
	return ret;
}

int apummu_remote_alloc_mem(void *drvinfo,
		uint32_t type, uint64_t input_addr, uint32_t size,
		uint64_t *addr, uint32_t *sid)
{
	struct apummu_dev_info *rdv = NULL;
	struct apummu_msg req, reply;
	int ret = 0;
	uint32_t ret_id = 0, mem_op = 0, out_op = 0;
	int widx = 0;
	int ridx = 0;
	uint64_t ret_addr = 0;
	uint32_t in_addr = 0, out_addr = 0;

	if (drvinfo == NULL) {
		AMMU_LOG_ERR("invalid argument\n");
		return -EINVAL;
	}

	if (!apummu_is_remote()) {
		AMMU_LOG_ERR("Remote Not Init\n");
		return -EINVAL;
	}

	rdv = (struct apummu_dev_info *)drvinfo;

	memset(&req, 0, sizeof(struct apummu_msg));
	memset(&reply, 0, sizeof(struct apummu_msg));

	req.cmd = APUMMU_CMD_SYSTEM_RAM;
	req.option = APUMMU_OPTION_SET;

	mem_op = APUMMU_MEM_ALLOC;

	in_addr = (uint32_t) input_addr;

	AMMU_RPMSG_write(&mem_op, req.data, sizeof(mem_op), widx);
	AMMU_RPMSG_write(&type, req.data, sizeof(type), widx);
	AMMU_RPMSG_write(&in_addr, req.data, sizeof(in_addr), widx);
	AMMU_RPMSG_write(&size, req.data, sizeof(size), widx);

	ret = apummu_remote_send_cmd_sync(drvinfo, (void *) &req, (void *) &reply, 0);
	if (ret) {
		AMMU_LOG_ERR("Send Msg Fail %d\n", ret);
		goto out;
	}
	ret = apummu_remote_check_reply((void *) &reply);
	if (ret) {
		AMMU_LOG_ERR("Check Msg Fail %d\n", ret);
		goto out;
	}

	AMMU_RPMSG_RAED(&out_op, reply.data, sizeof(out_op), ridx);
	AMMU_RPMSG_RAED(&out_addr, reply.data, sizeof(out_addr), ridx);
	AMMU_RPMSG_RAED(&ret_id, reply.data, sizeof(ret_id), ridx);

	if (out_op != mem_op) {
		AMMU_LOG_ERR("Check OP Fail %x/%x\n", out_op, mem_op);
		ret = -EINVAL;
		goto out;
	}

	ret_addr = (uint64_t) out_addr;

	*sid = ret_id;
	*addr = ret_addr;

out:
	return ret;
}

int apummu_remote_free_mem(void *drvinfo, uint32_t sid, uint32_t *type,
				uint64_t *addr, uint32_t *size)
{
	struct apummu_dev_info *rdv = NULL;
	struct apummu_msg req, reply;
	int ret = 0;
	int widx = 0;
	int ridx = 0;
	uint32_t mem_op = 0, out_op = 0;
	uint32_t out_addr = 0, out_size = 0, out_type = 0;
	uint64_t ret_addr = 0;

	if (drvinfo == NULL) {
		AMMU_LOG_ERR("invalid argument\n");
		return -EINVAL;
	}

	if (!apummu_is_remote()) {
		AMMU_LOG_ERR("Remote Not Init\n");
		return -EINVAL;
	}

	rdv = (struct apummu_dev_info *)drvinfo;

	memset(&req, 0, sizeof(struct apummu_msg));
	memset(&reply, 0, sizeof(struct apummu_msg));

	req.cmd = APUMMU_CMD_SYSTEM_RAM;
	req.option = APUMMU_OPTION_SET;

	mem_op = APUMMU_MEM_FREE;

	AMMU_RPMSG_write(&mem_op, req.data, sizeof(mem_op), widx);
	AMMU_RPMSG_write(&sid, req.data, sizeof(sid), widx);

	ret = apummu_remote_send_cmd_sync(drvinfo, (void *) &req, (void *) &reply, 0);
	if (ret) {
		AMMU_LOG_ERR("Send Msg Fail %d\n", ret);
		goto out;
	}
	ret = apummu_remote_check_reply((void *) &reply);
	if (ret) {
		AMMU_LOG_ERR("Check Msg Fail %d\n", ret);
		goto out;
	}

	AMMU_RPMSG_RAED(&out_op, reply.data, sizeof(out_op), ridx);
	AMMU_RPMSG_RAED(&out_type, reply.data, sizeof(out_type), ridx);
	AMMU_RPMSG_RAED(&out_addr, reply.data, sizeof(out_addr), ridx);
	AMMU_RPMSG_RAED(&out_size, reply.data, sizeof(out_size), ridx);

	if (out_op != mem_op) {
		AMMU_LOG_ERR("Check OP Fail %x/%x\n", out_op, mem_op);
		ret = -EINVAL;
		goto out;
	}

	ret_addr = (uint64_t) out_addr;
	*type = out_type;
	*addr = ret_addr;
	*size = out_size;

out:
	return ret;
}

int apummu_remote_map_mem(void *drvinfo,
		uint64_t session, uint32_t sid, uint64_t *addr)
{
	struct apummu_dev_info *rdv = NULL;
	struct apummu_msg req, reply;
	int ret = 0;
	uint32_t mem_op = 0, out_op = 0;
	int widx = 0;
	int ridx = 0;
	uint32_t out_addr = 0;
	uint64_t ret_addr = 0;

	if (drvinfo == NULL) {
		AMMU_LOG_ERR("invalid argument\n");
		return -EINVAL;
	}

	if (!apummu_is_remote()) {
		AMMU_LOG_ERR("Remote Not Init\n");
		return -EINVAL;
	}

	rdv = (struct apummu_dev_info *)drvinfo;

	memset(&req, 0, sizeof(struct apummu_msg));
	memset(&reply, 0, sizeof(struct apummu_msg));

	req.cmd = APUMMU_CMD_SYSTEM_RAM;
	req.option = APUMMU_OPTION_SET;

	mem_op = APUMMU_MEM_MAP;

	AMMU_RPMSG_write(&mem_op, req.data, sizeof(mem_op), widx);
	AMMU_RPMSG_write(&session, req.data, sizeof(session), widx);
	AMMU_RPMSG_write(&sid, req.data, sizeof(sid), widx);

	ret = apummu_remote_send_cmd_sync(drvinfo, (void *) &req, (void *) &reply, 0);
	if (ret) {
		AMMU_LOG_ERR("Send Msg Fail %d\n", ret);
		goto out;
	}
	ret = apummu_remote_check_reply((void *) &reply);
	if (ret) {
		AMMU_LOG_ERR("Check Msg Fail %d\n", ret);
		goto out;
	}

	AMMU_RPMSG_RAED(&out_op, reply.data, sizeof(out_op), ridx);
	AMMU_RPMSG_RAED(&out_addr, reply.data, sizeof(out_addr), ridx);

	if (out_op != mem_op) {
		AMMU_LOG_ERR("Check OP Fail %x/%x\n", out_op, mem_op);
		ret = -EINVAL;
		goto out;
	}


	ret_addr = (uint64_t) out_addr;
	*addr = ret_addr;

out:
	return ret;
}

int apummu_remote_unmap_mem(void *drvinfo,
		uint64_t session, uint32_t sid)
{
	struct apummu_dev_info *rdv = NULL;
	struct apummu_msg req, reply;
	int ret = 0;
	uint32_t mem_op = 0, out_op = 0;
	int widx = 0, ridx = 0;

	if (drvinfo == NULL) {
		AMMU_LOG_ERR("invalid argument\n");
		return -EINVAL;
	}

	if (!apummu_is_remote()) {
		AMMU_LOG_ERR("Remote Not Init\n");
		return -EINVAL;
	}

	rdv = (struct apummu_dev_info *)drvinfo;

	memset(&req, 0, sizeof(struct apummu_msg));
	memset(&reply, 0, sizeof(struct apummu_msg));

	req.cmd = APUMMU_CMD_SYSTEM_RAM;
	req.option = APUMMU_OPTION_SET;

	mem_op = APUMMU_MEM_UNMAP;

	AMMU_RPMSG_write(&mem_op, req.data, sizeof(mem_op), widx);
	AMMU_RPMSG_write(&session, req.data, sizeof(session), widx);
	AMMU_RPMSG_write(&sid, req.data, sizeof(sid), widx);

	ret = apummu_remote_send_cmd_sync(drvinfo, (void *) &req, (void *) &reply, 0);
	if (ret) {
		AMMU_LOG_ERR("Send Msg Fail %d\n", ret);
		goto out;
	}
	ret = apummu_remote_check_reply((void *) &reply);
	if (ret) {
		AMMU_LOG_ERR("Check Msg Fail %d\n", ret);
		goto out;
	}

	AMMU_RPMSG_RAED(&out_op, reply.data, sizeof(out_op), ridx);

	if (out_op != mem_op) {
		AMMU_LOG_ERR("Check OP Fail %x/%x\n", out_op, mem_op);
		ret = -EINVAL;
		goto out;
	}

out:
	return ret;
}

int apummu_remote_import_mem(void *drvinfo, uint64_t session, uint32_t sid)
{
	struct apummu_dev_info *rdv = NULL;
	struct apummu_msg req, reply;
	int ret = 0;
	int widx = 0, ridx = 0;
	uint32_t mem_op = 0, out_op = 0;

	if (drvinfo == NULL) {
		AMMU_LOG_ERR("invalid argument\n");
		return -EINVAL;
	}

	if (!apummu_is_remote()) {
		AMMU_LOG_ERR("Remote Not Init\n");
		return -EINVAL;
	}

	rdv = (struct apummu_dev_info *)drvinfo;

	memset(&req, 0, sizeof(struct apummu_msg));
	memset(&reply, 0, sizeof(struct apummu_msg));

	req.cmd = APUMMU_CMD_SYSTEM_RAM;
	req.option = APUMMU_OPTION_SET;

	mem_op = APUMMU_MEM_IMPORT;

	AMMU_LOG_INFO("session %llx free sid %x\n", session, sid);

	AMMU_RPMSG_write(&mem_op, req.data, sizeof(mem_op), widx);
	AMMU_RPMSG_write(&session, req.data, sizeof(session), widx);
	AMMU_RPMSG_write(&sid, req.data, sizeof(sid), widx);

	ret = apummu_remote_send_cmd_sync(drvinfo, (void *) &req, (void *) &reply, 0);
	if (ret) {
		AMMU_LOG_ERR("Send Msg Fail %d\n", ret);
		goto out;
	}
	ret = apummu_remote_check_reply((void *) &reply);
	if (ret) {
		AMMU_LOG_ERR("Check Msg Fail %d\n", ret);
		goto out;
	}

	AMMU_RPMSG_RAED(&out_op, reply.data, sizeof(out_op), ridx);

	if (out_op != mem_op) {
		AMMU_LOG_ERR("Check OP Fail %x/%x\n", out_op, mem_op);
		ret = -EINVAL;
		goto out;
	}

out:
	return ret;
}

int apummu_remote_unimport_mem(void *drvinfo, uint64_t session, uint32_t sid)
{
	struct apummu_dev_info *rdv = NULL;
	struct apummu_msg req, reply;
	int ret = 0;
	int widx = 0, ridx = 0;
	uint32_t mem_op = 0, out_op = 0;

	if (drvinfo == NULL) {
		AMMU_LOG_ERR("invalid argument\n");
		return -EINVAL;
	}

	if (!apummu_is_remote()) {
		AMMU_LOG_ERR("Remote Not Init\n");
		return -EINVAL;
	}

	rdv = (struct apummu_dev_info *)drvinfo;

	memset(&req, 0, sizeof(struct apummu_msg));
	memset(&reply, 0, sizeof(struct apummu_msg));

	req.cmd = APUMMU_CMD_SYSTEM_RAM;
	req.option = APUMMU_OPTION_SET;


	mem_op = APUMMU_MEM_UNIMPORT;

	AMMU_LOG_INFO("session %llx free sid %x\n", session, sid);

	AMMU_RPMSG_write(&mem_op, req.data, sizeof(mem_op), widx);
	AMMU_RPMSG_write(&session, req.data, sizeof(session), widx);
	AMMU_RPMSG_write(&sid, req.data, sizeof(sid), widx);


	ret = apummu_remote_send_cmd_sync(drvinfo, (void *) &req, (void *) &reply, 0);
	if (ret) {
		AMMU_LOG_ERR("Send Msg Fail %d\n", ret);
		goto out;
	}
	ret = apummu_remote_check_reply((void *) &reply);
	if (ret) {
		AMMU_LOG_ERR("Check Msg Fail %d\n", ret);
		goto out;
	}

	AMMU_RPMSG_RAED(&out_op, reply.data, sizeof(out_op), ridx);

	if (out_op != mem_op) {
		AMMU_LOG_ERR("Check OP Fail %x/%x\n", out_op, mem_op);
		ret = -EINVAL;
		goto out;
	}

out:
	return ret;
}

