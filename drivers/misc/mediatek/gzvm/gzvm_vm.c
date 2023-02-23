// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2022 MediaTek Inc.
 */

#include <linux/anon_inodes.h>
#include <linux/arm-smccc.h>
#include <linux/file.h>
#include <linux/kdev_t.h>
#include <linux/kvm_host.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/version.h>
#include <kvm/arm_vgic.h>

#include "gzvm.h"

static DEFINE_MUTEX(gzvm_list_lock);
static LIST_HEAD(gzvm_list);

/**
 * @brief Populate pa to buffer until full
 *
 * @param consti
 * @param max_nr_consti
 * @param gpa
 * @param total_pages
 * @return int how much pages we've fill in
 */
static int fill_constituents(struct mem_region_addr_range *consti,
			     int *consti_cnt, int max_nr_consti, gfn_t gfn,
			     u32 total_pages, struct kvm_memory_slot *slot)
{
	int i, nr_pages;
	kvm_pfn_t pfn, prev_pfn;
	gfn_t gfn_end;

	if (unlikely(total_pages == 0))
		return -EINVAL;
	gfn_end = gfn + total_pages;

	/* entry 0 */
	pfn = gfn_to_pfn_memslot(slot, gfn);
	consti[0].address = kvm_pfn_to_phys(pfn);
	consti[0].pg_cnt = 1;
	gfn++;
	prev_pfn = pfn;
	i = 0;
	nr_pages = 1;
	while (i < max_nr_consti && gfn < gfn_end) {
		pfn = gfn_to_pfn_memslot(slot, gfn);
		if (pfn == (prev_pfn + 1)) {
			consti[i].pg_cnt++;
		} else {
			i++;
			if (i >= max_nr_consti)
				break;
			consti[i].address = kvm_pfn_to_phys(pfn);
			consti[i].pg_cnt = 1;
		}
		prev_pfn = pfn;
		gfn++;
		nr_pages++;
	}
	if (i == max_nr_consti)
		*consti_cnt = i;
	else
		*consti_cnt = (i + 1);

	return nr_pages;
}

/**
 * @brief Register memory region to GZ
 *
 * @param gzvm
 * @param memslot
 * @return int
 */
static int
register_memslot_addr_range(struct gzvm *gzvm, struct gzvm_memslot *memslot)
{
	struct gzvm_memory_region_ranges *region;
	u32 buf_size;
	int max_nr_consti, remain_pages;
	gfn_t gfn, gfn_end;

	/* TODO: a proper size? */
	buf_size = PAGE_SIZE * 2;
	region = alloc_pages_exact(buf_size, GFP_KERNEL);
	if (!region)
		return -ENOMEM;
	max_nr_consti = (buf_size - sizeof(*region)) /
			sizeof(struct mem_region_addr_range);

	region->slot = memslot->slot.id;
	remain_pages = memslot->slot.npages;
	gfn = memslot->slot.base_gfn;
	gfn_end = gfn + remain_pages;
	GZVM_DEBUG("%s ipa=%llx, ipa_end=%llx, remain_pages=%d\n", __func__,
		 gfn_to_phys(gfn), gfn_to_phys(gfn_end), remain_pages);
	while (gfn < gfn_end) {
		struct arm_smccc_res res;
		int nr_pages;

		nr_pages = fill_constituents(region->constituents,
					     &region->constituent_cnt,
					     max_nr_consti, gfn,
					     remain_pages, &memslot->slot);
		region->gpa = gfn_to_phys(gfn);
		region->total_pages = nr_pages;

		remain_pages -= nr_pages;
		gfn += nr_pages;

		gzvm_hypcall_wrapper(MT_HVC_GZVM_SET_MEMREGION, gzvm->vm_id,
			     buf_size, virt_to_phys(region), 0, 0, 0, 0, &res);

		if (res.a0 != 0) {
			GZVM_ERR("Failed to register memregion to hypervisor\n");
			free_pages_exact(region, buf_size);
			return -EFAULT;
		}
	}
	free_pages_exact(region, buf_size);
	return 0;
}

/**
 * @brief Set memory region of guest
 *
 * @param gzvm struct gzvm
 * @param mem struct gzvm_userspace_memory_region: input from user
 * @retval -EXIO memslot is out-of-range
 * @retval -EFAULT  cannot find corresponding vma
 * @retval -EINVAL  region size and vma size does not match
 */
static int gzvm_vm_ioctl_set_memory_region(struct gzvm *gzvm,
				struct gzvm_userspace_memory_region *mem)
{
	struct vm_area_struct *vma;
	struct gzvm_memslot *memslot;
	unsigned long size;
	__u32 slot;

	GZVM_DEBUG("slot=%u, flags=%x, gpa=%llx, size=%llu, uva=%llx\n",
		 mem->slot, mem->flags, mem->guest_phys_addr, mem->memory_size,
		 mem->userspace_addr);

	slot = mem->slot;
	if (slot >= GZVM_MAX_MEM_REGION)
		return -ENXIO;
	memslot = &gzvm->memslot[slot];

	vma = vma_lookup(gzvm->mm, mem->userspace_addr);
	if (!vma) {
		GZVM_ERR("Error to find vma corresponding userva\n");
		return -EFAULT;
	}
	size = vma->vm_end - vma->vm_start;
	if (size != mem->memory_size) {
		GZVM_ERR("Size does not match: vma:%lx mem:%llx\n", size,
		       mem->memory_size);
		return -EINVAL;
	}

	memslot->slot.base_gfn = __phys_to_pfn(mem->guest_phys_addr);
	memslot->slot.npages = size >> PAGE_SHIFT;
	memslot->slot.dirty_bitmap = NULL;
	memslot->slot.userspace_addr = mem->userspace_addr;
	memslot->slot.flags = mem->flags;
	memslot->slot.id = mem->slot;
	memslot->slot.as_id = 0;
	memslot->vma = vma;
	return register_memslot_addr_range(gzvm, memslot);
}

/**
 * @brief Check the irq number and irq_type are matched
 */
static bool is_irq_valid(u32 irq, u32 irq_type)
{
	switch (irq_type) {
	case GZVM_IRQ_TYPE_CPU:	/*  0 ~ 15: SGI */
		if (likely(irq <= GZVM_IRQ_CPU_FIQ))
			return true;
		break;
	case GZVM_IRQ_TYPE_PPI:	/* 16 ~ 31: PPI */
		if (likely(irq >= VGIC_NR_SGIS && irq < VGIC_NR_PRIVATE_IRQS))
			return true;
		break;
	case GZVM_IRQ_TYPE_SPI:	/* 32 ~ : SPT */
		if (likely(irq >= VGIC_NR_PRIVATE_IRQS))
			return true;
		break;
	default:
		return false;
	}
	return false;
}

/**
 * @brief Inject virtual interrupt to a VM
 *
 * @param gzvm
 * @param vcpu_idx: vcpu index, only valid if PPI
 * @param irq: irq number
 * @param irq_type
 * @param level, true: 1; false: 0
 */
int gzvm_vgic_inject_irq(struct gzvm *gzvm, unsigned int vcpu_idx, u32 irq_type,
			 u32 irq, bool level)
{
	unsigned long a1 = assemble_vm_vcpu_tuple(gzvm->vm_id, vcpu_idx);
	struct arm_smccc_res res;

	GZVM_DEBUG("%s vcpu=%u irq=%u irq_type=%d level=%d\n", __func__,
		 vcpu_idx, irq, irq_type, level);

	if (!unlikely(is_irq_valid(irq, irq_type)))
		return -EINVAL;

	gzvm_hypcall_wrapper(MT_HVC_GZVM_IRQ_LINE, a1, irq, level,
			     0, 0, 0, 0, &res);
	if (res.a0) {
		GZVM_ERR("Failed to set IRQ level (%d) to irq#%u on vcpu %d with ret=%d\n",
		       level, irq, vcpu_idx, (int)res.a0);
		return -EFAULT;
	}

	return 0;
}

static int gzvm_vm_ioctl_irq_line(struct gzvm *gzvm,
				  struct gzvm_irq_level *irq_level)
{
	u32 irq = irq_level->irq;
	unsigned int irq_type, vcpu_idx, irq_num;
	bool level = irq_level->level;

	irq_type = (irq >> GZVM_IRQ_TYPE_SHIFT) & GZVM_IRQ_TYPE_MASK;
	vcpu_idx = (irq >> GZVM_IRQ_VCPU_SHIFT) & GZVM_IRQ_VCPU_MASK;
	vcpu_idx += ((irq >> GZVM_IRQ_VCPU2_SHIFT) & GZVM_IRQ_VCPU2_MASK) *
		(GZVM_IRQ_VCPU_MASK + 1);
	irq_num = (irq >> GZVM_IRQ_NUM_SHIFT) & GZVM_IRQ_NUM_MASK;

	return gzvm_vgic_inject_irq(gzvm, vcpu_idx, irq_num, irq_type, level);
}

static int gzvm_vm_ioctl_create_device(struct gzvm *gzvm, void __user *argp)
{
	struct gzvm_create_device *gzvm_dev;
	void *dev_data = NULL;
	struct arm_smccc_res res = {0};
	int ret;

	gzvm_dev = (struct gzvm_create_device *)alloc_pages_exact(PAGE_SIZE,
								  GFP_KERNEL);
	if (!gzvm_dev)
		return -ENOMEM;
	if (copy_from_user(gzvm_dev, argp, sizeof(*gzvm_dev))) {
		ret = -EFAULT;
		goto err_free_dev;
	}

	GZVM_DEBUG("%s type:%d addr:0x%llx, size=0x%llx", __func__,
		 gzvm_dev->dev_type, gzvm_dev->dev_addr,
		 gzvm_dev->dev_reg_size);
	if (gzvm_dev->attr_addr != 0 && gzvm_dev->attr_size != 0) {
		dev_data = alloc_pages_exact(gzvm_dev->attr_size, GFP_KERNEL);
		if (!dev_data) {
			ret = -ENOMEM;
			goto err_free_dev;
		}
		if (copy_from_user(dev_data, (void __user *)gzvm_dev->attr_addr,
				   gzvm_dev->attr_size)) {
			ret = -EFAULT;
			goto err_free_dev_data;
		}
		gzvm_dev->attr_addr = virt_to_phys(dev_data);
	}

	gzvm_hypcall_wrapper(MT_HVC_GZVM_CREATE_DEVICE, gzvm->vm_id,
			     virt_to_phys(gzvm_dev), 0, 0, 0, 0, 0, &res);
	ret = res.a0;
err_free_dev_data:
	if (dev_data)
		free_pages_exact(dev_data, 0);
err_free_dev:
	free_pages_exact(gzvm_dev, 0);
	return ret;
}

/**
 * @brief ioctl handler of VM FD
 *
 * @param filp
 * @param ioctl
 * @param arg
 * @return long
 */
static long gzvm_vm_ioctl(struct file *filp, unsigned int ioctl,
			  unsigned long arg)
{
	long ret = 0;
	void __user *argp = (void __user *)arg;
	struct gzvm *gzvm = filp->private_data;

	switch (ioctl) {
	case GZVM_CREATE_VCPU:
		ret = gzvm_vm_ioctl_create_vcpu(gzvm, arg);
		break;
	case GZVM_SET_USER_MEMORY_REGION: {
		struct gzvm_userspace_memory_region userspace_mem;

		ret = -EFAULT;
		if (copy_from_user(&userspace_mem, argp,
						sizeof(userspace_mem)))
			goto out;
		ret = gzvm_vm_ioctl_set_memory_region(gzvm, &userspace_mem);
		break;
	}
	case GZVM_IRQ_LINE: {
		struct gzvm_irq_level irq_event;

		ret = -EFAULT;
		if (copy_from_user(&irq_event, argp, sizeof(irq_event)))
			goto out;

		ret = gzvm_vm_ioctl_irq_line(gzvm, &irq_event);
		break;
	}
	case GZVM_CREATE_DEVICE: {
		ret = gzvm_vm_ioctl_create_device(gzvm, argp);
		if (ret)
			goto out;
		break;
	}
	case GZVM_CHECK_EXTENSION:
		return gzvm_dev_ioctl_check_extension(gzvm, arg);

	case GZVM_IOEVENTFD: {
		struct gzvm_ioeventfd data;

		ret = -EFAULT;
		if (copy_from_user(&data, argp, sizeof(data)))
			goto out;
		ret = gzvm_ioeventfd(gzvm, &data);
		break;
	}
	case GZVM_IRQFD: {
		struct gzvm_irqfd data;

		ret = -EFAULT;
		if (copy_from_user(&data, argp, sizeof(data)))
			goto out;
		ret = gzvm_irqfd(gzvm, &data);
		break;
	}
	case GZVM_ARM_PREFERRED_TARGET:
		return 0;
	case GZVM_SET_MEMORY_REGION:
	case GZVM_GET_DIRTY_LOG:
	case GZVM_SET_NR_MMU_PAGES:
	case GZVM_GET_NR_MMU_PAGES:
	case GZVM_CREATE_IRQCHIP:
	case GZVM_IRQ_LINE_STATUS:
	case GZVM_REGISTER_COALESCED_MMIO:
	case GZVM_UNREGISTER_COALESCED_MMIO:
	case GZVM_ASSIGN_PCI_DEVICE:
	case GZVM_SET_GSI_ROUTING:
	case GZVM_ASSIGN_DEV_IRQ:
	case GZVM_DEASSIGN_PCI_DEVICE:
	case GZVM_DEASSIGN_DEV_IRQ:
	case GZVM_ARM_MTE_COPY_TAGS:
	case GZVM_ENABLE_CAP:
	case GZVM_MEMORY_ENCRYPT_OP:
	case GZVM_MEMORY_ENCRYPT_REG_REGION:
	case GZVM_MEMORY_ENCRYPT_UNREG_REGION:
	case GZVM_CLEAR_DIRTY_LOG:
		/* TODO */
		return -EOPNOTSUPP;

	default:
		GZVM_ERR("%s invalid ioctl=0x%x\n", __func__, ioctl);
		ret = -EINVAL;
	}
out:
	return ret;
}

/**
 * @brief Destroy all vcpus
 *
 * @param gzvm vm struct that owns the vcpus
 */
static void gzvm_destroy_vcpus(struct gzvm *gzvm)
{
	int i;

	for (i = 0; i < GZVM_MAX_VCPUS; i++) {
		gzvm_destroy_vcpu(gzvm->vcpus[i]);
		gzvm->vcpus[i] = NULL;
	}
}

static int gzvm_destroy_vm_hyp(gzvm_id_t vm_id)
{
	struct arm_smccc_res res;

	gzvm_hypcall_wrapper(MT_HVC_GZVM_DESTROY_VM, vm_id, 0, 0, 0, 0, 0, 0,
			     &res);

	return 0;
}

static void gzvm_destroy_vm(struct gzvm *gzvm)
{
	GZVM_INFO("VM-%u is going to be destroyed\n", gzvm->vm_id);
	gzvm_destroy_vcpus(gzvm);
	gzvm_destroy_vm_hyp(gzvm->vm_id);
	mutex_lock(&gzvm_list_lock);
	list_del(&gzvm->vm_list);
	mutex_unlock(&gzvm_list_lock);

	kfree(gzvm);
}

static int gzvm_vm_release(struct inode *inode, struct file *filp)
{
	struct gzvm *gzvm = filp->private_data;

	gzvm_destroy_vm(gzvm);
	return 0;
}

static const struct file_operations gzvm_vm_fops = {
	.release        = gzvm_vm_release,
	.unlocked_ioctl = gzvm_vm_ioctl,
	.llseek		= noop_llseek,
};

static int gzvm_create_vm_hyp(void)
{
	struct arm_smccc_res res;

	gzvm_hypcall_wrapper(MT_HVC_GZVM_CREATE_VM, 0, 0, 0, 0, 0, 0, 0, &res);

	if (res.a0 != 0)
		return -EFAULT;
	return res.a1;
}

static struct gzvm *gzvm_create_vm(unsigned long vm_type)
{
	int ret;
	struct gzvm *gzvm;

	gzvm = kzalloc(sizeof(struct gzvm), GFP_KERNEL);
	if (IS_ERR(gzvm))
		return ERR_PTR(-ENOMEM);

	ret = gzvm_create_vm_hyp();
	if (ret < 0)
		goto err;

	gzvm->vm_id = ret;
	gzvm->mm = current->mm;
	mutex_init(&gzvm->lock);
	INIT_LIST_HEAD(&gzvm->devices);
	mutex_init(&gzvm->irq_lock);
	ret = gzvm_init_eventfd(gzvm);
	if (ret) {
		GZVM_ERR("Failed to initialize eventfd\n");
		goto err;
	}
	GZVM_INFO("VM-%u is created\n", gzvm->vm_id);

	mutex_lock(&gzvm_list_lock);
	list_add(&gzvm->vm_list, &gzvm_list);
	mutex_unlock(&gzvm_list_lock);

	return gzvm;

err:
	kfree(gzvm);
	return ERR_PTR(ret);

}

/**
 * @brief create vm fd
 *
 * @param vm_type
 * @return int fd of vm, negative if error
 */
int gzvm_dev_ioctl_create_vm(unsigned long vm_type)
{
	struct gzvm *gzvm;
	int ret;

	gzvm = gzvm_create_vm(vm_type);
	if (IS_ERR(gzvm)) {
		ret = PTR_ERR(gzvm);
		goto error;
	}

	ret = anon_inode_getfd("gzvm-vm", &gzvm_vm_fops, gzvm, O_RDWR | O_CLOEXEC);
	if (ret < 0)
		goto error;

error:
	return ret;
}
