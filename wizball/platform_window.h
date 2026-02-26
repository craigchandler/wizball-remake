#ifndef _PLATFORM_WINDOW_H_
#define _PLATFORM_WINDOW_H_

int PLATFORM_WINDOW_init(void);
void PLATFORM_WINDOW_set_text_mode(void);
int PLATFORM_WINDOW_set_windowed_mode(int width, int height, int colour_depth);
int PLATFORM_WINDOW_set_software_game_mode(bool windowed, int width, int height, int colour_depth);
int PLATFORM_WINDOW_set_opengl_game_mode(int width, int height, int colour_depth);
void PLATFORM_WINDOW_begin_text_screen(int red, int green, int blue);
void PLATFORM_WINDOW_end_text_screen(void);
void PLATFORM_WINDOW_shutdown(void);

#endif
