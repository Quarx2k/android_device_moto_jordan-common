/*
 * drivers/misc/camera_misc/camera_misc.c
 *
 * Copyright (C) 2010 Motorola Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/regulator/consumer.h>
#include <linux/gpio.h>

#include "camera_misc.h"
#include <../drivers/media/video/isp/isp.h>
#include <../drivers/media/video/isp/ispreg.h>

#include <linux/gpio_mapping.h>
#include <linux/of.h>

#define CONTROL_PADCONF_CAM_FLD   0x48002114
#define CONTROL_PADCONF_CAM_XCLKA 0x48002110

#define GPIO_SENSOR_NAME "camera_misc"

#define DEBUG 2

#define err_print(fmt, args...) \
	printk(KERN_ERR "fun %s "fmt"\n", __func__, ##args)

#if DEBUG > 0

#define dbg_print(fmt, args...) \
	printk(KERN_INFO "fun %s "fmt"\n", __func__, ##args)

#if DEBUG > 1
#define ddbg_print(fmt, args...) \
	printk(KERN_INFO "fun %s "fmt"\n", __func__, ##args)
#else
#define ddbg_print(fmt, args...) ;
#endif

#else

#define dbg_print(fmt, args...)  ;
#define ddbg_print(fmt, args...) ;

#endif

static void _camera_lines_lowpower_mode(void);
static void _camera_lines_func_mode(void);
static int camera_dev_open(struct inode *inode, struct file *file);
static int camera_dev_release(struct inode *inode, struct file *file);
static int camera_dev_ioctl(struct inode *inode, struct file *file,
	unsigned int cmd, unsigned long arg);
static int camera_misc_probe(struct platform_device *device);
static int camera_misc_remove(struct platform_device *device);
static int cam_misc_suspend(struct platform_device *pdev, pm_message_t state);
static int cam_misc_resume(struct platform_device *pdev);
static void cam_misc_disableClk(void);
static void cam_misc_enableClk(unsigned long clock);

static int gpio_reset;
static int gpio_powerdown;
static int isp_count_local;
static struct regulator *regulator;
static int bHaveResetGpio;
static int bHavePowerDownGpio;

static const struct file_operations camera_dev_fops = {
	.open       = camera_dev_open,
	.release    = camera_dev_release,
	.ioctl      = camera_dev_ioctl
};

static struct miscdevice cam_misc_device0 = {
	.minor      = MISC_DYNAMIC_MINOR,
	.name       = "camera0",
	.fops       = &camera_dev_fops
};

static struct platform_driver cam_misc_driver = {
	.probe = camera_misc_probe,
	.remove = camera_misc_remove,
	.suspend = cam_misc_suspend,
	.resume = cam_misc_resume,
	.driver = {
		.name = CAMERA_DEVICE0,
	},
};

static int camera_dev_open(struct inode *inode, struct file *file)
{
	return 0;
}

static int camera_dev_release(struct inode *inode, struct file *file)
{
	return 0;
}

static void cam_misc_disableClk()
{
	/* powerup the isp/cam domain on the 3410 */
	if (isp_count_local == 0) {
		isp_get();
		isp_count_local++;
	}
	isp_power_settings(1);
	isp_set_xclk(0, 0);

	/* Need to make sure that all encounters of the
	   isp clocks are disabled*/
	while (isp_count_local > 0) {
		isp_put();
		isp_count_local--;
	}
}

static void cam_misc_enableClk(unsigned long clock)
{
	/* powerup the isp/cam domain on the 3410 */
	if (isp_count_local == 0) {
		isp_get();
		isp_count_local++;
	}
	isp_power_settings(1);
	isp_set_xclk(clock, 0);
}

static int cam_misc_suspend(struct platform_device *pdev, pm_message_t state)
{
	dbg_print("------------- MOT CAM _ SUSPEND CALLED ---------\n");

	/* Checking to make sure that camera is on */
	if (bHavePowerDownGpio && (gpio_get_value(gpio_powerdown) == 0)) {
		/* If camera is on, need to pull standby pin high */
		gpio_set_value(gpio_powerdown, 1);
	}

	/* Per sensor specification, need a minimum of 20 uS of time when
	   standby pin pull high and when the master clock is turned off  */
	msleep(5);

	if (isp_count_local > 0) {
		/* Need to make sure that all encounters of the
		   isp clocks are disabled*/
		cam_misc_disableClk();
		dbg_print("CAMERA_MISC turned off MCLK done\n");
	}

	_camera_lines_lowpower_mode();

	/* Turn off power */
	if (regulator != NULL) {
		regulator_disable(regulator);
		regulator_put(regulator);
		regulator = NULL;
	}

	return 0;
}

static int cam_misc_resume(struct platform_device *pdev)
{
	return 0;
}

static int camera_dev_ioctl(struct inode *inode, struct file *file,
	unsigned int cmd, unsigned long arg)
{
	int rc = 0;

	if (!bHaveResetGpio) {
		ddbg_print("Requesting reset gpio\n");
		rc = gpio_request(gpio_reset, "camera reset");
		if (!rc)
			bHaveResetGpio = 1;
	}
	if (!bHavePowerDownGpio) {
		ddbg_print("Requesting powerdown gpio\n");
		rc = gpio_request(gpio_powerdown, "camera powerdown");
		if (!rc)
			bHavePowerDownGpio = 1;
	}
	ddbg_print("camera ioctl cmd = %u, arg = %lu\n", cmd, arg);

	switch (cmd) {
	case CAMERA_RESET_WRITE:
		if (bHaveResetGpio) {
			gpio_direction_output(gpio_reset, 0);
			gpio_set_value(gpio_reset, (arg ? 1 : 0));
			dbg_print("CAMERA_MISC set RESET line to %u\n",
				(arg ? 1 : 0));
		}

		if (!bHaveResetGpio) {
			err_print ("CAMERA_MISC: gpio_request () failed. \
				rc = %d; cmd = %d; arg = %lu", rc, cmd, arg);
			return -EIO;
		}
		break;

	case CAMERA_POWERDOWN_WRITE:
		if (bHavePowerDownGpio) {
			gpio_direction_output(gpio_powerdown, 0);
			if (0 == arg)
				gpio_set_value(gpio_powerdown, 0);
			else
				gpio_set_value(gpio_powerdown, 1);
			dbg_print("CAMERA_MISC set POWERDOWN line to %u\n",
				(arg ? 1 : 0));
		}
		if (!bHavePowerDownGpio) {
			err_print ("CAMERA_MISC: gpio_request () failed. \
				rc = %d; cmd = %u; arg = %lu", rc, cmd, arg);
			return -EIO;
		}
		break;

	case CAMERA_CLOCK_DISABLE:
		cam_misc_disableClk();
		dbg_print("CAMERA_MISC turned off MCLK done\n");
		break;

	case CAMERA_CLOCK_ENABLE:
		cam_misc_enableClk(arg);
		dbg_print("CAMERA_MISC set MCLK to %d\n", (int) arg);
		break;
	case CAMERA_AVDD_POWER_ENABLE:
	case CAMERA_AVDD_POWER_DISABLE:
		if (arg == 1) {
			/* turn on digital power */
			if (regulator != NULL) {
				err_print("Already have regulator\n");
			} else {
				regulator = regulator_get(NULL, "vcam");
				if (IS_ERR(regulator)) {
					err_print("Cannot get vcam regulator, \
						err=%ld\n", PTR_ERR(regulator));
					return PTR_ERR(regulator);
				} else {
					regulator_enable(regulator);
					msleep(5);
				}
			}
			_camera_lines_func_mode();
		} else {
			/* Turn off power */
			if (regulator != NULL) {
				regulator_disable(regulator);
				regulator_put(regulator);
				regulator = NULL;
			} else {
				err_print("Regulator for vcam is not \
					initialized\n");
				return -EIO;
			}
			_camera_lines_lowpower_mode();
		}
		break;
	default:
		err_print("CAMERA_MISC received unsupported cmd; cmd = %u\n",
			cmd);
		return -EIO;
		break;
	}

	return 0;
}

static int __init camera_misc_probe(struct platform_device *device)
{
	printk(KERN_INFO "camera_misc_probe - probe function called\n");

	/* put the GPIO64 (camera powerdown) to default state
	Its getting altered by Jordan aptina sensor probe */
	omap_writew(0x001C, 0x480020D0);

	gpio_reset = get_gpio_by_name("gpio_cam_reset");
	gpio_powerdown = get_gpio_by_name("gpio_cam_pwdn");
	ddbg_print("gpio_cam_reset=%d gpio_cam_pwdn=%d", gpio_reset,
		gpio_powerdown);

	_camera_lines_lowpower_mode();

	/* Standby needs to be high */
	if (!gpio_request(gpio_powerdown, "camera powerdown")) {
		gpio_direction_output(gpio_powerdown, 0);
		gpio_set_value(gpio_powerdown, 1);

		/* Do not free gpio so that it cannot be overwritten */
		bHavePowerDownGpio = 1 ;
	}

	if (misc_register(&cam_misc_device0)) {
		err_print("error in register camera misc device!\n");
		return -EIO;
	}

	return 0;
}

static int camera_misc_remove(struct platform_device *device)
{
	misc_deregister(&cam_misc_device0);

	if (!bHaveResetGpio) {
		gpio_free(gpio_reset);
		bHaveResetGpio = 0;
	}
	if (!bHavePowerDownGpio) {
		gpio_free(gpio_powerdown);
		bHavePowerDownGpio = 0;
	}

	return 0;
}

extern int camise_add_i2c_device(void);

static int __init camera_misc_init(void)
{
	int ret=0;
	ret=platform_driver_register(&cam_misc_driver);
	camise_add_i2c_device();
	return ret;
}

static void __exit camera_misc_exit(void)
{
	platform_driver_unregister(&cam_misc_driver);
}


void _camera_lines_lowpower_mode(void)
{
	omap_writew(0x0007, 0x4800210C);   /* HSYNC */
	omap_writew(0x0007, 0x4800210E);   /* VSYNC */
	omap_writew(0x001F, 0x48002110);   /* MCLK */
	omap_writew(0x0007, 0x48002112);   /* PCLK */
	omap_writew(0x3A04, 0x48002114);   /* FLD */
	omap_writew(0x0007, 0x4800211A);   /* CAM_D2 */
	omap_writew(0x0007, 0x4800211C);   /* CAM_D3 */
	omap_writew(0x0007, 0x4800211E);   /* CAM_D4 */
	omap_writew(0x0007, 0x48002120);   /* CAM_D5 */
	omap_writew(0x0007, 0x48002122);   /* CAM_D6 */
	omap_writew(0x0007, 0x48002124);   /* CAM_D7 */
	omap_writew(0x001F, 0x48002126);   /* CAM_D8 */
	omap_writew(0x001F, 0x48002128);   /* CAM_D9 */
	omap_writew(0x001F, 0x4800212A);   /* CAM_D10 */
	omap_writew(0x001F, 0x4800212C);   /* CAM_D11 */
}

void _camera_lines_func_mode(void)
{
	omap_writew(0x0118, 0x4800210C);  /* HSYNC */
	omap_writew(0x0118, 0x4800210E);  /* VSYNC */
	omap_writew(0x0000, 0x48002110);  /* MCLK */
	omap_writew(0x0118, 0x48002112);  /* PCLK */
	omap_writew(0x0004, 0x48002114);  /* FLD */
	omap_writew(0x0100, 0x4800211A);  /* CAM_D2 */
	omap_writew(0x0100, 0x4800211C);  /* CAM_D3 */
	omap_writew(0x0100, 0x4800211E);  /* CAM_D4 */
	omap_writew(0x0100, 0x48002120);  /* CAM_D5 */
	omap_writew(0x0100, 0x48002122);  /* CAM_D6 */
	omap_writew(0x0100, 0x48002124);  /* CAM_D7 */
	omap_writew(0x0100, 0x48002126);  /* CAM_D8 */
	omap_writew(0x0100, 0x48002128);  /* CAM_D9 */
	omap_writew(0x0100, 0x4800212A);  /* CAM_D10 */
	omap_writew(0x0100, 0x4800212C);  /* CAM_D11 */
}

module_init(camera_misc_init);
module_exit(camera_misc_exit);

MODULE_VERSION("1.1");
MODULE_DESCRIPTION("Motorola Camera Driver Backport");
MODULE_AUTHOR("Tanguy Pruvot, From Motorola Open Sources");
MODULE_LICENSE("GPL");
