#include "sound.h"

int current_persistant_channels = 0;
int fading_channel_count = 0;

void SOUND_setup(void) {}
void SOUND_shutdown(void) {}
void SOUND_load_sound_effects(void) {}
void SOUND_update_channels(void) {}

int SOUND_play_sound(int sample_number, int volume, int frequency, int pan, bool loop, bool use_music_global)
{
	(void)sample_number;
	(void)volume;
	(void)frequency;
	(void)pan;
	(void)loop;
	(void)use_music_global;
	return -1;
}

void SOUND_adjust_sound(int channel_number, int volume, int frequency, int pan, bool use_music_global)
{
	(void)channel_number;
	(void)volume;
	(void)frequency;
	(void)pan;
	(void)use_music_global;
}

void SOUND_stop_sound(int channel_number)
{
	(void)channel_number;
}

int SOUND_play_stream(int stream_number, int volume, int frequency, int pan, int loop_count, bool use_music_global)
{
	(void)stream_number;
	(void)volume;
	(void)frequency;
	(void)pan;
	(void)loop_count;
	(void)use_music_global;
	return -1;
}

void SOUND_stop_stream(int stream_number)
{
	(void)stream_number;
}

void SOUND_open_sound_streams(void) {}

void SOUND_add_persistant_channel(int channel_number)
{
	(void)channel_number;
}

void SOUND_stop_persistant_channels(void) {}
void SOUND_transfer_persistant_channels_to_fader_channels(void) {}
void SOUND_fade_channels(void) {}
void SOUND_check_persistant_channel_validity(void) {}

void SOUND_remove_from_persistant_channels(int channel_number)
{
	(void)channel_number;
}

void SOUND_add_fader_channel(int channel_number)
{
	(void)channel_number;
}

void SOUND_set_global_sound_volume(int percentage)
{
	(void)percentage;
}

void SOUND_set_global_music_volume(int percentage)
{
	(void)percentage;
}

int SOUND_get_global_sound_volume(void)
{
	return 0;
}

int SOUND_get_global_music_volume(void)
{
	return 0;
}
