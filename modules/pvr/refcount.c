/*************************************************************************/ /*!
@Title          Services reference count debugging
@Copyright      Copyright (c) Imagination Technologies Ltd. All Rights Reserved
@License        Strictly Confidential.
*/ /**************************************************************************/

#if defined(PVRSRV_REFCOUNT_DEBUG)

#include "services_headers.h"

#ifndef __linux__
#warning Reference count debugging is not thread-safe on this platform
#define PVRSRV_LOCK_CCB()
#define PVRSRV_UNLOCK_CCB()
#else /* __linux__ */
#include <linux/mutex.h>
static DEFINE_MUTEX(gsCCBLock);
#define PVRSRV_LOCK_CCB()	mutex_lock(&gsCCBLock)
#define PVRSRV_UNLOCK_CCB()	mutex_unlock(&gsCCBLock)
#endif /* __linux__ */

#define PVRSRV_REFCOUNT_CCB_MAX			512
#define PVRSRV_REFCOUNT_CCB_MESG_MAX	80

#define PVRSRV_REFCOUNT_CCB_DEBUG_SYNCINFO	(1U << 0)
#define PVRSRV_REFCOUNT_CCB_DEBUG_MEMINFO	(1U << 1)
#define PVRSRV_REFCOUNT_CCB_DEBUG_BM_BUF	(1U << 2)
#define PVRSRV_REFCOUNT_CCB_DEBUG_BM_BUF2	(1U << 3)

#if defined(__linux__)
#define PVRSRV_REFCOUNT_CCB_DEBUG_MMAP		(1U << 4)
#define PVRSRV_REFCOUNT_CCB_DEBUG_MMAP2		(1U << 5)
#else
#define PVRSRV_REFCOUNT_CCB_DEBUG_MMAP		0
#define PVRSRV_REFCOUNT_CCB_DEBUG_MMAP2		0
#endif

#define PVRSRV_REFCOUNT_CCB_DEBUG_ALL		~0U

/*static const IMG_UINT guiDebugMask = PVRSRV_REFCOUNT_CCB_DEBUG_ALL;*/
static const IMG_UINT guiDebugMask =
	PVRSRV_REFCOUNT_CCB_DEBUG_SYNCINFO |
	PVRSRV_REFCOUNT_CCB_DEBUG_MMAP2;

typedef struct
{
	const IMG_CHAR *pszFile;
	IMG_INT iLine;
	IMG_UINT32 ui32PID;
	IMG_CHAR pcMesg[PVRSRV_REFCOUNT_CCB_MESG_MAX];
}
PVRSRV_REFCOUNT_CCB;

static PVRSRV_REFCOUNT_CCB gsRefCountCCB[PVRSRV_REFCOUNT_CCB_MAX];
static IMG_UINT giOffset;

static const IMG_CHAR gszHeader[] =
	/*        10        20        30        40        50        60        70
	 * 345678901234567890123456789012345678901234567890123456789012345678901
	 */
	"TYPE     SYNCINFO MEMINFO  MEMHANDLE OTHER    REF  REF' SIZE     PID";
	/* NCINFO deadbeef deadbeef deadbeef  deadbeef 1234 1234 deadbeef */

#define PVRSRV_REFCOUNT_CCB_FMT_STRING "%8.8s %8p %8p %8p  %8p %.4d %.4d %.8x"

IMG_INTERNAL
void PVRSRVDumpRefCountCCB(void)
{
	int i;

	PVRSRV_LOCK_CCB();

	PVR_LOG(("%s", gszHeader));

	for(i = 0; i < PVRSRV_REFCOUNT_CCB_MAX; i++)
	{
		PVRSRV_REFCOUNT_CCB *psRefCountCCBEntry =
			&gsRefCountCCB[(giOffset + i) % PVRSRV_REFCOUNT_CCB_MAX];

		/* Early on, we won't have MAX_REFCOUNT_CCB_SIZE messages */
		if(!psRefCountCCBEntry->pszFile)
			break;

		PVR_LOG(("%s %d %s:%d", psRefCountCCBEntry->pcMesg,
								psRefCountCCBEntry->ui32PID,
								psRefCountCCBEntry->pszFile,
								psRefCountCCBEntry->iLine));
	}

	PVRSRV_UNLOCK_CCB();
}

IMG_INTERNAL
void PVRSRVKernelSyncInfoIncRef2(const IMG_CHAR *pszFile, IMG_INT iLine,
								 PVRSRV_KERNEL_SYNC_INFO *psKernelSyncInfo,
								 PVRSRV_KERNEL_MEM_INFO *psKernelMemInfo)
{
	if(!(guiDebugMask & PVRSRV_REFCOUNT_CCB_DEBUG_SYNCINFO))
		goto skip;

	PVRSRV_LOCK_CCB();

	gsRefCountCCB[giOffset].pszFile = pszFile;
	gsRefCountCCB[giOffset].iLine = iLine;
	gsRefCountCCB[giOffset].ui32PID = OSGetCurrentProcessIDKM();
	snprintf(gsRefCountCCB[giOffset].pcMesg,
			 PVRSRV_REFCOUNT_CCB_MESG_MAX - 1,
			 PVRSRV_REFCOUNT_CCB_FMT_STRING,
			 "SYNCINFO",
			 psKernelSyncInfo,
			 psKernelMemInfo,
			 NULL,
			 (psKernelMemInfo) ? psKernelMemInfo->sMemBlk.hOSMemHandle : NULL,
			 psKernelSyncInfo->ui32RefCount,
			 psKernelSyncInfo->ui32RefCount + 1,
			 (psKernelMemInfo) ? psKernelMemInfo->uAllocSize : 0);
	gsRefCountCCB[giOffset].pcMesg[PVRSRV_REFCOUNT_CCB_MESG_MAX - 1] = 0;
	giOffset = (giOffset + 1) % PVRSRV_REFCOUNT_CCB_MAX;

	PVRSRV_UNLOCK_CCB();

skip:
	psKernelSyncInfo->ui32RefCount++;
}

IMG_INTERNAL
void PVRSRVKernelSyncInfoDecRef2(const IMG_CHAR *pszFile, IMG_INT iLine,
								 PVRSRV_KERNEL_SYNC_INFO *psKernelSyncInfo,
								 PVRSRV_KERNEL_MEM_INFO *psKernelMemInfo)
{
	if(!(guiDebugMask & PVRSRV_REFCOUNT_CCB_DEBUG_SYNCINFO))
		goto skip;

	PVRSRV_LOCK_CCB();

	gsRefCountCCB[giOffset].pszFile = pszFile;
	gsRefCountCCB[giOffset].iLine = iLine;
	gsRefCountCCB[giOffset].ui32PID = OSGetCurrentProcessIDKM();
	snprintf(gsRefCountCCB[giOffset].pcMesg,
			 PVRSRV_REFCOUNT_CCB_MESG_MAX - 1,
			 PVRSRV_REFCOUNT_CCB_FMT_STRING,
			 "SYNCINFO",
			 psKernelSyncInfo,
			 psKernelMemInfo,
			 (psKernelMemInfo) ? psKernelMemInfo->sMemBlk.hOSMemHandle : NULL,
			 NULL,
			 psKernelSyncInfo->ui32RefCount,
			 psKernelSyncInfo->ui32RefCount - 1,
			 (psKernelMemInfo) ? psKernelMemInfo->uAllocSize : 0);
	gsRefCountCCB[giOffset].pcMesg[PVRSRV_REFCOUNT_CCB_MESG_MAX - 1] = 0;
	giOffset = (giOffset + 1) % PVRSRV_REFCOUNT_CCB_MAX;

	PVRSRV_UNLOCK_CCB();

skip:
	psKernelSyncInfo->ui32RefCount--;
}

IMG_INTERNAL
void PVRSRVKernelMemInfoIncRef2(const IMG_CHAR *pszFile, IMG_INT iLine,
								PVRSRV_KERNEL_MEM_INFO *psKernelMemInfo)
{
	if(!(guiDebugMask & PVRSRV_REFCOUNT_CCB_DEBUG_MEMINFO))
		goto skip;

	PVRSRV_LOCK_CCB();

	gsRefCountCCB[giOffset].pszFile = pszFile;
	gsRefCountCCB[giOffset].iLine = iLine;
	gsRefCountCCB[giOffset].ui32PID = OSGetCurrentProcessIDKM();
	snprintf(gsRefCountCCB[giOffset].pcMesg,
			 PVRSRV_REFCOUNT_CCB_MESG_MAX - 1,
			 PVRSRV_REFCOUNT_CCB_FMT_STRING,
			 "MEMINFO",
			 psKernelMemInfo->psKernelSyncInfo,
			 psKernelMemInfo,
			 psKernelMemInfo->sMemBlk.hOSMemHandle,
			 NULL,
			 psKernelMemInfo->ui32RefCount,
			 psKernelMemInfo->ui32RefCount + 1,
			 psKernelMemInfo->uAllocSize);
	gsRefCountCCB[giOffset].pcMesg[PVRSRV_REFCOUNT_CCB_MESG_MAX - 1] = 0;
	giOffset = (giOffset + 1) % PVRSRV_REFCOUNT_CCB_MAX;

	PVRSRV_UNLOCK_CCB();

skip:
	psKernelMemInfo->ui32RefCount++;
}

IMG_INTERNAL
void PVRSRVKernelMemInfoDecRef2(const IMG_CHAR *pszFile, IMG_INT iLine,
								PVRSRV_KERNEL_MEM_INFO *psKernelMemInfo)
{
	if(!(guiDebugMask & PVRSRV_REFCOUNT_CCB_DEBUG_MEMINFO))
		goto skip;

	PVRSRV_LOCK_CCB();

	gsRefCountCCB[giOffset].pszFile = pszFile;
	gsRefCountCCB[giOffset].iLine = iLine;
	gsRefCountCCB[giOffset].ui32PID = OSGetCurrentProcessIDKM();
	snprintf(gsRefCountCCB[giOffset].pcMesg,
			 PVRSRV_REFCOUNT_CCB_MESG_MAX - 1,
			 PVRSRV_REFCOUNT_CCB_FMT_STRING,
			 "MEMINFO",
			 psKernelMemInfo->psKernelSyncInfo,
			 psKernelMemInfo,
			 psKernelMemInfo->sMemBlk.hOSMemHandle,
			 NULL,
			 psKernelMemInfo->ui32RefCount,
			 psKernelMemInfo->ui32RefCount - 1,
			 psKernelMemInfo->uAllocSize);
	gsRefCountCCB[giOffset].pcMesg[PVRSRV_REFCOUNT_CCB_MESG_MAX - 1] = 0;
	giOffset = (giOffset + 1) % PVRSRV_REFCOUNT_CCB_MAX;

	PVRSRV_UNLOCK_CCB();

skip:
	psKernelMemInfo->ui32RefCount--;
}

IMG_INTERNAL
void PVRSRVBMBufIncRef2(const IMG_CHAR *pszFile, IMG_INT iLine, BM_BUF *pBuf)
{
	if(!(guiDebugMask & PVRSRV_REFCOUNT_CCB_DEBUG_BM_BUF))
		goto skip;

	PVRSRV_LOCK_CCB();

	gsRefCountCCB[giOffset].pszFile = pszFile;
	gsRefCountCCB[giOffset].iLine = iLine;
	gsRefCountCCB[giOffset].ui32PID = OSGetCurrentProcessIDKM();
	snprintf(gsRefCountCCB[giOffset].pcMesg,
			 PVRSRV_REFCOUNT_CCB_MESG_MAX - 1,
			 PVRSRV_REFCOUNT_CCB_FMT_STRING,
			 "BM_BUF",
			 NULL,
			 NULL,
			 BM_HandleToOSMemHandle(pBuf),
			 pBuf,
			 pBuf->ui32RefCount,
			 pBuf->ui32RefCount + 1,
			 (pBuf->pMapping) ? pBuf->pMapping->uSize : 0);
	gsRefCountCCB[giOffset].pcMesg[PVRSRV_REFCOUNT_CCB_MESG_MAX - 1] = 0;
	giOffset = (giOffset + 1) % PVRSRV_REFCOUNT_CCB_MAX;

	PVRSRV_UNLOCK_CCB();

skip:
	pBuf->ui32RefCount++;
}

IMG_INTERNAL
void PVRSRVBMBufDecRef2(const IMG_CHAR *pszFile, IMG_INT iLine, BM_BUF *pBuf)
{
	if(!(guiDebugMask & PVRSRV_REFCOUNT_CCB_DEBUG_BM_BUF))
		goto skip;

	PVRSRV_LOCK_CCB();

	gsRefCountCCB[giOffset].pszFile = pszFile;
	gsRefCountCCB[giOffset].iLine = iLine;
	gsRefCountCCB[giOffset].ui32PID = OSGetCurrentProcessIDKM();
	snprintf(gsRefCountCCB[giOffset].pcMesg,
			 PVRSRV_REFCOUNT_CCB_MESG_MAX - 1,
			 PVRSRV_REFCOUNT_CCB_FMT_STRING,
			 "BM_BUF",
			 NULL,
			 NULL,
			 BM_HandleToOSMemHandle(pBuf),
			 pBuf,
			 pBuf->ui32RefCount,
			 pBuf->ui32RefCount - 1,
			 (pBuf->pMapping) ? pBuf->pMapping->uSize : 0);
	gsRefCountCCB[giOffset].pcMesg[PVRSRV_REFCOUNT_CCB_MESG_MAX - 1] = 0;
	giOffset = (giOffset + 1) % PVRSRV_REFCOUNT_CCB_MAX;

	PVRSRV_UNLOCK_CCB();

skip:
	pBuf->ui32RefCount--;
}

IMG_INTERNAL
void PVRSRVBMBufIncExport2(const IMG_CHAR *pszFile, IMG_INT iLine, BM_BUF *pBuf)
{
	if(!(guiDebugMask & PVRSRV_REFCOUNT_CCB_DEBUG_BM_BUF2))
		goto skip;

	PVRSRV_LOCK_CCB();

	gsRefCountCCB[giOffset].pszFile = pszFile;
	gsRefCountCCB[giOffset].iLine = iLine;
	gsRefCountCCB[giOffset].ui32PID = OSGetCurrentProcessIDKM();
	snprintf(gsRefCountCCB[giOffset].pcMesg,
			 PVRSRV_REFCOUNT_CCB_MESG_MAX - 1,
			 PVRSRV_REFCOUNT_CCB_FMT_STRING,
			 "BM_BUF2",
			 NULL,
			 NULL,
			 BM_HandleToOSMemHandle(pBuf),
			 pBuf,
			 pBuf->ui32ExportCount,
			 pBuf->ui32ExportCount + 1,
			 (pBuf->pMapping) ? pBuf->pMapping->uSize : 0);
	gsRefCountCCB[giOffset].pcMesg[PVRSRV_REFCOUNT_CCB_MESG_MAX - 1] = 0;
	giOffset = (giOffset + 1) % PVRSRV_REFCOUNT_CCB_MAX;

	PVRSRV_UNLOCK_CCB();

skip:
	pBuf->ui32ExportCount++;
}

IMG_INTERNAL
void PVRSRVBMBufDecExport2(const IMG_CHAR *pszFile, IMG_INT iLine, BM_BUF *pBuf)
{
	if(!(guiDebugMask & PVRSRV_REFCOUNT_CCB_DEBUG_BM_BUF2))
		goto skip;

	PVRSRV_LOCK_CCB();

	gsRefCountCCB[giOffset].pszFile = pszFile;
	gsRefCountCCB[giOffset].iLine = iLine;
	gsRefCountCCB[giOffset].ui32PID = OSGetCurrentProcessIDKM();
	snprintf(gsRefCountCCB[giOffset].pcMesg,
			 PVRSRV_REFCOUNT_CCB_MESG_MAX - 1,
			 PVRSRV_REFCOUNT_CCB_FMT_STRING,
			 "BM_BUF2",
			 NULL,
			 NULL,
			 BM_HandleToOSMemHandle(pBuf),
			 pBuf,
			 pBuf->ui32ExportCount,
			 pBuf->ui32ExportCount - 1,
			 (pBuf->pMapping) ? pBuf->pMapping->uSize : 0);
	gsRefCountCCB[giOffset].pcMesg[PVRSRV_REFCOUNT_CCB_MESG_MAX - 1] = 0;
	giOffset = (giOffset + 1) % PVRSRV_REFCOUNT_CCB_MAX;

	PVRSRV_UNLOCK_CCB();

skip:
	pBuf->ui32ExportCount--;
}

#if defined(__linux__)

/* mmap refcounting is Linux specific */

IMG_INTERNAL
void PVRSRVOffsetStructIncRef2(const IMG_CHAR *pszFile, IMG_INT iLine,
							   PKV_OFFSET_STRUCT psOffsetStruct)
{
	if(!(guiDebugMask & PVRSRV_REFCOUNT_CCB_DEBUG_MMAP))
		goto skip;

	PVRSRV_LOCK_CCB();

	gsRefCountCCB[giOffset].pszFile = pszFile;
	gsRefCountCCB[giOffset].iLine = iLine;
	gsRefCountCCB[giOffset].ui32PID = OSGetCurrentProcessIDKM();
	snprintf(gsRefCountCCB[giOffset].pcMesg,
			 PVRSRV_REFCOUNT_CCB_MESG_MAX - 1,
			 PVRSRV_REFCOUNT_CCB_FMT_STRING,
			 "MMAP",
			 NULL,
			 NULL,
			 psOffsetStruct->psLinuxMemArea,
			 psOffsetStruct,
			 psOffsetStruct->ui32RefCount,
			 psOffsetStruct->ui32RefCount + 1,
			 psOffsetStruct->ui32RealByteSize);
	gsRefCountCCB[giOffset].pcMesg[PVRSRV_REFCOUNT_CCB_MESG_MAX - 1] = 0;
	giOffset = (giOffset + 1) % PVRSRV_REFCOUNT_CCB_MAX;

	PVRSRV_UNLOCK_CCB();

skip:
	psOffsetStruct->ui32RefCount++;
}

IMG_INTERNAL
void PVRSRVOffsetStructDecRef2(const IMG_CHAR *pszFile, IMG_INT iLine,
							   PKV_OFFSET_STRUCT psOffsetStruct)
{
	if(!(guiDebugMask & PVRSRV_REFCOUNT_CCB_DEBUG_MMAP))
		goto skip;

	PVRSRV_LOCK_CCB();

	gsRefCountCCB[giOffset].pszFile = pszFile;
	gsRefCountCCB[giOffset].iLine = iLine;
	gsRefCountCCB[giOffset].ui32PID = OSGetCurrentProcessIDKM();
	snprintf(gsRefCountCCB[giOffset].pcMesg,
			 PVRSRV_REFCOUNT_CCB_MESG_MAX - 1,
			 PVRSRV_REFCOUNT_CCB_FMT_STRING,
			 "MMAP",
			 NULL,
			 NULL,
			 psOffsetStruct->psLinuxMemArea,
			 psOffsetStruct,
			 psOffsetStruct->ui32RefCount,
			 psOffsetStruct->ui32RefCount - 1,
			 psOffsetStruct->ui32RealByteSize);
	gsRefCountCCB[giOffset].pcMesg[PVRSRV_REFCOUNT_CCB_MESG_MAX - 1] = 0;
	giOffset = (giOffset + 1) % PVRSRV_REFCOUNT_CCB_MAX;

	PVRSRV_UNLOCK_CCB();

skip:
	psOffsetStruct->ui32RefCount--;
}

IMG_INTERNAL
void PVRSRVOffsetStructIncMapped2(const IMG_CHAR *pszFile, IMG_INT iLine,
								  PKV_OFFSET_STRUCT psOffsetStruct)
{
	if(!(guiDebugMask & PVRSRV_REFCOUNT_CCB_DEBUG_MMAP2))
		goto skip;

	PVRSRV_LOCK_CCB();

	gsRefCountCCB[giOffset].pszFile = pszFile;
	gsRefCountCCB[giOffset].iLine = iLine;
	gsRefCountCCB[giOffset].ui32PID = OSGetCurrentProcessIDKM();
	snprintf(gsRefCountCCB[giOffset].pcMesg,
			 PVRSRV_REFCOUNT_CCB_MESG_MAX - 1,
			 PVRSRV_REFCOUNT_CCB_FMT_STRING,
			 "MMAP2",
			 NULL,
			 NULL,
			 psOffsetStruct->psLinuxMemArea,
			 psOffsetStruct,
			 psOffsetStruct->ui32Mapped,
			 psOffsetStruct->ui32Mapped + 1,
			 psOffsetStruct->ui32RealByteSize);
	gsRefCountCCB[giOffset].pcMesg[PVRSRV_REFCOUNT_CCB_MESG_MAX - 1] = 0;
	giOffset = (giOffset + 1) % PVRSRV_REFCOUNT_CCB_MAX;

	PVRSRV_UNLOCK_CCB();

skip:
	psOffsetStruct->ui32Mapped++;
}

IMG_INTERNAL
void PVRSRVOffsetStructDecMapped2(const IMG_CHAR *pszFile, IMG_INT iLine,
								  PKV_OFFSET_STRUCT psOffsetStruct)
{
	if(!(guiDebugMask & PVRSRV_REFCOUNT_CCB_DEBUG_MMAP2))
		goto skip;

	PVRSRV_LOCK_CCB();

	gsRefCountCCB[giOffset].pszFile = pszFile;
	gsRefCountCCB[giOffset].iLine = iLine;
	gsRefCountCCB[giOffset].ui32PID = OSGetCurrentProcessIDKM();
	snprintf(gsRefCountCCB[giOffset].pcMesg,
			 PVRSRV_REFCOUNT_CCB_MESG_MAX - 1,
			 PVRSRV_REFCOUNT_CCB_FMT_STRING,
			 "MMAP2",
			 NULL,
			 NULL,
			 psOffsetStruct->psLinuxMemArea,
			 psOffsetStruct,
			 psOffsetStruct->ui32Mapped,
			 psOffsetStruct->ui32Mapped - 1,
			 psOffsetStruct->ui32RealByteSize);
	gsRefCountCCB[giOffset].pcMesg[PVRSRV_REFCOUNT_CCB_MESG_MAX - 1] = 0;
	giOffset = (giOffset + 1) % PVRSRV_REFCOUNT_CCB_MAX;

	PVRSRV_UNLOCK_CCB();

skip:
	psOffsetStruct->ui32Mapped--;
}

#endif /* defined(__linux__) */

#endif /* defined(PVRSRV_REFCOUNT_DEBUG) */
