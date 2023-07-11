// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2023 MediaTek Inc.
 */

#include <linux/device.h>
#include <linux/gzvm_drv.h>

/**
 * gzvm_handle_page_fault() - Handle guest page fault, find corresponding page
 * for the faulting gpa
 * @vcpu: Pointer to struct gzvm_vcpu_run in userspace
 */
static int gzvm_handle_page_fault(struct gzvm_vcpu *vcpu)
{
	struct gzvm *vm = vcpu->gzvm;
	int memslot_id;
	u64 pfn, gfn;
	int ret;

	gfn = PHYS_PFN(vcpu->run->exception.fault_gpa);
	memslot_id = gzvm_find_memslot(vm, gfn);
	if (unlikely(memslot_id < 0))
		return -EFAULT;

	ret = gzvm_gfn_to_pfn_memslot(&vm->memslot[memslot_id], gfn, &pfn);
	if (unlikely(ret))
		return -EFAULT;

	ret = gzvm_arch_map_guest(vm->vm_id, memslot_id, pfn, gfn, 1);
	if (unlikely(ret))
		return -EFAULT;

	return 0;
}

/**
 * gzvm_handle_guest_exception() - Handle guest exception
 * @vcpu: Pointer to struct gzvm_vcpu_run in userspace
 * Return:
 * * true - This exception has been processed, no need to back to VMM.
 * * false - This exception has not been processed, require userspace.
 */
bool gzvm_handle_guest_exception(struct gzvm_vcpu *vcpu)
{
	int ret;

	switch (vcpu->run->exception.exception) {
	case GZVM_EXCEPTION_PAGE_FAULT:
		ret = gzvm_handle_page_fault(vcpu);
		break;
	case GZVM_EXCEPTION_UNKNOWN:
		fallthrough;
	default:
		ret = -EFAULT;
	}

	if (!ret)
		return true;
	else
		return false;
}

