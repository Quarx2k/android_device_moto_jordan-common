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

#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <private/android_filesystem_config.h>

void board_reboot_hook(const char *reason, int *need_clear_reason)
{
    int fd;

    if (reason != NULL && strncmp(reason, "bppanic", 7) == 0) {
        *need_clear_reason = 1;
        return;
    }

    /* force SIM PIN check after reboot */
    fd = open("/data/simpin", O_WRONLY | O_CREAT, 0660);
    if (fd >= 0) {
        int ret;
        do {
            ret = write(fd, "lock", 4);
        } while (ret < 0 && errno == EINTR);
        fchown(fd, -1, AID_SYSTEM);
        fchmod(fd, 0660);
        close(fd);
    }
}
