/* SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note */
/*
 * Copyright (c) 2023 MediaTek Inc.
 */

#ifndef __GZVM_H__
#define __GZVM_H__

#include <linux/const.h>
#include <linux/types.h>
#include <linux/ioctl.h>

#include <asm/gzvm_arch.h>

/**
 * DOC: This file declares common data structure shared between userspace,
 *	kernel space, and GZ.
 */

typedef __u16 gzvm_id_t;
typedef __u16 gzvm_vcpu_id_t;

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
 * GZVM_CREATE_VCPU receives as a parameter the vcpu slot, and returns
 * a vcpu fd.
 */
#define GZVM_CREATE_VCPU           _IO(GZVM_IOC_MAGIC,   0x41)

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

/* ioctls for vcpu fds */
#define GZVM_RUN                   _IO(GZVM_IOC_MAGIC,   0x80)

/* for GZVM_ENABLE_CAP */
struct gzvm_enable_cap {
	/* in */
	__u64 cap;
	/* we have total 5 (8 - 3) registers can be used for additional args */
	__u64 args[5];
};

#define GZVM_ENABLE_CAP            _IOW(GZVM_IOC_MAGIC,  0xa3, \
					struct gzvm_enable_cap)

#endif /* __GZVM_H__ */
