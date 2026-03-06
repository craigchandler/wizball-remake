#include "sound.h"
#include "global_param_list.h"
#include "main.h"
#include "output.h"
#include "path_utils.h"
#include "string_size_constants.h"

#include <SDL.h>
#include <SDL_mixer.h>

#include <stdio.h>
#include <string.h>

#include "file_stuff.h"

static Mix_Chunk *sound_effects[MAX_SAMPLES];
static Mix_Chunk *sound_streams[MAX_STREAMS];
static int stream_channel_map[MAX_STREAMS];

int persistant_channel_array[MAX_CHANNELS_WANTED];
bool persistant_channel_array_active[MAX_CHANNELS_WANTED];
int current_persistant_channels = 0;

int fading_channel_array[MAX_FADER_CHANNELS];
bool fading_channel_array_active[MAX_FADER_CHANNELS];
int fading_channel_count = 0;

static bool sound_backend_ready = false;
static bool sdl_audio_initialized_here = false;

float global_sound_level = 0.5f;
float global_music_level = 0.5f;

static int sdl_preferred_driver = UNSET;
static int reserved_stream_channels = 8;

static void SOUND_log(const char *text)
{
	char line[MAX_LINE_SIZE];
	strncpy(line, text, MAX_LINE_SIZE - 1);
	line[MAX_LINE_SIZE - 1] = '\0';
	MAIN_add_to_log(line);
}

static int SOUND_clamp_int(int value, int min_value, int max_value)
{
	if (value < min_value)
	{
		return min_value;
	}
	if (value > max_value)
	{
		return max_value;
	}
	return value;
}

static int SOUND_scale_volume(int volume, bool use_music_global)
{
	float scaled_volume;

	volume = SOUND_clamp_int(volume, 0, 255);
	scaled_volume = float(volume);
	if (use_music_global)
	{
		scaled_volume *= global_music_level;
	}
	else
	{
		scaled_volume *= global_sound_level;
	}

	return SOUND_clamp_int(int(scaled_volume), 0, 255);
}

static int SOUND_to_mix_volume(int wizball_volume)
{
	/* WizBall volume is 0..255, SDL_mixer channel volume is 0..128. */
	return SOUND_clamp_int((wizball_volume * MIX_MAX_VOLUME) / 255, 0, MIX_MAX_VOLUME);
}

static void SOUND_apply_pan(int channel_number, int pan)
{
	Uint8 left;
	Uint8 right;

	pan = SOUND_clamp_int(pan, 0, 255);
	left = (Uint8)(255 - pan);
	right = (Uint8)pan;

	/* SDL_mixer panning is best-effort; ignore failures on unsupported backends. */
	Mix_SetPanning(channel_number, left, right);
}

static void SOUND_stop_channel_if_active(int channel_number)
{
	if (channel_number == UNSET)
	{
		return;
	}

	if (channel_number < 0)
	{
		return;
	}

	if (Mix_Playing(channel_number))
	{
		Mix_HaltChannel(channel_number);
	}
}

static int SOUND_pick_stream_channel(void)
{
	int channel;

	/* Streams/music use reserved channels [0 .. reserved_stream_channels-1]. */
	for (channel = 0; channel < reserved_stream_channels; channel++)
	{
		if (Mix_Playing(channel) == 0)
		{
			return channel;
		}
	}

	/* All reserved channels busy (likely overlapping fades); steal channel 0. */
	return 0;
}

static Mix_Chunk *SOUND_load_chunk_from_memory(char *data_pointer, int data_length)
{
	SDL_RWops *rw;

	if ((data_pointer == NULL) || (data_length <= 0))
	{
		return NULL;
	}

	rw = SDL_RWFromConstMem(data_pointer, data_length);
	if (rw == NULL)
	{
		return NULL;
	}

	return Mix_LoadWAV_RW(rw, 1);
}

static Mix_Chunk *SOUND_load_chunk_from_file(const char *filename)
{
	if (filename == NULL)
	{
		return NULL;
	}

	char lower_filename[TEXT_LINE_SIZE];
	lowercase_last_path_components(filename, lower_filename, sizeof(lower_filename), 1);
	if (strcmp(lower_filename, filename) != 0)
	{
		return Mix_LoadWAV(lower_filename);
	}

	/* If filename was already lower-case or unchanged, load the original. */
	return Mix_LoadWAV(filename);
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

void SOUND_add_persistant_channel(int channel_number)
{
	if (current_persistant_channels >= MAX_CHANNELS_WANTED)
	{
		return;
	}

	persistant_channel_array[current_persistant_channels] = channel_number;
	persistant_channel_array_active[current_persistant_channels] = true;
	current_persistant_channels++;
}

void SOUND_add_fader_channel(int channel_number)
{
	if (fading_channel_count >= MAX_FADER_CHANNELS)
	{
		return;
	}

	fading_channel_array[fading_channel_count] = channel_number;
	fading_channel_array_active[fading_channel_count] = true;
	fading_channel_count++;
}

void SOUND_stop_persistant_channels(void)
{
	int channel;

	for (channel = 0; channel < current_persistant_channels; channel++)
	{
		SOUND_stop_sound(persistant_channel_array[channel]);
	}

	current_persistant_channels = 0;
}

void SOUND_transfer_persistant_channels_to_fader_channels(void)
{
	int channel;

	for (channel = 0; channel < current_persistant_channels; channel++)
	{
		SOUND_add_fader_channel(persistant_channel_array[channel]);
	}

	current_persistant_channels = 0;
}

void SOUND_check_persistant_channel_validity(void)
{
	int channel;

	for (channel = 0; channel < current_persistant_channels; channel++)
	{
		if (persistant_channel_array_active[channel] == true)
		{
			if (Mix_Playing(persistant_channel_array[channel]) == 0)
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

void SOUND_remove_from_persistant_channels(int channel_number)
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

void SOUND_fade_channels(void)
{
	int channel;
	int volume;

	for (channel = 0; channel < fading_channel_count; channel++)
	{
		if (fading_channel_array_active[channel] == true)
		{
			if (Mix_Playing(fading_channel_array[channel]) == 0)
			{
				fading_channel_array_active[channel] = false;
				continue;
			}

			volume = Mix_Volume(fading_channel_array[channel], -1);
			volume -= SOUND_to_mix_volume(15);

			if (volume <= 0)
			{
				Mix_HaltChannel(fading_channel_array[channel]);
				fading_channel_array_active[channel] = false;
			}
			else
			{
				Mix_Volume(fading_channel_array[channel], volume);
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
	const char *current_driver;
	char line[MAX_LINE_SIZE];

	SOUND_reset_arrays();

	if ((sdl_preferred_driver != UNSET) && (sdl_preferred_driver >= 0))
	{
		const char *driver_name = SDL_GetAudioDriver(sdl_preferred_driver);
		if (driver_name != NULL)
		{
			SDL_setenv("SDL_AUDIODRIVER", driver_name, 1);
		}
	}

	if ((SDL_WasInit(SDL_INIT_AUDIO) & SDL_INIT_AUDIO) == 0)
	{
		if (SDL_InitSubSystem(SDL_INIT_AUDIO) != 0)
		{
			snprintf(line, sizeof(line), "SDL audio init failed: %s", SDL_GetError());
			SOUND_log(line);
			sound_backend_ready = false;
			return;
		}
		sdl_audio_initialized_here = true;
	}

	if (Mix_OpenAudio(44100, AUDIO_S16SYS, 2, 1024) != 0)
	{
		snprintf(line, sizeof(line), "SDL_mixer open failed: %s", Mix_GetError());
		SOUND_log(line);
		sound_backend_ready = false;
		return;
	}

	Mix_AllocateChannels(MAX_FADER_CHANNELS);
	if (reserved_stream_channels < 1)
	{
		reserved_stream_channels = 1;
	}
	if (reserved_stream_channels > MAX_FADER_CHANNELS)
	{
		reserved_stream_channels = MAX_FADER_CHANNELS;
	}
	Mix_ReserveChannels(reserved_stream_channels);

	sound_backend_ready = true;

	current_driver = SDL_GetCurrentAudioDriver();
	if (current_driver != NULL)
	{
		snprintf(line, sizeof(line), "SDL audio backend initialized (%s)", current_driver);
	}
	else
	{
		snprintf(line, sizeof(line), "SDL audio backend initialized");
	}
	SOUND_log(line);
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
			Mix_FreeChunk(sound_effects[counter]);
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
			Mix_FreeChunk(sound_streams[counter]);
			sound_streams[counter] = NULL;
		}
		stream_channel_map[counter] = UNSET;
		counter++;
	}

	Mix_HaltChannel(-1);
	Mix_CloseAudio();
	sound_backend_ready = false;

	if (sdl_audio_initialized_here)
	{
		SDL_QuitSubSystem(SDL_INIT_AUDIO);
		sdl_audio_initialized_here = false;
	}
}

void SOUND_load_sound_effects(void)
{
	int counter = 0;
	int loaded_count = 0;
	int failed_count = 0;
	int list_start;
	int list_end;
	int list_pointer;
	int data_length;
	char filename[MAX_LINE_SIZE];
	char full_filename[MAX_LINE_SIZE];
	char line[MAX_LINE_SIZE];
	char *extension_pointer;
	char *data_pointer;

	if (sound_backend_ready == false)
	{
		return;
	}

	GPL_list_extents("SOUND_FX", &list_start, &list_end);
	extension_pointer = GPL_what_is_list_extension("SOUND_FX");

	for (list_pointer = list_start; list_pointer < list_end; list_pointer++)
	{
		snprintf(filename, sizeof(filename), "%s%s", GPL_what_is_word_name(list_pointer), extension_pointer);

		PATH_UTIL_build_relative_path(full_filename, sizeof(full_filename), "sound_fx", filename);
		sound_effects[counter] = SOUND_load_chunk_from_file(MAIN_get_project_filename(full_filename));

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

	snprintf(line, sizeof(line), "SDL audio SFX load: loaded=%d failed=%d", loaded_count, failed_count);
	SOUND_log(line);
}

void SOUND_update_channels(void)
{
}

int SOUND_play_sound(int sample_number, int volume, int frequency, int pan, bool loop, bool use_music_global)
{
	Mix_Chunk *sample_pointer;
	int channel_number;
	int mixer_volume;

	(void)frequency;

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

	channel_number = Mix_PlayChannel(-1, sample_pointer, loop ? -1 : 0);
	if (channel_number < 0)
	{
		return UNSET;
	}

	mixer_volume = SOUND_to_mix_volume(SOUND_scale_volume(volume, use_music_global));
	Mix_Volume(channel_number, mixer_volume);
	SOUND_apply_pan(channel_number, pan);

	return channel_number;
}

void SOUND_adjust_sound(int channel_number, int volume, int frequency, int pan, bool use_music_global)
{
	int mixer_volume;

	(void)frequency;

	if (sound_backend_ready == false)
	{
		return;
	}

	if (channel_number < 0)
	{
		return;
	}

	mixer_volume = SOUND_to_mix_volume(SOUND_scale_volume(volume, use_music_global));
	Mix_Volume(channel_number, mixer_volume);
	SOUND_apply_pan(channel_number, pan);
}

void SOUND_stop_sound(int channel_number)
{
	if (sound_backend_ready == false)
	{
		return;
	}

	SOUND_stop_channel_if_active(channel_number);
}

int SOUND_play_stream(int stream_number, int volume, int frequency, int pan, int loop_count, bool use_music_global)
{
	Mix_Chunk *stream_sample;
	int channel_number;
	int mixer_volume;

	(void)frequency;

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

	if (stream_channel_map[stream_number] != UNSET)
	{
		SOUND_stop_channel_if_active(stream_channel_map[stream_number]);
		stream_channel_map[stream_number] = UNSET;
	}

	channel_number = SOUND_pick_stream_channel();
	channel_number = Mix_PlayChannel(channel_number, stream_sample, (loop_count != 0) ? -1 : 0);
	if (channel_number < 0)
	{
		return UNSET;
	}

	mixer_volume = SOUND_to_mix_volume(SOUND_scale_volume(volume, use_music_global));
	Mix_Volume(channel_number, mixer_volume);
	SOUND_apply_pan(channel_number, pan);

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
		SOUND_stop_channel_if_active(stream_channel_map[stream_number]);
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
	int data_length;
	char filename[MAX_LINE_SIZE];
	char full_filename[MAX_LINE_SIZE];
	char line[MAX_LINE_SIZE];
	char *extension_pointer;
	char *data_pointer;

	if (sound_backend_ready == false)
	{
		return;
	}

	GPL_list_extents("STREAMS", &list_start, &list_end);
	extension_pointer = GPL_what_is_list_extension("STREAMS");

	for (list_pointer = list_start; list_pointer < list_end; list_pointer++)
	{
		snprintf(filename, sizeof(filename), "%s%s", GPL_what_is_word_name(list_pointer), extension_pointer);

		PATH_UTIL_build_relative_path(full_filename, sizeof(full_filename), "streams", filename);
		sound_streams[counter] = SOUND_load_chunk_from_file(MAIN_get_project_filename(full_filename));
		if (sound_streams[counter] == NULL)
		{
			snprintf(line, sizeof(line), "SDL stream load failed: %.400s (err=%s)", filename, Mix_GetError());
			SOUND_log(line);
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

	snprintf(line, sizeof(line), "SDL audio stream load: loaded=%d failed=%d", loaded_count, failed_count);
	SOUND_log(line);
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
	sdl_preferred_driver = driver_index;
}
