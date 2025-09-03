#ifndef __PJMEDIA_AUDIODEV_QNX_H__
#define __PJMEDIA_AUDIODEV_QNX_H__

#include <pjmedia/port.h>
#include <pjmedia/audiodev.h>
#include <pjmedia-audiodev/audiodev.h>
#include <pjmedia_audiodev.h>
#include <sys/asoundlib.h>

PJ_BEGIN_DECL

typedef struct
{
    int card;
    int device;
} qnx_dev_id;

typedef unsigned long qnx_snd_pcm_uframes_t;
typedef struct _qnx_snd_pcm_t qnx_snd_pcm_t;


struct qnx_stream
{
    pjmedia_aud_stream   base;
    pj_pool_t           *pool;
    struct qnx_factory *af;
    void                *user_data;
    pjmedia_aud_param    param;
    int                  rec_id;
    int                  quit;
    qnx_snd_pcm_t           *pb_pcm;
    qnx_snd_pcm_uframes_t    pb_frames;
    pjmedia_aud_play_cb  pb_cb;
    unsigned             pb_buf_size;
    pj_uint8_t          *pb_buf;
    pj_thread_t         *pb_thread;
    qnx_snd_pcm_t           *ca_pcm;
    qnx_snd_pcm_uframes_t    ca_frames;
    pjmedia_aud_rec_cb   ca_cb;
    unsigned             ca_buf_size;
    pj_uint8_t          *ca_buf;
    pj_thread_t         *ca_thread;
    qnx_dev_id pb_dev_id;
    qnx_dev_id ca_dev_id;
    qnx_snd_pcm_t *pcm_handle;
    unsigned input_vol;
    unsigned output_vol;
    pjmedia_aud_dev_route input_route;
    pjmedia_aud_dev_route output_route;
    int frame_count;
    pjmedia_port *port;
};


#define QNX_SND_PCM_STREAM_PLAYBACK 0
#define QNX_SND_PCM_STREAM_CAPTURE  1
#define QNX_SND_PCM_NONBLOCK        0x0001
#define QNX_SND_PCM_ASYNC           0x0002

int qnx_snd_pcm_open_name(qnx_snd_pcm_t **pcm, const char *name, int stream, int mode);
int qnx_snd_pcm_close(qnx_snd_pcm_t *pcm);
int qnx_snd_pcm_prepare(qnx_snd_pcm_t *pcm);
int qnx_snd_pcm_readi(qnx_snd_pcm_t *pcm, void *buffer, qnx_snd_pcm_uframes_t size);
int qnx_snd_pcm_writei(qnx_snd_pcm_t *pcm, const void *buffer, qnx_snd_pcm_uframes_t size);
int qnx_snd_pcm_drop(qnx_snd_pcm_t *pcm);

const char *qnx_snd_strerror(int err);

PJ_DECL(pj_status_t) pjmedia_aud_qnx_set_devices(pjmedia_aud_dev_factory *af,
                                                 unsigned count,
                                                 const char* names[]);
pj_status_t qnx_audio_set_input_volume(struct qnx_stream *handle);
pj_status_t qnx_audio_set_output_volume(struct qnx_stream *handle);
pj_status_t qnx_audio_set_input_route(struct qnx_stream *handle);
pj_status_t qnx_audio_set_output_route(struct qnx_stream *handle);

PJ_END_DECL

#endif  /* __PJMEDIA_AUDIODEV_QNX_H__ */