/* SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note */
/*
 * Copyright (c) 2023 MediaTek Inc.
 */

/**
 * DOC: UAPI of GenieZone Hypervisor
 *
 * This file declares common data structure shared among user space,
 * kernel space, and GenieZone hypervisor.
 */
#ifndef __GZVM_H__
#define __GZVM_H__

#include <linux/const.h>
#include <linux/types.h>
#include <linux/ioctl.h>

#include <asm/gzvm_arch.h>

/* GZVM ioctls */
#define GZVM_IOC_MAGIC			0x92	/* gz */

/* ioctls for /dev/gzvm fds */
#define GZVM_GET_API_VERSION       _IO(GZVM_IOC_MAGIC,   0x00)
#define GZVM_CREATE_VM             _IO(GZVM_IOC_MAGIC,   0x01)

#define GZVM_CHECK_EXTENSION       _IO(GZVM_IOC_MAGIC,   0x03)

/* ioctls for VM fds */
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
 * GZVM_CREATE_VCPU receives as a parameter the vcpu slot,
 * and returns a vcpu fd.
 */
#define GZVM_CREATE_VCPU           _IO(GZVM_IOC_MAGIC,   0x41)

/* for GZVM_SET_USER_MEMORY_REGION */
struct gzvm_userspace_memory_region {
	__u32 slot;
	__u32 flags;
	__u64 guest_phys_addr;
	/* bytes */
	__u64 memory_size;
	/* start of the userspace allocated memory */
	__u64 userspace_addr;
};

#define GZVM_SET_USER_MEMORY_REGION _IOW(GZVM_IOC_MAGIC, 0x46, \
					 struct gzvm_userspace_memory_region)

#define GZVM_VTIMER_IRQ			27

/* for GZVM_IRQ_LINE, irq field index values */
#define GZVM_IRQ_VCPU_MASK		0xff
#define GZVM_IRQ_LINE_TYPE		GENMASK(27, 24)
#define GZVM_IRQ_LINE_VCPU		GENMASK(23, 16)
#define GZVM_IRQ_LINE_VCPU2		GENMASK(31, 28)
#define GZVM_IRQ_LINE_NUM		GENMASK(15, 0)

/* irq_type field */
#define GZVM_IRQ_TYPE_CPU		0
#define GZVM_IRQ_TYPE_SPI		1
#define GZVM_IRQ_TYPE_PPI		2

/* out-of-kernel GIC cpu interrupt injection irq_number field */
#define GZVM_IRQ_CPU_IRQ		0
#define GZVM_IRQ_CPU_FIQ		1

struct gzvm_irq_level {
	union {
		__u32 irq;
		__s32 status;
	};
	__u32 level;
};

#define GZVM_IRQ_LINE              _IOW(GZVM_IOC_MAGIC,  0x61, \
					struct gzvm_irq_level)

enum gzvm_device_type {
	GZVM_DEV_TYPE_ARM_VGIC_V3_DIST = 0,
	GZVM_DEV_TYPE_ARM_VGIC_V3_REDIST = 1,
	GZVM_DEV_TYPE_MAX,
};

/**
 * struct gzvm_create_device: for GZVM_CREATE_DEVICE.
 *
 * Store information needed to create device.
 */
struct gzvm_create_device {
	/* private: internal use only */
	/* device type */
	__u32 dev_type;
	/* out: device id */
	__u32 id;
	/* device specific flags */
	__u64 flags;
	/* device ipa address in VM's view */
	__u64 dev_addr;
	/* device register range size */
	__u64 dev_reg_size;
	/*
	 * If user -> kernel, this is user virtual address of device specific
	 * attributes (if needed). If kernel->hypervisor, this is ipa.
	 */
	__u64 attr_addr;
	/* size of device specific attributes */
	__u64 attr_size;
};

#define GZVM_CREATE_DEVICE	   _IOWR(GZVM_IOC_MAGIC,  0xe0, \
					struct gzvm_create_device)

/*
 * ioctls for vcpu fds
 */
#define GZVM_RUN                   _IO(GZVM_IOC_MAGIC,   0x80)

/* VM exit reason */
enum {
	GZVM_EXIT_UNKNOWN = 0x92920000,
	GZVM_EXIT_MMIO = 0x92920001,
	GZVM_EXIT_HYPERCALL = 0x92920002,
	GZVM_EXIT_IRQ = 0x92920003,
	GZVM_EXIT_EXCEPTION = 0x92920004,
	GZVM_EXIT_DEBUG = 0x92920005,
	GZVM_EXIT_FAIL_ENTRY = 0x92920006,
	GZVM_EXIT_INTERNAL_ERROR = 0x92920007,
	GZVM_EXIT_SYSTEM_EVENT = 0x92920008,
	GZVM_EXIT_SHUTDOWN = 0x92920009,
	GZVM_EXIT_GZ = 0x9292000a,
};

/* exception definitions of GZVM_EXIT_EXCEPTION */
enum {
	GZVM_EXCEPTION_UNKNOWN = 0x0,
	GZVM_EXCEPTION_PAGE_FAULT = 0x1,
};

/**
 * struct gzvm_vcpu_run: Same purpose as kvm_run, this struct is
 *			shared between userspace, kernel and
 *			GenieZone hypervisor
 *
 * Keep identical layout between the 3 modules
 */
struct gzvm_vcpu_run {
	/* private: internal use only */
	/* to userspace */
	__u32 exit_reason;
	__u8 immediate_exit;
	/* reserved for future use and must be zero filled */
	__u8 padding1[3];
	/* union structure of collection of guest exit reason */
	union {
		/* GZVM_EXIT_MMIO */
		struct {
			/* from FAR_EL2 */
			__u64 phys_addr;
			__u8 data[8];
			/* from ESR_EL2 as */
			__u64 size;
			/* from ESR_EL2 */
			__u32 reg_nr;
			/* from ESR_EL2 */
			__u8 is_write;
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
			__u64 fault_gpa;
			/**
			 * future-proof reservation and should be zeroed, and this can also
			 * fix the offset of `gzvm_arch_exception`
			 */
			__u64 reserved[6];
			struct gzvm_arch_exception arch;
		} exception;
		/* GZVM_EXIT_HYPERCALL */
		struct {
			__u64 args[8];	/* in-out */
		} hypercall;
		/* GZVM_EXIT_INTERNAL_ERROR */
		struct {
			__u32 suberror;
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
			__u64 data[16];
		} system_event;
		/* Fix the size of the union. */
		char padding[256];
	};
};

/* for GZVM_ENABLE_CAP */
struct gzvm_enable_cap {
	/* in */
	__u64 cap;
	/**
	 * we have total 5 (8 - 3) registers can be used for
	 * additional args
	 */
	__u64 args[5];
};

#define GZVM_ENABLE_CAP            _IOW(GZVM_IOC_MAGIC,  0xa3, \
					struct gzvm_enable_cap)

/* for GZVM_GET/SET_ONE_REG */
struct gzvm_one_reg {
	__u64 id;
	__u64 addr;
};

#define GZVM_GET_ONE_REG	   _IOW(GZVM_IOC_MAGIC,  0xab, \
					struct gzvm_one_reg)
#define GZVM_SET_ONE_REG	   _IOW(GZVM_IOC_MAGIC,  0xac, \
					struct gzvm_one_reg)

#define GZVM_REG_GENERIC	   0x0000000000000000ULL

#define GZVM_IRQFD_FLAG_DEASSIGN	BIT(0)
/*
 * GZVM_IRQFD_FLAG_RESAMPLE indicates resamplefd is valid and specifies
 * the irqfd to operate in resampling mode for level triggered interrupt
 * emulation.
 */
#define GZVM_IRQFD_FLAG_RESAMPLE	BIT(1)

/**
 * struct gzvm_irqfd: gzvm irqfd descriptor
 * @fd: File descriptor.
 * @gsi: Used for level IRQ fast-path.
 * @flags: FLAG_DEASSIGN or FLAG_RESAMPLE.
 * @resamplefd: The file descriptor of the resampler.
 */
struct gzvm_irqfd {
	__u32 fd;
	__u32 gsi;
	__u32 flags;
	__u32 resamplefd;
	/* private: reserved as padding; use zero, this may
	 * be used in the future
	 */
	__u8  pad[16];
};

#define GZVM_IRQFD	_IOW(GZVM_IOC_MAGIC, 0x76, struct gzvm_irqfd)

enum {
	gzvm_ioeventfd_flag_nr_datamatch = 0,
	gzvm_ioeventfd_flag_nr_pio = 1,
	gzvm_ioeventfd_flag_nr_deassign = 2,
	gzvm_ioeventfd_flag_nr_max,
};

#define GZVM_IOEVENTFD_FLAG_DATAMATCH	(1 << gzvm_ioeventfd_flag_nr_datamatch)
#define GZVM_IOEVENTFD_FLAG_PIO		(1 << gzvm_ioeventfd_flag_nr_pio)
#define GZVM_IOEVENTFD_FLAG_DEASSIGN	(1 << gzvm_ioeventfd_flag_nr_deassign)
#define GZVM_IOEVENTFD_VALID_FLAG_MASK	((1 << gzvm_ioeventfd_flag_nr_max) - 1)

struct gzvm_ioeventfd {
	__u64 datamatch;
	/* private: legal pio/mmio address */
	__u64 addr;
	/* private: 1, 2, 4, or 8 bytes; or 0 to ignore length */
	__u32 len;
	__s32 fd;
	__u32 flags;
	__u8  pad[36];
};

#define GZVM_IOEVENTFD	_IOW(GZVM_IOC_MAGIC, 0x79, struct gzvm_ioeventfd)

/**
 * struct gzvm_dtb_config: store address and size of dtb passed from userspace
 *
 * @dtb_addr: dtb address set by VMM (guset memory)
 * @dtb_size: dtb size
 */
struct gzvm_dtb_config {
	__u64 dtb_addr;
	__u64 dtb_size;
};

#define GZVM_SET_DTB_CONFIG       _IOW(GZVM_IOC_MAGIC, 0xff, \
				       struct gzvm_dtb_config)

#endif /* __GZVM_H__ */
