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

#ifndef RIL_INTERFACE_H
#define RIL_INTERFACE_H

#define RIL_NETMUX_AUDIO_PATH "/dev/netmux/audio"

/* Voice quality */
#define NORMAL_VOICE 1
#define CLEAR_VOICE 2
#define CRISP_VOICE 3
#define BRIGHT_VOICE 4

/* Function prototypes */
int ril_open();
void ril_close();
int ril_set_call_volume(float volume);
int ril_set_voice_quality(int voice_type);

#endif

