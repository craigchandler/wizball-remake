#include <allegro.h>
#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <ctype.h>

#include "string_size_constants.h"
#include "output.h"
#include "file_stuff.h"
#include "math_stuff.h"
#include "scripting.h"
#include "graphics.h"
#include "tilemaps.h"
#include "tilesets.h"
#include "main.h"
#include "control.h"
#include "particles.h"
#include "string_stuff.h"
#include "global_param_list.h"
#include "object_collision.h"
#include "platform_window.h"
#include "platform_renderer.h"

#include "fortify.h"

typedef struct {
	char name[NAME_SIZE];
	int parent_bitmap;
	int x;
	int y;
	int pivot_x;
	int pivot_y;
	int width;
	int height;
	float u1;
	float u2;
	float v1;
	float v2;
	float uv_pixel_size;
} sprite;



typedef struct {
	BITMAP *image;
	unsigned int texture_handle;
	int width;
	int height;
	int sprite_count;
	sprite *sprite_list;
} bitmap_holder;



typedef struct {
	int current_x;
	int current_y;
	int current_z;
	int texture;
	bool blend;
	int blend_type;
	int vertex_colour_red;
	int vertex_colour_green;
	int vertex_colour_blue;
	float current_alpha;
} rendering_status;



typedef struct 
{
	float red[4];
	float green[4];
	float blue[4];
} vertex_colour_struct;



vertex_colour_struct entity_vertex_colours[MAX_ENTITIES];


float float_window_scale_multiplier;

rendering_status rs;

BITMAP *pages[3] = {NULL, NULL, NULL};
BITMAP *active_page;
BITMAP *current_page;

int update_method;
bool wait_for_vsync = false;

bitmap_holder *bmps = NULL;
// The thing that holds all the bitmaps.
int total_bitmaps_loaded = 0;
int total_bitmaps_allocated = 0;

// This is the size of the created window.
int game_screen_width = UNSET;
int game_screen_height = UNSET;

// And this is the size of the window within it. If they do
// not match then all calls to the window drawing function
// are scaled.

int virtual_screen_width;
int virtual_screen_height;

int window_scale_multiplier;

bool texture_combiner_available;
bool best_texture_combiner_available;

bool enable_multi_texture_effects_ideally = true;

bool secondary_colour_available;
static bool output_sdl_effect_trace_checked_env = false;
static bool output_sdl_effect_trace_enabled = false;
static unsigned int output_sdl_effect_trace_frame_counter = 0;

bool software_mode_active = false;

DATAFILE *data_dat = NULL;
DATAFILE *sfx_dat = NULL;
DATAFILE *gfx_dat = NULL;
DATAFILE *stream_dat = NULL;
DATAFILE *music_dat = NULL;

static bool OUTPUT_env_enabled(const char *name)
{
	const char *value = getenv(name);
	if (value == NULL)
	{
		return false;
	}

	if ((strcmp(value, "1") == 0) || (strcmp(value, "true") == 0) || (strcmp(value, "TRUE") == 0) || (strcmp(value, "yes") == 0) || (strcmp(value, "on") == 0))
	{
		return true;
	}

	return false;
}

static bool OUTPUT_is_sdl_effect_trace_enabled(void)
{
	if (!output_sdl_effect_trace_checked_env)
	{
		output_sdl_effect_trace_enabled = OUTPUT_env_enabled("WIZBALL_SDL2_EFFECT_TRACE");
		output_sdl_effect_trace_checked_env = true;
	}
	return output_sdl_effect_trace_enabled;
}

static void OUTPUT_sanitize_window_transform_values(
	int window_number,
	const char *context,
	float *left_window_transform_x,
	float *top_window_transform_y,
	float *total_scale_x,
	float *total_scale_y)
{
	static unsigned int transform_sanitize_counter = 0;
	const float min_abs_scale = 0.0001f;
	bool corrected = false;
	float raw_left;
	float raw_top;
	float raw_scale_x;
	float raw_scale_y;

	if ((left_window_transform_x == NULL) ||
		(top_window_transform_y == NULL) ||
		(total_scale_x == NULL) ||
		(total_scale_y == NULL))
	{
		return;
	}

	raw_left = *left_window_transform_x;
	raw_top = *top_window_transform_y;
	raw_scale_x = *total_scale_x;
	raw_scale_y = *total_scale_y;

	if (!isfinite(*left_window_transform_x))
	{
		*left_window_transform_x = 0.0f;
		corrected = true;
	}
	if (!isfinite(*top_window_transform_y))
	{
		*top_window_transform_y = 0.0f;
		corrected = true;
	}
	if (!isfinite(*total_scale_x) || (fabsf(*total_scale_x) < min_abs_scale))
	{
		*total_scale_x = 1.0f;
		corrected = true;
	}
	if (!isfinite(*total_scale_y) || (fabsf(*total_scale_y) < min_abs_scale))
	{
		*total_scale_y = 1.0f;
		corrected = true;
	}

	if (corrected)
	{
		transform_sanitize_counter++;
		if ((transform_sanitize_counter <= 200) || ((transform_sanitize_counter % 1000) == 0))
		{
			fprintf(
				stderr,
				"[OUTPUT-TRANSFORM] n=%u window=%d context=%s raw_left=%.3f raw_top=%.3f raw_scale=%.6f,%.6f fixed_left=%.3f fixed_top=%.3f fixed_scale=%.6f,%.6f\n",
				transform_sanitize_counter,
				window_number,
				(context != NULL) ? context : "-",
				raw_left,
				raw_top,
				raw_scale_x,
				raw_scale_y,
				*left_window_transform_x,
				*top_window_transform_y,
				*total_scale_x,
				*total_scale_y);
		}
	}
}

void * INPUT_get_stream_data_pointer(char *object_name, int *data_length)
{
	DATAFILE *data_pointer = find_datafile_object(stream_dat, object_name);
	*data_length = data_pointer->size;
	return (data_pointer->dat);
}



void * INPUT_get_sfx_data_pointer(char *object_name, int *data_length)
{
	DATAFILE *data_pointer = find_datafile_object(sfx_dat, object_name);
	*data_length = data_pointer->size;
	return (data_pointer->dat);
}



bool INPUT_load_datafile (void)
{
	data_dat = load_datafile(MAIN_get_pack_filename("data.dat")); 

	if (data_dat != NULL)
	{
		return true;
	}
	else
	{
		return false;
	}
}



void INPUT_load_media_datafiles (void)
{
	sfx_dat = load_datafile(MAIN_get_pack_filename("sfx.dat"));
	gfx_dat = load_datafile(MAIN_get_pack_filename("gfx.dat"));
	stream_dat = load_datafile(MAIN_get_pack_filename("stream.dat"));
	music_dat = load_datafile(MAIN_get_pack_filename("music.dat"));
}



void INPUT_unload_datafiles (void)
{
	unload_datafile (data_dat);
	unload_datafile (sfx_dat);
	unload_datafile (gfx_dat);
	unload_datafile (stream_dat);
	unload_datafile (music_dat);
}



void OUTPUT_set_virtual_screen_size (int width, int height)
{
	virtual_screen_width = width;
	virtual_screen_height = height;
}



void OUTPUT_message (char *msg)
{
	allegro_message (msg);
}



void OUTPUT_rectangle (int x1, int y1, int x2, int y2, int r, int g, int b)
{
	if (software_mode_active)
	{
		rect (active_page , x1 , y1 , x2 , y2 , makecol(r,g,b) );
	}
	else
	{
		PLATFORM_RENDERER_draw_outline_rect(x1, y1, x2, y2, r, g, b, virtual_screen_height);
	}
}



void OUTPUT_rectangle_by_size (int x, int y, int width, int height, int r, int g, int b)
{
	OUTPUT_rectangle (x, y, x+width-1, y+height-1, r, g, b);
}



void OUTPUT_filled_rectangle (int x1, int y1, int x2, int y2, int r, int g, int b)
{

	if (software_mode_active)
	{
		rectfill (active_page , x1 , y1 , x2 , y2 , makecol(r,g,b) );
	}
	else
	{
		PLATFORM_RENDERER_draw_filled_rect(x1, y1, x2, y2, r, g, b, virtual_screen_height);
	}
	
}



void OUTPUT_filled_rectangle_by_size (int x, int y, int width, int height, int r, int g, int b)
{
	OUTPUT_filled_rectangle (x, y, x+width-1, y+height-1, r, g, b);
}



void OUTPUT_text (int x, int y, char *text, int r, int g, int b, int scale)
{
	if (software_mode_active)
	{
		textout (active_page , font, text, x , y , makecol(r,g,b) );
	}
	else
	{
		int c;
		int length = strlen(text);
		float scale_multiplier = float(scale) / 10000.0f;
		float pen_x = 0.0f;

		// Just get the basic sizes for the first letter as it'll be the same for the rest.

		sprite *sp = &bmps[small_font_gfx].sprite_list[0];

		float u1 = sp->u1;
		float u2 = sp->u2;
		float v1 = sp->v1;
		float v2 = sp->v2;

		float left = -sp->pivot_x;
		float right = sp->width - sp->pivot_x;
		float up = sp->pivot_y;
		float down = - (sp->height - sp->pivot_y);
		float scaled_left = left * scale_multiplier;
		float scaled_right = right * scale_multiplier;
		float scaled_up = up * scale_multiplier;
		float scaled_down = down * scale_multiplier;
		float base_x = (float) x;
		float base_y = (float) (virtual_screen_height - y);
		int legacy_text_r = (r * 255) / 256;
		int legacy_text_g = (g * 255) / 256;
		int legacy_text_b = (b * 255) / 256;

		for(c=0; c<length; c++)
		{
			sprite *sp = &bmps[small_font_gfx].sprite_list[text[c]-32];

			u1 = sp->u1;
			u2 = sp->u2;
			v1 = sp->v1;
			v2 = sp->v2;

			PLATFORM_RENDERER_draw_textured_quad(
				bmps[small_font_gfx].texture_handle,
				legacy_text_r, legacy_text_g, legacy_text_b,
				base_x + (pen_x * scale_multiplier), base_y, virtual_screen_height,
				scaled_left, scaled_right, scaled_up, scaled_down,
				u1, v1, u2, v2,
				true);

			pen_x += right;

		}
	}
}



void OUTPUT_truncated_text (int max_chars ,int x, int y, char *text, int r, int g, int b)
{
	char temp[TEXT_LINE_SIZE];
	strcpy(temp,text);
	temp[max_chars]='\0';
	OUTPUT_text (x, y, temp, r, g, b);
}



void OUTPUT_centred_text (int x, int y, char *text, int r, int g, int b)
{
	int offet_x;

	if (software_mode_active)
	{
		offet_x = strlen(text)*(SOFTWARE_FONT_WIDTH/2);
	}
	else
	{
		offet_x = strlen(text)*(FONT_WIDTH/2);
	}

	OUTPUT_text (x-offet_x, y, text, r, g, b);
}



void OUTPUT_boxed_centred_text (int x, int y, char *text, int r, int g, int b)
{
	int offet_x;

	if (software_mode_active)
	{
		offet_x = strlen(text)*(SOFTWARE_FONT_WIDTH/2);
	}
	else
	{
		offet_x = strlen(text)*(FONT_WIDTH/2);
	}

	OUTPUT_text (x-offet_x, y, text, r, g, b);

	OUTPUT_rectangle (x - strlen(text)*(FONT_WIDTH/2) - 2 , y - 2, x + strlen(text)*(FONT_WIDTH/2) + 1, y + FONT_HEIGHT + 1, r/2, g/2, b/2);
}



void OUTPUT_right_aligned_text (int x, int y, char *text, int r, int g, int b)
{
	int offet_x;

	if (software_mode_active)
	{
		offet_x = strlen(text)*(SOFTWARE_FONT_WIDTH);
	}
	else
	{
		offet_x = strlen(text)*(FONT_WIDTH);
	}

	OUTPUT_text (x-offet_x, y, text, r, g, b);
}



void OUTPUT_line (int x1 , int y1, int x2, int y2, int r, int g, int b)
{
	if (software_mode_active)
	{
		line (active_page , x1 , y1 , x2 , y2 , makecol(r,g,b) );
	}
	else
	{
		PLATFORM_RENDERER_draw_line(x1, y1, x2, y2, r, g, b, virtual_screen_height);
	}
}



void OUTPUT_set_arbitrary_quad_from_base (int entity_index)
{
	int *entity_pointer = &entity[entity_index][0];
	
	if ((entity_pointer[ENT_SPRITE] > UNSET) && (entity_pointer[ENT_CURRENT_FRAME] > UNSET))
	{
		sprite *sp = &bmps[entity_pointer[ENT_SPRITE]].sprite_list[entity_pointer[ENT_CURRENT_FRAME]];
		
		arbitrary_quads_struct *aq = &arbitrary_quads[entity_index];

		aq->x[0] = -sp->pivot_x;
		aq->x[2] = -sp->pivot_x;

		aq->x[1] = sp->width - sp->pivot_x;
		aq->x[3] = sp->width - sp->pivot_x;

		aq->y[0] = sp->pivot_y;
		aq->y[1] = sp->pivot_y;

		aq->y[2] = - (sp->height - sp->pivot_y);
		aq->y[3] = - (sp->height - sp->pivot_y);
	}
}



void OUTPUT_hline (int x1 , int y1, int x2, int r, int g, int b)
{
	OUTPUT_line ( x1 , y1 , x2 , y1 , r,g,b );
}



void OUTPUT_vline (int x1 , int y1, int y2, int r, int g, int b)
{
	OUTPUT_line ( x1 , y1 , x1 , y2 , r,g,b );
}



void OUTPUT_circle (int x , int y , int radius , int r, int g, int b)
{
	if (software_mode_active)
	{
		circle (active_page , x , y , radius , makecol(r,g,b) );
/*
		float angle;
		float step = (2.0f * PI) / OPENGL_CIRCLE_RESOLUTION;

		float cx1,cy1;
		float cx2,cy2;

		for (angle = 0; angle < (2.0f * PI) ; angle += step)
		{
			cx1 = float(radius) * float (cos(angle) - sin(angle));
			cy1 = float(radius) * float (cos(angle) + sin(angle));

			cx2 = float(radius) * float (cos(angle+step) - sin(angle+step));
			cy2 = float(radius) * float (cos(angle+step) + sin(angle+step));

			OUTPUT_line ( x+cx1 , y+cy1 , x+cx2 , y+cy2 , r,g,b );
		}
*/
	}
	else
	{
		PLATFORM_RENDERER_draw_circle(x, y, radius, r, g, b, virtual_screen_height, OPENGL_CIRCLE_RESOLUTION);
	}
}



void OUTPUT_centred_square (int x , int y , int widthandheight , int r, int g, int b)
{
	OUTPUT_rectangle (x-widthandheight, y-widthandheight, x+widthandheight, y+widthandheight, r, g, b);
}



void OUTPUT_clear_screen (void)
{
	if (software_mode_active)
	{
		clear_bitmap (active_page);
	}
	else
	{
		PLATFORM_RENDERER_clear_backbuffer();
	}
}



#define TRIPLEBUFFER		(0)
#define PAGEFLIP			(1)
#define SYSTEMBUFFER		(2)
#define DOUBLEBUFFER		(3)



void OUTPUT_updatescreen (void)
{
	if (software_mode_active)
	{
		switch(update_method)
		{
		case TRIPLEBUFFER:
			do
			{
			}
			while (poll_scroll());
			
			current_page = active_page;

			request_video_bitmap (current_page);

			if (active_page == pages[0])
			{
				active_page = pages[1];
			}
			else if (active_page == pages[1])
			{
				active_page = pages[2];
			}
			else
			{
				active_page = pages[0];
			}
			break;

		case PAGEFLIP:
			current_page = active_page;
			show_video_bitmap (current_page);
			
			if (active_page == pages[0])
			{
				active_page = pages[1];
			}
			else
			{
				active_page = pages[0];
			}
			break;

		case SYSTEMBUFFER:
		case DOUBLEBUFFER:
			if (wait_for_vsync)
			{
				vsync();
			}
			blit (active_page, current_page, 0, 0, 0, 0, SCREEN_W, SCREEN_H);
			break;
		}
	}
	else
	{
		int present_width = SCREEN_W;
		int present_height = SCREEN_H;
		if (!PLATFORM_RENDERER_is_gl_enabled())
		{
			present_width = game_screen_width;
			present_height = game_screen_height;
		}
		PLATFORM_RENDERER_present_frame(present_width, present_height);
	}
}



BITMAP * OUTPUT_get_buffer (void)
{
	return active_page;
}



BITMAP * OUTPUT_get_screen (void)
{
	return current_page;
}



void OUTPUT_enable_vsync (void)
{
	wait_for_vsync = true;
}



void OUTPUT_disable_vsync (void)
{
	wait_for_vsync = false;
}



bool OUTPUT_is_vsync_enabled (void)
{
	return wait_for_vsync;
}



int OUTPUT_get_screen_update_method (void)
{
	return update_method;
}



void OUTPUT_enable_multi_texture_effects (void)
{
	if (best_texture_combiner_available)
	{
		texture_combiner_available = true;
	}
}



void OUTPUT_disable_multi_texture_effects (void)
{
	texture_combiner_available = false;
}



bool OUTPUT_select_buffering_method (int buffering_method)
{
	int t;
	
	switch(buffering_method)
	{
	case TRIPLEBUFFER:
		if (!(gfx_capabilities & GFX_CAN_TRIPLE_BUFFER))
		{
			enable_triple_buffer();
		}

		if ((gfx_capabilities & GFX_CAN_TRIPLE_BUFFER))
		{
			pages[0] = create_video_bitmap(SCREEN_W, SCREEN_H);
			pages[1] = create_video_bitmap(SCREEN_W, SCREEN_H);
			pages[2] = create_video_bitmap(SCREEN_W, SCREEN_H);

			if(pages[0] && pages[1] && pages[2])
			{
				clear_bitmap(pages[0]);
				clear_bitmap(pages[1]);
				clear_bitmap(pages[2]);
				active_page  = pages[0];
				current_page = pages[2];
				show_video_bitmap(current_page);
				return true;
			}
			else
			{
				for (t=0;t<3;t++)
				{
					if(pages[t])
					{
						destroy_bitmap (pages[t]);
						pages[t] = NULL;
					}
				}

				return false;
			}
		}
		break;

	case PAGEFLIP:
		pages[0] = create_video_bitmap(SCREEN_W, SCREEN_H);
		if (!pages[0])
		{
			return false;
		}
		
		pages[1] = create_video_bitmap(SCREEN_W, SCREEN_H);
		if (!pages[1])
		{
			destroy_bitmap(pages[0]);
			return false;
		}

		// Pageflip...

		if (pages[0] && pages[1])
		{
			clear_bitmap (pages[0]);
			clear_bitmap (pages[1]);
			active_page = pages[0];
			current_page = pages[1];
			show_video_bitmap (current_page);
			return true;
		}
		else
		{
			for (t=0;t<2;t++)
			{
				if(pages[t])
				{
					destroy_bitmap (pages[t]);
					pages[t] = NULL;
				}
			}

			return false;
		}
		break;

	case SYSTEMBUFFER:
		active_page  = create_system_bitmap(SCREEN_W, SCREEN_H);

		if (!active_page)
		{
			return false;
		}

		current_page = create_video_bitmap(SCREEN_W, SCREEN_H);

		if (!current_page)
		{
			destroy_bitmap (active_page);
			return false;
		}

		clear_bitmap(active_page);
		clear_bitmap(current_page);
		show_video_bitmap(current_page);
		return true;
		break;

	case DOUBLEBUFFER:
		active_page  = create_bitmap(SCREEN_W, SCREEN_H);

		if (!active_page)
		{
			return false;
		}

		current_page = create_video_bitmap(SCREEN_W, SCREEN_H);

		if (!current_page)
		{
			destroy_bitmap (active_page);
			return false;
		}

		clear_bitmap(active_page);
		clear_bitmap(current_page);
		show_video_bitmap(current_page);
		return true;
		break;

	default:
		OUTPUT_message("WTF?!");
		assert(0);
		break;
	}

	return false;
}



void OUTPUT_setup_project_list (char *text)
{
	PLATFORM_WINDOW_set_windowed_mode(320, 240, 32);

	PLATFORM_WINDOW_begin_text_screen(255, 255, 255);

	int y=0;

	char *pointer = strtok (text,"\n");

	while (pointer != NULL)
	{
		textout (screen , font, pointer, 0 , y , makecol(0,0,0) );
		y += 8;
		pointer = strtok (NULL,"\n");
	}

	PLATFORM_WINDOW_end_text_screen();
}



void OUTPUT_setup_running_mode (void)
{
	PLATFORM_WINDOW_begin_text_screen(255, 255, 255);

	char options[3][64]={
		"1. Parse, debug, output datfiles.",
		"2. Parse, output datfiles.",
		"3. Load from datfiles.",
	};

	int y=0;
	int counter;
	
	for (counter=0; counter<3; counter++)
	{
		textout (screen , font, &options[counter][0], 0 , y , makecol(0,0,0) );
		y += 8;
	}

	PLATFORM_WINDOW_end_text_screen();
}



void OUTPUT_setup_allegro (bool windowed, int colour_depth, int base_screen_width, int base_screen_height, int screen_multiplier)
{
	int result;
	int t;

	game_screen_width = base_screen_width * screen_multiplier;
	game_screen_height = base_screen_height * screen_multiplier;

	virtual_screen_width = base_screen_width;
	virtual_screen_height = base_screen_height;

	window_scale_multiplier = screen_multiplier;

	float_window_scale_multiplier = float (screen_multiplier);

#ifdef WIZBALL_USE_SDL2
	bool sdl_stub_enabled = PLATFORM_RENDERER_is_sdl2_stub_enabled();
	bool sdl_native_sprites_enabled = PLATFORM_RENDERER_is_sdl2_native_sprite_enabled();
	bool sdl_native_primary_enabled = PLATFORM_RENDERER_is_sdl2_native_primary_enabled();
	char sdl_stub_line[MAX_LINE_SIZE];
	sprintf(sdl_stub_line, "SDL2 renderer stub: enabled=%d native_sprites=%d native_primary=%d lazy_init=1 status=%s", sdl_stub_enabled ? 1 : 0, sdl_native_sprites_enabled ? 1 : 0, sdl_native_primary_enabled ? 1 : 0, PLATFORM_RENDERER_get_sdl2_stub_status());
	MAIN_add_to_log(sdl_stub_line);
#endif

	if (software_mode_active)
	{
		// This is for when we have no hardware accelleration. Which is never, nowadays.
		PLATFORM_WINDOW_set_software_game_mode(windowed, game_screen_width, game_screen_height, colour_depth);

		// Try to set buffering method by trying each one in turn until you score a hit!

		update_method = UNSET;

		for (t=TRIPLEBUFFER; t<=DOUBLEBUFFER; t++)
		{
			if (OUTPUT_select_buffering_method(t) == true)
			{
				update_method = t;
			}
		}

		if (update_method == UNSET)
		{
			OUTPUT_message("No available software buffering mode available!");
			assert(0);
		}

	}
	else
	{
		platform_renderer_caps caps;
		memset(&caps, 0, sizeof(caps));
		if (PLATFORM_RENDERER_is_gl_enabled())
		{
			// Configure AllegroGL renderer lifecycle in a platform seam.
			PLATFORM_RENDERER_prepare_allegro_gl(windowed, colour_depth);

			// And open our sexy OpenGL window! :)
			result = PLATFORM_WINDOW_set_opengl_game_mode(game_screen_width, game_screen_height, colour_depth);

			MAIN_add_to_log ("Setting up AllegroGL graphics & screen mode...");

			if (result == 0)
			{
				MAIN_add_to_log ("\tOK!");
			}
			else
			{
				MAIN_add_to_log ("\tFAILED!");
			}

			#ifdef ALLEGRO_MACOSX
				if (result<0)
				{
					allegro_message("Error setting OpenGL graphics mode:\n%s\nAllegro GL error : %s\n",allegro_error, PLATFORM_RENDERER_get_allegro_gl_error_text());
					exit(1);
				}
			#else
				if (result)
				{
					allegro_message("Error setting OpenGL graphics mode:\n%s\nAllegro GL error : %s\n",allegro_error, PLATFORM_RENDERER_get_allegro_gl_error_text());
				}
			#endif

				if (result == 0)
				{
					// Always initialize legacy GL defaults when GL is enabled, even in SDL-native test mode.
					PLATFORM_RENDERER_apply_gl_defaults(SCREEN_W, SCREEN_H, virtual_screen_width, virtual_screen_height);

		#ifdef WIZBALL_USE_SDL2
					if (PLATFORM_RENDERER_is_sdl2_native_sprite_enabled())
					{
						MAIN_add_to_log("SDL2 native mode active: OpenGL defaults applied; skipping GL caps/extensions query.");
					}
					else
		#endif
					{
						if (!PLATFORM_RENDERER_query_and_build_caps(enable_multi_texture_effects_ideally, &caps))
						{
							MAIN_add_to_log("OpenGL capability query failed; continuing with minimal renderer capabilities.");
							memset(&caps, 0, sizeof(caps));
						}

						/* Setup OpenGL like we want */

						if (output_debug_information)
						{
							const char *gl_vendor = PLATFORM_RENDERER_get_vendor_text();
							const char *gl_renderer = PLATFORM_RENDERER_get_renderer_text();
							const char *gl_version = PLATFORM_RENDERER_get_version_text();
							char gl_line[MAX_LINE_SIZE];

							sprintf(gl_line, "OpenGL vendor: %s", (gl_vendor != NULL) ? gl_vendor : "UNKNOWN");
							MAIN_add_to_log(gl_line);
							sprintf(gl_line, "OpenGL renderer: %s", (gl_renderer != NULL) ? gl_renderer : "UNKNOWN");
							MAIN_add_to_log(gl_line);
							sprintf(gl_line, "OpenGL version: %s", (gl_version != NULL) ? gl_version : "UNKNOWN");
							MAIN_add_to_log(gl_line);
						}

						// We output it, too. Uh-huh.
						if (output_debug_information)
						{
							if (!PLATFORM_RENDERER_write_extensions_to_file("OpenGL_Extension.txt"))
							{
								MAIN_add_to_log("Cannot output OpenGL extension information; continuing.");
							}
						}
					}
				}
				else
				{
					MAIN_add_to_log("OpenGL graphics mode failed; continuing without OpenGL capabilities.");
					memset(&caps, 0, sizeof(caps));
				}
			}
		#ifdef WIZBALL_USE_SDL2
		else
		{
			MAIN_add_to_log("SDL2 native mode active: OpenGL window disabled.");
			if (!PLATFORM_RENDERER_prepare_sdl2_stub(game_screen_width, game_screen_height, windowed))
			{
				char sdl_bootstrap_line[MAX_LINE_SIZE];
				sprintf(sdl_bootstrap_line, "SDL2 native mode bootstrap failed: %s", PLATFORM_RENDERER_get_sdl2_stub_status());
				MAIN_add_to_log(sdl_bootstrap_line);
			}
		}
		#endif

		secondary_colour_available = caps.secondary_colour_available;
		best_texture_combiner_available = caps.best_texture_combiner_available;
		texture_combiner_available = caps.texture_combiner_available;
		PLATFORM_RENDERER_apply_caps(&caps);
	}

	if (software_mode_active || PLATFORM_RENDERER_is_gl_enabled())
	{
		PLATFORM_WINDOW_center_game_window();
	}
}



void OUTPUT_draw_sprite (int bitmap_number , int sprite_number, int x, int y, int r, int g, int b)
{

	if (software_mode_active)
	{
		blit ( bmps[bitmap_number].image , active_page, bmps[bitmap_number].sprite_list[sprite_number].x , bmps[bitmap_number].sprite_list[sprite_number].y , x-bmps[bitmap_number].sprite_list[sprite_number].pivot_x , y-bmps[bitmap_number].sprite_list[sprite_number].pivot_y , bmps[bitmap_number].sprite_list[sprite_number].width , bmps[bitmap_number].sprite_list[sprite_number].height );
	}
	else
	{
		sprite *sp = &bmps[bitmap_number].sprite_list[sprite_number];

		float u1 = sp->u1;
		float u2 = sp->u2;
		float v1 = sp->v1;
		float v2 = sp->v2;

		float left = -sp->pivot_x;
		float right = sp->width - sp->pivot_x;
		float up = sp->pivot_y;
		float down = - (sp->height - sp->pivot_y);

		PLATFORM_RENDERER_draw_textured_quad(
			bmps[bitmap_number].texture_handle,
			r, g, b,
			x, virtual_screen_height - y, virtual_screen_height,
			left, right, up, down,
			u1, v1, u2, v2,
			false);
	}
}



void OUTPUT_draw_masked_sprite (int bitmap_number , int sprite_number, int x, int y, int r, int g, int b)
{
	if (software_mode_active)
	{
		masked_blit ( bmps[bitmap_number].image , active_page, bmps[bitmap_number].sprite_list[sprite_number].x , bmps[bitmap_number].sprite_list[sprite_number].y , x-bmps[bitmap_number].sprite_list[sprite_number].pivot_x , y-bmps[bitmap_number].sprite_list[sprite_number].pivot_y , bmps[bitmap_number].sprite_list[sprite_number].width , bmps[bitmap_number].sprite_list[sprite_number].height );
	}
	else
	{
		sprite *sp = &bmps[bitmap_number].sprite_list[sprite_number];

		float u1 = sp->u1;
		float u2 = sp->u2;
		float v1 = sp->v1;
		float v2 = sp->v2;

		float left = -sp->pivot_x;
		float right = sp->width - sp->pivot_x;
		float up = sp->pivot_y;
		float down = - (sp->height - sp->pivot_y);

		PLATFORM_RENDERER_draw_textured_quad(
			bmps[bitmap_number].texture_handle,
			r, g, b,
			x, virtual_screen_height - y, virtual_screen_height,
			left, right, up, down,
			u1, v1, u2, v2,
			true);
	}
}



void OUTPUT_get_sprite_uvs (int bitmap_number , int sprite_number, float &u1, float &v1, float &u2, float &v2)
{
	u1 = bmps[bitmap_number].sprite_list[sprite_number].u1;
	v1 = bmps[bitmap_number].sprite_list[sprite_number].v1;
	u2 = bmps[bitmap_number].sprite_list[sprite_number].u2;
	v2 = bmps[bitmap_number].sprite_list[sprite_number].v2;
}



int OUTPUT_get_sprite_width (int bitmap_number , int sprite_number)
{
	return bmps[bitmap_number].sprite_list[sprite_number].width;
}



int OUTPUT_get_sprite_height (int bitmap_number , int sprite_number)
{
	return bmps[bitmap_number].sprite_list[sprite_number].height;
}



int OUTPUT_get_sprite_pivot_x (int bitmap_number , int sprite_number)
{
	return bmps[bitmap_number].sprite_list[sprite_number].pivot_x;
}



int OUTPUT_get_sprite_pivot_y (int bitmap_number , int sprite_number)
{
	return bmps[bitmap_number].sprite_list[sprite_number].pivot_y;
}



void OUTPUT_draw_bitmap (int bitmap_number, int x, int y, int window)
{
	// This draws the whole image rather than the tiny slice which the sprite
	// might make up.

	if (software_mode_active)
	{
		blit ( bmps[bitmap_number].image , active_page, 0 , 0 , x , y , bmps[bitmap_number].width , bmps[bitmap_number].height );
	}
	else
	{
		float u1 = 0.0f;
		float u2 = 1.0f;
		float v1 = 0.0f;
		float v2 = 1.0f;

		float left = 0;
		float right = bmps[bitmap_number].width;
		float up = 0;
		float down = -bmps[bitmap_number].height;

		PLATFORM_RENDERER_draw_textured_quad(
			bmps[bitmap_number].texture_handle,
			255, 255, 255,
			x, virtual_screen_height - (y + up - down), virtual_screen_height,
			left, right, up, down,
			u1, v1, u2, v2,
			false);
	}
}



int OUTPUT_sprite_width (int bitmap_number , int sprite_number)
{
	return (bmps[bitmap_number].sprite_list[sprite_number].width);
}



int OUTPUT_sprite_height (int bitmap_number ,int sprite_number)
{
	return (bmps[bitmap_number].sprite_list[sprite_number].height);
}



int OUTPUT_bitmap_width (int bitmap_number)
{
	return (bmps[bitmap_number].width);	
}



int OUTPUT_bitmap_height (int bitmap_number)
{
	return (bmps[bitmap_number].height);
}



int OUTPUT_bitmap_frames (int bitmap_number)
{
	return (bmps[bitmap_number].sprite_count);
}

static bool OUTPUT_is_valid_bitmap_index(int bitmap_number)
{
	return (bitmap_number >= 0 && bitmap_number < total_bitmaps_loaded);
}

static bool OUTPUT_is_valid_sprite_frame(int bitmap_number, int sprite_number)
{
	if (!OUTPUT_is_valid_bitmap_index(bitmap_number))
	{
		return false;
	}

	if (bmps[bitmap_number].sprite_list == NULL)
	{
		return false;
	}

	return (sprite_number >= 0 && sprite_number < bmps[bitmap_number].sprite_count);
}

static bool OUTPUT_has_multitexture_flags(int opengl_booleans)
{
	return (opengl_booleans & (OPENGL_BOOLEAN_MULTITEXTURE_MASK | OPENGL_BOOLEAN_MULTITEXTURE_DOUBLE_MASK)) != 0;
}

static bool OUTPUT_is_double_multitexture_mode(int opengl_booleans)
{
	return (opengl_booleans & OPENGL_BOOLEAN_MULTITEXTURE_DOUBLE_MASK) != 0;
}

static bool OUTPUT_is_secondary_multitexture_active(bool texture_combiner_available, int opengl_booleans, int secondary_bitmap_number)
{
	return texture_combiner_available &&
		(secondary_bitmap_number != UNSET) &&
		OUTPUT_is_valid_bitmap_index(secondary_bitmap_number) &&
		OUTPUT_has_multitexture_flags(opengl_booleans);
}

static bool OUTPUT_draw_mode_requires_bitmap(int draw_mode)
{
	switch (draw_mode)
	{
		case DRAW_MODE_SPRITE:
		case DRAW_MODE_TILEMAP:
		case DRAW_MODE_TILEMAP_LINE:
		case DRAW_MODE_TEXT:
		case DRAW_MODE_HISCORE_ENTRY_NAME:
		case DRAW_MODE_HISCORE_ENTRY_SCORE:
			return true;
		default:
			return false;
	}
}

static bool OUTPUT_window_queue_step(int window_number, int y_list, int *current_entity, int *step_counter)
{
	int next_entity;
	static unsigned int cycle_log_counter = 0;

	if ((current_entity == NULL) || (step_counter == NULL))
	{
		return false;
	}

	if ((*current_entity < 0) || (*current_entity >= MAX_ENTITIES))
	{
		return false;
	}

	next_entity = entity[*current_entity][ENT_NEXT_WINDOW_ENT];
	(*step_counter)++;
	if ((*step_counter) > MAX_ENTITIES)
	{
		if (cycle_log_counter < 64)
		{
			cycle_log_counter++;
			fprintf(stderr, "[WINDOW-QUEUE] y-list cycle detected: window=%d y_list=%d start=%d\n", window_number, y_list, *current_entity);
			fflush(stderr);
		}
		return false;
	}

	if (next_entity == *current_entity)
	{
		if (cycle_log_counter < 64)
		{
			cycle_log_counter++;
			fprintf(stderr, "[WINDOW-QUEUE] self-loop detected: window=%d y_list=%d start=%d\n", window_number, y_list, *current_entity);
			fflush(stderr);
		}
		return false;
	}

	*current_entity = next_entity;
	return true;
}

static void OUTPUT_fatal_exit(char *message)
{
	OUTPUT_message(message);
	MAIN_add_to_log(message);
	exit(1);
}

static const char *OUTPUT_strip_project_prefix(const char *path)
{
	const char *project_name;
	size_t project_len;

	if (path == NULL)
	{
		return path;
	}

	project_name = MAIN_get_project_name();
	if (project_name == NULL)
	{
		return path;
	}

	project_len = strlen(project_name);
	if (project_len == 0)
	{
		return path;
	}

	if (strncasecmp(path, project_name, project_len) != 0)
	{
		return path;
	}

	if ((path[project_len] == '/') || (path[project_len] == '\\'))
	{
		return path + project_len + 1;
	}

	return path;
}



#define CHUNK_SIZE		(128)



void INPUT_create_sprite_space (int parent_bitmap, int total_sprites)
{
	char error[MAX_LINE_SIZE];

	if (!OUTPUT_is_valid_bitmap_index(parent_bitmap))
	{
		sprintf(error, "Invalid parent bitmap index %i while creating sprite space.", parent_bitmap);
		OUTPUT_fatal_exit(error);
	}

	if ((total_sprites <= 0) || (total_sprites > 50000))
	{
		sprintf(error, "Invalid sprite count %i for bitmap index %i.", total_sprites, parent_bitmap);
		OUTPUT_fatal_exit(error);
	}

	bmps[parent_bitmap].sprite_count = total_sprites;
	bmps[parent_bitmap].sprite_list = (sprite *) malloc (total_sprites * sizeof(sprite));

	if (bmps[parent_bitmap].sprite_list == NULL)
	{
		sprintf(error, "Out of memory while allocating %i sprites for bitmap %i.", total_sprites, parent_bitmap);
		OUTPUT_fatal_exit(error);
	}
}



void INPUT_create_equal_sized_sprite_series (int parent_bitmap, char *name_base, int sprite_width, int sprite_height, int pivot_x, int pivot_y)
{
	// This uses the dimensions of the parent bitmap to make a series of sprites.
	// If it does not divide exactly then some fragment sprites will be generated.

	char full_name[NAME_SIZE];
	int counter;
	int x,y;
	int total_sprites;

	total_sprites = (bmps[parent_bitmap].height / sprite_height) * (bmps[parent_bitmap].width / sprite_width);
	INPUT_create_sprite_space (parent_bitmap , total_sprites);

	counter = 0;

	for (y=0; y<bmps[parent_bitmap].height-(sprite_height-1) ; y+=sprite_height)
	{
		for (x=0; x<(bmps[parent_bitmap].width-(sprite_width-1)) ; x+=sprite_width)
		{
			sprintf(full_name,"%s%i",name_base,counter);
			INPUT_create_sprite (parent_bitmap, counter, full_name, x, y, sprite_width, sprite_height, pivot_x, pivot_y);
			counter++;
		}
	}
}



int INPUT_get_nearest_value (int value, int compare_1, int compare_2, int compare_3)
{
	int dist_1,dist_2,dist_3;

	dist_1 = MATH_abs_int (compare_1 - value);
	dist_2 = MATH_abs_int (compare_2 - value);
	dist_3 = MATH_abs_int (compare_3 - value);

	if (dist_1 < dist_2)
	{
		if (dist_1 < dist_3)
		{
			return compare_1;
		}
		else
		{
			return compare_3;
		}
	}
	else
	{
		if (dist_2 < dist_3)
		{
			return compare_2;
		}
		else
		{
			return compare_3;
		}
	}


}



#define ARB_DATA_UNIT_SIZE	(6)

void INPUT_create_arb_list (int parent_bitmap, char *descriptor_file)
{
	int *data = NULL;
	int *data_pointer = NULL;
	int lines = 0;
	int t;

	int mx;
	int my;

	int grabbing_stage = 0;

	int start_x,start_y,end_x,end_y;
	int rounded_mx,rounded_my;
	int pivot_x,pivot_y;

	while (CONTROL_key_hit(KEY_ESC) == false)
	{
		OUTPUT_clear_screen();
		CONTROL_update_all_input ();

		mx = CONTROL_mouse_x();
		my = CONTROL_mouse_y();

		rounded_mx = mx - (mx % 8);
		rounded_my = my - (my % 8);

		if (CONTROL_mouse_hit(LMB) == true)
		{
			if (grabbing_stage == 0)
			{
				// Highlighting the bounding box...
				grabbing_stage = 1;
				start_x = rounded_mx;
				start_y = rounded_my;
			}
			else if (grabbing_stage == 2)
			{
				// Choosing offset...
				grabbing_stage = 0;

				if (data == NULL)
				{
					data = (int *) malloc (sizeof(int) * ARB_DATA_UNIT_SIZE);
				}
				else
				{
					data = (int *) realloc (data , sizeof(int) * ARB_DATA_UNIT_SIZE * (lines+1) );
				}

				data_pointer = &data[ARB_DATA_UNIT_SIZE * lines];

				data_pointer[0] = start_x;
				data_pointer[1] = start_y;
				data_pointer[2] = end_x - start_x;
				data_pointer[3] = end_y - start_y;
				data_pointer[4] = pivot_x - start_x;
				data_pointer[5] = pivot_y - start_y;

				lines++;
			}
		}

		if ( (CONTROL_mouse_hit(RMB) == true) && (grabbing_stage == 0) && (lines > 0) )
		{
			if (lines > 1)
			{
				lines--;
				data = (int *) realloc (data , sizeof(int) * ARB_DATA_UNIT_SIZE * lines );
			}
			else
			{
				free (data);
				data = NULL;
				lines--;
			}
		}

		OUTPUT_draw_bitmap(parent_bitmap,0,0);

		// Draw out pre-chosen boxes...

		for (t=0; t<lines; t++)
		{
			data_pointer = &data[ARB_DATA_UNIT_SIZE * t];
			OUTPUT_filled_rectangle_by_size (data_pointer[0],data_pointer[1],data_pointer[2],data_pointer[3],0,0,0);
		}

		if (grabbing_stage == 1)
		{
			OUTPUT_rectangle(start_x,start_y,rounded_mx,rounded_my);

			if (CONTROL_mouse_down(LMB) == false)
			{
				grabbing_stage = 2;
				end_x = rounded_mx;
				end_y = rounded_my;
			}
		}
		else if (grabbing_stage == 2)
		{
			OUTPUT_rectangle(start_x,start_y,end_x,end_y,128,128,128);

			pivot_x = INPUT_get_nearest_value (rounded_mx, start_x, (start_x+end_x)/2, end_x-1);
			pivot_y = INPUT_get_nearest_value (rounded_my, start_y, (start_y+end_y)/2, end_y-1);

			OUTPUT_vline(pivot_x,start_y,end_y);
			OUTPUT_hline(start_x,pivot_y,end_x);
		}

		// Draw mouse_pointer...

		OUTPUT_hline (rounded_mx,rounded_my,rounded_mx+16);
		OUTPUT_vline (rounded_mx,rounded_my,rounded_my+16);
		OUTPUT_line (rounded_mx,rounded_my, rounded_mx+32, rounded_my+32);

		if (grabbing_stage == 0)
		{
		}
		else if (grabbing_stage == 1)
		{
		}
		else if (grabbing_stage == 2)
		{
		}
		
		OUTPUT_updatescreen();
	}

	FILE *file_pointer = fopen (descriptor_file,"w");
	char line[MAX_LINE_SIZE];
	
	if (file_pointer != NULL)
	{
		sprintf(line,"%i\n",lines);

		fputs(line,file_pointer);

		for (t=0; t<lines; t++)
		{
			data_pointer = &data[ARB_DATA_UNIT_SIZE * t];
			sprintf(line,"%i,%i,%i,%i,%i,%i \t// %3i = \n",data_pointer[0],data_pointer[1],data_pointer[2],data_pointer[3],data_pointer[4],data_pointer[5],t);

			fputs(line,file_pointer);
		}

		fclose (file_pointer);
	}
	else
	{
		OUTPUT_message ("Cannot save descriptor file!!!");
		assert(0);
	}

	if (data != NULL)
	{
		free (data);
	}

	CONTROL_update_all_input ();

	MAIN_reset_game_trigger ();
}



void INPUT_create_arbitrary_sized_sprite_series (int parent_bitmap, char *name_base, char *descriptor_file)
{
	// This opens the descriptor file and reads the sprite sizes from it.
	// The descriptor itself is generated by another function.

	char full_pack_filename[MAX_LINE_SIZE];
	char full_name[NAME_SIZE];
	int counter;
	int t;
	int x,y,width,height,pivot_x,pivot_y;
	char line[MAX_LINE_SIZE];
	char *pointer;
	const char *normalised_descriptor_file = OUTPUT_strip_project_prefix(descriptor_file);

	if (load_from_dat_file)
	{
		sprintf(full_pack_filename,"%s\\gfx.dat#%s",MAIN_get_pack_project_name(),normalised_descriptor_file);
		fix_filename_slashes(full_pack_filename);
		PACKFILE *packfile_pointer = pack_fopen (full_pack_filename,"r");

		if (packfile_pointer != NULL)
		{
			pack_fgets ( line , MAX_LINE_SIZE , packfile_pointer );
			strtok (line,"\t\n ");
			
			counter = atoi(line);

			INPUT_create_sprite_space (parent_bitmap , counter);

				for (t=0 ; t<counter ; t++)
				{
					sprintf(full_name,"%s%i",name_base,t);

				pack_fgets ( line , MAX_LINE_SIZE , packfile_pointer );

					pointer = strtok(line,", ");
					if (pointer == NULL) { OUTPUT_fatal_exit((char*)"Malformed sprite descriptor line (x missing)."); }
					x = atoi(pointer);
					pointer = strtok(NULL,", ");
					if (pointer == NULL) { OUTPUT_fatal_exit((char*)"Malformed sprite descriptor line (y missing)."); }
					y = atoi(pointer);
					pointer = strtok(NULL,", ");
					if (pointer == NULL) { OUTPUT_fatal_exit((char*)"Malformed sprite descriptor line (width missing)."); }
					width = atoi(pointer);
					pointer = strtok(NULL,", ");
					if (pointer == NULL) { OUTPUT_fatal_exit((char*)"Malformed sprite descriptor line (height missing)."); }
					height = atoi(pointer);
					pointer = strtok(NULL,", ");
					if (pointer == NULL) { OUTPUT_fatal_exit((char*)"Malformed sprite descriptor line (pivot_x missing)."); }
					pivot_x = atoi(pointer);
					pointer = strtok(NULL,", ");
					if (pointer == NULL) { OUTPUT_fatal_exit((char*)"Malformed sprite descriptor line (pivot_y missing)."); }
					pivot_y = atoi(pointer);

				INPUT_create_sprite (parent_bitmap, t, full_name, x, y, width, height, pivot_x, pivot_y);
			}

			pack_fclose(packfile_pointer);
		}
				else
				{
					char error[MAX_LINE_SIZE];
					sprintf(error, "Sprite descriptor not found in gfx.dat: '%s' (normalised '%s').",
					        descriptor_file, normalised_descriptor_file);
					OUTPUT_fatal_exit(error);
				}
	}
	else
	{
		FILE *file_pointer = FILE_open_read_case_fallback(descriptor_file);
		if (file_pointer == NULL)
		{
			file_pointer = FILE_open_project_read_case_fallback((char *)normalised_descriptor_file);
		}

		if (file_pointer != NULL)
		{
			counter = FILE_get_int_from_file (file_pointer);

			INPUT_create_sprite_space (parent_bitmap , counter);

				for (t=0 ; t<counter ; t++)
				{
					sprintf(full_name,"%s%i",name_base,t);

				fgets ( line , MAX_LINE_SIZE , file_pointer );

					pointer = strtok(line,", ");
					if (pointer == NULL) { OUTPUT_fatal_exit((char*)"Malformed sprite descriptor line (x missing)."); }
					x = atoi(pointer);
					pointer = strtok(NULL,", ");
					if (pointer == NULL) { OUTPUT_fatal_exit((char*)"Malformed sprite descriptor line (y missing)."); }
					y = atoi(pointer);
					pointer = strtok(NULL,", ");
					if (pointer == NULL) { OUTPUT_fatal_exit((char*)"Malformed sprite descriptor line (width missing)."); }
					width = atoi(pointer);
					pointer = strtok(NULL,", ");
					if (pointer == NULL) { OUTPUT_fatal_exit((char*)"Malformed sprite descriptor line (height missing)."); }
					height = atoi(pointer);
					pointer = strtok(NULL,", ");
					if (pointer == NULL) { OUTPUT_fatal_exit((char*)"Malformed sprite descriptor line (pivot_x missing)."); }
					pivot_x = atoi(pointer);
					pointer = strtok(NULL,", ");
					if (pointer == NULL) { OUTPUT_fatal_exit((char*)"Malformed sprite descriptor line (pivot_y missing)."); }
					pivot_y = atoi(pointer);

				INPUT_create_sprite (parent_bitmap, t, full_name, x, y, width, height, pivot_x, pivot_y);
			}

			fclose(file_pointer);
		}
			else
			{
				char error[MAX_LINE_SIZE];
				sprintf(error, "Sprite descriptor not found on disk: '%s' (normalised '%s').",
				        descriptor_file, normalised_descriptor_file);
				OUTPUT_fatal_exit(error);
			}
	}
}



int INPUT_load_bitmap (char *filename)
{
	#ifdef RETRENGINE_DEBUG_VERSION_THE_LAST_THING_I_DID
		char debug_text[MAX_LINE_SIZE];
		sprintf(debug_text,"Entered INPUT_load_bitmap function...");
		MAIN_debug_last_thing (debug_text);
	#endif

	RGB *pal = NULL;
	char error[MAX_LINE_SIZE];

	// First of all create space for the bitmap by mallocing or reallocing
	// the space needed.

	if (total_bitmaps_loaded == total_bitmaps_allocated)
	{
		if (bmps == NULL)
		{
			bmps = (bitmap_holder *) malloc (sizeof(bitmap_holder) * CHUNK_SIZE);
			total_bitmaps_allocated = CHUNK_SIZE;
		}
		else
		{
			bmps = (bitmap_holder *) realloc (bmps , sizeof(bitmap_holder) * (CHUNK_SIZE + total_bitmaps_allocated) );
			total_bitmaps_allocated += CHUNK_SIZE;
		}

		if (bmps == NULL)
		{
			sprintf(error, "Out of memory while allocating bitmap storage for '%s'.", filename);
			OUTPUT_message(error);
			MAIN_add_to_log(error);
			exit(1);
		}
	}

	if (load_from_dat_file)
	{
		DATAFILE *data_pointer = find_datafile_object(gfx_dat, filename);

		#ifdef RETRENGINE_DEBUG_VERSION_THE_LAST_THING_I_DID
			sprintf(debug_text,"Found data pointer %p",data_pointer);
			MAIN_debug_last_thing (debug_text);
		#endif

			if ((data_pointer == NULL) || (data_pointer->dat == NULL))
			{
				sprintf(error, "Could not load bitmap '%s' from gfx.dat.", filename);
				OUTPUT_message(error);
				MAIN_add_to_log(error);
				exit(1);
			}

		bmps[total_bitmaps_loaded].image = (BITMAP *) data_pointer->dat;
	}
	else
	{
		bmps[total_bitmaps_loaded].image = load_bitmap( filename , pal);

#if defined(ALLEGRO_LINUX) || defined(ALLEGRO_MACOSX)
		if (bmps[total_bitmaps_loaded].image == NULL)
		{
			char lower_filename[MAX_LINE_SIZE];
			size_t len;
			size_t i;

			strncpy(lower_filename, filename, sizeof(lower_filename) - 1);
			lower_filename[sizeof(lower_filename) - 1] = '\0';
			len = strlen(lower_filename);

			// First fallback: lowercase basename.
			for (i = len; i > 0; i--)
			{
				if ((lower_filename[i - 1] == '/') || (lower_filename[i - 1] == '\\'))
				{
					break;
				}
			}
			for (; i < len; i++)
			{
				lower_filename[i] = (char)tolower((unsigned char)lower_filename[i]);
			}

			bmps[total_bitmaps_loaded].image = load_bitmap(lower_filename, pal);

			// Second fallback: lowercase final directory + basename tail.
			if (bmps[total_bitmaps_loaded].image == NULL)
			{
				strncpy(lower_filename, filename, sizeof(lower_filename) - 1);
				lower_filename[sizeof(lower_filename) - 1] = '\0';
				len = strlen(lower_filename);

				int slash_count = 0;
				size_t start = 0;
				for (i = len; i > 0; i--)
				{
					if ((lower_filename[i - 1] == '/') || (lower_filename[i - 1] == '\\'))
					{
						slash_count++;
						if (slash_count == 2)
						{
							start = i;
							break;
						}
					}
				}
				if (slash_count < 2)
				{
					start = 0;
				}
				for (i = start; i < len; i++)
				{
					lower_filename[i] = (char)tolower((unsigned char)lower_filename[i]);
				}

				bmps[total_bitmaps_loaded].image = load_bitmap(lower_filename, pal);
			}
		}
#endif

			if (bmps[total_bitmaps_loaded].image == NULL)
			{
				sprintf(error, "Could not load bitmap '%s'.", filename);
				OUTPUT_message(error);
				MAIN_add_to_log(error);
				exit(1);
			}
		}

	if (bmps[total_bitmaps_loaded].image == NULL)
	{
		sprintf(error, "Bitmap '%s' resolved to NULL image pointer.", filename);
		OUTPUT_message(error);
		MAIN_add_to_log(error);
		exit(1);
	}

	bmps[total_bitmaps_loaded].width = bmps[total_bitmaps_loaded].image->w;
	bmps[total_bitmaps_loaded].height = bmps[total_bitmaps_loaded].image->h;

	if (software_mode_active == false)
	{
		// After we've turned it into a texture then we can get rid of the software
		// bitmap to free up memory. Woot!

		#ifdef RETRENGINE_DEBUG_VERSION_THE_LAST_THING_I_DID
			sprintf(debug_text,"About to make masked texture of %i,%i.",bmps[total_bitmaps_loaded].width,bmps[total_bitmaps_loaded].height);
			MAIN_debug_last_thing (debug_text);
		#endif

		bmps[total_bitmaps_loaded].texture_handle = PLATFORM_RENDERER_create_masked_texture(bmps[total_bitmaps_loaded].image);

		#ifdef RETRENGINE_DEBUG_VERSION_THE_LAST_THING_I_DID
			sprintf(debug_text,"Made masked texture.");
			MAIN_debug_last_thing (debug_text);
		#endif

		if (load_from_dat_file == false)
		{
			destroy_bitmap (bmps[total_bitmaps_loaded].image);
			bmps[total_bitmaps_loaded].image = NULL;
		}
	}

	bmps[total_bitmaps_loaded].sprite_list = NULL;

	total_bitmaps_loaded++;

	return (total_bitmaps_loaded-1);
}



void INPUT_create_sprite (int parent_bitmap, int sprite_number, char *name, int x, int y, int width, int height, int pivot_x, int pivot_y)
{
	// First of all create space for the sprite by mallocing or reallocing
	// the space needed.

	float unit_x,unit_y;
	char error[MAX_LINE_SIZE];

	if (!OUTPUT_is_valid_bitmap_index(parent_bitmap))
	{
		sprintf(error, "Invalid parent bitmap index %i while creating sprite.", parent_bitmap);
		OUTPUT_fatal_exit(error);
	}

	if (bmps[parent_bitmap].sprite_list == NULL)
	{
		sprintf(error, "Sprite list is NULL for bitmap index %i.", parent_bitmap);
		OUTPUT_fatal_exit(error);
	}

	if ((sprite_number < 0) || (sprite_number >= bmps[parent_bitmap].sprite_count))
	{
		sprintf(error, "Sprite frame index %i is out of range [0,%i) for bitmap index %i.",
		        sprite_number, bmps[parent_bitmap].sprite_count, parent_bitmap);
		OUTPUT_fatal_exit(error);
	}

	if ((bmps[parent_bitmap].width <= 0) || (bmps[parent_bitmap].height <= 0))
	{
		sprintf(error, "Invalid bitmap dimensions %ix%i for bitmap index %i while creating sprite.",
		        bmps[parent_bitmap].width, bmps[parent_bitmap].height, parent_bitmap);
		OUTPUT_fatal_exit(error);
	}

	strcpy (bmps[parent_bitmap].sprite_list[sprite_number].name , name);
	bmps[parent_bitmap].sprite_list[sprite_number].parent_bitmap = parent_bitmap;
	
	bmps[parent_bitmap].sprite_list[sprite_number].x = x;
	bmps[parent_bitmap].sprite_list[sprite_number].y = y;
	bmps[parent_bitmap].sprite_list[sprite_number].width = width;
	bmps[parent_bitmap].sprite_list[sprite_number].height = height;
	bmps[parent_bitmap].sprite_list[sprite_number].pivot_x = pivot_x;
	bmps[parent_bitmap].sprite_list[sprite_number].pivot_y = pivot_y;

	unit_x = 1.0f / float (bmps[parent_bitmap].width);
	unit_y = 1.0f / float (bmps[parent_bitmap].height);

	bmps[parent_bitmap].sprite_list[sprite_number].u1 = float (x) * unit_x;
	bmps[parent_bitmap].sprite_list[sprite_number].u2 = float (x+width) * unit_x;
	bmps[parent_bitmap].sprite_list[sprite_number].v1 = 1.0f - (float (y) * unit_y);
	bmps[parent_bitmap].sprite_list[sprite_number].v2 = 1.0f - (float (y+height) * unit_y);
}



void OUTPUT_draw_sprite_scale (int bitmap_number, int sprite_number , int x , int y , float scale , int r, int g, int b)
{
	if (software_mode_active)
	{
		// Use masked_stretch_blit again for speed...

		int new_pivot_x = bmps[bitmap_number].sprite_list[sprite_number].pivot_x * scale;
		int new_pivot_y = bmps[bitmap_number].sprite_list[sprite_number].pivot_y * scale;

		masked_stretch_blit ( bmps[bitmap_number].image , active_page, bmps[bitmap_number].sprite_list[sprite_number].x , bmps[bitmap_number].sprite_list[sprite_number].y , bmps[bitmap_number].sprite_list[sprite_number].width , bmps[bitmap_number].sprite_list[sprite_number].height , x-new_pivot_x , y-new_pivot_y , bmps[bitmap_number].sprite_list[sprite_number].width * scale , bmps[bitmap_number].sprite_list[sprite_number].height * scale );
	}
	else
	{
		sprite *sp = &bmps[bitmap_number].sprite_list[sprite_number];

		float u1 = sp->u1;
		float u2 = sp->u2;
		float v1 = sp->v1;
		float v2 = sp->v2;

		float left = -sp->pivot_x;
		float right = sp->width - sp->pivot_x;
		float up = sp->pivot_y;
		float down = - (sp->height - sp->pivot_y);

		left *= scale;
		right *= scale;
		up *= scale;
		down *= scale;

		PLATFORM_RENDERER_draw_textured_quad(
			bmps[bitmap_number].texture_handle,
			r, g, b,
			x, virtual_screen_height - y, virtual_screen_height,
			left, right, up, down,
			u1, v1, u2, v2,
			true);
	}
}



void OUTPUT_draw_sprite_scale_no_pivot (int bitmap_number, int sprite_number , int x , int y , float scale , int r, int g, int b)
{
	if (software_mode_active)
	{
		// Use masked_stretch_blit again for speed...

		masked_stretch_blit ( bmps[bitmap_number].image , active_page, bmps[bitmap_number].sprite_list[sprite_number].x , bmps[bitmap_number].sprite_list[sprite_number].y , bmps[bitmap_number].sprite_list[sprite_number].width , bmps[bitmap_number].sprite_list[sprite_number].height , x , y , bmps[bitmap_number].sprite_list[sprite_number].width * scale , bmps[bitmap_number].sprite_list[sprite_number].height * scale );
	}
	else
	{
		sprite *sp = &bmps[bitmap_number].sprite_list[sprite_number];

		float u1 = sp->u1;
		float u2 = sp->u2;
		float v1 = sp->v1;
		float v2 = sp->v2;

		float left = -sp->pivot_x;
		float right = sp->width - sp->pivot_x;
		float up = sp->pivot_y;
		float down = - (sp->height - sp->pivot_y);

		left *= scale;
		right *= scale;
		up *= scale;
		down *= scale;

		PLATFORM_RENDERER_draw_textured_quad(
			bmps[bitmap_number].texture_handle,
			r, g, b,
			x - left, virtual_screen_height - (y + up), virtual_screen_height,
			left, right, up, down,
			u1, v1, u2, v2,
			true);
	}

}


void OUTPUT_shut_down_screen_update (void)
{
	int t;

	for (t=0; t<3; t++)
	{
		if (pages[t])
		{
			destroy_bitmap (pages[t]);
			pages[t] = NULL;
		}
	}
    
    if ((update_method == DOUBLEBUFFER) || (update_method == SYSTEMBUFFER))
    {
        destroy_bitmap (active_page);
		active_page = NULL;

        destroy_bitmap (current_page);
		current_page = NULL;
    }
}



void OUTPUT_shutdown (void)
{
	// Destroys all the bitmaps in memory...

	MAIN_add_to_log ("Destroying graphics...");

	int bitmap_number;

	if (software_mode_active)
	{
		if (bmps != NULL)
		{
			for (bitmap_number=0 ; bitmap_number < total_bitmaps_loaded ; bitmap_number++)
			{
				if (load_from_dat_file == false) // You really don't wanna' try destroying them inside the packfile.
				{
					destroy_bitmap(bmps[bitmap_number].image);
					bmps[bitmap_number].image = NULL;
				}
			}
		}

		OUTPUT_shut_down_screen_update ();
	}

	MAIN_add_to_log ("\tOK!");

	PLATFORM_RENDERER_shutdown();
}



#define ARROW_SEGMENT_LENGTH		(16)

void OUTPUT_draw_arrow (int x1, int y1, int x2, int y2, int red, int green, int blue, bool suppress_line)
{
	if (suppress_line == false)
	{
		OUTPUT_line (x1,y1,x2,y2,red,green,blue);
	}

	float angle = atan2( (x2-x1) , (y2-y1) );
	float arrow_angle_1 = angle - (PI/4);
	float arrow_angle_2 = angle + (PI/4);
	
	int dx = (x2-x1);
	int dy = (y2-y1);

	int mid_x = x1 + ((dx*2)/3);
	int mid_y = y1 + ((dy*2)/3);

	OUTPUT_line ( mid_x, mid_y, mid_x-(ARROW_SEGMENT_LENGTH*sin(arrow_angle_1)), mid_y-(ARROW_SEGMENT_LENGTH*cos(arrow_angle_1)), red, green, blue );
	OUTPUT_line ( mid_x, mid_y, mid_x-(ARROW_SEGMENT_LENGTH*sin(arrow_angle_2)), mid_y-(ARROW_SEGMENT_LENGTH*cos(arrow_angle_2)), red, green, blue );
}



static void OUTPUT_apply_texture_parameters_from_flags(int opengl_booleans, int old_opengl_booleans, bool force_filter_apply = false)
{
	bool filtered = (opengl_booleans & OPENGL_BOOLEAN_FILTERED) != 0;
	bool old_filtered = (old_opengl_booleans & OPENGL_BOOLEAN_FILTERED) != 0;
	bool state_changed = (opengl_booleans != old_opengl_booleans);

	PLATFORM_RENDERER_apply_texture_parameters(filtered, old_filtered, state_changed, force_filter_apply);
}

static void OUTPUT_configure_secondary_multitexture_state(int opengl_booleans, int secondary_opengl_booleans, int &old_secondary_opengl_booleans)
{
	bool double_mask_mode = OUTPUT_is_double_multitexture_mode(opengl_booleans);

	PLATFORM_RENDERER_prepare_multitexture_second_unit(double_mask_mode);
	OUTPUT_apply_texture_parameters_from_flags(secondary_opengl_booleans, old_secondary_opengl_booleans);
	old_secondary_opengl_booleans = secondary_opengl_booleans;
	PLATFORM_RENDERER_finalize_multitexture_second_unit(double_mask_mode);
}



void OUTPUT_draw_starfield (int starfield_id)
{
	int starfield_number = PARTICLES_get_index_from_id (starfield_id);

	if (starfield_number == UNSET)
	{
		// Invalid ID.
		return;
	}

	if (starfields[starfield_number].in_use == false)
	{
		// Trying to draw a non-existant starfield! Naughty!
		return;
	}

	starfield_controller_struct *scp = &starfields[starfield_number];
	starfield_struct *sfp = &scp->stars[0];

	int star_count = scp->number_of_stars;

	int counter;

	for (counter = 0; counter < star_count; counter++)
	{
		PLATFORM_RENDERER_draw_point(sfp->x, -sfp->y);

		sfp++;
	}
}



void OUTPUT_draw_starfield_colour (int starfield_id)
{
	int starfield_number = PARTICLES_get_index_from_id (starfield_id);

	if (starfields[starfield_number].in_use == false)
	{
		// Trying to draw a non-existant starfield! Naughty!
		return;
	}

	starfield_controller_struct *scp = &starfields[starfield_number];
	starfield_struct *sfp = &scp->stars[0];

	int star_count = scp->number_of_stars;

	int counter;
	int ramp_entry;

	for (counter = 0; counter < star_count; counter++)
	{
		ramp_entry = sfp->percentage_speed;

		PLATFORM_RENDERER_draw_coloured_point(
			sfp->x, -sfp->y,
			scp->red_ramp[ramp_entry],
			scp->green_ramp[ramp_entry],
			scp->blue_ramp[ramp_entry]);

		sfp++;
	}
}



void OUTPUT_draw_starfield_lines (int starfield_id)
{
	static bool starfield_lines_trace_checked_env = false;
	static bool starfield_lines_trace_enabled = false;
	static unsigned int starfield_lines_trace_counter = 0;

	if (!starfield_lines_trace_checked_env)
	{
		starfield_lines_trace_enabled = OUTPUT_env_enabled("WIZBALL_SDL2_LINE_TRACE");
		starfield_lines_trace_checked_env = true;
	}

	int starfield_number = PARTICLES_get_index_from_id (starfield_id);

	if (starfields[starfield_number].in_use == false)
	{
		// Trying to draw a non-existant starfield! Naughty!
		return;
	}

	starfield_controller_struct *scp = &starfields[starfield_number];
	starfield_struct *sfp = &scp->stars[0];

	int star_count = scp->number_of_stars;
	if (starfield_lines_trace_enabled)
	{
		starfield_lines_trace_counter++;
		if ((starfield_lines_trace_counter <= 200) || ((starfield_lines_trace_counter % 5000) == 0))
		{
			fprintf(stderr,
				"[SDL2-SFLINES] n=%u id=%d idx=%d in_use=%d stars=%d\n",
				starfield_lines_trace_counter,
				starfield_id,
				starfield_number,
				scp->in_use ? 1 : 0,
				star_count);
		}
	}

	int counter;

	for (counter = 0; counter < star_count; counter++)
	{
		PLATFORM_RENDERER_draw_line(sfp->x, -sfp->y, sfp->ox, -sfp->oy);

		sfp++;
	}
}



void OUTPUT_draw_starfield_colour_lines (int starfield_id)
{
	int starfield_number = PARTICLES_get_index_from_id (starfield_id);

	if (starfields[starfield_number].in_use == false)
	{
		// Trying to draw a non-existant starfield! Naughty!
		return;
	}

	starfield_controller_struct *scp = &starfields[starfield_number];
	starfield_struct *sfp = &scp->stars[0];

	int star_count = scp->number_of_stars;

	int counter;
	int ramp_entry;

	for (counter = 0; counter < star_count; counter++)
	{
		ramp_entry = sfp->percentage_speed;

		PLATFORM_RENDERER_draw_coloured_line(
			sfp->x, -sfp->y, sfp->ox, -sfp->oy,
			scp->red_ramp[ramp_entry],
			scp->green_ramp[ramp_entry],
			scp->blue_ramp[ramp_entry]);

		sfp++;
	}
}



void OUTPUT_blank_window_contents (int window_number)
{
	int z_ordering_list;
	int z_ordering_list_size = window_details[window_number].z_ordering_list_size;
	
	for (z_ordering_list=0; z_ordering_list<z_ordering_list_size; z_ordering_list++)
	{
		window_details[window_number].z_ordering_list_starts[z_ordering_list] = UNSET;
		window_details[window_number].z_ordering_list_ends[z_ordering_list] = UNSET;
	}
}



void OUTPUT_list_all_window_contents (int look_for_script)
{
	int window_number;
	int counter = 0;

	for (window_number=0; window_number<number_of_windows; window_number++)
	{
		if (window_details[window_number].active == true)
		{
			window_struct *wp = &window_details[window_number];

			int *entity_pointer;

			int current_entity;

			int z_ordering_list;
			int z_ordering_list_size = wp->z_ordering_list_size;

				for (z_ordering_list=0; z_ordering_list<z_ordering_list_size; z_ordering_list++)
				{
					int queue_step_counter = 0;
					current_entity = window_details[window_number].z_ordering_list_starts[z_ordering_list];

					while (current_entity != UNSET)
				{
					entity_pointer = &entity[current_entity][0];

						if (entity_pointer[ENT_SCRIPT_NUMBER]==look_for_script)
						{
							counter++;
						}

						if (!OUTPUT_window_queue_step(window_number, z_ordering_list, &current_entity, &queue_step_counter))
						{
							current_entity = UNSET;
						}
					}
				}

		}
	}
}



void OUTPUT_draw_window_collision_contents (int window_number, bool draw_world_collision)
{
	int z_ordering_list_size = window_details[window_number].z_ordering_list_size;
	int z_ordering_list;

	float left,right,up,down;
	int x;
	int y;

	window_struct *wp = &window_details[window_number];

	int *entity_pointer;

	int window_x = wp->current_x;
	int window_y = wp->current_y;

	int window_width = wp->width;
	int window_height = wp->height;

	int current_entity;

	float rotate_angle;

	char debug_text[MAX_LINE_SIZE];

	#ifdef RETRENGINE_DEBUG_VERSION_THE_LAST_THING_I_DID
		MAIN_debug_last_thing ("Got into draw window collision contents...");
	#endif

	#ifdef RETRENGINE_DEBUG_VERSION_THE_LAST_THING_I_DID
		sprintf (debug_text,"Collision window number %i = %i %i ",window_number,window_details[window_number].width,window_details[window_number].height);
		MAIN_debug_last_thing (debug_text);
	#endif

	float display_scale_x,display_scale_y;
	float window_scale_x,window_scale_y;
	float total_scale_x,total_scale_y;

//	if (game_screen_width != virtual_screen_width)
//	{
//		display_scale_x = game_screen_width/virtual_screen_width;
//	}
//	else
	{
		display_scale_x = 1.0f;
	}

//	if (game_screen_height != virtual_screen_height)
//	{
//		display_scale_y = game_screen_height/virtual_screen_height;
//	}
//	else
	{
		display_scale_y = 1.0f;
	}

	float left_window_transform_x = (wp->screen_x + wp->opengl_transform_x) * display_scale_x;
	float bottom_window_transform_y = virtual_screen_height - ( (wp->screen_y + wp->opengl_transform_y + wp->scaled_height) * display_scale_y);
	float top_window_transform_y = virtual_screen_height - ( (wp->screen_y + wp->opengl_transform_y) * display_scale_y);

	window_scale_x = window_details[window_number].opengl_scale_x;
	window_scale_y = window_details[window_number].opengl_scale_y;

	total_scale_x = window_scale_x * display_scale_x;
	total_scale_y = window_scale_y * display_scale_y;
	OUTPUT_sanitize_window_transform_values(
		window_number,
		"collision",
		&left_window_transform_x,
		&top_window_transform_y,
		&total_scale_x,
		&total_scale_y);

	int collision_grid_size = 1<<collision_bitshift;

//	BITMAP *window_buffer = create_sub_bitmap (buffer , window_details[window_number].screen_x , window_details[window_number].screen_y , window_details[window_number].width, window_details[window_number].height );

	PLATFORM_RENDERER_set_window_scissor(
		left_window_transform_x,
		bottom_window_transform_y,
		wp->scaled_width,
		wp->scaled_height,
		display_scale_x,
		display_scale_y,
		1.0f);

	#ifdef RETRENGINE_DEBUG_VERSION_THE_LAST_THING_I_DID
		MAIN_debug_last_thing ("Reset main colour to white...");
	#endif

	PLATFORM_RENDERER_set_colour4f(1.0f, 1.0f, 1.0f, 1.0f);

	if (window_details[window_number].skip_me_this_frame)
	{
		for (z_ordering_list=0; z_ordering_list<z_ordering_list_size; z_ordering_list++)
		{
			window_details[window_number].z_ordering_list_starts[z_ordering_list] = UNSET;
			window_details[window_number].z_ordering_list_ends[z_ordering_list] = UNSET;
		}

		#ifdef RETRENGINE_DEBUG_VERSION_THE_LAST_THING_I_DID
			MAIN_debug_last_thing ("Window skipped");
		#endif
	}
	else
	{
		PLATFORM_RENDERER_set_texture_enabled(false);

		// Draw the collision grid at the correct resolution.

		int col_grid_x_offset = window_x % collision_grid_size;
		int col_grid_y_offset = window_y % collision_grid_size;

		PLATFORM_RENDERER_set_window_transform(left_window_transform_x, top_window_transform_y, total_scale_x, total_scale_y);

		PLATFORM_RENDERER_set_colour3f(0.0f, 0.0f, 1.0f);

		if (draw_world_collision == false)
		{
			for (x=0; x<window_details[window_number].width/total_scale_x; x+=collision_grid_size)
			{
				PLATFORM_RENDERER_draw_line(
					x-col_grid_x_offset, 0,
					x-col_grid_x_offset, -window_details[window_number].height/total_scale_y);
			}

			for (y=0; y<window_details[window_number].height/total_scale_y; y+=collision_grid_size)
			{
				PLATFORM_RENDERER_draw_line(
					0, -(y-col_grid_y_offset),
					window_details[window_number].width/total_scale_x, -(y-col_grid_y_offset));
			}
		}

		int z_ordering_list;

			for (z_ordering_list=0; z_ordering_list<z_ordering_list_size; z_ordering_list++)
			{
				int queue_step_counter = 0;
				PLATFORM_RENDERER_set_window_transform(left_window_transform_x, top_window_transform_y, total_scale_x, total_scale_y);

			current_entity = window_details[window_number].z_ordering_list_starts[z_ordering_list];

			while (current_entity != UNSET)
			{
				entity_pointer = &entity[current_entity][0];

				PLATFORM_RENDERER_set_window_transform(left_window_transform_x, top_window_transform_y, total_scale_x, total_scale_y);

				if (draw_world_collision == true)
				{

					x = entity_pointer[ENT_WORLD_X] - window_x;
					y = entity_pointer[ENT_WORLD_Y] - window_y;

					left = -entity_pointer[ENT_UPPER_WORLD_WIDTH];
					right = entity_pointer[ENT_LOWER_WORLD_WIDTH];
					up = entity_pointer[ENT_UPPER_WORLD_HEIGHT];
					down = -entity_pointer[ENT_LOWER_WORLD_HEIGHT];

					PLATFORM_RENDERER_translatef(x,-y,0.0);

					PLATFORM_RENDERER_set_colour3f(0.0f, 1.0f, 0.0f);

					{
						const float loop_x[4] = { left, left, right, right };
						const float loop_y[4] = { up, down, down, up };
						PLATFORM_RENDERER_draw_line_loop_array(loop_x, loop_y, 4);
					}

					PLATFORM_RENDERER_set_colour3f(1.0f, 1.0f, 1.0f);

					PLATFORM_RENDERER_draw_line(left, 0, right, 0);

					PLATFORM_RENDERER_draw_line(0, up, 0, down);

					PLATFORM_RENDERER_translatef(-x,y,0);

				}
				else
				{
					switch (entity_pointer[ENT_COLLISION_SHAPE])
					{

					case SHAPE_NONE:
						break;
						
					case SHAPE_POINT:
						break;

					case SHAPE_CIRCLE:
						x = entity_pointer[ENT_WORLD_X] - window_x;
						y = entity_pointer[ENT_WORLD_Y] - window_y;

						left = -entity_pointer[ENT_RADIUS];
						right = entity_pointer[ENT_RADIUS];
						up = entity_pointer[ENT_RADIUS];
						down = -entity_pointer[ENT_RADIUS];

						PLATFORM_RENDERER_translatef(x,-y,0.0);

						PLATFORM_RENDERER_set_colour3f(0.0f, 1.0f, 0.0f);

						{
							const float loop_x[8] = { left, left*0.707f, 0, right*0.707f, right, right*0.707f, 0, left*0.707f };
							const float loop_y[8] = { 0, up*0.707f, up, up*0.707f, 0, down*0.707f, down, down*0.707f };
							PLATFORM_RENDERER_draw_line_loop_array(loop_x, loop_y, 8);
						}

						PLATFORM_RENDERER_set_colour3f(1.0f, 1.0f, 1.0f);

						PLATFORM_RENDERER_draw_line(left, 0, right, 0);

						PLATFORM_RENDERER_draw_line(0, up, 0, down);

						PLATFORM_RENDERER_translatef(-x,y,0);
						break;

					case SHAPE_ROTATED_RECTANGLE:
					case SHAPE_RECTANGLE:

						x = entity_pointer[ENT_WORLD_X] - window_x;
						y = entity_pointer[ENT_WORLD_Y] - window_y;

						left = -entity_pointer[ENT_UPPER_WIDTH];
						right = entity_pointer[ENT_LOWER_WIDTH];
						up = entity_pointer[ENT_UPPER_HEIGHT];
						down = -entity_pointer[ENT_LOWER_HEIGHT];

						PLATFORM_RENDERER_translatef(x,-y,0.0);

						if (entity_pointer[ENT_COLLISION_SHAPE] == SHAPE_ROTATED_RECTANGLE)
						{
							rotate_angle = float (entity_pointer[ENT_OPENGL_ANGLE]) / 100.0f;
							PLATFORM_RENDERER_rotatef(-rotate_angle , 0.0f, 0.0f, 1.0f);
						}

						PLATFORM_RENDERER_set_colour3f(0.0f, 1.0f, 0.0f);

						{
							const float loop_x[4] = { left, left, right, right };
							const float loop_y[4] = { up, down, down, up };
							PLATFORM_RENDERER_draw_line_loop_array(loop_x, loop_y, 4);
						}

						PLATFORM_RENDERER_set_colour3f(1.0f, 1.0f, 1.0f);

						PLATFORM_RENDERER_draw_line(left, 0, right, 0);

						PLATFORM_RENDERER_draw_line(0, up, 0, down);

						if (entity_pointer[ENT_COLLISION_SHAPE] == SHAPE_ROTATED_RECTANGLE)
						{
							PLATFORM_RENDERER_rotatef(rotate_angle , 0.0f, 0.0f, 1.0f);	
						}

						PLATFORM_RENDERER_translatef(-x,y,0);
						break;

					default:
					{
						static bool draw_mode_trace_checked_env = false;
						static bool draw_mode_trace_enabled = false;
						if (draw_mode_trace_checked_env == false)
						{
							const char *trace_value = getenv("WIZBALL_WINDOW_QUEUE_TRACE");
							if ((trace_value != NULL) && (atoi(trace_value) != 0))
							{
								draw_mode_trace_enabled = true;
							}
							draw_mode_trace_checked_env = true;
						}
						if (draw_mode_trace_enabled)
						{
								fprintf(stderr,
									"[OUTPUT] unknown draw mode: entity=%d mode=%d script=%d window=%d draw_order=%d alive=%d\n",
									current_entity,
									entity_pointer[ENT_DRAW_MODE],
									entity_pointer[ENT_SCRIPT_NUMBER],
									entity_pointer[ENT_WINDOW_NUMBER],
									entity_pointer[ENT_DRAW_ORDER],
									entity_pointer[ENT_ALIVE]);
								fflush(stderr);
							}
						// Skip unknown modes in non-debug paths to avoid crashing while tracing queue corruption.
						break;
					}
					}
				}

					if (!OUTPUT_window_queue_step(window_number, z_ordering_list, &current_entity, &queue_step_counter))
					{
						current_entity = UNSET;
					}
				}

			window_details[window_number].z_ordering_list_starts[z_ordering_list] = UNSET;
			window_details[window_number].z_ordering_list_ends[z_ordering_list] = UNSET;
		}

	}

	PLATFORM_RENDERER_finish_textured_window_draw(
		false,
		false,
		false,
		window_details[window_number].secondary_window_colour == true,
		game_screen_width,
		game_screen_height);

}



void OUTPUT_destroy_bitmap_and_sprite_containers (void)
{
	if (bmps != NULL)
	{
		int bitmap_number;

		for (bitmap_number=0; bitmap_number<total_bitmaps_loaded; bitmap_number++)
		{
			if (bmps[bitmap_number].sprite_list != NULL)
			{
				free (bmps[bitmap_number].sprite_list);
			}
		}

		free (bmps);
		bmps = NULL;
	}

	// Keep renderer texture handle bookkeeping aligned with bitmap container teardown.
	PLATFORM_RENDERER_reset_texture_registry();

	total_bitmaps_loaded = 0;
	total_bitmaps_allocated = 0;
}



int OUTPUT_draw_window_contents (int window_number, bool texture_combiner_available)
{
	// Constrain drawing buffer to window and then get weaving!
	// Goes through all the z-ordering queues splunking out the contents to the screen.

	char debug_text[MAX_LINE_SIZE];

	#ifdef RETRENGINE_DEBUG_VERSION_THE_LAST_THING_I_DID
		MAIN_debug_last_thing ("Got into draw window contents...");
	#endif

	#ifdef RETRENGINE_DEBUG_VERSION_THE_LAST_THING_I_DID
		sprintf (debug_text,"Window number %i = %i %i ",window_number,window_details[window_number].width,window_details[window_number].height);
		MAIN_debug_last_thing (debug_text);
	#endif

	int z_ordering_list_size = window_details[window_number].z_ordering_list_size;
	int z_ordering_list;

	int old_bitmap_number = UNSET;
	int bitmap_number;

	int frame_number;

	int total_entities_drawn = 0;

	float u1,v1,u2,v2;

	float left,right,up,down;
	int x;
	int y;

	int opengl_booleans;
	int old_opengl_booleans = 0;

	int secondary_frame_number;
	float secondary_u1,secondary_v1,secondary_u2,secondary_v2;
	int secondary_bitmap_number;
	int old_secondary_bitmap_number = UNSET;
	int secondary_opengl_booleans;
	int old_secondary_opengl_booleans = 0;

	int start_bx,start_by,end_bx,end_by;

	float interp_u1,interp_v1,interp_u2,interp_v2;
	float interp_x,interp_y;
	int interp_frame_number;
	int secondary_interp_frame_number;
	
	int bx,by;

	int x_under, y_under;

	short *map_pointer;
	short *map_line_pointer;
	
	short *opt_pointer;
	short *opt_offset;

	sprite *sp = NULL;
	sprite *interp_sp = NULL;

	sprite *secondary_sp = NULL;
	sprite *secondary_interp_sp = NULL;

	window_struct *wp = &window_details[window_number];

	int tileset;
	int tilesize;
	int tile_graphic;
	bool effect_trace_enabled = OUTPUT_is_sdl_effect_trace_enabled();
	static bool window_geom_trace_checked_env = false;
	static bool window_geom_trace_enabled = false;
	static unsigned int window_geom_trace_frame_counter = 0;
	int trace_total_entities = 0;
	int trace_sprite_entities = 0;
	int trace_multitexture_entities = 0;
	int trace_double_mask_entities = 0;
	int trace_secondary_colour_entities = 0;
	int trace_arbitrary_quad_entities = 0;
	int trace_arbitrary_perspective_entities = 0;
	int trace_vertex_colour_alpha_entities = 0;
	int trace_blend_add_entities = 0;
	int trace_blend_multiply_entities = 0;
	int trace_blend_subtract_entities = 0;
	int trace_first_non_sprite_mode = UNSET;
	int trace_first_non_sprite_script = UNSET;
	int trace_first_non_sprite_entity = UNSET;
	int trace_first_non_sprite_flags = 0;

	int layer_number;

	int map_number;

	int *entity_pointer;

	int draw_type;

	int map_width,map_height;

	if (!window_geom_trace_checked_env)
	{
		window_geom_trace_enabled = OUTPUT_env_enabled("WIZBALL_SDL2_WINDOW_GEOM_TRACE");
		window_geom_trace_checked_env = true;
	}

	if (window_geom_trace_enabled && (window_number == 0))
	{
		window_geom_trace_frame_counter++;
		if ((window_geom_trace_frame_counter % 60) == 0)
		{
			fprintf(
				stderr,
				"[SDL2-WGEOM] frame=%u win=%d scr=(%.2f,%.2f %dx%d) world=(%d,%d) scale=(%.4f,%.4f) scaled=(%.2f,%.2f) xform=(%.4f,%.4f) buffered=(%d,%d)-(%d,%d)\n",
				window_geom_trace_frame_counter,
				window_number,
				wp->screen_x, wp->screen_y, wp->width, wp->height,
				wp->current_x, wp->current_y,
				wp->opengl_scale_x, wp->opengl_scale_y,
				wp->scaled_width, wp->scaled_height,
				wp->opengl_transform_x, wp->opengl_transform_y,
				wp->buffered_tl_x, wp->buffered_tl_y, wp->buffered_br_x, wp->buffered_br_y
			);
		}
	}

	int window_x = wp->current_x;
	int window_y = wp->current_y;

	int window_width = wp->width;
	int window_height = wp->height;

	int current_entity;

	float rotate_angle;
	float scale_x;
	float scale_y;

	float rotate_angle_2;
	float scale_x_2;
	float scale_y_2;

	float temp_uv;

	float tileset_drawing_start_offset_x;
	float tileset_drawing_start_offset_y;
	float next_line_transform_x;
	float next_line_transform_y;

	bool map_optimised;

	arbitrary_quads_struct *aq_pointer;
	arbitrary_vertex_colour_struct *avc_pointer;

	float line_length_ratio;

	#ifdef RETRENGINE_DEBUG_VERSION_THE_LAST_THING_I_DID
		MAIN_debug_last_thing ("Just before secondary colour...");
	#endif

	if (secondary_colour_available)
	{
		if (window_details[window_number].secondary_window_colour == true)
		{
			#ifdef RETRENGINE_DEBUG_VERSION_THE_LAST_THING_I_DID
				MAIN_debug_last_thing ("Setting secondary colour...");
			#endif

			PLATFORM_RENDERER_set_colour_sum_enabled(true);
			PLATFORM_RENDERER_set_secondary_colour( float(wp->vertex_red)/256.0f , float(wp->vertex_green)/256.0f , float(wp->vertex_blue)/256.0f );
		}
	}
	else
	{
		#ifdef RETRENGINE_DEBUG_VERSION_THE_LAST_THING_I_DID
			MAIN_debug_last_thing ("Skipping secondary colour...");
		#endif
	}

	#ifdef RETRENGINE_DEBUG_VERSION_THE_LAST_THING_I_DID
		MAIN_debug_last_thing ("Just after secondary colour...");
	#endif

	float display_scale_x,display_scale_y;
	float window_scale_x,window_scale_y;
	float total_scale_x,total_scale_y;

//	if (game_screen_width != virtual_screen_width)
//	{
//		display_scale_x = float(game_screen_width)/float(virtual_screen_width);
//	}
//	else
	{
		display_scale_x = 1.0f;
	}

//	if (game_screen_height != virtual_screen_height)
//	{
//		display_scale_y = float(game_screen_height)/float(virtual_screen_height);
//	}
//	else
	{
		display_scale_y = 1.0f;
	}

	float left_window_transform_x = (wp->screen_x + wp->opengl_transform_x) * display_scale_x;
	float bottom_window_transform_y = virtual_screen_height - ( (wp->screen_y + wp->opengl_transform_y + wp->scaled_height) * display_scale_y);
	float top_window_transform_y = virtual_screen_height - ( (wp->screen_y + wp->opengl_transform_y) * display_scale_y);

	window_scale_x = window_details[window_number].opengl_scale_x;
	window_scale_y = window_details[window_number].opengl_scale_y;

	total_scale_x = window_scale_x * display_scale_x;
	total_scale_y = window_scale_y * display_scale_y;

//	BITMAP *window_buffer = create_sub_bitmap (buffer , window_details[window_number].screen_x , window_details[window_number].screen_y , window_details[window_number].width, window_details[window_number].height );

//	float_window_scale_multiplier

	PLATFORM_RENDERER_set_window_scissor(
		left_window_transform_x,
		bottom_window_transform_y,
		wp->scaled_width,
		wp->scaled_height,
		display_scale_x,
		display_scale_y,
		float_window_scale_multiplier);
	// This restricts all drawing to the given area. Remember that co-ords start from the bottom left and go up-right!
	
	#ifdef RETRENGINE_DEBUG_VERSION_THE_LAST_THING_I_DID
		MAIN_debug_last_thing ("Reset main colour to white...");
	#endif

	PLATFORM_RENDERER_set_colour4f(1.0f, 1.0f, 1.0f, 1.0f);

	if (window_details[window_number].skip_me_this_frame)
	{
		#ifndef RETRENGINE_DEBUG_VERSION_VIEW_WORLD_COLLISION
			#ifndef RETRENGINE_DEBUG_VERSION_VIEW_OBJECT_COLLISION
				for (z_ordering_list=0; z_ordering_list<z_ordering_list_size; z_ordering_list++)
				{
					window_details[window_number].z_ordering_list_starts[z_ordering_list] = UNSET;
					window_details[window_number].z_ordering_list_ends[z_ordering_list] = UNSET;
				}

				window_details[window_number].skip_me_this_frame--;
			#endif
		#endif

		#ifdef RETRENGINE_DEBUG_VERSION_THE_LAST_THING_I_DID
			MAIN_debug_last_thing ("Window skipped");
		#endif
	}
	else
	{
		PLATFORM_RENDERER_prepare_textured_window_draw(texture_combiner_available);

			for (z_ordering_list=0; z_ordering_list<z_ordering_list_size; z_ordering_list++)
			{
				int queue_step_counter = 0;
				PLATFORM_RENDERER_set_window_transform(left_window_transform_x, top_window_transform_y, total_scale_x, total_scale_y);

			current_entity = window_details[window_number].z_ordering_list_starts[z_ordering_list];

			while (current_entity != UNSET)
			{
				total_entities_drawn++;

				entity_pointer = &entity[current_entity][0];

				#ifdef RETRENGINE_DEBUG_VERSION_ENTITY_DEBUG_FLAG_CHECKS
				if (entity_pointer[ENT_DEBUG_FLAGS] & DEBUG_FLAG_STOP_WHEN_DRAWN_IN_WINDOW)
				{
					assert(0);
				}
				#endif

					draw_type = entity_pointer[ENT_DRAW_MODE];
					opengl_booleans = entity_pointer[ENT_OPENGL_BOOLEANS];
					if (effect_trace_enabled)
					{
						trace_total_entities++;
						if (draw_type == DRAW_MODE_SPRITE) trace_sprite_entities++;
						if ((draw_type != DRAW_MODE_SPRITE) && (trace_first_non_sprite_mode == UNSET))
						{
							trace_first_non_sprite_mode = draw_type;
							trace_first_non_sprite_script = entity_pointer[ENT_SCRIPT_NUMBER];
							trace_first_non_sprite_entity = current_entity;
							trace_first_non_sprite_flags = opengl_booleans;
						}
						if (OUTPUT_has_multitexture_flags(opengl_booleans)) trace_multitexture_entities++;
						if (OUTPUT_is_double_multitexture_mode(opengl_booleans)) trace_double_mask_entities++;
						if (opengl_booleans & OPENGL_BOOLEAN_VERTEX_SECONDARY_COLOUR) trace_secondary_colour_entities++;
						if (opengl_booleans & OPENGL_BOOLEAN_ARBITRARY_QUAD) trace_arbitrary_quad_entities++;
						if (opengl_booleans & OPENGL_BOOLEAN_ARBITRARY_PERSPECTIVE_QUAD) trace_arbitrary_perspective_entities++;
						if (opengl_booleans & OPENGL_BOOLEAN_INDIVIDUAL_VERTEX_COLOUR_ALPHA) trace_vertex_colour_alpha_entities++;
						if (opengl_booleans & OPENGL_BOOLEAN_BLEND_ADD) trace_blend_add_entities++;
						if (opengl_booleans & OPENGL_BOOLEAN_BLEND_MULTIPLY) trace_blend_multiply_entities++;
						if (opengl_booleans & OPENGL_BOOLEAN_BLEND_SUBTRACT) trace_blend_subtract_entities++;
					}

					#ifdef RETRENGINE_DEBUG_VERSION
						if (draw_type == DRAW_MODE_SPRITE)
						{
							int sprite_index = entity_pointer[ENT_SPRITE];
							int frame_index = entity_pointer[ENT_CURRENT_FRAME];
							if (!OUTPUT_is_valid_sprite_frame(sprite_index, frame_index))
							{
								int max_frame = -1;
								if (OUTPUT_is_valid_bitmap_index(sprite_index))
								{
									max_frame = bmps[sprite_index].sprite_count - 1;
								}

								sprintf (debug_text,"Entity %s is trying to access frame %i (max is %i)", GPL_get_entry_name("SCRIPTS",entity_pointer[ENT_SCRIPT_NUMBER]) , frame_index , max_frame );
								MAIN_add_to_log(debug_text);
							}
						}
					#endif

					#ifdef RETRENGINE_DEBUG_VERSION_THE_LAST_THING_I_DID
						sprintf (debug_text,"Entity %i = %s ",current_entity, GPL_get_entry_name("SCRIPTS",entity_pointer[ENT_SCRIPT_NUMBER]) );
						MAIN_debug_last_thing (debug_text);
					#endif

					#ifdef RETRENGINE_DEBUG_VERSION_THE_LAST_THING_I_DID
						sprintf (debug_text,"Draw Type = %i",draw_type );
						MAIN_debug_last_thing (debug_text);

					MAIN_debug_last_thing ("About to check multitexture sprite...");
				#endif

					if (texture_combiner_available)
					{
						secondary_bitmap_number = entity_pointer[ENT_SECONDARY_SPRITE];
						if (OUTPUT_is_secondary_multitexture_active(texture_combiner_available, opengl_booleans, secondary_bitmap_number))
					{
						#ifdef RETRENGINE_DEBUG_VERSION_THE_LAST_THING_I_DID
							MAIN_debug_last_thing ("About to set multi-texture sprite...");
						#endif

						PLATFORM_RENDERER_bind_secondary_texture(
							bmps[secondary_bitmap_number].texture_handle,
							old_secondary_bitmap_number == UNSET);

						#ifdef RETRENGINE_DEBUG_VERSION_THE_LAST_THING_I_DID
							MAIN_debug_last_thing ("Set multitexture sprite...");
						#endif
					}
					else if (old_secondary_bitmap_number != UNSET)
					{
						#ifdef RETRENGINE_DEBUG_VERSION_THE_LAST_THING_I_DID
							MAIN_debug_last_thing ("About to reset multitexture sprite...");
						#endif

						PLATFORM_RENDERER_disable_secondary_texture_and_restore_combiner();

						#ifdef RETRENGINE_DEBUG_VERSION_THE_LAST_THING_I_DID
							MAIN_debug_last_thing ("Reset multitexture sprite...");
						#endif
					}
					old_secondary_bitmap_number = secondary_bitmap_number;
				}

				#ifdef RETRENGINE_DEBUG_VERSION_THE_LAST_THING_I_DID
					MAIN_debug_last_thing ("About to bind texture sprite...");
				#endif

					bitmap_number = entity_pointer[ENT_SPRITE];
					bool bitmap_changed = false;
					const bool draw_mode_requires_bitmap = OUTPUT_draw_mode_requires_bitmap(draw_type);
						if (draw_mode_requires_bitmap && !OUTPUT_is_valid_bitmap_index(bitmap_number))
						{
						if (OUTPUT_env_enabled("WIZBALL_SDL2_LINE_TRACE"))
						{
							static unsigned int bitmap_skip_trace_counter = 0;
							bitmap_skip_trace_counter++;
							if ((bitmap_skip_trace_counter <= 200) || ((bitmap_skip_trace_counter % 5000) == 0))
							{
								fprintf(
									stderr,
									"[SDL2-BMPSKIP] n=%u entity=%d mode=%d script=%d sprite=%d flags=0x%x\n",
									bitmap_skip_trace_counter,
									current_entity,
									draw_type,
									entity_pointer[ENT_SCRIPT_NUMBER],
									bitmap_number,
									opengl_booleans);
							}
						}
							if (!OUTPUT_window_queue_step(window_number, z_ordering_list, &current_entity, &queue_step_counter))
							{
								current_entity = UNSET;
							}
							continue;
						}
					if ( (bitmap_number != old_bitmap_number) && (bitmap_number != UNSET) )
					{
						bitmap_changed = true;
						PLATFORM_RENDERER_bind_texture(bmps[bitmap_number].texture_handle);
					}

				if (bitmap_changed || (opengl_booleans != old_opengl_booleans))
				{
					OUTPUT_apply_texture_parameters_from_flags(opengl_booleans, old_opengl_booleans, bitmap_changed);
				}
				old_bitmap_number = bitmap_number;

				if (opengl_booleans != old_opengl_booleans)
				{
					if ( !(opengl_booleans & OPENGL_BOOLEAN_ROTATE) && (old_opengl_booleans & OPENGL_BOOLEAN_ROTATE) )
					{
						// ie, if the previous one was rotated and the new one ain't. Reset.

						#ifdef RETRENGINE_DEBUG_VERSION_THE_LAST_THING_I_DID
							MAIN_debug_last_thing ("Resetting Transform as previous entity was rotated and this one ain't.");
						#endif

						PLATFORM_RENDERER_set_window_transform(left_window_transform_x, top_window_transform_y, total_scale_x, total_scale_y);
					}
					else if ( !(opengl_booleans & OPENGL_BOOLEAN_SCALE) && (old_opengl_booleans & OPENGL_BOOLEAN_SCALE) )
					{
						// ie, if the previous one was scaled and the new one ain't. Reset.

						#ifdef RETRENGINE_DEBUG_VERSION_THE_LAST_THING_I_DID
							MAIN_debug_last_thing ("Resetting Transform as previous entity was scaled and this one ain't.");
						#endif

						PLATFORM_RENDERER_set_window_transform(left_window_transform_x, top_window_transform_y, total_scale_x, total_scale_y);
					}

					if ( !(opengl_booleans & OPENGL_BOOLEAN_BLEND_EITHER) && (old_opengl_booleans & OPENGL_BOOLEAN_BLEND_ADD) )
					{
						// Turn off additive blending

						#ifdef RETRENGINE_DEBUG_VERSION_THE_LAST_THING_I_DID
							MAIN_debug_last_thing ("Turn off blending.");
						#endif

						PLATFORM_RENDERER_set_blend_enabled(false);
					}
					else if ( (opengl_booleans & OPENGL_BOOLEAN_BLEND_ADD) && !(old_opengl_booleans & OPENGL_BOOLEAN_BLEND_ADD) )
					{
						// Turn on additive blending

						#ifdef RETRENGINE_DEBUG_VERSION_THE_LAST_THING_I_DID
							MAIN_debug_last_thing ("Turn on additive blending.");
						#endif

						PLATFORM_RENDERER_set_blend_enabled(true);
						PLATFORM_RENDERER_set_blend_mode_additive();
					}
					
					if ( !(opengl_booleans & OPENGL_BOOLEAN_BLEND_EITHER) && (old_opengl_booleans & OPENGL_BOOLEAN_BLEND_MULTIPLY) )
					{
						// Turn off multiplicative blending

						#ifdef RETRENGINE_DEBUG_VERSION_THE_LAST_THING_I_DID
							MAIN_debug_last_thing ("Turn off blending.");
						#endif

						PLATFORM_RENDERER_set_blend_enabled(false);
					}
					else if ( (opengl_booleans & OPENGL_BOOLEAN_BLEND_MULTIPLY) && !(old_opengl_booleans & OPENGL_BOOLEAN_BLEND_MULTIPLY) )
					{
						// Turn on multiplicative blending

						#ifdef RETRENGINE_DEBUG_VERSION_THE_LAST_THING_I_DID
							MAIN_debug_last_thing ("Turn on multiplicative blending.");
						#endif

						PLATFORM_RENDERER_set_blend_enabled(true);
						PLATFORM_RENDERER_set_blend_mode_alpha();
					}
					
					if ( !(opengl_booleans & OPENGL_BOOLEAN_BLEND_EITHER) && (old_opengl_booleans & OPENGL_BOOLEAN_BLEND_SUBTRACT) )
					{
						// Turn off subtractive blending

						#ifdef RETRENGINE_DEBUG_VERSION_THE_LAST_THING_I_DID
							MAIN_debug_last_thing ("Turn off blending.");
						#endif

						PLATFORM_RENDERER_set_blend_enabled(false);
					}
					else if ( (opengl_booleans & OPENGL_BOOLEAN_BLEND_SUBTRACT) && !(old_opengl_booleans & OPENGL_BOOLEAN_BLEND_SUBTRACT) )
					{
						// Turn on subtractive blending

						#ifdef RETRENGINE_DEBUG_VERSION_THE_LAST_THING_I_DID
							MAIN_debug_last_thing ("Turn on subtractive blending.");
						#endif

						PLATFORM_RENDERER_set_blend_enabled(true);
						PLATFORM_RENDERER_set_blend_mode_subtractive();
					}

					if ( !(opengl_booleans & OPENGL_BOOLEAN_MASKED) && (old_opengl_booleans & OPENGL_BOOLEAN_MASKED) )
					{
						// Turn off alpha test (masking)

						#ifdef RETRENGINE_DEBUG_VERSION_THE_LAST_THING_I_DID
							MAIN_debug_last_thing ("Turn off masking.");
						#endif

						PLATFORM_RENDERER_set_alpha_test_enabled(false);
					}
					else if ( (opengl_booleans & OPENGL_BOOLEAN_MASKED) && !(old_opengl_booleans & OPENGL_BOOLEAN_MASKED) )
					{
						// Turn on alpha test (masking)

						#ifdef RETRENGINE_DEBUG_VERSION_THE_LAST_THING_I_DID
							MAIN_debug_last_thing ("Turn on masking.");
						#endif

						PLATFORM_RENDERER_set_alpha_test_enabled(true);
						PLATFORM_RENDERER_set_alpha_func_greater(0.5f);
					}

					if (secondary_colour_available)
					{
						if ( !(opengl_booleans & OPENGL_BOOLEAN_VERTEX_SECONDARY_COLOUR) && (old_opengl_booleans & OPENGL_BOOLEAN_VERTEX_SECONDARY_COLOUR) )
						{
							// Reset secondary colour to black and turn it off.

							#ifdef RETRENGINE_DEBUG_VERSION_THE_LAST_THING_I_DID
								MAIN_debug_last_thing ("Turn off secondary colour.");
							#endif

							PLATFORM_RENDERER_set_secondary_colour( 0.0f , 0.0f , 0.0f );
							PLATFORM_RENDERER_set_colour_sum_enabled(false);
						}
						else if ( (opengl_booleans & OPENGL_BOOLEAN_VERTEX_SECONDARY_COLOUR) && !(old_opengl_booleans & OPENGL_BOOLEAN_VERTEX_SECONDARY_COLOUR) )
						{
							// Reset secondary colour to black and turn it on.

							#ifdef RETRENGINE_DEBUG_VERSION_THE_LAST_THING_I_DID
								MAIN_debug_last_thing ("Turn on secondary colour.");
							#endif

							PLATFORM_RENDERER_set_secondary_colour( 0.0f , 0.0f , 0.0f );
							PLATFORM_RENDERER_set_colour_sum_enabled(true);
						}
					}

					if ( (opengl_booleans & OPENGL_BOOLEAN_VERTEX_SECONDARY_COLOUR) && !(old_opengl_booleans & OPENGL_BOOLEAN_VERTEX_SECONDARY_COLOUR) )
					{
						// Turn on secondary colouring

						#ifdef RETRENGINE_DEBUG_VERSION_THE_LAST_THING_I_DID
							MAIN_debug_last_thing ("Turn on secondary colour.");
						#endif

						PLATFORM_RENDERER_set_colour_sum_enabled(true);
					}

					if ( !(opengl_booleans & OPENGL_BOOLEAN_VERTEX_COLOUR) && (old_opengl_booleans & OPENGL_BOOLEAN_VERTEX_COLOUR) )
					{
						// Reset vertex colour to white

						#ifdef RETRENGINE_DEBUG_VERSION_THE_LAST_THING_I_DID
							MAIN_debug_last_thing ("Reset vertex colour to white.");
						#endif

						PLATFORM_RENDERER_set_colour4f(1.0f, 1.0f, 1.0f, 1.0f);
					}
					
					if ( !(opengl_booleans & OPENGL_BOOLEAN_VERTEX_COLOUR_ALPHA) && (old_opengl_booleans & OPENGL_BOOLEAN_VERTEX_COLOUR_ALPHA) )
					{
						// Reset vertex colour to white

						#ifdef RETRENGINE_DEBUG_VERSION_THE_LAST_THING_I_DID
							MAIN_debug_last_thing ("Reset vertex colour to white.");
						#endif

						PLATFORM_RENDERER_set_colour4f(1.0f, 1.0f, 1.0f, 1.0f);
					}
				}

				if (secondary_colour_available)
				{
					if (opengl_booleans & OPENGL_BOOLEAN_VERTEX_SECONDARY_COLOUR)
					{
						#ifdef RETRENGINE_DEBUG_VERSION_THE_LAST_THING_I_DID
							MAIN_debug_last_thing ("Set secondary vertex colour.");
						#endif

						PLATFORM_RENDERER_set_secondary_colour( float(entity_pointer[ENT_OPENGL_VERTEX_RED])/256.0f , float(entity_pointer[ENT_OPENGL_VERTEX_GREEN])/256.0f , float(entity_pointer[ENT_OPENGL_VERTEX_BLUE])/256.0f );
					}
				}

				if (opengl_booleans & OPENGL_BOOLEAN_VERTEX_COLOUR)
				{
					#ifdef RETRENGINE_DEBUG_VERSION_THE_LAST_THING_I_DID
						MAIN_debug_last_thing ("Set primary vertex colour.");
					#endif

					PLATFORM_RENDERER_set_colour4f( float(entity_pointer[ENT_OPENGL_VERTEX_RED])/256.0f , float(entity_pointer[ENT_OPENGL_VERTEX_GREEN])/256.0f , float(entity_pointer[ENT_OPENGL_VERTEX_BLUE])/256.0f , 1.0f);
				}

				if (opengl_booleans & OPENGL_BOOLEAN_VERTEX_COLOUR_ALPHA)
				{
					#ifdef RETRENGINE_DEBUG_VERSION_THE_LAST_THING_I_DID
						MAIN_debug_last_thing ("Set primary vertex colour and alpha.");
					#endif

					PLATFORM_RENDERER_set_colour4f( float(entity_pointer[ENT_OPENGL_VERTEX_RED])/256.0f , float(entity_pointer[ENT_OPENGL_VERTEX_GREEN])/256.0f , float(entity_pointer[ENT_OPENGL_VERTEX_BLUE])/256.0f , float(entity_pointer[ENT_OPENGL_VERTEX_ALPHA])/256.0f );
				}

				#ifdef RETRENGINE_DEBUG_VERSION_THE_LAST_THING_I_DID
					MAIN_debug_last_thing ("Entering main switch...");
				#endif

				#ifdef RETRENGINE_DEBUG_VERSION
					if ((entity_pointer[ENT_OPENGL_SCALE_X] == 0) || (entity_pointer[ENT_OPENGL_SCALE_Y] == 0))
					{
						sprintf (debug_text,"Tiny scale error in entity '%s'!",GPL_get_entry_name("SCRIPTS",entity_pointer[ENT_SCRIPT_NUMBER]));
						OUTPUT_message(debug_text);
						assert(0);
					}
				#endif

				switch (draw_type)
				{
						case DRAW_MODE_SPRITE: // NEW TYPE!!!				
						{
							frame_number = entity_pointer[ENT_CURRENT_FRAME];
							if (!OUTPUT_is_valid_sprite_frame(bitmap_number, frame_number))
							{
								break;
							}

							if (opengl_booleans & OPENGL_BOOLEAN_INTERPOLATED)
							{
								interp_frame_number = entity_pointer[ENT_INTERPOLATED_FRAME];
								if (!OUTPUT_is_valid_sprite_frame(bitmap_number, interp_frame_number))
								{
									interp_frame_number = frame_number;
								}
								
								sp = &bmps[bitmap_number].sprite_list[frame_number];
								interp_sp = &bmps[bitmap_number].sprite_list[interp_frame_number];

							interp_x = float (entity_pointer[ENT_INTERPOLATION_X_PERCENTAGE]) / 10000.0f;
							interp_y = float (entity_pointer[ENT_INTERPOLATION_Y_PERCENTAGE]) / 10000.0f;

							u1 = sp->u1;
							u2 = sp->u2;
							v1 = sp->v1;
							v2 = sp->v2;

							interp_u1 = interp_sp->u1;
							interp_v1 = interp_sp->v1;
							interp_u2 = interp_sp->u2;
							interp_v2 = interp_sp->v2;

							u1 = MATH_lerp (u1,interp_u1,interp_x);
							u2 = MATH_lerp (u2,interp_u2,interp_x);
							v1 = MATH_lerp (v1,interp_v1,interp_y);
							v2 = MATH_lerp (v2,interp_v2,interp_y);
							}
							else
							{
								sp = &bmps[bitmap_number].sprite_list[frame_number];

								u1 = sp->u1;
							u2 = sp->u2;
							v1 = sp->v1;
							v2 = sp->v2;
						}

						x = entity_pointer[ENT_WORLD_X] - window_x;
						y = entity_pointer[ENT_WORLD_Y] - window_y;

						if (opengl_booleans & OPENGL_BOOLEAN_OVERRIDE_PIVOT)
						{
							left = -entity_pointer[ENT_OVERRIDE_PIVOT_X];
							right = sp->width - entity_pointer[ENT_OVERRIDE_PIVOT_X];
							up = entity_pointer[ENT_OVERRIDE_PIVOT_Y];
							down = - (sp->height - entity_pointer[ENT_OVERRIDE_PIVOT_Y]);
						}
						else
						{
							left = -sp->pivot_x;
							right = sp->width - sp->pivot_x;
							up = sp->pivot_y;
							down = - (sp->height - sp->pivot_y);
						}

						if (opengl_booleans & OPENGL_BOOLEAN_CLIP_FRAME)
						{
							u1 += sp->uv_pixel_size * float(entity_pointer[ENT_CUT_SPRITE_LEFT_INDENTATION]);
							u2 -= sp->uv_pixel_size * float(entity_pointer[ENT_CUT_SPRITE_RIGHT_INDENTATION]);
							v1 += sp->uv_pixel_size * float(entity_pointer[ENT_CUT_SPRITE_TOP_INDENTATION]);
							v2 -= sp->uv_pixel_size * float(entity_pointer[ENT_CUT_SPRITE_BOTTOM_INDENTATION]);

							left += entity_pointer[ENT_CUT_SPRITE_LEFT_INDENTATION];
							right -= entity_pointer[ENT_CUT_SPRITE_RIGHT_INDENTATION];
							up -= entity_pointer[ENT_CUT_SPRITE_TOP_INDENTATION];
							down += entity_pointer[ENT_CUT_SPRITE_BOTTOM_INDENTATION];
						}

						PLATFORM_RENDERER_translatef(x,-y,0.0);

						if (opengl_booleans & OPENGL_BOOLEAN_SECONDARY_SCALE)
						{
							scale_x_2 = float (entity_pointer[ENT_OPENGL_SECONDARY_SCALE_X]) / 10000.0f;
							scale_y_2 = float (entity_pointer[ENT_OPENGL_SECONDARY_SCALE_Y]) / 10000.0f;
							PLATFORM_RENDERER_scalef(scale_x_2,scale_y_2,1.0f);
						}

						if (opengl_booleans & OPENGL_BOOLEAN_ROTATE)
						{
							rotate_angle = float (entity_pointer[ENT_OPENGL_ANGLE]) / 100.0f;
							PLATFORM_RENDERER_rotatef(-rotate_angle , 0.0f, 0.0f, 1.0f);
						}

						if (opengl_booleans & OPENGL_BOOLEAN_SCALE)
						{
							scale_x = float (entity_pointer[ENT_OPENGL_SCALE_X]) / 10000.0f;
							scale_y = float (entity_pointer[ENT_OPENGL_SCALE_Y]) / 10000.0f;
							PLATFORM_RENDERER_scalef(scale_x,scale_y,1.0f);
						}

						if (opengl_booleans & OPENGL_BOOLEAN_SECONDARY_ROTATE)
						{
							rotate_angle_2 = float (entity_pointer[ENT_OPENGL_SECONDARY_ANGLE]) / 100.0f;
							PLATFORM_RENDERER_rotatef(-rotate_angle_2 , 0.0f, 0.0f, 1.0f);
						}

						if (opengl_booleans & OPENGL_BOOLEAN_HORI_FLIPPED)
						{
							temp_uv = u1;
							u1 = u2;
							u2 = temp_uv;
						}

						if (opengl_booleans & OPENGL_BOOLEAN_VERT_FLIPPED)
						{
							temp_uv = v1;
							v1 = v2;
							v2 = temp_uv;
						}

							if (OUTPUT_is_secondary_multitexture_active(texture_combiner_available, opengl_booleans, secondary_bitmap_number))
							{
								if (secondary_opengl_booleans & OPENGL_BOOLEAN_INTERPOLATED)
								{
									secondary_frame_number = entity_pointer[ENT_SECONDARY_CURRENT_FRAME];
									secondary_interp_frame_number = entity_pointer[ENT_SECONDARY_INTERPOLATED_FRAME];
									if (!OUTPUT_is_valid_sprite_frame(secondary_bitmap_number, secondary_frame_number))
									{
										secondary_frame_number = 0;
									}
									if (!OUTPUT_is_valid_sprite_frame(secondary_bitmap_number, secondary_interp_frame_number))
									{
										secondary_interp_frame_number = secondary_frame_number;
									}
									
									secondary_sp = &bmps[secondary_bitmap_number].sprite_list[secondary_frame_number];
									secondary_interp_sp = &bmps[secondary_bitmap_number].sprite_list[secondary_interp_frame_number];

								interp_x = float (entity_pointer[ENT_SECONDARY_INTERPOLATION_X_PERCENTAGE]) / 10000.0f;
								interp_y = float (entity_pointer[ENT_SECONDARY_INTERPOLATION_Y_PERCENTAGE]) / 10000.0f;

								secondary_u1 = secondary_sp->u1;
								secondary_u2 = secondary_sp->u2;
								secondary_v1 = secondary_sp->v1;
								secondary_v2 = secondary_sp->v2;

								interp_u1 = secondary_interp_sp->u1;
								interp_v1 = secondary_interp_sp->v1;
								interp_u2 = secondary_interp_sp->u2;
								interp_v2 = secondary_interp_sp->v2;

								secondary_u1 = MATH_lerp (secondary_u1,interp_u1,interp_x);
								secondary_u2 = MATH_lerp (secondary_u2,interp_u2,interp_x);
								secondary_v1 = MATH_lerp (secondary_v1,interp_v1,interp_y);
								secondary_v2 = MATH_lerp (secondary_v2,interp_v2,interp_y);
							}
								else
								{
									secondary_frame_number = entity_pointer[ENT_SECONDARY_CURRENT_FRAME];
									if (!OUTPUT_is_valid_sprite_frame(secondary_bitmap_number, secondary_frame_number))
									{
										secondary_frame_number = 0;
									}
										
									secondary_sp = &bmps[secondary_bitmap_number].sprite_list[secondary_frame_number];

									secondary_u1 = secondary_sp->u1;
									secondary_u2 = secondary_sp->u2;
								secondary_v1 = secondary_sp->v1;
								secondary_v2 = secondary_sp->v2;
							}

							if (secondary_opengl_booleans & OPENGL_BOOLEAN_HORI_FLIPPED)
							{
								temp_uv = secondary_u1;
								secondary_u1 = secondary_u2;
								secondary_u2 = temp_uv;
							}

							if (secondary_opengl_booleans & OPENGL_BOOLEAN_VERT_FLIPPED)
							{
								temp_uv = secondary_v1;
								secondary_v1 = secondary_v2;
								secondary_v2 = temp_uv;
							}

							if (opengl_booleans & OPENGL_BOOLEAN_CLIP_FRAME)
							{
								secondary_u1 += secondary_sp->uv_pixel_size * float(entity_pointer[ENT_CUT_SPRITE_LEFT_INDENTATION]);
								secondary_u2 -= secondary_sp->uv_pixel_size * float(entity_pointer[ENT_CUT_SPRITE_RIGHT_INDENTATION]);
								secondary_v1 += secondary_sp->uv_pixel_size * float(entity_pointer[ENT_CUT_SPRITE_TOP_INDENTATION]);
								secondary_v2 -= secondary_sp->uv_pixel_size * float(entity_pointer[ENT_CUT_SPRITE_BOTTOM_INDENTATION]);
							}

							PLATFORM_RENDERER_set_active_texture_unit0();
							OUTPUT_apply_texture_parameters_from_flags(opengl_booleans, old_opengl_booleans);

							if (OUTPUT_has_multitexture_flags(opengl_booleans))
							{
								secondary_opengl_booleans = entity_pointer[ENT_SECONDARY_OPENGL_BOOLEANS];
								OUTPUT_configure_secondary_multitexture_state(opengl_booleans, secondary_opengl_booleans, old_secondary_opengl_booleans);
							}

							if ((opengl_booleans & OPENGL_BOOLEAN_ARBITRARY_QUAD) == 0)
							{
								if (opengl_booleans & OPENGL_BOOLEAN_ROTATE_CLOCKWISE)
								{
									const float quad_x[4] = { right, left, left, right };
									const float quad_y[4] = { up, up, down, down };
									const float quad_u0[4] = { u1, u1, u2, u2 };
									const float quad_v0[4] = { v1, v2, v2, v1 };
									const float quad_u1[4] = { secondary_u1, secondary_u1, secondary_u2, secondary_u2 };
									const float quad_v1[4] = { secondary_v1, secondary_v2, secondary_v2, secondary_v1 };
									PLATFORM_RENDERER_draw_bound_multitextured_quad_array(quad_x, quad_y, quad_u0, quad_v0, quad_u1, quad_v1);
								}
								else
								{
									const float quad_x[4] = { left, left, right, right };
									const float quad_y[4] = { up, down, down, up };
									const float quad_u0[4] = { u1, u1, u2, u2 };
									const float quad_v0[4] = { v1, v2, v2, v1 };
									const float quad_u1[4] = { secondary_u1, secondary_u1, secondary_u2, secondary_u2 };
									const float quad_v1[4] = { secondary_v1, secondary_v2, secondary_v2, secondary_v1 };
									PLATFORM_RENDERER_draw_bound_multitextured_quad_array(quad_x, quad_y, quad_u0, quad_v0, quad_u1, quad_v1);
								}
							}
							else if (opengl_booleans & OPENGL_BOOLEAN_ARBITRARY_QUAD)
							{
								aq_pointer = &arbitrary_quads[current_entity];

								if (opengl_booleans & OPENGL_BOOLEAN_ROTATE_CLOCKWISE)
								{
									const float quad_x[4] = { aq_pointer->x[1], aq_pointer->x[0], aq_pointer->x[2], aq_pointer->x[3] };
									const float quad_y[4] = { aq_pointer->y[1], aq_pointer->y[0], aq_pointer->y[2], aq_pointer->y[3] };
									const float quad_u0[4] = { u1, u1, u2, u2 };
									const float quad_v0[4] = { v1, v2, v2, v1 };
									const float quad_u1[4] = { secondary_u1, secondary_u1, secondary_u2, secondary_u2 };
									const float quad_v1[4] = { secondary_v1, secondary_v2, secondary_v2, secondary_v1 };
									PLATFORM_RENDERER_draw_bound_multitextured_quad_array(quad_x, quad_y, quad_u0, quad_v0, quad_u1, quad_v1);
								}
								else
								{
									const float quad_x[4] = { aq_pointer->x[0], aq_pointer->x[2], aq_pointer->x[3], aq_pointer->x[1] };
									const float quad_y[4] = { aq_pointer->y[0], aq_pointer->y[2], aq_pointer->y[3], aq_pointer->y[1] };
									const float quad_u0[4] = { u1, u1, u2, u2 };
									const float quad_v0[4] = { v1, v1, v2, v1 };
									const float quad_u1[4] = { secondary_u1, secondary_u1, secondary_u2, secondary_u2 };
									const float quad_v1[4] = { secondary_v1, secondary_v1, secondary_v2, secondary_v1 };
									PLATFORM_RENDERER_draw_bound_multitextured_quad_array(quad_x, quad_y, quad_u0, quad_v0, quad_u1, quad_v1);
								}
							}
							else if (opengl_booleans & OPENGL_BOOLEAN_ARBITRARY_PERSPECTIVE_QUAD)
							{


							}
						}
						else
						{
							PLATFORM_RENDERER_set_active_texture_unit0();
							OUTPUT_apply_texture_parameters_from_flags(opengl_booleans, old_opengl_booleans);

							if (opengl_booleans & OPENGL_BOOLEAN_INDIVIDUAL_VERTEX_COLOUR_ALPHA)
							{
								avc_pointer = &arbitrary_vertex_colours[current_entity];

								if ((opengl_booleans & (OPENGL_BOOLEAN_ARBITRARY_QUAD|OPENGL_BOOLEAN_ARBITRARY_PERSPECTIVE_QUAD)) == 0)
								{
									if (opengl_booleans & OPENGL_BOOLEAN_ROTATE_CLOCKWISE)
									{
										const float quad_x[4] = { right, left, left, right };
										const float quad_y[4] = { up, up, down, down };
										const float quad_u[4] = { u1, u1, u2, u2 };
										const float quad_v[4] = { v1, v2, v2, v1 };
										const float quad_r[4] = { avc_pointer->red[1], avc_pointer->red[0], avc_pointer->red[2], avc_pointer->red[3] };
										const float quad_g[4] = { avc_pointer->green[1], avc_pointer->green[0], avc_pointer->green[2], avc_pointer->green[3] };
										const float quad_b[4] = { avc_pointer->blue[1], avc_pointer->blue[0], avc_pointer->blue[2], avc_pointer->blue[3] };
										const float quad_a[4] = { avc_pointer->alpha[1], avc_pointer->alpha[0], avc_pointer->alpha[2], avc_pointer->alpha[3] };
										PLATFORM_RENDERER_draw_bound_coloured_textured_quad_array(quad_x, quad_y, quad_u, quad_v, quad_r, quad_g, quad_b, quad_a);
									}
									else
									{
										const float quad_x[4] = { left, left, right, right };
										const float quad_y[4] = { up, down, down, up };
										const float quad_u[4] = { u1, u1, u2, u2 };
										const float quad_v[4] = { v1, v2, v2, v1 };
										const float quad_r[4] = { avc_pointer->red[0], avc_pointer->red[2], avc_pointer->red[3], avc_pointer->red[1] };
										const float quad_g[4] = { avc_pointer->green[0], avc_pointer->green[2], avc_pointer->green[3], avc_pointer->green[1] };
										const float quad_b[4] = { avc_pointer->blue[0], avc_pointer->blue[2], avc_pointer->blue[3], avc_pointer->blue[1] };
										const float quad_a[4] = { avc_pointer->alpha[0], avc_pointer->alpha[2], avc_pointer->alpha[3], avc_pointer->alpha[1] };
										PLATFORM_RENDERER_draw_bound_coloured_textured_quad_array(quad_x, quad_y, quad_u, quad_v, quad_r, quad_g, quad_b, quad_a);
									}
								}
								else if (opengl_booleans & OPENGL_BOOLEAN_ARBITRARY_QUAD)
								{
									aq_pointer = &arbitrary_quads[current_entity];

									if (opengl_booleans & OPENGL_BOOLEAN_ROTATE_CLOCKWISE)
									{
										const float quad_x[4] = { aq_pointer->x[1], aq_pointer->x[0], aq_pointer->x[2], aq_pointer->x[3] };
										const float quad_y[4] = { aq_pointer->y[1], aq_pointer->y[0], aq_pointer->y[2], aq_pointer->y[3] };
										const float quad_u[4] = { u1, u1, u2, u2 };
										const float quad_v[4] = { v1, v2, v2, v1 };
										const float quad_r[4] = { avc_pointer->red[1], avc_pointer->red[0], avc_pointer->red[2], avc_pointer->red[3] };
										const float quad_g[4] = { avc_pointer->green[1], avc_pointer->green[0], avc_pointer->green[2], avc_pointer->green[3] };
										const float quad_b[4] = { avc_pointer->blue[1], avc_pointer->blue[0], avc_pointer->blue[2], avc_pointer->blue[3] };
										const float quad_a[4] = { avc_pointer->alpha[1], avc_pointer->alpha[0], avc_pointer->alpha[2], avc_pointer->alpha[3] };
										PLATFORM_RENDERER_draw_bound_coloured_textured_quad_array(quad_x, quad_y, quad_u, quad_v, quad_r, quad_g, quad_b, quad_a);
									}
									else
									{
										const float quad_x[4] = { aq_pointer->x[0], aq_pointer->x[2], aq_pointer->x[3], aq_pointer->x[1] };
										const float quad_y[4] = { aq_pointer->y[0], aq_pointer->y[2], aq_pointer->y[3], aq_pointer->y[1] };
										const float quad_u[4] = { u1, u1, u2, u2 };
										const float quad_v[4] = { v1, v2, v2, v1 };
										const float quad_r[4] = { avc_pointer->red[0], avc_pointer->red[2], avc_pointer->red[3], avc_pointer->red[1] };
										const float quad_g[4] = { avc_pointer->green[0], avc_pointer->green[2], avc_pointer->green[3], avc_pointer->green[1] };
										const float quad_b[4] = { avc_pointer->blue[0], avc_pointer->blue[2], avc_pointer->blue[3], avc_pointer->blue[1] };
										const float quad_a[4] = { avc_pointer->alpha[0], avc_pointer->alpha[2], avc_pointer->alpha[3], avc_pointer->alpha[1] };
										PLATFORM_RENDERER_draw_bound_coloured_textured_quad_array(quad_x, quad_y, quad_u, quad_v, quad_r, quad_g, quad_b, quad_a);
									}
								}
								else if (opengl_booleans & OPENGL_BOOLEAN_ARBITRARY_PERSPECTIVE_QUAD)
								{
									aq_pointer = &arbitrary_quads[current_entity];

									line_length_ratio = aq_pointer->line_lengths[0] / aq_pointer->line_lengths[2];
									const float quad_r[4] = { avc_pointer->red[0], avc_pointer->red[2], avc_pointer->red[3], avc_pointer->red[1] };
									const float quad_g[4] = { avc_pointer->green[0], avc_pointer->green[2], avc_pointer->green[3], avc_pointer->green[1] };
									const float quad_b[4] = { avc_pointer->blue[0], avc_pointer->blue[2], avc_pointer->blue[3], avc_pointer->blue[1] };
									const float quad_a[4] = { avc_pointer->alpha[0], avc_pointer->alpha[2], avc_pointer->alpha[3], avc_pointer->alpha[1] };
									PLATFORM_RENDERER_draw_bound_coloured_perspective_textured_quad(
										aq_pointer->x[0], aq_pointer->y[0],
										aq_pointer->x[2], aq_pointer->y[2],
										aq_pointer->x[3], aq_pointer->y[3],
										aq_pointer->x[1], aq_pointer->y[1],
										u1, v1, u2, v2, line_length_ratio,
										quad_r, quad_g, quad_b, quad_a);
								}
							}
							else
							{
								if ((opengl_booleans & (OPENGL_BOOLEAN_ARBITRARY_QUAD|OPENGL_BOOLEAN_ARBITRARY_PERSPECTIVE_QUAD)) == 0)
								{
									if (opengl_booleans & OPENGL_BOOLEAN_ROTATE_CLOCKWISE)
									{
										PLATFORM_RENDERER_draw_bound_textured_quad_custom(
											right, up,
											left, up,
											left, down,
											right, down,
											u1, v1, u2, v2);
									}
									else
									{
										PLATFORM_RENDERER_draw_bound_textured_quad(left, right, up, down, u1, v1, u2, v2);
									}
								}
								else if (opengl_booleans & OPENGL_BOOLEAN_ARBITRARY_QUAD)
								{
									aq_pointer = &arbitrary_quads[current_entity];

									if (opengl_booleans & OPENGL_BOOLEAN_ROTATE_CLOCKWISE)
									{
										PLATFORM_RENDERER_draw_bound_textured_quad_custom(
											aq_pointer->x[1], aq_pointer->y[1],
											aq_pointer->x[0], aq_pointer->y[0],
											aq_pointer->x[2], aq_pointer->y[2],
											aq_pointer->x[3], aq_pointer->y[3],
											u1, v1, u2, v2);
									}
									else
									{
										PLATFORM_RENDERER_draw_bound_textured_quad_custom(
											aq_pointer->x[0], aq_pointer->y[0],
											aq_pointer->x[2], aq_pointer->y[2],
											aq_pointer->x[3], aq_pointer->y[3],
											aq_pointer->x[1], aq_pointer->y[1],
											u1, v1, u2, v2);
									}
								}
								else if (opengl_booleans & OPENGL_BOOLEAN_ARBITRARY_PERSPECTIVE_QUAD)
								{
									aq_pointer = &arbitrary_quads[current_entity];

									line_length_ratio = aq_pointer->line_lengths[0] / aq_pointer->line_lengths[2];
									PLATFORM_RENDERER_draw_bound_perspective_textured_quad(
										aq_pointer->x[0], aq_pointer->y[0],
										aq_pointer->x[2], aq_pointer->y[2],
										aq_pointer->x[3], aq_pointer->y[3],
										aq_pointer->x[1], aq_pointer->y[1],
										u1, v1, u2, v2, line_length_ratio);
								}
							}
						}

						if (opengl_booleans & OPENGL_BOOLEAN_SECONDARY_ROTATE)
						{
							PLATFORM_RENDERER_rotatef(rotate_angle_2 , 0.0f, 0.0f, 1.0f);	
						}

						if (opengl_booleans & OPENGL_BOOLEAN_SCALE)
						{
							PLATFORM_RENDERER_scalef(1.0f/scale_x,1.0f/scale_y,1.0f);
						}

						if (opengl_booleans & OPENGL_BOOLEAN_ROTATE)
						{
							PLATFORM_RENDERER_rotatef(rotate_angle , 0.0f, 0.0f, 1.0f);	
						}

						if (opengl_booleans & OPENGL_BOOLEAN_SECONDARY_SCALE)
						{
							PLATFORM_RENDERER_scalef(1.0f/scale_x_2,1.0f/scale_y_2,1.0f);
						}

						PLATFORM_RENDERER_translatef(-x,y,0);
					}
					break;
					
					
				case DRAW_MODE_INVISIBLE:
					// Something was killed and recycled during the object to object collision phase and so
					// is now blank. May as well let it sliiiiiide.
					break;


				case DRAW_MODE_TILEMAP:
				{
					#ifdef RETRENGINE_DEBUG_VERSION_THE_LAST_THING_I_DID
						MAIN_debug_last_thing ("DRAW_MODE_TILEMAP...");
					#endif

					OUTPUT_apply_texture_parameters_from_flags(opengl_booleans, old_opengl_booleans);

					map_number = entity_pointer[ENT_TILEMAP_NUMBER];
					layer_number = entity_pointer[ENT_TILEMAP_LAYER];
					map_pointer = cm[map_number].map_pointer;
					map_width = cm[map_number].map_width;
					map_height = cm[map_number].map_height;
					map_pointer += layer_number * map_width * map_height;

					map_optimised = cm[map_number].optimisation_data_flag;

					if (map_optimised)
					{
						opt_pointer = cm[map_number].optimisation_data;
						opt_pointer += layer_number * map_width * map_height;
					}

					// Map pointer now points to the start of the layer of the map we're drawing.

					tileset = cm[map_number].tile_set_index;
					tilesize = ts[tileset].tilesize;
					tile_graphic = ts[tileset].tileset_image_index;

					start_bx = entity_pointer[ENT_TILEMAP_POSITION_X];
					start_by = entity_pointer[ENT_TILEMAP_POSITION_Y];

					x_under = MATH_wrap (start_bx , tilesize);
					y_under = MATH_wrap (start_by , tilesize);

					// Drawing offset of tiles within the window...

					// Bah! All this fuss below because of negative co-ordinates.

					if (start_bx >= 0)
					{
						start_bx /= tilesize;
					}
					else if (x_under != 0)
					{
						start_bx /= tilesize;
						start_bx -= 1;
					}
					else
					{
						start_bx /= tilesize;
					}

					if (start_by >= 0)
					{
						start_by /= tilesize;
					}
					else if (y_under != 0)
					{
						start_by /= tilesize;
						start_by -= 1;
					}
					else
					{
						start_by /= tilesize;
					}

					end_bx = start_bx + (window_width / tilesize) + 1;
					end_by = start_by + (window_height / tilesize) + 1;

					if (start_bx < 0)
					{
						x_under += (start_bx * tilesize);
						start_bx = 0;
					}

					if (start_by < 0)
					{
						y_under += (start_by * tilesize);
						start_by = 0;
					}

					if (end_bx > map_width)
					{
						end_bx = map_width;
					}

					if (end_by > map_height)
					{
						end_by = map_height;
					}

					tileset_drawing_start_offset_x = -x_under;
					tileset_drawing_start_offset_y = y_under;

					next_line_transform_x = (end_bx - start_bx) * -tilesize;
					next_line_transform_y = -tilesize;

					PLATFORM_RENDERER_set_window_transform(left_window_transform_x, top_window_transform_y, total_scale_x, total_scale_y);

					PLATFORM_RENDERER_translatef(tileset_drawing_start_offset_x,tileset_drawing_start_offset_y,0);
					
					// Get tile pivot data from the first tile.
					sp = &bmps[tile_graphic].sprite_list[0];

					left = -sp->pivot_x;
					right = sp->width - sp->pivot_x;
					up = sp->pivot_y;
					down = - (sp->height - sp->pivot_y);
						
					if (map_optimised)
					{
						for (by=start_by; by<end_by; by++)
						{
							map_line_pointer = &map_pointer[(map_width * by) + start_bx];

							for (bx=start_bx; bx<end_bx; bx++)
							{
								if (*map_line_pointer != 0)
								{
									sp = &bmps[tile_graphic].sprite_list[*map_line_pointer];

									u1 = sp->u1;
									u2 = sp->u2;
									v1 = sp->v1;
									v2 = sp->v2;

									PLATFORM_RENDERER_draw_bound_textured_quad(left, right, up, down, u1, v1, u2, v2);
								}
								else
								{
									opt_offset = opt_pointer + (map_line_pointer - map_pointer);
									map_line_pointer += *opt_offset;
									bx += *opt_offset;
									PLATFORM_RENDERER_translatef(tilesize * *opt_offset,0,0);
								}

								PLATFORM_RENDERER_translatef(tilesize,0,0);
								
								map_line_pointer++;
							}
							
							PLATFORM_RENDERER_translatef(next_line_transform_x - (tilesize * (bx - end_bx)),next_line_transform_y,0);
						}
					}
					else
					{
						for (by=start_by; by<end_by; by++)
						{
							map_line_pointer = &map_pointer[(map_width * by) + start_bx];

							for (bx=start_bx; bx<end_bx; bx++)
							{
								if (*map_line_pointer != 0)
								{
									sp = &bmps[tile_graphic].sprite_list[*map_line_pointer];

									u1 = sp->u1;
									u2 = sp->u2;
									v1 = sp->v1;
									v2 = sp->v2;

									PLATFORM_RENDERER_draw_bound_textured_quad(left, right, up, down, u1, v1, u2, v2);
								}

								PLATFORM_RENDERER_translatef(tilesize,0,0);
								
								map_line_pointer++;
							}

							PLATFORM_RENDERER_translatef(next_line_transform_x,next_line_transform_y,0);
						}
					}

					PLATFORM_RENDERER_set_window_transform(left_window_transform_x, top_window_transform_y, total_scale_x, total_scale_y);
					break;
				}

				case DRAW_MODE_TILEMAP_LINE:
				{
					#ifdef RETRENGINE_DEBUG_VERSION_THE_LAST_THING_I_DID
						MAIN_debug_last_thing ("DRAW_MODE_TILEMAP_LINE...");
					#endif
						
					// This is a single tiny line of tiles. The reason for this is for games where we
					// want to interleve the drawing of the tiles with the drawing of the sprites. A typical
					// example of this would be Android 2, where the walls obscure the player and other sprites.

					// Unlike tilemaps you can't manually specify where the tiles should be drawn from the map
					// but rather it is inferred from the world_y of the entity and the window_x of the entity.

					OUTPUT_apply_texture_parameters_from_flags(opengl_booleans, old_opengl_booleans);

					map_number = entity_pointer[ENT_TILEMAP_NUMBER];
					layer_number = entity_pointer[ENT_TILEMAP_LAYER];
					map_pointer = cm[map_number].map_pointer;
					map_width = cm[map_number].map_width;
					map_height = cm[map_number].map_height;
					map_pointer += layer_number * map_width * map_height;

					map_optimised = cm[map_number].optimisation_data_flag;

					if (map_optimised)
					{
						opt_pointer = cm[map_number].optimisation_data;
						opt_pointer += layer_number * map_width * map_height;
					}

					// Map pointer now points to the start of the layer of the map we're drawing.

					tileset = cm[map_number].tile_set_index;
					tilesize = ts[tileset].tilesize;
					tile_graphic = ts[tileset].tileset_image_index;

					start_bx = window_details[window_number].current_x;
					start_by = entity_pointer[ENT_WORLD_Y];

					x_under = MATH_wrap (start_bx , tilesize);
					y_under = -(start_by - window_details[window_number].current_y);

					// Drawing offset of tiles within the window...

					// Bah! All this fuss below because of negative co-ordinates.

					if (start_bx >= 0)
					{
						start_bx /= tilesize;
					}
					else if (x_under != 0)
					{
						start_bx /= tilesize;
						start_bx -= 1;
					}
					else
					{
						start_bx /= tilesize;
					}

					if (start_by >= 0)
					{
						start_by /= tilesize;
					}
					else if (y_under != 0)
					{
						start_by /= tilesize;
						start_by -= 1;
					}
					else
					{
						start_by /= tilesize;
					}

					end_bx = start_bx + (window_width / tilesize) + 1;

					if (start_bx < 0)
					{
						x_under += (start_bx * tilesize);
						start_bx = 0;
					}

					if (start_by < 0)
					{
						y_under += (start_by * tilesize);
						start_by = 0;
					}

					if (start_by > map_height)
					{
						start_by = map_height;
					}

					if (end_bx > map_width)
					{
						end_bx = map_width;
					}

					tileset_drawing_start_offset_x = -x_under;
					tileset_drawing_start_offset_y = y_under;

					PLATFORM_RENDERER_set_window_transform(left_window_transform_x, top_window_transform_y, total_scale_x, total_scale_y);

					PLATFORM_RENDERER_translatef(tileset_drawing_start_offset_x,tileset_drawing_start_offset_y,0);
					
					// Get tile pivot data from the first tile.
					sp = &bmps[tile_graphic].sprite_list[0];

					left = -sp->pivot_x;
					right = sp->width - sp->pivot_x;
					up = sp->pivot_y;
					down = - (sp->height - sp->pivot_y);
						
					if (map_optimised)
					{
						map_line_pointer = &map_pointer[(map_width * start_by) + start_bx];

						for (bx=start_bx; bx<end_bx; bx++)
						{
							if (*map_line_pointer != 0)
							{
								sp = &bmps[tile_graphic].sprite_list[*map_line_pointer];

								u1 = sp->u1;
								u2 = sp->u2;
								v1 = sp->v1;
								v2 = sp->v2;

								PLATFORM_RENDERER_draw_bound_textured_quad(left, right, up, down, u1, v1, u2, v2);
							}
							else
							{
								opt_offset = opt_pointer + (map_line_pointer - map_pointer);
								map_line_pointer += *opt_offset;
								bx += *opt_offset;
								PLATFORM_RENDERER_translatef(tilesize * *opt_offset,0,0);
							}

							PLATFORM_RENDERER_translatef(tilesize,0,0);
							
							map_line_pointer++;
						}
					}
					else
					{
						map_line_pointer = &map_pointer[(map_width * start_by) + start_bx];

						for (bx=start_bx; bx<end_bx; bx++)
						{
							if (*map_line_pointer != 0)
							{
								sp = &bmps[tile_graphic].sprite_list[*map_line_pointer];

								u1 = sp->u1;
								u2 = sp->u2;
								v1 = sp->v1;
								v2 = sp->v2;

								PLATFORM_RENDERER_draw_bound_textured_quad(left, right, up, down, u1, v1, u2, v2);
							}

							PLATFORM_RENDERER_translatef(tilesize,0,0);
							
							map_line_pointer++;
						}
					}

					PLATFORM_RENDERER_set_window_transform(left_window_transform_x, top_window_transform_y, total_scale_x, total_scale_y);
					break;
				}






				case DRAW_MODE_STARFIELD:
					#ifdef RETRENGINE_DEBUG_VERSION_THE_LAST_THING_I_DID
						MAIN_debug_last_thing ("DRAW_MODE_STARFIELD...");
					#endif

					PLATFORM_RENDERER_set_window_transform(left_window_transform_x, top_window_transform_y, total_scale_x, total_scale_y);

					if (entity_pointer[ENT_DRAW_OVERRIDE] == DRAW_OVERRIDE_NONE)
					{
						x = entity_pointer[ENT_WORLD_X] - window_x;
						y = entity_pointer[ENT_WORLD_Y] - window_y;
						PLATFORM_RENDERER_translatef(x,-y,0.0);
					}

					PLATFORM_RENDERER_set_texture_enabled(false);

					OUTPUT_draw_starfield (entity_pointer[ENT_UNIQUE_ID]);

					PLATFORM_RENDERER_set_texture_enabled(true);
					PLATFORM_RENDERER_set_window_transform(left_window_transform_x, top_window_transform_y, total_scale_x, total_scale_y);
					break;

				case DRAW_MODE_STARFIELD_LINES:
					#ifdef RETRENGINE_DEBUG_VERSION_THE_LAST_THING_I_DID
						MAIN_debug_last_thing ("DRAW_MODE_STARFIELD_LINES...");
					#endif
					if (OUTPUT_env_enabled("WIZBALL_SDL2_LINE_TRACE"))
					{
						static unsigned int starfield_lines_case_counter = 0;
						starfield_lines_case_counter++;
						if ((starfield_lines_case_counter <= 200) || ((starfield_lines_case_counter % 5000) == 0))
						{
							fprintf(
								stderr,
								"[SDL2-SFCASE] n=%u entity=%d script=%d uid=%d flags=0x%x\n",
								starfield_lines_case_counter,
								current_entity,
								entity_pointer[ENT_SCRIPT_NUMBER],
								entity_pointer[ENT_UNIQUE_ID],
								opengl_booleans);
						}
					}

					PLATFORM_RENDERER_set_window_transform(left_window_transform_x, top_window_transform_y, total_scale_x, total_scale_y);

					if (entity_pointer[ENT_DRAW_OVERRIDE] == DRAW_OVERRIDE_NONE)
					{
						x = entity_pointer[ENT_WORLD_X] - window_x;
						y = entity_pointer[ENT_WORLD_Y] - window_y;
						PLATFORM_RENDERER_translatef(x,-y,0.0);
					}
					
					PLATFORM_RENDERER_set_texture_enabled(false);

					OUTPUT_draw_starfield_lines (entity_pointer[ENT_UNIQUE_ID]);

					PLATFORM_RENDERER_set_texture_enabled(true);
					PLATFORM_RENDERER_set_window_transform(left_window_transform_x, top_window_transform_y, total_scale_x, total_scale_y);
					break;

				case DRAW_MODE_STARFIELD_COLOUR:
					#ifdef RETRENGINE_DEBUG_VERSION_THE_LAST_THING_I_DID
						MAIN_debug_last_thing ("DRAW_MODE_STARFIELD_COLOUR...");
					#endif

					PLATFORM_RENDERER_set_window_transform(left_window_transform_x, top_window_transform_y, total_scale_x, total_scale_y);

					if (entity_pointer[ENT_DRAW_OVERRIDE] == DRAW_OVERRIDE_NONE)
					{
						x = entity_pointer[ENT_WORLD_X] - window_x;
						y = entity_pointer[ENT_WORLD_Y] - window_y;
						PLATFORM_RENDERER_translatef(x,-y,0.0);
					}
					
					PLATFORM_RENDERER_set_texture_enabled(false);

					OUTPUT_draw_starfield_colour (entity_pointer[ENT_UNIQUE_ID]);

					PLATFORM_RENDERER_set_texture_enabled(true);
					PLATFORM_RENDERER_set_window_transform(left_window_transform_x, top_window_transform_y, total_scale_x, total_scale_y);
					break;

				case DRAW_MODE_STARFIELD_COLOUR_LINES:
					#ifdef RETRENGINE_DEBUG_VERSION_THE_LAST_THING_I_DID
						MAIN_debug_last_thing ("DRAW_MODE_STARFIELD_COLOUR_LINES...");
					#endif

					PLATFORM_RENDERER_set_window_transform(left_window_transform_x, top_window_transform_y, total_scale_x, total_scale_y);

					if (entity_pointer[ENT_DRAW_OVERRIDE] == DRAW_OVERRIDE_NONE)
					{
						x = entity_pointer[ENT_WORLD_X] - window_x;
						y = entity_pointer[ENT_WORLD_Y] - window_y;
						PLATFORM_RENDERER_translatef(x,-y,0.0);
					}
					
					PLATFORM_RENDERER_set_texture_enabled(false);

					OUTPUT_draw_starfield_colour_lines (entity_pointer[ENT_UNIQUE_ID]);

					PLATFORM_RENDERER_set_texture_enabled(true);
					PLATFORM_RENDERER_set_window_transform(left_window_transform_x, top_window_transform_y, total_scale_x, total_scale_y);
					break;

				case DRAW_MODE_SOLID_RECTANGLE:
					#ifdef RETRENGINE_DEBUG_VERSION_THE_LAST_THING_I_DID
						MAIN_debug_last_thing ("DRAW_MODE_SOLID_RECTANGLE...");
					#endif

					PLATFORM_RENDERER_set_texture_enabled(false);

					OUTPUT_apply_texture_parameters_from_flags(opengl_booleans, old_opengl_booleans);

					x = entity_pointer[ENT_WORLD_X] - window_x;
					y = entity_pointer[ENT_WORLD_Y] - window_y;

					left = -entity_pointer[ENT_UPPER_WIDTH];
					right = entity_pointer[ENT_LOWER_WIDTH];
					up = entity_pointer[ENT_UPPER_HEIGHT];
					down = -entity_pointer[ENT_LOWER_HEIGHT];

					PLATFORM_RENDERER_translatef(x,-y,0.0);

					if (opengl_booleans & OPENGL_BOOLEAN_ROTATE)
					{
						rotate_angle = float (entity_pointer[ENT_OPENGL_ANGLE]) / 100.0f;
						PLATFORM_RENDERER_rotatef(-rotate_angle , 0.0f, 0.0f, 1.0f);
					}

					if (opengl_booleans & OPENGL_BOOLEAN_SCALE)
					{
						scale_x = float (entity_pointer[ENT_OPENGL_SCALE_X]) / 10000.0f;
						scale_y = float (entity_pointer[ENT_OPENGL_SCALE_Y]) / 10000.0f;
						PLATFORM_RENDERER_scalef(scale_x,scale_y,1.0f);
					}

					PLATFORM_RENDERER_draw_bound_solid_quad(left, right, up, down);

					if (opengl_booleans & OPENGL_BOOLEAN_SCALE)
					{
						PLATFORM_RENDERER_scalef(1.0f/scale_x,1.0f/scale_y,1.0f);
					}

					if (opengl_booleans & OPENGL_BOOLEAN_ROTATE)
					{
						PLATFORM_RENDERER_rotatef(rotate_angle , 0.0f, 0.0f, 1.0f);	
					}

					PLATFORM_RENDERER_translatef(-x,y,0);

					PLATFORM_RENDERER_set_texture_enabled(true);
					break;

					default:
					{
						static bool draw_mode_trace_checked_env = false;
						static bool draw_mode_trace_enabled = false;
						if (draw_mode_trace_checked_env == false)
						{
							const char *trace_value = getenv("WIZBALL_WINDOW_QUEUE_TRACE");
							if ((trace_value != NULL) && (atoi(trace_value) != 0))
							{
								draw_mode_trace_enabled = true;
							}
							draw_mode_trace_checked_env = true;
						}
						if (draw_mode_trace_enabled)
						{
							fprintf(stderr,
								"[OUTPUT] unknown draw mode: entity=%d mode=%d script=%d window=%d draw_order=%d alive=%d\n",
								current_entity,
								draw_type,
								entity_pointer[ENT_SCRIPT_NUMBER],
								entity_pointer[ENT_WINDOW_NUMBER],
								entity_pointer[ENT_DRAW_ORDER],
								entity_pointer[ENT_ALIVE]);
							fflush(stderr);
						}
						// Arse-candles!
						break;
					}
				}

				#ifdef RETRENGINE_DEBUG_VERSION_THE_LAST_THING_I_DID
					MAIN_debug_last_thing ("Finished Switch...");
				#endif

					if (!OUTPUT_window_queue_step(window_number, z_ordering_list, &current_entity, &queue_step_counter))
					{
						current_entity = UNSET;
					}

				old_opengl_booleans = opengl_booleans;
			}

			#ifndef RETRENGINE_DEBUG_VERSION_VIEW_WORLD_COLLISION
				#ifndef RETRENGINE_DEBUG_VERSION_VIEW_OBJECT_COLLISION
					window_details[window_number].z_ordering_list_starts[z_ordering_list] = UNSET;
					window_details[window_number].z_ordering_list_ends[z_ordering_list] = UNSET;
				#endif
			#endif

		}

	}

	PLATFORM_RENDERER_finish_textured_window_draw(
		texture_combiner_available,
		old_secondary_bitmap_number != UNSET,
		secondary_colour_available,
		window_details[window_number].secondary_window_colour == true,
		game_screen_width,
		game_screen_height);

	#ifdef RETRENGINE_DEBUG_VERSION_THE_LAST_THING_I_DID
		MAIN_debug_last_thing ("Elvis has left the building...");
	#endif

	if (effect_trace_enabled && (trace_total_entities > 0))
	{
		output_sdl_effect_trace_frame_counter++;
		fprintf(
			stderr,
			"[SDL2-TRACE] frame=%u win=%d ents=%d sprite=%d mtex=%d double=%d sec_col=%d arb=%d persp=%d vcol_a=%d blend(add/mul/sub)=%d/%d/%d non_sprite(mode/script/entity/flags)=%d/%d/%d/0x%x\n",
			output_sdl_effect_trace_frame_counter,
			window_number,
			trace_total_entities,
			trace_sprite_entities,
			trace_multitexture_entities,
			trace_double_mask_entities,
			trace_secondary_colour_entities,
			trace_arbitrary_quad_entities,
			trace_arbitrary_perspective_entities,
			trace_vertex_colour_alpha_entities,
			trace_blend_add_entities,
			trace_blend_multiply_entities,
			trace_blend_subtract_entities,
			trace_first_non_sprite_mode,
			trace_first_non_sprite_script,
			trace_first_non_sprite_entity,
			trace_first_non_sprite_flags);
	}

	return total_entities_drawn;
}



void OUTPUT_store_frame_info_in_entity_collision (int *entity_pointer, int modifier)
{
	int sprite = entity_pointer[ENT_SPRITE];
	int frame = entity_pointer[ENT_BASE_FRAME];

	if ( (sprite < 0) || (frame < 0) )
	{
		OUTPUT_message("Trying to set collision with unset sprite or frame!");
	}

	int width = bmps[sprite].sprite_list[frame].width;
	int height = bmps[sprite].sprite_list[frame].height;

	int pivot_x = bmps[sprite].sprite_list[frame].pivot_x;
	int pivot_y = bmps[sprite].sprite_list[frame].pivot_y;

	entity_pointer [ENT_UPPER_WIDTH] = pivot_x + modifier;
	entity_pointer [ENT_UPPER_HEIGHT] = pivot_y + modifier;

	entity_pointer [ENT_LOWER_WIDTH] = ((width-1) - pivot_x) + modifier;
	entity_pointer [ENT_LOWER_HEIGHT] = ((height-1) - pivot_y) + modifier;
}



void OUTPUT_store_frame_info_in_entity_collision_including_scale (int *entity_pointer, int modifier)
{
	int sprite = entity_pointer[ENT_SPRITE];
	int frame = entity_pointer[ENT_BASE_FRAME];

	int width = (bmps[sprite].sprite_list[frame].width * entity_pointer[ENT_OPENGL_SCALE_X]) / 10000;
	int height = (bmps[sprite].sprite_list[frame].height * entity_pointer[ENT_OPENGL_SCALE_Y]) / 10000;

	int pivot_x = (bmps[sprite].sprite_list[frame].pivot_x * entity_pointer[ENT_OPENGL_SCALE_X]) / 10000;
	int pivot_y = (bmps[sprite].sprite_list[frame].pivot_y * entity_pointer[ENT_OPENGL_SCALE_Y]) / 10000;

	entity_pointer [ENT_UPPER_WIDTH] = pivot_x + modifier;
	entity_pointer [ENT_UPPER_HEIGHT] = pivot_y + modifier;

	entity_pointer [ENT_LOWER_WIDTH] = ((width-1) - pivot_x) + modifier;
	entity_pointer [ENT_LOWER_HEIGHT] = ((height-1) - pivot_y) + modifier;
}
