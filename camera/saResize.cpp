/*
 * saResize.c
 *
 * This is a resize application which shows down scaling fuctionality.
 * It reads input image of size 640x480 and resizes into 320x240 stores
 * output image.
 *
 * Copyright (C) 2009 Texas Instruments Incorporated - http://www.ti.com/
 *
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions
 *  are met:
 *
 *    Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 *    Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the
 *    distribution.
 *
 *    Neither the name of Texas Instruments Incorporated nor the names of
 *    its contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *  A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *  OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *  LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *  DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *  THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
*/

 /******************************************************************************
  Header File Inclusion
 ******************************************************************************/
#define LOG_TAG "CameraFMResize"
#include <utils/Log.h>

#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <asm/types.h>
#include <linux/videodev.h>
#include <linux/omap_resizer.h>

#define COEF(nr, dr)   ((short)(((nr)*(int)QMUL)/(dr)))
#define RDRV_RESZ_SPEC__MAX_FILTER_COEFF 32
#define QMUL  (0x100)

#define RSZDRIVER	"/dev/omap-resizer"
#define FMT		RSZ_PIX_FMT_YUYV


/* For resizing between */
short gRDRV_reszFilter4TapHighQuality[RDRV_RESZ_SPEC__MAX_FILTER_COEFF]= {
	0, 256, 0, 0, -6, 246, 16, 0, -7, 219, 44, 0, -5, 179, 83, -1, -3,
	130, 132, -3, -1, 83, 179, -5, 0, 44, 219, -7, 0, 16, 246, -6
};
short gRDRV_reszFilter7TapHighQuality[RDRV_RESZ_SPEC__MAX_FILTER_COEFF] = {
	-1, 19, 108, 112, 19, -1, 0, 0, 0, 6, 88, 126, 37, -1, 0, 0,
	0, 0, 61, 134, 61, 0, 0, 0, 0, -1, 37, 126, 88, 6, 0, 0
};

int OMAPResizerOpen()
{
	/* Open the resizer driver */
	int handle = -1;
	handle = open(RSZDRIVER, O_RDWR);
	if (handle < 0) {
		LOGE("Error opening Resizer Driver\n");
	}
	return handle;
}
void OMAPResizerClose(int handle)
{
	/* Open the resizer driver */
	close(handle);
}
int OMAPResizerConvert(int handle, void *inData,
							int inHeight,
							int inWidth,
							void *outData,
							int outHeight,
							int outWidth)
{
	struct rsz_params params;
	struct v4l2_requestbuffers creqbuf;
	struct v4l2_buffer vbuffer;

	int i, read_exp;
	int ret_val = -1;
	void *inStart;
	void *outStart;

	/* Set Parameters */
	params.in_hsize = inWidth;
	params.in_vsize = inHeight;
	params.in_pitch = params.in_hsize * 2;
	params.inptyp = RSZ_INTYPE_YCBCR422_16BIT;
	params.vert_starting_pixel = 0;
	params.horz_starting_pixel = 0;
	params.cbilin = 0;
	params.pix_fmt = FMT;
	params.out_hsize = outWidth;
	params.out_vsize = outHeight;
	params.out_pitch = params.out_hsize * 2;
	params.hstph = 0;
	params.vstph = 0;

	/* As we are downsizing, we put */
	for (i = 0; i < 32; i++)
		params.tap4filt_coeffs[i] =
			gRDRV_reszFilter4TapHighQuality[i];

	for (i = 0; i < 32; i++)
		params.tap7filt_coeffs[i] =
			gRDRV_reszFilter7TapHighQuality[i];

	params.yenh_params.type = 0;
	params.yenh_params.gain = 0;
	params.yenh_params.slop = 0;
	params.yenh_params.core = 0;

	ret_val = ioctl(handle, RSZ_S_PARAM, &params);
	if (ret_val) {
		LOGE("RSZ_S_PARAM\n");
		OMAPResizerClose(handle);
		return -1;
	}

	read_exp = 0x0;
	ret_val = ioctl(handle, RSZ_S_EXP, &read_exp);
	if (ret_val){
		LOGE("RSZ_S_EXP\n");
		OMAPResizerClose(handle);
		return -1;
	}

	creqbuf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	creqbuf.memory = V4L2_MEMORY_MMAP;
	creqbuf.count = 2;

	/* Request input buffer */
	ret_val = ioctl (handle, RSZ_REQBUF, &creqbuf);
	if (ret_val < 0) {
		LOGE("RSZ_REQBUF\n");
		OMAPResizerClose(handle);
		return -1;
	}

	vbuffer.type = creqbuf.type;
	vbuffer.memory = creqbuf.memory;
	vbuffer.index = 0;

	/* This IOCTL just updates buffer */
	ret_val = ioctl (handle, RSZ_QUERYBUF, &vbuffer);
	if (ret_val < 0) {
		LOGE("RSZ_QUERYBUF\n");
		LOGE("Index - %d\n", vbuffer.index);
		OMAPResizerClose(handle);
		return -1;
	}

	inStart = mmap(NULL, vbuffer.length, PROT_READ |
			PROT_WRITE, MAP_SHARED, handle,
			vbuffer.m.offset);
	if (inStart == MAP_FAILED) {
		LOGE("mmap error!\n");
		ret_val = -1;
		OMAPResizerClose(handle);
		return -1;
	}

	vbuffer.index = 1;

	/* This IOCTL just updates buffer */
	ret_val = ioctl (handle, RSZ_QUERYBUF, &vbuffer);
	if (ret_val) {
		LOGE("RSZ_QUERYBUF\n");
		LOGE("Index - %d\n", vbuffer.index);
		goto exit4;
	}

	outStart =	mmap(NULL, vbuffer.length, PROT_READ |
				PROT_WRITE, MAP_SHARED, handle,
				vbuffer.m.offset);
	if (outStart == MAP_FAILED) {
		LOGE("mmap error!\n");
		ret_val = -1;
		goto exit4;
	}
	/* Copying input data to input buffer */
    memcpy(inStart, inData, (size_t) inWidth*inHeight*2);

	vbuffer.type = creqbuf.type;
	vbuffer.memory = creqbuf.memory;
	vbuffer.index = 0;
	vbuffer.m.userptr = (unsigned int)inStart;

	ret_val = ioctl(handle, RSZ_QUEUEBUF, &vbuffer);
	if (ret_val < 0) {
		LOGE("RSZ_QUEUEBUF");
		goto exit5;
	}

	vbuffer.index = 1;
	vbuffer.m.userptr = (unsigned int)outStart;

	ret_val = ioctl(handle, RSZ_QUEUEBUF, &vbuffer);
	if (ret_val < 0) {
		LOGE("RSZ_QUEUEBUF");
		goto exit5;
	}

	//gettimeofday(&before, NULL);
	ret_val = ioctl(handle, RSZ_RESIZE, NULL);
	if (ret_val) {
		LOGE("RSZ_RESIZE\n");
		goto exit5;
	}

	/* Copying out buffer to out data */
	memcpy(outData, outStart, (size_t) outWidth *outHeight*2);

	ret_val = 0;

exit5:
	munmap(outStart, vbuffer.length);
exit4:
	munmap(inStart, vbuffer.length);

	return ret_val;
}
int OMAPResizerConvertDirect(int handle, void *in_start,
							int inHeight,
							int inWidth,
							void *out_start,
							int outHeight,
							int outWidth)
{
	struct rsz_params params;
	struct v4l2_requestbuffers creqbuf;
	struct v4l2_buffer vbuffer;

	int i, read_exp;
	int ret_val = -1;

	/* Set Parameters */
	params.in_hsize = inWidth;
	params.in_vsize = inHeight;
	params.in_pitch = params.in_hsize * 2;
	params.inptyp = RSZ_INTYPE_YCBCR422_16BIT;
	params.vert_starting_pixel = 0;
	params.horz_starting_pixel = 0;
	params.cbilin = 0;
	params.pix_fmt = FMT;
	params.out_hsize = outWidth;
	params.out_vsize = outHeight;
	params.out_pitch = params.out_hsize * 2;
	params.hstph = 0;
	params.vstph = 0;

	/* As we are downsizing, we put */
	for (i = 0; i < 32; i++)
		params.tap4filt_coeffs[i] =
			gRDRV_reszFilter4TapHighQuality[i];

	for (i = 0; i < 32; i++)
		params.tap7filt_coeffs[i] =
			gRDRV_reszFilter7TapHighQuality[i];

	params.yenh_params.type = 0;
	params.yenh_params.gain = 0;
	params.yenh_params.slop = 0;
	params.yenh_params.core = 0;

	ret_val = ioctl(handle, RSZ_S_PARAM, &params);
	if (ret_val) {
		LOGE("RSZ_S_PARAM:%d\n",ret_val);
		OMAPResizerClose(handle);
		return -1;
	}

	read_exp = 0x0;
	ret_val = ioctl(handle, RSZ_S_EXP, &read_exp);
	if (ret_val){
		LOGE("RSZ_S_EXP\n");
		OMAPResizerClose(handle);
		return -1;
	}

	creqbuf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	creqbuf.memory = V4L2_MEMORY_USERPTR;
	creqbuf.count = 2;

	/* Request input buffer */
	ret_val = ioctl (handle, RSZ_REQBUF, &creqbuf);
	if (ret_val < 0) {
		LOGE("Error requesting input buffer\n");
		OMAPResizerClose(handle);
		return -1;
	}

	vbuffer.type = creqbuf.type;
	vbuffer.memory = creqbuf.memory;
	vbuffer.index = 0;
	vbuffer.m.userptr = (unsigned int)in_start;


	ret_val = ioctl(handle, RSZ_QUEUEBUF, &vbuffer);
	if (ret_val) {
		LOGE("RSZ_QUEUEBUF:%d",ret_val);
		OMAPResizerClose(handle);
		return -1;
	}

	vbuffer.type = creqbuf.type;
	vbuffer.memory = creqbuf.memory;
	vbuffer.index = 1;
	vbuffer.m.userptr = (unsigned int)out_start;

	ret_val = ioctl(handle, RSZ_QUEUEBUF, &vbuffer);
	if (ret_val) {
		LOGE("RSZ_QUEUEBUF2");
		return -1;
	}

	ret_val = ioctl(handle, RSZ_RESIZE, NULL);
	if (ret_val) {
		LOGE("RSZ_RESIZE\n");
		return -1;
	}
	LOGE("RSZ_RESIZE success\n");
	return 0;
}
int OMAPResizerConvertV4l2(int handle, void *in_start,
							int inHeight,
							int inWidth,
							void *out_start,
							int outHeight,
							int outWidth)
{
	struct rsz_params params;
	struct v4l2_requestbuffers creqbuf;
	struct v4l2_buffer vbuffer;

	int i, read_exp;
	int ret_val = -1;

	/* Set Parameters */
	params.in_hsize = inWidth;
	params.in_vsize = inHeight;
	params.in_pitch = params.in_hsize * 2;
	params.inptyp = RSZ_INTYPE_YCBCR422_16BIT;
	params.vert_starting_pixel = 0;
	params.horz_starting_pixel = 0;
	params.cbilin = 0;
	params.pix_fmt = FMT;
	params.out_hsize = outWidth;
	params.out_vsize = outHeight;
	params.out_pitch = params.out_hsize * 2;
	params.hstph = 0;
	params.vstph = 0;

	/* As we are downsizing, we put */
	for (i = 0; i < 32; i++)
		params.tap4filt_coeffs[i] =
			gRDRV_reszFilter4TapHighQuality[i];

	for (i = 0; i < 32; i++)
		params.tap7filt_coeffs[i] =
			gRDRV_reszFilter7TapHighQuality[i];

	params.yenh_params.type = 0;
	params.yenh_params.gain = 0;
	params.yenh_params.slop = 0;
	params.yenh_params.core = 0;

	ret_val = ioctl(handle, RSZ_S_PARAM, &params);
	if (ret_val) {
		LOGE("RSZ_S_PARAM:%d\n",ret_val);
		OMAPResizerClose(handle);
		return -1;
	}

	read_exp = 0x0;
	ret_val = ioctl(handle, RSZ_S_EXP, &read_exp);
	if (ret_val){
		LOGE("RSZ_S_EXP\n");
		OMAPResizerClose(handle);
		return -1;
	}

	creqbuf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	creqbuf.memory = V4L2_MEMORY_MMAP;
	creqbuf.count = 1;

	/* Request input buffer */
	ret_val = ioctl (handle, RSZ_REQBUF, &creqbuf);
	if (ret_val < 0) {
		LOGE("RSZ_REQBUF\n");
		OMAPResizerClose(handle);
		return -1;
	}

	vbuffer.type = creqbuf.type;
	vbuffer.memory = creqbuf.memory;
	vbuffer.index = 0;
	vbuffer.m.userptr = (unsigned int)in_start;

	ret_val = ioctl(handle, RSZ_QUEUEBUF, &vbuffer);
		if (ret_val) {
			LOGE("RSZ_QUEUEBUF1");
			OMAPResizerClose(handle);
			return -1;
		}

	creqbuf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	creqbuf.memory = V4L2_MEMORY_USERPTR;
	creqbuf.count = 1;

	/* Request input buffer */
	ret_val = ioctl (handle, RSZ_REQBUF, &creqbuf);
	if (ret_val < 0) {
		LOGE("Error requesting input buffer\n");
		OMAPResizerClose(handle);
		return -1;
	}

	vbuffer.type = creqbuf.type;
	vbuffer.memory = creqbuf.memory;
	vbuffer.index = 1;
	vbuffer.m.userptr = (unsigned int)out_start;

	ret_val = ioctl(handle, RSZ_QUEUEBUF, &vbuffer);
	if (ret_val) {
		LOGE("RSZ_QUEUEBUF2");
		return -1;
	}

	ret_val = ioctl(handle, RSZ_RESIZE, NULL);
	if (ret_val) {
		LOGE("RSZ_RESIZE\n");
		return -1;
	}
	LOGE("RSZ_RESIZE success\n");
	return 0;
}
