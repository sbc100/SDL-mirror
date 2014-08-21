#include "SDL_config.h"
#include "SDL_naclaudio.h"

#include <assert.h>

#include <ppapi/c/pp_instance.h>
#include <ppapi/c/ppb.h>
#include <ppapi/c/ppb_audio.h>
#include <ppapi/c/ppb_audio_config.h>
#include <ppapi/c/ppb_core.h>

extern PP_Instance g_nacl_pp_instance;
extern PPB_GetInterface g_nacl_get_interface;
extern const PPB_Core_1_0* g_nacl_core_interface;

const PPB_Audio_1_1* g_nacl_audio_interface;
const PPB_AudioConfig_1_1* g_nacl_audio_config_interface;

extern "C" {

#include "SDL_rwops.h"
#include "SDL_timer.h"
#include "SDL_audio.h"
#include "SDL_mutex.h"
#include "../SDL_audiomem.h"
#include "../SDL_audio_c.h"
#include "../SDL_audiodev_c.h"

/* The tag name used by NACL audio */
#define NACLAUD_DRIVER_NAME "nacl"

const uint32_t kSampleFrameCount = 4096u;

/* Audio driver functions */
static int NACLAUD_OpenAudio(_THIS, SDL_AudioSpec* spec);
static void NACLAUD_CloseAudio(_THIS);

static void AudioCallback(void* samples, uint32_t buffer_size, PP_TimeDelta,
                          void* data);

/* Audio driver bootstrap functions */
static int NACLAUD_Available(void) {
  return g_nacl_pp_instance && g_nacl_get_interface;
}

static void NACLAUD_DeleteDevice(SDL_AudioDevice* device) {
  if (device->hidden->audio) {
    g_nacl_audio_interface->StopPlayback(device->hidden->audio);
    g_nacl_core_interface->ReleaseResource(device->hidden->audio);
    device->hidden->audio = 0;
  }
}

static SDL_AudioDevice* NACLAUD_CreateDevice(int devindex) {
  SDL_AudioDevice* _this;

  /* Initialize all variables that we clean on shutdown */
  _this = (SDL_AudioDevice*)SDL_malloc(sizeof(SDL_AudioDevice));
  if (_this) {
    SDL_memset(_this, 0, (sizeof *_this));
    _this->hidden =
        (struct SDL_PrivateAudioData*)SDL_malloc((sizeof *_this->hidden));
  }
  if ((_this == NULL) || (_this->hidden == NULL)) {
    SDL_OutOfMemory();
    if (_this) {
      SDL_free(_this);
    }
    return (0);
  }
  SDL_memset(_this->hidden, 0, (sizeof *_this->hidden));

  _this->hidden->mutex = SDL_CreateMutex();

  // TODO: Move audio device creation to NACLAUD_OpenAudio.
  g_nacl_audio_interface =
      (const PPB_Audio_1_1*)g_nacl_get_interface(PPB_AUDIO_INTERFACE_1_1);
  g_nacl_audio_config_interface =
      (const PPB_AudioConfig_1_1*)g_nacl_get_interface(
          PPB_AUDIO_CONFIG_INTERFACE_1_1);

  _this->hidden->sample_frame_count =
      g_nacl_audio_config_interface->RecommendSampleFrameCount(
          g_nacl_pp_instance, PP_AUDIOSAMPLERATE_44100, kSampleFrameCount);

  PP_Resource audio_config = g_nacl_audio_config_interface->CreateStereo16Bit(
      g_nacl_pp_instance, PP_AUDIOSAMPLERATE_44100,
      _this->hidden->sample_frame_count);
  _this->hidden->audio = g_nacl_audio_interface->Create(
      g_nacl_pp_instance, audio_config, AudioCallback, _this);

  // Start audio playback while we are still on the main thread.
  g_nacl_audio_interface->StartPlayback(_this->hidden->audio);

  /* Set the function pointers */
  _this->OpenAudio = NACLAUD_OpenAudio;
  _this->CloseAudio = NACLAUD_CloseAudio;

  _this->free = NACLAUD_DeleteDevice;

  return _this;
}

AudioBootStrap NACLAUD_bootstrap = {
    NACLAUD_DRIVER_NAME, "SDL nacl audio driver",
    NACLAUD_Available, NACLAUD_CreateDevice
};

static void NACLAUD_CloseAudio(_THIS) {
}

static void AudioCallback(void* samples, uint32_t buffer_size, PP_TimeDelta,
                          void* data) {
  SDL_AudioDevice* _this = reinterpret_cast<SDL_AudioDevice*>(data);

  SDL_LockMutex(_this->hidden->mutex);
  /* Only do anything if audio is enabled and not paused */
  if (_this->enabled && !_this->paused) {
    SDL_memset(samples, _this->spec.silence, buffer_size);
    SDL_LockMutex(_this->mixer_lock);
    (*_this->spec.callback)(_this->spec.userdata, (Uint8*)samples, buffer_size);
    SDL_UnlockMutex(_this->mixer_lock);
  } else {
    SDL_memset(samples, 0, buffer_size);
  }
  SDL_UnlockMutex(_this->hidden->mutex);

  return;
}

static int NACLAUD_OpenAudio(_THIS, SDL_AudioSpec* spec) {
  // We don't care what the user wants.
  spec->freq = 44100;
  spec->format = AUDIO_S16LSB;
  spec->channels = 2;
  spec->samples = _this->hidden->sample_frame_count;
  return 1;
}

}  // extern "C"
