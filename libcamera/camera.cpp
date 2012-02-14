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

/**
* @file camera.cpp
*/

#define LOG_TAG "CameraHAL"

#define MAX_CAMERAS_SUPPORTED 1

#define DUMP_PREVIEW_FRAMES

#include <fcntl.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>

#include <cutils/log.h>
#include <ui/Overlay.h>
#include <camera/CameraParameters.h>
#include <hardware/camera.h>
#include "CameraHardwareInterface.h"

using android::sp;
using android::Overlay;
using android::String8;
using android::IMemory;
using android::IMemoryHeap;
using android::CameraParameters;

using android::CameraInfo;
using android::HAL_openCameraHardware;
using android::CameraHardwareInterface;

android::CameraParameters camParams;
android::String8 params_str8;

static sp<CameraHardwareInterface> gCameraHals[MAX_CAMERAS_SUPPORTED];

static unsigned int gCamerasOpen = 0;
//static android::Mutex gCameraDeviceLock;

static int camera_device_open(const hw_module_t* module, const char* name,
                hw_device_t** device);
static int camera_device_close(hw_device_t* device);
int CameraHAL_GetNum_Cameras(void);
int CameraHAL_GetCam_Info(int camera_id, struct camera_info *info);
static struct hw_module_methods_t camera_module_methods = {
        open: camera_device_open
};

camera_module_t HAL_MODULE_INFO_SYM = {
    common: {
         tag: HARDWARE_MODULE_TAG,
         version_major: 1,
         version_minor: 0,
         id: CAMERA_HARDWARE_MODULE_ID,
         name: "Jordan CameraHal Module",
         author: "Zhibin Wu, Quarx",
         methods: &camera_module_methods,
         dso: NULL, /* remove compilation warnings */
         reserved: {0}, /* remove compilation warnings */
    },
    get_number_of_cameras: CameraHAL_GetNum_Cameras,
    get_camera_info: CameraHAL_GetCam_Info,
};

typedef struct priv_camera_device {
    camera_device_t base;
    /* specific "private" data can go here (base.priv) */
    int cameraid;
    /* new world */
    preview_stream_ops *window;
    camera_notify_callback notify_callback;
    camera_data_callback data_callback;
    camera_data_timestamp_callback data_timestamp_callback;
    camera_request_memory request_memory;
    void *user;
    /* old world*/
    int preview_width;
    int preview_height;
    sp<Overlay> overlay;
    gralloc_module_t const *gralloc;
} priv_camera_device_t;


static struct {
    int type;
    const char *text;
} msg_map[] = {
    {0x0001, "CAMERA_MSG_ERROR"},
    {0x0002, "CAMERA_MSG_SHUTTER"},
    {0x0004, "CAMERA_MSG_FOCUS"},
    {0x0008, "CAMERA_MSG_ZOOM"},
    {0x0010, "CAMERA_MSG_PREVIEW_FRAME"},
    {0x0020, "CAMERA_MSG_VIDEO_FRAME"},
    {0x0040, "CAMERA_MSG_POSTVIEW_FRAME"},
    {0x0080, "CAMERA_MSG_RAW_IMAGE"},
    {0x0100, "CAMERA_MSG_COMPRESSED_IMAGE"},
    {0x0200, "CAMERA_MSG_RAW_IMAGE_NOTIFY"},
    {0x0400, "CAMERA_MSG_PREVIEW_METADATA"},
    {0x0000, "CAMERA_MSG_ALL_MSGS"}, //0xFFFF
    {0x0000, "NULL"},
};

static void dump_msg(const char *tag, int msg_type)
{
    int i;
    for (i = 0; msg_map[i].type; i++) {
        if (msg_type & msg_map[i].type) {
            LOGD("%s: %s", tag, msg_map[i].text);
        }
    }
}

/*******************************************************************
 * overlay hook
 *******************************************************************/

static void wrap_set_fd_hook(void *data, int fd)
{
    priv_camera_device_t* dev = NULL;

    if(!data)
        return;

    dev = (priv_camera_device_t*) data;
    //LOGD("%s: %i", __FUNCTION__, fd);
}

static void wrap_set_crop_hook(void *data,
                               uint32_t x, uint32_t y,
                               uint32_t w, uint32_t h)
{
    priv_camera_device_t* dev = NULL;

    if(!data)
        return;

    dev = (priv_camera_device_t*) data;
    //LOGD("%s: %i %i %i %i", __FUNCTION__, x, y, w, h);
}

static void wrap_queue_buffer_hook(void *data, void* buffer)
{
    sp<IMemoryHeap> heap;
    priv_camera_device_t* dev = NULL;
    preview_stream_ops* window = NULL;

    if(!data)
        return;

    dev = (priv_camera_device_t*) data;
    window = dev->window;
    heap =  gCameraHals[dev->cameraid]->getPreviewHeap();
    int offset = (int)buffer;
    char *frame = (char *)(heap->base()) + offset;

    LOGD("%s: base:%p offset:%i frame:%p", __FUNCTION__,
         heap->base(), offset, frame);

    int stride;
    void *vaddr;
    buffer_handle_t *buf_handle;
    int tries = 5, err = 0;
    int width = dev->preview_width;
    int height = dev->preview_height;
    int framesize = (width * height * 3 / 2);
    if (0 != window->dequeue_buffer(window, &buf_handle, &stride)) {
        LOGE("%s: could not dequeue gralloc buffer", __FUNCTION__);
	goto skipframe;
    }

    err = dev->gralloc->lock(dev->gralloc, *buf_handle,
			    GRALLOC_USAGE_SW_WRITE_MASK,
			    0, 0, width, height, &vaddr);
    while (err && tries) {
        // Pano frames almost always need a retry...
        usleep(1000);
        dev->gralloc->unlock(dev->gralloc, *buf_handle);
        err = dev->gralloc->lock(dev->gralloc, *buf_handle,
                            GRALLOC_USAGE_SW_WRITE_MASK,
                            0, 0, width, height, &vaddr);
        LOGW("%s: retry gralloc lock %d", __FUNCTION__, (6 - tries));
        tries--;
    }
    if (0 == err) {
        // the code below assumes YUV, not RGB
	memcpy(vaddr, frame, framesize);
        dev->gralloc->unlock(dev->gralloc, *buf_handle);
    } else {
        LOGE("%s: could not lock gralloc buffer", __FUNCTION__);
	goto skipframe;
    }

    if (0 != window->enqueue_buffer(window, buf_handle)) {
        LOGE("%s: could not dequeue gralloc buffer", __FUNCTION__);
	goto skipframe;
    }

skipframe:

#ifdef DUMP_PREVIEW_FRAMES
    static int frameCnt = 0;
    int written;
    if (frameCnt >= 100 && frameCnt <= 109 ) {
        char path[128];
        snprintf(path, sizeof(path), "/data/%d_preview.yuv", frameCnt);
        int file_fd = open(path, O_RDWR | O_CREAT, 0666);
        LOGD("dumping preview frame %d", frameCnt);
        if (file_fd < 0) {
            LOGE("cannot open file:%s (error:%i)\n", path, file_fd);
        }
        else
        {
            LOGV("dumping data");
            written = write(file_fd, (char *)frame,
                    width * height * 3 / 2);//dev->preview_frame_size);
            if(written < 0)
                LOGE("error in data write");
        }
        close(file_fd);
    }
    frameCnt++;
#endif

    return;
}

/*******************************************************************
 * camera interface callback
 *******************************************************************/

static camera_memory_t *wrap_memory_data(priv_camera_device_t *dev,
                                    const sp<IMemory>& dataPtr)
{
    void *data;
    size_t size;
    ssize_t offset;
    sp<IMemoryHeap> heap;
    camera_memory_t *mem;

    LOGD("%s", __FUNCTION__);

    if (!dev->request_memory)
        return NULL;

    heap = dataPtr->getMemory(&offset, &size);
    data = (void *)((char *)(heap->base()) + offset);

    LOGD("%s: data: 0x%p size: %i", __FUNCTION__, data, size);

    mem = dev->request_memory(-1, size, 1, dev->user);
    memcpy(mem->data, data, size);

    return mem;
}

static void wrap_notify_callback(int32_t msg_type, int32_t ext1,
                                int32_t ext2, void* user)
{
    priv_camera_device_t* dev = NULL;

    LOGD("%s: type %i", __FUNCTION__, msg_type);
    dump_msg(__FUNCTION__, msg_type);

    if(!user)
        return;

    dev = (priv_camera_device_t*) user;

    if (dev->notify_callback)
        dev->notify_callback(msg_type, ext1, ext2, dev->user);
}

static void wrap_data_callback(int32_t msg_type, const sp<IMemory>& dataPtr,
                              void* user)
{
    camera_memory_t *data = NULL;
    priv_camera_device_t* dev = NULL;

    LOGD("%s: type %i", __FUNCTION__, msg_type);
    dump_msg(__FUNCTION__, msg_type);

    if(!user)
        return;

    dev = (priv_camera_device_t*) user;

    data = wrap_memory_data(dev, dataPtr);

    if (dev->data_callback)
        dev->data_callback(msg_type, data, 0, NULL, dev->user);
}

static void wrap_data_callback_timestamp(nsecs_t timestamp, int32_t msg_type,
                                        const sp<IMemory>& dataPtr, void* user)
{
    priv_camera_device_t* dev = NULL;

    LOGD("%s: type %i", __FUNCTION__, msg_type);
    dump_msg(__FUNCTION__, msg_type);

    if(!user)
        return;

    dev = (priv_camera_device_t*) user;
}

int
CameraHAL_GetNum_Cameras(void)
{
   LOGV("CameraHAL_GetNum_Cameras:\n");
   return 1;
}

int 
CameraHAL_GetCam_Info(int camera_id, struct camera_info *info)
{
   LOGV("CameraHAL_GetCam_Info:\n");
   info->facing      = CAMERA_FACING_BACK;
   info->orientation = 90;
   return 0;
}

void
CameraHAL_FixupParams(android::CameraParameters &settings)
{
   const char *preview_sizes =
      "640x480,576x432,480x320,384x288,352x288,320x240,240x160,176x144";
   const char *video_sizes = 
      "848x480,640x480,352x288,320x240,176x144";
   const char *preferred_size       = "480x320";
   const char *preview_frame_rates  = "25,24,15,10";
   const char *preferred_frame_rate = "15";

   const char *preview_fps_range    = "1000,25000";
   const char *preview_fps_range_values = "(1000,7000),(1000,10000),(1000,15000),(1000,20000),(1000,24000),(1000,25000)";
   const char *horizontal_view_angle= "360";
   const char *vertical_view_angle= "360";

   LOGV("CameraHAL FixupParams\n");
   settings.set(android::CameraParameters::KEY_VIDEO_FRAME_FORMAT,
                android::CameraParameters::PIXEL_FORMAT_YUV422I);

   if (!settings.get(android::CameraParameters::KEY_SUPPORTED_PREVIEW_SIZES)) {
      settings.set(android::CameraParameters::KEY_SUPPORTED_PREVIEW_SIZES,
                   preview_sizes);
   }

   if (!settings.get(android::CameraParameters::KEY_SUPPORTED_VIDEO_SIZES)) {
      settings.set(android::CameraParameters::KEY_SUPPORTED_VIDEO_SIZES,
                   video_sizes);
   }

   if (!settings.get(android::CameraParameters::KEY_VIDEO_SIZE)) {
      settings.set(android::CameraParameters::KEY_VIDEO_SIZE, preferred_size);
   }

   if (!settings.get(android::CameraParameters::KEY_PREFERRED_PREVIEW_SIZE_FOR_VIDEO)) {
      settings.set(android::CameraParameters::KEY_PREFERRED_PREVIEW_SIZE_FOR_VIDEO,
                   preferred_size);
   }

   if (!settings.get(android::CameraParameters::KEY_SUPPORTED_PREVIEW_FRAME_RATES)) {
      settings.set(android::CameraParameters::KEY_SUPPORTED_PREVIEW_FRAME_RATES,
                   preview_frame_rates);
   }

   if (!settings.get(android::CameraParameters::KEY_PREVIEW_FPS_RANGE)) {
      settings.set(android::CameraParameters::KEY_PREVIEW_FPS_RANGE,
                   preview_fps_range);
   }

   if (!settings.get(android::CameraParameters::KEY_SUPPORTED_PREVIEW_FPS_RANGE)) {
      settings.set(android::CameraParameters::KEY_SUPPORTED_PREVIEW_FPS_RANGE,
                   preview_fps_range_values);
   }

   if (!settings.get(android::CameraParameters::KEY_HORIZONTAL_VIEW_ANGLE)) {
      settings.set(android::CameraParameters::KEY_HORIZONTAL_VIEW_ANGLE,
                   horizontal_view_angle);
   }

   if (!settings.get(android::CameraParameters::KEY_VERTICAL_VIEW_ANGLE)) {
      settings.set(android::CameraParameters::KEY_VERTICAL_VIEW_ANGLE,
                   vertical_view_angle);
   }

   if (!settings.get(android::CameraParameters::KEY_PREVIEW_FRAME_RATE)) {
      settings.set(android::CameraParameters::KEY_PREVIEW_FRAME_RATE,
                   preferred_frame_rate);
   }
}


/*******************************************************************
 * implementation of priv_camera_device_ops functions
 *******************************************************************/

int camera_set_preview_window(struct camera_device * device,
        struct preview_stream_ops *window)
{
    int rv = -EINVAL;
    int min_bufs = -1;
    int kBufferCount = 4;
    priv_camera_device_t* dev = NULL;

    LOGD("%s", __FUNCTION__);

    if(!device)
        return rv;

    dev = (priv_camera_device_t*) device;

    dev->window = window;

    if (!window) {
        LOGD("%s: window is NULL", __FUNCTION__);
        return 0;
    }

    if (!dev->gralloc) {
        if (hw_get_module(GRALLOC_HARDWARE_MODULE_ID,
                (const hw_module_t **)&(dev->gralloc))) {
            LOGE("%s: Fail on loading gralloc HAL", __FUNCTION__);
        }
    }

    if (window->get_min_undequeued_buffer_count(window, &min_bufs)) {
        LOGE("%s: could not retrieve min undequeued buffer count", __FUNCTION__);
        return -1;
    }

    LOGD("%s: min_bufs:%i", __FUNCTION__, min_bufs);

    if (min_bufs >= kBufferCount) {
        LOGE("%s: min undequeued buffer count %i is too high (expecting at most %i)",
             __FUNCTION__, min_bufs, kBufferCount - 1);
    }

    LOGD("%s: setting buffer count to %i", __FUNCTION__, kBufferCount);
    if (window->set_buffer_count(window, kBufferCount)) {
        LOGE("%s: could not set buffer count", __FUNCTION__);
        return -1;
    }

    int preview_width;
    int preview_height;
    CameraParameters params(gCameraHals[dev->cameraid]->getParameters());
    params.getPreviewSize(&preview_width, &preview_height);
    int hal_pixel_format = HAL_PIXEL_FORMAT_YCbCr_422_I;
    //int hal_pixel_format = HAL_PIXEL_FORMAT_RGB_565;

    const char *str_preview_format = params.getPreviewFormat();
    LOGD("%s: preview format %s", __FUNCTION__, str_preview_format);

    if (window->set_usage(window, GRALLOC_USAGE_SW_WRITE_MASK)) {
        LOGE("%s: could not set usage on gralloc buffer", __FUNCTION__);
        return -1;
    }

    if (window->set_buffers_geometry(window, preview_width,
                                     preview_height, hal_pixel_format)) {
        LOGE("%s: could not set buffers geometry to %s",
             __FUNCTION__, str_preview_format);
        return -1;
    }

    dev->preview_width = preview_width;
    dev->preview_height = preview_height;

    dev->overlay =  new Overlay(wrap_set_fd_hook,
                                wrap_set_crop_hook,
                                wrap_queue_buffer_hook,
                                (void *)dev);
    gCameraHals[dev->cameraid]->setOverlay(dev->overlay);

    return rv;
}

void camera_set_callbacks(struct camera_device * device,
        camera_notify_callback notify_cb,
        camera_data_callback data_cb,
        camera_data_timestamp_callback data_cb_timestamp,
        camera_request_memory get_memory,
        void *user)
{
    priv_camera_device_t* dev = NULL;

    LOGD("%s", __FUNCTION__);

    if(!device)
        return;

    dev = (priv_camera_device_t*) device;

    dev->notify_callback = notify_cb;
    dev->data_callback = data_cb;
    dev->data_timestamp_callback = data_cb_timestamp;
    dev->request_memory = get_memory;
    dev->user = user;

    gCameraHals[dev->cameraid]->setCallbacks(wrap_notify_callback, wrap_data_callback,
                                             wrap_data_callback_timestamp, (void *)dev);
}

void camera_enable_msg_type(struct camera_device * device, int32_t msg_type)
{
    priv_camera_device_t* dev = NULL;

    LOGD("%s: type %i", __FUNCTION__, msg_type);
    dump_msg(__FUNCTION__, msg_type);

    if(!device)
        return;

    dev = (priv_camera_device_t*) device;

    gCameraHals[dev->cameraid]->enableMsgType(msg_type);
}

void camera_disable_msg_type(struct camera_device * device, int32_t msg_type)
{
    priv_camera_device_t* dev = NULL;

    LOGD("%s: type %i", __FUNCTION__, msg_type);
    dump_msg(__FUNCTION__, msg_type);

    if(!device)
        return;

    dev = (priv_camera_device_t*) device;

    gCameraHals[dev->cameraid]->disableMsgType(msg_type);
}

int camera_msg_type_enabled(struct camera_device * device, int32_t msg_type)
{
    priv_camera_device_t* dev = NULL;

    LOGD("%s", __FUNCTION__);

    if(!device)
        return 0;

    dev = (priv_camera_device_t*) device;

    return gCameraHals[dev->cameraid]->msgTypeEnabled(msg_type);
}

int camera_start_preview(struct camera_device * device)
{
    int rv = -EINVAL;
    priv_camera_device_t* dev = NULL;

    LOGD("%s", __FUNCTION__);

    if(!device)
        return rv;

    dev = (priv_camera_device_t*) device;
   // gCameraHals[dev->cameraid]->enableMsgType(CAMERA_MSG_PREVIEW_FRAME);
    rv = gCameraHals[dev->cameraid]->startPreview();

    return rv;
}

void camera_stop_preview(struct camera_device * device)
{
    priv_camera_device_t* dev = NULL;

    LOGD("%s", __FUNCTION__);

    if(!device)
        return;

    dev = (priv_camera_device_t*) device;

    gCameraHals[dev->cameraid]->stopPreview();
}

int camera_preview_enabled(struct camera_device * device)
{
    int rv = -EINVAL;
    priv_camera_device_t* dev = NULL;

    LOGD("%s", __FUNCTION__);

    if(!device)
        return rv;

    dev = (priv_camera_device_t*) device;

    rv = gCameraHals[dev->cameraid]->previewEnabled();
    return rv;
}

int camera_store_meta_data_in_buffers(struct camera_device * device, int enable)
{
    int rv = -EINVAL;
    priv_camera_device_t* dev = NULL;

    LOGD("%s", __FUNCTION__);

    if(!device)
        return rv;

    dev = (priv_camera_device_t*) device;

    //  TODO: meta data buffer not current supported
    //rv = gCameraHals[dev->cameraid]->storeMetaDataInBuffers(enable);
    return rv;
    //return enable ? android::INVALID_OPERATION: android::OK;
}

int camera_start_recording(struct camera_device * device)
{
    int rv = -EINVAL;
    priv_camera_device_t* dev = NULL;

    LOGD("%s", __FUNCTION__);

    if(!device)
        return rv;

    dev = (priv_camera_device_t*) device;

    rv = gCameraHals[dev->cameraid]->startRecording();
    return rv;
}

void camera_stop_recording(struct camera_device * device)
{
    priv_camera_device_t* dev = NULL;

    LOGD("%s", __FUNCTION__);

    if(!device)
        return;

    dev = (priv_camera_device_t*) device;

    gCameraHals[dev->cameraid]->stopRecording();
}

int camera_recording_enabled(struct camera_device * device)
{
    int rv = -EINVAL;
    priv_camera_device_t* dev = NULL;

    LOGD("%s", __FUNCTION__);

    if(!device)
        return rv;

    dev = (priv_camera_device_t*) device;

    rv = gCameraHals[dev->cameraid]->recordingEnabled();
    return rv;
}

void camera_release_recording_frame(struct camera_device * device,
                const void *opaque)
{
    priv_camera_device_t* dev = NULL;

    LOGD("%s", __FUNCTION__);

    if(!device)
        return;

    dev = (priv_camera_device_t*) device;

    //gCameraHals[dev->cameraid]->releaseRecordingFrame(opaque);
}

int camera_auto_focus(struct camera_device * device)
{
    int rv = -EINVAL;
    priv_camera_device_t* dev = NULL;

    LOGD("%s", __FUNCTION__);

    if(!device)
        return rv;

    dev = (priv_camera_device_t*) device;

    rv = gCameraHals[dev->cameraid]->autoFocus();
    return rv;
}

int camera_cancel_auto_focus(struct camera_device * device)
{
    int rv = -EINVAL;
    priv_camera_device_t* dev = NULL;

    LOGD("%s", __FUNCTION__);

    if(!device)
        return rv;

    dev = (priv_camera_device_t*) device;

    rv = gCameraHals[dev->cameraid]->cancelAutoFocus();
    return rv;
}

int camera_take_picture(struct camera_device * device)
{
    int rv = -EINVAL;
    priv_camera_device_t* dev = NULL;

    LOGD("%s", __FUNCTION__);

    if(!device)
        return rv;

    dev = (priv_camera_device_t*) device;

    rv = gCameraHals[dev->cameraid]->takePicture();
    return rv;
}

int camera_cancel_picture(struct camera_device * device)
{
    int rv = -EINVAL;
    priv_camera_device_t* dev = NULL;

    LOGD("%s", __FUNCTION__);

    if(!device)
        return rv;

    dev = (priv_camera_device_t*) device;

    rv = gCameraHals[dev->cameraid]->cancelPicture();
    return rv;
}

int camera_set_parameters(struct camera_device * device, const char *params)
{
    int rv = -EINVAL;
    priv_camera_device_t* dev = NULL;

    LOGD("%s", __FUNCTION__);

    if(!device)
        return rv;

    dev = (priv_camera_device_t*) device;
    LOGE("camera_set_parameters: %s\n", params);

    params_str8 = android::String8(params);
    camParams.unflatten(params_str8);

    rv = gCameraHals[dev->cameraid]->setParameters(camParams);
    LOGE("camera_set_parameters: rv=%d\n", rv);
    return 0;
}

char* camera_get_parameters(struct camera_device * device)
{
    char* params = NULL;
    priv_camera_device_t* dev = NULL;
    LOGD("%s", __FUNCTION__);

    if(!device)
        return NULL;

    dev = (priv_camera_device_t*) device;

    camParams = gCameraHals[dev->cameraid]->getParameters();
    CameraHAL_FixupParams(camParams);
    params_str8 = camParams.flatten();
    params = strdup((char *)params_str8.string());
    LOGV("camera_get_parameters: returning params:%p :%s\n",
        params, (params != NULL) ? params : "EMPTY STRING");
    return params;
}

static void camera_put_parameters(struct camera_device *device, char *parms)
{
    LOGD("%s", __FUNCTION__);
    free(parms);
}

int camera_send_command(struct camera_device * device,
            int32_t cmd, int32_t arg1, int32_t arg2)
{
    int rv = -EINVAL;
    priv_camera_device_t* dev = NULL;

    LOGD("%s: cmd %i", __FUNCTION__, cmd);

    if(!device)
        return rv;

    dev = (priv_camera_device_t*) device;

    rv = gCameraHals[dev->cameraid]->sendCommand(cmd, arg1, arg2);
    return rv;
}

void camera_release(struct camera_device * device)
{
    priv_camera_device_t* dev = NULL;

    LOGD("%s", __FUNCTION__);

    if(!device)
        return;

    dev = (priv_camera_device_t*) device;

    gCameraHals[dev->cameraid]->release();
}

int camera_dump(struct camera_device * device, int fd)
{
    int rv = -EINVAL;
    priv_camera_device_t* dev = NULL;

    if(!device)
        return rv;

    dev = (priv_camera_device_t*) device;

    //rv = gCameraHals[dev->cameraid]->dump(fd);
    return rv;
}

extern "C" void heaptracker_free_leaked_memory(void);

int camera_device_close(hw_device_t* device)
{
    int ret = 0;
    priv_camera_device_t* dev = NULL;

    LOGD("%s", __FUNCTION__);

    //android::Mutex::Autolock lock(gCameraDeviceLock);

    if (!device) {
        ret = -EINVAL;
        goto done;
    }

    dev = (priv_camera_device_t*) device;

    if (dev) {
        gCameraHals[dev->cameraid] = NULL;
        gCamerasOpen--;

        if (dev->base.ops) {
            free(dev->base.ops);
        }
        free(dev);
    }
done:
#ifdef HEAPTRACKER
    heaptracker_free_leaked_memory();
#endif
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
    int cameraid;
    int num_cameras = 0;
    priv_camera_device_t* priv_camera_device = NULL;
    camera_device_ops_t* camera_ops = NULL;
    sp<CameraHardwareInterface> camera = NULL;

    //android::Mutex::Autolock lock(gCameraDeviceLock);

    LOGI("camera_device open");

    if (name != NULL) {
        cameraid = atoi(name);
        num_cameras = CameraHAL_GetNum_Cameras();

        if(cameraid > num_cameras)
        {
            LOGE("camera service provided cameraid out of bounds, "
                    "cameraid = %d, num supported = %d",
                    cameraid, num_cameras);
            rv = -EINVAL;
            goto fail;
        }

        if(gCamerasOpen >= MAX_CAMERAS_SUPPORTED)
        {
            LOGE("maximum number of cameras already open");
            rv = -ENOMEM;
            goto fail;
        }

        priv_camera_device = (priv_camera_device_t*)malloc(sizeof(*priv_camera_device));
        if(!priv_camera_device)
        {
            LOGE("camera_device allocation fail");
            rv = -ENOMEM;
            goto fail;
        }

        camera_ops = (camera_device_ops_t*)malloc(sizeof(*camera_ops));
        if(!camera_ops)
        {
            LOGE("camera_ops allocation fail");
            rv = -ENOMEM;
            goto fail;
        }

        memset(priv_camera_device, 0, sizeof(*priv_camera_device));
        memset(camera_ops, 0, sizeof(*camera_ops));

        priv_camera_device->base.common.tag = HARDWARE_DEVICE_TAG;
        priv_camera_device->base.common.version = 0;
        priv_camera_device->base.common.module = (hw_module_t *)(module);
        priv_camera_device->base.common.close = camera_device_close;
        priv_camera_device->base.ops = camera_ops;

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

        *device = &priv_camera_device->base.common;

        // -------- specific stuff --------

        priv_camera_device->cameraid = cameraid;
        LOGE("cameraid %d",cameraid);
        camera = HAL_openCameraHardware(cameraid);

        if(camera == NULL)
        {
            LOGE("Couldn't create instance of CameraHal class");
            rv = -ENOMEM;
            goto fail;
        }

        gCameraHals[cameraid] = camera;
        gCamerasOpen++;
    }

    return rv;

fail:
    if(priv_camera_device) {
        free(priv_camera_device);
        priv_camera_device = NULL;
    }
    if(camera_ops) {
        free(camera_ops);
        camera_ops = NULL;
    }
    *device = NULL;
    return rv;
}
