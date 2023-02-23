// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2022 MediaTek Inc.
 */

#include <linux/anon_inodes.h>
#include <linux/arm-smccc.h>
#include <linux/device.h>
#include <linux/file.h>
#include <linux/kdev_t.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/slab.h>

#include "gzvm.h"

static void (*invoke_gzvm_fn)(unsigned long, unsigned long, unsigned long,
			      unsigned long, unsigned long, unsigned long,
			      unsigned long, unsigned long,
			      struct arm_smccc_res *);

static void gzvm_hvc(unsigned long a0, unsigned long a1, unsigned long a2,
		      unsigned long a3, unsigned long a4, unsigned long a5,
		      unsigned long a6, unsigned long a7,
		      struct arm_smccc_res *res)
{
	arm_smccc_hvc(a0, a1, a2, a3, a4, a5, a6, a7, res);
}

static void gzvm_smc(unsigned long a0, unsigned long a1, unsigned long a2,
		      unsigned long a3, unsigned long a4, unsigned long a5,
		      unsigned long a6, unsigned long a7,
		      struct arm_smccc_res *res)
{
	arm_smccc_smc(a0, a1, a2, a3, a4, a5, a6, a7, res);
}

static void probe_gzvm_conduit(void)
{
	struct arm_smccc_res res;

	arm_smccc_hvc(MT_HVC_GZVM_PROBE, 0, 0, 0, 0, 0, 0, 0, &res);
	if (res.a0 == 0)
		invoke_gzvm_fn = gzvm_hvc;
	else {
		GZVM_DEBUG("Using smc conduit\n");
		invoke_gzvm_fn = gzvm_smc;
	}
}

void gzvm_hypcall_wrapper(unsigned long a0, unsigned long a1, unsigned long a2,
			  unsigned long a3, unsigned long a4, unsigned long a5,
			  unsigned long a6, unsigned long a7,
			  struct arm_smccc_res *res)
{
	invoke_gzvm_fn(a0, a1, a2, a3, a4, a5, a6, a7, res);
}

/**
 * @brief Check if given capability is support or not
 *
 * @param user_args GZVM_CAP_*
 * @return 1: support, 0: not support
 */
long gzvm_dev_ioctl_check_extension(struct gzvm *gzvm, unsigned long args)
{
	/* TODO */
	return 0;
}

static long gzvm_dev_ioctl(struct file *filp, unsigned int cmd,
			    unsigned long user_args)
{
	long ret = -EINVAL;

	switch (cmd) {
	case GZVM_GET_API_VERSION:
		return GZVM_DRIVER_VERSION;
	case GZVM_CREATE_VM:
		ret = gzvm_dev_ioctl_create_vm(user_args);
		break;
	case GZVM_GET_VCPU_MMAP_SIZE:
		if (user_args)
			goto out;
		ret = GZVM_VCPU_MMAP_SIZE;
		break;
	case GZVM_CHECK_EXTENSION:
		return gzvm_dev_ioctl_check_extension(NULL, user_args);
	default:
		ret = -EINVAL;
	}
out:
	return ret;
}

static const struct file_operations gzvm_chardev_ops = {
	.unlocked_ioctl = gzvm_dev_ioctl,
	.llseek		= noop_llseek,
};

static struct miscdevice gzvm_dev = {
	/* TODO: is it need to be fixed? */
	.minor = MISC_DYNAMIC_MINOR,
	.name = MODULE_NAME,
	.fops = &gzvm_chardev_ops,
};

static int gzvm_init(void)
{
	int ret;

	/* TODO: Using device tree to enable this driver */
	/* TODO: probe if gz can support gzvm commands */
	ret = gzvm_irqfd_init();
	if (ret) {
		GZVM_ERR("Failed to initial irqfd\n");
		return ret;
	}
	ret = misc_register(&gzvm_dev);
	if (ret) {
		GZVM_ERR("misc device register failed\n");
		return ret;
	}

	probe_gzvm_conduit();

	GZVM_INFO("gzvm driver init completes\n");

	return ret;
}

static void gzvm_exit(void)
{
	gzvm_irqfd_exit();
	misc_deregister(&gzvm_dev);
}

module_init(gzvm_init);
module_exit(gzvm_exit);

MODULE_AUTHOR("MediaTek");
MODULE_DESCRIPTION("GenieZone interface for VMM");
MODULE_LICENSE("GPL");
