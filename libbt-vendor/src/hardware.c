/*
 * Copyright 2012 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/******************************************************************************
 *
 *  Filename:      hardware.c
 *
 *  Description:   Contains controller-specific functions, like
 *                      firmware patch download
 *                      low power mode operations
 *
 ******************************************************************************/

#define LOG_TAG "bt_hwcfg"

#include <utils/Log.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <signal.h>
#include <time.h>
#include <errno.h>
#include <fcntl.h>
#include <dirent.h>
#include <ctype.h>
#include <cutils/properties.h>
#include <stdlib.h>
#include "bt_hci_bdroid.h"
#include "bt_vendor_qcom.h"

#define MAX_CNT_RETRY 100

int hw_config(int nState)
{
    ALOGI("Starting hciattach daemon");
    char *szState[] = {"true", "false"};
    char *szReqSt = NULL;

    if(nState == BT_VND_PWR_OFF)
        szReqSt = szState[1];
    else
        szReqSt = szState[0];

    ALOGI("try to set %s", szReqSt);

    if (property_set("bluetooth.hciattach", szReqSt) < 0){
        ALOGE("Property Setting fail");
	return -1;
    }

    return 0;
}

int readTrpState()
{
    char szBtStatus[20] = {0, };
    if(property_get("bluetooth.status", szBtStatus, "") < 0){
        ALOGE("Fail to get bluetooth satus");
        return FALSE;
    }

    if(!strncmp(szBtStatus, "on", strlen("on"))){
        ALOGI("bluetooth satus is on");
        return TRUE;
    }
    return FALSE;
}

int is_hw_ready()
{
    int i=0;
    char szStatus[10] = {0,};

    for(i=MAX_CNT_RETRY; i>0; i--){
       usleep(50*1000);
       //TODO :: checking routine
       if(readTrpState()==TRUE){
           break;
       }
    }

    return (i==0)? FALSE:TRUE;
}
