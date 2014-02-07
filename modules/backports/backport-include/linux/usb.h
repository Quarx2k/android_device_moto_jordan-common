#ifndef __BACKPORT_USB_H
#define __BACKPORT_USB_H

#include_next <linux/usb.h>
#include <linux/version.h>

#ifndef module_usb_driver
/**
 * module_usb_driver() - Helper macro for registering a USB driver
 * @__usb_driver: usb_driver struct
 *
 * Helper macro for USB drivers which do not do anything special in module
 * init/exit. This eliminates a lot of boilerplate. Each module may only
 * use this macro once, and calling it replaces module_init() and module_exit()
 */
#define module_usb_driver(__usb_driver) \
	module_driver(__usb_driver, usb_register, \
		       usb_deregister)
#endif

#ifndef USB_VENDOR_AND_INTERFACE_INFO
/**
 * Backports
 *
 * commit d81a5d1956731c453b85c141458d4ff5d6cc5366
 * Author: Gustavo Padovan <gustavo.padovan@collabora.co.uk>
 * Date:   Tue Jul 10 19:10:06 2012 -0300
 *
 * 	USB: add USB_VENDOR_AND_INTERFACE_INFO() macro
 */
#define USB_VENDOR_AND_INTERFACE_INFO(vend, cl, sc, pr) \
       .match_flags = USB_DEVICE_ID_MATCH_INT_INFO \
               | USB_DEVICE_ID_MATCH_VENDOR, \
       .idVendor = (vend), \
       .bInterfaceClass = (cl), \
       .bInterfaceSubClass = (sc), \
       .bInterfaceProtocol = (pr)
#endif /* USB_VENDOR_AND_INTERFACE_INFO */

#ifndef USB_DEVICE_INTERFACE_NUMBER
/**
 * USB_DEVICE_INTERFACE_NUMBER - describe a usb device with a specific interface number
 * @vend: the 16 bit USB Vendor ID
 * @prod: the 16 bit USB Product ID
 * @num: bInterfaceNumber value
 *
 * This macro is used to create a struct usb_device_id that matches a
 * specific interface number of devices.
 */
#define USB_DEVICE_INTERFACE_NUMBER(vend, prod, num) \
	.match_flags = USB_DEVICE_ID_MATCH_DEVICE, \
	.idVendor = (vend), \
	.idProduct = (prod)
#endif /* USB_DEVICE_INTERFACE_NUMBER */

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,36)
#ifdef CPTCFG_BACKPORT_OPTION_USB_URB_THREAD_FIX
#define usb_scuttle_anchored_urbs LINUX_BACKPORT(usb_scuttle_anchored_urbs)
#define usb_get_from_anchor LINUX_BACKPORT(usb_get_from_anchor)

extern struct urb *usb_get_from_anchor(struct usb_anchor *anchor);
extern void usb_scuttle_anchored_urbs(struct usb_anchor *anchor);
#endif
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,35)
/* mask usb_pipe_endpoint as RHEL6 backports this */
#define usb_pipe_endpoint LINUX_BACKPORT(usb_pipe_endpoint)

static inline struct usb_host_endpoint *
usb_pipe_endpoint(struct usb_device *dev, unsigned int pipe)
{
	struct usb_host_endpoint **eps;
	eps = usb_pipein(pipe) ? dev->ep_in : dev->ep_out;
	return eps[usb_pipeendpoint(pipe)];
}
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,34)
#define usb_alloc_coherent(dev, size, mem_flags, dma) usb_buffer_alloc(dev, size, mem_flags, dma)
#define usb_free_coherent(dev, size, addr, dma) usb_buffer_free(dev, size, addr, dma)

/* USB autosuspend and autoresume */
static inline int usb_enable_autosuspend(struct usb_device *udev)
{ return 0; }
static inline int usb_disable_autosuspend(struct usb_device *udev)
{ return 0; }
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,33)
#define usb_autopm_get_interface_no_resume LINUX_BACKPORT(usb_autopm_get_interface_no_resume)
#define usb_autopm_put_interface_no_suspend LINUX_BACKPORT(usb_autopm_put_interface_no_suspend)
#ifdef CONFIG_USB_SUSPEND
extern void usb_autopm_get_interface_no_resume(struct usb_interface *intf);
extern void usb_autopm_put_interface_no_suspend(struct usb_interface *intf);
#else
static inline void usb_autopm_get_interface_no_resume(struct usb_interface *intf)
{
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,32))
	atomic_inc(&intf->pm_usage_cnt);
#else
	intf->pm_usage_cnt++;
#endif
}
static inline void usb_autopm_put_interface_no_suspend(struct usb_interface *intf)
{
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,32))
	atomic_dec(&intf->pm_usage_cnt);
#else
	intf->pm_usage_cnt--;
#endif
}
#endif /* CONFIG_USB_SUSPEND */
#endif /* < 2.6.33 */

#ifndef USB_SUBCLASS_VENDOR_SPEC
/* this is defined in usb/ch9.h, but we only need it through here */
#define USB_SUBCLASS_VENDOR_SPEC	0xff
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,29)
static inline void usb_autopm_put_interface_async(struct usb_interface *intf)
{ }
static inline int usb_autopm_get_interface_async(struct usb_interface *intf)
{ return 0; }
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,29) && \
    LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,23)
#if defined(CONFIG_USB) || defined(CONFIG_USB_MODULE)
#define usb_unpoison_anchored_urbs LINUX_BACKPORT(usb_unpoison_anchored_urbs)
extern void usb_unpoison_anchored_urbs(struct usb_anchor *anchor);
#endif /* CONFIG_USB */
#endif /* 2.6.23 - 2.6.28 */

/* USB anchors were added as of 2.6.23 */
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,28) && \
    LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,23)
#define usb_unpoison_urb LINUX_BACKPORT(usb_unpoison_urb)
extern void usb_unpoison_urb(struct urb *urb);

#define usb_anchor_empty LINUX_BACKPORT(usb_anchor_empty)
extern int usb_anchor_empty(struct usb_anchor *anchor);
#endif /* 2.6.23-2.6.27 */

#if LINUX_VERSION_CODE < KERNEL_VERSION(3,2,0)
#define usb_translate_errors LINUX_BACKPORT(usb_translate_errors)
static inline int usb_translate_errors(int error_code)
{
	switch (error_code) {
	case 0:
	case -ENOMEM:
	case -ENODEV:
	case -EOPNOTSUPP:
		return error_code;
	default:
		return -EIO;
	}
}
#endif /* < 2.6.39 */

#endif /* __BACKPORT_USB_H */
