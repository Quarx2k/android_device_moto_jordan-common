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

#define LOG_TAG "audio_hw_primary"
//#define LOG_NDEBUG 0

#include <errno.h>
#include <pthread.h>
#include <stdint.h>
#include <sys/time.h>
#include <stdlib.h>
#include <expat.h>
#include <fcntl.h>
#include <sys/ioctl.h>

#include <cutils/log.h>
#include <cutils/str_parms.h>
#include <cutils/properties.h>

#include <hardware/hardware.h>
#include <system/audio.h>
#include <hardware/audio.h>

#include <tinyalsa/asoundlib.h>
#include <audio_utils/resampler.h>
#include <audio_utils/echo_reference.h>
#include <hardware/audio_effect.h>
#include <audio_effects/effect_aec.h>

#include "audio_hw.h"
#include "ril_interface.h"

struct pcm_config pcm_config_mm = {
    .channels = 2,
    .rate = MM_FULL_POWER_SAMPLING_RATE,
    .period_size = DEEP_BUFFER_LONG_PERIOD_SIZE,
    .period_count = PLAYBACK_DEEP_BUFFER_LONG_PERIOD_COUNT,
    .format = PCM_FORMAT_S16_LE,
};

struct pcm_config pcm_config_tones = {
    .channels = 2,
    .rate = MM_FULL_POWER_SAMPLING_RATE,
    .period_size = SHORT_PERIOD_SIZE,
    .period_count = PLAYBACK_SHORT_PERIOD_COUNT,
    .format = PCM_FORMAT_S16_LE,
    .start_threshold = 0,
    .avail_min = 0,
};

struct pcm_config pcm_config_capture = {
    .channels = 2,
    .rate = DEFAULT_IN_SAMPLING_RATE,
    .period_size = CAPTURE_PERIOD_SIZE,
    .period_count = CAPTURE_PERIOD_COUNT,
    .format = PCM_FORMAT_S16_LE,
};

struct pcm_config pcm_config_vx = {
    .channels = 2,
    .rate = VX_NB_SAMPLING_RATE,
    .period_size = 160,
    .period_count = 2,
    .format = PCM_FORMAT_S16_LE,
};

#define MIN(x, y) ((x) > (y) ? (y) : (x))

struct m0_audio_device {
    struct audio_hw_device hw_device;

    pthread_mutex_t lock;       /* see note below on mutex acquisition order */
    struct m0_dev_cfg *dev_cfgs;
    int num_dev_cfgs;
    struct mixer *mixer;
    struct mixer_ctls mixer_ctls;
    audio_mode_t mode;
    int active_out_device;
    int out_device;
    int active_in_device;
    int in_device;
    struct pcm *pcm_modem_dl;
    struct pcm *pcm_modem_ul;
    int in_call;
    float voice_volume;
    struct m0_stream_in *active_input;
    struct m0_stream_out *outputs[OUTPUT_TOTAL];
    bool mic_mute;
    int tty_mode;
    struct echo_reference_itfe *echo_reference;
    bool bluetooth_nrec;
    int wb_amr;
    bool screen_off;
};

struct m0_stream_out {
    struct audio_stream_out stream;

    pthread_mutex_t lock;       /* see note below on mutex acquisition order */
    struct pcm_config config[PCM_TOTAL];
    struct pcm *pcm[PCM_TOTAL];
    struct resampler_itfe *resampler;
    char *buffer;
    size_t buffer_frames;
    int standby;
    struct echo_reference_itfe *echo_reference;
    int write_threshold;
    bool use_long_periods;
    audio_channel_mask_t channel_mask;
    audio_channel_mask_t sup_channel_masks[3];

    struct m0_audio_device *dev;
};

#define MAX_PREPROCESSORS 3 /* maximum one AGC + one NS + one AEC per input stream */

struct effect_info_s {
    effect_handle_t effect_itfe;
    size_t num_channel_configs;
    channel_config_t* channel_configs;
};

#define NUM_IN_AUX_CNL_CONFIGS 2
channel_config_t in_aux_cnl_configs[NUM_IN_AUX_CNL_CONFIGS] = {
    { AUDIO_CHANNEL_IN_FRONT , AUDIO_CHANNEL_IN_BACK},
    { AUDIO_CHANNEL_IN_STEREO , AUDIO_CHANNEL_IN_RIGHT}
};

struct m0_stream_in {
    struct audio_stream_in stream;

    pthread_mutex_t lock;       /* see note below on mutex acquisition order */
    struct pcm_config config;
    struct pcm *pcm;
    int device;
    struct resampler_itfe *resampler;
    struct resampler_buffer_provider buf_provider;
    unsigned int requested_rate;
    int standby;
    int source;
    struct echo_reference_itfe *echo_reference;
    bool need_echo_reference;

    int16_t *read_buf;
    size_t read_buf_size;
    size_t read_buf_frames;

    int16_t *proc_buf_in;
    int16_t *proc_buf_out;
    size_t proc_buf_size;
    size_t proc_buf_frames;

    int16_t *ref_buf;
    size_t ref_buf_size;
    size_t ref_buf_frames;

    int read_status;

    int num_preprocessors;
    struct effect_info_s preprocessors[MAX_PREPROCESSORS];

    bool aux_channels_changed;
    uint32_t main_channels;
    uint32_t aux_channels;
    struct m0_audio_device *dev;
};

struct m0_dev_cfg {
    int mask;

    struct route_setting *on;
    unsigned int on_len;

    struct route_setting *off;
    unsigned int off_len;
};

/**
 * NOTE: when multiple mutexes have to be acquired, always respect the following order:
 *        hw device > in stream > out stream
 */

static void select_output_device(struct m0_audio_device *adev);
static void select_input_device(struct m0_audio_device *adev);
static int adev_set_voice_volume(struct audio_hw_device *dev, float volume);
static int do_input_standby(struct m0_stream_in *in);
static int do_output_standby(struct m0_stream_out *out);
static void in_update_aux_channels(struct m0_stream_in *in, effect_handle_t effect);

/* The enable flag when 0 makes the assumption that enums are disabled by
 * "Off" and integers/booleans by 0 */
static int set_voicecall_route_by_array(struct mixer *mixer, struct route_setting *route,
                              int enable)
{
    struct mixer_ctl *ctl;
    unsigned int i, j, ret;

    /* Go through the route array and set each value */
    i = 0;
    while (route[i].ctl_name) {
        ctl = mixer_get_ctl_by_name(mixer, route[i].ctl_name);
        if (!ctl) {
        ALOGE("Unknown control '%s'\n", route[i].ctl_name);
            return -EINVAL;
        }

        if (route[i].strval) {
            if (enable) {
                ret = mixer_ctl_set_enum_by_string(ctl, route[i].strval);
                if (ret != 0) {
                    ALOGE("Failed to set '%s' to '%s'\n", route[i].ctl_name, route[i].strval);
                } else {
                    ALOGV("Set '%s' to '%s'\n", route[i].ctl_name, route[i].strval);
                }
            } else {
                ret = mixer_ctl_set_enum_by_string(ctl, "Off");
                if (ret != 0) {
                    ALOGE("Failed to set '%s' to '%s'\n", route[i].ctl_name, route[i].strval);
                } else {
                    ALOGV("Set '%s' to '%s'\n", route[i].ctl_name, "Off");
                }
            }
        } else {
            /* This ensures multiple (i.e. stereo) values are set jointly */
            for (j = 0; j < mixer_ctl_get_num_values(ctl); j++) {
                if (enable) {
                    ret = mixer_ctl_set_value(ctl, j, route[i].intval);
                    if (ret != 0) {
                        ALOGE("Failed to set '%s' to '%d'\n", route[i].ctl_name, route[i].intval);
                    } else {
                        ALOGV("Set '%s' to '%d'\n", route[i].ctl_name, route[i].intval);
                    }
                } else {
                    ret = mixer_ctl_set_value(ctl, j, 0);
                    if (ret != 0) {
                        ALOGE("Failed to set '%s' to '%d'\n", route[i].ctl_name, route[i].intval);
                    } else {
                        ALOGV("Set '%s' to '%d'\n", route[i].ctl_name, 0);
                    }
                }
            }
        }
        i++;
    }

    return 0;
}

/* The enable flag when 0 makes the assumption that enums are disabled by
 * "Off" and integers/booleans by 0 */
static int set_route_by_array(struct mixer *mixer, struct route_setting *route,
                  unsigned int len)
{
    struct mixer_ctl *ctl;
    unsigned int i, j, ret;

    /* Go through the route array and set each value */
    for (i = 0; i < len; i++) {
        ctl = mixer_get_ctl_by_name(mixer, route[i].ctl_name);
        if (!ctl) {
        ALOGE("Unknown control '%s'\n", route[i].ctl_name);
            return -EINVAL;
        }
        if (route[i].strval) {
        ret = mixer_ctl_set_enum_by_string(ctl, route[i].strval);
        if (ret != 0) {
        ALOGE("Failed to set '%s' to '%s'\n",
             route[i].ctl_name, route[i].strval);
        } else {
        ALOGV("Set '%s' to '%s'\n",
             route[i].ctl_name, route[i].strval);
        }

        } else {
            /* This ensures multiple (i.e. stereo) values are set jointly */
            for (j = 0; j < mixer_ctl_get_num_values(ctl); j++) {
        ret = mixer_ctl_set_value(ctl, j, route[i].intval);
        if (ret != 0) {
            ALOGE("Failed to set '%s'.%d to %d\n",
             route[i].ctl_name, j, route[i].intval);
        } else {
            ALOGV("Set '%s'.%d to %d\n",
             route[i].ctl_name, j, route[i].intval);
        }
        }
        }
    }

    return 0;
}

void update_devices(struct m0_audio_device *adev)
{
    int i;
    for (i = 0; i < adev->num_dev_cfgs; i++)
    {
   	 if ((adev->out_device & adev->dev_cfgs[i].mask) && (adev->active_out_device != 6) &&
            (adev->active_out_device & adev->dev_cfgs[i].mask)) 
	{
        	set_route_by_array(adev->mixer, adev->dev_cfgs[i].on,
        	           adev->dev_cfgs[i].on_len);

    	}
    }
}

/* Must be called with lock */
void select_devices(struct m0_audio_device *adev)
{
    int i;

    ALOGV("Changing output device %x => %x\n", adev->active_out_device, adev->out_device);
    ALOGV("Changing input device %x => %x\n", adev->active_in_device, adev->in_device);

    if (adev->active_out_device == adev->out_device && adev->active_in_device == adev->in_device)
    return;

    /* Turn on new devices first so we don't glitch due to powerdown... */
    for (i = 0; i < adev->num_dev_cfgs; i++)
    {
   	 if ((adev->out_device & adev->dev_cfgs[i].mask) &&
            !(adev->active_out_device & adev->dev_cfgs[i].mask)) 
  	{
        	set_route_by_array(adev->mixer, adev->dev_cfgs[i].on,
        	           adev->dev_cfgs[i].on_len);
    	}
    }

    for (i = 0; i < adev->num_dev_cfgs; i++)
    {
   	 if ((adev->in_device & adev->dev_cfgs[i].mask) &&
            !(adev->active_in_device & adev->dev_cfgs[i].mask))
  	{
        	set_route_by_array(adev->mixer, adev->dev_cfgs[i].on,
        	           adev->dev_cfgs[i].on_len);
    	}
    }

    /* ...then disable old ones. */
    for (i = 0; i < adev->num_dev_cfgs; i++)
    {
   	 if (!(adev->out_device & adev->dev_cfgs[i].mask) &&
            (adev->active_out_device & adev->dev_cfgs[i].mask))
  	{
        	set_route_by_array(adev->mixer, adev->dev_cfgs[i].off,
        	           adev->dev_cfgs[i].off_len);
        }
    }

    for (i = 0; i < adev->num_dev_cfgs; i++)
    {
   	 if (!(adev->in_device & adev->dev_cfgs[i].mask) &&
            (adev->active_in_device & adev->dev_cfgs[i].mask))
  	{
        	set_route_by_array(adev->mixer, adev->dev_cfgs[i].off,
        	           adev->dev_cfgs[i].off_len);
        }
    }

    adev->active_out_device = adev->out_device;
    adev->active_in_device = adev->in_device;
}

static int start_call(struct m0_audio_device *adev)
{
    int bt_on;

    bt_on = adev->out_device & AUDIO_DEVICE_OUT_ALL_SCO;
    pcm_config_vx.rate = adev->wb_amr ? VX_WB_SAMPLING_RATE : VX_NB_SAMPLING_RATE;

    /* Open Netmux Audio channel */
    if (ril_open() < 0)
        return -ENOMEM;

    /* Open modem PCM channels */
    if (adev->pcm_modem_dl == NULL) {
        if (bt_on)
            adev->pcm_modem_dl = pcm_open(CARD_DEFAULT, PORT_BT, PCM_OUT, &pcm_config_vx);
        else
            adev->pcm_modem_dl = pcm_open(CARD_DEFAULT, PORT_MODEM, PCM_OUT, &pcm_config_vx);
        if (!pcm_is_ready(adev->pcm_modem_dl)) {
            ALOGE("cannot open PCM modem DL stream: %s", pcm_get_error(adev->pcm_modem_dl));
            goto err_open_dl;
        }
    }
    if (adev->pcm_modem_ul == NULL) {
            adev->pcm_modem_ul = pcm_open(CARD_DEFAULT, PORT_MODEM, PCM_IN, &pcm_config_vx);
        if (!pcm_is_ready(adev->pcm_modem_ul)) {
            ALOGE("cannot open PCM modem UL stream: %s", pcm_get_error(adev->pcm_modem_ul));
            goto err_open_ul;
        }
    }

    pcm_start(adev->pcm_modem_dl);
    pcm_start(adev->pcm_modem_ul);


    return 0;

err_open_ul:
    pcm_close(adev->pcm_modem_ul);
    adev->pcm_modem_ul = NULL;
err_open_dl:
    pcm_close(adev->pcm_modem_dl);
    adev->pcm_modem_dl = NULL;

    return -ENOMEM;
}

static void end_call(struct m0_audio_device *adev)
{
    ALOGD("Closing modem PCMs");
    ril_close();
    pcm_stop(adev->pcm_modem_dl);
    pcm_stop(adev->pcm_modem_ul);
    pcm_close(adev->pcm_modem_dl);
    pcm_close(adev->pcm_modem_ul);
    adev->pcm_modem_dl = NULL;
    adev->pcm_modem_ul = NULL;
    
}

static void set_eq_filter(struct m0_audio_device *adev)
{
}

void audio_set_wb_amr_callback(void *data, int enable)
{
    struct m0_audio_device *adev = (struct m0_audio_device *)data;

    pthread_mutex_lock(&adev->lock);
    if (adev->wb_amr != enable) {
        adev->wb_amr = enable;

        /* reopen the modem PCMs at the new rate */
        if (adev->in_call) {
            end_call(adev);
            start_call(adev);
            select_output_device(adev);
        }
    }
    pthread_mutex_unlock(&adev->lock);
}

static void set_input_volumes(struct m0_audio_device *adev, int main_mic_on,
                              int headset_mic_on, int sub_mic_on)
{
}

static void set_output_volumes(struct m0_audio_device *adev, bool tty_volume)
{
}

static void force_all_standby(struct m0_audio_device *adev)
{
    struct m0_stream_in *in;
    struct m0_stream_out *out;

    /* only needed for low latency output streams as other streams are not used
     * for voice use cases */
    if (adev->outputs[OUTPUT_LOW_LATENCY] != NULL &&
            !adev->outputs[OUTPUT_LOW_LATENCY]->standby) {
        out = adev->outputs[OUTPUT_LOW_LATENCY];
        pthread_mutex_lock(&out->lock);
        do_output_standby(out);
        pthread_mutex_unlock(&out->lock);
    }

    if (adev->active_input) {
        in = adev->active_input;
        pthread_mutex_lock(&in->lock);
        do_input_standby(in);
        pthread_mutex_unlock(&in->lock);
    }
}

static void select_mode(struct m0_audio_device *adev)
{
    if (adev->mode == AUDIO_MODE_IN_CALL) {
        ALOGI("Entering IN_CALL state, in_call=%d", adev->in_call);
        if (!adev->in_call) {
            force_all_standby(adev);
            /* force earpiece route for in call state if speaker is the
            only currently selected route. This prevents having to tear
            down the modem PCMs to change route from speaker to earpiece
            after the ringtone is played, but doesn't cause a route
            change if a headset or bt device is already connected. If
            speaker is not the only thing active, just remove it from
            the route. We'll assume it'll never be used initally during
            a call. This works because we're sure that the audio policy
            manager will update the output device after the audio mode
            change, even if the device selection did not change. */
            if (adev->out_device == AUDIO_DEVICE_OUT_SPEAKER) {
                adev->out_device = AUDIO_DEVICE_OUT_EARPIECE;
                adev->in_device = AUDIO_DEVICE_IN_BUILTIN_MIC & ~AUDIO_DEVICE_BIT_IN;
            } else {
                adev->out_device &= ~AUDIO_DEVICE_OUT_SPEAKER;
            }
            start_call(adev);
            select_output_device(adev);
            adev_set_voice_volume(&adev->hw_device, adev->voice_volume);
            adev->in_call = 1;
        }
    } else {
        ALOGD("Leaving IN_CALL state, in_call=%d, mode=%d",
             adev->in_call, adev->mode);
        if (adev->in_call) {
            adev->in_call = 0;
            end_call(adev);
            force_all_standby(adev);

            ALOGD("%s: set voicecall route: voicecall_default_disable", __func__);
            set_voicecall_route_by_array(adev->mixer, voicecall_default_disable, 1);
            ALOGD("%s: set voicecall route: default_input_disable", __func__);
            set_voicecall_route_by_array(adev->mixer, default_input_disable, 1);
            ALOGD("%s: set voicecall route: headset_input_disable", __func__);
            set_voicecall_route_by_array(adev->mixer, headset_input_disable, 1);
            ALOGD("%s: set voicecall route: bt_disable", __func__);
            set_voicecall_route_by_array(adev->mixer, bt_disable, 1);

            select_output_device(adev);
            //Force Input Standby
            adev->in_device = AUDIO_DEVICE_NONE;
            select_input_device(adev);
        }
    }
}

static void select_output_device(struct m0_audio_device *adev)
{
    int headset_on;
    int headphone_on;
    int speaker_on;
    int earpiece_on;
    int bt_on;
    bool tty_volume = false;
    unsigned int channel;

    headset_on = adev->out_device & AUDIO_DEVICE_OUT_WIRED_HEADSET;
    headphone_on = adev->out_device & AUDIO_DEVICE_OUT_WIRED_HEADPHONE;
    speaker_on = adev->out_device & AUDIO_DEVICE_OUT_SPEAKER;
    earpiece_on = adev->out_device & AUDIO_DEVICE_OUT_EARPIECE;
    bt_on = adev->out_device & AUDIO_DEVICE_OUT_ALL_SCO;

    switch(adev->out_device) {
        case AUDIO_DEVICE_OUT_SPEAKER:
            ALOGD("%s: AUDIO_DEVICE_OUT_SPEAKER", __func__);
            break;
        case AUDIO_DEVICE_OUT_WIRED_HEADSET:
            ALOGD("%s: AUDIO_DEVICE_OUT_WIRED_HEADSET", __func__);
            break;
        case AUDIO_DEVICE_OUT_WIRED_HEADPHONE:
            ALOGD("%s: AUDIO_DEVICE_OUT_WIRED_HEADPHONE", __func__);
            break;
        case AUDIO_DEVICE_OUT_EARPIECE:
            ALOGD("%s: AUDIO_DEVICE_OUT_EARPIECE", __func__);
            break;
        case AUDIO_DEVICE_OUT_ANLG_DOCK_HEADSET:
            ALOGD("%s: AUDIO_DEVICE_OUT_ANLG_DOCK_HEADSET", __func__);
            break;
        case AUDIO_DEVICE_OUT_DGTL_DOCK_HEADSET:
            ALOGD("%s: AUDIO_DEVICE_OUT_DGTL_DOCK_HEADSET", __func__);
            break;
        case AUDIO_DEVICE_OUT_ALL_SCO:
            ALOGD("%s: AUDIO_DEVICE_OUT_ALL_SCO", __func__);
            break;
        case AUDIO_DEVICE_OUT_USB_ACCESSORY:
            ALOGD("%s: AUDIO_DEVICE_OUT_USB_ACCESSORY", __func__);
            break;
        case AUDIO_DEVICE_OUT_USB_DEVICE:
            ALOGD("%s: AUDIO_DEVICE_OUT_USB_DEVICE", __func__);
            break;
        default:
            ALOGD("%s: AUDIO_DEVICE_OUT_ALL", __func__);
            break;
    }

    select_devices(adev);
    update_devices(adev);
    set_eq_filter(adev);

    if (adev->mode == AUDIO_MODE_IN_CALL) {
        if (earpiece_on) {
            set_voicecall_route_by_array(adev->mixer, voicecall_earpice, 1);
            set_voicecall_route_by_array(adev->mixer, earpice_input, 1);
            ALOGD("In-Call Earpiece Enabled!!!");
	}
        if (speaker_on) {
            ALOGD("In-Call Speaker Enabled!!!");
            set_voicecall_route_by_array(adev->mixer, voicecall_speaker, 1);
            set_voicecall_route_by_array(adev->mixer, speaker_input, 1);
	}
        if (headset_on || headphone_on) {
            ALOGD("In-Call Headset Enabled!!!");
            set_voicecall_route_by_array(adev->mixer, voicecall_headset, 1);
            set_voicecall_route_by_array(adev->mixer, headset_input, 1);
	}
        if (bt_on) {
            ALOGD("In-Call Bluetooth Enabled!!!");
            end_call(adev);
            start_call(adev);
            set_voicecall_route_by_array(adev->mixer, voicecall_bluetooth, 1);
	}
    }
}

static void select_input_device(struct m0_audio_device *adev)
{
    int input_device = AUDIO_DEVICE_BIT_IN | adev->in_device;

    switch(input_device) {
        case AUDIO_DEVICE_IN_BUILTIN_MIC:
            ALOGD("%s: AUDIO_DEVICE_IN_BUILTIN_MIC", __func__);
            break;
        case AUDIO_DEVICE_IN_BACK_MIC:
            ALOGD("%s: AUDIO_DEVICE_IN_BACK_MIC | AUDIO_DEVICE_IN_BUILTIN_MIC", __func__);
            // Force use both mics for video recording
            adev->in_device = (AUDIO_DEVICE_IN_BACK_MIC | AUDIO_DEVICE_IN_BUILTIN_MIC) & ~AUDIO_DEVICE_BIT_IN;
            break;
        case AUDIO_DEVICE_IN_BLUETOOTH_SCO_HEADSET:
            ALOGD("%s: AUDIO_DEVICE_IN_BLUETOOTH_SCO_HEADSET", __func__);
            break;
        case AUDIO_DEVICE_IN_WIRED_HEADSET:
            ALOGD("%s: AUDIO_DEVICE_IN_WIRED_HEADSET", __func__);
            break;
        default:
            break;
    }

    select_devices(adev);
}

/* must be called with hw device and output stream mutexes locked */
static int start_output_stream_low_latency(struct m0_stream_out *out)
{
    struct m0_audio_device *adev = out->dev;
    unsigned int flags = PCM_OUT;
    int i;
    bool success = true;

    if (adev->mode != AUDIO_MODE_IN_CALL) {
        select_output_device(adev);
    }

    /* default to low power: will be corrected in out_write if necessary before first write to
     * tinyalsa.
     */

    if (adev->out_device & ~(AUDIO_DEVICE_OUT_DGTL_DOCK_HEADSET | AUDIO_DEVICE_OUT_AUX_DIGITAL)) {
        /* Something not a dock in use */
        out->config[PCM_NORMAL] = pcm_config_tones;
        out->config[PCM_NORMAL].rate = MM_FULL_POWER_SAMPLING_RATE;
        out->pcm[PCM_NORMAL] = pcm_open(CARD_DEFAULT, PORT_PLAYBACK,
                                            flags, &out->config[PCM_NORMAL]);
    }

    if (adev->out_device & AUDIO_DEVICE_OUT_DGTL_DOCK_HEADSET) {
        /* SPDIF output in use */
        out->config[PCM_SPDIF] = pcm_config_tones;
        out->config[PCM_SPDIF].rate = MM_FULL_POWER_SAMPLING_RATE;
        out->pcm[PCM_SPDIF] = pcm_open(CARD_DEFAULT, PORT_PLAYBACK,
                                           flags, &out->config[PCM_SPDIF]);
    }

    /* Close any PCMs that could not be opened properly and return an error */
    for (i = 0; i < PCM_TOTAL; i++) {
        if (out->pcm[i] && !pcm_is_ready(out->pcm[i])) {
            ALOGE("%s: cannot open pcm_out driver %d: %s", __func__ , i, pcm_get_error(out->pcm[i]));
            pcm_close(out->pcm[i]);
            out->pcm[i] = NULL;
            success = false;
        }
    }

    if (success) {
        out->buffer_frames = pcm_config_tones.period_size * 2;
        if (out->buffer == NULL)
            out->buffer = malloc(out->buffer_frames * audio_stream_frame_size(&out->stream.common));

        if (adev->echo_reference != NULL)
            out->echo_reference = adev->echo_reference;
        out->resampler->reset(out->resampler);

        return 0;
    }

    return -ENOMEM;
}

/* must be called with hw device and output stream mutexes locked */
static int start_output_stream_deep_buffer(struct m0_stream_out *out)
{
    struct m0_audio_device *adev = out->dev;

    if (adev->mode != AUDIO_MODE_IN_CALL) {
        select_output_device(adev);
    }

    out->write_threshold = PLAYBACK_DEEP_BUFFER_LONG_PERIOD_COUNT * DEEP_BUFFER_LONG_PERIOD_SIZE;
    out->use_long_periods = true;

    out->config[PCM_NORMAL] = pcm_config_mm;
    out->config[PCM_NORMAL].rate = MM_FULL_POWER_SAMPLING_RATE;
    out->pcm[PCM_NORMAL] = pcm_open(CARD_DEFAULT, PORT_PLAYBACK,
                                        PCM_OUT | PCM_MMAP | PCM_NOIRQ, &out->config[PCM_NORMAL]);
    if (out->pcm[PCM_NORMAL] && !pcm_is_ready(out->pcm[PCM_NORMAL])) {
        ALOGE("%s: cannot open pcm_out driver: %s", __func__, pcm_get_error(out->pcm[PCM_NORMAL]));
        pcm_close(out->pcm[PCM_NORMAL]);
        out->pcm[PCM_NORMAL] = NULL;
        return -ENOMEM;
    }
    out->buffer_frames = DEEP_BUFFER_SHORT_PERIOD_SIZE * 2;
    if (out->buffer == NULL)
        out->buffer = malloc(PLAYBACK_DEEP_BUFFER_LONG_PERIOD_COUNT * DEEP_BUFFER_LONG_PERIOD_SIZE);

    return 0;
}

static int check_input_parameters(uint32_t sample_rate, audio_format_t format, int channel_count)
{
    if (format != AUDIO_FORMAT_PCM_16_BIT)
        return -EINVAL;

    if ((channel_count < 1) || (channel_count > 2))
        return -EINVAL;

    switch(sample_rate) {
    case 8000:
    case 11025:
    case 16000:
    case 22050:
    case 24000:
    case 32000:
    case 44100:
    case 48000:
        break;
    default:
        return -EINVAL;
    }

    return 0;
}

static size_t get_input_buffer_size(uint32_t sample_rate, audio_format_t format, int channel_count)
{
    size_t size;
    size_t device_rate;

    if (check_input_parameters(sample_rate, format, channel_count) != 0)
        return 0;

    /* take resampling into account and return the closest majoring
    multiple of 16 frames, as audioflinger expects audio buffers to
    be a multiple of 16 frames */
    size = (pcm_config_capture.period_size * sample_rate) / pcm_config_capture.rate;
    size = ((size + 15) / 16) * 16;

    return size * channel_count * sizeof(short);
}

static void add_echo_reference(struct m0_stream_out *out,
                               struct echo_reference_itfe *reference)
{
    pthread_mutex_lock(&out->lock);
    out->echo_reference = reference;
    pthread_mutex_unlock(&out->lock);
}

static void remove_echo_reference(struct m0_stream_out *out,
                                  struct echo_reference_itfe *reference)
{
    pthread_mutex_lock(&out->lock);
    if (out->echo_reference == reference) {
        /* stop writing to echo reference */
        reference->write(reference, NULL);
        out->echo_reference = NULL;
    }
    pthread_mutex_unlock(&out->lock);
}

static void put_echo_reference(struct m0_audio_device *adev,
                          struct echo_reference_itfe *reference)
{
    if (adev->echo_reference != NULL &&
            reference == adev->echo_reference) {
        /* echo reference is taken from the low latency output stream used
         * for voice use cases */
        if (adev->outputs[OUTPUT_LOW_LATENCY] != NULL &&
                !adev->outputs[OUTPUT_LOW_LATENCY]->standby)
            remove_echo_reference(adev->outputs[OUTPUT_LOW_LATENCY], reference);
        release_echo_reference(reference);
        adev->echo_reference = NULL;
    }
}

static struct echo_reference_itfe *get_echo_reference(struct m0_audio_device *adev,
                                               audio_format_t format,
                                               uint32_t channel_count,
                                               uint32_t sampling_rate)
{
    put_echo_reference(adev, adev->echo_reference);
    /* echo reference is taken from the low latency output stream used
     * for voice use cases */
    if (adev->outputs[OUTPUT_LOW_LATENCY] != NULL &&
            !adev->outputs[OUTPUT_LOW_LATENCY]->standby) {
        struct audio_stream *stream =
                &adev->outputs[OUTPUT_LOW_LATENCY]->stream.common;
        uint32_t wr_channel_count = popcount(stream->get_channels(stream));
        uint32_t wr_sampling_rate = stream->get_sample_rate(stream);

        int status = create_echo_reference(AUDIO_FORMAT_PCM_16_BIT,
                                           channel_count,
                                           sampling_rate,
                                           AUDIO_FORMAT_PCM_16_BIT,
                                           wr_channel_count,
                                           wr_sampling_rate,
                                           &adev->echo_reference);
        if (status == 0)
            add_echo_reference(adev->outputs[OUTPUT_LOW_LATENCY],
                               adev->echo_reference);
    }
    return adev->echo_reference;
}

static int get_playback_delay(struct m0_stream_out *out,
                       size_t frames,
                       struct echo_reference_buffer *buffer)
{
    size_t kernel_frames;
    int status;
    int primary_pcm = 0;

    /* Find the first active PCM to act as primary */
    while ((primary_pcm < PCM_TOTAL) && !out->pcm[primary_pcm])
        primary_pcm++;

    status = pcm_get_htimestamp(out->pcm[primary_pcm], &kernel_frames, &buffer->time_stamp);
    if (status < 0) {
        buffer->time_stamp.tv_sec  = 0;
        buffer->time_stamp.tv_nsec = 0;
        buffer->delay_ns           = 0;
        ALOGV("%s: pcm_get_htimestamp error,"
                "setting playbackTimestamp to 0", __func__);
        return status;
    }

    kernel_frames = pcm_get_buffer_size(out->pcm[primary_pcm]) - kernel_frames;

    /* adjust render time stamp with delay added by current driver buffer.
     * Add the duration of current frame as we want the render time of the last
     * sample being written. */
    buffer->delay_ns = (long)(((int64_t)(kernel_frames + frames)* 1000000000)/
                            MM_FULL_POWER_SAMPLING_RATE);

    return 0;
}

static uint32_t out_get_sample_rate(const struct audio_stream *stream)
{
    return DEFAULT_OUT_SAMPLING_RATE;
}

static int out_set_sample_rate(struct audio_stream *stream, uint32_t rate)
{
    return 0;
}

static size_t out_get_buffer_size_low_latency(const struct audio_stream *stream)
{
    struct m0_stream_out *out = (struct m0_stream_out *)stream;

    /* take resampling into account and return the closest majoring
    multiple of 16 frames, as audioflinger expects audio buffers to
    be a multiple of 16 frames. Note: we use the default rate here
    from pcm_config_tones.rate. */
    size_t size = (SHORT_PERIOD_SIZE * DEFAULT_OUT_SAMPLING_RATE) / pcm_config_tones.rate;
    size = ((size + 15) / 16) * 16;
    return size * audio_stream_frame_size((struct audio_stream *)stream);
}

static size_t out_get_buffer_size_deep_buffer(const struct audio_stream *stream)
{
    struct m0_stream_out *out = (struct m0_stream_out *)stream;

    /* take resampling into account and return the closest majoring
    multiple of 16 frames, as audioflinger expects audio buffers to
    be a multiple of 16 frames. Note: we use the default rate here
    from pcm_config_mm.rate. */
    size_t size = (DEEP_BUFFER_SHORT_PERIOD_SIZE * DEFAULT_OUT_SAMPLING_RATE) /
                        pcm_config_mm.rate;
    size = ((size + 15) / 16) * 16;
    return size * audio_stream_frame_size((struct audio_stream *)stream);
}

static audio_channel_mask_t out_get_channels(const struct audio_stream *stream)
{
    struct m0_stream_out *out = (struct m0_stream_out *)stream;

    return out->channel_mask;
}

static audio_format_t out_get_format(const struct audio_stream *stream)
{
    return AUDIO_FORMAT_PCM_16_BIT;
}

static int out_set_format(struct audio_stream *stream, audio_format_t format)
{
    return 0;
}

/* must be called with hw device and output stream mutexes locked */
static int do_output_standby(struct m0_stream_out *out)
{
    struct m0_audio_device *adev = out->dev;
    int i;
    bool all_outputs_in_standby = true;

    if (!out->standby) {
        out->standby = 1;

        for (i = 0; i < PCM_TOTAL; i++) {
            if (out->pcm[i]) {
                pcm_close(out->pcm[i]);
                out->pcm[i] = NULL;
            }
        }

        for (i = 0; i < OUTPUT_TOTAL; i++) {
            if (adev->outputs[i] != NULL && !adev->outputs[i]->standby) {
                all_outputs_in_standby = false;
                break;
            }
        }

        /* force standby on low latency output stream so that it can reuse HDMI driver if
         * necessary when restarted */
        if (out == adev->outputs[OUTPUT_HDMI]) {
            if (adev->outputs[OUTPUT_LOW_LATENCY] != NULL &&
                    !adev->outputs[OUTPUT_LOW_LATENCY]->standby) {
                struct m0_stream_out *ll_out = adev->outputs[OUTPUT_LOW_LATENCY];
                pthread_mutex_lock(&ll_out->lock);
                do_output_standby(ll_out);
                pthread_mutex_unlock(&ll_out->lock);
            }
        }

        /* stop writing to echo reference */
        if (out->echo_reference != NULL) {
            out->echo_reference->write(out->echo_reference, NULL);
            out->echo_reference = NULL;
        }
    }
    return 0;
}

static int out_standby(struct audio_stream *stream)
{
    struct m0_stream_out *out = (struct m0_stream_out *)stream;
    int status;

    pthread_mutex_lock(&out->dev->lock);
    pthread_mutex_lock(&out->lock);
    status = do_output_standby(out);
    pthread_mutex_unlock(&out->lock);
    pthread_mutex_unlock(&out->dev->lock);
    return status;
}

static int out_dump(const struct audio_stream *stream, int fd)
{
    return 0;
}

static int out_set_parameters(struct audio_stream *stream, const char *kvpairs)
{
    struct m0_stream_out *out = (struct m0_stream_out *)stream;
    struct m0_audio_device *adev = out->dev;
    struct m0_stream_in *in;
    struct str_parms *parms;
    char *str;
    char value[32];
    int ret, val = 0;
    bool force_input_standby = false;

    parms = str_parms_create_str(kvpairs);

    ret = str_parms_get_str(parms, AUDIO_PARAMETER_STREAM_ROUTING, value, sizeof(value));
    if (ret >= 0) {
        val = atoi(value);
        pthread_mutex_lock(&adev->lock);
        pthread_mutex_lock(&out->lock);
        if (((adev->out_device) != val) && (val != 0)) {
            /* this is needed only when changing device on low latency output
             * as other output streams are not used for voice use cases nor
             * handle duplication to HDMI or SPDIF */
            if (out == adev->outputs[OUTPUT_LOW_LATENCY] && !out->standby) {
                /* a change in output device may change the microphone selection */
                if (adev->active_input &&
                        adev->active_input->source == AUDIO_SOURCE_VOICE_COMMUNICATION) {
                    force_input_standby = true;
                }
                /* force standby if moving to/from HDMI/SPDIF or if the output
                 * device changes when in HDMI/SPDIF mode */
                /* FIXME also force standby when in call as some audio path switches do not work
                 * while in call and an output stream is active (e.g BT SCO => earpiece) */

                /* FIXME workaround for audio being dropped when switching path without forcing standby
                 * (several hundred ms of audio can be lost: e.g beginning of a ringtone. We must understand
                 * the root cause in audio HAL, driver or ABE.
                if (((val & AUDIO_DEVICE_OUT_AUX_DIGITAL) ^
                        (adev->out_device & AUDIO_DEVICE_OUT_AUX_DIGITAL)) ||
                        ((val & AUDIO_DEVICE_OUT_DGTL_DOCK_HEADSET) ^
                        (adev->out_device & AUDIO_DEVICE_OUT_DGTL_DOCK_HEADSET)) ||
                        (adev->out_device & (AUDIO_DEVICE_OUT_AUX_DIGITAL |
                                         AUDIO_DEVICE_OUT_DGTL_DOCK_HEADSET)))
                */
                if (((val & AUDIO_DEVICE_OUT_AUX_DIGITAL) ^
                        (adev->out_device & AUDIO_DEVICE_OUT_AUX_DIGITAL)) ||
                        ((val & AUDIO_DEVICE_OUT_DGTL_DOCK_HEADSET) ^
                        (adev->out_device & AUDIO_DEVICE_OUT_DGTL_DOCK_HEADSET)) ||
                        (adev->out_device & (AUDIO_DEVICE_OUT_AUX_DIGITAL |
                                         AUDIO_DEVICE_OUT_DGTL_DOCK_HEADSET)) ||
                        ((val & AUDIO_DEVICE_OUT_SPEAKER) ^
                        (adev->out_device & AUDIO_DEVICE_OUT_SPEAKER)) ||
                        (adev->mode == AUDIO_MODE_IN_CALL))
                    do_output_standby(out);
            }
            if (out != adev->outputs[OUTPUT_HDMI]) {
                adev->out_device = val;
                select_output_device(adev);
            }
        }
        pthread_mutex_unlock(&out->lock);
        if (force_input_standby) {
            in = adev->active_input;
            pthread_mutex_lock(&in->lock);
            do_input_standby(in);
            pthread_mutex_unlock(&in->lock);
        }
        pthread_mutex_unlock(&adev->lock);
    }

    str_parms_destroy(parms);
    return ret;
}

static char * out_get_parameters(const struct audio_stream *stream, const char *keys)
{
    struct m0_stream_out *out = (struct m0_stream_out *)stream;

    struct str_parms *query = str_parms_create_str(keys);
    char *str;
    char value[256];
    struct str_parms *reply = str_parms_create();
    size_t i, j;
    int ret;
    bool first = true;

    ret = str_parms_get_str(query, AUDIO_PARAMETER_STREAM_SUP_CHANNELS, value, sizeof(value));
    if (ret >= 0) {
        value[0] = '\0';
        i = 0;
        while (out->sup_channel_masks[i] != 0) {
            for (j = 0; j < ARRAY_SIZE(out_channels_name_to_enum_table); j++) {
                if (out_channels_name_to_enum_table[j].value == out->sup_channel_masks[i]) {
                    if (!first) {
                        strcat(value, "|");
                    }
                    strcat(value, out_channels_name_to_enum_table[j].name);
                    first = false;
                    break;
                }
            }
            i++;
        }
        str_parms_add_str(reply, AUDIO_PARAMETER_STREAM_SUP_CHANNELS, value);
        str = strdup(str_parms_to_str(reply));
    } else {
        str = strdup(keys);
    }
    str_parms_destroy(query);
    str_parms_destroy(reply);
    return str;
}

static uint32_t out_get_latency_low_latency(const struct audio_stream_out *stream)
{
    struct m0_stream_out *out = (struct m0_stream_out *)stream;

    /*  Note: we use the default rate here from pcm_config_mm.rate */
    return (SHORT_PERIOD_SIZE * PLAYBACK_SHORT_PERIOD_COUNT * 1000) / pcm_config_tones.rate;
}

static uint32_t out_get_latency_deep_buffer(const struct audio_stream_out *stream)
{
    struct m0_stream_out *out = (struct m0_stream_out *)stream;

    /*  Note: we use the default rate here from pcm_config_mm.rate */
    return (DEEP_BUFFER_LONG_PERIOD_SIZE * PLAYBACK_DEEP_BUFFER_LONG_PERIOD_COUNT * 1000) /
                    pcm_config_mm.rate;
}

static int out_set_volume(struct audio_stream_out *stream, float left,
                          float right)
{
    return -ENOSYS;
}

static ssize_t out_write_low_latency(struct audio_stream_out *stream, const void* buffer,
                         size_t bytes)
{
    int ret;
    struct m0_stream_out *out = (struct m0_stream_out *)stream;
    struct m0_audio_device *adev = out->dev;
    size_t frame_size = audio_stream_frame_size(&out->stream.common);
    size_t in_frames = bytes / frame_size;
    size_t out_frames = in_frames;
    bool force_input_standby = false;
    struct m0_stream_in *in;
    int i;

    /* acquiring hw device mutex systematically is useful if a low priority thread is waiting
     * on the output stream mutex - e.g. executing select_mode() while holding the hw device
     * mutex
     */
    pthread_mutex_lock(&adev->lock);
    pthread_mutex_lock(&out->lock);
    if (out->standby) {
        ret = start_output_stream_low_latency(out);
        if (ret != 0) {
            pthread_mutex_unlock(&adev->lock);
            goto exit;
        }
        out->standby = 0;
        /* a change in output device may change the microphone selection */
        if (adev->active_input &&
                adev->active_input->source == AUDIO_SOURCE_VOICE_COMMUNICATION)
            force_input_standby = true;
    }
    pthread_mutex_unlock(&adev->lock);

    for (i = 0; i < PCM_TOTAL; i++) {
        /* only use resampler if required */
        if (out->pcm[i] && (out->config[i].rate != DEFAULT_OUT_SAMPLING_RATE)) {
            out_frames = out->buffer_frames;
            out->resampler->resample_from_input(out->resampler,
                                                (int16_t *)buffer,
                                                &in_frames,
                                                (int16_t *)out->buffer,
                                                &out_frames);
            break;
        }
    }

    if (out->echo_reference != NULL) {
        struct echo_reference_buffer b;
        b.raw = (void *)buffer;
        b.frame_count = in_frames;

        get_playback_delay(out, out_frames, &b);
        out->echo_reference->write(out->echo_reference, &b);
    }

    /* Write to all active PCMs */
    for (i = 0; i < PCM_TOTAL; i++) {
        if (out->pcm[i]) {
            if (out->config[i].rate == DEFAULT_OUT_SAMPLING_RATE) {
                /* PCM uses native sample rate */
                ret = PCM_WRITE(out->pcm[i], (void *)buffer, bytes);
            } else {
                /* PCM needs resampler */
                ret = PCM_WRITE(out->pcm[i], (void *)out->buffer, out_frames * frame_size);
            }
            if (ret)
                break;
        }
    }

exit:
    pthread_mutex_unlock(&out->lock);

    if (ret != 0) {
        usleep(bytes * 1000000 / audio_stream_frame_size(&stream->common) /
               out_get_sample_rate(&stream->common));
    }

    if (force_input_standby) {
        pthread_mutex_lock(&adev->lock);
        if (adev->active_input) {
            in = adev->active_input;
            pthread_mutex_lock(&in->lock);
            do_input_standby(in);
            pthread_mutex_unlock(&in->lock);
        }
        pthread_mutex_unlock(&adev->lock);
    }

    return bytes;
}

static ssize_t out_write_deep_buffer(struct audio_stream_out *stream, const void* buffer,
                         size_t bytes)
{
    int ret;
    struct m0_stream_out *out = (struct m0_stream_out *)stream;
    struct m0_audio_device *adev = out->dev;
    size_t frame_size = audio_stream_frame_size(&out->stream.common);
    size_t in_frames = bytes / frame_size;
    size_t out_frames;
    bool use_long_periods;
    int kernel_frames;
    void *buf;

    /* acquiring hw device mutex systematically is useful if a low priority thread is waiting
     * on the output stream mutex - e.g. executing select_mode() while holding the hw device
     * mutex
     */
    pthread_mutex_lock(&adev->lock);
    pthread_mutex_lock(&out->lock);
    if (out->standby) {
        ret = start_output_stream_deep_buffer(out);
        if (ret != 0) {
            pthread_mutex_unlock(&adev->lock);
            goto exit;
        }
        out->standby = 0;
    }
    use_long_periods = adev->screen_off && !adev->active_input;
    pthread_mutex_unlock(&adev->lock);

    if (use_long_periods != out->use_long_periods) {
        size_t period_size;
        size_t period_count;

        if (use_long_periods) {
            period_size = DEEP_BUFFER_LONG_PERIOD_SIZE;
            period_count = PLAYBACK_DEEP_BUFFER_LONG_PERIOD_COUNT;
        } else {
            period_size = DEEP_BUFFER_SHORT_PERIOD_SIZE;
            period_count = PLAYBACK_DEEP_BUFFER_SHORT_PERIOD_COUNT;
        }
        out->write_threshold = period_size * period_count;
        pcm_set_avail_min(out->pcm[PCM_NORMAL], period_size);
        out->use_long_periods = use_long_periods;
    }

    /* only use resampler if required */
    if (out->config[PCM_NORMAL].rate != DEFAULT_OUT_SAMPLING_RATE) {
        out_frames = out->buffer_frames;
        out->resampler->resample_from_input(out->resampler,
                                            (int16_t *)buffer,
                                            &in_frames,
                                            (int16_t *)out->buffer,
                                            &out_frames);
        buf = (void *)out->buffer;
    } else {
        out_frames = in_frames;
        buf = (void *)buffer;
    }

    /* do not allow more than out->write_threshold frames in kernel pcm driver buffer */
    do {
        struct timespec time_stamp;

        if (pcm_get_htimestamp(out->pcm[PCM_NORMAL],
                               (unsigned int *)&kernel_frames, &time_stamp) < 0)
            break;
        kernel_frames = pcm_get_buffer_size(out->pcm[PCM_NORMAL]) - kernel_frames;

        if (kernel_frames > out->write_threshold) {
            unsigned long time = (unsigned long)
                    (((int64_t)(kernel_frames - out->write_threshold) * 1000000) /
                            MM_FULL_POWER_SAMPLING_RATE);
            if (time < MIN_WRITE_SLEEP_US)
                time = MIN_WRITE_SLEEP_US;
            usleep(time);
        }
    } while (kernel_frames > out->write_threshold);

    ret = pcm_mmap_write(out->pcm[PCM_NORMAL], buf, out_frames * frame_size);

exit:
    pthread_mutex_unlock(&out->lock);

    if (ret != 0) {
        usleep(bytes * 1000000 / audio_stream_frame_size(&stream->common) /
               out_get_sample_rate(&stream->common));
    }

    return bytes;
}

static int out_get_render_position(const struct audio_stream_out *stream,
                                   uint32_t *dsp_frames)
{
    return -EINVAL;
}

static int out_add_audio_effect(const struct audio_stream *stream, effect_handle_t effect)
{
    return 0;
}

static int out_remove_audio_effect(const struct audio_stream *stream, effect_handle_t effect)
{
    return 0;
}

/** audio_stream_in implementation **/

/* must be called with hw device and input stream mutexes locked */
static int start_input_stream(struct m0_stream_in *in)
{
    int ret = 0;
    struct m0_audio_device *adev = in->dev;

    adev->active_input = in;

    if (adev->mode != AUDIO_MODE_IN_CALL) {
        adev->in_device = in->device;
        select_input_device(adev);
    }

    if (in->aux_channels_changed)
    {
        in->aux_channels_changed = false;
        in->config.channels = popcount(in->main_channels | in->aux_channels);

        if (in->resampler) {
            /* release and recreate the resampler with the new number of channel of the input */
            release_resampler(in->resampler);
            in->resampler = NULL;
            ret = create_resampler(in->config.rate,
                               in->requested_rate,
                               in->config.channels,
                               RESAMPLER_QUALITY_DEFAULT,
                               &in->buf_provider,
                               &in->resampler);
        }
        ALOGV("%s: New channel configuration, "
                "main_channels = [%04x], aux_channels = [%04x], config.channels = %d",
                __func__, in->main_channels, in->aux_channels, in->config.channels);
    }

    if (in->need_echo_reference && in->echo_reference == NULL)
        in->echo_reference = get_echo_reference(adev,
                                        AUDIO_FORMAT_PCM_16_BIT,
                                        popcount(in->main_channels),
                                        in->requested_rate);

    /* this assumes routing is done previously */
    in->pcm = pcm_open(CARD_DEFAULT, PORT_CAPTURE, PCM_IN, &in->config);
    if (!pcm_is_ready(in->pcm)) {
        ALOGE("cannot open pcm_in driver: %s", pcm_get_error(in->pcm));
        pcm_close(in->pcm);
        adev->active_input = NULL;
        return -ENOMEM;
    }

    /* force read and proc buf reallocation case of frame size or channel count change */
    in->read_buf_frames = 0;
    in->read_buf_size = 0;
    in->proc_buf_frames = 0;
    in->proc_buf_size = 0;
    /* if no supported sample rate is available, use the resampler */
    if (in->resampler) {
        in->resampler->reset(in->resampler);
    }
    return 0;
}

static uint32_t in_get_sample_rate(const struct audio_stream *stream)
{
    struct m0_stream_in *in = (struct m0_stream_in *)stream;

    return in->requested_rate;
}

static int in_set_sample_rate(struct audio_stream *stream, uint32_t rate)
{
    return 0;
}

static size_t in_get_buffer_size(const struct audio_stream *stream)
{
    struct m0_stream_in *in = (struct m0_stream_in *)stream;

    return get_input_buffer_size(in->requested_rate,
                                 AUDIO_FORMAT_PCM_16_BIT,
                                 popcount(in->main_channels));
}

static audio_channel_mask_t in_get_channels(const struct audio_stream *stream)
{
    struct m0_stream_in *in = (struct m0_stream_in *)stream;

    return in->main_channels;
}

static audio_format_t in_get_format(const struct audio_stream *stream)
{
    return AUDIO_FORMAT_PCM_16_BIT;
}

static int in_set_format(struct audio_stream *stream, audio_format_t format)
{
    return 0;
}

/* must be called with hw device and input stream mutexes locked */
static int do_input_standby(struct m0_stream_in *in)
{
    struct m0_audio_device *adev = in->dev;

    if (!in->standby) {
        pcm_close(in->pcm);
        in->pcm = NULL;

        adev->active_input = 0;
        if (adev->mode != AUDIO_MODE_IN_CALL) {
            adev->in_device = AUDIO_DEVICE_NONE;
            select_input_device(adev);
        }

        if (in->echo_reference != NULL) {
            /* stop reading from echo reference */
            in->echo_reference->read(in->echo_reference, NULL);
            put_echo_reference(adev, in->echo_reference);
            in->echo_reference = NULL;
        }

        in->standby = 1;
    }
    return 0;
}

static int in_standby(struct audio_stream *stream)
{
    struct m0_stream_in *in = (struct m0_stream_in *)stream;
    int status;

    pthread_mutex_lock(&in->dev->lock);
    pthread_mutex_lock(&in->lock);
    status = do_input_standby(in);
    pthread_mutex_unlock(&in->lock);
    pthread_mutex_unlock(&in->dev->lock);
    return status;
}

static int in_dump(const struct audio_stream *stream, int fd)
{
    return 0;
}

static int in_set_parameters(struct audio_stream *stream, const char *kvpairs)
{
    struct m0_stream_in *in = (struct m0_stream_in *)stream;
    struct m0_audio_device *adev = in->dev;
    struct str_parms *parms;
    char *str;
    char value[32];
    int ret, val = 0;
    bool do_standby = false;

    parms = str_parms_create_str(kvpairs);

    ret = str_parms_get_str(parms, AUDIO_PARAMETER_STREAM_INPUT_SOURCE, value, sizeof(value));

    pthread_mutex_lock(&adev->lock);
    pthread_mutex_lock(&in->lock);
    if (ret >= 0) {
        val = atoi(value);
        /* no audio source uses val == 0 */
        if ((in->source != val) && (val != 0)) {
            in->source = val;
            do_standby = true;
        }
    }

    ret = str_parms_get_str(parms, AUDIO_PARAMETER_STREAM_ROUTING, value, sizeof(value));
    if (ret >= 0) {
        val = atoi(value) & ~AUDIO_DEVICE_BIT_IN;
        if ((in->device != val) && (val != 0)) {
            in->device = val;
            do_standby = true;
            /* make sure new device selection is incompatible with multi-mic pre processing
             * configuration */
            in_update_aux_channels(in, NULL);
        }
    }

    if (do_standby)
        do_input_standby(in);
    pthread_mutex_unlock(&in->lock);
    pthread_mutex_unlock(&adev->lock);

    str_parms_destroy(parms);
    return ret;
}

static char * in_get_parameters(const struct audio_stream *stream,
                                const char *keys)
{
    return strdup("");
}

static int in_set_gain(struct audio_stream_in *stream, float gain)
{
    return 0;
}

static void get_capture_delay(struct m0_stream_in *in,
                       size_t frames,
                       struct echo_reference_buffer *buffer)
{

    /* read frames available in kernel driver buffer */
    size_t kernel_frames;
    struct timespec tstamp;
    long buf_delay;
    long rsmp_delay;
    long kernel_delay;
    long delay_ns;

    if (pcm_get_htimestamp(in->pcm, &kernel_frames, &tstamp) < 0) {
        buffer->time_stamp.tv_sec  = 0;
        buffer->time_stamp.tv_nsec = 0;
        buffer->delay_ns           = 0;
        ALOGW("%s: pcm_htimestamp error", __func__);
        return;
    }

    /* read frames available in audio HAL input buffer
     * add number of frames being read as we want the capture time of first sample
     * in current buffer */
    /* frames in in->buffer are at driver sampling rate while frames in in->proc_buf are
     * at requested sampling rate */
    buf_delay = (long)(((int64_t)(in->read_buf_frames) * 1000000000) / in->config.rate +
                       ((int64_t)(in->proc_buf_frames) * 1000000000) /
                           in->requested_rate);

    /* add delay introduced by resampler */
    rsmp_delay = 0;
    if (in->resampler) {
        rsmp_delay = in->resampler->delay_ns(in->resampler);
    }

    kernel_delay = (long)(((int64_t)kernel_frames * 1000000000) / in->config.rate);

    delay_ns = kernel_delay + buf_delay + rsmp_delay;

    buffer->time_stamp = tstamp;
    buffer->delay_ns   = delay_ns;
    ALOGV("%s: time_stamp = [%ld].[%ld], delay_ns: [%d],"
         " kernel_delay:[%ld], buf_delay:[%ld], rsmp_delay:[%ld], kernel_frames:[%d], "
         "in->read_buf_frames:[%d], in->proc_buf_frames:[%d], frames:[%d]",
         __func__, buffer->time_stamp.tv_sec , buffer->time_stamp.tv_nsec, buffer->delay_ns,
         kernel_delay, buf_delay, rsmp_delay, kernel_frames,
         in->read_buf_frames, in->proc_buf_frames, frames);

}

static int32_t update_echo_reference(struct m0_stream_in *in, size_t frames)
{
    struct echo_reference_buffer b;
    b.delay_ns = 0;

    ALOGV("%s: frames = [%d], in->ref_frames_in = [%d],  "
          "b.frame_count = [%d]",
         __func__, frames, in->ref_buf_frames, frames - in->ref_buf_frames);
    if (in->ref_buf_frames < frames) {
        if (in->ref_buf_size < frames) {
            in->ref_buf_size = frames;
            in->ref_buf = (int16_t *)realloc(in->ref_buf, pcm_frames_to_bytes(in->pcm, frames));
            ALOG_ASSERT((in->ref_buf != NULL),
                        "%s failed to reallocate ref_buf", __func__);
            ALOGV("%s: ref_buf %p extended to %d bytes",
                      __func__, in->ref_buf, pcm_frames_to_bytes(in->pcm, frames));
        }
        b.frame_count = frames - in->ref_buf_frames;
        b.raw = (void *)(in->ref_buf + in->ref_buf_frames * in->config.channels);

        get_capture_delay(in, frames, &b);

        if (in->echo_reference->read(in->echo_reference, &b) == 0)
        {
            in->ref_buf_frames += b.frame_count;
            ALOGD("%s: in->ref_buf_frames:[%d], "
                    "in->ref_buf_size:[%d], frames:[%d], b.frame_count:[%d]",
                 __func__, in->ref_buf_frames, in->ref_buf_size, frames, b.frame_count);
        }
    } else
        ALOGW("%s: NOT enough frames to read ref buffer", __func__);
    return b.delay_ns;
}

static int set_preprocessor_param(effect_handle_t handle,
                           effect_param_t *param)
{
    uint32_t size = sizeof(int);
    uint32_t psize = ((param->psize - 1) / sizeof(int) + 1) * sizeof(int) +
                        param->vsize;

    int status = (*handle)->command(handle,
                                   EFFECT_CMD_SET_PARAM,
                                   sizeof (effect_param_t) + psize,
                                   param,
                                   &size,
                                   &param->status);
    if (status == 0)
        status = param->status;

    return status;
}

static int set_preprocessor_echo_delay(effect_handle_t handle,
                                     int32_t delay_us)
{
    uint32_t buf[sizeof(effect_param_t) / sizeof(uint32_t) + 2];
    effect_param_t *param = (effect_param_t *)buf;

    param->psize = sizeof(uint32_t);
    param->vsize = sizeof(uint32_t);
    *(uint32_t *)param->data = AEC_PARAM_ECHO_DELAY;
    *((int32_t *)param->data + 1) = delay_us;

    return set_preprocessor_param(handle, param);
}

static void push_echo_reference(struct m0_stream_in *in, size_t frames)
{
    /* read frames from echo reference buffer and update echo delay
     * in->ref_buf_frames is updated with frames available in in->ref_buf */
    int32_t delay_us = update_echo_reference(in, frames)/1000;
    int i;
    audio_buffer_t buf;

    if (in->ref_buf_frames < frames)
        frames = in->ref_buf_frames;

    buf.frameCount = frames;
    buf.raw = in->ref_buf;

    for (i = 0; i < in->num_preprocessors; i++) {
        if ((*in->preprocessors[i].effect_itfe)->process_reverse == NULL)
            continue;

        (*in->preprocessors[i].effect_itfe)->process_reverse(in->preprocessors[i].effect_itfe,
                                               &buf,
                                               NULL);
        set_preprocessor_echo_delay(in->preprocessors[i].effect_itfe, delay_us);
    }

    in->ref_buf_frames -= buf.frameCount;
    if (in->ref_buf_frames) {
        memcpy(in->ref_buf,
               in->ref_buf + buf.frameCount * in->config.channels,
               in->ref_buf_frames * in->config.channels * sizeof(int16_t));
    }
}

static int get_next_buffer(struct resampler_buffer_provider *buffer_provider,
                                   struct resampler_buffer* buffer)
{
    struct m0_stream_in *in;

    if (buffer_provider == NULL || buffer == NULL)
        return -EINVAL;

    in = (struct m0_stream_in *)((char *)buffer_provider -
                                   offsetof(struct m0_stream_in, buf_provider));

    if (in->pcm == NULL) {
        buffer->raw = NULL;
        buffer->frame_count = 0;
        in->read_status = -ENODEV;
        return -ENODEV;
    }

    if (in->read_buf_frames == 0) {
        size_t size_in_bytes = pcm_frames_to_bytes(in->pcm, in->config.period_size);
        if (in->read_buf_size < in->config.period_size) {
            in->read_buf_size = in->config.period_size;
            in->read_buf = (int16_t *) realloc(in->read_buf, size_in_bytes);
            ALOG_ASSERT((in->read_buf != NULL),
                        "%s failed to reallocate read_buf", __func__);
            ALOGV("%s: read_buf %p extended to %d bytes",
                  __func__, in->read_buf, size_in_bytes);
        }

        in->read_status = pcm_read(in->pcm, (void*)in->read_buf, size_in_bytes);

        if (in->read_status != 0) {
            ALOGE("%s: pcm_read error %d", __func__, in->read_status);
            buffer->raw = NULL;
            buffer->frame_count = 0;
            return in->read_status;
        }
        in->read_buf_frames = in->config.period_size;
    }

    buffer->frame_count = (buffer->frame_count > in->read_buf_frames) ?
                                in->read_buf_frames : buffer->frame_count;
    buffer->i16 = in->read_buf + (in->config.period_size - in->read_buf_frames) *
                                                in->config.channels;

    return in->read_status;

}

static void release_buffer(struct resampler_buffer_provider *buffer_provider,
                                  struct resampler_buffer* buffer)
{
    struct m0_stream_in *in;

    if (buffer_provider == NULL || buffer == NULL)
        return;

    in = (struct m0_stream_in *)((char *)buffer_provider -
                                   offsetof(struct m0_stream_in, buf_provider));

    in->read_buf_frames -= buffer->frame_count;
}

/* read_frames() reads frames from kernel driver, down samples to capture rate
 * if necessary and output the number of frames requested to the buffer specified */
static ssize_t read_frames(struct m0_stream_in *in, void *buffer, ssize_t frames)
{
    ssize_t frames_wr = 0;

    while (frames_wr < frames) {
        size_t frames_rd = frames - frames_wr;
        if (in->resampler != NULL) {
            in->resampler->resample_from_provider(in->resampler,
                                                  (int16_t *)((char *)buffer +
                                                      pcm_frames_to_bytes(in->pcm ,frames_wr)),
                                                  &frames_rd);

        } else {
            struct resampler_buffer buf = {
                    { raw : NULL, },
                    frame_count : frames_rd,
            };
            get_next_buffer(&in->buf_provider, &buf);
            if (buf.raw != NULL) {
                memcpy((char *)buffer +
                            pcm_frames_to_bytes(in->pcm, frames_wr),
                        buf.raw,
                        pcm_frames_to_bytes(in->pcm, buf.frame_count));
                frames_rd = buf.frame_count;
            }
            release_buffer(&in->buf_provider, &buf);
        }
        /* in->read_status is updated by getNextBuffer() also called by
         * in->resampler->resample_from_provider() */
        if (in->read_status != 0)
            return in->read_status;

        frames_wr += frames_rd;
    }
    return frames_wr;
}

/* process_frames() reads frames from kernel driver (via read_frames()),
 * calls the active audio pre processings and output the number of frames requested
 * to the buffer specified */
static ssize_t process_frames(struct m0_stream_in *in, void* buffer, ssize_t frames)
{
    ssize_t frames_wr = 0;
    audio_buffer_t in_buf;
    audio_buffer_t out_buf;
    int i;
    bool has_aux_channels = (~in->main_channels & in->aux_channels);
    void *proc_buf_out;

    if (has_aux_channels)
        proc_buf_out = in->proc_buf_out;
    else
        proc_buf_out = buffer;

    /* since all the processing below is done in frames and using the config.channels
     * as the number of channels, no changes is required in case aux_channels are present */
    while (frames_wr < frames) {
        /* first reload enough frames at the end of process input buffer */
        if (in->proc_buf_frames < (size_t)frames) {
            ssize_t frames_rd;

            if (in->proc_buf_size < (size_t)frames) {
                size_t size_in_bytes = pcm_frames_to_bytes(in->pcm, frames);

                in->proc_buf_size = (size_t)frames;
                in->proc_buf_in = (int16_t *)realloc(in->proc_buf_in, size_in_bytes);
                ALOG_ASSERT((in->proc_buf_in != NULL),
                            "%s failed to reallocate proc_buf_in", __func__);
                if (has_aux_channels) {
                    in->proc_buf_out = (int16_t *)realloc(in->proc_buf_out, size_in_bytes);
                    ALOG_ASSERT((in->proc_buf_out != NULL),
                                "%s failed to reallocate proc_buf_out", __func__);
                    proc_buf_out = in->proc_buf_out;
                }
                ALOGV("process_frames(): proc_buf_in %p extended to %d bytes",
                     in->proc_buf_in, size_in_bytes);
            }
            frames_rd = read_frames(in,
                                    in->proc_buf_in +
                                        in->proc_buf_frames * in->config.channels,
                                    frames - in->proc_buf_frames);
            if (frames_rd < 0) {
                frames_wr = frames_rd;
                break;
            }
            in->proc_buf_frames += frames_rd;
        }

        if (in->echo_reference != NULL)
            push_echo_reference(in, in->proc_buf_frames);

         /* in_buf.frameCount and out_buf.frameCount indicate respectively
          * the maximum number of frames to be consumed and produced by process() */
        in_buf.frameCount = in->proc_buf_frames;
        in_buf.s16 = in->proc_buf_in;
        out_buf.frameCount = frames - frames_wr;
        out_buf.s16 = (int16_t *)proc_buf_out + frames_wr * in->config.channels;

        /* FIXME: this works because of current pre processing library implementation that
         * does the actual process only when the last enabled effect process is called.
         * The generic solution is to have an output buffer for each effect and pass it as
         * input to the next.
         */
        for (i = 0; i < in->num_preprocessors; i++) {
            (*in->preprocessors[i].effect_itfe)->process(in->preprocessors[i].effect_itfe,
                                               &in_buf,
                                               &out_buf);
        }

        /* process() has updated the number of frames consumed and produced in
         * in_buf.frameCount and out_buf.frameCount respectively
         * move remaining frames to the beginning of in->proc_buf_in */
        in->proc_buf_frames -= in_buf.frameCount;

        if (in->proc_buf_frames) {
            memcpy(in->proc_buf_in,
                   in->proc_buf_in + in_buf.frameCount * in->config.channels,
                   in->proc_buf_frames * in->config.channels * sizeof(int16_t));
        }

        /* if not enough frames were passed to process(), read more and retry. */
        if (out_buf.frameCount == 0) {
            ALOGW("No frames produced by preproc");
            continue;
        }

        if ((frames_wr + (ssize_t)out_buf.frameCount) <= frames) {
            frames_wr += out_buf.frameCount;
        } else {
            /* The effect does not comply to the API. In theory, we should never end up here! */
            ALOGE("%s: preprocessing produced too many frames: %d + %d  > %d !", __func__,
                  (unsigned int)frames_wr, out_buf.frameCount, (unsigned int)frames);
            frames_wr = frames;
        }
    }

    /* Remove aux_channels that have been added on top of main_channels
     * Assumption is made that the channels are interleaved and that the main
     * channels are first. */
    if (has_aux_channels)
    {
        size_t src_channels = in->config.channels;
        size_t dst_channels = popcount(in->main_channels);
        int16_t* src_buffer = (int16_t *)proc_buf_out;
        int16_t* dst_buffer = (int16_t *)buffer;

        if (dst_channels == 1) {
            for (i = frames_wr; i > 0; i--)
            {
                *dst_buffer++ = *src_buffer;
                src_buffer += src_channels;
            }
        } else {
            for (i = frames_wr; i > 0; i--)
            {
                memcpy(dst_buffer, src_buffer, dst_channels*sizeof(int16_t));
                dst_buffer += dst_channels;
                src_buffer += src_channels;
            }
        }
    }

    return frames_wr;
}

static ssize_t in_read(struct audio_stream_in *stream, void* buffer,
                       size_t bytes)
{
    int ret = 0;
    struct m0_stream_in *in = (struct m0_stream_in *)stream;
    struct m0_audio_device *adev = in->dev;
    size_t frames_rq = bytes / audio_stream_frame_size(&stream->common);

    /* acquiring hw device mutex systematically is useful if a low priority thread is waiting
     * on the input stream mutex - e.g. executing select_mode() while holding the hw device
     * mutex
     */
    pthread_mutex_lock(&adev->lock);
    pthread_mutex_lock(&in->lock);
    if (in->standby) {
        ret = start_input_stream(in);
        if (ret == 0)
            in->standby = 0;
    }
    pthread_mutex_unlock(&adev->lock);

    if (ret < 0)
        goto exit;

    if (in->num_preprocessors != 0)
        ret = process_frames(in, buffer, frames_rq);
    else if (in->resampler != NULL)
        ret = read_frames(in, buffer, frames_rq);
    else
        ret = pcm_read(in->pcm, buffer, bytes);

    if (ret > 0)
        ret = 0;

    if (ret == 0 && adev->mic_mute)
        memset(buffer, 0, bytes);

exit:
    if (ret < 0)
        usleep(bytes * 1000000 / audio_stream_frame_size(&stream->common) /
               in_get_sample_rate(&stream->common));

    pthread_mutex_unlock(&in->lock);
    return bytes;
}

static uint32_t in_get_input_frames_lost(struct audio_stream_in *stream)
{
    return 0;
}

#define GET_COMMAND_STATUS(status, fct_status, cmd_status) \
            do {                                           \
                if (fct_status != 0)                       \
                    status = fct_status;                   \
                else if (cmd_status != 0)                  \
                    status = cmd_status;                   \
            } while(0)

static int in_configure_reverse(struct m0_stream_in *in)
{
    int32_t cmd_status;
    uint32_t size = sizeof(int);
    effect_config_t config;
    int32_t status = 0;
    int32_t fct_status = 0;
    int i;

    if (in->num_preprocessors > 0) {
        config.inputCfg.channels = in->main_channels;
        config.outputCfg.channels = in->main_channels;
        config.inputCfg.format = AUDIO_FORMAT_PCM_16_BIT;
        config.outputCfg.format = AUDIO_FORMAT_PCM_16_BIT;
        config.inputCfg.samplingRate = in->requested_rate;
        config.outputCfg.samplingRate = in->requested_rate;
        config.inputCfg.mask =
                ( EFFECT_CONFIG_SMP_RATE | EFFECT_CONFIG_CHANNELS | EFFECT_CONFIG_FORMAT );
        config.outputCfg.mask =
                ( EFFECT_CONFIG_SMP_RATE | EFFECT_CONFIG_CHANNELS | EFFECT_CONFIG_FORMAT );

        for (i = 0; i < in->num_preprocessors; i++)
        {
            if ((*in->preprocessors[i].effect_itfe)->process_reverse == NULL)
                continue;
            fct_status = (*(in->preprocessors[i].effect_itfe))->command(
                                                        in->preprocessors[i].effect_itfe,
                                                        EFFECT_CMD_SET_CONFIG_REVERSE,
                                                        sizeof(effect_config_t),
                                                        &config,
                                                        &size,
                                                        &cmd_status);
            GET_COMMAND_STATUS(status, fct_status, cmd_status);
        }
    }
    return status;
}

#define MAX_NUM_CHANNEL_CONFIGS 10

static void in_read_audio_effect_channel_configs(struct m0_stream_in *in,
                                                 struct effect_info_s *effect_info)
{
    /* size and format of the cmd are defined in hardware/audio_effect.h */
    effect_handle_t effect = effect_info->effect_itfe;
    uint32_t cmd_size = 2 * sizeof(uint32_t);
    uint32_t cmd[] = { EFFECT_FEATURE_AUX_CHANNELS, MAX_NUM_CHANNEL_CONFIGS };
    /* reply = status + number of configs (n) + n x channel_config_t */
    uint32_t reply_size =
            2 * sizeof(uint32_t) + (MAX_NUM_CHANNEL_CONFIGS * sizeof(channel_config_t));
    int32_t reply[reply_size];
    int32_t cmd_status;

    ALOG_ASSERT((effect_info->num_channel_configs == 0),
                "in_read_audio_effect_channel_configs() num_channel_configs not cleared");
    ALOG_ASSERT((effect_info->channel_configs == NULL),
                "in_read_audio_effect_channel_configs() channel_configs not cleared");

    /* if this command is not supported, then the effect is supposed to return -EINVAL.
     * This error will be interpreted as if the effect supports the main_channels but does not
     * support any aux_channels */
    cmd_status = (*effect)->command(effect,
                                EFFECT_CMD_GET_FEATURE_SUPPORTED_CONFIGS,
                                cmd_size,
                                (void*)&cmd,
                                &reply_size,
                                (void*)&reply);

    if (cmd_status != 0) {
        ALOGI("%s: fx->command returned %d", __func__, cmd_status);
        return;
    }

    if (reply[0] != 0) {
        ALOGW("%s: "
                "command EFFECT_CMD_GET_FEATURE_SUPPORTED_CONFIGS error %d num configs %d",
                __func__, reply[0], (reply[0] == -ENOMEM) ? reply[1] : MAX_NUM_CHANNEL_CONFIGS);
        return;
    }

    /* the feature is not supported */
    ALOGI("in_read_audio_effect_channel_configs()(): "
            "Feature supported and adding %d channel configs to the list", reply[1]);
    effect_info->num_channel_configs = reply[1];
    effect_info->channel_configs =
            (channel_config_t *) malloc(sizeof(channel_config_t) * reply[1]); /* n x configs */
    memcpy(effect_info->channel_configs, (reply + 2), sizeof(channel_config_t) * reply[1]);
}


static uint32_t in_get_aux_channels(struct m0_stream_in *in)
{
    int i;
    channel_config_t new_chcfg = {0, 0};

    if (in->num_preprocessors == 0)
        return 0;

    /* do not enable dual mic configurations when capturing from other microphones than
     * main or sub */
    if (!(in->device & (AUDIO_DEVICE_IN_BUILTIN_MIC | AUDIO_DEVICE_IN_BACK_MIC)))
        return 0;

    /* retain most complex aux channels configuration compatible with requested main channels and
     * supported by audio driver and all pre processors */
    for (i = 0; i < NUM_IN_AUX_CNL_CONFIGS; i++) {
        channel_config_t *cur_chcfg = &in_aux_cnl_configs[i];
        if (cur_chcfg->main_channels == in->main_channels) {
            size_t match_cnt;
            size_t idx_preproc;
            for (idx_preproc = 0, match_cnt = 0;
                 /* no need to continue if at least one preprocessor doesn't match */
                 idx_preproc < (size_t)in->num_preprocessors && match_cnt == idx_preproc;
                 idx_preproc++) {
                struct effect_info_s *effect_info = &in->preprocessors[idx_preproc];
                size_t idx_chcfg;

                for (idx_chcfg = 0; idx_chcfg < effect_info->num_channel_configs; idx_chcfg++) {
                    if (memcmp(effect_info->channel_configs + idx_chcfg,
                               cur_chcfg,
                               sizeof(channel_config_t)) == 0) {
                        match_cnt++;
                        break;
                    }
                }
            }
            /* if all preprocessors match, we have a candidate */
            if (match_cnt == (size_t)in->num_preprocessors) {
                /* retain most complex aux channels configuration */
                if (popcount(cur_chcfg->aux_channels) > popcount(new_chcfg.aux_channels)) {
                    new_chcfg = *cur_chcfg;
                }
            }
        }
    }

    ALOGI("in_get_aux_channels(): return %04x", new_chcfg.aux_channels);

    return new_chcfg.aux_channels;
}

static int in_configure_effect_channels(effect_handle_t effect,
                                        channel_config_t *channel_config)
{
    int status = 0;
    int fct_status;
    int32_t cmd_status;
    uint32_t reply_size;
    effect_config_t config;
    uint32_t cmd[(sizeof(uint32_t) + sizeof(channel_config_t) - 1) / sizeof(uint32_t) + 1];

    ALOGI("in_configure_effect_channels(): configure effect with channels: [%04x][%04x]",
            channel_config->main_channels,
            channel_config->aux_channels);

    config.inputCfg.mask = EFFECT_CONFIG_CHANNELS;
    config.outputCfg.mask = EFFECT_CONFIG_CHANNELS;
    reply_size = sizeof(effect_config_t);
    fct_status = (*effect)->command(effect,
                                EFFECT_CMD_GET_CONFIG,
                                0,
                                NULL,
                                &reply_size,
                                &config);
    if (fct_status != 0) {
        ALOGE("in_configure_effect_channels(): EFFECT_CMD_GET_CONFIG failed");
        return fct_status;
    }

    config.inputCfg.channels = channel_config->main_channels | channel_config->aux_channels;
    config.outputCfg.channels = config.inputCfg.channels;
    reply_size = sizeof(uint32_t);
    fct_status = (*effect)->command(effect,
                                    EFFECT_CMD_SET_CONFIG,
                                    sizeof(effect_config_t),
                                    &config,
                                    &reply_size,
                                    &cmd_status);
    GET_COMMAND_STATUS(status, fct_status, cmd_status);

    cmd[0] = EFFECT_FEATURE_AUX_CHANNELS;
    memcpy(cmd + 1, channel_config, sizeof(channel_config_t));
    reply_size = sizeof(uint32_t);
    fct_status = (*effect)->command(effect,
                                EFFECT_CMD_SET_FEATURE_CONFIG,
                                sizeof(cmd), //sizeof(uint32_t) + sizeof(channel_config_t),
                                cmd,
                                &reply_size,
                                &cmd_status);
    GET_COMMAND_STATUS(status, fct_status, cmd_status);

    /* some implementations need to be re-enabled after a config change */
    reply_size = sizeof(uint32_t);
    fct_status = (*effect)->command(effect,
                                  EFFECT_CMD_ENABLE,
                                  0,
                                  NULL,
                                  &reply_size,
                                  &cmd_status);
    GET_COMMAND_STATUS(status, fct_status, cmd_status);

    return status;
}

static int in_reconfigure_channels(struct m0_stream_in *in,
                                   effect_handle_t effect,
                                   channel_config_t *channel_config,
                                   bool config_changed) {

    int status = 0;

    ALOGI("%s: config_changed %d effect %p",
          __func__, config_changed, effect);

    /* if config changed, reconfigure all previously added effects */
    if (config_changed) {
        int i;
        for (i = 0; i < in->num_preprocessors; i++)
        {
            int cur_status = in_configure_effect_channels(in->preprocessors[i].effect_itfe,
                                                  channel_config);
            if (cur_status != 0) {
                ALOGI("%s: error %d configuring effect "
                        "%d with channels: [%04x][%04x]",
                        __func__,
                        cur_status,
                        i,
                        channel_config->main_channels,
                        channel_config->aux_channels);
                status = cur_status;
            }
        }
    } else if (effect != NULL && channel_config->aux_channels) {
        /* if aux channels config did not change but aux channels are present,
         * we still need to configure the effect being added */
        status = in_configure_effect_channels(effect, channel_config);
    }
    return status;
}

static void in_update_aux_channels(struct m0_stream_in *in,
                                   effect_handle_t effect)
{
    uint32_t aux_channels;
    channel_config_t channel_config;
    int status;

    aux_channels = in_get_aux_channels(in);

    channel_config.main_channels = in->main_channels;
    channel_config.aux_channels = aux_channels;
    status = in_reconfigure_channels(in,
                                     effect,
                                     &channel_config,
                                     (aux_channels != in->aux_channels));

    if (status != 0) {
        ALOGI("%s: in_reconfigure_channels error %d", __func__, status);
        /* resetting aux channels configuration */
        aux_channels = 0;
        channel_config.aux_channels = 0;
        in_reconfigure_channels(in, effect, &channel_config, true);
    }
    if (in->aux_channels != aux_channels) {
        in->aux_channels_changed = true;
        in->aux_channels = aux_channels;
        do_input_standby(in);
    }
}

static int in_add_audio_effect(const struct audio_stream *stream,
                               effect_handle_t effect)
{
    struct m0_stream_in *in = (struct m0_stream_in *)stream;
    int status;
    effect_descriptor_t desc;

    pthread_mutex_lock(&in->dev->lock);
    pthread_mutex_lock(&in->lock);
    if (in->num_preprocessors >= MAX_PREPROCESSORS) {
        status = -ENOSYS;
        goto exit;
    }

    status = (*effect)->get_descriptor(effect, &desc);
    if (status != 0)
        goto exit;

    in->preprocessors[in->num_preprocessors].effect_itfe = effect;
    /* add the supported channel of the effect in the channel_configs */
    in_read_audio_effect_channel_configs(in, &in->preprocessors[in->num_preprocessors]);

    in->num_preprocessors++;

    /* check compatibility between main channel supported and possible auxiliary channels */
    in_update_aux_channels(in, effect);

    ALOGV("%s: effect type: %08x", __func__, desc.type.timeLow);

    if (memcmp(&desc.type, FX_IID_AEC, sizeof(effect_uuid_t)) == 0) {
        in->need_echo_reference = true;
        do_input_standby(in);
        in_configure_reverse(in);
    }

exit:

    ALOGW_IF(status != 0, "%s: error %d", __func__, status);
    pthread_mutex_unlock(&in->lock);
    pthread_mutex_unlock(&in->dev->lock);
    return status;
}

static int in_remove_audio_effect(const struct audio_stream *stream,
                                  effect_handle_t effect)
{
    struct m0_stream_in *in = (struct m0_stream_in *)stream;
    int i;
    int status = -EINVAL;
    effect_descriptor_t desc;

    pthread_mutex_lock(&in->dev->lock);
    pthread_mutex_lock(&in->lock);
    if (in->num_preprocessors <= 0) {
        status = -ENOSYS;
        goto exit;
    }

    for (i = 0; i < in->num_preprocessors; i++) {
        if (status == 0) { /* status == 0 means an effect was removed from a previous slot */
            in->preprocessors[i - 1].effect_itfe = in->preprocessors[i].effect_itfe;
            in->preprocessors[i - 1].channel_configs = in->preprocessors[i].channel_configs;
            in->preprocessors[i - 1].num_channel_configs = in->preprocessors[i].num_channel_configs;
            ALOGI("in_remove_audio_effect moving fx from %d to %d", i, i - 1);
            continue;
        }
        if (in->preprocessors[i].effect_itfe == effect) {
            ALOGI("in_remove_audio_effect found fx at index %d", i);
            free(in->preprocessors[i].channel_configs);
            status = 0;
        }
    }

    if (status != 0)
        goto exit;

    in->num_preprocessors--;
    /* if we remove one effect, at least the last preproc should be reset */
    in->preprocessors[in->num_preprocessors].num_channel_configs = 0;
    in->preprocessors[in->num_preprocessors].effect_itfe = NULL;
    in->preprocessors[in->num_preprocessors].channel_configs = NULL;

    /* check compatibility between main channel supported and possible auxiliary channels */
    in_update_aux_channels(in, NULL);

    status = (*effect)->get_descriptor(effect, &desc);
    if (status != 0)
        goto exit;

    ALOGI("%s: effect type: %08x", __func__, desc.type.timeLow);

    if (memcmp(&desc.type, FX_IID_AEC, sizeof(effect_uuid_t)) == 0) {
        in->need_echo_reference = false;
        do_input_standby(in);
    }

exit:

    ALOGW_IF(status != 0, "%s: error %d", __func__, status);
    pthread_mutex_unlock(&in->lock);
    pthread_mutex_unlock(&in->dev->lock);
    return status;
}

static int adev_open_output_stream(struct audio_hw_device *dev,
                                   audio_io_handle_t handle,
                                   audio_devices_t devices,
                                   audio_output_flags_t flags,
                                   struct audio_config *config,
                                   struct audio_stream_out **stream_out)
{
    struct m0_audio_device *ladev = (struct m0_audio_device *)dev;
    struct m0_stream_out *out;
    int ret;
    int output_type;
    *stream_out = NULL;

    out = (struct m0_stream_out *)calloc(1, sizeof(struct m0_stream_out));
    if (!out)
        return -ENOMEM;

    out->sup_channel_masks[0] = AUDIO_CHANNEL_OUT_STEREO;
    out->channel_mask = AUDIO_CHANNEL_OUT_STEREO;

    if (ladev->outputs[OUTPUT_DEEP_BUF] != NULL) {
        ret = -ENOSYS;
        goto err_open;
    }
    output_type = OUTPUT_DEEP_BUF;
    out->channel_mask = AUDIO_CHANNEL_OUT_STEREO;
    out->stream.common.get_buffer_size = out_get_buffer_size_deep_buffer;
    out->stream.common.get_sample_rate = out_get_sample_rate;
    out->stream.get_latency = out_get_latency_deep_buffer;
    out->stream.write = out_write_deep_buffer;

    ret = create_resampler(DEFAULT_OUT_SAMPLING_RATE,
                           MM_FULL_POWER_SAMPLING_RATE,
                           2,
                           RESAMPLER_QUALITY_DEFAULT,
                           NULL,
                           &out->resampler);
    if (ret != 0)
        goto err_open;

    out->stream.common.set_sample_rate = out_set_sample_rate;
    out->stream.common.get_channels = out_get_channels;
    out->stream.common.get_format = out_get_format;
    out->stream.common.set_format = out_set_format;
    out->stream.common.standby = out_standby;
    out->stream.common.dump = out_dump;
    out->stream.common.set_parameters = out_set_parameters;
    out->stream.common.get_parameters = out_get_parameters;
    out->stream.common.add_audio_effect = out_add_audio_effect;
    out->stream.common.remove_audio_effect = out_remove_audio_effect;
    out->stream.set_volume = out_set_volume;
    out->stream.get_render_position = out_get_render_position;

    out->dev = ladev;
    out->standby = 1;

    /* FIXME: when we support multiple output devices, we will want to
     * do the following:
     * adev->out_device = out->device;
     * select_output_device(adev);
     * This is because out_set_parameters() with a route is not
     * guaranteed to be called after an output stream is opened. */

    config->format = out->stream.common.get_format(&out->stream.common);
    config->channel_mask = out->stream.common.get_channels(&out->stream.common);
    config->sample_rate = out->stream.common.get_sample_rate(&out->stream.common);

    *stream_out = &out->stream;
    ladev->outputs[output_type] = out;

    return 0;

err_open:
    free(out);
    return ret;
}

static void adev_close_output_stream(struct audio_hw_device *dev,
                                     struct audio_stream_out *stream)
{
    struct m0_audio_device *ladev = (struct m0_audio_device *)dev;
    struct m0_stream_out *out = (struct m0_stream_out *)stream;
    int i;

    out_standby(&stream->common);
    for (i = 0; i < OUTPUT_TOTAL; i++) {
        if (ladev->outputs[i] == out) {
            ladev->outputs[i] = NULL;
            break;
        }
    }

    if (out->buffer)
        free(out->buffer);
    if (out->resampler)
        release_resampler(out->resampler);
    free(stream);
}

static int adev_set_parameters(struct audio_hw_device *dev, const char *kvpairs)
{
    struct m0_audio_device *adev = (struct m0_audio_device *)dev;
    struct str_parms *parms;
    char *str;
    char value[32];
    int ret;

    parms = str_parms_create_str(kvpairs);
    ret = str_parms_get_str(parms, AUDIO_PARAMETER_KEY_TTY_MODE, value, sizeof(value));
    if (ret >= 0) {
        int tty_mode;

        if (strcmp(value, AUDIO_PARAMETER_VALUE_TTY_OFF) == 0)
            tty_mode = TTY_MODE_OFF;
        else if (strcmp(value, AUDIO_PARAMETER_VALUE_TTY_VCO) == 0)
            tty_mode = TTY_MODE_VCO;
        else if (strcmp(value, AUDIO_PARAMETER_VALUE_TTY_HCO) == 0)
            tty_mode = TTY_MODE_HCO;
        else if (strcmp(value, AUDIO_PARAMETER_VALUE_TTY_FULL) == 0)
            tty_mode = TTY_MODE_FULL;
        else
            return -EINVAL;

        pthread_mutex_lock(&adev->lock);
        if (tty_mode != adev->tty_mode) {
            adev->tty_mode = tty_mode;
            if (adev->mode == AUDIO_MODE_IN_CALL)
                select_output_device(adev);
        }
        pthread_mutex_unlock(&adev->lock);
    }

    ret = str_parms_get_str(parms, AUDIO_PARAMETER_KEY_BT_NREC, value, sizeof(value));
    if (ret >= 0) {
        if (strcmp(value, AUDIO_PARAMETER_VALUE_ON) == 0)
            adev->bluetooth_nrec = true;
        else
            adev->bluetooth_nrec = false;
    }

    ret = str_parms_get_str(parms, "screen_off", value, sizeof(value));
    if (ret >= 0) {
        if (strcmp(value, AUDIO_PARAMETER_VALUE_ON) == 0)
            adev->screen_off = false;
        else
            adev->screen_off = true;
    }

    ret = str_parms_get_str(parms, "voiceQualityConfig", value, sizeof(value));
    if (ret >= 0) {
        if (strcmp(value, "Normal") == 0) {
            ALOGI("%s: Voice quality config received with value = %s", __func__,value);
            ril_set_voice_quality(NORMAL_VOICE);
        } else if (strcmp(value, "Clear") == 0) {
            ALOGI("%s: Voice quality config received with value = %s", __func__,value);
            ril_set_voice_quality(CLEAR_VOICE);
        } else if (strcmp(value, "Crisp") == 0) {
            ALOGI("%s: Voice quality config received with value = %s", __func__,value);
            ril_set_voice_quality(CRISP_VOICE);
        } else if (strcmp(value, "Bright") == 0) {
            ALOGI("%s: Voice quality config received with value = %s", __func__,value);
            ril_set_voice_quality(BRIGHT_VOICE);
        } else  {
            ALOGE("%s: Voice quality config received with unknown value = %s", __func__,value);
        }
    }

    str_parms_destroy(parms);
    return ret;
}

static char * adev_get_parameters(const struct audio_hw_device *dev,
                                  const char *keys)
{
    return strdup("");
}

static int adev_init_check(const struct audio_hw_device *dev)
{
    return 0;
}

static int adev_set_voice_volume(struct audio_hw_device *dev, float volume)
{
    struct m0_audio_device *adev = (struct m0_audio_device *)dev;
    int ret;

    adev->voice_volume = volume;

    return ril_set_call_volume(volume);
}

static int adev_set_master_volume(struct audio_hw_device *dev, float volume)
{
    return -ENOSYS;
}

static int adev_set_mode(struct audio_hw_device *dev, audio_mode_t mode)
{
    struct m0_audio_device *adev = (struct m0_audio_device *)dev;

    pthread_mutex_lock(&adev->lock);
    if (adev->mode != mode) {
        adev->mode = mode;
        select_mode(adev);
    }
    pthread_mutex_unlock(&adev->lock);

    return 0;
}

static int adev_set_mic_mute(struct audio_hw_device *dev, bool state)
{
    struct m0_audio_device *adev = (struct m0_audio_device *)dev;

    adev->mic_mute = state;

    return 0;
}

static int adev_get_mic_mute(const struct audio_hw_device *dev, bool *state)
{
    struct m0_audio_device *adev = (struct m0_audio_device *)dev;

    *state = adev->mic_mute;

    return 0;
}

static size_t adev_get_input_buffer_size(const struct audio_hw_device *dev,
                                         const struct audio_config *config)
{
    size_t size;
    int channel_count = popcount(config->channel_mask);
    if (check_input_parameters(config->sample_rate, config->format, channel_count) != 0)
        return 0;

    return get_input_buffer_size(config->sample_rate, config->format, channel_count);
}

static int adev_open_input_stream(struct audio_hw_device *dev,
                                  audio_io_handle_t handle,
                                  audio_devices_t devices,
                                  struct audio_config *config,
                                  struct audio_stream_in **stream_in)
{
    struct m0_audio_device *ladev = (struct m0_audio_device *)dev;
    struct m0_stream_in *in;
    int ret;

    /* Respond with a request for stereo if a different format is given. */
    if (config->channel_mask != AUDIO_CHANNEL_IN_STEREO) {
        config->channel_mask = AUDIO_CHANNEL_IN_STEREO;
        return -EINVAL;
    }

    int channel_count = popcount(config->channel_mask);

    *stream_in = NULL;

    if (check_input_parameters(config->sample_rate, config->format, channel_count) != 0)
        return -EINVAL;

    in = (struct m0_stream_in *)calloc(1, sizeof(struct m0_stream_in));
    if (!in)
        return -ENOMEM;

    in->stream.common.get_sample_rate = in_get_sample_rate;
    in->stream.common.set_sample_rate = in_set_sample_rate;
    in->stream.common.get_buffer_size = in_get_buffer_size;
    in->stream.common.get_channels = in_get_channels;
    in->stream.common.get_format = in_get_format;
    in->stream.common.set_format = in_set_format;
    in->stream.common.standby = in_standby;
    in->stream.common.dump = in_dump;
    in->stream.common.set_parameters = in_set_parameters;
    in->stream.common.get_parameters = in_get_parameters;
    in->stream.common.add_audio_effect = in_add_audio_effect;
    in->stream.common.remove_audio_effect = in_remove_audio_effect;
    in->stream.set_gain = in_set_gain;
    in->stream.read = in_read;
    in->stream.get_input_frames_lost = in_get_input_frames_lost;

    in->requested_rate = config->sample_rate;

    memcpy(&in->config, &pcm_config_capture, sizeof(pcm_config_capture));
    in->config.channels = channel_count;

    in->main_channels = config->channel_mask;

    /* initialisation of preprocessor structure array is implicit with the calloc.
     * same for in->aux_channels and in->aux_channels_changed */

    if (in->requested_rate != in->config.rate) {
        in->buf_provider.get_next_buffer = get_next_buffer;
        in->buf_provider.release_buffer = release_buffer;

        ret = create_resampler(in->config.rate,
                               in->requested_rate,
                               in->config.channels,
                               RESAMPLER_QUALITY_DEFAULT,
                               &in->buf_provider,
                               &in->resampler);
        if (ret != 0) {
            ret = -EINVAL;
            goto err;
        }
    }

    in->dev = ladev;
    in->standby = 1;
    in->device = devices & ~AUDIO_DEVICE_BIT_IN;

    *stream_in = &in->stream;
    return 0;

err:
    if (in->resampler)
        release_resampler(in->resampler);

    free(in);
    return ret;
}

static void adev_close_input_stream(struct audio_hw_device *dev,
                                   struct audio_stream_in *stream)
{
    struct m0_stream_in *in = (struct m0_stream_in *)stream;
    int i;

    in_standby(&stream->common);

    for (i = 0; i < in->num_preprocessors; i++) {
        free(in->preprocessors[i].channel_configs);
    }

    free(in->read_buf);
    if (in->resampler) {
        release_resampler(in->resampler);
    }
    if (in->proc_buf_in)
        free(in->proc_buf_in);
    if (in->proc_buf_out)
        free(in->proc_buf_out);
    if (in->ref_buf)
        free(in->ref_buf);

    free(stream);
    return;
}

static int adev_dump(const audio_hw_device_t *device, int fd)
{
    return 0;
}

static int adev_close(hw_device_t *device)
{
    struct m0_audio_device *adev = (struct m0_audio_device *)device;

    mixer_close(adev->mixer);
    free(device);
    return 0;
}

struct config_parse_state {
    struct m0_audio_device *adev;
    struct m0_dev_cfg *dev;
    bool on;

    struct route_setting *path;
    unsigned int path_len;
};

static const struct {
    int mask;
    const char *name;
} dev_names[] = {
    { AUDIO_DEVICE_OUT_SPEAKER, "speaker" },
    { AUDIO_DEVICE_OUT_WIRED_HEADSET | AUDIO_DEVICE_OUT_WIRED_HEADPHONE, "headphone" },
    { AUDIO_DEVICE_OUT_EARPIECE, "earpiece" },
    { AUDIO_DEVICE_OUT_ANLG_DOCK_HEADSET, "dock" },
    { AUDIO_DEVICE_OUT_DGTL_DOCK_HEADSET, "dock" },
    { AUDIO_DEVICE_OUT_ALL_SCO, "sco-out" },
    { AUDIO_DEVICE_OUT_AUX_DIGITAL, "aux-digital" },

    { AUDIO_DEVICE_IN_BUILTIN_MIC, "builtin-mic" },
    { AUDIO_DEVICE_IN_BACK_MIC, "back-mic" },
    { AUDIO_DEVICE_IN_WIRED_HEADSET, "headset-in" },
    { AUDIO_DEVICE_IN_ALL_SCO, "sco-in" },
};

static void adev_config_start(void *data, const XML_Char *elem,
                  const XML_Char **attr)
{
    struct config_parse_state *s = data;
    struct m0_dev_cfg *dev_cfg;
    const XML_Char *name = NULL;
    const XML_Char *val = NULL;
    unsigned int i, j;

    for (i = 0; attr[i]; i += 2) {
    if (strcmp(attr[i], "name") == 0)
        name = attr[i + 1];

    if (strcmp(attr[i], "val") == 0)
        val = attr[i + 1];
    }

    if (strcmp(elem, "device") == 0) {
    if (!name) {
        ALOGE("Unnamed device\n");
        return;
    }

    for (i = 0; i < sizeof(dev_names) / sizeof(dev_names[0]); i++) {
        if (strcmp(dev_names[i].name, name) == 0) {
        ALOGI("Allocating device %s\n", name);
        dev_cfg = realloc(s->adev->dev_cfgs,
                  (s->adev->num_dev_cfgs + 1)
                  * sizeof(*dev_cfg));
        if (!dev_cfg) {
            ALOGE("Unable to allocate dev_cfg\n");
            return;
        }

        s->dev = &dev_cfg[s->adev->num_dev_cfgs];
        memset(s->dev, 0, sizeof(*s->dev));
        s->dev->mask = dev_names[i].mask;

        s->adev->dev_cfgs = dev_cfg;
        s->adev->num_dev_cfgs++;
        }
    }

    } else if (strcmp(elem, "path") == 0) {
    if (s->path_len)
        ALOGW("Nested paths\n");

    /* If this a path for a device it must have a role */
    if (s->dev) {
        /* Need to refactor a bit... */
        if (strcmp(name, "on") == 0) {
        s->on = true;
        } else if (strcmp(name, "off") == 0) {
        s->on = false;
        } else {
        ALOGW("Unknown path name %s\n", name);
        }
    }

    } else if (strcmp(elem, "ctl") == 0) {
    struct route_setting *r;

    if (!name) {
        ALOGE("Unnamed control\n");
        return;
    }

    if (!val) {
        ALOGE("No value specified for %s\n", name);
        return;
    }

    ALOGV("Parsing control %s => %s\n", name, val);

    r = realloc(s->path, sizeof(*r) * (s->path_len + 1));
    if (!r) {
        ALOGE("Out of memory handling %s => %s\n", name, val);
        return;
    }

    r[s->path_len].ctl_name = strdup(name);
    r[s->path_len].strval = NULL;

    /* This can be fooled but it'll do */
    r[s->path_len].intval = atoi(val);
    if (!r[s->path_len].intval && strcmp(val, "0") != 0)
        r[s->path_len].strval = strdup(val);

    s->path = r;
    s->path_len++;
    }
}

static void adev_config_end(void *data, const XML_Char *name)
{
    struct config_parse_state *s = data;
    unsigned int i;

    if (strcmp(name, "path") == 0) {
    if (!s->path_len)
        ALOGW("Empty path\n");

    if (!s->dev) {
        ALOGV("Applying %d element default route\n", s->path_len);

        set_route_by_array(s->adev->mixer, s->path, s->path_len);

        for (i = 0; i < s->path_len; i++) {
        free(s->path[i].ctl_name);
        free(s->path[i].strval);
        }

        free(s->path);

        /* Refactor! */
    } else if (s->on) {
        ALOGV("%d element on sequence\n", s->path_len);
        s->dev->on = s->path;
        s->dev->on_len = s->path_len;

    } else {
        ALOGV("%d element off sequence\n", s->path_len);

        /* Apply it, we'll reenable anything that's wanted later */
        set_route_by_array(s->adev->mixer, s->path, s->path_len);

        s->dev->off = s->path;
        s->dev->off_len = s->path_len;
    }

    s->path_len = 0;
    s->path = NULL;

    } else if (strcmp(name, "device") == 0) {
    s->dev = NULL;
    }
}

static int adev_config_parse(struct m0_audio_device *adev)
{
    struct config_parse_state s;
    FILE *f;
    XML_Parser p;
    char property[PROPERTY_VALUE_MAX];
    char file[80];
    int ret = 0;
    bool eof = false;
    int len;

    property_get("ro.product.device", property, "tiny_hw");
    snprintf(file, sizeof(file), "/system/etc/tiny_hw.xml");

    ALOGV("Reading configuration from %s\n", file);
    f = fopen(file, "r");
    if (!f) {
    ALOGE("Failed to open %s\n", file);
    return -ENODEV;
    }

    p = XML_ParserCreate(NULL);
    if (!p) {
    ALOGE("Failed to create XML parser\n");
    ret = -ENOMEM;
    goto out;
    }

    memset(&s, 0, sizeof(s));
    s.adev = adev;
    XML_SetUserData(p, &s);

    XML_SetElementHandler(p, adev_config_start, adev_config_end);

    while (!eof) {
    len = fread(file, 1, sizeof(file), f);
    if (ferror(f)) {
        ALOGE("I/O error reading config\n");
        ret = -EIO;
        goto out_parser;
    }
    eof = feof(f);

    if (XML_Parse(p, file, len, eof) == XML_STATUS_ERROR) {
        ALOGE("Parse error at line %u:\n%s\n",
         (unsigned int)XML_GetCurrentLineNumber(p),
         XML_ErrorString(XML_GetErrorCode(p)));
        ret = -EINVAL;
        goto out_parser;
    }
    }

 out_parser:
    XML_ParserFree(p);
 out:
    fclose(f);

    return ret;
}

static int adev_open(const hw_module_t* module, const char* name,
                     hw_device_t** device)
{
    struct m0_audio_device *adev;
    int ret;

    if (strcmp(name, AUDIO_HARDWARE_INTERFACE) != 0)
        return -EINVAL;

    adev = calloc(1, sizeof(struct m0_audio_device));
    if (!adev)
        return -ENOMEM;

    adev->hw_device.common.tag = HARDWARE_DEVICE_TAG;
    adev->hw_device.common.version = AUDIO_DEVICE_API_VERSION_2_0;
    adev->hw_device.common.module = (struct hw_module_t *) module;
    adev->hw_device.common.close = adev_close;

    adev->hw_device.init_check = adev_init_check;
    adev->hw_device.set_voice_volume = adev_set_voice_volume;
    adev->hw_device.set_master_volume = adev_set_master_volume;
    adev->hw_device.set_mode = adev_set_mode;
    adev->hw_device.set_mic_mute = adev_set_mic_mute;
    adev->hw_device.get_mic_mute = adev_get_mic_mute;
    adev->hw_device.set_parameters = adev_set_parameters;
    adev->hw_device.get_parameters = adev_get_parameters;
    adev->hw_device.get_input_buffer_size = adev_get_input_buffer_size;
    adev->hw_device.open_output_stream = adev_open_output_stream;
    adev->hw_device.close_output_stream = adev_close_output_stream;
    adev->hw_device.open_input_stream = adev_open_input_stream;
    adev->hw_device.close_input_stream = adev_close_input_stream;
    adev->hw_device.dump = adev_dump;

    adev->mixer = mixer_open(CARD_DEFAULT);
    if (!adev->mixer) {
        free(adev);
        ALOGE("Unable to open the mixer, aborting.");
        return -EINVAL;
    }

    ret = adev_config_parse(adev);
    if (ret != 0)
        goto err_mixer;

    /* Set the default route before the PCM stream is opened */
    pthread_mutex_lock(&adev->lock);
    adev->mode = AUDIO_MODE_NORMAL;
    adev->out_device = AUDIO_DEVICE_OUT_SPEAKER;
    adev->in_device = AUDIO_DEVICE_NONE;
    select_devices(adev);

    adev->pcm_modem_dl = NULL;
    adev->pcm_modem_ul = NULL;
    adev->voice_volume = 1.0f;
    adev->tty_mode = TTY_MODE_OFF;
    adev->bluetooth_nrec = true;
    adev->wb_amr = 0;

    pthread_mutex_unlock(&adev->lock);
    *device = &adev->hw_device.common;
    return 0;

err_mixer:
    mixer_close(adev->mixer);
err:
    return -EINVAL;
}

static struct hw_module_methods_t hal_module_methods = {
    .open = adev_open,
};

struct audio_module HAL_MODULE_INFO_SYM = {
    .common = {
        .tag = HARDWARE_MODULE_TAG,
        .module_api_version = AUDIO_MODULE_API_VERSION_0_1,
        .hal_api_version = HARDWARE_HAL_API_VERSION,
        .id = AUDIO_HARDWARE_MODULE_ID,
        .name = "M0 audio HW HAL",
        .author = "",
        .methods = &hal_module_methods,
    },
};
