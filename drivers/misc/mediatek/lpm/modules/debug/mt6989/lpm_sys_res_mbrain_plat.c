// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2023 MediaTek Inc.
 */
#include <linux/spinlock.h>

#include <lpm_sys_res.h>
#include <lpm_sys_res_plat.h>
#include <lpm_sys_res_mbrain_dbg.h>
#include <lpm_sys_res_mbrain_plat.h>

unsigned int sys_res_sig_num;
struct sys_res_mbrain_header header = {
	SYS_RES_DATA_MODULE_ID,
	SYS_RES_DATA_VERSION,
	0,
	0,
};

static int group_release[NR_SPM_GRP] = {
	RELEASE_GROUP,
	RELEASE_GROUP,
	NOT_RELEASE,
	NOT_RELEASE,
	NOT_RELEASE,
	RELEASE_GROUP,
	NOT_RELEASE,
	RELEASE_GROUP,
	NOT_RELEASE,
	NOT_RELEASE,
};

void get_sys_res_header(void)
{
	int i = 0;

	for (i=0; i<NR_SPM_GRP; i++) {
		if (group_release[i]) {
			sys_res_sig_num += 1;
			sys_res_sig_num += sys_res_group_info[i].group_num;
		}
	}

	header.data_offset = sizeof(struct sys_res_mbrain_header);
	header.index_data_length = SCENE_RELEASE_NUM *
		(sizeof(struct sys_res_scene_info) +
		sys_res_sig_num * sizeof(struct sys_res_sig_info));
}

void *sys_res_data_copy(void *dest, void *src, uint64_t size)
{
	memcpy(dest, src, size);
	dest += size;
	return dest;
}

unsigned int lpm_mbrain_get_sys_res_length(void)
{
	return header.index_data_length;
}

int lpm_mbrain_get_sys_res_data(void *address, uint32_t size)
{
	int i = 0;
	int j = 0;
	int ret = 0, sys_res_update = 0;
	unsigned long flag;
	struct lpm_sys_res_ops *sys_res_ops;
	struct sys_res_record *sys_res_record[SCENE_RELEASE_NUM];
	struct sys_res_scene_info scene_info;
	void *sig_info;

	if (!address ||
	    header.index_data_length == 0 ||
	    size < header.index_data_length + header.data_offset) {
		pr_info("[name:spm&][SPM][Mbrain] mbrain address/buffer size error\n");
		ret = -1;
	}

	sys_res_ops = get_lpm_sys_res_ops();
	if (!sys_res_ops ||
	    !sys_res_ops->update ||
	    !sys_res_ops->get ||
	    !sys_res_ops->get_detail) {
		pr_info("[name:spm&][SPM][Mbrain] Get sys res operations fail\n");
		ret = -1;
	}

	if (ret)
		return ret;

	/* Copy header */
	address = sys_res_data_copy(address, &header, sizeof(struct sys_res_mbrain_header));

	sys_res_update = sys_res_ops->update();
	if(sys_res_update)
		pr_info("[name:spm&][SPM][Mbrain] SWPM data is invalid, Error Code [%d]\n", sys_res_update);

	/* Copy scenario data */
	sys_res_record[SYS_RES_RELEASE_SCENE_COMMON] = sys_res_ops->get(SYS_RES_COMMON);
	sys_res_record[SYS_RES_RELEASE_SCENE_SUSPEND] = sys_res_ops->get(SYS_RES_SUSPEND);
	sys_res_record[SYS_RES_RELEASE_SCENE_LAST_SUSPEND] = sys_res_ops->get(SYS_RES_LAST_SUSPEND);

	spin_lock_irqsave(sys_res_ops->lock, flag);
	scene_info.res_sig_num = sys_res_sig_num;
	for (i=0; i<SCENE_RELEASE_NUM; i++) {
		scene_info.duration_time = sys_res_ops->get_detail(sys_res_record[i],
								SYS_RES_DURATION, 0);
		scene_info.suspend_time = sys_res_ops->get_detail(sys_res_record[i],
								SYS_RES_SUSPEND_TIME, 0);
		address = sys_res_data_copy(address, &scene_info,
							sizeof(struct sys_res_scene_info));
	}

	/* Copy signal data */
	for (i=0; i<SCENE_RELEASE_NUM; i++) {
		for (j = 0; j < SYS_RES_SYS_RESOURCE_NUM; j++){
			if (group_release[j]) {
				sig_info = (void *)sys_res_ops->get_detail(sys_res_record[i],
							SYS_RES_SIG_ADDR,
							sys_res_group_info[j].sys_index);
				if (sig_info)
					address = sys_res_data_copy(address,
								sig_info,
								sizeof(struct sys_res_sig_info));

				sig_info = (void *)sys_res_ops->get_detail(sys_res_record[i],
							SYS_RES_SIG_ADDR,
							sys_res_group_info[j].sig_table_index);
				if (sig_info)
					address = sys_res_data_copy(address,
								sig_info,
								sizeof(struct sys_res_sig_info) *
								sys_res_group_info[j].group_num);
			}
		}
	}
	spin_unlock_irqrestore(sys_res_ops->lock, flag);

	return ret;
}

static struct lpm_sys_res_mbrain_dbg_ops sys_res_mbrain_ops = {
	.get_length = lpm_mbrain_get_sys_res_length,
	.get_data = lpm_mbrain_get_sys_res_data
};

int lpm_sys_res_mbrain_plat_init (void)
{
	get_sys_res_header();
	return register_lpm_mbrain_dbg_ops(&sys_res_mbrain_ops);
}

void lpm_sys_res_mbrain_plat_deinit (void)
{
	unregister_lpm_mbrain_dbg_ops();
}
