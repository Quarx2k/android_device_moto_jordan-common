/*
 * Common include file for android_usb module wrapper
 *
 * Author: Tanguy Pruvot 2011 <tpruvot@github>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc., 59
 * Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 */

#ifndef __ANDROID_USB_COMMON_H__
#define __ANDROID_USB_COMMON_H__

#include <linux/errno.h>
#include <linux/string.h>
#include <linux/kobject.h>
#include <linux/device.h>
#include <linux/sysfs.h>
#include <linux/usb.h>
#include <linux/usb/android_composite.h>
#include <linux/usb/ch9.h>

#define TAG "android_usb"

#define SYS_CLASS  "android_usb"
#define SYS_DEVICE "android0"

#define KDEBUG 1

#define KINFO(format, ...) printk(KERN_INFO TAG ": " format, ## __VA_ARGS__)

#if KDEBUG
#define KDBG(format, ...) printk(KERN_DEBUG TAG ": " format, ## __VA_ARGS__)
#else
#define KDBG(format, ...)
#endif

#define MAX_DEVICE_TYPE_NUM   20
#define MAX_DEVICE_NAME_SIZE  20

#define FUNCTIONS_SZ 255

/* for later use

struct android_usb_gadget_data {
	char name[MAX_DEVICE_NAME_SIZE];
	int enable;

	struct usb_device_descriptor desc;

	struct device *pdev;
	struct device *parent; // android0

	struct device *target; // real kernel gadget dev
};
*/

struct android_usb_data {
	char functions[FUNCTIONS_SZ];
	int enable;

	struct usb_device_descriptor desc;

	struct device *pdev;

	int comp;

	int acm;
	int adb;
	int rndis;
	int mtp;
	int ptp;
	int msc;
	int ets;

	int num_functions;

        // for later use
	// struct android_usb_gadget_data gadget_data[MAX_DEVICE_TYPE_NUM];
};

#endif //__ANDROID_USB_COMMON_H__
