#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/mm.h>
#include <linux/i2c.h>
#include <linux/regulator/consumer.h>
#include <plat/mux.h>
#include <plat/board-mapphone.h>
#include <plat/omap-pm.h>
#include <plat/control.h>
#include <linux/string.h>
#include <linux/gpio_mapping.h>
#include <plat/resource.h>

#ifdef CONFIG_ARM_OF
# include <mach/dt_path.h>
# include <asm/prom.h>
#endif

#if defined(CONFIG_VIDEO_OMAP3)
# include <media/v4l2-int-device.h>
# include <../drivers/media/video/omap34xxcam.h>
# include <../drivers/media/video/isp/ispreg.h>
# include <../drivers/media/video/isp/isp.h>
# include <../drivers/media/video/isp/ispcsi2.h>
# if defined(CONFIG_VIDEO_MT9P012) || defined(CONFIG_VIDEO_MT9P012_MODULE)
#  include <media/mt9p012.h>
#  define MT9P012_XCLK_48MHZ		48000000
# endif
# if defined(CONFIG_VIDEO_OV8810) || defined(CONFIG_VIDEO_OV8810_MODULE)
#  include <media/ov8810.h>
#  if defined(CONFIG_LEDS_FLASH_RESET)
#   include <linux/spi/cpcap.h>
#   include <linux/spi/cpcap-regbits.h>
#  endif
#  include "camise.h"
# endif
#endif

#define CAM_IOMUX_SAFE_MODE (OMAP343X_PADCONF_PULL_UP | \
				OMAP343X_PADCONF_PUD_ENABLED | \
				OMAP343X_PADCONF_MUXMODE7)
#define CAM_IOMUX_SAFE_MODE_INPUT (OMAP343X_PADCONF_INPUT_ENABLED | \
				OMAP343X_PADCONF_PULL_UP | \
				OMAP343X_PADCONF_PUD_ENABLED | \
				OMAP343X_PADCONF_MUXMODE7)
#define CAM_IOMUX_FUNC_MODE (OMAP343X_PADCONF_INPUT_ENABLED | \
				OMAP343X_PADCONF_MUXMODE0)
#define CAM_IOMUX_SAFE_MODE_DOWN (OMAP343X_PADCONF_PULL_DOWN | \
				OMAP343X_PADCONF_PUD_ENABLED | \
				OMAP343X_PADCONF_MUXMODE7)
#define CAM_IOMUX_FUNC_MODE4 (OMAP343X_PADCONF_PULL_UP | \
				OMAP343X_PADCONF_PUD_ENABLED | \
				OMAP343X_PADCONF_MUXMODE4)

#define CAM_MAX_REGS 5
#define CAM_MAX_REG_NAME_LEN 8
#define SENSOR_POWER_OFF     0
#define SENSOR_POWER_STANDBY 1

static char regulator_list[CAM_MAX_REGS][CAM_MAX_REG_NAME_LEN];

static int cam_reset_gpio   = -1;
static int cam_standby_gpio = -1;
static unsigned int is_smart_cam = 0;

struct camise_capture_size {
	unsigned long width;
	unsigned long height;
};

#include "../symsearch/symsearch.h"
SYMSEARCH_DECLARE_FUNCTION_STATIC(
	void, ss_omap_ctrl_writew, u16 val, u16 offset);

const static struct camise_capture_size camise_sizes_1[] = {
	{  176, 144 }, /* QCIF for Video Recording */
	{  320, 240 }, /* QVGA for Preview and Video Recording */
	{  352, 288 }, /* CIF for Video Recording */
	{  480, 360 }, /* Bigger Viewfinder */
	{  640, 480 }, /* 4X BINNING */
	{  512, 1024},
	/*{  512, 2048},*/ /* Jpeg Capture Resolution */
	{  640, 2048}, /* JPEG Catpure Resolution */
	{  848, 480 }, /* Support for WVGA preview */
};

const static struct camise_capture_size camise_sizes_2[] = {
	{  176, 144 }, /* QCIF for Video Recording */
	{  320, 240 }, /* QVGA for Preview and Video Recording */
	{  352, 288 }, /* CIF for Video Recording */
	{  480, 360 }, /* Bigger Viewfinder */
	{  640, 480 }, /* 4X BINNING */
	{  512, 1024},
	{  768, 1024}, /* JPEG Catpure Resolution */
};

static struct omap34xxcam_sensor_config camise_cam_hwc = {
	.sensor_isp = 1,
	.xclk = OMAP34XXCAM_XCLK_A,
	.capture_mem = PAGE_ALIGN(2048 * 1536 * 2) * 4,
};

static struct isp_interface_config camise_if_config = {
	.ccdc_par_ser = ISP_PARLL,
	.dataline_shift = 0x2, /* 8bit sensor using D11 to D4. */
	.hsvs_syncdetect = ISPCTRL_SYNC_DETECT_VSRISE,
	.strobe = 0x0,
	.prestrobe = 0x0,
	.shutter = 0x0,
	.wenlog = ISPCCDC_CFG_WENLOG_AND,
	.dcsub = 0,	 /* Disabling DCSubtract function */
	/*.raw_fmt_in = ISPCCDC_INPUT_FMT_GR_BG,*/
	.wait_bayer_frame = 1,
	.wait_yuv_frame = 1,
	/*.cam_mclk = 216000000,
	.cam_mclk_src_div = OMAP_MCAM_SRC_DIV_36xx, */
	.cam_mclk = 216000000,
	.cam_mclk_src_div = OMAP_MCAM_SRC_DIV,
	.raw_fmt_in = ISPCCDC_INPUT_FMT_RG_GB,
	.u.par.par_bridge = 0x3,
	.u.par.par_clk_pol = 0x0,
};

int mapphone_camera_reg_power(bool enable)
{
	static struct regulator *regulator[CAM_MAX_REGS];
	static bool reg_resource_acquired;
	int i, error;

	error = 0;

	if (reg_resource_acquired == false && enable) {
		/* get list of regulators and enable*/
		for (i = 0; i < CAM_MAX_REGS && \
			regulator_list[i][0] != 0; i++) {
			printk(KERN_INFO "%s - enable %s\n",\
				__func__,\
				regulator_list[i]);
			regulator[i] = regulator_get(NULL, regulator_list[i]);
			if (IS_ERR(regulator[i])) {
				pr_err("%s: Cannot get %s "\
					"regulator, err=%ld\n",\
					__func__, regulator_list[i],
					PTR_ERR(regulator[i]));
				error = PTR_ERR(regulator[i]);
				regulator[i] = NULL;
				break;
			}
			if (regulator_enable(regulator[i]) != 0) {
				pr_err("%s: Cannot enable regulator: %s \n",
					__func__, regulator_list[i]);
				error = -EIO;
				regulator_put(regulator[i]);
				regulator[i] = NULL;
				break;
			}
		}

		if (error != 0 && i > 0) {
			/* return all acquired regulator resources if error */
			while (--i && regulator[i]) {
				regulator_disable(regulator[i]);
				regulator_put(regulator[i]);
				regulator[i] = NULL;
			}
		} else
			reg_resource_acquired = true;

	} else if (reg_resource_acquired && !enable) {
		/* get list of regulators and disable*/
		for (i = 0; i < CAM_MAX_REGS && \
			regulator_list[i][0] != 0; i++) {
			printk(KERN_INFO "%s - disable %s\n",\
					 __func__,\
					 regulator_list[i]);
			if (regulator[i]) {
				regulator_disable(regulator[i]);
				regulator_put(regulator[i]);
				regulator[i] = NULL;
			}
		}

		reg_resource_acquired = false;
	} else {
		pr_err("%s: Invalid regulator state\n", __func__);
		error = -EIO;
	}
	return error;
}

/* We can't change the IOMUX config after bootup
 * with the current pad configuration architecture,
 * the next two functions are hack to configure the
 * camera pads at runtime to save power in standby.
 * For phones don't have MIPI camera support, like
 * Ruth, Tablet P2,P3 */

int mapphone_camera_lines_safe_mode(void)
{
	SYMSEARCH_BIND_FUNCTION_TO(camera, omap_ctrl_writew, ss_omap_ctrl_writew);
	ss_omap_ctrl_writew(CAM_IOMUX_SAFE_MODE_INPUT, 0x011a);
	ss_omap_ctrl_writew(CAM_IOMUX_SAFE_MODE_INPUT, 0x011c);
	ss_omap_ctrl_writew(CAM_IOMUX_SAFE_MODE_INPUT, 0x011e);
	ss_omap_ctrl_writew(CAM_IOMUX_SAFE_MODE_INPUT, 0x0120);
	ss_omap_ctrl_writew(CAM_IOMUX_SAFE_MODE, 0x0122);
	ss_omap_ctrl_writew(CAM_IOMUX_SAFE_MODE, 0x0124);
	ss_omap_ctrl_writew(CAM_IOMUX_SAFE_MODE, 0x0126);
	ss_omap_ctrl_writew(CAM_IOMUX_SAFE_MODE, 0x0128);
	if (cam_standby_gpio >= 0)
		ss_omap_ctrl_writew(CAM_IOMUX_SAFE_MODE_DOWN, 0x20D0);
	return 0;
}

int mapphone_camera_lines_func_mode(void)
{
	SYMSEARCH_BIND_FUNCTION_TO(camera, omap_ctrl_writew, ss_omap_ctrl_writew);
	ss_omap_ctrl_writew(CAM_IOMUX_FUNC_MODE, 0x011a);
	ss_omap_ctrl_writew(CAM_IOMUX_FUNC_MODE, 0x011c);
	ss_omap_ctrl_writew(CAM_IOMUX_FUNC_MODE, 0x011e);
	ss_omap_ctrl_writew(CAM_IOMUX_FUNC_MODE, 0x0120);
	ss_omap_ctrl_writew(CAM_IOMUX_FUNC_MODE, 0x0122);
	ss_omap_ctrl_writew(CAM_IOMUX_FUNC_MODE, 0x0124);
	ss_omap_ctrl_writew(CAM_IOMUX_FUNC_MODE, 0x0126);
	ss_omap_ctrl_writew(CAM_IOMUX_FUNC_MODE, 0x0128);
	if (cam_standby_gpio >= 0)
		ss_omap_ctrl_writew(CAM_IOMUX_FUNC_MODE4, 0x20D0);
	return 0;
}

static void mapphone_lock_cpufreq(int lock)
{
	static struct device *ov_dev;
	static int flag;

	if (lock == 1) {
		resource_request("vdd1_opp",
			ov_dev, omap_pm_get_max_vdd1_opp());
		flag = 1;
	} else {
		if (flag == 1) {
			resource_release("vdd1_opp", ov_dev);
			flag = 0;
		}
	}
}

static int camise_get_capture_size(int index,
				unsigned long *w,
				unsigned long *h)
{
	int max_size = is_smart_cam ? ARRAY_SIZE(camise_sizes_2) :
			ARRAY_SIZE(camise_sizes_1);

	if (is_smart_cam && index < max_size) {
		*h = camise_sizes_2[index].height;
		*w = camise_sizes_2[index].width;
	} else if (index < max_size) {
		*h = camise_sizes_1[index].height;
		*w = camise_sizes_1[index].width;
	}

	return max_size;
}

static int camise_sensor_set_prv_data(void *priv)
{
	struct omap34xxcam_hw_config *hwc = priv;
	hwc->u.sensor.xclk = camise_cam_hwc.xclk;
	hwc->u.sensor.sensor_isp = camise_cam_hwc.sensor_isp;
	hwc->u.sensor.capture_mem = camise_cam_hwc.capture_mem;
	hwc->dev_index = 1;
	hwc->dev_minor = CAM_DEVICE_SOC;
	hwc->dev_type = OMAP34XXCAM_SLAVE_SENSOR;
	return 0;
}

static void camise_if_configure(void)
{
	isp_configure_interface(&camise_if_config);
}

static int camise_sensor_power_set(struct device *dev, enum v4l2_power power)
{
	static enum v4l2_power previous_power = V4L2_POWER_OFF;
	int error = 0;

	switch (power) {
	case V4L2_POWER_OFF:
		if (previous_power != V4L2_POWER_OFF) {
			printk(KERN_DEBUG "%s: power off\n", __func__);
			/* Power Down Sequence
			Need to free gpios since other drivers may request them
			*/
			if (cam_standby_gpio >= 0) {
				gpio_set_value(cam_standby_gpio, 1);
				gpio_free(cam_standby_gpio);
				cam_standby_gpio = -1;
			}

			if (cam_reset_gpio >= 0) {
				gpio_set_value(cam_reset_gpio, 0);
				gpio_free(cam_reset_gpio);
				cam_reset_gpio = -1;
			}

			/* turn off ISP clock */
			isp_set_xclk(0, OMAP34XXCAM_XCLK_A);
			msleep(1);

			error = mapphone_camera_reg_power(false);
			if (error != 0) {
				pr_err("%s: Failed to power off regulators\n",
				__func__);
			}
			mapphone_camera_lines_safe_mode();
		}
		break;

	case V4L2_POWER_ON:
		printk(KERN_DEBUG "%s: power on\n", __func__);

		if (previous_power == V4L2_POWER_OFF) {
			mapphone_camera_lines_func_mode();

			/* request for the GPIO's */
			cam_reset_gpio = get_gpio_by_name("gpio_cam_reset");
			if (cam_reset_gpio >= 0) {
				printk(KERN_INFO "cam_reset_gpio %d\n",
					cam_reset_gpio);
				if (gpio_request(cam_reset_gpio,
					"camera reset") != 0) {
					printk(KERN_ERR "Failed to req cam reset\n");
					goto failed_cam_reset_gpio;
				}
				gpio_direction_output(cam_reset_gpio, 0);
			}

			cam_standby_gpio = get_gpio_by_name("gpio_cam_pwdn");
			if (cam_standby_gpio >= 0) {
				printk(KERN_INFO "cam_standby_gpio %d\n",
					cam_standby_gpio);
				if (gpio_request(cam_standby_gpio,
					"camera standby") != 0) {
					printk(KERN_ERR "Failed to req cam standby\n");
					goto failed_cam_standby_gpio;
				}
				gpio_direction_output(cam_standby_gpio, 0);
				gpio_set_value(cam_standby_gpio, 1);
			}

			/* turn on ISP clock */
			isp_set_xclk(CAMISE_XCLK_24MHZ, OMAP34XXCAM_XCLK_A);
			/* Power Up Sequence */
			camise_if_configure();
			msleep(10);

			/* Turn on power */
			error = mapphone_camera_reg_power(true);
			if (error != 0) {
				pr_err("%s: Failed to power on regulators\n",
					__func__);
				goto out;
			}
			msleep(15);

			if (cam_standby_gpio >= 0) {
				/* Bring camera out of standby */
				gpio_set_value(cam_standby_gpio, 0);
				msleep(10);
			}

			if (cam_reset_gpio >= 0) {
				/* release reset */
				gpio_set_value(cam_reset_gpio, 0);
				msleep(10);
				gpio_set_value(cam_reset_gpio, 1);
				msleep(10);
			}

			/* Wait 20ms per OVT recommendation */
			msleep(20);
		}
		break;
	default:
		break;
	}

	previous_power = power;
	return 0;
out:
	isp_set_xclk(0, OMAP34XXCAM_XCLK_A);
failed_cam_standby_gpio:
	if (cam_standby_gpio >= 0) {
		gpio_free(cam_standby_gpio);
		cam_standby_gpio = -1;
	}
failed_cam_reset_gpio:
	if (cam_reset_gpio >= 0) {
		gpio_free(cam_reset_gpio);
		cam_reset_gpio = -1;
	}
	return error;
}

struct camise_platform_data mapphone_camise_platform_data = {
	.power_set      = camise_sensor_power_set,
	.priv_data_set  = camise_sensor_set_prv_data,
	.lock_cpufreq   = mapphone_lock_cpufreq,
	.if_config      = camise_if_configure,
	.get_size       = camise_get_capture_size,
};

struct i2c_board_info __initdata mapphone_i2c_bus_camise_board_info = {
	I2C_BOARD_INFO("camise", CAMISE_I2C_ADDR),
	.platform_data = &mapphone_camise_platform_data,
};

int camise_add_i2c_device(void)
{
	struct i2c_board_info info;
	struct i2c_adapter *adapter;
	struct i2c_client *client;

	memset(&info, 0, sizeof(struct i2c_board_info));
	info.addr = CAMISE_I2C_ADDR;
	strlcpy(info.type, "camise", I2C_NAME_SIZE);
	info.platform_data = &mapphone_camise_platform_data;

	adapter = i2c_get_adapter(3);
	if (!adapter) {
		printk(KERN_DEBUG "can't get i2c adapter\n");
		return -ENODEV;
	}

	client = i2c_new_device(adapter, &info);
	i2c_put_adapter(adapter);
	if (!client) {
		printk(KERN_DEBUG "can't add i2c device\n");
		return -ENODEV;
	}

	printk(KERN_INFO "camise_add_i2c_device: done\n");

	return 0;
}
