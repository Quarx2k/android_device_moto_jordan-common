/*
 * Copyright (C) 2011 The Android Open Source Project
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

/*
 * Quarx (c) 2011 stud for libtpa.so, libtpa_core.so, libsmiledetect.so, libmotodbgutils.so 
 */

#define LOG_TAG "libfnc"

#include <fcntl.h>
#include <errno.h>
#include <dirent.h>
#include <math.h>
#include <poll.h>

#include <cutils/log.h>
#include <cutils/atomic.h>

/*****************************************************************************/
/*
EXPORTS
*/

//libtpa_core.so

 int TPA_SECCLK_Get_NITZ_Time()
{
 ALOGW(__func__);
return 0 ;
}

 int TPA_SECCLK_Set_NITZ_Time()
{
 ALOGW(__func__);
return 0 ;
}

 int TPA_SECCLK_Get_Alarm_Time()
{
 ALOGW(__func__);
return 0 ;
}

 int TPA_SECCLK_Set_Alarm_Time()
{
 ALOGW(__func__);
return 0 ;
}

 int TPA_SECCLK_Get_User_Time()
{
 ALOGW(__func__);
return 0 ;
}

 int TPA_SECCLK_Set_User_Time()
{
 ALOGW(__func__);
return 0 ;
}

 int TPA_SECCLK_Get_GPS_Time()
{
 ALOGW(__func__);
return 0 ;
}

 int TPA_SECCLK_Set_GPS_Time()
{
 ALOGW(__func__);
return 0 ;
}

//libtpa.so
 int TPA_LIB_KDF_Gen()
{
 ALOGW(__func__);
return 0 ;
}

//libmotodbgutils.so
 int moto_panic()
{
 ALOGW(__func__);
return 0 ;
}

//libsmiledetect.so
 int destroySmileDetectEngine()
{
 ALOGW(__func__);
return 0 ;
}

 int createSmileDetectEngine()
{
 ALOGW(__func__);
return 0 ;
}

//libbattd.so
 int BATTD_send_daemon_cmd()
{
 ALOGW(__func__);
return 0 ;
}

//libmetainfo.so
 int MM_MediaExtractor_DetectMediaType()
{
 ALOGW(__func__);
return 0;
}

 int MM_MediaExtractor_GetTrackType()
{
 ALOGW(__func__);
return 0;
}

//libjanus.so
 int WMDRM_MTPE_CleanDataStore()
{
 ALOGW(__func__);
return 0;
}

 int WMDRM_MTPE_GetDeviceCertification()
{
 ALOGW(__func__);
return 0;
}

 int WMDRM_MTPE_GetLicenseState()
{
 ALOGW(__func__);
return 0;
}

 int WMDRM_MTPE_GetMeterChallenge()
{
 ALOGW(__func__);
return 0;
}

 int WMDRM_MTPE_GetSecClock()
{
 ALOGW(__func__);
return 0;
}

 int WMDRM_MTPE_GetSecureTimeChallenge()
{
 ALOGW(__func__);
return 0;
}

 int WMDRM_MTPE_GetSyncList()
{
 ALOGW(__func__);
return 0;
}

 int WMDRM_MTPE_SendWMDRMPDCommand()
{
 ALOGW(__func__);
return 0;
}

 int WMDRM_MTPE_SendWMDRMPDRequest()
{
 ALOGW(__func__);
return 0;
}

 int WMDRM_MTPE_SetLicenseResponse()
{
 ALOGW(__func__);
return 0;
}

 int WMDRM_MTPE_SetMeterResponse()
{
 ALOGW(__func__);
return 0;
}

 int WMDRM_MTPE_SetSecureTimeResponse()
{
 ALOGW(__func__);
return 0;
}

