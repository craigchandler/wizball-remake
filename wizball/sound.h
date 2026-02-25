#ifndef _SOUND_H_
#define _SOUND_H_

void SOUND_setup (void);
void SOUND_shutdown (void);
void SOUND_load_sound_effects (void);
void SOUND_update_channels (void);
int SOUND_play_sound (int sample_number, int volume, int frequency, int pan, bool loop, bool use_music_global=false);
void SOUND_adjust_sound (int channel_number, int volume, int frequency, int pan, bool use_music_global=false);
void SOUND_stop_sound (int channel_number);

int SOUND_play_stream (int stream_number, int volume, int frequency, int pan, int loop_count, bool use_music_global=false);
void SOUND_stop_stream (int stream_number);
void SOUND_open_sound_streams (void);

void SOUND_add_persistant_channel (int channel_number);
void SOUND_stop_persistant_channels (void);
void SOUND_transfer_persistant_channels_to_fader_channels (void);
void SOUND_fade_channels (void);
void SOUND_check_persistant_channel_validity (void);
void SOUND_remove_from_persistant_channels (int channel_number);
void SOUND_add_fader_channel (int channel_number);

void SOUND_set_global_sound_volume (int percentage);
void SOUND_set_global_music_volume (int percentage);
int SOUND_get_global_sound_volume (void);
int SOUND_get_global_music_volume (void);
void SOUND_set_preferred_driver (int driver_index);

extern int current_persistant_channels;
extern int fading_channel_count;

#define MAX_SAMPLES				(1024)
#define MAX_STREAMS				MAX_SAMPLES
#define MAX_CHANNELS_WANTED		(32)
#define MAX_FADER_CHANNELS		(128)

#endif
