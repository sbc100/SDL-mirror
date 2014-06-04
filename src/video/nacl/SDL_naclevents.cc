#include "../../SDL_internal.h"
#include "SDL_config.h"

#include "SDL_nacl.h"
#include "SDL.h"

extern "C" {
#include "../../events/SDL_sysevents.h"
#include "../../events/SDL_events_c.h"
}

#include "SDL_naclevents.h"
#include "eventqueue.h"
#include <ppapi/c/pp_point.h>
#include <ppapi/c/pp_var.h>
#include <ppapi/c/ppb_input_event.h>
#include <ppapi/c/ppb_var.h>

#include <math.h>

extern const PPB_InputEvent_1_0 *g_nacl_input_event_interface;
extern const PPB_MouseInputEvent_1_1 *g_nacl_mouse_input_event_interface;
extern const PPB_WheelInputEvent_1_0 *g_nacl_wheel_input_event_interface;
extern const PPB_KeyboardInputEvent_1_0 *g_nacl_keyboard_input_event_interface;
extern const PPB_Var_1_1 *g_nacl_var_interface;

static EventQueue event_queue;

static Uint8 translateButton(int32_t button) {
  switch (button) {
    case PP_INPUTEVENT_MOUSEBUTTON_LEFT:
      return SDL_BUTTON_LEFT;
    case PP_INPUTEVENT_MOUSEBUTTON_MIDDLE:
      return SDL_BUTTON_MIDDLE;
    case PP_INPUTEVENT_MOUSEBUTTON_RIGHT:
      return SDL_BUTTON_RIGHT;
    case PP_INPUTEVENT_MOUSEBUTTON_NONE:
    default:
      return 0;
  }
}

// Translate browser keycode to SDL_Keycode
static SDL_Keycode translateKey(uint32_t code) {
  if (code >= 'A' && code <= 'Z')
    return (SDL_Keycode)(code - 'A' + SDLK_a);
  if (code >= SDLK_0 && code <= SDLK_9)
    return (SDL_Keycode)code;
  const uint32_t f1_code = 112;
  if (code >= f1_code && code < f1_code + 12)
    return (SDL_Keycode)(code - f1_code + SDLK_F1);
  const uint32_t kp0_code = 96;
  if (code >= kp0_code && code < kp0_code + 10)
    return (SDL_Keycode)(code - kp0_code + SDLK_KP_0);
  switch (code) {
    case SDLK_BACKSPACE:
      return SDLK_BACKSPACE;
    case SDLK_TAB:
      return SDLK_TAB;
    case SDLK_RETURN:
      return SDLK_RETURN;
    case SDLK_PAUSE:
      return SDLK_PAUSE;
    case SDLK_ESCAPE:
      return SDLK_ESCAPE;
    case 16:
      return SDLK_LSHIFT;
    case 17:
      return SDLK_LCTRL;
    case 18:
      return SDLK_LALT;
    case 32:
      return SDLK_SPACE;
    case 37:
      return SDLK_LEFT;
    case 38:
      return SDLK_UP;
    case 39:
      return SDLK_RIGHT;
    case 40:
      return SDLK_DOWN;
    case 106:
      return SDLK_KP_MULTIPLY;
    case 107:
      return SDLK_KP_PLUS;
    case 109:
      return SDLK_KP_MINUS;
    case 110:
      return SDLK_KP_PERIOD;
    case 111:
      return SDLK_KP_DIVIDE;
    case 45:
      return SDLK_INSERT;
    case 46:
      return SDLK_DELETE;
    case 36:
      return SDLK_HOME;
    case 35:
      return SDLK_END;
    case 33:
      return SDLK_PAGEUP;
    case 34:
      return SDLK_PAGEDOWN;
    case 189:
      return SDLK_MINUS;
    case 187:
      return SDLK_EQUALS;
    case 219:
      return SDLK_LEFTBRACKET;
    case 221:
      return SDLK_RIGHTBRACKET;
    case 186:
      return SDLK_SEMICOLON;
    case 222:
      return SDLK_QUOTE;
    case 220:
      return SDLK_BACKSLASH;
    case 188:
      return SDLK_COMMA;
    case 190:
      return SDLK_PERIOD;
    case 191:
      return SDLK_SLASH;
    case 192:
      return SDLK_BACKQUOTE;
    default:
      return SDLK_UNKNOWN;
  }
}

static SDL_Event *copyEvent(SDL_Event *event) {
  SDL_Event *event_copy = (SDL_Event*)malloc(sizeof(SDL_Event));
  *event_copy = *event;
  return event_copy;
}

void SDL_NACL_PushEvent(PP_Resource input_event) {
  SDL_Event event;
  PP_InputEvent_Type type = g_nacl_input_event_interface->GetType(input_event);

  if (type == PP_INPUTEVENT_TYPE_MOUSEDOWN ||
      type == PP_INPUTEVENT_TYPE_MOUSEUP) {
    event.type = (type == PP_INPUTEVENT_TYPE_MOUSEUP) ? SDL_MOUSEBUTTONUP
                                                      : SDL_MOUSEBUTTONDOWN;
    event.button.button = translateButton(
        g_nacl_mouse_input_event_interface->GetButton(input_event));
    PP_Point point =
        g_nacl_mouse_input_event_interface->GetPosition(input_event);
    event.button.x = point.x;
    event.button.y = point.y;
    event_queue.PushEvent(copyEvent(&event));
  } else if (type == PP_INPUTEVENT_TYPE_WHEEL) {
      event.type = SDL_MOUSEWHEEL; // I ain't no buttonz ;)
      struct PP_FloatPoint delta = g_nacl_wheel_input_event_interface->GetDelta(input_event);
      event.wheel.x = delta.x;
      event.wheel.y = delta.y;
      event_queue.PushEvent(copyEvent(&event));
  } else if (type == PP_INPUTEVENT_TYPE_MOUSEMOVE) {
    event.type = SDL_MOUSEMOTION;
    PP_Point point =
        g_nacl_mouse_input_event_interface->GetPosition(input_event);
    event.motion.x = point.x;
    event.motion.y = point.y;
    event_queue.PushEvent(copyEvent(&event));
  } else if (type == PP_INPUTEVENT_TYPE_KEYDOWN ||
             type == PP_INPUTEVENT_TYPE_KEYUP) {
    // PPAPI sends us separate events for KEYDOWN and CHAR; the first one
    // contains only the scancode, the second one only the unicode text.
    // Then there is an IME composition event. All this is what SDL2 expects to get.
    event.key.keysym.scancode = SDL_GetScancodeFromKey(
        translateKey(
            g_nacl_keyboard_input_event_interface->GetKeyCode(input_event)
        )
    );
    if (type == PP_INPUTEVENT_TYPE_KEYDOWN) {
        event.type = SDL_KEYDOWN;
    } else {
        event.type = SDL_KEYUP;
    }
    event_queue.PushEvent(copyEvent(&event));
    //FIXME: I am confused about this
  /*} else if (type == PP_INPUTEVENT_TYPE_IME) {
      struct PP_Var var = g_nacl_ime_input_event_interface->GetText(input_event);
      int n;
      char* text = g_nacl_var_interface->VarToUtf8(var, &n);
      g_nacl_var_interface->Release(var);
      //FIXME: who is supposed to free this?
      char* text0 = calloc(sizeof(char), n+1);
      memcpy(text0, text, n);
      text0[n] = '\0';
      event.type = SDL_TEXTEDITING;
      event.edit.text = text0; // http://wiki.libsdl.org/SDL_TextInputEvent
      event_queue.PushEvent(copyEvent(&event));
  } else if (type == PP_INPUTEVENT_TYPE_CHAR) {
  } else if (type == PP_INPUTEVENT_TYPE_IME_TEXT) {
      event.type = SDL_TEXTINPUT;
      
    event_queue.PushEvent(copyEvent(&event)); */
  } else if (type == PP_INPUTEVENT_TYPE_MOUSEENTER ||
             type == PP_INPUTEVENT_TYPE_MOUSELEAVE) {
    event.type = SDL_WINDOWEVENT;
    event.window.event = (type == PP_INPUTEVENT_TYPE_MOUSEENTER) ?
        SDL_WINDOWEVENT_ENTER : SDL_WINDOWEVENT_LEAVE;
    event_queue.PushEvent(copyEvent(&event));
  }
}

/*
void SDL_NACL_SetHasFocus(int has_focus) {
  SDL_Event event;
  event.type = SDL_ACTIVEEVENT;
  event.active.gain = has_focus ? 1 : 0;
  event.active.state = SDL_APPINPUTFOCUS;
  //SDL_SetMouseFocus(NULL);?
  event_queue.PushEvent(copyEvent(&event));
}

void SDL_NACL_SetPageVisible(int is_visible) {
  SDL_Event event;
  event.type = SDL_ACTIVEEVENT;
  event.active.gain = is_visible ? 1 : 0;
  event.active.state = SDL_APPACTIVE;
  event_queue.PushEvent(copyEvent(&event));
}
*/

void NACL_PumpEvents(_THIS) {
  SDL_Event* event;
  SDL_Mouse *mouse = SDL_GetMouse();
  while ( (event = event_queue.PopEvent()) ) {
    if (event->type == SDL_MOUSEBUTTONDOWN) {
        SDL_SendMouseButton(mouse->focus, mouse->mouseID, SDL_PRESSED, event->button.button);
    } else if (event->type == SDL_MOUSEBUTTONUP) {
        SDL_SendMouseButton(mouse->focus, mouse->mouseID, SDL_RELEASED, event->button.button);
    } else if (event->type == SDL_MOUSEMOTION) {
        const SDL_bool relative = SDL_TRUE;
        SDL_SendMouseMotion(mouse->focus, mouse->mouseID, relative, event->motion.x, event->motion.y);
    } else if (event->type == SDL_MOUSEWHEEL) {
        SDL_SendMouseWheel(mouse->focus, mouse->mouseID, event->wheel.x, event->wheel.y);
    } else if (event->type == SDL_KEYDOWN) {
        SDL_SendKeyboardKey(SDL_PRESSED, event->key.keysym.scancode);
    } else if (event->type == SDL_KEYUP) {
        SDL_SendKeyboardKey(SDL_RELEASED, event->key.keysym.scancode);
    } else if (event->type == SDL_WINDOWEVENT) {
        SDL_SendWindowEvent(NACL_Window, event->window.event, event->window.data1, event->window.data2);
    } else {
        assert(false);
    }
    free(event);
  }
}

void NACL_InitOSKeymap(_THIS) {
  /* do nothing. */
}
