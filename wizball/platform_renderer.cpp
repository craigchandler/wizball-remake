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
#ifdef WIZBALL_USE_SDL2
	SDL_Texture *sdl_texture;
	unsigned char *sdl_rgba_pixels;
	int width;
	int height;
#endif
} platform_renderer_texture_entry;
static platform_renderer_texture_entry *platform_renderer_texture_entries = NULL;
static int platform_renderer_texture_count = 0;
static int platform_renderer_texture_capacity = 0;
static float platform_renderer_current_colour_r = 1.0f;
static float platform_renderer_current_colour_g = 1.0f;
static float platform_renderer_current_colour_b = 1.0f;
static float platform_renderer_current_colour_a = 1.0f;
static float platform_renderer_tx_a = 1.0f;
static float platform_renderer_tx_b = 0.0f;
static float platform_renderer_tx_c = 0.0f;
static float platform_renderer_tx_d = 1.0f;
static float platform_renderer_tx_x = 0.0f;
static float platform_renderer_tx_y = 0.0f;
static unsigned int platform_renderer_last_bound_texture_handle = 0;
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
static bool platform_renderer_sdl_native_sprite_checked_env = false;
static bool platform_renderer_sdl_native_sprite_enabled = false;
static bool platform_renderer_sdl_native_primary_checked_env = false;
static bool platform_renderer_sdl_native_primary_enabled = false;
static bool platform_renderer_sdl_native_primary_strict_checked_env = false;
static bool platform_renderer_sdl_native_primary_strict_enabled = false;
static bool platform_renderer_sdl_native_primary_force_no_mirror_checked_env = false;
static bool platform_renderer_sdl_native_primary_force_no_mirror_enabled = false;
static bool platform_renderer_sdl_native_primary_auto_no_mirror_checked_env = false;
static bool platform_renderer_sdl_native_primary_auto_no_mirror_enabled = false;
static bool platform_renderer_sdl_native_primary_allow_degraded_no_mirror_checked_env = false;
static bool platform_renderer_sdl_native_primary_allow_degraded_no_mirror_enabled = false;
static bool platform_renderer_sdl_core_quads_checked_env = false;
static bool platform_renderer_sdl_core_quads_enabled = false;
static bool platform_renderer_sdl_geometry_checked_env = false;
static bool platform_renderer_sdl_geometry_enabled = false;
static bool platform_renderer_sdl_status_stderr_checked_env = false;
static bool platform_renderer_sdl_status_stderr_enabled = false;
static bool platform_renderer_sdl_status_stderr_throttle_checked_env = false;
static bool platform_renderer_sdl_status_stderr_throttle_enabled = true;
static bool platform_renderer_sdl_no_mirror_min_draws_checked_env = false;
static int platform_renderer_sdl_no_mirror_min_draws = 24;
static bool platform_renderer_sdl_no_mirror_min_textured_checked_env = false;
static int platform_renderer_sdl_no_mirror_min_textured = 16;
static bool platform_renderer_sdl_no_mirror_required_streak_checked_env = false;
static int platform_renderer_sdl_no_mirror_required_streak = 30;
static int platform_renderer_sdl_native_draw_count = 0;
static int platform_renderer_sdl_native_textured_draw_count = 0;
static int platform_renderer_sdl_geometry_fallback_miss_count = 0;
static int platform_renderer_sdl_geometry_degraded_count = 0;
enum
{
	PLATFORM_RENDERER_GEOM_SRC_NONE = 0,
	PLATFORM_RENDERER_GEOM_SRC_BOUND_QUAD = 1,
	PLATFORM_RENDERER_GEOM_SRC_BOUND_QUAD_CUSTOM = 2,
	PLATFORM_RENDERER_GEOM_SRC_MULTITEXTURE_ARRAY = 3,
	PLATFORM_RENDERER_GEOM_SRC_COLOURED_ARRAY = 4,
	PLATFORM_RENDERER_GEOM_SRC_PERSPECTIVE = 5,
	PLATFORM_RENDERER_GEOM_SRC_COLOURED_PERSPECTIVE = 6,
	PLATFORM_RENDERER_GEOM_SRC_SDL_CUSTOM = 7,
	PLATFORM_RENDERER_GEOM_SRC_COUNT = 8
};
static int platform_renderer_sdl_geometry_miss_sources[PLATFORM_RENDERER_GEOM_SRC_COUNT] = {0};
static int platform_renderer_sdl_geometry_degraded_sources[PLATFORM_RENDERER_GEOM_SRC_COUNT] = {0};
static int platform_renderer_sdl_native_coverage_streak = 0;
static int platform_renderer_sdl_no_mirror_cooldown_frames = 0;
static bool platform_renderer_sdl_no_mirror_blocked_for_session = false;
static int platform_renderer_present_width = 0;
static int platform_renderer_present_height = 0;
static bool platform_renderer_started_sdl_video = false;
static char platform_renderer_sdl_status[256] = "SDL2 stub not checked.";
static char platform_renderer_sdl_last_printed_status[512] = "";
static int platform_renderer_sdl_status_print_tick = 0;
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

static int PLATFORM_RENDERER_env_int_or_default(const char *name, int default_value, int min_value, int max_value)
{
	const char *value = getenv(name);
	char *endptr = NULL;
	long parsed = 0;

	if ((value == NULL) || (*value == '\0'))
	{
		return default_value;
	}

	parsed = strtol(value, &endptr, 10);
	if ((endptr == value) || ((endptr != NULL) && (*endptr != '\0')))
	{
		return default_value;
	}

	if ((min_value > 0) && (parsed < min_value))
	{
		parsed = min_value;
	}
	if ((max_value > 0) && (parsed > max_value))
	{
		parsed = max_value;
	}

	return (int) parsed;
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
	if (!platform_renderer_sdl_native_sprite_checked_env)
	{
		platform_renderer_sdl_native_sprite_enabled = PLATFORM_RENDERER_env_enabled("WIZBALL_SDL2_NATIVE_SPRITES");
		platform_renderer_sdl_native_sprite_checked_env = true;
	}
	if (!platform_renderer_sdl_native_primary_checked_env)
	{
		platform_renderer_sdl_native_primary_enabled = PLATFORM_RENDERER_env_enabled("WIZBALL_SDL2_NATIVE_PRIMARY");
		platform_renderer_sdl_native_primary_checked_env = true;
	}
	if (!platform_renderer_sdl_native_primary_strict_checked_env)
	{
		platform_renderer_sdl_native_primary_strict_enabled = PLATFORM_RENDERER_env_enabled("WIZBALL_SDL2_NATIVE_PRIMARY_STRICT");
		platform_renderer_sdl_native_primary_strict_checked_env = true;
	}
	if (!platform_renderer_sdl_native_primary_force_no_mirror_checked_env)
	{
		platform_renderer_sdl_native_primary_force_no_mirror_enabled = PLATFORM_RENDERER_env_enabled("WIZBALL_SDL2_NATIVE_PRIMARY_FORCE_NO_MIRROR");
		platform_renderer_sdl_native_primary_force_no_mirror_checked_env = true;
	}
	if (!platform_renderer_sdl_native_primary_auto_no_mirror_checked_env)
	{
		platform_renderer_sdl_native_primary_auto_no_mirror_enabled = PLATFORM_RENDERER_env_enabled("WIZBALL_SDL2_NATIVE_PRIMARY_AUTO_NO_MIRROR");
		platform_renderer_sdl_native_primary_auto_no_mirror_checked_env = true;
	}
	if (!platform_renderer_sdl_native_primary_allow_degraded_no_mirror_checked_env)
	{
		platform_renderer_sdl_native_primary_allow_degraded_no_mirror_enabled = PLATFORM_RENDERER_env_enabled("WIZBALL_SDL2_NATIVE_PRIMARY_ALLOW_DEGRADED_NO_MIRROR");
		platform_renderer_sdl_native_primary_allow_degraded_no_mirror_checked_env = true;
	}
	if (!platform_renderer_sdl_core_quads_checked_env)
	{
		platform_renderer_sdl_core_quads_enabled = PLATFORM_RENDERER_env_enabled("WIZBALL_SDL2_CORE_QUADS");
		platform_renderer_sdl_core_quads_checked_env = true;
	}
	if (!platform_renderer_sdl_geometry_checked_env)
	{
		const char *geometry_env = getenv("WIZBALL_SDL2_GEOMETRY");
		if (geometry_env != NULL)
		{
			platform_renderer_sdl_geometry_enabled = PLATFORM_RENDERER_env_enabled("WIZBALL_SDL2_GEOMETRY");
		}
		else
		{
			platform_renderer_sdl_geometry_enabled = platform_renderer_sdl_native_primary_enabled && platform_renderer_sdl_native_primary_strict_enabled;
		}
		platform_renderer_sdl_geometry_checked_env = true;
	}
	if (!platform_renderer_sdl_status_stderr_checked_env)
	{
		platform_renderer_sdl_status_stderr_enabled = PLATFORM_RENDERER_env_enabled("WIZBALL_SDL2_STATUS_STDERR");
		platform_renderer_sdl_status_stderr_checked_env = true;
	}
	if (!platform_renderer_sdl_status_stderr_throttle_checked_env)
	{
		// Default-on throttling so status logging cannot stall frame pacing.
		platform_renderer_sdl_status_stderr_throttle_enabled = !PLATFORM_RENDERER_env_enabled("WIZBALL_SDL2_STATUS_STDERR_UNTHROTTLED");
		platform_renderer_sdl_status_stderr_throttle_checked_env = true;
	}
	if (!platform_renderer_sdl_no_mirror_min_draws_checked_env)
	{
		platform_renderer_sdl_no_mirror_min_draws = PLATFORM_RENDERER_env_int_or_default("WIZBALL_SDL2_NATIVE_PRIMARY_MIN_DRAWS", 24, 1, 10000);
		platform_renderer_sdl_no_mirror_min_draws_checked_env = true;
	}
	if (!platform_renderer_sdl_no_mirror_min_textured_checked_env)
	{
		platform_renderer_sdl_no_mirror_min_textured = PLATFORM_RENDERER_env_int_or_default("WIZBALL_SDL2_NATIVE_PRIMARY_MIN_TEXTURED_DRAWS", 16, 1, 10000);
		platform_renderer_sdl_no_mirror_min_textured_checked_env = true;
	}
	if (!platform_renderer_sdl_no_mirror_required_streak_checked_env)
	{
		platform_renderer_sdl_no_mirror_required_streak = PLATFORM_RENDERER_env_int_or_default("WIZBALL_SDL2_NATIVE_PRIMARY_REQUIRED_STREAK", 30, 1, 1000000);
		platform_renderer_sdl_no_mirror_required_streak_checked_env = true;
	}
	if (platform_renderer_sdl_native_primary_enabled)
	{
		if (platform_renderer_sdl_native_primary_strict_enabled)
		{
			platform_renderer_sdl_native_sprite_enabled = true;
		}
		else
		{
			// Compat primary mode: keep mirror-only output until strict mode is explicitly requested.
			platform_renderer_sdl_native_sprite_enabled = false;
		}
	}
	if (platform_renderer_sdl_native_sprite_enabled)
	{
		platform_renderer_sdl_stub_enabled = true;
		if (!platform_renderer_sdl_native_primary_enabled || !platform_renderer_sdl_native_primary_strict_enabled)
		{
			// Non-strict modes keep mirror fallback while native coverage is incomplete.
			platform_renderer_sdl_mirror_enabled = true;
		}
		else if (platform_renderer_sdl_native_primary_enabled && platform_renderer_sdl_native_primary_strict_enabled)
		{
			// Strict primary still needs mirror available as a safety fallback while
			// textured native coverage is below threshold.
			platform_renderer_sdl_mirror_enabled = true;
		}
	}
	else if (platform_renderer_sdl_native_primary_enabled)
	{
		platform_renderer_sdl_stub_enabled = true;
		platform_renderer_sdl_mirror_enabled = true;
	}
}

bool PLATFORM_RENDERER_is_sdl2_native_sprite_enabled(void)
{
	PLATFORM_RENDERER_refresh_sdl_stub_env_flags();
	return platform_renderer_sdl_native_sprite_enabled;
}

bool PLATFORM_RENDERER_is_sdl2_native_primary_enabled(void)
{
	PLATFORM_RENDERER_refresh_sdl_stub_env_flags();
	return platform_renderer_sdl_native_primary_enabled;
}

static bool PLATFORM_RENDERER_is_sdl_geometry_enabled(void)
{
	PLATFORM_RENDERER_refresh_sdl_stub_env_flags();
	return platform_renderer_sdl_geometry_enabled;
}

static bool PLATFORM_RENDERER_try_sdl_geometry_textured_fallback(SDL_Texture *texture, const SDL_Vertex *vertices, int vertex_count, const int *indices, int index_count, int source_id)
{
	(void) indices;
	(void) index_count;
	if ((texture == NULL) || (vertices == NULL) || (vertex_count < 4))
	{
		return false;
	}

	float min_x = vertices[0].position.x;
	float max_x = vertices[0].position.x;
	float min_y = vertices[0].position.y;
	float max_y = vertices[0].position.y;
	float min_u = vertices[0].tex_coord.x;
	float max_u = vertices[0].tex_coord.x;
	float min_v = vertices[0].tex_coord.y;
	float max_v = vertices[0].tex_coord.y;
	int i;
	for (i = 1; i < 4; i++)
	{
		if (vertices[i].position.x < min_x) min_x = vertices[i].position.x;
		if (vertices[i].position.x > max_x) max_x = vertices[i].position.x;
		if (vertices[i].position.y < min_y) min_y = vertices[i].position.y;
		if (vertices[i].position.y > max_y) max_y = vertices[i].position.y;
		if (vertices[i].tex_coord.x < min_u) min_u = vertices[i].tex_coord.x;
		if (vertices[i].tex_coord.x > max_u) max_u = vertices[i].tex_coord.x;
		if (vertices[i].tex_coord.y < min_v) min_v = vertices[i].tex_coord.y;
		if (vertices[i].tex_coord.y > max_v) max_v = vertices[i].tex_coord.y;
	}

	if (!isfinite(min_x) || !isfinite(max_x) || !isfinite(min_y) || !isfinite(max_y) || !isfinite(min_u) || !isfinite(max_u) || !isfinite(min_v) || !isfinite(max_v))
	{
		return false;
	}

	SDL_RendererFlip final_flip = SDL_FLIP_NONE;
	if (vertices[2].tex_coord.x < vertices[0].tex_coord.x)
	{
		final_flip = (SDL_RendererFlip) (final_flip | SDL_FLIP_HORIZONTAL);
	}
	if (vertices[1].tex_coord.y < vertices[0].tex_coord.y)
	{
		final_flip = (SDL_RendererFlip) (final_flip | SDL_FLIP_VERTICAL);
	}

	SDL_Rect src_rect;
	src_rect.x = (int) (min_u * 4096.0f);
	src_rect.y = (int) (min_v * 4096.0f);
	src_rect.w = (int) ((max_u - min_u) * 4096.0f);
	src_rect.h = (int) ((max_v - min_v) * 4096.0f);
	if (src_rect.w <= 0) src_rect.w = 1;
	if (src_rect.h <= 0) src_rect.h = 1;

	int tex_w = 0;
	int tex_h = 0;
	if (SDL_QueryTexture(texture, NULL, NULL, &tex_w, &tex_h) != 0 || tex_w <= 0 || tex_h <= 0)
	{
		return false;
	}
	src_rect.x = (int) (min_u * (float) tex_w);
	src_rect.y = (int) (min_v * (float) tex_h);
	src_rect.w = (int) ((max_u - min_u) * (float) tex_w);
	src_rect.h = (int) ((max_v - min_v) * (float) tex_h);
	if (src_rect.x < 0) src_rect.x = 0;
	if (src_rect.y < 0) src_rect.y = 0;
	if (src_rect.x >= tex_w) src_rect.x = tex_w - 1;
	if (src_rect.y >= tex_h) src_rect.y = tex_h - 1;
	if (src_rect.w <= 0) src_rect.w = 1;
	if (src_rect.h <= 0) src_rect.h = 1;
	if (src_rect.x + src_rect.w > tex_w) src_rect.w = tex_w - src_rect.x;
	if (src_rect.y + src_rect.h > tex_h) src_rect.h = tex_h - src_rect.y;
	if (src_rect.w <= 0) src_rect.w = 1;
	if (src_rect.h <= 0) src_rect.h = 1;

	// First try a rotated-rectangle approximation when the quad looks like
	// an affine-transformed rectangle (common sprite case).
	{
		const float p0x = vertices[0].position.x;
		const float p0y = vertices[0].position.y;
		const float p1x = vertices[1].position.x;
		const float p1y = vertices[1].position.y;
		const float p2x = vertices[2].position.x;
		const float p2y = vertices[2].position.y;
		const float p3x = vertices[3].position.x;
		const float p3y = vertices[3].position.y;
		const float exx = p3x - p0x;
		const float exy = p3y - p0y;
		const float eyx = p1x - p0x;
		const float eyy = p1y - p0y;
		const float width = sqrtf((exx * exx) + (exy * exy));
		const float height = sqrtf((eyx * eyx) + (eyy * eyy));
		const float eps = 1.5f;
		const float p21x = p2x - p1x;
		const float p21y = p2y - p1y;
		const float p23x = p2x - p3x;
		const float p23y = p2y - p3y;
		const float dot = (exx * eyx) + (exy * eyy);
		const bool edges_match =
			(fabsf((p21x - exx)) <= eps) &&
			(fabsf((p21y - exy)) <= eps) &&
			(fabsf((p23x - eyx)) <= eps) &&
			(fabsf((p23y - eyy)) <= eps);
		const bool near_rect = (width > 0.5f) && (height > 0.5f) && (fabsf(dot) < ((width * height) * 0.15f)) && edges_match;

		if (near_rect)
		{
			SDL_Rect dst_rect;
			SDL_Point center;
			const float cx = (p0x + p1x + p2x + p3x) * 0.25f;
			const float cy = (p0y + p1y + p2y + p3y) * 0.25f;
			const double angle_degrees = atan2((double) exy, (double) exx) * (180.0 / 3.14159265358979323846);

			dst_rect.w = (int) (width + 0.5f);
			dst_rect.h = (int) (height + 0.5f);
			if (dst_rect.w <= 0) dst_rect.w = 1;
			if (dst_rect.h <= 0) dst_rect.h = 1;
			dst_rect.x = (int) (cx - ((float) dst_rect.w * 0.5f));
			dst_rect.y = (int) (cy - ((float) dst_rect.h * 0.5f));

			center.x = dst_rect.w / 2;
			center.y = dst_rect.h / 2;

			(void) SDL_SetTextureColorMod(texture, vertices[0].color.r, vertices[0].color.g, vertices[0].color.b);
			(void) SDL_SetTextureAlphaMod(texture, vertices[0].color.a);
			if (SDL_RenderCopyEx(platform_renderer_sdl_renderer, texture, &src_rect, &dst_rect, angle_degrees, &center, final_flip) == 0)
			{
				platform_renderer_sdl_native_draw_count++;
				platform_renderer_sdl_native_textured_draw_count++;
				platform_renderer_sdl_geometry_degraded_count++;
				if ((source_id > 0) && (source_id < PLATFORM_RENDERER_GEOM_SRC_COUNT))
				{
					platform_renderer_sdl_geometry_degraded_sources[source_id]++;
				}
				return true;
			}
		}
	}

	SDL_Rect dst_rect;
	dst_rect.x = (int) min_x;
	dst_rect.y = (int) min_y;
	dst_rect.w = (int) (max_x - min_x);
	dst_rect.h = (int) (max_y - min_y);
	if (dst_rect.w <= 0) dst_rect.w = 1;
	if (dst_rect.h <= 0) dst_rect.h = 1;

	// Protect against pathological quads that can stall software/driver paths.
	if ((dst_rect.w > 8192) || (dst_rect.h > 8192))
	{
		return false;
	}
	if ((platform_renderer_present_width > 0) && (platform_renderer_present_height > 0))
	{
		const int margin = 2048;
		const int min_x_clip = -margin;
		const int min_y_clip = -margin;
		const int max_x_clip = platform_renderer_present_width + margin;
		const int max_y_clip = platform_renderer_present_height + margin;
		if ((dst_rect.x > max_x_clip) || (dst_rect.y > max_y_clip) || ((dst_rect.x + dst_rect.w) < min_x_clip) || ((dst_rect.y + dst_rect.h) < min_y_clip))
		{
			// Entirely far off-screen; treat as handled.
			return true;
		}
	}

	(void) SDL_SetTextureColorMod(texture, vertices[0].color.r, vertices[0].color.g, vertices[0].color.b);
	(void) SDL_SetTextureAlphaMod(texture, vertices[0].color.a);
	if (SDL_RenderCopyEx(platform_renderer_sdl_renderer, texture, &src_rect, &dst_rect, 0.0, NULL, final_flip) == 0)
	{
		platform_renderer_sdl_native_draw_count++;
		platform_renderer_sdl_native_textured_draw_count++;
		platform_renderer_sdl_geometry_degraded_count++;
		if ((source_id > 0) && (source_id < PLATFORM_RENDERER_GEOM_SRC_COUNT))
		{
			platform_renderer_sdl_geometry_degraded_sources[source_id]++;
		}
		return true;
	}

	return false;
}

static bool PLATFORM_RENDERER_try_sdl_geometry_textured(SDL_Texture *texture, const SDL_Vertex *vertices, int vertex_count, const int *indices, int index_count, int source_id)
{
	if (!PLATFORM_RENDERER_is_sdl_geometry_enabled())
	{
		if (PLATFORM_RENDERER_try_sdl_geometry_textured_fallback(texture, vertices, vertex_count, indices, index_count, source_id))
		{
			return true;
		}
		platform_renderer_sdl_geometry_fallback_miss_count++;
		if ((source_id > 0) && (source_id < PLATFORM_RENDERER_GEOM_SRC_COUNT))
		{
			platform_renderer_sdl_geometry_miss_sources[source_id]++;
		}
		platform_renderer_sdl_no_mirror_blocked_for_session = true;
		if (platform_renderer_sdl_no_mirror_cooldown_frames < 180)
		{
			platform_renderer_sdl_no_mirror_cooldown_frames = 180;
		}
		return false;
	}

	if (SDL_RenderGeometry(platform_renderer_sdl_renderer, texture, vertices, vertex_count, indices, index_count) == 0)
	{
		platform_renderer_sdl_native_draw_count++;
		platform_renderer_sdl_native_textured_draw_count++;
		return true;
	}

	if (PLATFORM_RENDERER_try_sdl_geometry_textured_fallback(texture, vertices, vertex_count, indices, index_count, source_id))
	{
		return true;
	}
	platform_renderer_sdl_geometry_fallback_miss_count++;
	if ((source_id > 0) && (source_id < PLATFORM_RENDERER_GEOM_SRC_COUNT))
	{
		platform_renderer_sdl_geometry_miss_sources[source_id]++;
	}
	platform_renderer_sdl_no_mirror_blocked_for_session = true;
	if (platform_renderer_sdl_no_mirror_cooldown_frames < 180)
	{
		platform_renderer_sdl_no_mirror_cooldown_frames = 180;
	}
	return false;
}

static void PLATFORM_RENDERER_maybe_print_sdl_status_stderr(const char *status_text)
{
	PLATFORM_RENDERER_refresh_sdl_stub_env_flags();
	if (!platform_renderer_sdl_status_stderr_enabled)
	{
		return;
	}

	if (status_text == NULL)
	{
		return;
	}

	if (strcmp(status_text, platform_renderer_sdl_last_printed_status) == 0)
	{
		return;
	}

	platform_renderer_sdl_status_print_tick++;
	if (platform_renderer_sdl_status_stderr_throttle_enabled && ((platform_renderer_sdl_status_print_tick % 30) != 0))
	{
		return;
	}

	strncpy(platform_renderer_sdl_last_printed_status, status_text, sizeof(platform_renderer_sdl_last_printed_status) - 1);
	platform_renderer_sdl_last_printed_status[sizeof(platform_renderer_sdl_last_printed_status) - 1] = '\0';
	fprintf(stderr, "[SDL2] %s\n", platform_renderer_sdl_last_printed_status);
}

static bool PLATFORM_RENDERER_build_sdl_texture_from_entry(platform_renderer_texture_entry *entry)
{
	if ((entry == NULL) || (platform_renderer_sdl_renderer == NULL))
	{
		return false;
	}

	if ((entry->sdl_texture != NULL) || (entry->sdl_rgba_pixels == NULL) || (entry->width <= 0) || (entry->height <= 0))
	{
		return entry->sdl_texture != NULL;
	}

	entry->sdl_texture = SDL_CreateTexture(
		platform_renderer_sdl_renderer,
		SDL_PIXELFORMAT_RGBA32,
		SDL_TEXTUREACCESS_STATIC,
		entry->width,
		entry->height);

	if (entry->sdl_texture == NULL)
	{
		return false;
	}

	if (SDL_UpdateTexture(entry->sdl_texture, NULL, entry->sdl_rgba_pixels, entry->width * 4) != 0)
	{
		SDL_DestroyTexture(entry->sdl_texture);
		entry->sdl_texture = NULL;
		return false;
	}

	SDL_SetTextureBlendMode(entry->sdl_texture, SDL_BLENDMODE_BLEND);
	return true;
}

static bool PLATFORM_RENDERER_build_safe_sdl_src_rect(
	const platform_renderer_texture_entry *entry,
	float u1, float v1, float u2, float v2,
	SDL_Rect *out_src_rect)
{
	if ((entry == NULL) || (out_src_rect == NULL))
	{
		return false;
	}

	if ((entry->width <= 0) || (entry->height <= 0))
	{
		return false;
	}

	float src_u_min = (u1 < u2) ? u1 : u2;
	float src_u_max = (u1 > u2) ? u1 : u2;
	float src_v_min = (v1 < v2) ? v1 : v2;
	float src_v_max = (v1 > v2) ? v1 : v2;

	int src_x = (int) (src_u_min * (float) entry->width);
	int src_y = (int) (src_v_min * (float) entry->height);
	int src_w = (int) ((src_u_max - src_u_min) * (float) entry->width);
	int src_h = (int) ((src_v_max - src_v_min) * (float) entry->height);

	if (src_w <= 0)
	{
		src_w = 1;
	}
	if (src_h <= 0)
	{
		src_h = 1;
	}

	if (src_x < 0)
	{
		src_w += src_x;
		src_x = 0;
	}
	if (src_y < 0)
	{
		src_h += src_y;
		src_y = 0;
	}

	if (src_x >= entry->width)
	{
		src_x = entry->width - 1;
		src_w = 1;
	}
	if (src_y >= entry->height)
	{
		src_y = entry->height - 1;
		src_h = 1;
	}

	if (src_x + src_w > entry->width)
	{
		src_w = entry->width - src_x;
	}
	if (src_y + src_h > entry->height)
	{
		src_h = entry->height - src_y;
	}

	if (src_w <= 0)
	{
		src_w = 1;
	}
	if (src_h <= 0)
	{
		src_h = 1;
	}

	out_src_rect->x = src_x;
	out_src_rect->y = src_y;
	out_src_rect->w = src_w;
	out_src_rect->h = src_h;
	return true;
}

static void PLATFORM_RENDERER_transform_point(float in_x, float in_y, float *out_x, float *out_y)
{
	if (out_x != NULL)
	{
		float tx = (platform_renderer_tx_a * in_x) + (platform_renderer_tx_c * in_y) + platform_renderer_tx_x;
		if (!isfinite(tx) || (fabsf(tx) > 1000000.0f))
		{
			tx = in_x;
		}
		*out_x = tx;
	}
	if (out_y != NULL)
	{
		float ty = (platform_renderer_tx_b * in_x) + (platform_renderer_tx_d * in_y) + platform_renderer_tx_y;
		if (!isfinite(ty) || (fabsf(ty) > 1000000.0f))
		{
			ty = in_y;
		}
		*out_y = ty;
	}
}

static void PLATFORM_RENDERER_set_sdl_draw_colour_from_floats(float r, float g, float b, float a)
{
	int ir = (int) (r * 255.0f);
	int ig = (int) (g * 255.0f);
	int ib = (int) (b * 255.0f);
	int ia = (int) (a * 255.0f);

	if (ir < 0) ir = 0;
	if (ir > 255) ir = 255;
	if (ig < 0) ig = 0;
	if (ig > 255) ig = 255;
	if (ib < 0) ib = 0;
	if (ib > 255) ib = 255;
	if (ia < 0) ia = 0;
	if (ia > 255) ia = 255;

	(void) SDL_SetRenderDrawBlendMode(platform_renderer_sdl_renderer, SDL_BLENDMODE_BLEND);
	(void) SDL_SetRenderDrawColor(platform_renderer_sdl_renderer, (Uint8) ir, (Uint8) ig, (Uint8) ib, (Uint8) ia);
}
#endif

#ifndef WIZBALL_USE_SDL2
bool PLATFORM_RENDERER_is_sdl2_native_sprite_enabled(void)
{
	return false;
}

bool PLATFORM_RENDERER_is_sdl2_native_primary_enabled(void)
{
	return false;
}
#endif

void PLATFORM_RENDERER_reset_texture_registry(void)
{
#ifdef WIZBALL_USE_SDL2
	int i;
	for (i = 0; i < platform_renderer_texture_count; i++)
	{
		if (platform_renderer_texture_entries[i].sdl_texture != NULL)
		{
			SDL_DestroyTexture(platform_renderer_texture_entries[i].sdl_texture);
			platform_renderer_texture_entries[i].sdl_texture = NULL;
		}
		free(platform_renderer_texture_entries[i].sdl_rgba_pixels);
		platform_renderer_texture_entries[i].sdl_rgba_pixels = NULL;
	}
#endif
	free(platform_renderer_texture_entries);
	platform_renderer_texture_entries = NULL;
	platform_renderer_texture_count = 0;
	platform_renderer_texture_capacity = 0;
	platform_renderer_last_bound_texture_handle = 0;
}

static unsigned int PLATFORM_RENDERER_register_gl_texture(unsigned int gl_texture_id, BITMAP *image)
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
#ifdef WIZBALL_USE_SDL2
	platform_renderer_texture_entries[platform_renderer_texture_count].sdl_texture = NULL;
	platform_renderer_texture_entries[platform_renderer_texture_count].sdl_rgba_pixels = NULL;
	platform_renderer_texture_entries[platform_renderer_texture_count].width = 0;
	platform_renderer_texture_entries[platform_renderer_texture_count].height = 0;

	if ((image != NULL) && (image->w > 0) && (image->h > 0))
	{
		int x;
		int y;
		int image_width = image->w;
		int image_height = image->h;
		int mask_colour = bitmap_mask_color(image);
		unsigned char *rgba_pixels = (unsigned char *) malloc((size_t) image_width * (size_t) image_height * 4u);

		if (rgba_pixels != NULL)
		{
			for (y = 0; y < image_height; y++)
			{
				for (x = 0; x < image_width; x++)
				{
					int pixel = getpixel(image, x, y);
					size_t index = ((size_t) y * (size_t) image_width + (size_t) x) * 4u;
					rgba_pixels[index + 0] = (unsigned char) getr(pixel);
					rgba_pixels[index + 1] = (unsigned char) getg(pixel);
					rgba_pixels[index + 2] = (unsigned char) getb(pixel);
					rgba_pixels[index + 3] = (pixel == mask_colour) ? 0u : 255u;
				}
			}

			platform_renderer_texture_entries[platform_renderer_texture_count].sdl_rgba_pixels = rgba_pixels;
			platform_renderer_texture_entries[platform_renderer_texture_count].width = image_width;
			platform_renderer_texture_entries[platform_renderer_texture_count].height = image_height;
		}
	}
#else
	(void) image;
#endif
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
#ifdef WIZBALL_USE_SDL2
	platform_renderer_sdl_native_draw_count = 0;
	platform_renderer_sdl_native_textured_draw_count = 0;
	platform_renderer_sdl_geometry_fallback_miss_count = 0;
	platform_renderer_sdl_geometry_degraded_count = 0;
	memset(platform_renderer_sdl_geometry_miss_sources, 0, sizeof(platform_renderer_sdl_geometry_miss_sources));
	memset(platform_renderer_sdl_geometry_degraded_sources, 0, sizeof(platform_renderer_sdl_geometry_degraded_sources));
	if (PLATFORM_RENDERER_is_sdl2_native_sprite_enabled() && PLATFORM_RENDERER_is_sdl2_stub_ready())
	{
		(void) SDL_SetRenderDrawColor(platform_renderer_sdl_renderer, 0, 0, 0, 255);
		(void) SDL_RenderClear(platform_renderer_sdl_renderer);
	}
#endif
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
#ifdef WIZBALL_USE_SDL2
	if (platform_renderer_sdl_core_quads_enabled && PLATFORM_RENDERER_is_sdl2_native_sprite_enabled() && PLATFORM_RENDERER_is_sdl2_stub_ready() && (platform_renderer_present_height > 0))
	{
		float tx0;
		float ty0;
		float tx1;
		float ty1;
		PLATFORM_RENDERER_transform_point(left, up, &tx0, &ty0);
		PLATFORM_RENDERER_transform_point(right, down, &tx1, &ty1);

		SDL_Rect rect;
		rect.x = (int) ((tx0 < tx1) ? tx0 : tx1);
		rect.y = (int) ((float) platform_renderer_present_height - ((ty0 > ty1) ? ty0 : ty1));
		rect.w = (int) fabsf(tx1 - tx0);
		rect.h = (int) fabsf(ty1 - ty0);
		if (rect.w <= 0)
		{
			rect.w = 1;
		}
		if (rect.h <= 0)
		{
			rect.h = 1;
		}

		PLATFORM_RENDERER_set_sdl_draw_colour_from_floats(
			platform_renderer_current_colour_r,
			platform_renderer_current_colour_g,
			platform_renderer_current_colour_b,
			platform_renderer_current_colour_a);
		if (SDL_RenderFillRect(platform_renderer_sdl_renderer, &rect) == 0)
		{
			platform_renderer_sdl_native_draw_count++;
		}
	}
#endif
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
#ifdef WIZBALL_USE_SDL2
	if (PLATFORM_RENDERER_is_sdl2_native_sprite_enabled() && PLATFORM_RENDERER_is_sdl2_stub_ready() && (platform_renderer_present_height > 0))
	{
#if SDL_VERSION_ATLEAST(2, 0, 18)
		if ((platform_renderer_last_bound_texture_handle >= 1) && (platform_renderer_last_bound_texture_handle <= (unsigned int) platform_renderer_texture_count))
		{
			platform_renderer_texture_entry *entry = &platform_renderer_texture_entries[platform_renderer_last_bound_texture_handle - 1];
			if (PLATFORM_RENDERER_build_sdl_texture_from_entry(entry))
			{
				float quad_x[4] = { left, left, right, right };
				float quad_y[4] = { up, down, down, up };
				float quad_u[4] = { u1, u1, u2, u2 };
				float quad_v[4] = { v1, v2, v2, v1 };
				float tx[4];
				float ty[4];
				SDL_Vertex vertices[4];
				int i;
				bool src_flip_x;
				bool src_flip_y;
				const float axis_eps = 0.01f;
				bool is_axis_aligned;

				for (i = 0; i < 4; i++)
				{
					int cr;
					int cg;
					int cb;
					int ca;

					PLATFORM_RENDERER_transform_point(quad_x[i], quad_y[i], &tx[i], &ty[i]);
					vertices[i].position.x = tx[i];
					vertices[i].position.y = (float) platform_renderer_present_height - ty[i];
					vertices[i].tex_coord.x = quad_u[i];
					vertices[i].tex_coord.y = quad_v[i];

					cr = (int) (platform_renderer_current_colour_r * 255.0f);
					cg = (int) (platform_renderer_current_colour_g * 255.0f);
					cb = (int) (platform_renderer_current_colour_b * 255.0f);
					ca = (int) (platform_renderer_current_colour_a * 255.0f);
					if (cr < 0) cr = 0; if (cr > 255) cr = 255;
					if (cg < 0) cg = 0; if (cg > 255) cg = 255;
					if (cb < 0) cb = 0; if (cb > 255) cb = 255;
					if (ca < 0) ca = 0; if (ca > 255) ca = 255;
					vertices[i].color.r = (Uint8) cr;
					vertices[i].color.g = (Uint8) cg;
					vertices[i].color.b = (Uint8) cb;
					vertices[i].color.a = (Uint8) ca;
				}

				src_flip_x = (u2 < u1);
				src_flip_y = (v2 < v1);
				is_axis_aligned =
					(fabsf(tx[0] - tx[1]) < axis_eps) &&
					(fabsf(tx[2] - tx[3]) < axis_eps) &&
					(fabsf(ty[0] - ty[3]) < axis_eps) &&
					(fabsf(ty[1] - ty[2]) < axis_eps);

				if (is_axis_aligned)
				{
					SDL_Rect src_rect;
					SDL_Rect dst_rect;
					float gl_left = (tx[0] < tx[2]) ? tx[0] : tx[2];
					float gl_right = (tx[0] > tx[2]) ? tx[0] : tx[2];
					float gl_top = (ty[0] > ty[2]) ? ty[0] : ty[2];
					float gl_bottom = (ty[0] < ty[2]) ? ty[0] : ty[2];
					SDL_RendererFlip final_flip = SDL_FLIP_NONE;

					if (!PLATFORM_RENDERER_build_safe_sdl_src_rect(entry, u1, v1, u2, v2, &src_rect))
					{
						return;
					}

					dst_rect.x = (int) gl_left;
					dst_rect.y = (int) ((float) platform_renderer_present_height - gl_top);
					dst_rect.w = (int) (gl_right - gl_left);
					dst_rect.h = (int) (gl_top - gl_bottom);
					if (dst_rect.w <= 0)
					{
						dst_rect.w = 1;
					}
					if (dst_rect.h <= 0)
					{
						dst_rect.h = 1;
					}

					if (src_flip_x)
					{
						final_flip = (SDL_RendererFlip) (final_flip | SDL_FLIP_HORIZONTAL);
					}
					if (src_flip_y)
					{
						final_flip = (SDL_RendererFlip) (final_flip | SDL_FLIP_VERTICAL);
					}

					(void) SDL_SetTextureColorMod(
						entry->sdl_texture,
						(Uint8) (platform_renderer_current_colour_r * 255.0f),
						(Uint8) (platform_renderer_current_colour_g * 255.0f),
						(Uint8) (platform_renderer_current_colour_b * 255.0f));
					(void) SDL_SetTextureAlphaMod(entry->sdl_texture, (Uint8) (platform_renderer_current_colour_a * 255.0f));

					if (SDL_RenderCopyEx(platform_renderer_sdl_renderer, entry->sdl_texture, &src_rect, &dst_rect, 0.0, NULL, final_flip) == 0)
					{
						platform_renderer_sdl_native_draw_count++;
						platform_renderer_sdl_native_textured_draw_count++;
					}
				}
				else
				{
					const float p0x = vertices[0].position.x;
					const float p0y = vertices[0].position.y;
					const float p1x = vertices[1].position.x;
					const float p1y = vertices[1].position.y;
					const float p2x = vertices[2].position.x;
					const float p2y = vertices[2].position.y;
					const float p3x = vertices[3].position.x;
					const float p3y = vertices[3].position.y;
					const float exx = p3x - p0x;
					const float exy = p3y - p0y;
					const float eyx = p1x - p0x;
					const float eyy = p1y - p0y;
					const float width = sqrtf((exx * exx) + (exy * exy));
					const float height = sqrtf((eyx * eyx) + (eyy * eyy));
					bool drawn_rotated_rect = false;

					if ((width > 0.5f) && (height > 0.5f))
					{
						SDL_Rect src_rect;
						SDL_Rect dst_rect;
						SDL_Point center;
						SDL_RendererFlip final_flip = SDL_FLIP_NONE;
						const float cx = (p0x + p1x + p2x + p3x) * 0.25f;
						const float cy = (p0y + p1y + p2y + p3y) * 0.25f;
						const double angle_degrees = atan2((double) exy, (double) exx) * (180.0 / 3.14159265358979323846);

						if (PLATFORM_RENDERER_build_safe_sdl_src_rect(entry, u1, v1, u2, v2, &src_rect))
						{
							dst_rect.w = (int) (width + 0.5f);
							dst_rect.h = (int) (height + 0.5f);
							if (dst_rect.w <= 0) dst_rect.w = 1;
							if (dst_rect.h <= 0) dst_rect.h = 1;
							dst_rect.x = (int) (cx - ((float) dst_rect.w * 0.5f));
							dst_rect.y = (int) (cy - ((float) dst_rect.h * 0.5f));

							center.x = dst_rect.w / 2;
							center.y = dst_rect.h / 2;

							if (src_flip_x)
							{
								final_flip = (SDL_RendererFlip) (final_flip | SDL_FLIP_HORIZONTAL);
							}
							if (src_flip_y)
							{
								final_flip = (SDL_RendererFlip) (final_flip | SDL_FLIP_VERTICAL);
							}

							(void) SDL_SetTextureColorMod(
								entry->sdl_texture,
								(Uint8) (platform_renderer_current_colour_r * 255.0f),
								(Uint8) (platform_renderer_current_colour_g * 255.0f),
								(Uint8) (platform_renderer_current_colour_b * 255.0f));
							(void) SDL_SetTextureAlphaMod(entry->sdl_texture, (Uint8) (platform_renderer_current_colour_a * 255.0f));

							if (SDL_RenderCopyEx(platform_renderer_sdl_renderer, entry->sdl_texture, &src_rect, &dst_rect, angle_degrees, &center, final_flip) == 0)
							{
								platform_renderer_sdl_native_draw_count++;
								platform_renderer_sdl_native_textured_draw_count++;
								drawn_rotated_rect = true;
							}
						}
					}

					if (!drawn_rotated_rect)
					{
						SDL_Rect src_rect;
						SDL_Rect dst_rect;
						SDL_RendererFlip final_flip = SDL_FLIP_NONE;
						float min_tx = tx[0];
						float max_tx = tx[0];
						float min_ty = ty[0];
						float max_ty = ty[0];
						bool finite_points = true;

						for (i = 1; i < 4; i++)
						{
							if (!isfinite(tx[i]) || !isfinite(ty[i]))
							{
								finite_points = false;
								break;
							}
							if (tx[i] < min_tx) min_tx = tx[i];
							if (tx[i] > max_tx) max_tx = tx[i];
							if (ty[i] < min_ty) min_ty = ty[i];
							if (ty[i] > max_ty) max_ty = ty[i];
						}

						if (finite_points && PLATFORM_RENDERER_build_safe_sdl_src_rect(entry, u1, v1, u2, v2, &src_rect))
						{
							dst_rect.x = (int) min_tx;
							dst_rect.y = (int) ((float) platform_renderer_present_height - max_ty);
							dst_rect.w = (int) (max_tx - min_tx);
							dst_rect.h = (int) (max_ty - min_ty);
							if (dst_rect.w <= 0) dst_rect.w = 1;
							if (dst_rect.h <= 0) dst_rect.h = 1;

							if (src_flip_x)
							{
								final_flip = (SDL_RendererFlip) (final_flip | SDL_FLIP_HORIZONTAL);
							}
							if (src_flip_y)
							{
								final_flip = (SDL_RendererFlip) (final_flip | SDL_FLIP_VERTICAL);
							}

							(void) SDL_SetTextureColorMod(
								entry->sdl_texture,
								(Uint8) (platform_renderer_current_colour_r * 255.0f),
								(Uint8) (platform_renderer_current_colour_g * 255.0f),
								(Uint8) (platform_renderer_current_colour_b * 255.0f));
							(void) SDL_SetTextureAlphaMod(entry->sdl_texture, (Uint8) (platform_renderer_current_colour_a * 255.0f));

							if (SDL_RenderCopyEx(platform_renderer_sdl_renderer, entry->sdl_texture, &src_rect, &dst_rect, 0.0, NULL, final_flip) == 0)
							{
								platform_renderer_sdl_native_draw_count++;
								platform_renderer_sdl_native_textured_draw_count++;
								drawn_rotated_rect = true;
							}
						}
					}

					if (!drawn_rotated_rect)
					{
						int indices[6] = { 0, 1, 2, 0, 2, 3 };
						(void) PLATFORM_RENDERER_try_sdl_geometry_textured(entry->sdl_texture, vertices, 4, indices, 6, PLATFORM_RENDERER_GEOM_SRC_BOUND_QUAD);
					}
				}
			}
		}
#endif
	}
#endif
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
#ifdef WIZBALL_USE_SDL2
	if (PLATFORM_RENDERER_is_sdl2_native_sprite_enabled() && PLATFORM_RENDERER_is_sdl2_stub_ready() && (platform_renderer_present_height > 0))
	{
#if SDL_VERSION_ATLEAST(2, 0, 18)
		if ((platform_renderer_last_bound_texture_handle >= 1) && (platform_renderer_last_bound_texture_handle <= (unsigned int) platform_renderer_texture_count))
		{
			platform_renderer_texture_entry *entry = &platform_renderer_texture_entries[platform_renderer_last_bound_texture_handle - 1];
			if (PLATFORM_RENDERER_build_sdl_texture_from_entry(entry))
			{
				float quad_x[4] = { x0, x1, x2, x3 };
				float quad_y[4] = { y0, y1, y2, y3 };
				float quad_u[4] = { u1, u1, u2, u2 };
				float quad_v[4] = { v1, v2, v2, v1 };
				float tx[4];
				float ty[4];
				SDL_Vertex vertices[4];
				int i;
				bool src_flip_x;
				bool src_flip_y;
				const float axis_eps = 0.01f;
				bool is_axis_aligned;

				for (i = 0; i < 4; i++)
				{
					int cr;
					int cg;
					int cb;
					int ca;

					PLATFORM_RENDERER_transform_point(quad_x[i], quad_y[i], &tx[i], &ty[i]);
					vertices[i].position.x = tx[i];
					vertices[i].position.y = (float) platform_renderer_present_height - ty[i];
					vertices[i].tex_coord.x = quad_u[i];
					vertices[i].tex_coord.y = quad_v[i];

					cr = (int) (platform_renderer_current_colour_r * 255.0f);
					cg = (int) (platform_renderer_current_colour_g * 255.0f);
					cb = (int) (platform_renderer_current_colour_b * 255.0f);
					ca = (int) (platform_renderer_current_colour_a * 255.0f);
					if (cr < 0) cr = 0; if (cr > 255) cr = 255;
					if (cg < 0) cg = 0; if (cg > 255) cg = 255;
					if (cb < 0) cb = 0; if (cb > 255) cb = 255;
					if (ca < 0) ca = 0; if (ca > 255) ca = 255;
					vertices[i].color.r = (Uint8) cr;
					vertices[i].color.g = (Uint8) cg;
					vertices[i].color.b = (Uint8) cb;
					vertices[i].color.a = (Uint8) ca;
				}

				src_flip_x = (u2 < u1);
				src_flip_y = (v2 < v1);
				is_axis_aligned =
					(fabsf(tx[0] - tx[1]) < axis_eps) &&
					(fabsf(tx[2] - tx[3]) < axis_eps) &&
					(fabsf(ty[0] - ty[3]) < axis_eps) &&
					(fabsf(ty[1] - ty[2]) < axis_eps);

				if (is_axis_aligned)
				{
					SDL_Rect src_rect;
					SDL_Rect dst_rect;
					float gl_left = (tx[0] < tx[2]) ? tx[0] : tx[2];
					float gl_right = (tx[0] > tx[2]) ? tx[0] : tx[2];
					float gl_top = (ty[0] > ty[2]) ? ty[0] : ty[2];
					float gl_bottom = (ty[0] < ty[2]) ? ty[0] : ty[2];
					SDL_RendererFlip final_flip = SDL_FLIP_NONE;

					if (!PLATFORM_RENDERER_build_safe_sdl_src_rect(entry, u1, v1, u2, v2, &src_rect))
					{
						return;
					}

					dst_rect.x = (int) gl_left;
					dst_rect.y = (int) ((float) platform_renderer_present_height - gl_top);
					dst_rect.w = (int) (gl_right - gl_left);
					dst_rect.h = (int) (gl_top - gl_bottom);
					if (dst_rect.w <= 0)
					{
						dst_rect.w = 1;
					}
					if (dst_rect.h <= 0)
					{
						dst_rect.h = 1;
					}

					if (src_flip_x)
					{
						final_flip = (SDL_RendererFlip) (final_flip | SDL_FLIP_HORIZONTAL);
					}
					if (src_flip_y)
					{
						final_flip = (SDL_RendererFlip) (final_flip | SDL_FLIP_VERTICAL);
					}

					(void) SDL_SetTextureColorMod(
						entry->sdl_texture,
						(Uint8) (platform_renderer_current_colour_r * 255.0f),
						(Uint8) (platform_renderer_current_colour_g * 255.0f),
						(Uint8) (platform_renderer_current_colour_b * 255.0f));
					(void) SDL_SetTextureAlphaMod(entry->sdl_texture, (Uint8) (platform_renderer_current_colour_a * 255.0f));

					if (SDL_RenderCopyEx(platform_renderer_sdl_renderer, entry->sdl_texture, &src_rect, &dst_rect, 0.0, NULL, final_flip) == 0)
					{
						platform_renderer_sdl_native_draw_count++;
						platform_renderer_sdl_native_textured_draw_count++;
					}
				}
				else
				{
					const float p0x = vertices[0].position.x;
					const float p0y = vertices[0].position.y;
					const float p1x = vertices[1].position.x;
					const float p1y = vertices[1].position.y;
					const float p2x = vertices[2].position.x;
					const float p2y = vertices[2].position.y;
					const float p3x = vertices[3].position.x;
					const float p3y = vertices[3].position.y;
					const float exx = p3x - p0x;
					const float exy = p3y - p0y;
					const float eyx = p1x - p0x;
					const float eyy = p1y - p0y;
					const float width = sqrtf((exx * exx) + (exy * exy));
					const float height = sqrtf((eyx * eyx) + (eyy * eyy));
					bool drawn_rotated_rect = false;

					if ((width > 0.5f) && (height > 0.5f))
					{
						SDL_Rect src_rect;
						SDL_Rect dst_rect;
						SDL_Point center;
						SDL_RendererFlip final_flip = SDL_FLIP_NONE;
						const float cx = (p0x + p1x + p2x + p3x) * 0.25f;
						const float cy = (p0y + p1y + p2y + p3y) * 0.25f;
						const double angle_degrees = atan2((double) exy, (double) exx) * (180.0 / 3.14159265358979323846);

						if (PLATFORM_RENDERER_build_safe_sdl_src_rect(entry, u1, v1, u2, v2, &src_rect))
						{
							dst_rect.w = (int) (width + 0.5f);
							dst_rect.h = (int) (height + 0.5f);
							if (dst_rect.w <= 0) dst_rect.w = 1;
							if (dst_rect.h <= 0) dst_rect.h = 1;
							dst_rect.x = (int) (cx - ((float) dst_rect.w * 0.5f));
							dst_rect.y = (int) (cy - ((float) dst_rect.h * 0.5f));

							center.x = dst_rect.w / 2;
							center.y = dst_rect.h / 2;

							if (src_flip_x)
							{
								final_flip = (SDL_RendererFlip) (final_flip | SDL_FLIP_HORIZONTAL);
							}
							if (src_flip_y)
							{
								final_flip = (SDL_RendererFlip) (final_flip | SDL_FLIP_VERTICAL);
							}

							(void) SDL_SetTextureColorMod(
								entry->sdl_texture,
								(Uint8) (platform_renderer_current_colour_r * 255.0f),
								(Uint8) (platform_renderer_current_colour_g * 255.0f),
								(Uint8) (platform_renderer_current_colour_b * 255.0f));
							(void) SDL_SetTextureAlphaMod(entry->sdl_texture, (Uint8) (platform_renderer_current_colour_a * 255.0f));

							if (SDL_RenderCopyEx(platform_renderer_sdl_renderer, entry->sdl_texture, &src_rect, &dst_rect, angle_degrees, &center, final_flip) == 0)
							{
								platform_renderer_sdl_native_draw_count++;
								platform_renderer_sdl_native_textured_draw_count++;
								drawn_rotated_rect = true;
							}
						}
					}

					if (!drawn_rotated_rect)
					{
						int indices[6] = { 0, 1, 2, 0, 2, 3 };
						(void) PLATFORM_RENDERER_try_sdl_geometry_textured(entry->sdl_texture, vertices, 4, indices, 6, PLATFORM_RENDERER_GEOM_SRC_BOUND_QUAD_CUSTOM);
					}
				}
			}
		}
#endif
	}
#endif
}

void PLATFORM_RENDERER_draw_point(float x, float y)
{
	glBegin(GL_POINTS);
		glVertex2f(x, y);
	glEnd();
#ifdef WIZBALL_USE_SDL2
	if (PLATFORM_RENDERER_is_sdl2_native_sprite_enabled() && PLATFORM_RENDERER_is_sdl2_stub_ready() && (platform_renderer_present_height > 0))
	{
		float tx;
		float ty;
		PLATFORM_RENDERER_transform_point(x, y, &tx, &ty);
		PLATFORM_RENDERER_set_sdl_draw_colour_from_floats(
			platform_renderer_current_colour_r,
			platform_renderer_current_colour_g,
			platform_renderer_current_colour_b,
			platform_renderer_current_colour_a);
		if (SDL_RenderDrawPoint(platform_renderer_sdl_renderer, (int) tx, (int) ((float) platform_renderer_present_height - ty)) == 0)
		{
			platform_renderer_sdl_native_draw_count++;
		}
	}
#endif
}

void PLATFORM_RENDERER_draw_coloured_point(float x, float y, float r, float g, float b)
{
	glBegin(GL_POINTS);
		glColor3f(r, g, b);
		glVertex2f(x, y);
	glEnd();
#ifdef WIZBALL_USE_SDL2
	if (PLATFORM_RENDERER_is_sdl2_native_sprite_enabled() && PLATFORM_RENDERER_is_sdl2_stub_ready() && (platform_renderer_present_height > 0))
	{
		float tx;
		float ty;
		PLATFORM_RENDERER_transform_point(x, y, &tx, &ty);
		PLATFORM_RENDERER_set_sdl_draw_colour_from_floats(r, g, b, platform_renderer_current_colour_a);
		if (SDL_RenderDrawPoint(platform_renderer_sdl_renderer, (int) tx, (int) ((float) platform_renderer_present_height - ty)) == 0)
		{
			platform_renderer_sdl_native_draw_count++;
		}
	}
#endif
}

void PLATFORM_RENDERER_draw_line(float x1, float y1, float x2, float y2)
{
	glBegin(GL_LINES);
		glVertex2f(x1, y1);
		glVertex2f(x2, y2);
	glEnd();
#ifdef WIZBALL_USE_SDL2
	if (PLATFORM_RENDERER_is_sdl2_native_sprite_enabled() && PLATFORM_RENDERER_is_sdl2_stub_ready() && (platform_renderer_present_height > 0))
	{
		float tx1;
		float ty1;
		float tx2;
		float ty2;
		PLATFORM_RENDERER_transform_point(x1, y1, &tx1, &ty1);
		PLATFORM_RENDERER_transform_point(x2, y2, &tx2, &ty2);
		PLATFORM_RENDERER_set_sdl_draw_colour_from_floats(
			platform_renderer_current_colour_r,
			platform_renderer_current_colour_g,
			platform_renderer_current_colour_b,
			platform_renderer_current_colour_a);
		if (SDL_RenderDrawLine(platform_renderer_sdl_renderer, (int) tx1, (int) ((float) platform_renderer_present_height - ty1), (int) tx2, (int) ((float) platform_renderer_present_height - ty2)) == 0)
		{
			platform_renderer_sdl_native_draw_count++;
		}
	}
#endif
}

void PLATFORM_RENDERER_draw_coloured_line(float x1, float y1, float x2, float y2, float r, float g, float b)
{
	glBegin(GL_LINES);
		glColor3f(r, g, b);
		glVertex2f(x1, y1);
		glVertex2f(x2, y2);
	glEnd();
#ifdef WIZBALL_USE_SDL2
	if (PLATFORM_RENDERER_is_sdl2_native_sprite_enabled() && PLATFORM_RENDERER_is_sdl2_stub_ready() && (platform_renderer_present_height > 0))
	{
		float tx1;
		float ty1;
		float tx2;
		float ty2;
		PLATFORM_RENDERER_transform_point(x1, y1, &tx1, &ty1);
		PLATFORM_RENDERER_transform_point(x2, y2, &tx2, &ty2);
		PLATFORM_RENDERER_set_sdl_draw_colour_from_floats(r, g, b, platform_renderer_current_colour_a);
		if (SDL_RenderDrawLine(platform_renderer_sdl_renderer, (int) tx1, (int) ((float) platform_renderer_present_height - ty1), (int) tx2, (int) ((float) platform_renderer_present_height - ty2)) == 0)
		{
			platform_renderer_sdl_native_draw_count++;
		}
	}
#endif
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
#ifdef WIZBALL_USE_SDL2
	if (PLATFORM_RENDERER_is_sdl2_native_sprite_enabled() && PLATFORM_RENDERER_is_sdl2_stub_ready() && (platform_renderer_present_height > 0))
	{
		PLATFORM_RENDERER_set_sdl_draw_colour_from_floats(
			platform_renderer_current_colour_r,
			platform_renderer_current_colour_g,
			platform_renderer_current_colour_b,
			platform_renderer_current_colour_a);
		for (i = 0; i < count; i++)
		{
			int j = (i + 1) % count;
			float tx1;
			float ty1;
			float tx2;
			float ty2;
			PLATFORM_RENDERER_transform_point(x[i], y[i], &tx1, &ty1);
			PLATFORM_RENDERER_transform_point(x[j], y[j], &tx2, &ty2);
			if (SDL_RenderDrawLine(platform_renderer_sdl_renderer, (int) tx1, (int) ((float) platform_renderer_present_height - ty1), (int) tx2, (int) ((float) platform_renderer_present_height - ty2)) == 0)
			{
				platform_renderer_sdl_native_draw_count++;
			}
		}
	}
#endif
}

void PLATFORM_RENDERER_set_window_transform(float left_window_transform_x, float top_window_transform_y, float total_scale_x, float total_scale_y)
{
	glLoadIdentity();
	glTranslatef(left_window_transform_x, top_window_transform_y, 0.0f);
	glScalef(total_scale_x, total_scale_y, 0.0f);
	platform_renderer_tx_a = total_scale_x;
	platform_renderer_tx_b = 0.0f;
	platform_renderer_tx_c = 0.0f;
	platform_renderer_tx_d = total_scale_y;
	platform_renderer_tx_x = left_window_transform_x;
	platform_renderer_tx_y = top_window_transform_y;
}

void PLATFORM_RENDERER_set_colour3f(float r, float g, float b)
{
	glColor3f(r, g, b);
	platform_renderer_current_colour_r = r;
	platform_renderer_current_colour_g = g;
	platform_renderer_current_colour_b = b;
	platform_renderer_current_colour_a = 1.0f;
}

void PLATFORM_RENDERER_set_colour4f(float r, float g, float b, float a)
{
	glColor4f(r, g, b, a);
	platform_renderer_current_colour_r = r;
	platform_renderer_current_colour_g = g;
	platform_renderer_current_colour_b = b;
	platform_renderer_current_colour_a = a;
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
	platform_renderer_last_bound_texture_handle = texture_handle;
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
	platform_renderer_tx_x += (platform_renderer_tx_a * x) + (platform_renderer_tx_c * y);
	platform_renderer_tx_y += (platform_renderer_tx_b * x) + (platform_renderer_tx_d * y);
}

void PLATFORM_RENDERER_scalef(float x, float y, float z)
{
	glScalef(x, y, z);
	platform_renderer_tx_a *= x;
	platform_renderer_tx_b *= x;
	platform_renderer_tx_c *= y;
	platform_renderer_tx_d *= y;
}

void PLATFORM_RENDERER_rotatef(float angle_degrees, float x, float y, float z)
{
	glRotatef(angle_degrees, x, y, z);
	(void) x;
	(void) y;
	(void) z;
	float radians = angle_degrees * (3.14159265358979323846f / 180.0f);
	float c = cosf(radians);
	float s = sinf(radians);
	float old_a = platform_renderer_tx_a;
	float old_b = platform_renderer_tx_b;
	float old_c = platform_renderer_tx_c;
	float old_d = platform_renderer_tx_d;
	platform_renderer_tx_a = (old_a * c) + (old_c * s);
	platform_renderer_tx_c = (-old_a * s) + (old_c * c);
	platform_renderer_tx_b = (old_b * c) + (old_d * s);
	platform_renderer_tx_d = (-old_b * s) + (old_d * c);
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

	// Bind unit1 texture directly so unit0 "last bound texture" tracking stays on
	// the primary layer used by SDL-native fallback draw paths.
	{
		unsigned int resolved_texture_id = PLATFORM_RENDERER_resolve_gl_texture(texture_handle);
		glBindTexture(GL_TEXTURE_2D, (GLuint) resolved_texture_id);
	}
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
#ifdef WIZBALL_USE_SDL2
	if (PLATFORM_RENDERER_is_sdl2_native_sprite_enabled() && PLATFORM_RENDERER_is_sdl2_stub_ready() && (platform_renderer_present_height > 0))
	{
#if SDL_VERSION_ATLEAST(2, 0, 18)
		if ((platform_renderer_last_bound_texture_handle >= 1) && (platform_renderer_last_bound_texture_handle <= (unsigned int) platform_renderer_texture_count))
		{
			platform_renderer_texture_entry *entry = &platform_renderer_texture_entries[platform_renderer_last_bound_texture_handle - 1];
			if (PLATFORM_RENDERER_build_sdl_texture_from_entry(entry))
			{
				SDL_Vertex vertices[4];
				float txs[4];
				float tys[4];
				bool is_axis_aligned;
				bool src_flip_x;
				bool src_flip_y;
				const float axis_eps = 0.01f;
				for (i = 0; i < 4; i++)
				{
					float tx;
					float ty;
					int cr;
					int cg;
					int cb;
					int ca;

					PLATFORM_RENDERER_transform_point(x[i], y[i], &tx, &ty);
					txs[i] = tx;
					tys[i] = ty;
					vertices[i].position.x = tx;
					vertices[i].position.y = (float) platform_renderer_present_height - ty;
					vertices[i].tex_coord.x = u0[i];
					vertices[i].tex_coord.y = v0[i];

					cr = (int) (platform_renderer_current_colour_r * 255.0f);
					cg = (int) (platform_renderer_current_colour_g * 255.0f);
					cb = (int) (platform_renderer_current_colour_b * 255.0f);
					ca = (int) (platform_renderer_current_colour_a * 255.0f);

					if (cr < 0) cr = 0;
					if (cr > 255) cr = 255;
					if (cg < 0) cg = 0;
					if (cg > 255) cg = 255;
					if (cb < 0) cb = 0;
					if (cb > 255) cb = 255;
					if (ca < 0) ca = 0;
					if (ca > 255) ca = 255;

					vertices[i].color.r = (Uint8) cr;
					vertices[i].color.g = (Uint8) cg;
					vertices[i].color.b = (Uint8) cb;
					vertices[i].color.a = (Uint8) ca;
				}

				src_flip_x = (u0[2] < u0[0]);
				src_flip_y = (v0[1] < v0[0]);
				is_axis_aligned =
					(fabsf(txs[0] - txs[1]) < axis_eps) &&
					(fabsf(txs[2] - txs[3]) < axis_eps) &&
					(fabsf(tys[0] - tys[3]) < axis_eps) &&
					(fabsf(tys[1] - tys[2]) < axis_eps);

				if (is_axis_aligned)
				{
					SDL_Rect src_rect;
					SDL_Rect dst_rect;
					float gl_left = (txs[0] < txs[2]) ? txs[0] : txs[2];
					float gl_right = (txs[0] > txs[2]) ? txs[0] : txs[2];
					float gl_top = (tys[0] > tys[2]) ? tys[0] : tys[2];
					float gl_bottom = (tys[0] < tys[2]) ? tys[0] : tys[2];
					SDL_RendererFlip final_flip = SDL_FLIP_NONE;

					if (!PLATFORM_RENDERER_build_safe_sdl_src_rect(entry, u0[0], v0[0], u0[2], v0[2], &src_rect))
					{
						return;
					}

					dst_rect.x = (int) gl_left;
					dst_rect.y = (int) ((float) platform_renderer_present_height - gl_top);
					dst_rect.w = (int) (gl_right - gl_left);
					dst_rect.h = (int) (gl_top - gl_bottom);
					if (dst_rect.w <= 0)
					{
						dst_rect.w = 1;
					}
					if (dst_rect.h <= 0)
					{
						dst_rect.h = 1;
					}

					if (src_flip_x)
					{
						final_flip = (SDL_RendererFlip) (final_flip | SDL_FLIP_HORIZONTAL);
					}
					if (src_flip_y)
					{
						final_flip = (SDL_RendererFlip) (final_flip | SDL_FLIP_VERTICAL);
					}

					(void) SDL_SetTextureColorMod(
						entry->sdl_texture,
						(Uint8) (platform_renderer_current_colour_r * 255.0f),
						(Uint8) (platform_renderer_current_colour_g * 255.0f),
						(Uint8) (platform_renderer_current_colour_b * 255.0f));
					(void) SDL_SetTextureAlphaMod(entry->sdl_texture, (Uint8) (platform_renderer_current_colour_a * 255.0f));

					if (SDL_RenderCopyEx(platform_renderer_sdl_renderer, entry->sdl_texture, &src_rect, &dst_rect, 0.0, NULL, final_flip) == 0)
					{
						platform_renderer_sdl_native_draw_count++;
						platform_renderer_sdl_native_textured_draw_count++;
					}
				}
				else
				{
					int indices[6] = { 0, 1, 2, 0, 2, 3 };
					(void) PLATFORM_RENDERER_try_sdl_geometry_textured(entry->sdl_texture, vertices, 4, indices, 6, PLATFORM_RENDERER_GEOM_SRC_MULTITEXTURE_ARRAY);
				}
			}
		}
#endif
	}
#endif
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
#ifdef WIZBALL_USE_SDL2
	if (PLATFORM_RENDERER_is_sdl2_native_sprite_enabled() && PLATFORM_RENDERER_is_sdl2_stub_ready() && (platform_renderer_present_height > 0))
	{
#if SDL_VERSION_ATLEAST(2, 0, 18)
		if ((platform_renderer_last_bound_texture_handle >= 1) && (platform_renderer_last_bound_texture_handle <= (unsigned int) platform_renderer_texture_count))
		{
			platform_renderer_texture_entry *entry = &platform_renderer_texture_entries[platform_renderer_last_bound_texture_handle - 1];
			if (PLATFORM_RENDERER_build_sdl_texture_from_entry(entry))
			{
				SDL_Vertex vertices[4];
				float txs[4];
				float tys[4];
				bool is_axis_aligned;
				bool src_flip_x;
				bool src_flip_y;
				bool uniform_colour = true;
				const float axis_eps = 0.01f;
				const float colour_eps = 0.01f;
				for (i = 0; i < 4; i++)
				{
					float tx;
					float ty;
					int cr;
					int cg;
					int cb;
					int ca;

					PLATFORM_RENDERER_transform_point(x[i], y[i], &tx, &ty);
					txs[i] = tx;
					tys[i] = ty;
					vertices[i].position.x = tx;
					vertices[i].position.y = (float) platform_renderer_present_height - ty;
					vertices[i].tex_coord.x = u[i];
					vertices[i].tex_coord.y = v[i];

					cr = (int) (r[i] * 255.0f);
					cg = (int) (g[i] * 255.0f);
					cb = (int) (b[i] * 255.0f);
					ca = (int) (a[i] * 255.0f);

					if (cr < 0) cr = 0;
					if (cr > 255) cr = 255;
					if (cg < 0) cg = 0;
					if (cg > 255) cg = 255;
					if (cb < 0) cb = 0;
					if (cb > 255) cb = 255;
					if (ca < 0) ca = 0;
					if (ca > 255) ca = 255;

					vertices[i].color.r = (Uint8) cr;
					vertices[i].color.g = (Uint8) cg;
					vertices[i].color.b = (Uint8) cb;
					vertices[i].color.a = (Uint8) ca;
				}
				for (i = 1; i < 4; i++)
				{
					if ((fabsf(r[i] - r[0]) > colour_eps) || (fabsf(g[i] - g[0]) > colour_eps) || (fabsf(b[i] - b[0]) > colour_eps) || (fabsf(a[i] - a[0]) > colour_eps))
					{
						uniform_colour = false;
						break;
					}
				}

				src_flip_x = (u[2] < u[0]);
				src_flip_y = (v[1] < v[0]);
				is_axis_aligned =
					(fabsf(txs[0] - txs[1]) < axis_eps) &&
					(fabsf(txs[2] - txs[3]) < axis_eps) &&
					(fabsf(tys[0] - tys[3]) < axis_eps) &&
					(fabsf(tys[1] - tys[2]) < axis_eps);

				if (is_axis_aligned && uniform_colour)
				{
					SDL_Rect src_rect;
					SDL_Rect dst_rect;
					float gl_left = (txs[0] < txs[2]) ? txs[0] : txs[2];
					float gl_right = (txs[0] > txs[2]) ? txs[0] : txs[2];
					float gl_top = (tys[0] > tys[2]) ? tys[0] : tys[2];
					float gl_bottom = (tys[0] < tys[2]) ? tys[0] : tys[2];
					SDL_RendererFlip final_flip = SDL_FLIP_NONE;

					if (!PLATFORM_RENDERER_build_safe_sdl_src_rect(entry, u[0], v[0], u[2], v[2], &src_rect))
					{
						return;
					}

					dst_rect.x = (int) gl_left;
					dst_rect.y = (int) ((float) platform_renderer_present_height - gl_top);
					dst_rect.w = (int) (gl_right - gl_left);
					dst_rect.h = (int) (gl_top - gl_bottom);
					if (dst_rect.w <= 0)
					{
						dst_rect.w = 1;
					}
					if (dst_rect.h <= 0)
					{
						dst_rect.h = 1;
					}

					if (src_flip_x)
					{
						final_flip = (SDL_RendererFlip) (final_flip | SDL_FLIP_HORIZONTAL);
					}
					if (src_flip_y)
					{
						final_flip = (SDL_RendererFlip) (final_flip | SDL_FLIP_VERTICAL);
					}

					(void) SDL_SetTextureColorMod(
						entry->sdl_texture,
						(Uint8) (r[0] * 255.0f),
						(Uint8) (g[0] * 255.0f),
						(Uint8) (b[0] * 255.0f));
					(void) SDL_SetTextureAlphaMod(entry->sdl_texture, (Uint8) (a[0] * 255.0f));

					if (SDL_RenderCopyEx(platform_renderer_sdl_renderer, entry->sdl_texture, &src_rect, &dst_rect, 0.0, NULL, final_flip) == 0)
					{
						platform_renderer_sdl_native_draw_count++;
						platform_renderer_sdl_native_textured_draw_count++;
					}
				}
				else
				{
					int indices[6] = { 0, 1, 2, 0, 2, 3 };
					(void) PLATFORM_RENDERER_try_sdl_geometry_textured(entry->sdl_texture, vertices, 4, indices, 6, PLATFORM_RENDERER_GEOM_SRC_COLOURED_ARRAY);
				}
			}
		}
#endif
	}
#endif
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
#ifdef WIZBALL_USE_SDL2
	if (PLATFORM_RENDERER_is_sdl2_native_sprite_enabled() && PLATFORM_RENDERER_is_sdl2_stub_ready() && (platform_renderer_present_height > 0))
	{
#if SDL_VERSION_ATLEAST(2, 0, 18)
		if ((platform_renderer_last_bound_texture_handle >= 1) && (platform_renderer_last_bound_texture_handle <= (unsigned int) platform_renderer_texture_count))
		{
			platform_renderer_texture_entry *entry = &platform_renderer_texture_entries[platform_renderer_last_bound_texture_handle - 1];
			if (PLATFORM_RENDERER_build_sdl_texture_from_entry(entry))
			{
				SDL_Vertex vertices[4];
				int i;
				const float px[4] = { x0, x1, x2, x3 };
				const float py[4] = { y0, y1, y2, y3 };
				const float tu[4] = { u1, u1, u2, u2 };
				const float tv[4] = { v1, v2, v2, v1 };
				float txs[4];
				float tys[4];
				const float axis_eps = 0.01f;
				bool is_axis_aligned;
				bool src_flip_x = (u2 < u1);
				bool src_flip_y = (v2 < v1);

				for (i = 0; i < 4; i++)
				{
					float tx;
					float ty;
					PLATFORM_RENDERER_transform_point(px[i], py[i], &tx, &ty);
					txs[i] = tx;
					tys[i] = ty;
					vertices[i].position.x = tx;
					vertices[i].position.y = (float) platform_renderer_present_height - ty;
					vertices[i].tex_coord.x = tu[i];
					vertices[i].tex_coord.y = tv[i];
					vertices[i].color.r = (Uint8) (platform_renderer_current_colour_r * 255.0f);
					vertices[i].color.g = (Uint8) (platform_renderer_current_colour_g * 255.0f);
					vertices[i].color.b = (Uint8) (platform_renderer_current_colour_b * 255.0f);
					vertices[i].color.a = (Uint8) (platform_renderer_current_colour_a * 255.0f);
				}

				is_axis_aligned =
					(fabsf(txs[0] - txs[1]) < axis_eps) &&
					(fabsf(txs[2] - txs[3]) < axis_eps) &&
					(fabsf(tys[0] - tys[3]) < axis_eps) &&
					(fabsf(tys[1] - tys[2]) < axis_eps);

				if (is_axis_aligned)
				{
					SDL_Rect src_rect;
					SDL_Rect dst_rect;
					float gl_left = (txs[0] < txs[2]) ? txs[0] : txs[2];
					float gl_right = (txs[0] > txs[2]) ? txs[0] : txs[2];
					float gl_top = (tys[0] > tys[2]) ? tys[0] : tys[2];
					float gl_bottom = (tys[0] < tys[2]) ? tys[0] : tys[2];
					SDL_RendererFlip final_flip = SDL_FLIP_NONE;

					if (!PLATFORM_RENDERER_build_safe_sdl_src_rect(entry, u1, v1, u2, v2, &src_rect))
					{
						return;
					}

					dst_rect.x = (int) gl_left;
					dst_rect.y = (int) ((float) platform_renderer_present_height - gl_top);
					dst_rect.w = (int) (gl_right - gl_left);
					dst_rect.h = (int) (gl_top - gl_bottom);
					if (dst_rect.w <= 0)
					{
						dst_rect.w = 1;
					}
					if (dst_rect.h <= 0)
					{
						dst_rect.h = 1;
					}

					if (src_flip_x)
					{
						final_flip = (SDL_RendererFlip) (final_flip | SDL_FLIP_HORIZONTAL);
					}
					if (src_flip_y)
					{
						final_flip = (SDL_RendererFlip) (final_flip | SDL_FLIP_VERTICAL);
					}

					(void) SDL_SetTextureColorMod(
						entry->sdl_texture,
						(Uint8) (platform_renderer_current_colour_r * 255.0f),
						(Uint8) (platform_renderer_current_colour_g * 255.0f),
						(Uint8) (platform_renderer_current_colour_b * 255.0f));
					(void) SDL_SetTextureAlphaMod(entry->sdl_texture, (Uint8) (platform_renderer_current_colour_a * 255.0f));

					if (SDL_RenderCopyEx(platform_renderer_sdl_renderer, entry->sdl_texture, &src_rect, &dst_rect, 0.0, NULL, final_flip) == 0)
					{
						platform_renderer_sdl_native_draw_count++;
						platform_renderer_sdl_native_textured_draw_count++;
					}
				}
				else
				{
					const float p0x = vertices[0].position.x;
					const float p0y = vertices[0].position.y;
					const float p1x = vertices[1].position.x;
					const float p1y = vertices[1].position.y;
					const float p2x = vertices[2].position.x;
					const float p2y = vertices[2].position.y;
					const float p3x = vertices[3].position.x;
					const float p3y = vertices[3].position.y;
					const float exx = p3x - p0x;
					const float exy = p3y - p0y;
					const float eyx = p1x - p0x;
					const float eyy = p1y - p0y;
					const float width = sqrtf((exx * exx) + (exy * exy));
					const float height = sqrtf((eyx * eyx) + (eyy * eyy));
					bool drawn_rotated_rect = false;

					if ((width > 0.5f) && (height > 0.5f))
					{
						SDL_Rect src_rect;
						SDL_Rect dst_rect;
						SDL_Point center;
						SDL_RendererFlip final_flip = SDL_FLIP_NONE;
						const float cx = (p0x + p1x + p2x + p3x) * 0.25f;
						const float cy = (p0y + p1y + p2y + p3y) * 0.25f;
						const double angle_degrees = atan2((double) exy, (double) exx) * (180.0 / 3.14159265358979323846);

						if (PLATFORM_RENDERER_build_safe_sdl_src_rect(entry, u1, v1, u2, v2, &src_rect))
						{
							dst_rect.w = (int) (width + 0.5f);
							dst_rect.h = (int) (height + 0.5f);
							if (dst_rect.w <= 0) dst_rect.w = 1;
							if (dst_rect.h <= 0) dst_rect.h = 1;
							dst_rect.x = (int) (cx - ((float) dst_rect.w * 0.5f));
							dst_rect.y = (int) (cy - ((float) dst_rect.h * 0.5f));

							center.x = dst_rect.w / 2;
							center.y = dst_rect.h / 2;

							if (src_flip_x)
							{
								final_flip = (SDL_RendererFlip) (final_flip | SDL_FLIP_HORIZONTAL);
							}
							if (src_flip_y)
							{
								final_flip = (SDL_RendererFlip) (final_flip | SDL_FLIP_VERTICAL);
							}

							(void) SDL_SetTextureColorMod(
								entry->sdl_texture,
								(Uint8) (platform_renderer_current_colour_r * 255.0f),
								(Uint8) (platform_renderer_current_colour_g * 255.0f),
								(Uint8) (platform_renderer_current_colour_b * 255.0f));
							(void) SDL_SetTextureAlphaMod(entry->sdl_texture, (Uint8) (platform_renderer_current_colour_a * 255.0f));

							if (SDL_RenderCopyEx(platform_renderer_sdl_renderer, entry->sdl_texture, &src_rect, &dst_rect, angle_degrees, &center, final_flip) == 0)
							{
								platform_renderer_sdl_native_draw_count++;
								platform_renderer_sdl_native_textured_draw_count++;
								drawn_rotated_rect = true;
							}
						}
					}

					if (!drawn_rotated_rect)
					{
						const int indices[6] = { 0, 1, 2, 0, 2, 3 };
						(void) PLATFORM_RENDERER_try_sdl_geometry_textured(entry->sdl_texture, vertices, 4, indices, 6, PLATFORM_RENDERER_GEOM_SRC_PERSPECTIVE);
					}
				}
			}
		}
#endif
	}
#endif
	(void) q;
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
#ifdef WIZBALL_USE_SDL2
	if (PLATFORM_RENDERER_is_sdl2_native_sprite_enabled() && PLATFORM_RENDERER_is_sdl2_stub_ready() && (platform_renderer_present_height > 0))
	{
#if SDL_VERSION_ATLEAST(2, 0, 18)
		if ((platform_renderer_last_bound_texture_handle >= 1) && (platform_renderer_last_bound_texture_handle <= (unsigned int) platform_renderer_texture_count))
		{
			platform_renderer_texture_entry *entry = &platform_renderer_texture_entries[platform_renderer_last_bound_texture_handle - 1];
			if (PLATFORM_RENDERER_build_sdl_texture_from_entry(entry))
			{
				SDL_Vertex vertices[4];
				int i;
				const float px[4] = { x0, x1, x2, x3 };
				const float py[4] = { y0, y1, y2, y3 };
				const float tu[4] = { u1, u1, u2, u2 };
				const float tv[4] = { v1, v2, v2, v1 };
				float txs[4];
				float tys[4];
				bool uniform_colour = true;
				const float axis_eps = 0.01f;
				const float colour_eps = 0.01f;
				bool is_axis_aligned;
				bool src_flip_x = (u2 < u1);
				bool src_flip_y = (v2 < v1);

				for (i = 0; i < 4; i++)
				{
					float tx;
					float ty;
					int cr = (int) (r[i] * 255.0f);
					int cg = (int) (g[i] * 255.0f);
					int cb = (int) (b[i] * 255.0f);
					int ca = (int) (a[i] * 255.0f);

					if (cr < 0) cr = 0;
					if (cr > 255) cr = 255;
					if (cg < 0) cg = 0;
					if (cg > 255) cg = 255;
					if (cb < 0) cb = 0;
					if (cb > 255) cb = 255;
					if (ca < 0) ca = 0;
					if (ca > 255) ca = 255;

					PLATFORM_RENDERER_transform_point(px[i], py[i], &tx, &ty);
					txs[i] = tx;
					tys[i] = ty;
					vertices[i].position.x = tx;
					vertices[i].position.y = (float) platform_renderer_present_height - ty;
					vertices[i].tex_coord.x = tu[i];
					vertices[i].tex_coord.y = tv[i];
					vertices[i].color.r = (Uint8) cr;
					vertices[i].color.g = (Uint8) cg;
					vertices[i].color.b = (Uint8) cb;
					vertices[i].color.a = (Uint8) ca;
				}

				for (i = 1; i < 4; i++)
				{
					if ((fabsf(r[i] - r[0]) > colour_eps) || (fabsf(g[i] - g[0]) > colour_eps) || (fabsf(b[i] - b[0]) > colour_eps) || (fabsf(a[i] - a[0]) > colour_eps))
					{
						uniform_colour = false;
						break;
					}
				}

				is_axis_aligned =
					(fabsf(txs[0] - txs[1]) < axis_eps) &&
					(fabsf(txs[2] - txs[3]) < axis_eps) &&
					(fabsf(tys[0] - tys[3]) < axis_eps) &&
					(fabsf(tys[1] - tys[2]) < axis_eps);

				if (is_axis_aligned && uniform_colour)
				{
					SDL_Rect src_rect;
					SDL_Rect dst_rect;
					float gl_left = (txs[0] < txs[2]) ? txs[0] : txs[2];
					float gl_right = (txs[0] > txs[2]) ? txs[0] : txs[2];
					float gl_top = (tys[0] > tys[2]) ? tys[0] : tys[2];
					float gl_bottom = (tys[0] < tys[2]) ? tys[0] : tys[2];
					SDL_RendererFlip final_flip = SDL_FLIP_NONE;

					if (!PLATFORM_RENDERER_build_safe_sdl_src_rect(entry, u1, v1, u2, v2, &src_rect))
					{
						return;
					}

					dst_rect.x = (int) gl_left;
					dst_rect.y = (int) ((float) platform_renderer_present_height - gl_top);
					dst_rect.w = (int) (gl_right - gl_left);
					dst_rect.h = (int) (gl_top - gl_bottom);
					if (dst_rect.w <= 0)
					{
						dst_rect.w = 1;
					}
					if (dst_rect.h <= 0)
					{
						dst_rect.h = 1;
					}

					if (src_flip_x)
					{
						final_flip = (SDL_RendererFlip) (final_flip | SDL_FLIP_HORIZONTAL);
					}
					if (src_flip_y)
					{
						final_flip = (SDL_RendererFlip) (final_flip | SDL_FLIP_VERTICAL);
					}

					(void) SDL_SetTextureColorMod(
						entry->sdl_texture,
						(Uint8) (r[0] * 255.0f),
						(Uint8) (g[0] * 255.0f),
						(Uint8) (b[0] * 255.0f));
					(void) SDL_SetTextureAlphaMod(entry->sdl_texture, (Uint8) (a[0] * 255.0f));

					if (SDL_RenderCopyEx(platform_renderer_sdl_renderer, entry->sdl_texture, &src_rect, &dst_rect, 0.0, NULL, final_flip) == 0)
					{
						platform_renderer_sdl_native_draw_count++;
						platform_renderer_sdl_native_textured_draw_count++;
					}
				}
				else
				{
					const float p0x = vertices[0].position.x;
					const float p0y = vertices[0].position.y;
					const float p1x = vertices[1].position.x;
					const float p1y = vertices[1].position.y;
					const float p2x = vertices[2].position.x;
					const float p2y = vertices[2].position.y;
					const float p3x = vertices[3].position.x;
					const float p3y = vertices[3].position.y;
					const float exx = p3x - p0x;
					const float exy = p3y - p0y;
					const float eyx = p1x - p0x;
					const float eyy = p1y - p0y;
					const float width = sqrtf((exx * exx) + (exy * exy));
					const float height = sqrtf((eyx * eyx) + (eyy * eyy));
					bool drawn_rotated_rect = false;

					if ((width > 0.5f) && (height > 0.5f))
					{
						SDL_Rect src_rect;
						SDL_Rect dst_rect;
						SDL_Point center;
						SDL_RendererFlip final_flip = SDL_FLIP_NONE;
						const float cx = (p0x + p1x + p2x + p3x) * 0.25f;
						const float cy = (p0y + p1y + p2y + p3y) * 0.25f;
						const double angle_degrees = atan2((double) exy, (double) exx) * (180.0 / 3.14159265358979323846);
						const float avg_r = (r[0] + r[1] + r[2] + r[3]) * 0.25f;
						const float avg_g = (g[0] + g[1] + g[2] + g[3]) * 0.25f;
						const float avg_b = (b[0] + b[1] + b[2] + b[3]) * 0.25f;
						const float avg_a = (a[0] + a[1] + a[2] + a[3]) * 0.25f;

						if (PLATFORM_RENDERER_build_safe_sdl_src_rect(entry, u1, v1, u2, v2, &src_rect))
						{
							dst_rect.w = (int) (width + 0.5f);
							dst_rect.h = (int) (height + 0.5f);
							if (dst_rect.w <= 0) dst_rect.w = 1;
							if (dst_rect.h <= 0) dst_rect.h = 1;
							dst_rect.x = (int) (cx - ((float) dst_rect.w * 0.5f));
							dst_rect.y = (int) (cy - ((float) dst_rect.h * 0.5f));

							center.x = dst_rect.w / 2;
							center.y = dst_rect.h / 2;

							if (src_flip_x)
							{
								final_flip = (SDL_RendererFlip) (final_flip | SDL_FLIP_HORIZONTAL);
							}
							if (src_flip_y)
							{
								final_flip = (SDL_RendererFlip) (final_flip | SDL_FLIP_VERTICAL);
							}

							(void) SDL_SetTextureColorMod(
								entry->sdl_texture,
								(Uint8) (avg_r * 255.0f),
								(Uint8) (avg_g * 255.0f),
								(Uint8) (avg_b * 255.0f));
							(void) SDL_SetTextureAlphaMod(entry->sdl_texture, (Uint8) (avg_a * 255.0f));

							if (SDL_RenderCopyEx(platform_renderer_sdl_renderer, entry->sdl_texture, &src_rect, &dst_rect, angle_degrees, &center, final_flip) == 0)
							{
								platform_renderer_sdl_native_draw_count++;
								platform_renderer_sdl_native_textured_draw_count++;
								drawn_rotated_rect = true;
							}
						}
					}

					if (!drawn_rotated_rect)
					{
						const int indices[6] = { 0, 1, 2, 0, 2, 3 };
						(void) PLATFORM_RENDERER_try_sdl_geometry_textured(entry->sdl_texture, vertices, 4, indices, 6, PLATFORM_RENDERER_GEOM_SRC_COLOURED_PERSPECTIVE);
					}
				}
			}
		}
#endif
	}
#endif
	(void) q;
}

void PLATFORM_RENDERER_draw_textured_quad(unsigned int texture_handle, int r, int g, int b, float screen_x, float screen_y, int virtual_screen_height, float left, float right, float up, float down, float u1, float v1, float u2, float v2, bool alpha_test)
{
#ifdef WIZBALL_USE_SDL2
	if (PLATFORM_RENDERER_is_sdl2_native_sprite_enabled() && PLATFORM_RENDERER_is_sdl2_stub_ready())
	{
		if ((texture_handle >= 1) && (texture_handle <= (unsigned int) platform_renderer_texture_count))
		{
			platform_renderer_texture_entry *entry = &platform_renderer_texture_entries[texture_handle - 1];
			if (PLATFORM_RENDERER_build_sdl_texture_from_entry(entry))
			{
				float x0 = screen_x + left;
				float x1 = screen_x + right;
				float y0 = screen_y + up;
				float y1 = screen_y + down;
				float sdl_y0 = (float) virtual_screen_height - y0;
				float sdl_y1 = (float) virtual_screen_height - y1;
				float dst_left = (x0 < x1) ? x0 : x1;
				float dst_right = (x0 > x1) ? x0 : x1;
				float dst_top = (sdl_y0 < sdl_y1) ? sdl_y0 : sdl_y1;
				float dst_bottom = (sdl_y0 > sdl_y1) ? sdl_y0 : sdl_y1;
				SDL_Rect src_rect;
				SDL_Rect dst_rect;
				if (!PLATFORM_RENDERER_build_safe_sdl_src_rect(entry, u1, v1, u2, v2, &src_rect))
				{
					return;
				}

				dst_rect.x = (int) dst_left;
				dst_rect.y = (int) dst_top;
				dst_rect.w = (int) (dst_right - dst_left);
				dst_rect.h = (int) (dst_bottom - dst_top);

				if (dst_rect.w <= 0)
				{
					dst_rect.w = 1;
				}
				if (dst_rect.h <= 0)
				{
					dst_rect.h = 1;
				}

				(void) alpha_test;
				(void) SDL_SetTextureColorMod(entry->sdl_texture, (Uint8) r, (Uint8) g, (Uint8) b);
				(void) SDL_SetTextureAlphaMod(entry->sdl_texture, 255);
				if (SDL_RenderCopy(platform_renderer_sdl_renderer, entry->sdl_texture, &src_rect, &dst_rect) == 0)
				{
					platform_renderer_sdl_native_draw_count++;
					platform_renderer_sdl_native_textured_draw_count++;
				}
			}
		}
	}
#endif
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

void PLATFORM_RENDERER_draw_sdl_window_sprite(unsigned int texture_handle, int r, int g, int b, float entity_x, float entity_y, float left, float right, float up, float down, float u1, float v1, float u2, float v2, float left_window_transform_x, float top_window_transform_y, float total_scale_x, float total_scale_y, float sprite_scale_x, float sprite_scale_y, float sprite_rotation_degrees, bool sprite_flip_x, bool sprite_flip_y)
{
#ifdef WIZBALL_USE_SDL2
	if (!(PLATFORM_RENDERER_is_sdl2_native_sprite_enabled() && PLATFORM_RENDERER_is_sdl2_stub_ready()))
	{
		return;
	}

	if ((texture_handle < 1) || (texture_handle > (unsigned int) platform_renderer_texture_count))
	{
		return;
	}

	platform_renderer_texture_entry *entry = &platform_renderer_texture_entries[texture_handle - 1];
	if (!PLATFORM_RENDERER_build_sdl_texture_from_entry(entry))
	{
		return;
	}

	if (platform_renderer_present_height <= 0)
	{
		return;
	}

	float transformed_left = left * sprite_scale_x;
	float transformed_right = right * sprite_scale_x;
	float transformed_up = up * sprite_scale_y;
	float transformed_down = down * sprite_scale_y;

	float tx0 = left_window_transform_x + ((entity_x + transformed_left) * total_scale_x);
	float tx1 = left_window_transform_x + ((entity_x + transformed_right) * total_scale_x);
	float ty0 = top_window_transform_y + (((-entity_y) + transformed_up) * total_scale_y);
	float ty1 = top_window_transform_y + (((-entity_y) + transformed_down) * total_scale_y);

	float dst_left = (tx0 < tx1) ? tx0 : tx1;
	float dst_right = (tx0 > tx1) ? tx0 : tx1;
	float gl_bottom = (ty0 < ty1) ? ty0 : ty1;
	float gl_top = (ty0 > ty1) ? ty0 : ty1;

	bool src_flip_x = (u2 < u1);
	bool src_flip_y = (v2 < v1);

	SDL_Rect src_rect;
	if (!PLATFORM_RENDERER_build_safe_sdl_src_rect(entry, u1, v1, u2, v2, &src_rect))
	{
		return;
	}

	SDL_Rect dst_rect;
	dst_rect.x = (int) dst_left;
	dst_rect.y = (int) ((float) platform_renderer_present_height - gl_top);
	dst_rect.w = (int) (dst_right - dst_left);
	dst_rect.h = (int) (gl_top - gl_bottom);

	if (dst_rect.w <= 0)
	{
		dst_rect.w = 1;
	}
	if (dst_rect.h <= 0)
	{
		dst_rect.h = 1;
	}

	float origin_x_gl = left_window_transform_x + (entity_x * total_scale_x);
	float origin_y_gl = top_window_transform_y + ((-entity_y) * total_scale_y);
	float origin_y_sdl = (float) platform_renderer_present_height - origin_y_gl;

	SDL_Point center;
	center.x = (int) (origin_x_gl - (float) dst_rect.x);
	center.y = (int) (origin_y_sdl - (float) dst_rect.y);

	SDL_RendererFlip final_flip = SDL_FLIP_NONE;
	if (src_flip_x ^ sprite_flip_x)
	{
		final_flip = (SDL_RendererFlip) (final_flip | SDL_FLIP_HORIZONTAL);
	}
	if (src_flip_y ^ sprite_flip_y)
	{
		final_flip = (SDL_RendererFlip) (final_flip | SDL_FLIP_VERTICAL);
	}

	(void) SDL_SetTextureColorMod(entry->sdl_texture, (Uint8) r, (Uint8) g, (Uint8) b);
	(void) SDL_SetTextureAlphaMod(entry->sdl_texture, 255);
	if (SDL_RenderCopyEx(platform_renderer_sdl_renderer, entry->sdl_texture, &src_rect, &dst_rect, sprite_rotation_degrees, &center, final_flip) == 0)
	{
		platform_renderer_sdl_native_draw_count++;
		platform_renderer_sdl_native_textured_draw_count++;
	}
#else
	(void) texture_handle;
	(void) r;
	(void) g;
	(void) b;
	(void) entity_x;
	(void) entity_y;
	(void) left;
	(void) right;
	(void) up;
	(void) down;
	(void) u1;
	(void) v1;
	(void) u2;
	(void) v2;
	(void) left_window_transform_x;
	(void) top_window_transform_y;
	(void) total_scale_x;
	(void) total_scale_y;
	(void) sprite_scale_x;
	(void) sprite_scale_y;
	(void) sprite_rotation_degrees;
	(void) sprite_flip_x;
	(void) sprite_flip_y;
#endif
}

void PLATFORM_RENDERER_draw_sdl_window_solid_rect(int r, int g, int b, float entity_x, float entity_y, float left, float right, float up, float down, float left_window_transform_x, float top_window_transform_y, float total_scale_x, float total_scale_y, float rect_scale_x, float rect_scale_y)
{
#ifdef WIZBALL_USE_SDL2
	if (!(PLATFORM_RENDERER_is_sdl2_native_sprite_enabled() && PLATFORM_RENDERER_is_sdl2_stub_ready()))
	{
		return;
	}

	if (platform_renderer_present_height <= 0)
	{
		return;
	}

	float transformed_left = left * rect_scale_x;
	float transformed_right = right * rect_scale_x;
	float transformed_up = up * rect_scale_y;
	float transformed_down = down * rect_scale_y;

	float tx0 = left_window_transform_x + ((entity_x + transformed_left) * total_scale_x);
	float tx1 = left_window_transform_x + ((entity_x + transformed_right) * total_scale_x);
	float ty0 = top_window_transform_y + (((-entity_y) + transformed_up) * total_scale_y);
	float ty1 = top_window_transform_y + (((-entity_y) + transformed_down) * total_scale_y);

	float dst_left = (tx0 < tx1) ? tx0 : tx1;
	float dst_right = (tx0 > tx1) ? tx0 : tx1;
	float gl_bottom = (ty0 < ty1) ? ty0 : ty1;
	float gl_top = (ty0 > ty1) ? ty0 : ty1;

	SDL_Rect dst_rect;
	dst_rect.x = (int) dst_left;
	dst_rect.y = (int) ((float) platform_renderer_present_height - gl_top);
	dst_rect.w = (int) (dst_right - dst_left);
	dst_rect.h = (int) (gl_top - gl_bottom);

	if (dst_rect.w <= 0)
	{
		dst_rect.w = 1;
	}
	if (dst_rect.h <= 0)
	{
		dst_rect.h = 1;
	}

	(void) SDL_SetRenderDrawBlendMode(platform_renderer_sdl_renderer, SDL_BLENDMODE_BLEND);
	(void) SDL_SetRenderDrawColor(platform_renderer_sdl_renderer, (Uint8) r, (Uint8) g, (Uint8) b, 255);
	if (SDL_RenderFillRect(platform_renderer_sdl_renderer, &dst_rect) == 0)
	{
		platform_renderer_sdl_native_draw_count++;
	}
#else
	(void) r;
	(void) g;
	(void) b;
	(void) entity_x;
	(void) entity_y;
	(void) left;
	(void) right;
	(void) up;
	(void) down;
	(void) left_window_transform_x;
	(void) top_window_transform_y;
	(void) total_scale_x;
	(void) total_scale_y;
	(void) rect_scale_x;
	(void) rect_scale_y;
#endif
}

void PLATFORM_RENDERER_draw_sdl_bound_textured_quad_custom(unsigned int texture_handle, float x0, float y0, float x1, float y1, float x2, float y2, float x3, float y3, float u1, float v1, float u2, float v2)
{
#ifdef WIZBALL_USE_SDL2
	if (!(PLATFORM_RENDERER_is_sdl2_native_sprite_enabled() && PLATFORM_RENDERER_is_sdl2_stub_ready()))
	{
		return;
	}

	if (platform_renderer_present_height <= 0)
	{
		return;
	}

	if ((texture_handle < 1) || (texture_handle > (unsigned int) platform_renderer_texture_count))
	{
		return;
	}

	platform_renderer_texture_entry *entry = &platform_renderer_texture_entries[texture_handle - 1];
	if (!PLATFORM_RENDERER_build_sdl_texture_from_entry(entry))
	{
		return;
	}

#if SDL_VERSION_ATLEAST(2, 0, 18)
	float px[4];
	float py[4];
	PLATFORM_RENDERER_transform_point(x0, y0, &px[0], &py[0]);
	PLATFORM_RENDERER_transform_point(x1, y1, &px[1], &py[1]);
	PLATFORM_RENDERER_transform_point(x2, y2, &px[2], &py[2]);
	PLATFORM_RENDERER_transform_point(x3, y3, &px[3], &py[3]);

	SDL_Vertex vertices[4];
	int i;
	bool src_flip_x = (u2 < u1);
	bool src_flip_y = (v2 < v1);
	const float axis_eps = 0.01f;
	bool is_axis_aligned;
	for (i = 0; i < 4; i++)
	{
		vertices[i].position.x = px[i];
		vertices[i].position.y = (float) platform_renderer_present_height - py[i];
		vertices[i].color.r = (Uint8) (platform_renderer_current_colour_r * 255.0f);
		vertices[i].color.g = (Uint8) (platform_renderer_current_colour_g * 255.0f);
		vertices[i].color.b = (Uint8) (platform_renderer_current_colour_b * 255.0f);
		vertices[i].color.a = (Uint8) (platform_renderer_current_colour_a * 255.0f);
	}

	vertices[0].tex_coord.x = u1; vertices[0].tex_coord.y = v1;
	vertices[1].tex_coord.x = u1; vertices[1].tex_coord.y = v2;
	vertices[2].tex_coord.x = u2; vertices[2].tex_coord.y = v2;
	vertices[3].tex_coord.x = u2; vertices[3].tex_coord.y = v1;

	is_axis_aligned =
		(fabsf(px[0] - px[1]) < axis_eps) &&
		(fabsf(px[2] - px[3]) < axis_eps) &&
		(fabsf(py[0] - py[3]) < axis_eps) &&
		(fabsf(py[1] - py[2]) < axis_eps);

	if (is_axis_aligned)
	{
		SDL_Rect src_rect;
		SDL_Rect dst_rect;
		float gl_left = (px[0] < px[2]) ? px[0] : px[2];
		float gl_right = (px[0] > px[2]) ? px[0] : px[2];
		float gl_top = (py[0] > py[2]) ? py[0] : py[2];
		float gl_bottom = (py[0] < py[2]) ? py[0] : py[2];
		SDL_RendererFlip final_flip = SDL_FLIP_NONE;

		if (!PLATFORM_RENDERER_build_safe_sdl_src_rect(entry, u1, v1, u2, v2, &src_rect))
		{
			return;
		}

		dst_rect.x = (int) gl_left;
		dst_rect.y = (int) ((float) platform_renderer_present_height - gl_top);
		dst_rect.w = (int) (gl_right - gl_left);
		dst_rect.h = (int) (gl_top - gl_bottom);
		if (dst_rect.w <= 0)
		{
			dst_rect.w = 1;
		}
		if (dst_rect.h <= 0)
		{
			dst_rect.h = 1;
		}

		if (src_flip_x)
		{
			final_flip = (SDL_RendererFlip) (final_flip | SDL_FLIP_HORIZONTAL);
		}
		if (src_flip_y)
		{
			final_flip = (SDL_RendererFlip) (final_flip | SDL_FLIP_VERTICAL);
		}

		(void) SDL_SetTextureColorMod(
			entry->sdl_texture,
			(Uint8) (platform_renderer_current_colour_r * 255.0f),
			(Uint8) (platform_renderer_current_colour_g * 255.0f),
			(Uint8) (platform_renderer_current_colour_b * 255.0f));
		(void) SDL_SetTextureAlphaMod(entry->sdl_texture, (Uint8) (platform_renderer_current_colour_a * 255.0f));

		if (SDL_RenderCopyEx(platform_renderer_sdl_renderer, entry->sdl_texture, &src_rect, &dst_rect, 0.0, NULL, final_flip) == 0)
		{
			platform_renderer_sdl_native_draw_count++;
			platform_renderer_sdl_native_textured_draw_count++;
		}
	}
	else
	{
		const float p0x = vertices[0].position.x;
		const float p0y = vertices[0].position.y;
		const float p1x = vertices[1].position.x;
		const float p1y = vertices[1].position.y;
		const float p2x = vertices[2].position.x;
		const float p2y = vertices[2].position.y;
		const float p3x = vertices[3].position.x;
		const float p3y = vertices[3].position.y;
		const float exx = p3x - p0x;
		const float exy = p3y - p0y;
		const float eyx = p1x - p0x;
		const float eyy = p1y - p0y;
		const float width = sqrtf((exx * exx) + (exy * exy));
		const float height = sqrtf((eyx * eyx) + (eyy * eyy));
		const float eps = 1.5f;
		const float p21x = p2x - p1x;
		const float p21y = p2y - p1y;
		const float p23x = p2x - p3x;
		const float p23y = p2y - p3y;
		const float dot = (exx * eyx) + (exy * eyy);
		const bool edges_match =
			(fabsf((p21x - exx)) <= eps) &&
			(fabsf((p21y - exy)) <= eps) &&
			(fabsf((p23x - eyx)) <= eps) &&
			(fabsf((p23y - eyy)) <= eps);
		const bool near_rect = (width > 0.5f) && (height > 0.5f) && (fabsf(dot) < ((width * height) * 0.15f)) && edges_match;
		bool drawn_rotated_rect = false;

		if (near_rect || true)
		{
			SDL_Rect src_rect;
			SDL_Rect dst_rect;
			SDL_Point center;
			SDL_RendererFlip final_flip = SDL_FLIP_NONE;
			const float cx = (p0x + p1x + p2x + p3x) * 0.25f;
			const float cy = (p0y + p1y + p2y + p3y) * 0.25f;
			const double angle_degrees = atan2((double) exy, (double) exx) * (180.0 / 3.14159265358979323846);

			if (PLATFORM_RENDERER_build_safe_sdl_src_rect(entry, u1, v1, u2, v2, &src_rect))
			{
				dst_rect.w = (int) (width + 0.5f);
				dst_rect.h = (int) (height + 0.5f);
				if (dst_rect.w <= 0) dst_rect.w = 1;
				if (dst_rect.h <= 0) dst_rect.h = 1;
				dst_rect.x = (int) (cx - ((float) dst_rect.w * 0.5f));
				dst_rect.y = (int) (cy - ((float) dst_rect.h * 0.5f));

				center.x = dst_rect.w / 2;
				center.y = dst_rect.h / 2;

				if (src_flip_x)
				{
					final_flip = (SDL_RendererFlip) (final_flip | SDL_FLIP_HORIZONTAL);
				}
				if (src_flip_y)
				{
					final_flip = (SDL_RendererFlip) (final_flip | SDL_FLIP_VERTICAL);
				}

				(void) SDL_SetTextureColorMod(
					entry->sdl_texture,
					(Uint8) (platform_renderer_current_colour_r * 255.0f),
					(Uint8) (platform_renderer_current_colour_g * 255.0f),
					(Uint8) (platform_renderer_current_colour_b * 255.0f));
				(void) SDL_SetTextureAlphaMod(entry->sdl_texture, (Uint8) (platform_renderer_current_colour_a * 255.0f));

				if (SDL_RenderCopyEx(platform_renderer_sdl_renderer, entry->sdl_texture, &src_rect, &dst_rect, angle_degrees, &center, final_flip) == 0)
				{
					platform_renderer_sdl_native_draw_count++;
					platform_renderer_sdl_native_textured_draw_count++;
					drawn_rotated_rect = true;
				}
			}
		}

		if (!drawn_rotated_rect)
		{
			int indices[6] = { 0, 1, 2, 0, 2, 3 };
			(void) PLATFORM_RENDERER_try_sdl_geometry_textured(entry->sdl_texture, vertices, 4, indices, 6, PLATFORM_RENDERER_GEOM_SRC_SDL_CUSTOM);
		}
	}
#else
	(void) x0;
	(void) y0;
	(void) x1;
	(void) y1;
	(void) x2;
	(void) y2;
	(void) x3;
	(void) y3;
	(void) u1;
	(void) v1;
	(void) u2;
	(void) v2;
#endif
#else
	(void) texture_handle;
	(void) x0;
	(void) y0;
	(void) x1;
	(void) y1;
	(void) x2;
	(void) y2;
	(void) x3;
	(void) y3;
	(void) u1;
	(void) v1;
	(void) u2;
	(void) v2;
#endif
}

unsigned int PLATFORM_RENDERER_create_masked_texture(BITMAP *image)
{
	unsigned int gl_texture_id = (unsigned int) allegro_gl_make_masked_texture(image);
	return PLATFORM_RENDERER_register_gl_texture(gl_texture_id, image);
}

void PLATFORM_RENDERER_present_frame(int width, int height)
{
	bool should_flip_allegro_gl = true;
#ifdef WIZBALL_USE_SDL2
	const int native_primary_min_draws_for_no_mirror = platform_renderer_sdl_no_mirror_min_draws;
	const int native_primary_min_textured_draws_for_no_mirror = platform_renderer_sdl_no_mirror_min_textured;
	const int native_primary_required_streak_frames = platform_renderer_sdl_no_mirror_required_streak;
	platform_renderer_present_width = width;
	platform_renderer_present_height = height;
	bool native_primary_strict = platform_renderer_sdl_native_primary_strict_enabled;
	bool native_primary_requested = PLATFORM_RENDERER_is_sdl2_native_primary_enabled();
	bool native_primary = native_primary_requested && native_primary_strict;
	bool native_primary_force_no_mirror = platform_renderer_sdl_native_primary_force_no_mirror_enabled;
	bool native_primary_auto_no_mirror = platform_renderer_sdl_native_primary_auto_no_mirror_enabled;
	bool allow_degraded_for_transition = platform_renderer_sdl_native_primary_allow_degraded_no_mirror_enabled;
	bool coverage_good_this_frame =
		(platform_renderer_sdl_native_textured_draw_count > 0) &&
		(platform_renderer_sdl_geometry_fallback_miss_count == 0) &&
		(allow_degraded_for_transition || (platform_renderer_sdl_geometry_degraded_count == 0));
	bool allow_no_mirror_this_frame = false;
	bool used_mirror_fallback = false;
	int geom_hot_source = PLATFORM_RENDERER_GEOM_SRC_NONE;
	int geom_hot_count = 0;
	int geom_i;
	for (geom_i = 1; geom_i < PLATFORM_RENDERER_GEOM_SRC_COUNT; geom_i++)
	{
		int combined = platform_renderer_sdl_geometry_miss_sources[geom_i] + platform_renderer_sdl_geometry_degraded_sources[geom_i];
		if (combined > geom_hot_count)
		{
			geom_hot_count = combined;
			geom_hot_source = geom_i;
		}
	}
	if (platform_renderer_sdl_no_mirror_cooldown_frames > 0)
	{
		platform_renderer_sdl_no_mirror_cooldown_frames--;
	}
	if (coverage_good_this_frame)
	{
		if (platform_renderer_sdl_native_coverage_streak < 1000000)
		{
			platform_renderer_sdl_native_coverage_streak++;
		}
	}
	else
	{
		platform_renderer_sdl_native_coverage_streak = 0;
	}
	if (native_primary && native_primary_force_no_mirror)
	{
		allow_no_mirror_this_frame = true;
	}
	else if (native_primary && native_primary_auto_no_mirror && !platform_renderer_sdl_no_mirror_blocked_for_session && (platform_renderer_sdl_no_mirror_cooldown_frames == 0) && coverage_good_this_frame)
	{
		allow_no_mirror_this_frame = true;
	}
	if (PLATFORM_RENDERER_is_sdl2_stub_enabled() && !PLATFORM_RENDERER_is_sdl2_stub_ready())
	{
		if (PLATFORM_RENDERER_prepare_sdl2_stub(width, height, true) && !platform_renderer_sdl_stub_self_test_done)
		{
			(void) PLATFORM_RENDERER_run_sdl2_stub_self_test();
			platform_renderer_sdl_stub_self_test_done = true;
		}
	}
	if (!native_primary)
	{
		(void) PLATFORM_RENDERER_mirror_from_current_backbuffer(width, height);
		used_mirror_fallback = true;
	}
	else if ((!native_primary_strict) || !allow_no_mirror_this_frame)
	{
		// Default primary mode remains safe: mirror while migration coverage is incomplete.
		(void) PLATFORM_RENDERER_mirror_from_current_backbuffer(width, height);
		used_mirror_fallback = true;
	}
	if (PLATFORM_RENDERER_is_sdl2_stub_ready())
	{
		if (PLATFORM_RENDERER_is_sdl2_native_sprite_enabled())
		{
			if (platform_renderer_sdl_native_draw_count < native_primary_min_draws_for_no_mirror)
			{
				if (native_primary)
				{
					if (native_primary_force_no_mirror)
					{
						snprintf(platform_renderer_sdl_status, sizeof(platform_renderer_sdl_status), "SDL2 strict no-mirror forced: draws=%d textured=%d geom_miss=%d degraded=%d allow_degraded=%d hot_src=%d(%d) streak=%d cooldown=%d blocked=%d.", platform_renderer_sdl_native_draw_count, platform_renderer_sdl_native_textured_draw_count, platform_renderer_sdl_geometry_fallback_miss_count, platform_renderer_sdl_geometry_degraded_count, allow_degraded_for_transition ? 1 : 0, geom_hot_source, geom_hot_count, platform_renderer_sdl_native_coverage_streak, platform_renderer_sdl_no_mirror_cooldown_frames, platform_renderer_sdl_no_mirror_blocked_for_session ? 1 : 0);
					}
					else if (native_primary_auto_no_mirror)
					{
						snprintf(platform_renderer_sdl_status, sizeof(platform_renderer_sdl_status), "SDL2 strict auto no-mirror: fallback=%d draws=%d textured=%d geom_miss=%d degraded=%d allow_degraded=%d hot_src=%d(%d) streak=%d/%d cooldown=%d blocked=%d.", used_mirror_fallback ? 1 : 0, platform_renderer_sdl_native_draw_count, platform_renderer_sdl_native_textured_draw_count, platform_renderer_sdl_geometry_fallback_miss_count, platform_renderer_sdl_geometry_degraded_count, allow_degraded_for_transition ? 1 : 0, geom_hot_source, geom_hot_count, platform_renderer_sdl_native_coverage_streak, native_primary_required_streak_frames, platform_renderer_sdl_no_mirror_cooldown_frames, platform_renderer_sdl_no_mirror_blocked_for_session ? 1 : 0);
					}
					else
					{
						snprintf(platform_renderer_sdl_status, sizeof(platform_renderer_sdl_status), "SDL2 strict-primary safe mode: mirrored fallback active (set WIZBALL_SDL2_NATIVE_PRIMARY_AUTO_NO_MIRROR=1 or ...FORCE_NO_MIRROR=1). draws=%d textured=%d geom_miss=%d degraded=%d hot_src=%d(%d) streak=%d.", platform_renderer_sdl_native_draw_count, platform_renderer_sdl_native_textured_draw_count, platform_renderer_sdl_geometry_fallback_miss_count, platform_renderer_sdl_geometry_degraded_count, geom_hot_source, geom_hot_count, platform_renderer_sdl_native_coverage_streak);
					}
				}
				else if (native_primary_requested)
				{
					snprintf(platform_renderer_sdl_status, sizeof(platform_renderer_sdl_status), "SDL2 native primary requested (compat mode); using mirrored GL fallback (draws=%d textured=%d geom_miss=%d degraded=%d hot_src=%d(%d)).", platform_renderer_sdl_native_draw_count, platform_renderer_sdl_native_textured_draw_count, platform_renderer_sdl_geometry_fallback_miss_count, platform_renderer_sdl_geometry_degraded_count, geom_hot_source, geom_hot_count);
				}
				else
				{
					snprintf(platform_renderer_sdl_status, sizeof(platform_renderer_sdl_status), "SDL2 native sprites enabled; currently no native draw-path calls this frame (showing mirrored GL).");
				}
			}
		}
		SDL_RenderPresent(platform_renderer_sdl_renderer);
	}
	PLATFORM_RENDERER_maybe_print_sdl_status_stderr(platform_renderer_sdl_status);
	if (native_primary)
	{
		// Keep GL swap active in strict auto mode so mirror fallback transitions
		// always sample a fresh/sane GL frame instead of stale buffer content.
		if (native_primary_auto_no_mirror && !native_primary_force_no_mirror)
		{
			should_flip_allegro_gl = true;
		}
		else
		{
			should_flip_allegro_gl = used_mirror_fallback;
		}
	}
#else
	(void) width;
	(void) height;
#endif
	if (should_flip_allegro_gl)
	{
		allegro_gl_flip();
	}
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
	// Clear any pre-existing GL error flags from gameplay draw code so mirror checks
	// only capture errors caused by this readback call itself.
	while (glGetError() != GL_NO_ERROR)
	{
	}

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
