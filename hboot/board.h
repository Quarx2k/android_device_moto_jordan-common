#ifndef __BOARD_H__
#define __BOARD_H__

void board_init();
const char *board_get_cmdline();

/* Check defines */

#if BOARD_UMTS_SHOLES || BOARD_UMTS_JORDAN

#define BOARD_DEBUG_UART_BASE			0x49020000
#define BOARD_WDTIMER2_BASE			0x48314000
#define BOARD_MPU_INTC_BASE			0x48200000
#define BOARD_MPU_INTC_BASE			0x48200000

#else

#error "No board defined!"

#endif

#endif // __BOARD_H__

