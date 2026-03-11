#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <stdarg.h>
#include <stddef.h>
#include <SDL.h>
#if defined(WIZBALL_RENDER_BACKEND_GLES2)
#include <SDL_opengles2.h>
#endif

#include "platform_renderer.h"
#include "main.h"
#include "output.h"
#include "scripting.h"
#include "platform_renderer_texture_entry.h"

#if defined(WIZBALL_RENDER_BACKEND_GLES2)
#define PLATFORM_RENDERER_GLES2_BATCH_MAX_VERTICES 8192
#define PLATFORM_RENDERER_GLES2_BATCH_MAX_INDICES 12288
#endif

static void PLATFORM_RENDERER_apply_sdl_texture_blend_mode(SDL_Texture *texture);
static Uint8 PLATFORM_RENDERER_get_sdl_texture_alpha_mod(Uint8 requested_alpha);
static void PLATFORM_RENDERER_set_texture_color_alpha_cached(SDL_Texture *tex, Uint8 r, Uint8 g, Uint8 b, Uint8 a);
static int PLATFORM_RENDERER_sdl_copy_ex(SDL_Renderer *renderer, SDL_Texture *texture, const SDL_Rect *src, const SDL_Rect *dst, double angle, const SDL_Point *center, SDL_RendererFlip flip);
static void PLATFORM_RENDERER_flush_addq(void);
static void PLATFORM_RENDERER_flush_command_queue(void);
static bool PLATFORM_RENDERER_should_defer_add_geometry(SDL_Texture *texture, int vertex_count, const int *indices, int index_count, int source_id);
static bool PLATFORM_RENDERER_prepare_geometry_texture_atlas(void);
static bool PLATFORM_RENDERER_try_remap_geometry_texture_to_atlas(SDL_Texture **io_texture, SDL_Vertex *vertices, int vertex_count);
static platform_renderer_texture_entry *PLATFORM_RENDERER_get_texture_entry(unsigned int texture_handle);
static Uint8 PLATFORM_RENDERER_clamp_sdl_unit_to_byte(float value);
static void PLATFORM_RENDERER_transform_point(float in_x, float in_y, float *out_x, float *out_y);
static void PLATFORM_RENDERER_draw_bound_perspective_textured_quad_immediate(float x0, float y0, float x1, float y1, float x2, float y2, float x3, float y3, float u1, float v1, float u2, float v2, float q);
static void PLATFORM_RENDERER_draw_bound_coloured_perspective_textured_quad_immediate(float x0, float y0, float x1, float y1, float x2, float y2, float x3, float y3, float u1, float v1, float u2, float v2, float q, const float *r, const float *g, const float *b, const float *a);
static bool PLATFORM_RENDERER_draw_bound_perspective_textured_quad_batch_immediate(const platform_renderer_perspective_quad *quads, int quad_count);
static bool PLATFORM_RENDERER_draw_bound_coloured_perspective_textured_quad_batch_immediate(const platform_renderer_coloured_perspective_quad *quads, int quad_count);
#if defined(WIZBALL_RENDER_BACKEND_GLES2)
static bool PLATFORM_RENDERER_gles2_init_context(void);
static void PLATFORM_RENDERER_gles2_shutdown(void);
static bool PLATFORM_RENDERER_gles2_is_ready(void);
static bool PLATFORM_RENDERER_build_gles2_texture_from_entry(platform_renderer_texture_entry *entry);
static void PLATFORM_RENDERER_gles2_prewarm_registered_textures(void);
static void PLATFORM_RENDERER_gles2_apply_blend_state(void);
static bool PLATFORM_RENDERER_gles2_draw_textured_geometry_gltex(
		GLuint tex,
		unsigned int logical_texture_handle,
		const SDL_Vertex *vertices,
		int vertex_count,
		const int *indices,
		int index_count,
		int source_id,
		bool repeat_s,
		bool repeat_t);
static bool PLATFORM_RENDERER_gles2_submit_textured_geometry_gltex(
		GLuint tex,
		unsigned int logical_texture_handle,
		const SDL_Vertex *vertices,
		int vertex_count,
		const GLushort *indices,
		int index_count,
		int source_id,
		bool repeat_s,
		bool repeat_t);
static void PLATFORM_RENDERER_gles2_flush_textured_batch(void);
static void PLATFORM_RENDERER_gles2_apply_scissor_rect(int x, int y, int w, int h);
static bool PLATFORM_RENDERER_gles2_draw_wrapped_quad_geometry_gltex(
		GLuint tex,
		unsigned int logical_texture_handle,
		const SDL_Vertex *vertices,
		int vertex_count,
		const int *indices,
		int index_count,
		int source_id);
static bool PLATFORM_RENDERER_gles2_draw_textured_geometry(unsigned int texture_handle, const SDL_Vertex *vertices, int vertex_count, const int *indices, int index_count, int source_id);
static bool PLATFORM_RENDERER_gles2_draw_textured_subrect_geometry(
		platform_renderer_texture_entry *entry,
		unsigned int logical_texture_handle,
		const SDL_Rect *src_rect,
		const SDL_Vertex *vertices,
		int vertex_count,
		const int *indices,
		int index_count,
		int source_id);
static bool PLATFORM_RENDERER_gles2_draw_textured_subrect_geometry_gltex(
		platform_renderer_texture_entry *entry,
		GLuint tex,
		unsigned int logical_texture_handle,
		const SDL_Rect *src_rect,
		const SDL_Vertex *vertices,
		int vertex_count,
		const int *indices,
		int index_count,
		int source_id);
static float PLATFORM_RENDERER_convert_v_to_sdl(float v);
static void PLATFORM_RENDERER_gles2_interp_vertex(SDL_Vertex *out, const SDL_Vertex *a, const SDL_Vertex *b, float t);
static void PLATFORM_RENDERER_gles2_normalize_quad_uv_band(SDL_Vertex quad[4]);
static bool PLATFORM_RENDERER_gles2_split_quad_wrap_s(const SDL_Vertex in_quad[4], SDL_Vertex out_quads[2][4]);
static bool PLATFORM_RENDERER_gles2_split_quad_wrap_t(const SDL_Vertex in_quad[4], SDL_Vertex out_quads[2][4]);
static bool PLATFORM_RENDERER_gles2_draw_multitextured_geometry(
		unsigned int texture0_handle,
		unsigned int texture1_handle,
		int combiner_mode,
		const float *positions,
		const float *uv0,
		const float *uv1,
		const unsigned char *colours,
		int vertex_count,
		const int *indices,
		int index_count,
		int source_id);
static bool PLATFORM_RENDERER_gles2_draw_multitextured_perspective_quad(
		float x0, float y0, float x1, float y1, float x2, float y2, float x3, float y3,
		float u0_1, float v0_1, float u0_2, float v0_2,
		float u1_1, float v1_1, float u1_2, float v1_2,
		float q);
static int PLATFORM_RENDERER_get_perspective_strip_count(float q, bool gles_optimized);
static bool PLATFORM_RENDERER_gles2_draw_axis_wrapped_u_repeat_geometry(
		unsigned int logical_texture_handle,
		const float *screen_x,
		const float *screen_y,
		float u1, float v1, float u2, float v2,
		Uint8 mod_r, Uint8 mod_g, Uint8 mod_b, Uint8 mod_a,
		int source_id);
static bool PLATFORM_RENDERER_gles2_draw_coloured_line_segment(
		float x1, float y1, float x2, float y2,
		Uint8 r, Uint8 g, Uint8 b, Uint8 a);
#endif

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
static bool platform_renderer_texture_filter_linear = false;
static bool platform_renderer_texture_masked = false;
enum
{
	PLATFORM_RENDERER_BLEND_MODE_ALPHA = 0,
	PLATFORM_RENDERER_BLEND_MODE_ADD = 1,
	PLATFORM_RENDERER_BLEND_MODE_SUBTRACT = 2,
	PLATFORM_RENDERER_BLEND_MODE_MULTIPLY = 3
};
static int platform_renderer_blend_mode = PLATFORM_RENDERER_BLEND_MODE_ALPHA;
static unsigned int platform_renderer_last_bound_texture_handle = 0;
static unsigned int platform_renderer_last_bound_secondary_texture_handle = 0;
enum
{
	PLATFORM_RENDERER_COMBINER_MODULATE_PRIMARY = 0,
	PLATFORM_RENDERER_COMBINER_REPLACE_RGB_MODULATE_ALPHA_PREVIOUS = 1,
	PLATFORM_RENDERER_COMBINER_REPLACE_RGB_MODULATE_ALPHA_PRIMARY = 2,
	PLATFORM_RENDERER_COMBINER_MODULATE_RGB_REPLACE_ALPHA_PREVIOUS = 3
};
static int platform_renderer_combiner_mode = PLATFORM_RENDERER_COMBINER_MODULATE_PRIMARY;
typedef void (*platform_active_texture_proc_t)(int texture_unit);
static platform_active_texture_proc_t platform_renderer_active_texture_proc = NULL;
typedef void (*platform_secondary_colour_proc_t)(float red, float green, float blue);
static SDL_Window *platform_renderer_sdl_window = NULL;
static SDL_Renderer *platform_renderer_sdl_renderer = NULL;
#if defined(WIZBALL_RENDER_BACKEND_GLES2)
static SDL_GLContext platform_renderer_gles2_context = NULL;
typedef struct
{
	float position[2];
	float uv0[2];
	float uv1[2];
	unsigned char color[4];
} platform_renderer_gles2_multitex_vertex;
typedef struct
{
	GLuint tex;
	unsigned int logical_texture_handle;
	bool repeat_s;
	bool repeat_t;
	bool filtered;
	bool masked;
	bool blend_enabled;
	int blend_mode;
	bool affine_active;
	GLfloat affine_row0[3];
	GLfloat affine_row1[3];
} platform_renderer_gles2_textured_batch_key;
static GLuint platform_renderer_gles2_program = 0;
static GLint platform_renderer_gles2_attr_pos = -1;
static GLint platform_renderer_gles2_attr_uv = -1;
static GLint platform_renderer_gles2_attr_color = -1;
static GLint platform_renderer_gles2_uniform_screen = -1;
static GLint platform_renderer_gles2_uniform_sampler = -1;
static GLint platform_renderer_gles2_uniform_masked = -1;
static GLint platform_renderer_gles2_uniform_affine_row0 = -1;
static GLint platform_renderer_gles2_uniform_affine_row1 = -1;
static GLuint platform_renderer_gles2_multitex_program = 0;
static GLint platform_renderer_gles2_multitex_attr_pos = -1;
static GLint platform_renderer_gles2_multitex_attr_uv0 = -1;
static GLint platform_renderer_gles2_multitex_attr_uv1 = -1;
static GLint platform_renderer_gles2_multitex_attr_color = -1;
static GLint platform_renderer_gles2_multitex_uniform_screen = -1;
static GLint platform_renderer_gles2_multitex_uniform_sampler0 = -1;
static GLint platform_renderer_gles2_multitex_uniform_sampler1 = -1;
static GLint platform_renderer_gles2_multitex_uniform_mode = -1;
static GLint platform_renderer_gles2_multitex_uniform_masked = -1;
static GLint platform_renderer_gles2_multitex_uniform_affine_row0 = -1;
static GLint platform_renderer_gles2_multitex_uniform_affine_row1 = -1;
static bool platform_renderer_gles2_affine_override_active = false;
static GLfloat platform_renderer_gles2_affine_row0[3] = {1.0f, 0.0f, 0.0f};
static GLfloat platform_renderer_gles2_affine_row1[3] = {0.0f, 1.0f, 0.0f};
static GLuint platform_renderer_gles2_white_texture = 0;
static GLuint platform_renderer_gles2_stream_vbo = 0;
static GLuint platform_renderer_gles2_stream_ibo = 0;
static bool platform_renderer_gles2_textured_batch_active = false;
static platform_renderer_gles2_textured_batch_key platform_renderer_gles2_textured_batch_state = {0};
static SDL_Vertex platform_renderer_gles2_textured_batch_vertices[PLATFORM_RENDERER_GLES2_BATCH_MAX_VERTICES];
static GLushort platform_renderer_gles2_textured_batch_indices[PLATFORM_RENDERER_GLES2_BATCH_MAX_INDICES];
static int platform_renderer_gles2_textured_batch_vertex_count = 0;
static int platform_renderer_gles2_textured_batch_index_count = 0;
static int platform_renderer_gles2_flush_key_mismatch_count = 0;
static int platform_renderer_gles2_flush_batch_full_count = 0;
static int platform_renderer_gles2_flush_state_change_count = 0;
static int platform_renderer_gles2_flush_window_finish_count = 0;
static int platform_renderer_gles2_flush_clear_count = 0;
static int platform_renderer_gles2_flush_present_count = 0;
#endif
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
enum
{
	PLATFORM_RENDERER_CMD_NONE = 0,
	PLATFORM_RENDERER_CMD_PERSPECTIVE = 1,
	PLATFORM_RENDERER_CMD_COLOURED_PERSPECTIVE = 2
};
#define PLATFORM_RENDERER_CMDQ_MAX 512
struct platform_renderer_command
{
	int type;
	unsigned int texture_handle;
	bool blend_enabled;
	int blend_mode;
	float colour_r;
	float colour_g;
	float colour_b;
	float colour_a;
	float tx_a;
	float tx_b;
	float tx_c;
	float tx_d;
	float tx_x;
	float tx_y;
	float x0;
	float y0;
	float x1;
	float y1;
	float x2;
	float y2;
	float x3;
	float y3;
	float u1;
	float v1;
	float u2;
	float v2;
	float q;
	float vr[4];
	float vg[4];
	float vb[4];
	float va[4];
};
static platform_renderer_command platform_renderer_command_queue[PLATFORM_RENDERER_CMDQ_MAX];
static int platform_renderer_command_queue_count = 0;
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
static int platform_renderer_gles2_draw_sources[PLATFORM_RENDERER_GEOM_SRC_COUNT] = {0};
static int platform_renderer_sdl_source_bounds_count[PLATFORM_RENDERER_GEOM_SRC_COUNT] = {0};
static float platform_renderer_sdl_source_min_x[PLATFORM_RENDERER_GEOM_SRC_COUNT] = {0.0f};
static float platform_renderer_sdl_source_min_y[PLATFORM_RENDERER_GEOM_SRC_COUNT] = {0.0f};
static float platform_renderer_sdl_source_max_x[PLATFORM_RENDERER_GEOM_SRC_COUNT] = {0.0f};
static float platform_renderer_sdl_source_max_y[PLATFORM_RENDERER_GEOM_SRC_COUNT] = {0.0f};
static const char *PLATFORM_RENDERER_geom_source_name(int source_id)
{
	switch (source_id)
	{
	case PLATFORM_RENDERER_GEOM_SRC_BOUND_QUAD:
		return "bound_quad";
	case PLATFORM_RENDERER_GEOM_SRC_BOUND_QUAD_CUSTOM:
		return "bound_quad_custom";
	case PLATFORM_RENDERER_GEOM_SRC_MULTITEXTURE_ARRAY:
		return "multitexture";
	case PLATFORM_RENDERER_GEOM_SRC_COLOURED_ARRAY:
		return "coloured_array";
	case PLATFORM_RENDERER_GEOM_SRC_PERSPECTIVE:
		return "perspective";
	case PLATFORM_RENDERER_GEOM_SRC_COLOURED_PERSPECTIVE:
		return "coloured_perspective";
	case PLATFORM_RENDERER_GEOM_SRC_SDL_CUSTOM:
		return "window_sprite";
	default:
		return "unknown";
	}
}

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
static char platform_renderer_backend_name[32] =
#if defined(WIZBALL_RENDER_BACKEND_GLES2)
	"GLES2";
#else
	"SDL";
#endif
static bool platform_renderer_backend_checked_env = false;
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

static void PLATFORM_RENDERER_refresh_backend_name(void)
{
	if (platform_renderer_backend_checked_env)
	{
		return;
	}

	platform_renderer_backend_checked_env = true;
	{
		const char *backend_env = getenv("WIZBALL_RENDERER_BACKEND");
		if ((backend_env != NULL) && (backend_env[0] != '\0'))
		{
			if ((strcmp(backend_env, "gles2") == 0) || (strcmp(backend_env, "GLES2") == 0))
			{
				strcpy(platform_renderer_backend_name, "GLES2");
			}
			else if ((strcmp(backend_env, "sdl") == 0) || (strcmp(backend_env, "SDL") == 0))
			{
				strcpy(platform_renderer_backend_name, "SDL");
			}
		}
	}
}

static void PLATFORM_RENDERER_refresh_sdl_stub_env_flags(void)
{
	PLATFORM_RENDERER_refresh_backend_name();
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

#if defined(WIZBALL_RENDER_BACKEND_GLES2)
static bool PLATFORM_RENDERER_gles2_is_ready(void)
{
	return
		PLATFORM_RENDERER_backend_targets_gles2() &&
		(platform_renderer_sdl_window != NULL) &&
		(platform_renderer_gles2_context != NULL) &&
		(platform_renderer_gles2_program != 0);
}

static void PLATFORM_RENDERER_gles2_apply_scissor_rect(int x, int y, int w, int h)
{
	int gl_y;

	if (!PLATFORM_RENDERER_gles2_is_ready())
	{
		return;
	}

	if ((w <= 0) || (h <= 0) || (platform_renderer_present_height <= 0))
	{
		glDisable(GL_SCISSOR_TEST);
		return;
	}

	gl_y = platform_renderer_present_height - (y + h);
	if (gl_y < 0)
	{
		h += gl_y;
		gl_y = 0;
	}
	if ((gl_y + h) > platform_renderer_present_height)
	{
		h = platform_renderer_present_height - gl_y;
	}
	if ((x < 0) || (y < 0) || ((x + w) > platform_renderer_present_width))
	{
		if (x < 0)
		{
			w += x;
			x = 0;
		}
		if ((x + w) > platform_renderer_present_width)
		{
			w = platform_renderer_present_width - x;
		}
	}
	if ((w <= 0) || (h <= 0))
	{
		glDisable(GL_SCISSOR_TEST);
		return;
	}

	glEnable(GL_SCISSOR_TEST);
	glScissor(x, gl_y, w, h);
}

static GLuint PLATFORM_RENDERER_gles2_compile_shader(GLenum type, const char *source)
{
	GLuint shader = glCreateShader(type);
	if (shader == 0)
	{
		return 0;
	}
	glShaderSource(shader, 1, &source, NULL);
	glCompileShader(shader);
	{
		GLint compiled = 0;
		glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
		if (!compiled)
		{
			glDeleteShader(shader);
			return 0;
		}
	}
	return shader;
}

static bool PLATFORM_RENDERER_gles2_init_context(void)
{
	static const char *vertex_source =
		"attribute vec2 a_pos;\n"
		"attribute vec2 a_uv;\n"
		"attribute vec4 a_color;\n"
		"uniform vec2 u_screen;\n"
		"uniform vec3 u_affine_row0;\n"
		"uniform vec3 u_affine_row1;\n"
		"varying vec2 v_uv;\n"
		"varying vec4 v_color;\n"
		"void main(void) {\n"
		"  vec2 screen_pos = vec2(dot(u_affine_row0, vec3(a_pos, 1.0)), dot(u_affine_row1, vec3(a_pos, 1.0)));\n"
		"  vec2 ndc = vec2((screen_pos.x / u_screen.x) * 2.0 - 1.0, 1.0 - (screen_pos.y / u_screen.y) * 2.0);\n"
		"  gl_Position = vec4(ndc, 0.0, 1.0);\n"
		"  v_uv = a_uv;\n"
		"  v_color = a_color;\n"
		"}\n";
	static const char *fragment_source =
		"precision mediump float;\n"
		"uniform sampler2D u_tex;\n"
		"uniform int u_masked;\n"
		"varying vec2 v_uv;\n"
		"varying vec4 v_color;\n"
		"void main(void) {\n"
		"  vec4 out_color = texture2D(u_tex, v_uv) * v_color;\n"
		"  if ((u_masked != 0) && (out_color.a < 0.5)) discard;\n"
		"  gl_FragColor = out_color;\n"
		"}\n";
	static const char *multitex_vertex_source =
		"attribute vec2 a_pos;\n"
		"attribute vec2 a_uv0;\n"
		"attribute vec2 a_uv1;\n"
		"attribute vec4 a_color;\n"
		"uniform vec2 u_screen;\n"
		"uniform vec3 u_affine_row0;\n"
		"uniform vec3 u_affine_row1;\n"
		"varying vec2 v_uv0;\n"
		"varying vec2 v_uv1;\n"
		"varying vec4 v_color;\n"
		"void main(void) {\n"
		"  vec2 screen_pos = vec2(dot(u_affine_row0, vec3(a_pos, 1.0)), dot(u_affine_row1, vec3(a_pos, 1.0)));\n"
		"  vec2 ndc = vec2((screen_pos.x / u_screen.x) * 2.0 - 1.0, 1.0 - (screen_pos.y / u_screen.y) * 2.0);\n"
		"  gl_Position = vec4(ndc, 0.0, 1.0);\n"
		"  v_uv0 = a_uv0;\n"
		"  v_uv1 = a_uv1;\n"
		"  v_color = a_color;\n"
		"}\n";
	static const char *multitex_fragment_source =
		"precision mediump float;\n"
		"uniform sampler2D u_tex0;\n"
		"uniform sampler2D u_tex1;\n"
		"uniform int u_mode;\n"
		"uniform int u_masked;\n"
		"varying vec2 v_uv0;\n"
		"varying vec2 v_uv1;\n"
		"varying vec4 v_color;\n"
		"void main(void) {\n"
		"  vec4 primary = texture2D(u_tex0, v_uv0) * v_color;\n"
		"  vec4 secondary = texture2D(u_tex1, v_uv1);\n"
		"  vec4 out_color;\n"
		"  if (u_mode == 3) {\n"
		"    out_color = vec4(secondary.rgb * primary.rgb, primary.a);\n"
		"  } else if (u_mode == 2) {\n"
		"    out_color = vec4(secondary.rgb, secondary.a * v_color.a);\n"
		"  } else {\n"
		"    out_color = vec4(secondary.rgb, secondary.a * primary.a);\n"
		"  }\n"
		"  if ((u_masked != 0) && (out_color.a < 0.5)) discard;\n"
		"  gl_FragColor = out_color;\n"
		"}\n";
	GLuint vs = 0;
	GLuint fs = 0;
	GLuint multitex_vs = 0;
	GLuint multitex_fs = 0;

	if (platform_renderer_gles2_context != NULL)
	{
		return true;
	}

	if (platform_renderer_sdl_window == NULL)
	{
		return false;
	}

	platform_renderer_gles2_context = SDL_GL_CreateContext(platform_renderer_sdl_window);
	if (platform_renderer_gles2_context == NULL)
	{
		snprintf(platform_renderer_sdl_status, sizeof(platform_renderer_sdl_status), "SDL_GL_CreateContext failed: %s", SDL_GetError());
		return false;
	}
	(void)SDL_GL_MakeCurrent(platform_renderer_sdl_window, platform_renderer_gles2_context);
	(void)SDL_GL_SetSwapInterval(0);

	vs = PLATFORM_RENDERER_gles2_compile_shader(GL_VERTEX_SHADER, vertex_source);
	fs = PLATFORM_RENDERER_gles2_compile_shader(GL_FRAGMENT_SHADER, fragment_source);
	multitex_vs = PLATFORM_RENDERER_gles2_compile_shader(GL_VERTEX_SHADER, multitex_vertex_source);
	multitex_fs = PLATFORM_RENDERER_gles2_compile_shader(GL_FRAGMENT_SHADER, multitex_fragment_source);
	if ((vs == 0) || (fs == 0) || (multitex_vs == 0) || (multitex_fs == 0))
	{
		if (vs != 0) glDeleteShader(vs);
		if (fs != 0) glDeleteShader(fs);
		if (multitex_vs != 0) glDeleteShader(multitex_vs);
		if (multitex_fs != 0) glDeleteShader(multitex_fs);
		snprintf(platform_renderer_sdl_status, sizeof(platform_renderer_sdl_status), "GLES2 shader compile failed.");
		return false;
	}

	platform_renderer_gles2_program = glCreateProgram();
	glAttachShader(platform_renderer_gles2_program, vs);
	glAttachShader(platform_renderer_gles2_program, fs);
	glBindAttribLocation(platform_renderer_gles2_program, 0, "a_pos");
	glBindAttribLocation(platform_renderer_gles2_program, 1, "a_uv");
	glBindAttribLocation(platform_renderer_gles2_program, 2, "a_color");
	glLinkProgram(platform_renderer_gles2_program);
	glDeleteShader(vs);
	glDeleteShader(fs);
	{
		GLint linked = 0;
		glGetProgramiv(platform_renderer_gles2_program, GL_LINK_STATUS, &linked);
		if (!linked)
		{
			glDeleteProgram(platform_renderer_gles2_program);
			platform_renderer_gles2_program = 0;
			snprintf(platform_renderer_sdl_status, sizeof(platform_renderer_sdl_status), "GLES2 program link failed.");
			return false;
		}
	}

	platform_renderer_gles2_multitex_program = glCreateProgram();
	glAttachShader(platform_renderer_gles2_multitex_program, multitex_vs);
	glAttachShader(platform_renderer_gles2_multitex_program, multitex_fs);
	glBindAttribLocation(platform_renderer_gles2_multitex_program, 0, "a_pos");
	glBindAttribLocation(platform_renderer_gles2_multitex_program, 1, "a_uv0");
	glBindAttribLocation(platform_renderer_gles2_multitex_program, 2, "a_uv1");
	glBindAttribLocation(platform_renderer_gles2_multitex_program, 3, "a_color");
	glLinkProgram(platform_renderer_gles2_multitex_program);
	glDeleteShader(multitex_vs);
	glDeleteShader(multitex_fs);
	{
		GLint linked = 0;
		glGetProgramiv(platform_renderer_gles2_multitex_program, GL_LINK_STATUS, &linked);
		if (!linked)
		{
			glDeleteProgram(platform_renderer_gles2_multitex_program);
			platform_renderer_gles2_multitex_program = 0;
			snprintf(platform_renderer_sdl_status, sizeof(platform_renderer_sdl_status), "GLES2 multitexture program link failed.");
			return false;
		}
	}

	platform_renderer_gles2_attr_pos = 0;
	platform_renderer_gles2_attr_uv = 1;
	platform_renderer_gles2_attr_color = 2;
	platform_renderer_gles2_uniform_screen = glGetUniformLocation(platform_renderer_gles2_program, "u_screen");
	platform_renderer_gles2_uniform_sampler = glGetUniformLocation(platform_renderer_gles2_program, "u_tex");
	platform_renderer_gles2_uniform_masked = glGetUniformLocation(platform_renderer_gles2_program, "u_masked");
	platform_renderer_gles2_uniform_affine_row0 = glGetUniformLocation(platform_renderer_gles2_program, "u_affine_row0");
	platform_renderer_gles2_uniform_affine_row1 = glGetUniformLocation(platform_renderer_gles2_program, "u_affine_row1");
	platform_renderer_gles2_multitex_attr_pos = 0;
	platform_renderer_gles2_multitex_attr_uv0 = 1;
	platform_renderer_gles2_multitex_attr_uv1 = 2;
	platform_renderer_gles2_multitex_attr_color = 3;
	platform_renderer_gles2_multitex_uniform_screen = glGetUniformLocation(platform_renderer_gles2_multitex_program, "u_screen");
	platform_renderer_gles2_multitex_uniform_sampler0 = glGetUniformLocation(platform_renderer_gles2_multitex_program, "u_tex0");
	platform_renderer_gles2_multitex_uniform_sampler1 = glGetUniformLocation(platform_renderer_gles2_multitex_program, "u_tex1");
	platform_renderer_gles2_multitex_uniform_mode = glGetUniformLocation(platform_renderer_gles2_multitex_program, "u_mode");
	platform_renderer_gles2_multitex_uniform_masked = glGetUniformLocation(platform_renderer_gles2_multitex_program, "u_masked");
	platform_renderer_gles2_multitex_uniform_affine_row0 = glGetUniformLocation(platform_renderer_gles2_multitex_program, "u_affine_row0");
	platform_renderer_gles2_multitex_uniform_affine_row1 = glGetUniformLocation(platform_renderer_gles2_multitex_program, "u_affine_row1");

	glGenTextures(1, &platform_renderer_gles2_white_texture);
	glBindTexture(GL_TEXTURE_2D, platform_renderer_gles2_white_texture);
	{
		const unsigned char pixel[4] = {255, 255, 255, 255};
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixel);
	}
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	glGenBuffers(1, &platform_renderer_gles2_stream_vbo);
	glGenBuffers(1, &platform_renderer_gles2_stream_ibo);

	glUseProgram(platform_renderer_gles2_program);
	glUniform1i(platform_renderer_gles2_uniform_sampler, 0);
	glUniform1i(platform_renderer_gles2_uniform_masked, 0);
	glUniform3f(platform_renderer_gles2_uniform_affine_row0, 1.0f, 0.0f, 0.0f);
	glUniform3f(platform_renderer_gles2_uniform_affine_row1, 0.0f, 1.0f, 0.0f);
	glUseProgram(platform_renderer_gles2_multitex_program);
	glUniform1i(platform_renderer_gles2_multitex_uniform_sampler0, 0);
	glUniform1i(platform_renderer_gles2_multitex_uniform_sampler1, 1);
	glUniform1i(platform_renderer_gles2_multitex_uniform_masked, 0);
	glUniform3f(platform_renderer_gles2_multitex_uniform_affine_row0, 1.0f, 0.0f, 0.0f);
	glUniform3f(platform_renderer_gles2_multitex_uniform_affine_row1, 0.0f, 1.0f, 0.0f);
	glDisable(GL_DEPTH_TEST);
	glDisable(GL_CULL_FACE);
	glEnable(GL_BLEND);
	PLATFORM_RENDERER_gles2_prewarm_registered_textures();
	return true;
}

static int PLATFORM_RENDERER_get_perspective_strip_count(float q, bool gles_optimized)
{
	const float perspective_eps = 0.0005f;
	float strength;

	if ((fabsf(q - 1.0f) <= perspective_eps) || (q <= perspective_eps))
	{
		return 1;
	}

	strength = fabsf(q - 1.0f);
	if (gles_optimized)
	{
		if (strength < 0.08f)
		{
			return 2;
		}
		if (strength < 0.18f)
		{
			return 4;
		}
		if (strength < 0.35f)
		{
			return 6;
		}
		if (strength < 0.70f)
		{
			return 8;
		}
		return 12;
	}

	if (strength < 0.08f)
	{
		return 4;
	}
	if (strength < 0.18f)
	{
		return 8;
	}
	if (strength < 0.35f)
	{
		return 12;
	}
	if (strength < 0.70f)
	{
		return 16;
	}
	return 24;
}

static void PLATFORM_RENDERER_gles2_set_affine_override_identity(void)
{
	platform_renderer_gles2_affine_override_active = false;
	platform_renderer_gles2_affine_row0[0] = 1.0f;
	platform_renderer_gles2_affine_row0[1] = 0.0f;
	platform_renderer_gles2_affine_row0[2] = 0.0f;
	platform_renderer_gles2_affine_row1[0] = 0.0f;
	platform_renderer_gles2_affine_row1[1] = 1.0f;
	platform_renderer_gles2_affine_row1[2] = 0.0f;
}

static void PLATFORM_RENDERER_gles2_set_affine_override_current_transform(void)
{
	platform_renderer_gles2_affine_override_active = true;
	platform_renderer_gles2_affine_row0[0] = platform_renderer_tx_a;
	platform_renderer_gles2_affine_row0[1] = platform_renderer_tx_c;
	platform_renderer_gles2_affine_row0[2] = platform_renderer_tx_x;
	platform_renderer_gles2_affine_row1[0] = -platform_renderer_tx_b;
	platform_renderer_gles2_affine_row1[1] = -platform_renderer_tx_d;
	platform_renderer_gles2_affine_row1[2] = (GLfloat)platform_renderer_present_height - platform_renderer_tx_y;
}

static void PLATFORM_RENDERER_gles2_capture_affine_state(
		bool *out_active,
		GLfloat out_row0[3],
		GLfloat out_row1[3])
{
	if (out_active != NULL)
	{
		*out_active = platform_renderer_gles2_affine_override_active;
	}
	if (out_row0 != NULL)
	{
		out_row0[0] = platform_renderer_gles2_affine_row0[0];
		out_row0[1] = platform_renderer_gles2_affine_row0[1];
		out_row0[2] = platform_renderer_gles2_affine_row0[2];
	}
	if (out_row1 != NULL)
	{
		out_row1[0] = platform_renderer_gles2_affine_row1[0];
		out_row1[1] = platform_renderer_gles2_affine_row1[1];
		out_row1[2] = platform_renderer_gles2_affine_row1[2];
	}
}

static void PLATFORM_RENDERER_gles2_apply_affine_state(
		bool active,
		const GLfloat row0[3],
		const GLfloat row1[3])
{
	platform_renderer_gles2_affine_override_active = active;
	if ((row0 != NULL) && (row1 != NULL))
	{
		platform_renderer_gles2_affine_row0[0] = row0[0];
		platform_renderer_gles2_affine_row0[1] = row0[1];
		platform_renderer_gles2_affine_row0[2] = row0[2];
		platform_renderer_gles2_affine_row1[0] = row1[0];
		platform_renderer_gles2_affine_row1[1] = row1[1];
		platform_renderer_gles2_affine_row1[2] = row1[2];
	}
}

static void PLATFORM_RENDERER_gles2_build_textured_batch_key(
		platform_renderer_gles2_textured_batch_key *batch_key,
		GLuint tex,
		unsigned int logical_texture_handle,
		bool repeat_s,
		bool repeat_t)
{
	if (batch_key == NULL)
	{
		return;
	}

	batch_key->tex = tex;
	batch_key->logical_texture_handle = logical_texture_handle;
	batch_key->repeat_s = repeat_s;
	batch_key->repeat_t = repeat_t;
	batch_key->filtered = platform_renderer_texture_filter_linear;
	batch_key->masked = platform_renderer_texture_masked;
	batch_key->blend_enabled = platform_renderer_blend_enabled;
	batch_key->blend_mode = platform_renderer_blend_mode;
	batch_key->affine_active = platform_renderer_gles2_affine_override_active;
	PLATFORM_RENDERER_gles2_capture_affine_state(
			NULL,
			batch_key->affine_row0,
			batch_key->affine_row1);
}

static void PLATFORM_RENDERER_gles2_shutdown(void)
{
	int i;
	for (i = 0; i < platform_renderer_texture_count; i++)
	{
		if (platform_renderer_texture_entries[i].gles2_texture != 0)
		{
			glDeleteTextures(1, &platform_renderer_texture_entries[i].gles2_texture);
			platform_renderer_texture_entries[i].gles2_texture = 0;
		}
	}
	if (platform_renderer_gles2_white_texture != 0)
	{
		glDeleteTextures(1, &platform_renderer_gles2_white_texture);
		platform_renderer_gles2_white_texture = 0;
	}
	if (platform_renderer_gles2_stream_vbo != 0)
	{
		glDeleteBuffers(1, &platform_renderer_gles2_stream_vbo);
		platform_renderer_gles2_stream_vbo = 0;
	}
	if (platform_renderer_gles2_stream_ibo != 0)
	{
		glDeleteBuffers(1, &platform_renderer_gles2_stream_ibo);
		platform_renderer_gles2_stream_ibo = 0;
	}
	if (platform_renderer_gles2_program != 0)
	{
		glDeleteProgram(platform_renderer_gles2_program);
		platform_renderer_gles2_program = 0;
	}
	if (platform_renderer_gles2_multitex_program != 0)
	{
		glDeleteProgram(platform_renderer_gles2_multitex_program);
		platform_renderer_gles2_multitex_program = 0;
	}
	if (platform_renderer_gles2_context != NULL)
	{
		SDL_GL_DeleteContext(platform_renderer_gles2_context);
		platform_renderer_gles2_context = NULL;
	}
}

static bool PLATFORM_RENDERER_build_gles2_texture_from_entry(platform_renderer_texture_entry *entry)
{
	if (!PLATFORM_RENDERER_gles2_is_ready() || (entry == NULL))
	{
		return false;
	}
	if (entry->gles2_texture != 0)
	{
		return true;
	}
	if ((entry->sdl_rgba_pixels == NULL) || (entry->width <= 0) || (entry->height <= 0))
	{
		return false;
	}

	glGenTextures(1, &entry->gles2_texture);
	glBindTexture(GL_TEXTURE_2D, entry->gles2_texture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, entry->width, entry->height, 0, GL_RGBA, GL_UNSIGNED_BYTE, entry->sdl_rgba_pixels);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	entry->gles2_filter_linear = false;
	entry->gles2_wrap_s_repeat = false;
	entry->gles2_wrap_t_repeat = false;
	return entry->gles2_texture != 0;
}

static void PLATFORM_RENDERER_gles2_prewarm_registered_textures(void)
{
	int i;

	if (!PLATFORM_RENDERER_gles2_is_ready() || (platform_renderer_texture_entries == NULL))
	{
		return;
	}

	for (i = 0; i < platform_renderer_texture_count; i++)
	{
		platform_renderer_texture_entry *entry = &platform_renderer_texture_entries[i];
		if (entry->gles2_texture == 0)
		{
			(void)PLATFORM_RENDERER_build_gles2_texture_from_entry(entry);
		}
	}
}

static void PLATFORM_RENDERER_gles2_apply_blend_state(void)
{
	glEnable(GL_BLEND);
	if (!platform_renderer_blend_enabled)
	{
		/*
		 * Match the SDL renderer path: textured draws still need ordinary alpha
		 * compositing even when the higher-level blend flag is "off", otherwise
		 * sprites with transparent/masked pixels turn into opaque black boxes or
		 * inherit stale multiply/add/subtract state from previous entities.
		 */
		glBlendEquation(GL_FUNC_ADD);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		return;
	}
	switch (platform_renderer_blend_mode)
	{
	case PLATFORM_RENDERER_BLEND_MODE_ADD:
		glBlendEquation(GL_FUNC_ADD);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE);
		break;
	case PLATFORM_RENDERER_BLEND_MODE_MULTIPLY:
		glBlendEquation(GL_FUNC_ADD);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		break;
	case PLATFORM_RENDERER_BLEND_MODE_SUBTRACT:
		glBlendEquation(GL_FUNC_REVERSE_SUBTRACT);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE);
		break;
	case PLATFORM_RENDERER_BLEND_MODE_ALPHA:
	default:
		glBlendEquation(GL_FUNC_ADD);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		break;
	}
}

static bool PLATFORM_RENDERER_gles2_submit_textured_geometry_gltex(
		GLuint tex,
		unsigned int logical_texture_handle,
		const SDL_Vertex *vertices,
		int vertex_count,
		const GLushort *indices,
		int index_count,
		int source_id,
		bool repeat_s,
		bool repeat_t)
{
	if (!PLATFORM_RENDERER_gles2_is_ready() || (vertices == NULL) || (indices == NULL) || (vertex_count <= 0) || (index_count <= 0))
	{
		return false;
	}
	if (tex == 0)
	{
		return false;
	}

	glViewport(0, 0, platform_renderer_present_width, platform_renderer_present_height);
	PLATFORM_RENDERER_gles2_apply_blend_state();
	glUseProgram(platform_renderer_gles2_program);
	glUniform2f(platform_renderer_gles2_uniform_screen, (GLfloat)platform_renderer_present_width, (GLfloat)platform_renderer_present_height);
	glUniform1i(platform_renderer_gles2_uniform_masked, platform_renderer_texture_masked ? 1 : 0);
	glUniform3f(
			platform_renderer_gles2_uniform_affine_row0,
			platform_renderer_gles2_affine_row0[0],
			platform_renderer_gles2_affine_row0[1],
			platform_renderer_gles2_affine_row0[2]);
	glUniform3f(
			platform_renderer_gles2_uniform_affine_row1,
			platform_renderer_gles2_affine_row1[0],
			platform_renderer_gles2_affine_row1[1],
			platform_renderer_gles2_affine_row1[2]);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, tex);
	if (logical_texture_handle != 0)
	{
		platform_renderer_texture_entry *entry = PLATFORM_RENDERER_get_texture_entry(logical_texture_handle);
		if (entry != NULL)
		{
			if (entry->gles2_filter_linear != platform_renderer_texture_filter_linear)
			{
				const GLint filter = platform_renderer_texture_filter_linear ? GL_LINEAR : GL_NEAREST;
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter);
				entry->gles2_filter_linear = platform_renderer_texture_filter_linear;
			}
			if (entry->gles2_wrap_s_repeat != repeat_s)
			{
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, repeat_s ? GL_REPEAT : GL_CLAMP_TO_EDGE);
				entry->gles2_wrap_s_repeat = repeat_s;
			}
			if (entry->gles2_wrap_t_repeat != repeat_t)
			{
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, repeat_t ? GL_REPEAT : GL_CLAMP_TO_EDGE);
				entry->gles2_wrap_t_repeat = repeat_t;
			}
		}
	}
	glEnableVertexAttribArray((GLuint)platform_renderer_gles2_attr_pos);
	glEnableVertexAttribArray((GLuint)platform_renderer_gles2_attr_uv);
	glEnableVertexAttribArray((GLuint)platform_renderer_gles2_attr_color);
	if ((platform_renderer_gles2_stream_vbo != 0) && (platform_renderer_gles2_stream_ibo != 0))
	{
		glBindBuffer(GL_ARRAY_BUFFER, platform_renderer_gles2_stream_vbo);
		glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)((size_t)vertex_count * sizeof(SDL_Vertex)), vertices, GL_DYNAMIC_DRAW);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, platform_renderer_gles2_stream_ibo);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, (GLsizeiptr)((size_t)index_count * sizeof(GLushort)), indices, GL_DYNAMIC_DRAW);
		glVertexAttribPointer((GLuint)platform_renderer_gles2_attr_pos, 2, GL_FLOAT, GL_FALSE, sizeof(SDL_Vertex), (const GLvoid *)offsetof(SDL_Vertex, position));
		glVertexAttribPointer((GLuint)platform_renderer_gles2_attr_uv, 2, GL_FLOAT, GL_FALSE, sizeof(SDL_Vertex), (const GLvoid *)offsetof(SDL_Vertex, tex_coord));
		glVertexAttribPointer((GLuint)platform_renderer_gles2_attr_color, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(SDL_Vertex), (const GLvoid *)offsetof(SDL_Vertex, color));
		glDrawElements(GL_TRIANGLES, index_count, GL_UNSIGNED_SHORT, 0);
		glBindBuffer(GL_ARRAY_BUFFER, 0);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
	}
	else
	{
		glVertexAttribPointer((GLuint)platform_renderer_gles2_attr_pos, 2, GL_FLOAT, GL_FALSE, sizeof(SDL_Vertex), &vertices[0].position.x);
		glVertexAttribPointer((GLuint)platform_renderer_gles2_attr_uv, 2, GL_FLOAT, GL_FALSE, sizeof(SDL_Vertex), &vertices[0].tex_coord.x);
		glVertexAttribPointer((GLuint)platform_renderer_gles2_attr_color, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(SDL_Vertex), &vertices[0].color.r);
		glDrawElements(GL_TRIANGLES, index_count, GL_UNSIGNED_SHORT, indices);
	}
	platform_renderer_sdl_native_draw_count++;
	if (logical_texture_handle != 0)
	{
		platform_renderer_sdl_native_textured_draw_count++;
	}
	if ((source_id > 0) && (source_id < PLATFORM_RENDERER_GEOM_SRC_COUNT))
	{
		platform_renderer_gles2_draw_sources[source_id]++;
	}
	return true;
}

static void PLATFORM_RENDERER_gles2_flush_textured_batch(void)
{
	bool prev_affine_active = false;
	GLfloat prev_affine_row0[3] = {1.0f, 0.0f, 0.0f};
	GLfloat prev_affine_row1[3] = {0.0f, 1.0f, 0.0f};

	if (!platform_renderer_gles2_textured_batch_active ||
			(platform_renderer_gles2_textured_batch_vertex_count <= 0) ||
			(platform_renderer_gles2_textured_batch_index_count <= 0))
	{
		platform_renderer_gles2_textured_batch_active = false;
		platform_renderer_gles2_textured_batch_vertex_count = 0;
		platform_renderer_gles2_textured_batch_index_count = 0;
		return;
	}

	platform_renderer_blend_enabled = platform_renderer_gles2_textured_batch_state.blend_enabled;
	platform_renderer_blend_mode = platform_renderer_gles2_textured_batch_state.blend_mode;
	platform_renderer_texture_filter_linear = platform_renderer_gles2_textured_batch_state.filtered;
	platform_renderer_texture_masked = platform_renderer_gles2_textured_batch_state.masked;
	PLATFORM_RENDERER_gles2_capture_affine_state(&prev_affine_active, prev_affine_row0, prev_affine_row1);
	PLATFORM_RENDERER_gles2_apply_affine_state(
			platform_renderer_gles2_textured_batch_state.affine_active,
			platform_renderer_gles2_textured_batch_state.affine_row0,
			platform_renderer_gles2_textured_batch_state.affine_row1);

	(void)PLATFORM_RENDERER_gles2_submit_textured_geometry_gltex(
			platform_renderer_gles2_textured_batch_state.tex,
			platform_renderer_gles2_textured_batch_state.logical_texture_handle,
			platform_renderer_gles2_textured_batch_vertices,
			platform_renderer_gles2_textured_batch_vertex_count,
			platform_renderer_gles2_textured_batch_indices,
			platform_renderer_gles2_textured_batch_index_count,
			PLATFORM_RENDERER_GEOM_SRC_NONE,
			platform_renderer_gles2_textured_batch_state.repeat_s,
			platform_renderer_gles2_textured_batch_state.repeat_t);
	PLATFORM_RENDERER_gles2_apply_affine_state(prev_affine_active, prev_affine_row0, prev_affine_row1);

	platform_renderer_gles2_textured_batch_active = false;
	platform_renderer_gles2_textured_batch_vertex_count = 0;
	platform_renderer_gles2_textured_batch_index_count = 0;
}

static bool PLATFORM_RENDERER_gles2_draw_textured_geometry_gltex(
		GLuint tex,
		unsigned int logical_texture_handle,
		const SDL_Vertex *vertices,
		int vertex_count,
		const int *indices,
		int index_count,
		int source_id,
		bool repeat_s,
		bool repeat_t)
{
	static const GLushort standard_quad_indices[6] = {0, 1, 2, 0, 2, 3};
	GLushort stack_index_data[PLATFORM_RENDERER_MAX_GEOMETRY_INDICES];
	GLushort *index_data = stack_index_data;
	bool used_heap_indices = false;
	bool can_batch = false;
	bool standard_quad = false;
	int i;
	platform_renderer_gles2_textured_batch_key batch_key;

	if (!PLATFORM_RENDERER_gles2_is_ready() || (vertices == NULL) || (indices == NULL) || (vertex_count <= 0) || (index_count <= 0))
	{
		return false;
	}
	if (tex == 0)
	{
		return false;
	}

	standard_quad =
		(vertex_count == 4) &&
		(index_count == 6) &&
		(indices[0] == 0) &&
		(indices[1] == 1) &&
		(indices[2] == 2) &&
		(indices[3] == 0) &&
		(indices[4] == 2) &&
		(indices[5] == 3);

	if (!standard_quad && (index_count > PLATFORM_RENDERER_MAX_GEOMETRY_INDICES))
	{
		index_data = (GLushort *)malloc((size_t)index_count * sizeof(GLushort));
		if (index_data == NULL)
		{
			return false;
		}
		used_heap_indices = true;
	}
	if (standard_quad)
	{
		index_data = (GLushort *)standard_quad_indices;
	}
	else for (i = 0; i < index_count; i++)
	{
		index_data[i] = (GLushort)indices[i];
	}

	PLATFORM_RENDERER_gles2_build_textured_batch_key(
			&batch_key,
			tex,
			logical_texture_handle,
			repeat_s,
			repeat_t);

	can_batch =
		(vertex_count <= PLATFORM_RENDERER_GLES2_BATCH_MAX_VERTICES) &&
		(index_count <= PLATFORM_RENDERER_GLES2_BATCH_MAX_INDICES);

	if (can_batch)
	{
		const bool key_matches =
			platform_renderer_gles2_textured_batch_active &&
			(platform_renderer_gles2_textured_batch_state.tex == batch_key.tex) &&
			(platform_renderer_gles2_textured_batch_state.logical_texture_handle == batch_key.logical_texture_handle) &&
			(platform_renderer_gles2_textured_batch_state.repeat_s == batch_key.repeat_s) &&
			(platform_renderer_gles2_textured_batch_state.repeat_t == batch_key.repeat_t) &&
			(platform_renderer_gles2_textured_batch_state.filtered == batch_key.filtered) &&
			(platform_renderer_gles2_textured_batch_state.masked == batch_key.masked) &&
			(platform_renderer_gles2_textured_batch_state.blend_enabled == batch_key.blend_enabled) &&
			(platform_renderer_gles2_textured_batch_state.blend_mode == batch_key.blend_mode) &&
			(platform_renderer_gles2_textured_batch_state.affine_active == batch_key.affine_active) &&
			(platform_renderer_gles2_textured_batch_state.affine_row0[0] == batch_key.affine_row0[0]) &&
			(platform_renderer_gles2_textured_batch_state.affine_row0[1] == batch_key.affine_row0[1]) &&
			(platform_renderer_gles2_textured_batch_state.affine_row0[2] == batch_key.affine_row0[2]) &&
			(platform_renderer_gles2_textured_batch_state.affine_row1[0] == batch_key.affine_row1[0]) &&
			(platform_renderer_gles2_textured_batch_state.affine_row1[1] == batch_key.affine_row1[1]) &&
			(platform_renderer_gles2_textured_batch_state.affine_row1[2] == batch_key.affine_row1[2]);
		const bool fits =
			(platform_renderer_gles2_textured_batch_vertex_count + vertex_count <= PLATFORM_RENDERER_GLES2_BATCH_MAX_VERTICES) &&
			(platform_renderer_gles2_textured_batch_index_count + index_count <= PLATFORM_RENDERER_GLES2_BATCH_MAX_INDICES);

		if (!key_matches || !fits)
		{
			if (!key_matches)
			{
				platform_renderer_gles2_flush_key_mismatch_count++;
			}
			if (!fits)
			{
				platform_renderer_gles2_flush_batch_full_count++;
			}
			PLATFORM_RENDERER_gles2_flush_textured_batch();
		}

		if (!platform_renderer_gles2_textured_batch_active)
		{
			platform_renderer_gles2_textured_batch_active = true;
			platform_renderer_gles2_textured_batch_state = batch_key;
		}

		if ((platform_renderer_gles2_textured_batch_vertex_count + vertex_count <= PLATFORM_RENDERER_GLES2_BATCH_MAX_VERTICES) &&
				(platform_renderer_gles2_textured_batch_index_count + index_count <= PLATFORM_RENDERER_GLES2_BATCH_MAX_INDICES))
		{
			const int base_vertex = platform_renderer_gles2_textured_batch_vertex_count;
			memcpy(
					&platform_renderer_gles2_textured_batch_vertices[base_vertex],
					vertices,
					(size_t)vertex_count * sizeof(SDL_Vertex));
			if (standard_quad)
			{
				platform_renderer_gles2_textured_batch_indices[platform_renderer_gles2_textured_batch_index_count + 0] = (GLushort)(base_vertex + 0);
				platform_renderer_gles2_textured_batch_indices[platform_renderer_gles2_textured_batch_index_count + 1] = (GLushort)(base_vertex + 1);
				platform_renderer_gles2_textured_batch_indices[platform_renderer_gles2_textured_batch_index_count + 2] = (GLushort)(base_vertex + 2);
				platform_renderer_gles2_textured_batch_indices[platform_renderer_gles2_textured_batch_index_count + 3] = (GLushort)(base_vertex + 0);
				platform_renderer_gles2_textured_batch_indices[platform_renderer_gles2_textured_batch_index_count + 4] = (GLushort)(base_vertex + 2);
				platform_renderer_gles2_textured_batch_indices[platform_renderer_gles2_textured_batch_index_count + 5] = (GLushort)(base_vertex + 3);
			}
			else for (i = 0; i < index_count; i++)
			{
				platform_renderer_gles2_textured_batch_indices[platform_renderer_gles2_textured_batch_index_count + i] =
					(GLushort)(base_vertex + index_data[i]);
			}
			platform_renderer_gles2_textured_batch_vertex_count += vertex_count;
			platform_renderer_gles2_textured_batch_index_count += index_count;
			if (used_heap_indices)
			{
				free(index_data);
			}
			(void)source_id;
			return true;
		}
	}

	PLATFORM_RENDERER_gles2_flush_textured_batch();
	{
		const bool ok = PLATFORM_RENDERER_gles2_submit_textured_geometry_gltex(
				tex,
				logical_texture_handle,
				vertices,
				vertex_count,
				index_data,
				index_count,
				source_id,
				repeat_s,
				repeat_t);
		if (used_heap_indices)
		{
			free(index_data);
		}
		return ok;
	}
}

static bool PLATFORM_RENDERER_gles2_draw_textured_quad_batch_gltex(
		GLuint tex,
		unsigned int logical_texture_handle,
		const SDL_Vertex *vertices,
		int quad_count,
		int source_id,
		bool repeat_s,
		bool repeat_t)
{
	platform_renderer_gles2_textured_batch_key batch_key;
	const int vertex_count = quad_count * 4;
	const int index_count = quad_count * 6;
	GLushort stack_indices[PLATFORM_RENDERER_MAX_GEOMETRY_INDICES];
	GLushort *indices = stack_indices;
	bool used_heap_indices = false;
	int quad_index;

	if (!PLATFORM_RENDERER_gles2_is_ready() ||
			(tex == 0) ||
			(vertices == NULL) ||
			(quad_count <= 0))
	{
		return false;
	}

	PLATFORM_RENDERER_gles2_build_textured_batch_key(
			&batch_key,
			tex,
			logical_texture_handle,
			repeat_s,
			repeat_t);

	if ((vertex_count <= PLATFORM_RENDERER_GLES2_BATCH_MAX_VERTICES) &&
			(index_count <= PLATFORM_RENDERER_GLES2_BATCH_MAX_INDICES))
	{
		const bool key_matches =
			platform_renderer_gles2_textured_batch_active &&
			(platform_renderer_gles2_textured_batch_state.tex == batch_key.tex) &&
			(platform_renderer_gles2_textured_batch_state.logical_texture_handle == batch_key.logical_texture_handle) &&
			(platform_renderer_gles2_textured_batch_state.repeat_s == batch_key.repeat_s) &&
			(platform_renderer_gles2_textured_batch_state.repeat_t == batch_key.repeat_t) &&
			(platform_renderer_gles2_textured_batch_state.filtered == batch_key.filtered) &&
			(platform_renderer_gles2_textured_batch_state.masked == batch_key.masked) &&
			(platform_renderer_gles2_textured_batch_state.blend_enabled == batch_key.blend_enabled) &&
			(platform_renderer_gles2_textured_batch_state.blend_mode == batch_key.blend_mode) &&
			(platform_renderer_gles2_textured_batch_state.affine_active == batch_key.affine_active) &&
			(platform_renderer_gles2_textured_batch_state.affine_row0[0] == batch_key.affine_row0[0]) &&
			(platform_renderer_gles2_textured_batch_state.affine_row0[1] == batch_key.affine_row0[1]) &&
			(platform_renderer_gles2_textured_batch_state.affine_row0[2] == batch_key.affine_row0[2]) &&
			(platform_renderer_gles2_textured_batch_state.affine_row1[0] == batch_key.affine_row1[0]) &&
			(platform_renderer_gles2_textured_batch_state.affine_row1[1] == batch_key.affine_row1[1]) &&
			(platform_renderer_gles2_textured_batch_state.affine_row1[2] == batch_key.affine_row1[2]);
		const bool fits =
			(platform_renderer_gles2_textured_batch_vertex_count + vertex_count <= PLATFORM_RENDERER_GLES2_BATCH_MAX_VERTICES) &&
			(platform_renderer_gles2_textured_batch_index_count + index_count <= PLATFORM_RENDERER_GLES2_BATCH_MAX_INDICES);

		if (!key_matches || !fits)
		{
			PLATFORM_RENDERER_gles2_flush_textured_batch();
		}

		if (!platform_renderer_gles2_textured_batch_active)
		{
			platform_renderer_gles2_textured_batch_active = true;
			platform_renderer_gles2_textured_batch_state = batch_key;
		}

		if ((platform_renderer_gles2_textured_batch_vertex_count + vertex_count <= PLATFORM_RENDERER_GLES2_BATCH_MAX_VERTICES) &&
				(platform_renderer_gles2_textured_batch_index_count + index_count <= PLATFORM_RENDERER_GLES2_BATCH_MAX_INDICES))
		{
			const int base_vertex = platform_renderer_gles2_textured_batch_vertex_count;
			memcpy(
					&platform_renderer_gles2_textured_batch_vertices[base_vertex],
					vertices,
					(size_t)vertex_count * sizeof(SDL_Vertex));
			for (quad_index = 0; quad_index < quad_count; quad_index++)
			{
				const int quad_base_vertex = base_vertex + (quad_index * 4);
				const int quad_base_index = platform_renderer_gles2_textured_batch_index_count + (quad_index * 6);
				platform_renderer_gles2_textured_batch_indices[quad_base_index + 0] = (GLushort)(quad_base_vertex + 0);
				platform_renderer_gles2_textured_batch_indices[quad_base_index + 1] = (GLushort)(quad_base_vertex + 1);
				platform_renderer_gles2_textured_batch_indices[quad_base_index + 2] = (GLushort)(quad_base_vertex + 2);
				platform_renderer_gles2_textured_batch_indices[quad_base_index + 3] = (GLushort)(quad_base_vertex + 0);
				platform_renderer_gles2_textured_batch_indices[quad_base_index + 4] = (GLushort)(quad_base_vertex + 2);
				platform_renderer_gles2_textured_batch_indices[quad_base_index + 5] = (GLushort)(quad_base_vertex + 3);
			}
			platform_renderer_gles2_textured_batch_vertex_count += vertex_count;
			platform_renderer_gles2_textured_batch_index_count += index_count;
			(void)source_id;
			return true;
		}
	}

	if (index_count > PLATFORM_RENDERER_MAX_GEOMETRY_INDICES)
	{
		indices = (GLushort *)malloc((size_t)index_count * sizeof(GLushort));
		if (indices == NULL)
		{
			return false;
		}
		used_heap_indices = true;
	}
	for (quad_index = 0; quad_index < quad_count; quad_index++)
	{
		const int base_vertex = quad_index * 4;
		const int base_index = quad_index * 6;
		indices[base_index + 0] = (GLushort)(base_vertex + 0);
		indices[base_index + 1] = (GLushort)(base_vertex + 1);
		indices[base_index + 2] = (GLushort)(base_vertex + 2);
		indices[base_index + 3] = (GLushort)(base_vertex + 0);
		indices[base_index + 4] = (GLushort)(base_vertex + 2);
		indices[base_index + 5] = (GLushort)(base_vertex + 3);
	}

	PLATFORM_RENDERER_gles2_flush_textured_batch();
	{
		const bool ok = PLATFORM_RENDERER_gles2_submit_textured_geometry_gltex(
				tex,
				logical_texture_handle,
				vertices,
				vertex_count,
				indices,
				index_count,
				source_id,
				repeat_s,
				repeat_t);
		if (used_heap_indices)
		{
			free(indices);
		}
		return ok;
	}
}

static bool PLATFORM_RENDERER_gles2_draw_textured_geometry(unsigned int texture_handle, const SDL_Vertex *vertices, int vertex_count, const int *indices, int index_count, int source_id)
{
	platform_renderer_texture_entry *entry;
	GLuint tex;
	SDL_Vertex local_vertices[PLATFORM_RENDERER_MAX_GEOMETRY_VERTICES];
	SDL_Vertex *work_vertices = local_vertices;
	const bool wrapped_source =
			(source_id == PLATFORM_RENDERER_GEOM_SRC_BOUND_QUAD) ||
			(source_id == PLATFORM_RENDERER_GEOM_SRC_BOUND_QUAD_CUSTOM) ||
			(source_id == PLATFORM_RENDERER_GEOM_SRC_PERSPECTIVE) ||
			(source_id == PLATFORM_RENDERER_GEOM_SRC_COLOURED_PERSPECTIVE);
	bool repeat_s = false;
	bool repeat_t = false;
	int i;

	if (texture_handle == 0)
	{
		tex = platform_renderer_gles2_white_texture;
		return PLATFORM_RENDERER_gles2_draw_textured_geometry_gltex(tex, 0, vertices, vertex_count, indices, index_count, source_id, false, false);
	}

	entry = PLATFORM_RENDERER_get_texture_entry(texture_handle);
	if ((entry == NULL) || !PLATFORM_RENDERER_build_gles2_texture_from_entry(entry))
	{
		return false;
	}
	tex = entry->gles2_texture;

	for (i = 0; i < vertex_count; i++)
	{
		const float sdl_v = PLATFORM_RENDERER_convert_v_to_sdl(vertices[i].tex_coord.y);
		if ((vertices[i].tex_coord.x < 0.0f) || (vertices[i].tex_coord.x > 1.0f))
		{
			repeat_s = true;
		}
		if ((sdl_v < 0.0f) || (sdl_v > 1.0f))
		{
			repeat_t = true;
		}
	}

	if ((repeat_s || repeat_t) &&
			wrapped_source &&
			PLATFORM_RENDERER_gles2_draw_wrapped_quad_geometry_gltex(
				tex,
				texture_handle,
				vertices,
				vertex_count,
				indices,
				index_count,
				source_id))
	{
		return true;
	}

	if (vertex_count > PLATFORM_RENDERER_MAX_GEOMETRY_VERTICES)
	{
		work_vertices = (SDL_Vertex *)malloc((size_t)vertex_count * sizeof(SDL_Vertex));
		if (work_vertices == NULL)
		{
			return false;
		}
	}

	for (i = 0; i < vertex_count; i++)
	{
		work_vertices[i] = vertices[i];
		work_vertices[i].tex_coord.y = PLATFORM_RENDERER_convert_v_to_sdl(vertices[i].tex_coord.y);
	}

	{
		const bool ok = PLATFORM_RENDERER_gles2_draw_textured_geometry_gltex(
				tex, texture_handle, work_vertices, vertex_count, indices, index_count, source_id, repeat_s, repeat_t);
		if (work_vertices != local_vertices)
		{
			free(work_vertices);
		}
		return ok;
	}
}

static bool PLATFORM_RENDERER_gles2_draw_textured_subrect_geometry(
		platform_renderer_texture_entry *entry,
		unsigned int logical_texture_handle,
		const SDL_Rect *src_rect,
		const SDL_Vertex *vertices,
		int vertex_count,
		const int *indices,
		int index_count,
	int source_id)
{
	if (!PLATFORM_RENDERER_gles2_is_ready() ||
			(entry == NULL) ||
			(src_rect == NULL) ||
			(vertices == NULL) ||
			(indices == NULL) ||
			(entry->sdl_rgba_pixels == NULL) ||
			(vertex_count <= 0) ||
			(index_count <= 0) ||
			(src_rect->w <= 0) ||
			(src_rect->h <= 0))
	{
		return false;
	}

	if (!PLATFORM_RENDERER_build_gles2_texture_from_entry(entry))
	{
		return false;
	}

	return PLATFORM_RENDERER_gles2_draw_textured_subrect_geometry_gltex(
			entry,
			entry->gles2_texture,
			logical_texture_handle,
			src_rect,
			vertices,
			vertex_count,
			indices,
			index_count,
			source_id);
}

static bool PLATFORM_RENDERER_gles2_draw_textured_subrect_geometry_gltex(
		platform_renderer_texture_entry *entry,
		GLuint tex,
		unsigned int logical_texture_handle,
		const SDL_Rect *src_rect,
		const SDL_Vertex *vertices,
		int vertex_count,
		const int *indices,
		int index_count,
		int source_id)
{
	SDL_Vertex local_vertices[PLATFORM_RENDERER_MAX_GEOMETRY_VERTICES];
	SDL_Vertex *work_vertices = local_vertices;
	int i;
	float src_u_min;
	float src_u_max;
	float src_v_min;
	float src_v_max;

	if ((entry == NULL) ||
			(src_rect == NULL) ||
			(vertices == NULL) ||
			(indices == NULL) ||
			(tex == 0) ||
			(vertex_count <= 0) ||
			(index_count <= 0) ||
			(src_rect->w <= 0) ||
			(src_rect->h <= 0))
	{
		return false;
	}

	if (vertex_count > PLATFORM_RENDERER_MAX_GEOMETRY_VERTICES)
	{
		work_vertices = (SDL_Vertex *)malloc((size_t)vertex_count * sizeof(SDL_Vertex));
		if (work_vertices == NULL)
		{
			return false;
		}
	}

	src_u_min = (float)src_rect->x / (float)entry->width;
	src_u_max = (float)(src_rect->x + src_rect->w) / (float)entry->width;
	src_v_min = (float)src_rect->y / (float)entry->height;
	src_v_max = (float)(src_rect->y + src_rect->h) / (float)entry->height;

	for (i = 0; i < vertex_count; i++)
	{
		work_vertices[i] = vertices[i];
		work_vertices[i].tex_coord.y = PLATFORM_RENDERER_convert_v_to_sdl(vertices[i].tex_coord.y);
		if (work_vertices[i].tex_coord.x < src_u_min) work_vertices[i].tex_coord.x = src_u_min;
		if (work_vertices[i].tex_coord.x > src_u_max) work_vertices[i].tex_coord.x = src_u_max;
		if (work_vertices[i].tex_coord.y < src_v_min) work_vertices[i].tex_coord.y = src_v_min;
		if (work_vertices[i].tex_coord.y > src_v_max) work_vertices[i].tex_coord.y = src_v_max;
	}

	{
		const bool ok = PLATFORM_RENDERER_gles2_draw_textured_geometry_gltex(
				tex,
				logical_texture_handle,
				work_vertices,
				vertex_count,
				indices,
				index_count,
				source_id,
				false,
				false);
		if (work_vertices != local_vertices)
		{
			free(work_vertices);
		}
		return ok;
	}
}


static bool PLATFORM_RENDERER_gles2_draw_coloured_line_segment(
		float x1, float y1, float x2, float y2,
		Uint8 r, Uint8 g, Uint8 b, Uint8 a)
{
	SDL_Vertex vertices[4];
	const int indices[6] = {0, 1, 2, 0, 2, 3};
	float dx;
	float dy;
	float length;
	float nx;
	float ny;
	const float half_width = 0.5f;
	int i;

	if (!PLATFORM_RENDERER_gles2_is_ready() || (platform_renderer_present_height <= 0))
	{
		return false;
	}
	if (a == 0)
	{
		return true;
	}

	dx = x2 - x1;
	dy = y2 - y1;
	length = sqrtf((dx * dx) + (dy * dy));
	if (length < 0.0001f)
	{
		const float px = x1;
		const float py = y1;
		vertices[0].position.x = px - 0.5f; vertices[0].position.y = py - 0.5f;
		vertices[1].position.x = px + 0.5f; vertices[1].position.y = py - 0.5f;
		vertices[2].position.x = px + 0.5f; vertices[2].position.y = py + 0.5f;
		vertices[3].position.x = px - 0.5f; vertices[3].position.y = py + 0.5f;
	}
	else
	{
		nx = -dy / length;
		ny = dx / length;
		vertices[0].position.x = x1 + (nx * half_width); vertices[0].position.y = y1 + (ny * half_width);
		vertices[1].position.x = x1 - (nx * half_width); vertices[1].position.y = y1 - (ny * half_width);
		vertices[2].position.x = x2 - (nx * half_width); vertices[2].position.y = y2 - (ny * half_width);
		vertices[3].position.x = x2 + (nx * half_width); vertices[3].position.y = y2 + (ny * half_width);
	}

	for (i = 0; i < 4; i++)
	{
		vertices[i].tex_coord.x = 0.5f;
		vertices[i].tex_coord.y = 0.5f;
		vertices[i].color.r = r;
		vertices[i].color.g = g;
		vertices[i].color.b = b;
		vertices[i].color.a = a;
	}

	return PLATFORM_RENDERER_gles2_draw_textured_geometry(0, vertices, 4, indices, 6, PLATFORM_RENDERER_GEOM_SRC_NONE);
}

static bool PLATFORM_RENDERER_gles2_draw_multitextured_geometry(
		unsigned int texture0_handle,
		unsigned int texture1_handle,
		int combiner_mode,
		const float *positions,
		const float *uv0,
		const float *uv1,
		const unsigned char *colours,
		int vertex_count,
		const int *indices,
		int index_count,
		int source_id)
{
	platform_renderer_texture_entry *entry0;
	platform_renderer_texture_entry *entry1;
	platform_renderer_gles2_multitex_vertex vertex_stack[PLATFORM_RENDERER_MAX_GEOMETRY_VERTICES];
	platform_renderer_gles2_multitex_vertex *vertex_data = vertex_stack;
	GLushort index_stack[PLATFORM_RENDERER_MAX_GEOMETRY_INDICES];
	GLushort *index_data = index_stack;
	bool used_heap_vertices = false;
	bool used_heap_indices = false;
	bool repeat0_s = false;
	bool repeat0_t = false;
	bool repeat1_s = false;
	bool repeat1_t = false;
	int i;

	if (!PLATFORM_RENDERER_gles2_is_ready() ||
			(platform_renderer_gles2_multitex_program == 0) ||
			(positions == NULL) ||
			(uv0 == NULL) ||
			(uv1 == NULL) ||
			(colours == NULL) ||
			(indices == NULL) ||
			(vertex_count <= 0) ||
			(index_count <= 0) ||
			(texture0_handle == 0) ||
			(texture1_handle == 0))
	{
		return false;
	}

	entry0 = PLATFORM_RENDERER_get_texture_entry(texture0_handle);
	entry1 = PLATFORM_RENDERER_get_texture_entry(texture1_handle);
	if ((entry0 == NULL) || (entry1 == NULL) ||
			!PLATFORM_RENDERER_build_gles2_texture_from_entry(entry0) ||
			!PLATFORM_RENDERER_build_gles2_texture_from_entry(entry1))
	{
		return false;
	}
	if (vertex_count > PLATFORM_RENDERER_MAX_GEOMETRY_VERTICES)
	{
		vertex_data = (platform_renderer_gles2_multitex_vertex *)malloc((size_t)vertex_count * sizeof(platform_renderer_gles2_multitex_vertex));
		if (vertex_data == NULL)
		{
			return false;
		}
		used_heap_vertices = true;
	}
	for (i = 0; i < vertex_count; i++)
	{
		const float p_u = uv0[(i * 2) + 0];
		const float p_v = PLATFORM_RENDERER_convert_v_to_sdl(uv0[(i * 2) + 1]);
		const float s_u = uv1[(i * 2) + 0];
		const float s_v = PLATFORM_RENDERER_convert_v_to_sdl(uv1[(i * 2) + 1]);
		vertex_data[i].position[0] = positions[(i * 2) + 0];
		vertex_data[i].position[1] = positions[(i * 2) + 1];
		vertex_data[i].uv0[0] = p_u;
		vertex_data[i].uv0[1] = p_v;
		vertex_data[i].uv1[0] = s_u;
		vertex_data[i].uv1[1] = s_v;
		vertex_data[i].color[0] = colours[(i * 4) + 0];
		vertex_data[i].color[1] = colours[(i * 4) + 1];
		vertex_data[i].color[2] = colours[(i * 4) + 2];
		vertex_data[i].color[3] = colours[(i * 4) + 3];
		if ((p_u < 0.0f) || (p_u > 1.0f)) repeat0_s = true;
		if ((p_v < 0.0f) || (p_v > 1.0f)) repeat0_t = true;
		if ((s_u < 0.0f) || (s_u > 1.0f)) repeat1_s = true;
		if ((s_v < 0.0f) || (s_v > 1.0f)) repeat1_t = true;
	}

	if (index_count > PLATFORM_RENDERER_MAX_GEOMETRY_INDICES)
	{
		index_data = (GLushort *)malloc((size_t)index_count * sizeof(GLushort));
		if (index_data == NULL)
		{
			if (used_heap_vertices)
			{
				free(vertex_data);
			}
			return false;
		}
		used_heap_indices = true;
	}
	for (i = 0; i < index_count; i++)
	{
		index_data[i] = (GLushort)indices[i];
	}

	glViewport(0, 0, platform_renderer_present_width, platform_renderer_present_height);
	PLATFORM_RENDERER_gles2_apply_blend_state();
	glUseProgram(platform_renderer_gles2_multitex_program);
	glUniform2f(platform_renderer_gles2_multitex_uniform_screen, (GLfloat)platform_renderer_present_width, (GLfloat)platform_renderer_present_height);
	glUniform1i(platform_renderer_gles2_multitex_uniform_mode, combiner_mode);
	glUniform1i(platform_renderer_gles2_multitex_uniform_masked, platform_renderer_texture_masked ? 1 : 0);
	glUniform3f(
			platform_renderer_gles2_multitex_uniform_affine_row0,
			platform_renderer_gles2_affine_row0[0],
			platform_renderer_gles2_affine_row0[1],
			platform_renderer_gles2_affine_row0[2]);
	glUniform3f(
			platform_renderer_gles2_multitex_uniform_affine_row1,
			platform_renderer_gles2_affine_row1[0],
			platform_renderer_gles2_affine_row1[1],
			platform_renderer_gles2_affine_row1[2]);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, entry0->gles2_texture);
	if (entry0->gles2_filter_linear != platform_renderer_texture_filter_linear)
	{
		const GLint filter = platform_renderer_texture_filter_linear ? GL_LINEAR : GL_NEAREST;
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter);
		entry0->gles2_filter_linear = platform_renderer_texture_filter_linear;
	}
	if (entry0->gles2_wrap_s_repeat != repeat0_s)
	{
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, repeat0_s ? GL_REPEAT : GL_CLAMP_TO_EDGE);
		entry0->gles2_wrap_s_repeat = repeat0_s;
	}
	if (entry0->gles2_wrap_t_repeat != repeat0_t)
	{
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, repeat0_t ? GL_REPEAT : GL_CLAMP_TO_EDGE);
		entry0->gles2_wrap_t_repeat = repeat0_t;
	}
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, entry1->gles2_texture);
	if (entry1->gles2_filter_linear != platform_renderer_texture_filter_linear)
	{
		const GLint filter = platform_renderer_texture_filter_linear ? GL_LINEAR : GL_NEAREST;
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter);
		entry1->gles2_filter_linear = platform_renderer_texture_filter_linear;
	}
	if (entry1->gles2_wrap_s_repeat != repeat1_s)
	{
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, repeat1_s ? GL_REPEAT : GL_CLAMP_TO_EDGE);
		entry1->gles2_wrap_s_repeat = repeat1_s;
	}
	if (entry1->gles2_wrap_t_repeat != repeat1_t)
	{
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, repeat1_t ? GL_REPEAT : GL_CLAMP_TO_EDGE);
		entry1->gles2_wrap_t_repeat = repeat1_t;
	}
	glEnableVertexAttribArray((GLuint)platform_renderer_gles2_multitex_attr_pos);
	glEnableVertexAttribArray((GLuint)platform_renderer_gles2_multitex_attr_uv0);
	glEnableVertexAttribArray((GLuint)platform_renderer_gles2_multitex_attr_uv1);
	glEnableVertexAttribArray((GLuint)platform_renderer_gles2_multitex_attr_color);
	if ((platform_renderer_gles2_stream_vbo != 0) && (platform_renderer_gles2_stream_ibo != 0))
	{
		glBindBuffer(GL_ARRAY_BUFFER, platform_renderer_gles2_stream_vbo);
		glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)((size_t)vertex_count * sizeof(platform_renderer_gles2_multitex_vertex)), vertex_data, GL_DYNAMIC_DRAW);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, platform_renderer_gles2_stream_ibo);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, (GLsizeiptr)((size_t)index_count * sizeof(GLushort)), index_data, GL_DYNAMIC_DRAW);
		glVertexAttribPointer((GLuint)platform_renderer_gles2_multitex_attr_pos, 2, GL_FLOAT, GL_FALSE, (GLsizei)sizeof(platform_renderer_gles2_multitex_vertex), (const GLvoid *)offsetof(platform_renderer_gles2_multitex_vertex, position));
		glVertexAttribPointer((GLuint)platform_renderer_gles2_multitex_attr_uv0, 2, GL_FLOAT, GL_FALSE, (GLsizei)sizeof(platform_renderer_gles2_multitex_vertex), (const GLvoid *)offsetof(platform_renderer_gles2_multitex_vertex, uv0));
		glVertexAttribPointer((GLuint)platform_renderer_gles2_multitex_attr_uv1, 2, GL_FLOAT, GL_FALSE, (GLsizei)sizeof(platform_renderer_gles2_multitex_vertex), (const GLvoid *)offsetof(platform_renderer_gles2_multitex_vertex, uv1));
		glVertexAttribPointer((GLuint)platform_renderer_gles2_multitex_attr_color, 4, GL_UNSIGNED_BYTE, GL_TRUE, (GLsizei)sizeof(platform_renderer_gles2_multitex_vertex), (const GLvoid *)offsetof(platform_renderer_gles2_multitex_vertex, color));
		glDrawElements(GL_TRIANGLES, index_count, GL_UNSIGNED_SHORT, 0);
		glBindBuffer(GL_ARRAY_BUFFER, 0);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
	}
	else
	{
		glVertexAttribPointer((GLuint)platform_renderer_gles2_multitex_attr_pos, 2, GL_FLOAT, GL_FALSE, (GLsizei)sizeof(platform_renderer_gles2_multitex_vertex), &vertex_data[0].position[0]);
		glVertexAttribPointer((GLuint)platform_renderer_gles2_multitex_attr_uv0, 2, GL_FLOAT, GL_FALSE, (GLsizei)sizeof(platform_renderer_gles2_multitex_vertex), &vertex_data[0].uv0[0]);
		glVertexAttribPointer((GLuint)platform_renderer_gles2_multitex_attr_uv1, 2, GL_FLOAT, GL_FALSE, (GLsizei)sizeof(platform_renderer_gles2_multitex_vertex), &vertex_data[0].uv1[0]);
		glVertexAttribPointer((GLuint)platform_renderer_gles2_multitex_attr_color, 4, GL_UNSIGNED_BYTE, GL_TRUE, (GLsizei)sizeof(platform_renderer_gles2_multitex_vertex), &vertex_data[0].color[0]);
		glDrawElements(GL_TRIANGLES, index_count, GL_UNSIGNED_SHORT, index_data);
	}
	if (used_heap_vertices)
	{
		free(vertex_data);
	}
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, entry0->gles2_texture);
	if (used_heap_indices)
	{
		free(index_data);
	}
	platform_renderer_sdl_native_draw_count++;
	platform_renderer_sdl_native_textured_draw_count++;
	if ((source_id > 0) && (source_id < PLATFORM_RENDERER_GEOM_SRC_COUNT))
	{
		platform_renderer_gles2_draw_sources[source_id]++;
	}
	return true;
}

static bool PLATFORM_RENDERER_gles2_draw_multitextured_perspective_quad(
		float x0, float y0, float x1, float y1, float x2, float y2, float x3, float y3,
		float u0_1, float v0_1, float u0_2, float v0_2,
		float u1_1, float v1_1, float u1_2, float v1_2,
		float q)
{
	const float perspective_eps = 0.0005f;
	const int strip_count = PLATFORM_RENDERER_get_perspective_strip_count(q, true);
	float positions[24 * 4 * 2];
	float primary_uv[24 * 4 * 2];
	float secondary_uv[24 * 4 * 2];
	unsigned char colours[24 * 4 * 4];
	int indices[24 * 6];
	int base_vertex = 0;
	int base_index = 0;
	int strip_index;
	const unsigned char cr = PLATFORM_RENDERER_clamp_sdl_unit_to_byte(platform_renderer_current_colour_r);
	const unsigned char cg = PLATFORM_RENDERER_clamp_sdl_unit_to_byte(platform_renderer_current_colour_g);
	const unsigned char cb = PLATFORM_RENDERER_clamp_sdl_unit_to_byte(platform_renderer_current_colour_b);
	const unsigned char ca = PLATFORM_RENDERER_clamp_sdl_unit_to_byte(platform_renderer_current_colour_a);

	for (strip_index = 0; strip_index < strip_count; strip_index++)
	{
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
		const float w0 = (strip_count > 1) ? ((q * one_minus_t0) + t0) : 1.0f;
		const float w1 = (strip_count > 1) ? ((q * one_minus_t1) + t1) : 1.0f;
		const float primary_v0 = ((fabsf(q - 1.0f) > perspective_eps) && (q > perspective_eps)) ? (((v0_1 * q * one_minus_t0) + (v0_2 * t0)) / w0) : v0_1;
		const float primary_v1 = ((fabsf(q - 1.0f) > perspective_eps) && (q > perspective_eps)) ? (((v0_1 * q * one_minus_t1) + (v0_2 * t1)) / w1) : v0_2;
		const float secondary_v0 = ((fabsf(q - 1.0f) > perspective_eps) && (q > perspective_eps)) ? (((v1_1 * q * one_minus_t0) + (v1_2 * t0)) / w0) : v1_1;
		const float secondary_v1 = ((fabsf(q - 1.0f) > perspective_eps) && (q > perspective_eps)) ? (((v1_1 * q * one_minus_t1) + (v1_2 * t1)) / w1) : v1_2;
		int i;

		if ((fabsf(w0) <= perspective_eps) || (fabsf(w1) <= perspective_eps))
		{
			return false;
		}

		positions[(base_vertex + 0) * 2 + 0] = left_x0;
		positions[(base_vertex + 0) * 2 + 1] = left_y0;
		primary_uv[(base_vertex + 0) * 2 + 0] = u0_1;
		primary_uv[(base_vertex + 0) * 2 + 1] = primary_v0;
		secondary_uv[(base_vertex + 0) * 2 + 0] = u1_1;
		secondary_uv[(base_vertex + 0) * 2 + 1] = secondary_v0;

		positions[(base_vertex + 1) * 2 + 0] = left_x1;
		positions[(base_vertex + 1) * 2 + 1] = left_y1;
		primary_uv[(base_vertex + 1) * 2 + 0] = u0_1;
		primary_uv[(base_vertex + 1) * 2 + 1] = primary_v1;
		secondary_uv[(base_vertex + 1) * 2 + 0] = u1_1;
		secondary_uv[(base_vertex + 1) * 2 + 1] = secondary_v1;

		positions[(base_vertex + 2) * 2 + 0] = right_x1;
		positions[(base_vertex + 2) * 2 + 1] = right_y1;
		primary_uv[(base_vertex + 2) * 2 + 0] = u0_2;
		primary_uv[(base_vertex + 2) * 2 + 1] = primary_v1;
		secondary_uv[(base_vertex + 2) * 2 + 0] = u1_2;
		secondary_uv[(base_vertex + 2) * 2 + 1] = secondary_v1;

		positions[(base_vertex + 3) * 2 + 0] = right_x0;
		positions[(base_vertex + 3) * 2 + 1] = right_y0;
		primary_uv[(base_vertex + 3) * 2 + 0] = u0_2;
		primary_uv[(base_vertex + 3) * 2 + 1] = primary_v0;
		secondary_uv[(base_vertex + 3) * 2 + 0] = u1_2;
		secondary_uv[(base_vertex + 3) * 2 + 1] = secondary_v0;

		for (i = 0; i < 4; i++)
		{
			colours[((base_vertex + i) * 4) + 0] = cr;
			colours[((base_vertex + i) * 4) + 1] = cg;
			colours[((base_vertex + i) * 4) + 2] = cb;
			colours[((base_vertex + i) * 4) + 3] = ca;
		}

		indices[base_index + 0] = base_vertex + 0;
		indices[base_index + 1] = base_vertex + 1;
		indices[base_index + 2] = base_vertex + 2;
		indices[base_index + 3] = base_vertex + 0;
		indices[base_index + 4] = base_vertex + 2;
		indices[base_index + 5] = base_vertex + 3;

		base_vertex += 4;
		base_index += 6;
	}

	PLATFORM_RENDERER_gles2_flush_textured_batch();
	PLATFORM_RENDERER_gles2_set_affine_override_current_transform();
	strip_index = PLATFORM_RENDERER_gles2_draw_multitextured_geometry(
			platform_renderer_last_bound_texture_handle,
			platform_renderer_last_bound_secondary_texture_handle,
			platform_renderer_combiner_mode,
			positions,
			primary_uv,
			secondary_uv,
			colours,
			base_vertex,
			indices,
			base_index,
			PLATFORM_RENDERER_GEOM_SRC_MULTITEXTURE_ARRAY) ? 1 : 0;
	PLATFORM_RENDERER_gles2_set_affine_override_identity();
	return strip_index != 0;
}
#endif

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
	SDL_Vertex stack_vertices[PLATFORM_RENDERER_MAX_GEOMETRY_VERTICES];
	SDL_Vertex *converted_vertices = stack_vertices;
	const SDL_Vertex *work_vertices = vertices;
	SDL_Texture *draw_texture = texture;
	bool used_heap_vertices = false;
	int v;

#define PLATFORM_RENDERER_GEOM_RETURN(value) \
	do \
	{ \
		if (used_heap_vertices) \
		{ \
			free(converted_vertices); \
		} \
		return (value); \
	} while (0)

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
			(vertex_count <= 0) ||
			(index_count <= 0))
	{
		return false;
	}

	if (vertex_count > PLATFORM_RENDERER_MAX_GEOMETRY_VERTICES)
	{
		converted_vertices = (SDL_Vertex *)malloc((size_t)vertex_count * sizeof(SDL_Vertex));
		if (converted_vertices == NULL)
		{
			return false;
		}
		used_heap_vertices = true;
	}

	if ((vertices != NULL) && (vertex_count > 0))
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
		if ((vertex_count > 8) || (index_count > 18))
		{
			PLATFORM_RENDERER_GEOM_RETURN(false);
		}
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
		PLATFORM_RENDERER_GEOM_RETURN(true);
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
				PLATFORM_RENDERER_GEOM_RETURN(true);
			}
		}

		if (texture == NULL)
		{
			// Keep untextured geometry active even when textured-geometry mode is off.
			// This preserves coloured/perspective primitives used by UI/gameplay layers.
			if (SDL_RenderGeometry(platform_renderer_sdl_renderer, NULL, work_vertices, vertex_count, indices, index_count) == 0)
			{
				platform_renderer_sdl_native_draw_count++;
				PLATFORM_RENDERER_GEOM_RETURN(true);
			}
		}
		if (PLATFORM_RENDERER_try_sdl_geometry_textured_fallback(texture, work_vertices, vertex_count, indices, index_count, source_id))
		{
			PLATFORM_RENDERER_GEOM_RETURN(true);
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
		PLATFORM_RENDERER_GEOM_RETURN(false);
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
		PLATFORM_RENDERER_GEOM_RETURN(true);
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
		PLATFORM_RENDERER_GEOM_RETURN(true);
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
#undef PLATFORM_RENDERER_GEOM_RETURN
	if (used_heap_vertices)
	{
		free(converted_vertices);
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


static bool PLATFORM_RENDERER_uvs_are_texel_aligned(
		const platform_renderer_texture_entry *entry,
		float u1, float v1, float u2, float v2)
{
	const float eps = 0.01f;
	float sdl_v1;
	float sdl_v2;
	float px[4];
	int i;

	if ((entry == NULL) || (entry->width <= 0) || (entry->height <= 0))
	{
		return false;
	}

	sdl_v1 = PLATFORM_RENDERER_convert_v_to_sdl(v1);
	sdl_v2 = PLATFORM_RENDERER_convert_v_to_sdl(v2);
	px[0] = u1 * (float)entry->width;
	px[1] = u2 * (float)entry->width;
	px[2] = sdl_v1 * (float)entry->height;
	px[3] = sdl_v2 * (float)entry->height;

	for (i = 0; i < 4; i++)
	{
		const float nearest = roundf(px[i]);
		if (fabsf(px[i] - nearest) > eps)
		{
			return false;
		}
	}
	return true;
}

static bool PLATFORM_RENDERER_uvs_require_wrap(float u1, float v1, float u2, float v2, bool *out_wrap_s, bool *out_wrap_t)
{
	const float sdl_v1 = PLATFORM_RENDERER_convert_v_to_sdl(v1);
	const float sdl_v2 = PLATFORM_RENDERER_convert_v_to_sdl(v2);
	const bool wrap_s = (u1 < 0.0f) || (u1 > 1.0f) || (u2 < 0.0f) || (u2 > 1.0f);
	const bool wrap_t = (sdl_v1 < 0.0f) || (sdl_v1 > 1.0f) || (sdl_v2 < 0.0f) || (sdl_v2 > 1.0f);

	if (out_wrap_s != NULL)
	{
		*out_wrap_s = wrap_s;
	}
	if (out_wrap_t != NULL)
	{
		*out_wrap_t = wrap_t;
	}
	return wrap_s || wrap_t;
}

static void PLATFORM_RENDERER_build_safe_gles2_uv_bounds(
		const platform_renderer_texture_entry *entry,
		float u1, float v1, float u2, float v2,
		float *out_u_left,
		float *out_v_top,
		float *out_u_right,
		float *out_v_bottom)
{
	const float half_texel_u = ((entry != NULL) && (entry->width > 0)) ? (0.5f / (float)entry->width) : 0.0f;
	const float half_texel_v = ((entry != NULL) && (entry->height > 0)) ? (0.5f / (float)entry->height) : 0.0f;
	float left = u1;
	float right = u2;
	float top = v1;
	float bottom = v2;

	if (u2 >= u1)
	{
		left = u1 + half_texel_u;
		right = u2 - half_texel_u;
	}
	else
	{
		left = u1 - half_texel_u;
		right = u2 + half_texel_u;
	}

	if (v2 <= v1)
	{
		top = v1 - half_texel_v;
		bottom = v2 + half_texel_v;
	}
	else
	{
		top = v1 + half_texel_v;
		bottom = v2 - half_texel_v;
	}

	if (out_u_left != NULL) *out_u_left = left;
	if (out_v_top != NULL) *out_v_top = top;
	if (out_u_right != NULL) *out_u_right = right;
	if (out_v_bottom != NULL) *out_v_bottom = bottom;
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

typedef struct platform_renderer_axis_wrap_split
{
	float t_split;
	float a_u1;
	float a_u2;
	float b_u1;
	float b_u2;
} platform_renderer_axis_wrap_split;

static bool PLATFORM_RENDERER_build_axis_wrap_u_split(
		float u1,
		float v1,
		float u2,
		float v2,
		platform_renderer_axis_wrap_split *out_split)
{
	const float du = u2 - u1;
	const float abs_du = fabsf(du);
	float t_split;

	if (out_split == NULL)
	{
		return false;
	}
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
		const float boundary = floorf(u1) + 1.0f;
		t_split = (boundary - u1) / du;
		if ((t_split <= 0.00001f) || (t_split >= 0.99999f))
		{
			return false;
		}

		out_split->a_u1 = PLATFORM_RENDERER_wrap01(u1);
		out_split->a_u2 = 1.0f;
		out_split->b_u1 = 0.0f;
		out_split->b_u2 = PLATFORM_RENDERER_wrap01(u2);
	}
	else
	{
		const float boundary = floorf(u1);
		t_split = (u1 - boundary) / abs_du;
		if ((t_split <= 0.00001f) || (t_split >= 0.99999f))
		{
			return false;
		}

		out_split->a_u1 = PLATFORM_RENDERER_wrap01(u1);
		out_split->a_u2 = 0.0f;
		out_split->b_u1 = 1.0f;
		out_split->b_u2 = PLATFORM_RENDERER_wrap01(u2);
	}

	out_split->t_split = t_split;
	return true;
}

static void PLATFORM_RENDERER_gles2_interp_vertex(
		SDL_Vertex *out,
		const SDL_Vertex *a,
		const SDL_Vertex *b,
		float t)
{
	if ((out == NULL) || (a == NULL) || (b == NULL))
	{
		return;
	}

	out->position.x = a->position.x + ((b->position.x - a->position.x) * t);
	out->position.y = a->position.y + ((b->position.y - a->position.y) * t);
	out->tex_coord.x = a->tex_coord.x + ((b->tex_coord.x - a->tex_coord.x) * t);
	out->tex_coord.y = a->tex_coord.y + ((b->tex_coord.y - a->tex_coord.y) * t);
	out->color.r = (Uint8)((float)a->color.r + (((float)b->color.r - (float)a->color.r) * t));
	out->color.g = (Uint8)((float)a->color.g + (((float)b->color.g - (float)a->color.g) * t));
	out->color.b = (Uint8)((float)a->color.b + (((float)b->color.b - (float)a->color.b) * t));
	out->color.a = (Uint8)((float)a->color.a + (((float)b->color.a - (float)a->color.a) * t));
}

static void PLATFORM_RENDERER_gles2_normalize_quad_uv_band(SDL_Vertex quad[4])
{
	int i;
	float min_u = quad[0].tex_coord.x;
	float max_u = quad[0].tex_coord.x;
	float min_v = quad[0].tex_coord.y;
	float max_v = quad[0].tex_coord.y;

	for (i = 1; i < 4; i++)
	{
		if (quad[i].tex_coord.x < min_u) min_u = quad[i].tex_coord.x;
		if (quad[i].tex_coord.x > max_u) max_u = quad[i].tex_coord.x;
		if (quad[i].tex_coord.y < min_v) min_v = quad[i].tex_coord.y;
		if (quad[i].tex_coord.y > max_v) max_v = quad[i].tex_coord.y;
	}

	if ((floorf(min_u) == floorf(max_u)) && ((min_u < 0.0f) || (max_u > 1.0f)))
	{
		const float offset_u = floorf(min_u);
		for (i = 0; i < 4; i++)
		{
			quad[i].tex_coord.x -= offset_u;
		}
	}

	if ((floorf(min_v) == floorf(max_v)) && ((min_v < 0.0f) || (max_v > 1.0f)))
	{
		const float offset_v = floorf(min_v);
		for (i = 0; i < 4; i++)
		{
			quad[i].tex_coord.y -= offset_v;
		}
	}
}

static bool PLATFORM_RENDERER_gles2_split_quad_wrap_s(
		const SDL_Vertex in_quad[4],
		SDL_Vertex out_quads[2][4])
{
	const float u_left = (in_quad[0].tex_coord.x + in_quad[1].tex_coord.x) * 0.5f;
	const float u_right = (in_quad[2].tex_coord.x + in_quad[3].tex_coord.x) * 0.5f;
	const float du = u_right - u_left;
	const float abs_du = fabsf(du);
	float boundary;
	float t;
	SDL_Vertex split_top;
	SDL_Vertex split_bottom;

	if ((abs_du < 0.00001f) || (floorf(u_left) == floorf(u_right)))
	{
		return false;
	}

	if (du > 0.0f)
	{
		boundary = floorf(u_left) + 1.0f;
		t = (boundary - u_left) / du;
	}
	else
	{
		boundary = floorf(u_left);
		t = (u_left - boundary) / abs_du;
	}

	if ((t <= 0.00001f) || (t >= 0.99999f))
	{
		return false;
	}

	PLATFORM_RENDERER_gles2_interp_vertex(&split_top, &in_quad[0], &in_quad[3], t);
	PLATFORM_RENDERER_gles2_interp_vertex(&split_bottom, &in_quad[1], &in_quad[2], t);

	out_quads[0][0] = in_quad[0];
	out_quads[0][1] = in_quad[1];
	out_quads[0][2] = split_bottom;
	out_quads[0][3] = split_top;
	out_quads[1][0] = split_top;
	out_quads[1][1] = split_bottom;
	out_quads[1][2] = in_quad[2];
	out_quads[1][3] = in_quad[3];

	if (du > 0.0f)
	{
		out_quads[0][0].tex_coord.x = PLATFORM_RENDERER_wrap01(u_left);
		out_quads[0][1].tex_coord.x = PLATFORM_RENDERER_wrap01(u_left);
		out_quads[0][2].tex_coord.x = 1.0f;
		out_quads[0][3].tex_coord.x = 1.0f;
		out_quads[1][0].tex_coord.x = 0.0f;
		out_quads[1][1].tex_coord.x = 0.0f;
		out_quads[1][2].tex_coord.x = PLATFORM_RENDERER_wrap01(u_right);
		out_quads[1][3].tex_coord.x = PLATFORM_RENDERER_wrap01(u_right);
	}
	else
	{
		out_quads[0][0].tex_coord.x = PLATFORM_RENDERER_wrap01(u_left);
		out_quads[0][1].tex_coord.x = PLATFORM_RENDERER_wrap01(u_left);
		out_quads[0][2].tex_coord.x = 0.0f;
		out_quads[0][3].tex_coord.x = 0.0f;
		out_quads[1][0].tex_coord.x = 1.0f;
		out_quads[1][1].tex_coord.x = 1.0f;
		out_quads[1][2].tex_coord.x = PLATFORM_RENDERER_wrap01(u_right);
		out_quads[1][3].tex_coord.x = PLATFORM_RENDERER_wrap01(u_right);
	}

	return true;
}

static bool PLATFORM_RENDERER_gles2_split_quad_wrap_t(
		const SDL_Vertex in_quad[4],
		SDL_Vertex out_quads[2][4])
{
	const float v_top = (in_quad[0].tex_coord.y + in_quad[3].tex_coord.y) * 0.5f;
	const float v_bottom = (in_quad[1].tex_coord.y + in_quad[2].tex_coord.y) * 0.5f;
	const float dv = v_bottom - v_top;
	const float abs_dv = fabsf(dv);
	float boundary;
	float t;
	SDL_Vertex split_left;
	SDL_Vertex split_right;

	if ((abs_dv < 0.00001f) || (floorf(v_top) == floorf(v_bottom)))
	{
		return false;
	}

	if (dv > 0.0f)
	{
		boundary = floorf(v_top) + 1.0f;
		t = (boundary - v_top) / dv;
	}
	else
	{
		boundary = floorf(v_top);
		t = (v_top - boundary) / abs_dv;
	}

	if ((t <= 0.00001f) || (t >= 0.99999f))
	{
		return false;
	}

	PLATFORM_RENDERER_gles2_interp_vertex(&split_left, &in_quad[0], &in_quad[1], t);
	PLATFORM_RENDERER_gles2_interp_vertex(&split_right, &in_quad[3], &in_quad[2], t);

	out_quads[0][0] = in_quad[0];
	out_quads[0][1] = split_left;
	out_quads[0][2] = split_right;
	out_quads[0][3] = in_quad[3];
	out_quads[1][0] = split_left;
	out_quads[1][1] = in_quad[1];
	out_quads[1][2] = in_quad[2];
	out_quads[1][3] = split_right;

	if (dv > 0.0f)
	{
		out_quads[0][0].tex_coord.y = PLATFORM_RENDERER_wrap01(v_top);
		out_quads[0][3].tex_coord.y = PLATFORM_RENDERER_wrap01(v_top);
		out_quads[0][1].tex_coord.y = 1.0f;
		out_quads[0][2].tex_coord.y = 1.0f;
		out_quads[1][0].tex_coord.y = 0.0f;
		out_quads[1][3].tex_coord.y = 0.0f;
		out_quads[1][1].tex_coord.y = PLATFORM_RENDERER_wrap01(v_bottom);
		out_quads[1][2].tex_coord.y = PLATFORM_RENDERER_wrap01(v_bottom);
	}
	else
	{
		out_quads[0][0].tex_coord.y = PLATFORM_RENDERER_wrap01(v_top);
		out_quads[0][3].tex_coord.y = PLATFORM_RENDERER_wrap01(v_top);
		out_quads[0][1].tex_coord.y = 0.0f;
		out_quads[0][2].tex_coord.y = 0.0f;
		out_quads[1][0].tex_coord.y = 1.0f;
		out_quads[1][3].tex_coord.y = 1.0f;
		out_quads[1][1].tex_coord.y = PLATFORM_RENDERER_wrap01(v_bottom);
		out_quads[1][2].tex_coord.y = PLATFORM_RENDERER_wrap01(v_bottom);
	}

	return true;
}

static bool PLATFORM_RENDERER_gles2_draw_wrapped_quad_geometry_gltex(
		GLuint tex,
		unsigned int logical_texture_handle,
		const SDL_Vertex *vertices,
		int vertex_count,
		const int *indices,
		int index_count,
		int source_id)
{
	SDL_Vertex stack_vertices[PLATFORM_RENDERER_MAX_GEOMETRY_VERTICES * 4];
	int stack_indices[(PLATFORM_RENDERER_MAX_GEOMETRY_VERTICES / 4) * 24];
	SDL_Vertex *batched_vertices = stack_vertices;
	int *batched_indices = stack_indices;
	platform_renderer_texture_entry *entry = PLATFORM_RENDERER_get_texture_entry(logical_texture_handle);
	const int max_output_vertices = vertex_count * 4;
	const int max_output_indices = (vertex_count / 4) * 24;
	int batched_vertex_count = 0;
	int batched_index_count = 0;
	int quad_index;

	if ((vertices == NULL) || (indices == NULL) ||
			(vertex_count <= 0) || (index_count <= 0) ||
			((vertex_count % 4) != 0) || ((index_count % 6) != 0))
	{
		return false;
	}

	if (max_output_vertices > (PLATFORM_RENDERER_MAX_GEOMETRY_VERTICES * 4))
	{
		batched_vertices = (SDL_Vertex *)malloc((size_t)max_output_vertices * sizeof(SDL_Vertex));
		if (batched_vertices == NULL)
		{
			return false;
		}
	}
	if (max_output_indices > ((PLATFORM_RENDERER_MAX_GEOMETRY_VERTICES / 4) * 24))
	{
		batched_indices = (int *)malloc((size_t)max_output_indices * sizeof(int));
		if (batched_indices == NULL)
		{
			if (batched_vertices != stack_vertices)
			{
				free(batched_vertices);
			}
			return false;
		}
	}

	for (quad_index = 0; quad_index < (index_count / 6); quad_index++)
	{
		const int base_vertex = quad_index * 4;
		const int base_index = quad_index * 6;
		if ((indices[base_index + 0] != (base_vertex + 0)) ||
				(indices[base_index + 1] != (base_vertex + 1)) ||
				(indices[base_index + 2] != (base_vertex + 2)) ||
				(indices[base_index + 3] != (base_vertex + 0)) ||
				(indices[base_index + 4] != (base_vertex + 2)) ||
				(indices[base_index + 5] != (base_vertex + 3)))
		{
			return false;
		}
	}

	for (quad_index = 0; quad_index < (vertex_count / 4); quad_index++)
	{
		SDL_Vertex quads[4][4];
		int quad_count = 1;
		int current = 0;

		memcpy(quads[0], &vertices[quad_index * 4], sizeof(SDL_Vertex) * 4);
		PLATFORM_RENDERER_gles2_normalize_quad_uv_band(quads[0]);

		while (current < quad_count)
		{
			SDL_Vertex split_quads[2][4];
			if (PLATFORM_RENDERER_gles2_split_quad_wrap_s(quads[current], split_quads))
			{
				memcpy(quads[current], split_quads[0], sizeof(split_quads[0]));
				memcpy(quads[quad_count], split_quads[1], sizeof(split_quads[1]));
				PLATFORM_RENDERER_gles2_normalize_quad_uv_band(quads[current]);
				PLATFORM_RENDERER_gles2_normalize_quad_uv_band(quads[quad_count]);
				quad_count++;
				continue;
			}
			if (PLATFORM_RENDERER_gles2_split_quad_wrap_t(quads[current], split_quads))
			{
				memcpy(quads[current], split_quads[0], sizeof(split_quads[0]));
				memcpy(quads[quad_count], split_quads[1], sizeof(split_quads[1]));
				PLATFORM_RENDERER_gles2_normalize_quad_uv_band(quads[current]);
				PLATFORM_RENDERER_gles2_normalize_quad_uv_band(quads[quad_count]);
				quad_count++;
				continue;
			}
			current++;
		}

		for (current = 0; current < quad_count; current++)
		{
			SDL_Rect src_rect;
			SDL_Vertex final_quad[4];
			float min_u = quads[current][0].tex_coord.x;
			float min_v = quads[current][0].tex_coord.y;
			float max_u = quads[current][0].tex_coord.x;
			float max_v = quads[current][0].tex_coord.y;
			int vi;

			for (vi = 1; vi < 4; vi++)
			{
				if (quads[current][vi].tex_coord.x < min_u) min_u = quads[current][vi].tex_coord.x;
				if (quads[current][vi].tex_coord.y < min_v) min_v = quads[current][vi].tex_coord.y;
				if (quads[current][vi].tex_coord.x > max_u) max_u = quads[current][vi].tex_coord.x;
				if (quads[current][vi].tex_coord.y > max_v) max_v = quads[current][vi].tex_coord.y;
			}

			for (vi = 0; vi < 4; vi++)
			{
				final_quad[vi] = quads[current][vi];
				final_quad[vi].tex_coord.y = PLATFORM_RENDERER_convert_v_to_sdl(final_quad[vi].tex_coord.y);
			}

			if ((entry != NULL) &&
					PLATFORM_RENDERER_build_safe_sdl_src_rect(entry, min_u, min_v, max_u, max_v, &src_rect))
			{
				const float src_u_min = (float)src_rect.x / (float)entry->width;
				const float src_u_max = (float)(src_rect.x + src_rect.w) / (float)entry->width;
				const float src_v_min = (float)src_rect.y / (float)entry->height;
				const float src_v_max = (float)(src_rect.y + src_rect.h) / (float)entry->height;
				for (vi = 0; vi < 4; vi++)
				{
					if (final_quad[vi].tex_coord.x < src_u_min) final_quad[vi].tex_coord.x = src_u_min;
					if (final_quad[vi].tex_coord.x > src_u_max) final_quad[vi].tex_coord.x = src_u_max;
					if (final_quad[vi].tex_coord.y < src_v_min) final_quad[vi].tex_coord.y = src_v_min;
					if (final_quad[vi].tex_coord.y > src_v_max) final_quad[vi].tex_coord.y = src_v_max;
				}
			}

			for (vi = 0; vi < 4; vi++)
			{
				batched_vertices[batched_vertex_count + vi] = final_quad[vi];
			}
			batched_indices[batched_index_count + 0] = batched_vertex_count + 0;
			batched_indices[batched_index_count + 1] = batched_vertex_count + 1;
			batched_indices[batched_index_count + 2] = batched_vertex_count + 2;
			batched_indices[batched_index_count + 3] = batched_vertex_count + 0;
			batched_indices[batched_index_count + 4] = batched_vertex_count + 2;
			batched_indices[batched_index_count + 5] = batched_vertex_count + 3;
			batched_vertex_count += 4;
			batched_index_count += 6;
		}
	}

	{
		const bool ok = (batched_vertex_count > 0) &&
				PLATFORM_RENDERER_gles2_draw_textured_geometry_gltex(
						tex,
						logical_texture_handle,
						batched_vertices,
						batched_vertex_count,
						batched_indices,
						batched_index_count,
						source_id,
						false,
						false);
		if (batched_indices != stack_indices)
		{
			free(batched_indices);
		}
		if (batched_vertices != stack_vertices)
		{
			free(batched_vertices);
		}
		return ok;
	}
}

static bool PLATFORM_RENDERER_draw_axis_wrapped_u_repeat(
		platform_renderer_texture_entry *entry,
		SDL_Texture *draw_texture,
		const SDL_Rect *dst_rect,
		float u1, float v1, float u2, float v2,
		Uint8 mod_r, Uint8 mod_g, Uint8 mod_b, Uint8 mod_a,
		int source_id)
{
	platform_renderer_axis_wrap_split split;
	SDL_Rect dst_a;
	SDL_Rect dst_b;
	int split_x;

	if ((entry == NULL) || (draw_texture == NULL) || (dst_rect == NULL))
	{
		return false;
	}
	if ((dst_rect->w <= 0) || (dst_rect->h <= 0))
	{
		return false;
	}

	if (!PLATFORM_RENDERER_build_axis_wrap_u_split(u1, v1, u2, v2, &split))
	{
		return false;
	}
	split_x = dst_rect->x + (int)floorf(((float)dst_rect->w * split.t_split) + 0.5f);
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
		const bool a_flip_x = (split.a_u2 < split.a_u1);
		const bool b_flip_x = (split.b_u2 < split.b_u1);
		const bool flip_y = PLATFORM_RENDERER_src_flip_y_for_sdl(v1, v2);
		const float a_u_left = a_flip_x ? split.a_u2 : split.a_u1;
		const float a_u_right = a_flip_x ? split.a_u1 : split.a_u2;
		const float b_u_left = b_flip_x ? split.b_u2 : split.b_u1;
		const float b_u_right = b_flip_x ? split.b_u1 : split.b_u2;
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

static bool PLATFORM_RENDERER_gles2_draw_axis_wrapped_u_repeat_geometry(
		unsigned int logical_texture_handle,
		const float *screen_x,
		const float *screen_y,
		float u1, float v1, float u2, float v2,
		Uint8 mod_r, Uint8 mod_g, Uint8 mod_b, Uint8 mod_a,
		int source_id)
{
	platform_renderer_texture_entry *entry;
	GLuint tex;
	platform_renderer_axis_wrap_split split;
	SDL_Vertex vertices[8];
	const int indices[12] = {0, 1, 2, 0, 2, 3, 4, 5, 6, 4, 6, 7};
	const bool flip_y = PLATFORM_RENDERER_src_flip_y_for_sdl(v1, v2);
	const float v_top = PLATFORM_RENDERER_convert_v_to_sdl(flip_y ? v2 : v1);
	const float v_bottom = PLATFORM_RENDERER_convert_v_to_sdl(flip_y ? v1 : v2);
	float split_top_x;
	float split_top_y;
	float split_bottom_x;
	float split_bottom_y;

	if ((screen_x == NULL) || (screen_y == NULL))
	{
		return false;
	}
	entry = PLATFORM_RENDERER_get_texture_entry(logical_texture_handle);
	if ((entry == NULL) || !PLATFORM_RENDERER_build_gles2_texture_from_entry(entry))
	{
		return false;
	}
	tex = entry->gles2_texture;

	if (!PLATFORM_RENDERER_build_axis_wrap_u_split(u1, v1, u2, v2, &split))
	{
		return false;
	}

	split_top_x = screen_x[0] + ((screen_x[3] - screen_x[0]) * split.t_split);
	split_top_y = screen_y[0] + ((screen_y[3] - screen_y[0]) * split.t_split);
	split_bottom_x = screen_x[1] + ((screen_x[2] - screen_x[1]) * split.t_split);
	split_bottom_y = screen_y[1] + ((screen_y[2] - screen_y[1]) * split.t_split);

	vertices[0].position.x = screen_x[0];
	vertices[0].position.y = screen_y[0];
	vertices[1].position.x = screen_x[1];
	vertices[1].position.y = screen_y[1];
	vertices[2].position.x = split_bottom_x;
	vertices[2].position.y = split_bottom_y;
	vertices[3].position.x = split_top_x;
	vertices[3].position.y = split_top_y;
	vertices[4].position.x = split_top_x;
	vertices[4].position.y = split_top_y;
	vertices[5].position.x = split_bottom_x;
	vertices[5].position.y = split_bottom_y;
	vertices[6].position.x = screen_x[2];
	vertices[6].position.y = screen_y[2];
	vertices[7].position.x = screen_x[3];
	vertices[7].position.y = screen_y[3];

	vertices[0].tex_coord.x = split.a_u1;
	vertices[0].tex_coord.y = v_top;
	vertices[1].tex_coord.x = split.a_u1;
	vertices[1].tex_coord.y = v_bottom;
	vertices[2].tex_coord.x = split.a_u2;
	vertices[2].tex_coord.y = v_bottom;
	vertices[3].tex_coord.x = split.a_u2;
	vertices[3].tex_coord.y = v_top;
	vertices[4].tex_coord.x = split.b_u1;
	vertices[4].tex_coord.y = v_top;
	vertices[5].tex_coord.x = split.b_u1;
	vertices[5].tex_coord.y = v_bottom;
	vertices[6].tex_coord.x = split.b_u2;
	vertices[6].tex_coord.y = v_bottom;
	vertices[7].tex_coord.x = split.b_u2;
	vertices[7].tex_coord.y = v_top;

	for (int i = 0; i < 8; i++)
	{
		vertices[i].color.r = mod_r;
		vertices[i].color.g = mod_g;
		vertices[i].color.b = mod_b;
		vertices[i].color.a = mod_a;
	}

	return PLATFORM_RENDERER_gles2_draw_textured_geometry_gltex(
			tex,
			logical_texture_handle,
			vertices,
			8,
			indices,
			12,
			source_id,
			false,
			false);
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

static void PLATFORM_RENDERER_reset_transform_state(void)
{
	platform_renderer_tx_a = 1.0f;
	platform_renderer_tx_b = 0.0f;
	platform_renderer_tx_c = 0.0f;
	platform_renderer_tx_d = 1.0f;
	platform_renderer_tx_x = 0.0f;
	platform_renderer_tx_y = 0.0f;
}

static void PLATFORM_RENDERER_flush_command_queue(void)
{
	int i;

	for (i = 0; i < platform_renderer_command_queue_count; i++)
	{
		const platform_renderer_command *cmd = &platform_renderer_command_queue[i];
		const float saved_colour_r = platform_renderer_current_colour_r;
		const float saved_colour_g = platform_renderer_current_colour_g;
		const float saved_colour_b = platform_renderer_current_colour_b;
		const float saved_colour_a = platform_renderer_current_colour_a;
		const bool saved_blend_enabled = platform_renderer_blend_enabled;
		const int saved_blend_mode = platform_renderer_blend_mode;
		const unsigned int saved_texture_handle = platform_renderer_last_bound_texture_handle;
		const float saved_tx_a = platform_renderer_tx_a;
		const float saved_tx_b = platform_renderer_tx_b;
		const float saved_tx_c = platform_renderer_tx_c;
		const float saved_tx_d = platform_renderer_tx_d;
		const float saved_tx_x = platform_renderer_tx_x;
		const float saved_tx_y = platform_renderer_tx_y;

		platform_renderer_current_colour_r = cmd->colour_r;
		platform_renderer_current_colour_g = cmd->colour_g;
		platform_renderer_current_colour_b = cmd->colour_b;
		platform_renderer_current_colour_a = cmd->colour_a;
		platform_renderer_blend_enabled = cmd->blend_enabled;
		platform_renderer_blend_mode = cmd->blend_mode;
		platform_renderer_last_bound_texture_handle = cmd->texture_handle;
		platform_renderer_tx_a = cmd->tx_a;
		platform_renderer_tx_b = cmd->tx_b;
		platform_renderer_tx_c = cmd->tx_c;
		platform_renderer_tx_d = cmd->tx_d;
		platform_renderer_tx_x = cmd->tx_x;
		platform_renderer_tx_y = cmd->tx_y;

		if (cmd->type == PLATFORM_RENDERER_CMD_PERSPECTIVE)
		{
			PLATFORM_RENDERER_draw_bound_perspective_textured_quad_immediate(
					cmd->x0, cmd->y0, cmd->x1, cmd->y1, cmd->x2, cmd->y2, cmd->x3, cmd->y3,
					cmd->u1, cmd->v1, cmd->u2, cmd->v2, cmd->q);
		}
		else if (cmd->type == PLATFORM_RENDERER_CMD_COLOURED_PERSPECTIVE)
		{
			PLATFORM_RENDERER_draw_bound_coloured_perspective_textured_quad_immediate(
					cmd->x0, cmd->y0, cmd->x1, cmd->y1, cmd->x2, cmd->y2, cmd->x3, cmd->y3,
					cmd->u1, cmd->v1, cmd->u2, cmd->v2, cmd->q,
					cmd->vr, cmd->vg, cmd->vb, cmd->va);
		}

		platform_renderer_current_colour_r = saved_colour_r;
		platform_renderer_current_colour_g = saved_colour_g;
		platform_renderer_current_colour_b = saved_colour_b;
		platform_renderer_current_colour_a = saved_colour_a;
		platform_renderer_blend_enabled = saved_blend_enabled;
		platform_renderer_blend_mode = saved_blend_mode;
		platform_renderer_last_bound_texture_handle = saved_texture_handle;
		platform_renderer_tx_a = saved_tx_a;
		platform_renderer_tx_b = saved_tx_b;
		platform_renderer_tx_c = saved_tx_c;
		platform_renderer_tx_d = saved_tx_d;
		platform_renderer_tx_x = saved_tx_x;
		platform_renderer_tx_y = saved_tx_y;
	}

	platform_renderer_command_queue_count = 0;
}

static void PLATFORM_RENDERER_apply_sdl_draw_blend_mode(void)
{
	if (platform_renderer_command_queue_count > 0)
	{
		PLATFORM_RENDERER_flush_command_queue();
	}
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
	platform_renderer_command_queue_count = 0;
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
#if defined(WIZBALL_RENDER_BACKEND_GLES2)
	platform_renderer_texture_entries[platform_renderer_texture_count].gles2_texture = 0;
#endif
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
#if defined(WIZBALL_RENDER_BACKEND_GLES2)
			if (PLATFORM_RENDERER_gles2_is_ready())
			{
				(void)PLATFORM_RENDERER_build_gles2_texture_from_entry(&platform_renderer_texture_entries[platform_renderer_texture_count]);
			}
#endif
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
	if (platform_renderer_command_queue_count > 0)
	{
		PLATFORM_RENDERER_flush_command_queue();
	}
#if defined(WIZBALL_RENDER_BACKEND_GLES2)
	platform_renderer_gles2_flush_clear_count++;
	PLATFORM_RENDERER_gles2_flush_textured_batch();
#endif
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
#if defined(WIZBALL_RENDER_BACKEND_GLES2)
	if (PLATFORM_RENDERER_gles2_is_ready())
	{
		glViewport(0, 0, platform_renderer_present_width > 0 ? platform_renderer_present_width : 640, platform_renderer_present_height > 0 ? platform_renderer_present_height : 480);
		glDisable(GL_SCISSOR_TEST);
		glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT);
		s_frame_clear_ticks = SDL_GetTicks();
		return;
	}
#endif
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
	if (platform_renderer_command_queue_count > 0)
	{
		PLATFORM_RENDERER_flush_command_queue();
	}
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
	if (platform_renderer_command_queue_count > 0)
	{
		PLATFORM_RENDERER_flush_command_queue();
	}
#if defined(WIZBALL_RENDER_BACKEND_GLES2)
	if (PLATFORM_RENDERER_gles2_is_ready())
	{
		const float left = (float)((x1 < x2) ? x1 : x2);
		const float right = (float)((x1 > x2) ? x1 : x2);
		const float top = (float)(virtual_screen_height - ((y1 < y2) ? y1 : y2));
		const float bottom = (float)(virtual_screen_height - ((y1 > y2) ? y1 : y2));
		const float old_r = platform_renderer_current_colour_r;
		const float old_g = platform_renderer_current_colour_g;
		const float old_b = platform_renderer_current_colour_b;
		const float old_a = platform_renderer_current_colour_a;
		PLATFORM_RENDERER_set_colour4f((float)r / 255.0f, (float)g / 255.0f, (float)b / 255.0f, old_a);
		PLATFORM_RENDERER_draw_bound_solid_quad(left, right, top, bottom);
		platform_renderer_current_colour_r = old_r;
		platform_renderer_current_colour_g = old_g;
		platform_renderer_current_colour_b = old_b;
		platform_renderer_current_colour_a = old_a;
		return;
	}
#endif
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
	if (platform_renderer_command_queue_count > 0)
	{
		PLATFORM_RENDERER_flush_command_queue();
	}
#if defined(WIZBALL_RENDER_BACKEND_GLES2)
	if (PLATFORM_RENDERER_gles2_is_ready())
	{
		(void)PLATFORM_RENDERER_gles2_draw_coloured_line_segment(
				(float)x1, (float)y1, (float)x2, (float)y2,
				PLATFORM_RENDERER_clamp_sdl_colour_mod(r),
				PLATFORM_RENDERER_clamp_sdl_colour_mod(g),
				PLATFORM_RENDERER_clamp_sdl_colour_mod(b),
				255);
		return;
	}
#endif
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
	if (platform_renderer_command_queue_count > 0)
	{
		PLATFORM_RENDERER_flush_command_queue();
	}
#if defined(WIZBALL_RENDER_BACKEND_GLES2)
	if (PLATFORM_RENDERER_gles2_is_ready() && (platform_renderer_present_height > 0))
	{
		float tx1;
		float ty1;
		float tx2;
		float ty2;
		PLATFORM_RENDERER_transform_point(x1, y1, &tx1, &ty1);
		PLATFORM_RENDERER_transform_point(x2, y2, &tx2, &ty2);
		(void)PLATFORM_RENDERER_gles2_draw_coloured_line_segment(
				tx1,
				(float)platform_renderer_present_height - ty1,
				tx2,
				(float)platform_renderer_present_height - ty2,
				PLATFORM_RENDERER_clamp_sdl_unit_to_byte(r),
				PLATFORM_RENDERER_clamp_sdl_unit_to_byte(g),
				PLATFORM_RENDERER_clamp_sdl_unit_to_byte(b),
				PLATFORM_RENDERER_clamp_sdl_unit_to_byte(platform_renderer_current_colour_a));
		return;
	}
#endif
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
	if (platform_renderer_command_queue_count > 0)
	{
		PLATFORM_RENDERER_flush_command_queue();
	}
#if defined(WIZBALL_RENDER_BACKEND_GLES2)
	if (PLATFORM_RENDERER_gles2_is_ready() && (platform_renderer_present_height > 0))
	{
		float tx1;
		float ty1;
		float tx2;
		float ty2;
		PLATFORM_RENDERER_transform_point(x1, y1, &tx1, &ty1);
		PLATFORM_RENDERER_transform_point(x2, y2, &tx2, &ty2);
		(void)PLATFORM_RENDERER_gles2_draw_coloured_line_segment(
				tx1,
				(float)platform_renderer_present_height - ty1,
				tx2,
				(float)platform_renderer_present_height - ty2,
				PLATFORM_RENDERER_clamp_sdl_unit_to_byte(platform_renderer_current_colour_r),
				PLATFORM_RENDERER_clamp_sdl_unit_to_byte(platform_renderer_current_colour_g),
				PLATFORM_RENDERER_clamp_sdl_unit_to_byte(platform_renderer_current_colour_b),
				PLATFORM_RENDERER_clamp_sdl_unit_to_byte(platform_renderer_current_colour_a));
		return;
	}
#endif
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
	if (platform_renderer_command_queue_count > 0)
	{
		PLATFORM_RENDERER_flush_command_queue();
	}
	int i;

	if ((x == NULL) || (y == NULL) || (count < 2))
	{
		return;
	}
#if defined(WIZBALL_RENDER_BACKEND_GLES2)
	if (PLATFORM_RENDERER_gles2_is_ready() && (platform_renderer_present_height > 0))
	{
		for (i = 0; i < count; i++)
		{
			int j = (i + 1) % count;
			PLATFORM_RENDERER_draw_line(x[i], y[i], x[j], y[j]);
		}
		return;
	}
#endif
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
	if (platform_renderer_command_queue_count > 0)
	{
		PLATFORM_RENDERER_flush_command_queue();
	}
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
	if (platform_renderer_command_queue_count > 0)
	{
		PLATFORM_RENDERER_flush_command_queue();
	}
#if defined(WIZBALL_RENDER_BACKEND_GLES2)
	if (PLATFORM_RENDERER_gles2_is_ready() && (platform_renderer_present_height > 0))
	{
		SDL_Vertex verts[4];
		const int idx[6] = {0, 1, 2, 0, 2, 3};
		const float px[4] = {left, left, right, right};
		const float py[4] = {up, down, down, up};
		const Uint8 cr = PLATFORM_RENDERER_clamp_sdl_unit_to_byte(platform_renderer_current_colour_r);
		const Uint8 cg = PLATFORM_RENDERER_clamp_sdl_unit_to_byte(platform_renderer_current_colour_g);
		const Uint8 cb = PLATFORM_RENDERER_clamp_sdl_unit_to_byte(platform_renderer_current_colour_b);
		const Uint8 ca = PLATFORM_RENDERER_clamp_sdl_unit_to_byte(platform_renderer_current_colour_a);
		bool prev_affine_active = false;
		GLfloat prev_affine_row0[3];
		GLfloat prev_affine_row1[3];
		for (int vi = 0; vi < 4; vi++)
		{
			verts[vi].position.x = px[vi];
			verts[vi].position.y = py[vi];
			verts[vi].tex_coord.x = (vi == 0 || vi == 1) ? 0.0f : 1.0f;
			verts[vi].tex_coord.y = (vi == 0 || vi == 3) ? 0.0f : 1.0f;
			verts[vi].color.r = cr;
			verts[vi].color.g = cg;
			verts[vi].color.b = cb;
			verts[vi].color.a = ca;
		}
		PLATFORM_RENDERER_gles2_capture_affine_state(&prev_affine_active, prev_affine_row0, prev_affine_row1);
		PLATFORM_RENDERER_gles2_set_affine_override_current_transform();
		(void)PLATFORM_RENDERER_gles2_draw_textured_geometry(0, verts, 4, idx, 6, PLATFORM_RENDERER_GEOM_SRC_NONE);
		PLATFORM_RENDERER_gles2_apply_affine_state(prev_affine_active, prev_affine_row0, prev_affine_row1);
		return;
	}
#endif
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
	if (platform_renderer_command_queue_count > 0)
	{
		PLATFORM_RENDERER_flush_command_queue();
	}
#if defined(WIZBALL_RENDER_BACKEND_GLES2)
	if (PLATFORM_RENDERER_gles2_is_ready())
	{
		platform_renderer_texture_entry *entry = PLATFORM_RENDERER_get_texture_entry(platform_renderer_last_bound_texture_handle);
		SDL_Vertex vertices[4];
		const int indices[6] = {0, 1, 2, 0, 2, 3};
		SDL_Rect src_rect;
		bool wrap_s = false;
		bool wrap_t = false;
		const bool src_flip_x = (u2 < u1);
		const bool src_flip_y = PLATFORM_RENDERER_src_flip_y_for_sdl(v1, v2);
		const float quad_x[4] = {left, left, right, right};
		const float quad_y[4] = {up, down, down, up};
		bool prev_affine_active = false;
		GLfloat prev_affine_row0[3];
		GLfloat prev_affine_row1[3];
		int i;

		if ((entry == NULL) || !PLATFORM_RENDERER_build_gles2_texture_from_entry(entry))
		{
			return;
		}
		PLATFORM_RENDERER_gles2_capture_affine_state(&prev_affine_active, prev_affine_row0, prev_affine_row1);
		PLATFORM_RENDERER_gles2_set_affine_override_current_transform();

		(void)PLATFORM_RENDERER_uvs_require_wrap(u1, v1, u2, v2, &wrap_s, &wrap_t);
		for (i = 0; i < 4; i++)
		{
			int cr;
			int cg;
			int cb;
			int ca;

			vertices[i].position.x = quad_x[i];
			vertices[i].position.y = quad_y[i];
			vertices[i].tex_coord.x = (i < 2) ? (src_flip_x ? u2 : u1) : (src_flip_x ? u1 : u2);
			vertices[i].tex_coord.y = ((i == 0) || (i == 3)) ? (src_flip_y ? v2 : v1) : (src_flip_y ? v1 : v2);
			cr = (int)(platform_renderer_current_colour_r * 255.0f);
			cg = (int)(platform_renderer_current_colour_g * 255.0f);
			cb = (int)(platform_renderer_current_colour_b * 255.0f);
			ca = (int)(platform_renderer_current_colour_a * 255.0f);
			if (cr < 0) cr = 0; if (cr > 255) cr = 255;
			if (cg < 0) cg = 0; if (cg > 255) cg = 255;
			if (cb < 0) cb = 0; if (cb > 255) cb = 255;
			if (ca < 0) ca = 0; if (ca > 255) ca = 255;
			vertices[i].color.r = (Uint8)cr;
			vertices[i].color.g = (Uint8)cg;
			vertices[i].color.b = (Uint8)cb;
			vertices[i].color.a = (Uint8)ca;
		}

		if (!wrap_s && !wrap_t && PLATFORM_RENDERER_build_safe_sdl_src_rect(entry, u1, v1, u2, v2, &src_rect))
		{
			(void)PLATFORM_RENDERER_gles2_draw_textured_subrect_geometry(
					entry,
					platform_renderer_last_bound_texture_handle,
					&src_rect,
					vertices,
					4,
					indices,
					6,
					PLATFORM_RENDERER_GEOM_SRC_BOUND_QUAD);
			PLATFORM_RENDERER_gles2_apply_affine_state(prev_affine_active, prev_affine_row0, prev_affine_row1);
			return;
		}

		(void)PLATFORM_RENDERER_gles2_draw_textured_geometry(
				platform_renderer_last_bound_texture_handle,
				vertices,
				4,
				indices,
				6,
				PLATFORM_RENDERER_GEOM_SRC_BOUND_QUAD);
		PLATFORM_RENDERER_gles2_apply_affine_state(prev_affine_active, prev_affine_row0, prev_affine_row1);
		return;
	}
#endif

	/* Skip fully transparent draws early: SDL API calls have non-trivial CPU cost per-call.
	 * When alpha is zero the draw contributes nothing visible. */
	if (platform_renderer_current_colour_a < (1.0f / 256.0f))
		return;
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

void PLATFORM_RENDERER_draw_bound_textured_quad_translated(float offset_x, float offset_y, float left, float right, float up, float down, float u1, float v1, float u2, float v2)
{
#if defined(WIZBALL_RENDER_BACKEND_GLES2)
	if (PLATFORM_RENDERER_gles2_is_ready())
	{
		platform_renderer_texture_entry *entry = PLATFORM_RENDERER_get_texture_entry(platform_renderer_last_bound_texture_handle);
		SDL_Vertex vertices[4];
		const int indices[6] = {0, 1, 2, 0, 2, 3};
		SDL_Rect src_rect;
		bool wrap_s = false;
		bool wrap_t = false;
		const bool src_flip_x = (u2 < u1);
		const bool src_flip_y = PLATFORM_RENDERER_src_flip_y_for_sdl(v1, v2);
		const float quad_x[4] = {offset_x + left, offset_x + left, offset_x + right, offset_x + right};
		const float quad_y[4] = {offset_y + up, offset_y + down, offset_y + down, offset_y + up};
		bool prev_affine_active = false;
		GLfloat prev_affine_row0[3];
		GLfloat prev_affine_row1[3];
		int i;

		if ((entry == NULL) || !PLATFORM_RENDERER_build_gles2_texture_from_entry(entry))
		{
			return;
		}
		PLATFORM_RENDERER_gles2_capture_affine_state(&prev_affine_active, prev_affine_row0, prev_affine_row1);
		PLATFORM_RENDERER_gles2_set_affine_override_current_transform();

		(void)PLATFORM_RENDERER_uvs_require_wrap(u1, v1, u2, v2, &wrap_s, &wrap_t);
		for (i = 0; i < 4; i++)
		{
			int cr;
			int cg;
			int cb;
			int ca;

			vertices[i].position.x = quad_x[i];
			vertices[i].position.y = quad_y[i];
			vertices[i].tex_coord.x = (i < 2) ? (src_flip_x ? u2 : u1) : (src_flip_x ? u1 : u2);
			vertices[i].tex_coord.y = ((i == 0) || (i == 3)) ? (src_flip_y ? v2 : v1) : (src_flip_y ? v1 : v2);
			cr = (int)(platform_renderer_current_colour_r * 255.0f);
			cg = (int)(platform_renderer_current_colour_g * 255.0f);
			cb = (int)(platform_renderer_current_colour_b * 255.0f);
			ca = (int)(platform_renderer_current_colour_a * 255.0f);
			if (cr < 0) cr = 0; if (cr > 255) cr = 255;
			if (cg < 0) cg = 0; if (cg > 255) cg = 255;
			if (cb < 0) cb = 0; if (cb > 255) cb = 255;
			if (ca < 0) ca = 0; if (ca > 255) ca = 255;
			vertices[i].color.r = (Uint8)cr;
			vertices[i].color.g = (Uint8)cg;
			vertices[i].color.b = (Uint8)cb;
			vertices[i].color.a = (Uint8)ca;
		}

		if (!wrap_s && !wrap_t && PLATFORM_RENDERER_build_safe_sdl_src_rect(entry, u1, v1, u2, v2, &src_rect))
		{
			(void)PLATFORM_RENDERER_gles2_draw_textured_subrect_geometry(
					entry,
					platform_renderer_last_bound_texture_handle,
					&src_rect,
					vertices,
					4,
					indices,
					6,
					PLATFORM_RENDERER_GEOM_SRC_BOUND_QUAD);
			PLATFORM_RENDERER_gles2_apply_affine_state(prev_affine_active, prev_affine_row0, prev_affine_row1);
			return;
		}

		(void)PLATFORM_RENDERER_gles2_draw_textured_geometry(
				platform_renderer_last_bound_texture_handle,
				vertices,
				4,
				indices,
				6,
				PLATFORM_RENDERER_GEOM_SRC_BOUND_QUAD);
		PLATFORM_RENDERER_gles2_apply_affine_state(prev_affine_active, prev_affine_row0, prev_affine_row1);
		return;
	}
#endif

	PLATFORM_RENDERER_translatef(offset_x, offset_y, 0.0f);
	PLATFORM_RENDERER_draw_bound_textured_quad(left, right, up, down, u1, v1, u2, v2);
	PLATFORM_RENDERER_translatef(-offset_x, -offset_y, 0.0f);
}

void PLATFORM_RENDERER_draw_bound_textured_quad_custom(float x0, float y0, float x1, float y1, float x2, float y2, float x3, float y3, float u1, float v1, float u2, float v2)
{
#if defined(WIZBALL_RENDER_BACKEND_GLES2)
	if (PLATFORM_RENDERER_gles2_is_ready() && (platform_renderer_present_height > 0))
	{
		platform_renderer_texture_entry *entry = PLATFORM_RENDERER_get_texture_entry(platform_renderer_last_bound_texture_handle);
		SDL_Vertex vertices[4];
		const int indices[6] = {0, 1, 2, 0, 2, 3};
		SDL_Rect src_rect;
		bool wrap_s = false;
		bool wrap_t = false;
		const float quad_x[4] = {x0, x1, x2, x3};
		const float quad_y[4] = {y0, y1, y2, y3};
		float quad_u[4];
		float quad_v[4];
		const bool src_flip_x = (u2 < u1);
		const bool src_flip_y = PLATFORM_RENDERER_src_flip_y_for_sdl(v1, v2);
		bool prev_affine_active = false;
		GLfloat prev_affine_row0[3];
		GLfloat prev_affine_row1[3];

		if ((entry == NULL) || !PLATFORM_RENDERER_build_gles2_texture_from_entry(entry))
		{
			return;
		}
		PLATFORM_RENDERER_gles2_capture_affine_state(&prev_affine_active, prev_affine_row0, prev_affine_row1);
		PLATFORM_RENDERER_gles2_set_affine_override_current_transform();
		(void)PLATFORM_RENDERER_uvs_require_wrap(u1, v1, u2, v2, &wrap_s, &wrap_t);
		if (wrap_s || wrap_t || !PLATFORM_RENDERER_uvs_are_texel_aligned(entry, u1, v1, u2, v2))
		{
			float u_left;
			float u_right;
			float v_top;
			float v_bottom;

			if (wrap_s || wrap_t)
			{
				quad_u[0] = (src_flip_x ? u2 : u1);
				quad_u[1] = (src_flip_x ? u2 : u1);
				quad_u[2] = (src_flip_x ? u1 : u2);
				quad_u[3] = (src_flip_x ? u1 : u2);
				quad_v[0] = (src_flip_y ? v2 : v1);
				quad_v[1] = (src_flip_y ? v1 : v2);
				quad_v[2] = (src_flip_y ? v1 : v2);
				quad_v[3] = (src_flip_y ? v2 : v1);
			}
			else
			{
				PLATFORM_RENDERER_build_safe_gles2_uv_bounds(entry, u1, v1, u2, v2, &u_left, &v_top, &u_right, &v_bottom);
				quad_u[0] = u_left;
				quad_u[1] = u_left;
				quad_u[2] = u_right;
				quad_u[3] = u_right;
				quad_v[0] = v_top;
				quad_v[1] = v_bottom;
				quad_v[2] = v_bottom;
				quad_v[3] = v_top;
			}

			for (int i = 0; i < 4; i++)
			{
				int cr;
				int cg;
				int cb;
				int ca;

				vertices[i].position.x = quad_x[i];
				vertices[i].position.y = quad_y[i];
				vertices[i].tex_coord.x = quad_u[i];
				vertices[i].tex_coord.y = quad_v[i];
				cr = (int)(platform_renderer_current_colour_r * 255.0f);
				cg = (int)(platform_renderer_current_colour_g * 255.0f);
				cb = (int)(platform_renderer_current_colour_b * 255.0f);
				ca = (int)(platform_renderer_current_colour_a * 255.0f);
				if (cr < 0) cr = 0; if (cr > 255) cr = 255;
				if (cg < 0) cg = 0; if (cg > 255) cg = 255;
				if (cb < 0) cb = 0; if (cb > 255) cb = 255;
				if (ca < 0) ca = 0; if (ca > 255) ca = 255;
				vertices[i].color.r = (Uint8)cr;
				vertices[i].color.g = (Uint8)cg;
				vertices[i].color.b = (Uint8)cb;
				vertices[i].color.a = (Uint8)ca;
			}
			(void)PLATFORM_RENDERER_gles2_draw_textured_geometry(platform_renderer_last_bound_texture_handle, vertices, 4, indices, 6, PLATFORM_RENDERER_GEOM_SRC_BOUND_QUAD_CUSTOM);
			PLATFORM_RENDERER_gles2_apply_affine_state(prev_affine_active, prev_affine_row0, prev_affine_row1);
			return;
		}

		if (!PLATFORM_RENDERER_build_safe_sdl_src_rect(entry, u1, v1, u2, v2, &src_rect))
		{
			return;
		}
		for (int i = 0; i < 4; i++)
		{
			int cr;
			int cg;
			int cb;
			int ca;
			vertices[i].position.x = quad_x[i];
			vertices[i].position.y = quad_y[i];
			vertices[i].tex_coord.x = (i == 0 || i == 1) ? (src_flip_x ? u2 : u1) : (src_flip_x ? u1 : u2);
			vertices[i].tex_coord.y = (i == 0 || i == 3) ? (src_flip_y ? v2 : v1) : (src_flip_y ? v1 : v2);
			cr = (int)(platform_renderer_current_colour_r * 255.0f);
			cg = (int)(platform_renderer_current_colour_g * 255.0f);
			cb = (int)(platform_renderer_current_colour_b * 255.0f);
			ca = (int)(platform_renderer_current_colour_a * 255.0f);
			if (cr < 0) cr = 0; if (cr > 255) cr = 255;
			if (cg < 0) cg = 0; if (cg > 255) cg = 255;
			if (cb < 0) cb = 0; if (cb > 255) cb = 255;
			if (ca < 0) ca = 0; if (ca > 255) ca = 255;
			vertices[i].color.r = (Uint8)cr;
			vertices[i].color.g = (Uint8)cg;
			vertices[i].color.b = (Uint8)cb;
			vertices[i].color.a = (Uint8)ca;
		}
		(void)PLATFORM_RENDERER_gles2_draw_textured_subrect_geometry(entry, platform_renderer_last_bound_texture_handle, &src_rect, vertices, 4, indices, 6, PLATFORM_RENDERER_GEOM_SRC_BOUND_QUAD_CUSTOM);
		PLATFORM_RENDERER_gles2_apply_affine_state(prev_affine_active, prev_affine_row0, prev_affine_row1);
		return;
	}
#endif
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
#if defined(WIZBALL_RENDER_BACKEND_GLES2)
	if (PLATFORM_RENDERER_gles2_is_ready() && (platform_renderer_present_height > 0))
	{
		float tx;
		float ty;
		PLATFORM_RENDERER_transform_point(x, y, &tx, &ty);
		(void)PLATFORM_RENDERER_gles2_draw_coloured_line_segment(
				tx,
				(float)platform_renderer_present_height - ty,
				tx,
				(float)platform_renderer_present_height - ty,
				PLATFORM_RENDERER_clamp_sdl_unit_to_byte(platform_renderer_current_colour_r),
				PLATFORM_RENDERER_clamp_sdl_unit_to_byte(platform_renderer_current_colour_g),
				PLATFORM_RENDERER_clamp_sdl_unit_to_byte(platform_renderer_current_colour_b),
				PLATFORM_RENDERER_clamp_sdl_unit_to_byte(platform_renderer_current_colour_a));
		return;
	}
#endif
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
#if defined(WIZBALL_RENDER_BACKEND_GLES2)
	if (PLATFORM_RENDERER_gles2_is_ready() && (platform_renderer_present_height > 0))
	{
		float tx;
		float ty;
		PLATFORM_RENDERER_transform_point(x, y, &tx, &ty);
		(void)PLATFORM_RENDERER_gles2_draw_coloured_line_segment(
				tx,
				(float)platform_renderer_present_height - ty,
				tx,
				(float)platform_renderer_present_height - ty,
				PLATFORM_RENDERER_clamp_sdl_unit_to_byte(r),
				PLATFORM_RENDERER_clamp_sdl_unit_to_byte(g),
				PLATFORM_RENDERER_clamp_sdl_unit_to_byte(b),
				PLATFORM_RENDERER_clamp_sdl_unit_to_byte(platform_renderer_current_colour_a));
		return;
	}
#endif
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
		PLATFORM_RENDERER_reset_transform_state();
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

void PLATFORM_RENDERER_bind_secondary_texture(unsigned int texture_handle)
{
	unsigned int resolved_texture_id = PLATFORM_RENDERER_resolve_legacy_texture(texture_handle);
	unsigned int normalized_texture_handle = PLATFORM_RENDERER_normalize_texture_handle(texture_handle);
	(void)resolved_texture_id;
	platform_renderer_last_bound_secondary_texture_handle = normalized_texture_handle;
}

void PLATFORM_RENDERER_set_active_texture_proc(void *proc)
{
	platform_renderer_active_texture_proc = (platform_active_texture_proc_t)proc;
}

bool PLATFORM_RENDERER_is_active_texture_available(void)
{
	return platform_renderer_active_texture_proc != NULL;
}

void PLATFORM_RENDERER_set_combiner_modulate_primary(void)
{
	platform_renderer_combiner_mode = PLATFORM_RENDERER_COMBINER_MODULATE_PRIMARY;
}

void PLATFORM_RENDERER_set_combiner_replace_rgb_modulate_alpha_previous(void)
{
	platform_renderer_combiner_mode = PLATFORM_RENDERER_COMBINER_REPLACE_RGB_MODULATE_ALPHA_PREVIOUS;
}

void PLATFORM_RENDERER_set_combiner_replace_rgb_modulate_alpha_primary(void)
{
	platform_renderer_combiner_mode = PLATFORM_RENDERER_COMBINER_REPLACE_RGB_MODULATE_ALPHA_PRIMARY;
}

void PLATFORM_RENDERER_set_combiner_modulate_rgb_replace_alpha_previous(void)
{
	platform_renderer_combiner_mode = PLATFORM_RENDERER_COMBINER_MODULATE_RGB_REPLACE_ALPHA_PREVIOUS;
}

void PLATFORM_RENDERER_set_blend_enabled(bool enabled)
{
#if defined(WIZBALL_RENDER_BACKEND_GLES2)
	if (PLATFORM_RENDERER_gles2_is_ready() && platform_renderer_gles2_textured_batch_active &&
			(platform_renderer_blend_enabled != enabled))
	{
		platform_renderer_gles2_flush_state_change_count++;
		PLATFORM_RENDERER_gles2_flush_textured_batch();
	}
#endif
	platform_renderer_blend_enabled = enabled;
}

void PLATFORM_RENDERER_set_blend_mode_additive(void)
{
#if defined(WIZBALL_RENDER_BACKEND_GLES2)
	if (PLATFORM_RENDERER_gles2_is_ready() && platform_renderer_gles2_textured_batch_active &&
			(platform_renderer_blend_mode != PLATFORM_RENDERER_BLEND_MODE_ADD))
	{
		platform_renderer_gles2_flush_state_change_count++;
		PLATFORM_RENDERER_gles2_flush_textured_batch();
	}
#endif
	platform_renderer_blend_mode = PLATFORM_RENDERER_BLEND_MODE_ADD;
}

void PLATFORM_RENDERER_set_blend_mode_multiply(void)
{
#if defined(WIZBALL_RENDER_BACKEND_GLES2)
	if (PLATFORM_RENDERER_gles2_is_ready() && platform_renderer_gles2_textured_batch_active &&
			(platform_renderer_blend_mode != PLATFORM_RENDERER_BLEND_MODE_MULTIPLY))
	{
		platform_renderer_gles2_flush_state_change_count++;
		PLATFORM_RENDERER_gles2_flush_textured_batch();
	}
#endif
	platform_renderer_blend_mode = PLATFORM_RENDERER_BLEND_MODE_MULTIPLY;
}

void PLATFORM_RENDERER_set_blend_mode_alpha(void)
{
#if defined(WIZBALL_RENDER_BACKEND_GLES2)
	if (PLATFORM_RENDERER_gles2_is_ready() && platform_renderer_gles2_textured_batch_active &&
			(platform_renderer_blend_mode != PLATFORM_RENDERER_BLEND_MODE_ALPHA))
	{
		platform_renderer_gles2_flush_state_change_count++;
		PLATFORM_RENDERER_gles2_flush_textured_batch();
	}
#endif
	platform_renderer_blend_mode = PLATFORM_RENDERER_BLEND_MODE_ALPHA;
}

void PLATFORM_RENDERER_set_blend_mode_subtractive(void)
{
#if defined(WIZBALL_RENDER_BACKEND_GLES2)
	if (PLATFORM_RENDERER_gles2_is_ready() && platform_renderer_gles2_textured_batch_active &&
			(platform_renderer_blend_mode != PLATFORM_RENDERER_BLEND_MODE_SUBTRACT))
	{
		platform_renderer_gles2_flush_state_change_count++;
		PLATFORM_RENDERER_gles2_flush_textured_batch();
	}
#endif
	platform_renderer_blend_mode = PLATFORM_RENDERER_BLEND_MODE_SUBTRACT;
}

void PLATFORM_RENDERER_set_texture_filter(bool filtered)
{
#if defined(WIZBALL_RENDER_BACKEND_GLES2)
	if (PLATFORM_RENDERER_gles2_is_ready() && platform_renderer_gles2_textured_batch_active &&
			(platform_renderer_texture_filter_linear != filtered))
	{
		platform_renderer_gles2_flush_state_change_count++;
		PLATFORM_RENDERER_gles2_flush_textured_batch();
	}
#endif
	platform_renderer_texture_filter_linear = filtered;
}

void PLATFORM_RENDERER_set_texture_masked(bool masked)
{
#if defined(WIZBALL_RENDER_BACKEND_GLES2)
	if (PLATFORM_RENDERER_gles2_is_ready() && platform_renderer_gles2_textured_batch_active &&
			(platform_renderer_texture_masked != masked))
	{
		platform_renderer_gles2_flush_state_change_count++;
		PLATFORM_RENDERER_gles2_flush_textured_batch();
	}
#endif
	platform_renderer_texture_masked = masked;
}

void PLATFORM_RENDERER_capture_state_snapshot(platform_renderer_state_snapshot *snapshot)
{
	if (snapshot == NULL)
	{
		return;
	}

	snapshot->texture_handle = platform_renderer_last_bound_texture_handle;
	snapshot->blend_enabled = platform_renderer_blend_enabled;
	snapshot->blend_mode = platform_renderer_blend_mode;
	snapshot->filtered = platform_renderer_texture_filter_linear;
	snapshot->masked = platform_renderer_texture_masked;
	snapshot->scissor_enabled = (platform_renderer_legacy_scissor_enabled_last != 0);
	snapshot->scissor_x = platform_renderer_legacy_scissor_last[0];
	snapshot->scissor_y = platform_renderer_legacy_scissor_last[1];
	snapshot->scissor_w = platform_renderer_legacy_scissor_last[2];
	snapshot->scissor_h = platform_renderer_legacy_scissor_last[3];
	snapshot->colour_r = platform_renderer_current_colour_r;
	snapshot->colour_g = platform_renderer_current_colour_g;
	snapshot->colour_b = platform_renderer_current_colour_b;
	snapshot->colour_a = platform_renderer_current_colour_a;
	snapshot->tx_a = platform_renderer_tx_a;
	snapshot->tx_b = platform_renderer_tx_b;
	snapshot->tx_c = platform_renderer_tx_c;
	snapshot->tx_d = platform_renderer_tx_d;
	snapshot->tx_x = platform_renderer_tx_x;
	snapshot->tx_y = platform_renderer_tx_y;
}

void PLATFORM_RENDERER_apply_state_snapshot(const platform_renderer_state_snapshot *snapshot)
{
	if (snapshot == NULL)
	{
		return;
	}

	PLATFORM_RENDERER_flush_command_queue();
	PLATFORM_RENDERER_flush_addq();
#if defined(WIZBALL_RENDER_BACKEND_GLES2)
	platform_renderer_gles2_flush_window_finish_count++;
	PLATFORM_RENDERER_gles2_flush_textured_batch();
#endif
	platform_renderer_last_bound_texture_handle = snapshot->texture_handle;
	platform_renderer_blend_enabled = snapshot->blend_enabled;
	platform_renderer_blend_mode = snapshot->blend_mode;
	platform_renderer_texture_filter_linear = snapshot->filtered;
	platform_renderer_texture_masked = snapshot->masked;
	platform_renderer_legacy_scissor_enabled_last = snapshot->scissor_enabled ? 1 : 0;
	platform_renderer_legacy_scissor_last[0] = snapshot->scissor_x;
	platform_renderer_legacy_scissor_last[1] = snapshot->scissor_y;
	platform_renderer_legacy_scissor_last[2] = snapshot->scissor_w;
	platform_renderer_legacy_scissor_last[3] = snapshot->scissor_h;
	platform_renderer_current_colour_r = snapshot->colour_r;
	platform_renderer_current_colour_g = snapshot->colour_g;
	platform_renderer_current_colour_b = snapshot->colour_b;
	platform_renderer_current_colour_a = snapshot->colour_a;
	platform_renderer_tx_a = snapshot->tx_a;
	platform_renderer_tx_b = snapshot->tx_b;
	platform_renderer_tx_c = snapshot->tx_c;
	platform_renderer_tx_d = snapshot->tx_d;
	platform_renderer_tx_x = snapshot->tx_x;
	platform_renderer_tx_y = snapshot->tx_y;
#if defined(WIZBALL_RENDER_BACKEND_GLES2)
	if (PLATFORM_RENDERER_gles2_is_ready())
	{
		if (snapshot->scissor_enabled && (snapshot->scissor_w > 0) && (snapshot->scissor_h > 0))
		{
			PLATFORM_RENDERER_gles2_apply_scissor_rect(
				snapshot->scissor_x,
				snapshot->scissor_y,
				snapshot->scissor_w,
				snapshot->scissor_h);
		}
		else
		{
			glDisable(GL_SCISSOR_TEST);
		}
	}
	else
#endif
	if (platform_renderer_sdl_renderer != NULL)
	{
		if (snapshot->scissor_enabled && (snapshot->scissor_w > 0) && (snapshot->scissor_h > 0))
		{
			SDL_Rect clip;
			clip.x = snapshot->scissor_x;
			clip.y = snapshot->scissor_y;
			clip.w = snapshot->scissor_w;
			clip.h = snapshot->scissor_h;
			(void)SDL_RenderSetClipRect(platform_renderer_sdl_renderer, &clip);
			platform_renderer_sdl_clip_cache_enabled = true;
			platform_renderer_sdl_clip_cache = clip;
		}
		else
		{
			(void)SDL_RenderSetClipRect(platform_renderer_sdl_renderer, NULL);
			platform_renderer_sdl_clip_cache_enabled = false;
		}
	}
}

void PLATFORM_RENDERER_set_window_scissor(float left_window_transform_x, float bottom_window_transform_y, float scaled_width, float scaled_height, float display_scale_x, float display_scale_y, float window_scale_multiplier)
{
	if (platform_renderer_command_queue_count > 0)
	{
		PLATFORM_RENDERER_flush_command_queue();
	}
#if defined(WIZBALL_RENDER_BACKEND_GLES2)
	PLATFORM_RENDERER_gles2_flush_textured_batch();
#endif

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

#if defined(WIZBALL_RENDER_BACKEND_GLES2)
	if (PLATFORM_RENDERER_gles2_is_ready() && (platform_renderer_present_height > 0))
	{
		const float effective_multiplier = (window_scale_multiplier > 0.0f) ? window_scale_multiplier : 1.0f;
		int clip_x = (int)(x * effective_multiplier);
		int clip_w = (int)(width * effective_multiplier);
		int clip_h = (int)(height * effective_multiplier);
		int clip_y = platform_renderer_present_height - (((int)(y * effective_multiplier)) + clip_h);
		const bool effectively_fullscreen =
			(clip_x <= 1) &&
			(clip_y <= 1) &&
			((clip_x + clip_w) >= (platform_renderer_present_width - 1)) &&
			((clip_y + clip_h) >= (platform_renderer_present_height - 1));
		platform_renderer_legacy_scissor_last[0] = clip_x;
		platform_renderer_legacy_scissor_last[1] = clip_y;
		platform_renderer_legacy_scissor_last[2] = clip_w;
		platform_renderer_legacy_scissor_last[3] = clip_h;
		if ((clip_w > 0) && (clip_h > 0))
		{
			if (effectively_fullscreen)
			{
				platform_renderer_legacy_scissor_enabled_last = 0;
				glDisable(GL_SCISSOR_TEST);
			}
			else
			{
				platform_renderer_legacy_scissor_enabled_last = 1;
				PLATFORM_RENDERER_gles2_apply_scissor_rect(clip_x, clip_y, clip_w, clip_h);
			}
		}
		else
		{
			platform_renderer_legacy_scissor_enabled_last = 0;
			glDisable(GL_SCISSOR_TEST);
		}
		return;
	}
#endif

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
				platform_renderer_legacy_scissor_enabled_last = 0;
				memset(platform_renderer_legacy_scissor_last, 0, sizeof(platform_renderer_legacy_scissor_last));
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
					platform_renderer_legacy_scissor_enabled_last = 0;
					memset(platform_renderer_legacy_scissor_last, 0, sizeof(platform_renderer_legacy_scissor_last));
					if (platform_renderer_sdl_clip_cache_enabled)
					{
						PLATFORM_RENDERER_flush_addq();
						(void)SDL_RenderSetClipRect(platform_renderer_sdl_renderer, NULL);
						platform_renderer_sdl_clip_cache_enabled = false;
						platform_renderer_sdl_clip_switch_count++;
					}
					return;
				}
				platform_renderer_legacy_scissor_enabled_last = 1;
				platform_renderer_legacy_scissor_last[0] = clip.x;
				platform_renderer_legacy_scissor_last[1] = clip.y;
				platform_renderer_legacy_scissor_last[2] = clip.w;
				platform_renderer_legacy_scissor_last[3] = clip.h;
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
		PLATFORM_RENDERER_reset_transform_state();
		return;
	}
	platform_renderer_tx_x += (platform_renderer_tx_a * x) + (platform_renderer_tx_c * y);
	platform_renderer_tx_y += (platform_renderer_tx_b * x) + (platform_renderer_tx_d * y);
	if (!PLATFORM_RENDERER_transform_state_is_finite())
	{
		PLATFORM_RENDERER_reset_transform_state();
	}
}

void PLATFORM_RENDERER_scalef(float x, float y, float z)
{
	if (!isfinite(x) || !isfinite(y) || !isfinite(z))
	{
		PLATFORM_RENDERER_reset_transform_state();
		return;
	}
	platform_renderer_tx_a *= x;
	platform_renderer_tx_b *= x;
	platform_renderer_tx_c *= y;
	platform_renderer_tx_d *= y;
	if (!PLATFORM_RENDERER_transform_state_is_finite())
	{
		PLATFORM_RENDERER_reset_transform_state();
	}
}

void PLATFORM_RENDERER_rotatef(float angle_degrees, float x, float y, float z)
{
	if (!isfinite(angle_degrees) || !isfinite(x) || !isfinite(y) || !isfinite(z))
	{
		PLATFORM_RENDERER_reset_transform_state();
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
		PLATFORM_RENDERER_reset_transform_state();
	}
}

void PLATFORM_RENDERER_finish_textured_window_draw(bool texture_combiner_available, bool had_secondary_texture, bool secondary_colour_available, bool secondary_window_colour, int game_screen_width, int game_screen_height)
{
	if (platform_renderer_command_queue_count > 0)
	{
		PLATFORM_RENDERER_flush_command_queue();
	}
#if defined(WIZBALL_RENDER_BACKEND_GLES2)
	PLATFORM_RENDERER_gles2_flush_textured_batch();
#endif

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
	platform_renderer_last_bound_secondary_texture_handle = 0;
	platform_renderer_combiner_mode = PLATFORM_RENDERER_COMBINER_MODULATE_PRIMARY;
}

void PLATFORM_RENDERER_draw_bound_multitextured_quad_array(const float *x, const float *y, const float *u0, const float *v0, const float *u1, const float *v1)
{
	if (platform_renderer_command_queue_count > 0)
	{
		PLATFORM_RENDERER_flush_command_queue();
	}

	int i;

	if ((x == NULL) || (y == NULL) || (u0 == NULL) || (v0 == NULL) || (u1 == NULL) || (v1 == NULL))
	{
		return;
	}
#if defined(WIZBALL_RENDER_BACKEND_GLES2)
	if (PLATFORM_RENDERER_gles2_is_ready() && (platform_renderer_present_height > 0))
	{
		float positions[8];
		float primary_uv[8];
		float secondary_uv[8];
		unsigned char colours[16];
		const int indices[6] = {0, 1, 2, 0, 2, 3};
		bool prev_affine_active = false;
		GLfloat prev_affine_row0[3];
		GLfloat prev_affine_row1[3];
		PLATFORM_RENDERER_gles2_capture_affine_state(&prev_affine_active, prev_affine_row0, prev_affine_row1);
		PLATFORM_RENDERER_gles2_set_affine_override_current_transform();
		for (i = 0; i < 4; i++)
		{
			int cr;
			int cg;
			int cb;
			int ca;

			positions[(i * 2) + 0] = x[i];
			positions[(i * 2) + 1] = y[i];
			primary_uv[(i * 2) + 0] = u0[i];
			primary_uv[(i * 2) + 1] = v0[i];
			secondary_uv[(i * 2) + 0] = u1[i];
			secondary_uv[(i * 2) + 1] = v1[i];

			cr = (int)(platform_renderer_current_colour_r * 255.0f);
			cg = (int)(platform_renderer_current_colour_g * 255.0f);
			cb = (int)(platform_renderer_current_colour_b * 255.0f);
			ca = (int)(platform_renderer_current_colour_a * 255.0f);
			if (cr < 0) cr = 0; else if (cr > 255) cr = 255;
			if (cg < 0) cg = 0; else if (cg > 255) cg = 255;
			if (cb < 0) cb = 0; else if (cb > 255) cb = 255;
			if (ca < 0) ca = 0; else if (ca > 255) ca = 255;

			colours[(i * 4) + 0] = (unsigned char)cr;
			colours[(i * 4) + 1] = (unsigned char)cg;
			colours[(i * 4) + 2] = (unsigned char)cb;
			colours[(i * 4) + 3] = (unsigned char)ca;
		}

		(void)PLATFORM_RENDERER_gles2_draw_multitextured_geometry(
				platform_renderer_last_bound_texture_handle,
				platform_renderer_last_bound_secondary_texture_handle,
				platform_renderer_combiner_mode,
				positions,
				primary_uv,
				secondary_uv,
				colours,
				4,
				indices,
				6,
				PLATFORM_RENDERER_GEOM_SRC_MULTITEXTURE_ARRAY);
		PLATFORM_RENDERER_gles2_apply_affine_state(prev_affine_active, prev_affine_row0, prev_affine_row1);
		return;
	}
#endif
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
	if (platform_renderer_command_queue_count > 0)
	{
		PLATFORM_RENDERER_flush_command_queue();
	}

	int i;

	if ((x == NULL) || (y == NULL) || (u == NULL) || (v == NULL) || (r == NULL) || (g == NULL) || (b == NULL) || (a == NULL))
	{
		return;
	}
#if defined(WIZBALL_RENDER_BACKEND_GLES2)
	if (PLATFORM_RENDERER_gles2_is_ready() && (platform_renderer_present_height > 0))
	{
		platform_renderer_texture_entry *entry = PLATFORM_RENDERER_get_texture_entry(platform_renderer_last_bound_texture_handle);
		SDL_Vertex vertices[4];
		const int indices[6] = {0, 1, 2, 0, 2, 3};
		SDL_Rect src_rect;
		bool wrap_s = false;
		bool wrap_t = false;
		bool uniform_colour = true;
		bool prev_affine_active = false;
		GLfloat prev_affine_row0[3];
		GLfloat prev_affine_row1[3];
		PLATFORM_RENDERER_gles2_capture_affine_state(&prev_affine_active, prev_affine_row0, prev_affine_row1);
		PLATFORM_RENDERER_gles2_set_affine_override_current_transform();
		for (i = 0; i < 4; i++)
		{
			vertices[i].position.x = x[i];
			vertices[i].position.y = y[i];
			vertices[i].tex_coord.x = u[i];
			vertices[i].tex_coord.y = v[i];
			vertices[i].color.r = PLATFORM_RENDERER_clamp_sdl_unit_to_byte(r[i]);
			vertices[i].color.g = PLATFORM_RENDERER_clamp_sdl_unit_to_byte(g[i]);
			vertices[i].color.b = PLATFORM_RENDERER_clamp_sdl_unit_to_byte(b[i]);
			vertices[i].color.a = PLATFORM_RENDERER_clamp_sdl_unit_to_byte(a[i]);
		}
		for (i = 1; i < 4; i++)
		{
			if ((fabsf(r[i] - r[0]) > 0.01f) ||
					(fabsf(g[i] - g[0]) > 0.01f) ||
					(fabsf(b[i] - b[0]) > 0.01f) ||
					(fabsf(a[i] - a[0]) > 0.01f))
			{
				uniform_colour = false;
				break;
			}
		}

		if (uniform_colour)
		{
			const float old_r = platform_renderer_current_colour_r;
			const float old_g = platform_renderer_current_colour_g;
			const float old_b = platform_renderer_current_colour_b;
			const float old_a = platform_renderer_current_colour_a;
			const bool axis_aligned_rect =
				(fabsf(x[0] - x[1]) < 0.01f) &&
				(fabsf(x[2] - x[3]) < 0.01f) &&
				(fabsf(y[0] - y[3]) < 0.01f) &&
				(fabsf(y[1] - y[2]) < 0.01f);
			platform_renderer_current_colour_r = r[0];
			platform_renderer_current_colour_g = g[0];
			platform_renderer_current_colour_b = b[0];
			platform_renderer_current_colour_a = a[0];
			if (axis_aligned_rect)
			{
				PLATFORM_RENDERER_draw_bound_textured_quad(
						x[0],
						x[2],
						y[0],
						y[1],
						u[0],
						v[0],
						u[2],
						v[2]);
			}
			else
			{
				PLATFORM_RENDERER_draw_bound_textured_quad_custom(
						x[0], y[0],
						x[1], y[1],
						x[2], y[2],
						x[3], y[3],
						u[0], v[0], u[2], v[2]);
			}
			platform_renderer_current_colour_r = old_r;
			platform_renderer_current_colour_g = old_g;
			platform_renderer_current_colour_b = old_b;
			platform_renderer_current_colour_a = old_a;
			PLATFORM_RENDERER_gles2_apply_affine_state(prev_affine_active, prev_affine_row0, prev_affine_row1);
			return;
		}

		(void)PLATFORM_RENDERER_uvs_require_wrap(u[0], v[0], u[2], v[2], &wrap_s, &wrap_t);
		if ((entry != NULL) &&
				!wrap_s &&
				!wrap_t &&
				PLATFORM_RENDERER_uvs_are_texel_aligned(entry, u[0], v[0], u[2], v[2]) &&
				PLATFORM_RENDERER_build_safe_sdl_src_rect(entry, u[0], v[0], u[2], v[2], &src_rect))
		{
			SDL_Vertex direct_vertices[4];
			const float src_u_min = (float)src_rect.x / (float)entry->width;
			const float src_u_max = (float)(src_rect.x + src_rect.w) / (float)entry->width;
			const float src_v_min = (float)src_rect.y / (float)entry->height;
			const float src_v_max = (float)(src_rect.y + src_rect.h) / (float)entry->height;

			for (i = 0; i < 4; i++)
			{
				direct_vertices[i] = vertices[i];
				direct_vertices[i].tex_coord.y = PLATFORM_RENDERER_convert_v_to_sdl(vertices[i].tex_coord.y);
				if (direct_vertices[i].tex_coord.x < src_u_min) direct_vertices[i].tex_coord.x = src_u_min;
				if (direct_vertices[i].tex_coord.x > src_u_max) direct_vertices[i].tex_coord.x = src_u_max;
				if (direct_vertices[i].tex_coord.y < src_v_min) direct_vertices[i].tex_coord.y = src_v_min;
				if (direct_vertices[i].tex_coord.y > src_v_max) direct_vertices[i].tex_coord.y = src_v_max;
			}

			(void)PLATFORM_RENDERER_gles2_draw_textured_geometry_gltex(
					entry->gles2_texture,
					platform_renderer_last_bound_texture_handle,
					direct_vertices,
					4,
					indices,
					6,
					PLATFORM_RENDERER_GEOM_SRC_COLOURED_ARRAY,
					false,
					false);
		}
		else
		{
			(void)PLATFORM_RENDERER_gles2_draw_textured_geometry(platform_renderer_last_bound_texture_handle, vertices, 4, indices, 6, PLATFORM_RENDERER_GEOM_SRC_COLOURED_ARRAY);
		}
		PLATFORM_RENDERER_gles2_apply_affine_state(prev_affine_active, prev_affine_row0, prev_affine_row1);
		return;
	}
#endif
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
	platform_renderer_command *cmd;

#if defined(WIZBALL_RENDER_BACKEND_GLES2)
	if (PLATFORM_RENDERER_gles2_is_ready())
	{
		PLATFORM_RENDERER_draw_bound_perspective_textured_quad_immediate(
				x0, y0, x1, y1, x2, y2, x3, y3, u1, v1, u2, v2, q);
		return;
	}
#endif

	if (platform_renderer_command_queue_count >= PLATFORM_RENDERER_CMDQ_MAX)
	{
		PLATFORM_RENDERER_flush_command_queue();
	}

	cmd = &platform_renderer_command_queue[platform_renderer_command_queue_count++];
	memset(cmd, 0, sizeof(*cmd));
	cmd->type = PLATFORM_RENDERER_CMD_PERSPECTIVE;
	cmd->texture_handle = platform_renderer_last_bound_texture_handle;
	cmd->blend_enabled = platform_renderer_blend_enabled;
	cmd->blend_mode = platform_renderer_blend_mode;
	cmd->colour_r = platform_renderer_current_colour_r;
	cmd->colour_g = platform_renderer_current_colour_g;
	cmd->colour_b = platform_renderer_current_colour_b;
	cmd->colour_a = platform_renderer_current_colour_a;
	cmd->tx_a = platform_renderer_tx_a;
	cmd->tx_b = platform_renderer_tx_b;
	cmd->tx_c = platform_renderer_tx_c;
	cmd->tx_d = platform_renderer_tx_d;
	cmd->tx_x = platform_renderer_tx_x;
	cmd->tx_y = platform_renderer_tx_y;
	cmd->x0 = x0;
	cmd->y0 = y0;
	cmd->x1 = x1;
	cmd->y1 = y1;
	cmd->x2 = x2;
	cmd->y2 = y2;
	cmd->x3 = x3;
	cmd->y3 = y3;
	cmd->u1 = u1;
	cmd->v1 = v1;
	cmd->u2 = u2;
	cmd->v2 = v2;
	cmd->q = q;
}

void PLATFORM_RENDERER_draw_bound_perspective_textured_quad_batch(const platform_renderer_perspective_quad *quads, int quad_count)
{
	if ((quads == NULL) || (quad_count <= 0))
	{
		return;
	}

	if (!PLATFORM_RENDERER_draw_bound_perspective_textured_quad_batch_immediate(quads, quad_count))
	{
		int i;
		for (i = 0; i < quad_count; i++)
		{
			const platform_renderer_perspective_quad *quad = &quads[i];
			PLATFORM_RENDERER_draw_bound_perspective_textured_quad_immediate(
				quad->x0, quad->y0, quad->x1, quad->y1, quad->x2, quad->y2, quad->x3, quad->y3,
				quad->u1, quad->v1, quad->u2, quad->v2, quad->q);
		}
	}
}

void PLATFORM_RENDERER_draw_bound_multitextured_perspective_quad(float x0, float y0, float x1, float y1, float x2, float y2, float x3, float y3, float u0_1, float v0_1, float u0_2, float v0_2, float u1_1, float v1_1, float u1_2, float v1_2, float q)
{
	if (platform_renderer_command_queue_count > 0)
	{
		PLATFORM_RENDERER_flush_command_queue();
	}

#if defined(WIZBALL_RENDER_BACKEND_GLES2)
	if (PLATFORM_RENDERER_gles2_is_ready() && (platform_renderer_present_height > 0))
	{
		(void)PLATFORM_RENDERER_gles2_draw_multitextured_perspective_quad(
				x0, y0, x1, y1, x2, y2, x3, y3,
				u0_1, v0_1, u0_2, v0_2,
				u1_1, v1_1, u1_2, v1_2,
				q);
	}
#else
	(void)x0; (void)y0; (void)x1; (void)y1; (void)x2; (void)y2; (void)x3; (void)y3;
	(void)u0_1; (void)v0_1; (void)u0_2; (void)v0_2;
	(void)u1_1; (void)v1_1; (void)u1_2; (void)v1_2; (void)q;
#endif
}

static void PLATFORM_RENDERER_draw_bound_perspective_textured_quad_immediate(float x0, float y0, float x1, float y1, float x2, float y2, float x3, float y3, float u1, float v1, float u2, float v2, float q)
{
#if defined(WIZBALL_RENDER_BACKEND_GLES2)
	if (PLATFORM_RENDERER_gles2_is_ready())
	{
		platform_renderer_perspective_quad quad;
		quad.x0 = x0; quad.y0 = y0;
		quad.x1 = x1; quad.y1 = y1;
		quad.x2 = x2; quad.y2 = y2;
		quad.x3 = x3; quad.y3 = y3;
		quad.u1 = u1; quad.v1 = v1;
		quad.u2 = u2; quad.v2 = v2;
		quad.q = q;
		(void)PLATFORM_RENDERER_draw_bound_perspective_textured_quad_batch_immediate(&quad, 1);
		return;
	}
#endif
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
	platform_renderer_command *cmd;

#if defined(WIZBALL_RENDER_BACKEND_GLES2)
	if (PLATFORM_RENDERER_gles2_is_ready())
	{
		PLATFORM_RENDERER_draw_bound_coloured_perspective_textured_quad_immediate(
				x0, y0, x1, y1, x2, y2, x3, y3, u1, v1, u2, v2, q, r, g, b, a);
		return;
	}
#endif

	if (platform_renderer_command_queue_count >= PLATFORM_RENDERER_CMDQ_MAX)
	{
		PLATFORM_RENDERER_flush_command_queue();
	}

	cmd = &platform_renderer_command_queue[platform_renderer_command_queue_count++];
	memset(cmd, 0, sizeof(*cmd));
	cmd->type = PLATFORM_RENDERER_CMD_COLOURED_PERSPECTIVE;
	cmd->texture_handle = platform_renderer_last_bound_texture_handle;
	cmd->blend_enabled = platform_renderer_blend_enabled;
	cmd->blend_mode = platform_renderer_blend_mode;
	cmd->colour_r = platform_renderer_current_colour_r;
	cmd->colour_g = platform_renderer_current_colour_g;
	cmd->colour_b = platform_renderer_current_colour_b;
	cmd->colour_a = platform_renderer_current_colour_a;
	cmd->tx_a = platform_renderer_tx_a;
	cmd->tx_b = platform_renderer_tx_b;
	cmd->tx_c = platform_renderer_tx_c;
	cmd->tx_d = platform_renderer_tx_d;
	cmd->tx_x = platform_renderer_tx_x;
	cmd->tx_y = platform_renderer_tx_y;
	cmd->x0 = x0;
	cmd->y0 = y0;
	cmd->x1 = x1;
	cmd->y1 = y1;
	cmd->x2 = x2;
	cmd->y2 = y2;
	cmd->x3 = x3;
	cmd->y3 = y3;
	cmd->u1 = u1;
	cmd->v1 = v1;
	cmd->u2 = u2;
	cmd->v2 = v2;
	cmd->q = q;
	memcpy(cmd->vr, r, sizeof(cmd->vr));
	memcpy(cmd->vg, g, sizeof(cmd->vg));
	memcpy(cmd->vb, b, sizeof(cmd->vb));
	memcpy(cmd->va, a, sizeof(cmd->va));
}

void PLATFORM_RENDERER_draw_bound_coloured_perspective_textured_quad_batch(const platform_renderer_coloured_perspective_quad *quads, int quad_count)
{
	if ((quads == NULL) || (quad_count <= 0))
	{
		return;
	}

	if (!PLATFORM_RENDERER_draw_bound_coloured_perspective_textured_quad_batch_immediate(quads, quad_count))
	{
		int i;
		for (i = 0; i < quad_count; i++)
		{
			const platform_renderer_coloured_perspective_quad *quad = &quads[i];
			PLATFORM_RENDERER_draw_bound_coloured_perspective_textured_quad_immediate(
				quad->quad.x0, quad->quad.y0, quad->quad.x1, quad->quad.y1, quad->quad.x2, quad->quad.y2,
				quad->quad.x3, quad->quad.y3, quad->quad.u1, quad->quad.v1, quad->quad.u2, quad->quad.v2, quad->quad.q,
				quad->r, quad->g, quad->b, quad->a);
		}
	}
}

static void PLATFORM_RENDERER_draw_bound_coloured_perspective_textured_quad_immediate(float x0, float y0, float x1, float y1, float x2, float y2, float x3, float y3, float u1, float v1, float u2, float v2, float q, const float *r, const float *g, const float *b, const float *a)
{
	if ((r == NULL) || (g == NULL) || (b == NULL) || (a == NULL))
	{
		return;
	}
#if defined(WIZBALL_RENDER_BACKEND_GLES2)
	if (PLATFORM_RENDERER_gles2_is_ready())
	{
		platform_renderer_coloured_perspective_quad quad;
		quad.quad.x0 = x0; quad.quad.y0 = y0;
		quad.quad.x1 = x1; quad.quad.y1 = y1;
		quad.quad.x2 = x2; quad.quad.y2 = y2;
		quad.quad.x3 = x3; quad.quad.y3 = y3;
		quad.quad.u1 = u1; quad.quad.v1 = v1;
		quad.quad.u2 = u2; quad.quad.v2 = v2;
		quad.quad.q = q;
		memcpy(quad.r, r, sizeof(quad.r));
		memcpy(quad.g, g, sizeof(quad.g));
		memcpy(quad.b, b, sizeof(quad.b));
		memcpy(quad.a, a, sizeof(quad.a));
		(void)PLATFORM_RENDERER_draw_bound_coloured_perspective_textured_quad_batch_immediate(&quad, 1);
		return;
	}
#endif
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

static bool PLATFORM_RENDERER_draw_bound_perspective_textured_quad_batch_immediate(const platform_renderer_perspective_quad *quads, int quad_count)
{
	platform_renderer_texture_entry *entry;
	SDL_Vertex *vertices;
	int *indices;
	SDL_Vertex stack_vertices[768];
	int stack_indices[1152];
	int total_vertices = 0;
	int total_indices = 0;
	int base_vertex = 0;
	int base_index = 0;
	int quad_index;
	const float perspective_eps = 0.0005f;
	const Uint8 cr = PLATFORM_RENDERER_clamp_sdl_unit_to_byte(platform_renderer_current_colour_r);
	const Uint8 cg = PLATFORM_RENDERER_clamp_sdl_unit_to_byte(platform_renderer_current_colour_g);
	const Uint8 cb = PLATFORM_RENDERER_clamp_sdl_unit_to_byte(platform_renderer_current_colour_b);
	const Uint8 ca = PLATFORM_RENDERER_clamp_sdl_unit_to_byte(platform_renderer_current_colour_a);
	const bool gles_ready = PLATFORM_RENDERER_gles2_is_ready();

	if ((platform_renderer_present_height <= 0) || (quads == NULL) || (quad_count <= 0))
	{
		return false;
	}
	if (!gles_ready && !PLATFORM_RENDERER_is_sdl2_stub_ready())
	{
		return false;
	}

	entry = PLATFORM_RENDERER_get_texture_entry(platform_renderer_last_bound_texture_handle);
	if (entry == NULL)
	{
		return false;
	}
	if (gles_ready)
	{
		if (!PLATFORM_RENDERER_build_gles2_texture_from_entry(entry))
		{
			return false;
		}
	}
	else if (!PLATFORM_RENDERER_build_sdl_texture_from_entry(entry))
	{
		return false;
	}

	for (quad_index = 0; quad_index < quad_count; quad_index++)
	{
		const int strip_count = PLATFORM_RENDERER_get_perspective_strip_count(quads[quad_index].q, gles_ready);
		total_vertices += strip_count * 4;
		total_indices += strip_count * 6;
	}

	if (total_vertices <= (int)(sizeof(stack_vertices) / sizeof(stack_vertices[0])))
	{
		vertices = stack_vertices;
	}
	else
	{
		vertices = (SDL_Vertex *)malloc((size_t)total_vertices * sizeof(SDL_Vertex));
	}
	if (total_indices <= (int)(sizeof(stack_indices) / sizeof(stack_indices[0])))
	{
		indices = stack_indices;
	}
	else
	{
		indices = (int *)malloc((size_t)total_indices * sizeof(int));
	}
	if ((vertices == NULL) || (indices == NULL))
	{
		if (vertices != stack_vertices)
		{
			free(vertices);
		}
		if (indices != stack_indices)
		{
			free(indices);
		}
		return false;
	}

	for (quad_index = 0; quad_index < quad_count; quad_index++)
	{
		const platform_renderer_perspective_quad *quad = &quads[quad_index];
		const int strip_count = PLATFORM_RENDERER_get_perspective_strip_count(quad->q, gles_ready);
		const bool is_perspective = (strip_count > 1);
		int strip_index;

		for (strip_index = 0; strip_index < strip_count; strip_index++)
		{
			const float t0 = is_perspective ? ((float)strip_index / (float)strip_count) : 0.0f;
			const float t1 = is_perspective ? ((float)(strip_index + 1) / (float)strip_count) : 1.0f;
			const float one_minus_t0 = 1.0f - t0;
			const float one_minus_t1 = 1.0f - t1;
			const float left_x0 = (quad->x0 * one_minus_t0) + (quad->x1 * t0);
			const float left_y0 = (quad->y0 * one_minus_t0) + (quad->y1 * t0);
			const float left_x1 = (quad->x0 * one_minus_t1) + (quad->x1 * t1);
			const float left_y1 = (quad->y0 * one_minus_t1) + (quad->y1 * t1);
			const float right_x0 = (quad->x3 * one_minus_t0) + (quad->x2 * t0);
			const float right_y0 = (quad->y3 * one_minus_t0) + (quad->y2 * t0);
			const float right_x1 = (quad->x3 * one_minus_t1) + (quad->x2 * t1);
			const float right_y1 = (quad->y3 * one_minus_t1) + (quad->y2 * t1);
			const float w0 = is_perspective ? ((quad->q * one_minus_t0) + t0) : 1.0f;
			const float w1 = is_perspective ? ((quad->q * one_minus_t1) + t1) : 1.0f;
			const float strip_v0 = is_perspective ? (((quad->v1 * quad->q * one_minus_t0) + (quad->v2 * t0)) / w0) : quad->v1;
			const float strip_v1 = is_perspective ? (((quad->v1 * quad->q * one_minus_t1) + (quad->v2 * t1)) / w1) : quad->v2;
			float tx;
			float ty;

			if ((fabsf(w0) <= perspective_eps) || (fabsf(w1) <= perspective_eps))
			{
				if (vertices != stack_vertices)
				{
					free(vertices);
				}
				if (indices != stack_indices)
				{
					free(indices);
				}
				return false;
			}

			if (gles_ready)
			{
				vertices[base_vertex + 0].position.x = left_x0;
				vertices[base_vertex + 0].position.y = left_y0;
			}
			else
			{
				PLATFORM_RENDERER_transform_point(left_x0, left_y0, &tx, &ty);
				vertices[base_vertex + 0].position.x = tx;
				vertices[base_vertex + 0].position.y = (float)platform_renderer_present_height - ty;
			}
			vertices[base_vertex + 0].tex_coord.x = quad->u1;
			vertices[base_vertex + 0].tex_coord.y = strip_v0;
			vertices[base_vertex + 0].color.r = cr;
			vertices[base_vertex + 0].color.g = cg;
			vertices[base_vertex + 0].color.b = cb;
			vertices[base_vertex + 0].color.a = ca;

			if (gles_ready)
			{
				vertices[base_vertex + 1].position.x = left_x1;
				vertices[base_vertex + 1].position.y = left_y1;
			}
			else
			{
				PLATFORM_RENDERER_transform_point(left_x1, left_y1, &tx, &ty);
				vertices[base_vertex + 1].position.x = tx;
				vertices[base_vertex + 1].position.y = (float)platform_renderer_present_height - ty;
			}
			vertices[base_vertex + 1].tex_coord.x = quad->u1;
			vertices[base_vertex + 1].tex_coord.y = strip_v1;
			vertices[base_vertex + 1].color.r = cr;
			vertices[base_vertex + 1].color.g = cg;
			vertices[base_vertex + 1].color.b = cb;
			vertices[base_vertex + 1].color.a = ca;

			if (gles_ready)
			{
				vertices[base_vertex + 2].position.x = right_x1;
				vertices[base_vertex + 2].position.y = right_y1;
			}
			else
			{
				PLATFORM_RENDERER_transform_point(right_x1, right_y1, &tx, &ty);
				vertices[base_vertex + 2].position.x = tx;
				vertices[base_vertex + 2].position.y = (float)platform_renderer_present_height - ty;
			}
			vertices[base_vertex + 2].tex_coord.x = quad->u2;
			vertices[base_vertex + 2].tex_coord.y = strip_v1;
			vertices[base_vertex + 2].color.r = cr;
			vertices[base_vertex + 2].color.g = cg;
			vertices[base_vertex + 2].color.b = cb;
			vertices[base_vertex + 2].color.a = ca;

			if (gles_ready)
			{
				vertices[base_vertex + 3].position.x = right_x0;
				vertices[base_vertex + 3].position.y = right_y0;
			}
			else
			{
				PLATFORM_RENDERER_transform_point(right_x0, right_y0, &tx, &ty);
				vertices[base_vertex + 3].position.x = tx;
				vertices[base_vertex + 3].position.y = (float)platform_renderer_present_height - ty;
			}
			vertices[base_vertex + 3].tex_coord.x = quad->u2;
			vertices[base_vertex + 3].tex_coord.y = strip_v0;
			vertices[base_vertex + 3].color.r = cr;
			vertices[base_vertex + 3].color.g = cg;
			vertices[base_vertex + 3].color.b = cb;
			vertices[base_vertex + 3].color.a = ca;

			indices[base_index + 0] = base_vertex + 0;
			indices[base_index + 1] = base_vertex + 1;
			indices[base_index + 2] = base_vertex + 2;
			indices[base_index + 3] = base_vertex + 0;
			indices[base_index + 4] = base_vertex + 2;
			indices[base_index + 5] = base_vertex + 3;

			base_vertex += 4;
			base_index += 6;
		}
	}

	if (PLATFORM_RENDERER_gles2_is_ready())
	{
		PLATFORM_RENDERER_gles2_flush_textured_batch();
		PLATFORM_RENDERER_gles2_set_affine_override_current_transform();
		quad_index = PLATFORM_RENDERER_gles2_draw_textured_geometry(
			platform_renderer_last_bound_texture_handle, vertices, total_vertices, indices, total_indices, PLATFORM_RENDERER_GEOM_SRC_PERSPECTIVE) ? 1 : 0;
		PLATFORM_RENDERER_gles2_set_affine_override_identity();
	}
	else
	{
		quad_index = PLATFORM_RENDERER_try_sdl_geometry_textured(
			entry->sdl_texture, vertices, total_vertices, indices, total_indices, PLATFORM_RENDERER_GEOM_SRC_PERSPECTIVE) ? 1 : 0;
	}
	if (vertices != stack_vertices)
	{
		free(vertices);
	}
	if (indices != stack_indices)
	{
		free(indices);
	}
	return quad_index != 0;
}

static bool PLATFORM_RENDERER_draw_bound_coloured_perspective_textured_quad_batch_immediate(const platform_renderer_coloured_perspective_quad *quads, int quad_count)
{
	platform_renderer_texture_entry *entry;
	SDL_Vertex *vertices;
	int *indices;
	SDL_Vertex stack_vertices[768];
	int stack_indices[1152];
	int total_vertices = 0;
	int total_indices = 0;
	int base_vertex = 0;
	int base_index = 0;
	int quad_index;
	const float perspective_eps = 0.0005f;
	const bool gles_ready = PLATFORM_RENDERER_gles2_is_ready();

	if ((platform_renderer_present_height <= 0) || (quads == NULL) || (quad_count <= 0))
	{
		return false;
	}
	if (!gles_ready && !PLATFORM_RENDERER_is_sdl2_stub_ready())
	{
		return false;
	}

	entry = PLATFORM_RENDERER_get_texture_entry(platform_renderer_last_bound_texture_handle);
	if (entry == NULL)
	{
		return false;
	}
	if (gles_ready)
	{
		if (!PLATFORM_RENDERER_build_gles2_texture_from_entry(entry))
		{
			return false;
		}
	}
	else if (!PLATFORM_RENDERER_build_sdl_texture_from_entry(entry))
	{
		return false;
	}

	for (quad_index = 0; quad_index < quad_count; quad_index++)
	{
		const int strip_count = PLATFORM_RENDERER_get_perspective_strip_count(quads[quad_index].quad.q, gles_ready);
		total_vertices += strip_count * 4;
		total_indices += strip_count * 6;
	}

	if (total_vertices <= (int)(sizeof(stack_vertices) / sizeof(stack_vertices[0])))
	{
		vertices = stack_vertices;
	}
	else
	{
		vertices = (SDL_Vertex *)malloc((size_t)total_vertices * sizeof(SDL_Vertex));
	}
	if (total_indices <= (int)(sizeof(stack_indices) / sizeof(stack_indices[0])))
	{
		indices = stack_indices;
	}
	else
	{
		indices = (int *)malloc((size_t)total_indices * sizeof(int));
	}
	if ((vertices == NULL) || (indices == NULL))
	{
		if (vertices != stack_vertices)
		{
			free(vertices);
		}
		if (indices != stack_indices)
		{
			free(indices);
		}
		return false;
	}

	for (quad_index = 0; quad_index < quad_count; quad_index++)
	{
		const platform_renderer_coloured_perspective_quad *quad = &quads[quad_index];
		const int strip_count = PLATFORM_RENDERER_get_perspective_strip_count(quad->quad.q, gles_ready);
		const bool is_perspective = (strip_count > 1);
		int strip_index;

		for (strip_index = 0; strip_index < strip_count; strip_index++)
		{
			const float t0 = is_perspective ? ((float)strip_index / (float)strip_count) : 0.0f;
			const float t1 = is_perspective ? ((float)(strip_index + 1) / (float)strip_count) : 1.0f;
			const float one_minus_t0 = 1.0f - t0;
			const float one_minus_t1 = 1.0f - t1;
			const float left_x0 = (quad->quad.x0 * one_minus_t0) + (quad->quad.x1 * t0);
			const float left_y0 = (quad->quad.y0 * one_minus_t0) + (quad->quad.y1 * t0);
			const float left_x1 = (quad->quad.x0 * one_minus_t1) + (quad->quad.x1 * t1);
			const float left_y1 = (quad->quad.y0 * one_minus_t1) + (quad->quad.y1 * t1);
			const float right_x0 = (quad->quad.x3 * one_minus_t0) + (quad->quad.x2 * t0);
			const float right_y0 = (quad->quad.y3 * one_minus_t0) + (quad->quad.y2 * t0);
			const float right_x1 = (quad->quad.x3 * one_minus_t1) + (quad->quad.x2 * t1);
			const float right_y1 = (quad->quad.y3 * one_minus_t1) + (quad->quad.y2 * t1);
			const float w0 = is_perspective ? ((quad->quad.q * one_minus_t0) + t0) : 1.0f;
			const float w1 = is_perspective ? ((quad->quad.q * one_minus_t1) + t1) : 1.0f;
			const float strip_v0 = is_perspective ? (((quad->quad.v1 * quad->quad.q * one_minus_t0) + (quad->quad.v2 * t0)) / w0) : quad->quad.v1;
			const float strip_v1 = is_perspective ? (((quad->quad.v1 * quad->quad.q * one_minus_t1) + (quad->quad.v2 * t1)) / w1) : quad->quad.v2;
			float tx;
			float ty;
			int cr;
			int cg;
			int cb;
			int ca;

			if ((fabsf(w0) <= perspective_eps) || (fabsf(w1) <= perspective_eps))
			{
				if (vertices != stack_vertices)
				{
					free(vertices);
				}
				if (indices != stack_indices)
				{
					free(indices);
				}
				return false;
			}

			if (gles_ready)
			{
				vertices[base_vertex + 0].position.x = left_x0;
				vertices[base_vertex + 0].position.y = left_y0;
			}
			else
			{
				PLATFORM_RENDERER_transform_point(left_x0, left_y0, &tx, &ty);
				vertices[base_vertex + 0].position.x = tx;
				vertices[base_vertex + 0].position.y = (float)platform_renderer_present_height - ty;
			}
			vertices[base_vertex + 0].tex_coord.x = quad->quad.u1;
			vertices[base_vertex + 0].tex_coord.y = strip_v0;
			cr = (int)(((quad->r[0] * one_minus_t0) + (quad->r[1] * t0)) * 255.0f);
			cg = (int)(((quad->g[0] * one_minus_t0) + (quad->g[1] * t0)) * 255.0f);
			cb = (int)(((quad->b[0] * one_minus_t0) + (quad->b[1] * t0)) * 255.0f);
			ca = (int)(((quad->a[0] * one_minus_t0) + (quad->a[1] * t0)) * 255.0f);
			if (cr < 0) cr = 0; else if (cr > 255) cr = 255;
			if (cg < 0) cg = 0; else if (cg > 255) cg = 255;
			if (cb < 0) cb = 0; else if (cb > 255) cb = 255;
			if (ca < 0) ca = 0; else if (ca > 255) ca = 255;
			vertices[base_vertex + 0].color.r = (Uint8)cr;
			vertices[base_vertex + 0].color.g = (Uint8)cg;
			vertices[base_vertex + 0].color.b = (Uint8)cb;
			vertices[base_vertex + 0].color.a = (Uint8)ca;

			if (gles_ready)
			{
				vertices[base_vertex + 1].position.x = left_x1;
				vertices[base_vertex + 1].position.y = left_y1;
			}
			else
			{
				PLATFORM_RENDERER_transform_point(left_x1, left_y1, &tx, &ty);
				vertices[base_vertex + 1].position.x = tx;
				vertices[base_vertex + 1].position.y = (float)platform_renderer_present_height - ty;
			}
			vertices[base_vertex + 1].tex_coord.x = quad->quad.u1;
			vertices[base_vertex + 1].tex_coord.y = strip_v1;
			cr = (int)(((quad->r[0] * one_minus_t1) + (quad->r[1] * t1)) * 255.0f);
			cg = (int)(((quad->g[0] * one_minus_t1) + (quad->g[1] * t1)) * 255.0f);
			cb = (int)(((quad->b[0] * one_minus_t1) + (quad->b[1] * t1)) * 255.0f);
			ca = (int)(((quad->a[0] * one_minus_t1) + (quad->a[1] * t1)) * 255.0f);
			if (cr < 0) cr = 0; else if (cr > 255) cr = 255;
			if (cg < 0) cg = 0; else if (cg > 255) cg = 255;
			if (cb < 0) cb = 0; else if (cb > 255) cb = 255;
			if (ca < 0) ca = 0; else if (ca > 255) ca = 255;
			vertices[base_vertex + 1].color.r = (Uint8)cr;
			vertices[base_vertex + 1].color.g = (Uint8)cg;
			vertices[base_vertex + 1].color.b = (Uint8)cb;
			vertices[base_vertex + 1].color.a = (Uint8)ca;

			if (gles_ready)
			{
				vertices[base_vertex + 2].position.x = right_x1;
				vertices[base_vertex + 2].position.y = right_y1;
			}
			else
			{
				PLATFORM_RENDERER_transform_point(right_x1, right_y1, &tx, &ty);
				vertices[base_vertex + 2].position.x = tx;
				vertices[base_vertex + 2].position.y = (float)platform_renderer_present_height - ty;
			}
			vertices[base_vertex + 2].tex_coord.x = quad->quad.u2;
			vertices[base_vertex + 2].tex_coord.y = strip_v1;
			cr = (int)(((quad->r[3] * one_minus_t1) + (quad->r[2] * t1)) * 255.0f);
			cg = (int)(((quad->g[3] * one_minus_t1) + (quad->g[2] * t1)) * 255.0f);
			cb = (int)(((quad->b[3] * one_minus_t1) + (quad->b[2] * t1)) * 255.0f);
			ca = (int)(((quad->a[3] * one_minus_t1) + (quad->a[2] * t1)) * 255.0f);
			if (cr < 0) cr = 0; else if (cr > 255) cr = 255;
			if (cg < 0) cg = 0; else if (cg > 255) cg = 255;
			if (cb < 0) cb = 0; else if (cb > 255) cb = 255;
			if (ca < 0) ca = 0; else if (ca > 255) ca = 255;
			vertices[base_vertex + 2].color.r = (Uint8)cr;
			vertices[base_vertex + 2].color.g = (Uint8)cg;
			vertices[base_vertex + 2].color.b = (Uint8)cb;
			vertices[base_vertex + 2].color.a = (Uint8)ca;

			if (gles_ready)
			{
				vertices[base_vertex + 3].position.x = right_x0;
				vertices[base_vertex + 3].position.y = right_y0;
			}
			else
			{
				PLATFORM_RENDERER_transform_point(right_x0, right_y0, &tx, &ty);
				vertices[base_vertex + 3].position.x = tx;
				vertices[base_vertex + 3].position.y = (float)platform_renderer_present_height - ty;
			}
			vertices[base_vertex + 3].tex_coord.x = quad->quad.u2;
			vertices[base_vertex + 3].tex_coord.y = strip_v0;
			cr = (int)(((quad->r[3] * one_minus_t0) + (quad->r[2] * t0)) * 255.0f);
			cg = (int)(((quad->g[3] * one_minus_t0) + (quad->g[2] * t0)) * 255.0f);
			cb = (int)(((quad->b[3] * one_minus_t0) + (quad->b[2] * t0)) * 255.0f);
			ca = (int)(((quad->a[3] * one_minus_t0) + (quad->a[2] * t0)) * 255.0f);
			if (cr < 0) cr = 0; else if (cr > 255) cr = 255;
			if (cg < 0) cg = 0; else if (cg > 255) cg = 255;
			if (cb < 0) cb = 0; else if (cb > 255) cb = 255;
			if (ca < 0) ca = 0; else if (ca > 255) ca = 255;
			vertices[base_vertex + 3].color.r = (Uint8)cr;
			vertices[base_vertex + 3].color.g = (Uint8)cg;
			vertices[base_vertex + 3].color.b = (Uint8)cb;
			vertices[base_vertex + 3].color.a = (Uint8)ca;

			indices[base_index + 0] = base_vertex + 0;
			indices[base_index + 1] = base_vertex + 1;
			indices[base_index + 2] = base_vertex + 2;
			indices[base_index + 3] = base_vertex + 0;
			indices[base_index + 4] = base_vertex + 2;
			indices[base_index + 5] = base_vertex + 3;

			base_vertex += 4;
			base_index += 6;
		}
	}

	if (PLATFORM_RENDERER_gles2_is_ready())
	{
		PLATFORM_RENDERER_gles2_flush_textured_batch();
		PLATFORM_RENDERER_gles2_set_affine_override_current_transform();
		quad_index = PLATFORM_RENDERER_gles2_draw_textured_geometry(
			platform_renderer_last_bound_texture_handle, vertices, total_vertices, indices, total_indices, PLATFORM_RENDERER_GEOM_SRC_COLOURED_PERSPECTIVE) ? 1 : 0;
		PLATFORM_RENDERER_gles2_set_affine_override_identity();
	}
	else
	{
		quad_index = PLATFORM_RENDERER_try_sdl_geometry_textured(
			entry->sdl_texture, vertices, total_vertices, indices, total_indices, PLATFORM_RENDERER_GEOM_SRC_COLOURED_PERSPECTIVE) ? 1 : 0;
	}
	if (vertices != stack_vertices)
	{
		free(vertices);
	}
	if (indices != stack_indices)
	{
		free(indices);
	}
	return quad_index != 0;
}

void PLATFORM_RENDERER_draw_textured_quad(unsigned int texture_handle, int r, int g, int b, float screen_x, float screen_y, int virtual_screen_height, float left, float right, float up, float down, float u1, float v1, float u2, float v2, bool alpha_test)
{
#if defined(WIZBALL_RENDER_BACKEND_GLES2)
	if (PLATFORM_RENDERER_gles2_is_ready())
	{
		platform_renderer_texture_entry *entry = PLATFORM_RENDERER_get_texture_entry(texture_handle);
		SDL_Vertex vertices[4];
		const int indices[6] = {0, 1, 2, 0, 2, 3};
		SDL_Rect src_rect;
		const bool saved_masked = platform_renderer_texture_masked;
		const float x0 = screen_x + left;
		const float x1 = screen_x + right;
		const float y0 = screen_y + up;
		const float y1 = screen_y + down;
		const float top = (float)virtual_screen_height - y0;
		const float bottom = (float)virtual_screen_height - y1;
		const Uint8 cr = PLATFORM_RENDERER_clamp_sdl_colour_mod(r);
		const Uint8 cg = PLATFORM_RENDERER_clamp_sdl_colour_mod(g);
		const Uint8 cb = PLATFORM_RENDERER_clamp_sdl_colour_mod(b);
		const float tex_w = (entry != NULL) ? (float)entry->width : 0.0f;
		const float tex_h = (entry != NULL) ? (float)entry->height : 0.0f;
		float src_u_left;
		float src_u_right;
		float src_v_top;
		float src_v_bottom;

		if ((entry == NULL) || !PLATFORM_RENDERER_build_gles2_texture_from_entry(entry))
		{
			return;
		}
		if (!PLATFORM_RENDERER_build_safe_sdl_src_rect(entry, u1, v1, u2, v2, &src_rect))
		{
			return;
		}

		src_u_left = ((float)src_rect.x + 0.5f) / tex_w;
		src_u_right = ((float)(src_rect.x + src_rect.w) - 0.5f) / tex_w;
		src_v_top = 1.0f - (((float)src_rect.y + 0.5f) / tex_h);
		src_v_bottom = 1.0f - (((float)(src_rect.y + src_rect.h) - 0.5f) / tex_h);

		vertices[0].position.x = x0;
		vertices[0].position.y = top;
		vertices[1].position.x = x0;
		vertices[1].position.y = bottom;
		vertices[2].position.x = x1;
		vertices[2].position.y = bottom;
		vertices[3].position.x = x1;
		vertices[3].position.y = top;

		vertices[0].tex_coord.x = src_u_left;
		vertices[0].tex_coord.y = src_v_top;
		vertices[1].tex_coord.x = src_u_left;
		vertices[1].tex_coord.y = src_v_bottom;
		vertices[2].tex_coord.x = src_u_right;
		vertices[2].tex_coord.y = src_v_bottom;
		vertices[3].tex_coord.x = src_u_right;
		vertices[3].tex_coord.y = src_v_top;

		for (int i = 0; i < 4; i++)
		{
			vertices[i].color.r = cr;
			vertices[i].color.g = cg;
			vertices[i].color.b = cb;
			vertices[i].color.a = 255;
		}

		platform_renderer_texture_masked = alpha_test;
		(void)PLATFORM_RENDERER_gles2_draw_textured_subrect_geometry(
				entry,
				texture_handle,
				&src_rect,
				vertices,
				4,
				indices,
				6,
				PLATFORM_RENDERER_GEOM_SRC_SDL_CUSTOM);
		platform_renderer_texture_masked = saved_masked;
		return;
	}
#endif
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
					}
				}
			}
		}
	}
	(void)virtual_screen_height;
}

void PLATFORM_RENDERER_draw_textured_quad_batch(unsigned int texture_handle, const SDL_Vertex *vertices, int quad_count, bool alpha_test)
{
	if ((vertices == NULL) || (quad_count <= 0))
	{
		return;
	}

#if defined(WIZBALL_RENDER_BACKEND_GLES2)
	if (PLATFORM_RENDERER_gles2_is_ready())
	{
		platform_renderer_texture_entry *entry = PLATFORM_RENDERER_get_texture_entry(texture_handle);
		const bool saved_masked = platform_renderer_texture_masked;
		if ((entry != NULL) && PLATFORM_RENDERER_build_gles2_texture_from_entry(entry))
		{
			platform_renderer_texture_masked = alpha_test;
			(void)PLATFORM_RENDERER_gles2_draw_textured_quad_batch_gltex(
					entry->gles2_texture,
					texture_handle,
					vertices,
					quad_count,
					PLATFORM_RENDERER_GEOM_SRC_SDL_CUSTOM,
					false,
					false);
			platform_renderer_texture_masked = saved_masked;
			return;
		}
		return;
	}
#endif
	{
		const int vertex_count = quad_count * 4;
		const int index_count = quad_count * 6;
		int stack_indices[PLATFORM_RENDERER_MAX_GEOMETRY_INDICES];
		int *indices = stack_indices;
		bool used_heap_indices = false;
		int quad_index;

		if (index_count > PLATFORM_RENDERER_MAX_GEOMETRY_INDICES)
		{
			indices = (int *)malloc((size_t)index_count * sizeof(int));
			if (indices == NULL)
			{
				return;
			}
			used_heap_indices = true;
		}

		for (quad_index = 0; quad_index < quad_count; quad_index++)
		{
			const int base_vertex = quad_index * 4;
			const int base_index = quad_index * 6;
			indices[base_index + 0] = base_vertex + 0;
			indices[base_index + 1] = base_vertex + 1;
			indices[base_index + 2] = base_vertex + 2;
			indices[base_index + 3] = base_vertex + 0;
			indices[base_index + 4] = base_vertex + 2;
			indices[base_index + 5] = base_vertex + 3;
		}
	if (PLATFORM_RENDERER_is_sdl2_stub_ready())
	{
#if SDL_VERSION_ATLEAST(2, 0, 18)
		platform_renderer_texture_entry *entry = PLATFORM_RENDERER_get_texture_entry(texture_handle);
		if ((entry != NULL) && PLATFORM_RENDERER_build_sdl_texture_from_entry(entry))
		{
			SDL_Texture *draw_texture = PLATFORM_RENDERER_get_effective_texture_for_current_blend(entry);
			if (draw_texture != NULL)
			{
				PLATFORM_RENDERER_apply_sdl_texture_blend_mode(draw_texture);
				(void)PLATFORM_RENDERER_try_sdl_geometry_textured(
						draw_texture,
						vertices,
						vertex_count,
						indices,
						index_count,
						PLATFORM_RENDERER_GEOM_SRC_SDL_CUSTOM);
			}
		}
#endif
	}

		if (used_heap_indices)
		{
			free(indices);
		}
	}
}

void PLATFORM_RENDERER_draw_sdl_window_sprite(unsigned int texture_handle, int r, int g, int b, int a, float entity_x, float entity_y, float left, float right, float up, float down, float u1, float v1, float u2, float v2, float left_window_transform_x, float top_window_transform_y, float total_scale_x, float total_scale_y, float sprite_scale_x, float sprite_scale_y, float sprite_rotation_degrees, bool sprite_flip_x, bool sprite_flip_y)
{
	if (platform_renderer_command_queue_count > 0)
	{
		PLATFORM_RENDERER_flush_command_queue();
	}

	if (!(PLATFORM_RENDERER_is_sdl2_stub_ready()))
	{
		return;
	}

	platform_renderer_texture_entry *entry = PLATFORM_RENDERER_get_texture_entry(texture_handle);
	if (entry == NULL)
	{
		return;
	}
#if defined(WIZBALL_RENDER_BACKEND_GLES2)
	if (PLATFORM_RENDERER_gles2_is_ready())
	{
		SDL_Vertex vertices[4];
		const int indices[6] = {0, 1, 2, 0, 2, 3};
		SDL_Rect src_rect;
		const float transformed_left = left * sprite_scale_x;
		const float transformed_right = right * sprite_scale_x;
		const float transformed_up = up * sprite_scale_y;
		const float transformed_down = down * sprite_scale_y;
		const float origin_x = left_window_transform_x + (entity_x * total_scale_x);
		const float origin_y = top_window_transform_y + ((-entity_y) * total_scale_y);
		const bool src_flip_x = (u2 < u1);
		const bool src_flip_y = PLATFORM_RENDERER_src_flip_y_for_sdl(v1, v2);
		const bool effective_flip_x = src_flip_x ^ sprite_flip_x;
		const bool effective_flip_y = src_flip_y ^ sprite_flip_y;
		const float rel_left = transformed_left * total_scale_x;
		const float rel_right = transformed_right * total_scale_x;
		const float rel_up = transformed_up * total_scale_y;
		const float rel_down = transformed_down * total_scale_y;
		const float radians = sprite_rotation_degrees * (3.14159265358979323846f / 180.0f);
		const float c = cosf(radians);
		const float s = sinf(radians);
		const float local_x[4] = {rel_left, rel_left, rel_right, rel_right};
		const float local_y[4] = {rel_up, rel_down, rel_down, rel_up};
		const Uint8 cr = PLATFORM_RENDERER_clamp_sdl_colour_mod(r);
		const Uint8 cg = PLATFORM_RENDERER_clamp_sdl_colour_mod(g);
		const Uint8 cb = PLATFORM_RENDERER_clamp_sdl_colour_mod(b);
		const Uint8 ca = PLATFORM_RENDERER_clamp_sdl_colour_mod(a);
		const float tex_w = (float)entry->width;
		const float tex_h = (float)entry->height;
		float base_u_left;
		float base_u_right;
		float base_v_top;
		float base_v_bottom;
		float u_left;
		float u_right;
		float v_top;
		float v_bottom;
		float uv_x[4];
		float uv_y[4];
		int i;

		if (!PLATFORM_RENDERER_build_gles2_texture_from_entry(entry))
		{
			return;
		}
		if (platform_renderer_present_height <= 0)
		{
			return;
		}
		if (!PLATFORM_RENDERER_build_safe_sdl_src_rect(entry, u1, v1, u2, v2, &src_rect))
		{
			return;
		}
		/*
		 * Match SDL's source-rect semantics exactly on the direct GLES2 path:
		 * derive UVs from the normalized/clamped source rectangle rather than
		 * trusting raw UVs that may straddle atlas padding or differ by a
		 * half-texel from SDL_RenderCopyEx sampling.
		 */
		base_u_left = ((float)src_rect.x + 0.5f) / tex_w;
		base_u_right = ((float)(src_rect.x + src_rect.w) - 0.5f) / tex_w;
		base_v_top = 1.0f - (((float)src_rect.y + 0.5f) / tex_h);
		base_v_bottom = 1.0f - (((float)(src_rect.y + src_rect.h) - 0.5f) / tex_h);

		u_left = effective_flip_x ? base_u_right : base_u_left;
		u_right = effective_flip_x ? base_u_left : base_u_right;
		v_top = effective_flip_y ? base_v_bottom : base_v_top;
		v_bottom = effective_flip_y ? base_v_top : base_v_bottom;
		uv_x[0] = u_left;
		uv_x[1] = u_left;
		uv_x[2] = u_right;
		uv_x[3] = u_right;
		uv_y[0] = v_top;
		uv_y[1] = v_bottom;
		uv_y[2] = v_bottom;
		uv_y[3] = v_top;

		for (i = 0; i < 4; i++)
		{
			const float x = local_x[i];
			const float y = local_y[i];
			const float rx = (x * c) - (y * s);
			const float ry = (x * s) + (y * c);
			vertices[i].position.x = origin_x + rx;
			vertices[i].position.y = (float)platform_renderer_present_height - (origin_y + ry);
			vertices[i].tex_coord.x = uv_x[i];
			vertices[i].tex_coord.y = uv_y[i];
			vertices[i].color.r = cr;
			vertices[i].color.g = cg;
			vertices[i].color.b = cb;
			vertices[i].color.a = ca;
		}
		(void)PLATFORM_RENDERER_gles2_draw_textured_subrect_geometry(
				entry,
				texture_handle,
				&src_rect,
				vertices,
				4,
				indices,
				6,
				PLATFORM_RENDERER_GEOM_SRC_SDL_CUSTOM);
		return;
	}
#endif
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
	if (platform_renderer_command_queue_count > 0)
	{
		PLATFORM_RENDERER_flush_command_queue();
	}

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
	if (platform_renderer_command_queue_count > 0)
	{
		PLATFORM_RENDERER_flush_command_queue();
	}

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
	if (platform_renderer_command_queue_count > 0)
	{
		PLATFORM_RENDERER_flush_command_queue();
	}
#if defined(WIZBALL_RENDER_BACKEND_GLES2)
	platform_renderer_gles2_flush_present_count++;
	PLATFORM_RENDERER_gles2_flush_textured_batch();
	if (PLATFORM_RENDERER_gles2_is_ready())
	{
		static unsigned int gles2_present_debug_counter = 0;
		Uint32 frame_start_ticks = SDL_GetTicks();
		Uint32 render_ms = (s_frame_clear_ticks > 0 && frame_start_ticks >= s_frame_clear_ticks)
			? (frame_start_ticks - s_frame_clear_ticks) : 0;
		SDL_GL_SwapWindow(platform_renderer_sdl_window);
		gles2_present_debug_counter++;
		if ((gles2_present_debug_counter <= 300u) || ((gles2_present_debug_counter % 300u) == 0u))
		{
			fprintf(stderr,
				"[FRAME %u][GLES2] draws=%d textured=%d nontex=%d tx_sw=%d blend_sw=%d render_ms=%u\n",
				gles2_present_debug_counter,
				platform_renderer_sdl_native_draw_count,
				platform_renderer_sdl_native_textured_draw_count,
				platform_renderer_sdl_native_draw_count - platform_renderer_sdl_native_textured_draw_count,
				platform_renderer_sdl_tx_switch_count,
				platform_renderer_sdl_blend_switch_count,
				(unsigned int)render_ms);
			fprintf(stderr,
				"[GLES2-FLUSH] key=%d full=%d state=%d finish=%d clear=%d present=%d\n",
				platform_renderer_gles2_flush_key_mismatch_count,
				platform_renderer_gles2_flush_batch_full_count,
				platform_renderer_gles2_flush_state_change_count,
				platform_renderer_gles2_flush_window_finish_count,
				platform_renderer_gles2_flush_clear_count,
				platform_renderer_gles2_flush_present_count);
		}
		platform_renderer_clear_backbuffer_calls_since_present = 0;
		platform_renderer_midframe_reset_events = 0;
		platform_renderer_gles2_flush_key_mismatch_count = 0;
		platform_renderer_gles2_flush_batch_full_count = 0;
		platform_renderer_gles2_flush_state_change_count = 0;
		platform_renderer_gles2_flush_window_finish_count = 0;
		platform_renderer_gles2_flush_clear_count = 0;
		platform_renderer_gles2_flush_present_count = 0;
		return;
	}
#endif
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

	if (platform_renderer_sdl_window != NULL &&
			(platform_renderer_sdl_renderer != NULL
#if defined(WIZBALL_RENDER_BACKEND_GLES2)
			|| platform_renderer_gles2_context != NULL
#endif
			))
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

	if (PLATFORM_RENDERER_backend_targets_gles2())
	{
		/*
		 * Phase 0 GLES2 migration: prefer SDL's GLES2 renderer backend today,
		 * while the direct GLES2 draw path is migrated behind the same selector.
		 */
		(void)SDL_SetHint(SDL_HINT_RENDER_DRIVER, "opengles2");
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
	if (PLATFORM_RENDERER_backend_targets_gles2())
	{
		window_flags |= SDL_WINDOW_OPENGL;
#if defined(WIZBALL_RENDER_BACKEND_GLES2)
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
		SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
#endif
	}
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

#if defined(WIZBALL_RENDER_BACKEND_GLES2)
	if (PLATFORM_RENDERER_backend_targets_gles2())
	{
		if (!PLATFORM_RENDERER_gles2_init_context())
		{
			SDL_DestroyWindow(platform_renderer_sdl_window);
			platform_renderer_sdl_window = NULL;
			if (platform_renderer_started_sdl_video)
			{
				SDL_QuitSubSystem(needed_flags & ~already_flags);
				platform_renderer_started_sdl_video = false;
			}
			return false;
		}
		snprintf(platform_renderer_sdl_status, sizeof(platform_renderer_sdl_status), "GLES2 target initialized (direct SDL GL context).");
		return true;
	}
#endif

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
						"[SDL-INIT] backend_target=%s compiled=%d.%d.%d linked=%d.%d.%d "
						"renderer='%s' flags=0x%x accel=%d software=%d "
						"has_RenderGeometry=%s\n",
						PLATFORM_RENDERER_get_backend_name(),
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
			snprintf(platform_renderer_sdl_status, sizeof(platform_renderer_sdl_status), "%s target initialized (visible window + accelerated SDL renderer).", PLATFORM_RENDERER_get_backend_name());
		}
		else if (platform_renderer_sdl_renderer_software_forced)
		{
			snprintf(platform_renderer_sdl_status, sizeof(platform_renderer_sdl_status), "%s target initialized (visible window + software renderer forced for native-test stability).", PLATFORM_RENDERER_get_backend_name());
		}
		else
		{
			snprintf(platform_renderer_sdl_status, sizeof(platform_renderer_sdl_status), "%s target initialized (visible window + software SDL renderer).", PLATFORM_RENDERER_get_backend_name());
		}
	}
	else
	{
		if (platform_renderer_sdl_stub_accel_enabled && !platform_renderer_sdl_renderer_software_forced)
		{
			snprintf(platform_renderer_sdl_status, sizeof(platform_renderer_sdl_status), "%s target initialized (hidden window + accelerated SDL renderer).", PLATFORM_RENDERER_get_backend_name());
		}
		else if (platform_renderer_sdl_renderer_software_forced)
		{
			snprintf(platform_renderer_sdl_status, sizeof(platform_renderer_sdl_status), "%s target initialized (hidden window + software renderer forced for native-test stability).", PLATFORM_RENDERER_get_backend_name());
		}
		else
		{
			snprintf(platform_renderer_sdl_status, sizeof(platform_renderer_sdl_status), "%s target initialized (hidden window + software SDL renderer).", PLATFORM_RENDERER_get_backend_name());
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
	return
		(platform_renderer_sdl_window != NULL) &&
		(
			(platform_renderer_sdl_renderer != NULL)
#if defined(WIZBALL_RENDER_BACKEND_GLES2)
			|| (platform_renderer_gles2_context != NULL)
#endif
		);
}

bool PLATFORM_RENDERER_is_sdl2_stub_enabled(void)
{
	PLATFORM_RENDERER_refresh_sdl_stub_env_flags();
	return true;
}

bool PLATFORM_RENDERER_run_sdl2_stub_self_test(void)
{
#if defined(WIZBALL_RENDER_BACKEND_GLES2)
	if (PLATFORM_RENDERER_gles2_is_ready())
	{
		strcpy(platform_renderer_sdl_status, "GLES2 self-test passed (context/swap ready).");
		return true;
	}
#endif
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

const char *PLATFORM_RENDERER_get_backend_name(void)
{
	PLATFORM_RENDERER_refresh_backend_name();
	return platform_renderer_backend_name;
}

bool PLATFORM_RENDERER_backend_targets_gles2(void)
{
	PLATFORM_RENDERER_refresh_backend_name();
	return strcmp(platform_renderer_backend_name, "GLES2") == 0;
}

void PLATFORM_RENDERER_shutdown(void)
{
	PLATFORM_RENDERER_reset_texture_registry();
	platform_renderer_sdl_stub_self_test_done = false;

#if defined(WIZBALL_RENDER_BACKEND_GLES2)
	PLATFORM_RENDERER_gles2_shutdown();
#endif
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
