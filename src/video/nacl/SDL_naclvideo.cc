#include "SDL_config.h"

#include <assert.h>

#include "SDL_naclvideo.h"
#include "SDL_naclevents_c.h"

#include <ppapi/c/pp_completion_callback.h>
#include <ppapi/c/pp_errors.h>
#include <ppapi/c/pp_instance.h>
#include <ppapi/c/pp_point.h>
#include <ppapi/c/pp_rect.h>
#include <ppapi/c/pp_size.h>
#include <ppapi/c/pp_var.h>
#include <ppapi/c/ppb_core.h>
#include <ppapi/c/ppb_graphics_2d.h>
#include <ppapi/c/ppb_graphics_3d.h>
#include <ppapi/c/ppb_image_data.h>
#include <ppapi/c/ppb_input_event.h>
#include <ppapi/c/ppb_instance.h>
#include <ppapi/c/ppb_opengles2.h>
#include <ppapi/c/ppb_var.h>
#include <ppapi/gles2/gl2ext_ppapi.h>

PP_Instance g_nacl_pp_instance;
PPB_GetInterface g_nacl_get_interface;
const PPB_Core_1_0 *g_nacl_core_interface;
const PPB_Instance_1_0 *g_nacl_instance_interface;
const PPB_ImageData_1_0 *g_nacl_image_data_interface;
const PPB_Graphics2D_1_1 *g_nacl_graphics2d_interface;
const PPB_Graphics3D_1_0 *g_nacl_graphics3d_interface;
const PPB_OpenGLES2 *g_nacl_opengles2_interface;

const PPB_InputEvent_1_0 *g_nacl_input_event_interface;
const PPB_MouseInputEvent_1_1 *g_nacl_mouse_input_event_interface;
const PPB_WheelInputEvent_1_0 *g_nacl_wheel_input_event_interface;
const PPB_KeyboardInputEvent_1_0 *g_nacl_keyboard_input_event_interface;
const PPB_Var_1_1 *g_nacl_var_interface;

static int g_nacl_video_width;
static int g_nacl_video_height;

#include "SDL_nacl.h"

extern "C" {

#ifdef SDL_VIDEO_OPENGL_REGAL
#include "SDL_opengl.h"
#endif
#include "SDL_video.h"
#include "SDL_mouse.h"
#include "../SDL_sysvideo.h"
#include "../SDL_pixels_c.h"
#include "../../events/SDL_events_c.h"

#define NACLVID_DRIVER_NAME "nacl"

void SDL_NACL_SetInstance(PP_Instance instance, PPB_GetInterface get_interface,
                          int width, int height) {
  bool is_resize = g_nacl_pp_instance && (width != g_nacl_video_width ||
                                          height != g_nacl_video_height);

  g_nacl_pp_instance = instance;
  g_nacl_get_interface = get_interface;
  g_nacl_core_interface =
      (const PPB_Core_1_0 *)get_interface(PPB_CORE_INTERFACE_1_0);
  g_nacl_instance_interface =
      (const PPB_Instance_1_0 *)get_interface(PPB_INSTANCE_INTERFACE_1_0);
  g_nacl_image_data_interface =
      (const PPB_ImageData_1_0 *)get_interface(PPB_IMAGEDATA_INTERFACE_1_0);
  g_nacl_graphics2d_interface =
      (const PPB_Graphics2D_1_1 *)get_interface(PPB_GRAPHICS_2D_INTERFACE_1_1);
  g_nacl_graphics3d_interface =
      (const PPB_Graphics3D_1_0 *)get_interface(PPB_GRAPHICS_3D_INTERFACE_1_0);
  g_nacl_opengles2_interface =
      (const PPB_OpenGLES2 *)get_interface(PPB_OPENGLES2_INTERFACE_1_0);
  g_nacl_input_event_interface =
      (const PPB_InputEvent_1_0 *)get_interface(PPB_INPUT_EVENT_INTERFACE_1_0);
  g_nacl_mouse_input_event_interface =
      (const PPB_MouseInputEvent_1_1 *)get_interface(
          PPB_MOUSE_INPUT_EVENT_INTERFACE_1_1);
  g_nacl_wheel_input_event_interface =
      (const PPB_WheelInputEvent_1_0 *)get_interface(
          PPB_WHEEL_INPUT_EVENT_INTERFACE_1_0);
  g_nacl_keyboard_input_event_interface =
      (const PPB_KeyboardInputEvent_1_0 *)get_interface(
          PPB_KEYBOARD_INPUT_EVENT_INTERFACE_1_0);
  g_nacl_var_interface =
      (const PPB_Var_1_1 *)get_interface(PPB_VAR_INTERFACE_1_1);
  g_nacl_video_width = width;
  g_nacl_video_height = height;
  if (is_resize && current_video) {
    current_video->hidden->ow = width;
    current_video->hidden->oh = height;
    SDL_PrivateResize(width, height);
  }
}

/* Initialization/Query functions */
static int NACL_VideoInit(_THIS, SDL_PixelFormat *vformat);
static SDL_Rect **NACL_ListModes(_THIS, SDL_PixelFormat *format, Uint32 flags);
static SDL_Surface *NACL_SetVideoMode(_THIS, SDL_Surface *current, int width,
                                      int height, int bpp, Uint32 flags);
static void NACL_VideoQuit(_THIS);
static void NACL_UpdateRects(_THIS, int numrects, SDL_Rect *rects);

#ifdef SDL_VIDEO_OPENGL_REGAL
static int NACL_GL_GetAttribute(_THIS, SDL_GLattr attrib, int *value);
static int NACL_GL_MakeCurrent(_THIS);
static void NACL_GL_SwapBuffers(_THIS);
#endif

/* The implementation dependent data for the window manager cursor */
struct WMcursor {
  // Fake cursor data to fool SDL into not using its broken (as it seems)
  // software cursor emulation.
};

static void NACL_FreeWMCursor(_THIS, WMcursor *cursor);
static WMcursor *NACL_CreateWMCursor(_THIS, Uint8 *data, Uint8 *mask, int w,
                                     int h, int hot_x, int hot_y);
static int NACL_ShowWMCursor(_THIS, WMcursor *cursor);
static void NACL_WarpWMCursor(_THIS, Uint16 x, Uint16 y);

static int NACL_Available(void) {
  return g_nacl_pp_instance != 0;
}

static void NACL_DeleteDevice(SDL_VideoDevice *device) {
  SDL_free(device->hidden);
  SDL_free(device);
}

static SDL_VideoDevice *NACL_CreateDevice(int devindex) {
  SDL_VideoDevice *device;

  assert(g_nacl_pp_instance);

  /* Initialize all variables that we clean on shutdown */
  device = (SDL_VideoDevice *)SDL_malloc(sizeof(SDL_VideoDevice));
  if (device) {
    SDL_memset(device, 0, (sizeof *device));
    device->hidden =
        (struct SDL_PrivateVideoData *)SDL_malloc((sizeof *device->hidden));
  }
  if (device == NULL || device->hidden == NULL) {
    SDL_OutOfMemory();
    if (device) {
      SDL_free(device);
    }
    return 0;
  }
  SDL_memset(device->hidden, 0, (sizeof *device->hidden));

  device->hidden->ow = g_nacl_video_width;
  device->hidden->oh = g_nacl_video_height;

  // TODO: query the fullscreen size

  /* Set the function pointers */
  device->VideoInit = NACL_VideoInit;
  device->ListModes = NACL_ListModes;
  device->SetVideoMode = NACL_SetVideoMode;
  device->UpdateRects = NACL_UpdateRects;
  device->VideoQuit = NACL_VideoQuit;
  device->InitOSKeymap = NACL_InitOSKeymap;
  device->PumpEvents = NACL_PumpEvents;

  device->FreeWMCursor = NACL_FreeWMCursor;
  device->CreateWMCursor = NACL_CreateWMCursor;
  device->ShowWMCursor = NACL_ShowWMCursor;
  device->WarpWMCursor = NACL_WarpWMCursor;

#ifdef SDL_VIDEO_OPENGL_REGAL
  device->GL_GetAttribute = NACL_GL_GetAttribute;
  device->GL_MakeCurrent = NACL_GL_MakeCurrent;
  device->GL_SwapBuffers = NACL_GL_SwapBuffers;
#endif

  device->free = NACL_DeleteDevice;

  return device;
}

VideoBootStrap NACL_bootstrap = {
    NACLVID_DRIVER_NAME, "SDL Native Client video driver",
    NACL_Available, NACL_CreateDevice
};

int NACL_VideoInit(_THIS, SDL_PixelFormat *vformat) {
  fprintf(stderr,
          "SDL: Congratulations you are using the SDL nacl video driver!\n");

  /* Determine the screen depth (use default 8-bit depth) */
  /* we change this during the SDL_SetVideoMode implementation... */
  vformat->BitsPerPixel = 32;
  vformat->BytesPerPixel = 4;

  _this->info.current_w = g_nacl_video_width;
  _this->info.current_h = g_nacl_video_height;

  /* We're done! */
  return 0;
}

SDL_Rect **NACL_ListModes(_THIS, SDL_PixelFormat *format, Uint32 flags) {
  // TODO: list modes
  return (SDL_Rect **)-1;
}

SDL_Surface *NACL_SetVideoMode(_THIS, SDL_Surface *current, int width,
                               int height, int bpp, Uint32 flags) {

  fprintf(stderr, "SDL: setvideomode %dx%d bpp=%d opengl=%d flags=%u\n", width,
          height, bpp, flags & SDL_OPENGL ? 1 : 0, flags);
  fflush(stderr);

  if (width > _this->hidden->ow || height > _this->hidden->oh) return NULL;
  _this->hidden->bpp = bpp = 32;  // Let SDL handle pixel format conversion.
  _this->hidden->w = width;
  _this->hidden->h = height;

  if (_this->hidden->context2d) {
    g_nacl_core_interface->ReleaseResource(_this->hidden->context2d);
    _this->hidden->context2d = 0;
  }

  if (_this->hidden->context3d) {
    g_nacl_core_interface->ReleaseResource(_this->hidden->context3d);
    _this->hidden->context3d = 0;
  }

  if (flags & SDL_OPENGL) {
    int32_t attribs[] = {
      PP_GRAPHICS3DATTRIB_ALPHA_SIZE, 8,
      PP_GRAPHICS3DATTRIB_DEPTH_SIZE, 24,
      PP_GRAPHICS3DATTRIB_STENCIL_SIZE, 8,
      PP_GRAPHICS3DATTRIB_SAMPLES, 0,
      PP_GRAPHICS3DATTRIB_SAMPLE_BUFFERS, 0,
      PP_GRAPHICS3DATTRIB_WIDTH, width,
      PP_GRAPHICS3DATTRIB_HEIGHT, height,
      PP_GRAPHICS3DATTRIB_NONE
    };
    _this->hidden->context3d =
        g_nacl_graphics3d_interface->Create(g_nacl_pp_instance, 0, attribs);

    if (!g_nacl_instance_interface->BindGraphics(g_nacl_pp_instance,
                                                 _this->hidden->context3d)) {
      fprintf(stderr, "***** Couldn't bind the graphic3d context *****\n");
      return NULL;
    }
  } else {
    PP_Size size = PP_MakeSize(width, height);
    _this->hidden->context2d = g_nacl_graphics2d_interface->Create(
        g_nacl_pp_instance, &size, PP_FALSE /* is_always_opaque */);
    assert(_this->hidden->context2d != 0);

    if (!g_nacl_instance_interface->BindGraphics(g_nacl_pp_instance,
                                                 _this->hidden->context2d)) {
      fprintf(stderr, "***** Couldn't bind the graphic2d context *****\n");
      return NULL;
    }

    if (_this->hidden->image_data) {
      g_nacl_core_interface->ReleaseResource(_this->hidden->image_data);
    }

    _this->hidden->image_data = g_nacl_image_data_interface->Create(
        g_nacl_pp_instance, PP_IMAGEDATAFORMAT_BGRA_PREMUL, &size,
        PP_FALSE /* init_to_zero */);
    assert(_this->hidden->image_data != 0);

    current->pixels =
        g_nacl_image_data_interface->Map(_this->hidden->image_data);
  }

  /* Allocate the new pixel format for the screen */
  if (!SDL_ReallocFormat(current, bpp, 0xFF0000, 0xFF00, 0xFF, 0)) {
    SDL_SetError("Couldn't allocate new pixel format for requested mode");
    return NULL;
  }

  /* Set up the new mode framebuffer */
  current->flags = flags & (SDL_FULLSCREEN | SDL_OPENGL);
  _this->hidden->bpp = bpp;
  _this->hidden->w = current->w = width;
  _this->hidden->h = current->h = height;
  current->pitch = current->w * (bpp / 8);

  /* We're done */
  return current;
}

static void NACL_UpdateRects(_THIS, int numrects, SDL_Rect *rects) {
  if (_this->hidden->context2d == 0)  // not initialized
    return;

  assert(_this->hidden->image_data);
  assert(_this->hidden->w > 0);
  assert(_this->hidden->h > 0);

  // Clear alpha channel in the ImageData.
  unsigned char *start = (unsigned char*)_this->screen->pixels;
  unsigned char *end =
      start + (_this->hidden->w * _this->hidden->h * _this->hidden->bpp / 8);
  for (unsigned char *p = start + 3; p < end; p += 4) *p = 0xFF;

  // Flush on the main thread.
  for (int i = 0; i < numrects; ++i) {
    SDL_Rect &r = rects[i];
    PP_Point top_left = PP_MakePoint(0, 0);
    PP_Rect src_rect = PP_MakeRectFromXYWH(r.x, r.y, r.w, r.h);
    g_nacl_graphics2d_interface->PaintImageData(_this->hidden->context2d,
                                                _this->hidden->image_data,
                                                &top_left, &src_rect);
  }

  g_nacl_graphics2d_interface->Flush(_this->hidden->context2d,
                                     PP_BlockUntilComplete());
}

static void NACL_FreeWMCursor(_THIS, WMcursor *cursor) {
  delete cursor;
}

static WMcursor *NACL_CreateWMCursor(_THIS, Uint8 *data, Uint8 *mask, int w,
                                     int h, int hot_x, int hot_y) {
  return new WMcursor();
}

static int NACL_ShowWMCursor(_THIS, WMcursor *cursor) {
  return 1;  // Success!
}

static void NACL_WarpWMCursor(_THIS, Uint16 x, Uint16 y) {}

/* Note:  If we are terminated, this could be called in the middle of
   another SDL video routine -- notably UpdateRects.
*/
void NACL_VideoQuit(_THIS) {
  if (_this->hidden->context2d) {
    g_nacl_core_interface->ReleaseResource(_this->hidden->context2d);
    _this->hidden->context2d = 0;
  }

  if (_this->hidden->context3d) {
    g_nacl_core_interface->ReleaseResource(_this->hidden->context3d);
    _this->hidden->context3d = 0;
  }

  if (_this->hidden->image_data) {
    g_nacl_image_data_interface->Unmap(_this->hidden->image_data);
    g_nacl_core_interface->ReleaseResource(_this->hidden->image_data);
    _this->hidden->image_data = 0;
  }

  // No need to free pixels as this is a pointer directly to
  // the pixel data within the image_data.
  _this->screen->pixels = NULL;
}

#ifdef SDL_VIDEO_OPENGL_REGAL
static void regalLogCallback(GLenum stream, GLsizei length,
                             const GLchar *message, GLvoid *context) {
  fprintf(stderr, "regal: %s\n", message);
}

static int NACL_GL_GetAttribute(_THIS, SDL_GLattr attrib, int *value) {
  int unsupported = 0;
  int nacl_attrib = 0;

  switch (attrib) {
    case SDL_GL_RED_SIZE:
      nacl_attrib = PP_GRAPHICS3DATTRIB_RED_SIZE;
      break;
    case SDL_GL_GREEN_SIZE:
      nacl_attrib = PP_GRAPHICS3DATTRIB_GREEN_SIZE;
      break;
    case SDL_GL_BLUE_SIZE:
      nacl_attrib = PP_GRAPHICS3DATTRIB_BLUE_SIZE;
      break;
    case SDL_GL_ALPHA_SIZE:
      nacl_attrib = PP_GRAPHICS3DATTRIB_ALPHA_SIZE;
      break;
    case SDL_GL_DEPTH_SIZE:
      nacl_attrib = PP_GRAPHICS3DATTRIB_DEPTH_SIZE;
      break;
    case SDL_GL_STENCIL_SIZE:
      nacl_attrib = PP_GRAPHICS3DATTRIB_STENCIL_SIZE;
      break;
    case SDL_GL_MULTISAMPLEBUFFERS:
      nacl_attrib = PP_GRAPHICS3DATTRIB_SAMPLE_BUFFERS;
      break;
    case SDL_GL_MULTISAMPLESAMPLES:
      nacl_attrib = PP_GRAPHICS3DATTRIB_SAMPLES;
      break;
    // The rest of the these attributes are not part of PPAPI
    case SDL_GL_DOUBLEBUFFER:
    case SDL_GL_BUFFER_SIZE:
    case SDL_GL_ACCUM_RED_SIZE:
    case SDL_GL_ACCUM_GREEN_SIZE:
    case SDL_GL_ACCUM_BLUE_SIZE:
    case SDL_GL_ACCUM_ALPHA_SIZE:
    case SDL_GL_STEREO:
    case SDL_GL_ACCELERATED_VISUAL:
    case SDL_GL_SWAP_CONTROL:
    default:
      unsupported = 1;
      break;
  }

  if (unsupported) {
    SDL_SetError("OpenGL attribute is unsupported by NaCl: %d", attrib);
    return -1;
  }

  int32_t attribs[] = {nacl_attrib, 0, PP_GRAPHICS3DATTRIB_NONE, };

  int retval = g_nacl_graphics3d_interface->GetAttribs(_this->hidden->context3d,
                                                       attribs);
  if (retval != PP_OK) {
    // TODO(sbc): GetAttribs seems to always return PP_ERROR_FAILED(-2).
    // fprintf(stderr, "SDL: GetAttribs failed %#x -> %d\n", nacl_attrib,
    // retval);
    SDL_SetError("Error getting OpenGL attribute (%d) from NaCl: %d", attrib,
                 retval);
    return -1;
  }

  *value = attribs[1];
  return 0;
}

static int NACL_GL_MakeCurrent(_THIS) {
  if (!_this->hidden->context3d) {
    assert(_this->hidden->context3d);
    SDL_SetError("GL_MakeCurrent called without an OpenGL video mode set");
    return -1;
  }
  fprintf(stderr, "SDL: making GL context current\n");
  glSetCurrentContextPPAPI(_this->hidden->context3d);

  RegalMakeCurrent(_this->hidden->context3d,
                   (PPB_OpenGLES2 *)g_nacl_opengles2_interface);
  glLogMessageCallbackREGAL(regalLogCallback);
  return 0;
}

static void NACL_GL_SwapBuffers(_THIS) {
  if (!_this->hidden->context3d) {
    assert(_this->hidden->context3d);
    fprintf(stderr,
            "SDL: GL_SwapBuffers called without an OpenGL video mode set\n");
    return;
  }
  g_nacl_graphics3d_interface->SwapBuffers(_this->hidden->context3d,
                                           PP_BlockUntilComplete());
}
#endif

}  // extern "C"
