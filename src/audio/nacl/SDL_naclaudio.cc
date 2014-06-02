#include "../../SDL_internal.h"
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

const uint32_t kSampleFrameCount = 4096u;

static int
NACLAUD_Available(void)
{
    return g_nacl_pp_instance && g_nacl_get_interface;
}

static void
NACLAUD_CloseDevice(_THIS) {
    if (_this->hidden != NULL) {
        // TODO: this does not make sense
        SDL_LockMutex(_this->hidden->mutex);
        _this->hidden->opened = 0;
        SDL_UnlockMutex(_this->hidden->mutex);
        if (_this->hidden->audio) {
            g_nacl_audio_interface->StopPlayback(_this->hidden->audio);
            g_nacl_core_interface->ReleaseResource(_this->hidden->audio);
            _this->hidden->audio = 0;
        }
        if (_this->hidden->audio) {
            g_nacl_audio_interface->StopPlayback(_this->hidden->audio);
            g_nacl_core_interface->ReleaseResource(_this->hidden->audio);
            _this->hidden->audio = 0;
        }
//        SDL_FreeAudioMem(_this->hidden->mixbuf);
//         _this->hidden->mixbuf = NULL;
        
        SDL_free(_this->hidden);
        _this->hidden = NULL;
    }
}

static void
AudioCallback(void* samples, uint32_t buffer_size, PP_TimeDelta, void* data)
{
    SDL_AudioDevice* _this = reinterpret_cast<SDL_AudioDevice*>(data);
    struct SDL_PrivateAudioData *h = _this->hidden;

    SDL_LockMutex(h->mutex);
    if (h->opened) {
        // according to http://wiki.libsdl.org/MigrationGuide
        // filling the unused buffer space with silence is now
        // responsibility of the user
        //SDL_memset(samples, _this->spec.silence, buffer_size);
        SDL_LockMutex(_this->mixer_lock);
        // ask SDL for more data
        (*_this->spec.callback)(_this->spec.userdata, (Uint8*)samples, buffer_size);
        SDL_UnlockMutex(_this->mixer_lock);
    } else {
        // FIXME: in this case it must be probably silenced here
        SDL_memset(samples, 0, buffer_size);
    }
    SDL_UnlockMutex(h->mutex);

    return;
}

static int
NACLAUD_OpenDevice(_THIS, const char *devname, int iscapture)
{
    struct SDL_PrivateAudioData *h = NULL;
    
    if (iscapture) {
        /* TODO: implement capture */
        return SDL_SetError("Capture not supported on NaCl");
    }

    // FIXME: do we support multiple devices?
    //if (audioDevice != NULL) {
    //    return SDL_SetError("Only one audio device at a time please!");
    //}

    /* Initialize all variables that we clean on shutdown */
    _this->hidden = 
        (struct SDL_PrivateAudioData*)SDL_malloc((sizeof *_this->hidden));
    if (_this->hidden == NULL) {
	return SDL_OutOfMemory();
    }
    SDL_memset(_this->hidden, 0, (sizeof *_this->hidden));
    h = _this->hidden;
    
    h->mutex = SDL_CreateMutex();
    h->opened = false;
    
    /* Disregard what the user wants since this is the only thing we have */
    _this->spec.freq = 44100;
    _this->spec.format = AUDIO_S16LSB;
    _this->spec.channels = 2;
    _this->spec.samples = h->sample_frame_count;

    // FIXME: what is this?
    SDL_LockMutex(h->mutex);
    h->opened = 1;
    SDL_UnlockMutex(h->mutex);
    
    /* Calculate the final parameters for this audio specification */
    SDL_CalculateAudioSpec(&_this->spec);

    /* Allocate mixing buffer */
//     h->mixlen = _this->spec.size;
//     h->mixbuf = (Uint8 *) SDL_AllocAudioMem(h->mixlen);
//     if (h->mixbuf == NULL) {
        //PULSEAUDIO_CloseDevice(this);
//         return SDL_OutOfMemory();
//     }
//     SDL_memset(h->mixbuf, _this->spec.silence, _this->spec.size);
    
    // TODO: Move audio device creation to NACLAUD_OpenAudio.
    const PPB_Audio_1_1* g_nacl_audio_interface =
	(const PPB_Audio_1_1*)g_nacl_get_interface(PPB_AUDIO_INTERFACE_1_1);
    const PPB_AudioConfig_1_1* g_nacl_audio_config_interface =
	(const PPB_AudioConfig_1_1*)g_nacl_get_interface(
	    PPB_AUDIO_CONFIG_INTERFACE_1_1);

    h->sample_frame_count =
	g_nacl_audio_config_interface->RecommendSampleFrameCount(
	    g_nacl_pp_instance, PP_AUDIOSAMPLERATE_44100, kSampleFrameCount);

    PP_Resource audio_config = g_nacl_audio_config_interface->CreateStereo16Bit(
	g_nacl_pp_instance, PP_AUDIOSAMPLERATE_44100,
	h->sample_frame_count);
    h->audio = g_nacl_audio_interface->Create(
	g_nacl_pp_instance, audio_config, AudioCallback, _this);

    // Start audio playback while we are still on the main thread.
    g_nacl_audio_interface->StartPlayback(h->audio);

    return 0;
}

// FIXME: I probably do not need this
/*
static void
NACLAUD_PlayDevice(_THIS)
{
  */  /* Write the audio data */ /*
    struct SDL_PrivateAudioData *h = this->hidden;
    if (PULSEAUDIO_pa_stream_write(h->stream, h->mixbuf, h->mixlen, NULL, 0LL,
                                   PA_SEEK_RELATIVE) < 0) {
        this->enabled = 0;
    }
}

static Uint8 *
NACLAUD_GetDeviceBuf(_THIS)
{
    return (this->hidden->mixbuf);
}
*/

static int
NACLAUD_Init(SDL_AudioDriverImpl * impl)
{
    if (!NACLAUD_Available()) {
	return SDL_FALSE;
    }
  
    /* Set the function pointers */
    impl->OpenDevice = NACLAUD_OpenDevice;
    impl->PlayDevice = 0; //NACLAUD_PlayDevice;
    impl->GetDeviceBuf = 0; //NACLAUD_GetDeviceBuf;
    impl->CloseDevice = NACLAUD_CloseDevice;

    /* and the capabilities */
    impl->HasCaptureSupport = 0; /* TODO */
    impl->OnlyHasDefaultOutputDevice = 1; /* FIXME */
    impl->OnlyHasDefaultInputDevice = 1;
    impl->ProvidesOwnCallbackThread = 1;

    return SDL_TRUE;   /* this audio target is available. */
}

AudioBootStrap NACLAUD_bootstrap = {
    "nacl", "SDL nacl audio driver", NACLAUD_Init, 0
};

}  // extern "C"
