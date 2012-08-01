/*
 * TLS-enable: on-the-fly patch for locked Motorola's 2.6.32 kernel for Milestone
 * that has been compiled by Moto without CONFIG_HAS_TLS_REG set, using these two hacks:
 * http://gitorious.org/0xlab-kernel/kernel/commit/bd56f9006e21115a5baf3368a5728680f4ddc1e6
 * https://gitorious.org/rowboat/kernel/commit/5bb91b63deba763e08ccda469412f03c65161ec9
 * This patch removes the effect of the above hacks and restores normal kernel behaviour
 * like if it was compiled with CONFIG_HAS_TLS_REG=y .
 * 
 * Copyright (C) 2012 Nadlabak
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */


#include <linux/module.h>
#include <linux/smp_lock.h>
#include "../symsearch/symsearch.h"

/* Bravo and Defy Froyo Kernel
c0035ef0 T __switch_to
c0039d80 T arm_syscall
*/

/* Defy GB Kernel
c0035ef0 T __switch_to
c0039dc0 T arm_syscall
*/

SYMSEARCH_DECLARE_ADDRESS_STATIC(__switch_to);
SYMSEARCH_DECLARE_ADDRESS_STATIC(arm_syscall);

static int __init tls_enable_init(void)
{
    SYMSEARCH_BIND_ADDRESS(tls_enable, __switch_to);
    SYMSEARCH_BIND_ADDRESS(tls_enable, arm_syscall);

    unsigned char *instr;
    unsigned long func = SYMSEARCH_GET_ADDRESS(__switch_to);
    unsigned long func2 = SYMSEARCH_GET_ADDRESS(arm_syscall);

    printk(KERN_INFO "TLS-enable init\n");
    lock_kernel();

/*
    patch __switch_to (arch/arm/kernel/entry_arm.S)

    c0030f00: -e3e04a0f mvn  r4, #61440    ; 0xf000
    c0030f04: -e504300f str  r3, [r4, #-15]              @ TLS val at 0xffff0ff0
    c0030f00: +ee0d3f70 mcr  15, 0, r3, cr13, cr0, {3}   @ set TLS register
    c0030f04: +e320f000 nop  {0}
*/

    instr = func + 0x10; // __switch_to + 0x10
    instr[3] = 0xee;
    instr[2] = 0x0d;
    instr[1] = 0x3f;
    instr[0] = 0x70;
    instr[7] = 0xe3;
    instr[6] = 0x20;
    instr[5] = 0xf0;
    instr[4] = 0x00;

/*
    patch arm_syscall (arch/arm/kernel/traps.c)
    
    c0034d64: -e3e02a0f mvn r2, #61440    ; 0xf000
    c0034d68: -e502300f str r3, [r2, #-15]              @ TLS val at 0xffff0ff0
    c0034d64: +ee0d3f70 mcr 15, 0, r3, cr13, cr0, {3}   @ set TLS register
    c0034d68: +e320f000 nop {0}
*/

    instr = func2 + 0x1cc ;//arm_syscall + 0x1cc;
    instr[3] = 0xee;
    instr[2] = 0x0d;
    instr[1] = 0x3f;
    instr[0] = 0x70;
    instr[7] = 0xe3;
    instr[6] = 0x20;
    instr[5] = 0xf0;
    instr[4] = 0x00;

/*
    patch __kuser_get_tls (arch/arm/kernel/entry_arm.S)

    ffff0fe0: -e59f0008 ldr r0, [pc, #8]    ; 50 <__kuser_get_tls+0x10> @ TLS stored at 0xffff0ff0
    ffff0fe0: +ee1d0f70 mrc 15, 0, r0, cr13, cr0, {3}                   @ read TLS register
*/

    instr = 0xffff0fe0; // __kuser_get_tls (fixed relocated addr, not from kallsyms!)
    instr[3] = 0xee;
    instr[2] = 0x1d;
    instr[1] = 0x0f;
    instr[0] = 0x70;

    // store the currently set value in TLS register
    asm ("mov r4, #0xffff0fff;"
         "ldr r3, [r4, #-15];"
         "mcr p15, 0, r3, c13, c0, 3;");

    unlock_kernel();
    return 0;
}

static void __exit tls_enable_exit(void)
{
}

module_init(tls_enable_init);
module_exit(tls_enable_exit);

MODULE_ALIAS("TLS-enable");
MODULE_DESCRIPTION("enable usage of TLS register");
MODULE_AUTHOR("Nadlabak");
MODULE_LICENSE("GPL");
