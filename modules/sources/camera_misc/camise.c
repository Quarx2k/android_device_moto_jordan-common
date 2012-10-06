/*
 * drivers/media/video/camise.c
 *
 * Motorola generic camera driver
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
#include <media/v4l2-int-device.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/regulator/consumer.h>
#include <linux/gpio.h>
#include "camise.h"
#include <../drivers/media/video/omap34xxcam.h>
#include <../drivers/media/video/isp/isp.h>
#include <../drivers/media/video/isp/ispreg.h>
#include <linux/io.h>
#include <linux/mm.h>
#include <../arch/arm/plat-omap/include/plat/board-mapphone.h>
#define CAMISE_DRIVER_NAME "camise"

/* list of image formats supported by camise sensor */
const static struct v4l2_fmtdesc camise_formats[] = {
/*
	{
		.description	= "Bayer10 (GrR/BGb)",
		.pixelformat	= V4L2_PIX_FMT_SGRBG10,
	},
	{
		.description	= "Bayer10 pattern (GrR/BGb)",
		.pixelformat	= V4L2_PIX_FMT_PATT,
	},
 */
	{
		.description	= "Walking 1's pattern",
		.pixelformat	= V4L2_PIX_FMT_W1S_PATT,
	},

	{
		.description	= "YUYV Pattern",
		.pixelformat	=  V4L2_PIX_FMT_YUYV,
	},

/*
	{
		.description = "UYVY, packed",
		.pixelformat = V4L2_PIX_FMT_UYVY,
	}
*/

};

#define NUM_CAPTURE_FORMATS ARRAY_SIZE(camise_formats)


/* SOC Sensor has 8/16/32 registers */
#define CAMISE_8BIT			1
#define CAMISE_16BIT			2
#define CAMISE_32BIT			4

#define REG_MODEL_ID 0x300A

#define SENSOR_NOT_DETECTED 0
#define SENSOR_DETECTING 1
#define SENSOR_DETECTED 2

#define I2C_M_WR 0

#define CAMERA_DEVICE0  "/dev/camera0"



/**
 * struct camise_sensor_id
 */
struct camise_sensor_id {
	u16 revision ;
	u16 model ;
	u16 mfr ;
};

struct camise_sensor {
	struct device *dev;
	struct camise_platform_data *pdata;
	struct v4l2_int_device *v4l2_int_device;
	struct i2c_client *i2c_client;
	struct v4l2_pix_format pix;
	struct v4l2_rect crop_rect;
	struct camise_sensor_id sensor_id;
	int state;
};

static struct camise_sensor camise = {
	.sensor_id = {
		.revision = 0,
		.model = 0,
		.mfr = 0
	},

};

static void cam_misc_platform_release(struct device *device)
{
}

static struct platform_device cam_misc_device = {
	.name = CAMERA_DEVICE0,
	.id = -1,
	.dev = {
		.release = cam_misc_platform_release,
	}
};
/*
static struct i2c_driver camise_i2c_driver;
*/

static enum v4l2_power current_power_state;

/*
 * struct vcontrol - Video controls
 * @v4l2_queryctrl: V4L2 VIDIOC_QUERYCTRL ioctl structure
 * @current_value: current value of this control
 */
static struct vcontrol {
	struct v4l2_queryctrl qc;
	int current_value;
} video_control[] = {};

/*
 * find_vctrl - Finds the requested ID in the video control structure array
 * @id: ID of control to search the video control array.
 *
 * Returns the index of the requested ID from the control structure array
 */
static int find_vctrl(int id)
{
	int i = 0;

	if (id < V4L2_CID_BASE)
		return -EDOM;

	for (i = (ARRAY_SIZE(video_control) - 1); i >= 0; i--)
		if (video_control[i].qc.id == id)
			break;
	if (i < 0)
		i = -EINVAL;
	return i;
}

/*
 * Configuring camise.
 *
 * Returns zero if succesful, or non-zero otherwise
 *
 */
static int camise_configure(struct v4l2_int_device *s)
{

	return 0;
} /* end camise_configure() */

/* To get the cropping capabilities of camise
 * Returns zero if successful, or non-zero otherwise.
 */
static int ioctl_cropcap(struct v4l2_int_device *s,
			 struct v4l2_cropcap *cropcap)
{
	return 0;
}

/* To get the current crop window for of camise
 * Returns zero if successful, or non-zero otherwise.
 */
static int ioctl_g_crop(struct v4l2_int_device *s, struct  v4l2_crop *crop)
{
	return 0;
}

/* To set the crop window for of camise
 * Returns zero if successful, or non-zero otherwise.
 */
static int ioctl_s_crop(struct v4l2_int_device *s, struct  v4l2_crop *crop)
{
	return 0;
}

/*
 * ioctl_queryctrl - V4L2 sensor interface handler for VIDIOC_QUERYCTRL ioctl
 * @s: pointer to standard V4L2 device structure
 * @qc: standard V4L2 VIDIOC_QUERYCTRL ioctl structure
 *
 * If the requested control is supported, returns the control information
 * from the video_control[] array.  Otherwise, returns -EINVAL if the
 * control is not supported.
 */
static int ioctl_queryctrl(struct v4l2_int_device *s,
						struct v4l2_queryctrl *qc)
{
	int i;

	i = find_vctrl(qc->id);
	if (i == -EINVAL)
		qc->flags = V4L2_CTRL_FLAG_DISABLED;

	if (i < 0)
		return -EINVAL;

	*qc = video_control[i].qc;
	return 0;
}

/*
 * ioctl_g_ctrl - V4L2 sensor interface handler for VIDIOC_G_CTRL ioctl
 * @s: pointer to standard V4L2 device structure
 * @vc: standard V4L2 VIDIOC_G_CTRL ioctl structure
 *
 * If the requested control is supported, returns the control's current
 * value from the video_control[] array.  Otherwise, returns -EINVAL
 * if the control is not supported.
 */

static int ioctl_g_ctrl(struct v4l2_int_device *s,
			struct v4l2_control *vc)
{
	return 0;
}

/*
 * ioctl_s_ctrl - V4L2 sensor interface handler for VIDIOC_S_CTRL ioctl
 * @s: pointer to standard V4L2 device structure
 * @vc: standard V4L2 VIDIOC_S_CTRL ioctl structure
 *
 * If the requested control is supported, sets the control's current
 * value in HW (and updates the video_control[] array).  Otherwise,
 * returns -EINVAL if the control is not supported.
 */
static int ioctl_s_ctrl(struct v4l2_int_device *s,
			     struct v4l2_control *vc)
{
	return 0;
}

/*
 * ioctl_enum_fmt_cap - Implement the CAPTURE buffer VIDIOC_ENUM_FMT ioctl
 * @s: pointer to standard V4L2 device structure
 * @fmt: standard V4L2 VIDIOC_ENUM_FMT ioctl structure
 *
 * Implement the VIDIOC_ENUM_FMT ioctl for the CAPTURE buffer type.
 */
 static int ioctl_enum_fmt_cap(struct v4l2_int_device *s,
				   struct v4l2_fmtdesc *fmt)
{
	int index = fmt->index;
	enum v4l2_buf_type type = fmt->type;

	memset(fmt, 0, sizeof(*fmt));
	fmt->index = index;
	fmt->type = type;

	switch (fmt->type) {
	case V4L2_BUF_TYPE_VIDEO_CAPTURE:
		if (index >= NUM_CAPTURE_FORMATS)
			return -EINVAL;
	break;
	default:
		return -EINVAL;
	}

	fmt->flags = camise_formats[index].flags;
	strlcpy(fmt->description, camise_formats[index].description,
					sizeof(fmt->description));
	fmt->pixelformat = camise_formats[index].pixelformat;

	return 0;
}

/*
 * ioctl_try_fmt_cap - Implement the CAPTURE buffer VIDIOC_TRY_FMT ioctl
 * @s: pointer to standard V4L2 device structure
 * @f: pointer to standard V4L2 VIDIOC_TRY_FMT ioctl structure
 *
 * Implement the VIDIOC_TRY_FMT ioctl for the CAPTURE buffer type.  This
 * ioctl is used to negotiate the image capture size and pixel format
 * without actually making it take effect.
 */

static int ioctl_try_fmt_cap(struct v4l2_int_device *s,
			     struct v4l2_format *f)
{
	/* TODO: NEED TO IMPLEMENT */
	return 0;
}

/*
 * ioctl_s_fmt_cap - V4L2 sensor interface handler for VIDIOC_S_FMT ioctl
 * @s: pointer to standard V4L2 device structure
 * @f: pointer to standard V4L2 VIDIOC_S_FMT ioctl structure
 *
 * If the requested format is supported, configures the HW to use that
 * format, returns error code if format not supported or HW can't be
 * correctly configured.
 */
 static int ioctl_s_fmt_cap(struct v4l2_int_device *s,
				struct v4l2_format *f)
{
	/* TODO:
	struct ov3640_sensor *sensor = s->priv;
	struct v4l2_pix_format *pix = &f->fmt.pix;
	int rval;

	rval = ioctl_try_fmt_cap(s, f);
	if (rval)
		return rval;

	sensor->pix = *pix;
	*/
	return 0;
}

/*
 * ioctl_g_fmt_cap - V4L2 sensor interface handler for ioctl_g_fmt_cap
 * @s: pointer to standard V4L2 device structure
 * @f: pointer to standard V4L2 v4l2_format structure
 *
 * Returns the sensor's current pixel format in the v4l2_format
 * parameter.
 */
static int ioctl_g_fmt_cap(struct v4l2_int_device *s,
				struct v4l2_format *f)
{
/* TODO:
	struct ov3640_sensor *sensor = s->priv;
	f->fmt.pix = sensor->pix;
*/
	return 0;
}

/*
 * ioctl_g_parm - V4L2 sensor interface handler for VIDIOC_G_PARM ioctl
 * @s: pointer to standard V4L2 device structure
 * @a: pointer to standard V4L2 VIDIOC_G_PARM ioctl structure
 *
 * Returns the sensor's video CAPTURE parameters.
 */
static int ioctl_g_parm(struct v4l2_int_device *s,
			     struct v4l2_streamparm *a)
{
/* TODO
	struct ov3640_sensor *sensor = s->priv;
	struct v4l2_captureparm *cparm = &a->parm.capture;

	if (a->type != V4L2_BUF_TYPE_VIDEO_CAPTURE)
		return -EINVAL;

	memset(a, 0, sizeof(*a));
	a->type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

	cparm->capability = V4L2_CAP_TIMEPERFRAME;
	cparm->timeperframe = sensor->timeperframe;
*/
	return 0;
}

/*
 * ioctl_s_parm - V4L2 sensor interface handler for VIDIOC_S_PARM ioctl
 * @s: pointer to standard V4L2 device structure
 * @a: pointer to standard V4L2 VIDIOC_S_PARM ioctl structure
 *
 * Configures the sensor to use the input parameters, if possible.  If
 * not possible, reverts to the old parameters and returns the
 * appropriate error code.
 */
static int ioctl_s_parm(struct v4l2_int_device *s,
			     struct v4l2_streamparm *a)
{
	return 0;
}

/*
 * ioctl_g_priv - V4L2 sensor interface handler for vidioc_int_g_priv_num
 * @s: pointer to standard V4L2 device structure
 * @p: void pointer to hold sensor's private data address
 *
 * Returns device's (sensor's) private data area address in p parameter
 */
static int ioctl_g_priv(struct v4l2_int_device *s, void *p)
{

	struct camise_sensor *sensor = s->priv;

	return sensor->pdata->priv_data_set(p);

}

/**
 * camise_read_reg - Read a value from a register in an soc sensor device
 * @client: i2c driver client structure
 * @data_length: length of data to be read
 * @reg: register address / offset
 * @val: stores the value that gets read
 *
 * Read a value from a register in an soc sensor device.
 * The value is returned in 'val'.
 * Returns zero if successful, or non-zero otherwise.
 */
static int camise_read_reg(struct i2c_client *client, u16 data_length, u16 reg,
								u32 *val)
{
	int err = 0;
	struct i2c_msg msg[1];
	unsigned char data[4];

	if (!client->adapter)
		return -ENODEV;

	msg->addr = client->addr;
	msg->flags = I2C_M_WR;
	msg->len = 2;
	msg->buf = data;

	/* High byte goes out first */
	data[0] = (u8) (reg >> 8);
	data[1] = (u8) (reg & 0xff);

	err = i2c_transfer(client->adapter, msg, 1);
	if (err >= 0) {
		mdelay(3);
		msg->flags = I2C_M_RD;
		msg->len = data_length;
		err = i2c_transfer(client->adapter, msg, 1);
	}
	if (err >= 0) {
		*val = 0;
		/* High byte comes first */
		if (data_length == 1)
			*val = data[0];
		else if (data_length == 2)
			*val = data[1] + (data[0] << 8);
		else
			*val = data[3] + (data[2] << 8) +
				(data[1] << 16) + (data[0] << 24);
		return 0;
	}
	dev_err(&client->dev, "read from offset 0x%x error %d\n", reg, err);

	return err;
}

/* Detect if an soc sensor is present, returns a negative error number if no
 * device is detected, or pidl as version number if a device is detected.
 */
static int camise_detect(struct v4l2_int_device *s)
{
	u32 model_id = 0;
	struct camise_sensor *sensor = s->priv;
	struct i2c_client *client = sensor->i2c_client;

	if (!client)
		model_id = -ENODEV;
	else {
		if (camise_read_reg(client,
			CAMISE_16BIT, REG_MODEL_ID, &model_id))
			model_id = -ENODEV;
	}
	return model_id;
}

/*
 * ioctl_s_power - V4L2 sensor interface handler for vidioc_int_s_power_num
 * @s: pointer to standard V4L2 device structure
 * @on: power state to which device is to be set
 *
 * Sets devices power state to requrested state, if possible.
 */
static int ioctl_s_power(struct v4l2_int_device *s, enum v4l2_power on)
{
	struct camise_sensor *sensor = s->priv;
	struct i2c_client *c = to_i2c_client(sensor->dev);
	int rval = 0;

	if (sensor->state == SENSOR_DETECTED) {
		pr_info(CAMISE_DRIVER_NAME " Sensor in detected mode\n");
		if (on == V4L2_POWER_ON &&
			current_power_state == V4L2_POWER_OFF)
			sensor->pdata->if_config();
		current_power_state = on;
		return rval;
	}

	switch (on) {
	case V4L2_POWER_ON:
		rval = sensor->pdata->power_set(sensor->dev, V4L2_POWER_ON);
		if (rval)
			break;
		if (current_power_state == V4L2_POWER_OFF)
			camise_configure(s);
		break;
	case V4L2_POWER_OFF:
		rval = sensor->pdata->power_set(sensor->dev, V4L2_POWER_OFF);
		if (sensor->state == SENSOR_DETECTING)
			sensor->state = SENSOR_DETECTED;
		break;
	default:
		rval =  -EINVAL;
	}

	if ((on == V4L2_POWER_ON) && (sensor->state == SENSOR_NOT_DETECTED)) {

		rval = camise_detect(s);
		if (rval < 0) {
			dev_err(&c->dev, "Unable to detect "
					CAMISE_DRIVER_NAME " sensor\n");
			sensor->state = SENSOR_NOT_DETECTED;
			sensor->pdata->power_set(sensor->dev, V4L2_POWER_OFF);
			return rval;
		}
		sensor->state = SENSOR_DETECTING;
		/* free up all the camise resources */
		sensor->pdata->power_set(sensor->dev, V4L2_POWER_OFF);
		platform_device_register(&cam_misc_device);
		pr_info(CAMISE_DRIVER_NAME " Sensor detected\n");
		rval = 0;
	}

	current_power_state = on;
	return rval;
}

/*
 * ioctl_init - V4L2 sensor interface handler for VIDIOC_INT_INIT
 * @s: pointer to standard V4L2 device structure
 *
 * Initialize the sensor device (call camise_configure())
 */
static int ioctl_init(struct v4l2_int_device *s)
{
	return 0;
}

/**
 * ioctl_dev_exit - V4L2 sensor interface handler for vidioc_int_dev_exit_num
 * @s: pointer to standard V4L2 device structure
 *
 * Delinitialise the dev. at slave detach.  The complement of ioctl_dev_init.
 */
static int ioctl_dev_exit(struct v4l2_int_device *s)
{
	return 0;
}

/**
 * ioctl_dev_init - V4L2 sensor interface handler for vidioc_int_dev_init_num
 * @s: pointer to standard V4L2 device structure
 *
 * Initialise the device when slave attaches to the master.  Returns 0 if
 * ov3640 device could be found, otherwise returns appropriate error.
 */
static int ioctl_dev_init(struct v4l2_int_device *s)
{
/* TODO:
	struct ov3640_sensor *sensor = s->priv;
	struct i2c_client *c = sensor->i2c_client;
	int err;

	err = ov3640_detect(c);
	if (err < 0) {
		dev_err(&c->dev, "Unable to detect " OV3640_DRIVER_NAME
								" sensor\n");
		return err;
	}

	sensor->ver = err;
	pr_info(OV3640_DRIVER_NAME " chip version 0x%02x detected\n",
								sensor->ver);
*/
	return 0;
}
/**
 * ioctl_enum_framesizes - V4L2 sensor if handler for vidioc_int_enum_framesizes
 * @s: pointer to standard V4L2 device structure
 * @frms: pointer to standard V4L2 framesizes enumeration structure
 *
 * Returns possible framesizes depending on choosen pixel format
 **/
static int ioctl_enum_framesizes(struct v4l2_int_device *s,
					struct v4l2_frmsizeenum *frms)
{
	struct camise_sensor *sensor = s->priv;
	int ifmt;

	for (ifmt = 0; ifmt < NUM_CAPTURE_FORMATS; ifmt++) {
		if (frms->pixel_format == camise_formats[ifmt].pixelformat)
			break;
	}

	/* Is requested pixelformat not found on sensor? */
	if (ifmt == NUM_CAPTURE_FORMATS)
		return -EINVAL;
	/* Do we already reached all discrete framesizes? */
	if (frms->index >= (sensor->pdata->get_size(frms->index,
			(unsigned long *)&(frms->discrete.width),
			(unsigned long *)&(frms->discrete.height))))
		return -EINVAL;
	frms->type = V4L2_FRMSIZE_TYPE_DISCRETE;

	return 0;
}

const struct v4l2_fract camise_frameintervals[] = {
	{  .numerator = 1, .denominator = 11 },
	{  .numerator = 1, .denominator = 15 },
	{  .numerator = 1, .denominator = 20 },
	{  .numerator = 1, .denominator = 25 },
	{  .numerator = 1, .denominator = 30 },
	{  .numerator = 1, .denominator = 120 },
};

static int ioctl_enum_frameintervals(struct v4l2_int_device *s,
					struct v4l2_frmivalenum *frmi)
{
	int ifmt;

	for (ifmt = 0; ifmt < NUM_CAPTURE_FORMATS; ifmt++) {
		if (frmi->pixel_format == camise_formats[ifmt].pixelformat)
			break;
	}
	/* Is requested pixelformat not found on sensor? */
	if (ifmt == NUM_CAPTURE_FORMATS)
		return -EINVAL;

	/* Do we already reached all discrete framesizes? */
	/*
	if (((frmi->width == camise_sizes[4].width) &&
				(frmi->height == camise_sizes[4].height)) ||
				((frmi->width == camise_sizes[3].width) &&
				(frmi->height == camise_sizes[3].height))) {
		if (frmi->index != 0)
			return -EINVAL;
	} else if ((frmi->width == camise_sizes[1].width) &&
				(frmi->height == camise_sizes[1].height)) {
		if (frmi->index >= 6)
			return -EINVAL;
	} else {
		if (frmi->index >= 5)
			return -EINVAL;
	}
	*/
	if (frmi->index >= 5)
		return -EINVAL;
	frmi->type = V4L2_FRMSIZE_TYPE_DISCRETE;
	frmi->discrete.numerator =
		camise_frameintervals[frmi->index].numerator;
	frmi->discrete.denominator =
		camise_frameintervals[frmi->index].denominator;

	return 0;
}

static struct v4l2_int_ioctl_desc camise_ioctl_desc[] = {

	{vidioc_int_enum_framesizes_num,
		(v4l2_int_ioctl_func *)ioctl_enum_framesizes},
	{vidioc_int_enum_frameintervals_num,
		(v4l2_int_ioctl_func *)ioctl_enum_frameintervals},
	{vidioc_int_dev_init_num,
		(v4l2_int_ioctl_func *)ioctl_dev_init},
	{vidioc_int_dev_exit_num,
		(v4l2_int_ioctl_func *)ioctl_dev_exit},
	{vidioc_int_s_power_num,
		(v4l2_int_ioctl_func *)ioctl_s_power},
	{vidioc_int_g_priv_num,
		(v4l2_int_ioctl_func *)ioctl_g_priv},
	{vidioc_int_init_num,
		(v4l2_int_ioctl_func *)ioctl_init},
	{vidioc_int_enum_fmt_cap_num,
		(v4l2_int_ioctl_func *)ioctl_enum_fmt_cap},
	{vidioc_int_try_fmt_cap_num,
		(v4l2_int_ioctl_func *)ioctl_try_fmt_cap},
	{vidioc_int_g_fmt_cap_num,
		(v4l2_int_ioctl_func *)ioctl_g_fmt_cap},
	{vidioc_int_s_fmt_cap_num,
		(v4l2_int_ioctl_func *)ioctl_s_fmt_cap},
	{vidioc_int_g_parm_num,
		(v4l2_int_ioctl_func *)ioctl_g_parm},
	{vidioc_int_s_parm_num,
		(v4l2_int_ioctl_func *)ioctl_s_parm},
	{vidioc_int_queryctrl_num,
		(v4l2_int_ioctl_func *)ioctl_queryctrl},
	{vidioc_int_g_ctrl_num,
		(v4l2_int_ioctl_func *)ioctl_g_ctrl},
	{vidioc_int_s_ctrl_num,
		(v4l2_int_ioctl_func *)ioctl_s_ctrl},
	{ vidioc_int_g_crop_num,
		(v4l2_int_ioctl_func *)ioctl_g_crop},
	{vidioc_int_s_crop_num,
		(v4l2_int_ioctl_func *)ioctl_s_crop},
	{ vidioc_int_cropcap_num,
		(v4l2_int_ioctl_func *)ioctl_cropcap},
};


static struct v4l2_int_slave camise_slave = {
	.ioctls		= camise_ioctl_desc,
	.num_ioctls	= ARRAY_SIZE(camise_ioctl_desc),
};



static struct v4l2_int_device camise_int_device = {
	.module	= THIS_MODULE,
	.name	= CAMISE_DRIVER_NAME,
	.priv	= &camise,
	.type	= v4l2_int_type_slave,
	.u	= {
		.slave = &camise_slave,
	},
};


/**
 * camise_remove - sensor driver i2c remove handler
 * @client: i2c driver client device structure
 *
 * Unregister sensor as an i2c client device and V4L2
 * device.  Complement of camise_probe().
 */
static int __exit
camise_remove(struct i2c_client *client)
{
	struct camise_sensor *sensor = i2c_get_clientdata(client);

	if (!client->adapter)
		return -ENODEV;	/* our client isn't attached */

	v4l2_int_device_unregister(sensor->v4l2_int_device);
	i2c_set_clientdata(client, NULL);
	kfree(sensor->pdata);
	kfree(sensor);

	return 0;
}

static int camise_probe(struct i2c_client *client,
			 const struct i2c_device_id *id)
{
	struct camise_sensor *sensor;
	struct camise_platform_data *pdata;
	int err = 0;

	printk(KERN_INFO "camise_probe\n");

	if (i2c_get_clientdata(client))
		return -EBUSY;

	pdata = client->dev.platform_data;
	if (!pdata) {
		dev_err(&client->dev, "no platform data?\n");
		return -EINVAL;
	}

	sensor = kzalloc(sizeof(*sensor), GFP_KERNEL);
	if (!sensor)
		return -ENOMEM;

	/* Don't keep pointer to platform data, copy elements instead */
	sensor->pdata = kzalloc(sizeof(*sensor->pdata), GFP_KERNEL);
	if (!sensor->pdata) {
		err = -ENOMEM;
		goto on_err1;
	}

	sensor->pdata->power_set = pdata->power_set;
	sensor->pdata->priv_data_set = pdata->priv_data_set;
	sensor->pdata->lock_cpufreq = pdata->lock_cpufreq;
	sensor->pdata->if_config = pdata->if_config;
	sensor->pdata->get_size = pdata->get_size;

	sensor->v4l2_int_device = &camise_int_device;
	sensor->v4l2_int_device->priv = sensor;
	sensor->dev = &client->dev;

	sensor->i2c_client = client;

	sensor->state = SENSOR_NOT_DETECTED;

	i2c_set_clientdata(client, sensor);

	/*set sensor default values*/
	sensor->pix.pixelformat = V4L2_PIX_FMT_YUYV;

	err = v4l2_int_device_register(sensor->v4l2_int_device);
	if (err)
		i2c_set_clientdata(client, NULL);

	return err;

on_err1:
    kfree(sensor);
	return err;
}
static const struct i2c_device_id camise_id[] = {
	{ CAMISE_DRIVER_NAME, 0 },
	{ },
};
MODULE_DEVICE_TABLE(i2c, camise_id);

static struct i2c_driver camise_i2c_driver = {
	.driver = {
		.name = CAMISE_DRIVER_NAME,
		.owner = THIS_MODULE,
	},
	.probe = camise_probe,
	.remove = __exit_p(camise_remove),
	.id_table = camise_id,
};


static int __init camisesensor_init(void)
{
	return i2c_add_driver(&camise_i2c_driver);
}

static void __exit camisesensor_exit(void)
{
	i2c_del_driver(&camise_i2c_driver);
}

module_init(camisesensor_init);
module_exit(camisesensor_exit);

MODULE_VERSION("1.0");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("CAMISE camera sensor driver");
MODULE_AUTHOR("Tanguy Pruvot, From Motorola Open Sources");
