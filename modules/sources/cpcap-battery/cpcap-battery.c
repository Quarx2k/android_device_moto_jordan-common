/*
 * Copyright (C) 2007-2009 Motorola, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
 * 02111-1307, USA
 */

#include <asm/div64.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/platform_device.h>
#include <linux/poll.h>
#include <linux/power_supply.h>
#include <linux/types.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/spi/cpcap.h>
#include <linux/spi/cpcap-regbits.h>
#include <linux/spi/spi.h>
#include <linux/time.h>
#include <linux/miscdevice.h>
#include <linux/debugfs.h>
#include "../symsearch/symsearch.h"
#include <linux/delay.h>
#include <linux/kthread.h>
#include <linux/smp_lock.h>

#include "cpcap_charge_table.h"

SYMSEARCH_DECLARE_FUNCTION_STATIC(unsigned long long, sched_clock2, void);
SYMSEARCH_DECLARE_FUNCTION_STATIC(int, cpcap_regacc_write2, struct cpcap_device *cpcap, enum cpcap_reg reg,
		       unsigned short value, unsigned short mask);
SYMSEARCH_DECLARE_FUNCTION_STATIC(int, cpcap_regacc_read2, struct cpcap_device *cpcap, enum cpcap_reg reg,
		      unsigned short *value_ptr);

#define CPCAP_BATT_IRQ_BATTDET 0x01
#define CPCAP_BATT_IRQ_OV      0x02
#define CPCAP_BATT_IRQ_CC_CAL  0x04
#define CPCAP_BATT_IRQ_ADCDONE 0x08
#define CPCAP_BATT_IRQ_MACRO   0x10
#define SAMPLING_RATE_MS	20000
#define BATTERY_DEBUG

static unsigned long sampling_rate;
static struct workqueue_struct *wq;
static struct delayed_work update_battery;
static int cpcap_batt_probe(struct platform_device *pdev);
static int cpcap_batt_remove(struct platform_device *pdev);
static int cpcap_batt_resume(struct platform_device *pdev);

static int cpcap_batt_init(void);
static int __init cpcap_battery_unload_init(void);

struct driver_private {
	struct kobject kobj;
};

struct cpcap_batt_ps {
	struct power_supply batt;
	struct power_supply ac;
	struct power_supply usb;
	struct cpcap_device *cpcap;
	struct cpcap_batt_data batt_state;
	struct cpcap_batt_ac_data ac_state;
	struct cpcap_batt_usb_data usb_state;
	struct cpcap_adc_request req;
	struct mutex lock;
	char irq_status;
	char data_pending;
	wait_queue_head_t wait;
	char async_req_pending;
	unsigned long last_run_time;
	bool no_update;
	unsigned int battery_stats_counter[4]; /* Battery stats for past seconds */
};
static enum power_supply_property cpcap_batt_props[] = {
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_HEALTH,
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_TECHNOLOGY,
	POWER_SUPPLY_PROP_CAPACITY,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_TEMP,
	POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN,
	POWER_SUPPLY_PROP_CHARGE_COUNTER
};

static enum power_supply_property cpcap_batt_ac_props[] =
{
	POWER_SUPPLY_PROP_ONLINE,
	POWER_SUPPLY_PROP_MODEL_NAME
};

static enum power_supply_property cpcap_batt_usb_props[] =
{
	POWER_SUPPLY_PROP_ONLINE,
	POWER_SUPPLY_PROP_CURRENT_NOW,
	POWER_SUPPLY_PROP_MODEL_NAME
};

static struct platform_driver cpcap_batt_driver = {
	.probe		= cpcap_batt_probe,
	.remove		= cpcap_batt_remove,
	.resume		= cpcap_batt_resume,
	.driver		= {
		.name	= "cpcap_battery",
		.owner	= THIS_MODULE,
	},
};

static struct cpcap_batt_ps *cpcap_batt_sply;
static int cpcap_batt_status(struct cpcap_batt_ps *sply);
static int cpcap_batt_counter(struct cpcap_batt_ps *sply);
static int cpcap_batt_value(struct cpcap_batt_ps *sply, int value);

void cpcap_batt_irq_hdlr(enum cpcap_irqs irq, void *data)
{
	struct cpcap_batt_ps *sply = data;

	mutex_lock(&sply->lock);
	sply->data_pending = 1;

	switch (irq) {
	case CPCAP_IRQ_BATTDETB:
		sply->irq_status |= CPCAP_BATT_IRQ_BATTDET;
		cpcap_irq_unmask(sply->cpcap, irq);
		break;

	case CPCAP_IRQ_VBUSOV:
		sply->irq_status |=  CPCAP_BATT_IRQ_OV;
		cpcap_irq_unmask(sply->cpcap, irq);
		break;

	case CPCAP_IRQ_CC_CAL:
		sply->irq_status |= CPCAP_BATT_IRQ_CC_CAL;
		cpcap_irq_unmask(sply->cpcap, irq);
		break;

	case CPCAP_IRQ_UC_PRIMACRO_7:
	case CPCAP_IRQ_UC_PRIMACRO_8:
	case CPCAP_IRQ_UC_PRIMACRO_9:
	case CPCAP_IRQ_UC_PRIMACRO_10:
	case CPCAP_IRQ_UC_PRIMACRO_11:
		sply->irq_status |= CPCAP_BATT_IRQ_MACRO;
		break;
	default:
		break;
	}

	mutex_unlock(&sply->lock);

	wake_up_interruptible(&sply->wait);
}

void cpcap_batt_adc_hdlr(struct cpcap_device *cpcap, void *data)
{
	struct cpcap_batt_ps *sply = data;
	mutex_lock(&sply->lock);

	sply->async_req_pending = 0;

	sply->data_pending = 1;

	sply->irq_status |= CPCAP_BATT_IRQ_ADCDONE;

	mutex_unlock(&sply->lock);

	wake_up_interruptible(&sply->wait);
}
static char *cpcap_batt_ac_models[] = {
	"none", "charger"
};

static int cpcap_batt_ac_get_property(struct power_supply *psy,
				      enum power_supply_property psp,
				      union power_supply_propval *val)
{
	int ret = 0;
	struct cpcap_batt_ps *sply = container_of(psy, struct cpcap_batt_ps,
						 ac);

	switch (psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		val->intval = sply->ac_state.online;
		break;
	case POWER_SUPPLY_PROP_MODEL_NAME:
		val->strval = cpcap_batt_ac_models[sply->ac_state.online];
		break;
	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}

static char *cpcap_batt_usb_models[] = {
	"none", "usb", "factory"
};

static int cpcap_batt_usb_get_property(struct power_supply *psy,
				       enum power_supply_property psp,
				       union power_supply_propval *val)
{
	int ret = 0;
	struct cpcap_batt_ps *sply = container_of(psy, struct cpcap_batt_ps,
						 usb);

	switch (psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		val->intval = sply->usb_state.online;
		break;
	case POWER_SUPPLY_PROP_CURRENT_NOW:
		val->intval = sply->usb_state.current_now;
		break;
	case POWER_SUPPLY_PROP_MODEL_NAME:
		val->strval = cpcap_batt_usb_models[sply->usb_state.model];
		break;
	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}

static int cpcap_batt_get_property(struct power_supply *psy,
				   enum power_supply_property psp,
				   union power_supply_propval *val)
{
	int ret = 0;
	struct cpcap_batt_ps *sply = container_of(psy, struct cpcap_batt_ps,
						  batt);

	switch (psp) {
	case POWER_SUPPLY_PROP_STATUS:
		val->intval = cpcap_batt_status(sply);
		break;

	case POWER_SUPPLY_PROP_HEALTH:
		val->intval = sply->batt_state.health;
		break;

	case POWER_SUPPLY_PROP_PRESENT:
		val->intval = sply->batt_state.present;
		break;

	case POWER_SUPPLY_PROP_TECHNOLOGY:
		val->intval = POWER_SUPPLY_TECHNOLOGY_LION;
		break;

	case POWER_SUPPLY_PROP_CAPACITY:
		val->intval = cpcap_batt_counter(sply);
		break;

	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		val->intval = cpcap_batt_value(sply, CPCAP_ADC_BATTP)*1000;
		break;

	case POWER_SUPPLY_PROP_TEMP:
		val->intval = (cpcap_batt_value(sply, CPCAP_ADC_AD3)-273)*10;
		break;

	case POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN:
		val->intval = sply->batt_state.batt_full_capacity;
		break;

	case POWER_SUPPLY_PROP_CHARGE_COUNTER:
		val->intval = cpcap_batt_counter(sply);
		break;

	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}
static int cpcap_batt_status(struct cpcap_batt_ps *sply) {
	int amperage = 0;
	SYMSEARCH_BIND_FUNCTION_TO(cpcap-battery, cpcap_regacc_write, cpcap_regacc_write2);
	SYMSEARCH_BIND_FUNCTION_TO(cpcap-battery, cpcap_regacc_read, cpcap_regacc_read2);

	amperage = cpcap_batt_value(sply, CPCAP_ADC_CHG_ISENSE);
	if (sply->usb_state.online == 1 || sply->ac_state.online == 1) {
		cpcap_regacc_write2(sply->cpcap, CPCAP_REG_CRM, 949, 949);
		cpcap_regacc_write2(sply->cpcap, CPCAP_REG_ADCD0, 186, 186);
		cpcap_regacc_write2(sply->cpcap, CPCAP_REG_ADCC2 , 16758, 16758);
		cpcap_regacc_write2(sply->cpcap, CPCAP_REG_UCTM, 0, CPCAP_BIT_UCTM);
		if (amperage < 100 && amperage > 50) {
			printk("Your charger not powerful, try reconnect or change charger, %d mA\n", amperage);
			return POWER_SUPPLY_STATUS_DISCHARGING;
		}
		if (cpcap_batt_counter(sply) > 95)
			return POWER_SUPPLY_STATUS_FULL;
		else
			return POWER_SUPPLY_STATUS_CHARGING;
        } else {
		cpcap_regacc_write2(sply->cpcap, CPCAP_REG_CRM, 944, 944);
		cpcap_regacc_write2(sply->cpcap, CPCAP_REG_ADCC2 , 310, 310);
		cpcap_regacc_write2(sply->cpcap, CPCAP_REG_ADCD0, 0, 0);
		cpcap_regacc_write2(sply->cpcap, CPCAP_REG_UCTM, 0, CPCAP_BIT_UCTM);
		return POWER_SUPPLY_STATUS_DISCHARGING;
	}
}

static int cpcap_batt_value(struct cpcap_batt_ps *sply, int value) {
	struct cpcap_adc_request req;

	req.format = CPCAP_ADC_FORMAT_CONVERTED;
	req.timing = CPCAP_ADC_TIMING_IMM;
	req.type = CPCAP_ADC_TYPE_BANK_0;
 
	cpcap_adc_sync_read(sply->cpcap, &req);

        return req.result[value];
}

static int cpcap_batt_counter(struct cpcap_batt_ps *sply) {
	int i, volt_batt;
	u32 cap = 0;
	volt_batt = cpcap_batt_value(sply, CPCAP_ADC_BATTP);
#ifdef BATTERY_DEBUG
	printk("%s: batt_vol=%d\n",__func__, volt_batt);
#endif
	for (i=0; i < ARRAY_SIZE(tbl); i++) {
		if (volt_batt <= 3500) {
			cap = 0;
			break;
		}
		if (volt_batt >= 4181) {
			cap = 100;
			break;
		}
		if (volt_batt >= tbl[i].volt_batt) {
			if (i == (ARRAY_SIZE(tbl)-1)) {
				cap = 99;
				break;
			}
			continue;
	
		}
		
		cap = tbl[i].capacity;
	break;
	}
#ifdef BATTERY_DEBUG
	printk("%s: capacity=%d\n",__func__,cap);
#endif
	return cap;
}


static void cpcap_batt_phasing(void) {
	struct cpcap_batt_ps *sply = cpcap_batt_sply;
        struct cpcap_adc_phase phase;
	/*****Battery Phasing start ****/

//CPCAP_ADC_BATTI_ADC
	phase.offset_batti = 0;
	phase.slope_batti = 128;
//CPCAP_ADC_CHG_ISENSE
	phase.offset_chrgi = 0;
	phase.slope_chrgi = 128;
//CPCAP_ADC_BATTP
	phase.offset_battp = 0;
	phase.slope_battp = 128;
//CPCAP_ADC_BPLUS_AD4
	phase.offset_bp = 0;
	phase.slope_bp = 128;
//CPCAP_ADC_AD0_BATTDETB
	phase.offset_battt = 0;
	phase.slope_battt = 128;
//CPCAP_ADC_VBUS
	phase.offset_chrgv = 128;
	phase.slope_chrgv = 128;

	cpcap_adc_phase(sply->cpcap, &phase);
	/*****Battery Phasing end ****/

//For start Macros 7 we need phasing.
	cpcap_uc_start(sply->cpcap, CPCAP_MACRO_7);
	//cpcap_uc_stop(sply->cpcap, CPCAP_MACRO_8);
	cpcap_uc_start(sply->cpcap, CPCAP_MACRO_9);
	//cpcap_uc_stop(sply->cpcap, CPCAP_MACRO_10);
	cpcap_uc_start(sply->cpcap, CPCAP_MACRO_12);

	cpcap_regacc_write2(sply->cpcap, CPCAP_REG_CCM, 1002, 1002);  // Always == 1002
	cpcap_regacc_write2(sply->cpcap, CPCAP_REG_CRM, 944, 944); // Charger off == 944, Charger on = 949
	cpcap_regacc_write2(sply->cpcap, CPCAP_REG_ADCC2, 310, 310); // Charger off == 310, Charger on = 16758
	cpcap_regacc_write2(sply->cpcap, CPCAP_REG_ADCAL1, 504, 504); // Always == 504
	cpcap_regacc_write2(sply->cpcap, CPCAP_REG_ADCAL2, 506, 506); // Always == 506
	cpcap_regacc_write2(sply->cpcap, CPCAP_REG_ADCD0, 0, 0); // Charger off == 0, Charger on = 186/187
	cpcap_regacc_write2(sply->cpcap, CPCAP_REG_UCTM, 0, CPCAP_BIT_UCTM);  /* UC Turbo Mode - Always Disabled in battd*/  
}

#ifdef BATTERY_DEBUG
static void cpcap_batt_dump(struct cpcap_batt_ps *sply) {
	int i;
	struct cpcap_adc_request req;
	struct cpcap_adc_us_request req_us;
	unsigned short value;

	cpcap_regacc_read2(sply->cpcap, CPCAP_REG_CCC1, &value);
	printk("CPCAP_REG_CCC1 %x  == %d\n",value,value);
	cpcap_regacc_read2(sply->cpcap, CPCAP_REG_CRM, &value);
	printk("CPCAP_REG_CRM %x  == %d\n",value,value);
	cpcap_regacc_read2(sply->cpcap, CPCAP_REG_CCCC2, &value);
	printk("CPCAP_REG_CCCC2 %x  == %d\n",value,value);
	cpcap_regacc_read2(sply->cpcap, CPCAP_REG_CCM, &value);
	printk("CPCAP_REG_CCM %x  == %d\n",value,value);
	cpcap_regacc_read2(sply->cpcap, CPCAP_REG_CCA1, &value);
	printk("CPCAP_REG_CCA1 %x  == %d\n",value,value);
	cpcap_regacc_read2(sply->cpcap, CPCAP_REG_CCA2, &value);
	printk("CPCAP_REG_CCA2 %x  == %d\n",value,value);
	cpcap_regacc_read2(sply->cpcap, CPCAP_REG_CCO, &value);
	printk("CPCAP_REG_CC0 %x  == %d\n",value,value);
	cpcap_regacc_read2(sply->cpcap, CPCAP_REG_CCI, &value);
	printk("CPCAP_REG_CCI %x  == %d\n",value,value);
	cpcap_regacc_read2(sply->cpcap, CPCAP_REG_USBC1, &value);
	printk("CPCAP_REG_USBC1 %x  == %d\n",value,value);
	cpcap_regacc_read2(sply->cpcap, CPCAP_REG_USBC2, &value);
	printk("CPCAP_REG_USBC2 %x  == %d\n",value,value);
	cpcap_regacc_read2(sply->cpcap, CPCAP_REG_USBC2, &value);
	printk("CPCAP_REG_USBC3 %x  == %d\n",value,value);
	cpcap_regacc_read2(sply->cpcap, CPCAP_REG_UCTM, &value);
	printk("CPCAP_REG_UCTM %x  == %d\n",value,value);
	cpcap_regacc_read2(sply->cpcap, CPCAP_REG_ADCC1, &value);
	printk("CPCAP_REG_ADCC1 %x  == %d\n",value,value);
	cpcap_regacc_read2(sply->cpcap, CPCAP_REG_ADCC2, &value);
	printk("CPCAP_REG_ADCC2 %x  == %d\n",value,value);
	cpcap_regacc_read2(sply->cpcap, CPCAP_REG_ADCD0, &value);
	printk("CPCAP_REG_ADCD0 %x  == %d\n",value,value);
	cpcap_regacc_read2(sply->cpcap, CPCAP_REG_ADCD1, &value);
	printk("CPCAP_REG_ADCD1 %x  == %d\n",value,value);
	cpcap_regacc_read2(sply->cpcap, CPCAP_REG_ADCD2, &value);
	printk("CPCAP_REG_ADCD2 %x  == %d\n",value,value);
	cpcap_regacc_read2(sply->cpcap, CPCAP_REG_ADCD3, &value);
	printk("CPCAP_REG_ADCD3 %x  == %d\n",value,value);
	cpcap_regacc_read2(sply->cpcap, CPCAP_REG_ADCD4, &value);
	printk("CPCAP_REG_ADCD4 %x  == %d\n",value,value);
	cpcap_regacc_read2(sply->cpcap, CPCAP_REG_ADCD5, &value);
	printk("CPCAP_REG_ADCD5 %x  == %d\n",value,value);
	cpcap_regacc_read2(sply->cpcap, CPCAP_REG_ADCD6, &value);
	printk("CPCAP_REG_ADCD6 %x  == %d\n",value,value);
	cpcap_regacc_read2(sply->cpcap, CPCAP_REG_ADCD7, &value);
	printk("CPCAP_REG_ADCD7 %x  == %d\n",value,value);
	cpcap_regacc_read2(sply->cpcap, CPCAP_REG_ADCAL1, &value);
	printk("CPCAP_REG_ADCAL1 %x  == %d\n",value,value);
	cpcap_regacc_read2(sply->cpcap, CPCAP_REG_ADCAL2, &value);
	printk("CPCAP_REG_ADCAL2 %x  == %d\n",value,value);

	req.format = CPCAP_ADC_FORMAT_CONVERTED;
	req.timing = CPCAP_ADC_TIMING_IMM;
	req.type = CPCAP_ADC_TYPE_BANK_0;
	cpcap_adc_sync_read(sply->cpcap, &req);

	for (i = 0; i < CPCAP_ADC_BANK0_NUM; i++)
		req_us.result[i] = req.result[i];

	printk("Dump of CPCAP_ADC_BANK0_NUM:\n CPCAP_ADC_VBUS:%d\n CPCAP_ADC_AD3:%d\n \
CPCAP_ADC_BATTP:%d\n CPCAP_ADC_BPLUS_AD4:%d\n CPCAP_ADC_CHG_ISENSE:%d\n \
CPCAP_ADC_BATTI_ADC:%d\n CPCAP_ADC_USB_ID:%d\n CPCAP_ADC_AD0_BATTDETB:%d\n",
	req_us.result[CPCAP_ADC_VBUS],req_us.result[CPCAP_ADC_AD3],req_us.result[CPCAP_ADC_BATTP],
	req_us.result[CPCAP_ADC_BPLUS_AD4],req_us.result[CPCAP_ADC_CHG_ISENSE],
	req_us.result[CPCAP_ADC_BATTI_ADC],req_us.result[CPCAP_ADC_USB_ID], req_us.result[CPCAP_ADC_AD0_BATTDETB]);
}
#endif

static void __ref cpcap_batt_update(struct work_struct *work) {
	struct cpcap_batt_ps *sply = cpcap_batt_sply;
	power_supply_changed(&sply->batt);
#ifdef BATTERY_DEBUG
	cpcap_batt_dump(sply);
#endif
	/* Make a dedicated work_queue for CPU0 */
	queue_delayed_work_on(0, wq, &update_battery, sampling_rate);
}

static int cpcap_batt_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct cpcap_batt_ps *sply;
	if (pdev->dev.platform_data == NULL) {
		dev_err(&pdev->dev, "no platform_data\n");
		ret = -EINVAL;
		goto prb_exit;
	}

	sply = kzalloc(sizeof(struct cpcap_batt_ps), GFP_KERNEL);
	if (sply == NULL) {
		ret = -ENOMEM;
		goto prb_exit;
	}

	sply->cpcap = pdev->dev.platform_data;
	mutex_init(&sply->lock);
	init_waitqueue_head(&sply->wait);
	sply->batt_state.status	= POWER_SUPPLY_STATUS_DISCHARGING; //Set discharging by default.  //POWER_SUPPLY_STATUS_UNKNOWN;
	sply->batt_state.health	= POWER_SUPPLY_HEALTH_GOOD;
	sply->batt_state.present = 1;
	sply->batt_state.capacity = 100;	/* Percentage */
	sply->batt_state.batt_volt = 4200000;	/* uV */
	sply->batt_state.batt_temp = 230;	/* tenths of degrees Celsius */
	sply->batt_state.batt_full_capacity = 0;
	sply->batt_state.batt_capacity_one = 100;

	sply->ac_state.online = 0;

	sply->usb_state.online = 0;
	sply->usb_state.current_now = 0;
	sply->usb_state.model = CPCAP_BATT_USB_MODEL_NONE;

	sply->batt.properties = cpcap_batt_props;
	sply->batt.num_properties = ARRAY_SIZE(cpcap_batt_props);
	sply->batt.get_property = cpcap_batt_get_property;
	sply->batt.name = "battery";
	sply->batt.type = POWER_SUPPLY_TYPE_BATTERY;

	sply->ac.properties = cpcap_batt_ac_props;
	sply->ac.num_properties = ARRAY_SIZE(cpcap_batt_ac_props);
	sply->ac.get_property = cpcap_batt_ac_get_property;
	sply->ac.name = "ac";
	sply->ac.type = POWER_SUPPLY_TYPE_MAINS;

	sply->usb.properties = cpcap_batt_usb_props;
	sply->usb.num_properties = ARRAY_SIZE(cpcap_batt_usb_props);
	sply->usb.get_property = cpcap_batt_usb_get_property;
	sply->usb.name = "usb";
	sply->usb.type = POWER_SUPPLY_TYPE_USB;

	sply->no_update = false;

	ret = power_supply_register(&pdev->dev, &sply->ac);
	if (ret)
		goto prb_exit;
	ret = power_supply_register(&pdev->dev, &sply->batt);
	if (ret)
		goto unregac_exit;
	ret = power_supply_register(&pdev->dev, &sply->usb);
	if (ret)
		goto unregbatt_exit;
	platform_set_drvdata(pdev, sply);
	sply->cpcap->battdata = sply;
	cpcap_batt_sply = sply;

	ret = cpcap_irq_register(sply->cpcap, CPCAP_IRQ_VBUSOV,
				 cpcap_batt_irq_hdlr, sply);
	if (ret)
		goto unregirq_exit;
	ret = cpcap_irq_register(sply->cpcap, CPCAP_IRQ_BATTDETB,
				 cpcap_batt_irq_hdlr, sply);
	if (ret)
		goto unregirq_exit;
	ret = cpcap_irq_register(sply->cpcap, CPCAP_IRQ_CC_CAL,
				 cpcap_batt_irq_hdlr, sply);
	if (ret)
		goto unregirq_exit;

	ret = cpcap_irq_register(sply->cpcap, CPCAP_IRQ_UC_PRIMACRO_7,
				 cpcap_batt_irq_hdlr, sply);
	cpcap_irq_mask(sply->cpcap, CPCAP_IRQ_UC_PRIMACRO_7);

	if (ret)
		goto unregirq_exit;

	ret = cpcap_irq_register(sply->cpcap, CPCAP_IRQ_UC_PRIMACRO_8,
				 cpcap_batt_irq_hdlr, sply);
	cpcap_irq_mask(sply->cpcap, CPCAP_IRQ_UC_PRIMACRO_8);

	if (ret)
		goto unregirq_exit;

	ret = cpcap_irq_register(sply->cpcap, CPCAP_IRQ_UC_PRIMACRO_9,
				 cpcap_batt_irq_hdlr, sply);
	cpcap_irq_mask(sply->cpcap, CPCAP_IRQ_UC_PRIMACRO_9);

	if (ret)
		goto unregirq_exit;

	ret = cpcap_irq_register(sply->cpcap, CPCAP_IRQ_UC_PRIMACRO_10,
				 cpcap_batt_irq_hdlr, sply);
	cpcap_irq_mask(sply->cpcap, CPCAP_IRQ_UC_PRIMACRO_10);

	if (ret)
		goto unregirq_exit;

	ret = cpcap_irq_register(sply->cpcap, CPCAP_IRQ_UC_PRIMACRO_11,
				 cpcap_batt_irq_hdlr, sply);
	cpcap_irq_mask(sply->cpcap, CPCAP_IRQ_UC_PRIMACRO_11);


	if (ret)
		goto unregirq_exit;

	goto prb_exit;

unregirq_exit:
	cpcap_irq_free(sply->cpcap, CPCAP_IRQ_VBUSOV);
	cpcap_irq_free(sply->cpcap, CPCAP_IRQ_BATTDETB);
	cpcap_irq_free(sply->cpcap, CPCAP_IRQ_CC_CAL);
	cpcap_irq_free(sply->cpcap, CPCAP_IRQ_UC_PRIMACRO_7);
	cpcap_irq_free(sply->cpcap, CPCAP_IRQ_UC_PRIMACRO_8);
	cpcap_irq_free(sply->cpcap, CPCAP_IRQ_UC_PRIMACRO_9);
	cpcap_irq_free(sply->cpcap, CPCAP_IRQ_UC_PRIMACRO_10);
	cpcap_irq_free(sply->cpcap, CPCAP_IRQ_UC_PRIMACRO_11);
unregbatt_exit:
	power_supply_unregister(&sply->batt);
unregac_exit:
	power_supply_unregister(&sply->ac);

prb_exit:
	cpcap_batt_phasing();
	sampling_rate = msecs_to_jiffies(SAMPLING_RATE_MS);
	wq = create_singlethread_workqueue("battery_update_workqueue");

	if (!wq)
		return -ENOMEM;

	INIT_DELAYED_WORK(&update_battery, cpcap_batt_update);
	queue_delayed_work_on(0, wq, &update_battery, sampling_rate);
	cpcap_batt_set_usb_prop_curr(cpcap_batt_sply->cpcap, 100);
	cpcap_batt_set_usb_prop_online(cpcap_batt_sply->cpcap, 1, CPCAP_BATT_USB_MODEL_USB);
	return ret;
}

static int cpcap_batt_remove(struct platform_device *pdev)
{
	struct cpcap_batt_ps *sply = platform_get_drvdata(pdev);

	power_supply_unregister(&sply->batt);
	power_supply_unregister(&sply->ac);
	power_supply_unregister(&sply->usb);
	cpcap_irq_free(sply->cpcap, CPCAP_IRQ_VBUSOV);
	cpcap_irq_free(sply->cpcap, CPCAP_IRQ_BATTDETB);
	cpcap_irq_free(sply->cpcap, CPCAP_IRQ_CC_CAL);
	cpcap_irq_free(sply->cpcap, CPCAP_IRQ_UC_PRIMACRO_7);
	cpcap_irq_free(sply->cpcap, CPCAP_IRQ_UC_PRIMACRO_8);
	cpcap_irq_free(sply->cpcap, CPCAP_IRQ_UC_PRIMACRO_9);
	cpcap_irq_free(sply->cpcap, CPCAP_IRQ_UC_PRIMACRO_10);
	cpcap_irq_free(sply->cpcap, CPCAP_IRQ_UC_PRIMACRO_11);
	sply->cpcap->battdata = NULL;
	kfree(sply);

	return 0;
}

static int cpcap_batt_resume(struct platform_device *pdev)
{
	struct cpcap_batt_ps *sply = platform_get_drvdata(pdev);
	unsigned long cur_time;
	unsigned long long temp;
	SYMSEARCH_BIND_FUNCTION_TO(cpcap-battery, sched_clock, sched_clock2);
	temp = sched_clock2();
	do_div(temp, NSEC_PER_SEC);
	cur_time = ((unsigned long)temp);
	if ((cur_time - sply->last_run_time) < 0)
		sply->last_run_time = 0;

	if ((cur_time - sply->last_run_time) > 50) {
		mutex_lock(&sply->lock);
		sply->data_pending = 1;
		sply->irq_status |= CPCAP_BATT_IRQ_MACRO;

		mutex_unlock(&sply->lock);

		wake_up_interruptible(&sply->wait);
	}

	return 0;
}

static int __init cpcap_battery_unload_init(void)
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

    cpcap_batt_init();
    return 0;
}


static int cpcap_batt_init(void)
{
	return platform_driver_register(&cpcap_batt_driver);
}
subsys_initcall(cpcap_battery_unload_init);

static void __exit cpcap_batt_exit(void)
{
	platform_driver_unregister(&cpcap_batt_driver);
}
module_exit(cpcap_batt_exit);

MODULE_ALIAS("platform:cpcap_batt");
MODULE_DESCRIPTION("CPCAP BATTERY driver");
MODULE_AUTHOR("Quarx, Blechdose");
MODULE_LICENSE("GPL");
