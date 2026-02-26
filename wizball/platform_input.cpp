#include <allegro.h>
#ifdef WIZBALL_USE_SDL2
#include <SDL.h>
#endif

#include "platform_input.h"

static int platform_input_quit_requested = 0;

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

	return key[scancode];
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
#ifdef WIZBALL_USE_SDL2
	if ((SDL_WasInit(0) & (SDL_INIT_VIDEO | SDL_INIT_EVENTS)) == 0)
	{
		return;
	}

	SDL_Event event;

	while (SDL_PollEvent(&event))
	{
		if (event.type == SDL_QUIT)
		{
			platform_input_quit_requested = 1;
		}
	}
#endif
}

int PLATFORM_INPUT_quit_requested(void)
{
	return platform_input_quit_requested;
}

void PLATFORM_INPUT_clear_quit_requested(void)
{
	platform_input_quit_requested = 0;
}
