/* SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note */
/*
 * Copyright (c) 2023 MediaTek Inc.
 */

/*
 * This header is mostly copied from uapi/kvm.h, and replace KVMIO with
 * GZVM_IOC_MAGIC, but keep sequence number the same. Some ioctls which are
 * obviously arch-dependant are not included.
 */

#ifndef __GZVM_IOCTL_H__
#define __GZVM_IOCTL_H__

#include "gzvm_common.h"

#define GZVM_IOC_MAGIC		0x92

/*
 * ioctls for /dev/kvm fds:
 */
#define GZVM_GET_API_VERSION       _IO(GZVM_IOC_MAGIC, 0x00)
#define GZVM_CREATE_VM             _IO(GZVM_IOC_MAGIC, 0x01)

#define GZVM_CHECK_EXTENSION       _IO(GZVM_IOC_MAGIC,   0x03)
/*
 * Get size for mmap(vcpu_fd)
 */
#define GZVM_GET_VCPU_MMAP_SIZE    _IO(GZVM_IOC_MAGIC,   0x04) /* in bytes */

/*
 * ioctls for VM fds
 */
#define GZVM_SET_MEMORY_REGION     _IOW(GZVM_IOC_MAGIC,  0x40, \
					struct kvm_memory_region)
/*
 * GZVM_CREATE_VCPU receives as a parameter the vcpu slot, and returns
 * a vcpu fd.
 */
#define GZVM_CREATE_VCPU           _IO(GZVM_IOC_MAGIC,   0x41)
#define GZVM_GET_DIRTY_LOG         _IOW(GZVM_IOC_MAGIC,  0x42, \
					struct kvm_dirty_log)

#define GZVM_SET_NR_MMU_PAGES      _IO(GZVM_IOC_MAGIC,   0x44)
#define GZVM_GET_NR_MMU_PAGES      _IO(GZVM_IOC_MAGIC,   0x45)
#define GZVM_SET_USER_MEMORY_REGION _IOW(GZVM_IOC_MAGIC, 0x46, \
					struct kvm_userspace_memory_region)

/* Device model IOC */
#define GZVM_CREATE_IRQCHIP        _IO(GZVM_IOC_MAGIC,   0x60)
#define GZVM_IRQ_LINE              _IOW(GZVM_IOC_MAGIC,  0x61, \
					struct kvm_irq_level)

#define GZVM_IRQ_LINE_STATUS       _IOWR(GZVM_IOC_MAGIC, 0x67, \
					 struct kvm_irq_level)
#define GZVM_REGISTER_COALESCED_MMIO \
		_IOW(GZVM_IOC_MAGIC,  0x67, struct kvm_coalesced_mmio_zone)
#define GZVM_UNREGISTER_COALESCED_MMIO \
		_IOW(GZVM_IOC_MAGIC,  0x68, struct kvm_coalesced_mmio_zone)
#define GZVM_ASSIGN_PCI_DEVICE     _IOR(GZVM_IOC_MAGIC,  0x69, \
				       struct kvm_assigned_pci_dev)
#define GZVM_SET_GSI_ROUTING       _IOW(GZVM_IOC_MAGIC,  0x6a, \
					struct kvm_irq_routing)
#define GZVM_ASSIGN_DEV_IRQ        _IOW(GZVM_IOC_MAGIC,  0x70, \
					struct kvm_assigned_irq)
#define GZVM_DEASSIGN_PCI_DEVICE   _IOW(GZVM_IOC_MAGIC,  0x72, \
				       struct kvm_assigned_pci_dev)
#define GZVM_ASSIGN_SET_MSIX_NR    _IOW(GZVM_IOC_MAGIC,  0x73, \
				       struct kvm_assigned_msix_nr)
#define GZVM_ASSIGN_SET_MSIX_ENTRY _IOW(GZVM_IOC_MAGIC,  0x74, \
				       struct kvm_assigned_msix_entry)
#define GZVM_DEASSIGN_DEV_IRQ      _IOW(GZVM_IOC_MAGIC,  0x75, \
					struct kvm_assigned_irq)
#define GZVM_IRQFD                 _IOW(GZVM_IOC_MAGIC,  0x76, struct kvm_irqfd)
#define GZVM_IOEVENTFD             _IOW(GZVM_IOC_MAGIC,  0x79, \
					struct kvm_ioeventfd)

/* Available with KVM_CAP_SIGNAL_MSI */
#define GZVM_SIGNAL_MSI            _IOW(GZVM_IOC_MAGIC,  0xa5, struct kvm_msi)

/* Available with KVM_CAP_ARM_SET_DEVICE_ADDR */
#define GZVM_ARM_SET_DEVICE_ADDR   _IOW(GZVM_IOC_MAGIC,  0xab, \
					struct kvm_arm_device_addr)
/* Available with KVM_CAP_PMU_EVENT_FILTER */
#define GZVM_SET_PMU_EVENT_FILTER  _IOW(GZVM_IOC_MAGIC,  0xb2, \
					struct kvm_pmu_event_filter)
#define GZVM_ARM_MTE_COPY_TAGS	   _IOR(GZVM_IOC_MAGIC,  0xb4, \
					struct kvm_arm_copy_mte_tags)

/* ioctl for vm fd */
#define GZVM_CREATE_DEVICE	   _IOWR(GZVM_IOC_MAGIC,  0xe0, \
					struct gzvm_create_device)

/* ioctls for fds returned by GZVM_CREATE_DEVICE */
#define GZVM_SET_DEVICE_ATTR	   _IOW(GZVM_IOC_MAGIC,  0xe1, \
					struct kvm_device_attr)
#define GZVM_GET_DEVICE_ATTR	   _IOW(GZVM_IOC_MAGIC,  0xe2, \
					struct kvm_device_attr)
#define GZVM_HAS_DEVICE_ATTR	   _IOW(GZVM_IOC_MAGIC,  0xe3, \
					struct kvm_device_attr)

/*
 * ioctls for vcpu fds
 */
#define GZVM_RUN                   _IO(GZVM_IOC_MAGIC,   0x80)
#define GZVM_GET_REGS              _IOR(GZVM_IOC_MAGIC,  0x81, \
					struct cpu_user_regs)
#define GZVM_SET_REGS              _IOW(GZVM_IOC_MAGIC,  0x82, \
					struct cpu_user_regs)

#define GZVM_SET_SIGNAL_MASK       _IOW(GZVM_IOC_MAGIC,  0x8b, \
					struct kvm_signal_mask)

#define GZVM_GET_MP_STATE          _IOR(GZVM_IOC_MAGIC,  0x98, \
					struct kvm_mp_state)
#define GZVM_SET_MP_STATE          _IOW(GZVM_IOC_MAGIC,  0x99, \
					struct kvm_mp_state)
/* Available with GZVM_CAP_SET_GUEST_DEBUG */
#define GZVM_SET_GUEST_DEBUG       _IOW(GZVM_IOC_MAGIC,  0x9b, \
					struct kvm_guest_debug)
/* Available with GZVM_CAP_VCPU_EVENTS */
#define GZVM_GET_VCPU_EVENTS       _IOR(GZVM_IOC_MAGIC,  0x9f, \
					struct kvm_vcpu_events)
#define GZVM_SET_VCPU_EVENTS       _IOW(GZVM_IOC_MAGIC,  0xa0, \
					struct kvm_vcpu_events)
/*
 * vcpu version available with GZVM_ENABLE_CAP
 * vm version available with GZVM_CAP_ENABLE_CAP_VM
 */
#define GZVM_ENABLE_CAP            _IOW(GZVM_IOC_MAGIC,  0xa3, \
					struct kvm_enable_cap)

/* Available with GZVM_CAP_ONE_REG */
#define GZVM_GET_ONE_REG	   _IOW(GZVM_IOC_MAGIC,  0xab, \
					struct kvm_one_reg)
#define GZVM_SET_ONE_REG	   _IOW(GZVM_IOC_MAGIC,  0xac, \
					struct kvm_one_reg)
/* VM is being stopped by host */
#define GZVM_KVMCLOCK_CTRL	   _IO(GZVM_IOC_MAGIC,   0xad)
#define GZVM_ARM_VCPU_INIT	   _IOW(GZVM_IOC_MAGIC,  0xae, \
					struct kvm_vcpu_init)
#define GZVM_ARM_PREFERRED_TARGET  _IOR(GZVM_IOC_MAGIC,  0xaf, \
					struct kvm_vcpu_init)
#define GZVM_GET_REG_LIST	   _IOWR(GZVM_IOC_MAGIC, 0xb0, \
					 struct kvm_reg_list)
/* Memory Encryption Commands */
#define GZVM_MEMORY_ENCRYPT_OP     _IOWR(GZVM_IOC_MAGIC, 0xba, unsigned long)

#define GZVM_MEMORY_ENCRYPT_REG_REGION    _IOR(GZVM_IOC_MAGIC, 0xbb, \
					       struct kvm_enc_region)
#define GZVM_MEMORY_ENCRYPT_UNREG_REGION  _IOR(GZVM_IOC_MAGIC, 0xbc, \
					       struct kvm_enc_region)

/* Available with GZVM_CAP_MANUAL_DIRTY_LOG_PROTECT_2 */
#define GZVM_CLEAR_DIRTY_LOG       _IOWR(GZVM_IOC_MAGIC, 0xc0, \
					 struct kvm_clear_dirty_log)

/* Available with GZVM_CAP_ARM_SVE */
#define GZVM_ARM_VCPU_FINALIZE	  _IOW(GZVM_IOC_MAGIC,  0xc2, int)

#endif /* __GZVM_IOCTL_H__ */
