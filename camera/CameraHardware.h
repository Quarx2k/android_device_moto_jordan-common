/*
**
** Copyright (C) 2009 0xlab.org - http://0xlab.org/
** Copyright 2008, The Android Open Source Project
**
** Licensed under the Apache License, Version 2.0 (the "License");
** you may not use this file except in compliance with the License.
** You may obtain a copy of the License at
**
**     http://www.apache.org/licenses/LICENSE-2.0
**
** Unless required by applicable law or agreed to in writing, software
** distributed under the License is distributed on an "AS IS" BASIS,
** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
** See the License for the specific language governing permissions and
** limitations under the License.
*/

#ifndef ANDROID_HARDWARE_CAMERA_HARDWARE_H
#define ANDROID_HARDWARE_CAMERA_HARDWARE_H

#include <utils/threads.h>
#include <binder/MemoryBase.h>
#include <binder/MemoryHeapBase.h>

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
#include <utils/threads.h>
#include <linux/videodev2.h>
#include "binder/MemoryBase.h"
#include "binder/MemoryHeapBase.h"

#include <camera/CameraParameters.h>
#include <hardware/camera.h>
#include <sys/ioctl.h>

#include <jpeglib.h>
#include "V4L2Camera.h"

namespace android {

typedef struct {
    size_t width;
    size_t height;
} supported_resolution;

class CameraHardware {
public:
    virtual sp<IMemoryHeap> getPreviewHeap() const;
    virtual sp<IMemoryHeap> getRawHeap() const;

    virtual void setCallbacks(camera_notify_callback notify_cb,
                              camera_data_callback data_cb,
                              camera_data_timestamp_callback data_cb_timestamp,
                              camera_request_memory get_memory,
                              void* user);
    virtual void        enableMsgType(int32_t msgType);
    virtual void        disableMsgType(int32_t msgType);
    virtual bool        msgTypeEnabled(int32_t msgType);

    virtual int setPreviewWindow( struct preview_stream_ops *window);

    virtual status_t    startPreview();
    virtual void        stopPreview();
    virtual bool        previewEnabled();

    virtual status_t    startRecording();
    virtual void        stopRecording();
    virtual bool        recordingEnabled();
    virtual void        releaseRecordingFrame(const void* opaque);

    virtual status_t    autoFocus();
    virtual status_t    cancelAutoFocus();
    virtual status_t    takePicture();
    virtual status_t    cancelPicture();
    virtual status_t    dump(int fd, const Vector<String16>& args) const;
    virtual status_t    setParameters(const CameraParameters& params);
    virtual CameraParameters  getParameters() const;
    virtual status_t sendCommand(int32_t cmd, int32_t arg1, int32_t arg2);
    virtual void release();

                        CameraHardware();
    virtual             ~CameraHardware();

    //static wp<CameraHardwareInterface> singleton;

private:

    static const int kBufferCount = 4;

    class PreviewThread : public Thread {
        CameraHardware* mHardware;
    public:
        PreviewThread(CameraHardware* hw):
            //: Thread(false), mHardware(hw) { }
#ifdef SINGLE_PROCESS
            // In single process mode this thread needs to be a java thread,
            // since we won't be calling through the binder.
            Thread(true),
#else
			// We use Andorid thread
            Thread(false),
#endif
              mHardware(hw) { }
        virtual void onFirstRef() {
            run("CameraPreviewThread", PRIORITY_URGENT_DISPLAY);
        }
        virtual bool threadLoop() {
            mHardware->previewThread();
            // loop until we need to quit
            return true;
        }
    };

    void initDefaultParameters();
	int get_kernel_version();

    int previewThread();
	/* validating supported size */
	bool validateSize(size_t width, size_t height,
			const supported_resolution *supRes, size_t count);

    static int beginAutoFocusThread(void *cookie);
    int autoFocusThread();

    static int beginPictureThread(void *cookie);
    int pictureThread();

    camera_request_memory   mRequestMemory;
    preview_stream_ops_t*  mNativeWindow;

    mutable Mutex       mLock;           // member property lock
    mutable Mutex       mPreviewLock;    // hareware v4l2 operation lock
    Mutex               mRecordingLock;
    CameraParameters    mParameters;

    sp<MemoryHeapBase>  mHeap;         // format: 420
    sp<MemoryBase>      mBuffer;
    sp<MemoryHeapBase>  mPreviewHeap;
    sp<MemoryBase>      mPreviewBuffer;
	sp<MemoryHeapBase>  mRawHeap;      /* format: 422 */
	sp<MemoryBase>      mRawBuffer;
    sp<MemoryBase>      mBuffers[kBufferCount];

    V4L2Camera         *mCamera;
    bool                mPreviewRunning;
    int                 mPreviewFrameSize;

	int					mPreviewWidth;
	int					mPreviewHeight;
	static const supported_resolution supportedPreviewRes[];
	static const supported_resolution supportedPictureRes[];
	static const char supportedPictureSizes[];
	static const char supportedPreviewSizes[];

    /* protected by mLock */
    sp<PreviewThread>   mPreviewThread;

    camera_notify_callback     mNotifyCb;
    camera_data_callback       mDataCb;
    camera_data_timestamp_callback mDataCbTimestamp;
    void               *mCallbackCookie;

    int32_t             mMsgEnabled;

    bool                previewStopped;
    bool                mRecordingEnabled;
   camera_memory_t* mRecordBuffer ;
   camera_memory_t* mPictureBuffer ;
};

}; // namespace android

#endif
