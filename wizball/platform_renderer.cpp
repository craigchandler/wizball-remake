#include <alleggl.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#ifdef WIZBALL_USE_SDL2
#include <SDL.h>
#endif

#include "platform_renderer.h"
#include "main.h"
#include "output.h"
#include "scripting.h"

#ifdef WIZBALL_USE_SDL2
static void PLATFORM_RENDERER_apply_sdl_texture_blend_mode(SDL_Texture *texture);
static Uint8 PLATFORM_RENDERER_get_sdl_texture_alpha_mod(Uint8 requested_alpha);
#endif

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
	SDL_Texture *sdl_texture_premultiplied;
	SDL_Texture *sdl_texture_inverted;
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
static bool platform_renderer_blend_enabled = false;
enum
{
	PLATFORM_RENDERER_BLEND_MODE_ALPHA = 0,
	PLATFORM_RENDERER_BLEND_MODE_ADD = 1,
	PLATFORM_RENDERER_BLEND_MODE_SUBTRACT = 2
};
static int platform_renderer_blend_mode = PLATFORM_RENDERER_BLEND_MODE_ALPHA;
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
static bool platform_renderer_sdl_dual_present_checked_env = false;
static bool platform_renderer_sdl_dual_present_enabled = false;
static bool platform_renderer_sdl_present_with_gl_checked_env = false;
static bool platform_renderer_sdl_present_with_gl_enabled = false;
static bool platform_renderer_sdl_native_test_mode_checked_env = false;
static bool platform_renderer_sdl_native_test_mode_enabled = false;
static bool platform_renderer_sdl_compare_checked_env = false;
static bool platform_renderer_sdl_compare_enabled = false;
static bool platform_renderer_sdl_compare_every_checked_env = false;
static int platform_renderer_sdl_compare_every = 30;
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
static int platform_renderer_sdl_window_sprite_draw_count = 0;
static int platform_renderer_sdl_window_solid_rect_draw_count = 0;
static int platform_renderer_sdl_bound_custom_draw_count = 0;
static int platform_renderer_gl_window_sprite_draw_count = 0;
static int platform_renderer_gl_window_solid_rect_draw_count = 0;
static int platform_renderer_gl_bound_custom_draw_count = 0;
static int platform_renderer_gl_src6_count = 0;
static float platform_renderer_gl_src6_min_q = 0.0f;
static float platform_renderer_gl_src6_max_q = 0.0f;
static float platform_renderer_gl_src6_min_x = 0.0f;
static float platform_renderer_gl_src6_min_y = 0.0f;
static float platform_renderer_gl_src6_max_x = 0.0f;
static float platform_renderer_gl_src6_max_y = 0.0f;
static int platform_renderer_gl_draw_count = 0;
static int platform_renderer_gl_textured_draw_count = 0;
static int platform_renderer_clear_backbuffer_calls_since_present = 0;
static int platform_renderer_midframe_reset_events = 0;
static int platform_renderer_sdl_output_width = 0;
static int platform_renderer_sdl_output_height = 0;
static float platform_renderer_sdl_scale_x = 1.0f;
static float platform_renderer_sdl_scale_y = 1.0f;
static bool platform_renderer_sdl_renderer_software_forced = false;
static bool platform_renderer_sdl_renderer_info_logged = false;
static bool platform_renderer_sdl_mode_info_logged = false;
static bool platform_renderer_gl_mode_checked_env = false;
static bool platform_renderer_gl_mode_enabled = true;
static bool platform_renderer_gl_mode_forced_on = false;
static bool platform_renderer_gl_mode_forced_off = false;
static bool platform_renderer_gl_safe_off_enabled = false;
static bool platform_renderer_gl_safe_off_latched = false;
static int platform_renderer_gl_safe_off_required_streak = 240;
static int platform_renderer_gl_safe_off_regression_streak = 0;
static int platform_renderer_gl_safe_off_rollback_streak = 6;
static unsigned int platform_renderer_gl_safe_off_progress_log_counter = 0;
static bool platform_renderer_gl_diag_overlay_checked_env = false;
static bool platform_renderer_gl_diag_overlay_enabled = false;
static bool platform_renderer_gl_diag_trace_checked_env = false;
static bool platform_renderer_gl_diag_trace_enabled = false;
static bool platform_renderer_gl_texture_enable_supported = true;
static bool platform_renderer_gl_texture_enable_warned = false;
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
static int platform_renderer_gl_draw_sources[PLATFORM_RENDERER_GEOM_SRC_COUNT] = {0};
static int platform_renderer_sdl_source_bounds_count[PLATFORM_RENDERER_GEOM_SRC_COUNT] = {0};
static float platform_renderer_sdl_source_min_x[PLATFORM_RENDERER_GEOM_SRC_COUNT] = {0.0f};
static float platform_renderer_sdl_source_min_y[PLATFORM_RENDERER_GEOM_SRC_COUNT] = {0.0f};
static float platform_renderer_sdl_source_max_x[PLATFORM_RENDERER_GEOM_SRC_COUNT] = {0.0f};
static float platform_renderer_sdl_source_max_y[PLATFORM_RENDERER_GEOM_SRC_COUNT] = {0.0f};
static int platform_renderer_gl_viewport_last[4] = {0, 0, 0, 0};
static int platform_renderer_gl_scissor_last[4] = {0, 0, 0, 0};
static int platform_renderer_gl_scissor_enabled_last = 0;
static int platform_renderer_gl_draw_buffer_last = 0;
static int platform_renderer_gl_read_buffer_last = 0;
static int platform_renderer_gl_matrix_mode_last = 0;
static int platform_renderer_gl_modelview_identity_last = 0;
static int platform_renderer_gl_projection_identity_last = 0;
static float platform_renderer_gl_modelview_tx_last = 0.0f;
static float platform_renderer_gl_modelview_ty_last = 0.0f;
static float platform_renderer_gl_projection_tx_last = 0.0f;
static float platform_renderer_gl_projection_ty_last = 0.0f;
static int platform_renderer_gl_error_last = 0;
static int platform_renderer_gl_diag_first_error_code = 0;
static unsigned int platform_renderer_gl_diag_error_count_frame = 0;
static char platform_renderer_gl_diag_first_error_stage[64] = "-";
static int platform_renderer_sdl_native_coverage_streak = 0;
static int platform_renderer_sdl_no_mirror_cooldown_frames = 0;
static bool platform_renderer_sdl_no_mirror_blocked_for_session = false;
static int platform_renderer_present_width = 0;
static int platform_renderer_present_height = 0;
static bool platform_renderer_started_sdl_video = false;
static char platform_renderer_sdl_status[256] = "SDL2 stub not checked.";
static char platform_renderer_sdl_last_printed_status[512] = "";
static int platform_renderer_sdl_status_print_tick = 0;
static bool platform_renderer_sdl_sprite_trace_checked_env = false;
static bool platform_renderer_sdl_sprite_trace_enabled = false;
static unsigned int platform_renderer_sdl_sprite_trace_counter = 0;
static bool platform_renderer_sdl_subtractive_texture_supported = true;
static bool platform_renderer_sdl_subtractive_texture_support_checked = false;
static bool platform_renderer_sdl_diag_verbose_checked_env = false;
static bool platform_renderer_sdl_diag_verbose_enabled = false;
static unsigned int platform_renderer_sdl_draw_skip_log_counter = 0;
static unsigned int platform_renderer_sdl_compare_frame_counter = 0;
static unsigned int platform_renderer_compare_last_gl_hash = 0;
static unsigned int platform_renderer_compare_last_sdl_hash = 0;
static int platform_renderer_compare_gl_same_sdl_changed_streak = 0;
static int platform_renderer_prev_sdl_draw_count = 0;
static int platform_renderer_prev_sdl_textured_count = 0;
static int platform_renderer_prev_gl_draw_count = 0;
static int platform_renderer_prev_gl_textured_count = 0;
static unsigned int platform_renderer_draw_transition_counter = 0;
static int platform_renderer_sdl_compare_width = 0;
static int platform_renderer_sdl_compare_height = 0;
static int platform_renderer_sdl_compare_pitch = 0;
static unsigned char *platform_renderer_sdl_compare_gl_pixels = NULL;
static unsigned char *platform_renderer_sdl_compare_sdl_pixels = NULL;
#endif

#ifdef WIZBALL_USE_SDL2
static void PLATFORM_RENDERER_refresh_sdl_stub_env_flags(void);
static int PLATFORM_RENDERER_find_texture_index_from_gl_id(unsigned int gl_texture_id);

static void PLATFORM_RENDERER_log_native_draw_transition(
	unsigned int frame_number,
	int prev_sdl_draws,
	int prev_sdl_textured,
	int prev_gl_draws,
	int prev_gl_textured,
	int current_sdl_draws,
	int current_sdl_textured,
	int current_gl_draws,
	int current_gl_textured)
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
	fprintf(
		stderr,
		"[SDL2-DRAW-TRANSITION] n=%u frame=%u event=%s sdl=%d/%d->%d/%d gl=%d/%d->%d/%d game_state=%d program_state=%d windows(active=%d pending=%d skipped=%d nonempty_z=%d first_nonempty=%d:%d total=%d) entities(used=%d alive=%d free=%d limbo=%d lost=%d next=%d) clear_calls=%d midframe_resets=%d\n",
		platform_renderer_draw_transition_counter,
		frame_number,
		now_zero ? "enter_zero" : "leave_zero",
		prev_sdl_draws,
		prev_sdl_textured,
		current_sdl_draws,
		current_sdl_textured,
		prev_gl_draws,
		prev_gl_textured,
		current_gl_draws,
		current_gl_textured,
		game_state,
		program_state,
		active_windows,
		pending_active_windows,
		skipped_windows,
		windows_with_nonempty_z,
		first_nonempty_window,
		first_nonempty_z,
		number_of_windows,
		used_entities,
		alive_process_counter,
		free_entities,
		limboed_entities,
		lost_entities,
		global_next_entity,
		platform_renderer_clear_backbuffer_calls_since_present,
		platform_renderer_midframe_reset_events);
}
#endif

static bool PLATFORM_RENDERER_should_use_gl(void)
{
#ifdef WIZBALL_USE_SDL2
	if (!platform_renderer_gl_mode_checked_env)
	{
		const char *enable_gl = getenv("WIZBALL_SDL2_ENABLE_GL");
		const char *disable_gl = getenv("WIZBALL_SDL2_DISABLE_GL");
		const char *safe_gl_off = getenv("WIZBALL_SDL2_SAFE_GL_OFF");
		const char *safe_gl_off_streak = getenv("WIZBALL_SDL2_SAFE_GL_OFF_STREAK");
		const char *safe_gl_off_rollback_streak = getenv("WIZBALL_SDL2_SAFE_GL_OFF_ROLLBACK_STREAK");

		platform_renderer_gl_mode_enabled = true;
		platform_renderer_gl_mode_forced_on = false;
		platform_renderer_gl_mode_forced_off = false;
		platform_renderer_gl_safe_off_enabled = false;
		platform_renderer_gl_safe_off_latched = false;
		platform_renderer_gl_safe_off_required_streak = 240;
		platform_renderer_gl_safe_off_regression_streak = 0;
		platform_renderer_gl_safe_off_rollback_streak = 6;
		platform_renderer_gl_safe_off_progress_log_counter = 0;

		if (enable_gl != NULL)
		{
			if ((strcmp(enable_gl, "1") == 0) ||
				(strcmp(enable_gl, "true") == 0) ||
				(strcmp(enable_gl, "TRUE") == 0) ||
				(strcmp(enable_gl, "yes") == 0) ||
				(strcmp(enable_gl, "on") == 0))
			{
				platform_renderer_gl_mode_enabled = true;
				platform_renderer_gl_mode_forced_on = true;
			}
		}

		if (disable_gl != NULL)
		{
			if ((strcmp(disable_gl, "1") == 0) ||
				(strcmp(disable_gl, "true") == 0) ||
				(strcmp(disable_gl, "TRUE") == 0) ||
				(strcmp(disable_gl, "yes") == 0) ||
				(strcmp(disable_gl, "on") == 0))
			{
				platform_renderer_gl_mode_enabled = false;
				platform_renderer_gl_mode_forced_off = true;
			}
		}

		if (!platform_renderer_gl_mode_forced_on && !platform_renderer_gl_mode_forced_off && (safe_gl_off != NULL))
		{
			if ((strcmp(safe_gl_off, "1") == 0) ||
				(strcmp(safe_gl_off, "true") == 0) ||
				(strcmp(safe_gl_off, "TRUE") == 0) ||
				(strcmp(safe_gl_off, "yes") == 0) ||
				(strcmp(safe_gl_off, "on") == 0))
			{
				platform_renderer_gl_safe_off_enabled = true;
			}
		}

		if (platform_renderer_gl_safe_off_enabled && (safe_gl_off_streak != NULL) && (*safe_gl_off_streak != '\0'))
		{
			char *endptr = NULL;
			long parsed = strtol(safe_gl_off_streak, &endptr, 10);
			if ((endptr != safe_gl_off_streak) && ((endptr == NULL) || (*endptr == '\0')) && (parsed > 0) && (parsed <= 1000000))
			{
				platform_renderer_gl_safe_off_required_streak = (int) parsed;
			}
		}
		if (platform_renderer_gl_safe_off_enabled && (safe_gl_off_rollback_streak != NULL) && (*safe_gl_off_rollback_streak != '\0'))
		{
			char *endptr = NULL;
			long parsed = strtol(safe_gl_off_rollback_streak, &endptr, 10);
			if ((endptr != safe_gl_off_rollback_streak) && ((endptr == NULL) || (*endptr == '\0')) && (parsed > 0) && (parsed <= 1000000))
			{
				platform_renderer_gl_safe_off_rollback_streak = (int) parsed;
			}
		}

		platform_renderer_gl_mode_checked_env = true;
	}
	return platform_renderer_gl_mode_enabled;
#else
	/*
	 * Keep legacy AllegroGL path active by default.
	 * SDL native migration runs alongside GL (mirror/compare) until an explicit
	 * full-SDL switch is introduced.
	 */
	return true;
#endif
}

#ifdef WIZBALL_USE_SDL2
static bool PLATFORM_RENDERER_env_enabled(const char *name);

static void PLATFORM_RENDERER_gl_diag_check_stage(const char *stage)
{
	GLenum gl_error;

	if (!PLATFORM_RENDERER_should_use_gl())
	{
		return;
	}
	if (!platform_renderer_gl_diag_trace_checked_env)
	{
		platform_renderer_gl_diag_trace_enabled = PLATFORM_RENDERER_env_enabled("WIZBALL_GL_DIAG_TRACE");
		platform_renderer_gl_diag_trace_checked_env = true;
	}
	if (!platform_renderer_gl_diag_trace_enabled)
	{
		return;
	}

	while ((gl_error = glGetError()) != GL_NO_ERROR)
	{
		platform_renderer_gl_diag_error_count_frame++;
		if (platform_renderer_gl_diag_first_error_code == 0)
		{
			platform_renderer_gl_diag_first_error_code = (int) gl_error;
			snprintf(
				platform_renderer_gl_diag_first_error_stage,
				sizeof(platform_renderer_gl_diag_first_error_stage),
				"%s",
				(stage != NULL) ? stage : "unknown");
		}
	}
}
#else
static void PLATFORM_RENDERER_gl_diag_check_stage(const char *stage)
{
	(void) stage;
}
#endif

#ifdef WIZBALL_USE_SDL2
static void PLATFORM_RENDERER_diag_log(const char *tag, const char *message)
{
	if (!platform_renderer_sdl_diag_verbose_enabled)
	{
		return;
	}

	if ((tag == NULL) || (message == NULL))
	{
		return;
	}

	fprintf(stderr, "[%s] %s\n", tag, message);
	fflush(stderr);
}

static void PLATFORM_RENDERER_diag_log_draw_skip(const char *context, const char *reason, unsigned int texture_handle)
{
	if (!platform_renderer_sdl_diag_verbose_enabled)
	{
		return;
	}

	platform_renderer_sdl_draw_skip_log_counter++;
	if ((platform_renderer_sdl_draw_skip_log_counter > 600) && ((platform_renderer_sdl_draw_skip_log_counter % 2000) != 0))
	{
		return;
	}

	fprintf(
		stderr,
		"[SDL2-DIAG] draw-skip n=%u ctx=%s reason=%s tex=%u tex_count=%d bound=%u renderer=%p present_h=%d\n",
		platform_renderer_sdl_draw_skip_log_counter,
		(context != NULL) ? context : "unknown",
		(reason != NULL) ? reason : "unknown",
		texture_handle,
		platform_renderer_texture_count,
		platform_renderer_last_bound_texture_handle,
		(void *) platform_renderer_sdl_renderer,
		platform_renderer_present_height);
	fflush(stderr);
}

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

static bool PLATFORM_RENDERER_is_sdl_sprite_trace_enabled(void)
{
	PLATFORM_RENDERER_refresh_sdl_stub_env_flags();
	if (platform_renderer_sdl_diag_verbose_enabled)
	{
		return true;
	}

	if (!platform_renderer_sdl_sprite_trace_checked_env)
	{
		platform_renderer_sdl_sprite_trace_enabled = PLATFORM_RENDERER_env_enabled("WIZBALL_SDL2_SPRITE_TRACE");
		platform_renderer_sdl_sprite_trace_checked_env = true;
	}
	return platform_renderer_sdl_sprite_trace_enabled;
}

static bool PLATFORM_RENDERER_should_force_subtractive_mod_path(void)
{
	/*
	 * In SAFE_GL_OFF we run fully native SDL in strict/no-mirror mode.
	 * Some SDL renderer backends report custom subtractive blend support but
	 * still produce rectangular matte artefacts on masked subtractive sprites.
	 * Force the stable alpha-weighted inverted-texture MOD path in this mode.
	 */
	return platform_renderer_gl_safe_off_enabled;
}

static bool PLATFORM_RENDERER_using_subtractive_mod_fallback(void)
{
	if (!platform_renderer_blend_enabled ||
		(platform_renderer_blend_mode != PLATFORM_RENDERER_BLEND_MODE_SUBTRACT))
	{
		return false;
	}

	if (PLATFORM_RENDERER_should_force_subtractive_mod_path())
	{
		return true;
	}

	return
		platform_renderer_sdl_subtractive_texture_support_checked &&
		(!platform_renderer_sdl_subtractive_texture_supported);
}

static void PLATFORM_RENDERER_trace_sdl_sprite_draw(
	const char *tag,
	const platform_renderer_texture_entry *entry,
	const SDL_Rect *src_rect,
	const SDL_Rect *dst_rect,
	SDL_RendererFlip flip,
	int vertex_count)
{
	if (!PLATFORM_RENDERER_is_sdl_sprite_trace_enabled())
	{
		return;
	}

	platform_renderer_sdl_sprite_trace_counter++;
	if ((platform_renderer_sdl_sprite_trace_counter > 800) && ((platform_renderer_sdl_sprite_trace_counter % 2000) != 0))
	{
		return;
	}

	{
		SDL_BlendMode texture_blend = SDL_BLENDMODE_NONE;
		Uint8 mod_r = 0;
		Uint8 mod_g = 0;
		Uint8 mod_b = 0;
		Uint8 mod_a = 0;
		int src_x = -1;
		int src_y = -1;
		int src_w = -1;
		int src_h = -1;
		int dst_x = -1;
		int dst_y = -1;
		int dst_w = -1;
		int dst_h = -1;

		if ((entry != NULL) && (entry->sdl_texture != NULL))
		{
			(void) SDL_GetTextureBlendMode(entry->sdl_texture, &texture_blend);
			(void) SDL_GetTextureColorMod(entry->sdl_texture, &mod_r, &mod_g, &mod_b);
			(void) SDL_GetTextureAlphaMod(entry->sdl_texture, &mod_a);
		}
		if (src_rect != NULL)
		{
			src_x = src_rect->x;
			src_y = src_rect->y;
			src_w = src_rect->w;
			src_h = src_rect->h;
		}
		if (dst_rect != NULL)
		{
			dst_x = dst_rect->x;
			dst_y = dst_rect->y;
			dst_w = dst_rect->w;
			dst_h = dst_rect->h;
		}

		fprintf(
			stderr,
			"[SDL2-SPRITE] n=%u tag=%s tex=%p size=%dx%d src=%d,%d %dx%d dst=%d,%d %dx%d flip=%d vtx=%d draw_blend_en=%d draw_blend_mode=%d tex_blend=%d mod=%u,%u,%u,%u\n",
			platform_renderer_sdl_sprite_trace_counter,
			(tag != NULL) ? tag : "unknown",
			(void *) ((entry != NULL) ? entry->sdl_texture : NULL),
			(entry != NULL) ? entry->width : 0,
			(entry != NULL) ? entry->height : 0,
			src_x, src_y, src_w, src_h,
			dst_x, dst_y, dst_w, dst_h,
			(int) flip,
			vertex_count,
			platform_renderer_blend_enabled ? 1 : 0,
			platform_renderer_blend_mode,
			(int) texture_blend,
			(unsigned int) mod_r,
			(unsigned int) mod_g,
			(unsigned int) mod_b,
			(unsigned int) mod_a);
	}
}

static void PLATFORM_RENDERER_refresh_sdl_stub_env_flags(void)
{
	if (!platform_renderer_sdl_diag_verbose_checked_env)
	{
		platform_renderer_sdl_diag_verbose_enabled = PLATFORM_RENDERER_env_enabled("WIZBALL_SDL2_DIAG_VERBOSE");
		platform_renderer_sdl_diag_verbose_checked_env = true;
	}

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
				// Geometry has produced alpha/blend regressions on some SDL renderers.
				// Keep it opt-in so default strict mode stays on the stable copy/copyex path.
				platform_renderer_sdl_geometry_enabled = false;
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
	if (!platform_renderer_sdl_dual_present_checked_env)
	{
		platform_renderer_sdl_dual_present_enabled = PLATFORM_RENDERER_env_enabled("WIZBALL_SDL2_DUAL_PRESENT");
		platform_renderer_sdl_dual_present_checked_env = true;
	}
	if (!platform_renderer_sdl_present_with_gl_checked_env)
	{
		platform_renderer_sdl_present_with_gl_enabled = PLATFORM_RENDERER_env_enabled("WIZBALL_SDL2_PRESENT_WITH_GL");
		platform_renderer_sdl_present_with_gl_checked_env = true;
	}
	if (!platform_renderer_sdl_native_test_mode_checked_env)
	{
		platform_renderer_sdl_native_test_mode_enabled = PLATFORM_RENDERER_env_enabled("WIZBALL_SDL2_NATIVE_TEST_MODE");
		platform_renderer_sdl_native_test_mode_checked_env = true;
	}
	if (!platform_renderer_gl_diag_overlay_checked_env)
	{
		platform_renderer_gl_diag_overlay_enabled = PLATFORM_RENDERER_env_enabled("WIZBALL_GL_DIAG_OVERLAY");
		platform_renderer_gl_diag_overlay_checked_env = true;
	}
	if (!platform_renderer_gl_diag_trace_checked_env)
	{
		platform_renderer_gl_diag_trace_enabled = PLATFORM_RENDERER_env_enabled("WIZBALL_GL_DIAG_TRACE");
		platform_renderer_gl_diag_trace_checked_env = true;
	}
	if (!platform_renderer_sdl_compare_checked_env)
	{
		platform_renderer_sdl_compare_enabled = PLATFORM_RENDERER_env_enabled("WIZBALL_SDL2_COMPARE_WINDOWS");
		platform_renderer_sdl_compare_checked_env = true;
	}
	if (!platform_renderer_sdl_compare_every_checked_env)
	{
		platform_renderer_sdl_compare_every = PLATFORM_RENDERER_env_int_or_default("WIZBALL_SDL2_COMPARE_EVERY", 30, 1, 600);
		platform_renderer_sdl_compare_every_checked_env = true;
	}
	if (platform_renderer_sdl_diag_verbose_enabled)
	{
		platform_renderer_sdl_status_stderr_enabled = true;
		platform_renderer_sdl_status_stderr_throttle_enabled = false;
		platform_renderer_sdl_sprite_trace_enabled = true;
		platform_renderer_sdl_sprite_trace_checked_env = true;
		platform_renderer_sdl_compare_enabled = true;
		platform_renderer_sdl_compare_checked_env = true;
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
	if (platform_renderer_sdl_native_test_mode_enabled)
	{
		/*
		 * Native test mode is the migration harness:
		 * - SDL native primary path only
		 * - no mirror fallback mixing
		 * - verbose status/compare logs enabled
		 */
		platform_renderer_sdl_stub_enabled = true;
		platform_renderer_sdl_native_primary_enabled = true;
		platform_renderer_sdl_native_primary_strict_enabled = true;
		platform_renderer_sdl_native_primary_force_no_mirror_enabled = true;
		platform_renderer_sdl_native_sprite_enabled = true;
		platform_renderer_sdl_mirror_enabled = false;
		platform_renderer_sdl_dual_present_enabled = false;
		if (getenv("WIZBALL_SDL2_PRESENT_WITH_GL") == NULL)
		{
			/*
			 * In native-test mode keep GL as the owner of the visible swap by
			 * default; SDL present can be re-enabled explicitly for A/B checks.
			 */
			platform_renderer_sdl_present_with_gl_enabled = false;
		}
		platform_renderer_sdl_compare_enabled = true;
		platform_renderer_sdl_status_stderr_enabled = true;
		platform_renderer_sdl_status_stderr_throttle_enabled = false;
		if (getenv("WIZBALL_SDL2_GEOMETRY") == NULL)
		{
			/*
			 * Exercise true geometry in native test mode so degradation counters
			 * reflect unsupported cases, not default-disabled geometry.
			 */
			platform_renderer_sdl_geometry_enabled = true;
		}
	}
	if (platform_renderer_gl_safe_off_enabled &&
		!platform_renderer_gl_mode_forced_on &&
		!platform_renderer_gl_mode_forced_off)
	{
		/*
		 * Safe GL-off migration mode:
		 * keep GL active initially, but ensure SDL strict-primary paths are
		 * fully exercised so coverage streak can meaningfully latch GL off.
		 */
		platform_renderer_sdl_stub_enabled = true;
		platform_renderer_sdl_native_primary_enabled = true;
		platform_renderer_sdl_native_primary_strict_enabled = true;
		platform_renderer_sdl_native_primary_force_no_mirror_enabled = true;
		platform_renderer_sdl_native_primary_auto_no_mirror_enabled = false;
			platform_renderer_sdl_native_sprite_enabled = true;
			platform_renderer_sdl_mirror_enabled = false;
			platform_renderer_sdl_dual_present_enabled = false;
			platform_renderer_sdl_present_with_gl_enabled = false;
			if (getenv("WIZBALL_SDL2_GEOMETRY") == NULL)
			{
				platform_renderer_sdl_geometry_enabled = true;
			}
			if (getenv("WIZBALL_SDL2_STUB_SHOW") == NULL)
			{
				platform_renderer_sdl_stub_show_enabled = true;
			}
	}
	if (platform_renderer_sdl_dual_present_enabled)
	{
		/*
		 * Stability guard:
		 * dual-present mode is for side-by-side visual validation and should keep
		 * GL as the authoritative renderer while SDL shows mirrored output.
		 * Running native primary + mirror together currently causes visible
		 * flicker on some paths.
		 */
		platform_renderer_sdl_native_primary_enabled = false;
		platform_renderer_sdl_native_sprite_enabled = false;
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
		if (getenv("WIZBALL_SDL2_GEOMETRY") == NULL)
		{
			/*
			 * Keep native SDL rendering on one textured path by default.
			 * This avoids copy/geometry mismatches (UV/alpha/orientation) that
			 * show up on animated sprites and shadow quads.
			 */
			platform_renderer_sdl_geometry_enabled = true;
		}
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
		if (platform_renderer_gl_safe_off_enabled &&
			!platform_renderer_gl_mode_forced_on &&
			!platform_renderer_gl_mode_forced_off)
		{
			/*
			 * Final safe-mode enforcement after generic native-sprite policy:
			 * run strict SDL-primary without mirror fallback while GL is still on,
			 * so latch/rollback decisions are based on true native coverage.
			 */
			platform_renderer_sdl_stub_enabled = true;
			platform_renderer_sdl_native_primary_enabled = true;
			platform_renderer_sdl_native_primary_strict_enabled = true;
			platform_renderer_sdl_native_primary_force_no_mirror_enabled = true;
			platform_renderer_sdl_native_primary_auto_no_mirror_enabled = false;
			platform_renderer_sdl_native_sprite_enabled = true;
			platform_renderer_sdl_mirror_enabled = false;
			platform_renderer_sdl_dual_present_enabled = false;
			platform_renderer_sdl_present_with_gl_enabled = false;
			if (getenv("WIZBALL_SDL2_GEOMETRY") == NULL)
			{
				platform_renderer_sdl_geometry_enabled = true;
			}
		}
		if (!PLATFORM_RENDERER_should_use_gl())
		{
		/*
		 * Full SDL mode: no AllegroGL ownership and no GL mirror fallback.
		 */
		platform_renderer_sdl_stub_enabled = true;
		platform_renderer_sdl_native_sprite_enabled = true;
		platform_renderer_sdl_native_primary_enabled = true;
		platform_renderer_sdl_native_primary_strict_enabled = true;
		platform_renderer_sdl_native_primary_force_no_mirror_enabled = true;
		platform_renderer_sdl_native_primary_auto_no_mirror_enabled = false;
		platform_renderer_sdl_mirror_enabled = false;
		platform_renderer_sdl_dual_present_enabled = false;
		platform_renderer_sdl_present_with_gl_enabled = false;
		platform_renderer_sdl_compare_enabled = false;
		if (getenv("WIZBALL_SDL2_STUB_SHOW") == NULL)
		{
			platform_renderer_sdl_stub_show_enabled = true;
		}
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

static void PLATFORM_RENDERER_note_gl_draw(bool textured)
{
	platform_renderer_gl_draw_count++;
	if (textured)
	{
		platform_renderer_gl_textured_draw_count++;
	}
}

static void PLATFORM_RENDERER_note_gl_draw_source(int source_id, bool textured)
{
	PLATFORM_RENDERER_note_gl_draw(textured);
	if ((source_id > 0) && (source_id < PLATFORM_RENDERER_GEOM_SRC_COUNT))
	{
		platform_renderer_gl_draw_sources[source_id]++;
	}
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

	min_x = (float) rect->x;
	min_y = (float) rect->y;
	max_x = (float) (rect->x + rect->w);
	max_y = (float) (rect->y + rect->h);
	if (platform_renderer_sdl_source_bounds_count[source_id] <= 0)
	{
		platform_renderer_sdl_source_min_x[source_id] = min_x;
		platform_renderer_sdl_source_min_y[source_id] = min_y;
		platform_renderer_sdl_source_max_x[source_id] = max_x;
		platform_renderer_sdl_source_max_y[source_id] = max_y;
		platform_renderer_sdl_source_bounds_count[source_id] = 1;
		return;
	}

	if (min_x < platform_renderer_sdl_source_min_x[source_id]) platform_renderer_sdl_source_min_x[source_id] = min_x;
	if (min_y < platform_renderer_sdl_source_min_y[source_id]) platform_renderer_sdl_source_min_y[source_id] = min_y;
	if (max_x > platform_renderer_sdl_source_max_x[source_id]) platform_renderer_sdl_source_max_x[source_id] = max_x;
	if (max_y > platform_renderer_sdl_source_max_y[source_id]) platform_renderer_sdl_source_max_y[source_id] = max_y;
	platform_renderer_sdl_source_bounds_count[source_id]++;
}

static void PLATFORM_RENDERER_record_gl_src6_bounds_and_q(
	float x0, float y0,
	float x1, float y1,
	float x2, float y2,
	float x3, float y3,
	float q)
{
	float min_x = x0;
	float min_y = y0;
	float max_x = x0;
	float max_y = y0;
	if (x1 < min_x) min_x = x1;
	if (x2 < min_x) min_x = x2;
	if (x3 < min_x) min_x = x3;
	if (y1 < min_y) min_y = y1;
	if (y2 < min_y) min_y = y2;
	if (y3 < min_y) min_y = y3;
	if (x1 > max_x) max_x = x1;
	if (x2 > max_x) max_x = x2;
	if (x3 > max_x) max_x = x3;
	if (y1 > max_y) max_y = y1;
	if (y2 > max_y) max_y = y2;
	if (y3 > max_y) max_y = y3;

	if (platform_renderer_gl_src6_count == 0)
	{
		platform_renderer_gl_src6_min_q = q;
		platform_renderer_gl_src6_max_q = q;
		platform_renderer_gl_src6_min_x = min_x;
		platform_renderer_gl_src6_min_y = min_y;
		platform_renderer_gl_src6_max_x = max_x;
		platform_renderer_gl_src6_max_y = max_y;
	}
	else
	{
		if (q < platform_renderer_gl_src6_min_q) platform_renderer_gl_src6_min_q = q;
		if (q > platform_renderer_gl_src6_max_q) platform_renderer_gl_src6_max_q = q;
		if (min_x < platform_renderer_gl_src6_min_x) platform_renderer_gl_src6_min_x = min_x;
		if (min_y < platform_renderer_gl_src6_min_y) platform_renderer_gl_src6_min_y = min_y;
		if (max_x > platform_renderer_gl_src6_max_x) platform_renderer_gl_src6_max_x = max_x;
		if (max_y > platform_renderer_gl_src6_max_y) platform_renderer_gl_src6_max_y = max_y;
	}
	platform_renderer_gl_src6_count++;
}

static bool PLATFORM_RENDERER_sync_sdl_render_target_size(int width, int height)
{
	if ((platform_renderer_sdl_renderer == NULL) || (width <= 0) || (height <= 0))
	{
		return false;
	}

	(void) SDL_RenderSetViewport(platform_renderer_sdl_renderer, NULL);
	(void) SDL_RenderSetClipRect(platform_renderer_sdl_renderer, NULL);
#if SDL_VERSION_ATLEAST(2, 0, 5)
	(void) SDL_RenderSetIntegerScale(platform_renderer_sdl_renderer, SDL_FALSE);
#endif
	if (SDL_RenderSetLogicalSize(platform_renderer_sdl_renderer, 0, 0) != 0)
	{
		PLATFORM_RENDERER_diag_log("SDL2-DIAG", SDL_GetError());
	}
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
				sx = (float) out_w / (float) width;
				sy = (float) out_h / (float) height;
			}
		}
		platform_renderer_sdl_scale_x = sx;
		platform_renderer_sdl_scale_y = sy;
		if (SDL_RenderSetScale(platform_renderer_sdl_renderer, sx, sy) != 0)
		{
			PLATFORM_RENDERER_diag_log("SDL2-DIAG", SDL_GetError());
		}
	}

	return true;
}

static bool PLATFORM_RENDERER_should_force_software_renderer_for_gl_side_by_side(void)
{
#ifdef WIZBALL_USE_SDL2
	/*
	 * When GL remains active while SDL is also presenting, keep SDL on software
	 * renderer to avoid backend context interference across windows.
	 */
	return platform_renderer_sdl_native_test_mode_enabled &&
		(platform_renderer_sdl_present_with_gl_enabled || PLATFORM_RENDERER_should_use_gl());
#else
	return false;
#endif
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
	(void) indices;
	(void) index_count;
	if (platform_renderer_sdl_native_primary_enabled && platform_renderer_sdl_native_primary_strict_enabled)
	{
		/*
		 * In strict native mode we avoid copy/copyex rectangle fallback entirely:
		 * it causes UV/alpha mismatches (e.g. rectangular shadows) versus GL.
		 */
		return false;
	}
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
	float min_v = PLATFORM_RENDERER_convert_v_to_sdl(vertices[0].tex_coord.y);
	float max_v = min_v;
	int i;
	for (i = 1; i < 4; i++)
	{
		if (vertices[i].position.x < min_x) min_x = vertices[i].position.x;
		if (vertices[i].position.x > max_x) max_x = vertices[i].position.x;
		if (vertices[i].position.y < min_y) min_y = vertices[i].position.y;
		if (vertices[i].position.y > max_y) max_y = vertices[i].position.y;
		if (vertices[i].tex_coord.x < min_u) min_u = vertices[i].tex_coord.x;
		if (vertices[i].tex_coord.x > max_u) max_u = vertices[i].tex_coord.x;
		{
			const float sv = PLATFORM_RENDERER_convert_v_to_sdl(vertices[i].tex_coord.y);
			if (sv < min_v) min_v = sv;
			if (sv > max_v) max_v = sv;
		}
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
	if (vertices[1].tex_coord.y > vertices[0].tex_coord.y)
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

			PLATFORM_RENDERER_apply_sdl_texture_blend_mode(texture);
	(void) SDL_SetTextureColorMod(texture, vertices[0].color.r, vertices[0].color.g, vertices[0].color.b);
			(void) SDL_SetTextureAlphaMod(texture, PLATFORM_RENDERER_get_sdl_texture_alpha_mod(vertices[0].color.a));
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
			PLATFORM_RENDERER_diag_log("SDL2-DIAG", SDL_GetError());
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

	PLATFORM_RENDERER_apply_sdl_texture_blend_mode(texture);
	(void) SDL_SetTextureColorMod(texture, vertices[0].color.r, vertices[0].color.g, vertices[0].color.b);
	(void) SDL_SetTextureAlphaMod(texture, PLATFORM_RENDERER_get_sdl_texture_alpha_mod(vertices[0].color.a));
	if (SDL_RenderCopyEx(platform_renderer_sdl_renderer, texture, &src_rect, &dst_rect, 0.0, NULL, final_flip) == 0)
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
	PLATFORM_RENDERER_diag_log("SDL2-DIAG", SDL_GetError());

	return false;
}

static bool PLATFORM_RENDERER_try_sdl_geometry_textured(SDL_Texture *texture, const SDL_Vertex *vertices, int vertex_count, const int *indices, int index_count, int source_id)
{
	SDL_Vertex converted_vertices[8];
	const SDL_Vertex *work_vertices = vertices;
	int v;

	if ((vertices != NULL) && (vertex_count > 0) && (vertex_count <= 8))
	{
		for (v = 0; v < vertex_count; v++)
		{
			converted_vertices[v] = vertices[v];
			converted_vertices[v].tex_coord.y = PLATFORM_RENDERER_convert_v_to_sdl(vertices[v].tex_coord.y);
		}
		work_vertices = converted_vertices;
	}

	if (!PLATFORM_RENDERER_is_sdl_geometry_enabled())
	{
		const bool force_coloured_geometry =
			(source_id == PLATFORM_RENDERER_GEOM_SRC_PERSPECTIVE) ||
			(source_id == PLATFORM_RENDERER_GEOM_SRC_COLOURED_ARRAY) ||
			(source_id == PLATFORM_RENDERER_GEOM_SRC_COLOURED_PERSPECTIVE);

		if (force_coloured_geometry && (texture != NULL))
		{
			PLATFORM_RENDERER_apply_sdl_texture_blend_mode(texture);
			if (SDL_RenderGeometry(platform_renderer_sdl_renderer, texture, work_vertices, vertex_count, indices, index_count) == 0)
			{
				SDL_Rect geom_bounds;
				float min_x = work_vertices[0].position.x;
				float min_y = work_vertices[0].position.y;
				float max_x = work_vertices[0].position.x;
				float max_y = work_vertices[0].position.y;
				for (v = 1; v < vertex_count; v++)
				{
					if (work_vertices[v].position.x < min_x) min_x = work_vertices[v].position.x;
					if (work_vertices[v].position.y < min_y) min_y = work_vertices[v].position.y;
					if (work_vertices[v].position.x > max_x) max_x = work_vertices[v].position.x;
					if (work_vertices[v].position.y > max_y) max_y = work_vertices[v].position.y;
				}
				geom_bounds.x = (int) min_x;
				geom_bounds.y = (int) min_y;
				geom_bounds.w = (int) (max_x - min_x);
				geom_bounds.h = (int) (max_y - min_y);
				if (geom_bounds.w <= 0) geom_bounds.w = 1;
				if (geom_bounds.h <= 0) geom_bounds.h = 1;
				PLATFORM_RENDERER_trace_sdl_sprite_draw("render-geometry-forced-colour", NULL, NULL, NULL, SDL_FLIP_NONE, vertex_count);
				platform_renderer_sdl_native_draw_count++;
				platform_renderer_sdl_native_textured_draw_count++;
				PLATFORM_RENDERER_note_sdl_draw_source(source_id);
				PLATFORM_RENDERER_record_sdl_source_bounds_rect(source_id, &geom_bounds);
				return true;
			}
			PLATFORM_RENDERER_diag_log("SDL2-DIAG", SDL_GetError());
		}

		if (texture == NULL)
		{
			// Keep untextured geometry active even when textured-geometry mode is off.
			// This preserves coloured/perspective primitives used by UI/gameplay layers.
			if (SDL_RenderGeometry(platform_renderer_sdl_renderer, NULL, work_vertices, vertex_count, indices, index_count) == 0)
			{
				PLATFORM_RENDERER_trace_sdl_sprite_draw("geom-disabled-untextured", NULL, NULL, NULL, SDL_FLIP_NONE, vertex_count);
				platform_renderer_sdl_native_draw_count++;
				return true;
			}
			PLATFORM_RENDERER_diag_log("SDL2-DIAG", SDL_GetError());
		}
		PLATFORM_RENDERER_trace_sdl_sprite_draw("geom-disabled-fallback", NULL, NULL, NULL, SDL_FLIP_NONE, vertex_count);
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

	if (texture != NULL)
	{
		PLATFORM_RENDERER_apply_sdl_texture_blend_mode(texture);
	}
	if (SDL_RenderGeometry(platform_renderer_sdl_renderer, texture, work_vertices, vertex_count, indices, index_count) == 0)
	{
		SDL_Rect geom_bounds;
		float min_x = work_vertices[0].position.x;
		float min_y = work_vertices[0].position.y;
		float max_x = work_vertices[0].position.x;
		float max_y = work_vertices[0].position.y;
		for (v = 1; v < vertex_count; v++)
		{
			if (work_vertices[v].position.x < min_x) min_x = work_vertices[v].position.x;
			if (work_vertices[v].position.y < min_y) min_y = work_vertices[v].position.y;
			if (work_vertices[v].position.x > max_x) max_x = work_vertices[v].position.x;
			if (work_vertices[v].position.y > max_y) max_y = work_vertices[v].position.y;
		}
		geom_bounds.x = (int) min_x;
		geom_bounds.y = (int) min_y;
		geom_bounds.w = (int) (max_x - min_x);
		geom_bounds.h = (int) (max_y - min_y);
		if (geom_bounds.w <= 0) geom_bounds.w = 1;
		if (geom_bounds.h <= 0) geom_bounds.h = 1;
		PLATFORM_RENDERER_trace_sdl_sprite_draw("render-geometry", NULL, NULL, NULL, SDL_FLIP_NONE, vertex_count);
		platform_renderer_sdl_native_draw_count++;
		platform_renderer_sdl_native_textured_draw_count++;
		PLATFORM_RENDERER_note_sdl_draw_source(source_id);
		PLATFORM_RENDERER_record_sdl_source_bounds_rect(source_id, &geom_bounds);
		return true;
	}
	PLATFORM_RENDERER_diag_log("SDL2-DIAG", SDL_GetError());

	PLATFORM_RENDERER_trace_sdl_sprite_draw("render-geometry-fallback", NULL, NULL, NULL, SDL_FLIP_NONE, vertex_count);
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
		PLATFORM_RENDERER_diag_log_draw_skip("build-sdl-texture", "entry-or-renderer-null", 0);
		return false;
	}

	if ((entry->sdl_texture != NULL) || (entry->sdl_rgba_pixels == NULL) || (entry->width <= 0) || (entry->height <= 0))
	{
		if ((entry->sdl_texture == NULL) && (entry->sdl_rgba_pixels == NULL))
		{
			PLATFORM_RENDERER_diag_log_draw_skip("build-sdl-texture", "rgba-pixels-missing", 0);
		}
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
		PLATFORM_RENDERER_diag_log("SDL2-DIAG", SDL_GetError());
		return false;
	}

	if (SDL_UpdateTexture(entry->sdl_texture, NULL, entry->sdl_rgba_pixels, entry->width * 4) != 0)
	{
		PLATFORM_RENDERER_diag_log("SDL2-DIAG", SDL_GetError());
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

	pixel_count = (size_t) entry->width * (size_t) entry->height;
	byte_count = pixel_count * 4u;
	inverted_pixels = (unsigned char *) malloc(byte_count);
	if (inverted_pixels == NULL)
	{
		return NULL;
	}

	for (i = 0; i < pixel_count; i++)
	{
		size_t base = i * 4u;
		unsigned int a = (unsigned int) entry->sdl_rgba_pixels[base + 3];
		unsigned int src_r = ((unsigned int) entry->sdl_rgba_pixels[base + 0] * a) / 255u;
		unsigned int src_g = ((unsigned int) entry->sdl_rgba_pixels[base + 1] * a) / 255u;
		unsigned int src_b = ((unsigned int) entry->sdl_rgba_pixels[base + 2] * a) / 255u;

		/*
		 * Subtractive fallback uses MOD, so we feed 1-src into the texture.
		 * Weight by alpha first so fully transparent texels become neutral white
		 * (no dark rectangle around masked sprites like lab shadows).
		 */
		inverted_pixels[base + 0] = (unsigned char) (255u - src_r);
		inverted_pixels[base + 1] = (unsigned char) (255u - src_g);
		inverted_pixels[base + 2] = (unsigned char) (255u - src_b);
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
	(void) SDL_SetTextureBlendMode(entry->sdl_texture_inverted, SDL_BLENDMODE_MOD);
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

	pixel_count = (size_t) entry->width * (size_t) entry->height;
	byte_count = pixel_count * 4u;
	premul_pixels = (unsigned char *) malloc(byte_count);
	if (premul_pixels == NULL)
	{
		return NULL;
	}

	for (i = 0; i < pixel_count; i++)
	{
		size_t base = i * 4u;
		unsigned int a = (unsigned int) entry->sdl_rgba_pixels[base + 3];
		unsigned int src_r = ((unsigned int) entry->sdl_rgba_pixels[base + 0] * a) / 255u;
		unsigned int src_g = ((unsigned int) entry->sdl_rgba_pixels[base + 1] * a) / 255u;
		unsigned int src_b = ((unsigned int) entry->sdl_rgba_pixels[base + 2] * a) / 255u;

		premul_pixels[base + 0] = (unsigned char) src_r;
		premul_pixels[base + 1] = (unsigned char) src_g;
		premul_pixels[base + 2] = (unsigned char) src_b;
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
	(void) SDL_SetTextureBlendMode(entry->sdl_texture_premultiplied, SDL_BLENDMODE_BLEND);
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
		(PLATFORM_RENDERER_should_force_subtractive_mod_path() ||
		 (platform_renderer_sdl_subtractive_texture_support_checked &&
		  (!platform_renderer_sdl_subtractive_texture_supported))))
	{
		SDL_Texture *inverted = PLATFORM_RENDERER_build_sdl_inverted_texture_from_entry(entry);
		if (inverted != NULL)
		{
			(void) SDL_SetTextureBlendMode(inverted, SDL_BLENDMODE_MOD);
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
	float tex_w_f = (float) entry->width;
	float tex_h_f = (float) entry->height;
	const float uv_eps = 0.0001f;
	int src_x1 = (int) floorf((src_u_min * tex_w_f) + uv_eps);
	int src_x2 = (int) ceilf((src_u_max * tex_w_f) - uv_eps);
	int src_y1 = (int) floorf((src_v_min * tex_h_f) + uv_eps);
	int src_y2 = (int) ceilf((src_v_max * tex_h_f) - uv_eps);
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

	split_x = dst_rect->x + (int) floorf(((float) dst_rect->w * t_split) + 0.5f);
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
		int indices[6] = { 0, 1, 2, 0, 2, 3 };
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

		vertices[0].position.x = (float) dst_a.x;
		vertices[0].position.y = (float) dst_a.y;
		vertices[1].position.x = (float) dst_a.x;
		vertices[1].position.y = (float) (dst_a.y + dst_a.h);
		vertices[2].position.x = (float) (dst_a.x + dst_a.w);
		vertices[2].position.y = (float) (dst_a.y + dst_a.h);
		vertices[3].position.x = (float) (dst_a.x + dst_a.w);
		vertices[3].position.y = (float) dst_a.y;
		vertices[0].tex_coord.x = a_u_left;  vertices[0].tex_coord.y = v_top;
		vertices[1].tex_coord.x = a_u_left;  vertices[1].tex_coord.y = v_bottom;
		vertices[2].tex_coord.x = a_u_right; vertices[2].tex_coord.y = v_bottom;
		vertices[3].tex_coord.x = a_u_right; vertices[3].tex_coord.y = v_top;
		vertices[0].color.r = vertices[1].color.r = vertices[2].color.r = vertices[3].color.r = mod_r;
		vertices[0].color.g = vertices[1].color.g = vertices[2].color.g = vertices[3].color.g = mod_g;
		vertices[0].color.b = vertices[1].color.b = vertices[2].color.b = vertices[3].color.b = mod_b;
		vertices[0].color.a = vertices[1].color.a = vertices[2].color.a = vertices[3].color.a = mod_a;
		if (!PLATFORM_RENDERER_try_sdl_geometry_textured(draw_texture, vertices, 4, indices, 6, source_id))
		{
			return false;
		}

		vertices[0].position.x = (float) dst_b.x;
		vertices[0].position.y = (float) dst_b.y;
		vertices[1].position.x = (float) dst_b.x;
		vertices[1].position.y = (float) (dst_b.y + dst_b.h);
		vertices[2].position.x = (float) (dst_b.x + dst_b.w);
		vertices[2].position.y = (float) (dst_b.y + dst_b.h);
		vertices[3].position.x = (float) (dst_b.x + dst_b.w);
		vertices[3].position.y = (float) dst_b.y;
		vertices[0].tex_coord.x = b_u_left;  vertices[0].tex_coord.y = v_top;
		vertices[1].tex_coord.x = b_u_left;  vertices[1].tex_coord.y = v_bottom;
		vertices[2].tex_coord.x = b_u_right; vertices[2].tex_coord.y = v_bottom;
		vertices[3].tex_coord.x = b_u_right; vertices[3].tex_coord.y = v_top;
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

	(void) SDL_SetRenderDrawColor(platform_renderer_sdl_renderer, (Uint8) ir, (Uint8) ig, (Uint8) ib, (Uint8) ia);
}

static bool PLATFORM_RENDERER_transform_state_is_finite(void)
{
	return
		isfinite(platform_renderer_tx_a) &&
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

	if (PLATFORM_RENDERER_should_use_gl())
	{
		glMatrixMode(GL_MODELVIEW);
		glLoadIdentity();
	}

	transform_reset_counter++;
	if ((transform_reset_counter <= 200) || ((transform_reset_counter % 1000) == 0))
	{
		fprintf(
			stderr,
			"[GL-DIAG] reset non-finite transform state n=%u (%s)\n",
			transform_reset_counter,
			(reason != NULL) ? reason : "unknown");
	}
}

#ifdef WIZBALL_USE_SDL2
static void PLATFORM_RENDERER_apply_sdl_draw_blend_mode(void)
{
	if ((platform_renderer_sdl_renderer == NULL) || !platform_renderer_blend_enabled)
	{
		if (platform_renderer_sdl_renderer != NULL)
		{
			(void) SDL_SetRenderDrawBlendMode(platform_renderer_sdl_renderer, SDL_BLENDMODE_NONE);
		}
		return;
	}

	switch (platform_renderer_blend_mode)
	{
		case PLATFORM_RENDERER_BLEND_MODE_ADD:
			(void) SDL_SetRenderDrawBlendMode(platform_renderer_sdl_renderer, SDL_BLENDMODE_ADD);
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
					fprintf(stderr, "[SDL2-DIAG] custom subtractive draw blend unsupported, using MOD fallback: %s\n", SDL_GetError());
					sdl_subtractive_draw_warned = true;
				}
				(void) SDL_SetRenderDrawBlendMode(platform_renderer_sdl_renderer, SDL_BLENDMODE_MOD);
			}
			break;
		}
		case PLATFORM_RENDERER_BLEND_MODE_ALPHA:
		default:
			(void) SDL_SetRenderDrawBlendMode(platform_renderer_sdl_renderer, SDL_BLENDMODE_BLEND);
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

static void PLATFORM_RENDERER_apply_sdl_texture_blend_mode(SDL_Texture *texture)
{
	if (texture == NULL)
	{
		return;
	}

	if (!platform_renderer_blend_enabled)
	{
		/*
		 * GL paths often render cutout sprites with blending disabled and alpha-test enabled.
		 * SDL has no alpha-test equivalent in RenderCopy, so keep texture blending on
		 * to preserve transparent texels instead of drawing opaque matte backgrounds.
		 */
		(void) SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
		return;
	}

	switch (platform_renderer_blend_mode)
	{
		case PLATFORM_RENDERER_BLEND_MODE_ADD:
			(void) SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_ADD);
			break;
		case PLATFORM_RENDERER_BLEND_MODE_SUBTRACT:
		{
			if (PLATFORM_RENDERER_should_force_subtractive_mod_path())
			{
				platform_renderer_sdl_subtractive_texture_support_checked = true;
				platform_renderer_sdl_subtractive_texture_supported = false;
				(void) SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_MOD);
				break;
			}

			SDL_BlendMode subtract_mode = SDL_ComposeCustomBlendMode(
				SDL_BLENDFACTOR_ZERO, SDL_BLENDFACTOR_ONE_MINUS_SRC_COLOR, SDL_BLENDOPERATION_ADD,
				SDL_BLENDFACTOR_ZERO, SDL_BLENDFACTOR_ONE, SDL_BLENDOPERATION_ADD);
			platform_renderer_sdl_subtractive_texture_support_checked = true;
			if (SDL_SetTextureBlendMode(texture, subtract_mode) != 0)
			{
				static bool sdl_subtractive_texture_warned = false;
				platform_renderer_sdl_subtractive_texture_supported = false;
				if (!sdl_subtractive_texture_warned)
				{
					fprintf(stderr, "[SDL2-DIAG] custom subtractive texture blend unsupported, using MOD fallback: %s\n", SDL_GetError());
					sdl_subtractive_texture_warned = true;
				}
				(void) SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_MOD);
			}
			else
			{
				platform_renderer_sdl_subtractive_texture_supported = true;
			}
			break;
		}
		case PLATFORM_RENDERER_BLEND_MODE_ALPHA:
		default:
			(void) SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
			break;
	}
}
#endif
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

bool PLATFORM_RENDERER_is_gl_enabled(void)
{
	return PLATFORM_RENDERER_should_use_gl();
}

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
#endif
	free(platform_renderer_texture_entries);
	platform_renderer_texture_entries = NULL;
	platform_renderer_texture_count = 0;
	platform_renderer_texture_capacity = 0;
	platform_renderer_last_bound_texture_handle = 0;
}

static unsigned int PLATFORM_RENDERER_register_gl_texture(unsigned int gl_texture_id, BITMAP *image)
{
	bool require_nonzero_gl_texture = false;
#ifdef WIZBALL_USE_SDL2
	require_nonzero_gl_texture =
		PLATFORM_RENDERER_should_use_gl() &&
		(!platform_renderer_gl_safe_off_enabled || platform_renderer_gl_mode_forced_on);
#else
	require_nonzero_gl_texture = PLATFORM_RENDERER_should_use_gl();
#endif
	if ((gl_texture_id == 0) && require_nonzero_gl_texture)
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
	platform_renderer_texture_entries[platform_renderer_texture_count].sdl_texture_premultiplied = NULL;
	platform_renderer_texture_entries[platform_renderer_texture_count].sdl_texture_inverted = NULL;
	platform_renderer_texture_entries[platform_renderer_texture_count].sdl_rgba_pixels = NULL;
	platform_renderer_texture_entries[platform_renderer_texture_count].width = 0;
	platform_renderer_texture_entries[platform_renderer_texture_count].height = 0;

	if ((image != NULL) && (image->w > 0) && (image->h > 0))
	{
		int x;
		int y;
		int image_width = image->w;
		int image_height = image->h;
		int image_depth = bitmap_color_depth(image);
		int mask_colour = bitmap_mask_color(image);
		int mask_r = getr(mask_colour);
		int mask_g = getg(mask_colour);
		int mask_b = getb(mask_colour);
		unsigned char *rgba_pixels = (unsigned char *) malloc((size_t) image_width * (size_t) image_height * 4u);

		if (rgba_pixels != NULL)
		{
			int mask_pixel_count = 0;
			int alpha_zero_pixel_count = 0;
			int alpha_zero_black_rgb_count = 0;
			for (y = 0; y < image_height; y++)
			{
				for (x = 0; x < image_width; x++)
				{
					int pixel = getpixel(image, x, y);
					size_t index = ((size_t) y * (size_t) image_width + (size_t) x) * 4u;
					int pixel_r = getr(pixel);
					int pixel_g = getg(pixel);
					int pixel_b = getb(pixel);
					bool pixel_is_mask = false;

					/*
					 * Raw packed-pixel equality can fail across colour-depth conversions
					 * even when the visual RGB equals the bitmap mask colour.
					 */
					if (pixel == mask_colour)
					{
						pixel_is_mask = true;
					}
					else if ((pixel_r == mask_r) && (pixel_g == mask_g) && (pixel_b == mask_b))
					{
						pixel_is_mask = true;
					}

					if (pixel_is_mask)
					{
						mask_pixel_count++;
						alpha_zero_pixel_count++;
						if ((pixel_r == 0) && (pixel_g == 0) && (pixel_b == 0))
						{
							alpha_zero_black_rgb_count++;
						}
						// Keep masked texels neutral so multiply-style blend paths do
						// not darken transparent regions to black.
						rgba_pixels[index + 0] = 255u;
						rgba_pixels[index + 1] = 255u;
						rgba_pixels[index + 2] = 255u;
						rgba_pixels[index + 3] = 0u;
					}
					else
					{
						int alpha = 255;
						if (image_depth == 32)
						{
							alpha = geta(pixel);
							if (alpha < 0) alpha = 0;
							if (alpha > 255) alpha = 255;
						}
						rgba_pixels[index + 0] = (unsigned char) pixel_r;
						rgba_pixels[index + 1] = (unsigned char) pixel_g;
						rgba_pixels[index + 2] = (unsigned char) pixel_b;
						rgba_pixels[index + 3] = (unsigned char) alpha;
						if (alpha == 0)
						{
							alpha_zero_pixel_count++;
							if ((rgba_pixels[index + 0] == 0u) && (rgba_pixels[index + 1] == 0u) && (rgba_pixels[index + 2] == 0u))
							{
								alpha_zero_black_rgb_count++;
							}
							// Transparent texels should stay chroma-neutral for MOD/MUL blend paths.
							rgba_pixels[index + 0] = 255u;
							rgba_pixels[index + 1] = 255u;
							rgba_pixels[index + 2] = 255u;
						}
					}
				}
			}

				if (PLATFORM_RENDERER_is_sdl_sprite_trace_enabled())
				{
					static unsigned int texture_trace_counter = 0;
					texture_trace_counter++;
					if ((texture_trace_counter <= 300) || ((texture_trace_counter % 1000) == 0))
					{
						fprintf(
							stderr,
							"[SDL2-TEX] n=%u gl=%u size=%dx%d depth=%d mask=%d alpha0=%d alpha0_black=%d\n",
							texture_trace_counter,
							gl_texture_id,
							image_width,
							image_height,
							image_depth,
							mask_pixel_count,
							alpha_zero_pixel_count,
							alpha_zero_black_rgb_count);
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
	{
		int index = PLATFORM_RENDERER_find_texture_index_from_gl_id(texture_handle);
		if (index >= 0)
		{
			/* Accept legacy/raw GL texture IDs as valid handles during migration. */
			return platform_renderer_texture_entries[index].gl_texture_id;
		}
	}
	return 0;
}

static int PLATFORM_RENDERER_find_texture_index_from_gl_id(unsigned int gl_texture_id)
{
	int i;

	if (gl_texture_id == 0)
	{
		return -1;
	}

	for (i = 0; i < platform_renderer_texture_count; i++)
	{
		if (platform_renderer_texture_entries[i].gl_texture_id == gl_texture_id)
		{
			return i;
		}
	}

	return -1;
}

static platform_renderer_texture_entry *PLATFORM_RENDERER_get_texture_entry(unsigned int texture_handle)
{
	if ((texture_handle >= 1) && (texture_handle <= (unsigned int) platform_renderer_texture_count))
	{
		return &platform_renderer_texture_entries[texture_handle - 1];
	}

	{
		int index = PLATFORM_RENDERER_find_texture_index_from_gl_id(texture_handle);
		if (index >= 0)
		{
			return &platform_renderer_texture_entries[index];
		}
	}

	return NULL;
}

static unsigned int PLATFORM_RENDERER_normalize_texture_handle(unsigned int texture_handle)
{
	if ((texture_handle >= 1) && (texture_handle <= (unsigned int) platform_renderer_texture_count))
	{
		return texture_handle;
	}

	{
		int index = PLATFORM_RENDERER_find_texture_index_from_gl_id(texture_handle);
		if (index >= 0)
		{
			return (unsigned int) (index + 1);
		}
	}

	return 0;
}

static platform_renderer_texture_entry *PLATFORM_RENDERER_get_last_bound_texture_entry(void)
{
	return PLATFORM_RENDERER_get_texture_entry(platform_renderer_last_bound_texture_handle);
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
	platform_renderer_clear_backbuffer_calls_since_present++;
#ifdef WIZBALL_USE_SDL2
	/* Detect resets after draw activity, which can hide earlier GL output. */
	if ((platform_renderer_gl_draw_count > 0) || (platform_renderer_sdl_native_draw_count > 0))
	{
		platform_renderer_midframe_reset_events++;
	}
#endif
	if (PLATFORM_RENDERER_should_use_gl())
	{
		if (!PLATFORM_RENDERER_transform_state_is_finite())
		{
			PLATFORM_RENDERER_reset_transform_state("clear_backbuffer");
		}
		/*
		 * Some drivers/backends can leave buffer routing in an unexpected state
		 * across window/context interactions. Reassert back-buffer draw/read so
		 * AllegroGL flip always presents freshly rendered content.
		 */
		glDrawBuffer(GL_BACK);
		glReadBuffer(GL_BACK);
		/*
		 * Reassert fixed-function 2D defaults each frame so transient state drift
		 * from mixed GL/SDL execution cannot accumulate into blank output.
		 */
		PLATFORM_RENDERER_apply_gl_defaults(SCREEN_W, SCREEN_H, SCREEN_W, SCREEN_H);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	}
#ifdef WIZBALL_USE_SDL2
	platform_renderer_sdl_native_draw_count = 0;
	platform_renderer_sdl_native_textured_draw_count = 0;
	platform_renderer_sdl_geometry_fallback_miss_count = 0;
	platform_renderer_sdl_geometry_degraded_count = 0;
	platform_renderer_sdl_window_sprite_draw_count = 0;
	platform_renderer_sdl_window_solid_rect_draw_count = 0;
	platform_renderer_sdl_bound_custom_draw_count = 0;
	platform_renderer_gl_window_sprite_draw_count = 0;
	platform_renderer_gl_window_solid_rect_draw_count = 0;
	platform_renderer_gl_bound_custom_draw_count = 0;
	platform_renderer_gl_src6_count = 0;
	platform_renderer_gl_src6_min_q = 0.0f;
	platform_renderer_gl_src6_max_q = 0.0f;
	platform_renderer_gl_src6_min_x = 0.0f;
	platform_renderer_gl_src6_min_y = 0.0f;
	platform_renderer_gl_src6_max_x = 0.0f;
	platform_renderer_gl_src6_max_y = 0.0f;
	platform_renderer_gl_draw_count = 0;
	platform_renderer_gl_textured_draw_count = 0;
	memset(platform_renderer_sdl_geometry_miss_sources, 0, sizeof(platform_renderer_sdl_geometry_miss_sources));
	memset(platform_renderer_sdl_geometry_degraded_sources, 0, sizeof(platform_renderer_sdl_geometry_degraded_sources));
	memset(platform_renderer_sdl_native_draw_sources, 0, sizeof(platform_renderer_sdl_native_draw_sources));
	memset(platform_renderer_gl_draw_sources, 0, sizeof(platform_renderer_gl_draw_sources));
	memset(platform_renderer_sdl_source_bounds_count, 0, sizeof(platform_renderer_sdl_source_bounds_count));
	memset(platform_renderer_sdl_source_min_x, 0, sizeof(platform_renderer_sdl_source_min_x));
	memset(platform_renderer_sdl_source_min_y, 0, sizeof(platform_renderer_sdl_source_min_y));
	memset(platform_renderer_sdl_source_max_x, 0, sizeof(platform_renderer_sdl_source_max_x));
	memset(platform_renderer_sdl_source_max_y, 0, sizeof(platform_renderer_sdl_source_max_y));
	memset(platform_renderer_gl_viewport_last, 0, sizeof(platform_renderer_gl_viewport_last));
	memset(platform_renderer_gl_scissor_last, 0, sizeof(platform_renderer_gl_scissor_last));
	platform_renderer_gl_scissor_enabled_last = 0;
	platform_renderer_gl_draw_buffer_last = 0;
	platform_renderer_gl_read_buffer_last = 0;
	platform_renderer_gl_error_last = 0;
	platform_renderer_gl_diag_first_error_code = 0;
	platform_renderer_gl_diag_error_count_frame = 0;
	snprintf(platform_renderer_gl_diag_first_error_stage, sizeof(platform_renderer_gl_diag_first_error_stage), "-");
	if (PLATFORM_RENDERER_is_sdl2_native_sprite_enabled() && PLATFORM_RENDERER_is_sdl2_stub_ready())
	{
		(void) SDL_SetRenderDrawColor(platform_renderer_sdl_renderer, 0, 0, 0, 255);
		(void) SDL_RenderClear(platform_renderer_sdl_renderer);
	}
#endif
}

void PLATFORM_RENDERER_draw_outline_rect(int x1, int y1, int x2, int y2, int r, int g, int b, int virtual_screen_height)
{
	if (PLATFORM_RENDERER_should_use_gl())
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
#ifdef WIZBALL_USE_SDL2
	else if (PLATFORM_RENDERER_is_sdl2_native_sprite_enabled() && PLATFORM_RENDERER_is_sdl2_stub_ready())
	{
		int left = (x1 < x2) ? x1 : x2;
		int right = (x1 > x2) ? x1 : x2;
		int top_input = (y1 < y2) ? y1 : y2;
		int bottom_input = (y1 > y2) ? y1 : y2;
		int top = (virtual_screen_height - 1) - bottom_input;
		int bottom = (virtual_screen_height - 1) - top_input;
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
		(void) SDL_SetRenderDrawColor(platform_renderer_sdl_renderer, (Uint8) r, (Uint8) g, (Uint8) b, 255);
		if (SDL_RenderDrawRect(platform_renderer_sdl_renderer, &rect) == 0)
		{
			platform_renderer_sdl_native_draw_count++;
		}
	}
#endif
}

void PLATFORM_RENDERER_draw_filled_rect(int x1, int y1, int x2, int y2, int r, int g, int b, int virtual_screen_height)
{
	if (PLATFORM_RENDERER_should_use_gl())
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
#ifdef WIZBALL_USE_SDL2
	else if (PLATFORM_RENDERER_is_sdl2_native_sprite_enabled() && PLATFORM_RENDERER_is_sdl2_stub_ready())
	{
		int left = (x1 < x2) ? x1 : x2;
		int right = (x1 > x2) ? x1 : x2;
		int top_input = (y1 < y2) ? y1 : y2;
		int bottom_input = (y1 > y2) ? y1 : y2;
		int top = (virtual_screen_height - 1) - bottom_input;
		int bottom = (virtual_screen_height - 1) - top_input;
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
		(void) SDL_SetRenderDrawColor(platform_renderer_sdl_renderer, (Uint8) r, (Uint8) g, (Uint8) b, 255);
		if (SDL_RenderFillRect(platform_renderer_sdl_renderer, &rect) == 0)
		{
			platform_renderer_sdl_native_draw_count++;
		}
	}
#endif
}

void PLATFORM_RENDERER_draw_line(int x1, int y1, int x2, int y2, int r, int g, int b, int virtual_screen_height)
{
	if (PLATFORM_RENDERER_should_use_gl())
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
#ifdef WIZBALL_USE_SDL2
	else if (PLATFORM_RENDERER_is_sdl2_native_sprite_enabled() && PLATFORM_RENDERER_is_sdl2_stub_ready())
	{
		int sx1 = x1;
		int sy1 = (virtual_screen_height - 1) - y1;
		int sx2 = x2;
		int sy2 = (virtual_screen_height - 1) - y2;
		PLATFORM_RENDERER_apply_sdl_draw_blend_mode();
		(void) SDL_SetRenderDrawColor(platform_renderer_sdl_renderer, (Uint8) r, (Uint8) g, (Uint8) b, 255);
		if (SDL_RenderDrawLine(platform_renderer_sdl_renderer, sx1, sy1, sx2, sy2) == 0)
		{
			platform_renderer_sdl_native_draw_count++;
		}
	}
#endif
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

	if (PLATFORM_RENDERER_should_use_gl())
	{
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
#ifdef WIZBALL_USE_SDL2
	else if (PLATFORM_RENDERER_is_sdl2_native_sprite_enabled() && PLATFORM_RENDERER_is_sdl2_stub_ready())
	{
		PLATFORM_RENDERER_apply_sdl_draw_blend_mode();
		(void) SDL_SetRenderDrawColor(platform_renderer_sdl_renderer, (Uint8) r, (Uint8) g, (Uint8) b, 255);
		for (angle = 0.0f; angle < (2.0f * 3.14159265358979323846f); angle += step)
		{
			float cx1 = (float) radius * cosf(angle);
			float cy1 = (float) radius * sinf(angle);
			float cx2 = (float) radius * cosf(angle + step);
			float cy2 = (float) radius * sinf(angle + step);
			int x_start = x + (int) cx1;
			int y_start = (virtual_screen_height - 1) - (y + (int) cy1);
			int x_end = x + (int) cx2;
			int y_end = (virtual_screen_height - 1) - (y + (int) cy2);
			if (SDL_RenderDrawLine(platform_renderer_sdl_renderer, x_start, y_start, x_end, y_end) == 0)
			{
				platform_renderer_sdl_native_draw_count++;
			}
		}
	}
#endif
}

void PLATFORM_RENDERER_draw_bound_solid_quad(float left, float right, float up, float down)
{
	if (PLATFORM_RENDERER_should_use_gl())
	{
		glBegin(GL_QUADS);
			glVertex2f(left, up);
			glVertex2f(left, down);
			glVertex2f(right, down);
			glVertex2f(right, up);
		glEnd();
	}
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
	if (PLATFORM_RENDERER_should_use_gl())
	{
		PLATFORM_RENDERER_note_gl_draw_source(PLATFORM_RENDERER_GEOM_SRC_BOUND_QUAD, true);
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
#ifdef WIZBALL_USE_SDL2
	if (PLATFORM_RENDERER_is_sdl2_native_sprite_enabled() && PLATFORM_RENDERER_is_sdl2_stub_ready() && (platform_renderer_present_height > 0))
	{
#if SDL_VERSION_ATLEAST(2, 0, 18)
		platform_renderer_texture_entry *entry = PLATFORM_RENDERER_get_last_bound_texture_entry();
		if (entry != NULL)
		{
			if (PLATFORM_RENDERER_build_sdl_texture_from_entry(entry))
			{
				SDL_Texture *draw_texture = PLATFORM_RENDERER_get_effective_texture_for_current_blend(entry);
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
					vertices[i].position.y = (float) platform_renderer_present_height - ty[i];
					vertices[i].tex_coord.x = quad_u[i];
					vertices[i].tex_coord.y = quad_v[i];

					cr = (int) (platform_renderer_current_colour_r * 255.0f);
					cg = (int) (platform_renderer_current_colour_g * 255.0f);
					cb = (int) (platform_renderer_current_colour_b * 255.0f);
					ca = (int) (platform_renderer_current_colour_a * 255.0f);
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
					float gl_left = (tx[0] < tx[2]) ? tx[0] : tx[2];
					float gl_right = (tx[0] > tx[2]) ? tx[0] : tx[2];
					float gl_top = (ty[0] > ty[2]) ? ty[0] : ty[2];
					float gl_bottom = (ty[0] < ty[2]) ? ty[0] : ty[2];
					int mod_r = (int) (platform_renderer_current_colour_r * 255.0f);
					int mod_g = (int) (platform_renderer_current_colour_g * 255.0f);
					int mod_b = (int) (platform_renderer_current_colour_b * 255.0f);
					int mod_a = (int) (platform_renderer_current_colour_a * 255.0f);
					if (PLATFORM_RENDERER_using_subtractive_mod_fallback())
					{
						mod_r = 255;
						mod_g = 255;
						mod_b = 255;
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

					if (mod_r < 0) mod_r = 0; if (mod_r > 255) mod_r = 255;
					if (mod_g < 0) mod_g = 0; if (mod_g > 255) mod_g = 255;
					if (mod_b < 0) mod_b = 0; if (mod_b > 255) mod_b = 255;
					if (mod_a < 0) mod_a = 0; if (mod_a > 255) mod_a = 255;

					if (PLATFORM_RENDERER_draw_axis_wrapped_u_repeat(
						entry,
						draw_texture,
						&dst_rect,
						u1, v1, u2, v2,
						(Uint8) mod_r, (Uint8) mod_g, (Uint8) mod_b, (Uint8) mod_a,
						PLATFORM_RENDERER_GEOM_SRC_BOUND_QUAD))
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
						int indices[6] = { 0, 1, 2, 0, 2, 3 };

						vertices[0].tex_coord.x = u_left;  vertices[0].tex_coord.y = v_top;
						vertices[1].tex_coord.x = u_left;  vertices[1].tex_coord.y = v_bottom;
						vertices[2].tex_coord.x = u_right; vertices[2].tex_coord.y = v_bottom;
						vertices[3].tex_coord.x = u_right; vertices[3].tex_coord.y = v_top;
						vertices[0].color.r = vertices[1].color.r = vertices[2].color.r = vertices[3].color.r = (Uint8) mod_r;
						vertices[0].color.g = vertices[1].color.g = vertices[2].color.g = vertices[3].color.g = (Uint8) mod_g;
						vertices[0].color.b = vertices[1].color.b = vertices[2].color.b = vertices[3].color.b = (Uint8) mod_b;
						vertices[0].color.a = vertices[1].color.a = vertices[2].color.a = vertices[3].color.a = (Uint8) mod_a;

						if (PLATFORM_RENDERER_try_sdl_geometry_textured(draw_texture, vertices, 4, indices, 6, PLATFORM_RENDERER_GEOM_SRC_BOUND_QUAD))
						{
							PLATFORM_RENDERER_record_sdl_source_bounds_rect(PLATFORM_RENDERER_GEOM_SRC_BOUND_QUAD, &dst_rect);
						}
					}
				}
				else
				{
					int indices[6] = { 0, 1, 2, 0, 2, 3 };
					(void) PLATFORM_RENDERER_try_sdl_geometry_textured(draw_texture, vertices, 4, indices, 6, PLATFORM_RENDERER_GEOM_SRC_BOUND_QUAD);
				}
			}
		}
#endif
	}
#endif
}

void PLATFORM_RENDERER_draw_bound_textured_quad_custom(float x0, float y0, float x1, float y1, float x2, float y2, float x3, float y3, float u1, float v1, float u2, float v2)
{
	if (PLATFORM_RENDERER_should_use_gl())
	{
		PLATFORM_RENDERER_note_gl_draw_source(PLATFORM_RENDERER_GEOM_SRC_BOUND_QUAD_CUSTOM, true);
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
#ifdef WIZBALL_USE_SDL2
	if (PLATFORM_RENDERER_is_sdl2_native_sprite_enabled() && PLATFORM_RENDERER_is_sdl2_stub_ready() && (platform_renderer_present_height > 0))
	{
#if SDL_VERSION_ATLEAST(2, 0, 18)
		platform_renderer_texture_entry *entry = PLATFORM_RENDERER_get_last_bound_texture_entry();
		if (entry != NULL)
		{
			if (PLATFORM_RENDERER_build_sdl_texture_from_entry(entry))
			{
				SDL_Texture *draw_texture = PLATFORM_RENDERER_get_effective_texture_for_current_blend(entry);
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
					vertices[i].position.y = (float) platform_renderer_present_height - ty[i];
					vertices[i].tex_coord.x = quad_u[i];
					vertices[i].tex_coord.y = quad_v[i];

					cr = (int) (platform_renderer_current_colour_r * 255.0f);
					cg = (int) (platform_renderer_current_colour_g * 255.0f);
					cb = (int) (platform_renderer_current_colour_b * 255.0f);
					ca = (int) (platform_renderer_current_colour_a * 255.0f);
					if (PLATFORM_RENDERER_using_subtractive_mod_fallback())
					{
						cr = 255;
						cg = 255;
						cb = 255;
					}
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
					float gl_left = (tx[0] < tx[2]) ? tx[0] : tx[2];
					float gl_right = (tx[0] > tx[2]) ? tx[0] : tx[2];
					float gl_top = (ty[0] > ty[2]) ? ty[0] : ty[2];
					float gl_bottom = (ty[0] < ty[2]) ? ty[0] : ty[2];
					int mod_r = (int) (platform_renderer_current_colour_r * 255.0f);
					int mod_g = (int) (platform_renderer_current_colour_g * 255.0f);
					int mod_b = (int) (platform_renderer_current_colour_b * 255.0f);
					int mod_a = (int) (platform_renderer_current_colour_a * 255.0f);
					if (PLATFORM_RENDERER_using_subtractive_mod_fallback())
					{
						mod_r = 255;
						mod_g = 255;
						mod_b = 255;
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

					if (mod_r < 0) mod_r = 0; if (mod_r > 255) mod_r = 255;
					if (mod_g < 0) mod_g = 0; if (mod_g > 255) mod_g = 255;
					if (mod_b < 0) mod_b = 0; if (mod_b > 255) mod_b = 255;
					if (mod_a < 0) mod_a = 0; if (mod_a > 255) mod_a = 255;

					if (PLATFORM_RENDERER_draw_axis_wrapped_u_repeat(
						entry,
						draw_texture,
						&dst_rect,
						u1, v1, u2, v2,
						(Uint8) mod_r, (Uint8) mod_g, (Uint8) mod_b, (Uint8) mod_a,
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
						int indices[6] = { 0, 1, 2, 0, 2, 3 };

						vertices[0].tex_coord.x = u_left;  vertices[0].tex_coord.y = v_top;
						vertices[1].tex_coord.x = u_left;  vertices[1].tex_coord.y = v_bottom;
						vertices[2].tex_coord.x = u_right; vertices[2].tex_coord.y = v_bottom;
						vertices[3].tex_coord.x = u_right; vertices[3].tex_coord.y = v_top;
						vertices[0].color.r = vertices[1].color.r = vertices[2].color.r = vertices[3].color.r = (Uint8) mod_r;
						vertices[0].color.g = vertices[1].color.g = vertices[2].color.g = vertices[3].color.g = (Uint8) mod_g;
						vertices[0].color.b = vertices[1].color.b = vertices[2].color.b = vertices[3].color.b = (Uint8) mod_b;
						vertices[0].color.a = vertices[1].color.a = vertices[2].color.a = vertices[3].color.a = (Uint8) mod_a;

						if (PLATFORM_RENDERER_try_sdl_geometry_textured(draw_texture, vertices, 4, indices, 6, PLATFORM_RENDERER_GEOM_SRC_BOUND_QUAD_CUSTOM))
						{
							PLATFORM_RENDERER_record_sdl_source_bounds_rect(PLATFORM_RENDERER_GEOM_SRC_BOUND_QUAD_CUSTOM, &dst_rect);
						}
					}
				}
				else
				{
					int indices[6] = { 0, 1, 2, 0, 2, 3 };
					(void) PLATFORM_RENDERER_try_sdl_geometry_textured(draw_texture, vertices, 4, indices, 6, PLATFORM_RENDERER_GEOM_SRC_BOUND_QUAD_CUSTOM);
				}
			}
		}
#endif
	}
#endif
}

void PLATFORM_RENDERER_draw_point(float x, float y)
{
	if (PLATFORM_RENDERER_should_use_gl())
	{
		glBegin(GL_POINTS);
			glVertex2f(x, y);
		glEnd();
	}
#ifdef WIZBALL_USE_SDL2
	if (PLATFORM_RENDERER_is_sdl2_native_sprite_enabled() && PLATFORM_RENDERER_is_sdl2_stub_ready() && (platform_renderer_present_height > 0))
	{
		float tx;
		float ty;
		PLATFORM_RENDERER_transform_point(x, y, &tx, &ty);
		PLATFORM_RENDERER_apply_sdl_draw_blend_mode();
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
	if (PLATFORM_RENDERER_should_use_gl())
	{
		glBegin(GL_POINTS);
			glColor3f(r, g, b);
			glVertex2f(x, y);
		glEnd();
	}
#ifdef WIZBALL_USE_SDL2
	if (PLATFORM_RENDERER_is_sdl2_native_sprite_enabled() && PLATFORM_RENDERER_is_sdl2_stub_ready() && (platform_renderer_present_height > 0))
	{
		float tx;
		float ty;
		PLATFORM_RENDERER_transform_point(x, y, &tx, &ty);
		PLATFORM_RENDERER_apply_sdl_draw_blend_mode();
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
	if (PLATFORM_RENDERER_should_use_gl())
	{
		glBegin(GL_LINES);
			glVertex2f(x1, y1);
			glVertex2f(x2, y2);
		glEnd();
	}
#ifdef WIZBALL_USE_SDL2
	if (PLATFORM_RENDERER_is_sdl2_native_sprite_enabled() && PLATFORM_RENDERER_is_sdl2_stub_ready() && (platform_renderer_present_height > 0))
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
		if (SDL_RenderDrawLine(platform_renderer_sdl_renderer, (int) tx1, (int) ((float) platform_renderer_present_height - ty1), (int) tx2, (int) ((float) platform_renderer_present_height - ty2)) == 0)
		{
			platform_renderer_sdl_native_draw_count++;
		}
	}
#endif
}

void PLATFORM_RENDERER_draw_coloured_line(float x1, float y1, float x2, float y2, float r, float g, float b)
{
	if (PLATFORM_RENDERER_should_use_gl())
	{
		glBegin(GL_LINES);
			glColor3f(r, g, b);
			glVertex2f(x1, y1);
			glVertex2f(x2, y2);
		glEnd();
	}
#ifdef WIZBALL_USE_SDL2
	if (PLATFORM_RENDERER_is_sdl2_native_sprite_enabled() && PLATFORM_RENDERER_is_sdl2_stub_ready() && (platform_renderer_present_height > 0))
	{
		float tx1;
		float ty1;
		float tx2;
		float ty2;
		PLATFORM_RENDERER_transform_point(x1, y1, &tx1, &ty1);
		PLATFORM_RENDERER_transform_point(x2, y2, &tx2, &ty2);
		PLATFORM_RENDERER_apply_sdl_draw_blend_mode();
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

	if (PLATFORM_RENDERER_should_use_gl())
	{
		glBegin(GL_LINE_LOOP);
		for (i = 0; i < count; i++)
		{
			glVertex2f(x[i], y[i]);
		}
		glEnd();
	}
#ifdef WIZBALL_USE_SDL2
	if (PLATFORM_RENDERER_is_sdl2_native_sprite_enabled() && PLATFORM_RENDERER_is_sdl2_stub_ready() && (platform_renderer_present_height > 0))
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
	if (!isfinite(left_window_transform_x) ||
		!isfinite(top_window_transform_y) ||
		!isfinite(total_scale_x) ||
		!isfinite(total_scale_y))
	{
		PLATFORM_RENDERER_reset_transform_state("set_window_transform input");
		return;
	}

	if (PLATFORM_RENDERER_should_use_gl())
	{
		glLoadIdentity();
		glTranslatef(left_window_transform_x, top_window_transform_y, 0.0f);
		glScalef(total_scale_x, total_scale_y, 1.0f);
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
	if (PLATFORM_RENDERER_should_use_gl())
	{
		glColor3f(r, g, b);
	}
	platform_renderer_current_colour_r = r;
	platform_renderer_current_colour_g = g;
	platform_renderer_current_colour_b = b;
	platform_renderer_current_colour_a = 1.0f;
}

void PLATFORM_RENDERER_set_colour4f(float r, float g, float b, float a)
{
	if (PLATFORM_RENDERER_should_use_gl())
	{
		glColor4f(r, g, b, a);
	}
	platform_renderer_current_colour_r = r;
	platform_renderer_current_colour_g = g;
	platform_renderer_current_colour_b = b;
	platform_renderer_current_colour_a = a;
}

void PLATFORM_RENDERER_set_texture_enabled(bool enabled)
{
	GLenum gl_err;
	if (!PLATFORM_RENDERER_should_use_gl())
	{
		return;
	}
	if (!platform_renderer_gl_texture_enable_supported)
	{
		return;
	}
	if (!platform_renderer_gl_diag_trace_checked_env)
	{
		platform_renderer_gl_diag_trace_enabled = PLATFORM_RENDERER_env_enabled("WIZBALL_GL_DIAG_TRACE");
		platform_renderer_gl_diag_trace_checked_env = true;
	}
	if (platform_renderer_gl_diag_trace_enabled)
	{
		/* Stage attribution: discard prior-frame errors before this call. */
		while (glGetError() != GL_NO_ERROR)
		{
		}
	}
	if (enabled)
	{
		glEnable(GL_TEXTURE_2D);
		PLATFORM_RENDERER_gl_diag_check_stage("texture_enable");
	}
	else
	{
		glDisable(GL_TEXTURE_2D);
		PLATFORM_RENDERER_gl_diag_check_stage("texture_disable");
	}
	gl_err = glGetError();
	if (gl_err == GL_INVALID_ENUM)
	{
		platform_renderer_gl_texture_enable_supported = false;
		if (!platform_renderer_gl_texture_enable_warned)
		{
			fprintf(stderr, "[GL-DIAG] disabling GL_TEXTURE_2D toggles after GL_INVALID_ENUM\n");
			platform_renderer_gl_texture_enable_warned = true;
		}
	}
	else if (gl_err != GL_NO_ERROR)
	{
		/* Preserve previous behavior: leave non-enum errors for frame summary checks. */
	}
}

void PLATFORM_RENDERER_set_colour_sum_enabled(bool enabled)
{
	if (!PLATFORM_RENDERER_should_use_gl())
	{
		return;
	}
	if (enabled)
	{
		glEnable(GL_COLOR_SUM);
		PLATFORM_RENDERER_gl_diag_check_stage("colour_sum_enable");
	}
	else
	{
		glDisable(GL_COLOR_SUM);
		PLATFORM_RENDERER_gl_diag_check_stage("colour_sum_disable");
	}
}

void PLATFORM_RENDERER_bind_texture(unsigned int texture_handle)
{
	unsigned int resolved_texture_id = PLATFORM_RENDERER_resolve_gl_texture(texture_handle);
	unsigned int normalized_texture_handle = PLATFORM_RENDERER_normalize_texture_handle(texture_handle);
	if (PLATFORM_RENDERER_should_use_gl())
	{
		glBindTexture(GL_TEXTURE_2D, (GLuint) resolved_texture_id);
	}
	platform_renderer_last_bound_texture_handle = normalized_texture_handle;
}

void PLATFORM_RENDERER_set_texture_filter(bool linear)
{
	if (!PLATFORM_RENDERER_should_use_gl())
	{
		return;
	}
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
	if (!PLATFORM_RENDERER_should_use_gl())
	{
		return;
	}
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
	if (!PLATFORM_RENDERER_should_use_gl())
	{
		return false;
	}
	if (platform_renderer_active_texture_proc == NULL)
	{
		return false;
	}

	platform_renderer_active_texture_proc((GLint) texture_unit);
	PLATFORM_RENDERER_gl_diag_check_stage((texture_unit == GL_TEXTURE1) ? "active_texture_1" : "active_texture_0");
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
	if (!PLATFORM_RENDERER_should_use_gl())
	{
		return false;
	}
	if (platform_renderer_secondary_colour_proc == NULL)
	{
		return false;
	}

	platform_renderer_secondary_colour_proc((GLfloat) r, (GLfloat) g, (GLfloat) b);
	return true;
}

void PLATFORM_RENDERER_set_blend_enabled(bool enabled)
{
	platform_renderer_blend_enabled = enabled;
	if (!PLATFORM_RENDERER_should_use_gl())
	{
		return;
	}
	if (enabled)
	{
		glEnable(GL_BLEND);
		PLATFORM_RENDERER_gl_diag_check_stage("blend_enable");
	}
	else
	{
		glDisable(GL_BLEND);
		PLATFORM_RENDERER_gl_diag_check_stage("blend_disable");
	}
}

void PLATFORM_RENDERER_set_blend_func(int src_factor, int dst_factor)
{
	if (!PLATFORM_RENDERER_should_use_gl())
	{
		return;
	}
	glBlendFunc((GLenum) src_factor, (GLenum) dst_factor);
	PLATFORM_RENDERER_gl_diag_check_stage("blend_func");
}

void PLATFORM_RENDERER_set_blend_mode_additive(void)
{
	PLATFORM_RENDERER_set_blend_func(GL_SRC_ALPHA, GL_ONE);
	platform_renderer_blend_mode = PLATFORM_RENDERER_BLEND_MODE_ADD;
}

void PLATFORM_RENDERER_set_blend_mode_alpha(void)
{
	PLATFORM_RENDERER_set_blend_func(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	platform_renderer_blend_mode = PLATFORM_RENDERER_BLEND_MODE_ALPHA;
}

void PLATFORM_RENDERER_set_blend_mode_subtractive(void)
{
	PLATFORM_RENDERER_set_blend_func(GL_ZERO, GL_ONE_MINUS_SRC_COLOR);
	platform_renderer_blend_mode = PLATFORM_RENDERER_BLEND_MODE_SUBTRACT;
}

void PLATFORM_RENDERER_set_alpha_test_enabled(bool enabled)
{
	if (!PLATFORM_RENDERER_should_use_gl())
	{
		return;
	}
	if (enabled)
	{
		glEnable(GL_ALPHA_TEST);
		PLATFORM_RENDERER_gl_diag_check_stage("alpha_test_enable");
	}
	else
	{
		glDisable(GL_ALPHA_TEST);
		PLATFORM_RENDERER_gl_diag_check_stage("alpha_test_disable");
	}
}

void PLATFORM_RENDERER_set_alpha_func_greater(float threshold)
{
	if (!PLATFORM_RENDERER_should_use_gl())
	{
		return;
	}
	glAlphaFunc(GL_GREATER, threshold);
	PLATFORM_RENDERER_gl_diag_check_stage("alpha_func_greater");
}

void PLATFORM_RENDERER_set_scissor_rect(int x, int y, int width, int height)
{
	if (!PLATFORM_RENDERER_should_use_gl())
	{
		return;
	}
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

#ifdef WIZBALL_USE_SDL2
	if ((platform_renderer_sdl_renderer != NULL) && (platform_renderer_present_height > 0))
	{
		/* SDL clip rect uses top-left origin; transform from GL-style bottom-left space. */
		SDL_Rect clip;
		int clip_x = (int) x;
		int clip_w = (int) width;
		int clip_h = (int) height;
		int clip_y = platform_renderer_present_height - ((int) y + clip_h);

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
		if (clip_w < 0) clip_w = 0;
		if (clip_h < 0) clip_h = 0;

		clip.x = clip_x;
		clip.y = clip_y;
		clip.w = clip_w;
		clip.h = clip_h;
		(void) SDL_RenderSetClipRect(platform_renderer_sdl_renderer, &clip);
	}
#endif
}

void PLATFORM_RENDERER_translatef(float x, float y, float z)
{
	if (!isfinite(x) || !isfinite(y) || !isfinite(z))
	{
		PLATFORM_RENDERER_reset_transform_state("translatef input");
		return;
	}
	if (PLATFORM_RENDERER_should_use_gl())
	{
		glTranslatef(x, y, z);
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
	if (PLATFORM_RENDERER_should_use_gl())
	{
		glScalef(x, y, z);
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
	if (PLATFORM_RENDERER_should_use_gl())
	{
		glRotatef(angle_degrees, x, y, z);
	}
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
	if (!PLATFORM_RENDERER_transform_state_is_finite())
	{
		PLATFORM_RENDERER_reset_transform_state("rotatef accum");
	}
}

void PLATFORM_RENDERER_set_combiner_modulate_primary(void)
{
	if (!PLATFORM_RENDERER_should_use_gl())
	{
		return;
	}
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE);
	glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB, GL_MODULATE);
	glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_RGB, GL_TEXTURE);
	glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE1_RGB, GL_PRIMARY_COLOR);
	glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_ALPHA, GL_MODULATE);
	glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_ALPHA, GL_TEXTURE);
	glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE1_ALPHA, GL_PRIMARY_COLOR);
	PLATFORM_RENDERER_gl_diag_check_stage("combiner_modulate_primary");
}

void PLATFORM_RENDERER_set_combiner_replace_rgb_modulate_alpha_previous(void)
{
	if (!PLATFORM_RENDERER_should_use_gl())
	{
		return;
	}
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE);
	glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB, GL_REPLACE);
	glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_RGB, GL_PREVIOUS);
	glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_ALPHA, GL_MODULATE);
	glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_ALPHA, GL_TEXTURE);
	glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE1_ALPHA, GL_PREVIOUS);
	PLATFORM_RENDERER_gl_diag_check_stage("combiner_replace_prev_alpha_prev");
}

void PLATFORM_RENDERER_set_combiner_replace_rgb_modulate_alpha_primary(void)
{
	if (!PLATFORM_RENDERER_should_use_gl())
	{
		return;
	}
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE);
	glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB, GL_REPLACE);
	glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_RGB, GL_PREVIOUS);
	glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_ALPHA, GL_MODULATE);
	glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_ALPHA, GL_TEXTURE);
	glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE1_ALPHA, GL_PRIMARY_COLOR);
	PLATFORM_RENDERER_gl_diag_check_stage("combiner_replace_prev_alpha_primary");
}

void PLATFORM_RENDERER_set_combiner_modulate_rgb_replace_alpha_previous(void)
{
	if (!PLATFORM_RENDERER_should_use_gl())
	{
		return;
	}
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE);
	glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB, GL_MODULATE);
	glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_RGB, GL_TEXTURE);
	glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE1_RGB, GL_PRIMARY_COLOR);
	glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_ALPHA, GL_REPLACE);
	glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_ALPHA, GL_PREVIOUS);
	PLATFORM_RENDERER_gl_diag_check_stage("combiner_modulate_alpha_prev");
}

void PLATFORM_RENDERER_bind_secondary_texture(unsigned int texture_handle, bool enable_second_unit_texturing)
{
	if (!PLATFORM_RENDERER_should_use_gl())
	{
		return;
	}
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
	if (!PLATFORM_RENDERER_should_use_gl())
	{
		return;
	}
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
	if (!PLATFORM_RENDERER_should_use_gl())
	{
		return;
	}
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
#ifdef WIZBALL_USE_SDL2
	if (platform_renderer_sdl_renderer != NULL)
	{
		(void) SDL_RenderSetClipRect(platform_renderer_sdl_renderer, NULL);
	}
#endif
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

	if (PLATFORM_RENDERER_should_use_gl())
	{
		PLATFORM_RENDERER_note_gl_draw_source(PLATFORM_RENDERER_GEOM_SRC_MULTITEXTURE_ARRAY, true);
		glBegin(GL_QUADS);
		for (i = 0; i < 4; i++)
		{
			glMultiTexCoord2f(GL_TEXTURE0, u0[i], v0[i]);
			glMultiTexCoord2f(GL_TEXTURE1, u1[i], v1[i]);
			glVertex2f(x[i], y[i]);
		}
		glEnd();
	}
#ifdef WIZBALL_USE_SDL2
	if (PLATFORM_RENDERER_is_sdl2_native_sprite_enabled() && PLATFORM_RENDERER_is_sdl2_stub_ready() && (platform_renderer_present_height > 0))
	{
#if SDL_VERSION_ATLEAST(2, 0, 18)
		platform_renderer_texture_entry *entry = PLATFORM_RENDERER_get_last_bound_texture_entry();
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
				src_flip_y = PLATFORM_RENDERER_src_flip_y_for_sdl(v0[0], v0[1]);
				is_axis_aligned =
					(fabsf(txs[0] - txs[1]) < axis_eps) &&
					(fabsf(txs[2] - txs[3]) < axis_eps) &&
					(fabsf(tys[0] - tys[3]) < axis_eps) &&
					(fabsf(tys[1] - tys[2]) < axis_eps);

				{
					const int indices[6] = { 0, 1, 2, 0, 2, 3 };
					(void) is_axis_aligned;
					(void) src_flip_x;
					(void) src_flip_y;
					(void) PLATFORM_RENDERER_try_sdl_geometry_textured(entry->sdl_texture, vertices, 4, indices, 6, PLATFORM_RENDERER_GEOM_SRC_PERSPECTIVE);
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

					PLATFORM_RENDERER_apply_sdl_texture_blend_mode(entry->sdl_texture);
					(void) SDL_SetTextureColorMod(
						entry->sdl_texture,
						(Uint8) (platform_renderer_current_colour_r * 255.0f),
						(Uint8) (platform_renderer_current_colour_g * 255.0f),
						(Uint8) (platform_renderer_current_colour_b * 255.0f));
					(void) SDL_SetTextureAlphaMod(entry->sdl_texture, PLATFORM_RENDERER_get_sdl_texture_alpha_mod((Uint8) (platform_renderer_current_colour_a * 255.0f)));

					if (SDL_RenderCopyEx(platform_renderer_sdl_renderer, entry->sdl_texture, &src_rect, &dst_rect, 0.0, NULL, final_flip) == 0)
					{
						platform_renderer_sdl_native_draw_count++;
						platform_renderer_sdl_native_textured_draw_count++;
						PLATFORM_RENDERER_note_sdl_draw_source(PLATFORM_RENDERER_GEOM_SRC_MULTITEXTURE_ARRAY);
						PLATFORM_RENDERER_record_sdl_source_bounds_rect(PLATFORM_RENDERER_GEOM_SRC_MULTITEXTURE_ARRAY, &dst_rect);
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

	if (PLATFORM_RENDERER_should_use_gl())
	{
		PLATFORM_RENDERER_note_gl_draw_source(PLATFORM_RENDERER_GEOM_SRC_COLOURED_ARRAY, true);
		glBegin(GL_QUADS);
		for (i = 0; i < 4; i++)
		{
			glTexCoord2f(u[i], v[i]);
			glColor4f(r[i], g[i], b[i], a[i]);
			glVertex2f(x[i], y[i]);
		}
		glEnd();
	}
#ifdef WIZBALL_USE_SDL2
	if (PLATFORM_RENDERER_is_sdl2_native_sprite_enabled() && PLATFORM_RENDERER_is_sdl2_stub_ready() && (platform_renderer_present_height > 0))
	{
#if SDL_VERSION_ATLEAST(2, 0, 18)
		platform_renderer_texture_entry *entry = PLATFORM_RENDERER_get_last_bound_texture_entry();
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
				src_flip_y = PLATFORM_RENDERER_src_flip_y_for_sdl(v[0], v[1]);
				is_axis_aligned =
					(fabsf(txs[0] - txs[1]) < axis_eps) &&
					(fabsf(txs[2] - txs[3]) < axis_eps) &&
					(fabsf(tys[0] - tys[3]) < axis_eps) &&
					(fabsf(tys[1] - tys[2]) < axis_eps);

				{
					const int indices[6] = { 0, 1, 2, 0, 2, 3 };
					(void) is_axis_aligned;
					(void) src_flip_x;
					(void) src_flip_y;
					(void) PLATFORM_RENDERER_try_sdl_geometry_textured(entry->sdl_texture, vertices, 4, indices, 6, PLATFORM_RENDERER_GEOM_SRC_COLOURED_PERSPECTIVE);
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

					PLATFORM_RENDERER_apply_sdl_texture_blend_mode(entry->sdl_texture);
					(void) SDL_SetTextureColorMod(
						entry->sdl_texture,
						(Uint8) (r[0] * 255.0f),
						(Uint8) (g[0] * 255.0f),
						(Uint8) (b[0] * 255.0f));
					(void) SDL_SetTextureAlphaMod(entry->sdl_texture, PLATFORM_RENDERER_get_sdl_texture_alpha_mod((Uint8) (a[0] * 255.0f)));

					if (SDL_RenderCopyEx(platform_renderer_sdl_renderer, entry->sdl_texture, &src_rect, &dst_rect, 0.0, NULL, final_flip) == 0)
					{
						platform_renderer_sdl_native_draw_count++;
						platform_renderer_sdl_native_textured_draw_count++;
						PLATFORM_RENDERER_note_sdl_draw_source(PLATFORM_RENDERER_GEOM_SRC_COLOURED_ARRAY);
						PLATFORM_RENDERER_record_sdl_source_bounds_rect(PLATFORM_RENDERER_GEOM_SRC_COLOURED_ARRAY, &dst_rect);
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
	if (PLATFORM_RENDERER_should_use_gl())
	{
		PLATFORM_RENDERER_note_gl_draw_source(PLATFORM_RENDERER_GEOM_SRC_PERSPECTIVE, true);
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
#ifdef WIZBALL_USE_SDL2
	if (PLATFORM_RENDERER_is_sdl2_native_sprite_enabled() && PLATFORM_RENDERER_is_sdl2_stub_ready() && (platform_renderer_present_height > 0))
	{
#if SDL_VERSION_ATLEAST(2, 0, 18)
		platform_renderer_texture_entry *entry = PLATFORM_RENDERER_get_last_bound_texture_entry();
		if (entry != NULL)
		{
			if (PLATFORM_RENDERER_build_sdl_texture_from_entry(entry))
			{
				const float perspective_eps = 0.0005f;
				if ((fabsf(q - 1.0f) > perspective_eps) && (q > perspective_eps))
				{
					bool strip_ok = true;
					const int strip_count = 24;
					const int indices[6] = { 0, 1, 2, 0, 2, 3 };
					int strip_index;

					for (strip_index = 0; strip_index < strip_count; strip_index++)
					{
						const float t0 = (float) strip_index / (float) strip_count;
						const float t1 = (float) (strip_index + 1) / (float) strip_count;
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
						SDL_Vertex strip_vertices[4];

						if ((fabsf(w0) <= perspective_eps) || (fabsf(w1) <= perspective_eps))
						{
							strip_ok = false;
							break;
						}

						strip_v0 = ((v1 * q * one_minus_t0) + (v2 * t0)) / w0;
						strip_v1 = ((v1 * q * one_minus_t1) + (v2 * t1)) / w1;

						PLATFORM_RENDERER_transform_point(left_x0, left_y0, &tx, &ty);
						strip_vertices[0].position.x = tx;
						strip_vertices[0].position.y = (float) platform_renderer_present_height - ty;
						strip_vertices[0].tex_coord.x = u1;
						strip_vertices[0].tex_coord.y = 1.0f - strip_v0;
						strip_vertices[0].color.r = (Uint8) (platform_renderer_current_colour_r * 255.0f);
						strip_vertices[0].color.g = (Uint8) (platform_renderer_current_colour_g * 255.0f);
						strip_vertices[0].color.b = (Uint8) (platform_renderer_current_colour_b * 255.0f);
						strip_vertices[0].color.a = (Uint8) (platform_renderer_current_colour_a * 255.0f);

						PLATFORM_RENDERER_transform_point(left_x1, left_y1, &tx, &ty);
						strip_vertices[1].position.x = tx;
						strip_vertices[1].position.y = (float) platform_renderer_present_height - ty;
						strip_vertices[1].tex_coord.x = u1;
						strip_vertices[1].tex_coord.y = 1.0f - strip_v1;
						strip_vertices[1].color.r = strip_vertices[0].color.r;
						strip_vertices[1].color.g = strip_vertices[0].color.g;
						strip_vertices[1].color.b = strip_vertices[0].color.b;
						strip_vertices[1].color.a = strip_vertices[0].color.a;

						PLATFORM_RENDERER_transform_point(right_x1, right_y1, &tx, &ty);
						strip_vertices[2].position.x = tx;
						strip_vertices[2].position.y = (float) platform_renderer_present_height - ty;
						strip_vertices[2].tex_coord.x = u2;
						strip_vertices[2].tex_coord.y = 1.0f - strip_v1;
						strip_vertices[2].color.r = strip_vertices[0].color.r;
						strip_vertices[2].color.g = strip_vertices[0].color.g;
						strip_vertices[2].color.b = strip_vertices[0].color.b;
						strip_vertices[2].color.a = strip_vertices[0].color.a;

						PLATFORM_RENDERER_transform_point(right_x0, right_y0, &tx, &ty);
						strip_vertices[3].position.x = tx;
						strip_vertices[3].position.y = (float) platform_renderer_present_height - ty;
						strip_vertices[3].tex_coord.x = u2;
						strip_vertices[3].tex_coord.y = 1.0f - strip_v0;
						strip_vertices[3].color.r = strip_vertices[0].color.r;
						strip_vertices[3].color.g = strip_vertices[0].color.g;
						strip_vertices[3].color.b = strip_vertices[0].color.b;
						strip_vertices[3].color.a = strip_vertices[0].color.a;

						if (!PLATFORM_RENDERER_try_sdl_geometry_textured(entry->sdl_texture, strip_vertices, 4, indices, 6, PLATFORM_RENDERER_GEOM_SRC_PERSPECTIVE))
						{
							strip_ok = false;
							break;
						}
					}

					if (strip_ok)
					{
						return;
					}
				}

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
					vertices[i].position.y = (float) platform_renderer_present_height - ty;
					vertices[i].tex_coord.x = tu[i];
					vertices[i].tex_coord.y = tv[i];
					vertices[i].color.r = (Uint8) (platform_renderer_current_colour_r * 255.0f);
					vertices[i].color.g = (Uint8) (platform_renderer_current_colour_g * 255.0f);
					vertices[i].color.b = (Uint8) (platform_renderer_current_colour_b * 255.0f);
					vertices[i].color.a = (Uint8) (platform_renderer_current_colour_a * 255.0f);
				}

				/*
				 * Prefer true geometry for perspective quads even when they look
				 * axis-aligned. Rectangle approximations can introduce visible
				 * stretch during parallax/scrolling.
				 */
				{
					const int indices[6] = { 0, 1, 2, 0, 2, 3 };
					if (PLATFORM_RENDERER_try_sdl_geometry_textured(entry->sdl_texture, vertices, 4, indices, 6, PLATFORM_RENDERER_GEOM_SRC_PERSPECTIVE))
					{
						return;
					}
					if (platform_renderer_sdl_native_primary_enabled && platform_renderer_sdl_native_primary_strict_enabled)
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

					if (src_flip_x ^ geom_flip_x)
					{
						final_flip = (SDL_RendererFlip) (final_flip | SDL_FLIP_HORIZONTAL);
					}
					if (src_flip_y ^ geom_flip_y)
					{
						final_flip = (SDL_RendererFlip) (final_flip | SDL_FLIP_VERTICAL);
					}

					PLATFORM_RENDERER_apply_sdl_texture_blend_mode(entry->sdl_texture);
					(void) SDL_SetTextureColorMod(
						entry->sdl_texture,
						(Uint8) (platform_renderer_current_colour_r * 255.0f),
						(Uint8) (platform_renderer_current_colour_g * 255.0f),
						(Uint8) (platform_renderer_current_colour_b * 255.0f));
					(void) SDL_SetTextureAlphaMod(entry->sdl_texture, PLATFORM_RENDERER_get_sdl_texture_alpha_mod((Uint8) (platform_renderer_current_colour_a * 255.0f)));

					if (SDL_RenderCopyEx(platform_renderer_sdl_renderer, entry->sdl_texture, &src_rect, &dst_rect, 0.0, NULL, final_flip) == 0)
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

							PLATFORM_RENDERER_apply_sdl_texture_blend_mode(entry->sdl_texture);
					(void) SDL_SetTextureColorMod(
						entry->sdl_texture,
								(Uint8) (platform_renderer_current_colour_r * 255.0f),
								(Uint8) (platform_renderer_current_colour_g * 255.0f),
								(Uint8) (platform_renderer_current_colour_b * 255.0f));
							(void) SDL_SetTextureAlphaMod(entry->sdl_texture, PLATFORM_RENDERER_get_sdl_texture_alpha_mod((Uint8) (platform_renderer_current_colour_a * 255.0f)));

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

	if (PLATFORM_RENDERER_should_use_gl())
	{
		PLATFORM_RENDERER_note_gl_draw_source(PLATFORM_RENDERER_GEOM_SRC_COLOURED_PERSPECTIVE, true);
		PLATFORM_RENDERER_record_gl_src6_bounds_and_q(x0, y0, x1, y1, x2, y2, x3, y3, q);
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
#ifdef WIZBALL_USE_SDL2
	if (PLATFORM_RENDERER_is_sdl2_native_sprite_enabled() && PLATFORM_RENDERER_is_sdl2_stub_ready() && (platform_renderer_present_height > 0))
	{
#if SDL_VERSION_ATLEAST(2, 0, 18)
		platform_renderer_texture_entry *entry = PLATFORM_RENDERER_get_last_bound_texture_entry();
		if (entry != NULL)
		{
			if (PLATFORM_RENDERER_build_sdl_texture_from_entry(entry))
			{
				const float perspective_eps = 0.0005f;
				if ((fabsf(q - 1.0f) > perspective_eps) && (q > perspective_eps))
				{
					bool strip_ok = true;
					const int strip_count = 24;
					const int indices[6] = { 0, 1, 2, 0, 2, 3 };
					int strip_index;

					for (strip_index = 0; strip_index < strip_count; strip_index++)
					{
						const float t0 = (float) strip_index / (float) strip_count;
						const float t1 = (float) (strip_index + 1) / (float) strip_count;
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
						SDL_Vertex strip_vertices[4];
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
						strip_vertices[0].position.x = tx;
						strip_vertices[0].position.y = (float) platform_renderer_present_height - ty;
						strip_vertices[0].tex_coord.x = u1;
						strip_vertices[0].tex_coord.y = 1.0f - strip_v0;
						cr = (int) (((r[0] * one_minus_t0) + (r[1] * t0)) * 255.0f);
						cg = (int) (((g[0] * one_minus_t0) + (g[1] * t0)) * 255.0f);
						cb = (int) (((b[0] * one_minus_t0) + (b[1] * t0)) * 255.0f);
						ca = (int) (((a[0] * one_minus_t0) + (a[1] * t0)) * 255.0f);
						if (cr < 0) cr = 0; else if (cr > 255) cr = 255;
						if (cg < 0) cg = 0; else if (cg > 255) cg = 255;
						if (cb < 0) cb = 0; else if (cb > 255) cb = 255;
						if (ca < 0) ca = 0; else if (ca > 255) ca = 255;
						strip_vertices[0].color.r = (Uint8) cr;
						strip_vertices[0].color.g = (Uint8) cg;
						strip_vertices[0].color.b = (Uint8) cb;
						strip_vertices[0].color.a = (Uint8) ca;

						PLATFORM_RENDERER_transform_point(left_x1, left_y1, &tx, &ty);
						strip_vertices[1].position.x = tx;
						strip_vertices[1].position.y = (float) platform_renderer_present_height - ty;
						strip_vertices[1].tex_coord.x = u1;
						strip_vertices[1].tex_coord.y = 1.0f - strip_v1;
						cr = (int) (((r[0] * one_minus_t1) + (r[1] * t1)) * 255.0f);
						cg = (int) (((g[0] * one_minus_t1) + (g[1] * t1)) * 255.0f);
						cb = (int) (((b[0] * one_minus_t1) + (b[1] * t1)) * 255.0f);
						ca = (int) (((a[0] * one_minus_t1) + (a[1] * t1)) * 255.0f);
						if (cr < 0) cr = 0; else if (cr > 255) cr = 255;
						if (cg < 0) cg = 0; else if (cg > 255) cg = 255;
						if (cb < 0) cb = 0; else if (cb > 255) cb = 255;
						if (ca < 0) ca = 0; else if (ca > 255) ca = 255;
						strip_vertices[1].color.r = (Uint8) cr;
						strip_vertices[1].color.g = (Uint8) cg;
						strip_vertices[1].color.b = (Uint8) cb;
						strip_vertices[1].color.a = (Uint8) ca;

						PLATFORM_RENDERER_transform_point(right_x1, right_y1, &tx, &ty);
						strip_vertices[2].position.x = tx;
						strip_vertices[2].position.y = (float) platform_renderer_present_height - ty;
						strip_vertices[2].tex_coord.x = u2;
						strip_vertices[2].tex_coord.y = 1.0f - strip_v1;
						cr = (int) (((r[3] * one_minus_t1) + (r[2] * t1)) * 255.0f);
						cg = (int) (((g[3] * one_minus_t1) + (g[2] * t1)) * 255.0f);
						cb = (int) (((b[3] * one_minus_t1) + (b[2] * t1)) * 255.0f);
						ca = (int) (((a[3] * one_minus_t1) + (a[2] * t1)) * 255.0f);
						if (cr < 0) cr = 0; else if (cr > 255) cr = 255;
						if (cg < 0) cg = 0; else if (cg > 255) cg = 255;
						if (cb < 0) cb = 0; else if (cb > 255) cb = 255;
						if (ca < 0) ca = 0; else if (ca > 255) ca = 255;
						strip_vertices[2].color.r = (Uint8) cr;
						strip_vertices[2].color.g = (Uint8) cg;
						strip_vertices[2].color.b = (Uint8) cb;
						strip_vertices[2].color.a = (Uint8) ca;

						PLATFORM_RENDERER_transform_point(right_x0, right_y0, &tx, &ty);
						strip_vertices[3].position.x = tx;
						strip_vertices[3].position.y = (float) platform_renderer_present_height - ty;
						strip_vertices[3].tex_coord.x = u2;
						strip_vertices[3].tex_coord.y = 1.0f - strip_v0;
						cr = (int) (((r[3] * one_minus_t0) + (r[2] * t0)) * 255.0f);
						cg = (int) (((g[3] * one_minus_t0) + (g[2] * t0)) * 255.0f);
						cb = (int) (((b[3] * one_minus_t0) + (b[2] * t0)) * 255.0f);
						ca = (int) (((a[3] * one_minus_t0) + (a[2] * t0)) * 255.0f);
						if (cr < 0) cr = 0; else if (cr > 255) cr = 255;
						if (cg < 0) cg = 0; else if (cg > 255) cg = 255;
						if (cb < 0) cb = 0; else if (cb > 255) cb = 255;
						if (ca < 0) ca = 0; else if (ca > 255) ca = 255;
						strip_vertices[3].color.r = (Uint8) cr;
						strip_vertices[3].color.g = (Uint8) cg;
						strip_vertices[3].color.b = (Uint8) cb;
						strip_vertices[3].color.a = (Uint8) ca;

						if (!PLATFORM_RENDERER_try_sdl_geometry_textured(entry->sdl_texture, strip_vertices, 4, indices, 6, PLATFORM_RENDERER_GEOM_SRC_COLOURED_PERSPECTIVE))
						{
							strip_ok = false;
							break;
						}
					}

					if (strip_ok)
					{
						return;
					}
				}

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
				bool src_flip_y = PLATFORM_RENDERER_src_flip_y_for_sdl(v1, v2);
				bool geom_flip_x = false;
				bool geom_flip_y = false;

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

				/*
				 * Prefer true geometry for perspective+colour quads even when the
				 * quad is axis-aligned. This avoids copy/rect approximation stretch
				 * artifacts during background scrolling.
				 */
				{
					const int indices[6] = { 0, 1, 2, 0, 2, 3 };
					if (PLATFORM_RENDERER_try_sdl_geometry_textured(entry->sdl_texture, vertices, 4, indices, 6, PLATFORM_RENDERER_GEOM_SRC_COLOURED_PERSPECTIVE))
					{
						return;
					}
					if (platform_renderer_sdl_native_primary_enabled && platform_renderer_sdl_native_primary_strict_enabled)
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

					if (src_flip_x ^ geom_flip_x)
					{
						final_flip = (SDL_RendererFlip) (final_flip | SDL_FLIP_HORIZONTAL);
					}
					if (src_flip_y ^ geom_flip_y)
					{
						final_flip = (SDL_RendererFlip) (final_flip | SDL_FLIP_VERTICAL);
					}

					PLATFORM_RENDERER_apply_sdl_texture_blend_mode(entry->sdl_texture);
					(void) SDL_SetTextureColorMod(
						entry->sdl_texture,
						(Uint8) (r[0] * 255.0f),
						(Uint8) (g[0] * 255.0f),
						(Uint8) (b[0] * 255.0f));
					(void) SDL_SetTextureAlphaMod(entry->sdl_texture, PLATFORM_RENDERER_get_sdl_texture_alpha_mod((Uint8) (a[0] * 255.0f)));

					if (SDL_RenderCopyEx(platform_renderer_sdl_renderer, entry->sdl_texture, &src_rect, &dst_rect, 0.0, NULL, final_flip) == 0)
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

							PLATFORM_RENDERER_apply_sdl_texture_blend_mode(entry->sdl_texture);
					(void) SDL_SetTextureColorMod(
						entry->sdl_texture,
								(Uint8) (avg_r * 255.0f),
								(Uint8) (avg_g * 255.0f),
								(Uint8) (avg_b * 255.0f));
							(void) SDL_SetTextureAlphaMod(entry->sdl_texture, PLATFORM_RENDERER_get_sdl_texture_alpha_mod((Uint8) (avg_a * 255.0f)));

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
		platform_renderer_texture_entry *entry = PLATFORM_RENDERER_get_texture_entry(texture_handle);
		if (entry != NULL)
		{
			if (PLATFORM_RENDERER_build_sdl_texture_from_entry(entry))
			{
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
				PLATFORM_RENDERER_transform_point(x0, y0, &tx0, &ty0);
				PLATFORM_RENDERER_transform_point(x1, y1, &tx1, &ty1);
				sdl_y0 = (float) sdl_height - ty0;
				sdl_y1 = (float) sdl_height - ty1;
				dst_left = (tx0 < tx1) ? tx0 : tx1;
				dst_right = (tx0 > tx1) ? tx0 : tx1;
				dst_top = (sdl_y0 < sdl_y1) ? sdl_y0 : sdl_y1;
				dst_bottom = (sdl_y0 > sdl_y1) ? sdl_y0 : sdl_y1;

				if (!PLATFORM_RENDERER_build_safe_sdl_src_rect(entry, u1, v1, u2, v2, &src_rect))
				{
					PLATFORM_RENDERER_diag_log_draw_skip("draw-textured-quad", "src-rect-invalid", texture_handle);
				}
				else
				{
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
					PLATFORM_RENDERER_apply_sdl_texture_blend_mode(entry->sdl_texture);
					(void) SDL_SetTextureColorMod(entry->sdl_texture, (Uint8) r, (Uint8) g, (Uint8) b);
					(void) SDL_SetTextureAlphaMod(entry->sdl_texture, PLATFORM_RENDERER_get_sdl_texture_alpha_mod(255));
					if (SDL_RenderCopy(platform_renderer_sdl_renderer, entry->sdl_texture, &src_rect, &dst_rect) == 0)
					{
						platform_renderer_sdl_native_draw_count++;
						platform_renderer_sdl_native_textured_draw_count++;
					}
					else
					{
						PLATFORM_RENDERER_diag_log("SDL2-DIAG", SDL_GetError());
					}
				}
			}
			else
			{
				PLATFORM_RENDERER_diag_log_draw_skip("draw-textured-quad", "build-sdl-texture-failed", texture_handle);
			}
		}
		else
		{
			PLATFORM_RENDERER_diag_log_draw_skip("draw-textured-quad", "invalid-texture-handle", texture_handle);
		}
	}
#endif
	(void) virtual_screen_height;
	if (PLATFORM_RENDERER_should_use_gl())
	{
		PLATFORM_RENDERER_note_gl_draw(true);
		unsigned int resolved_texture_id = PLATFORM_RENDERER_resolve_gl_texture(texture_handle);
		glBindTexture(GL_TEXTURE_2D, (GLuint) resolved_texture_id);
#ifdef WIZBALL_USE_SDL2
		PLATFORM_RENDERER_gl_diag_check_stage("draw_tq_bind");
#endif
		glColor3f((float) r / 255.0f, (float) g / 255.0f, (float) b / 255.0f);
		glLoadIdentity();
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
#ifdef WIZBALL_USE_SDL2
		PLATFORM_RENDERER_gl_diag_check_stage("draw_tq_texparam_mag");
#endif
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
#ifdef WIZBALL_USE_SDL2
		PLATFORM_RENDERER_gl_diag_check_stage("draw_tq_texparam_min");
#endif
		glEnable(GL_TEXTURE_2D);
#ifdef WIZBALL_USE_SDL2
		PLATFORM_RENDERER_gl_diag_check_stage("draw_tq_enable_tex2d");
#endif

		if (alpha_test)
		{
			glEnable(GL_ALPHA_TEST);
#ifdef WIZBALL_USE_SDL2
			PLATFORM_RENDERER_gl_diag_check_stage("draw_tq_enable_alpha");
#endif
			glAlphaFunc(GL_GREATER, 0.5f);
#ifdef WIZBALL_USE_SDL2
			PLATFORM_RENDERER_gl_diag_check_stage("draw_tq_alpha_func");
#endif
		}

		glTranslatef((float) screen_x, (float) screen_y, 0.0f);
#ifdef WIZBALL_USE_SDL2
		PLATFORM_RENDERER_gl_diag_check_stage("draw_tq_translate");
#endif

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
		PLATFORM_RENDERER_gl_diag_check_stage("draw_tq_end");
#endif

		if (alpha_test)
		{
			glDisable(GL_ALPHA_TEST);
#ifdef WIZBALL_USE_SDL2
			PLATFORM_RENDERER_gl_diag_check_stage("draw_tq_disable_alpha");
#endif
		}

		glDisable(GL_TEXTURE_2D);
#ifdef WIZBALL_USE_SDL2
		PLATFORM_RENDERER_gl_diag_check_stage("draw_textured_quad");
#endif
	}
}

void PLATFORM_RENDERER_draw_sdl_window_sprite(unsigned int texture_handle, int r, int g, int b, float entity_x, float entity_y, float left, float right, float up, float down, float u1, float v1, float u2, float v2, float left_window_transform_x, float top_window_transform_y, float total_scale_x, float total_scale_y, float sprite_scale_x, float sprite_scale_y, float sprite_rotation_degrees, bool sprite_flip_x, bool sprite_flip_y)
{
#ifdef WIZBALL_USE_SDL2
	if (!(PLATFORM_RENDERER_is_sdl2_native_sprite_enabled() && PLATFORM_RENDERER_is_sdl2_stub_ready()))
	{
		PLATFORM_RENDERER_diag_log_draw_skip("draw-sdl-window-sprite", "native-or-stub-disabled", texture_handle);
		return;
	}

	platform_renderer_texture_entry *entry = PLATFORM_RENDERER_get_texture_entry(texture_handle);
	if (entry == NULL)
	{
		PLATFORM_RENDERER_diag_log_draw_skip("draw-sdl-window-sprite", "invalid-texture-handle", texture_handle);
		return;
	}
	if (!PLATFORM_RENDERER_build_sdl_texture_from_entry(entry))
	{
		PLATFORM_RENDERER_diag_log_draw_skip("draw-sdl-window-sprite", "build-sdl-texture-failed", texture_handle);
		return;
	}
	SDL_Texture *draw_texture = PLATFORM_RENDERER_get_effective_texture_for_current_blend(entry);
	if (draw_texture == NULL)
	{
		PLATFORM_RENDERER_diag_log_draw_skip("draw-sdl-window-sprite", "effective-texture-null", texture_handle);
		return;
	}

	if (platform_renderer_present_height <= 0)
	{
		PLATFORM_RENDERER_diag_log_draw_skip("draw-sdl-window-sprite", "present-height-invalid", texture_handle);
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
		PLATFORM_RENDERER_diag_log_draw_skip("draw-sdl-window-sprite", "src-rect-invalid", texture_handle);
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

	{
		/* Prefer geometry for animated/rotated window sprites to match GL transform order. */
		SDL_Vertex vertices[4];
		int indices[6] = { 0, 1, 2, 0, 2, 3 };
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
		const float local_x[4] = { rel_left, rel_left, rel_right, rel_right };
		const float local_y[4] = { rel_up, rel_down, rel_down, rel_up };
		const float uv_x[4] = { u_left, u_left, u_right, u_right };
		const float uv_y[4] = { v_top, v_bottom, v_bottom, v_top };
		int i;
		int native_draw_count_before = platform_renderer_sdl_native_draw_count;

		for (i = 0; i < 4; i++)
		{
			const float x = local_x[i];
			const float y = local_y[i];
			const float rx = (x * c) - (y * s);
			const float ry = (x * s) + (y * c);
			vertices[i].position.x = origin_x_gl + rx;
			vertices[i].position.y = (float) platform_renderer_present_height - (origin_y_gl + ry);
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
				vertices[i].color.r = (Uint8) r;
				vertices[i].color.g = (Uint8) g;
				vertices[i].color.b = (Uint8) b;
			}
			vertices[i].color.a = 255;
		}

			PLATFORM_RENDERER_apply_sdl_texture_blend_mode(draw_texture);
			(void) SDL_SetTextureColorMod(draw_texture, 255, 255, 255);
			(void) SDL_SetTextureAlphaMod(draw_texture, PLATFORM_RENDERER_get_sdl_texture_alpha_mod(255));
			if (!PLATFORM_RENDERER_try_sdl_geometry_textured(draw_texture, vertices, 4, indices, 6, PLATFORM_RENDERER_GEOM_SRC_SDL_CUSTOM))
			{
				if (platform_renderer_sdl_native_primary_enabled && platform_renderer_sdl_native_primary_strict_enabled)
				{
					/* Strict native mode should not regress to copy/copyex textured path. */
				}
				else
				{
				(void) SDL_SetTextureColorMod(draw_texture, (Uint8) r, (Uint8) g, (Uint8) b);
				if (PLATFORM_RENDERER_using_subtractive_mod_fallback())
				{
					(void) SDL_SetTextureColorMod(draw_texture, 255, 255, 255);
				}
				PLATFORM_RENDERER_apply_sdl_texture_blend_mode(draw_texture);
				(void) SDL_SetTextureAlphaMod(draw_texture, PLATFORM_RENDERER_get_sdl_texture_alpha_mod(255));
				if (SDL_RenderCopyEx(platform_renderer_sdl_renderer, draw_texture, &src_rect, &dst_rect, sprite_rotation_degrees, &center, final_flip) == 0)
				{
					platform_renderer_sdl_native_draw_count++;
					platform_renderer_sdl_native_textured_draw_count++;
				}
			}
		}
		if (platform_renderer_sdl_native_draw_count > native_draw_count_before)
		{
			platform_renderer_sdl_window_sprite_draw_count++;
		}
	}

	if (PLATFORM_RENDERER_should_use_gl())
	{
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
		unsigned int resolved_texture_id = PLATFORM_RENDERER_resolve_gl_texture(texture_handle);

		glBindTexture(GL_TEXTURE_2D, (GLuint) resolved_texture_id);
		glColor3f((float) r / 255.0f, (float) g / 255.0f, (float) b / 255.0f);
		glEnable(GL_TEXTURE_2D);
		glLoadIdentity();
		glTranslatef(origin_x_gl, origin_y_gl, 0.0f);
		glRotatef(sprite_rotation_degrees, 0.0f, 0.0f, 1.0f);
		glBegin(GL_QUADS);
			glTexCoord2f(u_left, v_top);
			glVertex2f(rel_left, rel_up);
			glTexCoord2f(u_left, v_bottom);
			glVertex2f(rel_left, rel_down);
			glTexCoord2f(u_right, v_bottom);
			glVertex2f(rel_right, rel_down);
			glTexCoord2f(u_right, v_top);
			glVertex2f(rel_right, rel_up);
		glEnd();
		glDisable(GL_TEXTURE_2D);
		platform_renderer_gl_window_sprite_draw_count++;
#ifdef WIZBALL_USE_SDL2
		PLATFORM_RENDERER_gl_diag_check_stage("draw_sdl_window_sprite");
#endif
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
		platform_renderer_sdl_window_solid_rect_draw_count++;
	}

	if (PLATFORM_RENDERER_should_use_gl())
	{
		glLoadIdentity();
		glDisable(GL_TEXTURE_2D);
		glColor3f((float) r / 255.0f, (float) g / 255.0f, (float) b / 255.0f);
		glBegin(GL_QUADS);
			glVertex2f(dst_left, gl_top);
			glVertex2f(dst_left, gl_bottom);
			glVertex2f(dst_right, gl_bottom);
			glVertex2f(dst_right, gl_top);
		glEnd();
		platform_renderer_gl_window_solid_rect_draw_count++;
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
	{
		int indices[6] = { 0, 1, 2, 0, 2, 3 };
		(void) PLATFORM_RENDERER_try_sdl_geometry_textured(entry->sdl_texture, vertices, 4, indices, 6, PLATFORM_RENDERER_GEOM_SRC_SDL_CUSTOM);
	}
	if (platform_renderer_sdl_native_draw_count > native_draw_count_before)
	{
		platform_renderer_sdl_bound_custom_draw_count++;
	}

	if (PLATFORM_RENDERER_should_use_gl())
	{
		unsigned int resolved_texture_id = PLATFORM_RENDERER_resolve_gl_texture(texture_handle);
		glBindTexture(GL_TEXTURE_2D, (GLuint) resolved_texture_id);
		glEnable(GL_TEXTURE_2D);
		glColor4f(
			platform_renderer_current_colour_r,
			platform_renderer_current_colour_g,
			platform_renderer_current_colour_b,
			platform_renderer_current_colour_a);
		glLoadIdentity();
		glBegin(GL_QUADS);
			glTexCoord2f(u1, v1);
			glVertex2f(px[0], py[0]);
			glTexCoord2f(u1, v2);
			glVertex2f(px[1], py[1]);
			glTexCoord2f(u2, v2);
			glVertex2f(px[2], py[2]);
			glTexCoord2f(u2, v1);
			glVertex2f(px[3], py[3]);
		glEnd();
		glDisable(GL_TEXTURE_2D);
		platform_renderer_gl_bound_custom_draw_count++;
#ifdef WIZBALL_USE_SDL2
		PLATFORM_RENDERER_gl_diag_check_stage("draw_sdl_bound_textured_quad_custom");
#endif
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
	unsigned int gl_texture_id = 0;
	bool build_gl_texture = PLATFORM_RENDERER_should_use_gl();
#ifdef WIZBALL_USE_SDL2
	/*
	 * In safe GL-off mode we keep GL context active initially, but avoid creating
	 * new AllegroGL textures so SDL texture ownership is exercised end-to-end.
	 */
	if (platform_renderer_gl_safe_off_enabled && !platform_renderer_gl_mode_forced_on)
	{
		build_gl_texture = false;
	}
#endif
	if (build_gl_texture)
	{
		gl_texture_id = (unsigned int) allegro_gl_make_masked_texture(image);
	}
	return PLATFORM_RENDERER_register_gl_texture(gl_texture_id, image);
}

static bool PLATFORM_RENDERER_ensure_compare_buffers(int width, int height)
{
	size_t bytes;

	if ((width <= 0) || (height <= 0))
	{
		return false;
	}

	if ((platform_renderer_sdl_compare_gl_pixels != NULL) &&
		(platform_renderer_sdl_compare_sdl_pixels != NULL) &&
		(platform_renderer_sdl_compare_width == width) &&
		(platform_renderer_sdl_compare_height == height) &&
		(platform_renderer_sdl_compare_pitch == (width * 4)))
	{
		return true;
	}

	free(platform_renderer_sdl_compare_gl_pixels);
	free(platform_renderer_sdl_compare_sdl_pixels);
	platform_renderer_sdl_compare_gl_pixels = NULL;
	platform_renderer_sdl_compare_sdl_pixels = NULL;
	platform_renderer_sdl_compare_width = 0;
	platform_renderer_sdl_compare_height = 0;
	platform_renderer_sdl_compare_pitch = 0;

	platform_renderer_sdl_compare_pitch = width * 4;
	bytes = (size_t) platform_renderer_sdl_compare_pitch * (size_t) height;
	platform_renderer_sdl_compare_gl_pixels = (unsigned char *) malloc(bytes);
	platform_renderer_sdl_compare_sdl_pixels = (unsigned char *) malloc(bytes);
	if ((platform_renderer_sdl_compare_gl_pixels == NULL) || (platform_renderer_sdl_compare_sdl_pixels == NULL))
	{
		free(platform_renderer_sdl_compare_gl_pixels);
		free(platform_renderer_sdl_compare_sdl_pixels);
		platform_renderer_sdl_compare_gl_pixels = NULL;
		platform_renderer_sdl_compare_sdl_pixels = NULL;
		platform_renderer_sdl_compare_pitch = 0;
		return false;
	}

	platform_renderer_sdl_compare_width = width;
	platform_renderer_sdl_compare_height = height;
	return true;
}

static unsigned int PLATFORM_RENDERER_hash_pixels_sampled(
	const unsigned char *pixels,
	int width,
	int height,
	int pitch,
	int step_x,
	int step_y)
{
	unsigned int hash = 2166136261u;
	int x, y;

	if ((pixels == NULL) || (width <= 0) || (height <= 0) || (pitch <= 0))
	{
		return 0u;
	}

	if (step_x <= 0) step_x = 1;
	if (step_y <= 0) step_y = 1;

	for (y = 0; y < height; y += step_y)
	{
		const unsigned char *row = pixels + ((size_t) y * (size_t) pitch);
		for (x = 0; x < width; x += step_x)
		{
			const unsigned char *px = row + ((size_t) x * 4u);
			hash ^= (unsigned int) px[0];
			hash *= 16777619u;
			hash ^= (unsigned int) px[1];
			hash *= 16777619u;
			hash ^= (unsigned int) px[2];
			hash *= 16777619u;
			hash ^= (unsigned int) px[3];
			hash *= 16777619u;
		}
	}

	return hash;
}

static void PLATFORM_RENDERER_compare_gl_and_sdl_outputs(int width, int height)
{
	GLenum gl_error;
	int step_x;
	int step_y;
	int samples = 0;
	int differing = 0;
	int max_channel_delta = 0;
	unsigned int gl_hash;
	unsigned int sdl_hash;
	int hot_source = PLATFORM_RENDERER_GEOM_SRC_NONE;
	int hot_source_count = 0;
	int source_id;
	int x, y;
	unsigned char gl_center_r = 0;
	unsigned char gl_center_g = 0;
	unsigned char gl_center_b = 0;
	unsigned char gl_center_a = 0;
	unsigned char sdl_center_r = 0;
	unsigned char sdl_center_g = 0;
	unsigned char sdl_center_b = 0;
	unsigned char sdl_center_a = 0;

	if (!platform_renderer_sdl_compare_enabled)
	{
		return;
	}
	if (platform_renderer_sdl_renderer == NULL)
	{
		return;
	}
	if (!PLATFORM_RENDERER_should_use_gl())
	{
		return;
	}
	if (!PLATFORM_RENDERER_ensure_compare_buffers(width, height))
	{
		return;
	}

	platform_renderer_sdl_compare_frame_counter++;
	if ((platform_renderer_sdl_compare_every > 1) &&
		((platform_renderer_sdl_compare_frame_counter % (unsigned int) platform_renderer_sdl_compare_every) != 0u))
	{
		return;
	}

	while ((gl_error = glGetError()) != GL_NO_ERROR)
	{
		(void) gl_error;
	}

	glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, platform_renderer_sdl_compare_gl_pixels);
	gl_error = glGetError();
	if (gl_error != GL_NO_ERROR)
	{
		fprintf(stderr, "[SDL2-COMPARE] glReadPixels failed err=0x%x\n", (unsigned int) gl_error);
		return;
	}

	if (SDL_RenderReadPixels(
			platform_renderer_sdl_renderer,
			NULL,
			SDL_PIXELFORMAT_RGBA32,
			platform_renderer_sdl_compare_sdl_pixels,
			platform_renderer_sdl_compare_pitch) != 0)
	{
		fprintf(stderr, "[SDL2-COMPARE] SDL_RenderReadPixels failed: %s\n", SDL_GetError());
		return;
	}

	step_x = width / 160;
	step_y = height / 120;
	if (step_x < 1) step_x = 1;
	if (step_y < 1) step_y = 1;

	for (y = 0; y < height; y += step_y)
	{
		const unsigned char *gl_row = platform_renderer_sdl_compare_gl_pixels + ((size_t) y * (size_t) platform_renderer_sdl_compare_pitch);
		const unsigned char *sdl_row = platform_renderer_sdl_compare_sdl_pixels + ((size_t) y * (size_t) platform_renderer_sdl_compare_pitch);
		for (x = 0; x < width; x += step_x)
		{
			const unsigned char *gl_px = gl_row + ((size_t) x * 4u);
			const unsigned char *sdl_px = sdl_row + ((size_t) x * 4u);
			int dr = abs((int) gl_px[0] - (int) sdl_px[0]);
			int dg = abs((int) gl_px[1] - (int) sdl_px[1]);
			int db = abs((int) gl_px[2] - (int) sdl_px[2]);
			int da = abs((int) gl_px[3] - (int) sdl_px[3]);
			int dmax = dr;

			if (dg > dmax) dmax = dg;
			if (db > dmax) dmax = db;
			if (da > dmax) dmax = da;
			if (dmax > max_channel_delta) max_channel_delta = dmax;

			if ((dr > 12) || (dg > 12) || (db > 12) || (da > 12))
			{
				differing++;
			}
			samples++;
		}
	}

	gl_hash = PLATFORM_RENDERER_hash_pixels_sampled(
		platform_renderer_sdl_compare_gl_pixels,
		width, height, platform_renderer_sdl_compare_pitch,
		step_x, step_y);
	sdl_hash = PLATFORM_RENDERER_hash_pixels_sampled(
		platform_renderer_sdl_compare_sdl_pixels,
		width, height, platform_renderer_sdl_compare_pitch,
		step_x, step_y);
	if ((platform_renderer_sdl_compare_frame_counter > (unsigned int) platform_renderer_sdl_compare_every) &&
		(gl_hash == platform_renderer_compare_last_gl_hash) &&
		(sdl_hash != platform_renderer_compare_last_sdl_hash))
	{
		platform_renderer_compare_gl_same_sdl_changed_streak++;
	}
	else
	{
		platform_renderer_compare_gl_same_sdl_changed_streak = 0;
	}
	platform_renderer_compare_last_gl_hash = gl_hash;
	platform_renderer_compare_last_sdl_hash = sdl_hash;
	if ((width > 0) && (height > 0))
	{
		const int cx = width / 2;
		const int cy = height / 2;
		const unsigned char *gl_center = platform_renderer_sdl_compare_gl_pixels + ((size_t) cy * (size_t) platform_renderer_sdl_compare_pitch) + ((size_t) cx * 4u);
		const unsigned char *sdl_center = platform_renderer_sdl_compare_sdl_pixels + ((size_t) cy * (size_t) platform_renderer_sdl_compare_pitch) + ((size_t) cx * 4u);
		gl_center_r = gl_center[0];
		gl_center_g = gl_center[1];
		gl_center_b = gl_center[2];
		gl_center_a = gl_center[3];
		sdl_center_r = sdl_center[0];
		sdl_center_g = sdl_center[1];
		sdl_center_b = sdl_center[2];
		sdl_center_a = sdl_center[3];
	}

	for (source_id = 1; source_id < PLATFORM_RENDERER_GEOM_SRC_COUNT; source_id++)
	{
		int combined = platform_renderer_sdl_geometry_miss_sources[source_id] + platform_renderer_sdl_geometry_degraded_sources[source_id];
		if (combined > hot_source_count)
		{
			hot_source_count = combined;
			hot_source = source_id;
		}
	}

	fprintf(
		stderr,
		"[SDL2-COMPARE] frame=%u sample=%d diff=%d max_delta=%d gl_hash=%08x sdl_hash=%08x gl_center=%u,%u,%u,%u sdl_center=%u,%u,%u,%u draws=%d textured=%d geom_miss=%d degraded=%d hot_src=%d(%d) miss=[%d,%d,%d,%d,%d,%d,%d] degr=[%d,%d,%d,%d,%d,%d,%d]\n",
		platform_renderer_sdl_compare_frame_counter,
		samples,
		differing,
		max_channel_delta,
		gl_hash,
		sdl_hash,
		(unsigned int) gl_center_r,
		(unsigned int) gl_center_g,
		(unsigned int) gl_center_b,
		(unsigned int) gl_center_a,
		(unsigned int) sdl_center_r,
		(unsigned int) sdl_center_g,
		(unsigned int) sdl_center_b,
		(unsigned int) sdl_center_a,
		platform_renderer_sdl_native_draw_count,
		platform_renderer_sdl_native_textured_draw_count,
		platform_renderer_sdl_geometry_fallback_miss_count,
		platform_renderer_sdl_geometry_degraded_count,
		hot_source,
		hot_source_count,
		platform_renderer_sdl_geometry_miss_sources[1],
		platform_renderer_sdl_geometry_miss_sources[2],
		platform_renderer_sdl_geometry_miss_sources[3],
		platform_renderer_sdl_geometry_miss_sources[4],
		platform_renderer_sdl_geometry_miss_sources[5],
		platform_renderer_sdl_geometry_miss_sources[6],
		platform_renderer_sdl_geometry_miss_sources[7],
		platform_renderer_sdl_geometry_degraded_sources[1],
		platform_renderer_sdl_geometry_degraded_sources[2],
		platform_renderer_sdl_geometry_degraded_sources[3],
		platform_renderer_sdl_geometry_degraded_sources[4],
		platform_renderer_sdl_geometry_degraded_sources[5],
		platform_renderer_sdl_geometry_degraded_sources[6],
		platform_renderer_sdl_geometry_degraded_sources[7]);

	if (platform_renderer_compare_gl_same_sdl_changed_streak >= 3)
	{
		fprintf(
			stderr,
			"[SDL2-COMPARE-WARN] frame=%u gl_hash_stalled=%08x sdl_hash_now=%08x streak=%d gl_draws=%d sdl_draws=%d\n",
			platform_renderer_sdl_compare_frame_counter,
			gl_hash,
			sdl_hash,
			platform_renderer_compare_gl_same_sdl_changed_streak,
			platform_renderer_gl_draw_count,
			platform_renderer_sdl_native_draw_count);
	}
}

void PLATFORM_RENDERER_present_frame(int width, int height)
{
	bool should_flip_allegro_gl = PLATFORM_RENDERER_should_use_gl();
	bool gl_flipped_in_native_test_pre_sdl = false;
#ifdef WIZBALL_USE_SDL2
	PLATFORM_RENDERER_refresh_sdl_stub_env_flags();
	const int native_primary_min_draws_for_no_mirror = platform_renderer_sdl_no_mirror_min_draws;
	const int native_primary_min_textured_draws_for_no_mirror = platform_renderer_sdl_no_mirror_min_textured;
	const int native_primary_required_streak_frames = platform_renderer_sdl_no_mirror_required_streak;
	platform_renderer_present_width = width;
	platform_renderer_present_height = height;
	if (PLATFORM_RENDERER_is_sdl2_stub_ready())
	{
		(void) PLATFORM_RENDERER_sync_sdl_render_target_size(width, height);
	}
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
	int sdl_hot_source = PLATFORM_RENDERER_GEOM_SRC_NONE;
	int sdl_hot_count = 0;
	int gl_hot_source = PLATFORM_RENDERER_GEOM_SRC_NONE;
	int gl_hot_count = 0;
	int geom_i;
	if (!PLATFORM_RENDERER_should_use_gl())
	{
		static int no_gl_present_log_counter = 0;
		native_primary_requested = true;
		native_primary = true;
		native_primary_strict = true;
		native_primary_force_no_mirror = true;
		no_gl_present_log_counter++;
		if ((no_gl_present_log_counter <= 3) || ((no_gl_present_log_counter % 300) == 0))
		{
			PLATFORM_RENDERER_diag_log("SDL2-DIAG", "GL path disabled; forcing no-mirror primary present.");
		}
	}
	if (!platform_renderer_sdl_mode_info_logged)
	{
		fprintf(
			stderr,
			"[SDL2-MODE] use_gl=%d native_sprite=%d native_test=%d native_primary=%d strict=%d force_no_mirror=%d auto_no_mirror=%d allow_degraded=%d mirror=%d dual_present=%d present_with_gl=%d show=%d\n",
			PLATFORM_RENDERER_should_use_gl() ? 1 : 0,
			PLATFORM_RENDERER_is_sdl2_native_sprite_enabled() ? 1 : 0,
			platform_renderer_sdl_native_test_mode_enabled ? 1 : 0,
			native_primary_requested ? 1 : 0,
			native_primary_strict ? 1 : 0,
			native_primary_force_no_mirror ? 1 : 0,
			native_primary_auto_no_mirror ? 1 : 0,
			allow_degraded_for_transition ? 1 : 0,
			platform_renderer_sdl_mirror_enabled ? 1 : 0,
			platform_renderer_sdl_dual_present_enabled ? 1 : 0,
			platform_renderer_sdl_present_with_gl_enabled ? 1 : 0,
			platform_renderer_sdl_stub_show_enabled ? 1 : 0);
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
		if (platform_renderer_gl_draw_sources[geom_i] > gl_hot_count)
		{
			gl_hot_count = platform_renderer_gl_draw_sources[geom_i];
			gl_hot_source = geom_i;
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
		if (platform_renderer_gl_safe_off_enabled &&
			!platform_renderer_gl_mode_forced_on &&
			!platform_renderer_gl_mode_forced_off &&
			!platform_renderer_gl_safe_off_latched &&
			PLATFORM_RENDERER_should_use_gl() &&
			native_primary &&
			native_primary_strict &&
			native_primary_force_no_mirror &&
			PLATFORM_RENDERER_is_sdl2_stub_ready() &&
			coverage_good_this_frame &&
			(platform_renderer_sdl_native_draw_count >= native_primary_min_draws_for_no_mirror) &&
			(platform_renderer_sdl_native_textured_draw_count >= native_primary_min_textured_draws_for_no_mirror) &&
			(platform_renderer_sdl_native_coverage_streak >= platform_renderer_gl_safe_off_required_streak))
		{
			platform_renderer_gl_mode_enabled = false;
			platform_renderer_gl_safe_off_latched = true;
			platform_renderer_sdl_mode_info_logged = false;
			fprintf(
				stderr,
				"[SDL2-MODE] safe_gl_off latch streak=%d req=%d draws=%d textured=%d\n",
				platform_renderer_sdl_native_coverage_streak,
				platform_renderer_gl_safe_off_required_streak,
				platform_renderer_sdl_native_draw_count,
				platform_renderer_sdl_native_textured_draw_count);
		}
		if (platform_renderer_gl_safe_off_enabled &&
			!platform_renderer_gl_mode_forced_on &&
			!platform_renderer_gl_mode_forced_off &&
			!platform_renderer_gl_safe_off_latched)
		{
			platform_renderer_gl_safe_off_progress_log_counter++;
			if ((platform_renderer_gl_safe_off_progress_log_counter <= 5u) ||
				((platform_renderer_gl_safe_off_progress_log_counter % 300u) == 0u))
			{
				fprintf(
					stderr,
					"[SDL2-MODE] safe_gl_off progress streak=%d/%d draws=%d textured=%d cov=%d geom_miss=%d degraded=%d\n",
					platform_renderer_sdl_native_coverage_streak,
					platform_renderer_gl_safe_off_required_streak,
					platform_renderer_sdl_native_draw_count,
					platform_renderer_sdl_native_textured_draw_count,
					coverage_good_this_frame ? 1 : 0,
					platform_renderer_sdl_geometry_fallback_miss_count,
					platform_renderer_sdl_geometry_degraded_count);
			}
		}
		if (platform_renderer_gl_safe_off_enabled &&
			!platform_renderer_gl_mode_forced_on &&
			!platform_renderer_gl_mode_forced_off &&
			platform_renderer_gl_safe_off_latched &&
			!PLATFORM_RENDERER_should_use_gl())
		{
			bool rollback_bad_coverage =
				!coverage_good_this_frame ||
				(platform_renderer_sdl_native_draw_count < native_primary_min_draws_for_no_mirror) ||
				(platform_renderer_sdl_native_textured_draw_count < native_primary_min_textured_draws_for_no_mirror);

			if (rollback_bad_coverage)
			{
				if (platform_renderer_gl_safe_off_regression_streak < 1000000)
				{
					platform_renderer_gl_safe_off_regression_streak++;
				}
			}
			else
			{
				platform_renderer_gl_safe_off_regression_streak = 0;
			}

			if (platform_renderer_gl_safe_off_regression_streak >= platform_renderer_gl_safe_off_rollback_streak)
			{
				platform_renderer_gl_mode_enabled = true;
				platform_renderer_gl_safe_off_latched = false;
				platform_renderer_gl_safe_off_regression_streak = 0;
				platform_renderer_sdl_native_coverage_streak = 0;
				platform_renderer_sdl_mode_info_logged = false;
				fprintf(
					stderr,
					"[SDL2-MODE] safe_gl_off rollback req=%d draws=%d textured=%d geom_miss=%d degraded=%d\n",
					platform_renderer_gl_safe_off_rollback_streak,
					platform_renderer_sdl_native_draw_count,
					platform_renderer_sdl_native_textured_draw_count,
					platform_renderer_sdl_geometry_fallback_miss_count,
					platform_renderer_sdl_geometry_degraded_count);
			}
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
		PLATFORM_RENDERER_diag_log("SDL2-DIAG", "Present requested mirror fallback because native_primary=false.");
		(void) PLATFORM_RENDERER_mirror_from_current_backbuffer(width, height);
		used_mirror_fallback = true;
	}
	else if (platform_renderer_sdl_dual_present_enabled && PLATFORM_RENDERER_should_use_gl())
	{
		/*
		 * Dual-present mode keeps both GL and SDL windows visually active by
		 * mirroring the current GL backbuffer to SDL each frame.
		 * This avoids introducing an extra independent swap path.
		 */
		(void) PLATFORM_RENDERER_mirror_from_current_backbuffer(width, height);
		used_mirror_fallback = true;
	}
	else if ((!native_primary_strict) || !allow_no_mirror_this_frame)
	{
		// Default primary mode remains safe: mirror while migration coverage is incomplete.
		PLATFORM_RENDERER_diag_log("SDL2-DIAG", "Present requested mirror fallback because strict coverage gate was not met.");
		if (!platform_renderer_sdl_native_test_mode_enabled)
		{
			(void) PLATFORM_RENDERER_mirror_from_current_backbuffer(width, height);
			used_mirror_fallback = true;
		}
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
		if (native_primary && !used_mirror_fallback)
		{
			PLATFORM_RENDERER_compare_gl_and_sdl_outputs(width, height);
		}
		{
			const bool should_present_sdl =
				!(platform_renderer_sdl_native_test_mode_enabled &&
				  platform_renderer_sdl_stub_show_enabled &&
				  PLATFORM_RENDERER_should_use_gl() &&
				  !platform_renderer_sdl_present_with_gl_enabled);
		if (platform_renderer_sdl_native_test_mode_enabled)
		{
				if (PLATFORM_RENDERER_should_use_gl())
				{
					GLint viewport[4] = {0, 0, 0, 0};
					GLint scissor[4] = {0, 0, 0, 0};
					GLint draw_buffer = 0;
					GLint read_buffer = 0;
					GLint matrix_mode = 0;
					GLfloat modelview[16];
					GLfloat projection[16];
					const float identity_epsilon = 0.0005f;
					int i;
					GLenum gl_err = GL_NO_ERROR;
					glGetIntegerv(GL_VIEWPORT, viewport);
					glGetIntegerv(GL_SCISSOR_BOX, scissor);
					glGetIntegerv(GL_DRAW_BUFFER, &draw_buffer);
					glGetIntegerv(GL_READ_BUFFER, &read_buffer);
					glGetIntegerv(GL_MATRIX_MODE, &matrix_mode);
					glGetFloatv(GL_MODELVIEW_MATRIX, modelview);
					glGetFloatv(GL_PROJECTION_MATRIX, projection);
					platform_renderer_gl_viewport_last[0] = viewport[0];
					platform_renderer_gl_viewport_last[1] = viewport[1];
					platform_renderer_gl_viewport_last[2] = viewport[2];
					platform_renderer_gl_viewport_last[3] = viewport[3];
				platform_renderer_gl_scissor_last[0] = scissor[0];
				platform_renderer_gl_scissor_last[1] = scissor[1];
				platform_renderer_gl_scissor_last[2] = scissor[2];
					platform_renderer_gl_scissor_last[3] = scissor[3];
					platform_renderer_gl_scissor_enabled_last = glIsEnabled(GL_SCISSOR_TEST) ? 1 : 0;
					platform_renderer_gl_draw_buffer_last = (int) draw_buffer;
					platform_renderer_gl_read_buffer_last = (int) read_buffer;
					platform_renderer_gl_matrix_mode_last = (int) matrix_mode;
					platform_renderer_gl_modelview_identity_last = 1;
					platform_renderer_gl_projection_identity_last = 1;
					platform_renderer_gl_modelview_tx_last = modelview[12];
					platform_renderer_gl_modelview_ty_last = modelview[13];
					platform_renderer_gl_projection_tx_last = projection[12];
					platform_renderer_gl_projection_ty_last = projection[13];
					for (i = 0; i < 16; i++)
					{
						const float identity_value = ((i % 5) == 0) ? 1.0f : 0.0f;
						if (fabsf(modelview[i] - identity_value) > identity_epsilon)
						{
							platform_renderer_gl_modelview_identity_last = 0;
						}
						if (fabsf(projection[i] - identity_value) > identity_epsilon)
						{
							platform_renderer_gl_projection_identity_last = 0;
						}
					}
					gl_err = glGetError();
					platform_renderer_gl_error_last = (int) gl_err;
					if (platform_renderer_gl_diag_first_error_code != 0)
					{
					platform_renderer_gl_error_last = platform_renderer_gl_diag_first_error_code;
				}
			}
				fprintf(
					stderr,
					"[SDL2-NATIVE-TEST] frame=%u fallback=%d draws=%d textured=%d geom_miss=%d degraded=%d streak=%d compare_every=%d flip_gl=%d present_sdl=%d native_primary=%d strict=%d force_no_mirror=%d auto_no_mirror=%d allow_no_mirror=%d mirror_enabled=%d present_with_gl=%d dual_present=%d clear_calls=%d midframe_resets=%d\n",
				platform_renderer_sdl_compare_frame_counter,
				used_mirror_fallback ? 1 : 0,
				platform_renderer_sdl_native_draw_count,
				platform_renderer_sdl_native_textured_draw_count,
				platform_renderer_sdl_geometry_fallback_miss_count,
				platform_renderer_sdl_geometry_degraded_count,
				platform_renderer_sdl_native_coverage_streak,
				platform_renderer_sdl_compare_every,
				should_flip_allegro_gl ? 1 : 0,
				should_present_sdl ? 1 : 0,
				native_primary_requested ? 1 : 0,
				native_primary_strict ? 1 : 0,
				native_primary_force_no_mirror ? 1 : 0,
				native_primary_auto_no_mirror ? 1 : 0,
				allow_no_mirror_this_frame ? 1 : 0,
				platform_renderer_sdl_mirror_enabled ? 1 : 0,
				platform_renderer_sdl_present_with_gl_enabled ? 1 : 0,
					platform_renderer_sdl_dual_present_enabled ? 1 : 0,
					platform_renderer_clear_backbuffer_calls_since_present,
					platform_renderer_midframe_reset_events);
				PLATFORM_RENDERER_log_native_draw_transition(
					platform_renderer_sdl_compare_frame_counter,
					platform_renderer_prev_sdl_draw_count,
					platform_renderer_prev_sdl_textured_count,
					platform_renderer_prev_gl_draw_count,
					platform_renderer_prev_gl_textured_count,
					platform_renderer_sdl_native_draw_count,
					platform_renderer_sdl_native_textured_draw_count,
					platform_renderer_gl_draw_count,
					platform_renderer_gl_textured_draw_count);
					fprintf(
						stderr,
						"[SDL2-NATIVE-TEST] details win_sprite=%d win_rect=%d bound_custom=%d gl_win_sprite=%d gl_win_rect=%d gl_bound_custom=%d gl_src6=%d q=[%.3f,%.3f] gl_src6_bounds=[%.1f,%.1f]-[%.1f,%.1f] gl_draws=%d gl_textured=%d hot_sdl_src=%d(%d) hot_gl_src=%d(%d) sdl_bounds=[%.1f,%.1f]-[%.1f,%.1f] out=%dx%d scale=%.3fx%.3f gl_vp=%d,%d,%d,%d gl_scissor=%d,%d,%d,%d gl_scissor_on=%d gl_draw_buf=0x%04x gl_read_buf=0x%04x gl_matrix_mode=0x%04x gl_mv_id=%d gl_pr_id=%d gl_mv_t=%.1f,%.1f gl_pr_t=%.1f,%.1f gl_err=0x%04x gl_err_stage=%s gl_err_count=%u sw_forced=%d src6_degraded=%d geometry=%d sdl_present=%d gl_diag_overlay=%d\n",
				platform_renderer_sdl_window_sprite_draw_count,
				platform_renderer_sdl_window_solid_rect_draw_count,
				platform_renderer_sdl_bound_custom_draw_count,
				platform_renderer_gl_window_sprite_draw_count,
				platform_renderer_gl_window_solid_rect_draw_count,
				platform_renderer_gl_bound_custom_draw_count,
				platform_renderer_gl_src6_count,
				platform_renderer_gl_src6_min_q,
				platform_renderer_gl_src6_max_q,
				platform_renderer_gl_src6_min_x,
				platform_renderer_gl_src6_min_y,
				platform_renderer_gl_src6_max_x,
				platform_renderer_gl_src6_max_y,
				platform_renderer_gl_draw_count,
				platform_renderer_gl_textured_draw_count,
				sdl_hot_source,
				sdl_hot_count,
				gl_hot_source,
				gl_hot_count,
				(sdl_hot_source > 0) ? platform_renderer_sdl_source_min_x[sdl_hot_source] : 0.0f,
				(sdl_hot_source > 0) ? platform_renderer_sdl_source_min_y[sdl_hot_source] : 0.0f,
				(sdl_hot_source > 0) ? platform_renderer_sdl_source_max_x[sdl_hot_source] : 0.0f,
				(sdl_hot_source > 0) ? platform_renderer_sdl_source_max_y[sdl_hot_source] : 0.0f,
				platform_renderer_sdl_output_width,
				platform_renderer_sdl_output_height,
				platform_renderer_sdl_scale_x,
				platform_renderer_sdl_scale_y,
				platform_renderer_gl_viewport_last[0],
				platform_renderer_gl_viewport_last[1],
				platform_renderer_gl_viewport_last[2],
				platform_renderer_gl_viewport_last[3],
				platform_renderer_gl_scissor_last[0],
				platform_renderer_gl_scissor_last[1],
				platform_renderer_gl_scissor_last[2],
					platform_renderer_gl_scissor_last[3],
					platform_renderer_gl_scissor_enabled_last,
					platform_renderer_gl_draw_buffer_last,
					platform_renderer_gl_read_buffer_last,
					platform_renderer_gl_matrix_mode_last,
					platform_renderer_gl_modelview_identity_last,
					platform_renderer_gl_projection_identity_last,
					platform_renderer_gl_modelview_tx_last,
					platform_renderer_gl_modelview_ty_last,
					platform_renderer_gl_projection_tx_last,
					platform_renderer_gl_projection_ty_last,
					platform_renderer_gl_error_last,
					platform_renderer_gl_diag_first_error_stage,
				platform_renderer_gl_diag_error_count_frame,
				platform_renderer_sdl_renderer_software_forced ? 1 : 0,
					platform_renderer_sdl_geometry_degraded_sources[PLATFORM_RENDERER_GEOM_SRC_COLOURED_PERSPECTIVE],
					platform_renderer_sdl_geometry_enabled ? 1 : 0,
					should_present_sdl ? 1 : 0,
					platform_renderer_gl_diag_overlay_enabled ? 1 : 0);
				platform_renderer_prev_sdl_draw_count = platform_renderer_sdl_native_draw_count;
				platform_renderer_prev_sdl_textured_count = platform_renderer_sdl_native_textured_draw_count;
				platform_renderer_prev_gl_draw_count = platform_renderer_gl_draw_count;
				platform_renderer_prev_gl_textured_count = platform_renderer_gl_textured_draw_count;
			}
		if (should_present_sdl)
		{
			SDL_RenderPresent(platform_renderer_sdl_renderer);
		}
		}
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
		if (platform_renderer_sdl_native_test_mode_enabled && platform_renderer_sdl_stub_show_enabled && PLATFORM_RENDERER_should_use_gl())
		{
			/*
			 * Keep GL flip at the canonical end-of-frame position when SDL is also
			 * presenting. Pre-SDL GL flips can run under a different active context
			 * on some renderer backends, leaving the GL window black.
			 */
			should_flip_allegro_gl = true;
		}
	#else
		(void) width;
		(void) height;
#endif
	if (PLATFORM_RENDERER_should_use_gl() && platform_renderer_gl_diag_overlay_enabled)
	{
		static unsigned int gl_diag_overlay_frame = 0;
		GLint previous_matrix_mode = GL_MODELVIEW;
		GLboolean was_texture_2d_enabled = GL_FALSE;
		GLboolean was_blend_enabled = GL_FALSE;
		const bool use_red = (((gl_diag_overlay_frame / 30u) % 2u) == 0u);
		gl_diag_overlay_frame++;

		glGetIntegerv(GL_MATRIX_MODE, &previous_matrix_mode);
		was_texture_2d_enabled = glIsEnabled(GL_TEXTURE_2D);
		was_blend_enabled = glIsEnabled(GL_BLEND);

		glDisable(GL_TEXTURE_2D);
		glDisable(GL_BLEND);
		glMatrixMode(GL_PROJECTION);
		glPushMatrix();
		glLoadIdentity();
		glMatrixMode(GL_MODELVIEW);
		glPushMatrix();
		glLoadIdentity();

		glColor4f(use_red ? 1.0f : 0.0f, use_red ? 0.0f : 1.0f, 0.0f, 1.0f);
		glBegin(GL_QUADS);
		glVertex2f(-1.0f, -1.0f);
		glVertex2f(1.0f, -1.0f);
		glVertex2f(1.0f, 1.0f);
		glVertex2f(-1.0f, 1.0f);
		glEnd();
		glColor4f(1.0f, 1.0f, 1.0f, 1.0f);

		glPopMatrix();
		glMatrixMode(GL_PROJECTION);
		glPopMatrix();
		glMatrixMode(previous_matrix_mode);

		if (was_texture_2d_enabled)
		{
			glEnable(GL_TEXTURE_2D);
		}
		else
		{
			glDisable(GL_TEXTURE_2D);
		}
		if (was_blend_enabled)
		{
			glEnable(GL_BLEND);
		}
		else
		{
			glDisable(GL_BLEND);
		}

		if ((gl_diag_overlay_frame <= 5u) || ((gl_diag_overlay_frame % 120u) == 0u))
		{
			fprintf(
				stderr,
				"[GL-DIAG] overlay frame=%u colour=%s\n",
				gl_diag_overlay_frame,
				use_red ? "red" : "green");
		}
	}
	if (should_flip_allegro_gl)
	{
		allegro_gl_flip();
	}
#ifdef WIZBALL_USE_SDL2
	else if (platform_renderer_sdl_native_test_mode_enabled && platform_renderer_sdl_stub_show_enabled && PLATFORM_RENDERER_should_use_gl())
	{
		/*
		 * Native test mode still keeps GL visible for side-by-side sanity checks.
		 * Rendering authority remains SDL native in this mode.
		 */
		if (!gl_flipped_in_native_test_pre_sdl)
		{
			allegro_gl_flip();
		}
	}
#endif
	platform_renderer_clear_backbuffer_calls_since_present = 0;
	platform_renderer_midframe_reset_events = 0;
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
	if (!PLATFORM_RENDERER_should_use_gl())
	{
		return;
	}
	/*
	 * Wizball's legacy GL path is 2D fixed-function. Force a predictable state
	 * baseline so driver/context defaults cannot silently reject draw calls.
	 */
	glDisable(GL_DEPTH_TEST);
	glDepthMask(GL_FALSE);
	glDisable(GL_CULL_FACE);
	glDisable(GL_LIGHTING);
	glDisable(GL_FOG);
	glDisable(GL_ALPHA_TEST);
	glDisable(GL_BLEND);
	glEnable(GL_TEXTURE_2D);
	glPolygonMode(GL_FRONT, GL_FILL);
	glShadeModel(GL_SMOOTH);
	glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);
	glDisable(GL_SCISSOR_TEST);
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

	if (!platform_renderer_sdl_renderer_info_logged)
	{
		if (platform_renderer_sdl_renderer == NULL)
		{
			fprintf(stderr, "[SDL2-RENDERER] info unavailable: renderer not initialized yet.\n");
		}
		else
		{
			SDL_RendererInfo renderer_info;
			if (SDL_GetRendererInfo(platform_renderer_sdl_renderer, &renderer_info) == 0)
			{
				fprintf(
					stderr,
					"[SDL2-RENDERER] name=%s flags=0x%x sw_forced=%d accel_env=%d native_test=%d show=%d\n",
					(renderer_info.name != NULL) ? renderer_info.name : "unknown",
					(unsigned int) renderer_info.flags,
					platform_renderer_sdl_renderer_software_forced ? 1 : 0,
					platform_renderer_sdl_stub_accel_enabled ? 1 : 0,
					platform_renderer_sdl_native_test_mode_enabled ? 1 : 0,
					platform_renderer_sdl_stub_show_enabled ? 1 : 0);
			}
			else
			{
				fprintf(stderr, "[SDL2-RENDERER] SDL_GetRendererInfo failed: %s\n", SDL_GetError());
			}
		}
		platform_renderer_sdl_renderer_info_logged = true;
	}

	*opengl_major_version = 0;
	*opengl_minor_version = 0;
	if (!PLATFORM_RENDERER_should_use_gl())
	{
		return false;
	}

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

	/*
	 * Keep SDL submission order strict for mixed primitive/sprite scenes
	 * (e.g. starfield lines + intro text overlays).
	 */
	(void) SDL_SetHint(SDL_HINT_RENDER_BATCHING, "0");

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
		bool force_software_renderer = false;
		if (PLATFORM_RENDERER_should_force_software_renderer_for_gl_side_by_side())
		{
			/*
			 * Side-by-side native test mode can keep GL active later in startup
			 * (or explicitly via PRESENT_WITH_GL). Force SDL software renderer to
			 * avoid backend GL context contention across windows regardless of
			 * init ordering.
			 */
			force_software_renderer = true;
		}
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
		platform_renderer_sdl_renderer = SDL_CreateRenderer(
			platform_renderer_sdl_window,
			-1,
			SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
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
		bool force_software_now = PLATFORM_RENDERER_should_force_software_renderer_for_gl_side_by_side();
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
					fprintf(
						stderr,
						"[SDL2-RENDERER] name=%s flags=0x%x sw_forced=%d accel_env=%d native_test=%d present_with_gl=%d show=%d\n",
						(renderer_info.name != NULL) ? renderer_info.name : "unknown",
						(unsigned int) renderer_info.flags,
						platform_renderer_sdl_renderer_software_forced ? 1 : 0,
						platform_renderer_sdl_stub_accel_enabled ? 1 : 0,
						platform_renderer_sdl_native_test_mode_enabled ? 1 : 0,
						platform_renderer_sdl_present_with_gl_enabled ? 1 : 0,
						platform_renderer_sdl_stub_show_enabled ? 1 : 0);
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
			strcpy(platform_renderer_sdl_status, "SDL2 stub initialized (visible window + software renderer forced for native-test GL stability).");
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
			strcpy(platform_renderer_sdl_status, "SDL2 stub initialized (hidden window + software renderer forced for native-test GL stability).");
		}
		else
		{
			strcpy(platform_renderer_sdl_status, "SDL2 stub initialized (hidden window + software renderer).");
		}
	}
	(void) PLATFORM_RENDERER_sync_sdl_render_target_size(width, height);
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
	if (!PLATFORM_RENDERER_should_use_gl())
	{
		static bool mirror_block_logged = false;
		if (!mirror_block_logged)
		{
			PLATFORM_RENDERER_diag_log("SDL2-DIAG", "Mirror path blocked because GL is disabled.");
			mirror_block_logged = true;
		}
		return false;
	}
	PLATFORM_RENDERER_refresh_sdl_stub_env_flags();

	if (!platform_renderer_sdl_mirror_enabled)
	{
		PLATFORM_RENDERER_diag_log("SDL2-DIAG", "Mirror path skipped (WIZBALL_SDL2_STUB_MIRROR disabled).");
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
	free(platform_renderer_sdl_compare_gl_pixels);
	free(platform_renderer_sdl_compare_sdl_pixels);
	platform_renderer_sdl_compare_gl_pixels = NULL;
	platform_renderer_sdl_compare_sdl_pixels = NULL;
	platform_renderer_sdl_compare_width = 0;
	platform_renderer_sdl_compare_height = 0;
	platform_renderer_sdl_compare_pitch = 0;
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
