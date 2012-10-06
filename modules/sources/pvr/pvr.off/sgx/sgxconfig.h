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

#ifndef __SGXCONFIG_H__
#define __SGXCONFIG_H__

#include "sgxdefs.h"

#define DEV_DEVICE_TYPE			PVRSRV_DEVICE_TYPE_SGX
#define DEV_DEVICE_CLASS		PVRSRV_DEVICE_CLASS_3D

#define DEV_MAJOR_VERSION		1
#define DEV_MINOR_VERSION		0

#if SGX_FEATURE_ADDRESS_SPACE_SIZE == 32
	#if defined(SGX_FEATURE_2D_HARDWARE)
	#define SGX_2D_HEAP_BASE					 0x00100000
	#define SGX_2D_HEAP_SIZE					(0x08000000-0x00100000-0x00001000)
	#else
		#if defined(FIX_HW_BRN_26915)
		#define SGX_CGBUFFER_HEAP_BASE					 0x00100000
		#define SGX_CGBUFFER_HEAP_SIZE					(0x08000000-0x00100000-0x00001000)
		#endif
	#endif

	#if defined(SUPPORT_SGX_GENERAL_MAPPING_HEAP)
	#define SGX_GENERAL_MAPPING_HEAP_BASE		 0x08000000
	#define SGX_GENERAL_MAPPING_HEAP_SIZE		(0x08000000-0x00001000)
	#endif

	#define SGX_GENERAL_HEAP_BASE				 0x10000000
	#define SGX_GENERAL_HEAP_SIZE				(0xC8000000-0x00001000)

	#define SGX_3DPARAMETERS_HEAP_BASE			 0xD8000000
	#define SGX_3DPARAMETERS_HEAP_SIZE			(0x10000000-0x00001000)

	#define SGX_TADATA_HEAP_BASE				 0xE8000000
	#define SGX_TADATA_HEAP_SIZE				(0x0D000000-0x00001000)

	#define SGX_SYNCINFO_HEAP_BASE				 0xF5000000
	#define SGX_SYNCINFO_HEAP_SIZE				(0x01000000-0x00001000)

	#define SGX_PDSPIXEL_CODEDATA_HEAP_BASE		 0xF6000000
	#define SGX_PDSPIXEL_CODEDATA_HEAP_SIZE		(0x02000000-0x00001000)

	#define SGX_KERNEL_CODE_HEAP_BASE			 0xF8000000
	#define SGX_KERNEL_CODE_HEAP_SIZE			(0x00080000-0x00001000)

	#define SGX_PDSVERTEX_CODEDATA_HEAP_BASE	 0xF8400000
	#define SGX_PDSVERTEX_CODEDATA_HEAP_SIZE	(0x01C00000-0x00001000)

	#define SGX_KERNEL_DATA_HEAP_BASE		 	 0xFA000000
	#define SGX_KERNEL_DATA_HEAP_SIZE			(0x05000000-0x00001000)

	#define SGX_PIXELSHADER_HEAP_BASE			 0xFF000000
	#define SGX_PIXELSHADER_HEAP_SIZE			(0x00500000-0x00001000)
	
	#define SGX_VERTEXSHADER_HEAP_BASE			 0xFF800000
	#define SGX_VERTEXSHADER_HEAP_SIZE			(0x00200000-0x00001000)

	
	#define SGX_CORE_IDENTIFIED
#endif 

#if SGX_FEATURE_ADDRESS_SPACE_SIZE == 28
	#if defined(SUPPORT_SGX_GENERAL_MAPPING_HEAP)
	#define SGX_GENERAL_MAPPING_HEAP_BASE		 0x00001000
	#define SGX_GENERAL_MAPPING_HEAP_SIZE		(0x01800000-0x00001000-0x00001000)
	#endif
		
	#define SGX_GENERAL_HEAP_BASE				 0x01800000
	#define SGX_GENERAL_HEAP_SIZE				(0x07000000-0x00001000)

	#define SGX_3DPARAMETERS_HEAP_BASE			 0x08800000
	#define SGX_3DPARAMETERS_HEAP_SIZE			(0x04000000-0x00001000)

	#define SGX_TADATA_HEAP_BASE				 0x0C800000
	#define SGX_TADATA_HEAP_SIZE				(0x01000000-0x00001000)

	#define SGX_SYNCINFO_HEAP_BASE				 0x0D800000
	#define SGX_SYNCINFO_HEAP_SIZE				(0x00400000-0x00001000)

	#define SGX_PDSPIXEL_CODEDATA_HEAP_BASE		 0x0DC00000
	#define SGX_PDSPIXEL_CODEDATA_HEAP_SIZE		(0x00800000-0x00001000)

	#define SGX_KERNEL_CODE_HEAP_BASE			 0x0E400000
	#define SGX_KERNEL_CODE_HEAP_SIZE			(0x00080000-0x00001000)

	#define SGX_PDSVERTEX_CODEDATA_HEAP_BASE	 0x0E800000
	#define SGX_PDSVERTEX_CODEDATA_HEAP_SIZE	(0x00800000-0x00001000)

	#define SGX_KERNEL_DATA_HEAP_BASE			 0x0F000000
	#define SGX_KERNEL_DATA_HEAP_SIZE			(0x00400000-0x00001000)

	#define SGX_PIXELSHADER_HEAP_BASE			 0x0F400000
	#define SGX_PIXELSHADER_HEAP_SIZE			(0x00500000-0x00001000)

	#define SGX_VERTEXSHADER_HEAP_BASE			 0x0FC00000
	#define SGX_VERTEXSHADER_HEAP_SIZE			(0x00200000-0x00001000)

	
	#define SGX_CORE_IDENTIFIED

#endif 

#if !defined(SGX_CORE_IDENTIFIED)
	#error "sgxconfig.h: ERROR: unspecified SGX Core version"
#endif	

#endif 

