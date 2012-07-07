/*************************************************************************/ /*!
@Title          Services reference count debugging
@Copyright      Copyright (c) Imagination Technologies Ltd. All Rights Reserved
@License        Strictly Confidential.
*/ /**************************************************************************/

#ifndef __REFCOUNT_H__
#define __REFCOUNT_H__

#if defined(PVRSRV_REFCOUNT_DEBUG)

void PVRSRVDumpRefCountCCB(void);

#define PVRSRVKernelSyncInfoIncRef(x...) \
	PVRSRVKernelSyncInfoIncRef2(__FILE__, __LINE__, x)
#define PVRSRVKernelSyncInfoDecRef(x...) \
	PVRSRVKernelSyncInfoDecRef2(__FILE__, __LINE__, x)
#define PVRSRVKernelMemInfoIncRef(x...) \
	PVRSRVKernelMemInfoIncRef2(__FILE__, __LINE__, x)
#define PVRSRVKernelMemInfoDecRef(x...) \
	PVRSRVKernelMemInfoDecRef2(__FILE__, __LINE__, x)
#define PVRSRVBMBufIncRef(x...) \
	PVRSRVBMBufIncRef2(__FILE__, __LINE__, x)
#define PVRSRVBMBufDecRef(x...) \
	PVRSRVBMBufDecRef2(__FILE__, __LINE__, x)
#define PVRSRVBMBufIncExport(x...) \
	PVRSRVBMBufIncExport2(__FILE__, __LINE__, x)
#define PVRSRVBMBufDecExport(x...) \
	PVRSRVBMBufDecExport2(__FILE__, __LINE__, x)

void PVRSRVKernelSyncInfoIncRef2(const IMG_CHAR *pszFile, IMG_INT iLine,
								 PVRSRV_KERNEL_SYNC_INFO *psKernelSyncInfo,
								 PVRSRV_KERNEL_MEM_INFO *psKernelMemInfo);
void PVRSRVKernelSyncInfoDecRef2(const IMG_CHAR *pszFile, IMG_INT iLine,
								 PVRSRV_KERNEL_SYNC_INFO *psKernelSyncInfo,
								 PVRSRV_KERNEL_MEM_INFO *psKernelMemInfo);
void PVRSRVKernelMemInfoIncRef2(const IMG_CHAR *pszFile, IMG_INT iLine,
								PVRSRV_KERNEL_MEM_INFO *psKernelMemInfo);
void PVRSRVKernelMemInfoDecRef2(const IMG_CHAR *pszFile, IMG_INT iLine,
								PVRSRV_KERNEL_MEM_INFO *psKernelMemInfo);
void PVRSRVBMBufIncRef2(const IMG_CHAR *pszFile,
						IMG_INT iLine, BM_BUF *pBuf);
void PVRSRVBMBufDecRef2(const IMG_CHAR *pszFile,
						IMG_INT iLine, BM_BUF *pBuf);
void PVRSRVBMBufIncExport2(const IMG_CHAR *pszFile,
						   IMG_INT iLine, BM_BUF *pBuf);
void PVRSRVBMBufDecExport2(const IMG_CHAR *pszFile,
						   IMG_INT iLine, BM_BUF *pBuf);

#if defined(__linux__)

/* mmap refcounting is Linux specific */
#include "mmap.h"

#define PVRSRVOffsetStructIncRef(x...) \
	PVRSRVOffsetStructIncRef2(__FILE__, __LINE__, x)
#define PVRSRVOffsetStructDecRef(x...) \
	PVRSRVOffsetStructDecRef2(__FILE__, __LINE__, x)
#define PVRSRVOffsetStructIncMapped(x...) \
	PVRSRVOffsetStructIncMapped2(__FILE__, __LINE__, x)
#define PVRSRVOffsetStructDecMapped(x...) \
	PVRSRVOffsetStructDecMapped2(__FILE__, __LINE__, x)

void PVRSRVOffsetStructIncRef2(const IMG_CHAR *pszFile, IMG_INT iLine,
							   PKV_OFFSET_STRUCT psOffsetStruct);
void PVRSRVOffsetStructDecRef2(const IMG_CHAR *pszFile, IMG_INT iLine,
							   PKV_OFFSET_STRUCT psOffsetStruct);
void PVRSRVOffsetStructIncMapped2(const IMG_CHAR *pszFile, IMG_INT iLine,
								  PKV_OFFSET_STRUCT psOffsetStruct);
void PVRSRVOffsetStructDecMapped2(const IMG_CHAR *pszFile, IMG_INT iLine,
								  PKV_OFFSET_STRUCT psOffsetStruct);

#endif /* defined(__linux__) */

#else /* defined(PVRSRV_REFCOUNT_DEBUG) */

static INLINE void PVRSRVDumpRefCountCCB(void) { }

static INLINE void PVRSRVKernelSyncInfoIncRef(PVRSRV_KERNEL_SYNC_INFO *psKernelSyncInfo,
								PVRSRV_KERNEL_MEM_INFO *psKernelMemInfo)
{
	PVR_UNREFERENCED_PARAMETER(psKernelMemInfo);
	psKernelSyncInfo->ui32RefCount++;
}

static INLINE void PVRSRVKernelSyncInfoDecRef(PVRSRV_KERNEL_SYNC_INFO *psKernelSyncInfo,
								PVRSRV_KERNEL_MEM_INFO *psKernelMemInfo)
{
	PVR_UNREFERENCED_PARAMETER(psKernelMemInfo);
	psKernelSyncInfo->ui32RefCount--;
}

static INLINE void PVRSRVKernelMemInfoIncRef(PVRSRV_KERNEL_MEM_INFO *psKernelMemInfo)
{
	psKernelMemInfo->ui32RefCount++;
}

static INLINE void PVRSRVKernelMemInfoDecRef(PVRSRV_KERNEL_MEM_INFO *psKernelMemInfo)
{
	psKernelMemInfo->ui32RefCount--;
}

#if defined(__linux__)

/* mmap refcounting is Linux specific */
#include "mmap.h"

static INLINE void PVRSRVOffsetStructIncRef(PKV_OFFSET_STRUCT psOffsetStruct)
{
	psOffsetStruct->ui32RefCount++;
}

static INLINE void PVRSRVOffsetStructDecRef(PKV_OFFSET_STRUCT psOffsetStruct)
{
	psOffsetStruct->ui32RefCount--;
}

static INLINE void PVRSRVOffsetStructIncMapped(PKV_OFFSET_STRUCT psOffsetStruct)
{
	psOffsetStruct->ui32Mapped++;
}

static INLINE void PVRSRVOffsetStructDecMapped(PKV_OFFSET_STRUCT psOffsetStruct)
{
	psOffsetStruct->ui32Mapped--;
}

static INLINE void PVRSRVBMBufIncRef(BM_BUF *pBuf)
{
	pBuf->ui32RefCount++;
}

static INLINE void PVRSRVBMBufDecRef(BM_BUF *pBuf)
{
	pBuf->ui32RefCount--;
}

static INLINE void PVRSRVBMBufIncExport(BM_BUF *pBuf)
{
	pBuf->ui32ExportCount++;
}

static INLINE void PVRSRVBMBufDecExport(BM_BUF *pBuf)
{
	pBuf->ui32ExportCount--;
}

#endif /* defined(__linux__) */

#endif /* defined(PVRSRV_REFCOUNT_DEBUG) */

#endif /* __REFCOUNT_H__ */
