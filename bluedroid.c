/*
 * Copyright (C) 2008 The Android Open Source Project
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

#define LOG_TAG "bluedroid"

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <cutils/log.h>
#include <cutils/properties.h>

#include <bluetooth/bluetooth.h>
#include <bluetooth/hci.h>
#include <bluetooth/hci_lib.h>

#include <bluedroid/bluetooth.h>

#ifndef HCI_DEV_ID
#define HCI_DEV_ID 0
#endif

#define HCID_START_DELAY_SEC   3
#define HCID_STOP_DELAY_USEC 500000

#define MIN(x,y) (((x)<(y))?(x):(y))

static int rfkill_id = -1;
static char *rfkill_state_path = NULL;

static int init_rfkill() {
    char path[64];
    char buf[16];
    int fd;
    int sz;
    int id;
    for (id = 0; ; id++) {
        snprintf(path, sizeof(path), "/sys/class/rfkill/rfkill%d/type", id);
        fd = open(path, O_RDONLY);
        if (fd < 0) {
            ALOGW("open(%s) failed: %s (%d)\n", path, strerror(errno), errno);
            return -1;
        }
        sz = read(fd, &buf, sizeof(buf));
        close(fd);
        if (sz >= 9 && memcmp(buf, "bluetooth", 9) == 0) {
            rfkill_id = id;
            break;
        }
    }

    asprintf(&rfkill_state_path, "/sys/class/rfkill/rfkill%d/state", rfkill_id);
    return 0;
}

static int check_bluetooth_power() {
    int sz;
    int fd = -1;
    int ret = -1;
    char buffer;

    if (rfkill_id == -1) {
        if (init_rfkill()) goto out;
    }

    fd = open(rfkill_state_path, O_RDONLY);
    if (fd < 0) {
        ALOGE("open(%s) failed: %s (%d)", rfkill_state_path, strerror(errno),
             errno);
        goto out;
    }
    sz = read(fd, &buffer, 1);
    if (sz != 1) {
        ALOGE("read(%s) failed: %s (%d)", rfkill_state_path, strerror(errno),
             errno);
        goto out;
    }

    switch (buffer) {
    case '1':
        ret = 1;
        break;
    case '0':
        ret = 0;
        break;
    }

out:
    if (fd >= 0) close(fd);
    return ret;
}

static int set_bluetooth_power(int on) {
    int sz;
    int fd = -1;
    int ret = -1;
    const char buffer = (on ? '1' : '0');

    if (rfkill_id == -1) {
        if (init_rfkill()) goto out;
    }

    fd = open(rfkill_state_path, O_WRONLY);
    if (fd < 0) {
        ALOGE("open(%s) for write failed: %s (%d)", rfkill_state_path,
             strerror(errno), errno);
        goto out;
    }
    sz = write(fd, &buffer, 1);
    if (sz < 0) {
        ALOGE("write(%s) failed: %s (%d)", rfkill_state_path, strerror(errno),
             errno);
        goto out;
    }
    ret = 0;

out:
    if (fd >= 0) close(fd);
    return ret;
}

static inline int create_hci_sock() {
    int sk = socket(AF_BLUETOOTH, SOCK_RAW, BTPROTO_HCI);
    if (sk < 0) {
        ALOGE("Failed to create bluetooth hci socket: %s (%d)",
             strerror(errno), errno);
    }
    return sk;
}

struct RefBase_s {
    unsigned int magic;
    unsigned int base;
};

static const char const * REF_FILE = "/tmp/bluedroid_ref";
static const unsigned int MAGIC = 0x55665566;

static int inc_atomic()
{
    struct RefBase_s ref = {magic : MAGIC, base : 0};
    /*l_type l_whence l_start l_len l_pid*/
    struct flock fl = {F_RDLCK,    SEEK_SET,  0,        0,      0};
    int fd = -1;
    int ret = -1;
    int magic = MAGIC;

    ALOGV("Enter inc_atomic");

    if ((fd = open(REF_FILE, O_RDWR | O_CREAT, S_IRWXU | S_IRWXG | S_IRWXO)) == -1)
    {
        ALOGW("btdroid warning: open or create bluedroid_ref error");
        ret = -2;
        goto out;
    }

    ALOGV("Trying to get lock in inc_atomic....");

    if (fcntl(fd, F_SETLKW, &fl) == -1)
    {
        ALOGW("btdroid waring: F_SETLKW failed");
        ret = -1;
        goto out;
    }

    ALOGV("got lock");

    if (read(fd, (void *)&ref, sizeof(ref)) == -1)
    {
        ALOGW("btdroid waring: read reference count error");
        ret = -1;
        goto out;
    }

    if(ref.magic != MAGIC)
    {
        ALOGV("btdroid reference file magic number is 0x5566");
        ref.magic = MAGIC;
        ref.base = 0;
    }

    if(ref.base > 3) ALOGW("btdroid waring: btdroid reference count > 3 base = %d", ref.base);

    ref.base += 1;

    fl.l_type = F_UNLCK;

    if (fcntl(fd, F_SETLK, &fl) == -1)
    {
        ALOGW("btdroid waring: F_SETLK : F_UNLCK failed");
        ret = -1;
        goto out;
    }

    fl.l_type = F_WRLCK;

    if (fcntl(fd, F_SETLKW, &fl) == -1)
    {
        ALOGW("btdroid waring: F_SETLKW : F_WRLCK failed");
        ret = -1;
        goto out;
    }

    lseek(fd, 0, SEEK_SET);

    if (write(fd, (const void*)&ref, sizeof(ref)) == -1)
    {
        ALOGW("btdroid waring: write reference count failed");
        ret = -1;
        goto out;
    }

    ret = ref.base;
    ALOGV("Exit inc_atomic ref = %d", ret);
out:
    if(fd != -1) close(fd);
    return ret;
}

static int dec_atomic()
{
    struct RefBase_s ref = {magic : MAGIC, base : 0};
    /*l_type l_whence l_start l_len l_pid*/
    struct flock fl = {F_RDLCK,    SEEK_SET,  0,        0,      0};
    int fd = -1;
    int ret = -1;
    int magic = MAGIC;

    ALOGV("Enter dec_atomic");

    if ((fd = open(REF_FILE, O_RDWR)) == -1)
    {
        ALOGW("btdroid warning: open or create bluedroid_ref error");
        ret = -2;
        goto out;
    }

    ALOGV("Trying to get lock in inc_atomic....");

    if (fcntl(fd, F_SETLKW, &fl) == -1)
    {
        ALOGW("btdroid waring: F_SETLKW failed");
        ret = -1;
        goto out;
    }

    ALOGV("got lock");

    if (read(fd, (void *)&ref, sizeof(ref)) == -1)
    {
        ALOGW("btdroid waring: read reference count error");
        ret = -1;
        goto out;
    }

    if(ref.magic != MAGIC)
    {
        ALOGV("btdroid reference file magic number is 0x5566");
        ref.magic = MAGIC;
        ref.base = 1;
    }

    if(ref.base == 0)
    {
        ALOGW("btdroid waring: dereference btdroid reference  base = %d", ref.base);
        ref.base = 1;
    }

    ref.base -= 1;

    fl.l_type = F_UNLCK;

    if (fcntl(fd, F_SETLK, &fl) == -1)
    {
        ALOGW("btdroid waring: F_SETLK : F_UNLCK failed");
        ret = -1;
        goto out;
    }

    fl.l_type = F_WRLCK;

    if (fcntl(fd, F_SETLKW, &fl) == -1)
    {
        ALOGW("btdroid waring: F_SETLKW : F_WRLCK failed");
        ret = -1;
        goto out;
    }

    lseek(fd, 0, SEEK_SET);

    if (write(fd, (const void*)&ref, sizeof(ref)) == -1)
    {
        ALOGW("btdroid waring: write reference count failed");
        ret = -1;
        goto out;
    }

    ret = ref.base;
    ALOGV("Exit inc_atomic ref = %d", ret);
out:
    if(fd != -1) close(fd);
    return ret;
}

int bt_hw_en() {
    ALOGV(__FUNCTION__);

    int ret = -1;
    int hci_sock = -1;
    int attempt;
    int refcount = dec_atomic();

    if (bt_is_enabled() == 1) {
        ALOGI("bt has been enabled already. inc reference count only");
        ret = 0;
        goto out;
    }

    if (set_bluetooth_power(1) < 0) {
        goto out;
    }

    ALOGI("Starting hciattach daemon");
    if (property_set("ctl.start", "hciattach") < 0) {
        ALOGE("Failed to start hciattach");
        set_bluetooth_power(0);
        goto out;
    }

    // Try for 10 seconds, this can only succeed once hciattach has sent the
    // firmware and then turned on hci device via HCIUARTSETPROTO ioctl
    hci_sock = create_hci_sock();
    if (hci_sock < 0) goto out;
    for (attempt = 1000; attempt > 0;  attempt--) {

        if (!ioctl(hci_sock, HCIDEVUP, HCI_DEV_ID)) {
            break;
        }
        usleep(10000);  // 10 ms retry delay
    }

    if (attempt == 0) {
        ALOGE("%s: Timeout waiting for HCI device to come up", __FUNCTION__);
        set_bluetooth_power(0);
        goto out;
    } 

    ret = 0;

out:
    if (hci_sock >= 0) close(hci_sock);

    if(0 == ret)
    {
        inc_atomic();
        ALOGV("after inc : reference count = %d", refcount);
    }
    return ret;
}

int bt_hw_dis()  {
    ALOGV(__FUNCTION__);

    int ret = -1;
    int refcount = dec_atomic();
    int hci_sock = -1;

    ALOGV("bt_disable() : reference count now = %d", refcount);
    if(refcount > 0)
    {
        ALOGV("bt_disable: other app using bt so just do noting");
        return 0;
    }

    usleep(HCID_STOP_DELAY_USEC);
    hci_sock = create_hci_sock();

    if (hci_sock < 0) goto out;

    ioctl(hci_sock, HCIDEVDOWN, HCI_DEV_ID);

    ALOGI("Stopping hciattach deamon");
    if (property_set("ctl.stop", "hciattach") < 0) {
        ALOGE("Error stopping hciattach");
        goto out;
    }

    if (set_bluetooth_power(0) < 0) {
        goto out;
    }
    ret = 0;

out:
    if (hci_sock >= 0) close(hci_sock);
    return ret;
}

int bt_enable() {
    ALOGV(__FUNCTION__);
    int ret = bt_hw_en();

    if (ret < 0) {
        return ret;
    }

    ALOGI("Starting bluetoothd deamon");
    if (property_set("ctl.start", "bluetoothd") < 0) {
        ALOGE("Failed to start bluetoothd");
        bt_hw_dis();
        return -1;
    }

    sleep(HCID_START_DELAY_SEC);
    return 0;
}

int bt_disable() {
    ALOGV(__FUNCTION__);

    ALOGI("Stopping bluetoothd deamon");
    if (property_set("ctl.stop", "bluetoothd") < 0) {
        ALOGE("Error stopping bluetoothd");
    }
    usleep(HCID_STOP_DELAY_USEC);

    return bt_hw_dis();
}

int bt_is_enabled() {
    ALOGV(__FUNCTION__);

    int hci_sock = -1;
    int ret = -1;
    struct hci_dev_info dev_info;

    // Check power first
    ret = check_bluetooth_power();
   // ALOGE("%s: check_bluetooth_power %d",__func__,ret);
    if (ret == -1 || ret == 0) goto out;

    ret = -1;

    // Power is on, now check if the HCI interface is up
    hci_sock = create_hci_sock();
   // ALOGE("%s: create_hci_sock %d",__func__,hci_sock);
    if (hci_sock < 0) goto out;

    dev_info.dev_id = HCI_DEV_ID;
    ret = ioctl(hci_sock, HCIGETDEVINFO, (void *)&dev_info);
   // ALOGE("%s: ioctl(hci_sock, HCIGETDEVINFO, (void *)&dev_info) %d",__func__,ret);
    if (ret < 0) {
        ret = 0;
        goto out;
    }

    if (dev_info.flags & (1 << (HCI_UP & 31))) {
        ret = 1;
    } else {
        ret = 0;
    }

out:
   // ALOGE("%s: Return ret: %d",__func__,ret);
    if (hci_sock >= 0) close(hci_sock);
    return ret;
}

int ba2str(const bdaddr_t *ba, char *str) {
    return sprintf(str, "%2.2X:%2.2X:%2.2X:%2.2X:%2.2X:%2.2X",
                ba->b[5], ba->b[4], ba->b[3], ba->b[2], ba->b[1], ba->b[0]);
}

int str2ba(const char *str, bdaddr_t *ba) {
    int i;
    for (i = 5; i >= 0; i--) {
        ba->b[i] = (uint8_t) strtoul(str, &str, 16);
        str++;
    }
    return 0;
}
