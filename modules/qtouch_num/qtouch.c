/*
 * qtouch_num - 5 point multitouch for Milestone, by Nadlabak
 * hooking taken from "n - for testing kernel function hooking" by Nothize
 * needs symsearch module by Skrilax
 *
 * Copyright (C) 2011 Nadlabak, 2010 Nothize
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
#include <linux/device.h>
#include <linux/earlysuspend.h>
#include <linux/qtouch_obp_ts.h>

#include "hook.h"

struct qtm_object {
	struct qtm_obj_entry		entry;
	uint8_t				report_id_min;
	uint8_t				report_id_max;
};

struct coordinate_map {
	int x_data;
	int y_data;
	int z_data;
	int w_data;
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
	uint16_t			eeprom_checksum;
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

	uint8_t				*msg_buf;
	int				msg_size;
};

uint16_t eeprom_checksum;
bool checksumNeedsCorrection = false;
bool done = false;
struct qtouch_ts_data *ts;

static int qtouch_read(struct qtouch_ts_data *ts, void *buf, int buf_sz) {
	if (!done && ts->pdata) {
		done = true;
		ts->pdata->multi_touch_cfg.num_touch = 5;
		printk(KERN_INFO "qtouch_num: num_touch set to %u \n",ts->pdata->multi_touch_cfg.num_touch);
		printk(KERN_INFO "qtouch_num: forcing checksum error to cause qtouch_hw_init call \n");
		// qtouch_hw_init will be invoked on next qtouch resume (screen on)
		ts->pdata->flags |= QTOUCH_EEPROM_CHECKSUM;
		eeprom_checksum = ts->eeprom_checksum;
		ts->eeprom_checksum = 0;
		ts->checksum_cnt = 0;
		checksumNeedsCorrection = true;
	} else if (!done) {
		printk(KERN_INFO "qtouch_num: ts->pdata is null! \n");
	}
	return HOOK_INVOKE(qtouch_read, ts, buf, buf_sz);
}

static int qtouch_hw_init(struct qtouch_ts_data *ts) {
	if (checksumNeedsCorrection) {
		ts->eeprom_checksum = eeprom_checksum;
		checksumNeedsCorrection = false;
	}
	return HOOK_INVOKE(qtouch_hw_init, ts);
}

struct hook_info g_hi[] = {
	HOOK_INIT(qtouch_read),
	HOOK_INIT(qtouch_hw_init),
	HOOK_INIT_END
};

static int __init qtouch_init(void)
{
/* ts struct address previously found for final Milestone kernel, not used
	ts = (struct qtouch_ts_data *) 0xced71400; */
	hook_init();
	return 0;
}

static void __exit qtouch_exit(void)
{
	hook_exit();
}

module_init(qtouch_init);
module_exit(qtouch_exit);

MODULE_ALIAS("QTOUCH_NUM");
MODULE_DESCRIPTION("change qtouch num_touch via kernel function hook");
MODULE_AUTHOR("Nadlabak, Nothize");
MODULE_LICENSE("GPL");
