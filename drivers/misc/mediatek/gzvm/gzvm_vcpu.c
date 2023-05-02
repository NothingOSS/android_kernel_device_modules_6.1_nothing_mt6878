// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2022 MediaTek Inc.
 */
#include <asm/sysreg.h>
#include <linux/anon_inodes.h>
#include <linux/file.h>
#include <linux/irqchip/arm-gic-v3.h>
#include <linux/ktime.h>
#include <linux/kvm_host.h>
#include <linux/mm.h>

#include "gzvm.h"

static long gzvm_vcpu_update_regs(struct gzvm_vcpu *vcpu, void * __user argp,
				  bool is_write)
{
	struct arm_smccc_res res;
	unsigned long a0, a1;

	if (!is_write)
		a0 = MT_HVC_GZVM_GET_REGS;
	else {
		a0 = MT_HVC_GZVM_SET_REGS;
		if (copy_from_user(vcpu->vm_regs, argp,
				   sizeof(struct gzvm_cpu_user_regs)))
			return -EFAULT;
	}
	a1 = assemble_vm_vcpu_tuple(vcpu->gzvm->vm_id, vcpu->vcpuid);
	gzvm_hypcall_wrapper(a0, a1, 0, 0, 0, 0, 0, 0, &res);

	if (!is_write && res.a0 == 0) {
		if (copy_to_user(argp, vcpu->vm_regs,
				 sizeof(struct gzvm_cpu_user_regs)))
			return -EFAULT;
	}

	return res.a0;
}

static int gzvm_vcpu_update_one_reg_hyp(struct gzvm_vcpu *vcpu, __u64 reg_id,
					bool is_write, __u64 *data)
{
	struct arm_smccc_res res;
	unsigned long a1;

	a1 = assemble_vm_vcpu_tuple(vcpu->gzvm->vm_id, vcpu->vcpuid);
	if (!is_write) {
		gzvm_hypcall_wrapper(MT_HVC_GZVM_GET_ONE_REG,
				     a1, reg_id, 0, 0, 0, 0, 0, &res);
		if (res.a0 == 0)
			*data = res.a1;
	} else {
		gzvm_hypcall_wrapper(MT_HVC_GZVM_SET_ONE_REG,
				     a1, reg_id, *data, 0, 0, 0, 0, &res);
	}

	return res.a0;
}

static long gzvm_vcpu_update_one_reg(struct gzvm_vcpu *vcpu, void * __user argp,
				     bool is_write)
{
	long ret;
	__u64 reg_size, data = 0;
	struct gzvm_one_reg reg;
	void __user *reg_addr;

	if (copy_from_user(&reg, argp, sizeof(reg)))
		return -EFAULT;
	reg_addr = (void __user *)reg.addr;

	/* reg id follows KVM's encoding */
	switch (reg.id & GZVM_REG_ARM_COPROC_MASK) {
	case GZVM_REG_ARM_CORE:
		break;
	case GZVM_REG_ARM_FW:
	case GZVM_REG_ARM64_SVE:
	default:
		return -EOPNOTSUPP;
	}

	reg_size = 1 << ((reg.id & GZVM_REG_SIZE_MASK) >> GZVM_REG_SIZE_SHIFT);
	if (is_write) {
		if (copy_from_user(&data, reg_addr, reg_size))
			return -EFAULT;
	}

	ret = gzvm_vcpu_update_one_reg_hyp(vcpu, reg.id, is_write, &data);

	if (!is_write && ret == 0) {
		if (copy_to_user(reg_addr, &data, reg_size))
			return -EFAULT;
	}

	return ret;
}

/**
 * @brief try to handle mmio in kernel space
 *
 * @param vcpu
 * @return true this mmio exit has been processed.
 * @return false this mmio exit has not been processed, require userspace.
 */
static bool gzvm_vcpu_handle_mmio(struct gzvm_vcpu *vcpu)
{
	__u64 addr;
	__u32 len;
	const void *val_ptr;
	/* TODO: so far, we don't have in-kernel mmio read handler */
	if (!vcpu->run->mmio.is_write)
		return false;
	addr = vcpu->run->mmio.phys_addr;
	len = vcpu->run->mmio.size;
	val_ptr = &vcpu->run->mmio.data;

	return gzvm_ioevent_write(vcpu, addr, len, val_ptr);
}

static bool lr_signals_eoi(uint64_t lr_val)
{
	return !(lr_val & ICH_LR_STATE) && (lr_val & ICH_LR_EOI) &&
	       !(lr_val & ICH_LR_HW);
}

/**
 * @brief check all LRs synced from gz hypervisor
 * Traverse all LRs, see if any EOIed vint, notify_acked_irq if any.
 * GZ does not fold/unfold everytime KVM_RUN, so we have to traverse all saved
 * LRs. It will not takes much more time comparing to fold/unfold everytime
 * GZVM_RUN, because there are only few LRs.
 */
static void gzvm_sync_vgic_state(struct gzvm_vcpu *vcpu)
{
	int i;

	for (i = 0; i < vcpu->hwstate->nr_lrs; i++) {
		uint32_t vintid;
		uint64_t lr_val = vcpu->hwstate->lr[i];
		/* 0 means unused */
		if (!lr_val)
			continue;

		vintid = lr_val & ICH_LR_VIRTUAL_ID_MASK;
		if (lr_signals_eoi(lr_val)) {
			gzvm_notify_acked_irq(vcpu->gzvm,
					      vintid - VGIC_NR_PRIVATE_IRQS);
		}
	}
}

static void gzvm_sync_hwstate(struct gzvm_vcpu *vcpu)
{
	gzvm_sync_vgic_state(vcpu);
}

ktime_t exit_start_time;
/**
 * @brief Handle vcpu run ioctl, entry point to guest and exit point from guest
 *
 * @param filp
 * @param argp pointer to struct gzvm_vcpu_run in userspace
 * @return long
 */
static long gzvm_vcpu_run(struct gzvm_vcpu *vcpu, void * __user argp)
{
	unsigned long a1;
	struct arm_smccc_res res;
	bool need_userspace = false;

	if (copy_from_user(vcpu->run, argp, sizeof(struct gzvm_vcpu_run)))
		return -EFAULT;

	if (vcpu->run->immediate_exit == 1) {
		exit_start_time = ktime_get();
		return -EINTR;
	}

	a1 = assemble_vm_vcpu_tuple(vcpu->gzvm->vm_id, vcpu->vcpuid);
	while (!need_userspace && !signal_pending(current)) {
		gzvm_hypcall_wrapper(MT_HVC_GZVM_RUN, a1, 0, 0, 0, 0, 0, 0,
				     &res);
		switch (res.a1) {
		case GZVM_EXIT_MMIO:
			if (!gzvm_vcpu_handle_mmio(vcpu))
				need_userspace = true;
			break;
		case GZVM_EXIT_IRQ:
			break;
		case GZVM_EXIT_HVC:
		case GZVM_EXIT_EXCEPTION:		/* TODO */
		case GZVM_EXIT_DEBUG:			/* TODO */
		case GZVM_EXIT_FAIL_ENTRY:		/* TODO */
		case GZVM_EXIT_INTERNAL_ERROR:		/* TODO */
		case GZVM_EXIT_SYSTEM_EVENT:		/* TODO */
		case GZVM_EXIT_SHUTDOWN:		/* TODO */
			need_userspace = true;
			break;
		case GZVM_EXIT_UNKNOWN:
		default:
			GZVM_ERR("unknown exit\n");
			need_userspace = true;
			goto out;
		}

		gzvm_sync_hwstate(vcpu);
	}

out:
	if (copy_to_user(argp, vcpu->run, sizeof(struct gzvm_vcpu_run)))
		return -EFAULT;
	if (signal_pending(current))
		return -ERESTARTSYS;
	return 0;
}

static long gzvm_vcpu_ioctl(struct file *filp, unsigned int ioctl,
			    unsigned long arg)
{
	int ret = -EINVAL;
	void __user *argp = (void __user *)arg;
	struct gzvm_vcpu *vcpu = filp->private_data;

	switch (ioctl) {
	case GZVM_RUN:
		return gzvm_vcpu_run(vcpu, argp);
	case GZVM_GET_REGS:
		return gzvm_vcpu_update_regs(vcpu, argp, false /*is_write*/);
	case GZVM_SET_REGS:
		return gzvm_vcpu_update_regs(vcpu, argp, true  /*is_write*/);
	case GZVM_GET_ONE_REG:
		return gzvm_vcpu_update_one_reg(vcpu, argp, false /*is_write*/);
	case GZVM_SET_ONE_REG:
		return gzvm_vcpu_update_one_reg(vcpu, argp, true  /*is_write*/);
	case GZVM_ARM_VCPU_INIT:
		return 0;
	case GZVM_SET_SIGNAL_MASK:
	case GZVM_GET_MP_STATE:
	case GZVM_SET_MP_STATE:
	case GZVM_SET_GUEST_DEBUG:
	case GZVM_GET_VCPU_EVENTS:
	case GZVM_SET_VCPU_EVENTS:
	case GZVM_GET_REG_LIST:
	case GZVM_ARM_VCPU_FINALIZE:
		/* TODO */
		return -EOPNOTSUPP;
	default:
		GZVM_ERR("%s invalid ioctl=0x%x\n", __func__, ioctl);
		ret = -EINVAL;
	}

	return ret;
}

static int gzvm_vcpu_release(struct inode *inode, struct file *filp)
{
	return 0;
}

static const struct file_operations gzvm_vcpu_fops = {
	.release        = gzvm_vcpu_release,
	.unlocked_ioctl = gzvm_vcpu_ioctl,
	.llseek		= noop_llseek,
};

static int gzvm_destroy_vcpu_hyp(gzvm_id_t vm_id, int vcpuid)
{
	struct arm_smccc_res res;
	unsigned long a1;

	a1 = assemble_vm_vcpu_tuple(vm_id, vcpuid);
	gzvm_hypcall_wrapper(MT_HVC_GZVM_DESTROY_VCPU, a1, 0, 0, 0, 0, 0, 0,
			     &res);

	return 0;
}

/**
 * @brief call smc to gz hypervisor to create vcpu
 *
 * @param run virtual address of vcpu->run
 * @return int
 */
static int gzvm_create_vcpu_hyp(gzvm_id_t vm_id, int vcpuid, void *run)
{
	struct arm_smccc_res res;
	unsigned long a1, a2;

	a1 = assemble_vm_vcpu_tuple(vm_id, vcpuid);
	a2 = (__u64)virt_to_phys(run);
	gzvm_hypcall_wrapper(MT_HVC_GZVM_CREATE_VCPU, a1, a2, 0, 0, 0, 0, 0,
			     &res);

	return 0;
}

void gzvm_destroy_vcpu(struct gzvm_vcpu *vcpu)
{
	if (!vcpu)
		return;

	gzvm_destroy_vcpu_hyp(vcpu->gzvm->vm_id, vcpu->vcpuid);
	free_pages_exact(vcpu->run, GZVM_VCPU_RUN_MAP_SIZE);
	kfree(vcpu);
}

/**
 * @brief Allocates an inode for the vcpu.
 */
static int create_vcpu_fd(struct gzvm_vcpu *vcpu)
{
	/* sizeof("gzvm-vcpu:") + max(strlen(itoa(vcpuid))) + null */
	char name[10 + ITOA_MAX_LEN + 1];

	snprintf(name, sizeof(name), "gzvm-vcpu:%d", vcpu->vcpuid);
	return anon_inode_getfd(name, &gzvm_vcpu_fops, vcpu, O_RDWR | O_CLOEXEC);
}

/**
 * @brief GZVM_CREATE_VCPU
 *
 * @param cpuid = arg
 * @return fd of vcpu, negative errno if error occurs
 */
int gzvm_vm_ioctl_create_vcpu(struct gzvm *gzvm, u32 cpuid)
{
	struct gzvm_vcpu *vcpu;
	int ret;

	if (cpuid >= GZVM_MAX_VCPUS)
		return -EINVAL;

	vcpu = kzalloc(sizeof(*vcpu), GFP_KERNEL);
	if (!vcpu)
		return -ENOMEM;

	BUILD_BUG_ON((sizeof(*vcpu->run) + sizeof(*vcpu->vm_regs)) > PAGE_SIZE);
	BUILD_BUG_ON(sizeof(struct gzvm_vcpu_hwstate) > PAGE_SIZE);
	/**
	 * allocate 2 pages for data sharing between driver and gz hypervisor
	 * |- page 0                   -|- page 1      -|
	 * |gzvm_vcpu_run|vm_regs|......|hwstate|.......|
	 */
	vcpu->run = alloc_pages_exact(GZVM_VCPU_RUN_MAP_SIZE,
				      GFP_KERNEL_ACCOUNT | __GFP_ZERO);
	if (!vcpu->run) {
		ret = -ENOMEM;
		goto free_vcpu;
	}
	vcpu->vm_regs = (void *)vcpu->run + sizeof(struct gzvm_vcpu_run);
	vcpu->hwstate = (void *)vcpu->run + PAGE_SIZE;
	vcpu->vcpuid = cpuid;
	vcpu->gzvm = gzvm;
	mutex_init(&vcpu->lock);

	ret = gzvm_create_vcpu_hyp(gzvm->vm_id, vcpu->vcpuid, vcpu->run);
	if (ret < 0)
		goto free_vcpu_run;

	ret = create_vcpu_fd(vcpu);
	if (ret < 0)
		goto free_vcpu_run;
	gzvm->vcpus[cpuid] = vcpu;

	return ret;

free_vcpu_run:
	free_pages_exact(vcpu->run, PAGE_SIZE * 2);
free_vcpu:
	kfree(vcpu);
	return ret;
}
