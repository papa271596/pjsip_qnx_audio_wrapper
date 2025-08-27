#ifndef __PJMEDIA_AUDIODEV_QNX_H__
#define __PJMEDIA_AUDIODEV_QNX_H__


#include <pjmedia/port.h>
#include <pjmedia/audiodev.h>
#include <pjmedia-audiodev/audiodev.h>
#include <sys/asoundlib.h>
#include "qnx_alsa_wrapper.h" // Include the QNX ALSA wrapper header


PJ_BEGIN_DECL

/**
 * Manually set QNX devices.
 */
PJ_DECL(pj_status_t) pjmedia_aud_qnx_set_devices(pjmedia_aud_dev_factory *af,
                                                 unsigned count,
                                                 const char* names[]);
PJ_END_DECL


pj_status_t qnx_audio_set_input_volume(snd_pcm_t *handle, unsigned volume);
pj_status_t qnx_audio_set_output_volume(snd_pcm_t *handle, unsigned volume);
pj_status_t qnx_audio_set_input_route(snd_pcm_t *handle, pjmedia_aud_dev_route route);
pj_status_t qnx_audio_set_output_route(snd_pcm_t *handle, pjmedia_aud_dev_route route);


#endif  /* __PJMEDIA_AUDIODEV_QNX_H__ */
