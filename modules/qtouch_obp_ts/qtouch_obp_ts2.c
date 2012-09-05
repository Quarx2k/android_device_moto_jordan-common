/*
 * drivers/input/touchscreen/qtouch_obp_ts.c - driver for Quantum touch IC
 *
 * Copyright (C) 2009 Google, Inc.
 * Copyright (C) 2009-2010 Motorola, Inc.
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
 * Derived from the Motorola OBP touch driver.
 *
 */
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/earlysuspend.h>
#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/qtouch_obp_ts.h>
#include <linux/qtouch_obp_ts_firmware.h>
#include <linux/timer.h>
#include "../symsearch/symsearch.h"
#include <linux/proc_fs.h>
#include <asm/uaccess.h>

#define DEBUG 0
#define MULTITOUCH_TAG "multitouch"
#define IGNORE_CHECKSUM_MISMATCH
#define ABS_MT_PRESSURE         0x3a

struct qtm_object {
	struct qtm_obj_entry		entry;
	uint8_t				report_id_min;
	uint8_t				report_id_max;
};

struct axis_map {
	int	key;
	int	x;
	int	y;
};

struct coordinate_map {
	int x_data;
	int y_data;
	int z_data;
	int w_data;
	int vector;
	int down;
};

#define _BITMAP_LEN			BITS_TO_LONGS(QTM_OBP_MAX_OBJECT_NUM)
#define _NUM_FINGERS			10
struct qtouch_ts_data {
	struct i2c_client		*client;
	struct input_dev		*input_dev;
	struct work_struct		init_work;
	struct work_struct		work;
	struct work_struct		boot_work;
	struct qtouch_ts_platform_data	*pdata;
	struct coordinate_map		finger_data[_NUM_FINGERS];
	struct early_suspend		early_suspend;

	struct qtm_object		obj_tbl[QTM_OBP_MAX_OBJECT_NUM];
	unsigned long			obj_map[_BITMAP_LEN];

	uint32_t			last_keystate;
	uint32_t			eeprom_checksum;
	uint8_t				checksum_cnt;
	int				x_delta;
	int				y_delta;
	uint8_t				family_id;
	uint8_t				variant_id;
	uint8_t				fw_version;
	uint8_t				build_version;
	uint8_t				fw_error_count;
	uint32_t			touch_fw_size;
	uint8_t				*touch_fw_image;
	uint8_t				base_fw_version;

	atomic_t			irq_enabled;
	int				status;

	uint8_t				mode;
	int				boot_pkt_size;
	int				current_pkt_sz;
	uint8_t				org_i2c_addr;

	/* Note: The message buffer is reused for reading different messages.
	 * MUST enforce that there is no concurrent access to msg_buf. */
	uint8_t				*msg_buf;
	int				msg_size;
	uint8_t				cal_check_flag;
	unsigned long				cal_timer;
};

#ifdef CONFIG_HAS_EARLYSUSPEND
static void qtouch_ts_early_suspend(struct early_suspend *handler);
static void qtouch_ts_late_resume(struct early_suspend *handler);
#endif

static struct workqueue_struct *qtouch_ts_wq;

static struct timer_list keyarray_timer;
static int ignore_keyarray_touches = 0;
#define KEYARRAY_IGNORE_TIME (msecs_to_jiffies(100))

static uint32_t qtouch_tsdebug;
module_param_named(tsdebug, qtouch_tsdebug, uint, 0664);

static uint32_t qtouch_disable_touch;
module_param_named(disable_touch,qtouch_disable_touch,uint,0664);

static uint8_t calibrate_chip(struct qtouch_ts_data *ts);
static uint8_t check_chip_calibration(struct qtouch_ts_data *ts);

static struct qtouch_ts_data *ts_=NULL;
static struct proc_dir_entry *proc_root;
static char buf[32];
static bool checksumNeedsCorrection = false;
static uint16_t eeprom_checksum;

static int wanted_touch;
static int touch_num = 2;
module_param(touch_num, int, 0);
MODULE_PARM_DESC(touch_num,  "Number of contacts 2-10");
static int enable_pressure = 0;

#define DBG(format, ...) if (DEBUG) printk(KERN_DEBUG MULTITOUCH_TAG ": " format, ## __VA_ARGS__)

// declare functions
SYMSEARCH_DECLARE_FUNCTION_STATIC(int, ss_mapphone_touch_reset, void);
static void qtouch_force_reset(struct qtouch_ts_data *ts, uint8_t sw_reset);

static int set_numtouch(void) {

	if (ts_ == NULL) return -1;

	printk(KERN_INFO MULTITOUCH_TAG": set num_touch to %d\n", wanted_touch);
	ts_->pdata->multi_touch_cfg.num_touch = wanted_touch;
	DBG("forcing checksum error to run qtouch_hw_init()\n");
	ts_->pdata->flags |= QTOUCH_EEPROM_CHECKSUM;
	eeprom_checksum = ts_->eeprom_checksum;
	ts_->eeprom_checksum = 0;
	ts_->checksum_cnt = 0;
	checksumNeedsCorrection = true;

	qtouch_force_reset(ts_, 0);

	return 0;
}

static int proc_numtouch_read(char *buffer, char **buffer_location, off_t offset, int count, int *eof, void *data) {
	int ret;
	if (offset > 0) return 0;
	ret = scnprintf(buffer, count, "%u\n", touch_num);
	return ret;
}

static int proc_numtouch_write(struct file *filp, const char __user *buffer, unsigned long len, void *data)
{
	if (!len || len >= sizeof(buf)) return -ENOSPC;
	if (copy_from_user(buf, buffer, len)) return -EFAULT;

	buf[len] = 0;
	if (sscanf(buf, "%d", &wanted_touch) >= 1) {
		if (wanted_touch > 10) wanted_touch = 10;
		else if (wanted_touch < 2) wanted_touch = 2;
		if (touch_num != wanted_touch && !checksumNeedsCorrection) {
			DBG("wanted touch_num : %d...\n", wanted_touch);
			if (ts_) {
				set_numtouch();
			} else {
				DBG("ts address not set!\n");
				DBG("was the screen off at the time of insmod?\n");

				SYMSEARCH_BIND_FUNCTION_TO(qtouch_obp_ts2, mapphone_touch_reset, ss_mapphone_touch_reset);
				ss_mapphone_touch_reset();
			}
		}
	}
	return len;
}

static irqreturn_t qtouch_ts_irq_handler(int irq, void *dev_id)
{
	struct qtouch_ts_data *ts = dev_id;

	disable_irq_nosync(ts->client->irq);
	if (ts->mode == 1)
		queue_work(qtouch_ts_wq, &ts->boot_work);
	else
		queue_work(qtouch_ts_wq, &ts->work);

	return IRQ_HANDLED;
}

static void keyarray_ignore_timer(unsigned long handle)
{
	ignore_keyarray_touches = 0;
}

static int qtouch_write(struct qtouch_ts_data *ts, void *buf, int buf_sz)
{
	int retries = 10;
	int ret;

	do {
		ret = i2c_master_send(ts->client, (char *)buf, buf_sz);
	} while ((ret < buf_sz) && (--retries > 0));

	if (ret < 0)
		pr_info("%s: Error while trying to write %d bytes\n", __func__,
			buf_sz);
	else if (ret != buf_sz) {
		pr_info("%s: Write %d bytes, expected %d\n", __func__,
			ret, buf_sz);
		ret = -EIO;
	}
	return ret;
}

static int qtouch_set_addr(struct qtouch_ts_data *ts, uint16_t addr)
{
	int ret;

	/* Note: addr on the wire is LSB first */
	ret = qtouch_write(ts, (char *)&addr, sizeof(uint16_t));
	if (ret < 0)
		pr_info("%s: Can't send obp addr 0x%4x\n", __func__, addr);

	// original return
	ret = ret >= 0 ? 0 : ret;
	
	// multiboot
	if (ts != NULL && ts->pdata != NULL) {
		touch_num = ts->pdata->multi_touch_cfg.num_touch;
		if (touch_num != wanted_touch) {
			ts_ = ts;
			DBG("wanted/current multitouch points: %d/%d\n", wanted_touch, touch_num);
			set_numtouch();
		}
	}
	DBG("qtouch_set_addr 0x%x=%d\n", addr, ret);
	return ret;
}

static int qtouch_read(struct qtouch_ts_data *ts, void *buf, int buf_sz)
{
	int retries = 10;
	int ret;

	do {
		memset(buf, 0, buf_sz);
		ret = i2c_master_recv(ts->client, (char *)buf, buf_sz);
	} while ((ret < 0) && (--retries > 0));

	if (ret < 0)
		pr_info("%s: Error while trying to read %d bytes\n", __func__,
			buf_sz);
	else if (ret != buf_sz) {
		pr_info("%s: Read %d bytes, expected %d\n", __func__,
			ret, buf_sz);
		ret = -EIO;
	}

	return ret >= 0 ? 0 : ret;
}

static int qtouch_read_addr(struct qtouch_ts_data *ts, uint16_t addr,
			    void *buf, int buf_sz)
{
	int ret;

	ret = qtouch_set_addr(ts, addr);
	if (ret != 0)
		return ret;

	return qtouch_read(ts, buf, buf_sz);
}

static struct qtm_obj_message *qtouch_read_msg(struct qtouch_ts_data *ts)
{
	int ret;

	ret = qtouch_read(ts, ts->msg_buf, ts->msg_size);
	if (!ret)
		return (struct qtm_obj_message *)ts->msg_buf;
	return NULL;
}

static int qtouch_write_addr(struct qtouch_ts_data *ts, uint16_t addr,
			     void *buf, int buf_sz)
{
	int ret;
	uint8_t *write_buf;

	write_buf = kmalloc((buf_sz + sizeof(uint16_t)), GFP_KERNEL);
	if (write_buf == NULL) {
		pr_err("%s: Can't allocate write buffer (%d)\n", __func__, buf_sz);
		return -ENOMEM;
	}

	memcpy(write_buf, (void *)&addr, sizeof(addr));
	memcpy((void *)write_buf + sizeof(addr), buf, buf_sz);

	ret = qtouch_write(ts, write_buf, buf_sz + sizeof(addr));

	kfree (write_buf);

	if (ret < 0) {
		pr_err("%s: Could not write %d bytes.\n", __func__, buf_sz);
		return ret;
	}

	return 0;
}

static uint16_t calc_csum(uint16_t curr_sum, void *_buf, int buf_sz)
{
	uint8_t *buf = _buf;
	uint32_t new_sum;
	int i;

	while (buf_sz-- > 0) {
		new_sum = (((uint32_t) curr_sum) << 8) | *(buf++);
		for (i = 0; i < 8; ++i) {
			if (new_sum & 0x800000)
				new_sum ^= 0x800500;
			new_sum <<= 1;
		}
		curr_sum = ((uint32_t) new_sum >> 8) & 0xffff;
	}

	return curr_sum;
}

static inline struct qtm_object *find_obj(struct qtouch_ts_data *ts, int id)
{
	return &ts->obj_tbl[id];
}

static struct qtm_object *create_obj(struct qtouch_ts_data *ts,
				     struct qtm_obj_entry *entry)
{
	struct qtm_object *obj;

	obj = &ts->obj_tbl[entry->type];
	memcpy(&obj->entry, entry, sizeof(*entry));
	set_bit(entry->type, ts->obj_map);

	return obj;
}

static struct qtm_object *find_object_rid(struct qtouch_ts_data *ts, int rid)
{
	int i;

	for_each_bit(i, ts->obj_map, QTM_OBP_MAX_OBJECT_NUM) {
		struct qtm_object *obj = &ts->obj_tbl[i];

		if ((rid >= obj->report_id_min) && (rid <= obj->report_id_max))
			return obj;
	}

	return NULL;
}

static void qtouch_force_reset(struct qtouch_ts_data *ts, uint8_t sw_reset)
{
	struct qtm_object *obj;
	uint16_t addr;
	uint8_t val = 1;
	int ret;

	if (ts->pdata->hw_reset && !sw_reset) {
		pr_info("%s: Forcing HW reset\n", __func__);
		ts->pdata->hw_reset();
	} else if (sw_reset) {
		pr_info("%s: Forcing SW reset\n", __func__);
		obj = find_obj(ts, QTM_OBJ_GEN_CMD_PROC);
		addr =
		    obj->entry.addr + offsetof(struct qtm_gen_cmd_proc, reset);
		/* Check to see if to reset into boot mode */
		if (sw_reset == 2)
			val = 0xa5;
		ret = qtouch_write_addr(ts, addr, &val, 1);
		if (ret)
			pr_err("%s: Unable to send the reset msg\n", __func__);
	}
}

static int qtouch_force_calibration(struct qtouch_ts_data *ts)
{
	struct qtm_object *obj;
	uint16_t addr;
	uint8_t val;
	int ret;

	pr_info("%s: Forcing calibration\n", __func__);

	obj = find_obj(ts, QTM_OBJ_GEN_CMD_PROC);

	addr = obj->entry.addr + offsetof(struct qtm_gen_cmd_proc, calibrate);
	val = 1;
	ret = qtouch_write_addr(ts, addr, &val, 1);
	if (ret)
		pr_err("%s: Unable to send the calibrate message\n", __func__);

	return ret;
}

#undef min
#define min(a, b) (((a) < (b)) ? (a) : (b))
static int qtouch_power_config(struct qtouch_ts_data *ts, int on)
{
	struct qtm_gen_power_cfg pwr_cfg;
	struct qtm_object *obj;

	if (!on) {
		/* go to standby mode */
		pwr_cfg.idle_acq_int = 0;
		pwr_cfg.active_acq_int = 0;
	} else {
		pwr_cfg.idle_acq_int = ts->pdata->power_cfg.idle_acq_int;
		pwr_cfg.active_acq_int = ts->pdata->power_cfg.active_acq_int;
	}

	pwr_cfg.active_idle_to = ts->pdata->power_cfg.active_idle_to;

	obj = find_obj(ts, QTM_OBJ_GEN_PWR_CONF);
	return qtouch_write_addr(ts, obj->entry.addr, &pwr_cfg,
				 min(sizeof(pwr_cfg), obj->entry.size));
}

/* Apply the configuration provided in the platform_data to the hardware */
static int qtouch_hw_init(struct qtouch_ts_data *ts)
{
	struct qtm_object *obj;
	int i;
	int ret;
	uint16_t adj_addr;

	pr_info("%s: Doing hw init\n", __func__);
	
	/* multitouch */
	if (checksumNeedsCorrection) {
		DBG("checksum need correction...\n");
		checksumNeedsCorrection = false;
		ts->eeprom_checksum = eeprom_checksum;
	}

	/* take the IC out of suspend */
	qtouch_power_config(ts, 1);

	/* configure the acquisition object. */
	obj = find_obj(ts, QTM_OBJ_GEN_ACQUIRE_CONF);
	ret = qtouch_write_addr(ts, obj->entry.addr, &ts->pdata->acquire_cfg,
				min(sizeof(ts->pdata->acquire_cfg),
				    obj->entry.size));
	if (ret != 0) {
		pr_err("%s: Can't write acquisition config\n", __func__);
		return ret;
	}

	/* The multitouch and keyarray objects have very similar memory
	 * layout, but are just different enough where we basically have to
	 * repeat the same code */

	/* configure the multi-touch object. */
	obj = find_obj(ts, QTM_OBJ_TOUCH_MULTI);
	if (obj && obj->entry.num_inst > 0) {
		struct qtm_touch_multi_cfg cfg;
		memcpy(&cfg, &ts->pdata->multi_touch_cfg, sizeof(cfg));
		if (ts->pdata->flags & QTOUCH_USE_MULTITOUCH)
			cfg.ctrl |= (1 << 1) | (1 << 0); /* reporten | enable */
		else
			cfg.ctrl = 0;
		ret = qtouch_write_addr(ts, obj->entry.addr, &cfg,
					min(sizeof(cfg), obj->entry.size));
		if (ret != 0) {
			pr_err("%s: Can't write multi-touch config\n",
			       __func__);
			return ret;
		}
	}

	/* configure the key-array object. */
	obj = find_obj(ts, QTM_OBJ_TOUCH_KEYARRAY);
	if (obj && obj->entry.num_inst > 0) {
		struct qtm_touch_keyarray_cfg cfg;
		for (i = 0; i < obj->entry.num_inst; i++) {
			if (ts->pdata->flags & QTOUCH_USE_KEYARRAY) {
				memcpy(&cfg, &ts->pdata->key_array.cfg[i],
				       sizeof(cfg));
			} else
				memset(&cfg, 0, sizeof(cfg));

			adj_addr = obj->entry.addr +
				((obj->entry.size + 1) * i);
			ret = qtouch_write_addr(ts, adj_addr, &cfg,
						min(sizeof(cfg),
						    obj->entry.size));
			if (ret != 0) {
				pr_err("%s: Can't write keyarray config\n",
					   __func__);
				return ret;
			}
		}
	}

	/* configure the signal filter */
	obj = find_obj(ts, QTM_OBJ_PROCG_SIG_FILTER);
	if (obj && obj->entry.num_inst > 0) {
		ret = qtouch_write_addr(ts, obj->entry.addr,
					&ts->pdata->sig_filter_cfg,
					min(sizeof(ts->pdata->sig_filter_cfg),
					    obj->entry.size));
		if (ret != 0) {
			pr_err("%s: Can't write signal filter config\n",
			       __func__);
			return ret;
		}
	}

	/* configure the linearization table */
	obj = find_obj(ts, QTM_OBJ_PROCI_LINEAR_TBL);
	if (obj && obj->entry.num_inst > 0) {
		ret = qtouch_write_addr(ts, obj->entry.addr,
					&ts->pdata->linear_tbl_cfg,
					min(sizeof(ts->pdata->linear_tbl_cfg),
					    obj->entry.size));
		if (ret != 0) {
			pr_err("%s: Can't write linear table config\n",
			       __func__);
			return ret;
		}
	}

	/* configure the comms configuration */
	obj = find_obj(ts, QTM_OBJ_SPT_COM_CONFIG);
	if (obj && obj->entry.num_inst > 0) {
		ret = qtouch_write_addr(ts, obj->entry.addr,
					&ts->pdata->comms_config_cfg,
					min(sizeof(ts->pdata->comms_config_cfg),
					    obj->entry.size));
		if (ret != 0) {
			pr_err("%s: Can't write the comms configuration config\n",
			       __func__);
			return ret;
		}
	}

	/* configure the GPIO PWM support */
	obj = find_obj(ts, QTM_OBJ_SPT_GPIO_PWM);
	if (obj && obj->entry.num_inst > 0) {
		ret = qtouch_write_addr(ts, obj->entry.addr,
					&ts->pdata->gpio_pwm_cfg,
					min(sizeof(ts->pdata->gpio_pwm_cfg),
					    obj->entry.size));
		if (ret != 0) {
			pr_err("%s: Can't write the GPIO PWM config\n",
			       __func__);
			return ret;
		}
	}

	/* configure the grip suppression table */
	obj = find_obj(ts, QTM_OBJ_PROCI_GRIPFACESUPPRESSION);
	if (obj && obj->entry.num_inst > 0) {
		ret = qtouch_write_addr(ts, obj->entry.addr,
					&ts->pdata->grip_suppression_cfg,
					min(sizeof
					    (ts->pdata->grip_suppression_cfg),
					    obj->entry.size));
		if (ret != 0) {
			pr_err("%s: Can't write the grip suppression config\n",
			       __func__);
			return ret;
		}
	}

	/* configure noise suppression */
	obj = find_obj(ts, QTM_OBJ_PROCG_NOISE_SUPPRESSION);
	if (obj && obj->entry.num_inst > 0) {
		ret = qtouch_write_addr(ts, obj->entry.addr,
					&ts->pdata->noise_suppression_cfg,
					min(sizeof(ts->pdata->noise_suppression_cfg),
					    obj->entry.size));
		if (ret != 0) {
			pr_err("%s: Can't write the noise suppression config\n",
			       __func__);
			return ret;
		}
	}

	/* configure the touch proximity sensor */
	obj = find_obj(ts, QTM_OBJ_TOUCH_PROXIMITY);
	if (obj && obj->entry.num_inst > 0) {
		ret = qtouch_write_addr(ts, obj->entry.addr,
					&ts->pdata->touch_proximity_cfg,
					min(sizeof(ts->pdata->touch_proximity_cfg),
					    obj->entry.size));
		if (ret != 0) {
			pr_err("%s: Can't write the touch proximity config\n",
			__func__);
			return ret;
		}
	}

	/* configure the one touch gesture processor */
	obj = find_obj(ts, QTM_OBJ_PROCI_ONE_TOUCH_GESTURE_PROC);
	if (obj && obj->entry.num_inst > 0) {
		ret = qtouch_write_addr(ts, obj->entry.addr,
					&ts->pdata->one_touch_gesture_proc_cfg,
					min(sizeof(ts->pdata->one_touch_gesture_proc_cfg),
					    obj->entry.size));
		if (ret != 0) {
			pr_err("%s: Can't write the one touch gesture processor config\n",
			       __func__);
			return ret;
		}
	}

	/* configure self test */
	obj = find_obj(ts, QTM_OBJ_SPT_SELF_TEST);
	if (obj && obj->entry.num_inst > 0) {
		ret = qtouch_write_addr(ts, obj->entry.addr,
					&ts->pdata->self_test_cfg,
					min(sizeof(ts->pdata->self_test_cfg),
					    obj->entry.size));
		if (ret != 0) {
			pr_err("%s: Can't write the self test config\n",
			       __func__);
			return ret;
		}
	}

	/* configure the two touch gesture processor */
	obj = find_obj(ts, QTM_OBJ_PROCI_TWO_TOUCH_GESTURE_PROC);
	if (obj && obj->entry.num_inst > 0) {
		ret = qtouch_write_addr(ts, obj->entry.addr,
					&ts->pdata->two_touch_gesture_proc_cfg,
					min(sizeof(ts->pdata->two_touch_gesture_proc_cfg),
					    obj->entry.size));
		if (ret != 0) {
			pr_err("%s: Can't write the two touch gesture processor config\n",
			       __func__);
			return ret;
		}
	}

	/* configure the capacitive touch engine  */
	obj = find_obj(ts, QTM_OBJ_SPT_CTE_CONFIG);
	if (obj && obj->entry.num_inst > 0) {
		ret = qtouch_write_addr(ts, obj->entry.addr,
					&ts->pdata->cte_config_cfg,
					min(sizeof(ts->pdata->cte_config_cfg),
					    obj->entry.size));
		if (ret != 0) {
			pr_err("%s: Can't write the capacitive touch engine config\n",
			       __func__);
			return ret;
		}
	}

	/* configure the noise suppression table */
	obj = find_obj(ts, QTM_OBJ_NOISESUPPRESSION_1);
	if (obj && obj->entry.num_inst > 0) {
		ret = qtouch_write_addr(ts, obj->entry.addr,
					&ts->pdata->noise1_suppression_cfg,
					min(sizeof
					    (ts->pdata->noise1_suppression_cfg),
					    obj->entry.size));
		if (ret != 0) {
			pr_err("%s: Can't write the noise suppression config\n",
			       __func__);
			return ret;
		}
	}

	ret = qtouch_force_calibration(ts);
	if (ret != 0) {
		pr_err("%s: Unable to recalibrate after reset\n", __func__);
		return ret;
	}

	/* Write the settings into nvram, if needed */
	if (ts->pdata->flags & QTOUCH_CFG_BACKUPNV) {
		uint8_t val;
		uint16_t addr;

		obj = find_obj(ts, QTM_OBJ_GEN_CMD_PROC);
		addr = obj->entry.addr + offsetof(struct qtm_gen_cmd_proc,
						  backupnv);
		val = 0x55;
		ret = qtouch_write_addr(ts, addr, &val, 1);
		if (ret != 0) {
			pr_err("%s: Can't backup nvram settings\n", __func__);
			return ret;
		}
		/* Since the IC does not indicate that has completed the
		backup place a hard wait here.  If we communicate with the
		IC during backup the EEPROM may be corrupted */

		msleep(QTM_OBP_SLEEP_WAIT_FOR_BACKUP);
	}

	/* If debugging, read back and print all settings */
	if (qtouch_tsdebug) {
		int object;
		int size;
		uint8_t *data_buff;
		int byte;
		int msg_bytes;
		int msg_location;
		char *msg;

		msg = kmalloc(1024, GFP_KERNEL);
		if (msg != NULL) {
			for (object = 7; object < QTM_OBP_MAX_OBJECT_NUM; object++) {

				size = ts->obj_tbl[object].entry.size
				       * ts->obj_tbl[object].entry.num_inst;
				if (size != 0) {
					data_buff = kmalloc(size, GFP_KERNEL);
					if (data_buff == NULL) {
						pr_err("%s: Object %d: Malloc failed\n",
						       __func__, object);
						continue;
					}

					qtouch_read_addr(ts,
					                 ts->obj_tbl[object].entry.addr,
					                 (void *)data_buff, size);

					msg_location = sprintf(msg, "%s: Object %d:",
					                       __func__, object);
					for (byte=0; byte < size; byte++) {
						msg_bytes = snprintf((msg + msg_location),
						                    (1024 - msg_location),
						                    " 0x%02x",
						                    *(data_buff + byte));
						msg_location += msg_bytes;
						if (msg_location >= 1024)
							break;
					}
					if (msg_location < 1024) {
						pr_info("%s\n", msg);
					} else {
						pr_info("%s:  Object %d: String overflow\n",
						        __func__, object);
					}

					kfree (data_buff);
				}
			}

			kfree (msg);
		}

		qtouch_set_addr(ts, ts->obj_tbl[QTM_OBJ_GEN_MSG_PROC].entry.addr);
	}

	/* reset the address pointer */
	ret = qtouch_set_addr(ts, ts->obj_tbl[QTM_OBJ_GEN_MSG_PROC].entry.addr);
	if (ret != 0) {
		pr_err("%s: Unable to reset address pointer after reset\n",
		       __func__);
		return ret;
	}

	return 0;
}

/* Handles a message from the command processor object. */
static int do_cmd_proc_msg(struct qtouch_ts_data *ts, struct qtm_object *obj,
			   void *_msg)
{
	struct qtm_cmd_proc_msg *msg = _msg;
	int ret = 0;
	int hw_reset = 0;
	uint32_t checksum = 0x0;

	if (msg->status & QTM_CMD_PROC_STATUS_RESET) {
		if (qtouch_tsdebug)
			pr_info("%s:EEPROM checksum is 0x%02X%02X%02X cnt %i\n",
				__func__, msg->checksum[2], msg->checksum[1],
				msg->checksum[0], ts->checksum_cnt);

		checksum = (msg->checksum[0] |
			(msg->checksum[1] << BITS_PER_BYTE) |
			(msg->checksum[2] << (BITS_PER_BYTE << 1)));

		checksum = __le32_to_cpu(checksum);
		if (checksum != ts->eeprom_checksum) {
			if (ts->checksum_cnt > 2) {
				/* Assume the checksum is what it is, cannot
				disable the touch screen so set the checksum*/
				ts->eeprom_checksum = checksum;
				ts->checksum_cnt = 0;
			} else {
				pr_info("%s:EEPROM checksum mismatch 0x%8X\n",
					 __func__, checksum);
				ret = qtouch_hw_init(ts);
				if (ret != 0)
					pr_err("%s:Cannot init the touch IC\n",
						   __func__);
				hw_reset = 1;
				ts->checksum_cnt++;
			}
		} else {
			pr_info("%s:EEPROM checksum matches\n", __func__);
		}
		if (ts->family_id == QTM_OBP_FAMILY_XMEGAT)
			calibrate_chip(ts);
		pr_info("%s: Reset done.\n", __func__);
	}

	if (msg->status & QTM_CMD_PROC_STATUS_CAL) {
		if (ts->family_id == QTM_OBP_FAMILY_XMEGAT) {
			ts->cal_check_flag = 1;
			ts->cal_timer = 0;
		}
		pr_info("%s: Self-calibration started.\n", __func__);
	}

	if (msg->status & QTM_CMD_PROC_STATUS_OFL)
		pr_err("%s: Acquisition cycle length overflow\n", __func__);

	if (msg->status & QTM_CMD_PROC_STATUS_SIGERR)
		pr_err("%s: Acquisition error\n", __func__);

	if (msg->status & QTM_CMD_PROC_STATUS_CFGERR) {
		pr_err("%s: Configuration error\n", __func__);
		ret = qtouch_hw_init(ts);
		if (ret != 0)
			pr_err("%s:Cannot init the touch IC\n", __func__);
	}
	/* Check the EEPROM checksum.  An ESD event may cause
	the checksum to change during operation so we need to
	reprogram the EEPROM and reset the IC */
	if (ts->pdata->flags & QTOUCH_EEPROM_CHECKSUM) {

		if (qtouch_tsdebug)
			pr_info("%s:EEPROM checksum is 0x%02X%02X%02X cnt %i\n",
				__func__, msg->checksum[2], msg->checksum[1],
				msg->checksum[0], ts->checksum_cnt);

		checksum = (msg->checksum[0] |
			(msg->checksum[1] << BITS_PER_BYTE) |
			(msg->checksum[2] << (BITS_PER_BYTE << 1)));

		if (checksum != ts->eeprom_checksum) {
			if (qtouch_tsdebug)
				pr_info("%s:EEPROM checksum is 0x%08X cnt %i\n",
					__func__, checksum, ts->checksum_cnt);

			if (ts->checksum_cnt > 2) {
				/* Assume the checksum is what it is, cannot
				disable the touch screen so set the checksum*/
				ts->eeprom_checksum = checksum;
				ts->checksum_cnt = 0;
			} else {
				if (!hw_reset) {
					ret = qtouch_hw_init(ts);
					if (ret != 0)
						pr_err
						    ("%s:Cannot init the touch IC\n",
						__func__);
					qtouch_force_reset(ts, 0);
					ts->checksum_cnt++;
				}
			}
		}
	}
	return ret;
}
#define QTM_TOUCH_MULTI_STATUS_SUPPRESS         (1 << 1)
/* Handles a message from a multi-touch object. */
static int do_touch_multi_msg(struct qtouch_ts_data *ts, struct qtm_object *obj,
			      void *_msg)
{
	struct qtm_touch_multi_msg *msg = _msg;
	int i;
	int x;
	int y;
	int pressure;
	int width;
	uint32_t finger;
	int down;
	int num_fingers_down;

	finger = msg->report_id - obj->report_id_min;
	if (finger >= ts->pdata->multi_touch_cfg.num_touch)
		return 0;

	if (qtouch_tsdebug & 0x10)
		printk("%s: msgxpos_msb 0x%X msgypos_msb 0x%X msgxypos 0x%X \n",
			__func__, msg->xpos_msb, msg->ypos_msb, msg->xypos_lsb);

	/* x/y are 10bit values, with bottom 2 bits inside the xypos_lsb */
	x = (msg->xpos_msb << 2) | ((msg->xypos_lsb >> 6) & 0x3);
	y = (msg->ypos_msb << 2) | ((msg->xypos_lsb >> 2) & 0x3);

	width = msg->touch_area;
	pressure = msg->touch_amp;

	if (ts->pdata->flags & QTOUCH_SWAP_XY)
		swap(x, y);
	
	if (qtouch_tsdebug & 2)
		pr_info("%s: stat=%02x, f=%d x=%d y=%d p=%d w=%d\n", __func__,
			msg->status, finger, x, y, pressure, width);

	if (finger >= _NUM_FINGERS) {
		pr_err("%s: Invalid finger number %dd\n", __func__, finger);
		return 1;
	}

	down = !(msg->status & (QTM_TOUCH_MULTI_STATUS_RELEASE |
		 QTM_TOUCH_MULTI_STATUS_SUPPRESS));

	if (down) {
		ts->finger_data[finger].x_data = x;
		ts->finger_data[finger].y_data = y;
		ts->finger_data[finger].w_data = width;
		ts->finger_data[finger].z_data = pressure;
		ts->finger_data[finger].vector = msg->touch_vect;
		ts->finger_data[finger].down = 1;
	} else {
		memset(&ts->finger_data[finger], 0,
			sizeof(struct coordinate_map));
	}

	num_fingers_down = 0;
	for (i = 0; i < ts->pdata->multi_touch_cfg.num_touch; i++) {
		if (ts->finger_data[i].down == 0)
			continue;
		
		input_report_abs(ts->input_dev, ABS_MT_TOUCH_MAJOR,
				 ts->finger_data[i].w_data);
		input_report_abs(ts->input_dev, ABS_MT_POSITION_X,
				 ts->finger_data[i].x_data);
		input_report_abs(ts->input_dev, ABS_MT_POSITION_Y,
				 ts->finger_data[i].y_data);
		input_report_abs(ts->input_dev, ABS_MT_ORIENTATION,
				 ts->finger_data[i].vector);
		input_report_abs(ts->input_dev, ABS_MT_TRACKING_ID,
				 i);
		if(enable_pressure) {
			input_report_abs(ts->input_dev, ABS_MT_PRESSURE,
				 ts->finger_data[i].z_data);
		}
		else {
			input_set_abs_params(ts->input_dev, ABS_MT_WIDTH_MAJOR,
					    ts->pdata->abs_min_w, ts->pdata->abs_max_w,
					    ts->pdata->fuzz_w, 0);
		}
		input_mt_sync(ts->input_dev);
		num_fingers_down++;
	}
	if (num_fingers_down == 0)
		input_mt_sync(ts->input_dev);
	input_sync(ts->input_dev);

	return 0;
}

/* Handles a message from a keyarray object. */
static int do_touch_keyarray_msg(struct qtouch_ts_data *ts,
				 struct qtm_object *obj, void *_msg)
{
	struct qtm_touch_keyarray_msg *msg = _msg;
	int i;

	/* nothing changed.. odd. */
	if (ts->last_keystate == msg->keystate)
		return 0;

	for (i = 0; i < ts->pdata->key_array.num_keys; ++i) {
		struct qtouch_key *key = &ts->pdata->key_array.keys[i];
		uint32_t bit = 1 << (key->channel & 0x1f);
		if ((msg->keystate & bit) != (ts->last_keystate & bit)) {
			if (msg->keystate & bit) {
				if (ignore_keyarray_touches)
					return 0;
				input_report_abs(ts->input_dev, ABS_MT_TOUCH_MAJOR, 4);
			} else {
				input_report_abs(ts->input_dev,
						 ABS_MT_TOUCH_MAJOR, 0);
			}
			input_report_abs(ts->input_dev, ABS_MT_WIDTH_MAJOR, 4);
			input_report_abs(ts->input_dev, ABS_MT_POSITION_X,
					 ts->pdata->key_array.keys[i].x_coord);
			input_report_abs(ts->input_dev, ABS_MT_POSITION_Y,
					 ts->pdata->key_array.keys[i].y_coord);
			input_mt_sync(ts->input_dev);
			input_sync(ts->input_dev);
		}
	}


	if (qtouch_tsdebug & 2)
		pr_info("%s: key state changed 0x%08x -> 0x%08x\n", __func__,
			ts->last_keystate, msg->keystate);

	/* update our internal state */
	ts->last_keystate = msg->keystate;

	return 0;
}

static int qtouch_handle_msg(struct qtouch_ts_data *ts, struct qtm_object *obj,
			     struct qtm_obj_message *msg)
{
	int ret = 0;

	/* These are all the known objects that we know how to handle. */
	switch (obj->entry.type) {
	case QTM_OBJ_GEN_CMD_PROC:
		ret = do_cmd_proc_msg(ts, obj, msg);
		break;

	case QTM_OBJ_TOUCH_MULTI:
		ret = do_touch_multi_msg(ts, obj, msg);
		break;

	case QTM_OBJ_TOUCH_KEYARRAY:
		ret = do_touch_keyarray_msg(ts, obj, msg);
		break;

	default:
		/* probably not fatal? */
		ret = 0;
		pr_info("%s: No handler defined for message from object "
			"type %d, report_id %d\n", __func__, obj->entry.type,
			msg->report_id);
	}

	return ret;
}

static int qtouch_ts_register_input(struct qtouch_ts_data *ts)
{
	struct qtm_object *obj;
	int err;
	int i;

	if (ts->input_dev == NULL) {
		ts->input_dev = input_allocate_device();
		if (ts->input_dev == NULL) {
			pr_err("%s: failed to alloc input device\n", __func__);
			err = -ENOMEM;
			return err;
		}
	}

	ts->input_dev->name = "qtouch-touchscreen";
	input_set_drvdata(ts->input_dev, ts);

	ts->msg_buf = kmalloc(ts->msg_size, GFP_KERNEL);
	if (ts->msg_buf == NULL) {
		pr_err("%s: Cannot allocate msg_buf\n", __func__);
		err = -ENOMEM;
		goto err_alloc_msg_buf;
	}

	/* Point the address pointer to the message processor.
	 * Must do this before enabling interrupts */
	obj = find_obj(ts, QTM_OBJ_GEN_MSG_PROC);
	err = qtouch_set_addr(ts, obj->entry.addr);
	if (err != 0) {
	        pr_err("%s: Can't to set addr to msg processor\n", __func__);
	        goto err_rst_addr_msg_proc;
	}

	set_bit(EV_SYN, ts->input_dev->evbit);

	/* register the harwdare assisted virtual keys, if any */
	obj = find_obj(ts, QTM_OBJ_TOUCH_KEYARRAY);
	if (obj && (obj->entry.num_inst > 0) &&
	    (ts->pdata->flags & QTOUCH_USE_KEYARRAY)) {
		for (i = 0; i < ts->pdata->key_array.num_keys; ++i)
			input_set_capability(ts->input_dev, EV_KEY,
			                     ts->pdata->key_array.keys[i].code);
	}

	/* register the software virtual keys, if any are provided */
	for (i = 0; i < ts->pdata->vkeys.count; ++i)
		input_set_capability(ts->input_dev, EV_KEY,
		                     ts->pdata->vkeys.keys[i].code);

	obj = find_obj(ts, QTM_OBJ_TOUCH_MULTI);
	if (obj && obj->entry.num_inst > 0) {
		set_bit(EV_ABS, ts->input_dev->evbit);

		input_set_abs_params(ts->input_dev, ABS_MT_POSITION_X,
				     ts->pdata->abs_min_x, ts->pdata->abs_max_x,
				     ts->pdata->fuzz_x, 0);
		input_set_abs_params(ts->input_dev, ABS_MT_POSITION_Y,
				     ts->pdata->abs_min_y, ts->pdata->abs_max_y,
				     ts->pdata->fuzz_y, 0);
		if(enable_pressure) {
			input_set_abs_params(ts->input_dev, ABS_MT_PRESSURE,
				     ts->pdata->abs_min_p, ts->pdata->abs_max_p,
				     ts->pdata->fuzz_p, 0);
		}
		else {
			input_report_abs(ts->input_dev, ABS_MT_WIDTH_MAJOR,
				     ts->finger_data[i].w_data);
		}
		input_set_abs_params(ts->input_dev, ABS_MT_TOUCH_MAJOR,
				     ts->pdata->abs_min_w, ts->pdata->abs_max_w,
				     ts->pdata->fuzz_w, 0);
		input_set_abs_params(ts->input_dev, ABS_MT_ORIENTATION,
				     0, 255, 0, 0);
		input_set_abs_params(ts->input_dev, ABS_MT_TRACKING_ID,
				     0, ts->pdata->multi_touch_cfg.num_touch, 1, 0);
		
	}

	memset(&ts->finger_data[0], 0,
	       (sizeof(struct coordinate_map) *
	       _NUM_FINGERS));

	/* Use a 1024 event evdev buffer to minimize overflow */
	input_set_events_per_packet(ts->input_dev, 128);

	err = input_register_device(ts->input_dev);
	if (err != 0) {
		pr_err("%s: Cannot register input device \"%s\"\n", __func__,
		       ts->input_dev->name);
		goto err_input_register_dev;
	}
	return 0;

err_input_register_dev:
err_rst_addr_msg_proc:
	if (ts->msg_buf)
		kfree(ts->msg_buf);

err_alloc_msg_buf:
	input_free_device(ts->input_dev);
	ts->input_dev = NULL;

	return err;
}

static int qtouch_process_info_block(struct qtouch_ts_data *ts)
{
	struct qtm_id_info qtm_info;
	uint32_t our_csum = 0x0;
	uint32_t their_csum = 0x0;
	uint8_t report_id;
	uint8_t checksum[3];
	uint16_t addr;
	int err;
	int i;

	/* query the device and get the info block. */
	err = qtouch_read_addr(ts, QTM_OBP_ID_INFO_ADDR, &qtm_info,
			       sizeof(qtm_info));
	if (err != 0) {
		pr_err("%s: Cannot read info object block\n", __func__);
		goto err_read_info_block;
	}
	our_csum = calc_csum(our_csum, &qtm_info, sizeof(qtm_info));

	/* TODO: Add a version/family/variant check? */
	pr_info("%s: Build version is 0x%x\n", __func__, qtm_info.version);

	if (qtm_info.num_objs == 0) {
		pr_err("%s: Device (0x%x/0x%x/0x%x/0x%x) does not export any "
		       "objects.\n", __func__, qtm_info.family_id,
		       qtm_info.variant_id, qtm_info.version, qtm_info.build);
		err = -ENODEV;
		goto err_no_objects;
	}

	addr = QTM_OBP_ID_INFO_ADDR + sizeof(qtm_info);
	report_id = 1;

	/* Clear the object table */
	for (i = 0; i < QTM_OBP_MAX_OBJECT_NUM; ++i) {
		ts->obj_tbl[i].entry.type = 0;
		ts->obj_tbl[i].entry.addr = 0;
		ts->obj_tbl[i].entry.size = 0;
		ts->obj_tbl[i].entry.num_inst = 0;
		ts->obj_tbl[i].entry.num_rids = 0;
		ts->obj_tbl[i].report_id_min = 0;
		ts->obj_tbl[i].report_id_max = 0;
	}

	/* read out the object entries table */
	for (i = 0; i < qtm_info.num_objs; ++i) {
		struct qtm_object *obj;
		struct qtm_obj_entry entry;

		err = qtouch_read_addr(ts, addr, &entry, sizeof(entry));
		if (err != 0) {
			pr_err("%s: Can't read object (%d) entry.\n",
			       __func__, i);
			err = -EIO;
			goto err_read_entry;
		}
		our_csum = calc_csum(our_csum, &entry, sizeof(entry));
		addr += sizeof(entry);

		entry.size++;
		entry.num_inst++;

		pr_info("%s: Object %d @ 0x%04x (%d) insts %d rep_ids %d\n",
			__func__, entry.type, entry.addr, entry.size,
			entry.num_inst, entry.num_rids);

		if (entry.type >= QTM_OBP_MAX_OBJECT_NUM) {
			pr_warning("%s: Unknown object type (%d) encountered\n",
				   __func__, entry.type);
			/* Not fatal */
			continue;
		}

		/* save the message_procesor msg_size for easy reference. */
		if (entry.type == QTM_OBJ_GEN_MSG_PROC)
			ts->msg_size = entry.size;

		obj = create_obj(ts, &entry);
		/* set the report_id range that the object is responsible for */
		if ((obj->entry.num_rids * obj->entry.num_inst) != 0) {
			obj->report_id_min = report_id;
			report_id += obj->entry.num_rids * obj->entry.num_inst;
			obj->report_id_max = report_id - 1;
		}
	}

	if (!ts->msg_size) {
		pr_err("%s: Message processing object not found. Bailing.\n",
		       __func__);
		err = -ENODEV;
		goto err_no_msg_proc;
	}

	/* verify that some basic objects are present. These objects are
	 * assumed to be present by the rest of the driver, so fail out now
	 * if the firmware is busted. */
	if (!find_obj(ts, QTM_OBJ_GEN_PWR_CONF) ||
	    !find_obj(ts, QTM_OBJ_GEN_ACQUIRE_CONF) ||
	    !find_obj(ts, QTM_OBJ_GEN_MSG_PROC) ||
	    !find_obj(ts, QTM_OBJ_GEN_CMD_PROC)) {
		pr_err("%s: Required objects are missing\n", __func__);
		err = -ENOENT;
		goto err_missing_objs;
	}

	err = qtouch_read_addr(ts, addr, &checksum[0], sizeof(checksum));
	if (err != 0) {
		pr_err("%s: Unable to read remote checksum\n", __func__);
		err = -ENODEV;
		goto err_no_checksum;
	}

	/* FIXME: The algorithm described in the datasheet doesn't seem to
	 * match what the touch firmware is doing on the other side. We
	 * always get mismatches! */
	their_csum = (checksum[0] |
		(checksum[1] << BITS_PER_BYTE) |
		(checksum[2] << (BITS_PER_BYTE << 1)));

	their_csum = __le32_to_cpu(their_csum);
	if (our_csum != their_csum) {
		pr_warning("%s: Checksum mismatch (0x%08x != 0x%08x)\n",
			   __func__, our_csum, their_csum);
#ifndef IGNORE_CHECKSUM_MISMATCH
		err = -ENODEV;
		goto err_bad_checksum;
#endif
	}

	pr_info("%s: %s found. family 0x%x, variant 0x%x, ver 0x%x, "
		"build 0x%x, matrix %dx%d, %d objects.\n", __func__,
		QTOUCH_TS_NAME, qtm_info.family_id, qtm_info.variant_id,
		qtm_info.version, qtm_info.build, qtm_info.matrix_x_size,
		qtm_info.matrix_y_size, qtm_info.num_objs);

	ts->eeprom_checksum = ts->pdata->nv_checksum;
	ts->family_id = qtm_info.family_id;
	ts->variant_id = qtm_info.variant_id;
	ts->fw_version = qtm_info.version;
	ts->build_version = qtm_info.build;

	return 0;

err_no_checksum:
err_missing_objs:
err_no_msg_proc:
err_read_entry:
err_no_objects:
err_read_info_block:
	return err;
}

static int qtouch_ts_unregister_input(struct qtouch_ts_data *ts)
{
	input_unregister_device(ts->input_dev);
	input_free_device(ts->input_dev);
	ts->input_dev = NULL;
	return 0;
}

static void qtouch_ts_boot_work_func(struct work_struct *work)
{
	int err = 0;
	struct qtouch_ts_data *ts = container_of(work,
						 struct qtouch_ts_data,
						 boot_work);
	unsigned char boot_msg[3];

	err = qtouch_read(ts, &boot_msg, sizeof(boot_msg));
	if (err) {
		pr_err("%s: Cannot read message\n", __func__);
		goto done;
	}
	if (qtouch_tsdebug & 8)
		pr_err("%s: Message is 0x%X err is %i\n",
		       __func__, boot_msg[0], err);

	if (boot_msg[0] == QTM_OBP_BOOT_CRC_CHECK) {
		if (qtouch_tsdebug & 8)
		    pr_err("%s: CRC Check\n", __func__);
		goto done;
	} else if (boot_msg[0] == QTM_OBP_BOOT_CRC_FAIL) {
		if (qtouch_tsdebug & 8)
			pr_err("%s: Boot size %i current pkt size %i\n",
			__func__, ts->boot_pkt_size, ts->current_pkt_sz);

		if (ts->fw_error_count > 3) {
			pr_err("%s: Resetting the IC fw upgrade failed\n",
				__func__);
			goto reset_touch_ic;
		} else {
			/* If this is a failure on the first packet then
			reset the boot packet size to 0 */
			if (!ts->fw_error_count) {
				if (ts->current_pkt_sz == 0) {
					ts->current_pkt_sz = ts->boot_pkt_size;
					ts->boot_pkt_size -= ts->boot_pkt_size;
				}
			}
			ts->fw_error_count++;
			pr_err("%s: Frame CRC check failed %i times\n",
				__func__, ts->fw_error_count);
		}
		goto done;
	} else if (boot_msg[0] == QTM_OBP_BOOT_CRC_PASSED) {
		if (qtouch_tsdebug & 8)
		    pr_err("%s: Frame CRC check passed\n", __func__);

		ts->status =
		    (ts->boot_pkt_size * 100) / ts->touch_fw_size;

		ts->boot_pkt_size += ts->current_pkt_sz;
		ts->fw_error_count = 0;

		/* Check to see if the update is done if it is
		   then register the touch with the system */
		if (ts->boot_pkt_size == ts->touch_fw_size) {
			pr_info("%s: Touch FW update done\n", __func__);
			ts->status = 100;
			goto touch_to_normal_mode;
		}
		goto done;
	} else if (boot_msg[0] & QTM_OBP_BOOT_WAIT_FOR_DATA) {
		if (qtouch_tsdebug & 8)
			pr_err("%s: Data sent so far %i\n",
				__func__, ts->boot_pkt_size);

		/* Don't change the packet size if there was a failure */
		if (!ts->fw_error_count) {
			ts->current_pkt_sz =
			    ((ts->touch_fw_image[ts->boot_pkt_size] << 8) |
				ts->touch_fw_image[ts->boot_pkt_size + 1]) + 2;
		}
		if (qtouch_tsdebug & 8)
			pr_err("%s: Size of the next packet is %i\n",
				__func__, ts->current_pkt_sz);

		err = qtouch_write(ts, &ts->touch_fw_image[ts->boot_pkt_size],
			ts->current_pkt_sz);
		if (err != ts->current_pkt_sz) {
			pr_err("%s: Could not write the packet %i\n",
				__func__, err);
			ts->status = 0xff; /*rkj473 add for support status query.*/
			goto reset_touch_ic;
		}
	} else {
		pr_err("%s: Message is 0x%X is not handled\n",
			__func__, boot_msg[0]);
	}

done:
	enable_irq(ts->client->irq);
	return;

reset_touch_ic:
	qtouch_force_reset(ts, 0);
touch_to_normal_mode:
	ts->client->addr = ts->org_i2c_addr;
	ts->mode = 0;
	/* Wait for the IC to recover */
	msleep(QTM_OBP_SLEEP_WAIT_FOR_RESET);
	err = qtouch_process_info_block(ts);
	if (err != 0) {
		pr_err("%s:Cannot read info block %i\n", __func__, err);
	}
	err = qtouch_ts_register_input(ts);
	if (err != 0)
		pr_err("%s: Registering input failed %i\n", __func__, err);
	enable_irq(ts->client->irq);
	return;

}

static void qtouch_ts_work_func(struct work_struct *work)
{
	struct qtouch_ts_data *ts =
		container_of(work, struct qtouch_ts_data, work);
	struct qtm_obj_message *msg;
	struct qtm_object *obj;
	int ret;

	msg = qtouch_read_msg(ts);
	if (msg == NULL) {
		pr_err("%s: Cannot read message\n", __func__);
		goto done;
	}

	if ((ts->family_id == QTM_OBP_FAMILY_XMEGAT) && (ts->cal_check_flag))
		check_chip_calibration(ts);

	obj = find_object_rid(ts, msg->report_id);
	if (!obj) {
		pr_err("%s: Unknown object for report_id %d\n", __func__,
		       msg->report_id);
		goto done;
	}

	ret = qtouch_handle_msg(ts, obj, msg);
	if (ret != 0) {
		pr_err("%s: Unable to process message for obj %d, "
		       "report_id %d\n", __func__, obj->entry.type,
		       msg->report_id);
		goto done;
	}

done:
    if(qtouch_disable_touch)
	{
		pr_err("%s: Not enabling touch\n", __func__);

	}
	else
	{
		enable_irq(ts->client->irq);
	}
}

static int qtouch_set_boot_mode(struct qtouch_ts_data *ts)
{
	unsigned char FWupdateInfo[3];
	int err;
	int try_again = 0;

	err = qtouch_read(ts, FWupdateInfo, 3);
	if (err)
		pr_err("%s: Could not read back data\n", __func__);

	while ((FWupdateInfo[0] & QTM_OBP_BOOT_CMD_MASK) != QTM_OBP_BOOT_WAIT_FOR_DATA) {
		err = qtouch_read(ts, FWupdateInfo, 3);
		if (err)
			pr_err("%s: Could not read back data\n", __func__);

		if ((FWupdateInfo[0] & QTM_OBP_BOOT_CMD_MASK) == QTM_OBP_BOOT_WAIT_ON_BOOT_CMD) {
			FWupdateInfo[0] = 0xDC;
			FWupdateInfo[1] = 0xAA;
			err = qtouch_write(ts, FWupdateInfo, 2);
			if (err != 2) {
				pr_err("%s: Could not write to BL %i\n",
				       __func__, err);
				return -EIO;
			}
		} else if (try_again > 10) {
				pr_err("%s: Cannot get into bootloader mode\n",
					__func__);
			return -ENODEV;
		} else {
			try_again++;
			msleep(QTM_OBP_SLEEP_WAIT_FOR_BOOT);
		}
	}

	return err;
}

static ssize_t qtouch_irq_status(struct device *dev,
                                 struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = container_of(dev,
	                                         struct i2c_client, dev);
	struct qtouch_ts_data *ts = i2c_get_clientdata(client);
	return sprintf(buf, "%u\n", atomic_read(&ts->irq_enabled));
}

static ssize_t qtouch_irq_enable(struct device *dev,
                                 struct device_attribute *attr,
                                 const char *buf, size_t size)
{
	struct i2c_client *client = container_of(dev,
	                                         struct i2c_client, dev);
	struct qtouch_ts_data *ts = i2c_get_clientdata(client);
	int err = 0;
	unsigned long value;
	struct qtm_obj_message *msg;

	if (size > 2)
		return -EINVAL;

	err = strict_strtoul(buf, 10, &value);
	if (err != 0)
		return err;

	switch (value) {
	case 0:
		if (atomic_cmpxchg(&ts->irq_enabled, 1, 0)) {
			pr_info("touch irq disabled!\n");
			disable_irq_nosync(ts->client->irq);
		}
		err = size;
		break;
	case 1:
		if (!atomic_cmpxchg(&ts->irq_enabled, 0, 1)) {
			pr_info("touch irq enabled!\n");
			msg = qtouch_read_msg(ts);
			if (msg == NULL)
				pr_err("%s: Cannot read message\n", __func__);
			enable_irq(ts->client->irq);
		}
		err = size;
		break;
	default:
		pr_info("qtouch_irq_enable failed -> irq_enabled = %d\n",
		atomic_read(&ts->irq_enabled));
		err = -EINVAL;
		break;
	}

	return err;
}

static DEVICE_ATTR(irq_enable, 0777, qtouch_irq_status, qtouch_irq_enable);

static ssize_t qtouch_update_status(struct device *dev,
				    struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = container_of(dev,
						 struct i2c_client, dev);
	struct qtouch_ts_data *ts = i2c_get_clientdata(client);

	return sprintf(buf, "%u\n", ts->status);
}

static DEVICE_ATTR(update_status, 0777, qtouch_update_status, NULL);

static ssize_t qtouch_fw_version(struct device *dev,
				 struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = container_of(dev,
						 struct i2c_client, dev);
	struct qtouch_ts_data *ts = i2c_get_clientdata(client);

	return sprintf(buf, "0x%X%X\n", ts->fw_version, ts->build_version);
}

static DEVICE_ATTR(fw_version, 0777, qtouch_fw_version, NULL);

static int qtouch_ts_probe(struct i2c_client *client,
			   const struct i2c_device_id *id)
{
	struct qtouch_ts_platform_data *pdata = client->dev.platform_data;
	struct qtouch_ts_data *ts;
	int err;
	struct touch_fw_entry *touch_fw_table_ptr;
	unsigned char boot_info;
	int loop_count;

	if (pdata == NULL) {
		pr_err("%s: platform data required\n", __func__);
		return -ENODEV;
	} else if (!client->irq) {
		pr_err("%s: polling mode currently not supported\n", __func__);
		return -ENODEV;
	} else if (!pdata->hw_reset) {
		pr_err("%s: Must supply a hw reset function\n", __func__);
		return -ENODEV;
	}

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		pr_err("%s: need I2C_FUNC_I2C\n", __func__);
		return -ENODEV;
	}

	ts = kzalloc(sizeof(struct qtouch_ts_data), GFP_KERNEL);
	if (ts == NULL) {
		err = -ENOMEM;
		goto err_alloc_data_failed;
	}
	ts->pdata = pdata;
	ts->client = client;
	i2c_set_clientdata(client, ts);
	ts->checksum_cnt = 0;
	ts->fw_version = 0;
	ts->build_version = 0;
	ts->fw_error_count = 0;
	ts->current_pkt_sz = 0;
	ts->x_delta = ts->pdata->x_delta;
	ts->y_delta = ts->pdata->y_delta;
	atomic_set(&ts->irq_enabled, 1);
	ts->status = 0xfe; /*rkj473 add for support status query.*/
	ts->touch_fw_size = 0;
	ts->touch_fw_image = NULL;
	ts->base_fw_version = 0;
	touch_fw_table_ptr = touch_fw_table;

	qtouch_force_reset(ts, 0);
	err = qtouch_process_info_block(ts);
	if ((err == 0) && (ts->family_id == 0x31)) {
		pr_info("%s: Need to verify FW checksum\n", __func__);
		if (ts->fw_version >= ts->base_fw_version) {
			qtouch_force_reset(ts, 2);
			msleep(QTM_OBP_SLEEP_WAIT_FOR_RESET);
			ts->org_i2c_addr = ts->client->addr;
			ts->client->addr = ts->pdata->boot_i2c_addr;
			err = qtouch_set_boot_mode(ts);
			if (err < 0)
				pr_err("%s: Failed unlocking IC in boot mode %i\n",
				       __func__, err);
			if (qtouch_tsdebug) {
				qtouch_read(ts, &boot_info, 1);
				pr_info("%s:Data read 0x%x\n", __func__, boot_info);
			}
			ts->client->addr = ts->org_i2c_addr;
			qtouch_force_reset(ts, 0);
			msleep(QTM_OBP_SLEEP_WAIT_FOR_CKSUM);
		} else {
			pr_err("%s:Can't verify FW checksum - FW 0x%X does not support boot mode\n",
			       __func__, ts->fw_version);
		}
		err = qtouch_process_info_block(ts);
	}
	if (err == 0) {
		pr_info("%s: FW version is 0x%X Build 0x%X\n", __func__,
					   ts->fw_version, ts->build_version);

		while (touch_fw_table_ptr->family_id != 0x00) {
			if ((ts->family_id == touch_fw_table_ptr->family_id)
			    && (ts->variant_id == touch_fw_table_ptr->variant_id)) {
				pr_info("%s: Chip type matched\n", __func__);

				if ((ts->fw_version != touch_fw_table_ptr->fw_version)
				    || (ts->build_version != touch_fw_table_ptr->fw_build)) {
					pr_info("%s: Reflash needed\n", __func__);
					ts->touch_fw_size = touch_fw_table_ptr->size;
					ts->touch_fw_image = touch_fw_table_ptr->image;
					ts->base_fw_version = touch_fw_table_ptr->base_fw_version;
				} else {
					pr_info("%s: Reflash not needed\n", __func__);
				}
				break;
			}
			touch_fw_table_ptr++;
		}

		if ((ts->touch_fw_size != 0) && (ts->touch_fw_image != NULL)) {
			/* Reset the chip into bootloader mode */
			if (ts->fw_version >= ts->base_fw_version) {
				qtouch_force_reset(ts, 2);
				msleep(QTM_OBP_SLEEP_WAIT_FOR_RESET);

				ts->org_i2c_addr = ts->client->addr;
				ts->client->addr = ts->pdata->boot_i2c_addr;
			} else {
				pr_err("%s:FW 0x%X does not support boot mode\n",
				       __func__, ts->fw_version);
				ts->touch_fw_size = 0;
				ts->touch_fw_image = NULL;
			}
		}
	} else {
		pr_info("%s:Cannot read info block %i, checking for bootloader mode.\n", __func__, err);

		qtouch_force_reset(ts, 0);
		msleep(QTM_OBP_SLEEP_WAIT_FOR_RESET);

		ts->org_i2c_addr = ts->client->addr;
		ts->client->addr = ts->pdata->boot_i2c_addr;

		err = qtouch_read(ts, &boot_info, 1);
		if (err) {
			pr_err("%s:Read failed %d\n", __func__, err);
		} else {
			pr_info("%s:Data read 0x%x\n", __func__, boot_info);
			loop_count = 0;
			while ((boot_info & QTM_OBP_BOOT_CMD_MASK) != QTM_OBP_BOOT_WAIT_ON_BOOT_CMD) {
				err = qtouch_read(ts, &boot_info, 1);
				if (err) {
					pr_err("%s:Read failed %d\n", __func__, err);
					break;
				}
				pr_info("%s:Data read 0x%x\n", __func__, boot_info);
				loop_count++;
				if (loop_count == 10) {
					err = 1;
					break;
				}
			}
		}
		if (!err) {
			boot_info &= QTM_OBP_BOOT_VERSION_MASK;
			pr_info("%s:Bootloader version %d\n", __func__, boot_info);

			while (touch_fw_table_ptr->family_id != 0x00) {
				if (boot_info == touch_fw_table_ptr->boot_version) {
					pr_info("%s: Chip type matched\n", __func__);
					ts->touch_fw_size = touch_fw_table_ptr->size;
					ts->touch_fw_image = touch_fw_table_ptr->image;
					ts->base_fw_version = touch_fw_table_ptr->base_fw_version;
					break;
				}
				touch_fw_table_ptr++;
			}
		}
	}

	INIT_WORK(&ts->work, qtouch_ts_work_func);
	INIT_WORK(&ts->boot_work, qtouch_ts_boot_work_func);

	if (ts->pdata->flags & QTOUCH_USE_KEYARRAY) {
		setup_timer(&keyarray_timer, keyarray_ignore_timer, (unsigned long) ts);
	}

	if ((ts->touch_fw_size != 0) && (ts->touch_fw_image != NULL)) {
		err = qtouch_set_boot_mode(ts);
		if (err < 0) {
			pr_err("%s: Failed setting IC in boot mode %i\n",
			       __func__, err);
			/* We must have been in boot mode to begin with
			or the IC is not present so just exit out of probe */
			if (ts->fw_version == 0) {
				ts->status = 0xfd;
				return err;
			}

			ts->client->addr = ts->org_i2c_addr;
			qtouch_force_reset(ts, 0);
			pr_err("%s: I2C address is 0x%X\n",
				__func__, ts->client->addr);
			err = qtouch_process_info_block(ts);
			if (err) {
				pr_err("%s: Failed reading info block %i\n",
				       __func__, err);
				goto err_reading_info_block;
			}
			goto err_boot_mode_failure;
		}

		ts->mode = 1;
		/* Add 2 because the firmware packet size bytes
		are not taken into account for the total size */
		ts->boot_pkt_size = ((ts->touch_fw_image[0] << 8) |
			ts->touch_fw_image[1]) + 2;

		err = qtouch_write(ts, &ts->touch_fw_image[0], ts->boot_pkt_size);
		if (err != ts->boot_pkt_size) {
			pr_err("%s: Could not write the first packet %i\n",
			       __func__, err);
                        ts->status = 0xff;  /*rkj473 add for support status query.*/
			ts->client->addr = ts->org_i2c_addr;
			qtouch_force_reset(ts, 0);
			err = qtouch_process_info_block(ts);
			if (err) {
				pr_err("%s: Failed reading info block %i\n",
				__func__, err);
				goto err_reading_info_block;
			}
			goto err_boot_mode_failure;
		}
		goto finish_touch_setup;

	}

/* If the update should fail the touch should still work */
err_boot_mode_failure:
	ts->mode = 0;
	err = qtouch_ts_register_input(ts);
	if (err != 0) {
		pr_err("%s: Registering input failed %i\n", __func__, err);
		goto err_input_register_dev;
	}

finish_touch_setup:
	err = request_irq(ts->client->irq, qtouch_ts_irq_handler,
			  IRQ_DISABLED | pdata->irqflags, "qtouch_ts_int", ts);
	if (err != 0) {
		pr_err("%s: request_irq (%d) failed\n", __func__,
		       ts->client->irq);
		goto err_request_irq;
	}

	err = device_create_file(&ts->client->dev, &dev_attr_irq_enable);
	if (err != 0) {
		pr_err("%s:File device creation failed: %d\n", __func__, err);
		err = -ENODEV;
		goto err_create_file_failed;
	}

	err = device_create_file(&ts->client->dev, &dev_attr_update_status);
	if (err != 0) {
		pr_err("%s:File device creation failed: %d\n", __func__, err);
		err = -ENODEV;
		goto err_create_update_status_failed;
	}

	err = device_create_file(&ts->client->dev, &dev_attr_fw_version);
	if (err != 0) {
		pr_err("%s:File device creation failed: %d\n", __func__, err);
		err = -ENODEV;
		goto err_create_fw_version_file_failed;
	}

#ifdef CONFIG_HAS_EARLYSUSPEND
	ts->early_suspend.level = EARLY_SUSPEND_LEVEL_DISABLE_FB + 1;
	ts->early_suspend.suspend = qtouch_ts_early_suspend;
	ts->early_suspend.resume = qtouch_ts_late_resume;
	register_early_suspend(&ts->early_suspend);
#endif

	ts->cal_check_flag = 0;
	ts->cal_timer = 0;

	return 0;

err_create_fw_version_file_failed:
	device_remove_file(&ts->client->dev, &dev_attr_update_status);
err_create_update_status_failed:
	device_remove_file(&ts->client->dev, &dev_attr_irq_enable);
err_create_file_failed:
	free_irq(ts->client->irq, ts);
err_request_irq:
	qtouch_ts_unregister_input(ts);

err_input_register_dev:
err_reading_info_block:
	i2c_set_clientdata(client, NULL);

	if (ts->pdata->flags & QTOUCH_USE_KEYARRAY)
		del_timer(&keyarray_timer);

	kfree(ts);
	ts = NULL;

err_alloc_data_failed:
	return err;
}

static int qtouch_ts_remove(struct i2c_client *client)
{
	struct qtouch_ts_data *ts = i2c_get_clientdata(client);

	device_remove_file(&ts->client->dev, &dev_attr_irq_enable);
	device_remove_file(&ts->client->dev, &dev_attr_update_status);
	device_remove_file(&ts->client->dev, &dev_attr_fw_version);

	if (ts->pdata->flags & QTOUCH_USE_KEYARRAY)
		del_timer(&keyarray_timer);

	unregister_early_suspend(&ts->early_suspend);
	free_irq(ts->client->irq, ts);
	qtouch_ts_unregister_input(ts);
	i2c_set_clientdata(client, NULL);
	kfree(ts);
	return 0;
}

static int qtouch_ts_suspend(struct i2c_client *client, pm_message_t mesg)
{
	struct qtouch_ts_data *ts = i2c_get_clientdata(client);
	int ret;
	if (qtouch_tsdebug & 4)
		pr_info("%s: Suspending\n", __func__);

	if (!atomic_read(&ts->irq_enabled))
		return 0;

	if (ts->mode == 1)
		return -EBUSY;

	disable_irq_nosync(ts->client->irq);
	ret = cancel_work_sync(&ts->work);
	if (ret) { /* if work was pending disable-count is now 2 */
		pr_info("%s: Pending work item\n", __func__);
		enable_irq(ts->client->irq);
	}

	ret = qtouch_power_config(ts, 0);
	if (ret < 0)
		pr_err("%s: Cannot write power config\n", __func__);

	return 0;
}

static int qtouch_ts_resume(struct i2c_client *client)
{
	struct qtouch_ts_data *ts = i2c_get_clientdata(client);
	int ret;
	int i;

	if (qtouch_tsdebug & 4)
		pr_info("%s: Resuming\n", __func__);

	if (!atomic_read(&ts->irq_enabled))
		return 0;

	if (ts->mode == 1)
		return -EBUSY;

	/* If we were suspended while a touch was happening
	   we need to tell the upper layers so they do not hang
	   waiting on the liftoff that will not come. */
	for (i = 0; i < ts->pdata->multi_touch_cfg.num_touch; i++) {
		if (qtouch_tsdebug & 4)
			pr_info("%s: Finger %i down state %i\n",
				__func__, i, ts->finger_data[i].down);
		if (ts->finger_data[i].down == 0)
			continue;
		input_report_abs(ts->input_dev, ABS_MT_TOUCH_MAJOR, 0);
		input_mt_sync(ts->input_dev);
		memset(&ts->finger_data[i], 0, sizeof(struct coordinate_map));
	}
	input_sync(ts->input_dev);

	ret = qtouch_power_config(ts, 1);
	if (ret < 0) {
		pr_err("%s: Cannot write power config\n", __func__);
		return -EIO;
	}
	qtouch_force_reset(ts, 0);

	enable_irq(ts->client->irq);
	return 0;
}

#ifdef CONFIG_HAS_EARLYSUSPEND
static void qtouch_ts_early_suspend(struct early_suspend *handler)
{
	struct qtouch_ts_data *ts;

	ts = container_of(handler, struct qtouch_ts_data, early_suspend);
	qtouch_ts_suspend(ts->client, PMSG_SUSPEND);
}

static void qtouch_ts_late_resume(struct early_suspend *handler)
{
	struct qtouch_ts_data *ts;

	ts = container_of(handler, struct qtouch_ts_data, early_suspend);
	qtouch_ts_resume(ts->client);
}
#endif

static uint8_t calibrate_chip(struct qtouch_ts_data *ts)
{
	uint8_t atchcalst, atchcalsthr, tchautocal;
	struct qtm_object *obj;
	int ret;

	ts->cal_timer = 0;

	atchcalst = ts->pdata->acquire_cfg.atch_cal_suspend_time;
	atchcalsthr = ts->pdata->acquire_cfg.atch_cal_suspend_thres;
	tchautocal = ts->pdata->acquire_cfg.touch_autocal;
	ts->pdata->acquire_cfg.atch_cal_suspend_time = 0;
	ts->pdata->acquire_cfg.atch_cal_suspend_thres = 0;
	ts->pdata->acquire_cfg.touch_autocal = 5;

	if (qtouch_tsdebug & 16)
		pr_info("%s: issue cal. calst=%d, calsthr=%d\n",
			__func__, atchcalst, atchcalsthr);

	/* configure the acquisition object. */
	obj = find_obj(ts, QTM_OBJ_GEN_ACQUIRE_CONF);
	ret = qtouch_write_addr(ts, obj->entry.addr, &ts->pdata->acquire_cfg,
				min(sizeof(ts->pdata->acquire_cfg),
				    obj->entry.size));
	if (ret != 0) {
		pr_err("%s: Can't write acquisition config\n", __func__);
		return ret;
	}

	ts->pdata->acquire_cfg.atch_cal_suspend_time = atchcalst;
	ts->pdata->acquire_cfg.atch_cal_suspend_thres = atchcalsthr;
	ts->pdata->acquire_cfg.touch_autocal = tchautocal;

	ret = qtouch_force_calibration(ts);
	if (ret != 0) {
		pr_err("%s: Unable to write to calibrate\n", __func__);
		return ret;
	}

	/* Reset the address pointer after forcing a calibration */
	ret = qtouch_set_addr(ts, ts->obj_tbl[QTM_OBJ_GEN_MSG_PROC].entry.addr);
	if (ret != 0) {
		pr_err("%s: Unable to reset addr pointer after calibration\n",
		       __func__);
		return ret;
	}

	return 0;
}

static uint8_t check_chip_calibration(struct qtouch_ts_data *ts)
{
	uint8_t val, num_x_lines, num_y_lines;
	uint16_t bytes_to_read, maskl, maskh;
	struct qtm_object *obj;
	int ret, try_ctr = 0;
	uint8_t *data_buffer = NULL;
	uint8_t i;
	uint8_t *diag_ptr = NULL;
	int touch = 0;
	int atouch = 0;
	uint16_t addr;
	unsigned long current_jiffies;

	obj = find_obj(ts, QTM_OBJ_GEN_CMD_PROC);
	addr = obj->entry.addr + offsetof(struct qtm_gen_cmd_proc, diagnostic);
	val = 0xF3;
	ret = qtouch_write_addr(ts, addr, &val, 1);
	if (ret) {
		pr_err("%s: Unable to send the diagnostic message\n", __func__);
		goto err_diag_data;
	}

	data_buffer = kmalloc(QTM_OBP_DIAG_BUFFER_SIZE, GFP_KERNEL);
	if (data_buffer == NULL) {
		pr_err("%s: Unable to allocate memory for diag buffer\n",
			 __func__);
		goto err_diag_data;
	}

	msleep(QTM_OBP_SLEEP_WAIT_FOR_DIAG_DATA);
	memset(data_buffer, 0xFF, QTM_OBP_DIAG_BUFFER_SIZE);

	obj = find_obj(ts, QTM_OBJ_DBG_DIAG);
	while (!((*data_buffer == 0xF3) && (*(data_buffer + 1) == 0x00))) {
		if (try_ctr > QTM_OBP_NUM_DIAG_READ_RETRIES) {
			pr_err("%s: Failed to get Diag block response\n",
				__func__);
			ret = -1;
			goto err_diag_data;
		}

		try_ctr++;
		msleep(QTM_OBP_SLEEP_WAIT_FOR_DIAG_DATA);
		ret = qtouch_read_addr(ts, obj->entry.addr, data_buffer, 2);

		if (ret != 0) {
			pr_err("%s: Cannot read diag block header\n", __func__);
			goto err_diag_data;
		}
	}

	/* Always read 82 bytes; (20 * 2 * 2) + 2 */
	num_x_lines = ts->pdata->multi_touch_cfg.x_size;
	num_y_lines = ts->pdata->multi_touch_cfg.y_size;
	bytes_to_read = (QTM_OBP_DIAG_NUM_X_LINES * 2 * 2) + 2;

	if (bytes_to_read > QTM_OBP_DIAG_BUFFER_SIZE) {
		pr_err("%s: data_buffer not enough\n", __func__);
		ret = -1;
		goto err_diag_data;
	}

	if (qtouch_tsdebug & 16)
		pr_info("%s: Reading diag info. xsize=%d, \
			ysize=%d, read_size = %d\n",
			__func__, num_x_lines, num_y_lines, bytes_to_read);

	ret = qtouch_read_addr(ts, obj->entry.addr, data_buffer,
		bytes_to_read);

	if (ret != 0) {
		pr_err("%s: Cannot read diag block data\n", __func__);
		goto err_diag_data;
	}

	if (qtouch_tsdebug & 32) {
		int msg_bytes;
		int msg_location;
		char *msg;
		int index;

		msg = kmalloc(1024, GFP_KERNEL);
		if (msg != NULL) {
			msg_location = sprintf(msg, "%s: Diag data:",
				__func__);
			for (index = 0; index < QTM_OBP_DIAG_BUFFER_SIZE;
				index++) {
				msg_bytes = snprintf((msg + msg_location),
					(1024 - msg_location),
					" 0x%02x",
					*(data_buffer + index));
				msg_location += msg_bytes;
				if (msg_location >= 1024)
					break;
			}
			if (msg_location < 1024) {
				pr_info("%s\n", msg);
			} else {
				pr_info("%s:  Diag data: String overflow\n",
				__func__);
			}

			kfree(msg);
		}
	}

	diag_ptr = data_buffer + 2;
	maskl = ((1 << num_y_lines) - 1) & (0x00FF);
	maskh = (((1 << num_y_lines) - 1) & (0xFF00)) >> 8;

	if ((*data_buffer == 0xF3) && (*(data_buffer + 1) == 0)) {
		for (i = 0; i < num_x_lines; i++) {
			touch += hweight8((uint8_t)(*diag_ptr & maskl));
			touch += hweight8((uint8_t)(*(diag_ptr + 1) & maskh));
			atouch += hweight8(
				(uint8_t)(*(diag_ptr
				+ (QTM_OBP_DIAG_NUM_X_LINES<<1)) & maskl));
			atouch += hweight8(
				(uint8_t)(*(diag_ptr
				+ (QTM_OBP_DIAG_NUM_X_LINES<<1) + 1) & maskh));
			diag_ptr += 2;
		}
	} else {
		pr_err("%s: Diag block header not correct\n", __func__);
		ret = -1;
		goto err_diag_data;
	}

	if (qtouch_tsdebug & 16)
		pr_info("%s: Processed diag data. touch=%d, atouch=%d\n",
			__func__, touch, atouch);

	obj = find_obj(ts, QTM_OBJ_GEN_CMD_PROC);
	addr = obj->entry.addr + offsetof(struct qtm_gen_cmd_proc, diagnostic);
	val = 0x01;
	ret = qtouch_write_addr(ts, addr, &val, 1);
	if (ret) {
		pr_err("%s: Unable to send pageup diagnostic message\n",
			__func__);
		goto err_diag_data;
	}

	if (touch && (atouch == 0)) {
		current_jiffies = jiffies;

		if (qtouch_tsdebug & 16)
			pr_info("%s: cal_timer=%lu curr_jiffies=%lu\n",
				__func__, ts->cal_timer, current_jiffies);

		if (ts->cal_timer == 0) {
			ts->cal_timer = current_jiffies;
		} else if ((((long)current_jiffies -
				(long)ts->cal_timer) * 1000 / HZ) >=
				QTM_OBP_DIAG_CALIB_WAIT_MS) {

			ts->cal_check_flag = 0;
			ts->cal_timer = 0;
			obj = find_obj(ts, QTM_OBJ_GEN_ACQUIRE_CONF);
			ret = qtouch_write_addr(ts, obj->entry.addr,
						&ts->pdata->acquire_cfg,
						min(sizeof(
							ts->pdata->acquire_cfg),
						obj->entry.size));
			if (ret != 0) {
				pr_err("%s: Can't write acquisition config\n",
					__func__);
				goto err_diag_data;
			}
		}
	} else if (0 < atouch) {
		calibrate_chip(ts);
	} else {
		if (qtouch_tsdebug & 16)
			pr_info("%s: cannot decide.  Wait for next data\n",
				__func__);
	}

err_diag_data:
	if (data_buffer != NULL)
		kfree(data_buffer);

	ret = qtouch_set_addr(ts, ts->obj_tbl[QTM_OBJ_GEN_MSG_PROC].entry.addr);
	if (ret != 0) {
		pr_err("%s: Unable to reset address pointer to Gen Msg\n",
		       __func__);
	}

	return ret;
}

/******** init ********/
static const struct i2c_device_id qtouch_ts_id[] = {
	{ QTOUCH_TS_NAME, 0 },
	{ }
};

static struct i2c_driver qtouch_ts_driver = {
	.probe		= qtouch_ts_probe,
	.remove		= qtouch_ts_remove,
#ifndef CONFIG_HAS_EARLYSUSPEND
	.suspend	= qtouch_ts_suspend,
	.resume		= qtouch_ts_resume,
#endif
	.id_table	= qtouch_ts_id,
	.driver = {
		.name	= QTOUCH_TS_NAME,
		.owner	= THIS_MODULE,
	},
};

static int __devinit qtouch_ts_init(void)
{
	struct proc_dir_entry *proc_entry;

	memset(&buf, 0, sizeof(buf));

	proc_root = proc_mkdir(MULTITOUCH_TAG, NULL);
	proc_entry = create_proc_read_entry("num", 0666, proc_root, proc_numtouch_read, NULL);
	proc_entry->write_proc = proc_numtouch_write;

	wanted_touch = touch_num;

	/* reset will provoke qtouch_set_addr call, so we can get the ts struct address immediately */
	SYMSEARCH_BIND_FUNCTION_TO(qtouch_obp_ts2, mapphone_touch_reset, ss_mapphone_touch_reset);
	ss_mapphone_touch_reset();

	printk(KERN_INFO MULTITOUCH_TAG": wrong parameter for wanted_touch\n");
	
	qtouch_ts_wq = create_singlethread_workqueue("qtouch_obp_ts_wq");
	if (qtouch_ts_wq == NULL) {
		pr_err("%s: No memory for qtouch_ts_wq\n", __func__);
		return -ENOMEM;
	}
	return i2c_add_driver(&qtouch_ts_driver);
}

static void __exit qtouch_ts_exit(void)
{
	i2c_del_driver(&qtouch_ts_driver);
	if (qtouch_ts_wq)
		destroy_workqueue(qtouch_ts_wq);
	
	if (proc_root) {
		remove_proc_entry("num", proc_root);
		remove_proc_entry(MULTITOUCH_TAG, NULL);
	}
}

module_init(qtouch_ts_init);
module_exit(qtouch_ts_exit);

MODULE_AUTHOR("Dima Zavin <dima@android.com>");
MODULE_DESCRIPTION("Quantum OBP Touchscreen Driver");
MODULE_LICENSE("GPL");
