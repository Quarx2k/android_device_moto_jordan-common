#include "types.h"
#include "stdio.h"
#include "string.h"
#include "board.h"
#include "common.h"

/* NOTE: These are initialized PRIOR bss cleanup */
uint32_t __attribute__ ((section ("data"))) cfg_powerup_reason = 0x00000080;
uint32_t __attribute__ ((section ("data"))) cfg_emu_uart = 0;

int putchar(int c) 
{
	if (!cfg_emu_uart)
		return;
	
	if (c == '\n')
		putchar('\r');
	
	while ((read32(BOARD_DEBUG_UART_BASE + 0x44) & 1) != 0);
	write32(c, BOARD_DEBUG_UART_BASE + 0x00);
	
	return (unsigned char)c;
}

int puts(const char *s) 
{
	unsigned char c;
	
	while (c = (unsigned char)(*s++)) 
	{
		if (putchar((int)c) == EOF)
			return EOF;
	}
	
	return 0;
}

void u_to_hex(int x, int digits, char *s)
{
	int i, digit;
	for (i = 0; i < digits; i++) 
	{
		digit = x&0x0f;
		s[digits-1 - i] = (digit > 9) ? (digit - 10 + 'a') : (digit + '0');
		x >>= 4;
	}
}
 