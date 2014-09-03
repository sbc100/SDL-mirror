#include "SDL_config.h"

#ifndef _SDL_naclaudio_h
#define _SDL_naclaudio_h

#include "SDL_audio.h"
#include "../SDL_sysaudio.h"
#include "SDL_mutex.h"

#include <ppapi/c/ppb_audio.h>

/* Hidden "this" pointer for the video functions */
#define _THIS	SDL_AudioDevice *_this

struct SDL_PrivateAudioData {
  SDL_mutex* mutex;
  int sample_frame_count;
  PP_Resource audio;

  const struct PPB_Core_1_0* core_interface;
  const struct PPB_Audio_1_1* audio_interface;
  const struct PPB_AudioConfig_1_1* audio_config_interface;
};

#endif /* _SDL_naclaudio_h */
