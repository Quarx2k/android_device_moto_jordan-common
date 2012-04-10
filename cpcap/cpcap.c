#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <linux/spi/cpcap.h>

#include "cpcap.h"

static int cpcap_fd = -1;
static struct cpcap_adc_us_request req_us;

#define FMT_HEX "0x%04x"
#define FMT_DEC "%5d"

#define FMT_PRINTF " %d: " FMT_DEC

/**
 * bank   int : enum cpcap_adc_type
 *
 *        CPCAP_ADC_TYPE_BANK_0
 *        CPCAP_ADC_TYPE_BANK_1
 *        CPCAP_ADC_TYPE_BATT_PI
 *
 * format int : enum cpcap_adc_format
 *
 *        CPCAP_ADC_FORMAT_RAW
 *        CPCAP_ADC_FORMAT_PHASED
 *        CPCAP_ADC_FORMAT_CONVERTED
 */
int read_bank(int bank, int format) {

    int ret;

    memset(&req_us, 0, sizeof(struct cpcap_adc_us_request));

    req_us.timing = CPCAP_ADC_TIMING_IMM;
    req_us.format = format;
    req_us.type   = bank;

    ret = ioctl(cpcap_fd, CPCAP_IOCTL_BATT_ATOD_SYNC, &req_us);
    if (ret != 0) {
        fprintf(stderr, "ioctl failed, ret=%d errno=%d", ret, errno);
    }
    return ret;
}


/**
 * start/stop cpcap macros
 */
int macro_uc(int macro, int enable) {
    int ret;

    int cpcap_uc = open("/dev/cpcap_uc", O_RDWR | O_NONBLOCK);
    if (cpcap_uc <= 0) {
        fprintf(stderr, "%s: failed, errno=%d\n", __func__, errno);
        return -errno;
    }

    if (enable)
        ret = ioctl(cpcap_uc, CPCAP_IOCTL_UC_MACRO_START, macro);
    else
        ret = ioctl(cpcap_uc, CPCAP_IOCTL_UC_MACRO_STOP, macro);

    if (ret != 0) {
        fprintf(stderr, "%s: ioctl failed, ret=%d errno=%d\n", __func__, ret, errno);
        return ret;
    }

    close(cpcap_uc);

    return ret;
}


/**
 * Read battery data, doesnt works (blank, no error)
 */
int read_batt_ps() {

    int ret;
#ifdef OMAP_COMPAT
    struct cpcap_batt_data_mb525 batt_state;
#else
    struct cpcap_batt_data batt_state;
#endif

    memset(&batt_state, 0, sizeof(batt_state));

    ret = ioctl(cpcap_fd, CPCAP_IOCTL_BATT_DISPLAY_UPDATE, &batt_state);
    if (ret != 0) {
        fprintf(stderr, "%s: ioctl failed, ret=%d errno=%d\n", __func__, ret, errno);
        return ret;
    }

    printf("batt_state :\n");
    printf(" status=%d\n", batt_state.status);
    printf(" health=%d\n", batt_state.health);
    printf(" present=%d\n", batt_state.present);
    printf(" capacity=%d\n", batt_state.capacity);
    printf(" batt_volt=%d\n", batt_state.batt_volt);
    printf(" batt_temp=%d\n", batt_state.batt_temp);
#ifdef OMAP_COMPAT
    printf(" batt_full_capacity=%d\n", batt_state.batt_full_capacity);
    printf(" batt_capacity_one=%d\n", batt_state.batt_capacity_one);
#endif
    return ret;
}


/**
 * Program entry point
 */
int main(int argc, char **argv) {

    int i, ret;

    if (argc < 3) {
        printf("Usage: cpcap command [option]\n");
        printf("\n");
        printf("commands:\n");
        printf("\n");
        printf(" volt <source>  Get current voltage (mV)\n");
        printf("       source : batt/vbus/battpi\n");
        printf("\n");
        printf(" dump <format>  Dump ADC registers\n");
        printf("       format : all/raw/phase/convert\n");
        return 1;
    }

    cpcap_fd = open("/dev/cpcap_batt", O_RDONLY | O_NONBLOCK);
    if (cpcap_fd <= 0) {
        return errno;
    }


    /* dump raw */
    if (!strcmp(argv[1], "dump")
        && (!strcmp(argv[2], "raw") || !strcmp(argv[2], "all")))
    {
        printf("using format CPCAP_ADC_FORMAT_RAW\n");

        ret = read_bank(CPCAP_ADC_TYPE_BANK_0, CPCAP_ADC_FORMAT_RAW);
        if (ret == 0) {
            printf("CPCAP_ADC_TYPE_BANK_0  :");
            for (i = 0; i < CPCAP_ADC_BANK0_NUM; i++) {
                printf(FMT_PRINTF, i, req_us.result[i]);
            }
            printf("\n");
        }

        ret = read_bank(CPCAP_ADC_TYPE_BANK_1, CPCAP_ADC_FORMAT_RAW);
        if (ret == 0) {
            printf("CPCAP_ADC_TYPE_BANK_1  :");
            for (i = 0; i < CPCAP_ADC_BANK1_NUM; i++) {
                printf(FMT_PRINTF, i, req_us.result[i]);
            }
            printf("\n");
        }

        ret = read_bank(CPCAP_ADC_TYPE_BATT_PI, CPCAP_ADC_FORMAT_RAW);
        if (ret == 0) {
            printf("CPCAP_ADC_TYPE_BATT_PI :");
            for (i = 0; i < CPCAP_ADC_BANK0_NUM; i++) {
                printf(FMT_PRINTF, i, req_us.result[i]);
            }
            printf("\n");
        }
    }

    /* dump phase */
    if (!strcmp(argv[1], "dump")
        && (!strcmp(argv[2], "phase") || !strcmp(argv[2], "all")))
    {
        printf("using format CPCAP_ADC_FORMAT_PHASED\n");

        ret = read_bank(CPCAP_ADC_TYPE_BANK_0, CPCAP_ADC_FORMAT_PHASED);
        if (ret == 0) {
            printf("CPCAP_ADC_TYPE_BANK_0  :");
            for (i = 0; i < CPCAP_ADC_BANK0_NUM; i++) {
                printf(FMT_PRINTF, i, req_us.result[i]);
            }
            printf("\n");
        }

        ret = read_bank(CPCAP_ADC_TYPE_BANK_1, CPCAP_ADC_FORMAT_PHASED);
        if (ret == 0) {
            printf("CPCAP_ADC_TYPE_BANK_1  :");
            for (i = 0; i < CPCAP_ADC_BANK1_NUM; i++) {
                printf(FMT_PRINTF, i, req_us.result[i]);
            }
            printf("\n");
        }

        ret = read_bank(CPCAP_ADC_TYPE_BATT_PI, CPCAP_ADC_FORMAT_PHASED);
        if (ret == 0) {
            printf("CPCAP_ADC_TYPE_BATT_PI :");
            for (i = 0; i < CPCAP_ADC_BANK0_NUM; i++) {
                printf(FMT_PRINTF, i, req_us.result[i]);
            }
            printf("\n");
        }
    }

    /* dump convert */
    if (!strcmp(argv[1], "dump")
        && (!strcmp(argv[2], "convert") || !strcmp(argv[2], "all")))
    {
        printf("using format CPCAP_ADC_FORMAT_CONVERTED\n");

        ret = read_bank(CPCAP_ADC_TYPE_BANK_0, CPCAP_ADC_FORMAT_CONVERTED);
        if (ret == 0) {
            printf("CPCAP_ADC_TYPE_BANK_0  :");
            for (i = 0; i < CPCAP_ADC_BANK0_NUM; i++) {
                printf(FMT_PRINTF, i, req_us.result[i]);
            }
            printf("\n");
        }

        ret = read_bank(CPCAP_ADC_TYPE_BANK_1, CPCAP_ADC_FORMAT_CONVERTED);
        if (ret == 0) {
            printf("CPCAP_ADC_TYPE_BANK_1  :");
            for (i = 0; i < CPCAP_ADC_BANK1_NUM; i++) {
                printf(FMT_PRINTF, i, req_us.result[i]);
            }
            printf("\n");
        }

        ret = read_bank(CPCAP_ADC_TYPE_BATT_PI, CPCAP_ADC_FORMAT_CONVERTED);
        if (ret == 0) {
            printf("CPCAP_ADC_TYPE_BATT_PI :");
            for (i = 0; i < CPCAP_ADC_BANK0_NUM; i++) {
                printf(FMT_PRINTF, i, req_us.result[i]);
            }
            printf("\n");
        }
    }

    /* volt batt : battery voltage */
    if (!strcmp(argv[1], "volt") && !strcmp(argv[2], "batt"))
    {
        ret = read_bank(CPCAP_ADC_TYPE_BANK_0, CPCAP_ADC_FORMAT_CONVERTED);
        printf("%d\n", req_us.result[CPCAP_ADC_BATTP]);
    }

    /* volt vbus : +5V line (4400 or more on ac/usb, else 4000 to 4400) */
    if (!strcmp(argv[1], "volt") && !strcmp(argv[2], "vbus"))
    {
        ret = read_bank(CPCAP_ADC_TYPE_BANK_0, CPCAP_ADC_FORMAT_CONVERTED);
        printf("%d\n", req_us.result[CPCAP_ADC_VBUS]);
    }

    /* volt battpi : battery voltage based on 4 (last?) values */
    if (!strcmp(argv[1], "volt") && !strcmp(argv[2], "battpi"))
    {
        int tot = 0;
        ret = read_bank(CPCAP_ADC_TYPE_BATT_PI, CPCAP_ADC_FORMAT_CONVERTED);
        for (i = 0; i < 8; i+=2) {
            tot += req_us.result[i];
        }
        printf("%d\n", tot / 4);
    }


    // buggy, all is zero and breaks system battery report
    if (!strcmp(argv[1], "batt") && !strcmp(argv[2], "data")) {
        read_batt_ps();
    }

    if (!strcmp(argv[1], "macro") && argc >= 4) {
        int macro  = atoi(argv[2]);
        int enable = atoi(argv[3]);

        printf("%s macro %d...\n", enable?"start":"stop", macro);
        ret = macro_uc(macro, enable);
    }

    close(cpcap_fd);

    return 0;
}

