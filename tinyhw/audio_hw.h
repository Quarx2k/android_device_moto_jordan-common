/*
 * Copyright (C) 2011 The Android Open Source Project
 * Copyright (C) 2012 Wolfson Microelectronics plc
 * Copyright (C) 2012 The CyanogenMod Project
 *               Daniel Hillenbrand <codeworkx@cyanogenmod.com>
 *               Guillaume "XpLoDWilD" Lesniak <xplodgui@gmail.com>
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

/* ALSA cards for WM1811 */
#define CARD_DEFAULT  0

#define PORT_PLAYBACK    0 // Music.
#define PORT_CAPTURE     1 // Mic
#define PORT_MODEM       2 // Voice stream   
#define PORT_FM          3 // FM -Radio
#define PORT_BT          4 // BT-SOC
#define PORT_BT_CAPTURE  5 // BT-SOC mic
#define PORT_BP          6 // ???

#define PCM_WRITE pcm_write

#define PLAYBACK_PERIOD_SIZE  880
#define PLAYBACK_PERIOD_COUNT 8
#define PLAYBACK_SHORT_PERIOD_COUNT 2

#define CAPTURE_PERIOD_SIZE   1024
#define CAPTURE_PERIOD_COUNT  4

#define SHORT_PERIOD_SIZE 192

//
// deep buffer
//
/* screen on */
#define DEEP_BUFFER_SHORT_PERIOD_SIZE 1056
#define PLAYBACK_DEEP_BUFFER_SHORT_PERIOD_COUNT 4
/* screen off */
#define DEEP_BUFFER_LONG_PERIOD_SIZE 880
#define PLAYBACK_DEEP_BUFFER_LONG_PERIOD_COUNT 8

/* minimum sleep time in out_write() when write threshold is not reached */
#define MIN_WRITE_SLEEP_US 5000

#define RESAMPLER_BUFFER_FRAMES (PLAYBACK_PERIOD_SIZE * 2)
#define RESAMPLER_BUFFER_SIZE (4 * RESAMPLER_BUFFER_FRAMES)

#define DEFAULT_OUT_SAMPLING_RATE 44100
#define MM_LOW_POWER_SAMPLING_RATE 44100
#define MM_FULL_POWER_SAMPLING_RATE 44100
#define DEFAULT_IN_SAMPLING_RATE 44100

/* sampling rate when using VX port for narrow band */
#define VX_NB_SAMPLING_RATE 8000
/* sampling rate when using VX port for wide band */
#define VX_WB_SAMPLING_RATE 16000

/* product-specific defines */
#define PRODUCT_DEVICE_PROPERTY "ro.product.device"
#define PRODUCT_NAME_PROPERTY   "ro.product.name"

#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))

#define STRING_TO_ENUM(string) { #string, string }

struct string_to_enum {
    const char *name;
    uint32_t value;
};

const struct string_to_enum out_channels_name_to_enum_table[] = {
    STRING_TO_ENUM(AUDIO_CHANNEL_OUT_STEREO),
    STRING_TO_ENUM(AUDIO_CHANNEL_OUT_5POINT1),
    STRING_TO_ENUM(AUDIO_CHANNEL_OUT_7POINT1),
};

enum pcm_type {
    PCM_NORMAL = 0,
    PCM_SPDIF,
    PCM_HDMI,
    PCM_TOTAL,
};

enum output_type {
    OUTPUT_DEEP_BUF,      // deep PCM buffers output stream
    OUTPUT_LOW_LATENCY,   // low latency output stream
    OUTPUT_HDMI,
    OUTPUT_TOTAL
};

enum tty_modes {
    TTY_MODE_OFF,
    TTY_MODE_VCO,
    TTY_MODE_HCO,
    TTY_MODE_FULL
};

struct mixer_ctls
{
    struct mixer_ctl *mixinl_in1l_volume;
    struct mixer_ctl *mixinl_in2l_volume;
};

struct route_setting
{
    char *ctl_name;
    int intval;
    char *strval;
};


/* DAC - CPCAP Mixer Stereo DAC - STDac Volume*/
/* CDC - CPCAP Mixer Voice Code - Codec Volume*/

struct route_setting voicecall_earpice[] = {
    { .ctl_name = "Codec Volume", .intval = 15, },
    { .ctl_name = "CPCAP Mixer Voice Codec", .intval = 1, },
    { .ctl_name = "LDSPLCDC Switch", .intval = 0, },
    { .ctl_name = "EPCDC Switch", .intval = 1, },
    { .ctl_name = "DAI Mode", .strval = "Audio", },
    { .ctl_name = NULL, },
};

struct route_setting voicecall_speaker[] = {
    { .ctl_name = "Codec Volume", .intval = 15, },
    { .ctl_name = "CPCAP Mixer Voice Codec", .intval = 1, },
    { .ctl_name = "EPCDC Switch", .intval = 0, },
    { .ctl_name = "HSRCDC Switch", .intval = 0, },
    { .ctl_name = "HSLCDC Switch", .intval = 0, },
    { .ctl_name = "LDSPLCDC Switch", .intval = 1, },
    { .ctl_name = "DAI Mode", .strval = "Audio", },  
    { .ctl_name = NULL, },
};
struct route_setting voicecall_headset[] = {
    { .ctl_name = "Codec Volume", .intval = 15, },
    { .ctl_name = "CPCAP Mixer Voice Codec", .intval = 1, },
    { .ctl_name = "LDSPLCDC Switch", .intval = 0, },
    { .ctl_name = "EPCDC Switch", .intval = 0, },
    { .ctl_name = "HSRCDC Switch", .intval = 1, },
    { .ctl_name = "HSLCDC Switch", .intval = 1, },
    { .ctl_name = "DAI Mode", .strval = "Audio", },  
    { .ctl_name = NULL, },
};
struct route_setting voicecall_default_disable[] = {
    { .ctl_name = "DAI Mode", .strval = "Audio", },
    { .ctl_name = "CPCAP Mixer Voice Codec", .intval = 0, },
    { .ctl_name = "LDSPLCDC Switch", .intval = 0, },
    { .ctl_name = "EPCDC Switch", .intval = 0, },
    { .ctl_name = "HSRCDC Switch", .intval = 0, },
    { .ctl_name = "HSLCDC Switch", .intval = 0, },
    { .ctl_name = NULL, },
};

struct route_setting earpice_input[] = {
    { .ctl_name = "Analog Left Capture Route", .strval = "Mic2", },
    { .ctl_name = "Analog Right Capture Route", .strval = "Mic1", },
    { .ctl_name = "MIC1 Gain", .intval = 31, },
    { .ctl_name = "MIC2 Gain", .intval = 31, },
    { .ctl_name = NULL, },
};

struct route_setting speaker_input[] = {
    { .ctl_name = "Analog Left Capture Route", .strval = "Off", }, //Mic2 cays whistle.
    { .ctl_name = "Analog Right Capture Route", .strval = "Mic1", },
    { .ctl_name = "MIC1 Gain", .intval = 0, },
    { .ctl_name = "MIC2 Gain", .intval = 31, },
    { .ctl_name = NULL, },
};

struct route_setting default_input_disable[] = {
    { .ctl_name = "Analog Left Capture Route", .strval = "Off", },
    { .ctl_name = "Analog Right Capture Route", .strval = "Off", },
    { .ctl_name = "MIC1 Gain", .intval = 0, },
    { .ctl_name = "MIC2 Gain", .intval = 0, },
    { .ctl_name = NULL, },
};

struct route_setting headset_input[] = {
    { .ctl_name = "Analog Left Capture Route", .strval = "Off", },
    { .ctl_name = "Analog Right Capture Route", .strval = "HS Mic", },
    { .ctl_name = "MIC1 Gain", .intval = 20, },
    { .ctl_name = "MIC2 Gain", .intval = 0, },
    { .ctl_name = NULL, },
};

struct route_setting headset_input_disable[] = {
    { .ctl_name = "Analog Left Capture Route", .strval = "Off", },
    { .ctl_name = "Analog Right Capture Route", .strval = "Off", },
    { .ctl_name = "MIC1 Gain", .intval = 0, },
    { .ctl_name = "MIC2 Gain", .intval = 0, },
    { .ctl_name = NULL, },
};

struct route_setting voicecall_bluetooth[] = {
    { .ctl_name = "DAI Mode", .strval = "Voice Call BT", },
    { .ctl_name = "CPCAP Mixer Voice Codec", .intval = 1, },
    { .ctl_name = "LDSPLCDC Switch", .intval = 0, },
    { .ctl_name = "EPCDC Switch", .intval = 0, },
    { .ctl_name = "HSRCDC Switch", .intval = 0, },
    { .ctl_name = "HSLCDC Switch", .intval = 0, },
    { .ctl_name = "Analog Left Capture Route", .strval = "Off", },
    { .ctl_name = "Analog Right Capture Route", .strval = "Off", },
    { .ctl_name = "MIC1 Gain", .intval = 0, },
    { .ctl_name = "MIC2 Gain", .intval = 0, },
    { .ctl_name = NULL, },
};

struct route_setting bt_input[] = {
    { .ctl_name = NULL, },
};

struct route_setting bt_disable[] = {
    { .ctl_name = NULL, },
};

