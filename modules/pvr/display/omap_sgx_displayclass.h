/*************************************************************************
 *
 * Copyright(c) 2008 Imagination Technologies Ltd. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful but, except
 * as otherwise stated in writing, without any warranty; without even the
 * implied warranty of merchantability or fitness for a particular purpose.
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * The full GNU General Public License is included in this distribution in
 * the file called "COPYING".
 *
 * Contact Information:
 * Imagination Technologies Ltd. <gpl-support@imgtec.com>
 * Home Park Estate, Kings Langley, Herts, WD4 8LZ, UK
 *
 *************************************************************************/

#ifndef __OMAP_SGX_DISPLAYCLASS_H__
#define __OMAP_SGX_DISPLAYCLASS_H__

extern IMG_BOOL PVRGetDisplayClassJTable2(
	PVRSRV_DC_DISP2SRV_KMJTABLE *psJTable);

typedef void * OMAP_HANDLE;

enum OMAP_BOOL
{
	OMAP_FALSE = 0,
	OMAP_TRUE = 1,
};

struct OMAP_DISP_BUFFER
{
	unsigned long ulBufferSize;
	IMG_SYS_PHYADDR sSysAddr;
	IMG_CPU_VIRTADDR sCPUVAddr;
	PVRSRV_SYNC_DATA *psSyncData;
	struct OMAP_DISP_BUFFER *psNext;
	struct omap_display_buffer *display_buffer;
};

struct OMAP_DISP_FLIP_ITEM
{
	OMAP_HANDLE hCmdComplete;
	unsigned long ulSwapInterval;
	enum OMAP_BOOL bValid;
	enum OMAP_BOOL bFlipped;
	enum OMAP_BOOL bCmdCompleted;
	IMG_SYS_PHYADDR *sSysAddr;
	struct omap_display_buffer *display_buffer;
};

struct OMAP_DISP_SWAPCHAIN
{
	unsigned long ulBufferCount;
	struct OMAP_DISP_BUFFER *psBuffer;
	struct OMAP_DISP_FLIP_ITEM *psFlipItems;
	unsigned long ulInsertIndex;
	unsigned long ulRemoveIndex;
	PVRSRV_DC_DISP2SRV_KMJTABLE *psPVRJTable;
	enum OMAP_BOOL bFlushCommands;
	unsigned long ulSetFlushStateRefCount;
	enum OMAP_BOOL bBlanked;
	spinlock_t *psSwapChainLock;
	void *pvDevInfo;
};

struct OMAP_DISP_DEVINFO
{
	unsigned long ulDeviceID;
	struct OMAP_DISP_BUFFER sSystemBuffer;
	PVRSRV_DC_DISP2SRV_KMJTABLE sPVRJTable;
	PVRSRV_DC_SRV2DISP_KMJTABLE sDCJTable;
	struct OMAP_DISP_SWAPCHAIN *psSwapChain;
	enum OMAP_BOOL bFlushCommands;
	enum OMAP_BOOL bDeviceSuspended;
	struct mutex sSwapChainLockMutex;
	IMG_DEV_VIRTADDR sDisplayDevVAddr;
	DISPLAY_INFO sDisplayInfo;
	DISPLAY_FORMAT sDisplayFormat;
	DISPLAY_DIMS sDisplayDim;
	struct workqueue_struct *sync_display_wq;
	struct work_struct sync_display_work;
	PVRSRV_PIXEL_FORMAT ePixelFormat;
	struct omap_display_device *display;
};

enum OMAP_ERROR
{
	OMAP_OK                             =  0,
	OMAP_ERROR_GENERIC                  =  1,
	OMAP_ERROR_OUT_OF_MEMORY            =  2,
	OMAP_ERROR_TOO_FEW_BUFFERS          =  3,
	OMAP_ERROR_INVALID_PARAMS           =  4,
	OMAP_ERROR_INIT_FAILURE             =  5,
	OMAP_ERROR_CANT_REGISTER_CALLBACK   =  6,
	OMAP_ERROR_INVALID_DEVICE           =  7,
	OMAP_ERROR_DEVICE_REGISTER_FAILED   =  8

};

#define DISPLAY_DEVICE_NAME "PowerVR OMAP Display Driver"
#define	DRVNAME	"omap_sgx_displayclass"
#define	DEVNAME	DRVNAME
#define	DRIVER_PREFIX DRVNAME

#define	DEBUG_PRINTK(format, ...) printk("DEBUG " DRIVER_PREFIX \
	" (%s %i): " format "\n", __func__, __LINE__, ## __VA_ARGS__)
#define	WARNING_PRINTK(format, ...) printk("WARNING " DRIVER_PREFIX \
	" (%s %i): " format "\n", __func__, __LINE__, ## __VA_ARGS__)
#define	ERROR_PRINTK(format, ...) printk("ERROR " DRIVER_PREFIX \
	" (%s %i): " format "\n", __func__, __LINE__, ## __VA_ARGS__)

#endif
