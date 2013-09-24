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

#define LOG_TAG "audio_hw_primary"
#define LOG_NDEBUG 0

#include <errno.h>
#include <pthread.h>
#include <stdint.h>
#include <sys/time.h>
#include <stdlib.h>
#include <expat.h>

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

struct pcm_config pcm_config_playback = {
    .channels = 2,
    .rate = DEFAULT_OUT_SAMPLING_RATE,
    .period_count = PLAYBACK_PERIOD_COUNT,
    .period_size = PLAYBACK_PERIOD_SIZE,
    .format = PCM_FORMAT_S16_LE,
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

struct mixer_ctls
{
    struct mixer_ctl *dai_mode;
    struct mixer_ctl *codec;
    struct mixer_ctl *stdac;
    struct mixer_ctl *ext;
    struct mixer_ctl *spkr_dac_l;
    struct mixer_ctl *spkr_dac_r;
    struct mixer_ctl *ep_cdc;
    struct mixer_ctl *cap_l;
    struct mixer_ctl *cap_r;
};

struct m0_audio_device {
    struct audio_hw_device hw_device;

    pthread_mutex_t lock;       /* see note below on mutex acquisition order */
    struct m0_dev_cfg *dev_cfgs;
    int num_dev_cfgs;
    struct mixer *mixer;
    struct mixer_ctls mixer_ctls;
    int mode;
    int active_devices;
    int devices;
    struct pcm *pcm_modem_dl;
    struct pcm *pcm_modem_ul;
    int in_call;
    float voice_volume;
    struct m0_stream_in *active_input;
    struct m0_stream_out *active_output;
    bool mic_mute;
    int tty_mode;
    struct echo_reference_itfe *echo_reference;
    bool bluetooth_nrec;
    int wb_amr;
    bool screen_state;

    /* RIL */
  //  struct ril_handle ril;
};

struct m0_stream_out {
    struct audio_stream_out stream;

    pthread_mutex_t lock;       /* see note below on mutex acquisition order */
    struct pcm_config config;
    struct pcm *pcm;
    struct resampler_itfe *resampler;
    char *buffer;
    int standby;
    struct echo_reference_itfe *echo_reference;
    struct m0_audio_device *dev;
    int write_threshold;
    bool screen_state;
};

#define MAX_PREPROCESSORS 3 /* maximum one AGC + one NS + one AEC per input stream */

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
    effect_handle_t preprocessors[MAX_PREPROCESSORS];

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

/* The enable flag when 0 makes the assumption that enums are disabled by
 * "Off" and integers/booleans by 0 */
static int set_bigroute_by_array(struct mixer *mixer, struct route_setting *route,
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

/* Must be called with lock */
void select_devices(struct m0_audio_device *adev)
{
    int i;

    if (adev->active_devices == adev->devices) {
    ALOGV("Keeping device %x => %x\n", adev->active_devices, adev->devices);
    for (i = 0; i < adev->num_dev_cfgs; i++)
    if ((adev->devices & adev->dev_cfgs[i].mask) &&
        !(adev->active_devices & adev->dev_cfgs[i].mask))
        set_route_by_array(adev->mixer, adev->dev_cfgs[i].on,
                   adev->dev_cfgs[i].on_len);
    return;
}
    ALOGV("Changing devices %x => %x\n", adev->active_devices, adev->devices);

    /* Turn on new devices first so we don't glitch due to powerdown... */
    for (i = 0; i < adev->num_dev_cfgs; i++)
    if ((adev->devices & adev->dev_cfgs[i].mask) &&
        !(adev->active_devices & adev->dev_cfgs[i].mask))
        set_route_by_array(adev->mixer, adev->dev_cfgs[i].on,
                   adev->dev_cfgs[i].on_len);

    /* ...then disable old ones. */
    for (i = 0; i < adev->num_dev_cfgs; i++)
    if (!(adev->devices & adev->dev_cfgs[i].mask) &&
        (adev->active_devices & adev->dev_cfgs[i].mask))
        set_route_by_array(adev->mixer, adev->dev_cfgs[i].off,
                   adev->dev_cfgs[i].off_len);

    adev->active_devices = adev->devices;
}

static int start_call(struct m0_audio_device *adev)
{
    ALOGD("%s: E", __func__);
    ALOGE("Opening modem PCMs");
    int bt_on;

    bt_on = adev->devices & AUDIO_DEVICE_OUT_ALL_SCO;
    pcm_config_vx.rate = adev->wb_amr ? VX_WB_SAMPLING_RATE : VX_NB_SAMPLING_RATE;

    ALOGD("%s: X", __func__);

    return 0;

}

static void end_call(struct m0_audio_device *adev)
{
    ALOGD("%s: E", __func__);
    ALOGD("%s: X", __func__);
}

static void set_eq_filter(struct m0_audio_device *adev)
{
}

void audio_set_wb_amr_callback(void *data, int enable)
{
    ALOGD("%s: E", __func__);
    struct m0_audio_device *adev = (struct m0_audio_device *)data;

    pthread_mutex_lock(&adev->lock);
    if (adev->wb_amr != enable) {
        adev->wb_amr = enable;

        /* reopen the modem PCMs at the new rate */
        if (adev->in_call) {
            end_call(adev);
            set_eq_filter(adev);
            start_call(adev);
        }
    }
    pthread_mutex_unlock(&adev->lock);
    ALOGD("%s: X", __func__);
}

static void set_incall_device(struct m0_audio_device *adev)
{
    ALOGD("%s: E", __func__);
    int device_type;

    switch(adev->devices & AUDIO_DEVICE_OUT_ALL) {
        case AUDIO_DEVICE_OUT_EARPIECE:
            device_type = SOUND_AUDIO_PATH_HANDSET;
            break;
        case AUDIO_DEVICE_OUT_SPEAKER:
        case AUDIO_DEVICE_OUT_AUX_DIGITAL:
        case AUDIO_DEVICE_OUT_DGTL_DOCK_HEADSET:
            device_type = SOUND_AUDIO_PATH_SPEAKER;
            break;
        case AUDIO_DEVICE_OUT_WIRED_HEADSET:
            device_type = SOUND_AUDIO_PATH_HEADSET;
            break;
        case AUDIO_DEVICE_OUT_WIRED_HEADPHONE:
            device_type = SOUND_AUDIO_PATH_HEADPHONE;
            break;
        case AUDIO_DEVICE_OUT_BLUETOOTH_SCO:
        case AUDIO_DEVICE_OUT_BLUETOOTH_SCO_HEADSET:
        case AUDIO_DEVICE_OUT_BLUETOOTH_SCO_CARKIT:
            if (adev->bluetooth_nrec) {
                device_type = SOUND_AUDIO_PATH_BLUETOOTH;
            } else {
                device_type = SOUND_AUDIO_PATH_BLUETOOTH_NO_NR;
            }
            break;
        default:
            device_type = SOUND_AUDIO_PATH_HANDSET;
            break;
    }

    /* if output device isn't supported, open modem side to handset by default */
   // ALOGE("%s: ril_set_call_audio_path(%d)", __func__, device_type);
  //  ril_set_call_audio_path(&adev->ril, device_type);
   // ALOGD("%s: X", __func__);
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

    if (adev->active_output) {
        out = adev->active_output;
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
    ALOGD("%s: E", __func__);
    if (adev->mode == AUDIO_MODE_IN_CALL) {
        ALOGE("Entering IN_CALL state, in_call=%d", adev->in_call);
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
            if ((adev->devices & AUDIO_DEVICE_OUT_ALL) == AUDIO_DEVICE_OUT_SPEAKER)
                adev->devices = AUDIO_DEVICE_OUT_EARPIECE |
                                AUDIO_DEVICE_IN_BUILTIN_MIC;
            else
                adev->devices &= ~AUDIO_DEVICE_OUT_SPEAKER;

            select_output_device(adev);
            start_call(adev);
            ALOGD("%s: set bigroute: voicecall_daimode", __func__);
            set_bigroute_by_array(adev->mixer, voicecall_daimode, 1);
          //  ril_set_call_clock_sync(&adev->ril, SOUND_CLOCK_START);
            adev_set_voice_volume(&adev->hw_device, adev->voice_volume);
            adev->in_call = 1;
        }
    } else {
        ALOGE("Leaving IN_CALL state, in_call=%d, mode=%d",
             adev->in_call, adev->mode);
        if (adev->in_call) {
            adev->in_call = 0;
            end_call(adev);
            force_all_standby(adev);
            select_output_device(adev);
            select_input_device(adev);
        }
    }
    ALOGD("%s: X", __func__);
}

static void select_output_device(struct m0_audio_device *adev)
{
    ALOGD("%s: E", __func__);
    int headset_on;
    int headphone_on;
    int speaker_on;
    int earpiece_on;
    int bt_on;
    bool tty_volume = false;
    unsigned int channel = 0;

    headset_on = adev->devices & AUDIO_DEVICE_OUT_WIRED_HEADSET;
    headphone_on = adev->devices & AUDIO_DEVICE_OUT_WIRED_HEADPHONE;
    speaker_on = adev->devices & AUDIO_DEVICE_OUT_SPEAKER;
    earpiece_on = adev->devices & AUDIO_DEVICE_OUT_EARPIECE;
    bt_on = adev->devices & AUDIO_DEVICE_OUT_ALL_SCO;

    switch(adev->devices & AUDIO_DEVICE_OUT_ALL) {
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
        default:
            ALOGD("%s: DEFAULT OUTPUT", __func__);
            break;
    }

    select_devices(adev);

    if (adev->mode == AUDIO_MODE_IN_CALL) {
        if (!bt_on) {
            /* force tx path according to TTY mode when in call */
            switch(adev->tty_mode) {
                case TTY_MODE_FULL:
                case TTY_MODE_HCO:
                    /* tx path from headset mic */
                    headphone_on = 0;
                    headset_on = 1;
                    speaker_on = 0;
                    earpiece_on = 0;
                    break;
                case TTY_MODE_VCO:
                    /* tx path from device sub mic */
                    headphone_on = 0;
                    headset_on = 0;
                    speaker_on = 1;
                    earpiece_on = 0;
                    break;
                case TTY_MODE_OFF:
                default:
                    break;
            }
        }

        if (headset_on || headphone_on || speaker_on || earpiece_on) {
            ALOGD("%s: set bigroute: voicecall_input_default", __func__);
            set_bigroute_by_array(adev->mixer, voicecall_default, 1);
        } else {
            ALOGD("%s: set bigroute: voicecall_input_default_disable", __func__);
            set_bigroute_by_array(adev->mixer, voicecall_default_disable, 1);
        }

        if (headset_on || headphone_on) {
            ALOGD("%s: set bigroute: headset_input", __func__);
            set_bigroute_by_array(adev->mixer, headset_input, 1);
        }

        if (bt_on) {
            // bt uses a different port (PORT_BT) for playback, reopen the pcms
            end_call(adev);
            start_call(adev);
            ALOGD("%s: set bigroute: bt_input", __func__);
            set_bigroute_by_array(adev->mixer, bt_input, 1);
            ALOGD("%s: set bigroute: bt_output", __func__);
            set_bigroute_by_array(adev->mixer, bt_output, 1);
        }
        set_incall_device(adev);
    }
    set_bigroute_by_array(adev->mixer, mm_default, 1);
    ALOGD("%s: X", __func__);
}

static void select_input_device(struct m0_audio_device *adev)
{
    ALOGD("%s: E", __func__);

    switch(adev->devices & AUDIO_DEVICE_IN_ALL) {
        case AUDIO_DEVICE_IN_BUILTIN_MIC:
            ALOGD("%s: AUDIO_DEVICE_IN_BUILTIN_MIC", __func__);
            break;
        case AUDIO_DEVICE_IN_BACK_MIC:
            ALOGD("%s: AUDIO_DEVICE_IN_BACK_MIC", __func__);
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
    ALOGD("%s: X", __func__);
}

/* must be called with hw device and output stream mutexes locked */
static int start_output_stream(struct m0_stream_out *out)
{
    ALOGD("%s: E", __func__);
    struct m0_audio_device *adev = out->dev;
    unsigned int flags = PCM_OUT | PCM_MMAP;
    int i;
    bool success = true;

    adev->active_output = out;

    if (adev->mode != AUDIO_MODE_IN_CALL) {
        /* FIXME: only works if only one output can be active at a time */
        select_output_device(adev);
    }

    out->config = pcm_config_playback;
    out->config.rate = DEFAULT_OUT_SAMPLING_RATE;
    out->pcm = pcm_open(CARD_DEFAULT, PORT_PLAYBACK, flags, &out->config);

    /* Close PCM that could not be opened properly and return an error */
    if (out->pcm && !pcm_is_ready(out->pcm)) {
        ALOGE("cannot open pcm_out driver: %s", pcm_get_error(out->pcm));
        pcm_close(out->pcm);
        out->pcm = NULL;
        success = false;
    }

    if (success) {
        if (adev->echo_reference != NULL)
            out->echo_reference = adev->echo_reference;
        out->resampler->reset(out->resampler);

        return 0;
    }

    adev->active_output = NULL;
    ALOGD("%s: X", __func__);
    return -ENOMEM;
}

static int check_input_parameters(uint32_t sample_rate, int format, int channel_count)
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

static size_t get_input_buffer_size(uint32_t sample_rate, int format, int channel_count)
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
        if (adev->active_output != NULL)
            remove_echo_reference(adev->active_output, reference);
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
    if (adev->active_output != NULL) {
        struct audio_stream *stream = &adev->active_output->stream.common;
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
            add_echo_reference(adev->active_output, adev->echo_reference);
    }
    return adev->echo_reference;
}

static int get_playback_delay(struct m0_stream_out *out,
                       size_t frames,
                       struct echo_reference_buffer *buffer)
{
    size_t kernel_frames;
    int status;

    status = pcm_get_htimestamp(out->pcm, &kernel_frames, &buffer->time_stamp);
    if (status < 0) {
        buffer->time_stamp.tv_sec  = 0;
        buffer->time_stamp.tv_nsec = 0;
        buffer->delay_ns           = 0;
        ALOGV("%s: pcm_get_htimestamp error,"
                "setting playbackTimestamp to 0", __func__);
        return status;
    }

    kernel_frames = pcm_get_buffer_size(out->pcm) - kernel_frames;

    /* adjust render time stamp with delay added by current driver buffer.
     * Add the duration of current frame as we want the render time of the last
     * sample being written. */
    buffer->delay_ns = (long)(((int64_t)(kernel_frames + frames)* 1000000000)/
                            DEFAULT_OUT_SAMPLING_RATE);

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

static size_t out_get_buffer_size(const struct audio_stream *stream)
{
    struct m0_stream_out *out = (struct m0_stream_out *)stream;

    /* take resampling into account and return the closest majoring
    multiple of 16 frames, as audioflinger expects audio buffers to
    be a multiple of 16 frames. Note: we use the default rate here
    from pcm_config_playback.rate. */
    size_t size = (PLAYBACK_PERIOD_SIZE * DEFAULT_OUT_SAMPLING_RATE) / pcm_config_playback.rate;
    size = ((size + 15) / 16) * 16;
    return size * audio_stream_frame_size((struct audio_stream *)stream);
}

static audio_channel_mask_t out_get_channels(const struct audio_stream *stream)
{
    return AUDIO_CHANNEL_OUT_STEREO;
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

    if (!out->standby) {

        if (out->pcm) {
            pcm_close(out->pcm);
            out->pcm = NULL;
        }

        adev->active_output = 0;

        /* stop writing to echo reference */
        if (out->echo_reference != NULL) {
            out->echo_reference->write(out->echo_reference, NULL);
            out->echo_reference = NULL;
        }

        out->standby = 1;
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
    ALOGD("%s: E", __func__);
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
        if (((adev->devices & AUDIO_DEVICE_OUT_ALL) != val) && (val != 0)) {
            if (out == adev->active_output) {
                /* a change in output device may change the microphone selection */
                if (adev->active_input &&
                        adev->active_input->source == AUDIO_SOURCE_VOICE_COMMUNICATION) {
                    force_input_standby = true;
                }
            }
            adev->devices &= ~AUDIO_DEVICE_OUT_ALL;
            adev->devices |= val;
            select_output_device(adev);
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
    ALOGD("%s: X", __func__);
    return ret;
}

static char * out_get_parameters(const struct audio_stream *stream, const char *keys)
{
    return strdup("");
}

static uint32_t out_get_latency(const struct audio_stream_out *stream)
{
    struct m0_stream_out *out = (struct m0_stream_out *)stream;

    /*  Note: we use the default rate here from pcm_config_playback.rate */
    return (PLAYBACK_PERIOD_SIZE * PLAYBACK_PERIOD_COUNT * 1000) / pcm_config_playback.rate;
}

static int out_set_volume(struct audio_stream_out *stream, float left,
                          float right)
{
    return -ENOSYS;
}

static ssize_t out_write(struct audio_stream_out *stream, const void* buffer,
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
    bool screen_state;
    int kernel_frames;
    void *buf;
    /* If we're in out_write, we will find at least one pcm active */
    int primary_pcm = -1;
    int i;
    bool use_resampler = false;
    int period_size = 0;

    /* acquiring hw device mutex systematically is useful if a low priority thread is waiting
     * on the output stream mutex - e.g. executing select_mode() while holding the hw device
     * mutex
     */
    pthread_mutex_lock(&adev->lock);
    pthread_mutex_lock(&out->lock);
    if (out->standby) {
        ret = start_output_stream(out);
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

    out->write_threshold = PLAYBACK_PERIOD_SIZE * PLAYBACK_PERIOD_COUNT;

    if (out->pcm) {
        if (out->config.rate != DEFAULT_OUT_SAMPLING_RATE)
            use_resampler = true;
    }

    /* only use resampler if required */
    if (use_resampler)
        out->resampler->resample_from_input(out->resampler,
                                            (int16_t *)buffer,
                                            &in_frames,
                                            (int16_t *)out->buffer,
                                            &out_frames);
    else
        out_frames = in_frames;

    if (out->echo_reference != NULL) {
        struct echo_reference_buffer b;
        b.raw = (void *)buffer;
        b.frame_count = in_frames;

        get_playback_delay(out, out_frames, &b);
        out->echo_reference->write(out->echo_reference, &b);
    }

    /* do not allow more than out->write_threshold frames in kernel pcm driver buffer */
    do {
        struct timespec time_stamp;

        if (pcm_get_htimestamp(out->pcm, (unsigned int *)&kernel_frames, &time_stamp) < 0)
            break;
        kernel_frames = pcm_get_buffer_size(out->pcm) - kernel_frames;

        if (kernel_frames > out->write_threshold) {
            unsigned long time = (unsigned long)
                    (((int64_t)(kernel_frames - out->write_threshold) * 1000000) /
                            DEFAULT_OUT_SAMPLING_RATE);
            if (time < MIN_WRITE_SLEEP_US)
                time = MIN_WRITE_SLEEP_US;
            usleep(time);
        }
    } while (kernel_frames > out->write_threshold);

    /* Write to all active PCMs */

    if (out->pcm) {
        if (out->config.rate == DEFAULT_OUT_SAMPLING_RATE) {
            /* PCM uses native sample rate */
            ret = pcm_mmap_write(out->pcm, (void *)buffer, bytes);
        } else {
            /* PCM needs resampler */
            ret = pcm_mmap_write(out->pcm, (void *)out->buffer, out_frames * frame_size);
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
    ALOGD("%s: E", __func__);
    int ret = 0;
    struct m0_audio_device *adev = in->dev;

    adev->active_input = in;

    if (adev->mode != AUDIO_MODE_IN_CALL) {
        adev->devices &= ~AUDIO_DEVICE_IN_ALL;
        adev->devices |= in->device;
        select_input_device(adev);
    }

    /* in case channel count has changed, restart the resampler */
    if (in->resampler) {
        release_resampler(in->resampler);
        in->resampler = NULL;
        ret = create_resampler(in->config.rate,
                               in->requested_rate,
                               in->config.channels,
                               RESAMPLER_QUALITY_DEFAULT,
                               &in->buf_provider,
                               &in->resampler);
    }

    if (in->need_echo_reference && in->echo_reference == NULL)
        in->echo_reference = get_echo_reference(adev,
                                        AUDIO_FORMAT_PCM_16_BIT,
                                        in->config.channels,
                                        in->requested_rate);

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
    ALOGD("%s: X", __func__);
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
                                 in->config.channels);
}

static audio_channel_mask_t in_get_channels(const struct audio_stream *stream)
{
    struct m0_stream_in *in = (struct m0_stream_in *)stream;

    if (in->config.channels == 1) {
        return AUDIO_CHANNEL_IN_MONO;
    } else {
        return AUDIO_CHANNEL_IN_STEREO;
    }
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
            adev->devices &= ~AUDIO_DEVICE_IN_ALL;
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
    ALOGD("%s: E", __func__);
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
        val = atoi(value);
        if ((in->device != val) && (val != 0)) {
            in->device = val;
            do_standby = true;
        }
    }

    if (do_standby)
        do_input_standby(in);
    pthread_mutex_unlock(&in->lock);
    pthread_mutex_unlock(&adev->lock);

    str_parms_destroy(parms);
    ALOGD("%s: X", __func__);
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
        if ((*in->preprocessors[i])->process_reverse == NULL)
            continue;

        (*in->preprocessors[i])->process_reverse(in->preprocessors[i],
                                               &buf,
                                               NULL);
        set_preprocessor_echo_delay(in->preprocessors[i], delay_us);
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
            ALOGI("%s: read_buf %p extended to %d bytes",
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

    while (frames_wr < frames) {
        /* first reload enough frames at the end of process input buffer */
        if (in->proc_buf_frames < (size_t)frames) {
            ssize_t frames_rd;

            if (in->proc_buf_size < (size_t)frames) {
                size_t size_in_bytes = pcm_frames_to_bytes(in->pcm, frames);

                in->proc_buf_size = (size_t)frames;
                in->proc_buf_in = (int16_t *)realloc(in->proc_buf_in, size_in_bytes);
                ALOGD("%s: proc_buf_in %p extended to %d bytes", __func__, in->proc_buf_in, size_in_bytes);
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
        out_buf.s16 = (int16_t *)buffer + frames_wr * in->config.channels;

        for (i = 0; i < in->num_preprocessors; i++)
            (*in->preprocessors[i])->process(in->preprocessors[i],
                                               &in_buf,
                                               &out_buf);

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
        if (out_buf.frameCount == 0)
            continue;

        if ((frames_wr + (ssize_t)out_buf.frameCount) <= frames) {
            frames_wr += out_buf.frameCount;
        } else {
            /* The effect does not comply to the API. In theory, we should never end up here! */
            ALOGE("%s: preprocessing produced too many frames: %d + %d  > %d !", __func__,
                  (unsigned int)frames_wr, out_buf.frameCount, (unsigned int)frames);
            frames_wr = frames;
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

    in->preprocessors[in->num_preprocessors++] = effect;

    if (memcmp(&desc.type, FX_IID_AEC, sizeof(effect_uuid_t)) == 0) {
        in->need_echo_reference = true;
        do_input_standby(in);
    }

exit:

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
    bool found = false;
    effect_descriptor_t desc;

    pthread_mutex_lock(&in->dev->lock);
    pthread_mutex_lock(&in->lock);
    if (in->num_preprocessors <= 0) {
        status = -ENOSYS;
        goto exit;
    }

    for (i = 0; i < in->num_preprocessors; i++) {
        if (found) {
            in->preprocessors[i - 1] = in->preprocessors[i];
            continue;
        }
        if (in->preprocessors[i] == effect) {
            in->preprocessors[i] = NULL;
            status = 0;
            found = true;
        }
    }

    if (status != 0)
        goto exit;

    in->num_preprocessors--;

    status = (*effect)->get_descriptor(effect, &desc);
    if (status != 0)
        goto exit;
    if (memcmp(&desc.type, FX_IID_AEC, sizeof(effect_uuid_t)) == 0) {
        in->need_echo_reference = false;
        do_input_standby(in);
    }

exit:

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

    out = (struct m0_stream_out *)calloc(1, sizeof(struct m0_stream_out));
    if (!out)
        return -ENOMEM;

    ret = create_resampler(DEFAULT_OUT_SAMPLING_RATE,
                           DEFAULT_OUT_SAMPLING_RATE,
                           2,
                           RESAMPLER_QUALITY_DEFAULT,
                           NULL,
                           &out->resampler);
    if (ret != 0)
        goto err_open;
    out->buffer = malloc(RESAMPLER_BUFFER_SIZE); /* todo: allow for reallocing */

    out->stream.common.get_sample_rate = out_get_sample_rate;
    out->stream.common.set_sample_rate = out_set_sample_rate;
    out->stream.common.get_buffer_size = out_get_buffer_size;
    out->stream.common.get_channels = out_get_channels;
    out->stream.common.get_format = out_get_format;
    out->stream.common.set_format = out_set_format;
    out->stream.common.standby = out_standby;
    out->stream.common.dump = out_dump;
    out->stream.common.set_parameters = out_set_parameters;
    out->stream.common.get_parameters = out_get_parameters;
    out->stream.common.add_audio_effect = out_add_audio_effect;
    out->stream.common.remove_audio_effect = out_remove_audio_effect;
    out->stream.get_latency = out_get_latency;
    out->stream.set_volume = out_set_volume;
    out->stream.write = out_write;
    out->stream.get_render_position = out_get_render_position;

    out->dev = ladev;
    out->standby = 1;

    /* FIXME: when we support multiple output devices, we will want to
     * do the following:
     * adev->devices &= ~AUDIO_DEVICE_OUT_ALL;
     * adev->devices |= out->device;
     * select_devices(adev);
     * This is because out_set_parameters() with a route is not
     * guaranteed to be called after an output stream is opened. */

    config->format = out_get_format(&out->stream.common);
    config->channel_mask = out_get_channels(&out->stream.common);
    config->sample_rate = out_get_sample_rate(&out->stream.common);


    *stream_out = &out->stream;
    return 0;

err_open:
    free(out);
    *stream_out = NULL;
    return ret;
}

static void adev_close_output_stream(struct audio_hw_device *dev,
                                     struct audio_stream_out *stream)
{
    struct m0_stream_out *out = (struct m0_stream_out *)stream;

    out_standby(&stream->common);
    if (out->buffer)
        free(out->buffer);
    if (out->resampler)
        release_resampler(out->resampler);
    free(stream);
}

static int adev_set_parameters(struct audio_hw_device *dev, const char *kvpairs)
{
    ALOGD("%s: E", __func__);
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

    ret = str_parms_get_str(parms, "screen_state", value, sizeof(value));
    if (ret >= 0) {
        if (strcmp(value, AUDIO_PARAMETER_VALUE_ON) == 0)
            adev->screen_state = false;
        else
            adev->screen_state = true;
    }

    str_parms_destroy(parms);
    ALOGD("%s: X", __func__);
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

    adev->voice_volume = volume;

   // if (adev->mode == AUDIO_MODE_IN_CALL)
      //  ril_set_call_volume(&adev->ril, SOUND_TYPE_VOICE, volume);

    return 0;
}

static int adev_set_master_volume(struct audio_hw_device *dev, float volume)
{
    return -ENOSYS;
}

static int adev_set_mode(struct audio_hw_device *dev, int mode)
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
    ALOGD("%s: E", __func__);
    struct m0_audio_device *ladev = (struct m0_audio_device *)dev;
    struct m0_stream_in *in;
    int ret;
    int channel_count = popcount(config->channel_mask);


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
    in->device = devices;

    *stream_in = &in->stream;
    ALOGD("%s: X", __func__);
    return 0;

err:
    if (in->resampler)
        release_resampler(in->resampler);

    free(in);
    *stream_in = NULL;
    return ret;
}

static void adev_close_input_stream(struct audio_hw_device *dev,
                                   struct audio_stream_in *stream)
{
    struct m0_stream_in *in = (struct m0_stream_in *)stream;

    in_standby(&stream->common);

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

    /* RIL */
  //  ril_close(&adev->ril);

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
    { AUDIO_DEVICE_OUT_ANLG_DOCK_HEADSET, "analog-dock" },
    { AUDIO_DEVICE_OUT_DGTL_DOCK_HEADSET, "digital-dock" },
    { AUDIO_DEVICE_OUT_ALL_SCO, "sco-out" },

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

    adev->mixer = mixer_open(0);
    if (!adev->mixer) {
        free(adev);
        ALOGE("Unable to open the mixer, aborting.");
        return -EINVAL;
    }

    adev->mixer_ctls.dai_mode = mixer_get_ctl_by_name(adev->mixer, "DAI Mode");
    adev->mixer_ctls.codec = mixer_get_ctl_by_name(adev->mixer, "CPCAP Mixer Voice Codec");
    adev->mixer_ctls.stdac = mixer_get_ctl_by_name(adev->mixer, "CPCAP Mixer Stereo DAC");
    adev->mixer_ctls.ext = mixer_get_ctl_by_name(adev->mixer, "CPCAP Mixer External PGA");

    adev->mixer_ctls.spkr_dac_l = mixer_get_ctl_by_name(adev->mixer, "LDSPLDAC Switch");
    adev->mixer_ctls.spkr_dac_r = mixer_get_ctl_by_name(adev->mixer, "LDSPRDAC Switch");

    adev->mixer_ctls.ep_cdc = mixer_get_ctl_by_name(adev->mixer, "EPCDC Switch");

    adev->mixer_ctls.cap_l = mixer_get_ctl_by_name(adev->mixer, "Analog Left Capture Route");
    adev->mixer_ctls.cap_r = mixer_get_ctl_by_name(adev->mixer, "Analog Right Capture Route");

	ret = adev_config_parse(adev);
	if (ret != 0)
		goto err_mixer;

    /* Set the default route before the PCM stream is opened */
    pthread_mutex_init(&adev->lock, NULL);
    adev->mode = AUDIO_MODE_NORMAL;
    adev->devices = AUDIO_DEVICE_OUT_SPEAKER | AUDIO_DEVICE_IN_BUILTIN_MIC;
    select_devices(adev);

    adev->pcm_modem_dl = NULL;
    adev->pcm_modem_ul = NULL;
    adev->voice_volume = 1.0f;
    adev->tty_mode = TTY_MODE_OFF;
    adev->bluetooth_nrec = true;
    adev->wb_amr = 0;

    /* RIL */
   // ril_open(&adev->ril);
    pthread_mutex_unlock(&adev->lock);
    /* register callback for wideband AMR setting */
  //  ril_register_set_wb_amr_callback(audio_set_wb_amr_callback, (void *)adev);

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
        .name = "Mapphone audio HAL",
        .author = "The Android Open Source Project",
        .methods = &hal_module_methods,
    },
};
