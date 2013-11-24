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

#define LOG_TAG "V4L2Camera"
#include <utils/Log.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/select.h>

#include <linux/videodev.h>
extern int version;

extern "C" {
    #include <jpeglib.h>
}
#define MEDIA_DEVICE "/dev/media0"
#define ENTITY_VIDEO_CCDC_OUT_NAME      "OMAP3 ISP CCDC output"
#define ENTITY_CCDC_NAME                "OMAP3 ISP CCDC"
#define ENTITY_TVP514X_NAME             "tvp514x 3-005c"
#define ENTITY_MT9T111_NAME             "mt9t111 2-003c"
#ifdef CONFIG_FLASHBOARD
#define ENTITY_MT9V113_NAME             "mt9v113 3-003c"
#else
#define ENTITY_MT9V113_NAME             "mt9v113 2-003c"
#endif
#define IMG_WIDTH_VGA           640
#define IMG_HEIGHT_VGA          480
#define DEF_PIX_FMT             V4L2_PIX_FMT_UYVY

#include "V4L2Camera.h"

#define KERNEL_VERSION(a,b) (((a) << 16) + ((b) << 8) )

namespace android {

V4L2Camera::V4L2Camera ()
    : nQueued(0), nDequeued(0)
{
    videoIn = (struct vdIn *) calloc (1, sizeof (struct vdIn));
    mediaIn = (struct mdIn *) calloc (1, sizeof (struct mdIn));
    mediaIn->input_source=1;
    camHandle = -1;
#ifdef _OMAP_RESIZER_
	videoIn->resizeHandle = -1;
#endif //_OMAP_RESIZER_
}

V4L2Camera::~V4L2Camera()
{
    free(videoIn);
    free(mediaIn);
}

int V4L2Camera::Open(const char *device)
{
	int ret = 0;
	int ccdc_fd, tvp_fd;
	struct v4l2_subdev_format fmt;
	char subdev[20];

	do
	{
		if ((camHandle = open(device, O_RDWR)) == -1) {
			ALOGE("ERROR opening V4L interface: %s %s", strerror(errno),device);
			if(version == KERNEL_VERSION(2,6))
				reset_links(MEDIA_DEVICE);
			return -1;
		}
		if(version == KERNEL_VERSION(2,6))
		{
			ccdc_fd = open("/dev/v4l-subdev2", O_RDWR);
			if(ccdc_fd == -1) {
				ALOGE("Error opening ccdc device");
				close(camHandle);
				reset_links(MEDIA_DEVICE);
				return -1;
			}
			fmt.pad = 0;
			fmt.which = V4L2_SUBDEV_FORMAT_ACTIVE;
			fmt.format.code = V4L2_MBUS_FMT_UYVY8_2X8;
			fmt.format.width = IMG_WIDTH_VGA;
			fmt.format.height = IMG_HEIGHT_VGA;
			fmt.format.colorspace = V4L2_COLORSPACE_SMPTE170M;
			fmt.format.field = V4L2_FIELD_INTERLACED;
			ret = ioctl(ccdc_fd, VIDIOC_SUBDEV_S_FMT, &fmt);
			if(ret < 0)
			{
				ALOGE("Failed to set format on pad");
			}
			memset(&fmt, 0, sizeof(fmt));
			fmt.pad = 1;
			fmt.which = V4L2_SUBDEV_FORMAT_ACTIVE;
			fmt.format.code = V4L2_MBUS_FMT_UYVY8_2X8;
			fmt.format.width = IMG_WIDTH_VGA;
			fmt.format.height = IMG_HEIGHT_VGA;
			fmt.format.colorspace = V4L2_COLORSPACE_SMPTE170M;
			fmt.format.field = V4L2_FIELD_INTERLACED;
			ret = ioctl(ccdc_fd, VIDIOC_SUBDEV_S_FMT, &fmt);
			if(ret) {
				ALOGE("Failed to set format on pad");
			}
			mediaIn->input_source=1;
			if (mediaIn->input_source != 0)
				strcpy(subdev, "/dev/v4l-subdev8");
			else
				strcpy(subdev, "/dev/v4l-subdev9");
			tvp_fd = open(subdev, O_RDWR);
			if(tvp_fd == -1) {
				ALOGE("Failed to open subdev");
				ret=-1;
				close(camHandle);
				reset_links(MEDIA_DEVICE);
				return ret;
			}
		}

		ret = ioctl (camHandle, VIDIOC_QUERYCAP, &videoIn->cap);
		if (ret < 0) {
			ALOGE("Error opening device: unable to query device.");
			break;
		}

		if ((videoIn->cap.capabilities & V4L2_CAP_VIDEO_CAPTURE) == 0) {
			ALOGE("Error opening device: video capture not supported.");
			ret = -1;
			break;
		}

		if (!(videoIn->cap.capabilities & V4L2_CAP_STREAMING)) {
			ALOGE("Capture device does not support streaming i/o");
			ret = -1;
			break;
		}
#ifdef _OMAP_RESIZER_
		videoIn->resizeHandle = OMAPResizerOpen();
#endif //_OMAP_RESIZER_
	} while(0);

    return ret;
}

int V4L2Camera::Open_media_device(const char *device)
{
	int ret = 0;
	int index = 0;
	int i;
	struct media_link_desc link;
	struct media_links_enum links;
	int input_v4l;

	/*opening the media device*/
	mediaIn->media_fd = open(device, O_RDWR);
	if(mediaIn->media_fd <= 0)
	{
		ALOGE("ERROR opening media device: %s",strerror(errno));
		return -1;
	}

	/*enumerate_all_entities*/
	do {
		mediaIn->entity[index].id = index | MEDIA_ENTITY_ID_FLAG_NEXT;
		ret = ioctl(mediaIn->media_fd, MEDIA_IOC_ENUM_ENTITIES, &mediaIn->entity[index]);
		if (ret < 0) {
			break;
		} else {
			if (!strcmp(mediaIn->entity[index].name, ENTITY_VIDEO_CCDC_OUT_NAME))
				mediaIn->video =  mediaIn->entity[index].id;
			else if (!strcmp(mediaIn->entity[index].name, ENTITY_TVP514X_NAME))
				mediaIn->tvp5146 =  mediaIn->entity[index].id;
			else if (!strcmp(mediaIn->entity[index].name, ENTITY_MT9T111_NAME))
			{
				mediaIn->mt9t111 =  mediaIn->entity[index].id;
				mediaIn->input_source=1;
			}
			else if (!strcmp(mediaIn->entity[index].name, ENTITY_CCDC_NAME))
				mediaIn->ccdc =  mediaIn->entity[index].id;
			else if (!strcmp(mediaIn->entity[index].name, ENTITY_MT9V113_NAME))
			{
				mediaIn->mt9v113 =  mediaIn->entity[index].id;
				mediaIn->input_source=2;
			}
		}
		index++;
	}while(ret==0);

	if ((ret < 0) && (index <= 0)) {
		ALOGE("Failed to enumerate entities ret val is %d",ret);
		close(mediaIn->media_fd);
		return -1;
	}
	mediaIn->num_entities = index;

	/*setup_media_links*/
	for(index = 0; index < mediaIn->num_entities; index++) {
		links.entity = mediaIn->entity[index].id;
		links.pads =(struct media_pad_desc *) malloc((sizeof( struct media_pad_desc)) * (mediaIn->entity[index].pads));
		links.links = (struct media_link_desc *) malloc((sizeof(struct media_link_desc)) * mediaIn->entity[index].links);
		ret = ioctl(mediaIn->media_fd, MEDIA_IOC_ENUM_LINKS, &links);
		if (ret < 0) {
			ALOGE("ERROR  while enumerating links/pads");
			break;
		}
		else {
			if(mediaIn->entity[index].pads)
				ALOGD("pads for entity %d=", mediaIn->entity[index].id);
			for(i = 0 ; i < mediaIn->entity[index].pads; i++) {
				ALOGD("(%d %s) ", links.pads->index,(links.pads->flags & MEDIA_PAD_FLAG_INPUT) ?"INPUT" : "OUTPUT");
				links.pads++;
			}
			for(i = 0; i < mediaIn->entity[index].links; i++) {
				ALOGD("[%d:%d]===>[%d:%d]",links.links->source.entity,links.links->source.index,links.links->sink.entity,links.links->sink.index);
				if(links.links->flags & MEDIA_LINK_FLAG_ENABLED)
					ALOGD("\tACTIVE\n");
				else
					ALOGD("\tINACTIVE \n");
				links.links++;
			}
		}
	}
	if (mediaIn->input_source == 1)
		input_v4l = mediaIn->mt9t111;
	else if (mediaIn->input_source == 2)
		input_v4l = mediaIn->mt9v113;
	else
		input_v4l = mediaIn->tvp5146;

	memset(&link, 0, sizeof(link));
	link.flags |=  MEDIA_LINK_FLAG_ENABLED;
	link.source.entity = input_v4l;
	link.source.index = 0;

	link.source.flags = MEDIA_PAD_FLAG_OUTPUT;
	link.sink.entity = mediaIn->ccdc;
	link.sink.index = 0;
	link.sink.flags = MEDIA_PAD_FLAG_INPUT;

	ret = ioctl(mediaIn->media_fd, MEDIA_IOC_SETUP_LINK, &link);
	if(ret) {
		ALOGE("Failed to enable link bewteen entities");
		close(mediaIn->media_fd);
		return -1;
	}
	memset(&link, 0, sizeof(link));
	link.flags |=  MEDIA_LINK_FLAG_ENABLED;
	link.source.entity = mediaIn->ccdc;
	link.source.index = 1;
	link.source.flags = MEDIA_PAD_FLAG_OUTPUT;
	link.sink.entity = mediaIn->video;
	link.sink.index = 0;
	link.sink.flags = MEDIA_PAD_FLAG_INPUT;
	ret = ioctl(mediaIn->media_fd, MEDIA_IOC_SETUP_LINK, &link);
	if(ret){
		ALOGE("Failed to enable link");

		close(mediaIn->media_fd);
		return -1;
	}

	/*close media device*/
	close(mediaIn->media_fd);
	return 0;
}
int V4L2Camera::Configure(int width,int height,int pixelformat,int fps)
{
	int ret = 0;
	struct v4l2_streamparm parm;

	if(version == KERNEL_VERSION(2,6))
	{
		videoIn->width = IMG_WIDTH_VGA;
		videoIn->height = IMG_HEIGHT_VGA;
		videoIn->framesizeIn =((IMG_WIDTH_VGA * IMG_HEIGHT_VGA) << 1);
		videoIn->formatIn = DEF_PIX_FMT;

		videoIn->format.fmt.pix.width =IMG_WIDTH_VGA;
		videoIn->format.fmt.pix.height =IMG_HEIGHT_VGA;
		videoIn->format.fmt.pix.pixelformat = DEF_PIX_FMT;
	}
	else
	{
		videoIn->width = width;
		videoIn->height = height;
		videoIn->framesizeIn =((width * height) << 1);
		videoIn->formatIn = pixelformat;
		videoIn->format.fmt.pix.width =width;
		videoIn->format.fmt.pix.height =height;
		videoIn->format.fmt.pix.pixelformat = pixelformat;
	}
    videoIn->format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	do
	{
		ret = ioctl(camHandle, VIDIOC_S_FMT, &videoIn->format);
		if (ret < 0) {
			ALOGE("Open: VIDIOC_S_FMT Failed: %s", strerror(errno));
			break;
		}
		ALOGD("CameraConfigure PreviewFormat: w=%d h=%d", videoIn->format.fmt.pix.width, videoIn->format.fmt.pix.height);

	}while(0);

    return ret;
}
int V4L2Camera::BufferMap()
{
    int ret;

    /* Check if camera can handle NB_BUFFER buffers */
    videoIn->rb.type	= V4L2_BUF_TYPE_VIDEO_CAPTURE;
    videoIn->rb.memory	= V4L2_MEMORY_MMAP;
    videoIn->rb.count	= NB_BUFFER;

    ret = ioctl(camHandle, VIDIOC_REQBUFS, &videoIn->rb);
    if (ret < 0) {
        ALOGE("Init: VIDIOC_REQBUFS failed: %s", strerror(errno));
        return ret;
    }

    for (int i = 0; i < NB_BUFFER; i++) {

        memset (&videoIn->buf, 0, sizeof (struct v4l2_buffer));

        videoIn->buf.index = i;
        videoIn->buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        videoIn->buf.memory = V4L2_MEMORY_MMAP;

        ret = ioctl (camHandle, VIDIOC_QUERYBUF, &videoIn->buf);
        if (ret < 0) {
            ALOGE("Init: Unable to query buffer (%s)", strerror(errno));
            return ret;
        }

        videoIn->mem[i] = mmap (0,
               videoIn->buf.length,
               PROT_READ | PROT_WRITE,
               MAP_SHARED,
               camHandle,
               videoIn->buf.m.offset);

        if (videoIn->mem[i] == MAP_FAILED) {
            ALOGE("Init: Unable to map buffer (%s)", strerror(errno));
            return -1;
        }

        ret = ioctl(camHandle, VIDIOC_QBUF, &videoIn->buf);
        if (ret < 0) {
            ALOGE("Init: VIDIOC_QBUF Failed");
            return -1;
        }

        nQueued++;
    }

    return 0;
}

void V4L2Camera::reset_links(const char *device)
{
	struct media_link_desc link;
	struct media_links_enum links;
	int ret, index, i;

	/*reset the media links*/
    mediaIn->media_fd= open(device, O_RDWR);
    for(index = 0; index < mediaIn->num_entities; index++)
    {
	    links.entity = mediaIn->entity[index].id;
	    links.pads = (struct media_pad_desc *)malloc(sizeof( struct media_pad_desc) * mediaIn->entity[index].pads);
	    links.links = (struct media_link_desc *)malloc(sizeof(struct media_link_desc) * mediaIn->entity[index].links);
	    ret = ioctl(mediaIn->media_fd, MEDIA_IOC_ENUM_LINKS, &links);
	    if (ret < 0) {
		    ALOGE("Error while enumeration links/pads - %d\n", ret);
		    break;
	    }
	    else {
	        ALOGV("Inside else");
		    for(i = 0; i < mediaIn->entity[index].links; i++) {
			    link.source.entity = links.links->source.entity;
			    link.source.index = links.links->source.index;
			    link.source.flags = MEDIA_PAD_FLAG_OUTPUT;
			    link.sink.entity = links.links->sink.entity;
			    link.sink.index = links.links->sink.index;
			    link.sink.flags = MEDIA_PAD_FLAG_INPUT;
			    link.flags = (link.flags & ~MEDIA_LINK_FLAG_ENABLED) | (link.flags & MEDIA_LINK_FLAG_IMMUTABLE);
			    ret = ioctl(mediaIn->media_fd, MEDIA_IOC_SETUP_LINK, &link);
			    if(ret)
				    break;
			    links.links++;
		    }
	    }
    }
    close (mediaIn->media_fd);
}

void V4L2Camera::Close ()
{
    close(camHandle);
    camHandle = -1;
#ifdef _OMAP_RESIZER_
    OMAPResizerClose(videoIn->resizeHandle);
    videoIn->resizeHandle = -1;
#endif //_OMAP_RESIZER_
    return;
}

int V4L2Camera::init_parm()
{
    int ret;
    int framerate;
    struct v4l2_streamparm parm;

    framerate = DEFAULT_FRAME_RATE;
    parm.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    ret = ioctl(camHandle, VIDIOC_G_PARM, &parm);
    if(ret != 0) {
        ALOGE("VIDIOC_G_PARM fail....");
        return ret;
    }

    parm.parm.capture.timeperframe.numerator = 1;
    parm.parm.capture.timeperframe.denominator = framerate;
    ret = ioctl(camHandle, VIDIOC_S_PARM, &parm);
    if(ret != 0) {
        ALOGE("VIDIOC_S_PARM  Fail....");
        return -1;
    }

    return 0;
}

void V4L2Camera::Uninit()
{
    int ret;

    videoIn->buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    videoIn->buf.memory = V4L2_MEMORY_MMAP;

    /* Dequeue everything */
    int DQcount = nQueued - nDequeued;

    for (int i = 0; i < DQcount-1; i++) {
        ret = ioctl(camHandle, VIDIOC_DQBUF, &videoIn->buf);
        if (ret < 0)
            ALOGE("Uninit: VIDIOC_DQBUF Failed");
    }
    nQueued = 0;
    nDequeued = 0;

    /* Unmap buffers */
    for (int i = 0; i < NB_BUFFER; i++)
        if (munmap(videoIn->mem[i], videoIn->buf.length) < 0)
            ALOGE("Uninit: Unmap failed");

    return;
}

int V4L2Camera::StartStreaming ()
{
    enum v4l2_buf_type type;
    int ret;

    if (!videoIn->isStreaming) {
        type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

        ret = ioctl (camHandle, VIDIOC_STREAMON, &type);
        if (ret < 0) {
            ALOGE("StartStreaming: Unable to start capture: %s", strerror(errno));
            return ret;
        }

        videoIn->isStreaming = true;
    }

    return 0;
}

int V4L2Camera::StopStreaming ()
{
    enum v4l2_buf_type type;
    int ret;

    if (videoIn->isStreaming) {
        type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

        ret = ioctl (camHandle, VIDIOC_STREAMOFF, &type);
        if (ret < 0) {
            ALOGE("StopStreaming: Unable to stop capture: %s", strerror(errno));
            return ret;
        }

        videoIn->isStreaming = false;
    }

    return 0;
}

void * V4L2Camera::GrabPreviewFrame ()
{
    int ret;

    videoIn->buf.memory = V4L2_MEMORY_MMAP;

    /* DQ */
    ret = ioctl(camHandle, VIDIOC_DQBUF, &videoIn->buf);
    if (ret < 0) {
        ALOGE("GrabPreviewFrame: VIDIOC_DQBUF Failed");
        return NULL;
    }
    nDequeued++;

    return( videoIn->mem[videoIn->buf.index] );
}

void V4L2Camera::ReleasePreviewFrame ()
{
    int ret;
    ret = ioctl(camHandle, VIDIOC_QBUF, &videoIn->buf);
    nQueued++;
    if (ret < 0) {
        ALOGE("ReleasePreviewFrame: VIDIOC_QBUF Failed");
        return;
    }
}

void V4L2Camera::GrabRawFrame(void *previewBuffer,unsigned int width, unsigned int height)
{
    int ret = 0;
    int DQcount = 0;

    videoIn->buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    videoIn->buf.memory = V4L2_MEMORY_MMAP;

    DQcount = nQueued - nDequeued;
    if(DQcount == 0)
    {
	    ALOGV("postGrabRawFrame: Drop the frame");
		ret = ioctl(camHandle, VIDIOC_QBUF, &videoIn->buf);
		if (ret < 0) {
			ALOGE("postGrabRawFrame: VIDIOC_QBUF Failed");
			return;
		}
    }

    /* DQ */
    ret = ioctl(camHandle, VIDIOC_DQBUF, &videoIn->buf);
    if (ret < 0) {
        ALOGE("GrabRawFrame: VIDIOC_DQBUF Failed");
        return;
    }
    nDequeued++;

    if(videoIn->format.fmt.pix.width != width || \
		videoIn->format.fmt.pix.height != height)
    {
	    /* do resize */
	    ALOGV("Resizing required");
#ifdef _OMAP_RESIZER_
    ret = OMAPResizerConvert(videoIn->resizeHandle, videoIn->mem[videoIn->buf.index],\
									videoIn->format.fmt.pix.height,\
									videoIn->format.fmt.pix.width,\
									previewBuffer,\
									height,\
									width);
    if(ret < 0)
	    ALOGE("Resize operation:%d",ret);
#endif //_OMAP_RESIZER_
    }
    else
    {
	    memcpy(previewBuffer, videoIn->mem[videoIn->buf.index], (size_t) videoIn->buf.bytesused);
    }

    ret = ioctl(camHandle, VIDIOC_QBUF, &videoIn->buf);
    if (ret < 0) {
        ALOGE("postGrabRawFrame: VIDIOC_QBUF Failed");
        return;
    }

    nQueued++;
}

int
V4L2Camera::savePicture(unsigned char *inputBuffer, const char * filename)
{
    FILE *output;
    int fileSize;
    int ret;
    output = fopen(filename, "wb");

    if (output == NULL) {
        ALOGE("GrabJpegFrame: Ouput file == NULL %d : ",errno);
        return 0;
    }

    fileSize = saveYUYVtoJPEG(inputBuffer, videoIn->width, videoIn->height, output, 100);

    fclose(output);
    return fileSize;
}

camera_memory_t* V4L2Camera::GrabJpegFrame (camera_request_memory mRequestMemory)
{
    FILE *output;
    FILE *input;
    int fileSize;
    int ret;
    camera_memory_t* picture = NULL;

    videoIn->buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    videoIn->buf.memory = V4L2_MEMORY_MMAP;

    /*Skip some initial frames*/
    for (int i= 0; i< 20; i++)
    {
	    ret = ioctl(camHandle, VIDIOC_DQBUF, &videoIn->buf);
	    if (ret < 0) {
		    ALOGE("GrabJpegFrame: VIDIOC_DQBUF Failed");
		    break;
	    }
	    nDequeued++;

	    /* Enqueue buffer */
	    ret = ioctl(camHandle, VIDIOC_QBUF, &videoIn->buf);
	    if (ret < 0) {
		    ALOGE("GrabJpegFrame: VIDIOC_QBUF Failed");
		    break;
	    }
	    nQueued++;
    }

    do{
	    ALOGV("Dequeue buffer");
	        /* Dequeue buffer */
		ret = ioctl(camHandle, VIDIOC_DQBUF, &videoIn->buf);
		if (ret < 0) {
			ALOGE("GrabJpegFrame: VIDIOC_DQBUF Failed");
			break;
		}
		nDequeued++;

		ALOGV("savePicture");
		fileSize = savePicture((unsigned char *)videoIn->mem[videoIn->buf.index], "/data/misc/camera/tmp.jpg");

		ALOGV("VIDIOC_QBUF");

		/* Enqueue buffer */
		ret = ioctl(camHandle, VIDIOC_QBUF, &videoIn->buf);
		if (ret < 0) {
			ALOGE("GrabJpegFrame: VIDIOC_QBUF Failed");
			break;
		}
		nQueued++;

		ALOGV("fopen temp file");
		input = fopen("/data/misc/camera/tmp.jpg", "rb");

		if (input == NULL)
			ALOGE("GrabJpegFrame: Input file == NULL");
		else {
			picture = mRequestMemory(-1,fileSize,1,NULL);
			fread((uint8_t *)picture->data, 1, fileSize, input);
			fclose(input);
		}
		break;
    }while(0);

    return picture;
}

camera_memory_t* V4L2Camera::CreateJpegFromBuffer(void *rawBuffer, camera_request_memory mRequestMemory)
{
    FILE *output;
    FILE *input;
    int fileSize;
    int ret;
    camera_memory_t* picture = NULL;

    do{
	        ALOGV("savePicture");
		fileSize = savePicture((unsigned char *)rawBuffer, "/data/misc/camera/tmp.jpg");

		ALOGV("fopen temp file");
		input = fopen("/data/misc/camera/tmp.jpg", "rb");

		if (input == NULL)
			ALOGE("GrabJpegFrame: Input file == NULL : %d ", errno);
		else {
			picture = mRequestMemory(-1,fileSize,1,NULL);
			fread((uint8_t *)picture->data, 1, fileSize, input);
			fclose(input);
		}
		break;
    }while(0);

    return picture;
}

int V4L2Camera::saveYUYVtoJPEG (unsigned char *inputBuffer, int width, int height, FILE *file, int quality)
{
    struct jpeg_compress_struct cinfo;
    struct jpeg_error_mgr jerr;
    JSAMPROW row_pointer[1];
    unsigned char *line_buffer, *yuyv;
    int z;
    int fileSize;

    line_buffer = (unsigned char *) calloc (width * 3, 1);
    yuyv = inputBuffer;

    cinfo.err = jpeg_std_error (&jerr);
    jpeg_create_compress (&cinfo);
    jpeg_stdio_dest (&cinfo, file);

    cinfo.image_width = width;
    cinfo.image_height = height;
    cinfo.input_components = 3;
    cinfo.in_color_space = JCS_RGB;

    jpeg_set_defaults (&cinfo);
    jpeg_set_quality (&cinfo, quality, TRUE);

    jpeg_start_compress (&cinfo, TRUE);

    z = 0;
    while (cinfo.next_scanline < cinfo.image_height) {
        int x;
        unsigned char *ptr = line_buffer;

        for (x = 0; x < width; x++) {
            int r, g, b;
            int y, u, v;

			if(version == KERNEL_VERSION(2,6))
			{
				if (!z)
					y = yuyv[1] << 8;
				else
					y = yuyv[3] << 8;

				u = yuyv[0] - 128;
				v = yuyv[2] - 128;
			}
			else
			{
				if (!z)
					y = yuyv[0] << 8;
				else
				y = yuyv[2] << 8;

				u = yuyv[1] - 128;
				v = yuyv[3] - 128;
			}
            r = (y + (359 * v)) >> 8;
            g = (y - (88 * u) - (183 * v)) >> 8;
            b = (y + (454 * u)) >> 8;

            *(ptr++) = (r > 255) ? 255 : ((r < 0) ? 0 : r);
            *(ptr++) = (g > 255) ? 255 : ((g < 0) ? 0 : g);
            *(ptr++) = (b > 255) ? 255 : ((b < 0) ? 0 : b);

            if (z++) {
                z = 0;
                yuyv += 4;
            }
        }

        row_pointer[0] = line_buffer;
        jpeg_write_scanlines (&cinfo, row_pointer, 1);
    }

    jpeg_finish_compress (&cinfo);
    fileSize = ftell(file);
    jpeg_destroy_compress (&cinfo);

    free (line_buffer);

    return fileSize;
}

static inline void yuv_to_rgb16(unsigned char y,
                                unsigned char u,
                                unsigned char v,
                                unsigned char *rgb)
{
    register int r,g,b;
    int rgb16;

    r = (1192 * (y - 16) + 1634 * (v - 128) ) >> 10;
    g = (1192 * (y - 16) - 833 * (v - 128) - 400 * (u -128) ) >> 10;
    b = (1192 * (y - 16) + 2066 * (u - 128) ) >> 10;

    r = r > 255 ? 255 : r < 0 ? 0 : r;
    g = g > 255 ? 255 : g < 0 ? 0 : g;
    b = b > 255 ? 255 : b < 0 ? 0 : b;

    rgb16 = (int)(((r >> 3)<<11) | ((g >> 2) << 5)| ((b >> 3) << 0));

    *rgb = (unsigned char)(rgb16 & 0xFF);
    rgb++;
    *rgb = (unsigned char)((rgb16 & 0xFF00) >> 8);

}

void V4L2Camera::convert(unsigned char *buf, unsigned char *rgb, int width, int height)
{
    int x,y,z=0;
    int blocks;

    blocks = (width * height) * 2;

    for (y = 0; y < blocks; y+=4) {
        unsigned char Y1, Y2, U, V;
		if(version == KERNEL_VERSION(2,6))
		{
			U = buf[y + 0];
			Y1 = buf[y + 1];
			V = buf[y + 2];
			Y2 = buf[y + 3];
		}
		else
		{
			Y1 = buf[y + 0];
			U = buf[y + 1];
			Y2 = buf[y + 2];
			V = buf[y + 3];
		}


        yuv_to_rgb16(Y1, U, V, &rgb[y]);
        yuv_to_rgb16(Y2, U, V, &rgb[y + 2]);
    }
}

}; // namespace android
