/*
 *   mot_boot_mode - Check phone bootup mode, which will be
 *                   executed in init.rc to decide what kind
 *                   of services should be started during phone
 *                   powerup.
 *
 *   Copyright Motorola 2009
 *
 *   Date         Author      Comment
 *   01/03/2009   Motorola    Creat initial version
 *   17/04/2009   Motorola    Support Charge Only Mode
 *   20/09/2011   D.Baumann   Integer modes
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <ctype.h>

#include <cutils/properties.h>
#include <cutils/log.h>

#define MOTO_PU_REASON_CHARGER   0x00000100
#define MOTO_PU_REASON_USB_CABLE 0x00000010

/********************************************************************
 * Check POWERUPREASON, decide phone powerup to charge only mode or not
 * Return value:
 * Type: int
 * 1: charge_only_mode
 * 0: NOT charge_only_mode
 ********************************************************************/
int boot_reason_charge_only(void)
{
    char data[1024], powerup_reason[32];
    int fd, n;
    char *x, *pwrup_rsn;
    unsigned long reason = 0;

    fd = open("/proc/bootinfo", O_RDONLY);
    if (fd < 0) return 0;

    n = read(fd, data, 1023);
    close(fd);
    if (n < 0) return 0;

    data[n] = '\0';

    memset(powerup_reason, 0, 32);

    pwrup_rsn = strstr(data, "POWERUPREASON");
    ALOGD("MOTO : pwr_rsn = %s\n", pwrup_rsn);
    if (pwrup_rsn) {
        x = strstr(pwrup_rsn, ": ");
        if (x) {
            x += 2;
            n = 0;
            while (*x && !isspace(*x)) {
                powerup_reason[n++] = *x;
                x++;
                if (n == 31) break;
            }
            powerup_reason[n] = '\0';
            ALOGD("MOTO_PUPD: powerup_reason=%s\n", powerup_reason);
            reason = strtoul(powerup_reason, NULL, 0);
        }
    }

    return reason == MOTO_PU_REASON_CHARGER;
    //  || reason == MOTO_PU_REASON_USB_CABLE;
}

/********************************************************************
 * Check boot reason is charge only mode or not, set property
 ********************************************************************/
int main(int argc, char **argv)
{    
    ALOGD("MOTO_PUPD: mot_boot_mode\n");

    if (boot_reason_charge_only()) {
    	ALOGD("MOTO_PUPD: boot_reason_charge_only: 1\n");
        property_set("sys.chargeonly.mode", "1");
    }

    return 0;
}
