// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2022 MediaTek Inc.
 */

#include <linux/eventfd.h>
#include <linux/file.h>
#include <linux/kdev_t.h>
#include <linux/kvm.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/version.h>

#include "gzvm.h"

struct gzvm_ioevent {
	struct list_head list;
	__u64 addr;
	__u32 len;
	struct eventfd_ctx  *evt_ctx;
	__u64 datamatch;
	bool wildcard;
};

 /* assumes kvm->slots_lock held */
static bool
ioeventfd_check_collision(struct gzvm *gzvm, struct gzvm_ioevent *p)
{
	struct gzvm_ioevent *_p;

	list_for_each_entry(_p, &gzvm->ioevents, list)
		if (_p->addr == p->addr &&
		    (!_p->len || !p->len ||
		     (_p->len == p->len &&
		      (_p->wildcard || p->wildcard ||
		       _p->datamatch == p->datamatch))))
			return true;

	return false;
}

static void gzvm_ioevent_release(struct gzvm_ioevent *p)
{
	eventfd_ctx_put(p->evt_ctx);
	list_del(&p->list);
	kfree(p);
}

static bool
gzvm_ioevent_in_range(struct gzvm_ioevent *p, gpa_t addr, int len, const void *val)
{
	u64 _val;

	if (addr != p->addr)
		/* address must be precise for a hit */
		return false;

	if (!p->len)
		/* length = 0 means only look at the address, so always a hit */
		return true;

	if (len != p->len)
		/* address-range must be precise for a hit */
		return false;

	if (p->wildcard)
		/* all else equal, wildcard is always a hit */
		return true;

	/* otherwise, we have to actually compare the data */

	WARN_ON_ONCE(!IS_ALIGNED((unsigned long)val, len));

	switch (len) {
	case 1:
		_val = *(u8 *)val;
		break;
	case 2:
		_val = *(u16 *)val;
		break;
	case 4:
		_val = *(u32 *)val;
		break;
	case 8:
		_val = *(u64 *)val;
		break;
	default:
		return false;
	}

	return _val == p->datamatch;
}

static int gzvm_deassign_ioeventfd(struct gzvm *gzvm, struct kvm_ioeventfd *args)
{
	struct gzvm_ioevent *p, *tmp;
	struct eventfd_ctx *evt_ctx;
	int ret = -ENOENT;
	bool wildcard;

	evt_ctx = eventfd_ctx_fdget(args->fd);
	if (IS_ERR(evt_ctx))
		return PTR_ERR(evt_ctx);

	wildcard = !(args->flags & KVM_IOEVENTFD_FLAG_DATAMATCH);

	mutex_lock(&gzvm->lock);

	list_for_each_entry_safe(p, tmp, &gzvm->ioevents, list) {
		if (p->evt_ctx != evt_ctx  ||
		    p->addr != args->addr  ||
		    p->len != args->len ||
		    p->wildcard != wildcard)
			continue;

		if (!p->wildcard && p->datamatch != args->datamatch)
			continue;

		gzvm_ioevent_release(p);
		ret = 0;
		break;
	}

	mutex_unlock(&gzvm->lock);

	/* got in the front of this function */
	eventfd_ctx_put(evt_ctx);

	return ret;
}

static int gzvm_assign_ioeventfd(struct gzvm *gzvm, struct kvm_ioeventfd *args)
{
	struct eventfd_ctx *evt_ctx;
	struct gzvm_ioevent *evt;
	int ret;

	evt_ctx = eventfd_ctx_fdget(args->fd);
	if (IS_ERR(evt_ctx))
		return PTR_ERR(evt_ctx);

	evt = kmalloc(sizeof(*evt), GFP_KERNEL);
	*evt = (struct gzvm_ioevent) {
		.addr = args->addr,
		.len = args->len,
		.evt_ctx = evt_ctx,
	};
	if (args->flags & KVM_IOEVENTFD_FLAG_DATAMATCH) {
		evt->datamatch = args->datamatch;
		evt->wildcard = false;
	} else {
		evt->wildcard = true;
	}

	if (ioeventfd_check_collision(gzvm, evt)) {
		ret = -EEXIST;
		goto err_free;
	}

	mutex_lock(&gzvm->lock);
	list_add_tail(&evt->list, &gzvm->ioevents);
	mutex_unlock(&gzvm->lock);

	return 0;

err_free:
	kfree(evt);
	eventfd_ctx_put(evt_ctx);
	return ret;
}

/**
 * @brief Check user arguments is valid
 *
 * @param args
 * @retval true valid arguments
 * @retval false invalid arguments
 */
static bool gzvm_ioeventfd_check_valid(struct kvm_ioeventfd *args)
{
	/* must be natural-word sized, or 0 to ignore length */
	switch (args->len) {
	case 0:
	case 1:
	case 2:
	case 4:
	case 8:
		break;
	default:
		return false;
	}

	/* check for range overflow */
	if (args->addr + args->len < args->addr)
		return false;

	/* check for extra flags that we don't understand */
	if (args->flags & ~KVM_IOEVENTFD_VALID_FLAG_MASK)
		return false;

	/* ioeventfd with no length can't be combined with DATAMATCH */
	if (!args->len && (args->flags & KVM_IOEVENTFD_FLAG_DATAMATCH))
		return false;

	/* gzvm does not support pio bus ioeventfd */
	if (args->flags & KVM_IOEVENTFD_FLAG_PIO)
		return -EINVAL;

	return true;
}

/**
 * @brief KVM_IOEVENTFD, register ioevent to ioevent list
 *
 * @param gzvm
 * @param kvm_ioevent
 * @return long
 */
int gzvm_ioeventfd(struct gzvm *gzvm, struct kvm_ioeventfd *args)
{
	pr_info("%s ioaddr=0x%llx, iolen=%x, eventfd=%d, flags=%x\n", __func__,
		args->addr, args->len, args->fd, args->flags);

	if (gzvm_ioeventfd_check_valid(args) == false)
		return -EINVAL;

	if (args->flags & KVM_IOEVENTFD_FLAG_DEASSIGN)
		return gzvm_deassign_ioeventfd(gzvm, args);
	return gzvm_assign_ioeventfd(gzvm, args);
}

/**
 * @brief Travers this vm's registered ioeventfd to see if need notifying it
 * @retval true if this io is already sent to ioeventfd's listner
 * @retval false if we cannot find any ioeventfd registering this mmio write
 */
bool gzvm_ioevent_write(struct gzvm_vcpu *vcpu, __u64 addr, int len,
			const void *val)
{
	/* TODO: do we need to lock gzvm->lock ? */
	struct gzvm_ioevent *e;

	list_for_each_entry(e, &vcpu->gzvm->ioevents, list) {
		if (gzvm_ioevent_in_range(e, addr, len, val)) {
			eventfd_signal(e->evt_ctx, 92);
			return true;
		}
	}
	return false;
}
