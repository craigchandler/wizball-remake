#include "sound.h"
#include "global_param_list.h"
#include "graphics.h"
#include "main.h"
#include "output.h"

#include <allegro.h>
#include <stdio.h>
#include <string.h>

static SAMPLE *sound_effects[MAX_SAMPLES];
static SAMPLE *sound_streams[MAX_STREAMS];
static int stream_channel_map[MAX_STREAMS];

int persistant_channel_array [MAX_CHANNELS_WANTED];
bool persistant_channel_array_active [MAX_CHANNELS_WANTED];
int current_persistant_channels = 0;

int fading_channel_array [MAX_FADER_CHANNELS];
bool fading_channel_array_active [MAX_FADER_CHANNELS];
int fading_channel_count = 0;

static bool sound_backend_ready = false;

float global_sound_level = 0.5f;
float global_music_level = 0.5f;

static void SOUND_add_to_log(const char *text)
{
	char line[MAX_LINE_SIZE];
	strncpy(line, text, MAX_LINE_SIZE - 1);
	line[MAX_LINE_SIZE - 1] = '\0';
	MAIN_add_to_log(line);
}

static void SOUND_reset_arrays(void)
{
	int i;

	for (i = 0; i < MAX_SAMPLES; i++)
	{
		sound_effects[i] = NULL;
	}

	for (i = 0; i < MAX_STREAMS; i++)
	{
		sound_streams[i] = NULL;
		stream_channel_map[i] = UNSET;
	}

	current_persistant_channels = 0;
	fading_channel_count = 0;
}

void SOUND_add_persistant_channel (int channel_number)
{
	if (current_persistant_channels >= MAX_CHANNELS_WANTED)
	{
		return;
	}

	persistant_channel_array[current_persistant_channels] = channel_number;
	persistant_channel_array_active[current_persistant_channels] = true;
	current_persistant_channels++;
}

void SOUND_add_fader_channel (int channel_number)
{
	if (fading_channel_count >= MAX_FADER_CHANNELS)
	{
		return;
	}

	fading_channel_array[fading_channel_count] = channel_number;
	fading_channel_array_active[fading_channel_count] = true;
	fading_channel_count++;
}

void SOUND_stop_persistant_channels (void)
{
	int channel;

	for (channel = 0; channel < current_persistant_channels; channel++)
	{
		voice_stop(persistant_channel_array[channel]);
	}

	current_persistant_channels = 0;
}

void SOUND_transfer_persistant_channels_to_fader_channels (void)
{
	int channel;

	for (channel = 0; channel < current_persistant_channels; channel++)
	{
		SOUND_add_fader_channel(persistant_channel_array[channel]);
	}

	current_persistant_channels = 0;
}

void SOUND_check_persistant_channel_validity (void)
{
	int channel;

	for (channel = 0; channel < current_persistant_channels; channel++)
	{
		if (persistant_channel_array_active[channel] == true)
		{
			if (voice_get_position(persistant_channel_array[channel]) < 0)
			{
				persistant_channel_array_active[channel] = false;
			}
		}
	}

	if (current_persistant_channels > 0)
	{
		channel = current_persistant_channels - 1;
		while ((channel >= 0) && (persistant_channel_array_active[channel] == false))
		{
			current_persistant_channels--;
			channel--;
		}
	}
}

void SOUND_remove_from_persistant_channels (int channel_number)
{
	int channel;
	for (channel = 0; channel < current_persistant_channels; channel++)
	{
		if (persistant_channel_array[channel] == channel_number)
		{
			persistant_channel_array_active[channel] = false;
		}
	}
}

void SOUND_fade_channels (void)
{
	int channel;
	int volume;

	for (channel = 0; channel < fading_channel_count; channel++)
	{
		if (fading_channel_array_active[channel] == true)
		{
			volume = voice_get_volume(fading_channel_array[channel]);
			volume -= 15;

			if (volume <= 0)
			{
				voice_stop(fading_channel_array[channel]);
				fading_channel_array_active[channel] = false;
			}
			else
			{
				voice_set_volume(fading_channel_array[channel], volume);
			}
		}
	}

	if (fading_channel_count > 0)
	{
		channel = fading_channel_count - 1;
		while ((channel >= 0) && (fading_channel_array_active[channel] == false))
		{
			fading_channel_count--;
			channel--;
		}
	}
}

void SOUND_setup(void)
{
	if (install_sound(DIGI_AUTODETECT, MIDI_NONE, NULL) != 0)
	{
		SOUND_add_to_log("Allegro sound init failed; continuing without audio.");
		sound_backend_ready = false;
		SOUND_reset_arrays();
		return;
	}

	sound_backend_ready = true;
	SOUND_reset_arrays();
	SOUND_add_to_log("Allegro sound backend initialized.");
}

void SOUND_shutdown(void)
{
	int counter;
	int list_start;
	int list_end;
	int list_pointer;

	if (sound_backend_ready == false)
	{
		return;
	}

	GPL_list_extents("SOUND_FX", &list_start, &list_end);
	counter = 0;
	for (list_pointer = list_start; list_pointer < list_end; list_pointer++)
	{
		if (sound_effects[counter] != NULL)
		{
			destroy_sample(sound_effects[counter]);
			sound_effects[counter] = NULL;
		}
		counter++;
	}

	GPL_list_extents("STREAMS", &list_start, &list_end);
	counter = 0;
	for (list_pointer = list_start; list_pointer < list_end; list_pointer++)
	{
		if (sound_streams[counter] != NULL)
		{
			destroy_sample(sound_streams[counter]);
			sound_streams[counter] = NULL;
		}
		stream_channel_map[counter] = UNSET;
		counter++;
	}

	remove_sound();
	sound_backend_ready = false;
}

void SOUND_load_sound_effects(void)
{
	int counter = 0;
	int loaded_count = 0;
	int failed_count = 0;
	int list_start;
	int list_end;
	int list_pointer;
	char filename[MAX_NAME_SIZE];
	char full_filename[MAX_LINE_SIZE];
	char pack_filename[MAX_LINE_SIZE];
	char line[MAX_LINE_SIZE];
	char *extension_pointer;

	if (sound_backend_ready == false)
	{
		return;
	}

	GPL_list_extents("SOUND_FX", &list_start, &list_end);
	extension_pointer = GPL_what_is_list_extension("SOUND_FX");

	for (list_pointer = list_start; list_pointer < list_end; list_pointer++)
	{
		sprintf(filename, "%s%s", GPL_what_is_word_name(list_pointer), extension_pointer);

		if (load_from_dat_file)
		{
			sprintf(pack_filename, "%s\\sfx.dat#%s", MAIN_get_pack_project_name(), filename);
			fix_filename_slashes(pack_filename);
			sound_effects[counter] = load_sample(pack_filename);
		}
		else
		{
			append_filename(full_filename, "sound_fx", filename, sizeof(full_filename));
			sound_effects[counter] = load_sample(MAIN_get_project_filename(full_filename));
		}

		if (sound_effects[counter] != NULL)
		{
			loaded_count++;
		}
		else
		{
			failed_count++;
		}

		counter++;
	}

	sprintf(line, "Audio SFX load: loaded=%d failed=%d", loaded_count, failed_count);
	SOUND_add_to_log(line);
}

void SOUND_update_channels(void)
{
}

int SOUND_play_sound(int sample_number, int volume, int frequency, int pan, bool loop, bool use_music_global)
{
	int channel_number;
	SAMPLE *sample_pointer;

	if (sound_backend_ready == false)
	{
		return UNSET;
	}

	if ((sample_number < 0) || (sample_number >= MAX_SAMPLES))
	{
		return UNSET;
	}

	sample_pointer = sound_effects[sample_number];
	if (sample_pointer == NULL)
	{
		return UNSET;
	}

	if (use_music_global)
	{
		volume = int(float(volume) * global_music_level);
	}
	else
	{
		volume = int(float(volume) * global_sound_level);
	}

	channel_number = play_sample(sample_pointer, volume, pan, frequency, loop ? 1 : 0);
	if (channel_number < 0)
	{
		return UNSET;
	}

	return channel_number;
}

void SOUND_adjust_sound(int channel_number, int volume, int frequency, int pan, bool use_music_global)
{
	if (sound_backend_ready == false)
	{
		return;
	}

	if (use_music_global)
	{
		volume = int(float(volume) * global_music_level);
	}
	else
	{
		volume = int(float(volume) * global_sound_level);
	}

	voice_set_volume(channel_number, volume);
	voice_set_pan(channel_number, pan);
	voice_set_frequency(channel_number, frequency);
}

void SOUND_stop_sound(int channel_number)
{
	if (sound_backend_ready == false)
	{
		return;
	}

	voice_stop(channel_number);
}

int SOUND_play_stream(int stream_number, int volume, int frequency, int pan, int loop_count, bool use_music_global)
{
	int channel_number;
	SAMPLE *stream_sample;

	if (sound_backend_ready == false)
	{
		return UNSET;
	}

	if ((stream_number < 0) || (stream_number >= MAX_STREAMS))
	{
		return UNSET;
	}

	stream_sample = sound_streams[stream_number];
	if (stream_sample == NULL)
	{
		return UNSET;
	}

	if (use_music_global)
	{
		volume = int(float(volume) * global_music_level);
	}
	else
	{
		volume = int(float(volume) * global_sound_level);
	}

	channel_number = play_sample(stream_sample, volume, pan, frequency, (loop_count != 0) ? 1 : 0);
	if (channel_number < 0)
	{
		return UNSET;
	}

	stream_channel_map[stream_number] = channel_number;
	return channel_number;
}

void SOUND_stop_stream(int stream_number)
{
	if (sound_backend_ready == false)
	{
		return;
	}

	if ((stream_number < 0) || (stream_number >= MAX_STREAMS))
	{
		return;
	}

	if (stream_channel_map[stream_number] != UNSET)
	{
		voice_stop(stream_channel_map[stream_number]);
		stream_channel_map[stream_number] = UNSET;
	}
}

void SOUND_open_sound_streams(void)
{
	int counter = 0;
	int loaded_count = 0;
	int failed_count = 0;
	int list_start;
	int list_end;
	int list_pointer;
	char filename[MAX_NAME_SIZE];
	char full_filename[MAX_LINE_SIZE];
	char pack_filename[MAX_LINE_SIZE];
	char line[MAX_LINE_SIZE];
	char *extension_pointer;

	if (sound_backend_ready == false)
	{
		return;
	}

	GPL_list_extents("STREAMS", &list_start, &list_end);
	extension_pointer = GPL_what_is_list_extension("STREAMS");

	for (list_pointer = list_start; list_pointer < list_end; list_pointer++)
	{
		sprintf(filename, "%s%s", GPL_what_is_word_name(list_pointer), extension_pointer);

		if (load_from_dat_file)
		{
			sprintf(pack_filename, "%s\\stream.dat#%s", MAIN_get_pack_project_name(), filename);
			fix_filename_slashes(pack_filename);
			sound_streams[counter] = load_sample(pack_filename);
		}
		else
		{
			append_filename(full_filename, "streams", filename, sizeof(full_filename));
			sound_streams[counter] = load_sample(MAIN_get_project_filename(full_filename));
		}

		if (sound_streams[counter] != NULL)
		{
			loaded_count++;
		}
		else
		{
			failed_count++;
		}

		counter++;
	}

	sprintf(line, "Audio stream load: loaded=%d failed=%d", loaded_count, failed_count);
	SOUND_add_to_log(line);
}

void SOUND_set_global_sound_volume(int percentage)
{
	global_sound_level = float(percentage) / 10000.0f;
}

void SOUND_set_global_music_volume(int percentage)
{
	global_music_level = float(percentage) / 10000.0f;
}

int SOUND_get_global_sound_volume(void)
{
	return int(global_sound_level * 10000);
}

int SOUND_get_global_music_volume(void)
{
	return int(global_music_level * 10000);
}

void SOUND_set_preferred_driver(int driver_index)
{
	(void)driver_index;
}
