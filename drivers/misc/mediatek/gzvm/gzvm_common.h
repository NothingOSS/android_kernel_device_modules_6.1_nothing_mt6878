/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2022 MediaTek Inc.
 */

#ifndef __GZVM_COMMON_H__
#define __GZVM_COMMON_H__

/**
 * @file gzvm_common.h
 * @brief This file declares common data structure shared between userspace,
 *        kernel space, and GZ.
 */

enum {
	GZVM_SMC_FUNC_CREATE_VM = 0,
	GZVM_SMC_FUNC_DESTROY_VM,
	GZVM_SMC_FUNC_CREATE_VCPU,
	GZVM_SMC_FUNC_DESTROY_VCPU,
	GZVM_SMC_FUNC_SET_MEMREGION,
	GZVM_SMC_FUNC_RUN,
	GZVM_SMC_FUNC_GET_REGS,
	GZVM_SMC_FUNC_SET_REGS,
	GZVM_SMC_FUNC_GET_ONE_REG,
	GZVM_SMC_FUNC_SET_ONE_REG,
	GZVM_SMC_FUNC_IRQ_LINE,
	GZVM_SMC_FUNC_CREATE_DEVICE,
	NR_GZVM_SMC
};

typedef uint16_t gzvm_id_t;
typedef uint16_t gzvm_vcpu_id_t;

#ifdef WITH_EL2
typedef u32 __u32;
typedef u64 __u64;
typedef u8 __u8;

struct kvm_debug_exit_arch {
	__u32 hsr;
	__u64 far;	/* used for watchpoints */
};

#endif

#ifndef WITH_EL2

#ifndef __KERNEL__

#include <stdbool.h>

typedef __u32 u32;
typedef __u64 u64;
typedef __u8 u8;
#endif

#define __DECL_REG(n64, n32) union {		\
	uint64_t n64;				\
	uint32_t n32;				\
}

struct vmm_info {
	uint32_t id;
};

struct vcontext_guard {
	uint64_t sp_el2;
	uint64_t tail[2];
};

struct fpstate {
	uint32_t fpcr;
	uint32_t fpsr;
};

struct fpreg {
	uint64_t regs[64];
};

/* On stack VCPU state */
struct cpu_user_regs {
	/*	 Aarch64	   Aarch32 */
	__DECL_REG(x0,		r0/*_usr*/);
	__DECL_REG(x1,		r1/*_usr*/);
	__DECL_REG(x2,		r2/*_usr*/);
	__DECL_REG(x3,		r3/*_usr*/);
	__DECL_REG(x4,		r4/*_usr*/);
	__DECL_REG(x5,		r5/*_usr*/);
	__DECL_REG(x6,		r6/*_usr*/);
	__DECL_REG(x7,		r7/*_usr*/);
	__DECL_REG(x8,		r8/*_usr*/);
	__DECL_REG(x9,		r9/*_usr*/);
	__DECL_REG(x10,		r10/*_usr*/);
	__DECL_REG(x11,		r11/*_usr*/);
	__DECL_REG(x12,		r12/*_usr*/);

	__DECL_REG(x13,	  /* r13_usr */ sp_usr);
	__DECL_REG(x14,	  /* r14_usr */ lr_usr);

	__DECL_REG(x15,	  /* r13_hyp */ __unused_sp_hyp);

	__DECL_REG(x16,	  /* r14_irq */ lr_irq);
	__DECL_REG(x17,	  /* r13_irq */ sp_irq);

	__DECL_REG(x18,	  /* r14_svc */ lr_svc);
	__DECL_REG(x19,	  /* r13_svc */ sp_svc);

	__DECL_REG(x20,	  /* r14_abt */ lr_abt);
	__DECL_REG(x21,	  /* r13_abt */ sp_abt);

	__DECL_REG(x22,	  /* r14_und */ lr_und);
	__DECL_REG(x23,	  /* r13_und */ sp_und);

	__DECL_REG(x24,		r8_fiq);
	__DECL_REG(x25,		r9_fiq);
	__DECL_REG(x26,		r10_fiq);
	__DECL_REG(x27,		r11_fiq);
	__DECL_REG(x28,		r12_fiq);
	__DECL_REG(/* x29 */ fp, /* r13_fiq */ sp_fiq);

	__DECL_REG(/* x30 */ lr, /* r14_fiq */ lr_fiq);

	uint64_t sp; /* Valid for hypervisor frames */

	/* Return address and mode */
	__DECL_REG(pc,		pc32);		 /* ELR_EL2 */
	uint32_t cpsr;				  /* SPSR_EL2 */

	uint32_t pad0; /* Align end of kernel frame. */

	/* Outer guest frame only from here on... */

	union {
	uint32_t spsr_el1;	   /* AArch64 */
	uint32_t spsr_svc;	   /* AArch32 */
	};

	uint32_t pad1; /* Doubleword-align the user half of the frame */

	/* AArch32 guests only */
	uint32_t spsr_fiq, spsr_irq, spsr_und, spsr_abt;

	/* AArch64 guests only */
	uint64_t sp_el0;
	uint64_t sp_el1, elr_el1;

	/* VFP register */
	struct fpstate fp_state;
	struct fpreg fp_regs;

	/* For record vmm info */
	struct vmm_info vmm;

	/* For context corruption check */
	struct vcontext_guard context_guard;
};
#endif /* WITH_EL2 */

_Static_assert(sizeof(struct cpu_user_regs) == 872,
	       "sizeof(struct cpu_user_regs) must be 872 bytes, change must be synced with gz");

/* VM exit reason */
enum {
	GZVM_EXIT_UNKNOWN = 0x92920000,
	GZVM_EXIT_MMIO,
	GZVM_EXIT_HVC,
	GZVM_EXIT_IRQ,
};

/**
 * @brief same purpose as kvm_run, this struct is shared between userspace,
 *	kernel and GZ
 * Note: keep identical layout between the 3 modules
 * TODO: use a compiler assert to make sure the size is kept the same.
 */
struct gzvm_vcpu_run {
	/* to userspace */
	__u32 exit_reason;
	/* union structure of collection of guest exit reason */
	union {
		/* GZVM_EXIT_MMIO */
		struct {
			uint64_t phys_addr;		/* from FAR_EL2 */
			union {		/* from registers[reg_nr], little-endian */
				uint8_t c;
				uint16_t s;
				uint32_t w;
				uint64_t dw;
			} data;
			uint64_t size;			/* from ESR_EL2 as */
			int reg_nr;				/* from ESR_EL2 */
			bool is_write;			/* from EST_EL2 */
		} mmio;
	};
};
_Static_assert(sizeof(struct gzvm_vcpu_run) == 40,
	       "sizeof(struct gzvm_vcpu_run) must be 40 bytes, change must be also synced with gz");

#define GZVM_MAX_MEM_REGION 1

/* This is used for parameter between linux and gz */

/* identical to ffa memory constituent */
struct mem_region_addr_range {
	/* The base IPA of the constituent memory region, aligned to 4 kiB */
	u64 address;
	/* The number of 4 kiB pages in the constituent memory region. */
	u32 pg_cnt;
	u32 reserved;
};

struct gzvm_memory_region {
	u32 slot;
	u32 constituent_cnt;
	u64 total_pages;
	u64 gpa;
	struct mem_region_addr_range constituents[];
};

static inline unsigned int
assembel_vm_vcpu_tuple(gzvm_id_t vmid, gzvm_vcpu_id_t vcpuid)
{
	return ((unsigned int)vmid << 16 | vcpuid);
}

static inline gzvm_id_t get_vmid_from_tuple(unsigned int tuple)
{
	return (gzvm_id_t)(tuple >> 16);
}

static inline gzvm_vcpu_id_t get_vcpuid_from_tuple(unsigned int tuple)
{
	return (gzvm_vcpu_id_t) (tuple & 0xffff);
}

static inline void
disassemble_vm_vcpu_tuple(unsigned int tuple, gzvm_id_t *vmid,
			  gzvm_vcpu_id_t *vcpuid)
{
	*vmid = get_vmid_from_tuple(tuple);
	*vcpuid = get_vcpuid_from_tuple(tuple);
}

struct gzvm_create_device {
	__u32 type;			/* device type */
	__u32 id;			/* out: device id */
	__u64 flags;			/* device specific flags */
	__u64 dev_addr;			/* device ipa address in VM's view */
	__u64 dev_reg_size;		/* device register range size */
	/* If user -> kernel, this is user virtual address of device specific
	 * attributes (if needed). If kernel->hypervisor, this is ipa.
	 */
	__u64 attr_addr;
	__u64 attr_size;		/* size of device specific attributes */
};

enum gzvm_device_type {
	GZVM_DEV_TYPE_ARM_VGIC_V3_DIST,
	GZVM_DEV_TYPE_ARM_VGIC_V3_REDIST,
	GZVM_DEV_TYPE_MAX,
};

#endif /* __GZVM_COMMON_H__ */
