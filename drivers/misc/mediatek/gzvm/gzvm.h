/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2022 MediaTek Inc.
 */

#ifndef __GZVM_H__
#define __GZVM_H__

#include <linux/kvm_host.h>
#include <linux/srcu.h>

#include "gzvm_common.h"

#define MODULE_NAME	"gzvm"

#define GZVM_DRIVER_VERSION	0

/* Worst case buffer size needed for holding an integer. */
#define ITOA_MAX_LEN 12

#define GZVM_VCPU_MMAP_SIZE  PAGE_SIZE

#define INVALID_VM_ID   0xffff

struct gzvm_vcpu;

#define gfn_to_gpa(x)		(x << PAGE_SHIFT)
#define gfn_to_phys(x)		(gfn_to_gpa(x))
#define kvm_pfn_to_phys(x)	(gfn_to_phys(x))
#define gpa_to_gfn(x)		(x >> PAGE_SHIFT)

struct gzvm_memslot {
	/*
	 * this is needed in order to call kvm's gfn_to_pfn_memslot() to get pfn
	 */
	struct kvm_memory_slot slot;
	struct vm_area_struct *vma;
};

#define GZVM_MAX_VCPUS			NR_CPUS

struct gzvm {
	struct gzvm_vcpu *vcpus[GZVM_MAX_VCPUS];
	struct mm_struct *mm; /* userspace tied to this vm */
	struct gzvm_memslot memslot[GZVM_MAX_MEM_REGION];
	struct mutex lock;
	struct list_head vm_list;
	struct list_head devices;
	gzvm_id_t vm_id;

	struct list_head ioevents;
	struct {
		spinlock_t        lock;
		struct list_head  items;
		struct list_head  resampler_list;
		struct mutex      resampler_lock;
	} irqfds;
	struct hlist_head irq_ack_notifier_list;
	struct srcu_struct irq_srcu;
	struct mutex irq_lock;
};

#define GZVM_VCPU_RUN_MAP_SIZE		(PAGE_SIZE * 2)

struct gzvm_vcpu {
	struct gzvm *gzvm;
	int vcpuid;
	struct mutex lock;
	struct gzvm_vcpu_run *run;
	struct gzvm_cpu_user_regs *vm_regs;
	struct gzvm_vcpu_hwstate *hwstate;
};

long gzvm_dev_ioctl_check_extension(struct gzvm *gzvm, unsigned long args);

void gzvm_destroy_vcpu(struct gzvm_vcpu *vcpu);
int gzvm_vm_ioctl_create_vcpu(struct gzvm *gzvm, u32 cpuid);
int gzvm_dev_ioctl_create_vm(unsigned long vm_type);

int gzvm_arm_get_reg(struct gzvm_vcpu *vcpu, const struct gzvm_one_reg *reg);
int gzvm_arm_set_reg(struct gzvm_vcpu *vcpu, const struct gzvm_one_reg *reg);

void gzvm_hypcall_wrapper(unsigned long a0, unsigned long a1,
			  unsigned long a2, unsigned long a3, unsigned long a4,
			  unsigned long a5, unsigned long a6, unsigned long a7,
			  struct arm_smccc_res *res);

#define SMC_ENTITY_MTK			59
#define GZVM_FUNCID_START		(0x1000)
#define GZVM_HCALL_ID(func)				\
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

#define READ_SYSREG64(name) ({                          \
	uint64_t _r;                                        \
	asm volatile("mrs  %0, "__stringify(name) : "=r" (_r));         \
	_r; })

void gzvm_notify_acked_irq(struct gzvm *gzvm, unsigned int gsi);
int gzvm_init_eventfd(struct gzvm *gzvm);
int gzvm_ioeventfd(struct gzvm *gzvm, struct gzvm_ioeventfd *args);
bool gzvm_ioevent_write(struct gzvm_vcpu *vcpu, __u64 addr, int len,
			const void *val);

#define GZVM_USERSPACE_IRQ_SOURCE_ID		0
#define GZVM_IRQFD_RESAMPLE_IRQ_SOURCE_ID	1

int gzvm_irqfd(struct gzvm *gzvm, struct gzvm_irqfd *args);
int gzvm_irqfd_init(void);
void gzvm_irqfd_exit(void);

int gzvm_vgic_inject_irq(struct gzvm *gzvm, unsigned int vcpu_idx, u32 irq_type,
			 u32 irq, bool level);


#define GZVM_INFO(fmt...) pr_info("[GZVM]" fmt)
#define GZVM_DEBUG(fmt...) pr_info("[GZVM][DBG]" fmt)
#define GZVM_ERR(fmt...) pr_info("[GZVM][ERR]" fmt)

#endif /* __GZVM_H__ */

