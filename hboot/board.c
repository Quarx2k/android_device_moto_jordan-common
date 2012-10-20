#include "common.h"
#include "board.h"
#include "string.h"
#include "memory.h"

char cmdline_buffer[1024];

#ifdef BOARD_UMTS_SHOLES

/* CMDLINE */

#define DEFAULT_CMDLINE \
"console=/dev/null console=ttyMTD10 vram=4M omapfb.vram=0:4M rw mem=244M@0x80C00000 init=/init ip=off brdrev=P2A " \
"mtdparts=omap2-nand.0:1536k@2176k(pds),384k@4480k(cid),384k@7424k(misc),3584k(boot),4608k@15232k(recovery)," \
"8960k(cdrom),179840k@29184k(system),106m@209408k(cache),201856k(userdata),1536k(cust),2m@521728k(kpanic)"

#define DEFAULT_UART_CMDLINE \
"console=ttyS2,%un8 vram=4M omapfb.vram=0:4M rw mem=244M@0x80C00000 init=/init ip=off brdrev=P2A " \
"mtdparts=omap2-nand.0:1536k@2176k(pds),384k@4480k(cid),384k@7424k(misc),3584k(boot),4608k@15232k(recovery)," \
"8960k(cdrom),179840k@29184k(system),106m@209408k(cache),201856k(userdata),1536k(cust),2m@521728k(kpanic)"

/* Used registers */
#define GPTIMER1_BASE						0x48318000

#define CM_CLKEN_PLL_MPU				0x48004904
#define CM_IDLEST_MPU						0x48004920
#define CM_IDLEST_PLL_MPU				0x48004924
#define CM_AUTOIDLE_PLL_MPU			0x48004934
#define CM_CLKSEL1_PLL_MPU			0x48004940
#define CM_CLKSEL2_PLL_MPU			0x48004944
#define CM_CLKSTCTRL_MPU				0x48004948
#define CM_CLKSTST_MPU					0x4800494C

#define CM_CLKEN_PLL						0x48004D00
#define CM_CLKEN2_PLL						0x48004D04
#define CM_IDLEST_CKGEN					0x48004D20
#define CM_IDLEST2_CKGEN				0x48004D24
#define CM_AUTOIDLE_PLL					0x48004D30
#define CM_AUTOIDLE2_PLL				0x48004D34
#define CM_CLKSEL1_PLL					0x48004D40
#define CM_CLKSEL2_PLL					0x48004D44
#define CM_CLKSEL3_PLL					0x48004D48
#define CM_CLKSEL4_PLL					0x48004D4C
#define CM_CLKSEL5_PLL					0x48004D50
#define CM_CLKOUT_CTRL					0x48004D70

/* Get CMDLINE */
const char *board_get_cmdline()
{
	if (!cfg_emu_uart)
		return DEFAULT_CMDLINE;
	else
	{
		sprintf(cmdline_buffer, DEFAULT_UART_CMDLINE, cfg_emu_uart);
		return cmdline_buffer;
	}
}

void board_init()
{
	int i;

	/* 
	 * This is probably not mandatory, but just in case setup
	 * them to state in which they exited MBM
	 */

	/* Stop & Reset GPTIMER1 */
	modify_register32(GPTIMER1_BASE + 0x24, 0x1, 0x0);

	write32(1, GPTIMER1_BASE + 0x10);
	while ((read32(GPTIMER1_BASE + 0x14) & 1) != 1);

	/* 
	 * MBM boots OMAP3430 with 500 MHz. 
	 */

	/* Unlock PLL */ 
	modify_register32(CM_CLKEN_PLL_MPU, 0x7, 0x5);
	while ((read32(CM_IDLEST_PLL_MPU) & 1) != 0);

	/* Set the dividers */
	modify_register32(CM_CLKSEL2_PLL_MPU, 0x1F, 0x1);
	modify_register32(CM_CLKSEL1_PLL_MPU, 0x07 << 18, 0x01 << 18);
	modify_register32(CM_CLKSEL1_PLL_MPU, 0x7FF << 8, 0xFA << 8);
	modify_register32(CM_CLKSEL1_PLL_MPU, 0x7F, 0x0C);

	/* Set frequency */
	modify_register32(CM_CLKEN_PLL_MPU, 0xF << 4, 0x7 << 4);

	/* Relock PLL */
	i = 0;

	modify_register32(CM_CLKEN_PLL_MPU, 0x7, 0x7);
	while ((read32(CM_IDLEST_PLL_MPU) & 1) != 1)
	{
		i++;
		if (i > 0x2000)
		{
			/* Kick it again */
			i = 0;
			modify_register32(CM_CLKEN_PLL_MPU, 0x7, 0x7);
		}
	}

	/* Unlock DPPL5 */
	modify_register32(CM_CLKEN2_PLL, 0x7, 0x1);
	while((read32(CM_IDLEST2_CKGEN) & 1) != 0);
}

#elif BOARD_UMTS_JORDAN

/* CMDLINE */

#define DEFAULT_CMDLINE \
"console=/dev/null mem=498M init=/init ip=off brdrev=P3A omapfb.vram=0:4M "

#define DEFAULT_UART_CMDLINE \
"console=ttyS2,%un8 mem=498M init=/init ip=off brdrev=P3A omapfb.vram=0:4M "

/* Used registers */
#define GPTIMER1_BASE						0x48318000

#define CM_CLKEN_PLL_MPU				0x48004904
#define CM_IDLEST_MPU						0x48004920
#define CM_IDLEST_PLL_MPU				0x48004924
#define CM_AUTOIDLE_PLL_MPU			0x48004934
#define CM_CLKSEL1_PLL_MPU			0x48004940
#define CM_CLKSEL2_PLL_MPU			0x48004944
#define CM_CLKSTCTRL_MPU				0x48004948
#define CM_CLKSTST_MPU					0x4800494C

#define CM_CLKEN_PLL						0x48004D00
#define CM_CLKEN2_PLL						0x48004D04
#define CM_IDLEST_CKGEN					0x48004D20
#define CM_IDLEST2_CKGEN				0x48004D24
#define CM_AUTOIDLE_PLL					0x48004D30
#define CM_AUTOIDLE2_PLL				0x48004D34
#define CM_CLKSEL1_PLL					0x48004D40
#define CM_CLKSEL2_PLL					0x48004D44
#define CM_CLKSEL3_PLL					0x48004D48
#define CM_CLKSEL4_PLL					0x48004D4C
#define CM_CLKSEL5_PLL					0x48004D50
#define CM_CLKOUT_CTRL					0x48004D70

/* Get CMDLINE */
const char *board_get_cmdline()
{
	if (!cfg_emu_uart)
		return DEFAULT_CMDLINE;
	else
	{
		sprintf(cmdline_buffer, DEFAULT_UART_CMDLINE, cfg_emu_uart);
		return cmdline_buffer;
	}
}

void board_init()
{
	int i;

	/* 
	 * This is probably not mandatory, but just in case setup
	 * them to state in which they exited MBM
	 */

	/* Stop & Reset GPTIMER1 */
	modify_register32(GPTIMER1_BASE + 0x24, 0x1, 0x0);

	write32(1, GPTIMER1_BASE + 0x10);
	while ((read32(GPTIMER1_BASE + 0x14) & 1) != 1);

	/* 
	 * MBM boots OMAP3430 with 500 MHz. 
	 */

	/* Unlock PLL */ 
	modify_register32(CM_CLKEN_PLL_MPU, 0x7, 0x5);
	while ((read32(CM_IDLEST_PLL_MPU) & 1) != 0);

	/* Set the dividers */
	modify_register32(CM_CLKSEL2_PLL_MPU, 0x1F, 0x1);
	modify_register32(CM_CLKSEL1_PLL_MPU, 0x07 << 18, 0x01 << 18);
	modify_register32(CM_CLKSEL1_PLL_MPU, 0x7FF << 8, 0xFA << 8);
	modify_register32(CM_CLKSEL1_PLL_MPU, 0x7F, 0x0C);

	/* Set frequency */
	modify_register32(CM_CLKEN_PLL_MPU, 0xF << 4, 0x7 << 4);

	/* Relock PLL */
	i = 0;

	modify_register32(CM_CLKEN_PLL_MPU, 0x7, 0x7);
	while ((read32(CM_IDLEST_PLL_MPU) & 1) != 1)
	{
		i++;
		if (i > 0x2000)
		{
			/* Kick it again */
			i = 0;
			modify_register32(CM_CLKEN_PLL_MPU, 0x7, 0x7);
		}
	}

	/* Unlock DPPL5 */
	modify_register32(CM_CLKEN2_PLL, 0x7, 0x1);
	while((read32(CM_IDLEST2_CKGEN) & 1) != 0);
}

#else

#error "No board defined!"

#endif
