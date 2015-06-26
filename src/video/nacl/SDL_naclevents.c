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

#include "SDL.h"
#include "../../events/SDL_sysevents.h"
#include "../../events/SDL_events_c.h"
#include "../../SDL_trace.h"
#include "SDL_naclevents.h"
#include "SDL_naclvideo.h"

#include <math.h>
#include <ppapi_simple/ps_event.h>

#define PPAPI_KEY_CTRL 17
#define PPAPI_KEY_ALT 18

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
        case PPAPI_KEY_CTRL:
            return SDLK_LCTRL;
        case PPAPI_KEY_ALT:
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

void HandleInputEvent(_THIS, PP_Resource event) {
    static Uint8 last_scancode = 0;
    static int alt_down = 0;
    static int ctrl_down = 0;
    PP_InputEvent_Type type;
    PP_InputEvent_Modifier modifiers;
    Uint8 button;
    Uint8 state;
    Uint8 gained;
    struct PP_Point location;
    struct PP_FloatPoint ticks;
    SDL_keysym keysym;
    uint32_t unicode_var_len;
    struct PP_Var unicode_var;
    struct SDL_PrivateVideoData *dd = _this->hidden;
    static double wheel_clicks_x;
    static double wheel_clicks_y;
    int sdl_wheel_clicks_x;
    int sdl_wheel_clicks_y;
    int i;

    type = dd->ppb_input_event->GetType(event);
    modifiers = dd->ppb_input_event->GetModifiers(event);

    switch (type) {
        case PP_INPUTEVENT_TYPE_MOUSEDOWN:
        case PP_INPUTEVENT_TYPE_MOUSEUP:
            location = dd->ppb_mouse_input_event->GetPosition(event);
            button = translateButton(dd->ppb_mouse_input_event->GetButton(event));
            state = (type == PP_INPUTEVENT_TYPE_MOUSEDOWN) ? SDL_PRESSED : SDL_RELEASED;
            SDL_PrivateMouseButton(state, button, location.x, location.y);
            break;
        case PP_INPUTEVENT_TYPE_WHEEL:
            ticks = dd->ppb_wheel_input_event->GetTicks(event);
            wheel_clicks_x += ticks.x;
            wheel_clicks_y += ticks.y;
            sdl_wheel_clicks_x = trunc(wheel_clicks_x);
            sdl_wheel_clicks_y = trunc(wheel_clicks_y);
            button = (sdl_wheel_clicks_x > 0) ? SDL_BUTTON_X1 : SDL_BUTTON_X2;
            for (i = 0; i < abs(sdl_wheel_clicks_x); i++) {
              SDL_PrivateMouseButton(SDL_MOUSEBUTTONDOWN, button, 0, 0);
              SDL_PrivateMouseButton(SDL_MOUSEBUTTONUP, button, 0, 0);
            }
            button = (sdl_wheel_clicks_y > 0) ? SDL_BUTTON_WHEELUP : SDL_BUTTON_WHEELDOWN;
            for (i = 0; i < abs(sdl_wheel_clicks_y); i++) {
              SDL_PrivateMouseButton(SDL_MOUSEBUTTONDOWN, button, 0, 0);
              SDL_PrivateMouseButton(SDL_MOUSEBUTTONUP, button, 0, 0);
            }
            wheel_clicks_x -= sdl_wheel_clicks_x;
            wheel_clicks_y -= sdl_wheel_clicks_y;
            break;

        case PP_INPUTEVENT_TYPE_MOUSELEAVE:
        case PP_INPUTEVENT_TYPE_MOUSEENTER:
            gained = (type == PP_INPUTEVENT_TYPE_MOUSEENTER) ? 1 : 0;
            SDL_PrivateAppActive(gained, SDL_APPMOUSEFOCUS);
            break;

        case PP_INPUTEVENT_TYPE_MOUSEMOVE:
            location = dd->ppb_mouse_input_event->GetPosition(event);
            SDL_PrivateMouseMotion(0, 0, location.x, location.y);
            break;

        case PP_INPUTEVENT_TYPE_TOUCHSTART:
        case PP_INPUTEVENT_TYPE_TOUCHMOVE:
        case PP_INPUTEVENT_TYPE_TOUCHEND:
        case PP_INPUTEVENT_TYPE_TOUCHCANCEL:
            /* FIXME: Touch events */
            break;

        case PP_INPUTEVENT_TYPE_KEYDOWN:
        case PP_INPUTEVENT_TYPE_KEYUP:
        case PP_INPUTEVENT_TYPE_CHAR:
            // PPAPI sends us separate events for KEYDOWN and CHAR; the first one
            // contains only the keycode, the second one - only the unicode text.
            // SDL wants both in SDL_PRESSED event :(
            // For now, ignore the keydown event for printable ascii (32-126) as we
            // know we'll get a char event and can set sym directly. For everything
            // else, risk sending an extra SDL_PRESSED with unicode text and zero
            // keycode for scancode / sym.
            // It seems that SDL 1.3 is better in this regard.
            keysym.scancode = dd->ppb_keyboard_input_event->GetKeyCode(event);
            unicode_var = dd->ppb_keyboard_input_event->GetCharacterText(event);
            keysym.unicode = dd->ppb_var->VarToUtf8(unicode_var, &unicode_var_len)[0];
            dd->ppb_var->Release(unicode_var);
            keysym.sym = translateKey(keysym.scancode);

            state = SDL_PRESSED;
            if (type == PP_INPUTEVENT_TYPE_KEYDOWN) {
#if !defined(NDEBUG)
              fprintf(stderr, "KEYDOWN: %d\n", keysym.scancode);
#endif
              /*
               * PPAPI + Javascript recently started skipping KEYPRESS
               * events for letter + number keys when ALT is pressed.
               * Rather than wait for KEYPRESS to complete a keystroke,
               * emit immediately when ALT is pressed.
               * A similar situation exists for CTRL-H, CTRL-M, CTRL-Space.
               * Handling these explicitly as well (they require control
               * code decoding).
               * NOTE: The old behavior continues on OSX, so we must
               * also ignore KEYPRESS codes in case we are on OSX.
               */
              if (keysym.scancode == PPAPI_KEY_CTRL) ctrl_down = 1;
              if (keysym.scancode == PPAPI_KEY_ALT) alt_down = 1;
              last_scancode = keysym.scancode;
              if (ctrl_down && keysym.scancode == ' ') {
                /* Ctrl-Space == Ctrl-@ == '\0' */
                keysym.unicode = 0;
              } else if (ctrl_down && (keysym.scancode == 'H' ||
                                       keysym.scancode == 'M')) {
                /* Convert from [A - Z] to [Ctrl-A - Ctrl-Z]. */
                keysym.unicode = keysym.scancode - '@';
              } else if (keysym.sym >= ' ' &&  keysym.sym <= 126 && !alt_down) {
                return;
              }
            } else if (type == PP_INPUTEVENT_TYPE_CHAR) {
#if !defined(NDEBUG)
              fprintf(stderr, "CHAR: %d %d %d\n",
                  keysym.sym, keysym.unicode, last_scancode);
#endif
              if (ctrl_down &&
                  (keysym.sym == ' ' ||
                   keysym.sym == 'H' ||
                   keysym.sym == 'M')) return;
              if (alt_down) return;
              if (keysym.sym >= ' ' &&  keysym.sym <= 126) {
                keysym.scancode = translateAscii(keysym.unicode);
                keysym.sym = translateKey(keysym.scancode);
              } else if (last_scancode) {
                keysym.scancode = last_scancode;
                keysym.sym = translateKey(keysym.scancode);
              }
            } else {  // event->type == PP_INPUTEVENT_TYPE_KEYUP
#if !defined(NDEBUG)
              fprintf(stderr, "KEYUP: %d\n", keysym.scancode);
#endif
              if (keysym.scancode == PPAPI_KEY_CTRL) ctrl_down = 0;
              if (keysym.scancode == PPAPI_KEY_ALT) alt_down = 0;
              state = SDL_RELEASED;
              last_scancode = 0;
            }
            keysym.mod = KMOD_NONE;
            SDL_TRACE("Key event: %d: %s\n", state, SDL_GetKeyName(keysym.sym));
            SDL_PrivateKeyboard(state, &keysym);
            break;

        default:
            SDL_TRACE("got unhandled PSEvent: %d\n", type);
            break;
    }
}

static void HandleEvent(_THIS, PSEvent* ps_event) {
    PP_Resource event = ps_event->as_resource;
    struct PP_Rect rect;

    switch (ps_event->type) {
        /* From DidChangeView, contains a view resource */
        case PSE_INSTANCE_DIDCHANGEVIEW:
            _this->hidden->ppb_view->GetRect(event, &rect);
            NACL_SetScreenResolution(rect.size.width, rect.size.height);
            // FIXME: Rebuild context? See life.c UpdateContext
            break;

        /* From HandleInputEvent, contains an input resource. */
        case PSE_INSTANCE_HANDLEINPUT:
            HandleInputEvent(_this, event);
            break;

        /* From HandleMessage, contains a PP_Var. */
        case PSE_INSTANCE_HANDLEMESSAGE:
            break;

        /* From DidChangeFocus, contains a PP_Bool with the current focus state. */
        case PSE_INSTANCE_DIDCHANGEFOCUS:
            break;

        /* When the 3D context is lost, no resource. */
        case PSE_GRAPHICS3D_GRAPHICS3DCONTEXTLOST:
            break;

        /* When the mouse lock is lost. */
        case PSE_MOUSELOCK_MOUSELOCKLOST:
            break;

        default:
            break;
    }
}

void NACL_PumpEvents(_THIS) {
    PSEvent* ps_event;

    while ((ps_event = PSEventTryAcquire()) != NULL) {
        HandleEvent(_this, ps_event);
        PSEventRelease(ps_event);
    }
}

void NACL_InitOSKeymap(_THIS) {
    /* do nothing. */
}

