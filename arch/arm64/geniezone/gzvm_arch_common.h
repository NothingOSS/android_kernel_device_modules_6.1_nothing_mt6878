/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2023 MediaTek Inc.
 */

#ifndef __GZVM_ARCH_COMMON_H__
#define __GZVM_ARCH_COMMON_H__

#include <linux/arm-smccc.h>

enum {
	GZVM_FUNC_CREATE_VM = 0,
	GZVM_FUNC_DESTROY_VM,
	GZVM_FUNC_CREATE_VCPU,
	GZVM_FUNC_DESTROY_VCPU,
	GZVM_FUNC_SET_MEMREGION,
	GZVM_FUNC_RUN,
	GZVM_FUNC_GET_REGS,
	GZVM_FUNC_SET_REGS,
	GZVM_FUNC_GET_ONE_REG,
	GZVM_FUNC_SET_ONE_REG,
	GZVM_FUNC_IRQ_LINE,
	GZVM_FUNC_CREATE_DEVICE,
	GZVM_FUNC_PROBE,
	GZVM_FUNC_ENABLE_CAP,
	GZVM_FUNC_INFORM_EXIT,
	NR_GZVM_FUNC
};

#define SMC_ENTITY_MTK			59
#define GZVM_FUNCID_START		(0x1000)
#define GZVM_HCALL_ID(func)						\
	ARM_SMCCC_CALL_VAL(ARM_SMCCC_FAST_CALL, ARM_SMCCC_SMC_32,	\
			   SMC_ENTITY_MTK, (GZVM_FUNCID_START + (func)))

#define MT_HVC_GZVM_CREATE_VM		GZVM_HCALL_ID(GZVM_FUNC_CREATE_VM)
#define MT_HVC_GZVM_DESTROY_VM		GZVM_HCALL_ID(GZVM_FUNC_DESTROY_VM)
#define MT_HVC_GZVM_CREATE_VCPU		GZVM_HCALL_ID(GZVM_FUNC_CREATE_VCPU)
#define MT_HVC_GZVM_DESTROY_VCPU	GZVM_HCALL_ID(GZVM_FUNC_DESTROY_VCPU)
#define MT_HVC_GZVM_SET_MEMREGION	GZVM_HCALL_ID(GZVM_FUNC_SET_MEMREGION)
#define MT_HVC_GZVM_RUN			GZVM_HCALL_ID(GZVM_FUNC_RUN)
#define MT_HVC_GZVM_GET_REGS		GZVM_HCALL_ID(GZVM_FUNC_GET_REGS)
#define MT_HVC_GZVM_SET_REGS		GZVM_HCALL_ID(GZVM_FUNC_SET_REGS)
#define MT_HVC_GZVM_GET_ONE_REG		GZVM_HCALL_ID(GZVM_FUNC_GET_ONE_REG)
#define MT_HVC_GZVM_SET_ONE_REG		GZVM_HCALL_ID(GZVM_FUNC_SET_ONE_REG)
#define MT_HVC_GZVM_IRQ_LINE		GZVM_HCALL_ID(GZVM_FUNC_IRQ_LINE)
#define MT_HVC_GZVM_CREATE_DEVICE	GZVM_HCALL_ID(GZVM_FUNC_CREATE_DEVICE)
#define MT_HVC_GZVM_PROBE		GZVM_HCALL_ID(GZVM_FUNC_PROBE)
#define MT_HVC_GZVM_ENABLE_CAP		GZVM_HCALL_ID(GZVM_FUNC_ENABLE_CAP)
#define MT_HVC_GZVM_INFORM_EXIT		GZVM_HCALL_ID(GZVM_FUNC_INFORM_EXIT)
#define GIC_V3_NR_LRS			16

/**
 * gzvm_hypercall_wrapper()
 *
 * Return: The wrapper helps caller to convert geniezone errno to Linux errno.
 */
static int gzvm_hypcall_wrapper(unsigned long a0, unsigned long a1,
				unsigned long a2, unsigned long a3,
				unsigned long a4, unsigned long a5,
				unsigned long a6, unsigned long a7,
				struct arm_smccc_res *res)
{
	arm_smccc_hvc(a0, a1, a2, a3, a4, a5, a6, a7, res);
	return gz_err_to_errno(res->a0);
}

static inline gzvm_id_t get_vmid_from_tuple(unsigned int tuple)
{
	return (gzvm_id_t)(tuple >> 16);
}

static inline gzvm_vcpu_id_t get_vcpuid_from_tuple(unsigned int tuple)
{
	return (gzvm_vcpu_id_t)(tuple & 0xffff);
}

struct gzvm_vcpu_hwstate {
	__u32 nr_lrs;
	__u64 lr[GIC_V3_NR_LRS];
	__u64 vtimer_delay;
	__u32 vtimer_migrate;
};

static inline unsigned int
assemble_vm_vcpu_tuple(gzvm_id_t vmid, gzvm_vcpu_id_t vcpuid)
{
	return ((unsigned int)vmid << 16 | vcpuid);
}

static inline void
disassemble_vm_vcpu_tuple(unsigned int tuple, gzvm_id_t *vmid,
			  gzvm_vcpu_id_t *vcpuid)
{
	*vmid = get_vmid_from_tuple(tuple);
	*vcpuid = get_vcpuid_from_tuple(tuple);
}

#endif /* __GZVM_ARCH_COMMON_H__ */
