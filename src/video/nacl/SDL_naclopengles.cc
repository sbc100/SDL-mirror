// /*
//   Simple DirectMedia Layer
//   Copyright (C) 1997-2012 Sam Lantinga <slouken@libsdl.org>
// 
//   This software is provided 'as-is', without any express or implied
//   warranty.  In no event will the authors be held liable for any damages
//   arising from the use of this software.
// 
//   Permission is granted to anyone to use this software for any purpose,
//   including commercial applications, and to alter it and redistribute it
//   freely, subject to the following restrictions:
// 
//   1. The origin of this software must not be misrepresented; you must not
//      claim that you wrote the original software. If you use this software
//      in a product, an acknowledgment in the product documentation would be
//      appreciated but is not required.
//   2. Altered source versions must be plainly marked as such, and must not be
//      misrepresented as being the original software.
//   3. This notice may not be removed or altered from any source distribution.
// */
// #include "SDL_config.h"
// 
// #if SDL_VIDEO_DRIVER_NACL && SDL_VIDEO_OPENGL_REGAL
// 
// /* NaCl SDL video GLES 2 driver implementation */
// 
// #include "SDL_video.h"
// #include "SDL_naclvideo.h"
// //#include "SDL_naclwrapper.h"
// extern "C" {
// #include "../SDL_sysvideo.h"
// }
// 
// #include <ppapi/cpp/graphics_3d_client.h>
// #include <ppapi/cpp/graphics_3d.h>
// #include <ppapi/gles2/gl2ext_ppapi.h>
// #include <dlfcn.h>
// 
// static void* NACL_GLHandle = NULL;
// 
// /* GL functions */
// int
// NACL_GL_LoadLibrary(_THIS, const char *path)
// {
//     //TODO
//     return 0;
// }
// 
// void *
// NACL_GL_GetProcAddress(_THIS, const char *proc)
// {
//     //TODO
//     return NULL;
// }
// 
// void
// NACL_GL_UnloadLibrary(_THIS)
// {
//     //TODO
// }
// 
// SDL_GLContext
// NACL_GL_CreateContext(_THIS, SDL_Window * window)
// {
//     SDL_PrivateVideoData* dd = reinterpret_cast<SDL_PrivateVideoData*>(_this->driverdata);
//     if (!dd->context3d) {
//         assert(dd->context3d);
//         SDL_SetError("GL_MakeCurrent called without an OpenGL video mode set");
//         return -1;
//     }
//     fprintf(stderr, "SDL: making GL context current\n");
//     glSetCurrentContextPPAPI(dd->context3d);
// 
//     RegalMakeCurrent(dd->context3d,
//                     (PPB_OpenGLES2 *)g_nacl_opengles2_interface);
//     glLogMessageCallbackREGAL(regalLogCallback);
//     return (SDL_GLContext)1;
//     //FIXME OR
// //     /* This NEEDS to run in the main thread! */
// //     assert(gNaclMainThreadRunner);
// //     assert(pp::Module::Get()->core()->IsMainThread());
// //     pp::Instance *instance = gNaclMainThreadRunner->pp_instance();
// //     pp::Module* module = pp::Module::Get();    
// //     assert(module);
// //     const struct PPB_OpenGLES2* gles2_interface_ = static_cast<const struct PPB_OpenGLES2*>(module->GetBrowserInterface(PPB_OPENGLES2_INTERFACE));
// //  
// //     SDL_VideoData *driverdata = (SDL_VideoData *) _this->driverdata;
// //     
// // 
// //         glInitializePPAPI(module->get_browser_interface());
// //         if (instance == NULL) {
// //           glSetCurrentContextPPAPI(0);
// //           return NULL;
// //         }
// //     if (!driverdata->context3d) {
// //         int32_t attribs[] = {
// //             PP_GRAPHICS3DATTRIB_ALPHA_SIZE, 8,
// //             PP_GRAPHICS3DATTRIB_DEPTH_SIZE, 24,
// //             PP_GRAPHICS3DATTRIB_STENCIL_SIZE, 8,
// //             PP_GRAPHICS3DATTRIB_SAMPLES, 0,
// //             PP_GRAPHICS3DATTRIB_SAMPLE_BUFFERS, 0,
// //             PP_GRAPHICS3DATTRIB_WIDTH, driverdata->w,
// //             PP_GRAPHICS3DATTRIB_HEIGHT,  driverdata->h,
// //             PP_GRAPHICS3DATTRIB_NONE
// //         };
// //         driverdata->context3d = new pp::Graphics3D(instance, pp::Graphics3D(), attribs);
// //         if (driverdata->context3d->is_null()) {
// //           glSetCurrentContextPPAPI(0);
// //           return NULL;
// //         }
// //         instance->BindGraphics(*driverdata->context3d);
// //     }
// //     glSetCurrentContextPPAPI(driverdata->context3d->pp_resource());
// //     return (SDL_GLContext)1;
// }
// 
// int
// NACL_GL_MakeCurrent(_THIS, SDL_Window * window, SDL_GLContext context)
// {
//     /* There's only one context, nothing to do... */
//     return 0;
// }
// 
// int
// NACL_GL_SetSwapInterval(_THIS, int interval)
// {
//     //TODO
//     return 0;
// }
// 
// int
// NACL_GL_GetSwapInterval(_THIS)
// {
//     //TODO
//     return 0;
// }
// 
// void FlushCallback(void* data, int32_t result)
// {
// }
// 
// void
// NACL_GL_SwapWindow(_THIS, SDL_Window * window)
// {
//     SDL_PrivateVideoData* dd = reinterpret_cast<SDL_PrivateVideoData*>(_this->driverdata);
//     if (!dd->context3d) {
//         assert(dd->context3d);
//         fprintf(stderr,
//                 "SDL: GL_SwapBuffers called without an OpenGL video mode set\n");
//         return;
//     }
//     g_nacl_graphics3d_interface->SwapBuffers(dd->context3d,
//                                              PP_BlockUntilComplete());
//     //FIXME: OR
//     //SDL_VideoData *driverdata = (SDL_VideoData *) _this->driverdata;
//     ///* Callback is required so we don't block the main thread */
//     //driverdata->context3d->SwapBuffers(pp::CompletionCallback(&FlushCallback, _this));
// }
// 
// void
// NACL_GL_DeleteContext(_THIS, SDL_GLContext context)
// {
//     SDL_VideoData *driverdata = (SDL_VideoData *) _this->driverdata;
//     delete driverdata->context3d;
//     driverdata->context3d = NULL;
// }
// 
// #endif /* SDL_VIDEO_DRIVER_NACL */
// 
// /* vi: set ts=4 sw=4 expandtab: */
// 
// #ifdef SDL_VIDEO_OPENGL_REGAL
// static void regalLogCallback(GLenum stream, GLsizei length,
//                              const GLchar *message, GLvoid *context) {
//   fprintf(stderr, "regal: %s\n", message);
// }
// #endif