// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2022 MediaTek Inc.
 */

#include <linux/anon_inodes.h>
#include <linux/arm-smccc.h>
#include <linux/device.h>
#include <linux/file.h>
#include <linux/kdev_t.h>
#include <linux/kvm.h>
#include <linux/kvm_host.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/slab.h>

#include "gzvm.h"
#include "gzvm_ioctl.h"

void gzvm_hypcall_wrapper(unsigned long a0, unsigned long a1,
			unsigned long a2, unsigned long a3, unsigned long a4,
			unsigned long a5, unsigned long a6, unsigned long a7,
			struct arm_smccc_res *res)
{
	arm_smccc_smc(a0, a1, a2, a3, a4, a5, a6, a7, res);
	//arm_smccc_1_1_hvc();
	// arm_smccc_1_2_smc(&args, &res);
}

static long gzvm_dev_ioctl(struct file *filp, unsigned int cmd,
			    unsigned long user_args)
{
	long ret = -EINVAL;

	switch (cmd) {
	case KVM_CREATE_VM:
	case GZVM_CREATE_VM:
		ret = gzvm_dev_ioctl_creat_vm(user_args);
		break;
	// Need to know the size to allocate
	case KVM_GET_VCPU_MMAP_SIZE:
		/* TODO: remove this */
		GZVM_DEBUG("KVM_GET_VCPU_MMAP_SIZE\n");
		if (user_args)
			goto out;
		ret = GZVM_VCPU_MMAP_SIZE;
		break;
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

	/* TODO: probe if gz can support gzvm commands */
	ret = gzvm_irqfd_init();
	if (ret) {
		GZVM_ERR("Failed to initial irqfd\n");
		return ret;
	}
	ret = misc_register(&gzvm_dev);
	if (ret) {
		GZVM_ERR("misc device register failed\n");
		// goto out_unreg;
	}
	GZVM_INFO("%s %s completes\n", __FILE__, __func__);

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
MODULE_DESCRIPTION("GenieZone kvm-like interface");
MODULE_LICENSE("GPL");
