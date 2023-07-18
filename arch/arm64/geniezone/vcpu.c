// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2023 MediaTek Inc.
 */

#include <linux/arm-smccc.h>
#include <linux/clocksource.h>
#include <linux/err.h>
#include <linux/uaccess.h>

#include <linux/gzvm.h>
#include <linux/gzvm_drv.h>
#include "gzvm_arch_common.h"

int gzvm_arch_vcpu_update_one_reg(struct gzvm_vcpu *vcpu, __u64 reg_id,
				  bool is_write, __u64 *data)
{
	struct arm_smccc_res res;
	unsigned long a1;
	int ret;

	/* reg id follows KVM's encoding */
	switch (reg_id & GZVM_REG_ARM_COPROC_MASK) {
	case GZVM_REG_ARM_CORE:
		break;
	default:
		return -EOPNOTSUPP;
	}

	a1 = assemble_vm_vcpu_tuple(vcpu->gzvm->vm_id, vcpu->vcpuid);
	if (!is_write) {
		ret = gzvm_hypcall_wrapper(MT_HVC_GZVM_GET_ONE_REG,
					   a1, reg_id, 0, 0, 0, 0, 0, &res);
		if (ret == 0)
			*data = res.a1;
	} else {
		ret = gzvm_hypcall_wrapper(MT_HVC_GZVM_SET_ONE_REG,
					   a1, reg_id, *data, 0, 0, 0, 0, &res);
	}

	return ret;
}

static void clear_migrate_state(struct gzvm_vcpu *vcpu)
{
	vcpu->hwstate->vtimer_migrate = 0;
	vcpu->hwstate->vtimer_delay = 0;
}

static u64 gzvm_mtimer_delay_time(u64 delay)
{
	u64 ns;

	ns = clocksource_cyc2ns(delay, clock_scale_factor.mult,
				clock_scale_factor.shift);

	return ns;
}

static void gzvm_mtimer_release(struct gzvm_vcpu *vcpu)
{
	hrtimer_cancel(&vcpu->gzvm_mtimer);

	clear_migrate_state(vcpu);
}

static void gzvm_mtimer_catch(struct hrtimer *hrt, u64 delay)
{
	u64 ns;

	ns = gzvm_mtimer_delay_time(delay);
	hrtimer_start(hrt, ktime_add_ns(ktime_get(), ns), HRTIMER_MODE_ABS_HARD);
}

static void mtimer_irq_forward(struct gzvm_vcpu *vcpu)
{
	struct gzvm *gzvm;
	u32 irq_num, vcpu_idx, vcpu2_idx;

	gzvm = vcpu->gzvm;

	irq_num = FIELD_GET(GZVM_IRQ_LINE_NUM, GZVM_VTIMER_IRQ);
	vcpu_idx = FIELD_GET(GZVM_IRQ_LINE_VCPU, GZVM_VTIMER_IRQ);
	vcpu2_idx = FIELD_GET(GZVM_IRQ_LINE_VCPU2, GZVM_VTIMER_IRQ) *
		    (GZVM_IRQ_VCPU_MASK + 1);

	gzvm_vgic_inject_ppi(gzvm, vcpu_idx + vcpu2_idx, irq_num, 1);
}

static enum hrtimer_restart gzvm_mtimer_expire(struct hrtimer *hrt)
{
	struct gzvm_vcpu *vcpu;

	vcpu = container_of(hrt, struct gzvm_vcpu, gzvm_mtimer);

	mtimer_irq_forward(vcpu);

	return HRTIMER_NORESTART;
}

static void vtimer_init(struct gzvm_vcpu *vcpu)
{
	/* gzvm_mtimer init based on hrtimer */
	hrtimer_init(&vcpu->gzvm_mtimer, CLOCK_MONOTONIC, HRTIMER_MODE_ABS_HARD);
	vcpu->gzvm_mtimer.function = gzvm_mtimer_expire;
}

int gzvm_arch_vcpu_run(struct gzvm_vcpu *vcpu, __u64 *exit_reason)
{
	struct arm_smccc_res res;
	unsigned long a1;
	int ret;

	/* hrtimer cancel and clear migrate state */
	if (vcpu->hwstate->vtimer_migrate)
		gzvm_mtimer_release(vcpu);

	a1 = assemble_vm_vcpu_tuple(vcpu->gzvm->vm_id, vcpu->vcpuid);
	ret = gzvm_hypcall_wrapper(MT_HVC_GZVM_RUN, a1, 0, 0, 0, 0, 0,
				   0, &res);

	/* hrtimer register if migration needed */
	if (vcpu->hwstate->vtimer_migrate)
		gzvm_mtimer_catch(&vcpu->gzvm_mtimer, vcpu->hwstate->vtimer_delay);

	*exit_reason = res.a1;
	return ret;
}

int gzvm_arch_destroy_vcpu(struct gzvm_vcpu *vcpu)
{
	struct arm_smccc_res res;
	unsigned long a1;

	hrtimer_cancel(&vcpu->gzvm_mtimer);

	a1 = assemble_vm_vcpu_tuple(vcpu->gzvm->vm_id, vcpu->vcpuid);
	gzvm_hypcall_wrapper(MT_HVC_GZVM_DESTROY_VCPU, a1, 0, 0, 0, 0, 0, 0,
			     &res);

	return 0;
}

/**
 * gzvm_arch_create_vcpu() - Call smc to gz hypervisor to create vcpu
 * @vcpu: Pointer to struct gzvm_vcpu
 *
 * Return: The wrapper helps caller to convert geniezone errno to Linux errno.
 */
int gzvm_arch_create_vcpu(struct gzvm_vcpu *vcpu)
{
	struct arm_smccc_res res;
	unsigned long a1, a2;
	int ret;

	vtimer_init(vcpu);

	a1 = assemble_vm_vcpu_tuple(vcpu->gzvm->vm_id, vcpu->vcpuid);
	a2 = (__u64)virt_to_phys(vcpu->run);
	ret = gzvm_hypcall_wrapper(MT_HVC_GZVM_CREATE_VCPU, a1, a2, 0, 0, 0, 0,
				   0, &res);

	return ret;
}
