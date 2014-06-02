#include "SDL_config.h"

#ifndef _SDL_naclaudio_h
#define _SDL_naclaudio_h

extern "C" {
#include "SDL_audio.h"
#include "../SDL_sysaudio.h"
#include "SDL_mutex.h"
}

#include <ppapi/c/ppb_audio.h>

/* Hidden "this" pointer for the audio functions */
#define _THIS SDL_AudioDevice *_this

struct SDL_PrivateAudioData {

  SDL_mutex* mutex;
  // This flag is use to determine when the audio is opened and we can start
  // serving audio data instead of silence. This is needed because current
  // Pepper2 can only be used from the main thread; Therefore, we start the
  // audio thread very early.
  bool opened;

  int sample_frame_count;
  PP_Resource audio;
};

#endif /* _SDL_naclaudio_h */
