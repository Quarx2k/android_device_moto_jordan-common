/**********************************************************************
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

#ifndef __LINKAGE_H__
#define __LINKAGE_H__

#if !defined(SUPPORT_DRI_DRM)
IMG_INT32 PVRSRV_BridgeDispatchKM(struct file *file, IMG_UINT cmd, IMG_UINT32 arg);
#endif

IMG_VOID PVRDPFInit(IMG_VOID);

#ifdef DEBUG
IMG_INT PVRDebugProcSetLevel(struct file *file, const IMG_CHAR *buffer, IMG_UINT32 count, IMG_VOID *data);
IMG_VOID PVRDebugSetLevel(IMG_UINT32 uDebugLevel);

#ifdef PVR_PROC_USE_SEQ_FILE
void ProcSeqShowDebugLevel(struct seq_file *sfile,void* el);
#else
IMG_INT PVRDebugProcGetLevel(IMG_CHAR *page, IMG_CHAR **start, off_t off, IMG_INT count, IMG_INT *eof, IMG_VOID *data);
#endif

#ifdef PVR_MANUAL_POWER_CONTROL
IMG_INT PVRProcSetPowerLevel(struct file *file, const IMG_CHAR *buffer, IMG_UINT32 count, IMG_VOID *data);

#ifdef PVR_PROC_USE_SEQ_FILE
void ProcSeqShowPowerLevel(struct seq_file *sfile,void* el);
#else
IMG_INT PVRProcGetPowerLevel(IMG_CHAR *page, IMG_CHAR **start, off_t off, IMG_INT count, IMG_INT *eof, IMG_VOID *data);
#endif


#endif
#endif	

#endif 
