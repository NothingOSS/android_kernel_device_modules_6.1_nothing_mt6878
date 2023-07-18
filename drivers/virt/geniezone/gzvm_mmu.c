// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2023 MediaTek Inc.
 */

#include <linux/gzvm_drv.h>

/**
 * hva_to_pa_fast() - converts hva to pa in generic fast way
 * @hva: Host virtual address.
 *
 * Return: 0 if translation error
 */
u64 hva_to_pa_fast(u64 hva)
{
	struct page *page[1];

	u64 pfn;

	if (get_user_page_fast_only(hva, 0, page)) {
		pfn = page_to_phys(page[0]);
		put_page((struct page *)page);
		return pfn;
	} else {
		return 0;
	}
}

/**
 * hva_to_pa_slow() - note that this function may sleep
 * @hva: Host virtual address.
 *
 * Return: 0 if translation error
 */
u64 hva_to_pa_slow(u64 hva)
{
	struct page *page = NULL;
	u64 pfn = 0;
	int npages;

	npages = get_user_pages_unlocked(hva, 1, &page, 0);
	if (npages != 1)
		return 0;

	if (page) {
		pfn = page_to_phys(page);
		put_page(page);
	}

	return pfn;
}

static u64 __gzvm_gfn_to_pfn_memslot(struct gzvm_memslot *memslot, u64 gfn)
{
	u64 hva, pa;

	hva = gzvm_gfn_to_hva_memslot(memslot, gfn);

	pa = hva_to_pa_arch(hva);
	if (pa != 0)
		return PHYS_PFN(pa);

	pa = hva_to_pa_fast(hva);
	if (pa)
		return PHYS_PFN(pa);

	pa = hva_to_pa_slow(hva);
	if (pa)
		return PHYS_PFN(pa);

	return 0;
}

/**
 * gzvm_gfn_to_pfn_memslot() - Translate gfn (guest ipa) to pfn (host pa),
 *			       result is in @pfn
 * @memslot: Pointer to struct gzvm_memslot.
 * @gfn: Guest frame number.
 * @pfn: Host page frame number.
 *
 * Return:
 * * 0			- Succeed
 * * -EFAULT		- Failed to convert
 */
int gzvm_gfn_to_pfn_memslot(struct gzvm_memslot *memslot, u64 gfn, u64 *pfn)
{
	u64 __pfn;

	if (!memslot)
		return -EFAULT;

	__pfn = __gzvm_gfn_to_pfn_memslot(memslot, gfn);
	if (__pfn == 0) {
		*pfn = 0;
		return -EFAULT;
	}

	*pfn = __pfn;

	return 0;
}

static int cmp_ppages(struct rb_node *node, const struct rb_node *parent)
{
	struct gzvm_pinned_page *a = container_of(node, struct gzvm_pinned_page, node);
	struct gzvm_pinned_page *b = container_of(parent, struct gzvm_pinned_page, node);

	if (a->ipa < b->ipa)
		return -1;
	if (a->ipa > b->ipa)
		return 1;
	return 0;
}

static int rb_ppage_cmp(const void *key, const struct rb_node *node)
{
	struct gzvm_pinned_page *p = container_of(node, struct gzvm_pinned_page, node);
	phys_addr_t ipa = (phys_addr_t)key;

	return (ipa < p->ipa) ? -1 : (ipa > p->ipa);
}

static int gzvm_insert_ppage(struct gzvm *vm, struct gzvm_pinned_page *ppage)
{
	if (rb_find_add(&ppage->node, &vm->pinned_pages, cmp_ppages))
		return -EEXIST;
	return 0;
}

static int pin_one_page(unsigned long hva, struct page **page)
{
	struct mm_struct *mm = current->mm;
	unsigned int flags = FOLL_HWPOISON | FOLL_LONGTERM | FOLL_WRITE;

	mmap_read_lock(mm);
	pin_user_pages(hva, 1, flags, page, NULL);
	mmap_read_unlock(mm);

	return 0;
}

/**
 * gzvm_handle_relinquish() - Handle memory relinquish request from hypervisor
 *
 * @vcpu: Pointer to struct gzvm_vcpu_run in userspace
 * @ipa: Start address(gpa) of a reclaimed page
 */
int gzvm_handle_relinquish(struct gzvm_vcpu *vcpu, phys_addr_t ipa)
{
	struct gzvm_pinned_page *ppage;
	struct rb_node *node;
	struct gzvm *vm = vcpu->gzvm;

	node = rb_find((void *)ipa, &vm->pinned_pages,
		rb_ppage_cmp);

	if(node)
		rb_erase(node, &vm->pinned_pages);
	else
		return 0;

	ppage = container_of(node, struct gzvm_pinned_page, node);
	unpin_user_pages_dirty_lock(&ppage->page, 1, true);
	kfree(ppage);
	return 0;
}

/**
 * gzvm_handle_page_fault() - Handle guest page fault, find corresponding page
 * for the faulting gpa
 * @vcpu: Pointer to struct gzvm_vcpu_run in userspace
 */
int gzvm_handle_page_fault(struct gzvm_vcpu *vcpu)
{
	struct gzvm *vm = vcpu->gzvm;
	int memslot_id;
	u64 pfn, gfn;
	unsigned long hva;
	struct gzvm_pinned_page *ppage = NULL;
	struct page *page = NULL;
	int ret;

	gfn = PHYS_PFN(vcpu->run->exception.fault_gpa);
	memslot_id = gzvm_find_memslot(vm, gfn);
	if (unlikely(memslot_id < 0))
		return -EFAULT;

	ret = gzvm_gfn_to_pfn_memslot(&vm->memslot[memslot_id], gfn, &pfn);
	if (unlikely(ret))
		return -EFAULT;

	ret = gzvm_arch_map_guest(vm->vm_id, memslot_id, pfn, gfn, 1);
	if (unlikely(ret))
		return -EFAULT;

	hva = gzvm_gfn_to_hva_memslot(&vm->memslot[memslot_id], gfn);
	pin_one_page(hva, &page);

	if (!page)
		return -EFAULT;

	ppage = kmalloc(sizeof(*ppage), GFP_KERNEL_ACCOUNT);
	if (!ppage)
		return -ENOMEM;

	ppage->page = page;
	ppage->ipa = vcpu->run->exception.fault_gpa;
	gzvm_insert_ppage(vm, ppage);

	return 0;
}
