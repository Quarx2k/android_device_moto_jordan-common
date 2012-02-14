/*
 * Copyright (C) 2012, Won-Kyu Park
 * Copyright (C) 2012, Raviprasad V Mummidi
 * Copyright (C) 2011, Ivan Zupan
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
 * ChangeLog
 *
 * 2012/01/19 - based on Raviprasad V Mummidi's code and some code by Ivan Zupan
 * 2012/01/21 - cleaned up by wkpark.
 * 2012/02/09 - first working version for P990/SU660 with software rendering
 *            - need to revert "MemoryHeapBase: Save and binderize the offset"
 *              commit f24c4cd0f204068a17f61f1c195ccf140c6c1d67.
 *            - some wrapper functions are needed (please see the libui.patch)
 */

#define LOG_TAG "CameraHAL"
#define LOG_NDEBUG 0

#include "CameraHardwareInterface.h"
#include <hardware/hardware.h>
#include <hardware/camera.h>
#include <binder/IMemory.h>
#include <fcntl.h>
#include <linux/ioctl.h>
#include <hardware/gralloc.h>

#define NO_ERROR 0

struct legacy_camera_device {
   camera_device_t device;
   android::sp<android::CameraHardwareInterface> hwif;
   int id;

   camera_notify_callback         notify_callback;
   camera_data_callback           data_callback;
   camera_data_timestamp_callback data_timestamp_callback;
   camera_request_memory          request_memory;
   void                          *user;

   preview_stream_ops *window;
   gralloc_module_t const *gralloc;
   int passed_frames;
};

/** camera_hw_device implementation **/
static inline struct legacy_camera_device * to_lcdev(struct camera_device *dev)
{
    return reinterpret_cast<struct legacy_camera_device *>(dev);
}

/* Prototypes and extern functions. */
extern "C" android::sp<android::CameraHardwareInterface> HAL_openCameraHardware(int cameraId);
extern "C" int HAL_getNumberOfCameras();
extern "C" void HAL_getCameraInfo(int cameraId, struct CameraInfo* cameraInfo);

int qcom_camera_device_open(const hw_module_t* module, const char* name,
                        hw_device_t** device);
int CameraHAL_GetCam_Info(int camera_id, struct camera_info *info);

static hw_module_methods_t camera_module_methods = {
   open: qcom_camera_device_open
};

camera_module_t HAL_MODULE_INFO_SYM = {
   common: {
      tag: HARDWARE_MODULE_TAG,
      version_major: 1,
      version_minor: 0,
      id: CAMERA_HARDWARE_MODULE_ID,
      name: "Camera HAL for ICS/CM9",
      author: "Won-Kyu Park, Raviprasad V Mummidi, Ivan Zupan",
      methods: &camera_module_methods,
      dso: NULL,
      reserved: {0},
   },
   get_number_of_cameras: HAL_getNumberOfCameras,
   get_camera_info: CameraHAL_GetCam_Info,
};

/* HAL helper functions. */
void
CameraHAL_NotifyCb(int32_t msg_type, int32_t ext1,
                   int32_t ext2, void *user)
{
   struct legacy_camera_device *lcdev = (struct legacy_camera_device *) user;
   LOGV("CameraHAL_NotifyCb: msg_type:%d ext1:%d ext2:%d user:%p\n",
        msg_type, ext1, ext2, user);
   if (lcdev->notify_callback != NULL) {
      lcdev->notify_callback(msg_type, ext1, ext2, lcdev->user);
   }
}

//
// http://code.google.com/p/android/issues/detail?id=823#c4
//
void Yuv420spToRGBA8888(char* rgb, char* yuv420sp, int width, int height)
{
    int frameSize = width * height;
    int colr = 0;
    for (int j = 0, yp = 0, k = 0; j < height; j++) {
        int uvp = frameSize + (j >> 1) * width, u = 0, v = 0;
        for (int i = 0; i < width; i++, yp++) {
            int y = (0xff & ((int) yuv420sp[yp])) - 16;
            if (y < 0) y = 0;
            if ((i & 1) == 0) {
                v = (0xff & yuv420sp[uvp++]) - 128;
                u = (0xff & yuv420sp[uvp++]) - 128;
            }

            int y1192 = 1192 * y;
            int r = (y1192 + 1634 * v);
            int g = (y1192 - 833 * v - 400 * u);
            int b = (y1192 + 2066 * u);

            if (r < 0) r = 0; else if (r > 262143) r = 262143;
            if (g < 0) g = 0; else if (g > 262143) g = 262143;
            if (b < 0) b = 0; else if (b > 262143) b = 262143;

#if 1
            /* for RGB8888 */
            r = (r >> 10) & 0xff;
            g = (g >> 10) & 0xff;
            b = (b >> 10) & 0xff;

            rgb[k++] = r;
            rgb[k++] = g;
            rgb[k++] = b;
            rgb[k++] = 255;
#else
            /* for RGB565 */
            /*
            r = (r >> 13) & 0x1f;
            g = (g >> 12) & 0x3f;
            b = (b >> 13) & 0x1f;
            unsigned int rgb2 = (r << 11) | (g << 5)| b;
            */
            unsigned short rgb2 = ((r >> 2) & 0xf800) | ((g >> 7) & 0x7e0) | ((b >> 13) & 0x1f);
            rgb[k++] = rgb2 & 0x00ff;
            rgb[k++] = (rgb2 >> 8) & 0xff;
#endif
        }
    }
}

void
CameraHAL_HandlePreviewData(const android::sp<android::IMemory>& dataPtr,
                            int32_t previewWidth, int32_t previewHeight, void* user)
{
   struct legacy_camera_device *lcdev = (struct legacy_camera_device *) user;
   if (lcdev->window != NULL && lcdev->request_memory != NULL) {
      ssize_t  offset;
      size_t   size;
      int retVal;

      android::sp<android::IMemoryHeap> mHeap = dataPtr->getMemory(&offset,
                                                                   &size);

      LOGV("%s: previewWidth:%d previewHeight:%d "
           "offset:%#x size:%#x base:%p\n", __FUNCTION__, previewWidth, previewHeight,
           (unsigned)offset, size, mHeap != NULL ? mHeap->base() : 0);

      int previewFormat = HAL_PIXEL_FORMAT_YCrCb_420_SP;
      int bufferFormat = HAL_PIXEL_FORMAT_RGBA_8888;
                         //HAL_PIXEL_FORMAT_RGB_565; // work
                         //HAL_PIXEL_FORMAT_YCrCb_420_SP; // not work

      retVal = lcdev->window->set_buffers_geometry(lcdev->window,
                                             previewWidth, previewHeight,
                                             bufferFormat);

      if (retVal == NO_ERROR) {
         int32_t          stride;
         buffer_handle_t *bufHandle = NULL;

         retVal = lcdev->window->dequeue_buffer(lcdev->window, &bufHandle, &stride);
         if (retVal == NO_ERROR) {
            retVal = lcdev->window->lock_buffer(lcdev->window, bufHandle);
            if (retVal == NO_ERROR) {
               int tries = 5;
               int err = 0;
               void *vaddr;

               err = lcdev->gralloc->lock(lcdev->gralloc, *bufHandle,
			    GRALLOC_USAGE_SW_WRITE_MASK,
                            0, 0, previewWidth, previewHeight, &vaddr);
               while (err && tries) {
                  // Pano frames almost always need a retry...
                  usleep(1000);
                  //LOGW("RETRYING LOCK");
                  lcdev->gralloc->unlock(lcdev->gralloc, *bufHandle);
                  err = lcdev->gralloc->lock(lcdev->gralloc, *bufHandle,
			    GRALLOC_USAGE_SW_WRITE_MASK,
                            0, 0, previewWidth, previewHeight, &vaddr);
                  tries--;
               }
               if (!err) {
                  char *frame = (char *)(mHeap->base()) + offset;
                  // direct copy
                  //memcpy(vaddr, frame, size);
                  // software conversion
                  Yuv420spToRGBA8888((char *)vaddr, frame, previewWidth, previewHeight);

                  lcdev->gralloc->unlock(lcdev->gralloc, *bufHandle);
                  if (0 != lcdev->window->enqueue_buffer(lcdev->window, bufHandle)) {
                     LOGE("%s: could not enqueue gralloc buffer", __FUNCTION__);
                  }
               } else {
                  LOGE("%s: could not lock gralloc buffer", __FUNCTION__);
               }
            } else {
               LOGE("%s: ERROR locking the buffer\n", __FUNCTION__);
               lcdev->window->cancel_buffer(lcdev->window, bufHandle);
            }
         } else {
            LOGE("%s: ERROR dequeueing the buffer\n", __FUNCTION__);
         }
      }
   }
}

camera_memory_t *
CameraHAL_GenClientData(const android::sp<android::IMemory> &dataPtr,
                        void *user)
{
   ssize_t          offset;
   size_t           size;
   camera_memory_t *clientData = NULL;
   struct legacy_camera_device *lcdev = (struct legacy_camera_device *) user;
   android::sp<android::IMemoryHeap> mHeap = dataPtr->getMemory(&offset, &size);

   LOGV("CameraHAL_GenClientData: offset:%#x size:%#x base:%p\n",
        (unsigned)offset, size, mHeap != NULL ? mHeap->base() : 0);

   LOGV("%s: #1", __FUNCTION__);
   clientData = lcdev->request_memory(-1, size, 1, lcdev->user);
   LOGV("%s: #2", __FUNCTION__);
   if (clientData != NULL) {
      memcpy(clientData->data, (char *)(mHeap->base()) + offset, size);
   LOGV("%s: #3", __FUNCTION__);
   } else {
      LOGV("CameraHAL_GenClientData: ERROR allocating memory from client\n");
   }
   return clientData;
}

void
CameraHAL_DataCb(int32_t msg_type, const android::sp<android::IMemory>& dataPtr,
                 void *user)
{
   struct legacy_camera_device *lcdev = (struct legacy_camera_device *) user;

   //LOGV("CameraHAL_DataCb: msg_type:%d user:%p\n", msg_type, user);
   if (msg_type == CAMERA_MSG_PREVIEW_FRAME) {
      int32_t previewWidth, previewHeight;

      android::CameraParameters hwParameters(lcdev->hwif->getParameters());
      hwParameters.getPreviewSize(&previewWidth, &previewHeight);
      //LOGV("CameraHAL_DataCb: preview size = %dx%d\n", previewWidth, previewHeight);
      CameraHAL_HandlePreviewData(dataPtr, previewWidth, previewHeight, lcdev);
      if (!lcdev->passed_frames && lcdev->data_callback != NULL && lcdev->request_memory != NULL) {
         /* Ugly hack. The device won't handle copying all the frames for
          * userspace processing, since it'll dry out the memory and hog
          * the CPU. So just pass one out of every 10 */
         camera_memory_t *clientData = CameraHAL_GenClientData(dataPtr, lcdev);
         if (clientData != NULL) {
            LOGV("CameraHAL_DataCb: Posting data to client\n");
            lcdev->data_callback(msg_type, clientData, 0, NULL, lcdev->user);
         }
      }
   }
   lcdev->passed_frames++;
   lcdev->passed_frames%=10;
}

void
CameraHAL_DataTSCb(nsecs_t timestamp, int32_t msg_type,
                   const android::sp<android::IMemory>& dataPtr, void *user)
{
   struct legacy_camera_device *lcdev = (struct legacy_camera_device *) user;

   LOGD("CameraHAL_DataTSCb: timestamp:%lld msg_type:%d user:%p\n",
        timestamp /1000, msg_type, user);

   if (lcdev->data_callback != NULL && lcdev->request_memory != NULL) {
      camera_memory_t *clientData = CameraHAL_GenClientData(dataPtr, lcdev);
      if (clientData != NULL) {
         LOGV("CameraHAL_DataTSCb: Posting data to client timestamp:%lld\n",
              systemTime());
         lcdev->data_timestamp_callback(timestamp, msg_type, clientData, 0, lcdev->user);
         lcdev->hwif->releaseRecordingFrame(dataPtr);
      } else {
         LOGD("CameraHAL_DataTSCb: ERROR allocating memory from client\n");
      }
   }
}

int
CameraHAL_GetCam_Info(int camera_id, struct camera_info *info)
{
   int rv = 0;
   LOGV("CameraHAL_GetCam_Info:\n");

   android::CameraInfo cam_info;
   android::HAL_getCameraInfo(camera_id, &cam_info);

   info->facing = cam_info.facing;
   info->orientation = cam_info.orientation;

   LOGD("%s: id:%i faceing:%i orientation: %i", __FUNCTION__,
        camera_id, info->facing, info->orientation);

   return rv;
}

#if 0
void
CameraHAL_FixupParams(android::CameraParameters &settings)
{
   const char *video_sizes = "480x320,352x288,320x240,176x144";
   //const char *video_sizes = "512x288,480x320,352x288,320x240,176x144";
   const char *preferred_size       = "480x320";
   const char *preview_frame_rates  = "15,10";
   //const char *preview_frame_rates  = "30,27,24,15";
   const char *preferred_frame_rate = "15";

   const char *preview_fps_range    = "1000,30000";
   const char *preview_fps_range_values = "(1000,7000),(1000,10000),(1000,15000),(1000,20000),(1000,24000),(1000,30000)";
   const char *horizontal_view_angle= "360";
   const char *vertical_view_angle= "360";

   settings.set(android::CameraParameters::KEY_VIDEO_FRAME_FORMAT,
                android::CameraParameters::PIXEL_FORMAT_YUV420SP);

#if 0
   const char *effect_values= "none,mono,negative,solarize,sepia,aqua";
   const char *flash_mode_values= "auto,on,off,red-eye";
   const char *focus_mode_values= "auto,macro";

   /* fix some supported modes to generic */
   settings.set(android::CameraParameters::KEY_SUPPORTED_FLASH_MODES,
                flash_mode_values);
   settings.set(android::CameraParameters::KEY_SUPPORTED_FOCUS_MODES,
                focus_mode_values);
   settings.set(android::CameraParameters::KEY_SUPPORTED_EFFECTS,
                effect_values);
#endif

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

   LOGV("fixup\n");
   if (!settings.get(android::CameraParameters::KEY_PREVIEW_FRAME_RATE)) {
      settings.set(android::CameraParameters::KEY_PREVIEW_FRAME_RATE,
                   preferred_frame_rate);
   }
}
#endif

/* Hardware Camera interface handlers. */
int
qcom_camera_set_preview_window(struct camera_device * device,
                           struct preview_stream_ops *window)
{
   int rv = -EINVAL;
   int min_bufs = -1;
   int kBufferCount = 4;
   struct legacy_camera_device *lcdev = to_lcdev(device);

   LOGV("qcom_camera_set_preview_window : Window :%p\n", window);
   if (device == NULL) {
      LOGE("qcom_camera_set_preview_window : Invalid device.\n");
      return -EINVAL;
   }

   if (lcdev->window == window) {
      return 0;
   }

   lcdev->window = window;

   if (!window) {
      LOGD("%s: window is NULL", __FUNCTION__);
      return -EINVAL;
   }

   LOGD("%s : OK window is %p", __FUNCTION__, window);

   if (!lcdev->gralloc) {
      hw_module_t const* module;
      int err = 0;
      if (hw_get_module(GRALLOC_HARDWARE_MODULE_ID, &module) == 0) {
         lcdev->gralloc = (const gralloc_module_t *)module;
      } else {
         LOGE("%s: Fail on loading gralloc HAL", __FUNCTION__);
      }
   }


   LOGD("%s: OK on loading gralloc HAL", __FUNCTION__);
   if (window->get_min_undequeued_buffer_count(window, &min_bufs)) {
      LOGE("%s: could not retrieve min undequeued buffer count", __FUNCTION__);
      return -1;
   }
   LOGD("%s: OK get_min_undequeued_buffer_count", __FUNCTION__);

   LOGD("%s: bufs:%i", __FUNCTION__, min_bufs);
   if (min_bufs >= kBufferCount) {
      LOGE("%s: min undequeued buffer count %i is too high (expecting at most %i)",
          __FUNCTION__, min_bufs, kBufferCount - 1);
   }

   LOGD("%s: setting buffer count to %i", __FUNCTION__, kBufferCount);
   if (window->set_buffer_count(window, kBufferCount)) {
      LOGE("%s: could not set buffer count", __FUNCTION__);
      return -1;
   }

   int w, h;
   android::CameraParameters params(lcdev->hwif->getParameters());
   params.getPreviewSize(&w, &h);
   int hal_pixel_format = HAL_PIXEL_FORMAT_YCrCb_420_SP;

   const char *str_preview_format = params.getPreviewFormat();
   LOGD("%s: preview format %s", __FUNCTION__, str_preview_format);

   if (window->set_usage(window, GRALLOC_USAGE_SW_WRITE_MASK)) {
      LOGE("%s: could not set usage on gralloc buffer", __FUNCTION__);
      return -1;
   }

   if (window->set_buffers_geometry(window, w, h, hal_pixel_format)) {
      LOGE("%s: could not set buffers geometry to %s",
          __FUNCTION__, str_preview_format);
      return -1;
   }

   return NO_ERROR;
}

void
qcom_camera_set_callbacks(struct camera_device * device,
                      camera_notify_callback notify_cb,
                      camera_data_callback data_cb,
                      camera_data_timestamp_callback data_cb_timestamp,
                      camera_request_memory get_memory, void *user)
{
   struct legacy_camera_device *lcdev = to_lcdev(device);
   LOGV("qcom_camera_set_callbacks: notify_cb: %p, data_cb: %p "
        "data_cb_timestamp: %p, get_memory: %p, user :%p",
        notify_cb, data_cb, data_cb_timestamp, get_memory, user);

   lcdev->notify_callback = notify_cb;
   lcdev->data_callback = data_cb;
   lcdev->data_timestamp_callback = data_cb_timestamp;
   lcdev->request_memory = get_memory;
   lcdev->user = user;

   lcdev->hwif->setCallbacks(CameraHAL_NotifyCb, CameraHAL_DataCb,
                         CameraHAL_DataTSCb, (void *) lcdev);
}

void
qcom_camera_enable_msg_type(struct camera_device * device, int32_t msg_type)
{
   struct legacy_camera_device *lcdev = to_lcdev(device);
   LOGV("qcom_camera_enable_msg_type: msg_type:%d\n", msg_type);
   lcdev->hwif->enableMsgType(msg_type);
}

void
qcom_camera_disable_msg_type(struct camera_device * device, int32_t msg_type)
{
   struct legacy_camera_device *lcdev = to_lcdev(device);
   LOGV("qcom_camera_disable_msg_type: msg_type:%d\n", msg_type);
   lcdev->hwif->disableMsgType(msg_type);
}

int
qcom_camera_msg_type_enabled(struct camera_device * device, int32_t msg_type)
{
   struct legacy_camera_device *lcdev = to_lcdev(device);
   LOGV("qcom_camera_msg_type_enabled: msg_type:%d\n", msg_type);
   return lcdev->hwif->msgTypeEnabled(msg_type);
}

int
qcom_camera_start_preview(struct camera_device * device)
{
   struct legacy_camera_device *lcdev = to_lcdev(device);
   LOGV("qcom_camera_start_preview:\n");

   /* TODO: Remove hack. */
   LOGV("qcom_camera_start_preview: Enabling CAMERA_MSG_PREVIEW_FRAME\n");
   lcdev->hwif->enableMsgType(CAMERA_MSG_PREVIEW_FRAME);
   return lcdev->hwif->startPreview();
}

void
qcom_camera_stop_preview(struct camera_device * device)
{
   struct legacy_camera_device *lcdev = to_lcdev(device);
   LOGV("qcom_camera_stop_preview:\n");

   /* TODO: Remove hack. */
   lcdev->hwif->disableMsgType(CAMERA_MSG_PREVIEW_FRAME);
   lcdev->hwif->stopPreview();
   return;
}

int
qcom_camera_preview_enabled(struct camera_device * device)
{
   struct legacy_camera_device *lcdev = to_lcdev(device);
   int ret = lcdev->hwif->previewEnabled();
   LOGV("qcom_camera_preview_enabled: %d\n", ret);
   return ret;
}

int
qcom_camera_store_meta_data_in_buffers(struct camera_device * device, int enable)
{
   /*struct legacy_camera_device *lcdev = to_lcdev(device);
   int ret = lcdev->hwif->storeMetaDataInBuffers(enable);*/
   LOGV("qcom_camera_store_meta_data_in_buffers:\n");
   return NO_ERROR;
}

int
qcom_camera_start_recording(struct camera_device * device)
{
   struct legacy_camera_device *lcdev = to_lcdev(device);
   LOGV("qcom_camera_start_recording\n");

   /* TODO: Remove hack. */
   lcdev->hwif->enableMsgType(CAMERA_MSG_VIDEO_FRAME);
   lcdev->hwif->startRecording();
   return NO_ERROR;
}

void
qcom_camera_stop_recording(struct camera_device * device)
{
   struct legacy_camera_device *lcdev = to_lcdev(device);
   LOGV("qcom_camera_stop_recording:\n");

   /* TODO: Remove hack. */
   lcdev->hwif->disableMsgType(CAMERA_MSG_VIDEO_FRAME);
   lcdev->hwif->stopRecording();
}

int
qcom_camera_recording_enabled(struct camera_device * device)
{
   struct legacy_camera_device *lcdev = to_lcdev(device);
   LOGV("qcom_camera_recording_enabled:\n");
   return (int)lcdev->hwif->recordingEnabled();
}

void
qcom_camera_release_recording_frame(struct camera_device * device,
                                const void *opaque)
{
   /*
    * We release the frame immediately in CameraHAL_DataTSCb after making a
    * copy. So, this is just a NOP.
    */
   LOGV("qcom_camera_release_recording_frame: opaque:%p\n", opaque);
}

int
qcom_camera_auto_focus(struct camera_device * device)
{
   struct legacy_camera_device *lcdev = to_lcdev(device);
   LOGV("qcom_camera_auto_focus:\n");
   lcdev->hwif->autoFocus();
   return NO_ERROR;
}

int
qcom_camera_cancel_auto_focus(struct camera_device * device)
{
   struct legacy_camera_device *lcdev = to_lcdev(device);
   LOGV("qcom_camera_cancel_auto_focus:\n");
   lcdev->hwif->cancelAutoFocus();
   return NO_ERROR;
}

int
qcom_camera_take_picture(struct camera_device * device)
{
   struct legacy_camera_device *lcdev = to_lcdev(device);
   LOGV("qcom_camera_take_picture:\n");

   /* TODO: Remove hack. */
   lcdev->hwif->enableMsgType(CAMERA_MSG_SHUTTER |
                         CAMERA_MSG_POSTVIEW_FRAME |
                         CAMERA_MSG_RAW_IMAGE |
                         CAMERA_MSG_COMPRESSED_IMAGE);

   lcdev->hwif->takePicture();
   return NO_ERROR;
}

int
qcom_camera_cancel_picture(struct camera_device * device)
{
   struct legacy_camera_device *lcdev = to_lcdev(device);
   LOGV("camera_cancel_picture:\n");
   lcdev->hwif->cancelPicture();
   return NO_ERROR;
}

int
qcom_camera_set_parameters(struct camera_device * device, const char *params)
{
   struct legacy_camera_device *lcdev = to_lcdev(device);
   LOGV("qcom_camera_set_parameters: %s\n", params);
   android::String8 s(params);
   android::CameraParameters p(s);

#if 0
   // Fix up zoom
   int zoom = p.getInt(android::CameraParameters::KEY_ZOOM);
   if (zoom == 0) {
      p.set(android::CameraParameters::KEY_ZOOM, "1");
   }
#endif

   lcdev->hwif->setParameters(p);
   return NO_ERROR;
}

char*
qcom_camera_get_parameters(struct camera_device * device)
{
   struct legacy_camera_device *lcdev = to_lcdev(device);
   char *rc = NULL;
   LOGV("qcom_camera_get_parameters\n");
   android::CameraParameters params(lcdev->hwif->getParameters());
   //CameraHAL_FixupParams(params);
   LOGV("qcom_camera_get_parameters: after calling hwif->getParameters()\n");
   rc = strdup((char *)params.flatten().string());
   LOGV("camera_get_parameters: returning rc:%p :%s\n",
        rc, (rc != NULL) ? rc : "EMPTY STRING");
   return rc;
}

void
qcom_camera_put_parameters(struct camera_device *device, char *params)
{
   LOGV("qcom_camera_put_parameters: params:%p %s", params, params);
   free(params);
}


int
qcom_camera_send_command(struct camera_device * device, int32_t cmd,
                        int32_t arg0, int32_t arg1)
{
   struct legacy_camera_device *lcdev = to_lcdev(device);
   LOGV("qcom_camera_send_command: cmd:%d arg0:%d arg1:%d\n",
        cmd, arg0, arg1);
   return lcdev->hwif->sendCommand(cmd, arg0, arg1);
}

void
qcom_camera_release(struct camera_device * device)
{
   struct legacy_camera_device *lcdev = to_lcdev(device);
   LOGV("qcom_camera_release:\n");
   lcdev->hwif->release();
}

int
qcom_camera_dump(struct camera_device * device, int fd)
{
   struct legacy_camera_device *lcdev = to_lcdev(device);
   LOGV("qcom_camera_dump:\n");
   android::Vector<android::String16> args;
   return lcdev->hwif->dump(fd, args);
}

int
qcom_camera_device_close(hw_device_t* device)
{
   struct camera_device * hwdev = reinterpret_cast<struct camera_device *>(device);
   struct legacy_camera_device *lcdev = to_lcdev(hwdev);
   int rc = -EINVAL;
   LOGD("camera_device_close\n");
   if (lcdev != NULL) {
      camera_device_ops_t *camera_ops = lcdev->device.ops;
      if (camera_ops) {
         if (lcdev->hwif != NULL) {
            lcdev->hwif.clear();
         }
         free(camera_ops);
      }
      free(lcdev);
      rc = NO_ERROR;
   }
   return rc;
}

int
qcom_camera_device_open(const hw_module_t* module, const char* name,
                   hw_device_t** device)
{
   int ret;
   struct legacy_camera_device *lcdev;
   camera_device_t* camera_device;
   camera_device_ops_t* camera_ops;

   if (name == NULL)
      return 0;

   int cameraId = atoi(name);

   LOGD("qcom_camera_device_open: name:%s device:%p cameraId:%d\n",
        name, device, cameraId);

   lcdev = (struct legacy_camera_device *)calloc(1, sizeof(*lcdev));
   camera_ops = (camera_device_ops_t *)malloc(sizeof(*camera_ops));
   memset(camera_ops, 0, sizeof(*camera_ops));

   lcdev->device.common.tag               = HARDWARE_DEVICE_TAG;
   lcdev->device.common.version           = 0;
   lcdev->device.common.module            = (hw_module_t *)(module);
   lcdev->device.common.close             = qcom_camera_device_close;
   lcdev->device.ops                      = camera_ops;

   camera_ops->set_preview_window         = qcom_camera_set_preview_window;
   camera_ops->set_callbacks              = qcom_camera_set_callbacks;
   camera_ops->enable_msg_type            = qcom_camera_enable_msg_type;
   camera_ops->disable_msg_type           = qcom_camera_disable_msg_type;
   camera_ops->msg_type_enabled           = qcom_camera_msg_type_enabled;
   camera_ops->start_preview              = qcom_camera_start_preview;
   camera_ops->stop_preview               = qcom_camera_stop_preview;
   camera_ops->preview_enabled            = qcom_camera_preview_enabled;
   camera_ops->store_meta_data_in_buffers = qcom_camera_store_meta_data_in_buffers;
   camera_ops->start_recording            = qcom_camera_start_recording;
   camera_ops->stop_recording             = qcom_camera_stop_recording;
   camera_ops->recording_enabled          = qcom_camera_recording_enabled;
   camera_ops->release_recording_frame    = qcom_camera_release_recording_frame;
   camera_ops->auto_focus                 = qcom_camera_auto_focus;
   camera_ops->cancel_auto_focus          = qcom_camera_cancel_auto_focus;
   camera_ops->take_picture               = qcom_camera_take_picture;
   camera_ops->cancel_picture             = qcom_camera_cancel_picture;

   camera_ops->set_parameters             = qcom_camera_set_parameters;
   camera_ops->get_parameters             = qcom_camera_get_parameters;
   camera_ops->put_parameters             = qcom_camera_put_parameters;
   camera_ops->send_command               = qcom_camera_send_command;
   camera_ops->release                    = qcom_camera_release;
   camera_ops->dump                       = qcom_camera_dump;

   lcdev->id = cameraId;
   lcdev->hwif = HAL_openCameraHardware(cameraId);
   if (lcdev->hwif == NULL) {
       ret = -EIO;
       goto err_create_camera_hw;
   }
   *device = &lcdev->device.common;
   return NO_ERROR;

err_create_camera_hw:
   free(lcdev);
   free(camera_ops);
   return ret;
}

/*
 * vim:et:sts=3:sw=3
 */
