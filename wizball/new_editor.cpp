#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <assert.h>

#include "scripting.h"
#include "parser.h"
#include "graphics.h"
#include "new_editor.h"
#include "main.h"
#include "control.h"
#include "math_stuff.h"
#include "string_stuff.h"
#include "simple_gui.h"
#include "game.h"
#include "paths.h"
#include "global_param_list.h"
#include "output.h"
#include "file_stuff.h"
#include "tilesets.h"
#include "tilemaps.h"
#include "textfiles.h"
#include "roomzones.h"
#include "spawn_points.h"
#include "ai_nodes.h"
#include "hiscores.h"
#include "arrays.h"
#include "object_collision.h"
#include "particles.h"
#include "world_collision.h"
#include "platform_renderer.h"

#include "fortify.h"

// Need support for offset tilemap collision really if the user wants it as it's JOLLY fast.



char *report = NULL;

int report_length;

int first_icon = 0;
int bitmask_gfx = 0;
int small_font_gfx = 0;
int object_sides_gfx = 0;



void EDIT_load_game_descriptor_file (void)
{
	// This is the file which remembers a few sundry facts such as the room size in the editor
	// as well as all the map filenames in order so that numbering is preserved which is jolly
	// important.

	


}



void EDIT_save_game_descriptor_file (char *project)
{
	




}



#define REPORT_CHUNK_SIZE			(64)

int EDIT_add_line_to_report (char *line)
{
	static int report_line = 0;

	if (report == NULL) // No lines in there yet...
	{
		report = (char *) malloc ( REPORT_CHUNK_SIZE * MAX_LINE_SIZE * sizeof(char) );
	}
	else // Adding a line?
	{
		if (report_line % REPORT_CHUNK_SIZE == 0)
		{
			report = (char *) realloc ( report , (report_line+REPORT_CHUNK_SIZE) * MAX_LINE_SIZE * sizeof(char) );
		}
	}
	
	strcpy ( &report[report_line*MAX_LINE_SIZE] , line );

	report_line++;

	return report_line;
}



void EDIT_output_report (void)
{
	int l;

	FILE *file_pointer = fopen (MAIN_get_project_filename ("report.txt", true),"w");

	if (file_pointer != NULL)
	{
		for (l=0 ; l<report_length; l++)
		{
			fputs ( &report[l*MAX_LINE_SIZE] , file_pointer );
		}

		fclose(file_pointer);
	}
	else
	{
		OUTPUT_message("Could not open file to output report!");
		assert(0); // WTF?!
	}
}



char * EDIT_clip_quotes (char *word)
{
	// This searches through a string for the first and last quotes and removes everything
	// outside them, including the quotes themselves.

	unsigned int i;
	bool exitflag = false;
	char *pointer;

	// First first quote...
	for (i=0; (i<strlen(word)) && (exitflag == false); i++)
	{
		if (word[i] == '"')
		{
			pointer = &word[i]+1;
			exitflag = true;
		}
	}

	exitflag = false;

	for (i=strlen(word); i>0 && (exitflag == false); i--)
	{
		if (word[i] == '"')
		{
			word[i] = '\0';
			exitflag = true;
		}
		else
		{
			word[i] = '\0';
		}
	}

	return pointer;

}



bool EDIT_add_data_to_line (char *line, int int_value , unsigned int max_line_length)
{
	// Attempts to add a value to a line of type char, but only does it if it'll fit, otherwise returns false
	// It will also add a comma to the line if it isn't the first bit of data to be added to it.

	char string_value[32];
	int length;
	
	sprintf(string_value,"%d",int_value);

	if (strlen(line)>0)
	{
		length = strlen(string_value)+1;
	}
	else
	{
		length = strlen(string_value);
	}

	if (strlen(line) + length > max_line_length)
	{
		return false;
	}
	else
	{
		if (strlen(line)>0)
		{
			strcat(line,",");
			strcat(line,string_value);
		}
		else
		{
			strcat(line,string_value);
		}
		return true;
	}

}



void EDIT_change_name (char *list_name , char *old_entry_name , char *new_entry_name)
{
	// Drop out immediately if the name ain't changed.
	if (strcmp (old_entry_name,new_entry_name) == 0)
	{
		return;
	}

	// Righty, first of all change the one from the global paramter list...

	GPL_rename_entry (list_name, old_entry_name, new_entry_name);

	// Then deal with the actual name in the data itself..

	// Is the list type "PATHS"?

	if (strcmp (list_name , "PATHS") == 0)
	{
		PATHS_change_name (old_entry_name, new_entry_name);
	}

	// Is the list type "TILEMAPS"?

	if (strcmp (list_name , "TILEMAPS") == 0)
	{
		TILEMAPS_change_name (old_entry_name, new_entry_name);
	}

	// Is the list type "TILESETS"?

	if (strcmp (list_name , "TILESETS") == 0)
	{
		TILESETS_change_name (old_entry_name, new_entry_name);
	}

/*

	// Is the list type "SPRITES"?

	if (strcmp (list_name , "SPRITES") == 0)
	{
		// Now this actually means changing the filename rather than an internally held name.
	}

	// Is the list type "SPAWNPOINTS"?

	// Note that this is a special type as nothing ever refers to a spawn point by name
	// (it's purely a description for the user to reference the point) so there's no
	// cross-referencing necessary after changing the name in spawn point itself.

	if (strcmp (list_name , "SPAWNPOINTS") == 0)
	{
		
	}

	// Is the list type "GAMEZONE"?

	// Note that this is a special type as nothing ever refers to a game zone by name
	// (it's purely a description for the user to reference the zone) so there's no
	// cross-referencing necessary after changing the name in zone itself.

	if (strcmp (list_name , "GAMEZONE") == 0)
	{
		
	}

*/

}



void EDIT_register_name_change (char *list_name , char *old_entry_name , char *new_entry_name)
{
	// Righty, if you change any of the words in the word list then there's every chance
	// that some poor plebby spawn point or tileset was pointing at it beforehand and now
	// it's pointing at a word that doesn't exist any more. So what this does is first of
	// all look through all the objects that contain that kind of data to see if any of the
	// idiots were looking at the current word and if so updates them so they are looking
	// at the new word. Then it calls the alphabetising routine.

	// Obviously we can split up the checking at this point as certain data structures only
	// look at certain lists, ie. if the list_name of the changed word is SPRITES (unlikely)
	// then only the TILESETS need to be looked through.

	// The exception to this rule is the spawn points, which can have words from any word_list
	// and so must always be checked.

	// Is the list type "SPRITES"?

	if (strcmp (list_name , "SPRITES") == 0)
	{
		// Check tilesets for "SPRITES" name changes... (very unlikely I'll includer that functionality, though)
		TILESETS_register_sprite_name_change (old_entry_name, new_entry_name);
	}

	// Is the list type "TILESETS"?

	if (strcmp (list_name , "TILESETS") == 0)
	{
		// Check for "TILESETS" name changes that tilemaps might be pointing at. The scamps.
		TILEMAPS_register_tileset_name_change (old_entry_name, new_entry_name);
	}

	// Is the list type "SCRIPTS"?

	if (strcmp (list_name , "SCRIPTS") == 0)
	{
		// Then check all the spawn points for "SCRIPTS" name changes...
		TILEMAPS_register_script_name_change (old_entry_name, new_entry_name);

		// Now check for "SCRIPTS" name changes in the dead scripts for the tiles.
		TILESETS_register_script_name_change (old_entry_name, new_entry_name);
	}

	// Check all the parameters in the spawn points and tiles for any name change.

	TILEMAPS_register_parameter_name_change (list_name, old_entry_name, new_entry_name);

	TILESETS_register_parameter_name_change (list_name, old_entry_name, new_entry_name);

}



bool EDIT_confirm_links (void)
{
	// This looks at the loaded tilesets and maps and confirms the existance of both the
	// graphic files that the tilesets use and the tilesets that tilemaps use. It also checks
	// that the script names stored in tiles are correct.

	// It'll also check all the spawn points to make sure that the scripts they call are likewise
	// present, that the path which is attached is present and that the parameters are all okay.
	
	// First of all check all the tilesets to see if the equivalent graphics exist for them, and if so set
	// the pointer to where it needs to be. Also check that the scripts exist for the tiles.

	bool tilesets_okay;
	bool tilemaps_okay;

	EDIT_add_line_to_report ("TILESET REPORTS\n");
	EDIT_add_line_to_report ("---------------\n");
	EDIT_add_line_to_report ("\n");

	tilesets_okay = TILESETS_confirm_all_links();

	EDIT_add_line_to_report ("\n\n\n");
	EDIT_add_line_to_report ("TILEMAP REPORTS\n");
	EDIT_add_line_to_report ("---------------\n");
	EDIT_add_line_to_report ("\n");

	tilemaps_okay = TILEMAPS_confirm_links();

	EDIT_add_line_to_report ("\n\n\n");
	report_length = EDIT_add_line_to_report ("END OF REPORT\n");

	if ( (tilesets_okay) && (tilemaps_okay) )
	{
		return true;
	}
	else
	{
		return false;
	}

}


static bool EDIT_tilemaps_have_valid_dimensions(void)
{
	if (cm == NULL || number_of_tilemaps_loaded <= 0)
	{
		MAIN_add_to_log("Tilemap validation failed: no tilemaps are loaded.");
		return false;
	}

	for (int map_number = 0; map_number < number_of_tilemaps_loaded; map_number++)
	{
		if ((cm[map_number].map_width <= 0) || (cm[map_number].map_height <= 0) || (cm[map_number].map_layers <= 0))
		{
			char line[MAX_LINE_SIZE];
			sprintf(line,
				"Tilemap validation failed: map=%d width=%d height=%d layers=%d tileset=%d name=%s",
				map_number,
				cm[map_number].map_width,
				cm[map_number].map_height,
				cm[map_number].map_layers,
				cm[map_number].tile_set_index,
				cm[map_number].name);
			MAIN_add_to_log(line);
			return false;
		}
	}

	return true;
}



void EDIT_setup (void)
{
	MAIN_add_to_log ("Loading game descriptor file...");
	EDIT_load_game_descriptor_file ();
	MAIN_add_to_log ("\tOK!");

	MAIN_add_to_log ("Loading editor settings file...");
	TILEMAPS_load_editor_settings ();
	MAIN_add_to_log ("\tOK!");

	if (parse_all_data)
	{
		MAIN_add_to_log ("Loading tilesets from verbose files...");
		TILESETS_load_all ();
		MAIN_add_to_log ("\tOK!");

		MAIN_add_to_log ("Loading tilemaps from verbose files...");
		TILEMAPS_load_all ();
		MAIN_add_to_log ("\tOK!");
	}
	else
	{
#ifdef ALLEGRO_LINUX
		MAIN_add_to_log ("Linux compatibility mode: loading verbose map data...");

		MAIN_add_to_log ("Loading tilesets from verbose files...");
		TILESETS_load_all ();
		MAIN_add_to_log ("\tOK!");

		MAIN_add_to_log ("Loading tilemaps from verbose files...");
		TILEMAPS_load_all ();
		MAIN_add_to_log ("\tOK!");
#else
		MAIN_add_to_log ("Loading tilesets from data files...");
		if (TILESETS_load_game_data())
		{
			MAIN_add_to_log ("\tOK!");

			MAIN_add_to_log ("Loading tilemaps from data files...");
			TILEMAPS_load_game_data ();
			MAIN_add_to_log ("\tOK!");
		}
		else
		{
			MAIN_add_to_log ("\tFAILED! Falling back to verbose map data.");

			MAIN_add_to_log ("Loading tilesets from verbose files...");
			TILESETS_load_all ();
			MAIN_add_to_log ("\tOK!");

			MAIN_add_to_log ("Loading tilemaps from verbose files...");
			TILEMAPS_load_all ();
			MAIN_add_to_log ("\tOK!");
		}
#endif
	}

	if (!EDIT_tilemaps_have_valid_dimensions())
	{
		MAIN_add_to_log("Tilemaps had invalid dimensions after initial load. Reloading verbose tilemap files (non-destructive reset)...");
		// Avoid freeing potentially corrupted pointers from a bad initial load.
		cm = NULL;
		number_of_tilemaps_loaded = 0;
		TILEMAPS_load_all();

		if (!EDIT_tilemaps_have_valid_dimensions())
		{
			OUTPUT_message("Tilemap load failed: invalid map dimensions after verbose reload.");
			assert(0);
		}

		MAIN_add_to_log("\tOK!");
	}

	MAIN_draw_loading_picture ("LINKING ZONE TYPE INDEXES",30);
	
	MAIN_add_to_log ("Linking zone type indexes to names...");
	ROOMZONES_confirm_room_zone_links();
	MAIN_add_to_log ("\tOK!");

	MAIN_draw_loading_picture ("LINKING WAYPOINT INDEXES",40);

	MAIN_add_to_log ("Linking waypoint indexes to names...");
	AINODES_confirm_waypoint_links();
	MAIN_add_to_log ("\tOK!");
	
	MAIN_draw_loading_picture ("LINKING SPAWN POINTS",50);

	MAIN_add_to_log ("Linking spawn points together...");
	SPAWNPOINTS_confirm_all_links ();
	MAIN_add_to_log ("\tOK!");

	MAIN_draw_loading_picture ("LOADING PATHS",60);

	MAIN_add_to_log ("Loading paths...");
	PATHS_load_all ();
	MAIN_add_to_log ("\tOK!");

	MAIN_draw_loading_picture ("CROSS-REFERENCING RESOURCES",70);

	MAIN_add_to_log ("Cross referencing resources...");
	EDIT_confirm_links ();
	MAIN_add_to_log ("\tOK!");

	#ifdef RETRENGINE_DEBUG_VERSION
		MAIN_add_to_log ("Outputting report...");
		EDIT_output_report ();
		MAIN_add_to_log ("\tOK!");
	#endif

	MAIN_add_to_log ("Setting up Simple GUI code...");
	SIMPGUI_setup ();
	MAIN_add_to_log ("\tOK!");

	MAIN_draw_loading_picture ("SETTING UP ZONE SHORTCUT LISTS",80);

	MAIN_add_to_log ("Setting up zone shortcut lists...");
	TILEMAPS_create_real_zone_sizes ();
	TILEMAPS_create_zone_spawn_point_lists ();
	TILEMAPS_create_zone_ai_node_lists ();
	MAIN_add_to_log ("\tOK!");

	MAIN_draw_loading_picture ("SETTING UP LOCALISED ZONE LISTS",90);

	MAIN_add_to_log ("Setting up localised zone lists...");
	ROOMZONES_generate_localised_zone_lists ();
	MAIN_add_to_log ("\tOK!");

	MAIN_draw_loading_picture ("GENERATING AI NODE LOOKUP TABLES",100);
	
	MAIN_add_to_log ("Generating AI node lookup tables...");
	AINODES_create_all_waypoint_lookup_tables ();
	MAIN_add_to_log ("\tOK!");

	#ifdef RETRENGINE_DEBUG_VERSION
		MAIN_add_to_log ("Outputting AI node lookup tables...");
		AINODES_output_waypoint_lookup_tables ();
		MAIN_add_to_log ("\tOK!");
	#endif
}



void EDIT_load_icons (void)
{
	if (load_from_dat_file)
	{
		first_icon = INPUT_load_bitmap ("editor_gui.bmp");
		bitmask_gfx = INPUT_load_bitmap ("editor_bitmask.bmp");
		object_sides_gfx = INPUT_load_bitmap ("editor_object_sides.bmp");
	}
	else
	{
		first_icon = INPUT_load_bitmap ("editor/editor_gui.bmp");
		bitmask_gfx = INPUT_load_bitmap ("editor/editor_bitmask.bmp");
		object_sides_gfx = INPUT_load_bitmap ("editor/editor_object_sides.bmp");
	}

	INPUT_create_equal_sized_sprite_series (first_icon, "ICON", ICON_SIZE, ICON_SIZE, 0, 0);
	INPUT_create_equal_sized_sprite_series (bitmask_gfx, "BITMASK", ICON_SIZE, ICON_SIZE, 0, 0);
	INPUT_create_equal_sized_sprite_series (object_sides_gfx, "EDGE_BITMASK", ICON_SIZE, ICON_SIZE, 0, 0);
}



void EDIT_close_down (void)
{
	// Deletes all that malloc'ed memory...

	free (report);
	report = NULL;

	if (program_state == PROGRAM_STATE_EDITOR)
	{
		MAIN_add_to_log ("Saving paths...");
		PATHS_save_all ();
		MAIN_add_to_log ("\tOK!");

		MAIN_add_to_log ("Saving tilemaps...");
		TILEMAPS_save_all ();
		MAIN_add_to_log ("\tOK!");

		MAIN_add_to_log ("Saving tilesets...");
		TILESETS_save_all ();
		MAIN_add_to_log ("\tOK!");

		MAIN_add_to_log ("Saving compiled textfile...");
		TEXTFILE_save_compiled_text ();
		MAIN_add_to_log ("\tOK!");

		MAIN_add_to_log ("Saving editor settings...");
		TILEMAPS_save_editor_settings ();
		MAIN_add_to_log ("\tOK!");

		MAIN_add_to_log ("Saving Compiled prefabs...");
		SCRIPTING_save_compiled_prefabs();
		MAIN_add_to_log ("\tOK!");
	}

	MAIN_add_to_log ("Destroying object collision buffers tables...");
	OBCOLL_free_collision_buffers ();
	MAIN_add_to_log ("\tOK!");

	MAIN_add_to_log ("Destroying starfields...");
	PARTICLES_destroy_all_starfields ();
	MAIN_add_to_log ("\tOK!");

	MAIN_add_to_log ("Destroying window sorting queues and windows...");
	OBCOLL_free_collision_buffers ();
	MAIN_add_to_log ("\tOK!");

	MAIN_add_to_log ("Destroying tile block collision shapes and tile priority/property list...");
	WORLDCOLL_destroy_block_collision_shapes();
	MAIN_add_to_log ("\tOK!");
	
	MAIN_add_to_log ("Destroying flag arrays...");
	SCRIPTING_destroy_flag_arrays ();
	MAIN_add_to_log ("\tOK!");

	MAIN_add_to_log ("Destroying sprite list and bmp list...");
	OUTPUT_destroy_bitmap_and_sprite_containers();
	MAIN_add_to_log ("\tOK!");

	MAIN_add_to_log ("Destroying extension list...");
	PLATFORM_RENDERER_clear_extensions();
	MAIN_add_to_log ("\tOK!");

	MAIN_add_to_log ("Destroying textfiles...");
	TEXTFILE_destroy ();
	MAIN_add_to_log ("\tOK!");

	MAIN_add_to_log ("Destroying tilemaps...");
	TILEMAPS_destroy_all_maps ();
	MAIN_add_to_log ("\tOK!");

	MAIN_add_to_log ("Destroying tilesets...");
	TILESETS_destroy_all_tilesets ();
	MAIN_add_to_log ("\tOK!");

	MAIN_add_to_log ("Destroying paths...");
	PATHS_destroy_all ();
	MAIN_add_to_log ("\tOK!");

	MAIN_add_to_log ("Destroying prefabs...");
	SCRIPTING_free_prefabs();
	MAIN_add_to_log ("\tOK!");

	MAIN_add_to_log ("Destroying trig tables...");
	MATH_destroy_trig_tables();
	MAIN_add_to_log ("\tOK!");

	MAIN_add_to_log ("Destroying hiscore tables...");
	HISCORES_destroy_hiscores ();
	MAIN_add_to_log ("\tOK!");

	MAIN_add_to_log ("Destroying datatables...");
	ARRAY_destroy_datatables ();
	MAIN_add_to_log ("\tOK!");

	#ifdef RETRENGINE_DEBUG_VERSION_COMPILE_INTERACTION_TABLE
		MAIN_add_to_log ("Saving interaction table...");
		SCRIPTING_output_interaction_table();
		MAIN_add_to_log ("\tOK!");

		MAIN_add_to_log ("Saving interaction table...");
		SCRIPTING_destroy_interaction_table ();
		MAIN_add_to_log ("\tOK!");
	#endif

	MAIN_add_to_log ("Destroying Scripts...");
	SCRIPTING_destroy_all_scripts ();
	MAIN_add_to_log ("\tOK!");

	MAIN_add_to_log ("Destroying All Entity's Arrays...");
	SCRIPTING_destroy_all_entity_arrays ();
	MAIN_add_to_log ("\tOK!");

	MAIN_add_to_log ("Destroying Global Parameter Lists...");
	GPL_destroy_lists ();
	MAIN_add_to_log ("\tOK!");
}



/*

void EDIT_get_path_offset (int path_number, float old_path_pos, float new_path_pos, float *x_vel, float *y_vel, float *angle)
{
	// Find the position of the old path offset, and the new one and store the differences in the given variables.

	float x1,y1,x2,y2,angle1,angle2;
	float start_section_remainder,end_section_remainder;
	float start_section,end_section;

	start_section = old_path_pos / lookup_path_resolution[path_number];
	end_section = new_path_pos / lookup_path_resolution[path_number];

	start_section_remainder = start_section - int(start_section);
	end_section_remainder = end_section - int(end_section);

	x1 = lookup_path [path_number][int(start_section)][0] + MATH_lerp ( lookup_path [path_number][int(start_section)][0] , lookup_path [path_number][int(start_section+1)][0] , start_section_remainder );
	y1 = lookup_path [path_number][int(start_section)][1] + MATH_lerp ( lookup_path [path_number][int(start_section)][1] , lookup_path [path_number][int(start_section+1)][1] , start_section_remainder );
	angle1 = lookup_path [path_number][int(start_section)][2] + MATH_lerp ( lookup_path [path_number][int(start_section)][2] , lookup_path [path_number][int(start_section+1)][2] , start_section_remainder );

	x2 = lookup_path [path_number][int(end_section)][0] + MATH_lerp ( lookup_path [path_number][int(end_section)][0] , lookup_path [path_number][int(end_section+1)][0] , end_section_remainder );
	y2 = lookup_path [path_number][int(end_section)][1] + MATH_lerp ( lookup_path [path_number][int(end_section)][1] , lookup_path [path_number][int(end_section+1)][1] , end_section_remainder );
	angle2 = lookup_path [path_number][int(end_section)][2] + MATH_lerp ( lookup_path [path_number][int(end_section)][2] , lookup_path [path_number][int(end_section+1)][2] , end_section_remainder );

	*x_vel = x2-x1;
	*y_vel = y2-y1;
	*angle = angle2 - angle1;
}

*/

/*
	Pathfinding options...

	Okay, the main difference between having a nice lookup table and actually doing the path-finding live
	is that you use a lookup then you can't have beliefs factored into the system as that's just incompatible
	with the lookup approach.
	
	Obviously the lovely thing about the lookup system is that it makes pathfinding completely trivial and not
	a processing overhead at all. Even the re-calculation of a part of the zone network doesn't take that long
	and so it's all hunky-dory. And by breaking down the map into smaller zone-groups you stop it using up too
	much memory.

	One way we can get around the computational load issues with doing it live is by first of all making sure
	the data we produce is used by as many entities as need it (ie, not recalcing the same stuff for each request
	to get to zone X from the same group) and also by making sure that only a set number of different requests
	are served each frame, meaning that the requester needs to have some kind of internal flag so that after asking
	for data it can sit around thinking until that data pops into it's in-tray.
	
	Every time a change to the map is made which alters the node connections.
	
	You know, I think that rather than zones, a system of nodes might be better.

	I wonder how difficult it would be to identify and place zones automatically in a map? Actually pretty
	easy, now that I think about it. Although gateways will have to be manually placed. Identifying those areas
	which fall into a particular zone will be tricky...

	How about just defining all zones by what side of a gate they are on.

	I think we need to split things up a little more into activation zones, visibility zones and pathfinding zones.

	Hmm, for cul-de-sac areas you don't need the accompanying neighbour table because anything that isn't in the
	zone, so they can be flagged as such and anything with a number outside the range of the zone is told to go to
	the exit in an orderly fashion.

	For visibility zones any positive value is a zone number and a zero is an unbreachable boundary.
*/



#define LINES_ON_SCREEN			(50)
#define OVERVIEW_MENU_GROUP_ID	(1)

bool EDIT_editor_overview (bool initialise)
{
	// This is where you create tilemaps and tilesets, as if by magic. Woooooo!

	static int tilemap_list_start,tilemap_list_end;
	static int tileset_list_start,tileset_list_end;

	static int first_tilemap_in_list;
	static int first_tileset_in_list; // Variables for the scrollbars to hang on to.

	int chosen_tilemap;
	int chosen_tileset;

	int number;

	int mx,my;

	mx = CONTROL_mouse_x();
	my = CONTROL_mouse_y();

	if (initialise)
	{
		initialise = false;

		first_tilemap_in_list = 0;
		first_tileset_in_list = 0;
	}
	else
	{
		OUTPUT_clear_screen();

		GPL_list_extents ( "TILEMAPS" , &tilemap_list_start , &tilemap_list_end );
		GPL_list_extents ( "TILESETS" , &tileset_list_start , &tileset_list_end );

		SIMPGUI_check_all_buttons ();
		SIMPGUI_draw_all_buttons ();

		OUTPUT_rectangle (0 , 0 , 319 , 479, 0, 0, 255);
		OUTPUT_rectangle (320 , 0 , 639 , 479, 0, 0, 255);
		OUTPUT_rectangle (0 , 32 , 319 , 64, 0, 0, 255);
		OUTPUT_rectangle (320 , 32 , 639 , 64, 0, 0, 255);

		OUTPUT_rectangle (0 , 0 , 159 , 31, 0, 0, 255);
		OUTPUT_rectangle (160 , 0 , 319 , 31, 0, 0, 255);
		OUTPUT_rectangle (320 , 0 , 479 , 31, 0, 0, 255);
		OUTPUT_rectangle (480 , 0 , 639 , 31, 0, 0, 255);

		OUTPUT_centred_text (80, 8, "CREATE", 0 , 255 , 255 );
		OUTPUT_centred_text (80, 16, "TILEMAP!", 0 , 255 , 255 );

		OUTPUT_centred_text (240, 8, "CREATE", 0 , 255 , 255 );
		OUTPUT_centred_text (240, 16, "TILESET!", 0 , 255 , 255 );

		OUTPUT_centred_text (400, 8, "RELOAD", 0 , 255 , 255 );
		OUTPUT_centred_text (400, 16, "SYSTEM", 0 , 255 , 255 );

		OUTPUT_centred_text (560, 8, "EXIT", 0 , 255 , 255 );
		OUTPUT_centred_text (560, 16, "EDITOR", 0 , 255 , 255 );

		OUTPUT_centred_text (160, 44, "----TILEMAPS----", 0 , 255 , 255 );
		OUTPUT_centred_text (480, 44, "----TILESETS----", 0 , 255 , 0 );

		chosen_tilemap = GPL_draw_list ("TILEMAPS" , 8 , 72 , 256 , LINES_ON_SCREEN*8 , first_tilemap_in_list , mx , my , true, 8, 0, 192, 192);
		chosen_tileset = GPL_draw_list ("TILESETS" , 328 , 72 , 256 , LINES_ON_SCREEN*8 , first_tileset_in_list , mx , my , true, 8, 0, 192, 0);

		if (CONTROL_mouse_hit (LMB) == true)
		{
			if (chosen_tilemap != UNSET)
			{
				initialise = true;
				cm[chosen_tilemap].altered_this_load = true;
				initialise = TILEMAPS_edit ( initialise , int (GPL_get_value_by_index (chosen_tilemap)) );
				game_state = GAME_STATE_EDITOR;
			}

			if (chosen_tileset != UNSET)
			{
				initialise = true;
				ts[chosen_tileset].altered_this_load = true;
				initialise = TILESETS_edit ( initialise , int (GPL_get_value_by_index (chosen_tileset)) );
				game_state = GAME_STATE_EDIT_TILESET;
			}

			if (MATH_rectangle_to_point_intersect ( 0 , 0 , 160 , 32 , mx, my ) == true)
			{
				// New Tilemap
				number = TILEMAPS_create (true);
				GPL_add_to_global_parameter_list ("TILEMAPS" , TILEMAPS_get_name(number) , number );
				if (number_of_tilemaps_loaded > LINES_ON_SCREEN)
				{
					// Then we'll need a drag-bar in order to see all of them. Cripes!
					// Delete the old one first, though.
					SIMPGUI_kill_button (OVERVIEW_MENU_GROUP_ID, 0);
					SIMPGUI_create_v_drag_bar (OVERVIEW_MENU_GROUP_ID, 0, &first_tilemap_in_list , 320-32, 64, 32, 480-64 , 0, number_of_tilemaps_loaded - LINES_ON_SCREEN, 10, 64, LMB);
				}
			}

			if (MATH_rectangle_to_point_intersect ( 160 , 0 , 320 , 32 , mx, my ) == true)
			{
				// New Tileset
				number = TILESETS_create (true);
				GPL_add_to_global_parameter_list ("TILESETS" , TILESETS_get_name(number) , number);
				if (number_of_tilesets_loaded > LINES_ON_SCREEN)
				{
					// Then we'll need a drag-bar in order to see all of them. Cripes!
					// Delete the old one first, though.
					SIMPGUI_kill_button (OVERVIEW_MENU_GROUP_ID, 1);
					SIMPGUI_create_v_drag_bar (OVERVIEW_MENU_GROUP_ID, 1, &first_tileset_in_list , 640-32, 64, 32, 480-64 , 0, number_of_tilesets_loaded - LINES_ON_SCREEN, 10, 64, LMB);
				}
			}

			if (MATH_rectangle_to_point_intersect ( 320 , 0 , 480 , 32 , mx, my ) == true)
			{
				// Restart system
			}

			if (MATH_rectangle_to_point_intersect ( 480 , 0 , 640 , 32 , mx, my ) == true)
			{
				// Exit editor!
				game_state = GAME_STATE_EXIT_GAME;
			}
		}

		if (0)
		{
			// Created a new tilemap, so basically we need to shut down the system and re-start
			// so that everything is alphabetised and points at the right thing.

			// Have a button to do this for preference so it's not automatic.
		}

		OUTPUT_draw_masked_sprite ( first_icon , SELECT_ARROW , mx, my );

		if (CONTROL_key_hit (KEY_ESC) == true)
		{
			game_state = GAME_STATE_EXIT_GAME;
			initialise = true;
		}
	}

	return initialise;
}



fill_spreader * EDIT_add_filler (int list_size, fill_spreader *list_pointer, int x, int y, int value)
{
	// This simply adds a pair of co-ordinates and possibly a value to a dynamically allocated list
	// and returns its new pointer. It for things like flood-fill routines or whatever else you might
	// want (ie, pathfinding et al).

	if ( (list_size == 0) && (list_pointer == NULL) )
	{
		list_pointer = (fill_spreader *) malloc (sizeof (fill_spreader) );
	}
	else
	{
		list_pointer = (fill_spreader *) realloc (list_pointer , (list_size+1) * sizeof (fill_spreader) );
	}

	list_pointer[list_size].x = x;
	list_pointer[list_size].y = y;
	list_pointer[list_size].value = value;

	return list_pointer;
}



#define GPL_LIST_MENU_LEFT_WIDTH		(180)
#define GPL_LIST_MENU_RIGHT_WIDTH		(480-GPL_LIST_MENU_LEFT_WIDTH)
#define GPL_LIST_MENU_HEIGHT			(384)
#define GPL_LIST_MENU_HEADER_SIZE		(16)

bool EDIT_gpl_list_menu (int x, int y, char *list_name , char *list_entry, bool fixed_list)
{
	static int first_in_list_list = 0;
	static int first_in_word_list = 0;
	static bool editing_number = false;
	static int number = 0;

	OUTPUT_filled_rectangle (x,y,x+GPL_LIST_MENU_LEFT_WIDTH+GPL_LIST_MENU_RIGHT_WIDTH,y+GPL_LIST_MENU_HEIGHT,0,0,0);
	OUTPUT_rectangle (x,y,x+GPL_LIST_MENU_LEFT_WIDTH+GPL_LIST_MENU_RIGHT_WIDTH,y+GPL_LIST_MENU_HEIGHT,0,0,255);
	OUTPUT_rectangle (x,y,x+GPL_LIST_MENU_LEFT_WIDTH,y+GPL_LIST_MENU_HEIGHT,0,0,255);
	OUTPUT_rectangle (x,y,x+GPL_LIST_MENU_LEFT_WIDTH+GPL_LIST_MENU_RIGHT_WIDTH,y+GPL_LIST_MENU_HEADER_SIZE,0,0,255);

//	OUTPUT_centred_text( x + GPL_LIST_MENU_WIDTH/4 , y + FONT_SIZE/2 + GPL_LIST_MENU_HEIGHT , list_name->name_struct_name , 255,255,255 );
//	OUTPUT_centred_text( x + GPL_LIST_MENU_WIDTH/4 + GPL_LIST_MENU_WIDTH/2 , y + FONT_SIZE/2 + GPL_LIST_MENU_HEIGHT , list_entry->name_struct_name , 255,255,255 );

	OUTPUT_centred_text( x + GPL_LIST_MENU_LEFT_WIDTH/2 , y + FONT_HEIGHT/2 , "LIST" , 255,255,255 );
	OUTPUT_centred_text( x + GPL_LIST_MENU_RIGHT_WIDTH/2 + GPL_LIST_MENU_LEFT_WIDTH , y + FONT_HEIGHT/2 , "ENTRIES" , 255,255,255 );

	int start,end;
	int list_list_length;
	int word_list_length;
	int visible_entries;
	int t;
	int list_list_selection;
	int word_list_selection;
	int list_selected;

	int list_list_drag_bar_exists;
	int word_list_drag_bar_exists;

	int current_list_list;
	int current_word_list;

	int selected_number_option;

	int red,green,blue;

	char number_word[NAME_SIZE];

	int mx,my;

	int entry;

	int diff;

	int searching_for_entry;
	int found_greater_entry;
	char *searching_name_pointer;

	selected_number_option = UNSET;

	mx = CONTROL_mouse_x();
	my = CONTROL_mouse_y();

	if (strcmp (list_name,"NUMBER") == 0)
	{
		editing_number = true;
		number = atoi (list_entry);
	}

	if (editing_number == false)
	{
		list_list_length = GPL_how_many_lists();

		current_list_list = GPL_what_is_list_number (list_name);
		current_word_list = GPL_find_word_index (list_name , list_entry);

		GPL_list_extents (list_name, &start, &end);

		word_list_length = end-start;
	}
	else
	{
		list_list_length = GPL_how_many_lists();

		current_list_list = list_list_length;
		current_word_list = UNSET;

		start = 0;
		end = 0;

		word_list_length = end-start;
	}

	visible_entries = ( (GPL_LIST_MENU_HEIGHT - GPL_LIST_MENU_HEADER_SIZE) / FONT_HEIGHT ) - 1;

	if (list_list_length > visible_entries)
	{
		list_list_drag_bar_exists = 16;
		if (SIMPGUI_does_button_exist (TILESET_SUB_GROUP_ID,IMPORTANT_GPL_MENU_1) == false)
		{
			SIMPGUI_create_v_drag_bar (TILESET_SUB_GROUP_ID, IMPORTANT_GPL_MENU_1, &first_in_list_list, x+(GPL_LIST_MENU_LEFT_WIDTH)-16, y+GPL_LIST_MENU_HEADER_SIZE, 16, GPL_LIST_MENU_HEIGHT-GPL_LIST_MENU_HEADER_SIZE , 0 , (list_list_length - visible_entries) + 1, 1 , 32 , LMB );
		}
	}
	else
	{
		list_list_drag_bar_exists = 0;
		SIMPGUI_kill_button (TILESET_SUB_GROUP_ID, IMPORTANT_GPL_MENU_1);
	}

	if (word_list_length > visible_entries)
	{
		word_list_drag_bar_exists = 16;
		if (SIMPGUI_does_button_exist (TILESET_SUB_GROUP_ID,IMPORTANT_GPL_MENU_2) == false)
		{
			SIMPGUI_create_v_drag_bar (TILESET_SUB_GROUP_ID, IMPORTANT_GPL_MENU_2, &first_in_word_list, x+(GPL_LIST_MENU_LEFT_WIDTH+GPL_LIST_MENU_RIGHT_WIDTH)-16, y+GPL_LIST_MENU_HEADER_SIZE, 16, GPL_LIST_MENU_HEIGHT-GPL_LIST_MENU_HEADER_SIZE , 0 , (word_list_length - visible_entries) + 1, 1 , 32 , LMB );
		}
	}
	else
	{
		word_list_drag_bar_exists = 0;
		SIMPGUI_kill_button (TILESET_SUB_GROUP_ID, IMPORTANT_GPL_MENU_2);
	}

	// What's the mouse pointing at?

	if ( (my > y+(FONT_HEIGHT/2)+GPL_LIST_MENU_HEADER_SIZE) && (my < y+GPL_LIST_MENU_HEIGHT-(FONT_HEIGHT/2) ) )
	{
		list_list_selection = (CONTROL_mouse_y() - (y+(FONT_HEIGHT/2)+GPL_LIST_MENU_HEADER_SIZE)) / FONT_HEIGHT + first_in_list_list;
		word_list_selection = (CONTROL_mouse_y() - (y+(FONT_HEIGHT/2)+GPL_LIST_MENU_HEADER_SIZE)) / FONT_HEIGHT + first_in_word_list;
	}
	else
	{
		list_list_selection = UNSET;
		word_list_selection = UNSET;
	}

	if ( ( mx > x+(FONT_WIDTH/2) ) && ( mx < x+(GPL_LIST_MENU_LEFT_WIDTH)-(FONT_WIDTH/2)-list_list_drag_bar_exists ) )
	{
		list_selected = 0;
	}
	else if ( ( mx > x+(FONT_WIDTH/2)+(GPL_LIST_MENU_LEFT_WIDTH) ) && ( mx < x+(GPL_LIST_MENU_LEFT_WIDTH+GPL_LIST_MENU_RIGHT_WIDTH)-(FONT_WIDTH/2)-word_list_drag_bar_exists ) )
	{
		list_selected = 1;
	}
	else
	{
		list_selected = UNSET;
	}

	// Draw the lists...
		
	for (t=0 ; (t<=list_list_length) && (t<visible_entries); t++)
	{
		entry = t+first_in_list_list;

		if ( (list_list_selection == entry) && (list_selected == 0) && (fixed_list == false) )
		{
			red = 255;
			green = 255;
			blue = 255;
		}
		else if (current_list_list == entry)
		{
			red = 0;
			green = 255;
			blue = 255;
		}
		else
		{
			if (fixed_list == true)
			{
				red = 255;
			}
			else
			{
				red = 0;
			}
			green = 0;
			blue = 255;
		}

		if (entry<list_list_length)
		{
			OUTPUT_truncated_text ( ((GPL_LIST_MENU_LEFT_WIDTH)-(FONT_WIDTH)-list_list_drag_bar_exists)/FONT_WIDTH , x+(FONT_WIDTH/2) , y+(FONT_HEIGHT/2)+GPL_LIST_MENU_HEADER_SIZE+(t*FONT_HEIGHT) , GPL_what_is_list_name(t + first_in_list_list) , red , green , blue );
		}
		else if (entry==list_list_length)
		{
			OUTPUT_truncated_text ( ((GPL_LIST_MENU_LEFT_WIDTH)-(FONT_WIDTH)-list_list_drag_bar_exists)/FONT_WIDTH , x+(FONT_WIDTH/2) , y+(FONT_HEIGHT/2)+GPL_LIST_MENU_HEADER_SIZE+(t*FONT_HEIGHT) , "NUMBER" , red , green , blue );
		}
	}

	if (editing_number == false)
	{
		for (t=0 ; (t<=word_list_length) && (t<visible_entries); t++)
		{
			entry = t+first_in_word_list;

			if ( (word_list_selection == entry) && (list_selected == 1) )
			{
				red = 255;
				green = 255;
				blue = 255;
			}
			else if (current_word_list == start + entry)
			{
				red = 255;
				green = 255;
				blue = 0;
			}
			else
			{
				red = 0;
				green = 255;
				blue = 0;
			}

			if (entry<word_list_length)
			{
				OUTPUT_truncated_text ( ((GPL_LIST_MENU_RIGHT_WIDTH)-(FONT_WIDTH)-word_list_drag_bar_exists)/FONT_WIDTH , x+(FONT_WIDTH/2)+(GPL_LIST_MENU_LEFT_WIDTH) , y+(FONT_HEIGHT/2)+GPL_LIST_MENU_HEADER_SIZE+(t*FONT_HEIGHT) , GPL_what_is_word_name(t + first_in_word_list + start) , red , green , blue );
			}
			else if (entry==word_list_length)
			{
				OUTPUT_truncated_text ( ((GPL_LIST_MENU_RIGHT_WIDTH)-(FONT_WIDTH)-word_list_drag_bar_exists)/FONT_WIDTH , x+(FONT_WIDTH/2)+(GPL_LIST_MENU_LEFT_WIDTH) , y+(FONT_HEIGHT/2)+GPL_LIST_MENU_HEADER_SIZE+(t*FONT_HEIGHT) , "UNSET" , 255 , 0 , 0 );
			}

			// Only if we're looking at the TEXT word list.
			if ( strcmp (list_name , "TEXT") == 0 )
			{
				if ( (word_list_selection == entry) && (word_list_selection < word_list_length) )
				{
					OUTPUT_filled_rectangle ( x , y+GPL_LIST_MENU_HEIGHT , x+GPL_LIST_MENU_LEFT_WIDTH+GPL_LIST_MENU_RIGHT_WIDTH , y+GPL_LIST_MENU_HEIGHT+(FONT_HEIGHT*2) , 0 , 0 , 0 );
					OUTPUT_rectangle ( x , y+GPL_LIST_MENU_HEIGHT , x+(GPL_LIST_MENU_LEFT_WIDTH+GPL_LIST_MENU_RIGHT_WIDTH) , y+GPL_LIST_MENU_HEIGHT+(FONT_HEIGHT*2) , 0 , 0 , 255 );
					OUTPUT_truncated_text ( ((GPL_LIST_MENU_LEFT_WIDTH+GPL_LIST_MENU_RIGHT_WIDTH) / FONT_WIDTH) - 1 , x+(FONT_WIDTH/2) , y + GPL_LIST_MENU_HEIGHT + (FONT_HEIGHT/2) , TEXTFILE_get_line_by_index(word_list_selection) );
				}
			}
		}
	}
	else
	{
		sprintf (number_word,"%d",number);
		OUTPUT_centred_text ( x+(GPL_LIST_MENU_LEFT_WIDTH)+(GPL_LIST_MENU_RIGHT_WIDTH/2) , y+(FONT_HEIGHT/2)+GPL_LIST_MENU_HEADER_SIZE+(FONT_HEIGHT*1) , number_word , 255 , 255 , 255 );

		OUTPUT_centred_text ( x+(GPL_LIST_MENU_LEFT_WIDTH)+(GPL_LIST_MENU_RIGHT_WIDTH/2) , y+(FONT_HEIGHT/2)+GPL_LIST_MENU_HEADER_SIZE+(FONT_HEIGHT*9) , "DELETE" , 255 , 255 , 0 );
		OUTPUT_centred_text ( x+(GPL_LIST_MENU_LEFT_WIDTH)+(GPL_LIST_MENU_RIGHT_WIDTH/2)-(GPL_LIST_MENU_RIGHT_WIDTH/3) , y+(FONT_HEIGHT/2)+GPL_LIST_MENU_HEADER_SIZE+(FONT_HEIGHT*9) , "CLEAR" , 255 , 255 , 0 );
		OUTPUT_centred_text ( x+(GPL_LIST_MENU_LEFT_WIDTH)+(GPL_LIST_MENU_RIGHT_WIDTH/2)+(GPL_LIST_MENU_RIGHT_WIDTH/3) , y+(FONT_HEIGHT/2)+GPL_LIST_MENU_HEADER_SIZE+(FONT_HEIGHT*9) , "ACCEPT" , 255 , 255 , 0 );

		OUTPUT_rectangle ( x+(GPL_LIST_MENU_LEFT_WIDTH)+(GPL_LIST_MENU_RIGHT_WIDTH/2)-(FONT_WIDTH*4) , y+(FONT_HEIGHT/2)+GPL_LIST_MENU_HEADER_SIZE+(FONT_HEIGHT*9)-(FONT_HEIGHT*1) , x+(GPL_LIST_MENU_LEFT_WIDTH)+(GPL_LIST_MENU_RIGHT_WIDTH/2)+(FONT_WIDTH*4) , y+(FONT_HEIGHT/2)+GPL_LIST_MENU_HEADER_SIZE+(FONT_HEIGHT*9)+(FONT_HEIGHT*2) , 0 , 255 , 0 );
		OUTPUT_rectangle ( x+(GPL_LIST_MENU_LEFT_WIDTH)+(GPL_LIST_MENU_RIGHT_WIDTH/2)-(GPL_LIST_MENU_RIGHT_WIDTH/3)-(FONT_WIDTH*4) , y+(FONT_HEIGHT/2)+GPL_LIST_MENU_HEADER_SIZE+(FONT_HEIGHT*9)-(FONT_HEIGHT*1) , x+(GPL_LIST_MENU_LEFT_WIDTH)+(GPL_LIST_MENU_RIGHT_WIDTH/2)-(GPL_LIST_MENU_RIGHT_WIDTH/3)+(FONT_WIDTH*4) , y+(FONT_HEIGHT/2)+GPL_LIST_MENU_HEADER_SIZE+(FONT_HEIGHT*9)+(FONT_HEIGHT*2) , 0 , 255 , 0 );
		OUTPUT_rectangle ( x+(GPL_LIST_MENU_LEFT_WIDTH)+(GPL_LIST_MENU_RIGHT_WIDTH/2)+(GPL_LIST_MENU_RIGHT_WIDTH/3)-(FONT_WIDTH*4) , y+(FONT_HEIGHT/2)+GPL_LIST_MENU_HEADER_SIZE+(FONT_HEIGHT*9)-(FONT_HEIGHT*1) , x+(GPL_LIST_MENU_LEFT_WIDTH)+(GPL_LIST_MENU_RIGHT_WIDTH/2)+(GPL_LIST_MENU_RIGHT_WIDTH/3)+(FONT_WIDTH*4) , y+(FONT_HEIGHT/2)+GPL_LIST_MENU_HEADER_SIZE+(FONT_HEIGHT*9)+(FONT_HEIGHT*2) , 0 , 255 , 0 );

		if ( MATH_rectangle_to_point_intersect ( x+(GPL_LIST_MENU_LEFT_WIDTH)+(GPL_LIST_MENU_RIGHT_WIDTH/2)-(FONT_WIDTH*4) , y+(FONT_HEIGHT/2)+GPL_LIST_MENU_HEADER_SIZE+(FONT_HEIGHT*9)-(FONT_HEIGHT*1) , x+(GPL_LIST_MENU_LEFT_WIDTH)+(GPL_LIST_MENU_RIGHT_WIDTH/2)+(FONT_WIDTH*4) , y+(FONT_HEIGHT/2)+GPL_LIST_MENU_HEADER_SIZE+(FONT_HEIGHT*9)+(FONT_HEIGHT*2) , mx , my ) == true )
		{
			selected_number_option = 11;
		}

		if ( MATH_rectangle_to_point_intersect ( x+(GPL_LIST_MENU_LEFT_WIDTH)+(GPL_LIST_MENU_RIGHT_WIDTH/2)-(GPL_LIST_MENU_RIGHT_WIDTH/3)-(FONT_WIDTH*4) , y+(FONT_HEIGHT/2)+GPL_LIST_MENU_HEADER_SIZE+(FONT_HEIGHT*9)-(FONT_HEIGHT*1) , x+(GPL_LIST_MENU_LEFT_WIDTH)+(GPL_LIST_MENU_RIGHT_WIDTH/2)-(GPL_LIST_MENU_RIGHT_WIDTH/3)+(FONT_WIDTH*4) , y+(FONT_HEIGHT/2)+GPL_LIST_MENU_HEADER_SIZE+(FONT_HEIGHT*9)+(FONT_HEIGHT*2) , mx , my ) == true )
		{
			selected_number_option = 10;
		}

		if ( MATH_rectangle_to_point_intersect ( x+(GPL_LIST_MENU_LEFT_WIDTH)+(GPL_LIST_MENU_RIGHT_WIDTH/2)+(GPL_LIST_MENU_RIGHT_WIDTH/3)-(FONT_WIDTH*4) , y+(FONT_HEIGHT/2)+GPL_LIST_MENU_HEADER_SIZE+(FONT_HEIGHT*9)-(FONT_HEIGHT*1) , x+(GPL_LIST_MENU_LEFT_WIDTH)+(GPL_LIST_MENU_RIGHT_WIDTH/2)+(GPL_LIST_MENU_RIGHT_WIDTH/3)+(FONT_WIDTH*4) , y+(FONT_HEIGHT/2)+GPL_LIST_MENU_HEADER_SIZE+(FONT_HEIGHT*9)+(FONT_HEIGHT*2) , mx , my ) == true )
		{
			selected_number_option = 12;
		}

		for (t=0; t<10; t++)
		{
			sprintf (number_word,"%d",t);
			OUTPUT_text ( x+(FONT_WIDTH/2)+(GPL_LIST_MENU_LEFT_WIDTH)+(t*FONT_WIDTH*2)+(FONT_WIDTH*5) , y+(FONT_HEIGHT/2)+GPL_LIST_MENU_HEADER_SIZE+(FONT_HEIGHT*5) , number_word , 0 , 255 , 255 );
			OUTPUT_rectangle ( x+(GPL_LIST_MENU_LEFT_WIDTH)+(t*FONT_WIDTH*2)+(FONT_WIDTH*5) , y+GPL_LIST_MENU_HEADER_SIZE+(FONT_HEIGHT*5) , x+(GPL_LIST_MENU_LEFT_WIDTH)+(t*FONT_WIDTH*2)+(FONT_WIDTH*7) , y+GPL_LIST_MENU_HEADER_SIZE+(FONT_HEIGHT*7) , 0 , 255 , 0 );
			if ( MATH_rectangle_to_point_intersect (x+(GPL_LIST_MENU_LEFT_WIDTH)+(t*FONT_WIDTH*2)+(FONT_WIDTH*5) , y+GPL_LIST_MENU_HEADER_SIZE+(FONT_HEIGHT*5) , x+(GPL_LIST_MENU_LEFT_WIDTH)+(t*FONT_WIDTH*2)+(FONT_WIDTH*7) , y+GPL_LIST_MENU_HEADER_SIZE+(FONT_HEIGHT*7) , mx , my ) == true )
			{
				selected_number_option = t;
			}
		}
	}

	// Allow pressing of letters to quick-skip first_in_word_list to the first with that beginning letter if it exists.
	
	if (editing_number == false)
	{
		for (t=0; t<26; t++)
		{
			if (CONTROL_key_hit(KEY_A + t) == true)
			{
				found_greater_entry = UNSET;

				for ( searching_for_entry=0 ; searching_for_entry<word_list_length ; searching_for_entry++)
				{
					searching_name_pointer = GPL_what_is_word_name(searching_for_entry + start);
					if ( ( searching_name_pointer[0] >= (t+'A') ) && (found_greater_entry == UNSET) )
					{
						found_greater_entry = searching_for_entry;
					}
				}

				if (found_greater_entry != UNSET)
				{
					first_in_word_list = found_greater_entry;
					CONTROL_position_mouse ( x + GPL_LIST_MENU_LEFT_WIDTH + FONT_WIDTH , y + GPL_LIST_MENU_HEADER_SIZE + FONT_HEIGHT );
				}

				if (first_in_word_list + visible_entries > word_list_length+1)
				{
					diff = first_in_word_list - (word_list_length+1-visible_entries);
					CONTROL_position_mouse ( x + GPL_LIST_MENU_LEFT_WIDTH + FONT_WIDTH , y + GPL_LIST_MENU_HEADER_SIZE + FONT_HEIGHT + (diff*FONT_HEIGHT) );

					first_in_word_list = word_list_length+1-visible_entries;
				}

			}
		}
	}

	// Deal with mouse clicks

	if (CONTROL_mouse_hit (LMB) )
	{
		if ( (list_selected == 0) && (fixed_list == false) )
		{
			if ( (list_list_selection != UNSET) && (list_list_selection < list_list_length) )
			{
				if (list_list_selection != current_list_list)
				{
					strcpy (list_name , GPL_what_is_list_name (list_list_selection) );
					strcpy (list_entry , "UNSET");
					first_in_word_list = 0;
					SIMPGUI_kill_button (TILESET_SUB_GROUP_ID, IMPORTANT_GPL_MENU_2);
					editing_number = false;
				}
			}
			else if (list_list_selection == list_list_length) // You've clicked just below the list which is where the NUMBER list is.
			{
				if (list_list_selection != current_list_list)
				{
					strcpy (list_name , "NUMBER" );
					strcpy (list_entry , "UNSET");
					first_in_word_list = 0;
					SIMPGUI_kill_button (TILESET_SUB_GROUP_ID, IMPORTANT_GPL_MENU_2);
					editing_number = true;
				}
			}
		}
		else if (list_selected == 1)
		{
			if ( (word_list_selection != UNSET) && (word_list_selection < word_list_length) && (editing_number == false) )
			{
				strcpy (list_entry , GPL_what_is_word_name (word_list_selection + start) );
				SIMPGUI_kill_button (TILESET_SUB_GROUP_ID, IMPORTANT_GPL_MENU_1);
				SIMPGUI_kill_button (TILESET_SUB_GROUP_ID, IMPORTANT_GPL_MENU_2);
				first_in_word_list = 0;
				first_in_list_list = 0;
				return false;
			}
			else if ( (word_list_selection == word_list_length) && (editing_number == false) )
			{
				strcpy (list_entry , "UNSET" );
				SIMPGUI_kill_button (TILESET_SUB_GROUP_ID, IMPORTANT_GPL_MENU_1);
				SIMPGUI_kill_button (TILESET_SUB_GROUP_ID, IMPORTANT_GPL_MENU_2);
				first_in_word_list = 0;
				first_in_list_list = 0;
				return false;
			}
			else if (editing_number == true)
			{
				if ( (selected_number_option >= 0) && (selected_number_option <= 9) )
				{
					number *= 10;
					number += selected_number_option;
				}

				if (selected_number_option == 10)
				{
					number = 0;
				}

				if (selected_number_option == 11)
				{
					number /= 10;
				}

				sprintf(number_word, "%d", number);
				strcpy (list_entry , number_word );

				if (selected_number_option == 12)
				{
					SIMPGUI_kill_button (TILESET_SUB_GROUP_ID, IMPORTANT_GPL_MENU_1);
					number = 0;
					editing_number = false;
					first_in_word_list = 0;
					first_in_list_list = 0;
					return false;
				}

			}
		}
	}

	SIMPGUI_draw_all_buttons (TILESET_SUB_GROUP_ID);

	return true;

}
