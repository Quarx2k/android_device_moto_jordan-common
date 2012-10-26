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

#ifndef __BRIDGED_SUPPORT_H__
#define __BRIDGED_SUPPORT_H__

#include "handle.h"

#if defined(__cplusplus)
extern "C" {
#endif

#if defined (SUPPORT_SID_INTERFACE)
PVRSRV_ERROR PVRSRVLookupOSMemHandle(PVRSRV_HANDLE_BASE *psBase, IMG_HANDLE *phOSMemHandle, IMG_SID hMHandle);
#else
PVRSRV_ERROR PVRSRVLookupOSMemHandle(PVRSRV_HANDLE_BASE *psBase, IMG_HANDLE *phOSMemHandle, IMG_HANDLE hMHandle);
#endif

#if defined (__cplusplus)
}
#endif

#endif 

