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

#define LOG_TAG "audio_hw_primary"
//#define LOG_NDEBUG 0

#include <stdlib.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <utils/Log.h>
#include <string.h>
#include <system/audio.h>
#include <hardware/audio.h>

#include "ril_interface.h"
 
void ril_config();

static int netmux_fd = -1;

/* Function pointers */

int ril_open()
{    
    ALOGD("Open Audio Netmux");

    netmux_fd = open(RIL_NETMUX_AUDIO_PATH, O_RDWR | O_NONBLOCK);

    if (netmux_fd <= 0) {
     	ALOGE("%s: failed, errno=%d\n", __func__, errno);
	return errno;
    }

    ALOGI("Netmux Opened!");

    ril_config();

    return 0;
    
}

void ril_config()
{    
    int ret;

    unsigned char codec_req[] = "\x00\x00\x00\x11\x00\x00\x00\x00\x00\x00\x00\x09\x02\x00\x0F\x00\x04\x00\x00\x00\x01";
    unsigned char request_msg_ack[] ="\x00\x00\x00\x0F\x00\x00\x00\x00\x00\x00\x00\x1B\x02\x00\x0D\x00\x04\x00\x00\x80\x09\x02\x00\x0D\x01\x04\x00\x00\x00\x01\x02\x00\x0D\x02\x04\x00\x00\x00\x00";
    unsigned char playback_mix_req[] = "\x00\x00\x00\x0A\x00\x00\x00\x00\x00\x00\x00\x12\x02\x00\x09\x00\x04\x00\x00\x00\x00\x02\x00\x09\x01\x04\x00\x00\x00\x01";
  
    ALOGI("codec_req msg, ID=11");
    ret = write(netmux_fd, &codec_req, sizeof(codec_req)-1);
    if (ret < 0) {
    	ALOGE("codec_req error: %d", ret);
    }

    ALOGI("request_msg_ack, ID=15");
    ret = write(netmux_fd, &request_msg_ack, sizeof(request_msg_ack)-1);
    if (ret < 0) {
    	ALOGE("request_msg_ack error: %d", ret);
    }

    ALOGI("playback_mix_req, ID=10");
    ret = write(netmux_fd, &playback_mix_req, sizeof(playback_mix_req)-1);
    if (ret < 0) {
    	ALOGE("playback_mix_req error: %d", ret);
    }

}

void ril_close()
{
    if (netmux_fd < 0) {
        ALOGE("Netmux already closed!");
        return;
    }
    	ALOGI("Netmux closed");

	close(netmux_fd);
}

/* AUDMGR_setMuteState called directly by libril-moto-umts-1.so */
/* TODO: Need share netmux audio device with other processes*/
int AUDMGR_setMuteState(int mic_spkr, int mute_state)
{
    int ret;

    unsigned char am_aipcm_mic_mute_req[] = "\x00\x00\x00\x05\x00\x00\x00\x00\x00\x00\x00\x12\x02\x00\x03\x01\x04\x00\x00\x00\x01\x02\x00\x03\x00\x04\x00\x00\x00\x00";
    ALOGI("am_aipcm_mic_mute_req, ID=5; mic_spkr=%d, mute_state=%d", mic_spkr, mute_state);
    am_aipcm_mic_mute_req[28] = mute_state;
    if (netmux_fd > 0) {
        ret = write(netmux_fd, &am_aipcm_mic_mute_req, sizeof(am_aipcm_mic_mute_req)-1);
        if (ret < 0) {
            ALOGE("playback_mix_req error: %d", ret);
        }
    }

    return mute_state;
}

int ril_set_call_volume(float volume)
{
    
    int ret;

    unsigned char set_volume_req[] = "\x00\x00\x00\x03\x00\x00\x00\x00\x00\x00\x00\x12\x02\x00\x02\x00\x04\x00\x00\x00\x07\x02\x00\x02\x01\x04\x00\x00\x00\x02";

    int vol = 0;

    if (volume == (float) 0.0) {
        vol = 0;
    } else if (volume == (float) 0.2) {
        vol = 3;
    } else if (volume == (float) 0.4) {
        vol = 4;
    } else if (volume == (float) 0.6) {
        vol = 5;
    } else if (volume == (float) 0.8) {
        vol = 6;
    } else if (volume == (float) 1.0) {
        vol = 7;
    }

    set_volume_req[20] = vol;

    if (netmux_fd > 0) {
        ret = write(netmux_fd, &set_volume_req, sizeof(set_volume_req)-1);
        if (ret < 0) {
            ALOGE("Write Volume Error, netmux closed? %d", ret);
            return -1;
        }
    }

    return 0;
}
/* Crystal Talk */
int ril_set_voice_quality(int voice_type)
{

/*
Normal:00000002000000000000001B020005000400000009020005010400000003020005020400000001
clear: 00000002000000000000001B020005000400000009020005010400000003020005020400000002
crisp: 00000002000000000000001B020005000400000009020005010400000003020005020400000003
bright:00000002000000000000001B020005000400000009020005010400000003020005020400000004
*/

    unsigned char set_voice_quality[] = "\x00\x00\x00\x02\x00\x00\x00\x00\x00\x00\x00\x1B\x02\x00\x05\x00\x04\x00\x00\x00\x09\x02\x00\x05\x01\x04\x00\x00\x00\x03\x02\x00\x05\x02\x04\x00\x00\x00\x01";
    int ret;

    if (voice_type > 0)
        set_voice_quality[38] = voice_type;

    if (netmux_fd > 0) {
        ret = write(netmux_fd, &set_voice_quality, sizeof(set_voice_quality)-1);
        if (ret < 0) {
            ALOGE("Set Voice Quality Error, netmux closed? %d", ret);
            return -1;
        }
    }
    return 0;
}


/* Stubs for Motorola RIL lib */

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
/* End of stubs */
