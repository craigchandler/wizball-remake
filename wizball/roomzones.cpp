#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <math.h>

#include "spawn_points.h"
#include "tilemaps.h" // Duh!
#include "main.h" // For get_project_filename and also state constants
#include "output.h" // Drawing stuff
#include "control.h" // Mouse and keyboard input
#include "global_param_list.h" // For list access
#include "math_stuff.h"
#include "string_stuff.h"
#include "file_stuff.h"
#include "tilesets.h"
#include "simple_gui.h"
#include "paths.h"
#include "roomzones.h"
#include "world_collision.h"

#include "fortify.h"

int localised_zone_divider = 512;



void ROOMZONES_swap_zones (int tilemap_number, int zone_1, int zone_2)
{
	zone temp;

	temp = cm[tilemap_number].zone_list_pointer[zone_1];
	cm[tilemap_number].zone_list_pointer[zone_1] = cm[tilemap_number].zone_list_pointer[zone_2];
	cm[tilemap_number].zone_list_pointer[zone_2] = temp;
}



void ROOMZONES_destroy_zones (int tilemap_number)
{
	free (cm[tilemap_number].zone_list_pointer);
	cm[tilemap_number].zone_list_pointer = NULL;
	cm[tilemap_number].zones = 0;
}



void ROOMZONES_destroy_localised_zone_list (int tilemap_number)
{
	int lzone_number;

	for (lzone_number=0; lzone_number<cm[tilemap_number].localised_zone_list_size; lzone_number++)
	{
		free (cm[tilemap_number].localised_zone_list_pointer[lzone_number].indexes);
		cm[tilemap_number].localised_zone_list_pointer[lzone_number].indexes = NULL;
	}

	free (cm[tilemap_number].localised_zone_list_pointer);
	cm[tilemap_number].localised_zone_list_pointer = NULL;
}



void ROOMZONES_delete_particular_zone (int tilemap_number , int zone_number)
{
	// This deletes the given zone by first bubbling it to the end of the queue and then
	// destroying it.

	int total;
	int swap_counter;
	
	total = cm[tilemap_number].zones;

	// Okay, to bubble the given one up to the surface we need to swap all the elements
	// from zone_number to total-2 with the one after them.

	for (swap_counter = zone_number ; swap_counter<total-1 ; swap_counter++)
	{
		ROOMZONES_swap_zones (tilemap_number , swap_counter , swap_counter+1 );
	}

	// Then we realloc the memory used by the zone list to cut off the last zone.
	// We don't need to free it up first as it doesn't contain any pointers or malloc'd space itself.

	cm[tilemap_number].zone_list_pointer = (zone *) realloc ( cm[tilemap_number].zone_list_pointer , (total-1) * sizeof (zone) );

	cm[tilemap_number].zones--;
}



int ROOMZONES_get_free_roomzone_uid (int tilemap_number)
{
	cm[tilemap_number].zone_next_uid++;

	return (cm[tilemap_number].zone_next_uid - 1);
}



void ROOMZONES_create_zone (int tilemap_number, int x, int y, int width, int height, int zone_type=UNSET)
{
	int total;

	total = cm[tilemap_number].zones;

	if (cm[tilemap_number].zone_list_pointer == NULL)
	{
		// None created yet.

		cm[tilemap_number].zone_list_pointer = (zone *) malloc ( sizeof (zone) );
	}
	else
	{
		cm[tilemap_number].zone_list_pointer = (zone *) realloc ( cm[tilemap_number].zone_list_pointer , (total+1) * sizeof (zone) );
	}

	cm[tilemap_number].zone_list_pointer[total].x = x;
	cm[tilemap_number].zone_list_pointer[total].y = y;

	cm[tilemap_number].zone_list_pointer[total].width = width;
	cm[tilemap_number].zone_list_pointer[total].height = height;
	
	if (zone_type == UNSET)
	{
		strcpy ( cm[tilemap_number].zone_list_pointer[total].type , "UNSET" );
		cm[tilemap_number].zone_list_pointer[total].type_index = UNSET;
	}
	else
	{
		cm[tilemap_number].zone_list_pointer[total].type_index = zone_type;
		strcpy ( cm[tilemap_number].zone_list_pointer[total].type , GPL_get_entry_name ("ZONE_TYPES",zone_type) );
	}

	strcpy ( cm[tilemap_number].zone_list_pointer[total].text_tag , "UNSET" );
	cm[tilemap_number].zone_list_pointer[total].text_tag_index = UNSET;

	cm[tilemap_number].zone_list_pointer[total].flag = 0;

	cm[tilemap_number].zone_list_pointer[total].spawn_point_index_list = NULL;
	cm[tilemap_number].zone_list_pointer[total].spawn_point_index_list_size = 0;

	cm[tilemap_number].zone_list_pointer[total].ai_node_index_list = NULL;
	cm[tilemap_number].zone_list_pointer[total].ai_node_index_list_size = 0;

	cm[tilemap_number].zone_list_pointer[total].uid = ROOMZONES_get_free_roomzone_uid (tilemap_number);

	cm[tilemap_number].zones++;
}



#define ZONE_HANDLE_SIZE					(16)

int ROOMZONES_check_zone_handle ( int tilemap_number, int real_mx , int real_my, float zoom_level)
{
	// This checks all the zones in the specified map to see if the mouse is within the handle of them (at the top-left of the bounding box)

	int tileset_number;
	int tilesize;

	tileset_number = cm[tilemap_number].tile_set_index;

	if (tileset_number != UNSET)
	{
		tilesize = TILESETS_get_tilesize(tileset_number);
	}
	else
	{
		tilesize = 16;
	}

	int z;
	int real_zone_x;
	int real_zone_y;

	for (z=0; z<cm[tilemap_number].zones; z++)
	{
		real_zone_x = cm[tilemap_number].zone_list_pointer[z].x * tilesize;
		real_zone_y = cm[tilemap_number].zone_list_pointer[z].y * tilesize;

		if (MATH_rectangle_to_point_intersect ( real_zone_x,real_zone_y,real_zone_x+int(float(2*ZONE_HANDLE_SIZE)/zoom_level),real_zone_y+int(float(2*ZONE_HANDLE_SIZE)/zoom_level) , real_mx,real_my ) == true )
		{
			return z;
		}
	}

	return UNSET;
}



void ROOMZONES_create_multiple_zones ( int tilemap_number, bool overwrite_zones, bool create_partials, int start_x, int start_y, int zone_width, int zone_height, int gap_width, int gap_height, int zone_type)
{
	if (overwrite_zones == true)
	{
		ROOMZONES_destroy_zones (tilemap_number);
	}

	int remainder_x;
	int remainder_y;

	// These'll be used to create the remainders if there are any. They should never be <0.
	remainder_x = (cm[tilemap_number].map_width-start_x) % (zone_width+gap_width);
	remainder_y = (cm[tilemap_number].map_height-start_y) % (zone_height+gap_height);

	if ( (remainder_x<0) || (remainder_y<0) )
	{
		assert (0);
	}

	int x;
	int y;

	for (x=start_x ; x < cm[tilemap_number].map_width - remainder_x ; x += (zone_width + gap_width) )
	{
		for (y=start_y ; y < cm[tilemap_number].map_height - remainder_y ; y += (zone_height + gap_height) )
		{
			ROOMZONES_create_zone (tilemap_number,x,y,zone_width,zone_height,zone_type);
		}
	}

	int new_start_x;
	int new_start_y;

	if (create_partials == true)
	{
		new_start_x = cm[tilemap_number].map_width - remainder_x;
		new_start_y = cm[tilemap_number].map_height - remainder_y;

		if (remainder_x > 0)
		{
			for (y=start_y ; y < cm[tilemap_number].map_height - remainder_y ; y += (zone_height + gap_height) )
			{
				ROOMZONES_create_zone (tilemap_number,new_start_x,y,remainder_x,zone_height,zone_type);
			}	
		}

		if (remainder_y > 0)
		{
			for (x=start_x ; x < cm[tilemap_number].map_width - remainder_x ; x += (zone_width + gap_width) )
			{
				ROOMZONES_create_zone (tilemap_number,x,new_start_y,zone_width,remainder_y,zone_type);
			}	
		}

		if ( (remainder_y > 0) && (remainder_x > 0) )
		{
			ROOMZONES_create_zone (tilemap_number,new_start_x,new_start_y,remainder_x,remainder_y,zone_type);
		}
	}

}



void ROOMZONES_confirm_room_zone_links (void)
{
	int tilemap_number;
	int zone_number;

	for (tilemap_number=0; tilemap_number<number_of_tilemaps_loaded; tilemap_number++)
	{
		for (zone_number=0; zone_number<cm[tilemap_number].zones; zone_number++)
		{
			cm[tilemap_number].zone_list_pointer[zone_number].type_index = GPL_find_word_value ("ZONE_TYPES" , cm[tilemap_number].zone_list_pointer[zone_number].type );
		}
	}
}



bool ROOMZONES_autozone_dialogue (int tilemap_number , int *autozoning_progress, int mx , int my , int x, int y)
{
	// This pops up several dialogues on the the screen to guide you through the autozoning process before
	// actually doing the zoning.

	static bool overwrite_zones;
	static int zone_width;
	static int zone_height;
	static int start_x;
	static int start_y;
	static int gap_width;
	static int gap_height;
	static bool create_partials;
	static int zone_type;

	int first_zone_type_in_list;
	int last_zone_type_in_list;

	int zone_type_box_height;

	int y_text_pos;

	int index;

	char line[TEXT_LINE_SIZE];

	switch (*autozoning_progress)
	{
	case 0:
		zone_width = guide_box_width;
		zone_height = guide_box_height;
		gap_width = 0;
		gap_height = 0;
		*autozoning_progress += 1;
		break;

	case 1:
		TILEMAPS_dialogue_box (x,y,ICON_SIZE*8,ICON_SIZE,"OVERWRITE CURRENT ZONES?");
		TILEMAPS_dialogue_box (x,y+ICON_SIZE,ICON_SIZE*4,ICON_SIZE,"YES");
		TILEMAPS_dialogue_box (x+ICON_SIZE*4,y+ICON_SIZE,ICON_SIZE*4,ICON_SIZE,"NO");

		if (CONTROL_mouse_hit(LMB))
		{
			if (MATH_rectangle_to_point_intersect_by_size (x,y+ICON_SIZE,ICON_SIZE*4,ICON_SIZE,mx,my) == true)
			{
				overwrite_zones = true;
				*autozoning_progress += 1; // Because ++ just increases the pointer, bizarrely.
			}
			else if (MATH_rectangle_to_point_intersect_by_size (x+ICON_SIZE*4,y+ICON_SIZE,ICON_SIZE*4,ICON_SIZE,mx,my) == true)
			{
				overwrite_zones = false;
				*autozoning_progress += 1;
			}

		}
		break;

	case 2:
		TILEMAPS_dialogue_box (x,y,ICON_SIZE*8,ICON_SIZE,"ZONE SIZE (IN TILES):");

		sprintf (line,"WIDTH %5d",zone_width);
		TILEMAPS_dialogue_box (x+ICON_SIZE,y+ICON_SIZE,ICON_SIZE*6,ICON_SIZE,line);

		sprintf (line,"HEIGHT %5d",zone_height);
		TILEMAPS_dialogue_box (x+ICON_SIZE,y+ICON_SIZE*2,ICON_SIZE*6,ICON_SIZE,line);

		OUTPUT_draw_masked_sprite ( first_icon , LEFT_ARROW_ICON, x , y+ICON_SIZE );
		OUTPUT_draw_masked_sprite ( first_icon , RIGHT_ARROW_ICON, x+ICON_SIZE*7 , y+ICON_SIZE );

		OUTPUT_draw_masked_sprite ( first_icon , LEFT_ARROW_ICON, x , y+ICON_SIZE*2 );
		OUTPUT_draw_masked_sprite ( first_icon , RIGHT_ARROW_ICON, x+ICON_SIZE*7 , y+ICON_SIZE*2 );

		TILEMAPS_dialogue_box (x+ICON_SIZE*2,y+ICON_SIZE*3,ICON_SIZE*4,ICON_SIZE,"ACCEPT");

		if (CONTROL_mouse_repeat(LMB))
		{
			if (MATH_rectangle_to_point_intersect_by_size (x,y+ICON_SIZE,ICON_SIZE,ICON_SIZE,mx,my) == true)
			{
				zone_width--;
			}

			if (MATH_rectangle_to_point_intersect_by_size (x+ICON_SIZE*7,y+ICON_SIZE,ICON_SIZE,ICON_SIZE,mx,my) == true)
			{
				zone_width++;
			}

			if (MATH_rectangle_to_point_intersect_by_size (x,y+ICON_SIZE*2,ICON_SIZE,ICON_SIZE,mx,my) == true)
			{
				zone_height--;
			}

			if (MATH_rectangle_to_point_intersect_by_size (x+ICON_SIZE*7,y+ICON_SIZE*2,ICON_SIZE,ICON_SIZE,mx,my) == true)
			{
				zone_height++;
			}

			zone_width = MATH_constrain (zone_width,1,cm[tilemap_number].map_width);
			zone_height = MATH_constrain (zone_height,1,cm[tilemap_number].map_height);
		}

		if (CONTROL_mouse_hit(LMB))
		{
			if (MATH_rectangle_to_point_intersect_by_size (x+ICON_SIZE*2,y+ICON_SIZE*3,ICON_SIZE*4,ICON_SIZE,mx,my) == true)
			{
				*autozoning_progress += 1;
			}
		}
		break;

	case 3:
		TILEMAPS_dialogue_box (x,y,ICON_SIZE*8,ICON_SIZE,"GAP SIZE (IN TILES):");

		sprintf (line,"WIDTH %5d",gap_width);
		TILEMAPS_dialogue_box (x+ICON_SIZE,y+ICON_SIZE,ICON_SIZE*6,ICON_SIZE,line);

		sprintf (line,"HEIGHT %5d",gap_height);
		TILEMAPS_dialogue_box (x+ICON_SIZE,y+ICON_SIZE*2,ICON_SIZE*6,ICON_SIZE,line);

		OUTPUT_draw_masked_sprite ( first_icon , LEFT_ARROW_ICON, x , y+ICON_SIZE );
		OUTPUT_draw_masked_sprite ( first_icon , RIGHT_ARROW_ICON, x+ICON_SIZE*7 , y+ICON_SIZE );

		OUTPUT_draw_masked_sprite ( first_icon , LEFT_ARROW_ICON, x , y+ICON_SIZE*2 );
		OUTPUT_draw_masked_sprite ( first_icon , RIGHT_ARROW_ICON, x+ICON_SIZE*7 , y+ICON_SIZE*2 );

		TILEMAPS_dialogue_box (x+ICON_SIZE*2,y+ICON_SIZE*3,ICON_SIZE*4,ICON_SIZE,"ACCEPT");

		if (CONTROL_mouse_repeat(LMB))
		{
			if (MATH_rectangle_to_point_intersect_by_size (x,y+ICON_SIZE,ICON_SIZE,ICON_SIZE,mx,my) == true)
			{
				gap_width--;
			}

			if (MATH_rectangle_to_point_intersect_by_size (x+ICON_SIZE*7,y+ICON_SIZE,ICON_SIZE,ICON_SIZE,mx,my) == true)
			{
				gap_width++;
			}

			if (MATH_rectangle_to_point_intersect_by_size (x,y+ICON_SIZE*2,ICON_SIZE,ICON_SIZE,mx,my) == true)
			{
				gap_height--;
			}

			if (MATH_rectangle_to_point_intersect_by_size (x+ICON_SIZE*7,y+ICON_SIZE*2,ICON_SIZE,ICON_SIZE,mx,my) == true)
			{
				gap_height++;
			}
		}

		gap_width = MATH_constrain (gap_width,0,cm[tilemap_number].map_width-zone_width);
		gap_height = MATH_constrain (gap_height,0,cm[tilemap_number].map_height-zone_height);

		if (CONTROL_mouse_hit(LMB))
		{
			if (MATH_rectangle_to_point_intersect_by_size (x+ICON_SIZE*2,y+ICON_SIZE*3,ICON_SIZE*4,ICON_SIZE,mx,my) == true)
			{
				*autozoning_progress += 1;
				start_x = gap_width/2;
				start_y = gap_height/2;
			}
		}
		break;

	case 4:
		TILEMAPS_dialogue_box (x,y,ICON_SIZE*8,ICON_SIZE,"START OFFSET (IN TILES):");

		sprintf (line,"WIDTH %5d",start_x);
		TILEMAPS_dialogue_box (x+ICON_SIZE,y+ICON_SIZE,ICON_SIZE*6,ICON_SIZE,line);

		sprintf (line,"HEIGHT %5d",start_y);
		TILEMAPS_dialogue_box (x+ICON_SIZE,y+ICON_SIZE*2,ICON_SIZE*6,ICON_SIZE,line);

		OUTPUT_draw_masked_sprite ( first_icon , LEFT_ARROW_ICON, x , y+ICON_SIZE );
		OUTPUT_draw_masked_sprite ( first_icon , RIGHT_ARROW_ICON, x+ICON_SIZE*7 , y+ICON_SIZE );

		OUTPUT_draw_masked_sprite ( first_icon , LEFT_ARROW_ICON, x , y+ICON_SIZE*2 );
		OUTPUT_draw_masked_sprite ( first_icon , RIGHT_ARROW_ICON, x+ICON_SIZE*7 , y+ICON_SIZE*2 );

		TILEMAPS_dialogue_box (x+ICON_SIZE*2,y+ICON_SIZE*3,ICON_SIZE*4,ICON_SIZE,"ACCEPT");

		if (CONTROL_mouse_repeat(LMB))
		{
			if (MATH_rectangle_to_point_intersect_by_size (x,y+ICON_SIZE,ICON_SIZE,ICON_SIZE,mx,my) == true)
			{
				start_x--;
			}

			if (MATH_rectangle_to_point_intersect_by_size (x+ICON_SIZE*7,y+ICON_SIZE,ICON_SIZE,ICON_SIZE,mx,my) == true)
			{
				start_x++;
			}

			if (MATH_rectangle_to_point_intersect_by_size (x,y+ICON_SIZE*2,ICON_SIZE,ICON_SIZE,mx,my) == true)
			{
				start_y--;
			}

			if (MATH_rectangle_to_point_intersect_by_size (x+ICON_SIZE*7,y+ICON_SIZE*2,ICON_SIZE,ICON_SIZE,mx,my) == true)
			{
				start_y++;
			}

		}

		start_x = MATH_constrain (start_x,0,cm[tilemap_number].map_width-zone_width-gap_width);
		start_y = MATH_constrain (start_y,0,cm[tilemap_number].map_height-zone_height-gap_height);

		if (CONTROL_mouse_hit(LMB))
		{
			if (MATH_rectangle_to_point_intersect_by_size (x+ICON_SIZE*2,y+ICON_SIZE*3,ICON_SIZE*4,ICON_SIZE,mx,my) == true)
			{
				*autozoning_progress += 1;
			}
		}
		break;

	case 5:
		TILEMAPS_dialogue_box (x,y,ICON_SIZE*8,ICON_SIZE,"CREATE PARTIAL ZONES?");
		TILEMAPS_dialogue_box (x,y+ICON_SIZE,ICON_SIZE*4,ICON_SIZE,"YES");
		TILEMAPS_dialogue_box (x+ICON_SIZE*4,y+ICON_SIZE,ICON_SIZE*4,ICON_SIZE,"NO");

		if (CONTROL_mouse_hit(LMB))
		{
			if (MATH_rectangle_to_point_intersect_by_size (x,y+ICON_SIZE,ICON_SIZE*4,ICON_SIZE,mx,my) == true)
			{
				create_partials= true;
				*autozoning_progress += 1;
			}
			else if (MATH_rectangle_to_point_intersect_by_size (x+ICON_SIZE*4,y+ICON_SIZE,ICON_SIZE*4,ICON_SIZE,mx,my) == true)
			{
				create_partials = false;
				*autozoning_progress += 1;
			}

		}
		break;

	case 6:
		GPL_list_extents("ZONE_TYPES",&first_zone_type_in_list,&last_zone_type_in_list);
		zone_type_box_height = ((last_zone_type_in_list-first_zone_type_in_list)+2) * FONT_HEIGHT;

		TILEMAPS_dialogue_box (x,y,ICON_SIZE*8,ICON_SIZE,"CHOOSE ZONE TYPE:");

		OUTPUT_filled_rectangle_by_size ( x , y+ICON_SIZE , ICON_SIZE*8, zone_type_box_height , 0, 0, 0 );
		OUTPUT_rectangle_by_size ( x , y+ICON_SIZE , ICON_SIZE*8, zone_type_box_height , 0, 0, 255 );
	
		zone_type = UNSET;

		for (index=first_zone_type_in_list; index<last_zone_type_in_list; index++)
		{
			y_text_pos = y+ICON_SIZE+FONT_HEIGHT+((index-first_zone_type_in_list)*FONT_HEIGHT);

			if ((my>=y_text_pos) && (my<y_text_pos+FONT_HEIGHT) && (mx>x) && (mx<x+(8*ICON_SIZE)))
			{
				zone_type = index - first_zone_type_in_list;
				OUTPUT_text (x+FONT_WIDTH,y_text_pos,GPL_what_is_word_name(index));
			}
			else
			{
				OUTPUT_text (x+FONT_WIDTH,y_text_pos,GPL_what_is_word_name(index),0,255,0);
			}
		}

		if ((CONTROL_mouse_hit(LMB)) && (zone_type != UNSET))
		{
			*autozoning_progress += 1;
		}
		break;

	case 7:
		ROOMZONES_create_multiple_zones (tilemap_number,overwrite_zones,create_partials,start_x,start_y,zone_width,zone_height,gap_width,gap_height,zone_type);
		return false;
		break;

	default:
		assert(0);
		break;
	}

	return true;
}



#define ZONE_EDIT_MODE_CREATE_ZONES			(0)
#define ZONE_EDIT_MODE_RESIZE_ZONES			(1)

#define ZONE_GRABBED_BODY					(1)
#define ZONE_GRABBED_TOP_LEFT				(2)
#define ZONE_GRABBED_TOP					(3)
#define ZONE_GRABBED_TOP_RIGHT				(4)
#define ZONE_GRABBED_RIGHT					(5)
#define ZONE_GRABBED_BOTTOM_RIGHT			(6)
#define ZONE_GRABBED_BOTTOM					(7)
#define ZONE_GRABBED_BOTTOM_LEFT			(8)
#define ZONE_GRABBED_LEFT					(9)

#define SETTING_PROPERTY_TEXT_TAG			(1)
#define SETTING_PROPERTY_TYPE				(2)

bool ROOMZONES_edit_room_zones (int state , bool overlay_display, int *current_cursor ,int sx, int sy,  int tilemap_number, int mx, int my, float zoom_level, int grid_size)
{
	// This allows you to place large arbitrary rectangles in your game which you can use for various things,
	// not least naming areas (as each zone has space for an text file tag) and constraining the camera. More commonly
	// it can be used with the "fire_spawn_points_in_zone" and "fire_spawn_points_in_zone_by_type" functions.

	int tx;
	int ty;

	int real_tx;
	int real_ty;

	char text_line[TEXT_LINE_SIZE];

	static int zone_start_tx;
	static int zone_start_ty;

	int tileset_number;
	int tilesize;

	int zone_tx;
	int zone_ty;
	int zone_width;
	int zone_height;

	static int making_zone_tx;
	static int making_zone_ty;
	static int making_zone_width;
	static int making_zone_height;

	int real_zone_x;
	int real_zone_y;
	int real_zone_width;
	int real_zone_height;
	int real_mx;
	int real_my; // These are used for the rectangles in worldspace.

	int diff_tx1;
	int diff_ty1;
	int diff_tx2;
	int diff_ty2;

	static int grabbed_real_tx;
	static int grabbed_real_ty;

	int proposed_grabbed_zone_handle;

	int z;

	int selection_box_x;
	int selection_box_y;
	int selection_box_width;
	int selection_box_height;

	static int replacement_cursor;

	static bool drawing_zone;

	static bool flip_flop;

	static int zone_editing_mode;
	static int selected_zone;
	static int grabbed_zone_handle;

	static int autozoning_progress;

	static bool menu_override;
	static bool auto_zone_override;

	char text_tag_name[NAME_SIZE] = "TEXT";
	char type_name[NAME_SIZE] = "ZONE_TYPES";

	static int setting_property;

	int zoomed_tilesize;
	int zone_edge_handle_size;

	if (state == STATE_INITIALISE)
	{
		drawing_zone = false;
		menu_override = false;
		auto_zone_override = false;
		selected_zone = UNSET;

		setting_property = UNSET;

		replacement_cursor = UNSET;

		zone_editing_mode = ZONE_EDIT_MODE_CREATE_ZONES;
	}
	else if (state == STATE_SET_UP_BUTTONS)
	{
		// No buttons, daddio! But if there were...

		editor_view_width = game_screen_width - (ICON_SIZE*8);
		editor_view_height = game_screen_height;



		SIMPGUI_sleep_group (TILEMAP_EDITOR_SUB_GROUP);
	}
	else if (state == STATE_RUNNING)
	{
		// Get the tilesize...

		tileset_number = cm[tilemap_number].tile_set_index;

		if (tileset_number != UNSET)
		{
			tilesize = TILESETS_get_tilesize(tileset_number);
		}
		else
		{
			tilesize = 16;
		}

		zoomed_tilesize = int(float(tilesize) * zoom_level);
		zone_edge_handle_size = int(float(tilesize) / zoom_level);

		// First of all, draw the zones!

		for (z=0; z<cm[tilemap_number].zones; z++)
		{
			zone_tx = cm[tilemap_number].zone_list_pointer[z].x;
			zone_ty = cm[tilemap_number].zone_list_pointer[z].y;
			zone_width = cm[tilemap_number].zone_list_pointer[z].width;
			zone_height = cm[tilemap_number].zone_list_pointer[z].height;

			cm[tilemap_number].zone_list_pointer[z].real_x = zone_tx * tilesize;
			cm[tilemap_number].zone_list_pointer[z].real_y = zone_ty * tilesize;
			cm[tilemap_number].zone_list_pointer[z].real_width = zone_width * tilesize;
			cm[tilemap_number].zone_list_pointer[z].real_height = zone_height * tilesize;

			if (MATH_rectangle_to_rectangle_intersect ( zone_tx,zone_ty,zone_tx+zone_width,zone_ty+zone_height , sx,sy,sx+(editor_view_width/(zoomed_tilesize)),sx+(editor_view_height/(zoomed_tilesize)) ) != 0)
			{
				// Draw the handles first...
				if (zone_editing_mode == ZONE_EDIT_MODE_CREATE_ZONES)
				{
					OUTPUT_circle ( (zone_tx-sx)*(zoomed_tilesize)+ZONE_HANDLE_SIZE , (zone_ty-sy)*(zoomed_tilesize)+ZONE_HANDLE_SIZE , ZONE_HANDLE_SIZE , 255,255,0 );
				}
				else if (selected_zone == z)
				{
					// Top left
					OUTPUT_centred_square ( (zone_tx-sx)*(zoomed_tilesize)-(tilesize/2) , (zone_ty-sy)*(zoomed_tilesize)-(tilesize/2) , (tilesize/2) , 0,255,0 );
					// Top right
					OUTPUT_centred_square ( (zone_tx-sx+zone_width)*(zoomed_tilesize)+(tilesize/2) , (zone_ty-sy)*(zoomed_tilesize)-(tilesize/2) , (tilesize/2) , 0,255,0 );
					// Bottom right
					OUTPUT_centred_square ( (zone_tx-sx+zone_width)*(zoomed_tilesize)+(tilesize/2) , (zone_ty-sy+zone_height)*(zoomed_tilesize)+(tilesize/2) , (tilesize/2) , 0,255,0 );
					// Bottom left
					OUTPUT_centred_square ( (zone_tx-sx)*(zoomed_tilesize)-(tilesize/2) , (zone_ty-sy+zone_height)*(zoomed_tilesize)+(tilesize/2) , (tilesize/2) , 0,255,0 );

					// Left
					OUTPUT_centred_square ( (zone_tx-sx)*(zoomed_tilesize)-(tilesize/2) , int ((zone_ty-sy+(float(zone_height)/2))*(tilesize*zoom_level)) , (tilesize/2) , 0,255,255 );
					// Right
					OUTPUT_centred_square ( (zone_tx-sx+zone_width)*(zoomed_tilesize)+(tilesize/2) , int ((zone_ty-sy+(float(zone_height)/2))*(tilesize*zoom_level)) , (tilesize/2) , 0,255,255 );
					// Top
					OUTPUT_centred_square ( int ((zone_tx-sx+(float(zone_width)/2))*(zoomed_tilesize)) , (zone_ty-sy)*(zoomed_tilesize)-(tilesize/2) , (tilesize/2) , 0,255,255 );
					// Bottom
					OUTPUT_centred_square ( int ((zone_tx-sx+(float(zone_width)/2))*(zoomed_tilesize)) , (zone_ty-sy+zone_height)*(zoomed_tilesize)+(tilesize/2) , (tilesize/2) , 0,255,255 );
				}

				// Then the box itself so that's the dominant part.
				if (zone_editing_mode == ZONE_EDIT_MODE_CREATE_ZONES)
				{
					// If creating then draw them all in green.
					OUTPUT_rectangle ( (zone_tx-sx)*(zoomed_tilesize) , (zone_ty-sy)*(zoomed_tilesize) , (zone_tx-sx+zone_width)*(zoomed_tilesize) , (zone_ty-sy+zone_height)*(zoomed_tilesize) , 0 , 255 , 0 );
				}
				else
				{
					// If we ain't then draw the selected one in white and the rest in dark blue.
					if (z == selected_zone)
					{
						OUTPUT_rectangle ( (zone_tx-sx)*(zoomed_tilesize) , (zone_ty-sy)*(zoomed_tilesize) , (zone_tx-sx+zone_width)*(zoomed_tilesize) , (zone_ty-sy+zone_height)*(zoomed_tilesize) , 255 , 255 , 255 );
					}
					else
					{
						OUTPUT_rectangle ( (zone_tx-sx)*(zoomed_tilesize) , (zone_ty-sy)*(zoomed_tilesize) , (zone_tx-sx+zone_width)*(zoomed_tilesize) , (zone_ty-sy+zone_height)*(zoomed_tilesize) , 0 , 0 , 255 );
					}
				}
			}
		}

		// Display overlay

		if (overlay_display == true)
		{
			SIMPGUI_wake_group (TILEMAP_EDITOR_SUB_GROUP);

			editor_view_width = game_screen_width - (ICON_SIZE*8);
			editor_view_height = game_screen_height;

			OUTPUT_filled_rectangle (editor_view_width,0,game_screen_width,game_screen_height,0,0,0);

			OUTPUT_vline (editor_view_width,0,game_screen_height,255,255,255);

			OUTPUT_centred_text ( editor_view_width+(ICON_SIZE*4) , (ICON_SIZE-FONT_HEIGHT)/2 , "GAME ZONE EDITOR" );

			if (selected_zone == UNSET)
			{
				OUTPUT_centred_text ( editor_view_width+(ICON_SIZE*4) , ICON_SIZE + (ICON_SIZE-FONT_HEIGHT)/2 , "NO ZONE SELECTED" );

				OUTPUT_draw_masked_sprite ( first_icon , AUTO_ZONE_ICON, editor_view_width+(ICON_SIZE/2) , ICON_SIZE*2 );
				OUTPUT_draw_masked_sprite ( first_icon , BIG_YES_ICON, editor_view_width+(ICON_SIZE/2)+ICON_SIZE , ICON_SIZE*2 );
			}
			else
			{
				sprintf (text_line,"ZONE %4d SELECTED",selected_zone);
				OUTPUT_centred_text ( editor_view_width+(ICON_SIZE*4) , ICON_SIZE + (FONT_HEIGHT*0) , text_line );

				sprintf (text_line,"     X = %5d" , (cm[tilemap_number].zone_list_pointer[selected_zone].x)*tilesize );
				OUTPUT_centred_text ( editor_view_width+(ICON_SIZE*4) , ICON_SIZE + (FONT_HEIGHT*2) , text_line );

				sprintf (text_line,"     Y = %5d" , (cm[tilemap_number].zone_list_pointer[selected_zone].y)*tilesize );
				OUTPUT_centred_text ( editor_view_width+(ICON_SIZE*4) , ICON_SIZE + (FONT_HEIGHT*3) , text_line );

				sprintf (text_line," WIDTH = %5d" , (cm[tilemap_number].zone_list_pointer[selected_zone].width)*tilesize );
				OUTPUT_centred_text ( editor_view_width+(ICON_SIZE*4) , ICON_SIZE + (FONT_HEIGHT*4) , text_line );

				sprintf (text_line,"HEIGHT = %5d" , (cm[tilemap_number].zone_list_pointer[selected_zone].height)*tilesize );
				OUTPUT_centred_text ( editor_view_width+(ICON_SIZE*4) , ICON_SIZE + (FONT_HEIGHT*5) , text_line );

				OUTPUT_draw_masked_sprite ( first_icon , ZONE_NAME_ICON, editor_view_width+(ICON_SIZE/2) , ICON_SIZE*3 );
				TILEMAPS_dialogue_box ( editor_view_width+(ICON_SIZE/2)+(ICON_SIZE) , ICON_SIZE*3 , ICON_SIZE*6, ICON_SIZE*1 , cm[tilemap_number].zone_list_pointer[selected_zone].text_tag );

				OUTPUT_draw_masked_sprite ( first_icon , ZONE_TYPE_ICON, editor_view_width+(ICON_SIZE/2) , ICON_SIZE*4 );
				TILEMAPS_dialogue_box ( editor_view_width+(ICON_SIZE/2)+(ICON_SIZE) , ICON_SIZE*4 , ICON_SIZE*6, ICON_SIZE*1 , cm[tilemap_number].zone_list_pointer[selected_zone].type );

				OUTPUT_draw_masked_sprite ( first_icon , ERASE_ZONE_ICON, editor_view_width+(ICON_SIZE/2) , ICON_SIZE*6 );
				OUTPUT_draw_masked_sprite ( first_icon , BIG_YES_ICON, editor_view_width+(ICON_SIZE/2)+ICON_SIZE , ICON_SIZE*6 );
			}

			SIMPGUI_check_all_buttons ();
			SIMPGUI_draw_all_buttons ();
		}
		else
		{
			SIMPGUI_sleep_group (TILEMAP_EDITOR_SUB_GROUP);
			editor_view_width = game_screen_width;
			editor_view_height = game_screen_height;
		}

		tx = mx / (zoomed_tilesize);
		ty = my / (zoomed_tilesize);

		real_tx = tx + sx;
		real_ty = ty + sy;

		if (replacement_cursor != UNSET)
		{
			*current_cursor = replacement_cursor;
		}

		// Clicks inside the overlay

		if (menu_override == true)
		{

			if (auto_zone_override == false)
			{
				// GPL MENU OVERLAY!

				if (setting_property == SETTING_PROPERTY_TEXT_TAG)
				{
					menu_override = EDIT_gpl_list_menu (80, 48, text_tag_name, cm[tilemap_number].zone_list_pointer[selected_zone].text_tag , true);
				}

				if (setting_property == SETTING_PROPERTY_TYPE)
				{
					menu_override = EDIT_gpl_list_menu (80, 48, type_name, cm[tilemap_number].zone_list_pointer[selected_zone].type , true);
				}

				if (menu_override == false)
				{
					// Yay! We've chosen one! Relink the text_tag and type.
					cm[tilemap_number].zone_list_pointer[selected_zone].text_tag_index = GPL_find_word_value (text_tag_name , cm[tilemap_number].zone_list_pointer[selected_zone].text_tag );
					cm[tilemap_number].zone_list_pointer[selected_zone].type_index = GPL_find_word_value (type_name , cm[tilemap_number].zone_list_pointer[selected_zone].type );

					// Lock the LMB so we don't accidentally place tiles until we've released the LMB after this selection.
					CONTROL_lock_mouse_button (LMB);

					setting_property = UNSET;
				}

			}
			else
			{
				// AUTO ZONE OVERLAY!
				
				menu_override = ROOMZONES_autozone_dialogue (tilemap_number , &autozoning_progress, mx , my , 128,128);
				
			}

		}
		else if (mx >= editor_view_width)
		{

			if (selected_zone != UNSET)
			{
				if (CONTROL_mouse_hit(LMB))
				{
					if (MATH_rectangle_to_point_intersect_by_size ( editor_view_width+(ICON_SIZE/2) , ICON_SIZE*3 , ICON_SIZE*7 , ICON_SIZE , mx , my ) == true )
					{
						// Rename zone.
						menu_override = true;
						auto_zone_override = false;
						setting_property = SETTING_PROPERTY_TEXT_TAG;
					}

					if (MATH_rectangle_to_point_intersect_by_size ( editor_view_width+(ICON_SIZE/2) , ICON_SIZE*4 , ICON_SIZE*7 , ICON_SIZE , mx , my ) == true )
					{
						// Rename zone.
						menu_override = true;
						auto_zone_override = false;
						setting_property = SETTING_PROPERTY_TYPE;
					}

					if (MATH_rectangle_to_point_intersect_by_size ( editor_view_width+(ICON_SIZE/2) , ICON_SIZE*6 , ICON_SIZE*2 , ICON_SIZE , mx , my ) == true )
					{
						// Delete zone
						ROOMZONES_delete_particular_zone(tilemap_number,selected_zone);
						selected_zone = UNSET;
						zone_editing_mode = ZONE_EDIT_MODE_CREATE_ZONES;
					}

				}
			}
			else
			{
				if (CONTROL_mouse_hit(LMB))
				{
					if (MATH_rectangle_to_point_intersect_by_size ( editor_view_width+(ICON_SIZE/2) , ICON_SIZE*2 , ICON_SIZE*2 , ICON_SIZE , mx , my ) == true )
					{
						// AUTOZONING!!!
						menu_override = true;
						auto_zone_override = true;
						autozoning_progress = 0;
					}
				}
			}

		}
		else
		{
			if (zone_editing_mode == ZONE_EDIT_MODE_CREATE_ZONES)
			{
				if (CONTROL_mouse_hit(LMB) == true)
				{
					if (ROOMZONES_check_zone_handle ( tilemap_number , int(float(mx)/zoom_level)+(sx*tilesize) , int(float(my)/zoom_level)+(sy*tilesize) , zoom_level ) != UNSET)
					{
						// Have we clicked upon the handle of a zone, therefor selecting it?
						zone_editing_mode = ZONE_EDIT_MODE_RESIZE_ZONES;
						selected_zone = ROOMZONES_check_zone_handle ( tilemap_number , int(float(mx)/zoom_level)+(sx*tilesize) , int(float(my)/zoom_level)+(sy*tilesize) , zoom_level  );
						grabbed_zone_handle = UNSET;
					}
					else
					{
						// Start the zone
						drawing_zone = true;
						making_zone_tx = real_tx;
						making_zone_ty = real_ty;
					}
				}

				if ( (CONTROL_mouse_down(LMB) == true) && (drawing_zone == true) )
				{
					selection_box_x = (making_zone_tx - sx) * zoomed_tilesize;
					selection_box_y = (making_zone_ty - sy) * zoomed_tilesize;
					selection_box_width = (real_tx - making_zone_tx) * zoomed_tilesize;
					selection_box_height = (real_ty - making_zone_ty) * zoomed_tilesize;

					if (selection_box_width<0)
					{
						selection_box_x += selection_box_width;
						selection_box_width *= -1;
					}

					if (selection_box_height<0)
					{
						selection_box_y += selection_box_height;
						selection_box_height *= -1;
					}

					OUTPUT_rectangle (selection_box_x, selection_box_y, selection_box_x+selection_box_width+(zoomed_tilesize), selection_box_y+selection_box_height+(zoomed_tilesize), 255, 255, 255);
				}

				if ( (CONTROL_mouse_down(LMB) == false) && (drawing_zone == true) )
				{
					// Finished making the zone...
					making_zone_width = real_tx - making_zone_tx;
					making_zone_height = real_ty - making_zone_ty;

					drawing_zone = false;

					if (making_zone_width<0)
					{
						making_zone_tx += making_zone_width;
						making_zone_width *= -1;
					}

					if (making_zone_height<0)
					{
						making_zone_ty += making_zone_height;
						making_zone_height *= -1;
					}

					making_zone_width++;
					making_zone_height++;

					ROOMZONES_create_zone(tilemap_number,making_zone_tx,making_zone_ty,making_zone_width,making_zone_height);
				}


				if ( (CONTROL_key_hit(KEY_DEL)) && (selected_zone != UNSET) )
				{
					// Delete zone
					ROOMZONES_delete_particular_zone(tilemap_number,selected_zone);
					selected_zone = UNSET;
					zone_editing_mode = ZONE_EDIT_MODE_CREATE_ZONES;
				}

			}
			else if (zone_editing_mode == ZONE_EDIT_MODE_RESIZE_ZONES)
			{
				// In this mode you can either resize or move the map.
				// First of all allow you to grab a handle.

				if (grabbed_zone_handle == UNSET)
				{
					real_zone_x = cm[tilemap_number].zone_list_pointer[selected_zone].x * tilesize;
					real_zone_y = cm[tilemap_number].zone_list_pointer[selected_zone].y * tilesize;
					real_zone_width = cm[tilemap_number].zone_list_pointer[selected_zone].width * tilesize;
					real_zone_height = cm[tilemap_number].zone_list_pointer[selected_zone].height * tilesize;
					real_mx = (sx * tilesize) + int(float(mx) / zoom_level);
					real_my = (sy * tilesize) + int(float(my) / zoom_level);

					proposed_grabbed_zone_handle = UNSET;
					replacement_cursor = UNSET;

					if (MATH_rectangle_to_point_intersect ( real_zone_x-zone_edge_handle_size,real_zone_y-zone_edge_handle_size,real_zone_x,real_zone_y , real_mx, real_my ) == true)
					{
						proposed_grabbed_zone_handle = ZONE_GRABBED_TOP_LEFT;
						replacement_cursor = DIAG_ARROW_1_ICON;
					}
					else if (MATH_rectangle_to_point_intersect ( real_zone_x,real_zone_y-zone_edge_handle_size,real_zone_x+real_zone_width,real_zone_y , real_mx, real_my ) == true)
					{
						proposed_grabbed_zone_handle = ZONE_GRABBED_TOP;
						replacement_cursor = VERT_ARROW_ICON;
					}
					else if (MATH_rectangle_to_point_intersect ( real_zone_x+real_zone_width,real_zone_y-zone_edge_handle_size,real_zone_x+real_zone_width+zone_edge_handle_size,real_zone_y , real_mx, real_my ) == true)
					{
						proposed_grabbed_zone_handle = ZONE_GRABBED_TOP_RIGHT;
						replacement_cursor = DIAG_ARROW_2_ICON;
					}
					else if (MATH_rectangle_to_point_intersect ( real_zone_x+real_zone_width,real_zone_y,real_zone_x+real_zone_width+zone_edge_handle_size,real_zone_y+real_zone_height , real_mx, real_my ) == true)
					{
						proposed_grabbed_zone_handle = ZONE_GRABBED_RIGHT;
						replacement_cursor = HORI_ARROW_ICON;
					}
					else if (MATH_rectangle_to_point_intersect ( real_zone_x+real_zone_width,real_zone_y+real_zone_height,real_zone_x+real_zone_width+zone_edge_handle_size,real_zone_y+real_zone_height+zone_edge_handle_size , real_mx, real_my ) == true)
					{
						proposed_grabbed_zone_handle = ZONE_GRABBED_BOTTOM_RIGHT;
						replacement_cursor = DIAG_ARROW_1_ICON;
					}
					else if (MATH_rectangle_to_point_intersect ( real_zone_x,real_zone_y+real_zone_height,real_zone_x+real_zone_width,real_zone_y+real_zone_height+zone_edge_handle_size , real_mx, real_my ) == true)
					{
						proposed_grabbed_zone_handle = ZONE_GRABBED_BOTTOM;
						replacement_cursor = VERT_ARROW_ICON;
					}
					else if (MATH_rectangle_to_point_intersect ( real_zone_x-zone_edge_handle_size,real_zone_y+real_zone_height,real_zone_x,real_zone_y+real_zone_height+zone_edge_handle_size , real_mx, real_my ) == true)
					{
						proposed_grabbed_zone_handle = ZONE_GRABBED_BOTTOM_LEFT;
						replacement_cursor = DIAG_ARROW_2_ICON;
					}
					else if (MATH_rectangle_to_point_intersect ( real_zone_x-zone_edge_handle_size,real_zone_y,real_zone_x,real_zone_y+real_zone_height , real_mx, real_my ) == true)
					{
						proposed_grabbed_zone_handle = ZONE_GRABBED_LEFT;
						replacement_cursor = HORI_ARROW_ICON;
					}
					else if (MATH_rectangle_to_point_intersect ( real_zone_x,real_zone_y,real_zone_x+real_zone_width,real_zone_y+real_zone_height , real_mx, real_my ) == true)
					{
						proposed_grabbed_zone_handle = ZONE_GRABBED_BODY;
						grabbed_real_tx = real_tx;
						grabbed_real_ty = real_ty;
						replacement_cursor = OMNI_ARROW_ICON;
					}

					if (CONTROL_mouse_hit(LMB) == true)
					{
						if (proposed_grabbed_zone_handle == UNSET)
						{
							// Clicked outside of everything so deselect zone and return to creation mode.
							zone_editing_mode = ZONE_EDIT_MODE_CREATE_ZONES;
							selected_zone = UNSET;
						}
						else
						{
							grabbed_zone_handle = proposed_grabbed_zone_handle;
						}
					}

				}
				else
				{

					if (CONTROL_mouse_down(LMB))
					{
						// Difference between current top of zone and cursor
						diff_tx1 = ((cm[tilemap_number].zone_list_pointer[selected_zone].x) - real_tx) - 1;
						// Difference between current left of zone and cursor
						diff_ty1 = ((cm[tilemap_number].zone_list_pointer[selected_zone].y) - real_ty) - 1;
						// Difference between current right of zone and cursor
						diff_tx2 = (cm[tilemap_number].zone_list_pointer[selected_zone].x + cm[tilemap_number].zone_list_pointer[selected_zone].width) - real_tx;
						// Difference between current bottom of zone and cursor
						diff_ty2 = (cm[tilemap_number].zone_list_pointer[selected_zone].y + cm[tilemap_number].zone_list_pointer[selected_zone].height) - real_ty;

						switch (grabbed_zone_handle)
						{
						case ZONE_GRABBED_BODY:
							cm[tilemap_number].zone_list_pointer[selected_zone].x -= (grabbed_real_tx - real_tx);
							cm[tilemap_number].zone_list_pointer[selected_zone].y -= (grabbed_real_ty - real_ty);
							grabbed_real_tx = real_tx;
							grabbed_real_ty = real_ty;
							break;

						case ZONE_GRABBED_TOP_LEFT:
							cm[tilemap_number].zone_list_pointer[selected_zone].x -= diff_tx1;
							cm[tilemap_number].zone_list_pointer[selected_zone].width += diff_tx1;
							cm[tilemap_number].zone_list_pointer[selected_zone].y -= diff_ty1;
							cm[tilemap_number].zone_list_pointer[selected_zone].height += diff_ty1;
							break;
							
						case ZONE_GRABBED_TOP:
							cm[tilemap_number].zone_list_pointer[selected_zone].y -= diff_ty1;
							cm[tilemap_number].zone_list_pointer[selected_zone].height += diff_ty1;
							break;

						case ZONE_GRABBED_TOP_RIGHT:
							cm[tilemap_number].zone_list_pointer[selected_zone].width -= diff_tx2;
							cm[tilemap_number].zone_list_pointer[selected_zone].y -= diff_ty1;
							cm[tilemap_number].zone_list_pointer[selected_zone].height += diff_ty1;
							break;

						case ZONE_GRABBED_RIGHT:
							cm[tilemap_number].zone_list_pointer[selected_zone].width -= diff_tx2;
							break;

						case ZONE_GRABBED_BOTTOM_RIGHT:
							cm[tilemap_number].zone_list_pointer[selected_zone].width -= diff_tx2;
							cm[tilemap_number].zone_list_pointer[selected_zone].height -= diff_ty2;
							break;

						case ZONE_GRABBED_BOTTOM:
							cm[tilemap_number].zone_list_pointer[selected_zone].height -= diff_ty2;
							break;

						case ZONE_GRABBED_BOTTOM_LEFT:
							cm[tilemap_number].zone_list_pointer[selected_zone].height -= diff_ty2;
							cm[tilemap_number].zone_list_pointer[selected_zone].x -= diff_tx1;
							cm[tilemap_number].zone_list_pointer[selected_zone].width += diff_tx1;
							break;

						case ZONE_GRABBED_LEFT:
							cm[tilemap_number].zone_list_pointer[selected_zone].x -= diff_tx1;
							cm[tilemap_number].zone_list_pointer[selected_zone].width += diff_tx1;
							break;

						default:
							break;
						}

						// It's possible that the zone could have been pushed outside the extents of the map
						// in which case it needs *ahem* bringing down to size.

						if (cm[tilemap_number].zone_list_pointer[selected_zone].x < 0)
						{
							cm[tilemap_number].zone_list_pointer[selected_zone].width += cm[tilemap_number].zone_list_pointer[selected_zone].x;
							cm[tilemap_number].zone_list_pointer[selected_zone].x -= cm[tilemap_number].zone_list_pointer[selected_zone].x;
						}

						if (cm[tilemap_number].zone_list_pointer[selected_zone].y < 0)
						{
							cm[tilemap_number].zone_list_pointer[selected_zone].height += cm[tilemap_number].zone_list_pointer[selected_zone].y;
							cm[tilemap_number].zone_list_pointer[selected_zone].y -= cm[tilemap_number].zone_list_pointer[selected_zone].y;
						}

						if (cm[tilemap_number].zone_list_pointer[selected_zone].x + cm[tilemap_number].zone_list_pointer[selected_zone].width > cm[tilemap_number].map_width)
						{
							cm[tilemap_number].zone_list_pointer[selected_zone].width -= ( (cm[tilemap_number].zone_list_pointer[selected_zone].x + cm[tilemap_number].zone_list_pointer[selected_zone].width) - cm[tilemap_number].map_width );
						}

						if (cm[tilemap_number].zone_list_pointer[selected_zone].y + cm[tilemap_number].zone_list_pointer[selected_zone].height > cm[tilemap_number].map_height)
						{
							cm[tilemap_number].zone_list_pointer[selected_zone].height -= ( (cm[tilemap_number].zone_list_pointer[selected_zone].y + cm[tilemap_number].zone_list_pointer[selected_zone].height) - cm[tilemap_number].map_height );
						}

						// Now it's also possible the zone could have been inverted during that stage or by
						// the user so we need to stop that happening.

						if (cm[tilemap_number].zone_list_pointer[selected_zone].width <= 0)
						{
							if (cm[tilemap_number].zone_list_pointer[selected_zone].x > 0)
							{
								cm[tilemap_number].zone_list_pointer[selected_zone].x += (cm[tilemap_number].zone_list_pointer[selected_zone].width-1);
								cm[tilemap_number].zone_list_pointer[selected_zone].width -= (cm[tilemap_number].zone_list_pointer[selected_zone].width-1);
							}
							else
							{
								// If it's off the top of the map then the above method won't work as it'll attempt to expand
								// the box leftwards which'll keep it off-map so instead we just set the width to 1.
								cm[tilemap_number].zone_list_pointer[selected_zone].width = 1;
							}
						}

						if (cm[tilemap_number].zone_list_pointer[selected_zone].height <= 0)
						{
							if (cm[tilemap_number].zone_list_pointer[selected_zone].y > 0)
							{
								cm[tilemap_number].zone_list_pointer[selected_zone].y += (cm[tilemap_number].zone_list_pointer[selected_zone].height-1);
								cm[tilemap_number].zone_list_pointer[selected_zone].height -= (cm[tilemap_number].zone_list_pointer[selected_zone].height-1);
							}
							else
							{
								// If it's off the top of the map then the above method won't work as it'll attempt to expand
								// the box upwards which'll keep it off-map so instead we just set the height to 1.
								cm[tilemap_number].zone_list_pointer[selected_zone].height = 1;
							}
						}


					}
					else
					{
						grabbed_zone_handle = UNSET;
					}

				}
				
			}

		}


	}
	else if (state == STATE_RESET_BUTTONS)
	{
		SIMPGUI_kill_group (TILEMAP_EDITOR_SUB_GROUP);
	}
	else if (state == STATE_SHUTDOWN)
	{
		SIMPGUI_kill_group (TILEMAP_EDITOR_SUB_GROUP);
	}

	return menu_override;
}



void ROOMZONES_generate_localised_zone_lists (void)
{
	int tilemap_number;
	int width_in_lzones,height_in_lzones;
	int lzone_total;
	int lzone_number;
	int zone_number;
	int lz_x,lz_y;
	int z_x,z_y,z_width,z_height;
	int counter;
	int tileset_index;
	int tilesize;


	for (tilemap_number=0;tilemap_number<number_of_tilemaps_loaded;tilemap_number++)
	{
		tileset_index = cm[tilemap_number].tile_set_index;
		tilesize = TILESETS_get_tilesize(tileset_index);
		if (tilesize <= 0)
		{
			tilesize = 1;
		}

		width_in_lzones = ((cm[tilemap_number].map_width * tilesize) / localised_zone_divider) + 1;
		height_in_lzones = ((cm[tilemap_number].map_height * tilesize) / localised_zone_divider) + 1;

		lzone_total = width_in_lzones * height_in_lzones;

		cm[tilemap_number].localised_zone_list_size = lzone_total;
		cm[tilemap_number].map_width_in_lzones = width_in_lzones;

		cm[tilemap_number].localised_zone_list_pointer = (localised_zone_list *) malloc (sizeof(localised_zone_list) * lzone_total);

		for (lzone_number=0; lzone_number<lzone_total; lzone_number++)
		{
			lz_x = (lzone_number % width_in_lzones) * localised_zone_divider;
			lz_y = (lzone_number / width_in_lzones) * localised_zone_divider;


			// Count the number of zones which overlap with each lzone...
			counter = 0;

			for (zone_number=0; zone_number<cm[tilemap_number].zones; zone_number++)
			{
				z_x = cm[tilemap_number].zone_list_pointer[zone_number].real_x;
				z_y = cm[tilemap_number].zone_list_pointer[zone_number].real_y;
				z_width = cm[tilemap_number].zone_list_pointer[zone_number].real_width;
				z_height = cm[tilemap_number].zone_list_pointer[zone_number].real_height;

				if (MATH_rectangle_to_rectangle_intersect (lz_x,lz_y,lz_x+localised_zone_divider,lz_y+localised_zone_divider , z_x,z_y,z_x+z_width,z_y+z_height) == true)
				{
					counter++;
				}
			}

			// First of all blank it all...
			cm[tilemap_number].localised_zone_list_pointer[lzone_number].list_size = counter;
			cm[tilemap_number].localised_zone_list_pointer[lzone_number].indexes = (int *) malloc (counter * sizeof(int));

			// Then bung their indexes into the local list...
			counter = 0;

			for (zone_number=0; zone_number<cm[tilemap_number].zones; zone_number++)
			{
				z_x = cm[tilemap_number].zone_list_pointer[zone_number].real_x;
				z_y = cm[tilemap_number].zone_list_pointer[zone_number].real_y;
				z_width = cm[tilemap_number].zone_list_pointer[zone_number].real_width;
				z_height = cm[tilemap_number].zone_list_pointer[zone_number].real_height;

				if (MATH_rectangle_to_rectangle_intersect (lz_x,lz_y,lz_x+localised_zone_divider,lz_y+localised_zone_divider , z_x,z_y,z_x+z_width,z_y+z_height) == true)
				{
					cm[tilemap_number].localised_zone_list_pointer[lzone_number].indexes[counter] = zone_number;
					counter++;
				}
			}

		}
		
	}

}



localised_zone_list * ROOMZONES_get_localised_list_size_and_pointer (int tilemap_number, int x, int y)
{
	x /= localised_zone_divider;
	y /= localised_zone_divider;

	int offset = (cm[tilemap_number].map_width_in_lzones * y) + x;

	return &cm[tilemap_number].localised_zone_list_pointer[offset];
}



void ROOMZONES_set_localised_zone_divider_size (int new_size)
{
	localised_zone_divider = new_size;
}



void ROOMZONES_set_zone_flag_by_uid (int map_uid, int zone_uid, int flag_value)
{
	int zone_count;
	int zone_number;
	int tilemap_number = TILEMAPS_get_map_from_uid(map_uid);

	if (tilemap_number != UNSET)
	{
		zone_count = cm[tilemap_number].zones;
		for (zone_number=0; zone_number<zone_count; zone_number++)
		{
			if (zone_uid == cm[tilemap_number].zone_list_pointer[zone_number].uid)
			{
				cm[tilemap_number].zone_list_pointer[zone_number].flag = flag_value;
			}
		}
	}
}
