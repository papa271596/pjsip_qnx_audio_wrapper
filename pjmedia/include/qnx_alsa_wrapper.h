#ifndef QNX_ALSA_WRAPPER_H
#define QNX_ALSA_WRAPPER_H

typedef struct _qnx_snd_pcm_t qnx_snd_pcm_t;
typedef unsigned long qnx_snd_pcm_uframes_t;

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


typedef struct qnx_dev_id
{
    int card;
    int device;
} qnx_dev_id;


#endif // QNX_ALSA_WRAPPER_H
