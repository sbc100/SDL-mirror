/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2013 Sam Lantinga <slouken@libsdl.org>

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely, subject to the following restrictions:

  1. The origin of this software must not be misrepresented; you must not
     claim that you wrote the original software. If you use this software
     in a product, an acknowledgment in the product documentation would be
     appreciated but is not required.
  2. Altered source versions must be plainly marked as such, and must not be
     misrepresented as being the original software.
  3. This notice may not be removed or altered from any source distribution.
*/
#include "SDL_config.h"

#if SDL_VIDEO_DRIVER_NACL

#include "SDL_naclvideo.h"
#include "SDL_naclframebuffer.h"

extern "C" {

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
//#include <ppapi/c/ppb_opengles2.h>
#include <ppapi/c/ppb_var.h>
//#include <ppapi/gles2/gl2ext_ppapi.h>
    
extern const PP_Instance g_nacl_pp_instance;
extern const PPB_Graphics2D_1_1 *g_nacl_graphics2d_interface;
extern const PPB_Instance_1_0 *g_nacl_instance_interface;
extern const PPB_Core_1_0 *g_nacl_core_interface;
extern const PPB_ImageData_1_0 *g_nacl_image_data_interface;

int
NACL_CreateWindowFramebuffer(_THIS, SDL_Window * window, Uint32 * format,
                            void ** pixels, int *pitch)
{
fprintf(stderr, "NACL_CreateWindowFramebuffer\n");
    SDL_PrivateVideoData* dd = reinterpret_cast<SDL_PrivateVideoData*>(_this->driverdata);
    
    PP_Size size = PP_MakeSize(window->w, window->h);
    
    /* Create the graphics context for drawing */
    dd->context2d = g_nacl_graphics2d_interface->Create(
        g_nacl_pp_instance, &size, PP_FALSE /* is_always_opaque */);
    assert(dd->context2d != 0);

    if (!g_nacl_instance_interface->BindGraphics(g_nacl_pp_instance,
                                                 dd->context2d)) {
      fprintf(stderr, "***** NACL video: Pepper couldn't bind the graphic2d context *****\n");
      return NULL;
    }

    /* Free the old framebuffer surface */
    if (dd->image_data) {
      g_nacl_core_interface->ReleaseResource(dd->image_data);
    }
     
    /* Find out the pixel format and depth */
    *format = SDL_PIXELFORMAT_BGRA8888; //TODO: let PPAPI choose for us
                                             // the native one?
    if (*format == SDL_PIXELFORMAT_UNKNOWN) {
fprintf(stderr, "***** NACL video: Unknown window pixel format *****\n");
        return SDL_SetError("NACL video: Unknown window pixel format");
    }
    
    /* Calculate pitch */
    *pitch = (((window->w * SDL_BYTESPERPIXEL(*format)) + 3) & ~3); //FIXME: WTF?
    //current->pitch = current->w * (bpp / 8);
    
    /* Create the actual image */
    dd->image_data = g_nacl_image_data_interface->Create(
        //TODO: we might be more flexible and use native pixel format
        //      from GetNativeImageDataFormat()
        g_nacl_pp_instance, PP_IMAGEDATAFORMAT_BGRA_PREMUL, &size,
        PP_FALSE /* init_to_zero */);
    if (dd->image_data == 0) {
        fprintf(stderr, "***** NACL video: Couldn't create nacl image data *****\n");
        return SDL_SetError("NACL video: Couldn't create nacl image data");
    };

    dd->pixels =
         (unsigned char *) g_nacl_image_data_interface->Map(dd->image_data);
    *pixels = dd->pixels; //FIXME:
    
    return 0; // FIXME: what should I be returning?
}

int
NACL_UpdateWindowFramebuffer(_THIS,
                             SDL_Window * window,
                             const SDL_Rect * rects,
                             int numrects)
{
fprintf(stderr, "NACL_UpdateWindowFramebuffer\n");
    SDL_PrivateVideoData* dd = reinterpret_cast<SDL_PrivateVideoData*>(_this->driverdata);
    if (dd->context2d == 0) {  // not initialized
        return 0; //FIXME: what should the return value mean?
    }

    assert(dd->image_data);
    assert(dd->w > 0);
    assert(dd->h > 0);

    // Clear alpha channel in the ImageData.
    unsigned char *start = dd->pixels;
    unsigned char *end =
        start + (dd->w * dd->h * dd->bpp / 8);
    for (unsigned char *p = start + 3; p < end; p += 4) {
        *p = 0xFF;
    }

    // Flush on the main thread.
    for (int i = 0; i < numrects; ++i) {
        const SDL_Rect r = rects[i];
        PP_Point top_left = PP_MakePoint(0, 0);
        PP_Rect src_rect = PP_MakeRectFromXYWH(r.x, r.y, r.w, r.h);
        g_nacl_graphics2d_interface->PaintImageData(dd->context2d,
                                                    dd->image_data,
                                                    &top_left, &src_rect);
    }
    g_nacl_graphics2d_interface->Flush(dd->context2d,
                                       PP_BlockUntilComplete());
    return 0; //FIXME: what should the return value mean?
}

void
NACL_DestroyWindowFramebuffer(_THIS, SDL_Window * window)
{
fprintf(stderr, "NACL_DestroyWindowFramebuffer\n");
    SDL_PrivateVideoData* dd = reinterpret_cast<SDL_PrivateVideoData*>(_this->driverdata);
    //FIXME: no SDL_WindowData defined yet
    //SDL_WindowData *data = (SDL_WindowData *) window->driverdata;
    //Display *display;
    
    if (dd->context2d) {
        g_nacl_core_interface->ReleaseResource(dd->context2d);
        dd->context2d = 0;
    }
    
    if (dd->image_data) {
        g_nacl_image_data_interface->Unmap(dd->image_data);
        g_nacl_core_interface->ReleaseResource(dd->image_data);
        dd->image_data = 0;
    }

//     if (!data) {
//         /* The window wasn't fully initialized */
//         return;
//     }
}

#endif /* SDL_VIDEO_DRIVER_NACL */

} // extern "C"

/* vi: set ts=4 sw=4 expandtab: */
