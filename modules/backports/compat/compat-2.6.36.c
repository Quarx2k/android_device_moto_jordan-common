/*
 * Copyright 2010    Hauke Mehrtens <hauke@hauke-m.de>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Backport functionality introduced in Linux 2.6.36.
 */

#include <linux/compat.h>
#include <linux/usb.h>

#ifdef CPTCFG_BACKPORT_OPTION_USB_URB_THREAD_FIX
/* Callers must hold anchor->lock */
static void __usb_unanchor_urb(struct urb *urb, struct usb_anchor *anchor)
{
	urb->anchor = NULL;
	list_del(&urb->anchor_list);
	usb_put_urb(urb);
	if (list_empty(&anchor->urb_list))
		wake_up(&anchor->wait);
}

/**
 * usb_get_from_anchor - get an anchor's oldest urb
 * @anchor: the anchor whose urb you want
 *
 * this will take the oldest urb from an anchor,
 * unanchor and return it
 */
struct urb *usb_get_from_anchor(struct usb_anchor *anchor)
{
	struct urb *victim;
	unsigned long flags;

	spin_lock_irqsave(&anchor->lock, flags);
	if (!list_empty(&anchor->urb_list)) {
		victim = list_entry(anchor->urb_list.next, struct urb,
				    anchor_list);
		usb_get_urb(victim);
		__usb_unanchor_urb(victim, anchor);
	} else {
		victim = NULL;
	}
	spin_unlock_irqrestore(&anchor->lock, flags);

	return victim;
}
EXPORT_SYMBOL_GPL(usb_get_from_anchor);

/**
 * usb_scuttle_anchored_urbs - unanchor all an anchor's urbs
 * @anchor: the anchor whose urbs you want to unanchor
 *
 * use this to get rid of all an anchor's urbs
 */
void usb_scuttle_anchored_urbs(struct usb_anchor *anchor)
{
	struct urb *victim;
	unsigned long flags;

	spin_lock_irqsave(&anchor->lock, flags);
	while (!list_empty(&anchor->urb_list)) {
		victim = list_entry(anchor->urb_list.prev, struct urb,
				    anchor_list);
		__usb_unanchor_urb(victim, anchor);
	}
	spin_unlock_irqrestore(&anchor->lock, flags);
}
EXPORT_SYMBOL_GPL(usb_scuttle_anchored_urbs);

#endif /* CPTCFG_BACKPORT_OPTION_USB_URB_THREAD_FIX */

struct workqueue_struct *system_wq __read_mostly;
struct workqueue_struct *system_long_wq __read_mostly;
struct workqueue_struct *system_nrt_wq __read_mostly;
EXPORT_SYMBOL_GPL(system_wq);
EXPORT_SYMBOL_GPL(system_long_wq);
EXPORT_SYMBOL_GPL(system_nrt_wq);

int schedule_work(struct work_struct *work)
{
	return queue_work(system_wq, work);
}
EXPORT_SYMBOL_GPL(schedule_work);

int schedule_delayed_work(struct delayed_work *dwork,
                                 unsigned long delay)
{
	return queue_delayed_work(system_wq, dwork, delay);
}
EXPORT_SYMBOL_GPL(schedule_delayed_work);

void flush_scheduled_work(void)
{
	/*
	 * It is debatable which one we should prioritize first, lets
	 * go with the old kernel's one first for now (keventd_wq) and
	 * if think its reasonable later we can flip this around.
	 */
	flush_workqueue(system_wq);
	flush_scheduled_work();
}
EXPORT_SYMBOL_GPL(flush_scheduled_work);

void backport_system_workqueue_create(void)
{
	system_wq = alloc_workqueue("events", 0, 0);
	system_long_wq = alloc_workqueue("events_long", 0, 0);
	system_nrt_wq = create_singlethread_workqueue("events_nrt");
	BUG_ON(!system_wq || !system_long_wq || !system_nrt_wq);
}

void backport_system_workqueue_destroy(void)
{
	destroy_workqueue(system_wq);
	destroy_workqueue(system_long_wq);
	destroy_workqueue(system_nrt_wq);
}
