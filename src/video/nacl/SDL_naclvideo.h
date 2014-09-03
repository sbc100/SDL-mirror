#ifndef _SDL_naclvideo_h
#define _SDL_naclvideo_h

#include "SDL_config.h"

#include "../SDL_sysvideo.h"
#include <ppapi_simple/ps_interface.h>
#include <ppapi/c/pp_input_event.h>
#include <ppapi/c/ppb_opengles2.h>

/* Hidden "this" pointer for the video functions */
#define _THIS	SDL_VideoDevice *_this

/* Private display data */
struct SDL_PrivateVideoData {
  int bpp;
  int w, h;

  int ow, oh; // plugin output dimensions
  int fsw, fsh; // fullscreen dimensions

  const struct PPB_Graphics2D_1_1 *ppb_graphics2d;
  const struct PPB_Graphics3D_1_0 *ppb_graphics3d;
  const struct PPB_Core_1_0 *ppb_core;
  const struct PPB_Fullscreen_1_0 *ppb_fullscreen;
  const struct PPB_Instance_1_0 *ppb_instance;
  const struct PPB_ImageData_1_0 *ppb_image_data;
  const struct PPB_View_1_2 *ppb_view;
  const struct PPB_Var_1_2 *ppb_var;
  const struct PPB_InputEvent_1_0 *ppb_input_event;
  const struct PPB_KeyboardInputEvent_1_2 *ppb_keyboard_input_event;
  const struct PPB_MouseInputEvent_1_1 *ppb_mouse_input_event;
  const struct PPB_WheelInputEvent_1_0 *ppb_wheel_input_event;
  const struct PPB_TouchInputEvent_1_0 *ppb_touch_input_event;
  const struct PPB_OpenGLES2 *ppb_opengles2;

  PP_Instance instance;
  PP_Resource image_data;
  unsigned char* image_pixels;
  PP_Resource context2d;  // The PPAPI 2D drawing context.
  PP_Resource context3d;  // The PPAPI 3D drawing context.
};

void NACL_SetScreenResolution(int width, int height);

#endif /* _SDL_naclvideo_h */
