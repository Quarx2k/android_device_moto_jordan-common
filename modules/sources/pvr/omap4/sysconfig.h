/**********************************************************************
 *
 * Copyright(c) 2008 Imagination Technologies Ltd. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful but, except
 * as otherwise stated in writing, without any warranty; without even the
 * implied warranty of merchantability or fitness for a particular purpose.
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * The full GNU General Public License is included in this distribution in
 * the file called "COPYING".
 *
 * Contact Information:
 * Imagination Technologies Ltd. <gpl-support@imgtec.com>
 * Home Park Estate, Kings Langley, Herts, WD4 8LZ, UK
 *
 ******************************************************************************/

#if !defined(__SOCCONFIG_H__)
#define __SOCCONFIG_H__

#include "syscommon.h"

#define VS_PRODUCT_NAME	"OMAP4"

#if defined(SGX_CLK_PER_192)
#define SYS_SGX_CLOCK_SPEED 192000000
#else
#if defined(SGX_CLK_CORE_DIV8)
#define SYS_SGX_CLOCK_SPEED 190464000
#else
#if defined(SGX_CLK_CORE_DIV5)
#if defined(CONFIG_SGX_REV110)
#define SYS_SGX_CLOCK_SPEED 304742400
#endif
#if defined(CONFIG_SGX_REV120)
#define SYS_SGX_CLOCK_SPEED 320000000
#endif
#endif
#endif
#endif

#define SYS_SGX_HWRECOVERY_TIMEOUT_FREQ		(100)
#define SYS_SGX_PDS_TIMER_FREQ				(1000)

#if !defined(SYS_SGX_ACTIVE_POWER_LATENCY_MS)
#define SYS_SGX_ACTIVE_POWER_LATENCY_MS		(100)
#endif



#define SYS_OMAP4430_SGX_REGS_SYS_PHYS_BASE  0x56000000
#define SYS_OMAP4430_SGX_REGS_SIZE           0xFFFF

#define SYS_OMAP4430_SGX_IRQ				 53 /* OMAP 4 IRQs are offset by 32 */



#endif
