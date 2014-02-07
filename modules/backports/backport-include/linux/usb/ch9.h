#ifndef __BACKPORT__LINUX_USB_CH9_H
#define __BACKPORT__LINUX_USB_CH9_H

#include <linux/version.h>

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,31)
#define usb_device_speed old_usb_device_speed
#define USB_SPEED_UNKNOWN OLD_USB_SPEED_UNKNOWN
#define USB_SPEED_LOW OLD_USB_SPEED_LOW
#define USB_SPEED_FULL OLD_USB_SPEED_FULL
#define USB_SPEED_HIGH OLD_USB_SPEED_HIGH
#include_next <linux/usb/ch9.h>
#undef usb_device_speed
#undef USB_SPEED_UNKNOWN
#undef USB_SPEED_LOW
#undef USB_SPEED_FULL
#undef USB_SPEED_HIGH
enum usb_device_speed {
	USB_SPEED_UNKNOWN = 0,          /* enumerating */
	USB_SPEED_LOW, USB_SPEED_FULL,      /* usb 1.1 */
	USB_SPEED_HIGH,             /* usb 2.0 */
	USB_SPEED_WIRELESS,         /* wireless (usb 2.5) */
	USB_SPEED_SUPER,            /* usb 3.0 */
};
#else
#include_next <linux/usb/ch9.h>
#endif /* < 2.6.30 */

#if LINUX_VERSION_CODE < KERNEL_VERSION(3,2,0)
#include <linux/types.h>    /* __u8 etc */
#include <asm/byteorder.h>  /* le16_to_cpu */

/**
 * usb_endpoint_maxp - get endpoint's max packet size
 * @epd: endpoint to be checked
 *
 * Returns @epd's max packet
 */
#define usb_endpoint_maxp LINUX_BACKPORT(usb_endpoint_maxp)
static inline int usb_endpoint_maxp(const struct usb_endpoint_descriptor *epd)
{
	return __le16_to_cpu(epd->wMaxPacketSize);
}
#endif /* < 3.2 */

#endif /* __BACKPORT__LINUX_USB_CH9_H */
