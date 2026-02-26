#include <alleggl.h>

#include "platform_renderer.h"

void PLATFORM_RENDERER_prepare_allegro_gl(bool windowed, int colour_depth)
{
	install_allegro_gl();
	allegro_gl_clear_settings();

	allegro_gl_set(AGL_COLOR_DEPTH, colour_depth);
	allegro_gl_set(AGL_Z_DEPTH, 8);
	allegro_gl_set(AGL_DOUBLEBUFFER, 1);
	allegro_gl_set(AGL_RENDERMETHOD, 1);

	if (windowed)
	{
		allegro_gl_set(AGL_WINDOWED, TRUE);
		allegro_gl_set(AGL_SUGGEST, AGL_COLOR_DEPTH | AGL_DOUBLEBUFFER | AGL_RENDERMETHOD | AGL_Z_DEPTH | AGL_WINDOWED);
	}
	else
	{
		allegro_gl_set(AGL_WINDOWED, FALSE);
		allegro_gl_set(AGL_SUGGEST, AGL_COLOR_DEPTH | AGL_DOUBLEBUFFER | AGL_RENDERMETHOD | AGL_Z_DEPTH | AGL_WINDOWED);
	}
}

void PLATFORM_RENDERER_shutdown(void)
{
}
