// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2023 MediaTek Inc.
 */

#include <linux/module.h>
#include <lpm_sys_res_mbrain_dbg.h>

static struct lpm_sys_res_mbrain_dbg_ops _lpm_sys_res_mbrain_dbg_ops;

struct lpm_sys_res_mbrain_dbg_ops *get_lpm_mbrain_dbg_ops(void)
{
	return &_lpm_sys_res_mbrain_dbg_ops;
}
EXPORT_SYMBOL(get_lpm_mbrain_dbg_ops);

int register_lpm_mbrain_dbg_ops(struct lpm_sys_res_mbrain_dbg_ops *ops)
{
	if (!ops)
		return -1;

	_lpm_sys_res_mbrain_dbg_ops.get_length = ops->get_length;
	_lpm_sys_res_mbrain_dbg_ops.get_data = ops->get_data;

	return 0;
}
EXPORT_SYMBOL(register_lpm_mbrain_dbg_ops);

void unregister_lpm_mbrain_dbg_ops(void)
{
	_lpm_sys_res_mbrain_dbg_ops.get_length = NULL;
	_lpm_sys_res_mbrain_dbg_ops.get_data = NULL;
}
EXPORT_SYMBOL(unregister_lpm_mbrain_dbg_ops);
