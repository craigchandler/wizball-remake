#ifndef _SIMPLE_GUI_H_
#define _SIMPLE_GUI_H_

#define MAX_GUI_BUTTONS						(128)

#define UNSET								(-1)

#define BUTTON_TYPE_TOGGLE					(0)
#define BUTTON_TYPE_RADIO_SET				(1)
#define BUTTON_TYPE_H_RAMP					(2)
#define BUTTON_TYPE_V_RAMP					(3)
#define BUTTON_TYPE_NUDGE_VALUE				(4)
#define BUTTON_TYPE_						(5)
#define BUTTON_TYPE_H_DRAG_BAR				(6)
#define BUTTON_TYPE_V_DRAG_BAR				(7)
#define BUTTON_TYPE_BINARY_TOGGLE			(8)
#define BUTTON_TYPE_SET						(9)

#define BUTTON_OFF							(0)
#define BUTTON_ON							(1)

#define BUTTON_DEAD							(0)
#define BUTTON_ALIVE						(1)
#define BUTTON_HIDDEN						(2)
#define BUTTON_SLEEPING						(3)

#define DEFAULT_MOUSE_REPEAT_DELAY			(10)
#define DEFAULT_MOUSE_REPEAT_FREQENCY		(2)



void SIMPGUI_create_toggle_button (int group_id, int button_id, int *integer_value, int x, int y, int bitmap_number, int sprite_number, int mouse_button);
void SIMPGUI_create_radio_set_button (int group_id, int button_id, int *integer_value, int x, int y, int bitmap_number, int sprite_number, int mouse_button, int set_value);
void SIMPGUI_create_set_button (int group_id, int button_id, int *integer_value, int x, int y, int bitmap_number, int sprite_number, int mouse_button, int set_value);
void SIMPGUI_create_h_ramp_button (int group_id, int button_id, int *integer_value, int x, int y, int width, int height , int start, int end, int multiple, int mouse_button , bool rounded_up , bool draw_bars);
void SIMPGUI_create_v_ramp_button (int group_id, int button_id, int *integer_value, int x, int y, int width, int height , int start, int end, int multiple, int mouse_button , bool rounded_up , bool draw_bars);
void SIMPGUI_create_nudge_button (int group_id, int button_id, int *integer_value, int x, int y, int bitmap_number, int sprite_number, int mouse_button, int start, int end, int nudge_by, bool wrap);
void SIMPGUI_create_h_drag_bar (int group_id, int button_id, int *integer_value, int x, int y, int width, int height , int start, int end, int drag_value, int drag_bar_size, int mouse_button);
void SIMPGUI_create_v_drag_bar (int group_id, int button_id, int *integer_value, int x, int y, int width, int height , int start, int end, int drag_value, int drag_bar_size, int mouse_button);
void SIMPGUI_create_binary_toggle_button (int group_id, int button_id, int *integer_value, int x, int y, int bitmap_number, int sprite_number, int mouse_button, int xor_value);

void SIMPGUI_sleep_group (int group_id);
void SIMPGUI_wake_group (int group_id);
void SIMPGUI_hide_group (int group_id);
void SIMPGUI_unhide_group (int group_id);
void SIMPGUI_kill_group (int group_id);
void SIMPGUI_kill_button (int group_id, int button_id);

void SIMPGUI_setup (void);

void SIMPGUI_check_all_buttons (void);
void SIMPGUI_draw_all_buttons (int group_number=UNSET, bool inverse_selection = false);

void SIMPGUI_set_overlay_group (int group_id , int sprite_number);

bool SIMPGUI_get_click_status (int group_id, int button_id);

void SIMPGUI_set_h_flip_group (int group_id, bool on_or_off);

bool SIMPGUI_does_button_exist (int group_id, int button_id);

#endif
