// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2022 MediaTek Inc.
 */
#include <asm/sysreg.h>
#include <linux/anon_inodes.h>
#include <linux/arm-smccc.h>
#include <linux/file.h>
#include <linux/irqchip/arm-gic-v3.h>
#include <linux/kdev_t.h>
#include <linux/kvm_host.h>
#include <linux/miscdevice.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/slab.h>

#include "gzvm.h"
#include "gzvm_ioctl.h"

static long gzvm_vcpu_update_regs(struct file *filp, unsigned long ioctl,
				  void * __user argp)
{
	struct gzvm_vcpu *vcpu = filp->private_data;
	struct arm_smccc_res res;
	unsigned long a0, a1;

	if (ioctl == KVM_GET_REGS)
		a0 = MT_SMC_FC_GZVM_GET_REGS;
	else {
		a0 = MT_SMC_FC_GZVM_SET_REGS;
		if (copy_from_user(vcpu->vm_regs, argp, sizeof(struct cpu_user_regs)))
			return -EFAULT;
	}
	a1 = assembel_vm_vcpu_tuple(vcpu->gzvm->vm_id, vcpu->vcpuid);
	gzvm_hypcall_wrapper(a0, a1, 0, 0, 0, 0, 0, 0, &res);

	GZVM_DEBUG("%s a0=%lx, a1=%lx, a2=%lx, a3=%lx\n", __func__,
		res.a0, res.a1, res.a2, res.a3);
	if (res.a0 == 0 && ioctl == KVM_GET_REGS) {
		if (copy_to_user(argp, vcpu->vm_regs, sizeof(struct cpu_user_regs)))
			return -EFAULT;
	}

	return res.a0;
}

static bool is_timer_reg(u64 index)
{
	switch (index) {
	case KVM_REG_ARM_TIMER_CTL:
	case KVM_REG_ARM_TIMER_CNT:
	case KVM_REG_ARM_TIMER_CVAL:
		return true;
	}
	return false;
}

static int gzvm_vcpu_access_one_reg_smc(struct gzvm_vcpu *vcpu,
	unsigned long ioctl, __u64 reg_id, __u64 *data)
{
	struct arm_smccc_res res;
	unsigned long a1;

	a1 = assembel_vm_vcpu_tuple(vcpu->gzvm->vm_id, vcpu->vcpuid);
	if (ioctl == KVM_GET_ONE_REG) {
		gzvm_hypcall_wrapper(MT_SMC_FC_GZVM_GET_ONE_REG,
				     a1, reg_id, 0, 0, 0, 0, 0, &res);
		if (res.a0 == 0)
			*data = res.a1;
	} else {
		gzvm_hypcall_wrapper(MT_SMC_FC_GZVM_SET_ONE_REG,
				     a1, reg_id, *data, 0, 0, 0, 0, &res);
	}

	return res.a0;
}

static long gzvm_vcpu_access_one_reg(struct file *filp, unsigned long ioctl,
	struct kvm_one_reg *reg)
{
	struct gzvm_vcpu *vcpu = filp->private_data;
	long ret;
	__u64 data, reg_size;
	void __user *reg_addr = (void __user *)reg->addr;

	switch (reg->id & KVM_REG_ARM_COPROC_MASK) {
	case KVM_REG_ARM_CORE:
		GZVM_ERR("TODO: reg->id = %llx KVM_REG_ARM_CORE\n", reg->id);
		break;
	case KVM_REG_ARM_FW:
		GZVM_ERR("TODO: reg->id = %llx KVM_REG_ARM_FW\n", reg->id);
		break;
	case KVM_REG_ARM64_SVE:
		GZVM_ERR("TODO: reg->id = %llx KVM_REG_ARM64_SVE\n", reg->id);
		break;
	default:
		if (is_timer_reg(reg->id))
			GZVM_ERR("TODO: reg->id = %llx timer reg?\n", reg->id);
		else
			GZVM_ERR("TODO: reg->id = %llx system reg?\n", reg->id);
	}

	reg_size = 1 << ((reg->id & KVM_REG_SIZE_MASK) >> KVM_REG_SIZE_SHIFT);
	if (ioctl == KVM_SET_ONE_REG) {
		if (copy_from_user(&data, reg_addr, reg_size))
			return -EFAULT;
	}

	ret = gzvm_vcpu_access_one_reg_smc(vcpu, ioctl, reg->id, &data);

	if (ret == 0 && ioctl == KVM_GET_ONE_REG) {
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
		GZVM_DEBUG("%s %d vint=%u %llx\n", __func__, i, vintid, lr_val);
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

/**
 * @brief Handle vcpu run ioctl, entry point to guest and exit point from guest
 *
 * @param filp
 * @param argp pointer to struct gzvm_vcpu_run in userspace
 * @return long
 */
static long gzvm_vcpu_run(struct file *filp, void * __user argp)
{
	struct gzvm_vcpu *vcpu = filp->private_data;
	unsigned long a1;
	struct arm_smccc_res res;
	bool need_userspace = false;

	if (copy_from_user(vcpu->run, argp, sizeof(struct gzvm_vcpu_run)))
		return -EFAULT;

	if (vcpu->run->immediate_exit == 1)
		return -EINTR;

	/* TODO: TBD: update vcpu reg if last_exit is mmio read:
	 * (1) copy_from_user when mmio read, (2) VMM will call SET_ONE_REG.
	 */
	if (vcpu->run->exit_reason == GZVM_EXIT_MMIO) {
		if (!vcpu->run->mmio.is_write) {
			if (copy_from_user(vcpu->run, argp,
					   sizeof(struct gzvm_vcpu_run)))
				return -EFAULT;
		}
	}

	a1 = assembel_vm_vcpu_tuple(vcpu->gzvm->vm_id, vcpu->vcpuid);
	do {
		gzvm_hypcall_wrapper(MT_SMC_FC_GZVM_RUN, a1, 0, 0, 0, 0, 0, 0, &res);
		switch (res.a1) {
		case GZVM_EXIT_MMIO:
			if (!gzvm_vcpu_handle_mmio(vcpu))
				need_userspace = true;
			break;
		case GZVM_EXIT_HVC:
			need_userspace = true;
			break;
		case GZVM_EXIT_IRQ: /* is it needed to back to userspace? */
			break;
		case GZVM_EXIT_UNKNOWN:
		default:
			GZVM_ERR("unknown exit\n");
			need_userspace = true;
			goto out;
		}

		gzvm_sync_hwstate(vcpu);
	} while (!need_userspace);
	// GZVM_DEBUG("%s VM_EXIT a0=%lx, a1=%lx, a2=%lx, a3=%lx\n", __func__,
	//	   res.a0, res.a1, res.a2, res.a3);
out:
	if (copy_to_user(argp, vcpu->run, sizeof(struct gzvm_vcpu_run)))
		return -EFAULT;
	return 0;
}

static long gzvm_vcpu_ioctl(struct file *filp, unsigned int ioctl,
			   unsigned long arg)
{
	int ret = -EINVAL;
	void __user *argp = (void __user *)arg;

	switch (ioctl) {
	case KVM_RUN:
	case GZVM_RUN:
		return gzvm_vcpu_run(filp, argp);
	case KVM_GET_REGS:
	case GZVM_GET_REGS: {
		ret = 0;
		break;
	}
	case KVM_SET_REGS:
	case GZVM_SET_REGS: {
		return gzvm_vcpu_update_regs(filp, ioctl, argp);
	}
	case KVM_GET_ONE_REG:
	case GZVM_GET_ONE_REG: {
		GZVM_DEBUG("%s %d TODO:KVM_GET_ONE_REG\n", __func__, __LINE__);
		ret = 0;
		break;
	}
	case KVM_SET_ONE_REG:
	case GZVM_SET_ONE_REG: {
		struct kvm_one_reg reg;

		ret = -EFAULT;
		if (copy_from_user(&reg, argp, sizeof(reg)))
			break;
		return gzvm_vcpu_access_one_reg(filp, ioctl, &reg);
	}
	case KVM_ARM_VCPU_INIT: {
		/* TODO: remove this */
		GZVM_DEBUG("%s %d KVM_ARM_VCPU_INIT\n", __func__, __LINE__);
		ret = 0;
		break;
	}
	default:
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

static int gzvm_destroy_vcpu_smc(gzvm_id_t vm_id, int vcpuid)
{
	struct arm_smccc_res res;
	unsigned long a1;

	a1 = assembel_vm_vcpu_tuple(vm_id, vcpuid);
	gzvm_hypcall_wrapper(MT_SMC_FC_GZVM_DESTROY_VCPU, a1, 0, 0, 0, 0, 0, 0,
			     &res);

	GZVM_DEBUG("%s a0=%lx, a1=%lx, a2=%lx, a3=%lx\n", __func__,
		res.a0, res.a1, res.a2, res.a3);
	return 0;
}

/**
 * @brief call smc to gz hypervisor to create vcpu
 *
 * @param run virtual address of vcpu->run
 * @return int
 */
static int gzvm_create_vcpu_smc(gzvm_id_t vm_id, int vcpuid, void *run)
{
	struct arm_smccc_res res;
	unsigned long a1, a2;

	a1 = assembel_vm_vcpu_tuple(vm_id, vcpuid);
	a2 = (__u64)virt_to_phys(run);
	gzvm_hypcall_wrapper(MT_SMC_FC_GZVM_CREATE_VCPU, a1, a2, 0, 0, 0, 0, 0,
			 &res);

	GZVM_DEBUG("%s a0=%lx, a1=%lx, a2=%lx, a3=%lx\n", __func__,
		res.a0, res.a1, res.a2, res.a3);
	return 0;
}

void gzvm_destroy_vcpu(struct gzvm_vcpu *vcpu)
{
	if (!vcpu)
		return;

	GZVM_DEBUG("%s %d\n", __func__, __LINE__);
	gzvm_destroy_vcpu_smc(vcpu->gzvm->vm_id, vcpu->vcpuid);
	free_pages_exact(vcpu->run, PAGE_SIZE * 2);
	kfree(vcpu);
}

/**
 * @brief Allocates an inode for the vcpu.
 */
static int create_vcpu_fd(struct gzvm_vcpu *vcpu)
{
	char name[8 + 1 + ITOA_MAX_LEN + 1];

	snprintf(name, sizeof(name), "gzvm-vcpu:%d", vcpu->vcpuid);
	return anon_inode_getfd(name, &gzvm_vcpu_fops, vcpu, O_RDWR | O_CLOEXEC);
}

/**
 * @brief KVM_CREATE_VCPU
 *
 * @param cpuid = arg
 * @return fd of vcpu, negative errno if error occurs
 */
int gzvm_vm_ioctl_create_vcpu(struct gzvm *gzvm, u32 cpuid)
{
	struct gzvm_vcpu *vcpu;
	int ret;

	if (cpuid >= KVM_MAX_VCPUS)
		return -EINVAL;

	vcpu = kzalloc(sizeof(*vcpu), GFP_KERNEL);
	if (IS_ERR(gzvm)) {
		ret = PTR_ERR(gzvm);
		goto error;
	}

	BUILD_BUG_ON(sizeof(struct gzvm_vcpu_run) > PAGE_SIZE);
	BUILD_BUG_ON(sizeof(struct gzvm_vcpu_hwstate) > PAGE_SIZE);
	vcpu->run = alloc_pages_exact(PAGE_SIZE * 2,
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

	ret = gzvm_create_vcpu_smc(gzvm->vm_id, vcpu->vcpuid, vcpu->run);
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
error:
	return ret;
}
