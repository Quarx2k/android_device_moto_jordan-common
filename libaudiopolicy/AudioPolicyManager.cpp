/*
 * Copyright (C) 2009 The Android Open Source Project
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

#define LOG_TAG "AudioPolicyManager"
//#define LOG_NDEBUG 0
#include <utils/Log.h>
#include "AudioPolicyManager.h"
#include <media/mediarecorder.h>

namespace android {


// ----------------------------------------------------------------------------
// AudioPolicyManager for jordan platform
// Common audio policy manager code is implemented in AudioPolicyManagerBase class
// ----------------------------------------------------------------------------

// ---  class factory


extern "C" AudioPolicyInterface* createAudioPolicyManager(AudioPolicyClientInterface *clientInterface)
{
    return new AudioPolicyManager(clientInterface);
}

extern "C" void destroyAudioPolicyManager(AudioPolicyInterface *interface)
{
    delete interface;
}


#ifdef HAVE_FM_RADIO

status_t AudioPolicyManager::setDeviceConnectionState(AudioSystem::audio_devices device,
                                                  AudioSystem::device_connection_state state,
                                                  const char *device_address)
{
    LOGW("setDeviceConnectionState() : device=%x state=%x (%s)", device, state, device_address);

    if(!strcmp(device_address,"fm_reset") && AudioSystem::isFmDevice(device)) {

        if(state == AudioSystem::DEVICE_STATE_AVAILABLE) {
            resetFm(device);
        }
        return NO_ERROR;
    }

    //to check, static call or object instance ?
    return AudioPolicyManagerBase::setDeviceConnectionState(device, state, device_address);
}

void AudioPolicyManager::resetFm(AudioSystem::audio_devices device)
{
    LOGW("resetFm() device=%x", device);

    mAvailableOutputDevices |= device;
    AudioOutputDescriptor *hwOutputDesc = mOutputs.valueFor(mHardwareOutput);
    hwOutputDesc->mRefCount[AudioSystem::FM] = 1;

    mpClientInterface->setParameters(mHardwareOutput, String8("FM_launch=off"));

    //to implement...
    long wantedRouting = AudioSystem::DEVICE_OUT_ALL;

    String8 routing;
    switch (wantedRouting)
    {
      case AudioSystem::DEVICE_OUT_ALL:
          routing = String8("FM_routing=DEVICE_OUT_ALL");
          break;
      case AudioSystem::DEVICE_OUT_EARPIECE:
          routing = String8("FM_routing=DEVICE_OUT_EARPIECE");
          break;
      case AudioSystem::DEVICE_OUT_SPEAKER:
          routing = String8("FM_routing=DEVICE_OUT_SPEAKER");
          break;
      case AudioSystem::DEVICE_OUT_WIRED_HEADPHONE:
          routing = String8("FM_routing=DEVICE_OUT_WIRED_HEADPHONE");
          break;
      case AudioSystem::DEVICE_OUT_WIRED_HEADSET:
          routing = String8("FM_routing=DEVICE_OUT_WIRED_HEADSET");
          break;
      case AudioSystem::DEVICE_OUT_ALL_A2DP:
          routing = String8("FM_routing=DEVICE_OUT_ALL_A2DP");
          break;
      default:
          routing = String8("FM_routing=DEVICE_OUT_ALL");
    }
    mpClientInterface->setParameters(mHardwareOutput, routing);
    mpClientInterface->setParameters(mHardwareOutput, String8("FM_launch=on"));

    uint32_t newDevice = getNewDevice(mHardwareOutput, false);
    updateDeviceForStrategy();
    setOutputDevice(mHardwareOutput, newDevice);
}

#endif



#ifdef OMAP_ENHANCEMENT
status_t AudioPolicyManager::setFMRxActive(bool status) {
    return NO_ERROR;
}
#endif


}; // namespace android
