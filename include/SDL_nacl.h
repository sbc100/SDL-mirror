#ifndef _SDL_nacl_h
#define _SDL_nacl_h

#include "begin_code.h"
#include <ppapi/c/pp_instance.h>
#include <ppapi/c/pp_resource.h>
#include <ppapi/c/ppb.h>

/* Set up for C function definitions, even when using C++ */
#ifdef __cplusplus
extern "C" {
#endif

void SDL_NACL_SetInstance(PP_Instance instance, PPB_GetInterface get_interface,
                          int width, int height);
void SDL_NACL_PushEvent(PP_Resource input_event);
void SDL_NACL_SetHasFocus(int has_focus);
void SDL_NACL_SetPageVisible(int is_visible);

/* Ends C function definitions when using C++ */
#ifdef __cplusplus
}
#endif
#include "close_code.h"

#endif /* _SDL_nacl_h */
