#include "string_size_constants.h"
#include <stddef.h>

#ifndef _CONTROL_H_
#define _CONTROL_H_

// Control Functions



// ---------------- DEFINING ROUTINES ----------------
void CONTROL_define_control_from_config (char *pointer);
char * CONTROL_get_control_description (int player_number, int control_number);
bool CONTROL_is_player_button_undefined (int player, int button);
// ---------------- KEYBOARD INPUT AND OUTPUT ROUTINES ----------------
bool CONTROL_key_down (int scancode);
bool CONTROL_key_hit (int scancode);
bool CONTROL_key_release (int scancode);
bool CONTROL_key_repeat (int scancode, int initial_delay=10, int repeat_delay=5, bool ignore_hit=false);
bool CONTROL_key_double_click (int scancode, int click_speed);
float CONTROL_key_power (int scancode);
// ---------------- PAD INPUT AND OUTPUT ROUTINES ----------------
bool CONTROL_joy_down (int port, int button);
bool CONTROL_joy_hit (int port, int button);
bool CONTROL_joy_release (int port, int button);
bool CONTROL_joy_repeat (int port, int button, int initial_delay=10, int repeat_delay=5, bool ignore_hit=false);
bool CONTROL_joy_double_click (int port, int button, int click_speed);
float CONTROL_joy_power (int port, int button);
// ---------------- STICK INPUT AND OUTPUT ROUTINES ----------------
bool CONTROL_stick_down (int port, int stick, int axis, int side);
bool CONTROL_stick_hit (int port, int stick, int axis, int side);
bool CONTROL_stick_release (int port, int stick, int axis, int side);
bool CONTROL_stick_repeat (int port, int stick, int axis, int side, int initial_delay=10, int repeat_delay=5, bool ignore_hit=false);
bool CONTROL_stick_double_click (int port, int stick, int axis, int side, int click_speed);
float CONTROL_stick_power (int port, int stick, int axis, int side);
// ---------------- MOUSE INPUT AND OUTPUT ROUTINES ----------------
void CONTROL_position_mouse (int x, int y);
void CONTROL_lock_mouse_button (int button);
bool CONTROL_mouse_down (int button);
bool CONTROL_mouse_hit (int button);
bool CONTROL_mouse_release (int button);
bool CONTROL_mouse_repeat (int button , int initial_delay=10, int repeat_delay=5, bool ignore_hit=false);
bool CONTROL_mouse_double_click (int button, int click_speed);
int CONTROL_mouse_speed (int button);
int CONTROL_mouse_power (int button);
int CONTROL_mouse_x (void);
int CONTROL_mouse_y (void);
int CONTROL_mouse_z (void);
// ---------------- PLAYER INPUT AND OUTPUT ROUTINES ----------------
void CONTROL_lock_player_button (int player, int button);
bool CONTROL_player_button_hit (int player, int button);
bool CONTROL_player_button_down (int player, int button);
bool CONTROL_player_button_release (int player, int button);
int CONTROL_player_button_power (int player, int button);
bool CONTROL_player_button_repeat (int player, int button, int initial_delay=10, int repeat_delay=5, bool ignore_hit=false);
bool CONTROL_player_button_double_click (int player, int button, int click_speed);

int CONTROL_read_back_player_control_device (int player, int control);
int CONTROL_read_back_player_control_port (int player, int control);
int CONTROL_read_back_player_control_button (int player, int control);
int CONTROL_read_back_player_control_stick (int player, int control);
int CONTROL_read_back_player_control_axis (int player, int control);
// ---------------- SETUP ROUTINES ----------------
void CONTROL_setup_everything(void);
// ---------------- DEVICE AVAILABILITY ----------------
bool CONTROL_is_keyboard_available(void);
bool CONTROL_is_joystick_available(void);
bool CONTROL_is_mouse_available(void);
// ---------------- UPDATE ----------------
void CONTROL_update_all_input (void);
// ---------------- MISC FUNCTIONALITY ----------------
int CONTROL_get_word (char *word , int status , unsigned int max_length , bool capitalise, bool no_spaces=false);
bool CONTROL_grab_start (int button);
int CONTROL_grab_offset_x (void);
int CONTROL_grab_offset_y (void);
void CONTROL_set_stick_sensitivity (int sensitivity);
int CONTROL_get_stick_sensitivity (void);
void CONTROL_set_mouse_sensitivity (int sensitivity);
int CONTROL_get_mouse_sensitivity (void);
void CONTROL_set_mouse_smoothing (int mouse_button, int sampling_rate);
void CONTROL_update_sequences (void);
void CONTROL_constrain_mouse (int x, int y, int distance);
bool CONTROL_get_keypress (int player, int control, char *description, size_t description_size, bool check_previous_for_match);
void CONTROL_define_control_keyboard (int player_number, int control_number, int input_key);
void CONTROL_get_input (int *device_type, int *device_button);
void CONTROL_set_dupe_check_value (int player_number, int control_number, bool boolean_value);
// ---------------- INPUT RECORDING AND PLAYBACK ----------------
void CONTROL_start_recording_input_next_frame (int player_number);
void CONTROL_start_playback_input_next_frame (int player_number);
void CONTROL_stop_playback (int player_number);
void CONTROL_save_recorded_input (int player_number, char *filename_pointer);
void CONTROL_load_recorded_input (int player_number, char *filename_pointer);
void CONTROL_load_compressed_recorded_input_and_inflate (int player_number, char *filename_pointer);
void CONTROL_stop_recording_input (int player_number);
int CONTROL_get_recording_playback_status (int player_number);
void CONTROL_stop_and_save_active_channels (char *filename_prefix);
void CONTROL_dump_realtime_compressed_button_data (int player_number);

int CONTROL_get_first_keyboard_hit (void);
int CONTROL_get_ascii_for_scancode (int scancode, bool uppercase=false);
int CONTROL_get_scancode_for_ascii (int ascii);

bool CONTROL_check_any_joypad_control_hit (int player_number);

void CONTROL_write_variable_data_to_recorded_input (int player_number, int value_id, int value);
int CONTROL_read_variable_data_from_recorded_input (int player_number, int value_id);



// Control Defines

#define CURRENT						(0)
#define OLD							(1)
#define REPEAT						(2)
#define DIFF						(2)
#define LOCKED_BUTTON				(3)

#define CONTROL_UP						(0)
#define CONTROL_DOWN					(1)
#define CONTROL_LEFT					(2)
#define CONTROL_RIGHT					(3)
#define CONTROL_JUMP					(4)
#define CONTROL_FIRE_1					(5)
#define CONTROL_FIRE_2					(6)
#define CONTROL_FIRE_3					(7)
#define CONTROL_FIRE_4					(8)
#define CONTROL_PICK_UP					(9)
#define CONTROL_DROP					(10)
#define CONTROL_CYCLE_INVENTORY_LEFT	(11)
#define CONTROL_CYCLE_INVENTORY_RIGHT	(12)
#define CONTROL_MENU_1					(13)
#define CONTROL_MENU_2					(14)
#define CONTROL_PAUSE					(15)
// Put more here!
#define CONTROL_DEBUG_1					(22)
#define CONTROL_DEBUG_2					(23)
#define CONTROL_DEBUG_3					(24)
#define CONTROL_DEBUG_4					(25)
#define CONTROL_DEBUG_5					(26)
#define CONTROL_DEBUG_6					(27)
#define CONTROL_DEBUG_7					(28)
#define CONTROL_DEBUG_8					(29)
#define CONTROL_DEBUG_9					(30)
#define CONTROL_DEBUG_10				(31)

#define NUM_BUTTONS					(32) // Number of different buttons per set of controls (including directions).

#define DEVICE_KEYBOARD				(0)
#define DEVICE_JOYPAD				(1)
#define DEVICE_JOYSTICK_POS			(2)
#define DEVICE_JOYSTICK_NEG			(3)
#define DEVICE_MOUSE_BUTTONS		(4)

#define MAX_PLAYERS					(8)

#define NUM_PORTS					(4) // Number of different joypad/joystick ports to poll.

#define KEY_REPEAT_SPEED			(6)
#define MOUSE_REPEAT_SPEED			(6)

#define GET_NEW_WORD				(4)
#define GET_WORD					(3)
#define GETTING_WORD				(2)
#define GOTTEN_WORD					(1)
#define DO_NOTHING					(0)

#define LMB							(0)
#define RMB							(1)
#define MMB							(2)

#define X_POS						(0)
#define Y_POS						(1)
#define Z_POS						(2)

#define MAX_KEY_NAME_LENGTH			(32)


#define KEY_ANY					0
#ifndef KEY_A
#define KEY_A					1
#define KEY_B					2
#define KEY_C					3
#define KEY_D					4
#define KEY_E					5
#define KEY_F					6
#define KEY_G					7
#define KEY_H					8
#define KEY_I					9
#define KEY_J					10
#define KEY_K					11
#define KEY_L					12
#define KEY_M					13
#define KEY_N					14
#define KEY_O					15
#define KEY_P					16
#define KEY_Q					17
#define KEY_R					18
#define KEY_S					19
#define KEY_T					20
#define KEY_U					21
#define KEY_V					22
#define KEY_W					23
#define KEY_X					24
#define KEY_Y					25
#define KEY_Z					26
#define KEY_0					27
#define KEY_1					28
#define KEY_2					29
#define KEY_3					30
#define KEY_4					31
#define KEY_5					32
#define KEY_6					33
#define KEY_7					34
#define KEY_8					35
#define KEY_9					36
#define KEY_0_PAD				37
#define KEY_1_PAD				38
#define KEY_2_PAD				39
#define KEY_3_PAD				40
#define KEY_4_PAD				41
#define KEY_5_PAD				42
#define KEY_6_PAD				43
#define KEY_7_PAD				44
#define KEY_8_PAD				45
#define KEY_9_PAD				46
#define KEY_F1					47
#define KEY_F2					48
#define KEY_F3					49
#define KEY_F4					50
#define KEY_F5					51
#define KEY_F6					52
#define KEY_F7					53
#define KEY_F8					54
#define KEY_F9					55
#define KEY_F10					56
#define KEY_F11					57
#define KEY_F12					58
#define KEY_ESC					59
#define KEY_TILDE				60
#define KEY_MINUS				61
#define KEY_EQUALS				62
#define KEY_BACKSPACE			63
#define KEY_TAB					64
#define KEY_OPENBRACE			65
#define KEY_CLOSEBRACE			66
#define KEY_ENTER				67
#define KEY_COLON				68
#define KEY_QUOTE				69
#define KEY_BACKSLASH			70
#define KEY_BACKSLASH2			71
#define KEY_COMMA				72
#define KEY_STOP				73
#define KEY_SLASH				74
#define KEY_SPACE				75
#define KEY_INSERT				76
#define KEY_DEL					77
#define KEY_HOME				78
#define KEY_END					79
#define KEY_PGUP				80
#define KEY_PGDN				81
#define KEY_LEFT				82
#define KEY_RIGHT				83
#define KEY_UP					84
#define KEY_DOWN				85
#define KEY_SLASH_PAD			86
#define KEY_ASTERISK			87
#define KEY_MINUS_PAD			88
#define KEY_PLUS_PAD			89
#define KEY_DEL_PAD				90
#define KEY_ENTER_PAD			91
#define KEY_PRTSCR				92
#define KEY_PAUSE				93
#define KEY_ABNT_C1				94
#define KEY_YEN					95
#define KEY_KANA				96
#define KEY_CONVERT				97
#define KEY_NOCONVERT			98
#define KEY_AT					99
#define KEY_CIRCUMFLEX			100
#define KEY_COLON2				101
#define KEY_KANJI				102

#define KEY_MODIFIERS			103

#define KEY_LSHIFT				103
#define KEY_RSHIFT				104
#define KEY_LCONTROL			105
#define KEY_RCONTROL			106
#define KEY_ALT					107
#define KEY_ALTGR				108
#define KEY_LWIN				109
#define KEY_RWIN				110
#define KEY_MENU				111
#define KEY_SCRLOCK				112
#define KEY_NUMLOCK				113
#define KEY_CAPSLOCK			114
#endif 


#define BOOL_CONTROL_HIT				(1)
#define BOOL_CONTROL_DOWN				(2)
#define BOOL_CONTROL_REPEAT				(4)
#define BOOL_CONTROL_DOUBLE_CLICK		(8)
#define BOOL_CONTROL_RELEASE			(16)
// These are the booleans used to save player input

#define STATUS_NOTHING					(0)
#define STATUS_RECORDING				(1)
#define STATUS_PLAYBACK					(2)



// Control Structures

typedef struct {	
	int button_pressed; // Which button for the player had been pushed (ie, CONTROL_UP to CONTROL_FIRE_2).
	int hold_length; // How long the button must be held down (in frames) for this part to register as complete.
	int press_next_within; // How long they have to press the next button in the sequence before the sequence resets. Unless it's the last in the sequence.
} button_sequence_entry;

typedef struct {
	button_sequence_entry *button_sequence_list_pointer; // The list of instructions in the sequence.
	int sequence_size; // How long is the list?
	int players_progress[MAX_PLAYERS]; // Which segment each player is in.
	bool players_active[MAX_PLAYERS]; // Which player controls to check this against to avoid extra work.
	bool players_complete[MAX_PLAYERS]; // Has a sequence just been completed?
} button_sequence;

typedef struct {
	int key_hit;
	int key_down;
	int key_repeat;
	int this_hit;
	int last_hit;
	bool double_click;
} input;

typedef struct {
	int current_state; // Stores either key state, joypad state.
	int old_state; // The previous frame state as above.
	int current_position; // Joystick position or mouse position.
	int old_position; // The previous frame position as above.
	int analogue_offset; // How far past the deadzone the joystick is as a percentage from 0-100.
	int this_hit; // The last time it was keyhit.
	int prev_hit; // The previous time to that.
	int held_time; // How long the button has been held down.
	bool locked; // Is it locked? If so then always reports as false until released.
} control_record;

typedef struct {
	int device;
	int button;
	int port;
	int stick;
	int axis;
	int description_tag_number; // This the number of the line within the text file structure with a suitable name for the input selected.
	bool dupe_check;
} buttons;

typedef struct {
	int default_position;
	int upper_position;
	int lower_position;
} joy_defaults;



typedef struct
{
	unsigned char boolean[NUM_BUTTONS];
	char power[NUM_BUTTONS]; // analogue value from -100 to 100, I think...
} individual_player_input;


/*
typedef struct
{
	int total_bool_states;
	unsigned char *new_bool_state_value;
	int *new_bool_state_length;

	int total_char_states;
	char *new_char_state_value;
	int *new_char_state_length;
} compressed_button_input;
*/


typedef struct
{
	int allocated_bool_states;
	int current_bool_state;
	unsigned char *new_bool_state_value;
	int *new_bool_state_length;

	int allocated_char_states;
	int current_char_state;
	char *new_char_state_value;
	int *new_char_state_length;
} realtime_compressed_button_input;



typedef struct
{
	// How many frames did we record, eh? EH?
	int frames_recorded;

	// This is where the currently recording frames are stored,
	// or the currently playing back frames are pushed to.
	individual_player_input current_frame_data;

	// These store which section of recorded data we're reading from and how far we're through it.
	int current_bool_section_number[NUM_BUTTONS];
	int current_bool_section_position[NUM_BUTTONS];
	int current_char_section_number[NUM_BUTTONS];
	int current_char_section_position[NUM_BUTTONS];

	// This is where the recorded data lives.
	realtime_compressed_button_input realtime_compressed_button_data[NUM_BUTTONS];

	// These are where the extra bits of data live.
	int written_variable_count;
	int *written_variable_data; // This is basically alternating values of id and value.

	int written_array_data_size;
	int *written_array_data; // This is just a big wobbly set of values (array id, width, height, depth, data) in a row.

	int written_flag_count;
	int *written_flag_data; // This is like the variable data, only alternating flag number and value instead.

	// Are we recording or playing back or getting ready to do one of those? Eh? EH?
	bool recording;
	bool recorded_but_not_saved;
	bool playback;
	bool loaded_but_not_played_back;
	bool start_recording_this_frame;
	bool start_playback_this_frame;

	// And how far are we through playing back? Hmm? HMM?
	int currently_playing;

//	compressed_button_input compressed_button_data[NUM_BUTTONS];
//	int frames_allocated;
//	individual_player_input *data;
} recorded_player_input;



/*
typedef struct
{
	unsigned char boolean;
	char power; // analogue value from -100 to 100, I think...
	int number_of_frames; // How many frames this data is valid for.
} compressed_player_button_input;

typedef struct
{
	compressed_player_button_input *data[NUM_BUTTONS];
	int data_sizes[NUM_BUTTONS];
} compressed_player_input;
*/


// Externed Arrays

extern input player_input [MAX_PLAYERS][NUM_BUTTONS];

// End

#endif
