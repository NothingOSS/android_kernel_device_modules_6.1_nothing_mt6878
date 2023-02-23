// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2022 MediaTek Inc.
 */

#include <linux/module.h>
#include <linux/errno.h>
#include <linux/debugfs.h>
#include <linux/proc_fs.h>
#include <linux/miscdevice.h>
#include <linux/slab.h>
#include <linux/io.h>
#include "wlcdrv_ipi.h"

#define WLC_CMD_BUFFER_SIZE 256
struct miscdevice *wlcdrv_device;
static char cmd_buf[WLC_CMD_BUFFER_SIZE];

#define MAX_CPU_CORE_NUMBER 8
#define TEST_PMU_INPUT_SIZE 8
#define MAX_PMU_ARRAY_SIZE (MAX_CPU_CORE_NUMBER * TEST_PMU_INPUT_SIZE)
static int g_wlc_latest_send_cmd     = WLCIPI_CMD_DEFAULT;

#define WLC_SHARE_EB_BASE        (0x0003F000)
#define WLC_SHARE_AP_BASE        (WLC_SHARE_EB_BASE + 0x0C080000)

#define SCENARIO_TYPE_BIT_OFFSET     (0)
#define LCPUCLUSTER_TYPE_BIT_OFFSET  (8)
#define MCPUCLUSTER_TYPE_BIT_OFFSET  (16)
#define BCPUCLUSTER_TYPE_BIT_OFFSET  (24)

#define SCENARIO_TYPE_BITMASK    (0x000000FF << SCENARIO_TYPE_BIT_OFFSET)
#define LCPUCLUSTER_TYPE_BITMASK (0x000000FF << LCPUCLUSTER_TYPE_BIT_OFFSET)
#define MCPUCLUSTER_TYPE_BITMASK (0x000000FF << MCPUCLUSTER_TYPE_BIT_OFFSET)
#define BCPUCLUSTER_TYPE_BITMASK (0x000000FF << BCPUCLUSTER_TYPE_BIT_OFFSET)

/******************************************************************************
 * WLC driver Procfs file operations
 *****************************************************************************/
static int _wlc_proc_show(struct seq_file *m, void *v)
{
	return 0;
}


static int _wlc_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, _wlc_proc_show, NULL);
}


static ssize_t _wlc_proc_read(struct file *file, char __user *buf, size_t len,
			   loff_t *ppos)
{
	return 0;
}

static ssize_t _wlc_proc_write(struct file *fp, const char __user *userbuf,
				 size_t count, loff_t *f_pos)
{
	unsigned long val;
	ssize_t ret;
	size_t length = count;

	if (length > WLC_CMD_BUFFER_SIZE)
		length = WLC_CMD_BUFFER_SIZE;
	ret = length;

	if (copy_from_user(&cmd_buf, userbuf, length))
		return -EFAULT;

	cmd_buf[length] = 0;

	ret = kstrtoul(cmd_buf, 10, (unsigned long *)&val);

	g_wlc_latest_send_cmd = val;

	wlc_ipi_to_mcupm_send(val);

	return count;
}


static const struct proc_ops _wlc_proc_fops = {
	.proc_open = _wlc_proc_open,
	.proc_read  = _wlc_proc_read,
	.proc_write = _wlc_proc_write,
	.proc_lseek   = seq_lseek,
	.proc_release = single_release,
};


/******************************************************************************
 * WLC driver Procfs file operations
 *****************************************************************************/
static int _wlc_proc_param_show(struct seq_file *m, void *v)
{
	return 0;
}


static int _wlc_proc_param_open(struct inode *inode, struct file *file)
{
	return single_open(file, _wlc_proc_param_show, NULL);
}


static ssize_t _wlc_proc_param_read(struct file *file, char __user *buf, size_t len,
			   loff_t *ppos)
{
	uint32_t counts = 0;

	return counts;
}

static ssize_t _wlc_proc_param_write(struct file *fp, const char __user *userbuf,
				 size_t count, loff_t *f_pos)
{
	return count;
}


static int _wlc_proc_wlctype_show(struct seq_file *m, void *v)
{
	return 0;
}


static int _wlc_proc_wlctype_open(struct inode *inode, struct file *file)
{
	return single_open(file, _wlc_proc_wlctype_show, NULL);
}


static ssize_t _wlc_proc_wlctype_read(struct file *file, char __user *buf, size_t len,
			   loff_t *ppos)
{
	return 0;
}

static ssize_t _wlc_proc_wlctype_write(struct file *fp, const char __user *userbuf,
				 size_t count, loff_t *f_pos)
{
	int ret;
	int force_type = 0;
	size_t length = count;
	char *cmd_str;

	if (length > WLC_CMD_BUFFER_SIZE)
		length = WLC_CMD_BUFFER_SIZE;
	ret = length;

	if (copy_from_user(&cmd_buf, userbuf, length))
		return -EFAULT;

	cmd_buf[length] = '\0';
	cmd_str = &cmd_buf[0];

	if (kstrtouint(cmd_str, 10, &force_type))
		pr_info("%s Failed to get %s wl type number\n", __func__, cmd_str);

	return count;
}


static const struct proc_ops _wlc_proc_param_fops = {
	.proc_open  = _wlc_proc_param_open,
	.proc_read  = _wlc_proc_param_read,
	.proc_write = _wlc_proc_param_write,
	.proc_lseek   = seq_lseek,
	.proc_release = single_release,
};



static const struct proc_ops _wlc_proc_wlctype_fops = {
	.proc_open  = _wlc_proc_wlctype_open,
	.proc_read  = _wlc_proc_wlctype_read,
	.proc_write = _wlc_proc_wlctype_write,
	.proc_lseek   = seq_lseek,
	.proc_release = single_release,
};

/******************************************************************************
 * WLC driver Debugfs file operations
 *****************************************************************************/
static int _wlcdrv_open(struct inode *inode, struct file *filp)
{
	return 0;
}


static int _wlcdrv_release(struct inode *inode, struct file *filp)
{
	return 0;
}


static ssize_t _wlcdrv_read(struct file *filp, char __user *buf,
		size_t count, loff_t *f_pos)
{
	return 0;
}


static ssize_t _wlcdrv_write(struct file *filp, const char __user *ubuf,
		size_t count, loff_t *f_pos)
{
	unsigned long val;
	ssize_t ret;
	size_t length = count;

	if (length > 127)
		length = 127;
	ret = length;

	if (copy_from_user(&cmd_buf, ubuf, length))
		return -EFAULT;

	cmd_buf[length] = 0;

	ret = kstrtoul(cmd_buf, 10, (unsigned long *)&val);

	return count;
}


static const struct file_operations _wlcdrv_debugfs_file_fops = {
	.open   = _wlcdrv_open,
	.release = _wlcdrv_release,
	.write  = _wlcdrv_write,
	.read   = _wlcdrv_read,
	.llseek = seq_lseek,
	.release = single_release,
};


/* -------------------------- */
static int wlcdrv_probe(void)
{
	int ret;
	/* Create debugfs */
	struct proc_dir_entry *procfs_wlc_dir = NULL;

	procfs_wlc_dir = proc_mkdir("wlcdrv", NULL);
	if (procfs_wlc_dir == NULL) {
		pr_info("[wlcdrv] can not create procfs directory: proc/wlcdrv\n");
		return -ENOMEM;
	}

	if (!proc_create("cmd", 0664, procfs_wlc_dir, &_wlc_proc_fops))
		pr_info("@%s: create /proc/wlcdrv/cmd failed\n", __func__);

	if (!proc_create("parameters", 0664, procfs_wlc_dir, &_wlc_proc_param_fops))
		pr_info("@%s: create /proc/wlcdrv/parameters failed\n", __func__);

	if (!proc_create("wltype", 0664, procfs_wlc_dir, &_wlc_proc_wlctype_fops))
		pr_info("@%s: create /proc/wlcdrv/wltype failed\n", __func__);

	wlcdrv_device = kzalloc(sizeof(*wlcdrv_device), GFP_KERNEL);
	if (!wlcdrv_device)
		return -ENOMEM;

	wlcdrv_device->minor = MISC_DYNAMIC_MINOR;
	wlcdrv_device->name = "wlcdrv";
	wlcdrv_device->fops = &_wlcdrv_debugfs_file_fops;
	wlcdrv_device->parent = NULL;

	ret = misc_register(wlcdrv_device);
	if (ret) {
		pr_info("[wlcdrv]: failed to register misc device.\n");
		return ret;
	}

	dev_set_drvdata(wlcdrv_device->this_device, procfs_wlc_dir);

	pr_info("[WLCDrv]Init the IPI from TinySys to mcupm\n");
	wlc_ipi_to_mcupm_init();

	return 0;
}


static int wlcdrv_remove(void)
{
	misc_deregister(wlcdrv_device);
	kfree(wlcdrv_device);

	return 0;
}


static void __exit wlcdrv_exit(void)
{
	wlc_ipi_to_mcupm_deinit();
	wlcdrv_remove();
}

static int __init wlcdrv_init(void)
{
	wlcdrv_probe();
	return 0;
}



module_init(wlcdrv_init);
module_exit(wlcdrv_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("MediaTek WLC_DRV POC v0.1");
MODULE_AUTHOR("MediaTek Inc.");
