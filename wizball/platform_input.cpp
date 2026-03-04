// SDL-only input backend.
//
// This file provides the old Allegro-style input API expected by the rest of the
// codebase (keyboard scancodes 0..114, mouse x/y/z + button mask, joystick ports,
// sticks/axes/buttons), but implemented purely using SDL2.

#include "platform_input.h"

#if __has_include(<SDL2/SDL.h>)
#include <SDL2/SDL.h>
#else
#include <SDL.h>
#endif
#include <algorithm>
#include <vector>
#include <string.h>

// -----------------------------------------------------------------------------
// Internal state
// -----------------------------------------------------------------------------

static bool g_sdl_video_inited = false;
static bool g_sdl_events_inited = false;
static bool g_sdl_joystick_inited = false;

static bool g_quit_requested = false;

static unsigned int g_debug_pump_count = 0;
static unsigned int g_debug_event_count = 0;
static unsigned int g_debug_window_event_count = 0;
static unsigned int g_debug_keydown_event_count = 0;
static unsigned int g_debug_quit_event_count = 0;

static int g_mouse_x = 0;
static int g_mouse_y = 0;
static int g_mouse_z = 0; // accumulated wheel
static int g_mouse_buttons_mask = 0;

struct JoyPort
{
    SDL_Joystick *joy = NULL;
    SDL_GameController *controller = NULL; // preferred when available
    bool is_controller = false;
};

static std::vector<JoyPort> g_joyports;

// Clamp number of opened joysticks to match old code expectations.
static const int kMaxJoyPorts = 8;

static bool g_need_rescan_joy = false;

// SDL event pump.
// - Keeps g_quit_requested updated.
// - Accumulates mouse wheel into g_mouse_z.
static void pump_events_nonblocking()
{
    if (!g_sdl_events_inited)
        return;

    ++g_debug_pump_count;

    SDL_Event e;
    while (SDL_PollEvent(&e))
    {
        ++g_debug_event_count;
        switch (e.type)
        {
        case SDL_QUIT:
            g_quit_requested = true;
            ++g_debug_quit_event_count;
            break;
        case SDL_MOUSEWHEEL:
            // SDL's wheel is typically +/-1 per notch. Preserve sign.
            g_mouse_z += e.wheel.y;
            break;
        case SDL_KEYDOWN:
            ++g_debug_keydown_event_count;
            break;
        case SDL_WINDOWEVENT:
            ++g_debug_window_event_count;
            break;
        case SDL_CONTROLLERDEVICEADDED:
        case SDL_CONTROLLERDEVICEREMOVED:
        case SDL_JOYDEVICEADDED:
        case SDL_JOYDEVICEREMOVED:
            g_need_rescan_joy = true;
            break;
        default:
            break;
        }
    }
}

static SDL_Scancode map_allegro_scancode_to_sdl(int allegro_scancode)
{
    // Mapping follows the Allegro 4 scancode set the project uses.
    switch (allegro_scancode)
    {
    case KEY_A: return SDL_SCANCODE_A;
    case KEY_B: return SDL_SCANCODE_B;
    case KEY_C: return SDL_SCANCODE_C;
    case KEY_D: return SDL_SCANCODE_D;
    case KEY_E: return SDL_SCANCODE_E;
    case KEY_F: return SDL_SCANCODE_F;
    case KEY_G: return SDL_SCANCODE_G;
    case KEY_H: return SDL_SCANCODE_H;
    case KEY_I: return SDL_SCANCODE_I;
    case KEY_J: return SDL_SCANCODE_J;
    case KEY_K: return SDL_SCANCODE_K;
    case KEY_L: return SDL_SCANCODE_L;
    case KEY_M: return SDL_SCANCODE_M;
    case KEY_N: return SDL_SCANCODE_N;
    case KEY_O: return SDL_SCANCODE_O;
    case KEY_P: return SDL_SCANCODE_P;
    case KEY_Q: return SDL_SCANCODE_Q;
    case KEY_R: return SDL_SCANCODE_R;
    case KEY_S: return SDL_SCANCODE_S;
    case KEY_T: return SDL_SCANCODE_T;
    case KEY_U: return SDL_SCANCODE_U;
    case KEY_V: return SDL_SCANCODE_V;
    case KEY_W: return SDL_SCANCODE_W;
    case KEY_X: return SDL_SCANCODE_X;
    case KEY_Y: return SDL_SCANCODE_Y;
    case KEY_Z: return SDL_SCANCODE_Z;

    case KEY_0: return SDL_SCANCODE_0;
    case KEY_1: return SDL_SCANCODE_1;
    case KEY_2: return SDL_SCANCODE_2;
    case KEY_3: return SDL_SCANCODE_3;
    case KEY_4: return SDL_SCANCODE_4;
    case KEY_5: return SDL_SCANCODE_5;
    case KEY_6: return SDL_SCANCODE_6;
    case KEY_7: return SDL_SCANCODE_7;
    case KEY_8: return SDL_SCANCODE_8;
    case KEY_9: return SDL_SCANCODE_9;

    case KEY_0_PAD: return SDL_SCANCODE_KP_0;
    case KEY_1_PAD: return SDL_SCANCODE_KP_1;
    case KEY_2_PAD: return SDL_SCANCODE_KP_2;
    case KEY_3_PAD: return SDL_SCANCODE_KP_3;
    case KEY_4_PAD: return SDL_SCANCODE_KP_4;
    case KEY_5_PAD: return SDL_SCANCODE_KP_5;
    case KEY_6_PAD: return SDL_SCANCODE_KP_6;
    case KEY_7_PAD: return SDL_SCANCODE_KP_7;
    case KEY_8_PAD: return SDL_SCANCODE_KP_8;
    case KEY_9_PAD: return SDL_SCANCODE_KP_9;

    case KEY_F1: return SDL_SCANCODE_F1;
    case KEY_F2: return SDL_SCANCODE_F2;
    case KEY_F3: return SDL_SCANCODE_F3;
    case KEY_F4: return SDL_SCANCODE_F4;
    case KEY_F5: return SDL_SCANCODE_F5;
    case KEY_F6: return SDL_SCANCODE_F6;
    case KEY_F7: return SDL_SCANCODE_F7;
    case KEY_F8: return SDL_SCANCODE_F8;
    case KEY_F9: return SDL_SCANCODE_F9;
    case KEY_F10: return SDL_SCANCODE_F10;
    case KEY_F11: return SDL_SCANCODE_F11;
    case KEY_F12: return SDL_SCANCODE_F12;

    case KEY_ESC: return SDL_SCANCODE_ESCAPE;
    case KEY_TILDE: return SDL_SCANCODE_GRAVE;
    case KEY_MINUS: return SDL_SCANCODE_MINUS;
    case KEY_EQUALS: return SDL_SCANCODE_EQUALS;
    case KEY_BACKSPACE: return SDL_SCANCODE_BACKSPACE;
    case KEY_TAB: return SDL_SCANCODE_TAB;
    case KEY_OPENBRACE: return SDL_SCANCODE_LEFTBRACKET;
    case KEY_CLOSEBRACE: return SDL_SCANCODE_RIGHTBRACKET;
    case KEY_ENTER: return SDL_SCANCODE_RETURN;
    case KEY_SEMICOLON: return SDL_SCANCODE_SEMICOLON;
    case KEY_QUOTE: return SDL_SCANCODE_APOSTROPHE;
    case KEY_BACKSLASH: return SDL_SCANCODE_BACKSLASH;
    case KEY_BACKSLASH2: return SDL_SCANCODE_NONUSBACKSLASH;
    case KEY_COMMA: return SDL_SCANCODE_COMMA;
    case KEY_STOP: return SDL_SCANCODE_PERIOD;
    case KEY_SLASH: return SDL_SCANCODE_SLASH;
    case KEY_SPACE: return SDL_SCANCODE_SPACE;

    case KEY_INSERT: return SDL_SCANCODE_INSERT;
    case KEY_DEL: return SDL_SCANCODE_DELETE;
    case KEY_HOME: return SDL_SCANCODE_HOME;
    case KEY_END: return SDL_SCANCODE_END;
    case KEY_PGUP: return SDL_SCANCODE_PAGEUP;
    case KEY_PGDN: return SDL_SCANCODE_PAGEDOWN;
    case KEY_LEFT: return SDL_SCANCODE_LEFT;
    case KEY_RIGHT: return SDL_SCANCODE_RIGHT;
    case KEY_UP: return SDL_SCANCODE_UP;
    case KEY_DOWN: return SDL_SCANCODE_DOWN;

    case KEY_SLASH_PAD: return SDL_SCANCODE_KP_DIVIDE;
    case KEY_ASTERISK: return SDL_SCANCODE_KP_MULTIPLY;
    case KEY_MINUS_PAD: return SDL_SCANCODE_KP_MINUS;
    case KEY_PLUS_PAD: return SDL_SCANCODE_KP_PLUS;
    case KEY_DEL_PAD: return SDL_SCANCODE_KP_PERIOD;
    case KEY_ENTER_PAD: return SDL_SCANCODE_KP_ENTER;

    case KEY_PRTSCR: return SDL_SCANCODE_PRINTSCREEN;
    case KEY_PAUSE: return SDL_SCANCODE_PAUSE;

    case KEY_LSHIFT: return SDL_SCANCODE_LSHIFT;
    case KEY_RSHIFT: return SDL_SCANCODE_RSHIFT;
    case KEY_LCONTROL: return SDL_SCANCODE_LCTRL;
    case KEY_RCONTROL: return SDL_SCANCODE_RCTRL;
    case KEY_ALT: return SDL_SCANCODE_LALT;
    case KEY_ALTGR: return SDL_SCANCODE_RALT;
    case KEY_LWIN: return SDL_SCANCODE_LGUI;
    case KEY_RWIN: return SDL_SCANCODE_RGUI;
    case KEY_MENU: return SDL_SCANCODE_APPLICATION;

    case KEY_SCRLOCK: return SDL_SCANCODE_SCROLLLOCK;
    case KEY_NUMLOCK: return SDL_SCANCODE_NUMLOCKCLEAR;
    case KEY_CAPSLOCK: return SDL_SCANCODE_CAPSLOCK;

    // Rare/IME keys: map to unknown for now.
    default:
        return SDL_SCANCODE_UNKNOWN;
    }
}

static void ensure_sdl_events()
{
    if (!g_sdl_events_inited)
    {
        // If the app already inited SDL elsewhere, SDL_InitSubSystem will succeed.
        SDL_InitSubSystem(SDL_INIT_EVENTS);
        g_sdl_events_inited = true;
    }
}

static void ensure_sdl_video()
{
    if (!g_sdl_video_inited)
    {
        SDL_InitSubSystem(SDL_INIT_VIDEO);
        g_sdl_video_inited = true;
    }
}

// -----------------------------------------------------------------------------
// Public API
// -----------------------------------------------------------------------------

int PLATFORM_INPUT_install_keyboard(void)
{
    ensure_sdl_events();
    ensure_sdl_video(); // keyboard state is tied to the video subsystem on some platforms
    return 0;
}

int PLATFORM_INPUT_install_mouse(void)
{
    ensure_sdl_events();
    ensure_sdl_video();
    return 0;
}

void PLATFORM_INPUT_poll_mouse(void)
{
    ensure_sdl_events();
    ensure_sdl_video();
    // Do not pump events here (key_state may be called many times per frame).
    // Call PLATFORM_INPUT_begin_frame() or PLATFORM_INPUT_pump_events() once per frame instead.

    int x = 0, y = 0;
    Uint32 buttons = SDL_GetMouseState(&x, &y);
    g_mouse_x = x;
    g_mouse_y = y;

    // Convert SDL's button bitfield to the old (1<<buttonIndex) mask.
    // SDL_BUTTON_LEFT is 1, MIDDLE is 2, RIGHT is 3.
    g_mouse_buttons_mask = 0;
    if (buttons & SDL_BUTTON(SDL_BUTTON_LEFT))   g_mouse_buttons_mask |= (1 << 0);
    if (buttons & SDL_BUTTON(SDL_BUTTON_RIGHT))  g_mouse_buttons_mask |= (1 << 1);
    if (buttons & SDL_BUTTON(SDL_BUTTON_MIDDLE)) g_mouse_buttons_mask |= (1 << 2);
}

int PLATFORM_INPUT_key_state(int scancode)
{
    if (scancode < 0 || scancode >= KEY_MAX)
        return 0;

    ensure_sdl_events();
    ensure_sdl_video();
    // Do NOT pump here: CONTROL_update_all_input() calls PLATFORM_INPUT_begin_frame()
    // before the key loop, which already drains the event queue. Pumping once per
    // key (114 calls per update) wastes CPU and can interleave event state mid-scan.

    const Uint8 *keys = SDL_GetKeyboardState(NULL);
    if (!keys)
        return 0;

    SDL_Scancode sdl = map_allegro_scancode_to_sdl(scancode);
    if (sdl == SDL_SCANCODE_UNKNOWN)
        return 0;

    return keys[sdl] ? 1 : 0;
}

int PLATFORM_INPUT_mouse_x(void) { return g_mouse_x; }
int PLATFORM_INPUT_mouse_y(void) { return g_mouse_y; }
int PLATFORM_INPUT_mouse_z(void) { return g_mouse_z; }
int PLATFORM_INPUT_mouse_buttons_mask(void) { return g_mouse_buttons_mask; }

int PLATFORM_INPUT_num_joysticks(void)
{
    return (int)g_joyports.size();
}

int PLATFORM_INPUT_joystick_axis_pos(int port, int axis)
{
    // Convenience overload used in older codepaths.
    return PLATFORM_INPUT_joystick_axis_pos(port, 0, axis);
}

int PLATFORM_INPUT_joystick_needs_calibration(int /*port*/)
{
    // SDL doesn't require calibration in the old Allegro sense.
    return 0;
}

const char *PLATFORM_INPUT_calibrate_joystick_name(int /*port*/)
{
    return "";
}

int PLATFORM_INPUT_calibrate_joystick(int /*port*/)
{
    return 0;
}

int PLATFORM_INPUT_readkey_ascii(void)
{
    // Blocking wait for a key press; returns 27 for ESC, otherwise 0..255 when possible.
    ensure_sdl_events();
    ensure_sdl_video();

    SDL_Event e;
    while (SDL_WaitEvent(&e))
    {
        if (e.type == SDL_QUIT)
        {
            g_quit_requested = true;
            return 0;
        }

        if (e.type == SDL_KEYDOWN)
        {
            SDL_Keycode sym = e.key.keysym.sym;
            if (sym == SDLK_ESCAPE)
                return 27;

            // If it's a printable ASCII keycode, return it; else return 0.
            if (sym >= 0 && sym <= 255)
                return (int)sym;
            return 0;
        }
    }
    return 0;
}

int PLATFORM_INPUT_any_key_pressed(void)
{
    for (int i = 1; i < KEY_MAX; ++i)
    {
        if (PLATFORM_INPUT_key_state(i) != 0)
            return 1;
    }
    return 0;
}

int PLATFORM_INPUT_quit_requested(void)
{
    ensure_sdl_events();
    pump_events_nonblocking();
    return g_quit_requested ? 1 : 0;
}

void PLATFORM_INPUT_pump_events(void)
{
    ensure_sdl_events();
    pump_events_nonblocking();
}

void PLATFORM_INPUT_clear_quit_requested(void)
{
    g_quit_requested = false;
}

unsigned int PLATFORM_INPUT_debug_pump_count(void) { return g_debug_pump_count; }
unsigned int PLATFORM_INPUT_debug_event_count(void) { return g_debug_event_count; }
unsigned int PLATFORM_INPUT_debug_window_event_count(void) { return g_debug_window_event_count; }
unsigned int PLATFORM_INPUT_debug_keydown_event_count(void) { return g_debug_keydown_event_count; }
unsigned int PLATFORM_INPUT_debug_quit_event_count(void) { return g_debug_quit_event_count; }

// ------------------------------------------------------------
// Frame helpers
// ------------------------------------------------------------

void PLATFORM_INPUT_begin_frame(void)
{
    // Single entry point for callers.
    PLATFORM_INPUT_pump_events();
}

void PLATFORM_INPUT_warp_mouse(int x, int y)
{
    ensure_sdl_events();
    SDL_WarpMouseInWindow(NULL, x, y);
}

// ------------------------------------------------------------
// Joystick / controller
// ------------------------------------------------------------

static void close_joyports()
{
    for (JoyPort &jp : g_joyports)
    {
        if (jp.controller)
        {
            SDL_GameControllerClose(jp.controller);
            jp.controller = NULL;
            jp.joy = NULL;
        }
        else if (jp.joy)
        {
            SDL_JoystickClose(jp.joy);
            jp.joy = NULL;
        }
        jp.is_controller = false;
    }
    g_joyports.clear();
}

static void rescan_joyports()
{
    close_joyports();

    // Prefer the GameController API for consistent mappings (Xbox/PS/etc).
    const int count = SDL_NumJoysticks();
    for (int i = 0; i < count && (int)g_joyports.size() < kMaxJoyPorts; ++i)
    {
        JoyPort jp;
        if (SDL_IsGameController(i))
        {
            jp.controller = SDL_GameControllerOpen(i);
            if (jp.controller)
            {
                jp.is_controller = true;
                jp.joy = SDL_GameControllerGetJoystick(jp.controller);
                g_joyports.push_back(jp);
                continue;
            }
        }

        jp.joy = SDL_JoystickOpen(i);
        if (jp.joy)
        {
            jp.is_controller = false;
            g_joyports.push_back(jp);
        }
    }

    g_need_rescan_joy = false;
}

int PLATFORM_INPUT_install_joystick(void)
{
    ensure_sdl_events();
    if (SDL_InitSubSystem(SDL_INIT_JOYSTICK | SDL_INIT_GAMECONTROLLER | SDL_INIT_HAPTIC) != 0)
        return -1;

    SDL_JoystickEventState(SDL_ENABLE);
    SDL_GameControllerEventState(SDL_ENABLE);

    rescan_joyports();
    return 0;
}

void PLATFORM_INPUT_remove_joystick(void)
{
    close_joyports();
}

void PLATFORM_INPUT_poll_joysticks(void)
{
    ensure_sdl_events();
    pump_events_nonblocking();

    if (g_need_rescan_joy)
        rescan_joyports();

    SDL_GameControllerUpdate();
    SDL_JoystickUpdate();
}

int PLATFORM_INPUT_joystick_num_buttons(int port)
{
    if (port < 0 || port >= (int)g_joyports.size())
        return 0;
    const JoyPort &jp = g_joyports[port];
    if (jp.is_controller && jp.controller)
        return (int)SDL_CONTROLLER_BUTTON_MAX;
    if (!jp.joy)
        return 0;
    return SDL_JoystickNumButtons(jp.joy);
}

int PLATFORM_INPUT_joystick_button_state(int port, int button)
{
    if (port < 0 || port >= (int)g_joyports.size())
        return 0;
    const JoyPort &jp = g_joyports[port];

    if (jp.is_controller && jp.controller)
    {
        if (button < 0 || button >= (int)SDL_CONTROLLER_BUTTON_MAX)
            return 0;
        return SDL_GameControllerGetButton(jp.controller, (SDL_GameControllerButton)button) ? 1 : 0;
    }

    if (!jp.joy)
        return 0;
    if (button < 0 || button >= SDL_JoystickNumButtons(jp.joy))
        return 0;
    return SDL_JoystickGetButton(jp.joy, button) ? 1 : 0;
}

int PLATFORM_INPUT_joystick_num_sticks(int port)
{
    if (port < 0 || port >= (int)g_joyports.size())
        return 0;
    const JoyPort &jp = g_joyports[port];
    if (jp.is_controller && jp.controller)
        return 3; // left, right, triggers
    return 1; // raw SDL joystick is treated as one stick
}

int PLATFORM_INPUT_joystick_stick_num_axes(int port, int stick)
{
    if (port < 0 || port >= (int)g_joyports.size())
        return 0;
    const JoyPort &jp = g_joyports[port];
    if (jp.is_controller && jp.controller)
    {
        if (stick == 0 || stick == 1 || stick == 2)
            return 2;
        return 0;
    }

    if (!jp.joy)
        return 0;
    if (stick != 0)
        return 0;
    return SDL_JoystickNumAxes(jp.joy);
}

int PLATFORM_INPUT_joystick_stick_is_signed(int port, int stick)
{
    if (port < 0 || port >= (int)g_joyports.size())
        return 1;
    const JoyPort &jp = g_joyports[port];
    if (jp.is_controller && jp.controller)
    {
        // Triggers are exposed as unsigned 0..255.
        return (stick == 2) ? 0 : 1;
    }
    return 1;
}

int PLATFORM_INPUT_joystick_axis_pos(int port, int stick, int axis)
{
    if (port < 0 || port >= (int)g_joyports.size())
        return 0;
    const JoyPort &jp = g_joyports[port];
    if (jp.is_controller && jp.controller)
    {
        if (axis < 0 || axis > 1)
            return 0;
        if (stick == 0)
        {
            const Sint16 v = (axis == 0)
                ? SDL_GameControllerGetAxis(jp.controller, SDL_CONTROLLER_AXIS_LEFTX)
                : SDL_GameControllerGetAxis(jp.controller, SDL_CONTROLLER_AXIS_LEFTY);
            return (int)((v / 32767.0f) * 128.0f);
        }
        if (stick == 1)
        {
            const Sint16 v = (axis == 0)
                ? SDL_GameControllerGetAxis(jp.controller, SDL_CONTROLLER_AXIS_RIGHTX)
                : SDL_GameControllerGetAxis(jp.controller, SDL_CONTROLLER_AXIS_RIGHTY);
            return (int)((v / 32767.0f) * 128.0f);
        }
        if (stick == 2)
        {
            const Sint16 v = (axis == 0)
                ? SDL_GameControllerGetAxis(jp.controller, SDL_CONTROLLER_AXIS_TRIGGERLEFT)
                : SDL_GameControllerGetAxis(jp.controller, SDL_CONTROLLER_AXIS_TRIGGERRIGHT);
            return (int)((v / 32767.0f) * 255.0f);
        }
        return 0;
    }

    // Raw joystick
    if (!jp.joy)
        return 0;
    if (stick != 0)
        return 0;
    if (axis < 0 || axis >= SDL_JoystickNumAxes(jp.joy))
        return 0;

    const Sint16 v = SDL_JoystickGetAxis(jp.joy, axis);
    return (int)((v / 32767.0f) * 128.0f);
}
