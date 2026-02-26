#include <alleggl.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#ifdef WIZBALL_USE_SDL2
#include <SDL.h>
#endif

#include "platform_renderer.h"

static char *PLATFORM_RENDERER_dup_string(const char *text)
{
	if (text == NULL)
	{
		return NULL;
	}

	size_t length = strlen(text);
	char *copy = (char *) malloc(length + 1);
	if (copy == NULL)
	{
		return NULL;
	}
	strcpy(copy, text);
	return copy;
}

static char *platform_renderer_version_text = NULL;
static char *platform_renderer_vendor_text = NULL;
static char *platform_renderer_renderer_text = NULL;
static int platform_renderer_extension_count = 0;
static char **platform_renderer_extensions = NULL;
typedef struct
{
	unsigned int gl_texture_id;
} platform_renderer_texture_entry;
static platform_renderer_texture_entry *platform_renderer_texture_entries = NULL;
static int platform_renderer_texture_count = 0;
static int platform_renderer_texture_capacity = 0;
typedef void (APIENTRYP platform_active_texture_proc_t) (GLint texture_unit);
static platform_active_texture_proc_t platform_renderer_active_texture_proc = NULL;
typedef void (APIENTRYP platform_secondary_colour_proc_t) (GLfloat red, GLfloat green, GLfloat blue);
static platform_secondary_colour_proc_t platform_renderer_secondary_colour_proc = NULL;
#ifdef WIZBALL_USE_SDL2
static SDL_Window *platform_renderer_sdl_window = NULL;
static SDL_Renderer *platform_renderer_sdl_renderer = NULL;
static SDL_Texture *platform_renderer_sdl_mirror_texture = NULL;
static unsigned char *platform_renderer_sdl_mirror_pixels = NULL;
static int platform_renderer_sdl_mirror_width = 0;
static int platform_renderer_sdl_mirror_height = 0;
static int platform_renderer_sdl_mirror_pitch = 0;
static bool platform_renderer_sdl_stub_checked_env = false;
static bool platform_renderer_sdl_stub_enabled = false;
static bool platform_renderer_sdl_stub_show_checked_env = false;
static bool platform_renderer_sdl_stub_show_enabled = false;
static bool platform_renderer_sdl_stub_accel_checked_env = false;
static bool platform_renderer_sdl_stub_accel_enabled = false;
static bool platform_renderer_sdl_stub_self_test_done = false;
static bool platform_renderer_sdl_mirror_checked_env = false;
static bool platform_renderer_sdl_mirror_enabled = false;
static bool platform_renderer_started_sdl_video = false;
static char platform_renderer_sdl_status[256] = "SDL2 stub not checked.";
#endif

#ifdef WIZBALL_USE_SDL2
static bool PLATFORM_RENDERER_env_enabled(const char *name)
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

static void PLATFORM_RENDERER_refresh_sdl_stub_env_flags(void)
{
	if (!platform_renderer_sdl_stub_checked_env)
	{
		platform_renderer_sdl_stub_enabled = PLATFORM_RENDERER_env_enabled("WIZBALL_SDL2_STUB_BOOTSTRAP");
		platform_renderer_sdl_stub_checked_env = true;
	}
	if (!platform_renderer_sdl_stub_show_checked_env)
	{
		platform_renderer_sdl_stub_show_enabled = PLATFORM_RENDERER_env_enabled("WIZBALL_SDL2_STUB_SHOW");
		platform_renderer_sdl_stub_show_checked_env = true;
	}
	if (!platform_renderer_sdl_stub_accel_checked_env)
	{
		platform_renderer_sdl_stub_accel_enabled = PLATFORM_RENDERER_env_enabled("WIZBALL_SDL2_STUB_ACCELERATED");
		platform_renderer_sdl_stub_accel_checked_env = true;
	}
	if (!platform_renderer_sdl_mirror_checked_env)
	{
		platform_renderer_sdl_mirror_enabled = PLATFORM_RENDERER_env_enabled("WIZBALL_SDL2_STUB_MIRROR");
		platform_renderer_sdl_mirror_checked_env = true;
	}
}
#endif

void PLATFORM_RENDERER_reset_texture_registry(void)
{
	free(platform_renderer_texture_entries);
	platform_renderer_texture_entries = NULL;
	platform_renderer_texture_count = 0;
	platform_renderer_texture_capacity = 0;
}

static unsigned int PLATFORM_RENDERER_register_gl_texture(unsigned int gl_texture_id)
{
	if (gl_texture_id == 0)
	{
		return 0;
	}

	if (platform_renderer_texture_count == platform_renderer_texture_capacity)
	{
		int new_capacity = (platform_renderer_texture_capacity == 0) ? 128 : (platform_renderer_texture_capacity * 2);
		platform_renderer_texture_entry *new_entries = (platform_renderer_texture_entry *) realloc(
			platform_renderer_texture_entries,
			sizeof(platform_renderer_texture_entry) * new_capacity);
		if (new_entries == NULL)
		{
			return 0;
		}

		platform_renderer_texture_entries = new_entries;
		platform_renderer_texture_capacity = new_capacity;
	}

	platform_renderer_texture_entries[platform_renderer_texture_count].gl_texture_id = gl_texture_id;
	platform_renderer_texture_count++;

	// Externally-visible texture ids become renderer-owned handles (1-based).
	return (unsigned int) platform_renderer_texture_count;
}

static unsigned int PLATFORM_RENDERER_resolve_gl_texture(unsigned int texture_handle)
{
	if ((texture_handle >= 1) && (texture_handle <= (unsigned int) platform_renderer_texture_count))
	{
		return platform_renderer_texture_entries[texture_handle - 1].gl_texture_id;
	}

	return 0;
}

void PLATFORM_RENDERER_clear_extensions(void)
{
	int i;

	for (i = 0; i < platform_renderer_extension_count; i++)
	{
		free(platform_renderer_extensions[i]);
	}
	free(platform_renderer_extensions);
	platform_renderer_extensions = NULL;
	platform_renderer_extension_count = 0;

	free(platform_renderer_version_text);
	platform_renderer_version_text = NULL;
	free(platform_renderer_vendor_text);
	platform_renderer_vendor_text = NULL;
	free(platform_renderer_renderer_text);
	platform_renderer_renderer_text = NULL;
}

const char *PLATFORM_RENDERER_get_allegro_gl_error_text(void)
{
	return allegro_gl_error;
}

void PLATFORM_RENDERER_clear_backbuffer(void)
{
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void PLATFORM_RENDERER_draw_outline_rect(int x1, int y1, int x2, int y2, int r, int g, int b, int virtual_screen_height)
{
	glLoadIdentity();
	glColor3f((float) r / 256.0f, (float) g / 256.0f, (float) b / 256.0f);
	glDisable(GL_TEXTURE_2D);
	glTranslatef((float) x1, (float) ((virtual_screen_height - 1) - y1), 0.0f);
	glBegin(GL_LINE_LOOP);
		glVertex2f(0.0f, 0.0f);
		glVertex2f(0.0f, (float) (-(y2 - y1)));
		glVertex2f((float) (x2 - x1), (float) (-(y2 - y1)));
		glVertex2f((float) (x2 - x1), 0.0f);
	glEnd();
}

void PLATFORM_RENDERER_draw_filled_rect(int x1, int y1, int x2, int y2, int r, int g, int b, int virtual_screen_height)
{
	glLoadIdentity();
	glColor3f((float) r / 256.0f, (float) g / 256.0f, (float) b / 256.0f);
	glDisable(GL_TEXTURE_2D);
	glTranslatef((float) x1, (float) ((virtual_screen_height - 1) - y1), 0.0f);
	glBegin(GL_QUADS);
		glVertex2f(0.0f, 0.0f);
		glVertex2f(0.0f, (float) (-(y2 - y1)));
		glVertex2f((float) (x2 - x1), (float) (-(y2 - y1)));
		glVertex2f((float) (x2 - x1), 0.0f);
	glEnd();
}

void PLATFORM_RENDERER_draw_line(int x1, int y1, int x2, int y2, int r, int g, int b, int virtual_screen_height)
{
	glLoadIdentity();
	glColor3f((float) r / 256.0f, (float) g / 256.0f, (float) b / 256.0f);
	glDisable(GL_TEXTURE_2D);
	glTranslatef((float) x1, (float) ((virtual_screen_height - 1) - y1), 0.0f);
	glBegin(GL_LINES);
		glVertex2f(0.0f, 0.0f);
		glVertex2f((float) (x2 - x1), (float) (-(y2 - y1)));
	glEnd();
}

void PLATFORM_RENDERER_draw_circle(int x, int y, int radius, int r, int g, int b, int virtual_screen_height, int resolution)
{
	float angle;
	float step;

	if (resolution <= 0)
	{
		return;
	}

	step = (2.0f * 3.14159265358979323846f) / (float) resolution;

	glLoadIdentity();
	glColor3f((float) r / 256.0f, (float) g / 256.0f, (float) b / 256.0f);
	glDisable(GL_TEXTURE_2D);
	glTranslatef((float) x, (float) ((virtual_screen_height - 1) - y), 0.0f);

	for (angle = 0.0f; angle < (2.0f * 3.14159265358979323846f); angle += step)
	{
		float cx1 = (float) radius * cosf(angle);
		float cy1 = (float) radius * sinf(angle);
		float cx2 = (float) radius * cosf(angle + step);
		float cy2 = (float) radius * sinf(angle + step);

		glBegin(GL_LINES);
			glVertex2f(cx1, cy1);
			glVertex2f(cx2, cy2);
		glEnd();
	}
}

void PLATFORM_RENDERER_draw_bound_solid_quad(float left, float right, float up, float down)
{
	glBegin(GL_QUADS);
		glVertex2f(left, up);
		glVertex2f(left, down);
		glVertex2f(right, down);
		glVertex2f(right, up);
	glEnd();
}

void PLATFORM_RENDERER_draw_bound_textured_quad(float left, float right, float up, float down, float u1, float v1, float u2, float v2)
{
	glBegin(GL_QUADS);
		glTexCoord2f(u1, v1);
		glVertex2f(left, up);
		glTexCoord2f(u1, v2);
		glVertex2f(left, down);
		glTexCoord2f(u2, v2);
		glVertex2f(right, down);
		glTexCoord2f(u2, v1);
		glVertex2f(right, up);
	glEnd();
}

void PLATFORM_RENDERER_draw_bound_textured_quad_custom(float x0, float y0, float x1, float y1, float x2, float y2, float x3, float y3, float u1, float v1, float u2, float v2)
{
	glBegin(GL_QUADS);
		glTexCoord2f(u1, v1);
		glVertex2f(x0, y0);
		glTexCoord2f(u1, v2);
		glVertex2f(x1, y1);
		glTexCoord2f(u2, v2);
		glVertex2f(x2, y2);
		glTexCoord2f(u2, v1);
		glVertex2f(x3, y3);
	glEnd();
}

void PLATFORM_RENDERER_draw_point(float x, float y)
{
	glBegin(GL_POINTS);
		glVertex2f(x, y);
	glEnd();
}

void PLATFORM_RENDERER_draw_coloured_point(float x, float y, float r, float g, float b)
{
	glBegin(GL_POINTS);
		glColor3f(r, g, b);
		glVertex2f(x, y);
	glEnd();
}

void PLATFORM_RENDERER_draw_line(float x1, float y1, float x2, float y2)
{
	glBegin(GL_LINES);
		glVertex2f(x1, y1);
		glVertex2f(x2, y2);
	glEnd();
}

void PLATFORM_RENDERER_draw_coloured_line(float x1, float y1, float x2, float y2, float r, float g, float b)
{
	glBegin(GL_LINES);
		glColor3f(r, g, b);
		glVertex2f(x1, y1);
		glVertex2f(x2, y2);
	glEnd();
}

void PLATFORM_RENDERER_draw_line_loop_array(const float *x, const float *y, int count)
{
	int i;

	if ((x == NULL) || (y == NULL) || (count < 2))
	{
		return;
	}

	glBegin(GL_LINE_LOOP);
	for (i = 0; i < count; i++)
	{
		glVertex2f(x[i], y[i]);
	}
	glEnd();
}

void PLATFORM_RENDERER_set_window_transform(float left_window_transform_x, float top_window_transform_y, float total_scale_x, float total_scale_y)
{
	glLoadIdentity();
	glTranslatef(left_window_transform_x, top_window_transform_y, 0.0f);
	glScalef(total_scale_x, total_scale_y, 0.0f);
}

void PLATFORM_RENDERER_set_colour3f(float r, float g, float b)
{
	glColor3f(r, g, b);
}

void PLATFORM_RENDERER_set_colour4f(float r, float g, float b, float a)
{
	glColor4f(r, g, b, a);
}

void PLATFORM_RENDERER_set_texture_enabled(bool enabled)
{
	if (enabled)
	{
		glEnable(GL_TEXTURE_2D);
	}
	else
	{
		glDisable(GL_TEXTURE_2D);
	}
}

void PLATFORM_RENDERER_set_colour_sum_enabled(bool enabled)
{
	if (enabled)
	{
		glEnable(GL_COLOR_SUM);
	}
	else
	{
		glDisable(GL_COLOR_SUM);
	}
}

void PLATFORM_RENDERER_bind_texture(unsigned int texture_handle)
{
	unsigned int resolved_texture_id = PLATFORM_RENDERER_resolve_gl_texture(texture_handle);
	glBindTexture(GL_TEXTURE_2D, (GLuint) resolved_texture_id);
}

void PLATFORM_RENDERER_set_texture_filter(bool linear)
{
	if (linear)
	{
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	}
	else
	{
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	}
}

void PLATFORM_RENDERER_set_texture_wrap_repeat(void)
{
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
}

void PLATFORM_RENDERER_apply_texture_parameters(bool filtered, bool old_filtered, bool state_changed, bool force_filter_apply)
{
	if (force_filter_apply || (state_changed && (filtered != old_filtered)))
	{
		PLATFORM_RENDERER_set_texture_filter(filtered);
	}

	PLATFORM_RENDERER_set_texture_wrap_repeat();
}

void PLATFORM_RENDERER_set_active_texture_proc(void *proc)
{
	platform_renderer_active_texture_proc = (platform_active_texture_proc_t) proc;
}

bool PLATFORM_RENDERER_is_active_texture_available(void)
{
	return platform_renderer_active_texture_proc != NULL;
}

bool PLATFORM_RENDERER_set_active_texture_unit(int texture_unit)
{
	if (platform_renderer_active_texture_proc == NULL)
	{
		return false;
	}

	platform_renderer_active_texture_proc((GLint) texture_unit);
	return true;
}

bool PLATFORM_RENDERER_set_active_texture_unit0(void)
{
	return PLATFORM_RENDERER_set_active_texture_unit(GL_TEXTURE0);
}

bool PLATFORM_RENDERER_set_active_texture_unit1(void)
{
	return PLATFORM_RENDERER_set_active_texture_unit(GL_TEXTURE1);
}

void PLATFORM_RENDERER_set_secondary_colour_proc(void *proc)
{
	platform_renderer_secondary_colour_proc = (platform_secondary_colour_proc_t) proc;
}

bool PLATFORM_RENDERER_set_secondary_colour(float r, float g, float b)
{
	if (platform_renderer_secondary_colour_proc == NULL)
	{
		return false;
	}

	platform_renderer_secondary_colour_proc((GLfloat) r, (GLfloat) g, (GLfloat) b);
	return true;
}

void PLATFORM_RENDERER_set_blend_enabled(bool enabled)
{
	if (enabled)
	{
		glEnable(GL_BLEND);
	}
	else
	{
		glDisable(GL_BLEND);
	}
}

void PLATFORM_RENDERER_set_blend_func(int src_factor, int dst_factor)
{
	glBlendFunc((GLenum) src_factor, (GLenum) dst_factor);
}

void PLATFORM_RENDERER_set_blend_mode_additive(void)
{
	PLATFORM_RENDERER_set_blend_func(GL_SRC_ALPHA, GL_ONE);
}

void PLATFORM_RENDERER_set_blend_mode_alpha(void)
{
	PLATFORM_RENDERER_set_blend_func(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

void PLATFORM_RENDERER_set_blend_mode_subtractive(void)
{
	PLATFORM_RENDERER_set_blend_func(GL_ZERO, GL_ONE_MINUS_SRC_COLOR);
}

void PLATFORM_RENDERER_set_alpha_test_enabled(bool enabled)
{
	if (enabled)
	{
		glEnable(GL_ALPHA_TEST);
	}
	else
	{
		glDisable(GL_ALPHA_TEST);
	}
}

void PLATFORM_RENDERER_set_alpha_func_greater(float threshold)
{
	glAlphaFunc(GL_GREATER, threshold);
}

void PLATFORM_RENDERER_set_scissor_rect(int x, int y, int width, int height)
{
	glScissor(x, y, width, height);
}

void PLATFORM_RENDERER_set_window_scissor(float left_window_transform_x, float bottom_window_transform_y, float scaled_width, float scaled_height, float display_scale_x, float display_scale_y, float window_scale_multiplier)
{
	float x = left_window_transform_x;
	float y = bottom_window_transform_y;
	float width = scaled_width * display_scale_x;
	float height = scaled_height * display_scale_y;

	if (scaled_width < 0.0f)
	{
		x += scaled_width;
		width = -width;
	}

	if (scaled_height < 0.0f)
	{
		y += scaled_height;
		height = -height;
	}

	PLATFORM_RENDERER_set_scissor_rect(
		(int) (x * window_scale_multiplier),
		(int) (y * window_scale_multiplier),
		(int) (width * window_scale_multiplier),
		(int) (height * window_scale_multiplier));
}

void PLATFORM_RENDERER_translatef(float x, float y, float z)
{
	glTranslatef(x, y, z);
}

void PLATFORM_RENDERER_scalef(float x, float y, float z)
{
	glScalef(x, y, z);
}

void PLATFORM_RENDERER_rotatef(float angle_degrees, float x, float y, float z)
{
	glRotatef(angle_degrees, x, y, z);
}

void PLATFORM_RENDERER_set_combiner_modulate_primary(void)
{
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE);
	glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB, GL_MODULATE);
	glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_RGB, GL_TEXTURE);
	glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE1_RGB, GL_PRIMARY_COLOR);
	glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_ALPHA, GL_MODULATE);
	glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_ALPHA, GL_TEXTURE);
	glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE1_ALPHA, GL_PRIMARY_COLOR);
}

void PLATFORM_RENDERER_set_combiner_replace_rgb_modulate_alpha_previous(void)
{
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE);
	glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB, GL_REPLACE);
	glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_RGB, GL_PREVIOUS);
	glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_ALPHA, GL_MODULATE);
	glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_ALPHA, GL_TEXTURE);
	glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE1_ALPHA, GL_PREVIOUS);
}

void PLATFORM_RENDERER_set_combiner_replace_rgb_modulate_alpha_primary(void)
{
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE);
	glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB, GL_REPLACE);
	glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_RGB, GL_PREVIOUS);
	glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_ALPHA, GL_MODULATE);
	glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_ALPHA, GL_TEXTURE);
	glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE1_ALPHA, GL_PRIMARY_COLOR);
}

void PLATFORM_RENDERER_set_combiner_modulate_rgb_replace_alpha_previous(void)
{
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE);
	glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB, GL_MODULATE);
	glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_RGB, GL_TEXTURE);
	glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE1_RGB, GL_PRIMARY_COLOR);
	glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_ALPHA, GL_REPLACE);
	glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_ALPHA, GL_PREVIOUS);
}

void PLATFORM_RENDERER_bind_secondary_texture(unsigned int texture_handle, bool enable_second_unit_texturing)
{
	(void) PLATFORM_RENDERER_set_active_texture_unit1();

	if (enable_second_unit_texturing)
	{
		PLATFORM_RENDERER_set_texture_enabled(true);
	}

	PLATFORM_RENDERER_bind_texture(texture_handle);
	(void) PLATFORM_RENDERER_set_active_texture_unit0();
}

void PLATFORM_RENDERER_disable_secondary_texture_and_restore_combiner(void)
{
	(void) PLATFORM_RENDERER_set_active_texture_unit1();
	PLATFORM_RENDERER_set_texture_enabled(false);
	(void) PLATFORM_RENDERER_set_active_texture_unit0();
	PLATFORM_RENDERER_set_combiner_modulate_primary();
}

void PLATFORM_RENDERER_prepare_multitexture_second_unit(bool double_mask_mode)
{
	if (double_mask_mode)
	{
		PLATFORM_RENDERER_set_combiner_modulate_primary();
	}
	else
	{
		PLATFORM_RENDERER_set_combiner_replace_rgb_modulate_alpha_primary();
	}

	(void) PLATFORM_RENDERER_set_active_texture_unit1();
}

void PLATFORM_RENDERER_finalize_multitexture_second_unit(bool double_mask_mode)
{
	if (double_mask_mode)
	{
		PLATFORM_RENDERER_set_combiner_replace_rgb_modulate_alpha_previous();
	}
	else
	{
		PLATFORM_RENDERER_set_combiner_modulate_rgb_replace_alpha_previous();
	}

	(void) PLATFORM_RENDERER_set_active_texture_unit0();
}

void PLATFORM_RENDERER_prepare_textured_window_draw(bool texture_combiner_available)
{
	if (PLATFORM_RENDERER_is_active_texture_available())
	{
		if (texture_combiner_available)
		{
			// Defensive frame-begin reset: ensure texture unit 1 starts disabled.
			(void) PLATFORM_RENDERER_set_active_texture_unit1();
			PLATFORM_RENDERER_set_texture_enabled(false);
		}
		(void) PLATFORM_RENDERER_set_active_texture_unit0();
	}
	if (texture_combiner_available)
	{
		PLATFORM_RENDERER_set_combiner_modulate_primary();
	}

	PLATFORM_RENDERER_set_texture_enabled(true);
}

void PLATFORM_RENDERER_finish_textured_window_draw(bool texture_combiner_available, bool had_secondary_texture, bool secondary_colour_available, bool secondary_window_colour, int game_screen_width, int game_screen_height)
{
	if (had_secondary_texture && texture_combiner_available)
	{
		// Disable secondary texture unit and restore default combiner mode.
		(void) PLATFORM_RENDERER_set_active_texture_unit1();
		PLATFORM_RENDERER_set_texture_enabled(false);
		(void) PLATFORM_RENDERER_set_active_texture_unit0();
		PLATFORM_RENDERER_set_combiner_modulate_primary();
	}
	else if (texture_combiner_available && PLATFORM_RENDERER_is_active_texture_available())
	{
		(void) PLATFORM_RENDERER_set_active_texture_unit0();
	}

	if (secondary_colour_available)
	{
		PLATFORM_RENDERER_set_secondary_colour(0.0f, 0.0f, 0.0f);
		PLATFORM_RENDERER_set_colour_sum_enabled(false);
	}

	PLATFORM_RENDERER_set_scissor_rect(0, 0, game_screen_width, game_screen_height);
	PLATFORM_RENDERER_set_blend_enabled(false);
	PLATFORM_RENDERER_set_alpha_test_enabled(false);
	PLATFORM_RENDERER_set_colour4f(1.0f, 1.0f, 1.0f, 1.0f);

	if (secondary_window_colour)
	{
		PLATFORM_RENDERER_set_colour_sum_enabled(false);
	}
}

void PLATFORM_RENDERER_draw_bound_multitextured_quad_array(const float *x, const float *y, const float *u0, const float *v0, const float *u1, const float *v1)
{
	int i;

	if ((x == NULL) || (y == NULL) || (u0 == NULL) || (v0 == NULL) || (u1 == NULL) || (v1 == NULL))
	{
		return;
	}

	glBegin(GL_QUADS);
	for (i = 0; i < 4; i++)
	{
		glMultiTexCoord2f(GL_TEXTURE0, u0[i], v0[i]);
		glMultiTexCoord2f(GL_TEXTURE1, u1[i], v1[i]);
		glVertex2f(x[i], y[i]);
	}
	glEnd();
}

void PLATFORM_RENDERER_draw_bound_coloured_textured_quad_array(const float *x, const float *y, const float *u, const float *v, const float *r, const float *g, const float *b, const float *a)
{
	int i;

	if ((x == NULL) || (y == NULL) || (u == NULL) || (v == NULL) || (r == NULL) || (g == NULL) || (b == NULL) || (a == NULL))
	{
		return;
	}

	glBegin(GL_QUADS);
	for (i = 0; i < 4; i++)
	{
		glTexCoord2f(u[i], v[i]);
		glColor4f(r[i], g[i], b[i], a[i]);
		glVertex2f(x[i], y[i]);
	}
	glEnd();
}

void PLATFORM_RENDERER_draw_bound_perspective_textured_quad(float x0, float y0, float x1, float y1, float x2, float y2, float x3, float y3, float u1, float v1, float u2, float v2, float q)
{
	glBegin(GL_QUADS);
		glTexCoord4f(u1 * q, v1 * q, 0.0f, q);
		glVertex2f(x0, y0);
		glTexCoord2f(u1, v2);
		glVertex2f(x1, y1);
		glTexCoord2f(u2, v2);
		glVertex2f(x2, y2);
		glTexCoord4f(u2 * q, v1 * q, 0.0f, q);
		glVertex2f(x3, y3);
	glEnd();
}

void PLATFORM_RENDERER_draw_bound_coloured_perspective_textured_quad(float x0, float y0, float x1, float y1, float x2, float y2, float x3, float y3, float u1, float v1, float u2, float v2, float q, const float *r, const float *g, const float *b, const float *a)
{
	if ((r == NULL) || (g == NULL) || (b == NULL) || (a == NULL))
	{
		return;
	}

	glBegin(GL_QUADS);
		glColor4f(r[0], g[0], b[0], a[0]);
		glTexCoord4f(u1 * q, v1 * q, 0.0f, q);
		glVertex2f(x0, y0);

		glColor4f(r[1], g[1], b[1], a[1]);
		glTexCoord2f(u1, v2);
		glVertex2f(x1, y1);

		glColor4f(r[2], g[2], b[2], a[2]);
		glTexCoord2f(u2, v2);
		glVertex2f(x2, y2);

		glColor4f(r[3], g[3], b[3], a[3]);
		glTexCoord4f(u2 * q, v1 * q, 0.0f, q);
		glVertex2f(x3, y3);
	glEnd();
}

void PLATFORM_RENDERER_draw_textured_quad(unsigned int texture_handle, int r, int g, int b, float screen_x, float screen_y, int virtual_screen_height, float left, float right, float up, float down, float u1, float v1, float u2, float v2, bool alpha_test)
{
	(void) virtual_screen_height;
	unsigned int resolved_texture_id = PLATFORM_RENDERER_resolve_gl_texture(texture_handle);
	glBindTexture(GL_TEXTURE_2D, (GLuint) resolved_texture_id);
	glColor3f((float) r / 255.0f, (float) g / 255.0f, (float) b / 255.0f);
	glLoadIdentity();
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glEnable(GL_TEXTURE_2D);

	if (alpha_test)
	{
		glEnable(GL_ALPHA_TEST);
		glAlphaFunc(GL_GREATER, 0.5f);
	}

	glTranslatef((float) screen_x, (float) screen_y, 0.0f);

	glBegin(GL_QUADS);
		glTexCoord2f(u1, v1);
		glVertex2f(left, up);
		glTexCoord2f(u1, v2);
		glVertex2f(left, down);
		glTexCoord2f(u2, v2);
		glVertex2f(right, down);
		glTexCoord2f(u2, v1);
		glVertex2f(right, up);
	glEnd();

	if (alpha_test)
	{
		glDisable(GL_ALPHA_TEST);
	}

	glDisable(GL_TEXTURE_2D);
}

unsigned int PLATFORM_RENDERER_create_masked_texture(BITMAP *image)
{
	unsigned int gl_texture_id = (unsigned int) allegro_gl_make_masked_texture(image);
	return PLATFORM_RENDERER_register_gl_texture(gl_texture_id);
}

void PLATFORM_RENDERER_present_frame(int width, int height)
{
#ifdef WIZBALL_USE_SDL2
	if (PLATFORM_RENDERER_is_sdl2_stub_enabled() && !PLATFORM_RENDERER_is_sdl2_stub_ready())
	{
		if (PLATFORM_RENDERER_prepare_sdl2_stub(width, height, true) && !platform_renderer_sdl_stub_self_test_done)
		{
			(void) PLATFORM_RENDERER_run_sdl2_stub_self_test();
			platform_renderer_sdl_stub_self_test_done = true;
		}
	}
	(void) PLATFORM_RENDERER_mirror_from_current_backbuffer(width, height);
#else
	(void) width;
	(void) height;
#endif
	allegro_gl_flip();
}

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

void PLATFORM_RENDERER_apply_gl_defaults(int viewport_width, int viewport_height, int virtual_width, int virtual_height)
{
	glEnable(GL_TEXTURE_2D);
	glPolygonMode(GL_FRONT, GL_FILL);
	glShadeModel(GL_SMOOTH);
	glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);
	glEnable(GL_SCISSOR_TEST);
	glLineWidth(1.0f);
	glViewport(0, 0, viewport_width, viewport_height);
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glOrtho(0, virtual_width, 0, virtual_height, 0, 255);

	glMatrixMode(GL_MODELVIEW);
}

platform_renderer_caps PLATFORM_RENDERER_build_caps(bool enable_multi_texture_effects_ideally, bool has_ext_secondary_color, bool has_ext_texture_env_combine, bool has_ext_multitexture, int opengl_major_version, int opengl_minor_version)
{
	platform_renderer_caps caps;

	caps.secondary_colour_proc = allegro_gl_get_proc_address("glSecondaryColor3fEXT");
	caps.active_texture_proc = allegro_gl_get_proc_address("glActiveTextureARB");

	caps.secondary_colour_available = has_ext_secondary_color && (caps.secondary_colour_proc != NULL);

	if (has_ext_texture_env_combine && has_ext_multitexture && (((opengl_major_version == 1) && (opengl_minor_version >= 3)) || (opengl_major_version > 1)))
	{
		caps.best_texture_combiner_available = true;
		caps.texture_combiner_available = enable_multi_texture_effects_ideally && (caps.active_texture_proc != NULL);
	}
	else
	{
		caps.best_texture_combiner_available = false;
		caps.texture_combiner_available = false;
	}

	return caps;
}

platform_renderer_caps PLATFORM_RENDERER_build_caps_for_current_context(bool enable_multi_texture_effects_ideally, int opengl_major_version, int opengl_minor_version)
{
	return PLATFORM_RENDERER_build_caps(
		enable_multi_texture_effects_ideally,
		PLATFORM_RENDERER_is_extension_supported("GL_EXT_secondary_color"),
		PLATFORM_RENDERER_is_extension_supported("GL_ARB_texture_env_combine"),
		PLATFORM_RENDERER_is_extension_supported("GL_ARB_multitexture"),
		opengl_major_version,
		opengl_minor_version);
}

bool PLATFORM_RENDERER_query_extensions(int *opengl_major_version, int *opengl_minor_version)
{
	if ((opengl_major_version == NULL) || (opengl_minor_version == NULL))
	{
		return false;
	}

	*opengl_major_version = 0;
	*opengl_minor_version = 0;

	PLATFORM_RENDERER_clear_extensions();

	const char *vendor = (const char *) glGetString(GL_VENDOR);
	const char *renderer = (const char *) glGetString(GL_RENDERER);
	const char *version = (const char *) glGetString(GL_VERSION);
	const char *extension_line = (const char *) glGetString(GL_EXTENSIONS);

	if ((vendor == NULL) || (renderer == NULL) || (version == NULL) || (extension_line == NULL))
	{
		return false;
	}

	platform_renderer_vendor_text = PLATFORM_RENDERER_dup_string(vendor);
	platform_renderer_renderer_text = PLATFORM_RENDERER_dup_string(renderer);
	platform_renderer_version_text = PLATFORM_RENDERER_dup_string(version);
	if ((platform_renderer_vendor_text == NULL) || (platform_renderer_renderer_text == NULL) || (platform_renderer_version_text == NULL))
	{
		PLATFORM_RENDERER_clear_extensions();
		return false;
	}

	(void) sscanf(platform_renderer_version_text, "%d.%d", opengl_major_version, opengl_minor_version);

	char *count_buffer = PLATFORM_RENDERER_dup_string(extension_line);
	if (count_buffer == NULL)
	{
		PLATFORM_RENDERER_clear_extensions();
		return false;
	}

	int count = 0;
	char *token = strtok(count_buffer, " ");
	while (token != NULL)
	{
		count++;
		token = strtok(NULL, " ");
	}
	free(count_buffer);

	char **list = NULL;
	if (count > 0)
	{
		list = (char **) malloc(sizeof(char *) * count);
		if (list == NULL)
		{
			PLATFORM_RENDERER_clear_extensions();
			return false;
		}
	}

	char *fill_buffer = PLATFORM_RENDERER_dup_string(extension_line);
	if (fill_buffer == NULL)
	{
		if (list != NULL)
		{
			free(list);
		}
		PLATFORM_RENDERER_clear_extensions();
		return false;
	}

	int index = 0;
	token = strtok(fill_buffer, " ");
	while ((token != NULL) && (index < count))
	{
		list[index] = PLATFORM_RENDERER_dup_string(token);
		if (list[index] == NULL)
		{
			int i;
			for (i = 0; i < index; i++)
			{
				free(list[i]);
			}
				if (list != NULL)
				{
					free(list);
				}
				free(fill_buffer);
				PLATFORM_RENDERER_clear_extensions();
				return false;
			}

		index++;
		token = strtok(NULL, " ");
	}
	free(fill_buffer);

	platform_renderer_extension_count = count;
	platform_renderer_extensions = list;
	return true;
}

bool PLATFORM_RENDERER_query_and_build_caps(bool enable_multi_texture_effects_ideally, platform_renderer_caps *out_caps)
{
	int opengl_major_version = 0;
	int opengl_minor_version = 0;

	if (out_caps == NULL)
	{
		return false;
	}

	if (!PLATFORM_RENDERER_query_extensions(&opengl_major_version, &opengl_minor_version))
	{
		return false;
	}

	*out_caps = PLATFORM_RENDERER_build_caps_for_current_context(
		enable_multi_texture_effects_ideally,
		opengl_major_version,
		opengl_minor_version);
	return true;
}

void PLATFORM_RENDERER_apply_caps(const platform_renderer_caps *caps)
{
	if (caps == NULL)
	{
		return;
	}

	PLATFORM_RENDERER_set_secondary_colour_proc(caps->secondary_colour_proc);
	PLATFORM_RENDERER_set_active_texture_proc(caps->active_texture_proc);
}

bool PLATFORM_RENDERER_is_extension_supported(const char *extension)
{
	int i;

	if (extension == NULL)
	{
		return false;
	}

	for (i = 0; i < platform_renderer_extension_count; i++)
	{
		if (strcmp(extension, platform_renderer_extensions[i]) == 0)
		{
			return true;
		}
	}

	return false;
}

const char *PLATFORM_RENDERER_get_vendor_text(void)
{
	return platform_renderer_vendor_text;
}

const char *PLATFORM_RENDERER_get_renderer_text(void)
{
	return platform_renderer_renderer_text;
}

const char *PLATFORM_RENDERER_get_version_text(void)
{
	return platform_renderer_version_text;
}

int PLATFORM_RENDERER_get_extension_count(void)
{
	return platform_renderer_extension_count;
}

const char *PLATFORM_RENDERER_get_extension_at(int index)
{
	if ((index < 0) || (index >= platform_renderer_extension_count))
	{
		return NULL;
	}

	return platform_renderer_extensions[index];
}

bool PLATFORM_RENDERER_write_extensions_to_file(const char *path)
{
	if (path == NULL)
	{
		return false;
	}

	FILE *file_pointer = fopen(path, "w");
	if (file_pointer == NULL)
	{
		return false;
	}

	const char *version_text = PLATFORM_RENDERER_get_version_text();
	if (version_text == NULL)
	{
		version_text = "UNKNOWN";
	}

	fputs(version_text, file_pointer);
	fputs("\n\n", file_pointer);

	int counter;
	for (counter = 0; counter < PLATFORM_RENDERER_get_extension_count(); counter++)
	{
		const char *ext = PLATFORM_RENDERER_get_extension_at(counter);
		if (ext != NULL)
		{
			fputs(ext, file_pointer);
		}
		fputs("\n", file_pointer);
	}

	fputs("\n", file_pointer);
	fclose(file_pointer);
	return true;
}

bool PLATFORM_RENDERER_prepare_sdl2_stub(int width, int height, bool windowed)
{
#ifdef WIZBALL_USE_SDL2
	PLATFORM_RENDERER_refresh_sdl_stub_env_flags();

	if (!platform_renderer_sdl_stub_enabled)
	{
		strcpy(platform_renderer_sdl_status, "SDL2 stub disabled (set WIZBALL_SDL2_STUB_BOOTSTRAP=1 to enable).");
		return false;
	}

	if (width <= 0)
	{
		width = 640;
	}
	if (height <= 0)
	{
		height = 480;
	}

	if (platform_renderer_sdl_window != NULL && platform_renderer_sdl_renderer != NULL)
	{
		strcpy(platform_renderer_sdl_status, "SDL2 stub already initialized.");
		return true;
	}

	Uint32 needed_flags = SDL_INIT_VIDEO | SDL_INIT_EVENTS;
	Uint32 already_flags = SDL_WasInit(needed_flags);

	if (already_flags != needed_flags)
	{
		if (SDL_InitSubSystem(needed_flags & ~already_flags) != 0)
		{
			snprintf(platform_renderer_sdl_status, sizeof(platform_renderer_sdl_status), "SDL_InitSubSystem failed: %s", SDL_GetError());
			return false;
		}
		platform_renderer_started_sdl_video = true;
	}

	Uint32 window_flags = platform_renderer_sdl_stub_show_enabled ? SDL_WINDOW_SHOWN : SDL_WINDOW_HIDDEN;
	if (windowed)
	{
		window_flags |= SDL_WINDOW_RESIZABLE;
	}

	platform_renderer_sdl_window = SDL_CreateWindow(
		"WizBall SDL2 Stub",
		SDL_WINDOWPOS_UNDEFINED,
		SDL_WINDOWPOS_UNDEFINED,
		width,
		height,
		window_flags);

	if (platform_renderer_sdl_window == NULL)
	{
		snprintf(platform_renderer_sdl_status, sizeof(platform_renderer_sdl_status), "SDL_CreateWindow failed: %s", SDL_GetError());
		if (platform_renderer_started_sdl_video)
		{
			SDL_QuitSubSystem(needed_flags & ~already_flags);
			platform_renderer_started_sdl_video = false;
		}
		return false;
	}

	if (platform_renderer_sdl_stub_accel_enabled)
	{
		platform_renderer_sdl_renderer = SDL_CreateRenderer(
			platform_renderer_sdl_window,
			-1,
			SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
	}
	else
	{
		platform_renderer_sdl_renderer = SDL_CreateRenderer(
			platform_renderer_sdl_window,
			-1,
			SDL_RENDERER_SOFTWARE);
	}

	if (platform_renderer_sdl_renderer == NULL)
	{
		snprintf(platform_renderer_sdl_status, sizeof(platform_renderer_sdl_status), "SDL_CreateRenderer failed: %s", SDL_GetError());
		SDL_DestroyWindow(platform_renderer_sdl_window);
		platform_renderer_sdl_window = NULL;
		if (platform_renderer_started_sdl_video)
		{
			SDL_QuitSubSystem(needed_flags & ~already_flags);
			platform_renderer_started_sdl_video = false;
		}
		return false;
	}

	if (platform_renderer_sdl_stub_show_enabled)
	{
		if (platform_renderer_sdl_stub_accel_enabled)
		{
			strcpy(platform_renderer_sdl_status, "SDL2 stub initialized (visible window + accelerated renderer).");
		}
		else
		{
			strcpy(platform_renderer_sdl_status, "SDL2 stub initialized (visible window + software renderer).");
		}
	}
	else
	{
		if (platform_renderer_sdl_stub_accel_enabled)
		{
			strcpy(platform_renderer_sdl_status, "SDL2 stub initialized (hidden window + accelerated renderer).");
		}
		else
		{
			strcpy(platform_renderer_sdl_status, "SDL2 stub initialized (hidden window + software renderer).");
		}
	}
	return true;
#else
	(void) width;
	(void) height;
	(void) windowed;
#endif
	return false;
}

bool PLATFORM_RENDERER_is_sdl2_stub_ready(void)
{
#ifdef WIZBALL_USE_SDL2
	return (platform_renderer_sdl_window != NULL && platform_renderer_sdl_renderer != NULL);
#else
	return false;
#endif
}

bool PLATFORM_RENDERER_is_sdl2_stub_enabled(void)
{
#ifdef WIZBALL_USE_SDL2
	PLATFORM_RENDERER_refresh_sdl_stub_env_flags();
	return platform_renderer_sdl_stub_enabled;
#else
	return false;
#endif
}

bool PLATFORM_RENDERER_run_sdl2_stub_self_test(void)
{
#ifdef WIZBALL_USE_SDL2
	if ((platform_renderer_sdl_window == NULL) || (platform_renderer_sdl_renderer == NULL))
	{
		strcpy(platform_renderer_sdl_status, "SDL2 stub self-test skipped: renderer not initialized.");
		return false;
	}

	if (SDL_SetRenderDrawColor(platform_renderer_sdl_renderer, 0, 0, 0, 255) != 0)
	{
		snprintf(platform_renderer_sdl_status, sizeof(platform_renderer_sdl_status), "SDL2 stub self-test failed at SetRenderDrawColor: %s", SDL_GetError());
		return false;
	}

	if (SDL_RenderClear(platform_renderer_sdl_renderer) != 0)
	{
		snprintf(platform_renderer_sdl_status, sizeof(platform_renderer_sdl_status), "SDL2 stub self-test failed at RenderClear: %s", SDL_GetError());
		return false;
	}

	SDL_RenderPresent(platform_renderer_sdl_renderer);
	strcpy(platform_renderer_sdl_status, "SDL2 stub self-test passed (clear/present).");
	return true;
#else
	return false;
#endif
}

bool PLATFORM_RENDERER_mirror_from_current_backbuffer(int width, int height)
{
#ifdef WIZBALL_USE_SDL2
	PLATFORM_RENDERER_refresh_sdl_stub_env_flags();

	if (!platform_renderer_sdl_mirror_enabled)
	{
		return false;
	}

	if ((platform_renderer_sdl_window == NULL) || (platform_renderer_sdl_renderer == NULL))
	{
		strcpy(platform_renderer_sdl_status, "SDL2 mirror skipped: stub renderer not initialized.");
		return false;
	}

	if ((width <= 0) || (height <= 0))
	{
		strcpy(platform_renderer_sdl_status, "SDL2 mirror skipped: invalid dimensions.");
		return false;
	}

	if ((platform_renderer_sdl_mirror_texture == NULL) || (platform_renderer_sdl_mirror_width != width) || (platform_renderer_sdl_mirror_height != height))
	{
		if (platform_renderer_sdl_mirror_texture != NULL)
		{
			SDL_DestroyTexture(platform_renderer_sdl_mirror_texture);
			platform_renderer_sdl_mirror_texture = NULL;
		}
		free(platform_renderer_sdl_mirror_pixels);
		platform_renderer_sdl_mirror_pixels = NULL;
		platform_renderer_sdl_mirror_width = 0;
		platform_renderer_sdl_mirror_height = 0;
		platform_renderer_sdl_mirror_pitch = 0;

		platform_renderer_sdl_mirror_texture = SDL_CreateTexture(
			platform_renderer_sdl_renderer,
			SDL_PIXELFORMAT_RGBA32,
			SDL_TEXTUREACCESS_STREAMING,
			width,
			height);
		if (platform_renderer_sdl_mirror_texture == NULL)
		{
			snprintf(platform_renderer_sdl_status, sizeof(platform_renderer_sdl_status), "SDL2 mirror texture create failed: %s", SDL_GetError());
			return false;
		}

		platform_renderer_sdl_mirror_pitch = width * 4;
		platform_renderer_sdl_mirror_pixels = (unsigned char *) malloc((size_t) platform_renderer_sdl_mirror_pitch * (size_t) height);
		if (platform_renderer_sdl_mirror_pixels == NULL)
		{
			strcpy(platform_renderer_sdl_status, "SDL2 mirror pixel buffer allocation failed.");
			SDL_DestroyTexture(platform_renderer_sdl_mirror_texture);
			platform_renderer_sdl_mirror_texture = NULL;
			return false;
		}

		platform_renderer_sdl_mirror_width = width;
		platform_renderer_sdl_mirror_height = height;
	}

	glPixelStorei(GL_PACK_ALIGNMENT, 1);
	glReadBuffer(GL_BACK);
	glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, platform_renderer_sdl_mirror_pixels);
	GLenum gl_error = glGetError();
	if (gl_error != GL_NO_ERROR)
	{
		snprintf(platform_renderer_sdl_status, sizeof(platform_renderer_sdl_status), "SDL2 mirror glReadPixels failed (0x%x).", (unsigned int) gl_error);
		return false;
	}

	if (SDL_UpdateTexture(platform_renderer_sdl_mirror_texture, NULL, platform_renderer_sdl_mirror_pixels, platform_renderer_sdl_mirror_pitch) != 0)
	{
		snprintf(platform_renderer_sdl_status, sizeof(platform_renderer_sdl_status), "SDL2 mirror texture update failed: %s", SDL_GetError());
		return false;
	}

	if (SDL_RenderClear(platform_renderer_sdl_renderer) != 0)
	{
		snprintf(platform_renderer_sdl_status, sizeof(platform_renderer_sdl_status), "SDL2 mirror clear failed: %s", SDL_GetError());
		return false;
	}

	if (SDL_RenderCopyEx(platform_renderer_sdl_renderer, platform_renderer_sdl_mirror_texture, NULL, NULL, 0.0, NULL, SDL_FLIP_VERTICAL) != 0)
	{
		snprintf(platform_renderer_sdl_status, sizeof(platform_renderer_sdl_status), "SDL2 mirror copy failed: %s", SDL_GetError());
		return false;
	}

	SDL_RenderPresent(platform_renderer_sdl_renderer);
	strcpy(platform_renderer_sdl_status, "SDL2 mirror active.");
	return true;
#else
	(void) width;
	(void) height;
	return false;
#endif
}

const char *PLATFORM_RENDERER_get_sdl2_stub_status(void)
{
#ifdef WIZBALL_USE_SDL2
	return platform_renderer_sdl_status;
#else
	return "SDL2 stub unavailable in non-SDL2 build.";
#endif
}

void PLATFORM_RENDERER_shutdown(void)
{
	PLATFORM_RENDERER_reset_texture_registry();
#ifdef WIZBALL_USE_SDL2
	if (platform_renderer_sdl_mirror_texture != NULL)
	{
		SDL_DestroyTexture(platform_renderer_sdl_mirror_texture);
		platform_renderer_sdl_mirror_texture = NULL;
	}
	free(platform_renderer_sdl_mirror_pixels);
	platform_renderer_sdl_mirror_pixels = NULL;
	platform_renderer_sdl_mirror_width = 0;
	platform_renderer_sdl_mirror_height = 0;
	platform_renderer_sdl_mirror_pitch = 0;
	platform_renderer_sdl_stub_self_test_done = false;

	if (platform_renderer_sdl_renderer != NULL)
	{
		SDL_DestroyRenderer(platform_renderer_sdl_renderer);
		platform_renderer_sdl_renderer = NULL;
	}
	if (platform_renderer_sdl_window != NULL)
	{
		SDL_DestroyWindow(platform_renderer_sdl_window);
		platform_renderer_sdl_window = NULL;
	}
	if (platform_renderer_started_sdl_video)
	{
		SDL_QuitSubSystem(SDL_INIT_VIDEO | SDL_INIT_EVENTS);
		platform_renderer_started_sdl_video = false;
	}
#endif

	PLATFORM_RENDERER_clear_extensions();
}
