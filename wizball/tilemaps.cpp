#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <math.h>
#include <ctype.h>
#include <allegro.h>

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
#include "textfiles.h"
#include "paths.h"

#include "ai_zones.h"
#include "ai_nodes.h"
#include "spawn_points.h"
#include "helpertag.h"
#include "tilegroup.h"
#include "roomzones.h"
#include "tilecolour.h"

#include "fortify.h"

#ifdef ALLEGRO_MACOSX
#include <CoreServices/CoreServices.h>
static void endian(int& i) 
{
	i = EndianS32_LtoN(i);
}
static void endian(short& i) 
{
	i = EndianS16_LtoN(i);
}
static void endian(zone& z)
{
	endian(z.text_tag_index);
	endian(z.x);
	endian(z.y);
	endian(z.width);
	endian(z.height);
	endian(z.uid);
	endian(z.type_index);
	endian(z.flag);
	endian(z.real_x);
	endian(z.real_y);
	endian(z.real_width);
	endian(z.real_height);
}
static void endian(tilemap& t)
{
	endian(t.uid);
	endian(t.spawn_point_next_uid);
	endian(t.zone_next_uid);
	endian(t.waypoint_next_uid);
	endian(t.tile_set_index);
	endian(t.map_layers);
	endian(t.map_width);
	endian(t.map_height);
	endian(t.waypoints);
	endian(t.spawn_points);
	endian(t.zones);
	endian(t.ai_zones);
	endian(t.gates);
	endian(t.remotes);
	endian(t.map_width_in_lzones);
	endian(t.localised_zone_list_size);
	endian(t.ai_node_lookup_table_list_size);
	endian(t.vertex_colour_mode);
}
static void endian(gate& g) 
{
	endian(g.x1);
	endian(g.y1);
	endian(g.x2);
	endian(g.y2);
	endian(g.side_1_p1_neighbour);
	endian(g.side_1_p2_neighbour);
	endian(g.side_2_p1_neighbour);
	endian(g.side_2_p2_neighbour);
	endian(g.neighbour1);
	endian(g.neighbour2);
}
static void endian(remote_connection& r)
{
	endian(r.x1);
	endian(r.y1);
	endian(r.x2);
	endian(r.y2);
	endian(r.zone_1);
	endian(r.zone_2);
	endian(r.connective_direction);
}
static void endian(waypoint& w) 
{
	endian(w.x);
	endian(w.y);
	endian(w.uid);
	endian(w.type_index);
	endian(w.location_index);
	endian(w.connections);
	endian(w.grabbed_status);
	endian(w.grabbed_x);
	endian(w.grabbed_y);
	endian(w.family);
	endian(w.total_distance);
}
static void endian(spawn_point& s)
{
	endian(s.x);
	endian(s.y);
	endian(s.uid);
	endian(s.flag);
	endian(s.type_value);
	endian(s.grabbed_status);
	endian(s.grabbed_x);
	endian(s.grabbed_y);
	endian(s.script_index);
	endian(s.next_sibling_uid);
	endian(s.parent_uid);
	endian(s.parent_index);
	endian(s.child_index);
	endian(s.prev_sibling_index);
	endian(s.next_sibling_index);
	endian(s.params);
	endian(s.family_id);
	endian(s.prev_in_family);
	endian(s.next_in_family);
	endian(s.last_fired);
	endian(s.created_entity_index);
}
static void endian(parameter& p) 
{
	endian(p.param_dest_var_index);
	endian(p.param_value);
}
#endif


int next_free_map_uid = 0;

tilemap *cm = NULL;

int number_of_tilemaps_loaded = 0;

int editor_view_width = game_screen_width;
int editor_view_height = game_screen_height;

int guide_box_width = 40;
int guide_box_height = 26;



void TILEMAPS_load_editor_settings (void)
{
	// Go through all the loaded maps looking for the highest
	// number and set the next free map uid to that plus 1.

	bool exitmainloop = false;

	char line[TEXT_LINE_SIZE];

	char *pointer;

	FILE *file_pointer = fopen (MAIN_get_project_filename ("editor_settings.txt"),"r");

	if (file_pointer != NULL)
	{
		while ( ( fgets ( line , TEXT_LINE_SIZE , file_pointer ) != NULL ) && (exitmainloop == false) )
		{
			STRING_strip_newlines (line);
			strupr(line);

			pointer = strstr(line,"#END_OF_FILE");
			if (pointer != NULL) // It's the end of the world as we know it...
			{
				exitmainloop = true;
			}

			pointer = STRING_end_of_string(line,"#DEFAULT_ROOM_WIDTH = ");
			if (pointer != NULL) // It's the default tile set! Wooty-wooty-woot! *ahem*
			{
				guide_box_width = atoi(pointer);
			}

			pointer = STRING_end_of_string(line,"#DEFAULT_ROOM_HEIGHT = ");
			if (pointer != NULL) // It's the default tile set! Wooty-wooty-woot! *ahem*
			{
				guide_box_height = atoi(pointer);
			}

			pointer = STRING_end_of_string(line,"#NEXT_MAP_UID = ");
			if (pointer != NULL) // It's the default tile set! Wooty-wooty-woot! *ahem*
			{
				next_free_map_uid = atoi(pointer);
			}
		}
	}
	else
	{
		// No editor settings to load!
	}
}



void TILEMAPS_save_editor_settings (void)
{
	// Go through all the loaded maps looking for the highest
	// number and set the next free map uid to that plus 1.

	bool exitmainloop = false;

	char line[TEXT_LINE_SIZE];

	FILE *file_pointer = fopen (MAIN_get_project_filename ("editor_settings.txt"),"w");

	if (file_pointer != NULL)
	{
		sprintf(line,"#DEFAULT_ROOM_WIDTH = %i\n",guide_box_width);
		fputs(line,file_pointer);

		sprintf(line,"#DEFAULT_ROOM_HEIGHT = %i\n",guide_box_height);
		fputs(line,file_pointer);

		sprintf(line,"#NEXT_MAP_UID = %i\n",next_free_map_uid);
		fputs(line,file_pointer);

		fputs("#END_OF_FILE\n",file_pointer);
	}
	else
	{
		OUTPUT_message("Cannot save editor settings!");
		assert(0);
		// No editor settings to load!
	}
}



int TILEMAPS_get_width (int map_index, bool in_pixels)
{
	if (cm == NULL || map_index < 0 || map_index >= number_of_tilemaps_loaded)
	{
		return 0;
	}

	if (in_pixels)
	{
		int tilesize = TILESETS_get_tilesize(cm[map_index].tile_set_index);
		if (tilesize <= 0)
		{
			return 0;
		}
		return (cm[map_index].map_width * tilesize);
	}
	else
	{
		return (cm[map_index].map_width);
	}
}



int TILEMAPS_get_height (int map_index, bool in_pixels)
{
	if (cm == NULL || map_index < 0 || map_index >= number_of_tilemaps_loaded)
	{
		return 0;
	}

	if (in_pixels)
	{
		int tilesize = TILESETS_get_tilesize(cm[map_index].tile_set_index);
		if (tilesize <= 0)
		{
			return 0;
		}
		return (cm[map_index].map_height * tilesize);
	}
	else
	{
		return (cm[map_index].map_height);
	}
}



void TILEMAPS_swap_tilemaps (int t1, int t2)
{
	// If you wish to INSERT a tilemap into the list at specific point (for instance a new level into a game)
	// then this will allow for it as you'll just create a new TILEMAP and then swap all the ones from the end
	// down to the insertion point. Same with deleting a particular one, which is the more common application.

	tilemap temp;

	temp = cm[t1];
	cm[t1] = cm[t2];
	cm[t2] = temp;
}



void TILEMAPS_backup_tilemap (int map_number)
{
	OUTPUT_message("Function not implemented yet!");
}



void TILEMAPS_restore_tilemap (int map_number)
{
	OUTPUT_message("Function not implemented yet!");
}



void TILEMAPS_destroy_map (int tilemap_number)
{
	// Frees up all the memory malloced in the map.

	int spawn_point_number;
	int waypoint_number;
	int zone_number;

	for (spawn_point_number = 0; spawn_point_number < cm[tilemap_number].spawn_points ; spawn_point_number++)
	{
		SPAWNPOINTS_destroy_spawn_point (tilemap_number,spawn_point_number);
	}

	free (cm[tilemap_number].spawn_point_list_pointer);
	cm[tilemap_number].spawn_points = 0;

	AINODES_destroy_waypoint_lookup_tables (tilemap_number);

	for (waypoint_number = 0; waypoint_number<cm[tilemap_number].waypoints ; waypoint_number++)
	{
		AINODES_destroy_waypoint (tilemap_number, waypoint_number);
	}

	free (cm[tilemap_number].waypoint_list_pointer);
	cm[tilemap_number].waypoints = 0;

	for (zone_number=0; zone_number<cm[tilemap_number].zones; zone_number++)
	{
		if (cm[tilemap_number].zone_list_pointer[zone_number].spawn_point_index_list != NULL)
		{
			free(cm[tilemap_number].zone_list_pointer[zone_number].spawn_point_index_list);
		}

		if (cm[tilemap_number].zone_list_pointer[zone_number].ai_node_index_list != NULL)
		{
			free(cm[tilemap_number].zone_list_pointer[zone_number].ai_node_index_list);
		}
	}
	
	ROOMZONES_destroy_localised_zone_list (tilemap_number);
	ROOMZONES_destroy_zones (tilemap_number);

	free (cm[tilemap_number].map_pointer);
	free (cm[tilemap_number].group_pointer);
	free (cm[tilemap_number].helper_tag_pointer);
	free (cm[tilemap_number].helper_x_offset_pointer);
	free (cm[tilemap_number].helper_y_offset_pointer);

	if (cm[tilemap_number].backup_in_use == true)
	{
		if (cm[tilemap_number].backup_map_pointer != NULL)
		{
			free (cm[tilemap_number].backup_map_pointer);
			cm[tilemap_number].backup_map_pointer = NULL;
		}
		else
		{
			OUTPUT_message("Backup flagged as in use, but memory not allocated! WTF?!");
		}
	}

	if (cm[tilemap_number].collision_data_pointer != NULL)
	{
		free (cm[tilemap_number].collision_data_pointer);
	}

	if (cm[tilemap_number].exposure_data_pointer != NULL)
	{
		free (cm[tilemap_number].exposure_data_pointer);
	}

	if (cm[tilemap_number].collision_bitmask_data_pointer != NULL)
	{
		free (cm[tilemap_number].collision_bitmask_data_pointer);
	}
	
	if (cm[tilemap_number].optimisation_data != NULL)
	{
		free (cm[tilemap_number].optimisation_data);
		cm[tilemap_number].optimisation_data = NULL;
		cm[tilemap_number].optimisation_data_flag = false;
	}

	if (cm[tilemap_number].layer_vertex_colours_pointer != NULL)
	{
		free (cm[tilemap_number].layer_vertex_colours_pointer);
	}

	if (cm[tilemap_number].vertex_colours_pointer != NULL)
	{
		free (cm[tilemap_number].vertex_colours_pointer);
	}

	if (cm[tilemap_number].detailed_vertex_colours_pointer != NULL)
	{
		free (cm[tilemap_number].detailed_vertex_colours_pointer);
	}
}



void TILEMAPS_destroy_all_maps (void)
{
	int tilemap_number;

	for (tilemap_number = 0; tilemap_number<number_of_tilemaps_loaded ; tilemap_number++)
	{
		TILEMAPS_destroy_map (tilemap_number);
	}

	free (cm);

	number_of_tilemaps_loaded = 0;
}



void TILEMAPS_create_real_zone_sizes (void)
{
	int zone_number;
	int tilemap_number;
	int tileset_index;
	int tile_size;

	for (tilemap_number = 0; tilemap_number<number_of_tilemaps_loaded; tilemap_number++)
	{
		tileset_index = cm[tilemap_number].tile_set_index;
		tile_size = TILESETS_get_tilesize (tileset_index);
		if (tile_size <= 0)
		{
			tile_size = 0;
		}

		for (zone_number = 0; zone_number<cm[tilemap_number].zones; zone_number++)
		{
			cm[tilemap_number].zone_list_pointer[zone_number].real_x = cm[tilemap_number].zone_list_pointer[zone_number].x * tile_size;
			cm[tilemap_number].zone_list_pointer[zone_number].real_y = cm[tilemap_number].zone_list_pointer[zone_number].y * tile_size;
			cm[tilemap_number].zone_list_pointer[zone_number].real_width = cm[tilemap_number].zone_list_pointer[zone_number].width * tile_size;
			cm[tilemap_number].zone_list_pointer[zone_number].real_height = cm[tilemap_number].zone_list_pointer[zone_number].height * tile_size;
		}
	}

}



void TILEMAPS_create_zone_spawn_point_lists (void)
{
	// This goes through each zone, finds the indexes of each spawn point which is within it
	// and compiles a table of them. That way when you launch them in the game it'll save on
	// trawling through the entire map.

	// Also sets up the real_x,real_y,real_width and real_height for each zone.

	int x,y,width,height;
	int count;
	int spawn_point_number;
	int zone_number;
	int tilemap_number;

	for (tilemap_number = 0; tilemap_number<number_of_tilemaps_loaded; tilemap_number++)
	{
		for (zone_number = 0; zone_number<cm[tilemap_number].zones; zone_number++)
		{
			x = cm[tilemap_number].zone_list_pointer[zone_number].real_x;
			y = cm[tilemap_number].zone_list_pointer[zone_number].real_y;
			width = cm[tilemap_number].zone_list_pointer[zone_number].real_width;
			height = cm[tilemap_number].zone_list_pointer[zone_number].real_height;

			count = 0;

			// First of all, just run through and count 'em.

			for (spawn_point_number = 0; spawn_point_number < cm[tilemap_number].spawn_points ; spawn_point_number++)
			{
				if (MATH_rectangle_to_point_intersect_by_size (x,y,width,height , cm[tilemap_number].spawn_point_list_pointer[spawn_point_number].x , cm[tilemap_number].spawn_point_list_pointer[spawn_point_number].y))
				{
					count++;
				}
			}

			// Now we can malloc the space needed...

			cm[tilemap_number].zone_list_pointer[zone_number].spawn_point_index_list = (int *) malloc (count * sizeof(int));
			cm[tilemap_number].zone_list_pointer[zone_number].spawn_point_index_list_size = count;

			count = 0;

			for (spawn_point_number = 0; spawn_point_number < cm[tilemap_number].spawn_points ; spawn_point_number++)
			{
				if (MATH_rectangle_to_point_intersect_by_size (x,y,width,height, cm[tilemap_number].spawn_point_list_pointer[spawn_point_number].x , cm[tilemap_number].spawn_point_list_pointer[spawn_point_number].y))
				{
					cm[tilemap_number].zone_list_pointer[zone_number].spawn_point_index_list[count] = spawn_point_number;
					count++;
				}
			}

		}
	}

}



void TILEMAPS_create_zone_ai_node_lists (void)
{
	// This goes through each zone, finds the indexes of each waypoint which is within it
	// and compiles a table of them. That way when you look for them in the game it'll save 
	// on trawling through the entire map.

	int x,y,width,height;
	int count;
	int ai_node_number;
	int zone_number;
	int tilemap_number;
	int tileset_index;
	int tile_size;

	for (tilemap_number = 0; tilemap_number<number_of_tilemaps_loaded; tilemap_number++)
	{
		tileset_index = cm[tilemap_number].tile_set_index;
		tile_size = TILESETS_get_tilesize (tileset_index);

		for (zone_number = 0; zone_number<cm[tilemap_number].zones; zone_number++)
		{
			x = cm[tilemap_number].zone_list_pointer[zone_number].real_x;
			y = cm[tilemap_number].zone_list_pointer[zone_number].real_y;
			width = cm[tilemap_number].zone_list_pointer[zone_number].real_width;
			height = cm[tilemap_number].zone_list_pointer[zone_number].real_height;

			count = 0;

			// First of all, just run through and count 'em.

			for (ai_node_number = 0; ai_node_number < cm[tilemap_number].waypoints ; ai_node_number++)
			{
				if (MATH_rectangle_to_point_intersect_by_size (x,y,width,height , cm[tilemap_number].waypoint_list_pointer[ai_node_number].x , cm[tilemap_number].waypoint_list_pointer[ai_node_number].y))
				{
					count++;
				}
			}

			// Now we can malloc the space needed...

			cm[tilemap_number].zone_list_pointer[zone_number].ai_node_index_list = (int *) malloc (count * sizeof(int));
			cm[tilemap_number].zone_list_pointer[zone_number].ai_node_index_list_size = count;

			count = 0;

			for (ai_node_number = 0; ai_node_number < cm[tilemap_number].waypoints ; ai_node_number++)
			{
				if (MATH_rectangle_to_point_intersect_by_size (x,y,width,height , cm[tilemap_number].waypoint_list_pointer[ai_node_number].x , cm[tilemap_number].waypoint_list_pointer[ai_node_number].y))
				{
					cm[tilemap_number].zone_list_pointer[zone_number].ai_node_index_list[count] = ai_node_number;
					count++;
				}
			}

		}
	}

}



detailed_vertex_colours * TILEMAPS_get_complex_vertex_pointer (int tilemap_number, int layer, int x, int y)
{
	if ( (layer>=0) && (layer<cm[tilemap_number].map_layers) && (x>=0) && (y>=0) && (x<cm[tilemap_number].map_width) && (y<cm[tilemap_number].map_height) )
	{
		return &cm[tilemap_number].detailed_vertex_colours_pointer[ (layer * cm[tilemap_number].map_width * cm[tilemap_number].map_height) + (y * cm[tilemap_number].map_width ) + x ];
	}
	else
	{
		return NULL;
	}
}



simple_vertex_colours * TILEMAPS_get_simple_vertex_pointer (int tilemap_number, int layer, int x, int y)
{
	if ( (layer>=0) && (layer<cm[tilemap_number].map_layers) && (x>=0) && (y>=0) && (x<cm[tilemap_number].map_width) && (y<cm[tilemap_number].map_height) )
	{
		return &cm[tilemap_number].vertex_colours_pointer[ (layer * cm[tilemap_number].map_width * cm[tilemap_number].map_height) + (y * cm[tilemap_number].map_width ) + x ];
	}
	else
	{
		return NULL;
	}
}



int TILEMAPS_get_tile (int tilemap_number, int layer, int x, int y)
{
	if ( (layer>=0) && (layer<cm[tilemap_number].map_layers) && (x>=0) && (y>=0) && (x<cm[tilemap_number].map_width) && (y<cm[tilemap_number].map_height) )
	{
		return cm[tilemap_number].map_pointer[ (layer * cm[tilemap_number].map_width * cm[tilemap_number].map_height) + (y * cm[tilemap_number].map_width ) + x ];
	}
	else
	{
		return 0;
	}
}



void TILEMAPS_put_tile (int tilemap_number, int layer, int x, int y , int tile)
{
	if ( (x>=0) && (y>=0) && (x<cm[tilemap_number].map_width) && (y<cm[tilemap_number].map_height) )
	{
		cm[tilemap_number].map_pointer[ (layer * cm[tilemap_number].map_width * cm[tilemap_number].map_height) + (y * cm[tilemap_number].map_width ) + x ] = tile;
	}
}



void TILEMAPS_plonk (int tilemap_number, int tilemap_layer)
{
	// This simply dumps all the tiles into the map in order so that you can easily
	// grab them afterwards.

	int width_in_tiles; 
	int number_of_tiles;
	int tileset_index;
	int image_index;
	int tilesize;

	tileset_index = cm[tilemap_number].tile_set_index;

	number_of_tiles = TILESETS_get_tile_count (tileset_index);

	tilesize = TILESETS_get_tilesize (tileset_index);

	image_index = TILESETS_get_sprite_index (tileset_index);

	width_in_tiles = OUTPUT_bitmap_width (image_index) / tilesize;

	int t;
	int tx,ty;

	for (t=0; t<number_of_tiles; t++)
	{
		tx = t % width_in_tiles;
		ty = t / width_in_tiles;

		TILEMAPS_put_tile (tilemap_number, tilemap_layer, tx, ty , t);
	}
	
}




void TILEMAPS_resize_map (int tilemap_number, int x_inc, int y_inc, int layer_inc)
{
	int new_width,new_height,new_layers;
	short *new_map_pointer;
	char *new_group_pointer;
	char *new_helper_tag_pointer;
	char *new_helper_x_offset_pointer;
	char *new_helper_y_offset_pointer;
	simple_vertex_colours *new_vertex_colours_pointer;
	detailed_vertex_colours *new_detailed_vertex_colours_pointer;
	simple_vertex_colours *new_layer_vertex_colours_pointer;

	bool layer_vertex,tile_vertex,tile_detailed_vertex;

	if (cm[tilemap_number].layer_vertex_colours_pointer == NULL)
	{
		layer_vertex = false;
	}
	else
	{
		layer_vertex = true;
	}

	if (cm[tilemap_number].vertex_colours_pointer == NULL)
	{
		tile_vertex = false;
	}
	else
	{
		tile_vertex = true;
	}

	if (cm[tilemap_number].detailed_vertex_colours_pointer == NULL)
	{
		tile_detailed_vertex = false;
	}
	else
	{
		tile_detailed_vertex = true;
	}
	
	int x,y,layer;

	int data_offset; 

	int smallest_width,smallest_height,smallest_layers;

	// First of all make sure that x/y inc won't reduce the size to <1

	if (cm[tilemap_number].map_width + x_inc < 1)
	{
		x_inc = 1 - cm[tilemap_number].map_width;
	}

	if (cm[tilemap_number].map_height + y_inc < 1)
	{
		y_inc = 1 - cm[tilemap_number].map_height;
	}

	if (cm[tilemap_number].map_layers + layer_inc < 1)
	{
		layer_inc = 1 - cm[tilemap_number].map_layers;
	}

	// Then malloc a big ol' chunk of space which is the size of the new map.

	new_width = cm[tilemap_number].map_width + x_inc;
	new_height = cm[tilemap_number].map_height + y_inc;
	new_layers = cm[tilemap_number].map_layers + layer_inc;

	new_map_pointer = (short *) malloc (new_layers * new_width * new_height * sizeof(short));
	new_group_pointer = (char *) malloc (new_layers * new_width * new_height * sizeof(char));
	new_helper_tag_pointer = (char *) malloc (new_layers * new_width * new_height * sizeof(char));
	new_helper_x_offset_pointer = (char *) malloc (new_layers * new_width * new_height * sizeof(char));
	new_helper_y_offset_pointer = (char *) malloc (new_layers * new_width * new_height * sizeof(char));
	
	if (tile_vertex)
	{
		new_vertex_colours_pointer = (simple_vertex_colours *) malloc (new_layers * new_width * new_height * sizeof(simple_vertex_colours));
	}

	if (tile_detailed_vertex)
	{
		new_detailed_vertex_colours_pointer = (detailed_vertex_colours *) malloc (new_layers * new_width * new_height * sizeof(detailed_vertex_colours));
	}

	if (layer_vertex)
	{
		new_layer_vertex_colours_pointer = (simple_vertex_colours *) malloc (new_layers * sizeof(simple_vertex_colours));
	}

	if ( (new_map_pointer == NULL) || (new_group_pointer == NULL) || (new_helper_tag_pointer == NULL) || (new_helper_x_offset_pointer == NULL) || (new_helper_y_offset_pointer == NULL) )
	{
		OUTPUT_message("Could not allocate memory to resize map! Arsebadgers!");
		assert(0);
	}

	// Now blank this new map...

	int colour;

	for (layer=0; layer<new_layers ; layer++)
	{
		if (layer_vertex)
		{
			new_layer_vertex_colours_pointer[layer].colours[0] = 1.0f;
			new_layer_vertex_colours_pointer[layer].colours[1] = 1.0f;
			new_layer_vertex_colours_pointer[layer].colours[2] = 1.0f;
		}

		for (y=0; y<new_height ; y++)
		{
			for (x=0; x<new_width ; x++)
			{
				data_offset = (layer * new_height * new_width) + (y * new_width ) + x;
				
				new_map_pointer [data_offset] = short(0);
				new_group_pointer [data_offset] = char(0);
				new_helper_tag_pointer [data_offset] = char(0);
				new_helper_x_offset_pointer [data_offset] = char(0);
				new_helper_y_offset_pointer [data_offset] = char(0);

				if (tile_vertex)
				{
					for (colour=0; colour<3; colour++)
					{
						new_vertex_colours_pointer[data_offset].colours[colour] = 1.0f;
					}
				}

				if (tile_detailed_vertex)
				{
					for (colour=0; colour<12; colour++)
					{
						new_detailed_vertex_colours_pointer[data_offset].colours[colour] = 1.0f;
					}
				}
			}
		}
	}

	// Now copy as much of the old map into it as possible...

	smallest_width = MATH_smallest_int(new_width,cm[tilemap_number].map_width);
	smallest_height = MATH_smallest_int(new_height,cm[tilemap_number].map_height);
	smallest_layers = MATH_smallest_int(new_layers,cm[tilemap_number].map_layers);

	int source_data_offset;
	int destination_data_offset;

	for (layer=0; layer<smallest_layers ; layer++)
	{
		if (layer_vertex)
		{
			new_layer_vertex_colours_pointer[layer].colours[0] = cm[tilemap_number].layer_vertex_colours_pointer[layer].colours[0];
			new_layer_vertex_colours_pointer[layer].colours[1] = cm[tilemap_number].layer_vertex_colours_pointer[layer].colours[1];
			new_layer_vertex_colours_pointer[layer].colours[2] = cm[tilemap_number].layer_vertex_colours_pointer[layer].colours[2];
		}

		for (y=0; y<smallest_height ; y++)
		{
			for (x=0; x<smallest_width ; x++)
			{
				source_data_offset = (layer * cm[tilemap_number].map_width * cm[tilemap_number].map_height) + (y * cm[tilemap_number].map_width ) + x;
				destination_data_offset = (layer * new_width * new_height) + (y * new_width ) + x;

				new_map_pointer [destination_data_offset] = cm[tilemap_number].map_pointer[ source_data_offset ];
				new_group_pointer [destination_data_offset] = cm[tilemap_number].group_pointer[ source_data_offset ];

				if (tile_vertex)
				{
					for (colour=0; colour<3; colour++)
					{
						new_vertex_colours_pointer[data_offset].colours[colour] = cm[tilemap_number].vertex_colours_pointer[source_data_offset].colours[colour];
					}
				}

				if (tile_detailed_vertex)
				{
					for (colour=0; colour<12; colour++)
					{
						new_detailed_vertex_colours_pointer[data_offset].colours[colour] = cm[tilemap_number].detailed_vertex_colours_pointer[source_data_offset].colours[colour];
					}
				}

				new_helper_tag_pointer [destination_data_offset] = cm[tilemap_number].helper_tag_pointer[ source_data_offset ];
				new_helper_x_offset_pointer [destination_data_offset] = cm[tilemap_number].helper_x_offset_pointer[ source_data_offset ];
				new_helper_y_offset_pointer [destination_data_offset] = cm[tilemap_number].helper_y_offset_pointer[ source_data_offset ];
			}
		}
	}

	// Then free up the old map and give this new one to it instead.

	free (cm[tilemap_number].map_pointer);
	free (cm[tilemap_number].group_pointer);
	free (cm[tilemap_number].helper_tag_pointer);
	free (cm[tilemap_number].helper_x_offset_pointer);
	free (cm[tilemap_number].helper_y_offset_pointer);

	if (layer_vertex)
	{
		free (cm[tilemap_number].layer_vertex_colours_pointer);
		cm[tilemap_number].layer_vertex_colours_pointer = new_layer_vertex_colours_pointer;
	}

	if (tile_vertex)
	{
		free (cm[tilemap_number].vertex_colours_pointer);
		cm[tilemap_number].vertex_colours_pointer = new_vertex_colours_pointer;
	}

	if (tile_detailed_vertex)
	{
		free (cm[tilemap_number].detailed_vertex_colours_pointer);
		cm[tilemap_number].detailed_vertex_colours_pointer = new_detailed_vertex_colours_pointer;
	}

	cm[tilemap_number].map_pointer = new_map_pointer;
	cm[tilemap_number].group_pointer = new_group_pointer;
	cm[tilemap_number].helper_tag_pointer = new_helper_tag_pointer;
	cm[tilemap_number].helper_x_offset_pointer = new_helper_x_offset_pointer;
	cm[tilemap_number].helper_y_offset_pointer = new_helper_y_offset_pointer;
	
	cm[tilemap_number].map_width = new_width;
	cm[tilemap_number].map_height = new_height;
	cm[tilemap_number].map_layers = new_layers;
}



short * TILEMAPS_copy_map_section (int tilemap_number, int slayer, int sx, int sy, int *width, int *height, int *depth, short *copy_area)
{
	int counter;
	int x,y,layer;

	if (sx > cm[tilemap_number].map_width)
	{
		sx = 0;
		*width = 1;
	}

	if (sy > cm[tilemap_number].map_height)
	{
		sy = 0;
		*height = 1;
	}

	if (sx + *width > cm[tilemap_number].map_width)
	{
		*width = cm[tilemap_number].map_width - sx;
	}

	if (sy + *height > cm[tilemap_number].map_height)
	{
		*height = cm[tilemap_number].map_height - sy;
	}

	if (slayer + *depth > cm[tilemap_number].map_layers)
	{
		*depth = cm[tilemap_number].map_layers - slayer;
	}

	if (copy_area != NULL) // If there's any memory tied up, free it up!
	{
		free(copy_area);
	}

	copy_area = (short *) malloc (sizeof(short) * *width * *height * *depth);
	
	counter = 0;

	for (layer=slayer; layer<(slayer+*depth) ; layer++)
	{
		for (x=sx; x<(sx+*width) ; x++)
		{
			for (y=sy; y<(sy+*height) ; y++)
			{
				copy_area[counter] = TILEMAPS_get_tile (tilemap_number, layer, x, y);
				counter++;
			}
		}
	}

	return (copy_area);
}



#define TILE_PLACEMENT_SOLID				(0)
#define TILE_PLACEMENT_NON_ZERO				(1)
#define TILE_PLACEMENT_ONLY_ZERO			(2)



void TILEMAPS_paste_map_section (int tilemap_number, int slayer, int sx, int sy, int width, int height, int depth, short *copy_area, int tile_edit_mode)
{
	int counter;
	int x,y,layer;
	int tile_number;
	int destination_tile_number;

	int source_width = width;
	int source_height = height;

	if (sx + width > cm[tilemap_number].map_width)
	{
		width = cm[tilemap_number].map_width - sx;
	}

	if (sy + height > cm[tilemap_number].map_height)
	{
		height = cm[tilemap_number].map_height - sy;
	}

	if (slayer + depth > cm[tilemap_number].map_layers)
	{
		depth = cm[tilemap_number].map_layers - slayer;
	}

	counter = 0;

	for (layer=slayer; layer<(slayer+depth) ; layer++)
	{
		for (x=sx; x<(sx+width) ; x++)
		{
			for (y=sy; y<(sy+height) ; y++)
			{
				tile_number = copy_area[(x-sx)*source_height + (y-sy)];
				switch (tile_edit_mode)
				{
				case TILE_PLACEMENT_SOLID:
					TILEMAPS_put_tile (tilemap_number, layer, x, y , tile_number);
					break;
				case TILE_PLACEMENT_NON_ZERO:
					if (tile_number > 0)
					{
						TILEMAPS_put_tile (tilemap_number, layer, x, y , tile_number);
					}
					break;
				case TILE_PLACEMENT_ONLY_ZERO:
					destination_tile_number = TILEMAPS_get_tile (tilemap_number,layer,x,y);
					if (destination_tile_number == 0)
					{
						TILEMAPS_put_tile (tilemap_number, layer, x, y , tile_number);
					}
					break;

				}

				counter++;
			}
		}
	}
}



void TILEMAPS_fill_area (int tilemap_number, int slayer, int sx, int sy, int width, int height, int depth, short *copy_area, int tile_edit_mode)
{
	// This simply builds up a hooooj list of all the tiles connected to the one at the parsed co-ordinates and then
	// proceeds to overwrite them all with a block from the copy_area. Simple!

	// So what tile are we gonna' replace?
	int replace_me = TILEMAPS_get_tile (tilemap_number,slayer,sx,sy);

	int source_tile_x;
	int source_tile_y;

	int list_size;
	int reading_from;

	short spare_tile;

	fill_spreader *list_pointer = NULL;

	list_size = 0;
	reading_from = 0;

	list_pointer = EDIT_add_filler (list_size, list_pointer, sx, sy);
	TILEMAPS_put_tile ( tilemap_number , slayer , list_pointer[reading_from].x , list_pointer[reading_from].y , -1 );
	list_size++;

	do {

		if ( ( TILEMAPS_get_tile (tilemap_number, slayer, list_pointer[reading_from].x-1, list_pointer[reading_from].y ) == replace_me ) && (list_pointer[reading_from].x-1 >= 0) )
		{
			list_pointer = EDIT_add_filler ( list_size, list_pointer, list_pointer[reading_from].x-1, list_pointer[reading_from].y );
			TILEMAPS_put_tile (tilemap_number, slayer, list_pointer[reading_from].x-1, list_pointer[reading_from].y, -1);
			// Set the tile to -1 so it's not looked at again.
			list_size++;
		}

		if ( ( TILEMAPS_get_tile (tilemap_number, slayer, list_pointer[reading_from].x+1, list_pointer[reading_from].y ) == replace_me ) && (list_pointer[reading_from].x+1 < cm[tilemap_number].map_width) )
		{
			list_pointer = EDIT_add_filler ( list_size, list_pointer, list_pointer[reading_from].x+1, list_pointer[reading_from].y );
			TILEMAPS_put_tile (tilemap_number, slayer, list_pointer[reading_from].x+1, list_pointer[reading_from].y, -1);
			list_size++;
		}

		if ( ( TILEMAPS_get_tile (tilemap_number, slayer, list_pointer[reading_from].x, list_pointer[reading_from].y-1 ) == replace_me ) && (list_pointer[reading_from].y-1 >= 0) )
		{
			list_pointer = EDIT_add_filler ( list_size, list_pointer, list_pointer[reading_from].x, list_pointer[reading_from].y-1 );
			TILEMAPS_put_tile (tilemap_number, slayer, list_pointer[reading_from].x, list_pointer[reading_from].y-1, -1);
			list_size++;
		}

		if ( ( TILEMAPS_get_tile (tilemap_number, slayer, list_pointer[reading_from].x, list_pointer[reading_from].y+1 ) == replace_me ) && (list_pointer[reading_from].y+1 < cm[tilemap_number].map_height) )
		{
			list_pointer = EDIT_add_filler ( list_size, list_pointer, list_pointer[reading_from].x, list_pointer[reading_from].y+1 );
			TILEMAPS_put_tile (tilemap_number, slayer, list_pointer[reading_from].x, list_pointer[reading_from].y+1, -1);
			list_size++;
		}
		
		reading_from++;
		
	} while(reading_from < list_size);
	
	// Okay now we've propagated the list...

	for (reading_from = 0; reading_from<list_size ; reading_from++)
	{
		source_tile_x = (list_pointer[reading_from].x + sx) % width;
		source_tile_y = (list_pointer[reading_from].y + sy) % height;

		// First replace the -1 with the old tile value regardless...
		TILEMAPS_put_tile(tilemap_number,slayer,list_pointer[reading_from].x,list_pointer[reading_from].y,replace_me);

		// Then use the TILEMAPS_paste_map_section function so that it takes into account the various editing settings...
		spare_tile = copy_area [(source_tile_y * width) + source_tile_x];

		TILEMAPS_paste_map_section (tilemap_number, slayer, list_pointer[reading_from].x, list_pointer[reading_from].y, 1, 1, 1, &spare_tile, tile_edit_mode);
	}

	// And now we clear it.

	free (list_pointer);
	list_pointer = NULL; // Because I'm anal...
}



void TILEMAPS_draw (int start_layer, int end_layer, int tilemap_number, int sx, int sy, int width, int height, float zoom_level, int colour_display_mode)
{
	int tilesize = TILESETS_get_tilesize (cm[tilemap_number].tile_set_index);
	int sprite_index = TILESETS_get_sprite_index (cm[tilemap_number].tile_set_index);
	int tile_count = TILESETS_get_tile_count (cm[tilemap_number].tile_set_index);
	int x,y,layer;
	int tile_value;

	int tiles_per_row = int (float(width/tilesize)/zoom_level);
	int tiles_per_col = int (float(height/tilesize)/zoom_level);

	simple_vertex_colours *simple_vertex_pointer;
	detailed_vertex_colours *detailed_vertex_pointer;

	if (sprite_index != UNSET) // Check that the tilemap of the graphic exists.
	{
		switch(colour_display_mode)
		{
		case COLOUR_DISPLAY_MODE_NONE:
			for (layer=start_layer; layer<end_layer; layer++)
			{
				for (y=0; y<tiles_per_col ; y++)
				{
					for (x=0; x<tiles_per_row ; x++)
					{
						tile_value = TILEMAPS_get_tile (tilemap_number, layer, sx+x, sy+y);
						simple_vertex_pointer = TILEMAPS_get_simple_vertex_pointer (tilemap_number, layer, sx+x, sy+y);
						detailed_vertex_pointer = TILEMAPS_get_complex_vertex_pointer (tilemap_number, layer, sx+x, sy+y);

						if ( (tile_value > 0) && (tile_value < tile_count) )
						{
							OUTPUT_draw_sprite_scale (sprite_index, tile_value, int(float(x*tilesize)*zoom_level), int(float(y*tilesize)*zoom_level) , zoom_level );
						}
						else if (tile_value >= tile_count) // Tile is out of range of current tileset.
						{
							OUTPUT_rectangle ( int(float(x*tilesize)*zoom_level) , int(float(y*tilesize)*zoom_level) , int(float(x*tilesize)*zoom_level) + int(float(tilesize)*zoom_level) , int(float(y*tilesize)*zoom_level) + int(float(tilesize)*zoom_level) , 255 , 0 , 0 );
						}
					}
				}
			}
			break;

		case COLOUR_DISPLAY_MODE_LAYER_VERTEX:
			for (layer=start_layer; layer<end_layer; layer++)
			{
				for (y=0; y<tiles_per_col ; y++)
				{
					for (x=0; x<tiles_per_row ; x++)
					{
						tile_value = TILEMAPS_get_tile (tilemap_number, layer, sx+x, sy+y);
						simple_vertex_pointer = TILEMAPS_get_simple_vertex_pointer (tilemap_number, layer, sx+x, sy+y);
						detailed_vertex_pointer = TILEMAPS_get_complex_vertex_pointer (tilemap_number, layer, sx+x, sy+y);

						if ( (tile_value > 0) && (tile_value < tile_count) )
						{
							OUTPUT_draw_sprite_scale (sprite_index, tile_value, int(float(x*tilesize)*zoom_level), int(float(y*tilesize)*zoom_level) , float(zoom_level) );
						}
						else if (tile_value >= tile_count) // Tile is out of range of current tileset.
						{
							OUTPUT_rectangle ( int(float(x*tilesize)*zoom_level) , int(float(y*tilesize)*zoom_level) , int(float(x*tilesize)*zoom_level) + int(float(tilesize)*zoom_level) , int(float(y*tilesize)*zoom_level) + int(float(tilesize)*zoom_level) , 255 , 0 , 0 );
						}
					}
				}
			}
			break;

		case COLOUR_DISPLAY_MODE_SIMPLE_VERTEX:
			for (layer=start_layer; layer<end_layer; layer++)
			{
				for (y=0; y<tiles_per_col ; y++)
				{
					for (x=0; x<tiles_per_row ; x++)
					{
						tile_value = TILEMAPS_get_tile (tilemap_number, layer, sx+x, sy+y);
						simple_vertex_pointer = TILEMAPS_get_simple_vertex_pointer (tilemap_number, layer, sx+x, sy+y);
						detailed_vertex_pointer = TILEMAPS_get_complex_vertex_pointer (tilemap_number, layer, sx+x, sy+y);

						if ( (tile_value > 0) && (tile_value < tile_count) )
						{
							OUTPUT_draw_sprite_scale (sprite_index, tile_value, int(float(x*tilesize)*zoom_level) , int(float(y*tilesize)*zoom_level) , float(zoom_level) );
						}
						else if (tile_value >= tile_count) // Tile is out of range of current tileset.
						{
							OUTPUT_rectangle ( int(float(x*tilesize)*zoom_level) , int(float(y*tilesize)*zoom_level) , int(float(x*tilesize)*zoom_level) + int(float(tilesize)*zoom_level) , int(float(y*tilesize)*zoom_level) + int(float(tilesize)*zoom_level) , 255 , 0 , 0 );
						}
					}
				}
			}
			break;

		case COLOUR_DISPLAY_MODE_COMPLEX_VERTEX:
			for (layer=start_layer; layer<end_layer; layer++)
			{
				for (y=0; y<tiles_per_col ; y++)
				{
					for (x=0; x<tiles_per_row ; x++)
					{
						tile_value = TILEMAPS_get_tile (tilemap_number, layer, sx+x, sy+y);
						simple_vertex_pointer = TILEMAPS_get_simple_vertex_pointer (tilemap_number, layer, sx+x, sy+y);
						detailed_vertex_pointer = TILEMAPS_get_complex_vertex_pointer (tilemap_number, layer, sx+x, sy+y);

						if ( (tile_value > 0) && (tile_value < tile_count) )
						{
							OUTPUT_draw_sprite_scale (sprite_index, tile_value, int(float(x*tilesize)*zoom_level) , int(float(y*tilesize)*zoom_level) , float(zoom_level) );
						}
						else if (tile_value >= tile_count) // Tile is out of range of current tileset.
						{
							OUTPUT_rectangle ( int(float(x*tilesize)*zoom_level) , int(float(y*tilesize)*zoom_level) , int(float(x*tilesize)*zoom_level) + int(float(tilesize)*zoom_level) , int(float(y*tilesize)*zoom_level) + int(float(tilesize)*zoom_level) , 255 , 0 , 0 );
						}
					}
				}
			}
			break;

		default:
			assert(0);
			break;
		}


	}
}



void TILEMAPS_draw_stored_tiles (short *source, int source_width, int source_height, int tilemap_number, int sx, int sy, int width, int height, float zoom_level)
{
	int tilesize = TILESETS_get_tilesize (cm[tilemap_number].tile_set_index);
	int sprite_index = TILESETS_get_sprite_index (cm[tilemap_number].tile_set_index);
	int tile_count = TILESETS_get_tile_count (cm[tilemap_number].tile_set_index);
	int x,y;
	int tile_value;

	int tiles_per_row = int(float(width/tilesize)/zoom_level);
	int tiles_per_col = int(float(height/tilesize)/zoom_level);

	int counter = 0;

	if (sprite_index != UNSET) // Check that the tilemap of the graphic exists.
	{
		for (x=0; ((sx+x)<tiles_per_row) && (x<source_width) ; x++)
		{
			for (y=0; ((sy+y)<tiles_per_col) && (y<source_height) ; y++)
			{
				tile_value = source[x*source_height + y];

				if ( (tile_value > 0) && (tile_value < tile_count) )
				{
					OUTPUT_draw_sprite_scale (sprite_index, tile_value, int(float((sx+x)*tilesize)*zoom_level), int(float((sy+y)*tilesize)*zoom_level) , zoom_level );
				}
				else if (tile_value >= tile_count) // Tile is out of range of current tileset.
				{
					OUTPUT_rectangle ( int(float((sx+x)*tilesize)*zoom_level) , int(float((sy+y)*tilesize)*zoom_level) , int(float((sx+x)*tilesize)*zoom_level)+int(float(tilesize)*zoom_level) , int(float((sy+y)*tilesize)*zoom_level)+int(float(tilesize)*zoom_level) , 255 , 0 , 0 );
				}

				counter++;
			}
		}

	}
}



void TILEMAPS_get_free_map_name (char *name)
{
	int i;
	int test_num;

	test_num = 0;

	for (i=0; i<number_of_tilemaps_loaded; i++)
	{
		sprintf(name,"TILEMAP_#%3d",test_num);
		STRING_replace_char (name, ' ' , '0');
		if (strcmp(name,cm[i].name) == 0)
		{
			test_num++;
		}
		// As the tilemap list is alphabetised it should mean that test_num ends
		// up pointing to the first free name of the "Tilemap_#num" variety.
	}

	sprintf(name,"TILEMAP_#%3d",test_num);
	STRING_replace_char (name, ' ' , '0');
}



int TILEMAPS_create (bool new_tilemap)
{	
	char default_name[NAME_SIZE];
//	int colour;

	if (cm == NULL) // first in list...
	{
		cm = (tilemap *) malloc (sizeof (tilemap));
	}
	else // Already someone in the list...
	{
		cm = (tilemap *) realloc ( cm, (number_of_tilemaps_loaded + 1) * sizeof (tilemap) );
	}

	cm[number_of_tilemaps_loaded].map_pointer = NULL;
	cm[number_of_tilemaps_loaded].group_pointer = NULL;
	cm[number_of_tilemaps_loaded].helper_tag_pointer = NULL;
	cm[number_of_tilemaps_loaded].helper_x_offset_pointer = NULL;
	cm[number_of_tilemaps_loaded].helper_y_offset_pointer = NULL;
	cm[number_of_tilemaps_loaded].vertex_colours_pointer = NULL;
	cm[number_of_tilemaps_loaded].detailed_vertex_colours_pointer = NULL;
	cm[number_of_tilemaps_loaded].layer_vertex_colours_pointer = NULL;

	cm[number_of_tilemaps_loaded].backup_map_pointer = NULL;
	cm[number_of_tilemaps_loaded].backup_in_use = false;

	cm[number_of_tilemaps_loaded].ai_node_lookup_table_list_pointer = NULL;

	cm[number_of_tilemaps_loaded].map_width = 0;
	cm[number_of_tilemaps_loaded].map_height = 0;
	cm[number_of_tilemaps_loaded].map_layers = 0;

	cm[number_of_tilemaps_loaded].spawn_point_next_uid = 0;
	cm[number_of_tilemaps_loaded].waypoint_next_uid = 0;
	cm[number_of_tilemaps_loaded].zone_next_uid = 0;
	cm[number_of_tilemaps_loaded].uid = next_free_map_uid;

	cm[number_of_tilemaps_loaded].altered_this_load = true;

	cm[number_of_tilemaps_loaded].spawn_point_list_pointer = NULL;
	cm[number_of_tilemaps_loaded].zone_list_pointer = NULL;
	cm[number_of_tilemaps_loaded].waypoint_list_pointer = NULL;
	cm[number_of_tilemaps_loaded].gate_list_pointer = NULL;
	cm[number_of_tilemaps_loaded].ai_zone_list_pointer = NULL;
	cm[number_of_tilemaps_loaded].connection_list_pointer = NULL;

	cm[number_of_tilemaps_loaded].spawn_points = 0;
	cm[number_of_tilemaps_loaded].zones = 0;
	cm[number_of_tilemaps_loaded].waypoints = 0;
	cm[number_of_tilemaps_loaded].gates = 0;
	cm[number_of_tilemaps_loaded].ai_zones = 0;
	cm[number_of_tilemaps_loaded].remotes = 0;

	cm[number_of_tilemaps_loaded].collision_bitmask_data_pointer = NULL;
	cm[number_of_tilemaps_loaded].collision_data_pointer = NULL;
	cm[number_of_tilemaps_loaded].exposure_data_pointer = NULL;
	
	cm[number_of_tilemaps_loaded].localised_zone_list_pointer = NULL;
	cm[number_of_tilemaps_loaded].localised_zone_list_size = 0;

	cm[number_of_tilemaps_loaded].vertex_colour_mode = VERTEX_MODE_NONE;

	cm[number_of_tilemaps_loaded].tile_set_index = UNSET;

	if (new_tilemap) // ie, we're not just freeing up space for a loaded tileset, but rather it's a brand-spanking-new one.
	{
		strcpy ( cm[number_of_tilemaps_loaded].default_tile_set , "UNSET" );
		strcpy ( cm[number_of_tilemaps_loaded].old_name , "UNSET" );
		cm[number_of_tilemaps_loaded].map_width = 1;
		cm[number_of_tilemaps_loaded].map_height = 1;
		cm[number_of_tilemaps_loaded].map_layers = 1;

		cm[number_of_tilemaps_loaded].map_pointer = (short *) malloc (cm[number_of_tilemaps_loaded].map_layers * cm[number_of_tilemaps_loaded].map_width * cm[number_of_tilemaps_loaded].map_height * sizeof(short));
		cm[number_of_tilemaps_loaded].map_pointer[0] = 0;

		cm[number_of_tilemaps_loaded].group_pointer = (char *) malloc (cm[number_of_tilemaps_loaded].map_layers * cm[number_of_tilemaps_loaded].map_width * cm[number_of_tilemaps_loaded].map_height * sizeof(char));
		cm[number_of_tilemaps_loaded].group_pointer[0] = char(0);

		cm[number_of_tilemaps_loaded].helper_tag_pointer = (char *) malloc (cm[number_of_tilemaps_loaded].map_layers * cm[number_of_tilemaps_loaded].map_width * cm[number_of_tilemaps_loaded].map_height * sizeof(char));
		cm[number_of_tilemaps_loaded].helper_tag_pointer[0] = char(0);

		cm[number_of_tilemaps_loaded].helper_x_offset_pointer = (char *) malloc (cm[number_of_tilemaps_loaded].map_layers * cm[number_of_tilemaps_loaded].map_width * cm[number_of_tilemaps_loaded].map_height * sizeof(char));
		cm[number_of_tilemaps_loaded].helper_x_offset_pointer[0] = char(0);

		cm[number_of_tilemaps_loaded].helper_y_offset_pointer = (char *) malloc (cm[number_of_tilemaps_loaded].map_layers * cm[number_of_tilemaps_loaded].map_width * cm[number_of_tilemaps_loaded].map_height * sizeof(char));
		cm[number_of_tilemaps_loaded].helper_y_offset_pointer[0] = char(0);

//		These are left as NULL until by choosing that editing mode in the editor, it mallocs some memory.

//		cm[number_of_tilemaps_loaded].layer_vertex_colours_pointer = (simple_vertex_colours *) malloc (cm[number_of_tilemaps_loaded].map_layers * sizeof(simple_vertex_colours));
//		cm[number_of_tilemaps_loaded].vertex_colours_pointer = (simple_vertex_colours *) malloc (cm[number_of_tilemaps_loaded].map_layers * cm[number_of_tilemaps_loaded].map_width * cm[number_of_tilemaps_loaded].map_height * sizeof(simple_vertex_colours));
//		cm[number_of_tilemaps_loaded].detailed_vertex_colours_pointer = (detailed_vertex_colours *) malloc (cm[number_of_tilemaps_loaded].map_layers * cm[number_of_tilemaps_loaded].map_width * cm[number_of_tilemaps_loaded].map_height * sizeof(detailed_vertex_colours));

//		for (colour=0; colour<3; colour++)
//		{
//			cm[number_of_tilemaps_loaded].vertex_colours_pointer[0].colours[colour] = 1.0f;
//			cm[number_of_tilemaps_loaded].layer_vertex_colours_pointer[0].colours[colour] = 1.0f;
//		}

//		for (colour=0; colour<12; colour++)
//		{
//			cm[number_of_tilemaps_loaded].detailed_vertex_colours_pointer[0].colours[colour] = 1.0f;
//		}

	}

	TILEMAPS_get_free_map_name (default_name);
	strcpy ( cm[number_of_tilemaps_loaded].name , default_name );

	number_of_tilemaps_loaded++;
	return (number_of_tilemaps_loaded-1);
}



void TILEMAPS_load (char *filename , int tilemap_number)
{
	// This loads the verbose text-based map structure.

	bool rle_map = false;
	bool exitflag = false;
	bool exitmainloop = false;

	int t;

	int zone_number;
	int spawn_point_number;
	int parameter_number;

	int layer;

	int gate_number;
	int remote_number;
	int waypoint_number;
	int connection_number;

	char line[TEXT_LINE_SIZE];

	char full_filename [TEXT_LINE_SIZE];

	char *pointer;
	char *param_pointer;

	int colour;

	char temp_char [TEXT_LINE_SIZE];

	append_filename(full_filename,"tilemaps", filename, sizeof(full_filename) );

	FILE *file_pointer = fopen (MAIN_get_project_filename (full_filename),"r");

	if (file_pointer != NULL)
	{
		// Set some defaults seeing as we've found a map...
		cm[tilemap_number].spawn_point_list_pointer = NULL;
		cm[tilemap_number].zone_list_pointer = NULL;
		cm[tilemap_number].map_pointer = NULL;
		cm[tilemap_number].collision_data_pointer = NULL;
		cm[tilemap_number].exposure_data_pointer = NULL;
		cm[tilemap_number].collision_bitmask_data_pointer = NULL;
		cm[tilemap_number].optimisation_data = NULL;
		cm[tilemap_number].altered_this_load = false;
		cm[tilemap_number].optimisation_data_flag = false;
		strcpy (cm[tilemap_number].default_tile_set,"UNSET");

		cm[tilemap_number].vertex_colour_mode = VERTEX_MODE_NONE;

		strupr(filename); // Just in case it ain't upper case already.
		strtok(filename,"."); // Get rid of extension.
		strcpy (cm[tilemap_number].name , filename);
		strcpy (cm[tilemap_number].old_name , filename);

		while ( ( fgets ( line , TEXT_LINE_SIZE , file_pointer ) != NULL ) && (exitmainloop == false) )
		{
			STRING_strip_newlines (line);
			strupr(line);

			pointer = strstr(line,"#END OF FILE");
			if (pointer != NULL) // It's the end of the world as we know it...
			{
				exitmainloop = true;
			}

			pointer = STRING_end_of_string(line,"#DEFAULT TILE SET = ");
			if (pointer != NULL) // It's the default tile set! Wooty-wooty-woot! *ahem*
			{
				strcpy (cm[tilemap_number].default_tile_set , pointer);
			}

			pointer = STRING_end_of_string(line,"#VERTEX_MODE_NONE");
			if (pointer != NULL) // No vertex colours for you!
			{
				cm[tilemap_number].vertex_colour_mode = VERTEX_MODE_NONE;
			}

			pointer = STRING_end_of_string(line,"#VERTEX_MODE_SINGLE_MAP_VALUE");
			if (pointer != NULL) // No vertex colours for you!
			{
				cm[tilemap_number].vertex_colour_mode = VERTEX_MODE_SINGLE_MAP_VALUE;
			}

			pointer = STRING_end_of_string(line,"#VERTEX_MODE_SIMPLE_TILE_VALUES");
			if (pointer != NULL) // No vertex colours for you!
			{
				cm[tilemap_number].vertex_colour_mode = VERTEX_MODE_SIMPLE_TILE_VALUES;
			}

			pointer = STRING_end_of_string(line,"#VERTEX_MODE_COMPLEX_TILE_VALUES");
			if (pointer != NULL) // No vertex colours for you!
			{
				cm[tilemap_number].vertex_colour_mode = VERTEX_MODE_COMPLEX_TILE_VALUES;
			}
	
			pointer = STRING_end_of_string(line,"#DEFAULT TILE SET = ");
			if (pointer != NULL) // It's the default tile set! Wooty-wooty-woot! *ahem*
			{
				strcpy (cm[tilemap_number].default_tile_set , pointer);
			}

			pointer = STRING_end_of_string(line,"#MAP WIDTH = ");
			if (pointer != NULL) // How wide is the map?
			{
				cm[tilemap_number].map_width = atoi(pointer);	
			}

			pointer = STRING_end_of_string(line,"#MAP HEIGHT = ");
			if (pointer != NULL) // How tall is the map?
			{
				cm[tilemap_number].map_height = atoi(pointer);	
			}

			pointer = STRING_end_of_string(line,"#MAP LAYERS = ");
			if (pointer != NULL) // How deep is the map?
			{
				cm[tilemap_number].map_layers = atoi(pointer);	
			}

			pointer = STRING_end_of_string(line,"#MAP UID = ");
			if (pointer != NULL) // How deep is the map?
			{
				cm[tilemap_number].uid = atoi(pointer);	
			}

			pointer = STRING_end_of_string(line,"#MAP SPAWN POINT NEXT UID = ");
			if (pointer != NULL) // How deep is the map?
			{
				cm[tilemap_number].spawn_point_next_uid = atoi(pointer);	
			}

			pointer = STRING_end_of_string(line,"#MAP WAYPOINT NEXT UID = ");
			if (pointer != NULL) // How deep is the map?
			{
				cm[tilemap_number].waypoint_next_uid = atoi(pointer);	
			}

			pointer = STRING_end_of_string(line,"#MAP ZONE NEXT UID = ");
			if (pointer != NULL) // How deep is the map?
			{
				cm[tilemap_number].zone_next_uid = atoi(pointer);	
			}

			pointer = STRING_end_of_string(line,"#RLE MAP = ");
			if (pointer != NULL) // Is it a run-length-encoded map?
			{
				strcpy(temp_char , pointer);
				pointer = strstr(temp_char,"TRUE");
				if (pointer != NULL)
				{
					rle_map = true;
				}
				else
				{
					rle_map = false;
				}
			}

			if (cm[tilemap_number].vertex_colour_mode == VERTEX_MODE_SINGLE_MAP_VALUE)
			{
				pointer = strstr(line,"#MAP DEFAULT VERTEX COLOUR");
				if (pointer != NULL) // It's the map layer vertex data.
				{
					for (layer=0; layer<cm[tilemap_number].map_layers; layer++)
					{
						for (colour=0; colour<3; colour++)
						{
							cm[tilemap_number].layer_vertex_colours_pointer[layer].colours[colour] = FILE_get_float_from_file (file_pointer);
						}
					}
				}
			}

			pointer = strstr(line,"#MAP TILE DATA");
			if (pointer != NULL) // It's the map data.
			{
				// Set aside some space for the map...
				cm[tilemap_number].map_pointer = (short *) malloc (cm[tilemap_number].map_layers * cm[tilemap_number].map_height * cm[tilemap_number].map_width * sizeof(short));

				for (t=0 ; t<cm[tilemap_number].map_layers * cm[tilemap_number].map_width * cm[tilemap_number].map_height ; t++)
				{
					cm[tilemap_number].map_pointer[t] = FILE_get_int_from_file (file_pointer);
				}
			}

			pointer = strstr(line,"#MAP GROUP DATA");
			if (pointer != NULL) // It's the map data.
			{
				// Set aside some space for the map...
				cm[tilemap_number].group_pointer = (char *) malloc (cm[tilemap_number].map_layers * cm[tilemap_number].map_height * cm[tilemap_number].map_width * sizeof(char));
		
				for (t=0 ; t<cm[tilemap_number].map_layers * cm[tilemap_number].map_width * cm[tilemap_number].map_height ; t++)
				{
					cm[tilemap_number].group_pointer[t] = FILE_get_int_from_file (file_pointer);
				}
			}

			pointer = strstr(line,"#MAP HELPER TAG DATA");
			if (pointer != NULL) // It's the map data.
			{
				// Set aside some space for the map...
				cm[tilemap_number].helper_tag_pointer = (char *) malloc (cm[tilemap_number].map_layers * cm[tilemap_number].map_height * cm[tilemap_number].map_width * sizeof(char));
				cm[tilemap_number].helper_x_offset_pointer = (char *) malloc (cm[tilemap_number].map_layers * cm[tilemap_number].map_height * cm[tilemap_number].map_width * sizeof(char));
				cm[tilemap_number].helper_y_offset_pointer = (char *) malloc (cm[tilemap_number].map_layers * cm[tilemap_number].map_height * cm[tilemap_number].map_width * sizeof(char));
				
				for (t=0 ; t<cm[tilemap_number].map_layers * cm[tilemap_number].map_width * cm[tilemap_number].map_height ; t++)
				{
					cm[tilemap_number].helper_tag_pointer[t] = FILE_get_int_from_file (file_pointer);
					cm[tilemap_number].helper_x_offset_pointer[t] = FILE_get_int_from_file (file_pointer);
					cm[tilemap_number].helper_y_offset_pointer[t] = FILE_get_int_from_file (file_pointer);
				}
			}

			if (cm[tilemap_number].vertex_colour_mode == VERTEX_MODE_SIMPLE_TILE_VALUES)
			{
				pointer = strstr(line,"#MAP VERTEX DATA");
				if (pointer != NULL) // It's the map data.
				{
					// Set aside some space for the map...
					cm[tilemap_number].vertex_colours_pointer = (simple_vertex_colours *) malloc (cm[tilemap_number].map_layers * cm[tilemap_number].map_height * cm[tilemap_number].map_width * sizeof(simple_vertex_colours));
		
					for (t=0 ; t<cm[tilemap_number].map_layers * cm[tilemap_number].map_width * cm[tilemap_number].map_height ; t++)
					{
						for (colour=0; colour<3; colour++)
						{
							cm[tilemap_number].vertex_colours_pointer[t].colours[colour] = FILE_get_float_from_file (file_pointer);
						}
					}
				}
			}

			if (cm[tilemap_number].vertex_colour_mode == VERTEX_MODE_COMPLEX_TILE_VALUES)
			{
				pointer = strstr(line,"#MAP DETAILED VERTEX DATA");
				if (pointer != NULL) // It's the map data.
				{
					// Set aside some space for the map...
					cm[tilemap_number].detailed_vertex_colours_pointer = (detailed_vertex_colours *) malloc (cm[tilemap_number].map_layers * cm[tilemap_number].map_height * cm[tilemap_number].map_width * sizeof(detailed_vertex_colours));
		
					for (t=0 ; t<cm[tilemap_number].map_layers * cm[tilemap_number].map_width * cm[tilemap_number].map_height ; t++)
					{
						for (colour=0; colour<12; colour++)
						{
							cm[tilemap_number].detailed_vertex_colours_pointer[t].colours[colour] = FILE_get_float_from_file (file_pointer);
						}
					}
				}
			}

			pointer = STRING_end_of_string(line,"#ZONE DATA = ");
			if (pointer != NULL) // It's the zone data.
			{
				cm[tilemap_number].zones = atoi(pointer);
				// Set aside the memory needed for the list of zones.
				cm[tilemap_number].zone_list_pointer = (zone *) malloc (cm[tilemap_number].zones * sizeof(zone));

				exitflag = false;
				zone_number = 0;

				while (exitflag == false)
				{
					fgets ( line , TEXT_LINE_SIZE , file_pointer );
					STRING_strip_newlines (line);

					pointer = STRING_end_of_string(line,"#ZONE NUMBER = ");
					if (pointer != NULL)
					{
						zone_number = atoi(pointer);
						cm[tilemap_number].zone_list_pointer[zone_number].spawn_point_index_list = NULL;
						cm[tilemap_number].zone_list_pointer[zone_number].spawn_point_index_list_size = 0;
						cm[tilemap_number].zone_list_pointer[zone_number].flag = 0;
					}

					pointer = STRING_end_of_string(line,"#NAME = ");
					if (pointer != NULL)
					{
						strcpy (cm[tilemap_number].zone_list_pointer[zone_number].text_tag , pointer);
					}

					pointer = STRING_end_of_string(line,"#X POS = ");
					if (pointer != NULL)
					{
						cm[tilemap_number].zone_list_pointer[zone_number].x = atoi(pointer);
					}

					pointer = STRING_end_of_string(line,"#Y POS = ");
					if (pointer != NULL)
					{
						cm[tilemap_number].zone_list_pointer[zone_number].y = atoi(pointer);
					}

					pointer = STRING_end_of_string(line,"#WIDTH = ");
					if (pointer != NULL)
					{
						cm[tilemap_number].zone_list_pointer[zone_number].width = atoi(pointer);
					}

					pointer = STRING_end_of_string(line,"#HEIGHT = ");
					if (pointer != NULL)
					{
						cm[tilemap_number].zone_list_pointer[zone_number].height = atoi(pointer);
					}

					pointer = STRING_end_of_string(line,"#ID TYPE = ");
					if (pointer != NULL)
					{
						strcpy (cm[tilemap_number].zone_list_pointer[zone_number].type , pointer);
					}

					pointer = strstr(line,"#END OF ZONE DATA");
					if (pointer != NULL)
					{
						exitflag = true;
					}

					pointer = STRING_end_of_string(line,"#UID = ");
					if (pointer != NULL)
					{
						cm[tilemap_number].zone_list_pointer[zone_number].uid = atoi(pointer);
					}
				}
			}

			pointer = strstr(line,"#SPAWN POINT DATA = ");
			if (pointer != NULL) // It's the spawn point data.
			{
				cm[tilemap_number].spawn_points = atoi(&line[strlen("#SPAWN POINT DATA = ")]);
				// Set aside the memory needed for the list of spawn points.
				cm[tilemap_number].spawn_point_list_pointer = (spawn_point *) malloc (cm[tilemap_number].spawn_points * sizeof(spawn_point));

				spawn_point_number = 0;
				exitflag = false;

				while (exitflag == false)
				{
					fgets ( line , TEXT_LINE_SIZE , file_pointer );
					STRING_strip_newlines (line);

					pointer = STRING_end_of_string(line,"#SPAWN POINT NUMBER = ");
					if (pointer != NULL)
					{
						spawn_point_number = atoi(pointer);
						cm[tilemap_number].spawn_point_list_pointer[spawn_point_number].flag = 0;
					}

					pointer = STRING_end_of_string(line,"#UNIQUE IDENTIFIER NUMBER = ");
					if (pointer != NULL)
					{
						cm[tilemap_number].spawn_point_list_pointer[spawn_point_number].uid = atoi(pointer);
					}

					pointer = STRING_end_of_string(line,"#PARENT UID = ");
					if (pointer != NULL)
					{
						cm[tilemap_number].spawn_point_list_pointer[spawn_point_number].parent_uid = atoi(pointer);
					}

					pointer = STRING_end_of_string(line,"#NEXT SIBLING UID = ");
					if (pointer != NULL)
					{
						cm[tilemap_number].spawn_point_list_pointer[spawn_point_number].next_sibling_uid = atoi(pointer);
					}

					pointer = STRING_end_of_string(line,"#NAME = ");
					if (pointer != NULL)
					{
						strcpy (cm[tilemap_number].spawn_point_list_pointer[spawn_point_number].name , pointer);
					}

					pointer = STRING_end_of_string(line,"#SCRIPT = ");
					if (pointer != NULL)
					{
						strcpy (cm[tilemap_number].spawn_point_list_pointer[spawn_point_number].script_name , pointer);
					}

					pointer = STRING_end_of_string(line,"#X POS = ");
					if (pointer != NULL)
					{
						cm[tilemap_number].spawn_point_list_pointer[spawn_point_number].x = atoi(pointer);
					}

					pointer = STRING_end_of_string(line,"#Y POS = ");
					if (pointer != NULL)
					{
						cm[tilemap_number].spawn_point_list_pointer[spawn_point_number].y = atoi(pointer);
					}

					pointer = STRING_end_of_string(line,"#ID TYPE = ");
					if (pointer != NULL)
					{
						strcpy (cm[tilemap_number].spawn_point_list_pointer[spawn_point_number].type , pointer);
					}

					pointer = STRING_end_of_string(line,"#PARAMETERS = ");
					if (pointer != NULL)
					{

						cm[tilemap_number].spawn_point_list_pointer[spawn_point_number].params = atoi(pointer);

						if (cm[tilemap_number].spawn_point_list_pointer[spawn_point_number].params > 0)
						{
							// Set aside the memory needed for the list of parameters.
							cm[tilemap_number].spawn_point_list_pointer[spawn_point_number].param_list_pointer = (parameter *) malloc (cm[tilemap_number].spawn_point_list_pointer[spawn_point_number].params * sizeof(parameter));
						}
						else
						{
							cm[tilemap_number].spawn_point_list_pointer[spawn_point_number].param_list_pointer = NULL;
						}

						for (parameter_number=0; parameter_number<cm[tilemap_number].spawn_point_list_pointer[spawn_point_number].params; parameter_number++)
						{
							fgets ( line , TEXT_LINE_SIZE , file_pointer );
							STRING_strip_newlines (line);
							pointer = STRING_end_of_string(line,"#PARAMETER ");

							param_pointer = strtok(pointer,"=:\n");
							strcpy (cm[tilemap_number].spawn_point_list_pointer[spawn_point_number].param_list_pointer[parameter_number].param_dest_var , param_pointer);
							
							param_pointer = strtok(NULL,"=:\n");
							strcpy (cm[tilemap_number].spawn_point_list_pointer[spawn_point_number].param_list_pointer[parameter_number].param_list_type , param_pointer);

							param_pointer = strtok(NULL,"=:\n");
							strcpy (cm[tilemap_number].spawn_point_list_pointer[spawn_point_number].param_list_pointer[parameter_number].param_name , param_pointer);
						}
					}

					pointer = strstr(line,"#END OF SPAWN POINT DATA");
					if (pointer != NULL)
					{
						exitflag = true;
					}
				}
			}

			pointer = strstr(line,"#AI ZONE GATE DATA = ");
			if (pointer != NULL) // It's the spawn point data.
			{
				cm[tilemap_number].gates = atoi(&line[strlen("#AI ZONE GATE DATA = ")]);
				// Set aside the memory needed for the list of spawn points.
				cm[tilemap_number].gate_list_pointer = (gate *) malloc (cm[tilemap_number].gates * sizeof(gate));

				gate_number = 0;
				exitflag = false;

				while (exitflag == false)
				{
					fgets ( line , TEXT_LINE_SIZE , file_pointer );
					STRING_strip_newlines (line);

					pointer = STRING_end_of_string(line,"#GATE NUMBER = ");
					if (pointer != NULL)
					{
						gate_number = atoi(pointer);
					}

					pointer = STRING_end_of_string(line,"#START X = ");
					if (pointer != NULL)
					{
						cm[tilemap_number].gate_list_pointer[gate_number].x1 = atoi(pointer);
					}

					pointer = STRING_end_of_string(line,"#START Y = ");
					if (pointer != NULL)
					{
						cm[tilemap_number].gate_list_pointer[gate_number].y1 = atoi(pointer);
					}

					pointer = STRING_end_of_string(line,"#END X = ");
					if (pointer != NULL)
					{
						cm[tilemap_number].gate_list_pointer[gate_number].x2 = atoi(pointer);
					}

					pointer = STRING_end_of_string(line,"#END Y = ");
					if (pointer != NULL)
					{
						cm[tilemap_number].gate_list_pointer[gate_number].y2 = atoi(pointer);
					}

					pointer = strstr(line,"#END OF AI ZONE GATE DATA");
					if (pointer != NULL)
					{
						exitflag = true;
					}
				}
			}

			pointer = strstr(line,"#AI ZONE REMOTE DATA = ");
			if (pointer != NULL) // It's the spawn point data.
			{
				cm[tilemap_number].remotes = atoi(&line[strlen("#AI ZONE REMOTE DATA = ")]);
				// Set aside the memory needed for the list of spawn points.
				cm[tilemap_number].connection_list_pointer = (remote_connection *) malloc (cm[tilemap_number].remotes * sizeof(remote_connection));

				remote_number = 0;
				exitflag = false;

				while (exitflag == false)
				{
					fgets ( line , TEXT_LINE_SIZE , file_pointer );
					STRING_strip_newlines (line);

					pointer = STRING_end_of_string(line,"#REMOTE CONNECTION NUMBER = ");
					if (pointer != NULL)
					{
						remote_number = atoi(pointer);
					}

					pointer = STRING_end_of_string(line,"#CONNECTIVE DIRECTION = ");
					if (pointer != NULL)
					{
						cm[tilemap_number].connection_list_pointer[remote_number].connective_direction = atoi(pointer);
					}

					pointer = STRING_end_of_string(line,"#START X = ");
					if (pointer != NULL)
					{
						cm[tilemap_number].connection_list_pointer[remote_number].x1 = atoi(pointer);
					}

					pointer = STRING_end_of_string(line,"#START Y = ");
					if (pointer != NULL)
					{
						cm[tilemap_number].connection_list_pointer[remote_number].y1 = atoi(pointer);
					}

					pointer = STRING_end_of_string(line,"#END X = ");
					if (pointer != NULL)
					{
						cm[tilemap_number].connection_list_pointer[remote_number].x2 = atoi(pointer);
					}

					pointer = STRING_end_of_string(line,"#END Y = ");
					if (pointer != NULL)
					{
						cm[tilemap_number].connection_list_pointer[remote_number].y2 = atoi(pointer);
					}

					pointer = strstr(line,"#END OF AI ZONE REMOTE DATA");
					if (pointer != NULL)
					{
						exitflag = true;
					}
				}
			}
			
			pointer = strstr(line,"#AI NODE DATA = ");
			if (pointer != NULL) // It's the spawn point data.
			{
				cm[tilemap_number].waypoints = atoi(&line[strlen("#AI NODE DATA = ")]);
				// Set aside the memory needed for the list of spawn points.
				cm[tilemap_number].waypoint_list_pointer = (waypoint *) malloc (cm[tilemap_number].waypoints * sizeof(waypoint));

				waypoint_number = 0;
				exitflag = false;

				while (exitflag == false)
				{
					fgets ( line , TEXT_LINE_SIZE , file_pointer );
					STRING_strip_newlines (line);

					pointer = STRING_end_of_string(line,"#WAYPOINT NUMBER = ");
					if (pointer != NULL)
					{
						waypoint_number = atoi(pointer);
					}

					pointer = STRING_end_of_string(line,"#UNIQUE IDENTIFIER NUMBER = ");
					if (pointer != NULL)
					{
						cm[tilemap_number].waypoint_list_pointer[waypoint_number].uid = atoi(pointer);
					}

					pointer = STRING_end_of_string(line,"#X POS = ");
					if (pointer != NULL)
					{
						cm[tilemap_number].waypoint_list_pointer[waypoint_number].x = atoi(pointer);
					}

					pointer = STRING_end_of_string(line,"#Y POS = ");
					if (pointer != NULL)
					{
						cm[tilemap_number].waypoint_list_pointer[waypoint_number].y = atoi(pointer);
					}

					pointer = STRING_end_of_string(line,"#ID TYPE: ");
					if (pointer != NULL)
					{
						strcpy (cm[tilemap_number].waypoint_list_pointer[waypoint_number].type , pointer);
					}

					pointer = STRING_end_of_string(line,"#LOCATION TYPE: ");
					if (pointer != NULL)
					{
						strcpy (cm[tilemap_number].waypoint_list_pointer[waypoint_number].location , pointer);
					}
					
					pointer = STRING_end_of_string(line,"#CONNECTIONS = ");
					if (pointer != NULL)
					{
						cm[tilemap_number].waypoint_list_pointer[waypoint_number].connections = atoi(pointer);

						if (cm[tilemap_number].waypoint_list_pointer[waypoint_number].connections > 0)
						{
							// Set aside the memory needed for the list of parameters.
							cm[tilemap_number].waypoint_list_pointer[waypoint_number].connection_list_uids = (int *) malloc (cm[tilemap_number].waypoint_list_pointer[waypoint_number].connections * sizeof(int));
							cm[tilemap_number].waypoint_list_pointer[waypoint_number].connection_list_indexes = (int *) malloc (cm[tilemap_number].waypoint_list_pointer[waypoint_number].connections * sizeof(int));
							cm[tilemap_number].waypoint_list_pointer[waypoint_number].connection_list_types = (int *) malloc (cm[tilemap_number].waypoint_list_pointer[waypoint_number].connections * sizeof(int));
						}
						else
						{
							cm[tilemap_number].waypoint_list_pointer[waypoint_number].connection_list_uids = NULL;
							cm[tilemap_number].waypoint_list_pointer[waypoint_number].connection_list_indexes = NULL;
							cm[tilemap_number].waypoint_list_pointer[waypoint_number].connection_list_types = NULL;
						}

						for (connection_number=0; connection_number<cm[tilemap_number].waypoint_list_pointer[waypoint_number].connections; connection_number++)
						{
							fgets ( line , TEXT_LINE_SIZE , file_pointer );
							STRING_strip_newlines (line);
							pointer = STRING_end_of_string(line," = ");
							pointer = strtok (pointer,"()");

							cm[tilemap_number].waypoint_list_pointer[waypoint_number].connection_list_uids[connection_number] = atoi (pointer);

							pointer = strtok (NULL,"()");

							cm[tilemap_number].waypoint_list_pointer[waypoint_number].connection_list_types[connection_number] = atoi (pointer);
						}
					}

					pointer = strstr(line,"#END OF AI NODE DATA");
					if (pointer != NULL)
					{
						exitflag = true;
					}
				}
			}

		}

		fclose(file_pointer);
	}
}



void TILEMAPS_blank_verbose_data (int tilemap_number)
{
	int spawn_point_number;
	int parameter_number;
	int zone_number;
	int waypoint_number;

	int t;

	for (t=0; t<NAME_SIZE; t++)
	{
		cm[tilemap_number].name[t] = '\0';
		cm[tilemap_number].old_name[t] = '\0';
		cm[tilemap_number].default_tile_set[t] = '\0';
	}

	for (spawn_point_number=0; spawn_point_number<cm[tilemap_number].spawn_points; spawn_point_number++)
	{
		for (t=0; t<NAME_SIZE; t++)
		{
			cm[tilemap_number].spawn_point_list_pointer[spawn_point_number].name[t] = '\0';
			cm[tilemap_number].spawn_point_list_pointer[spawn_point_number].type[t] = '\0';
			cm[tilemap_number].spawn_point_list_pointer[spawn_point_number].script_name[t] = '\0';
		}

		for (parameter_number=0; parameter_number<cm[tilemap_number].spawn_point_list_pointer[spawn_point_number].params; parameter_number++)
		{
			for (t=0; t<NAME_SIZE; t++)
			{
				cm[tilemap_number].spawn_point_list_pointer[spawn_point_number].param_list_pointer[parameter_number].param_dest_var[t] = '\0';
				cm[tilemap_number].spawn_point_list_pointer[spawn_point_number].param_list_pointer[parameter_number].param_list_type[t] = '\0';
				cm[tilemap_number].spawn_point_list_pointer[spawn_point_number].param_list_pointer[parameter_number].param_name[t] = '\0';
			}
		}
	}

	for (zone_number=0; zone_number<cm[tilemap_number].zones; zone_number++)
	{
		for (t=0; t<NAME_SIZE; t++)
		{
			cm[tilemap_number].zone_list_pointer[zone_number].text_tag[t] = '\0';
			cm[tilemap_number].zone_list_pointer[zone_number].type[t] = '\0';
		}
	}

	for (waypoint_number=0; waypoint_number<cm[tilemap_number].waypoints; waypoint_number++)
	{
		for (t=0; t<NAME_SIZE; t++)
		{
			cm[tilemap_number].waypoint_list_pointer[waypoint_number].type[t] = '\0';
			cm[tilemap_number].waypoint_list_pointer[waypoint_number].location[t] = '\0';
		}
	}
}



void TILEMAPS_load_game_data (void)
{
	// This loads the data in it's more compressed format, malloc'ing happily as it goes...

	// This routine will only ever be called by the game itself when in release mode.

	int data[1];
	int spawn_point_number;
	int tilemap_number;
	int waypoint_number;
	int i;

	FILE *file_pointer = fopen (MAIN_get_project_filename ("tilemaps.dat"),"rb");
	
	if (file_pointer != NULL)
	{
		fread ( data, sizeof(int), 1, file_pointer );
#ifdef ALLEGRO_MACOSX
	data[0] = EndianS32_LtoN(data[0]);
#endif
		number_of_tilemaps_loaded = data[0];
		cm = (tilemap *) malloc (sizeof (tilemap) * number_of_tilemaps_loaded);

		for (tilemap_number = 0; tilemap_number<number_of_tilemaps_loaded; tilemap_number++)
		{
			fread (&cm[tilemap_number] , sizeof(tilemap) , 1 , file_pointer);
#ifdef ALLEGRO_MACOSX
			endian(cm[tilemap_number]);
#endif
			cm[tilemap_number].map_pointer = (short *) malloc ( cm[tilemap_number].map_layers * cm[tilemap_number].map_width * cm[tilemap_number].map_height * sizeof(short) );
			cm[tilemap_number].group_pointer = (char *) malloc ( cm[tilemap_number].map_layers * cm[tilemap_number].map_width * cm[tilemap_number].map_height * sizeof(char) );
			cm[tilemap_number].helper_tag_pointer = (char *) malloc ( cm[tilemap_number].map_layers * cm[tilemap_number].map_width * cm[tilemap_number].map_height * sizeof(char) );
			cm[tilemap_number].helper_x_offset_pointer = (char *) malloc ( cm[tilemap_number].map_layers * cm[tilemap_number].map_width * cm[tilemap_number].map_height * sizeof(char) );
			cm[tilemap_number].helper_y_offset_pointer = (char *) malloc ( cm[tilemap_number].map_layers * cm[tilemap_number].map_width * cm[tilemap_number].map_height * sizeof(char) );

			cm[tilemap_number].layer_vertex_colours_pointer = NULL;
			cm[tilemap_number].vertex_colours_pointer = NULL;
			cm[tilemap_number].detailed_vertex_colours_pointer = NULL;

			switch(cm[tilemap_number].vertex_colour_mode)
			{
			case VERTEX_MODE_NONE:
				// Nuttin!
				break;

			case VERTEX_MODE_SINGLE_MAP_VALUE:
				cm[tilemap_number].layer_vertex_colours_pointer = (simple_vertex_colours *) malloc (sizeof(simple_vertex_colours) * cm[tilemap_number].map_layers);
				break;

			case VERTEX_MODE_SIMPLE_TILE_VALUES:
				cm[tilemap_number].vertex_colours_pointer = (simple_vertex_colours *) malloc ( cm[tilemap_number].map_layers * cm[tilemap_number].map_width * cm[tilemap_number].map_height * sizeof(simple_vertex_colours) );
				break;

			case VERTEX_MODE_COMPLEX_TILE_VALUES:
				cm[tilemap_number].detailed_vertex_colours_pointer = (detailed_vertex_colours *) malloc ( cm[tilemap_number].map_layers * cm[tilemap_number].map_width * cm[tilemap_number].map_height * sizeof(detailed_vertex_colours) );
				break;

			default:
				assert(0);
				break;
			}
				fread (cm[tilemap_number].map_pointer , sizeof(short), cm[tilemap_number].map_layers * cm[tilemap_number].map_width * cm[tilemap_number].map_height, file_pointer);
				#ifdef ALLEGRO_MACOSX
				for (int map = 0; map < cm[tilemap_number].map_layers * cm[tilemap_number].map_width * cm[tilemap_number].map_height; ++map)
				{
					endian(cm[tilemap_number].map_pointer[map]);
				}
				#endif
				fread (cm[tilemap_number].group_pointer , sizeof(char), cm[tilemap_number].map_layers * cm[tilemap_number].map_width * cm[tilemap_number].map_height, file_pointer);
			fread (cm[tilemap_number].helper_tag_pointer , sizeof(char), cm[tilemap_number].map_layers * cm[tilemap_number].map_width * cm[tilemap_number].map_height, file_pointer);
			fread (cm[tilemap_number].helper_x_offset_pointer , sizeof(char), cm[tilemap_number].map_layers * cm[tilemap_number].map_width * cm[tilemap_number].map_height, file_pointer);
			fread (cm[tilemap_number].helper_y_offset_pointer , sizeof(char), cm[tilemap_number].map_layers * cm[tilemap_number].map_width * cm[tilemap_number].map_height, file_pointer);

			switch(cm[tilemap_number].vertex_colour_mode)
			{
			case VERTEX_MODE_NONE:
				// Nuttin!
				break;

			case VERTEX_MODE_SINGLE_MAP_VALUE:
				fread (cm[tilemap_number].layer_vertex_colours_pointer , sizeof(simple_vertex_colours), cm[tilemap_number].map_layers, file_pointer);
				break;

			case VERTEX_MODE_SIMPLE_TILE_VALUES:
				fread (cm[tilemap_number].vertex_colours_pointer , sizeof(simple_vertex_colours), cm[tilemap_number].map_layers * cm[tilemap_number].map_width * cm[tilemap_number].map_height, file_pointer);
				break;

			case VERTEX_MODE_COMPLEX_TILE_VALUES:
				fread (cm[tilemap_number].detailed_vertex_colours_pointer , sizeof(detailed_vertex_colours), cm[tilemap_number].map_layers * cm[tilemap_number].map_width * cm[tilemap_number].map_height, file_pointer);
				break;

			default:
				assert(0);
				break;
			}

			if (cm[tilemap_number].zones > 0)
			{
				cm[tilemap_number].zone_list_pointer = (zone *) malloc ( cm[tilemap_number].zones * sizeof(zone) );
				fread (cm[tilemap_number].zone_list_pointer , sizeof(zone), cm[tilemap_number].zones, file_pointer);
				#ifdef ALLEGRO_MACOSX
				for (int zone = 0; zone < cm[tilemap_number].zones; ++zone)
				{
					endian(cm[tilemap_number].zone_list_pointer[zone]);
				}
				#endif
			}
			else
			{
				cm[tilemap_number].zone_list_pointer = NULL;
			}

			if (cm[tilemap_number].gates > 0)
			{
				cm[tilemap_number].gate_list_pointer = (gate *) malloc ( cm[tilemap_number].gates * sizeof(gate) );
				fread (cm[tilemap_number].gate_list_pointer , sizeof(gate), cm[tilemap_number].gates, file_pointer);
				#ifdef ALLEGRO_MACOSX
				for (int gate = 0; gate < cm[tilemap_number].gates; ++gate)
				{
					endian(cm[tilemap_number].gate_list_pointer[gate]);
				}
				#endif
			}
			else
			{
				cm[tilemap_number].gate_list_pointer = NULL;
			}
			
			if (cm[tilemap_number].remotes > 0)
			{
				cm[tilemap_number].connection_list_pointer = (remote_connection *) malloc ( cm[tilemap_number].remotes * sizeof(remote_connection) );
				fread (cm[tilemap_number].connection_list_pointer , sizeof(remote_connection), cm[tilemap_number].remotes, file_pointer);
				#ifdef ALLEGRO_MACOSX
				for (int remote = 0; remote < cm[tilemap_number].remotes; ++remote)
				{
					endian(cm[tilemap_number].connection_list_pointer[remote]);
				}
				#endif
			}
			else
			{
				cm[tilemap_number].connection_list_pointer = NULL;
			}
			

			if (cm[tilemap_number].waypoints > 0)
			{
				cm[tilemap_number].waypoint_list_pointer = (waypoint *) malloc ( cm[tilemap_number].waypoints * sizeof(waypoint) );
				fread (cm[tilemap_number].waypoint_list_pointer , sizeof(waypoint), cm[tilemap_number].waypoints, file_pointer);
				#ifdef ALLEGRO_MACOSX
				for (int waypoint = 0; waypoint < cm[tilemap_number].waypoints; ++waypoint)
				{
					endian(cm[tilemap_number].waypoint_list_pointer[waypoint]);
				}
				#endif
				for (waypoint_number=0; waypoint_number<cm[tilemap_number].waypoints; waypoint_number++)
				{
					if (cm[tilemap_number].waypoint_list_pointer[waypoint_number].connections > 0)
					{
						cm[tilemap_number].waypoint_list_pointer[waypoint_number].connection_list_indexes = (int *) malloc (cm[tilemap_number].waypoint_list_pointer[waypoint_number].connections * sizeof(int));
						cm[tilemap_number].waypoint_list_pointer[waypoint_number].connection_list_uids = (int *) malloc (cm[tilemap_number].waypoint_list_pointer[waypoint_number].connections * sizeof(int));
						cm[tilemap_number].waypoint_list_pointer[waypoint_number].connection_list_types = (int *) malloc (cm[tilemap_number].waypoint_list_pointer[waypoint_number].connections * sizeof(int));
						fread (cm[tilemap_number].waypoint_list_pointer[waypoint_number].connection_list_indexes , sizeof(int), cm[tilemap_number].waypoint_list_pointer[waypoint_number].connections, file_pointer);
						fread (cm[tilemap_number].waypoint_list_pointer[waypoint_number].connection_list_uids , sizeof(int), cm[tilemap_number].waypoint_list_pointer[waypoint_number].connections, file_pointer);
						//fread (cm[tilemap_number].waypoint_list_pointer[waypoint_number].connection_list_types , sizeof(int) * cm[tilemap_number].waypoint_list_pointer[waypoint_number].connections , 1 , file_pointer);
				#ifdef ALLEGRO_MACOSX
						for (int connection = 0; connection < cm[tilemap_number].waypoint_list_pointer[waypoint_number].connections; ++connection)
						{
							endian(cm[tilemap_number].waypoint_list_pointer[waypoint_number].connection_list_indexes[connection]);
							endian(cm[tilemap_number].waypoint_list_pointer[waypoint_number].connection_list_uids[connection]);
						}
				#endif
					}
					else
					{
						cm[tilemap_number].waypoint_list_pointer[waypoint_number].connection_list_indexes = NULL;
						cm[tilemap_number].waypoint_list_pointer[waypoint_number].connection_list_uids = NULL;
						cm[tilemap_number].waypoint_list_pointer[waypoint_number].connection_list_types = NULL;
					}
				}
			}
			else
			{
				cm[tilemap_number].waypoint_list_pointer = NULL;
			}
			

			if (cm[tilemap_number].spawn_points > 0)
			{
				cm[tilemap_number].spawn_point_list_pointer = (spawn_point *) malloc ( cm[tilemap_number].spawn_points * sizeof(spawn_point) );
				fread (cm[tilemap_number].spawn_point_list_pointer , sizeof(spawn_point), cm[tilemap_number].spawn_points , file_pointer);
				#ifdef ALLEGRO_MACOSX
				for (int spawn = 0; spawn < cm[tilemap_number].spawn_points; ++spawn) 
					endian(cm[tilemap_number].spawn_point_list_pointer[spawn]);
				#endif
				for (spawn_point_number=0; spawn_point_number<cm[tilemap_number].spawn_points; spawn_point_number++)
				{
					if (cm[tilemap_number].spawn_point_list_pointer[spawn_point_number].params > 0)
					{
						cm[tilemap_number].spawn_point_list_pointer[spawn_point_number].param_list_pointer = (parameter *) malloc ( cm[tilemap_number].spawn_point_list_pointer[spawn_point_number].params * sizeof(parameter) );
						fread (cm[tilemap_number].spawn_point_list_pointer[spawn_point_number].param_list_pointer , sizeof(parameter), cm[tilemap_number].spawn_point_list_pointer[spawn_point_number].params, file_pointer);
						#ifdef ALLEGRO_MACOSX
						for (int param = 0; param < cm[tilemap_number].spawn_point_list_pointer[spawn_point_number].params; ++param)
							endian(cm[tilemap_number].spawn_point_list_pointer[spawn_point_number].param_list_pointer[param]);
						#endif
					}
					else
					{
						cm[tilemap_number].spawn_point_list_pointer[spawn_point_number].param_list_pointer = NULL;
					}
				}
			}
			else
			{
				cm[tilemap_number].spawn_point_list_pointer = NULL;
			}

		}

		fclose (file_pointer);
	}
	else
	{
		OUTPUT_message("Could not open tilemaps.dat!");
		assert(0); // couldn't create file!
	}

}



void TILEMAPS_append_data (int tilemap_number)
{
	int spawn_point_number;
	int waypoint_number;

	FILE *file_pointer = fopen (MAIN_get_project_filename ("tilemaps.dat"),"ab");

	if (file_pointer != NULL)
	{
		fwrite (&cm[tilemap_number] , sizeof(tilemap) , 1 , file_pointer);

		fwrite (cm[tilemap_number].map_pointer , sizeof(short) * cm[tilemap_number].map_layers * cm[tilemap_number].map_width * cm[tilemap_number].map_height , 1 , file_pointer);
		fwrite (cm[tilemap_number].group_pointer , sizeof(char) * cm[tilemap_number].map_layers * cm[tilemap_number].map_width * cm[tilemap_number].map_height , 1 , file_pointer);
		fwrite (cm[tilemap_number].helper_tag_pointer , sizeof(char) * cm[tilemap_number].map_layers * cm[tilemap_number].map_width * cm[tilemap_number].map_height , 1 , file_pointer);
		fwrite (cm[tilemap_number].helper_x_offset_pointer , sizeof(char) * cm[tilemap_number].map_layers * cm[tilemap_number].map_width * cm[tilemap_number].map_height , 1 , file_pointer);
		fwrite (cm[tilemap_number].helper_y_offset_pointer , sizeof(char) * cm[tilemap_number].map_layers * cm[tilemap_number].map_width * cm[tilemap_number].map_height , 1 , file_pointer);

		switch(cm[tilemap_number].vertex_colour_mode)
		{
		case VERTEX_MODE_NONE:
			// Nuttin!
			break;

		case VERTEX_MODE_SINGLE_MAP_VALUE:
			fwrite (cm[tilemap_number].layer_vertex_colours_pointer , sizeof(simple_vertex_colours) * cm[tilemap_number].map_layers , 1 , file_pointer);
			break;

		case VERTEX_MODE_SIMPLE_TILE_VALUES:
			fwrite (cm[tilemap_number].vertex_colours_pointer , sizeof(simple_vertex_colours) * cm[tilemap_number].map_layers * cm[tilemap_number].map_width * cm[tilemap_number].map_height , 1 , file_pointer);
			break;

		case VERTEX_MODE_COMPLEX_TILE_VALUES:
			fwrite (cm[tilemap_number].detailed_vertex_colours_pointer , sizeof(detailed_vertex_colours) * cm[tilemap_number].map_layers * cm[tilemap_number].map_width * cm[tilemap_number].map_height , 1 , file_pointer);
			break;

		default:
			assert(0);
			break;
		}

		if (cm[tilemap_number].zones > 0)
		{
			fwrite (cm[tilemap_number].zone_list_pointer , sizeof(zone) * cm[tilemap_number].zones , 1 , file_pointer);
		}

		if (cm[tilemap_number].gates > 0)
		{
			fwrite (cm[tilemap_number].gate_list_pointer , sizeof(gate) * cm[tilemap_number].gates , 1 , file_pointer);
		}

		if (cm[tilemap_number].remotes > 0)
		{
			fwrite (cm[tilemap_number].connection_list_pointer , sizeof(remote_connection) * cm[tilemap_number].remotes , 1 , file_pointer);
		}

		if (cm[tilemap_number].waypoints > 0)
		{
			fwrite (cm[tilemap_number].waypoint_list_pointer , sizeof(waypoint) * cm[tilemap_number].waypoints , 1 , file_pointer);

			for (waypoint_number=0; waypoint_number<cm[tilemap_number].waypoints; waypoint_number++)
			{
				if (cm[tilemap_number].waypoint_list_pointer[waypoint_number].connections > 0)
				{
					fwrite (cm[tilemap_number].waypoint_list_pointer[waypoint_number].connection_list_indexes , sizeof(int) * cm[tilemap_number].waypoint_list_pointer[waypoint_number].connections , 1 , file_pointer);
					fwrite (cm[tilemap_number].waypoint_list_pointer[waypoint_number].connection_list_uids , sizeof(int) * cm[tilemap_number].waypoint_list_pointer[waypoint_number].connections , 1 , file_pointer);
					fwrite (cm[tilemap_number].waypoint_list_pointer[waypoint_number].connection_list_types , sizeof(int) * cm[tilemap_number].waypoint_list_pointer[waypoint_number].connections , 1 , file_pointer);
				}
			}
		}

		if (cm[tilemap_number].spawn_points > 0)
		{
			fwrite (cm[tilemap_number].spawn_point_list_pointer , sizeof(spawn_point) * cm[tilemap_number].spawn_points , 1 , file_pointer);

			for (spawn_point_number=0; spawn_point_number<cm[tilemap_number].spawn_points; spawn_point_number++)
			{
				if (cm[tilemap_number].spawn_point_list_pointer[spawn_point_number].params > 0)
				{
					fwrite (cm[tilemap_number].spawn_point_list_pointer[spawn_point_number].param_list_pointer , sizeof(parameter) * cm[tilemap_number].spawn_point_list_pointer[spawn_point_number].params , 1 , file_pointer);
				}
			}
		}

		fclose(file_pointer);
	}
	else
	{
		OUTPUT_message("Could not append to tilemaps.dat!");
		assert(0); // couldn't create file!
	}
}



void TILEMAPS_save_all (void)
{
	// This saves out all the tilemaps, however first it deletes the original files
	// so we don't create duplicates.

	int tilemap_number;
	char filename[NAME_SIZE];
	char full_filename [TEXT_LINE_SIZE];

	for (tilemap_number = 0; tilemap_number < number_of_tilemaps_loaded; tilemap_number++)
	{
		if (cm[tilemap_number].altered_this_load == true)
		{
			if (strcmp (cm[tilemap_number].old_name,"UNSET") != 0)
			{
				sprintf(filename, "%s.txt", cm[tilemap_number].old_name);
				append_filename (full_filename, "tilemaps", filename, sizeof(full_filename) );

				remove (MAIN_get_project_filename (full_filename));
			}
		}
	}

	for (tilemap_number = 0; tilemap_number < number_of_tilemaps_loaded; tilemap_number++)
	{
		if (cm[tilemap_number].altered_this_load == true)
		{
			TILEMAPS_save (tilemap_number);
		}
	}

	// Now we blank ALL the words in the tilemap data. This is so when we save out all the
	// data in a big chunk all the words will be made of zeroed bytes so that they'll zip
	// up a treat. Except we don't do this - idiot face!

	for (tilemap_number = 0; tilemap_number < number_of_tilemaps_loaded; tilemap_number++)
	{
//		TILEMAPS_blank_verbose_data (tilemap_number);
	}

	// And then we finally splurt out all the data as a big lump.

	FILE *file_pointer = fopen (MAIN_get_project_filename ("tilemaps.dat"),"wb");
	int data[1];

	if (file_pointer != NULL)
	{
		data[0] = number_of_tilemaps_loaded;
		fwrite (data , sizeof(int) , 1 , file_pointer);
		fclose (file_pointer);
	}
	else
	{
		OUTPUT_message("Could not save tilemaps.dat!");
		assert(0); // couldn't create file!
	}

	for (tilemap_number = 0; tilemap_number < number_of_tilemaps_loaded; tilemap_number++)
	{
		TILEMAPS_append_data (tilemap_number);
	}
}



void TILEMAPS_save (int tilemap_number)
{
	// Goes through all the map data saving all the lovely guff.

	char filename[NAME_SIZE];
	sprintf(filename,"%s%s",cm[tilemap_number].name, ".txt");

	char full_filename [TEXT_LINE_SIZE];

	bool exitflag = false;

	int i,p,t,c;
	int colour;
	int layer;

	char temp_char [TEXT_LINE_SIZE];

	append_filename(full_filename,"tilemaps", filename, sizeof (full_filename) );

	int map_data_size = cm[tilemap_number].map_layers * cm[tilemap_number].map_height * cm[tilemap_number].map_width;

	FILE *file_pointer = fopen (MAIN_get_project_filename (full_filename),"w");

	if (file_pointer != NULL)
	{
		// Output default tile set name.
		sprintf(temp_char,"#DEFAULT TILE SET = %s\n\n",cm[tilemap_number].default_tile_set);
		fputs(temp_char,file_pointer);

		// Output map size.
		sprintf(temp_char,"#MAP WIDTH = %d\n",cm[tilemap_number].map_width);
		fputs(temp_char,file_pointer);

		sprintf(temp_char,"#MAP HEIGHT = %d\n",cm[tilemap_number].map_height);
		fputs(temp_char,file_pointer);

		sprintf(temp_char,"#MAP LAYERS = %d\n",cm[tilemap_number].map_layers);
		fputs(temp_char,file_pointer);

		sprintf(temp_char,"#MAP UID = %d\n",cm[tilemap_number].uid);
		fputs(temp_char,file_pointer);

		sprintf(temp_char,"#MAP SPAWN POINT NEXT UID = %d\n",cm[tilemap_number].spawn_point_next_uid);
		fputs(temp_char,file_pointer);

		sprintf(temp_char,"#MAP WAYPOINT NEXT UID = %d\n",cm[tilemap_number].waypoint_next_uid);
		fputs(temp_char,file_pointer);

		sprintf(temp_char,"#MAP ZONE NEXT UID = %d\n",cm[tilemap_number].zone_next_uid);
		fputs(temp_char,file_pointer);

		// Output if we're using RLE MAP format yet, which we ain't.
		sprintf(temp_char,"#RLE MAP = FALSE\n\n");
		fputs(temp_char,file_pointer);

		switch(cm[tilemap_number].vertex_colour_mode)
		{
		case VERTEX_MODE_NONE:
			sprintf(temp_char,"#VERTEX_MODE_NONE\n\n");
			break;

		case VERTEX_MODE_SINGLE_MAP_VALUE:
			sprintf(temp_char,"#VERTEX_MODE_SINGLE_MAP_VALUE\n\n");
			break;

		case VERTEX_MODE_SIMPLE_TILE_VALUES:
			sprintf(temp_char,"#VERTEX_MODE_SIMPLE_TILE_VALUES\n\n");
			break;

		case VERTEX_MODE_COMPLEX_TILE_VALUES:
			sprintf(temp_char,"#VERTEX_MODE_COMPLEX_TILE_VALUES\n\n");
			break;

		default:
			assert(0);
			break;
		}
		fputs(temp_char,file_pointer);

		// Output map tile data.
		sprintf(temp_char,"#MAP TILE DATA\n");
		fputs(temp_char,file_pointer);
		for (t = 0 ; t < map_data_size ; t++)
		{
			EDIT_put_int_to_file (file_pointer , cm[tilemap_number].map_pointer[t] , false);
		}
		FILE_reset_to_file (file_pointer);
		fputs("\n",file_pointer);

		// Output map group data.
		sprintf(temp_char,"#MAP GROUP DATA\n");
		fputs(temp_char,file_pointer);
		for (t = 0 ; t < map_data_size ; t++)
		{
			EDIT_put_int_to_file (file_pointer , cm[tilemap_number].group_pointer[t] , false);
		}
		FILE_reset_to_file (file_pointer);
		fputs("\n",file_pointer);

		// Output map helper tag data.
		sprintf(temp_char,"#MAP HELPER TAG DATA\n");
		fputs(temp_char,file_pointer);
		for (t = 0 ; t < map_data_size ; t++)
		{
			EDIT_put_int_to_file (file_pointer , cm[tilemap_number].helper_tag_pointer[t] , false);
			EDIT_put_int_to_file (file_pointer , cm[tilemap_number].helper_x_offset_pointer[t] , false);
			EDIT_put_int_to_file (file_pointer , cm[tilemap_number].helper_y_offset_pointer[t] , false);
		}
		FILE_reset_to_file (file_pointer);
		fputs("\n",file_pointer);

		if (cm[tilemap_number].vertex_colour_mode == VERTEX_MODE_SINGLE_MAP_VALUE)
		{
			// Output map main vertex colour
			sprintf(temp_char,"#MAP DEFAULT VERTEX COLOUR\n");
			fputs(temp_char,file_pointer);
			for (layer=0; layer<cm[tilemap_number].map_layers; layer++)
			{
				for (colour=0;colour<3;colour++)
				{
					EDIT_put_float_to_file (file_pointer , cm[tilemap_number].layer_vertex_colours_pointer[layer].colours[colour] , false);
				}
				FILE_reset_to_file (file_pointer);
			}
			fputs("\n",file_pointer);
		}

		if (cm[tilemap_number].vertex_colour_mode == VERTEX_MODE_SIMPLE_TILE_VALUES)
		{
			// Output map vertex data.
			sprintf(temp_char,"#MAP VERTEX DATA\n");
			fputs(temp_char,file_pointer);
			for (t = 0 ; t < map_data_size ; t++)
			{
				for (colour=0;colour<3;colour++)
				{
					EDIT_put_float_to_file (file_pointer , cm[tilemap_number].vertex_colours_pointer[t].colours[colour] , false);
				}
			}
			FILE_reset_to_file (file_pointer);
			fputs("\n",file_pointer);
		}

		if (cm[tilemap_number].vertex_colour_mode == VERTEX_MODE_COMPLEX_TILE_VALUES)
		{
			// Output map detailed vertex data.
			sprintf(temp_char,"#MAP DETAILED VERTEX DATA\n");
			fputs(temp_char,file_pointer);
			for (t = 0 ; t < map_data_size ; t++)
			{
				for (colour=0;colour<12;colour++)
				{
					EDIT_put_float_to_file (file_pointer , cm[tilemap_number].detailed_vertex_colours_pointer[t].colours[colour] , false);
				}
			}
			FILE_reset_to_file (file_pointer);
			fputs("\n",file_pointer);
		}

		// Output zone data!
		sprintf(temp_char,"#ZONE DATA = %d\n\n",cm[tilemap_number].zones);
		fputs(temp_char,file_pointer);

		for (i=0 ; i<cm[tilemap_number].zones ; i++)
		{
			sprintf(temp_char,"\t#ZONE NUMBER = %d\n",i); // number - used to mark the start of a new zone
			fputs(temp_char,file_pointer);

			sprintf(temp_char,"\t\t#NAME = %s\n",cm[tilemap_number].zone_list_pointer[i].text_tag);
			fputs(temp_char,file_pointer);

			sprintf(temp_char,"\t\t#X POS = %d\n",cm[tilemap_number].zone_list_pointer[i].x);
			fputs(temp_char,file_pointer);

			sprintf(temp_char,"\t\t#Y POS = %d\n",cm[tilemap_number].zone_list_pointer[i].y);
			fputs(temp_char,file_pointer);

			sprintf(temp_char,"\t\t#WIDTH = %d\n",cm[tilemap_number].zone_list_pointer[i].width);
			fputs(temp_char,file_pointer);

			sprintf(temp_char,"\t\t#HEIGHT = %d\n",cm[tilemap_number].zone_list_pointer[i].height);
			fputs(temp_char,file_pointer);

			sprintf(temp_char,"\t\t#ID TYPE = %s\n",cm[tilemap_number].zone_list_pointer[i].type);
			fputs(temp_char,file_pointer);

			sprintf(temp_char,"\t\t#UID = %i\n\n",cm[tilemap_number].zone_list_pointer[i].uid);
			fputs(temp_char,file_pointer);
		}

		fputs("#END OF ZONE DATA\n\n",file_pointer);

		// Output spawn point data!

		sprintf(temp_char,"#SPAWN POINT DATA = %d\n",cm[tilemap_number].spawn_points);
		fputs(temp_char,file_pointer);

		for (i=0 ; i<cm[tilemap_number].spawn_points ; i++)
		{
			sprintf(temp_char,"\t#SPAWN POINT NUMBER = %d\n",i); // number - used to mark the start of a new spawn point
			fputs(temp_char,file_pointer);

			sprintf(temp_char,"\t#UNIQUE IDENTIFIER NUMBER = %d\n",cm[tilemap_number].spawn_point_list_pointer[i].uid); // number - used to mark the start of a new spawn point
			fputs(temp_char,file_pointer);

			sprintf(temp_char,"\t#PARENT UID = %d\n",cm[tilemap_number].spawn_point_list_pointer[i].parent_uid); // number - used to mark the start of a new spawn point
			fputs(temp_char,file_pointer);

			sprintf(temp_char,"\t#NEXT SIBLING UID = %d\n",cm[tilemap_number].spawn_point_list_pointer[i].next_sibling_uid); // number - used to mark the start of a new spawn point
			fputs(temp_char,file_pointer);

			sprintf(temp_char,"\t\t#NAME = %s\n",cm[tilemap_number].spawn_point_list_pointer[i].name); // name
			fputs(temp_char,file_pointer);

			sprintf(temp_char,"\t\t#SCRIPT = %s\n",cm[tilemap_number].spawn_point_list_pointer[i].script_name); // script
			fputs(temp_char,file_pointer);

			sprintf(temp_char,"\t\t#X POS = %d\n" , cm[tilemap_number].spawn_point_list_pointer[i].x ); // size and number of params
			fputs(temp_char,file_pointer);

			sprintf(temp_char,"\t\t#Y POS = %d\n" , cm[tilemap_number].spawn_point_list_pointer[i].y ); // size and number of params
			fputs(temp_char,file_pointer);

			sprintf(temp_char,"\t\t#ID TYPE = %s\n",cm[tilemap_number].spawn_point_list_pointer[i].type);
			fputs(temp_char,file_pointer);

			sprintf(temp_char,"\t\t#PARAMETERS = %d\n" , cm[tilemap_number].spawn_point_list_pointer[i].params ); // size and number of params
			fputs(temp_char,file_pointer);

			if (cm[tilemap_number].spawn_point_list_pointer[i].params > 0)
			{
				for (p=0 ; p<cm[tilemap_number].spawn_point_list_pointer[i].params ; p++)
				{
					sprintf(temp_char,"\t\t\t#PARAMETER %s=%s:%s\n" , cm[tilemap_number].spawn_point_list_pointer[i].param_list_pointer[p].param_dest_var , cm[tilemap_number].spawn_point_list_pointer[i].param_list_pointer[p].param_list_type , cm[tilemap_number].spawn_point_list_pointer[i].param_list_pointer[p].param_name );
					fputs(temp_char,file_pointer);
				}
			}

			fputs("\n",file_pointer);
		}

		fputs("#END OF SPAWN POINT DATA\n\n",file_pointer);

		// Output AI Zone Gate data...

		sprintf(temp_char,"#AI ZONE GATE DATA = %d\n\n",cm[tilemap_number].gates);
		fputs(temp_char,file_pointer);

		for (i=0 ; i<cm[tilemap_number].gates ; i++)
		{
			sprintf(temp_char,"\t#GATE NUMBER = %d\n",i);
			fputs(temp_char,file_pointer);

			sprintf(temp_char,"\t#START X = %d\n",cm[tilemap_number].gate_list_pointer[i].x1);
			fputs(temp_char,file_pointer);

			sprintf(temp_char,"\t#START Y = %d\n",cm[tilemap_number].gate_list_pointer[i].y1);
			fputs(temp_char,file_pointer);

			sprintf(temp_char,"\t#END X = %d\n",cm[tilemap_number].gate_list_pointer[i].x2);
			fputs(temp_char,file_pointer);

			sprintf(temp_char,"\t#END Y = %d\n",cm[tilemap_number].gate_list_pointer[i].y2);
			fputs(temp_char,file_pointer);

			fputs("\n",file_pointer);
		}
		
		fputs("#END OF AI ZONE GATE DATA\n\n",file_pointer);

		// Output AI Zone Remote data...

		sprintf(temp_char,"#AI ZONE REMOTE DATA = %d\n\n",cm[tilemap_number].remotes);
		fputs(temp_char,file_pointer);

		for (i=0 ; i<cm[tilemap_number].remotes ; i++)
		{
			sprintf(temp_char,"\t#REMOTE CONNECTION NUMBER = %d\n",i);
			fputs(temp_char,file_pointer);

			sprintf(temp_char,"\t#CONNECTIVE DIRECTION = %d\n",cm[tilemap_number].connection_list_pointer[i].connective_direction);
			fputs(temp_char,file_pointer);

			sprintf(temp_char,"\t#START X = %d\n",cm[tilemap_number].connection_list_pointer[i].x1);
			fputs(temp_char,file_pointer);

			sprintf(temp_char,"\t#START Y = %d\n",cm[tilemap_number].connection_list_pointer[i].y1);
			fputs(temp_char,file_pointer);

			sprintf(temp_char,"\t#END X = %d\n",cm[tilemap_number].connection_list_pointer[i].x2);
			fputs(temp_char,file_pointer);

			sprintf(temp_char,"\t#END Y = %d\n",cm[tilemap_number].connection_list_pointer[i].y2);
			fputs(temp_char,file_pointer);

			fputs("\n",file_pointer);
		}

		fputs("#END OF AI ZONE REMOTE DATA\n\n",file_pointer);
		
		// Output AI Node data...

		sprintf(temp_char,"#AI NODE DATA = %d\n\n",cm[tilemap_number].waypoints);
		fputs(temp_char,file_pointer);

		for (i=0 ; i<cm[tilemap_number].waypoints ; i++)
		{
			sprintf(temp_char,"\t#WAYPOINT NUMBER = %d\n",i); // number - used to mark the start of a new spawn point
			fputs(temp_char,file_pointer);

			sprintf(temp_char,"\t\t#UNIQUE IDENTIFIER NUMBER = %d\n",cm[tilemap_number].waypoint_list_pointer[i].uid); // number - used to mark the start of a new spawn point
			fputs(temp_char,file_pointer);

			sprintf(temp_char,"\t\t#X POS = %d\n" , cm[tilemap_number].waypoint_list_pointer[i].x ); // size and number of params
			fputs(temp_char,file_pointer);

			sprintf(temp_char,"\t\t#Y POS = %d\n" , cm[tilemap_number].waypoint_list_pointer[i].y ); // size and number of params
			fputs(temp_char,file_pointer);

			sprintf(temp_char,"\t\t#ID TYPE: %s\n",cm[tilemap_number].waypoint_list_pointer[i].type);
			fputs(temp_char,file_pointer);

			sprintf(temp_char,"\t\t#LOCATION TYPE: %s\n",cm[tilemap_number].waypoint_list_pointer[i].location);
			fputs(temp_char,file_pointer);

			sprintf(temp_char,"\t\t#CONNECTIONS = %d\n" , cm[tilemap_number].waypoint_list_pointer[i].connections ); // size and number of params
			fputs(temp_char,file_pointer);

			for (c=0; c<cm[tilemap_number].waypoint_list_pointer[i].connections; c++)
			{
				sprintf(temp_char,"\t\t\t#CONNECTION (%i) = %i(%i)\n" , c , cm[tilemap_number].waypoint_list_pointer[i].connection_list_uids[c] , cm[tilemap_number].waypoint_list_pointer[i].connection_list_types[c] ); // size and number of params
				fputs(temp_char,file_pointer);
			}

			fputs("\n",file_pointer);
		}

		fputs("#END OF AI NODE DATA\n\n",file_pointer);

		// End of file!

		fputs("#END OF FILE\n",file_pointer);

		fclose(file_pointer);
	}
	else
	{
		OUTPUT_message("Could not save map!");
		assert(0); // couldn't create file!
	}
}



void TILEMAPS_load_all (void)
{
	int start,end,i;
	char filename[NAME_SIZE];
	char extension[NAME_SIZE];
	char description[MAX_LINE_SIZE];

	number_of_tilemaps_loaded = 0;

	GPL_list_extents("TILEMAPS",&start,&end);

	for (i=start; i<end ; i++)
	{
		strcpy(filename,GPL_what_is_word_name(i));

		sprintf(description,"LOADING TILEMAP : %s",filename);
		MAIN_draw_loading_picture (description,20);

		strcpy(extension, GPL_what_is_list_extension("TILEMAPS"));
#ifdef ALLEGRO_LINUX
		// Asset files in this repo use lowercase .txt extensions on Linux.
		for (char *p = extension; *p != '\0'; ++p)
		{
			*p = (char)tolower((unsigned char)*p);
		}
#endif
		strcat(filename, extension);
		TILEMAPS_load ( filename , TILEMAPS_create (false) ); // Load the map.
	}
}



void TILEMAPS_change_name (char *old_entry_name, char *new_entry_name)
{
	int t;

	for (t=0 ; t<number_of_tilemaps_loaded ; t++)
	{
		if ( strcmp (cm[t].name , old_entry_name) == 0)
		{
			strcpy (cm[t].name , new_entry_name);
		}
	}
}



void TILEMAPS_register_script_name_change (char *old_entry_name, char *new_entry_name)
{
	// When the name of a path is changed then it'll need to be changed in the spawn
	// points which reference it.

	int t,i;

	for (t=0 ; t<number_of_tilemaps_loaded ; t++)
	{
		for (i=0; i<cm[t].spawn_points ; i++)
		{
			if ( strcmp (cm[t].spawn_point_list_pointer[i].script_name , old_entry_name) == 0)
			{
				strcpy (cm[t].spawn_point_list_pointer[i].script_name , new_entry_name);
			}
		}
	}
}



void TILEMAPS_register_tileset_name_change (char *old_entry_name, char *new_entry_name)
{
	// When the name of a path is changed then it'll need to be changed in the spawn
	// points which reference it.

	int t;

	for (t=0 ; t<number_of_tilemaps_loaded ; t++)
	{
		if ( strcmp (cm[t].default_tile_set , old_entry_name) == 0)
		{
			strcpy (cm[t].default_tile_set , new_entry_name);
		}
	}

}



void TILEMAPS_register_parameter_name_change (char *list_name, char *old_entry_name, char *new_entry_name)
{
	// If *anything* changes name it could well affect a parameter being passed to a
	// spawn point.

	int t,i,j;

	for (t=0 ; t<number_of_tilemaps_loaded ; t++)
	{
		for (i=0; i<cm[t].spawn_points ; i++)
		{
			for (j=0; j<cm[t].spawn_point_list_pointer[i].params; j++)
			{
				if ( strcmp (cm[t].spawn_point_list_pointer[i].param_list_pointer[j].param_list_type , list_name) == 0)
				{
					// Well, it's a parameter of a matching list type, but is it a matching name?
					if ( strcmp (cm[t].spawn_point_list_pointer[i].param_list_pointer[j].param_name , old_entry_name) == 0)
					{
						// Yes it is! Righty, change the sucker!
						strcpy (cm[t].spawn_point_list_pointer[i].param_list_pointer[j].param_name , new_entry_name);
					}
				}
			}
		}
	}

}



bool TILEMAPS_confirm_links (void)
{
	int t,i,j;
	bool okay;
	bool all_okay;

	int result;

	char line[MAX_LINE_SIZE];

	all_okay = true;

	for (t=0 ; t<number_of_tilemaps_loaded ; t++)
	{
		// Check if tileset is present...

		cm[t].tile_set_index = int (GPL_find_word_value ("TILESETS", cm[t].default_tile_set));

		if (cm[t].tile_set_index == UNSET)
		{
			// Linux compatibility fallback: some legacy map files end up with corrupted
			// default tileset strings. Use a deterministic valid tileset so gameplay can proceed.
			if (number_of_tilesets_loaded > 0)
			{
				int fallback_index = t;
				if (fallback_index < 0 || fallback_index >= number_of_tilesets_loaded)
				{
					fallback_index = 0;
				}

				cm[t].tile_set_index = fallback_index;
				strcpy(cm[t].default_tile_set, TILESETS_get_name(fallback_index));

				sprintf ( line , "'%s' FALLBACK TILESET APPLIED: '%s'.\n" , cm[t].name , cm[t].default_tile_set );
				EDIT_add_line_to_report (line);
			}
			else
			{
			if (strcmp (cm[t].default_tile_set,"UNSET") == 0)
			{
				// Used to be Unset and still is - no worries...
				sprintf ( line , "'%s' STILL NEEDS TILESET ASSIGNED.\n" , cm[t].name );
				EDIT_add_line_to_report (line);
			}
			else
			{
				// Used to be something but is now UNSET! SHIIIIIIT!
				sprintf ( line , "'%s' CANNOT FIND TILESET '%s'.\n" , cm[t].name , cm[t].default_tile_set );
				EDIT_add_line_to_report (line);
				all_okay = false;
			}
			}
		}
		else
		{
			sprintf ( line , "'%s' HAS A TILESET ASSIGNED.\n" , cm[t].name );
			EDIT_add_line_to_report (line);
		}

		// Then check all the spawn points for the map...

		for (i=0; i<cm[t].spawn_points ; i++)
		{
			cm[t].spawn_point_list_pointer[i].script_index = int (GPL_find_word_value ("SCRIPTS", cm[t].spawn_point_list_pointer[i].script_name) );

			okay = true;

			if (cm[t].spawn_point_list_pointer[i].script_index == UNSET) // if it can't find a script with a matching name...
			{
				if (strcmp (cm[t].spawn_point_list_pointer[i].script_name,"UNSET") == 0)
				{
					// Used to be Unset and still is - no worries...
				}
				else
				{
					// Used to be something but is now UNSET! SHIIIIIIT!
					cm[t].spawn_point_list_pointer[i].script_index = BROKEN_LINK;
					sprintf ( line , "   TILEMAP '%s' SPAWN POINT '%s' CANNOT FIND SCRIPT '%s'.\n" , cm[t].name , cm[t].spawn_point_list_pointer[i].name , cm[t].spawn_point_list_pointer[i].script_name );
					EDIT_add_line_to_report (line);
					okay = false;
				}
			}

			for (j=0; j<cm[t].spawn_point_list_pointer[i].params; j++)
			{
				
				result = GPL_does_parameter_exist ( cm[t].spawn_point_list_pointer[i].param_list_pointer[j].param_list_type , cm[t].spawn_point_list_pointer[i].param_list_pointer[j].param_name );
				
				if (result == OKAY)
				{
					// Everything is okay, daddio!
				}
				else if (result == BROKEN_LINK)
				{
					// The word in the list was not recognised.
					sprintf ( line , "   TILEMAP '%s' SPAWN POINT '%s' PARAMETER %i = '%s' CANNOT FIND '%s' IN WORD LIST '%s'.\n" , cm[t].name , cm[t].spawn_point_list_pointer[i].name , j , cm[t].spawn_point_list_pointer[i].param_list_pointer[j].param_dest_var , cm[t].spawn_point_list_pointer[i].param_list_pointer[j].param_name , cm[t].spawn_point_list_pointer[i].param_list_pointer[j].param_list_type );
					EDIT_add_line_to_report (line);
					okay = false;
					cm[t].spawn_point_list_pointer[i].param_list_pointer[j].param_value = 0;
				}
				else if (result == VERY_BROKEN_LINK)
				{
					// Can't even find the word list...
					sprintf ( line , "   TILEMAP '%s' SPAWN POINT '%s' PARAMETER %i = '%s' CANNOT FIND WORD LIST '%s'.\n" , cm[t].name , cm[t].spawn_point_list_pointer[i].name , j , cm[t].spawn_point_list_pointer[i].param_list_pointer[j].param_dest_var , cm[t].spawn_point_list_pointer[i].param_list_pointer[j].param_list_type );
					EDIT_add_line_to_report (line);
					okay = false;
					cm[t].spawn_point_list_pointer[i].param_list_pointer[j].param_value = 0;
				}

			}

		}

		if (okay)
		{
			sprintf ( line , "ALL SPAWN POINTS IN TILEMAP '%s' ARE OKAY.\n" , cm[t].name );
			EDIT_add_line_to_report (line);
		}

		AINODES_relink_all_waypoint_connections (t);

	}

	if (all_okay)
	{
		if (number_of_tilemaps_loaded)
		{
			EDIT_add_line_to_report ("ALL TILEMAPS PEACHY!\n");
		}
		else
		{
			EDIT_add_line_to_report ("NO TILEMAPS TO PROCESS...\n");
		}		
	}

	return all_okay;
}



void TILEMAPS_dialogue_box (int x, int y, int width, int height, char *word_string, int r, int g, int b)
{
	OUTPUT_filled_rectangle ( x , y , x+width-1, y+height-1 , 0, 0, 128 );
	OUTPUT_filled_rectangle ( x+FONT_WIDTH , y+FONT_HEIGHT , x+width-FONT_WIDTH-1, y+height-FONT_HEIGHT-1 , 0, 0, 0 );

	OUTPUT_rectangle ( x , y , x+width-1, y+height-1 , 0, 0, 255 );
	OUTPUT_rectangle ( x+FONT_WIDTH-1 , y+FONT_HEIGHT-1 , x+width-FONT_WIDTH, y+height-FONT_HEIGHT , 0, 0, 255 );

	OUTPUT_truncated_text ( (width/FONT_WIDTH)-3 , x+FONT_WIDTH+(FONT_WIDTH/2) , y+FONT_HEIGHT+(FONT_HEIGHT/2) , word_string , r , g , b );
}



void TILEMAPS_draw_grid (float zoom_level, int new_grid_size=UNSET)
{
	static int grid_size = 16;
	int x,y;

	if (new_grid_size != UNSET)
	{
		grid_size = new_grid_size;
	}

	int zoomed_grid_size = int (zoom_level * float(grid_size));

	// Draw grid lines!

	if ( ( zoomed_grid_size >= 8) && (grid_size > 1) )
	{
		for (x=0; x<(editor_view_width); x += zoomed_grid_size )
		{
			OUTPUT_vline (x , 0, editor_view_height, 0, 0, 64);
		}

		for (y=0; y<(editor_view_height); y += zoomed_grid_size )
		{
			OUTPUT_hline (0 , y, editor_view_width, 0, 0, 64);
		}
	}
	
	if ( ( zoomed_grid_size*4 >= 16) && (grid_size > 1) )
	{
		for (x=0; x<(editor_view_width); x += (zoomed_grid_size*4) )
		{
			OUTPUT_vline (x , 0, editor_view_height, 0, 0, 128);
		}

		for (y=0; y<(editor_view_height); y += (zoomed_grid_size*4) )
		{
			OUTPUT_hline (0 , y, editor_view_width, 0, 0, 128);
		}
	}

}



void TILEMAPS_draw_guides (int tilemap_number, int sx, int sy, int tilesize, float zoom_level, int new_guide_box_width, int new_guide_box_height)
{
	int x,y;

	int real_width;
	int real_height;

	int offset_of_real_width_into_screen;
	int offset_of_real_height_into_screen;

	int zero_offset_x;
	int zero_offset_y;

	if (new_guide_box_width != UNSET)
	{
		guide_box_width = new_guide_box_width;
	}

	if (new_guide_box_height != UNSET)
	{
		guide_box_height = new_guide_box_height;
	}

	// Draw screen guide lines!

	for (x=0 ; x<int(float(editor_view_width)/zoom_level)*2 ; x += guide_box_width*tilesize)
	{
		if ( int(float(x-((sx*tilesize)%(guide_box_width*tilesize)))*zoom_level) < editor_view_width )
		{
			OUTPUT_vline ( int(float(x-((sx*tilesize)%(guide_box_width*tilesize)))*zoom_level) , 0, editor_view_height-1, 64, 64, 64);
		}
	}

	for (y=0 ; y<int(float(editor_view_height)/zoom_level)*2 ; y += guide_box_height*tilesize)
	{
		if ( int(float(y-((sy*tilesize)%(guide_box_height*tilesize)))*zoom_level) < editor_view_height )
		{
			OUTPUT_hline (0 , int(float(y-((sy*tilesize)%(guide_box_height*tilesize)))*zoom_level), editor_view_width-1, 64, 64, 64);
		}
	}

	// Draw static guide line!

//	if (int(float(guide_box_width*tilesize)*zoom_level) >= editor_view_width)
//	{
//		OUTPUT_rectangle ( 0 , 0 , editor_view_width , int(float(guide_box_height*tilesize)*zoom_level) );
//	}
//	else
//	{
//		OUTPUT_rectangle ( 0 , 0 , int(float(guide_box_width*tilesize)*zoom_level) , int(float(guide_box_height*tilesize)*zoom_level) );
//	}

	// Draw map bounding box line if present.

	real_width = cm[tilemap_number].map_width * tilesize;
	real_height = cm[tilemap_number].map_height * tilesize;

	offset_of_real_width_into_screen = int(float (real_width - (sx * tilesize)) * zoom_level);
	offset_of_real_height_into_screen = int(float (real_height - (sy * tilesize)) * zoom_level);

	zero_offset_x = int(float (-sx * tilesize) * zoom_level);
	zero_offset_y = int(float (-sy * tilesize) * zoom_level);

/*
	if (offset_of_real_width_into_screen > editor_view_width)
	{
		offset_of_real_width_into_screen = editor_view_width;
	}

	if (offset_of_real_height_into_screen > editor_view_height)
	{
		offset_of_real_height_into_screen = editor_view_height;
	}
*/

	if ( zero_offset_x >= 0 )
	{
		OUTPUT_vline (zero_offset_x,zero_offset_y,offset_of_real_height_into_screen,255,0,0);
	}

	if ( offset_of_real_width_into_screen < editor_view_width )
	{
		OUTPUT_vline (offset_of_real_width_into_screen,zero_offset_y,offset_of_real_height_into_screen,255,0,0);
	}

	if ( zero_offset_y >= 0 )
	{
		OUTPUT_hline (zero_offset_x,zero_offset_y,offset_of_real_width_into_screen,255,0,0);
	}

	if ( offset_of_real_height_into_screen < editor_view_height )
	{
		OUTPUT_hline (zero_offset_x,offset_of_real_height_into_screen,offset_of_real_width_into_screen,255,0,0);
	}
}



void TILEMAPS_draw_display (int tilemap_number, int sx, int sy, int layer_display_mode, int editing_layer, float zoom_level, int colour_display_mode)
{
	// Draw Tiles! (but only if a tileset has been assigned.

	if (cm[tilemap_number].tile_set_index != UNSET)
	{
		switch(layer_display_mode)
		{
		case VIEW_ALL_LAYERS:
			TILEMAPS_draw (0, cm[tilemap_number].map_layers, tilemap_number, sx, sy, editor_view_width, editor_view_height, zoom_level,colour_display_mode);
			break;

		case VIEW_ONLY_THIS_LAYER:
			TILEMAPS_draw (editing_layer, editing_layer+1, tilemap_number, sx, sy, editor_view_width, editor_view_height, zoom_level,colour_display_mode);
			break;

		case VIEW_UP_TO_THIS_LAYER:
			TILEMAPS_draw (0, editing_layer+1, tilemap_number, sx, sy, editor_view_width, editor_view_height, zoom_level,colour_display_mode);
			break;

		default:
			break;
		}
	}
	
}



#define TILE_PLACEMENT_TOOL_NORMAL		(0)
#define TILE_PLACEMENT_TOOL_BLOCK		(1)
#define TILE_PLACEMENT_TOOL_FILL		(2)

bool TILEMAPS_edit_tilemap (int state , bool overlay_display, int *current_cursor ,int sx, int sy,  int tilemap_number, int mx, int my, float zoom_level, int grid_size, int editing_layer)
{
	// This just deals with the grabbing and placing of tiles is all.

	static short *copy_area;
	// This is where tiles are stored when they are copied.

	int tileset_number;
	int tilesize;
	
	int tx;
	int ty;

	int real_tx;
	int real_ty;

	int block_real_tx;
	int block_real_ty;

	static char tileset_name[NAME_SIZE];

	char word_string[NAME_SIZE];

	static bool grabbing;
	static bool flip_flop;

	static int grab_start_tx;
	static int grab_start_ty;
	static int grabbed_width;
	static int grabbed_height;
	static int grabbed_depth;
	static int tile_number;

	static int current_width;
	static int current_height;
	static int current_layers;

	static int block_placement_start_x;
	static int block_placement_start_y;

	static int tile_edit_mode;
	static int layer_display_mode;
	static int draw_tool_mode;

	static bool menu_override;

	int selection_box_x;
	int selection_box_y;
	int selection_box_width;
	int selection_box_height;

	int max_tile_number;

	if (state == STATE_INITIALISE)
	{
		strcpy (tileset_name,"TILESETS");

		grabbing = false;
		flip_flop = false;

		// Malloc some space and stick the first tile in it.
		copy_area = (short *) malloc (sizeof(short));
		copy_area[0] = 0;
		grabbed_width = 1;
		grabbed_height = 1;
		grabbed_depth = 1;
		
		tile_number = 0;

		editing_layer = 0;
		tile_edit_mode = 0;
		layer_display_mode = 0;
		draw_tool_mode = TILE_PLACEMENT_TOOL_NORMAL;

		grid_size = 16;

		menu_override = false;
	}
	else if (state == STATE_SET_UP_BUTTONS)
	{
		grabbing = false;

		editor_view_width = game_screen_width - (8*ICON_SIZE);
		editor_view_height = game_screen_height;

		SIMPGUI_create_nudge_button (TILEMAP_EDITOR_SUB_GROUP, EDIT_TILEMAP, &current_width, editor_view_width+(ICON_SIZE/2)+(1*ICON_SIZE) , ICON_SIZE*1, first_icon , LEFT_ARROW_ICON, LMB, 0, 4096, -1, false);
		SIMPGUI_create_nudge_button (TILEMAP_EDITOR_SUB_GROUP, EDIT_TILEMAP, &current_width, editor_view_width+(ICON_SIZE/2)+(6*ICON_SIZE) , ICON_SIZE*1, first_icon , RIGHT_ARROW_ICON, LMB, 0, 4096, 1, false);

		SIMPGUI_create_nudge_button (TILEMAP_EDITOR_SUB_GROUP, EDIT_TILEMAP, &current_height, editor_view_width+(ICON_SIZE/2)+(1*ICON_SIZE) , ICON_SIZE*2, first_icon , LEFT_ARROW_ICON, LMB, 0, 4096, -1, false);
		SIMPGUI_create_nudge_button (TILEMAP_EDITOR_SUB_GROUP, EDIT_TILEMAP, &current_height, editor_view_width+(ICON_SIZE/2)+(6*ICON_SIZE) , ICON_SIZE*2, first_icon , RIGHT_ARROW_ICON, LMB, 0, 4096, 1, false);

		SIMPGUI_create_nudge_button (TILEMAP_EDITOR_SUB_GROUP, EDIT_TILEMAP, &current_layers, editor_view_width+(ICON_SIZE/2)+(1*ICON_SIZE) , ICON_SIZE*3, first_icon , LEFT_ARROW_ICON, LMB, 0, 4096, -1, false);
		SIMPGUI_create_nudge_button (TILEMAP_EDITOR_SUB_GROUP, EDIT_TILEMAP, &current_layers, editor_view_width+(ICON_SIZE/2)+(6*ICON_SIZE) , ICON_SIZE*3, first_icon , RIGHT_ARROW_ICON, LMB, 0, 4096, 1, false);

		SIMPGUI_sleep_group (TILEMAP_EDITOR_SUB_GROUP);
	}
	else if (state == STATE_RUNNING)
	{
		current_width = cm[tilemap_number].map_width;
		current_height = cm[tilemap_number].map_height;
		current_layers = cm[tilemap_number].map_layers;
		
		// Display Overlay

		if (overlay_display == true)
		{
			editor_view_width = game_screen_width - (8*ICON_SIZE);
			editor_view_height = game_screen_height;

			// Wake button group.
			SIMPGUI_wake_group (TILEMAP_EDITOR_SUB_GROUP);

			OUTPUT_filled_rectangle (editor_view_width,0,game_screen_width,game_screen_height,0,0,0);

			OUTPUT_vline (editor_view_width,0,game_screen_height,255,255,255);

			OUTPUT_centred_text ( editor_view_width+(ICON_SIZE*4) , (ICON_SIZE-FONT_HEIGHT)/2 , "TILEMAP EDITOR" );

			OUTPUT_draw_masked_sprite ( first_icon , MAP_SIZE_ICON, editor_view_width+(ICON_SIZE/2)+(0*ICON_SIZE) , ICON_SIZE*1 );
			OUTPUT_draw_masked_sprite ( first_icon , LAYER_DRAW_MODE_ICON, editor_view_width+(ICON_SIZE/2)+(0*ICON_SIZE) , ICON_SIZE*7 );
			OUTPUT_draw_masked_sprite ( first_icon , EDIT_MODE_ICON, editor_view_width+(ICON_SIZE/2)+(0*ICON_SIZE) , ICON_SIZE*8 );
			OUTPUT_draw_masked_sprite ( first_icon , DRAW_TOOL_ICON, editor_view_width+(ICON_SIZE/2)+(0*ICON_SIZE) , ICON_SIZE*9 );
			OUTPUT_draw_masked_sprite ( first_icon , SELECT_TILE_SET_ICON, editor_view_width+(ICON_SIZE/2)+(0*ICON_SIZE) , ICON_SIZE*10 );

			OUTPUT_draw_masked_sprite ( first_icon , EVERY_LAYER_ICON, editor_view_width+(ICON_SIZE/2)+(2*ICON_SIZE) , ICON_SIZE*7 );
			OUTPUT_draw_masked_sprite ( first_icon , UP_TO_THIS_LAYER_ICON, editor_view_width+(ICON_SIZE/2)+(4*ICON_SIZE) , ICON_SIZE*7 );
			OUTPUT_draw_masked_sprite ( first_icon , ONLY_THIS_LAYER_ICON, editor_view_width+(ICON_SIZE/2)+(6*ICON_SIZE) , ICON_SIZE*7 );
			OUTPUT_draw_masked_sprite ( first_icon , FORWARD_ARROW, editor_view_width+(ICON_SIZE/2)+(2*ICON_SIZE*layer_display_mode)+ICON_SIZE , ICON_SIZE*7 );

			OUTPUT_draw_masked_sprite ( first_icon , SOLID_ICON, editor_view_width+(ICON_SIZE/2)+(2*ICON_SIZE) , ICON_SIZE*8 );
			OUTPUT_draw_masked_sprite ( first_icon , ONLY_NON_ZERO_ICON, editor_view_width+(ICON_SIZE/2)+(4*ICON_SIZE) , ICON_SIZE*8 );
			OUTPUT_draw_masked_sprite ( first_icon , ONLY_OVER_ZERO_ICON, editor_view_width+(ICON_SIZE/2)+(6*ICON_SIZE) , ICON_SIZE*8 );
			OUTPUT_draw_masked_sprite ( first_icon , FORWARD_ARROW, editor_view_width+(ICON_SIZE/2)+(2*ICON_SIZE*tile_edit_mode)+ICON_SIZE , ICON_SIZE*8 );

			OUTPUT_draw_masked_sprite ( first_icon , TOOL_NORMAL_ICON, editor_view_width+(ICON_SIZE/2)+(2*ICON_SIZE) , ICON_SIZE*9 );
			OUTPUT_draw_masked_sprite ( first_icon , TOOL_BLOC_REPEAT_ICON, editor_view_width+(ICON_SIZE/2)+(4*ICON_SIZE) , ICON_SIZE*9 );
			OUTPUT_draw_masked_sprite ( first_icon , TOOL_FILL_ICON, editor_view_width+(ICON_SIZE/2)+(6*ICON_SIZE) , ICON_SIZE*9 );
			OUTPUT_draw_masked_sprite ( first_icon , FORWARD_ARROW, editor_view_width+(ICON_SIZE/2)+(2*ICON_SIZE*draw_tool_mode)+ICON_SIZE , ICON_SIZE*9 );

			sprintf (word_string, " WIDTH = %d", current_width );
			TILEMAPS_dialogue_box ( editor_view_width+(ICON_SIZE/2)+(2*ICON_SIZE) , ICON_SIZE*1 , ICON_SIZE*4, ICON_SIZE*1 , word_string );

			sprintf (word_string, "HEIGHT = %d", current_height );
			TILEMAPS_dialogue_box ( editor_view_width+(ICON_SIZE/2)+(2*ICON_SIZE) , ICON_SIZE*2 , ICON_SIZE*4, ICON_SIZE*1 , word_string );

			sprintf (word_string, "LAYERS = %d", current_layers );
			TILEMAPS_dialogue_box ( editor_view_width+(ICON_SIZE/2)+(2*ICON_SIZE) , ICON_SIZE*3 , ICON_SIZE*4, ICON_SIZE*1 , word_string );

			sprintf (word_string, "%s", cm[tilemap_number].default_tile_set );
			TILEMAPS_dialogue_box ( editor_view_width+(ICON_SIZE/2)+(1*ICON_SIZE) , ICON_SIZE*10 , ICON_SIZE*6, ICON_SIZE*1 , word_string );

			OUTPUT_draw_masked_sprite ( first_icon , PLONK_TILES_ICON, editor_view_width+(ICON_SIZE/2) , ICON_SIZE*12 );
			OUTPUT_draw_masked_sprite ( first_icon , BIG_YES_ICON, editor_view_width+(ICON_SIZE/2)+ICON_SIZE , ICON_SIZE*12 );

			SIMPGUI_check_all_buttons ();
			SIMPGUI_draw_all_buttons ();

			if (editing_layer >= cm[tilemap_number].map_layers)
			{
				editing_layer = cm[tilemap_number].map_layers-1;
			}
		}
		else
		{
			// Sleep button group
			SIMPGUI_sleep_group (TILEMAP_EDITOR_SUB_GROUP);

			editor_view_width = game_screen_width;
			editor_view_height = game_screen_height;
		}

		if ( (current_width != cm[tilemap_number].map_width) || (current_height != cm[tilemap_number].map_height) || (current_layers != cm[tilemap_number].map_layers) )
		{
			TILEMAPS_resize_map ( tilemap_number , current_width - cm[tilemap_number].map_width , current_height - cm[tilemap_number].map_height , current_layers - cm[tilemap_number].map_layers );
		}

		tileset_number = cm[tilemap_number].tile_set_index;

		if (tileset_number != UNSET)
		{
			tilesize = TILESETS_get_tilesize(tileset_number);
		}
		else
		{
			tilesize = 16;
		}

		tx = mx / int(float(tilesize)*zoom_level);
		ty = my / int(float(tilesize)*zoom_level);

		real_tx = tx + sx;
		real_ty = ty + sy;

		if (flip_flop == true)
		{
			flip_flop = false;
		}
		else
		{
			flip_flop = true;
		}

//		flip_flop = ~flip_flop;

//		flip_flop = !flip_flop;

		if (menu_override == true)
		{
			menu_override = EDIT_gpl_list_menu (80, 48, tileset_name, cm[tilemap_number].default_tile_set, true);
			if (menu_override == false)
			{
				// Yay! We've chosen one!
				cm[tilemap_number].tile_set_index = GPL_find_word_value (tileset_name , cm[tilemap_number].default_tile_set);
				// Lock the LMB so we don't accidentally place tiles until we've released the LMB after this selection.
				CONTROL_lock_mouse_button (LMB);
			}
		}
		else
		{
			// Only draw stored tiles every other frame and if we ain't grabbing anything.
			if ( (flip_flop == true) && (copy_area != NULL) && (tileset_number != UNSET) && (grabbing == false) && (mx<editor_view_width) && (mx<editor_view_width) )
			{
				TILEMAPS_draw_stored_tiles (copy_area, grabbed_width, grabbed_height, tilemap_number, tx, ty, editor_view_width, editor_view_height, zoom_level);
			
				selection_box_x = int(float((real_tx - sx) * tilesize) * zoom_level);
				selection_box_y = int(float((real_ty - sy) * tilesize) * zoom_level);
				selection_box_width = int(float(grabbed_width * tilesize) * zoom_level);
				selection_box_height = int(float(grabbed_height * tilesize) * zoom_level);

				if (selection_box_x + selection_box_width > editor_view_width)
				{
					selection_box_width = editor_view_width - selection_box_x;
				}

				if (selection_box_y + selection_box_height > editor_view_height)
				{
					selection_box_height = editor_view_height - selection_box_y;
				}

				OUTPUT_rectangle (selection_box_x,selection_box_y,selection_box_x+selection_box_width,selection_box_y+selection_box_height,255,255,255);
			}

			// Place blocks!
			if (CONTROL_mouse_hit(LMB) == true)
			{
				if (mx<editor_view_width)
				{
					switch (draw_tool_mode)
					{
					case TILE_PLACEMENT_TOOL_BLOCK:
						// If we're in block placement then the first click creates the base position for all blocks.
						block_placement_start_x = real_tx;
						block_placement_start_y = real_ty;
						break;
					case TILE_PLACEMENT_TOOL_FILL:
						if ( (real_tx<cm[tilemap_number].map_width) && (real_ty<cm[tilemap_number].map_height) )
						{
							// If we're in fill mode then we need to do a fill from this place outwards, so we just call a special routine to do that.
							TILEMAPS_fill_area (tilemap_number, editing_layer, real_tx, real_ty, grabbed_width, grabbed_height, 1, copy_area, tile_edit_mode);
						}
						break;
					default:
						break;
					}
				}
			}

			if (CONTROL_mouse_down(LMB) == true)
			{
				if (mx<editor_view_width)
				{
					switch (draw_tool_mode)
					{
					case TILE_PLACEMENT_TOOL_NORMAL:
						TILEMAPS_paste_map_section (tilemap_number, editing_layer, real_tx, real_ty, grabbed_width, grabbed_height, 1, copy_area, tile_edit_mode);
						break;
					case TILE_PLACEMENT_TOOL_BLOCK:
						block_real_tx = real_tx - ((real_tx + block_placement_start_x) % grabbed_width);
						block_real_ty = real_ty - ((real_ty + block_placement_start_y) % grabbed_height);

						TILEMAPS_paste_map_section (tilemap_number, editing_layer, block_real_tx, block_real_ty, grabbed_width, grabbed_height, 1, copy_area, tile_edit_mode);
						break;
					default:
						break;
					}

				}
			}

			// Grab blocks!
			if (CONTROL_mouse_hit(RMB) == true)
			{
				if (mx<editor_view_width)
				{
					if (grabbing == false)
					{
						grabbing = true;
						grab_start_tx = real_tx;
						grab_start_ty = real_ty;
					}
				}
			}

			if ( (CONTROL_mouse_down(RMB) == true) && (grabbing == true) )
			{
				// Draw the bounding box...

				selection_box_x = int(float((grab_start_tx - sx) * tilesize) * zoom_level);
				selection_box_y = int(float((grab_start_ty - sy) * tilesize) * zoom_level);
				selection_box_width = int(float((real_tx - grab_start_tx) * tilesize) * zoom_level);
				selection_box_height = int(float((real_ty - grab_start_ty) * tilesize) * zoom_level);

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

				OUTPUT_rectangle (selection_box_x, selection_box_y, selection_box_x+selection_box_width+int(float(tilesize)*zoom_level), selection_box_y+selection_box_height+int(float(tilesize)*zoom_level), 255, 255, 255);
			}

			if ( (CONTROL_mouse_down(RMB) == false) && (grabbing == true) )
			{
				grabbing = false;


				if ( (grab_start_tx >= cm[tilemap_number].map_width) || (grab_start_ty >= cm[tilemap_number].map_height) )
				{
					grabbed_width = 0;
					grabbed_height = 0;
					grab_start_tx = 0;
					grab_start_ty = 0;
				}
				else
				{
					grabbed_width = real_tx - grab_start_tx;
					grabbed_height = real_ty - grab_start_ty;
				}

				if (grabbed_width < 0)
				{
					grab_start_tx += grabbed_width;
					grabbed_width *= -1;
				}

				if (grabbed_height < 0)
				{
					grab_start_ty += grabbed_height;
					grabbed_height *= -1;
				}

				grabbed_width += 1;
				grabbed_height += 1;

				copy_area = TILEMAPS_copy_map_section (tilemap_number,editing_layer,grab_start_tx,grab_start_ty,&grabbed_width,&grabbed_height,&grabbed_depth,copy_area);

				tile_number = copy_area[0];
			}

			// Clicks inside the menu.
			if (overlay_display == true)
			{
				if (CONTROL_mouse_hit(LMB))
				{
					// Plonk tiles
					if (MATH_rectangle_to_point_intersect (editor_view_width+(ICON_SIZE/2)+ICON_SIZE , ICON_SIZE*12 , editor_view_width+(ICON_SIZE/2)+(2*ICON_SIZE), ICON_SIZE*13 , mx , my ) == true)
					{
						TILEMAPS_plonk (tilemap_number, editing_layer);
					}

					// Change Tileset
					if (MATH_rectangle_to_point_intersect (editor_view_width+(ICON_SIZE/2) , ICON_SIZE*10 , editor_view_width+(ICON_SIZE/2)+(7*ICON_SIZE), ICON_SIZE*11 , mx , my ) == true)
					{
						menu_override = true;
					}

					// Change layer display mode.
					if (MATH_rectangle_to_point_intersect (editor_view_width+(ICON_SIZE/2)+(ICON_SIZE) , ICON_SIZE*7 , editor_view_width+(ICON_SIZE/2)+(7*ICON_SIZE)-1, ICON_SIZE*8 , mx , my ) == true)
					{
						layer_display_mode = (mx - (editor_view_width+(ICON_SIZE/2)+(ICON_SIZE)) ) / (ICON_SIZE*2);
					}

					// Change pasting mode.
					if (MATH_rectangle_to_point_intersect (editor_view_width+(ICON_SIZE/2)+(ICON_SIZE) , ICON_SIZE*8 , editor_view_width+(ICON_SIZE/2)+(7*ICON_SIZE)-1, ICON_SIZE*9 , mx , my ) == true)
					{
						tile_edit_mode = (mx - (editor_view_width+(ICON_SIZE/2)+(ICON_SIZE)) ) / (ICON_SIZE*2);
					}

					// Change editing mode.
					if (MATH_rectangle_to_point_intersect (editor_view_width+(ICON_SIZE/2)+(ICON_SIZE) , ICON_SIZE*9 , editor_view_width+(ICON_SIZE/2)+(7*ICON_SIZE)-1, ICON_SIZE*10 , mx , my ) == true)
					{
						draw_tool_mode = (mx - (editor_view_width+(ICON_SIZE/2)+(ICON_SIZE)) ) / (ICON_SIZE*2);
					}


				}

			}

			// Scroll through tiles.




/*
			if (CONTROL_mouse_speed(Z_POS))
			{
				tile_number += (CONTROL_mouse_speed(Z_POS) < 0 ? -1 : 1);

				if (cm[tilemap_number].tile_set_index >= 0)
				{
					max_tile_number = TILESETS_get_tile_count(cm[tilemap_number].tile_set_index);
				}
				else
				{
					max_tile_number = 1;
				}

				tile_number = MATH_fold( tile_number , 0 , max_tile_number );

				if (copy_area != NULL)
				{
					free (copy_area);
				}
				copy_area = (short *) malloc (sizeof(short));
				copy_area[0] = tile_number;
				grabbed_width = 1;
				grabbed_height = 1;
			}
*/

			if ( (CONTROL_key_hit(KEY_W)) || (CONTROL_key_hit(KEY_S)) )
			{
				if (CONTROL_key_hit(KEY_W))
				{
					tile_number++;
				}
				else
				{
					tile_number--;
				}

				if (cm[tilemap_number].tile_set_index >= 0)
				{
					max_tile_number = TILESETS_get_tile_count(cm[tilemap_number].tile_set_index);
				}
				else
				{
					max_tile_number = 1;
				}

				tile_number = MATH_fold( tile_number , 0 , max_tile_number );

				if (copy_area != NULL)
				{
					free (copy_area);
				}
				copy_area = (short *) malloc (sizeof(short));
				copy_area[0] = tile_number;
				grabbed_width = 1;
				grabbed_height = 1;
			}

			
		}

	}
	else if (state == STATE_RESET_BUTTONS)
	{
		SIMPGUI_kill_group (TILEMAP_EDITOR_SUB_GROUP);
	}
	else if (state == STATE_SHUTDOWN)
	{
		if (tileset_name != NULL)
		{
			SIMPGUI_kill_group (TILEMAP_EDITOR_SUB_GROUP);
		}

	}

	return menu_override;

}



#define SELECT_TILES			(0)
#define EDIT_VERTEX_COLOURS		(1)
#define COLOUR_BAR_SIZE			(320)

#define STAT_ALTERED_RED		(0)
#define STAT_ALTERED_GREEN		(1)
#define STAT_ALTERED_BLUE		(2)
#define STAT_ALTERED_OPACITY	(3)
#define STAT_ALTERED_RANGE		(4)

bool TILEMAPS_edit_vertex_colours (int state , bool overlay_display, int *current_cursor ,int sx, int sy,  int tilemap_number, int mx, int my, float zoom_level, int grid_size, int editing_layer, int colour_display_mode)
{
	// This just deals with the grabbing and placing of tiles is all.

	static bool *selection_area = NULL;
	// This is where tiles are stored when they are copied.

	int tileset_number;
	int tilesize;

	static int previous_tilemap_number;
	
	int tx;
	int ty;

	int real_tx;
	int real_ty;

	float percent;

	static float v_red;
	static float v_green;
	static float v_blue;
	static float opacity;
	static int size;

	static int last_stat_altered;

	static char tileset_name[NAME_SIZE];

	char word_string[NAME_SIZE];

	static bool grabbing;
	static bool flip_flop;

	static int grab_start_tx;
	static int grab_start_ty;
	static int grabbed_width;
	static int grabbed_height;
	static int grabbed_depth;
	static int tile_number;
	
	static int block_placement_start_x;
	static int block_placement_start_y;

	static int tile_paint_mode;
	static int draw_tool_mode;

	static bool menu_override;

	if (state == STATE_INITIALISE)
	{
		strcpy (tileset_name,"TILESETS");

		grabbing = false;
		flip_flop = false;

		tile_number = 0;
		previous_tilemap_number = tilemap_number;

		v_red = 1.0f;
		v_green = 1.0f;
		v_blue = 1.0f;

		opacity = 1.0f;
		size = 32;

		last_stat_altered = STAT_ALTERED_RED;

		editing_layer = 0;
		tile_paint_mode = 0;
		draw_tool_mode = EDIT_VERTEX_COLOURS;

		grid_size = 16;

		menu_override = false;
	}
	else if (state == STATE_SET_UP_BUTTONS)
	{
		editor_view_width = game_screen_width;
		editor_view_height = game_screen_height - (4*ICON_SIZE);

		SIMPGUI_create_nudge_button (TILEMAP_EDITOR_SUB_GROUP, EDIT_TILEMAP, &guide_box_width, (14*ICON_SIZE) , editor_view_height+(ICON_SIZE/2), first_icon , LEFT_ARROW_ICON, LMB, 0, 160, -1, false);
		SIMPGUI_create_nudge_button (TILEMAP_EDITOR_SUB_GROUP, EDIT_TILEMAP, &guide_box_width, (19*ICON_SIZE) , editor_view_height+(ICON_SIZE/2), first_icon , RIGHT_ARROW_ICON, LMB, 0, 160, 1, false);

		SIMPGUI_create_nudge_button (TILEMAP_EDITOR_SUB_GROUP, EDIT_TILEMAP, &guide_box_height, (14*ICON_SIZE) , editor_view_height+(ICON_SIZE*1)+(ICON_SIZE/2), first_icon , LEFT_ARROW_ICON, LMB, 0, 120, -1, false);
		SIMPGUI_create_nudge_button (TILEMAP_EDITOR_SUB_GROUP, EDIT_TILEMAP, &guide_box_height, (19*ICON_SIZE) , editor_view_height+(ICON_SIZE*1)+(ICON_SIZE/2), first_icon , RIGHT_ARROW_ICON, LMB, 0, 120, 1, false);

		SIMPGUI_create_nudge_button (TILEMAP_EDITOR_SUB_GROUP, EDIT_TILEMAP, &editing_layer, (14*ICON_SIZE) , editor_view_height+(ICON_SIZE*2)+(ICON_SIZE/2), first_icon , LEFT_ARROW_ICON, LMB, 0, 128, -1, false);
		SIMPGUI_create_nudge_button (TILEMAP_EDITOR_SUB_GROUP, EDIT_TILEMAP, &editing_layer, (19*ICON_SIZE) , editor_view_height+(ICON_SIZE*2)+(ICON_SIZE/2), first_icon , RIGHT_ARROW_ICON, LMB, 0, 128, 1, false);

		SIMPGUI_sleep_group (VERTEX_EDITOR_SUB_GROUP);
	}
	else if (state == STATE_RUNNING)
	{
		// Create selection space...

		if (tilemap_number != previous_tilemap_number)
		{
			if (selection_area == NULL)
			{
				selection_area = (bool *) malloc (sizeof(bool) * cm[tilemap_number].map_width * cm[tilemap_number].map_height);
			}
			else
			{
				free (selection_area);
				selection_area = (bool *) malloc (sizeof(bool) * cm[tilemap_number].map_width * cm[tilemap_number].map_height);
			}
		}

		tileset_number = cm[tilemap_number].tile_set_index;

		if (tileset_number != UNSET)
		{
			tilesize = TILESETS_get_tilesize(tileset_number);
		}
		else
		{
			tilesize = 16;
		}

		// Display Overlay

		if (overlay_display == true)
		{
			editor_view_width = game_screen_width;
			editor_view_height = game_screen_height - (4*ICON_SIZE);

			SIMPGUI_wake_group(VERTEX_EDITOR_SUB_GROUP);

			// Wake button group.
			OUTPUT_filled_rectangle (0,editor_view_height,game_screen_width,game_screen_height,0,0,0);

			OUTPUT_hline (0,editor_view_height,game_screen_width,255,255,255);

			OUTPUT_centred_text ( 320 , editor_view_height + (FONT_HEIGHT/2) , "VERTEX COLOUR EDITOR" );

			OUTPUT_draw_masked_sprite ( first_icon , EDIT_MODE_ICON, 0 , editor_view_height+ICON_SIZE/2 );
			OUTPUT_draw_masked_sprite ( first_icon , SELECT_TILES_ICON, ICON_SIZE , editor_view_height+ICON_SIZE/2 );
			OUTPUT_draw_masked_sprite ( first_icon , VERTEX_EDIT_ICON, ICON_SIZE*2 , editor_view_height+ICON_SIZE/2 );

			OUTPUT_draw_masked_sprite ( first_icon , PAINT_TOOL_ICON, ICON_SIZE*9 , editor_view_height+ICON_SIZE/2 );
			OUTPUT_draw_masked_sprite ( first_icon , POINT_TOOL_ICON, ICON_SIZE*10 , editor_view_height+ICON_SIZE/2 );
			OUTPUT_draw_masked_sprite ( first_icon , GRADIENT_TOOL_ICON, ICON_SIZE*11 , editor_view_height+ICON_SIZE/2 );
			OUTPUT_draw_masked_sprite ( first_icon , STAMP_TOOL_ICON, ICON_SIZE*12 , editor_view_height+ICON_SIZE/2 );

			OUTPUT_draw_masked_sprite ( first_icon , BRIGHT_OVERLAY, (ICON_SIZE*1)+(ICON_SIZE*draw_tool_mode) , editor_view_height+ICON_SIZE/2 );
			OUTPUT_draw_masked_sprite ( first_icon , BRIGHT_OVERLAY, (ICON_SIZE*10)+(ICON_SIZE*tile_paint_mode) , editor_view_height+ICON_SIZE/2 );

			sprintf (word_string, "DSP WDTH = %d", guide_box_width );
			TILEMAPS_dialogue_box ( (15*ICON_SIZE) , editor_view_height+(ICON_SIZE*0)+(ICON_SIZE/2) , ICON_SIZE*4, ICON_SIZE*1 , word_string );

			sprintf (word_string, "DSP HGHT = %d", guide_box_height );
			TILEMAPS_dialogue_box ( (15*ICON_SIZE) , editor_view_height+(ICON_SIZE*1)+(ICON_SIZE/2) , ICON_SIZE*4, ICON_SIZE*1 , word_string );

			sprintf (word_string, "LAYER = %d", editing_layer );
			TILEMAPS_dialogue_box ( (15*ICON_SIZE) , editor_view_height+(ICON_SIZE*2)+(ICON_SIZE/2) , ICON_SIZE*4, ICON_SIZE*1 , word_string );

			OUTPUT_text (0,editor_view_height+(ICON_SIZE*1)+(ICON_SIZE/2),"RED",255,0,0,20000);
			OUTPUT_rectangle_by_size (64, editor_view_height+(ICON_SIZE*1)+(ICON_SIZE/2), COLOUR_BAR_SIZE, 14, 255, 0, 0 );
			OUTPUT_filled_rectangle_by_size (64, editor_view_height+(ICON_SIZE*1)+(ICON_SIZE/2), int(COLOUR_BAR_SIZE*v_red), 14, 255, 0, 0 );

			OUTPUT_text (0,editor_view_height+(ICON_SIZE*2),"GR'N",0,255,0,20000);
			OUTPUT_rectangle_by_size (64, editor_view_height+(ICON_SIZE*2)+1, COLOUR_BAR_SIZE, 14, 0, 255, 0 );
			OUTPUT_filled_rectangle_by_size (64, editor_view_height+(ICON_SIZE*2)+1, int(COLOUR_BAR_SIZE*v_green), 14, 0, 255, 0 );

			OUTPUT_text (0,editor_view_height+(ICON_SIZE*2)+(ICON_SIZE/2),"BLUE",0,0,255,20000);
			OUTPUT_rectangle_by_size (64, editor_view_height+(ICON_SIZE*2)+(ICON_SIZE/2)+1, COLOUR_BAR_SIZE, 14, 0, 0, 255 );
			OUTPUT_filled_rectangle_by_size (64, editor_view_height+(ICON_SIZE*2)+(ICON_SIZE/2)+1, int(COLOUR_BAR_SIZE*v_blue), 14, 0, 0, 255 );

			OUTPUT_text (0,editor_view_height+(ICON_SIZE*3),"OPCT",255,0,255,20000);
			OUTPUT_rectangle_by_size (64, editor_view_height+(ICON_SIZE*3)+1, COLOUR_BAR_SIZE, 14, 255, 0, 255 );
			OUTPUT_filled_rectangle_by_size (64, editor_view_height+(ICON_SIZE*3)+1, int(COLOUR_BAR_SIZE*opacity), 14, 255, 0, 255 );

			OUTPUT_text (0,editor_view_height+(ICON_SIZE*3)+(ICON_SIZE/2),"SIZE",0,255,255,20000);
			OUTPUT_rectangle_by_size (64, editor_view_height+(ICON_SIZE*3)+(ICON_SIZE/2)+1, COLOUR_BAR_SIZE, 14, 0, 255, 255 );
			OUTPUT_filled_rectangle_by_size (64, editor_view_height+(ICON_SIZE*3)+(ICON_SIZE/2)+1, int(size*2), 14, 0, 255, 255 );

			OUTPUT_filled_rectangle_by_size (64+COLOUR_BAR_SIZE+1, editor_view_height+(ICON_SIZE*1)+(ICON_SIZE/2)+1, 46, 46, int(v_red * 255), int(v_green * 255), int(v_blue * 255) );
	
			SIMPGUI_check_all_buttons ();
			SIMPGUI_draw_all_buttons ();

			if (editing_layer >= cm[tilemap_number].map_layers)
			{
				editing_layer = cm[tilemap_number].map_layers-1;
			}
		}
		else
		{
			// Sleep button group
			SIMPGUI_sleep_group (VERTEX_EDITOR_SUB_GROUP);

			editor_view_width = game_screen_width;
			editor_view_height = game_screen_height;
		}

		tx = mx / int(float(tilesize)*zoom_level);
		ty = my / int(float(tilesize)*zoom_level);

		real_tx = tx + sx;
		real_ty = ty + sy;

		if (flip_flop == true)
		{
			flip_flop = false;
		}
		else
		{
			flip_flop = true;
		}

		// Select blocks / draw colours!
		if (CONTROL_mouse_hit(LMB) == true)
		{
			if (my<editor_view_height)
			{
				if (draw_tool_mode == SELECT_TILES)
				{

				}
				else if (draw_tool_mode == EDIT_VERTEX_COLOURS)
				{

				}
			}
			else
			{
				if (MATH_rectangle_to_point_intersect (64 , editor_view_height+ICON_SIZE+(ICON_SIZE/2) , 64 + COLOUR_BAR_SIZE , editor_view_height+(4*ICON_SIZE) , mx , my ) == true)
				{
					last_stat_altered = (my - (editor_view_height+ICON_SIZE+(ICON_SIZE/2))) / 16;
				}
			}
		}

		if (CONTROL_mouse_down(LMB) == true)
		{
			if (my<editor_view_height)
			{
				if (draw_tool_mode == SELECT_TILES)
				{

				}
				else if (draw_tool_mode == EDIT_VERTEX_COLOURS)
				{

				}
			}
			else
			{
				if (MATH_rectangle_to_point_intersect (64 , editor_view_height+ICON_SIZE+(ICON_SIZE/2) , 64 + COLOUR_BAR_SIZE , editor_view_height+(4*ICON_SIZE) , mx , my ) == true)
				{
					percent = float (mx-64) / float (COLOUR_BAR_SIZE);

					switch(last_stat_altered)
					{
					case STAT_ALTERED_RED:
						v_red = percent;
						break;

					case STAT_ALTERED_GREEN:
						v_green = percent;
						break;

					case STAT_ALTERED_BLUE:
						v_blue = percent;
						break;

					case STAT_ALTERED_OPACITY:
						opacity = percent;
						break;

					case STAT_ALTERED_RANGE:
						size = int (float (COLOUR_BAR_SIZE/2) * percent);
						break;

					default:
						break;
					}
				}
			}
		}

		// Grab vertex colours...
		if (CONTROL_mouse_hit(RMB) == true)
		{
			if (my<editor_view_height)
			{
				// Grab vertex colour!
			}
		}

		// Clicks inside the menu.
		if (overlay_display == true)
		{
			if (CONTROL_mouse_hit(LMB))
			{
				// Change layer display mode.
				if (MATH_rectangle_to_point_intersect (ICON_SIZE , editor_view_height+(ICON_SIZE/2) , ICON_SIZE*3, editor_view_height+(ICON_SIZE*1)+(ICON_SIZE/2) , mx , my ) == true)
				{
					draw_tool_mode = (mx-ICON_SIZE) / ICON_SIZE;
				}

				// Change editing mode.
				if (MATH_rectangle_to_point_intersect (ICON_SIZE*10 , editor_view_height+(ICON_SIZE/2) , ICON_SIZE*13, editor_view_height+(ICON_SIZE*1)+(ICON_SIZE/2) , mx , my ) == true)
				{
					tile_paint_mode = (mx-ICON_SIZE*10) / ICON_SIZE;
				}
			}

		}

		// Scroll through tiles.

		if (CONTROL_mouse_speed(Z_POS))
		{
			percent = float (CONTROL_mouse_speed(Z_POS)) / 20.0f;

			switch(last_stat_altered)
			{
			case STAT_ALTERED_RED:
				v_red += percent;
				v_red = MATH_constrain_float (v_red,0.0f,1.0f);
				break;

			case STAT_ALTERED_GREEN:
				v_green += percent;
				v_green = MATH_constrain_float (v_green,0.0f,1.0f);
				break;

			case STAT_ALTERED_BLUE:
				v_blue += percent;
				v_blue = MATH_constrain_float (v_blue,0.0f,1.0f);
				break;

			case STAT_ALTERED_OPACITY:
				opacity += percent;
				opacity = MATH_constrain_float (opacity,0.0f,1.0f);
				break;

			case STAT_ALTERED_RANGE:
				size += int (float (COLOUR_BAR_SIZE/2) * percent);
				size = MATH_constrain (size,0,COLOUR_BAR_SIZE/2);
				break;

			default:
				break;
			}
		}

	}
	else if (state == STATE_RESET_BUTTONS)
	{
		SIMPGUI_kill_group (TILEMAP_EDITOR_SUB_GROUP);
	}
	else if (state == STATE_SHUTDOWN)
	{
		if (selection_area != NULL)
		{
			free (selection_area);
			selection_area = NULL;
		}

		if (tileset_name != NULL)
		{
			SIMPGUI_kill_group (TILEMAP_EDITOR_SUB_GROUP);
		}

	}

	return menu_override;

}



void TILEMAPS_find_zones_for_remotes ( int tilemap_number )
{

}



bool TILEMAPS_edit_call_appropriate_editor ( int editing_mode , int state , bool draw_overlay=false, int sx=UNSET, int sy=UNSET, int *current_cursor=NULL , int tilemap_number=UNSET, int mx=UNSET, int my=UNSET, float zoom_level=UNSET, int grid_size=UNSET, int editing_layer=UNSET, int colour_display_mode=UNSET)
{
	bool result = false;

	switch (editing_mode)
	{
	case EDIT_TILEMAP:
		result = TILEMAPS_edit_tilemap (state , draw_overlay, current_cursor ,sx, sy,  tilemap_number, mx, my, zoom_level, grid_size,editing_layer);
		break;

	case EDIT_ROOM_ZONES:
		result = ROOMZONES_edit_room_zones (state , draw_overlay, current_cursor ,sx, sy,  tilemap_number, mx, my, zoom_level, grid_size);
		break;

	case EDIT_SPAWN_POINTS:
		result = SPAWNPOINTS_edit_spawn_points (state , draw_overlay, current_cursor ,sx, sy,  tilemap_number, mx, my, zoom_level, grid_size);
		break;

	case EDIT_PATHS:
		result = PATHS_edit_paths (state , draw_overlay, current_cursor ,sx, sy,  tilemap_number, mx, my, zoom_level, grid_size);
		break;

	case EDIT_AI_ZONES:
		result = AIZONES_edit_ai_zones (state , draw_overlay, current_cursor ,sx, sy,  tilemap_number, mx, my, zoom_level, grid_size);
		break;

	case EDIT_AI_NODES:
		result = AINODES_edit_ai_nodes (state , draw_overlay, current_cursor ,sx, sy,  tilemap_number, mx, my, zoom_level, grid_size);
		break;

	case EDIT_GROUPING:
		result = TILEGROUPS_edit_tile_grouping (state , draw_overlay, current_cursor ,sx, sy,  tilemap_number, mx, my, zoom_level, grid_size, editing_layer);
		break;

	case EDIT_HELPER_TAGGING:
		result = HELPERTAGS_edit_helper_tagging (state , draw_overlay, current_cursor ,sx, sy,  tilemap_number, mx, my, zoom_level, grid_size, editing_layer);
		break;

	case EDIT_COLOURING:
		result = TILEMAPS_edit_vertex_colours (state , draw_overlay, current_cursor ,sx, sy,  tilemap_number, mx, my, zoom_level, grid_size, editing_layer, colour_display_mode);
		break;

	default:
		break;
	}

	return result;
}



#define MAX_ZOOM_LEVELS		(7)

bool TILEMAPS_edit (bool initialise, int starting_map_number)
{
	static int editing_mode;

	char colour_display_mode_names[4][NAME_SIZE] = {"NONE","SINGLE LAYER","TILE BASED","CORNER BASED"};

	static int tilemap_number;

	static int sx,sy;

	static int zoom_level_index;

	float zoom_level_options[MAX_ZOOM_LEVELS] = { 0.125f, 0.25f, 0.5f, 1.0f, 2.0f, 4.0f, 8.0f };

	static float zoom_level;

	static int zoom_grid_change;
	static int grid_size;

	static int remembered_sx;
	static int remembered_sy;

	static int cursor_image;

	static bool exit_editor;

	static int layer_display_mode;
	static int colour_display_mode;

	static int editing_layer;

	static char stat_altered_message[MAX_LINE_SIZE];
	static int stat_altered_timer;
	static int stat_altered_red;
	static int stat_altered_green;
	static int stat_altered_blue;

	static bool scroll_drag;
	static int scroll_drag_start_mx;
	static int scroll_drag_start_my;
	static int scroll_drag_start_sx;
	static int scroll_drag_start_sy;

	bool zoom_or_grid_altered = false;
	bool co_ord_altered = false;
	bool guide_box_size_changed = false;
	static bool draw_grid = false;
	static bool overlay_display = false;

	char co_ordinate_text[MAX_LINE_SIZE];

	bool reset_stuff;

	int temp_counter;

//	int temp;

	int help_box_x = 160;
	int help_box_y = 80;

	int tilesize;

	static int ignore_middle_double_click_count = 0;

	int t;

	int tx,ty;
	int mx,my;

	bool override_keys = false;

	int zoomed_tilesize;

	int temp;
	int old_tile_width,old_tile_height;
	int new_tile_width,new_tile_height;

	bool menu_option_selected = false;

	// If you're running the editor for the first time ever there won't be any maps, so quickly map one, dammit!
	if (number_of_tilemaps_loaded == 0)
	{
		TILEMAPS_create (true);
	}

	if (initialise)
	{
		sx = 0;
		sy = 0;

		for (editing_mode=0; editing_mode<DIFFERENT_TILEMAP_EDITING_MODES; editing_mode++)
		{
			TILEMAPS_edit_call_appropriate_editor (editing_mode , STATE_INITIALISE);
		}

		tilemap_number = starting_map_number;

		initialise = false;

		sprintf(stat_altered_message,"");
		stat_altered_timer = 0;

		stat_altered_red = stat_altered_green = stat_altered_blue = 0;

		layer_display_mode = VIEW_ALL_LAYERS;
		colour_display_mode = COLOUR_DISPLAY_MODE_NONE;

		remembered_sx = 0;
		remembered_sy = 0;

		zoom_level_index = 3;
		zoom_level = zoom_level_options[zoom_level_index];

		zoom_grid_change = 0;
		grid_size = 8;

		editing_mode = UNSET;

		reset_stuff = true;

		exit_editor = false;

		scroll_drag = false;
	}
	else
	{
		mx = CONTROL_mouse_x();
		my = CONTROL_mouse_y();

		if (cm[tilemap_number].tile_set_index >= 0)
		{
			tilesize = TILESETS_get_tilesize(cm[tilemap_number].tile_set_index);
		}
		else
		{
			// ie, there's either no tileset for this map.
			tilesize = 16;
		}

		zoomed_tilesize = int(float(tilesize) * zoom_level);

		tx = (mx/zoomed_tilesize) + sx;
		ty = (my/zoomed_tilesize) + sy;

		cursor_image = SELECT_ARROW;

		// Clear window!

		OUTPUT_clear_screen();

		if (editing_mode != EDIT_AI_ZONES)
		{
			SPAWNPOINTS_draw_spawn_points (tilemap_number, sx, sy, editor_view_width, editor_view_height, zoom_level, UNSET, false, true);

//			TILEMAPS_draw_display (tilemap_number, sx, sy, layer_display_mode, editing_layer, zoom_level, colour_display_mode);

			TILEMAPS_draw_display (tilemap_number, sx, sy, VIEW_ONLY_THIS_LAYER, editing_layer, zoom_level, colour_display_mode);
			
			if (draw_grid)
			{
				TILEMAPS_draw_grid (zoom_level, grid_size);
			}
			TILEMAPS_draw_guides (tilemap_number , sx, sy, tilesize, zoom_level,guide_box_width,guide_box_height);

			sprintf(co_ordinate_text,"%i,%i (%i,%i)",(sx*tilesize)+int(float(mx)/zoom_level),(sy*tilesize)+int(float(my)/zoom_level), tx*tilesize,ty*tilesize);
			OUTPUT_text(0,480-8,co_ordinate_text);
		}

		if (stat_altered_timer > 0)
		{
			stat_altered_timer--;
			OUTPUT_text(8,8,stat_altered_message,0,255,255,10000);
		}

		override_keys = TILEMAPS_edit_call_appropriate_editor (editing_mode , STATE_RUNNING , overlay_display, sx , sy , &cursor_image , tilemap_number, mx, my, zoom_level, grid_size, editing_layer,colour_display_mode);

		OUTPUT_draw_masked_sprite ( first_icon , cursor_image , mx , my );

		// .Draw change of grid size warning

		if ( (CONTROL_key_down(KEY_F1) == true) || (editing_mode == UNSET) )
		{
			OUTPUT_filled_rectangle ( help_box_x , help_box_y , help_box_x+320 , help_box_y+320 , 0 , 0 , 0 );
			OUTPUT_rectangle ( help_box_x , help_box_y , help_box_x+320 , help_box_y+320 );

			temp_counter = 8;

			OUTPUT_text ( help_box_x+8, help_box_y+(temp_counter+=8) , "          GENERAL HELP PAGE!          " );
			OUTPUT_text ( help_box_x+8, help_box_y+(temp_counter+=8) , "--------------------------------------" );
			temp_counter+=8;
			OUTPUT_text ( help_box_x+8, help_box_y+(temp_counter+=8) , "CURSORS SCROLL WINDOW.                " , 255 , 255 , 0 );
			OUTPUT_text ( help_box_x+8, help_box_y+(temp_counter+=8) , "R_SHIFT+CURSORS SCROLL WINDOW 1 ROOM. " , 255 , 255 , 0 );
			OUTPUT_text ( help_box_x+8, help_box_y+(temp_counter+=8) , "HOLD MMB AND DRAG TO MOVE WINDOW.     " , 255 , 255 , 0 );
			temp_counter+=8;
			OUTPUT_text ( help_box_x+8, help_box_y+(temp_counter+=8) , "F1 = GENERAL HELP PAGE.               " , 255 , 255 , 0 );
			OUTPUT_text ( help_box_x+8, help_box_y+(temp_counter+=8) , "F2 = SPECIFIC HELP PAGE.              " , 255 , 255 , 0 );
			OUTPUT_text ( help_box_x+8, help_box_y+(temp_counter+=8) , "F3 = TILEMAP EDITING MODE             " , 255 , 255 , 0 );
			OUTPUT_text ( help_box_x+8, help_box_y+(temp_counter+=8) , "F4 = ROOM ZONE EDITING MODE           " , 255 , 255 , 0 );
			OUTPUT_text ( help_box_x+8, help_box_y+(temp_counter+=8) , "F5 = SPAWN POINT EDITING MODE         " , 255 , 255 , 0 );
			OUTPUT_text ( help_box_x+8, help_box_y+(temp_counter+=8) , "F6 = PATH EDITING MODE                " , 255 , 255 , 0 );
			OUTPUT_text ( help_box_x+8, help_box_y+(temp_counter+=8) , "F7 = AI ZONES EDITING MODE            " , 255 , 255 , 0 );
			OUTPUT_text ( help_box_x+8, help_box_y+(temp_counter+=8) , "F8 = AI NODES EDITING MODE            " , 255 , 255 , 0 );
			OUTPUT_text ( help_box_x+8, help_box_y+(temp_counter+=8) , "F9 = TILE GROUPING EDITOR             " , 255 , 255 , 0 );
			OUTPUT_text ( help_box_x+8, help_box_y+(temp_counter+=8) , "F10 = HELPER TAG EDITOR               " , 255 , 255 , 0 );
			OUTPUT_text ( help_box_x+8, help_box_y+(temp_counter+=8) , "F11 = VERTEX COLOUR EDITOR            " , 255 , 255 , 0 );
			temp_counter+=8;
			OUTPUT_text ( help_box_x+8, help_box_y+(temp_counter+=8) , "DOUBLE CLICKING MMB TOGGLES DIALOGUES." , 255 , 255 , 0 );
			temp_counter+=8;
			OUTPUT_text ( help_box_x+8, help_box_y+(temp_counter+=8) , "Q/A/Z/X = ALTER GUIDE SIZE.           " , 255 , 255 , 0 );
			OUTPUT_text ( help_box_x+8, help_box_y+(temp_counter+=8) , "O/P = SELECT EDITING LAYER.           " , 255 , 255 , 0 );
			OUTPUT_text ( help_box_x+8, help_box_y+(temp_counter+=8) , "K/L = SELECT ZOOM LEVEL.              " , 255 , 255 , 0 );
			OUTPUT_text ( help_box_x+8, help_box_y+(temp_counter+=8) , "N/M = SELECT GRID SIZE.	              " , 255 , 255 , 0 );
			OUTPUT_text ( help_box_x+8, help_box_y+(temp_counter+=8) , "V = TOGGLE VERTEX COLOUR DISPLAY MODE." , 255 , 255 , 0 );
			OUTPUT_text ( help_box_x+8, help_box_y+(temp_counter+=8) , "G = TOGGLE GRID.                      " , 255 , 255 , 0 );
			temp_counter+=8;
		}

		if (CONTROL_key_down(KEY_F2) == true)
		{
			OUTPUT_filled_rectangle ( help_box_x , help_box_y , help_box_x+320 , help_box_y+320 , 0 , 0 , 0 );
			OUTPUT_rectangle ( help_box_x , help_box_y , help_box_x+320 , help_box_y+320 );

			temp_counter = 8;

			OUTPUT_text ( help_box_x+8, help_box_y+(temp_counter+=8) , "          SPECIFIC HELP PAGE!         " );
			OUTPUT_text ( help_box_x+8, help_box_y+(temp_counter+=16) , "--------------------------------------" );

			switch(editing_mode)
			{
			case EDIT_TILEMAP:
				OUTPUT_text ( help_box_x+8, help_box_y+(temp_counter+=8) , "HOLD LMB TO PLACE TILES.              " , 255 , 255 , 0 );
				temp_counter+=8;
				OUTPUT_text ( help_box_x+8, help_box_y+(temp_counter+=8) , "CLICK AND DRAG RMB TO COPY TILES.     " , 255 , 255 , 0 );
				temp_counter+=8;
				OUTPUT_text ( help_box_x+8, help_box_y+(temp_counter+=8) , "ROLL MOUSE TO CYCLE THROUGH TILES.    " , 255 , 255 , 0 );
				temp_counter+=8;
				OUTPUT_text ( help_box_x+8, help_box_y+(temp_counter+=8) , "CLICK ARROWS AT SIDE OF SCREEN TO     " , 255 , 255 , 0 );
				OUTPUT_text ( help_box_x+8, help_box_y+(temp_counter+=8) , "ALTER MAP SIZE. SHIFT-CLICK TO ALTER  " , 255 , 255 , 0 );
				OUTPUT_text ( help_box_x+8, help_box_y+(temp_counter+=8) , "IN MULTIPLES OF 8.                    " , 255 , 255 , 0 );
				temp_counter+=8;
				OUTPUT_text ( help_box_x+8, help_box_y+(temp_counter+=8) , "CLICK ICONS TO ALTER DRAWING MODE.    " , 255 , 255 , 0 );
				temp_counter+=8;
				break;

			case EDIT_ROOM_ZONES:
				OUTPUT_text ( help_box_x+8, help_box_y+(temp_counter+=16) , "CLICK LMB TO PLACE ZONE TILES, OR USE " , 0 , 255 , 255 );
				OUTPUT_text ( help_box_x+8, help_box_y+(temp_counter+=8) , "RECTANGLE OR FILL TOOLS.              " , 0 , 255 , 255 );
				OUTPUT_text ( help_box_x+8, help_box_y+(temp_counter+=8) , "" , 255 , 255 , 0 );
				OUTPUT_text ( help_box_x+8, help_box_y+(temp_counter+=8) , "" , 255 , 255 , 0 );
				OUTPUT_text ( help_box_x+8, help_box_y+(temp_counter+=8) , "" , 255 , 255 , 0 );
				OUTPUT_text ( help_box_x+8, help_box_y+(temp_counter+=8) , "" , 255 , 255 , 0 );
				OUTPUT_text ( help_box_x+8, help_box_y+(temp_counter+=8) , "" , 255 , 255 , 0 );
				break;

			case EDIT_SPAWN_POINTS:
				OUTPUT_text ( help_box_x+8, help_box_y+(temp_counter+=16) , "CLICK LMB TO SELECT SPAWN POINTS WHEN " , 0 , 255 , 255 );
				OUTPUT_text ( help_box_x+8, help_box_y+(temp_counter+=8) , "CURSOR IS OVER ONE.                   " , 0 , 255 , 255 );
				OUTPUT_text ( help_box_x+8, help_box_y+(temp_counter+=16) , "RMB DESELECTS SPAWN POINT.            " , 0 , 255 , 255 );
				OUTPUT_text ( help_box_x+8, help_box_y+(temp_counter+=16) , "CLICK LMB IN FREE AREA TO CREATE NEW  " , 0 , 255 , 255 );
				OUTPUT_text ( help_box_x+8, help_box_y+(temp_counter+=8) , "SPAWN POINTS.                         " , 0 , 255 , 255 );
				OUTPUT_text ( help_box_x+8, help_box_y+(temp_counter+=16) , "ERASE CHOSEN SPAWN POINT WITH DELETE. " , 0 , 255 , 255 );
				OUTPUT_text ( help_box_x+8, help_box_y+(temp_counter+=16) , "ROLL MOUSE TO CYCLE THROUGH SPAWN     " , 0 , 255 , 255 );
				OUTPUT_text ( help_box_x+8, help_box_y+(temp_counter+=8) , "POINTS WITH SAME SCRIPT AS SELECTED.  " , 0 , 255 , 255 );
				break;

			case EDIT_PATHS:
				OUTPUT_text ( help_box_x+8, help_box_y+(temp_counter+=16) , "CLICK LMB TO SELECT PATH/NODE WHEN    " , 0 , 255 , 255 );
				OUTPUT_text ( help_box_x+8, help_box_y+(temp_counter+=8) , "CURSOR IS OVER ONE.                   " , 0 , 255 , 255 );
				OUTPUT_text ( help_box_x+8, help_box_y+(temp_counter+=16) , "LMB TO DRAG SELECTED NODE. HOLD SHIFT " , 0 , 255 , 255 );
				OUTPUT_text ( help_box_x+8, help_box_y+(temp_counter+=8) , "TO EFFECT ALL SUBSEQUENT NODES, TOO.  " , 0 , 255 , 255 );
				OUTPUT_text ( help_box_x+8, help_box_y+(temp_counter+=8) , "RMB DESELECTS PATH/NODE.              " , 0 , 255 , 255 );
				OUTPUT_text ( help_box_x+8, help_box_y+(temp_counter+=16) , "CLICK LMB IN FREE AREA TO CREATE NEW  " , 0 , 255 , 255 );
				OUTPUT_text ( help_box_x+8, help_box_y+(temp_counter+=8) , "PATH/NODE. CLICKING ON A LINE WILL    " , 0 , 255 , 255 );
				OUTPUT_text ( help_box_x+8, help_box_y+(temp_counter+=8) , "SUB-DIVIDE IT WITH A NEW NODE.        " , 0 , 255 , 255 );
				OUTPUT_text ( help_box_x+8, help_box_y+(temp_counter+=16) , "ERASE CHOSEN PATH/NODE WITH DELETE.   " , 0 , 255 , 255 );
				OUTPUT_text ( help_box_x+8, help_box_y+(temp_counter+=8) , "ROLL MOUSE TO CYCLE THROUGH PATHS     " , 0 , 255 , 255 );
				OUTPUT_text ( help_box_x+8, help_box_y+(temp_counter+=8) , "AND NODES.                            " , 0 , 255 , 255 );
				break;

			case EDIT_AI_ZONES:
				OUTPUT_text ( help_box_x+8, help_box_y+(temp_counter+=16) , "CLICK ON ZONE/GATE TO SELECT IT.      " , 0 , 255 , 255 );
				OUTPUT_text ( help_box_x+8, help_box_y+(temp_counter+=8) , "RMB DESELECTS PATH/NODE.              " , 0 , 255 , 255 );
				OUTPUT_text ( help_box_x+8, help_box_y+(temp_counter+=16) , "CLICK AND DRAG LMB IN FREE AREA TO    " , 0 , 255 , 255 );
				OUTPUT_text ( help_box_x+8, help_box_y+(temp_counter+=8) , "CREATE ZONES OR GATES.                " , 0 , 255 , 255 );
				OUTPUT_text ( help_box_x+8, help_box_y+(temp_counter+=16) , "ERASE CHOSEN ZONE/GATE WITH DELETE.   " , 0 , 255 , 255 );
				break;

			case EDIT_AI_NODES:
				
				break;

			case EDIT_COLOURING:
				break;

			}

		}

		// GLOBAL EDITOR CONTROL STUFF!

		if (override_keys == false) // ie, we ain't typing in a name or summat...
		{
			if (CONTROL_key_hit (KEY_ESC) == true)
			{
				exit_editor = true;
			}

			for (t=0; t<DIFFERENT_TILEMAP_EDITING_MODES ;t++)
			{
				if (CONTROL_key_hit(KEY_F3+t))
				{
					TILEMAPS_edit_call_appropriate_editor (editing_mode , STATE_RESET_BUTTONS);
					editing_mode = t;
					TILEMAPS_edit_call_appropriate_editor (editing_mode , STATE_SET_UP_BUTTONS);
					reset_stuff = true;
				}
			}

			for(t=KEY_1; t<KEY_9; t++)
			{
				if (CONTROL_key_hit(t))
				{
					editing_layer = t-KEY_1;
				}

				if (editing_layer >= cm[tilemap_number].map_layers)
				{
					editing_layer = cm[tilemap_number].map_layers - 1;
				}

				stat_altered_blue = stat_altered_green = 255;
				stat_altered_red = 0;
				stat_altered_timer = 60;
				sprintf (stat_altered_message,"EDITING LAYER : %i",editing_layer);
			}

			if (CONTROL_key_hit(KEY_O))
			{
				editing_layer--;

				if (editing_layer < 0)
				{
					editing_layer = 0;
				}

				stat_altered_blue = stat_altered_green = 255;
				stat_altered_red = 0;
				stat_altered_timer = 60;
				sprintf (stat_altered_message,"EDITING LAYER : %i",editing_layer);
			}

			if (CONTROL_key_hit(KEY_P))
			{
				editing_layer++;

				if (editing_layer == cm[tilemap_number].map_layers)
				{
					editing_layer = cm[tilemap_number].map_layers - 1;
				}

				stat_altered_blue = stat_altered_green = 255;
				stat_altered_red = 0;
				stat_altered_timer = 60;
				sprintf (stat_altered_message,"EDITING LAYER : %i",editing_layer);
			}

			if (CONTROL_key_hit(KEY_G))
			{
				draw_grid = !draw_grid;
				if (draw_grid)
				{
					sprintf (stat_altered_message,"GRID DISPLAY ON");
					stat_altered_blue = stat_altered_green = 0;
					stat_altered_red = 255;
					stat_altered_timer = 60;
				}
				else
				{
					sprintf (stat_altered_message,"GRID DISPLAY OFF");
					stat_altered_blue = stat_altered_green = 0;
					stat_altered_red = 255;
					stat_altered_timer = 60;
				}
			}

			if (CONTROL_key_hit(KEY_V))
			{
				colour_display_mode++;
				if (colour_display_mode>COLOUR_DISPLAY_MODE_COMPLEX_VERTEX)
				{
					colour_display_mode=COLOUR_DISPLAY_MODE_NONE;
				}
				sprintf (stat_altered_message,"VERTEX COLOUR DISPLAY = %s",colour_display_mode_names[colour_display_mode]);
				stat_altered_green = 0;
				stat_altered_red = stat_altered_blue = 255;
				stat_altered_timer = 60;
			}

			if (CONTROL_mouse_speed(Z_POS)<0)
			{
				old_tile_width = game_screen_width / int((float(tilesize) * zoom_level));
				old_tile_height = game_screen_height / int((float(tilesize) * zoom_level));

				zoom_level_index--;
				if (zoom_level_index < 0)
				{
					zoom_level_index = 0;
				}
				zoom_level = zoom_level_options[zoom_level_index];

				new_tile_width = game_screen_width / int((float(tilesize) * zoom_level));
				new_tile_height = game_screen_height / int((float(tilesize) * zoom_level));

				temp = new_tile_width - old_tile_width;
				sx -= temp/2;

				temp = new_tile_height - old_tile_height;
				sy -= temp/2;

				zoom_or_grid_altered = true;
			}

			if (CONTROL_mouse_speed(Z_POS)>0)
			{
				zoom_level_index++;
				if (zoom_level_index == MAX_ZOOM_LEVELS)
				{
					zoom_level_index = MAX_ZOOM_LEVELS-1;
				}
				zoom_level = zoom_level_options[zoom_level_index];

				new_tile_width = game_screen_width / int((float(tilesize) * zoom_level));
				new_tile_height = game_screen_height / int((float(tilesize) * zoom_level));

				sx = tx - (new_tile_width/2);
				sy = ty - (new_tile_height/2);

				zoom_or_grid_altered = true;
			}

			if (CONTROL_mouse_hit(MMB))
			{
				scroll_drag = true;
				scroll_drag_start_mx = mx;
				scroll_drag_start_my = my;
				scroll_drag_start_sx = sx;
				scroll_drag_start_sy = sy;
			}
			else if (CONTROL_mouse_down(MMB))
			{
				temp = (mx - scroll_drag_start_mx)/zoomed_tilesize;
				sx = scroll_drag_start_sx - temp;
				temp = (my - scroll_drag_start_my)/zoomed_tilesize;
				sy = scroll_drag_start_sy - temp;

				if ( (sy!=scroll_drag_start_sy) || (sx!=scroll_drag_start_sx) )
				{
					ignore_middle_double_click_count = 20;
				}
			}

			if (CONTROL_key_hit(KEY_N))
			{
				grid_size/=2;
				if (grid_size == 0)
				{
					grid_size = 1;
				}
				zoom_or_grid_altered = true;
			}

			if (CONTROL_key_hit(KEY_M))
			{
				grid_size*=2;
				if (grid_size == 128)
				{
					grid_size = 64;
				}
				zoom_or_grid_altered = true;
			}

			if (zoom_or_grid_altered)
			{
				if ((int (zoom_level * float(grid_size)) < 8) || (grid_size == 1))
				{
					if (grid_size > 1)
					{
						sprintf (stat_altered_message,"GRID NOT VISIBLE AT THIS ZOOM!");
					}
					else
					{
						sprintf (stat_altered_message,"NO GRID!");
					}
					stat_altered_green = stat_altered_blue = 0;
					stat_altered_red = 255;
				}
				else
				{
					sprintf (stat_altered_message,"GRID = %i   ZOOM = %f",grid_size,zoom_level);
					stat_altered_green = 255;
					stat_altered_red = stat_altered_blue = 0;
				}
				stat_altered_timer = 60;
			}	

			if (CONTROL_key_repeat(KEY_Q))
			{
				guide_box_height -= 1;
				if (guide_box_height == 0)
				{
					guide_box_height = 1;
				}
				guide_box_size_changed = true;
			}

			if (CONTROL_key_repeat(KEY_Z))
			{
				guide_box_width -= 1;
				if (guide_box_width == 0)
				{
					guide_box_width = 1;
				}
				guide_box_size_changed = true;
			}

			if (CONTROL_key_repeat(KEY_A))
			{
				guide_box_height += 1;
				if (guide_box_height > cm[tilemap_number].map_height)
				{
					guide_box_height = cm[tilemap_number].map_height;
				}
				guide_box_size_changed = true;
			}

			if (CONTROL_key_repeat(KEY_X))
			{
				guide_box_width += 1;
				if (guide_box_width > cm[tilemap_number].map_width)
				{
					guide_box_width = cm[tilemap_number].map_width;
				}
				guide_box_size_changed = true;
			}

			if (guide_box_size_changed)
			{
				sprintf (stat_altered_message,"WIDTH = %i   HEIGHT = %i",guide_box_width,guide_box_height);
				stat_altered_blue = 0;
				stat_altered_red = stat_altered_green = 255;
				stat_altered_timer = 60;
			}

			if (ignore_middle_double_click_count == 0)
			{
				if (CONTROL_mouse_double_click(MMB,20)==true)
				{
					overlay_display = !overlay_display;
				}
			}
			else
			{
				ignore_middle_double_click_count--;
			}

			if (exit_editor)
			{
				for (editing_mode=0; editing_mode<DIFFERENT_TILEMAP_EDITING_MODES; editing_mode++)
				{
					TILEMAPS_edit_call_appropriate_editor (editing_mode , STATE_SHUTDOWN);
				}

				game_state = GAME_STATE_EDITOR_OVERVIEW;
			}

			if ( (CONTROL_key_down (KEY_RSHIFT) == true) || (CONTROL_key_down (KEY_LSHIFT) == true) )
			{
				if (CONTROL_key_hit (KEY_RIGHT) == true)
				{
					sx += guide_box_width;
					sx = sx - (sx % guide_box_width);
					co_ord_altered = true;
				}
			}
			else
			{
				if (CONTROL_key_down (KEY_RIGHT) == true)
				{
					sx++;
					co_ord_altered = true;
				}
			}

			if ( (CONTROL_key_down (KEY_RSHIFT) == true) || (CONTROL_key_down (KEY_LSHIFT) == true) )
			{
				if (CONTROL_key_hit (KEY_LEFT) == true)
				{
					sx -= 1;
					sx = sx - (sx % guide_box_width);
					co_ord_altered = true;
				}
			}
			else
			{
				if (CONTROL_key_down (KEY_LEFT) == true)
				{
					sx--;
					co_ord_altered = true;
				}
			}

			if ( (CONTROL_key_down (KEY_RSHIFT) == true) || (CONTROL_key_down (KEY_LSHIFT) == true) )
			{
				if (CONTROL_key_hit (KEY_DOWN) == true)
				{
					sy += guide_box_height;
					sy = sy - (sy % guide_box_height);
					co_ord_altered = true;
				}
			}
			else
			{
				if (CONTROL_key_down (KEY_DOWN) == true)
				{
					sy++;
					co_ord_altered = true;
				}
			}

			if ( (CONTROL_key_down (KEY_RSHIFT) == true) || (CONTROL_key_down (KEY_LSHIFT) == true) )
			{
				if (CONTROL_key_hit (KEY_UP) == true)
				{
					sy -= 1;
					sy = sy - (sy % guide_box_height);
					co_ord_altered = true;
				}
			}
			else
			{
				if (CONTROL_key_down (KEY_UP) == true)
				{
					sy--;
					co_ord_altered = true;
				}
			}

			if (co_ord_altered)
			{
				sprintf (stat_altered_message,"TOP LEFT = %i,%i",sx*tilesize,sy*tilesize,sx,sy);
				stat_altered_timer = 60;
				stat_altered_green = 0;
				stat_altered_red = stat_altered_blue = 255;
			}
		}
	}

	return initialise;
}



char * TILEMAPS_get_name (int tilemap_number)
{
	return cm[tilemap_number].name;
}



void TILEMAPS_use_conversion_table (int tilemap_number, int start_layer, int end_layer)
{
	// This uses the conversion table generated by the tileset functions in order
	// to change lumps of the map.
	
	int tile_offset;
	int tileset_index = cm[tilemap_number].tile_set_index;

	int start,end;

	int layer_size = cm[tilemap_number].map_width * cm[tilemap_number].map_height;

	if (start_layer == UNSET)
	{
		start = 0;
		end = layer_size * cm[tilemap_number].map_layers;
	}
	else
	{
		if ( (start_layer<0) || (start_layer>=cm[tilemap_number].map_layers) || (end_layer<0) || (end_layer>=cm[tilemap_number].map_layers) )
		{
			assert(0);	
		}
		else
		{
			start = layer_size * start_layer;
			end = layer_size * (end_layer+1);
		}
	}

	short *tct = ts[tileset_index].tile_conversion_table;
	short *mp = cm[tilemap_number].map_pointer;

	for (tile_offset=start; tile_offset<end; tile_offset++)
	{
		mp[tile_offset] = tct[mp[tile_offset]];
	}
}



int TILEMAPS_get_tilemap_sprite (int map_index)
{
	if (cm == NULL || map_index < 0 || map_index >= number_of_tilemaps_loaded)
	{
		return UNSET;
	}

	int tile_set_index = cm[map_index].tile_set_index;
	return TILESETS_get_sprite_index(tile_set_index);
}



int TILEMAPS_get_map_from_uid (int map_uid)
{
	int map_number;

	for (map_number=0; map_number<number_of_tilemaps_loaded; map_number++)
	{
		if (cm[map_number].uid == map_uid)
		{
			return map_number;
		}
	}

	return UNSET;
}



void TILEMAPS_output_altered_zone_flags_to_file (char *filename)
{
	// Find any spawn point flag in any map which is non-zero and
	// write a line of text giving the map number, the spawn point's
	// uid and the value.

	int tilemap_number;
	int zone_number;

	int map_uid,flag,uid;

	char line[MAX_LINE_SIZE];

	FILE *file_pointer = fopen (MAIN_get_project_filename (filename),"a");

	if (file_pointer != NULL)
	{
		fputs("#START_OF_ZONE_FLAG_DATA\n",file_pointer);

		for (tilemap_number=0; tilemap_number<number_of_tilemaps_loaded; tilemap_number++)
		{
			map_uid = cm[tilemap_number].uid;

			for (zone_number=0; zone_number<cm[tilemap_number].zones; zone_number++)
			{
				if (cm[tilemap_number].zone_list_pointer[zone_number].flag != 0)
				{
					flag = cm[tilemap_number].zone_list_pointer[zone_number].flag;
					uid = cm[tilemap_number].zone_list_pointer[zone_number].uid;

					sprintf (line,"\t#ZONE_FLAG_MAP_UID = %i ZONE_FLAG_UID = %i ZONE_FLAG_VALUE = %i\n",map_uid,uid,flag);

					fputs (line,file_pointer);
				}
			}
		}

		fputs("#END_OF_DATA\n\n",file_pointer);

		fclose(file_pointer);
	}
	else
	{
		OUTPUT_message("Cannot append Zone Flags to save file!");
		assert(0);
	}
}



void TILEMAPS_input_zone_flags_from_file (char *filename)
{
	int map_uid,zone_uid,flag_value;
	char line[MAX_LINE_SIZE];
	char word[MAX_LINE_SIZE];
	char *pointer;
	bool exit_loop = false;
	bool can_exit = false;

	FILE *file_pointer = fopen (MAIN_get_project_filename (filename, true),"r");

	if (file_pointer != NULL)
	{
		while ( ( fgets ( line , MAX_LINE_SIZE , file_pointer ) != NULL ) && (exit_loop == false) )
		{
			STRING_strip_all_disposable (line);

			if (strcmp(line,"#START_OF_ZONE_FLAG_DATA") == 0)
			{
				// We've found the flag data start!
				can_exit = true;
			}

			if (can_exit)
			{
				// We've gotten to the start of the flag data, so lets start looking for stuff! WOOOO!

				pointer = STRING_end_of_string(line,"#ZONE_FLAG_MAP_UID = ");
				if (pointer != NULL)
				{
					strcpy(word,pointer);
					strtok(word," ");
					map_uid = atoi(word);

					pointer = STRING_end_of_string(pointer,"ZONE_FLAG_UID = ");
					if (pointer != NULL)
					{
						strcpy(word,pointer);
						strtok(word," ");
						zone_uid = atoi(word);
						
						pointer = STRING_end_of_string(pointer,"ZONE_FLAG_VALUE = ");
						if (pointer != NULL)
						{
							strcpy(word,pointer);
							strtok(word," ");
							flag_value = atoi(word);

							ROOMZONES_set_zone_flag_by_uid (map_uid, zone_uid, flag_value);
						}
						else
						{
							// WTF!

							OUTPUT_message("No value for flag!");
							assert(0);
						}
					}
					else
					{
						// WTF!

						OUTPUT_message("No index for flag!");
						assert(0);
					}
				}

				if (strcmp(line,"#END_OF_DATA") == 0)
				{
					// We've found the flag data start!
					exit_loop = true;
				}

			}

		}

		fclose(file_pointer);
	}
	else
	{
		OUTPUT_message("Flag reading routine cannot read from Save File!");
		assert(0);
	}
}
