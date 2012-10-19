/*
 * Copyright (C) 2012 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#define LOG_TAG "OMAP3 PowerHAL"
#include <utils/Log.h>

#include <hardware/hardware.h>
#include <hardware/power.h>

#define CPUFREQ_INTERACTIVE "/sys/devices/system/cpu/cpufreq/interactive/"
#define CPUFREQ_CPU0 "/sys/devices/system/cpu/cpu0/cpufreq/"
#define BOOSTPULSE_PATH (CPUFREQ_INTERACTIVE "boostpulse")

#define MAX_BUF_SZ  10

#define MAX_FREQ_NUMBER 10
#define NOM_FREQ_INDEX 3

static int freq_num;
static char *freq_list[MAX_FREQ_NUMBER];
static char *max_freq, *nom_freq;

struct omap3_power_module {
    struct power_module base;
    pthread_mutex_t lock;
    int boostpulse_fd;
    int boostpulse_warned;
    short inited;
};

static int str_to_tokens(char *str, char **token, int max_token_idx)
{
    char *pos, *start_pos = str;
    char *token_pos;
    int token_idx = 0;

    if (!str || !token || !max_token_idx) {
        return 0;
    }

    do {
        token_pos = strtok_r(start_pos, " \t\r\n", &pos);

        if (token_pos)
            token[token_idx++] = strdup(token_pos);
        start_pos = NULL;
    } while (token_pos && token_idx < max_token_idx);

    return token_idx;
}

static void sysfs_write(char *path, char *s)
{
    char buf[80];
    int len;
    int fd = open(path, O_WRONLY);

    if (fd < 0) {
        strerror_r(errno, buf, sizeof(buf));
        ALOGE("Error opening %s: %s\n", path, buf);
        return;
    }

    len = write(fd, s, strlen(s));
    if (len < 0) {
        strerror_r(errno, buf, sizeof(buf));
        ALOGE("Error writing to %s: %s\n", path, buf);
    }

    close(fd);
}

int sysfs_read(const char *path, char *buf, size_t size)
{
    int fd, len;

    fd = open(path, O_RDONLY);
    if (fd < 0)
        return -1;

    do {
        len = read(fd, buf, size);
    } while (len < 0 && errno == EINTR);

    close(fd);

    return len;
}

static void omap3_power_init(struct power_module *module)
{
    int tmp;
    struct omap3_power_module *powmod =
                                   (struct omap3_power_module *) module;
    char freq_buf[MAX_FREQ_NUMBER*10];

    tmp = sysfs_read(CPUFREQ_CPU0 "scaling_available_frequencies",
                                                   freq_buf, sizeof(freq_buf));
    if (tmp <= 0) {
        return;
    }

    freq_num = str_to_tokens(freq_buf, freq_list, MAX_FREQ_NUMBER);
    if (!freq_num) {
        return;
    }

    max_freq = freq_list[0];
    tmp = (NOM_FREQ_INDEX > freq_num) ? freq_num : NOM_FREQ_INDEX;
    nom_freq = freq_list[tmp - 1];

    /*
     * cpufreq interactive governor: timer 20ms, min sample 50ms,
     * hispeed nominal (3rd freq) at load 50%.
     */

    sysfs_write(CPUFREQ_INTERACTIVE "timer_rate", "20000");
    sysfs_write(CPUFREQ_INTERACTIVE "min_sample_time", "50000");
    sysfs_write(CPUFREQ_INTERACTIVE "hispeed_freq", nom_freq);
    sysfs_write(CPUFREQ_INTERACTIVE "go_hispeed_load", "50");
    sysfs_write(CPUFREQ_INTERACTIVE "above_hispeed_delay", "100000");
    sysfs_write(CPUFREQ_INTERACTIVE "input_boost", "1");

    powmod->inited = 1;
}

static int boostpulse_open(struct omap3_power_module *omap3)
{
    char buf[80];

    pthread_mutex_lock(&omap3->lock);

    if (omap3->boostpulse_fd < 0) {
        omap3->boostpulse_fd = open(BOOSTPULSE_PATH, O_WRONLY);

        if (omap3->boostpulse_fd < 0) {
            if (!omap3->boostpulse_warned) {
                strerror_r(errno, buf, sizeof(buf));
                ALOGE("Error opening %s: %s\n", BOOSTPULSE_PATH, buf);
                omap3->boostpulse_warned = 1;
            }
        }
    }

    pthread_mutex_unlock(&omap3->lock);
    return omap3->boostpulse_fd;
}

static void omap3_power_set_interactive(struct power_module *module, int on)
{
    int len;
    char buf[MAX_BUF_SZ];
    struct omap3_power_module *powmod =
                                   (struct omap3_power_module *) module;

    if (!powmod->inited) {
        return;
    }

    /*
     * Lower maximum frequency when screen is off.  
     */
    sysfs_write(CPUFREQ_CPU0 "scaling_max_freq", on ? max_freq : nom_freq);

    sysfs_write(CPUFREQ_INTERACTIVE "input_boost", on ? "1" : "0");
}

static void omap3_power_hint(struct power_module *module, power_hint_t hint,
                            void *data)
{
    struct omap3_power_module *omap3 = (struct omap3_power_module *) module;
    char buf[80];
    int len;
    struct omap3_power_module *powmod =
                                   (struct omap3_power_module *) module;

    if (!powmod->inited) {
        return;
    }

    switch (hint) {
    case POWER_HINT_INTERACTION:
        if (boostpulse_open(omap3) >= 0) {
	    len = write(omap3->boostpulse_fd, "1", 1);

	    if (len < 0) {
	        strerror_r(errno, buf, sizeof(buf));
		ALOGE("Error writing to %s: %s\n", BOOSTPULSE_PATH, buf);
	    }
	}
        break;

    case POWER_HINT_VSYNC:
        break;

    default:
        break;
    }
}

static struct hw_module_methods_t power_module_methods = {
    .open = NULL,
};

struct omap3_power_module HAL_MODULE_INFO_SYM = {
    base: {
        common: {
            tag: HARDWARE_MODULE_TAG,
            module_api_version: POWER_MODULE_API_VERSION_0_2,
            hal_api_version: HARDWARE_HAL_API_VERSION,
            id: POWER_HARDWARE_MODULE_ID,
            name: "OMAP3 Power HAL",
            author: "The Android Open Source Project",
            methods: &power_module_methods,
        },

       init: omap3_power_init,
       setInteractive: omap3_power_set_interactive,
       powerHint: omap3_power_hint,
    },

    lock: PTHREAD_MUTEX_INITIALIZER,
    boostpulse_fd: -1,
    boostpulse_warned: 0,
};
