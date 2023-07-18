// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2023 MediaTek Inc.
 */

#include <linux/anon_inodes.h>
#include <linux/clocksource.h>
#include <linux/file.h>
#include <linux/kdev_t.h>
#include <linux/miscdevice.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/gzvm_drv.h>
#include "gzvm_common.h"

static DEFINE_MUTEX(gzvm_list_lock);
static LIST_HEAD(gzvm_list);

struct timecycle clock_scale_factor;

u64 gzvm_gfn_to_hva_memslot(struct gzvm_memslot *memslot, u64 gfn)
{
	u64 offset = gfn - memslot->base_gfn;

	return memslot->userspace_addr + offset * PAGE_SIZE;
}

/**
 * gzvm_find_memslot() - Find memslot containing this @gpa
 *
 * Return:
 * * >=0		- Index of memslot
 * * -EFAULT		- Not found
 */
int gzvm_find_memslot(struct gzvm *vm, u64 gfn)
{
	int i;

	for (i = 0; i < GZVM_MAX_MEM_REGION; i++) {
		if (vm->memslot[i].npages == 0)
			continue;

		if (gfn >= vm->memslot[i].base_gfn &&
		    gfn < vm->memslot[i].base_gfn + vm->memslot[i].npages)
			return i;
	}

	return -EFAULT;
}

/**
 * fill_constituents() - Populate pa to buffer until full
 * @consti: Pointer to struct mem_region_addr_range.
 * @consti_cnt: Constituent count.
 * @max_nr_consti: Maximum number of constituent count.
 * @gfn: Guest frame number.
 * @total_pages: Total page numbers.
 * @slot: Pointer to struct gzvm_memslot.
 *
 * Return: how many pages we've fill in, negative if error
 */
static int fill_constituents(struct mem_region_addr_range *consti,
			     int *consti_cnt, int max_nr_consti, u64 gfn,
			     u32 total_pages, struct gzvm_memslot *slot)
{
	u64 pfn, prev_pfn, gfn_end;
	int nr_pages = 1;
	int i = 0;

	if (unlikely(total_pages == 0))
		return -EINVAL;
	gfn_end = gfn + total_pages;

	/* entry 0 */
	if (gzvm_gfn_to_pfn_memslot(slot, gfn, &pfn) != 0)
		return -EFAULT;
	consti[0].address = PFN_PHYS(pfn);
	consti[0].pg_cnt = 1;
	gfn++;
	prev_pfn = pfn;

	while (i < max_nr_consti && gfn < gfn_end) {
		if (gzvm_gfn_to_pfn_memslot(slot, gfn, &pfn) != 0)
			return -EFAULT;
		if (pfn == (prev_pfn + 1)) {
			consti[i].pg_cnt++;
		} else {
			i++;
			if (i >= max_nr_consti)
				break;
			consti[i].address = PFN_PHYS(pfn);
			consti[i].pg_cnt = 1;
		}
		prev_pfn = pfn;
		gfn++;
		nr_pages++;
	}
	if (i != max_nr_consti)
		i++;
	*consti_cnt = i;

	return nr_pages;
}

/* register_memslot_addr_range() - Register memory region to GZ */
static int
register_memslot_addr_range(struct gzvm *gzvm, struct gzvm_memslot *memslot)
{
	struct gzvm_memory_region_ranges *region;
	u32 buf_size;
	int max_nr_consti, remain_pages;
	u64 gfn, gfn_end;

	buf_size = PAGE_SIZE * 2;
	region = alloc_pages_exact(buf_size, GFP_KERNEL);
	if (!region)
		return -ENOMEM;
	max_nr_consti = (buf_size - sizeof(*region)) /
			sizeof(struct mem_region_addr_range);

	region->slot = memslot->slot_id;
	remain_pages = memslot->npages;
	gfn = memslot->base_gfn;
	gfn_end = gfn + remain_pages;
	while (gfn < gfn_end) {
		int nr_pages;

		nr_pages = fill_constituents(region->constituents,
					     &region->constituent_cnt,
					     max_nr_consti, gfn,
					     remain_pages, memslot);
		if (nr_pages < 0) {
			pr_info("Failed to fill constituents\n");
			free_pages_exact(region, buf_size);
			return nr_pages;
		}
		region->gpa = PFN_PHYS(gfn);
		region->total_pages = nr_pages;

		remain_pages -= nr_pages;
		gfn += nr_pages;

		if (gzvm_arch_set_memregion(gzvm->vm_id, buf_size,
					    virt_to_phys(region))) {
			dev_info(&gzvm_debug_dev->dev,
				"Failed to register memregion to hypervisor\n");
			free_pages_exact(region, buf_size);
			return -EFAULT;
		}
	}
	free_pages_exact(region, buf_size);
	return 0;
}

/**
 * gzvm_vm_ioctl_set_memory_region() - Set memory region of guest
 * @gzvm: Pointer to struct gzvm.
 * @mem: Input memory region from user.
 *
 * Return:
 * * -EXIO		- memslot is out-of-range
 * * -EFAULT		- Cannot find corresponding vma
 * * -EINVAL		- region size and vma size does not match
 */
static int
gzvm_vm_ioctl_set_memory_region(struct gzvm *gzvm,
				struct gzvm_userspace_memory_region *mem)
{
	int ret;
	struct vm_area_struct *vma;
	struct gzvm_memslot *memslot;
	unsigned long size;
	__u32 slot;

	slot = mem->slot;
	if (slot >= GZVM_MAX_MEM_REGION)
		return -ENXIO;
	memslot = &gzvm->memslot[slot];

	vma = vma_lookup(gzvm->mm, mem->userspace_addr);
	if (!vma)
		return -EFAULT;

	size = vma->vm_end - vma->vm_start;
	if (size != mem->memory_size)
		return -EINVAL;

	memslot->base_gfn = __phys_to_pfn(mem->guest_phys_addr);
	memslot->npages = size >> PAGE_SHIFT;
	memslot->userspace_addr = mem->userspace_addr;
	memslot->vma = vma;
	memslot->flags = mem->flags;
	memslot->slot_id = mem->slot;

	ret = gzvm_arch_memregion_purpose(gzvm, mem);
	if (ret) {
		dev_info(&gzvm_debug_dev->dev,
			"Failed to config memory region for the specified purpose\n");
		return -EFAULT;
	}
	return register_memslot_addr_range(gzvm, memslot);
}

static int gzvm_vm_ioctl_irq_line(struct gzvm *gzvm,
				  struct gzvm_irq_level *irq_level)
{
	u32 irq = irq_level->irq;
	u32 irq_type, vcpu_idx, vcpu2_idx, irq_num;
	bool level = irq_level->level;

	irq_type = FIELD_GET(GZVM_IRQ_LINE_TYPE, irq);
	vcpu_idx = FIELD_GET(GZVM_IRQ_LINE_VCPU, irq);
	vcpu2_idx = FIELD_GET(GZVM_IRQ_LINE_VCPU2, irq) * (GZVM_IRQ_VCPU_MASK + 1);
	irq_num = FIELD_GET(GZVM_IRQ_LINE_NUM, irq);

	return gzvm_irqchip_inject_irq(gzvm, vcpu_idx + vcpu2_idx, irq_type, irq_num,
				       level);
}

static int gzvm_vm_ioctl_create_device(struct gzvm *gzvm, void __user *argp)
{
	struct gzvm_create_device *gzvm_dev;
	void *dev_data = NULL;
	int ret;

	gzvm_dev = (struct gzvm_create_device *)alloc_pages_exact(PAGE_SIZE,
								  GFP_KERNEL);
	if (!gzvm_dev)
		return -ENOMEM;
	if (copy_from_user(gzvm_dev, argp, sizeof(*gzvm_dev))) {
		ret = -EFAULT;
		goto err_free_dev;
	}

	if (gzvm_dev->attr_addr != 0 && gzvm_dev->attr_size != 0) {
		size_t attr_size = gzvm_dev->attr_size;
		void __user *attr_addr = (void __user *)gzvm_dev->attr_addr;

		/* Size of device specific data should not be over a page. */
		if (attr_size > PAGE_SIZE)
			return -EINVAL;

		dev_data = alloc_pages_exact(attr_size, GFP_KERNEL);
		if (!dev_data) {
			ret = -ENOMEM;
			goto err_free_dev;
		}

		if (copy_from_user(dev_data, attr_addr, attr_size)) {
			ret = -EFAULT;
			goto err_free_dev_data;
		}
		gzvm_dev->attr_addr = virt_to_phys(dev_data);
	}

	ret = gzvm_arch_create_device(gzvm->vm_id, gzvm_dev);
err_free_dev_data:
	if (dev_data)
		free_pages_exact(dev_data, 0);
err_free_dev:
	free_pages_exact(gzvm_dev, 0);
	return ret;
}

static int gzvm_vm_ioctl_enable_cap(struct gzvm *gzvm,
				    struct gzvm_enable_cap *cap,
				    void __user *argp)
{
	return gzvm_vm_ioctl_arch_enable_cap(gzvm, cap, argp);
}

/* gzvm_vm_ioctl() - Ioctl handler of VM FD */
static long gzvm_vm_ioctl(struct file *filp, unsigned int ioctl,
			  unsigned long arg)
{
	long ret = -ENOTTY;
	void __user *argp = (void __user *)arg;
	struct gzvm *gzvm = filp->private_data;

	switch (ioctl) {
	case GZVM_CHECK_EXTENSION: {
		ret = gzvm_dev_ioctl_check_extension(gzvm, arg);
		break;
	}
	case GZVM_CREATE_VCPU: {
		ret = gzvm_vm_ioctl_create_vcpu(gzvm, arg);
		break;
	}
	case GZVM_SET_USER_MEMORY_REGION: {
		struct gzvm_userspace_memory_region userspace_mem;

		if (copy_from_user(&userspace_mem, argp, sizeof(userspace_mem))) {
			ret = -EFAULT;
			goto out;
		}
		ret = gzvm_vm_ioctl_set_memory_region(gzvm, &userspace_mem);
		break;
	}
	case GZVM_IRQ_LINE: {
		struct gzvm_irq_level irq_event;

		if (copy_from_user(&irq_event, argp, sizeof(irq_event))) {
			ret = -EFAULT;
			goto out;
		}
		ret = gzvm_vm_ioctl_irq_line(gzvm, &irq_event);
		break;
	}
	case GZVM_CREATE_DEVICE: {
		ret = gzvm_vm_ioctl_create_device(gzvm, argp);
		break;
	}
	case GZVM_IRQFD: {
		struct gzvm_irqfd data;

		if (copy_from_user(&data, argp, sizeof(data))) {
			ret = -EFAULT;
			goto out;
		}
		ret = gzvm_irqfd(gzvm, &data);
		break;
	}
	case GZVM_IOEVENTFD: {
		struct gzvm_ioeventfd data;

		if (copy_from_user(&data, argp, sizeof(data))) {
			ret = -EFAULT;
			goto out;
		}
		ret = gzvm_ioeventfd(gzvm, &data);
		break;
	}
	case GZVM_ENABLE_CAP: {
		struct gzvm_enable_cap cap;

		if (copy_from_user(&cap, argp, sizeof(cap))) {
			ret = -EFAULT;
			goto out;
		}
		ret = gzvm_vm_ioctl_enable_cap(gzvm, &cap, argp);
		break;
	}
	case GZVM_SET_DTB_CONFIG: {
		struct gzvm_dtb_config cfg;

		if (copy_from_user(&cfg, argp, sizeof(cfg))) {
			ret = -EFAULT;
			goto out;
		}
		ret = gzvm_arch_set_dtb_config(gzvm, &cfg);
		break;
	}
	default:
		ret = -ENOTTY;
	}
out:
	return ret;
}

static void gzvm_destroy_ppage(struct gzvm *gzvm)
{
	struct gzvm_pinned_page *ppage;
	struct rb_node *node;

	node = rb_first(&gzvm->pinned_pages);
	while (node) {
		ppage = rb_entry(node, struct gzvm_pinned_page, node);
		unpin_user_pages_dirty_lock(&ppage->page, 1, true);
		node = rb_next(node);
		rb_erase(&ppage->node, &gzvm->pinned_pages);
		kfree(ppage);
	}
}

static void gzvm_destroy_vm(struct gzvm *gzvm)
{
	dev_info(&gzvm_debug_dev->dev,
		 "VM-%u is going to be destroyed\n", gzvm->vm_id);

	mutex_lock(&gzvm->lock);

	gzvm_vm_irqfd_release(gzvm);
	gzvm_destroy_vcpus(gzvm);
	gzvm_arch_destroy_vm(gzvm->vm_id);

	mutex_lock(&gzvm_list_lock);
	list_del(&gzvm->vm_list);
	mutex_unlock(&gzvm_list_lock);

	GZVM_INFO("%s, done exit %lld us", __func__,
		  ktime_us_delta(ktime_get(), gzvm->exit_start_time));

	mutex_unlock(&gzvm->lock);

	gzvm_destroy_ppage(gzvm);

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

static struct gzvm *gzvm_create_vm(unsigned long vm_type)
{
	int ret;
	struct gzvm *gzvm;

	gzvm = kzalloc(sizeof(*gzvm), GFP_KERNEL);
	if (!gzvm)
		return ERR_PTR(-ENOMEM);

	ret = gzvm_arch_create_vm(vm_type);
	if (ret < 0) {
		kfree(gzvm);
		return ERR_PTR(ret);
	}

	gzvm->vm_id = ret;
	gzvm->mm = current->mm;
	mutex_init(&gzvm->lock);
	gzvm->pinned_pages = RB_ROOT;

	ret = gzvm_vm_irqfd_init(gzvm);
	if (ret) {
		dev_info(&gzvm_debug_dev->dev,
			"Failed to initialize irqfd\n");
		kfree(gzvm);
		return ERR_PTR(ret);
	}

	ret = gzvm_init_ioeventfd(gzvm);
	if (ret) {
		dev_info(&gzvm_debug_dev->dev,
			"Failed to initialize ioeventfd\n");
		kfree(gzvm);
		return ERR_PTR(ret);
	}

	/* clock_scale_factor init mult shift */
	clocks_calc_mult_shift(&clock_scale_factor.mult,
			       &clock_scale_factor.shift,
			       arch_timer_get_cntfrq(),
			       NSEC_PER_SEC,
			       10);

	mutex_lock(&gzvm_list_lock);
	list_add(&gzvm->vm_list, &gzvm_list);
	mutex_unlock(&gzvm_list_lock);

	GZVM_INFO("VM-%u is created\n", gzvm->vm_id);

	return gzvm;
}

/**
 * gzvm_dev_ioctl_create_vm - Create vm fd
 * @vm_type: VM type. Only supports Linux VM now.
 *
 * Return: fd of vm, negative if error
 */
int gzvm_dev_ioctl_create_vm(unsigned long vm_type)
{
	struct gzvm *gzvm;

	gzvm = gzvm_create_vm(vm_type);
	if (IS_ERR(gzvm))
		return PTR_ERR(gzvm);

	return anon_inode_getfd("gzvm-vm", &gzvm_vm_fops, gzvm,
			       O_RDWR | O_CLOEXEC);
}

void destroy_all_vm(void)
{
	struct gzvm *gzvm, *tmp;

	mutex_lock(&gzvm_list_lock);
	if (list_empty(&gzvm_list))
		goto out;

	list_for_each_entry_safe(gzvm, tmp, &gzvm_list, vm_list)
		gzvm_destroy_vm(gzvm);

out:
	mutex_unlock(&gzvm_list_lock);
}
