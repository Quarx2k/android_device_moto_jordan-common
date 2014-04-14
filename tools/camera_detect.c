/*
 * Copyright (C) 2014 Quarx2k (agent00791@gmail.com)
 * camera_detect.c: Simple program to detect type of camera on Motorola Defy.
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

#include <fcntl.h>
#include <stdbool.h> 
#include <cutils/properties.h>
#include <sys/ioctl.h>
#include <linux/videodev2.h>

static bool deviceCardMatches(const char *device, const char *matchCard)
{
    struct v4l2_capability caps;
    int fd = open(device, O_RDWR);
    bool ret;

    if (fd < 0) {
        return false;
    }

    if (ioctl(fd, VIDIOC_QUERYCAP, &caps) < 0) {
        ret = false;
    } else {
        const char *card = (const char *) caps.card;
        ret = strstr(card, matchCard) != NULL;
    }

    close(fd);

    return ret;
}

int main(int argc, char **argv) {
    if (deviceCardMatches("/dev/video3", "camise")) {
	property_set("ro.camera.type", "camise");
    } else if (deviceCardMatches("/dev/video0", "mt9p012")) {
	property_set("ro.camera.type", "mt9p012");
    } else {
	property_set("ro.camera.type", "Unknown");
    }
	return 0;
}

