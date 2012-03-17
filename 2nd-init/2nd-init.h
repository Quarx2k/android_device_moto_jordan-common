/*
Copyright (C) 2010-2011 Skrilax_CZ (skrilax@gmail.com)

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*/

/* This file contains signatures of execve inside init */

#ifndef SECOND_INIT_H
#define SECOND_INIT_H

/*===============================================================================
 *
 * Signature of calling execve (syscall 11)
 *
 * execve:
 *
 * STMFD   SP!, {R4,R7}
 * MOV     R7, #0xB
 * SVC     0
 * LDMFD   SP!, {R4,R7}
 *
 * HEX: 90002DE9 0B70A0E3 000000EF 9000BDE8
===============================================================================*/

char execve_code[] = {
	0x90, 0x00, 0x2D, 0xE9,
	0x0B, 0x70, 0xA0, 0xE3,
	0x00, 0x00, 0x00, 0xEF,
	0x90, 0x00, 0xBD, 0xE8
};
#endif //!SECOND_INIT_H
