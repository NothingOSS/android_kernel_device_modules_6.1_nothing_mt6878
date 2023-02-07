// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2022 MediaTek Inc.
 */

#include <linux/vmalloc.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/debugfs.h>
#include <linux/errno.h>
#include <linux/io.h>
#include <linux/uaccess.h>
#include <linux/string.h>
#include <linux/delay.h>

#include "apummu_cmn.h"
#include "apummu_drv.h"
#include "apummu_dbg.h"
#include "apummu_remote.h"
#include "apummu_remote_cmd.h"
#include "apummu_export.h"	// for verify API to MDW

#include "apummu_mgt.h"

/* All level default on */
uint32_t g_ammu_klog = 0xF;	/* apummu kernel log level */

#define APUMMU_DBG_DIR "apummu"

#define PARSER_DBG	(0)		/* Enable op parse dump */
#define KERNEL_DBG	(1)		/* Enable kernel debug node */

/* debug root node */
static struct dentry *apummu_dbg_root;

/* remote debug node */
static struct dentry *apummu_dbg_remote_op;

#if KERNEL_DBG
static struct dentry *apummu_dbg_kernel;
#endif

/* parse input and send IPI */
static ssize_t apummu_dbg_write_op(struct file *file, const char __user *user_buf,
			size_t count, loff_t *ppos)
{
#define MAX_ARG	(7)
	struct apummu_dev_info *adv = file->private_data;
	char *tmp, *token, *cursor;
	uint32_t argv[MAX_ARG];
	int ret, i;

	tmp = kzalloc(count + 1, GFP_KERNEL);
	if (!tmp)
		return -ENOMEM;

	ret = copy_from_user(tmp, user_buf, count);
	if (ret) {
		ret = -EINVAL;
		goto out;
	}

	tmp[count] = '\0';
	cursor = tmp;

	/* parse arguments */
	for (i = 0; i < MAX_ARG && (token = strsep(&cursor, " ")); i++) {
		ret = kstrtouint(token, 16, &argv[i]);
		if (ret) {
			AMMU_LOG_ERR("fail to parse argv[%d]\n", i);
			goto out;
		}
	}

#if PARSER_DBG
	for (i = 0; i < MAX_ARG; i++)
		AMMU_LOG_INFO("args[%d][%d]\n", i, argv[i]);
#endif

	ret = apummu_remote_set_op(adv, argv, MAX_ARG);
	if (ret) {
		AMMU_LOG_ERR("set OP fail %d\n", ret);
		goto out;
	}
	ret = count;
out:
	kfree(tmp);
	return ret;

}
static const struct file_operations apummu_dbg_fops_remoteop = {
	.open = simple_open,
	.write = apummu_dbg_write_op,
	.llseek = default_llseek,
};

#if KERNEL_DBG
/* dump interest kernel info */
static ssize_t apummu_dbg_kernel_read(struct file *filp, char *buffer,
			size_t length, loff_t *offset)
{
	int ret = 0;

	dump_session_table_set();

	return ret;
}

static ssize_t apummu_dbg_write_kernel(struct file *file, const char __user *user_buf,
			size_t count, loff_t *ppos)
{
#define MAX_ARG_kernel	(4)
	char *tmp, *token, *cursor;
	uint32_t argv[MAX_ARG_kernel];
	int ret, i;
	uint32_t mode, type, device_va, eva, size, j;
	uint64_t session;
	void *tbl_kva = NULL;
	struct apummu_session_tbl *g_ammu_session_table_ptr_DBG = NULL;

	tmp = kzalloc(count + 1, GFP_KERNEL);
	if (!tmp)
		return -ENOMEM;

	ret = copy_from_user(tmp, user_buf, count);
	if (ret) {
		ret = -EINVAL;
		goto out;
	}

	tmp[count] = '\0';
	cursor = tmp;

	/* parse arguments */
	for (i = 0; i < MAX_ARG_kernel && (token = strsep(&cursor, " ")); i++) {
		ret = kstrtouint(token, 16, &argv[i]);
		if (ret) {
			AMMU_LOG_ERR("fail to parse argv[%d]\n", i);
			goto out;
		}
	}

#if PARSER_DBG
	for (i = 0; i < MAX_ARG_kernel; i++)
		AMMU_LOG_INFO("args[%x][%x]\n", i, argv[i]);
#endif

	mode = argv[0];
	type = argv[1];
	session = argv[2];
	device_va = argv[3];

	switch (mode) {
	case 0:
		ret = apummu_iova2eva(type, session, device_va, 0, &eva);
		if (ret)
			AMMU_LOG_ERR("apummu_iova2eva fail\n");
		else
			AMMU_LOG_DBG("apummu_iova2eva ret IOVA = 0x%x\n", eva);

		break;
	case 1:
		AMMU_LOG_DBG("Free stable, session = 0x%llx\n", session);
		apummu_table_free(session);
		break;
	case 2:
		AMMU_LOG_DBG("Destroy apummu stable\n");
		apummu_mgt_destroy();
		break;
	case 3:
		AMMU_LOG_DBG("apummu get stable, session = 0x%llx\n", session);
		ret = apummu_table_get(session, &tbl_kva, &size);
		if (ret) {
			AMMU_LOG_ERR("apummu get_session_table fail\n");
			break;
		}

		if (!tbl_kva) {
			AMMU_LOG_ERR("tbl_kva NULL\n");
			break;
		}

		g_ammu_session_table_ptr_DBG = (struct apummu_session_tbl *) tbl_kva;

		AMMU_LOG_DBG("== APUMMU dump session table in DBG Start ==\n");
		AMMU_LOG_DBG("== size = 0x%x\n", size);
		AMMU_LOG_DBG("session           = 0x%llx\n", g_ammu_session_table_ptr_DBG->session);
		AMMU_LOG_DBG("session_entry_cnt = %u\n",
				g_ammu_session_table_ptr_DBG->session_entry_cnt);
		AMMU_LOG_DBG("dram_idx_mask     = 0x%x\n",
				g_ammu_session_table_ptr_DBG->dram_idx_mask);
		AMMU_LOG_DBG("page_idx_mask     = 0x%x 0x%x\n",
				g_ammu_session_table_ptr_DBG->page_idx_mask[0],
				g_ammu_session_table_ptr_DBG->page_idx_mask[1]);

		AMMU_LOG_DBG("== dump session addr table ==\n");
		AMMU_LOG_DBG("           | type |    IOVA    |     EVA    |\n");
		for (j = 0; j < g_ammu_session_table_ptr_DBG->session_entry_cnt; j++) {
			if (g_ammu_session_table_ptr_DBG->adr[j].type == 1) {
				AMMU_LOG_DBG("> entry%3u:     %u | 0x%8x | 0x%8x |\n", j,
					g_ammu_session_table_ptr_DBG->adr[j].type,
					g_ammu_session_table_ptr_DBG->adr[j].iova,
					g_ammu_session_table_ptr_DBG->adr[j].eva);
			} else {
				AMMU_LOG_DBG("> entry%3u:     %u | 0x%8x | 0x%8x |\n", j,
					g_ammu_session_table_ptr_DBG->adr[j].type,
					g_ammu_session_table_ptr_DBG->adr[j].iova,
					g_ammu_session_table_ptr_DBG->adr[j].eva);
			}
		}
		AMMU_LOG_DBG("== APUMMU dump session table End ==\n");
		break;
	}

	ret = count;
out:
	kfree(tmp);
	return ret;
}

static const struct file_operations apummu_dbg_fops_kernel = {
	.open = simple_open,
	.read = apummu_dbg_kernel_read,
	.write = apummu_dbg_write_kernel,
	.llseek = default_llseek,
};
#endif

void apummu_dbg_init(struct apummu_dev_info *adv, struct dentry *apu_dbg_root)
{
	g_ammu_klog = 0xF; /* log all on, plz refer to apummu_cmn.h*/

	/* create apummu FS root node */
	apummu_dbg_root = debugfs_create_dir(APUMMU_DBG_DIR, apu_dbg_root);
	if (IS_ERR_OR_NULL(apummu_dbg_root)) {
		AMMU_LOG_WRN("failed to create debug dir.\n");
		goto fail;
	}

	/* create log level */
	debugfs_create_u32("klog", 0644,
			apummu_dbg_root, &g_ammu_klog);

	/* create remote op node */
	apummu_dbg_remote_op = debugfs_create_file("op", 0644,
			apummu_dbg_root, adv,
			&apummu_dbg_fops_remoteop);
	if (IS_ERR_OR_NULL(apummu_dbg_remote_op)) {
		AMMU_LOG_WRN("failed to create debug node(op).\n");
		goto fail;
	}

#if KERNEL_DBG
	/* create kernel debug(dump) node */
	apummu_dbg_kernel = debugfs_create_file("kernel", 0644,
			apummu_dbg_root, adv,
			&apummu_dbg_fops_kernel);
	if (IS_ERR_OR_NULL(apummu_dbg_kernel)) {
		AMMU_LOG_WRN("failed to create debug node(kernel).\n");
		goto fail;
	}
#endif

	return;
fail:
	apummu_dbg_destroy(adv);
}

void apummu_dbg_destroy(struct apummu_dev_info *adv)
{
	debugfs_remove_recursive(apummu_dbg_root);
}
