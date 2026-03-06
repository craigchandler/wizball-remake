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

#include "fortify.h"



char TILEGROUPS_read_group_tile (int tilemap_number, int layer, int tx, int ty)
{
	int data_offset;
	int map_layers = cm[tilemap_number].map_layers;
	int map_width = cm[tilemap_number].map_width;
	int map_height = cm[tilemap_number].map_height;

	if ( (layer >= 0) && (layer < map_layers) && (tx >= 0) && (tx < map_width) && (ty >= 0) && (ty < map_height) )
	{
		data_offset = (layer * map_height * map_width) + (ty * map_width) + tx;
		return cm[tilemap_number].group_pointer[data_offset];
	}
	else
	{
		return UNSET;
	}
}



void TILEGROUPS_write_group_tile (int tilemap_number, int layer, int tx, int ty, char tile_value)
{
	int data_offset;
	int map_layers = cm[tilemap_number].map_layers;
	int map_width = cm[tilemap_number].map_width;
	int map_height = cm[tilemap_number].map_height;

	if ( (layer >= 0) && (layer < map_layers) && (tx >= 0) && (tx < map_width) && (ty >= 0) && (ty < map_height) )
	{
		data_offset = (layer * map_height * map_width) + (ty * map_width) + tx;
		cm[tilemap_number].group_pointer[data_offset] = tile_value;
	}

}



void TILEGROUPS_create_group (int tilemap_number, int layer)
{
	// This goes through the map array looking for GROUP_CONVERSION_TILE_NUMBER number tiles and
	// creates a list entry for it and then burns all those list entries into the map.
	
	fill_spreader *tile_list = NULL;
	int list_size;

	int x,y;
	int side;
	
	int map_width;
	int map_height;
	int map_depth;

	int x_offset,y_offset;

	int current_tile_value;
	int tile_value_counter;

	int t;

	map_width = cm[tilemap_number].map_width;
	map_height = cm[tilemap_number].map_height;
	map_depth = cm[tilemap_number].map_layers;

	list_size = 0;

	for (y=0; (y<map_height) ; y++)
	{
		for (x=0; (x<map_width) ; x++)
		{
			current_tile_value = TILEGROUPS_read_group_tile (tilemap_number, layer, x, y);

			if ( current_tile_value == GROUP_CONVERSION_TILE_NUMBER)
			{
				tile_value_counter = 0;

				for (side=0; side<4; side++)
				{
					if (MATH_check_4bit_within_size (side, x, y, map_width, map_height, &x_offset, &y_offset) == true)
					{
						if (TILEGROUPS_read_group_tile (tilemap_number, layer, x+x_offset, y+y_offset) == GROUP_CONVERSION_TILE_NUMBER)
						{
							tile_value_counter += MATH_pow(side);
						}
					}
				}
				
				tile_list = EDIT_add_filler (list_size,tile_list,x,y,tile_value_counter);
				list_size++;
			}
		}
	}

	for (t=0; t<list_size; t++)
	{
		TILEGROUPS_write_group_tile (tilemap_number, layer, tile_list[t].x, tile_list[t].y, tile_list[t].value);
	}

	free(tile_list);
	tile_list = NULL; // Anal, I know...
}



void TILEGROUPS_alter_group (int tilemap_number, int layer, int tx, int ty, bool reset)
{
	// Alters all of the group pointed to by layer,tx,ty to the specified value - either
	// for deleting or absorbing.

	fill_spreader *tile_list = NULL;
	int list_size;
	int read_from;
	int spreading_to_value;

	int replacement_value;

	// Store new value...
	if (reset == true)
	{
		replacement_value = 0;
	}
	else
	{
		replacement_value = GROUP_CONVERSION_TILE_NUMBER;
	}

	int x,y;
	int side;
	int x_offset,y_offset;
	
	int map_width;
	int map_height;
	int map_depth;

	int current_tile_value;

	map_width = cm[tilemap_number].map_width;
	map_height = cm[tilemap_number].map_height;
	map_depth = cm[tilemap_number].map_layers;

	list_size = 0;
	read_from = 0;

	current_tile_value = TILEGROUPS_read_group_tile (tilemap_number, layer, tx, ty);
	TILEGROUPS_write_group_tile (tilemap_number, layer, tx, ty, replacement_value);

	tile_list = EDIT_add_filler (list_size,tile_list,tx,ty,current_tile_value);
	list_size++;

	while (read_from < list_size)
	{
		x = tile_list[read_from].x;
		y = tile_list[read_from].y;

		// Get old value...
		current_tile_value = tile_list[read_from].value;

		if ( ( current_tile_value != 0 ) && ( current_tile_value != GROUP_CONVERSION_TILE_NUMBER ) )
		{

			for (side=0; side<4; side++)
			{
				if (MATH_check_4bit_direction (current_tile_value,side,&x_offset,&y_offset) == true)
				{
					spreading_to_value = TILEGROUPS_read_group_tile (tilemap_number, layer, x+x_offset, y+y_offset);
					TILEGROUPS_write_group_tile (tilemap_number, layer, x+x_offset, y+y_offset, replacement_value);
					
					tile_list = EDIT_add_filler (list_size,tile_list, x+x_offset, y+y_offset, spreading_to_value);
					list_size++;
				}
			}

		}
		
		read_from++;
	}

	free(tile_list);
	tile_list = NULL; // Anal, I know...
}



void TILEGROUPS_absorb_group (int tilemap_number, int layer, int tx, int ty)
{
	TILEGROUPS_alter_group (tilemap_number, layer, tx, ty, false);
}



void TILEGROUPS_reset_group (int tilemap_number, int layer, int tx, int ty)
{
	TILEGROUPS_alter_group (tilemap_number, layer, tx, ty, true);
}



void TILEGROUPS_draw_groups (int tilemap_number, int sx, int sy, int layer, float zoom_level)
{
	int tilesize = TILESETS_get_tilesize (cm[tilemap_number].tile_set_index);
	int x,y;
	
	int map_width;
	int map_height;
	int map_depth;

	char tile_number;

	map_width = cm[tilemap_number].map_width;
	map_height = cm[tilemap_number].map_height;
	map_depth = cm[tilemap_number].map_layers;
	
	int data_offset;

	float scale_factor = float (tilesize) / float (ICON_SIZE);

	int tiles_per_row = int(float(editor_view_width/tilesize)/zoom_level);
	int tiles_per_col = int(float(editor_view_height/tilesize)/zoom_level);

	for (y=0; (y<tiles_per_col) && (y+sy<map_height) ; y++)
	{
		for (x=0; (x<tiles_per_row) && (x+sx<map_width) ; x++)
		{
			data_offset = (layer * map_height * map_width) + ((sy+y) * map_width) + (sx+x);

			tile_number = cm[tilemap_number].group_pointer[data_offset];

			if ( ( tile_number >= 0) && ( tile_number < 16) )
			{
				OUTPUT_draw_sprite_scale (first_icon, tile_number+BLOCK_GROUPING_SIDES_START, int(float(x*tilesize)*zoom_level), int(float(y*tilesize)*zoom_level) , zoom_level*scale_factor );
			}
			else if ( tile_number == GROUP_CONVERSION_TILE_NUMBER)
			{
				OUTPUT_draw_sprite_scale (first_icon, GROUP_FLAGGING_ICON, int(float(x*tilesize)*zoom_level), int(float(y*tilesize)*zoom_level) , zoom_level*scale_factor );
			}
		}
	}

}



bool TILEGROUPS_edit_tile_grouping (int state , bool overlay_display, int *current_cursor ,int sx, int sy,  int tilemap_number, int mx, int my, float zoom_level, int grid_size, int editing_layer)
{
	int tx,ty;
	int real_tx,real_ty;

	int tileset;
	int tilesize;

	char temp_group_value;

	static int old_real_tx;
	static int old_real_ty;

	char word_string[TEXT_LINE_SIZE];

	char tile_value;

	static bool precise_mode;

	static bool creating_group;

	if (state == STATE_INITIALISE)
	{
		creating_group = false;
		precise_mode = false;
	}
	else if (state == STATE_SET_UP_BUTTONS)
	{
		editor_view_width = game_screen_width;
		editor_view_height = game_screen_height - (3*ICON_SIZE);

		SIMPGUI_create_nudge_button (TILEMAP_EDITOR_SUB_GROUP, EDIT_TILEMAP, &editing_layer, (1*ICON_SIZE) , editor_view_height+(ICON_SIZE/2)+ICON_SIZE, first_icon , LEFT_ARROW_ICON, LMB, 0, 128, -1, false);
		SIMPGUI_create_nudge_button (TILEMAP_EDITOR_SUB_GROUP, EDIT_TILEMAP, &editing_layer, (4*ICON_SIZE) , editor_view_height+(ICON_SIZE/2)+ICON_SIZE, first_icon , RIGHT_ARROW_ICON, LMB, 0, 128, 1, false);

		SIMPGUI_sleep_group (TILEMAP_EDITOR_SUB_GROUP);
	}
	else if (state == STATE_RUNNING)
	{
		if (editing_layer >= cm[tilemap_number].map_layers)
		{
			editing_layer = cm[tilemap_number].map_layers-1;
		}

		tileset = cm[tilemap_number].tile_set_index;

		if (tileset != UNSET)
		{
			tilesize = TILESETS_get_tilesize (tileset);
		}
		else
		{
			tilesize = 16;
		}

		float scale_factor = float (tilesize) / float (ICON_SIZE);

		tx = mx/int(float(tilesize)*zoom_level);
		ty = my/int(float(tilesize)*zoom_level);

		real_tx = sx+tx;
		real_ty = sy+ty;

		if (tileset != UNSET)
		{
			TILEMAPS_draw (editing_layer,editing_layer+1,tilemap_number,sx,sy,editor_view_width,editor_view_height,zoom_level, COLOUR_DISPLAY_MODE_NONE);
			TILEMAPS_draw_guides ( tilemap_number , sx, sy, tilesize, zoom_level, guide_box_width , guide_box_height );
			TILEGROUPS_draw_groups ( tilemap_number, sx, sy, editing_layer, zoom_level );
		}

		if (overlay_display == true)
		{
			editor_view_width = game_screen_width;
			editor_view_height = game_screen_height - (3*ICON_SIZE);

			SIMPGUI_wake_group (TILEMAP_EDITOR_SUB_GROUP);

			editor_view_width = game_screen_width;
			editor_view_height = game_screen_height-(3*ICON_SIZE);

			OUTPUT_filled_rectangle (0,editor_view_height,game_screen_width,game_screen_height,0,0,0);
			
			TILEMAPS_dialogue_box ( (1*ICON_SIZE) , editor_view_height+(ICON_SIZE/2) , ICON_SIZE*4, ICON_SIZE , "EDITING LAYER");

			snprintf (word_string, sizeof(word_string), "%3d", editing_layer);
			TILEMAPS_dialogue_box ( (2*ICON_SIZE) , editor_view_height+(ICON_SIZE/2)+ICON_SIZE , ICON_SIZE*2, ICON_SIZE , word_string);

			OUTPUT_hline (0,editor_view_height,game_screen_width,255,255,255);

			OUTPUT_centred_text ( editor_view_width/2 , editor_view_height+(ICON_SIZE-FONT_HEIGHT)/2 , "BLOCK GROUPING EDITOR" );

			if (precise_mode == true)
			{
				TILEMAPS_dialogue_box ( editor_view_width-(5*ICON_SIZE) , editor_view_height+(ICON_SIZE/2) , ICON_SIZE*4, ICON_SIZE , "PRECISE EDIT");
			}
			else
			{
				TILEMAPS_dialogue_box ( editor_view_width-(5*ICON_SIZE) , editor_view_height+(ICON_SIZE/2) , ICON_SIZE*4, ICON_SIZE , "FAST EDITING");
			}

			SIMPGUI_check_all_buttons();
			SIMPGUI_draw_all_buttons();
		}
		else
		{
			editor_view_width = game_screen_width;
			editor_view_height = game_screen_height;
		}

		TILEMAPS_draw_guides(tilemap_number,sx,sy,tilesize,zoom_level,guide_box_width,guide_box_height);

		if (my<editor_view_height)
		{
			// Clicks in the world...

			tile_value = TILEGROUPS_read_group_tile (tilemap_number, editing_layer, real_tx, real_ty);

			if (precise_mode == false)
			{

				if (CONTROL_mouse_down(LMB) == true)
				{
					creating_group = true;
					if ( (tile_value >=0) && (tile_value <16) )
					{
						TILEGROUPS_absorb_group (tilemap_number, editing_layer, real_tx, real_ty);
					}
				}
				else if (creating_group == true)
				{
					creating_group = false;
					TILEGROUPS_create_group (tilemap_number,editing_layer);
				}

				if (CONTROL_mouse_down(RMB) == true)
				{
					if ( (tile_value >=0) && (tile_value <16) )
					{
						TILEGROUPS_reset_group (tilemap_number, editing_layer, real_tx, real_ty);
					}
				}

			}
			else
			{

				if (CONTROL_mouse_hit(LMB) == true)
				{
					old_real_tx = real_tx;
					old_real_ty = real_ty;
				}

				if (CONTROL_mouse_down(LMB) == true)
				{
					if ( (real_tx == old_real_tx+1) && (real_ty == old_real_ty) )
					{
						// Dragged to the right of the previous block...
						if (real_tx < cm[tilemap_number].map_width)
						{
							// Check we ain't dragged outside of the map.

							// Good, so OR this block's group value with LEFT and the previous one with RIGHT.

							temp_group_value = TILEGROUPS_read_group_tile (tilemap_number, editing_layer, real_tx, real_ty);
							temp_group_value |= FOUR_BIT_LEFT;
							TILEGROUPS_write_group_tile (tilemap_number,editing_layer,real_tx,real_ty,temp_group_value);

							temp_group_value = TILEGROUPS_read_group_tile (tilemap_number, editing_layer, old_real_tx, old_real_ty);
							temp_group_value |= FOUR_BIT_RIGHT;
							TILEGROUPS_write_group_tile (tilemap_number,editing_layer,old_real_tx,old_real_ty,temp_group_value);

							old_real_tx = real_tx;
							old_real_ty = real_ty;
						}
					}

					if ( (real_tx == old_real_tx-1) && (real_ty == old_real_ty) )
					{
						// Dragged to the right of the previous block...
						if (real_tx < cm[tilemap_number].map_width)
						{
							// Check we ain't dragged outside of the map.

							// Good, so OR this block's group value with LEFT and the previous one with RIGHT.

							temp_group_value = TILEGROUPS_read_group_tile (tilemap_number, editing_layer, real_tx, real_ty);
							temp_group_value |= FOUR_BIT_RIGHT;
							TILEGROUPS_write_group_tile (tilemap_number,editing_layer,real_tx,real_ty,temp_group_value);

							temp_group_value = TILEGROUPS_read_group_tile (tilemap_number, editing_layer, old_real_tx, old_real_ty);
							temp_group_value |= FOUR_BIT_LEFT;
							TILEGROUPS_write_group_tile (tilemap_number,editing_layer,old_real_tx,old_real_ty,temp_group_value);

							old_real_tx = real_tx;
							old_real_ty = real_ty;
						}
					}

					if ( (real_tx == old_real_tx) && (real_ty == old_real_ty+1) )
					{
						// Dragged to the right of the previous block...
						if (real_tx < cm[tilemap_number].map_width)
						{
							// Check we ain't dragged outside of the map.

							// Good, so OR this block's group value with LEFT and the previous one with RIGHT.

							temp_group_value = TILEGROUPS_read_group_tile (tilemap_number, editing_layer, real_tx, real_ty);
							temp_group_value |= FOUR_BIT_TOP;
							TILEGROUPS_write_group_tile (tilemap_number,editing_layer,real_tx,real_ty,temp_group_value);

							temp_group_value = TILEGROUPS_read_group_tile (tilemap_number, editing_layer, old_real_tx, old_real_ty);
							temp_group_value |= FOUR_BIT_BOTTOM;
							TILEGROUPS_write_group_tile (tilemap_number,editing_layer,old_real_tx,old_real_ty,temp_group_value);

							old_real_tx = real_tx;
							old_real_ty = real_ty;
						}
					}

					if ( (real_tx == old_real_tx) && (real_ty == old_real_ty-1) )
					{
						// Dragged to the right of the previous block...
						if (real_tx < cm[tilemap_number].map_width)
						{
							// Check we ain't dragged outside of the map.

							// Good, so OR this block's group value with LEFT and the previous one with RIGHT.

							temp_group_value = TILEGROUPS_read_group_tile (tilemap_number, editing_layer, real_tx, real_ty);
							temp_group_value |= FOUR_BIT_BOTTOM;
							TILEGROUPS_write_group_tile (tilemap_number,editing_layer,real_tx,real_ty,temp_group_value);

							temp_group_value = TILEGROUPS_read_group_tile (tilemap_number, editing_layer, old_real_tx, old_real_ty);
							temp_group_value |= FOUR_BIT_TOP;
							TILEGROUPS_write_group_tile (tilemap_number,editing_layer,old_real_tx,old_real_ty,temp_group_value);

							old_real_tx = real_tx;
							old_real_ty = real_ty;
						}
					}

				}

				if (CONTROL_mouse_down(RMB) == true)
				{
					TILEGROUPS_write_group_tile (tilemap_number,editing_layer,real_tx,real_ty,0);
					
					if (real_tx>0)
					{
						temp_group_value = TILEGROUPS_read_group_tile (tilemap_number, editing_layer, real_tx-1, real_ty);
						temp_group_value &= ~FOUR_BIT_RIGHT;
						TILEGROUPS_write_group_tile (tilemap_number,editing_layer,real_tx-1,real_ty,temp_group_value);
					}

					if (real_ty>0)
					{
						temp_group_value = TILEGROUPS_read_group_tile (tilemap_number, editing_layer, real_tx, real_ty-1);
						temp_group_value &= ~FOUR_BIT_BOTTOM;
						TILEGROUPS_write_group_tile (tilemap_number,editing_layer,real_tx,real_ty-1,temp_group_value);
					}

					if (real_tx < cm[tilemap_number].map_width-1)
					{
						temp_group_value = TILEGROUPS_read_group_tile (tilemap_number, editing_layer, real_tx+1, real_ty);
						temp_group_value &= ~FOUR_BIT_LEFT;
						TILEGROUPS_write_group_tile (tilemap_number,editing_layer,real_tx+1,real_ty,temp_group_value);
					}

					if (real_ty < cm[tilemap_number].map_height-1)
					{
						temp_group_value = TILEGROUPS_read_group_tile (tilemap_number, editing_layer, real_tx, real_ty+1);
						temp_group_value &= ~FOUR_BIT_TOP;
						TILEGROUPS_write_group_tile (tilemap_number,editing_layer,real_tx,real_ty+1,temp_group_value);
					}
				}
				
			}
		}
		else
		{
			// Clicks in the panel

			if (CONTROL_mouse_hit(LMB) == true)
			{
				if (MATH_rectangle_to_point_intersect_by_size (editor_view_width-(ICON_SIZE*5),editor_view_height+(ICON_SIZE/2),ICON_SIZE*4,ICON_SIZE,mx,my) == true)
				{
					if (precise_mode == true)
					{
						precise_mode = false;
					}
					else
					{
						precise_mode = true;
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

	return true;

}

