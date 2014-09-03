#include "SDL_config.h"

#include <assert.h>
#include <ppapi/c/pp_errors.h>
#include <ppapi_simple/ps.h>

#include "SDL_naclvideo.h"
#include "SDL_naclevents.h"
#include "../../SDL_trace.h"

static int g_nacl_video_width;
static int g_nacl_video_height;

#ifdef SDL_VIDEO_OPENGL_REGAL
#include "SDL_opengl.h"
#endif
#include "SDL_video.h"
#include "SDL_mouse.h"
#include "../SDL_sysvideo.h"
#include "../SDL_pixels_c.h"
#include "../../events/SDL_events_c.h"

#include <ppapi/gles2/gl2ext_ppapi.h>
#include <ppapi_simple/ps_event.h>

#define NACLVID_DRIVER_NAME "nacl"

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

void NACL_SetScreenResolution(int width, int height) {
  if (current_video) {
    current_video->hidden->ow = width;
    current_video->hidden->oh = height;
    SDL_PrivateResize(width, height);
  } else {
    g_nacl_video_width = width;
    g_nacl_video_height = height;
  }
}

static void NACL_FreeWMCursor(_THIS, WMcursor *cursor);
static WMcursor *NACL_CreateWMCursor(_THIS, Uint8 *data, Uint8 *mask, int w,
                                     int h, int hot_x, int hot_y);
static int NACL_ShowWMCursor(_THIS, WMcursor *cursor);
static void NACL_WarpWMCursor(_THIS, Uint16 x, Uint16 y);

static int NACL_Available(void) {
  return PSGetInstanceId() != 0;
}

static void NACL_DeleteDevice(SDL_VideoDevice *device) {
  SDL_free(device->hidden);
  SDL_free(device);
}

static SDL_VideoDevice *NACL_CreateDevice(int devindex) {
  SDL_VideoDevice *device;

  assert(NACL_Available());

  /* Initialize all variables that we clean on shutdown */
  device = SDL_malloc(sizeof(SDL_VideoDevice));
  if (device) {
    SDL_memset(device, 0, sizeof(*device));
    device->hidden = SDL_malloc(sizeof(*device->hidden));
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
  SDL_TRACE("Congratulations you are using the SDL nacl video driver!\n");

  /* Determine the screen depth (use default 8-bit depth) */
  /* we change this during the SDL_SetVideoMode implementation... */
  vformat->BitsPerPixel = 32;
  vformat->BytesPerPixel = 4;

  _this->info.current_w = g_nacl_video_width;
  _this->info.current_h = g_nacl_video_height;

  struct SDL_PrivateVideoData *dd = _this->hidden;

  PSInterfaceInit();
  dd->instance = PSGetInstanceId();
  dd->ppb_graphics2d = PSGetInterface(PPB_GRAPHICS_2D_INTERFACE_1_1);
  dd->ppb_graphics3d = PSGetInterface(PPB_GRAPHICS_3D_INTERFACE_1_0);
  dd->ppb_core = PSGetInterface(PPB_CORE_INTERFACE_1_0);
  dd->ppb_fullscreen = PSGetInterface(PPB_FULLSCREEN_INTERFACE_1_0);
  dd->ppb_instance = PSGetInterface(PPB_INSTANCE_INTERFACE_1_0);
  dd->ppb_image_data = PSGetInterface(PPB_IMAGEDATA_INTERFACE_1_0);
  dd->ppb_view = PSGetInterface(PPB_VIEW_INTERFACE_1_2);
  dd->ppb_var = PSGetInterface(PPB_VAR_INTERFACE_1_2);
  dd->ppb_input_event = PSGetInterface(PPB_INPUT_EVENT_INTERFACE_1_0);
  dd->ppb_keyboard_input_event = PSGetInterface(PPB_KEYBOARD_INPUT_EVENT_INTERFACE_1_2);
  dd->ppb_mouse_input_event = PSGetInterface(PPB_MOUSE_INPUT_EVENT_INTERFACE_1_1);
  dd->ppb_wheel_input_event = PSGetInterface(PPB_WHEEL_INPUT_EVENT_INTERFACE_1_0);
  dd->ppb_touch_input_event = PSGetInterface(PPB_TOUCH_INPUT_EVENT_INTERFACE_1_0);
  dd->ppb_opengles2 = PSGetInterface(PPB_OPENGLES2_INTERFACE);
  PSEventSetFilter(PSE_ALL);

  /* We're done! */
  return 0;
}

SDL_Rect **NACL_ListModes(_THIS, SDL_PixelFormat *format, Uint32 flags) {
  // TODO: list modes
  return (SDL_Rect **)-1;
}

SDL_Surface *NACL_SetVideoMode(_THIS, SDL_Surface *current, int width,
                               int height, int bpp, Uint32 flags) {

  struct SDL_PrivateVideoData *dd = _this->hidden;
  SDL_TRACE("setvideomode %dx%d bpp=%d opengl=%d flags=%u\n", width,
            height, bpp, flags & SDL_OPENGL ? 1 : 0, flags);

  if (width > dd->ow || height > dd->oh)
    return NULL;

  dd->bpp = bpp = 32;  // Let SDL handle pixel format conversion.
  dd->w = width;
  dd->h = height;

  if (dd->context2d) {
    dd->ppb_core->ReleaseResource(dd->context2d);
    dd->context2d = 0;
  }

  if (dd->context3d) {
    dd->ppb_core->ReleaseResource(dd->context3d);
    dd->context3d = 0;
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
    dd->context3d = dd->ppb_graphics3d->Create(dd->instance, 0, attribs);

    if (!dd->ppb_instance->BindGraphics(dd->instance, dd->context3d)) {
      SDL_SetError("Couldn't bind the graphic3d context");
      return NULL;
    }
    if (!dd->ppb_opengles2) {
      SDL_SetError("OpenGLES2 interface not available");
      return NULL;
    }
  } else {
    struct PP_Size size = PP_MakeSize(width, height);

    /*
     * Set is_always_opaque to true since we always clear the alpha channel
     * before painting to the context.
     */
    dd->context2d = dd->ppb_graphics2d->Create(dd->instance,
                                               &size,
                                               PP_TRUE /* is_always_opaque */);
    assert(dd->context2d != 0);

    if (!dd->ppb_instance->BindGraphics(dd->instance, dd->context2d)) {
      SDL_SetError("Couldn't bind the graphic2d context");
      return NULL;
    }

    if (dd->image_data) {
      dd->ppb_core->ReleaseResource(dd->image_data);
    }

    dd->image_data = dd->ppb_image_data->Create(
        dd->instance,
        PP_IMAGEDATAFORMAT_BGRA_PREMUL,
        &size,
        PP_TRUE /* init_to_zero */);
    assert(dd->image_data != 0);

    dd->image_pixels = dd->ppb_image_data->Map(dd->image_data);
    current->pixels = dd->image_pixels;
  }

  /* Allocate the new pixel format for the screen */
  if (!SDL_ReallocFormat(current, bpp, 0xFF0000, 0xFF00, 0xFF, 0)) {
    SDL_SetError("Couldn't allocate new pixel format for requested mode");
    return NULL;
  }

  /* Set up the new mode framebuffer */
  current->flags = flags & (SDL_FULLSCREEN | SDL_OPENGL);
  current->w = width;
  current->h = height;
  current->pitch = width * (bpp / 8);

  if ((flags & SDL_OPENGL) == 0) {
    // Paint the new (empty) image data
    SDL_Rect rect = { 0, 0, dd->w, dd->h };
    NACL_UpdateRects(_this, 1, &rect);
  }

  /* We're done */
  return current;
}

static void NACL_UpdateRects(_THIS, int numrects, SDL_Rect *rects) {
  struct SDL_PrivateVideoData *dd = _this->hidden;
  unsigned char *p;
  unsigned char *start;
  unsigned char *end;
  int i;

  if (dd->context2d == 0)  // not initialized
    return;

  assert(dd->image_data);
  assert(dd->w > 0);
  assert(dd->h > 0);

  // Clear alpha channel in the ImageData.
  start = dd->image_pixels;
  end = start + (dd->w * dd->h * dd->bpp / 8);
  for (p = start + 3; p < end; p += 4)
    *p = 0xFF;

  // Flush on the main thread.
  for (i = 0; i < numrects; ++i) {
    SDL_Rect *r = rects+i;
    struct PP_Point top_left = PP_MakePoint(0, 0);
    struct PP_Rect src_rect = PP_MakeRectFromXYWH(r->x, r->y, r->w, r->h);
    dd->ppb_graphics2d->PaintImageData(dd->context2d,
                                       dd->image_data,
                                       &top_left,
                                       &src_rect);
  }

  dd->ppb_graphics2d->Flush(dd->context2d, PP_BlockUntilComplete());
}

static void NACL_FreeWMCursor(_THIS, WMcursor *cursor) {
  free(cursor);
}

static WMcursor *NACL_CreateWMCursor(_THIS, Uint8 *data, Uint8 *mask, int w,
                                     int h, int hot_x, int hot_y) {
  return malloc(sizeof(WMcursor));
}

static int NACL_ShowWMCursor(_THIS, WMcursor *cursor) {
  return 1;  // Success!
}

static void NACL_WarpWMCursor(_THIS, Uint16 x, Uint16 y) {
}

/* Note:  If we are terminated, this could be called in the middle of
   another SDL video routine -- notably UpdateRects.
*/
void NACL_VideoQuit(_THIS) {
  SDL_TRACE("NACL_VideoInit\n");
  struct SDL_PrivateVideoData *dd = _this->hidden;

  if (dd->context2d) {
    dd->ppb_core->ReleaseResource(dd->context2d);
    dd->context2d = 0;
  }

  if (dd->context3d) {
    dd->ppb_core->ReleaseResource(dd->context3d);
    dd->context3d = 0;
  }

  if (dd->image_data) {
    dd->ppb_image_data->Unmap(dd->image_data);
    dd->ppb_core->ReleaseResource(dd->image_data);
    dd->image_data = 0;
  }

  // No need to free pixels as this is a pointer directly to
  // the pixel data within the image_data.
  _this->screen->pixels = NULL;
}

#ifdef SDL_VIDEO_OPENGL_REGAL
static void regalLogCallback(GLenum stream, GLsizei length,
                             const GLchar *message, GLvoid *context) {
  SDL_TRACE("regal: %s\n", message);
}

static int NACL_GL_GetAttribute(_THIS, SDL_GLattr attrib, int *value) {
  int unsupported = 0;
  int nacl_attrib = 0;
  struct SDL_PrivateVideoData *dd = _this->hidden;

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

  int retval = dd->ppb_graphics3d->GetAttribs(dd->context3d, attribs);
  if (retval != PP_OK) {
    // TODO(sbc): GetAttribs seems to always return PP_ERROR_FAILED(-2).
    // SDL_TRACE("GetAttribs failed %#x -> %d\n", nacl_attrib, retval);
    SDL_SetError("Error getting OpenGL attribute (%d) from NaCl: %d", attrib,
                 retval);
    return -1;
  }

  *value = attribs[1];
  return 0;
}

static int NACL_GL_MakeCurrent(_THIS) {
  struct SDL_PrivateVideoData *dd = _this->hidden;
  assert(dd->ppb_opengles2);
  if (!dd->context3d) {
    assert(dd->context3d);
    SDL_SetError("GL_MakeCurrent called without an OpenGL video mode set");
    return -1;
  }
  SDL_TRACE("making GL context current\n");
  glSetCurrentContextPPAPI(dd->context3d);

  RegalMakeCurrent(dd->context3d, (struct PPB_OpenGLES2*)dd->ppb_opengles2);
  glLogMessageCallbackREGAL(regalLogCallback);
  return 0;
}

static void NACL_GL_SwapBuffers(_THIS) {
  struct SDL_PrivateVideoData *dd = _this->hidden;

  if (!dd->context3d) {
    assert(dd->context3d);
    SDL_TRACE("GL_SwapBuffers called without an OpenGL video mode set\n");
    return;
  }
  dd->ppb_graphics3d->SwapBuffers(dd->context3d, PP_BlockUntilComplete());
}
#endif
