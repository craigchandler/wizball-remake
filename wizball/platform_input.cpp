#include <allegro.h>

#include "platform_input.h"

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
