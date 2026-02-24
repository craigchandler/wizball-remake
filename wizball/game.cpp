/*
	So, this is the game file itself. It basically deals with the faffy stuff such as the menus
	and the like. It's not exactly the most exciting stuff but it should be nice and pretty. Coo.
*/

#include "version.h"

#include <stdio.h>
#include <assert.h>

#include <allegro.h>

#include "fortify.h"



#ifdef ALLEGRO_WINDOWS
	#include <winalleg.h>
	#include "windows.h"
#else
	#include <stdlib.h>
	#include <sys/types.h>
	#include <sys/stat.h>
	#include <string.h>
#endif

#include "graphics.h" // So we can draw stuff.
#include "main.h" // For the game states.
#include "control.h" // So we can get key strokes.
#include "game.h" // It's my daddy!
#include "scripting.h" // Because it needs to actually start the game.
#include "parser.h" // For parsing, doncherknow.
#include "new_editor.h"
#include "output.h"
#include "global_param_list.h"
#include "textfiles.h"
#include "math_stuff.h"
#include "sound.h"
#include "arrays.h"
#include "file_stuff.h"
#include "string_stuff.h"



void GAME_load_and_set_up_everything (void)
{
	bool result;

	MAIN_add_to_log ("Loading tokenised script file...");
	result = SCRIPTING_load_script ("scriptfile.tsl" );
	if (result == true)
	{
		OUTPUT_message("'SCRIPTFILE.TSL' or 'SCRIPTS.TXT' not present! Where the hell is it? Eh?! EH!?!");
		assert(0);
	}
	MAIN_add_to_log ("\tOK!");

	if (parse_all_data)
	{
		MAIN_add_to_log ("Loading text files...");
		TEXTFILE_load_text();
		// This loads the lines of text who tags are featured in the GPL.
		MAIN_add_to_log ("\tOK!");
	}
	else
	{
		if (load_from_dat_file)
		{
			MAIN_add_to_log ("Loading compiled text files from datafile...");
			TEXTFILE_load_compiled_text_from_datafile();
			MAIN_add_to_log ("\tOK!");
		}
		else
		{
			MAIN_add_to_log ("Loading compiled text files...");
			TEXTFILE_load_compiled_text ();
			// This loads the lines of text but not the tags.
			MAIN_add_to_log ("\tOK!");
		}
	}

	MAIN_add_to_log ("Loading graphics...");
	GRAPHICS_load_graphics();
	// Uses the above-loaded list to load all the graphics.
	MAIN_add_to_log ("\tOK!");

	// At this point we can shove the loading picture up on screen! Woot!
	MAIN_draw_loading_picture ("",0);

	MAIN_add_to_log ("Setting up sound...");
	SOUND_setup();
	// Sets up the sound stuff...
	MAIN_add_to_log ("\tOK!");
	
	MAIN_add_to_log ("Loading sound effects...");
	SOUND_load_sound_effects();
	// Uses the above-loaded list to load all the sound effects.
	MAIN_add_to_log ("\tOK!");

	MAIN_add_to_log ("Opening sound streams...");
	SOUND_open_sound_streams();
	// Uses the above-loaded list to load all the sound effects.
	MAIN_add_to_log ("\tOK!");

	if (parse_all_data)
	{
		MAIN_add_to_log ("Loading editor icons...");
		EDIT_load_icons ();
		MAIN_add_to_log ("\tOK!\n");
	}

	MAIN_add_to_log ("Blanking entity data tables...");
	SCRIPTING_setup_data_stores ();
	MAIN_add_to_log ("\tOK!\n");

	MAIN_add_to_log ("Creating trigonometry tables...");
	MATH_setup_trig_tables (10000,36000);
	MAIN_add_to_log ("\tOK!\n");

	if (parse_all_data)
	{
		MAIN_add_to_log ("Loading prefabs...");
		SCRIPTING_load_prefabs ();
		MAIN_add_to_log ("\tOK!\n");
	}
	else
	{
		MAIN_add_to_log ("Loading compiled prefabs...");
		SCRIPTING_load_compiled_prefabs ();
		MAIN_add_to_log ("\tOK!\n");
	}

	MAIN_add_to_log ("Loading datatables...");
	ARRAY_load_datatables ();
	MAIN_add_to_log ("\tOK!\n");

	EDIT_setup(); // This does a LOAD of stuff, take a look inside, baby!
}


void GAME_game (void)
{
	static bool initialise = true;
	bool exit_game;

	if (initialise == true)
	{
		SCRIPTING_setup_everything();
		MAIN_reset_frame_counter();
		SCRIPTING_spawn_entity_by_name ("STARTUP" , 0 , 0 , 1 , 2 , 0 );

		initialise = false;
	}
	else
	{
		SCRIPTING_update_window_positions ();

		exit_game = SCRIPTING_process_entities();

		if (exit_game == true)
		{
			CONTROL_stop_and_save_active_channels ("exited_game_no_error");
			game_state = GAME_STATE_EXIT_GAME;
			initialise = true;
		}
	}
}



void GAME_output_list_to_bat_file (FILE *fp, char *list, char *tags, char *packname, bool special_tag=false)
{
	char text[MAX_LINE_SIZE];
	char extension[MAX_LINE_SIZE];
	char project_dir[MAX_LINE_SIZE];
	int t;
	int start,end;

	strcpy(project_dir, MAIN_get_project_name());
	put_backslash(project_dir);

	GPL_list_extents (list, &start, &end);

	sprintf(extension,"%s",GPL_what_is_list_extension(list));

	for (t=start; t<end; t++)
	{
		if (special_tag)
		{
			if (STRING_instr_word(GPL_what_is_word_name(t),"[ARB]") != UNSET)
			{
				// Well, it looks like we've got a thingummy. Eep!

				sprintf (text,"dat %s%s.dat -a %s%s\\%s%s -k -t DATA\n",project_dir,packname,project_dir,list,GPL_what_is_word_name(t),".TXT");
				fix_filename_slashes(text);
				fputs (text,fp);
			}
		}
		else
		{
			sprintf (text,"dat %s%s.dat -a %s%s\\%s%s %s\n",project_dir,packname,project_dir,list,GPL_what_is_word_name(t),extension,tags);
			fix_filename_slashes(text);
			fputs (text,fp);
		}
	}
}

#ifdef ALLEGRO_WINDOWS
	#define BAT_EXT ".BAT"
	#define DEL_CMD "del"
	#define PREAMBLE ""
	#define EXECUTE(f) system(f)
#else
	#define BAT_EXT ""
	#define DEL_CMD "rm -f"
	#define PREAMBLE "#!/bin/sh\n"
	#define EXECUTE(f) sprintf(text, "sh %s", f); system(text)
#endif

void GAME_create_resource_bat_file (void)
{
	char filename[MAX_LINE_SIZE];
	char project_dir[MAX_LINE_SIZE];
	char text[MAX_LINE_SIZE];

	sprintf(filename,"data_compile_%s" BAT_EXT, MAIN_get_project_name());
	strcpy(project_dir, MAIN_get_project_name());
	put_backslash(project_dir);

	// First of all, erase the old .bat file

	remove (MAIN_get_project_filename("data_compile.bat"));
	
	// Good... gooooood.

	FILE *fp = fopen(filename,"w");

	if (fp == NULL)
	{
		assert(0);
		// WTF?!
	}

	// Tell the .bat file to remove the current "data.dat"
	sprintf(text,DEL_CMD " %sdata.dat\n",project_dir);
	fputs(text,fp);

	// Add all the data files.
	sprintf(text,"dat %sdata.dat -a %sglobal_parameter_list.txt -k -t DATA -c0\n",project_dir,project_dir);
	fputs (text,fp);

	sprintf(text,"dat %sdata.dat -a %sgpl_list_size.txt -k -t DATA -c0\n",project_dir,project_dir);
	fputs (text,fp);

	sprintf(text,"dat %sdata.dat -a %scptf.txt -k -t DATA -c0\n",project_dir,project_dir);
	fputs (text,fp);

	sprintf(text,"dat %sdata.dat -a %sproject_settings.txt -k -t DATA -c0\n",project_dir,project_dir);
	fputs (text,fp);

	// Delete the "gfx.dat"
	sprintf(text,"\n" DEL_CMD " %sgfx.dat\n",project_dir);
	fputs(text,fp);

	// Stick 'em back in...
	GAME_output_list_to_bat_file (fp,"SPRITES","-k -t BMP -c0","gfx",true); // So all the text files are added first which'll speed dat production
	GAME_output_list_to_bat_file (fp,"SPRITES","-k -t BMP -c0","gfx"); // Then the actual graphics...
	// And the editor graphics...
	sprintf(text,"dat %sgfx.dat -a editor\\editor_gui.bmp -k -t BMP -c0\n",project_dir);
	fix_filename_slashes(text);
	fputs (text,fp);

	sprintf(text,"dat %sgfx.dat -a editor\\editor_bitmask.bmp -k -t BMP -c0\n",project_dir);
	fix_filename_slashes(text);
	fputs (text,fp);

	sprintf(text,"dat %sgfx.dat -a editor\\editor_object_sides.bmp -k -t BMP -c0\n",project_dir);
	fix_filename_slashes(text);
	fputs (text,fp);


	// And now the sound effects...
	sprintf(text,"\n" DEL_CMD " %ssfx.dat\n",project_dir);
	fputs(text,fp);
	GAME_output_list_to_bat_file (fp,"SOUND_FX","-k -t DATA -c0","sfx");

	// And the streams...
	sprintf(text,"\n" DEL_CMD " %sstream.dat\n",project_dir);
	fputs(text,fp);
	GAME_output_list_to_bat_file (fp,"STREAMS","-k -t DATA -c0","stream");

	// And music...
	sprintf(text,"\n" DEL_CMD " %smusic.dat\n",project_dir);
	fputs(text,fp);
	GAME_output_list_to_bat_file (fp,"MUSIC","-k -t DATA -c0","music");

	// And finally, the paths...
	sprintf(text,"\n" DEL_CMD " %spaths.dat\n",project_dir);
	fputs(text,fp);
	GAME_output_list_to_bat_file (fp,"PATHS","-k -t DATA -c0","paths");

	// And close the file. Hurrah!
	fclose(fp);

//	EXECUTE(filename);

//	remove (filename);
}



void GAME_destroy_everything (bool create_dat_file)
{
	// Quite simply clears out all the malloced stuff.

	MAIN_add_to_log ("CLOSING DOWN...\n");
	
	if (create_dat_file)
	{
		GAME_create_resource_bat_file ();
	}

	EDIT_close_down ();
}

