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

#define LOG_TAG "AudioPolicyJordan"
#define LOG_NDEBUG 0
#include <utils/Log.h>

#include "AudioPolicyManager.h"
#include <media/mediarecorder.h>

namespace android_audio_legacy {



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

AudioPolicyManagerBase::routing_strategy AudioPolicyManager::getStrategy(
        AudioSystem::stream_type stream) {
    // stream to strategy mapping
    switch (stream) {
    case AudioSystem::VOICE_CALL:
    case AudioSystem::BLUETOOTH_SCO:
        return STRATEGY_PHONE;
    case AudioSystem::RING:
    case AudioSystem::NOTIFICATION:
    case AudioSystem::ALARM:
        return STRATEGY_SONIFICATION;
    case AudioSystem::DTMF:
        return STRATEGY_DTMF;
    case AudioSystem::SYSTEM:
        // NOTE: SYSTEM stream uses MEDIA strategy because muting music and switching outputs
        // while key clicks are played produces a poor result
#ifdef HAVE_FM_RADIO
    case AudioSystem::FM:
#else
    case 0x0A:
#endif
    case AudioSystem::TTS:
    case AudioSystem::MUSIC:
        return STRATEGY_MEDIA;
    case AudioSystem::ENFORCED_AUDIBLE:
        return STRATEGY_ENFORCED_AUDIBLE;

    default:
        ALOGE("unknown stream type 0x%x", (uint32_t) stream);
        ALOGI("known ones :");
        ALOGI(" %x VOICE_CALL", AudioSystem::VOICE_CALL);
        ALOGI(" %x BLUETOOTH_SCO", AudioSystem::BLUETOOTH_SCO);
        ALOGI(" %x RING", AudioSystem::RING);
        ALOGI(" %x NOTIFICATION", AudioSystem::NOTIFICATION);
        ALOGI(" %x ALARM", AudioSystem::ALARM);
        ALOGI(" %x DTMF", AudioSystem::DTMF);
        ALOGI(" %x SYSTEM", AudioSystem::SYSTEM);
        ALOGI(" %x TTS", AudioSystem::TTS);
        ALOGI(" %x MUSIC", AudioSystem::MUSIC);
        ALOGI(" %x ENFORCED_AUDIBLE", AudioSystem::ENFORCED_AUDIBLE);
        return STRATEGY_MEDIA;
    }
}


}; // namespace android_audio_legacy
