/*
 * cpcap_battery_unload - cpcap_battery-unloader module for Motorola Defy
 *
 *
 * Copyright (C) 2011 m11kkaa
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */


#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/smp_lock.h>

#define TAG "cpcap_battery_unload"

struct driver_private {
	struct kobject kobj;
};

static int __init qtouch_unload_init(void)
{
    struct device_driver *drv;
    struct kobject *kobj;

    printk(KERN_INFO "cpcap_battery_unload init\n");
    lock_kernel();
    
    drv = driver_find("cpcap_battery", &platform_bus_type);
    if (!drv) {
	    printk("cpcap_battery_unload driver not found, bailing out!\n");
	    unlock_kernel();
	    return -1;
    } else {
	    kobj = &drv->p->kobj;
	    driver_unregister(drv);
	    kobject_del(kobj);
    }
    
    unlock_kernel();
    return 0;
}

static void __exit qtouch_unload_exit(void)
{
    printk(KERN_INFO "cpcap_battery_unload exit\n");
}

module_init(qtouch_unload_init);
module_exit(qtouch_unload_exit);

MODULE_ALIAS("cpcap_battery_unload");
MODULE_DESCRIPTION("unload driver: cpcap_battery");
MODULE_AUTHOR("m11kkaa");
MODULE_LICENSE("GPL");

