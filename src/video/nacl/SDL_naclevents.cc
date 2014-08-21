#include "SDL_config.h"

#include "SDL_nacl.h"

extern "C" {
#include "SDL.h"
#include "../../events/SDL_sysevents.h"
#include "../../events/SDL_events_c.h"
}

#include "SDL_naclevents_c.h"
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

// Translate ASCII code to browser keycode
static uint8_t translateAscii(uint8_t ascii) {
  if ('A' <= ascii && ascii <= 'Z') {
    return ascii;
  } else if ('a' <= ascii && ascii <= 'z') {
    return toupper(ascii);
  } else if ('0' <= ascii && ascii <= '9') {
    return ascii;
  } else if (' ' == ascii || '\r' == ascii || '\t' == ascii ||
             '\x1b' == ascii || '\b' == ascii) {
    return ascii;
  } else {
    switch (ascii) {
      case '!': return '1';
      case '@': return '2';
      case '#': return '3';
      case '$': return '4';
      case '%': return '5';
      case '^': return '6';
      case '&': return '7';
      case '*': return '8';
      case '(': return '9';
      case ')': return '0';
      case ';': case ':': return 186;
      case '=': case '+': return 187;
      case ',': case '<': return 188;
      case '-': case '_': return 189;
      case '.': case '>': return 190;
      case '/': case '?': return 191;
      case '`': case '~': return 192;
      case '[': case '{': return 219;
      case '\\': case '|': return 220;
      case ']': case '}': return 221;
      case '\'': case '"': return 222;
      default:
        break;
    }
  }
  return 0;
}

// Translate browser keycode to SDLKey
static SDLKey translateKey(uint32_t code) {
  if (code >= 'A' && code <= 'Z')
    return (SDLKey)(code - 'A' + SDLK_a);
  if (code >= SDLK_0 && code <= SDLK_9)
    return (SDLKey)code;
  const uint32_t f1_code = 112;
  if (code >= f1_code && code < f1_code + 12)
    return (SDLKey)(code - f1_code + SDLK_F1);
  const uint32_t kp0_code = 96;
  if (code >= kp0_code && code < kp0_code + 10)
    return (SDLKey)(code - kp0_code + SDLK_KP0);
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
  static Uint8 last_scancode = 0;
  SDL_keysym keysym;
  static double wheel_clicks_x;
  static double wheel_clicks_y;
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
    PP_FloatPoint ticks =
        g_nacl_wheel_input_event_interface->GetTicks(input_event);
    wheel_clicks_x += ticks.x;
    wheel_clicks_y += ticks.y;
    int sdl_wheel_clicks_x = trunc(wheel_clicks_x);
    int sdl_wheel_clicks_y = trunc(wheel_clicks_y);
    event.button.x = event.button.y = 0;
    event.button.button =
        (sdl_wheel_clicks_x > 0) ? SDL_BUTTON_X1 : SDL_BUTTON_X2;
    for (int i = 0; i < abs(sdl_wheel_clicks_x); i++) {
      event.type = SDL_MOUSEBUTTONDOWN;
      event_queue.PushEvent(copyEvent(&event));
      event.type = SDL_MOUSEBUTTONUP;
      event_queue.PushEvent(copyEvent(&event));
    }
    event.button.button =
        (sdl_wheel_clicks_y > 0) ? SDL_BUTTON_WHEELUP : SDL_BUTTON_WHEELDOWN;
    for (int i = 0; i < abs(sdl_wheel_clicks_y); i++) {
      event.type = SDL_MOUSEBUTTONDOWN;
      event_queue.PushEvent(copyEvent(&event));
      event.type = SDL_MOUSEBUTTONUP;
      event_queue.PushEvent(copyEvent(&event));
    }
    wheel_clicks_x -= sdl_wheel_clicks_x;
    wheel_clicks_y -= sdl_wheel_clicks_y;
  } else if (type == PP_INPUTEVENT_TYPE_MOUSEMOVE) {
    event.type = SDL_MOUSEMOTION;
    PP_Point point =
        g_nacl_mouse_input_event_interface->GetPosition(input_event);
    event.motion.x = point.x;
    event.motion.y = point.y;
    event_queue.PushEvent(copyEvent(&event));
  } else if (type == PP_INPUTEVENT_TYPE_KEYDOWN ||
             type == PP_INPUTEVENT_TYPE_KEYUP ||
             type == PP_INPUTEVENT_TYPE_CHAR) {
    // PPAPI sends us separate events for KEYDOWN and CHAR; the first one
    // contains only the keycode, the second one - only the unicode text.
    // SDL wants both in SDL_PRESSED event :(
    // For now, ignore the keydown event for printable ascii (32-126) as we
    // know we'll get a char event and can set sym directly. For everything
    // else, risk sending an extra SDL_PRESSED with unicode text and zero
    // keycode for scancode / sym.
    // It seems that SDL 1.3 is better in this regard.
    keysym.scancode =
        g_nacl_keyboard_input_event_interface->GetKeyCode(input_event);
    PP_Var unicode_var =
        g_nacl_keyboard_input_event_interface->GetCharacterText(input_event);
    uint32_t unicode_var_len;
    keysym.unicode =
        g_nacl_var_interface->VarToUtf8(unicode_var, &unicode_var_len)[0];
    g_nacl_var_interface->Release(unicode_var);
    keysym.sym = translateKey(keysym.scancode);
    if (type == PP_INPUTEVENT_TYPE_KEYDOWN) {
      event.type = SDL_KEYDOWN;
      last_scancode = keysym.scancode;
      if (keysym.sym >= ' ' &&  keysym.sym <= 126) {
        return;
      }
    } else if (type == PP_INPUTEVENT_TYPE_CHAR) {
      event.type = SDL_KEYDOWN;
      if (keysym.sym >= ' ' &&  keysym.sym <= 126) {
        keysym.scancode = translateAscii(keysym.unicode);
        keysym.sym = translateKey(keysym.scancode);
      } else if (last_scancode) {
        keysym.scancode = last_scancode;
        keysym.sym = translateKey(keysym.scancode);
      }
    } else {  // event->type == PP_INPUTEVENT_TYPE_KEYUP
      event.type = SDL_KEYUP;
      last_scancode = 0;
    }
    keysym.mod = KMOD_NONE;
    event.key.keysym = keysym;
    event_queue.PushEvent(copyEvent(&event));
  } else if (type == PP_INPUTEVENT_TYPE_MOUSEENTER ||
             type == PP_INPUTEVENT_TYPE_MOUSELEAVE) {
    event.type = SDL_ACTIVEEVENT;
    event.active.gain = (type == PP_INPUTEVENT_TYPE_MOUSEENTER) ? 1 : 0;
    event.active.state = SDL_APPMOUSEFOCUS;
    event_queue.PushEvent(copyEvent(&event));
  }
}

void SDL_NACL_SetHasFocus(int has_focus) {
  SDL_Event event;
  event.type = SDL_ACTIVEEVENT;
  event.active.gain = has_focus ? 1 : 0;
  event.active.state = SDL_APPINPUTFOCUS;
  event_queue.PushEvent(copyEvent(&event));
}

void SDL_NACL_SetPageVisible(int is_visible) {
  SDL_Event event;
  event.type = SDL_ACTIVEEVENT;
  event.active.gain = is_visible ? 1 : 0;
  event.active.state = SDL_APPACTIVE;
  event_queue.PushEvent(copyEvent(&event));
}

void NACL_PumpEvents(_THIS) {
  SDL_Event* event;
  while ((event = event_queue.PopEvent())) {
    if (event->type == SDL_MOUSEBUTTONDOWN) {
      SDL_PrivateMouseButton(SDL_PRESSED, event->button.button,
                             event->button.x, event->button.y);
    } else if (event->type == SDL_MOUSEBUTTONUP) {
      SDL_PrivateMouseButton(SDL_RELEASED, event->button.button,
                             event->button.x, event->button.y);
    } else if (event->type == SDL_MOUSEMOTION) {
      SDL_PrivateMouseMotion(0, 0, event->motion.x, event->motion.y);
    } else if (event->type == SDL_KEYDOWN) {
      SDL_PrivateKeyboard(SDL_PRESSED, &event->key.keysym);
    } else if (event->type == SDL_KEYUP) {
      SDL_PrivateKeyboard(SDL_RELEASED, &event->key.keysym);
    } else if (event->type == SDL_ACTIVEEVENT) {
      SDL_PrivateAppActive(event->active.gain, event->active.state);
    }
    free(event);
  }
}

void NACL_InitOSKeymap(_THIS) {
  /* do nothing. */
}
