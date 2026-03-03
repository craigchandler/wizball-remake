#ifndef _PLATFORM_INPUT_H_
#define _PLATFORM_INPUT_H_

#include "platform_scancodes.h"

// SDL is the only backend for this input layer.
#include <SDL.h>

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

// Call once per frame (or once per fixed-step tick) to keep SDL input state fresh.
// This wraps PLATFORM_INPUT_pump_events() and any per-frame housekeeping.
// Safe to call multiple times.
void PLATFORM_INPUT_begin_frame(void);
int PLATFORM_INPUT_quit_requested(void);
void PLATFORM_INPUT_clear_quit_requested(void);

unsigned int PLATFORM_INPUT_debug_pump_count(void);
unsigned int PLATFORM_INPUT_debug_event_count(void);
unsigned int PLATFORM_INPUT_debug_window_event_count(void);
unsigned int PLATFORM_INPUT_debug_keydown_event_count(void);
unsigned int PLATFORM_INPUT_debug_quit_event_count(void);

// Optional helper used by legacy code to reposition the cursor.
// Uses the current mouse-focus window.
void PLATFORM_INPUT_warp_mouse(int x, int y);

#endif
