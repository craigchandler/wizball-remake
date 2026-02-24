#include "sound.h"
#include "global_param_list.h"
#include "graphics.h"
#include "fmod.h"
#include "main.h"
#include "allegro.h"
#include "output.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "fortify.h"



FSOUND_SAMPLE *sound_effects[MAX_SAMPLES];
FSOUND_STREAM *sound_streams[MAX_STREAMS];

int persistant_channel_array [MAX_CHANNELS_WANTED];
bool persistant_channel_array_active [MAX_CHANNELS_WANTED];
int current_persistant_channels = 0;

int fading_channel_array [MAX_FADER_CHANNELS];
bool fading_channel_array_active [MAX_FADER_CHANNELS];
int fading_channel_count = 0;

float global_sound_level = 0.5f;
float global_music_level = 0.5f;

// This set of routines is for things like the thruster flames and stuff which will still
// be playing at the end of a room and won't know when to cut off as they're killed externally.
// So when they start playing, they add their channel number to this here array and the game
// takes care of the rest.

void SOUND_add_persistant_channel (int channel_number)
{
	persistant_channel_array[current_persistant_channels] = channel_number;
	persistant_channel_array_active[current_persistant_channels] = true;

	current_persistant_channels++;
}



void SOUND_add_fader_channel (int channel_number)
{
	fading_channel_array[fading_channel_count] = channel_number;
	fading_channel_array_active[fading_channel_count] = true;
	
	fading_channel_count++;
}



void SOUND_stop_persistant_channels (void)
{
	int channel;

	for (channel=0; channel<current_persistant_channels; channel++)
	{
		FSOUND_StopSound (persistant_channel_array[channel]);
	}

	current_persistant_channels = 0;
}



void SOUND_transfer_persistant_channels_to_fader_channels (void)
{
	int channel;

	for (channel=0; channel<current_persistant_channels; channel++)
	{
		SOUND_add_fader_channel (persistant_channel_array[channel]);
	}

	current_persistant_channels = 0;
}



void SOUND_check_persistant_channel_validity (void)
{
	int channel;

	// Chops any dead channels off the end.

	if (current_persistant_channels > 0)
	{
		channel = current_persistant_channels-1;

		while ( (persistant_channel_array_active [channel] == false) && (current_persistant_channels > 0) )
		{
			current_persistant_channels--;
			channel--;
		}
	}
}



void SOUND_remove_from_persistant_channels (int channel_number)
{
	int channel;

	for (channel=0; channel<current_persistant_channels; channel++)
	{
		if (persistant_channel_array[channel] == channel_number)
		{
			persistant_channel_array_active [channel] = false;
		}
	}
}



void SOUND_fade_channels (void)
{
	int channel;
	int volume;

	for (channel=0; channel<fading_channel_count; channel++)
	{
		if (fading_channel_array_active [channel] == true)
		{
			volume = FSOUND_GetVolume (fading_channel_array[channel]);
			volume -= 15;
			if (volume <=0)
			{
				volume = 0;
				FSOUND_StopSound (fading_channel_array[channel]);
				fading_channel_array_active [channel] = false;
			}
			else
			{
				FSOUND_SetVolume(fading_channel_array[channel],volume);
			}
		}
	}

	// Chops any dead channels off the end.

	if (fading_channel_count > 0)
	{
		channel = fading_channel_count-1;

		while ( (fading_channel_array_active [channel] == false) && (fading_channel_count > 0) )
		{
			fading_channel_count--;
			channel--;
		}
	}

	// Now deal with fading streamed channels...
}



void SOUND_setup (void)
{
	FSOUND_Init (44100,MAX_CHANNELS_WANTED,0);
}



void SOUND_shutdown (void)
{
	int counter = 0;

	int list_start,list_end;
	int list_pointer;

	GPL_list_extents ("SOUND_FX", &list_start, &list_end);

	for (list_pointer = list_start; list_pointer < list_end ; list_pointer++)
	{
		FSOUND_Sample_Free (sound_effects[counter]);

		counter++;
	}

	GPL_list_extents ("STREAMS", &list_start, &list_end);

	counter = 0;

	for (list_pointer = list_start; list_pointer < list_end ; list_pointer++)
	{
		FSOUND_Stream_Close (sound_streams[counter]);

		counter++;
	}

	FSOUND_Close ();
}



void SOUND_load_sound_effects (void)
{
	bool flag = true;
	char filename[MAX_NAME_SIZE];
	char full_filename[MAX_NAME_SIZE];

	int counter = 0;

	int list_start,list_end;
	int list_pointer;

	int data_length;
	char *data_pointer;

	GPL_list_extents ("SOUND_FX", &list_start, &list_end);
	char *extension_pointer = GPL_what_is_list_extension ("SOUND_FX");

	for (list_pointer = list_start; list_pointer < list_end ; list_pointer++)
	{
		if (load_from_dat_file)
		{
			sprintf (filename , "%s%s" , GPL_what_is_word_name (list_pointer) , extension_pointer );
			data_pointer = (char *) INPUT_get_sfx_data_pointer (filename,&data_length);
			sound_effects[counter] = FSOUND_Sample_Load (FSOUND_UNMANAGED, data_pointer ,FSOUND_LOADMEMORY|FSOUND_16BITS,0,data_length);
		}
		else
		{
			sprintf (filename , "%s%s" , GPL_what_is_word_name (list_pointer) , extension_pointer );

			// Okay, check the file to see if it contains the "-arb" or "-set"
			append_filename(full_filename, "sound_fx", filename, sizeof(full_filename) );

			sound_effects[counter] = FSOUND_Sample_Load (FSOUND_UNMANAGED, MAIN_get_project_filename (full_filename) ,FSOUND_16BITS,0,0);

		}

		counter++;
	}
}



int SOUND_play_sound (int sample_number, int volume, int frequency, int pan, bool loop, bool use_music_global)
{
	int channel_number;

	if (use_music_global)
	{
		volume = int(float(volume)*global_music_level);
	}
	else
	{
		volume = int(float(volume)*global_sound_level);
	}

	channel_number = FSOUND_PlaySound (FSOUND_FREE , sound_effects[sample_number]);

	if (channel_number != -1)
	{
		
		if (loop)
		{
			FSOUND_SetLoopMode (channel_number,FSOUND_LOOP_NORMAL);
		}
		
		FSOUND_SetVolume(channel_number, volume);
		FSOUND_SetFrequency(channel_number, frequency);
		FSOUND_SetPan(channel_number, pan);

		return channel_number;
	}
	else
	{
		return UNSET;
	}	
}



void SOUND_adjust_sound (int channel_number, int volume, int frequency, int pan, bool use_music_global)
{
	if (use_music_global)
	{
		volume = int(float(volume)*global_music_level);
	}
	else
	{
		volume = int(float(volume)*global_sound_level);
	}

	FSOUND_SetVolume(channel_number, volume);
	FSOUND_SetFrequency(channel_number, frequency);
	FSOUND_SetPan(channel_number, pan);
}



void SOUND_stop_sound (int channel_number)
{
	// Only used for looping sounds, really.

	FSOUND_StopSound (channel_number);
}



void SOUND_set_global_sound_volume (int percentage)
{
	global_sound_level = float(percentage) / 10000.0f;
}



void SOUND_set_global_music_volume (int percentage)
{
	global_music_level = float(percentage) / 10000.0f;
}



int SOUND_get_global_sound_volume (void)
{
	return int (global_sound_level * 10000);
}



int SOUND_get_global_music_volume (void)
{
	return int (global_music_level * 10000);
}



// These are the stream functions, they're for MP3s and other compressed sound streams which are used
// as music (for the most part).



void SOUND_open_sound_streams (void)
{
	bool flag = true;
	char filename[MAX_NAME_SIZE];
	char full_filename[MAX_NAME_SIZE];

	int counter = 0;
	char *data_pointer;
	int data_length;

	int list_start,list_end;
	int list_pointer;

	GPL_list_extents ("STREAMS", &list_start, &list_end);
	char *extension_pointer = GPL_what_is_list_extension ("STREAMS");

	for (list_pointer = list_start; list_pointer < list_end ; list_pointer++)
	{
		if (load_from_dat_file)
		{
			sprintf (filename , "%s%s" , GPL_what_is_word_name (list_pointer) , extension_pointer );
			data_pointer = (char *) INPUT_get_stream_data_pointer (filename,&data_length);
			sound_streams[counter] = FSOUND_Stream_Open (data_pointer, FSOUND_LOADMEMORY,0,data_length);
		}
		else
		{
			sprintf (filename , "%s%s" , GPL_what_is_word_name (list_pointer) , extension_pointer );

			// Okay, check the file to see if it contains the "-arb" or "-set"

			append_filename(full_filename,"streams", filename, sizeof(full_filename) );

			sound_streams[counter] = FSOUND_Stream_Open (MAIN_get_project_filename(full_filename), 0,0,0);
		}
		
		counter++;
	}
}



void SOUND_close_sound_streams (void)
{
	// FILL ME SEYMOUR!
}



int SOUND_play_stream (int stream_number, int volume, int frequency, int pan, int loop_count, bool use_music_global)
{
	int channel_number;

	if (use_music_global)
	{
		volume = int(float(volume)*global_music_level);
	}
	else
	{
		volume = int(float(volume)*global_sound_level);
	}

	if (loop_count)
	{
		FSOUND_Stream_SetMode (sound_streams[stream_number],FSOUND_LOOP_NORMAL);
	}

	FSOUND_Stream_SetLoopCount (sound_streams[stream_number],loop_count);

	channel_number = FSOUND_Stream_Play (FSOUND_FREE , sound_streams[stream_number]);

	if (channel_number != -1)
	{
		FSOUND_SetVolume(channel_number, volume);
		FSOUND_SetFrequency(channel_number, frequency);
		FSOUND_SetPan(channel_number, pan);

		return channel_number;
	}
	else
	{
		return UNSET;
	}
}



void SOUND_stop_stream (int stream_number)
{
	FSOUND_Stream_Stop (sound_streams[stream_number]);
}


