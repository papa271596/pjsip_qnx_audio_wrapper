/*
 * Copyright (C) 2009-2011 Teluu Inc. (http://www.teluu.com)
 * Copyright (C) 2007-2009 Keystream AB and Konftel AB, All rights reserved.
 *                         Author: <dan.aberg@keystream.se>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
#include <pjmedia_audiodev.h>
#include <pj/assert.h>
#include <pj/log.h>
#include <pj/os.h>
#include <pj/pool.h>
#include <pjmedia/errno.h>
#include <pjmedia-audiodev/qnx_dev.h>
#include <stddef.h>

#if defined(PJMEDIA_AUDIO_DEV_HAS_QNX) && PJMEDIA_AUDIO_DEV_HAS_QNX

// #include <sys/syscall.h>
// #include "qnx_alsa_wrapper.h" // Include the QNX ALSA wrapper header
#include <sys/time.h>
#include <sys/neutrino.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/select.h>
#include <pthread.h>
#include <errno.h>
// #include <sys/asoundlib.h>


#define THIS_FILE                       "qnx_dev.c"
#define ALSA_DEVICE_NAME                "plughw:%d,%d"
#define ALSASOUND_PLAYBACK              1
#define ALSASOUND_CAPTURE               2
#define MAX_SOUND_CARDS                 5
#define MAX_SOUND_DEVICES_PER_CARD      5
#define MAX_DEVICES                     PJMEDIA_AUD_DEV_MAX_DEVS
#define MAX_MIX_NAME_LEN                64 

/* Set to 1 to enable tracing */
#define ENABLE_TRACING                  0

#if ENABLE_TRACING
#       define TRACE_(expr)             PJ_LOG(5,expr)
#else
#       define TRACE_(expr)
#endif

/*
 * Factory prototypes
 */
static pj_status_t qnx_factory_init(pjmedia_aud_dev_factory *f);
static pj_status_t qnx_factory_destroy(pjmedia_aud_dev_factory *f);
static pj_status_t qnx_factory_refresh(pjmedia_aud_dev_factory *f);
static unsigned    qnx_factory_get_dev_count(pjmedia_aud_dev_factory *f);
static pj_status_t qnx_factory_get_dev_info(pjmedia_aud_dev_factory *f,
                                             unsigned index,
                                             pjmedia_aud_dev_info *info);
static pj_status_t qnx_factory_default_param(pjmedia_aud_dev_factory *f,
                                              unsigned index,
                                              pjmedia_aud_param *param);
static pj_status_t qnx_factory_create_stream(pjmedia_aud_dev_factory *f,
                                              const pjmedia_aud_param *param,
                                              pjmedia_aud_rec_cb rec_cb,
                                              pjmedia_aud_play_cb play_cb,
                                              void *user_data,
                                              pjmedia_aud_stream **p_strm);

/*
 * Stream prototypes
 */
static pj_status_t qnx_stream_get_param(pjmedia_aud_stream *strm,
                                         pjmedia_aud_param *param);
static pj_status_t qnx_stream_get_cap(pjmedia_aud_stream *strm,
                                       pjmedia_aud_dev_cap cap,
                                       void *value);
static pj_status_t qnx_stream_set_cap(pjmedia_aud_stream *strm,
                                       pjmedia_aud_dev_cap cap,
                                       const void *value);
static pj_status_t qnx_stream_start(pjmedia_aud_stream *strm);
static pj_status_t qnx_stream_stop(pjmedia_aud_stream *strm);
static pj_status_t qnx_stream_destroy(pjmedia_aud_stream *strm);


struct qnx_factory
{
    pjmedia_aud_dev_factory      base;
    pj_pool_factory             *pf;
    pj_pool_t                   *pool;
    pj_pool_t                   *base_pool;

    unsigned                     dev_cnt;
    pjmedia_aud_dev_info         devs[MAX_DEVICES];
    char                         pb_mixer_name[MAX_MIX_NAME_LEN];
    char                         cap_mixer_name[MAX_MIX_NAME_LEN];

    unsigned                     custom_dev_cnt;
    pj_str_t                     custom_dev[MAX_DEVICES];
};

static pjmedia_aud_dev_factory *default_factory;

struct qnx_stream
{
    pjmedia_aud_stream   base;

    /* Common */
    pj_pool_t           *pool;
    struct qnx_factory *af;
    void                *user_data;
    pjmedia_aud_param    param;         /* Running parameter            */
    int                  rec_id;        /* Capture device id            */
    int                  quit;

    /* Playback */
    snd_pcm_t           *pb_pcm;
    qnx_snd_pcm_uframes_t    pb_frames;     /* samples_per_frame            */
    pjmedia_aud_play_cb  pb_cb;
    unsigned             pb_buf_size;
    pj_uint8_t          *pb_buf;
    pj_thread_t         *pb_thread;

    /* Capture */
    snd_pcm_t           *ca_pcm;
    qnx_snd_pcm_uframes_t    ca_frames;     /* samples_per_frame            */
    pjmedia_aud_rec_cb   ca_cb;
    unsigned             ca_buf_size;
    pj_uint8_t          *ca_buf;
    pj_thread_t         *ca_thread;
    

    // QNX specific
    qnx_dev_id pb_dev_id;  // Playback device ID
    qnx_dev_id ca_dev_id;  // Capture device ID
    
    snd_pcm_t *pcm_handle;

    unsigned input_vol;
    unsigned output_vol;
    pjmedia_aud_dev_route input_route;
    pjmedia_aud_dev_route output_route;


    
    int frame_count;              // Number of frames per buffer
    pjmedia_port *port;          // PJSIP media port


};

static pjmedia_aud_dev_factory_op qnx_factory_op =
{
    &qnx_factory_init,
    &qnx_factory_destroy,
    &qnx_factory_get_dev_count,
    &qnx_factory_get_dev_info,
    &qnx_factory_default_param,
    &qnx_factory_create_stream,
    &qnx_factory_refresh
};

static pjmedia_aud_stream_op qnx_stream_op =
{
    &qnx_stream_get_param,
    &qnx_stream_get_cap,
    &qnx_stream_set_cap,
    &qnx_stream_start,
    &qnx_stream_stop,
    &qnx_stream_destroy
};

/* Create QNX audio driver. */
pjmedia_aud_dev_factory* pjmedia_qnx_factory(pj_pool_factory *pf)
{
    struct qnx_factory *af;
    pj_pool_t *pool;

    pool = pj_pool_create(pf, "qnx_aud", 512, 512, NULL);
    af = PJ_POOL_ZALLOC_T(pool, struct qnx_factory);
    af->pf = pf;
    af->base_pool = pool;
    af->base.op = &qnx_factory_op;

    return &af->base;
}

/* API: init factory */
static pj_status_t qnx_factory_init(pjmedia_aud_dev_factory *f)
{
    pj_status_t status = qnx_factory_refresh(f);
    if (PJ_SUCCESS != status)
        return status;

    if (!default_factory)
        default_factory = f;

    PJ_LOG(4,(THIS_FILE, "ALSA initialized"));
    return PJ_SUCCESS;
}


/* API: destroy factory */
static pj_status_t qnx_factory_destroy(pjmedia_aud_dev_factory *f)
{
    struct qnx_factory *af = (struct qnx_factory*)f;

    if (default_factory == f)
        default_factory = NULL;

    if (af->pool)
        pj_pool_release(af->pool);

    if (af->base_pool) {
        pj_pool_t *pool = af->base_pool;
        af->base_pool = NULL;
        pj_pool_release(pool);
    }

    /* Restore handler */
    // snd_lib_error_set_handler(NULL);

    return PJ_SUCCESS;
}


/* API: refresh the device list */
static pj_status_t qnx_factory_refresh(pjmedia_aud_dev_factory *f)
{
    // QSA does not support ALSA-style device enumeration.
    // Instead, use known device names or configuration.
    // This function will enumerate QSA PCM devices using preferred open.   

    struct qnx_factory *af = (struct qnx_factory*)f;
    int dev_index = 0;

    PJ_LOG(4, ("qnx_dev", "Enumerating QSA PCM devices using preferred open"));

    // Try opening a few devices in playback and capture mode
    for (int mode = SND_PCM_OPEN_PLAYBACK; mode <= SND_PCM_OPEN_CAPTURE; ++mode)
    {
        snd_pcm_t *pcm_handle;
        snd_pcm_info_t pcm_info;
        int card = -1;
        int device = -1;

        if (snd_pcm_open_preferred(&pcm_handle, &card, &device, mode) == 0)
        {
            if (snd_pcm_info(pcm_handle, &pcm_info) == 0 &&
                dev_index < PJ_ARRAY_SIZE(af->devs))
            {
                pjmedia_aud_dev_info *info = &af->devs[dev_index];
                pj_bzero(info, sizeof(*info));

                pj_ansi_snprintf(info->name, sizeof(info->name),
                                 "QNX PCM Card %d Device %d (%s)",
                                 card, device,
                                 (mode == SND_PCM_OPEN_PLAYBACK ? "Playback" : "Capture"));
                pj_ansi_strcpy(info->driver, "QSA");

                info->input_count = (mode == SND_PCM_OPEN_CAPTURE) ? 1 : 0;
                info->output_count = (mode == SND_PCM_OPEN_PLAYBACK) ? 1 : 0;
                info->caps = PJMEDIA_AUD_DEV_CAP_EXT_FORMAT |
             ((mode == SND_PCM_OPEN_CAPTURE) ? PJMEDIA_AUD_DEV_CAP_INPUT_ROUTE : PJMEDIA_AUD_DEV_CAP_OUTPUT_ROUTE);

                dev_index++;
            }

            snd_pcm_close(pcm_handle);
        }
    }

    af->dev_cnt = dev_index;
    return PJ_SUCCESS;
}


/* API: get device count */
static unsigned  qnx_factory_get_dev_count(pjmedia_aud_dev_factory *f)
{
    struct qnx_factory *af = (struct qnx_factory*)f;
    return af->dev_cnt;
}


/* API: get device info */
static pj_status_t qnx_factory_get_dev_info(pjmedia_aud_dev_factory *f,
                                             unsigned index,
                                             pjmedia_aud_dev_info *info)
{
    
    struct qnx_factory *af = (struct qnx_factory*)f;

    PJ_ASSERT_RETURN(index < af->dev_cnt, PJ_EINVAL);

    pj_memcpy(info, &af->devs[index], sizeof(*info));
    info->caps = PJMEDIA_AUD_DEV_CAP_INPUT_LATENCY |
                 PJMEDIA_AUD_DEV_CAP_OUTPUT_LATENCY |
                 PJMEDIA_AUD_DEV_CAP_INPUT_VOLUME_SETTING |
                 PJMEDIA_AUD_DEV_CAP_OUTPUT_VOLUME_SETTING;

    return PJ_SUCCESS;
}

/* API: create default parameter */
static pj_status_t qnx_factory_default_param(pjmedia_aud_dev_factory *f,
                                              unsigned index,
                                              pjmedia_aud_param *param)
{
    struct qnx_factory *af = (struct qnx_factory*)f;
    pjmedia_aud_dev_info *adi;

    PJ_ASSERT_RETURN(index<af->dev_cnt, PJ_EINVAL);

    adi = &af->devs[index];

    pj_bzero(param, sizeof(*param));
    if (adi->input_count && adi->output_count) {
        param->dir = PJMEDIA_DIR_CAPTURE_PLAYBACK;
        param->rec_id = index;
        param->play_id = index;
    } else if (adi->input_count) {
        param->dir = PJMEDIA_DIR_CAPTURE;
        param->rec_id = index;
        param->play_id = PJMEDIA_AUD_INVALID_DEV;
    } else if (adi->output_count) {
        param->dir = PJMEDIA_DIR_PLAYBACK;
        param->play_id = index;
        param->rec_id = PJMEDIA_AUD_INVALID_DEV;
    } else {
        return PJMEDIA_EAUD_INVDEV;
    }

    param->clock_rate = adi->default_samples_per_sec;
    param->channel_count = 1;
    param->samples_per_frame = adi->default_samples_per_sec * 20 / 1000;
    param->bits_per_sample = 16;
    param->flags = adi->caps;
    param->input_latency_ms = PJMEDIA_SND_DEFAULT_REC_LATENCY;
    param->output_latency_ms = PJMEDIA_SND_DEFAULT_PLAY_LATENCY;

    return PJ_SUCCESS;
}


static int pb_thread_func (void *arg)
{

    /** Code for QSA */
    // This function will handle playback in a separate thread.
    // It will read audio data from the PJSIP port and write it to the QSA playback device.
    struct qnx_stream *stream = (struct qnx_stream*)arg;
    #define BYTES_PER_FRAME (stream->pb_frames * sizeof(pj_int16_t))

    snd_pcm_t *pcm = stream->pb_pcm;
    pj_uint8_t *buf = stream->pb_buf;
    int nframes = stream->frame_count;
    
    pjmedia_frame frame;
    frame.buf = buf;
    frame.size = nframes * BYTES_PER_FRAME;  // Define BYTES_PER_FRAME appropriately
    int result;

    while (!stream->quit) {
        // Fill buffer with audio data from PJSIP
        pjmedia_port_get_frame(stream->port, &frame);

        // Write to QSA playback device
        result = snd_pcm_plugin_write(pcm, frame.buf, nframes);
        if (result < 0) {
            PJ_LOG(3, ("qnx_dev", "Playback write failed: %d", result));
            break;
        }
    }

    // snd_pcm_flush(pcm);  // Optional: flush remaining data
    snd_pcm_close(pcm);  // If you're done with the stream
    return PJ_SUCCESS;



    // struct qnx_stream* stream = (struct qnx_stream*) arg;
    // snd_pcm_t* pcm             = stream->pb_pcm;
    // int size                   = stream->pb_buf_size;
    // qnx_snd_pcm_uframes_t nframes  = stream->pb_frames;
    // void* user_data            = stream->user_data;
    // char* buf                  = stream->pb_buf;
    // pj_timestamp tstamp;
    // int result;

    // pj_bzero (buf, size);
    // tstamp.u64 = 0;

    // TRACE_((THIS_FILE, "pb_thread_func(%u): Started",
    //         (unsigned)gettid()));

    // snd_pcm_prepare (pcm);

    // while (!stream->quit) {
    //     pjmedia_frame frame;

    //     frame.type = PJMEDIA_FRAME_TYPE_AUDIO;
    //     frame.buf = buf;
    //     frame.size = size;
    //     frame.timestamp.u64 = tstamp.u64;
    //     frame.bit_info = 0;

    //     result = stream->pb_cb (user_data, &frame);
    //     if (result != PJ_SUCCESS || stream->quit)
    //         break;

    //     if (frame.type != PJMEDIA_FRAME_TYPE_AUDIO)
    //         pj_bzero (buf, size);

    //     result = snd_pcm_writei (pcm, buf, nframes);
    //     if (result == -EPIPE) {
    //         PJ_LOG (4,(THIS_FILE, "pb_thread_func: underrun!"));
    //         snd_pcm_prepare (pcm);
    //     } else if (result < 0) {
    //         PJ_LOG (4,(THIS_FILE, "pb_thread_func: error writing data!"));
    //     }

    //     tstamp.u64 += nframes;
    // }

    // snd_pcm_drop(pcm);
    // TRACE_((THIS_FILE, "pb_thread_func: Stopped"));
    // return PJ_SUCCESS;
}



static int ca_thread_func (void *arg)
{
    /** Code for QSA */
    // This function will handle audio capture in a separate thread.
    // It will read audio data from the QSA capture device and send it to the PJSIP port.
    
    struct qnx_stream *stream = (struct qnx_stream*)arg;
    #define BYTES_PER_CA_FRAME (stream->ca_frames * sizeof(pj_int16_t))

    snd_pcm_t *pcm = stream->ca_pcm;
    pj_uint8_t *buf = stream->ca_buf;
    int nframes = stream->frame_count;
    int size = stream->ca_buf_size;
    
    pjmedia_frame frame;
    frame.buf = buf;
    frame.size = nframes * BYTES_PER_CA_FRAME; // Define BYTES_PER_CA_FRAME appropriately
    pj_timestamp tstamp;
    int result;
    struct sched_param param;
    pthread_t* thid;

    thid = (pthread_t*) pj_thread_get_os_handle (pj_thread_this());
    param.sched_priority = sched_get_priority_max (SCHED_RR);
    PJ_LOG (5,(THIS_FILE, "ca_thread_func(%u): Set thread priority "
                          "for audio capture thread.",
                          (unsigned)gettid()));
    result = pthread_setschedparam (*thid, SCHED_RR, &param);
    if (result) {
        if (result == EPERM)
            PJ_LOG (5,(THIS_FILE, "Unable to increase thread priority, "
                                  "root access needed."));
        else
            PJ_LOG (5,(THIS_FILE, "Unable to increase thread priority, "
                                  "error: %d",
                                  result));
    }

    pj_bzero (buf, size);
    tstamp.u64 = 0;

    TRACE_((THIS_FILE, "ca_thread_func(%u): Started",
            (unsigned)gettid()));

    while (!stream->quit) {
        pj_bzero (buf, size);
        // Read from QSA capture device
        result = snd_pcm_plugin_read(pcm, buf, nframes);
        if (result < 0) {
            PJ_LOG(3, ("qnx_dev", "Capture read failed: %d", result));
            break;
        }

        // Pass captured data to PJSIP
        pjmedia_port_put_frame(stream->port, &frame);
        tstamp.u64 += nframes;
    }

    // snd_pcm_flush(pcm);  // Optional: flush capture buffer
    snd_pcm_close(pcm);  // If you're done with the stream
    TRACE_((THIS_FILE, "ca_thread_func: Stopped"));
    return PJ_SUCCESS;
}

static pj_status_t open_playback(struct qnx_stream *stream,
                                 unsigned clock_rate,
                                 unsigned channel_count,
                                 unsigned samples_per_frame)
{
    int result;
    snd_pcm_channel_params_t params;
    stream->pcm_handle = stream->pb_pcm;

    // Open PCM playback device
    result = snd_pcm_open(&stream->pb_pcm, stream->pb_dev_id.card,
                          stream->pb_dev_id.device, SND_PCM_OPEN_PLAYBACK);
    if (result < 0) {
        TRACE_((THIS_FILE, "Failed to open playback device"));
        return PJMEDIA_EAUD_SYSERR;
    }

    // Clear and configure playback parameters
    memset(&params, 0, sizeof(params));
    params.channel = SND_PCM_CHANNEL_PLAYBACK;
    params.mode = SND_PCM_MODE_BLOCK;
    params.format.interleave = 1; // Interleaved audio
    params.format.format = SND_PCM_SFMT_S16_LE;
    params.format.rate = clock_rate;
    params.format.voices = channel_count;
    params.start_mode = SND_PCM_START_FULL;
    params.stop_mode = SND_PCM_STOP_ROLLOVER;
    params.buf.block.frag_size = samples_per_frame * channel_count * 2; // 2 bytes per sample
    params.buf.block.frags_max = 4;
    params.buf.block.frags_min = 1;

    // Apply parameters
    result = snd_pcm_plugin_params(stream->pb_pcm, &params);
    if (result < 0) {
        TRACE_((THIS_FILE, "Failed to set playback parameters"));
        snd_pcm_close(stream->pb_pcm);
        return PJMEDIA_EAUD_SYSERR;
    }

    TRACE_((THIS_FILE, "Playback stream opened successfully"));
    return PJ_SUCCESS;
}


static pj_status_t open_capture(struct qnx_stream *stream,
                                unsigned clock_rate,
                                unsigned channel_count,
                                unsigned samples_per_frame)
{
    int result;
    snd_pcm_channel_params_t params;
    stream->pcm_handle = stream->ca_pcm; 

    // Open PCM capture device
    result = snd_pcm_open(&stream->ca_pcm, stream->ca_dev_id.card,
                          stream->ca_dev_id.device, SND_PCM_OPEN_CAPTURE);
    if (result < 0) {
        TRACE_((THIS_FILE, "Failed to open capture device"));
        return PJMEDIA_EAUD_SYSERR;
    }

    // Clear and configure capture parameters
    memset(&params, 0, sizeof(params));
    params.channel = SND_PCM_CHANNEL_CAPTURE;
    params.mode = SND_PCM_MODE_BLOCK;
    params.format.interleave = 1; // Interleaved audio
    params.format.format = SND_PCM_SFMT_S16_LE;
    params.format.rate = clock_rate;
    params.format.voices = channel_count;
    params.start_mode = SND_PCM_START_FULL;
    params.stop_mode = SND_PCM_STOP_ROLLOVER;
    params.buf.block.frag_size = samples_per_frame * channel_count * 2; // 2 bytes per sample
    params.buf.block.frags_max = 4;
    params.buf.block.frags_min = 1;

    // Apply parameters
    result = snd_pcm_plugin_params(stream->ca_pcm, &params);
    if (result < 0) {
        TRACE_((THIS_FILE, "Failed to set capture parameters"));
        snd_pcm_close(stream->ca_pcm);
        return PJMEDIA_EAUD_SYSERR;
    }

    TRACE_((THIS_FILE, "Capture stream opened successfully"));
    return PJ_SUCCESS;
}

/* API: create stream */
static pj_status_t qnx_factory_create_stream(pjmedia_aud_dev_factory *f,
                                              const pjmedia_aud_param *param,
                                              pjmedia_aud_rec_cb rec_cb,
                                              pjmedia_aud_play_cb play_cb,
                                              void *user_data,
                                              pjmedia_aud_stream **p_strm)
{
    struct qnx_factory *af = (struct qnx_factory*)f;
    pj_status_t status;
    pj_pool_t* pool;
    struct qnx_stream* stream;

    pool = pj_pool_create (af->pf, "qnx%p", 1024, 1024, NULL);
    if (!pool)
        return PJ_ENOMEM;

    /* Allocate and initialize common stream data */
    stream = PJ_POOL_ZALLOC_T (pool, struct qnx_stream);
    stream->base.op = &qnx_stream_op;
    stream->pool      = pool;
    stream->af        = af;
    stream->user_data = user_data;
    stream->pb_cb     = play_cb;
    stream->ca_cb     = rec_cb;
    stream->pb_dev_id.device = param->play_id;
    stream->pb_dev_id.card = f->sys.drv_idx;
    stream->ca_dev_id.device = param->rec_id;
    stream->ca_dev_id.card = f->sys.drv_idx;
    stream->quit      = 0;
    pj_memcpy(&stream->param, param, sizeof(*param));

    /* Init playback */
    if (param->dir & PJMEDIA_DIR_PLAYBACK) {
        
        status = open_playback(stream,
                       param->clock_rate,
                       param->channel_count,
                       param->samples_per_frame);

        if (status != PJ_SUCCESS) {
            pj_pool_release (pool);
            return status;
        }
    }

    /* Init capture */
    if (param->dir & PJMEDIA_DIR_CAPTURE) {
        
        status = open_capture(stream,
                      param->clock_rate,
                      param->channel_count,
                      param->samples_per_frame);

        if (status != PJ_SUCCESS) {
            if (param->dir & PJMEDIA_DIR_PLAYBACK)
                snd_pcm_close (stream->pb_pcm);
            pj_pool_release (pool);
            return status;
        }
    }

    *p_strm = &stream->base;
    return PJ_SUCCESS;
}


/* API: get running parameter */
static pj_status_t qnx_stream_get_param(pjmedia_aud_stream *s,
                                         pjmedia_aud_param *pi)
{
    struct qnx_stream *stream = (struct qnx_stream*)s;

    PJ_ASSERT_RETURN(s && pi, PJ_EINVAL);

    pj_memcpy(pi, &stream->param, sizeof(*pi));

    return PJ_SUCCESS;
}


/* API: get capability */
static pj_status_t qnx_stream_get_cap(pjmedia_aud_stream *s,
                                       pjmedia_aud_dev_cap cap,
                                       void *pval)
{
    struct qnx_stream *stream = (struct qnx_stream*)s;

    PJ_ASSERT_RETURN(s && pval, PJ_EINVAL);

    if (cap==PJMEDIA_AUD_DEV_CAP_INPUT_LATENCY &&
        (stream->param.dir & PJMEDIA_DIR_CAPTURE))
    {
        /* Recording latency */
        *(unsigned*)pval = stream->param.input_latency_ms;
        return PJ_SUCCESS;
    } else if (cap==PJMEDIA_AUD_DEV_CAP_OUTPUT_LATENCY &&
               (stream->param.dir & PJMEDIA_DIR_PLAYBACK))
    {
        /* Playback latency */
        *(unsigned*)pval = stream->param.output_latency_ms;
        return PJ_SUCCESS;
    } else {
        return PJMEDIA_EAUD_INVCAP;
    }
}


pj_status_t qnx_audio_set_input_volume(snd_pcm_t *handle, unsigned volume)
{
    PJ_UNUSED_ARG(handle);
    PJ_UNUSED_ARG(volume);
    PJ_LOG(4, (__FILE__, "Stub: qnx_audio_set_input_volume"));
    return PJ_SUCCESS;
}

pj_status_t qnx_audio_set_output_volume(snd_pcm_t *handle, unsigned volume)
{
    PJ_UNUSED_ARG(handle);
    PJ_UNUSED_ARG(volume);
    PJ_LOG(4, (__FILE__, "Stub: qnx_audio_set_output_volume"));
    return PJ_SUCCESS;
}

pj_status_t qnx_audio_set_input_route(snd_pcm_t *handle, pjmedia_aud_dev_route route)
{
    PJ_UNUSED_ARG(handle);
    PJ_UNUSED_ARG(route);
    PJ_LOG(4, (__FILE__, "Stub: qnx_audio_set_input_route"));
    return PJ_SUCCESS;
}

pj_status_t qnx_audio_set_output_route(snd_pcm_t *handle, pjmedia_aud_dev_route route)
{
    PJ_UNUSED_ARG(handle);
    PJ_UNUSED_ARG(route);
    PJ_LOG(4, (__FILE__, "Stub: qnx_audio_set_output_route"));
    return PJ_SUCCESS;
}



/* API: set capability */
static pj_status_t qnx_stream_set_cap(pjmedia_aud_stream *strm,
                                      pjmedia_aud_dev_cap cap,
                                      const void *pval)
{
    struct qnx_stream *qnx_strm = (struct qnx_stream*)strm;
    pj_status_t status = PJ_SUCCESS;

    PJ_ASSERT_RETURN(qnx_strm && pval, PJ_EINVAL);

    switch (cap) {
    case PJMEDIA_AUD_DEV_CAP_INPUT_VOLUME_SETTING:
        qnx_strm->input_vol = *(const unsigned*)pval;
        // Apply volume setting to QNX audio input device
        status = qnx_audio_set_input_volume(qnx_strm->pcm_handle, qnx_strm->input_vol);
        break;

    case PJMEDIA_AUD_DEV_CAP_OUTPUT_VOLUME_SETTING:
        qnx_strm->output_vol = *(const unsigned*)pval;
        // Apply volume setting to QNX audio output device
        status = qnx_audio_set_output_volume(qnx_strm->pcm_handle, qnx_strm->output_vol);
        break;

    case PJMEDIA_AUD_DEV_CAP_INPUT_ROUTE:
        qnx_strm->input_route = *(const pjmedia_aud_dev_route*)pval;
        // Apply input route if supported
        status = qnx_audio_set_input_route(qnx_strm->pcm_handle, qnx_strm->input_route);
        break;

    case PJMEDIA_AUD_DEV_CAP_OUTPUT_ROUTE:
        qnx_strm->output_route = *(const pjmedia_aud_dev_route*)pval;
        // Apply output route if supported
        status = qnx_audio_set_output_route(qnx_strm->pcm_handle, qnx_strm->output_route);
        break;

    default:
        status = PJMEDIA_EAUD_INVCAP;
        break;
    }

    return status;
}


/* API: start stream */
static pj_status_t qnx_stream_start (pjmedia_aud_stream *s)
{
    struct qnx_stream *stream = (struct qnx_stream*)s;
    pj_status_t status = PJ_SUCCESS;

    stream->quit = 0;
    if (stream->param.dir & PJMEDIA_DIR_PLAYBACK) {
        status = pj_thread_create (stream->pool,
                                   "qnxasound_playback",
                                   pb_thread_func,
                                   stream,
                                   0, //ZERO,
                                   0,
                                   &stream->pb_thread);
        if (status != PJ_SUCCESS)
            return status;
    }

    if (stream->param.dir & PJMEDIA_DIR_CAPTURE) {
        status = pj_thread_create (stream->pool,
                                   "qnxsound_capture",
                                   ca_thread_func,
                                   stream,
                                   0, //ZERO,
                                   0,
                                   &stream->ca_thread);
        if (status != PJ_SUCCESS) {
            stream->quit = PJ_TRUE;
            pj_thread_join(stream->pb_thread);
            pj_thread_destroy(stream->pb_thread);
            stream->pb_thread = NULL;
        }
    }

    return status;
}


/* API: stop stream */
static pj_status_t qnx_stream_stop (pjmedia_aud_stream *s)
{
    struct qnx_stream *stream = (struct qnx_stream*)s;

    stream->quit = 1;

    if (stream->pb_thread) {
        TRACE_((THIS_FILE,
                   "qnx_stream_stop(%u): Waiting for playback to stop.",
                   (unsigned)gettid()));
        pj_thread_join (stream->pb_thread);
        TRACE_((THIS_FILE,
                   "qnx_stream_stop(%u): playback stopped.",
                   (unsigned)gettid()));
        pj_thread_destroy(stream->pb_thread);
        stream->pb_thread = NULL;
    }

    if (stream->ca_thread) {
        TRACE_((THIS_FILE,
                   "qnx_stream_stop(%u): Waiting for capture to stop.",
                   (unsigned)gettid()));
        pj_thread_join (stream->ca_thread);
        TRACE_((THIS_FILE,
                   "qnx_stream_stop(%u): capture stopped.",
                   (unsigned)gettid()));
        pj_thread_destroy(stream->ca_thread);
        stream->ca_thread = NULL;
    }

    return PJ_SUCCESS;
}



static pj_status_t qnx_stream_destroy (pjmedia_aud_stream *s)
{
    struct qnx_stream *stream = (struct qnx_stream*)s;

    qnx_stream_stop (s);

    if (stream->param.dir & PJMEDIA_DIR_PLAYBACK) {
        snd_pcm_close (stream->pb_pcm);
        stream->pb_pcm = NULL;
    }
    if (stream->param.dir & PJMEDIA_DIR_CAPTURE) {
        snd_pcm_close (stream->ca_pcm);
        stream->ca_pcm = NULL;
    }

    pj_pool_release (stream->pool);

    return PJ_SUCCESS;
}


/*
 * Manually set ALSA devices.
 */
PJ_DEF(pj_status_t) pjmedia_aud_qnx_set_devices( pjmedia_aud_dev_factory *f,
                                                  unsigned count,
                                                  const char* names[] )
{
    struct qnx_factory *af = (struct qnx_factory*)f;

    if (!af)
        af = (struct qnx_factory*)default_factory;

    PJ_ASSERT_RETURN(af, PJ_EINVAL);
    PJ_ASSERT_RETURN(count <= MAX_DEVICES, PJ_ETOOMANY);

    PJ_LOG(4,(THIS_FILE, "ALSA driver set manually %d devices", count));

    if (af->pool != NULL) {
        pj_pool_release(af->pool);
        af->pool = NULL;
    }

    if (count > 0) {
        unsigned i;
        af->pool = pj_pool_create(af->pf, "qnx_custom_dev", 256, 256, NULL);

        for (i = 0; i < count; ++i) {
            pj_strdup2_with_null(af->pool, &af->custom_dev[i], names[i]);
        }
    }
    af->custom_dev_cnt = count;

    return pjmedia_aud_dev_refresh();
}


#endif  /* PJMEDIA_AUDIO_DEV_HAS_QNX */
