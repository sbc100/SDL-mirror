#include "SDL_config.h"

#ifndef _SDL_naclvideo_h
#define _SDL_naclvideo_h

extern "C" {
#include "../SDL_sysvideo.h"
#include "SDL_mutex.h"
}

#include <ppapi/cpp/instance.h>
#include <ppapi/cpp/graphics_2d.h>
#include <ppapi/cpp/graphics_3d.h>
#include <vector>


/* Hidden "this" pointer for the video functions */
#define _THIS	SDL_VideoDevice *_this

/* Currently only one window */
extern SDL_Window *NACL_Window;

/* These are filled in with real values in NACL_SetScreenResolution on init (before SDL_main()) */
extern int g_nacl_video_width; //FIXME: where I set and use this?
extern int g_nacl_video_height;
extern Uint32 g_nacl_screen_format;

/* Private display data */

struct SDL_PrivateVideoData {
  int bpp;
  int w, h;

  int ow, oh; // plugin output dimensions
  int fsw, fsh; // fullscreen dimensions

  PP_Resource image_data;
  unsigned char* pixels;  // later bound to image_data
  PP_Resource context2d;  // The PPAPI 2D drawing context.
  PP_Resource context3d;  // The PPAPI 3D drawing context.
};

#endif /* _SDL_naclvideo_h */
