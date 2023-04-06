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

#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/types.h>


#define WLC_CMD_BUFFER_SIZE 256
struct miscdevice *wlcdrv_device;
static char cmd_buf[WLC_CMD_BUFFER_SIZE];

#define MAX_CPU_CORE_NUMBER 8
#define TEST_PMU_INPUT_SIZE 8
#define MAX_PMU_ARRAY_SIZE (MAX_CPU_CORE_NUMBER * TEST_PMU_INPUT_SIZE)
static int g_wlc_latest_send_cmd	 = WLCIPI_CMD_DEFAULT;


#define SCENARIO_TYPE_BIT_OFFSET	 (0)
#define LCPUCLUSTER_TYPE_BIT_OFFSET	 (8)
#define MCPUCLUSTER_TYPE_BIT_OFFSET	 (16)
#define BCPUCLUSTER_TYPE_BIT_OFFSET	 (24)

#define SCENARIO_TYPE_BITMASK	 (0x000000FF << SCENARIO_TYPE_BIT_OFFSET)
#define LCPUCLUSTER_TYPE_BITMASK (0x000000FF << LCPUCLUSTER_TYPE_BIT_OFFSET)
#define MCPUCLUSTER_TYPE_BITMASK (0x000000FF << MCPUCLUSTER_TYPE_BIT_OFFSET)
#define BCPUCLUSTER_TYPE_BITMASK (0x000000FF << BCPUCLUSTER_TYPE_BIT_OFFSET)

#define WLC_ENABLE_CHECK_ID		0x454E4142
#define WLC_DISABLE_CHECK_ID	0x44495341

#define WLCDRV_SRAM_OFFSET_DBG_CNT	0x4
#define WLCDRV_SRAM_OFFSET_DBG_SRAM 0x8
#define WLCDRV_SRAM_OFFSET_DBG_TYPE 0xc

static void __iomem *wl_sram_addr_iomapped;
static int g_wl_support;
static int g_wl_sram_addr;
static int g_wl_sram_sz;

static int g_wl_tbl_addr;
static int g_wl_tbl_sz;
/******************************************************************************
 * WLC driver Procfs file operations
 *****************************************************************************/
static int _wlc_proc_show(struct seq_file *m, void *v)
{
	int value = -1;

	seq_puts(m, "=== wl status ===\n");
	seq_printf(m, "Enable(dts):%d\n", g_wl_support);
	seq_printf(m, "addr:0x%x, sz:%d\n", g_wl_sram_addr, g_wl_sram_sz);
	seq_printf(m, "io_mapped:0x%lx\n",	 (unsigned long)wl_sram_addr_iomapped);
	seq_printf(m, "tbl base:0x%x, sz:%d\n", g_wl_tbl_addr, g_wl_tbl_sz);

	value = ioread32(wl_sram_addr_iomapped + WLCDRV_SRAM_OFFSET_DBG_SRAM);
	seq_printf(m, "addr(in wlc): 0x%08x\n", value);

	seq_printf(m, "curr ctrl cmd: 0x%x\n\n", g_wlc_latest_send_cmd);
	value = ioread32(wl_sram_addr_iomapped + 0);
	seq_printf(m, "inf wl-types: 0x%08x\n", value);

	value = ioread32(wl_sram_addr_iomapped + WLCDRV_SRAM_OFFSET_DBG_TYPE);
	seq_printf(m, "inf wl-types(per-core): 0x%08x\n", value);

	value = ioread32(wl_sram_addr_iomapped + WLCDRV_SRAM_OFFSET_DBG_CNT);
	seq_printf(m, "inf cnts: 0x%08x\n", value);

	seq_puts(m, "=== wl status ===\n\n");

	return 0;
}

static int _wlc_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, _wlc_proc_show, NULL);
}

static ssize_t _wlc_proc_write(struct file *fp, const char __user *userbuf,
				 size_t count, loff_t *f_pos)
{
	unsigned long val = 0;
	ssize_t ret;
	size_t length = count;

	if (length >= WLC_CMD_BUFFER_SIZE)
		length = WLC_CMD_BUFFER_SIZE - 1;
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
	.proc_write = _wlc_proc_write,
	.proc_read	= seq_read,
	.proc_lseek	  = seq_lseek,
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
	return 0;
}


static const struct file_operations _wlcdrv_debugfs_file_fops = {
	.open	= _wlcdrv_open,
	.release = _wlcdrv_release,
	.write	= _wlcdrv_write,
	.read	= _wlcdrv_read,
	.llseek = seq_lseek,
	.release = single_release,
};



/* -------------------------- */
static int wlcdrv_probe(void)
{
	int ret;
	struct device_node *wl_node;
	struct platform_device *pdev;
	struct resource *sram_res;
	int wl_support = 0;
	int wl_sram_addr = 0;
	int wl_sram_sz	 = 0;
	int wl_tbl_addr = 0;
	int wl_tbl_sz	= 0;
	int value = -1;

	/* Create debugfs */
	struct proc_dir_entry *procfs_wlc_dir = NULL;

	procfs_wlc_dir = proc_mkdir("wlcdrv", NULL);
	if (procfs_wlc_dir == NULL) {
		pr_info("[wlcdrv] can not create procfs directory: proc/wlcdrv\n");
		return -ENOMEM;
	}

	if (!proc_create("cmd", 0664, procfs_wlc_dir, &_wlc_proc_fops))
		pr_info("@%s: create /proc/wlcdrv/cmd failed\n", __func__);

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

	pr_info("[WLCDrv]Init the IPI from tinysys to mcupm\n");
	wlc_ipi_to_mcupm_init();

	wl_node = of_find_node_by_name(NULL, "wl-info");
	if (wl_node == NULL) {
		pr_info("failed to find node wl-info @ %s\n", __func__);
		return -ENODEV;
	}

	pdev = of_find_device_by_node(wl_node);
	if (pdev == NULL) {
		pr_info("failed to find pdev @ %s\n", __func__);
		return -EINVAL;
	}

	ret = of_property_read_u32(wl_node, "wl-support", &wl_support);
	if (ret < 0)
		pr_info("no wl-support found in wl-info node: %s\n",  __func__);

	sram_res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "wl_sram_base");

	if (sram_res) {
		wl_sram_addr = sram_res->start;
		wl_sram_sz	 = resource_size(sram_res);
		wl_sram_addr_iomapped = ioremap(sram_res->start, resource_size(sram_res));
	} else {
		pr_info("%s can't get resource\n", __func__);
		return -ENODEV;
	}

	sram_res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "wl_tbl_base");

	if (sram_res) {
		wl_tbl_addr = sram_res->start;
		wl_tbl_sz	 = resource_size(sram_res);
	} else {
		pr_info("%s can't get resource\n", __func__);
		return -ENODEV;
	}

	g_wl_support = wl_support;
	g_wl_sram_addr = wl_sram_addr;
	g_wl_sram_sz = wl_sram_sz;

	g_wl_tbl_addr = wl_tbl_addr;
	g_wl_tbl_sz = wl_tbl_sz;

	pr_info("[wl-info dts]enable:%d\n", wl_support);
	pr_info("[wl-info dts]sram addr:0x%x, sz:0x%x\n", wl_sram_addr, wl_sram_sz);
	pr_info("[wl-info dts]wl-tbl addr:0x%x, sz:0x%x\n", wl_tbl_addr, wl_tbl_sz);
	pr_info("[wl-info dts]io_mapped:0x%lx\n", (unsigned long)wl_sram_addr_iomapped);

	value = ioread32(wl_sram_addr_iomapped + WLCDRV_SRAM_OFFSET_DBG_CNT);
	pr_info("[wl-in-up]wl state:0x%x\n", value);

	return 0;
}


static int wlcdrv_remove(void)
{
	misc_deregister(wlcdrv_device);
	kfree(wlcdrv_device);

	if (wl_sram_addr_iomapped)
		iounmap(wl_sram_addr_iomapped);

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
