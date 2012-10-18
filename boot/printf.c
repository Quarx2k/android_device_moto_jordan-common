/*
 * printf.c: C stdio-compatible printf function that writes to serial port.
 *
 * Copyright 2006 Motorola, Inc.
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
 *
 */

/* Date         Author          Comment
 * ===========  ==============  ==============================================
 * 04-Oct-2006  Motorola        Initial revision.
 */

#include "common.h"
#include "stdarg.h"
#define PRINTF_BUFFER_SIZE  1024

/* *********************************************************************
 * printf
 *
 * Print a formatted null-terminated string to the output device.
 *
 * Parameters:
 *   fmt - Format string.
 *   ... - Variable argument; number and type specified by fmt.
 *
 * Return Value:
 *   The number of characters sent to the output device (excluding
 *   any automatically added carriage returns).
 *
 * Note: This function is not-reentrant.
 *
 * ********************************************************************/

int vprintf(const char *fmt, va_list args) 
{
	static char print_buffer[PRINTF_BUFFER_SIZE];
	vsprintf(print_buffer, fmt, args);
	return puts(print_buffer);
}

int printf(const char *fmt, ...)
{
	int ret;
	va_list args;

	va_start(args, fmt);
	ret = vprintf(fmt, args);
	va_end(args);

	return ret;
}
