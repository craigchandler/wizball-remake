#include <allegro.h>
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

void PLATFORM_WINDOW_shutdown(void)
{
	allegro_exit();
}
