/*
 * Copyright 2010    Hauke Mehrtens <hauke@hauke-m.de>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Backport functionality introduced in Linux 2.6.37.
 */

#include <linux/compat.h>
#include <linux/netdevice.h>
#include <net/sock.h>
#include <linux/nsproxy.h>
#include <linux/vmalloc.h>
#include <linux/leds.h>

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,35)
static const void *net_current_ns(void)
{
	return current->nsproxy->net_ns;
}

static const void *net_initial_ns(void)
{
	return &init_net;
}

static const void *net_netlink_ns(struct sock *sk)
{
	return sock_net(sk);
}

struct kobj_ns_type_operations net_ns_type_operations = {
	.type = KOBJ_NS_TYPE_NET,
	.current_ns = net_current_ns,
	.netlink_ns = net_netlink_ns,
	.initial_ns = net_initial_ns,
};
EXPORT_SYMBOL_GPL(net_ns_type_operations);
#endif /* LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,35)*/ 

#if IS_ENABLED(CONFIG_LEDS_CLASS) && !defined(CPTCFG_BACKPORT_BUILD_LEDS)

#undef led_brightness_set
#undef led_classdev_unregister

static DEFINE_SPINLOCK(led_lock);
static LIST_HEAD(led_timers);

struct led_timer {
	struct list_head list;
	struct led_classdev *cdev;
	struct timer_list blink_timer;
	unsigned long blink_delay_on;
	unsigned long blink_delay_off;
	int blink_brightness;
};

static void led_brightness_set(struct led_classdev *led_cdev,
			       enum led_brightness brightness)
{
	led_cdev->brightness = brightness;
	led_cdev->brightness_set(led_cdev, brightness);
}

static struct led_timer *led_get_timer(struct led_classdev *led_cdev)
{
	struct led_timer *p;
	unsigned long flags;

	spin_lock_irqsave(&led_lock, flags);
	list_for_each_entry(p, &led_timers, list) {
		if (p->cdev == led_cdev)
			goto found;
	}
	p = NULL;
found:
	spin_unlock_irqrestore(&led_lock, flags);
	return p;
}

static void led_stop_software_blink(struct led_timer *led)
{
	del_timer_sync(&led->blink_timer);
	led->blink_delay_on = 0;
	led->blink_delay_off = 0;
}

static void led_timer_function(unsigned long data)
{
	struct led_timer *led = (struct led_timer *)data;
	unsigned long brightness;
	unsigned long delay;

	if (!led->blink_delay_on || !led->blink_delay_off) {
		led->cdev->brightness_set(led->cdev, LED_OFF);
		return;
	}

	brightness = led->cdev->brightness;
	if (!brightness) {
		/* Time to switch the LED on. */
		brightness = led->blink_brightness;
		delay = led->blink_delay_on;
	} else {
		/* Store the current brightness value to be able
		 * to restore it when the delay_off period is over.
		 */
		led->blink_brightness = brightness;
		brightness = LED_OFF;
		delay = led->blink_delay_off;
	}

	led_brightness_set(led->cdev, brightness);
	mod_timer(&led->blink_timer, jiffies + msecs_to_jiffies(delay));
}

static struct led_timer *led_new_timer(struct led_classdev *led_cdev)
{
	struct led_timer *led;
	unsigned long flags;

	led = kzalloc(sizeof(struct led_timer), GFP_ATOMIC);
	if (!led)
		return NULL;

	led->cdev = led_cdev;
	init_timer(&led->blink_timer);
	led->blink_timer.function = led_timer_function;
	led->blink_timer.data = (unsigned long) led;

	spin_lock_irqsave(&led_lock, flags);
	list_add(&led->list, &led_timers);
	spin_unlock_irqrestore(&led_lock, flags);

	return led;
}

void led_blink_set(struct led_classdev *led_cdev,
		   unsigned long *delay_on,
		   unsigned long *delay_off)
{
	struct led_timer *led;
	int current_brightness;

	if (led_cdev->blink_set &&
	    !led_cdev->blink_set(led_cdev, delay_on, delay_off))
		return;

	led = led_get_timer(led_cdev);
	if (!led) {
		led = led_new_timer(led_cdev);
		if (!led)
			return;
	}

	/* blink with 1 Hz as default if nothing specified */
	if (!*delay_on && !*delay_off)
		*delay_on = *delay_off = 500;

	if (led->blink_delay_on == *delay_on &&
	    led->blink_delay_off == *delay_off)
		return;

	current_brightness = led_cdev->brightness;
	if (current_brightness)
		led->blink_brightness = current_brightness;
	if (!led->blink_brightness)
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,30)
		led->blink_brightness = led_cdev->max_brightness;
#else
		led->blink_brightness = LED_FULL;
#endif

	led_stop_software_blink(led);
	led->blink_delay_on = *delay_on;
	led->blink_delay_off = *delay_off;

	/* never on - don't blink */
	if (!*delay_on)
		return;

	/* never off - just set to brightness */
	if (!*delay_off) {
		led_brightness_set(led_cdev, led->blink_brightness);
		return;
	}

	mod_timer(&led->blink_timer, jiffies + 1);
}
EXPORT_SYMBOL_GPL(led_blink_set);

void compat_led_brightness_set(struct led_classdev *led_cdev,
			       enum led_brightness brightness)
{
	struct led_timer *led = led_get_timer(led_cdev);

	if (led)
		led_stop_software_blink(led);

	return led_cdev->brightness_set(led_cdev, brightness);
}
EXPORT_SYMBOL_GPL(compat_led_brightness_set);

void compat_led_classdev_unregister(struct led_classdev *led_cdev)
{
	struct led_timer *led = led_get_timer(led_cdev);
	unsigned long flags;

	if (led) {
		del_timer_sync(&led->blink_timer);
		spin_lock_irqsave(&led_lock, flags);
		list_del(&led->list);
		spin_unlock_irqrestore(&led_lock, flags);
		kfree(led);
	}

	led_classdev_unregister(led_cdev);
}
EXPORT_SYMBOL_GPL(compat_led_classdev_unregister);
#endif

/**
 *	vzalloc - allocate virtually contiguous memory with zero fill
 *	@size:	allocation size
 *	Allocate enough pages to cover @size from the page level
 *	allocator and map them into contiguous kernel virtual space.
 *	The memory allocated is set to zero.
 *
 *	For tight control over page level allocator and protection flags
 *	use __vmalloc() instead.
 */
void *vzalloc(unsigned long size)
{
	void *buf;
	buf = vmalloc(size);
	if (buf)
		memset(buf, 0, size);
	return buf;
}
EXPORT_SYMBOL_GPL(vzalloc);

#if (RHEL_RELEASE_CODE < RHEL_RELEASE_VERSION(6,4))
#ifdef CONFIG_RPS
/**
 *	netif_set_real_num_rx_queues - set actual number of RX queues used
 *	@dev: Network device
 *	@rxq: Actual number of RX queues
 *
 *	This must be called either with the rtnl_lock held or before
 *	registration of the net device.  Returns 0 on success, or a
 *	negative error code.  If called before registration, it always
 *	succeeds.
 */
int netif_set_real_num_rx_queues(struct net_device *dev, unsigned int rxq)
{
	if (rxq < 1 || rxq > dev->num_rx_queues)
		return -EINVAL;

	/* we can't update the sysfs object for older kernels */
	if (dev->reg_state == NETREG_REGISTERED)
		return -EINVAL;
	dev->num_rx_queues = rxq;
	return 0;
}
EXPORT_SYMBOL_GPL(netif_set_real_num_rx_queues);
#endif
#endif
