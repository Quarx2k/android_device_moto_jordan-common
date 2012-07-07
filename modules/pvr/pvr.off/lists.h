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

#ifndef __LISTS_UTILS__
#define __LISTS_UTILS__

#include <stdarg.h>
#include "img_types.h"

#define DECLARE_LIST_FOR_EACH(TYPE) \
IMG_VOID List_##TYPE##_ForEach(TYPE *psHead, IMG_VOID(*pfnCallBack)(TYPE* psNode))

#define IMPLEMENT_LIST_FOR_EACH(TYPE) \
IMG_VOID List_##TYPE##_ForEach(TYPE *psHead, IMG_VOID(*pfnCallBack)(TYPE* psNode))\
{\
	while(psHead)\
	{\
		pfnCallBack(psHead);\
		psHead = psHead->psNext;\
	}\
}


#define DECLARE_LIST_FOR_EACH_VA(TYPE) \
IMG_VOID List_##TYPE##_ForEach_va(TYPE *psHead, IMG_VOID(*pfnCallBack)(TYPE* psNode, va_list va), ...)

#define IMPLEMENT_LIST_FOR_EACH_VA(TYPE) \
IMG_VOID List_##TYPE##_ForEach_va(TYPE *psHead, IMG_VOID(*pfnCallBack)(TYPE* psNode, va_list va), ...) \
{\
	va_list ap;\
	while(psHead)\
	{\
		va_start(ap, pfnCallBack);\
		pfnCallBack(psHead, ap);\
		psHead = psHead->psNext;\
		va_end(ap);\
	}\
}


#define DECLARE_LIST_ANY(TYPE) \
IMG_VOID* List_##TYPE##_Any(TYPE *psHead, IMG_VOID* (*pfnCallBack)(TYPE* psNode))

#define IMPLEMENT_LIST_ANY(TYPE) \
IMG_VOID* List_##TYPE##_Any(TYPE *psHead, IMG_VOID* (*pfnCallBack)(TYPE* psNode))\
{ \
	IMG_VOID *pResult;\
	TYPE *psNextNode;\
	pResult = IMG_NULL;\
	psNextNode = psHead;\
	while(psHead && !pResult)\
	{\
		psNextNode = psNextNode->psNext;\
		pResult = pfnCallBack(psHead);\
		psHead = psNextNode;\
	}\
	return pResult;\
}


#define DECLARE_LIST_ANY_VA(TYPE) \
IMG_VOID* List_##TYPE##_Any_va(TYPE *psHead, IMG_VOID*(*pfnCallBack)(TYPE* psNode, va_list va), ...)

#define IMPLEMENT_LIST_ANY_VA(TYPE) \
IMG_VOID* List_##TYPE##_Any_va(TYPE *psHead, IMG_VOID*(*pfnCallBack)(TYPE* psNode, va_list va), ...)\
{\
	va_list ap;\
	TYPE *psNextNode;\
	IMG_VOID* pResult = IMG_NULL;\
	while(psHead && !pResult)\
	{\
		psNextNode = psHead->psNext;\
		va_start(ap, pfnCallBack);\
		pResult = pfnCallBack(psHead, ap);\
		va_end(ap);\
		psHead = psNextNode;\
	}\
	return pResult;\
}

#define DECLARE_LIST_ANY_2(TYPE, RTYPE, CONTINUE) \
RTYPE List_##TYPE##_##RTYPE##_Any(TYPE *psHead, RTYPE (*pfnCallBack)(TYPE* psNode))

#define IMPLEMENT_LIST_ANY_2(TYPE, RTYPE, CONTINUE) \
RTYPE List_##TYPE##_##RTYPE##_Any(TYPE *psHead, RTYPE (*pfnCallBack)(TYPE* psNode))\
{ \
	RTYPE result;\
	TYPE *psNextNode;\
	result = CONTINUE;\
	psNextNode = psHead;\
	while(psHead && result == CONTINUE)\
	{\
		psNextNode = psNextNode->psNext;\
		result = pfnCallBack(psHead);\
		psHead = psNextNode;\
	}\
	return result;\
}


#define DECLARE_LIST_ANY_VA_2(TYPE, RTYPE, CONTINUE) \
RTYPE List_##TYPE##_##RTYPE##_Any_va(TYPE *psHead, RTYPE(*pfnCallBack)(TYPE* psNode, va_list va), ...)

#define IMPLEMENT_LIST_ANY_VA_2(TYPE, RTYPE, CONTINUE) \
RTYPE List_##TYPE##_##RTYPE##_Any_va(TYPE *psHead, RTYPE(*pfnCallBack)(TYPE* psNode, va_list va), ...)\
{\
	va_list ap;\
	TYPE *psNextNode;\
	RTYPE result = CONTINUE;\
	while(psHead && result == CONTINUE)\
	{\
		psNextNode = psHead->psNext;\
		va_start(ap, pfnCallBack);\
		result = pfnCallBack(psHead, ap);\
		va_end(ap);\
		psHead = psNextNode;\
	}\
	return result;\
}


#define DECLARE_LIST_REMOVE(TYPE) \
IMG_VOID List_##TYPE##_Remove(TYPE *psNode)

#define IMPLEMENT_LIST_REMOVE(TYPE) \
IMG_VOID List_##TYPE##_Remove(TYPE *psNode)\
{\
	(*psNode->ppsThis)=psNode->psNext;\
	if(psNode->psNext)\
	{\
		psNode->psNext->ppsThis = psNode->ppsThis;\
	}\
}

#define DECLARE_LIST_INSERT(TYPE) \
IMG_VOID List_##TYPE##_Insert(TYPE **ppsHead, TYPE *psNewNode)

#define IMPLEMENT_LIST_INSERT(TYPE) \
IMG_VOID List_##TYPE##_Insert(TYPE **ppsHead, TYPE *psNewNode)\
{\
	psNewNode->ppsThis = ppsHead;\
	psNewNode->psNext = *ppsHead;\
	*ppsHead = psNewNode;\
	if(psNewNode->psNext)\
	{\
		psNewNode->psNext->ppsThis = &(psNewNode->psNext);\
	}\
}


#define IS_LAST_ELEMENT(x) ((x)->psNext == IMG_NULL)

#endif
