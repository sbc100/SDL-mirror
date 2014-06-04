#include "SDL_config.h"

#include <assert.h>

#include "SDL_naclvideo.h"
#include "SDL_naclevents.h"
#include "SDL_naclopengles.h"
#include "SDL_naclframebuffer.h"
#include "SDL_naclwindow.h"

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

SDL_Window *NACL_Window = NULL;

int g_nacl_video_width = 0; //FIXME: where I set and use this?
int g_nacl_video_height = 0;
Uint32 g_nacl_screen_format = SDL_PIXELFORMAT_UNKNOWN;

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
  fprintf(stderr, "SDL_NACL_SetInstance\n");
  //bool is_resize = g_nacl_pp_instance && (width != g_nacl_video_width ||
                                          //height != g_nacl_video_height);

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
  // FIXME: we are handling this in Android_SetScreenResolution now
  //if (is_resize && current_video) {
  //  current_video->driverdata->ow = width;
  //  current_video->driverdata->oh = height;
  //  SDL_PrivateResize(width, height);
  //}
}


// static void flush(void *data, int32_t unused);

/* Initialization/Query functions */
int NACL_VideoInit(_THIS);
//static SDL_Rect **NACL_ListModes(_THIS, SDL_PixelFormat *format, Uint32 flags);
//static SDL_Surface *NACL_SetVideoMode(_THIS, SDL_Surface *current, int width,
//                                      int height, int bpp, Uint32 flags);
void NACL_VideoQuit(_THIS);
//static void NACL_UpdateRects(_THIS, int numrects, SDL_Rect *rects);

#ifdef SDL_VIDEO_OPENGL_REGAL
//static int NACL_GL_GetAttribute(_THIS, SDL_GLattr attrib, int *value);
//static int NACL_GL_MakeCurrent(_THIS);
//static void NACL_GL_SwapBuffers(_THIS);
#endif

/* The implementation dependent data for the window manager cursor */
//struct WMcursor {
  // Fake cursor data to fool SDL into not using its broken (as it seems)
  // software cursor emulation.
//};

//static void NACL_FreeWMCursor(_THIS, WMcursor *cursor);
//static WMcursor *NACL_CreateWMCursor(_THIS, Uint8 *data, Uint8 *mask, int w,
//                                     int h, int hot_x, int hot_y);
//static int NACL_ShowWMCursor(_THIS, WMcursor *cursor);
//static void NACL_WarpWMCursor(_THIS, Uint16 x, Uint16 y);

static int NACL_Available(void)
{
fprintf(stderr, "NACL_Available\n");
  return g_nacl_pp_instance != 0;
}

static void NACL_DeleteDevice(SDL_VideoDevice *device) {
fprintf(stderr, "NACL_DeleteDevice\n");
  SDL_free(device->driverdata);
  SDL_free(device);
}

static SDL_VideoDevice *
NACL_CreateDevice(int devindex)
{
fprintf(stderr, "NACL_CreateDevice\n");
  SDL_VideoDevice *device;

  assert(g_nacl_pp_instance);

  /* Initialize all variables that we clean on shutdown */
  device = (SDL_VideoDevice *)SDL_malloc(sizeof(SDL_VideoDevice));
  struct SDL_PrivateVideoData * dd = NULL;
  if (device) {
    SDL_memset(device, 0, (sizeof *device)); // FIXME: why *device?
    dd = (struct SDL_PrivateVideoData *)SDL_malloc((sizeof(struct SDL_PrivateVideoData)));
    device->driverdata = dd;
        
  }
  if (device == NULL || dd == NULL) {
    SDL_OutOfMemory();
    if (device) {
      SDL_free(device);
    }
    return 0;
  }
  SDL_memset(dd, 0, (sizeof(struct SDL_PrivateVideoData)));

  dd->ow = g_nacl_video_width;
  dd->oh = g_nacl_video_height;

  // TODO: query the fullscreen size
    //FIXME:device->name = "nacl";
  /* Set the function pointers */
    device->VideoInit = NACL_VideoInit;
  //device->ListModes = NACL_ListModes;
  //device->SetVideoMode = NACL_SetVideoMode;
  //device->UpdateRects = NACL_UpdateRects;
    device->VideoQuit = NACL_VideoQuit;
  //device->InitOSKeymap = NACL_InitOSKeymap;
    device->PumpEvents = NACL_PumpEvents;
  
    /* Window */
    device->CreateWindow = NACL_CreateWindow;
    device->SetWindowTitle = NACL_SetWindowTitle;
    device->DestroyWindow = NACL_DestroyWindow;
    
    /* Framebuffer */
    device->CreateWindowFramebuffer = NACL_CreateWindowFramebuffer;
    device->UpdateWindowFramebuffer = NACL_UpdateWindowFramebuffer;
    device->DestroyWindowFramebuffer = NACL_DestroyWindowFramebuffer;

  //device->FreeWMCursor = NACL_FreeWMCursor;
  //device->CreateWMCursor = NACL_CreateWMCursor;
  //device->ShowWMCursor = NACL_ShowWMCursor;
  //device->WarpWMCursor = NACL_WarpWMCursor;

#ifdef SDL_VIDEO_OPENGL_REGAL
  //device->GL_GetAttribute = NACL_GL_GetAttribute;
  //device->GL_MakeCurrent = NACL_GL_MakeCurrent;
  //device->GL_SwapBuffers = NACL_GL_SwapBuffers;
    
    // new calls, FIXME: uncomment
//     device->GL_LoadLibrary = NACL_GL_LoadLibrary;
//     device->GL_GetProcAddress = NACL_GL_GetProcAddress;
//     device->GL_UnloadLibrary = NACL_GL_UnloadLibrary;
//     device->GL_CreateContext = NACL_GL_CreateContext;
//     device->GL_MakeCurrent = NACL_GL_MakeCurrent;
//     device->GL_SetSwapInterval = NACL_GL_SetSwapInterval;
//     device->GL_GetSwapInterval = NACL_GL_GetSwapInterval;
//     device->GL_SwapWindow = NACL_GL_SwapWindow;
//     device->GL_DeleteContext = NACL_GL_DeleteContext;
#endif
    
    device->free = NACL_DeleteDevice;
  
    //TODO:
    /* Text input */
    //device->StartTextInput = Android_StartTextInput;
    //device->StopTextInput = Android_StopTextInput;
    //device->SetTextInputRect = Android_SetTextInputRect;

    /* Screen keyboard */
    //device->HasScreenKeyboardSupport = Android_HasScreenKeyboardSupport;
    //device->IsScreenKeyboardShown = Android_IsScreenKeyboardShown;

    /* Clipboard */
    //device->SetClipboardText = Android_SetClipboardText;
    //device->GetClipboardText = Android_GetClipboardText;
    //device->HasClipboardText = Android_HasClipboardText;

    return device;
}

VideoBootStrap NACL_bootstrap = {
    NACLVID_DRIVER_NAME, "SDL Native Client video driver",
    NACL_Available, NACL_CreateDevice
};

//FIXME: nobody calls this now
/* This function gets called before VideoInit() */
void
NACL_SetScreenResolution(int width, int height, Uint32 format)
{
fprintf(stderr, "NACL_SetScreenResolution\n");
    g_nacl_video_width = width;
    g_nacl_video_height = height;
    g_nacl_screen_format = format;

    if (NACL_Window) {
        SDL_SendWindowEvent(NACL_Window, SDL_WINDOWEVENT_RESIZED, width, height);
    }
}

int NACL_VideoInit(_THIS) {
  fprintf(stderr,
          "SDL: Congratulations you are using the SDL nacl video driver!\n");

    SDL_DisplayMode mode;

    mode.format = g_nacl_screen_format;
    mode.w = g_nacl_video_width;
    mode.h = g_nacl_video_height;
    mode.refresh_rate = 0;
    mode.driverdata = NULL;
    if (SDL_AddBasicVideoDisplay(&mode) < 0) {
        return -1;
    }
    
    SDL_zero(mode); //FIXME: why?
    SDL_AddDisplayMode(&_this->displays[0], &mode);

    return 0;
    
  /* Determine the screen depth (use default 8-bit depth) */
  /* we change this during the SDL_SetVideoMode implementation... */
//FIXME:  vformat->BitsPerPixel = 32;
//FIXME:  vformat->BytesPerPixel = 4;
}

//FIXME: figure out where to put this.
// in window, probably. dead code now
SDL_Surface *NACL_SetVideoMode(_THIS, SDL_Surface *current, int width,
                               int height, int bpp, Uint32 flags)
{
fprintf(stderr, "NACL_SetVideoMode\n");
  SDL_PrivateVideoData* dd = reinterpret_cast<SDL_PrivateVideoData*>(_this->driverdata);

  fprintf(stderr, "SDL: setvideomode %dx%d bpp=%d opengl=%d flags=%u\n", width,
          height, bpp, -1/*flags & SDL_OPENGL ? 1 : 0*/, flags);
  fflush(stderr);

  

  if (dd->context3d) {
    g_nacl_core_interface->ReleaseResource(dd->context3d);
    dd->context3d = 0;
  }

//   if (flags & SDL_OPENGL) {
//     int32_t attribs[] = {
//       PP_GRAPHICS3DATTRIB_ALPHA_SIZE, 8,
//       PP_GRAPHICS3DATTRIB_DEPTH_SIZE, 24,
//       PP_GRAPHICS3DATTRIB_STENCIL_SIZE, 8,
//       PP_GRAPHICS3DATTRIB_SAMPLES, 0,
//       PP_GRAPHICS3DATTRIB_SAMPLE_BUFFERS, 0,
//       PP_GRAPHICS3DATTRIB_WIDTH, width,
//       PP_GRAPHICS3DATTRIB_HEIGHT, height,
//       PP_GRAPHICS3DATTRIB_NONE
//     };
//     dd->context3d =
//         g_nacl_graphics3d_interface->Create(g_nacl_pp_instance, 0, attribs);
// 
//     if (!g_nacl_instance_interface->BindGraphics(g_nacl_pp_instance,
//                                                  dd->context3d)) {
//       fprintf(stderr, "***** Couldn't bind the graphic3d context *****\n");
//       return NULL;
//     }
//   } else {
//     //framebuffer
//   }

  /* Allocate the new pixel format for the screen */
//   if (!SDL_ReallocFormat(current, bpp, 0xFF0000, 0xFF00, 0xFF, 0)) {
//     SDL_SetError("Couldn't allocate new pixel format for requested mode");
//     return NULL;
//   }

  /* Set up the new mode framebuffer */
//   current->flags = flags & (SDL_FULLSCREEN | SDL_OPENGL);
  dd->bpp = bpp;
  dd->w = current->w = width;
  dd->h = current->h = height;
  

  /* We're done */
  return current;
}

// static void NACL_FreeWMCursor(_THIS, WMcursor *cursor) {
//   delete cursor;
// }
// 
// static WMcursor *NACL_CreateWMCursor(_THIS, Uint8 *data, Uint8 *mask, int w,
//                                      int h, int hot_x, int hot_y) {
//   return new WMcursor();
// }
// 
// static int NACL_ShowWMCursor(_THIS, WMcursor *cursor) {
//   return 1;  // Success!
// }
// 
// static void NACL_WarpWMCursor(_THIS, Uint16 x, Uint16 y) {}

// FIXME:this we are also deleting in screen

/* Note:  If we are terminated, this could be called in the middle of
   another SDL video routine -- notably UpdateRects.
*/
void NACL_VideoQuit(_THIS)
{
fprintf(stderr, "NACL_VideoQuit\n");
    SDL_PrivateVideoData* dd = reinterpret_cast<SDL_PrivateVideoData*>(_this->driverdata);
  if (dd->context2d) {
    g_nacl_core_interface->ReleaseResource(dd->context2d);
    dd->context2d = 0;
  }

  if (dd->context3d) {
    g_nacl_core_interface->ReleaseResource(dd->context3d);
    dd->context3d = 0;
  }

  // No need to free pixels as this is a pointer directly to
  // the pixel data within the image_data.
//   _this->screen->pixels = NULL;
}

}  // extern "C"
