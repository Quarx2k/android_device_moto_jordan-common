#ifndef __BOARD_H__
#define __BOARD_H__

void board_init();
void muxing();
const char *board_get_cmdline();

/* Check defines */

#if BOARD_UMTS_SHOLES || BOARD_UMTS_JORDAN

#define BOARD_DEBUG_UART_BASE			0x49020000
#define BOARD_WDTIMER2_BASE			0x48314000
#define BOARD_MPU_INTC_BASE			0x48200000
#define BOARD_GLOBAL_REG_PRM_BASE		0x48307200

/*
 * L4 Peripherals - L4 Wakeup and L4 Core now
 */
#define OMAP34XX_CORE_L4_IO_BASE	0x48000000
#define OMAP34XX_WAKEUP_L4_IO_BASE	0x48300000
#define OMAP34XX_ID_L4_IO_BASE		0x4830A200
#define OMAP34XX_L4_PER			0x49000000
#define OMAP34XX_L4_IO_BASE		OMAP34XX_CORE_L4_IO_BASE

/* DMA4/SDMA */
#define OMAP34XX_DMA4_BASE              0x48056000

/* CONTROL */
#define OMAP34XX_CTRL_BASE		(OMAP34XX_L4_IO_BASE + 0x2000)

#else

#error "No board defined!"

#endif

#endif // __BOARD_H__

