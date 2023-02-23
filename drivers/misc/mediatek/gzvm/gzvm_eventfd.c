// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2022 MediaTek Inc.
 */

#include <linux/eventfd.h>
#include <linux/file.h>
#include <linux/kvm.h>
#include <linux/workqueue.h>
#include <linux/syscalls.h>
#include <linux/wait.h>
#include <linux/poll.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/version.h>
#include <kvm/arm_vgic.h>

#include "gzvm.h"

struct gzvm_ioevent {
	struct list_head list;
	__u64 addr;
	__u32 len;
	struct eventfd_ctx  *evt_ctx;
	__u64 datamatch;
	bool wildcard;
};

struct gzvm_irq_ack_notifier {
	struct hlist_node link;
	unsigned int gsi;
	void (*irq_acked)(struct gzvm_irq_ack_notifier *ian);
};

/*
 * Resampling irqfds are a special variety of irqfds used to emulate
 * level triggered interrupts.  The interrupt is asserted on eventfd
 * trigger.  On acknowledgment through the irq ack notifier, the
 * interrupt is de-asserted and userspace is notified through the
 * resamplefd.  All resamplers on the same gsi are de-asserted
 * together, so we don't need to track the state of each individual
 * user.  We can also therefore share the same irq source ID.
 */
struct gzvm_kernel_irqfd_resampler {
	struct gzvm *gzvm;
	/*
	 * List of resampling struct _irqfd objects sharing this gsi.
	 * RCU list modified under gzvm->irqfds.resampler_lock
	 */
	struct list_head list;
	struct gzvm_irq_ack_notifier notifier;
	/*
	 * Entry in list of gzvm->irqfd.resampler_list.  Use for sharing
	 * resamplers among irqfds on the same gsi.
	 * Accessed and modified under gzvm->irqfds.resampler_lock
	 */
	struct list_head link;
};

struct gzvm_kernel_irqfd {
	/* Used for MSI fast-path */
	struct gzvm *gzvm;
	wait_queue_entry_t wait;
	/* Used for level IRQ fast-path */
	/* Update side is protected by irqfds.lock */
	struct kvm_kernel_irq_routing_entry irq_entry;
	seqcount_spinlock_t irq_entry_sc;
	/* Used for level IRQ fast-path */
	int gsi;
	struct work_struct inject;
	/* The resampler used by this irqfd (resampler-only) */
	struct gzvm_kernel_irqfd_resampler *resampler;
	/* Eventfd notified on resample (resampler-only) */
	struct eventfd_ctx *resamplefd;
	/* Entry in list of irqfds for a resampler (resampler-only) */
	struct list_head resampler_link;
	/* Used for setup/shutdown */
	struct eventfd_ctx *eventfd;
	struct list_head list;
	poll_table pt;
	struct work_struct shutdown;
};

static struct workqueue_struct *irqfd_cleanup_wq;

/**
 * @brief irqfd to inject virtual interrupt
 *
 * @param gzvm
 * @param irq_source_id
 * @param irq This is spi interrupt number (starts from 0 instead of 32)
 * @param level
 * @param line_status
 */
static void irqfd_set_irq(struct gzvm *gzvm, int irq_source_id, u32 irq,
			       int level, bool line_status)
{
	if (level)
		gzvm_vgic_inject_irq(gzvm, 0, KVM_ARM_IRQ_TYPE_SPI,
				     irq + VGIC_NR_PRIVATE_IRQS, level);
}

static void irqfd_inject(struct work_struct *work)
{
	struct gzvm_kernel_irqfd *irqfd =
		container_of(work, struct gzvm_kernel_irqfd, inject);
	struct gzvm *gzvm = irqfd->gzvm;

	if (!irqfd->resampler) {
		irqfd_set_irq(gzvm, KVM_USERSPACE_IRQ_SOURCE_ID, irqfd->gsi, 1,
			      false);
		irqfd_set_irq(gzvm, KVM_USERSPACE_IRQ_SOURCE_ID, irqfd->gsi, 0,
			      false);
	} else
		irqfd_set_irq(gzvm, KVM_IRQFD_RESAMPLE_IRQ_SOURCE_ID,
			      irqfd->gsi, 1, false);
}

/*
 * Since resampler irqfds share an IRQ source ID, we de-assert once
 * then notify all of the resampler irqfds using this GSI.  We can't
 * do multiple de-asserts or we risk racing with incoming re-asserts.
 */
static void
irqfd_resampler_ack(struct gzvm_irq_ack_notifier *ian)
{
	struct gzvm_kernel_irqfd_resampler *resampler;
	struct gzvm *gzvm;
	struct gzvm_kernel_irqfd *irqfd;
	int idx;

	resampler = container_of(ian,
			struct gzvm_kernel_irqfd_resampler, notifier);
	gzvm = resampler->gzvm;

	GZVM_DEBUG("%s gsi=%u(%u)\n", __func__, resampler->notifier.gsi,
		   resampler->notifier.gsi + VGIC_NR_PRIVATE_IRQS);

	irqfd_set_irq(gzvm, KVM_IRQFD_RESAMPLE_IRQ_SOURCE_ID,
		      resampler->notifier.gsi, 0, false);

	idx = srcu_read_lock(&gzvm->irq_srcu);

	list_for_each_entry_srcu(irqfd, &resampler->list, resampler_link,
	    srcu_read_lock_held(&gzvm->irq_srcu)) {
		GZVM_DEBUG("eventfd_signal resamplefd\n");
		eventfd_signal(irqfd->resamplefd, 1);
	}

	srcu_read_unlock(&gzvm->irq_srcu, idx);
}

static void gzvm_register_irq_ack_notifier(struct gzvm *gzvm,
					   struct gzvm_irq_ack_notifier *ian)
{
	mutex_lock(&gzvm->irq_lock);
	hlist_add_head_rcu(&ian->link, &gzvm->irq_ack_notifier_list);
	mutex_unlock(&gzvm->irq_lock);
	/* for x86 */
	// gzvm_arch_post_irq_ack_notifier_list_update(gzvm);
}

static void gzvm_unregister_irq_ack_notifier(struct gzvm *gzvm,
					     struct gzvm_irq_ack_notifier *ian)
{
	mutex_lock(&gzvm->irq_lock);
	hlist_del_init_rcu(&ian->link);
	mutex_unlock(&gzvm->irq_lock);
	synchronize_srcu(&gzvm->irq_srcu);
	/* for x86 */
	// gzvm_arch_post_irq_ack_notifier_list_update(gzvm);
}

static void
irqfd_resampler_shutdown(struct gzvm_kernel_irqfd *irqfd)
{
	struct gzvm_kernel_irqfd_resampler *resampler = irqfd->resampler;
	struct gzvm *gzvm = resampler->gzvm;

	mutex_lock(&gzvm->irqfds.resampler_lock);

	list_del_rcu(&irqfd->resampler_link);
	synchronize_srcu(&gzvm->irq_srcu);

	if (list_empty(&resampler->list)) {
		list_del(&resampler->link);
		gzvm_unregister_irq_ack_notifier(gzvm, &resampler->notifier);
		irqfd_set_irq(gzvm, KVM_IRQFD_RESAMPLE_IRQ_SOURCE_ID,
			      resampler->notifier.gsi, 0, false);
		kfree(resampler);
	}

	mutex_unlock(&gzvm->irqfds.resampler_lock);
}

/*
 * Race-free decouple logic (ordering is critical)
 */
static void
irqfd_shutdown(struct work_struct *work)
{
	struct gzvm_kernel_irqfd *irqfd =
		container_of(work, struct gzvm_kernel_irqfd, shutdown);
	struct gzvm *gzvm = irqfd->gzvm;
	u64 cnt;

	/* Make sure irqfd has been initialized in assign path. */
	synchronize_srcu(&gzvm->irq_srcu);

	/*
	 * Synchronize with the wait-queue and unhook ourselves to prevent
	 * further events.
	 */
	eventfd_ctx_remove_wait_queue(irqfd->eventfd, &irqfd->wait, &cnt);

	/*
	 * We know no new events will be scheduled at this point, so block
	 * until all previously outstanding events have completed
	 */
	flush_work(&irqfd->inject);

	if (irqfd->resampler) {
		irqfd_resampler_shutdown(irqfd);
		eventfd_ctx_put(irqfd->resamplefd);
	}

	/*
	 * It is now safe to release the object's resources
	 */
	eventfd_ctx_put(irqfd->eventfd);
	kfree(irqfd);
}


/* assumes gzvm->irqfds.lock is held */
static bool
irqfd_is_active(struct gzvm_kernel_irqfd *irqfd)
{
	return list_empty(&irqfd->list) ? false : true;
}

/*
 * Mark the irqfd as inactive and schedule it for removal
 *
 * assumes gzvm->irqfds.lock is held
 */
static void
irqfd_deactivate(struct gzvm_kernel_irqfd *irqfd)
{
	if (!irqfd_is_active(irqfd))
		return;

	GZVM_DEBUG("%s (gsi=%u(%u))\n", __func__, irqfd->gsi,
		   irqfd->gsi + VGIC_NR_PRIVATE_IRQS);
	list_del_init(&irqfd->list);

	queue_work(irqfd_cleanup_wq, &irqfd->shutdown);
}

/*
 * Called with wqh->lock held and interrupts disabled
 */
static int
irqfd_wakeup(wait_queue_entry_t *wait, unsigned int mode, int sync, void *key)
{
	struct gzvm_kernel_irqfd *irqfd =
		container_of(wait, struct gzvm_kernel_irqfd, wait);
	__poll_t flags = key_to_poll(key);
	// struct kvm_kernel_irq_routing_entry irq;
	// unsigned seq;
	// int idx;
	struct gzvm *gzvm = irqfd->gzvm;
	int ret = 0;

	if (flags & EPOLLIN) {
#ifdef eventfd_ctx_do_read
		/* FIXME: b59e00dd8cda7 drain this eventfd, 5.10 does not have
		 * this function exported, disable it for 5.10 workaround,
		 * looks like not necessary to drain it.
		 */
		u64 cnt;

		eventfd_ctx_do_read(irqfd->eventfd, &cnt);
#endif
		/* gzvm's irq injection is not blocked, don't need workq */
		irqfd_set_irq(gzvm, KVM_USERSPACE_IRQ_SOURCE_ID, irqfd->gsi, 1,
			      false);
		ret = 1;
	}

	if (flags & EPOLLHUP) {
		/* The eventfd is closing, detach from KVM */
		unsigned long iflags;

		GZVM_DEBUG("irqfd (gsi=%u(%u)) is closing\n", irqfd->gsi,
			   irqfd->gsi + VGIC_NR_PRIVATE_IRQS);
		spin_lock_irqsave(&gzvm->irqfds.lock, iflags);

		/*
		 * We must check if someone deactivated the irqfd before
		 * we could acquire the irqfds.lock since the item is
		 * deactivated from the KVM side before it is unhooked from
		 * the wait-queue.  If it is already deactivated, we can
		 * simply return knowing the other side will cleanup for us.
		 * We cannot race against the irqfd going away since the
		 * other side is required to acquire wqh->lock, which we hold
		 */
		if (irqfd_is_active(irqfd))
			irqfd_deactivate(irqfd);

		spin_unlock_irqrestore(&gzvm->irqfds.lock, iflags);
	}

	return ret;
}

static void
irqfd_ptable_queue_proc(struct file *file, wait_queue_head_t *wqh,
			poll_table *pt)
{
	struct gzvm_kernel_irqfd *irqfd =
		container_of(pt, struct gzvm_kernel_irqfd, pt);
#ifdef add_wait_queue_priority
	add_wait_queue_priority(wqh, &irqfd->wait);
#else
	add_wait_queue(wqh, &irqfd->wait);
#endif
}

/* Must be called under irqfds.lock */
static void irqfd_update(struct gzvm *gzvm, struct gzvm_kernel_irqfd *irqfd)
{
	/* TODO: ? */
	// GZVM_ERR("%s %d\n", __func__, __LINE__);
}

static int
gzvm_irqfd_assign(struct gzvm *gzvm, struct kvm_irqfd *args)
{
	struct gzvm_kernel_irqfd *irqfd, *tmp;
	struct fd f;
	struct eventfd_ctx *eventfd = NULL, *resamplefd = NULL;
	int ret;
	__poll_t events;
	int idx;

	/* TODO: check gzvm intc is initialized */

	irqfd = kzalloc(sizeof(*irqfd), GFP_KERNEL_ACCOUNT);
	if (!irqfd)
		return -ENOMEM;

	irqfd->gzvm = gzvm;
	irqfd->gsi = args->gsi;
	INIT_LIST_HEAD(&irqfd->list);
	INIT_WORK(&irqfd->inject, irqfd_inject);
	INIT_WORK(&irqfd->shutdown, irqfd_shutdown);
	seqcount_spinlock_init(&irqfd->irq_entry_sc, &gzvm->irqfds.lock);

	GZVM_DEBUG("%s gsi=%d fd=%d resample_fd=%d\n", __func__, args->gsi,
		  args->fd, args->resamplefd);

	f = fdget(args->fd);
	if (!f.file) {
		ret = -EBADF;
		goto out;
	}

	eventfd = eventfd_ctx_fileget(f.file);
	if (IS_ERR(eventfd)) {
		ret = PTR_ERR(eventfd);
		goto fail;
	}

	irqfd->eventfd = eventfd;

	if (args->flags & KVM_IRQFD_FLAG_RESAMPLE) {
		struct gzvm_kernel_irqfd_resampler *resampler;

		resamplefd = eventfd_ctx_fdget(args->resamplefd);
		if (IS_ERR(resamplefd)) {
			ret = PTR_ERR(resamplefd);
			goto fail;
		}

		irqfd->resamplefd = resamplefd;
		INIT_LIST_HEAD(&irqfd->resampler_link);

		mutex_lock(&gzvm->irqfds.resampler_lock);

		list_for_each_entry(resampler,
				    &gzvm->irqfds.resampler_list, link) {
			if (resampler->notifier.gsi == irqfd->gsi) {
				irqfd->resampler = resampler;
				break;
			}
		}

		if (!irqfd->resampler) {
			resampler = kzalloc(sizeof(*resampler),
					    GFP_KERNEL_ACCOUNT);
			if (!resampler) {
				ret = -ENOMEM;
				mutex_unlock(&gzvm->irqfds.resampler_lock);
				goto fail;
			}

			resampler->gzvm = gzvm;
			INIT_LIST_HEAD(&resampler->list);
			resampler->notifier.gsi = irqfd->gsi;
			resampler->notifier.irq_acked = irqfd_resampler_ack;
			INIT_LIST_HEAD(&resampler->link);

			list_add(&resampler->link, &gzvm->irqfds.resampler_list);
			gzvm_register_irq_ack_notifier(gzvm,
						       &resampler->notifier);
			irqfd->resampler = resampler;
		}

		list_add_rcu(&irqfd->resampler_link, &irqfd->resampler->list);
		synchronize_srcu(&gzvm->irq_srcu);

		mutex_unlock(&gzvm->irqfds.resampler_lock);
	}

	/*
	 * Install our own custom wake-up handling so we are notified via
	 * a callback whenever someone signals the underlying eventfd
	 */
	init_waitqueue_func_entry(&irqfd->wait, irqfd_wakeup);
	init_poll_funcptr(&irqfd->pt, irqfd_ptable_queue_proc);

	spin_lock_irq(&gzvm->irqfds.lock);

	ret = 0;
	list_for_each_entry(tmp, &gzvm->irqfds.items, list) {
		if (irqfd->eventfd != tmp->eventfd)
			continue;
		/* This fd is used for another irq already. */
		GZVM_ERR("already used: gsi=%d fd=%d\n", args->gsi, args->fd);
		ret = -EBUSY;
		spin_unlock_irq(&gzvm->irqfds.lock);
		goto fail;
	}

	idx = srcu_read_lock(&gzvm->irq_srcu);
	irqfd_update(gzvm, irqfd);

	list_add_tail(&irqfd->list, &gzvm->irqfds.items);

	spin_unlock_irq(&gzvm->irqfds.lock);

	/*
	 * Check if there was an event already pending on the eventfd
	 * before we registered, and trigger it as if we didn't miss it.
	 */
	events = vfs_poll(f.file, &irqfd->pt);

	if (events & EPOLLIN)
		schedule_work(&irqfd->inject);

	srcu_read_unlock(&gzvm->irq_srcu, idx);

	/*
	 * do not drop the file until the irqfd is fully initialized, otherwise
	 * we might race against the EPOLLHUP
	 */
	fdput(f);
	return 0;

fail:
	if (irqfd->resampler)
		irqfd_resampler_shutdown(irqfd);

	if (resamplefd && !IS_ERR(resamplefd))
		eventfd_ctx_put(resamplefd);

	if (eventfd && !IS_ERR(eventfd))
		eventfd_ctx_put(eventfd);

	fdput(f);

out:
	kfree(irqfd);
	return ret;
}

static void gzvm_notify_acked_gsi(struct gzvm *gzvm, int gsi)
{
	struct gzvm_irq_ack_notifier *gian;

	hlist_for_each_entry_srcu(gian, &gzvm->irq_ack_notifier_list,
				  link, srcu_read_lock_held(&gzvm->irq_srcu))
		if (gian->gsi == gsi)
			gian->irq_acked(gian);
}

static bool gzvm_valid_gsi(unsigned int gsi)
{
	/* TODO: */
	return true;
}

void gzvm_notify_acked_irq(struct gzvm *gzvm, unsigned int gsi)
{
	int idx;

	idx = srcu_read_lock(&gzvm->irq_srcu);
	if (gzvm_valid_gsi(gsi))
		gzvm_notify_acked_gsi(gzvm, gsi);
	srcu_read_unlock(&gzvm->irq_srcu, idx);
}

/*
 * shutdown any irqfd's that match fd+gsi
 */
static int gzvm_irqfd_deassign(struct gzvm *gzvm, struct kvm_irqfd *args)
{
	struct gzvm_kernel_irqfd *irqfd, *tmp;
	struct eventfd_ctx *eventfd;

	eventfd = eventfd_ctx_fdget(args->fd);
	if (IS_ERR(eventfd))
		return PTR_ERR(eventfd);

	GZVM_DEBUG("%s gsi=%d fd=%d resample_fd=%d\n", __func__, args->gsi,
		   args->fd, args->resamplefd);

	spin_lock_irq(&gzvm->irqfds.lock);

	list_for_each_entry_safe(irqfd, tmp, &gzvm->irqfds.items, list) {
		if (irqfd->eventfd == eventfd && irqfd->gsi == args->gsi) {
			/*
			 * This clearing of irq_entry.type is needed for when
			 * another thread calls kvm_irq_routing_update before
			 * we flush workqueue below (we synchronize with
			 * kvm_irq_routing_update using irqfds.lock).
			 */
			write_seqcount_begin(&irqfd->irq_entry_sc);
			irqfd->irq_entry.type = 0;
			write_seqcount_end(&irqfd->irq_entry_sc);
			irqfd_deactivate(irqfd);
		}
	}

	spin_unlock_irq(&gzvm->irqfds.lock);
	eventfd_ctx_put(eventfd);

	/*
	 * Block until we know all outstanding shutdown jobs have completed
	 * so that we guarantee there will not be any more interrupts on this
	 * gsi once this deassign function returns.
	 */
	flush_workqueue(irqfd_cleanup_wq);

	return 0;
}

int gzvm_irqfd(struct gzvm *gzvm, struct kvm_irqfd *args)
{
	if (args->flags & ~(KVM_IRQFD_FLAG_DEASSIGN | KVM_IRQFD_FLAG_RESAMPLE))
		return -EINVAL;

	if (args->flags & KVM_IRQFD_FLAG_DEASSIGN)
		return gzvm_irqfd_deassign(gzvm, args);

	return gzvm_irqfd_assign(gzvm, args);
}

/*
 * This function is called as the kvm VM fd is being released. Shutdown all
 * irqfds that still remain open
 */
void gzvm_irqfd_release(struct gzvm *gzvm)
{
	struct gzvm_kernel_irqfd *irqfd, *tmp;

	spin_lock_irq(&gzvm->irqfds.lock);

	list_for_each_entry_safe(irqfd, tmp, &gzvm->irqfds.items, list)
		irqfd_deactivate(irqfd);

	spin_unlock_irq(&gzvm->irqfds.lock);

	/*
	 * Block until we know all outstanding shutdown jobs have completed
	 * since we do not take a kvm* reference.
	 */
	flush_workqueue(irqfd_cleanup_wq);
}

/*
 * Take note of a change in irq routing.
 * Caller must invoke synchronize_srcu(&gzvm->irq_srcu) afterwards.
 */
void gzvm_irq_routing_update(struct gzvm *gzvm)
{
	struct gzvm_kernel_irqfd *irqfd;

	spin_lock_irq(&gzvm->irqfds.lock);

	list_for_each_entry(irqfd, &gzvm->irqfds.items, list) {
		irqfd_update(gzvm, irqfd);
	}

	spin_unlock_irq(&gzvm->irqfds.lock);
}

/*
 * create a host-wide workqueue for issuing deferred shutdown requests
 * aggregated from all vm* instances. We need our own isolated
 * queue to ease flushing work items when a VM exits.
 */
int gzvm_irqfd_init(void)
{
	irqfd_cleanup_wq = alloc_workqueue("kvm-irqfd-cleanup", 0, 0);
	if (!irqfd_cleanup_wq)
		return -ENOMEM;

	return 0;
}

void gzvm_irqfd_exit(void)
{
	destroy_workqueue(irqfd_cleanup_wq);
}

/* assumes gzvm->slots_lock held */
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

int gzvm_init_eventfd(struct gzvm *gzvm)
{
	spin_lock_init(&gzvm->irqfds.lock);
	INIT_LIST_HEAD(&gzvm->irqfds.items);
	INIT_LIST_HEAD(&gzvm->irqfds.resampler_list);
	if (init_srcu_struct(&gzvm->irq_srcu))
		return -EINVAL;
	INIT_HLIST_HEAD(&gzvm->irq_ack_notifier_list);
	mutex_init(&gzvm->irqfds.resampler_lock);
	INIT_LIST_HEAD(&gzvm->ioevents);

	return 0;
}
