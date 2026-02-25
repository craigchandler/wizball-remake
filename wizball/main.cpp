#include "version.h"

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <allegro.h>

#ifdef ALLEGRO_WINDOWS
	#include <winalleg.h>
	#include <windows.h>
#endif

#include <signal.h>
#ifdef ALLEGRO_LINUX
	#include <execinfo.h>
	#include <sys/time.h>
#endif
#ifdef ALLEGRO_MACOSX
	#include <sys/time.h>
#endif

#include "string_size_constants.h"
#include "parser.h"
#include "scripting.h"
#include "main.h"
#include "graphics.h"
#include "control.h"
#include "game.h"
#include "new_editor.h"
#include "tilesets.h"
#include "tilemaps.h"
#include "file_stuff.h"
#include "math_stuff.h"

#ifdef ALLEGRO_WINDOWS
	#include "direct.h"
#endif

#include "output.h"
#include "sound.h"
#include "object_collision.h"
#include "string_stuff.h"
#include "global_param_list.h"

#include "fortify.h"

int program_state;

bool draw_debug_overlay = false;

volatile int game_trigger;	// game timer
volatile int frame_count;	// fps counter to see the actual fps achieved
volatile int fps;

char wheres_wally[128];

int frames_so_far = 0;

int demo_file_index = 0;

int game_state;

// This reflects the fact that on OSX we'll install the dat files in the bundle's content directory and not in a subdir of it.
/*
#ifdef ALLEGRO_MACOSX
#ifdef RELEASE_MODE
char project[MAX_NAME_SIZE] = ".";
#else
char project[MAX_NAME_SIZE] = "wizball";
char pack_project[MAX_NAME_SIZE] = "";
char pack_project[MAX_NAME_SIZE] = "";
char pack_project[MAX_NAME_SIZE] = "";
char pack_project[MAX_NAME_SIZE] = "";
#endif
#else // not OSX
char project[MAX_NAME_SIZE] = "wizball";
#endif
*/
char project[MAX_NAME_SIZE] = "wizball";
char pack_project[MAX_NAME_SIZE] = "";

//char project[] = "kon_and_drderekdrs";
char full_project_filename[FULL_PROJECT_NAME_LENGTH];

int close_callback_signal = 0;

char loading_screen_image_name[NAME_SIZE];
int loading_screen_image_index = UNSET;
int loading_screen_frame = UNSET;
float loading_screen_scale = 1.0f;

int native_resolution_screen_width = UNSET;
int native_resolution_screen_height = UNSET;
int native_resolution_screen_multiplier = UNSET;

char system_font_image_name[NAME_SIZE];

int progress_bar_x = 200;
int progress_bar_y = 400;
int progress_bar_width = 240;
int progress_bar_height = 16;

bool run_windowed = true;
int colour_depth = 32;
int config_window_multiple = UNSET;

bool parse_all_data = true;
// If this is true then all the scripts will be parsed and the
// levels loaded from the text format files rather than the
// pre-processed ones.

bool output_debug_information = false;

bool create_dat_file = true;

bool load_from_dat_file = false;
bool linux_vblank_hint_enabled = true;
bool linux_timer_watchdog_enabled = true;

typedef struct
{
	int frame_number;
	int random_value;
	int entity_script_number;
	int line_number;
} rand_log_struct;

rand_log_struct *rand_log = NULL;
int rand_log_length = 0;



#ifdef RELEASE_MODE
	bool release_mode = true;
#else
	bool release_mode = false;
#endif


int restorescreensaver;




typedef struct {
	char name[MAX_NAME_SIZE];
} project_name;



project_name *project_list = NULL;
int project_counter = 0;

#ifdef ALLEGRO_MACOSX
	extern void do_exit_game(void);
#endif

void MAIN_start_log (void)
{
	if (output_debug_information)
	{
		FILE *file_pointer = fopen ("logfile.txt","w");

		assert (file_pointer != NULL);

		fputs("LOG FILE\n\nSTARTING UP\n\n",file_pointer);

		fclose (file_pointer);
	}
}



void close_callback()
{
	if (close_callback_signal == 0)
	{
		close_callback_signal = 1;
	}
}

char * MAIN_get_project_name(void)
{
#ifdef ALLEGRO_MACOSX
	return (char*) (release_mode ? "." : project);
	#else
	return &project[0];
	#endif
}



char * MAIN_get_pack_project_name(void)
{
	if (pack_project[0] != '\0')
	{
		return &pack_project[0];
	}
	return MAIN_get_project_name();
}



void MAIN_add_to_log (char *text)
{
	if (output_debug_information)
	{
		FILE *file_pointer = fopen ("logfile.txt","a");

		assert (file_pointer != NULL);

		fputs(text,file_pointer);
		fputs("\n",file_pointer);

		fclose (file_pointer);
	}
}



void MAIN_debug_start_the_last_thing (void)
{
	if (output_debug_information)
	{
		FILE *fp = fopen("the_last_thing_i_did.txt" , "w");

		if (fp != NULL)
		{
			fputs("Start Of The Last Thing I Did...\n",fp);
			fclose(fp);
		}
		else
		{
			OUTPUT_message("Cannot output debug message file");
			assert(0);
		}
	}
	else
	{
		remove ("the_last_thing_i_did.txt");
	}
}



void MAIN_debug_last_thing (char *message)
{
	if (output_debug_information)
	{
		static int counter = 0;

		counter++;

		if (counter > 200000)
		{
			return;
		}

		FILE *fp = fopen("the_last_thing_i_did.txt" , "a");

		if (fp != NULL)
		{
			fputs(message,fp);
			fputc('\n',fp);
			fclose(fp);
		}
		else
		{
			OUTPUT_message("Cannot output debug message file");
			assert(0);
		}
	}
}



void MAIN_reset_game_trigger (void)
{
	game_trigger = 0;
}



void TIMING_game_timer()
{
	game_trigger ++;
}
END_OF_FUNCTION (TIMING_game_timer);



void TIMING_fps_handler()
{
	fps = frame_count;		//set fps to display to user
	frame_count = 0;		//reset the count for the next loop
}
END_OF_FUNCTION (TIMING_fps_handler);


unsigned int MAIN_get_wall_time_ms(void)
{
#ifdef ALLEGRO_WINDOWS
	return (unsigned int)GetTickCount();
#else
	struct timeval tv;
	gettimeofday(&tv, NULL);
	return (unsigned int)((tv.tv_sec * 1000) + (tv.tv_usec / 1000));
#endif
}


void MAIN_configure_linux_vblank_environment(void)
{
#ifdef ALLEGRO_LINUX
	if (linux_vblank_hint_enabled == false)
	{
		MAIN_add_to_log("Linux vblank env hints disabled by -novblankhint.");
		return;
	}

	char line[MAX_LINE_SIZE];
	const char *mesa_vblank = getenv("vblank_mode");
	const char *nvidia_vblank = getenv("__GL_SYNC_TO_VBLANK");

	if (mesa_vblank == NULL)
	{
		setenv("vblank_mode", "0", 0);
		MAIN_add_to_log("Set env vblank_mode=0 (Mesa swap sync disable hint).");
	}
	else
	{
		sprintf(line, "Keep existing env vblank_mode=%s", mesa_vblank);
		MAIN_add_to_log(line);
	}

	if (nvidia_vblank == NULL)
	{
		setenv("__GL_SYNC_TO_VBLANK", "0", 0);
		MAIN_add_to_log("Set env __GL_SYNC_TO_VBLANK=0 (NVIDIA swap sync disable hint).");
	}
	else
	{
		sprintf(line, "Keep existing env __GL_SYNC_TO_VBLANK=%s", nvidia_vblank);
		MAIN_add_to_log(line);
	}
#endif
}

#ifdef ALLEGRO_MACOSX
extern "C" char* project_writeable(char*);
#endif

char * MAIN_get_project_filename (char *filename, bool writeable)
{
	// Simply appends the passed filename onto the project's name

	#ifdef ALLEGRO_MACOSX
	/* For OSX, if it's a writeable file (e.g. settings, highscores) 
	* it goes in the user's ~/Library/Application Support/ 
	* otherwise if we're in editing mode it should go in the 
	* project directory,
	* otherwise if we're in release mode
	* it's in the bundle's resource directory.
	*/
		char* dir;
		if (writeable) {
			dir = project_writeable(project);
		}
		else {
			if (release_mode) {
				dir = ".";
			}
			else {
				dir = project;
			}
		}
		return append_filename(full_project_filename,  dir, filename, sizeof(full_project_filename));
	#else
		// In -dat mode, prefer read-only resources from the selected pack/data dir
		// (e.g. -datdir wizball/repacked), but fall back to the project dir if absent.
		if (!writeable && load_from_dat_file)
		{
			const char *pack_dir = MAIN_get_pack_project_name();
			if (strcmp(pack_dir, project) != 0)
			{
				char candidate[FULL_PROJECT_NAME_LENGTH];
				FILE *probe;
				append_filename(candidate, pack_dir, filename, sizeof(candidate));
				probe = fopen(candidate, "rb");
				if (probe != NULL)
				{
					fclose(probe);
					return append_filename(full_project_filename, pack_dir, filename, sizeof(full_project_filename));
				}
			}
		}
		return append_filename(full_project_filename, project, filename, sizeof(full_project_filename));
	#endif
}



char * MAIN_get_pack_filename (char *filename)
{
	return append_filename(full_project_filename, MAIN_get_pack_project_name(), filename, sizeof(full_project_filename));
}



int editor_main(void)
{
	game_state = GAME_STATE_EDITOR_OVERVIEW;

	bool draw = false;

	bool exit_game = false;

	game_trigger=0;

	int counter = 0;
	bool initialise = true;

	do {

		// This will loop as long as the game timer trigger is >0.
		// The stuff in here is fast as its just logic code with no rasterizing.
		while ((game_trigger) && (draw == false))
		{
			CONTROL_update_all_input ();

			frames_so_far++;

			switch(game_state)
			{
			case GAME_STATE_EDITOR_OVERVIEW:
				initialise = EDIT_editor_overview (initialise);
				break;

			case GAME_STATE_EXIT_GAME:
				exit_game = true;
				break;

			case GAME_STATE_EDITOR:
				initialise = TILEMAPS_edit (initialise);
				break;

			case GAME_STATE_EDIT_TILESET:
				initialise = TILESETS_edit (initialise);
				break;

			default:
				break;
			}

			counter++;

			game_trigger--; // Decrement game count for next loop

			if (game_trigger == 0)
			{
				draw = true;
			}

		}

		if (draw==true)
		{
			OUTPUT_updatescreen();
			frame_count++;
			draw=false;
		}

	} while (exit_game == false);

	return 0;
}



int game_main(void)
{
	game_state = GAME_STATE_GAME;

	set_close_button_callback (close_callback);

//	EDIT_load_map ("map1.txt");
//	EDIT_save_map (2);

//	EDITOR_blank_tilemap_arrays();
//	EDITOR_blank_path_arrays();
//	EDITOR_blank_spawn_point_arrays();

//	EDITOR_load_tilemap();
//	EDITOR_load_paths();
//	EDITOR_load_spawn_points();
//	EDITOR_load_tile_properties();

//	EDITOR_generate_all_path_rects();

//	EDITOR_calculate_lookup_paths ();

	bool draw = false;

	int total;

	bool exit_game = false;

	int total_entities_drawn;

	game_trigger=0;

	int first_frames = 2;
	const int max_logic_steps_per_frame = 2;
	const int max_logic_backlog = 4;
	const int timer_stall_recovery_ms = 100;
	unsigned int last_logic_tick_time = MAIN_get_wall_time_ms();

	int counter = 0;

	char word[MAX_LINE_SIZE];

	do {
		// Keep simulation ticking at a fixed timer rate while rendering once per frame.
		// A small cap prevents runaway catch-up when rendering stalls.
		int logic_steps = 0;

		if (close_callback_signal == 1)
		{
			close_callback_signal = 2;

			SCRIPTING_spawn_shutdown_entity();
//			SCRIPTING_spawn_entity_by_name("shutdown",0,0,0,0,0);
		}

		if ((linux_timer_watchdog_enabled == true) && (game_trigger <= 0))
		{
			unsigned int now = MAIN_get_wall_time_ms();
			unsigned int stalled_ms = now - last_logic_tick_time;

			// Some Linux compositor/display transitions can temporarily stop Allegro timer callbacks.
			// Recover with a conservative synthetic tick so the game loop cannot hard-freeze.
			if (stalled_ms >= timer_stall_recovery_ms)
			{
				game_trigger = 1;
				last_logic_tick_time = now;
			}
		}

		if (game_trigger > max_logic_backlog)
		{
			game_trigger = max_logic_backlog;
		}

		if (game_trigger > 0)
		{
			#ifdef RETRENGINE_DEBUG_VERSION_WHERES_WALLY
			sprintf (wheres_wally,"I'm in the game loop!");
			#endif
		
			SOUND_check_persistant_channel_validity ();
			SOUND_fade_channels ();

			#ifdef RETRENGINE_DEBUG_VERSION_WHERES_WALLY
			sprintf (wheres_wally,"Just updated the control!");
			#endif

			while ((game_trigger > 0) && (logic_steps < max_logic_steps_per_frame) && (exit_game == false))
			{
				// Poll input on logic ticks so CONTROL_key_hit edge transitions are observed by game logic.
				CONTROL_update_all_input ();

				switch(game_state)
				{
				case GAME_STATE_EXIT_GAME:
					exit_game = true;
					break;

				case GAME_STATE_GAME:
					GAME_game ();
					break;

				default:
					break;
				}

				frames_so_far++;
				counter++;
				game_trigger--;
				logic_steps++;
				last_logic_tick_time = MAIN_get_wall_time_ms();
			}

			draw = (logic_steps > 0);
		}
		else
		{
			draw = false;
		}

		if (draw==true)
		{
			#ifdef RETRENGINE_DEBUG_VERSION_WHERES_WALLY
			sprintf (wheres_wally,"I'm in the render loop!");
			#endif

			OUTPUT_clear_screen();
//			SCRIPTING_check_for_loop_links();
			total_entities_drawn = SCRIPTING_draw_all_windows ();

			if (first_frames)
			{
				if ( (loading_screen_image_index != UNSET) && (loading_screen_frame != UNSET) )
				{
					OUTPUT_draw_sprite_scale (loading_screen_image_index, loading_screen_frame , 0 , 0 , loading_screen_scale );
					OUTPUT_text (0,0, "", 0, 0, 0, 10000);
				}
				first_frames--;
			}

			if (draw_debug_overlay)
			{
				SCRIPTING_confirm_entity_counts ();

				sprintf(word,"Total process count : %i",total_process_counter);
				OUTPUT_text (0,0, word, 255, 255, 255);

				sprintf(word,"Drawn process count : %i",drawn_process_counter);
				OUTPUT_text (0,8, word, 255, 255, 0);

				sprintf(word,"Alive process count : %i",alive_process_counter);
				OUTPUT_text (0,16, word, 0, 255, 255);

				sprintf(word,"Script line process count : %i",script_lines_executed);
				OUTPUT_text (0,24, word, 0, 255, 0);

				sprintf(word,"Free Entities : %i",free_entities);
				OUTPUT_text (0,40, word, 255, 255, 0);

				sprintf(word,"Used Entities : %i",used_entities);
				OUTPUT_text (0,48, word, 0, 255, 255);

				sprintf(word,"Limbo'd Entities : %i",limboed_entities);
				OUTPUT_text (0,56, word, 0, 255, 0);

				sprintf(word,"Lost Entities : %i",lost_entities);
				OUTPUT_text (0,64, word, 255, 0, 255);

				total = free_entities + used_entities + limboed_entities;

				sprintf(word,"Total Entities : %i",total);
				OUTPUT_text (0,72, word, 255, 0, 0);

				sprintf(word,"Persistant Channels : %i",current_persistant_channels);
				OUTPUT_text (0,80, word, 0, 255, 255);

				sprintf(word,"Fader Channels : %i",fading_channel_count);
				OUTPUT_text (0,88, word, 0, 255, 0);

				sprintf(word,"Collision Count : %i",collision_entity_count);
				OUTPUT_text (0,96, word, 0, 255, 0);

				sprintf(word,"Actual Collision Count : %i",collision_actual_count);
				OUTPUT_text (0,104, word, 0, 255, 0);

				sprintf(word,"Total Entities Drawn : %i",total_entities_drawn);
				OUTPUT_text (0,112, word, 0, 255, 0);

				SCRIPTING_display_script_list (128);

				SCRIPTING_display_flag_list (256,0);

				sprintf(word,"Frame : %i",frames_so_far);
				OUTPUT_text (game_screen_width-128,0, word, 0, 255, 0);
			}
			
			OUTPUT_updatescreen();

			frame_count++;

			if (output_debug_information)
			{
				static int perf_log_last_second = -1;
				int current_second = frames_so_far / 60;
				if (current_second != perf_log_last_second)
				{
					char perf_line[MAX_LINE_SIZE];
					sprintf(perf_line,
						"PERF frame=%d fps=%d trigger=%d drawn=%d software=%d",
						frames_so_far,
						fps,
						game_trigger,
						total_entities_drawn,
						software_mode_active ? 1 : 0);
					MAIN_add_to_log(perf_line);
					perf_log_last_second = current_second;
				}
			}

			draw=false;
		}
		else 
		{
			rest(1);
		}

	} while (exit_game == false);

	return 0;
}



void MAIN_draw_loading_picture (char *description, int int_progress)
{
	float progress = float(int_progress) / 100.0f;

	loading_screen_image_index = GPL_find_word_value ("SPRITES", loading_screen_image_name);

	if ( (loading_screen_image_index != UNSET) && (loading_screen_frame != UNSET) )
	{
		OUTPUT_clear_screen ();
		OUTPUT_draw_sprite_scale (loading_screen_image_index, loading_screen_frame , 0 , 0 , loading_screen_scale );

		if (progress_bar_width > 0)
		{
			OUTPUT_filled_rectangle_by_size (progress_bar_x, progress_bar_y, progress_bar_width, progress_bar_height, 0, 0, 0);
			OUTPUT_rectangle_by_size (progress_bar_x, progress_bar_y, progress_bar_width, progress_bar_height, 255, 255, 255);

			if (progress > 0)
			{
				int new_width = int (float (progress_bar_width - 3) * progress);

				OUTPUT_filled_rectangle_by_size (progress_bar_x+2, progress_bar_y+2, new_width, progress_bar_height-4, 255*progress, 0, 255);
			}

			if (strlen(description)>0)
			{
				OUTPUT_centred_text (progress_bar_x+(progress_bar_width/2) , progress_bar_y + 4, description,255,255,255);
			}
		}

		OUTPUT_updatescreen ();
	}
}



void MAIN_load_config (void)
{
	// This just loads the few gubbiney things that are required so we have something to gawp at while the game loads.

	char line[MAX_LINE_SIZE];
	bool exitmainloop = false;
	char *pointer;
	
	FILE *file_pointer = fopen (MAIN_get_project_filename("config.txt", true),"r");

	if (file_pointer != NULL)
	{
		while ( (fgets ( line , MAX_LINE_SIZE , file_pointer ) != NULL) && (exitmainloop == false) )
		{
			STRING_strip_newlines(line);
			strupr(line);

			pointer = STRING_end_of_string(line,"#END OF FILE");
			if (pointer != NULL)
			{
				exitmainloop = true;
			}

			pointer = STRING_end_of_string(line,"#WINDOWED = TRUE");
			if (pointer != NULL)
			{
				run_windowed = true;
			}

			pointer = STRING_end_of_string(line,"#WINDOWED = FALSE");
			if (pointer != NULL)
			{
				run_windowed = false;
			}

			pointer = STRING_end_of_string(line,"#WINDOW_SIZE_MULTIPLE = ");
			if (pointer != NULL)
			{
				config_window_multiple = atoi (pointer);
			}

			pointer = STRING_end_of_string(line,"#SOUND EFFECTS VOLUME = ");
			if (pointer != NULL)
			{
				SOUND_set_global_sound_volume (atoi(pointer));
			}

			pointer = STRING_end_of_string(line,"#MUSIC VOLUME = ");
			if (pointer != NULL)
			{
				SOUND_set_global_music_volume (atoi(pointer));
			}

			pointer = STRING_end_of_string(line,"#NEXT DEMO INDEX = ");
			if (pointer != NULL)
			{
				demo_file_index = (atoi(pointer));
			}

			pointer = STRING_end_of_string(line,"#PLAYER_INPUT ");
			if (pointer != NULL)
			{
				CONTROL_define_control_from_config (pointer);
			}

			pointer = STRING_end_of_string(line,"#HARDWARE = FALSE");
			if (pointer != NULL)
			{
				software_mode_active = true;
			}

			pointer = STRING_end_of_string(line,"#HARDWARE = TRUE");
			if (pointer != NULL)
			{
				software_mode_active = false;
			}

			pointer = STRING_end_of_string(line,"#OPENGL_13 = TRUE");
			if (pointer != NULL)
			{
				enable_multi_texture_effects_ideally = true;
			}

			pointer = STRING_end_of_string(line,"#OPENGL_13 = FALSE");
			if (pointer != NULL)
			{
				enable_multi_texture_effects_ideally = false;
			}

			pointer = STRING_end_of_string(line,"#COLOUR DEPTH = ");
			if (pointer != NULL)
			{
				colour_depth = atoi(pointer);
				if ( (colour_depth != 16) && (colour_depth != 32) )
				{
					colour_depth = 16;
				}
			}
		}

		fclose(file_pointer);
	}
	else
	{
		// "config.txt" not found, ah well... Could be worse.
	}
}



void MAIN_load_project_settings_from_datafile (void)
{
	// This just loads the few gubbiney things that are required so we have something to gawp at while the game loads.

	char line[MAX_LINE_SIZE];
	bool exitmainloop = false;
	char *pointer;
	char filename[MAX_LINE_SIZE];

	sprintf (filename,"%s\\data.dat#project_settings.txt",MAIN_get_pack_project_name());
	fix_filename_slashes(filename);

	sprintf (loading_screen_image_name,"");

	PACKFILE *packfile_pointer = pack_fopen (filename,"r");

	if (packfile_pointer != NULL)
	{
		while ( (pack_fgets ( line , MAX_LINE_SIZE , packfile_pointer ) != NULL) && (exitmainloop == false) )
		{
			STRING_strip_newlines(line);
			strupr(line);

			pointer = STRING_end_of_string(line,"#END OF FILE");
			if (pointer != NULL)
			{
				exitmainloop = true;
			}

			pointer = STRING_end_of_string(line,"#LOADING_SCREEN = ");
			if (pointer != NULL)
			{
				strcpy (loading_screen_image_name,pointer);
			}

			pointer = STRING_end_of_string(line,"#LOADING_FRAME = ");
			if (pointer != NULL)
			{
				loading_screen_frame = atoi (pointer);
			}

			pointer = STRING_end_of_string(line,"#SCALE = ");
			if (pointer != NULL)
			{
				loading_screen_scale = atof (pointer);
			}

			pointer = STRING_end_of_string(line,"#LOADING_BAR_POSITION_X = ");
			if (pointer != NULL)
			{
				progress_bar_x = atoi (pointer);
			}

			pointer = STRING_end_of_string(line,"#LOADING_BAR_POSITION_Y = ");
			if (pointer != NULL)
			{
				progress_bar_y = atoi (pointer);
			}

			pointer = STRING_end_of_string(line,"#SYSTEM_FONT = ");
			if (pointer != NULL)
			{
				strcpy (system_font_image_name,pointer);
			}

			pointer = STRING_end_of_string(line,"#LOADING_BAR_WIDTH = ");
			if (pointer != NULL)
			{
				progress_bar_width = atoi (pointer);
			}

			pointer = STRING_end_of_string(line,"#LOADING_BAR_HEIGHT = ");
			if (pointer != NULL)
			{
				progress_bar_height = atoi (pointer);
			}

			pointer = STRING_end_of_string(line,"#NATIVE_SCREEN_WIDTH = ");
			if (pointer != NULL)
			{
				native_resolution_screen_width = atoi (pointer);
			}

			pointer = STRING_end_of_string(line,"#NATIVE_SCREEN_HEIGHT = ");
			if (pointer != NULL)
			{
				native_resolution_screen_height = atoi (pointer);
			}

			pointer = STRING_end_of_string(line,"#NATIVE_SCREEN_MULTIPLIER = ");
			if (pointer != NULL)
			{
				native_resolution_screen_multiplier = atoi (pointer);
			}

		}

		pack_fclose(packfile_pointer);
	}
	else
	{
		OUTPUT_message("Project setting file not found!");
		assert(0);
	}
}



void MAIN_load_project_settings (void)
{
	// This just loads the few gubbiney things that are required so we have something to gawp at while the game loads.

	char line[MAX_LINE_SIZE];
	bool exitmainloop = false;
	char *pointer;

	sprintf (loading_screen_image_name,"");
	
	FILE *file_pointer = fopen (MAIN_get_project_filename("project_settings.txt",false),"r");

	if (file_pointer != NULL)
	{
		while ( (fgets ( line , MAX_LINE_SIZE , file_pointer ) != NULL) && (exitmainloop == false) )
		{
			STRING_strip_newlines(line);
			strupr(line);

			pointer = STRING_end_of_string(line,"#END OF FILE");
			if (pointer != NULL)
			{
				exitmainloop = true;
			}

			pointer = STRING_end_of_string(line,"#LOADING_SCREEN = ");
			if (pointer != NULL)
			{
				strcpy (loading_screen_image_name,pointer);
			}

			pointer = STRING_end_of_string(line,"#LOADING_FRAME = ");
			if (pointer != NULL)
			{
				loading_screen_frame = atoi (pointer);
			}

			pointer = STRING_end_of_string(line,"#SCALE = ");
			if (pointer != NULL)
			{
				loading_screen_scale = atof (pointer);
			}

			pointer = STRING_end_of_string(line,"#LOADING_BAR_POSITION_X = ");
			if (pointer != NULL)
			{
				progress_bar_x = atoi (pointer);
			}

			pointer = STRING_end_of_string(line,"#LOADING_BAR_POSITION_Y = ");
			if (pointer != NULL)
			{
				progress_bar_y = atoi (pointer);
			}

			pointer = STRING_end_of_string(line,"#SYSTEM_FONT = ");
			if (pointer != NULL)
			{
				strcpy (system_font_image_name,pointer);
			}

			pointer = STRING_end_of_string(line,"#LOADING_BAR_WIDTH = ");
			if (pointer != NULL)
			{
				progress_bar_width = atoi (pointer);
			}

			pointer = STRING_end_of_string(line,"#LOADING_BAR_HEIGHT = ");
			if (pointer != NULL)
			{
				progress_bar_height = atoi (pointer);
			}
			
			pointer = STRING_end_of_string(line,"#NATIVE_SCREEN_WIDTH = ");
			if (pointer != NULL)
			{
				native_resolution_screen_width = atoi (pointer);
			}

			pointer = STRING_end_of_string(line,"#NATIVE_SCREEN_HEIGHT = ");
			if (pointer != NULL)
			{
				native_resolution_screen_height = atoi (pointer);
			}

			pointer = STRING_end_of_string(line,"#NATIVE_SCREEN_MULTIPLIER = ");
			if (pointer != NULL)
			{
				native_resolution_screen_multiplier = atoi (pointer);
			}
		}

		fclose(file_pointer);
	}
	else
	{
		OUTPUT_message("Project setting file not found!");
		assert(0);
	}
}



void MAIN_crash_signal_handler (int signal)
{
	FILE *fp = fopen("crash.log", "w");
	if (fp != NULL)
	{
		fprintf(fp, "Signal: %d\n", signal);
		fprintf(fp, "Where: %s\n", wheres_wally);
#ifdef ALLEGRO_LINUX
		void *stack[64];
		int depth = backtrace(stack, 64);
		char **symbols = backtrace_symbols(stack, depth);
		if (symbols != NULL)
		{
			int i;
			fputs("Backtrace:\n", fp);
			for (i = 0; i < depth; i++)
			{
				fputs(symbols[i], fp);
				fputc('\n', fp);
			}
			free(symbols);
		}
#endif
		fclose(fp);
	}

	allegro_exit();
	raise(signal);
}


char * MAIN_create_project_selector (void)
{
//	static char name[]={"exolon"};
//	return name;

	char text_line[TEXT_LINE_SIZE];
	char project_menu_text[TEXT_LINE_SIZE] = "";
	int t;
	
	for (t=0; t<project_counter; t++)
	{
		sprintf (text_line,"%c = %s\n",t+65,project_list[t].name);
		strcat (project_menu_text,text_line);
	}

	OUTPUT_setup_project_list (project_menu_text);

	bool exitloop = false;
	int selected_project = -1;

	while (exitloop == false)
	{
		CONTROL_update_all_input ();

		for (t=0; t<project_counter; t++)
		{
			if (CONTROL_key_hit(KEY_A+t) == true)
			{
				exitloop = true;
				selected_project = t;
			}
		}
	}

	OUTPUT_setup_running_mode ();

	exitloop = false;
	int selected_mode;

	while (exitloop == false)
	{
		CONTROL_update_all_input ();

		for (t=0; t<3; t++)
		{
			if (CONTROL_key_hit(KEY_1+t) == true)
			{
				exitloop = true;
				selected_mode = t+1;
			}
		}
	}

	switch(selected_mode)
	{

	case 1:
		parse_all_data = true;
		output_debug_information = true;
		create_dat_file = true;
		load_from_dat_file = false;
		break;

	case 2:
		parse_all_data = true;
		output_debug_information = false;
		create_dat_file = true;
		load_from_dat_file = false;
		break;

	case 3:
		parse_all_data = false;
		output_debug_information = false;
		create_dat_file = false;
		load_from_dat_file = true;
		break;

	default:
		assert(0);
		break;
	}

	return (project_list[selected_project].name);
}



void MAIN_read_project_list (void)
{
	char *dir_pointer;

	FILE *fp;

	dir_pointer = FILE_open_dir ( "" , ".prj", false);
	char file_text_line[TEXTFILE_SUPER_SIZE];
	char filename[NAME_SIZE];

	if (dir_pointer != NULL) // If there's anything in the directory (and it exists)
	{
		do
		{
			// Copy and cut off anything past the ".";

			strcpy (filename,dir_pointer);
			
			fp = fopen(filename,"r");

			if (fp != NULL)
			{
				fgets(file_text_line,TEXTFILE_SUPER_SIZE,fp);

				STRING_strip_all_disposable (file_text_line);

				fclose (fp);
			}
			else
			{
				assert(0);
			}

			if (project_list==NULL)
			{
				project_list = (project_name *) malloc (sizeof(project_name));
			}
			else
			{
				project_list = (project_name *) realloc (project_list,sizeof(project_name) * (project_counter+1) );
			}

			strcpy(project_list[project_counter].name,file_text_line);

			project_counter++;

		}
#if defined(ALLEGRO_MACOSX) || defined(ALLEGRO_LINUX)
		while ( (dir_pointer = FILE_read_dir_entry (false)) != NULL);
#else
		while ( (dir_pointer = FILE_read_dir_entry (true)) != NULL);
#endif
	}
}



#ifdef DEBUG_RAND_LOGGING

void MAIN_reset_rand_log (void)
{
	if (rand_log != NULL)
	{
		free(rand_log);
		rand_log = NULL;
	}

	rand_log_length = 0;
}



void MAIN_log_rand (int random_value, int line_number, int entity_script_number, int frame_number)
{
	if (rand_log == NULL)
	{
		rand_log = (rand_log_struct *) malloc (sizeof(rand_log_struct));
	}
	else
	{
		rand_log = (rand_log_struct *) realloc (rand_log, (rand_log_length+1) * sizeof(rand_log_struct));
	}

	rand_log[rand_log_length].frame_number = frame_number;
	rand_log[rand_log_length].line_number = line_number;
	rand_log[rand_log_length].random_value = random_value;
	rand_log[rand_log_length].entity_script_number = entity_script_number;

	rand_log_length++;
}



void MAIN_save_rand_log (bool playback)
{
	char filename[MAX_LINE_SIZE];

	if (!playback)
	{
		sprintf(filename,"demos\\random_log_original_%i.txt",demo_file_index);
	}
	else
	{
		sprintf(filename,"demos\\random_log_playback.txt");
	}

	FILE *file_pointer = fopen (MAIN_get_project_filename (filename, true),"w");

	if (file_pointer != NULL)
	{
		int l;
		char text_line[MAX_LINE_SIZE];

		for (l=0 ; l<rand_log_length; l++)
		{
			sprintf(text_line,"%6i V=%i F=%i S=%i L=%i\n",l,rand_log[l].random_value,rand_log[l].frame_number,rand_log[l].entity_script_number,rand_log[l].line_number);
			fputs ( text_line , file_pointer );
		}

		fclose(file_pointer);
	}
	else
	{
		OUTPUT_message("Could not open file to output random log!");
		assert(0); // WTF?!
	}
}

#endif



void MAIN_reset_frame_counter (void)
{
	frames_so_far = 0;
}



int MAIN_get_window_size_multiple (void)
{
	return config_window_multiple;
}



#ifdef FORTIFY

void fortify_clear_log()
{
	FILE *fp;
	fp=fopen("fortify.log", "w");
	fclose(fp);
}

void fortify_output(const char *text)
{
	FILE *fp;

	fp=fopen("fortify.log", "a");
	fprintf(fp, text);
	fclose(fp);
}

#endif


void do_nothing_function (void)
{

}



int main (int argc, char *argv[])
{
	signal (SIGSEGV, MAIN_crash_signal_handler);
	signal (SIGABRT, MAIN_crash_signal_handler);

	#ifdef DEBUG_RAND_LOGGING
	MAIN_reset_rand_log();
	#endif

#ifdef FORTIFY
	fortify_clear_log();

	Fortify_SetOutputFunc(fortify_output);

	Fortify_EnterScope();
#endif

	int t;
	bool force_dat_mode = false;
	bool debug_requested = false;
	char *dat_directory_override = NULL;

	for (t=1; t<argc; t++)
	{
		if (strcmp("-debug",argv[t]) == 0)
		{
			output_debug_information = true;
			debug_requested = true;
		}
		else if (strcmp("-dat",argv[t]) == 0)
		{
			force_dat_mode = true;
		}
		else if (strcmp("-novblankhint",argv[t]) == 0)
		{
			linux_vblank_hint_enabled = false;
		}
		else if (strcmp("-notimerwatchdog",argv[t]) == 0)
		{
			linux_timer_watchdog_enabled = false;
		}
		else if (strcmp("-fmoddriver", argv[t]) == 0)
		{
			if ((t + 1) < argc)
			{
				SOUND_set_preferred_driver(atoi(argv[t + 1]));
				t++;
			}
			else
			{
				OUTPUT_message("Missing argument after -fmoddriver");
				return 1;
			}
		}
		else if (strcmp("-datdir", argv[t]) == 0)
		{
			if ((t + 1) < argc)
			{
				dat_directory_override = argv[t + 1];
				t++;
			}
			else
			{
				OUTPUT_message("Missing argument after -datdir");
				return 1;
			}
		}
	}

	if (dat_directory_override != NULL)
	{
		if (strlen(dat_directory_override) >= MAX_NAME_SIZE)
		{
			OUTPUT_message("Data directory path is too long for -datdir");
			return 1;
		}

		strcpy(pack_project, dat_directory_override);
	}

#ifdef ALLEGRO_WINDOWS
	SystemParametersInfo(SPI_GETSCREENSAVEACTIVE, 0, &restorescreensaver, 0);

	if (restorescreensaver)
	{
		SystemParametersInfo(SPI_SETSCREENSAVEACTIVE, 0, 0, 0);
	}
#endif

	MAIN_start_log ();
	MAIN_configure_linux_vblank_environment();
	if (linux_timer_watchdog_enabled)
	{
		MAIN_add_to_log("Linux timer watchdog enabled.");
	}
	else
	{
		MAIN_add_to_log("Linux timer watchdog disabled by -notimerwatchdog.");
	}

	MAIN_add_to_log ("Initialised Allegro...");
	allegro_init();
	MAIN_add_to_log ("\tOK!");

	set_close_button_callback (do_nothing_function);

	MAIN_add_to_log ("Setting up control code...");
	CONTROL_setup_everything();
	MAIN_add_to_log ("\tOK!");
	
	MAIN_read_project_list();

	if (release_mode == true || force_dat_mode == true)
	{
		parse_all_data = false;
		create_dat_file = false;
		load_from_dat_file = true;
		output_debug_information = debug_requested ? true : false;
	}
	else
	{
		sprintf (project,MAIN_create_project_selector());

		MAIN_add_to_log ("Setting graphics mode to text...");
		set_gfx_mode(GFX_TEXT, 0, 0, 0, 0);
		MAIN_add_to_log ("\tOK!");
	}

	#ifdef RETRENGINE_DEBUG_VERSION_THE_LAST_THING_I_DID
		MAIN_debug_start_the_last_thing ();
	#endif

	if (parse_all_data)
	{
		MAIN_add_to_log ("Parsing scripts...");
		if (PARSER_parse ("SCRIPTLANGUAGE.TXT") == true)
		{
			OUTPUT_message("Error during parsing! Consult 'DIAGNOSIS.TXT'");
			assert(0);
		}
		MAIN_add_to_log ("\tOK!");
	}

	if (load_from_dat_file)
	{
		if (INPUT_load_datafile () == false)
		{
			assert(0);
			// Arse-candles...
		}
	}

//	_chdir ("C:\\Program Files\\Microsoft Visual Studio\\Script Parser\\");
//	_chdir ("D:\\C Projects\\new_toodee_new\\");

	if (load_from_dat_file)
	{
		MAIN_add_to_log ("Loading Project Settings...");
		MAIN_load_project_settings_from_datafile ();
		MAIN_add_to_log ("\tOK!");
	}
	else
	{
		MAIN_add_to_log ("Loading Project Settings...");
		MAIN_load_project_settings ();
		MAIN_add_to_log ("\tOK!");
	}

	MAIN_add_to_log ("Installing timer...");
	install_timer();
	MAIN_add_to_log ("\tOK!");

	if (load_from_dat_file)
	{
		MAIN_add_to_log ("Loading global parameter lists from packfile...");
		GPL_load_global_parameter_list_from_datfile();
		// This loads a list of names and files and all the resources the game needs so that the various
		// other file loading bits know what to load and in what order.
		MAIN_add_to_log ("\tOK!");
	}
	else
	{
		MAIN_add_to_log ("Loading global parameter lists...");
		GPL_load_global_parameter_list();
		// This loads a list of names and files and all the resources the game needs so that the various
		// other file loading bits know what to load and in what order.
		MAIN_add_to_log ("\tOK!");
	}
	
	#ifdef RETRENGINE_DEBUG_VERSION_WATCH_LIST
		SCRIPTING_setup_watch_list ();
	#endif

	small_font_gfx = GPL_find_word_value ("SPRITES", system_font_image_name);
	
	MAIN_add_to_log ("Loading Config...");
	MAIN_load_config ();
	MAIN_add_to_log ("\tOK!");

	if (config_window_multiple == UNSET)
	{
		config_window_multiple = native_resolution_screen_multiplier;
	}

	OUTPUT_setup_allegro (run_windowed,colour_depth,native_resolution_screen_width,native_resolution_screen_height,config_window_multiple);

	if (load_from_dat_file)
	{
		INPUT_load_media_datafiles();
	}

	MAIN_add_to_log ("Setting up frame counter...");

	LOCK_VARIABLE(game_trigger);

	LOCK_VARIABLE(fps);
	LOCK_VARIABLE(frame_count);
	LOCK_FUNCTION(TIMING_game_timer);
	LOCK_FUNCTION(TIMING_fps_handler);

	install_int (TIMING_fps_handler,1000);
	install_int_ex (TIMING_game_timer,BPS_TO_TIMER(60));

	MAIN_add_to_log ("\tOK!");

	GAME_load_and_set_up_everything ();

	int counter;	

	while (progress_bar_width > 0)
	{
		MAIN_draw_loading_picture("",100);
		
		if (progress_bar_width > 0)
		{
			progress_bar_x += 2;
			progress_bar_width -= 4;
		}
	}

	int result;

	if (parse_all_data == true) // If we ain't parsed data then we shouldn't go into the editor!
	{
		// Now we choose whether we want to go down the EDITOR route  or the GAME route. 
		// In each execution of the program you can only enter one of them to save on faffing 
		// about creating and destroying and saving and reloading.

		counter = 0;
		
		bool draw = false;
		bool exit_intro = false;

		do {
			// This will loop as long as the game timer trigger is >0.
			// The stuff in here is fast as its just logic code with no rasterizing.
			while ((game_trigger) && (draw == false))
			{
				CONTROL_update_all_input ();

				if (CONTROL_key_hit(KEY_1)==true)
				{
						program_state = PROGRAM_STATE_GAME;
						exit_intro = true;
				}

				if (CONTROL_key_hit(KEY_2)==true)
				{
						program_state = PROGRAM_STATE_EDITOR;
						exit_intro = true;
				}

				frames_so_far++;

				counter++;

				draw = true;

				game_trigger--; // Decrement game count for next loop
			}

			if (draw==true)
			{
				OUTPUT_clear_screen();
				
				OUTPUT_text ((virtual_screen_width/2)-80,(virtual_screen_height/2)-16, "1 = GAME", 255, 255, 0, 20000);
				OUTPUT_text ((virtual_screen_width/2)-80,(virtual_screen_height/2), "2 = EDITOR", 0, 255, 255, 20000);

				OUTPUT_updatescreen();

				frame_count++;
				draw=false;
			}

		} while (exit_intro == false);

		switch (program_state)
		{
		case PROGRAM_STATE_EDITOR:
			result = editor_main();
			break;

		case PROGRAM_STATE_GAME:
			result = game_main();
			break;

		default:
			break;
		}
	}
	else
	{
		OUTPUT_text (0,0, "", 0, 0, 0, 10000);

		result = game_main();
	}

	// Once the game is finished, destroy all the crap clogging up the memory

	GAME_destroy_everything (create_dat_file);

	OUTPUT_shutdown ();

	SOUND_shutdown ();

	INPUT_unload_datafiles ();

	#ifdef RETRENGINE_DEBUG_VERSION_SCRIPT_TRACER
		MAIN_add_to_log ("Outputting Possible Tracer Script...");
		SCRIPTING_output_tracer_script_to_file (false);
		MAIN_add_to_log ("\tOK!");

		MAIN_add_to_log ("Destroying Tracer Script...");
		PARSER_destroy_trace_script ();
		MAIN_add_to_log ("\tOK!");
	#endif

	MAIN_add_to_log ("Removing frame counter...");
	remove_int(TIMING_game_timer);
	remove_int(TIMING_fps_handler);
	
	MAIN_add_to_log ("\tOK!");

	#ifdef DEBUG_RAND_LOGGING
	MAIN_reset_rand_log();
	#endif

	MAIN_add_to_log ("Setting graphics mode to text...");
	set_gfx_mode(GFX_TEXT, 0, 0, 0, 0);
	MAIN_add_to_log ("\tOK!");

	MAIN_add_to_log ("Shutting down Allegro...");
	allegro_exit();
	MAIN_add_to_log ("\tOK!");

#ifdef ALLEGRO_WINDOWS
	SystemParametersInfo(SPI_GETSCREENSAVEACTIVE, 0, &restorescreensaver, 0);

	if (restorescreensaver)
	{
		SystemParametersInfo(SPI_SETSCREENSAVEACTIVE, 1, 0, 0);
	}
#endif

#ifdef FORTIFY
	Fortify_LeaveScope();

	Fortify_OutputStatistics();
	Fortify_ListAllMemory();
	Fortify_CheckAllMemory();
#endif

	return (result);
}

END_OF_MAIN();
