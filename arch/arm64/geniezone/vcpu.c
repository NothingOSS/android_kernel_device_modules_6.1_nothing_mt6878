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

	ns = clocksource_cyc2ns(
		delay,
		timecycle.mult,
		timecycle.shift
	);

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
	hrtimer_start(hrt, ktime_add_ns(ktime_get(), ns),
				HRTIMER_MODE_ABS_HARD);
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

int gzvm_arch_destroy_vcpu(gzvm_id_t vm_id, int vcpuid)
{
	struct arm_smccc_res res;
	unsigned long a1;

	a1 = assemble_vm_vcpu_tuple(vm_id, vcpuid);
	gzvm_hypcall_wrapper(MT_HVC_GZVM_DESTROY_VCPU, a1, 0, 0, 0, 0, 0, 0,
			     &res);

	return 0;
}

/**
 * gzvm_arch_create_vcpu() - Call smc to gz hypervisor to create vcpu
 * @run: Virtual address of vcpu->run
 */
int gzvm_arch_create_vcpu(gzvm_id_t vm_id, int vcpuid, void *run)
{
	struct arm_smccc_res res;
	unsigned long a1, a2;
	int ret;

	a1 = assemble_vm_vcpu_tuple(vm_id, vcpuid);
	a2 = (__u64)virt_to_phys(run);
	ret = gzvm_hypcall_wrapper(MT_HVC_GZVM_CREATE_VCPU, a1, a2, 0, 0, 0, 0,
				   0, &res);

	return ret;
}
