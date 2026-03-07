#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <stdarg.h>
#include <SDL.h>

#include "platform_renderer.h"
#include "main.h"
#include "output.h"
#include "scripting.h"
#include "platform_renderer_texture_entry.h"

static void PLATFORM_RENDERER_apply_sdl_texture_blend_mode(SDL_Texture *texture);
static Uint8 PLATFORM_RENDERER_get_sdl_texture_alpha_mod(Uint8 requested_alpha);
static void PLATFORM_RENDERER_set_texture_color_alpha_cached(SDL_Texture *tex, Uint8 r, Uint8 g, Uint8 b, Uint8 a);
static int PLATFORM_RENDERER_sdl_copy_ex(SDL_Renderer *renderer, SDL_Texture *texture, const SDL_Rect *src, const SDL_Rect *dst, double angle, const SDL_Point *center, SDL_RendererFlip flip);
static void PLATFORM_RENDERER_flush_addq(void);
static bool PLATFORM_RENDERER_should_defer_add_geometry(SDL_Texture *texture, int vertex_count, const int *indices, int index_count, int source_id);
static bool PLATFORM_RENDERER_prepare_geometry_texture_atlas(void);
static bool PLATFORM_RENDERER_try_remap_geometry_texture_to_atlas(SDL_Texture **io_texture, SDL_Vertex *vertices, int vertex_count);

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
static bool platform_renderer_blend_enabled = false;
enum
{
	PLATFORM_RENDERER_BLEND_MODE_ALPHA = 0,
	PLATFORM_RENDERER_BLEND_MODE_ADD = 1,
	PLATFORM_RENDERER_BLEND_MODE_SUBTRACT = 2,
	PLATFORM_RENDERER_BLEND_MODE_MULTIPLY = 3
};
static int platform_renderer_blend_mode = PLATFORM_RENDERER_BLEND_MODE_ALPHA;
static unsigned int platform_renderer_last_bound_texture_handle = 0;
typedef void (*platform_active_texture_proc_t)(int texture_unit);
static platform_active_texture_proc_t platform_renderer_active_texture_proc = NULL;
typedef void (*platform_secondary_colour_proc_t)(float red, float green, float blue);
static SDL_Window *platform_renderer_sdl_window = NULL;
static SDL_Renderer *platform_renderer_sdl_renderer = NULL;
static bool platform_renderer_sdl_stub_show_checked_env = false;
static bool platform_renderer_sdl_stub_show_enabled = false;
static bool platform_renderer_sdl_stub_accel_checked_env = false;
static bool platform_renderer_sdl_stub_accel_enabled = false;
static bool platform_renderer_sdl_stub_self_test_done = false;
static bool platform_renderer_sdl_native_primary_strict_enabled = false;
static bool platform_renderer_sdl_native_primary_force_no_mirror_enabled = false;
static bool platform_renderer_sdl_core_quads_enabled = false;
static bool platform_renderer_sdl_geometry_checked_env = false;
static bool platform_renderer_sdl_geometry_enabled = false;
static int platform_renderer_sdl_no_mirror_min_draws = 24;
static int platform_renderer_sdl_no_mirror_min_textured = 16;
static int platform_renderer_sdl_no_mirror_required_streak = 30;
static int platform_renderer_sdl_native_draw_count = 0;
static int platform_renderer_sdl_native_textured_draw_count = 0;
static int platform_renderer_sdl_geometry_fallback_miss_count = 0;
static int platform_renderer_sdl_geometry_degraded_count = 0;
static int platform_renderer_sdl_window_sprite_draw_count = 0;
static int platform_renderer_sdl_window_solid_rect_draw_count = 0;
static int platform_renderer_sdl_bound_custom_draw_count = 0;
static int platform_renderer_legacy_window_sprite_draw_count = 0;
static int platform_renderer_legacy_window_solid_rect_draw_count = 0;
static int platform_renderer_legacy_bound_custom_draw_count = 0;
static int platform_renderer_legacy_src6_count = 0;
static float platform_renderer_legacy_src6_min_q = 0.0f;
static float platform_renderer_legacy_src6_max_q = 0.0f;
static float platform_renderer_legacy_src6_min_x = 0.0f;
static float platform_renderer_legacy_src6_min_y = 0.0f;
static float platform_renderer_legacy_src6_max_x = 0.0f;
static float platform_renderer_legacy_src6_max_y = 0.0f;
static int platform_renderer_legacy_draw_count = 0;
static int platform_renderer_legacy_textured_draw_count = 0;
static int platform_renderer_clear_backbuffer_calls_since_present = 0;
static int platform_renderer_midframe_reset_events = 0;
static int platform_renderer_sdl_tx_switch_count = 0;    /* texture object changes between consecutive SDL_RenderGeometry calls */
static int platform_renderer_sdl_clip_switch_count = 0;  /* SDL_RenderSetClipRect calls that actually change state */
static int platform_renderer_sdl_blend_switch_count = 0; /* SDL_SetTextureBlendMode calls that actually change state */
static int platform_renderer_sdl_colmod_switch_count = 0;/* SDL_SetTextureColorMod/AlphaMod calls that actually change state */
static int platform_renderer_sdl_alpha_skip_count = 0;   /* draw_point/draw_coloured_point skipped due to alpha=0 */
static int platform_renderer_sdl_add_draw_count = 0;     /* draws submitted with BLEND_MODE_ADD blend */
static int platform_renderer_sdl_spec_draw_count = 0;    /* draws submitted with non-alpha blend (ADD+MUL+SUB) */
static int platform_renderer_sdl_defer_draw_count = 0;   /* draws pushed to deferred ADD batch queue */
/* --- deferred ADD-blend draw queue ------------------------------------ *
 * Textured draws using ADD blend are buffered here then flushed sorted  *
 * by texture pointer at frame end.  Eliminates interleaved ADD/non-ADD  *
 * blend transitions that each force a GPU batch submit on Panfrost.     */
#define PLATFORM_RENDERER_ADDQ_MAX 512
#define PLATFORM_RENDERER_ADDQ_MAX_VERTS (PLATFORM_RENDERER_ADDQ_MAX * 8)
#define PLATFORM_RENDERER_ADDQ_MAX_INDICES (PLATFORM_RENDERER_ADDQ_MAX * 18)
#define PLATFORM_RENDERER_ADDQ_ATLAS_MAX_TEXTURES 64
#define PLATFORM_RENDERER_MAX_GEOMETRY_VERTICES 128
#define PLATFORM_RENDERER_MAX_GEOMETRY_INDICES 192
#define PLATFORM_RENDERER_GEOM_ATLAS_WIDTH 2048
#define PLATFORM_RENDERER_GEOM_ATLAS_HEIGHT 2048
#define PLATFORM_RENDERER_GEOM_ATLAS_MAX_SIDE 512
#define PLATFORM_RENDERER_GEOM_ATLAS_MAX_AREA 131072
struct platform_renderer_addq_entry
{
	SDL_Texture *tex;
	SDL_Vertex   verts[8];
	int          indices[18];
	int          vertex_count;
	int          index_count;
	int          source_id;
};
static platform_renderer_addq_entry platform_renderer_addq[PLATFORM_RENDERER_ADDQ_MAX];
static int platform_renderer_addq_count = 0;
static SDL_Vertex platform_renderer_addq_flush_verts[PLATFORM_RENDERER_ADDQ_MAX_VERTS];
static int platform_renderer_addq_flush_indices[PLATFORM_RENDERER_ADDQ_MAX_INDICES];
struct platform_renderer_addq_atlas_slot
{
	SDL_Texture *source_tex;
	int x;
	int y;
	int w;
	int h;
};
static SDL_Texture *platform_renderer_addq_atlas_texture = NULL;
static int platform_renderer_addq_atlas_width = 0;
static int platform_renderer_addq_atlas_height = 0;
static int platform_renderer_addq_atlas_slot_count = 0;
static platform_renderer_addq_atlas_slot platform_renderer_addq_atlas_slots[PLATFORM_RENDERER_ADDQ_ATLAS_MAX_TEXTURES];
struct platform_renderer_geom_atlas_item
{
	int entry_index;
	int width;
	int height;
	int area;
};
static SDL_Texture *platform_renderer_geom_atlas_texture = NULL;
static int platform_renderer_geom_atlas_texture_count = 0;
static bool platform_renderer_geom_atlas_dirty = true;
static bool platform_renderer_geom_atlas_disabled = false;
static SDL_Texture *platform_renderer_last_geom_texture = NULL; /* last texture argument to SDL_RenderGeometry */
static SDL_Rect platform_renderer_sdl_clip_cache = {-1,-1,-1,-1};
static bool platform_renderer_sdl_clip_cache_enabled = false;
static int platform_renderer_sdl_output_width = 0;
static int platform_renderer_sdl_output_height = 0;
static float platform_renderer_sdl_scale_x = 1.0f;
static float platform_renderer_sdl_scale_y = 1.0f;
static bool platform_renderer_sdl_renderer_software_forced = false;
static bool platform_renderer_sdl_renderer_info_logged = false;
static bool platform_renderer_sdl_mode_info_logged = false;
static bool platform_renderer_legacy_submission_mode_checked_env = false;
static bool platform_renderer_legacy_submission_mode_enabled = true;
static bool platform_renderer_legacy_submission_mode_forced_on = false;
static bool platform_renderer_legacy_submission_mode_forced_off = false;
static bool platform_renderer_legacy_safe_off_latched = false;
static bool platform_renderer_legacy_safe_off_start_submission_off = false;
static int platform_renderer_legacy_safe_off_required_streak = 240;
static int platform_renderer_legacy_safe_off_regression_streak = 0;
static int platform_renderer_legacy_safe_off_rollback_streak = 6;
static unsigned int platform_renderer_legacy_safe_off_progress_log_counter = 0;
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
static int platform_renderer_sdl_native_draw_sources[PLATFORM_RENDERER_GEOM_SRC_COUNT] = {0};
static int platform_renderer_legacy_draw_sources[PLATFORM_RENDERER_GEOM_SRC_COUNT] = {0};
static int platform_renderer_sdl_source_bounds_count[PLATFORM_RENDERER_GEOM_SRC_COUNT] = {0};
static float platform_renderer_sdl_source_min_x[PLATFORM_RENDERER_GEOM_SRC_COUNT] = {0.0f};
static float platform_renderer_sdl_source_min_y[PLATFORM_RENDERER_GEOM_SRC_COUNT] = {0.0f};
static float platform_renderer_sdl_source_max_x[PLATFORM_RENDERER_GEOM_SRC_COUNT] = {0.0f};
static float platform_renderer_sdl_source_max_y[PLATFORM_RENDERER_GEOM_SRC_COUNT] = {0.0f};
static int platform_renderer_legacy_viewport_last[4] = {0, 0, 0, 0};
static int platform_renderer_legacy_scissor_last[4] = {0, 0, 0, 0};
static int platform_renderer_legacy_scissor_enabled_last = 0;
static int platform_renderer_legacy_draw_buffer_last = 0;
static int platform_renderer_legacy_read_buffer_last = 0;
static int platform_renderer_legacy_error_last = 0;
static int platform_renderer_legacy_diag_first_error_code = 0;
static unsigned int platform_renderer_legacy_diag_error_count_frame = 0;
static char platform_renderer_legacy_diag_first_error_stage[64] = "-";
static int platform_renderer_sdl_native_coverage_streak = 0;
static int platform_renderer_sdl_no_mirror_cooldown_frames = 0;
static bool platform_renderer_sdl_no_mirror_blocked_for_session = false;
static int platform_renderer_present_width = 0;
static int platform_renderer_present_height = 0;
static bool platform_renderer_started_sdl_video = false;
static char platform_renderer_sdl_status[256] = "SDL2 stub not checked.";
static unsigned int platform_renderer_sdl_sprite_trace_counter = 0;
static bool platform_renderer_sdl_subtractive_texture_supported = true;
static bool platform_renderer_sdl_subtractive_texture_support_checked = false;
static bool platform_renderer_sdl_multiply_texture_supported = true;
static bool platform_renderer_sdl_multiply_texture_support_checked = false;
static bool platform_renderer_sdl_diag_verbose_enabled = false;
static unsigned int platform_renderer_sdl_present_frame_counter = 0;
static SDL_Texture *platform_renderer_white_texture = NULL;  /* 1x1 white RGBA; routes colored-primitive calls through RenderGeometry for SDL batch coherence */
static bool platform_renderer_sdl_frame_log_enabled = false;  /* enabled by WIZBALL_SDL2_DIAG=1 */
static int platform_renderer_prev_sdl_draw_count = 0;
static int platform_renderer_prev_sdl_textured_count = 0;
static int platform_renderer_prev_legacy_draw_count = 0;
static int platform_renderer_prev_legacy_textured_count = 0;
static unsigned int platform_renderer_draw_transition_counter = 0;
static Uint32 s_frame_clear_ticks = 0; /* timestamp of last clear_backbuffer, used for render-phase timing */

// SDL Magic Pink setup
static Uint8 MAGIC_R = 255;
static Uint8 MAGIC_G = 0;
static Uint8 MAGIC_B = 255;

static void PLATFORM_RENDERER_refresh_sdl_stub_env_flags(void);
static int PLATFORM_RENDERER_find_texture_index_from_legacy_id(unsigned int legacy_texture_id);
static platform_renderer_texture_entry *PLATFORM_RENDERER_find_texture_entry_from_sdl_texture(SDL_Texture *texture);
static int PLATFORM_RENDERER_geom_atlas_item_cmp(const void *a, const void *b);

static void PLATFORM_RENDERER_log_native_draw_transition(
		unsigned int frame_number,
		int prev_sdl_draws,
		int prev_sdl_textured,
		int prev_legacy_draws,
		int prev_legacy_textured,
		int current_sdl_draws,
		int current_sdl_textured,
		int current_legacy_draws,
		int current_legacy_textured)
{
	const bool now_zero = (current_sdl_draws == 0) && (current_sdl_textured == 0);
	const bool was_zero = (prev_sdl_draws == 0) && (prev_sdl_textured == 0);
	int active_windows = 0;
	int pending_active_windows = 0;
	int skipped_windows = 0;
	int windows_with_nonempty_z = 0;
	int first_nonempty_window = UNSET;
	int first_nonempty_z = UNSET;
	int window_index;

	if (now_zero == was_zero)
	{
		return;
	}

	if (window_details != NULL)
	{
		for (window_index = 0; window_index < number_of_windows; window_index++)
		{
			window_struct *wp = &window_details[window_index];
			int z;

			if (wp->active)
			{
				active_windows++;
			}
			if (wp->new_active)
			{
				pending_active_windows++;
			}
			if (wp->skip_me_this_frame > 0)
			{
				skipped_windows++;
			}

			if ((wp->z_ordering_list_starts != NULL) && (wp->z_ordering_list_size > 0))
			{
				for (z = 0; z < wp->z_ordering_list_size; z++)
				{
					if (wp->z_ordering_list_starts[z] != UNSET)
					{
						windows_with_nonempty_z++;
						if (first_nonempty_window == UNSET)
						{
							first_nonempty_window = window_index;
							first_nonempty_z = z;
						}
						break;
					}
				}
			}
		}
	}

	platform_renderer_draw_transition_counter++;
	(void)frame_number;
	(void)prev_legacy_draws;
	(void)prev_legacy_textured;
	(void)current_legacy_draws;
	(void)current_legacy_textured;
}

static bool PLATFORM_RENDERER_env_enabled(const char *name);

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

static bool PLATFORM_RENDERER_using_subtractive_mod_fallback(void)
{
	if (!platform_renderer_blend_enabled ||
			(platform_renderer_blend_mode != PLATFORM_RENDERER_BLEND_MODE_SUBTRACT))
	{
		return false;
	}

	return platform_renderer_sdl_subtractive_texture_support_checked &&
				 (!platform_renderer_sdl_subtractive_texture_supported);
}

static bool PLATFORM_RENDERER_using_multiply_blend_fallback(void)
{
	if (!platform_renderer_blend_enabled ||
			(platform_renderer_blend_mode != PLATFORM_RENDERER_BLEND_MODE_MULTIPLY))
	{
		return false;
	}

	return platform_renderer_sdl_multiply_texture_support_checked &&
				 (!platform_renderer_sdl_multiply_texture_supported);
}

static void PLATFORM_RENDERER_refresh_sdl_stub_env_flags(void)
{
	platform_renderer_sdl_diag_verbose_enabled = false;
	{
		const char *diag_env = getenv("WIZBALL_SDL2_DIAG");
		platform_renderer_sdl_frame_log_enabled = (diag_env != NULL) && PLATFORM_RENDERER_env_enabled("WIZBALL_SDL2_DIAG");
	}
	if (!platform_renderer_sdl_stub_show_checked_env)
	{
		const char *show_env = getenv("WIZBALL_SDL2_STUB_SHOW");
		platform_renderer_sdl_stub_show_enabled = (show_env == NULL) ? true : PLATFORM_RENDERER_env_enabled("WIZBALL_SDL2_STUB_SHOW");
		platform_renderer_sdl_stub_show_checked_env = true;
	}
	if (!platform_renderer_sdl_stub_accel_checked_env)
	{
		const char *accel_env = getenv("WIZBALL_SDL2_STUB_ACCELERATED");
		/* Default to accelerated; set WIZBALL_SDL2_STUB_ACCELERATED=0 to force software. */
		platform_renderer_sdl_stub_accel_enabled = (accel_env == NULL) ? true : PLATFORM_RENDERER_env_enabled("WIZBALL_SDL2_STUB_ACCELERATED");
		platform_renderer_sdl_stub_accel_checked_env = true;
	}
	platform_renderer_sdl_native_primary_strict_enabled = true;
	platform_renderer_sdl_native_primary_force_no_mirror_enabled = true;
	platform_renderer_sdl_core_quads_enabled = true;
	platform_renderer_sdl_no_mirror_min_draws = 24;
	platform_renderer_sdl_no_mirror_min_textured = 16;
	platform_renderer_sdl_no_mirror_required_streak = 30;

	if (!platform_renderer_sdl_geometry_checked_env)
	{
		const char *geometry_env = getenv("WIZBALL_SDL2_GEOMETRY");
		if (geometry_env != NULL)
		{
			platform_renderer_sdl_geometry_enabled = PLATFORM_RENDERER_env_enabled("WIZBALL_SDL2_GEOMETRY");
		}
		else
		{
			platform_renderer_sdl_geometry_enabled = true;
		}
		platform_renderer_sdl_geometry_checked_env = true;
	}
}

static bool PLATFORM_RENDERER_is_sdl_geometry_enabled(void)
{
	PLATFORM_RENDERER_refresh_sdl_stub_env_flags();
	return platform_renderer_sdl_geometry_enabled;
}

static void PLATFORM_RENDERER_note_sdl_draw_source(int source_id)
{
	if ((source_id > 0) && (source_id < PLATFORM_RENDERER_GEOM_SRC_COUNT))
	{
		platform_renderer_sdl_native_draw_sources[source_id]++;
	}
}

static void PLATFORM_RENDERER_record_sdl_source_bounds_rect(int source_id, const SDL_Rect *rect)
{
	float min_x;
	float min_y;
	float max_x;
	float max_y;

	if ((source_id <= 0) || (source_id >= PLATFORM_RENDERER_GEOM_SRC_COUNT) || (rect == NULL))
	{
		return;
	}

	min_x = (float)rect->x;
	min_y = (float)rect->y;
	max_x = (float)(rect->x + rect->w);
	max_y = (float)(rect->y + rect->h);
	if (platform_renderer_sdl_source_bounds_count[source_id] <= 0)
	{
		platform_renderer_sdl_source_min_x[source_id] = min_x;
		platform_renderer_sdl_source_min_y[source_id] = min_y;
		platform_renderer_sdl_source_max_x[source_id] = max_x;
		platform_renderer_sdl_source_max_y[source_id] = max_y;
		platform_renderer_sdl_source_bounds_count[source_id] = 1;
		return;
	}

	if (min_x < platform_renderer_sdl_source_min_x[source_id])
		platform_renderer_sdl_source_min_x[source_id] = min_x;
	if (min_y < platform_renderer_sdl_source_min_y[source_id])
		platform_renderer_sdl_source_min_y[source_id] = min_y;
	if (max_x > platform_renderer_sdl_source_max_x[source_id])
		platform_renderer_sdl_source_max_x[source_id] = max_x;
	if (max_y > platform_renderer_sdl_source_max_y[source_id])
		platform_renderer_sdl_source_max_y[source_id] = max_y;
	platform_renderer_sdl_source_bounds_count[source_id]++;
}

static bool PLATFORM_RENDERER_sync_sdl_render_target_size(int width, int height)
{
	if ((platform_renderer_sdl_renderer == NULL) || (width <= 0) || (height <= 0))
	{
		return false;
	}

	(void)SDL_RenderSetViewport(platform_renderer_sdl_renderer, NULL);
	(void)SDL_RenderSetClipRect(platform_renderer_sdl_renderer, NULL);
	(void)SDL_RenderSetIntegerScale(platform_renderer_sdl_renderer, SDL_FALSE);
	{
		int out_w = 0;
		int out_h = 0;
		float sx = 1.0f;
		float sy = 1.0f;
		if (SDL_GetRendererOutputSize(platform_renderer_sdl_renderer, &out_w, &out_h) == 0)
		{
			platform_renderer_sdl_output_width = out_w;
			platform_renderer_sdl_output_height = out_h;
			if ((out_w > 0) && (out_h > 0))
			{
				sx = (float)out_w / (float)width;
				sy = (float)out_h / (float)height;
			}
		}
		platform_renderer_sdl_scale_x = sx;
		platform_renderer_sdl_scale_y = sy;
	}

	return true;
}

static float PLATFORM_RENDERER_convert_v_to_sdl(float v)
{
	return 1.0f - v;
}

static bool PLATFORM_RENDERER_src_flip_y_for_sdl(float v1, float v2)
{
	return PLATFORM_RENDERER_convert_v_to_sdl(v2) < PLATFORM_RENDERER_convert_v_to_sdl(v1);
}

static void PLATFORM_RENDERER_axis_orientation_flips_from_transformed_quad(
		const float tx[4],
		const float ty[4],
		bool *out_flip_x,
		bool *out_flip_y)
{
	const float axis_eps = 0.01f;
	bool flip_x = false;
	bool flip_y = false;

	if ((tx != NULL) && (ty != NULL))
	{
		/*
		 * For axis-aligned quads, detect whether transformed vertex ordering
		 * implies an implicit horizontal/vertical inversion (e.g. 180-degree
		 * rotation collapsing to a rectangle). This must be combined with UV
		 * flips to match GL winding/orientation.
		 */
		flip_x = (tx[3] + axis_eps) < tx[0];
		flip_y = (ty[1] - axis_eps) > ty[0];
	}

	if (out_flip_x != NULL)
	{
		*out_flip_x = flip_x;
	}
	if (out_flip_y != NULL)
	{
		*out_flip_y = flip_y;
	}
}

static bool PLATFORM_RENDERER_try_sdl_geometry_textured_fallback(SDL_Texture *texture, const SDL_Vertex *vertices, int vertex_count, const int *indices, int index_count, int source_id)
{
	(void)indices;
	(void)index_count;
	if (platform_renderer_sdl_native_primary_strict_enabled)
	{
		/*
		 * In strict native mode we avoid copy/copyex rectangle fallback entirely:
		 * it causes UV/alpha mismatches (e.g. rectangular shadows) versus GL.
		 */
		return false;
	}
	if ((texture == NULL) || (vertices == NULL) || (vertex_count != 4))
	{
		return false;
	}

	float min_x = vertices[0].position.x;
	float max_x = vertices[0].position.x;
	float min_y = vertices[0].position.y;
	float max_y = vertices[0].position.y;
	float min_u = vertices[0].tex_coord.x;
	float max_u = vertices[0].tex_coord.x;
	float min_v = PLATFORM_RENDERER_convert_v_to_sdl(vertices[0].tex_coord.y);
	float max_v = min_v;
	int i;
	for (i = 1; i < 4; i++)
	{
		if (vertices[i].position.x < min_x)
			min_x = vertices[i].position.x;
		if (vertices[i].position.x > max_x)
			max_x = vertices[i].position.x;
		if (vertices[i].position.y < min_y)
			min_y = vertices[i].position.y;
		if (vertices[i].position.y > max_y)
			max_y = vertices[i].position.y;
		if (vertices[i].tex_coord.x < min_u)
			min_u = vertices[i].tex_coord.x;
		if (vertices[i].tex_coord.x > max_u)
			max_u = vertices[i].tex_coord.x;
		{
			const float sv = PLATFORM_RENDERER_convert_v_to_sdl(vertices[i].tex_coord.y);
			if (sv < min_v)
				min_v = sv;
			if (sv > max_v)
				max_v = sv;
		}
	}

	if (!isfinite(min_x) || !isfinite(max_x) || !isfinite(min_y) || !isfinite(max_y) || !isfinite(min_u) || !isfinite(max_u) || !isfinite(min_v) || !isfinite(max_v))
	{
		return false;
	}

	SDL_RendererFlip final_flip = SDL_FLIP_NONE;
	if (vertices[2].tex_coord.x < vertices[0].tex_coord.x)
	{
		final_flip = (SDL_RendererFlip)(final_flip | SDL_FLIP_HORIZONTAL);
	}
	if (vertices[1].tex_coord.y > vertices[0].tex_coord.y)
	{
		final_flip = (SDL_RendererFlip)(final_flip | SDL_FLIP_VERTICAL);
	}

	SDL_Rect src_rect;
	src_rect.x = (int)(min_u * 4096.0f);
	src_rect.y = (int)(min_v * 4096.0f);
	src_rect.w = (int)((max_u - min_u) * 4096.0f);
	src_rect.h = (int)((max_v - min_v) * 4096.0f);
	if (src_rect.w <= 0)
		src_rect.w = 1;
	if (src_rect.h <= 0)
		src_rect.h = 1;

	int tex_w = 0;
	int tex_h = 0;
	if (SDL_QueryTexture(texture, NULL, NULL, &tex_w, &tex_h) != 0 || tex_w <= 0 || tex_h <= 0)
	{
		return false;
	}
	src_rect.x = (int)(min_u * (float)tex_w);
	src_rect.y = (int)(min_v * (float)tex_h);
	src_rect.w = (int)((max_u - min_u) * (float)tex_w);
	src_rect.h = (int)((max_v - min_v) * (float)tex_h);
	if (src_rect.x < 0)
		src_rect.x = 0;
	if (src_rect.y < 0)
		src_rect.y = 0;
	if (src_rect.x >= tex_w)
		src_rect.x = tex_w - 1;
	if (src_rect.y >= tex_h)
		src_rect.y = tex_h - 1;
	if (src_rect.w <= 0)
		src_rect.w = 1;
	if (src_rect.h <= 0)
		src_rect.h = 1;
	if (src_rect.x + src_rect.w > tex_w)
		src_rect.w = tex_w - src_rect.x;
	if (src_rect.y + src_rect.h > tex_h)
		src_rect.h = tex_h - src_rect.y;
	if (src_rect.w <= 0)
		src_rect.w = 1;
	if (src_rect.h <= 0)
		src_rect.h = 1;

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
			const double angle_degrees = atan2((double)exy, (double)exx) * (180.0 / 3.14159265358979323846);

			dst_rect.w = (int)(width + 0.5f);
			dst_rect.h = (int)(height + 0.5f);
			if (dst_rect.w <= 0)
				dst_rect.w = 1;
			if (dst_rect.h <= 0)
				dst_rect.h = 1;
			dst_rect.x = (int)(cx - ((float)dst_rect.w * 0.5f));
			dst_rect.y = (int)(cy - ((float)dst_rect.h * 0.5f));

			center.x = dst_rect.w / 2;
			center.y = dst_rect.h / 2;

			PLATFORM_RENDERER_apply_sdl_texture_blend_mode(texture);
			PLATFORM_RENDERER_set_texture_color_alpha_cached(texture, vertices[0].color.r, vertices[0].color.g, vertices[0].color.b, PLATFORM_RENDERER_get_sdl_texture_alpha_mod(vertices[0].color.a));
			if (SDL_RenderCopyEx(platform_renderer_sdl_renderer, texture, &src_rect, &dst_rect, angle_degrees, &center, final_flip) == 0)
			{
				platform_renderer_sdl_native_draw_count++;
				platform_renderer_sdl_native_textured_draw_count++;
				PLATFORM_RENDERER_note_sdl_draw_source(source_id);
				PLATFORM_RENDERER_record_sdl_source_bounds_rect(source_id, &dst_rect);
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
	dst_rect.x = (int)min_x;
	dst_rect.y = (int)min_y;
	dst_rect.w = (int)(max_x - min_x);
	dst_rect.h = (int)(max_y - min_y);
	if (dst_rect.w <= 0)
		dst_rect.w = 1;
	if (dst_rect.h <= 0)
		dst_rect.h = 1;

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

	PLATFORM_RENDERER_apply_sdl_texture_blend_mode(texture);
	PLATFORM_RENDERER_set_texture_color_alpha_cached(texture, vertices[0].color.r, vertices[0].color.g, vertices[0].color.b, PLATFORM_RENDERER_get_sdl_texture_alpha_mod(vertices[0].color.a));
	if (PLATFORM_RENDERER_sdl_copy_ex(platform_renderer_sdl_renderer, texture, &src_rect, &dst_rect, 0.0, NULL, final_flip) == 0)
	{
		platform_renderer_sdl_native_draw_count++;
		platform_renderer_sdl_native_textured_draw_count++;
		PLATFORM_RENDERER_note_sdl_draw_source(source_id);
		PLATFORM_RENDERER_record_sdl_source_bounds_rect(source_id, &dst_rect);
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
	SDL_Vertex converted_vertices[PLATFORM_RENDERER_MAX_GEOMETRY_VERTICES];
	const SDL_Vertex *work_vertices = vertices;
	SDL_Texture *draw_texture = texture;
	int v;

	if ((texture != NULL) &&
			PLATFORM_RENDERER_using_multiply_blend_fallback() &&
			((source_id == PLATFORM_RENDERER_GEOM_SRC_BOUND_QUAD) ||
			 (source_id == PLATFORM_RENDERER_GEOM_SRC_BOUND_QUAD_CUSTOM)))
	{
		/*
		 * Software SDL backends can produce opaque matte artefacts for textured
		 * geometry in multiply-fallback mode. Force copy/copyex fallback.
		 */
		return false;
	}

	if ((vertices == NULL) || (indices == NULL) ||
			(vertex_count <= 0) || (vertex_count > PLATFORM_RENDERER_MAX_GEOMETRY_VERTICES) ||
			(index_count <= 0) || (index_count > PLATFORM_RENDERER_MAX_GEOMETRY_INDICES))
	{
		return false;
	}

	if ((vertices != NULL) && (vertex_count > 0) && (vertex_count <= PLATFORM_RENDERER_MAX_GEOMETRY_VERTICES))
	{
		for (v = 0; v < vertex_count; v++)
		{
			float su, sv;
			converted_vertices[v] = vertices[v];
			converted_vertices[v].tex_coord.y = PLATFORM_RENDERER_convert_v_to_sdl(vertices[v].tex_coord.y);
			/*
			 * SDL 2.28.5 GLES2 backend rejects RenderGeometry calls where any UV
			 * is outside [0,1], logging "Values of 'uv' out of bounds". This happens
			 * for perspective tunnel quads near the screen extremes where texture-space
			 * coordinates wrap past the texture edge. Clamping here keeps those quads
			 * in the batch (rather than falling back to RenderCopyEx which flushes the
			 * entire pending command queue).
			 */
			su = converted_vertices[v].tex_coord.x;
			sv = converted_vertices[v].tex_coord.y;
			if (su < 0.0f) su = 0.0f; else if (su > 1.0f) su = 1.0f;
			if (sv < 0.0f) sv = 0.0f; else if (sv > 1.0f) sv = 1.0f;
			converted_vertices[v].tex_coord.x = su;
			converted_vertices[v].tex_coord.y = sv;
		}
		work_vertices = converted_vertices;
	}

	if ((draw_texture != NULL) && (work_vertices == converted_vertices))
	{
		(void)PLATFORM_RENDERER_try_remap_geometry_texture_to_atlas(&draw_texture, converted_vertices, vertex_count);
	}

	if (PLATFORM_RENDERER_should_defer_add_geometry(draw_texture, vertex_count, indices, index_count, source_id))
	{
		platform_renderer_addq_entry *e = &platform_renderer_addq[platform_renderer_addq_count++];
		platform_renderer_sdl_add_draw_count++;
		platform_renderer_sdl_spec_draw_count++;
		e->tex = draw_texture;
		e->vertex_count = vertex_count;
		e->index_count = index_count;
		e->source_id = source_id;
		memcpy(e->verts, work_vertices, (size_t)vertex_count * sizeof(SDL_Vertex));
		memcpy(e->indices, indices, (size_t)index_count * sizeof(int));
		platform_renderer_sdl_defer_draw_count++;
		return true;
	}

	if (!PLATFORM_RENDERER_is_sdl_geometry_enabled())
	{
		const bool force_coloured_geometry =
				(source_id == PLATFORM_RENDERER_GEOM_SRC_PERSPECTIVE) ||
				(source_id == PLATFORM_RENDERER_GEOM_SRC_COLOURED_ARRAY) ||
				(source_id == PLATFORM_RENDERER_GEOM_SRC_COLOURED_PERSPECTIVE);

		if (force_coloured_geometry && (texture != NULL))
		{
			PLATFORM_RENDERER_apply_sdl_texture_blend_mode(draw_texture);
			if (SDL_RenderGeometry(platform_renderer_sdl_renderer, draw_texture, work_vertices, vertex_count, indices, index_count) == 0)
			{
				SDL_Rect geom_bounds;
				float min_x = work_vertices[0].position.x;
				float min_y = work_vertices[0].position.y;
				float max_x = work_vertices[0].position.x;
				float max_y = work_vertices[0].position.y;
				for (v = 1; v < vertex_count; v++)
				{
					if (work_vertices[v].position.x < min_x)
						min_x = work_vertices[v].position.x;
					if (work_vertices[v].position.y < min_y)
						min_y = work_vertices[v].position.y;
					if (work_vertices[v].position.x > max_x)
						max_x = work_vertices[v].position.x;
					if (work_vertices[v].position.y > max_y)
						max_y = work_vertices[v].position.y;
				}
				geom_bounds.x = (int)min_x;
				geom_bounds.y = (int)min_y;
				geom_bounds.w = (int)(max_x - min_x);
				geom_bounds.h = (int)(max_y - min_y);
				if (geom_bounds.w <= 0)
					geom_bounds.w = 1;
				if (geom_bounds.h <= 0)
					geom_bounds.h = 1;
				platform_renderer_sdl_native_draw_count++;
				platform_renderer_sdl_native_textured_draw_count++;
				PLATFORM_RENDERER_note_sdl_draw_source(source_id);
				PLATFORM_RENDERER_record_sdl_source_bounds_rect(source_id, &geom_bounds);
				return true;
			}
		}

		if (texture == NULL)
		{
			// Keep untextured geometry active even when textured-geometry mode is off.
			// This preserves coloured/perspective primitives used by UI/gameplay layers.
			if (SDL_RenderGeometry(platform_renderer_sdl_renderer, NULL, work_vertices, vertex_count, indices, index_count) == 0)
			{
				platform_renderer_sdl_native_draw_count++;
				return true;
			}
		}
		if (PLATFORM_RENDERER_try_sdl_geometry_textured_fallback(texture, work_vertices, vertex_count, indices, index_count, source_id))
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

	if (draw_texture != NULL)
	{
		PLATFORM_RENDERER_apply_sdl_texture_blend_mode(draw_texture);
	}
	if (SDL_RenderGeometry(platform_renderer_sdl_renderer, draw_texture, work_vertices, vertex_count, indices, index_count) == 0)
	{
		SDL_Rect geom_bounds;
		float min_x = work_vertices[0].position.x;
		float min_y = work_vertices[0].position.y;
		float max_x = work_vertices[0].position.x;
		float max_y = work_vertices[0].position.y;
		for (v = 1; v < vertex_count; v++)
		{
			if (work_vertices[v].position.x < min_x)
				min_x = work_vertices[v].position.x;
			if (work_vertices[v].position.y < min_y)
				min_y = work_vertices[v].position.y;
			if (work_vertices[v].position.x > max_x)
				max_x = work_vertices[v].position.x;
			if (work_vertices[v].position.y > max_y)
				max_y = work_vertices[v].position.y;
		}
		geom_bounds.x = (int)min_x;
		geom_bounds.y = (int)min_y;
		geom_bounds.w = (int)(max_x - min_x);
		geom_bounds.h = (int)(max_y - min_y);
		if (geom_bounds.w <= 0)
			geom_bounds.w = 1;
		if (geom_bounds.h <= 0)
			geom_bounds.h = 1;
		if (draw_texture != platform_renderer_last_geom_texture)
		{
			platform_renderer_sdl_tx_switch_count++;
			platform_renderer_last_geom_texture = draw_texture;
		}
		platform_renderer_sdl_native_draw_count++;
		platform_renderer_sdl_native_textured_draw_count++;
		PLATFORM_RENDERER_note_sdl_draw_source(source_id);
		PLATFORM_RENDERER_record_sdl_source_bounds_rect(source_id, &geom_bounds);
		return true;
	}

	{
		static int rg_fail_logged = 0;
		if (rg_fail_logged < 5)
		{
			rg_fail_logged++;
			fprintf(stderr, "[GEOM-FAIL %d] SDL_RenderGeometry returned non-zero (src=%d): %s\n",
				rg_fail_logged, source_id, SDL_GetError());
		}
	}
	if (PLATFORM_RENDERER_try_sdl_geometry_textured_fallback(texture, work_vertices, vertex_count, indices, index_count, source_id))
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

	fprintf(stderr, "Building SDL texture for legacy ID %u (size %dx%d, Gotpixels: %d)\n", entry->legacy_texture_id, entry->width, entry->height, (entry->sdl_rgba_pixels == NULL));

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

static SDL_Texture *PLATFORM_RENDERER_build_sdl_inverted_texture_from_entry(platform_renderer_texture_entry *entry)
{
	unsigned char *inverted_pixels = NULL;
	size_t pixel_count;
	size_t byte_count;
	size_t i;

	if ((entry == NULL) || (platform_renderer_sdl_renderer == NULL))
	{
		return NULL;
	}
	if (entry->sdl_texture_inverted != NULL)
	{
		return entry->sdl_texture_inverted;
	}
	if ((entry->sdl_rgba_pixels == NULL) || (entry->width <= 0) || (entry->height <= 0))
	{
		return NULL;
	}

	pixel_count = (size_t)entry->width * (size_t)entry->height;
	byte_count = pixel_count * 4u;
	inverted_pixels = (unsigned char *)malloc(byte_count);
	if (inverted_pixels == NULL)
	{
		return NULL;
	}

	for (i = 0; i < pixel_count; i++)
	{
		size_t base = i * 4u;
		unsigned int a = (unsigned int)entry->sdl_rgba_pixels[base + 3];
		unsigned int src_r = ((unsigned int)entry->sdl_rgba_pixels[base + 0] * a) / 255u;
		unsigned int src_g = ((unsigned int)entry->sdl_rgba_pixels[base + 1] * a) / 255u;
		unsigned int src_b = ((unsigned int)entry->sdl_rgba_pixels[base + 2] * a) / 255u;

		/*
		 * Subtractive fallback uses MOD, so we feed 1-src into the texture.
		 * Weight by alpha first so fully transparent texels become neutral white
		 * (no dark rectangle around masked sprites like lab shadows).
		 */
		inverted_pixels[base + 0] = (unsigned char)(255u - src_r);
		inverted_pixels[base + 1] = (unsigned char)(255u - src_g);
		inverted_pixels[base + 2] = (unsigned char)(255u - src_b);
		inverted_pixels[base + 3] = entry->sdl_rgba_pixels[base + 3];
	}

	entry->sdl_texture_inverted = SDL_CreateTexture(
			platform_renderer_sdl_renderer,
			SDL_PIXELFORMAT_RGBA32,
			SDL_TEXTUREACCESS_STATIC,
			entry->width,
			entry->height);
	if (entry->sdl_texture_inverted == NULL)
	{
		free(inverted_pixels);
		return NULL;
	}
	if (SDL_UpdateTexture(entry->sdl_texture_inverted, NULL, inverted_pixels, entry->width * 4) != 0)
	{
		SDL_DestroyTexture(entry->sdl_texture_inverted);
		entry->sdl_texture_inverted = NULL;
		free(inverted_pixels);
		return NULL;
	}
	free(inverted_pixels);
	(void)SDL_SetTextureBlendMode(entry->sdl_texture_inverted, SDL_BLENDMODE_MOD);
	return entry->sdl_texture_inverted;
}

static SDL_Texture *PLATFORM_RENDERER_build_sdl_premultiplied_texture_from_entry(platform_renderer_texture_entry *entry)
{
	unsigned char *premul_pixels = NULL;
	size_t pixel_count;
	size_t byte_count;
	size_t i;

	if ((entry == NULL) || (platform_renderer_sdl_renderer == NULL))
	{
		return NULL;
	}
	if (entry->sdl_texture_premultiplied != NULL)
	{
		return entry->sdl_texture_premultiplied;
	}
	if ((entry->sdl_rgba_pixels == NULL) || (entry->width <= 0) || (entry->height <= 0))
	{
		return NULL;
	}

	pixel_count = (size_t)entry->width * (size_t)entry->height;
	byte_count = pixel_count * 4u;
	premul_pixels = (unsigned char *)malloc(byte_count);
	if (premul_pixels == NULL)
	{
		return NULL;
	}

	for (i = 0; i < pixel_count; i++)
	{
		size_t base = i * 4u;
		unsigned int a = (unsigned int)entry->sdl_rgba_pixels[base + 3];
		unsigned int src_r = ((unsigned int)entry->sdl_rgba_pixels[base + 0] * a) / 255u;
		unsigned int src_g = ((unsigned int)entry->sdl_rgba_pixels[base + 1] * a) / 255u;
		unsigned int src_b = ((unsigned int)entry->sdl_rgba_pixels[base + 2] * a) / 255u;

		premul_pixels[base + 0] = (unsigned char)src_r;
		premul_pixels[base + 1] = (unsigned char)src_g;
		premul_pixels[base + 2] = (unsigned char)src_b;
		premul_pixels[base + 3] = entry->sdl_rgba_pixels[base + 3];
	}

	entry->sdl_texture_premultiplied = SDL_CreateTexture(
			platform_renderer_sdl_renderer,
			SDL_PIXELFORMAT_RGBA32,
			SDL_TEXTUREACCESS_STATIC,
			entry->width,
			entry->height);
	if (entry->sdl_texture_premultiplied == NULL)
	{
		free(premul_pixels);
		return NULL;
	}
	if (SDL_UpdateTexture(entry->sdl_texture_premultiplied, NULL, premul_pixels, entry->width * 4) != 0)
	{
		SDL_DestroyTexture(entry->sdl_texture_premultiplied);
		entry->sdl_texture_premultiplied = NULL;
		free(premul_pixels);
		return NULL;
	}
	free(premul_pixels);
	(void)SDL_SetTextureBlendMode(entry->sdl_texture_premultiplied, SDL_BLENDMODE_BLEND);
	return entry->sdl_texture_premultiplied;
}

static SDL_Texture *PLATFORM_RENDERER_get_effective_texture_for_current_blend(platform_renderer_texture_entry *entry)
{
	if (entry == NULL)
	{
		return NULL;
	}

	if (platform_renderer_blend_enabled &&
			(platform_renderer_blend_mode == PLATFORM_RENDERER_BLEND_MODE_SUBTRACT) &&
			((platform_renderer_sdl_subtractive_texture_support_checked &&
				(!platform_renderer_sdl_subtractive_texture_supported))))
	{
		SDL_Texture *inverted = PLATFORM_RENDERER_build_sdl_inverted_texture_from_entry(entry);
		if (inverted != NULL)
		{
			(void)SDL_SetTextureBlendMode(inverted, SDL_BLENDMODE_MOD);
			return inverted;
		}
	}

	if (platform_renderer_blend_enabled &&
			(platform_renderer_blend_mode == PLATFORM_RENDERER_BLEND_MODE_SUBTRACT))
	{
		SDL_Texture *premul = PLATFORM_RENDERER_build_sdl_premultiplied_texture_from_entry(entry);
		if (premul != NULL)
		{
			return premul;
		}
	}

	return entry->sdl_texture;
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
	float sdl_v1 = PLATFORM_RENDERER_convert_v_to_sdl(v1);
	float sdl_v2 = PLATFORM_RENDERER_convert_v_to_sdl(v2);
	float src_v_min = (sdl_v1 < sdl_v2) ? sdl_v1 : sdl_v2;
	float src_v_max = (sdl_v1 > sdl_v2) ? sdl_v1 : sdl_v2;
	float tex_w_f = (float)entry->width;
	float tex_h_f = (float)entry->height;
	const float uv_eps = 0.0001f;
	int src_x1 = (int)floorf((src_u_min * tex_w_f) + uv_eps);
	int src_x2 = (int)ceilf((src_u_max * tex_w_f) - uv_eps);
	int src_y1 = (int)floorf((src_v_min * tex_h_f) + uv_eps);
	int src_y2 = (int)ceilf((src_v_max * tex_h_f) - uv_eps);
	int src_x = src_x1;
	int src_y = src_y1;
	int src_w = src_x2 - src_x1;
	int src_h = src_y2 - src_y1;

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

static float PLATFORM_RENDERER_estimate_alpha_coverage(
		const platform_renderer_texture_entry *entry,
		const SDL_Rect *src_rect)
{
	int x;
	int y;
	int opaque = 0;
	int total = 0;

	if ((entry == NULL) || (entry->sdl_rgba_pixels == NULL) || (src_rect == NULL))
	{
		return -1.0f;
	}
	if ((src_rect->w <= 0) || (src_rect->h <= 0))
	{
		return -1.0f;
	}

	for (y = 0; y < src_rect->h; y++)
	{
		for (x = 0; x < src_rect->w; x++)
		{
			const int px = src_rect->x + x;
			const int py = src_rect->y + y;
			if ((px < 0) || (px >= entry->width) || (py < 0) || (py >= entry->height))
			{
				continue;
			}
			{
				const size_t idx = ((size_t)py * (size_t)entry->width + (size_t)px) * 4u + 3u;
				if (entry->sdl_rgba_pixels[idx] > 0u)
				{
					opaque++;
				}
				total++;
			}
		}
	}

	if (total <= 0)
	{
		return -1.0f;
	}
	return (float)opaque / (float)total;
}

static void PLATFORM_RENDERER_estimate_rect_colour_stats(
		const platform_renderer_texture_entry *entry,
		const SDL_Rect *src_rect,
		float *out_avg_r,
		float *out_avg_g,
		float *out_avg_b,
		float *out_avg_a)
{
	int x;
	int y;
	double sum_r = 0.0;
	double sum_g = 0.0;
	double sum_b = 0.0;
	double sum_a = 0.0;
	int total = 0;

	if (out_avg_r != NULL)
		*out_avg_r = -1.0f;
	if (out_avg_g != NULL)
		*out_avg_g = -1.0f;
	if (out_avg_b != NULL)
		*out_avg_b = -1.0f;
	if (out_avg_a != NULL)
		*out_avg_a = -1.0f;

	if ((entry == NULL) || (entry->sdl_rgba_pixels == NULL) || (src_rect == NULL) || (src_rect->w <= 0) || (src_rect->h <= 0))
	{
		return;
	}

	for (y = 0; y < src_rect->h; y++)
	{
		for (x = 0; x < src_rect->w; x++)
		{
			const int px = src_rect->x + x;
			const int py = src_rect->y + y;
			size_t idx;
			if ((px < 0) || (px >= entry->width) || (py < 0) || (py >= entry->height))
			{
				continue;
			}
			idx = ((size_t)py * (size_t)entry->width + (size_t)px) * 4u;
			sum_r += (double)entry->sdl_rgba_pixels[idx + 0u];
			sum_g += (double)entry->sdl_rgba_pixels[idx + 1u];
			sum_b += (double)entry->sdl_rgba_pixels[idx + 2u];
			sum_a += (double)entry->sdl_rgba_pixels[idx + 3u];
			total++;
		}
	}

	if (total > 0)
	{
		if (out_avg_r != NULL)
			*out_avg_r = (float)(sum_r / (double)total);
		if (out_avg_g != NULL)
			*out_avg_g = (float)(sum_g / (double)total);
		if (out_avg_b != NULL)
			*out_avg_b = (float)(sum_b / (double)total);
		if (out_avg_a != NULL)
			*out_avg_a = (float)(sum_a / (double)total);
	}
}

static float PLATFORM_RENDERER_wrap01(float value)
{
	float wrapped = fmodf(value, 1.0f);
	if (wrapped < 0.0f)
	{
		wrapped += 1.0f;
	}
	return wrapped;
}

static bool PLATFORM_RENDERER_draw_axis_wrapped_u_repeat(
		platform_renderer_texture_entry *entry,
		SDL_Texture *draw_texture,
		const SDL_Rect *dst_rect,
		float u1, float v1, float u2, float v2,
		Uint8 mod_r, Uint8 mod_g, Uint8 mod_b, Uint8 mod_a,
		int source_id)
{
	float du;
	float abs_du;
	int split_x;
	float t_split;
	SDL_Rect dst_a;
	SDL_Rect dst_b;
	float a_u1;
	float a_u2;
	float b_u1;
	float b_u2;

	if ((entry == NULL) || (draw_texture == NULL) || (dst_rect == NULL))
	{
		return false;
	}
	if ((dst_rect->w <= 0) || (dst_rect->h <= 0))
	{
		return false;
	}

	/*
	 * This path emulates GL_REPEAT for axis-aligned quads where U spans across a
	 * texture seam (common in scrolling level backgrounds). The current content
	 * only needs one seam split (|du| ~= 1.0), so keep the implementation tight.
	 */
	du = u2 - u1;
	abs_du = fabsf(du);
	if (abs_du < 0.00001f)
	{
		return false;
	}
	if (abs_du > 1.001f)
	{
		return false;
	}
	if ((v1 < 0.0f) || (v1 > 1.0f) || (v2 < 0.0f) || (v2 > 1.0f))
	{
		return false;
	}

	if ((floorf(u1) == floorf(u2)) && (u1 >= 0.0f) && (u1 <= 1.0f) && (u2 >= 0.0f) && (u2 <= 1.0f))
	{
		return false;
	}

	if (du > 0.0f)
	{
		float boundary = floorf(u1) + 1.0f;
		t_split = (boundary - u1) / du;
		if ((t_split <= 0.00001f) || (t_split >= 0.99999f))
		{
			return false;
		}

		a_u1 = PLATFORM_RENDERER_wrap01(u1);
		a_u2 = 1.0f;
		b_u1 = 0.0f;
		b_u2 = PLATFORM_RENDERER_wrap01(u2);
	}
	else
	{
		float boundary = floorf(u1);
		t_split = (u1 - boundary) / abs_du;
		if ((t_split <= 0.00001f) || (t_split >= 0.99999f))
		{
			return false;
		}

		a_u1 = PLATFORM_RENDERER_wrap01(u1);
		a_u2 = 0.0f;
		b_u1 = 1.0f;
		b_u2 = PLATFORM_RENDERER_wrap01(u2);
	}

	split_x = dst_rect->x + (int)floorf(((float)dst_rect->w * t_split) + 0.5f);
	if (split_x <= dst_rect->x)
	{
		split_x = dst_rect->x + 1;
	}
	if (split_x >= (dst_rect->x + dst_rect->w))
	{
		split_x = dst_rect->x + dst_rect->w - 1;
	}

	dst_a.x = dst_rect->x;
	dst_a.y = dst_rect->y;
	dst_a.w = split_x - dst_rect->x;
	dst_a.h = dst_rect->h;
	dst_b.x = split_x;
	dst_b.y = dst_rect->y;
	dst_b.w = (dst_rect->x + dst_rect->w) - split_x;
	dst_b.h = dst_rect->h;

	if ((dst_a.w <= 0) || (dst_b.w <= 0))
	{
		return false;
	}

	{
		SDL_Vertex vertices[4];
		int indices[6] = {0, 1, 2, 0, 2, 3};
		const bool a_flip_x = (a_u2 < a_u1);
		const bool b_flip_x = (b_u2 < b_u1);
		const bool flip_y = PLATFORM_RENDERER_src_flip_y_for_sdl(v1, v2);
		const float a_u_left = a_flip_x ? a_u2 : a_u1;
		const float a_u_right = a_flip_x ? a_u1 : a_u2;
		const float b_u_left = b_flip_x ? b_u2 : b_u1;
		const float b_u_right = b_flip_x ? b_u1 : b_u2;
		const float v_top = flip_y ? v2 : v1;
		const float v_bottom = flip_y ? v1 : v2;
		int before = platform_renderer_sdl_native_draw_count;

		vertices[0].position.x = (float)dst_a.x;
		vertices[0].position.y = (float)dst_a.y;
		vertices[1].position.x = (float)dst_a.x;
		vertices[1].position.y = (float)(dst_a.y + dst_a.h);
		vertices[2].position.x = (float)(dst_a.x + dst_a.w);
		vertices[2].position.y = (float)(dst_a.y + dst_a.h);
		vertices[3].position.x = (float)(dst_a.x + dst_a.w);
		vertices[3].position.y = (float)dst_a.y;
		vertices[0].tex_coord.x = a_u_left;
		vertices[0].tex_coord.y = v_top;
		vertices[1].tex_coord.x = a_u_left;
		vertices[1].tex_coord.y = v_bottom;
		vertices[2].tex_coord.x = a_u_right;
		vertices[2].tex_coord.y = v_bottom;
		vertices[3].tex_coord.x = a_u_right;
		vertices[3].tex_coord.y = v_top;
		vertices[0].color.r = vertices[1].color.r = vertices[2].color.r = vertices[3].color.r = mod_r;
		vertices[0].color.g = vertices[1].color.g = vertices[2].color.g = vertices[3].color.g = mod_g;
		vertices[0].color.b = vertices[1].color.b = vertices[2].color.b = vertices[3].color.b = mod_b;
		vertices[0].color.a = vertices[1].color.a = vertices[2].color.a = vertices[3].color.a = mod_a;
		if (!PLATFORM_RENDERER_try_sdl_geometry_textured(draw_texture, vertices, 4, indices, 6, source_id))
		{
			return false;
		}

		vertices[0].position.x = (float)dst_b.x;
		vertices[0].position.y = (float)dst_b.y;
		vertices[1].position.x = (float)dst_b.x;
		vertices[1].position.y = (float)(dst_b.y + dst_b.h);
		vertices[2].position.x = (float)(dst_b.x + dst_b.w);
		vertices[2].position.y = (float)(dst_b.y + dst_b.h);
		vertices[3].position.x = (float)(dst_b.x + dst_b.w);
		vertices[3].position.y = (float)dst_b.y;
		vertices[0].tex_coord.x = b_u_left;
		vertices[0].tex_coord.y = v_top;
		vertices[1].tex_coord.x = b_u_left;
		vertices[1].tex_coord.y = v_bottom;
		vertices[2].tex_coord.x = b_u_right;
		vertices[2].tex_coord.y = v_bottom;
		vertices[3].tex_coord.x = b_u_right;
		vertices[3].tex_coord.y = v_top;
		if (!PLATFORM_RENDERER_try_sdl_geometry_textured(draw_texture, vertices, 4, indices, 6, source_id))
		{
			return false;
		}

		if (platform_renderer_sdl_native_draw_count > before)
		{
			PLATFORM_RENDERER_note_sdl_draw_source(source_id);
			PLATFORM_RENDERER_record_sdl_source_bounds_rect(source_id, &dst_a);
			PLATFORM_RENDERER_record_sdl_source_bounds_rect(source_id, &dst_b);
			return true;
		}
	}
	return false;
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
	int ir = (int)(r * 255.0f);
	int ig = (int)(g * 255.0f);
	int ib = (int)(b * 255.0f);
	int ia = (int)(a * 255.0f);

	if (ir < 0)
		ir = 0;
	if (ir > 255)
		ir = 255;
	if (ig < 0)
		ig = 0;
	if (ig > 255)
		ig = 255;
	if (ib < 0)
		ib = 0;
	if (ib > 255)
		ib = 255;
	if (ia < 0)
		ia = 0;
	if (ia > 255)
		ia = 255;

	(void)SDL_SetRenderDrawColor(platform_renderer_sdl_renderer, (Uint8)ir, (Uint8)ig, (Uint8)ib, (Uint8)ia);
}

static bool PLATFORM_RENDERER_transform_state_is_finite(void)
{
	return isfinite(platform_renderer_tx_a) &&
				 isfinite(platform_renderer_tx_b) &&
				 isfinite(platform_renderer_tx_c) &&
				 isfinite(platform_renderer_tx_d) &&
				 isfinite(platform_renderer_tx_x) &&
				 isfinite(platform_renderer_tx_y);
}

static void PLATFORM_RENDERER_reset_transform_state(const char *reason)
{
	static unsigned int transform_reset_counter = 0;
	platform_renderer_tx_a = 1.0f;
	platform_renderer_tx_b = 0.0f;
	platform_renderer_tx_c = 0.0f;
	platform_renderer_tx_d = 1.0f;
	platform_renderer_tx_x = 0.0f;
	platform_renderer_tx_y = 0.0f;

	transform_reset_counter++;
	if ((transform_reset_counter <= 200) || ((transform_reset_counter % 1000) == 0))
	{
	}
}

static void PLATFORM_RENDERER_apply_sdl_draw_blend_mode(void)
{
	if (platform_renderer_addq_count > 0)
	{
		PLATFORM_RENDERER_flush_addq();
	}

	if ((platform_renderer_sdl_renderer == NULL) || !platform_renderer_blend_enabled)
	{
		if (platform_renderer_sdl_renderer != NULL)
		{
			(void)SDL_SetRenderDrawBlendMode(platform_renderer_sdl_renderer, SDL_BLENDMODE_NONE);
		}
		return;
	}

	switch (platform_renderer_blend_mode)
	{
	case PLATFORM_RENDERER_BLEND_MODE_ADD:
		(void)SDL_SetRenderDrawBlendMode(platform_renderer_sdl_renderer, SDL_BLENDMODE_ADD);
		break;
	case PLATFORM_RENDERER_BLEND_MODE_MULTIPLY:
		/*
		 * The original GL code used GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA for
		 * BLEND_MULTIPLY — i.e. standard alpha blending despite the name.
		 * Use SDL_BLENDMODE_BLEND to match.
		 */
		(void)SDL_SetRenderDrawBlendMode(platform_renderer_sdl_renderer, SDL_BLENDMODE_BLEND);
		break;
	case PLATFORM_RENDERER_BLEND_MODE_SUBTRACT:
	{
		SDL_BlendMode subtract_mode = SDL_ComposeCustomBlendMode(
				SDL_BLENDFACTOR_ZERO, SDL_BLENDFACTOR_ONE_MINUS_SRC_COLOR, SDL_BLENDOPERATION_ADD,
				SDL_BLENDFACTOR_ZERO, SDL_BLENDFACTOR_ONE, SDL_BLENDOPERATION_ADD);
		if (SDL_SetRenderDrawBlendMode(platform_renderer_sdl_renderer, subtract_mode) != 0)
		{
			static bool sdl_subtractive_draw_warned = false;
			if (!sdl_subtractive_draw_warned)
			{

				sdl_subtractive_draw_warned = true;
			}
			(void)SDL_SetRenderDrawBlendMode(platform_renderer_sdl_renderer, SDL_BLENDMODE_MOD);
		}
		break;
	}
	case PLATFORM_RENDERER_BLEND_MODE_ALPHA:
	default:
		(void)SDL_SetRenderDrawBlendMode(platform_renderer_sdl_renderer, SDL_BLENDMODE_BLEND);
		break;
	}
}

static Uint8 PLATFORM_RENDERER_get_sdl_texture_alpha_mod(Uint8 requested_alpha)
{
	/*
	 * GL with blending disabled does not produce translucent textured output from
	 * vertex alpha alone. Match that behavior on SDL by forcing opaque alpha mod.
	 */
	if (!platform_renderer_blend_enabled)
	{
		return 255;
	}
	return requested_alpha;
}

static Uint8 PLATFORM_RENDERER_clamp_sdl_colour_mod(int value)
{
	if (value < 0)
	{
		return 0;
	}
	if (value > 255)
	{
		return 255;
	}
	return (Uint8)value;
}

static Uint8 PLATFORM_RENDERER_clamp_sdl_unit_to_byte(float unit_value)
{
	if (unit_value <= 0.0f)
	{
		return 0;
	}
	if (unit_value >= 1.0f)
	{
		return 255;
	}
	return (Uint8)(int)(unit_value * 255.0f);
}

/*
 * Wrapper around SDL_RenderCopyEx that falls back to SDL_RenderCopy when there
 * is no rotation or flip to apply.  On some GLES2/Panfrost drivers (SDL 2.28.x)
 * SDL_RenderCopyEx silently produces no pixels even when returning 0, while
 * SDL_RenderCopy works correctly.
 */
static int PLATFORM_RENDERER_sdl_copy_ex(
		SDL_Renderer *renderer,
		SDL_Texture *texture,
		const SDL_Rect *src,
		const SDL_Rect *dst,
		double angle,
		const SDL_Point *center,
		SDL_RendererFlip flip)
{
	if ((angle == 0.0) && (center == NULL) && (flip == SDL_FLIP_NONE))
	{
		return SDL_RenderCopy(renderer, texture, src, dst);
	}
	return SDL_RenderCopyEx(renderer, texture, src, dst, angle, center, flip);
}

/*
 * SDL texture state cache -----------------------------------------------
 * SDL_SetTextureBlendMode / ColorMod / AlphaMod are expensive on GLES2
 * because each call can flush the renderer's internal draw batch and
 * results in GPU state changes.  We keep a tiny per-slot cache of the last
 * value applied to each SDL_Texture* and skip redundant calls.
 *
 * The game uses at most ~50 textures, but can have up to 3 variants each
 * (normal, premultiplied, inverted).  A 128-slot direct-mapped hash keyed
 * on the lower bits of the pointer handles this without extra allocation.
 */
#define SDLCACHE_SLOTS 256
#define SDLCACHE_IDX(p) (((uintptr_t)(p) >> 3) & (SDLCACHE_SLOTS - 1))

static struct
{
	SDL_Texture    *tex;
	SDL_BlendMode   blend;
	Uint8           r, g, b, a;
	bool            color_valid; /* false when blend evicted slot: forces next color call to hit SDL */
} sdl_tex_cache[SDLCACHE_SLOTS];

/* Invalidate the cache slot for a texture (call when texture is rebuilt). */
static void PLATFORM_RENDERER_invalidate_tex_cache(SDL_Texture *tex)
{
	if (tex == NULL) return;
	int slot = (int)SDLCACHE_IDX(tex);
	if (sdl_tex_cache[slot].tex == tex)
		sdl_tex_cache[slot].tex = NULL;
}

static void PLATFORM_RENDERER_set_texture_blend_cached(SDL_Texture *tex, SDL_BlendMode mode)
{
	if (tex == NULL) return;
	int slot = (int)SDLCACHE_IDX(tex);
	if (sdl_tex_cache[slot].tex == tex && sdl_tex_cache[slot].blend == mode)
		return; /* no-op: already set */
	/* If this slot was owned by a different texture, the r,g,b,a fields are stale.
	 * Mark color_valid=false so the next color call will always make the SDL call. */
	if (sdl_tex_cache[slot].tex != tex)
		sdl_tex_cache[slot].color_valid = false;
	(void)SDL_SetTextureBlendMode(tex, mode);
	sdl_tex_cache[slot].tex   = tex;
	sdl_tex_cache[slot].blend = mode;
	platform_renderer_sdl_blend_switch_count++;
}

static void PLATFORM_RENDERER_set_texture_color_alpha_cached(SDL_Texture *tex, Uint8 r, Uint8 g, Uint8 b, Uint8 a)
{
	if (tex == NULL) return;
	int slot = (int)SDLCACHE_IDX(tex);
	/* color_valid guards against stale r,g,b,a left by a prior texture that owned this slot.
	 * It is cleared by set_texture_blend_cached when the slot owner changes. */
	if (sdl_tex_cache[slot].tex == tex &&
			sdl_tex_cache[slot].color_valid &&
			sdl_tex_cache[slot].r == r &&
			sdl_tex_cache[slot].g == g &&
			sdl_tex_cache[slot].b == b &&
			sdl_tex_cache[slot].a == a)
		return; /* no-op */
	(void)SDL_SetTextureColorMod(tex, r, g, b);
	(void)SDL_SetTextureAlphaMod(tex, a);
	sdl_tex_cache[slot].tex         = tex;
	sdl_tex_cache[slot].r           = r;
	sdl_tex_cache[slot].g           = g;
	sdl_tex_cache[slot].b           = b;
	sdl_tex_cache[slot].a           = a;
	sdl_tex_cache[slot].color_valid = true;
	platform_renderer_sdl_colmod_switch_count++;
}
/* --- end SDL texture state cache --------------------------------------- */

static void PLATFORM_RENDERER_apply_sdl_texture_blend_mode(SDL_Texture *texture)
{
	if (texture == NULL)
	{
		return;
	}

	if (platform_renderer_addq_count > 0)
	{
		PLATFORM_RENDERER_flush_addq();
	}

	/* Count per-blend-mode draw usage for batching diagnostics. */
	if (platform_renderer_blend_mode == PLATFORM_RENDERER_BLEND_MODE_ADD)
		platform_renderer_sdl_add_draw_count++;
	if (platform_renderer_blend_mode != PLATFORM_RENDERER_BLEND_MODE_ALPHA)
		platform_renderer_sdl_spec_draw_count++;

	if (!platform_renderer_blend_enabled)
	{
		/*
		 * GL paths often render cutout sprites with blending disabled and alpha-test enabled.
		 * SDL has no alpha-test equivalent in RenderCopy, so keep texture blending on
		 * to preserve transparent texels instead of drawing opaque matte backgrounds.
		 */
		PLATFORM_RENDERER_set_texture_blend_cached(texture, SDL_BLENDMODE_BLEND);
		return;
	}

	switch (platform_renderer_blend_mode)
	{
	case PLATFORM_RENDERER_BLEND_MODE_ADD:
		PLATFORM_RENDERER_set_texture_blend_cached(texture, SDL_BLENDMODE_ADD);
		break;
	case PLATFORM_RENDERER_BLEND_MODE_MULTIPLY:
		/*
		 * The original GL code used GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA for
		 * BLEND_MULTIPLY — i.e. standard alpha blending despite the name.
		 * Use SDL_BLENDMODE_BLEND to match.
		 */
		platform_renderer_sdl_multiply_texture_support_checked = true;
		platform_renderer_sdl_multiply_texture_supported = true;
		PLATFORM_RENDERER_set_texture_blend_cached(texture, SDL_BLENDMODE_BLEND);
		break;
	case PLATFORM_RENDERER_BLEND_MODE_SUBTRACT:
	{
		/* Cache the subtract composed blend mode too. */
		static SDL_BlendMode s_subtract_mode = SDL_BLENDMODE_INVALID;
		if (s_subtract_mode == SDL_BLENDMODE_INVALID)
		{
			s_subtract_mode = SDL_ComposeCustomBlendMode(
					SDL_BLENDFACTOR_ZERO, SDL_BLENDFACTOR_ONE_MINUS_SRC_COLOR, SDL_BLENDOPERATION_ADD,
					SDL_BLENDFACTOR_ZERO, SDL_BLENDFACTOR_ONE, SDL_BLENDOPERATION_ADD);
		}
		SDL_BlendMode subtract_mode = s_subtract_mode;
		platform_renderer_sdl_subtractive_texture_support_checked = true;
		/* Cache check: skip SDL call (and batch flush) if already set. */
		{
			int slot = (int)SDLCACHE_IDX(texture);
			if (sdl_tex_cache[slot].tex == texture && sdl_tex_cache[slot].blend == subtract_mode)
				break;
		}
		if (SDL_SetTextureBlendMode(texture, subtract_mode) != 0)
		{
			static bool sdl_subtractive_texture_warned = false;
			platform_renderer_sdl_subtractive_texture_supported = false;
			if (!sdl_subtractive_texture_warned)
			{

				sdl_subtractive_texture_warned = true;
			}
			PLATFORM_RENDERER_set_texture_blend_cached(texture, SDL_BLENDMODE_MOD);
		}
		else
		{
			platform_renderer_sdl_subtractive_texture_supported = true;
			int slot = (int)SDLCACHE_IDX(texture);
			if (sdl_tex_cache[slot].tex != texture)
				sdl_tex_cache[slot].color_valid = false;
			sdl_tex_cache[slot].tex   = texture;
			sdl_tex_cache[slot].blend = subtract_mode;
			platform_renderer_sdl_blend_switch_count++;
		}
		break;
	}
	case PLATFORM_RENDERER_BLEND_MODE_ALPHA:
	default:
		PLATFORM_RENDERER_set_texture_blend_cached(texture, SDL_BLENDMODE_BLEND);
		break;
	}
}

static int platform_renderer_addq_cmp(const void *a, const void *b)
{
	const platform_renderer_addq_entry *ea = (const platform_renderer_addq_entry *)a;
	const platform_renderer_addq_entry *eb = (const platform_renderer_addq_entry *)b;
	if (ea->tex < eb->tex) return -1;
	if (ea->tex > eb->tex) return 1;
	return 0;
}

static bool PLATFORM_RENDERER_should_defer_add_geometry(
		SDL_Texture *texture,
		int vertex_count,
		const int *indices,
		int index_count,
		int source_id)
{
	if (texture == NULL)
		return false;
	if (indices == NULL)
		return false;
	if (!platform_renderer_blend_enabled ||
			(platform_renderer_blend_mode != PLATFORM_RENDERER_BLEND_MODE_ADD))
		return false;
	if ((source_id <= PLATFORM_RENDERER_GEOM_SRC_NONE) ||
			(source_id >= PLATFORM_RENDERER_GEOM_SRC_COUNT))
		return false;
	if ((vertex_count <= 0) || (vertex_count > 8))
		return false;
	if ((index_count <= 0) || (index_count > 18))
		return false;
	if (platform_renderer_addq_count >= PLATFORM_RENDERER_ADDQ_MAX)
		return false;
	return true;
}

static int PLATFORM_RENDERER_geom_atlas_item_cmp(const void *a, const void *b)
{
	const platform_renderer_geom_atlas_item *ia = (const platform_renderer_geom_atlas_item *)a;
	const platform_renderer_geom_atlas_item *ib = (const platform_renderer_geom_atlas_item *)b;

	if (ib->height != ia->height)
	{
		return ib->height - ia->height;
	}
	if (ib->width != ia->width)
	{
		return ib->width - ia->width;
	}
	return ib->area - ia->area;
}

static bool PLATFORM_RENDERER_prepare_geometry_texture_atlas(void)
{
	platform_renderer_geom_atlas_item *items = NULL;
	unsigned char *atlas_pixels = NULL;
	int item_count = 0;
	int packed_count = 0;
	int cursor_x = 0;
	int cursor_y = 0;
	int row_h = 0;
	int i;

	if (platform_renderer_geom_atlas_disabled)
	{
		return false;
	}
	if (!platform_renderer_geom_atlas_dirty)
	{
		return (platform_renderer_geom_atlas_texture != NULL) && (platform_renderer_geom_atlas_texture_count > 0);
	}

	platform_renderer_geom_atlas_dirty = false;
	platform_renderer_geom_atlas_texture_count = 0;
	for (i = 0; i < platform_renderer_texture_count; i++)
	{
		platform_renderer_texture_entries[i].sdl_atlas_x = 0;
		platform_renderer_texture_entries[i].sdl_atlas_y = 0;
		platform_renderer_texture_entries[i].sdl_atlas_w = 0;
		platform_renderer_texture_entries[i].sdl_atlas_h = 0;
		platform_renderer_texture_entries[i].sdl_atlas_mapped = false;
	}

	if ((platform_renderer_sdl_renderer == NULL) || (platform_renderer_texture_count <= 1))
	{
		return false;
	}

	items = (platform_renderer_geom_atlas_item *)malloc(
			(size_t)platform_renderer_texture_count * sizeof(platform_renderer_geom_atlas_item));
	if (items == NULL)
	{
		platform_renderer_geom_atlas_disabled = true;
		return false;
	}

	for (i = 0; i < platform_renderer_texture_count; i++)
	{
		platform_renderer_texture_entry *entry = &platform_renderer_texture_entries[i];
		const int area = entry->width * entry->height;

		if ((entry->sdl_texture == NULL) ||
				(entry->sdl_rgba_pixels == NULL) ||
				(entry->width <= 0) ||
				(entry->height <= 0) ||
				(entry->width > PLATFORM_RENDERER_GEOM_ATLAS_MAX_SIDE) ||
				(entry->height > PLATFORM_RENDERER_GEOM_ATLAS_MAX_SIDE) ||
				(area <= 0) ||
				(area > PLATFORM_RENDERER_GEOM_ATLAS_MAX_AREA))
		{
			continue;
		}

		items[item_count].entry_index = i;
		items[item_count].width = entry->width;
		items[item_count].height = entry->height;
		items[item_count].area = area;
		item_count++;
	}

	if (item_count <= 1)
	{
		free(items);
		return false;
	}

	qsort(items, (size_t)item_count, sizeof(platform_renderer_geom_atlas_item),
			PLATFORM_RENDERER_geom_atlas_item_cmp);

	for (i = 0; i < item_count; i++)
	{
		platform_renderer_texture_entry *entry = &platform_renderer_texture_entries[items[i].entry_index];

		if ((cursor_x + entry->width) > PLATFORM_RENDERER_GEOM_ATLAS_WIDTH)
		{
			cursor_x = 0;
			cursor_y += row_h;
			row_h = 0;
		}
		if ((cursor_y + entry->height) > PLATFORM_RENDERER_GEOM_ATLAS_HEIGHT)
		{
			continue;
		}

		entry->sdl_atlas_x = cursor_x;
		entry->sdl_atlas_y = cursor_y;
		entry->sdl_atlas_w = entry->width;
		entry->sdl_atlas_h = entry->height;
		entry->sdl_atlas_mapped = true;
		packed_count++;

		cursor_x += entry->width;
		if (entry->height > row_h)
		{
			row_h = entry->height;
		}
	}

	free(items);
	items = NULL;

	if (packed_count <= 1)
	{
		return false;
	}

	atlas_pixels = (unsigned char *)calloc(
			(size_t)PLATFORM_RENDERER_GEOM_ATLAS_WIDTH * (size_t)PLATFORM_RENDERER_GEOM_ATLAS_HEIGHT * 4u,
			1u);
	if (atlas_pixels == NULL)
	{
		platform_renderer_geom_atlas_disabled = true;
		for (i = 0; i < platform_renderer_texture_count; i++)
		{
			platform_renderer_texture_entries[i].sdl_atlas_mapped = false;
		}
		return false;
	}

	for (i = 0; i < platform_renderer_texture_count; i++)
	{
		const platform_renderer_texture_entry *entry = &platform_renderer_texture_entries[i];
		int y;

		if (!entry->sdl_atlas_mapped)
		{
			continue;
		}

		for (y = 0; y < entry->height; y++)
		{
			unsigned char *dst_row = atlas_pixels +
					(((size_t)(entry->sdl_atlas_y + y) * (size_t)PLATFORM_RENDERER_GEOM_ATLAS_WIDTH) +
					 (size_t)entry->sdl_atlas_x) * 4u;
			const unsigned char *src_row = entry->sdl_rgba_pixels +
					(((size_t)y * (size_t)entry->width) * 4u);
			memcpy(dst_row, src_row, (size_t)entry->width * 4u);
		}
	}

	if (platform_renderer_geom_atlas_texture == NULL)
	{
		platform_renderer_geom_atlas_texture = SDL_CreateTexture(
				platform_renderer_sdl_renderer,
				SDL_PIXELFORMAT_RGBA32,
				SDL_TEXTUREACCESS_STATIC,
				PLATFORM_RENDERER_GEOM_ATLAS_WIDTH,
				PLATFORM_RENDERER_GEOM_ATLAS_HEIGHT);
	}
	if (platform_renderer_geom_atlas_texture == NULL)
	{
		free(atlas_pixels);
		platform_renderer_geom_atlas_disabled = true;
		for (i = 0; i < platform_renderer_texture_count; i++)
		{
			platform_renderer_texture_entries[i].sdl_atlas_mapped = false;
		}
		return false;
	}

	if (SDL_UpdateTexture(platform_renderer_geom_atlas_texture, NULL, atlas_pixels,
			PLATFORM_RENDERER_GEOM_ATLAS_WIDTH * 4) != 0)
	{
		free(atlas_pixels);
		SDL_DestroyTexture(platform_renderer_geom_atlas_texture);
		platform_renderer_geom_atlas_texture = NULL;
		platform_renderer_geom_atlas_disabled = true;
		for (i = 0; i < platform_renderer_texture_count; i++)
		{
			platform_renderer_texture_entries[i].sdl_atlas_mapped = false;
		}
		return false;
	}

	free(atlas_pixels);
	platform_renderer_geom_atlas_texture_count = packed_count;
	return true;
}

static bool PLATFORM_RENDERER_try_remap_geometry_texture_to_atlas(
		SDL_Texture **io_texture,
		SDL_Vertex *vertices,
		int vertex_count)
{
	platform_renderer_texture_entry *entry;
	int i;

	if ((io_texture == NULL) || (*io_texture == NULL) || (vertices == NULL) || (vertex_count <= 0))
	{
		return false;
	}
	if (!PLATFORM_RENDERER_prepare_geometry_texture_atlas())
	{
		return false;
	}

	entry = PLATFORM_RENDERER_find_texture_entry_from_sdl_texture(*io_texture);
	if ((entry == NULL) || !entry->sdl_atlas_mapped || (platform_renderer_geom_atlas_texture == NULL))
	{
		return false;
	}

	for (i = 0; i < vertex_count; i++)
	{
		vertices[i].tex_coord.x =
				((float)entry->sdl_atlas_x + (vertices[i].tex_coord.x * (float)entry->sdl_atlas_w)) /
				(float)PLATFORM_RENDERER_GEOM_ATLAS_WIDTH;
		vertices[i].tex_coord.y =
				((float)entry->sdl_atlas_y + (vertices[i].tex_coord.y * (float)entry->sdl_atlas_h)) /
				(float)PLATFORM_RENDERER_GEOM_ATLAS_HEIGHT;
	}

	*io_texture = platform_renderer_geom_atlas_texture;
	return true;
}

static bool PLATFORM_RENDERER_find_addq_atlas_slot(SDL_Texture *source_tex, platform_renderer_addq_atlas_slot *out_slot)
{
	int i;

	if ((source_tex == NULL) || (out_slot == NULL))
	{
		return false;
	}

	for (i = 0; i < platform_renderer_addq_atlas_slot_count; i++)
	{
		if (platform_renderer_addq_atlas_slots[i].source_tex == source_tex)
		{
			*out_slot = platform_renderer_addq_atlas_slots[i];
			return true;
		}
	}

	return false;
}

static bool PLATFORM_RENDERER_prepare_addq_atlas(void)
{
	SDL_Texture *unique_textures[PLATFORM_RENDERER_ADDQ_ATLAS_MAX_TEXTURES];
	platform_renderer_texture_entry *unique_entries[PLATFORM_RENDERER_ADDQ_ATLAS_MAX_TEXTURES];
	int unique_count = 0;
	int atlas_width = 1024;
	int atlas_height = 1024;
	int cursor_x = 0;
	int cursor_y = 0;
	int row_h = 0;
	unsigned char *atlas_pixels = NULL;
	int i;
	int j;

	if ((platform_renderer_addq_count <= 1) || (platform_renderer_sdl_renderer == NULL))
	{
		return false;
	}

	for (i = 0; i < platform_renderer_addq_count; i++)
	{
		SDL_Texture *tex = platform_renderer_addq[i].tex;
		bool seen = false;

		for (j = 0; j < unique_count; j++)
		{
			if (unique_textures[j] == tex)
			{
				seen = true;
				break;
			}
		}
		if (seen)
		{
			continue;
		}
		if (unique_count >= PLATFORM_RENDERER_ADDQ_ATLAS_MAX_TEXTURES)
		{
			return false;
		}
		unique_textures[unique_count] = tex;
		unique_entries[unique_count] = PLATFORM_RENDERER_find_texture_entry_from_sdl_texture(tex);
		if ((unique_entries[unique_count] == NULL) ||
				(unique_entries[unique_count]->sdl_rgba_pixels == NULL) ||
				(unique_entries[unique_count]->width <= 0) ||
				(unique_entries[unique_count]->height <= 0))
		{
			return false;
		}
		unique_count++;
	}

	if (unique_count <= 1)
	{
		return false;
	}

	platform_renderer_addq_atlas_slot_count = 0;
	memset(platform_renderer_addq_atlas_slots, 0, sizeof(platform_renderer_addq_atlas_slots));

	for (i = 0; i < unique_count; i++)
	{
		const int w = unique_entries[i]->width;
		const int h = unique_entries[i]->height;

		if ((w > atlas_width) || (h > atlas_height))
		{
			return false;
		}
		if ((cursor_x + w) > atlas_width)
		{
			cursor_x = 0;
			cursor_y += row_h;
			row_h = 0;
		}
		if ((cursor_y + h) > atlas_height)
		{
			return false;
		}

		platform_renderer_addq_atlas_slots[platform_renderer_addq_atlas_slot_count].source_tex = unique_textures[i];
		platform_renderer_addq_atlas_slots[platform_renderer_addq_atlas_slot_count].x = cursor_x;
		platform_renderer_addq_atlas_slots[platform_renderer_addq_atlas_slot_count].y = cursor_y;
		platform_renderer_addq_atlas_slots[platform_renderer_addq_atlas_slot_count].w = w;
		platform_renderer_addq_atlas_slots[platform_renderer_addq_atlas_slot_count].h = h;
		platform_renderer_addq_atlas_slot_count++;

		cursor_x += w;
		if (h > row_h)
		{
			row_h = h;
		}
	}

	atlas_pixels = (unsigned char *)calloc((size_t)atlas_width * (size_t)atlas_height * 4u, 1u);
	if (atlas_pixels == NULL)
	{
		platform_renderer_addq_atlas_slot_count = 0;
		return false;
	}

	for (i = 0; i < unique_count; i++)
	{
		const platform_renderer_addq_atlas_slot slot = platform_renderer_addq_atlas_slots[i];
		const platform_renderer_texture_entry *entry = unique_entries[i];
		int y;

		for (y = 0; y < entry->height; y++)
		{
			unsigned char *dst_row = atlas_pixels +
					(((size_t)(slot.y + y) * (size_t)atlas_width) + (size_t)slot.x) * 4u;
			const unsigned char *src_row = entry->sdl_rgba_pixels +
					(((size_t)y * (size_t)entry->width) * 4u);
			memcpy(dst_row, src_row, (size_t)entry->width * 4u);
		}
	}

	if ((platform_renderer_addq_atlas_texture == NULL) ||
			(platform_renderer_addq_atlas_width != atlas_width) ||
			(platform_renderer_addq_atlas_height != atlas_height))
	{
		if (platform_renderer_addq_atlas_texture != NULL)
		{
			SDL_DestroyTexture(platform_renderer_addq_atlas_texture);
			platform_renderer_addq_atlas_texture = NULL;
		}
		platform_renderer_addq_atlas_texture = SDL_CreateTexture(
				platform_renderer_sdl_renderer,
				SDL_PIXELFORMAT_RGBA32,
				SDL_TEXTUREACCESS_STATIC,
				atlas_width,
				atlas_height);
		if (platform_renderer_addq_atlas_texture == NULL)
		{
			free(atlas_pixels);
			platform_renderer_addq_atlas_slot_count = 0;
			return false;
		}
		platform_renderer_addq_atlas_width = atlas_width;
		platform_renderer_addq_atlas_height = atlas_height;
	}

	if (SDL_UpdateTexture(platform_renderer_addq_atlas_texture, NULL, atlas_pixels, atlas_width * 4) != 0)
	{
		free(atlas_pixels);
		platform_renderer_addq_atlas_slot_count = 0;
		return false;
	}

	free(atlas_pixels);
	return true;
}

/* Submit all deferred ADD-blend draws sorted by texture pointer.
 * Must be called before any SDL_RenderSetClipRect change and before SDL_RenderPresent.
 * Safe to call with an empty queue. */
static void PLATFORM_RENDERER_flush_addq(void)
{
	int i;
	int group_start;

	if (platform_renderer_addq_count == 0)
		return;

	if (PLATFORM_RENDERER_prepare_addq_atlas())
	{
		int out_vertex_count = 0;
		int out_index_count = 0;

		for (i = 0; i < platform_renderer_addq_count; i++)
		{
			const platform_renderer_addq_entry *e = &platform_renderer_addq[i];
			platform_renderer_addq_atlas_slot slot;

			if (!PLATFORM_RENDERER_find_addq_atlas_slot(e->tex, &slot))
			{
				platform_renderer_addq_atlas_slot_count = 0;
				break;
			}

			for (int v = 0; v < e->vertex_count; v++)
			{
				SDL_Vertex vert = e->verts[v];
				vert.tex_coord.x = ((float)slot.x + (vert.tex_coord.x * (float)slot.w)) /
						(float)platform_renderer_addq_atlas_width;
				vert.tex_coord.y = ((float)slot.y + (vert.tex_coord.y * (float)slot.h)) /
						(float)platform_renderer_addq_atlas_height;
				platform_renderer_addq_flush_verts[out_vertex_count + v] = vert;
			}
			for (int idx = 0; idx < e->index_count; idx++)
			{
				platform_renderer_addq_flush_indices[out_index_count + idx] =
						e->indices[idx] + out_vertex_count;
			}
			out_vertex_count += e->vertex_count;
			out_index_count += e->index_count;
		}

		if (platform_renderer_addq_atlas_slot_count > 0)
		{
			PLATFORM_RENDERER_set_texture_blend_cached(platform_renderer_addq_atlas_texture, SDL_BLENDMODE_ADD);
			PLATFORM_RENDERER_set_texture_color_alpha_cached(platform_renderer_addq_atlas_texture, 255, 255, 255, 255);
			SDL_RenderGeometry(platform_renderer_sdl_renderer, platform_renderer_addq_atlas_texture,
				platform_renderer_addq_flush_verts, out_vertex_count,
				platform_renderer_addq_flush_indices, out_index_count);
			if (platform_renderer_addq_atlas_texture != platform_renderer_last_geom_texture)
			{
				platform_renderer_sdl_tx_switch_count++;
				platform_renderer_last_geom_texture = platform_renderer_addq_atlas_texture;
			}
			platform_renderer_addq_count = 0;
			platform_renderer_addq_atlas_slot_count = 0;
			return;
		}
	}

	if (platform_renderer_addq_count > 1)
		qsort(platform_renderer_addq, platform_renderer_addq_count,
			  sizeof(platform_renderer_addq_entry), platform_renderer_addq_cmp);
	for (group_start = 0; group_start < platform_renderer_addq_count; )
	{
		SDL_Texture *group_tex = platform_renderer_addq[group_start].tex;
		int group_end = group_start;
		int out_vertex_count = 0;
		int out_index_count = 0;

		while ((group_end < platform_renderer_addq_count) &&
				(platform_renderer_addq[group_end].tex == group_tex))
		{
			platform_renderer_addq_entry *e = &platform_renderer_addq[group_end];
			for (i = 0; i < e->vertex_count; i++)
			{
				platform_renderer_addq_flush_verts[out_vertex_count + i] = e->verts[i];
			}
			for (i = 0; i < e->index_count; i++)
			{
				platform_renderer_addq_flush_indices[out_index_count + i] =
						e->indices[i] + out_vertex_count;
			}
			out_vertex_count += e->vertex_count;
			out_index_count += e->index_count;
			group_end++;
		}

		/* Set ADD blend on the texture (SDLCACHE skips the SDL call if already set). */
		PLATFORM_RENDERER_set_texture_blend_cached(group_tex, SDL_BLENDMODE_ADD);
		/* Reset color mod to white: SDL_RenderGeometry uses vertex colors for all
		 * modulation. A stale dark color mod left by a CopyEx-fallback draw (e.g.
		 * during a SUBTRACT fade) would otherwise multiply the vertex colors dark. */
		PLATFORM_RENDERER_set_texture_color_alpha_cached(group_tex, 255, 255, 255, 255);
		SDL_RenderGeometry(platform_renderer_sdl_renderer, group_tex,
			platform_renderer_addq_flush_verts, out_vertex_count,
			platform_renderer_addq_flush_indices, out_index_count);
		if (group_tex != platform_renderer_last_geom_texture)
		{
			platform_renderer_sdl_tx_switch_count++;
			platform_renderer_last_geom_texture = group_tex;
		}
		group_start = group_end;
	}
	platform_renderer_addq_count = 0;
}

void PLATFORM_RENDERER_reset_texture_registry(void)
{
	int i;
	for (i = 0; i < platform_renderer_texture_count; i++)
	{
		if (platform_renderer_texture_entries[i].sdl_texture != NULL)
		{
			SDL_DestroyTexture(platform_renderer_texture_entries[i].sdl_texture);
			platform_renderer_texture_entries[i].sdl_texture = NULL;
		}
		if (platform_renderer_texture_entries[i].sdl_texture_premultiplied != NULL)
		{
			SDL_DestroyTexture(platform_renderer_texture_entries[i].sdl_texture_premultiplied);
			platform_renderer_texture_entries[i].sdl_texture_premultiplied = NULL;
		}
		if (platform_renderer_texture_entries[i].sdl_texture_inverted != NULL)
		{
			SDL_DestroyTexture(platform_renderer_texture_entries[i].sdl_texture_inverted);
			platform_renderer_texture_entries[i].sdl_texture_inverted = NULL;
		}
		free(platform_renderer_texture_entries[i].sdl_rgba_pixels);
		platform_renderer_texture_entries[i].sdl_rgba_pixels = NULL;
	}
	free(platform_renderer_texture_entries);
	platform_renderer_texture_entries = NULL;
	platform_renderer_texture_count = 0;
	platform_renderer_texture_capacity = 0;
	platform_renderer_last_bound_texture_handle = 0;
	if (platform_renderer_geom_atlas_texture != NULL)
	{
		SDL_DestroyTexture(platform_renderer_geom_atlas_texture);
		platform_renderer_geom_atlas_texture = NULL;
	}
	platform_renderer_geom_atlas_texture_count = 0;
	platform_renderer_geom_atlas_dirty = true;
	platform_renderer_geom_atlas_disabled = false;
	if (platform_renderer_addq_atlas_texture != NULL)
	{
		SDL_DestroyTexture(platform_renderer_addq_atlas_texture);
		platform_renderer_addq_atlas_texture = NULL;
	}
	platform_renderer_addq_atlas_width = 0;
	platform_renderer_addq_atlas_height = 0;
	platform_renderer_addq_atlas_slot_count = 0;
}

static inline Uint32 get_surface_pixel(SDL_Surface *surface, int x, int y)
{
	const int bpp = surface->format->BytesPerPixel;
	const Uint8 *p = static_cast<const Uint8 *>(surface->pixels) + y * surface->pitch + x * bpp;

	switch (bpp)
	{
	case 1:
		return *p;
	case 2:
		return *reinterpret_cast<const Uint16 *>(p);
	case 3:
#if SDL_BYTEORDER == SDL_BIG_ENDIAN
		return (p[0] << 16) | (p[1] << 8) | p[2];
#else
		return p[0] | (p[1] << 8) | (p[2] << 16);
#endif
	case 4:
		return *reinterpret_cast<const Uint32 *>(p);
	default:
		return 0;
	}
}

static unsigned int PLATFORM_RENDERER_register_SDL_texture(unsigned int legacy_texture_id, SDL_Surface *image)
{
	bool require_nonzero_legacy_texture = false;
	if ((legacy_texture_id == 0) && require_nonzero_legacy_texture)
		return 0;

	if (platform_renderer_texture_count == platform_renderer_texture_capacity)
	{
		int new_capacity = (platform_renderer_texture_capacity == 0) ? 128 : (platform_renderer_texture_capacity * 2);
		platform_renderer_texture_entry *new_entries =
				(platform_renderer_texture_entry *)realloc(platform_renderer_texture_entries,
																									 sizeof(platform_renderer_texture_entry) * new_capacity);
		if (new_entries == NULL)
			return 0;

		platform_renderer_texture_entries = new_entries;
		platform_renderer_texture_capacity = new_capacity;
	}

	platform_renderer_texture_entries[platform_renderer_texture_count].legacy_texture_id = legacy_texture_id;
	platform_renderer_texture_entries[platform_renderer_texture_count].sdl_texture = NULL;
	platform_renderer_texture_entries[platform_renderer_texture_count].sdl_texture_premultiplied = NULL;
	platform_renderer_texture_entries[platform_renderer_texture_count].sdl_texture_inverted = NULL;
	platform_renderer_texture_entries[platform_renderer_texture_count].sdl_rgba_pixels = NULL;
	platform_renderer_texture_entries[platform_renderer_texture_count].width = 0;
	platform_renderer_texture_entries[platform_renderer_texture_count].height = 0;
	platform_renderer_texture_entries[platform_renderer_texture_count].sdl_atlas_x = 0;
	platform_renderer_texture_entries[platform_renderer_texture_count].sdl_atlas_y = 0;
	platform_renderer_texture_entries[platform_renderer_texture_count].sdl_atlas_w = 0;
	platform_renderer_texture_entries[platform_renderer_texture_count].sdl_atlas_h = 0;
	platform_renderer_texture_entries[platform_renderer_texture_count].sdl_atlas_mapped = false;
	platform_renderer_geom_atlas_dirty = true;

	if ((image != NULL) && (image->w > 0) && (image->h > 0))
	{
		const int image_width = image->w;
		const int image_height = image->h;
		const int image_depth = image->format ? image->format->BitsPerPixel : 0;

		// Determine mask colour (SDL equivalent: color key). If none, fall back to magic pink.
		Uint32 mask_pixel_value = 0;
		bool has_color_key = (SDL_GetColorKey(image, &mask_pixel_value) == 0);

		Uint8 mask_r = 255, mask_g = 0, mask_b = 255;
		if (has_color_key)
		{
			SDL_GetRGB(mask_pixel_value, image->format, &mask_r, &mask_g, &mask_b);
		}
		else
		{
			// Map magic pink into the image format so exact pixel matching works for true-colour.
			mask_pixel_value = SDL_MapRGB(image->format, mask_r, mask_g, mask_b);
		}

		// Convert paletted/low depth assets once to a 32-bit surface so RGB comes out palette-resolved.
		// We use this "colour_source" for extracting RGB (and alpha if meaningful).
		SDL_Surface *colour_source = image;
		SDL_Surface *converted_32 = NULL;

		if (image_depth <= 8)
		{
			converted_32 = SDL_ConvertSurfaceFormat(image, SDL_PIXELFORMAT_RGBA32, 0);
			if (converted_32 != NULL)
				colour_source = converted_32;
		}

		unsigned char *rgba_pixels =
				(unsigned char *)malloc((size_t)image_width * (size_t)image_height * 4u);

		if (rgba_pixels != NULL)
		{
			if (SDL_MUSTLOCK(image))
				SDL_LockSurface(image);
			if (SDL_MUSTLOCK(colour_source))
				SDL_LockSurface(colour_source);

			int mask_pixel_count = 0;
			int alpha_zero_pixel_count = 0;
			int alpha_zero_black_rgb_count = 0;

			for (int y = 0; y < image_height; y++)
			{
				for (int x = 0; x < image_width; x++)
				{
					const Uint32 pixel = get_surface_pixel(image, x, y);						 // for exact mask compare
					const Uint32 colour_px = get_surface_pixel(colour_source, x, y); // for RGB/alpha extraction
					const size_t index = ((size_t)y * (size_t)image_width + (size_t)x) * 4u;

					Uint8 pixel_r = 0, pixel_g = 0, pixel_b = 0, pixel_a = 255;
					SDL_GetRGBA(colour_px, colour_source->format, &pixel_r, &pixel_g, &pixel_b, &pixel_a);

					bool pixel_is_mask = false;

					// Exact pixel-index match for indexed/paletted assets (or any surface, really).
					if (pixel == mask_pixel_value)
					{
						pixel_is_mask = true;
					}
					// RGB fallback only for true-colour surfaces (>=15bpp), matching your Allegro logic.
					else if ((image_depth >= 15) &&
									 (pixel_r == mask_r) && (pixel_g == mask_g) && (pixel_b == mask_b))
					{
						pixel_is_mask = true;
					}

					if (pixel_is_mask)
					{
						mask_pixel_count++;
						alpha_zero_pixel_count++;
						if ((pixel_r == 0) && (pixel_g == 0) && (pixel_b == 0))
							alpha_zero_black_rgb_count++;

						// Force RGB to black for fully transparent pixels to avoid pink halos with filtering.
						rgba_pixels[index + 0] = 0u;
						rgba_pixels[index + 1] = 0u;
						rgba_pixels[index + 2] = 0u;
						rgba_pixels[index + 3] = 0u;
					}
					else
					{
						int alpha = 255;

						// Only preserve authored per-pixel alpha when the SOURCE is meaningful 32-bit alpha.
						// SDL analogue: 32bpp + non-zero Amask.
						if (image_depth == 32 && image->format && image->format->Amask != 0)
						{
							alpha = (int)pixel_a;
							if (alpha < 0)
								alpha = 0;
							if (alpha > 255)
								alpha = 255;
						}

						rgba_pixels[index + 0] = pixel_r;
						rgba_pixels[index + 1] = pixel_g;
						rgba_pixels[index + 2] = pixel_b;
						rgba_pixels[index + 3] = (unsigned char)alpha;

						if (alpha == 0)
						{
							alpha_zero_pixel_count++;
							if (rgba_pixels[index + 0] == 0u &&
									rgba_pixels[index + 1] == 0u &&
									rgba_pixels[index + 2] == 0u)
							{
								alpha_zero_black_rgb_count++;
							}
						}
					}
				}
			}

			if (SDL_MUSTLOCK(colour_source))
				SDL_UnlockSurface(colour_source);
			if (SDL_MUSTLOCK(image))
				SDL_UnlockSurface(image);

			if (converted_32 != NULL)
			{
				SDL_FreeSurface(converted_32);
				converted_32 = NULL;
			}

			platform_renderer_texture_entries[platform_renderer_texture_count].sdl_rgba_pixels = rgba_pixels;
			platform_renderer_texture_entries[platform_renderer_texture_count].width = image_width;
			platform_renderer_texture_entries[platform_renderer_texture_count].height = image_height;
		}
		else
		{
			if (converted_32 != NULL)
				SDL_FreeSurface(converted_32);

			fprintf(stderr,
							"Warning: Failed to allocate RGBA pixel buffer for legacy texture (id=%u, w=%d, h=%d)\n",
							legacy_texture_id, image_width, image_height);
		}
	}
	else
	{
		fprintf(stderr, "Warning: Legacy texture registration with invalid image (id=%u, w=%d, h=%d)\n",
						legacy_texture_id,
						(image != NULL) ? image->w : -1,
						(image != NULL) ? image->h : -1);
	}

	platform_renderer_texture_count++;

	// Externally-visible texture ids become renderer-owned handles (1-based).
	return (unsigned int)platform_renderer_texture_count;
}

static unsigned int PLATFORM_RENDERER_resolve_legacy_texture(unsigned int texture_handle)
{
	if ((texture_handle >= 1) && (texture_handle <= (unsigned int)platform_renderer_texture_count))
	{
		return platform_renderer_texture_entries[texture_handle - 1].legacy_texture_id;
	}
	{
		int index = PLATFORM_RENDERER_find_texture_index_from_legacy_id(texture_handle);
		if (index >= 0)
		{
			/* Accept legacy/raw GL texture IDs as valid handles during migration. */
			return platform_renderer_texture_entries[index].legacy_texture_id;
		}
	}
	return 0;
}

static int PLATFORM_RENDERER_find_texture_index_from_legacy_id(unsigned int legacy_texture_id)
{
	int i;

	if (legacy_texture_id == 0)
	{
		return -1;
	}

	for (i = 0; i < platform_renderer_texture_count; i++)
	{
		if (platform_renderer_texture_entries[i].legacy_texture_id == legacy_texture_id)
		{
			return i;
		}
	}

	return -1;
}

static platform_renderer_texture_entry *PLATFORM_RENDERER_get_texture_entry(unsigned int texture_handle)
{
	if ((texture_handle >= 1) && (texture_handle <= (unsigned int)platform_renderer_texture_count))
	{
		return &platform_renderer_texture_entries[texture_handle - 1];
	}

	{
		int index = PLATFORM_RENDERER_find_texture_index_from_legacy_id(texture_handle);
		if (index >= 0)
		{
			return &platform_renderer_texture_entries[index];
		}
	}

	return NULL;
}

static platform_renderer_texture_entry *PLATFORM_RENDERER_find_texture_entry_from_sdl_texture(SDL_Texture *texture)
{
	int i;

	if (texture == NULL)
	{
		return NULL;
	}

	for (i = 0; i < platform_renderer_texture_count; i++)
	{
		if (platform_renderer_texture_entries[i].sdl_texture == texture)
		{
			return &platform_renderer_texture_entries[i];
		}
	}

	return NULL;
}

static unsigned int PLATFORM_RENDERER_normalize_texture_handle(unsigned int texture_handle)
{
	if ((texture_handle >= 1) && (texture_handle <= (unsigned int)platform_renderer_texture_count))
	{
		return texture_handle;
	}

	{
		int index = PLATFORM_RENDERER_find_texture_index_from_legacy_id(texture_handle);
		if (index >= 0)
		{
			return (unsigned int)(index + 1);
		}
	}

	return 0;
}

void PLATFORM_RENDERER_clear_backbuffer(void)
{
	platform_renderer_clear_backbuffer_calls_since_present++;
	/* Detect resets after draw activity, which can hide earlier GL output. */
	if ((platform_renderer_legacy_draw_count > 0) || (platform_renderer_sdl_native_draw_count > 0))
	{
		platform_renderer_midframe_reset_events++;
	}
	platform_renderer_sdl_native_draw_count = 0;
	platform_renderer_sdl_native_textured_draw_count = 0;
	platform_renderer_sdl_geometry_fallback_miss_count = 0;
	platform_renderer_sdl_geometry_degraded_count = 0;
	platform_renderer_sdl_window_sprite_draw_count = 0;
	platform_renderer_sdl_window_solid_rect_draw_count = 0;
	platform_renderer_sdl_bound_custom_draw_count = 0;
	platform_renderer_sdl_tx_switch_count = 0;
	platform_renderer_sdl_clip_switch_count = 0;
	platform_renderer_sdl_blend_switch_count = 0;
	platform_renderer_sdl_colmod_switch_count = 0;
	platform_renderer_sdl_alpha_skip_count = 0;
	platform_renderer_sdl_add_draw_count = 0;
	platform_renderer_sdl_spec_draw_count = 0;
	platform_renderer_sdl_defer_draw_count = 0;
	platform_renderer_addq_count = 0; /* discard any deferred draws from aborted frames */
	platform_renderer_last_geom_texture = NULL;
	platform_renderer_legacy_window_sprite_draw_count = 0;
	platform_renderer_legacy_window_solid_rect_draw_count = 0;
	platform_renderer_legacy_bound_custom_draw_count = 0;
	platform_renderer_legacy_src6_count = 0;
	platform_renderer_legacy_src6_min_q = 0.0f;
	platform_renderer_legacy_src6_max_q = 0.0f;
	platform_renderer_legacy_src6_min_x = 0.0f;
	platform_renderer_legacy_src6_min_y = 0.0f;
	platform_renderer_legacy_src6_max_x = 0.0f;
	platform_renderer_legacy_src6_max_y = 0.0f;
	platform_renderer_legacy_draw_count = 0;
	platform_renderer_legacy_textured_draw_count = 0;
	memset(platform_renderer_sdl_geometry_miss_sources, 0, sizeof(platform_renderer_sdl_geometry_miss_sources));
	memset(platform_renderer_sdl_geometry_degraded_sources, 0, sizeof(platform_renderer_sdl_geometry_degraded_sources));
	memset(platform_renderer_sdl_native_draw_sources, 0, sizeof(platform_renderer_sdl_native_draw_sources));
	memset(platform_renderer_legacy_draw_sources, 0, sizeof(platform_renderer_legacy_draw_sources));
	memset(platform_renderer_sdl_source_bounds_count, 0, sizeof(platform_renderer_sdl_source_bounds_count));
	memset(platform_renderer_sdl_source_min_x, 0, sizeof(platform_renderer_sdl_source_min_x));
	memset(platform_renderer_sdl_source_min_y, 0, sizeof(platform_renderer_sdl_source_min_y));
	memset(platform_renderer_sdl_source_max_x, 0, sizeof(platform_renderer_sdl_source_max_x));
	memset(platform_renderer_sdl_source_max_y, 0, sizeof(platform_renderer_sdl_source_max_y));
	memset(platform_renderer_legacy_viewport_last, 0, sizeof(platform_renderer_legacy_viewport_last));
	memset(platform_renderer_legacy_scissor_last, 0, sizeof(platform_renderer_legacy_scissor_last));
	platform_renderer_legacy_scissor_enabled_last = 0;
	platform_renderer_legacy_draw_buffer_last = 0;
	platform_renderer_legacy_read_buffer_last = 0;
	platform_renderer_legacy_error_last = 0;
	platform_renderer_legacy_diag_first_error_code = 0;
	platform_renderer_legacy_diag_error_count_frame = 0;
	snprintf(platform_renderer_legacy_diag_first_error_stage, sizeof(platform_renderer_legacy_diag_first_error_stage), "-");
	if (true && PLATFORM_RENDERER_is_sdl2_stub_ready())
	{
		(void)SDL_SetRenderDrawColor(platform_renderer_sdl_renderer, 0, 0, 0, 255);
		(void)SDL_RenderClear(platform_renderer_sdl_renderer);
	}
	/* Mark the start of the render/draw-submission phase for per-frame timing. */
	s_frame_clear_ticks = SDL_GetTicks();
}

void PLATFORM_RENDERER_draw_outline_rect(int x1, int y1, int x2, int y2, int r, int g, int b, int virtual_screen_height)
{
	if (true && PLATFORM_RENDERER_is_sdl2_stub_ready())
	{
		int left = (x1 < x2) ? x1 : x2;
		int right = (x1 > x2) ? x1 : x2;
		int top_input = (y1 < y2) ? y1 : y2;
		int bottom_input = (y1 > y2) ? y1 : y2;
		int top = top_input;
		int bottom = bottom_input;
		int width = right - left;
		int height = bottom - top;
		SDL_Rect rect;

		if (width <= 0)
		{
			width = 1;
		}
		if (height <= 0)
		{
			height = 1;
		}
		rect.x = left;
		rect.y = top;
		rect.w = width;
		rect.h = height;

		PLATFORM_RENDERER_apply_sdl_draw_blend_mode();
		(void)SDL_SetRenderDrawColor(
				platform_renderer_sdl_renderer,
				PLATFORM_RENDERER_clamp_sdl_colour_mod(r),
				PLATFORM_RENDERER_clamp_sdl_colour_mod(g),
				PLATFORM_RENDERER_clamp_sdl_colour_mod(b),
				255);
		if (SDL_RenderDrawRect(platform_renderer_sdl_renderer, &rect) == 0)
		{
			platform_renderer_sdl_native_draw_count++;
		}
	}
}

void PLATFORM_RENDERER_draw_filled_rect(int x1, int y1, int x2, int y2, int r, int g, int b, int virtual_screen_height)
{
	if (true && PLATFORM_RENDERER_is_sdl2_stub_ready())
	{
		int left = (x1 < x2) ? x1 : x2;
		int right = (x1 > x2) ? x1 : x2;
		int top_input = (y1 < y2) ? y1 : y2;
		int bottom_input = (y1 > y2) ? y1 : y2;
		int top = top_input;
		int bottom = bottom_input;
		int width = right - left;
		int height = bottom - top;
		SDL_Rect rect;

		if (width <= 0)
		{
			width = 1;
		}
		if (height <= 0)
		{
			height = 1;
		}
		rect.x = left;
		rect.y = top;
		rect.w = width;
		rect.h = height;

		if (platform_renderer_white_texture != NULL)
		{
			const Uint8 cr = PLATFORM_RENDERER_clamp_sdl_colour_mod(r);
			const Uint8 cg = PLATFORM_RENDERER_clamp_sdl_colour_mod(g);
			const Uint8 cb = PLATFORM_RENDERER_clamp_sdl_colour_mod(b);
			SDL_Vertex verts[4];
			static const int idx[6] = {0, 1, 2, 0, 2, 3};
			verts[0].position.x = (float)rect.x;             verts[0].position.y = (float)rect.y;
			verts[1].position.x = (float)(rect.x + rect.w);  verts[1].position.y = (float)rect.y;
			verts[2].position.x = (float)(rect.x + rect.w);  verts[2].position.y = (float)(rect.y + rect.h);
			verts[3].position.x = (float)rect.x;             verts[3].position.y = (float)(rect.y + rect.h);
			for (int vi = 0; vi < 4; vi++) {
				verts[vi].tex_coord.x = 0.5f;
				verts[vi].tex_coord.y = 0.5f;
				verts[vi].color.r = cr; verts[vi].color.g = cg;
				verts[vi].color.b = cb; verts[vi].color.a = 255;
			}
			PLATFORM_RENDERER_apply_sdl_texture_blend_mode(platform_renderer_white_texture);
			PLATFORM_RENDERER_set_texture_color_alpha_cached(platform_renderer_white_texture, 255, 255, 255, 255);
			if (SDL_RenderGeometry(platform_renderer_sdl_renderer, platform_renderer_white_texture,
					verts, 4, idx, 6) == 0)
			{
				if (platform_renderer_white_texture != platform_renderer_last_geom_texture)
				{
					platform_renderer_sdl_tx_switch_count++;
					platform_renderer_last_geom_texture = platform_renderer_white_texture;
				}
				platform_renderer_sdl_native_draw_count++;
				platform_renderer_sdl_native_textured_draw_count++;
			}
		}
		else
		{
			PLATFORM_RENDERER_apply_sdl_draw_blend_mode();
			(void)SDL_SetRenderDrawColor(
					platform_renderer_sdl_renderer,
					PLATFORM_RENDERER_clamp_sdl_colour_mod(r),
					PLATFORM_RENDERER_clamp_sdl_colour_mod(g),
					PLATFORM_RENDERER_clamp_sdl_colour_mod(b),
					255);
			if (SDL_RenderFillRect(platform_renderer_sdl_renderer, &rect) == 0)
			{
				platform_renderer_sdl_native_draw_count++;
			}
		}
	}
}

void PLATFORM_RENDERER_draw_line(int x1, int y1, int x2, int y2, int r, int g, int b)
{
	if (PLATFORM_RENDERER_is_sdl2_stub_ready())
	{
		PLATFORM_RENDERER_apply_sdl_draw_blend_mode();
		(void)SDL_SetRenderDrawColor(
				platform_renderer_sdl_renderer,
				PLATFORM_RENDERER_clamp_sdl_colour_mod(r),
				PLATFORM_RENDERER_clamp_sdl_colour_mod(g),
				PLATFORM_RENDERER_clamp_sdl_colour_mod(b),
				255);
		if (SDL_RenderDrawLine(platform_renderer_sdl_renderer, x1, y1, x2, y2) == 0)
		{
			platform_renderer_sdl_native_draw_count++;
		}
	}
}

void PLATFORM_RENDERER_draw_coloured_line(float x1, float y1, float x2, float y2, float r, float g, float b)
{
	if (PLATFORM_RENDERER_is_sdl2_stub_ready() && (platform_renderer_present_height > 0))
	{
		float tx1;
		float ty1;
		float tx2;
		float ty2;
		PLATFORM_RENDERER_transform_point(x1, y1, &tx1, &ty1);
		PLATFORM_RENDERER_transform_point(x2, y2, &tx2, &ty2);
		PLATFORM_RENDERER_apply_sdl_draw_blend_mode();
		PLATFORM_RENDERER_set_sdl_draw_colour_from_floats(r, g, b, platform_renderer_current_colour_a);
		if (SDL_RenderDrawLine(platform_renderer_sdl_renderer, (int)tx1, (int)((float)platform_renderer_present_height - ty1), (int)tx2, (int)((float)platform_renderer_present_height - ty2)) == 0)
		{
			platform_renderer_sdl_native_draw_count++;
		}
	}
}

void PLATFORM_RENDERER_draw_line(float x1, float y1, float x2, float y2)
{
	if (PLATFORM_RENDERER_is_sdl2_stub_ready() && (platform_renderer_present_height > 0))
	{
		float tx1;
		float ty1;
		float tx2;
		float ty2;
		PLATFORM_RENDERER_transform_point(x1, y1, &tx1, &ty1);
		PLATFORM_RENDERER_transform_point(x2, y2, &tx2, &ty2);
		PLATFORM_RENDERER_apply_sdl_draw_blend_mode();
		PLATFORM_RENDERER_set_sdl_draw_colour_from_floats(
				platform_renderer_current_colour_r,
				platform_renderer_current_colour_g,
				platform_renderer_current_colour_b,
				platform_renderer_current_colour_a);
		if (SDL_RenderDrawLine(platform_renderer_sdl_renderer, (int)tx1, (int)((float)platform_renderer_present_height - ty1), (int)tx2, (int)((float)platform_renderer_present_height - ty2)) == 0)
		{
			platform_renderer_sdl_native_draw_count++;
		}
	}
}

void PLATFORM_RENDERER_draw_line_loop_array(const float *x, const float *y, int count)
{
	int i;

	if ((x == NULL) || (y == NULL) || (count < 2))
	{
		return;
	}
	if (true && PLATFORM_RENDERER_is_sdl2_stub_ready() && (platform_renderer_present_height > 0))
	{
		PLATFORM_RENDERER_apply_sdl_draw_blend_mode();
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
			if (SDL_RenderDrawLine(platform_renderer_sdl_renderer, (int)tx1, (int)((float)platform_renderer_present_height - ty1), (int)tx2, (int)((float)platform_renderer_present_height - ty2)) == 0)
			{
				platform_renderer_sdl_native_draw_count++;
			}
		}
	}
}

void PLATFORM_RENDERER_draw_circle(int x, int y, int radius, int r, int g, int b, int virtual_screen_height, int resolution)
{
	float angle;
	float step;

	if (resolution <= 0)
	{
		return;
	}

	step = (2.0f * 3.14159265358979323846f) / (float)resolution;

	if (true && PLATFORM_RENDERER_is_sdl2_stub_ready())
	{
		PLATFORM_RENDERER_apply_sdl_draw_blend_mode();
		(void)SDL_SetRenderDrawColor(
				platform_renderer_sdl_renderer,
				PLATFORM_RENDERER_clamp_sdl_colour_mod(r),
				PLATFORM_RENDERER_clamp_sdl_colour_mod(g),
				PLATFORM_RENDERER_clamp_sdl_colour_mod(b),
				255);
		for (angle = 0.0f; angle < (2.0f * 3.14159265358979323846f); angle += step)
		{
			float cx1 = (float)radius * cosf(angle);
			float cy1 = (float)radius * sinf(angle);
			float cx2 = (float)radius * cosf(angle + step);
			float cy2 = (float)radius * sinf(angle + step);
			int x_start = x + (int)cx1;
			int y_start = y + (int)cy1;
			int x_end = x + (int)cx2;
			int y_end = y + (int)cy2;
			if (SDL_RenderDrawLine(platform_renderer_sdl_renderer, x_start, y_start, x_end, y_end) == 0)
			{
				platform_renderer_sdl_native_draw_count++;
			}
		}
	}
}

void PLATFORM_RENDERER_draw_bound_solid_quad(float left, float right, float up, float down)
{
	if (platform_renderer_sdl_core_quads_enabled && true && PLATFORM_RENDERER_is_sdl2_stub_ready() && (platform_renderer_present_height > 0))
	{
		float tx0;
		float ty0;
		float tx1;
		float ty1;
		PLATFORM_RENDERER_transform_point(left, up, &tx0, &ty0);
		PLATFORM_RENDERER_transform_point(right, down, &tx1, &ty1);

		SDL_Rect rect;
		rect.x = (int)((tx0 < tx1) ? tx0 : tx1);
		rect.y = (int)((float)platform_renderer_present_height - ((ty0 > ty1) ? ty0 : ty1));
		rect.w = (int)fabsf(tx1 - tx0);
		rect.h = (int)fabsf(ty1 - ty0);
		if (rect.w <= 0)
		{
			rect.w = 1;
		}
		if (rect.h <= 0)
		{
			rect.h = 1;
		}

		if (platform_renderer_white_texture != NULL)
		{
			const Uint8 cr = PLATFORM_RENDERER_clamp_sdl_unit_to_byte(platform_renderer_current_colour_r);
			const Uint8 cg = PLATFORM_RENDERER_clamp_sdl_unit_to_byte(platform_renderer_current_colour_g);
			const Uint8 cb = PLATFORM_RENDERER_clamp_sdl_unit_to_byte(platform_renderer_current_colour_b);
			const Uint8 ca = PLATFORM_RENDERER_clamp_sdl_unit_to_byte(platform_renderer_current_colour_a);
			SDL_Vertex verts[4];
			static const int idx[6] = {0, 1, 2, 0, 2, 3};
			verts[0].position.x = (float)rect.x;             verts[0].position.y = (float)rect.y;
			verts[1].position.x = (float)(rect.x + rect.w);  verts[1].position.y = (float)rect.y;
			verts[2].position.x = (float)(rect.x + rect.w);  verts[2].position.y = (float)(rect.y + rect.h);
			verts[3].position.x = (float)rect.x;             verts[3].position.y = (float)(rect.y + rect.h);
			for (int vi = 0; vi < 4; vi++) {
				verts[vi].tex_coord.x = 0.5f;
				verts[vi].tex_coord.y = 0.5f;
				verts[vi].color.r = cr; verts[vi].color.g = cg;
				verts[vi].color.b = cb; verts[vi].color.a = ca;
			}
			PLATFORM_RENDERER_apply_sdl_texture_blend_mode(platform_renderer_white_texture);
			PLATFORM_RENDERER_set_texture_color_alpha_cached(platform_renderer_white_texture, 255, 255, 255, 255);
			if (SDL_RenderGeometry(platform_renderer_sdl_renderer, platform_renderer_white_texture,
					verts, 4, idx, 6) == 0)
			{
				if (platform_renderer_white_texture != platform_renderer_last_geom_texture)
				{
					platform_renderer_sdl_tx_switch_count++;
					platform_renderer_last_geom_texture = platform_renderer_white_texture;
				}
				platform_renderer_sdl_native_draw_count++;
				platform_renderer_sdl_native_textured_draw_count++;
			}
		}
		else
		{
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
	}
}

/*
 * Pre-SDL-2.0.18 fallback for bound-textured-quad drawing.
 * Computes the transformed bounding box from the four quad corners and issues
 * SDL_RenderCopyEx.  This handles the common axis-aligned sprite case correctly
 * on older SDL builds (e.g. PortMaster / Ambernic muOS which ships SDL ~2.0.14).
 * Rotated/skewed quads are approximated by their bounding rectangle.
 */
static void PLATFORM_RENDERER_draw_bound_quad_legacy_fallback(
		const float *quad_x, const float *quad_y,
		float u1, float v1, float u2, float v2)
{
	/* Skip fully transparent draws before doing any geometry/SDL work. */
	if (platform_renderer_current_colour_a < (1.0f / 256.0f))
		return;
	platform_renderer_texture_entry *entry = PLATFORM_RENDERER_get_texture_entry(platform_renderer_last_bound_texture_handle);
	if (entry == NULL)
		return;
	if (!PLATFORM_RENDERER_build_sdl_texture_from_entry(entry))
		return;
	SDL_Texture *draw_texture = PLATFORM_RENDERER_get_effective_texture_for_current_blend(entry);
	if (draw_texture == NULL)
		return;

	float tx[4], ty[4];
	int i;
	for (i = 0; i < 4; i++)
	{
		PLATFORM_RENDERER_transform_point(quad_x[i], quad_y[i], &tx[i], &ty[i]);
	}

	float gl_left   = fminf(fminf(tx[0], tx[1]), fminf(tx[2], tx[3]));
	float gl_right  = fmaxf(fmaxf(tx[0], tx[1]), fmaxf(tx[2], tx[3]));
	float gl_top    = fmaxf(fmaxf(ty[0], ty[1]), fmaxf(ty[2], ty[3]));
	float gl_bottom = fminf(fminf(ty[0], ty[1]), fminf(ty[2], ty[3]));

	SDL_Rect dst_rect;
	SDL_Rect src_rect;
	dst_rect.x = (int)gl_left;
	dst_rect.y = (int)((float)platform_renderer_present_height - gl_top);
	dst_rect.w = (int)(gl_right - gl_left);
	dst_rect.h = (int)(gl_top - gl_bottom);
	if (dst_rect.w <= 0)
		dst_rect.w = 1;
	if (dst_rect.h <= 0)
		dst_rect.h = 1;

	if (!PLATFORM_RENDERER_build_safe_sdl_src_rect(entry, u1, v1, u2, v2, &src_rect))
		return;

	SDL_RendererFlip copy_flip = SDL_FLIP_NONE;
	if (u2 < u1)
		copy_flip = (SDL_RendererFlip)(copy_flip | SDL_FLIP_HORIZONTAL);
	if (!PLATFORM_RENDERER_src_flip_y_for_sdl(v1, v2))
		copy_flip = (SDL_RendererFlip)(copy_flip | SDL_FLIP_VERTICAL);

	int mod_r = (int)(platform_renderer_current_colour_r * 255.0f);
	int mod_g = (int)(platform_renderer_current_colour_g * 255.0f);
	int mod_b = (int)(platform_renderer_current_colour_b * 255.0f);
	int mod_a = (int)(platform_renderer_current_colour_a * 255.0f);
	if (PLATFORM_RENDERER_using_subtractive_mod_fallback())
	{
		mod_r = 255;
		mod_g = 255;
		mod_b = 255;
	}
	if (mod_r < 0) mod_r = 0; if (mod_r > 255) mod_r = 255;
	if (mod_g < 0) mod_g = 0; if (mod_g > 255) mod_g = 255;
	if (mod_b < 0) mod_b = 0; if (mod_b > 255) mod_b = 255;
	if (mod_a < 0) mod_a = 0; if (mod_a > 255) mod_a = 255;

	PLATFORM_RENDERER_apply_sdl_texture_blend_mode(draw_texture);
	PLATFORM_RENDERER_set_texture_color_alpha_cached(draw_texture, (Uint8)mod_r, (Uint8)mod_g, (Uint8)mod_b, PLATFORM_RENDERER_get_sdl_texture_alpha_mod((Uint8)mod_a));
	if (PLATFORM_RENDERER_sdl_copy_ex(platform_renderer_sdl_renderer, draw_texture, &src_rect, &dst_rect, 0.0, NULL, copy_flip) == 0)
	{
		platform_renderer_sdl_native_draw_count++;
		platform_renderer_sdl_native_textured_draw_count++;
	}
}

void PLATFORM_RENDERER_draw_bound_textured_quad(float left, float right, float up, float down, float u1, float v1, float u2, float v2)
{
	/* Skip fully transparent draws early: SDL API calls have non-trivial CPU cost per-call.
	 * When alpha is zero the draw contributes nothing visible. */
	if (platform_renderer_current_colour_a < (1.0f / 256.0f))
		return;
	static unsigned int sdl_bound_colour_diag_counter = 0;
	if (true && PLATFORM_RENDERER_is_sdl2_stub_ready() && (platform_renderer_present_height > 0))
	{
#if SDL_VERSION_ATLEAST(2, 0, 18)
		platform_renderer_texture_entry *entry = PLATFORM_RENDERER_get_texture_entry(platform_renderer_last_bound_texture_handle);
		if (entry != NULL)
		{
			if (PLATFORM_RENDERER_build_sdl_texture_from_entry(entry))
			{
				SDL_Texture *draw_texture = PLATFORM_RENDERER_get_effective_texture_for_current_blend(entry);
				float quad_x[4] = {left, left, right, right};
				float quad_y[4] = {up, down, down, up};
				float quad_u[4] = {u1, u1, u2, u2};
				float quad_v[4] = {v1, v2, v2, v1};
				float tx[4];
				float ty[4];
				SDL_Vertex vertices[4];
				int i;
				bool src_flip_x;
				bool src_flip_y;
				bool geom_flip_x = false;
				bool geom_flip_y = false;
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
					vertices[i].position.y = (float)platform_renderer_present_height - ty[i];
					vertices[i].tex_coord.x = quad_u[i];
					vertices[i].tex_coord.y = quad_v[i];

					cr = (int)(platform_renderer_current_colour_r * 255.0f);
					cg = (int)(platform_renderer_current_colour_g * 255.0f);
					cb = (int)(platform_renderer_current_colour_b * 255.0f);
					ca = (int)(platform_renderer_current_colour_a * 255.0f);
					if (PLATFORM_RENDERER_using_subtractive_mod_fallback())
					{
						/*
						 * MOD-based subtractive fallback expects the texture to carry
						 * (1-src) already. Additional vertex RGB modulation can turn
						 * transparent texels into black matte rectangles.
						 */
						cr = 255;
						cg = 255;
						cb = 255;
					}
					if (cr < 0)
						cr = 0;
					if (cr > 255)
						cr = 255;
					if (cg < 0)
						cg = 0;
					if (cg > 255)
						cg = 255;
					if (cb < 0)
						cb = 0;
					if (cb > 255)
						cb = 255;
					if (ca < 0)
						ca = 0;
					if (ca > 255)
						ca = 255;
					vertices[i].color.r = (Uint8)cr;
					vertices[i].color.g = (Uint8)cg;
					vertices[i].color.b = (Uint8)cb;
					vertices[i].color.a = (Uint8)ca;
				}

				src_flip_x = (u2 < u1);
				src_flip_y = PLATFORM_RENDERER_src_flip_y_for_sdl(v1, v2);
				PLATFORM_RENDERER_axis_orientation_flips_from_transformed_quad(tx, ty, &geom_flip_x, &geom_flip_y);
				{
					const float tx_min = fminf(fminf(tx[0], tx[1]), fminf(tx[2], tx[3]));
					const float tx_max = fmaxf(fmaxf(tx[0], tx[1]), fmaxf(tx[2], tx[3]));
					const float ty_min = fminf(fminf(ty[0], ty[1]), fminf(ty[2], ty[3]));
					const float ty_max = fmaxf(fmaxf(ty[0], ty[1]), fmaxf(ty[2], ty[3]));
					const float sy_min = fminf(fminf(vertices[0].position.y, vertices[1].position.y), fminf(vertices[2].position.y, vertices[3].position.y));
					const float sy_max = fmaxf(fmaxf(vertices[0].position.y, vertices[1].position.y), fmaxf(vertices[2].position.y, vertices[3].position.y));
					const int vp_w = platform_renderer_present_width;
					const int vp_h = platform_renderer_present_height;
					const bool offscreen =
							(tx_max < 0.0f) ||
							(tx_min > (float)vp_w) ||
							(sy_max < 0.0f) ||
							(sy_min > (float)vp_h);
				}
				is_axis_aligned =
						(fabsf(tx[0] - tx[1]) < axis_eps) &&
						(fabsf(tx[2] - tx[3]) < axis_eps) &&
						(fabsf(ty[0] - ty[3]) < axis_eps) &&
						(fabsf(ty[1] - ty[2]) < axis_eps);

				if (is_axis_aligned)
				{
					SDL_Rect dst_rect;
					SDL_Rect src_rect;
					SDL_Rect src_rect_no_vflip;
					float gl_left = (tx[0] < tx[2]) ? tx[0] : tx[2];
					float gl_right = (tx[0] > tx[2]) ? tx[0] : tx[2];
					float gl_top = (ty[0] > ty[2]) ? ty[0] : ty[2];
					float gl_bottom = (ty[0] < ty[2]) ? ty[0] : ty[2];
					int mod_r = (int)(platform_renderer_current_colour_r * 255.0f);
					int mod_g = (int)(platform_renderer_current_colour_g * 255.0f);
					int mod_b = (int)(platform_renderer_current_colour_b * 255.0f);
					int mod_a = (int)(platform_renderer_current_colour_a * 255.0f);
					if (PLATFORM_RENDERER_using_subtractive_mod_fallback())
					{
						mod_r = 255;
						mod_g = 255;
						mod_b = 255;
					}

					dst_rect.x = (int)gl_left;
					dst_rect.y = (int)((float)platform_renderer_present_height - gl_top);
					dst_rect.w = (int)(gl_right - gl_left);
					dst_rect.h = (int)(gl_top - gl_bottom);
					if (dst_rect.w <= 0)
					{
						dst_rect.w = 1;
					}
					if (dst_rect.h <= 0)
					{
						dst_rect.h = 1;
					}

					if (mod_r < 0)
						mod_r = 0;
					if (mod_r > 255)
						mod_r = 255;
					if (mod_g < 0)
						mod_g = 0;
					if (mod_g > 255)
						mod_g = 255;
					if (mod_b < 0)
						mod_b = 0;
					if (mod_b > 255)
						mod_b = 255;
					if (mod_a < 0)
						mod_a = 0;
					if (mod_a > 255)
						mod_a = 255;
					sdl_bound_colour_diag_counter++;
					if ((sdl_bound_colour_diag_counter <= 120u) &&
							((mod_r < 8) || (mod_g < 8) || (mod_b < 8)))
					{
					}

					if (PLATFORM_RENDERER_draw_axis_wrapped_u_repeat(
									entry,
									draw_texture,
									&dst_rect,
									u1, v1, u2, v2,
									(Uint8)mod_r, (Uint8)mod_g, (Uint8)mod_b, (Uint8)mod_a,
									PLATFORM_RENDERER_GEOM_SRC_BOUND_QUAD))
					{
						return;
					}
					{
						if (PLATFORM_RENDERER_build_safe_sdl_src_rect(entry, u1, v1, u2, v2, &src_rect) &&
								PLATFORM_RENDERER_build_safe_sdl_src_rect(entry, u1, 1.0f - v1, u2, 1.0f - v2, &src_rect_no_vflip))
						{
							static unsigned int uv_diag_counter = 0;
							uv_diag_counter++;
							if ((uv_diag_counter <= 120u) || ((uv_diag_counter % 1000u) == 0u))
							{
								const float cov_current = PLATFORM_RENDERER_estimate_alpha_coverage(entry, &src_rect);
								const float cov_alt = PLATFORM_RENDERER_estimate_alpha_coverage(entry, &src_rect_no_vflip);
							}
						}

						/*
						 * Geometry path keeps vertex orientation, so only apply
						 * source UV flips here. geom_flip_* was only needed for
						 * copy/copyex rectangle fallbacks.
						 */
						const bool effective_flip_x = src_flip_x;
						const bool effective_flip_y = src_flip_y;
						const float u_left = effective_flip_x ? u2 : u1;
						const float u_right = effective_flip_x ? u1 : u2;
						const float v_top = effective_flip_y ? v2 : v1;
						const float v_bottom = effective_flip_y ? v1 : v2;
						int indices[6] = {0, 1, 2, 0, 2, 3};

						vertices[0].tex_coord.x = u_left;
						vertices[0].tex_coord.y = v_top;
						vertices[1].tex_coord.x = u_left;
						vertices[1].tex_coord.y = v_bottom;
						vertices[2].tex_coord.x = u_right;
						vertices[2].tex_coord.y = v_bottom;
						vertices[3].tex_coord.x = u_right;
						vertices[3].tex_coord.y = v_top;
						vertices[0].color.r = vertices[1].color.r = vertices[2].color.r = vertices[3].color.r = (Uint8)mod_r;
						vertices[0].color.g = vertices[1].color.g = vertices[2].color.g = vertices[3].color.g = (Uint8)mod_g;
						vertices[0].color.b = vertices[1].color.b = vertices[2].color.b = vertices[3].color.b = (Uint8)mod_b;
						vertices[0].color.a = vertices[1].color.a = vertices[2].color.a = vertices[3].color.a = (Uint8)mod_a;

						if (PLATFORM_RENDERER_try_sdl_geometry_textured(draw_texture, vertices, 4, indices, 6, PLATFORM_RENDERER_GEOM_SRC_BOUND_QUAD))
						{
							PLATFORM_RENDERER_record_sdl_source_bounds_rect(PLATFORM_RENDERER_GEOM_SRC_BOUND_QUAD, &dst_rect);
						}
						else if (PLATFORM_RENDERER_build_safe_sdl_src_rect(entry, u1, v1, u2, v2, &src_rect))
						{
							const bool copy_flip_x = src_flip_x ^ geom_flip_x;
							const bool copy_flip_y = src_flip_y ^ geom_flip_y;
							SDL_RendererFlip copy_flip = SDL_FLIP_NONE;
							if (copy_flip_x)
							{
								copy_flip = (SDL_RendererFlip)(copy_flip | SDL_FLIP_HORIZONTAL);
							}
							if (copy_flip_y)
							{
								copy_flip = (SDL_RendererFlip)(copy_flip | SDL_FLIP_VERTICAL);
							}

							PLATFORM_RENDERER_apply_sdl_texture_blend_mode(draw_texture);
							PLATFORM_RENDERER_set_texture_color_alpha_cached(draw_texture, (Uint8)mod_r, (Uint8)mod_g, (Uint8)mod_b, PLATFORM_RENDERER_get_sdl_texture_alpha_mod((Uint8)mod_a));
							{
								/*
								 * Log the first 3 bound-draws per frame for the first 10 frames.
								 * Detect frame boundary by watching textured_draw_count reset.
								 */
								static unsigned int bd_frame_seq = 0;
								static unsigned int bd_prev_tex = 0;
								static unsigned int bd_frame_num = 0;
								if (platform_renderer_sdl_native_textured_draw_count < bd_prev_tex)
								{
									bd_frame_seq = 0;
									bd_frame_num++;
								}
								bd_prev_tex = platform_renderer_sdl_native_textured_draw_count;
								bd_frame_seq++;
								if ((bd_frame_num <= 10) && (bd_frame_seq <= 3))
								{
									fprintf(stderr, "[BOUND-DRAW f=%u s=%u] dst=(%d,%d %dx%d) src=(%d,%d %dx%d) flip=%d mod=(%d,%d,%d,%d)\n",
										bd_frame_num, bd_frame_seq,
										dst_rect.x, dst_rect.y, dst_rect.w, dst_rect.h,
										src_rect.x, src_rect.y, src_rect.w, src_rect.h,
										(int)copy_flip, mod_r, mod_g, mod_b, mod_a);
								}
							}
							if (PLATFORM_RENDERER_sdl_copy_ex(platform_renderer_sdl_renderer, draw_texture, &src_rect, &dst_rect, 0.0, NULL, copy_flip) == 0)
							{
								platform_renderer_sdl_native_draw_count++;
								platform_renderer_sdl_native_textured_draw_count++;
								PLATFORM_RENDERER_record_sdl_source_bounds_rect(PLATFORM_RENDERER_GEOM_SRC_BOUND_QUAD, &dst_rect);
							}
						}
					}
				}
				else
				{
					int indices[6] = {0, 1, 2, 0, 2, 3};
					if (!PLATFORM_RENDERER_try_sdl_geometry_textured(draw_texture, vertices, 4, indices, 6, PLATFORM_RENDERER_GEOM_SRC_BOUND_QUAD))
					{
						SDL_Rect src_rect;
						SDL_Rect dst_rect;
						const float tx_min = fminf(fminf(tx[0], tx[1]), fminf(tx[2], tx[3]));
						const float tx_max = fmaxf(fmaxf(tx[0], tx[1]), fmaxf(tx[2], tx[3]));
						const float ty_min = fminf(fminf(ty[0], ty[1]), fminf(ty[2], ty[3]));
						const float ty_max = fmaxf(fmaxf(ty[0], ty[1]), fmaxf(ty[2], ty[3]));
						const bool copy_flip_x = src_flip_x ^ geom_flip_x;
						const bool copy_flip_y = src_flip_y ^ geom_flip_y;
						SDL_RendererFlip copy_flip = SDL_FLIP_NONE;

						if (PLATFORM_RENDERER_build_safe_sdl_src_rect(entry, u1, v1, u2, v2, &src_rect))
						{
							dst_rect.x = (int)tx_min;
							dst_rect.y = (int)((float)platform_renderer_present_height - ty_max);
							dst_rect.w = (int)(tx_max - tx_min);
							dst_rect.h = (int)(ty_max - ty_min);
							if (dst_rect.w <= 0)
							{
								dst_rect.w = 1;
							}
							if (dst_rect.h <= 0)
							{
								dst_rect.h = 1;
							}

							if (copy_flip_x)
							{
								copy_flip = (SDL_RendererFlip)(copy_flip | SDL_FLIP_HORIZONTAL);
							}
							if (copy_flip_y)
							{
								copy_flip = (SDL_RendererFlip)(copy_flip | SDL_FLIP_VERTICAL);
							}

							PLATFORM_RENDERER_apply_sdl_texture_blend_mode(draw_texture);
							PLATFORM_RENDERER_set_texture_color_alpha_cached(
									draw_texture,
									PLATFORM_RENDERER_clamp_sdl_unit_to_byte(platform_renderer_current_colour_r),
									PLATFORM_RENDERER_clamp_sdl_unit_to_byte(platform_renderer_current_colour_g),
									PLATFORM_RENDERER_clamp_sdl_unit_to_byte(platform_renderer_current_colour_b),
									PLATFORM_RENDERER_get_sdl_texture_alpha_mod(PLATFORM_RENDERER_clamp_sdl_unit_to_byte(platform_renderer_current_colour_a)));
							if (PLATFORM_RENDERER_sdl_copy_ex(platform_renderer_sdl_renderer, draw_texture, &src_rect, &dst_rect, 0.0, NULL, copy_flip) == 0)
							{
								platform_renderer_sdl_native_draw_count++;
								platform_renderer_sdl_native_textured_draw_count++;
								PLATFORM_RENDERER_record_sdl_source_bounds_rect(PLATFORM_RENDERER_GEOM_SRC_BOUND_QUAD, &dst_rect);
							}
						}
					}
				}
			}
		}
#else
		{
			/* Pre-2.0.18 fallback */
			float quad_x[4] = {left, left, right, right};
			float quad_y[4] = {up, down, down, up};
			PLATFORM_RENDERER_draw_bound_quad_legacy_fallback(quad_x, quad_y, u1, v1, u2, v2);
		}
#endif
	}
}

void PLATFORM_RENDERER_draw_bound_textured_quad_custom(float x0, float y0, float x1, float y1, float x2, float y2, float x3, float y3, float u1, float v1, float u2, float v2)
{
	if (true && PLATFORM_RENDERER_is_sdl2_stub_ready() && (platform_renderer_present_height > 0))
	{
#if SDL_VERSION_ATLEAST(2, 0, 18)
		platform_renderer_texture_entry *entry = PLATFORM_RENDERER_get_texture_entry(platform_renderer_last_bound_texture_handle);
		if (entry != NULL)
		{
			if (PLATFORM_RENDERER_build_sdl_texture_from_entry(entry))
			{
				SDL_Texture *draw_texture = PLATFORM_RENDERER_get_effective_texture_for_current_blend(entry);
				float quad_x[4] = {x0, x1, x2, x3};
				float quad_y[4] = {y0, y1, y2, y3};
				float quad_u[4] = {u1, u1, u2, u2};
				float quad_v[4] = {v1, v2, v2, v1};
				float tx[4];
				float ty[4];
				SDL_Vertex vertices[4];
				int i;
				bool src_flip_x;
				bool src_flip_y;
				bool geom_flip_x = false;
				bool geom_flip_y = false;
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
					vertices[i].position.y = (float)platform_renderer_present_height - ty[i];
					vertices[i].tex_coord.x = quad_u[i];
					vertices[i].tex_coord.y = quad_v[i];

					cr = (int)(platform_renderer_current_colour_r * 255.0f);
					cg = (int)(platform_renderer_current_colour_g * 255.0f);
					cb = (int)(platform_renderer_current_colour_b * 255.0f);
					ca = (int)(platform_renderer_current_colour_a * 255.0f);
					if (PLATFORM_RENDERER_using_subtractive_mod_fallback())
					{
						cr = 255;
						cg = 255;
						cb = 255;
					}
					if (cr < 0)
						cr = 0;
					if (cr > 255)
						cr = 255;
					if (cg < 0)
						cg = 0;
					if (cg > 255)
						cg = 255;
					if (cb < 0)
						cb = 0;
					if (cb > 255)
						cb = 255;
					if (ca < 0)
						ca = 0;
					if (ca > 255)
						ca = 255;
					vertices[i].color.r = (Uint8)cr;
					vertices[i].color.g = (Uint8)cg;
					vertices[i].color.b = (Uint8)cb;
					vertices[i].color.a = (Uint8)ca;
				}

				src_flip_x = (u2 < u1);
				src_flip_y = PLATFORM_RENDERER_src_flip_y_for_sdl(v1, v2);
				PLATFORM_RENDERER_axis_orientation_flips_from_transformed_quad(tx, ty, &geom_flip_x, &geom_flip_y);
				is_axis_aligned =
						(fabsf(tx[0] - tx[1]) < axis_eps) &&
						(fabsf(tx[2] - tx[3]) < axis_eps) &&
						(fabsf(ty[0] - ty[3]) < axis_eps) &&
						(fabsf(ty[1] - ty[2]) < axis_eps);

				if (is_axis_aligned)
				{
					SDL_Rect dst_rect;
					SDL_Rect src_rect;
					float gl_left = (tx[0] < tx[2]) ? tx[0] : tx[2];
					float gl_right = (tx[0] > tx[2]) ? tx[0] : tx[2];
					float gl_top = (ty[0] > ty[2]) ? ty[0] : ty[2];
					float gl_bottom = (ty[0] < ty[2]) ? ty[0] : ty[2];
					int mod_r = (int)(platform_renderer_current_colour_r * 255.0f);
					int mod_g = (int)(platform_renderer_current_colour_g * 255.0f);
					int mod_b = (int)(platform_renderer_current_colour_b * 255.0f);
					int mod_a = (int)(platform_renderer_current_colour_a * 255.0f);
					if (PLATFORM_RENDERER_using_subtractive_mod_fallback())
					{
						mod_r = 255;
						mod_g = 255;
						mod_b = 255;
					}

					dst_rect.x = (int)gl_left;
					dst_rect.y = (int)((float)platform_renderer_present_height - gl_top);
					dst_rect.w = (int)(gl_right - gl_left);
					dst_rect.h = (int)(gl_top - gl_bottom);
					if (dst_rect.w <= 0)
					{
						dst_rect.w = 1;
					}
					if (dst_rect.h <= 0)
					{
						dst_rect.h = 1;
					}

					if (mod_r < 0)
						mod_r = 0;
					if (mod_r > 255)
						mod_r = 255;
					if (mod_g < 0)
						mod_g = 0;
					if (mod_g > 255)
						mod_g = 255;
					if (mod_b < 0)
						mod_b = 0;
					if (mod_b > 255)
						mod_b = 255;
					if (mod_a < 0)
						mod_a = 0;
					if (mod_a > 255)
						mod_a = 255;

					if (PLATFORM_RENDERER_draw_axis_wrapped_u_repeat(
									entry,
									draw_texture,
									&dst_rect,
									u1, v1, u2, v2,
									(Uint8)mod_r, (Uint8)mod_g, (Uint8)mod_b, (Uint8)mod_a,
									PLATFORM_RENDERER_GEOM_SRC_BOUND_QUAD_CUSTOM))
					{
						return;
					}
					{
						/*
						 * Geometry path keeps vertex orientation, so only apply
						 * source UV flips here. geom_flip_* was only needed for
						 * copy/copyex rectangle fallbacks.
						 */
						const bool effective_flip_x = src_flip_x;
						const bool effective_flip_y = src_flip_y;
						const float u_left = effective_flip_x ? u2 : u1;
						const float u_right = effective_flip_x ? u1 : u2;
						const float v_top = effective_flip_y ? v2 : v1;
						const float v_bottom = effective_flip_y ? v1 : v2;
						int indices[6] = {0, 1, 2, 0, 2, 3};

						vertices[0].tex_coord.x = u_left;
						vertices[0].tex_coord.y = v_top;
						vertices[1].tex_coord.x = u_left;
						vertices[1].tex_coord.y = v_bottom;
						vertices[2].tex_coord.x = u_right;
						vertices[2].tex_coord.y = v_bottom;
						vertices[3].tex_coord.x = u_right;
						vertices[3].tex_coord.y = v_top;
						vertices[0].color.r = vertices[1].color.r = vertices[2].color.r = vertices[3].color.r = (Uint8)mod_r;
						vertices[0].color.g = vertices[1].color.g = vertices[2].color.g = vertices[3].color.g = (Uint8)mod_g;
						vertices[0].color.b = vertices[1].color.b = vertices[2].color.b = vertices[3].color.b = (Uint8)mod_b;
						vertices[0].color.a = vertices[1].color.a = vertices[2].color.a = vertices[3].color.a = (Uint8)mod_a;

						if (PLATFORM_RENDERER_try_sdl_geometry_textured(draw_texture, vertices, 4, indices, 6, PLATFORM_RENDERER_GEOM_SRC_BOUND_QUAD_CUSTOM))
						{
							PLATFORM_RENDERER_record_sdl_source_bounds_rect(PLATFORM_RENDERER_GEOM_SRC_BOUND_QUAD_CUSTOM, &dst_rect);
						}
						else if (PLATFORM_RENDERER_build_safe_sdl_src_rect(entry, u1, v1, u2, v2, &src_rect))
						{
							const bool copy_flip_x = src_flip_x ^ geom_flip_x;
							const bool copy_flip_y = src_flip_y ^ geom_flip_y;
							SDL_RendererFlip copy_flip = SDL_FLIP_NONE;
							if (copy_flip_x)
							{
								copy_flip = (SDL_RendererFlip)(copy_flip | SDL_FLIP_HORIZONTAL);
							}
							if (copy_flip_y)
							{
								copy_flip = (SDL_RendererFlip)(copy_flip | SDL_FLIP_VERTICAL);
							}

							PLATFORM_RENDERER_apply_sdl_texture_blend_mode(draw_texture);
							PLATFORM_RENDERER_set_texture_color_alpha_cached(draw_texture, (Uint8)mod_r, (Uint8)mod_g, (Uint8)mod_b, PLATFORM_RENDERER_get_sdl_texture_alpha_mod((Uint8)mod_a));
							if (PLATFORM_RENDERER_sdl_copy_ex(platform_renderer_sdl_renderer, draw_texture, &src_rect, &dst_rect, 0.0, NULL, copy_flip) == 0)
							{
								platform_renderer_sdl_native_draw_count++;
								platform_renderer_sdl_native_textured_draw_count++;
								PLATFORM_RENDERER_record_sdl_source_bounds_rect(PLATFORM_RENDERER_GEOM_SRC_BOUND_QUAD_CUSTOM, &dst_rect);
							}
						}
					}
				}
				else
				{
					int indices[6] = {0, 1, 2, 0, 2, 3};
					if (!PLATFORM_RENDERER_try_sdl_geometry_textured(draw_texture, vertices, 4, indices, 6, PLATFORM_RENDERER_GEOM_SRC_BOUND_QUAD_CUSTOM))
					{
						SDL_Rect src_rect;
						SDL_Rect dst_rect;
						const float tx_min = fminf(fminf(tx[0], tx[1]), fminf(tx[2], tx[3]));
						const float tx_max = fmaxf(fmaxf(tx[0], tx[1]), fmaxf(tx[2], tx[3]));
						const float ty_min = fminf(fminf(ty[0], ty[1]), fminf(ty[2], ty[3]));
						const float ty_max = fmaxf(fmaxf(ty[0], ty[1]), fmaxf(ty[2], ty[3]));
						const bool copy_flip_x = src_flip_x ^ geom_flip_x;
						const bool copy_flip_y = src_flip_y ^ geom_flip_y;
						SDL_RendererFlip copy_flip = SDL_FLIP_NONE;

						if (PLATFORM_RENDERER_build_safe_sdl_src_rect(entry, u1, v1, u2, v2, &src_rect))
						{
							dst_rect.x = (int)tx_min;
							dst_rect.y = (int)((float)platform_renderer_present_height - ty_max);
							dst_rect.w = (int)(tx_max - tx_min);
							dst_rect.h = (int)(ty_max - ty_min);
							if (dst_rect.w <= 0)
							{
								dst_rect.w = 1;
							}
							if (dst_rect.h <= 0)
							{
								dst_rect.h = 1;
							}

							if (copy_flip_x)
							{
								copy_flip = (SDL_RendererFlip)(copy_flip | SDL_FLIP_HORIZONTAL);
							}
							if (copy_flip_y)
							{
								copy_flip = (SDL_RendererFlip)(copy_flip | SDL_FLIP_VERTICAL);
							}

							PLATFORM_RENDERER_apply_sdl_texture_blend_mode(draw_texture);
							PLATFORM_RENDERER_set_texture_color_alpha_cached(
									draw_texture,
									PLATFORM_RENDERER_clamp_sdl_unit_to_byte(platform_renderer_current_colour_r),
									PLATFORM_RENDERER_clamp_sdl_unit_to_byte(platform_renderer_current_colour_g),
									PLATFORM_RENDERER_clamp_sdl_unit_to_byte(platform_renderer_current_colour_b),
									PLATFORM_RENDERER_get_sdl_texture_alpha_mod(PLATFORM_RENDERER_clamp_sdl_unit_to_byte(platform_renderer_current_colour_a)));
							if (PLATFORM_RENDERER_sdl_copy_ex(platform_renderer_sdl_renderer, draw_texture, &src_rect, &dst_rect, 0.0, NULL, copy_flip) == 0)
							{
								platform_renderer_sdl_native_draw_count++;
								platform_renderer_sdl_native_textured_draw_count++;
								PLATFORM_RENDERER_record_sdl_source_bounds_rect(PLATFORM_RENDERER_GEOM_SRC_BOUND_QUAD_CUSTOM, &dst_rect);
							}
						}
					}
				}
			}
		}
#else
		{
			/* Pre-2.0.18 fallback */
			float quad_x[4] = {x0, x1, x2, x3};
			float quad_y[4] = {y0, y1, y2, y3};
			PLATFORM_RENDERER_draw_bound_quad_legacy_fallback(quad_x, quad_y, u1, v1, u2, v2);
		}
#endif
	}
}

void PLATFORM_RENDERER_draw_point(float x, float y)
{
	if (true && PLATFORM_RENDERER_is_sdl2_stub_ready() && (platform_renderer_present_height > 0))
	{
		float tx;
		float ty;
		PLATFORM_RENDERER_transform_point(x, y, &tx, &ty);
		/*
		 * Route point draws through SDL_RenderGeometry with the 1x1 white texture rather
		 * than SDL_RenderDrawPoint.  SDL_RenderDrawPoint emits a "draw_rects" command that
		 * forces a GPU pipeline-state flush from geometry mode on GLES2/Panfrost (~100us).
		 * With RenderGeometry all point/sprite/fill draws share one command type, so the
		 * ~400 per-frame flushes collapse to ~1-2 texture-boundary changes (~5us each).
		 */
		if (platform_renderer_white_texture != NULL)
		{
			const float px = tx;
			const float py = (float)platform_renderer_present_height - ty;
			const Uint8 cr = PLATFORM_RENDERER_clamp_sdl_unit_to_byte(platform_renderer_current_colour_r);
			const Uint8 cg = PLATFORM_RENDERER_clamp_sdl_unit_to_byte(platform_renderer_current_colour_g);
			const Uint8 cb = PLATFORM_RENDERER_clamp_sdl_unit_to_byte(platform_renderer_current_colour_b);
			const Uint8 ca = PLATFORM_RENDERER_clamp_sdl_unit_to_byte(platform_renderer_current_colour_a);
			if (ca == 0)
			{
				/* Fully transparent — skip the geometry draw entirely (no GPU work, no batch flush). */
				platform_renderer_sdl_alpha_skip_count++;
				return;
			}
			SDL_Vertex verts[4];
			static const int idx[6] = {0, 1, 2, 0, 2, 3};
			verts[0].position.x = px;        verts[0].position.y = py;
			verts[1].position.x = px + 1.0f; verts[1].position.y = py;
			verts[2].position.x = px + 1.0f; verts[2].position.y = py + 1.0f;
			verts[3].position.x = px;        verts[3].position.y = py + 1.0f;
			for (int vi = 0; vi < 4; vi++) {
				verts[vi].tex_coord.x = 0.5f;
				verts[vi].tex_coord.y = 0.5f;
				verts[vi].color.r = cr; verts[vi].color.g = cg;
				verts[vi].color.b = cb; verts[vi].color.a = ca;
			}
			PLATFORM_RENDERER_apply_sdl_texture_blend_mode(platform_renderer_white_texture);
			PLATFORM_RENDERER_set_texture_color_alpha_cached(platform_renderer_white_texture, 255, 255, 255, 255);
			if (SDL_RenderGeometry(platform_renderer_sdl_renderer, platform_renderer_white_texture,
					verts, 4, idx, 6) == 0)
			{
				if (platform_renderer_white_texture != platform_renderer_last_geom_texture)
				{
					platform_renderer_sdl_tx_switch_count++;
					platform_renderer_last_geom_texture = platform_renderer_white_texture;
				}
				platform_renderer_sdl_native_draw_count++;
				platform_renderer_sdl_native_textured_draw_count++;
			}
			return;
		}
		PLATFORM_RENDERER_apply_sdl_draw_blend_mode();
		PLATFORM_RENDERER_set_sdl_draw_colour_from_floats(
				platform_renderer_current_colour_r,
				platform_renderer_current_colour_g,
				platform_renderer_current_colour_b,
				platform_renderer_current_colour_a);
		if (SDL_RenderDrawPoint(platform_renderer_sdl_renderer, (int)tx, (int)((float)platform_renderer_present_height - ty)) == 0)
		{
			platform_renderer_sdl_native_draw_count++;
		}
	}
}

void PLATFORM_RENDERER_draw_coloured_point(float x, float y, float r, float g, float b)
{
	if (true && PLATFORM_RENDERER_is_sdl2_stub_ready() && (platform_renderer_present_height > 0))
	{
		float tx;
		float ty;
		PLATFORM_RENDERER_transform_point(x, y, &tx, &ty);
		if (platform_renderer_white_texture != NULL)
		{
			const float px = tx;
			const float py = (float)platform_renderer_present_height - ty;
			const Uint8 cr = PLATFORM_RENDERER_clamp_sdl_unit_to_byte(r);
			const Uint8 cg = PLATFORM_RENDERER_clamp_sdl_unit_to_byte(g);
			const Uint8 cb = PLATFORM_RENDERER_clamp_sdl_unit_to_byte(b);
			const Uint8 ca = PLATFORM_RENDERER_clamp_sdl_unit_to_byte(platform_renderer_current_colour_a);
			if (ca == 0)
			{
				platform_renderer_sdl_alpha_skip_count++;
				return;
			}
			SDL_Vertex verts[4];
			static const int idx[6] = {0, 1, 2, 0, 2, 3};
			verts[0].position.x = px;        verts[0].position.y = py;
			verts[1].position.x = px + 1.0f; verts[1].position.y = py;
			verts[2].position.x = px + 1.0f; verts[2].position.y = py + 1.0f;
			verts[3].position.x = px;        verts[3].position.y = py + 1.0f;
			for (int vi = 0; vi < 4; vi++) {
				verts[vi].tex_coord.x = 0.5f;
				verts[vi].tex_coord.y = 0.5f;
				verts[vi].color.r = cr; verts[vi].color.g = cg;
				verts[vi].color.b = cb; verts[vi].color.a = ca;
			}
			PLATFORM_RENDERER_apply_sdl_texture_blend_mode(platform_renderer_white_texture);
			PLATFORM_RENDERER_set_texture_color_alpha_cached(platform_renderer_white_texture, 255, 255, 255, 255);
			if (SDL_RenderGeometry(platform_renderer_sdl_renderer, platform_renderer_white_texture,
					verts, 4, idx, 6) == 0)
			{
				if (platform_renderer_white_texture != platform_renderer_last_geom_texture)
				{
					platform_renderer_sdl_tx_switch_count++;
					platform_renderer_last_geom_texture = platform_renderer_white_texture;
				}
				platform_renderer_sdl_native_draw_count++;
				platform_renderer_sdl_native_textured_draw_count++;
			}
			return;
		}
		PLATFORM_RENDERER_apply_sdl_draw_blend_mode();
		PLATFORM_RENDERER_set_sdl_draw_colour_from_floats(r, g, b, platform_renderer_current_colour_a);
		if (SDL_RenderDrawPoint(platform_renderer_sdl_renderer, (int)tx, (int)((float)platform_renderer_present_height - ty)) == 0)
		{
			platform_renderer_sdl_native_draw_count++;
		}
	}
}

void PLATFORM_RENDERER_set_window_transform(float left_window_transform_x, float top_window_transform_y, float total_scale_x, float total_scale_y)
{
	if (!isfinite(left_window_transform_x) ||
			!isfinite(top_window_transform_y) ||
			!isfinite(total_scale_x) ||
			!isfinite(total_scale_y))
	{
		PLATFORM_RENDERER_reset_transform_state("set_window_transform input");
		return;
	}

	platform_renderer_tx_a = total_scale_x;
	platform_renderer_tx_b = 0.0f;
	platform_renderer_tx_c = 0.0f;
	platform_renderer_tx_d = total_scale_y;
	platform_renderer_tx_x = left_window_transform_x;
	platform_renderer_tx_y = top_window_transform_y;
}

void PLATFORM_RENDERER_set_colour3f(float r, float g, float b)
{
	PLATFORM_RENDERER_set_colour4f(r, g, b, 1.0f);
}

void PLATFORM_RENDERER_set_colour4f(float r, float g, float b, float a)
{
	/*
	 * Diagnostic: log when colour is set to zero-alpha or all-black so we can
	 * track down which code path causes entity sprites to become invisible.
	 */
	if ((a < 0.01f) || (r < 0.01f && g < 0.01f && b < 0.01f))
	{
		static unsigned int colour_zero_count = 0;
		colour_zero_count++;
		if (colour_zero_count <= 20)
		{
			fprintf(stderr, "[COLOUR-ZERO %u] set_colour4f(%.3f, %.3f, %.3f, %.3f)\n",
				colour_zero_count, r, g, b, a);
		}
	}
	platform_renderer_current_colour_r = r;
	platform_renderer_current_colour_g = g;
	platform_renderer_current_colour_b = b;
	platform_renderer_current_colour_a = a;
}

void PLATFORM_RENDERER_bind_texture(unsigned int texture_handle)
{
	unsigned int resolved_texture_id = PLATFORM_RENDERER_resolve_legacy_texture(texture_handle);
	unsigned int normalized_texture_handle = PLATFORM_RENDERER_normalize_texture_handle(texture_handle);
	(void)resolved_texture_id;
	platform_renderer_last_bound_texture_handle = normalized_texture_handle;
}

void PLATFORM_RENDERER_set_active_texture_proc(void *proc)
{
	platform_renderer_active_texture_proc = (platform_active_texture_proc_t)proc;
}

bool PLATFORM_RENDERER_is_active_texture_available(void)
{
	return platform_renderer_active_texture_proc != NULL;
}

void PLATFORM_RENDERER_set_blend_enabled(bool enabled)
{
	platform_renderer_blend_enabled = enabled;
}

void PLATFORM_RENDERER_set_blend_mode_additive(void)
{
	platform_renderer_blend_mode = PLATFORM_RENDERER_BLEND_MODE_ADD;
}

void PLATFORM_RENDERER_set_blend_mode_multiply(void)
{
	platform_renderer_blend_mode = PLATFORM_RENDERER_BLEND_MODE_MULTIPLY;
}

void PLATFORM_RENDERER_set_blend_mode_alpha(void)
{
	platform_renderer_blend_mode = PLATFORM_RENDERER_BLEND_MODE_ALPHA;
}

void PLATFORM_RENDERER_set_blend_mode_subtractive(void)
{
	platform_renderer_blend_mode = PLATFORM_RENDERER_BLEND_MODE_SUBTRACT;
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

	if ((platform_renderer_sdl_renderer != NULL) && (platform_renderer_present_height > 0))
	{
		/* SDL clip rect uses top-left origin; transform from GL-style bottom-left space. */
		SDL_Rect clip;
		int clip_x = (int)x;
		int clip_w = (int)width;
		int clip_h = (int)height;
		int clip_y = platform_renderer_present_height - ((int)y + clip_h);

		if (clip_w < 0)
		{
			clip_x += clip_w;
			clip_w = -clip_w;
		}
		if (clip_h < 0)
		{
			clip_y += clip_h;
			clip_h = -clip_h;
		}
		if (clip_w < 0)
			clip_w = 0;
		if (clip_h < 0)
			clip_h = 0;

		/* Clamp to active SDL output viewport to avoid accidental empty/offscreen clip. */
		{
			int vp_x = 0;
			int vp_y = 0;
			int vp_w = platform_renderer_present_width;
			int vp_h = platform_renderer_present_height;
			int x0 = clip_x;
			int y0 = clip_y;
			int x1 = clip_x + clip_w;
			int y1 = clip_y + clip_h;

			if (vp_w <= 0)
				vp_w = platform_renderer_present_width;
			if (vp_h <= 0)
				vp_h = platform_renderer_present_height;

			if (x0 < vp_x)
				x0 = vp_x;
			if (y0 < vp_y)
				y0 = vp_y;
			if (x1 > (vp_x + vp_w))
				x1 = vp_x + vp_w;
			if (y1 > (vp_y + vp_h))
				y1 = vp_y + vp_h;

			clip_w = x1 - x0;
			clip_h = y1 - y0;

			if ((clip_w <= 0) || (clip_h <= 0))
			{
				/* Prefer no clipping over blank frame when window clip is invalid. */
				if (platform_renderer_sdl_clip_cache_enabled)
				{
					/* Flush deferred ADD draws before changing clip rect so they render
					 * in the clip context they were issued in, preserving draw order. */
					PLATFORM_RENDERER_flush_addq();
					(void)SDL_RenderSetClipRect(platform_renderer_sdl_renderer, NULL);
					platform_renderer_sdl_clip_cache_enabled = false;
					platform_renderer_sdl_clip_switch_count++;
				}
			}
			else
			{
				clip.x = x0;
				clip.y = y0;
				clip.w = clip_w;
				clip.h = clip_h;
				/*
				 * Panfrost pays heavily for clip-rect state changes. If the window clip
				 * covers the whole active output, treat it as "no clip" so SDL can keep
				 * one batch alive across the textured window boundary.
				 */
				if ((clip.x == vp_x) && (clip.y == vp_y) &&
						(clip.w == vp_w) && (clip.h == vp_h))
				{
					if (platform_renderer_sdl_clip_cache_enabled)
					{
						PLATFORM_RENDERER_flush_addq();
						(void)SDL_RenderSetClipRect(platform_renderer_sdl_renderer, NULL);
						platform_renderer_sdl_clip_cache_enabled = false;
						platform_renderer_sdl_clip_switch_count++;
					}
					return;
				}
				if (!platform_renderer_sdl_clip_cache_enabled ||
						platform_renderer_sdl_clip_cache.x != clip.x ||
						platform_renderer_sdl_clip_cache.y != clip.y ||
						platform_renderer_sdl_clip_cache.w != clip.w ||
						platform_renderer_sdl_clip_cache.h != clip.h)
				{
					/* Flush deferred ADD draws before changing clip rect. */
					PLATFORM_RENDERER_flush_addq();
					(void)SDL_RenderSetClipRect(platform_renderer_sdl_renderer, &clip);
					platform_renderer_sdl_clip_cache = clip;
					platform_renderer_sdl_clip_cache_enabled = true;
					platform_renderer_sdl_clip_switch_count++;
				}
			}
		}
	}
}

void PLATFORM_RENDERER_translatef(float x, float y, float z)
{
	if (!isfinite(x) || !isfinite(y) || !isfinite(z))
	{
		PLATFORM_RENDERER_reset_transform_state("translatef input");
		return;
	}
	platform_renderer_tx_x += (platform_renderer_tx_a * x) + (platform_renderer_tx_c * y);
	platform_renderer_tx_y += (platform_renderer_tx_b * x) + (platform_renderer_tx_d * y);
	if (!PLATFORM_RENDERER_transform_state_is_finite())
	{
		PLATFORM_RENDERER_reset_transform_state("translatef accum");
	}
}

void PLATFORM_RENDERER_scalef(float x, float y, float z)
{
	if (!isfinite(x) || !isfinite(y) || !isfinite(z))
	{
		PLATFORM_RENDERER_reset_transform_state("scalef input");
		return;
	}
	platform_renderer_tx_a *= x;
	platform_renderer_tx_b *= x;
	platform_renderer_tx_c *= y;
	platform_renderer_tx_d *= y;
	if (!PLATFORM_RENDERER_transform_state_is_finite())
	{
		PLATFORM_RENDERER_reset_transform_state("scalef accum");
	}
}

void PLATFORM_RENDERER_rotatef(float angle_degrees, float x, float y, float z)
{
	if (!isfinite(angle_degrees) || !isfinite(x) || !isfinite(y) || !isfinite(z))
	{
		PLATFORM_RENDERER_reset_transform_state("rotatef input");
		return;
	}
	(void)x;
	(void)y;
	(void)z;
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
	if (!PLATFORM_RENDERER_transform_state_is_finite())
	{
		PLATFORM_RENDERER_reset_transform_state("rotatef accum");
	}
}

void PLATFORM_RENDERER_finish_textured_window_draw(bool texture_combiner_available, bool had_secondary_texture, bool secondary_colour_available, bool secondary_window_colour, int game_screen_width, int game_screen_height)
{
	if (platform_renderer_sdl_renderer != NULL)
	{
		if (platform_renderer_sdl_clip_cache_enabled)
		{
			/* Flush deferred ADD draws before clearing clip at textured-window end. */
			PLATFORM_RENDERER_flush_addq();
			(void)SDL_RenderSetClipRect(platform_renderer_sdl_renderer, NULL);
			platform_renderer_sdl_clip_cache_enabled = false;
			platform_renderer_sdl_clip_switch_count++;
		}
	}
	PLATFORM_RENDERER_set_blend_enabled(false);
	PLATFORM_RENDERER_set_colour4f(1.0f, 1.0f, 1.0f, 1.0f);
}

void PLATFORM_RENDERER_draw_bound_multitextured_quad_array(const float *x, const float *y, const float *u0, const float *v0, const float *u1, const float *v1)
{
	int i;

	if ((x == NULL) || (y == NULL) || (u0 == NULL) || (v0 == NULL) || (u1 == NULL) || (v1 == NULL))
	{
		return;
	}
	if (true && PLATFORM_RENDERER_is_sdl2_stub_ready() && (platform_renderer_present_height > 0))
	{
#if SDL_VERSION_ATLEAST(2, 0, 18)
		platform_renderer_texture_entry *entry = PLATFORM_RENDERER_get_texture_entry(platform_renderer_last_bound_texture_handle);
		if (entry != NULL)
		{
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
					vertices[i].position.y = (float)platform_renderer_present_height - ty;
					vertices[i].tex_coord.x = u0[i];
					vertices[i].tex_coord.y = v0[i];

					cr = (int)(platform_renderer_current_colour_r * 255.0f);
					cg = (int)(platform_renderer_current_colour_g * 255.0f);
					cb = (int)(platform_renderer_current_colour_b * 255.0f);
					ca = (int)(platform_renderer_current_colour_a * 255.0f);

					if (cr < 0)
						cr = 0;
					if (cr > 255)
						cr = 255;
					if (cg < 0)
						cg = 0;
					if (cg > 255)
						cg = 255;
					if (cb < 0)
						cb = 0;
					if (cb > 255)
						cb = 255;
					if (ca < 0)
						ca = 0;
					if (ca > 255)
						ca = 255;

					vertices[i].color.r = (Uint8)cr;
					vertices[i].color.g = (Uint8)cg;
					vertices[i].color.b = (Uint8)cb;
					vertices[i].color.a = (Uint8)ca;
				}

				src_flip_x = (u0[2] < u0[0]);
				src_flip_y = PLATFORM_RENDERER_src_flip_y_for_sdl(v0[0], v0[1]);
				is_axis_aligned =
						(fabsf(txs[0] - txs[1]) < axis_eps) &&
						(fabsf(txs[2] - txs[3]) < axis_eps) &&
						(fabsf(tys[0] - tys[3]) < axis_eps) &&
						(fabsf(tys[1] - tys[2]) < axis_eps);

				{
					const int indices[6] = {0, 1, 2, 0, 2, 3};
					(void)is_axis_aligned;
					(void)src_flip_x;
					(void)src_flip_y;
					(void)PLATFORM_RENDERER_try_sdl_geometry_textured(entry->sdl_texture, vertices, 4, indices, 6, PLATFORM_RENDERER_GEOM_SRC_PERSPECTIVE);
					return;
				}

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

					dst_rect.x = (int)gl_left;
					dst_rect.y = (int)((float)platform_renderer_present_height - gl_top);
					dst_rect.w = (int)(gl_right - gl_left);
					dst_rect.h = (int)(gl_top - gl_bottom);
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
						final_flip = (SDL_RendererFlip)(final_flip | SDL_FLIP_HORIZONTAL);
					}
					if (src_flip_y)
					{
						final_flip = (SDL_RendererFlip)(final_flip | SDL_FLIP_VERTICAL);
					}

					PLATFORM_RENDERER_apply_sdl_texture_blend_mode(entry->sdl_texture);
					PLATFORM_RENDERER_set_texture_color_alpha_cached(
							entry->sdl_texture,
							PLATFORM_RENDERER_clamp_sdl_unit_to_byte(platform_renderer_current_colour_r),
							PLATFORM_RENDERER_clamp_sdl_unit_to_byte(platform_renderer_current_colour_g),
							PLATFORM_RENDERER_clamp_sdl_unit_to_byte(platform_renderer_current_colour_b),
							PLATFORM_RENDERER_get_sdl_texture_alpha_mod(PLATFORM_RENDERER_clamp_sdl_unit_to_byte(platform_renderer_current_colour_a)));

					if (PLATFORM_RENDERER_sdl_copy_ex(platform_renderer_sdl_renderer, entry->sdl_texture, &src_rect, &dst_rect, 0.0, NULL, final_flip) == 0)
					{
						platform_renderer_sdl_native_draw_count++;
						platform_renderer_sdl_native_textured_draw_count++;
						PLATFORM_RENDERER_note_sdl_draw_source(PLATFORM_RENDERER_GEOM_SRC_MULTITEXTURE_ARRAY);
						PLATFORM_RENDERER_record_sdl_source_bounds_rect(PLATFORM_RENDERER_GEOM_SRC_MULTITEXTURE_ARRAY, &dst_rect);
					}
				}
				else
				{
					int indices[6] = {0, 1, 2, 0, 2, 3};
					(void)PLATFORM_RENDERER_try_sdl_geometry_textured(entry->sdl_texture, vertices, 4, indices, 6, PLATFORM_RENDERER_GEOM_SRC_MULTITEXTURE_ARRAY);
				}
			}
		}
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
	if (true && PLATFORM_RENDERER_is_sdl2_stub_ready() && (platform_renderer_present_height > 0))
	{
#if SDL_VERSION_ATLEAST(2, 0, 18)
		platform_renderer_texture_entry *entry = PLATFORM_RENDERER_get_texture_entry(platform_renderer_last_bound_texture_handle);
		if (entry != NULL)
		{
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
					vertices[i].position.y = (float)platform_renderer_present_height - ty;
					vertices[i].tex_coord.x = u[i];
					vertices[i].tex_coord.y = v[i];

					cr = (int)(r[i] * 255.0f);
					cg = (int)(g[i] * 255.0f);
					cb = (int)(b[i] * 255.0f);
					ca = (int)(a[i] * 255.0f);

					if (cr < 0)
						cr = 0;
					if (cr > 255)
						cr = 255;
					if (cg < 0)
						cg = 0;
					if (cg > 255)
						cg = 255;
					if (cb < 0)
						cb = 0;
					if (cb > 255)
						cb = 255;
					if (ca < 0)
						ca = 0;
					if (ca > 255)
						ca = 255;

					vertices[i].color.r = (Uint8)cr;
					vertices[i].color.g = (Uint8)cg;
					vertices[i].color.b = (Uint8)cb;
					vertices[i].color.a = (Uint8)ca;
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
				src_flip_y = PLATFORM_RENDERER_src_flip_y_for_sdl(v[0], v[1]);
				is_axis_aligned =
						(fabsf(txs[0] - txs[1]) < axis_eps) &&
						(fabsf(txs[2] - txs[3]) < axis_eps) &&
						(fabsf(tys[0] - tys[3]) < axis_eps) &&
						(fabsf(tys[1] - tys[2]) < axis_eps);

				{
					const int indices[6] = {0, 1, 2, 0, 2, 3};
					(void)is_axis_aligned;
					(void)src_flip_x;
					(void)src_flip_y;
					(void)PLATFORM_RENDERER_try_sdl_geometry_textured(entry->sdl_texture, vertices, 4, indices, 6, PLATFORM_RENDERER_GEOM_SRC_COLOURED_PERSPECTIVE);
					return;
				}

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

					dst_rect.x = (int)gl_left;
					dst_rect.y = (int)((float)platform_renderer_present_height - gl_top);
					dst_rect.w = (int)(gl_right - gl_left);
					dst_rect.h = (int)(gl_top - gl_bottom);
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
						final_flip = (SDL_RendererFlip)(final_flip | SDL_FLIP_HORIZONTAL);
					}
					if (src_flip_y)
					{
						final_flip = (SDL_RendererFlip)(final_flip | SDL_FLIP_VERTICAL);
					}

					PLATFORM_RENDERER_apply_sdl_texture_blend_mode(entry->sdl_texture);
					PLATFORM_RENDERER_set_texture_color_alpha_cached(
							entry->sdl_texture,
							PLATFORM_RENDERER_clamp_sdl_unit_to_byte(r[0]),
							PLATFORM_RENDERER_clamp_sdl_unit_to_byte(g[0]),
							PLATFORM_RENDERER_clamp_sdl_unit_to_byte(b[0]),
							PLATFORM_RENDERER_get_sdl_texture_alpha_mod(PLATFORM_RENDERER_clamp_sdl_unit_to_byte(a[0])));

					if (PLATFORM_RENDERER_sdl_copy_ex(platform_renderer_sdl_renderer, entry->sdl_texture, &src_rect, &dst_rect, 0.0, NULL, final_flip) == 0)
					{
						platform_renderer_sdl_native_draw_count++;
						platform_renderer_sdl_native_textured_draw_count++;
						PLATFORM_RENDERER_note_sdl_draw_source(PLATFORM_RENDERER_GEOM_SRC_COLOURED_ARRAY);
						PLATFORM_RENDERER_record_sdl_source_bounds_rect(PLATFORM_RENDERER_GEOM_SRC_COLOURED_ARRAY, &dst_rect);
					}
				}
				else
				{
					int indices[6] = {0, 1, 2, 0, 2, 3};
					(void)PLATFORM_RENDERER_try_sdl_geometry_textured(entry->sdl_texture, vertices, 4, indices, 6, PLATFORM_RENDERER_GEOM_SRC_COLOURED_ARRAY);
				}
			}
		}
	}
#endif
}

void PLATFORM_RENDERER_draw_bound_perspective_textured_quad(float x0, float y0, float x1, float y1, float x2, float y2, float x3, float y3, float u1, float v1, float u2, float v2, float q)
{
	if (true && PLATFORM_RENDERER_is_sdl2_stub_ready() && (platform_renderer_present_height > 0))
	{
#if SDL_VERSION_ATLEAST(2, 0, 18)
		platform_renderer_texture_entry *entry = PLATFORM_RENDERER_get_texture_entry(platform_renderer_last_bound_texture_handle);
		if (entry != NULL)
		{
			if (PLATFORM_RENDERER_build_sdl_texture_from_entry(entry))
			{
				const float perspective_eps = 0.0005f;
				if ((fabsf(q - 1.0f) > perspective_eps) && (q > perspective_eps))
				{
					bool strip_ok = true;
					const int strip_count = 24;
					SDL_Vertex strip_vertices[strip_count * 4];
					int strip_indices[strip_count * 6];
					int strip_index;
					const Uint8 cr = PLATFORM_RENDERER_clamp_sdl_unit_to_byte(platform_renderer_current_colour_r);
					const Uint8 cg = PLATFORM_RENDERER_clamp_sdl_unit_to_byte(platform_renderer_current_colour_g);
					const Uint8 cb = PLATFORM_RENDERER_clamp_sdl_unit_to_byte(platform_renderer_current_colour_b);
					const Uint8 ca = PLATFORM_RENDERER_clamp_sdl_unit_to_byte(platform_renderer_current_colour_a);

					for (strip_index = 0; strip_index < strip_count; strip_index++)
					{
						const int base_vertex = strip_index * 4;
						const int base_index = strip_index * 6;
						const float t0 = (float)strip_index / (float)strip_count;
						const float t1 = (float)(strip_index + 1) / (float)strip_count;
						const float one_minus_t0 = 1.0f - t0;
						const float one_minus_t1 = 1.0f - t1;
						const float left_x0 = (x0 * one_minus_t0) + (x1 * t0);
						const float left_y0 = (y0 * one_minus_t0) + (y1 * t0);
						const float left_x1 = (x0 * one_minus_t1) + (x1 * t1);
						const float left_y1 = (y0 * one_minus_t1) + (y1 * t1);
						const float right_x0 = (x3 * one_minus_t0) + (x2 * t0);
						const float right_y0 = (y3 * one_minus_t0) + (y2 * t0);
						const float right_x1 = (x3 * one_minus_t1) + (x2 * t1);
						const float right_y1 = (y3 * one_minus_t1) + (y2 * t1);
						const float w0 = (q * one_minus_t0) + t0;
						const float w1 = (q * one_minus_t1) + t1;
						float strip_v0;
						float strip_v1;
						float tx;
						float ty;

						if ((fabsf(w0) <= perspective_eps) || (fabsf(w1) <= perspective_eps))
						{
							strip_ok = false;
							break;
						}

						strip_v0 = ((v1 * q * one_minus_t0) + (v2 * t0)) / w0;
						strip_v1 = ((v1 * q * one_minus_t1) + (v2 * t1)) / w1;

						PLATFORM_RENDERER_transform_point(left_x0, left_y0, &tx, &ty);
						strip_vertices[base_vertex + 0].position.x = tx;
						strip_vertices[base_vertex + 0].position.y = (float)platform_renderer_present_height - ty;
						strip_vertices[base_vertex + 0].tex_coord.x = u1;
						strip_vertices[base_vertex + 0].tex_coord.y = 1.0f - strip_v0;
						strip_vertices[base_vertex + 0].color.r = cr;
						strip_vertices[base_vertex + 0].color.g = cg;
						strip_vertices[base_vertex + 0].color.b = cb;
						strip_vertices[base_vertex + 0].color.a = ca;

						PLATFORM_RENDERER_transform_point(left_x1, left_y1, &tx, &ty);
						strip_vertices[base_vertex + 1].position.x = tx;
						strip_vertices[base_vertex + 1].position.y = (float)platform_renderer_present_height - ty;
						strip_vertices[base_vertex + 1].tex_coord.x = u1;
						strip_vertices[base_vertex + 1].tex_coord.y = 1.0f - strip_v1;
						strip_vertices[base_vertex + 1].color.r = cr;
						strip_vertices[base_vertex + 1].color.g = cg;
						strip_vertices[base_vertex + 1].color.b = cb;
						strip_vertices[base_vertex + 1].color.a = ca;

						PLATFORM_RENDERER_transform_point(right_x1, right_y1, &tx, &ty);
						strip_vertices[base_vertex + 2].position.x = tx;
						strip_vertices[base_vertex + 2].position.y = (float)platform_renderer_present_height - ty;
						strip_vertices[base_vertex + 2].tex_coord.x = u2;
						strip_vertices[base_vertex + 2].tex_coord.y = 1.0f - strip_v1;
						strip_vertices[base_vertex + 2].color.r = cr;
						strip_vertices[base_vertex + 2].color.g = cg;
						strip_vertices[base_vertex + 2].color.b = cb;
						strip_vertices[base_vertex + 2].color.a = ca;

						PLATFORM_RENDERER_transform_point(right_x0, right_y0, &tx, &ty);
						strip_vertices[base_vertex + 3].position.x = tx;
						strip_vertices[base_vertex + 3].position.y = (float)platform_renderer_present_height - ty;
						strip_vertices[base_vertex + 3].tex_coord.x = u2;
						strip_vertices[base_vertex + 3].tex_coord.y = 1.0f - strip_v0;
						strip_vertices[base_vertex + 3].color.r = cr;
						strip_vertices[base_vertex + 3].color.g = cg;
						strip_vertices[base_vertex + 3].color.b = cb;
						strip_vertices[base_vertex + 3].color.a = ca;

						strip_indices[base_index + 0] = base_vertex + 0;
						strip_indices[base_index + 1] = base_vertex + 1;
						strip_indices[base_index + 2] = base_vertex + 2;
						strip_indices[base_index + 3] = base_vertex + 0;
						strip_indices[base_index + 4] = base_vertex + 2;
						strip_indices[base_index + 5] = base_vertex + 3;
					}

					if (strip_ok &&
							PLATFORM_RENDERER_try_sdl_geometry_textured(
									entry->sdl_texture,
									strip_vertices,
									strip_count * 4,
									strip_indices,
									strip_count * 6,
									PLATFORM_RENDERER_GEOM_SRC_PERSPECTIVE))
					{
						return;
					}
				}

				SDL_Vertex vertices[4];
				int i;
				const float px[4] = {x0, x1, x2, x3};
				const float py[4] = {y0, y1, y2, y3};
				const float tu[4] = {u1, u1, u2, u2};
				const float tv[4] = {v1, v2, v2, v1};
				float txs[4];
				float tys[4];
				const float axis_eps = 0.01f;
				bool is_axis_aligned;
				bool src_flip_x = (u2 < u1);
				bool src_flip_y = PLATFORM_RENDERER_src_flip_y_for_sdl(v1, v2);
				bool geom_flip_x = false;
				bool geom_flip_y = false;

				for (i = 0; i < 4; i++)
				{
					float tx;
					float ty;
					PLATFORM_RENDERER_transform_point(px[i], py[i], &tx, &ty);
					txs[i] = tx;
					tys[i] = ty;
					vertices[i].position.x = tx;
					vertices[i].position.y = (float)platform_renderer_present_height - ty;
					vertices[i].tex_coord.x = tu[i];
					vertices[i].tex_coord.y = tv[i];
					vertices[i].color.r = PLATFORM_RENDERER_clamp_sdl_unit_to_byte(platform_renderer_current_colour_r);
					vertices[i].color.g = PLATFORM_RENDERER_clamp_sdl_unit_to_byte(platform_renderer_current_colour_g);
					vertices[i].color.b = PLATFORM_RENDERER_clamp_sdl_unit_to_byte(platform_renderer_current_colour_b);
					vertices[i].color.a = PLATFORM_RENDERER_clamp_sdl_unit_to_byte(platform_renderer_current_colour_a);
				}

				/*
				 * Prefer true geometry for perspective quads even when they look
				 * axis-aligned. Rectangle approximations can introduce visible
				 * stretch during parallax/scrolling.
				 */
				{
					const int indices[6] = {0, 1, 2, 0, 2, 3};
					if (PLATFORM_RENDERER_try_sdl_geometry_textured(entry->sdl_texture, vertices, 4, indices, 6, PLATFORM_RENDERER_GEOM_SRC_PERSPECTIVE))
					{
						return;
					}
					if (platform_renderer_sdl_native_primary_strict_enabled)
					{
						return;
					}
				}

				is_axis_aligned =
						(fabsf(txs[0] - txs[1]) < axis_eps) &&
						(fabsf(txs[2] - txs[3]) < axis_eps) &&
						(fabsf(tys[0] - tys[3]) < axis_eps) &&
						(fabsf(tys[1] - tys[2]) < axis_eps);
				PLATFORM_RENDERER_axis_orientation_flips_from_transformed_quad(txs, tys, &geom_flip_x, &geom_flip_y);

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

					dst_rect.x = (int)gl_left;
					dst_rect.y = (int)((float)platform_renderer_present_height - gl_top);
					dst_rect.w = (int)(gl_right - gl_left);
					dst_rect.h = (int)(gl_top - gl_bottom);
					if (dst_rect.w <= 0)
					{
						dst_rect.w = 1;
					}
					if (dst_rect.h <= 0)
					{
						dst_rect.h = 1;
					}

					if (src_flip_x ^ geom_flip_x)
					{
						final_flip = (SDL_RendererFlip)(final_flip | SDL_FLIP_HORIZONTAL);
					}
					if (src_flip_y ^ geom_flip_y)
					{
						final_flip = (SDL_RendererFlip)(final_flip | SDL_FLIP_VERTICAL);
					}

					PLATFORM_RENDERER_apply_sdl_texture_blend_mode(entry->sdl_texture);
					PLATFORM_RENDERER_set_texture_color_alpha_cached(
							entry->sdl_texture,
							PLATFORM_RENDERER_clamp_sdl_unit_to_byte(platform_renderer_current_colour_r),
							PLATFORM_RENDERER_clamp_sdl_unit_to_byte(platform_renderer_current_colour_g),
							PLATFORM_RENDERER_clamp_sdl_unit_to_byte(platform_renderer_current_colour_b),
							PLATFORM_RENDERER_get_sdl_texture_alpha_mod(PLATFORM_RENDERER_clamp_sdl_unit_to_byte(platform_renderer_current_colour_a)));

					if (PLATFORM_RENDERER_sdl_copy_ex(platform_renderer_sdl_renderer, entry->sdl_texture, &src_rect, &dst_rect, 0.0, NULL, final_flip) == 0)
					{
						platform_renderer_sdl_native_draw_count++;
						platform_renderer_sdl_native_textured_draw_count++;
						PLATFORM_RENDERER_note_sdl_draw_source(PLATFORM_RENDERER_GEOM_SRC_PERSPECTIVE);
						PLATFORM_RENDERER_record_sdl_source_bounds_rect(PLATFORM_RENDERER_GEOM_SRC_PERSPECTIVE, &dst_rect);
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
						const double angle_degrees = atan2((double)exy, (double)exx) * (180.0 / 3.14159265358979323846);

						if (PLATFORM_RENDERER_build_safe_sdl_src_rect(entry, u1, v1, u2, v2, &src_rect))
						{
							dst_rect.w = (int)(width + 0.5f);
							dst_rect.h = (int)(height + 0.5f);
							if (dst_rect.w <= 0)
								dst_rect.w = 1;
							if (dst_rect.h <= 0)
								dst_rect.h = 1;
							dst_rect.x = (int)(cx - ((float)dst_rect.w * 0.5f));
							dst_rect.y = (int)(cy - ((float)dst_rect.h * 0.5f));

							center.x = dst_rect.w / 2;
							center.y = dst_rect.h / 2;

							if (src_flip_x)
							{
								final_flip = (SDL_RendererFlip)(final_flip | SDL_FLIP_HORIZONTAL);
							}
							if (src_flip_y)
							{
								final_flip = (SDL_RendererFlip)(final_flip | SDL_FLIP_VERTICAL);
							}

							PLATFORM_RENDERER_apply_sdl_texture_blend_mode(entry->sdl_texture);
							PLATFORM_RENDERER_set_texture_color_alpha_cached(
									entry->sdl_texture,
									PLATFORM_RENDERER_clamp_sdl_unit_to_byte(platform_renderer_current_colour_r),
									PLATFORM_RENDERER_clamp_sdl_unit_to_byte(platform_renderer_current_colour_g),
									PLATFORM_RENDERER_clamp_sdl_unit_to_byte(platform_renderer_current_colour_b),
									PLATFORM_RENDERER_get_sdl_texture_alpha_mod(PLATFORM_RENDERER_clamp_sdl_unit_to_byte(platform_renderer_current_colour_a)));

							if (SDL_RenderCopyEx(platform_renderer_sdl_renderer, entry->sdl_texture, &src_rect, &dst_rect, angle_degrees, &center, final_flip) == 0)
							{
								platform_renderer_sdl_native_draw_count++;
								platform_renderer_sdl_native_textured_draw_count++;
								PLATFORM_RENDERER_note_sdl_draw_source(PLATFORM_RENDERER_GEOM_SRC_PERSPECTIVE);
								PLATFORM_RENDERER_record_sdl_source_bounds_rect(PLATFORM_RENDERER_GEOM_SRC_PERSPECTIVE, &dst_rect);
								drawn_rotated_rect = true;
							}
						}
					}

					if (!drawn_rotated_rect)
					{
						const int indices[6] = {0, 1, 2, 0, 2, 3};
						(void)PLATFORM_RENDERER_try_sdl_geometry_textured(entry->sdl_texture, vertices, 4, indices, 6, PLATFORM_RENDERER_GEOM_SRC_PERSPECTIVE);
					}
				}
			}
		}
	}
#endif
	(void)q;
}

void PLATFORM_RENDERER_draw_bound_coloured_perspective_textured_quad(float x0, float y0, float x1, float y1, float x2, float y2, float x3, float y3, float u1, float v1, float u2, float v2, float q, const float *r, const float *g, const float *b, const float *a)
{
	if ((r == NULL) || (g == NULL) || (b == NULL) || (a == NULL))
	{
		return;
	}
	if (true && PLATFORM_RENDERER_is_sdl2_stub_ready() && (platform_renderer_present_height > 0))
	{
#if SDL_VERSION_ATLEAST(2, 0, 18)
		platform_renderer_texture_entry *entry = PLATFORM_RENDERER_get_texture_entry(platform_renderer_last_bound_texture_handle);
		if (entry != NULL)
		{
			if (PLATFORM_RENDERER_build_sdl_texture_from_entry(entry))
			{
				const float perspective_eps = 0.0005f;
				if ((fabsf(q - 1.0f) > perspective_eps) && (q > perspective_eps))
				{
					bool strip_ok = true;
					const int strip_count = 24;
					SDL_Vertex strip_vertices[strip_count * 4];
					int strip_indices[strip_count * 6];
					int strip_index;

					for (strip_index = 0; strip_index < strip_count; strip_index++)
					{
						const int base_vertex = strip_index * 4;
						const int base_index = strip_index * 6;
						const float t0 = (float)strip_index / (float)strip_count;
						const float t1 = (float)(strip_index + 1) / (float)strip_count;
						const float one_minus_t0 = 1.0f - t0;
						const float one_minus_t1 = 1.0f - t1;
						const float left_x0 = (x0 * one_minus_t0) + (x1 * t0);
						const float left_y0 = (y0 * one_minus_t0) + (y1 * t0);
						const float left_x1 = (x0 * one_minus_t1) + (x1 * t1);
						const float left_y1 = (y0 * one_minus_t1) + (y1 * t1);
						const float right_x0 = (x3 * one_minus_t0) + (x2 * t0);
						const float right_y0 = (y3 * one_minus_t0) + (y2 * t0);
						const float right_x1 = (x3 * one_minus_t1) + (x2 * t1);
						const float right_y1 = (y3 * one_minus_t1) + (y2 * t1);
						const float w0 = (q * one_minus_t0) + t0;
						const float w1 = (q * one_minus_t1) + t1;
						float strip_v0;
						float strip_v1;
						float tx;
						float ty;
						int cr;
						int cg;
						int cb;
						int ca;

						if ((fabsf(w0) <= perspective_eps) || (fabsf(w1) <= perspective_eps))
						{
							strip_ok = false;
							break;
						}

						strip_v0 = ((v1 * q * one_minus_t0) + (v2 * t0)) / w0;
						strip_v1 = ((v1 * q * one_minus_t1) + (v2 * t1)) / w1;

						PLATFORM_RENDERER_transform_point(left_x0, left_y0, &tx, &ty);
						strip_vertices[base_vertex + 0].position.x = tx;
						strip_vertices[base_vertex + 0].position.y = (float)platform_renderer_present_height - ty;
						strip_vertices[base_vertex + 0].tex_coord.x = u1;
						strip_vertices[base_vertex + 0].tex_coord.y = 1.0f - strip_v0;
						cr = (int)(((r[0] * one_minus_t0) + (r[1] * t0)) * 255.0f);
						cg = (int)(((g[0] * one_minus_t0) + (g[1] * t0)) * 255.0f);
						cb = (int)(((b[0] * one_minus_t0) + (b[1] * t0)) * 255.0f);
						ca = (int)(((a[0] * one_minus_t0) + (a[1] * t0)) * 255.0f);
						if (cr < 0)
							cr = 0;
						else if (cr > 255)
							cr = 255;
						if (cg < 0)
							cg = 0;
						else if (cg > 255)
							cg = 255;
						if (cb < 0)
							cb = 0;
						else if (cb > 255)
							cb = 255;
						if (ca < 0)
							ca = 0;
						else if (ca > 255)
							ca = 255;
						strip_vertices[base_vertex + 0].color.r = (Uint8)cr;
						strip_vertices[base_vertex + 0].color.g = (Uint8)cg;
						strip_vertices[base_vertex + 0].color.b = (Uint8)cb;
						strip_vertices[base_vertex + 0].color.a = (Uint8)ca;

						PLATFORM_RENDERER_transform_point(left_x1, left_y1, &tx, &ty);
						strip_vertices[base_vertex + 1].position.x = tx;
						strip_vertices[base_vertex + 1].position.y = (float)platform_renderer_present_height - ty;
						strip_vertices[base_vertex + 1].tex_coord.x = u1;
						strip_vertices[base_vertex + 1].tex_coord.y = 1.0f - strip_v1;
						cr = (int)(((r[0] * one_minus_t1) + (r[1] * t1)) * 255.0f);
						cg = (int)(((g[0] * one_minus_t1) + (g[1] * t1)) * 255.0f);
						cb = (int)(((b[0] * one_minus_t1) + (b[1] * t1)) * 255.0f);
						ca = (int)(((a[0] * one_minus_t1) + (a[1] * t1)) * 255.0f);
						if (cr < 0)
							cr = 0;
						else if (cr > 255)
							cr = 255;
						if (cg < 0)
							cg = 0;
						else if (cg > 255)
							cg = 255;
						if (cb < 0)
							cb = 0;
						else if (cb > 255)
							cb = 255;
						if (ca < 0)
							ca = 0;
						else if (ca > 255)
							ca = 255;
						strip_vertices[base_vertex + 1].color.r = (Uint8)cr;
						strip_vertices[base_vertex + 1].color.g = (Uint8)cg;
						strip_vertices[base_vertex + 1].color.b = (Uint8)cb;
						strip_vertices[base_vertex + 1].color.a = (Uint8)ca;

						PLATFORM_RENDERER_transform_point(right_x1, right_y1, &tx, &ty);
						strip_vertices[base_vertex + 2].position.x = tx;
						strip_vertices[base_vertex + 2].position.y = (float)platform_renderer_present_height - ty;
						strip_vertices[base_vertex + 2].tex_coord.x = u2;
						strip_vertices[base_vertex + 2].tex_coord.y = 1.0f - strip_v1;
						cr = (int)(((r[3] * one_minus_t1) + (r[2] * t1)) * 255.0f);
						cg = (int)(((g[3] * one_minus_t1) + (g[2] * t1)) * 255.0f);
						cb = (int)(((b[3] * one_minus_t1) + (b[2] * t1)) * 255.0f);
						ca = (int)(((a[3] * one_minus_t1) + (a[2] * t1)) * 255.0f);
						if (cr < 0)
							cr = 0;
						else if (cr > 255)
							cr = 255;
						if (cg < 0)
							cg = 0;
						else if (cg > 255)
							cg = 255;
						if (cb < 0)
							cb = 0;
						else if (cb > 255)
							cb = 255;
						if (ca < 0)
							ca = 0;
						else if (ca > 255)
							ca = 255;
						strip_vertices[base_vertex + 2].color.r = (Uint8)cr;
						strip_vertices[base_vertex + 2].color.g = (Uint8)cg;
						strip_vertices[base_vertex + 2].color.b = (Uint8)cb;
						strip_vertices[base_vertex + 2].color.a = (Uint8)ca;

						PLATFORM_RENDERER_transform_point(right_x0, right_y0, &tx, &ty);
						strip_vertices[base_vertex + 3].position.x = tx;
						strip_vertices[base_vertex + 3].position.y = (float)platform_renderer_present_height - ty;
						strip_vertices[base_vertex + 3].tex_coord.x = u2;
						strip_vertices[base_vertex + 3].tex_coord.y = 1.0f - strip_v0;
						cr = (int)(((r[3] * one_minus_t0) + (r[2] * t0)) * 255.0f);
						cg = (int)(((g[3] * one_minus_t0) + (g[2] * t0)) * 255.0f);
						cb = (int)(((b[3] * one_minus_t0) + (b[2] * t0)) * 255.0f);
						ca = (int)(((a[3] * one_minus_t0) + (a[2] * t0)) * 255.0f);
						if (cr < 0)
							cr = 0;
						else if (cr > 255)
							cr = 255;
						if (cg < 0)
							cg = 0;
						else if (cg > 255)
							cg = 255;
						if (cb < 0)
							cb = 0;
						else if (cb > 255)
							cb = 255;
						if (ca < 0)
							ca = 0;
						else if (ca > 255)
							ca = 255;
						strip_vertices[base_vertex + 3].color.r = (Uint8)cr;
						strip_vertices[base_vertex + 3].color.g = (Uint8)cg;
						strip_vertices[base_vertex + 3].color.b = (Uint8)cb;
						strip_vertices[base_vertex + 3].color.a = (Uint8)ca;

						strip_indices[base_index + 0] = base_vertex + 0;
						strip_indices[base_index + 1] = base_vertex + 1;
						strip_indices[base_index + 2] = base_vertex + 2;
						strip_indices[base_index + 3] = base_vertex + 0;
						strip_indices[base_index + 4] = base_vertex + 2;
						strip_indices[base_index + 5] = base_vertex + 3;
					}

					if (strip_ok &&
							PLATFORM_RENDERER_try_sdl_geometry_textured(
									entry->sdl_texture,
									strip_vertices,
									strip_count * 4,
									strip_indices,
									strip_count * 6,
									PLATFORM_RENDERER_GEOM_SRC_COLOURED_PERSPECTIVE))
					{
						return;
					}
				}

				SDL_Vertex vertices[4];
				int i;
				const float px[4] = {x0, x1, x2, x3};
				const float py[4] = {y0, y1, y2, y3};
				const float tu[4] = {u1, u1, u2, u2};
				const float tv[4] = {v1, v2, v2, v1};
				float txs[4];
				float tys[4];
				bool uniform_colour = true;
				const float axis_eps = 0.01f;
				const float colour_eps = 0.01f;
				bool is_axis_aligned;
				bool src_flip_x = (u2 < u1);
				bool src_flip_y = PLATFORM_RENDERER_src_flip_y_for_sdl(v1, v2);
				bool geom_flip_x = false;
				bool geom_flip_y = false;

				for (i = 0; i < 4; i++)
				{
					float tx;
					float ty;
					int cr = (int)(r[i] * 255.0f);
					int cg = (int)(g[i] * 255.0f);
					int cb = (int)(b[i] * 255.0f);
					int ca = (int)(a[i] * 255.0f);

					if (cr < 0)
						cr = 0;
					if (cr > 255)
						cr = 255;
					if (cg < 0)
						cg = 0;
					if (cg > 255)
						cg = 255;
					if (cb < 0)
						cb = 0;
					if (cb > 255)
						cb = 255;
					if (ca < 0)
						ca = 0;
					if (ca > 255)
						ca = 255;

					PLATFORM_RENDERER_transform_point(px[i], py[i], &tx, &ty);
					txs[i] = tx;
					tys[i] = ty;
					vertices[i].position.x = tx;
					vertices[i].position.y = (float)platform_renderer_present_height - ty;
					vertices[i].tex_coord.x = tu[i];
					vertices[i].tex_coord.y = tv[i];
					vertices[i].color.r = (Uint8)cr;
					vertices[i].color.g = (Uint8)cg;
					vertices[i].color.b = (Uint8)cb;
					vertices[i].color.a = (Uint8)ca;
				}

				/*
				 * Prefer true geometry for perspective+colour quads even when the
				 * quad is axis-aligned. This avoids copy/rect approximation stretch
				 * artifacts during background scrolling.
				 */
				{
					const int indices[6] = {0, 1, 2, 0, 2, 3};
					if (PLATFORM_RENDERER_try_sdl_geometry_textured(entry->sdl_texture, vertices, 4, indices, 6, PLATFORM_RENDERER_GEOM_SRC_COLOURED_PERSPECTIVE))
					{
						return;
					}
					if (platform_renderer_sdl_native_primary_strict_enabled)
					{
						return;
					}
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
				PLATFORM_RENDERER_axis_orientation_flips_from_transformed_quad(txs, tys, &geom_flip_x, &geom_flip_y);

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

					dst_rect.x = (int)gl_left;
					dst_rect.y = (int)((float)platform_renderer_present_height - gl_top);
					dst_rect.w = (int)(gl_right - gl_left);
					dst_rect.h = (int)(gl_top - gl_bottom);
					if (dst_rect.w <= 0)
					{
						dst_rect.w = 1;
					}
					if (dst_rect.h <= 0)
					{
						dst_rect.h = 1;
					}

					if (src_flip_x ^ geom_flip_x)
					{
						final_flip = (SDL_RendererFlip)(final_flip | SDL_FLIP_HORIZONTAL);
					}
					if (src_flip_y ^ geom_flip_y)
					{
						final_flip = (SDL_RendererFlip)(final_flip | SDL_FLIP_VERTICAL);
					}

					PLATFORM_RENDERER_apply_sdl_texture_blend_mode(entry->sdl_texture);
					PLATFORM_RENDERER_set_texture_color_alpha_cached(
							entry->sdl_texture,
							PLATFORM_RENDERER_clamp_sdl_unit_to_byte(r[0]),
							PLATFORM_RENDERER_clamp_sdl_unit_to_byte(g[0]),
							PLATFORM_RENDERER_clamp_sdl_unit_to_byte(b[0]),
							PLATFORM_RENDERER_get_sdl_texture_alpha_mod(PLATFORM_RENDERER_clamp_sdl_unit_to_byte(a[0])));

					if (PLATFORM_RENDERER_sdl_copy_ex(platform_renderer_sdl_renderer, entry->sdl_texture, &src_rect, &dst_rect, 0.0, NULL, final_flip) == 0)
					{
						platform_renderer_sdl_native_draw_count++;
						platform_renderer_sdl_native_textured_draw_count++;
						PLATFORM_RENDERER_note_sdl_draw_source(PLATFORM_RENDERER_GEOM_SRC_COLOURED_PERSPECTIVE);
						PLATFORM_RENDERER_record_sdl_source_bounds_rect(PLATFORM_RENDERER_GEOM_SRC_COLOURED_PERSPECTIVE, &dst_rect);
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
						const double angle_degrees = atan2((double)exy, (double)exx) * (180.0 / 3.14159265358979323846);
						const float avg_r = (r[0] + r[1] + r[2] + r[3]) * 0.25f;
						const float avg_g = (g[0] + g[1] + g[2] + g[3]) * 0.25f;
						const float avg_b = (b[0] + b[1] + b[2] + b[3]) * 0.25f;
						const float avg_a = (a[0] + a[1] + a[2] + a[3]) * 0.25f;

						if (PLATFORM_RENDERER_build_safe_sdl_src_rect(entry, u1, v1, u2, v2, &src_rect))
						{
							dst_rect.w = (int)(width + 0.5f);
							dst_rect.h = (int)(height + 0.5f);
							if (dst_rect.w <= 0)
								dst_rect.w = 1;
							if (dst_rect.h <= 0)
								dst_rect.h = 1;
							dst_rect.x = (int)(cx - ((float)dst_rect.w * 0.5f));
							dst_rect.y = (int)(cy - ((float)dst_rect.h * 0.5f));

							center.x = dst_rect.w / 2;
							center.y = dst_rect.h / 2;

							if (src_flip_x)
							{
								final_flip = (SDL_RendererFlip)(final_flip | SDL_FLIP_HORIZONTAL);
							}
							if (src_flip_y)
							{
								final_flip = (SDL_RendererFlip)(final_flip | SDL_FLIP_VERTICAL);
							}

							PLATFORM_RENDERER_apply_sdl_texture_blend_mode(entry->sdl_texture);
							PLATFORM_RENDERER_set_texture_color_alpha_cached(
									entry->sdl_texture,
									PLATFORM_RENDERER_clamp_sdl_unit_to_byte(avg_r),
									PLATFORM_RENDERER_clamp_sdl_unit_to_byte(avg_g),
									PLATFORM_RENDERER_clamp_sdl_unit_to_byte(avg_b),
									PLATFORM_RENDERER_get_sdl_texture_alpha_mod(PLATFORM_RENDERER_clamp_sdl_unit_to_byte(avg_a)));

							if (SDL_RenderCopyEx(platform_renderer_sdl_renderer, entry->sdl_texture, &src_rect, &dst_rect, angle_degrees, &center, final_flip) == 0)
							{
								platform_renderer_sdl_native_draw_count++;
								platform_renderer_sdl_native_textured_draw_count++;
								PLATFORM_RENDERER_note_sdl_draw_source(PLATFORM_RENDERER_GEOM_SRC_COLOURED_PERSPECTIVE);
								PLATFORM_RENDERER_record_sdl_source_bounds_rect(PLATFORM_RENDERER_GEOM_SRC_COLOURED_PERSPECTIVE, &dst_rect);
								drawn_rotated_rect = true;
							}
						}
					}

					if (!drawn_rotated_rect)
					{
						const int indices[6] = {0, 1, 2, 0, 2, 3};
						(void)PLATFORM_RENDERER_try_sdl_geometry_textured(entry->sdl_texture, vertices, 4, indices, 6, PLATFORM_RENDERER_GEOM_SRC_COLOURED_PERSPECTIVE);
					}
				}
			}
		}
	}
#endif
	(void)q;
}

void PLATFORM_RENDERER_draw_textured_quad(unsigned int texture_handle, int r, int g, int b, float screen_x, float screen_y, int virtual_screen_height, float left, float right, float up, float down, float u1, float v1, float u2, float v2, bool alpha_test)
{
	static unsigned int sdl_quad_colour_diag_counter = 0;
	if (PLATFORM_RENDERER_is_sdl2_stub_ready())
	{
		platform_renderer_texture_entry *entry = PLATFORM_RENDERER_get_texture_entry(texture_handle);
		if (entry != NULL)
		{
			if (PLATFORM_RENDERER_build_sdl_texture_from_entry(entry))
			{
				SDL_Texture *draw_texture = PLATFORM_RENDERER_get_effective_texture_for_current_blend(entry);
				if (draw_texture == NULL)
				{
					return;
				}
				float x0 = screen_x + left;
				float x1 = screen_x + right;
				float y0 = screen_y + up;
				float y1 = screen_y + down;
				float tx0 = x0;
				float tx1 = x1;
				float ty0 = y0;
				float ty1 = y1;
				float sdl_y0;
				float sdl_y1;
				float dst_left;
				float dst_right;
				float dst_top;
				float dst_bottom;
				SDL_Rect src_rect;
				SDL_Rect dst_rect;
				int sdl_height = platform_renderer_present_height;

				if (sdl_height <= 0)
				{
					sdl_height = virtual_screen_height;
				}
				/*
				 * Match legacy GL behavior for this path: draw_textured_quad()
				 * issues glLoadIdentity()+glTranslatef(screen_x, screen_y, ...),
				 * so it should not inherit the global transform accumulator.
				 * Use identity-space coordinates directly for SDL too.
				 */
				tx0 = x0;
				ty0 = y0;
				tx1 = x1;
				ty1 = y1;
				sdl_y0 = (float)sdl_height - ty0;
				sdl_y1 = (float)sdl_height - ty1;
				dst_left = (tx0 < tx1) ? tx0 : tx1;
				dst_right = (tx0 > tx1) ? tx0 : tx1;
				dst_top = (sdl_y0 < sdl_y1) ? sdl_y0 : sdl_y1;
				dst_bottom = (sdl_y0 > sdl_y1) ? sdl_y0 : sdl_y1;

				if (!PLATFORM_RENDERER_build_safe_sdl_src_rect(entry, u1, v1, u2, v2, &src_rect))
				{
				}
				else
				{
					dst_rect.x = (int)dst_left;
					dst_rect.y = (int)dst_top;
					dst_rect.w = (int)(dst_right - dst_left);
					dst_rect.h = (int)(dst_bottom - dst_top);

					if (dst_rect.w <= 0)
					{
						dst_rect.w = 1;
					}
					if (dst_rect.h <= 0)
					{
						dst_rect.h = 1;
					}

					(void)alpha_test;
					sdl_quad_colour_diag_counter++;
					if ((sdl_quad_colour_diag_counter <= 120u) &&
							((r < 8) || (g < 8) || (b < 8)))
					{
					}
					PLATFORM_RENDERER_apply_sdl_texture_blend_mode(draw_texture);
					PLATFORM_RENDERER_set_texture_color_alpha_cached(draw_texture,
							PLATFORM_RENDERER_clamp_sdl_colour_mod(r),
							PLATFORM_RENDERER_clamp_sdl_colour_mod(g),
							PLATFORM_RENDERER_clamp_sdl_colour_mod(b),
							PLATFORM_RENDERER_get_sdl_texture_alpha_mod(255));
					if (SDL_RenderCopy(platform_renderer_sdl_renderer, draw_texture, &src_rect, &dst_rect) == 0)
					{
						platform_renderer_sdl_native_draw_count++;
						platform_renderer_sdl_native_textured_draw_count++;
						{
							static int quad_log = 0;
							if (quad_log < 10)
							{
								quad_log++;
								fprintf(stderr,
									"[QUAD-DRAW %d] handle=%u dst=(%d,%d %dx%d) "
									"src=(%d,%d %dx%d) sdl_h=%d\n",
									quad_log, texture_handle,
									dst_rect.x, dst_rect.y, dst_rect.w, dst_rect.h,
									src_rect.x, src_rect.y, src_rect.w, src_rect.h,
									sdl_height);
							}
						}
					}
					else
					{
						static int quad_fail_log = 0;
						if (quad_fail_log < 3)
						{
							quad_fail_log++;
							fprintf(stderr,
								"[QUAD-FAIL %d] handle=%u SDL error: %s "
								"dst=(%d,%d %dx%d)\n",
								quad_fail_log, texture_handle, SDL_GetError(),
								dst_rect.x, dst_rect.y, dst_rect.w, dst_rect.h);
						}
					}
				}
			}
		}
	}
	(void)virtual_screen_height;
}

void PLATFORM_RENDERER_draw_sdl_window_sprite(unsigned int texture_handle, int r, int g, int b, int a, float entity_x, float entity_y, float left, float right, float up, float down, float u1, float v1, float u2, float v2, float left_window_transform_x, float top_window_transform_y, float total_scale_x, float total_scale_y, float sprite_scale_x, float sprite_scale_y, float sprite_rotation_degrees, bool sprite_flip_x, bool sprite_flip_y)
{
	if (!(PLATFORM_RENDERER_is_sdl2_stub_ready()))
	{
		return;
	}

	platform_renderer_texture_entry *entry = PLATFORM_RENDERER_get_texture_entry(texture_handle);
	if (entry == NULL)
	{
		return;
	}
	if (!PLATFORM_RENDERER_build_sdl_texture_from_entry(entry))
	{
		return;
	}
	SDL_Texture *draw_texture = PLATFORM_RENDERER_get_effective_texture_for_current_blend(entry);
	if (draw_texture == NULL)
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
	bool src_flip_y = PLATFORM_RENDERER_src_flip_y_for_sdl(v1, v2);

	SDL_Rect src_rect;
	if (!PLATFORM_RENDERER_build_safe_sdl_src_rect(entry, u1, v1, u2, v2, &src_rect))
	{
		return;
	}

	SDL_Rect dst_rect;
	dst_rect.x = (int)dst_left;
	dst_rect.y = (int)((float)platform_renderer_present_height - gl_top);
	dst_rect.w = (int)(dst_right - dst_left);
	dst_rect.h = (int)(gl_top - gl_bottom);

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
	float origin_y_sdl = (float)platform_renderer_present_height - origin_y_gl;

	SDL_Point center;
	center.x = (int)(origin_x_gl - (float)dst_rect.x);
	center.y = (int)(origin_y_sdl - (float)dst_rect.y);

	SDL_RendererFlip final_flip = SDL_FLIP_NONE;
	if (src_flip_x ^ sprite_flip_x)
	{
		final_flip = (SDL_RendererFlip)(final_flip | SDL_FLIP_HORIZONTAL);
	}
	if (src_flip_y ^ sprite_flip_y)
	{
		final_flip = (SDL_RendererFlip)(final_flip | SDL_FLIP_VERTICAL);
	}

	{
		/*
		 * On accelerated renderers, keep normal sprites on the geometry path so
		 * they participate in the same SDL batching rules as the rest of the
		 * frame. Software renderers still prefer RenderCopyEx for stability.
		 */
		SDL_Vertex vertices[4];
		int indices[6] = {0, 1, 2, 0, 2, 3};
		const bool effective_flip_x = src_flip_x ^ sprite_flip_x;
		const bool effective_flip_y = src_flip_y ^ sprite_flip_y;
		const float rel_left = transformed_left * total_scale_x;
		const float rel_right = transformed_right * total_scale_x;
		const float rel_up = transformed_up * total_scale_y;
		const float rel_down = transformed_down * total_scale_y;
		const float u_left = effective_flip_x ? u2 : u1;
		const float u_right = effective_flip_x ? u1 : u2;
		const float v_top = effective_flip_y ? v2 : v1;
		const float v_bottom = effective_flip_y ? v1 : v2;
		const float radians = sprite_rotation_degrees * (3.14159265358979323846f / 180.0f);
		const float c = cosf(radians);
		const float s = sinf(radians);
		const float local_x[4] = {rel_left, rel_left, rel_right, rel_right};
		const float local_y[4] = {rel_up, rel_down, rel_down, rel_up};
		const float uv_x[4] = {u_left, u_left, u_right, u_right};
		const float uv_y[4] = {v_top, v_bottom, v_bottom, v_top};
		int i;
		int native_draw_count_before = platform_renderer_sdl_native_draw_count;
		bool copied = false;
		const bool prefer_geometry = !platform_renderer_sdl_renderer_software_forced;

		for (i = 0; i < 4; i++)
		{
			const float x = local_x[i];
			const float y = local_y[i];
			const float rx = (x * c) - (y * s);
			const float ry = (x * s) + (y * c);
			vertices[i].position.x = origin_x_gl + rx;
			vertices[i].position.y = (float)platform_renderer_present_height - (origin_y_gl + ry);
			vertices[i].tex_coord.x = uv_x[i];
			vertices[i].tex_coord.y = uv_y[i];
			if (PLATFORM_RENDERER_using_subtractive_mod_fallback())
			{
				vertices[i].color.r = 255;
				vertices[i].color.g = 255;
				vertices[i].color.b = 255;
			}
			else
			{
				vertices[i].color.r = PLATFORM_RENDERER_clamp_sdl_colour_mod(r);
				vertices[i].color.g = PLATFORM_RENDERER_clamp_sdl_colour_mod(g);
				vertices[i].color.b = PLATFORM_RENDERER_clamp_sdl_colour_mod(b);
			}
			vertices[i].color.a = PLATFORM_RENDERER_clamp_sdl_colour_mod(a);
		}

		if (prefer_geometry)
		{
			PLATFORM_RENDERER_set_texture_color_alpha_cached(draw_texture, 255, 255, 255,
					PLATFORM_RENDERER_get_sdl_texture_alpha_mod(PLATFORM_RENDERER_clamp_sdl_colour_mod(a)));
			copied = PLATFORM_RENDERER_try_sdl_geometry_textured(
					draw_texture, vertices, 4, indices, 6, PLATFORM_RENDERER_GEOM_SRC_SDL_CUSTOM);
			{
				static int ws_geom_log = 0;
				if (copied && (ws_geom_log < 30))
				{
					ws_geom_log++;
					fprintf(stderr,
						"[WIN-SPRITE-GEOM %d] handle=%u entity=(%.0f,%.0f) "
						"dst=(%d,%d %dx%d) src=(%d,%d %dx%d) rot=%.1f\n",
						ws_geom_log, texture_handle,
						entity_x, entity_y,
						dst_rect.x, dst_rect.y, dst_rect.w, dst_rect.h,
						src_rect.x, src_rect.y, src_rect.w, src_rect.h,
						sprite_rotation_degrees);
				}
			}
		}

		if (!copied)
		{
			if (PLATFORM_RENDERER_using_subtractive_mod_fallback())
			{
				PLATFORM_RENDERER_set_texture_color_alpha_cached(draw_texture, 255, 255, 255,
						PLATFORM_RENDERER_get_sdl_texture_alpha_mod(PLATFORM_RENDERER_clamp_sdl_colour_mod(a)));
			}
			else
			{
				PLATFORM_RENDERER_set_texture_color_alpha_cached(draw_texture,
						PLATFORM_RENDERER_clamp_sdl_colour_mod(r),
						PLATFORM_RENDERER_clamp_sdl_colour_mod(g),
						PLATFORM_RENDERER_clamp_sdl_colour_mod(b),
						PLATFORM_RENDERER_get_sdl_texture_alpha_mod(PLATFORM_RENDERER_clamp_sdl_colour_mod(a)));
			}
			PLATFORM_RENDERER_apply_sdl_texture_blend_mode(draw_texture);
			if (SDL_RenderCopyEx(platform_renderer_sdl_renderer, draw_texture, &src_rect, &dst_rect, sprite_rotation_degrees, &center, final_flip) == 0)
			{
				platform_renderer_sdl_native_draw_count++;
				platform_renderer_sdl_native_textured_draw_count++;
				copied = true;
			}
			else
			{
				static int ws_fail_log = 0;
				if (ws_fail_log < 5)
				{
					ws_fail_log++;
					fprintf(stderr,
						"[WIN-SPRITE-FAIL %d] handle=%u entity=(%.0f,%.0f) "
						"dst=(%d,%d %dx%d) src=(%d,%d %dx%d) err: %s\n",
						ws_fail_log, texture_handle,
						entity_x, entity_y,
						dst_rect.x, dst_rect.y, dst_rect.w, dst_rect.h,
						src_rect.x, src_rect.y, src_rect.w, src_rect.h,
						SDL_GetError());
				}
			}
		}
		if (platform_renderer_sdl_native_draw_count > native_draw_count_before)
		{
			platform_renderer_sdl_window_sprite_draw_count++;
		}
		else if (platform_renderer_sdl_diag_verbose_enabled)
		{
			static unsigned int sdl_window_sprite_miss_counter = 0;
			sdl_window_sprite_miss_counter++;
		}
	}
}

void PLATFORM_RENDERER_draw_sdl_window_solid_rect(int r, int g, int b, float entity_x, float entity_y, float left, float right, float up, float down, float left_window_transform_x, float top_window_transform_y, float total_scale_x, float total_scale_y, float rect_scale_x, float rect_scale_y)
{
	if (!(true && PLATFORM_RENDERER_is_sdl2_stub_ready()))
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
	dst_rect.x = (int)dst_left;
	dst_rect.y = (int)((float)platform_renderer_present_height - gl_top);
	dst_rect.w = (int)(dst_right - dst_left);
	dst_rect.h = (int)(gl_top - gl_bottom);

	if (dst_rect.w <= 0)
	{
		dst_rect.w = 1;
	}
	if (dst_rect.h <= 0)
	{
		dst_rect.h = 1;
	}

	(void)SDL_SetRenderDrawBlendMode(platform_renderer_sdl_renderer, SDL_BLENDMODE_BLEND);
	(void)SDL_SetRenderDrawColor(
			platform_renderer_sdl_renderer,
			PLATFORM_RENDERER_clamp_sdl_colour_mod(r),
			PLATFORM_RENDERER_clamp_sdl_colour_mod(g),
			PLATFORM_RENDERER_clamp_sdl_colour_mod(b),
			255);
	if (SDL_RenderFillRect(platform_renderer_sdl_renderer, &dst_rect) == 0)
	{
		platform_renderer_sdl_native_draw_count++;
		platform_renderer_sdl_window_solid_rect_draw_count++;
	}
}

void PLATFORM_RENDERER_draw_sdl_bound_textured_quad_custom(unsigned int texture_handle, float x0, float y0, float x1, float y1, float x2, float y2, float x3, float y3, float u1, float v1, float u2, float v2)
{
	if (!(true && PLATFORM_RENDERER_is_sdl2_stub_ready()))
	{
		return;
	}

	if (platform_renderer_present_height <= 0)
	{
		return;
	}

	platform_renderer_texture_entry *entry = PLATFORM_RENDERER_get_texture_entry(texture_handle);
	if (entry == NULL)
	{
		return;
	}
	if (!PLATFORM_RENDERER_build_sdl_texture_from_entry(entry))
	{
		return;
	}
	int native_draw_count_before = platform_renderer_sdl_native_draw_count;

#if SDL_VERSION_ATLEAST(2, 0, 18)
	float px[4];
	float py[4];
	PLATFORM_RENDERER_transform_point(x0, y0, &px[0], &py[0]);
	PLATFORM_RENDERER_transform_point(x1, y1, &px[1], &py[1]);
	PLATFORM_RENDERER_transform_point(x2, y2, &px[2], &py[2]);
	PLATFORM_RENDERER_transform_point(x3, y3, &px[3], &py[3]);

	SDL_Vertex vertices[4];
	int i;
	for (i = 0; i < 4; i++)
	{
		vertices[i].position.x = px[i];
		vertices[i].position.y = (float)platform_renderer_present_height - py[i];
		vertices[i].color.r = PLATFORM_RENDERER_clamp_sdl_unit_to_byte(platform_renderer_current_colour_r);
		vertices[i].color.g = PLATFORM_RENDERER_clamp_sdl_unit_to_byte(platform_renderer_current_colour_g);
		vertices[i].color.b = PLATFORM_RENDERER_clamp_sdl_unit_to_byte(platform_renderer_current_colour_b);
		vertices[i].color.a = PLATFORM_RENDERER_clamp_sdl_unit_to_byte(platform_renderer_current_colour_a);
	}

	vertices[0].tex_coord.x = u1;
	vertices[0].tex_coord.y = v1;
	vertices[1].tex_coord.x = u1;
	vertices[1].tex_coord.y = v2;
	vertices[2].tex_coord.x = u2;
	vertices[2].tex_coord.y = v2;
	vertices[3].tex_coord.x = u2;
	vertices[3].tex_coord.y = v1;
	{
		int indices[6] = {0, 1, 2, 0, 2, 3};
		(void)PLATFORM_RENDERER_try_sdl_geometry_textured(entry->sdl_texture, vertices, 4, indices, 6, PLATFORM_RENDERER_GEOM_SRC_SDL_CUSTOM);
	}
	if (platform_renderer_sdl_native_draw_count > native_draw_count_before)
	{
		platform_renderer_sdl_bound_custom_draw_count++;
	}

#else
					(void)texture_handle;
					(void)x0;
					(void)y0;
					(void)x1;
					(void)y1;
					(void)x2;
					(void)y2;
					(void)x3;
					(void)y3;
					(void)u1;
					(void)v1;
					(void)u2;
					(void)v2;
#endif
}

unsigned int PLATFORM_RENDERER_create_masked_texture(SDL_Surface *image)
{
	return PLATFORM_RENDERER_register_SDL_texture(0, image);
}

void PLATFORM_RENDERER_present_frame(int width, int height)
{
	bool can_submit_legacy = false;
	bool should_flip_legacy_output = can_submit_legacy;
	static unsigned int sdl_present_debug_counter = 0;
	PLATFORM_RENDERER_refresh_sdl_stub_env_flags();
	const int native_primary_min_draws_for_no_mirror = platform_renderer_sdl_no_mirror_min_draws;
	const int native_primary_min_textured_draws_for_no_mirror = platform_renderer_sdl_no_mirror_min_textured;
	const int native_primary_required_streak_frames = platform_renderer_sdl_no_mirror_required_streak;
	platform_renderer_present_width = width;
	platform_renderer_present_height = height;
	if (PLATFORM_RENDERER_is_sdl2_stub_ready())
	{
		(void)PLATFORM_RENDERER_sync_sdl_render_target_size(width, height);
	}
	bool native_primary_strict = platform_renderer_sdl_native_primary_strict_enabled;
	bool native_primary_requested = true;
	bool native_primary = native_primary_requested && native_primary_strict;
	bool native_primary_force_no_mirror = platform_renderer_sdl_native_primary_force_no_mirror_enabled;
	bool coverage_good_this_frame =
			(platform_renderer_sdl_native_textured_draw_count > 0) &&
			(platform_renderer_sdl_geometry_fallback_miss_count == 0) &&
			((platform_renderer_sdl_geometry_degraded_count == 0));
	bool allow_no_mirror_this_frame = false;
	bool used_mirror_fallback = false;
	int geom_hot_source = PLATFORM_RENDERER_GEOM_SRC_NONE;
	int geom_hot_count = 0;
	int sdl_hot_source = PLATFORM_RENDERER_GEOM_SRC_NONE;
	int sdl_hot_count = 0;
	int legacy_hot_source = PLATFORM_RENDERER_GEOM_SRC_NONE;
	int legacy_hot_count = 0;
	int geom_i;
	if (!can_submit_legacy)
	{
		static int no_legacy_present_log_counter = 0;
		native_primary_requested = true;
		native_primary = true;
		native_primary_strict = true;
		native_primary_force_no_mirror = true;
		no_legacy_present_log_counter++;
	}
	if (!platform_renderer_sdl_mode_info_logged)
	{

		platform_renderer_sdl_mode_info_logged = true;
	}

	for (geom_i = 1; geom_i < PLATFORM_RENDERER_GEOM_SRC_COUNT; geom_i++)
	{
		int combined = platform_renderer_sdl_geometry_miss_sources[geom_i] + platform_renderer_sdl_geometry_degraded_sources[geom_i];
		if (combined > geom_hot_count)
		{
			geom_hot_count = combined;
			geom_hot_source = geom_i;
		}
		if (platform_renderer_sdl_native_draw_sources[geom_i] > sdl_hot_count)
		{
			sdl_hot_count = platform_renderer_sdl_native_draw_sources[geom_i];
			sdl_hot_source = geom_i;
		}
		if (platform_renderer_legacy_draw_sources[geom_i] > legacy_hot_count)
		{
			legacy_hot_count = platform_renderer_legacy_draw_sources[geom_i];
			legacy_hot_source = geom_i;
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
	if (!platform_renderer_legacy_submission_mode_forced_on &&
			!platform_renderer_legacy_submission_mode_forced_off &&
			!platform_renderer_legacy_safe_off_latched &&
			can_submit_legacy &&
			native_primary &&
			native_primary_strict &&
			native_primary_force_no_mirror &&
			PLATFORM_RENDERER_is_sdl2_stub_ready() &&
			coverage_good_this_frame &&
			(platform_renderer_sdl_native_draw_count >= native_primary_min_draws_for_no_mirror) &&
			(platform_renderer_sdl_native_textured_draw_count >= native_primary_min_textured_draws_for_no_mirror) &&
			(platform_renderer_sdl_native_coverage_streak >= platform_renderer_legacy_safe_off_required_streak))
	{
		platform_renderer_legacy_submission_mode_enabled = false;
		can_submit_legacy = false;
		platform_renderer_legacy_safe_off_latched = true;
		platform_renderer_sdl_mode_info_logged = false;
	}
	if (!platform_renderer_legacy_submission_mode_forced_on &&
			!platform_renderer_legacy_submission_mode_forced_off &&
			!platform_renderer_legacy_safe_off_latched)
	{
		platform_renderer_legacy_safe_off_progress_log_counter++;
		if ((platform_renderer_legacy_safe_off_progress_log_counter <= 5u) ||
				((platform_renderer_legacy_safe_off_progress_log_counter % 300u) == 0u))
		{
		}
	}
	if (!platform_renderer_legacy_submission_mode_forced_on &&
			!platform_renderer_legacy_submission_mode_forced_off &&
			platform_renderer_legacy_safe_off_latched &&
			!can_submit_legacy)
	{
		bool rollback_bad_coverage =
				!coverage_good_this_frame ||
				(platform_renderer_sdl_native_draw_count < native_primary_min_draws_for_no_mirror) ||
				(platform_renderer_sdl_native_textured_draw_count < native_primary_min_textured_draws_for_no_mirror);

		if (rollback_bad_coverage)
		{
			if (platform_renderer_legacy_safe_off_regression_streak < 1000000)
			{
				platform_renderer_legacy_safe_off_regression_streak++;
			}
		}
		else
		{
			platform_renderer_legacy_safe_off_regression_streak = 0;
		}

		if (platform_renderer_legacy_safe_off_regression_streak >= platform_renderer_legacy_safe_off_rollback_streak)
		{
			platform_renderer_legacy_submission_mode_enabled = true;
			can_submit_legacy = false;
			platform_renderer_legacy_safe_off_latched = false;
			platform_renderer_legacy_safe_off_regression_streak = 0;
			platform_renderer_sdl_native_coverage_streak = 0;
			platform_renderer_sdl_mode_info_logged = false;
		}
	}
	if (native_primary && native_primary_force_no_mirror)
	{
		allow_no_mirror_this_frame = true;
	}
	else if (native_primary && !platform_renderer_sdl_no_mirror_blocked_for_session && (platform_renderer_sdl_no_mirror_cooldown_frames == 0) && coverage_good_this_frame)
	{
		allow_no_mirror_this_frame = true;
	}
	if (PLATFORM_RENDERER_is_sdl2_stub_enabled() && !PLATFORM_RENDERER_is_sdl2_stub_ready())
	{
		if (PLATFORM_RENDERER_prepare_sdl2_stub(width, height, true) && !platform_renderer_sdl_stub_self_test_done)
		{
			(void)PLATFORM_RENDERER_run_sdl2_stub_self_test();
			platform_renderer_sdl_stub_self_test_done = true;
		}
	}
	if (!native_primary)
	{
		used_mirror_fallback = true;
	}
	else if ((!native_primary_strict) || !allow_no_mirror_this_frame)
	{
		// Default primary mode remains safe: mirror while migration coverage is incomplete.
		used_mirror_fallback = true;
	}
	if (PLATFORM_RENDERER_is_sdl2_stub_ready())
	{
		if (platform_renderer_sdl_native_draw_count < native_primary_min_draws_for_no_mirror)
		{
			if (native_primary)
			{
				if (native_primary_force_no_mirror)
				{
					snprintf(platform_renderer_sdl_status, sizeof(platform_renderer_sdl_status), "SDL2 strict no-mirror forced: draws=%d textured=%d geom_miss=%d degraded=%d allow_degraded=%d hot_src=%d(%d) streak=%d cooldown=%d blocked=%d.", platform_renderer_sdl_native_draw_count, platform_renderer_sdl_native_textured_draw_count, platform_renderer_sdl_geometry_fallback_miss_count, platform_renderer_sdl_geometry_degraded_count, 0, geom_hot_source, geom_hot_count, platform_renderer_sdl_native_coverage_streak, platform_renderer_sdl_no_mirror_cooldown_frames, platform_renderer_sdl_no_mirror_blocked_for_session ? 1 : 0);
				}
				else
				{
					snprintf(platform_renderer_sdl_status, sizeof(platform_renderer_sdl_status), "SDL2 strict-primary safe mode: mirrored fallback active. draws=%d textured=%d geom_miss=%d degraded=%d hot_src=%d(%d) streak=%d.", platform_renderer_sdl_native_draw_count, platform_renderer_sdl_native_textured_draw_count, platform_renderer_sdl_geometry_fallback_miss_count, platform_renderer_sdl_geometry_degraded_count, geom_hot_source, geom_hot_count, platform_renderer_sdl_native_coverage_streak);
				}
			}
			else if (native_primary_requested)
			{
				snprintf(platform_renderer_sdl_status, sizeof(platform_renderer_sdl_status), "SDL2 native primary requested (compat mode); using mirrored fallback (draws=%d textured=%d geom_miss=%d degraded=%d hot_src=%d(%d)).", platform_renderer_sdl_native_draw_count, platform_renderer_sdl_native_textured_draw_count, platform_renderer_sdl_geometry_fallback_miss_count, platform_renderer_sdl_geometry_degraded_count, geom_hot_source, geom_hot_count);
			}
			else
			{
				snprintf(platform_renderer_sdl_status, sizeof(platform_renderer_sdl_status), "SDL2 native sprites enabled; currently no native draw-path calls this frame (showing mirror fallback).");
			}
		}
		{
			const bool should_present_sdl = true;
			platform_renderer_sdl_present_frame_counter++;
			/* Flush deferred ADD batch before measuring render_ms so the cost is included. */
			PLATFORM_RENDERER_flush_addq();
			/* Track total frame time (wall-clock from end of previous present to end of this one). */
			static Uint32 s_last_frame_end_ticks = 0;
			Uint32 frame_start_ticks = SDL_GetTicks();
			Uint32 frame_ms = (s_last_frame_end_ticks > 0) ? (frame_start_ticks - s_last_frame_end_ticks) : 0;
			/* render_ms = time from clear_backbuffer to present start = SDL draw-submission cost */
			Uint32 render_ms = (s_frame_clear_ticks > 0 && frame_start_ticks >= s_frame_clear_ticks)
								? (frame_start_ticks - s_frame_clear_ticks) : 0;
			Uint32 present_t0 = frame_start_ticks;
			if (should_present_sdl)
			{
				SDL_RenderPresent(platform_renderer_sdl_renderer);
			}
			Uint32 present_ms = SDL_GetTicks() - present_t0;
			s_last_frame_end_ticks = SDL_GetTicks();
			sdl_present_debug_counter++;
			if ((sdl_present_debug_counter <= 300u) || ((sdl_present_debug_counter % 300u) == 0u))
			{
				fprintf(stderr,
					"[FRAME %u] draws=%d textured=%d nontex=%d winspr=%d geom_miss=%d degraded=%d "
					"tx_sw=%d clip_sw=%d blend_sw=%d colmod_sw=%d skip=%d add_d=%d spec_d=%d defer_d=%d present_h=%d frame_ms=%u render_ms=%u present_ms=%u\n",
					sdl_present_debug_counter,
					platform_renderer_sdl_native_draw_count,
					platform_renderer_sdl_native_textured_draw_count,
					platform_renderer_sdl_native_draw_count - platform_renderer_sdl_native_textured_draw_count,
					platform_renderer_sdl_window_sprite_draw_count,
					platform_renderer_sdl_geometry_fallback_miss_count,
					platform_renderer_sdl_geometry_degraded_count,
					platform_renderer_sdl_tx_switch_count,
					platform_renderer_sdl_clip_switch_count,
					platform_renderer_sdl_blend_switch_count,
					platform_renderer_sdl_colmod_switch_count,
					platform_renderer_sdl_alpha_skip_count,
					platform_renderer_sdl_add_draw_count,
					platform_renderer_sdl_spec_draw_count,
					platform_renderer_sdl_defer_draw_count,
					platform_renderer_present_height,
					(unsigned int)frame_ms,
					(unsigned int)render_ms,
					(unsigned int)present_ms);
			}
		}
	}
	if (native_primary)
	{
		// Keep end-of-frame flip active in strict auto mode so mirror-fallback
		// transitions always sample a fresh frame instead of stale buffer content.
		if (can_submit_legacy && !native_primary_force_no_mirror)
		{
			should_flip_legacy_output = true;
		}
		else
		{
			should_flip_legacy_output = can_submit_legacy && used_mirror_fallback;
		}
	}
	if (platform_renderer_sdl_stub_show_enabled && can_submit_legacy)
	{
		/*
		 * Keep the final flip at the canonical end-of-frame position when SDL is
		 * also presenting.
		 */
		should_flip_legacy_output = true;
	}
	platform_renderer_clear_backbuffer_calls_since_present = 0;
	platform_renderer_midframe_reset_events = 0;
}

bool PLATFORM_RENDERER_prepare_sdl2_stub(int width, int height, bool windowed)
{
	PLATFORM_RENDERER_refresh_sdl_stub_env_flags();

	if (!PLATFORM_RENDERER_is_sdl2_stub_enabled())
	{
		strcpy(platform_renderer_sdl_status, "SDL2 stub disabled.");
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

	/*
	 * Enable SDL batch rendering. SDL processes its command queue in strict
	 * FIFO order — draw order is fully preserved. Consecutive SDL_RenderGeometry
	 * calls with the same texture+blend are merged into a single GL draw call,
	 * collapsing ~2400 per-frame calls into ~20 on GLES2/Panfrost (~10x speedup).
	 * Mixed primitive/sprite scenes (lines + overlays) are safe: SDL only merges
	 * consecutive same-type/same-texture commands, never across different draw types.
	 */
	(void)SDL_SetHint(SDL_HINT_RENDER_BATCHING, "1");

	/* Use nearest-neighbour filtering for textures (pixel-art game). */
	(void)SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");

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
	else
	{
		/* Non-windowed (e.g. PortMaster/embedded): fill the display without
		 * altering the desktop resolution (SDL handles scaling). */
		window_flags |= SDL_WINDOW_FULLSCREEN_DESKTOP;
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

	/* Grab keyboard/input focus immediately — required on embedded devices
	 * (e.g. muOS/PortMaster) where the window manager does not auto-focus
	 * new windows, causing SDL_GetKeyboardState to return all-zeros. */
	SDL_RaiseWindow(platform_renderer_sdl_window);
	SDL_SetWindowInputFocus(platform_renderer_sdl_window);

	if (platform_renderer_sdl_stub_accel_enabled)
	{
		bool force_software_renderer = false;
		platform_renderer_sdl_renderer_software_forced = force_software_renderer;
		if (force_software_renderer)
		{
			platform_renderer_sdl_renderer = SDL_CreateRenderer(
					platform_renderer_sdl_window,
					-1,
					SDL_RENDERER_SOFTWARE);
		}
		else
		{
			const bool novsync = PLATFORM_RENDERER_env_enabled("WIZBALL_SDL2_NOVSYNC");
			const Uint32 accel_flags = novsync
					? SDL_RENDERER_ACCELERATED
					: SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC;
			platform_renderer_sdl_renderer = SDL_CreateRenderer(
					platform_renderer_sdl_window,
					-1,
					accel_flags);
		}
	}
	else
	{
		platform_renderer_sdl_renderer_software_forced = true;
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

	{
		SDL_RendererInfo renderer_info;
		bool force_software_now = false;
		bool renderer_backend_is_gpu = false;
		if (SDL_GetRendererInfo(platform_renderer_sdl_renderer, &renderer_info) == 0)
		{
			const char *renderer_name = (renderer_info.name != NULL) ? renderer_info.name : "";
			renderer_backend_is_gpu = ((renderer_info.flags & SDL_RENDERER_ACCELERATED) != 0) ||
																(strstr(renderer_name, "opengl") != NULL) ||
																(strstr(renderer_name, "metal") != NULL) ||
																(strstr(renderer_name, "vulkan") != NULL) ||
																(strstr(renderer_name, "direct3d") != NULL);
		}
		if (force_software_now && renderer_backend_is_gpu)
		{
			SDL_DestroyRenderer(platform_renderer_sdl_renderer);
			platform_renderer_sdl_renderer = SDL_CreateRenderer(
					platform_renderer_sdl_window,
					-1,
					SDL_RENDERER_SOFTWARE);
			platform_renderer_sdl_renderer_software_forced = true;
		}
		if (platform_renderer_sdl_renderer == NULL)
		{
			snprintf(platform_renderer_sdl_status, sizeof(platform_renderer_sdl_status), "SDL_CreateRenderer fallback-to-software failed: %s", SDL_GetError());
			SDL_DestroyWindow(platform_renderer_sdl_window);
			platform_renderer_sdl_window = NULL;
			if (platform_renderer_started_sdl_video)
			{
				SDL_QuitSubSystem(needed_flags & ~already_flags);
				platform_renderer_started_sdl_video = false;
			}
			return false;
		}

		{
			SDL_RendererInfo renderer_info;
			if (SDL_GetRendererInfo(platform_renderer_sdl_renderer, &renderer_info) == 0)
			{
				const bool renderer_is_accelerated = ((renderer_info.flags & SDL_RENDERER_ACCELERATED) != 0);
				if (!renderer_is_accelerated)
				{
					platform_renderer_sdl_renderer_software_forced = true;
				}
				if (!platform_renderer_sdl_renderer_info_logged)
				{
					SDL_version compiled_ver, linked_ver;
					SDL_VERSION(&compiled_ver);
					SDL_GetVersion(&linked_ver);
					const bool linked_has_geometry = (linked_ver.major > 2) ||
						(linked_ver.major == 2 && linked_ver.minor > 0) ||
						(linked_ver.major == 2 && linked_ver.minor == 0 && linked_ver.patch >= 18);
					fprintf(stderr,
						"[SDL-INIT] compiled=%d.%d.%d linked=%d.%d.%d "
						"renderer='%s' flags=0x%x accel=%d software=%d "
						"has_RenderGeometry=%s\n",
						compiled_ver.major, compiled_ver.minor, compiled_ver.patch,
						linked_ver.major, linked_ver.minor, linked_ver.patch,
						renderer_info.name ? renderer_info.name : "(null)",
						(unsigned int)renderer_info.flags,
						(renderer_info.flags & SDL_RENDERER_ACCELERATED) ? 1 : 0,
						(renderer_info.flags & SDL_RENDERER_SOFTWARE) ? 1 : 0,
						linked_has_geometry ? "yes" : "NO");
					platform_renderer_sdl_renderer_info_logged = true;
				}
			}
		}
	}

	if (platform_renderer_sdl_stub_show_enabled)
	{
		if (platform_renderer_sdl_stub_accel_enabled && !platform_renderer_sdl_renderer_software_forced)
		{
			strcpy(platform_renderer_sdl_status, "SDL2 stub initialized (visible window + accelerated renderer).");
		}
		else if (platform_renderer_sdl_renderer_software_forced)
		{
			strcpy(platform_renderer_sdl_status, "SDL2 stub initialized (visible window + software renderer forced for native-test stability).");
		}
		else
		{
			strcpy(platform_renderer_sdl_status, "SDL2 stub initialized (visible window + software renderer).");
		}
	}
	else
	{
		if (platform_renderer_sdl_stub_accel_enabled && !platform_renderer_sdl_renderer_software_forced)
		{
			strcpy(platform_renderer_sdl_status, "SDL2 stub initialized (hidden window + accelerated renderer).");
		}
		else if (platform_renderer_sdl_renderer_software_forced)
		{
			strcpy(platform_renderer_sdl_status, "SDL2 stub initialized (hidden window + software renderer forced for native-test stability).");
		}
		else
		{
			strcpy(platform_renderer_sdl_status, "SDL2 stub initialized (hidden window + software renderer).");
		}
	}
	/* Create a 1x1 white RGBA texture used to route SDL_RenderFillRect / SDL_RenderDrawPoint
	 * calls through SDL_RenderGeometry.  All draw calls then share one command type, letting
	 * SDL batch consecutive same-texture calls and avoiding the ~100us per-call GPU
	 * pipeline-state flush that "draw_rects"-type commands cause when interleaved with
	 * geometry commands on GLES2/Panfrost (~400 flushes/frame = 40ms overhead). */
	if ((platform_renderer_sdl_renderer != NULL) && (platform_renderer_white_texture == NULL))
	{
		SDL_Texture *wt = SDL_CreateTexture(
				platform_renderer_sdl_renderer,
				SDL_PIXELFORMAT_RGBA8888,
				SDL_TEXTUREACCESS_STATIC,
				1, 1);
		if (wt != NULL)
		{
			const Uint32 white_pixel = 0xFFFFFFFFu;  /* R=FF G=FF B=FF A=FF in RGBA8888 */
			SDL_UpdateTexture(wt, NULL, &white_pixel, 4);
			SDL_SetTextureBlendMode(wt, SDL_BLENDMODE_BLEND);
			platform_renderer_white_texture = wt;
		}
	}
	/* Map all draw calls to game-space coordinates regardless of the actual
	 * display size or DPI scaling.  SDL will letterbox/pillarbox as needed. */
	if (SDL_RenderSetLogicalSize(platform_renderer_sdl_renderer, width, height) != 0)
	{
		char warn_buf[256];
		snprintf(warn_buf, sizeof(warn_buf), "SDL_RenderSetLogicalSize(%d,%d) failed: %s", width, height, SDL_GetError());
		MAIN_add_to_log(warn_buf);
	}
	(void)PLATFORM_RENDERER_sync_sdl_render_target_size(width, height);
	return true;
}

SDL_Renderer *PLATFORM_RENDERER_SDL_Renderer()
{
	return platform_renderer_sdl_renderer;
}

bool PLATFORM_RENDERER_is_sdl2_stub_ready(void)
{
	return (platform_renderer_sdl_window != NULL && platform_renderer_sdl_renderer != NULL);
}

bool PLATFORM_RENDERER_is_sdl2_stub_enabled(void)
{
	PLATFORM_RENDERER_refresh_sdl_stub_env_flags();
	return true;
}

bool PLATFORM_RENDERER_run_sdl2_stub_self_test(void)
{
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
}

const char *PLATFORM_RENDERER_get_sdl2_stub_status(void)
{
	return platform_renderer_sdl_status;
}

void PLATFORM_RENDERER_shutdown(void)
{
	PLATFORM_RENDERER_reset_texture_registry();
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
}
