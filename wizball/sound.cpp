#include "sound.h"
#include "global_param_list.h"
#include "main.h"
#include "output.h"
#include "string_size_constants.h"

#include "fmod.h"

#include <allegro.h>
#include <stdio.h>
#include <string.h>

static FMOD_SYSTEM *fmod_system = NULL;
static FMOD_SOUND *sound_effects[MAX_SAMPLES];
static FMOD_SOUND *sound_streams[MAX_STREAMS];

static int stream_channel_handles[MAX_STREAMS];

typedef struct
{
	FMOD_CHANNEL *channel;
	bool active;
} channel_slot;

#define MAX_ACTIVE_CHANNEL_HANDLES (1024)
static channel_slot channel_slots[MAX_ACTIVE_CHANNEL_HANDLES];

int persistant_channel_array [MAX_CHANNELS_WANTED];
bool persistant_channel_array_active [MAX_CHANNELS_WANTED];
int current_persistant_channels = 0;

int fading_channel_array [MAX_FADER_CHANNELS];
bool fading_channel_array_active [MAX_FADER_CHANNELS];
int fading_channel_count = 0;

float global_sound_level = 0.5f;
float global_music_level = 0.5f;

static bool sound_backend_ready = false;
static int fmod_preferred_driver = UNSET;

static void SOUND_log(const char *text)
{
	char line[MAX_LINE_SIZE];
	strncpy(line, text, MAX_LINE_SIZE - 1);
	line[MAX_LINE_SIZE - 1] = '\0';
	MAIN_add_to_log(line);
}

static void SOUND_log_fmod_result(const char *prefix, FMOD_RESULT result)
{
	char line[MAX_LINE_SIZE];
	sprintf(line, "%s (FMOD error=%d)", prefix, (int)result);
	SOUND_log(line);
}

static const char * SOUND_output_type_to_string(FMOD_OUTPUTTYPE output_type)
{
	switch (output_type)
	{
	case FMOD_OUTPUTTYPE_AUTODETECT: return "AUTODETECT";
	case FMOD_OUTPUTTYPE_UNKNOWN: return "UNKNOWN";
	case FMOD_OUTPUTTYPE_NOSOUND: return "NOSOUND";
	case FMOD_OUTPUTTYPE_WAVWRITER: return "WAVWRITER";
	case FMOD_OUTPUTTYPE_NOSOUND_NRT: return "NOSOUND_NRT";
	case FMOD_OUTPUTTYPE_WAVWRITER_NRT: return "WAVWRITER_NRT";
	case FMOD_OUTPUTTYPE_DSOUND: return "DSOUND";
	case FMOD_OUTPUTTYPE_WINMM: return "WINMM";
	case FMOD_OUTPUTTYPE_WASAPI: return "WASAPI";
	case FMOD_OUTPUTTYPE_ASIO: return "ASIO";
	case FMOD_OUTPUTTYPE_OSS: return "OSS";
	case FMOD_OUTPUTTYPE_ALSA: return "ALSA";
	case FMOD_OUTPUTTYPE_ESD: return "ESD";
	case FMOD_OUTPUTTYPE_PULSEAUDIO: return "PULSEAUDIO";
	case FMOD_OUTPUTTYPE_COREAUDIO: return "COREAUDIO";
	default: return "OTHER";
	}
}

static int SOUND_find_fmod_driver_by_name(FMOD_SYSTEM *system, int driver_count, const char *needle)
{
	int i;
	char driver_name[256];

	if ((system == NULL) || (needle == NULL))
	{
		return UNSET;
	}

	for (i = 0; i < driver_count; i++)
	{
		memset(driver_name, 0, sizeof(driver_name));
		if (FMOD_System_GetDriverInfo(system, i, driver_name, sizeof(driver_name), NULL) == FMOD_OK)
		{
			if (strcmp(driver_name, needle) == 0)
			{
				return i;
			}
		}
	}

	return UNSET;
}

static void SOUND_clear_channel_slots(void)
{
	int i;
	for (i = 0; i < MAX_ACTIVE_CHANNEL_HANDLES; i++)
	{
		channel_slots[i].channel = NULL;
		channel_slots[i].active = false;
	}
}

static int SOUND_register_channel(FMOD_CHANNEL *channel)
{
	int i;
	for (i = 0; i < MAX_ACTIVE_CHANNEL_HANDLES; i++)
	{
		if (channel_slots[i].active == false)
		{
			channel_slots[i].channel = channel;
			channel_slots[i].active = true;
			return i;
		}
	}

	return UNSET;
}

static FMOD_CHANNEL * SOUND_get_channel_from_handle(int channel_handle)
{
	if ((channel_handle < 0) || (channel_handle >= MAX_ACTIVE_CHANNEL_HANDLES))
	{
		return NULL;
	}

	if (channel_slots[channel_handle].active == false)
	{
		return NULL;
	}

	return channel_slots[channel_handle].channel;
}

static void SOUND_release_channel_handle(int channel_handle)
{
	if ((channel_handle < 0) || (channel_handle >= MAX_ACTIVE_CHANNEL_HANDLES))
	{
		return;
	}

	channel_slots[channel_handle].active = false;
	channel_slots[channel_handle].channel = NULL;
}

static bool SOUND_is_channel_playing(int channel_handle)
{
	FMOD_CHANNEL *channel;
	FMOD_BOOL is_playing = 0;
	FMOD_RESULT result;

	channel = SOUND_get_channel_from_handle(channel_handle);
	if (channel == NULL)
	{
		return false;
	}

	result = FMOD_Channel_IsPlaying(channel, &is_playing);
	if (result != FMOD_OK)
	{
		SOUND_release_channel_handle(channel_handle);
		return false;
	}

	return (is_playing != 0);
}

static float SOUND_clamp_unit_volume(float value)
{
	if (value < 0.0f)
	{
		return 0.0f;
	}
	if (value > 1.0f)
	{
		return 1.0f;
	}
	return value;
}

static float SOUND_to_fmod_volume(int volume, bool use_music_global)
{
	float base = float(volume) / 255.0f;
	if (use_music_global)
	{
		base *= global_music_level;
	}
	else
	{
		base *= global_sound_level;
	}
	return SOUND_clamp_unit_volume(base);
}

static float SOUND_to_fmod_pan(int pan)
{
	/* Legacy range is 0..255 with center near 128. FMOD expects -1..1. */
	float mapped = (float(pan) - 128.0f) / 127.0f;
	if (mapped < -1.0f)
	{
		mapped = -1.0f;
	}
	if (mapped > 1.0f)
	{
		mapped = 1.0f;
	}
	return mapped;
}

static bool SOUND_create_sound_from_memory(FMOD_SOUND **out_sound, char *data_pointer, int data_length, bool stream)
{
	FMOD_CREATESOUNDEXINFO exinfo;
	FMOD_MODE mode;
	FMOD_RESULT result;

	if ((out_sound == NULL) || (data_pointer == NULL) || (data_length <= 0))
	{
		return false;
	}

	memset(&exinfo, 0, sizeof(exinfo));
	exinfo.cbsize = sizeof(exinfo);
	exinfo.length = (unsigned int)data_length;

	mode = FMOD_SOFTWARE | FMOD_2D | FMOD_OPENMEMORY;
	if (stream)
	{
		mode |= FMOD_CREATESTREAM;
		result = FMOD_System_CreateStream(fmod_system, data_pointer, mode, &exinfo, out_sound);
	}
	else
	{
		result = FMOD_System_CreateSound(fmod_system, data_pointer, mode, &exinfo, out_sound);
	}

	return (result == FMOD_OK);
}

static bool SOUND_create_sound_from_file(FMOD_SOUND **out_sound, char *filename, bool stream)
{
	FMOD_MODE mode;
	FMOD_RESULT result;

	if ((out_sound == NULL) || (filename == NULL))
	{
		return false;
	}

	mode = FMOD_SOFTWARE | FMOD_2D;
	if (stream)
	{
		mode |= FMOD_CREATESTREAM;
		result = FMOD_System_CreateStream(fmod_system, filename, mode, NULL, out_sound);
	}
	else
	{
		result = FMOD_System_CreateSound(fmod_system, filename, mode, NULL, out_sound);
	}

	return (result == FMOD_OK);
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
		SOUND_stop_sound(persistant_channel_array[channel]);
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

	if (sound_backend_ready == false)
	{
		return;
	}

	for (channel = 0; channel < current_persistant_channels; channel++)
	{
		if (persistant_channel_array_active[channel] == true)
		{
			if (SOUND_is_channel_playing(persistant_channel_array[channel]) == false)
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

	FMOD_System_Update(fmod_system);
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
	float volume;
	FMOD_CHANNEL *fmod_channel;

	if (sound_backend_ready == false)
	{
		return;
	}

	for (channel = 0; channel < fading_channel_count; channel++)
	{
		if (fading_channel_array_active[channel] == true)
		{
			if (SOUND_is_channel_playing(fading_channel_array[channel]) == false)
			{
				fading_channel_array_active[channel] = false;
				continue;
			}

			fmod_channel = SOUND_get_channel_from_handle(fading_channel_array[channel]);
			if (fmod_channel == NULL)
			{
				fading_channel_array_active[channel] = false;
				continue;
			}

			if (FMOD_Channel_GetVolume(fmod_channel, &volume) != FMOD_OK)
			{
				fading_channel_array_active[channel] = false;
				SOUND_release_channel_handle(fading_channel_array[channel]);
				continue;
			}

			volume -= (15.0f / 255.0f);
			if (volume <= 0.0f)
			{
				FMOD_Channel_Stop(fmod_channel);
				SOUND_release_channel_handle(fading_channel_array[channel]);
				fading_channel_array_active[channel] = false;
			}
			else
			{
				FMOD_Channel_SetVolume(fmod_channel, volume);
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

	FMOD_System_Update(fmod_system);
}

void SOUND_setup (void)
{
	int i;
	int driver_count = 0;
	int current_driver = UNSET;
	int driver_index;
	FMOD_RESULT result;
	FMOD_OUTPUTTYPE output_type = FMOD_OUTPUTTYPE_UNKNOWN;
	char line[MAX_LINE_SIZE];
	char driver_name[256];

	for (i = 0; i < MAX_SAMPLES; i++)
	{
		sound_effects[i] = NULL;
	}
	for (i = 0; i < MAX_STREAMS; i++)
	{
		sound_streams[i] = NULL;
		stream_channel_handles[i] = UNSET;
	}
	SOUND_clear_channel_slots();

	result = FMOD_System_Create(&fmod_system);
	if (result != FMOD_OK)
	{
		SOUND_log_fmod_result("FMOD_System_Create failed", result);
		sound_backend_ready = false;
		return;
	}

	result = FMOD_System_GetOutput(fmod_system, &output_type);
	if (result == FMOD_OK)
	{
		sprintf(line, "FMOD output type: %s", SOUND_output_type_to_string(output_type));
		SOUND_log(line);
	}

	result = FMOD_System_GetNumDrivers(fmod_system, &driver_count);
	if (result == FMOD_OK)
	{
		sprintf(line, "FMOD driver count: %d", driver_count);
		SOUND_log(line);
	}

	if ((fmod_preferred_driver != UNSET) && (driver_count > 0))
	{
		if ((fmod_preferred_driver >= 0) && (fmod_preferred_driver < driver_count))
		{
			result = FMOD_System_SetDriver(fmod_system, fmod_preferred_driver);
			if (result == FMOD_OK)
			{
				sprintf(line, "FMOD preferred driver index set: %d", fmod_preferred_driver);
				SOUND_log(line);
			}
			else
			{
				SOUND_log_fmod_result("FMOD_System_SetDriver failed", result);
			}
		}
		else
		{
			sprintf(line, "FMOD preferred driver index out of range: %d", fmod_preferred_driver);
			SOUND_log(line);
		}
	}
	else if (driver_count > 0)
	{
		int auto_driver = SOUND_find_fmod_driver_by_name(fmod_system, driver_count, "pipewire");
		if (auto_driver == UNSET)
		{
			auto_driver = SOUND_find_fmod_driver_by_name(fmod_system, driver_count, "pulse");
		}

		if (auto_driver != UNSET)
		{
			result = FMOD_System_SetDriver(fmod_system, auto_driver);
			if (result == FMOD_OK)
			{
				sprintf(line, "FMOD auto-selected driver index: %d", auto_driver);
				SOUND_log(line);
			}
			else
			{
				SOUND_log_fmod_result("FMOD auto driver selection failed", result);
			}
		}
	}

	result = FMOD_System_Init(fmod_system, MAX_CHANNELS_WANTED, FMOD_INIT_NORMAL, NULL);
	if (result != FMOD_OK)
	{
		SOUND_log_fmod_result("FMOD_System_Init failed", result);
		FMOD_System_Release(fmod_system);
		fmod_system = NULL;
		sound_backend_ready = false;
		return;
	}

	sound_backend_ready = true;
	SOUND_log("FMOD Ex backend initialized.");

	result = FMOD_System_GetDriver(fmod_system, &current_driver);
	if (result == FMOD_OK)
	{
		sprintf(line, "FMOD current driver index: %d", current_driver);
		SOUND_log(line);

		if ((current_driver >= 0) && (current_driver < driver_count))
		{
			memset(driver_name, 0, sizeof(driver_name));
			if (FMOD_System_GetDriverInfo(fmod_system, current_driver, driver_name, sizeof(driver_name), NULL) == FMOD_OK)
			{
				sprintf(line, "FMOD current driver name: %s", driver_name);
				SOUND_log(line);
			}
		}
	}

	for (driver_index = 0; driver_index < driver_count; driver_index++)
	{
		memset(driver_name, 0, sizeof(driver_name));
		if (FMOD_System_GetDriverInfo(fmod_system, driver_index, driver_name, sizeof(driver_name), NULL) == FMOD_OK)
		{
			sprintf(line, "FMOD driver[%d]: %s", driver_index, driver_name);
			SOUND_log(line);
		}
	}
}

void SOUND_shutdown (void)
{
	int counter;
	int list_start, list_end;
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
			FMOD_Sound_Release(sound_effects[counter]);
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
			FMOD_Sound_Release(sound_streams[counter]);
			sound_streams[counter] = NULL;
		}
		stream_channel_handles[counter] = UNSET;
		counter++;
	}

	FMOD_System_Close(fmod_system);
	FMOD_System_Release(fmod_system);
	fmod_system = NULL;
	sound_backend_ready = false;
}

void SOUND_load_sound_effects (void)
{
	int counter = 0;
	int loaded_count = 0;
	int failed_count = 0;
	int list_start, list_end, list_pointer;
	int data_length;
	char *data_pointer;
	char filename[MAX_LINE_SIZE];
	char full_filename[MAX_LINE_SIZE];
	char *extension_pointer;
	char line[MAX_LINE_SIZE];

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
			data_pointer = (char *) INPUT_get_sfx_data_pointer(filename, &data_length);
			if (!SOUND_create_sound_from_memory(&sound_effects[counter], data_pointer, data_length, false))
			{
				failed_count++;
			}
			else
			{
				loaded_count++;
			}
		}
		else
		{
			append_filename(full_filename, "sound_fx", filename, sizeof(full_filename));
			if (!SOUND_create_sound_from_file(&sound_effects[counter], MAIN_get_project_filename(full_filename), false))
			{
				failed_count++;
			}
			else
			{
				loaded_count++;
			}
		}

		counter++;
	}

	sprintf(line, "FMOD SFX load: loaded=%d failed=%d", loaded_count, failed_count);
	SOUND_log(line);
}

void SOUND_update_channels (void)
{
	if (sound_backend_ready)
	{
		FMOD_System_Update(fmod_system);
	}
}

int SOUND_play_sound (int sample_number, int volume, int frequency, int pan, bool loop, bool use_music_global)
{
	FMOD_CHANNEL *channel = NULL;
	FMOD_RESULT result;
	FMOD_MODE channel_mode = FMOD_LOOP_OFF;
	int channel_handle;

	if (sound_backend_ready == false)
	{
		return UNSET;
	}

	if ((sample_number < 0) || (sample_number >= MAX_SAMPLES))
	{
		return UNSET;
	}

	if (sound_effects[sample_number] == NULL)
	{
		return UNSET;
	}

	result = FMOD_System_PlaySound(fmod_system, FMOD_CHANNEL_FREE, sound_effects[sample_number], 0, &channel);
	if (result != FMOD_OK || channel == NULL)
	{
		return UNSET;
	}

	if (loop)
	{
		channel_mode = FMOD_LOOP_NORMAL;
	}
	result = FMOD_Channel_SetMode(channel, channel_mode);
	(void)result;

	FMOD_Channel_SetVolume(channel, SOUND_to_fmod_volume(volume, use_music_global));
	FMOD_Channel_SetFrequency(channel, (float)frequency);
	FMOD_Channel_SetPan(channel, SOUND_to_fmod_pan(pan));

	channel_handle = SOUND_register_channel(channel);
	if (channel_handle == UNSET)
	{
		FMOD_Channel_Stop(channel);
		return UNSET;
	}

	return channel_handle;
}

void SOUND_adjust_sound (int channel_number, int volume, int frequency, int pan, bool use_music_global)
{
	FMOD_CHANNEL *channel;

	if (sound_backend_ready == false)
	{
		return;
	}

	channel = SOUND_get_channel_from_handle(channel_number);
	if (channel == NULL)
	{
		return;
	}

	FMOD_Channel_SetVolume(channel, SOUND_to_fmod_volume(volume, use_music_global));
	FMOD_Channel_SetFrequency(channel, (float)frequency);
	FMOD_Channel_SetPan(channel, SOUND_to_fmod_pan(pan));
}

void SOUND_stop_sound (int channel_number)
{
	FMOD_CHANNEL *channel;

	if (sound_backend_ready == false)
	{
		return;
	}

	channel = SOUND_get_channel_from_handle(channel_number);
	if (channel == NULL)
	{
		return;
	}

	FMOD_Channel_Stop(channel);
	SOUND_release_channel_handle(channel_number);
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
	return int(global_sound_level * 10000);
}

int SOUND_get_global_music_volume (void)
{
	return int(global_music_level * 10000);
}

void SOUND_open_sound_streams (void)
{
	int counter = 0;
	int loaded_count = 0;
	int failed_count = 0;
	int list_start, list_end, list_pointer;
	int data_length;
	char *data_pointer;
	char filename[MAX_LINE_SIZE];
	char full_filename[MAX_LINE_SIZE];
	char *extension_pointer;
	char line[MAX_LINE_SIZE];

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
			data_pointer = (char *) INPUT_get_stream_data_pointer(filename, &data_length);
			if (!SOUND_create_sound_from_memory(&sound_streams[counter], data_pointer, data_length, true))
			{
				failed_count++;
			}
			else
			{
				loaded_count++;
			}
		}
		else
		{
			append_filename(full_filename, "streams", filename, sizeof(full_filename));
			if (!SOUND_create_sound_from_file(&sound_streams[counter], MAIN_get_project_filename(full_filename), true))
			{
				failed_count++;
			}
			else
			{
				loaded_count++;
			}
		}

		counter++;
	}

	sprintf(line, "FMOD stream load: loaded=%d failed=%d", loaded_count, failed_count);
	SOUND_log(line);
}

int SOUND_play_stream (int stream_number, int volume, int frequency, int pan, int loop_count, bool use_music_global)
{
	FMOD_CHANNEL *channel = NULL;
	FMOD_RESULT result;
	FMOD_MODE mode = FMOD_LOOP_OFF;
	int channel_handle;

	if (sound_backend_ready == false)
	{
		return UNSET;
	}

	if ((stream_number < 0) || (stream_number >= MAX_STREAMS))
	{
		return UNSET;
	}

	if (sound_streams[stream_number] == NULL)
	{
		return UNSET;
	}

	if (stream_channel_handles[stream_number] != UNSET)
	{
		SOUND_stop_sound(stream_channel_handles[stream_number]);
		stream_channel_handles[stream_number] = UNSET;
	}

	result = FMOD_System_PlaySound(fmod_system, FMOD_CHANNEL_FREE, sound_streams[stream_number], 0, &channel);
	if (result != FMOD_OK || channel == NULL)
	{
		return UNSET;
	}

	if (loop_count != 0)
	{
		mode = FMOD_LOOP_NORMAL;
	}
	FMOD_Channel_SetMode(channel, mode);
	FMOD_Channel_SetLoopCount(channel, loop_count);
	FMOD_Channel_SetVolume(channel, SOUND_to_fmod_volume(volume, use_music_global));
	FMOD_Channel_SetFrequency(channel, (float)frequency);
	FMOD_Channel_SetPan(channel, SOUND_to_fmod_pan(pan));

	channel_handle = SOUND_register_channel(channel);
	if (channel_handle == UNSET)
	{
		FMOD_Channel_Stop(channel);
		return UNSET;
	}

	stream_channel_handles[stream_number] = channel_handle;
	return channel_handle;
}

void SOUND_stop_stream (int stream_number)
{
	if ((stream_number < 0) || (stream_number >= MAX_STREAMS))
	{
		return;
	}

	if (stream_channel_handles[stream_number] != UNSET)
	{
		SOUND_stop_sound(stream_channel_handles[stream_number]);
		stream_channel_handles[stream_number] = UNSET;
	}
}

void SOUND_set_preferred_driver (int driver_index)
{
	fmod_preferred_driver = driver_index;
}
