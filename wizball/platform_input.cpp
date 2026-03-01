#include <allegro.h>
#include <SDL.h>

#include "platform_input.h"

static int platform_input_quit_requested = 0;
static unsigned int platform_input_debug_pump_count = 0;
static unsigned int platform_input_debug_event_count = 0;
static unsigned int platform_input_debug_window_event_count = 0;
static unsigned int platform_input_debug_keydown_event_count = 0;
static unsigned int platform_input_debug_quit_event_count = 0;

static SDL_Scancode PLATFORM_INPUT_map_allegro_scancode_to_sdl(int scancode)
{
	switch (scancode)
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

	case KEY_ESC: return SDL_SCANCODE_ESCAPE;
	case KEY_ENTER: return SDL_SCANCODE_RETURN;
	case KEY_SPACE: return SDL_SCANCODE_SPACE;
	case KEY_TAB: return SDL_SCANCODE_TAB;
	case KEY_BACKSPACE: return SDL_SCANCODE_BACKSPACE;

	case KEY_UP: return SDL_SCANCODE_UP;
	case KEY_DOWN: return SDL_SCANCODE_DOWN;
	case KEY_LEFT: return SDL_SCANCODE_LEFT;
	case KEY_RIGHT: return SDL_SCANCODE_RIGHT;
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

	case KEY_LSHIFT: return SDL_SCANCODE_LSHIFT;
	case KEY_RSHIFT: return SDL_SCANCODE_RSHIFT;
	case KEY_LCONTROL: return SDL_SCANCODE_LCTRL;
	case KEY_RCONTROL: return SDL_SCANCODE_RCTRL;
	case KEY_ALT: return SDL_SCANCODE_LALT;
	case KEY_ALTGR: return SDL_SCANCODE_RALT;

	default:
		return SDL_SCANCODE_UNKNOWN;
	}
}

void PLATFORM_INPUT_poll_keyboard(void)
{
	poll_keyboard();
}

int PLATFORM_INPUT_key_state(int scancode)
{
	if ((scancode < 0) || (scancode >= KEY_MAX))
	{
		return 0;
	}

	if (key[scancode] != 0)
	{
		return key[scancode];
	}

	// When SDL owns focus (visible SDL stub), Allegro key[] may not update.
	// Fall back to SDL keyboard state for mapped Allegro scancodes.
	if ((SDL_WasInit(SDL_INIT_VIDEO) & SDL_INIT_VIDEO) != 0)
	{
		const Uint8 *sdl_keys = SDL_GetKeyboardState(NULL);
		SDL_Scancode sdl_scan = PLATFORM_INPUT_map_allegro_scancode_to_sdl(scancode);
		if ((sdl_keys != NULL) && (sdl_scan != SDL_SCANCODE_UNKNOWN) && (sdl_keys[sdl_scan] != 0))
		{
			return 1;
		}
	}

	return 0;
}

int PLATFORM_INPUT_install_keyboard(void)
{
	return install_keyboard();
}

int PLATFORM_INPUT_readkey_ascii(void)
{
	return (readkey() & 0xFF);
}

void PLATFORM_INPUT_poll_mouse(void)
{
	poll_mouse();
}

int PLATFORM_INPUT_mouse_buttons_mask(void)
{
	return mouse_b;
}

int PLATFORM_INPUT_mouse_x(void)
{
	return mouse_x;
}

int PLATFORM_INPUT_mouse_y(void)
{
	return mouse_y;
}

int PLATFORM_INPUT_mouse_z(void)
{
	return mouse_z;
}

int PLATFORM_INPUT_install_mouse(void)
{
	return install_mouse();
}

int PLATFORM_INPUT_install_joystick(void)
{
	return install_joystick(JOY_TYPE_AUTODETECT);
}

int PLATFORM_INPUT_num_joysticks(void)
{
	return num_joysticks;
}

int PLATFORM_INPUT_joystick_num_buttons(int port)
{
	if ((port < 0) || (port >= num_joysticks))
	{
		return 0;
	}

	return joy[port].num_buttons;
}

int PLATFORM_INPUT_joystick_button_state(int port, int button)
{
	if ((port < 0) || (port >= num_joysticks))
	{
		return 0;
	}
	if ((button < 0) || (button >= joy[port].num_buttons))
	{
		return 0;
	}

	return joy[port].button[button].b;
}

int PLATFORM_INPUT_joystick_num_sticks(int port)
{
	if ((port < 0) || (port >= num_joysticks))
	{
		return 0;
	}

	return joy[port].num_sticks;
}

int PLATFORM_INPUT_joystick_stick_num_axes(int port, int stick)
{
	if ((port < 0) || (port >= num_joysticks))
	{
		return 0;
	}
	if ((stick < 0) || (stick >= joy[port].num_sticks))
	{
		return 0;
	}

	return joy[port].stick[stick].num_axis;
}

int PLATFORM_INPUT_joystick_axis_pos(int port, int stick, int axis)
{
	if ((port < 0) || (port >= num_joysticks))
	{
		return 0;
	}
	if ((stick < 0) || (stick >= joy[port].num_sticks))
	{
		return 0;
	}
	if ((axis < 0) || (axis >= joy[port].stick[stick].num_axis))
	{
		return 0;
	}

	return joy[port].stick[stick].axis[axis].pos;
}

int PLATFORM_INPUT_joystick_stick_is_signed(int port, int stick)
{
	if ((port < 0) || (port >= num_joysticks))
	{
		return 0;
	}
	if ((stick < 0) || (stick >= joy[port].num_sticks))
	{
		return 0;
	}

	return ((joy[port].stick[stick].flags & JOYFLAG_SIGNED) != 0);
}

int PLATFORM_INPUT_joystick_needs_calibration(int port)
{
	if ((port < 0) || (port >= num_joysticks))
	{
		return 0;
	}

	return ((joy[port].flags & JOYFLAG_CALIBRATE) != 0);
}

const char *PLATFORM_INPUT_calibrate_joystick_name(int port)
{
	if ((port < 0) || (port >= num_joysticks))
	{
		return NULL;
	}

	return calibrate_joystick_name(port);
}

int PLATFORM_INPUT_calibrate_joystick(int port)
{
	if ((port < 0) || (port >= num_joysticks))
	{
		return -1;
	}

	return calibrate_joystick(port);
}

void PLATFORM_INPUT_poll_joysticks(void)
{
	poll_joystick();
}

void PLATFORM_INPUT_pump_events(void)
{
	platform_input_debug_pump_count++;
	// Only pump SDL events once SDL owns a video context/window.
	// Running an SDL event loop alongside Allegro-owned windowing can interfere on some platforms.
	if ((SDL_WasInit(SDL_INIT_VIDEO) & SDL_INIT_VIDEO) == 0)
	{
		return;
	}

	SDL_Event event;

	while (SDL_PollEvent(&event))
	{
		platform_input_debug_event_count++;
		if (event.type == SDL_QUIT)
		{
			platform_input_quit_requested = 1;
			platform_input_debug_quit_event_count++;
		}
		else if (event.type == SDL_WINDOWEVENT)
		{
			platform_input_debug_window_event_count++;
			if (event.window.event == SDL_WINDOWEVENT_CLOSE)
			{
				platform_input_quit_requested = 1;
				platform_input_debug_quit_event_count++;
			}
		}
		else if (event.type == SDL_KEYDOWN)
		{
			platform_input_debug_keydown_event_count++;
		}
	}
}

int PLATFORM_INPUT_quit_requested(void)
{
	return platform_input_quit_requested;
}

void PLATFORM_INPUT_clear_quit_requested(void)
{
	platform_input_quit_requested = 0;
}

unsigned int PLATFORM_INPUT_debug_pump_count(void)
{
	return platform_input_debug_pump_count;
}

unsigned int PLATFORM_INPUT_debug_event_count(void)
{
	return platform_input_debug_event_count;
}

unsigned int PLATFORM_INPUT_debug_window_event_count(void)
{
	return platform_input_debug_window_event_count;
}

unsigned int PLATFORM_INPUT_debug_keydown_event_count(void)
{
	return platform_input_debug_keydown_event_count;
}

unsigned int PLATFORM_INPUT_debug_quit_event_count(void)
{
	return platform_input_debug_quit_event_count;
}
