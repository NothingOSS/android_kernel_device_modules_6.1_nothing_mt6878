// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2023 MediaTek Inc.
 */

#include <linux/gzvm_drv.h>

/**
 * gzvm_handle_guest_hvc() - Handle guest hvc
 * @vcpu: Pointer to struct gzvm_vcpu_run in userspace
 * Return:
 * * true - This hvc has been processed, no need to back to VMM.
 * * false - This hvc has not been processed, require userspace.
 */
bool gzvm_handle_guest_hvc(struct gzvm_vcpu *vcpu)
{
	int ret;
	unsigned long ipa;

	switch(vcpu->run->hypercall.args[0]) {
	case GZVM_HVC_MEM_RELINQUISH:
		ipa = vcpu->run->hypercall.args[1];
		ret = gzvm_handle_relinquish(vcpu, ipa);
		break;
	default:
		ret = false;
		break;
	}

	if (!ret)
		return true;
	else
		return false;
}
