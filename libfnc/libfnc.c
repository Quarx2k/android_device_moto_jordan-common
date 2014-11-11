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

#include <endian.h>
#include "md5.h"
#include "hash.h"

#define A m->counter[0]
#define B m->counter[1]
#define C m->counter[2]
#define D m->counter[3]
#define X data

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

 int AUDMGR_setTty()
{
 ALOGW(__func__);
return 0;
}

 int AUDMGR_stopTone()
{
 ALOGW(__func__);
return 0;
}

 int AUDMGR_playTone()
{
 ALOGW(__func__);
return 0;
}

 int AUDMGR_disableVoicePath()
{
 ALOGW(__func__);
return 0;
}

 int AUDMGR_enableVoicePath()
{
 ALOGW(__func__);
return 0;
}

 int AUDMGR_stopDtmf()
{
 ALOGW(__func__);
return 0;
}

 int AUDMGR_playDtmf()
{
 ALOGW(__func__);
return 0;
}

 int AUDMGR_getMuteState()
{
 ALOGW(__func__);
return 0;
}

 int AUDMGR_setMuteState()
{
 ALOGW(__func__);
return 0;
}

 int a2dp_sink_address()
{
 ALOGW(__func__);
return 0;
}

 int a2dp_set_sink()
{
 ALOGW(__func__);
return 0;
}

 int a2dp_write()
{
 ALOGW(__func__);
return 0;
}

 int a2dp_init()
{
 ALOGW(__func__);
return 0;
}

 int a2dp_stop()
{
 ALOGW(__func__);
return 0;
}

 int a2dp_cleanup()
{
 ALOGW(__func__);
return 0;
}

/* /system/bin/protocol_driver vs Android 5.0 */ 

void
MD5_Init (struct md5 *m)
{
  m->sz[0] = 0;
  m->sz[1] = 0;
  D = 0x10325476;
  C = 0x98badcfe;
  B = 0xefcdab89;
  A = 0x67452301;
}

#define F(x,y,z) CRAYFIX((x & y) | (~x & z))
#define G(x,y,z) CRAYFIX((x & z) | (y & ~z))
#define H(x,y,z) (x ^ y ^ z)
#define I(x,y,z) CRAYFIX(y ^ (x | ~z))

#define DOIT(a,b,c,d,k,s,i,OP) \
a = b + cshift(a + OP(b,c,d) + X[k] + (i), s)

#define DO1(a,b,c,d,k,s,i) DOIT(a,b,c,d,k,s,i,F)
#define DO2(a,b,c,d,k,s,i) DOIT(a,b,c,d,k,s,i,G)
#define DO3(a,b,c,d,k,s,i) DOIT(a,b,c,d,k,s,i,H)
#define DO4(a,b,c,d,k,s,i) DOIT(a,b,c,d,k,s,i,I)

static inline void
calc (struct md5 *m, u_int32_t *data)
{
  u_int32_t AA, BB, CC, DD;

  AA = A;
  BB = B;
  CC = C;
  DD = D;

  /* Round 1 */

  DO1(A,B,C,D,0,7,0xd76aa478);
  DO1(D,A,B,C,1,12,0xe8c7b756);
  DO1(C,D,A,B,2,17,0x242070db);
  DO1(B,C,D,A,3,22,0xc1bdceee);

  DO1(A,B,C,D,4,7,0xf57c0faf);
  DO1(D,A,B,C,5,12,0x4787c62a);
  DO1(C,D,A,B,6,17,0xa8304613);
  DO1(B,C,D,A,7,22,0xfd469501);

  DO1(A,B,C,D,8,7,0x698098d8);
  DO1(D,A,B,C,9,12,0x8b44f7af);
  DO1(C,D,A,B,10,17,0xffff5bb1);
  DO1(B,C,D,A,11,22,0x895cd7be);

  DO1(A,B,C,D,12,7,0x6b901122);
  DO1(D,A,B,C,13,12,0xfd987193);
  DO1(C,D,A,B,14,17,0xa679438e);
  DO1(B,C,D,A,15,22,0x49b40821);

  /* Round 2 */

  DO2(A,B,C,D,1,5,0xf61e2562);
  DO2(D,A,B,C,6,9,0xc040b340);
  DO2(C,D,A,B,11,14,0x265e5a51);
  DO2(B,C,D,A,0,20,0xe9b6c7aa);

  DO2(A,B,C,D,5,5,0xd62f105d);
  DO2(D,A,B,C,10,9,0x2441453);
  DO2(C,D,A,B,15,14,0xd8a1e681);
  DO2(B,C,D,A,4,20,0xe7d3fbc8);

  DO2(A,B,C,D,9,5,0x21e1cde6);
  DO2(D,A,B,C,14,9,0xc33707d6);
  DO2(C,D,A,B,3,14,0xf4d50d87);
  DO2(B,C,D,A,8,20,0x455a14ed);

  DO2(A,B,C,D,13,5,0xa9e3e905);
  DO2(D,A,B,C,2,9,0xfcefa3f8);
  DO2(C,D,A,B,7,14,0x676f02d9);
  DO2(B,C,D,A,12,20,0x8d2a4c8a);

  /* Round 3 */

  DO3(A,B,C,D,5,4,0xfffa3942);
  DO3(D,A,B,C,8,11,0x8771f681);
  DO3(C,D,A,B,11,16,0x6d9d6122);
  DO3(B,C,D,A,14,23,0xfde5380c);

  DO3(A,B,C,D,1,4,0xa4beea44);
  DO3(D,A,B,C,4,11,0x4bdecfa9);
  DO3(C,D,A,B,7,16,0xf6bb4b60);
  DO3(B,C,D,A,10,23,0xbebfbc70);

  DO3(A,B,C,D,13,4,0x289b7ec6);
  DO3(D,A,B,C,0,11,0xeaa127fa);
  DO3(C,D,A,B,3,16,0xd4ef3085);
  DO3(B,C,D,A,6,23,0x4881d05);

  DO3(A,B,C,D,9,4,0xd9d4d039);
  DO3(D,A,B,C,12,11,0xe6db99e5);
  DO3(C,D,A,B,15,16,0x1fa27cf8);
  DO3(B,C,D,A,2,23,0xc4ac5665);

  /* Round 4 */

  DO4(A,B,C,D,0,6,0xf4292244);
  DO4(D,A,B,C,7,10,0x432aff97);
  DO4(C,D,A,B,14,15,0xab9423a7);
  DO4(B,C,D,A,5,21,0xfc93a039);

  DO4(A,B,C,D,12,6,0x655b59c3);
  DO4(D,A,B,C,3,10,0x8f0ccc92);
  DO4(C,D,A,B,10,15,0xffeff47d);
  DO4(B,C,D,A,1,21,0x85845dd1);

  DO4(A,B,C,D,8,6,0x6fa87e4f);
  DO4(D,A,B,C,15,10,0xfe2ce6e0);
  DO4(C,D,A,B,6,15,0xa3014314);
  DO4(B,C,D,A,13,21,0x4e0811a1);

  DO4(A,B,C,D,4,6,0xf7537e82);
  DO4(D,A,B,C,11,10,0xbd3af235);
  DO4(C,D,A,B,2,15,0x2ad7d2bb);
  DO4(B,C,D,A,9,21,0xeb86d391);

  A += AA;
  B += BB;
  C += CC;
  D += DD;
}

/*
 * From `Performance analysis of MD5' by Joseph D. Touch <touch@isi.edu>
 */
#if !defined(__BYTE_ORDER) || !defined (__BIG_ENDIAN)
#error __BYTE_ORDER macros not defined
#endif

#if __BYTE_ORDER == __BIG_ENDIAN
static inline u_int32_t
swap_u_int32_t (u_int32_t t)
{
  u_int32_t temp1, temp2;

  temp1   = cshift(t, 16);
  temp2   = temp1 >> 8;
  temp1  &= 0x00ff00ff;
  temp2  &= 0x00ff00ff;
  temp1 <<= 8;
  return temp1 | temp2;
}
#endif

struct x32{
  unsigned int a:32;
  unsigned int b:32;
};

void
MD5_Update (struct md5 *m, const void *v, size_t len)
{
  const unsigned char *p = v;
  size_t old_sz = m->sz[0];
  size_t offset;

  m->sz[0] += len * 8;
  if (m->sz[0] < old_sz)
      ++m->sz[1];
  offset = (old_sz / 8)  % 64;
  while(len > 0){
    size_t l = min(len, 64 - offset);
    memcpy(m->save + offset, p, l);
    offset += l;
    p += l;
    len -= l;
    if(offset == 64){
#if __BYTE_ORDER == __BIG_ENDIAN
      int i;
      u_int32_t current[16];
      struct x32 *u = (struct x32*)m->save;
      for(i = 0; i < 8; i++){
	current[2*i+0] = swap_u_int32_t(u[i].a);
	current[2*i+1] = swap_u_int32_t(u[i].b);
      }
      calc(m, current);
#else
      calc(m, (u_int32_t*)m->save);
#endif
      offset = 0;
    }
  }
}

void
MD5_Final (void *res, struct md5 *m)
{
  unsigned char zeros[72];
  unsigned offset = (m->sz[0] / 8) % 64;
  unsigned int dstart = (120 - offset - 1) % 64 + 1;

  *zeros = 0x80;
  memset (zeros + 1, 0, sizeof(zeros) - 1);
  zeros[dstart+0] = (m->sz[0] >> 0) & 0xff;
  zeros[dstart+1] = (m->sz[0] >> 8) & 0xff;
  zeros[dstart+2] = (m->sz[0] >> 16) & 0xff;
  zeros[dstart+3] = (m->sz[0] >> 24) & 0xff;
  zeros[dstart+4] = (m->sz[1] >> 0) & 0xff;
  zeros[dstart+5] = (m->sz[1] >> 8) & 0xff;
  zeros[dstart+6] = (m->sz[1] >> 16) & 0xff;
  zeros[dstart+7] = (m->sz[1] >> 24) & 0xff;
  MD5_Update (m, zeros, dstart + 8);
  {
      int i;
      unsigned char *r = (unsigned char *)res;

      for (i = 0; i < 4; ++i) {
	  r[4*i]   = m->counter[i] & 0xFF;
	  r[4*i+1] = (m->counter[i] >> 8) & 0xFF;
	  r[4*i+2] = (m->counter[i] >> 16) & 0xFF;
	  r[4*i+3] = (m->counter[i] >> 24) & 0xFF;
      }
  }
}

