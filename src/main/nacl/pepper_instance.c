/*
 * Copyright (c) 2011 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <libtar.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <sys/mount.h>
#include <unistd.h>

#include <SDL.h>
#include <SDL_nacl.h>
#include <SDL_video.h>
#include <SDL_main.h>

#include <ppapi/c/pp_errors.h>
#include <ppapi/c/pp_instance.h>
#include <ppapi/c/ppb.h>
#include <ppapi/c/ppb_input_event.h>
#include <ppapi/c/ppb_messaging.h>
#include <ppapi/c/ppb_var.h>
#include <ppapi/c/ppb_view.h>
#include <ppapi/c/ppp.h>
#include <ppapi/c/ppp_input_event.h>
#include <ppapi/c/ppp_instance.h>
#include <ppapi/c/pp_rect.h>

#include "nacl_io/nacl_io.h"
#include "nacl_io/ioctl.h"
#include "nacl_io/log.h"

static PP_Instance g_instance;
static PPB_GetInterface g_get_browser_interface;
static struct PPB_Messaging_1_0* g_messaging_interface;
static struct PPB_Var_1_1* g_var_interface;
static struct PPB_InputEvent_1_0* g_input_event_interface;
static struct PPB_View_1_1* g_view_interface;
static int g_width = 0;
static int g_height = 0;
static int g_argc;
static const char** g_argn;
static const char** g_argv;
static const char* g_tty_prefix;
static pthread_t g_sdl_thread;

static void* sdl_thread_start(void*);
static void ProcessArgs();

static PP_Bool Instance_DidCreate(PP_Instance instance, uint32_t argc,
                                  const char* argn[], const char* argv[]) {
  int i;

  g_instance = instance;
  g_input_event_interface->RequestInputEvents(instance,
                                              PP_INPUTEVENT_CLASS_MOUSE);
  g_input_event_interface->RequestFilteringInputEvents(
      instance, PP_INPUTEVENT_CLASS_KEYBOARD);

  nacl_io_init_ppapi(instance, g_get_browser_interface);

  // Copy args so they can be processed later by the SDL thread.
  g_argc = argc;
  g_argn = (const char**)malloc(argc * sizeof(char*));
  g_argv = (const char**)malloc(argc * sizeof(char*));
  memset(g_argn, 0, argc * sizeof(char*));
  memset(g_argv, 0, argc * sizeof(char*));

  for (i = 0; i < argc; i++) {
    if (argn[i]) g_argn[i] = strdup(argn[i]);
    if (argv[i]) g_argv[i] = strdup(argv[i]);
  }

  return PP_TRUE;
}

static void Instance_DidDestroy(PP_Instance instance) {}

static void Instance_DidChangeView(PP_Instance instance,
                                   PP_Resource view_resource) {
  struct PP_Rect view_rect;
  if (g_width && g_height) return;

  if (g_view_interface->GetRect(view_resource, &view_rect) == PP_FALSE) {
    fprintf(stderr, "SDL: Unable to get view rect!\n");
    return;
  }

  if (view_rect.size.width == g_width && view_rect.size.height == g_height)
    return;  // Size didn't change, no need to update anything.

  g_width = view_rect.size.width;
  g_height = view_rect.size.height;

  SDL_NACL_SetInstance(g_instance, g_get_browser_interface, g_width, g_height);
  pthread_create(&g_sdl_thread, NULL, sdl_thread_start, NULL);
}

/*
 * Entry point for the main SDL thread.  The job of this thread is to
 * run SDL_main, which in most cases is the application's main() function
 * re-defined by the pre-processor in SDL_main.h.
 */
static void* sdl_thread_start(void* arg) {
  char* argv[2];
  int rtn;

  ProcessArgs();
  fprintf(stderr, "SDL: calling SDL_main\n");
  argv[0] = (char*)"SDL applicaiton";
  argv[1] = NULL;
  rtn = SDL_main(1, argv);
  fprintf(stderr, "SDL: SDL_main returned: %d\n", rtn);
  exit(rtn);
  return NULL;
}

static ssize_t TtyOutputHandler(const char* buf, size_t count, void* user_data) {
  // We prepend the prefix_ to the data in buf, then package it up
  // and post it as a message to javascript.
  char* message = (char*)malloc(count + strlen(g_tty_prefix));
  strcpy(message, g_tty_prefix);
  memcpy(message + strlen(g_tty_prefix), buf, count);
  struct PP_Var var = g_var_interface->VarFromUtf8(message, count + strlen(g_tty_prefix));
  g_messaging_interface->PostMessage(g_instance, var);
  g_var_interface->Release(var);
  free(message);
  return count;
}

static void ProcessArgs() {
  int i;
  int rtn;
  int mount_default = 1;
  nacl_io_log("SDL: ProcessArgs %d\n", g_argc);

  // This could fail if the sdl_mount_http attributes
  // above created a mount on "/".
  fprintf(stderr, "SDL: creating memfs on root\n");

  rtn = umount("/");
  if (rtn != 0)
    fprintf(stderr, "SDL: umount failed: %s\n", strerror(errno));

  rtn = mount("", "/", "memfs", 0, "");
  if (rtn != 0)
    fprintf(stderr, "SDL: mount failed: %s\n", strerror(errno));

  rtn = mkdir("/tmp", S_IRWXU | S_IRWXG);
  if (rtn != 0)
    fprintf(stderr, "SDL: mkdir /tmp failed: %s\n", strerror(errno));

  rtn = mkdir("/home", S_IRWXU | S_IRWXG);
  if (rtn != 0)
    fprintf(stderr, "SDL: mkdir /home failed: %s\n", strerror(errno));

  rtn = mount("", "/tmp", "html5fs", 0, "type=TEMPORARY");
  if (rtn != 0)
    fprintf(stderr, "SDL: mount /tmp failed: %s\n", strerror(errno));

  for (i = 0; i < g_argc; i++) {
    if (!strcmp(g_argn[i], "ps_stdout")) {
      int fd1 = open(g_argv[i], O_WRONLY);
      dup2(fd1, 1);
    } else if (!strcmp(g_argn[i], "ps_stderr")) {
      int fd2 = open(g_argv[i], O_WRONLY);
      dup2(fd2, 2);
    } else if (!strcmp(g_argn[i], "ps_tty_prefix")) {
      g_tty_prefix = g_argv[i];
      struct tioc_nacl_output handler;
      handler.handler = TtyOutputHandler;
      handler.user_data = NULL;
      int tty_fd = open("/dev/tty", O_WRONLY);
      if (tty_fd != -1) {
        ioctl(tty_fd, TIOCNACLOUTPUT, &handler);
        close(tty_fd);
      }
    } else if (!strcmp(g_argn[i], "sdl_mount_http")) {
      const char* source;
      const char* target = "/";
      char* sep;
      char buf[64];
      int n;

      const char* q, *p = g_argv[i];
      while (*p) {
        while (*p && isspace(*p)) ++p;
        if (!*p) break;
        q = p;
        while (*p && !isspace(*p)) ++p;

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

        mount_default = 0;
        fprintf(stderr, "SDL: adding http mount '%s' -> '%s'\n", source,
                target);
        rtn = mount(source, target, "httpfs", 0, "");
        if (rtn != 0)
          fprintf(stderr, "SDL: mount failed: %s\n", strerror(errno));
      }
    }
  }

  if (mount_default) {
    // Create an http mount by default
    fprintf(stderr, "SDL: creating default http mount\n");
    rtn = chdir("/home");
    if (rtn != 0)
      fprintf(stderr, "SDL: chdir failed: %s\n", strerror(errno));
    rtn = mount(".", "/home", "httpfs", 0, "");
    if (rtn != 0)
      fprintf(stderr, "SDL: mount failed: %s\n", strerror(errno));
  }

  for (i = 0; i < g_argc; i++) {
    nacl_io_log("SDL: arg=%s\n", g_argn[i]);
    if (!strcasecmp(g_argn[i], "PWD")) {
      int rtn = chdir(g_argv[i]);
      if (rtn != 0)
        nacl_io_log("SDL: chdir(%s) failed\n", g_argv[i]);
      else
        nacl_io_log("SDL: chdir(%s)\n", g_argv[i]);
    } else if (!strcmp(g_argn[i], "sdl_tar_extract")) {
      const char* source;
      const char* target = "/";
      char* sep;
      char buf[64];
      int n;

      const char* q, *p = g_argv[i];
      while (*p) {
        while (*p && isspace(*p)) ++p;
        if (!*p) break;
        q = p;
        while (*p && !isspace(*p)) ++p;

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

        fprintf(stderr, "SDL: extracting tar file '%s' -> '%s'\n", source,
                target);

        TAR* tar;
        rtn = tar_open(&tar, (char*)source, NULL, O_RDONLY, 0, 0);
        if (rtn != 0) {
          fprintf(stderr, "SDL: tar_open failed\n");
          assert(0);
          continue;
        }

        rtn = mkdir(target, S_IRWXU | S_IRWXG);
        if (rtn != 0)
          fprintf(stderr, "SDL: mkdir failed: %s\n", strerror(errno));

        rtn = tar_extract_all(tar, (char*)target);
        if (rtn != 0) {
          fprintf(stderr, "SDL: tar_extract_all failed\n");
          assert(0);
        }

        rtn = tar_close(tar);
        if (rtn != 0) {
          fprintf(stderr, "SDL: tar_clone failed\n");
          assert(0);
        }
      }
    }
  }
}

static void Instance_DidChangeFocus(PP_Instance instance, PP_Bool has_focus) {}

static PP_Bool Instance_HandleDocumentLoad(PP_Instance instance,
                                           PP_Resource url_loader) {
  return PP_FALSE;
}

static PP_Bool InputEvent_HandleInputEvent(PP_Instance instance,
                                           PP_Resource input_event) {
  SDL_NACL_PushEvent(input_event);
  return PP_TRUE;
}

PP_EXPORT int32_t
PPP_InitializeModule(PP_Module module, PPB_GetInterface get_browser) {
  g_get_browser_interface = get_browser;
  g_input_event_interface =
      (struct PPB_InputEvent_1_0*)get_browser(PPB_INPUT_EVENT_INTERFACE_1_0);
  g_messaging_interface =
      (struct PPB_InputEvent_1_0*)get_browser(PPB_MESSAGING_INTERFACE_1_0);
  g_var_interface = (struct PPB_Var_1_1*)get_browser(PPB_VAR_INTERFACE_1_1);
  g_view_interface = (struct PPB_View_1_1*)get_browser(PPB_VIEW_INTERFACE_1_1);
  return PP_OK;
}

PP_EXPORT const void* PPP_GetInterface(const char* interface_name) {
  if (strcmp(interface_name, PPP_INSTANCE_INTERFACE) == 0) {
    static struct PPP_Instance_1_1 instance_interface = {
        &Instance_DidCreate,
        &Instance_DidDestroy,
        &Instance_DidChangeView,
        &Instance_DidChangeFocus,
        &Instance_HandleDocumentLoad,
    };
    return &instance_interface;
  } else if (strcmp(interface_name, PPP_INPUT_EVENT_INTERFACE_0_1) == 0) {
    static struct PPP_InputEvent_0_1 input_event_interface = {
        &InputEvent_HandleInputEvent,
    };
    return &input_event_interface;
  }
  return NULL;
}

PP_EXPORT void PPP_ShutdownModule() {}
