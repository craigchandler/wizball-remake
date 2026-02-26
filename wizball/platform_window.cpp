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

void PLATFORM_WINDOW_shutdown(void)
{
	allegro_exit();
}
