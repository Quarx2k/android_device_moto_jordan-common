#include <asm/uaccess.h>
#include <asm/cacheflush.h>
#include <asm/tlbflush.h>
#include <asm/io.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/sched.h>
#include <linux/delay.h>
#include "hboot.h"
#include <linux/platform_device.h>
#include <linux/device.h>
#include <linux/suspend.h>
#include <plat/display.h>
#define CTRL_DEVNAME "hbootctrl"
static int dev_major;

void v7_flush_kern_cache_all(void);
void activate_emu_uart(void);
extern void czecho_activate_emu_uart(void);


/*
void uart_send_char(char c)
{
    uint32_t uart_base = 0xFB020000;// UART3

    while ((__raw_readl(uart_base + 0x44) & 1) != 0);
    __raw_writel(c, uart_base + 0x00);
}

void uart_send_text(char *s)
{
    while (*s)
    {
        if (*s == '\n')
        {
            uart_send_char('\n');
            uart_send_char('\r');
        }
        else
            uart_send_char(*s);
        
        ++s;
    }
}
*/

void reconfigure_uart(void)
{
    uint32_t uart_base = 0xFB020000;// UART3
   uint32_t lcr;
   uint32_t efr;
   uint32_t mdr1;
   uint32_t ier;
   uint32_t dlh, dll;

   printk("Enabling clocks for uart3\n");
   __raw_writel(__raw_readl(0xFA005000) | 0x0800, 0xFA005000);
   __raw_writel(__raw_readl(0xFA005010) | 0x0800, 0xFA005010);

   printk("About to reconfigure uart, base address[0x%4x]\n", uart_base);
   printk("Remap of addres[0x%4x] is[0x%4x]\n", 0x49020000, (int)ioremap(0x49020000, 1024));

    __raw_writel(__raw_readl(uart_base + 0x54) | 0x02, uart_base + 0x54);  //reset uart
    while (!(__raw_readl(uart_base + 0x58)&1));

   mdr1 = __raw_readl(uart_base + 0x20);
   __raw_writel(mdr1 | 0x07, uart_base + 0x20);

   lcr = __raw_readl(uart_base + 0x0C);
   __raw_writel(0xBF, uart_base + 0x0C);

   efr = __raw_readl(uart_base + 0x08);
   __raw_writel(efr | (1 << 4), uart_base + 0x08);

   __raw_writel(0x00, uart_base + 0x0C);
   ier = __raw_readl(uart_base + 0x04);
   __raw_writel(0x00, uart_base + 0x04);  //disabale IRQs

   __raw_writel(0xBF, uart_base + 0x0C);
   dll = __raw_readl(uart_base + 0x00);
   dlh = __raw_readl(uart_base + 0x04);
   __raw_writel(0x00, uart_base + 0x00);
   __raw_writel(0x00, uart_base + 0x04);

   __raw_writel(0x80, uart_base + 0x0C);
   __raw_writel(0x06, uart_base + 0x08); //disable FIFO 

   __raw_writel(0xBF, uart_base + 0x0C);
   __raw_writel(0x1A, uart_base + 0x00); //speed 115200
   __raw_writel(0x00, uart_base + 0x04);
   
   __raw_writel(0xBF, uart_base + 0x0C);
   __raw_writel(0x00, uart_base + 0x08);

   __raw_writel(0x03, uart_base + 0x0C);

   __raw_writel(0x00, uart_base + 0x20);


   printk("Uart configured. lcr[0x%4X], efr[0x%4X], mdr1[0x%4x], ier[0x%4X], dll[0x%4x], dlh[0x%4x]\n", lcr, efr, mdr1, ier, dll, dlh);
   printk("Sending one character out\n");
    __raw_writel('D', uart_base + 0x00); 
    while ((__raw_readl(uart_base + 0x44) & 1) != 0);
}
static int should_reset_usb = 2;
module_param(should_reset_usb, int, 0);

/*
static int should_reset_usb = 2;
module_param(should_reset_usb, int, 0);

void reset_usb(void)
{
    uint32_t u;
    char arr[100];

    if (should_reset_usb == 0)
    {
        uart_send_text("skipping usb resetting and clock setting\n");
        return;
    }
    uart_send_text("enabling ttl clock\n");
    __raw_writel(__raw_readl(0xFA004A08) | 0x04, 0xFA004A08);

    uart_send_text("enabling usb 120 clock\n");
    __raw_writel(__raw_readl(0xFA005400) | 0x02, 0xFA005400);

    uart_send_text("enabling usb 48 clock\n");
    __raw_writel(__raw_readl(0xFA005400) | 0x01, 0xFA005400);

    if (should_reset_usb == 1)
    {
        uart_send_text("skipping usb resetting\n");
        return;
    }

    uart_send_text("resetting usb\n");
    u = __raw_readl(0xFA064010);
    sprintf(arr, "uhh_sysconfig[0x%4X]\n", u);
    uart_send_text(arr);
    __raw_writel(__raw_readl(0xFA064010) | 0x02, 0xFA064010);
    while(!(__raw_readl(0xFA064014) & 0x01));

    uart_send_text("resetting usbtll\n");
    __raw_writel(__raw_readl(0xFA062010) | 0x02, 0xFA062010);
    while(!(__raw_readl(0xFA062014) & 0x01));

    uart_send_text("entering standby\n");
    __raw_writel(0x01, 0xFA064010);

    u = __raw_readl(0xFA064010);
    sprintf(arr, "uhh_sysconfig[0x%4X]\n", u);
    uart_send_text(arr);

    u = __raw_readl(0xFA005420);
    sprintf(arr, "cm_idlest[0x%4X]\n", u);
    uart_send_text(arr);


}
*/

// will _never_ return
int __attribute__((__naked__)) do_branch(void *bootlist, uint32_t bootsize, uint32_t new_ttbl, void *func) 
{
    __asm__ volatile (
            "stmfd  sp!, {r0-r3}\n"
            );

    printk("About to reset sdma\n");
    __raw_writel(__raw_readl(0xFA05602C)|2, 0xFA05602C);//sdma reset
    while (__raw_readl(0xFA056028)!=1);

    printk("About to flush tlb\n");
    local_flush_tlb_all();
    printk("About to flush kern\n");
    v7_flush_kern_cache_all();
    printk("About to asm\n");

       __asm__ volatile (
       "mov    r0, #0x00\n"
       "mcr    p15, 0, r0, c7, c5, 0\n"
       "mcr    p15, 0, r0, c7, c5, 6\n"
       "mrc    p15, 0, r0, c1, c0, 0\n"
       "bic    r0, r0, #0x1800\n" //-i -z
       "bic    r0, r0, #0x0006\n" //-c -a
       "mcr    p15, 0, r0, c1, c0, 0\n"
       "mrc    p15, 0, r0, c1, c0, 1\n"
       "bic    r0, #0x02\n"
       "mcr    p15, 0, r0, c1, c0, 1\n"
       "ldmfd  sp!, {r0-r3}\n"
       "bx r3\n"
       );

    return 0;
}

static void l1_map(uint32_t *table, uint32_t phys, uint32_t virt, size_t sects, uint32_t flags) {
	uint32_t physbase, virtbase;

	physbase = phys >> 20;
	virtbase = virt >> 20;
	while (sects-- > 0) {
		table[virtbase++] = (physbase++ << 20) | flags;
	}
}

#define L1_NORMAL_MAPPING (PMD_TYPE_SECT | PMD_SECT_AP_WRITE | PMD_SECT_WB)
#define L1_DEVICE_MAPPING (PMD_TYPE_SECT | PMD_SECT_AP_WRITE | PMD_SECT_UNCACHED)
void build_l1_table(uint32_t *table) {
    printk("PHYS OFFSET[0x%4X], PAGE_OFFSET[0x%4X], normal mapping[0x%4X]\n", (int)PHYS_OFFSET, (int)PAGE_OFFSET, (int)L1_NORMAL_MAPPING);
	memset(table, 0, 4*4096);
        l1_map(table, PHYS_OFFSET, PHYS_OFFSET, 512-12, L1_NORMAL_MAPPING);
        l1_map(table, PHYS_OFFSET, PAGE_OFFSET, 512-12, L1_NORMAL_MAPPING);
        l1_map(table, 0x48000000, 0xFA000000, 1, L1_NORMAL_MAPPING);
        l1_map(table, 0x49000000, 0xFB000000, 1, L1_NORMAL_MAPPING);
}

static int emu_uart = 0;
module_param(emu_uart, int, 0);

#include <plat/panel.h>

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

int hboot_boot(int handle) {
	bootfunc_t boot_entry;
	uint32_t bootsize, listsize;
	void *bootlist;
	uint32_t l1_mem, *l1_table;
    
    printk("hboot_boot\n");
	l1_mem = get_high_pages(2);
	if (l1_mem == 0) {
		printk("Failed to allocate new l1 table\n");
		return -ENOMEM;
	}
    printk("got l1_mem\n");
	if (l1_mem & 0x3fff) {
		printk("unaligned l1 table\n");
		free_high_pages((void*)l1_mem, 2);
		return -EINVAL;
	} else {
		l1_table = (uint32_t*)l1_mem;
	}
    printk("about to build l1_table\n");
	build_l1_table(l1_table);

    printk("about to get bootentry\n");
	boot_entry = get_bootentry(&bootsize, handle);
	if (boot_entry == NULL) {
		return -EINVAL;
	}
    printk("about to get bootlist\n");
	bootlist = get_bootlist(&listsize, handle);
	if (bootlist == NULL) {
		return -ENOMEM;
	}
    printk("about to do_branch... bootlist[0x%4X : 0x%4X], listsize[%d], new_ttbl[0x%4x : 0x%4X], boot_entry[0x%4X : 0x%4x]\n", (int)bootlist, (int)virt_to_phys(bootlist), (int)listsize, (int)l1_table, (int)virt_to_phys(l1_table), (int)boot_entry, (int)virt_to_phys(boot_entry));

        disable_dss();

	preempt_disable();
	local_irq_disable();
	local_fiq_disable();

    if (emu_uart)
    {
        printk("About to activate emu uart\n");
        czecho_activate_emu_uart();
        reconfigure_uart();
    }
    else
    {
        printk("skipping emu uart activation\n");
    }
    printk("About to reconfigure uart\n");

	do_branch(bootlist, listsize, virt_to_phys(l1_table), boot_entry);

/*	not a chance	*/
#if 0
	set_ttbl_base(l1_old);
	local_fiq_enable();
	local_irq_enable();
	preempt_enable();
#else
	while (1);
#endif
	return -EBUSY;
}

static int hbootctrl_open(struct inode *inode, struct file *file);
static int hbootctrl_release(struct inode *inode, struct file *file);
static int hbootctrl_ioctl(struct inode *inode, struct file *file, unsigned int cmd, unsigned long arg);
static int hbootctrl_write(struct file *file, const char __user *buf, size_t count, loff_t *ppos);

static struct file_operations hbootctrl_ops = {
	.owner		=	THIS_MODULE,
	.open		=	hbootctrl_open,
	.release	=	hbootctrl_release,
	.ioctl		=	hbootctrl_ioctl,
	.write		=	hbootctrl_write,

};

static int hbootctrl_open(struct inode *inode, struct file *file) {
	printk(KERN_INFO CTRL_DEVNAME ": `open' stub called\n");
	return 0;
}

static int hbootctrl_release(struct inode *inode, struct file *file) {
	printk(KERN_INFO CTRL_DEVNAME ": `release' stub called\n");
	return 0;
}

static int hbootctrl_ioctl(struct inode *inode, struct file *file, unsigned int cmd, unsigned long arg) {
	int ret;
	int handle;
	struct hboot_buffer_req buf_req;

	ret = 0;
	switch (cmd) {
		case HBOOT_ALLOCATE_BUFFER:
			if (copy_from_user((void*)&buf_req, (void*)arg, sizeof(struct hboot_buffer_req)) != 0) {
				return -EINVAL;
			}
			ret = allocate_buffer(buf_req.tag, buf_req.type, buf_req.attrs, buf_req.size, buf_req.rest);
			break;
		case HBOOT_FREE_BUFFER:
			handle = (int)arg;
			ret = free_buffer(handle);
			break;
		case HBOOT_SELECT_BUFFER:
			handle = (int)arg;
			ret = select_buffer(handle);
			if (ret >= 0) {
				file->f_pos = 0;
			}
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

static int hbootctrl_write(struct file *file, const char __user *buf, size_t count, loff_t *ppos) {
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

/*
static int suspend_my_dev(struct device *dev, void *data)
{                                                                                                                                                                                
    struct device_driver *drv; 
    pm_message_t pm;
    pm.event = 0;

    printk("dev[%s]\n", dev_name(dev));
    if (!strncmp((char*)data, dev_name(dev), strlen((char*)data))) 
    {                                                                                                             
        printk(KERN_INFO "Found it\n");                                                                                                                                          
        drv = dev->driver;
        if (drv == NULL)
        {
            printk("device does not have driver\n");
            return -1;
        }
        printk("driver name[%s]\n", drv->name);
        if (drv->shutdown == NULL)
        {
            printk("driver shutdown is NULL\n");

            if (drv->suspend == NULL)
            {
                printk("driver suspend is NULL\n");
                return -1;
            }
            printk("suspending dss \n");
            drv->suspend(dev, pm);
            //drv->shutdown(dev, pm);
            printk("dss suspend\n");

        }
        else
        {
            printk("shutding down dss \n");
            drv->shutdown(dev);
            //drv->shutdown(dev, pm);
            printk("dss shutdown\n");
        }
    }                                                                                                                                                                            
    return 0;                                                                                                                                                                    
}    
*/

/*
int shutdown_dss(void)
{
    struct device *dev;
    struct device_driver *drv; 
    pm_message_t pm;
    struct klist_iter i;
    struct device *child, *parent = &platform_bus;
    int count = 0;
    pm.event = 0;

    device_for_each_child(&platform_bus, "omapdss", suspend_my_dev); 
    device_for_each_child(&platform_bus, "omap-panel", suspend_my_dev); 

    return 0;

}
*/

int __init hboot_init(void) {
	int ret;

	printk("hboot_init\n");
	ret = buffers_init();
	if (ret < 0) {
		printk(KERN_WARNING CTRL_DEVNAME ": Failed to initialize buffers table\n");
		return ret;
	}

	ret = register_chrdev(0, CTRL_DEVNAME, &hbootctrl_ops);
	if (ret < 0) {
		printk(KERN_WARNING CTRL_DEVNAME ": Failed to register dev\n");
		buffers_destroy();
		return ret;
	}
	dev_major = ret;

	if (ret < 0) {
		printk(KERN_WARNING CTRL_DEVNAME ": Failed to create dev\n");
		unregister_chrdev(dev_major, CTRL_DEVNAME);
		buffers_destroy();
		return ret;
	}

	printk(KERN_INFO CTRL_DEVNAME ":  Successfully registered dev\n");
	return 0;
}

void __exit hboot_exit(void) {
	if (dev_major) {
		buffers_destroy();
		unregister_chrdev(dev_major, CTRL_DEVNAME);
	}
	return;
}

module_init(hboot_init);
module_exit(hboot_exit);
MODULE_LICENSE("GPL"); 
