#include <asm/uaccess.h>
#include <asm/cacheflush.h>
#include <asm/tlbflush.h>
#include <asm/io.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/sched.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/device.h>
#include <linux/suspend.h>
#include <asm/bootinfo.h>
#include <plat/panel.h>

#include "hboot.h"
#include "emu_uart.h"
#include "cache.h"

#define CTRL_DEVNAME "hbootctrl"

static int dev_major;

/* NOTE: 0x00100000 is mapped per sector, keep that in mind */

#define MMU_48000000					0xFA000000
#define MMU_49000000					0xFB000000

#define MMU_SDMA_BASE					0xFA056000

#define MMU_UART3_BASE				0xFB020000

#define MMU_CM_FCLKEN_PER			0xFA005000
#define MMU_CM_ICLKEN_PER			0xFA005010

/* Clock is 48 MHz, divided by 16 */
#define UART_CLOCK_BASE_RATE	2995200

/* We have MMU enabled, so pick the virt. address */
void reconfigure_emu_uart(uint32_t uart_speed)
{
	uint32_t lcr;
	uint32_t efr;
	uint32_t mdr1;
	uint32_t ier;
	uint32_t dlh, dll;
	uint16_t div;

	printk(KERN_INFO CTRL_DEVNAME ": Enabling clocks for UART3\n");
	__raw_writel(__raw_readl(MMU_CM_FCLKEN_PER) | 0x0800, MMU_CM_FCLKEN_PER);
	__raw_writel(__raw_readl(MMU_CM_ICLKEN_PER) | 0x0800, MMU_CM_ICLKEN_PER);

	printk(KERN_INFO CTRL_DEVNAME ": Configuring UART3 on 0x%08X\n", MMU_UART3_BASE);

	__raw_writel(__raw_readl(MMU_UART3_BASE + 0x54) | 0x02, MMU_UART3_BASE + 0x54);  //reset uart
	while (!(__raw_readl(MMU_UART3_BASE + 0x58) & 1));

	mdr1 = __raw_readl(MMU_UART3_BASE + 0x20);
	__raw_writel(mdr1 | 0x07, MMU_UART3_BASE + 0x20);

	lcr = __raw_readl(MMU_UART3_BASE + 0x0C);
	__raw_writel(0xBF, MMU_UART3_BASE + 0x0C);

	efr = __raw_readl(MMU_UART3_BASE + 0x08);
	__raw_writel(efr | (1 << 4), MMU_UART3_BASE + 0x08);

	__raw_writel(0x00, MMU_UART3_BASE + 0x0C);
	ier = __raw_readl(MMU_UART3_BASE + 0x04);
	__raw_writel(0x00, MMU_UART3_BASE + 0x04); /* Disable IRQs */

	__raw_writel(0x80, MMU_UART3_BASE + 0x0C);
	__raw_writel(0x06, MMU_UART3_BASE + 0x08); /* Disable FIFO */ 

	__raw_writel(0xBF, MMU_UART3_BASE + 0x0C);
	
	/* Clock divisor 
	 *
	 * Determined based on uart_speed parameter:
	 * UART_CLOCK_BASE_RATE / speed (i.e 0x1A for 115200 etc.) 
	 */
	
	div = (UART_CLOCK_BASE_RATE / uart_speed) & 0x3FFF;
	dll = div & 0xFF;
	dlh = (div >> 8) & 0x3F;
	
	/* It's 14 bit 
	 * 8 LSB bits goes to 0x00 register
	 * 6 MSB bits goes to 0x04 register
	 */
	
	__raw_writel(dll, MMU_UART3_BASE + 0x00);
	__raw_writel(dlh, MMU_UART3_BASE + 0x04);

	__raw_writel(0xBF, MMU_UART3_BASE + 0x0C);
	__raw_writel(0x00, MMU_UART3_BASE + 0x08);
	__raw_writel(0x03, MMU_UART3_BASE + 0x0C);
	__raw_writel(0x00, MMU_UART3_BASE + 0x20);

	printk(KERN_INFO CTRL_DEVNAME ": UART3 configured for %u baudrate (div = 0x%04x).\n", uart_speed, div);
	printk(KERN_INFO CTRL_DEVNAME ": UART3 registers: lcr[0x%4X], efr[0x%4X], mdr1[0x%4x], ier[0x%4X], dll[0x%4x], dlh[0x%4x]\n", lcr, efr, mdr1, ier, dll, dlh);

	/* Wait for it to be ready */
	msleep(1);
	
	/* Test it */
	__raw_writel('h', MMU_UART3_BASE + 0x00); while ((__raw_readl(MMU_UART3_BASE + 0x44) & 1) != 0);
	__raw_writel('b', MMU_UART3_BASE + 0x00); while ((__raw_readl(MMU_UART3_BASE + 0x44) & 1) != 0);
	__raw_writel('o', MMU_UART3_BASE + 0x00); while ((__raw_readl(MMU_UART3_BASE + 0x44) & 1) != 0);
	__raw_writel('o', MMU_UART3_BASE + 0x00); while ((__raw_readl(MMU_UART3_BASE + 0x44) & 1) != 0);
	__raw_writel('t', MMU_UART3_BASE + 0x00); while ((__raw_readl(MMU_UART3_BASE + 0x44) & 1) != 0);
	__raw_writel('m', MMU_UART3_BASE + 0x00); while ((__raw_readl(MMU_UART3_BASE + 0x44) & 1) != 0);
	__raw_writel('o', MMU_UART3_BASE + 0x00); while ((__raw_readl(MMU_UART3_BASE + 0x44) & 1) != 0);
	__raw_writel('d', MMU_UART3_BASE + 0x00); while ((__raw_readl(MMU_UART3_BASE + 0x44) & 1) != 0);
	__raw_writel('!', MMU_UART3_BASE + 0x00); while ((__raw_readl(MMU_UART3_BASE + 0x44) & 1) != 0);
	__raw_writel('\r', MMU_UART3_BASE + 0x00); while ((__raw_readl(MMU_UART3_BASE + 0x44) & 1) != 0);
	__raw_writel('\n', MMU_UART3_BASE + 0x00); while ((__raw_readl(MMU_UART3_BASE + 0x44) & 1) != 0);
}

int __attribute__((__naked__)) do_branch(void *bootlist, uint32_t bootsize, uint32_t new_ttbl, void *func, 
																				 uint32_t p_emu_uart, uint32_t p_powerup_reason, uint16_t p_battery_status_at_boot,
																				 uint8_t p_cid_recover_boot) 
{
	__asm__ volatile 
	(
		"sub    sp, sp, #0x10\n"
		"stmfd  sp!, {r0-r3}\n"
	);
		
	/* Reset SDMA */
	__raw_writel(__raw_readl(MMU_SDMA_BASE + 0x2C) | 2, MMU_SDMA_BASE + 0x2C);
	while (__raw_readl(MMU_SDMA_BASE + 0x28) != 1);

	local_flush_tlb_all();
	v7_flush_kern_cache_all();

	/* Disable any caches */
	__asm__ volatile 
	(
		"mov    r0, #0x00\n"
		
		/* Invalidate Cache */
		"mcr    p15, 0, r0, c7, c5, 0\n"
		"mcr    p15, 0, r0, c7, c5, 4\n"
		"mcr    p15, 0, r0, c7, c5, 6\n"
		
		/* Invalidate TLBs */
		"mcr    p15, 0, r0, c8, c7, 0\n"
		"mcr    p15, 0, r0, c8, c6, 0\n"
		"mcr    p15, 0, r0, c8, c5, 0\n"
		
		/* Barriers */
		"mcr     p15, 0, r0, c7, c10, 4\n"
		"mcr     p15, 0, r0, c7, c5, 4\n"
		
		/* Control Register */
		"mrc    p15, 0, r0, c1, c0, 0\n"
		
		/* Disable Branch Prediction & Instruction Cache */
		"bic    r0, r0, #0x1800\n" //-i -z
		
		/* Disable cache requests for everything (Instruction, Data, L2) */
		"bic    r0, r0, #0x0006\n" //-c -a
		"mcr    p15, 0, r0, c1, c0, 0\n"
		
		/* Auxiliary Control Register */
		"mrc    p15, 0, r0, c1, c0, 1\n"
		
		/* Disable L1 & L2 cache */
		"bic    r0, #0x02\n"
		"mcr    p15, 0, r0, c1, c0, 1\n"
		
		"ldmfd  sp!, {r0-r3}\n"
		"add    sp, sp, #0x10\n"
		
		/* OK - Ready to branch */
		"bx     r3\n"
	);

	return 0;
}

static void l1_map(uint32_t *table, uint32_t phys, uint32_t virt, size_t sects, uint32_t flags) 
{
	uint32_t physbase, virtbase;

	physbase = phys >> 20;
	virtbase = virt >> 20;
	while (sects-- > 0)
		table[virtbase++] = (physbase++ << 20) | flags;
}

#define L1_NORMAL_MAPPING (PMD_TYPE_SECT | PMD_SECT_AP_WRITE | PMD_SECT_WB)
#define L1_DEVICE_MAPPING (PMD_TYPE_SECT | PMD_SECT_AP_WRITE | PMD_SECT_UNCACHED)
void build_l1_table(uint32_t *table) 
{
	memset(table, 0, 4*4096);
	l1_map(table, PHYS_OFFSET, PHYS_OFFSET, 512-12, L1_NORMAL_MAPPING);
	l1_map(table, PHYS_OFFSET, PAGE_OFFSET, 512-12, L1_NORMAL_MAPPING);
	l1_map(table, 0x48000000, MMU_48000000, 1, L1_NORMAL_MAPPING);
	l1_map(table, 0x49000000, MMU_49000000, 1, L1_NORMAL_MAPPING);
}

static int emu_uart = 0;
module_param(emu_uart, int, 0);

static int find_my_dev(struct device *dev, void *data);
#define to_platform_driver(x) container_of((x), struct platform_driver, driver)

static void disable_dss(void)
{
    struct device *dev;
    struct platform_device *plat_dev;
    struct device_driver *drv; 
    struct platform_driver *plat_drv; 
    pm_message_t pt;

    dev = device_find_child(&platform_bus, "omapdss", find_my_dev);
    if (!dev)
    {
        printk("Could not find omapdss device\n");
        return;
    }
    drv = dev->driver;
    if (!drv)
    {
        printk("Could not find omapdss driver\n");
        return;
    }
    plat_drv = to_platform_driver(drv);

    plat_dev = to_platform_device(dev);

    if (!plat_drv->suspend)
    {
        printk("Could not find suspend in omapdss driver\n");
        return;
    }
    pt.event = 0;
    plat_drv->suspend(plat_dev, pt);
}  

int hboot_boot(int handle) 
{
	bootfunc_t boot_entry;
	uint32_t bootsize, listsize;
	void *bootlist;
	uint32_t l1_mem, *l1_table;
    
	l1_mem = get_high_pages(2);
	if (l1_mem == 0) 
	{
		printk(KERN_INFO CTRL_DEVNAME ": Failed to allocate new l1 table\n");
		return -ENOMEM;
	}

	if (l1_mem & 0x3FFF) 
	{
		free_high_pages((void*)l1_mem, 2);
		return -EINVAL;
	} 
	else
		l1_table = (uint32_t*)l1_mem;
	
	build_l1_table(l1_table);

	boot_entry = get_bootentry(&bootsize, handle);
	if (boot_entry == NULL) 
		return -EINVAL;
	
	bootlist = get_bootlist(&listsize, handle);
	if (bootlist == NULL) 
		return -ENOMEM;
		
	/* UART - if desired */
	if (emu_uart)
	{
		activate_emu_uart();
		reconfigure_emu_uart(emu_uart);
	}
	/* Disable dss, some defy want it. */
	disable_dss();

	/* Disable preempting */
	preempt_disable();
	local_irq_disable();
	local_fiq_disable();

	do_branch(bootlist, listsize, virt_to_phys(l1_table), boot_entry, emu_uart, bi_powerup_reason(),
						bi_battery_status_at_boot(), bi_cid_recover_boot());
	return -EBUSY;
}

static int hbootctrl_open(struct inode *inode, struct file *file);
static int hbootctrl_release(struct inode *inode, struct file *file);
static int hbootctrl_ioctl(struct inode *inode, struct file *file, unsigned int cmd, unsigned long arg);
static int hbootctrl_write(struct file *file, const char __user *buf, size_t count, loff_t *ppos);

static struct file_operations hbootctrl_ops = 
{
	.owner		= THIS_MODULE,
	.open			= hbootctrl_open,
	.release	= hbootctrl_release,
	.ioctl		= hbootctrl_ioctl,
	.write		= hbootctrl_write,
};

static int hbootctrl_open(struct inode *inode, struct file *file) 
{
	printk(KERN_INFO CTRL_DEVNAME ": `open' stub called\n");
	return 0;
}

static int hbootctrl_release(struct inode *inode, struct file *file) 
{
	printk(KERN_INFO CTRL_DEVNAME ": `release' stub called\n");
	return 0;
}

static int hbootctrl_ioctl(struct inode *inode, struct file *file, unsigned int cmd, unsigned long arg) 
{
	int ret;
	int handle;
	struct hboot_buffer_req buf_req;

	ret = 0;
	switch (cmd) 
	{
		case HBOOT_ALLOCATE_BUFFER:
			if (copy_from_user((void*)&buf_req, (void*)arg, sizeof(struct hboot_buffer_req)) != 0)
				return -EINVAL;

			ret = allocate_buffer(buf_req.tag, buf_req.type, buf_req.attrs, buf_req.size, buf_req.rest);
			break;
			
		case HBOOT_FREE_BUFFER:
			handle = (int)arg;
			ret = free_buffer(handle);
			break;
			
		case HBOOT_SELECT_BUFFER:
			handle = (int)arg;
			ret = select_buffer(handle);
			if (ret >= 0)
				file->f_pos = 0;
			
			break;
			
		case HBOOT_BOOT:
			handle = (int)arg;
			ret = hboot_boot(handle);
			break;
			
		default:
			ret = -EINVAL;
			break;
	}
	return ret;
}

static int hbootctrl_write(struct file *file, const char __user *buf, size_t count, loff_t *ppos) 
{
	return buffer_append_userdata(buf, count, ppos);
}  

static int find_my_dev(struct device *dev, void *data)                                                                                                                          
{                                                                                                                                                                                
    char *drv_name = "UNKNOWN";
    if (dev->driver && dev->driver->name)
        drv_name = dev->driver->name;

    printk("dev[%s], driver[%s]\n", dev_name(dev), drv_name);
    if (!strncmp((char*)data, dev_name(dev), strlen((char*)data))) {                                                                                                             
        printk(KERN_INFO "Found it\n");                                                                                                                                          
        return 1;                                                                                                                                                                
    }                                                                                                                                                                            
    return 0;                                                                                                                                                                    
}    

int __init hbootmod_init(void) 
{
	int ret;

	ret = buffers_init();
	if (ret < 0) 
	{
		printk(KERN_ERR CTRL_DEVNAME ": Failed to initialize buffers table\n");
		return ret;
	}

	ret = register_chrdev(0, CTRL_DEVNAME, &hbootctrl_ops);
	if (ret < 0) 
	{
		printk(KERN_ERR CTRL_DEVNAME ": Failed to register dev\n");
		buffers_destroy();
		return ret;
	}
	dev_major = ret;

	printk(KERN_INFO CTRL_DEVNAME ":  Successfully registered.\n");
	return 0;
}

module_init(hbootmod_init);
MODULE_LICENSE("GPL"); 
