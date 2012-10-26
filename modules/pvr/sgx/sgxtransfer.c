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

#if defined(TRANSFER_QUEUE)

#include <stddef.h>

#include "sgxdefs.h"
#include "services_headers.h"
#include "buffer_manager.h"
#include "sgxinfo.h"
#include "../omap3/sysconfig.h"
#include "regpaths.h"
#include "pdump_km.h"
#include "mmu.h"
#include "pvr_bridge.h"
#include "sgx_bridge_km.h"
#include "sgxinfokm.h"
#include "osfunc.h"
#include "pvr_debug.h"
#include "sgxutils.h"
#include "ttrace.h"

#if defined (SUPPORT_SID_INTERFACE)
IMG_EXPORT PVRSRV_ERROR SGXSubmitTransferKM(IMG_HANDLE hDevHandle, PVRSRV_TRANSFER_SGX_KICK_KM *psKick)
#else
IMG_EXPORT PVRSRV_ERROR SGXSubmitTransferKM(IMG_HANDLE hDevHandle, PVRSRV_TRANSFER_SGX_KICK *psKick)
#endif
{
	PVRSRV_KERNEL_MEM_INFO		*psCCBMemInfo = (PVRSRV_KERNEL_MEM_INFO *)psKick->hCCBMemInfo;
	SGXMKIF_COMMAND				sCommand = {0};
	SGXMKIF_TRANSFERCMD_SHARED *psSharedTransferCmd;
	PVRSRV_KERNEL_SYNC_INFO 	*psSyncInfo;
	PVRSRV_ERROR				eError;
	IMG_UINT32					loop;
	IMG_HANDLE					hDevMemContext = IMG_NULL;
	IMG_BOOL					abSrcSyncEnable[SGX_MAX_TRANSFER_SYNC_OPS];
	IMG_UINT32					ui32RealSrcSyncNum = 0;
	IMG_BOOL					abDstSyncEnable[SGX_MAX_TRANSFER_SYNC_OPS];
	IMG_UINT32					ui32RealDstSyncNum = 0;


#if defined(PDUMP)
	IMG_BOOL bPersistentProcess = IMG_FALSE;
	
	{
		PVRSRV_PER_PROCESS_DATA* psPerProc = PVRSRVFindPerProcessData();
		if(psPerProc != IMG_NULL)
		{
			bPersistentProcess = psPerProc->bPDumpPersistent;
		}
	}
#endif 
#if defined(FIX_HW_BRN_31620)
	hDevMemContext = psKick->hDevMemContext;
#endif
	PVR_TTRACE(PVRSRV_TRACE_GROUP_TRANSFER, PVRSRV_TRACE_CLASS_FUNCTION_ENTER, TRANSFER_TOKEN_SUBMIT);

	for (loop = 0; loop < SGX_MAX_TRANSFER_SYNC_OPS; loop++)
	{
		abSrcSyncEnable[loop] = IMG_TRUE;
		abDstSyncEnable[loop] = IMG_TRUE;
	}

	if (!CCB_OFFSET_IS_VALID(SGXMKIF_TRANSFERCMD_SHARED, psCCBMemInfo, psKick, ui32SharedCmdCCBOffset))
	{
		PVR_DPF((PVR_DBG_ERROR, "SGXSubmitTransferKM: Invalid CCB offset"));
		PVR_TTRACE(PVRSRV_TRACE_GROUP_TRANSFER, PVRSRV_TRACE_CLASS_FUNCTION_EXIT,
				TRANSFER_TOKEN_SUBMIT);
		return PVRSRV_ERROR_INVALID_PARAMS;
	}
	
	
	psSharedTransferCmd =  CCB_DATA_FROM_OFFSET(SGXMKIF_TRANSFERCMD_SHARED, psCCBMemInfo, psKick, ui32SharedCmdCCBOffset);

	PVR_TTRACE(PVRSRV_TRACE_GROUP_TRANSFER, PVRSRV_TRACE_CLASS_CMD_START, TRANSFER_TOKEN_SUBMIT);
	PVR_TTRACE_UI32(PVRSRV_TRACE_GROUP_TRANSFER, PVRSRV_TRACE_CLASS_CCB,
			TRANSFER_TOKEN_CCB_OFFSET, psKick->ui32SharedCmdCCBOffset);

	if (psKick->hTASyncInfo != IMG_NULL)
	{
		psSyncInfo = (PVRSRV_KERNEL_SYNC_INFO *)psKick->hTASyncInfo;

		PVR_TTRACE_SYNC_OBJECT(PVRSRV_TRACE_GROUP_TRANSFER, TRANSFER_TOKEN_TA_SYNC,
					  psSyncInfo, PVRSRV_SYNCOP_SAMPLE);

		psSharedTransferCmd->ui32TASyncWriteOpsPendingVal = psSyncInfo->psSyncData->ui32WriteOpsPending++;
		psSharedTransferCmd->ui32TASyncReadOpsPendingVal  = psSyncInfo->psSyncData->ui32ReadOpsPending;

		psSharedTransferCmd->sTASyncWriteOpsCompleteDevVAddr = psSyncInfo->sWriteOpsCompleteDevVAddr;
		psSharedTransferCmd->sTASyncReadOpsCompleteDevVAddr = psSyncInfo->sReadOpsCompleteDevVAddr;
	}
	else
	{
		psSharedTransferCmd->sTASyncWriteOpsCompleteDevVAddr.uiAddr = 0;
		psSharedTransferCmd->sTASyncReadOpsCompleteDevVAddr.uiAddr = 0;
	}

	if (psKick->h3DSyncInfo != IMG_NULL)
	{
		psSyncInfo = (PVRSRV_KERNEL_SYNC_INFO *)psKick->h3DSyncInfo;

		PVR_TTRACE_SYNC_OBJECT(PVRSRV_TRACE_GROUP_TRANSFER, TRANSFER_TOKEN_3D_SYNC,
					  psSyncInfo, PVRSRV_SYNCOP_SAMPLE);

		psSharedTransferCmd->ui323DSyncWriteOpsPendingVal = psSyncInfo->psSyncData->ui32WriteOpsPending++;
		psSharedTransferCmd->ui323DSyncReadOpsPendingVal = psSyncInfo->psSyncData->ui32ReadOpsPending;

		psSharedTransferCmd->s3DSyncWriteOpsCompleteDevVAddr = psSyncInfo->sWriteOpsCompleteDevVAddr;
		psSharedTransferCmd->s3DSyncReadOpsCompleteDevVAddr = psSyncInfo->sReadOpsCompleteDevVAddr;
	}
	else
	{
		psSharedTransferCmd->s3DSyncWriteOpsCompleteDevVAddr.uiAddr = 0;
		psSharedTransferCmd->s3DSyncReadOpsCompleteDevVAddr.uiAddr = 0;
	}

	
	for (loop = 0; loop < MIN(SGX_MAX_TRANSFER_SYNC_OPS, psKick->ui32NumSrcSync); loop++)
	{
		IMG_UINT32 i;

		PVRSRV_KERNEL_SYNC_INFO * psMySyncInfo = (PVRSRV_KERNEL_SYNC_INFO *)psKick->ahSrcSyncInfo[loop];
	
		for (i = 0; i < loop; i++)	
		{
			if (abSrcSyncEnable[i])
			{
				psSyncInfo = (PVRSRV_KERNEL_SYNC_INFO *)psKick->ahSrcSyncInfo[i];

				if (psSyncInfo->sWriteOpsCompleteDevVAddr.uiAddr == psMySyncInfo->sWriteOpsCompleteDevVAddr.uiAddr)
				{
					PVR_DPF((PVR_DBG_WARNING, "SGXSubmitTransferKM : Same src synchronized multiple times!"));
					abSrcSyncEnable[loop] = IMG_FALSE;
					break;
				}
			}
		}
		if (abSrcSyncEnable[loop])
		{
			ui32RealSrcSyncNum++;
		}
	}
	for (loop = 0; loop < MIN(SGX_MAX_TRANSFER_SYNC_OPS, psKick->ui32NumDstSync); loop++)
	{
		IMG_UINT32 i;

		PVRSRV_KERNEL_SYNC_INFO * psMySyncInfo = (PVRSRV_KERNEL_SYNC_INFO *)psKick->ahDstSyncInfo[loop];
	
		for (i = 0; i < loop; i++)	
		{
			if (abDstSyncEnable[i])
			{
				psSyncInfo = (PVRSRV_KERNEL_SYNC_INFO *)psKick->ahDstSyncInfo[i];

				if (psSyncInfo->sWriteOpsCompleteDevVAddr.uiAddr == psMySyncInfo->sWriteOpsCompleteDevVAddr.uiAddr)
				{
					PVR_DPF((PVR_DBG_WARNING, "SGXSubmitTransferKM : Same dst synchronized multiple times!"));
					abDstSyncEnable[loop] = IMG_FALSE;
					break;
				}
			}
		}
		if (abDstSyncEnable[loop])
		{
			ui32RealDstSyncNum++;
		}
	}

	psSharedTransferCmd->ui32NumSrcSyncs = ui32RealSrcSyncNum; 
	psSharedTransferCmd->ui32NumDstSyncs = ui32RealDstSyncNum; 

	if ((psKick->ui32Flags & SGXMKIF_TQFLAGS_KEEPPENDING) == 0UL)
	{
		IMG_UINT32 i = 0;

		for (loop = 0; loop < psKick->ui32NumSrcSync; loop++)
		{
			if (abSrcSyncEnable[loop])
			{
				psSyncInfo = (PVRSRV_KERNEL_SYNC_INFO *)psKick->ahSrcSyncInfo[loop];

				PVR_TTRACE_SYNC_OBJECT(PVRSRV_TRACE_GROUP_TRANSFER, TRANSFER_TOKEN_SRC_SYNC,
						psSyncInfo, PVRSRV_SYNCOP_SAMPLE);

				psSharedTransferCmd->asSrcSyncs[i].ui32WriteOpsPendingVal = psSyncInfo->psSyncData->ui32WriteOpsPending;
				psSharedTransferCmd->asSrcSyncs[i].ui32ReadOpsPendingVal = psSyncInfo->psSyncData->ui32ReadOpsPending;

				psSharedTransferCmd->asSrcSyncs[i].sWriteOpsCompleteDevVAddr = psSyncInfo->sWriteOpsCompleteDevVAddr; 
				psSharedTransferCmd->asSrcSyncs[i].sReadOpsCompleteDevVAddr = psSyncInfo->sReadOpsCompleteDevVAddr;
				i++;
			}
		}
		PVR_ASSERT(i == ui32RealSrcSyncNum);

		i = 0;
		for (loop = 0; loop < psKick->ui32NumDstSync; loop++)
		{
			if (abDstSyncEnable[loop])
			{
				psSyncInfo = (PVRSRV_KERNEL_SYNC_INFO *)psKick->ahDstSyncInfo[loop];

				PVR_TTRACE_SYNC_OBJECT(PVRSRV_TRACE_GROUP_TRANSFER, TRANSFER_TOKEN_DST_SYNC,
						psSyncInfo, PVRSRV_SYNCOP_SAMPLE);

				psSharedTransferCmd->asDstSyncs[i].ui32WriteOpsPendingVal = psSyncInfo->psSyncData->ui32WriteOpsPending;
				psSharedTransferCmd->asDstSyncs[i].ui32ReadOpsPendingVal = psSyncInfo->psSyncData->ui32ReadOpsPending;

				psSharedTransferCmd->asDstSyncs[i].sWriteOpsCompleteDevVAddr = psSyncInfo->sWriteOpsCompleteDevVAddr;
				psSharedTransferCmd->asDstSyncs[i].sReadOpsCompleteDevVAddr = psSyncInfo->sReadOpsCompleteDevVAddr;
				i++;
			}
		}
		PVR_ASSERT(i == ui32RealDstSyncNum);

		
		for (loop = 0; loop < psKick->ui32NumSrcSync; loop++)
		{
			if (abSrcSyncEnable[loop])
			{
				psSyncInfo = (PVRSRV_KERNEL_SYNC_INFO *)psKick->ahSrcSyncInfo[loop];
				psSyncInfo->psSyncData->ui32ReadOpsPending++;
			}
		}
		for (loop = 0; loop < psKick->ui32NumDstSync; loop++)
		{
			if (abDstSyncEnable[loop])
			{
				psSyncInfo = (PVRSRV_KERNEL_SYNC_INFO *)psKick->ahDstSyncInfo[loop];
				psSyncInfo->psSyncData->ui32WriteOpsPending++;
			}
		}
	}

#if defined(PDUMP)
	if ((PDumpIsCaptureFrameKM()
	|| ((psKick->ui32PDumpFlags & PDUMP_FLAGS_CONTINUOUS) != 0)) 
	&&  (bPersistentProcess == IMG_FALSE) )
	{
		PDUMPCOMMENT("Shared part of transfer command\r\n");
		PDUMPMEM(psSharedTransferCmd,
				psCCBMemInfo,
				psKick->ui32CCBDumpWOff,
				sizeof(SGXMKIF_TRANSFERCMD_SHARED),
				psKick->ui32PDumpFlags,
				MAKEUNIQUETAG(psCCBMemInfo));

		if ((psKick->ui32Flags & SGXMKIF_TQFLAGS_KEEPPENDING) == 0UL)
		{
			IMG_UINT32 i = 0;

			for (loop = 0; loop < psKick->ui32NumSrcSync; loop++)
			{
				if (abSrcSyncEnable[loop])
				{
					psSyncInfo = psKick->ahSrcSyncInfo[loop];

					PDUMPCOMMENT("Hack src surface write op in transfer cmd\r\n");
					PDUMPMEM(&psSyncInfo->psSyncData->ui32LastOpDumpVal,
							psCCBMemInfo,
							psKick->ui32CCBDumpWOff + (IMG_UINT32)(offsetof(SGXMKIF_TRANSFERCMD_SHARED, asSrcSyncs) + i * sizeof(PVRSRV_DEVICE_SYNC_OBJECT) + offsetof(PVRSRV_DEVICE_SYNC_OBJECT, ui32WriteOpsPendingVal)),
							sizeof(psSyncInfo->psSyncData->ui32LastOpDumpVal),
							psKick->ui32PDumpFlags,
							MAKEUNIQUETAG(psCCBMemInfo));

					PDUMPCOMMENT("Hack src surface read op in transfer cmd\r\n");
					PDUMPMEM(&psSyncInfo->psSyncData->ui32LastReadOpDumpVal,
							psCCBMemInfo,
							psKick->ui32CCBDumpWOff + (IMG_UINT32)(offsetof(SGXMKIF_TRANSFERCMD_SHARED, asSrcSyncs) + i * sizeof(PVRSRV_DEVICE_SYNC_OBJECT) + offsetof(PVRSRV_DEVICE_SYNC_OBJECT, ui32ReadOpsPendingVal)),
							sizeof(psSyncInfo->psSyncData->ui32LastReadOpDumpVal),
							psKick->ui32PDumpFlags,
							MAKEUNIQUETAG(psCCBMemInfo));
					i++;
				}
			}

			i = 0;
			for (loop = 0; loop < psKick->ui32NumDstSync; loop++)
			{
				if (abDstSyncEnable[i])
				{
					psSyncInfo = psKick->ahDstSyncInfo[loop];

					PDUMPCOMMENT("Hack dest surface write op in transfer cmd\r\n");
					PDUMPMEM(&psSyncInfo->psSyncData->ui32LastOpDumpVal,
							psCCBMemInfo,
							psKick->ui32CCBDumpWOff + (IMG_UINT32)(offsetof(SGXMKIF_TRANSFERCMD_SHARED, asDstSyncs) + i * sizeof(PVRSRV_DEVICE_SYNC_OBJECT) + offsetof(PVRSRV_DEVICE_SYNC_OBJECT, ui32WriteOpsPendingVal)),
							sizeof(psSyncInfo->psSyncData->ui32LastOpDumpVal),
							psKick->ui32PDumpFlags,
							MAKEUNIQUETAG(psCCBMemInfo));

					PDUMPCOMMENT("Hack dest surface read op in transfer cmd\r\n");
					PDUMPMEM(&psSyncInfo->psSyncData->ui32LastReadOpDumpVal,
							psCCBMemInfo,
							psKick->ui32CCBDumpWOff + (IMG_UINT32)(offsetof(SGXMKIF_TRANSFERCMD_SHARED, asDstSyncs) + i * sizeof(PVRSRV_DEVICE_SYNC_OBJECT) + offsetof(PVRSRV_DEVICE_SYNC_OBJECT, ui32ReadOpsPendingVal)),
							sizeof(psSyncInfo->psSyncData->ui32LastReadOpDumpVal),
							psKick->ui32PDumpFlags,
							MAKEUNIQUETAG(psCCBMemInfo));
					i++;
				}
			}

			
			for (loop = 0; loop < (psKick->ui32NumSrcSync); loop++)
			{
				if (abSrcSyncEnable[loop])
				{	
					psSyncInfo = (PVRSRV_KERNEL_SYNC_INFO *)psKick->ahSrcSyncInfo[loop];
					psSyncInfo->psSyncData->ui32LastReadOpDumpVal++;
				}
			}

			for (loop = 0; loop < (psKick->ui32NumDstSync); loop++)
			{
				if (abDstSyncEnable[loop])
				{
					psSyncInfo = (PVRSRV_KERNEL_SYNC_INFO *)psKick->ahDstSyncInfo[0];
					psSyncInfo->psSyncData->ui32LastOpDumpVal++;
				}
			}
		}
	}		
#endif

	sCommand.ui32Data[1] = psKick->sHWTransferContextDevVAddr.uiAddr;
	
	PVR_TTRACE(PVRSRV_TRACE_GROUP_TRANSFER, PVRSRV_TRACE_CLASS_CMD_END,
			TRANSFER_TOKEN_SUBMIT);

	eError = SGXScheduleCCBCommandKM(hDevHandle, SGXMKIF_CMD_TRANSFER, &sCommand, KERNEL_ID, psKick->ui32PDumpFlags, hDevMemContext, IMG_FALSE);

	if (eError == PVRSRV_ERROR_RETRY)
	{
		
		if ((psKick->ui32Flags & SGXMKIF_TQFLAGS_KEEPPENDING) == 0UL)
		{
			for (loop = 0; loop < psKick->ui32NumSrcSync; loop++)
			{
				if (abSrcSyncEnable[loop])
				{
					psSyncInfo = (PVRSRV_KERNEL_SYNC_INFO *)psKick->ahSrcSyncInfo[loop];
					psSyncInfo->psSyncData->ui32ReadOpsPending--;
#if defined(PDUMP)
					if (PDumpIsCaptureFrameKM()
							|| ((psKick->ui32PDumpFlags & PDUMP_FLAGS_CONTINUOUS) != 0))
					{
						psSyncInfo->psSyncData->ui32LastReadOpDumpVal--;
					}
#endif
				}
			}
			for (loop = 0; loop < psKick->ui32NumDstSync; loop++)
			{
				if (abDstSyncEnable[loop])
				{
					psSyncInfo = (PVRSRV_KERNEL_SYNC_INFO *)psKick->ahDstSyncInfo[loop];
					psSyncInfo->psSyncData->ui32WriteOpsPending--;
#if defined(PDUMP)
					if (PDumpIsCaptureFrameKM()
							|| ((psKick->ui32PDumpFlags & PDUMP_FLAGS_CONTINUOUS) != 0))
					{
						psSyncInfo->psSyncData->ui32LastOpDumpVal--;
					}
#endif
				}
			}
		}

		
		if (psKick->hTASyncInfo != IMG_NULL)
		{
			psSyncInfo = (PVRSRV_KERNEL_SYNC_INFO *)psKick->hTASyncInfo;
			psSyncInfo->psSyncData->ui32WriteOpsPending--;
		}

		
		if (psKick->h3DSyncInfo != IMG_NULL)
		{
			psSyncInfo = (PVRSRV_KERNEL_SYNC_INFO *)psKick->h3DSyncInfo;
			psSyncInfo->psSyncData->ui32WriteOpsPending--;
		}
	}

	else if (PVRSRV_OK != eError)
	{
		PVR_DPF((PVR_DBG_ERROR, "SGXSubmitTransferKM: SGXScheduleCCBCommandKM failed."));
		PVR_TTRACE(PVRSRV_TRACE_GROUP_TRANSFER, PVRSRV_TRACE_CLASS_FUNCTION_EXIT,
				TRANSFER_TOKEN_SUBMIT);
		return eError;
	}
	

#if defined(NO_HARDWARE)
	if ((psKick->ui32Flags & SGXMKIF_TQFLAGS_NOSYNCUPDATE) == 0)
	{
		
		for (loop = 0; loop < psKick->ui32NumSrcSync; loop++)
		{
			if (abSrcSyncEnable[loop])
			{
				psSyncInfo = (PVRSRV_KERNEL_SYNC_INFO *)psKick->ahSrcSyncInfo[loop];
				psSyncInfo->psSyncData->ui32ReadOpsComplete = psSyncInfo->psSyncData->ui32ReadOpsPending;
			}
		}

		for (loop = 0; loop < psKick->ui32NumDstSync; loop++)
		{
			if (abDstSyncEnable[loop])
			{
				psSyncInfo = (PVRSRV_KERNEL_SYNC_INFO *)psKick->ahDstSyncInfo[loop];
				psSyncInfo->psSyncData->ui32WriteOpsComplete = psSyncInfo->psSyncData->ui32WriteOpsPending;
			}
		}

		if (psKick->hTASyncInfo != IMG_NULL)
		{
			psSyncInfo = (PVRSRV_KERNEL_SYNC_INFO *)psKick->hTASyncInfo;

			psSyncInfo->psSyncData->ui32WriteOpsComplete = psSyncInfo->psSyncData->ui32WriteOpsPending;
		}

		if (psKick->h3DSyncInfo != IMG_NULL)
		{
			psSyncInfo = (PVRSRV_KERNEL_SYNC_INFO *)psKick->h3DSyncInfo;

			psSyncInfo->psSyncData->ui32WriteOpsComplete = psSyncInfo->psSyncData->ui32WriteOpsPending;
		}
	}
#endif
	PVR_TTRACE(PVRSRV_TRACE_GROUP_TRANSFER, PVRSRV_TRACE_CLASS_FUNCTION_EXIT,
			TRANSFER_TOKEN_SUBMIT);
	return eError;
}

#if defined(SGX_FEATURE_2D_HARDWARE)
#if defined (SUPPORT_SID_INTERFACE)
IMG_EXPORT PVRSRV_ERROR SGXSubmit2DKM(IMG_HANDLE hDevHandle, PVRSRV_2D_SGX_KICK_KM *psKick)
#else
IMG_EXPORT PVRSRV_ERROR SGXSubmit2DKM(IMG_HANDLE hDevHandle, PVRSRV_2D_SGX_KICK *psKick)
#endif
					    
{
	PVRSRV_KERNEL_MEM_INFO  *psCCBMemInfo = (PVRSRV_KERNEL_MEM_INFO *)psKick->hCCBMemInfo;
	SGXMKIF_COMMAND sCommand = {0};
	SGXMKIF_2DCMD_SHARED *ps2DCmd;
	PVRSRV_KERNEL_SYNC_INFO *psSyncInfo;
	PVRSRV_ERROR eError;
	IMG_UINT32 i;
	IMG_HANDLE hDevMemContext = IMG_NULL;
#if defined(PDUMP)
	IMG_BOOL bPersistentProcess = IMG_FALSE;
	
	{
		PVRSRV_PER_PROCESS_DATA* psPerProc = PVRSRVFindPerProcessData();
		if(psPerProc != IMG_NULL)
		{
			bPersistentProcess = psPerProc->bPDumpPersistent;
		}
	}
#endif 
#if defined(FIX_HW_BRN_31620)
	hDevMemContext = psKick->hDevMemContext;
#endif

	if (!CCB_OFFSET_IS_VALID(SGXMKIF_2DCMD_SHARED, psCCBMemInfo, psKick, ui32SharedCmdCCBOffset))
	{
		PVR_DPF((PVR_DBG_ERROR, "SGXSubmit2DKM: Invalid CCB offset"));
		return PVRSRV_ERROR_INVALID_PARAMS;
	}
	
	
	ps2DCmd =  CCB_DATA_FROM_OFFSET(SGXMKIF_2DCMD_SHARED, psCCBMemInfo, psKick, ui32SharedCmdCCBOffset);

	OSMemSet(ps2DCmd, 0, sizeof(*ps2DCmd));

	
	if (psKick->hTASyncInfo != IMG_NULL)
	{
		psSyncInfo = (PVRSRV_KERNEL_SYNC_INFO *)psKick->hTASyncInfo;

		ps2DCmd->sTASyncData.ui32WriteOpsPendingVal = psSyncInfo->psSyncData->ui32WriteOpsPending++;
		ps2DCmd->sTASyncData.ui32ReadOpsPendingVal = psSyncInfo->psSyncData->ui32ReadOpsPending;

		ps2DCmd->sTASyncData.sWriteOpsCompleteDevVAddr 	= psSyncInfo->sWriteOpsCompleteDevVAddr;
		ps2DCmd->sTASyncData.sReadOpsCompleteDevVAddr 	= psSyncInfo->sReadOpsCompleteDevVAddr;
	}

	
	if (psKick->h3DSyncInfo != IMG_NULL)
	{
		psSyncInfo = (PVRSRV_KERNEL_SYNC_INFO *)psKick->h3DSyncInfo;

		ps2DCmd->s3DSyncData.ui32WriteOpsPendingVal = psSyncInfo->psSyncData->ui32WriteOpsPending++;
		ps2DCmd->s3DSyncData.ui32ReadOpsPendingVal = psSyncInfo->psSyncData->ui32ReadOpsPending;

		ps2DCmd->s3DSyncData.sWriteOpsCompleteDevVAddr = psSyncInfo->sWriteOpsCompleteDevVAddr;
		ps2DCmd->s3DSyncData.sReadOpsCompleteDevVAddr = psSyncInfo->sReadOpsCompleteDevVAddr;
	}

	
	ps2DCmd->ui32NumSrcSync = psKick->ui32NumSrcSync;
	for (i = 0; i < psKick->ui32NumSrcSync; i++)
	{
		psSyncInfo = psKick->ahSrcSyncInfo[i];

		ps2DCmd->sSrcSyncData[i].ui32WriteOpsPendingVal = psSyncInfo->psSyncData->ui32WriteOpsPending;
		ps2DCmd->sSrcSyncData[i].ui32ReadOpsPendingVal = psSyncInfo->psSyncData->ui32ReadOpsPending;

		ps2DCmd->sSrcSyncData[i].sWriteOpsCompleteDevVAddr = psSyncInfo->sWriteOpsCompleteDevVAddr;
		ps2DCmd->sSrcSyncData[i].sReadOpsCompleteDevVAddr = psSyncInfo->sReadOpsCompleteDevVAddr;
	}

	if (psKick->hDstSyncInfo != IMG_NULL)
	{
		psSyncInfo = psKick->hDstSyncInfo;

		ps2DCmd->sDstSyncData.ui32WriteOpsPendingVal = psSyncInfo->psSyncData->ui32WriteOpsPending;
		ps2DCmd->sDstSyncData.ui32ReadOpsPendingVal = psSyncInfo->psSyncData->ui32ReadOpsPending;

		ps2DCmd->sDstSyncData.sWriteOpsCompleteDevVAddr = psSyncInfo->sWriteOpsCompleteDevVAddr;
		ps2DCmd->sDstSyncData.sReadOpsCompleteDevVAddr = psSyncInfo->sReadOpsCompleteDevVAddr;
	}

	
	for (i = 0; i < psKick->ui32NumSrcSync; i++)
	{
		psSyncInfo = psKick->ahSrcSyncInfo[i];
		psSyncInfo->psSyncData->ui32ReadOpsPending++;
	}

	if (psKick->hDstSyncInfo != IMG_NULL)
	{
		psSyncInfo = psKick->hDstSyncInfo;
		psSyncInfo->psSyncData->ui32WriteOpsPending++;
	}

#if defined(PDUMP)
	if ((PDumpIsCaptureFrameKM()
	|| ((psKick->ui32PDumpFlags & PDUMP_FLAGS_CONTINUOUS) != 0))
	&&  (bPersistentProcess == IMG_FALSE) )
	{
		
		PDUMPCOMMENT("Shared part of 2D command\r\n");
		PDUMPMEM(ps2DCmd,
				psCCBMemInfo,
				psKick->ui32CCBDumpWOff,
				sizeof(SGXMKIF_2DCMD_SHARED),
				psKick->ui32PDumpFlags,
				MAKEUNIQUETAG(psCCBMemInfo));

		for (i = 0; i < psKick->ui32NumSrcSync; i++)
		{
			psSyncInfo = psKick->ahSrcSyncInfo[i];

			PDUMPCOMMENT("Hack src surface write op in 2D cmd\r\n");
			PDUMPMEM(&psSyncInfo->psSyncData->ui32LastOpDumpVal,
					psCCBMemInfo,
					psKick->ui32CCBDumpWOff + (IMG_UINT32)offsetof(SGXMKIF_2DCMD_SHARED, sSrcSyncData[i].ui32WriteOpsPendingVal),
					sizeof(psSyncInfo->psSyncData->ui32LastOpDumpVal),
					psKick->ui32PDumpFlags,
					MAKEUNIQUETAG(psCCBMemInfo));

			PDUMPCOMMENT("Hack src surface read op in 2D cmd\r\n");
			PDUMPMEM(&psSyncInfo->psSyncData->ui32LastReadOpDumpVal,
					psCCBMemInfo,
					psKick->ui32CCBDumpWOff + (IMG_UINT32)offsetof(SGXMKIF_2DCMD_SHARED, sSrcSyncData[i].ui32ReadOpsPendingVal),
					sizeof(psSyncInfo->psSyncData->ui32LastReadOpDumpVal),
					psKick->ui32PDumpFlags,
					MAKEUNIQUETAG(psCCBMemInfo));
		}

		if (psKick->hDstSyncInfo != IMG_NULL)
		{
			psSyncInfo = psKick->hDstSyncInfo;

			PDUMPCOMMENT("Hack dest surface write op in 2D cmd\r\n");
			PDUMPMEM(&psSyncInfo->psSyncData->ui32LastOpDumpVal,
					psCCBMemInfo,
					psKick->ui32CCBDumpWOff + (IMG_UINT32)offsetof(SGXMKIF_2DCMD_SHARED, sDstSyncData.ui32WriteOpsPendingVal),
					sizeof(psSyncInfo->psSyncData->ui32LastOpDumpVal),
					psKick->ui32PDumpFlags,
					MAKEUNIQUETAG(psCCBMemInfo));

			PDUMPCOMMENT("Hack dest surface read op in 2D cmd\r\n");
			PDUMPMEM(&psSyncInfo->psSyncData->ui32LastReadOpDumpVal,
					psCCBMemInfo,
					psKick->ui32CCBDumpWOff + (IMG_UINT32)offsetof(SGXMKIF_2DCMD_SHARED, sDstSyncData.ui32ReadOpsPendingVal),
					sizeof(psSyncInfo->psSyncData->ui32LastReadOpDumpVal),
					psKick->ui32PDumpFlags,
					MAKEUNIQUETAG(psCCBMemInfo));
		}

		
		for (i = 0; i < psKick->ui32NumSrcSync; i++)
		{
			psSyncInfo = psKick->ahSrcSyncInfo[i];
			psSyncInfo->psSyncData->ui32LastReadOpDumpVal++;
		}

		if (psKick->hDstSyncInfo != IMG_NULL)
		{
			psSyncInfo = psKick->hDstSyncInfo;
			psSyncInfo->psSyncData->ui32LastOpDumpVal++;
		}
	}		
#endif

	sCommand.ui32Data[1] = psKick->sHW2DContextDevVAddr.uiAddr;
	
	eError = SGXScheduleCCBCommandKM(hDevHandle, SGXMKIF_CMD_2D, &sCommand, KERNEL_ID, psKick->ui32PDumpFlags, hDevMemContext, IMG_FALSE);	

	if (eError == PVRSRV_ERROR_RETRY)
	{
		

#if defined(PDUMP)
		if (PDumpIsCaptureFrameKM())
		{
			for (i = 0; i < psKick->ui32NumSrcSync; i++)
			{
				psSyncInfo = psKick->ahSrcSyncInfo[i];
				psSyncInfo->psSyncData->ui32LastReadOpDumpVal--;
			}

			if (psKick->hDstSyncInfo != IMG_NULL)
			{
				psSyncInfo = psKick->hDstSyncInfo;
				psSyncInfo->psSyncData->ui32LastOpDumpVal--;
			}
		}
#endif

		for (i = 0; i < psKick->ui32NumSrcSync; i++)
		{
			psSyncInfo = psKick->ahSrcSyncInfo[i];
			psSyncInfo->psSyncData->ui32ReadOpsPending--;
		}

		if (psKick->hDstSyncInfo != IMG_NULL)
		{
			psSyncInfo = psKick->hDstSyncInfo;
			psSyncInfo->psSyncData->ui32WriteOpsPending--;
		}

		
		if (psKick->hTASyncInfo != IMG_NULL)
		{
			psSyncInfo = (PVRSRV_KERNEL_SYNC_INFO *)psKick->hTASyncInfo;

			psSyncInfo->psSyncData->ui32WriteOpsPending--;
		}

		
		if (psKick->h3DSyncInfo != IMG_NULL)
		{
			psSyncInfo = (PVRSRV_KERNEL_SYNC_INFO *)psKick->h3DSyncInfo;

			psSyncInfo->psSyncData->ui32WriteOpsPending--;
		}
	}

	


#if defined(NO_HARDWARE)
	
	for(i = 0; i < psKick->ui32NumSrcSync; i++)
	{
		psSyncInfo = (PVRSRV_KERNEL_SYNC_INFO *)psKick->ahSrcSyncInfo[i];
		psSyncInfo->psSyncData->ui32ReadOpsComplete = psSyncInfo->psSyncData->ui32ReadOpsPending;
	}

	if (psKick->hDstSyncInfo != IMG_NULL)
	{
		psSyncInfo = (PVRSRV_KERNEL_SYNC_INFO *)psKick->hDstSyncInfo;

		psSyncInfo->psSyncData->ui32WriteOpsComplete = psSyncInfo->psSyncData->ui32WriteOpsPending;
	}

	if (psKick->hTASyncInfo != IMG_NULL)
	{
		psSyncInfo = (PVRSRV_KERNEL_SYNC_INFO *)psKick->hTASyncInfo;

		psSyncInfo->psSyncData->ui32WriteOpsComplete = psSyncInfo->psSyncData->ui32WriteOpsPending;
	}

	if (psKick->h3DSyncInfo != IMG_NULL)
	{
		psSyncInfo = (PVRSRV_KERNEL_SYNC_INFO *)psKick->h3DSyncInfo;

		psSyncInfo->psSyncData->ui32WriteOpsComplete = psSyncInfo->psSyncData->ui32WriteOpsPending;
	}
#endif

	return eError;
}
#endif	
#endif	
