#include "SDL_config.h"

#include "SDL_naclvideo.h"
#include "SDL_nacl.h"

//FIXME: extern void NACL_InitOSKeymap(_THIS);
void NACL_PumpEvents(_THIS);
void RPI_EventInit(_THIS);
void RPI_EventQuit(_THIS);
