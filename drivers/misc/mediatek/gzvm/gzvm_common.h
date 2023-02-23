/* SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note */
/*
 * Copyright (c) 2022 MediaTek Inc.
 */

#ifndef __GZVM_COMMON_H__
#define __GZVM_COMMON_H__

#include <linux/const.h>
#include <linux/types.h>
#include <linux/compiler.h>
#include <linux/ioctl.h>
#include <asm/ptrace.h>

/**
 * @file gzvm_common.h
 * @brief This file declares common data structure shared between userspace,
 *        kernel space, and GZ.
 */

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
	NR_GZVM_FUNC
};

typedef __u16 gzvm_id_t;
typedef __u16 gzvm_vcpu_id_t;

/* On stack VCPU state */
struct gzvm_cpu_user_regs {
	__u64 x0;
	__u64 x1;
	__u64 x2;
	__u64 x3;
	__u64 x4;
	__u64 x5;
	__u64 x6;
	__u64 x7;
	__u64 x8;
	__u64 x9;
	__u64 x10;
	__u64 x11;
	__u64 x12;
	__u64 x13;
	__u64 x14;
	__u64 x15;
	__u64 x16;
	__u64 x17;
	__u64 x18;
	__u64 x19;
	__u64 x20;
	__u64 x21;
	__u64 x22;
	__u64 x23;
	__u64 x24;
	__u64 x25;
	__u64 x26;
	__u64 x27;
	__u64 x28;
	__u64 fp;	/* 29 */
	__u64 lr;	/* 30 */
	__u64 sp;	/* 31 */
	__u64 pc;	/* 32 */
	__u64 cpsr;
};

/* VM exit reason */
enum {
	GZVM_EXIT_UNKNOWN = 0x92920000,
	GZVM_EXIT_MMIO,
	GZVM_EXIT_HVC,
	GZVM_EXIT_IRQ,
	GZVM_EXIT_EXCEPTION,
	GZVM_EXIT_DEBUG,
	GZVM_EXIT_FAIL_ENTRY,
	GZVM_EXIT_INTERNAL_ERROR,
	GZVM_EXIT_SYSTEM_EVENT,
	GZVM_EXIT_SHUTDOWN,
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
	__u8 immediate_exit;
	__u8 padding1[3];
	/* union structure of collection of guest exit reason */
	union {
		/* GZVM_EXIT_MMIO */
		struct {
			__u64 phys_addr;		/* from FAR_EL2 */
			__u8 data[8];
			__u64 size;			/* from ESR_EL2 as */
			__u32 reg_nr;			/* from ESR_EL2 */
			__u8 is_write;			/* from ESR_EL2 */
		} mmio;
		/* GZVM_EXIT_FAIL_ENTRY */
		struct {
			__u64 hardware_entry_failure_reason;
			__u32 cpu;
		} fail_entry;
		/* GZVM_EXIT_EXCEPTION */
		struct {
			__u32 exception;
			__u32 error_code;
		} exception;
		/* GZVM_EXIT_HVC */
		struct {
			__u64 args[8];	/* in-out */
		} hvc;
		/* GZVM_EXIT_INTERNAL_ERROR */
		struct {
			__u32 suberror;
			/* Available with GZVM_CAP_INTERNAL_ERROR_DATA: */
			__u32 ndata;
			__u64 data[16];
		} internal;
		/* GZVM_EXIT_SYSTEM_EVENT */
		struct {
#define GZVM_SYSTEM_EVENT_SHUTDOWN       1
#define GZVM_SYSTEM_EVENT_RESET          2
#define GZVM_SYSTEM_EVENT_CRASH          3
#define GZVM_SYSTEM_EVENT_WAKEUP         4
#define GZVM_SYSTEM_EVENT_SUSPEND        5
#define GZVM_SYSTEM_EVENT_SEV_TERM       6
#define GZVM_SYSTEM_EVENT_S2IDLE         7
			__u32 type;
			__u32 ndata;
			union {
#ifndef __KERNEL__
				__u64 flags;
#endif
				__u64 data[16];
			};
		} system_event;
		/* Fix the size of the union. */
		char padding2[256];
	};
};

#define GIC_V3_NR_LRS		16

struct gzvm_vcpu_hwstate {
	__u32 nr_lrs;
	__u64 lr[GIC_V3_NR_LRS];
};


/*
 * On arm64, machine type can be used to request the physical
 * address size for the VM. Bits[7-0] are reserved for the guest
 * PA size shift (i.e, log2(PA_Size)). For backward compatibility,
 * value 0 implies the default IPA size, 40bits.
 */
#define GZVM_VM_TYPE_ARM_IPA_SIZE_MASK	0xffULL
#define GZVM_VM_TYPE_ARM_IPA_SIZE(x)		\
	((x) & GZVM_VM_TYPE_ARM_IPA_SIZE_MASK)

#define GZVM_VM_TYPE_ARM_PROTECTED	(1UL << 31)

#define GZVM_VM_TYPE_MASK	(GZVM_VM_TYPE_ARM_IPA_SIZE_MASK | \
				 GZVM_VM_TYPE_ARM_PROTECTED)

/* GZVM ioctls, most are copied from kvm.h */
#define GZVM_IOC_MAGIC		0x92	/* gz */

/*
 * ioctls for /dev/gzvm fds:
 */
#define GZVM_GET_API_VERSION       _IO(GZVM_IOC_MAGIC,   0x00)
#define GZVM_CREATE_VM             _IO(GZVM_IOC_MAGIC,   0x01)

#define GZVM_CAP_ARM_VM_IPA_SIZE	165
#define GZVM_CAP_ARM_PTRAUTH_ADDRESS	171
#define GZVM_CAP_ARM_PTRAUTH_GENERIC	172
#define GZVM_CAP_ARM_PROTECTED_VM	0xffbadab1

#define GZVM_CHECK_EXTENSION       _IO(GZVM_IOC_MAGIC,   0x03)
/*
 * Get size for mmap(vcpu_fd)
 */
#define GZVM_GET_VCPU_MMAP_SIZE    _IO(GZVM_IOC_MAGIC,   0x04) /* in bytes */

/*
 * ioctls for VM fds
 */

/* for GZVM_SET_MEMORY_REGION */
struct gzvm_memory_region {
	__u32 slot;
	__u32 flags;
	__u64 guest_phys_addr;
	__u64 memory_size; /* bytes */
};
#define GZVM_SET_MEMORY_REGION     _IOW(GZVM_IOC_MAGIC,  0x40, \
					struct gzvm_memory_region)
/*
 * GZVM_CREATE_VCPU receives as a parameter the vcpu slot, and returns
 * a vcpu fd.
 */
#define GZVM_CREATE_VCPU           _IO(GZVM_IOC_MAGIC,   0x41)

/* for GZVM_GET_DIRTY_LOG */
struct gzvm_dirty_log {
	__u32 slot;
	__u32 padding1;
	union {
		void __user *dirty_bitmap; /* one bit per page */
		__u64 padding2;
	};
};
#define GZVM_GET_DIRTY_LOG         _IOW(GZVM_IOC_MAGIC,  0x42, \
					struct gzvm_dirty_log)

#define GZVM_SET_NR_MMU_PAGES      _IO(GZVM_IOC_MAGIC,   0x44)
#define GZVM_GET_NR_MMU_PAGES      _IO(GZVM_IOC_MAGIC,   0x45)

/* for GZVM_SET_USER_MEMORY_REGION */
struct gzvm_userspace_memory_region {
	__u32 slot;
	__u32 flags;
	__u64 guest_phys_addr;
	__u64 memory_size; /* bytes */
	__u64 userspace_addr; /* start of the userspace allocated memory */
};
#define GZVM_SET_USER_MEMORY_REGION _IOW(GZVM_IOC_MAGIC, 0x46, \
					struct gzvm_userspace_memory_region)

/* Device model IOC */
#define GZVM_CREATE_IRQCHIP        _IO(GZVM_IOC_MAGIC,   0x60)

/* for GZVM_IRQ_LINE */
/* GZVM_IRQ_LINE irq field index values */
#define GZVM_IRQ_VCPU2_SHIFT		28
#define GZVM_IRQ_VCPU2_MASK		0xf
#define GZVM_IRQ_TYPE_SHIFT		24
#define GZVM_IRQ_TYPE_MASK		0xf
#define GZVM_IRQ_VCPU_SHIFT		16
#define GZVM_IRQ_VCPU_MASK		0xff
#define GZVM_IRQ_NUM_SHIFT		0
#define GZVM_IRQ_NUM_MASK		0xffff

/* irq_type field */
#define GZVM_IRQ_TYPE_CPU		0
#define GZVM_IRQ_TYPE_SPI		1
#define GZVM_IRQ_TYPE_PPI		2

/* out-of-kernel GIC cpu interrupt injection irq_number field */
#define GZVM_IRQ_CPU_IRQ		0
#define GZVM_IRQ_CPU_FIQ		1

struct gzvm_irq_level {
	/*
	 * ACPI gsi notion of irq.
	 * For IA-64 (APIC model) IOAPIC0: irq 0-23; IOAPIC1: irq 24-47..
	 * For X86 (standard AT mode) PIC0/1: irq 0-15. IOAPIC0: 0-23..
	 * For ARM: See Documentation/virt/kvm/api.rst
	 */
	union {
		__u32 irq;
		__s32 status;
	};
	__u32 level;
};
#define GZVM_IRQ_LINE              _IOW(GZVM_IOC_MAGIC,  0x61, \
					struct gzvm_irq_level)
#define GZVM_IRQ_LINE_STATUS       _IOWR(GZVM_IOC_MAGIC, 0x67, \
					 struct gzvm_irq_level)

/* for GZVM_REGISTER_COALESCED_MMIO / GZVM_UNREGISTER_COALESCED_MMIO */
struct gzvm_coalesced_mmio_zone {
	__u64 addr;
	__u32 size;
	union {
		__u32 pad;
		__u32 pio;
	};
};
#define GZVM_REGISTER_COALESCED_MMIO \
		_IOW(GZVM_IOC_MAGIC,  0x67, struct gzvm_coalesced_mmio_zone)
#define GZVM_UNREGISTER_COALESCED_MMIO \
		_IOW(GZVM_IOC_MAGIC,  0x68, struct gzvm_coalesced_mmio_zone)

#define GZVM_DEV_ASSIGN_ENABLE_IOMMU	(1 << 0)
#define GZVM_DEV_ASSIGN_PCI_2_3		(1 << 1)
#define GZVM_DEV_ASSIGN_MASK_INTX	(1 << 2)

struct gzvm_assigned_pci_dev {
	__u32 assigned_dev_id;
	__u32 busnr;
	__u32 devfn;
	__u32 flags;
	__u32 segnr;
	union {
		__u32 reserved[11];
	};
};
#define GZVM_ASSIGN_PCI_DEVICE     _IOR(GZVM_IOC_MAGIC,  0x69, \
				       struct gzvm_assigned_pci_dev)

struct gzvm_irq_routing_irqchip {
	__u32 irqchip;
	__u32 pin;
};

/* gsi routing entry types */
#define GZVM_IRQ_ROUTING_IRQCHIP 1

struct gzvm_irq_routing_entry {
	__u32 gsi;
	__u32 type;
	__u32 flags;
	__u32 pad;
	union {
		struct gzvm_irq_routing_irqchip irqchip;
		__u32 pad[8];
	} u;
};

struct gzvm_irq_routing {
	__u32 nr;
	__u32 flags;
	struct gzvm_irq_routing_entry entries[0];
};
#define GZVM_SET_GSI_ROUTING       _IOW(GZVM_IOC_MAGIC,  0x6a, \
					struct gzvm_irq_routing)

#define GZVM_DEV_IRQ_HOST_INTX    (1 << 0)

#define GZVM_DEV_IRQ_GUEST_INTX   (1 << 8)

#define GZVM_DEV_IRQ_HOST_MASK	 0x00ff
#define GZVM_DEV_IRQ_GUEST_MASK   0xff00

struct gzvm_assigned_irq {
	__u32 assigned_dev_id;
	__u32 host_irq; /* ignored (legacy field) */
	__u32 guest_irq;
	__u32 flags;
	union {
		__u32 reserved[12];
	};
};
#define GZVM_ASSIGN_DEV_IRQ        _IOW(GZVM_IOC_MAGIC,  0x70, \
					struct gzvm_assigned_irq)
#define GZVM_DEASSIGN_PCI_DEVICE   _IOW(GZVM_IOC_MAGIC,  0x72, \
				       struct gzvm_assigned_pci_dev)
#define GZVM_DEASSIGN_DEV_IRQ      _IOW(GZVM_IOC_MAGIC,  0x75, \
					struct gzvm_assigned_irq)

#define GZVM_IRQFD_FLAG_DEASSIGN (1 << 0)
/*
 * Available with GZVM_CAP_IRQFD_RESAMPLE
 *
 * GZVM_IRQFD_FLAG_RESAMPLE indicates resamplefd is valid and specifies
 * the irqfd to operate in resampling mode for level triggered interrupt
 * emulation.  See Documentation/virt/kvm/api.rst.
 */
#define GZVM_IRQFD_FLAG_RESAMPLE (1 << 1)

struct gzvm_irqfd {
	__u32 fd;
	__u32 gsi;
	__u32 flags;
	__u32 resamplefd;
	__u8  pad[16];
};
#define GZVM_IRQFD                 _IOW(GZVM_IOC_MAGIC,  0x76, \
					struct gzvm_irqfd)

enum {
	gzvm_ioeventfd_flag_nr_datamatch,
	gzvm_ioeventfd_flag_nr_pio,
	gzvm_ioeventfd_flag_nr_deassign,
	gzvm_ioeventfd_flag_nr_max,
};

#define GZVM_IOEVENTFD_FLAG_DATAMATCH (1 << gzvm_ioeventfd_flag_nr_datamatch)
#define GZVM_IOEVENTFD_FLAG_PIO       (1 << gzvm_ioeventfd_flag_nr_pio)
#define GZVM_IOEVENTFD_FLAG_DEASSIGN  (1 << gzvm_ioeventfd_flag_nr_deassign)
#define GZVM_IOEVENTFD_VALID_FLAG_MASK  ((1 << gzvm_ioeventfd_flag_nr_max) - 1)

struct gzvm_ioeventfd {
	__u64 datamatch;
	__u64 addr;        /* legal pio/mmio address */
	__u32 len;         /* 1, 2, 4, or 8 bytes; or 0 to ignore length */
	__s32 fd;
	__u32 flags;
	__u8  pad[36];
};

#define GZVM_IOEVENTFD             _IOW(GZVM_IOC_MAGIC,  0x79, \
					struct gzvm_ioeventfd)

/* GZVM_ARM_MTE_COPY_TAGS */
struct gzvm_arm_copy_mte_tags {
	__u64 guest_ipa;
	__u64 length;
	void __user *addr;
	__u64 flags;
	__u64 reserved[2];
};
#define GZVM_ARM_MTE_COPY_TAGS	   _IOR(GZVM_IOC_MAGIC,  0xb4, \
					struct gzvm_arm_copy_mte_tags)

enum gzvm_device_type {
	GZVM_DEV_TYPE_ARM_VGIC_V3_DIST,
	GZVM_DEV_TYPE_ARM_VGIC_V3_REDIST,
	GZVM_DEV_TYPE_MAX,
};

struct gzvm_create_device {
	__u32 dev_type;			/* device type */
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
#define GZVM_CREATE_DEVICE	   _IOWR(GZVM_IOC_MAGIC,  0xe0, \
					struct gzvm_create_device)


/*
 * ioctls for vcpu fds
 */
#define GZVM_RUN                   _IO(GZVM_IOC_MAGIC,   0x80)
#define GZVM_GET_REGS              _IOR(GZVM_IOC_MAGIC,  0x81, \
					struct gzvm_cpu_user_regs)
#define GZVM_SET_REGS              _IOW(GZVM_IOC_MAGIC,  0x82, \
					struct gzvm_cpu_user_regs)

/* for GZVM_SET_SIGNAL_MASK */
struct gzvm_signal_mask {
	__u32 len;
	__u8  sigset[0];
};
#define GZVM_SET_SIGNAL_MASK       _IOW(GZVM_IOC_MAGIC,  0x8b, \
					struct gzvm_signal_mask)

/* GZVM_{GET,SET}_MP_STATE */
struct gzvm_mp_state {
	__u32 mp_state;
};
#define GZVM_GET_MP_STATE          _IOR(GZVM_IOC_MAGIC,  0x98, \
					struct gzvm_mp_state)
#define GZVM_SET_MP_STATE          _IOW(GZVM_IOC_MAGIC,  0x99, \
					struct gzvm_mp_state)
/* Available with GZVM_CAP_SET_GUEST_DEBUG */
#define GZVM_ARM_MAX_DBG_REGS 16
struct gzvm_guest_debug_arch {
	__u64 dbg_bcr[GZVM_ARM_MAX_DBG_REGS];
	__u64 dbg_bvr[GZVM_ARM_MAX_DBG_REGS];
	__u64 dbg_wcr[GZVM_ARM_MAX_DBG_REGS];
	__u64 dbg_wvr[GZVM_ARM_MAX_DBG_REGS];
};

struct gzvm_guest_debug {
	__u32 control;
	__u32 pad;
	struct gzvm_guest_debug_arch arch;
};

#define GZVM_SET_GUEST_DEBUG       _IOW(GZVM_IOC_MAGIC,  0x9b, \
					struct gzvm_guest_debug)

/* Available with GZVM_CAP_VCPU_EVENTS */
/* for GZVM_GET/SET_VCPU_EVENTS */
struct gzvm_vcpu_events {
	struct {
		__u8 serror_pending;
		__u8 serror_has_esr;
		__u8 ext_dabt_pending;
		/* Align it to 8 bytes */
		__u8 pad[5];
		__u64 serror_esr;
	} exception;
	__u32 reserved[12];
};
#define GZVM_GET_VCPU_EVENTS       _IOR(GZVM_IOC_MAGIC,  0x9f, \
					struct gzvm_vcpu_events)
#define GZVM_SET_VCPU_EVENTS       _IOW(GZVM_IOC_MAGIC,  0xa0, \
					struct gzvm_vcpu_events)

#define GZVM_CAP_ARM_PROTECTED_VM_FLAGS_SET_FW_IPA	0
#define GZVM_CAP_ARM_PROTECTED_VM_FLAGS_INFO		1
/*
 * vcpu version available with GZVM_ENABLE_CAP
 * vm version available with GZVM_CAP_ENABLE_CAP_VM
 */
/* for GZVM_ENABLE_CAP */
struct gzvm_enable_cap {
	/* in */
	__u32 cap;
	__u32 flags;
	__u64 args[4];
	__u8  pad[64];
};
#define GZVM_ENABLE_CAP            _IOW(GZVM_IOC_MAGIC,  0xa3, \
					struct gzvm_enable_cap)

/* Available with GZVM_CAP_ONE_REG */
struct gzvm_one_reg {
	__u64 id;
	__u64 addr;
};
#define GZVM_GET_ONE_REG	   _IOW(GZVM_IOC_MAGIC,  0xab, \
					struct gzvm_one_reg)
#define GZVM_SET_ONE_REG	   _IOW(GZVM_IOC_MAGIC,  0xac, \
					struct gzvm_one_reg)

/* GZVM_ARM_VCPU_INIT */
struct gzvm_vcpu_init {
	__u32 target;
	__u32 features[7];
};
#define GZVM_ARM_VCPU_INIT	   _IOW(GZVM_IOC_MAGIC,  0xae, \
					struct gzvm_vcpu_init)
#define GZVM_ARM_PREFERRED_TARGET  _IOR(GZVM_IOC_MAGIC,  0xaf, \
					struct gzvm_vcpu_init)

/* Available with GZVM_CAP_ONE_REG */

#define GZVM_REG_ARCH_MASK	0xff00000000000000ULL
#define GZVM_REG_GENERIC	0x0000000000000000ULL

/*
 * Architecture specific registers are to be defined in arch headers and
 * ORed with the arch identifier.
 */
#define GZVM_REG_ARM		0x4000000000000000ULL
#define GZVM_REG_ARM64		0x6000000000000000ULL

#define GZVM_REG_SIZE_SHIFT	52
#define GZVM_REG_SIZE_MASK	0x00f0000000000000ULL
#define GZVM_REG_SIZE_U8	0x0000000000000000ULL
#define GZVM_REG_SIZE_U16	0x0010000000000000ULL
#define GZVM_REG_SIZE_U32	0x0020000000000000ULL
#define GZVM_REG_SIZE_U64	0x0030000000000000ULL
#define GZVM_REG_SIZE_U128	0x0040000000000000ULL
#define GZVM_REG_SIZE_U256	0x0050000000000000ULL
#define GZVM_REG_SIZE_U512	0x0060000000000000ULL
#define GZVM_REG_SIZE_U1024	0x0070000000000000ULL
#define GZVM_REG_SIZE_U2048	0x0080000000000000ULL

struct gzvm_reg_list {
	__u64 n; /* number of regs */
	__u64 reg[0];
};
#define GZVM_GET_REG_LIST	   _IOWR(GZVM_IOC_MAGIC, 0xb0, \
					 struct gzvm_reg_list)
/* Memory Encryption Commands */

/* GZVM_MEMORY_ENCRYPT_REG_REGION */
struct gzvm_enc_region {
	__u64 addr;
	__u64 size;
};
#define GZVM_MEMORY_ENCRYPT_OP     _IOWR(GZVM_IOC_MAGIC, 0xba, unsigned long)
#define GZVM_MEMORY_ENCRYPT_REG_REGION    _IOR(GZVM_IOC_MAGIC, 0xbb, \
					       struct gzvm_enc_region)
#define GZVM_MEMORY_ENCRYPT_UNREG_REGION  _IOR(GZVM_IOC_MAGIC, 0xbc, \
					       struct gzvm_enc_region)

/* Available with GZVM_CAP_MANUAL_DIRTY_LOG_PROTECT_2 */
/* for GZVM_CLEAR_DIRTY_LOG */
struct gzvm_clear_dirty_log {
	__u32 slot;
	__u32 num_pages;
	__u64 first_page;
	union {
		void __user *dirty_bitmap; /* one bit per page */
		__u64 padding2;
	};
};
#define GZVM_CLEAR_DIRTY_LOG       _IOWR(GZVM_IOC_MAGIC, 0xc0, \
					 struct gzvm_clear_dirty_log)

/* Available with GZVM_CAP_ARM_SVE */
#define GZVM_ARM_VCPU_FINALIZE	  _IOW(GZVM_IOC_MAGIC,  0xc2, int)

#define GZVM_NR_SPSR	5
struct gzvm_regs {
	struct user_pt_regs regs;	/* sp = sp_el0 */

	__u64	sp_el1;
	__u64	elr_el1;

	__u64	spsr[GZVM_NR_SPSR];

	struct user_fpsimd_state fp_regs;
};

/*
 * Supported CPU Targets - Adding a new target type is not recommended,
 * unless there are some special registers not supported by the
 * genericv8 syreg table.
 */
/* Generic ARM v8 target */
#define GZVM_ARM_TARGET_GENERIC_V8	5

/*
 * Reset caused by a PSCI v1.1 SYSTEM_RESET2 call.
 * Valid only when the system event has a type of GZVM_SYSTEM_EVENT_RESET.
 */
#define GZVM_SYSTEM_EVENT_RESET_FLAG_PSCI_RESET2	(1ULL << 0)

#define GZVM_MEM_LOG_DIRTY_PAGES	(1UL << 0)
#define GZVM_MEM_READONLY		(1UL << 1)

#define GZVM_ARM_VCPU_POWER_OFF		0 /* CPU is started in OFF state */
#define GZVM_ARM_VCPU_EL1_32BIT		1 /* CPU running a 32bit VM */
#define GZVM_ARM_VCPU_PSCI_0_2		2 /* CPU uses PSCI v0.2 */
#define GZVM_ARM_VCPU_PMU_V3		3 /* Support guest PMUv3 */
#define GZVM_ARM_VCPU_SVE		4 /* enable SVE for this CPU */
#define GZVM_ARM_VCPU_PTRAUTH_ADDRESS	5 /* VCPU uses address authentication */
#define GZVM_ARM_VCPU_PTRAUTH_GENERIC	6 /* VCPU uses generic authentication */

/* If you need to interpret the index values, here is the key: */
#define GZVM_REG_ARM_COPROC_MASK	0x000000000FFF0000
#define GZVM_REG_ARM_COPROC_SHIFT	16

/* Normal registers are mapped as coprocessor 16. */
#define GZVM_REG_ARM_CORE		(0x0010 << GZVM_REG_ARM_COPROC_SHIFT)
#define GZVM_REG_ARM_CORE_REG(name)	(offsetof(struct gzvm_regs, name) / sizeof(__u32))

/* Some registers need more space to represent values. */
#define GZVM_REG_ARM_DEMUX		(0x0011 << GZVM_REG_ARM_COPROC_SHIFT)
#define GZVM_REG_ARM_DEMUX_ID_MASK	0x000000000000FF00
#define GZVM_REG_ARM_DEMUX_ID_SHIFT	8
#define GZVM_REG_ARM_DEMUX_ID_CCSIDR	(0x00 << GZVM_REG_ARM_DEMUX_ID_SHIFT)
#define GZVM_REG_ARM_DEMUX_VAL_MASK	0x00000000000000FF
#define GZVM_REG_ARM_DEMUX_VAL_SHIFT	0

/* AArch64 system registers */
#define GZVM_REG_ARM64_SYSREG		(0x0013 << GZVM_REG_ARM_COPROC_SHIFT)
#define GZVM_REG_ARM64_SYSREG_OP0_MASK	0x000000000000c000
#define GZVM_REG_ARM64_SYSREG_OP0_SHIFT	14
#define GZVM_REG_ARM64_SYSREG_OP1_MASK	0x0000000000003800
#define GZVM_REG_ARM64_SYSREG_OP1_SHIFT	11
#define GZVM_REG_ARM64_SYSREG_CRN_MASK	0x0000000000000780
#define GZVM_REG_ARM64_SYSREG_CRN_SHIFT	7
#define GZVM_REG_ARM64_SYSREG_CRM_MASK	0x0000000000000078
#define GZVM_REG_ARM64_SYSREG_CRM_SHIFT	3
#define GZVM_REG_ARM64_SYSREG_OP2_MASK	0x0000000000000007
#define GZVM_REG_ARM64_SYSREG_OP2_SHIFT	0

/* Physical Timer EL0 Registers */
#define GZVM_REG_ARM_PTIMER_CTL		ARM64_SYS_REG(3, 3, 14, 2, 1)
#define GZVM_REG_ARM_PTIMER_CVAL	ARM64_SYS_REG(3, 3, 14, 2, 2)
#define GZVM_REG_ARM_PTIMER_CNT		ARM64_SYS_REG(3, 3, 14, 0, 1)

/* KVM-as-firmware specific pseudo-registers */
#define GZVM_REG_ARM_FW			(0x0014 << GZVM_REG_ARM_COPROC_SHIFT)
#define GZVM_REG_ARM_FW_REG(r)		(GZVM_REG_ARM64 | GZVM_REG_SIZE_U64 | \
					 GZVM_REG_ARM_FW | ((r) & 0xffff))

/* SVE registers */
#define GZVM_REG_ARM64_SVE		(0x0015 << KVM_REG_ARM_COPROC_SHIFT)

static inline unsigned int
assemble_vm_vcpu_tuple(gzvm_id_t vmid, gzvm_vcpu_id_t vcpuid)
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

/*
 * The following data structures are for data transferring between driver and
 * hypervisor
 */

#define GZVM_MAX_MEM_REGION	10

/* identical to ffa memory constituent */
struct mem_region_addr_range {
	/* The base IPA of the constituent memory region, aligned to 4 kiB */
	__u64 address;
	/* The number of 4 kiB pages in the constituent memory region. */
	__u32 pg_cnt;
	__u32 reserved;
};

struct gzvm_memory_region_ranges {
	__u32 slot;
	__u32 constituent_cnt;
	__u64 total_pages;
	__u64 gpa;
	struct mem_region_addr_range constituents[];
};

#endif /* __GZVM_COMMON_H__ */
