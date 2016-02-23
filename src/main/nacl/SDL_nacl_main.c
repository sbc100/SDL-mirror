/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2014 Sam Lantinga <slouken@libsdl.org>

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

/* Include the SDL main definition header */
#include "SDL_main.h"
#include "../../SDL_trace.h"

#include <errno.h>
#include <ppapi_simple/ps_main.h>
#include <ppapi_simple/ps_event.h>
#include <ppapi_simple/ps_interface.h>
#include <nacl_io/nacl_io.h>
#include <sys/mount.h>
#include <nacl_main.h>
#include <unistd.h>

#undef main

extern void NACL_SetScreenResolution(int width, int height);

static int
ProcessArgs(int argc, char** argv) {
    const char* arg = getenv("SDL_TAR_EXTRACT");
    if (arg != NULL) {
        const char* source;
        const char* target = "/";
        char* sep;
        char buf[64];
        int n;

        const char* q, *p = arg;
        while (*p) {
            while (*p && isspace((unsigned char)*p)) ++p;
            if (!*p) break;
            q = p;
            while (*p && !isspace((unsigned char)*p)) ++p;

            n = sizeof(buf) - 1;
            if (p - q < n) n = p - q;
            strncpy(buf, q, n);
            buf[n] = '\0';

            sep = strchr(buf, ':');
            source = buf;
            if (sep) {
                target = sep + 1;
                *sep = '\0';
            }

            SDL_log("extracting tar file '%s' -> '%s'\n", source, target);
            if (nacl_startup_untar(argv[0], source, target) != 0)
              return 1;
        }
    }

    return 0;
}

/* This main function is called a worker thread by ppapi_simple */
int
main(int argc, char *argv[])
{
    SDL_TRACE("SDL_nacl_main\n");
    PSEvent* ps_event;
    PP_Resource event;
    struct PP_Rect rect;
    int ready = 0;
    const PPB_View *ppb_view = PSInterfaceView();

    if (ProcessArgs(argc, argv) != 0) {
        return 1;
    }

    /* Wait for the first PSE_INSTANCE_DIDCHANGEVIEW event before starting the app */
    PSEventSetFilter(PSE_INSTANCE_DIDCHANGEVIEW);
    /* Process all waiting events without blocking */
    while (!ready) {
        ps_event = PSEventWaitAcquire();
        event = ps_event->as_resource;
        switch(ps_event->type) {
            /* From DidChangeView, contains a view resource */
            case PSE_INSTANCE_DIDCHANGEVIEW:
                ppb_view->GetRect(event, &rect);
                NACL_SetScreenResolution(rect.size.width, rect.size.height);
                ready = 1;
                break;
            default:
                break;
        }
        PSEventRelease(ps_event);
    }

    /*
     * Startup in /mnt/http by default so resources hosted on the webserver
     * are accessible in the current working directory.
     */
    if (getenv("PWD") == NULL) {
        if (chdir("/mnt/http") != 0) {
            SDL_log("chdir to /mnt/http failed: %s\n", strerror(errno));
        }
    }

    return SDL_main(argc, argv);
}

#endif /* SDL_VIDEO_DRIVER_NACL */
