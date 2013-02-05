/*
 * Copyright (C) 2011 The Android Open Source Project
 * Copyright (C) 2012 Wolfson Microelectronics plc
 * Copyright (C) 2012 The CyanogenMod Project
 *               Daniel Hillenbrand <codeworkx@cyanogenmod.com>
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

#define PORT_PLAYBACK 0
#define PORT_MODEM    1
#define PORT_BT       3
#define PORT_CAPTURE  1

#define PLAYBACK_PERIOD_SIZE  880
#define PLAYBACK_PERIOD_COUNT 8

#define CAPTURE_PERIOD_SIZE   1056
#define CAPTURE_PERIOD_COUNT  2

/* minimum sleep time in out_write() when write threshold is not reached */
#define MIN_WRITE_SLEEP_US 5000

#define RESAMPLER_BUFFER_FRAMES (PLAYBACK_PERIOD_SIZE * 2)
#define RESAMPLER_BUFFER_SIZE (4 * RESAMPLER_BUFFER_FRAMES)

#define DEFAULT_OUT_SAMPLING_RATE 44100
#define DEFAULT_IN_SAMPLING_RATE 44100

/* sampling rate when using VX port for narrow band */
#define VX_NB_SAMPLING_RATE 8000
/* sampling rate when using VX port for wide band */
#define VX_WB_SAMPLING_RATE 16000

/* product-specific defines */
#define PRODUCT_DEVICE_PROPERTY "ro.product.device"
#define PRODUCT_NAME_PROPERTY   "ro.product.name"

enum tty_modes {
    TTY_MODE_OFF,
    TTY_MODE_VCO,
    TTY_MODE_HCO,
    TTY_MODE_FULL
};

struct route_setting
{
    char *ctl_name;
    int intval;
    char *strval;
};


struct route_setting mm_default[] = {
    { .ctl_name = "LDSPLDAC Switch", .intval = 1, },
    { .ctl_name = "CPCAP Mixer Stereo DAC", .intval = 1, },
    { .ctl_name = "STDac Volume", .strval = "15", },
    { .ctl_name = "DAI Mode", .strval = "Audio", },
    { .ctl_name = NULL, },
};

struct route_setting voicecall_daimode[] = {
    { .ctl_name = "DAI Mode", .strval = "Voice Call Handset", },
    { .ctl_name = NULL, },
};

struct route_setting voicecall_default[] = {
    { .ctl_name = "Codec Volume", .intval = 15, },
    { .ctl_name = "MIC1 Gain", .intval = 31, },
    { .ctl_name = "MIC2 Gain", .intval = 31, },
    { .ctl_name = "CPCAP Mixer Voice Codec", .intval = 1, },
    { .ctl_name = "EPCDC Switch", .intval = 1, },
    { .ctl_name = "Analog Left", .strval = "Mic2", },
    { .ctl_name = "Analog Right", .strval = "Mic1", },
    { .ctl_name = "DAI Mode", .strval = "Voice Call Handset", },
    { .ctl_name = NULL, },
};

struct route_setting voicecall_default_disable[] = {
    { .ctl_name = "Codec Volume", .intval = 0, },
    { .ctl_name = "MIC1 Gain", .intval = 0, },
    { .ctl_name = "MIC2 Gain", .intval = 0, },
    { .ctl_name = "CPCAP Mixer Voice Codec", .intval = 0, },
    { .ctl_name = "EPCDC Switch", .intval = 0, },
    { .ctl_name = "Analog Left", .strval = "Off", },
    { .ctl_name = "Analog Right", .strval = "Off", },
    { .ctl_name = "DAI Mode", .strval = "Audio", },
    { .ctl_name = NULL, },
};

struct route_setting default_input[] = {
    { .ctl_name = NULL, },
};

struct route_setting default_input_disable[] = {
    { .ctl_name = NULL, },
};

struct route_setting headset_input[] = {

    { .ctl_name = NULL, },
};

struct route_setting headset_input_disable[] = {
    { .ctl_name = NULL, },
};

struct route_setting bt_output[] = {
    { .ctl_name = NULL, },
};

struct route_setting bt_input[] = {
    { .ctl_name = NULL, },
};
