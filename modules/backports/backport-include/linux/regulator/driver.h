/*
 * driver.h -- SoC Regulator driver support.
 *
 * Copyright (C) 2007, 2008 Wolfson Microelectronics PLC.
 *
 * Author: Liam Girdwood <lrg@slimlogic.co.uk>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Regulator Driver Interface.
 */

#ifndef __BACKPORT_LINUX_REGULATOR_DRIVER_H_
#define __BACKPORT_LINUX_REGULATOR_DRIVER_H_

#include <linux/version.h>
#include_next <linux/regulator/driver.h>

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3,6,0))
int regulator_map_voltage_ascend(struct regulator_dev *rdev,
				 int min_uV, int max_uV);
#endif /* (LINUX_VERSION_CODE >= KERNEL_VERSION(3,6,0)) */

#if (LINUX_VERSION_CODE < KERNEL_VERSION(3,13,0)) && \
    (LINUX_VERSION_CODE >= KERNEL_VERSION(3,5,0))
#define devm_regulator_register LINUX_BACKPORT(devm_regulator_register)
struct regulator_dev *
devm_regulator_register(struct device *dev,
			const struct regulator_desc *regulator_desc,
			const struct regulator_config *config);
#define devm_regulator_unregister LINUX_BACKPORT(devm_regulator_unregister)
void devm_regulator_unregister(struct device *dev, struct regulator_dev *rdev);
#endif /* (LINUX_VERSION_CODE < KERNEL_VERSION(3,13,0)) &&
	  (LINUX_VERSION_CODE >= KERNEL_VERSION(3,5,0)) */

#ifndef REGULATOR_LINEAR_RANGE
/* Initialize struct regulator_linear_range */
#define REGULATOR_LINEAR_RANGE(_min_uV, _min_sel, _max_sel, _step_uV)	\
{									\
	.min_uV		= _min_uV,					\
	.min_sel	= _min_sel,					\
	.max_sel	= _max_sel,					\
	.uV_step	= _step_uV,					\
}
#endif

#endif /* __BACKPORT_LINUX_REGULATOR_DRIVER_H_ */
