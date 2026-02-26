#ifndef _PLATFORM_INPUT_H_
#define _PLATFORM_INPUT_H_

void PLATFORM_INPUT_poll_keyboard(void);
int PLATFORM_INPUT_key_state(int scancode);
int PLATFORM_INPUT_install_keyboard(void);
int PLATFORM_INPUT_readkey_ascii(void);

void PLATFORM_INPUT_poll_mouse(void);
int PLATFORM_INPUT_mouse_buttons_mask(void);
int PLATFORM_INPUT_mouse_x(void);
int PLATFORM_INPUT_mouse_y(void);
int PLATFORM_INPUT_mouse_z(void);
int PLATFORM_INPUT_install_mouse(void);

int PLATFORM_INPUT_install_joystick(void);
int PLATFORM_INPUT_num_joysticks(void);
int PLATFORM_INPUT_joystick_num_buttons(int port);
int PLATFORM_INPUT_joystick_button_state(int port, int button);
int PLATFORM_INPUT_joystick_num_sticks(int port);
int PLATFORM_INPUT_joystick_stick_num_axes(int port, int stick);
int PLATFORM_INPUT_joystick_axis_pos(int port, int stick, int axis);
int PLATFORM_INPUT_joystick_stick_is_signed(int port, int stick);
int PLATFORM_INPUT_joystick_needs_calibration(int port);
const char *PLATFORM_INPUT_calibrate_joystick_name(int port);
int PLATFORM_INPUT_calibrate_joystick(int port);
void PLATFORM_INPUT_poll_joysticks(void);

void PLATFORM_INPUT_pump_events(void);
int PLATFORM_INPUT_quit_requested(void);
void PLATFORM_INPUT_clear_quit_requested(void);

#endif
