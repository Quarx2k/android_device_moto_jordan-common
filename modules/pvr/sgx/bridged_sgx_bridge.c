/**********************************************************************
 *
 * Copyright (C) Imagination Technologies Ltd. All rights reserved.
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
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
 * 
 * The full GNU General Public License is included in this distribution in
 * the file called "COPYING".
 *
 * Contact Information:
 * Imagination Technologies Ltd. <gpl-support@imgtec.com>
 * Home Park Estate, Kings Langley, Herts, WD4 8LZ, UK 
 *
 ******************************************************************************/



#include <stddef.h>

#include "img_defs.h"

#if defined(SUPPORT_SGX)

#include "services.h"
#include "pvr_debug.h"
#include "pvr_bridge.h"
#include "sgx_bridge.h"
#include "perproc.h"
#include "power.h"
#include "pvr_bridge_km.h"
#include "sgx_bridge_km.h"
#include "sgx_options.h"

#if defined(SUPPORT_MSVDX)
	#include "msvdx_bridge.h"
#endif

#include "bridged_pvr_bridge.h"
#include "bridged_sgx_bridge.h"
#include "sgxutils.h"
#include "buffer_manager.h"
#include "pdump_km.h"

static IMG_INT
SGXGetClientInfoBW(IMG_UINT32 ui32BridgeID,
				   PVRSRV_BRIDGE_IN_GETCLIENTINFO *psGetClientInfoIN,
				   PVRSRV_BRIDGE_OUT_GETCLIENTINFO *psGetClientInfoOUT,
				   PVRSRV_PER_PROCESS_DATA *psPerProc)
{
	IMG_HANDLE hDevCookieInt;

	PVRSRV_BRIDGE_ASSERT_CMD(ui32BridgeID, PVRSRV_BRIDGE_SGX_GETCLIENTINFO);

	psGetClientInfoOUT->eError =
		PVRSRVLookupHandle(psPerProc->psHandleBase,
						   &hDevCookieInt,
						   psGetClientInfoIN->hDevCookie,
						   PVRSRV_HANDLE_TYPE_DEV_NODE);
	if(psGetClientInfoOUT->eError != PVRSRV_OK)
	{
		return 0;
	}

	psGetClientInfoOUT->eError =
		SGXGetClientInfoKM(hDevCookieInt,
						   &psGetClientInfoOUT->sClientInfo);
	return 0;
}

static IMG_INT
SGXReleaseClientInfoBW(IMG_UINT32 ui32BridgeID,
					   PVRSRV_BRIDGE_IN_RELEASECLIENTINFO *psReleaseClientInfoIN,
					   PVRSRV_BRIDGE_RETURN *psRetOUT,
					   PVRSRV_PER_PROCESS_DATA *psPerProc)
{
	PVRSRV_SGXDEV_INFO *psDevInfo;
	IMG_HANDLE hDevCookieInt;

	PVRSRV_BRIDGE_ASSERT_CMD(ui32BridgeID, PVRSRV_BRIDGE_SGX_RELEASECLIENTINFO);

	psRetOUT->eError =
		PVRSRVLookupHandle(psPerProc->psHandleBase,
						   &hDevCookieInt,
						   psReleaseClientInfoIN->hDevCookie,
						   PVRSRV_HANDLE_TYPE_DEV_NODE);
	if(psRetOUT->eError != PVRSRV_OK)
	{
		return 0;
	}

	psDevInfo = (PVRSRV_SGXDEV_INFO *)((PVRSRV_DEVICE_NODE *)hDevCookieInt)->pvDevice;

	PVR_ASSERT(psDevInfo->ui32ClientRefCount > 0);

	
	if (psDevInfo->ui32ClientRefCount > 0)
	{
		psDevInfo->ui32ClientRefCount--;
	}
	
	psRetOUT->eError = PVRSRV_OK;

	return 0;
}


static IMG_INT
SGXGetInternalDevInfoBW(IMG_UINT32 ui32BridgeID,
						PVRSRV_BRIDGE_IN_GETINTERNALDEVINFO *psSGXGetInternalDevInfoIN,
						PVRSRV_BRIDGE_OUT_GETINTERNALDEVINFO *psSGXGetInternalDevInfoOUT,
						PVRSRV_PER_PROCESS_DATA *psPerProc)
{
	IMG_HANDLE hDevCookieInt;
#if defined (SUPPORT_SID_INTERFACE)
	SGX_INTERNAL_DEVINFO_KM sSGXInternalDevInfo;
#endif

	PVRSRV_BRIDGE_ASSERT_CMD(ui32BridgeID, PVRSRV_BRIDGE_SGX_GETINTERNALDEVINFO);

	psSGXGetInternalDevInfoOUT->eError =
		PVRSRVLookupHandle(psPerProc->psHandleBase,
						   &hDevCookieInt,
						   psSGXGetInternalDevInfoIN->hDevCookie,
						   PVRSRV_HANDLE_TYPE_DEV_NODE);
	if(psSGXGetInternalDevInfoOUT->eError != PVRSRV_OK)
	{
		return 0;
	}

	psSGXGetInternalDevInfoOUT->eError =
		SGXGetInternalDevInfoKM(hDevCookieInt,
#if defined (SUPPORT_SID_INTERFACE)
								&sSGXInternalDevInfo);
#else
								&psSGXGetInternalDevInfoOUT->sSGXInternalDevInfo);
#endif

	
	psSGXGetInternalDevInfoOUT->eError =
		PVRSRVAllocHandle(psPerProc->psHandleBase,
						  &psSGXGetInternalDevInfoOUT->sSGXInternalDevInfo.hHostCtlKernelMemInfoHandle,
#if defined (SUPPORT_SID_INTERFACE)
						  sSGXInternalDevInfo.hHostCtlKernelMemInfoHandle,
#else
						  psSGXGetInternalDevInfoOUT->sSGXInternalDevInfo.hHostCtlKernelMemInfoHandle,
#endif
						  PVRSRV_HANDLE_TYPE_MEM_INFO,
						  PVRSRV_HANDLE_ALLOC_FLAG_SHARED);

	return 0;
}


static IMG_INT
SGXDoKickBW(IMG_UINT32 ui32BridgeID,
			PVRSRV_BRIDGE_IN_DOKICK *psDoKickIN,
			PVRSRV_BRIDGE_RETURN *psRetOUT,
			PVRSRV_PER_PROCESS_DATA *psPerProc)
{
	IMG_HANDLE hDevCookieInt;
	IMG_UINT32 i;
	IMG_INT ret = 0;
	IMG_UINT32 ui32NumDstSyncs;
#if defined (SUPPORT_SID_INTERFACE)
	SGX_CCB_KICK_KM sCCBKickKM = {{0}};
	IMG_HANDLE	ahSyncInfoHandles[16];
#else
	IMG_HANDLE *phKernelSyncInfoHandles = IMG_NULL;
#endif

	PVRSRV_BRIDGE_ASSERT_CMD(ui32BridgeID, PVRSRV_BRIDGE_SGX_DOKICK);

	psRetOUT->eError =
		PVRSRVLookupHandle(psPerProc->psHandleBase,
						   &hDevCookieInt,
						   psDoKickIN->hDevCookie,
						   PVRSRV_HANDLE_TYPE_DEV_NODE);

	if(psRetOUT->eError != PVRSRV_OK)
	{
		return 0;
	}

	psRetOUT->eError =
		PVRSRVLookupHandle(psPerProc->psHandleBase,
#if defined (SUPPORT_SID_INTERFACE)
						   &sCCBKickKM.hCCBKernelMemInfo,
#else
						   &psDoKickIN->sCCBKick.hCCBKernelMemInfo,
#endif
						   psDoKickIN->sCCBKick.hCCBKernelMemInfo,
						   PVRSRV_HANDLE_TYPE_MEM_INFO);

	if(psRetOUT->eError != PVRSRV_OK)
	{
		return 0;
	}

#if defined (SUPPORT_SID_INTERFACE)
	if (psDoKickIN->sCCBKick.ui32NumDstSyncObjects > 16)
	{
		return 0;
	}
			
	if(psDoKickIN->sCCBKick.hTA3DSyncInfo != 0)
#else
	if(psDoKickIN->sCCBKick.hTA3DSyncInfo != IMG_NULL)
#endif
	{
		psRetOUT->eError =
			PVRSRVLookupHandle(psPerProc->psHandleBase,
#if defined (SUPPORT_SID_INTERFACE)
							   &sCCBKickKM.hTA3DSyncInfo,
#else
							   &psDoKickIN->sCCBKick.hTA3DSyncInfo,
#endif
							   psDoKickIN->sCCBKick.hTA3DSyncInfo,
							   PVRSRV_HANDLE_TYPE_SYNC_INFO);

		if(psRetOUT->eError != PVRSRV_OK)
		{
			return 0;
		}
	}

#if defined (SUPPORT_SID_INTERFACE)
	if(psDoKickIN->sCCBKick.hTASyncInfo != 0)
#else
	if(psDoKickIN->sCCBKick.hTASyncInfo != IMG_NULL)
#endif
	{
		psRetOUT->eError =
			PVRSRVLookupHandle(psPerProc->psHandleBase,
#if defined (SUPPORT_SID_INTERFACE)
							   &sCCBKickKM.hTASyncInfo,
#else
							   &psDoKickIN->sCCBKick.hTASyncInfo,
#endif
							   psDoKickIN->sCCBKick.hTASyncInfo,
							   PVRSRV_HANDLE_TYPE_SYNC_INFO);

		if(psRetOUT->eError != PVRSRV_OK)
		{
			return 0;
		}
	}

#if defined(FIX_HW_BRN_31620)
	
	psRetOUT->eError =
		PVRSRVLookupHandle(psPerProc->psHandleBase,
						   &psDoKickIN->sCCBKick.hDevMemContext,
						   psDoKickIN->sCCBKick.hDevMemContext,
						   PVRSRV_HANDLE_TYPE_DEV_MEM_CONTEXT);

	if(psRetOUT->eError != PVRSRV_OK)
	{
		return 0;
	}
#endif

#if defined (SUPPORT_SID_INTERFACE)
	if(psDoKickIN->sCCBKick.h3DSyncInfo != 0)
#else
	if(psDoKickIN->sCCBKick.h3DSyncInfo != IMG_NULL)
#endif
	{
		psRetOUT->eError =
			PVRSRVLookupHandle(psPerProc->psHandleBase,
#if defined (SUPPORT_SID_INTERFACE)
							   &sCCBKickKM.h3DSyncInfo,
#else
							   &psDoKickIN->sCCBKick.h3DSyncInfo,
#endif
							   psDoKickIN->sCCBKick.h3DSyncInfo,
							   PVRSRV_HANDLE_TYPE_SYNC_INFO);

		if(psRetOUT->eError != PVRSRV_OK)
		{
			return 0;
		}
	}


#if defined(SUPPORT_SGX_GENERALISED_SYNCOBJECTS)
	
	if (psDoKickIN->sCCBKick.ui32NumTASrcSyncs > SGX_MAX_TA_SRC_SYNCS)
	{
		psRetOUT->eError = PVRSRV_ERROR_INVALID_PARAMS;
		return 0;
	}

#if defined (SUPPORT_SID_INTERFACE)
	sCCBKickKM.ui32NumTASrcSyncs = psDoKickIN->sCCBKick.ui32NumTASrcSyncs;
#endif
	for(i=0; i<psDoKickIN->sCCBKick.ui32NumTASrcSyncs; i++)
	{
		psRetOUT->eError =
			PVRSRVLookupHandle(psPerProc->psHandleBase,
#if defined (SUPPORT_SID_INTERFACE)
							   &sCCBKickKM.ahTASrcKernelSyncInfo[i],
#else
							   &psDoKickIN->sCCBKick.ahTASrcKernelSyncInfo[i],
#endif
							   psDoKickIN->sCCBKick.ahTASrcKernelSyncInfo[i],
							   PVRSRV_HANDLE_TYPE_SYNC_INFO);

		if(psRetOUT->eError != PVRSRV_OK)
		{
			return 0;
		}
	}

	if (psDoKickIN->sCCBKick.ui32NumTADstSyncs > SGX_MAX_TA_DST_SYNCS)
	{
		psRetOUT->eError = PVRSRV_ERROR_INVALID_PARAMS;
		return 0;
	}

#if defined (SUPPORT_SID_INTERFACE)
	sCCBKickKM.ui32NumTADstSyncs = psDoKickIN->sCCBKick.ui32NumTADstSyncs;
#endif
	for(i=0; i<psDoKickIN->sCCBKick.ui32NumTADstSyncs; i++)
	{
		psRetOUT->eError =
			PVRSRVLookupHandle(psPerProc->psHandleBase,
#if defined (SUPPORT_SID_INTERFACE)
							   &sCCBKickKM.ahTADstKernelSyncInfo[i],
#else
							   &psDoKickIN->sCCBKick.ahTADstKernelSyncInfo[i],
#endif
							   psDoKickIN->sCCBKick.ahTADstKernelSyncInfo[i],
							   PVRSRV_HANDLE_TYPE_SYNC_INFO);

		if(psRetOUT->eError != PVRSRV_OK)
		{
			return 0;
		}
	}

	if (psDoKickIN->sCCBKick.ui32Num3DSrcSyncs > SGX_MAX_3D_SRC_SYNCS)
	{
		psRetOUT->eError = PVRSRV_ERROR_INVALID_PARAMS;
		return 0;
	}

#if defined (SUPPORT_SID_INTERFACE)
	sCCBKickKM.ui32Num3DSrcSyncs = psDoKickIN->sCCBKick.ui32Num3DSrcSyncs;
#endif
	for(i=0; i<psDoKickIN->sCCBKick.ui32Num3DSrcSyncs; i++)
	{
		psRetOUT->eError =
			PVRSRVLookupHandle(psPerProc->psHandleBase,
#if defined (SUPPORT_SID_INTERFACE)
							   &sCCBKickKM.ah3DSrcKernelSyncInfo[i],
#else
							   &psDoKickIN->sCCBKick.ah3DSrcKernelSyncInfo[i],
#endif
							   psDoKickIN->sCCBKick.ah3DSrcKernelSyncInfo[i],
							   PVRSRV_HANDLE_TYPE_SYNC_INFO);

		if(psRetOUT->eError != PVRSRV_OK)
		{
			return 0;
		}
	}
#else
	
	if (psDoKickIN->sCCBKick.ui32NumSrcSyncs > SGX_MAX_SRC_SYNCS)
	{
		psRetOUT->eError = PVRSRV_ERROR_INVALID_PARAMS;
		return 0;
	}

#if defined (SUPPORT_SID_INTERFACE)
	sCCBKickKM.ui32NumSrcSyncs = psDoKickIN->sCCBKick.ui32NumSrcSyncs;
#endif
	for(i=0; i<psDoKickIN->sCCBKick.ui32NumSrcSyncs; i++)
	{
		psRetOUT->eError =
			PVRSRVLookupHandle(psPerProc->psHandleBase,
#if defined (SUPPORT_SID_INTERFACE)
							   &sCCBKickKM.ahSrcKernelSyncInfo[i],
#else
							   &psDoKickIN->sCCBKick.ahSrcKernelSyncInfo[i],
#endif
							   psDoKickIN->sCCBKick.ahSrcKernelSyncInfo[i],
							   PVRSRV_HANDLE_TYPE_SYNC_INFO);

		if(psRetOUT->eError != PVRSRV_OK)
		{
			return 0;
		}
	}
#endif

	if (psDoKickIN->sCCBKick.ui32NumTAStatusVals > SGX_MAX_TA_STATUS_VALS)
	{
		psRetOUT->eError = PVRSRV_ERROR_INVALID_PARAMS;
		return 0;
	}
	for (i = 0; i < psDoKickIN->sCCBKick.ui32NumTAStatusVals; i++)
	{
		psRetOUT->eError =
#if defined(SUPPORT_SGX_NEW_STATUS_VALS)
			PVRSRVLookupHandle(psPerProc->psHandleBase,
#if defined (SUPPORT_SID_INTERFACE)
							   &sCCBKickKM.asTAStatusUpdate[i].hKernelMemInfo,
#else
							   &psDoKickIN->sCCBKick.asTAStatusUpdate[i].hKernelMemInfo,
#endif
							   psDoKickIN->sCCBKick.asTAStatusUpdate[i].hKernelMemInfo,
							   PVRSRV_HANDLE_TYPE_MEM_INFO);

#if defined (SUPPORT_SID_INTERFACE)
		sCCBKickKM.asTAStatusUpdate[i].sCtlStatus = psDoKickIN->sCCBKick.asTAStatusUpdate[i].sCtlStatus;
#endif

#else
			PVRSRVLookupHandle(psPerProc->psHandleBase,
#if defined (SUPPORT_SID_INTERFACE)
							   &sCCBKickKM.ahTAStatusSyncInfo[i],
#else
							   &psDoKickIN->sCCBKick.ahTAStatusSyncInfo[i],
#endif
							   psDoKickIN->sCCBKick.ahTAStatusSyncInfo[i],
							   PVRSRV_HANDLE_TYPE_SYNC_INFO);
#endif
		if(psRetOUT->eError != PVRSRV_OK)
		{
			return 0;
		}
	}

	if (psDoKickIN->sCCBKick.ui32Num3DStatusVals > SGX_MAX_3D_STATUS_VALS)
	{
		psRetOUT->eError = PVRSRV_ERROR_INVALID_PARAMS;
		return 0;
	}
	for(i = 0; i < psDoKickIN->sCCBKick.ui32Num3DStatusVals; i++)
	{
		psRetOUT->eError =
#if defined(SUPPORT_SGX_NEW_STATUS_VALS)
			PVRSRVLookupHandle(psPerProc->psHandleBase,
#if defined (SUPPORT_SID_INTERFACE)
							   &sCCBKickKM.as3DStatusUpdate[i].hKernelMemInfo,
#else
							   &psDoKickIN->sCCBKick.as3DStatusUpdate[i].hKernelMemInfo,
#endif
							   psDoKickIN->sCCBKick.as3DStatusUpdate[i].hKernelMemInfo,
							   PVRSRV_HANDLE_TYPE_MEM_INFO);
							   
#if defined (SUPPORT_SID_INTERFACE)
		sCCBKickKM.as3DStatusUpdate[i].sCtlStatus = psDoKickIN->sCCBKick.as3DStatusUpdate[i].sCtlStatus;
#endif
#else
			PVRSRVLookupHandle(psPerProc->psHandleBase,
#if defined (SUPPORT_SID_INTERFACE)
							   &sCCBKickKM.ah3DStatusSyncInfo[i],
#else
							   &psDoKickIN->sCCBKick.ah3DStatusSyncInfo[i],
#endif
							   psDoKickIN->sCCBKick.ah3DStatusSyncInfo[i],
							   PVRSRV_HANDLE_TYPE_SYNC_INFO);
#endif

		if(psRetOUT->eError != PVRSRV_OK)
		{
			return 0;
		}
	}

	ui32NumDstSyncs = psDoKickIN->sCCBKick.ui32NumDstSyncObjects;

	if(ui32NumDstSyncs > 0)
	{
		if(!OSAccessOK(PVR_VERIFY_READ,
						psDoKickIN->sCCBKick.pahDstSyncHandles,
						ui32NumDstSyncs * sizeof(IMG_HANDLE)))
		{
			PVR_DPF((PVR_DBG_ERROR, "%s: SGXDoKickBW:"
					" Invalid pasDstSyncHandles pointer", __FUNCTION__));
			return -EFAULT;
		}

		psRetOUT->eError = OSAllocMem(PVRSRV_OS_PAGEABLE_HEAP,
										ui32NumDstSyncs * sizeof(IMG_HANDLE),
										(IMG_VOID **)&phKernelSyncInfoHandles,
										0,
										"Array of Synchronization Info Handles");
		if (psRetOUT->eError != PVRSRV_OK)
		{
			return 0;
		}

#if defined (SUPPORT_SID_INTERFACE)
		sCCBKickKM.pahDstSyncHandles = phKernelSyncInfoHandles;
#else
		if(CopyFromUserWrapper(psPerProc,
							ui32BridgeID,
							phKernelSyncInfoHandles,
							psDoKickIN->sCCBKick.pahDstSyncHandles,
							ui32NumDstSyncs * sizeof(IMG_HANDLE)) != PVRSRV_OK)
		{
			ret = -EFAULT;
			goto PVRSRV_BRIDGE_SGX_DOKICK_RETURN_RESULT;
		}

		
		psDoKickIN->sCCBKick.pahDstSyncHandles = phKernelSyncInfoHandles;
#endif

		for( i = 0; i < ui32NumDstSyncs; i++)
		{
			psRetOUT->eError =
				PVRSRVLookupHandle(psPerProc->psHandleBase,
#if defined (SUPPORT_SID_INTERFACE)
									&sCCBKickKM.pahDstSyncHandles[i],
#else
									&psDoKickIN->sCCBKick.pahDstSyncHandles[i],
#endif
									psDoKickIN->sCCBKick.pahDstSyncHandles[i],
									PVRSRV_HANDLE_TYPE_SYNC_INFO);

			if(psRetOUT->eError != PVRSRV_OK)
			{
				goto PVRSRV_BRIDGE_SGX_DOKICK_RETURN_RESULT;
			}

		}

		psRetOUT->eError =
					PVRSRVLookupHandle(psPerProc->psHandleBase,
#if defined (SUPPORT_SID_INTERFACE)
									   &sCCBKickKM.hKernelHWSyncListMemInfo,
#else
									   &psDoKickIN->sCCBKick.hKernelHWSyncListMemInfo,
#endif
									   psDoKickIN->sCCBKick.hKernelHWSyncListMemInfo,
									   PVRSRV_HANDLE_TYPE_MEM_INFO);

		if(psRetOUT->eError != PVRSRV_OK)
		{
			goto PVRSRV_BRIDGE_SGX_DOKICK_RETURN_RESULT;
		}
	}

#if defined (SUPPORT_SID_INTERFACE)
	OSMemCopy(&sCCBKickKM.sCommand, &psDoKickIN->sCCBKick.sCommand, sizeof(sCCBKickKM.sCommand));

	sCCBKickKM.ui32NumDstSyncObjects = psDoKickIN->sCCBKick.ui32NumDstSyncObjects;
	sCCBKickKM.ui32NumTAStatusVals   = psDoKickIN->sCCBKick.ui32NumTAStatusVals;
	sCCBKickKM.ui32Num3DStatusVals   = psDoKickIN->sCCBKick.ui32Num3DStatusVals;
	sCCBKickKM.bFirstKickOrResume    = psDoKickIN->sCCBKick.bFirstKickOrResume;
	sCCBKickKM.ui32CCBOffset         = psDoKickIN->sCCBKick.ui32CCBOffset;
	sCCBKickKM.bTADependency         = psDoKickIN->sCCBKick.bTADependency;

#if (defined(NO_HARDWARE) || defined(PDUMP))
	sCCBKickKM.bTerminateOrAbort = psDoKickIN->sCCBKick.bTerminateOrAbort;
#endif
#if defined(PDUMP)
	sCCBKickKM.ui32CCBDumpWOff = psDoKickIN->sCCBKick.ui32CCBDumpWOff;
#endif

#if defined(NO_HARDWARE)
	sCCBKickKM.ui32WriteOpsPendingVal = psDoKickIN->sCCBKick.ui32WriteOpsPendingVal;
#endif
#endif 
	psRetOUT->eError =
		SGXDoKickKM(hDevCookieInt,
#if defined (SUPPORT_SID_INTERFACE)
					&sCCBKickKM);
#else
					&psDoKickIN->sCCBKick);
#endif

PVRSRV_BRIDGE_SGX_DOKICK_RETURN_RESULT:

	if(phKernelSyncInfoHandles)
	{
		OSFreeMem(PVRSRV_OS_PAGEABLE_HEAP,
				  ui32NumDstSyncs * sizeof(IMG_HANDLE),
				  (IMG_VOID *)phKernelSyncInfoHandles,
				  0);
		
	}
	return ret;
}


static IMG_INT
SGXScheduleProcessQueuesBW(IMG_UINT32 ui32BridgeID,
			PVRSRV_BRIDGE_IN_SGX_SCHEDULE_PROCESS_QUEUES *psScheduleProcQIN,
			PVRSRV_BRIDGE_RETURN *psRetOUT,
			PVRSRV_PER_PROCESS_DATA *psPerProc)
{
	IMG_HANDLE hDevCookieInt;

	PVRSRV_BRIDGE_ASSERT_CMD(ui32BridgeID, PVRSRV_BRIDGE_SGX_SCHEDULE_PROCESS_QUEUES);

	psRetOUT->eError =
		PVRSRVLookupHandle(psPerProc->psHandleBase,
						   &hDevCookieInt,
						   psScheduleProcQIN->hDevCookie,
						   PVRSRV_HANDLE_TYPE_DEV_NODE);

	if(psRetOUT->eError != PVRSRV_OK)
	{
		return 0;
	}

	psRetOUT->eError = SGXScheduleProcessQueuesKM(hDevCookieInt);

	return 0;
}


#if defined(TRANSFER_QUEUE)
static IMG_INT
SGXSubmitTransferBW(IMG_UINT32 ui32BridgeID,
			PVRSRV_BRIDGE_IN_SUBMITTRANSFER *psSubmitTransferIN,
			PVRSRV_BRIDGE_RETURN *psRetOUT,
			PVRSRV_PER_PROCESS_DATA *psPerProc)
{
	IMG_HANDLE hDevCookieInt;
	PVRSRV_TRANSFER_SGX_KICK *psKick;
#if defined (SUPPORT_SID_INTERFACE)
	PVRSRV_TRANSFER_SGX_KICK_KM sKickKM = {0};
#endif
	IMG_UINT32 i;

	PVRSRV_BRIDGE_ASSERT_CMD(ui32BridgeID, PVRSRV_BRIDGE_SGX_SUBMITTRANSFER);
	PVR_UNREFERENCED_PARAMETER(ui32BridgeID);

	psKick = &psSubmitTransferIN->sKick;

#if defined(FIX_HW_BRN_31620)
	
	psRetOUT->eError =
		PVRSRVLookupHandle(psPerProc->psHandleBase,
						   &psKick->hDevMemContext,
						   psKick->hDevMemContext,
						   PVRSRV_HANDLE_TYPE_DEV_MEM_CONTEXT);

	if(psRetOUT->eError != PVRSRV_OK)
	{
		return 0;
	}
#endif

	psRetOUT->eError =
		PVRSRVLookupHandle(psPerProc->psHandleBase,
						   &hDevCookieInt,
						   psSubmitTransferIN->hDevCookie,
						   PVRSRV_HANDLE_TYPE_DEV_NODE);
	if(psRetOUT->eError != PVRSRV_OK)
	{
		return 0;
	}

	psRetOUT->eError =
		PVRSRVLookupHandle(psPerProc->psHandleBase,
#if defined (SUPPORT_SID_INTERFACE)
						   &sKickKM.hCCBMemInfo,
#else
						   &psKick->hCCBMemInfo,
#endif
						   psKick->hCCBMemInfo,
						   PVRSRV_HANDLE_TYPE_MEM_INFO);
	if(psRetOUT->eError != PVRSRV_OK)
	{
		return 0;
	}

	if (psKick->hTASyncInfo != IMG_NULL)
	{
		psRetOUT->eError =
			PVRSRVLookupHandle(psPerProc->psHandleBase,
#if defined (SUPPORT_SID_INTERFACE)
							   &sKickKM.hTASyncInfo,
#else
							   &psKick->hTASyncInfo,
#endif
							   psKick->hTASyncInfo,
							   PVRSRV_HANDLE_TYPE_SYNC_INFO);
		if(psRetOUT->eError != PVRSRV_OK)
		{
			return 0;
		}
	}

	if (psKick->h3DSyncInfo != IMG_NULL)
	{
		psRetOUT->eError =
			PVRSRVLookupHandle(psPerProc->psHandleBase,
#if defined (SUPPORT_SID_INTERFACE)
							   &sKickKM.h3DSyncInfo,
#else
							   &psKick->h3DSyncInfo,
#endif
							   psKick->h3DSyncInfo,
							   PVRSRV_HANDLE_TYPE_SYNC_INFO);
		if(psRetOUT->eError != PVRSRV_OK)
		{
			return 0;
		}
	}

	if (psKick->ui32NumSrcSync > SGX_MAX_TRANSFER_SYNC_OPS)
	{
		psRetOUT->eError = PVRSRV_ERROR_INVALID_PARAMS;
		return 0;
	}
	for (i = 0; i < psKick->ui32NumSrcSync; i++)
	{
		psRetOUT->eError =
			PVRSRVLookupHandle(psPerProc->psHandleBase,
#if defined (SUPPORT_SID_INTERFACE)
							   &sKickKM.ahSrcSyncInfo[i],
#else
							   &psKick->ahSrcSyncInfo[i],
#endif
							   psKick->ahSrcSyncInfo[i],
							   PVRSRV_HANDLE_TYPE_SYNC_INFO);
		if(psRetOUT->eError != PVRSRV_OK)
		{
			return 0;
		}
	}

	if (psKick->ui32NumDstSync > SGX_MAX_TRANSFER_SYNC_OPS)
	{
		psRetOUT->eError = PVRSRV_ERROR_INVALID_PARAMS;
		return 0;
	}
	for (i = 0; i < psKick->ui32NumDstSync; i++)
	{
		psRetOUT->eError =
			PVRSRVLookupHandle(psPerProc->psHandleBase,
#if defined (SUPPORT_SID_INTERFACE)
							   &sKickKM.ahDstSyncInfo[i],
#else
							   &psKick->ahDstSyncInfo[i],
#endif
							   psKick->ahDstSyncInfo[i],
							   PVRSRV_HANDLE_TYPE_SYNC_INFO);
		if(psRetOUT->eError != PVRSRV_OK)
		{
			return 0;
		}
	}

#if defined (SUPPORT_SID_INTERFACE)
	sKickKM.sHWTransferContextDevVAddr = psKick->sHWTransferContextDevVAddr;
	sKickKM.ui32SharedCmdCCBOffset = psKick->ui32SharedCmdCCBOffset;
	sKickKM.ui32NumSrcSync         = psKick->ui32NumSrcSync;
	sKickKM.ui32NumDstSync         = psKick->ui32NumDstSync;
	sKickKM.ui32Flags              = psKick->ui32Flags;
	sKickKM.ui32PDumpFlags         = psKick->ui32PDumpFlags;
#if defined(PDUMP)
	sKickKM.ui32CCBDumpWOff        = psKick->ui32CCBDumpWOff;
#endif

	psRetOUT->eError = SGXSubmitTransferKM(hDevCookieInt, &sKickKM);
#else
	psRetOUT->eError = SGXSubmitTransferKM(hDevCookieInt, psKick);
#endif

	return 0;
}


#if defined(SGX_FEATURE_2D_HARDWARE)
static IMG_INT
SGXSubmit2DBW(IMG_UINT32 ui32BridgeID,
			PVRSRV_BRIDGE_IN_SUBMIT2D *psSubmit2DIN,
			PVRSRV_BRIDGE_RETURN *psRetOUT,
			PVRSRV_PER_PROCESS_DATA *psPerProc)
{
	IMG_HANDLE hDevCookieInt;
	PVRSRV_2D_SGX_KICK   *psKick;
#if defined (SUPPORT_SID_INTERFACE)
	PVRSRV_2D_SGX_KICK_KM sKickKM;
#endif
	IMG_UINT32 i;

	PVRSRV_BRIDGE_ASSERT_CMD(ui32BridgeID, PVRSRV_BRIDGE_SGX_SUBMIT2D);
	PVR_UNREFERENCED_PARAMETER(ui32BridgeID);

	psKick = &psSubmit2DIN->sKick;

#if defined(FIX_HW_BRN_31620)
	
	psRetOUT->eError =
		PVRSRVLookupHandle(psPerProc->psHandleBase,
						   &psKick->hDevMemContext,
						   psKick->hDevMemContext,
						   PVRSRV_HANDLE_TYPE_DEV_MEM_CONTEXT);

	if(psRetOUT->eError != PVRSRV_OK)
	{
		return 0;
	}
#endif


	psRetOUT->eError =
		PVRSRVLookupHandle(psPerProc->psHandleBase,
						   &hDevCookieInt,
						   psSubmit2DIN->hDevCookie,
						   PVRSRV_HANDLE_TYPE_DEV_NODE);

	if(psRetOUT->eError != PVRSRV_OK)
	{
		return 0;
	}


	psRetOUT->eError =
		PVRSRVLookupHandle(psPerProc->psHandleBase,
#if defined (SUPPORT_SID_INTERFACE)
						   &sKickKM.hCCBMemInfo,
#else
						   &psKick->hCCBMemInfo,
#endif
						   psKick->hCCBMemInfo,
						   PVRSRV_HANDLE_TYPE_MEM_INFO);
	if(psRetOUT->eError != PVRSRV_OK)
	{
		return 0;
	}

#if defined (SUPPORT_SID_INTERFACE)
	if (psKick->hTASyncInfo != 0)
#else
	if (psKick->hTASyncInfo != IMG_NULL)
#endif
	{
		psRetOUT->eError =
			PVRSRVLookupHandle(psPerProc->psHandleBase,
#if defined (SUPPORT_SID_INTERFACE)
							   &sKickKM.hTASyncInfo,
#else
							   &psKick->hTASyncInfo,
#endif
							   psKick->hTASyncInfo,
							   PVRSRV_HANDLE_TYPE_SYNC_INFO);
		if(psRetOUT->eError != PVRSRV_OK)
		{
			return 0;
		}
	}
#if defined (SUPPORT_SID_INTERFACE)
	else
	{
		sKickKM.hTASyncInfo = IMG_NULL;
	}
#endif

	if (psKick->h3DSyncInfo != IMG_NULL)
	{
		psRetOUT->eError =
			PVRSRVLookupHandle(psPerProc->psHandleBase,
#if defined (SUPPORT_SID_INTERFACE)
							   &sKickKM.h3DSyncInfo,
#else
							   &psKick->h3DSyncInfo,
#endif
							   psKick->h3DSyncInfo,
							   PVRSRV_HANDLE_TYPE_SYNC_INFO);
		if(psRetOUT->eError != PVRSRV_OK)
		{
			return 0;
		}
	}
#if defined (SUPPORT_SID_INTERFACE)
	else
	{
		sKickKM.h3DSyncInfo = IMG_NULL;
	}
#endif

	if (psKick->ui32NumSrcSync > SGX_MAX_2D_SRC_SYNC_OPS)
	{
		psRetOUT->eError = PVRSRV_ERROR_INVALID_PARAMS;
		return 0;
	}
#if defined (SUPPORT_SID_INTERFACE)
	for (i = 0; i < SGX_MAX_2D_SRC_SYNC_OPS; i++)
	{
		if (i < psKick->ui32NumSrcSync)
		{
			psRetOUT->eError =
				PVRSRVLookupHandle(psPerProc->psHandleBase,
								   &sKickKM.ahSrcSyncInfo[i],
								   psKick->ahSrcSyncInfo[i],
								   PVRSRV_HANDLE_TYPE_SYNC_INFO);
			if(psRetOUT->eError != PVRSRV_OK)
			{
				return 0;
			}
		}
		else
		{
			sKickKM.ahSrcSyncInfo[i] = IMG_NULL;
		}
	}
#else
	for (i = 0; i < psKick->ui32NumSrcSync; i++)
	{
		psRetOUT->eError =
			PVRSRVLookupHandle(psPerProc->psHandleBase,
							   &psKick->ahSrcSyncInfo[i],
							   psKick->ahSrcSyncInfo[i],
							   PVRSRV_HANDLE_TYPE_SYNC_INFO);
		if(psRetOUT->eError != PVRSRV_OK)
		{
			return 0;
		}
	}
#endif

	if (psKick->hDstSyncInfo != IMG_NULL)
	{
		psRetOUT->eError =
			PVRSRVLookupHandle(psPerProc->psHandleBase,
#if defined (SUPPORT_SID_INTERFACE)
							   &sKickKM.hDstSyncInfo,
#else
							   &psKick->hDstSyncInfo,
#endif
							   psKick->hDstSyncInfo,
							   PVRSRV_HANDLE_TYPE_SYNC_INFO);
		if(psRetOUT->eError != PVRSRV_OK)
		{
			return 0;
		}
	}
#if defined (SUPPORT_SID_INTERFACE)
	else
	{
		sKickKM.hDstSyncInfo = IMG_NULL;
	}

	
	sKickKM.ui32SharedCmdCCBOffset = psKick->ui32SharedCmdCCBOffset;
	sKickKM.ui32NumSrcSync         = psKick->ui32NumSrcSync;
	sKickKM.ui32PDumpFlags         = psKick->ui32PDumpFlags;
	sKickKM.sHW2DContextDevVAddr   = psKick->sHW2DContextDevVAddr;
#if defined(PDUMP)
	sKickKM.ui32CCBDumpWOff        = psKick->ui32CCBDumpWOff;
#endif
#endif

	psRetOUT->eError =
#if defined (SUPPORT_SID_INTERFACE)
		SGXSubmit2DKM(hDevCookieInt, &sKickKM);
#else
		SGXSubmit2DKM(hDevCookieInt, psKick);
#endif

	return 0;
}
#endif 
#endif 


static IMG_INT
SGXGetMiscInfoBW(IMG_UINT32 ui32BridgeID,
				 PVRSRV_BRIDGE_IN_SGXGETMISCINFO *psSGXGetMiscInfoIN,
				 PVRSRV_BRIDGE_RETURN *psRetOUT,
				 PVRSRV_PER_PROCESS_DATA *psPerProc)
{
	IMG_HANDLE hDevCookieInt;
	IMG_HANDLE hDevMemContextInt = 0;
	PVRSRV_SGXDEV_INFO *psDevInfo;
	SGX_MISC_INFO        sMiscInfo;
 	PVRSRV_DEVICE_NODE *psDeviceNode;

	PVRSRV_BRIDGE_ASSERT_CMD(ui32BridgeID,
							PVRSRV_BRIDGE_SGX_GETMISCINFO);

	psRetOUT->eError = PVRSRVLookupHandle(psPerProc->psHandleBase,
							&hDevCookieInt,
							psSGXGetMiscInfoIN->hDevCookie,
							PVRSRV_HANDLE_TYPE_DEV_NODE);

	if(psRetOUT->eError != PVRSRV_OK)
	{
		return 0;
	}

#if defined(SUPPORT_SGX_EDM_MEMORY_DEBUG)
	
	if (psSGXGetMiscInfoIN->psMiscInfo->eRequest == SGX_MISC_INFO_REQUEST_MEMREAD)
	{
		psRetOUT->eError = PVRSRVLookupHandle(psPerProc->psHandleBase,
								&hDevMemContextInt,
								psSGXGetMiscInfoIN->psMiscInfo->hDevMemContext,
								PVRSRV_HANDLE_TYPE_DEV_MEM_CONTEXT);

		if(psRetOUT->eError != PVRSRV_OK)
		{
			return 0;
		}
	}
#endif
	
	psDeviceNode = hDevCookieInt;
	PVR_ASSERT(psDeviceNode != IMG_NULL);
	if (psDeviceNode == IMG_NULL)
	{
		return -EFAULT;
	}

	psDevInfo = psDeviceNode->pvDevice;

	
	psRetOUT->eError = CopyFromUserWrapper(psPerProc,
			                               ui32BridgeID,
			                               &sMiscInfo,
			                               psSGXGetMiscInfoIN->psMiscInfo,
			                               sizeof(SGX_MISC_INFO));
	if (psRetOUT->eError != PVRSRV_OK)
	{
		return -EFAULT;
	}

	{
		psRetOUT->eError = SGXGetMiscInfoKM(psDevInfo, &sMiscInfo, psDeviceNode, hDevMemContextInt);

		if (psRetOUT->eError != PVRSRV_OK)
		{
			return 0;
		}
	}

	
	psRetOUT->eError = CopyToUserWrapper(psPerProc,
		                             ui32BridgeID,
		                             psSGXGetMiscInfoIN->psMiscInfo,
		                             &sMiscInfo,
		                             sizeof(SGX_MISC_INFO));
	if (psRetOUT->eError != PVRSRV_OK)
	{
		return -EFAULT;
	}
	return 0;
}


static IMG_INT
SGXReadHWPerfCBBW(IMG_UINT32							ui32BridgeID,
				  PVRSRV_BRIDGE_IN_SGX_READ_HWPERF_CB	*psSGXReadHWPerfCBIN,
				  PVRSRV_BRIDGE_OUT_SGX_READ_HWPERF_CB	*psSGXReadHWPerfCBOUT,
				  PVRSRV_PER_PROCESS_DATA				*psPerProc)
{
	IMG_HANDLE					hDevCookieInt;
	PVRSRV_SGX_HWPERF_CB_ENTRY	*psAllocated;
	IMG_HANDLE					hAllocatedHandle;
	IMG_UINT32					ui32AllocatedSize;

	PVRSRV_BRIDGE_ASSERT_CMD(ui32BridgeID, PVRSRV_BRIDGE_SGX_READ_HWPERF_CB);

	psSGXReadHWPerfCBOUT->eError =PVRSRVLookupHandle(psPerProc->psHandleBase,
							&hDevCookieInt,
							psSGXReadHWPerfCBIN->hDevCookie,
							PVRSRV_HANDLE_TYPE_DEV_NODE);

	if(psSGXReadHWPerfCBOUT->eError != PVRSRV_OK)
	{
		return 0;
	}

	ui32AllocatedSize = psSGXReadHWPerfCBIN->ui32ArraySize *
							sizeof(psSGXReadHWPerfCBIN->psHWPerfCBData[0]);
	ASSIGN_AND_EXIT_ON_ERROR(psSGXReadHWPerfCBOUT->eError,
	                    OSAllocMem(PVRSRV_OS_PAGEABLE_HEAP,
	                    ui32AllocatedSize,
	                    (IMG_VOID **)&psAllocated,
	                    &hAllocatedHandle,
						"Array of Hardware Performance Circular Buffer Data"));

	psSGXReadHWPerfCBOUT->eError = SGXReadHWPerfCBKM(hDevCookieInt,
													 psSGXReadHWPerfCBIN->ui32ArraySize,
													 psAllocated,
													 &psSGXReadHWPerfCBOUT->ui32DataCount,
													 &psSGXReadHWPerfCBOUT->ui32ClockSpeed,
													 &psSGXReadHWPerfCBOUT->ui32HostTimeStamp);
	if (psSGXReadHWPerfCBOUT->eError == PVRSRV_OK)
	{
		psSGXReadHWPerfCBOUT->eError = CopyToUserWrapper(psPerProc,
		                                                 ui32BridgeID,
		                                                 psSGXReadHWPerfCBIN->psHWPerfCBData,
		                                                 psAllocated,
		                                                 ui32AllocatedSize);
	}

	OSFreeMem(PVRSRV_OS_PAGEABLE_HEAP,
			  ui32AllocatedSize,
			  psAllocated,
			  hAllocatedHandle);
	

	return 0;
}


static IMG_INT
SGXDevInitPart2BW(IMG_UINT32 ui32BridgeID,
				  PVRSRV_BRIDGE_IN_SGXDEVINITPART2 *psSGXDevInitPart2IN,
				  PVRSRV_BRIDGE_OUT_SGXDEVINITPART2 *psSGXDevInitPart2OUT,
				  PVRSRV_PER_PROCESS_DATA *psPerProc)
{
	IMG_HANDLE hDevCookieInt;
#if defined (SUPPORT_SID_INTERFACE)
	PVRSRV_ERROR eError = PVRSRV_OK;
#else
	PVRSRV_ERROR eError;
#endif
	IMG_BOOL bDissociateFailed = IMG_FALSE;
	IMG_BOOL bLookupFailed = IMG_FALSE;
	IMG_BOOL bReleaseFailed = IMG_FALSE;
	IMG_HANDLE hDummy;
	IMG_UINT32 i;
#if defined (SUPPORT_SID_INTERFACE)
	SGX_BRIDGE_INIT_INFO_KM asInitInfoKM = {0};
#endif

	PVRSRV_BRIDGE_ASSERT_CMD(ui32BridgeID, PVRSRV_BRIDGE_SGX_DEVINITPART2);

	
	psSGXDevInitPart2OUT->ui32KMBuildOptions = SGX_BUILD_OPTIONS;

	if(!psPerProc->bInitProcess)
	{
		psSGXDevInitPart2OUT->eError = PVRSRV_ERROR_PROCESS_NOT_INITIALISED;
		return 0;
	}

	psSGXDevInitPart2OUT->eError =
		PVRSRVLookupHandle(psPerProc->psHandleBase,
						   &hDevCookieInt,
						   psSGXDevInitPart2IN->hDevCookie,
						   PVRSRV_HANDLE_TYPE_DEV_NODE);
	if(psSGXDevInitPart2OUT->eError != PVRSRV_OK)
	{
		return 0;
	}

	
	eError = PVRSRVLookupHandle(psPerProc->psHandleBase,
						   &hDummy,
						   psSGXDevInitPart2IN->sInitInfo.hKernelCCBMemInfo,
						   PVRSRV_HANDLE_TYPE_MEM_INFO);
	if (eError != PVRSRV_OK)
	{
		bLookupFailed = IMG_TRUE;
	}

	eError = PVRSRVLookupHandle(psPerProc->psHandleBase,
						   &hDummy,
						   psSGXDevInitPart2IN->sInitInfo.hKernelCCBCtlMemInfo,
						   PVRSRV_HANDLE_TYPE_MEM_INFO);
	if (eError != PVRSRV_OK)
	{
		bLookupFailed = IMG_TRUE;
	}

	eError = PVRSRVLookupHandle(psPerProc->psHandleBase,
						   &hDummy,
						   psSGXDevInitPart2IN->sInitInfo.hKernelCCBEventKickerMemInfo,
						   PVRSRV_HANDLE_TYPE_MEM_INFO);
	if (eError != PVRSRV_OK)
	{
		bLookupFailed = IMG_TRUE;
	}

	eError = PVRSRVLookupHandle(psPerProc->psHandleBase,
						   &hDummy,
						   psSGXDevInitPart2IN->sInitInfo.hKernelSGXHostCtlMemInfo,
						   PVRSRV_HANDLE_TYPE_MEM_INFO);
	if (eError != PVRSRV_OK)
	{
		bLookupFailed = IMG_TRUE;
	}

	eError = PVRSRVLookupHandle(psPerProc->psHandleBase,
						   &hDummy,
						   psSGXDevInitPart2IN->sInitInfo.hKernelSGXTA3DCtlMemInfo,
						   PVRSRV_HANDLE_TYPE_MEM_INFO);
	if (eError != PVRSRV_OK)
	{
		bLookupFailed = IMG_TRUE;
	}


	eError = PVRSRVLookupHandle(psPerProc->psHandleBase,
						   &hDummy,
						   psSGXDevInitPart2IN->sInitInfo.hKernelSGXMiscMemInfo,
						   PVRSRV_HANDLE_TYPE_MEM_INFO);
	if (eError != PVRSRV_OK)
	{
		bLookupFailed = IMG_TRUE;
	}

#if defined(SGX_SUPPORT_HWPROFILING)
	eError = PVRSRVLookupHandle(psPerProc->psHandleBase,
						   &hDummy,
						   psSGXDevInitPart2IN->sInitInfo.hKernelHWProfilingMemInfo,
						   PVRSRV_HANDLE_TYPE_MEM_INFO);
	if (eError != PVRSRV_OK)
	{
		bLookupFailed = IMG_TRUE;
	}
#endif

#if defined(SUPPORT_SGX_HWPERF)
	eError = PVRSRVLookupHandle(psPerProc->psHandleBase,
						   &hDummy,
						   psSGXDevInitPart2IN->sInitInfo.hKernelHWPerfCBMemInfo,
						   PVRSRV_HANDLE_TYPE_MEM_INFO);
	if (eError != PVRSRV_OK)
	{
		bLookupFailed = IMG_TRUE;
	}
#endif

	eError = PVRSRVLookupHandle(psPerProc->psHandleBase,
								&hDummy,
								psSGXDevInitPart2IN->sInitInfo.hKernelTASigBufferMemInfo,
								PVRSRV_HANDLE_TYPE_MEM_INFO);
	if (eError != PVRSRV_OK)
	{
		bLookupFailed = IMG_TRUE;
	}

	eError = PVRSRVLookupHandle(psPerProc->psHandleBase,
								&hDummy,
								psSGXDevInitPart2IN->sInitInfo.hKernel3DSigBufferMemInfo,
								PVRSRV_HANDLE_TYPE_MEM_INFO);
	if (eError != PVRSRV_OK)
	{
		bLookupFailed = IMG_TRUE;
	}

#if defined(FIX_HW_BRN_29702)
	eError = PVRSRVLookupHandle(psPerProc->psHandleBase,
						   &hDummy,
						   psSGXDevInitPart2IN->sInitInfo.hKernelCFIMemInfo,
						   PVRSRV_HANDLE_TYPE_MEM_INFO);
	if (eError != PVRSRV_OK)
	{
		bLookupFailed = IMG_TRUE;
	}
#endif

#if defined(FIX_HW_BRN_29823)
	eError = PVRSRVLookupHandle(psPerProc->psHandleBase,
						   &hDummy,
						   psSGXDevInitPart2IN->sInitInfo.hKernelDummyTermStreamMemInfo,
						   PVRSRV_HANDLE_TYPE_MEM_INFO);
	if (eError != PVRSRV_OK)
	{
		bLookupFailed = IMG_TRUE;
	}
#endif


#if defined(FIX_HW_BRN_31542)
	eError = PVRSRVLookupHandle(psPerProc->psHandleBase,
						   &hDummy,
						   psSGXDevInitPart2IN->sInitInfo.hKernelClearClipWAVDMStreamMemInfo,
						   PVRSRV_HANDLE_TYPE_MEM_INFO);
	if (eError != PVRSRV_OK)
	{
		bLookupFailed = IMG_TRUE;
	}
	eError = PVRSRVLookupHandle(psPerProc->psHandleBase,
						   &hDummy,
						   psSGXDevInitPart2IN->sInitInfo.hKernelClearClipWAIndexStreamMemInfo,
						   PVRSRV_HANDLE_TYPE_MEM_INFO);
	if (eError != PVRSRV_OK)
	{
		bLookupFailed = IMG_TRUE;
	}
	eError = PVRSRVLookupHandle(psPerProc->psHandleBase,
						   &hDummy,
						   psSGXDevInitPart2IN->sInitInfo.hKernelClearClipWAPDSMemInfo,
						   PVRSRV_HANDLE_TYPE_MEM_INFO);
	if (eError != PVRSRV_OK)
	{
		bLookupFailed = IMG_TRUE;
	}
	eError = PVRSRVLookupHandle(psPerProc->psHandleBase,
						   &hDummy,
						   psSGXDevInitPart2IN->sInitInfo.hKernelClearClipWAUSEMemInfo,
						   PVRSRV_HANDLE_TYPE_MEM_INFO);
	if (eError != PVRSRV_OK)
	{
		bLookupFailed = IMG_TRUE;
	}
	eError = PVRSRVLookupHandle(psPerProc->psHandleBase,
						   &hDummy,
						   psSGXDevInitPart2IN->sInitInfo.hKernelClearClipWAParamMemInfo,
						   PVRSRV_HANDLE_TYPE_MEM_INFO);
	if (eError != PVRSRV_OK)
	{
		bLookupFailed = IMG_TRUE;
	}
	eError = PVRSRVLookupHandle(psPerProc->psHandleBase,
						   &hDummy,
						   psSGXDevInitPart2IN->sInitInfo.hKernelClearClipWAPMPTMemInfo,
						   PVRSRV_HANDLE_TYPE_MEM_INFO);
	if (eError != PVRSRV_OK)
	{
		bLookupFailed = IMG_TRUE;
	}
	
	eError = PVRSRVLookupHandle(psPerProc->psHandleBase,
						   &hDummy,
						   psSGXDevInitPart2IN->sInitInfo.hKernelClearClipWATPCMemInfo,
						   PVRSRV_HANDLE_TYPE_MEM_INFO);
	if (eError != PVRSRV_OK)
	{
		bLookupFailed = IMG_TRUE;
	}
	eError = PVRSRVLookupHandle(psPerProc->psHandleBase,
						   &hDummy,
						   psSGXDevInitPart2IN->sInitInfo.hKernelClearClipWAPSGRgnHdrMemInfo,
						   PVRSRV_HANDLE_TYPE_MEM_INFO);
	if (eError != PVRSRV_OK)
	{
		bLookupFailed = IMG_TRUE;
	}
#endif

#if defined(SGX_FEATURE_VDM_CONTEXT_SWITCH) && defined(FIX_HW_BRN_31425)
	eError = PVRSRVLookupHandle(psPerProc->psHandleBase,
						   &hDummy,
						   psSGXDevInitPart2IN->sInitInfo.hKernelVDMSnapShotBufferMemInfo,
						   PVRSRV_HANDLE_TYPE_MEM_INFO);
	if (eError != PVRSRV_OK)
	{
		bLookupFailed = IMG_TRUE;
	}

	eError = PVRSRVLookupHandle(psPerProc->psHandleBase,
						   &hDummy,
						   psSGXDevInitPart2IN->sInitInfo.hKernelVDMCtrlStreamBufferMemInfo,
						   PVRSRV_HANDLE_TYPE_MEM_INFO);
	if (eError != PVRSRV_OK)
	{
		bLookupFailed = IMG_TRUE;
	}
#endif

#if defined(PVRSRV_USSE_EDM_STATUS_DEBUG)
	eError = PVRSRVLookupHandle(psPerProc->psHandleBase,
						   &hDummy,
						   psSGXDevInitPart2IN->sInitInfo.hKernelEDMStatusBufferMemInfo,
						   PVRSRV_HANDLE_TYPE_MEM_INFO);
	if (eError != PVRSRV_OK)
	{
		bLookupFailed = IMG_TRUE;
	}
#endif

#if defined(SGX_FEATURE_SPM_MODE_0)
	eError = PVRSRVLookupHandle(psPerProc->psHandleBase,
						   &hDummy,
						   psSGXDevInitPart2IN->sInitInfo.hKernelTmpDPMStateMemInfo,
						   PVRSRV_HANDLE_TYPE_MEM_INFO);
	if (eError != PVRSRV_OK)
	{
		bLookupFailed = IMG_TRUE;
	}
#endif

	for (i = 0; i < SGX_MAX_INIT_MEM_HANDLES; i++)
	{
#if defined (SUPPORT_SID_INTERFACE)
		IMG_SID hHandle = psSGXDevInitPart2IN->sInitInfo.asInitMemHandles[i];
#else
		IMG_HANDLE hHandle = psSGXDevInitPart2IN->sInitInfo.asInitMemHandles[i];
#endif

#if defined (SUPPORT_SID_INTERFACE)
		if (hHandle == 0)
#else
		if (hHandle == IMG_NULL)
#endif
		{
			continue;
		}

		eError = PVRSRVLookupHandle(psPerProc->psHandleBase,
							   &hDummy,
							   hHandle,
							   PVRSRV_HANDLE_TYPE_MEM_INFO);
		if (eError != PVRSRV_OK)
		{
			bLookupFailed = IMG_TRUE;
		}
	}

	if (bLookupFailed)
	{
		PVR_DPF((PVR_DBG_ERROR, "DevInitSGXPart2BW: A handle lookup failed"));
		psSGXDevInitPart2OUT->eError = PVRSRV_ERROR_INIT2_PHASE_FAILED;
		return 0;
	}

	
	eError = PVRSRVLookupAndReleaseHandle(psPerProc->psHandleBase,
#if defined (SUPPORT_SID_INTERFACE)
						   &asInitInfoKM.hKernelCCBMemInfo,
#else
						   &psSGXDevInitPart2IN->sInitInfo.hKernelCCBMemInfo,
#endif
						   psSGXDevInitPart2IN->sInitInfo.hKernelCCBMemInfo,
						   PVRSRV_HANDLE_TYPE_MEM_INFO);
	if (eError != PVRSRV_OK)
	{
		bReleaseFailed = IMG_TRUE;
	}

	eError = PVRSRVLookupAndReleaseHandle(psPerProc->psHandleBase,
#if defined (SUPPORT_SID_INTERFACE)
						   &asInitInfoKM.hKernelCCBCtlMemInfo,
#else
						   &psSGXDevInitPart2IN->sInitInfo.hKernelCCBCtlMemInfo,
#endif
						   psSGXDevInitPart2IN->sInitInfo.hKernelCCBCtlMemInfo,
						   PVRSRV_HANDLE_TYPE_MEM_INFO);
	if (eError != PVRSRV_OK)
	{
		bReleaseFailed = IMG_TRUE;
	}

	eError = PVRSRVLookupAndReleaseHandle(psPerProc->psHandleBase,
#if defined (SUPPORT_SID_INTERFACE)
						   &asInitInfoKM.hKernelCCBEventKickerMemInfo,
#else
						   &psSGXDevInitPart2IN->sInitInfo.hKernelCCBEventKickerMemInfo,
#endif
						   psSGXDevInitPart2IN->sInitInfo.hKernelCCBEventKickerMemInfo,
						   PVRSRV_HANDLE_TYPE_MEM_INFO);
	if (eError != PVRSRV_OK)
	{
		bReleaseFailed = IMG_TRUE;
	}


	eError = PVRSRVLookupAndReleaseHandle(psPerProc->psHandleBase,
#if defined (SUPPORT_SID_INTERFACE)
						   &asInitInfoKM.hKernelSGXHostCtlMemInfo,
#else
						   &psSGXDevInitPart2IN->sInitInfo.hKernelSGXHostCtlMemInfo,
#endif
						   psSGXDevInitPart2IN->sInitInfo.hKernelSGXHostCtlMemInfo,
						   PVRSRV_HANDLE_TYPE_MEM_INFO);
	if (eError != PVRSRV_OK)
	{
		bReleaseFailed = IMG_TRUE;
	}

	eError = PVRSRVLookupAndReleaseHandle(psPerProc->psHandleBase,
#if defined (SUPPORT_SID_INTERFACE)
						   &asInitInfoKM.hKernelSGXTA3DCtlMemInfo,
#else
						   &psSGXDevInitPart2IN->sInitInfo.hKernelSGXTA3DCtlMemInfo,
#endif
						   psSGXDevInitPart2IN->sInitInfo.hKernelSGXTA3DCtlMemInfo,
						   PVRSRV_HANDLE_TYPE_MEM_INFO);
	if (eError != PVRSRV_OK)
	{
		bReleaseFailed = IMG_TRUE;
	}

	eError = PVRSRVLookupAndReleaseHandle(psPerProc->psHandleBase,
#if defined (SUPPORT_SID_INTERFACE)
						   &asInitInfoKM.hKernelSGXMiscMemInfo,
#else
						   &psSGXDevInitPart2IN->sInitInfo.hKernelSGXMiscMemInfo,
#endif
						   psSGXDevInitPart2IN->sInitInfo.hKernelSGXMiscMemInfo,
						   PVRSRV_HANDLE_TYPE_MEM_INFO);
	if (eError != PVRSRV_OK)
	{
		bReleaseFailed = IMG_TRUE;
	}


#if defined(SGX_SUPPORT_HWPROFILING)
	eError = PVRSRVLookupAndReleaseHandle(psPerProc->psHandleBase,
#if defined (SUPPORT_SID_INTERFACE)
						   &asInitInfoKM.hKernelHWProfilingMemInfo,
#else
						   &psSGXDevInitPart2IN->sInitInfo.hKernelHWProfilingMemInfo,
#endif
						   psSGXDevInitPart2IN->sInitInfo.hKernelHWProfilingMemInfo,
						   PVRSRV_HANDLE_TYPE_MEM_INFO);
	if (eError != PVRSRV_OK)
	{
		bReleaseFailed = IMG_TRUE;
	}
#endif

#if defined(SUPPORT_SGX_HWPERF)
	eError = PVRSRVLookupAndReleaseHandle(psPerProc->psHandleBase,
#if defined (SUPPORT_SID_INTERFACE)
						   &asInitInfoKM.hKernelHWPerfCBMemInfo,
#else
						   &psSGXDevInitPart2IN->sInitInfo.hKernelHWPerfCBMemInfo,
#endif
						   psSGXDevInitPart2IN->sInitInfo.hKernelHWPerfCBMemInfo,
						   PVRSRV_HANDLE_TYPE_MEM_INFO);
	if (eError != PVRSRV_OK)
	{
		bReleaseFailed = IMG_TRUE;
	}
#endif

	eError = PVRSRVLookupAndReleaseHandle(psPerProc->psHandleBase,
#if defined (SUPPORT_SID_INTERFACE)
										  &asInitInfoKM.hKernelTASigBufferMemInfo,
#else
										  &psSGXDevInitPart2IN->sInitInfo.hKernelTASigBufferMemInfo,
#endif
										  psSGXDevInitPart2IN->sInitInfo.hKernelTASigBufferMemInfo,
										  PVRSRV_HANDLE_TYPE_MEM_INFO);
	if (eError != PVRSRV_OK)
	{
		bReleaseFailed = IMG_TRUE;
	}

	eError = PVRSRVLookupAndReleaseHandle(psPerProc->psHandleBase,
#if defined (SUPPORT_SID_INTERFACE)
										  &asInitInfoKM.hKernel3DSigBufferMemInfo,
#else
										  &psSGXDevInitPart2IN->sInitInfo.hKernel3DSigBufferMemInfo,
#endif
										  psSGXDevInitPart2IN->sInitInfo.hKernel3DSigBufferMemInfo,
										  PVRSRV_HANDLE_TYPE_MEM_INFO);
	if (eError != PVRSRV_OK)
	{
		bReleaseFailed = IMG_TRUE;
	}

#if defined(FIX_HW_BRN_29702)
	eError = PVRSRVLookupHandle(psPerProc->psHandleBase,
#if defined (SUPPORT_SID_INTERFACE)
						   &asInitInfoKM.hKernelCFIMemInfo,
#else
						   &psSGXDevInitPart2IN->sInitInfo.hKernelCFIMemInfo,
#endif
						   psSGXDevInitPart2IN->sInitInfo.hKernelCFIMemInfo,
						   PVRSRV_HANDLE_TYPE_MEM_INFO);
	if (eError != PVRSRV_OK)
	{
		bLookupFailed = IMG_TRUE;
	}
#endif

#if defined(FIX_HW_BRN_29823)
	eError = PVRSRVLookupAndReleaseHandle(psPerProc->psHandleBase,
#if defined (SUPPORT_SID_INTERFACE)
						   &asInitInfoKM.hKernelDummyTermStreamMemInfo,
#else
						   &psSGXDevInitPart2IN->sInitInfo.hKernelDummyTermStreamMemInfo,
#endif
						   psSGXDevInitPart2IN->sInitInfo.hKernelDummyTermStreamMemInfo,
						   PVRSRV_HANDLE_TYPE_MEM_INFO);
	if (eError != PVRSRV_OK)
	{
		bReleaseFailed = IMG_TRUE;
	}
#endif


#if defined(FIX_HW_BRN_31542)
	eError = PVRSRVLookupAndReleaseHandle(psPerProc->psHandleBase,
#if defined (SUPPORT_SID_INTERFACE)
						   &asInitInfoKM.hKernelClearClipWAVDMStreamMemInfo,
#else
						   &psSGXDevInitPart2IN->sInitInfo.hKernelClearClipWAVDMStreamMemInfo,
#endif
						   psSGXDevInitPart2IN->sInitInfo.hKernelClearClipWAVDMStreamMemInfo,
						   PVRSRV_HANDLE_TYPE_MEM_INFO);
	if (eError != PVRSRV_OK)
	{
		bReleaseFailed = IMG_TRUE;
	}
	eError = PVRSRVLookupAndReleaseHandle(psPerProc->psHandleBase,
#if defined (SUPPORT_SID_INTERFACE)
						   &asInitInfoKM.hKernelClearClipWAIndexStreamMemInfo,
#else
						   &psSGXDevInitPart2IN->sInitInfo.hKernelClearClipWAIndexStreamMemInfo,
#endif
						   psSGXDevInitPart2IN->sInitInfo.hKernelClearClipWAIndexStreamMemInfo,
						   PVRSRV_HANDLE_TYPE_MEM_INFO);
	if (eError != PVRSRV_OK)
	{
		bReleaseFailed = IMG_TRUE;
	}
	eError = PVRSRVLookupAndReleaseHandle(psPerProc->psHandleBase,
#if defined (SUPPORT_SID_INTERFACE)
						   &asInitInfoKM.hKernelClearClipWAPDSMemInfo,
#else
						   &psSGXDevInitPart2IN->sInitInfo.hKernelClearClipWAPDSMemInfo,
#endif
						   psSGXDevInitPart2IN->sInitInfo.hKernelClearClipWAPDSMemInfo,
						   PVRSRV_HANDLE_TYPE_MEM_INFO);
	if (eError != PVRSRV_OK)
	{
		bReleaseFailed = IMG_TRUE;
	}
	eError = PVRSRVLookupAndReleaseHandle(psPerProc->psHandleBase,
#if defined (SUPPORT_SID_INTERFACE)
						   &asInitInfoKM.hKernelClearClipWAUSEMemInfo,
#else
						   &psSGXDevInitPart2IN->sInitInfo.hKernelClearClipWAUSEMemInfo,
#endif
						   psSGXDevInitPart2IN->sInitInfo.hKernelClearClipWAUSEMemInfo,
						   PVRSRV_HANDLE_TYPE_MEM_INFO);
	if (eError != PVRSRV_OK)
	{
		bReleaseFailed = IMG_TRUE;
	}
	eError = PVRSRVLookupAndReleaseHandle(psPerProc->psHandleBase,
#if defined (SUPPORT_SID_INTERFACE)
						   &asInitInfoKM.hKernelClearClipWAParamMemInfo,
#else
						   &psSGXDevInitPart2IN->sInitInfo.hKernelClearClipWAParamMemInfo,
#endif
						   psSGXDevInitPart2IN->sInitInfo.hKernelClearClipWAParamMemInfo,
						   PVRSRV_HANDLE_TYPE_MEM_INFO);
	if (eError != PVRSRV_OK)
	{
		bReleaseFailed = IMG_TRUE;
	}
	eError = PVRSRVLookupAndReleaseHandle(psPerProc->psHandleBase,
#if defined (SUPPORT_SID_INTERFACE)
						   &asInitInfoKM.hKernelClearClipWAPMPTMemInfo,
#else
						   &psSGXDevInitPart2IN->sInitInfo.hKernelClearClipWAPMPTMemInfo,
#endif
						   psSGXDevInitPart2IN->sInitInfo.hKernelClearClipWAPMPTMemInfo,
						   PVRSRV_HANDLE_TYPE_MEM_INFO);
	if (eError != PVRSRV_OK)
	{
		bReleaseFailed = IMG_TRUE;
	}
	eError = PVRSRVLookupAndReleaseHandle(psPerProc->psHandleBase,
#if defined (SUPPORT_SID_INTERFACE)
						   &asInitInfoKM.hKernelClearClipWATPCMemInfo,
#else
						   &psSGXDevInitPart2IN->sInitInfo.hKernelClearClipWATPCMemInfo,
#endif
						   psSGXDevInitPart2IN->sInitInfo.hKernelClearClipWATPCMemInfo,
						   PVRSRV_HANDLE_TYPE_MEM_INFO);
	if (eError != PVRSRV_OK)
	{
		bReleaseFailed = IMG_TRUE;
	}
	eError = PVRSRVLookupAndReleaseHandle(psPerProc->psHandleBase,
#if defined (SUPPORT_SID_INTERFACE)
						   &asInitInfoKM.hKernelClearClipWAPSGRgnHdrMemInfo,
#else
						   &psSGXDevInitPart2IN->sInitInfo.hKernelClearClipWAPSGRgnHdrMemInfo,
#endif
						   psSGXDevInitPart2IN->sInitInfo.hKernelClearClipWAPSGRgnHdrMemInfo,
						   PVRSRV_HANDLE_TYPE_MEM_INFO);
	if (eError != PVRSRV_OK)
	{
		bReleaseFailed = IMG_TRUE;
	}
#endif
#if defined(SGX_FEATURE_VDM_CONTEXT_SWITCH) && defined(FIX_HW_BRN_31425)
	eError = PVRSRVLookupAndReleaseHandle(psPerProc->psHandleBase,
						   &psSGXDevInitPart2IN->sInitInfo.hKernelVDMSnapShotBufferMemInfo,
						   psSGXDevInitPart2IN->sInitInfo.hKernelVDMSnapShotBufferMemInfo,
						   PVRSRV_HANDLE_TYPE_MEM_INFO);
	if (eError != PVRSRV_OK)
	{
		bReleaseFailed = IMG_TRUE;
	}

	eError = PVRSRVLookupAndReleaseHandle(psPerProc->psHandleBase,
						   &psSGXDevInitPart2IN->sInitInfo.hKernelVDMCtrlStreamBufferMemInfo,
						   psSGXDevInitPart2IN->sInitInfo.hKernelVDMCtrlStreamBufferMemInfo,
						   PVRSRV_HANDLE_TYPE_MEM_INFO);
	if (eError != PVRSRV_OK)
	{
		bReleaseFailed = IMG_TRUE;
	}
#endif

#if defined(PVRSRV_USSE_EDM_STATUS_DEBUG)
	eError = PVRSRVLookupAndReleaseHandle(psPerProc->psHandleBase,
#if defined (SUPPORT_SID_INTERFACE)
						   &asInitInfoKM.hKernelEDMStatusBufferMemInfo,
#else
						   &psSGXDevInitPart2IN->sInitInfo.hKernelEDMStatusBufferMemInfo,
#endif
						   psSGXDevInitPart2IN->sInitInfo.hKernelEDMStatusBufferMemInfo,
						   PVRSRV_HANDLE_TYPE_MEM_INFO);
	if (eError != PVRSRV_OK)
	{
		bReleaseFailed = IMG_TRUE;
	}
#endif

#if defined(SGX_FEATURE_SPM_MODE_0)
	eError = PVRSRVLookupAndReleaseHandle(psPerProc->psHandleBase,
#if defined (SUPPORT_SID_INTERFACE)
						   &asInitInfoKM.hKernelTmpDPMStateMemInfo,
#else
						   &psSGXDevInitPart2IN->sInitInfo.hKernelTmpDPMStateMemInfo,
#endif
						   psSGXDevInitPart2IN->sInitInfo.hKernelTmpDPMStateMemInfo,
						   PVRSRV_HANDLE_TYPE_MEM_INFO);
	if (eError != PVRSRV_OK)
	{
		bReleaseFailed = IMG_TRUE;
	}
#endif


	for (i = 0; i < SGX_MAX_INIT_MEM_HANDLES; i++)
	{
#if defined (SUPPORT_SID_INTERFACE)
		IMG_SID hHandle = psSGXDevInitPart2IN->sInitInfo.asInitMemHandles[i];
		IMG_HANDLE *phHandleKM = &asInitInfoKM.asInitMemHandles[i];

		if (hHandle == 0)
#else
		IMG_HANDLE *phHandle = &psSGXDevInitPart2IN->sInitInfo.asInitMemHandles[i];

		if (*phHandle == IMG_NULL)
#endif
			continue;

		eError = PVRSRVLookupAndReleaseHandle(psPerProc->psHandleBase,
#if defined (SUPPORT_SID_INTERFACE)
							   phHandleKM,
							   hHandle,
#else
							   phHandle,
							   *phHandle,
#endif
							   PVRSRV_HANDLE_TYPE_MEM_INFO);
		if (eError != PVRSRV_OK)
		{
			bReleaseFailed = IMG_TRUE;
		}
	}

	if (bReleaseFailed)
	{
		PVR_DPF((PVR_DBG_ERROR, "DevInitSGXPart2BW: A handle release failed"));
		psSGXDevInitPart2OUT->eError = PVRSRV_ERROR_INIT2_PHASE_FAILED;
		
		PVR_DBG_BREAK;
		return 0;
	}

	
#if defined (SUPPORT_SID_INTERFACE)
	eError = PVRSRVDissociateDeviceMemKM(hDevCookieInt, asInitInfoKM.hKernelCCBMemInfo);
#else
	eError = PVRSRVDissociateDeviceMemKM(hDevCookieInt, psSGXDevInitPart2IN->sInitInfo.hKernelCCBMemInfo);
#endif
	if (eError != PVRSRV_OK)
	{
		bDissociateFailed = IMG_TRUE;
	}

#if defined (SUPPORT_SID_INTERFACE)
	eError = PVRSRVDissociateDeviceMemKM(hDevCookieInt, asInitInfoKM.hKernelCCBCtlMemInfo);
#else
	eError = PVRSRVDissociateDeviceMemKM(hDevCookieInt, psSGXDevInitPart2IN->sInitInfo.hKernelCCBCtlMemInfo);
#endif
	if (eError != PVRSRV_OK)
	{
		bDissociateFailed = IMG_TRUE;
	}

#if defined (SUPPORT_SID_INTERFACE)
	eError = PVRSRVDissociateDeviceMemKM(hDevCookieInt, asInitInfoKM.hKernelCCBEventKickerMemInfo);
#else
	eError = PVRSRVDissociateDeviceMemKM(hDevCookieInt, psSGXDevInitPart2IN->sInitInfo.hKernelCCBEventKickerMemInfo);
#endif
	if (eError != PVRSRV_OK)
	{
		bDissociateFailed = IMG_TRUE;
	}

#if defined (SUPPORT_SID_INTERFACE)
	eError = PVRSRVDissociateDeviceMemKM(hDevCookieInt, asInitInfoKM.hKernelSGXHostCtlMemInfo);
#else
	eError = PVRSRVDissociateDeviceMemKM(hDevCookieInt, psSGXDevInitPart2IN->sInitInfo.hKernelSGXHostCtlMemInfo);
#endif
	if (eError != PVRSRV_OK)
	{
		bDissociateFailed = IMG_TRUE;
	}

#if defined (SUPPORT_SID_INTERFACE)
	eError = PVRSRVDissociateDeviceMemKM(hDevCookieInt, asInitInfoKM.hKernelSGXTA3DCtlMemInfo);
#else
	eError = PVRSRVDissociateDeviceMemKM(hDevCookieInt, psSGXDevInitPart2IN->sInitInfo.hKernelSGXTA3DCtlMemInfo);
#endif
	if (eError != PVRSRV_OK)
	{
		bDissociateFailed = IMG_TRUE;
	}

	
#if defined (SUPPORT_SID_INTERFACE)
	eError = PVRSRVDissociateDeviceMemKM(hDevCookieInt, asInitInfoKM.hKernelSGXMiscMemInfo);
#else
	eError = PVRSRVDissociateDeviceMemKM(hDevCookieInt, psSGXDevInitPart2IN->sInitInfo.hKernelSGXMiscMemInfo);
#endif
	if (eError != PVRSRV_OK)
	{
		bDissociateFailed = IMG_TRUE;
	}


#if defined(SGX_SUPPORT_HWPROFILING)
#if defined (SUPPORT_SID_INTERFACE)
	eError = PVRSRVDissociateDeviceMemKM(hDevCookieInt, asInitInfoKM.hKernelHWProfilingMemInfo);
	if (eError != PVRSRV_OK)
	{
		bDissociateFailed = IMG_TRUE;
	}
#else
	eError = PVRSRVDissociateDeviceMemKM(hDevCookieInt, psSGXDevInitPart2IN->sInitInfo.hKernelHWProfilingMemInfo);
	bDissociateFailed |= (IMG_BOOL)(eError != PVRSRV_OK);
#endif
#endif

#if defined(SUPPORT_SGX_HWPERF)
#if defined (SUPPORT_SID_INTERFACE)
	eError = PVRSRVDissociateDeviceMemKM(hDevCookieInt, asInitInfoKM.hKernelHWPerfCBMemInfo);
#else
	eError = PVRSRVDissociateDeviceMemKM(hDevCookieInt, psSGXDevInitPart2IN->sInitInfo.hKernelHWPerfCBMemInfo);
#endif
	if (eError != PVRSRV_OK)
	{
		bDissociateFailed = IMG_TRUE;
	}
#endif

#if defined (SUPPORT_SID_INTERFACE)
	eError = PVRSRVDissociateDeviceMemKM(hDevCookieInt, asInitInfoKM.hKernelTASigBufferMemInfo);
#else
	eError = PVRSRVDissociateDeviceMemKM(hDevCookieInt, psSGXDevInitPart2IN->sInitInfo.hKernelTASigBufferMemInfo);
#endif
	if (eError != PVRSRV_OK)
	{
		bDissociateFailed = IMG_TRUE;
	}

#if defined (SUPPORT_SID_INTERFACE)
	eError = PVRSRVDissociateDeviceMemKM(hDevCookieInt, asInitInfoKM.hKernel3DSigBufferMemInfo);
#else
	eError = PVRSRVDissociateDeviceMemKM(hDevCookieInt, psSGXDevInitPart2IN->sInitInfo.hKernel3DSigBufferMemInfo);
#endif
	if (eError != PVRSRV_OK)
	{
		bDissociateFailed = IMG_TRUE;
	}

#if defined(FIX_HW_BRN_29702)
#if defined (SUPPORT_SID_INTERFACE)
	eError = PVRSRVDissociateDeviceMemKM(hDevCookieInt, asInitInfoKM.hKernelCFIMemInfo);
	if (eError != PVRSRV_OK)
	{
		bDissociateFailed = IMG_TRUE;
	}
#else
	eError = PVRSRVDissociateDeviceMemKM(hDevCookieInt, psSGXDevInitPart2IN->sInitInfo.hKernelCFIMemInfo);
	bDissociateFailed |= (IMG_BOOL)(eError != PVRSRV_OK);
#endif
#endif

#if defined(FIX_HW_BRN_29823)
#if defined (SUPPORT_SID_INTERFACE)
	eError = PVRSRVDissociateDeviceMemKM(hDevCookieInt, asInitInfoKM.hKernelDummyTermStreamMemInfo);
	if (eError != PVRSRV_OK)
	{
		bDissociateFailed = IMG_TRUE;
	}
#else
	eError = PVRSRVDissociateDeviceMemKM(hDevCookieInt, psSGXDevInitPart2IN->sInitInfo.hKernelDummyTermStreamMemInfo);
	bDissociateFailed |= (IMG_BOOL)(eError != PVRSRV_OK);
#endif
#endif

#if defined(FIX_HW_BRN_31542)
#if defined (SUPPORT_SID_INTERFACE)
	eError = PVRSRVDissociateDeviceMemKM(hDevCookieInt, asInitInfoKM.hKernelClearClipWAVDMStreamMemInfo);
	if (eError != PVRSRV_OK)
	{
		bDissociateFailed = IMG_TRUE;
	}
#else
	eError = PVRSRVDissociateDeviceMemKM(hDevCookieInt, psSGXDevInitPart2IN->sInitInfo.hKernelClearClipWAVDMStreamMemInfo);
	bDissociateFailed |= (IMG_BOOL)(eError != PVRSRV_OK);
#endif
#if defined (SUPPORT_SID_INTERFACE)
	eError = PVRSRVDissociateDeviceMemKM(hDevCookieInt, asInitInfoKM.hKernelClearClipWAIndexStreamMemInfo);
	if (eError != PVRSRV_OK)
	{
		bDissociateFailed = IMG_TRUE;
	}
#else
	eError = PVRSRVDissociateDeviceMemKM(hDevCookieInt, psSGXDevInitPart2IN->sInitInfo.hKernelClearClipWAIndexStreamMemInfo);
	bDissociateFailed |= (IMG_BOOL)(eError != PVRSRV_OK);
#endif
#if defined (SUPPORT_SID_INTERFACE)
	eError = PVRSRVDissociateDeviceMemKM(hDevCookieInt, asInitInfoKM.hKernelClearClipWAPDSMemInfo);
	if (eError != PVRSRV_OK)
	{
		bDissociateFailed = IMG_TRUE;
	}
#else
	eError = PVRSRVDissociateDeviceMemKM(hDevCookieInt, psSGXDevInitPart2IN->sInitInfo.hKernelClearClipWAPDSMemInfo);
	bDissociateFailed |= (IMG_BOOL)(eError != PVRSRV_OK);
#endif
#if defined (SUPPORT_SID_INTERFACE)
	eError = PVRSRVDissociateDeviceMemKM(hDevCookieInt, asInitInfoKM.hKernelClearClipWAUSEMemInfo);
	if (eError != PVRSRV_OK)
	{
		bDissociateFailed = IMG_TRUE;
	}
#else
	eError = PVRSRVDissociateDeviceMemKM(hDevCookieInt, psSGXDevInitPart2IN->sInitInfo.hKernelClearClipWAUSEMemInfo);
	bDissociateFailed |= (IMG_BOOL)(eError != PVRSRV_OK);
#endif
#if defined (SUPPORT_SID_INTERFACE)
	eError = PVRSRVDissociateDeviceMemKM(hDevCookieInt, asInitInfoKM.hKernelClearClipWAParamMemInfo);
	if (eError != PVRSRV_OK)
	{
		bDissociateFailed = IMG_TRUE;
	}
#else
	eError = PVRSRVDissociateDeviceMemKM(hDevCookieInt, psSGXDevInitPart2IN->sInitInfo.hKernelClearClipWAParamMemInfo);
	bDissociateFailed |= (IMG_BOOL)(eError != PVRSRV_OK);
#endif
#if defined (SUPPORT_SID_INTERFACE)
	eError = PVRSRVDissociateDeviceMemKM(hDevCookieInt, asInitInfoKM.hKernelClearClipWAPMPTMemInfo);
	if (eError != PVRSRV_OK)
	{
		bDissociateFailed = IMG_TRUE;
	}
#else
	eError = PVRSRVDissociateDeviceMemKM(hDevCookieInt, psSGXDevInitPart2IN->sInitInfo.hKernelClearClipWAPMPTMemInfo);
	bDissociateFailed |= (IMG_BOOL)(eError != PVRSRV_OK);
#endif
#if defined (SUPPORT_SID_INTERFACE)
	eError = PVRSRVDissociateDeviceMemKM(hDevCookieInt, asInitInfoKM.hKernelClearClipWATPCMemInfo);
	if (eError != PVRSRV_OK)
	{
		bDissociateFailed = IMG_TRUE;
	}
#else
	eError = PVRSRVDissociateDeviceMemKM(hDevCookieInt, psSGXDevInitPart2IN->sInitInfo.hKernelClearClipWATPCMemInfo);
	bDissociateFailed |= (IMG_BOOL)(eError != PVRSRV_OK);
#endif
#if defined (SUPPORT_SID_INTERFACE)
	eError = PVRSRVDissociateDeviceMemKM(hDevCookieInt, asInitInfoKM.hKernelClearClipWAPSGRgnHdrMemInfo);
	if (eError != PVRSRV_OK)
	{
		bDissociateFailed = IMG_TRUE;
	}
#else
	eError = PVRSRVDissociateDeviceMemKM(hDevCookieInt, psSGXDevInitPart2IN->sInitInfo.hKernelClearClipWAPSGRgnHdrMemInfo);
	bDissociateFailed |= (IMG_BOOL)(eError != PVRSRV_OK);
#endif
#endif

#if defined(SGX_FEATURE_VDM_CONTEXT_SWITCH) && defined(FIX_HW_BRN_31425)
	eError = PVRSRVDissociateDeviceMemKM(hDevCookieInt, psSGXDevInitPart2IN->sInitInfo.hKernelVDMSnapShotBufferMemInfo);
	bDissociateFailed |= (IMG_BOOL)(eError != PVRSRV_OK);
	
	eError = PVRSRVDissociateDeviceMemKM(hDevCookieInt, psSGXDevInitPart2IN->sInitInfo.hKernelVDMCtrlStreamBufferMemInfo);
	bDissociateFailed |= (IMG_BOOL)(eError != PVRSRV_OK);
#endif

#if defined(PVRSRV_USSE_EDM_STATUS_DEBUG)
#if defined (SUPPORT_SID_INTERFACE)
	eError = PVRSRVDissociateDeviceMemKM(hDevCookieInt, asInitInfoKM.hKernelEDMStatusBufferMemInfo);
	if (eError != PVRSRV_OK)
	{
		bDissociateFailed = IMG_TRUE;
	}
#else
	eError = PVRSRVDissociateDeviceMemKM(hDevCookieInt, psSGXDevInitPart2IN->sInitInfo.hKernelEDMStatusBufferMemInfo);
	bDissociateFailed |= (IMG_BOOL)(eError != PVRSRV_OK);
#endif
#endif

#if defined(SGX_FEATURE_SPM_MODE_0)
#if defined (SUPPORT_SID_INTERFACE)
	eError = PVRSRVDissociateDeviceMemKM(hDevCookieInt, asInitInfoKM.hKernelTmpDPMStateMemInfo);
#else
	eError = PVRSRVDissociateDeviceMemKM(hDevCookieInt, psSGXDevInitPart2IN->sInitInfo.hKernelTmpDPMStateMemInfo);
#endif
	if (eError != PVRSRV_OK)
	{
		bDissociateFailed = IMG_TRUE;
	}
#endif

	for (i = 0; i < SGX_MAX_INIT_MEM_HANDLES; i++)
	{
#if defined (SUPPORT_SID_INTERFACE)
		IMG_HANDLE hHandle = asInitInfoKM.asInitMemHandles[i];
#else
		IMG_HANDLE hHandle = psSGXDevInitPart2IN->sInitInfo.asInitMemHandles[i];
#endif

		if (hHandle == IMG_NULL)
			continue;

		eError = PVRSRVDissociateDeviceMemKM(hDevCookieInt, hHandle);
		if (eError != PVRSRV_OK)
		{
			bDissociateFailed = IMG_TRUE;
		}
	}

	
	if(bDissociateFailed)
	{
#if defined (SUPPORT_SID_INTERFACE)
		PVRSRVFreeDeviceMemKM(hDevCookieInt, asInitInfoKM.hKernelCCBMemInfo);
		PVRSRVFreeDeviceMemKM(hDevCookieInt, asInitInfoKM.hKernelCCBCtlMemInfo);
		PVRSRVFreeDeviceMemKM(hDevCookieInt, asInitInfoKM.hKernelSGXHostCtlMemInfo);
		PVRSRVFreeDeviceMemKM(hDevCookieInt, asInitInfoKM.hKernelSGXTA3DCtlMemInfo);
		PVRSRVFreeDeviceMemKM(hDevCookieInt, asInitInfoKM.hKernelSGXMiscMemInfo);
#else
		PVRSRVFreeDeviceMemKM(hDevCookieInt, psSGXDevInitPart2IN->sInitInfo.hKernelCCBMemInfo);
		PVRSRVFreeDeviceMemKM(hDevCookieInt, psSGXDevInitPart2IN->sInitInfo.hKernelCCBCtlMemInfo);
		PVRSRVFreeDeviceMemKM(hDevCookieInt, psSGXDevInitPart2IN->sInitInfo.hKernelSGXHostCtlMemInfo);
		PVRSRVFreeDeviceMemKM(hDevCookieInt, psSGXDevInitPart2IN->sInitInfo.hKernelSGXTA3DCtlMemInfo);
		PVRSRVFreeDeviceMemKM(hDevCookieInt, psSGXDevInitPart2IN->sInitInfo.hKernelSGXMiscMemInfo);
#endif

		for (i = 0; i < SGX_MAX_INIT_MEM_HANDLES; i++)
		{
#if defined (SUPPORT_SID_INTERFACE)
			IMG_HANDLE hHandle = asInitInfoKM.asInitMemHandles[i];

			if (hHandle == 0)
#else
			IMG_HANDLE hHandle = psSGXDevInitPart2IN->sInitInfo.asInitMemHandles[i];

			if (hHandle == IMG_NULL)
#endif
				continue;

			PVRSRVFreeDeviceMemKM(hDevCookieInt, (PVRSRV_KERNEL_MEM_INFO *)hHandle);

		}

		PVR_DPF((PVR_DBG_ERROR, "DevInitSGXPart2BW: A dissociate failed"));

		psSGXDevInitPart2OUT->eError = PVRSRV_ERROR_INIT2_PHASE_FAILED;

		
		PVR_DBG_BREAK;
		return 0;
	}

#if defined (SUPPORT_SID_INTERFACE)
	asInitInfoKM.sScripts               = psSGXDevInitPart2IN->sInitInfo.sScripts;
	asInitInfoKM.ui32ClientBuildOptions = psSGXDevInitPart2IN->sInitInfo.ui32ClientBuildOptions;
	asInitInfoKM.sSGXStructSizes        = psSGXDevInitPart2IN->sInitInfo.sSGXStructSizes;
	asInitInfoKM.ui32CacheControl       = psSGXDevInitPart2IN->sInitInfo.ui32CacheControl;
	asInitInfoKM.ui32EDMTaskReg0        = psSGXDevInitPart2IN->sInitInfo.ui32EDMTaskReg0;
	asInitInfoKM.ui32EDMTaskReg1        = psSGXDevInitPart2IN->sInitInfo.ui32EDMTaskReg1;
	asInitInfoKM.ui32ClkGateStatusReg   = psSGXDevInitPart2IN->sInitInfo.ui32ClkGateStatusReg;
	asInitInfoKM.ui32ClkGateStatusMask  = psSGXDevInitPart2IN->sInitInfo.ui32ClkGateStatusMask;

	OSMemCopy(&asInitInfoKM.asInitDevData ,
			  &psSGXDevInitPart2IN->sInitInfo.asInitDevData,
			  sizeof(asInitInfoKM.asInitDevData));
	OSMemCopy(&asInitInfoKM.aui32HostKickAddr,
			  &psSGXDevInitPart2IN->sInitInfo.aui32HostKickAddr,
			  sizeof(asInitInfoKM.aui32HostKickAddr));

	psSGXDevInitPart2OUT->eError =
		DevInitSGXPart2KM(psPerProc,
						  hDevCookieInt,
						  &asInitInfoKM);
#else
	psSGXDevInitPart2OUT->eError =
		DevInitSGXPart2KM(psPerProc,
						  hDevCookieInt,
						  &psSGXDevInitPart2IN->sInitInfo);
#endif

	return 0;
}


static IMG_INT
SGXRegisterHWRenderContextBW(IMG_UINT32 ui32BridgeID,
							 PVRSRV_BRIDGE_IN_SGX_REGISTER_HW_RENDER_CONTEXT *psSGXRegHWRenderContextIN,
							 PVRSRV_BRIDGE_OUT_SGX_REGISTER_HW_RENDER_CONTEXT *psSGXRegHWRenderContextOUT,
							 PVRSRV_PER_PROCESS_DATA *psPerProc)
{
	IMG_HANDLE hDevCookieInt;
	IMG_HANDLE hHWRenderContextInt;

	PVRSRV_BRIDGE_ASSERT_CMD(ui32BridgeID, PVRSRV_BRIDGE_SGX_REGISTER_HW_RENDER_CONTEXT);

	NEW_HANDLE_BATCH_OR_ERROR(psSGXRegHWRenderContextOUT->eError, psPerProc, 1);

	psSGXRegHWRenderContextOUT->eError =
		PVRSRVLookupHandle(psPerProc->psHandleBase,
						   &hDevCookieInt,
						   psSGXRegHWRenderContextIN->hDevCookie,
						   PVRSRV_HANDLE_TYPE_DEV_NODE);
	if(psSGXRegHWRenderContextOUT->eError != PVRSRV_OK)
	{
		return 0;
	}

	hHWRenderContextInt =
		SGXRegisterHWRenderContextKM(hDevCookieInt,
									 &psSGXRegHWRenderContextIN->sHWRenderContextDevVAddr,
									 psPerProc);

	if (hHWRenderContextInt == IMG_NULL)
	{
		psSGXRegHWRenderContextOUT->eError = PVRSRV_ERROR_UNABLE_TO_REGISTER_CONTEXT;
		return 0;
	}

	PVRSRVAllocHandleNR(psPerProc->psHandleBase,
					  &psSGXRegHWRenderContextOUT->hHWRenderContext,
					  hHWRenderContextInt,
					  PVRSRV_HANDLE_TYPE_SGX_HW_RENDER_CONTEXT,
					  PVRSRV_HANDLE_ALLOC_FLAG_NONE);

	COMMIT_HANDLE_BATCH_OR_ERROR(psSGXRegHWRenderContextOUT->eError, psPerProc);

	return 0;
}


static IMG_INT
SGXUnregisterHWRenderContextBW(IMG_UINT32 ui32BridgeID,
							   PVRSRV_BRIDGE_IN_SGX_UNREGISTER_HW_RENDER_CONTEXT *psSGXUnregHWRenderContextIN,
							   PVRSRV_BRIDGE_RETURN *psRetOUT,
							   PVRSRV_PER_PROCESS_DATA *psPerProc)
{
	IMG_HANDLE hHWRenderContextInt;

	PVRSRV_BRIDGE_ASSERT_CMD(ui32BridgeID, PVRSRV_BRIDGE_SGX_UNREGISTER_HW_RENDER_CONTEXT);

	psRetOUT->eError =
		PVRSRVLookupHandle(psPerProc->psHandleBase,
						   &hHWRenderContextInt,
						   psSGXUnregHWRenderContextIN->hHWRenderContext,
						   PVRSRV_HANDLE_TYPE_SGX_HW_RENDER_CONTEXT);
	if(psRetOUT->eError != PVRSRV_OK)
	{
		return 0;
	}

	psRetOUT->eError = SGXUnregisterHWRenderContextKM(hHWRenderContextInt,
													  psSGXUnregHWRenderContextIN->bForceCleanup);
	if(psRetOUT->eError != PVRSRV_OK)
	{
		return 0;
	}

	psRetOUT->eError =
		PVRSRVReleaseHandle(psPerProc->psHandleBase,
							psSGXUnregHWRenderContextIN->hHWRenderContext,
							PVRSRV_HANDLE_TYPE_SGX_HW_RENDER_CONTEXT);

	return 0;
}


static IMG_INT
SGXRegisterHWTransferContextBW(IMG_UINT32 ui32BridgeID,
							 PVRSRV_BRIDGE_IN_SGX_REGISTER_HW_TRANSFER_CONTEXT *psSGXRegHWTransferContextIN,
							 PVRSRV_BRIDGE_OUT_SGX_REGISTER_HW_TRANSFER_CONTEXT *psSGXRegHWTransferContextOUT,
							 PVRSRV_PER_PROCESS_DATA *psPerProc)
{
	IMG_HANDLE hDevCookieInt;
	IMG_HANDLE hHWTransferContextInt;

	PVRSRV_BRIDGE_ASSERT_CMD(ui32BridgeID, PVRSRV_BRIDGE_SGX_REGISTER_HW_TRANSFER_CONTEXT);

	NEW_HANDLE_BATCH_OR_ERROR(psSGXRegHWTransferContextOUT->eError, psPerProc, 1);

	psSGXRegHWTransferContextOUT->eError =
		PVRSRVLookupHandle(psPerProc->psHandleBase,
						   &hDevCookieInt,
						   psSGXRegHWTransferContextIN->hDevCookie,
						   PVRSRV_HANDLE_TYPE_DEV_NODE);
	if(psSGXRegHWTransferContextOUT->eError != PVRSRV_OK)
	{
		return 0;
	}

	hHWTransferContextInt =
		SGXRegisterHWTransferContextKM(hDevCookieInt,
									   &psSGXRegHWTransferContextIN->sHWTransferContextDevVAddr,
									   psPerProc);

	if (hHWTransferContextInt == IMG_NULL)
	{
		psSGXRegHWTransferContextOUT->eError = PVRSRV_ERROR_UNABLE_TO_REGISTER_CONTEXT;
		return 0;
	}

	PVRSRVAllocHandleNR(psPerProc->psHandleBase,
					  &psSGXRegHWTransferContextOUT->hHWTransferContext,
					  hHWTransferContextInt,
					  PVRSRV_HANDLE_TYPE_SGX_HW_TRANSFER_CONTEXT,
					  PVRSRV_HANDLE_ALLOC_FLAG_NONE);

	COMMIT_HANDLE_BATCH_OR_ERROR(psSGXRegHWTransferContextOUT->eError, psPerProc);

	return 0;
}


static IMG_INT
SGXUnregisterHWTransferContextBW(IMG_UINT32 ui32BridgeID,
							   PVRSRV_BRIDGE_IN_SGX_UNREGISTER_HW_TRANSFER_CONTEXT *psSGXUnregHWTransferContextIN,
							   PVRSRV_BRIDGE_RETURN *psRetOUT,
							   PVRSRV_PER_PROCESS_DATA *psPerProc)
{
#if defined (SUPPORT_SID_INTERFACE)
	IMG_HANDLE hHWTransferContextInt = 0;
#else
	IMG_HANDLE hHWTransferContextInt;
#endif

	PVRSRV_BRIDGE_ASSERT_CMD(ui32BridgeID, PVRSRV_BRIDGE_SGX_UNREGISTER_HW_TRANSFER_CONTEXT);

	psRetOUT->eError =
		PVRSRVLookupHandle(psPerProc->psHandleBase,
						   &hHWTransferContextInt,
						   psSGXUnregHWTransferContextIN->hHWTransferContext,
						   PVRSRV_HANDLE_TYPE_SGX_HW_TRANSFER_CONTEXT);
	if(psRetOUT->eError != PVRSRV_OK)
	{
		return 0;
	}

	psRetOUT->eError = SGXUnregisterHWTransferContextKM(hHWTransferContextInt,
														psSGXUnregHWTransferContextIN->bForceCleanup);
	if(psRetOUT->eError != PVRSRV_OK)
	{
		return 0;
	}

	psRetOUT->eError =
		PVRSRVReleaseHandle(psPerProc->psHandleBase,
							psSGXUnregHWTransferContextIN->hHWTransferContext,
							PVRSRV_HANDLE_TYPE_SGX_HW_TRANSFER_CONTEXT);

	return 0;
}


#if defined(SGX_FEATURE_2D_HARDWARE)
static IMG_INT
SGXRegisterHW2DContextBW(IMG_UINT32 ui32BridgeID,
							 PVRSRV_BRIDGE_IN_SGX_REGISTER_HW_2D_CONTEXT *psSGXRegHW2DContextIN,
							 PVRSRV_BRIDGE_OUT_SGX_REGISTER_HW_2D_CONTEXT *psSGXRegHW2DContextOUT,
							 PVRSRV_PER_PROCESS_DATA *psPerProc)
{
	IMG_HANDLE hDevCookieInt;
	IMG_HANDLE hHW2DContextInt;

	PVRSRV_BRIDGE_ASSERT_CMD(ui32BridgeID, PVRSRV_BRIDGE_SGX_REGISTER_HW_2D_CONTEXT);

	NEW_HANDLE_BATCH_OR_ERROR(psSGXRegHW2DContextOUT->eError, psPerProc, 1);

	psSGXRegHW2DContextOUT->eError =
		PVRSRVLookupHandle(psPerProc->psHandleBase,
						   &hDevCookieInt,
						   psSGXRegHW2DContextIN->hDevCookie,
						   PVRSRV_HANDLE_TYPE_DEV_NODE);
	if(psSGXRegHW2DContextOUT->eError != PVRSRV_OK)
	{
		return 0;
	}

	hHW2DContextInt =
		SGXRegisterHW2DContextKM(hDevCookieInt,
								 &psSGXRegHW2DContextIN->sHW2DContextDevVAddr,
								 psPerProc);

	if (hHW2DContextInt == IMG_NULL)
	{
		psSGXRegHW2DContextOUT->eError = PVRSRV_ERROR_UNABLE_TO_REGISTER_CONTEXT;
		return 0;
	}

	PVRSRVAllocHandleNR(psPerProc->psHandleBase,
					  &psSGXRegHW2DContextOUT->hHW2DContext,
					  hHW2DContextInt,
					  PVRSRV_HANDLE_TYPE_SGX_HW_2D_CONTEXT,
					  PVRSRV_HANDLE_ALLOC_FLAG_NONE);

	COMMIT_HANDLE_BATCH_OR_ERROR(psSGXRegHW2DContextOUT->eError, psPerProc);

	return 0;
}


static IMG_INT
SGXUnregisterHW2DContextBW(IMG_UINT32 ui32BridgeID,
							   PVRSRV_BRIDGE_IN_SGX_UNREGISTER_HW_2D_CONTEXT *psSGXUnregHW2DContextIN,
							   PVRSRV_BRIDGE_RETURN *psRetOUT,
							   PVRSRV_PER_PROCESS_DATA *psPerProc)
{
	IMG_HANDLE hHW2DContextInt;

	PVRSRV_BRIDGE_ASSERT_CMD(ui32BridgeID, PVRSRV_BRIDGE_SGX_UNREGISTER_HW_2D_CONTEXT);

	psRetOUT->eError =
		PVRSRVLookupHandle(psPerProc->psHandleBase,
						   &hHW2DContextInt,
						   psSGXUnregHW2DContextIN->hHW2DContext,
						   PVRSRV_HANDLE_TYPE_SGX_HW_2D_CONTEXT);
	if(psRetOUT->eError != PVRSRV_OK)
	{
		return 0;
	}

	psRetOUT->eError = SGXUnregisterHW2DContextKM(hHW2DContextInt,
												  psSGXUnregHW2DContextIN->bForceCleanup);
	if(psRetOUT->eError != PVRSRV_OK)
	{
		return 0;
	}

	psRetOUT->eError =
		PVRSRVReleaseHandle(psPerProc->psHandleBase,
							psSGXUnregHW2DContextIN->hHW2DContext,
							PVRSRV_HANDLE_TYPE_SGX_HW_2D_CONTEXT);

	return 0;
}
#endif 

static IMG_INT
SGXFlushHWRenderTargetBW(IMG_UINT32 ui32BridgeID,
						  PVRSRV_BRIDGE_IN_SGX_FLUSH_HW_RENDER_TARGET *psSGXFlushHWRenderTargetIN,
						  PVRSRV_BRIDGE_RETURN *psRetOUT,
						  PVRSRV_PER_PROCESS_DATA *psPerProc)
{
	IMG_HANDLE hDevCookieInt;
	PVRSRV_BRIDGE_ASSERT_CMD(ui32BridgeID, PVRSRV_BRIDGE_SGX_FLUSH_HW_RENDER_TARGET);

	psRetOUT->eError =
		PVRSRVLookupHandle(psPerProc->psHandleBase,
						   &hDevCookieInt,
						   psSGXFlushHWRenderTargetIN->hDevCookie,
						   PVRSRV_HANDLE_TYPE_DEV_NODE);
	if(psRetOUT->eError != PVRSRV_OK)
	{
		return 0;
	}

	psRetOUT->eError = SGXFlushHWRenderTargetKM(hDevCookieInt, psSGXFlushHWRenderTargetIN->sHWRTDataSetDevVAddr, IMG_FALSE);

	return 0;
}


static IMG_INT
SGX2DQueryBlitsCompleteBW(IMG_UINT32 ui32BridgeID,
						  PVRSRV_BRIDGE_IN_2DQUERYBLTSCOMPLETE *ps2DQueryBltsCompleteIN,
						  PVRSRV_BRIDGE_RETURN *psRetOUT,
						  PVRSRV_PER_PROCESS_DATA *psPerProc)
{
	IMG_HANDLE hDevCookieInt;
	IMG_VOID *pvSyncInfo;
	PVRSRV_SGXDEV_INFO *psDevInfo;

	PVRSRV_BRIDGE_ASSERT_CMD(ui32BridgeID, PVRSRV_BRIDGE_SGX_2DQUERYBLTSCOMPLETE);

	psRetOUT->eError =
		PVRSRVLookupHandle(psPerProc->psHandleBase,
						   &hDevCookieInt,
						   ps2DQueryBltsCompleteIN->hDevCookie,
						   PVRSRV_HANDLE_TYPE_DEV_NODE);
	if(psRetOUT->eError != PVRSRV_OK)
	{
		return 0;
	}

	psRetOUT->eError =
		PVRSRVLookupHandle(psPerProc->psHandleBase,
						   &pvSyncInfo,
						   ps2DQueryBltsCompleteIN->hKernSyncInfo,
						   PVRSRV_HANDLE_TYPE_SYNC_INFO);
	if(psRetOUT->eError != PVRSRV_OK)
	{
		return 0;
	}

	psDevInfo = (PVRSRV_SGXDEV_INFO *)((PVRSRV_DEVICE_NODE *)hDevCookieInt)->pvDevice;

	psRetOUT->eError =
		SGX2DQueryBlitsCompleteKM(psDevInfo,
								  (PVRSRV_KERNEL_SYNC_INFO *)pvSyncInfo,
								  ps2DQueryBltsCompleteIN->bWaitForComplete);

	return 0;
}


static IMG_INT
SGXFindSharedPBDescBW(IMG_UINT32 ui32BridgeID,
					  PVRSRV_BRIDGE_IN_SGXFINDSHAREDPBDESC *psSGXFindSharedPBDescIN,
					  PVRSRV_BRIDGE_OUT_SGXFINDSHAREDPBDESC *psSGXFindSharedPBDescOUT,
					  PVRSRV_PER_PROCESS_DATA *psPerProc)
{
	IMG_HANDLE hDevCookieInt;
	PVRSRV_KERNEL_MEM_INFO *psSharedPBDescKernelMemInfo;
	PVRSRV_KERNEL_MEM_INFO *psHWPBDescKernelMemInfo;
	PVRSRV_KERNEL_MEM_INFO *psBlockKernelMemInfo;
	PVRSRV_KERNEL_MEM_INFO *psHWBlockKernelMemInfo;
	PVRSRV_KERNEL_MEM_INFO **ppsSharedPBDescSubKernelMemInfos = IMG_NULL;
	IMG_UINT32 ui32SharedPBDescSubKernelMemInfosCount = 0;
	IMG_UINT32 i;
	IMG_HANDLE hSharedPBDesc = IMG_NULL;

	PVRSRV_BRIDGE_ASSERT_CMD(ui32BridgeID, PVRSRV_BRIDGE_SGX_FINDSHAREDPBDESC);

	NEW_HANDLE_BATCH_OR_ERROR(psSGXFindSharedPBDescOUT->eError, psPerProc, PVRSRV_BRIDGE_SGX_SHAREDPBDESC_MAX_SUBMEMINFOS + 4);

	psSGXFindSharedPBDescOUT->hSharedPBDesc = IMG_NULL;

	psSGXFindSharedPBDescOUT->eError =
		PVRSRVLookupHandle(psPerProc->psHandleBase,
						   &hDevCookieInt,
						   psSGXFindSharedPBDescIN->hDevCookie,
						   PVRSRV_HANDLE_TYPE_DEV_NODE);
	if(psSGXFindSharedPBDescOUT->eError != PVRSRV_OK)
		goto PVRSRV_BRIDGE_SGX_FINDSHAREDPBDESC_EXIT;

	psSGXFindSharedPBDescOUT->eError =
		SGXFindSharedPBDescKM(psPerProc, hDevCookieInt,
							  psSGXFindSharedPBDescIN->bLockOnFailure,
							  psSGXFindSharedPBDescIN->ui32TotalPBSize,
							  &hSharedPBDesc,
							  &psSharedPBDescKernelMemInfo,
							  &psHWPBDescKernelMemInfo,
							  &psBlockKernelMemInfo,
							  &psHWBlockKernelMemInfo,
							  &ppsSharedPBDescSubKernelMemInfos,
							  &ui32SharedPBDescSubKernelMemInfosCount);
	if(psSGXFindSharedPBDescOUT->eError != PVRSRV_OK)
		goto PVRSRV_BRIDGE_SGX_FINDSHAREDPBDESC_EXIT;

	PVR_ASSERT(ui32SharedPBDescSubKernelMemInfosCount
			   <= PVRSRV_BRIDGE_SGX_SHAREDPBDESC_MAX_SUBMEMINFOS);

	psSGXFindSharedPBDescOUT->ui32SharedPBDescSubKernelMemInfoHandlesCount =
		ui32SharedPBDescSubKernelMemInfosCount;

	if(hSharedPBDesc == IMG_NULL)
	{
		psSGXFindSharedPBDescOUT->hSharedPBDescKernelMemInfoHandle = 0;
		
		goto PVRSRV_BRIDGE_SGX_FINDSHAREDPBDESC_EXIT;
	}

	PVRSRVAllocHandleNR(psPerProc->psHandleBase,
					  &psSGXFindSharedPBDescOUT->hSharedPBDesc,
					  hSharedPBDesc,
					  PVRSRV_HANDLE_TYPE_SHARED_PB_DESC,
					  PVRSRV_HANDLE_ALLOC_FLAG_NONE);

	
	PVRSRVAllocSubHandleNR(psPerProc->psHandleBase,
					  &psSGXFindSharedPBDescOUT->hSharedPBDescKernelMemInfoHandle,
					  psSharedPBDescKernelMemInfo,
					  PVRSRV_HANDLE_TYPE_MEM_INFO_REF,
					  PVRSRV_HANDLE_ALLOC_FLAG_MULTI,
					  psSGXFindSharedPBDescOUT->hSharedPBDesc);

	PVRSRVAllocSubHandleNR(psPerProc->psHandleBase,
					  &psSGXFindSharedPBDescOUT->hHWPBDescKernelMemInfoHandle,
					  psHWPBDescKernelMemInfo,
					  PVRSRV_HANDLE_TYPE_MEM_INFO_REF,
					  PVRSRV_HANDLE_ALLOC_FLAG_MULTI,
					  psSGXFindSharedPBDescOUT->hSharedPBDesc);

	PVRSRVAllocSubHandleNR(psPerProc->psHandleBase,
				  &psSGXFindSharedPBDescOUT->hBlockKernelMemInfoHandle,
				  psBlockKernelMemInfo,
				  PVRSRV_HANDLE_TYPE_MEM_INFO_REF,
				  PVRSRV_HANDLE_ALLOC_FLAG_MULTI,
				  psSGXFindSharedPBDescOUT->hSharedPBDesc);

	PVRSRVAllocSubHandleNR(psPerProc->psHandleBase,
				  &psSGXFindSharedPBDescOUT->hHWBlockKernelMemInfoHandle,
				  psHWBlockKernelMemInfo,
				  PVRSRV_HANDLE_TYPE_MEM_INFO_REF,
				  PVRSRV_HANDLE_ALLOC_FLAG_MULTI,
				  psSGXFindSharedPBDescOUT->hSharedPBDesc);


	for(i=0; i<ui32SharedPBDescSubKernelMemInfosCount; i++)
	{
		PVRSRV_BRIDGE_OUT_SGXFINDSHAREDPBDESC *psSGXFindSharedPBDescOut =
			psSGXFindSharedPBDescOUT;

			PVRSRVAllocSubHandleNR(psPerProc->psHandleBase,
							  &psSGXFindSharedPBDescOut->ahSharedPBDescSubKernelMemInfoHandles[i],
							  ppsSharedPBDescSubKernelMemInfos[i],
							  PVRSRV_HANDLE_TYPE_MEM_INFO_REF,
							  PVRSRV_HANDLE_ALLOC_FLAG_MULTI,
							  psSGXFindSharedPBDescOUT->hSharedPBDescKernelMemInfoHandle);
	}

PVRSRV_BRIDGE_SGX_FINDSHAREDPBDESC_EXIT:
	if (ppsSharedPBDescSubKernelMemInfos != IMG_NULL)
	{
		OSFreeMem(PVRSRV_OS_PAGEABLE_HEAP,
				  sizeof(PVRSRV_KERNEL_MEM_INFO *) * ui32SharedPBDescSubKernelMemInfosCount,
				  ppsSharedPBDescSubKernelMemInfos,
				  IMG_NULL);
	}

	if(psSGXFindSharedPBDescOUT->eError != PVRSRV_OK)
	{
		if(hSharedPBDesc != IMG_NULL)
		{
			SGXUnrefSharedPBDescKM(hSharedPBDesc);
		}
	}
	else
	{
		COMMIT_HANDLE_BATCH_OR_ERROR(psSGXFindSharedPBDescOUT->eError, psPerProc);
	}

	return 0;
}


static IMG_INT
SGXUnrefSharedPBDescBW(IMG_UINT32 ui32BridgeID,
					   PVRSRV_BRIDGE_IN_SGXUNREFSHAREDPBDESC *psSGXUnrefSharedPBDescIN,
					   PVRSRV_BRIDGE_OUT_SGXUNREFSHAREDPBDESC *psSGXUnrefSharedPBDescOUT,
					   PVRSRV_PER_PROCESS_DATA *psPerProc)
{
	IMG_HANDLE hSharedPBDesc;

	PVRSRV_BRIDGE_ASSERT_CMD(ui32BridgeID, PVRSRV_BRIDGE_SGX_UNREFSHAREDPBDESC);

	psSGXUnrefSharedPBDescOUT->eError =
		PVRSRVLookupHandle(psPerProc->psHandleBase,
						   &hSharedPBDesc,
						   psSGXUnrefSharedPBDescIN->hSharedPBDesc,
						   PVRSRV_HANDLE_TYPE_SHARED_PB_DESC);
	if(psSGXUnrefSharedPBDescOUT->eError != PVRSRV_OK)
	{
		return 0;
	}

	psSGXUnrefSharedPBDescOUT->eError =
		SGXUnrefSharedPBDescKM(hSharedPBDesc);

	if(psSGXUnrefSharedPBDescOUT->eError != PVRSRV_OK)
	{
		return 0;
	}

	psSGXUnrefSharedPBDescOUT->eError =
		PVRSRVReleaseHandle(psPerProc->psHandleBase,
						   psSGXUnrefSharedPBDescIN->hSharedPBDesc,
						   PVRSRV_HANDLE_TYPE_SHARED_PB_DESC);

	return 0;
}


static IMG_INT
SGXAddSharedPBDescBW(IMG_UINT32 ui32BridgeID,
					 PVRSRV_BRIDGE_IN_SGXADDSHAREDPBDESC *psSGXAddSharedPBDescIN,
					 PVRSRV_BRIDGE_OUT_SGXADDSHAREDPBDESC *psSGXAddSharedPBDescOUT,
					 PVRSRV_PER_PROCESS_DATA *psPerProc)
{
	IMG_HANDLE hDevCookieInt;
	PVRSRV_KERNEL_MEM_INFO *psSharedPBDescKernelMemInfo;
	PVRSRV_KERNEL_MEM_INFO *psHWPBDescKernelMemInfo;
	PVRSRV_KERNEL_MEM_INFO *psBlockKernelMemInfo;
	PVRSRV_KERNEL_MEM_INFO *psHWBlockKernelMemInfo;
	IMG_UINT32 ui32KernelMemInfoHandlesCount =
		psSGXAddSharedPBDescIN->ui32KernelMemInfoHandlesCount;
	IMG_INT ret = 0;
#if defined (SUPPORT_SID_INTERFACE)
	IMG_SID *phKernelMemInfoHandles = 0;
#else
	IMG_HANDLE *phKernelMemInfoHandles = IMG_NULL;
#endif
	PVRSRV_KERNEL_MEM_INFO **ppsKernelMemInfos = IMG_NULL;
	IMG_UINT32 i;
	PVRSRV_ERROR eError;
	IMG_HANDLE hSharedPBDesc = IMG_NULL;

	PVRSRV_BRIDGE_ASSERT_CMD(ui32BridgeID, PVRSRV_BRIDGE_SGX_ADDSHAREDPBDESC);

	NEW_HANDLE_BATCH_OR_ERROR(psSGXAddSharedPBDescOUT->eError, psPerProc, 1);

	psSGXAddSharedPBDescOUT->hSharedPBDesc = IMG_NULL;

	PVR_ASSERT(ui32KernelMemInfoHandlesCount
			   <= PVRSRV_BRIDGE_SGX_SHAREDPBDESC_MAX_SUBMEMINFOS);

	eError = PVRSRVLookupHandle(psPerProc->psHandleBase,
								&hDevCookieInt,
								psSGXAddSharedPBDescIN->hDevCookie,
								PVRSRV_HANDLE_TYPE_DEV_NODE);
	if(eError != PVRSRV_OK)
	{
		goto PVRSRV_BRIDGE_SGX_ADDSHAREDPBDESC_RETURN_RESULT;
	}

	eError = PVRSRVLookupHandle(psPerProc->psHandleBase,
								(IMG_VOID **)&psSharedPBDescKernelMemInfo,
								psSGXAddSharedPBDescIN->hSharedPBDescKernelMemInfo,
								PVRSRV_HANDLE_TYPE_SHARED_SYS_MEM_INFO);
	if(eError != PVRSRV_OK)
	{
		goto PVRSRV_BRIDGE_SGX_ADDSHAREDPBDESC_RETURN_RESULT;
	}

	eError = PVRSRVLookupHandle(psPerProc->psHandleBase,
								(IMG_VOID **)&psHWPBDescKernelMemInfo,
								psSGXAddSharedPBDescIN->hHWPBDescKernelMemInfo,
								PVRSRV_HANDLE_TYPE_MEM_INFO);
	if(eError != PVRSRV_OK)
	{
		goto PVRSRV_BRIDGE_SGX_ADDSHAREDPBDESC_RETURN_RESULT;
	}

	eError = PVRSRVLookupHandle(psPerProc->psHandleBase,
								(IMG_VOID **)&psBlockKernelMemInfo,
								psSGXAddSharedPBDescIN->hBlockKernelMemInfo,
								PVRSRV_HANDLE_TYPE_SHARED_SYS_MEM_INFO);
	if(eError != PVRSRV_OK)
	{
		goto PVRSRV_BRIDGE_SGX_ADDSHAREDPBDESC_RETURN_RESULT;
	}

	eError = PVRSRVLookupHandle(psPerProc->psHandleBase,
								(IMG_VOID **)&psHWBlockKernelMemInfo,
								psSGXAddSharedPBDescIN->hHWBlockKernelMemInfo,
								PVRSRV_HANDLE_TYPE_MEM_INFO);
	if(eError != PVRSRV_OK)
	{
		goto PVRSRV_BRIDGE_SGX_ADDSHAREDPBDESC_RETURN_RESULT;
	}


	if(!OSAccessOK(PVR_VERIFY_READ,
				   psSGXAddSharedPBDescIN->phKernelMemInfoHandles,
				   ui32KernelMemInfoHandlesCount * sizeof(IMG_HANDLE)))
	{
		PVR_DPF((PVR_DBG_ERROR, "%s: PVRSRV_BRIDGE_SGX_ADDSHAREDPBDESC:"
				 " Invalid phKernelMemInfos pointer", __FUNCTION__));
		ret = -EFAULT;
		goto PVRSRV_BRIDGE_SGX_ADDSHAREDPBDESC_RETURN_RESULT;
	}

	eError = OSAllocMem(PVRSRV_OS_PAGEABLE_HEAP,
				  ui32KernelMemInfoHandlesCount * sizeof(IMG_HANDLE),
				  (IMG_VOID **)&phKernelMemInfoHandles,
				  0,
				  "Array of Handles");
	if (eError != PVRSRV_OK)
	{
		goto PVRSRV_BRIDGE_SGX_ADDSHAREDPBDESC_RETURN_RESULT;
	}

	if(CopyFromUserWrapper(psPerProc,
			               ui32BridgeID,
			               phKernelMemInfoHandles,
						   psSGXAddSharedPBDescIN->phKernelMemInfoHandles,
						   ui32KernelMemInfoHandlesCount * sizeof(IMG_HANDLE))
	   != PVRSRV_OK)
	{
		ret = -EFAULT;
		goto PVRSRV_BRIDGE_SGX_ADDSHAREDPBDESC_RETURN_RESULT;
	}

	eError = OSAllocMem(PVRSRV_OS_PAGEABLE_HEAP,
				  ui32KernelMemInfoHandlesCount * sizeof(PVRSRV_KERNEL_MEM_INFO *),
				  (IMG_VOID **)&ppsKernelMemInfos,
				  0,
				  "Array of pointers to Kernel Memory Info");
	if (eError != PVRSRV_OK)
	{
		goto PVRSRV_BRIDGE_SGX_ADDSHAREDPBDESC_RETURN_RESULT;
	}

	for(i=0; i<ui32KernelMemInfoHandlesCount; i++)
	{
		eError = PVRSRVLookupHandle(psPerProc->psHandleBase,
									(IMG_VOID **)&ppsKernelMemInfos[i],
									phKernelMemInfoHandles[i],
									PVRSRV_HANDLE_TYPE_MEM_INFO);
		if(eError != PVRSRV_OK)
		{
			goto PVRSRV_BRIDGE_SGX_ADDSHAREDPBDESC_RETURN_RESULT;
		}
	}

	
	 
	eError = PVRSRVReleaseHandle(psPerProc->psHandleBase,
								psSGXAddSharedPBDescIN->hSharedPBDescKernelMemInfo,
								PVRSRV_HANDLE_TYPE_SHARED_SYS_MEM_INFO);
	PVR_ASSERT(eError == PVRSRV_OK);

	 
	eError = PVRSRVReleaseHandle(psPerProc->psHandleBase,
								psSGXAddSharedPBDescIN->hHWPBDescKernelMemInfo,
								PVRSRV_HANDLE_TYPE_MEM_INFO);
	PVR_ASSERT(eError == PVRSRV_OK);

	 
	eError = PVRSRVReleaseHandle(psPerProc->psHandleBase,
								psSGXAddSharedPBDescIN->hBlockKernelMemInfo,
								PVRSRV_HANDLE_TYPE_SHARED_SYS_MEM_INFO);
	PVR_ASSERT(eError == PVRSRV_OK);

	 
	eError = PVRSRVReleaseHandle(psPerProc->psHandleBase,
								psSGXAddSharedPBDescIN->hHWBlockKernelMemInfo,
								PVRSRV_HANDLE_TYPE_MEM_INFO);
	PVR_ASSERT(eError == PVRSRV_OK);

	for(i=0; i<ui32KernelMemInfoHandlesCount; i++)
	{
		 
		eError = PVRSRVReleaseHandle(psPerProc->psHandleBase,
									phKernelMemInfoHandles[i],
									PVRSRV_HANDLE_TYPE_MEM_INFO);
		PVR_ASSERT(eError == PVRSRV_OK);
	}

	eError = SGXAddSharedPBDescKM(psPerProc, hDevCookieInt,
								  psSharedPBDescKernelMemInfo,
								  psHWPBDescKernelMemInfo,
								  psBlockKernelMemInfo,
								  psHWBlockKernelMemInfo,
								  psSGXAddSharedPBDescIN->ui32TotalPBSize,
								  &hSharedPBDesc,
								  ppsKernelMemInfos,
								  ui32KernelMemInfoHandlesCount,
								  psSGXAddSharedPBDescIN->sHWPBDescDevVAddr);


	if (eError != PVRSRV_OK)
	{
		goto PVRSRV_BRIDGE_SGX_ADDSHAREDPBDESC_RETURN_RESULT;
	}

	PVRSRVAllocHandleNR(psPerProc->psHandleBase,
				  &psSGXAddSharedPBDescOUT->hSharedPBDesc,
				  hSharedPBDesc,
				  PVRSRV_HANDLE_TYPE_SHARED_PB_DESC,
				  PVRSRV_HANDLE_ALLOC_FLAG_NONE);

PVRSRV_BRIDGE_SGX_ADDSHAREDPBDESC_RETURN_RESULT:

	if(phKernelMemInfoHandles)
	{
		OSFreeMem(PVRSRV_OS_PAGEABLE_HEAP,
				  psSGXAddSharedPBDescIN->ui32KernelMemInfoHandlesCount * sizeof(IMG_HANDLE),
				  (IMG_VOID *)phKernelMemInfoHandles,
				  0);
	}
	if(ppsKernelMemInfos)
	{
		OSFreeMem(PVRSRV_OS_PAGEABLE_HEAP,
				  psSGXAddSharedPBDescIN->ui32KernelMemInfoHandlesCount * sizeof(PVRSRV_KERNEL_MEM_INFO *),
				  (IMG_VOID *)ppsKernelMemInfos,
				  0);
	}

	if(ret == 0 && eError == PVRSRV_OK)
	{
		COMMIT_HANDLE_BATCH_OR_ERROR(psSGXAddSharedPBDescOUT->eError, psPerProc);
	}

	psSGXAddSharedPBDescOUT->eError = eError;

	return ret;
}

static IMG_INT
SGXGetInfoForSrvinitBW(IMG_UINT32 ui32BridgeID,
					   PVRSRV_BRIDGE_IN_SGXINFO_FOR_SRVINIT *psSGXInfoForSrvinitIN,
					   PVRSRV_BRIDGE_OUT_SGXINFO_FOR_SRVINIT *psSGXInfoForSrvinitOUT,
					   PVRSRV_PER_PROCESS_DATA *psPerProc)
{
	IMG_HANDLE hDevCookieInt;
	IMG_UINT32 i;
#if defined (SUPPORT_SID_INTERFACE)
	PVRSRV_HEAP_INFO_KM asHeapInfo[PVRSRV_MAX_CLIENT_HEAPS];
#endif
	PVRSRV_BRIDGE_ASSERT_CMD(ui32BridgeID, PVRSRV_BRIDGE_SGXINFO_FOR_SRVINIT);

	NEW_HANDLE_BATCH_OR_ERROR(psSGXInfoForSrvinitOUT->eError, psPerProc, PVRSRV_MAX_CLIENT_HEAPS);

	if(!psPerProc->bInitProcess)
	{
		psSGXInfoForSrvinitOUT->eError = PVRSRV_ERROR_PROCESS_NOT_INITIALISED;
		return 0;
	}

	psSGXInfoForSrvinitOUT->eError =
		PVRSRVLookupHandle(psPerProc->psHandleBase, &hDevCookieInt,
						   psSGXInfoForSrvinitIN->hDevCookie,
						   PVRSRV_HANDLE_TYPE_DEV_NODE);

	if(psSGXInfoForSrvinitOUT->eError != PVRSRV_OK)
	{
		return 0;
	}

	psSGXInfoForSrvinitOUT->eError =
		SGXGetInfoForSrvinitKM(hDevCookieInt,
#if defined (SUPPORT_SID_INTERFACE)
							   &asHeapInfo[0],
							   &psSGXInfoForSrvinitOUT->sInitInfo.sPDDevPAddr);
#else
							   &psSGXInfoForSrvinitOUT->sInitInfo);
#endif

	if(psSGXInfoForSrvinitOUT->eError != PVRSRV_OK)
	{
		return 0;
	}

	for(i = 0; i < PVRSRV_MAX_CLIENT_HEAPS; i++)
	{
		PVRSRV_HEAP_INFO *psHeapInfo;

		psHeapInfo = &psSGXInfoForSrvinitOUT->sInitInfo.asHeapInfo[i];

#if defined (SUPPORT_SID_INTERFACE)
		if ((asHeapInfo[i].ui32HeapID != (IMG_UINT32)SGX_UNDEFINED_HEAP_ID) &&
			(asHeapInfo[i].hDevMemHeap != IMG_NULL))
		{
			
			PVRSRVAllocHandleNR(psPerProc->psHandleBase,
							  &psHeapInfo->hDevMemHeap,
							  asHeapInfo[i].hDevMemHeap,
							  PVRSRV_HANDLE_TYPE_DEV_MEM_HEAP,
							  PVRSRV_HANDLE_ALLOC_FLAG_SHARED);
		}
		else
		{
			psHeapInfo->hDevMemHeap = 0;
		}

		psHeapInfo->ui32HeapID       = asHeapInfo[i].ui32HeapID;
		psHeapInfo->sDevVAddrBase    = asHeapInfo[i].sDevVAddrBase;
		psHeapInfo->ui32HeapByteSize = asHeapInfo[i].ui32HeapByteSize;
		psHeapInfo->ui32Attribs      = asHeapInfo[i].ui32Attribs;
		psHeapInfo->ui32XTileStride  = asHeapInfo[i].ui32XTileStride;
#else
		if (psHeapInfo->ui32HeapID != (IMG_UINT32)SGX_UNDEFINED_HEAP_ID)
		{
			IMG_HANDLE hDevMemHeapExt;

			if (psHeapInfo->hDevMemHeap != IMG_NULL)
			{
				
				PVRSRVAllocHandleNR(psPerProc->psHandleBase,
								  &hDevMemHeapExt,
								  psHeapInfo->hDevMemHeap,
								  PVRSRV_HANDLE_TYPE_DEV_MEM_HEAP,
								  PVRSRV_HANDLE_ALLOC_FLAG_SHARED);
				psHeapInfo->hDevMemHeap = hDevMemHeapExt;
			}
		}
#endif
	}

	COMMIT_HANDLE_BATCH_OR_ERROR(psSGXInfoForSrvinitOUT->eError, psPerProc);

	return 0;
}

#if defined(PDUMP)
static IMG_VOID
DumpBufferArray(PVRSRV_PER_PROCESS_DATA   *psPerProc,
#if defined (SUPPORT_SID_INTERFACE)
				PSGX_KICKTA_DUMP_BUFFER_KM psBufferArray,
#else
				PSGX_KICKTA_DUMP_BUFFER	psBufferArray,
#endif
				IMG_UINT32                 ui32BufferArrayLength,
				IMG_BOOL                   bDumpPolls)
{
	IMG_UINT32	i;

	for (i=0; i<ui32BufferArrayLength; i++)
	{
#if defined (SUPPORT_SID_INTERFACE)
		PSGX_KICKTA_DUMP_BUFFER_KM psBuffer;
#else
		PSGX_KICKTA_DUMP_BUFFER	psBuffer;
#endif
		PVRSRV_KERNEL_MEM_INFO    *psCtrlMemInfoKM;
		IMG_CHAR * pszName;
		IMG_HANDLE hUniqueTag;
		IMG_UINT32	ui32Offset;

		psBuffer = &psBufferArray[i];
		pszName = psBuffer->pszName;
		if (!pszName)
		{
			pszName = "Nameless buffer";
		}

		hUniqueTag = MAKEUNIQUETAG((PVRSRV_KERNEL_MEM_INFO *)psBuffer->hKernelMemInfo);

	#if defined(SUPPORT_SGX_NEW_STATUS_VALS)
		psCtrlMemInfoKM	= ((PVRSRV_KERNEL_MEM_INFO *)psBuffer->hCtrlKernelMemInfo);
		ui32Offset =  psBuffer->sCtrlDevVAddr.uiAddr - psCtrlMemInfoKM->sDevVAddr.uiAddr;
	#else
		psCtrlMemInfoKM = ((PVRSRV_KERNEL_MEM_INFO *)psBuffer->hKernelMemInfo)->psKernelSyncInfo->psSyncDataMemInfoKM;
		ui32Offset = offsetof(PVRSRV_SYNC_DATA, ui32ReadOpsComplete);
	#endif

		if (psBuffer->ui32Start <= psBuffer->ui32End)
		{
			if (bDumpPolls)
			{
				PDUMPCOMMENTWITHFLAGS(0, "Wait for %s space\r\n", pszName);
				PDUMPCBP(psCtrlMemInfoKM,
						 ui32Offset,
						 psBuffer->ui32Start,
						 psBuffer->ui32SpaceUsed,
						 psBuffer->ui32BufferSize,
						 0,
						 MAKEUNIQUETAG(psCtrlMemInfoKM));
			}

			PDUMPCOMMENTWITHFLAGS(0, "%s\r\n", pszName);
			PDUMPMEMUM(psPerProc,
					 IMG_NULL,
					 psBuffer->pvLinAddr,
					 (PVRSRV_KERNEL_MEM_INFO*)psBuffer->hKernelMemInfo,
					 psBuffer->ui32Start,
					 psBuffer->ui32End - psBuffer->ui32Start,
					 0,
					 hUniqueTag);
		}
		else
		{
			

			if (bDumpPolls)
			{
				PDUMPCOMMENTWITHFLAGS(0, "Wait for %s space\r\n", pszName);
				PDUMPCBP(psCtrlMemInfoKM,
						 ui32Offset,
						 psBuffer->ui32Start,
						 psBuffer->ui32BackEndLength,
						 psBuffer->ui32BufferSize,
						 0,
						 MAKEUNIQUETAG(psCtrlMemInfoKM));
			}
			PDUMPCOMMENTWITHFLAGS(0, "%s (part 1)\r\n", pszName);
			PDUMPMEMUM(psPerProc,
					 IMG_NULL,
					 psBuffer->pvLinAddr,
					 (PVRSRV_KERNEL_MEM_INFO*)psBuffer->hKernelMemInfo,
					 psBuffer->ui32Start,
					 psBuffer->ui32BackEndLength,
					 0,
					 hUniqueTag);

			if (bDumpPolls)
			{
				PDUMPMEMPOL(psCtrlMemInfoKM,
							ui32Offset,
							0,
							0xFFFFFFFF,
							PDUMP_POLL_OPERATOR_NOTEQUAL,
							0,
							MAKEUNIQUETAG(psCtrlMemInfoKM));

				PDUMPCOMMENTWITHFLAGS(0, "Wait for %s space\r\n", pszName);
				PDUMPCBP(psCtrlMemInfoKM,
						 ui32Offset,
						 0,
						 psBuffer->ui32End,
						 psBuffer->ui32BufferSize,
						 0,
						 MAKEUNIQUETAG(psCtrlMemInfoKM));
			}
			PDUMPCOMMENTWITHFLAGS(0, "%s (part 2)\r\n", pszName);
			PDUMPMEMUM(psPerProc,
					 IMG_NULL,
					 psBuffer->pvLinAddr,
					 (PVRSRV_KERNEL_MEM_INFO*)psBuffer->hKernelMemInfo,
					 0,
					 psBuffer->ui32End,
					 0,
					 hUniqueTag);
		}
	}
}
static IMG_INT
SGXPDumpBufferArrayBW(IMG_UINT32 ui32BridgeID,
				   PVRSRV_BRIDGE_IN_PDUMP_BUFFER_ARRAY *psPDumpBufferArrayIN,
				   IMG_VOID *psBridgeOut,
				   PVRSRV_PER_PROCESS_DATA *psPerProc)
{
	IMG_UINT32 i;
#if defined (SUPPORT_SID_INTERFACE)
	SGX_KICKTA_DUMP_BUFFER *psUMPtr;
	SGX_KICKTA_DUMP_BUFFER_KM *psKickTADumpBufferKM, *psKMPtr;
#else
	SGX_KICKTA_DUMP_BUFFER *psKickTADumpBuffer;
#endif
	IMG_UINT32 ui32BufferArrayLength =
		psPDumpBufferArrayIN->ui32BufferArrayLength;
	IMG_UINT32 ui32BufferArraySize =
		ui32BufferArrayLength * sizeof(SGX_KICKTA_DUMP_BUFFER);
	PVRSRV_ERROR eError = PVRSRV_ERROR_TOO_FEW_BUFFERS;

	PVR_UNREFERENCED_PARAMETER(psBridgeOut);

	PVRSRV_BRIDGE_ASSERT_CMD(ui32BridgeID, PVRSRV_BRIDGE_SGX_PDUMP_BUFFER_ARRAY);

#if defined (SUPPORT_SID_INTERFACE)
	if(OSAllocMem(PVRSRV_OS_PAGEABLE_HEAP,
				  ui32BufferArraySize,
				  (IMG_PVOID *)&psKickTADumpBufferKM, 0,
				  "Array of Kick Tile Accelerator Dump Buffer") != PVRSRV_OK)
#else
	if(OSAllocMem(PVRSRV_OS_PAGEABLE_HEAP,
				  ui32BufferArraySize,
				  (IMG_PVOID *)&psKickTADumpBuffer, 0,
				  "Array of Kick Tile Accelerator Dump Buffer") != PVRSRV_OK)
#endif				  
	{
		return -ENOMEM;
	}

#if !defined (SUPPORT_SID_INTERFACE)
	if(CopyFromUserWrapper(psPerProc,
			               ui32BridgeID,
						   psKickTADumpBuffer,
						   psPDumpBufferArrayIN->psBufferArray,
						   ui32BufferArraySize) != PVRSRV_OK)
	{
		OSFreeMem(PVRSRV_OS_PAGEABLE_HEAP, ui32BufferArraySize, psKickTADumpBuffer, 0);
		
		return -EFAULT;
	}
#endif

	for(i = 0; i < ui32BufferArrayLength; i++)
	{
#if defined (SUPPORT_SID_INTERFACE)
		IMG_VOID *pvMemInfo = IMG_NULL;
		psUMPtr = &psPDumpBufferArrayIN->psBufferArray[i];
		psKMPtr = &psKickTADumpBufferKM[i];
#else
		IMG_VOID *pvMemInfo;
#endif

		eError = PVRSRVLookupHandle(psPerProc->psHandleBase,
									&pvMemInfo,
#if defined (SUPPORT_SID_INTERFACE)
									psUMPtr->hKernelMemInfo,
#else
									psKickTADumpBuffer[i].hKernelMemInfo,
#endif
									PVRSRV_HANDLE_TYPE_MEM_INFO);

		if(eError != PVRSRV_OK)
		{
			PVR_DPF((PVR_DBG_ERROR, "PVRSRV_BRIDGE_SGX_PDUMP_BUFFER_ARRAY: "
					 "PVRSRVLookupHandle failed (%d)", eError));
			break;
		}
#if defined (SUPPORT_SID_INTERFACE)
		psKMPtr->hKernelMemInfo = pvMemInfo;
#else
		psKickTADumpBuffer[i].hKernelMemInfo = pvMemInfo;
#endif

#if defined(SUPPORT_SGX_NEW_STATUS_VALS)
		eError = PVRSRVLookupHandle(psPerProc->psHandleBase,
									&pvMemInfo,
#if defined (SUPPORT_SID_INTERFACE)
									psUMPtr->hCtrlKernelMemInfo,
#else
									psKickTADumpBuffer[i].hCtrlKernelMemInfo,
#endif
									PVRSRV_HANDLE_TYPE_MEM_INFO);

		if(eError != PVRSRV_OK)
		{
			PVR_DPF((PVR_DBG_ERROR, "PVRSRV_BRIDGE_SGX_PDUMP_BUFFER_ARRAY: "
					 "PVRSRVLookupHandle failed (%d)", eError));
			break;
		}
#if defined (SUPPORT_SID_INTERFACE)
		psKMPtr->hCtrlKernelMemInfo = pvMemInfo;
		psKMPtr->sCtrlDevVAddr = psUMPtr->sCtrlDevVAddr;
#else
		psKickTADumpBuffer[i].hCtrlKernelMemInfo = pvMemInfo;
#endif
#endif

#if defined (SUPPORT_SID_INTERFACE)
		psKMPtr->ui32SpaceUsed     = psUMPtr->ui32SpaceUsed;
		psKMPtr->ui32Start         = psUMPtr->ui32Start;
		psKMPtr->ui32End           = psUMPtr->ui32End;
		psKMPtr->ui32BufferSize    = psUMPtr->ui32BufferSize;
		psKMPtr->ui32BackEndLength = psUMPtr->ui32BackEndLength;
		psKMPtr->uiAllocIndex      = psUMPtr->uiAllocIndex;
		psKMPtr->pvLinAddr         = psUMPtr->pvLinAddr;
		psKMPtr->pszName           = psUMPtr->pszName;
#endif
	}

	if(eError == PVRSRV_OK)
	{
		DumpBufferArray(psPerProc,
#if defined (SUPPORT_SID_INTERFACE)
						psKickTADumpBufferKM,
#else
						psKickTADumpBuffer,
#endif
						ui32BufferArrayLength,
						psPDumpBufferArrayIN->bDumpPolls);
	}

#if defined (SUPPORT_SID_INTERFACE)
	OSFreeMem(PVRSRV_OS_PAGEABLE_HEAP, ui32BufferArraySize, psKickTADumpBufferKM, 0);
#else
	OSFreeMem(PVRSRV_OS_PAGEABLE_HEAP, ui32BufferArraySize, psKickTADumpBuffer, 0);
#endif
	

	return 0;
}

static IMG_INT
SGXPDump3DSignatureRegistersBW(IMG_UINT32 ui32BridgeID,
				   PVRSRV_BRIDGE_IN_PDUMP_3D_SIGNATURE_REGISTERS *psPDump3DSignatureRegistersIN,
				   PVRSRV_BRIDGE_RETURN *psRetOUT,
				   PVRSRV_PER_PROCESS_DATA *psPerProc)
{
	IMG_UINT32 ui32RegisterArraySize =  psPDump3DSignatureRegistersIN->ui32NumRegisters * sizeof(IMG_UINT32);
	IMG_UINT32 *pui32Registers = IMG_NULL;
	PVRSRV_SGXDEV_INFO	*psDevInfo;
#if defined(SGX_FEATURE_MP)	&& defined(FIX_HW_BRN_27270)
	IMG_UINT32	ui32RegVal = 0;
#endif
	PVRSRV_DEVICE_NODE *psDeviceNode;
	IMG_HANDLE hDevMemContextInt = 0;
	IMG_UINT32 ui32MMUContextID;
	IMG_INT ret = -EFAULT;

	PVR_UNREFERENCED_PARAMETER(psRetOUT);

	PVRSRV_BRIDGE_ASSERT_CMD(ui32BridgeID, PVRSRV_BRIDGE_SGX_PDUMP_3D_SIGNATURE_REGISTERS);

	if (ui32RegisterArraySize == 0)
	{
		goto ExitNoError;
	}

	psRetOUT->eError =
		PVRSRVLookupHandle(psPerProc->psHandleBase,
						   (IMG_VOID**)&psDeviceNode,
						   psPDump3DSignatureRegistersIN->hDevCookie,
						   PVRSRV_HANDLE_TYPE_DEV_NODE);
	if(psRetOUT->eError != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR, "PDumpTASignatureRegistersBW: hDevCookie lookup failed"));
		goto Exit;
	}

	psDevInfo = (PVRSRV_SGXDEV_INFO*)psDeviceNode->pvDevice;

#if defined(SGX_FEATURE_MP)	&& defined(FIX_HW_BRN_27270)
	
	ui32RegVal = OSReadHWReg(psDevInfo->pvRegsBaseKM, EUR_CR_MASTER_CORE);
	OSWriteHWReg(psDevInfo->pvRegsBaseKM, EUR_CR_MASTER_CORE, (SGX_FEATURE_MP_CORE_COUNT - 1) << EUR_CR_MASTER_CORE_ENABLE_SHIFT);
#if defined(PDUMP)
	PDUMPREGWITHFLAGS(SGX_PDUMPREG_NAME, EUR_CR_MASTER_CORE, (SGX_FEATURE_MP_CORE_COUNT - 1) << EUR_CR_MASTER_CORE_ENABLE_SHIFT,
						psPDump3DSignatureRegistersIN->bLastFrame ? PDUMP_FLAGS_LASTFRAME : 0);
#endif
#endif

	if(OSAllocMem(PVRSRV_OS_PAGEABLE_HEAP,
				  ui32RegisterArraySize,
				  (IMG_PVOID *)&pui32Registers, 0,
				  "Array of Registers") != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR, "PDump3DSignatureRegistersBW: OSAllocMem failed"));
		goto Exit;
	}

	if(CopyFromUserWrapper(psPerProc,
			        	ui32BridgeID,
					pui32Registers,
					psPDump3DSignatureRegistersIN->pui32Registers,
					ui32RegisterArraySize) != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR, "PDump3DSignatureRegistersBW: CopyFromUserWrapper failed"));
		goto Exit;
	}

	PDump3DSignatureRegisters(&psDeviceNode->sDevId,
					psPDump3DSignatureRegistersIN->ui32DumpFrameNum,
					psPDump3DSignatureRegistersIN->bLastFrame,
					pui32Registers,
					psPDump3DSignatureRegistersIN->ui32NumRegisters);

	psRetOUT->eError =
		PVRSRVLookupHandle(	psPerProc->psHandleBase,
							&hDevMemContextInt,
							psPDump3DSignatureRegistersIN->hDevMemContext,
							PVRSRV_HANDLE_TYPE_DEV_MEM_CONTEXT);
	if(psRetOUT->eError != PVRSRV_OK)
	{
		return 0;
	}

	
	PVR_ASSERT(psDeviceNode->pfnMMUGetContextID != IMG_NULL)
	ui32MMUContextID = psDeviceNode->pfnMMUGetContextID(hDevMemContextInt);

	PDumpSignatureBuffer(&psDeviceNode->sDevId,
						 "out.tasig", "TA", 0,
						 psDevInfo->psKernelTASigBufferMemInfo->sDevVAddr,
						 (IMG_UINT32)psDevInfo->psKernelTASigBufferMemInfo->uAllocSize,
						 ui32MMUContextID,
						 0 );
	PDumpSignatureBuffer(&psDeviceNode->sDevId,
						 "out.3dsig", "3D", 0,
						 psDevInfo->psKernel3DSigBufferMemInfo->sDevVAddr,
						 (IMG_UINT32)psDevInfo->psKernel3DSigBufferMemInfo->uAllocSize,
						 ui32MMUContextID,
						 0 );

ExitNoError:
	psRetOUT->eError = PVRSRV_OK;
	ret = 0;
Exit:
	if (pui32Registers != IMG_NULL)
	{
		OSFreeMem(PVRSRV_OS_PAGEABLE_HEAP, ui32RegisterArraySize, pui32Registers, 0);
	}

#if defined(SGX_FEATURE_MP)	&& defined(FIX_HW_BRN_27270)
	if (psDevInfo != IMG_NULL)
	{
		OSWriteHWReg(psDevInfo->pvRegsBaseKM, EUR_CR_MASTER_CORE, ui32RegVal);
#if defined(PDUMP)
		PDUMPREGWITHFLAGS(SGX_PDUMPREG_NAME, EUR_CR_MASTER_CORE, ui32RegVal,
							psPDump3DSignatureRegistersIN->bLastFrame ? PDUMP_FLAGS_LASTFRAME : 0);
#endif
	}
#endif

	return ret;
}

static IMG_INT
SGXPDumpCounterRegistersBW(IMG_UINT32 ui32BridgeID,
				   PVRSRV_BRIDGE_IN_PDUMP_COUNTER_REGISTERS *psPDumpCounterRegistersIN,
				   IMG_VOID *psBridgeOut,
				   PVRSRV_PER_PROCESS_DATA *psPerProc)
{
	IMG_UINT32 ui32RegisterArraySize =  psPDumpCounterRegistersIN->ui32NumRegisters * sizeof(IMG_UINT32);
	IMG_UINT32 *pui32Registers = IMG_NULL;
	PVRSRV_DEVICE_NODE *psDeviceNode ;
	IMG_INT ret = -EFAULT;

	PVR_UNREFERENCED_PARAMETER(psBridgeOut);

	PVRSRV_BRIDGE_ASSERT_CMD(ui32BridgeID, PVRSRV_BRIDGE_SGX_PDUMP_COUNTER_REGISTERS);

	if (ui32RegisterArraySize == 0)
	{
		goto ExitNoError;
	}

	if(PVRSRVLookupHandle(psPerProc->psHandleBase,
						  (IMG_VOID**)&psDeviceNode,
						  psPDumpCounterRegistersIN->hDevCookie,
						  PVRSRV_HANDLE_TYPE_DEV_NODE) != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR, "SGXPDumpCounterRegistersBW: hDevCookie lookup failed"));
		ret = -ENOMEM;
		goto Exit;
	}

	if(OSAllocMem(PVRSRV_OS_PAGEABLE_HEAP,
				  ui32RegisterArraySize,
				  (IMG_PVOID *)&pui32Registers, 0,
				  "Array of Registers") != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR, "PDumpCounterRegistersBW: OSAllocMem failed"));
		ret = -ENOMEM;
		goto Exit;
	}

	if(CopyFromUserWrapper(psPerProc,
			        	ui32BridgeID,
					pui32Registers,
					psPDumpCounterRegistersIN->pui32Registers,
					ui32RegisterArraySize) != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR, "PDumpCounterRegistersBW: CopyFromUserWrapper failed"));
		goto Exit;
	}

	PDumpCounterRegisters(&psDeviceNode->sDevId,
					psPDumpCounterRegistersIN->ui32DumpFrameNum,
					psPDumpCounterRegistersIN->bLastFrame,
					pui32Registers,
					psPDumpCounterRegistersIN->ui32NumRegisters);

ExitNoError:
	ret = 0;
Exit:
	if (pui32Registers != IMG_NULL)
	{
		OSFreeMem(PVRSRV_OS_PAGEABLE_HEAP, ui32RegisterArraySize, pui32Registers, 0);
	}

	return ret;
}

static IMG_INT
SGXPDumpTASignatureRegistersBW(IMG_UINT32 ui32BridgeID,
				   PVRSRV_BRIDGE_IN_PDUMP_TA_SIGNATURE_REGISTERS *psPDumpTASignatureRegistersIN,
				   PVRSRV_BRIDGE_RETURN *psRetOUT,
				   PVRSRV_PER_PROCESS_DATA *psPerProc)
{
	IMG_UINT32 ui32RegisterArraySize =  psPDumpTASignatureRegistersIN->ui32NumRegisters * sizeof(IMG_UINT32);
	IMG_UINT32 *pui32Registers = IMG_NULL;
#if defined(SGX_FEATURE_MP)	&& defined(FIX_HW_BRN_27270)
	PVRSRV_SGXDEV_INFO	*psDevInfo = IMG_NULL;
	IMG_UINT32	ui32RegVal = 0;
#endif
	PVRSRV_DEVICE_NODE *psDeviceNode;
	IMG_INT ret = -EFAULT;

	PVR_UNREFERENCED_PARAMETER(psRetOUT);

	PVRSRV_BRIDGE_ASSERT_CMD(ui32BridgeID, PVRSRV_BRIDGE_SGX_PDUMP_TA_SIGNATURE_REGISTERS);

	if (ui32RegisterArraySize == 0)
	{
		goto ExitNoError;
	}

	psRetOUT->eError =
		PVRSRVLookupHandle(psPerProc->psHandleBase, (IMG_VOID**)&psDeviceNode,
						   psPDumpTASignatureRegistersIN->hDevCookie,
						   PVRSRV_HANDLE_TYPE_DEV_NODE);
	if(psRetOUT->eError != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR, "PDumpTASignatureRegistersBW: hDevCookie lookup failed"));
		goto Exit;
	}

#if defined(SGX_FEATURE_MP)	&& defined(FIX_HW_BRN_27270)

	psDevInfo = (PVRSRV_SGXDEV_INFO*)psDeviceNode->pvDevice;

	
	ui32RegVal = OSReadHWReg(psDevInfo->pvRegsBaseKM, EUR_CR_MASTER_CORE);
	OSWriteHWReg(psDevInfo->pvRegsBaseKM, EUR_CR_MASTER_CORE, (SGX_FEATURE_MP_CORE_COUNT - 1) << EUR_CR_MASTER_CORE_ENABLE_SHIFT);
#if defined(PDUMP)
	PDUMPREGWITHFLAGS(SGX_PDUMPREG_NAME, EUR_CR_MASTER_CORE, (SGX_FEATURE_MP_CORE_COUNT - 1) << EUR_CR_MASTER_CORE_ENABLE_SHIFT,
						psPDumpTASignatureRegistersIN->bLastFrame ? PDUMP_FLAGS_LASTFRAME : 0);
#endif
#endif

	if(OSAllocMem(PVRSRV_OS_PAGEABLE_HEAP,
				  ui32RegisterArraySize,
				  (IMG_PVOID *)&pui32Registers, 0,
				  "Array of Registers") != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR, "PDumpTASignatureRegistersBW: OSAllocMem failed"));
		ret = -ENOMEM;
		goto Exit;
	}

	if(CopyFromUserWrapper(psPerProc,
			        	ui32BridgeID,
					pui32Registers,
					psPDumpTASignatureRegistersIN->pui32Registers,
					ui32RegisterArraySize) != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR, "PDumpTASignatureRegistersBW: CopyFromUserWrapper failed"));
		goto Exit;
	}

	PDumpTASignatureRegisters(&psDeviceNode->sDevId,
					psPDumpTASignatureRegistersIN->ui32DumpFrameNum,
					psPDumpTASignatureRegistersIN->ui32TAKickCount,
					psPDumpTASignatureRegistersIN->bLastFrame,
					pui32Registers,
					psPDumpTASignatureRegistersIN->ui32NumRegisters);

ExitNoError:
	psRetOUT->eError = PVRSRV_OK;
	ret = 0;
Exit:
	if (pui32Registers != IMG_NULL)
	{
		OSFreeMem(PVRSRV_OS_PAGEABLE_HEAP, ui32RegisterArraySize, pui32Registers, 0);
	}

#if defined(SGX_FEATURE_MP)	&& defined(FIX_HW_BRN_27270)
	if (psDevInfo != IMG_NULL)
	{
		OSWriteHWReg(psDevInfo->pvRegsBaseKM, EUR_CR_MASTER_CORE, ui32RegVal);
#if defined(PDUMP)
		PDUMPREGWITHFLAGS(SGX_PDUMPREG_NAME, EUR_CR_MASTER_CORE, ui32RegVal,
							psPDumpTASignatureRegistersIN->bLastFrame ? PDUMP_FLAGS_LASTFRAME : 0);
#endif
	}
#endif

	return ret;
}
static IMG_INT
SGXPDumpHWPerfCBBW(IMG_UINT32						ui32BridgeID,
				   PVRSRV_BRIDGE_IN_PDUMP_HWPERFCB	*psPDumpHWPerfCBIN,
				   PVRSRV_BRIDGE_RETURN 			*psRetOUT,
				   PVRSRV_PER_PROCESS_DATA 			*psPerProc)
{
#if defined(SUPPORT_SGX_HWPERF)
#if defined(__linux__)
	PVRSRV_SGXDEV_INFO	*psDevInfo;
	PVRSRV_DEVICE_NODE *psDeviceNode;
	IMG_HANDLE hDevMemContextInt = 0;
	IMG_UINT32 ui32MMUContextID = 0;

	PVRSRV_BRIDGE_ASSERT_CMD(ui32BridgeID, PVRSRV_BRIDGE_SGX_PDUMP_HWPERFCB);

	psRetOUT->eError =
		PVRSRVLookupHandle(psPerProc->psHandleBase, (IMG_VOID**)&psDeviceNode,
						   psPDumpHWPerfCBIN->hDevCookie,
						   PVRSRV_HANDLE_TYPE_DEV_NODE);
	if(psRetOUT->eError != PVRSRV_OK)
	{
		return 0;
	}

	psDevInfo = psDeviceNode->pvDevice;

	psRetOUT->eError =
		PVRSRVLookupHandle(	psPerProc->psHandleBase,
							&hDevMemContextInt,
							psPDumpHWPerfCBIN->hDevMemContext,
							PVRSRV_HANDLE_TYPE_DEV_MEM_CONTEXT);
	if(psRetOUT->eError != PVRSRV_OK)
	{
		return 0;
	}

	
	PVR_ASSERT(psDeviceNode->pfnMMUGetContextID != IMG_NULL)
	ui32MMUContextID = psDeviceNode->pfnMMUGetContextID(hDevMemContextInt);

	PDumpHWPerfCBKM(&psDeviceNode->sDevId,
					&psPDumpHWPerfCBIN->szFileName[0],
					psPDumpHWPerfCBIN->ui32FileOffset,
					psDevInfo->psKernelHWPerfCBMemInfo->sDevVAddr,
					psDevInfo->psKernelHWPerfCBMemInfo->uAllocSize,
					ui32MMUContextID,
					psPDumpHWPerfCBIN->ui32PDumpFlags);

	return 0;
#else
	PVR_UNREFERENCED_PARAMETER(ui32BridgeID);
	PVR_UNREFERENCED_PARAMETER(psPDumpHWPerfCBIN);
	PVR_UNREFERENCED_PARAMETER(psRetOUT);
	PVR_UNREFERENCED_PARAMETER(psPerProc);
	return 0;
#endif 
#else
	PVR_UNREFERENCED_PARAMETER(ui32BridgeID);
	PVR_UNREFERENCED_PARAMETER(psPDumpHWPerfCBIN);
	PVR_UNREFERENCED_PARAMETER(psRetOUT);
	PVR_UNREFERENCED_PARAMETER(psPerProc);
	return -EFAULT;
#endif 
}


static IMG_INT
SGXPDumpSaveMemBW(IMG_UINT32						ui32BridgeID,
				  PVRSRV_BRIDGE_IN_PDUMP_SAVEMEM	*psPDumpSaveMem,
				  PVRSRV_BRIDGE_RETURN 				*psRetOUT,
				  PVRSRV_PER_PROCESS_DATA 			*psPerProc)
{
	PVRSRV_DEVICE_NODE *psDeviceNode;
	IMG_HANDLE hDevMemContextInt = 0;
	IMG_UINT32 ui32MMUContextID;

	PVRSRV_BRIDGE_ASSERT_CMD(ui32BridgeID, PVRSRV_BRIDGE_SGX_PDUMP_SAVEMEM);

	psRetOUT->eError =
		PVRSRVLookupHandle(psPerProc->psHandleBase,
						   (IMG_VOID**)&psDeviceNode,
						   psPDumpSaveMem->hDevCookie,
						   PVRSRV_HANDLE_TYPE_DEV_NODE);
	if(psRetOUT->eError != PVRSRV_OK)
	{
		return 0;
	}

	psRetOUT->eError =
		PVRSRVLookupHandle(	psPerProc->psHandleBase,
							&hDevMemContextInt,
							psPDumpSaveMem->hDevMemContext,
							PVRSRV_HANDLE_TYPE_DEV_MEM_CONTEXT);
	if(psRetOUT->eError != PVRSRV_OK)
	{
		return 0;
	}

	
	PVR_ASSERT(psDeviceNode->pfnMMUGetContextID != IMG_NULL)
	ui32MMUContextID = psDeviceNode->pfnMMUGetContextID(hDevMemContextInt);

	PDumpSaveMemKM(&psDeviceNode->sDevId,
				   &psPDumpSaveMem->szFileName[0],
				   psPDumpSaveMem->ui32FileOffset,
				   psPDumpSaveMem->sDevVAddr,
				   psPDumpSaveMem->ui32Size,
				   ui32MMUContextID,
				   psPDumpSaveMem->ui32PDumpFlags);
	return 0;
}

#endif 


 
IMG_VOID SetSGXDispatchTableEntry(IMG_VOID)
{

	SetDispatchTableEntry(PVRSRV_BRIDGE_SGX_GETCLIENTINFO, SGXGetClientInfoBW);
	SetDispatchTableEntry(PVRSRV_BRIDGE_SGX_RELEASECLIENTINFO, SGXReleaseClientInfoBW);
	SetDispatchTableEntry(PVRSRV_BRIDGE_SGX_GETINTERNALDEVINFO, SGXGetInternalDevInfoBW);
	SetDispatchTableEntry(PVRSRV_BRIDGE_SGX_DOKICK, SGXDoKickBW);
	SetDispatchTableEntry(PVRSRV_BRIDGE_SGX_GETPHYSPAGEADDR, DummyBW);
	SetDispatchTableEntry(PVRSRV_BRIDGE_SGX_READREGISTRYDWORD, DummyBW);

	SetDispatchTableEntry(PVRSRV_BRIDGE_SGX_2DQUERYBLTSCOMPLETE, SGX2DQueryBlitsCompleteBW);

	SetDispatchTableEntry(PVRSRV_BRIDGE_SGX_GETMMUPDADDR, DummyBW);

#if defined(TRANSFER_QUEUE)
	SetDispatchTableEntry(PVRSRV_BRIDGE_SGX_SUBMITTRANSFER, SGXSubmitTransferBW);
#endif
	SetDispatchTableEntry(PVRSRV_BRIDGE_SGX_GETMISCINFO, SGXGetMiscInfoBW);
	SetDispatchTableEntry(PVRSRV_BRIDGE_SGXINFO_FOR_SRVINIT	, SGXGetInfoForSrvinitBW);
	SetDispatchTableEntry(PVRSRV_BRIDGE_SGX_DEVINITPART2, SGXDevInitPart2BW);

	SetDispatchTableEntry(PVRSRV_BRIDGE_SGX_FINDSHAREDPBDESC, SGXFindSharedPBDescBW);
	SetDispatchTableEntry(PVRSRV_BRIDGE_SGX_UNREFSHAREDPBDESC, SGXUnrefSharedPBDescBW);
	SetDispatchTableEntry(PVRSRV_BRIDGE_SGX_ADDSHAREDPBDESC, SGXAddSharedPBDescBW);
	SetDispatchTableEntry(PVRSRV_BRIDGE_SGX_REGISTER_HW_RENDER_CONTEXT, SGXRegisterHWRenderContextBW);
	SetDispatchTableEntry(PVRSRV_BRIDGE_SGX_FLUSH_HW_RENDER_TARGET, SGXFlushHWRenderTargetBW);
	SetDispatchTableEntry(PVRSRV_BRIDGE_SGX_UNREGISTER_HW_RENDER_CONTEXT, SGXUnregisterHWRenderContextBW);
#if defined(SGX_FEATURE_2D_HARDWARE)
	SetDispatchTableEntry(PVRSRV_BRIDGE_SGX_SUBMIT2D, SGXSubmit2DBW);
	SetDispatchTableEntry(PVRSRV_BRIDGE_SGX_REGISTER_HW_2D_CONTEXT, SGXRegisterHW2DContextBW);
	SetDispatchTableEntry(PVRSRV_BRIDGE_SGX_UNREGISTER_HW_2D_CONTEXT, SGXUnregisterHW2DContextBW);
#endif
	SetDispatchTableEntry(PVRSRV_BRIDGE_SGX_REGISTER_HW_TRANSFER_CONTEXT, SGXRegisterHWTransferContextBW);
	SetDispatchTableEntry(PVRSRV_BRIDGE_SGX_UNREGISTER_HW_TRANSFER_CONTEXT, SGXUnregisterHWTransferContextBW);

	SetDispatchTableEntry(PVRSRV_BRIDGE_SGX_SCHEDULE_PROCESS_QUEUES, SGXScheduleProcessQueuesBW);

	SetDispatchTableEntry(PVRSRV_BRIDGE_SGX_READ_HWPERF_CB, SGXReadHWPerfCBBW);

#if defined(PDUMP)
	SetDispatchTableEntry(PVRSRV_BRIDGE_SGX_PDUMP_BUFFER_ARRAY, SGXPDumpBufferArrayBW);
	SetDispatchTableEntry(PVRSRV_BRIDGE_SGX_PDUMP_3D_SIGNATURE_REGISTERS, SGXPDump3DSignatureRegistersBW);
	SetDispatchTableEntry(PVRSRV_BRIDGE_SGX_PDUMP_COUNTER_REGISTERS, SGXPDumpCounterRegistersBW);
	SetDispatchTableEntry(PVRSRV_BRIDGE_SGX_PDUMP_TA_SIGNATURE_REGISTERS, SGXPDumpTASignatureRegistersBW);
	SetDispatchTableEntry(PVRSRV_BRIDGE_SGX_PDUMP_HWPERFCB, SGXPDumpHWPerfCBBW);
	SetDispatchTableEntry(PVRSRV_BRIDGE_SGX_PDUMP_SAVEMEM, SGXPDumpSaveMemBW);
#endif
}
 

#endif 
