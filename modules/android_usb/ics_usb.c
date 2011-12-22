/*
 * ICS Compatibility sysfs wrapper module for moto 2.6.32 kernels
 *
 * Author: Tanguy Pruvot 2011 <tpruvot@github>
 *  Made for the Motorola Defy (jordan)
 *
 *  v1.0: empty module stub with sysfs
 *  v1.1: find and symlink standard usb gadgets
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

#include <linux/module.h>
#include <linux/platform_device.h>

#include "ics_usb.h"

static struct file_operations android_usb_fops = {
	.owner = THIS_MODULE,
};

static struct android_usb_data *android0 = NULL;

// required for gadgets intfs
static struct class *android_usb_class = NULL;
int android0_major = -1;

/*****************************************************************************/
//enable : 0/1

static ssize_t show_enable(struct device *dev,
                   struct device_attribute *attr, char *buf)
{
	KDBG("get enable");
	if (android0 != NULL) {
		return sprintf(buf, "%d\n", android0->enable);
	}
	return sprintf(buf, "\n");
}
static ssize_t set_enable(struct device *dev,
                   struct device_attribute *attr, const char *buf, size_t count)
{
	int value;
	if (sscanf(buf, "%d", &value) != 1) return -EINVAL;
	KDBG("set enable [%s]", buf);
	if (android0 != NULL) {
	   android0->enable = (int) (value != 0);
	}
	return count;
}
static DEVICE_ATTR(enable, 0664, show_enable, set_enable);

//functions : string
static ssize_t show_functions(struct device *dev,
                   struct device_attribute *attr, char *buf)
{
	KDBG("get functions");
	if (android0 != NULL) {
		return sprintf(buf, "%s\n", android0->functions);
	}
	return sprintf(buf, "\n");
}
static ssize_t set_functions(struct device *dev,
                   struct device_attribute *attr, const char *buf, size_t count)
{
	char buffer[FUNCTIONS_SZ];
	if (sscanf(buf, "%s", buffer) == 0) return -EINVAL;
	KDBG("set functions [%s]", buf);
	if (android0 != NULL) {
		strncpy(android0->functions, buffer, FUNCTIONS_SZ);
	}
	return count;
}
static DEVICE_ATTR(functions, 0664, show_functions, set_functions);

//idVendor : usb descriptor
static ssize_t show_desc_idVendor(struct device *dev,
                   struct device_attribute *attr, char *buf)
{
	KDBG("get idVendor");
	if (android0 != NULL) {
		return sprintf(buf, "%x\n", android0->desc.idVendor);
	}
	return sprintf(buf, "\n");
}
static ssize_t set_desc_idVendor(struct device *dev,
                   struct device_attribute *attr, const char *buf, size_t count)
{
	int value;
	if (sscanf(buf, "%x", &value) == 0) return -EINVAL;
	KDBG("set idVendor [%s]", buf);
	if (android0 != NULL) {
		android0->desc.idVendor = value;
	}
	return count;
}
static DEVICE_ATTR(idVendor, 0664, show_desc_idVendor, set_desc_idVendor);

//idProduct
static ssize_t show_desc_idProduct(struct device *dev,
                   struct device_attribute *attr, char *buf)
{
	KDBG("get idProduct");
	if (android0 != NULL) {
		return sprintf(buf, "%x\n", android0->desc.idProduct);
	}
	return sprintf(buf, "\n");
}
static ssize_t set_desc_idProduct(struct device *dev,
                   struct device_attribute *attr, const char *buf, size_t count)
{
	int value;
	if (sscanf(buf, "%x", &value) == 0) return -EINVAL;
	KDBG("set idProduct [%s]", buf);
	if (android0 != NULL) {
		android0->desc.idProduct = value;
	}
	return count;
}
static DEVICE_ATTR(idProduct, 0664, show_desc_idProduct, set_desc_idProduct);

//bDeviceClass
static ssize_t show_desc_bDeviceClass(struct device *dev,
                   struct device_attribute *attr, char *buf)
{
	KDBG("get bDeviceClass");
	if (android0 != NULL) {
		return sprintf(buf, "%d\n", android0->desc.bDeviceClass);
	}
	return sprintf(buf, "\n");
}
static ssize_t set_desc_bDeviceClass(struct device *dev,
                   struct device_attribute *attr, const char *buf, size_t count)
{
	int value;
	if (sscanf(buf, "%d", &value) == 0) return -EINVAL;
	KDBG("set bDeviceClass [%s]", buf);
	if (android0 != NULL) {
		android0->desc.bDeviceClass = value;
	}
	return count;
}
static DEVICE_ATTR(bDeviceClass, 0664, show_desc_bDeviceClass, set_desc_bDeviceClass);

/*****************************************************************************/
static int android_usb_create_sysfs_files(struct android_usb_data *data)
{
	int ret;
	ret  = device_create_file(data->pdev, &dev_attr_enable);
	ret |= device_create_file(data->pdev, &dev_attr_functions);
	ret |= device_create_file(data->pdev, &dev_attr_idVendor);
	ret |= device_create_file(data->pdev, &dev_attr_idProduct);
	ret |= device_create_file(data->pdev, &dev_attr_bDeviceClass);
	return ret;
}

static void android_usb_remove_sysfs_files(struct android_usb_data *data)
{
	device_remove_file(data->pdev, &dev_attr_enable);
	device_remove_file(data->pdev, &dev_attr_functions);
	device_remove_file(data->pdev, &dev_attr_idVendor);
	device_remove_file(data->pdev, &dev_attr_idProduct);
	device_remove_file(data->pdev, &dev_attr_bDeviceClass);
}

/*****************************************************************************/
static struct kobject *kset_find(struct kset *kset, const char *name)
{
	struct kobject *k, *ret = NULL;
	if (kset == NULL) {
		return NULL;
	}
	spin_lock(&kset->list_lock);
	list_for_each_entry(k, &kset->list, entry) {
		//KDBG("   kset_find=%s p=%x...", kobject_name(k), k->parent);
		if (kobject_name(k) && !strcmp(kobject_name(k), name)) {
			if (strcmp(kobject_name(k->parent), "usb_composite"))
				continue;
			ret = kobject_get(k);
			break;
		}
	}
	spin_unlock(&kset->list_lock);
	return ret;
}
static struct kobject *func_kobj = NULL;
static struct kobject *device_search_composite(struct device *dev)
{
	struct kobject * kobj;
	const char * devpath = NULL;
	if (!func_kobj) {
		kobj = &dev->kobj;
		if (kobj->parent) {
			func_kobj = kset_find(kobj->kset, "adb");
			if (func_kobj) {
				devpath = kobject_get_path(func_kobj, GFP_KERNEL);
				KDBG(" kobject found at %s...", devpath);
				kfree(devpath);
				kobject_put(func_kobj);
			}
		}
	}
	return func_kobj;
}
static struct kobject *device_get_func_kobj(const char* name)
{
	struct kobject * kobj;

	kobj = kset_find(func_kobj->kset, name);
	if (kobj) kobject_put(kobj);

	return kobj;
}
/*****************************************************************************/
static int search_and_link_gadget(struct android_usb_data *data,
                                  const char* moto_name,  const char* f_name)
{
	struct kobject* kobj = device_get_func_kobj(moto_name);
	int ret = 0;
	if (kobj) {
		ret = sysfs_create_link(&data->pdev->kobj, kobj, f_name);
		data->num_functions ++;
		KDBG(" linked %s to %s...", moto_name, f_name);
		return (ret == 0);
	}
	return 0;
}
static int __match_android_usb(struct device *dev, void *data)
{
	return !strncmp(dev_name(dev), "android_usb", 11);
}
static int android_usb_link_sysfs_platform(struct android_usb_data *data)
{
	struct device* sdev;
	// get device ref to this device : /sys/devices/platform/android_usb
	sdev = device_find_child(&platform_bus, NULL, __match_android_usb);
	if (!sdev) {
		return -ENODEV;
	}
	KDBG(" found %s device...", dev_name(sdev));

	// and search "adb" kobj to get the parent (usb_composite)
	data->comp = (device_search_composite(sdev) != NULL);

	data->adb = search_and_link_gadget(data, "adb", "f_adb");
	data->acm = search_and_link_gadget(data, "acm", "f_acm");
	data->mtp = search_and_link_gadget(data, "mtp", "f_mtp");
	data->ptp = search_and_link_gadget(data, "usbnet", "f_ptp");
	data->ets = search_and_link_gadget(data, "ets", "f_ets");
	data->rndis = search_and_link_gadget(data, "rndis", "f_rndis");
	data->msc = search_and_link_gadget(data, "usb_mass_storage", "f_mass_storage");

	return 0;
}
static void android_usb_unlink_sysfs_platform(struct android_usb_data *data)
{
	if (data->adb)  sysfs_remove_link(&data->pdev->kobj, "f_adb");
	if (data->acm)  sysfs_remove_link(&data->pdev->kobj, "f_acm");
	if (data->mtp)  sysfs_remove_link(&data->pdev->kobj, "f_mtp");
	if (data->ptp)  sysfs_remove_link(&data->pdev->kobj, "f_ptp");
	if (data->ets)  sysfs_remove_link(&data->pdev->kobj, "f_ets");
	if (data->rndis)  sysfs_remove_link(&data->pdev->kobj, "f_rndis");
	if (data->msc)  sysfs_remove_link(&data->pdev->kobj, "f_mass_storage");
}

/*****************************************************************************/

static int __init android_usb_init(void)
{
	int ret;
	struct android_usb_data *data;

	android_usb_class = class_create(THIS_MODULE, SYS_CLASS);
	if (!android_usb_class) {
		return -ENODEV;
	}

	android0_major = register_chrdev(0, SYS_DEVICE, &android_usb_fops);
	if (android0_major < 0) {
		return -ENODEV;
	}

	data = kzalloc(sizeof(struct android_usb_data), GFP_KERNEL);
	if (!data) {
		return -ENOMEM;
	}

	data->pdev = device_create(
		android_usb_class, NULL,
		MKDEV(android0_major, 0),
		data, SYS_DEVICE
	);


	ret = android_usb_create_sysfs_files(data);
	if (ret < 0) {
		return -ENODEV;
	}
	KDBG(SYS_DEVICE " attributes ready");

	//init data :
	data->desc.idVendor  = 0x22b8;
	data->desc.idProduct = 0x41da;

	data->enable = 1;

	android0 = data;

	KINFO("registered " SYS_DEVICE " device");

	ret = android_usb_link_sysfs_platform(data);

	return ret;
}

static void __exit android_usb_exit(void)
{
	if (android0 != NULL) {
		android_usb_unlink_sysfs_platform(android0);
		android_usb_remove_sysfs_files(android0);
		device_destroy(android_usb_class, MKDEV(android0_major, 0));
		kfree(android0);
		KDBG("unregistered " SYS_DEVICE " device");
		android0 = NULL;
	}
	if (android0_major >= 0) {
		unregister_chrdev(android0_major, SYS_DEVICE);
		android0_major = -1;
	}
	if (android_usb_class != NULL) {
		class_destroy(android_usb_class);
		KINFO("unregistered " SYS_CLASS " class");
		android_usb_class = NULL;
	}
}

module_init(android_usb_init);
module_exit(android_usb_exit);

MODULE_AUTHOR("Tanguy Pruvot");
MODULE_DESCRIPTION("ICS sysfs usb wrapper");
MODULE_VERSION("1.2");
MODULE_LICENSE("GPL");
