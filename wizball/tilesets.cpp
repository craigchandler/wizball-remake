#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#ifdef ALLEGRO_MACOSX
#include <CoreServices/CoreServices.h>
#endif

#include "tilesets.h" // Duh!
#include "tilemaps.h"
#include "main.h" // For get_project_filename and also state constants
#include "output.h" // Drawing stuff
#include "control.h" // Mouse and keyboard input
#include "global_param_list.h" // For list access
#include "math_stuff.h"
#include "string_stuff.h"
#include "file_stuff.h"
#include "simple_gui.h"

#include "fortify.h"



int window_width = 640 - ICON_SIZE;
int window_height = 480 - (128 + ICON_SIZE);

int number_of_tilesets_loaded = 0;
tileset *ts = NULL;



#define BLOCK_SHAPES			(34)
char block_shape_names [BLOCK_SHAPES][36] = {
"//     \n//     \n//     \n//     \n" ,
"// XXXX\n// XXXX\n// XXXX\n// XXXX\n" ,

"// XXXX\n//  XXX\n//   XX\n//    X\n" ,
"// XXXX\n// XXX \n// XX  \n// X   \n" ,
"//    X\n//   XX\n//  XXX\n// XXXX\n" ,
"// X   \n// XX  \n// XXX \n// XXXX\n" ,

"// XXXX\n//   XX\n//     \n//     \n" ,
"// XXXX\n// XXXX\n// XXXX\n//   XX\n" ,
"// XXXX\n// XXXX\n// XXXX\n// XX  \n" ,
"// XXXX\n// XX  \n//     \n//     \n" ,

"//     \n//     \n//   XX\n// XXXX\n" ,
"//   XX\n// XXXX\n// XXXX\n// XXXX\n" ,
"// XX  \n// XXXX\n// XXXX\n// XXXX\n" ,
"//     \n//     \n// XX  \n// XXXX\n" ,

"//   XX\n//   XX\n//    X\n//    X\n" ,
"// XXXX\n// XXXX\n//  XXX\n//  XXX\n" ,
"//  XXX\n//  XXX\n// XXXX\n// XXXX\n" ,
"//    X\n//    X\n//   XX\n//   XX\n" ,

"// XX  \n// XX  \n// X   \n// X   \n" ,
"// XXXX\n// XXXX\n// XXX \n// XXX \n" ,
"// XXX \n// XXX \n// XXXX\n// XXXX\n" ,
"// X   \n// X   \n// XX  \n// XX  \n" ,

"// XX  \n// XX  \n// XX  \n// XX  \n" ,
"//   XX\n//   XX\n//   XX\n//   XX\n" ,
"// XXXX\n// XXXX\n//     \n//     \n" ,
"//     \n//     \n// XXXX\n// XXXX\n" ,

"//   XX\n//  XXX\n// XXXX\n// XXXX\n" ,
"// XX  \n// XXX \n// XXXX\n// XXXX\n" ,
"// XXXX\n// XXXX\n//  XXX\n//   XX\n" ,
"// XXXX\n// XXXX\n// XXX \n// XX  \n" ,

"// XXXX\n// XX  \n// X   \n// X   \n" ,
"// XXXX\n//   XX\n//    X\n//    X\n" ,
"// X   \n// X   \n// XX  \n// XXXX\n" ,
"//    X\n//    X\n//   XX\n// XXXX\n"
};



#define SOLID_SIDE_COMBOS			(16)
char solid_side_names [SOLID_SIDE_COMBOS][32] = {
"// NONE\n",
"// TOP\n",
"// RIGHT\n",
"// TOP + RIGHT\n",
"// BOTTOM\n",
"// TOP + BOTTOM\n",
"// RIGHT + BOTTOM\n",
"// TOP + RIGHT + BOTTOM\n",
"// LEFT\n",
"// TOP + LEFT\n",
"// RIGHT + LEFT\n",
"// TOP + RIGHT + LEFT\n",
"// BOTTOM + LEFT\n",
"// TOP + BOTTOM + LEFT\n",
"// RIGHT + BOTTOM + LEFT\n",
"// ALL\n"
};



// Top, Right, Bottom, Left

int ignore_neighbouring_sides [BLOCK_SHAPES][4] = {
	1,1,1,1,
	0,0,0,0,

	0,0,1,1,
	0,1,1,0,
	1,0,0,1,
	1,1,0,0,

	0,0,1,1,
	0,0,1,0,
	0,0,1,0,
	0,1,1,0,
	1,0,0,1,
	1,0,0,0,
	1,0,0,0,
	1,1,0,0,

	0,0,1,1,
	0,0,0,1,
	0,0,0,1,
	1,0,0,1,
	0,1,1,0,
	0,1,0,0,
	0,1,0,0,
	1,1,0,0,

	0,1,0,0,
	0,0,0,1,
	0,0,1,0,
	1,0,0,0,

	1,0,0,1,
	1,1,0,0,
	0,0,1,1,
	0,1,1,0,

	0,1,1,0,
	0,0,1,1,
	1,1,0,0,
	1,0,0,1
};



void TILESETS_add_tile_param (int tileset_number, int tile_number, char *variable, char *list, char *entry)
{
	// This mallocs the room needed to add the new paramater and then adds it.

	int paramater_count;

	paramater_count = ts[tileset_number].tileset_pointer[tile_number].params;

	if ( (ts[tileset_number].tileset_pointer[tile_number].param_list_pointer == NULL) && (paramater_count = 0) )
	{
		ts[tileset_number].tileset_pointer[tile_number].param_list_pointer = (parameter *) malloc (sizeof(parameter));
	
		if (ts[tileset_number].tileset_pointer[tile_number].param_list_pointer == NULL)
		{
			assert(0);
		}
	}
	else
	{
		ts[tileset_number].tileset_pointer[tile_number].param_list_pointer = (parameter *) realloc ( ts[tileset_number].tileset_pointer[tile_number].param_list_pointer , (paramater_count+1) * sizeof(parameter) );
		
		if (ts[tileset_number].tileset_pointer[tile_number].param_list_pointer == NULL)
		{
			assert(0);
		}
	}

	strcpy ( ts[tileset_number].tileset_pointer[tile_number].param_list_pointer[paramater_count].param_dest_var , variable );
	strcpy ( ts[tileset_number].tileset_pointer[tile_number].param_list_pointer[paramater_count].param_list_type , list );
	strcpy ( ts[tileset_number].tileset_pointer[tile_number].param_list_pointer[paramater_count].param_name , entry );

	ts[tileset_number].tileset_pointer[tile_number].params++;
}



void TILESETS_destroy_tile_params (int tileset_number, int tile_number)
{
	if (ts[tileset_number].tileset_pointer[tile_number].param_list_pointer != NULL)
	{
		free (ts[tileset_number].tileset_pointer[tile_number].param_list_pointer);
		ts[tileset_number].tileset_pointer[tile_number].param_list_pointer = NULL;
		ts[tileset_number].tileset_pointer[tile_number].params = 0;
	}
}



void TILESETS_destroy_tile (int tileset_number, int tile_number)
{
	if (ts[tileset_number].tileset_pointer[tile_number].param_list_pointer != NULL)
	{
		TILESETS_destroy_tile_params (tileset_number, tile_number);
	}
}



void TILESETS_destroy_tileset (int tileset_number)
{
	int tile_number;

	for (tile_number=0; tile_number<ts[tileset_number].tile_count; tile_number++)
	{
		TILESETS_destroy_tile (tileset_number, tile_number);
	}

	free (ts[tileset_number].tileset_pointer);
	free (ts[tileset_number].tile_conversion_table);
}



void TILESETS_reset_conversion_table (int tilemap_index)
{
	// This uses the passed in variable to figure out a rectangle of tiles.

	int tileset_index = cm[tilemap_index].tile_set_index;

	int tile_number;

	for (tile_number=0; tile_number<ts[tileset_index].tile_count; tile_number++)
	{
		ts[tileset_index].tile_conversion_table[tile_number] = tile_number;
	}

}



void TILESETS_add_to_conversion_table (int tilemap_index, int first_original_tile, int last_original_tile, int first_replacement_tile)
{
	// This uses the passed in variable to figure out a rectangle of tiles.

	int tileset_index = cm[tilemap_index].tile_set_index;
	int image_index = ts[tileset_index].tileset_image_index;
	
	int width_in_tiles = OUTPUT_bitmap_width(image_index) / OUTPUT_get_sprite_width(image_index,0);

	int start_original_tile_x = first_original_tile % width_in_tiles;
	int start_original_tile_y = first_original_tile / width_in_tiles;

	int end_original_tile_x = last_original_tile % width_in_tiles;
	int end_original_tile_y = last_original_tile / width_in_tiles;

	int tile_diff = first_replacement_tile - first_original_tile;

	// Now, the fastest way to do this conversion, I think, is to build up a table
	// which is the same size as a tileset. Luckily there's space within the actual
	// tileset especially for that. Woo!

	// We should have called TILESETS_reset_conversion_table before embarking on this
	// course of action in order to reset the table to it's default values.

	int x,y;
	int tile_number;
	int dest_tile_number;

	for (x=start_original_tile_x; x<=end_original_tile_x; x++)
	{
		for (y=start_original_tile_y; y<=end_original_tile_y; y++)
		{
			tile_number = (y * width_in_tiles) + x;
			dest_tile_number = tile_number + tile_diff;

			if ( (tile_number<0) || (tile_number>=ts[tileset_index].tile_count) || (dest_tile_number<0) || (dest_tile_number>=ts[tileset_index].tile_count) )
			{
				assert(0);
			}

			ts[tileset_index].tile_conversion_table[tile_number] = dest_tile_number;
		}
	}
}



void TILESETS_destroy_all_tilesets (void)
{
	int tileset_number;

	for (tileset_number = 0; tileset_number<number_of_tilesets_loaded ; tileset_number++)
	{
		TILESETS_destroy_tileset (tileset_number);
	}

	free (ts);

	number_of_tilesets_loaded = 0;
}



void TILESETS_swap (int t1, int t2)
{
	// If you wish to INSERT a tileset into the list at specific point (for instance a new tileset into a game)
	// then this will allow for it as you'll just create a new TILESET and then swap all the ones from the end
	// down to the insertion point.

	tileset temp;

	temp = ts[t1];
	ts[t1] = ts[t2];
	ts[t2] = temp;
}



void TILESETS_get_free_name (char *name)
{
	int i;
	int test_num;

	test_num = 0;

	for (i=0; i<number_of_tilesets_loaded; i++)
	{
		sprintf(name,"TILESET_#%3d",test_num);
		STRING_replace_char (name, ' ' , '0');
		if (strcmp(name,ts[i].name) == 0)
		{
			test_num++;
		}
		// As the tileset list is alphabetised it should mean that test_num ends
		// up pointing to the first free name of the "Tileset_#num" variety.
	}

	sprintf(name,"TILESET_#%3d",test_num);
	STRING_replace_char (name, ' ' , '0');

}



void TILESETS_reset_tile (int tileset_number , int tile_number)
{
	int t;

	ts[tileset_number].tileset_pointer[tile_number].shape = 0;
	ts[tileset_number].tileset_pointer[tile_number].solid_sides = 0;
	ts[tileset_number].tileset_pointer[tile_number].default_energy = 1;
	ts[tileset_number].tileset_pointer[tile_number].next_of_kin = UNSET;
	ts[tileset_number].tileset_pointer[tile_number].vulnerability_flag = false;
	for (t=0; t<4; t++)
	{
		ts[tileset_number].tileset_pointer[tile_number].vulnerabilities[t] = 0;
	}
	strcpy ( ts[tileset_number].tileset_pointer[tile_number].dead_script , "UNSET" );
	ts[tileset_number].tileset_pointer[tile_number].dead_script_index = UNSET;
	ts[tileset_number].tileset_pointer[tile_number].priority = 0;
	ts[tileset_number].tileset_pointer[tile_number].convey_x = 0;
	ts[tileset_number].tileset_pointer[tile_number].convey_y = 0;
	ts[tileset_number].tileset_pointer[tile_number].accel_x = 0;
	ts[tileset_number].tileset_pointer[tile_number].accel_y = 0;
	ts[tileset_number].tileset_pointer[tile_number].friction = 0;
	ts[tileset_number].tileset_pointer[tile_number].bounciness = 0;
	ts[tileset_number].tileset_pointer[tile_number].damage = 0;
	ts[tileset_number].tileset_pointer[tile_number].use_only_top_vulnerability = true;
	ts[tileset_number].tileset_pointer[tile_number].params = 0;
	ts[tileset_number].tileset_pointer[tile_number].param_list_pointer = NULL;
	ts[tileset_number].tileset_pointer[tile_number].boolean_properties = 0;
	ts[tileset_number].tileset_pointer[tile_number].neighbouring_blocks = 0;
	ts[tileset_number].tileset_pointer[tile_number].family_neighbours = 0;
	ts[tileset_number].tileset_pointer[tile_number].family_id = UNSET;
	ts[tileset_number].tileset_pointer[tile_number].temp_data = 0;
	ts[tileset_number].tileset_pointer[tile_number].collision_bitmask = 0;
}



void TILESETS_allocate_tile_space (int tileset_number , int new_tile_count)
{
	// This just mallocs/reallocs the space needed for the tile data.

	int t;

	if (ts[tileset_number].tileset_pointer == NULL)
	{
		// Nothing allocated yet so do it, man!
		ts[tileset_number].tileset_pointer = (tile *) malloc (sizeof(tile) * new_tile_count);
		
		for (t=0; t<new_tile_count; t++)
		{
			TILESETS_reset_tile (tileset_number , t);
		}
	}
	else
	{
		// There were already some allocated so realloc.

		ts[tileset_number].tileset_pointer = (tile *) realloc (ts[tileset_number].tileset_pointer , sizeof(tile) * new_tile_count);

		for (t=ts[tileset_number].tile_count ; t<new_tile_count ; t++)
		{
			// Reset any new tiles created.
			TILESETS_reset_tile (tileset_number , t);
		}
	}

	ts[tileset_number].tile_count = new_tile_count;

}



int TILESETS_create (bool new_tileset)
{	
	char default_name[NAME_SIZE];

	if (ts == NULL) // first in list...
	{
		ts = (tileset *) malloc (sizeof (tileset));
	}
	else // Already someone in the list...
	{
		ts = (tileset *) realloc ( ts, (number_of_tilesets_loaded + 1) * sizeof (tileset) );
	}

	ts[number_of_tilesets_loaded].deleted = false;
	ts[number_of_tilesets_loaded].tile_count = 0;
	ts[number_of_tilesets_loaded].tileset_image_index = UNSET;
	ts[number_of_tilesets_loaded].tileset_pointer = NULL;
	ts[number_of_tilesets_loaded].tile_conversion_table = NULL;
	ts[number_of_tilesets_loaded].tilesize = UNSET;
	ts[number_of_tilesets_loaded].altered_this_load = true;

	if (new_tileset) // ie, we're not just freeing up space for a loaded tileset, but rather it's a brand-spanking-new one.
	{
		strcpy (ts[number_of_tilesets_loaded].tileset_sprite_name,"UNSET");
	}

	TILESETS_get_free_name(default_name);
	strcpy (ts[number_of_tilesets_loaded].name,default_name);
	strcpy (ts[number_of_tilesets_loaded].old_name,default_name);

	number_of_tilesets_loaded++;
	return (number_of_tilesets_loaded-1);
}



void TILESETS_draw_loaded_tileset_names (void)
{
	int t;

	for (t=0; t<number_of_tilesets_loaded; t++)
	{
		OUTPUT_text (0,t*8,ts[t].name);
	}

}




void TILESETS_load (char *filename, int tileset_number)
{
	// This loads the verbose tileset which includes all the details pertaining to each tile.

	bool rle_map = false;
	bool exitflag = false;
	bool exitmainloop = false;

	int parameter_number;

	int tile_number = 0;

	char line[TEXT_LINE_SIZE];

	char full_filename [TEXT_LINE_SIZE];

	char *pointer;
	char *param_pointer;

	char temp_char [TEXT_LINE_SIZE];

	FILE_append_filename (full_filename, "tilesets", filename, sizeof(full_filename) );

	FILE *file_pointer = FILE_open_project_read_case_fallback(full_filename);

	if (file_pointer != NULL)
	{
		ts[tileset_number].altered_this_load = false;

		while ( ( fgets ( line , TEXT_LINE_SIZE , file_pointer ) != NULL ) && (exitmainloop == false) )
		{
			STRING_strip_newlines(line);

			pointer = STRING_end_of_string(line,"#END OF FILE");
			if (pointer != NULL)
			{
				exitmainloop = true;
			}

			pointer = STRING_end_of_string(line,"#TILESET IMAGE = ");
			if (pointer != NULL)
			{
				strcpy (ts[tileset_number].tileset_sprite_name,pointer);
			}

			pointer = STRING_end_of_string(line,"#TILE DATA = ");
			if (pointer != NULL)
			{

				ts[tileset_number].tile_count = atoi(pointer);

				ts[tileset_number].tileset_pointer = (tile *) malloc (ts[tileset_number].tile_count * sizeof(tile));
				ts[tileset_number].tile_conversion_table = (short *) malloc (ts[tileset_number].tile_count * sizeof(short));

				for (tile_number=0; tile_number<ts[tileset_number].tile_count; tile_number++)
				{
					TILESETS_reset_tile (tileset_number,tile_number);
				}

				exitflag = false;

				while (exitflag == false)
				{	
					fgets ( line , TEXT_LINE_SIZE , file_pointer );
					STRING_strip_newlines (line);

					pointer = STRING_end_of_string(line,"#TILE NUMBER = ");
					if (pointer != NULL)
					{
						tile_number = atoi(pointer);
					}

					pointer = STRING_end_of_string(line,"#END OF TILE DATA");
					if (pointer != NULL)
					{
						exitflag = true;
					}

					pointer = STRING_end_of_string(line,"#TILE SHAPE = ");
					if (pointer != NULL)
					{
						ts[tileset_number].tileset_pointer[tile_number].shape = atoi(pointer);
					}

					pointer = STRING_end_of_string(line,"#SOLID SIDES = ");
					if (pointer != NULL)
					{
						ts[tileset_number].tileset_pointer[tile_number].solid_sides = atoi(pointer);
					}

					pointer = STRING_end_of_string(line,"#VULNERABLE = ");
					if (pointer != NULL)
					{
						strcpy(temp_char , pointer);
						pointer = strstr(temp_char,"TRUE");
						if (pointer != NULL)
						{
							ts[tileset_number].tileset_pointer[tile_number].vulnerability_flag = true;
						}
						else
						{
							ts[tileset_number].tileset_pointer[tile_number].vulnerability_flag = false;
						}
					}

					pointer = STRING_end_of_string(line,"#DEFAULT ENERGY = ");
					if (pointer != NULL)
					{
						ts[tileset_number].tileset_pointer[tile_number].default_energy = atoi(pointer);
					}

					pointer = STRING_end_of_string(line,"#NEXT OF KIN = ");
					if (pointer != NULL)
					{
						ts[tileset_number].tileset_pointer[tile_number].next_of_kin = atoi(pointer);
					}

					pointer = STRING_end_of_string(line,"#VULNERABILITY (TOP) = ");
					if (pointer != NULL)
					{
						ts[tileset_number].tileset_pointer[tile_number].vulnerabilities[0] = atoi(pointer);
					}

					pointer = STRING_end_of_string(line,"#VULNERABILITY (RIGHT) = ");
					if (pointer != NULL)
					{
						ts[tileset_number].tileset_pointer[tile_number].vulnerabilities[1] = atoi(pointer);
					}

					pointer = STRING_end_of_string(line,"#VULNERABILITY (BOTTOM) = ");
					if (pointer != NULL)
					{
						ts[tileset_number].tileset_pointer[tile_number].vulnerabilities[2] = atoi(pointer);
					}

					pointer = STRING_end_of_string(line,"#VULNERABILITY (LEFT) = ");
					if (pointer != NULL)
					{
						ts[tileset_number].tileset_pointer[tile_number].vulnerabilities[3] = atoi(pointer);
					}

					pointer = STRING_end_of_string(line,"#COLLISION MASK = ");
					if (pointer != NULL)
					{
						ts[tileset_number].tileset_pointer[tile_number].collision_bitmask = atoi(pointer);
					}

					pointer = STRING_end_of_string(line,"#X CONVEY = ");
					if (pointer != NULL)
					{
						ts[tileset_number].tileset_pointer[tile_number].convey_x = atoi(pointer);
						if (atoi(pointer) != 0)
						{
							ts[tileset_number].tileset_pointer[tile_number].deviated_stats = true;
						}
					}

					pointer = STRING_end_of_string(line,"#Y CONVEY = ");
					if (pointer != NULL)
					{
						ts[tileset_number].tileset_pointer[tile_number].convey_y = atoi(pointer);
						if (atoi(pointer) != 0)
						{
							ts[tileset_number].tileset_pointer[tile_number].deviated_stats = true;
						}
					}

					pointer = STRING_end_of_string(line,"#X ACCELL = ");
					if (pointer != NULL)
					{
						ts[tileset_number].tileset_pointer[tile_number].accel_x = atoi(pointer);
						if (atoi(pointer) != 0)
						{
							ts[tileset_number].tileset_pointer[tile_number].deviated_stats = true;
						}
					}

					pointer = STRING_end_of_string(line,"#Y ACCELL = ");
					if (pointer != NULL)
					{
						ts[tileset_number].tileset_pointer[tile_number].accel_y = atoi(pointer);
						if (atoi(pointer) != 0)
						{
							ts[tileset_number].tileset_pointer[tile_number].deviated_stats = true;
						}
					}

					pointer = STRING_end_of_string(line,"#FRICTION = ");
					if (pointer != NULL)
					{
						ts[tileset_number].tileset_pointer[tile_number].friction = atoi(pointer);
						if (atoi(pointer) != 0)
						{
							ts[tileset_number].tileset_pointer[tile_number].deviated_stats = true;
						}
					}

					pointer = STRING_end_of_string(line,"#PRIORITY = ");
					if (pointer != NULL)
					{
						ts[tileset_number].tileset_pointer[tile_number].priority = atoi(pointer);
						if (atoi(pointer) != 0)
						{
							ts[tileset_number].tileset_pointer[tile_number].deviated_stats = true;
						}
					}

					pointer = STRING_end_of_string(line,"#DEAD SCRIPT = ");
					if (pointer != NULL)
					{
						strcpy (ts[tileset_number].tileset_pointer[tile_number].dead_script , pointer);
					}

					pointer = STRING_end_of_string(line,"#PARAMETERS = ");
					if (pointer != NULL)
					{
						ts[tileset_number].tileset_pointer[tile_number].params = atoi(pointer);
						
						if (ts[tileset_number].tileset_pointer[tile_number].params > 0)
						{
							ts[tileset_number].tileset_pointer[tile_number].param_list_pointer = (parameter *) malloc ( ts[tileset_number].tileset_pointer[tile_number].params * sizeof(parameter) );
						}
						else
						{
							ts[tileset_number].tileset_pointer[tile_number].param_list_pointer = NULL;
						}

						for (parameter_number=0; parameter_number<ts[tileset_number].tileset_pointer[tile_number].params; parameter_number++)
						{
							fgets ( line , TEXT_LINE_SIZE , file_pointer );
							STRING_strip_newlines (line);
							pointer = STRING_end_of_string(line,"#PARAMETER ");

							param_pointer = strtok(pointer,"=:\n");
							strcpy (ts[tileset_number].tileset_pointer[tile_number].param_list_pointer[parameter_number].param_dest_var , param_pointer);
							
							param_pointer = strtok(NULL,"=:\n");
							strcpy (ts[tileset_number].tileset_pointer[tile_number].param_list_pointer[parameter_number].param_list_type , param_pointer);

							param_pointer = strtok(NULL,"=:\n");
							strcpy (ts[tileset_number].tileset_pointer[tile_number].param_list_pointer[parameter_number].param_name , param_pointer);
						}
					}
				}
			}

		}

		fclose(file_pointer);
		
		// Oh, and bung the capitalised name in. Haha.
		STRING_uppercase(filename);
		strtok(filename,"."); // Get rid of .txt extension...
		strcpy (ts[tileset_number].name,filename);
		strcpy (ts[tileset_number].old_name,filename);
	}

}



void TILESETS_blank_verbose_data (int tileset_number)
{
	int tile_number;

	int t;

	for (t=0; t<NAME_SIZE; t++)
	{
		ts[tileset_number].name[t] = '\0';
		ts[tileset_number].old_name[t] = '\0';
		ts[tileset_number].tileset_sprite_name[t] = '\0';
	}

	for (tile_number=0; tile_number<ts[tileset_number].tile_count; tile_number++)
	{
		for (t=0; t<NAME_SIZE; t++)
		{
			ts[tileset_number].tileset_pointer[tile_number].dead_script[t] = '\0';
		}
	}
}



void TILESETS_save (int tileset_number)
{
	// Goes through all the tileset data saving all the lovely guff.

	char sides[4][NAME_SIZE] = {"TOP","RIGHT","BOTTOM","LEFT"};

	char boolean_props[22][NAME_SIZE] = {"CONVEY","ACCELLERATE","SLIPPERY","CLIMBABLE_UP_DOWN","CLIMBABLE_LEFT_RIGHT","CLIMBABLE_OMNI","CLIMBABLE_WALL_UP_DOWN","CLIMBABLE_MONKEY_BARS","STICKY_WALL","STICKY_FLOOR","DEADLY","KLUDGE BLOCK","DEKLUDGER","TILE PATHFIND IGNORE","ZONE PATHFIND IGNORE","SPECIAL_1","SPECIAL_2","SPECIAL_3","SPECIAL_4","WATER","DEEP_WATER","HARMFUL"};

	char filename[NAME_SIZE];
	sprintf (filename , "%s.TXT", ts[tileset_number].name);

	char full_filename [TEXT_LINE_SIZE];

	bool exitflag = false;
//	bool sideflag;

	int i,t,j,p;

	char temp_char [TEXT_LINE_SIZE];
	char temp_char_2 [TEXT_LINE_SIZE];

	FILE_append_filename (full_filename,"tilesets", filename, sizeof (full_filename) );

	FILE *file_pointer = fopen (MAIN_get_project_filename (full_filename),"w");

	if (file_pointer != NULL)
	{
		// Tileset Image
		sprintf(temp_char,"#TILESET IMAGE = %s\n\n",ts[tileset_number].tileset_sprite_name);
		fputs(temp_char,file_pointer);

		// Tile data header
		sprintf(temp_char,"#TILE DATA = %d\n\n",ts[tileset_number].tile_count);
		fputs(temp_char,file_pointer);

		for (i=0 ; i<ts[tileset_number].tile_count ; i++)
		{
			sprintf(temp_char,"\t#TILE NUMBER = %d\n",i);
			fputs(temp_char,file_pointer);

			sprintf(temp_char,"\t\t#TILE SHAPE = %d\n",ts[tileset_number].tileset_pointer[i].shape);
			fputs(temp_char,file_pointer);
//			fputs(&block_shape_names [ ts[tileset_number].tileset_pointer[i].shape ][0] , file_pointer);

			sprintf(temp_char,"\t\t#SOLID SIDES = %d\n",ts[tileset_number].tileset_pointer[i].solid_sides);
			fputs(temp_char,file_pointer);
/*
			sprintf(temp_char,"\\\\ ");
			sideflag = false;
			for (t=0; t<4; t++)
			{
				if ( (ts[tileset_number].tileset_pointer[i].solid_sides & MATH_pow (t) ) > 0 )
				{
					strcat ( temp_char , &solid_side_names [t][0] );
					strcat ( temp_char , " + " );
					sideflag = true;
				}
			}
			if (sideflag == true)
			{
				// Get rid of trailing "+"
				temp_char[strlen(temp_char)-2] = '\0';
				strcat (temp_char,"\n");
				// And output it...
				fputs(temp_char,file_pointer);
			}
			else
			{
				fputs("\\\\ NONE\n",file_pointer);
			}
*/
			if (ts[tileset_number].tileset_pointer[i].vulnerability_flag == true)
			{
				fputs("\t\t#VULNERABLE = TRUE\n",file_pointer);
			}
			else
			{
				fputs("\t\t#VULNERABLE = FALSE\n",file_pointer);
			}

			sprintf(temp_char,"\t\t#DEFAULT ENERGY = %d\n",ts[tileset_number].tileset_pointer[i].default_energy);
			fputs(temp_char,file_pointer);

			sprintf(temp_char,"\t\t#NEXT OF KIN = %d\n",ts[tileset_number].tileset_pointer[i].next_of_kin);
			fputs(temp_char,file_pointer);

			for (j=0; j<4; j++)
			{
				sprintf(temp_char,"\t\t#VULNERABILITY (%s) = %d\n",&sides[j][0],ts[tileset_number].tileset_pointer[i].vulnerabilities[j]);
				fputs(temp_char,file_pointer);
				if (ts[tileset_number].tileset_pointer[i].vulnerabilities[j] > 0)
				{
					sprintf(temp_char,"\\ ");
					for (t=0; t<16 ; t++)
					{
						if ( (ts[tileset_number].tileset_pointer[i].vulnerabilities[j] & MATH_pow (t) ) > 0 )
						{
							sprintf( temp_char_2 , "%c" , t+'A' );
							strcat( temp_char , temp_char_2 );
						}
						else
						{
							strcat(temp_char," ");
						}
					}
					strcat(temp_char,"\n");
					fputs(temp_char,file_pointer);
				}
				
			}

			sprintf(temp_char,"\t\t#COLLISION MASK =  %i\n",ts[tileset_number].tileset_pointer[i].collision_bitmask);
			fputs(temp_char,file_pointer);

			sprintf(temp_char,"\t\t#BOOLEAN PROPERTIES = %i\n",ts[tileset_number].tileset_pointer[i].boolean_properties);
			fputs(temp_char,file_pointer);

			sprintf(temp_char,"\t\t#X CONVEY = %i\n",ts[tileset_number].tileset_pointer[i].convey_x);
			fputs(temp_char,file_pointer);
			sprintf(temp_char,"\t\t#Y CONVEY = %i\n",ts[tileset_number].tileset_pointer[i].convey_y);
			fputs(temp_char,file_pointer);

			sprintf(temp_char,"\t\t#X ACCELL = %i\n",ts[tileset_number].tileset_pointer[i].accel_x);
			fputs(temp_char,file_pointer);
			sprintf(temp_char,"\t\t#Y ACCELL = %i\n",ts[tileset_number].tileset_pointer[i].accel_y);
			fputs(temp_char,file_pointer);

			sprintf(temp_char,"\t\t#FRICTION = %i\n",ts[tileset_number].tileset_pointer[i].friction);
			fputs(temp_char,file_pointer);

			sprintf(temp_char,"\t\t#PRIORITY = %i\n",ts[tileset_number].tileset_pointer[i].priority);
			fputs(temp_char,file_pointer);

			sprintf(temp_char,"\t\t#DEAD SCRIPT = %s\n",ts[tileset_number].tileset_pointer[i].dead_script);
			fputs(temp_char,file_pointer);

			sprintf(temp_char,"\t\t#PARAMETERS = %i\n",ts[tileset_number].tileset_pointer[i].params);
			fputs(temp_char,file_pointer);

			if (ts[tileset_number].tileset_pointer[i].params > 0)
			{
				for (p=0 ; p<ts[tileset_number].tileset_pointer[i].params ; p++)
				{
					sprintf(temp_char,"\t\t\t#PARAMETER %s=%s:%s\n" , ts[tileset_number].tileset_pointer[i].param_list_pointer[p].param_dest_var , ts[tileset_number].tileset_pointer[i].param_list_pointer[p].param_list_type , ts[tileset_number].tileset_pointer[i].param_list_pointer[p].param_name );
					fputs(temp_char,file_pointer);
				}
			}

			fputs ("\n",file_pointer);
		}

		fputs("#END OF TILE DATA\n\n",file_pointer);

		fputs("#END OF FILE\n",file_pointer);

		fclose(file_pointer);
	}
	else
	{
		OUTPUT_message("Could not save Tileset!");
		assert(0); // couldn't create file!
	}
}



void TILESETS_append_data (int tileset_number)
{
	FILE *file_pointer = fopen (MAIN_get_project_filename ("tilesets.dat"),"ab");

	if (file_pointer != NULL)
	{
		fwrite (&ts[tileset_number] , sizeof(tileset) , 1 , file_pointer);

		fwrite (ts[tileset_number].tileset_pointer , sizeof(tile) * ts[tileset_number].tile_count , 1 , file_pointer);

		fclose(file_pointer);
	}
	else
	{
		OUTPUT_message("Could not append to tilesets.dat!");
		assert(0); // couldn't create file!
	}
}



static bool TILESETS_is_reasonable_tilesize (int tilesize)
{
	return (tilesize == 8 || tilesize == 16 || tilesize == 32 || tilesize == 64 || tilesize == 128);
}


static void TILESETS_cleanup_loaded_data (int loaded_tilesets)
{
	int tileset_number;

	if (ts != NULL)
	{
		for (tileset_number = 0; tileset_number < loaded_tilesets; tileset_number++)
		{
			if (ts[tileset_number].tileset_pointer != NULL)
			{
				free(ts[tileset_number].tileset_pointer);
				ts[tileset_number].tileset_pointer = NULL;
			}

			if (ts[tileset_number].tile_conversion_table != NULL)
			{
				free(ts[tileset_number].tile_conversion_table);
				ts[tileset_number].tile_conversion_table = NULL;
			}
		}

		free(ts);
		ts = NULL;
	}

	number_of_tilesets_loaded = 0;
}


bool TILESETS_load_game_data (void)
{
	// This loads the data in it's more compressed format, malloc'ing happily as it goes...

	// This routine will only ever be called by the game itself when in release mode.

	int data[1];
	int tileset_number;
	int tile_number;
	int loaded_tilesets = 0;

	FILE *file_pointer = FILE_open_project_case_fallback("tilesets.dat", "rb");
	
	if (file_pointer != NULL)
	{
		if (fread ( data, sizeof(int), 1, file_pointer ) != 1)
		{
			fclose(file_pointer);
			OUTPUT_message("tilesets.dat header read failed.");
			return false;
		}
#ifdef ALLEGRO_MACOSX
		data[0] = EndianS32_LtoN(data[0]);
#endif
		if (data[0] <= 0 || data[0] > 2048)
		{
			fclose(file_pointer);
			OUTPUT_message("tilesets.dat contains an invalid tileset count.");
			return false;
		}

		number_of_tilesets_loaded = data[0];
		ts = (tileset *) calloc (number_of_tilesets_loaded, sizeof (tileset));
		if (ts == NULL)
		{
			fclose(file_pointer);
			OUTPUT_message("Could not allocate tileset table.");
			number_of_tilesets_loaded = 0;
			return false;
		}

		for (tileset_number = 0; tileset_number<number_of_tilesets_loaded; tileset_number++)
		{
			if (fread (&ts[tileset_number] , sizeof(tileset) , 1 , file_pointer) != 1)
			{
				fclose(file_pointer);
				OUTPUT_message("tilesets.dat truncated while reading tileset header.");
				TILESETS_cleanup_loaded_data(loaded_tilesets);
				return false;
			}
#ifdef ALLEGRO_MACOSX
			ts[tileset_number].tilesize = EndianS32_LtoN(ts[tileset_number].tilesize);
			ts[tileset_number].tile_count = EndianS32_LtoN(ts[tileset_number].tile_count);
			ts[tileset_number].tileset_image_index = EndianS32_LtoN(ts[tileset_number].tileset_image_index);
#endif
			if (ts[tileset_number].tile_count <= 0 || ts[tileset_number].tile_count > 65536)
			{
				fclose(file_pointer);
				fprintf(stderr, "Invalid tile count %d in tileset %d\n", ts[tileset_number].tile_count, tileset_number);
				OUTPUT_message("tilesets.dat contains an invalid tile_count.");
				TILESETS_cleanup_loaded_data(loaded_tilesets);
				return false;
			}

			if (!TILESETS_is_reasonable_tilesize(ts[tileset_number].tilesize))
			{
				fclose(file_pointer);
				OUTPUT_message("tilesets.dat contains an invalid tilesize.");
				TILESETS_cleanup_loaded_data(loaded_tilesets);
				return false;
			}

			ts[tileset_number].tileset_pointer = (tile *) malloc (ts[tileset_number].tile_count * sizeof(tile));
			ts[tileset_number].tile_conversion_table = (short *) malloc (ts[tileset_number].tile_count * sizeof(short));
			if (ts[tileset_number].tileset_pointer == NULL || ts[tileset_number].tile_conversion_table == NULL)
			{
				fclose(file_pointer);
				OUTPUT_message("Could not allocate tile data while loading tilesets.dat.");
				TILESETS_cleanup_loaded_data(loaded_tilesets + 1);
				return false;
			}

			if (fread (ts[tileset_number].tileset_pointer , sizeof(tile) , ts[tileset_number].tile_count , file_pointer) != (size_t)ts[tileset_number].tile_count)
			{
				fclose(file_pointer);
				OUTPUT_message("tilesets.dat truncated while reading tiles.");
				TILESETS_cleanup_loaded_data(loaded_tilesets + 1);
				return false;
			}
#ifdef ALLEGRO_MACOSX
			for (tile_number = 0; tile_number < ts[tileset_number].tile_count; ++tile_number)
			{
				ts[tileset_number].tileset_pointer[tile_number].neighbouring_blocks = EndianS32_LtoN(ts[tileset_number].tileset_pointer[tile_number].neighbouring_blocks);
				ts[tileset_number].tileset_pointer[tile_number].family_neighbours = EndianS32_LtoN(ts[tileset_number].tileset_pointer[tile_number].family_neighbours);
				ts[tileset_number].tileset_pointer[tile_number].family_id = EndianS32_LtoN(ts[tileset_number].tileset_pointer[tile_number].family_id);
				ts[tileset_number].tileset_pointer[tile_number].shape = EndianS32_LtoN(ts[tileset_number].tileset_pointer[tile_number].shape);
				ts[tileset_number].tileset_pointer[tile_number].solid_sides = EndianS32_LtoN(ts[tileset_number].tileset_pointer[tile_number].solid_sides);
				ts[tileset_number].tileset_pointer[tile_number].default_energy = EndianS32_LtoN(ts[tileset_number].tileset_pointer[tile_number].default_energy);
				ts[tileset_number].tileset_pointer[tile_number].next_of_kin = EndianS32_LtoN(ts[tileset_number].tileset_pointer[tile_number].next_of_kin);
				ts[tileset_number].tileset_pointer[tile_number].vulnerabilities[0] = EndianS32_LtoN(ts[tileset_number].tileset_pointer[tile_number].vulnerabilities[0]);
				ts[tileset_number].tileset_pointer[tile_number].vulnerabilities[1] = EndianS32_LtoN(ts[tileset_number].tileset_pointer[tile_number].vulnerabilities[1]);
				ts[tileset_number].tileset_pointer[tile_number].vulnerabilities[2] = EndianS32_LtoN(ts[tileset_number].tileset_pointer[tile_number].vulnerabilities[2]);
				ts[tileset_number].tileset_pointer[tile_number].vulnerabilities[3] = EndianS32_LtoN(ts[tileset_number].tileset_pointer[tile_number].vulnerabilities[3]);
				ts[tileset_number].tileset_pointer[tile_number].boolean_properties = EndianS32_LtoN(ts[tileset_number].tileset_pointer[tile_number].boolean_properties);
				ts[tileset_number].tileset_pointer[tile_number].damage = EndianS32_LtoN(ts[tileset_number].tileset_pointer[tile_number].damage);
				ts[tileset_number].tileset_pointer[tile_number].damage_type = EndianS32_LtoN(ts[tileset_number].tileset_pointer[tile_number].damage_type);
				ts[tileset_number].tileset_pointer[tile_number].damage_sides = EndianS32_LtoN(ts[tileset_number].tileset_pointer[tile_number].damage_sides);
				ts[tileset_number].tileset_pointer[tile_number].dead_script_index = EndianS32_LtoN(ts[tileset_number].tileset_pointer[tile_number].dead_script_index);
				ts[tileset_number].tileset_pointer[tile_number].priority = EndianS32_LtoN(ts[tileset_number].tileset_pointer[tile_number].priority);
				ts[tileset_number].tileset_pointer[tile_number].convey_x = EndianS32_LtoN(ts[tileset_number].tileset_pointer[tile_number].convey_x);
				ts[tileset_number].tileset_pointer[tile_number].convey_y = EndianS32_LtoN(ts[tileset_number].tileset_pointer[tile_number].convey_y);
				ts[tileset_number].tileset_pointer[tile_number].accel_x = EndianS32_LtoN(ts[tileset_number].tileset_pointer[tile_number].accel_x);
				ts[tileset_number].tileset_pointer[tile_number].accel_y = EndianS32_LtoN(ts[tileset_number].tileset_pointer[tile_number].accel_y);
				ts[tileset_number].tileset_pointer[tile_number].friction = EndianS32_LtoN(ts[tileset_number].tileset_pointer[tile_number].friction);
				ts[tileset_number].tileset_pointer[tile_number].bounciness = EndianS32_LtoN(ts[tileset_number].tileset_pointer[tile_number].bounciness);
				ts[tileset_number].tileset_pointer[tile_number].params = EndianS32_LtoN(ts[tileset_number].tileset_pointer[tile_number].params);
				ts[tileset_number].tileset_pointer[tile_number].temp_data = EndianS32_LtoN(ts[tileset_number].tileset_pointer[tile_number].temp_data);
				ts[tileset_number].tileset_pointer[tile_number].move_collision_to_layer = EndianS32_LtoN(ts[tileset_number].tileset_pointer[tile_number].move_collision_to_layer);
			}	
#endif
			for (tile_number = 0; tile_number < ts[tileset_number].tile_count; ++tile_number)
			{
				ts[tileset_number].tileset_pointer[tile_number].param_list_pointer = NULL;
				ts[tileset_number].tileset_pointer[tile_number].params = 0;
			}

			loaded_tilesets++;
		}

		fclose (file_pointer);
		return true;
	}
	else
	{
		OUTPUT_message("Could not open tilesets.dat!");
		return false;
	}
}



void TILESETS_save_all (void)
{
	// This saves out all the tilemaps, however first it deletes the original files
	// so we don't create duplicates.

	int tileset_number;
	char filename[NAME_SIZE];
	char full_filename [TEXT_LINE_SIZE];

	for (tileset_number = 0; tileset_number < number_of_tilesets_loaded; tileset_number++)
	{
		if (ts[tileset_number].altered_this_load == true)
		{
			if (strcmp (ts[tileset_number].old_name,"UNSET") != 0)
			{
				sprintf(filename,"%s.TXT", ts[tileset_number].old_name);
				FILE_append_filename(full_filename, "tilesets", filename, sizeof(full_filename) );

				remove (MAIN_get_project_filename (full_filename));
			}
		}
	}

	for (tileset_number = 0; tileset_number < number_of_tilesets_loaded; tileset_number++)
	{
		TILESETS_save (tileset_number);
	}

	// Now we blank ALL the words in the tilemap data. This is so when we save out all the
	// data in a big chunk all the words will be made of zeroed bytes so that they'll zip
	// up a treat.

	for (tileset_number = 0; tileset_number < number_of_tilesets_loaded; tileset_number++)
	{
//		TILESETS_blank_verbose_data (tileset_number);
	}

	// And then we finally splurt out all the data as a big lump.

	FILE *file_pointer = fopen (MAIN_get_project_filename ("tilesets.dat"),"wb");
	int data[1];

	if (file_pointer != NULL)
	{
		data[0] = number_of_tilesets_loaded;
		fwrite (data , sizeof(int) , 1 , file_pointer);
		fclose (file_pointer);
	}
	else
	{
		OUTPUT_message("Could not save tilesets.dat!");
		assert(0); // couldn't create file!
	}

	for (tileset_number = 0; tileset_number < number_of_tilesets_loaded; tileset_number++)
	{
		TILESETS_append_data (tileset_number);
	}
	
}



void TILESETS_load_all (void)
{
	int start,end,i;
	char filename[NAME_SIZE];
	char description[MAX_LINE_SIZE];

	number_of_tilesets_loaded = 0;

	GPL_list_extents("TILESETS",&start,&end);

	for (i=start; i<end ; i++)
	{
		strcpy(filename,GPL_what_is_word_name(i));

		sprintf(description,"LOADING TILEMAP : %s",filename);
		MAIN_draw_loading_picture (description,20);

		strcat(filename,GPL_what_is_list_extension ("TILESETS"));
		TILESETS_load (filename, TILESETS_create (false) );
	}
}



#define EDITING_TILESET_MODE_SHAPE					(0)
#define EDITING_TILESET_MODE_SOLID					(1)
#define EDITING_TILESET_MODE_STATS					(2)
#define EDITING_TILESET_MODE_ENERGY_WEAKNESS		(3)
#define EDITING_TILESET_MODE_NEXT_OF_KIN			(4)
#define EDITING_TILESET_MODE_DEAD_SCRIPT			(5)
#define EDITING_TILESET_MODE_GROUPING				(6)
#define EDITING_TILESET_MODE_FAMILY					(7)
#define EDITING_TILESET_BOOLEAN_PROPS				(8)
#define EDITING_TILESET_COLLISION_BITMASK			(9)
#define EDITING_TILESET_TILE_PRIORITY				(10)

#define DIFFERENT_TILESET_EDITING_MODES				(11)



int TILESETS_draw (int tileset_number , int start_x_pixel_offset , int start_y_pixel_offset , float scale , bool grid , int mx, int my , int editing_mode , int highlighted_tile , int overlay_icon )
{
	// Just draws the tiles at twice magnification.

	int tilesize;
	int rows,columns;
	int screen_rows,screen_columns;
	int bitmap_number;
	int new_tilesize;
	int mouse_over_tile;
	int tile_number;
	int total_tiles;
	int remainder_x;
	int remainder_y;
	int start_x_block;
	int start_y_block;
	float tile_to_icon_ratio;

	mouse_over_tile = UNSET;

	bitmap_number = ts[tileset_number].tileset_image_index;

	tilesize = ts[tileset_number].tilesize;

	new_tilesize = int (tilesize * scale);

	tile_to_icon_ratio = float (tilesize) / float (ICON_SIZE);

	start_x_block = start_x_pixel_offset / new_tilesize;
	start_y_block = start_y_pixel_offset / new_tilesize;

	remainder_x = start_x_pixel_offset % new_tilesize;
	remainder_y = start_y_pixel_offset % new_tilesize;

	rows = (OUTPUT_bitmap_height(bitmap_number) / tilesize);
	columns = (OUTPUT_bitmap_width(bitmap_number) / tilesize);
	
	total_tiles = OUTPUT_bitmap_frames(bitmap_number);

	screen_columns = (window_width) / new_tilesize;
	screen_rows = (window_height) / new_tilesize;
	
	int bx,by;

	for (by = start_y_block ; (by < rows) && (by <= start_y_block+screen_rows+1) ; by++) // the +1 is because we're offsetting the blocks now
	{
		for (bx = start_x_block ; (bx < columns) && (bx <= start_x_block+screen_columns+1) ; bx++) // the +1 is because we're offsetting the blocks now
		{
			tile_number = (by*columns)+bx;

			if (tile_number < total_tiles)
			{
				if (highlighted_tile == UNSET)
				{
					OUTPUT_draw_sprite_scale_no_pivot ( bitmap_number, tile_number , ((bx-start_x_block) * new_tilesize)-remainder_x, ((by-start_y_block) * new_tilesize)-remainder_y , scale );

					switch(editing_mode)
					{
					case EDITING_TILESET_MODE_SHAPE:
						OUTPUT_draw_sprite_scale_no_pivot ( first_icon, ts[tileset_number].tileset_pointer[tile_number].shape + BLOCK_PROFILE_START , ((bx-start_x_block) * new_tilesize)-remainder_x, ((by-start_y_block) * new_tilesize)-remainder_y , scale*tile_to_icon_ratio );
						break;

					case EDITING_TILESET_MODE_SOLID:
						OUTPUT_draw_sprite_scale_no_pivot ( first_icon, ts[tileset_number].tileset_pointer[tile_number].shape + BLOCK_PROFILE_START , ((bx-start_x_block) * new_tilesize)-remainder_x, ((by-start_y_block) * new_tilesize)-remainder_y , scale*tile_to_icon_ratio );
						OUTPUT_draw_sprite_scale_no_pivot ( object_sides_gfx, ts[tileset_number].tileset_pointer[tile_number].solid_sides , ((bx-start_x_block) * new_tilesize)-remainder_x, ((by-start_y_block) * new_tilesize)-remainder_y , scale*tile_to_icon_ratio );
						break;

					case EDITING_TILESET_MODE_ENERGY_WEAKNESS:
						if (ts[tileset_number].tileset_pointer[tile_number].vulnerability_flag == true)
						{
							OUTPUT_draw_sprite_scale_no_pivot ( first_icon, ALTERED_STATS_OVERLAY , ((bx-start_x_block) * new_tilesize)-remainder_x, ((by-start_y_block) * new_tilesize)-remainder_y , scale*tile_to_icon_ratio );
						}
						break;

					case EDITING_TILESET_MODE_STATS:
						if (ts[tileset_number].tileset_pointer[tile_number].deviated_stats == true)
						{
							OUTPUT_draw_sprite_scale_no_pivot ( first_icon, ALTERED_STATS_OVERLAY , ((bx-start_x_block) * new_tilesize)-remainder_x, ((by-start_y_block) * new_tilesize)-remainder_y , scale*tile_to_icon_ratio );
						}
						break;

					case EDITING_TILESET_BOOLEAN_PROPS:
						if (ts[tileset_number].tileset_pointer[tile_number].boolean_properties != 0)
						{
							OUTPUT_draw_sprite_scale_no_pivot ( first_icon, ALTERED_STATS_OVERLAY , ((bx-start_x_block) * new_tilesize)-remainder_x, ((by-start_y_block) * new_tilesize)-remainder_y , scale*tile_to_icon_ratio );
						}
						break;

					case EDITING_TILESET_MODE_DEAD_SCRIPT:
						if ( (ts[tileset_number].tileset_pointer[tile_number].dead_script_index != UNSET) || (ts[tileset_number].tileset_pointer[tile_number].params != 0) )
						{
							OUTPUT_draw_sprite_scale_no_pivot ( first_icon, ALTERED_STATS_OVERLAY , ((bx-start_x_block) * new_tilesize)-remainder_x, ((by-start_y_block) * new_tilesize)-remainder_y , scale*tile_to_icon_ratio );
						}
						break;

					case EDITING_TILESET_MODE_GROUPING:
						if ( (ts[tileset_number].tileset_pointer[tile_number].neighbouring_blocks >= 0) && (ts[tileset_number].tileset_pointer[tile_number].neighbouring_blocks < 16) )
						{
							OUTPUT_draw_sprite_scale_no_pivot ( first_icon, BLOCK_GROUPING_SIDES_START+ts[tileset_number].tileset_pointer[tile_number].neighbouring_blocks , ((bx-start_x_block) * new_tilesize)-remainder_x, ((by-start_y_block) * new_tilesize)-remainder_y , scale*tile_to_icon_ratio );
						}
						else
						{
							OUTPUT_draw_sprite_scale_no_pivot ( first_icon, GROUP_FLAGGING_ICON , ((bx-start_x_block) * new_tilesize)-remainder_x, ((by-start_y_block) * new_tilesize)-remainder_y , scale*tile_to_icon_ratio );
						}
						break;

					case EDITING_TILESET_MODE_FAMILY:
						if ( (ts[tileset_number].tileset_pointer[tile_number].family_neighbours >= 0) && (ts[tileset_number].tileset_pointer[tile_number].family_neighbours < 16) )
						{
							OUTPUT_draw_sprite_scale_no_pivot ( first_icon, BLOCK_GROUPING_SIDES_START+ts[tileset_number].tileset_pointer[tile_number].family_neighbours , ((bx-start_x_block) * new_tilesize)-remainder_x, ((by-start_y_block) * new_tilesize)-remainder_y , scale*tile_to_icon_ratio );
						}
						else
						{
							OUTPUT_draw_sprite_scale_no_pivot ( first_icon, GROUP_FLAGGING_ICON , ((bx-start_x_block) * new_tilesize)-remainder_x, ((by-start_y_block) * new_tilesize)-remainder_y , scale*tile_to_icon_ratio );
						}
						break;

					case EDITING_TILESET_MODE_NEXT_OF_KIN:
						if (ts[tileset_number].tileset_pointer[tile_number].next_of_kin != UNSET)
						{
							OUTPUT_draw_sprite_scale_no_pivot ( first_icon, FROM_THIS_BLOCK , ((bx-start_x_block) * new_tilesize)-remainder_x, ((by-start_y_block) * new_tilesize)-remainder_y , scale*tile_to_icon_ratio );
						}
						break;

					case EDITING_TILESET_COLLISION_BITMASK:
						if (ts[tileset_number].tileset_pointer[tile_number].collision_bitmask < 256)
						{
							OUTPUT_draw_sprite_scale_no_pivot ( bitmask_gfx, ts[tileset_number].tileset_pointer[tile_number].collision_bitmask , ((bx-start_x_block) * new_tilesize)-remainder_x, ((by-start_y_block) * new_tilesize)-remainder_y , scale*tile_to_icon_ratio );
						}
						break;

					case EDITING_TILESET_TILE_PRIORITY:
						if (ts[tileset_number].tileset_pointer[tile_number].priority < 8)
						{
							OUTPUT_draw_sprite_scale_no_pivot ( first_icon, BLOCK_PRIORITY_ICON + ts[tileset_number].tileset_pointer[tile_number].priority , ((bx-start_x_block) * new_tilesize)-remainder_x, ((by-start_y_block) * new_tilesize)-remainder_y , scale*tile_to_icon_ratio );
						}
						break;

					default:
						break;
					}

					if (MATH_rectangle_to_point_intersect ( ((bx-start_x_block) * new_tilesize)-remainder_x , ((by-start_y_block) * new_tilesize)-remainder_y , ((1+bx-start_x_block) * new_tilesize - 1)-remainder_x, ((1+by-start_y_block) * new_tilesize - 1)-remainder_y , mx , my ) == true )
					{
						OUTPUT_rectangle ( ((bx-start_x_block) * new_tilesize)-remainder_x , ((by-start_y_block) * new_tilesize)-remainder_y , ((1+bx-start_x_block) * new_tilesize - 1)-remainder_x, ((1+by-start_y_block) * new_tilesize - 1)-remainder_y );
						mouse_over_tile = tile_number;
					}
				}
				else
				{
					if (highlighted_tile == tile_number)
					{
						OUTPUT_draw_sprite_scale_no_pivot ( first_icon, overlay_icon , ((bx-start_x_block) * new_tilesize)-remainder_x, ((by-start_y_block) * new_tilesize)-remainder_y , scale*tile_to_icon_ratio );
					}
				}
			}

		}
	}

	return mouse_over_tile;

}



#define TILESET_PROPERTY_EDITING_PARAMETER_VARIABLE			(0)
#define TILESET_PROPERTY_EDITING_PARAMETER_VALUE			(1)
#define TILESET_PROPERTY_EDITING_DEAD_SCRIPT				(2)

#define MAX_PARAMETERS_FOR_SCRIPT							(256)



bool TILESETS_edit_tile_script_properties (int state , int *current_cursor ,  int tileset_number, int selected_tile, int mx, int my)
{
	static bool override_main_editor;

	static char dead_script[NAME_SIZE];
	static char scripts_name[NAME_SIZE];
	static char variables_name[NAME_SIZE];

	static char parameter_variable[MAX_PARAMETERS_FOR_SCRIPT][NAME_SIZE];
	static char parameter_list[MAX_PARAMETERS_FOR_SCRIPT][NAME_SIZE];
	static char parameter_entry[MAX_PARAMETERS_FOR_SCRIPT][NAME_SIZE];

	static int first_parameter_in_list;
	static int editing_parameter_number;
	static int total_parameters;

	static int editing_property;

	int mouse_in_parameter = UNSET;
	int mouse_in_box = UNSET;

	int box_size = 152;

	static bool delete_selected;

	int t;

	if (state == STATE_INITIALISE)
	{
		override_main_editor = false;

		strcpy (dead_script,"UNSET");
		strcpy (scripts_name,"SCRIPTS");;
		strcpy (variables_name,"VARIABLE");

		editing_property = UNSET;

		total_parameters = 0;
		editing_parameter_number = 0;
		first_parameter_in_list = 0;

		delete_selected = false;

	}
	else if (state == STATE_SHUTDOWN)
	{
		override_main_editor = false;
	}
	else if (state == STATE_RUNNING)
	{
		// Input

		if ( (mx<window_width) && (my<window_height) && (override_main_editor == false) )
		{
			if (selected_tile != UNSET)
			{
				// Left-Mouse Hit! The writing stuff!

				if (CONTROL_mouse_hit (LMB) == true)
				{
					// Destroy the old parameters.

					TILESETS_destroy_tile_params (tileset_number, selected_tile);

					// Add the new params...
					for (t=0; t<total_parameters; t++)
					{
						TILESETS_add_tile_param (tileset_number, selected_tile, parameter_variable[t] , parameter_list[t] , parameter_entry[t] );
					}

					// Set the dead script...

					strcpy ( ts[tileset_number].tileset_pointer[selected_tile].dead_script , dead_script );

					// Then relink 'em.

					TILESETS_confirm_tile_links (tileset_number,selected_tile);

				}

				// Right-Mouse Hit! The reading stuff!

				if (CONTROL_mouse_down (RMB) == true)
				{
					// Read all the parameters in from the selected tile.
					
					total_parameters = ts[tileset_number].tileset_pointer[selected_tile].params;

					for (t=0; t<total_parameters; t++)
					{
						strcpy (parameter_variable[t] , ts[tileset_number].tileset_pointer[selected_tile].param_list_pointer[t].param_dest_var );
						strcpy (parameter_list[t] , ts[tileset_number].tileset_pointer[selected_tile].param_list_pointer[t].param_list_type );
						strcpy (parameter_entry[t] , ts[tileset_number].tileset_pointer[selected_tile].param_list_pointer[t].param_name );
					}

					// Grab the dead_script from the selected tile.

					strcpy ( dead_script , ts[tileset_number].tileset_pointer[selected_tile].dead_script );
				}
			}
		}

		if (CONTROL_mouse_hit(LMB) == true)
		{
			if (MATH_rectangle_to_point_intersect( 0, window_height+(ICON_SIZE*2) ,box_size-1 ,window_height+(ICON_SIZE*4)-1 ,mx,my) == true)
			{
				// Clicking to alter the script name.
				editing_property = TILESET_PROPERTY_EDITING_DEAD_SCRIPT;
				override_main_editor = true;
			}

			if (MATH_rectangle_to_point_intersect( 0, window_height+(ICON_SIZE*4) ,box_size-1 ,window_height+(ICON_SIZE*5)-1 ,mx,my) == true)
			{
				// Toggle delete mode!

				if (delete_selected==true)
				{
					delete_selected = false;
				}
				else
				{
					delete_selected = true;
				}
			}

			
		}

		// Output

		SIMPGUI_draw_all_buttons (TILESET_SUB_GROUP_ID,true);

		OUTPUT_boxed_centred_text (320,window_height+ICON_SIZE+12,"DEAD SCRIPT CHOOSING MODE");

		OUTPUT_centred_text (box_size/2,window_height+(ICON_SIZE*2)+8,"CURRENT SCRIPT");
		OUTPUT_truncated_text ((box_size/FONT_WIDTH)-1,(FONT_WIDTH/2),window_height+(ICON_SIZE*2)+24,dead_script);

		OUTPUT_rectangle ( 0, window_height+(ICON_SIZE*2) ,box_size-1 ,window_height+(ICON_SIZE*4)-1 ,0 ,0 ,255 );

		if (delete_selected == true)
		{
			OUTPUT_rectangle ( 0, window_height+(ICON_SIZE*4) ,box_size-1 ,window_height+(ICON_SIZE*5)-1 ,255 ,255 ,255 );
			OUTPUT_centred_text (box_size/2,window_height+(ICON_SIZE*4)+12,"DELETE PARAMETER");
		}
		else
		{
			OUTPUT_rectangle ( 0, window_height+(ICON_SIZE*4) ,box_size-1 ,window_height+(ICON_SIZE*5)-1 ,0 ,0 ,255 );
			OUTPUT_centred_text (box_size/2,window_height+(ICON_SIZE*4)+12,"DELETE PARAMETER",0,0,255);
		}

		// Output the list of current parameters...

		for (t=0; (t<total_parameters+1) && (t<(ICON_SIZE*4)/16); t++)
		{
			if (MATH_rectangle_to_point_intersect( (box_size*1), window_height+(ICON_SIZE*2)+(t*16), (box_size*4)-1, window_height+(ICON_SIZE*2)+(t*16)+15 , mx, my) == true)
			{
				mouse_in_parameter = t+first_parameter_in_list;
				mouse_in_box = (mx-box_size) / box_size;
			}

			OUTPUT_rectangle ( (box_size*1), window_height+(ICON_SIZE*2)+(t*16), (box_size*2)-1, window_height+(ICON_SIZE*2)+(t*16)+15 , 255 , 0 , 0 );
			OUTPUT_rectangle ( (box_size*2), window_height+(ICON_SIZE*2)+(t*16), (box_size*3)-1, window_height+(ICON_SIZE*2)+(t*16)+15 , 255 , 0 , 0 );
			OUTPUT_rectangle ( (box_size*3), window_height+(ICON_SIZE*2)+(t*16), (box_size*4)-1, window_height+(ICON_SIZE*2)+(t*16)+15 , 255 , 0 , 0 );

			if (t+first_parameter_in_list<total_parameters)
			{
				OUTPUT_truncated_text ( (box_size/FONT_WIDTH)-1 , (box_size*1) + (FONT_WIDTH/2) , window_height+(ICON_SIZE*2)+(t*16)+(FONT_HEIGHT/2) , parameter_variable[t+first_parameter_in_list] , 255, 255, 0 );
				OUTPUT_truncated_text ( (box_size/FONT_WIDTH)-1 , (box_size*2) + (FONT_WIDTH/2) , window_height+(ICON_SIZE*2)+(t*16)+(FONT_HEIGHT/2) , parameter_list[t+first_parameter_in_list] , 0, 255, 255 );
				OUTPUT_truncated_text ( (box_size/FONT_WIDTH)-1 , (box_size*3) + (FONT_WIDTH/2) , window_height+(ICON_SIZE*2)+(t*16)+(FONT_HEIGHT/2) , parameter_entry[t+first_parameter_in_list] , 0, 255, 0 );
			}
			else
			{
				OUTPUT_truncated_text ( (box_size/FONT_WIDTH)-1 , (box_size*1) + (FONT_WIDTH/2) , window_height+(ICON_SIZE*2)+(t*16)+(FONT_HEIGHT/2) , "CREATE" , 255, 0, 255 );
				OUTPUT_truncated_text ( (box_size/FONT_WIDTH)-1 , (box_size*2) + (FONT_WIDTH/2) , window_height+(ICON_SIZE*2)+(t*16)+(FONT_HEIGHT/2) , "NEW" , 255, 0, 255 );
				OUTPUT_truncated_text ( (box_size/FONT_WIDTH)-1 , (box_size*3) + (FONT_WIDTH/2) , window_height+(ICON_SIZE*2)+(t*16)+(FONT_HEIGHT/2) , "PARAMETER" , 255, 0, 255 );
			}
		}

		if (override_main_editor == true)
		{
			if (editing_property == TILESET_PROPERTY_EDITING_PARAMETER_VARIABLE)
			{
				override_main_editor = EDIT_gpl_list_menu (80, 48, variables_name , parameter_variable[editing_parameter_number], true);
			}
			else if (editing_property == TILESET_PROPERTY_EDITING_PARAMETER_VALUE)
			{
				override_main_editor = EDIT_gpl_list_menu (80, 48, parameter_list[editing_parameter_number] , parameter_entry[editing_parameter_number], false);
			}
			else if (editing_property == TILESET_PROPERTY_EDITING_DEAD_SCRIPT)
			{
				override_main_editor = EDIT_gpl_list_menu (80, 48, scripts_name , dead_script, true);
			}
		}

		SIMPGUI_draw_all_buttons (TILESET_SUB_GROUP_ID);

		if ( (CONTROL_mouse_hit(LMB)) && (override_main_editor == false) )
		{
			if (mouse_in_parameter != UNSET)
			{
				if ( (mouse_in_parameter == total_parameters) && (total_parameters < MAX_PARAMETERS_FOR_SCRIPT-1) && (delete_selected == false) )
				{
					// Create new parameter!
					strcpy(parameter_variable[total_parameters] , "UNSET" );
					strcpy(parameter_list[total_parameters] , "UNSET" );
					strcpy(parameter_entry[total_parameters] , "UNSET" );
					total_parameters++;

					SIMPGUI_kill_button (TILESET_SUB_GROUP_ID, ALL_OTHER_GUI_ELEMENTS);
				
					if (total_parameters>5)
					{
						SIMPGUI_create_v_drag_bar (TILESET_SUB_GROUP_ID, ALL_OTHER_GUI_ELEMENTS, &first_parameter_in_list, box_size*4, window_height+(ICON_SIZE*2), ICON_SIZE, 96, 0, total_parameters-5, 1, ICON_SIZE, LMB);
					}


				}
				else
				{
					if (delete_selected == false)
					{
						// You've selected an option which means it's gonna' be altered.

						if (mouse_in_box == 0)
						{
							editing_parameter_number = mouse_in_parameter;
							editing_property = TILESET_PROPERTY_EDITING_PARAMETER_VARIABLE;
							override_main_editor = true;
						}
						else if ( (mouse_in_box == 1) || (mouse_in_box == 2) )
						{
							editing_parameter_number = mouse_in_parameter;
							editing_property = TILESET_PROPERTY_EDITING_PARAMETER_VALUE;
							override_main_editor = true;
						}
					}
					else if (mouse_in_parameter < total_parameters)
					{
						delete_selected = false;

						for (t=mouse_in_parameter ; t<total_parameters-1; t++)
						{
							strcpy (parameter_variable[t] , parameter_variable[t+1] );
							strcpy (parameter_list[t] , parameter_list[t+1] );
							strcpy (parameter_entry[t] , parameter_entry[t+1] );
						}
						total_parameters--;

						// BUG HERE SOMEWHERE!

						SIMPGUI_kill_button (TILESET_SUB_GROUP_ID, ALL_OTHER_GUI_ELEMENTS);
					
						if (total_parameters>5)
						{
							SIMPGUI_create_v_drag_bar (TILESET_SUB_GROUP_ID, ALL_OTHER_GUI_ELEMENTS, &first_parameter_in_list, box_size*4, window_height+(ICON_SIZE*2), ICON_SIZE, 96, 0, total_parameters-5, 1, ICON_SIZE, LMB);
						}
						else
						{
							first_parameter_in_list = 0;
						}
					}

				}
			}
		}



	}
	else if (state == STATE_SET_UP_BUTTONS)
	{
		if (total_parameters>5)
		{
			SIMPGUI_create_v_drag_bar (TILESET_SUB_GROUP_ID, ALL_OTHER_GUI_ELEMENTS, &first_parameter_in_list, box_size*4, window_height+(ICON_SIZE*2), ICON_SIZE, 96, 0, total_parameters-5, 1, ICON_SIZE, LMB);
		}
	}
	else if (state == STATE_RESET_BUTTONS)
	{

	}


	return override_main_editor;

}



bool TILESETS_edit_tile_accell_properties (int state , int *current_cursor ,  int tileset_number, int selected_tile, int mx, int my)
{
	static bool override_main_editor;

	static int accel_x;
	static int accel_y;
	static int convey_x;
	static int convey_y;
	static int friction;
	static int bounciness;

	char text[TEXT_LINE_SIZE];

	if (state == STATE_INITIALISE)
	{
		override_main_editor = false;

		accel_x = 0;
		accel_y = 0;
		convey_x = 0;
		convey_y = 0;
		friction = 0;
		bounciness = 0;
	}
	else if (state == STATE_SHUTDOWN)
	{
		override_main_editor = false;


	}
	else if (state == STATE_RUNNING)
	{
		// Input

		if ( (mx<window_width) && (my<window_height) )
		{
			if (selected_tile != UNSET)
			{
				// Left-Mouse Hit! The writing stuff!

				if (CONTROL_mouse_down (LMB) == true)
				{
					ts[tileset_number].tileset_pointer[selected_tile].accel_x = (accel_x);
					ts[tileset_number].tileset_pointer[selected_tile].accel_y = (accel_y);
					ts[tileset_number].tileset_pointer[selected_tile].convey_x = (convey_x);
					ts[tileset_number].tileset_pointer[selected_tile].convey_y = (convey_y);
					ts[tileset_number].tileset_pointer[selected_tile].friction = (friction);
					ts[tileset_number].tileset_pointer[selected_tile].bounciness = (bounciness);

					if ( (accel_x != 0) || (accel_y !=0) || (convey_x !=0) || (convey_y !=0) || (friction !=0) || (bounciness !=0) )
					{
						ts[tileset_number].tileset_pointer[selected_tile].deviated_stats = true;
					}
					else
					{
						ts[tileset_number].tileset_pointer[selected_tile].deviated_stats = false;
					}
				}

				// Left-Mouse Hit! The reading stuff!

				if (CONTROL_mouse_down (RMB) == true)
				{
					accel_x = ts[tileset_number].tileset_pointer[selected_tile].accel_x;
					accel_y = ts[tileset_number].tileset_pointer[selected_tile].accel_y;
					convey_x = ts[tileset_number].tileset_pointer[selected_tile].convey_x;
					convey_y = ts[tileset_number].tileset_pointer[selected_tile].convey_y;
					friction = ts[tileset_number].tileset_pointer[selected_tile].friction;
					bounciness = ts[tileset_number].tileset_pointer[selected_tile].bounciness;
				}
			}
		}

		// Output
		OUTPUT_boxed_centred_text (320,window_height+ICON_SIZE+12,"GENERAL STATISTIC EDITING MODE");

		OUTPUT_centred_text (32,window_height+ICON_SIZE*3,"ACCELL X");
		OUTPUT_centred_text (128,window_height+ICON_SIZE*3,"ACCELL Y");
		OUTPUT_centred_text (224,window_height+ICON_SIZE*3,"CONVEY X");
		OUTPUT_centred_text (320,window_height+ICON_SIZE*3,"CONVEY Y");
		OUTPUT_centred_text (512,window_height+ICON_SIZE*3,"FRICTION");
		OUTPUT_centred_text (608,window_height+ICON_SIZE*3,"BOUNCE");

		sprintf(text,"%d",accel_x);
		OUTPUT_centred_text (32,window_height+ICON_SIZE*3+8,text);

		sprintf(text,"%d",accel_y);
		OUTPUT_centred_text (128,window_height+ICON_SIZE*3+8,text);

		sprintf(text,"%d",convey_x);
		OUTPUT_centred_text (224,window_height+ICON_SIZE*3+8,text);

		sprintf(text,"%d",convey_y);
		OUTPUT_centred_text (320,window_height+ICON_SIZE*3+8,text);

		sprintf(text,"%d%%",friction);
		OUTPUT_centred_text (512,window_height+ICON_SIZE*3+8,text);

		sprintf(text,"%d%%",bounciness);
		OUTPUT_centred_text (608,window_height+ICON_SIZE*3+8,text);

		SIMPGUI_draw_all_buttons ();
	}
	else if (state == STATE_SET_UP_BUTTONS)
	{
		SIMPGUI_create_nudge_button (TILESET_SUB_GROUP_ID, ALL_OTHER_GUI_ELEMENTS, &accel_x, (ICON_SIZE*0), window_height+(ICON_SIZE*2), first_icon, LEFT_ARROW_ICON, LMB, -128, 128, -1, false);
		SIMPGUI_create_nudge_button (TILESET_SUB_GROUP_ID, ALL_OTHER_GUI_ELEMENTS, &accel_y, (ICON_SIZE*3), window_height+(ICON_SIZE*2), first_icon, LEFT_ARROW_ICON, LMB, -128, 128, -1, false);
		SIMPGUI_create_nudge_button (TILESET_SUB_GROUP_ID, ALL_OTHER_GUI_ELEMENTS, &convey_x, (ICON_SIZE*6), window_height+(ICON_SIZE*2), first_icon, LEFT_ARROW_ICON, LMB, -1280, 1280, -1, false);
		SIMPGUI_create_nudge_button (TILESET_SUB_GROUP_ID, ALL_OTHER_GUI_ELEMENTS, &convey_y, (ICON_SIZE*9), window_height+(ICON_SIZE*2), first_icon, LEFT_ARROW_ICON, LMB, -1280, 1280, -1, false);
		SIMPGUI_create_nudge_button (TILESET_SUB_GROUP_ID, ALL_OTHER_GUI_ELEMENTS, &friction, (ICON_SIZE*15), window_height+(ICON_SIZE*2), first_icon, LEFT_ARROW_ICON, LMB, 0, 100, -1, false);
		SIMPGUI_create_nudge_button (TILESET_SUB_GROUP_ID, ALL_OTHER_GUI_ELEMENTS, &bounciness, (ICON_SIZE*18), window_height+(ICON_SIZE*2), first_icon, LEFT_ARROW_ICON, LMB, 0, 1000, -1, false);

		SIMPGUI_create_nudge_button (TILESET_SUB_GROUP_ID, ALL_OTHER_GUI_ELEMENTS, &accel_x, (ICON_SIZE*1), window_height+(ICON_SIZE*2), first_icon, RIGHT_ARROW_ICON, LMB, -128, 128, 1, false);
		SIMPGUI_create_nudge_button (TILESET_SUB_GROUP_ID, ALL_OTHER_GUI_ELEMENTS, &accel_y, (ICON_SIZE*4), window_height+(ICON_SIZE*2), first_icon, RIGHT_ARROW_ICON, LMB, -128, 128, 1, false);
		SIMPGUI_create_nudge_button (TILESET_SUB_GROUP_ID, ALL_OTHER_GUI_ELEMENTS, &convey_x, (ICON_SIZE*7), window_height+(ICON_SIZE*2), first_icon, RIGHT_ARROW_ICON, LMB, -1280, 1280, 1, false);
		SIMPGUI_create_nudge_button (TILESET_SUB_GROUP_ID, ALL_OTHER_GUI_ELEMENTS, &convey_y, (ICON_SIZE*10), window_height+(ICON_SIZE*2), first_icon, RIGHT_ARROW_ICON, LMB, -1280, 1280, 1, false);
		SIMPGUI_create_nudge_button (TILESET_SUB_GROUP_ID, ALL_OTHER_GUI_ELEMENTS, &friction, (ICON_SIZE*16), window_height+(ICON_SIZE*2), first_icon, RIGHT_ARROW_ICON, LMB, 0, 100, 1, false);
		SIMPGUI_create_nudge_button (TILESET_SUB_GROUP_ID, ALL_OTHER_GUI_ELEMENTS, &bounciness, (ICON_SIZE*19), window_height+(ICON_SIZE*2), first_icon, RIGHT_ARROW_ICON, LMB, 0, 1000, 1, false);
	}
	else if (state == STATE_RESET_BUTTONS)
	{

	}

	return override_main_editor;
}



bool TILESETS_edit_tile_shape (int state , int *current_cursor ,  int tileset_number, int selected_tile, int mx, int my)
{
	static bool override_main_editor;

	static int current_block_profile;
	int t;

	
	if (state == STATE_INITIALISE)
	{
		override_main_editor = false;
		current_block_profile = 0;

	}
	else if (state == STATE_SHUTDOWN)
	{
		override_main_editor = false;
		current_block_profile = 0;


	}
	else if (state == STATE_RUNNING)
	{
		// Input
		if ( (mx<window_width) && (my<window_height) )
		{
			if (selected_tile != UNSET)
			{
				// Left-Mouse Hit! The writing stuff!

				if (CONTROL_mouse_down (LMB) == true)
				{
					ts[tileset_number].tileset_pointer[selected_tile].shape = current_block_profile;
				}

				// Right-Mouse Hit! The reading stuff!

				if (CONTROL_mouse_down (RMB) == true)
				{
					current_block_profile = ts[tileset_number].tileset_pointer[selected_tile].shape;
				}
			}
		}

		// Output
		OUTPUT_boxed_centred_text (320,window_height+ICON_SIZE+12,"BLOCK SHAPE SELECTION MODE");

		for (t=0; t<(BLOCK_SHAPES/2); t++)
		{
//			OUTPUT_draw_masked_sprite ( first_icon, BLOCK_PROFILE_START+t , (t+2)*ICON_SIZE , window_height + ICON_SIZE*2 );
		}
		for (t=(BLOCK_SHAPES/2); t<BLOCK_SHAPES; t++)
		{
//			OUTPUT_draw_masked_sprite ( first_icon, BLOCK_PROFILE_START+t , (t+2-(BLOCK_SHAPES/2))*ICON_SIZE , window_height + ICON_SIZE*3 );
		}

		OUTPUT_draw_sprite_scale_no_pivot (first_icon,BLOCK_PROFILE_START+current_block_profile,0,window_height+ICON_SIZE*2,2);

		SIMPGUI_draw_all_buttons ();

	}
	else if (state == STATE_SET_UP_BUTTONS)
	{
//		SIMPGUI_create_h_ramp_button (TILESET_SUB_GROUP_ID, ALL_OTHER_GUI_ELEMENTS, &current_block_profile, ICON_SIZE*2 , window_height+ICON_SIZE*2, (BLOCK_SHAPES/2)*ICON_SIZE, ICON_SIZE , 0, (BLOCK_SHAPES/2), 1, LMB , false , false);
//		SIMPGUI_create_h_ramp_button (TILESET_SUB_GROUP_ID, ALL_OTHER_GUI_ELEMENTS, &current_block_profile, ICON_SIZE*2 , window_height+ICON_SIZE*3, (BLOCK_SHAPES/2)*ICON_SIZE, ICON_SIZE , (BLOCK_SHAPES/2), BLOCK_SHAPES, 1, LMB , false , false);

		SIMPGUI_create_radio_set_button (TILESET_SUB_GROUP_ID, ALL_OTHER_GUI_ELEMENTS, &current_block_profile, ICON_SIZE*3, window_height+ICON_SIZE*2, first_icon, BLOCK_PROFILE_START, LMB, 0);
		SIMPGUI_create_radio_set_button (TILESET_SUB_GROUP_ID, ALL_OTHER_GUI_ELEMENTS, &current_block_profile, ICON_SIZE*3, window_height+ICON_SIZE*3, first_icon, BLOCK_PROFILE_START+1, LMB, 1);

		SIMPGUI_create_radio_set_button (TILESET_SUB_GROUP_ID, ALL_OTHER_GUI_ELEMENTS, &current_block_profile, (ICON_SIZE*4), window_height+ICON_SIZE*3, first_icon, BLOCK_PROFILE_START+2, LMB, 2);
		SIMPGUI_create_radio_set_button (TILESET_SUB_GROUP_ID, ALL_OTHER_GUI_ELEMENTS, &current_block_profile, (ICON_SIZE*5), window_height+ICON_SIZE*3, first_icon, BLOCK_PROFILE_START+3, LMB, 3);
		SIMPGUI_create_radio_set_button (TILESET_SUB_GROUP_ID, ALL_OTHER_GUI_ELEMENTS, &current_block_profile, (ICON_SIZE*4), window_height+ICON_SIZE*2, first_icon, BLOCK_PROFILE_START+4, LMB, 4);
		SIMPGUI_create_radio_set_button (TILESET_SUB_GROUP_ID, ALL_OTHER_GUI_ELEMENTS, &current_block_profile, (ICON_SIZE*5), window_height+ICON_SIZE*2, first_icon, BLOCK_PROFILE_START+5, LMB, 5);

		SIMPGUI_create_radio_set_button (TILESET_SUB_GROUP_ID, ALL_OTHER_GUI_ELEMENTS, &current_block_profile, ICON_SIZE*6, window_height+ICON_SIZE*3, first_icon, BLOCK_PROFILE_START+6, LMB, 6);
		SIMPGUI_create_radio_set_button (TILESET_SUB_GROUP_ID, ALL_OTHER_GUI_ELEMENTS, &current_block_profile, ICON_SIZE*7, window_height+ICON_SIZE*3, first_icon, BLOCK_PROFILE_START+7, LMB, 7);
		SIMPGUI_create_radio_set_button (TILESET_SUB_GROUP_ID, ALL_OTHER_GUI_ELEMENTS, &current_block_profile, ICON_SIZE*6, window_height+ICON_SIZE*2, first_icon, BLOCK_PROFILE_START+10, LMB, 10);
		SIMPGUI_create_radio_set_button (TILESET_SUB_GROUP_ID, ALL_OTHER_GUI_ELEMENTS, &current_block_profile, ICON_SIZE*7, window_height+ICON_SIZE*2, first_icon, BLOCK_PROFILE_START+11, LMB, 11);

		SIMPGUI_create_radio_set_button (TILESET_SUB_GROUP_ID, ALL_OTHER_GUI_ELEMENTS, &current_block_profile, (ICON_SIZE*8), window_height+ICON_SIZE*3, first_icon, BLOCK_PROFILE_START+8, LMB, 8);
		SIMPGUI_create_radio_set_button (TILESET_SUB_GROUP_ID, ALL_OTHER_GUI_ELEMENTS, &current_block_profile, (ICON_SIZE*9), window_height+ICON_SIZE*3, first_icon, BLOCK_PROFILE_START+9, LMB, 9);
		SIMPGUI_create_radio_set_button (TILESET_SUB_GROUP_ID, ALL_OTHER_GUI_ELEMENTS, &current_block_profile, (ICON_SIZE*8), window_height+ICON_SIZE*2, first_icon, BLOCK_PROFILE_START+12, LMB, 12);
		SIMPGUI_create_radio_set_button (TILESET_SUB_GROUP_ID, ALL_OTHER_GUI_ELEMENTS, &current_block_profile, (ICON_SIZE*9), window_height+ICON_SIZE*2, first_icon, BLOCK_PROFILE_START+13, LMB, 13);

		SIMPGUI_create_radio_set_button (TILESET_SUB_GROUP_ID, ALL_OTHER_GUI_ELEMENTS, &current_block_profile, ICON_SIZE*10, window_height+ICON_SIZE*3, first_icon, BLOCK_PROFILE_START+14, LMB, 14);
		SIMPGUI_create_radio_set_button (TILESET_SUB_GROUP_ID, ALL_OTHER_GUI_ELEMENTS, &current_block_profile, ICON_SIZE*10, window_height+ICON_SIZE*2, first_icon, BLOCK_PROFILE_START+15, LMB, 15);
		SIMPGUI_create_radio_set_button (TILESET_SUB_GROUP_ID, ALL_OTHER_GUI_ELEMENTS, &current_block_profile, ICON_SIZE*11, window_height+ICON_SIZE*3, first_icon, BLOCK_PROFILE_START+18, LMB, 18);
		SIMPGUI_create_radio_set_button (TILESET_SUB_GROUP_ID, ALL_OTHER_GUI_ELEMENTS, &current_block_profile, ICON_SIZE*11, window_height+ICON_SIZE*2, first_icon, BLOCK_PROFILE_START+19, LMB, 19);

		SIMPGUI_create_radio_set_button (TILESET_SUB_GROUP_ID, ALL_OTHER_GUI_ELEMENTS, &current_block_profile, (ICON_SIZE*12), window_height+ICON_SIZE*3, first_icon, BLOCK_PROFILE_START+16, LMB, 16);
		SIMPGUI_create_radio_set_button (TILESET_SUB_GROUP_ID, ALL_OTHER_GUI_ELEMENTS, &current_block_profile, (ICON_SIZE*12), window_height+ICON_SIZE*2, first_icon, BLOCK_PROFILE_START+17, LMB, 17);
		SIMPGUI_create_radio_set_button (TILESET_SUB_GROUP_ID, ALL_OTHER_GUI_ELEMENTS, &current_block_profile, (ICON_SIZE*13), window_height+ICON_SIZE*3, first_icon, BLOCK_PROFILE_START+20, LMB, 20);
		SIMPGUI_create_radio_set_button (TILESET_SUB_GROUP_ID, ALL_OTHER_GUI_ELEMENTS, &current_block_profile, (ICON_SIZE*13), window_height+ICON_SIZE*2, first_icon, BLOCK_PROFILE_START+21, LMB, 21);

		SIMPGUI_create_radio_set_button (TILESET_SUB_GROUP_ID, ALL_OTHER_GUI_ELEMENTS, &current_block_profile, ICON_SIZE*14, window_height+ICON_SIZE*3, first_icon, BLOCK_PROFILE_START+22, LMB, 22);
		SIMPGUI_create_radio_set_button (TILESET_SUB_GROUP_ID, ALL_OTHER_GUI_ELEMENTS, &current_block_profile, ICON_SIZE*14, window_height+ICON_SIZE*2, first_icon, BLOCK_PROFILE_START+24, LMB, 24);
		SIMPGUI_create_radio_set_button (TILESET_SUB_GROUP_ID, ALL_OTHER_GUI_ELEMENTS, &current_block_profile, ICON_SIZE*15, window_height+ICON_SIZE*3, first_icon, BLOCK_PROFILE_START+25, LMB, 25);
		SIMPGUI_create_radio_set_button (TILESET_SUB_GROUP_ID, ALL_OTHER_GUI_ELEMENTS, &current_block_profile, ICON_SIZE*15, window_height+ICON_SIZE*2, first_icon, BLOCK_PROFILE_START+23, LMB, 23);

		SIMPGUI_create_radio_set_button (TILESET_SUB_GROUP_ID, ALL_OTHER_GUI_ELEMENTS, &current_block_profile, ICON_SIZE*16, window_height+ICON_SIZE*3, first_icon, BLOCK_PROFILE_START+28, LMB, 28);
		SIMPGUI_create_radio_set_button (TILESET_SUB_GROUP_ID, ALL_OTHER_GUI_ELEMENTS, &current_block_profile, ICON_SIZE*16, window_height+ICON_SIZE*2, first_icon, BLOCK_PROFILE_START+26, LMB, 26);
		SIMPGUI_create_radio_set_button (TILESET_SUB_GROUP_ID, ALL_OTHER_GUI_ELEMENTS, &current_block_profile, ICON_SIZE*17, window_height+ICON_SIZE*3, first_icon, BLOCK_PROFILE_START+29, LMB, 29);
		SIMPGUI_create_radio_set_button (TILESET_SUB_GROUP_ID, ALL_OTHER_GUI_ELEMENTS, &current_block_profile, ICON_SIZE*17, window_height+ICON_SIZE*2, first_icon, BLOCK_PROFILE_START+27, LMB, 27);

		SIMPGUI_create_radio_set_button (TILESET_SUB_GROUP_ID, ALL_OTHER_GUI_ELEMENTS, &current_block_profile, ICON_SIZE*18, window_height+ICON_SIZE*3, first_icon, BLOCK_PROFILE_START+32, LMB, 32);
		SIMPGUI_create_radio_set_button (TILESET_SUB_GROUP_ID, ALL_OTHER_GUI_ELEMENTS, &current_block_profile, ICON_SIZE*18, window_height+ICON_SIZE*2, first_icon, BLOCK_PROFILE_START+30, LMB, 30);
		SIMPGUI_create_radio_set_button (TILESET_SUB_GROUP_ID, ALL_OTHER_GUI_ELEMENTS, &current_block_profile, ICON_SIZE*19, window_height+ICON_SIZE*3, first_icon, BLOCK_PROFILE_START+33, LMB, 33);
		SIMPGUI_create_radio_set_button (TILESET_SUB_GROUP_ID, ALL_OTHER_GUI_ELEMENTS, &current_block_profile, ICON_SIZE*19, window_height+ICON_SIZE*2, first_icon, BLOCK_PROFILE_START+31, LMB, 31);

	}
	else if (state == STATE_RESET_BUTTONS)
	{

	}

	return override_main_editor;
}



void TILESETS_invert_tile_solidity (void)
{
	// This inverts the mask of the tile solidity so that we can exit from the collision routines with a
	// simple "if (a && b)" instead of "if !(a && b)".

	int tileset_number;
	int tile_number;

	for (tileset_number=0; tileset_number<number_of_tilesets_loaded; tileset_number++)
	{
		for (tile_number=0; tile_number<ts[tileset_number].tile_count; tile_number++)
		{
			ts[tileset_number].tileset_pointer[tile_number].solid_sides = 255 - ts[tileset_number].tileset_pointer[tile_number].solid_sides;
		}
	}
}



bool TILESETS_edit_tile_solidity (int state , int *current_cursor ,  int tileset_number, int selected_tile, int mx, int my)
{
	static bool override_main_editor;

	static int current_solidity;

	if (state == STATE_INITIALISE)
	{
		override_main_editor = false;
		current_solidity = 0;

	}
	else if (state == STATE_SHUTDOWN)
	{
		override_main_editor = false;
		current_solidity = 0;
	}
	else if (state == STATE_RUNNING)
	{
		// Input
		if ( (mx<window_width) && (my<window_height) )
		{
			if (selected_tile != UNSET)
			{
				// Left-Mouse Hit! The writing stuff!

				if (CONTROL_mouse_down (LMB) == true)
				{
					ts[tileset_number].tileset_pointer[selected_tile].solid_sides = current_solidity;
				}

				// Right-Mouse Hit! The reading stuff!

				if (CONTROL_mouse_down (RMB) == true)
				{
					current_solidity = ts[tileset_number].tileset_pointer[selected_tile].solid_sides;
				}
			}
		}

		if (CONTROL_mouse_hit (LMB) == true)
		{
			if ( ( mx>(ICON_SIZE*4)+(ICON_SIZE/2) ) && ( mx<(ICON_SIZE*5)+(ICON_SIZE/2) ) && ( my > window_height+(ICON_SIZE*3) ) && ( my < window_height+(ICON_SIZE*4) ) )
			{
				if (current_solidity == 0)
				{
					current_solidity = 255;
				}
				else
				{
					current_solidity = 0;
				}
			}
		}

		// Output
		OUTPUT_boxed_centred_text (320,window_height+ICON_SIZE+12,"OBJECT INTERACTION SELECTION MODE");
		
		OUTPUT_draw_sprite_scale_no_pivot ( object_sides_gfx , current_solidity , (ICON_SIZE*1)-(ICON_SIZE/2) , window_height+(ICON_SIZE*3)-(ICON_SIZE/2) , 2 );

		OUTPUT_draw_sprite ( first_icon , OBJECT_INTERACTION_SIDE_START+4 , (ICON_SIZE*4)+(ICON_SIZE/2) , window_height+(ICON_SIZE*3) );

		SIMPGUI_draw_all_buttons ();

	}
	else if (state == STATE_SET_UP_BUTTONS)
	{
		SIMPGUI_create_binary_toggle_button (TILESET_SUB_GROUP_ID, ALL_OTHER_GUI_ELEMENTS, &current_solidity, (ICON_SIZE*3)+(ICON_SIZE/2), window_height+(ICON_SIZE*2), first_icon, OBJECT_INTERACTION_SIDE_START+0, LMB, 128);
		SIMPGUI_create_binary_toggle_button (TILESET_SUB_GROUP_ID, ALL_OTHER_GUI_ELEMENTS, &current_solidity, (ICON_SIZE*4)+(ICON_SIZE/2), window_height+(ICON_SIZE*2), first_icon, OBJECT_INTERACTION_SIDE_START+1, LMB, 1);
		SIMPGUI_create_binary_toggle_button (TILESET_SUB_GROUP_ID, ALL_OTHER_GUI_ELEMENTS, &current_solidity, (ICON_SIZE*5)+(ICON_SIZE/2), window_height+(ICON_SIZE*2), first_icon, OBJECT_INTERACTION_SIDE_START+2, LMB, 2);
		SIMPGUI_create_binary_toggle_button (TILESET_SUB_GROUP_ID, ALL_OTHER_GUI_ELEMENTS, &current_solidity, (ICON_SIZE*3)+(ICON_SIZE/2), window_height+(ICON_SIZE*3), first_icon, OBJECT_INTERACTION_SIDE_START+3, LMB, 64);
		SIMPGUI_create_binary_toggle_button (TILESET_SUB_GROUP_ID, ALL_OTHER_GUI_ELEMENTS, &current_solidity, (ICON_SIZE*5)+(ICON_SIZE/2), window_height+(ICON_SIZE*3), first_icon, OBJECT_INTERACTION_SIDE_START+5, LMB, 4);
		SIMPGUI_create_binary_toggle_button (TILESET_SUB_GROUP_ID, ALL_OTHER_GUI_ELEMENTS, &current_solidity, (ICON_SIZE*3)+(ICON_SIZE/2), window_height+(ICON_SIZE*4), first_icon, OBJECT_INTERACTION_SIDE_START+6, LMB, 32);
		SIMPGUI_create_binary_toggle_button (TILESET_SUB_GROUP_ID, ALL_OTHER_GUI_ELEMENTS, &current_solidity, (ICON_SIZE*4)+(ICON_SIZE/2), window_height+(ICON_SIZE*4), first_icon, OBJECT_INTERACTION_SIDE_START+7, LMB, 16);
		SIMPGUI_create_binary_toggle_button (TILESET_SUB_GROUP_ID, ALL_OTHER_GUI_ELEMENTS, &current_solidity, (ICON_SIZE*5)+(ICON_SIZE/2), window_height+(ICON_SIZE*4), first_icon, OBJECT_INTERACTION_SIDE_START+8, LMB, 8);
	}
	else if (state == STATE_RESET_BUTTONS)
	{

	}

	return override_main_editor;
}



bool TILESETS_edit_tile_vulnerability (int state , int *current_cursor ,  int tileset_number, int selected_tile, int mx, int my)
{
	static bool override_main_editor;

	char sides[4][NAME_SIZE] = {"TOP","RIGHT","BOTTOM","LEFT"};
	int t,i;
	int temp;
	char text[TEXT_LINE_SIZE];
	int box_height = 12;
	int box_width = 24;
	int top_of_table = window_height+ICON_SIZE*2;
	int left_of_table = 96;
	int vulnerability_rows;
	static int fragile;
	static int bar_range;
	static int use_only_top;
	static bool setting_on;

	static int current_energy;
	static int current_vulnerability[4];

	if (state == STATE_INITIALISE)
	{
		override_main_editor = false;
		for (t=0; t<4; t++)
		{
			current_vulnerability[t]=0;
		}
		current_energy = 1;
		bar_range = 50;
		use_only_top = 0;
		fragile = 1;
		setting_on = true;
	}
	else if (state == STATE_SHUTDOWN)
	{
		override_main_editor = false;
	}
	else if (state == STATE_RUNNING)
	{
		
		if (use_only_top == 0)
		{
			vulnerability_rows = 4;
		}
		else
		{
			vulnerability_rows = 1;
		}

		// Input

		if ( (mx<window_width) && (my<window_height) )
		{
			if (selected_tile != UNSET)
			{
				// Left-Mouse Hit! The writing stuff!

				if (CONTROL_mouse_down (LMB) == true)
				{

					
					if (fragile == 0)
					{
						ts[tileset_number].tileset_pointer[selected_tile].vulnerability_flag = false;

						for (t=0; t<vulnerability_rows; t++)
						{
							ts[tileset_number].tileset_pointer[selected_tile].vulnerabilities[t] = 0;
						}

						ts[tileset_number].tileset_pointer[selected_tile].damage = 0;

						ts[tileset_number].tileset_pointer[selected_tile].use_only_top_vulnerability = false;
					}
					else
					{
						ts[tileset_number].tileset_pointer[selected_tile].vulnerability_flag = true;

						for (t=0; t<vulnerability_rows; t++)
						{
							ts[tileset_number].tileset_pointer[selected_tile].vulnerabilities[t] = current_vulnerability[t];
						}

						ts[tileset_number].tileset_pointer[selected_tile].damage = current_energy;

						if (use_only_top == 0)
						{
							ts[tileset_number].tileset_pointer[selected_tile].use_only_top_vulnerability = false;
						}
						else
						{
							ts[tileset_number].tileset_pointer[selected_tile].use_only_top_vulnerability = true;
						}
					}
				}

				// Right-Mouse Hit! The reading stuff!

				if (CONTROL_mouse_down (RMB) == true)
				{
					for (t=0; t<4; t++)
					{
						current_vulnerability[t] = ts[tileset_number].tileset_pointer[selected_tile].vulnerabilities[t];
					}

					current_energy = ts[tileset_number].tileset_pointer[selected_tile].damage;

					if (ts[tileset_number].tileset_pointer[selected_tile].use_only_top_vulnerability == false)
					{
						use_only_top = 0;
					}
					else
					{
						use_only_top = 1;
					}

					if (ts[tileset_number].tileset_pointer[selected_tile].vulnerability_flag == false)
					{
						fragile = 0;
					}
					else
					{
						fragile = 1;
					}
				}
			}
		}

		// Check for toggle vulnerability

		if (fragile == 1)
		{
			if (CONTROL_mouse_hit (LMB) == true)
			{
				for (t=0; t<vulnerability_rows; t++)
				{
					for (i=0 ; i<16; i++)
					{
						if ( MATH_rectangle_to_point_intersect ( left_of_table + (i*box_width) , top_of_table + (t*box_height) - 1 , left_of_table + (i*box_width) + box_width-1 , top_of_table + (t*box_height) + box_height-1 , mx , my ) )
						{
							if ( (current_vulnerability[t] & MATH_pow(i)) > 0 )
							{
								setting_on = false;
							}
							else
							{
								setting_on = true;
							}
						}
					}
				}
			}

			if (CONTROL_mouse_down (LMB) == true)
			{
				for (t=0; t<vulnerability_rows; t++)
				{
					for (i=0 ; i<16; i++)
					{
						if ( MATH_rectangle_to_point_intersect ( left_of_table + (i*box_width) , top_of_table + (t*box_height) - 1 , left_of_table + (i*box_width) + box_width-1 , top_of_table + (t*box_height) + box_height-1 , mx , my ) )
						{
							if ( setting_on == true )
							{
								current_vulnerability[t] |= MATH_pow(i);
							}
							else
							{
								current_vulnerability[t] &= ~MATH_pow(i);
							}
						}
					}
				}
			}

		}

		// Output
		OUTPUT_boxed_centred_text (320,window_height+ICON_SIZE+12,"ENERGY AND WEAKNESS SELECTION MODE");

		if (fragile == 1)
		{
			for (t=0; t<vulnerability_rows; t++)
			{
				OUTPUT_right_aligned_text (64 , top_of_table + (t*box_height) + ((box_height-FONT_HEIGHT)/2) , &sides[t][0]);

				for (i=0 ; i<16; i++)
				{
					sprintf(text,"%d",i);
					temp = ( current_vulnerability[t] & MATH_pow(i) );
					if (temp)
					{
						temp = 255;
					}
					OUTPUT_centred_text ( left_of_table + (i*box_width) + (box_width/2) , top_of_table + (t*box_height) + ((box_height-FONT_HEIGHT)/2) , text , 255 , temp , temp );
					OUTPUT_rectangle( left_of_table + (i*box_width) , top_of_table + (t*box_height) , left_of_table + (i*box_width) + box_width-1 , top_of_table + (t*box_height) + box_height-1 , 0 , 0 , 255);
				}
			}

			sprintf (text,"ENERGY = %i",current_energy);
			OUTPUT_text (left_of_table , top_of_table + (box_height*5) , text);

			sprintf (text,"0-%d",bar_range);
			OUTPUT_right_aligned_text (64 , top_of_table + (box_height*6) , text);
		}

		SIMPGUI_kill_button (TILESET_SUB_GROUP_ID, ALL_OTHER_GUI_ELEMENTS);

		SIMPGUI_create_toggle_button ( TILESET_SUB_GROUP_ID, ALL_OTHER_GUI_ELEMENTS, &fragile, left_of_table + (20*box_width), top_of_table, first_icon, FRAGILE_PANEL_ICON, LMB);
		if (fragile == 1)
		{
			SIMPGUI_create_toggle_button ( TILESET_SUB_GROUP_ID, ALL_OTHER_GUI_ELEMENTS, &use_only_top, left_of_table + (20*box_width), top_of_table + 2*ICON_SIZE, first_icon, ONLY_USE_TOP_ICON, LMB);
			SIMPGUI_create_v_ramp_button ( TILESET_SUB_GROUP_ID, ALL_OTHER_GUI_ELEMENTS, &bar_range, left_of_table + box_width * 17, top_of_table , box_width * 2 , box_height*8 , 50, 500, 50, LMB , true , true );
			SIMPGUI_create_h_ramp_button ( TILESET_SUB_GROUP_ID, ALL_OTHER_GUI_ELEMENTS, &current_energy, left_of_table, top_of_table + (box_height*6) , box_width * 16 , box_height*2 , 0, bar_range, bar_range/50, LMB , true , true );
		}
		SIMPGUI_set_overlay_group (TILESET_SUB_GROUP_ID , CROSS_OVERLAY);

		SIMPGUI_draw_all_buttons ();

	}
	else if (state == STATE_SET_UP_BUTTONS)
	{
	}
	else if (state == STATE_RESET_BUTTONS)
	{

	}

	return override_main_editor;
}



bool TILESETS_edit_tile_next_of_kin (int state , int *current_cursor ,  int tileset_number, int selected_tile, int mx, int my)
{
	static bool override_main_editor;
	static int from_tile;
	static int to_tile;
	static bool grabbing = false;

	if (current_cursor != NULL)
	{
		*current_cursor = FROM_BLOCK_ARROW;
	}
	
	if (state == STATE_INITIALISE)
	{
		override_main_editor = false;

	}
	else if (state == STATE_SHUTDOWN)
	{
		override_main_editor = false;


	}
	else if (state == STATE_RUNNING)
	{
		// Input
		if ( (mx<window_width) && (my<window_height) )
		{
			if (selected_tile != UNSET)
			{
				// Left-Mouse Hit! The writing stuff!

				if (CONTROL_mouse_hit (LMB) == true)
				{
					from_tile = selected_tile;
					grabbing = true;
				}

				if ( (CONTROL_mouse_down (LMB) == true) && (grabbing == true) )
				{
					*current_cursor = TO_BLOCK_ARROW;
				}

				if (grabbing == true)
				{
					if (CONTROL_mouse_down (LMB) == true)
					{
						to_tile = selected_tile;
						ts[tileset_number].tileset_pointer[from_tile].next_of_kin = to_tile;
					}
					if (CONTROL_mouse_down (LMB) == false)
					{
						grabbing = false;
					}
				}

				// Right-Mouse Hit! The blanking stuff!

				if (CONTROL_mouse_hit (RMB) == true)
				{
					ts[tileset_number].tileset_pointer[selected_tile].next_of_kin = UNSET;
				}

			}
		}

		// Output
		OUTPUT_boxed_centred_text (320,window_height+ICON_SIZE+12,"NEXT OF KIN CHOOSING MODE");

	}
	else if (state == STATE_SET_UP_BUTTONS)
	{

	}
	else if (state == STATE_RESET_BUTTONS)
	{

	}

	return override_main_editor;
}



void TILESETS_create_group (int tileset_number, int start_block, int end_block)
{
	int tiles_per_row;
	int tiles_per_col;

	int start_bx,start_by,end_bx,end_by;
	int bx,by;
	int temp_bx,temp_by;
	int tile_number;
	int temp_counter;

	tiles_per_row = OUTPUT_bitmap_width (ts[tileset_number].tileset_image_index) / ts[tileset_number].tilesize;
	tiles_per_col = OUTPUT_bitmap_height (ts[tileset_number].tileset_image_index) / ts[tileset_number].tilesize;

	start_bx = start_block % tiles_per_row;
	start_by = start_block / tiles_per_row;

	end_bx = end_block % tiles_per_row;
	end_by = end_block / tiles_per_row;

	if (start_block < end_block)
	{
		temp_counter = start_block;
		start_block = end_block;
		end_block = temp_counter;
	}

	// Flag all the blocks in the group...

	for (bx = start_bx; bx<=end_bx; bx++)
	{
		for (by = start_by; by<=end_by; by++)
		{
			tile_number = (by * tiles_per_row) + bx;
			ts[tileset_number].tileset_pointer[tile_number].temp_data = 1;
		}
	}

	// Then go through them and assign them a number based on how many neighbours they have.

	for (bx = start_bx; bx<=end_bx; bx++)
	{
		for (by = start_by; by<=end_by; by++)
		{
			temp_counter = 0;

			if (by > 0)
			{
				temp_bx = bx;
				temp_by = by-1;
				tile_number = (temp_by * tiles_per_row) + temp_bx;
				temp_counter += 1 * ts[tileset_number].tileset_pointer[tile_number].temp_data;
			}

			if (bx < tiles_per_row-1)
			{
				temp_bx = bx+1;
				temp_by = by;
				tile_number = (temp_by * tiles_per_row) + temp_bx;
				temp_counter += 2 * ts[tileset_number].tileset_pointer[tile_number].temp_data;
			}

			if (by < tiles_per_col-1)
			{
				temp_bx = bx;
				temp_by = by+1;
				tile_number = (temp_by * tiles_per_row) + temp_bx;
				temp_counter += 4 * ts[tileset_number].tileset_pointer[tile_number].temp_data;
			}

			if (bx > 0)
			{
				temp_bx = bx-1;
				temp_by = by;
				tile_number = (temp_by * tiles_per_row) + temp_bx;
				temp_counter += 8 * ts[tileset_number].tileset_pointer[tile_number].temp_data;
			}

			tile_number = (by * tiles_per_row) + bx;

			ts[tileset_number].tileset_pointer[tile_number].neighbouring_blocks = temp_counter;
			
		}
	}

	// Clear all the block's temp data.

	for (bx = start_bx; bx<=end_bx; bx++)
	{
		for (by = start_by; by<=end_by; by++)
		{
			tile_number = (by * tiles_per_row) + bx;
			ts[tileset_number].tileset_pointer[tile_number].temp_data = 0;
		}
	}
}



void TILESETS_clear_group (int tileset_number, int tile_number)
{
	int tiles_per_row;
	int tiles_per_col;

	int start_bx,start_by,end_bx,end_by;
	int bx,by;

	tiles_per_row = OUTPUT_bitmap_width (ts[tileset_number].tileset_image_index) / ts[tileset_number].tilesize;
	tiles_per_col = OUTPUT_bitmap_height (ts[tileset_number].tileset_image_index) / ts[tileset_number].tilesize;

	start_bx = tile_number % tiles_per_row;
	start_by = tile_number / tiles_per_row;

	end_bx = start_bx;
	end_by = start_by;

	// Now expand start and end by seeing if there's a block above them.

	while ( (ts[tileset_number].tileset_pointer[(start_by * tiles_per_row) + start_bx].neighbouring_blocks & 1) > 0)
	{
		start_by--;
	}

	while ( (ts[tileset_number].tileset_pointer[(end_by * tiles_per_row) + end_bx].neighbouring_blocks & 2) > 0)
	{
		end_bx++;
	}

	while ( (ts[tileset_number].tileset_pointer[(end_by * tiles_per_row) + end_bx].neighbouring_blocks & 4) > 0)
	{
		end_by++;
	}

	while ( (ts[tileset_number].tileset_pointer[(start_by * tiles_per_row) + start_bx].neighbouring_blocks & 8) > 0)
	{
		start_bx--;
	}

	for (bx=start_bx ; bx<=end_bx; bx++)
	{
		for (by=start_by ; by<=end_by; by++)
		{
			tile_number = (by * tiles_per_row) + bx;
			ts[tileset_number].tileset_pointer[tile_number].neighbouring_blocks = 0;
		}
	}

}



void TILESETS_clear_groups (int tileset_number, int start_block, int end_block)
{
	int tiles_per_row;
	int tiles_per_col;

	int start_bx,start_by,end_bx,end_by;
	int bx,by;
	int tile_number;
	int temp_counter;

	tiles_per_row = OUTPUT_bitmap_width (ts[tileset_number].tileset_image_index) / ts[tileset_number].tilesize;
	tiles_per_col = OUTPUT_bitmap_height (ts[tileset_number].tileset_image_index) / ts[tileset_number].tilesize;

	start_bx = start_block % tiles_per_row;
	start_by = start_block / tiles_per_row;

	end_bx = end_block % tiles_per_row;
	end_by = end_block / tiles_per_row;

	if (start_block < end_block)
	{
		temp_counter = start_block;
		start_block = end_block;
		end_block = temp_counter;
	}

	// Clear all the groups that could be around here, the pesky scamps.

	for (bx = start_bx; bx<=end_bx; bx++)
	{
		for (by = start_by; by<=end_by; by++)
		{
			tile_number = (by * tiles_per_row) + bx;
			TILESETS_clear_group (tileset_number, tile_number);
		}
	}
}



char TILESETS_read_group_tile (int tileset_number, int tx, int ty)
{
	int tile_number;
	int set_width = OUTPUT_bitmap_width ( ts[tileset_number].tileset_image_index ) / ts[tileset_number].tilesize;
	int set_height = OUTPUT_bitmap_height ( ts[tileset_number].tileset_image_index ) / ts[tileset_number].tilesize;

	if ( (tx >= 0) && (tx < set_width) && (ty >= 0) && (ty < set_height) )
	{
		tile_number = (ty * set_width) + tx;
		return ts[tileset_number].tileset_pointer[tile_number].neighbouring_blocks;
	}
	else
	{
		return UNSET;
	}
}



void TILESETS_write_group_tile (int tileset_number, int tx, int ty, char tile_value)
{
	int tile_number;
	int set_width = OUTPUT_bitmap_width ( ts[tileset_number].tileset_image_index ) / ts[tileset_number].tilesize;
	int set_height = OUTPUT_bitmap_height ( ts[tileset_number].tileset_image_index ) / ts[tileset_number].tilesize;

	if ( (tx >= 0) && (tx < set_width) && (ty >= 0) && (ty < set_height) )
	{
		tile_number = (ty * set_width) + tx;
		ts[tileset_number].tileset_pointer[tile_number].neighbouring_blocks = tile_value;
	}
}



char TILESETS_read_family_tile (int tileset_number, int tx, int ty)
{
	int tile_number;
	int set_width = OUTPUT_bitmap_width ( ts[tileset_number].tileset_image_index ) / ts[tileset_number].tilesize;
	int set_height = OUTPUT_bitmap_height ( ts[tileset_number].tileset_image_index ) / ts[tileset_number].tilesize;

	if ( (tx >= 0) && (tx < set_width) && (ty >= 0) && (ty < set_height) )
	{
		tile_number = (ty * set_width) + tx;
		return ts[tileset_number].tileset_pointer[tile_number].family_neighbours;
	}
	else
	{
		return UNSET;
	}
}



void TILESETS_write_family_tile (int tileset_number, int tx, int ty, char tile_value)
{
	int tile_number;
	int set_width = OUTPUT_bitmap_width ( ts[tileset_number].tileset_image_index ) / ts[tileset_number].tilesize;
	int set_height = OUTPUT_bitmap_height ( ts[tileset_number].tileset_image_index ) / ts[tileset_number].tilesize;

	if ( (tx >= 0) && (tx < set_width) && (ty >= 0) && (ty < set_height) )
	{
		tile_number = (ty * set_width) + tx;
		ts[tileset_number].tileset_pointer[tile_number].family_neighbours = tile_value;
	}
}



int TILESETS_read_family_id (int tileset_number, int tx, int ty)
{
	int tile_number;
	int set_width = OUTPUT_bitmap_width ( ts[tileset_number].tileset_image_index ) / ts[tileset_number].tilesize;
	int set_height = OUTPUT_bitmap_height ( ts[tileset_number].tileset_image_index ) / ts[tileset_number].tilesize;

	if ( (tx >= 0) && (tx < set_width) && (ty >= 0) && (ty < set_height) )
	{
		tile_number = (ty * set_width) + tx;
		return ts[tileset_number].tileset_pointer[tile_number].family_id;
	}
	else
	{
		return UNSET;
	}
}



void TILESETS_write_family_id (int tileset_number, int tx, int ty, int family_id)
{
	int tile_number;
	int set_width = OUTPUT_bitmap_width ( ts[tileset_number].tileset_image_index ) / ts[tileset_number].tilesize;
	int set_height = OUTPUT_bitmap_height ( ts[tileset_number].tileset_image_index ) / ts[tileset_number].tilesize;

	if ( (tx >= 0) && (tx < set_width) && (ty >= 0) && (ty < set_height) )
	{
		tile_number = (ty * set_width) + tx;
		ts[tileset_number].tileset_pointer[tile_number].family_id = family_id;
	}
}



void TILESETS_create_bitflag_data (int tileset_number, bool group_not_family)
{
	// This goes through the tileset array looking for GROUP_CONVERSION_TILE_NUMBER number tiles and
	// creates a list entry for it and then burns all those list entries into the tileset.
	
	fill_spreader *tile_list = NULL;
	int list_size;

	int x,y;
	int side;
	
	int x_offset,y_offset;

	int current_tile_value;
	int tile_value_counter;

	int t;

	int map_width = OUTPUT_bitmap_width ( ts[tileset_number].tileset_image_index ) / ts[tileset_number].tilesize;
	int map_height = OUTPUT_bitmap_height ( ts[tileset_number].tileset_image_index ) / ts[tileset_number].tilesize;

	list_size = 0;

	for (y=0; (y<map_height) ; y++)
	{
		for (x=0; (x<map_width) ; x++)
		{
			if (group_not_family == true)
			{
				current_tile_value = TILESETS_read_group_tile (tileset_number, x, y);
			}
			else
			{
				current_tile_value = TILESETS_read_family_tile (tileset_number, x, y);
			}

			if ( current_tile_value == GROUP_CONVERSION_TILE_NUMBER)
			{
				tile_value_counter = 0;

				for (side=0; side<4; side++)
				{
					if (MATH_check_4bit_within_size (side, x, y, map_width, map_height, &x_offset, &y_offset) == true)
					{
						
						if (group_not_family == true)
						{
							if (TILESETS_read_group_tile (tileset_number,  x+x_offset, y+y_offset) == GROUP_CONVERSION_TILE_NUMBER)
							{
								tile_value_counter += MATH_pow(side);
							}
						}
						else
						{
							if (TILESETS_read_family_tile (tileset_number,  x+x_offset, y+y_offset) == GROUP_CONVERSION_TILE_NUMBER)
							{
								tile_value_counter += MATH_pow(side);
							}
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
		if (group_not_family == true)
		{
			TILESETS_write_group_tile (tileset_number, tile_list[t].x, tile_list[t].y, tile_list[t].value);
		}
		else
		{
			TILESETS_write_family_tile (tileset_number, tile_list[t].x, tile_list[t].y, tile_list[t].value);
		}
	}

	free(tile_list);
	tile_list = NULL; // Anal, I know...
}



void TILESETS_create_group (int tileset_number)
{
	TILESETS_create_bitflag_data (tileset_number, true);
}



void TILESETS_alter_group (int tileset_number, int tx, int ty, bool reset, bool group_not_family)
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

	int current_tile_value;

	int map_width;
	int map_height;
	
	map_width = OUTPUT_bitmap_width ( ts[tileset_number].tileset_image_index ) / ts[tileset_number].tilesize;
	map_height = OUTPUT_bitmap_height ( ts[tileset_number].tileset_image_index ) / ts[tileset_number].tilesize;

	list_size = 0;
	read_from = 0;

	if (group_not_family == true)
	{
		current_tile_value = TILESETS_read_group_tile (tileset_number, tx, ty);
		TILESETS_write_group_tile (tileset_number,  tx, ty, replacement_value);
	}
	else
	{
		current_tile_value = TILESETS_read_family_tile (tileset_number, tx, ty);
		TILESETS_write_family_tile (tileset_number,  tx, ty, replacement_value);
	}

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

					if (group_not_family == true)
					{
						spreading_to_value = TILESETS_read_group_tile (tileset_number,  x+x_offset, y+y_offset);
						TILESETS_write_group_tile (tileset_number,  x+x_offset, y+y_offset, replacement_value);
					}
					else
					{
						spreading_to_value = TILESETS_read_family_tile (tileset_number,  x+x_offset, y+y_offset);
						TILESETS_write_family_tile (tileset_number,  x+x_offset, y+y_offset, replacement_value);
					}
					
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



void TILESETS_absorb_group (int tileset_number, int tx, int ty)
{
	TILESETS_alter_group (tileset_number, tx, ty, false, true);
}



void TILESETS_reset_group (int tileset_number, int tx, int ty)
{
	TILESETS_alter_group (tileset_number, tx, ty, true, true);
}



bool TILESETS_edit_tile_grouping (int state , int *current_cursor , int tileset_number, int selected_tile, int mx, int my)
{
	static bool override_main_editor;

	int real_tx,real_ty;
	int width_in_tiles;
	int tile_value;

	static bool creating_group;

	if (current_cursor != NULL)
	{
		*current_cursor = SELECTION_ARROW;
	}

	if (state == STATE_INITIALISE)
	{
		override_main_editor = false;
		creating_group = false;

	}
	else if (state == STATE_SHUTDOWN)
	{
		override_main_editor = false;
	}
	else if (state == STATE_RUNNING)
	{
		width_in_tiles = OUTPUT_bitmap_width(ts[tileset_number].tileset_image_index) / ts[tileset_number].tilesize;

		real_tx = selected_tile % width_in_tiles;
		real_ty = selected_tile / width_in_tiles;

		// Input
		if ( (mx<window_width) && (my<window_height) )
		{

			if (selected_tile != UNSET)
			{
				tile_value = TILESETS_read_group_tile (tileset_number, real_tx, real_ty);

				if (CONTROL_mouse_down(LMB) == true)
				{
					creating_group = true;
					if ( (tile_value >=0) && (tile_value <16) )
					{
						TILESETS_absorb_group (tileset_number, real_tx, real_ty);
					}
				}
				else if (creating_group == true)
				{
					creating_group = false;
					TILESETS_create_group (tileset_number);
				}

				if (CONTROL_mouse_down(RMB) == true)
				{
					if ( (tile_value >=0) && (tile_value <16) )
					{
						TILESETS_reset_group (tileset_number,  real_tx, real_ty);
					}
				}

			}
		}

		// Output
		OUTPUT_boxed_centred_text (320,window_height+ICON_SIZE+12,"BLOCK GROUPING MODE");

	}
	else if (state == STATE_SET_UP_BUTTONS)
	{

	}
	else if (state == STATE_RESET_BUTTONS)
	{

	}

	return override_main_editor;
}



void TILESETS_absorb_family (int tileset_number, int tx, int ty)
{
	TILESETS_alter_group (tileset_number, tx, ty, false, false);
}



void TILESETS_reset_family (int tileset_number, int tx, int ty)
{
	TILESETS_alter_group (tileset_number, tx, ty, true, false);
}



void TILESETS_create_family (int tileset_number)
{
	TILESETS_create_bitflag_data (tileset_number, false);
}



void TILESETS_create_family_ids (int tileset_number)
{
	// This goes through each family group in the map and assigns it a unique ID.

	fill_spreader *tile_list;
	int list_size;
	int read_from;

	int x,y;
	int side;

	int loop_x,loop_y;

	int family_size;

	int x_offset,y_offset;

	int current_tile_value;
	int spreading_to_value;

	int map_width;
	int map_height;

	int family_id;
	
	map_width = OUTPUT_bitmap_width ( ts[tileset_number].tileset_image_index ) / ts[tileset_number].tilesize;
	map_height = OUTPUT_bitmap_height ( ts[tileset_number].tileset_image_index ) / ts[tileset_number].tilesize;

	// But first it RESETS them!

	for (loop_x=0; loop_x<map_width; loop_x++)
	{
		for (loop_y=0; loop_y<map_height; loop_y++)
		{
			TILESETS_write_family_id (tileset_number,loop_x,loop_y,UNSET);
		}
	}

	family_id = 0;

	for (loop_x=0; loop_x<map_width; loop_x++)
	{
		for (loop_y=0; loop_y<map_height; loop_y++)
		{

			if (TILESETS_read_family_id(tileset_number,loop_x,loop_y) == UNSET)
			{

				family_size = 0;
				list_size = 0;
				read_from = 0;
				tile_list = NULL;

				tile_list = EDIT_add_filler (list_size,tile_list,loop_x,loop_y,TILESETS_read_family_tile(tileset_number,loop_x,loop_y));
				list_size++;
				family_size++;

				TILESETS_write_family_id (tileset_number,  loop_x, loop_y, family_id);

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
								if (TILESETS_read_family_id(tileset_number,x+x_offset, y+y_offset) == UNSET)
								{
									// Only spread if the tile we're spreading to hasn't been assigned a family ID.

									// Then read the bitflag which determines what's connected...
									spreading_to_value = TILESETS_read_family_tile (tileset_number,  x+x_offset, y+y_offset);
									// Write the current ID to the tile we're spreading to.
									TILESETS_write_family_id (tileset_number,  x+x_offset, y+y_offset, family_id);
								
									tile_list = EDIT_add_filler (list_size,tile_list, x+x_offset, y+y_offset, spreading_to_value);
									list_size++;
									family_size++;
								}
							}
						}

					}
					
					read_from++;
				}

				free(tile_list);

				if (family_size == 1)
				{
					// Only 1 member? Aw... Reset to UNSET then!
					TILESETS_write_family_id (tileset_number,loop_x,loop_y,UNSET);
				}
				else
				{
					// Increase the family counter as the current one has been made use of.
					family_id++;
				}
				
			}

		}
	}


}



bool TILESETS_edit_tile_family (int state , int *current_cursor , int tileset_number, int selected_tile, int mx, int my)
{
	static bool override_main_editor;

	int real_tx,real_ty;
	int width_in_tiles;
	int tile_value;

	char word_string[TEXT_LINE_SIZE];

	int family_id;

	static bool creating_group;

	if (current_cursor != NULL)
	{
		*current_cursor = SELECTION_ARROW;
	}

	if (state == STATE_INITIALISE)
	{
		override_main_editor = false;
		creating_group = false;

	}
	else if (state == STATE_SHUTDOWN)
	{
		override_main_editor = false;
	}
	else if (state == STATE_RUNNING)
	{
		width_in_tiles = OUTPUT_bitmap_width(ts[tileset_number].tileset_image_index) / ts[tileset_number].tilesize;

		real_tx = selected_tile % width_in_tiles;
		real_ty = selected_tile / width_in_tiles;

		// Input
		if ( (mx<window_width) && (my<window_height) )
		{
			family_id = ts[tileset_number].tileset_pointer[selected_tile].family_id;

			if (selected_tile != UNSET)
			{
				tile_value = TILESETS_read_family_tile (tileset_number, real_tx, real_ty);

				if (CONTROL_mouse_down(LMB) == true)
				{
					creating_group = true;
					if ( (tile_value >=0) && (tile_value <16) )
					{
						TILESETS_absorb_family (tileset_number, real_tx, real_ty);
					}
				}
				else if (creating_group == true)
				{
					creating_group = false;
					TILESETS_create_family (tileset_number);
					TILESETS_create_family_ids (tileset_number);
				}

				if (CONTROL_mouse_down(RMB) == true)
				{
					if ( (tile_value >=0) && (tile_value <16) )
					{
						TILESETS_reset_family (tileset_number,  real_tx, real_ty);
					}
				}

			}
		}
		else
		{
			family_id = UNSET;
		}

		// Output
		OUTPUT_boxed_centred_text (320,window_height+ICON_SIZE+12,"BLOCK FAMILY MODE");

		if (family_id == UNSET)
		{
			OUTPUT_centred_text (320,window_height+ICON_SIZE*2+12,"NO FAMILY");
		}
		else
		{
			sprintf(word_string,"FAMILY ID = %d",family_id);
			OUTPUT_centred_text (320,window_height+ICON_SIZE*2+12,word_string);
		}

	}
	else if (state == STATE_SET_UP_BUTTONS)
	{

	}
	else if (state == STATE_RESET_BUTTONS)
	{

	}

	return override_main_editor;
}



bool TILESETS_edit_collision_bitmask (int state , int *current_cursor , int tileset_number, int selected_tile, int mx, int my)
{
	static bool override_main_editor;

	static int current_collision_bitmask;

	static bool set_on = false;

	int t;
	int column;

	int box_height = 64;
	int box_width = 64;

	int colours[8][3] = { 0,0,255, 255,0,0, 255,128,0, 255,0,255, 255,255,0, 0,255,0, 0,255,255, 255,255,255};

	int selection = UNSET;

	if (state == STATE_INITIALISE)
	{
		override_main_editor = false;

	}
	else if (state == STATE_SHUTDOWN)
	{
		override_main_editor = false;
		
		current_collision_bitmask = 0;

	}
	else if (state == STATE_RUNNING)
	{
		// Input
		if ( (mx<window_width) && (my<window_height) )
		{
			if (selected_tile != UNSET)
			{
				// Left-Mouse Hit! The writing stuff!

				if ( (CONTROL_mouse_down (LMB) == true) && (CONTROL_key_down(KEY_LSHIFT) == false) && (CONTROL_key_down(KEY_LCONTROL) == false) )
				{
					// If no left-shift then overwrite!
					ts[tileset_number].tileset_pointer[selected_tile].collision_bitmask = (unsigned char) current_collision_bitmask;
				}

				if ( (CONTROL_mouse_down (LMB) == true) && (CONTROL_key_down(KEY_LSHIFT) == true) )
				{
					// If left-shift then OR.
					ts[tileset_number].tileset_pointer[selected_tile].collision_bitmask |= (unsigned char) current_collision_bitmask;
				}

				if ( (CONTROL_mouse_down (LMB) == true) && (CONTROL_key_down(KEY_LCONTROL) == true) )
				{
					// If left-shift then NAND.
					ts[tileset_number].tileset_pointer[selected_tile].collision_bitmask &= (unsigned char) ~current_collision_bitmask;
				}

				// Right-Mouse Hit! The reading stuff!

				if (CONTROL_mouse_down (RMB) == true)
				{
					current_collision_bitmask = ts[tileset_number].tileset_pointer[selected_tile].collision_bitmask;
				}
			}
		}

		// Output
		OUTPUT_boxed_centred_text (320,window_height+ICON_SIZE+12,"TILE COLLISION BITMASK");

		for(t=0; t<8; t++)
		{
			column = t * box_width;

			if (MATH_pow(t) & current_collision_bitmask)
			{
				OUTPUT_filled_rectangle (column , window_height+(ICON_SIZE*2) , column+box_width-1 , window_height+(ICON_SIZE*2)+box_height-1 , colours[t][0],colours[t][1],colours[t][2]);
			}
			else
			{
				OUTPUT_rectangle (column , window_height+(ICON_SIZE*2) , column+box_width-1 , window_height+(ICON_SIZE*2)+box_height-1 , colours[t][0],colours[t][1],colours[t][2]);
			}

			if (MATH_rectangle_to_point_intersect (column , window_height+(ICON_SIZE*2) , column+box_width-1 , window_height+(ICON_SIZE*2)+box_height-1 , mx , my ) == true )
			{
				selection = t;
			}
		}

		if ( (selection != UNSET) && (CONTROL_mouse_hit(LMB) == true) )
		{
			if ((current_collision_bitmask & MATH_pow(selection)) == 0)
			{
				set_on = true;
			}
			else
			{
				set_on = false;
			}
		}

		if ( (selection != UNSET) && (CONTROL_mouse_down(LMB) == true) )
		{
			if (set_on == true)
			{
				current_collision_bitmask |= MATH_pow(selection);
			}
			else
			{
				current_collision_bitmask &= ~MATH_pow(selection);
			}
		}

	}
	else if (state == STATE_SET_UP_BUTTONS)
	{

	}
	else if (state == STATE_RESET_BUTTONS)
	{

	}

	return override_main_editor;
}



bool TILESETS_edit_priority (int state , int *current_cursor , int tileset_number, int selected_tile, int mx, int my)
{
	static bool override_main_editor;

	static int current_priority;

	static bool set_on = false;

	int t;
	int column;

	int box_height = 64;
	int box_width = 64;

	int colours[8][3] = { 128,0,0, 192,0,0, 255,0,0, 255,64,0, 255,128,0, 255,192,0, 255,255,0, 255,255,255};

	if (state == STATE_INITIALISE)
	{
		override_main_editor = false;

	}
	else if (state == STATE_SHUTDOWN)
	{
		override_main_editor = false;
		
		current_priority = 0;

	}
	else if (state == STATE_RUNNING)
	{
		// Input
		if ( (mx<window_width) && (my<window_height) )
		{
			if (selected_tile != UNSET)
			{
				// Left-Mouse Hit! The writing stuff!

				if (CONTROL_mouse_down (LMB) == true)
				{
					// If no left-shift then overwrite!
					ts[tileset_number].tileset_pointer[selected_tile].priority = (unsigned char) current_priority;
				}

				// Right-Mouse Hit! The reading stuff!

				if (CONTROL_mouse_down (RMB) == true)
				{
					current_priority = ts[tileset_number].tileset_pointer[selected_tile].priority;
				}
			}
		}

		// Output
		OUTPUT_boxed_centred_text (320,window_height+ICON_SIZE+12,"TILE PRIORITY");

		for(t=0; t<8; t++)
		{
			column = t * box_width;

			if (t == current_priority)
			{
				OUTPUT_filled_rectangle (column , window_height+(ICON_SIZE*2) , column+box_width-1 , window_height+(ICON_SIZE*2)+box_height-1 , colours[t][0],colours[t][1],colours[t][2]);
			}
			else
			{
				OUTPUT_rectangle (column , window_height+(ICON_SIZE*2) , column+box_width-1 , window_height+(ICON_SIZE*2)+box_height-1 , colours[t][0],colours[t][1],colours[t][2]);
			}

			if (MATH_rectangle_to_point_intersect (column , window_height+(ICON_SIZE*2) , column+box_width-1 , window_height+(ICON_SIZE*2)+box_height-1 , mx , my ) == true )
			{
				if (CONTROL_mouse_hit (LMB) == true)
				{
					current_priority = t;
				}
			}
		}

	}
	else if (state == STATE_SET_UP_BUTTONS)
	{

	}
	else if (state == STATE_RESET_BUTTONS)
	{

	}

	return override_main_editor;
}



bool TILESETS_edit_boolean_tile_properties (int state , int *current_cursor , int tileset_number, int selected_tile, int mx, int my)
{
	static bool override_main_editor;

	static int current_boolean_props;

	static bool set_on = false;

	char boolean_names[24][NAME_SIZE] = {"CONVEYOR","ACCELLERATE","SLIPPERY","CLIMBABLE UP DOWN","CLIMBABLE LEFT RIGHT","CLIMBABLE OMNI","CLIMBABLE WALL UP DOWN","CLIMBABLE MONKEY BARS","STICKY WALL","STICKY FLOOR","DEADLY","KLUDGE UP BLOCK","KLUDGE DOWN BLOCK","DEKLUDGER","TILE PATHFIND IGNORE","ZONE PATHFIND IGNORE","WATER","DEEP_WATER","HARMFUL","FORCE_FIELD","SLOPE_RIGHT","SLOPE_LEFT","UNUSED_4","UNUSED_5" };

	int t;
	int column;
	int row;

	int box_height = 12;
	int box_width = 208;

	int colour;

	int selection = UNSET;

	if (state == STATE_INITIALISE)
	{
		override_main_editor = false;

	}
	else if (state == STATE_SHUTDOWN)
	{
		override_main_editor = false;
		
		current_boolean_props = 0;

	}
	else if (state == STATE_RUNNING)
	{
		// Input
		if ( (mx<window_width) && (my<window_height) )
		{
			if (selected_tile != UNSET)
			{
				// Left-Mouse Hit! The writing stuff!

				if ( (CONTROL_mouse_down (LMB) == true) && (CONTROL_key_down(KEY_LSHIFT) == false) && (CONTROL_key_down(KEY_LCONTROL) == false) )
				{
					// If no left-shift then overwrite!
					ts[tileset_number].tileset_pointer[selected_tile].boolean_properties = current_boolean_props;
				}

				if ( (CONTROL_mouse_down (LMB) == true) && (CONTROL_key_down(KEY_LSHIFT) == true) )
				{
					// If left-shift then OR.
					ts[tileset_number].tileset_pointer[selected_tile].boolean_properties |= current_boolean_props;
				}

				if ( (CONTROL_mouse_down (LMB) == true) && (CONTROL_key_down(KEY_LCONTROL) == true) )
				{
					// If left-shift then NAND.
					ts[tileset_number].tileset_pointer[selected_tile].boolean_properties &= ~current_boolean_props;
				}

				// Right-Mouse Hit! The reading stuff!

				if (CONTROL_mouse_down (RMB) == true)
				{
					current_boolean_props = ts[tileset_number].tileset_pointer[selected_tile].boolean_properties;
				}
			}
		}

		// Output
		OUTPUT_boxed_centred_text (320,window_height+ICON_SIZE+12,"BOOLEAN TILE PROPERTIES");

		for(t=0; t<24; t++)
		{
			column = (t / ((ICON_SIZE*3)/box_height)) * box_width;
			row = t % ((ICON_SIZE*3)/box_height) * box_height;

			if (MATH_pow(t) & current_boolean_props)
			{
				colour = 255;
			}
			else
			{
				colour = 0;
			}

			OUTPUT_centred_text(column+(box_width/2)+8, window_height+(ICON_SIZE*2)+row+((box_height-FONT_HEIGHT)/2)+1, &boolean_names[t][0], 255, colour, colour);
			OUTPUT_rectangle(column+8 , window_height+(ICON_SIZE*2)+row , column+box_width+8-1 , window_height+(ICON_SIZE*2)+row+box_height-1 , 0,0,255);

			if (MATH_rectangle_to_point_intersect (column+8 , window_height+(ICON_SIZE*2)+row , column+box_width+8-1 , window_height+(ICON_SIZE*2)+row+box_height-1 , mx , my ) == true )
			{
				selection = t;
			}
		}

		if ( (selection != UNSET) && (CONTROL_mouse_hit(LMB) == true) )
		{
			if ((current_boolean_props & MATH_pow(selection)) == 0)
			{
				set_on = true;
			}
			else
			{
				set_on = false;
			}
		}

		if ( (selection != UNSET) && (CONTROL_mouse_down(LMB) == true) )
		{
			if (set_on == true)
			{
				current_boolean_props |= MATH_pow(selection);
			}
			else
			{
				current_boolean_props &= ~MATH_pow(selection);
			}
		}

	}
	else if (state == STATE_SET_UP_BUTTONS)
	{

	}
	else if (state == STATE_RESET_BUTTONS)
	{

	}

	return override_main_editor;
}



bool TILESETS_edit_call_appropriate_editor (int editing_mode , int state , int *current_cursor=NULL , int tileset_number=UNSET, int selected_tile=UNSET, int mx=UNSET, int my=UNSET)
{
	bool result;

	switch (editing_mode)
	{
	case EDITING_TILESET_MODE_SHAPE:
		result = TILESETS_edit_tile_shape (state , current_cursor , tileset_number, selected_tile, mx, my);
		break;

	case EDITING_TILESET_MODE_SOLID:
		result = TILESETS_edit_tile_solidity (state , current_cursor , tileset_number, selected_tile, mx, my);
		break;

	case EDITING_TILESET_MODE_STATS:
		result = TILESETS_edit_tile_accell_properties (state , current_cursor , tileset_number, selected_tile, mx, my);
		break;

	case EDITING_TILESET_MODE_ENERGY_WEAKNESS:
		result = TILESETS_edit_tile_vulnerability (state , current_cursor , tileset_number, selected_tile, mx, my);
		break;

	case EDITING_TILESET_MODE_NEXT_OF_KIN:
		result = TILESETS_edit_tile_next_of_kin (state , current_cursor , tileset_number, selected_tile, mx, my);
		break;

	case EDITING_TILESET_MODE_DEAD_SCRIPT:
		result = TILESETS_edit_tile_script_properties (state , current_cursor , tileset_number, selected_tile, mx, my);
		break;

	case EDITING_TILESET_MODE_GROUPING:
		result = TILESETS_edit_tile_grouping (state , current_cursor , tileset_number, selected_tile, mx, my);
		break;

	case EDITING_TILESET_MODE_FAMILY:
		result = TILESETS_edit_tile_family (state , current_cursor , tileset_number, selected_tile, mx, my);
		break;
		
	case EDITING_TILESET_BOOLEAN_PROPS:
		result = TILESETS_edit_boolean_tile_properties (state , current_cursor , tileset_number, selected_tile, mx, my);
		break;

	case EDITING_TILESET_COLLISION_BITMASK:
		result = TILESETS_edit_collision_bitmask (state , current_cursor , tileset_number, selected_tile, mx, my);
		break;

	case EDITING_TILESET_TILE_PRIORITY:
		result = TILESETS_edit_priority (state , current_cursor , tileset_number, selected_tile, mx, my);
		break;
	default:
		break;
	}

	return result;
}



void TILESETS_draw_help (int editing_mode, int help_box_x, int help_box_y)
{
	int temp_counter;

	OUTPUT_filled_rectangle ( help_box_x , help_box_y , help_box_x+320 , help_box_y+320 , 0 , 0 , 0 );
	OUTPUT_rectangle ( help_box_x , help_box_y , help_box_x+320 , help_box_y+320 );

	temp_counter = 8;

	OUTPUT_text ( help_box_x+8, help_box_y+(temp_counter+=8) , "              HELP PAGE!              " );
	OUTPUT_text ( help_box_x+8, help_box_y+(temp_counter+=8) , "--------------------------------------" );
	OUTPUT_text ( help_box_x+8, help_box_y+(temp_counter+=8) , "F2-F9 CHANGES EDITING MODE AND HELP.  " , 255 , 255 , 0 );
	OUTPUT_text ( help_box_x+8, help_box_y+(temp_counter+=8) , "ROLLING THE MOUSEWHEEL DOES THE SAME. " , 255 , 255 , 0 );
	OUTPUT_text ( help_box_x+8, help_box_y+(temp_counter+=8) , "--------------------------------------" , 255 , 255 , 0 );

	switch(editing_mode)
	{
	case EDITING_TILESET_MODE_SHAPE:
		OUTPUT_text ( help_box_x+8, help_box_y+(temp_counter+=8) , "THIS ALLOWS YOU TO CHOOSE THE BASIC   " , 0 , 255 , 255 );
		OUTPUT_text ( help_box_x+8, help_box_y+(temp_counter+=16) , "COLLISION PROFILE OF THE BLOCK.       " , 0 , 255 , 255 );
		OUTPUT_text ( help_box_x+8, help_box_y+(temp_counter+=8) , "LEFT-CLICK TO SET A BLOCK TO THE      " , 0 , 255 , 255 );
		OUTPUT_text ( help_box_x+8, help_box_y+(temp_counter+=16) , "CURRENTLY SELECTED PROFILE.           " , 0 , 255 , 255 );
		OUTPUT_text ( help_box_x+8, help_box_y+(temp_counter+=8) , "RIGHT-CLICK TO GRAB THE PROFILE FROM A" , 0 , 255 , 255 );
		OUTPUT_text ( help_box_x+8, help_box_y+(temp_counter+=16) , "BLOCK.                                " , 0 , 255 , 255 );
		OUTPUT_text ( help_box_x+8, help_box_y+(temp_counter+=8) , "CLICK THE BLOCK PROFILES AT THE BOTTOM" , 0 , 255 , 255 );
		OUTPUT_text ( help_box_x+8, help_box_y+(temp_counter+=16) , "TO SELECT ONE.                        " , 0 , 255 , 255 );
		break;

	case EDITING_TILESET_MODE_SOLID:
		OUTPUT_text ( help_box_x+8, help_box_y+(temp_counter+=8) , "THIS ALLOWS YOU TO CHOOSE WHICH SIDES " , 0 , 255 , 255 );
		OUTPUT_text ( help_box_x+8, help_box_y+(temp_counter+=8) , "OF A BLOCK ARE TREATED AS SOLID, IE.  " , 0 , 255 , 255 );
		OUTPUT_text ( help_box_x+8, help_box_y+(temp_counter+=8) , "IF ONLY THE TOP OF A BLOCK IS SOLID   " , 0 , 255 , 255 );
		OUTPUT_text ( help_box_x+8, help_box_y+(temp_counter+=8) , "YOU CAN PASS UPWARDS THROUGH IT BUT   " , 0 , 255 , 255 );
		OUTPUT_text ( help_box_x+8, help_box_y+(temp_counter+=16) , "NOT DOWNWARDS.                        " , 0 , 255 , 255 );
		OUTPUT_text ( help_box_x+8, help_box_y+(temp_counter+=8) , "LEFT-CLICK TO SET A BLOCK TO THE      " , 0 , 255 , 255 );
		OUTPUT_text ( help_box_x+8, help_box_y+(temp_counter+=16) , "CURRENTLY SELECTED PROFILE.           " , 0 , 255 , 255 );
		OUTPUT_text ( help_box_x+8, help_box_y+(temp_counter+=8) , "RIGHT-CLICK TO GRAB THE PROFILE FROM A" , 0 , 255 , 255 );
		OUTPUT_text ( help_box_x+8, help_box_y+(temp_counter+=16) , "BLOCK.                                " , 0 , 255 , 255 );
		OUTPUT_text ( help_box_x+8, help_box_y+(temp_counter+=8) , "CLICK THE BLOCK PROFILES AT THE BOTTOM" , 0 , 255 , 255 );
		OUTPUT_text ( help_box_x+8, help_box_y+(temp_counter+=16) , "TO SELECT ONE.                        " , 0 , 255 , 255 );
		break;

	case EDITING_TILESET_MODE_STATS:
		OUTPUT_text ( help_box_x+8, help_box_y+(temp_counter+=8) , "THIS ALLOWS YOU TO SET ALL SORTS OF   " , 0 , 255 , 255 );
		OUTPUT_text ( help_box_x+8, help_box_y+(temp_counter+=8) , "STATISTICS ABOUT THE BLOCK. READ THE  " , 0 , 255 , 255 );
		OUTPUT_text ( help_box_x+8, help_box_y+(temp_counter+=8) , "DOCUMENTATION FOR MORE INFORMATION    " , 0 , 255 , 255 );
		OUTPUT_text ( help_box_x+8, help_box_y+(temp_counter+=16) , "ABOUT BLOCK STATISTICS.               " , 0 , 255 , 255 );
		OUTPUT_text ( help_box_x+8, help_box_y+(temp_counter+=8) , "CLICK ARROWS TO ALTER THE VALUES OF   " , 0 , 255 , 255 );
		OUTPUT_text ( help_box_x+8, help_box_y+(temp_counter+=16) , "ANY STATS.                            " , 0 , 255 , 255 );
		OUTPUT_text ( help_box_x+8, help_box_y+(temp_counter+=8) , "LEFT-CLICK TO SET A BLOCK'S STATS TO  " , 0 , 255 , 255 );
		OUTPUT_text ( help_box_x+8, help_box_y+(temp_counter+=16) , "THE CURRENTLY SELECTED ONES.          " , 0 , 255 , 255 );
		OUTPUT_text ( help_box_x+8, help_box_y+(temp_counter+=8) , "RIGHT-CLICK TO GRAB THE STATS FROM A  " , 0 , 255 , 255 );
		OUTPUT_text ( help_box_x+8, help_box_y+(temp_counter+=16) , "BLOCK.                                " , 0 , 255 , 255 );
		break;

	case EDITING_TILESET_MODE_ENERGY_WEAKNESS:
		OUTPUT_text ( help_box_x+8, help_box_y+(temp_counter+=8) , "THIS ALLOWS YOU TO CHOOSE HOW MUCH    " , 0 , 255 , 255 );
		OUTPUT_text ( help_box_x+8, help_box_y+(temp_counter+=8) , "ENERGY A BLOCK HAS AND ALSO WHAT TYPES" , 0 , 255 , 255 );
		OUTPUT_text ( help_box_x+8, help_box_y+(temp_counter+=16) , "OF DAMAGE IT IS SUSCEPTABLE TO.       " , 0 , 255 , 255 );
		OUTPUT_text ( help_box_x+8, help_box_y+(temp_counter+=8) , "CLICK THE RAMP AND TOGGLE THE ICONS TO" , 0 , 255 , 255 );
		OUTPUT_text ( help_box_x+8, help_box_y+(temp_counter+=16) , "ALTER THE VALUES." , 0 , 255 , 255 );
		OUTPUT_text ( help_box_x+8, help_box_y+(temp_counter+=8) , "LEFT-CLICK TO SET A BLOCK'S STATS TO  " , 0 , 255 , 255 );
		OUTPUT_text ( help_box_x+8, help_box_y+(temp_counter+=16) , "THE CURRENTLY SELECTED ONES.          " , 0 , 255 , 255 );
		OUTPUT_text ( help_box_x+8, help_box_y+(temp_counter+=8) , "RIGHT-CLICK TO GRAB THE STATS FROM A  " , 0 , 255 , 255 );
		OUTPUT_text ( help_box_x+8, help_box_y+(temp_counter+=16) , "BLOCK.                                " , 0 , 255 , 255 );
		break;

	case EDITING_TILESET_MODE_NEXT_OF_KIN:
		OUTPUT_text ( help_box_x+8, help_box_y+(temp_counter+=8) , "THIS ALLOWS YOU TO CHOOSE WHICH BLOCK " , 0 , 255 , 255 );
		OUTPUT_text ( help_box_x+8, help_box_y+(temp_counter+=8) , "A BLOCK TURNS INTO WHEN IT'S DESTROYED" , 0 , 255 , 255 );
		OUTPUT_text ( help_box_x+8, help_box_y+(temp_counter+=16) , "VIA A COLLISION.                      " , 0 , 255 , 255 );
		OUTPUT_text ( help_box_x+8, help_box_y+(temp_counter+=8) , "LEFT-CLICK ON A SOURCE TILE THEN DRAG " , 0 , 255 , 255 );
		OUTPUT_text ( help_box_x+8, help_box_y+(temp_counter+=8) , "TO A DESTINATION TILE TO SET NEXT OF  " , 0 , 255 , 255 );
		OUTPUT_text ( help_box_x+8, help_box_y+(temp_counter+=16) , "KIN.                                  " , 0 , 255 , 255 );
		OUTPUT_text ( help_box_x+8, help_box_y+(temp_counter+=8) , "RIGHT-CLICK ON SOURCE TILE TO DELETE  " , 0 , 255 , 255 );
		OUTPUT_text ( help_box_x+8, help_box_y+(temp_counter+=16) , "A NEXT OF KIN.                        " , 0 , 255 , 255 );
		break;

	case EDITING_TILESET_MODE_DEAD_SCRIPT:
		OUTPUT_text ( help_box_x+8, help_box_y+(temp_counter+=8) , "THIS ALLOWS YOU TO CHOOSE WHICH SCRIPT" , 0 , 255 , 255 );
		OUTPUT_text ( help_box_x+8, help_box_y+(temp_counter+=16) , "IS FIRED WHEN A BLOCK GOES KERPLOOEY. " , 0 , 255 , 255 );

		OUTPUT_text ( help_box_x+8, help_box_y+(temp_counter+=8) , "IT ALSO ALLOWS YOU TO SET THE         " , 0 , 255 , 255 );
		OUTPUT_text ( help_box_x+8, help_box_y+(temp_counter+=8) , "PARAMETERS WHICH ARE PASSED ALONG TO  " , 0 , 255 , 255 );
		OUTPUT_text ( help_box_x+8, help_box_y+(temp_counter+=16) , "CREATED ENTITY.                       " , 0 , 255 , 255 );

		OUTPUT_text ( help_box_x+8, help_box_y+(temp_counter+=8) , "CHOOSE A SCRIPT BY CLICKING THE TOP-  " , 0 , 255 , 255 );
		OUTPUT_text ( help_box_x+8, help_box_y+(temp_counter+=16) , "LEFT BOX.                             " , 0 , 255 , 255 );

		OUTPUT_text ( help_box_x+8, help_box_y+(temp_counter+=8) , "CREATE A NEW PARAMETER BY CLICKING THE" , 0 , 255 , 255 );
		OUTPUT_text ( help_box_x+8, help_box_y+(temp_counter+=16) , "BOTTOM ROW OF THE PARAMETER LIST.     " , 0 , 255 , 255 );

		OUTPUT_text ( help_box_x+8, help_box_y+(temp_counter+=8) , "DELETE A PARAMETER BY CLICKING THE    " , 0 , 255 , 255 );
		OUTPUT_text ( help_box_x+8, help_box_y+(temp_counter+=16) , "DELETE BUTTON THEN ON A PARAMETER.    " , 0 , 255 , 255 );

		OUTPUT_text ( help_box_x+8, help_box_y+(temp_counter+=8) , "ALTER A PARAMETER BY CLICKING THE     " , 0 , 255 , 255 );
		OUTPUT_text ( help_box_x+8, help_box_y+(temp_counter+=16) , "VARIOUS BOXES AT THE RIGHT.           " , 0 , 255 , 255 );

		OUTPUT_text ( help_box_x+8, help_box_y+(temp_counter+=16) , "LEFT-CLICK TO SET BLOCK.              " , 0 , 255 , 255 );

		OUTPUT_text ( help_box_x+8, help_box_y+(temp_counter+=16) , "RIGHT-CLICK TO GRAB BLOCK.            " , 0 , 255 , 255 );
		break;

	case EDITING_TILESET_MODE_GROUPING:
		OUTPUT_text ( help_box_x+8, help_box_y+(temp_counter+=8) , "THIS ALLOWS YOU TO SET A RECTANGLE OF " , 0 , 255 , 255 );
		OUTPUT_text ( help_box_x+8, help_box_y+(temp_counter+=8) , "BLOCKS THAT ACT AS ONE WHEN HIT. THIS " , 0 , 255 , 255 );
		OUTPUT_text ( help_box_x+8, help_box_y+(temp_counter+=8) , "MEANS THEY ALL LOSE THE SAME ENERGY   " , 0 , 255 , 255 );
		OUTPUT_text ( help_box_x+8, help_box_y+(temp_counter+=8) , "(EVEN IF THEY HAVE DIFFERING VULNS)   " , 0 , 255 , 255 );
		OUTPUT_text ( help_box_x+8, help_box_y+(temp_counter+=16) , "AND ALL FIRE THEIR SCRIPTS.           " , 0 , 255 , 255 );

		OUTPUT_text ( help_box_x+8, help_box_y+(temp_counter+=8) , "THIS IS BASICALLY FOR THINGS LIKE     " , 0 , 255 , 255 );
		OUTPUT_text ( help_box_x+8, help_box_y+(temp_counter+=8) , "ARKANOID CLONES WHERE MULTIPLE-TILE   " , 0 , 255 , 255 );
		OUTPUT_text ( help_box_x+8, help_box_y+(temp_counter+=16) , "BLOCKS AND THE LIKE ARE NEEDED.       " , 0 , 255 , 255 );

		OUTPUT_text ( help_box_x+8, help_box_y+(temp_counter+=8) , "LEFT-CLICK AND DRAG TO CREATE A BLOCK " , 0 , 255 , 255 );
		OUTPUT_text ( help_box_x+8, help_box_y+(temp_counter+=16) , "GROUP.                                " , 0 , 255 , 255 );
		
		OUTPUT_text ( help_box_x+8, help_box_y+(temp_counter+=16) , "RIGHT-CLICK TO DESTROY A BLOCK-FAMILY." , 0 , 255 , 255 );
		break;

	case EDITING_TILESET_BOOLEAN_PROPS:
		OUTPUT_text ( help_box_x+8, help_box_y+(temp_counter+=8) , "THIS ALLOWS YOU TO SET THE BOOLEAN    " , 0 , 255 , 255 );
		OUTPUT_text ( help_box_x+8, help_box_y+(temp_counter+=8) , "PROPERTIES OF EACH TILE, MOST OF WHICH" , 0 , 255 , 255 );
		OUTPUT_text ( help_box_x+8, help_box_y+(temp_counter+=8) , "ARE ONLY APPLICABLE TO THE PLATFORM   " , 0 , 255 , 255 );
		OUTPUT_text ( help_box_x+8, help_box_y+(temp_counter+=16) , "HANDLING SUBROUTINES.                 " , 0 , 255 , 255 );
		
		OUTPUT_text ( help_box_x+8, help_box_y+(temp_counter+=16) , "LEFT-CLICK TO TOGGLE BINARY PROPERTIES" , 0 , 255 , 255 );

		OUTPUT_text ( help_box_x+8, help_box_y+(temp_counter+=8) , "LEFT-CLICK TO SET TILE WITH CURRENT   " , 0 , 255 , 255 );
		OUTPUT_text ( help_box_x+8, help_box_y+(temp_counter+=16) , "PROPERTIES, OVERWRITING IT'S OWN.     " , 0 , 255 , 255 );

		OUTPUT_text ( help_box_x+8, help_box_y+(temp_counter+=8) , "SHIFT-LEFT-CLICK TO BINARY OR THE     " , 0 , 255 , 255 );
		OUTPUT_text ( help_box_x+8, help_box_y+(temp_counter+=8) , "CURRENT PROPERTIES WITH THOSE OF THE  " , 0 , 255 , 255 );
		OUTPUT_text ( help_box_x+8, help_box_y+(temp_counter+=16) , "TILE.                                 " , 0 , 255 , 255 );

		OUTPUT_text ( help_box_x+8, help_box_y+(temp_counter+=8) , "CTRL-LEFT-CLICK TO BINARY NAND THE    " , 0 , 255 , 255 );
		OUTPUT_text ( help_box_x+8, help_box_y+(temp_counter+=8) , "CURRENT PROPERTIES WITH THOSE OF THE  " , 0 , 255 , 255 );
		OUTPUT_text ( help_box_x+8, help_box_y+(temp_counter+=16) , "TILE (IE, REMOVE THE PROPERTY)        " , 0 , 255 , 255 );

		OUTPUT_text ( help_box_x+8, help_box_y+(temp_counter+=16) , "RIGHT-CLICK TO GRAB TILE PROPERTIES.  " , 0 , 255 , 255 );
		break;

	case EDITING_TILESET_COLLISION_BITMASK:
		OUTPUT_text ( help_box_x+8, help_box_y+(temp_counter+=8) , "THIS ALLOWS YOU TO SET THE COLLISION  " , 0 , 255 , 255 );
		OUTPUT_text ( help_box_x+8, help_box_y+(temp_counter+=8) , "BITMASK OF EACH TILE, WHICH DETERMINES" , 0 , 255 , 255 );
		OUTPUT_text ( help_box_x+8, help_box_y+(temp_counter+=8) , "WHICH ENTITIES WILL SEE THE COLLISION " , 0 , 255 , 255 );
		OUTPUT_text ( help_box_x+8, help_box_y+(temp_counter+=16) , "BASED ON THEIR OWN COLLISION BITMASKS." , 0 , 255 , 255 );
		
		OUTPUT_text ( help_box_x+8, help_box_y+(temp_counter+=16) , "LEFT-CLICK TO TOGGLE BITFLAG.         " , 0 , 255 , 255 );

		OUTPUT_text ( help_box_x+8, help_box_y+(temp_counter+=8) , "LEFT-CLICK TO SET TILE WITH CURRENT   " , 0 , 255 , 255 );
		OUTPUT_text ( help_box_x+8, help_box_y+(temp_counter+=16) , "BITMASK, OVERWRITING IT'S OWN.        " , 0 , 255 , 255 );

		OUTPUT_text ( help_box_x+8, help_box_y+(temp_counter+=8) , "SHIFT-LEFT-CLICK TO BINARY OR THE     " , 0 , 255 , 255 );
		OUTPUT_text ( help_box_x+8, help_box_y+(temp_counter+=16) , "CURRENT BITMASK WITH THE TILE'S      ." , 0 , 255 , 255 );

		OUTPUT_text ( help_box_x+8, help_box_y+(temp_counter+=8) , "CTRL-LEFT-CLICK TO BINARY NAND THE    " , 0 , 255 , 255 );
		OUTPUT_text ( help_box_x+8, help_box_y+(temp_counter+=8) , "CURRENT BITMASK WITH THOSE OF THE TILE" , 0 , 255 , 255 );
		OUTPUT_text ( help_box_x+8, help_box_y+(temp_counter+=16) , "(IE, REMOVE THE PROPERTY)             " , 0 , 255 , 255 );

		OUTPUT_text ( help_box_x+8, help_box_y+(temp_counter+=16) , "RIGHT-CLICK TO GRAB TILE BITMASK.     " , 0 , 255 , 255 );
		break;
		
	default:
		break;
	}

}



bool TILESETS_edit (bool initialise , int starting_tileset_number)
{
	static int tileset_number;
	// Duh!

	static float scale;
	// How zoomed in the display is.

	static int start_x_pixel_offset;
	static int start_y_pixel_offset;
	// How offset you are from the top-left of the tiles in display pixels.

	static int first_sprite_number;
	// The first tile in the set you want to draw. Usually 0.

	int bitmap_number;
	// The graphic file used by this tileset

	int mx,my,mz;
	// Mouse x,y,z.

	bool exit_editor;
	// Is it quitting time? Sheesh...

	int selected_sprite;
	// The bitmap image number when you're selecting it from the menu because it isn't set yet.

	int new_tilesize;
	// How big the tiles are after zooming.

	int selected_tile;
	// Which tile the mouse is over in the main window

	static bool scale_changed;
	// Has the size changed this frame?

	int temp;
	// Various uses.

	static bool grabbed;
	// Are we dragging the main display area around?

	static int grabbed_x_pixel_offset;
	static int grabbed_y_pixel_offset;
	// Where did we start grabbing it?

	static int editing_mode;
	// What statistic of the block are we editing?

	static bool important_menu_override;
	// Is there something jolly important happening in one of the editors which precludes us from
	// doing anything else?

	int t;
	// Loop counter.

	int current_cursor;
	// Which mouse cursor is being drawn?

	int help_box_x = 160;
	int help_box_y = 80;
	// Where the help box appears.

	int scroll_bar_size = ICON_SIZE;
	// The size of the bars are the right and bottom of the main window.

	int next_of_kin;
	// Used for the next of kin editing mode thing, shows the kin tile.

	new_tilesize = int (ts[tileset_number].tilesize * scale);

	exit_editor = false;

	mx = CONTROL_mouse_x();
	my = CONTROL_mouse_y();
	mz = CONTROL_mouse_speed (Z_POS);

	if (initialise)
	{
		tileset_number = starting_tileset_number;
		start_x_pixel_offset = 0;
		start_y_pixel_offset = 0;
		scale = 1;

		important_menu_override = false;

		first_sprite_number = 0;

		scale_changed = true;

		initialise = false;

		for (editing_mode=0; editing_mode<DIFFERENT_TILESET_EDITING_MODES; editing_mode++)
		{
			TILESETS_edit_call_appropriate_editor (editing_mode , STATE_INITIALISE);
		}

		editing_mode = EDITING_TILESET_MODE_SHAPE;

		TILESETS_edit_call_appropriate_editor (editing_mode , STATE_SET_UP_BUTTONS);
	}
	else
	{
		OUTPUT_clear_screen();

		current_cursor = SELECT_ARROW; // Always defaults to this.

		bitmap_number = ts[tileset_number].tileset_image_index;

		if (bitmap_number == UNSET)
		{
			selected_sprite = GPL_draw_list ("SPRITES" , 160 , 32 , 320 , 464 , first_sprite_number , mx , my , false);

			if (CONTROL_mouse_hit (LMB) == true)
			{
				if (selected_sprite != UNSET)
				{
					int start;
					GPL_list_extents ("SPRITES", &start, NULL);
					strcpy (ts[tileset_number].tileset_sprite_name , GPL_what_is_word_name (start+selected_sprite) );
					ts[tileset_number].tileset_image_index = GPL_find_word_value ("SPRITES" , ts[tileset_number].tileset_sprite_name);
					ts[tileset_number].tilesize = OUTPUT_sprite_width(ts[tileset_number].tileset_image_index,0);
					TILESETS_allocate_tile_space (tileset_number , OUTPUT_bitmap_frames (ts[tileset_number].tileset_image_index) );
				}
			}
		}
		else
		{
			// ---------------- Drawing Stuff ----------------

			selected_tile = TILESETS_draw ( tileset_number , start_x_pixel_offset , start_y_pixel_offset , scale , false , mx , my , editing_mode );

			if (editing_mode == EDITING_TILESET_MODE_NEXT_OF_KIN)
			{
				if ( (CONTROL_mouse_down(LMB) == false) && (selected_tile != UNSET) )
				{
					next_of_kin = ts[tileset_number].tileset_pointer[selected_tile].next_of_kin;
				}
				else
				{
					next_of_kin = selected_tile;
				}
			}				

			if ( (next_of_kin != UNSET) && (editing_mode == EDITING_TILESET_MODE_NEXT_OF_KIN) )
			{
				TILESETS_draw (tileset_number , start_x_pixel_offset , start_y_pixel_offset , scale , false , mx, my , editing_mode , next_of_kin , TO_THIS_BLOCK );
			}

			OUTPUT_filled_rectangle (window_width,0,game_screen_width,game_screen_height,0,0,0);
			OUTPUT_filled_rectangle (0,window_height,game_screen_width,game_screen_height,0,0,0);
			OUTPUT_rectangle (0,0,window_width,window_height,0,0,255);
		
			if ( (mx>window_width) || (my>window_height) )
			{
				selected_tile = UNSET;
			}

			bitmap_number = ts[tileset_number].tileset_image_index;

			if (scale_changed) // Have we zoomed in/out during the last frame?
			{
				if (OUTPUT_bitmap_width(bitmap_number) * scale > window_width)
				{
					// Need a scroll bar!
					// Kill the old one if it exists.
					SIMPGUI_kill_button (TILESET_BUTTON_GROUP_ID, HORIZONTAL_TILE_DRAG_BAR);
					// And create a new one.
					temp = (int (OUTPUT_bitmap_width(bitmap_number) * scale) - window_width);
					if (temp<0)
					{
						temp = 0;
					}
					SIMPGUI_create_h_drag_bar (TILESET_BUTTON_GROUP_ID, HORIZONTAL_TILE_DRAG_BAR, &start_x_pixel_offset, 0, window_height, window_width, scroll_bar_size , 0 , temp, 1, scroll_bar_size*2, LMB);
				}
				else
				{
					SIMPGUI_kill_button (TILESET_BUTTON_GROUP_ID, HORIZONTAL_TILE_DRAG_BAR);
					start_x_pixel_offset = 0;
				}

				if (OUTPUT_bitmap_height(bitmap_number) * scale > window_height)
				{
					// Need a scroll bar!
					// Kill the old one if it exists.
					SIMPGUI_kill_button (TILESET_BUTTON_GROUP_ID, VERTICAL_TILE_DRAG_BAR);
					// And create a new one.
					temp = (int (OUTPUT_bitmap_height(bitmap_number) * scale) - window_height);
					if (temp<0)
					{
						temp = 0;
					}
					SIMPGUI_create_v_drag_bar (TILESET_BUTTON_GROUP_ID, HORIZONTAL_TILE_DRAG_BAR, &start_y_pixel_offset, window_width, 0, scroll_bar_size, window_height , 0 , temp, 1, scroll_bar_size*2, LMB);
				}
				else
				{
					SIMPGUI_kill_button (TILESET_BUTTON_GROUP_ID, VERTICAL_TILE_DRAG_BAR);
					start_y_pixel_offset = 0;
				}

			}

			SIMPGUI_check_all_buttons ();
			
			OUTPUT_draw_masked_sprite ( first_icon , ALTER_GFX_ICON , game_screen_width-ICON_SIZE, window_height );

			important_menu_override = TILESETS_edit_call_appropriate_editor (editing_mode , STATE_RUNNING , &current_cursor , tileset_number, selected_tile, mx, my);

			// ---------------- Mouse Input ----------------

			if ( (mx<window_width) && (my<window_height) && (important_menu_override == false) )
			{
				// Clicks in the editing window...

				if (mz) // Mousewheel rolled
				{
					scale_changed = true;

					scale += MATH_sgn_int(mz);
					if (scale < 1)
					{
						scale = 1;
					}
					if (scale > 4)
					{
						scale = 4;
					}
				}
				else
				{
					// You haven't changed the scale...
					scale_changed = false;
				}

				// Middle-mouse window dragging!

				if (CONTROL_grab_start (MMB))
				{
					grabbed = true;
					grabbed_x_pixel_offset = start_x_pixel_offset;
					grabbed_y_pixel_offset = start_y_pixel_offset;
				}

				if (CONTROL_mouse_down (MMB) == true)
				{
					start_x_pixel_offset = grabbed_x_pixel_offset + (CONTROL_grab_offset_x());
					start_y_pixel_offset = grabbed_y_pixel_offset + (CONTROL_grab_offset_y());

					temp = (int (OUTPUT_bitmap_width(bitmap_number) * scale) - window_width);
					if (temp<0)
					{
						temp = 0;
					}
					start_x_pixel_offset = MATH_constrain (start_x_pixel_offset, 0, temp);

					temp = (int (OUTPUT_bitmap_height(bitmap_number) * scale) - window_height);
					if (temp<0)
					{
						temp = 0;
					}
					start_y_pixel_offset = MATH_constrain (start_y_pixel_offset, 0, temp);
				}
			}
			else if (important_menu_override == false)
			{
				// Clicks in the panel...

				if (mz)
				{
					// You have changed which statistic is being set...
					SIMPGUI_kill_group(TILESET_SUB_GROUP_ID);
					editing_mode += MATH_sgn_int(mz);
					editing_mode = MATH_wrap(editing_mode,DIFFERENT_TILESET_EDITING_MODES);
					TILESETS_edit_call_appropriate_editor (editing_mode , STATE_SET_UP_BUTTONS);
				}

				if (CONTROL_mouse_hit(LMB))
				{
					// Reset chosen sprite for tileset...

					if ( MATH_rectangle_to_point_intersect ( game_screen_width-ICON_SIZE, window_height , game_screen_width , window_height+ICON_SIZE , mx , my ) )
					{
						ts[tileset_number].tileset_image_index = UNSET;
						start_x_pixel_offset = 0;
						start_y_pixel_offset = 0;
					}
				}

			}

			// ---------------- Keyboard Input ----------------

			if (important_menu_override == false)
			{
				for (t=0; t<DIFFERENT_TILESET_EDITING_MODES ;t++)
				{
					if (CONTROL_key_hit(KEY_F2+t))
					{
						SIMPGUI_kill_group(TILESET_SUB_GROUP_ID);
						editing_mode = t;
						TILESETS_edit_call_appropriate_editor (editing_mode , STATE_SET_UP_BUTTONS);
					}
				}

				if (CONTROL_key_down(KEY_F1) == true)
				{
					TILESETS_draw_help (editing_mode, help_box_x, help_box_y);
				}
			}
		}

		if (CONTROL_key_hit (KEY_ESC) == true)
		{
			exit_editor = true;
		}

		OUTPUT_draw_masked_sprite ( first_icon , current_cursor , mx, my );
	}

	if (exit_editor)
	{
		game_state = GAME_STATE_EDITOR_OVERVIEW;
		SIMPGUI_kill_group (TILESET_BUTTON_GROUP_ID);
		SIMPGUI_kill_group (TILESET_SUB_GROUP_ID);

		for (editing_mode=0; editing_mode<DIFFERENT_TILESET_EDITING_MODES; editing_mode++)
		{
			TILESETS_edit_call_appropriate_editor (editing_mode , STATE_SHUTDOWN);
		}

		initialise = true;
	}

	return initialise;
}



void TILESETS_change_name (char *old_entry_name, char *new_entry_name)
{
	int t;

	for (t=0 ; t<number_of_tilesets_loaded ; t++)
	{
		if ( strcmp (ts[t].name , old_entry_name) == 0)
		{
			strcpy (ts[t].name , new_entry_name);
		}
	}

}



void TILESETS_register_script_name_change (char *old_entry_name, char *new_entry_name)
{
	int t,i;

	for (t=0 ; t<number_of_tilesets_loaded ; t++)
	{
		for (i=0; i<ts[t].tile_count ; i++)
		{
			if ( strcmp (ts[t].tileset_pointer[i].dead_script , old_entry_name) == 0)
			{
				strcpy (ts[t].tileset_pointer[i].dead_script , new_entry_name);
			}
		}
	}

}



void TILESETS_register_sprite_name_change (char *old_entry_name, char *new_entry_name)
{
	int t;

	for (t=0 ; t<number_of_tilesets_loaded ; t++)
	{
		if ( strcmp (ts[t].tileset_sprite_name , old_entry_name) == 0)
		{
			strcpy (ts[t].tileset_sprite_name , new_entry_name);
		}
	}

}



void TILESETS_register_parameter_name_change (char *list_name, char *old_entry_name, char *new_entry_name)
{
	int t,i,j;

	for (t=0 ; t<number_of_tilesets_loaded ; t++)
	{
		// Now have a lovely plough through all the tiles in this set and check them for broken script links.

		for (i=0; i<ts[t].tile_count ; i++)
		{
			for (j=0; j<ts[t].tileset_pointer[i].params; j++)
			{
				if ( strcmp (ts[t].tileset_pointer[i].param_list_pointer[j].param_list_type , list_name) == 0)
				{
					// Well, it's a parameter of a matching list type, but is it a matching name?
					if ( strcmp (ts[t].tileset_pointer[i].param_list_pointer[j].param_name , old_entry_name) == 0)
					{
						// Yes it is! Righty, change the sucker!
						strcpy (ts[t].tileset_pointer[i].param_list_pointer[j].param_name , new_entry_name);
					}
				}
			}
		}
	}

}



bool TILESETS_confirm_tile_links (int tileset_number, int tile_number , bool report)
{
	char line[MAX_LINE_SIZE];
	int j;
	int result;
	bool okay;

	ts[tileset_number].tileset_pointer[tile_number].dead_script_index = int (GPL_find_word_value ("SCRIPTS", ts[tileset_number].tileset_pointer[tile_number].dead_script));
	
	if (ts[tileset_number].tileset_pointer[tile_number].dead_script_index == UNSET) // if it can't find a script with a matching name...
	{
		if (strcmp(ts[tileset_number].tileset_pointer[tile_number].dead_script,"UNSET") == 0)
		{
			// Used to be Unset and still is - no worries...
		}
		else
		{
			// Used to be something but is now UNSET! SHIIIIIIT!
			if (report == true)
			{
				sprintf ( line , "   TILE %i CANNOT FIND SCRIPT '%s'.\n" , tile_number , ts[tileset_number].tileset_pointer[tile_number].dead_script );
				EDIT_add_line_to_report (line);
			}
			ts[tileset_number].tileset_pointer[tile_number].dead_script_index = BROKEN_LINK;
			okay = false;
		}
	}

	for (j=0; j<ts[tileset_number].tileset_pointer[tile_number].params; j++)
	{
		// FIND THE VARIABLE WE'RE SETTING WITH THE VALUE
		
		result = GPL_does_parameter_exist ( "VARIABLE" , ts[tileset_number].tileset_pointer[tile_number].param_list_pointer[j].param_dest_var );

		if (result == OKAY)
		{
			ts[tileset_number].tileset_pointer[tile_number].param_list_pointer[j].param_dest_var_index = GPL_find_word_value ( "VARIABLE" , ts[tileset_number].tileset_pointer[tile_number].param_list_pointer[j].param_dest_var );
		}
		else if (result == BROKEN_LINK)
		{
			// The variable name wasn't recognised - most probably just UNSET.
			if (report == true)
			{
				sprintf ( line , "   TILESET '%s' TILE #%d PARAMETER VARIABLE %d = '%s' CANNOT FIND THAT IN VARIABLE LIST.\n" , ts[tileset_number].name , tile_number , j , ts[tileset_number].tileset_pointer[tile_number].param_list_pointer[j].param_dest_var );
				EDIT_add_line_to_report (line);
			}
			okay = false;
			ts[tileset_number].tileset_pointer[tile_number].param_list_pointer[j].param_dest_var_index = UNSET;
		}
		else if (result == VERY_BROKEN_LINK)
		{
			// Can't even find the word list...
			if (report == true)
			{
				sprintf ( line , "   TILESET '%s' TILE #%d PARAMETER %d = '%s' CANNOT FIND VARIABLE LIST.\n" , ts[tileset_number].name , tile_number , j , ts[tileset_number].tileset_pointer[tile_number].param_list_pointer[j].param_dest_var );
				EDIT_add_line_to_report (line);
			}
			okay = false;
			ts[tileset_number].tileset_pointer[tile_number].param_list_pointer[j].param_dest_var_index = UNSET;
		}

		// FIND THE LIST AND VALUE

		result = GPL_does_parameter_exist ( ts[tileset_number].tileset_pointer[tile_number].param_list_pointer[j].param_list_type , ts[tileset_number].tileset_pointer[tile_number].param_list_pointer[j].param_name );
		
		if (result == OKAY)
		{
			// Everything is okay, daddio! Just store the values...

			ts[tileset_number].tileset_pointer[tile_number].param_list_pointer[j].param_value = GPL_find_word_value ( ts[tileset_number].tileset_pointer[tile_number].param_list_pointer[j].param_list_type , ts[tileset_number].tileset_pointer[tile_number].param_list_pointer[j].param_name );

		}
		else if (result == BROKEN_LINK)
		{
			// The word in the list was not recognised.
			if (report == true)
			{
				sprintf ( line , "   TILESET '%s' TILE #%d PARAMETER %d = '%s' CANNOT FIND '%s' IN WORD LIST '%s'.\n" , ts[tileset_number].name , tile_number , j , ts[tileset_number].tileset_pointer[tile_number].param_list_pointer[j].param_dest_var , ts[tileset_number].tileset_pointer[tile_number].param_list_pointer[j].param_name , ts[tileset_number].tileset_pointer[tile_number].param_list_pointer[j].param_list_type );
				EDIT_add_line_to_report (line);
			}
			okay = false;
			ts[tileset_number].tileset_pointer[tile_number].param_list_pointer[j].param_value = UNSET;
		}
		else if (result == VERY_BROKEN_LINK)
		{
			// Can't even find the word list...
			if (report == true)
			{
				sprintf ( line , "   TILESET '%s' TILE #%d PARAMETER %d = '%s' CANNOT FIND WORD LIST '%s'.\n" , ts[tileset_number].name , tile_number , j , ts[tileset_number].tileset_pointer[tile_number].param_list_pointer[j].param_dest_var , ts[tileset_number].tileset_pointer[tile_number].param_list_pointer[j].param_list_type );
				EDIT_add_line_to_report (line);
			}
			okay = false;
			ts[tileset_number].tileset_pointer[tile_number].param_list_pointer[j].param_value = UNSET;
		}

	}

	return okay;

}



bool TILESETS_confirm_tileset_links (int tileset_number , bool report)
{
	char line[MAX_LINE_SIZE];
	int i;
	bool okay = true;
	bool all_okay = true;

	if (report == true)
	{
		sprintf ( line , "TILESET '%s' REPORT:\n" , ts[tileset_number].name );
		EDIT_add_line_to_report (line);
	}

	if (ts[tileset_number].deleted == false)
	{
		// Check graphic existance...

		ts[tileset_number].tileset_image_index = int (GPL_find_word_value ("SPRITES", ts[tileset_number].tileset_sprite_name));

		if (ts[tileset_number].tileset_image_index == UNSET) // if it can't find a sprite with a matching name...
		{
			if (strcmp(ts[tileset_number].tileset_sprite_name,"UNSET") == 0)
			{
				// Used to be Unset and still is - no worries.
				if (report == true)
				{
					EDIT_add_line_to_report ("    STILL NEEDS SPRITE ASSIGNED!\n");
				}
			}
			else
			{
				// Used to be something but is now UNSET! SHIIIIIIT!
				if (report == true)
				{
					sprintf ( line , "    CANNOT FIND SPRITE '%s'.\n" , ts[tileset_number].tileset_sprite_name );
					EDIT_add_line_to_report (line);
				}
				ts[tileset_number].tileset_image_index = BROKEN_LINK;
				okay = false;
			}
		}
		else
		{
			ts[tileset_number].tilesize = OUTPUT_sprite_width (ts[tileset_number].tileset_image_index , 0);
		}

		// Now have a lovely plough through all the tiles in this set and check them for broken script links.
		for (i=0; i<ts[tileset_number].tile_count ; i++)
		{
			if (TILESETS_confirm_tile_links (tileset_number, i , report) == false)
			{
				okay = false;
			}
		}

		if (okay)
		{
			if (report == true)
			{
				EDIT_add_line_to_report ("    EVERYTHING IS PEACHY!\n");
			}
		}
		else
		{
			all_okay = false;
		}

	}
	else
	{
		if (report == true)
		{
			EDIT_add_line_to_report ("    MISSING PRESUMED DELETED!\n");
			all_okay = false;
		}
	}

	return all_okay;

}



bool TILESETS_confirm_all_links (void)
{
	int t;
	bool all_okay;

	all_okay = true;

	for (t=0 ; t<number_of_tilesets_loaded ; t++)
	{
		all_okay = TILESETS_confirm_tileset_links (t , true);
	}

	if (all_okay)
	{
		if (number_of_tilesets_loaded > 0)
		{
			EDIT_add_line_to_report ("ALL TILESETS PEACHY!\n");
		}
		else
		{
			EDIT_add_line_to_report ("NO TILESETS TO PROCESS...\n");
		}		
	}

	return all_okay;
}



char * TILESETS_get_name (int tileset_number)
{
	if (tileset_number < 0 || tileset_number >= number_of_tilesets_loaded || ts == NULL)
	{
		return (char *)"UNSET";
	}

	return ts[tileset_number].name;
}



int TILESETS_get_tilesize (int tileset_number)
{
	if (tileset_number < 0 || tileset_number >= number_of_tilesets_loaded || ts == NULL)
	{
		return 0;
	}

	return ts[tileset_number].tilesize;
}



int TILESETS_get_sprite_index (int tileset_number)
{
	if (tileset_number < 0 || tileset_number >= number_of_tilesets_loaded || ts == NULL)
	{
		return UNSET;
	}

	return ts[tileset_number].tileset_image_index;
}



int TILESETS_get_tile_count (int tileset_number)
{
	if (tileset_number < 0 || tileset_number >= number_of_tilesets_loaded || ts == NULL)
	{
		return 0;
	}

	return ts[tileset_number].tile_count;
}



bool TILESETS_is_tile_solid (int tileset_number, int tile_number)
{
	if (ts[tileset_number].tileset_pointer[tile_number].solid_sides > 0)
	{
		return true;
	}
	else
	{
		return false;
	}
}
