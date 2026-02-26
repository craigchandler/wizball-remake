#include <allegro.h>
#include <stdio.h>
#include <math.h>
#include <string.h>
#include <assert.h>
#include <allegro.h>
#ifdef ALLEGRO_MACOSX
#include <CoreServices/CoreServices.h>
#endif

#include "control.h" // Because - DUH!
#include "math_stuff.h"
#include "output.h"
#include "global_param_list.h"
#include "string_stuff.h"
#include "textfiles.h"
#include "main.h"
#include "file_stuff.h"
#include "platform_input.h"

//#include "fortify.h"

int frame_counter = 0;
	// This updates each frame so you can see how close 2 clicks were in order
	// to detect double-clicks.

#define MAX_KEYS				(115)

control_record key_store [MAX_KEYS];

int stored_mouse_x;
int stored_mouse_y;

#define MAX_MOUSE_BUFFER_SIZE		(60)
#define MOUSE_BUTTONS				(3)
#define MOUSE_AXIS					(3)

int mouse_sampling_size[MOUSE_AXIS]; // How many frames worth of data we look at when giving a mouse speed value, each axis has it's own value so the Z speed ain't all drunken.
int mouse_buffer[MOUSE_AXIS][MAX_MOUSE_BUFFER_SIZE];
int max_mouse_speed = 64;
// So that the mouse can emulate an analogue stick a maximum offset per frame must be
// specified so a percentage value can be figured out from it.

bool joystick_setup_okay = false;
bool mouse_setup_okay = false;
bool keyboard_setup_okay = false;

control_record mouse_button_store[MOUSE_BUTTONS];
control_record mouse_position_store[MOUSE_AXIS];

char key_name [MAX_KEYS][MAX_KEY_NAME_LENGTH] =
{ "N/A" , "a" , "b" , "c" , "d" , "e" , "f" , "g" , "h" , "i" , 
  "j" , "k" , "l" , "m" , "n" , "o" , "p" , "q" , "r" , "s" , 
  "t" , "u" , "v" , "w" , "x" , "y" , "z" , "0" , "1" , "2" , 
  "3" , "4" , "5" , "6" , "7" , "8" , "9" , "Keypad Insert" , "Keypad End" , "Keypad Down" , 
  "Keypad Page Down" , "Keypad Left" , "Keypad N/A" , "Keypad Right" , "Keypad Home" , "Keypad Uo" , "Keypad Page Up" , "F1" , "F2" , "F3" , 
  "F4" , "F5" , "F6" , "F7" , "F8" , "F9" , "F10" , "F11" , "F12" , "Escape" , "¬" , 
  "-" , "=" , "Backspace" , "Tab" , "[" , "]" , "Enter" , ";" , "\'" , "\\" , 
  "\\" , "," , "." , "/" , "Space" , "Insert" , "Delete" , "Home" , "End" , "Page Up" , 
  "Page" , "Cursor Left" , "Cursor Right" , "Cursor Up" , "Cursor Down" , "Keypad /" , "Keypad *" , "Keypad -" , "Keypad +" , "Keypad Delete" , 
  "Keypad Enter" , "Print Screen" , "Pause" , "ABNT C1" , "Yen" , "Kana" , "Convert" , "No Convert" , "At" , "Circumflex" , 
  ";" , "Kanji" , "Left Shift" , "Rigth Shift" , "Left Control" , "Right Control" , "Alt" , "Alt GR" , "Left Windows Key" , "Right Windows Key" , 
  "Menu" , "Scroll Lock" , "Num Lock" , "Caps Lock" };
// The names of all the keys on the keyboard arranged in scancode order. Obviously the user can
// UPPER or LOWER them after reading them back.

char capitalised_character_array[MAX_KEYS] =
{ '\0' , 'A' , 'B' , 'C' , 'D' , 'E' , 'F' , 'G' , 'H' , 'I' ,
  'J' , 'K' , 'L' , 'M' , 'N' , 'O' , 'P' , 'Q' , 'R' , 'S' ,
  'T' , 'U' , 'V' , 'W' , 'X' , 'Y' , 'Z' , '0' , '1' , '2' ,
  '3' , '4' , '5' , '6' , '7' , '8' , '9' , '0' , '1' , '2' ,
  '3' , '4' , '5' , '6' , '7' , '8' , '9' , '\0' , '\0' , '\0' ,
  '\0' , '\0' , '\0' , '\0' , '\0' , '\0' , '\0' , '\0' , '\0' , '\0' ,
  '`' , '-' , '=' , '\0' , '\0' , '[' , ']' , '\0' , ';' , '\'' ,
  '\\' , '\\' , ',' , '.' , '/' , ' ' , '\0' , '\0' , '\0' , '\0' ,
  '\0' , '\0' , '\0' , '\0' , '\0' , '\0' , '/' , '*' , '-' , '+' ,
  '\0' , '\0' , '\0' , '\0' , '\0' , '\0' , '\0' , '\0' , '\0' , '\0' ,
  '\0' , '\0' , '\0' , '\0' , '\0' , '\0' , '\0' , '\0' , '\0' , '\0' ,
  '\0' , '\0' , '\0' , '\0' , '\0' };
// These are capitalised letters as if CAPSLOCK was set to on.

char lower_character_array[MAX_KEYS] =
{ '\0' , 'a' , 'b' , 'c' , 'd' , 'e' , 'f' , 'g' , 'h' , 'i' ,
  'j' , 'k' , 'l' , 'm' , 'n' , 'o' , 'p' , 'q' , 'r' , 's' ,
  't' , 'u' , 'v' , 'w' , 'x' , 'y' , 'z' , '0' , '1' , '2' ,
  '3' , '4' , '5' , '6' , '7' , '8' , '9' , '0' , '1' , '2' ,
  '3' , '4' , '5' , '6' , '7' , '8' , '9' , '\0' , '\0' , '\0' ,
  '\0' , '\0' , '\0' , '\0' , '\0' , '\0' , '\0' , '\0' , '\0' , '\0' ,
  '`' , '-' , '=' , '\0' , '\0' , '[' , ']' , '\0' , ';' , '\'' ,
  '\\' , '\\' , ',' , '.' , '/' , ' ' , '\0' , '\0' , '\0' , '\0' ,
  '\0' , '\0' , '\0' , '\0' , '\0' , '\0' , '/' , '*' , '-' , '+' ,
  '\0' , '\0' , '\0' , '\0' , '\0' , '\0' , '\0' , '\0' , '\0' , '\0' ,
  '\0' , '\0' , '\0' , '\0' , '\0' , '\0' , '\0' , '\0' , '\0' , '\0' ,
  '\0' , '\0' , '\0' , '\0' , '\0' };
// Lower cased characters. Duh!

char upper_character_array[MAX_KEYS] = 
{ '\0' , 'A' , 'B' , 'C' , 'D' , 'E' , 'F' , 'G' , 'H' , 'I' ,
  'J' , 'K' , 'L' , 'M' , 'N' , 'O' , 'P' , 'Q' , 'R' , 'S' ,
  'T' , 'U' , 'V' , 'W' , 'X' , 'Y' , 'Z' , ')' , '!' , '\"' ,
  '£' , '$' , '%' , '^' , '&' , '*' , '(' , '0' , '1' , '2' ,
  '3' , '4' , '5' , '6' , '7' , '8' , '9' , '\0' , '\0' , '\0' ,
  '\0' , '\0' , '\0' , '\0' , '\0' , '\0' , '\0' , '\0' , '\0' , '\0' ,
  '¬' , '_' , '+' , '\0' , '\0' , '{' , '}' , '\0' , ':' , '@' ,
  '|' , '|' , '<' , '>' , '\?' , ' ' , '\0' , '\0' , '\0' , '\0' ,
  '\0' , '\0' , '\0' , '\0' , '\0' , '\0' , '/' , '*' , '-' , '+' ,
  '\0' , '\0' , '\0' , '\0' , '\0' , '\0' , '\0' , '\0' , '\0' , '\0' ,
  '\0' , '\0' , '\0' , '\0' , '\0' , '\0' , '\0' , '\0' , '\0' , '\0' ,
  '\0' , '\0' , '\0' , '\0' , '\0' };
// Upper cased characters as if SHIFT was held down.

int scancodes_in_ascii_order_main[256] = 
{
	UNSET , UNSET , UNSET , UNSET , UNSET , UNSET , UNSET , UNSET ,
	UNSET , UNSET , UNSET , UNSET , UNSET , UNSET , UNSET , UNSET , 
	UNSET , UNSET , UNSET , UNSET , UNSET , UNSET , UNSET , UNSET , 
	UNSET , UNSET , UNSET , UNSET , UNSET , UNSET , UNSET , UNSET ,
	KEY_SPACE
};
// This contains the key that needs to be pushed in order to get
// the ascii code listed.

int scancodes_in_ascii_order_modifier[256] = 
{
	UNSET , UNSET , UNSET , UNSET , UNSET , UNSET , UNSET , UNSET ,
	UNSET , UNSET , UNSET , UNSET , UNSET , UNSET , UNSET , UNSET , 
	UNSET , UNSET , UNSET , UNSET , UNSET , UNSET , UNSET , UNSET , 
	UNSET , UNSET , UNSET , UNSET , UNSET , UNSET , UNSET , UNSET ,
	KEY_SPACE
};
// This contains the needed modifiers in order to get the ascii code
// listed.

input player_input [MAX_PLAYERS][NUM_BUTTONS];
// Where the polled input data is stored.

bool locked_player_input [MAX_PLAYERS][NUM_BUTTONS];
// Allows you to lock the input of a player's controls until released. Primarily for
// when selected an option from a menu and you don't want the selection button to carry
// through to the game proper.

buttons controls [MAX_PLAYERS][NUM_BUTTONS];
// Where the control defaults go.
// 0 = number of keyboard or pad button input.
// 1 = input type (0 = keyboard, 1 = pad button, 2 = pad axis y -ve, 3 = pad axis y +ve, 4 = pad axis x -ve, 5 = pad axis x +ve, 6 = pad axis z -ve, 7 = pad axis z +ve, 8 = pad roll -ve, 9 = pad roll +ve).

#define MAX_PORTS			(8)
#define MAX_STICKS			(4)
#define MAX_AXIS			(4)
#define MAX_BUTTONS			(32)
#define MAX_STATES			(2)

#define AXIS_NEG_STATE		(0)
#define AXIS_POS_STATE		(1)
#define	MAX_AXIS_STATES		(2)

control_record pad_store [MAX_PORTS][MAX_BUTTONS];
control_record stick_store [MAX_PORTS][MAX_STICKS][MAX_AXIS][MAX_STATES];

joy_defaults default_axis_value [MAX_PORTS][MAX_STICKS][MAX_AXIS];

int deadzone = 64;
// This defines the sensitivity of analogue devices, values of less than 8-16 are likely to cause false-positives.

recorded_player_input rpi[MAX_PLAYERS];

#define RECORDING_ALLOCATION_CHUNKS		(3600)
#define COMPRESSED_RECORDING_ALLOCATION_CHUNKS		(8192)


int MATH_round_to_nearest (int value, int step)
{
	int sign = MATH_sgn_int (value);
	int temp_value = value / step;
	int low_value = temp_value * step * sign;
	int high_value = (temp_value+1) * step * sign;

	if (abs (low_value - value) > abs (high_value - value) )
	{
		return (high_value);
	}
	else
	{
		return (low_value);
	}
}



// ---------------- KEYBOARD INPUT AND OUTPUT ROUTINES ----------------



char * CONTROL_get_control_description (int player_number, int control_number)
{
	// This returns a nicely formatted line for when outputting all the control defines. Lovely.

	static char word[MAX_LINE_SIZE];
	char temp_word[MAX_LINE_SIZE];

	sprintf (word, "#PLAYER_INPUT %i ", player_number+1);

	strcat (word, GPL_get_entry_name ("CONTROL_CONSTANTS", control_number) );

	strcat (word, " = ");

	if (controls[player_number][control_number].device == DEVICE_KEYBOARD)
	{
		strcat (word, "(KEYBOARD)(");
		strcat (word, &key_name[controls[player_number][control_number].button][0] );
		strcat (word, ")");
	}
	else if (controls[player_number][control_number].device == DEVICE_JOYPAD)
	{
		strcat (word, "(JOYPAD)(");
		sprintf (temp_word,"%i)(%i)",controls[player_number][control_number].port,controls[player_number][control_number].button);
		strcat (word,temp_word);
	}
	else if (controls[player_number][control_number].device == DEVICE_JOYSTICK_POS)
	{
		strcat (word, "(JOYSTICK_AXIS_POSITIVE)(");
		sprintf (temp_word,"%i)(%i)(%i)",controls[player_number][control_number].port,controls[player_number][control_number].stick,controls[player_number][control_number].axis);
		strcat (word,temp_word);
	}
	else if (controls[player_number][control_number].device == DEVICE_JOYSTICK_NEG)
	{
		strcat (word, "(JOYSTICK_AXIS_NEGATIVE)(");
		sprintf (temp_word,"%i)(%i)(%i)",controls[player_number][control_number].port,controls[player_number][control_number].stick,controls[player_number][control_number].axis);
		strcat (word,temp_word);
	}

	strupr (word);

	return word;
}



void CONTROL_define_control_from_config (char *word)
{
	char word_copy[MAX_LINE_SIZE];
	char *pointer;
	int port;
	int stick;
	int button;
	int axis;

	strcpy (word_copy,word);

	pointer = strtok(word_copy," ");

	// Pointer should now be a number, that of the player! Woo!

	int player_num = atoi(pointer);

	if ((player_num < 1) || (player_num >= MAX_PLAYERS))
	{
		// Bah!
		return;
	}

	pointer = strtok(NULL," ");

	// Now it's the direction...

	int control_number = GPL_find_word_value ("CONTROL_CONSTANTS",pointer);

	if (control_number == UNSET)
	{
		// Arse!
		return;
	}

	strcpy (word_copy,word);

	int index = STRING_instr_char (word_copy,'=',0);

	pointer = strtok (&word_copy[index+2],"()");

	// Now it's the control method, woo!

	if (strcmp("KEYBOARD",pointer)==0)
	{
		pointer = strtok (NULL,"()");

		int key_num;
		int selected_key = UNSET;

		for (key_num=0; key_num<MAX_KEYS; key_num++)
		{
			if (strcmp (strupr(&key_name[key_num][0]) , pointer) == 0)
			{
				selected_key = key_num;
			}
		}

		if (selected_key != UNSET)
		{
			controls[player_num-1][control_number].device = DEVICE_KEYBOARD;
			controls[player_num-1][control_number].button = selected_key;
			return;
			// Selected key! Woo!
		}
	}
	else if (strcmp("JOYPAD",pointer)==0)
	{
		pointer = strtok (NULL,"()");

		port = atoi(pointer);

		pointer = strtok (NULL,"()");

		button = atoi(pointer);

		controls[player_num-1][control_number].device = DEVICE_JOYPAD;
		controls[player_num-1][control_number].port = port;
		controls[player_num-1][control_number].button = button;
	}
	else if (strcmp("JOYSTICK_AXIS_POSITIVE",pointer)==0)
	{
		pointer = strtok (NULL,"()");

		port = atoi(pointer);
		
		pointer = strtok (NULL,"()");

		stick = atoi(pointer);

		pointer = strtok (NULL,"()");

		axis = atoi(pointer);

		controls[player_num-1][control_number].device = DEVICE_JOYSTICK_POS;
		controls[player_num-1][control_number].port = port;
		controls[player_num-1][control_number].stick = stick;
		controls[player_num-1][control_number].axis = axis;
	}
	else if (strcmp("JOYSTICK_AXIS_NEGATIVE",pointer)==0)
	{
		pointer = strtok (NULL,"()");

		port = atoi(pointer);
		
		pointer = strtok (NULL,"()");

		stick = atoi(pointer);

		pointer = strtok (NULL,"()");

		axis = atoi(pointer);

		controls[player_num-1][control_number].device = DEVICE_JOYSTICK_NEG;

		controls[player_num-1][control_number].port = port;
		controls[player_num-1][control_number].stick = stick;
		controls[player_num-1][control_number].axis = axis;
	}
}



bool CONTROL_key_down (int scancode)
{
	if (scancode >= MAX_KEYS)
	{
		return false;
	}

	if ( (key_store[scancode].current_state != 0) && (key_store[scancode].locked == false) )
	{
		return true;
	}
	else
	{
		return false;
	}
}



bool CONTROL_key_hit (int scancode)
{
	if (scancode >= MAX_KEYS)
	{
		return false;
	}

	if ( (key_store[scancode].old_state == 0) && (key_store[scancode].current_state != 0) && (key_store[scancode].locked == false) )
	{
		return true;
	}
	else
	{
		return false;
	}
}



bool CONTROL_key_release (int scancode)
{
	if (scancode >= MAX_KEYS)
	{
		return false;
	}

	if ( (key_store[scancode].old_state != 0) && (key_store[scancode].current_state == 0) && (key_store[scancode].locked == false) )
	{
		return true;
	}
	else
	{
		return false;
	}
}



bool CONTROL_key_repeat (int scancode, int initial_delay, int repeat_delay , bool ignore_hit)
{
	if (scancode >= MAX_KEYS)
	{
		return false;
	}

	if ((CONTROL_key_hit(scancode)==true) && (ignore_hit == false))
	{
		return true;
	}

	int value;

	if ( (key_store[scancode].current_state != 0) && (key_store[scancode].locked == false) )
	{
		value = key_store[scancode].held_time - initial_delay;
		if ( (value >= 0) && (value % repeat_delay == 0) )
		{
			return true;
		}
		else
		{
			return false;
		}
	}
	else
	{
		return false;
	}
}



bool CONTROL_key_double_click (int scancode, int click_speed)
{
	if (scancode >= MAX_KEYS)
	{
		return false;
	}

	int value;

	if ( (key_store[scancode].current_state != 0) && (key_store[scancode].locked == false) )
	{
		value = key_store[scancode].this_hit - key_store[scancode].prev_hit;
		if (value < click_speed)
		{
			key_store[scancode].this_hit = 0;
			key_store[scancode].prev_hit = key_store[scancode].this_hit - click_speed;
			// Stops it re-reporting.
			return true;
		}
		else
		{
			return false;
		}
	}
	else
	{
		return false;
	}
}



float CONTROL_key_power (int scancode)
{
	if (scancode >= MAX_KEYS)
	{
		return false;
	}

	if (key_store[scancode].locked == false)
	{
		return key_store[scancode].analogue_offset;
	}
	else
	{
		return 0;
	}
}



// ---------------- PAD INPUT AND OUTPUT ROUTINES ----------------



bool CONTROL_joy_down (int port, int button)
{
	if ( (port >= PLATFORM_INPUT_num_joysticks()) || (button >= PLATFORM_INPUT_joystick_num_buttons(port)) )
	{
		// Button or port outside scope.
		return false;
	}

	if ( (pad_store[port][button].current_state != 0) && (pad_store[port][button].locked == false) )
	{
		return true;
	}
	else
	{
		return false;
	}
}



bool CONTROL_joy_hit (int port, int button)
{
	if ( (port >= PLATFORM_INPUT_num_joysticks()) || (button >= PLATFORM_INPUT_joystick_num_buttons(port)) )
	{
		// Button or port outside scope.
		return false;
	}

	if ( (pad_store[port][button].old_state == 0) && (pad_store[port][button].current_state != 0) && (pad_store[port][button].locked == false) )
	{
		return true;
	}
	else
	{
		return false;
	}
}



bool CONTROL_joy_release (int port, int button)
{
	if ( (port >= PLATFORM_INPUT_num_joysticks()) || (button >= PLATFORM_INPUT_joystick_num_buttons(port)) )
	{
		// Button or port outside scope.
		return false;
	}

	if ( (pad_store[port][button].old_state != 0) && (pad_store[port][button].current_state == 0) && (pad_store[port][button].locked == false) )
	{
		return true;
	}
	else
	{
		return false;
	}
}



bool CONTROL_joy_repeat (int port, int button, int initial_delay, int repeat_delay , bool ignore_hit)
{
	if ( (port >= PLATFORM_INPUT_num_joysticks()) || (button >= PLATFORM_INPUT_joystick_num_buttons(port)) )
	{
		// Button or port outside scope.
		return false;
	}

	if ((CONTROL_joy_hit(port,button)==true) && (ignore_hit == false))
	{
		return true;
	}

	int value;

	if ( (pad_store[port][button].current_state != 0) && (pad_store[port][button].locked == false) )
	{
		value = pad_store[port][button].held_time - initial_delay;
		if ( (value >= 0) && (value % repeat_delay == 0) )
		{
			return true;
		}
		else
		{
			return false;
		}
	}
	else
	{
		return false;
	}
}



bool CONTROL_joy_double_click (int port, int button, int click_speed)
{
	if ( (port >= PLATFORM_INPUT_num_joysticks()) || (button >= PLATFORM_INPUT_joystick_num_buttons(port)) )
	{
		// Button or port outside scope.
		return false;
	}

	int value;

	if ( (pad_store[port][button].current_state != 0) && (pad_store[port][button].locked == false) )
	{
		value = pad_store[port][button].this_hit - pad_store[port][button].prev_hit;
		if (value < click_speed)
		{
			pad_store[port][button].this_hit = 0;
			pad_store[port][button].prev_hit = pad_store[port][button].this_hit - click_speed;
			// Stops it re-reporting.
			return true;
		}
		else
		{
			return false;
		}
	}
	else
	{
		return false;
	}
}



float CONTROL_joy_power (int port, int button)
{
	if ( (port >= PLATFORM_INPUT_num_joysticks()) || (button >= PLATFORM_INPUT_joystick_num_buttons(port)) )
	{
		// Button or port outside scope.
		return false;
	}

	if (pad_store[port][button].locked == false)
	{
		return pad_store[port][button].analogue_offset;
	}
	else
	{
		return 0;
	}
}



// ---------------- STICK INPUT AND OUTPUT ROUTINES ----------------



bool CONTROL_stick_down (int port, int stick, int axis, int side)
{
	if ( (port >= PLATFORM_INPUT_num_joysticks()) || (stick >= PLATFORM_INPUT_joystick_num_sticks(port)) || (axis >= PLATFORM_INPUT_joystick_stick_num_axes(port,stick)) )
	{
		// Button or port outside scope.
		return false;
	}

	if ( (stick_store[port][stick][axis][side].current_state != 0) && (stick_store[port][stick][axis][side].locked == false) )
	{
		return true;
	}
	else
	{
		return false;
	}
}



bool CONTROL_stick_hit (int port, int stick, int axis, int side)
{
	if ( (port >= PLATFORM_INPUT_num_joysticks()) || (stick >= PLATFORM_INPUT_joystick_num_sticks(port)) || (axis >= PLATFORM_INPUT_joystick_stick_num_axes(port,stick)) )
	{
		// Button or port outside scope.
		return false;
	}

	if ( (stick_store[port][stick][axis][side].old_state == 0) && (stick_store[port][stick][axis][side].current_state != 0) && (stick_store[port][stick][axis][side].locked == false) )
	{
		return true;
	}
	else
	{
		return false;
	}
}



bool CONTROL_stick_release (int port, int stick, int axis, int side)
{
	if ( (port >= PLATFORM_INPUT_num_joysticks()) || (stick >= PLATFORM_INPUT_joystick_num_sticks(port)) || (axis >= PLATFORM_INPUT_joystick_stick_num_axes(port,stick)) )
	{
		// Button or port outside scope.
		return false;
	}

	if ( (stick_store[port][stick][axis][side].old_state != 0) && (stick_store[port][stick][axis][side].current_state == 0) && (stick_store[port][stick][axis][side].locked == false) )
	{
		return true;
	}
	else
	{
		return false;
	}
}



bool CONTROL_stick_repeat (int port, int stick, int axis, int side, int initial_delay, int repeat_delay , bool ignore_hit)
{
	if ( (port >= PLATFORM_INPUT_num_joysticks()) || (stick >= PLATFORM_INPUT_joystick_num_sticks(port)) || (axis >= PLATFORM_INPUT_joystick_stick_num_axes(port,stick)) )
	{
		// Button or port outside scope.
		return false;
	}

	if ((CONTROL_stick_hit(port,stick,axis,side)==true) && (ignore_hit == false))
	{
		return true;
	}

	int value;

	if ( (stick_store[port][stick][axis][side].current_state != 0) && (stick_store[port][stick][axis][side].locked == false) )
	{
		value = stick_store[port][stick][axis][side].held_time - initial_delay;
		if ( (value >= 0) && (value % repeat_delay == 0) )
		{
			return true;
		}
		else
		{
			return false;
		}
	}
	else
	{
		return false;
	}
}



bool CONTROL_stick_double_click (int port, int stick, int axis, int side, int click_speed)
{
	if ( (port >= PLATFORM_INPUT_num_joysticks()) || (stick >= PLATFORM_INPUT_joystick_num_sticks(port)) || (axis >= PLATFORM_INPUT_joystick_stick_num_axes(port,stick)) )
	{
		// Button or port outside scope.
		return false;
	}

	int value;

	if ( (stick_store[port][stick][axis][side].current_state != 0) && (stick_store[port][stick][axis][side].locked == false) )
	{
		value = stick_store[port][stick][axis][side].this_hit - stick_store[port][stick][axis][side].prev_hit;
		if (value < click_speed)
		{
			stick_store[port][stick][axis][side].this_hit = 0;
			stick_store[port][stick][axis][side].prev_hit = stick_store[port][stick][axis][side].this_hit - click_speed;
			// Stops it re-reporting.
			return true;
		}
		else
		{
			return false;
		}
	}
	else
	{
		return false;
	}
}



float CONTROL_stick_power (int port, int stick, int axis, int side)
{
	if ( (port >= PLATFORM_INPUT_num_joysticks()) || (stick >= PLATFORM_INPUT_joystick_num_sticks(port)) || (axis >= PLATFORM_INPUT_joystick_stick_num_axes(port,stick)) )
	{
		// Button or port outside scope.
		return false;
	}

	if (stick_store[port][stick][axis][side].locked == false)
	{
		return stick_store[port][stick][axis][side].analogue_offset;
	}
	else
	{
		return 0;
	}
}



// ---------------- MOUSE INPUT AND OUTPUT ROUTINES ----------------



void CONTROL_position_mouse (int x, int y)
{
	position_mouse (x,y);
}



void CONTROL_lock_mouse_button (int button)
{
	// This temporarily stops the given button from reporting a mousedown until it's
	// first been released.

	mouse_button_store[button].locked = true;
}



bool CONTROL_mouse_down (int button)
{
	if (button >= MAX_BUTTONS)
	{
		return false;
	}

	if ( (mouse_button_store[button].current_state != 0) && (mouse_button_store[button].locked == false ) )
	{
		return true;
	}
	else
	{
		return false;
	}
}



bool CONTROL_mouse_hit (int button)
{
	if (button >= MAX_BUTTONS)
	{
		return false;
	}

	if ( (mouse_button_store[button].old_state == 0) && (mouse_button_store[button].current_state != 0) && (mouse_button_store[button].locked == false) )
	{
		return true;
	}
	else
	{
		return false;
	}
}



bool CONTROL_mouse_release (int button)
{
	if (button >= MAX_BUTTONS)
	{
		return false;
	}

	if ( (mouse_button_store[button].old_state != 0) && (mouse_button_store[button].current_state == 0) && (mouse_button_store[button].locked == false) )
	{
		return true;
	}
	else
	{
		return false;
	}
}



bool CONTROL_mouse_repeat (int button , int initial_delay, int repeat_delay , bool ignore_hit)
{
	if (button >= MAX_BUTTONS)
	{
		return false;
	}

	if ((CONTROL_mouse_hit(button)==true) && (ignore_hit == false))
	{
		return true;
	}

	int value;

	if ( (mouse_button_store[button].current_state != 0) && (mouse_button_store[button].locked == false) )
	{
		value = mouse_button_store[button].held_time - initial_delay;
		if ( (value > 0) && (value % repeat_delay == 0) )
		{
			return true;
		}
		else
		{
			return false;
		}
	}
	else
	{
		return false;
	}
}



bool CONTROL_mouse_double_click (int button, int click_speed)
{
	if (button >= MAX_BUTTONS)
	{
		return false;
	}

	int value;

	if ( (mouse_button_store[button].current_state != 0) && (mouse_button_store[button].locked == false) )
	{
		value = mouse_button_store[button].this_hit - mouse_button_store[button].prev_hit;
		if (value < click_speed)
		{
			mouse_button_store[button].this_hit = 0;
			mouse_button_store[button].prev_hit = mouse_button_store[button].this_hit - click_speed;
			// Stops it re-reporting.
			return true;
		}
		else
		{
			return false;
		}
	}
	else
	{
		return false;
	}
}



int CONTROL_mouse_speed (int button)
{
	if (button >= MAX_BUTTONS)
	{
		return 0;
	}

	int t;
	int total;
	int buffer_size;

	total = 0;
	buffer_size = mouse_sampling_size[button];
	
	for (t=0; t<buffer_size; t++)
	{
		total += mouse_buffer[button][t];
	}

	return (total/buffer_size);
}



int CONTROL_mouse_power (int button)
{
	if (button >= MAX_BUTTONS)
	{
		return 0;
	}

	int value;
	int percent;

	value = CONTROL_mouse_speed (button);

	percent = (100 * value) / max_mouse_speed;
	percent = MATH_constrain (percent,-100,100);
	
	return percent;
}



int CONTROL_mouse_x (void)
{
	return mouse_position_store[X_POS].current_position;
}



int CONTROL_mouse_y (void)
{
	return mouse_position_store[Y_POS].current_position;
}



int CONTROL_mouse_z (void)
{
	return mouse_position_store[Z_POS].current_position;
}



// ---------------- PLAYER INPUT AND OUTPUT ROUTINES ----------------



void CONTROL_lock_player_button (int player, int button)
{
	// This temporarily stops the given button from reporting a mousedown until it's
	// first been released.

	locked_player_input[player][button] = true;
}



bool CONTROL_is_player_button_undefined (int player, int button)
{
	if (controls[player][button].device == UNSET)
	{
		return true;
	}
	else
	{
		return false;
	}
}



bool CONTROL_player_button_hit (int player, int button)
{
	if (rpi[player].playback == true)
	{
		// If we're playing back...

		if (rpi[player].currently_playing < rpi[player].frames_recorded)
		{
			// And we ain't exhausted the playback data...

//			if (rpi[player].current_frame_data.boolean[button] != rpi[player].data[rpi[player].currently_playing].boolean[button])
//			{
//				assert(0);
//			}

			if (rpi[player].current_frame_data.boolean[button] & BOOL_CONTROL_HIT)
			{
				return true;
			}
			else
			{
				return false;
			}
		}
	}

	bool result;
	bool returned_result;

	switch ( controls[player][button].device )
	{
	case DEVICE_KEYBOARD: // Keyboard feedback
		result = CONTROL_key_hit (controls[player][button].button);
		break;

	case DEVICE_JOYPAD: // Digital button feedback
		result = CONTROL_joy_hit (controls[player][button].port , controls[player][button].button);
		break;
	
	case DEVICE_JOYSTICK_POS: // Positive value on the axis
		result = CONTROL_stick_hit (controls[player][button].port , controls[player][button].stick , controls[player][button].axis , AXIS_POS_STATE);
		break;

	case DEVICE_JOYSTICK_NEG: // Negative value on the axis
		result = CONTROL_stick_hit (controls[player][button].port , controls[player][button].stick , controls[player][button].axis , AXIS_NEG_STATE);
		break;

	case DEVICE_MOUSE_BUTTONS: // Mouse button input
		result = CONTROL_mouse_hit (controls[player][button].button);
		break;

	default:
		assert(0);
		// Well I don't know what the hell to do, do you?
		break;
	}

	if (result == true)
	{
		if (locked_player_input[player][button] == true)
		{
			returned_result = false;
		}
		else
		{
			returned_result = true;
		}
	}
	else
	{
		locked_player_input[player][button] = false;
		returned_result = false;
	}

	if (rpi[player].recording == true)
	{
		if (returned_result)
		{
//			rpi[player].data[rpi[player].frames_recorded].boolean[button] |= BOOL_CONTROL_HIT;
			rpi[player].current_frame_data.boolean[button] |= BOOL_CONTROL_HIT;
		}
	}

	return returned_result;
}



bool CONTROL_player_button_down (int player, int button)
{
	if (rpi[player].playback == true)
	{
		// If we're playing back...

		if (rpi[player].currently_playing < rpi[player].frames_recorded)
		{
			// And we ain't exhausted the playback data...

			if (rpi[player].current_frame_data.boolean[button] & BOOL_CONTROL_DOWN)
			{
				return true;
			}
			else
			{
				return false;
			}
		}
	}

	bool result;
	bool returned_result;

	switch ( controls[player][button].device )
	{
	case DEVICE_KEYBOARD: // Keyboard feedback
		result = CONTROL_key_down (controls[player][button].button);
		break;

	case DEVICE_JOYPAD: // Digital button feedback
		result = CONTROL_joy_down (controls[player][button].port , controls[player][button].button);
		break;
	
	case DEVICE_JOYSTICK_POS: // Positive value on the axis
		result = CONTROL_stick_down (controls[player][button].port , controls[player][button].stick , controls[player][button].axis , AXIS_POS_STATE);
		break;

	case DEVICE_JOYSTICK_NEG: // Negative value on the axis
		result = CONTROL_stick_down (controls[player][button].port , controls[player][button].stick , controls[player][button].axis , AXIS_NEG_STATE);
		break;

	case DEVICE_MOUSE_BUTTONS: // Mouse button input
		result = CONTROL_mouse_down (controls[player][button].button);
		break;

	default:
		assert(0);
		// Well I don't know what the hell to do, do you?
		break;
	}

	if (result == true)
	{
		if (locked_player_input[player][button] == true)
		{
			returned_result = false;
		}
		else
		{
			returned_result = true;
		}
	}
	else
	{
		locked_player_input[player][button] = false;
		returned_result = false;
	}

	if (rpi[player].recording == true)
	{
		if (returned_result)
		{
//			rpi[player].data[rpi[player].frames_recorded].boolean[button] |= BOOL_CONTROL_DOWN;
			rpi[player].current_frame_data.boolean[button] |= BOOL_CONTROL_DOWN;
		}
	}

	return returned_result;
}



bool CONTROL_player_button_release (int player, int button)
{
	if (rpi[player].playback == true)
	{
		// If we're playing back...

		if (rpi[player].currently_playing < rpi[player].frames_recorded)
		{
			// And we ain't exhausted the playback data...

			if (rpi[player].current_frame_data.boolean[button] & BOOL_CONTROL_RELEASE)
			{
				return true;
			}
			else
			{
				return false;
			}
		}
	}

	bool result;
	bool returned_result;

	switch ( controls[player][button].device )
	{
	case DEVICE_KEYBOARD: // Keyboard feedback
		result = CONTROL_key_release (controls[player][button].button);
		break;

	case DEVICE_JOYPAD: // Digital button feedback
		result = CONTROL_joy_release (controls[player][button].port , controls[player][button].button);
		break;
	
	case DEVICE_JOYSTICK_POS: // Positive value on the axis
		result = CONTROL_stick_release (controls[player][button].port , controls[player][button].stick , controls[player][button].axis , AXIS_POS_STATE);
		break;

	case DEVICE_JOYSTICK_NEG: // Negative value on the axis
		result = CONTROL_stick_release (controls[player][button].port , controls[player][button].stick , controls[player][button].axis , AXIS_NEG_STATE);
		break;

	case DEVICE_MOUSE_BUTTONS: // Mouse button input
		result = CONTROL_mouse_release (controls[player][button].button);
		break;

	default:
		assert(0);
		// Well I don't know what the hell to do, do you?
		break;
	}

	if (result == true)
	{
		if (locked_player_input[player][button] == true)
		{
			returned_result = false;
		}
		else
		{
			returned_result = true;
		}
	}
	else
	{
		locked_player_input[player][button] = false;
		returned_result = false;
	}

	if (rpi[player].recording == true)
	{
		if (returned_result)
		{
//			rpi[player].data[rpi[player].frames_recorded].boolean[button] |= BOOL_CONTROL_RELEASE;
			rpi[player].current_frame_data.boolean[button] |= BOOL_CONTROL_RELEASE;
		}
	}

	return returned_result;
}



int CONTROL_player_button_power (int player, int button)
{
	int result;
	int returned_result;

	switch ( controls[player][button].device )
	{
	case DEVICE_KEYBOARD: // Keyboard feedback
		result = CONTROL_key_power (controls[player][button].button);
		break;

	case DEVICE_JOYPAD: // Digital button feedback
		result = CONTROL_joy_power (controls[player][button].port , controls[player][button].button);
		break;
	
	case DEVICE_JOYSTICK_POS: // Positive value on the axis
		result = CONTROL_stick_power (controls[player][button].port , controls[player][button].stick , controls[player][button].axis , AXIS_POS_STATE);
		break;

	case DEVICE_JOYSTICK_NEG: // Negative value on the axis
		result = CONTROL_stick_power (controls[player][button].port , controls[player][button].stick , controls[player][button].axis , AXIS_NEG_STATE);
		break;

	case DEVICE_MOUSE_BUTTONS: // Mouse button input
		result = CONTROL_mouse_power (controls[player][button].button);
		break;

	default:
		assert(0);
		// Well I don't know what the hell to do, do you?
		break;
	}

	if (result != 0)
	{
		if (locked_player_input[player][button] == true)
		{
			returned_result = 0;
		}
		else
		{
			returned_result = result;
		}
	}
	else
	{
		locked_player_input[player][button] = false;
		returned_result = 0;
	}

	if (rpi[player].recording == true)
	{
//		rpi[player].data[rpi[player].frames_recorded].power[button] = returned_result;
		rpi[player].current_frame_data.power[button] = returned_result;
	}

	return returned_result;
}



bool CONTROL_player_button_repeat (int player, int button, int initial_delay, int repeat_delay, bool ignore_hit)
{
	if (rpi[player].playback == true)
	{
		// If we're playing back...

		if (rpi[player].currently_playing < rpi[player].frames_recorded)
		{
			// And we ain't exhausted the playback data...

			if (rpi[player].current_frame_data.boolean[button] & BOOL_CONTROL_REPEAT)
			{
				return true;
			}
			else
			{
				return false;
			}
		}
	}

	bool result;
	bool returned_result;

	switch ( controls[player][button].device )
	{
	case DEVICE_KEYBOARD: // Keyboard feedback
		result = CONTROL_key_repeat (controls[player][button].button , initial_delay , repeat_delay , ignore_hit);
		break;

	case DEVICE_JOYPAD: // Digital button feedback
		result = CONTROL_joy_repeat (controls[player][button].port , controls[player][button].button , initial_delay , repeat_delay , ignore_hit);
		break;
	
	case DEVICE_JOYSTICK_POS: // Positive value on the axis
		result = CONTROL_stick_repeat (controls[player][button].port , controls[player][button].stick , controls[player][button].axis , AXIS_POS_STATE , initial_delay , repeat_delay , ignore_hit);
		break;

	case DEVICE_JOYSTICK_NEG: // Negative value on the axis
		result = CONTROL_stick_repeat (controls[player][button].port , controls[player][button].stick , controls[player][button].axis , AXIS_NEG_STATE , initial_delay , repeat_delay , ignore_hit);
		break;

	case DEVICE_MOUSE_BUTTONS: // Mouse button input
		result = CONTROL_mouse_repeat (controls[player][button].button , initial_delay , repeat_delay , ignore_hit);
		break;

	default:
		assert(0);
		// Well I don't know what the hell to do, do you?
		break;
	}

	if (result == true)
	{
		if (locked_player_input[player][button] == true)
		{
			returned_result = false;
		}
		else
		{
			returned_result = true;
		}
	}
	else
	{
		locked_player_input[player][button] = false;
		returned_result = false;
	}

	if (rpi[player].recording == true)
	{
		if (returned_result)
		{
//			rpi[player].data[rpi[player].frames_recorded].boolean[button] |= BOOL_CONTROL_REPEAT;
			rpi[player].current_frame_data.boolean[button] |= BOOL_CONTROL_REPEAT;
		}
	}

	return returned_result;
}



bool CONTROL_player_button_double_click (int player, int button, int click_speed)
{
	if (rpi[player].playback == true)
	{
		// If we're playing back...

		if (rpi[player].currently_playing < rpi[player].frames_recorded)
		{
			// And we ain't exhausted the playback data...

			if (rpi[player].current_frame_data.boolean[button] & BOOL_CONTROL_DOUBLE_CLICK)
			{
				return true;
			}
			else
			{
				return false;
			}
		}
	}

	bool result;
	bool returned_result;

	switch ( controls[player][button].device )
	{
	case DEVICE_KEYBOARD: // Keyboard feedback
		result = CONTROL_key_double_click (controls[player][button].button , click_speed);
		break;

	case DEVICE_JOYPAD: // Digital button feedback
		result = CONTROL_joy_double_click (controls[player][button].port , controls[player][button].button , click_speed);
		break;
	
	case DEVICE_JOYSTICK_POS: // Positive value on the axis
		result = CONTROL_stick_double_click (controls[player][button].port , controls[player][button].stick , controls[player][button].axis , AXIS_POS_STATE , click_speed);
		break;

	case DEVICE_JOYSTICK_NEG: // Negative value on the axis
		result = CONTROL_stick_double_click (controls[player][button].port , controls[player][button].stick , controls[player][button].axis , AXIS_NEG_STATE , click_speed);
		break;

	case DEVICE_MOUSE_BUTTONS: // Mouse button input
		result = CONTROL_mouse_double_click (controls[player][button].button , click_speed);
		break;

	default:
		assert(0);
		// Well I don't know what the hell to do, do you?
		break;
	}

	if (result == true)
	{
		if (locked_player_input[player][button] == true)
		{
			returned_result = false;
		}
		else
		{
			returned_result = true;
		}
	}
	else
	{
		locked_player_input[player][button] = false;
		returned_result = false;
	}

	if (rpi[player].recording == true)
	{
		if (returned_result)
		{
//			rpi[player].data[rpi[player].frames_recorded].boolean[button] |= BOOL_CONTROL_DOUBLE_CLICK;
			rpi[player].current_frame_data.boolean[button] |= BOOL_CONTROL_DOUBLE_CLICK;
		}
	}

	return returned_result;
}



// ---------------- DEVICE AVAILABILITY ----------------



bool CONTROL_is_keyboard_available(void)
{
	return keyboard_setup_okay;
}



bool CONTROL_is_joystick_available(void)
{
	return joystick_setup_okay;
}



bool CONTROL_is_mouse_available(void)
{
	return mouse_setup_okay;
}



// ---------------- UPDATE ----------------



void CONTROL_update_keyboard (void)
{
	// This polls the keyboard then stores the state of every key, making a backup of its old
	// state so that we can tell if the state's changed since the last poll. As long as it's
	// held down we also increase a counter which can be used for keyrepeats.

	PLATFORM_INPUT_poll_keyboard();

	int t;

	bool any_hit = false;
	bool any_down = false;

	for (t=1; t<MAX_KEYS; t++)
	{
		key_store[t].old_state = key_store[t].current_state;
		key_store[t].current_state = PLATFORM_INPUT_key_state(t);

		if ( key_store[t].current_state != 0 ) // If a key is held down then increase the held time value...
		{
			any_down = true;
			key_store[t].held_time++;
			key_store[t].analogue_offset = 100;
		}
		else // Else zero it and unlock the button.
		{
			key_store[t].held_time = 0;
			key_store[t].locked = false;
			key_store[t].analogue_offset = 0;
		}

		if ( (key_store[t].old_state == 0) && (key_store[t].current_state != 0) )
		{
			any_hit = true;
			key_store[t].prev_hit = key_store[t].this_hit;
			key_store[t].this_hit = frame_counter;
		}
	}

	// Now deal with key 0, which is effectively the "any" key.

	if (any_hit)
	{
		key_store[0].prev_hit = key_store[0].this_hit;
		key_store[0].this_hit = frame_counter;

		key_store[0].old_state = 0;
		key_store[0].current_state = 1;
	}
	else
	{
		if (any_down)
		{
			key_store[0].held_time++;
			key_store[0].analogue_offset = 100;
			key_store[0].current_state = 1;

			key_store[0].old_state = 1;
			key_store[0].current_state = 1;
		}
		else
		{
			key_store[0].held_time = 0;
			key_store[0].locked = false;
			key_store[0].analogue_offset = 0;

			key_store[0].old_state = 0;
			key_store[0].current_state = 0;
		}
	}
}



void CONTROL_update_joypads (void)
{
	// This polls the joysticks then stores the state of every button and stick, making a backup of
	// its old state so that we can tell if the state's changed since the last poll. As long as it's
	// held down we also increase a counter which can be used for keyrepeats.

	// We also store how far any sticks are being pushed outside of the deadzone so you can poll that and
	// use it to decide the speed or style of movement of a player.

	PLATFORM_INPUT_poll_joysticks();

	int port;
	int button;

	for (port=0; port<PLATFORM_INPUT_num_joysticks(); port++)
	{
		for (button=0; button<PLATFORM_INPUT_joystick_num_buttons(port) ; button++)
		{
			pad_store[port][button].old_state = pad_store[port][button].current_state;
			pad_store[port][button].current_state = PLATFORM_INPUT_joystick_button_state(port,button);

			if (pad_store[port][button].current_state != 0 ) // If a button is held down then increase the held time value...
			{
				pad_store[port][button].held_time++;
				pad_store[port][button].analogue_offset = 100;
			}
			else // Else zero it and unlock the button.
			{
				pad_store[port][button].held_time = 0;
				pad_store[port][button].locked = false;
				pad_store[port][button].analogue_offset = 0;
			}

			if ( (pad_store[port][button].old_state == 0) && (pad_store[port][button].current_state != 0) )
			{
				pad_store[port][button].prev_hit = pad_store[port][button].this_hit;
				pad_store[port][button].this_hit = frame_counter;
			}
		}
	}

	int stick;
	int axis;
	int result;
	int state_number;
	float percent;

	for (port=0; port<PLATFORM_INPUT_num_joysticks(); port++)
	{
		for (stick=0; stick<PLATFORM_INPUT_joystick_num_sticks(port) ; stick++)
		{
			for (axis=0; axis<PLATFORM_INPUT_joystick_stick_num_axes(port,stick) ; axis++)
			{
				result = PLATFORM_INPUT_joystick_axis_pos(port,stick,axis);

				stick_store[port][stick][axis][AXIS_POS_STATE].old_position = stick_store[port][stick][axis][AXIS_POS_STATE].current_position;
				stick_store[port][stick][axis][AXIS_POS_STATE].current_position = result;

				stick_store[port][stick][axis][AXIS_POS_STATE].old_state = stick_store[port][stick][axis][AXIS_POS_STATE].current_state;
				stick_store[port][stick][axis][AXIS_NEG_STATE].old_state = stick_store[port][stick][axis][AXIS_NEG_STATE].current_state;

				if ( result > default_axis_value[port][stick][axis].default_position + deadzone )
				{
					stick_store[port][stick][axis][AXIS_POS_STATE].current_state = 1;
					stick_store[port][stick][axis][AXIS_NEG_STATE].current_state = 0;

					percent = MATH_unlerp ( default_axis_value[port][stick][axis].default_position + deadzone , default_axis_value[port][stick][axis].upper_position , result );

					stick_store[port][stick][axis][AXIS_POS_STATE].analogue_offset = int (percent * 100);
					stick_store[port][stick][axis][AXIS_NEG_STATE].analogue_offset = 0;
				}
				else if ( result < default_axis_value[port][stick][axis].default_position - deadzone )
				{
					stick_store[port][stick][axis][AXIS_POS_STATE].current_state = 0;
					stick_store[port][stick][axis][AXIS_NEG_STATE].current_state = 1;

					percent = MATH_unlerp ( default_axis_value[port][stick][axis].default_position - deadzone , default_axis_value[port][stick][axis].lower_position , result );

					stick_store[port][stick][axis][AXIS_POS_STATE].analogue_offset = 0;
					stick_store[port][stick][axis][AXIS_NEG_STATE].analogue_offset = int (percent * 100);
				}
				else
				{
					stick_store[port][stick][axis][AXIS_POS_STATE].current_state = 0;
					stick_store[port][stick][axis][AXIS_NEG_STATE].current_state = 0;
					percent = 0;

					stick_store[port][stick][axis][AXIS_POS_STATE].analogue_offset = 0;
					stick_store[port][stick][axis][AXIS_NEG_STATE].analogue_offset = 0;
				}

				for (state_number=0; state_number< MAX_AXIS_STATES; state_number++)
				{

					if (stick_store[port][stick][axis][state_number].current_state != 0 ) // If a button is held down then increase the held time value...
					{
						stick_store[port][stick][axis][state_number].held_time++;
					}
					else // Else zero it and unlock the button.
					{
						stick_store[port][stick][axis][state_number].held_time = 0;
						stick_store[port][stick][axis][state_number].locked = false;
					}

					if ( (pad_store[port][button].old_state == 0) && (stick_store[port][stick][axis][state_number].current_state != 0) ) // If it's a buttonhit then set the repeat value to the repeat value as a repeat should count as a hit...
					{
						stick_store[port][stick][axis][state_number].prev_hit = stick_store[port][stick][axis][state_number].this_hit;
						stick_store[port][stick][axis][state_number].this_hit = frame_counter;
					}

				}
			
			}
		}
	}

}



void CONTROL_update_mouse (void)
{
	PLATFORM_INPUT_poll_mouse();

	int button;

	for (button=0; button<MOUSE_BUTTONS; button++)
	{
		mouse_button_store[button].old_state = mouse_button_store[button].current_state;
		mouse_button_store[button].current_state = ((PLATFORM_INPUT_mouse_buttons_mask() & 1<<button) > 0);

		if ( mouse_button_store[button].current_state != 0 ) // If a button is held down then increase the repeat value...
		{
			mouse_button_store[button].held_time++;
			mouse_button_store[button].analogue_offset = 100;
		}
		else // If it's not held down and it's locked then unlock it.
		{
			mouse_button_store[button].held_time = 0;
			mouse_button_store[button].analogue_offset = 0;
			mouse_button_store[button].locked = false;
		}

		if ( (mouse_button_store[button].old_state == 0) && (mouse_button_store[button].current_state != 0) )
		{
			mouse_button_store[button].prev_hit = mouse_button_store[button].this_hit;
			mouse_button_store[button].this_hit = frame_counter;
		}
	}

	int diff;
	int buffer_size;

	for (button=0; button<MOUSE_AXIS; button++)
	{
		mouse_position_store[button].old_position = mouse_position_store[button].current_position;

		switch (button)
		{
		case X_POS:
			mouse_position_store[button].current_position = PLATFORM_INPUT_mouse_x();
			break;

		case Y_POS:
			mouse_position_store[button].current_position = PLATFORM_INPUT_mouse_y();
			break;

		case Z_POS:
			mouse_position_store[button].current_position = PLATFORM_INPUT_mouse_z();
			break;

		default:
			break;
		}

		diff = mouse_position_store[button].current_position - mouse_position_store[button].old_position;
		buffer_size = mouse_sampling_size[button];

		mouse_buffer[button][frame_counter % buffer_size] = diff;
	}
}



// ---------------- MISC FUNCTIONALITY ----------------



int CONTROL_get_word (char *word , int status , unsigned int max_length , bool capitalise, bool no_spaces)
{
	// This function allows you to type in strings, it should be the first thing called after
	// updating the keyboard and mouse. I'd advise using the returned status to decide to blank
	// the mouse and keyboard input in order to stop keypresses getting misinterpreted.

	char read_char[2];
	int t;

	switch (status)
	{

	case GOTTEN_WORD: // Just so that you get a 1 frame long tick to tell you the user has pressed ENTER.
		status = DO_NOTHING; 
		break;

	case GETTING_WORD:
		if (strlen(word) < max_length) // If it isn't at maximum length already
		{
			for (t=0 ; t<MAX_KEYS ; t++)
			{
				if (CONTROL_key_repeat(t) == true)
				{
					if ( (capitalise == true) && (CONTROL_key_down(KEY_LSHIFT) == false) ) // Upper case letters, but no shifted characters
					{
						read_char[0] = capitalised_character_array[t];
						read_char[1] = '\0';
					}
					else if (CONTROL_key_down(KEY_LSHIFT) == true) // Upper case letters and shifted characters
					{
						read_char[0] = upper_character_array[t];
						read_char[1] = '\0';
					}
					else
					{
						read_char[0] = lower_character_array[t]; // Lower case letters and non-shifted characters
						read_char[1] = '\0';
						
					}
					if ( (no_spaces == false) || (read_char[0] != ' ') )
					{
						strcat(word,read_char);
					}
				}
			}
		}

		if ( (CONTROL_key_repeat(KEY_DEL_PAD) == true) || (CONTROL_key_repeat(KEY_BACKSPACE) == true) )
		{
			if (strlen(word) > 0)
			{
				word[strlen(word)-1] = '\0';
			}
		}

		if (CONTROL_key_repeat(KEY_DEL) == true)
		{
			sprintf(word,"");
		}

		if ( (CONTROL_key_hit(KEY_ENTER) == true) || (CONTROL_key_hit(KEY_ESC) == true) || (CONTROL_key_hit(KEY_ENTER_PAD) == true))
		{
			status = GOTTEN_WORD;
		}
		break;

	case GET_NEW_WORD:
		status = GETTING_WORD;
		strcpy (word,""); // blank word ready for input.
		break;

	case GET_WORD:
		status = GETTING_WORD;
		break;

	default:
		break;

	}

	return status;

}



bool CONTROL_grab_start (int button)
{
	if (CONTROL_mouse_hit (button))
	{
		stored_mouse_x = CONTROL_mouse_x();
		stored_mouse_y = CONTROL_mouse_y();
		return true;
	}
	return false;
}



int CONTROL_grab_offset_x (void)
{
	return (stored_mouse_x - CONTROL_mouse_x());
}



int CONTROL_grab_offset_y (void)
{
	return (stored_mouse_y - CONTROL_mouse_y());
}



void CONTROL_set_stick_sensitivity (int sensitivity)
{
	deadzone = sensitivity;
}



int CONTROL_get_stick_sensitivity (void)
{
	return deadzone;
}



void CONTROL_set_mouse_sensitivity (int sensitivity)
{
	max_mouse_speed = sensitivity;
}



int CONTROL_get_mouse_sensitivity (void)
{
	return max_mouse_speed;
}



void CONTROL_set_mouse_smoothing (int mouse_button, int sampling_rate)
{
	mouse_sampling_size[mouse_button] = sampling_rate;
}



void CONTROL_update_sequences (void)
{
	// Goes through all the loaded button sequences and players updating the progress of any
	// sequences as necessary.


}



void CONTROL_constrain_mouse (int x, int y, int distance)
{

}



int CONTROL_read_back_player_control_device (int player, int control)
{
	return (controls[player][control].device);
}



int CONTROL_read_back_player_control_port (int player, int control)
{
	return (controls[player][control].port);
}



int CONTROL_read_back_player_control_button (int player, int control)
{
	return (controls[player][control].button);
}



int CONTROL_read_back_player_control_stick (int player, int control)
{
	return (controls[player][control].stick);
}



int CONTROL_read_back_player_control_axis (int player, int control)
{
	return (controls[player][control].axis);
}



bool CONTROL_get_keypress (int player, int control, char *description, bool check_for_match)
{
	// This function polls the keyboard and joysticks and then finds what's been pressed and stores
	// it in the correct place in the player control array of structs.

	// It's used for when you're defining the controls, that's why you pass it a name so it can
	// store the pressed key's name there.

	bool defined = false;
	int port,button,stick,axis;
	int previous_input;
	bool check_passed;

	if (keyboard_setup_okay == true)
	{
		for (button=1; button<MAX_KEYS; button++)
		{
			if (CONTROL_key_hit(button) == true)
			{
				if (check_for_match == true)
				{
					check_passed = true;
					
					for (previous_input=0; previous_input<NUM_BUTTONS; previous_input++)
					{
						if ((controls[player][previous_input].dupe_check == true) && (controls[player][previous_input].device == DEVICE_KEYBOARD) && (controls[player][previous_input].button == button))
						{
							check_passed = false;
						}
					}
				}
				else
				{
					check_passed = true;
				}

				if (check_passed == true)
				{
					controls[player][control].device = DEVICE_KEYBOARD;
					controls[player][control].button = button;
					if (description != NULL)
					{
						sprintf (description,"Keyboard button %s",&key_name[button][0]);
					}
					defined = true;
				}
			}
		}
	}

	if (joystick_setup_okay == true)
	{
		for (port=0; port<PLATFORM_INPUT_num_joysticks(); port++)
		{
			for (button=0; button<PLATFORM_INPUT_joystick_num_buttons(port) ; button++)
			{
				if (CONTROL_joy_hit(port,button) == true)
				{
					if (check_for_match == true)
					{
						check_passed = true;
						
						for (previous_input=0; previous_input<NUM_BUTTONS; previous_input++)
						{
							if ((controls[player][previous_input].dupe_check == true) && (controls[player][previous_input].device == DEVICE_JOYPAD) && (controls[player][previous_input].port == port) && (controls[player][previous_input].button == button))
							{
								check_passed = false;
							}
						}
					}
					else
					{
						check_passed = true;
					}

					if (check_passed == true)
					{
						controls[player][control].device = DEVICE_JOYPAD;
						controls[player][control].port = port;
						controls[player][control].button = button;
						if (description != NULL)
						{
							sprintf (description,"Joypad on port %d, button %d",port,button);
						}
						defined = true;
					}
				}
			}
		}

		for (port=0; port<PLATFORM_INPUT_num_joysticks(); port++)
		{
			for (stick=0; stick<PLATFORM_INPUT_joystick_num_sticks(port) ; stick++)
			{
				for (axis=0; axis<PLATFORM_INPUT_joystick_stick_num_axes(port,stick) ; axis++)
				{
					int result = ( PLATFORM_INPUT_joystick_axis_pos(port,stick,axis) );

					if ( result > default_axis_value [port][stick][axis].default_position + deadzone )
					{
						if (check_for_match == true)
						{
							check_passed = true;
							
							for (previous_input=0; previous_input<NUM_BUTTONS; previous_input++)
							{
								if ((controls[player][previous_input].dupe_check == true) && (controls[player][previous_input].device == DEVICE_JOYSTICK_POS) && (controls[player][previous_input].port == port) && (controls[player][previous_input].stick == stick) && (controls[player][previous_input].axis == axis))
								{
									check_passed = false;
								}
							}
						}
						else
						{
							check_passed = true;
						}

						if (check_passed == true)
						{
							controls[player][control].device = DEVICE_JOYSTICK_POS;
							controls[player][control].port = port;
							controls[player][control].stick = stick;
							controls[player][control].axis = axis;

							defined = true;

							if (description != NULL)
							{
								sprintf (description,"Joypad on port %d, stick %d and axis %d set to +ve. Value = %d. Default = %d.",port,stick,axis,result,default_axis_value [port][stick][axis].default_position);
							}
						}
					}
					else if ( result < default_axis_value [port][stick][axis].default_position - deadzone )
					{
						if (check_for_match == true)
						{
							check_passed = true;

							for (previous_input=0; previous_input<NUM_BUTTONS; previous_input++)
							{
								if ((controls[player][previous_input].dupe_check == true) && (controls[player][previous_input].device == DEVICE_JOYSTICK_NEG) && (controls[player][previous_input].port == port) && (controls[player][previous_input].stick == stick) && (controls[player][previous_input].axis == axis))
								{
									check_passed = false;
								}
							}
						}
						else
						{
							check_passed = true;
						}

						if (check_passed == true)
						{
							controls[player][control].device = DEVICE_JOYSTICK_NEG;
							controls[player][control].port = port;
							controls[player][control].stick = stick;
							controls[player][control].axis = axis;
							
							defined = true;

							if (description != NULL)
							{
								sprintf (description,"Joypad on port %d, stick %d and axis %d set to -ve. Value = %d. Default = %d.",port,stick,axis,result,default_axis_value [port][stick][axis].default_position);
							}
						}
					}
				}
			}
		}

	}

	return defined;
}



// ---------------- RECORDING FUNCTIONS ----------------


/*
void CONTROL_allocate_recording_space (int player_number)
{

	if ((rpi[player_number].frames_recorded == 0) && (rpi[player_number].frames_allocated == 0))
	{
		// Righty, no space allocated as yet...

		rpi[player_number].frames_allocated = RECORDING_ALLOCATION_CHUNKS;
		rpi[player_number].data = (individual_player_input *) malloc (sizeof(individual_player_input) * RECORDING_ALLOCATION_CHUNKS);
	}
	else if (rpi[player_number].frames_recorded == rpi[player_number].frames_allocated)
	{
		// Ooh, we've reached the limit of our space, make some more room!
		
		rpi[player_number].frames_allocated += RECORDING_ALLOCATION_CHUNKS;
		rpi[player_number].data = (individual_player_input *) realloc (rpi[player_number].data , sizeof(individual_player_input) * rpi[player_number].frames_allocated);
	}
	else if (rpi[player_number].frames_recorded < rpi[player_number].frames_allocated)
	{
		// Do nothing!
	}
	else
	{
		// Worry! Because somehow there are more recorded frames than there are allocated ones...
		assert(0);
	}
}
*/



void CONTROL_destroy_stored_data (int player_number)
{
	if (rpi[player_number].written_variable_data != NULL)
	{
		free(rpi[player_number].written_variable_data);
		rpi[player_number].written_variable_data = NULL;
		rpi[player_number].written_variable_count = 0;
	}

	if (rpi[player_number].written_flag_data != NULL)
	{
		free(rpi[player_number].written_flag_data);
		rpi[player_number].written_flag_data = NULL;
		rpi[player_number].written_flag_count = 0;
	}

	if (rpi[player_number].written_array_data != NULL)
	{
		free(rpi[player_number].written_array_data);
		rpi[player_number].written_array_data = NULL;
		rpi[player_number].written_array_data_size = 0;
	}
}



void CONTROL_start_recording_input_next_frame (int player_number)
{
	rpi[player_number].start_recording_this_frame = true;
	
	CONTROL_destroy_stored_data (player_number);
}



void CONTROL_start_playback_input_next_frame (int player_number)
{
	rpi[player_number].start_playback_this_frame = true;
	rpi[player_number].loaded_but_not_played_back = false;
//	MAIN_reset_frame_counter();
}



void CONTROL_destroy_realtime_compressed_recording (int player_number)
{
	int button_number;

	for (button_number=0; button_number<MAX_BUTTONS; button_number++)
	{
		rpi[player_number].realtime_compressed_button_data[button_number].allocated_bool_states = 0;
		rpi[player_number].realtime_compressed_button_data[button_number].current_bool_state = 0;

		rpi[player_number].realtime_compressed_button_data[button_number].allocated_char_states = 0;
		rpi[player_number].realtime_compressed_button_data[button_number].current_char_state = 0;

		if (rpi[player_number].realtime_compressed_button_data[button_number].new_bool_state_value != NULL)
		{
			free(rpi[player_number].realtime_compressed_button_data[button_number].new_bool_state_value);
			rpi[player_number].realtime_compressed_button_data[button_number].new_bool_state_value = NULL;
		}

		if (rpi[player_number].realtime_compressed_button_data[button_number].new_bool_state_length != NULL)
		{
			free(rpi[player_number].realtime_compressed_button_data[button_number].new_bool_state_length);
			rpi[player_number].realtime_compressed_button_data[button_number].new_bool_state_length = NULL;
		}

		if (rpi[player_number].realtime_compressed_button_data[button_number].new_char_state_value != NULL)
		{
			free(rpi[player_number].realtime_compressed_button_data[button_number].new_char_state_value);
			rpi[player_number].realtime_compressed_button_data[button_number].new_char_state_value = NULL;
		}

		if (rpi[player_number].realtime_compressed_button_data[button_number].new_char_state_length != NULL)
		{
			free(rpi[player_number].realtime_compressed_button_data[button_number].new_char_state_length);
			rpi[player_number].realtime_compressed_button_data[button_number].new_char_state_length = NULL;
		}
	}
}



void CONTROL_start_recording_input (int player_number)
{
/*
	if (rpi[player_number].data != NULL)
	{
		free(rpi[player_number].data);
		rpi[player_number].data = NULL;
	}
*/
	CONTROL_destroy_realtime_compressed_recording (player_number);

	rpi[player_number].frames_recorded = -1;
//	rpi[player_number].frames_allocated = 0;

	rpi[player_number].recording = true;
}



void CONTROL_stop_recording_input (int player_number)
{
/*
	if (rpi[player_number].frames_recorded < rpi[player_number].frames_allocated)
	{
		if (rpi[player_number].frames_recorded > 0)
		{
			// Cut off any unused memory...
			rpi[player_number].data = (individual_player_input *) realloc (rpi[player_number].data , sizeof(individual_player_input) * rpi[player_number].frames_recorded);
			rpi[player_number].frames_allocated = rpi[player_number].frames_recorded;		
		}
	}
*/
	int button;
	realtime_compressed_button_input *rcbi = NULL;

	for (button=0; button<NUM_BUTTONS; button++)
	{
		rcbi = &rpi[player_number].realtime_compressed_button_data[button];

		// Okay, we need to see how much data was created for each button and snip
		// off any stuff which was allocated but not used.

		if (rcbi->new_bool_state_value != NULL)
		{
			// Okay, so space was allocated...

			// Add a state to push the data past the last frame.
			rcbi->current_bool_state++;

			// Snip off the excess if needed.
			if (rcbi->allocated_bool_states > rcbi->current_bool_state)
			{
				rcbi->new_bool_state_value = (unsigned char *) realloc (rcbi->new_bool_state_value, sizeof(unsigned char) * rcbi->current_bool_state);
				rcbi->new_bool_state_length = (int *) realloc (rcbi->new_bool_state_length, sizeof(int) * rcbi->current_bool_state);
				rcbi->allocated_bool_states = rcbi->current_bool_state;
			}
		}

		if (rcbi->new_char_state_value != NULL)
		{
			// Okay, so space was allocated...

			// Add a state to push the data past the last frame.
			rcbi->current_char_state++;

			// Snip off the excess if needed.
			if (rcbi->allocated_char_states > rcbi->current_char_state)
			{
				rcbi->new_char_state_value = (char *) realloc (rcbi->new_char_state_value, sizeof(char) * rcbi->current_char_state);
				rcbi->new_char_state_length = (int *) realloc (rcbi->new_char_state_length, sizeof(int) * rcbi->current_char_state);
				rcbi->allocated_char_states = rcbi->current_char_state;
			}
		}


	}

	rpi[player_number].recording = false;
	rpi[player_number].recorded_but_not_saved = true;
}



void CONTROL_stop_playback (int player_number)
{
	rpi[player_number].playback = false;
}


/*
void CONTROL_destroy_compressed_recording (int player_number)
{
	int button_number;

	for (button_number=0; button_number<MAX_BUTTONS; button_number++)
	{
		rpi[player_number].compressed_button_data[button_number].total_bool_states = 0;
		rpi[player_number].compressed_button_data[button_number].total_char_states = 0;

		if (rpi[player_number].compressed_button_data[button_number].new_bool_state_value != NULL)
		{
			free(rpi[player_number].compressed_button_data[button_number].new_bool_state_value);
			rpi[player_number].compressed_button_data[button_number].new_bool_state_value = NULL;
		}

		if (rpi[player_number].compressed_button_data[button_number].new_bool_state_length != NULL)
		{
			free(rpi[player_number].compressed_button_data[button_number].new_bool_state_length);
			rpi[player_number].compressed_button_data[button_number].new_bool_state_length = NULL;
		}

		if (rpi[player_number].compressed_button_data[button_number].new_char_state_value != NULL)
		{
			free(rpi[player_number].compressed_button_data[button_number].new_char_state_value);
			rpi[player_number].compressed_button_data[button_number].new_char_state_value = NULL;
		}

		if (rpi[player_number].compressed_button_data[button_number].new_char_state_length != NULL)
		{
			free(rpi[player_number].compressed_button_data[button_number].new_char_state_length);
			rpi[player_number].compressed_button_data[button_number].new_char_state_length = NULL;
		}
	}
}
*/

/*
void CONTROL_compress_recording (int player_number)
{
	CONTROL_destroy_compressed_recording (player_number);

	// Okay now we go through it a button at a time, figuring out how many state changes we have to store.

	int button_number;
	int frame_number;
	unsigned char old_bool_state;
	char old_char_state;

	int bool_state_changes;
	int char_state_changes;

	int frames_since_last_bool_state_change;
	int frames_since_last_char_state_change;

	for (button_number=0; button_number<MAX_BUTTONS; button_number++)
	{
		old_bool_state = rpi[player_number].data[0].boolean[button_number];
		old_char_state = rpi[player_number].data[0].power[button_number];

		// We add one more because it won't register the last state change.
		bool_state_changes = 1;
		char_state_changes = 1;

		for (frame_number=0; frame_number<rpi[player_number].frames_recorded; frame_number++)
		{
			if (rpi[player_number].data[frame_number].boolean[button_number] != old_bool_state)
			{
				old_bool_state = rpi[player_number].data[frame_number].boolean[button_number];
				bool_state_changes++;
			}

			if (rpi[player_number].data[frame_number].power[button_number] != old_char_state)
			{
				old_char_state = rpi[player_number].data[frame_number].power[button_number];
				char_state_changes++;
			}		
		}

		rpi[player_number].compressed_button_data[button_number].total_bool_states = bool_state_changes;
		rpi[player_number].compressed_button_data[button_number].total_char_states = char_state_changes;

		// Allocate some space...
		rpi[player_number].compressed_button_data[button_number].new_bool_state_value = (unsigned char *) malloc (sizeof(unsigned char) * bool_state_changes);
		rpi[player_number].compressed_button_data[button_number].new_bool_state_length = (int *) malloc (sizeof(int) * bool_state_changes);

		rpi[player_number].compressed_button_data[button_number].new_char_state_value = (char *) malloc (sizeof(char) * char_state_changes);
		rpi[player_number].compressed_button_data[button_number].new_char_state_length = (int *) malloc (sizeof(int) * char_state_changes);

		// And fill it with *love*. And stuff.
		bool_state_changes = 0;
		char_state_changes = 0;
		frames_since_last_bool_state_change = 0;
		frames_since_last_char_state_change = 0;
		
		old_bool_state = rpi[player_number].data[0].boolean[button_number];
		old_char_state = rpi[player_number].data[0].power[button_number];

		for (frame_number=0; frame_number<rpi[player_number].frames_recorded; frame_number++)
		{
			if (rpi[player_number].data[frame_number].boolean[button_number] != old_bool_state)
			{
				rpi[player_number].compressed_button_data[button_number].new_bool_state_value[bool_state_changes] = old_bool_state;
				rpi[player_number].compressed_button_data[button_number].new_bool_state_length[bool_state_changes] = frames_since_last_bool_state_change;

				old_bool_state = rpi[player_number].data[frame_number].boolean[button_number];
				bool_state_changes++;

				frames_since_last_bool_state_change = 1;
			}
			else
			{
				frames_since_last_bool_state_change++;
			}

			if (rpi[player_number].data[frame_number].power[button_number] != old_char_state)
			{
				rpi[player_number].compressed_button_data[button_number].new_char_state_value[char_state_changes] = old_char_state;
				rpi[player_number].compressed_button_data[button_number].new_char_state_length[char_state_changes] = frames_since_last_char_state_change;

				old_char_state = rpi[player_number].data[frame_number].power[button_number];
				char_state_changes++;

				frames_since_last_char_state_change = 1;
			}
			else
			{
				frames_since_last_char_state_change++;
			}
		}

		// And fill in the last ones with the remaining data...
		rpi[player_number].compressed_button_data[button_number].new_bool_state_value[bool_state_changes] = old_bool_state;
		rpi[player_number].compressed_button_data[button_number].new_bool_state_length[bool_state_changes] = frames_since_last_bool_state_change;

		rpi[player_number].compressed_button_data[button_number].new_char_state_value[char_state_changes] = old_char_state;
		rpi[player_number].compressed_button_data[button_number].new_char_state_length[char_state_changes] = frames_since_last_char_state_change;

		// Righty, that's that. Lovely fresh data that just needs saving. :)
	}
}
*/



int CONTROL_read_variable_data_from_recorded_input (int player_number, int value_id)
{
	if (rpi[player_number].loaded_but_not_played_back == true)
	{
		int i;

		for (i=0; i<rpi[player_number].written_variable_count; i++)
		{
			if (rpi[player_number].written_variable_data[i*2] == value_id)
			{
				return rpi[player_number].written_variable_data[(i*2)+1];
			}
		}

		// Gah! Not found!
		assert(0);
	}
	else
	{
		// Attempting to read data from a stream which you've started playing
		// back probably indicates a programming error, so we'll not allow it.
		// After all, the information you bundled with it should only be used
		// to set up the game before you commence playback.
		assert(0);
	}

	return UNSET;
}



void CONTROL_write_variable_data_to_recorded_input (int player_number, int value_id, int value)
{
	if ((rpi[player_number].start_recording_this_frame == true) || (rpi[player_number].recording == true) || (rpi[player_number].recorded_but_not_saved == true))
	{
		if (rpi[player_number].written_variable_data == NULL)
		{
			rpi[player_number].written_variable_data = (int *) malloc (sizeof(int) * 2);
		}
		else
		{
			rpi[player_number].written_variable_data = (int *) realloc (rpi[player_number].written_variable_data, (rpi[player_number].written_variable_count + 1) * sizeof(int) * 2);
		}

		rpi[player_number].written_variable_data[rpi[player_number].written_variable_count*2] = value_id;
		rpi[player_number].written_variable_data[(rpi[player_number].written_variable_count*2)+1] = value;

		rpi[player_number].written_variable_count++;
	}
	else
	{
		// Attempting to write data to a stream which either isn't recording
		// or hasn't yet been saved is probably a bad thing.
		assert(0);
	}
}



void CONTROL_save_recorded_input (int player_number, char *filename_pointer)
{
	char compress_filename[MAX_LINE_SIZE];

	sprintf (compress_filename,"demos\\%s_%4i.dem",filename_pointer,demo_file_index);
	STRING_replace_char(compress_filename,' ','0');
	fix_filename_slashes(compress_filename);

	if ( (rpi[player_number].recording == false) && (rpi[player_number].playback == false) )
	{
		if (rpi[player_number].frames_recorded > 0)
		{
			// Now lets save the nice compressed versions. So, er, compress it!
//			CONTROL_compress_recording (player_number);

			// And save it! Woo!
			FILE *file_pointer = fopen (MAIN_get_project_filename(compress_filename, true),"wb");

			if (file_pointer != NULL)
			{
				fwrite (&rpi[player_number] , sizeof(recorded_player_input) , 1 , file_pointer);

				int button_number;
/*
				// The processed compressed data
				for (button_number=0; button_number<MAX_BUTTONS; button_number++)
				{
					fwrite (&rpi[player_number].compressed_button_data[button_number].new_bool_state_value[0] , sizeof(unsigned char) * rpi[player_number].compressed_button_data[button_number].total_bool_states, 1 , file_pointer);
					fwrite (&rpi[player_number].compressed_button_data[button_number].new_bool_state_length[0] , sizeof(int) * rpi[player_number].compressed_button_data[button_number].total_bool_states, 1 , file_pointer);

					fwrite (&rpi[player_number].compressed_button_data[button_number].new_char_state_value[0] , sizeof(char) * rpi[player_number].compressed_button_data[button_number].total_char_states, 1 , file_pointer);
					fwrite (&rpi[player_number].compressed_button_data[button_number].new_char_state_length[0] , sizeof(int) * rpi[player_number].compressed_button_data[button_number].total_char_states, 1 , file_pointer);
				}
*/
				// And the realtime compressed data...
				for (button_number=0; button_number<MAX_BUTTONS; button_number++)
				{
					fwrite (&rpi[player_number].realtime_compressed_button_data[button_number].new_bool_state_value[0] , sizeof(unsigned char) * rpi[player_number].realtime_compressed_button_data[button_number].allocated_bool_states, 1 , file_pointer);
					fwrite (&rpi[player_number].realtime_compressed_button_data[button_number].new_bool_state_length[0] , sizeof(int) * rpi[player_number].realtime_compressed_button_data[button_number].allocated_bool_states, 1 , file_pointer);

					fwrite (&rpi[player_number].realtime_compressed_button_data[button_number].new_char_state_value[0] , sizeof(char) * rpi[player_number].realtime_compressed_button_data[button_number].allocated_char_states, 1 , file_pointer);
					fwrite (&rpi[player_number].realtime_compressed_button_data[button_number].new_char_state_length[0] , sizeof(int) * rpi[player_number].realtime_compressed_button_data[button_number].allocated_char_states, 1 , file_pointer);
				}

				if (rpi[player_number].written_variable_count > 0)
				{
					fwrite (&rpi[player_number].written_variable_data[0] , rpi[player_number].written_variable_count * sizeof(int) * 2, 1 , file_pointer);
				}

				if (rpi[player_number].written_flag_count > 0)
				{
					fwrite (&rpi[player_number].written_flag_data[0] , rpi[player_number].written_flag_count * sizeof(int) * 2, 1 , file_pointer);
				}

				if (rpi[player_number].written_array_data_size > 0)
				{
					fwrite (&rpi[player_number].written_array_data[0] , rpi[player_number].written_array_data_size * sizeof(int), 1 , file_pointer);
				}

				fclose(file_pointer);

				// File index ONLY increases after the successful saving of a file.
				demo_file_index++;
				demo_file_index %= 10000;

			}
			else
			{
				// Actually, there's no demo directory, so we ain't saving it. So nerr!
			}
/*
			STRING_replace_word(compress_filename,".cdm",".txt");

			// Now save the text version...
			file_pointer = fopen (MAIN_get_project_filename(compress_filename, true),"w");
			if (file_pointer != NULL)
			{
				int button_number;

				char text_line[MAX_LINE_SIZE];

				for (button_number=0; button_number<MAX_BUTTONS; button_number++)
				{
					int state_number;

					sprintf(text_line,"Realtime Compressed Button %i has %i boolean states.\n",button_number,rpi[player_number].realtime_compressed_button_data[button_number].allocated_bool_states);
					fputs(text_line,file_pointer);
					sprintf(text_line,"Processed Compressed Button %i has %i boolean states.\n\n",button_number,rpi[player_number].compressed_button_data[button_number].total_bool_states);
					fputs(text_line,file_pointer);

					for (state_number=0; state_number<rpi[player_number].realtime_compressed_button_data[button_number].allocated_bool_states; state_number++)
					{
						sprintf(text_line,"Realtime Compressed Button %i State %i Value = %i and Length = %i.\n",button_number, state_number, rpi[player_number].realtime_compressed_button_data[button_number].new_bool_state_value[state_number], rpi[player_number].realtime_compressed_button_data[button_number].new_bool_state_length[state_number]);
						fputs(text_line,file_pointer);
					}

					fputs("\n",file_pointer);

					for (state_number=0; state_number<rpi[player_number].compressed_button_data[button_number].total_bool_states; state_number++)
					{
						sprintf(text_line,"Processed Compressed Button %i State %i Value = %i and Length = %i.\n",button_number, state_number, rpi[player_number].compressed_button_data[button_number].new_bool_state_value[state_number], rpi[player_number].compressed_button_data[button_number].new_bool_state_length[state_number]);
						fputs(text_line,file_pointer);
					}

					fputs("\n",file_pointer);
				}

				fclose(file_pointer);
			}
			else
			{
				assert(0);
			}
*/
			CONTROL_destroy_realtime_compressed_recording (player_number);

			CONTROL_destroy_stored_data (player_number);
//			CONTROL_destroy_compressed_recording (player_number);
		}
		else
		{
			// Trying to save when there's nothing there!
			OUTPUT_message("Trying to save demo when no frames recorded!");
			assert(0);
		}
	}
	else
	{
		// Trying to save recording before it's been stopped or while it's playing!
		OUTPUT_message("Trying to save demo before stopping recording or playing back!");
		assert(0);
	}

//	free (rpi[player_number].data);
//	rpi[player_number].data = NULL;
//	rpi[player_number].frames_allocated = 0;
	rpi[player_number].frames_recorded = 0;
	rpi[player_number].recorded_but_not_saved = false;
}



void CONTROL_stop_and_save_active_channels (char *filename_prefix)
{
	int player_number;
	char filename[MAX_LINE_SIZE];

	for (player_number=0; player_number<MAX_PLAYERS; player_number++)
	{
		if (rpi[player_number].recording)
		{
			CONTROL_stop_recording_input (player_number);
			sprintf(filename,"%s_%i",filename_prefix,player_number);
			CONTROL_save_recorded_input (player_number,filename);
		}
	}
}



void CONTROL_load_compressed_recorded_input_and_inflate (int player_number, char *filename_pointer)
{
	char filename[MAX_LINE_SIZE];

	sprintf (filename,"demos\\%s.dem",filename_pointer);
	fix_filename_slashes(filename);

	FILE *file_pointer = FILE_open_project_case_fallback(filename, "rb");

	if (file_pointer != NULL)
	{
//		CONTROL_destroy_compressed_recording (player_number);
		CONTROL_destroy_realtime_compressed_recording (player_number);

		CONTROL_destroy_stored_data (player_number);

		fread (&rpi[player_number] , sizeof(recorded_player_input) , 1 , file_pointer); /*TODO*/
		
		int button_number;
/*
		for (button_number=0; button_number<MAX_BUTTONS; button_number++)
		{
			rpi[player_number].compressed_button_data[button_number].new_bool_state_value = (unsigned char *) malloc (sizeof(unsigned char) * rpi[player_number].compressed_button_data[button_number].total_bool_states);
			rpi[player_number].compressed_button_data[button_number].new_bool_state_length = (int *) malloc (sizeof(int) * rpi[player_number].compressed_button_data[button_number].total_bool_states);
			
			rpi[player_number].compressed_button_data[button_number].new_char_state_value = (char *) malloc (sizeof(char) * rpi[player_number].compressed_button_data[button_number].total_char_states);
			rpi[player_number].compressed_button_data[button_number].new_char_state_length = (int *) malloc (sizeof(int) * rpi[player_number].compressed_button_data[button_number].total_char_states);

			fread (&rpi[player_number].compressed_button_data[button_number].new_bool_state_value[0] , sizeof(unsigned char) * rpi[player_number].compressed_button_data[button_number].total_bool_states, 1 , file_pointer);
			fread (&rpi[player_number].compressed_button_data[button_number].new_bool_state_length[0] , sizeof(int) * rpi[player_number].compressed_button_data[button_number].total_bool_states, 1 , file_pointer);

			fread (&rpi[player_number].compressed_button_data[button_number].new_char_state_value[0] , sizeof(char) * rpi[player_number].compressed_button_data[button_number].total_char_states, 1 , file_pointer);
			fread (&rpi[player_number].compressed_button_data[button_number].new_char_state_length[0] , sizeof(int) * rpi[player_number].compressed_button_data[button_number].total_char_states, 1 , file_pointer);
		}
*/
		for (button_number=0; button_number<MAX_BUTTONS; button_number++)
		{
			rpi[player_number].realtime_compressed_button_data[button_number].new_bool_state_value = (unsigned char *) malloc (sizeof(unsigned char) * rpi[player_number].realtime_compressed_button_data[button_number].allocated_bool_states);
			rpi[player_number].realtime_compressed_button_data[button_number].new_bool_state_length = (int *) malloc (sizeof(int) * rpi[player_number].realtime_compressed_button_data[button_number].allocated_bool_states);

			rpi[player_number].realtime_compressed_button_data[button_number].new_char_state_value = (char *) malloc (sizeof(char) * rpi[player_number].realtime_compressed_button_data[button_number].allocated_char_states);
			rpi[player_number].realtime_compressed_button_data[button_number].new_char_state_length = (int *) malloc (sizeof(int) * rpi[player_number].realtime_compressed_button_data[button_number].allocated_char_states);
/*TODO*/
			fread (&rpi[player_number].realtime_compressed_button_data[button_number].new_bool_state_value[0] , sizeof(unsigned char) * rpi[player_number].realtime_compressed_button_data[button_number].allocated_bool_states, 1 , file_pointer);
			fread (&rpi[player_number].realtime_compressed_button_data[button_number].new_bool_state_length[0] , sizeof(int) * rpi[player_number].realtime_compressed_button_data[button_number].allocated_bool_states, 1 , file_pointer);

			fread (&rpi[player_number].realtime_compressed_button_data[button_number].new_char_state_value[0] , sizeof(char) * rpi[player_number].realtime_compressed_button_data[button_number].allocated_char_states, 1 , file_pointer);
			fread (&rpi[player_number].realtime_compressed_button_data[button_number].new_char_state_length[0] , sizeof(int) * rpi[player_number].realtime_compressed_button_data[button_number].allocated_char_states, 1 , file_pointer);
		}

		if (rpi[player_number].written_variable_count > 0)
		{
			rpi[player_number].written_variable_data = (int *) malloc (rpi[player_number].written_variable_count * sizeof(int) * 2);

			fread (&rpi[player_number].written_variable_data[0] , rpi[player_number].written_variable_count * sizeof(int) * 2, 1 , file_pointer);
		}
		else
		{
			rpi[player_number].written_variable_data = NULL;
		}

		if (rpi[player_number].written_flag_count > 0)
		{
			rpi[player_number].written_flag_data = (int *) malloc (rpi[player_number].written_flag_count * sizeof(int) * 2);

			fread (&rpi[player_number].written_flag_data[0] , rpi[player_number].written_flag_count * sizeof(int) * 2, 1 , file_pointer);
		}
		else
		{
			rpi[player_number].written_flag_data = NULL;
		}

		if (rpi[player_number].written_array_data_size > 0)
		{
			rpi[player_number].written_array_data = (int *) malloc (rpi[player_number].written_array_data_size * sizeof(int));

			fread (&rpi[player_number].written_array_data[0] , rpi[player_number].written_array_data_size * sizeof(int), 1 , file_pointer);
		}
		else
		{
			rpi[player_number].written_array_data = NULL;
		}

		rpi[player_number].loaded_but_not_played_back = true;

		fclose(file_pointer);
/*
		// Now that it's loaded, we have to inflate it! Lawks!

		// First of all check that all the totals tally.

		int old_length = UNSET;
		int current_length;
		int state_index;
		bool error_occured = false;

		for (button_number=0; button_number<MAX_BUTTONS; button_number++)
		{
			current_length = 0;

			for (state_index=0; state_index<rpi[player_number].compressed_button_data[button_number].total_bool_states; state_index++)
			{
				current_length += rpi[player_number].compressed_button_data[button_number].new_bool_state_length[state_index];
			}

			if (old_length != UNSET)
			{
				if (old_length != current_length)
				{
					error_occured = true;
				}
			}
			else
			{
				old_length = current_length;
			}
			
			current_length = 0;

			for (state_index=0; state_index<rpi[player_number].compressed_button_data[button_number].total_char_states; state_index++)
			{
				current_length += rpi[player_number].compressed_button_data[button_number].new_char_state_length[state_index];
			}

			if (old_length != current_length)
			{
				error_occured = true;
			}
		}

		if ((error_occured == true) || (current_length != rpi[player_number].frames_recorded))
		{
			// Crap. Crappity, crappity, crap.
			assert(0);
		}
		else
		{
			// Okay, so that cleared nicely. Let's inflate it!

			int frame_index;
			int i;

			rpi[player_number].data = (individual_player_input *) malloc (sizeof(individual_player_input) * rpi[player_number].frames_allocated );

			for (button_number=0; button_number<MAX_BUTTONS; button_number++)
			{
				frame_index = 0;

				for (state_index=0; state_index<rpi[player_number].compressed_button_data[button_number].total_bool_states; state_index++)
				{
					current_length = rpi[player_number].compressed_button_data[button_number].new_bool_state_length[state_index];

					for (i=0; i<current_length; i++)
					{
						rpi[player_number].data[frame_index].boolean[button_number] = rpi[player_number].compressed_button_data[button_number].new_bool_state_value[state_index];
						frame_index++;
					}
				}

				frame_index = 0;

				for (state_index=0; state_index<rpi[player_number].compressed_button_data[button_number].total_char_states; state_index++)
				{
					current_length = rpi[player_number].compressed_button_data[button_number].new_char_state_length[state_index];

					for (i=0; i<current_length; i++)
					{
						rpi[player_number].data[frame_index].power[button_number] = rpi[player_number].compressed_button_data[button_number].new_char_state_value[state_index];
						frame_index++;
					}
				}

			}
		}
*/
	}
	else
	{
		assert(0);
	}
}


/*
void CONTROL_load_recorded_input (int player_number, char *filename_pointer)
{
	char filename[MAX_LINE_SIZE];

	sprintf (filename,"%s.dem",filename_pointer);

	FILE *file_pointer = FILE_open_project_case_fallback(filename, "rb");

	if (file_pointer != NULL)
	{
		fread (&rpi[player_number] , sizeof(recorded_player_input) , 1 , file_pointer);
		
		rpi[player_number].data = (individual_player_input *) malloc (sizeof(individual_player_input) * rpi[player_number].frames_allocated );
		
		fread (&rpi[player_number].data[0] , sizeof(individual_player_input) * rpi[player_number].frames_allocated , 1 , file_pointer);

		fclose(file_pointer);
	}
	else
	{
		assert(0);
	}
}
*/


int CONTROL_get_recording_playback_status (int player_number)
{
	if (rpi[player_number].playback)
	{
		return STATUS_PLAYBACK;
	}
	else if (rpi[player_number].recording)
	{
		return STATUS_RECORDING;
	}
	else
	{
		return STATUS_NOTHING;
	}
}



void CONTROL_play_recorded_input (int player_number)
{
	rpi[player_number].playback = true;
	rpi[player_number].currently_playing = -1;
}



void CONTROL_setup_recording_bays (void)
{
	int player_number;
	int button_number;

	for (player_number=0; player_number<MAX_PLAYERS; player_number++)
	{
		rpi[player_number].frames_recorded = 0;
//		rpi[player_number].frames_allocated = 0;
//		rpi[player_number].data = NULL;
		rpi[player_number].recording = false;
		rpi[player_number].playback = false;
		rpi[player_number].recorded_but_not_saved = false;
		rpi[player_number].loaded_but_not_played_back = false;
		rpi[player_number].currently_playing = 0;
		rpi[player_number].start_playback_this_frame = false;
		rpi[player_number].start_recording_this_frame = false;

		rpi[player_number].written_variable_count = 0;
		rpi[player_number].written_variable_data = NULL;
		
		rpi[player_number].written_array_data_size = 0;
		rpi[player_number].written_array_data = NULL;

		rpi[player_number].written_flag_count = 0;
		rpi[player_number].written_flag_data = NULL;

		for (button_number=0; button_number<MAX_BUTTONS; button_number++)
		{
//			rpi[player_number].compressed_button_data[button_number].total_bool_states = 0;
//			rpi[player_number].compressed_button_data[button_number].new_bool_state_value = NULL;
//			rpi[player_number].compressed_button_data[button_number].new_bool_state_length = NULL;
//			rpi[player_number].compressed_button_data[button_number].total_char_states = 0;
//			rpi[player_number].compressed_button_data[button_number].new_char_state_value = NULL;
//			rpi[player_number].compressed_button_data[button_number].new_char_state_length = NULL;

			rpi[player_number].realtime_compressed_button_data[button_number].allocated_bool_states = 0;
			rpi[player_number].realtime_compressed_button_data[button_number].current_bool_state = 0;
			rpi[player_number].realtime_compressed_button_data[button_number].new_bool_state_value = NULL;
			rpi[player_number].realtime_compressed_button_data[button_number].new_bool_state_length = NULL;
			rpi[player_number].realtime_compressed_button_data[button_number].allocated_char_states = 0;
			rpi[player_number].realtime_compressed_button_data[button_number].current_char_state = 0;
			rpi[player_number].realtime_compressed_button_data[button_number].new_char_state_value = NULL;
			rpi[player_number].realtime_compressed_button_data[button_number].new_char_state_length = NULL;

		}
	}
}



void CONTROL_dump_realtime_compressed_button_data (int player_number)
{
	int button;
	realtime_compressed_button_input *rcbi = NULL;

	for (button=0; button<NUM_BUTTONS; button++)
	{
		rcbi = &rpi[player_number].realtime_compressed_button_data[button];

		// We need to check to see if the new bool data is different to the old bool data.
		// But of course, we don't even know if there'a any existant bool data yet.

		if (rcbi->new_bool_state_value == NULL)
		{
			// Allocate some space!
			rcbi->new_bool_state_value = (unsigned char *) malloc (sizeof(unsigned char) * COMPRESSED_RECORDING_ALLOCATION_CHUNKS);
			rcbi->new_bool_state_length = (int *) malloc (sizeof(int) * COMPRESSED_RECORDING_ALLOCATION_CHUNKS);

			rcbi->allocated_bool_states = COMPRESSED_RECORDING_ALLOCATION_CHUNKS;
			rcbi->current_bool_state = 0;

			// We KNOW it'll be the same, and so we just dump it in there...

			rcbi->new_bool_state_value[rcbi->current_bool_state] = rpi[player_number].current_frame_data.boolean[button];
			rcbi->new_bool_state_length[rcbi->current_bool_state] = 1;
		}
		else
		{
			// Okay, we have some data in there, so check to see if the new value is different to the old one.

			if (rcbi->new_bool_state_value[rcbi->current_bool_state] != rpi[player_number].current_frame_data.boolean[button])
			{
				// It's different. Lawks. So we need to increase the instruction counter...

				rcbi->current_bool_state++;

				if (rcbi->current_bool_state == rcbi->allocated_bool_states)
				{
					// Uh-oh! We've run out of room! Allocate more! MORE! MOOOOORE!
					
					rcbi->new_bool_state_value = (unsigned char *) realloc (rcbi->new_bool_state_value, sizeof(unsigned char) * (rcbi->allocated_bool_states + COMPRESSED_RECORDING_ALLOCATION_CHUNKS));
					rcbi->new_bool_state_length = (int *) realloc (rcbi->new_bool_state_length, sizeof(int) * (rcbi->allocated_bool_states + COMPRESSED_RECORDING_ALLOCATION_CHUNKS));

					rcbi->allocated_bool_states += COMPRESSED_RECORDING_ALLOCATION_CHUNKS;
				}

				// And then store the new value...
				
				rcbi->new_bool_state_value[rcbi->current_bool_state] = rpi[player_number].current_frame_data.boolean[button];
				rcbi->new_bool_state_length[rcbi->current_bool_state] = 1;
			}
			else
			{
				// It's the same! Woot! Just increase the counter, why doncher? :)

				rcbi->new_bool_state_length[rcbi->current_bool_state]++;
			}
		}

		// We need to check to see if the new power data is different to the old power data.
		// But of course, we don't even know if there'a any existant power data yet.

		if (rcbi->new_char_state_value == NULL)
		{
			// Allocate some space!
			rcbi->new_char_state_value = (char *) malloc (sizeof(char) * COMPRESSED_RECORDING_ALLOCATION_CHUNKS);
			rcbi->new_char_state_length = (int *) malloc (sizeof(int) * COMPRESSED_RECORDING_ALLOCATION_CHUNKS);

			rcbi->allocated_char_states = COMPRESSED_RECORDING_ALLOCATION_CHUNKS;
			rcbi->current_char_state = 0;

			// We KNOW it'll be the same, and so we just dump it in there...

			rcbi->new_char_state_value[rcbi->current_char_state] = rpi[player_number].current_frame_data.power[button];
			rcbi->new_char_state_length[rcbi->current_char_state] = 1;
		}
		else
		{
			// Okay, we have some data in there, so check to see if the new value is different to the old one.

			if (rcbi->new_char_state_value[rcbi->current_char_state] != rpi[player_number].current_frame_data.power[button])
			{
				// It's different. Lawks. So we need to increase the instruction counter...

				rcbi->current_char_state++;

				if (rcbi->current_char_state == rcbi->allocated_char_states)
				{
					// Uh-oh! We've run out of room! Allocate more! MORE! MOOOOORE!
					
					rcbi->new_char_state_value = (char *) realloc (rcbi->new_char_state_value, sizeof(unsigned char) * (rcbi->allocated_char_states + COMPRESSED_RECORDING_ALLOCATION_CHUNKS));
					rcbi->new_char_state_length = (int *) realloc (rcbi->new_char_state_length, sizeof(int) * (rcbi->allocated_char_states + COMPRESSED_RECORDING_ALLOCATION_CHUNKS));

					rcbi->allocated_char_states += COMPRESSED_RECORDING_ALLOCATION_CHUNKS;
				}

				// And then store the new value...
				
				rcbi->new_char_state_value[rcbi->current_char_state] = rpi[player_number].current_frame_data.power[button];
				rcbi->new_char_state_length[rcbi->current_char_state] = 1;
			}
			else
			{
				// It's the same! Woot! Just increase the counter, why doncher? :)

				rcbi->new_char_state_length[rcbi->current_char_state]++;
			}
		}
	}
}



void CONTROL_manage_recording (void)
{
	int player_number;
	int button;

	for (player_number=0; player_number<MAX_PLAYERS; player_number++)
	{
		// This is done so that everything is properly syncronised so that whenever in a frame the script
		// instruction to record/playback input occurs the actual start will always be inbetween frames.

		if (rpi[player_number].start_recording_this_frame)
		{
			rpi[player_number].start_recording_this_frame = false;
			CONTROL_start_recording_input (player_number);
		}

		if (rpi[player_number].recording)
		{
			rpi[player_number].frames_recorded++;

			if (rpi[player_number].frames_recorded > 0)
			{
				// We aren't on the first frame and so have usable data in the current_frame_data.
				CONTROL_dump_realtime_compressed_button_data (player_number);
			}

			// Allocate more space (if needed)...
//			CONTROL_allocate_recording_space (player_number);

			// Reset the input...
			for (button=0; button<NUM_BUTTONS; button++)
			{
//				rpi[player_number].data[rpi[player_number].frames_recorded].boolean[button] = 0;
//				rpi[player_number].data[rpi[player_number].frames_recorded].power[button] = 0;

				rpi[player_number].current_frame_data.boolean[button] = 0;
				rpi[player_number].current_frame_data.power[button] = 0;
			}
		}

		if (rpi[player_number].start_playback_this_frame)
		{
			rpi[player_number].start_playback_this_frame = false;
			CONTROL_play_recorded_input (player_number);
		}

		if (rpi[player_number].playback)
		{
			rpi[player_number].currently_playing++;

			if (rpi[player_number].currently_playing == 0)
			{
				for (button=0; button<NUM_BUTTONS; button++)
				{
					rpi[player_number].current_bool_section_number[button] = 0;
					rpi[player_number].current_bool_section_position[button] = 0;
					
					rpi[player_number].current_char_section_number[button] = 0;
					rpi[player_number].current_char_section_position[button] = 0;
				}
			}
			else
			{
				for (button=0; button<NUM_BUTTONS; button++)
				{
					rpi[player_number].current_bool_section_position[button]++;

					if (rpi[player_number].current_bool_section_position[button] == rpi[player_number].realtime_compressed_button_data[button].new_bool_state_length[rpi[player_number].current_bool_section_number[button]])
					{
						rpi[player_number].current_bool_section_number[button]++;
						rpi[player_number].current_bool_section_position[button] = 0;
					}

					rpi[player_number].current_char_section_position[button]++;

					if (rpi[player_number].current_char_section_position[button] == rpi[player_number].realtime_compressed_button_data[button].new_char_state_length[rpi[player_number].current_char_section_number[button]])
					{
						rpi[player_number].current_char_section_number[button]++;
						rpi[player_number].current_char_section_position[button] = 0;
					}
				}
			}

			for (button=0; button<NUM_BUTTONS; button++)
			{
				rpi[player_number].current_frame_data.boolean[button] = rpi[player_number].realtime_compressed_button_data[button].new_bool_state_value[rpi[player_number].current_bool_section_number[button]];
				rpi[player_number].current_frame_data.power[button] = rpi[player_number].realtime_compressed_button_data[button].new_char_state_value[rpi[player_number].current_char_section_number[button]];
			}
		}
	}
}



// ---------------- EVERY FRAME UPDATES ----------------



void CONTROL_update_all_input (void)
{
	if (keyboard_setup_okay == true)
	{
		CONTROL_update_keyboard();
	}

	if (joystick_setup_okay == true)
	{
		CONTROL_update_joypads();
	}
	
	if (mouse_setup_okay == true)
	{
		CONTROL_update_mouse();
	}

	CONTROL_manage_recording ();

	frame_counter++;
}



// ---------------- SETUP ROUTINES ----------------



void CONTROL_get_default_stick_positions (void)
{

	// This goes through all the sticks attached to all the joypads on the system and gets their default positions
	// so that we know where the deadzone effectively is. Also stores it's upper and lower limits.

	int port;
	int stick;
	int axis;
	int result;

	int lower,upper;

	for (port=0; port<PLATFORM_INPUT_num_joysticks(); port++)
	{
		for (stick=0; stick<PLATFORM_INPUT_joystick_num_sticks(port) ; stick++)
		{
			if (PLATFORM_INPUT_joystick_stick_is_signed(port,stick))
			{
				lower = -128;
				upper = 128;
			}
			else
			{
				lower = 0;
				upper = 255;
			}

			for (axis=0; axis<PLATFORM_INPUT_joystick_stick_num_axes(port,stick) ; axis++)
			{
				result = PLATFORM_INPUT_joystick_axis_pos(port,stick,axis);
				result = MATH_round_to_nearest ( result, 128 );
				default_axis_value [port][stick][axis].default_position = result;

				default_axis_value [port][stick][axis].lower_position = lower;
				default_axis_value [port][stick][axis].upper_position = upper;
			}
		}
	}

	// After that we check the flags of each of the sticks to see if they're throttle type
	// or stick type and therefor whether they go in one direction or two directions.

}



void CONTROL_blank_keyboard_input (void)
{
	int t;

	for (t=0; t<MAX_KEYS; t++)
	{
		key_store[t].analogue_offset = 0;
		key_store[t].current_state = 0;
		key_store[t].old_state = 0;
		key_store[t].current_position = 0;
		key_store[t].old_position = 0;
		key_store[t].this_hit = 0;
		key_store[t].prev_hit = 0;
		key_store[t].held_time = 0;
		key_store[t].locked = false;
	}
}



void CONTROL_blank_joypad_input (void)
{
	int port;
	int button;

	for (port=0; port<PLATFORM_INPUT_num_joysticks(); port++)
	{
		for (button=0; button<PLATFORM_INPUT_joystick_num_buttons(port); button++)
		{
			pad_store[port][button].analogue_offset = 0;
			pad_store[port][button].current_state = 0;
			pad_store[port][button].old_state = 0;
			pad_store[port][button].current_position = 0;
			pad_store[port][button].old_position = 0;
			pad_store[port][button].this_hit = 0;
			pad_store[port][button].prev_hit = 0;
			pad_store[port][button].held_time = 0;
			pad_store[port][button].locked = false;
		}
	}

	int stick;
	int axis;
	int side;

	for (port=0; port<PLATFORM_INPUT_num_joysticks(); port++)
	{
		for (stick=0; stick<PLATFORM_INPUT_joystick_num_sticks(port) ; stick++)
		{
			for (axis=0; axis<PLATFORM_INPUT_joystick_stick_num_axes(port,stick) ; axis++)
			{
				for (side=0; side< MAX_AXIS_STATES; side++)
				{
					stick_store[port][stick][axis][side].analogue_offset = 0;
					stick_store[port][stick][axis][side].current_state = 0;
					stick_store[port][stick][axis][side].old_state = 0;
					stick_store[port][stick][axis][side].current_position = 0;
					stick_store[port][stick][axis][side].old_position = 0;
					stick_store[port][stick][axis][side].this_hit = 0;
					stick_store[port][stick][axis][side].prev_hit = 0;
					stick_store[port][stick][axis][side].held_time = 0;
					stick_store[port][stick][axis][side].locked = false;
				}
			}
		}
	}

}



void CONTROL_blank_mouse_input (void)
{
	int t,i;

	for (t=0; t<MOUSE_AXIS; t++)
	{
		mouse_position_store[t].current_state = 0;
		mouse_position_store[t].old_state = 0;
		mouse_position_store[t].current_position = 0;
		mouse_position_store[t].old_position = 0;
		mouse_position_store[t].this_hit = 0;
		mouse_position_store[t].prev_hit = 0;
		mouse_position_store[t].held_time = 0;
		mouse_position_store[t].locked = false;

		mouse_sampling_size[t] = 1;

		for (i=0; i<MAX_MOUSE_BUFFER_SIZE; i++)
		{
			mouse_buffer[t][i] = 0;
		}
	}

	for (t=0; t<MOUSE_BUTTONS; t++)
	{
		mouse_button_store[t].current_state = 0;
		mouse_button_store[t].old_state = 0;
		mouse_button_store[t].current_position = 0;
		mouse_button_store[t].old_position = 0;
		mouse_button_store[t].this_hit = 0;
		mouse_button_store[t].prev_hit = 0;
		mouse_button_store[t].held_time = 0;
		mouse_button_store[t].locked = false;
	}
}



void CONTROL_set_dupe_check_value (int player_number, int control_number, bool boolean_value)
{
	controls[player_number][control_number].dupe_check = boolean_value;
}



void CONTROL_blank_player_input(void)
{
	int player;
	int control;

	for (player=0 ; player<MAX_PLAYERS ; player++)
	{
		for (control=0 ; control < NUM_BUTTONS ; control++)
		{
			controls[player][control].device = UNSET;
			controls[player][control].button = UNSET;
			controls[player][control].port = UNSET;
			controls[player][control].stick = UNSET;
			controls[player][control].axis = UNSET;
			controls[player][control].dupe_check = false;
		}
	}
}



void CONTROL_define_control_keyboard (int player_number, int control_number, int input_key)
{
	controls[player_number][control_number].device = DEVICE_KEYBOARD;
	controls[player_number][control_number].button = input_key;
}



void CONTROL_get_input (int *device_type, int *device_button)
{
	*device_type = 0;
	*device_button = 0;
}



int CONTROL_setup_joypad(void)
{
	const char *msg;
	char error_string[256];

	if (PLATFORM_INPUT_install_joystick() != 0)
	{
		joystick_setup_okay = false;
		set_gfx_mode(GFX_TEXT, 0, 0, 0, 0);
		sprintf(error_string , "Error initialising joystick\n%s\n", allegro_error);
		OUTPUT_message(error_string);
		return 1;
	}
	else
	{
		joystick_setup_okay = true;
	}

	if (!PLATFORM_INPUT_num_joysticks())
	{
//		set_gfx_mode(GFX_TEXT, 0, 0, 0, 0);
//		OUTPUT_message("Error: joystick not found\n");
//		return 1;
	}

	while (PLATFORM_INPUT_joystick_needs_calibration(0))
	{
		msg = PLATFORM_INPUT_calibrate_joystick_name(0);

//		clear_bitmap(screen);

//		textout_centre(screen, font, msg, SCREEN_W/2, 64, palette_color[255]);
//		textout_centre(screen, font, "and press a key.", SCREEN_W/2, 80, palette_color[255]);

		if ((readkey()&0xFF) == 27)
			return 0;

		if (PLATFORM_INPUT_calibrate_joystick(0) != 0)
		{
			set_gfx_mode(GFX_TEXT, 0, 0, 0, 0);
			OUTPUT_message("Error calibrating joystick!\n");
			return 1;
		}
	}
	
	CONTROL_get_default_stick_positions();
	CONTROL_blank_joypad_input();

	return 0;
}



int CONTROL_setup_mouse(void)
{
	char error_string[256];

	if (install_mouse() == -1)
	{
		mouse_setup_okay = false;
		set_gfx_mode(GFX_TEXT, 0, 0, 0, 0);
		sprintf(error_string , "Error initialising mouse!\n");
		return 1;
	}
	else
	{
		mouse_setup_okay = true;
	}

	CONTROL_blank_mouse_input();

	return 0;
}



int CONTROL_setup_keyboard(void)
{
	char error_string[256];

	if (install_keyboard() != 0)
	{
		keyboard_setup_okay = false;
		set_gfx_mode(GFX_TEXT, 0, 0, 0, 0);
		sprintf(error_string , "Error initialising keyboard!\n");
		return 1;
	}
	else
	{
		keyboard_setup_okay = true;
	}

	CONTROL_blank_keyboard_input();

	return 0;
}



void CONTROL_setup_everything(void)
{
	CONTROL_setup_keyboard();
	CONTROL_setup_mouse();
	CONTROL_setup_joypad();
	CONTROL_blank_player_input();
	CONTROL_setup_recording_bays();
}



bool CONTROL_check_any_joypad_control_hit (int player_number)
{
	int button_number;

	for (button_number=0; button_number<NUM_BUTTONS; button_number++)
	{
		if ((controls[button_number][player_number].device > DEVICE_KEYBOARD) && (controls[button_number][player_number].device < DEVICE_MOUSE_BUTTONS))
		{
			if (CONTROL_player_button_hit(player_number,button_number) == true)
			{
				return true;
			}
		}
	}

	return false;
}



int CONTROL_get_first_keyboard_hit (void)
{
	// Returns the scancode of the first keyboard key that was hit this frame, excluding modifiers.

	int key_num;

	for (key_num=1; key_num<KEY_MODIFIERS; key_num++)
	{
		if (CONTROL_key_hit (key_num))
		{
			return key_num;
		}
	}

	return UNSET;
}



int CONTROL_get_ascii_for_scancode (int scancode, bool uppercase)
{
	// Returns the ascii code for the given scancode, if there is one...
	// Otherwise returns UNSET (-1).

	bool shifted;

	if ( (CONTROL_key_down(KEY_LSHIFT)) || (CONTROL_key_down(KEY_RSHIFT)) )
	{
		shifted = true;
	}
	else
	{
		shifted = false;
	}

	char key_c = '\0';

	if (scancode<MAX_KEYS)
	{
		if (uppercase)
		{
			if (shifted)
			{
				key_c = upper_character_array[scancode];
			}
			else
			{
				key_c = capitalised_character_array[scancode];
			}
		}
		else
		{
			if (shifted)
			{
				key_c = upper_character_array[scancode];
			}
			else
			{
				key_c = lower_character_array[scancode];
			}
		}
	}

	if (key_c == '\0')
	{
		return UNSET;
	}
	else
	{
		return int(key_c);
	}
}



int CONTROL_get_scancode_for_ascii (int ascii)
{
	int scancode = 0;

	return scancode;
}
