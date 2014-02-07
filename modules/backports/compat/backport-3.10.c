/*
 * Copyright (c) 2013  Luis R. Rodriguez <mcgrof@do-not-panic.com>
 *
 * Linux backport symbols for kernels 3.10.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/err.h>
#include <linux/proc_fs.h>
#include <linux/random.h>
#include <linux/tty.h>

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3,6,0))
#include <linux/init.h>
#include <linux/debugfs.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/async.h>
#include <linux/mutex.h>
#include <linux/suspend.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/of.h>
#include <linux/regmap.h>
#include <linux/regulator/of_regulator.h>
#include <linux/regulator/consumer.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/machine.h>
#endif /* (LINUX_VERSION_CODE >= KERNEL_VERSION(3,6,0)) */

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3,6,0))
#ifdef CPTCFG_REGULATOR
/**
 * regulator_map_voltage_ascend - map_voltage() for ascendant voltage list
 *
 * @rdev: Regulator to operate on
 * @min_uV: Lower bound for voltage
 * @max_uV: Upper bound for voltage
 *
 * Drivers that have ascendant voltage list can use this as their
 * map_voltage() operation.
 */
int regulator_map_voltage_ascend(struct regulator_dev *rdev,
				 int min_uV, int max_uV)
{
	int i, ret;

	for (i = 0; i < rdev->desc->n_voltages; i++) {
		ret = rdev->desc->ops->list_voltage(rdev, i);
		if (ret < 0)
			continue;

		if (ret > max_uV)
			break;

		if (ret >= min_uV && ret <= max_uV)
			return i;
	}

	return -EINVAL;
}
EXPORT_SYMBOL_GPL(regulator_map_voltage_ascend);

#endif /* CPTCFG_REGULATOR */
#endif /* (LINUX_VERSION_CODE >= KERNEL_VERSION(3,6,0)) */

void proc_set_size(struct proc_dir_entry *de, loff_t size)
{
	de->size = size;
}
EXPORT_SYMBOL_GPL(proc_set_size);

void proc_set_user(struct proc_dir_entry *de, kuid_t uid, kgid_t gid)
{
	de->uid = uid;
	de->gid = gid;
}
EXPORT_SYMBOL_GPL(proc_set_user);

/* get_random_int() was not exported for module use until 3.10-rc.
   Implement it here in terms of the more expensive get_random_bytes()
 */
unsigned int get_random_int(void)
{
	unsigned int r;
	get_random_bytes(&r, sizeof(r));

	return r;
}
EXPORT_SYMBOL_GPL(get_random_int);

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,28))
#ifdef CONFIG_TTY
/**
 * tty_port_tty_wakeup - helper to wake up a tty
 *
 * @port: tty port
 */
void tty_port_tty_wakeup(struct tty_port *port)
{
	struct tty_struct *tty = tty_port_tty_get(port);

	if (tty) {
		tty_wakeup(tty);
		tty_kref_put(tty);
	}
}
EXPORT_SYMBOL_GPL(tty_port_tty_wakeup);

/**
 * tty_port_tty_hangup - helper to hang up a tty
 *
 * @port: tty port
 * @check_clocal: hang only ttys with CLOCAL unset?
 */
void tty_port_tty_hangup(struct tty_port *port, bool check_clocal)
{
	struct tty_struct *tty = tty_port_tty_get(port);

	if (tty && (!check_clocal || !C_CLOCAL(tty)))
		tty_hangup(tty);
	tty_kref_put(tty);
}
EXPORT_SYMBOL_GPL(tty_port_tty_hangup);
#endif /* CONFIG_TTY */
#endif /* (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,28)) */
