/*
 * board-mapphone-emu_uart.c
 *
 * Copyright (C) 2009 Motorola, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

/* Date         Author          Comment
 * ===========  ==============  ==============================================
 * Jun-26-2009  Motorola	Initial revision.
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/delay.h>
#include <linux/dma-mapping.h>
#include <linux/platform_device.h>
#include <linux/err.h>
#include <linux/clk.h>
#include <linux/io.h>

#include <linux/spi/spi.h>
#include <plat/system.h>
#include <linux/irq.h>
#include <linux/spi/spi.h>                                                                                                                                                       
#include <linux/spi/cpcap.h>                                                                                                                                                     
#include <linux/spi/cpcap-regbits.h> 

#include <plat/dma.h>
#include <plat/clock.h>
#include <plat/board-mapphone-emu_uart.h>
#include <plat/hardware.h>
#include <plat/omap34xx.h>

/*
 * Register definitions for CPCAP related SPI register
 */
#define OMAP2_MCSPI_MAX_FREQ		48000000

#define OMAP2_MCSPI_REVISION		0x00
#define OMAP2_MCSPI_SYSCONFIG		0x10
#define OMAP2_MCSPI_SYSSTATUS		0x14
#define OMAP2_MCSPI_IRQSTATUS		0x18
#define OMAP2_MCSPI_IRQENABLE		0x1c
#define OMAP2_MCSPI_WAKEUPENABLE	0x20
#define OMAP2_MCSPI_SYST		0x24
#define OMAP2_MCSPI_MODULCTRL		0x28

/* per-channel banks, 0x14 bytes each, first is: */
#define OMAP2_MCSPI_CHCONF0		0x2c
#define OMAP2_MCSPI_CHSTAT0		0x30
#define OMAP2_MCSPI_CHCTRL0		0x34
#define OMAP2_MCSPI_TX0			0x38
#define OMAP2_MCSPI_RX0			0x3c

/* per-register bitmasks: */

#define OMAP2_MCSPI_SYSCONFIG_AUTOIDLE	(1 << 0)
#define OMAP2_MCSPI_SYSCONFIG_SOFTRESET	(1 << 1)
#define OMAP2_AFTR_RST_SET_MASTER	(0 << 2)

#define OMAP2_MCSPI_SYSSTATUS_RESETDONE	(1 << 0)
#define OMAP2_MCSPI_SYS_CON_LVL_1 1
#define OMAP2_MCSPI_SYS_CON_LVL_2 2

#define OMAP2_MCSPI_MODULCTRL_SINGLE	(1 << 0)
#define OMAP2_MCSPI_MODULCTRL_MS	(1 << 2)
#define OMAP2_MCSPI_MODULCTRL_STEST	(1 << 3)

#define OMAP2_MCSPI_CHCONF_PHA		(1 << 0)
#define OMAP2_MCSPI_CHCONF_POL		(1 << 1)
#define OMAP2_MCSPI_CHCONF_CLKD_MASK	(0x0f << 2)
#define OMAP2_MCSPI_CHCONF_EPOL		(1 << 6)
#define OMAP2_MCSPI_CHCONF_WL_MASK	(0x1f << 7)
#define OMAP2_MCSPI_CHCONF_TRM_RX_ONLY	(0x01 << 12)
#define OMAP2_MCSPI_CHCONF_TRM_TX_ONLY	(0x02 << 12)
#define OMAP2_MCSPI_CHCONF_TRM_MASK	(0x03 << 12)
#define OMAP2_MCSPI_CHCONF_TRM_TXRX	(~OMAP2_MCSPI_CHCONF_TRM_MASK)
#define OMAP2_MCSPI_CHCONF_DMAW		(1 << 14)
#define OMAP2_MCSPI_CHCONF_DMAR		(1 << 15)
#define OMAP2_MCSPI_CHCONF_DPE0		(1 << 16)
#define OMAP2_MCSPI_CHCONF_DPE1		(1 << 17)
#define OMAP2_MCSPI_CHCONF_IS		(1 << 18)
#define OMAP2_MCSPI_CHCONF_TURBO	(1 << 19)
#define OMAP2_MCSPI_CHCONF_FORCE	(1 << 20)
#define OMAP2_MCSPI_CHCONF_TCS0		(1 << 25)
#define OMAP2_MCSPI_CHCONF_TCS1		(1 << 26)
#define OMAP2_MCSPI_CHCONF_TCS_MASK	(0x03 << 25)

#define OMAP2_MCSPI_SYSCFG_WKUP		(1 << 2)
#define OMAP2_MCSPI_SYSCFG_IDL		(2 << 3)
#define OMAP2_MCSPI_SYSCFG_CLK		(2 << 8)
#define OMAP2_MCSPI_WAKEUP_EN		(1 << 1)
#define OMAP2_MCSPI_IRQ_WKS		(1 << 16)
#define OMAP2_MCSPI_CHSTAT_RXS		(1 << 0)
#define OMAP2_MCSPI_CHSTAT_TXS		(1 << 1)
#define OMAP2_MCSPI_CHSTAT_EOT		(1 << 2)

#define OMAP2_MCSPI_CHCTRL_EN		(1 << 0)
#define OMAP2_MCSPI_MODE_IS_MASTER	0
#define OMAP2_MCSPI_MODE_IS_SLAVE	1
#define OMAP_MCSPI_WAKEUP_ENABLE	1

/*mcspi base address: (0x48098000)1st SPI, (0x4809A00) 2nd SPI*/
#define OMAP_MCSPI_BASE	0x48098000

#define WORD_LEN            32
#define CLOCK_DIV           12	/* 2^(12)=4096  48000000/4096<19200 */

#define LEVEL1              1
#define LEVEL2              2
#define WRITE_CPCAP         1
#define READ_CPCAP          0

#define CM_ICLKEN1_CORE  0x48004A10
#define CM_FCLKEN1_CORE  0x48004A00
#define OMAP2_MCSPI_EN_MCSPI1   (1 << 18)

#define  RESET_FAIL      1
#define RAW_MOD_REG_BIT(val, mask, set) do { \
    if (set) \
		val |= mask; \
    else \
		 val &= ~mask; \
} while (0)

struct cpcap_dev {
	u16 address;
	u16 value;
	u32 result;
	int access_flag;
};

/*   Although SPI driver is provided through linux system as implemented above,
 *   it can not cover some special situation.
 *
 *   During Debug phase, OMAP may need to acess CPCAP by SPI
 *   to configure or check related register when boot up is not finished.
 *   However, at this time, spi driver integrated in linux system may not
 *   be initialized properly.
 *
 *   So we provode the following SPI driver with common API for access capcap
 *   by SPI directly, i.e. we will skip the linux system driver,
 *   but access SPI hardware directly to configure read/write specially for
 *   cpcap access.
 *
 *   So developer should be very careful to use these APIs:
 *
 *        read_cpcap_register_raw()
 *        write_cpcap_register_raw()
 *
 *   Pay attention: Only use them when boot up phase.
 *   Rasons are as follows:
 *   1. Although we provide protection on these two APIs for concurrency and
 *      race conditions, it may impact the performance of system
 *      because it will mask all interrupts during access.
 *   2. Calling these APIs will reset all SPI registers, and may make previous
 *      data lost during run time.
 *
 *      So, if developer wants to access CPCAP after boot up is finished,
 *      we suggest they should use poweric interface.
 *
 */
static inline void raw_writel_reg(u32 value, u32 reg)
{
    unsigned int absolute_reg = (u32)OMAP_MCSPI_BASE + reg;
#if defined(LOCAL_DEVELOPER_DEBUG)
    printk(KERN_ERR " raw write reg =0x%x value=0x%x \n", absolute_reg,
            value);
#endif
    omap_writel(value, absolute_reg);
}

static inline u32 raw_readl_reg(u32 reg)
{
    u32 result;
    unsigned int absolute_reg = (u32)OMAP_MCSPI_BASE + reg;
    result = omap_readl(absolute_reg);
#if defined(LOCAL_DEVELOPER_DEBUG)
    printk(KERN_ERR " raw read reg =0x%x result =0x%x  \n",
            absolute_reg, result);
#endif
    return result;
}

static void write_omap_mux_register(u16 offset, u8 mode, u8 input_en)
{
    u16 tmp_val, reg_val;
    u32 reg = OMAP343X_CTRL_BASE + offset;

    reg_val = mode | (input_en << 8) | 0x0018;
    tmp_val = omap_readw(reg) & ~(0x0007 | (1 << 8) | 0x0018);
    reg_val = reg_val | tmp_val;
    omap_writew(reg_val, reg);
}


static int cpcap_spi_access(struct spi_device *spi, u8 *buf,
                size_t len)
{
    struct spi_message m;
    struct spi_transfer t = 
    {
        .tx_buf = buf,
        .len = len,
        .rx_buf = buf,
        .bits_per_word = 32,
    };
    spi_message_init(&m);
    spi_message_add_tail(&t, &m);
    return spi_sync(spi, &m);
}

static int cpcap_config_for_read(struct spi_device *spi, unsigned short reg,
                 unsigned short *data)
{
    int status = -ENOTTY;
    u32 buf32;  /* force buf to be 32bit aligned */
    u8 *buf = (u8 *) &buf32;

    if (spi != NULL) 
    {
        buf[3] = (reg >> 6) & 0x000000FF;
        buf[2] = (reg << 2) & 0x000000FF;
        buf[1] = 0;
        buf[0] = 0;
        status = cpcap_spi_access(spi, buf, 4);
        if (status == 0)
            *data = buf[0] | (buf[1] << 8);
    }
    return status;
}

static int cpcap_config_for_write(struct spi_device *spi, unsigned short reg,
                  unsigned short data)
{
    int status = -ENOTTY;
    u32 buf32;  /* force buf to be 32bit aligned */
    u8 *buf = (u8 *) &buf32;

    if (spi != NULL) 
    {
        buf[3] = ((reg >> 6) & 0x000000FF) | 0x80;
        buf[2] = (reg << 2) & 0x000000FF;
        buf[1] = (data >> 8) & 0x000000FF;
        buf[0] = data & 0x000000FF;
        status = cpcap_spi_access(spi, buf, 4);
    }

    return status;
}

static int find_ms2_dev(struct device *dev, void *data)
{
    if (!strncmp((char*)data, dev_name(dev), strlen((char*)data))) 
    {
        printk(KERN_INFO "Found it\n");
        return 1;
    }
    return 0;
} 


void czecho_activate_emu_uart(void)
{
    int i;
    u16 tmp = 0;
    struct device *cpcap = NULL;
    struct cpcap_device *cpcap_dev;
    struct spi_device *spi;

    printk(KERN_INFO "Searching for cpcap_usb...\n");

    cpcap = device_find_child(&platform_bus, "cpcap_usb", find_ms2_dev);

    if (cpcap == NULL)
        return;

    cpcap_dev = cpcap->platform_data;
    if (cpcap_dev == NULL)
        return;

    spi = cpcap_dev->spi;


    cpcap_config_for_read(spi, 18, &tmp);
    printk(KERN_ALERT "Reading CPCAP vendor_version: 0x%04X\n", tmp);

    /*
     * Step 1:
     * Configure OMAP SCM to set all ULPI pin of USB OTG to SAFE MODE
     */
    for (i = 0; i < 0x18; i += 2)
        write_omap_mux_register(0x1A2 + i, 7, 0);

    /*
     * Step 2:
     * Configure CPCAP to route UART3 to USB port; Switch VBUSIN to supply
     * UART/USB transeiver and set VBUS standby mode 3
     */
    cpcap_config_for_write(spi, 897, 0x0101);
    cpcap_config_for_write(spi, 411, 0x014C);

    /* Step 3:
     * Configure OMAP SCM to set ULPI port as UART3 function
     */
    /*
     * Set UART2(3) RX pin in safe mode
     */
    write_omap_mux_register(0x19E, 7, 0);
    /*
     * Route UART2(3) TX to ULPIDATA0, RX to ULPIDATA1
     */
    write_omap_mux_register(0x1AA, 2, 0);
    write_omap_mux_register(0x1AC, 2, 1);

    printk("Resetting UART\n");
    printk("Waiting for reset finish\n");

    printk
        (KERN_ALERT "WARNING: MiniUSB port works in UART3 mode,"
         "the USB functionality UNAVAILABLE!\n");

}
