/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2022 MediaTek Inc.
 */

#ifndef __GZVM_H__
#define __GZVM_H__

#include <linux/kvm_host.h>

#include "gzvm_common.h"

/* Worst case buffer size needed for holding an integer. */
#define ITOA_MAX_LEN 12

#define gzvm_VCPU_MMAP_SIZE  PAGE_SIZE

#define INVALID_VM_ID   0xffff

struct gzvm_vcpu;

#define gfn_to_gpa(x)		(x << PAGE_SHIFT)
#define gfn_to_phys(x)		(gfn_to_gpa(x))
#define kvm_pfn_to_phys(x)	(gfn_to_phys(x))
#define gpa_to_gfn(x)		(x >> PAGE_SHIFT)

struct gzvm_memslot {
	struct kvm_memory_slot slot;
	struct vm_area_struct *vma;
};

struct gzvm {
	struct gzvm_vcpu *vcpus[KVM_MAX_VCPUS];
	struct mm_struct *mm; /* userspace tied to this vm */
	struct gzvm_memslot memslot[GZVM_MAX_MEM_REGION];
	struct mutex lock;
	struct list_head vm_list;
	struct list_head devices;
	/* FIXME: dram pointer in kernel va */
	void *mem_ptr;
	gzvm_id_t vm_id;

	struct list_head ioevents;
};

struct gzvm_vcpu {
	struct gzvm *gzvm;
	int vcpuid;
	struct mutex lock;
	struct gzvm_vcpu_run *run;
	struct cpu_user_regs *vm_regs;
};

struct gzvm_device {
	struct kvm_device kdev;
	struct gzvm *gzvm;
};

void gzvm_destroy_vcpu(struct gzvm_vcpu *vcpu);
int gzvm_vm_ioctl_create_vcpu(struct gzvm *gzvm, u32 cpuid);
int gzvm_dev_ioctl_creat_vm(unsigned long vm_type);

int gzvm_arm_get_reg(struct gzvm_vcpu *vcpu, const struct kvm_one_reg *reg);
int gzvm_arm_set_reg(struct gzvm_vcpu *vcpu, const struct kvm_one_reg *reg);

void gzvm_hypcall_wrapper(unsigned long a0, unsigned long a1,
			  unsigned long a2, unsigned long a3, unsigned long a4,
			  unsigned long a5, unsigned long a6, unsigned long a7,
			  struct arm_smccc_res *res);

#define SMC_ENTITY_MTK  59
#define GZVM_FCALL_SMC_FUNCID_START	(0x1000)
#define GZVM_FCALL_SMC(func)				\
	ARM_SMCCC_CALL_VAL(ARM_SMCCC_FAST_CALL, ARM_SMCCC_SMC_32,	\
			   SMC_ENTITY_MTK, (GZVM_FCALL_SMC_FUNCID_START + func))

#define MT_SMC_FC_GZVM_CREATE_VM		GZVM_FCALL_SMC(GZVM_SMC_FUNC_CREATE_VM)
#define MT_SMC_FC_GZVM_DESTROY_VM		GZVM_FCALL_SMC(GZVM_SMC_FUNC_DESTROY_VM)
#define MT_SMC_FC_GZVM_CREATE_VCPU		GZVM_FCALL_SMC(GZVM_SMC_FUNC_CREATE_VCPU)
#define MT_SMC_FC_GZVM_DESTROY_VCPU		GZVM_FCALL_SMC(GZVM_SMC_FUNC_DESTROY_VCPU)
#define MT_SMC_FC_GZVM_SET_MEMREGION		GZVM_FCALL_SMC(GZVM_SMC_FUNC_SET_MEMREGION)
#define MT_SMC_FC_GZVM_RUN			GZVM_FCALL_SMC(GZVM_SMC_FUNC_RUN)
#define MT_SMC_FC_GZVM_GET_REGS			GZVM_FCALL_SMC(GZVM_SMC_FUNC_GET_REGS)
#define MT_SMC_FC_GZVM_SET_REGS			GZVM_FCALL_SMC(GZVM_SMC_FUNC_SET_REGS)
#define MT_SMC_FC_GZVM_GET_ONE_REG		GZVM_FCALL_SMC(GZVM_SMC_FUNC_GET_ONE_REG)
#define MT_SMC_FC_GZVM_SET_ONE_REG		GZVM_FCALL_SMC(GZVM_SMC_FUNC_SET_ONE_REG)
#define MT_SMC_FC_GZVM_IRQ_LINE			GZVM_FCALL_SMC(GZVM_SMC_FUNC_IRQ_LINE)
#define MT_SMC_FC_GZVM_CREATE_DEVICE		GZVM_FCALL_SMC(GZVM_SMC_FUNC_CREATE_DEVICE)

#define READ_SYSREG64(name) ({                          \
	uint64_t _r;                                        \
	asm volatile("mrs  %0, "__stringify(name) : "=r" (_r));         \
	_r; })

int gzvm_ioeventfd(struct gzvm *gzvm, struct kvm_ioeventfd *args);
bool gzvm_ioevent_write(struct gzvm_vcpu *vcpu, __u64 addr, int len,
			const void *val);
#endif /* __GZVM_H__ */
