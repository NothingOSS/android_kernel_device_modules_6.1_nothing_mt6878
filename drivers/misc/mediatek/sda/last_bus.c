// SPDX-License-Identifier: GPL-2.0

/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <asm/cputype.h>
#include <linux/arm-smccc.h>
#include <linux/atomic.h>
#include <linux/bug.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/irqreturn.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/of_irq.h>
#include <linux/platform_device.h>
#include <linux/printk.h>
#include <linux/proc_fs.h>
#include <linux/workqueue.h>
#include <linux/sched/clock.h>
#include <linux/seq_file.h>
#include <mt-plat/aee.h>
#include "sda.h"
#include "dbg_error_flag.h"
#include "last_bus.h"

#define DUMP_BUFF_SIZE 0x5000
static struct cfg_lastbus my_cfg_lastbus;
static char dump_buf[DUMP_BUFF_SIZE];
static int dump_buf_size;
#if IS_ENABLED(CONFIG_MTK_LASTBUS_DEBUG)
static struct proc_dir_entry *entry;
#endif

static int check_buf_size(int size)
{
	unsigned int buf_point = dump_buf_size % DUMP_BUFF_SIZE;

	if (buf_point + size > DUMP_BUFF_SIZE) {
		buf_point = 0;
		dump_buf_size = DUMP_BUFF_SIZE;
	}
	return buf_point;
}

static unsigned long long gray_code_to_binary_convert(unsigned long long gray_code)
{
	unsigned long long value = gray_code;

	while (gray_code > 0) {
		gray_code >>= 1;
		value ^= gray_code;
	}

	return value;
}

static void lastbus_dump_monitor(const struct lastbus_monitor *m, void __iomem *base)
{
	unsigned int i;
	unsigned long long grad_code, bin_code;
	unsigned int buf_point = 0;

	buf_point = check_buf_size(50);
	dump_buf_size += snprintf(dump_buf + buf_point, DUMP_BUFF_SIZE - buf_point,
		"%s 0x%08x %d\n", m->name, m->base, m->num_ports);
	pr_info("%s 0x%08x %d\n", m->name, m->base, m->num_ports);

	for (i = 0; i < m->num_ports; i++) {
		buf_point = check_buf_size(10);
		dump_buf_size += snprintf(dump_buf + buf_point, DUMP_BUFF_SIZE - buf_point,
			"%08x\n", readl(base + 0x408 + i * 4));
		pr_info("%08x\n", readl(base + 0x408 + i * 4));
	}

	grad_code = readl(base + 0x404);
	grad_code = (grad_code << 32) | readl(base + 0x400);
	bin_code = gray_code_to_binary_convert(grad_code);
	pr_info("%s: gray_code = 0x%llx, binary = 0x%llx\n", __func__, grad_code, bin_code);
	buf_point = check_buf_size(50);
	dump_buf_size += snprintf(dump_buf + buf_point, DUMP_BUFF_SIZE - buf_point,
		"timestamp: 0x%llx\n", bin_code);
}

static int lastbus_dump(int force_dump)
{
	unsigned int monitors_num = 0, i;
	unsigned int buf_point = 0;
	struct lastbus_monitor *m = NULL;
	u64 local_time = local_clock();
	bool is_timeout = false;
	void __iomem *base;
	uint32_t value = 0;

	do_div(local_time, 1000000);
	buf_point = check_buf_size(50);
	dump_buf_size += snprintf(dump_buf + buf_point, DUMP_BUFF_SIZE - buf_point,
		"dump lastbus %d, kernel time %llu ms.\n", force_dump, local_time);

	monitors_num = my_cfg_lastbus.num_used_monitors;

	for (i = 0; i < monitors_num; i++) {
		is_timeout = false;

		m = &my_cfg_lastbus.monitors[i];
		base = ioremap(m->base, ((0x408 + m->num_ports * 4) / 0x100 + 1) * 0x100);
		value = readl(base);
		is_timeout = value & LASTBUS_TIMEOUT;

		if (is_timeout || force_dump) {
			pr_info("%s: lastbus timeout happened(%d) (%s)\n",
				__func__, is_timeout, m->name);
			lastbus_dump_monitor(m, base);
		}
		iounmap(base);
	}

	return 1;
}

static int last_bus_dump_event(struct notifier_block *this,
	unsigned long err_flag_status, void *ptr)
{
	unsigned long last_bus_err_status;

	last_bus_err_status = get_dbg_error_flag_mask(INFRA_LASTBUS_TIMEOUT) |
				get_dbg_error_flag_mask(PERI_LASTBUS_TIMEOUT);


	if (!(err_flag_status & last_bus_err_status)) {
		pr_err("err_flag_status %lx, last_bus_err_status %lx\n",
			err_flag_status, last_bus_err_status);
		return 0;
	}
	lastbus_dump(0);

#if IS_ENABLED(CONFIG_MTK_AEE_FEATURE)
	aee_kernel_exception("last_bus_timeout", "last bus timeout detect");
#endif

	return 0;
}

static struct notifier_block dbg_error_flag_notifier_last_bus = {
	.notifier_call = last_bus_dump_event,
};


static int last_bus_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *np = pdev->dev.of_node;
	struct device_node *parts_node, *child_part;
	const char *str;
	int num = 0, ret;

	dev_info(dev, "driver probed\n");


	/* get enabled */
	ret = of_property_read_u32(np, "enabled", &my_cfg_lastbus.enabled);
	if (ret < 0) {
		dev_err(dev, "couldn't find property enabled(%d)\n", ret);
		my_cfg_lastbus.enabled = 0;
		return -ENODATA;
	}


	/* get sw_version */
	ret = of_property_read_u32(np, "sw-version", &my_cfg_lastbus.sw_version);
	if (ret < 0) {
		dev_err(dev, "couldn't find property sw-version(%d)\n", ret);
		my_cfg_lastbus.sw_version = LASTBUS_SW_V1;
	}

	/* get timeout_ms */
	ret = of_property_read_u32(np, "timeout-ms", &my_cfg_lastbus.timeout_ms);
	if (ret < 0) {
		dev_err(dev, "couldn't find property timeout-ms(%d)\n", ret);
		my_cfg_lastbus.timeout_ms = 0xFFFFFFFF;
	}

	/* get timeout_type */
	ret = of_property_read_u32(np, "timeout-type", &my_cfg_lastbus.timeout_type);
	if (ret < 0) {
		dev_err(dev, "couldn't find property timeout-type(%d)\n", ret);
		my_cfg_lastbus.timeout_type = LASTBUS_TIMEOUT_FIRST;
	}

	parts_node = of_get_child_by_name(np, "monitors");
	if (!parts_node) {
		dev_err(dev, "couldn't find property monitors(%d)\n", ret);
		return -ENODATA;
	}

	for_each_child_of_node(parts_node, child_part) {
		/* get monitor name */
		ret = of_property_read_string(child_part, "monitor-name", &str);
		if (ret < 0) {
			dev_err(dev, "%s: couldn't find property monitor-name\n", __func__);
			return -ENODATA;
		}

		if (strlen(str) <= MAX_MONITOR_NAME_LEN) {
			strncpy(my_cfg_lastbus.monitors[num].name, str, strlen(str));
		} else {
			strncpy(my_cfg_lastbus.monitors[num].name, str,	MAX_MONITOR_NAME_LEN);
			my_cfg_lastbus.monitors[num].name[MAX_MONITOR_NAME_LEN-1] = '\0';
		}

		pr_info("%s: name = %s\n", __func__, my_cfg_lastbus.monitors[num].name);

		/* get monitor base */
		ret = of_property_read_u32(child_part, "base", &my_cfg_lastbus.monitors[num].base);
		if (ret < 0) {
			dev_err(dev, "couldn't find property monitor base(%d)\n", ret);
			return -ENODATA;
		}

		/* get monitor num_ports */
		ret = of_property_read_u32(child_part, "num-ports",
			&my_cfg_lastbus.monitors[num].num_ports);
		if (ret < 0) {
			dev_err(dev, "couldn't find property num_ports(%d)\n", ret);
			return -ENODATA;
		}

		/* get monitor bus_freq_mhz */
		ret = of_property_read_u32(child_part, "bus-freq-mhz",
			&my_cfg_lastbus.monitors[num].bus_freq_mhz);
		if (ret < 0) {
			dev_err(dev, "couldn't find property bus_freq_mhz(%d)\n", ret);
			return -ENODATA;
		}

		num++;
	}

	if (my_cfg_lastbus.enabled == 1 && num != 0) {
		dbg_error_flag_register_notify(&dbg_error_flag_notifier_last_bus);

		my_cfg_lastbus.num_used_monitors = num;
		if (num > NR_MAX_LASTBUS_MONITOR) {
			dev_err(dev, "%s: Error: number of monitors(%d) is great than %d!\n",
					__func__, num, NR_MAX_LASTBUS_MONITOR);
			return -EINVAL;
		}
		pr_info("%s: num_used_monitors = %d\n", __func__, num);
	}

	return 0;
}

static int last_bus_remove(struct platform_device *pdev)
{
	dev_info(&pdev->dev, "driver removed\n");

	return 0;
}

static const struct of_device_id last_bus_of_ids[] = {
	{ .compatible = "mediatek,lastbus", },
	{}
};

static struct platform_driver last_bus_drv = {
	.driver = {
		.name = "last_bus",
		.bus = &platform_bus_type,
		.owner = THIS_MODULE,
		.of_match_table = last_bus_of_ids,
	},
	.probe = last_bus_probe,
	.remove = last_bus_remove,
};

#if IS_ENABLED(CONFIG_MTK_LASTBUS_DEBUG)
static int last_bus_show(struct seq_file *m, void *v)
{
	int i, point;


	if (my_cfg_lastbus.enabled == 1 && my_cfg_lastbus.num_used_monitors != 0) {
		seq_puts(m, "=== Last bus monitor: ===\n");

		for (i = 0; i < my_cfg_lastbus.num_used_monitors; i++) {
			seq_printf(m, "monitor-name: %s, ports %d.\n",
				my_cfg_lastbus.monitors[i].name,
				my_cfg_lastbus.monitors[i].num_ports);
		}

		if (dump_buf_size != 0) {
			if (dump_buf_size > DUMP_BUFF_SIZE) {
				point = dump_buf_size % DUMP_BUFF_SIZE;
				seq_write(m, (char *)(dump_buf + point), DUMP_BUFF_SIZE - point);
			}
			point = dump_buf_size % DUMP_BUFF_SIZE;
			seq_write(m, (char *)dump_buf, point);
		}
	} else
		seq_puts(m, "Last bus monitor not enable.\n");
	return 0;
}


static ssize_t last_bus_write(struct file *filp,
	const char *ubuf, size_t cnt, loff_t *data)
{
	char buf[64];
	long val;
	int ret;

	if (cnt >= sizeof(buf))
		return -EINVAL;

	if (copy_from_user(&buf, ubuf, cnt))
		return -EFAULT;

	buf[cnt] = 0;

	ret = kstrtoul(buf, 10, (unsigned long *)&val);

	if (ret < 0)
		return ret;

	switch (val) {
	case 0:
		lastbus_dump(0);
		break;
	case 1:
		lastbus_dump(1);
		break;
	default:
		break;
	}
	return cnt;
}


/*** Seq operation of last_bus ****/
static int last_bus_open(struct inode *inode, struct file *file)
{
	return single_open(file, last_bus_show, inode->i_private);
}

static const struct proc_ops last_bus_fops = {
	.proc_open = last_bus_open,
	.proc_write = last_bus_write,
	.proc_read = seq_read,
	.proc_lseek = seq_lseek,
	.proc_release = single_release,
};
#endif

static int __init last_bus_init(void)
{
	int ret;

	ret = platform_driver_register(&last_bus_drv);
	if (ret)
		return ret;

#if IS_ENABLED(CONFIG_MTK_LASTBUS_DEBUG)
	entry = proc_create("last_bus", 0664, NULL, &last_bus_fops);
	if (!entry)
		return -ENOMEM;
#endif
	return 0;
}

static __exit void last_bus_exit(void)
{
	platform_driver_unregister(&last_bus_drv);
#if IS_ENABLED(CONFIG_MTK_LASTBUS_DEBUG)
	if (entry)
		proc_remove(entry);
#endif
}

module_init(last_bus_init);
module_exit(last_bus_exit);

MODULE_DESCRIPTION("MediaTek Last Bus Driver");
MODULE_LICENSE("GPL");
