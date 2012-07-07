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

#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/console.h>

#include <linux/module.h>
#include <linux/string.h>
#include <linux/notifier.h>

#if defined(LDM_PLATFORM)
#include <linux/platform_device.h>
#if defined(SGX_EARLYSUSPEND)
#include <linux/earlysuspend.h>
#endif
#endif

#include "img_defs.h"
#include "servicesext.h"
#include "kerneldisplay.h"
#include "omap_sgx_displayclass.h"
#include "omap_display.h"

/* XXX: Expect 2 framebuffers for virtual display */
#if (CONFIG_FB_OMAP2_NUM_FBS < 2)
#error "Virtual display is supported only with 2 or more framebuffers, \
CONFIG_FB_OMAP2_NUM_FBS must be equal or greater than 2 \
see CONFIG_FB_OMAP2_NUM_FBS for details in the kernel config"
#endif

#define OMAP_DC_CMD_COUNT		1
#define MAX_BUFFERS_FLIPPING		4

/* Pointer Display->Services */
static PFN_DC_GET_PVRJTABLE pfnGetPVRJTable = NULL;

/* Pointer to the display devices */
static struct OMAP_DISP_DEVINFO *pDisplayDevices = NULL;
static int display_devices_count = 0;

static void display_sync_handler(struct work_struct *work);
static enum OMAP_ERROR get_pvr_dc_jtable (char *szFunctionName,
	PFN_DC_GET_PVRJTABLE *ppfnFuncTable);


/*
 * Swap to display buffer. This buffer refers to one inside the
 * framebuffer memory.
 * in: hDevice, hBuffer, ui32SwapInterval, hPrivateTag, ui32ClipRectCount,
 * psClipRect
 */
static PVRSRV_ERROR SwapToDCBuffer(IMG_HANDLE hDevice,
                                   IMG_HANDLE hBuffer,
                                   IMG_UINT32 ui32SwapInterval,
                                   IMG_HANDLE hPrivateTag,
                                   IMG_UINT32 ui32ClipRectCount,
                                   IMG_RECT *psClipRect)
{
	/* Nothing to do */
	return PVRSRV_OK;
}

/*
 * Set display destination rectangle.
 * in: hDevice, hSwapChain, psRect
 */
static PVRSRV_ERROR SetDCDstRect(IMG_HANDLE hDevice,
	IMG_HANDLE hSwapChain,
	IMG_RECT *psRect)
{
	/* Nothing to do */
	return PVRSRV_ERROR_NOT_SUPPORTED;
}

/*
 * Set display source rectangle.
 * in: hDevice, hSwapChain, psRect
 */
static PVRSRV_ERROR SetDCSrcRect(IMG_HANDLE hDevice,
                                 IMG_HANDLE hSwapChain,
                                 IMG_RECT *psRect)
{
	/* Nothing to do */
	return PVRSRV_ERROR_NOT_SUPPORTED;
}

/*
 * Set display destination colour key.
 * in: hDevice, hSwapChain, ui32CKColour
 */
static PVRSRV_ERROR SetDCDstColourKey(IMG_HANDLE hDevice,
                                      IMG_HANDLE hSwapChain,
                                      IMG_UINT32 ui32CKColour)
{
	/* Nothing to do */
	return PVRSRV_ERROR_NOT_SUPPORTED;
}

/*
 * Set display source colour key.
 * in: hDevice, hSwapChain, ui32CKColour
 */
static PVRSRV_ERROR SetDCSrcColourKey(IMG_HANDLE hDevice,
                                      IMG_HANDLE hSwapChain,
                                      IMG_UINT32 ui32CKColour)
{
	/* Nothing to do */
	return PVRSRV_ERROR_NOT_SUPPORTED;
}

/*
 * Closes the display.
 * in: hDevice
 */
static PVRSRV_ERROR CloseDCDevice(IMG_HANDLE hDevice)
{
	struct OMAP_DISP_DEVINFO *psDevInfo =
		(struct OMAP_DISP_DEVINFO*) hDevice;
	struct omap_display_device *display = psDevInfo->display;

	if(display->close(display))
		WARNING_PRINTK("Unable to close properly display '%s'",
			display->name);

	return PVRSRV_OK;
}

/*
 * Flushes the sync queue present in the specified swap chain.
 * in: psSwapChain
 */
static void FlushInternalSyncQueue(struct OMAP_DISP_SWAPCHAIN *psSwapChain)
{
	struct OMAP_DISP_DEVINFO *psDevInfo =
		(struct OMAP_DISP_DEVINFO*) psSwapChain->pvDevInfo;
	struct OMAP_DISP_FLIP_ITEM *psFlipItem;
	struct omap_display_device *display = psDevInfo->display;
	unsigned long            ulMaxIndex;
	unsigned long            i;

	psFlipItem = &psSwapChain->psFlipItems[psSwapChain->ulRemoveIndex];
	ulMaxIndex = psSwapChain->ulBufferCount - 1;

	DEBUG_PRINTK("Flushing sync queue on display %lu",
		psDevInfo->ulDeviceID);
	for(i = 0; i < psSwapChain->ulBufferCount; i++)
	{
		if (psFlipItem->bValid == OMAP_FALSE)
			continue;

		DEBUG_PRINTK("Flushing swap buffer index %lu",
			psSwapChain->ulRemoveIndex);

		/* Flip the buffer if it hasn't been flipped */
		if(psFlipItem->bFlipped == OMAP_FALSE)
		{
			display->present_buffer(psFlipItem->display_buffer);
		}

		/* If the command didn't complete, assume it did */
		if(psFlipItem->bCmdCompleted == OMAP_FALSE)
		{
			DEBUG_PRINTK("Calling command complete for swap "
				"buffer index %lu",
				psSwapChain->ulRemoveIndex);
			psSwapChain->psPVRJTable->pfnPVRSRVCmdComplete(
				(IMG_HANDLE)psFlipItem->hCmdComplete,
				IMG_TRUE);
		}

		psSwapChain->ulRemoveIndex++;
		if(psSwapChain->ulRemoveIndex > ulMaxIndex)
			psSwapChain->ulRemoveIndex = 0;

		/* Put the state of the buffer to be used again later */
		psFlipItem->bFlipped = OMAP_FALSE;
		psFlipItem->bCmdCompleted = OMAP_FALSE;
		psFlipItem->bValid = OMAP_FALSE;
		psFlipItem =
			&psSwapChain->psFlipItems[psSwapChain->ulRemoveIndex];
	}

	psSwapChain->ulInsertIndex = 0;
	psSwapChain->ulRemoveIndex = 0;
}

/*
 * Sets the flush state of the specified display device
 * at the swap chain level without blocking the call.
 * in: psDevInfo, bFlushState
 */
static void SetFlushStateInternalNoLock(struct OMAP_DISP_DEVINFO* psDevInfo,
                                        enum OMAP_BOOL bFlushState)
{
	struct OMAP_DISP_SWAPCHAIN *psSwapChain = psDevInfo->psSwapChain;

	/* Nothing to do if there is no swap chain */
	if (psSwapChain == NULL){
		DEBUG_PRINTK("Swap chain is null, nothing to do for"
			" display %lu", psDevInfo->ulDeviceID);
		return;
	}

	if (bFlushState)
	{
		DEBUG_PRINTK("Desired flushState is true for display %lu",
			psDevInfo->ulDeviceID);
		if (psSwapChain->ulSetFlushStateRefCount == 0)
		{
			psSwapChain->bFlushCommands = OMAP_TRUE;
			FlushInternalSyncQueue(psSwapChain);
		}
		psSwapChain->ulSetFlushStateRefCount++;
	}
	else
	{
		DEBUG_PRINTK("Desired flushState is false for display %lu",
			psDevInfo->ulDeviceID);
		if (psSwapChain->ulSetFlushStateRefCount != 0)
		{
			psSwapChain->ulSetFlushStateRefCount--;
			if (psSwapChain->ulSetFlushStateRefCount == 0)
			{
				psSwapChain->bFlushCommands = OMAP_FALSE;
			}
		}
	}
}

/*
 * Sets the flush state of the specified display device
 * at device level blocking the call if needed.
 * in: psDevInfo, bFlushState
 */
static void SetFlushStateExternal(struct OMAP_DISP_DEVINFO* psDevInfo,
                                  enum OMAP_BOOL bFlushState)
{
	DEBUG_PRINTK("Executing for display %lu",
		psDevInfo->ulDeviceID);
	mutex_lock(&psDevInfo->sSwapChainLockMutex);
	if (psDevInfo->bFlushCommands != bFlushState)
	{
		psDevInfo->bFlushCommands = bFlushState;
		SetFlushStateInternalNoLock(psDevInfo, bFlushState);
	}
	mutex_unlock(&psDevInfo->sSwapChainLockMutex);
}

/*
 * Opens the display.
 * in: ui32DeviceID, phDevice
 * out: psSystemBufferSyncData
 */
static PVRSRV_ERROR OpenDCDevice(IMG_UINT32 ui32DeviceID,
                                 IMG_HANDLE *phDevice,
                                 PVRSRV_SYNC_DATA* psSystemBufferSyncData)
{
	struct OMAP_DISP_DEVINFO *psDevInfo;
	struct omap_display_device *display;
	int i;

	psDevInfo = 0;
	for(i = 0; i < display_devices_count; i++)
	{
		if(ui32DeviceID == (&pDisplayDevices[i])->ulDeviceID)
		{
			psDevInfo = &pDisplayDevices[i];
			break;
		}
	}

	if(!psDevInfo)
	{
		WARNING_PRINTK("Unable to identify display device with id %i",
			(int)ui32DeviceID);
		return OMAP_ERROR_INVALID_DEVICE;
	}

	psDevInfo->sSystemBuffer.psSyncData = psSystemBufferSyncData;
	display = psDevInfo->display;

	DEBUG_PRINTK("Opening display %lu '%s'",psDevInfo->ulDeviceID,
		display->name);

	/* TODO: Explain here why ORIENTATION_VERTICAL is used*/
	if(display->open(display, ORIENTATION_VERTICAL | ORIENTATION_INVERT))
		ERROR_PRINTK("Unable to open properly display '%s'",
			psDevInfo->display->name);

	display->present_buffer(display->main_buffer);

	/* TODO: Turn on display here? */

	*phDevice = (IMG_HANDLE)psDevInfo;

	return PVRSRV_OK;
}

/*
 * Gets the available formats for the display.
 * in: hDevice
 * out: pui32NumFormats, psFormat
 */
static PVRSRV_ERROR EnumDCFormats(IMG_HANDLE hDevice,
                                  IMG_UINT32 *pui32NumFormats,
                                  DISPLAY_FORMAT *psFormat)
{
	struct OMAP_DISP_DEVINFO *psDevInfo;
	if(!hDevice || !pui32NumFormats)
	{
		ERROR_PRINTK("Invalid parameters");
		return PVRSRV_ERROR_INVALID_PARAMS;
	}

	psDevInfo = (struct OMAP_DISP_DEVINFO*)hDevice;
	*pui32NumFormats = 1;

	if(psFormat)
		psFormat[0] = psDevInfo->sDisplayFormat;
	else
		WARNING_PRINTK("Display format is null for"
			" display %lu", psDevInfo->ulDeviceID);

	return PVRSRV_OK;
}

/*
 * Gets the available dimensions for the display.
 * in: hDevice, psFormat
 * out: pui32NumDims, psDim
 */
static PVRSRV_ERROR EnumDCDims(IMG_HANDLE hDevice,
                               DISPLAY_FORMAT *psFormat,
                               IMG_UINT32 *pui32NumDims,
                               DISPLAY_DIMS *psDim)
{
	struct OMAP_DISP_DEVINFO *psDevInfo;
	if(!hDevice || !psFormat || !pui32NumDims)
	{
		ERROR_PRINTK("Invalid parameters");
		return PVRSRV_ERROR_INVALID_PARAMS;
	}

	psDevInfo = (struct OMAP_DISP_DEVINFO*)hDevice;
	*pui32NumDims = 1;

	if(psDim)
		psDim[0] = psDevInfo->sDisplayDim;
	else
		WARNING_PRINTK("Display dimensions are null for"
			" display %lu", psDevInfo->ulDeviceID);

	return PVRSRV_OK;
}

/*
 * Gets the display framebuffer physical address.
 * in: hDevice
 * out: phBuffer
 */
static PVRSRV_ERROR GetDCSystemBuffer(IMG_HANDLE hDevice, IMG_HANDLE *phBuffer)
{
	struct OMAP_DISP_DEVINFO *psDevInfo;

	if(!hDevice || !phBuffer)
	{
		ERROR_PRINTK("Invalid parameters");
		return PVRSRV_ERROR_INVALID_PARAMS;
	}

	psDevInfo = (struct OMAP_DISP_DEVINFO*)hDevice;
	*phBuffer = (IMG_HANDLE)&psDevInfo->sSystemBuffer;

	return PVRSRV_OK;
}

/*
 * Gets the display general information.
 * in: hDevice
 * out: psDCInfo
 */
static PVRSRV_ERROR GetDCInfo(IMG_HANDLE hDevice, DISPLAY_INFO *psDCInfo)
{
	struct OMAP_DISP_DEVINFO *psDevInfo;

	if(!hDevice || !psDCInfo)
	{
		ERROR_PRINTK("Invalid parameters");
		return PVRSRV_ERROR_INVALID_PARAMS;
	}

	psDevInfo = (struct OMAP_DISP_DEVINFO*)hDevice;
	*psDCInfo = psDevInfo->sDisplayInfo;

	return PVRSRV_OK;
}

/*
 * Gets the display framebuffer virtual address.
 * in: hDevice
 * out: ppsSysAddr, pui32ByteSize, ppvCpuVAddr, phOSMapInfo, pbIsContiguous
 */
static PVRSRV_ERROR GetDCBufferAddr(IMG_HANDLE        hDevice,
                                    IMG_HANDLE        hBuffer,
                                    IMG_SYS_PHYADDR   **ppsSysAddr,
                                    IMG_UINT32        *pui32ByteSize,
                                    IMG_VOID          **ppvCpuVAddr,
                                    IMG_HANDLE        *phOSMapInfo,
                                    IMG_BOOL          *pbIsContiguous,
	                            IMG_UINT32        *pui32TilingStride)
{
	struct OMAP_DISP_DEVINFO *psDevInfo;
	struct OMAP_DISP_BUFFER *psSystemBuffer;

	if(!hDevice || !hBuffer || !ppsSysAddr || !pui32ByteSize )
	{
		ERROR_PRINTK("Invalid parameters");
		return PVRSRV_ERROR_INVALID_PARAMS;
	}

	psDevInfo = (struct OMAP_DISP_DEVINFO*)hDevice;
	psSystemBuffer = (struct OMAP_DISP_BUFFER *)hBuffer;
	*ppsSysAddr = &psSystemBuffer->sSysAddr;
	*pui32ByteSize = (IMG_UINT32)psDevInfo->sSystemBuffer.ulBufferSize;

	if (ppvCpuVAddr)
		*ppvCpuVAddr = psSystemBuffer->sCPUVAddr;

	if (phOSMapInfo)
		*phOSMapInfo = (IMG_HANDLE)0;

	if (pbIsContiguous)
		*pbIsContiguous = IMG_TRUE;

	return PVRSRV_OK;
}

/*
 * Creates a swap chain. Called when a 3D application begins.
 * in: hDevice, ui32Flags, ui32BufferCount, psDstSurfAttrib, psSrcSurfAttrib
 * ui32OEMFlags
 * out: phSwapChain, ppsSyncData, pui32SwapChainID
 */
static PVRSRV_ERROR CreateDCSwapChain(IMG_HANDLE hDevice,
                                      IMG_UINT32 ui32Flags,
                                      DISPLAY_SURF_ATTRIBUTES *psDstSurfAttrib,
                                      DISPLAY_SURF_ATTRIBUTES *psSrcSurfAttrib,
                                      IMG_UINT32 ui32BufferCount,
                                      PVRSRV_SYNC_DATA **ppsSyncData,
                                      IMG_UINT32 ui32OEMFlags,
                                      IMG_HANDLE *phSwapChain,
                                      IMG_UINT32 *pui32SwapChainID)
{
	struct OMAP_DISP_DEVINFO *psDevInfo;
	struct OMAP_DISP_SWAPCHAIN *psSwapChain;
	struct OMAP_DISP_BUFFER *psBuffer;
	struct OMAP_DISP_FLIP_ITEM *psFlipItems;
	IMG_UINT32 i;
	PVRSRV_ERROR eError;
	IMG_UINT32 ui32BuffersToSkip;
	struct omap_display_device *display;
	int err;

	if(!hDevice || !psDstSurfAttrib || !psSrcSurfAttrib ||
		!ppsSyncData || !phSwapChain)
	{
		ERROR_PRINTK("Invalid parameters");
		return PVRSRV_ERROR_INVALID_PARAMS;
	}
	psDevInfo = (struct OMAP_DISP_DEVINFO*)hDevice;

	if (psDevInfo->sDisplayInfo.ui32MaxSwapChains == 0)
	{
		ERROR_PRINTK("Unable to operate with 0 MaxSwapChains for"
			" display %lu", psDevInfo->ulDeviceID);
		return PVRSRV_ERROR_NOT_SUPPORTED;
	}

	if(psDevInfo->psSwapChain != NULL)
	{
		ERROR_PRINTK("Swap chain already exists for"
			" display %lu", psDevInfo->ulDeviceID);
		return PVRSRV_ERROR_FLIP_CHAIN_EXISTS;
	}

	if(ui32BufferCount > psDevInfo->sDisplayInfo.ui32MaxSwapChainBuffers)
	{
		ERROR_PRINTK("Too many buffers. Trying to use %u buffers while"
			" there is only %u available for display %lu",
			(unsigned int)ui32BufferCount,
			(unsigned int)psDevInfo->
			sDisplayInfo.ui32MaxSwapChainBuffers,
			psDevInfo->ulDeviceID);
		return PVRSRV_ERROR_TOOMANYBUFFERS;
	}

	ui32BuffersToSkip = psDevInfo->sDisplayInfo.ui32MaxSwapChainBuffers -
		ui32BufferCount;

	if((psDstSurfAttrib->pixelformat !=
		psDevInfo->sDisplayFormat.pixelformat) ||
		(psDstSurfAttrib->sDims.ui32ByteStride !=
		psDevInfo->sDisplayDim.ui32ByteStride) ||
		(psDstSurfAttrib->sDims.ui32Width !=
		psDevInfo->sDisplayDim.ui32Width) ||
		(psDstSurfAttrib->sDims.ui32Height !=
		psDevInfo->sDisplayDim.ui32Height))
	{
		ERROR_PRINTK("Destination surface attributes differ from the"
			" current framebuffer for display %lu",
			psDevInfo->ulDeviceID);
		return PVRSRV_ERROR_INVALID_PARAMS;
	}

	if((psDstSurfAttrib->pixelformat !=
		psSrcSurfAttrib->pixelformat) ||
		(psDstSurfAttrib->sDims.ui32ByteStride !=
		psSrcSurfAttrib->sDims.ui32ByteStride) ||
		(psDstSurfAttrib->sDims.ui32Width !=
		psSrcSurfAttrib->sDims.ui32Width) ||
		(psDstSurfAttrib->sDims.ui32Height !=
		psSrcSurfAttrib->sDims.ui32Height))
	{
		ERROR_PRINTK("Destination surface attributes differ from the"
			" target destination surface for display %lu",
			psDevInfo->ulDeviceID);
		return PVRSRV_ERROR_INVALID_PARAMS;
	}

	/* Create the flip chain in display side */
	display = psDevInfo->display;
	/* TODO: What about TILER buffers? */
	/*
	 * Creating the flip chain with the maximum number of buffers
	 * we will decide which ones will be used later
	 */
	err = display->create_flip_chain(
		display, psDevInfo->sDisplayInfo.ui32MaxSwapChainBuffers);
	if(err)
	{
		ERROR_PRINTK("Unable to create the flip chain for '%s' display"
			" id %lu", display->name, psDevInfo->ulDeviceID);
		return PVRSRV_ERROR_INVALID_PARAMS;
	}

	/* Allocate memory needed for the swap chain */
	psSwapChain = (struct OMAP_DISP_SWAPCHAIN*) kmalloc(
		sizeof(struct OMAP_DISP_SWAPCHAIN), GFP_KERNEL);
	if(!psSwapChain)
	{
		ERROR_PRINTK("Out of memory to allocate swap chain for"
			" display %lu", psDevInfo->ulDeviceID);
		return PVRSRV_ERROR_OUT_OF_MEMORY;
	}

	DEBUG_PRINTK("Creating swap chain for display %lu",
		psDevInfo->ulDeviceID );

	/* Allocate memory for the buffer abstraction structures */
	psBuffer = (struct OMAP_DISP_BUFFER*) kmalloc(
		sizeof(struct OMAP_DISP_BUFFER) * ui32BufferCount, GFP_KERNEL);
	if(!psBuffer)
	{
		ERROR_PRINTK("Out of memory to allocate the buffer"
			" abstraction structures for display %lu",
			psDevInfo->ulDeviceID);
		eError = PVRSRV_ERROR_OUT_OF_MEMORY;
		goto ErrorFreeSwapChain;
	}

	/* Allocate memory for the flip item abstraction structures */
	psFlipItems = (struct OMAP_DISP_FLIP_ITEM *) kmalloc (
		sizeof(struct OMAP_DISP_FLIP_ITEM) * ui32BufferCount,
		GFP_KERNEL);
	if (!psFlipItems)
	{
		ERROR_PRINTK("Out of memory to allocate the flip item"
			" abstraction structures for display %lu",
			psDevInfo->ulDeviceID);
		eError = PVRSRV_ERROR_OUT_OF_MEMORY;
		goto ErrorFreeBuffers;
	}

	/* Assign to the swap chain structure the initial data */
	psSwapChain->ulBufferCount = (unsigned long)ui32BufferCount;
	psSwapChain->psBuffer = psBuffer;
	psSwapChain->psFlipItems = psFlipItems;
	psSwapChain->ulInsertIndex = 0;
	psSwapChain->ulRemoveIndex = 0;
	psSwapChain->psPVRJTable = &psDevInfo->sPVRJTable;
	psSwapChain->pvDevInfo = (void*)psDevInfo;

	/*
	 * Init the workqueue (single thread, freezable and real time)
	 * and its own work for this display
	 */
	INIT_WORK(&psDevInfo->sync_display_work, display_sync_handler);
	psDevInfo->sync_display_wq =
		__create_workqueue("pvr_display_sync_wq", 1, 1, 1);

	DEBUG_PRINTK("Swap chain will have %u buffers for display %lu",
		(unsigned int)ui32BufferCount, psDevInfo->ulDeviceID);
	/* Link the buffers available like a circular list */
	for(i=0; i<ui32BufferCount-1; i++)
	{
		psBuffer[i].psNext = &psBuffer[i+1];
	}
	psBuffer[i].psNext = &psBuffer[0];

	/* Initialize each buffer abstraction structure */
	for(i=0; i<ui32BufferCount; i++)
	{
		/* Get the needed buffers from the display flip chain */
		IMG_UINT32 ui32SwapBuffer = i + ui32BuffersToSkip;
		struct omap_display_buffer * flip_buffer =
			display->flip_chain->buffers[ui32SwapBuffer];
		psBuffer[i].display_buffer = flip_buffer;
		psBuffer[i].psSyncData = ppsSyncData[i];
		psBuffer[i].sSysAddr.uiAddr = flip_buffer->physical_addr;
		psBuffer[i].sCPUVAddr =
			(IMG_CPU_VIRTADDR) flip_buffer->virtual_addr;
		DEBUG_PRINTK("Display %lu buffer index %u has physical "
			"address 0x%x",
			psDevInfo->ulDeviceID,
			(unsigned int)i,
			(unsigned int)psBuffer[i].sSysAddr.uiAddr);
	}

	/* Initialize each flip item abstraction structure */
	for(i=0; i<ui32BufferCount; i++)
	{
		psFlipItems[i].bValid = OMAP_FALSE;
		psFlipItems[i].bFlipped = OMAP_FALSE;
		psFlipItems[i].bCmdCompleted = OMAP_FALSE;
		psFlipItems[i].display_buffer = 0;
	}

	mutex_lock(&psDevInfo->sSwapChainLockMutex);

	psDevInfo->psSwapChain = psSwapChain;
	psSwapChain->bFlushCommands = psDevInfo->bFlushCommands;
	if (psSwapChain->bFlushCommands)
		psSwapChain->ulSetFlushStateRefCount = 1;
	else
		psSwapChain->ulSetFlushStateRefCount = 0;

	mutex_unlock(&psDevInfo->sSwapChainLockMutex);

	*phSwapChain = (IMG_HANDLE)psSwapChain;

	return PVRSRV_OK;

ErrorFreeBuffers:
	kfree(psBuffer);
ErrorFreeSwapChain:
	kfree(psSwapChain);

	return eError;
}

/*
 * Destroy a swap chain. Called when a 3D application ends.
 * in: hDevice, hSwapChain
 */
static PVRSRV_ERROR DestroyDCSwapChain(IMG_HANDLE hDevice,
	IMG_HANDLE hSwapChain)
{
	struct OMAP_DISP_DEVINFO *psDevInfo;
	struct OMAP_DISP_SWAPCHAIN *psSwapChain;
	struct omap_display_device *display;
	int err;

	if(!hDevice || !hSwapChain)
	{
		ERROR_PRINTK("Invalid parameters");
		return PVRSRV_ERROR_INVALID_PARAMS;
	}

	psDevInfo = (struct OMAP_DISP_DEVINFO*)hDevice;
	psSwapChain = (struct OMAP_DISP_SWAPCHAIN*)hSwapChain;
	display = psDevInfo->display;

	if (psSwapChain != psDevInfo->psSwapChain)
	{
		ERROR_PRINTK("Swap chain handler differs from the one "
			"present in the display device pointer");
		return PVRSRV_ERROR_INVALID_PARAMS;
	}

	DEBUG_PRINTK("Destroying swap chain for display %lu",
		psDevInfo->ulDeviceID);

	mutex_lock(&psDevInfo->sSwapChainLockMutex);

	FlushInternalSyncQueue(psSwapChain);
	psDevInfo->psSwapChain = NULL;

	/*
	 * Present the buffer which is at the base of address of
	 * the framebuffer
	 */
	display->present_buffer(display->main_buffer);

	/* Destroy the flip chain in display side */
	err = display->destroy_flip_chain(display);
	if(err)
	{
		ERROR_PRINTK("Unable to destroy the flip chain for '%s' "
			"display id %lu", display->name,
			psDevInfo->ulDeviceID);
	}

	mutex_unlock(&psDevInfo->sSwapChainLockMutex);

	/* Destroy the workqueue */
	flush_workqueue(psDevInfo->sync_display_wq);
	destroy_workqueue(psDevInfo->sync_display_wq);

	kfree(psSwapChain->psFlipItems);
	kfree(psSwapChain->psBuffer);
	kfree(psSwapChain);

	return PVRSRV_OK;
}


/*
 * Get display buffers. These are the buffers that can be allocated
 * inside the framebuffer memory.
 * in: hDevice, hSwapChain
 * out: pui32BufferCount, phBuffer
 */
static PVRSRV_ERROR GetDCBuffers(IMG_HANDLE hDevice,
                                 IMG_HANDLE hSwapChain,
                                 IMG_UINT32 *pui32BufferCount,
                                 IMG_HANDLE *phBuffer)
{
	struct OMAP_DISP_DEVINFO *psDevInfo;
	struct OMAP_DISP_SWAPCHAIN *psSwapChain;
	unsigned long      i;

	if(!hDevice || !hSwapChain || !pui32BufferCount || !phBuffer)
	{
		ERROR_PRINTK("Invalid parameters");
		return PVRSRV_ERROR_INVALID_PARAMS;
	}

	psDevInfo = (struct OMAP_DISP_DEVINFO*)hDevice;
	psSwapChain = (struct OMAP_DISP_SWAPCHAIN*)hSwapChain;
	if (psSwapChain != psDevInfo->psSwapChain)
	{
		ERROR_PRINTK("Swap chain handler differs from the one "
			"present in the display device %lu pointer",
			psDevInfo->ulDeviceID);
		return PVRSRV_ERROR_INVALID_PARAMS;
	}
	*pui32BufferCount = (IMG_UINT32)psSwapChain->ulBufferCount;

	for(i=0; i<psSwapChain->ulBufferCount; i++)
		phBuffer[i] = (IMG_HANDLE)&psSwapChain->psBuffer[i];

	return PVRSRV_OK;
}

/*
 * Sets the display state.
 * in: ui32State, hDevice
 */
static IMG_VOID SetDCState(IMG_HANDLE hDevice, IMG_UINT32 ui32State)
{
	struct OMAP_DISP_DEVINFO *psDevInfo =
		(struct OMAP_DISP_DEVINFO*) hDevice;

	switch (ui32State)
	{
		case DC_STATE_FLUSH_COMMANDS:
			DEBUG_PRINTK("Setting state to flush commands for"
				" display %lu", psDevInfo->ulDeviceID);
			SetFlushStateExternal(psDevInfo, OMAP_TRUE);
			break;
		case DC_STATE_NO_FLUSH_COMMANDS:
			DEBUG_PRINTK("Setting state to not flush commands for"
				" display %lu", psDevInfo->ulDeviceID);
			SetFlushStateExternal(psDevInfo, OMAP_FALSE);
			break;
		default:
			WARNING_PRINTK("Unknown command state %u for display"
				" %lu", (unsigned int)ui32State,
				psDevInfo->ulDeviceID);
			break;
	}
}

/*
 * Swap to display system buffer. This buffer refers to the one which
 * is that fits in the framebuffer memory.
 * in: hDevice, hSwapChain
 */
static PVRSRV_ERROR SwapToDCSystem(IMG_HANDLE hDevice,
                                   IMG_HANDLE hSwapChain)
{
	struct OMAP_DISP_DEVINFO *psDevInfo;
	struct OMAP_DISP_SWAPCHAIN *psSwapChain;
	struct omap_display_device *display;

	if(!hDevice || !hSwapChain)
	{
		ERROR_PRINTK("Invalid parameters");
		return PVRSRV_ERROR_INVALID_PARAMS;
	}

	psDevInfo = (struct OMAP_DISP_DEVINFO*)hDevice;
	psSwapChain = (struct OMAP_DISP_SWAPCHAIN*)hSwapChain;
	display = psDevInfo->display;

	DEBUG_PRINTK("Executing for display %lu",
		psDevInfo->ulDeviceID);

	if (psSwapChain != psDevInfo->psSwapChain)
	{
		ERROR_PRINTK("Swap chain handler differs from the one "
			"present in the display device %lu pointer",
			psDevInfo->ulDeviceID);
		return PVRSRV_ERROR_INVALID_PARAMS;
	}

	mutex_lock(&psDevInfo->sSwapChainLockMutex);

	FlushInternalSyncQueue(psSwapChain);
	display->present_buffer(display->main_buffer);

	mutex_unlock(&psDevInfo->sSwapChainLockMutex);

	return PVRSRV_OK;
}

/*
 * Handles the synchronization with the display
 * in: work
 */

static void display_sync_handler(struct work_struct *work)
{
	/*
	 * TODO: Since present_buffer_sync waits and then present, this
	 * algorithm can be simplified further
	 */
	struct OMAP_DISP_DEVINFO *psDevInfo = container_of(work,
		struct OMAP_DISP_DEVINFO, sync_display_work);
	struct omap_display_device *display = psDevInfo->display;
	struct OMAP_DISP_FLIP_ITEM *psFlipItem;
	struct OMAP_DISP_SWAPCHAIN *psSwapChain;
	unsigned long ulMaxIndex;

	mutex_lock(&psDevInfo->sSwapChainLockMutex);

	psSwapChain = psDevInfo->psSwapChain;
	if (!psSwapChain || psSwapChain->bFlushCommands)
		goto ExitUnlock;

	psFlipItem = &psSwapChain->psFlipItems[psSwapChain->ulRemoveIndex];
	ulMaxIndex = psSwapChain->ulBufferCount - 1;

	/* Iterate through the flip items and flip them if necessary */
	while(psFlipItem->bValid)
	{
		if(psFlipItem->bFlipped)
		{
			if(!psFlipItem->bCmdCompleted)
			{
				psSwapChain->psPVRJTable->pfnPVRSRVCmdComplete(
					(IMG_HANDLE)psFlipItem->hCmdComplete,
					IMG_TRUE);
				psFlipItem->bCmdCompleted = OMAP_TRUE;
			}

			if(psFlipItem->ulSwapInterval == 0)
			{
				psSwapChain->ulRemoveIndex++;
				if(psSwapChain->ulRemoveIndex > ulMaxIndex)
					psSwapChain->ulRemoveIndex = 0;
				psFlipItem->bCmdCompleted = OMAP_FALSE;
				psFlipItem->bFlipped = OMAP_FALSE;
				psFlipItem->bValid = OMAP_FALSE;
			}
			else
			{
				/*
				 * Here the swap interval is not zero yet
				 * we need to schedule another work until
				 * it reaches zero
				 */
				display->sync(display);
				psFlipItem->ulSwapInterval--;
				queue_work(psDevInfo->sync_display_wq,
					&psDevInfo->sync_display_work);
				goto ExitUnlock;
			}
		}
		else
		{
			display->present_buffer_sync(
				psFlipItem->display_buffer);
			/*
			 * present_buffer_sync waits and then present, then
			 * swap interval decreases here too.
			 */
			psFlipItem->ulSwapInterval--;
			psFlipItem->bFlipped = OMAP_TRUE;
			/*
			 * If the flip has been presented here then we need
			 * in the next sync execute the command complete,
			 * schedule another work
			 */
			queue_work(psDevInfo->sync_display_wq,
				&psDevInfo->sync_display_work);
			goto ExitUnlock;
		}
		psFlipItem =
			&psSwapChain->psFlipItems[psSwapChain->ulRemoveIndex];
	}

ExitUnlock:
	mutex_unlock(&psDevInfo->sSwapChainLockMutex);
}

/*
 * Performs a flip. This function takes the necessary steps to present
 * the buffer to be flipped in the display.
 * in: hCmdCookie, ui32DataSize, pvData
 */
static IMG_BOOL ProcessFlip(IMG_HANDLE  hCmdCookie,
                            IMG_UINT32  ui32DataSize,
                            IMG_VOID   *pvData)
{
	DISPLAYCLASS_FLIP_COMMAND *psFlipCmd;
	struct OMAP_DISP_DEVINFO *psDevInfo;
	struct OMAP_DISP_BUFFER *psBuffer;
	struct OMAP_DISP_SWAPCHAIN *psSwapChain;
	struct omap_display_device *display;
#if defined(SYS_USING_INTERRUPTS)
	struct OMAP_DISP_FLIP_ITEM* psFlipItem;
#endif

	if(!hCmdCookie || !pvData)
	{
		WARNING_PRINTK("Ignoring call with NULL parameters");
		return IMG_FALSE;
	}

	psFlipCmd = (DISPLAYCLASS_FLIP_COMMAND*)pvData;

	if (psFlipCmd == IMG_NULL ||
		sizeof(DISPLAYCLASS_FLIP_COMMAND) != ui32DataSize)
	{
		WARNING_PRINTK("NULL command or command data size is wrong");
		return IMG_FALSE;
	}

	psDevInfo = (struct OMAP_DISP_DEVINFO*)psFlipCmd->hExtDevice;
	psBuffer = (struct OMAP_DISP_BUFFER*)psFlipCmd->hExtBuffer;
	psSwapChain = (struct OMAP_DISP_SWAPCHAIN*) psFlipCmd->hExtSwapChain;
	display = psDevInfo->display;

	mutex_lock(&psDevInfo->sSwapChainLockMutex);

	if (psDevInfo->bDeviceSuspended)
	{
		/* If is suspended then assume the commands are completed */
		psSwapChain->psPVRJTable->pfnPVRSRVCmdComplete(
			hCmdCookie, IMG_TRUE);
		goto ExitTrueUnlock;
	}

#if defined(SYS_USING_INTERRUPTS)

	if( psFlipCmd->ui32SwapInterval == 0 ||
		psSwapChain->bFlushCommands == OMAP_TRUE)
	{
#endif
		display->present_buffer(psBuffer->display_buffer);
		psSwapChain->psPVRJTable->pfnPVRSRVCmdComplete(
			hCmdCookie, IMG_TRUE);

#if defined(SYS_USING_INTERRUPTS)
		goto ExitTrueUnlock;
	}

	psFlipItem = &psSwapChain->psFlipItems[psSwapChain->ulInsertIndex];

	if(psFlipItem->bValid == OMAP_FALSE)
	{
		unsigned long ulMaxIndex = psSwapChain->ulBufferCount - 1;

		psFlipItem->bFlipped = OMAP_FALSE;

		/*
		 * The buffer is queued here, must be consumed by the workqueue
		 */
		psFlipItem->hCmdComplete = (OMAP_HANDLE)hCmdCookie;
		psFlipItem->ulSwapInterval =
			(unsigned long)psFlipCmd->ui32SwapInterval;
		psFlipItem->sSysAddr = &psBuffer->sSysAddr;
		psFlipItem->bValid = OMAP_TRUE;
		psFlipItem->display_buffer = psBuffer->display_buffer;

		psSwapChain->ulInsertIndex++;
		if(psSwapChain->ulInsertIndex > ulMaxIndex)
			psSwapChain->ulInsertIndex = 0;

		/* Give work to the workqueue to sync with the display */
		queue_work(psDevInfo->sync_display_wq,
			&psDevInfo->sync_display_work);

		goto ExitTrueUnlock;
	}

	mutex_unlock(&psDevInfo->sSwapChainLockMutex);
	return IMG_FALSE;
#endif

ExitTrueUnlock:
	mutex_unlock(&psDevInfo->sSwapChainLockMutex);
	return IMG_TRUE;
}

#if defined(LDM_PLATFORM)

/*
 *  Function called when the driver must suspend
 */
static void DriverSuspend(void)
{
	struct OMAP_DISP_DEVINFO *psDevInfo;
	int i;

	if(!pDisplayDevices)
		return;

	for(i = 0; i < display_devices_count; i++)
	{
		psDevInfo = &pDisplayDevices[i];

		mutex_lock(&psDevInfo->sSwapChainLockMutex);

		if (psDevInfo->bDeviceSuspended)
		{
			mutex_unlock(&psDevInfo->sSwapChainLockMutex);
			continue;
		}

		psDevInfo->bDeviceSuspended = OMAP_TRUE;
		SetFlushStateInternalNoLock(psDevInfo, OMAP_TRUE);

		mutex_unlock(&psDevInfo->sSwapChainLockMutex);
	}
}

/*
 *  Function called when the driver must resume
 */
static void DriverResume(void)
{
	struct OMAP_DISP_DEVINFO *psDevInfo;
	int i;

	if(!pDisplayDevices)
		return;

	for(i = 0; i < display_devices_count; i++)
	{
		psDevInfo = &pDisplayDevices[i];

		mutex_lock(&psDevInfo->sSwapChainLockMutex);

		if (!psDevInfo->bDeviceSuspended)
		{
			mutex_unlock(&psDevInfo->sSwapChainLockMutex);
			continue;
		}

		SetFlushStateInternalNoLock(psDevInfo, OMAP_FALSE);
		psDevInfo->bDeviceSuspended = OMAP_FALSE;

		mutex_unlock(&psDevInfo->sSwapChainLockMutex);
	}
}
#endif /* defined(LDM_PLATFORM) */

/*
 * Frees the kernel framebuffer
 * in: psDevInfo
 */
static void deinit_display_device(struct OMAP_DISP_DEVINFO *psDevInfo)
{
	/* TODO: Are we sure there is nothing to do here? */
}

/*
 *  Deinitialization routine for the 3rd party display driver
 */
static enum OMAP_ERROR destroy_display_devices(void)
{
	struct OMAP_DISP_DEVINFO *psDevInfo;
	PVRSRV_DC_DISP2SRV_KMJTABLE *psJTable;
	int i;

	DEBUG_PRINTK("Deinitializing 3rd party display driver");

	if(!pDisplayDevices)
		return OMAP_OK;

	for(i = 0; i < display_devices_count; i++)
	{
		psDevInfo = &pDisplayDevices[i];
		if(!psDevInfo->display)
			continue;

		/* Remove the ProcessFlip command callback */
		psJTable = &psDevInfo->sPVRJTable;

		if(!psJTable)
			continue;

		if (psDevInfo->sPVRJTable.pfnPVRSRVRemoveCmdProcList(
			psDevInfo->ulDeviceID,
			OMAP_DC_CMD_COUNT) != PVRSRV_OK)
		{
			ERROR_PRINTK("Unable to remove callback for "
				"ProcessFlip command for display %lu",
				psDevInfo->ulDeviceID);
			return OMAP_ERROR_GENERIC;
		}

		/* Remove the display device from services */
		if (psJTable->pfnPVRSRVRemoveDCDevice(
			psDevInfo->ulDeviceID) != PVRSRV_OK)
		{
			ERROR_PRINTK("Unable to remove the display %lu "
				"from services", psDevInfo->ulDeviceID);
			return OMAP_ERROR_GENERIC;
		}

		deinit_display_device(psDevInfo);
	}

	kfree(pDisplayDevices);

	return OMAP_OK;
}

/*
 * Extracts the framebuffer data from the kernel driver
 * in: psDevInfo
 */
static enum OMAP_ERROR init_display_device(struct OMAP_DISP_DEVINFO *psDevInfo,
	struct omap_display_device *display)
{
	int buffers_available = display->buffers_available;

	/* Extract the needed data from the display struct */
	DEBUG_PRINTK("Display '%s' id %i information:", display->name,
		display->id);
	DEBUG_PRINTK("*Width, height: %u,%u", display->width,
		display->height);
	DEBUG_PRINTK("*Rotation: %u", display->rotation);
	DEBUG_PRINTK("*Stride: %u bytes", display->byte_stride);
	DEBUG_PRINTK("*Buffers available: %u", buffers_available);
	DEBUG_PRINTK("*Bytes per pixel: %u (%u bpp)",
		display->bytes_per_pixel, display->bits_per_pixel);

	if(display->bits_per_pixel == 16)
	{
		if(display->pixel_format == RGB_565)
		{
			DEBUG_PRINTK("*Format: RGB565");
			psDevInfo->sDisplayFormat.pixelformat =
				PVRSRV_PIXEL_FORMAT_RGB565;
		}
		else
			WARNING_PRINTK("*Format: Unknown framebuffer"
				"format");
	}
	else if(display->bits_per_pixel == 24 ||
		display->bits_per_pixel == 32)
	{
		if(display->pixel_format == ARGB_8888)
		{
			DEBUG_PRINTK("*Format: ARGB8888");
			psDevInfo->sDisplayFormat.pixelformat =
				PVRSRV_PIXEL_FORMAT_ARGB8888;

		}
		else
			WARNING_PRINTK("*Format: Unknown framebuffer"
				"format");
	}
	else
		WARNING_PRINTK("*Format: Unknown framebuffer format");

	if(display->main_buffer)
	{
		DEBUG_PRINTK("*Bytes per buffer: %lu",
			display->main_buffer->size);
		DEBUG_PRINTK("*Main buffer physical address: 0x%lx",
			display->main_buffer->physical_addr);
		DEBUG_PRINTK("*Main buffer virtual address: 0x%lx",
			display->main_buffer->virtual_addr);
		DEBUG_PRINTK("*Main buffer size: %lu bytes",
			display->main_buffer->size);
	}
	else
	{
		psDevInfo->sDisplayInfo.ui32MaxSwapChainBuffers = 0;
		ERROR_PRINTK("*No main buffer found for display '%s'",
			display->name);
		return OMAP_ERROR_INIT_FAILURE;
	}

	psDevInfo->sDisplayInfo.ui32MaxSwapChainBuffers = buffers_available;
	mutex_init(&psDevInfo->sSwapChainLockMutex);
	psDevInfo->psSwapChain = 0;
	psDevInfo->bFlushCommands = OMAP_FALSE;
	psDevInfo->bDeviceSuspended = OMAP_FALSE;

	if(psDevInfo->sDisplayInfo.ui32MaxSwapChainBuffers > 1)
	{
		if(MAX_BUFFERS_FLIPPING == 1)
		{
			DEBUG_PRINTK("Flipping support is possible"
				" but you decided not to use it");
		}

		DEBUG_PRINTK("*Flipping support");
		if(psDevInfo->sDisplayInfo.ui32MaxSwapChainBuffers >
			MAX_BUFFERS_FLIPPING)
		psDevInfo->sDisplayInfo.ui32MaxSwapChainBuffers =
			MAX_BUFFERS_FLIPPING;
	}
	else
	{
		DEBUG_PRINTK("*Flipping not supported");
	}

	if (psDevInfo->sDisplayInfo.ui32MaxSwapChainBuffers == 0)
	{
		psDevInfo->sDisplayInfo.ui32MaxSwapChains = 0;
		psDevInfo->sDisplayInfo.ui32MaxSwapInterval = 0;
	}
	else
	{
		psDevInfo->sDisplayInfo.ui32MaxSwapChains = 1;
		psDevInfo->sDisplayInfo.ui32MaxSwapInterval = 3;
	}
	psDevInfo->sDisplayInfo.ui32MinSwapInterval = 0;

	/* Get the display and framebuffer needed info */
	strncpy(psDevInfo->sDisplayInfo.szDisplayName,
		DISPLAY_DEVICE_NAME, MAX_DISPLAY_NAME_SIZE);

	psDevInfo->sDisplayDim.ui32Width = display->width;
	psDevInfo->sDisplayDim.ui32Height = display->height;
	psDevInfo->sDisplayDim.ui32ByteStride = display->byte_stride;
	psDevInfo->sSystemBuffer.sSysAddr.uiAddr =
		display->main_buffer->physical_addr;
	psDevInfo->sSystemBuffer.sCPUVAddr =
		(IMG_CPU_VIRTADDR) display->main_buffer->virtual_addr;
	psDevInfo->sSystemBuffer.ulBufferSize = display->main_buffer->size;
	psDevInfo->display = display;

	return OMAP_OK;
}

/*
 *  Initialization routine for the 3rd party display driver
 */
static enum OMAP_ERROR create_display_devices(void)
{
	PFN_CMD_PROC pfnCmdProcList[OMAP_DC_CMD_COUNT];
	IMG_UINT32 aui32SyncCountList[OMAP_DC_CMD_COUNT][2];
	int i;
	unsigned int bytes_to_alloc;

	DEBUG_PRINTK("Initializing 3rd party display driver");

	/* Ask for the number of displays available */
	omap_display_init();
	/* TODO: allow more displays */
	display_devices_count = 1; // omap_display_count();

	DEBUG_PRINTK("Found %i displays", display_devices_count);

	/*
	 * Obtain the function pointer for the jump table from kernel
	 * services to fill it with the function pointers that we want
	 */
	if(get_pvr_dc_jtable ("PVRGetDisplayClassJTable2",
		&pfnGetPVRJTable) != OMAP_OK)
	{
		ERROR_PRINTK("Unable to get the function to get the"
			" jump table display->services");
		return OMAP_ERROR_INIT_FAILURE;
	}

	/*
	 * Allocate the display device structures, one per display available
	 */
	bytes_to_alloc =
		sizeof(struct OMAP_DISP_DEVINFO) * display_devices_count;
	pDisplayDevices = (struct OMAP_DISP_DEVINFO *) kmalloc(
		bytes_to_alloc, GFP_KERNEL);
	if(!pDisplayDevices)
	{
		pDisplayDevices = NULL;
		ERROR_PRINTK("Out of memory");
		return OMAP_ERROR_OUT_OF_MEMORY;
	}
	memset(pDisplayDevices, 0, bytes_to_alloc);

	/*
	 * Initialize each display device
	 */
	for(i = 0; i < display_devices_count; i++)
	{
		struct omap_display_device *display;
		struct OMAP_DISP_DEVINFO * psDevInfo;
		enum omap_display_id id;

		psDevInfo = &pDisplayDevices[i];
		psDevInfo->display = 0;

		id = OMAP_DISPID_VIRTUAL;

		/*
		 * TODO: Modify this to allow primary, secondary,
		 * not only virtual
		 */
#if 0
		switch(i)
		{
			case 0:
				id = OMAP_DISPID_PRIMARY;
				break;
			case 1:
				id = OMAP_DISPID_SECONDARY;
				break;
			case 2:
				id = OMAP_DISPID_TERTIARY;
				break;
			case 3:
				id = OMAP_DISPID_VIRTUAL;
				break;
			default:
				ERROR_PRINTK("Invalid display type %i", i);
				BUG();
		}

#endif

		display = omap_display_get(id);
		if(!display)
			continue;

		if(init_display_device(psDevInfo, display) != OMAP_OK)
		{
			ERROR_PRINTK("Unable to initialize display '%s' type"
				" %u", display->name, display->id);
			continue;
#if 0
			kfree(pDisplayDevices);
			pDisplayDevices = NULL;
			return OMAP_ERROR_INIT_FAILURE;
#endif
		}

		/*
		 * Populate each display device structure
		*/
		if(!(*pfnGetPVRJTable)(&psDevInfo->sPVRJTable))
		{
			ERROR_PRINTK("Unable to get the jump table"
				" display->services for display '%s'",
				display->name);
			return OMAP_ERROR_INIT_FAILURE;
		}

		/* Populate the function table that services will use */
		psDevInfo->sDCJTable.ui32TableSize =
			sizeof(PVRSRV_DC_SRV2DISP_KMJTABLE);
		psDevInfo->sDCJTable.pfnOpenDCDevice = OpenDCDevice;
		psDevInfo->sDCJTable.pfnCloseDCDevice = CloseDCDevice;
		psDevInfo->sDCJTable.pfnEnumDCFormats = EnumDCFormats;
		psDevInfo->sDCJTable.pfnEnumDCDims = EnumDCDims;
		psDevInfo->sDCJTable.pfnGetDCSystemBuffer = GetDCSystemBuffer;
		psDevInfo->sDCJTable.pfnGetDCInfo = GetDCInfo;
		psDevInfo->sDCJTable.pfnGetBufferAddr = GetDCBufferAddr;
		psDevInfo->sDCJTable.pfnCreateDCSwapChain = CreateDCSwapChain;
		psDevInfo->sDCJTable.pfnDestroyDCSwapChain =
			DestroyDCSwapChain;
		psDevInfo->sDCJTable.pfnSetDCDstRect = SetDCDstRect;
		psDevInfo->sDCJTable.pfnSetDCSrcRect = SetDCSrcRect;
		psDevInfo->sDCJTable.pfnSetDCDstColourKey = SetDCDstColourKey;
		psDevInfo->sDCJTable.pfnSetDCSrcColourKey = SetDCSrcColourKey;
		psDevInfo->sDCJTable.pfnGetDCBuffers = GetDCBuffers;
		psDevInfo->sDCJTable.pfnSwapToDCBuffer = SwapToDCBuffer;
		psDevInfo->sDCJTable.pfnSwapToDCSystem = SwapToDCSystem;
		psDevInfo->sDCJTable.pfnSetDCState = SetDCState;

		/* Register the display device */
		if(psDevInfo->sPVRJTable.pfnPVRSRVRegisterDCDevice(
			&psDevInfo->sDCJTable,
			(IMG_UINT32*) &psDevInfo->ulDeviceID) != PVRSRV_OK)
		{
			ERROR_PRINTK("Unable to register the jump table"
				" services->display");
			return OMAP_ERROR_DEVICE_REGISTER_FAILED;
		}

		DEBUG_PRINTK("Display '%s' registered with the GPU with"
			" id %lu", display->name, psDevInfo->ulDeviceID);

		/*
		 * Register the ProcessFlip function to notify when a frame is
		 * ready to be flipped
		 */
		pfnCmdProcList[DC_FLIP_COMMAND] = ProcessFlip;
		aui32SyncCountList[DC_FLIP_COMMAND][0] = 0;
		aui32SyncCountList[DC_FLIP_COMMAND][1] = 2;
		if (psDevInfo->sPVRJTable.pfnPVRSRVRegisterCmdProcList(
			psDevInfo->ulDeviceID, &pfnCmdProcList[0],
			aui32SyncCountList, OMAP_DC_CMD_COUNT) != PVRSRV_OK)
		{
			ERROR_PRINTK("Unable to register callback for "
				"ProcessFlip command");
			return OMAP_ERROR_CANT_REGISTER_CALLBACK;
		}

	}
	return OMAP_OK;
}

/*
 * Here we get the function pointer to get jump table from
 * services using an external function.
 * in: szFunctionName
 * out: ppfnFuncTable
 */
static enum OMAP_ERROR get_pvr_dc_jtable (char *szFunctionName,
	PFN_DC_GET_PVRJTABLE *ppfnFuncTable)
{
	if(strcmp("PVRGetDisplayClassJTable2", szFunctionName) != 0)
	{
		ERROR_PRINTK("Unable to get function pointer for %s"
			" from services", szFunctionName);
		return OMAP_ERROR_INVALID_PARAMS;
	}
	*ppfnFuncTable = PVRGetDisplayClassJTable2;

	return OMAP_OK;
}

#if defined(LDM_PLATFORM)

static volatile enum OMAP_BOOL bDeviceSuspended;

/*
 * Common suspend driver function
 * in: psSwapChain, aPhyAddr
 */
static void CommonSuspend(void)
{
	if (bDeviceSuspended)
	{
		DEBUG_PRINTK("Driver is already suspended");
		return;
	}

	DriverSuspend();
	bDeviceSuspended = OMAP_TRUE;
}

#if defined(SGX_EARLYSUSPEND)

static struct early_suspend driver_early_suspend;

/*
 * Android specific, driver is requested to be suspended
 * in: ea_event
 */
static void DriverSuspend_Entry(struct early_suspend *ea_event)
{
	DEBUG_PRINTK("Requested driver suspend");
	CommonSuspend();
}

/*
 * Android specific, driver is requested to be suspended
 * in: ea_event
 */
static void DriverResume_Entry(struct early_suspend *ea_event)
{
	DEBUG_PRINTK("Requested driver resume");
	DriverResume();
	bDeviceSuspended = OMAP_FALSE;
}

static struct platform_driver omap_sgx_dc_driver = {
	.driver = {
		.name = DRVNAME,
	}
};

#else /* defined(SGX_EARLYSUSPEND) */

/*
 * Function called when the driver is requested to be suspended
 * in: pDevice, state
 */
static int DriverSuspend_Entry(struct platform_device unref__ *pDevice,
	pm_message_t unref__ state)
{
	DEBUG_PRINTK("Requested driver suspend");
	CommonSuspend();
	return 0;
}

/*
 * Function called when the driver is requested to resume
 * in: pDevice
 */
static int DriverResume_Entry(struct platform_device unref__ *pDevice)
{
	DEBUG_PRINTK("Requested driver resume");
	DriverResume();
	bDeviceSuspended = OMAP_FALSE;
	return 0;
}

/*
 * Function called when the driver is requested to shutdown
 * in: pDevice
 */
static IMG_VOID DriverShutdown_Entry(
	struct platform_device unref__ *pDevice)
{
	DEBUG_PRINTK("Requested driver shutdown");
	CommonSuspend();
}

static struct platform_driver omap_sgx_dc_driver = {
	.driver = {
		.name = DRVNAME,
	},
	.suspend = DriverSuspend_Entry,
	.resume	= DriverResume_Entry,
	.shutdown = DriverShutdown_Entry,
};

#endif /* defined(SGX_EARLYSUSPEND) */

#endif /* defined(LDM_PLATFORM) */

/*
 * Driver init function
 */
static int __init omap_sgx_dc_init(void)
{
	if(create_display_devices() != OMAP_OK)
	{
		WARNING_PRINTK("Driver init failed");
		return -ENODEV;
	}

#if defined(LDM_PLATFORM)
	DEBUG_PRINTK("Registering platform driver");
	if (platform_driver_register(&omap_sgx_dc_driver))
	{
		WARNING_PRINTK("Unable to register platform driver");
		if(destroy_display_devices() != OMAP_OK)
			WARNING_PRINTK("Driver cleanup failed\n");
		return -ENODEV;
	}

#if defined(SGX_EARLYSUSPEND)
	driver_early_suspend.suspend = DriverSuspend_Entry;
        driver_early_suspend.resume = DriverResume_Entry;
        driver_early_suspend.level = EARLY_SUSPEND_LEVEL_DISABLE_FB;
        register_early_suspend(&driver_early_suspend);
	DEBUG_PRINTK("Registered early suspend support");
#endif

#endif
	return 0;
}

/*
 * Driver exit function
 */
static IMG_VOID __exit omap_sgx_dc_deinit(IMG_VOID)
{
#if defined(LDM_PLATFORM)
	DEBUG_PRINTK("Removing platform driver");
	platform_driver_unregister(&omap_sgx_dc_driver);
#if defined(SGX_EARLYSUSPEND)
        unregister_early_suspend(&driver_early_suspend);
#endif
#endif
	if(destroy_display_devices() != OMAP_OK)
		WARNING_PRINTK("Driver cleanup failed");
}

MODULE_SUPPORTED_DEVICE(DEVNAME);
late_initcall(omap_sgx_dc_init);
module_exit(omap_sgx_dc_deinit);
