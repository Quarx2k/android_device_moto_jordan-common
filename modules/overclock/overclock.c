/*
	Motorola OMAP3 overclock module
	version 1.4.9 - 2011-03-20
	by Tiago Sousa <mirage@kaotik.org>
	License: GNU GPLv2
	<http://www.gnu.org/licenses/old-licenses/gpl-2.0.html>

	Project site:
	http://code.google.com/p/milestone-overclock/

	Changelog:

	version 1.4.9 - 2012-05-31 (CyanogenDefy)
	- printk level tuneup, add Defy in Description
	  restore original max rate/vsel on module unload
	version 1.4.8 - 2011-03-20
	- build process for motorola milestone 2.2
	version 1.4.7 - 2011-01-16
	- added sr_adjust_vsel field when reading the mpu_opps table if
	  SMARTREFLEX is enabled (thanks tekahuna)
	version 1.4.6.1 - 2010-12-17
	- rolled back the address detection, needs more testing
	version 1.4.6 - 2010-12-16
	- automatic detection of omap2_clk_init_cpufreq_table_addr and
	  cpufreq_stats_update_addr in most 2.6.32 kernels (by Skrilax_CZ and
	  kabaldan)
	- build process for motorola flipout, samsung galaxy a and archos
	  tablets a70/a101
	version 1.4.5 - 2010-12-09
	- build process for motorola defy
	version 1.4.4 - 2010-10-28
	- build process for milestone leaked 2.6.32 and droid 2.2
	version 1.4.3 - 2010-10-25
	- build process for droid x 2.2
	- fix vsel setting for droid x 2.x and others (thanks to kabaldan
	  for hinting at this; the struct omap_opp changed to support "smart
	  reflex class 1.5" technology)
	- remove call to omap_pm_cpu_get_freq() because it's no longer
	  exported in froyo (thanks tekahuna)
	version 1.4.2 - 2010-08-31
	- build process for samsung galaxy beam
	version 1.4.1 - 2010-08-30
	- change cpufreq stats when writing to /proc/overclock/freq_table
	version 1.4 - 2010-08-16
	- support kernel 2.6.32 (android froyo)
	- improve the build process to compile against multiple kernels
	- stop using default addresses for mpu_opps_addr and
	  cpufreq_stats_table_addr to avoid confusion
	- get the address of freq_table automatically with
	  cpufreq_frequency_get_table()
	- fix assignments when writing to
	  /proc/overclock/omap2_clk_init_cpufreq_table_addr (thanks kabaldan)
	- fix cpufreq stats (patch by kabaldan)
	version 1.2 - 2010-05-25
	- change the values of freq_table by writing "index frequency" to
	  /proc/overclock/freq_table such as:
	  echo "1 400000" > /proc/overclock/freq_table
	- change the values of mpu_opps by writing "index rate vsel" to
	  /proc/overclock/mpu_opps such as:
	  echo "3 400000000 50" > /proc/overclock/mpu_opps
	- autodetect freq_table and mpu_opps given
	  omap2_clk_init_cpufreq_table's address at load time or:
	  echo 0xc004e498 > /proc/overclock/omap2_clk_init_cpufreq_table_addr
	  you can find the address with:
	  grep omap2_clk_init_cpufreq_table /proc/kallsyms
	version 1.1 - 2010-05-20
	- mpu_opps now configurable in load time
	- improved mpu_opps address lookup
	- corrected vsel units
	- applied mattih's patch to set policy->user_policy.max
	version 1.0 - 2010-05-14
	- initial release

	Description:

	The MPU (Microprocessor Unit) clock has 5 discrete pairs of possible
	rate frequencies and respective voltages, of which only 4 are passed
	down to cpufreq as you can see with a tool such as SetCPU. The
	default frequencies are 125, 250, 500 and 550 MHz (and a hidden
	600). By using this module, you are changing the highest pair in the
	tables of both cpufreq and MPU frequencies, so it becomes 125, 250,
	500 and, say, 800. It's quite stable up to 1200; beyond that it
	quickly becomes unusable, specially over 1300, with lockups or
	spontaneous reboots.

	This version was prepared for Motorola Milestone's official Android
	2.1 for Central Europe, build number SHOLS_U2_02.31.0. Fortunately
	Motorola appears to have used the same kernel in most of the 2.1
	firmwares and even 2.0 works. All that's needed is to specify
	autodetect addresses when loading the module. See KernelModule wiki
	page at the project site for more information. To port the module to
	different kernels or even other phones, see the Disassembly wiki
	page. Testers welcome!

	Usage:

	insmod overclock.ko
	busybox egrep "omap2_clk_init_cpufreq_table$" /proc/kallsyms
	echo 0xc004e4b0 > /proc/overclock/omap2_clk_init_cpufreq_table_addr
	busybox egrep "cpufreq_stats_update$" /proc/kallsyms
	echo 0xc0295704 > /proc/overclock/cpufreq_stats_update_addr
	echo 62 > /proc/overclock/max_vsel
	echo 800000 > /proc/overclock/max_rate

	You should set max_vsel before max_rate if the new rate is going to
	be higher than the current one, because higher frequencies often
	require more voltage than supplied by default. Likewise, lower
	max_rate first before max_vsel if you want to reduce both frequency
	and voltage:

	echo 550000 > /proc/overclock/max_rate
	echo 56 > /proc/overclock/max_vsel

	To set a specified frequency and voltage at load time (don't forget
	to change the addr parameters according to your kernel):

	insmod overclock.ko
	echo 0xc004e4b0 > /proc/overclock/omap2_clk_init_cpufreq_table_addr
	echo 0xc0295704 > /proc/overclock/cpufreq_stats_update_addr
	echo 62 > /proc/overclock/max_vsel
	echo 800000 > /proc/overclock/max_rate

	It has been reported that the vsel is reset to default while rate
	stays overclocked. This is caused by the default /system/bin/insmod
	takes only one argument, which can consist of "nested" arguments,
	ie, "arg1=1 arg2=2". Busybox's insmod, typically in
	/system/xbin/insmod, takes normal arguments, ie, "arg1=1" "arg2=2".
	This difference caused reports of the mpu_opps table not being
	initialized, because its address was usually the second parameter
	and it would be ignored when specified in the traditional insmod,
	although it would work when written to /proc. Therefore it's
	preferable to load the module without parameters and write the
	addresses using /proc to maximize compatibility with different
	insmod implementations.

	Be careful when you try to load this in mot_boot_mode. It's
	preferable to use the MilestoneOverclock app to do it, because it
	only works if the sdcard is mounted, which is easy to override in
	case of a bad overclock.

	Remember that you are merely changing the maximum possible value
	that cpufreq can choose to use. The current speed may well be lower
	than the one specified if the phone is idle. I recommend the use of
	the SetCPU app to effectively change the current frequency through
	its policies (use Autodetect Speeds instead of Droid/Milestone
	profile in the device settings).
*/

#ifndef SMARTREFLEX
#warning SMARTREFLEX cflag is not set
#endif

#include <linux/module.h>
#include <linux/init.h>
#include <linux/version.h>
#include <linux/proc_fs.h>
#include <linux/vmalloc.h>
#include <asm/uaccess.h>
#include <linux/interrupt.h>
#include <linux/clk.h>
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,32)
#include <plat/omap-pm.h>
#else
#include <mach/omap-pm.h>
#endif

#define DRIVER_AUTHOR "Tiago Sousa <mirage@kaotik.org>"
#define DRIVER_DESCRIPTION "Motorola Milestone/Defy/Droid/DroidX CPU overclocking"
#define DRIVER_VERSION "1.4.9"

#ifdef OMAP36XX
#define DRIVER_DEFAULT_RATE 1000000
#define DRIVER_DEFAULT_VSEL 66
#else
#define DRIVER_DEFAULT_RATE 550000
#define DRIVER_DEFAULT_VSEL 56
#endif

MODULE_AUTHOR(DRIVER_AUTHOR);
MODULE_AUTHOR("CyanogenDefy");
#ifdef SMARTREFLEX
MODULE_DESCRIPTION("With SmartReflex support");
#endif
MODULE_DESCRIPTION(DRIVER_DESCRIPTION);
MODULE_VERSION(DRIVER_VERSION);
MODULE_LICENSE("GPL");

static uint max_rate = DRIVER_DEFAULT_RATE;
static uint max_vsel = DRIVER_DEFAULT_VSEL;
static uint bak_rate = 0;
static uint bak_vsel = 0;
static uint mpu_opps_addr = 0;
static uint cpufreq_stats_table_addr = 0;
static uint omap2_clk_init_cpufreq_table_addr = 0;
static uint cpufreq_stats_update_addr = 0;

struct proc_dir_entry *proc_root = NULL;

module_param(max_rate, uint, 0444);
module_param(max_vsel, uint, 0444);
#ifdef OMAP36XX
MODULE_PARM_DESC(max_rate, "The maximum MPU clock rate frequency in KHz (default 1000000)");
MODULE_PARM_DESC(max_vsel, "The maximum MPU voltage selection (default 66)");
#else
MODULE_PARM_DESC(max_rate, "The maximum MPU clock rate frequency in KHz (default 550000)");
MODULE_PARM_DESC(max_vsel, "The maximum MPU voltage selection (default 56)");
#endif
module_param(mpu_opps_addr, uint, 0444);
MODULE_PARM_DESC(mpu_opps_addr, "The mpu_opps address (default 0, no overclock)");
module_param(cpufreq_stats_table_addr, uint, 0444);
MODULE_PARM_DESC(cpufreq_stats_table_addr, "The cpufreq_stats_table address (default 0, no cpufreq stats correction)");
module_param(omap2_clk_init_cpufreq_table_addr, uint, 0444);
MODULE_PARM_DESC(omap2_clk_init_cpufreq_table_addr, "The omap2_clk_init_cpufreq_table function address to autodetect the other addresses (default 0, doesn't autodetect)");
module_param(cpufreq_stats_update_addr, uint, 0444);
MODULE_PARM_DESC(cpufreq_stats_update_addr, "The cpufreq_stats_update function address to autodetect the stats address (default 0, doesn't autodetect)");

/* from drivers/cpufreq/cpufreq_stats.c */
struct cpufreq_stats {
	unsigned int cpu;
	unsigned int total_trans;
	unsigned long long  last_time;
	unsigned int max_state;
	unsigned int state_num;
	unsigned int last_index;
	cputime64_t *time_in_state;
	unsigned int *freq_table;
};

static struct cpufreq_frequency_table *freq_table;
static struct omap_opp *my_mpu_opps;
static struct cpufreq_stats *cpufreq_stats_table;
static struct cpufreq_policy *policy;
#define MPU_CLK         "arm_fck"
static struct clk *mpu_clk;

#define BUF_SIZE PAGE_SIZE
static char *buf = NULL;

#ifdef LOOKUP
extern unsigned long lookup_symbol_address(const char *name);
#endif

static void error_mpu_opps(void)
{
	pr_err("overclock: mpu_opps address not configured, aborting action\n");
}

static void error_cpufreq_stats_table(void)
{
	pr_err("overclock: cpufreq_stats_table address not configured, aborting action\n");
}

static void set_max_speed(void)
{
	pr_info("overclock: setting max_rate %u and max_vsel %u\n",
		max_rate, max_vsel);
	if(!my_mpu_opps) {
		error_mpu_opps();
		return;
	}

	if (!bak_vsel) {
		/* save original config for module unload */
		bak_vsel = my_mpu_opps[MAX_VDD1_OPP].vsel;
		bak_rate = freq_table[0].frequency;
	}

	freq_table[0].frequency = policy->max = policy->cpuinfo.max_freq =
		policy->user_policy.max = max_rate;
	if(cpufreq_stats_table)	/* can overclock without it */
		cpufreq_stats_table->freq_table[0] = max_rate;
	else
		error_cpufreq_stats_table();
	my_mpu_opps[MAX_VDD1_OPP].vsel = max_vsel;
#ifdef SMARTREFLEX
	my_mpu_opps[MAX_VDD1_OPP].sr_adjust_vsel = max_vsel;
#endif
	my_mpu_opps[MAX_VDD1_OPP].rate = max_rate*1000;
}

static void omap2_find_addr(void)
{
	unsigned char *func = (void *)omap2_clk_init_cpufreq_table_addr;
	uint *addr;
	int i;
	for(i = 0; i < 100; i+=4) {
		if((func[i+3] == 0xe5) /* ldr */
			&& func[i+2] == 0x9f) { /* [pc, */
			addr = (void *)((uint)func)+i+8+func[i];
			mpu_opps_addr = *addr;
			pr_info("overclock: found mpu_opps_addr at 0x%x\n",
				mpu_opps_addr);
			break;
		}
	}
}

static void stats_find_addr(void)
{
	unsigned char *func = (void *)cpufreq_stats_update_addr;
	uint *addr;
	int i;
	for(i = 0; i < 100; i+=4) {
		if((func[i+3] == 0xe5) /* ldr */
			&& func[i+2] == 0x9f) { /* [pc, */
			addr = (void *)((uint)func)+i+8+func[i];
			cpufreq_stats_table_addr = *addr;
			pr_info("overclock: found cpufreq_stats_table_addr at 0x%x\n",
				cpufreq_stats_table_addr);
			break;
		}
	}
}

static int proc_info_read(char *buffer, char **buffer_location,
		off_t offset, int count, int *eof, void *data)
{
	int ret;

	if (offset > 0)
		ret = 0;
	else
		ret = scnprintf(buffer, count, "cpumin=%u cpumax=%u min=%u max=%u usermin=%u usermax=%u\nclk_get_rate=%lu\n",
				policy->cpuinfo.min_freq, policy->cpuinfo.max_freq, policy->min, policy->max, policy->user_policy.min, policy->user_policy.max, clk_get_rate(mpu_clk) / 1000);

	return ret;
}

static int proc_max_rate_read(char *buffer, char **buffer_location,
		off_t offset, int count, int *eof, void *data)
{
	int ret;

	if (offset > 0)
		ret = 0;
	else
		ret = scnprintf(buffer, count, "%u\n", max_rate);

	return ret;
}

static int proc_max_rate_write(struct file *filp, const char __user *buffer,
		unsigned long len, void *data)
{
	ulong newrate;
	int result;

	if(!len || len >= BUF_SIZE)
		return -ENOSPC;
	if(copy_from_user(buf, buffer, len))
		return -EFAULT;
	buf[len] = 0;
	if((result = strict_strtoul(buf, 0, &newrate)))
		return result;
	if(max_rate != newrate) {
		max_rate = newrate;
		set_max_speed();
	}

	return len;
}

static int proc_max_vsel_read(char *buffer, char **buffer_location,
		off_t offset, int count, int *eof, void *data)
{
	int ret;

	if (offset > 0)
		ret = 0;
	else
		ret = scnprintf(buffer, count, "%u\n", max_vsel);

	return ret;
}

static int proc_max_vsel_write(struct file *filp, const char __user *buffer,
		unsigned long len, void *data)
{
	ulong newvsel;
	int result;

	if(!len || len >= BUF_SIZE)
		return -ENOSPC;
	if(copy_from_user(buf, buffer, len))
		return -EFAULT;
	buf[len] = 0;
	if((result = strict_strtoul(buf, 0, &newvsel)))
		return result;
	if(max_vsel != newvsel) {
		max_vsel = newvsel;
		set_max_speed();
	}

	return len;
}

static int proc_mpu_opps_addr_read(char *buffer, char **buffer_location,
		off_t offset, int count, int *eof, void *data)
{
	int ret;

	if (offset > 0)
		ret = 0;
	else
		ret = scnprintf(buffer, count, "0x%x\n", (uint)mpu_opps_addr);

	return ret;
}

static int proc_mpu_opps_addr_write(struct file *filp, const char __user *buffer,
		unsigned long len, void *data)
{
	ulong newaddr;
	int result;

	if(!len || len >= BUF_SIZE)
		return -ENOSPC;
	if(copy_from_user(buf, buffer, len))
		return -EFAULT;
	buf[len] = 0;
	if((result = strict_strtoul(buf, 0, &newaddr)))
		return result;
	mpu_opps_addr = newaddr;
	if(mpu_opps_addr)
		my_mpu_opps = *(struct omap_opp **)mpu_opps_addr;
	else
		error_mpu_opps();

	return len;
}

static int proc_omap2_clk_init_cpufreq_table_addr_read(char *buffer,
		char **buffer_location, off_t offset, int count, int *eof,
		void *data)
{
	int ret;

	if (offset > 0)
		ret = 0;
	else
		ret = scnprintf(buffer, count, "0x%x\n",
			(uint)omap2_clk_init_cpufreq_table_addr);

	return ret;
}

static int proc_omap2_clk_init_cpufreq_table_addr_write(struct file *filp,
		const char __user *buffer, unsigned long len, void *data)
{
	ulong newaddr;
	int result;

	if(!len || len >= BUF_SIZE)
		return -ENOSPC;
	if(copy_from_user(buf, buffer, len))
		return -EFAULT;
	buf[len] = 0;
	if((result = strict_strtoul(buf, 0, &newaddr)))
		return result;
	omap2_clk_init_cpufreq_table_addr = newaddr;
	if(omap2_clk_init_cpufreq_table_addr)
		omap2_find_addr();
	if(mpu_opps_addr)
		my_mpu_opps = *(struct omap_opp **)mpu_opps_addr;
	else
		error_mpu_opps();

	return len;
}

static int proc_cpufreq_stats_table_addr_read(char *buffer, char **buffer_location,
		off_t offset, int count, int *eof, void *data)
{
	int ret;

	if (offset > 0)
		ret = 0;
	else
		ret = scnprintf(buffer, count, "0x%x\n", (uint)cpufreq_stats_table_addr);

	return ret;
}

static int proc_cpufreq_stats_table_addr_write(struct file *filp,
		const char __user *buffer,
		unsigned long len, void *data)
{
	ulong newaddr;
	int result;

	if(!len || len >= BUF_SIZE)
		return -ENOSPC;
	if(copy_from_user(buf, buffer, len))
		return -EFAULT;
	buf[len] = 0;
	if((result = strict_strtoul(buf, 0, &newaddr)))
		return result;
	cpufreq_stats_table_addr = newaddr;
	if(cpufreq_stats_table_addr)
		cpufreq_stats_table = *(struct cpufreq_stats **)cpufreq_stats_table_addr;
	else
		error_cpufreq_stats_table();

	return len;
}

static int proc_cpufreq_stats_update_addr_read(char *buffer,
		char **buffer_location, off_t offset, int count, int *eof,
		void *data)
{
	int ret;

	if (offset > 0)
		ret = 0;
	else
		ret = scnprintf(buffer, count, "0x%x\n",
			(uint)cpufreq_stats_update_addr);

	return ret;
}

static int proc_cpufreq_stats_update_addr_write(struct file *filp,
		const char __user *buffer, unsigned long len, void *data)
{
	ulong newaddr;
	int result;

	if(!len || len >= BUF_SIZE)
		return -ENOSPC;
	if(copy_from_user(buf, buffer, len))
		return -EFAULT;
	buf[len] = 0;
	if((result = strict_strtoul(buf, 0, &newaddr)))
		return result;
	cpufreq_stats_update_addr = newaddr;
	if(cpufreq_stats_update_addr)
		stats_find_addr();
	if(cpufreq_stats_table_addr)
		cpufreq_stats_table = *(struct cpufreq_stats **)cpufreq_stats_table_addr;
	else
		error_cpufreq_stats_table();

	return len;
}

static int proc_freq_table_read(char *buffer, char **buffer_location,
		off_t offset, int count, int *eof, void *data)
{
	int i, ret = 0;

	if (offset > 0)
		ret = 0;
	else
		for(i = 0; freq_table[i].frequency != CPUFREQ_TABLE_END; i++) {
			if(ret >= count)
				break;
			ret += scnprintf(buffer+ret, count-ret, "freq_table[%d] index=%u frequency=%u\n", i, freq_table[i].index, freq_table[i].frequency);
		}

	return ret;
}

static int proc_freq_table_write(struct file *filp, const char __user *buffer,
		unsigned long len, void *data)
{
	uint index, frequency;

	if(!len || len >= BUF_SIZE)
		return -ENOSPC;
	if(copy_from_user(buf, buffer, len))
		return -EFAULT;
	buf[len] = 0;
	if(sscanf(buf, "%d %d", &index, &frequency) == 2) {
		freq_table[index].frequency = frequency;
		if(cpufreq_stats_table)
			cpufreq_stats_table->freq_table[index] = frequency;
		else
			error_cpufreq_stats_table();

	} else
		pr_warning("overclock: insufficient parameters for freq_table\n");

	return len;
}

static int proc_mpu_opps_read(char *buffer, char **buffer_location,
		off_t offset, int count, int *eof, void *data)
{
	int i, ret = 0;

	if(!my_mpu_opps) {
		error_mpu_opps();
		return 0;
	}

	if (offset > 0)
		ret = 0;
	else
		for(i = MAX_VDD1_OPP; my_mpu_opps[i].rate; i--) {
			if(ret >= count)
				break;
#ifdef SMARTREFLEX
			ret += scnprintf(buffer+ret, count-ret, "mpu_opps[%d] rate=%lu opp_id=%u vsel=%u sr_adjust_vsel=%u\n",
				i, my_mpu_opps[i].rate,
				my_mpu_opps[i].opp_id,
				my_mpu_opps[i].vsel,
				my_mpu_opps[i].sr_adjust_vsel);
#else
			ret += scnprintf(buffer+ret, count-ret, "mpu_opps[%d] rate=%lu opp_id=%u vsel=%u\n",
				i, my_mpu_opps[i].rate,
				my_mpu_opps[i].opp_id,
				my_mpu_opps[i].vsel);
#endif
		}

	return ret;
}

static int proc_mpu_opps_write(struct file *filp, const char __user *buffer,
		unsigned long len, void *data)
{
	uint index, rate, vsel;

	if(!len || len >= BUF_SIZE)
		return -ENOSPC;
	if(copy_from_user(buf, buffer, len))
		return -EFAULT;
	buf[len] = 0;
	if(sscanf(buf, "%d %d %d", &index, &rate, &vsel) == 3) {
		my_mpu_opps[index].rate = rate;
		my_mpu_opps[index].vsel = vsel;
#ifdef SMARTREFLEX
		my_mpu_opps[index].sr_adjust_vsel = vsel;
#endif
	} else
		pr_warning("overclock: insufficient parameters for mpu_opps\n");

	return len;
}

static int proc_version_read(char *buffer, char **buffer_location,
		off_t offset, int count, int *eof, void *data)
{
	int ret;

	if (offset > 0)
		ret = 0;
	else
		ret = scnprintf(buffer, count, "%s\n", DRIVER_VERSION);

	return ret;
}

static int __init overclock_init(void)
{
	struct proc_dir_entry *proc_entry;

	pr_info("overclock: %s version %s\n", DRIVER_DESCRIPTION, DRIVER_VERSION);

	buf = (char *)vmalloc(BUF_SIZE);
	if (!buf) {
		return -ENOMEM;
	}

#ifdef LOOKUP
	if(!omap2_clk_init_cpufreq_table_addr)
		omap2_clk_init_cpufreq_table_addr = lookup_symbol_address("omap2_clk_init_cpufreq_table");
	if(!cpufreq_stats_update_addr)
		cpufreq_stats_update_addr = lookup_symbol_address("cpufreq_stats_update");
#endif

	if(omap2_clk_init_cpufreq_table_addr)
		omap2_find_addr();
	if(cpufreq_stats_update_addr)
		stats_find_addr();

	freq_table = cpufreq_frequency_get_table(0);
	policy = cpufreq_cpu_get(0);
	mpu_clk = clk_get(NULL, MPU_CLK);
	if(mpu_opps_addr)
		my_mpu_opps = *(struct omap_opp **)mpu_opps_addr;
	else
		my_mpu_opps = 0;
	if(cpufreq_stats_table_addr)
		cpufreq_stats_table = *(struct cpufreq_stats **)cpufreq_stats_table_addr;
	else
		cpufreq_stats_table = 0;

	if(max_rate != DRIVER_DEFAULT_RATE || max_vsel != DRIVER_DEFAULT_VSEL)
		set_max_speed();

	proc_root = proc_mkdir("overclock", NULL);
	proc_entry = create_proc_read_entry("overclock/info", 0444, NULL, proc_info_read, NULL);
	proc_entry = create_proc_read_entry("overclock/max_rate", 0644, NULL, proc_max_rate_read, NULL);
	proc_entry->write_proc = proc_max_rate_write;
	proc_entry = create_proc_read_entry("overclock/max_vsel", 0644, NULL, proc_max_vsel_read, NULL);
	proc_entry->write_proc = proc_max_vsel_write;
	proc_entry = create_proc_read_entry("overclock/mpu_opps_addr", 0644, NULL, proc_mpu_opps_addr_read, NULL);
	proc_entry->write_proc = proc_mpu_opps_addr_write;
	proc_entry = create_proc_read_entry("overclock/omap2_clk_init_cpufreq_table_addr", 0644, NULL, proc_omap2_clk_init_cpufreq_table_addr_read, NULL);
	proc_entry->write_proc = proc_omap2_clk_init_cpufreq_table_addr_write;
	proc_entry = create_proc_read_entry("overclock/cpufreq_stats_table_addr", 0644, NULL, proc_cpufreq_stats_table_addr_read, NULL);
	proc_entry->write_proc = proc_cpufreq_stats_table_addr_write;
	proc_entry = create_proc_read_entry("overclock/cpufreq_stats_update_addr", 0644, NULL, proc_cpufreq_stats_update_addr_read, NULL);
	proc_entry->write_proc = proc_cpufreq_stats_update_addr_write;
	proc_entry = create_proc_read_entry("overclock/freq_table", 0644, NULL, proc_freq_table_read, NULL);
	proc_entry->write_proc = proc_freq_table_write;
	proc_entry = create_proc_read_entry("overclock/mpu_opps", 0644, NULL, proc_mpu_opps_read, NULL);
	proc_entry->write_proc = proc_mpu_opps_write;
	proc_entry = create_proc_read_entry("overclock/version", 0444, NULL, proc_version_read, NULL);

	return 0;
}

static void __exit overclock_exit(void)
{
	remove_proc_entry("overclock/version", NULL);
	remove_proc_entry("overclock/mpu_opps", NULL);
	remove_proc_entry("overclock/freq_table", NULL);
	remove_proc_entry("overclock/cpufreq_stats_update_addr", NULL);
	remove_proc_entry("overclock/cpufreq_stats_table_addr", NULL);
	remove_proc_entry("overclock/omap2_clk_init_cpufreq_table_addr", NULL);
	remove_proc_entry("overclock/mpu_opps_addr", NULL);
	remove_proc_entry("overclock/max_vsel", NULL);
	remove_proc_entry("overclock/max_rate", NULL);
	remove_proc_entry("overclock/info", NULL);
	remove_proc_entry("overclock", NULL);

	if (buf) vfree(buf);

	if(max_rate != DRIVER_DEFAULT_RATE || max_vsel != DRIVER_DEFAULT_VSEL) {
		max_rate = DRIVER_DEFAULT_RATE;
		max_vsel = DRIVER_DEFAULT_VSEL;
		if (bak_vsel && bak_rate) {
			max_vsel = bak_vsel;
			max_rate = bak_rate;
		}
		set_max_speed();
	}

	pr_info("overclock: overclocking disabled and module unloaded\n");
}

module_init(overclock_init);
module_exit(overclock_exit);

