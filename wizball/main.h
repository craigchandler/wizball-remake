#ifndef _MAIN_H_
#define _MAIN_H_

extern int game_state;

#define GAME_STATE_MENU					(0) // This is the main menu which just lets you start the game or editor
#define GAME_STATE_EDITOR				(1)
#define GAME_STATE_EDITOR_OVERVIEW		(2) // This comes before the editor and allows you to create and destroy tilemaps and tilesets, which is jolly exciting.
#define GAME_STATE_GAME					(3)
#define GAME_STATE_REINITIALISE			(4) // This calls the routine to save everything, then destroy everything, then load everything again.
#define GAME_STATE_EDIT_TILESET			(5)

#define PROGRAM_STATE_GAME			(0)
#define PROGRAM_STATE_EDITOR		(1)

#define WINDOWED_FALSE				(0)
#define WINDOWED_TRUE				(1)

#define GAME_STATE_EXIT_GAME			(99)

#define FULL_PROJECT_NAME_LENGTH		(256)

extern char wheres_wally[128];
extern int frames_so_far;

void MAIN_start_log (void);
void MAIN_add_to_log (char *text);

void MAIN_reset_game_trigger (void);

void MAIN_draw_loading_picture (char *description, int int_progress);

char * MAIN_get_project_filename (char *filename, bool writeable = false);
char * MAIN_get_project_name (void);

void MAIN_set_next_window_state (bool value);
void MAIN_set_next_colour_depth (int value);

void MAIN_load_config (void);

void MAIN_debug_last_thing (char *message);

void MAIN_reset_frame_counter (void);

int MAIN_get_window_size_multiple (void);


//#define DEBUG_RAND_LOGGING			(1)

#ifdef DEBUG_RAND_LOGGING

void MAIN_reset_rand_log (void);

void MAIN_log_rand (int random_value, int line_number, int entity_script_number, int frame_number);

void MAIN_save_rand_log (bool playback);

#endif





extern int program_state;

extern int virtual_screen_width;
extern int virtual_screen_height;

extern bool draw_debug_overlay;
extern bool parse_all_data;

extern bool run_windowed;
extern int colour_depth;

extern bool output_debug_information;

extern bool load_from_dat_file;

extern int demo_file_index;

extern bool release_mode;



#ifndef RELEASE_MODE

	#define RETRENGINE_DEBUG_VERSION								(1)
	#define RETRENGINE_DEBUG_VERSION_WHERES_WALLY					(1)
	#define RETRENGINE_DEBUG_VERSION_WATCH_LIST						(1)
	#define RETRENGINE_DEBUG_VERSION_CHECK_VARIABLE_SCOPE			(1)
	#define RETRENGINE_DEBUG_VERSION_COMPILE_INTERACTION_TABLE		(1)
	#define RETRENGINE_DEBUG_VERSION_THE_LAST_THING_I_DID			(1)
	#define RETRENGINE_DEBUG_VERSION_SCRIPT_TRACER					(1)
	#define RETRENGINE_DEBUG_VERSION_FORGOTTEN_SET_PRIVATE			(1)
	#define RETRENGINE_DEBUG_CHECK_ADDITIONS_TO_COLLISION_BUCKETS	(1)
	#define RETRENGINE_DEBUG_VERSION_ENTITY_DEBUG_FLAG_CHECKS		(1)
	#define RETRENGINE_DEBUG_CHECK_REMOVALS_FROM_COLLISION_BUCKETS	(1)

	#define DEBUG_INTERACTION_READ									(1)
	#define DEBUG_INTERACTION_WRITE									(2)
	#define DEBUG_INTERACTION_KILL									(4)

	#define DEBUG_FLAG_STOP_WHEN_TESTED_FOR_DRAWING					(1)
	#define DEBUG_FLAG_STOP_WHEN_Y_SORTED							(2)
	#define DEBUG_FLAG_STOP_WHEN_Z_SORTED							(4)
	#define DEBUG_FLAG_STOP_WHEN_DRAWN_IN_WINDOW					(8)

#endif



#define RELEASE_MODE_DEBUG_STUFF									(1)
// This turns on the outputting of a crapload of diagnostic data to allow
// me to check that there aren't unitialised variables causing problems.



/*
#define RETRENGINE_DEBUG_VERSION_VIEW_OBJECT_COLLISION_BUCKETS	(1)
#define RETRENGINE_DEBUG_VERSION_VIEW_OBJECT_COLLISION			(1)
#define RETRENGINE_DEBUG_VERSION_VIEW_WORLD_COLLISION			(1)
*/



#define RETRENGINE_RETROSPEC_HISCORE_GAME_ID				(86)

#endif

