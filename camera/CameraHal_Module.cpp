/*
 * Copyright (C) Texas Instruments - http://www.ti.com/
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

/**
* @file CameraHal.cpp
*
* This file maps the Camera Hardware Interface to V4L2.
*
*/

#define LOG_TAG "****CameraHAL"

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <utils/Log.h>
#include <linux/videodev2.h>
#include "binder/MemoryBase.h"
#include "binder/MemoryHeapBase.h"
#include <camera/CameraParameters.h>
#include <hardware/camera.h>
#include <sys/ioctl.h>
#include "CameraHardware.h"
#include <binder/MemoryBase.h>
#include <binder/MemoryHeapBase.h>
//#include <utils/threads.h>
#include "V4L2Camera.h"
#define LOG_FUNCTION_NAME           ALOGV("%d: %s() ENTER", __LINE__, __FUNCTION__);

using namespace android;
static CameraHardware *V4L2CameraHardware;
static int camera_device_open(const hw_module_t* module, const char* name,
                hw_device_t** device);
static int camera_device_close(hw_device_t* device);
static int camera_get_number_of_cameras(void);
static int camera_get_camera_info(int camera_id, struct camera_info *info);
static struct hw_module_methods_t camera_module_methods = {
        open: camera_device_open
};

camera_module_t HAL_MODULE_INFO_SYM = {
    common: {
         tag: HARDWARE_MODULE_TAG,
         version_major: 1,
         version_minor: 0,
         id: CAMERA_HARDWARE_MODULE_ID,
         name: "V4L2 CameraHal Module",
         author: "Linaro",
         methods: &camera_module_methods,
         dso: NULL, /* remove compilation warnings */
         reserved: {0}, /* remove compilation warnings */
    },
    get_number_of_cameras: camera_get_number_of_cameras,
    get_camera_info: camera_get_camera_info,
};

typedef struct V4L2_camera_device {
    camera_device_t base;
    /* TI specific "private" data can go here (base.priv) */
    int cameraid;
} V4l2_camera_device_t;


/*******************************************************************
 * implementation of camera_device_ops functions
 *******************************************************************/

int camera_set_preview_window(struct camera_device * device,
        struct preview_stream_ops *window)
{
    int rv = -EINVAL;
    LOG_FUNCTION_NAME
         if(!device)
        return rv;

    if(window==NULL)
    {
        ALOGW("window is NULL");
        V4L2CameraHardware->setPreviewWindow(window);
     return -1;
    }

        V4L2CameraHardware->setPreviewWindow(window);
        ALOGD("Exiting the function");
    return 0;
}

void camera_set_callbacks(struct camera_device * device,
        camera_notify_callback notify_cb,
        camera_data_callback data_cb,
        camera_data_timestamp_callback data_cb_timestamp,
        camera_request_memory get_memory,
        void *user)
{
    V4l2_camera_device_t* V4l2_dev = NULL;
    LOG_FUNCTION_NAME
    V4L2CameraHardware->setCallbacks(notify_cb,data_cb,data_cb_timestamp,get_memory,user);
}

void camera_enable_msg_type(struct camera_device * device, int32_t msg_type)
{
    V4l2_camera_device_t* V4l2_dev = NULL;

    LOG_FUNCTION_NAME
    V4L2CameraHardware->enableMsgType(msg_type);
}

void camera_disable_msg_type(struct camera_device * device, int32_t msg_type)
{
    V4l2_camera_device_t* V4l2_dev = NULL;
    LOG_FUNCTION_NAME
    V4L2CameraHardware->disableMsgType(msg_type);
}

int camera_msg_type_enabled(struct camera_device * device, int32_t msg_type)
{
    V4l2_camera_device_t* V4l2_dev = NULL;
    LOG_FUNCTION_NAME
    return V4L2CameraHardware->msgTypeEnabled(msg_type);
}

int camera_start_preview(struct camera_device * device)
{
    LOG_FUNCTION_NAME
    return V4L2CameraHardware->startPreview();
}

void camera_stop_preview(struct camera_device * device)
{
LOG_FUNCTION_NAME
V4L2CameraHardware->stopPreview();
}

int camera_preview_enabled(struct camera_device * device)
{
    LOG_FUNCTION_NAME
    if(V4L2CameraHardware->previewEnabled())
    {
        ALOGW("----Preview Enabled----");
        return 1;
    }
    else
    {
        ALOGW("----Preview not Enabled----");
        return 0;
    }
}

int camera_store_meta_data_in_buffers(struct camera_device * device, int enable)
{
    int rv = -EINVAL;
    return rv;
}

int camera_start_recording(struct camera_device * device)
{
    return V4L2CameraHardware->startRecording();
}

void camera_stop_recording(struct camera_device * device)
{
    V4L2CameraHardware->stopRecording();
}

int camera_recording_enabled(struct camera_device * device)
{
    return V4L2CameraHardware->recordingEnabled();
}

void camera_release_recording_frame(struct camera_device * device,
                const void *opaque)
{
    return V4L2CameraHardware->releaseRecordingFrame(opaque);
}

int camera_auto_focus(struct camera_device * device)
{
    return V4L2CameraHardware->autoFocus();
}

int camera_cancel_auto_focus(struct camera_device * device)
{
    return V4L2CameraHardware->cancelAutoFocus();
}

int camera_take_picture(struct camera_device * device)
{
    return V4L2CameraHardware->takePicture();
}

int camera_cancel_picture(struct camera_device * device)
{
    int rv = 0;// -EINVAL;
    return  V4L2CameraHardware->cancelPicture();
}

int camera_set_parameters(struct camera_device * device, const char *params)
{
    CameraParameters *camParams = new CameraParameters();
    String8 *params_str8 = new String8(params);
    camParams->unflatten(*params_str8);
    return  V4L2CameraHardware->setParameters(*camParams);
}

char* camera_get_parameters(struct camera_device * device)
{
    char* param = NULL ;
    String8 params_str8 = V4L2CameraHardware->getParameters().flatten();

    // camera service frees this string...
    param = (char*) malloc(sizeof(char) * (params_str8.length()+1));
    strcpy(param, params_str8.string());
    ALOGV("%s",param);
    return param;
}

static void camera_put_parameters(struct camera_device *device, char *params)
{
    CameraParameters *camParams = new CameraParameters();
    String8 *params_str8 = new String8(params);
    camParams->unflatten(*params_str8);
    V4L2CameraHardware->setParameters(*camParams);
}

int camera_send_command(struct camera_device * device,
            int32_t cmd, int32_t arg1, int32_t arg2)
{
    int rv =0;// -EINVAL;
    return rv;
}

void camera_release(struct camera_device * device)
{
    V4L2CameraHardware->release();
}


int camera_dump(struct camera_device * device, int fd)
{
    int rv = 0;//-EINVAL;
    return rv;
}

extern "C" void heaptracker_free_leaked_memory(void);

int camera_device_close(hw_device_t* device)
{
    int ret = 0;
    delete V4L2CameraHardware;
    V4L2CameraHardware=NULL;
    return ret;
}

/*******************************************************************
 * implementation of camera_module functions
 *******************************************************************/

/* open device handle to one of the cameras
 *
 * assume camera service will keep singleton of each camera
 * so this function will always only be called once per camera instance
 */

int camera_device_open(const hw_module_t* module, const char* name,
                hw_device_t** device)
{
    int rv = 0;
    int num_cameras = 1;
    int cameraid;
    V4l2_camera_device_t* camera_device = NULL;
    camera_device_ops_t* camera_ops = NULL;

    LOG_FUNCTION_NAME


    ALOGI("camera_device open");

    if (name != NULL) {
        cameraid = atoi(name);

        if(cameraid > num_cameras)
        {
            ALOGE("camera service provided cameraid out of bounds, "
                    "cameraid = %d, num supported = %d",
                    cameraid, num_cameras);
            rv = -EINVAL;
            goto fail;
        }


        camera_device = (V4l2_camera_device_t*)malloc(sizeof(*camera_device));
        if(!camera_device)
        {
            ALOGE("camera_device allocation fail");
            rv = -ENOMEM;
            goto fail;
        }

        camera_ops = (camera_device_ops_t*)malloc(sizeof(*camera_ops));
        if(!camera_ops)
        {
            ALOGE("camera_ops allocation fail");
            rv = -ENOMEM;
            goto fail;
        }

        memset(camera_device, 0, sizeof(*camera_device));
        memset(camera_ops, 0, sizeof(*camera_ops));

        camera_device->base.common.tag = HARDWARE_DEVICE_TAG;
        camera_device->base.common.version = 0;
        camera_device->base.common.module = (hw_module_t *)(module);
        camera_device->base.common.close = camera_device_close;
        camera_device->base.ops = camera_ops;

        camera_ops->set_preview_window = camera_set_preview_window;
        camera_ops->set_callbacks = camera_set_callbacks;
        camera_ops->enable_msg_type = camera_enable_msg_type;
        camera_ops->disable_msg_type = camera_disable_msg_type;
        camera_ops->msg_type_enabled = camera_msg_type_enabled;
        camera_ops->start_preview = camera_start_preview;
        camera_ops->stop_preview = camera_stop_preview;
        camera_ops->preview_enabled = camera_preview_enabled;
        camera_ops->store_meta_data_in_buffers = camera_store_meta_data_in_buffers;
        camera_ops->start_recording = camera_start_recording;
        camera_ops->stop_recording = camera_stop_recording;
        camera_ops->recording_enabled = camera_recording_enabled;
        camera_ops->release_recording_frame = camera_release_recording_frame;
        camera_ops->auto_focus = camera_auto_focus;
        camera_ops->cancel_auto_focus = camera_cancel_auto_focus;
        camera_ops->take_picture = camera_take_picture;
        camera_ops->cancel_picture = camera_cancel_picture;
        camera_ops->set_parameters = camera_set_parameters;
        camera_ops->get_parameters = camera_get_parameters;
        camera_ops->put_parameters = camera_put_parameters;
        camera_ops->send_command = camera_send_command;
        camera_ops->release = camera_release;
        camera_ops->dump = camera_dump;

        *device = &camera_device->base.common;

        // -------- TI specific stuff --------

        camera_device->cameraid = cameraid;
        V4L2CameraHardware = new CameraHardware();
    }

    return rv;

fail:
    if(camera_device) {
        free(camera_device);
        camera_device = NULL;
    }
    if(camera_ops) {
        free(camera_ops);
        camera_ops = NULL;
    }
    *device = NULL;
    return rv;
}

int camera_get_number_of_cameras(void)
{
    int num_cameras =1;// MAX_CAMERAS_SUPPORTED;
    return num_cameras;
}

int camera_get_camera_info(int camera_id, struct camera_info *info)
{
    int rv = 0;
    int face_value = CAMERA_FACING_FRONT;
    int orientation = 0;
    const char *valstr = NULL;
    if(camera_id == 0) {
    info->facing = CAMERA_FACING_BACK;
    ALOGD("cameraHal BACK %d",camera_id);
    }
    else {
    ALOGD("cameraHal Front %d",camera_id);
    info->facing = face_value;
    }
    info->orientation = orientation;
    ALOGD("cameraHal %d",camera_id);
    return rv;
}
