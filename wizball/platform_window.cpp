#include <allegro.h>
#ifdef ALLEGRO_WINDOWS
#include <winalleg.h>
#include <windows.h>
#endif
#include "platform_window.h"

int PLATFORM_WINDOW_init(void)
{
	return allegro_init();
}

void PLATFORM_WINDOW_set_text_mode(void)
{
	set_gfx_mode(GFX_TEXT, 0, 0, 0, 0);
}

int PLATFORM_WINDOW_set_windowed_mode(int width, int height, int colour_depth)
{
	set_color_depth(colour_depth);
	set_color_conversion(COLORCONV_TOTAL + COLORCONV_KEEP_TRANS);
	return set_gfx_mode(GFX_AUTODETECT_WINDOWED, width, height, 0, 0);
}

int PLATFORM_WINDOW_set_software_game_mode(bool windowed, int width, int height, int colour_depth)
{
	set_color_depth(colour_depth);
	set_color_conversion(COLORCONV_TOTAL + COLORCONV_KEEP_TRANS);

	if (windowed)
	{
		return set_gfx_mode(GFX_AUTODETECT_WINDOWED, width, height, 0, 0);
	}

	return set_gfx_mode(GFX_AUTODETECT_FULLSCREEN, width, height, 0, 0);
}

int PLATFORM_WINDOW_set_opengl_game_mode(int width, int height, int colour_depth)
{
	set_color_depth(colour_depth);
	set_color_conversion(COLORCONV_TOTAL + COLORCONV_KEEP_TRANS);
	return set_gfx_mode(GFX_AUTODETECT_WINDOWED, width, height, 0, 0);
}

void PLATFORM_WINDOW_begin_text_screen(int red, int green, int blue)
{
	clear_to_color(screen, makecol(red, green, blue));
	acquire_screen();
	text_mode(-1);
}

void PLATFORM_WINDOW_end_text_screen(void)
{
	release_screen();
}

void PLATFORM_WINDOW_center_game_window(void)
{
#ifdef ALLEGRO_WINDOWS
	HWND hWindow = win_get_window();
	HWND hDesktop = GetDesktopWindow();

	RECT rcWindow, rcDesktop;

	GetWindowRect(hWindow, &rcWindow);
	GetWindowRect(hDesktop, &rcDesktop);

	int x = (rcDesktop.right - (rcWindow.right - rcWindow.left)) / 2;
	int y = (rcDesktop.bottom - (rcWindow.bottom - rcWindow.top)) / 2;

	SetWindowPos(hWindow, NULL, x, y, 0, 0, SWP_NOSIZE);
#endif
}

void PLATFORM_WINDOW_shutdown(void)
{
	allegro_exit();
}
