/*
 * Copyright (C) 2008 The Android Open Source Project
 * Copyright (C) 2011 Sorin P. <sorin@hypermagik.com>
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

#include <fcntl.h>
#include <errno.h>
#include <math.h>
#include <poll.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/select.h>

//#define LOG_NDEBUG 0
#include <cutils/log.h>

#include "kernel/isl29030.h"

#include "SensorISL29030Combined.h"

#define TAG "ISL29030"

/*****************************************************************************/

SensorISL29030::SensorISL29030()
  : SensorBase(ISL29030_DEVICE_NAME, "light-prox"),
    mEnabled(0),
    mPendingMask(0),
    mInputReader(32)
{
    memset(mPendingEvents, 0, sizeof(mPendingEvents));

    mPendingEvents[Proximity].version = sizeof(sensors_event_t);
    mPendingEvents[Proximity].sensor = SENSOR_TYPE_PROXIMITY;
    mPendingEvents[Proximity].type = SENSOR_TYPE_PROXIMITY;

    mPendingEvents[Light].version = sizeof(sensors_event_t);
    mPendingEvents[Light].sensor = SENSOR_TYPE_LIGHT;
    mPendingEvents[Light].type = SENSOR_TYPE_LIGHT;

    openDevice();
    for (int i = 0; i < numSensors; i++) {
        bool enabled;
        if (isEnabled(i, enabled) == 0 && enabled) {
            mEnabled |= (1 << i);
        }
    }
    if (!mEnabled) {
        closeDevice();
    }
}

SensorISL29030::~SensorISL29030()
{
}

int SensorISL29030::enable(int32_t handle, int en)
{
    int what;
    char newState = en ? 1 : 0;
    int err = 0;

    switch (handle) {
        case SENSOR_TYPE_PROXIMITY: what = Proximity; break;
        case SENSOR_TYPE_LIGHT:     what = Light; break;
        default: return -EINVAL;
    }

    if (!mEnabled) {
        openDevice();
    }

    if ((uint32_t(newState) << what) != (mEnabled & (1 << what))) {
        int cmd;

        switch (what) {
            case Proximity: cmd = ISL29030_IOCTL_SET_ENABLE; break;
            case Light:     cmd = ISL29030_IOCTL_SET_LIGHT_ENABLE; break;
        }

        err = ioctl(dev_fd, cmd, &newState);
        err = err < 0 ? -errno : 0;

        LOGE_IF(err, TAG ": ISL29030_IOCTL_SET_XXX_ENABLE failed (%s)", strerror(-err));

        if (!err) {
            mEnabled &= ~(1 << what);
            mEnabled |= (uint32_t(newState) << what);
        }
    }

    if (!mEnabled) {
        closeDevice();
    }

    return err;
}

int SensorISL29030::readEvents(sensors_event_t* data, int count)
{
    if (count < 1) {
        return -EINVAL;
    }

    ssize_t n = mInputReader.fill(data_fd);
    if (n < 0) {
        return n;
    }

    int numEventReceived = 0;
    input_event const* event;

    while (count && mInputReader.readEvent(&event)) {
        if (event->type == EV_ABS || event->type == EV_LED) {
            processEvent(event->code, event->value);
            mInputReader.next();
        } else if (event->type == EV_SYN) {
            int64_t time = timevalToNano(event->time);
            for (int i = 0 ; count && mPendingMask && i < numSensors ; i++) {
                if (mPendingMask & (1 << i)) {
                    mPendingMask &= ~(1 << i);
                    mPendingEvents[i].timestamp = time;
                    if (mEnabled & (1 << i)) {
                        *data++ = mPendingEvents[i];
                        count--;
                        numEventReceived++;
                    }
                }
            }
            if (!mPendingMask) {
                mInputReader.next();
            }
        } else {
            LOGW(TAG "unknown event (type=0x%x, code=0x%x, value=0x%x)",
                    event->type, event->code, event->value);
            mInputReader.next();
        }
    }

    return numEventReceived;
}

void SensorISL29030::processEvent(int code, int value)
{
    switch (code) {
        case ABS_DISTANCE:
            LOGD(TAG "proximity event (code=0x%x, value=0x%x)", code, value);
            mPendingEvents[Proximity].distance = (value == PROXIMITY_NEAR ? 0 : 100);
            mPendingMask |= 1 << Proximity;
            break;
        case LED_MISC:
            mPendingEvents[Light].light = value;
            mPendingMask |= 1 << Light;
            break;
        default:
            LOGW(TAG "unknown code (code=0x%x, value=0x%x)", code, value);
            break;
    }
}

int SensorISL29030::isEnabled(int what, bool& enabled)
{
    int err = 0, cmd;
    char flag = 0;

    switch (what) {
        case Proximity: cmd = ISL29030_IOCTL_GET_ENABLE; break;
        case Light:     cmd = ISL29030_IOCTL_GET_LIGHT_ENABLE; break;
        default:        return -EINVAL;
    }

    err = ioctl(dev_fd, cmd, &flag);
    err = err < 0 ? -errno : 0;
    enabled = flag != 0;

    LOGE_IF(err, TAG "P: ISL29030_IOCTL_GET_ENABLE failed (%s)", strerror(-err));

    return err;
}

/*****************************************************************************/
