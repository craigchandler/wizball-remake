#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <math.h>

#include "ai_nodes.h"
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

void HELPERTAGS_draw_helper_tags (int tilemap_number, int sx, int sy, int layer, float zoom_level, int helper_tag_x = UNSET, int helper_tag_y = UNSET)
{
	static float cycler = 0.0f;

	cycler += 0.02f;

	if (cycler > 1.0f)
	{
		cycler = 0.0f;
	}

	int tilesize = TILESETS_get_tilesize (cm[tilemap_number].tile_set_index);
	int x,y;
	
	int map_width;
	int map_height;
	int map_depth;

	char tile_helper_tag;
	char tile_x_offset;
	char tile_y_offset;

	int screen_offset_x;
	int screen_offset_y;

	bool top_left_line;
	bool top_right_line;
	bool bottom_left_line;
	bool bottom_right_line;

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

			tile_helper_tag = cm[tilemap_number].helper_tag_pointer[data_offset];

			if ( tile_helper_tag != 0)
			{
				OUTPUT_draw_sprite_scale (first_icon, tile_helper_tag+HELPER_TAG_WARNING_JUMP_ICON-1, int(float(x*tilesize)*zoom_level), int(float(y*tilesize)*zoom_level) , zoom_level*scale_factor );
			}
		}
	}

	for (y=0; (y<tiles_per_col) && (y+sy<map_height) ; y++)
	{
		for (x=0; (x<tiles_per_row) && (x+sx<map_width) ; x++)
		{
			data_offset = (layer * map_height * map_width) + ((sy+y) * map_width) + (sx+x);

			tile_x_offset = cm[tilemap_number].helper_x_offset_pointer[data_offset];
			tile_y_offset = cm[tilemap_number].helper_y_offset_pointer[data_offset];

			top_left_line = false;
			top_right_line = false;
			bottom_left_line = false;
			bottom_right_line = false;

			if ( (tile_x_offset < 0) && (tile_y_offset < 0) )
			{
				top_right_line = true;
				bottom_left_line = true;
			}
			else if ( (tile_x_offset == 0) && (tile_y_offset < 0) )
			{
				top_left_line = true;
				top_right_line = true;
			}
			else if ( (tile_x_offset > 0) && (tile_y_offset < 0) )
			{
				top_left_line = true;
				bottom_right_line = true;
			}
			else if ( (tile_x_offset > 0) && (tile_y_offset == 0) )
			{
				bottom_right_line = true;
				top_right_line = true;
			}
			else if ( (tile_x_offset > 0) && (tile_y_offset > 0) )
			{
				top_right_line = true;
				bottom_left_line = true;
			}
			else if ( (tile_x_offset == 0) && (tile_y_offset > 0) )
			{
				bottom_left_line = true;
				bottom_right_line = true;
			}
			else if ( (tile_x_offset < 0) && (tile_y_offset > 0) )
			{
				top_left_line = true;
				bottom_right_line = true;
			}
			else if ( (tile_x_offset < 0) && (tile_y_offset == 0) )
			{
				top_left_line = true;
				bottom_left_line = true;
			}

			if ( ( (tile_x_offset != 0) || (tile_y_offset != 0) ) && ( ( (helper_tag_x == (sx+x)) && (helper_tag_y == (sy+y)) ) || (helper_tag_x == UNSET) ) )
			{
				screen_offset_x = int(float(tile_x_offset * tilesize) * zoom_level);
				screen_offset_y = int(float(tile_y_offset * tilesize) * zoom_level);

				// Draw bordering lines from the outside corners.
				if (bottom_left_line)
				{
					OUTPUT_line ( int(float(x*tilesize)*zoom_level) , int(float(y*tilesize)*zoom_level) + int(float(tilesize)*zoom_level) - 1 , int(float(x*tilesize)*zoom_level) + screen_offset_x , int(float(y*tilesize)*zoom_level) + screen_offset_y + int(float(tilesize)*zoom_level) - 1 , 255 , 0 , 0 );
				}
				if (bottom_right_line)
				{
					OUTPUT_line ( int(float(x*tilesize)*zoom_level) + int(float(tilesize)*zoom_level) - 1 , int(float(y*tilesize)*zoom_level) + int(float(tilesize)*zoom_level) - 1 , int(float(x*tilesize)*zoom_level) + screen_offset_x + int(float(tilesize)*zoom_level) - 1 , int(float(y*tilesize)*zoom_level) + screen_offset_y + int(float(tilesize)*zoom_level) - 1 , 255 , 0 , 0 );
				}
				if (top_left_line)
				{
					OUTPUT_line ( int(float(x*tilesize)*zoom_level) , int(float(y*tilesize)*zoom_level) , int(float(x*tilesize)*zoom_level) + screen_offset_x , int(float(y*tilesize)*zoom_level) + screen_offset_y , 255 , 0 , 0 );
				}
				if (top_right_line)
				{
					OUTPUT_line ( int(float(x*tilesize)*zoom_level) + int(float(tilesize)*zoom_level) - 1 , int(float(y*tilesize)*zoom_level) , int(float(x*tilesize)*zoom_level) + screen_offset_x + int(float(tilesize)*zoom_level) - 1 , int(float(y*tilesize)*zoom_level) + screen_offset_y , 255 , 0 , 0 );
				}
				
				// Draw the box itself at the destination
				OUTPUT_rectangle_by_size ( int(float(x*tilesize)*zoom_level) + screen_offset_x , int(float(y*tilesize)*zoom_level) + screen_offset_y , int(float(tilesize)*zoom_level) , int(float(tilesize)*zoom_level) , 0 , 255 , 0 );

				screen_offset_x = int (float (screen_offset_x) * cycler);
				screen_offset_y = int (float (screen_offset_y) * cycler);

				// Draw the box on it's way to the destination.
				OUTPUT_rectangle_by_size ( int(float(x*tilesize)*zoom_level) + screen_offset_x , int(float(y*tilesize)*zoom_level) + screen_offset_y , int(float(tilesize)*zoom_level) , int(float(tilesize)*zoom_level) , 255 , 255 , 255 );
			}

		}
	}

}



void HELPERTAGS_put_helper_tag (int tilemap_number, int x, int y, int layer, char x_offset, char y_offset, char helper_tag = UNSET)
{
	int map_width;
	int map_height;
	int map_depth;

	int data_offset;

	map_width = cm[tilemap_number].map_width;
	map_height = cm[tilemap_number].map_height;
	map_depth = cm[tilemap_number].map_layers;

	if ( (x>=0) && (x<map_width) && (y>=0) && (y<map_height) && (layer>=0) && (layer<map_depth) )
	{
		data_offset = (layer * map_height * map_width) + (y * map_width) + x;

		if (helper_tag != UNSET)
		{
			cm[tilemap_number].helper_tag_pointer[data_offset] = helper_tag;
		}
		cm[tilemap_number].helper_x_offset_pointer[data_offset] = x_offset;
		cm[tilemap_number].helper_y_offset_pointer[data_offset] = y_offset;
	}
}



bool HELPERTAGS_edit_helper_tagging (int state , bool overlay_display, int *current_cursor ,int sx, int sy,  int tilemap_number, int mx, int my, float zoom_level, int grid_size, int editing_layer)
{
	static int chosen_tag;

	static bool show_all_offsets;

	char word_string[TEXT_LINE_SIZE];

	int tileset;
	int tilesize;

	int tx;
	int ty;
	
	int real_tx;
	int real_ty;

	static int clicked_tx;
	static int clicked_ty;
	static bool placing;

	int t;

	if (state == STATE_INITIALISE)
	{
		chosen_tag = 1;
		editing_layer = 0;

		placing = false;

		show_all_offsets = false;
	}
	else if (state == STATE_SET_UP_BUTTONS)
	{
		editor_view_width = game_screen_width;
		editor_view_height = game_screen_height - (3*ICON_SIZE);

		for (t=0; t<14; t++)
		{
			SIMPGUI_create_set_button (TILEMAP_EDITOR_SUB_GROUP,EDIT_TILEMAP,&chosen_tag,(t*ICON_SIZE),editor_view_height+(ICON_SIZE),first_icon,HELPER_TAG_WARNING_JUMP_ICON+t,LMB,t+1);
		}

		SIMPGUI_create_nudge_button (TILEMAP_EDITOR_SUB_GROUP, EDIT_TILEMAP, &editing_layer, editor_view_width-(4*ICON_SIZE) , editor_view_height+(ICON_SIZE/2)+ICON_SIZE, first_icon , LEFT_ARROW_ICON, LMB, 0, 128, -1, false);
		SIMPGUI_create_nudge_button (TILEMAP_EDITOR_SUB_GROUP, EDIT_TILEMAP, &editing_layer, editor_view_width-(1*ICON_SIZE) , editor_view_height+(ICON_SIZE/2)+ICON_SIZE, first_icon , RIGHT_ARROW_ICON, LMB, 0, 128, 1, false);

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

		real_tx = tx + sx;
		real_ty = ty + sy;

		if (my < editor_view_height)
		{
			// Clicks in the main view

			if (CONTROL_mouse_hit(LMB) == true)
			{
				placing = true;
				clicked_tx = real_tx;
				clicked_ty = real_ty;
				HELPERTAGS_put_helper_tag (tilemap_number, clicked_tx, clicked_ty, editing_layer, 0, 0, chosen_tag );
			}

			if ( (CONTROL_mouse_down(LMB) == true)  )
			{
				if (placing == true)
				{
					HELPERTAGS_put_helper_tag (tilemap_number, clicked_tx, clicked_ty, editing_layer, (real_tx-clicked_tx), (real_ty-clicked_ty) );
				}
			}
			else
			{
				placing = false;
			}

		}
		else
		{
			// Clicks in the overlay - all dealt with by SIMPGUI.

		}

		if (tileset != UNSET)
		{
			TILEMAPS_draw (editing_layer,editing_layer+1,tilemap_number,sx,sy,editor_view_width,editor_view_height,zoom_level, COLOUR_DISPLAY_MODE_NONE);
			TILEMAPS_draw_guides ( tilemap_number , sx, sy, tilesize, zoom_level, guide_box_width , guide_box_height );

			if (show_all_offsets == true)
			{
				HELPERTAGS_draw_helper_tags ( tilemap_number, sx, sy, editing_layer, zoom_level );
			}
			else
			{
				if (placing == true)
				{
					HELPERTAGS_draw_helper_tags ( tilemap_number, sx, sy, editing_layer, zoom_level, clicked_tx , clicked_ty );
				}
				else
				{
					HELPERTAGS_draw_helper_tags ( tilemap_number, sx, sy, editing_layer, zoom_level, real_tx , real_ty );
				}
			}
		}

		OUTPUT_draw_sprite_scale (first_icon, chosen_tag+HELPER_TAG_WARNING_JUMP_ICON-1, int(float(tx*tilesize)*zoom_level), int(float(ty*tilesize)*zoom_level) , zoom_level*scale_factor );
		OUTPUT_rectangle_by_size ( int(float(tx*tilesize)*zoom_level), int(float(ty*tilesize)*zoom_level) , tilesize , tilesize , 255 , 255 , 255 );

		if (overlay_display == true)
		{
			SIMPGUI_wake_group (TILEMAP_EDITOR_SUB_GROUP);

			editor_view_width = game_screen_width;
			editor_view_height = game_screen_height-(3*ICON_SIZE);

			OUTPUT_filled_rectangle (0,editor_view_height,game_screen_width,game_screen_height,0,0,0);
			
			TILEMAPS_dialogue_box ( editor_view_width-(4*ICON_SIZE) , editor_view_height+(ICON_SIZE/2) , ICON_SIZE*4, ICON_SIZE , "EDITING LAYER");

			snprintf (word_string,sizeof(word_string),"%3d",editing_layer);
			TILEMAPS_dialogue_box ( editor_view_width-(3*ICON_SIZE) , editor_view_height+(ICON_SIZE/2)+ICON_SIZE , ICON_SIZE*2, ICON_SIZE , word_string);

			OUTPUT_hline (0,editor_view_height,game_screen_width,255,255,255);

			OUTPUT_centred_text ( editor_view_width/2 , editor_view_height+(ICON_SIZE-FONT_HEIGHT)/2 , "HELPER TAG EDITOR" );

			SIMPGUI_check_all_buttons();
			SIMPGUI_draw_all_buttons();
		}
		else
		{
			editor_view_width = game_screen_width;
			editor_view_height = game_screen_height;

			SIMPGUI_sleep_group (TILEMAP_EDITOR_SUB_GROUP);
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
