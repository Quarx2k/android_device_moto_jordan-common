/*
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */


#include <linux/module.h>
#include <linux/smp_lock.h>
#include <linux/i2c.h>

#define TAG "qtouch_unload"

struct driver_private {
	struct kobject kobj;
};

static int __init qtouch_unload_init(void)
{
    struct device_driver *drv;
    struct kobject *kobj;
  
    printk(KERN_INFO "qtouch_unload init\n");
    lock_kernel();
    
    drv = driver_find("qtouch-obp-ts", &i2c_bus_type);
    if (!drv) {
	    printk("qtouch_ts driver not found, bailing out!\n");
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
    
}

module_init(qtouch_unload_init);
module_exit(qtouch_unload_exit);

MODULE_ALIAS("qtouch_unload");
MODULE_DESCRIPTION("Disable builtin qtouch driver");
MODULE_AUTHOR("M1cha");
MODULE_LICENSE("GPL");
