#ifndef _PLATFORM_INPUT_H_
#define _PLATFORM_INPUT_H_

void PLATFORM_INPUT_poll_keyboard(void);
int PLATFORM_INPUT_key_state(int scancode);

void PLATFORM_INPUT_poll_mouse(void);
int PLATFORM_INPUT_mouse_buttons_mask(void);
int PLATFORM_INPUT_mouse_x(void);
int PLATFORM_INPUT_mouse_y(void);
int PLATFORM_INPUT_mouse_z(void);

#endif
