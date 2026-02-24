// This will be a collection of simple gui primitives where you can create and destroy buttons, or temporarily
// disable buttons or create series of buttons where activating one deactivates the others and all sorts. Basically
// get rid of all that crappy clicking in a rectangle guff from the editor code.

// For simplicity each button will be assigned it's own graphic buffer pointer so you have to have everything
// in memory already. You can opt to not draw anything if you want, however, and merely use the button as a sort
// of overlay for a already drawn image.

// Okay, here are the button types and properties (which would be in addition to the standard properties of
// GROUP_ID, BUTTON_ID, POSITION_STATUS, LIFE_STATUS, ON_IMAGE_BUFFER, OFF_OVERLAY_IMAGE_BUFFER, INT_POINTER, X, Y, WIDTH, HEIGHT, FLIP_OFF_OVERLAY, INVERT_ON_OFF_OVERLAY, CLICK_STATUS

// Toggle - clicking on this changes the INT_POINTER'd value from 0 to 1 and vice versa.

// Binary Toggle - clicking on this XORs the INT_POINTER'd value by the BINARY_VALUE given then sets the button status according to
//     INT_POINTER xor BINARY_VALUE.

// Radio Set To Value - clicking on this sets the INT_POINTER'd value to the given SET_VALUE and sets the POSITION_STATUS to ON, then searches
//     through all the other buttons looking for those with the same GROUP_ID and sets their POSITION_STATUS to OFF. Useful for TABS.

// Horizontal Value Ramp - you specify a START and END value and a MULTIPLE value and it'll store a value
//     between the START and END depending on where you are clicking in the window, rounded to the nearest MULTIPLE.

// Vertical Value Ramp - As above, but vertically. Create two of them over the top of one another to make a grid
//     selecter.

// Integer Nudge Value - You pass this a pointer to an integer variable upon creation and a START, END, WRAP and NUDGE_BY and NUDGE_TO_MULTIPLE
//     and it will increase that integer by NUDGE_BY or to the nearest multiple or NUDGE_TO_MULTIPLE, and if WRAP is set then when it goes over
//     CEILING it'll be set to FLOOR and vice versa. If WRAP isn't set it'll just cap the value instead. Comes in a FLOAT flavour, too.

// Integer Nudge Display - You pass this a pointer to a variable and it'll display the value of it every time the draw
//     gui functions are called. Peachy.

// Horizontal Drag Bar - This uses rectangle primitives to create a drag bar of the specified size with a proportionally sized
//     no less than MIN_BAR_SIZE in length using the given colour values.

// Vertical Drag Bar - See above.

// When a button is created it's allocated a free space in the button array and it's property's reset. All buttons have a GROUP_ID and a BUTTON_ID
// so you can either kill vast swathes of buttons or just single ones. If you will only be dealing with buttons by group then you can use a BUTTON_ID
// of UNSET (which is -1) for ease.

// When a button's status is set to OFF it will be drawn with the OFF_OVERLAY_IMAGE_BUFFER over it, and if FLIP_OFF_OVERLAY is set then it will be
// horizontally flipped every frame so that if it's a stipple if gives a transparency effect.

// Items which are likely to be common to groups of buttons rather than individual button will be set via commands which act on
// GROUP_IDs. This is so you aren't passing ridiculous numbers of parameters when setting up buttons.

#include <stdio.h>
#include <assert.h>

#include "simple_gui.h"
#include "control.h"
#include "math_stuff.h"
#include "output.h"
#include "global_param_list.h"

#include "fortify.h"



typedef struct {
	int button_type;
	int group_id;
	int button_id;
	int position_status;
	int life_status;
	int bitmap_number;
	int frame_number;
	int overlay_number;
	bool h_flip_overlay; // Draws the overlay flipped horizontally every frame is set to true 
	bool invert_overlay; // Changes it so overlay is drawn when button is ON instead of OFF
	bool flicker_overlay; // Alternates whether the overlay is drawn each frame is set to true
	int *i_pointer;
	int x,y;
	int width,height;
	bool click_status;
	int xor_value;
	int set_value;
	int start,end,multiple;
	bool half_over; // If this is set then the value on a ramp will be from START to END instead of START to END-1.
	int nudge_by,nudge_repeat_delay,nudge_repeat_frequency;
	bool wrap;
	int drag_bar_size;
	int drag_bar_equivalent;
	int counter;
	int main_bar_colour,scroll_bar_colour;
	bool grabbed;
	int grabbed_at;
	int mouse_button;
	int drag_bar_position;
	int grabbed_value;
	bool rounded_up;
	int full_bar_colour[3];
	int empty_bar_colour[3];
	bool draw_bar;
} button_struct;

button_struct bs[MAX_GUI_BUTTONS];

#define GPL_LIST_DRAG_BAR_SIZE		(32)




void SIMPGUI_reset_button (int b)
{
	// This deletes everything and flushes all the turds out of the system. :)

	int i;

	bs[b].button_id = UNSET;
	bs[b].group_id = UNSET;
	bs[b].position_status = BUTTON_OFF;
	bs[b].life_status = BUTTON_DEAD;
	bs[b].bitmap_number = UNSET;
	bs[b].frame_number = UNSET;
	bs[b].overlay_number = UNSET;
	bs[b].h_flip_overlay = false;
	bs[b].invert_overlay = false;
	bs[b].flicker_overlay = false;
	bs[b].i_pointer = NULL;
	bs[b].x = 0;
	bs[b].y = 0;
	bs[b].width = 0;
	bs[b].height = 0;
	bs[b].click_status = false;
	bs[b].xor_value = 0;
	bs[b].set_value = 0;
	bs[b].start = 0;
	bs[b].end = 0;
	bs[b].multiple = 0;
	bs[b].half_over = false;
	bs[b].nudge_by = 0;
	bs[b].nudge_repeat_delay = DEFAULT_MOUSE_REPEAT_DELAY;
	bs[b].nudge_repeat_frequency = DEFAULT_MOUSE_REPEAT_FREQENCY;
	bs[b].wrap = false;
	bs[b].drag_bar_size = 0;
	bs[b].drag_bar_equivalent = 0;
	bs[b].counter = 0;
	bs[b].main_bar_colour = 0;
	bs[b].scroll_bar_colour = 0;
	for (i=0; i<3; i++)
	{
		bs[b].full_bar_colour[i] = 0;
		bs[b].empty_bar_colour[i] = 0;
	}
	bs[b].grabbed = false;
	bs[b].grabbed_at = 0;
	bs[b].mouse_button = 0;
	bs[b].drag_bar_position = 0;
	bs[b].grabbed_value = 0;
	bs[b].rounded_up = false;
	bs[b].draw_bar = false;
}



int SIMPGUI_next_free_button (void)
{

	int b;

	for (b=0 ; b<MAX_GUI_BUTTONS ; b++)
	{
		if (bs[b].life_status == BUTTON_DEAD)
		{
			return b;
		}
	}

	return -1;

}



void SIMPGUI_create_toggle_button (int group_id, int button_id, int *integer_value, int x, int y, int bitmap_number, int sprite_number, int mouse_button)
{
	int b;

	b = SIMPGUI_next_free_button();

	if (b > -1)
	{
		bs[b].life_status = BUTTON_ALIVE;
		bs[b].button_type = BUTTON_TYPE_TOGGLE;
		bs[b].group_id = group_id;
		bs[b].button_id = button_id;
		bs[b].position_status = (*integer_value > 0 ? BUTTON_ON : BUTTON_OFF);
		bs[b].i_pointer = integer_value;
		bs[b].bitmap_number = bitmap_number;
		bs[b].frame_number = sprite_number;
		bs[b].overlay_number = sprite_number;
		bs[b].x = x;
		bs[b].y = y;
		bs[b].width = OUTPUT_sprite_width (bitmap_number,sprite_number);
		bs[b].height = OUTPUT_sprite_height (bitmap_number,sprite_number);
		bs[b].mouse_button = mouse_button;
		bs[b].h_flip_overlay = true;
	}
	else
	{
		OUTPUT_message("Could not create button! You ain't been cleaning up behind you, have you?!");
		assert(0);
	}
}



void SIMPGUI_create_radio_set_button (int group_id, int button_id, int *integer_value, int x, int y, int bitmap_number, int sprite_number, int mouse_button, int set_value)
{
	int b;

	b = SIMPGUI_next_free_button();

	if (b > -1)
	{
		bs[b].life_status = BUTTON_ALIVE;
		bs[b].button_type = BUTTON_TYPE_RADIO_SET;
		bs[b].set_value = set_value;
		bs[b].group_id = group_id;
		bs[b].button_id = button_id;
		bs[b].position_status = (*integer_value == set_value ? BUTTON_ON : BUTTON_OFF);
		bs[b].i_pointer = integer_value;
		bs[b].bitmap_number = bitmap_number;
		bs[b].frame_number = sprite_number;
		bs[b].overlay_number = sprite_number;
		bs[b].x = x;
		bs[b].y = y;
		bs[b].width = OUTPUT_sprite_width (bitmap_number,sprite_number);
		bs[b].height = OUTPUT_sprite_height (bitmap_number,sprite_number);
		bs[b].mouse_button = mouse_button;
		bs[b].h_flip_overlay = true;
	}
	else
	{
		OUTPUT_message("Could not create button! You ain't been cleaning up behind you, have you?!");
		assert(0);
	}
}



void SIMPGUI_create_set_button (int group_id, int button_id, int *integer_value, int x, int y, int bitmap_number, int sprite_number, int mouse_button, int set_value)
{
	int b;

	b = SIMPGUI_next_free_button();

	if (b > -1)
	{
		bs[b].life_status = BUTTON_ALIVE;
		bs[b].button_type = BUTTON_TYPE_SET;
		bs[b].set_value = set_value;
		bs[b].group_id = group_id;
		bs[b].button_id = button_id;
		bs[b].position_status = (*integer_value == set_value ? BUTTON_ON : BUTTON_OFF);
		bs[b].i_pointer = integer_value;
		bs[b].bitmap_number = bitmap_number;
		bs[b].frame_number = sprite_number;
		bs[b].overlay_number = sprite_number;
		bs[b].x = x;
		bs[b].y = y;
		bs[b].width = OUTPUT_sprite_width (bitmap_number,sprite_number);
		bs[b].height = OUTPUT_sprite_height (bitmap_number,sprite_number);
		bs[b].mouse_button = mouse_button;
		bs[b].h_flip_overlay = true;
	}
	else
	{
		OUTPUT_message("Could not create button! You ain't been cleaning up behind you, have you?!");
		assert(0);
	}
}



void SIMPGUI_create_h_ramp_button (int group_id, int button_id, int *integer_value, int x, int y, int width, int height , int start, int end, int multiple, int mouse_button , bool rounded_up , bool draw_bar)
{
	int b;

	b = SIMPGUI_next_free_button();

	if (b > -1)
	{
		bs[b].life_status = BUTTON_ALIVE;
		bs[b].rounded_up = rounded_up;
		bs[b].button_type = BUTTON_TYPE_H_RAMP;
		bs[b].start = start;
		bs[b].end = end;
		bs[b].multiple = multiple;
		bs[b].group_id = group_id;
		bs[b].button_id = button_id;
		bs[b].i_pointer = integer_value;
		bs[b].x = x;
		bs[b].y = y;
		bs[b].width = width;
		bs[b].height = height;
		bs[b].mouse_button = mouse_button;
		bs[b].draw_bar = draw_bar;
		bs[b].full_bar_colour[0] = 0;
		bs[b].full_bar_colour[1] = 255;
		bs[b].full_bar_colour[2] = 0;
		bs[b].empty_bar_colour[0] = 0;
		bs[b].empty_bar_colour[1] = 0;
		bs[b].empty_bar_colour[2] = 255;
	}
	else
	{
		OUTPUT_message("Could not create button! You ain't been cleaning up behind you, have you?!");
		assert(0);
	}
}



void SIMPGUI_create_v_ramp_button (int group_id, int button_id, int *integer_value, int x, int y, int width, int height , int start, int end, int multiple, int mouse_button , bool rounded_up , bool draw_bar)
{
	int b;

	b = SIMPGUI_next_free_button();

	if (b > -1)
	{
		bs[b].life_status = BUTTON_ALIVE;
		bs[b].rounded_up = rounded_up;
		bs[b].button_type = BUTTON_TYPE_V_RAMP;
		bs[b].start = start;
		bs[b].end = end;
		bs[b].multiple = multiple;
		bs[b].group_id = group_id;
		bs[b].button_id = button_id;
		bs[b].i_pointer = integer_value;
		bs[b].x = x;
		bs[b].y = y;
		bs[b].width = width;
		bs[b].height = height;
		bs[b].mouse_button = mouse_button;
		bs[b].draw_bar = draw_bar;
		bs[b].full_bar_colour[0] = 0;
		bs[b].full_bar_colour[1] = 255;
		bs[b].full_bar_colour[2] = 0;
		bs[b].empty_bar_colour[0] = 0;
		bs[b].empty_bar_colour[1] = 0;
		bs[b].empty_bar_colour[2] = 255;
	}
	else
	{
		OUTPUT_message("Could not create button! You ain't been cleaning up behind you, have you?!");
		assert(0);
	}
}



void SIMPGUI_create_nudge_button (int group_id, int button_id, int *integer_value, int x, int y, int bitmap_number, int sprite_number, int mouse_button, int start, int end, int nudge_by, bool wrap)
{
	int b;

	b = SIMPGUI_next_free_button();

	if (b > -1)
	{
		bs[b].life_status = BUTTON_ALIVE;
		bs[b].button_type = BUTTON_TYPE_NUDGE_VALUE;
		bs[b].start = start;
		bs[b].end = end;
		bs[b].nudge_by = nudge_by;
		bs[b].wrap = wrap;
		bs[b].group_id = group_id;
		bs[b].button_id = button_id;
		bs[b].i_pointer = integer_value;
		bs[b].bitmap_number = bitmap_number;
		bs[b].frame_number = sprite_number;
		bs[b].overlay_number = sprite_number;
		bs[b].x = x;
		bs[b].y = y;
		bs[b].width = OUTPUT_sprite_width (bitmap_number,sprite_number);
		bs[b].height = OUTPUT_sprite_height (bitmap_number,sprite_number);
		bs[b].mouse_button = mouse_button;
	}
	else
	{
		OUTPUT_message("Could not create button! You ain't been cleaning up behind you, have you?!");
		assert(0);
	}
}



void SIMPGUI_create_h_drag_bar (int group_id, int button_id, int *integer_value, int x, int y, int width, int height , int start, int end, int drag_value, int drag_bar_size, int mouse_button)
{
	int b;

	b = SIMPGUI_next_free_button();

	if (b > -1)
	{
		bs[b].life_status = BUTTON_ALIVE;
		bs[b].button_type = BUTTON_TYPE_H_DRAG_BAR;
		bs[b].start = start;
		bs[b].end = end;
		bs[b].drag_bar_equivalent = drag_value;
		bs[b].drag_bar_size = drag_bar_size;
		bs[b].group_id = group_id;
		bs[b].button_id = button_id;
		bs[b].i_pointer = integer_value;
		bs[b].x = x;
		bs[b].y = y;
		bs[b].width = width;
		bs[b].height = height;
		bs[b].mouse_button = mouse_button;

		*bs[b].i_pointer = MATH_constrain(*bs[b].i_pointer , bs[b].start , bs[b].end);
	}
	else
	{
		OUTPUT_message("Could not create button! You ain't been cleaning up behind you, have you?!");
		assert(0);
	}
}



void SIMPGUI_create_v_drag_bar (int group_id, int button_id, int *integer_value, int x, int y, int width, int height , int start, int end, int drag_value, int drag_bar_size, int mouse_button)
{
	int b;

	b = SIMPGUI_next_free_button();

	if (b > -1)
	{
		bs[b].life_status = BUTTON_ALIVE;
		bs[b].button_type = BUTTON_TYPE_V_DRAG_BAR;
		bs[b].start = start;
		bs[b].end = end;
		bs[b].drag_bar_equivalent = drag_value;
		bs[b].drag_bar_size = drag_bar_size;
		bs[b].group_id = group_id;
		bs[b].button_id = button_id;
		bs[b].i_pointer = integer_value;
		bs[b].x = x;
		bs[b].y = y;
		bs[b].width = width;
		bs[b].height = height;
		bs[b].mouse_button = mouse_button;

		*bs[b].i_pointer = MATH_constrain(*bs[b].i_pointer , bs[b].start , bs[b].end);

	}
	else
	{
		OUTPUT_message("Could not create button! You ain't been cleaning up behind you, have you?!");
		assert(0);
	}

}



void SIMPGUI_create_binary_toggle_button (int group_id, int button_id, int *integer_value, int x, int y, int bitmap_number, int sprite_number, int mouse_button, int xor_value)
{
	int b;

	b = SIMPGUI_next_free_button();

	if (b > -1)
	{
		bs[b].life_status = BUTTON_ALIVE;
		bs[b].button_type = BUTTON_TYPE_BINARY_TOGGLE;
		bs[b].xor_value = xor_value;
		bs[b].group_id = group_id;
		bs[b].button_id = button_id;
		bs[b].position_status = (*integer_value && xor_value ? BUTTON_ON : BUTTON_OFF);
		bs[b].i_pointer = integer_value;
		bs[b].bitmap_number = bitmap_number;
		bs[b].frame_number = sprite_number;
		bs[b].overlay_number = sprite_number;
		bs[b].x = x;
		bs[b].y = y;
		bs[b].width = OUTPUT_sprite_width (bitmap_number,sprite_number);
		bs[b].height = OUTPUT_sprite_height (bitmap_number,sprite_number);
		bs[b].mouse_button = mouse_button;
		bs[b].h_flip_overlay = true;
	}
	else
	{
		OUTPUT_message("Could not create button! You ain't been cleaning up behind you, have you?!");
		assert(0);
	}

}



void SIMPGUI_sleep_group (int group_id)
{

	int b;

	for (b=0 ; b<MAX_GUI_BUTTONS ; b++)
	{
		if ( ( (bs[b].group_id == group_id) || (group_id == UNSET) ) && (bs[b].life_status != BUTTON_DEAD) )
		{
			bs[b].life_status = BUTTON_SLEEPING;
		}
	}

}



void SIMPGUI_wake_group (int group_id)
{

	int b;

	for (b=0 ; b<MAX_GUI_BUTTONS ; b++)
	{
		if ( ( (bs[b].group_id == group_id) || (group_id == UNSET) ) && (bs[b].life_status == BUTTON_SLEEPING) )
		{
			bs[b].life_status = BUTTON_ALIVE;
		}
	}

}



void SIMPGUI_hide_group (int group_id)
{

	int b;

	for (b=0 ; b<MAX_GUI_BUTTONS ; b++)
	{
		if ( ( (bs[b].group_id == group_id) || (group_id == UNSET) ) && (bs[b].life_status != BUTTON_DEAD) )
		{
			bs[b].life_status = BUTTON_HIDDEN;
		}
	}

}



void SIMPGUI_unhide_group (int group_id)
{

	int b;

	for (b=0 ; b<MAX_GUI_BUTTONS ; b++)
	{
		if ( ( (bs[b].group_id == group_id) || (group_id == UNSET) ) && (bs[b].life_status == BUTTON_HIDDEN) )
		{
			bs[b].life_status = BUTTON_ALIVE;
		}
	}

}



void SIMPGUI_kill_group (int group_id)
{

	int b;

	for (b=0 ; b<MAX_GUI_BUTTONS ; b++)
	{
		if ( (bs[b].group_id == group_id) || (group_id == UNSET) )
		{
			SIMPGUI_reset_button (b);
		}
	}

}



void SIMPGUI_set_overlay_group (int group_id, int sprite_number)
{
	int b;

	for (b=0 ; b<MAX_GUI_BUTTONS ; b++)
	{
		if ( (bs[b].group_id == group_id) || (group_id == UNSET) )
		{
			bs[b].overlay_number = sprite_number;
		}
	}
}



void SIMPGUI_set_h_flip_group (int group_id, bool on_or_off)
{
	int b;

	for (b=0 ; b<MAX_GUI_BUTTONS ; b++)
	{
		if ( (bs[b].group_id == group_id) || (group_id == UNSET) )
		{
			bs[b].h_flip_overlay = on_or_off;
		}
	}
}



void SIMPGUI_setup (void)
{
	int b;

	for (b=0 ; b<MAX_GUI_BUTTONS ; b++)
	{
		SIMPGUI_reset_button (b);
	}

}



void SIMPGUI_check_all_buttons (void)
{
	// This goes through all the buttons checking to see if the mouse is within them and if a click has registered then acts on it.
	// Then goes through all the buttons regardless and updates the on/off position in case the value that controls them has been
	// altered externally.

	int b;

	static int priority_button = UNSET; // If this is set to >-1 then all buttons will be ignored except the numbered one. This is so when you
	// grab a scroll bar you don't go stomping all over other buttons.

	int x_offset,y_offset;

	float temp_f;
	bool flag;

	int temp_i;

	int start,end;
	bool override;

	if (priority_button == UNSET)
	{
		start = 0;
		end = MAX_GUI_BUTTONS;
		override = false;
	}
	else
	{
		start = priority_button;
		end = priority_button+1;
		override = true; // Because you can be outside the box when dragging
	}

	for (b=start ; b<end ; b++)
	{
		bs[b].click_status = false;

		if ( ( (MATH_rectangle_to_point_intersect (bs[b].x , bs[b].y , bs[b].x + bs[b].width , bs[b].y + bs[b].height , CONTROL_mouse_x() , CONTROL_mouse_y() ) == true) || (override) ) && (bs[b].life_status == BUTTON_ALIVE) ) // If (either the mouse is in the box OR the override is set) AND (the button is active)
		{
			x_offset = CONTROL_mouse_x() - bs[b].x;
			y_offset = CONTROL_mouse_y() - bs[b].y;

			switch (bs[b].button_type)
			{
			case BUTTON_TYPE_TOGGLE:
				if (CONTROL_mouse_hit (bs[b].mouse_button) == true)
				{
					bs[b].position_status = (bs[b].position_status == BUTTON_ON ? BUTTON_OFF : BUTTON_ON);
					*bs[b].i_pointer = bs[b].position_status;
					bs[b].click_status = true;
				}
				break;

			case BUTTON_TYPE_RADIO_SET:
				if (CONTROL_mouse_hit (bs[b].mouse_button) == true)
				{
					bs[b].position_status = BUTTON_ON; // turn on button
					*bs[b].i_pointer = bs[b].set_value; // set integer to value
					bs[b].click_status = true;

//					for (b2 = 0; b2<MAX_GUI_BUTTONS ; b2++) // turn off all other buttons in group
//					{
//						if ( (b2 != b) && (bs[b].group_id == bs[b2].group_id) )
//						{
//							bs[b2].position_status = BUTTON_OFF;
//						}
//					}

				}
				break;

			case BUTTON_TYPE_H_RAMP:
				if (CONTROL_mouse_down (bs[b].mouse_button) == true)
				{
					if (bs[b].rounded_up == true)
					{
						x_offset += int ( float (bs[b].width / 2) / float ((bs[b].end - bs[b].start) / bs[b].multiple) );
					}
					temp_f = float (x_offset) / float (bs[b].width);
					*bs[b].i_pointer = int ( MATH_lerp ( float (bs[b].start) , float(bs[b].end) , temp_f ) );
					if ( (bs[b].rounded_up == false) && (*bs[b].i_pointer == int ( MATH_largest ( float(bs[b].start), float(bs[b].end) ) ) ) )
					{
						*bs[b].i_pointer = int ( MATH_largest ( float(bs[b].start), float(bs[b].end) ) )-1;
					}
					*bs[b].i_pointer -= (*bs[b].i_pointer % bs[b].multiple);
					bs[b].click_status = true;
				}

				break;

			case BUTTON_TYPE_V_RAMP:
				if (CONTROL_mouse_down (bs[b].mouse_button) == true)
				{
					if (bs[b].rounded_up == true)
					{
						y_offset += int ( float (bs[b].height / 2) / float ((bs[b].end - bs[b].start) / bs[b].multiple) );
					}
					temp_f = float (y_offset) / float (bs[b].height);
					*bs[b].i_pointer = int ( MATH_lerp ( float(bs[b].start) , float(bs[b].end) , temp_f ) );
					if ( (bs[b].rounded_up == false) && (*bs[b].i_pointer == int ( MATH_largest ( float(bs[b].start), float(bs[b].end) ) ) ) )
					{
						*bs[b].i_pointer = int ( MATH_largest ( float(bs[b].start), float(bs[b].end) ) )-1;
					}
					*bs[b].i_pointer -= (*bs[b].i_pointer % bs[b].multiple);
					bs[b].click_status = true;
				}

				break;

			case BUTTON_TYPE_NUDGE_VALUE:
				flag = false;

				if (CONTROL_mouse_hit (bs[b].mouse_button) == true)
				{
					flag = true;
					bs[b].counter = 0;
				}

				if (CONTROL_mouse_down (bs[b].mouse_button) == true)
				{
					bs[b].counter++;

					if ( (bs[b].counter > bs[b].nudge_repeat_delay) && ( (bs[b].counter - bs[b].nudge_repeat_delay) % bs[b].nudge_repeat_frequency == 0) )
					{
						flag = true;
					}
				}

				if (flag)
				{
					bs[b].click_status = true;

					*bs[b].i_pointer += bs[b].nudge_by;
					
					if (*bs[b].i_pointer > bs[b].end)
					{
						if (bs[b].wrap == true)
						{
							*bs[b].i_pointer = bs[b].start;
						}
						else
						{
							*bs[b].i_pointer = bs[b].end;
						}
					}

					if (*bs[b].i_pointer < bs[b].start)
					{
						if (bs[b].wrap == true)
						{
							*bs[b].i_pointer = bs[b].end;
						}
						else
						{
							*bs[b].i_pointer = bs[b].start;
						}
					}

				}

				break;

			case BUTTON_TYPE_:
				// No action, purely a display type button
				break;

			case BUTTON_TYPE_H_DRAG_BAR:
				// Aargh! Drag bars!

				if (CONTROL_mouse_hit (bs[b].mouse_button) == true)
				{
					// Clicking above or below the drag bar will alter the value by a set amount.

					if (x_offset < bs[b].drag_bar_position)
					{
						*bs[b].i_pointer -= bs[b].drag_bar_equivalent;
						if (*bs[b].i_pointer < bs[b].start)
						{
							*bs[b].i_pointer = bs[b].start;
						}
						bs[b].click_status = true;
					}

					if (x_offset > bs[b].drag_bar_position + bs[b].drag_bar_size)
					{
						*bs[b].i_pointer += bs[b].drag_bar_equivalent;
						if (*bs[b].i_pointer > bs[b].end)
						{
							*bs[b].i_pointer = bs[b].end;
						}
						bs[b].click_status = true;
					}

					// Clicking on the drag bar will grab it and won't let go until you do.

					if ( (x_offset >= bs[b].drag_bar_position) && (x_offset <= bs[b].drag_bar_position + bs[b].drag_bar_size) )
					{
						priority_button = b;
						bs[b].grabbed = true;
						bs[b].grabbed_at = x_offset;
						bs[b].grabbed_value = *bs[b].i_pointer;
					}
				}
				
				if ( (CONTROL_mouse_down (bs[b].mouse_button) == true) && (bs[b].grabbed == true) )
				{
					temp_i = x_offset - bs[b].grabbed_at; // pixel offset from the grabbed position to the new one.
					temp_f = float (bs[b].end - bs[b].start) / float (bs[b].width - bs[b].drag_bar_size);
					*bs[b].i_pointer = bs[b].grabbed_value + int (temp_i * temp_f);
					bs[b].click_status = true;
					if (*bs[b].i_pointer > bs[b].end)
					{
						*bs[b].i_pointer = bs[b].end;
					}
					if (*bs[b].i_pointer < bs[b].start)
					{
						*bs[b].i_pointer = bs[b].start;
					}
				}
				else
				{
					bs[b].grabbed = false;
					priority_button = UNSET;
				}
				break;

			case BUTTON_TYPE_V_DRAG_BAR:
				if (CONTROL_mouse_hit (bs[b].mouse_button) == true)
				{
					// Clicking above or below the drag bar will alter the value by a set amount.

					if (y_offset < bs[b].drag_bar_position)
					{
						*bs[b].i_pointer -= bs[b].drag_bar_equivalent;
						if (*bs[b].i_pointer < bs[b].start)
						{
							*bs[b].i_pointer = bs[b].start;
						}
						bs[b].click_status = true;
					}

					if (y_offset > bs[b].drag_bar_position + bs[b].drag_bar_size)
					{
						*bs[b].i_pointer += bs[b].drag_bar_equivalent;
						if (*bs[b].i_pointer > bs[b].end)
						{
							*bs[b].i_pointer = bs[b].end;
						}
						bs[b].click_status = true;
					}

					// Clicking on the drag bar will grab it and won't let go until you do.

					if ( (y_offset >= bs[b].drag_bar_position) && (y_offset <= bs[b].drag_bar_position + bs[b].drag_bar_size) )
					{
						priority_button = b;
						bs[b].grabbed = true;
						bs[b].grabbed_at = y_offset;
						bs[b].grabbed_value = *bs[b].i_pointer;
					}
				}
				
				if ( (CONTROL_mouse_down (bs[b].mouse_button) == true) && (bs[b].grabbed == true) )
				{
					temp_i = y_offset - bs[b].grabbed_at; // pixel offset from the grabbed position to the new one.
					temp_f = float (bs[b].end - bs[b].start) / float (bs[b].height - bs[b].drag_bar_size);
					*bs[b].i_pointer = bs[b].grabbed_value + int (temp_i * temp_f);
					bs[b].click_status = true;
					if (*bs[b].i_pointer > bs[b].end)
					{
						*bs[b].i_pointer = bs[b].end;
					}
					if (*bs[b].i_pointer < bs[b].start)
					{
						*bs[b].i_pointer = bs[b].start;
					}
				}
				else
				{
					bs[b].grabbed = false;
					priority_button = UNSET;
				}
				break;

			case BUTTON_TYPE_BINARY_TOGGLE:
				if (CONTROL_mouse_hit (bs[b].mouse_button) == true)
				{
					*bs[b].i_pointer ^= bs[b].xor_value;
					bs[b].position_status = (*bs[b].i_pointer & bs[b].xor_value ? BUTTON_ON : BUTTON_OFF);
					bs[b].click_status = true;
				}
				break;

			case BUTTON_TYPE_SET:
				if (CONTROL_mouse_hit (bs[b].mouse_button) == true)
				{
					bs[b].position_status = BUTTON_ON; // turn on button
					*bs[b].i_pointer = bs[b].set_value; // set integer to value
					bs[b].click_status = true;
				}
				break;

			default:
				break;

			}

		}
	}

	for (b=0 ; b<MAX_GUI_BUTTONS ; b++)
	{
		if (bs[b].life_status != BUTTON_DEAD) // If (either the mouse is in the box OR the override is set) AND (the button is active)
		{
			switch (bs[b].button_type)
			{
			case BUTTON_TYPE_TOGGLE:
				bs[b].position_status = (*bs[b].i_pointer == 0 ? BUTTON_OFF : BUTTON_ON);
				break;

			case BUTTON_TYPE_RADIO_SET:
				bs[b].position_status = (*bs[b].i_pointer == bs[b].set_value ? BUTTON_ON : BUTTON_OFF);
				break;

			case BUTTON_TYPE_BINARY_TOGGLE:
				bs[b].position_status = (*bs[b].i_pointer & bs[b].xor_value ? BUTTON_ON : BUTTON_OFF);
				break;

			case BUTTON_TYPE_SET:
				bs[b].position_status = (*bs[b].i_pointer == bs[b].set_value ? BUTTON_ON : BUTTON_OFF);
				break;

			default:
				break;
			}
		}
	}

}



void SIMPGUI_draw_all_buttons (int group_number, bool inverse_selection)
{
	// This goes through all the buttons and draws them.

	static bool flip_flop;

	int b;
	float temp_f;
	int temp_i;
	float block_size;

	flip_flop = ( flip_flop == true ? false : true );

	for (b=0 ; b<MAX_GUI_BUTTONS ; b++)
	{
		if ( (group_number == UNSET) || ( ( (bs[b].group_id == group_number) && (inverse_selection == false) ) || ( (bs[b].group_id != group_number) && (inverse_selection == true) ) ) )
		{
			if ( (bs[b].life_status == BUTTON_ALIVE) || (bs[b].life_status == BUTTON_SLEEPING) )
			{

				switch (bs[b].button_type)
				{
				case BUTTON_TYPE_TOGGLE:
				case BUTTON_TYPE_RADIO_SET:
				case BUTTON_TYPE_NUDGE_VALUE:
				case BUTTON_TYPE_BINARY_TOGGLE:
				case BUTTON_TYPE_SET:
					OUTPUT_draw_masked_sprite ( bs[b].bitmap_number , bs[b].frame_number , bs[b].x, bs[b].y );
					if ( (bs[b].position_status == BUTTON_OFF) || (bs[b].life_status == BUTTON_SLEEPING) )
					{
						if ( (bs[b].h_flip_overlay == true) && (flip_flop) )
						{
							OUTPUT_draw_masked_sprite( bs[b].bitmap_number , bs[b].overlay_number , bs[b].x, bs[b].y );
						}
						else if ( (bs[b].flicker_overlay == true) && (flip_flop) )
						{
							// do sod all, don't draw it!
						}
						else
						{
							OUTPUT_draw_masked_sprite( bs[b].bitmap_number , bs[b].overlay_number , bs[b].x, bs[b].y );
						}
					}
					break;

				case BUTTON_TYPE_H_RAMP:
					if (bs[b].draw_bar == true)
					{
						OUTPUT_filled_rectangle ( bs[b].x , bs[b].y , bs[b].x + bs[b].width , bs[b].y + bs[b].height , 0 , 0 , 0 );
						block_size = float (bs[b].width) / float ( (bs[b].end - bs[b].start) / bs[b].multiple );
						for ( temp_i=0 ; temp_i < (bs[b].end - bs[b].start) ; temp_i += bs[b].multiple )
						{
							if (temp_i < *bs[b].i_pointer - bs[b].start)
							{
								OUTPUT_filled_rectangle ( bs[b].x + int ((temp_i/bs[b].multiple)*block_size) , bs[b].y +1 , bs[b].x + int (((temp_i/bs[b].multiple)*block_size) + block_size) - 2 , bs[b].y + bs[b].height -2 , 255 , 255 , 0 );
							}
							else
							{
								OUTPUT_rectangle ( bs[b].x + int ((temp_i/bs[b].multiple)*block_size) , bs[b].y +1 , bs[b].x + int (((temp_i/bs[b].multiple)*block_size) + block_size) - 2 , bs[b].y + bs[b].height -2 , 0 , 0 , 255 );
							}
						}
					}
					break;


				case BUTTON_TYPE_V_RAMP:
					if (bs[b].draw_bar == true)
					{
						OUTPUT_filled_rectangle ( bs[b].x , bs[b].y , bs[b].x + bs[b].width , bs[b].y + bs[b].height , 0 , 0 , 0 );
						block_size = float (bs[b].height) / float ( (bs[b].end - bs[b].start) / bs[b].multiple );
						for ( temp_i=0 ; temp_i < (bs[b].end - bs[b].start) ; temp_i += bs[b].multiple )
						{
							if (temp_i < *bs[b].i_pointer - bs[b].start)
							{
								OUTPUT_filled_rectangle ( bs[b].x +1 , bs[b].y + int ((temp_i/bs[b].multiple)*block_size) , bs[b].x + bs[b].width -2 , bs[b].y + int (((temp_i/bs[b].multiple)*block_size) + block_size) - 2 , 255 , 255 , 0 );
							}
							else
							{
								OUTPUT_rectangle ( bs[b].x +1, bs[b].y + int ((temp_i/bs[b].multiple)*block_size) , bs[b].x + bs[b].width -2 , bs[b].y + int (((temp_i/bs[b].multiple)*block_size) + block_size) - 2 , 0 , 0 , 255 );
							}
						}
					}
					break;

				case BUTTON_TYPE_:
					break;

				case BUTTON_TYPE_H_DRAG_BAR:
					OUTPUT_filled_rectangle ( bs[b].x , bs[b].y , bs[b].x + bs[b].width -1 , bs[b].y + bs[b].height -1 , 0 , 0 , 0 );
					temp_f = float (*bs[b].i_pointer - bs[b].start) / float (bs[b].end - bs[b].start);
					bs[b].drag_bar_position = int (float (bs[b].width - bs[b].drag_bar_size) * temp_f);
					OUTPUT_rectangle ( bs[b].x , bs[b].y , bs[b].x + bs[b].width - 1 , bs[b].y + bs[b].height - 1, 0 , 0 , 255 );
					OUTPUT_filled_rectangle ( bs[b].x+bs[b].drag_bar_position, bs[b].y+2, bs[b].x+bs[b].drag_bar_position+bs[b].drag_bar_size, bs[b].y+bs[b].height-3 , 0 , 255 , 0 );
					break;

				case BUTTON_TYPE_V_DRAG_BAR:
					OUTPUT_filled_rectangle ( bs[b].x , bs[b].y , bs[b].x + bs[b].width -1 , bs[b].y + bs[b].height -1 , 0 , 0 , 0 );
					temp_f = float (*bs[b].i_pointer - bs[b].start) / float (bs[b].end - bs[b].start);
					bs[b].drag_bar_position = int (float (bs[b].height - bs[b].drag_bar_size) * temp_f);
					OUTPUT_rectangle ( bs[b].x , bs[b].y , bs[b].x + bs[b].width - 1 , bs[b].y + bs[b].height - 1 , 0 , 0 , 255 );
					OUTPUT_filled_rectangle ( bs[b].x+2, bs[b].y+bs[b].drag_bar_position, bs[b].x+bs[b].width-3, bs[b].y+bs[b].drag_bar_position+bs[b].drag_bar_size , 0 , 255 , 0 );
					break;

				default:
					break;

				}

			}
		}

	}

}



bool SIMPGUI_get_click_status (int group_id, int button_id)
{
	int b;

	for (b=0 ; b<MAX_GUI_BUTTONS ; b++)
	{
		if ( (bs[b].group_id == group_id) && (bs[b].button_id == button_id) )
		{
			if (bs[b].click_status == true)
			{
				return true;
			}
		}
	}

	return false;
}



bool SIMPGUI_does_button_exist (int group_id, int button_id)
{
	int b;

	for (b=0 ; b<MAX_GUI_BUTTONS ; b++)
	{
		if ( (bs[b].group_id == group_id) && (bs[b].button_id == button_id) )
		{
			return true;
		}
	}

	return false;
}



void SIMPGUI_kill_button (int group_id, int button_id)
{
	int b;

	for (b=0 ; b<MAX_GUI_BUTTONS ; b++)
	{
		if ( (bs[b].group_id == group_id) && (bs[b].button_id == button_id) )
		{
			SIMPGUI_reset_button (b);
		}
	}
}
