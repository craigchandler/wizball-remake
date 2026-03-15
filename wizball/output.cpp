#include <SDL.h>
#include <SDL_image.h>
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
#include "platform_renderer.h"
#include "file_stuff.h"

#include "fortify.h"

typedef struct
{
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

typedef struct
{
	SDL_Surface *surface;
	SDL_Texture *texture;
	unsigned int texture_handle;
	int width;
	int height;
	int sprite_count;
	sprite *sprite_list;
} sdl_bitmap_holder;

typedef struct
{
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

sdl_bitmap_holder *sdl_bmps = NULL;
// The thing that holds all the bitmaps.
int total_sdl_bitmaps_loaded = 0;
int total_sdl_bitmaps_allocated = 0;

#define USE_SDL
#ifdef USE_SDL
typedef sdl_bitmap_holder active_bmp_t;
#define ACTIVE_BMPS sdl_bmps
#else
// typedef bitmap_holder active_bmp_t;
// #define ACTIVE_BMPS bmps
#endif

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

static sprite *GET_sprite(int bitmap_index, int sprite_index)
{
	return &ACTIVE_BMPS[bitmap_index].sprite_list[sprite_index];
}

static bool OUTPUT_is_valid_bitmap_index(int bitmap_number);
static bool OUTPUT_is_valid_sprite_frame(int bitmap_number, int frame_number);
static bool OUTPUT_window_queue_step(int window_number, int y_list, int *current_entity, int *step_counter);
static void OUTPUT_apply_texture_parameters_from_flags(int opengl_booleans, int old_opengl_booleans, bool force_filter_apply);
static bool OUTPUT_is_secondary_multitexture_active(bool texture_combiner_available, int opengl_booleans, int secondary_bitmap_number);

static void OUTPUT_prepare_sdl_stub_bootstrap(bool windowed)
{
	if (!PLATFORM_RENDERER_is_sdl2_stub_enabled())
	{
		return;
	}
	if (!PLATFORM_RENDERER_prepare_sdl2_stub(game_screen_width, game_screen_height, windowed))
	{
		char sdl_bootstrap_line[MAX_LINE_SIZE];
		snprintf(sdl_bootstrap_line, sizeof(sdl_bootstrap_line), "SDL2 native mode bootstrap failed: %s", PLATFORM_RENDERER_get_sdl2_stub_status());
		MAIN_add_to_log(sdl_bootstrap_line);
	}
}
static unsigned int output_sdl_script_trace_counter = 0;
static int output_cached_main_menu_logo_section_script = UNSET;
static int output_cached_main_menu_tunnel_light_script = UNSET;
static int output_cached_menu_tunnel_starfield_script = UNSET;
static int output_cached_menu_credit_text_script = UNSET;
static int output_cached_menu_scrolly_text_script = UNSET;
static int output_cached_menu_hiscore_text_script = UNSET;
static int output_cached_menu_hiscore_instruction_text_script = UNSET;
static int output_cached_overlay_fading_text_script = UNSET;
static int output_cached_bonus_level_summary_text_script = UNSET;
static int output_cached_menu_option_text_script = UNSET;
static int output_cached_menu_tune_text_script = UNSET;
static int output_cached_menu_entity_identity_flag = UNSET;
static int output_cached_ent_type_permanent_menu_entity = UNSET;
static int output_cached_menu_intro_wiz_or_cat_script = UNSET;
static int output_cached_menu_intro_wiz_or_cat_drag_script = UNSET;
static int output_cached_menu_intro_sensible_logo_script = UNSET;
static int output_cached_menu_window = UNSET;
static int output_cached_main_game_controller_script = UNSET;
static int output_cached_get_ready_starfield_script = UNSET;
static int output_cached_get_ready_starfield_id = UNSET;

static bool OUTPUT_is_menu_intro_artifact_script(int script_number);

static bool OUTPUT_should_skip_stale_menu_entity(int entity_id)
{
	int menu_controller_entity;
	int script_number;

	if ((entity_id < 0) || (entity_id >= MAX_ENTITIES))
	{
		return false;
	}

	if (output_cached_ent_type_permanent_menu_entity == UNSET)
	{
		output_cached_ent_type_permanent_menu_entity = GPL_find_word_value("CONSTANT", "ENT_TYPE_PERMANENT_MENU_ENTITY");
	}
	if (output_cached_menu_entity_identity_flag == UNSET)
	{
		output_cached_menu_entity_identity_flag = GPL_find_word_value("FLAG", "MENU_ENTITY_IDENTITY_FLAG");
	}

	if ((output_cached_ent_type_permanent_menu_entity <= 0) ||
			((entity[entity_id][ENT_ENTITY_TYPE] & output_cached_ent_type_permanent_menu_entity) == 0))
	{
		return false;
	}

	script_number = entity[entity_id][ENT_SCRIPT_NUMBER];
	if (!OUTPUT_is_menu_intro_artifact_script(script_number))
	{
		return false;
	}

	if ((flag_array == NULL) ||
			(output_cached_menu_entity_identity_flag < 0) ||
			(output_cached_menu_entity_identity_flag >= MAX_FLAGS))
	{
		return false;
	}

	menu_controller_entity = flag_array[output_cached_menu_entity_identity_flag];
	if ((menu_controller_entity >= 0) &&
			(menu_controller_entity < MAX_ENTITIES) &&
			(entity[menu_controller_entity][ENT_ALIVE] >= ALIVE))
	{
		return false;
	}

	return true;
}

static bool OUTPUT_is_menu_intro_artifact_script(int script_number)
{
	if (output_cached_menu_intro_wiz_or_cat_script == UNSET)
	{
		output_cached_menu_intro_wiz_or_cat_script = GPL_find_word_value("SCRIPTS", "MENU_INTRO_WIZ_OR_CAT");
	}
	if (output_cached_menu_intro_wiz_or_cat_drag_script == UNSET)
	{
		output_cached_menu_intro_wiz_or_cat_drag_script = GPL_find_word_value("SCRIPTS", "MENU_INTRO_WIZ_OR_CAT_DRAG");
	}
	if (output_cached_menu_intro_sensible_logo_script == UNSET)
	{
		output_cached_menu_intro_sensible_logo_script = GPL_find_word_value("SCRIPTS", "MENU_INTRO_SENSIBLE_SOFTWARE_LOGO");
	}

	return
		(script_number == output_cached_menu_intro_wiz_or_cat_script) ||
		(script_number == output_cached_menu_intro_wiz_or_cat_drag_script) ||
		(script_number == output_cached_menu_intro_sensible_logo_script);
}

static bool OUTPUT_should_skip_menu_intro_artifact(int window_number, int script_number)
{
	int menu_window;

	if (!OUTPUT_is_menu_intro_artifact_script(script_number))
	{
		return false;
	}

	if (window_details == NULL)
	{
		return false;
	}

	if (output_cached_menu_window == UNSET)
	{
		output_cached_menu_window = GPL_find_word_value("CONSTANT", "MENU_WINDOW");
		if (output_cached_menu_window == UNSET)
		{
			output_cached_menu_window = 0;
		}
	}
	menu_window = output_cached_menu_window;
	if (window_number != menu_window)
	{
		return false;
	}

	if (output_cached_main_game_controller_script == UNSET)
	{
		output_cached_main_game_controller_script =
			GPL_find_word_value("SCRIPTS", "MAIN_GAME_CONTROLLER");
	}

	if (output_cached_main_game_controller_script < 0)
	{
		return false;
	}

	for (int entity_id = 0; entity_id < MAX_ENTITIES; entity_id++)
	{
		if ((entity[entity_id][ENT_SCRIPT_NUMBER] == output_cached_main_game_controller_script) &&
				(entity[entity_id][ENT_ALIVE] >= JUST_BORN))
		{
			return true;
		}
	}

	return false;
}

static bool OUTPUT_is_main_game_controller_live(void)
{
	if (output_cached_main_game_controller_script == UNSET)
	{
		output_cached_main_game_controller_script =
			GPL_find_word_value("SCRIPTS", "MAIN_GAME_CONTROLLER");
	}

	if (output_cached_main_game_controller_script < 0)
	{
		return false;
	}

	for (int entity_id = 0; entity_id < MAX_ENTITIES; entity_id++)
	{
		if ((entity[entity_id][ENT_SCRIPT_NUMBER] == output_cached_main_game_controller_script) &&
				(entity[entity_id][ENT_ALIVE] >= JUST_BORN))
		{
			return true;
		}
	}

	return false;
}

static bool OUTPUT_is_get_ready_starfield_script(int script_number)
{
	if (output_cached_get_ready_starfield_script == UNSET)
	{
		output_cached_get_ready_starfield_script =
			GPL_find_word_value("SCRIPTS", "GET_READY_STARFIELD");
	}

	return (script_number == output_cached_get_ready_starfield_script);
}

static bool OUTPUT_is_laboratory_script(int script_number)
{
	const char *script_name;

	if ((script_number < 0) || (script_number >= GPL_list_size("SCRIPTS")))
	{
		return false;
	}

	script_name = GPL_get_entry_name("SCRIPTS", script_number);
	if (script_name == NULL)
	{
		return false;
	}

	return
		(strcmp(script_name, "laboratory") == 0) ||
		(strcmp(script_name, "laboratory_starfield") == 0) ||
		(strcmp(script_name, "laboratory_nebula") == 0) ||
		(strncmp(script_name, "lab_", 4) == 0);
}

static int output_frame_profile_text_entity_count = 0;
static int output_frame_profile_distorted_text_entity_count = 0;
static int output_frame_profile_hot_tunnel_entity_count = 0;

#define OUTPUT_TUNNEL_QUEUE_MAX 256
typedef struct
{
	bool coloured;
	platform_renderer_state_snapshot state;
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
	float r[4];
	float g[4];
	float b[4];
	float a[4];
} output_tunnel_command;

static output_tunnel_command output_tunnel_queue[OUTPUT_TUNNEL_QUEUE_MAX];
static int output_tunnel_queue_count = 0;

bool software_mode_active = false;

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

static int OUTPUT_get_main_menu_logo_section_script(void)
{
	if (output_cached_main_menu_logo_section_script == UNSET)
	{
		output_cached_main_menu_logo_section_script = GPL_find_word_value("SCRIPTS", "MAIN_MENU_LOGO_SECTION");
	}
	return output_cached_main_menu_logo_section_script;
}

static bool OUTPUT_is_hot_tunnel_script(int script_number)
{
	if (output_cached_main_menu_tunnel_light_script == UNSET)
	{
		output_cached_main_menu_tunnel_light_script = GPL_find_word_value("SCRIPTS", "MAIN_MENU_TUNNEL_LIGHT");
	}
	if (output_cached_menu_tunnel_starfield_script == UNSET)
	{
		output_cached_menu_tunnel_starfield_script = GPL_find_word_value("SCRIPTS", "MENU_TUNNEL_STARFIELD");
	}

	return
		(script_number == output_cached_main_menu_tunnel_light_script) ||
		(script_number == output_cached_menu_tunnel_starfield_script);
}

static bool OUTPUT_is_profiled_text_script(int script_number)
{
	if (output_cached_menu_credit_text_script == UNSET)
	{
		output_cached_menu_credit_text_script = GPL_find_word_value("SCRIPTS", "MENU_CREDIT_TEXT");
	}
	if (output_cached_menu_scrolly_text_script == UNSET)
	{
		output_cached_menu_scrolly_text_script = GPL_find_word_value("SCRIPTS", "MENU_SCROLLY_TEXT");
	}
	if (output_cached_menu_hiscore_text_script == UNSET)
	{
		output_cached_menu_hiscore_text_script = GPL_find_word_value("SCRIPTS", "MENU_HISCORE_TEXT");
	}
	if (output_cached_menu_hiscore_instruction_text_script == UNSET)
	{
		output_cached_menu_hiscore_instruction_text_script = GPL_find_word_value("SCRIPTS", "MENU_HISCORE_INSTRUCTION_TEXT");
	}
	if (output_cached_overlay_fading_text_script == UNSET)
	{
		output_cached_overlay_fading_text_script = GPL_find_word_value("SCRIPTS", "OVERLAY_FADING_TEXT");
	}
	if (output_cached_bonus_level_summary_text_script == UNSET)
	{
		output_cached_bonus_level_summary_text_script = GPL_find_word_value("SCRIPTS", "BONUS_LEVEL_SUMMARY_TEXT");
	}

	return
		(script_number == output_cached_menu_credit_text_script) ||
		(script_number == output_cached_menu_scrolly_text_script) ||
		(script_number == output_cached_menu_hiscore_text_script) ||
		(script_number == output_cached_menu_hiscore_instruction_text_script) ||
		(script_number == output_cached_overlay_fading_text_script) ||
		(script_number == output_cached_bonus_level_summary_text_script);
}

static bool OUTPUT_is_profiled_distorted_text_script(int script_number)
{
	if (output_cached_menu_option_text_script == UNSET)
	{
		output_cached_menu_option_text_script = GPL_find_word_value("SCRIPTS", "MENU_OPTION_TEXT");
	}
	if (output_cached_menu_tune_text_script == UNSET)
	{
		output_cached_menu_tune_text_script = GPL_find_word_value("SCRIPTS", "MENU_TUNE_TEXT");
	}

	return
		(script_number == output_cached_menu_option_text_script) ||
		(script_number == output_cached_menu_tune_text_script);
}

static bool OUTPUT_is_simple_translated_sprite(
		int draw_type,
		int opengl_booleans,
		bool texture_combiner_available,
		int secondary_bitmap_number)
{
	if (draw_type != DRAW_MODE_SPRITE)
	{
		return false;
	}
	if (OUTPUT_is_secondary_multitexture_active(texture_combiner_available, opengl_booleans, secondary_bitmap_number))
	{
		return false;
	}
	if (opengl_booleans & (OPENGL_BOOLEAN_INTERPOLATED |
			OPENGL_BOOLEAN_CLIP_FRAME |
			OPENGL_BOOLEAN_HORI_FLIPPED |
			OPENGL_BOOLEAN_VERT_FLIPPED |
			OPENGL_BOOLEAN_BLEND_EITHER |
			OPENGL_BOOLEAN_VERTEX_COLOUR |
			OPENGL_BOOLEAN_VERTEX_COLOUR_ALPHA |
			OPENGL_BOOLEAN_SCALE |
			OPENGL_BOOLEAN_ROTATE |
			OPENGL_BOOLEAN_SECONDARY_SCALE |
			OPENGL_BOOLEAN_SECONDARY_ROTATE |
			OPENGL_BOOLEAN_ROTATE_CLOCKWISE |
			OPENGL_BOOLEAN_ARBITRARY_QUAD |
			OPENGL_BOOLEAN_ARBITRARY_PERSPECTIVE_QUAD |
			OPENGL_BOOLEAN_INDIVIDUAL_VERTEX_COLOUR_ALPHA))
	{
		return false;
	}
	return true;
}

static bool OUTPUT_renderer_state_snapshots_match(const platform_renderer_state_snapshot *a, const platform_renderer_state_snapshot *b)
{
	if ((a == NULL) || (b == NULL))
	{
		return false;
	}

	return
		(a->texture_handle == b->texture_handle) &&
		(a->blend_enabled == b->blend_enabled) &&
		(a->blend_mode == b->blend_mode) &&
		(a->colour_r == b->colour_r) &&
		(a->colour_g == b->colour_g) &&
		(a->colour_b == b->colour_b) &&
		(a->colour_a == b->colour_a) &&
		(a->tx_a == b->tx_a) &&
		(a->tx_b == b->tx_b) &&
		(a->tx_c == b->tx_c) &&
		(a->tx_d == b->tx_d) &&
		(a->tx_x == b->tx_x) &&
		(a->tx_y == b->tx_y);
}

/* Like OUTPUT_renderer_state_snapshots_match but skips global colour fields.
 * Used for coloured perspective quads where per-vertex colours are baked from
 * cmd->r/g/b/a, making the global colour irrelevant for run-grouping. */
static bool OUTPUT_renderer_state_snapshots_match_no_colour(const platform_renderer_state_snapshot *a, const platform_renderer_state_snapshot *b)
{
	if ((a == NULL) || (b == NULL))
	{
		return false;
	}

	return
		(a->texture_handle == b->texture_handle) &&
		(a->blend_enabled == b->blend_enabled) &&
		(a->blend_mode == b->blend_mode) &&
		(a->tx_a == b->tx_a) &&
		(a->tx_b == b->tx_b) &&
		(a->tx_c == b->tx_c) &&
		(a->tx_d == b->tx_d) &&
		(a->tx_x == b->tx_x) &&
		(a->tx_y == b->tx_y);
}

static void OUTPUT_flush_hot_tunnel_queue(void)
{
	int run_start;
	platform_renderer_state_snapshot saved_state;
	/* Static batch buffers: tunnel queue is capped at OUTPUT_TUNNEL_QUEUE_MAX so no heap needed */
	static platform_renderer_coloured_perspective_quad s_coloured_batch[OUTPUT_TUNNEL_QUEUE_MAX];
	static platform_renderer_perspective_quad s_plain_batch[OUTPUT_TUNNEL_QUEUE_MAX];

	if (output_tunnel_queue_count <= 0)
	{
		return;
	}

	PLATFORM_RENDERER_capture_state_snapshot(&saved_state);
	for (run_start = 0; run_start < output_tunnel_queue_count;)
	{
		int run_end = run_start + 1;
		const output_tunnel_command *first = &output_tunnel_queue[run_start];
		PLATFORM_RENDERER_apply_state_snapshot(&first->state);
		while (run_end < output_tunnel_queue_count)
		{
			const output_tunnel_command *next = &output_tunnel_queue[run_end];
			if ((next->coloured != first->coloured) ||
				(first->coloured
					? !OUTPUT_renderer_state_snapshots_match_no_colour(&next->state, &first->state)
					: !OUTPUT_renderer_state_snapshots_match(&next->state, &first->state)))
			{
				break;
			}
			run_end++;
		}

		if (first->coloured)
		{
			const int run_count = run_end - run_start;
			int i;
			for (i = 0; i < run_count; i++)
			{
				const output_tunnel_command *cmd = &output_tunnel_queue[run_start + i];
				s_coloured_batch[i].quad.x0 = cmd->x0;
				s_coloured_batch[i].quad.y0 = cmd->y0;
				s_coloured_batch[i].quad.x1 = cmd->x1;
				s_coloured_batch[i].quad.y1 = cmd->y1;
				s_coloured_batch[i].quad.x2 = cmd->x2;
				s_coloured_batch[i].quad.y2 = cmd->y2;
				s_coloured_batch[i].quad.x3 = cmd->x3;
				s_coloured_batch[i].quad.y3 = cmd->y3;
				s_coloured_batch[i].quad.u1 = cmd->u1;
				s_coloured_batch[i].quad.v1 = cmd->v1;
				s_coloured_batch[i].quad.u2 = cmd->u2;
				s_coloured_batch[i].quad.v2 = cmd->v2;
				s_coloured_batch[i].quad.q = cmd->q;
				memcpy(s_coloured_batch[i].r, cmd->r, sizeof(s_coloured_batch[i].r));
				memcpy(s_coloured_batch[i].g, cmd->g, sizeof(s_coloured_batch[i].g));
				memcpy(s_coloured_batch[i].b, cmd->b, sizeof(s_coloured_batch[i].b));
				memcpy(s_coloured_batch[i].a, cmd->a, sizeof(s_coloured_batch[i].a));
			}
			PLATFORM_RENDERER_draw_bound_coloured_perspective_textured_quad_batch(s_coloured_batch, run_count);
		}
		else
		{
			const int run_count = run_end - run_start;
			int i;
			for (i = 0; i < run_count; i++)
			{
				const output_tunnel_command *cmd = &output_tunnel_queue[run_start + i];
				s_plain_batch[i].x0 = cmd->x0;
				s_plain_batch[i].y0 = cmd->y0;
				s_plain_batch[i].x1 = cmd->x1;
				s_plain_batch[i].y1 = cmd->y1;
				s_plain_batch[i].x2 = cmd->x2;
				s_plain_batch[i].y2 = cmd->y2;
				s_plain_batch[i].x3 = cmd->x3;
				s_plain_batch[i].y3 = cmd->y3;
				s_plain_batch[i].u1 = cmd->u1;
				s_plain_batch[i].v1 = cmd->v1;
				s_plain_batch[i].u2 = cmd->u2;
				s_plain_batch[i].v2 = cmd->v2;
				s_plain_batch[i].q = cmd->q;
			}
			PLATFORM_RENDERER_draw_bound_perspective_textured_quad_batch(s_plain_batch, run_count);
		}
		run_start = run_end;
	}
	PLATFORM_RENDERER_apply_state_snapshot(&saved_state);
	output_tunnel_queue_count = 0;
}

static void OUTPUT_queue_hot_tunnel_perspective_quad(
	float x0, float y0,
	float x1, float y1,
	float x2, float y2,
	float x3, float y3,
	float u1, float v1, float u2, float v2, float q)
{
	output_tunnel_command *cmd;
	if (output_tunnel_queue_count >= OUTPUT_TUNNEL_QUEUE_MAX)
	{
		OUTPUT_flush_hot_tunnel_queue();
	}

	cmd = &output_tunnel_queue[output_tunnel_queue_count++];
	memset(cmd, 0, sizeof(*cmd));
	PLATFORM_RENDERER_capture_state_snapshot(&cmd->state);
	cmd->coloured = false;
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

static void OUTPUT_queue_hot_tunnel_coloured_perspective_quad(
	float x0, float y0,
	float x1, float y1,
	float x2, float y2,
	float x3, float y3,
	float u1, float v1, float u2, float v2, float q,
	const float *r, const float *g, const float *b, const float *a)
{
	output_tunnel_command *cmd;
	if (output_tunnel_queue_count >= OUTPUT_TUNNEL_QUEUE_MAX)
	{
		OUTPUT_flush_hot_tunnel_queue();
	}

	cmd = &output_tunnel_queue[output_tunnel_queue_count++];
	memset(cmd, 0, sizeof(*cmd));
	PLATFORM_RENDERER_capture_state_snapshot(&cmd->state);
	cmd->coloured = true;
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
	memcpy(cmd->r, r, sizeof(cmd->r));
	memcpy(cmd->g, g, sizeof(cmd->g));
	memcpy(cmd->b, b, sizeof(cmd->b));
	memcpy(cmd->a, a, sizeof(cmd->a));
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

void INPUT_load_media_datafiles(void)
{
}

void INPUT_unload_datafiles(void)
{
}

void OUTPUT_message(char *msg)
{
	SDL_ShowSimpleMessageBox(
			SDL_MESSAGEBOX_INFORMATION,
			"WizBall",
			msg,
			NULL);
}

void OUTPUT_rectangle(int x1, int y1, int x2, int y2, int r, int g, int b)
{
	PLATFORM_RENDERER_draw_outline_rect(x1, y1, x2, y2, r, g, b, virtual_screen_height);
}

void OUTPUT_rectangle_by_size(int x, int y, int width, int height, int r, int g, int b)
{
	OUTPUT_rectangle(x, y, x + width - 1, y + height - 1, r, g, b);
}

void OUTPUT_filled_rectangle(int x1, int y1, int x2, int y2, int r, int g, int b)
{
	PLATFORM_RENDERER_draw_filled_rect(x1, y1, x2, y2, r, g, b, virtual_screen_height);
}

void OUTPUT_filled_rectangle_by_size(int x, int y, int width, int height, int r, int g, int b)
{
	OUTPUT_filled_rectangle(x, y, x + width - 1, y + height - 1, r, g, b);
}

void OUTPUT_text(int x, int y, char *text, int r, int g, int b, int scale)
{
	int c;
	int length = strlen(text);
	int drawable_glyphs = 0;
	float scale_multiplier = float(scale) / 10000.0f;
	float pen_x = 0.0f;
	SDL_Vertex stack_vertices[64 * 4];
	SDL_Vertex *vertices = stack_vertices;
	int vertex_count = 0;

	// Just get the basic sizes for the first letter as it'll be the same for the rest.

	sprite *sp = GET_sprite(small_font_gfx, 0);

	float u1 = sp->u1;
	float u2 = sp->u2;
	float v1 = sp->v1;
	float v2 = sp->v2;

	float left = -sp->pivot_x;
	float right = sp->width - sp->pivot_x;
	float up = sp->pivot_y;
	float down = -(sp->height - sp->pivot_y);
	float scaled_left = left * scale_multiplier;
	float scaled_right = right * scale_multiplier;
	float scaled_up = up * scale_multiplier;
	float scaled_down = down * scale_multiplier;
	float base_x = (float)x;
	float base_y = (float)(virtual_screen_height - y);
	int legacy_text_r = (r * 255) / 256;
	int legacy_text_g = (g * 255) / 256;
	int legacy_text_b = (b * 255) / 256;

	for (c = 0; c < length; c++)
	{
		if (text[c] != ' ')
		{
			drawable_glyphs++;
		}
	}

	if (drawable_glyphs <= 0)
	{
		return;
	}

	if (drawable_glyphs * 4 > (int)(sizeof(stack_vertices) / sizeof(stack_vertices[0])))
	{
		vertices = (SDL_Vertex *)malloc((size_t)drawable_glyphs * 4 * sizeof(SDL_Vertex));
		if (vertices == NULL)
		{
			return;
		}
	}

	for (c = 0; c < length; c++)
	{
		sprite *sp = GET_sprite(small_font_gfx, text[c] - 32);

		u1 = sp->u1;
		u2 = sp->u2;
		v1 = sp->v1;
		v2 = sp->v2;

		if (text[c] != ' ')
		{
			const float glyph_x0 = base_x + (pen_x * scale_multiplier) + scaled_left;
			const float glyph_x1 = base_x + (pen_x * scale_multiplier) + scaled_right;
			const float glyph_y0 = base_y + scaled_up;
			const float glyph_y1 = base_y + scaled_down;

			vertices[vertex_count + 0].position.x = glyph_x0;
			vertices[vertex_count + 0].position.y = (float)virtual_screen_height - glyph_y0;
			vertices[vertex_count + 1].position.x = glyph_x0;
			vertices[vertex_count + 1].position.y = (float)virtual_screen_height - glyph_y1;
			vertices[vertex_count + 2].position.x = glyph_x1;
			vertices[vertex_count + 2].position.y = (float)virtual_screen_height - glyph_y1;
			vertices[vertex_count + 3].position.x = glyph_x1;
			vertices[vertex_count + 3].position.y = (float)virtual_screen_height - glyph_y0;

			vertices[vertex_count + 0].tex_coord.x = u1;
			vertices[vertex_count + 0].tex_coord.y = v1;
			vertices[vertex_count + 1].tex_coord.x = u1;
			vertices[vertex_count + 1].tex_coord.y = v2;
			vertices[vertex_count + 2].tex_coord.x = u2;
			vertices[vertex_count + 2].tex_coord.y = v2;
			vertices[vertex_count + 3].tex_coord.x = u2;
			vertices[vertex_count + 3].tex_coord.y = v1;

			vertices[vertex_count + 0].color.r = (Uint8)legacy_text_r;
			vertices[vertex_count + 0].color.g = (Uint8)legacy_text_g;
			vertices[vertex_count + 0].color.b = (Uint8)legacy_text_b;
			vertices[vertex_count + 0].color.a = 255;
			vertices[vertex_count + 1].color = vertices[vertex_count + 0].color;
			vertices[vertex_count + 2].color = vertices[vertex_count + 0].color;
			vertices[vertex_count + 3].color = vertices[vertex_count + 0].color;

			vertex_count += 4;
		}

		pen_x += right;
	}

	PLATFORM_RENDERER_draw_textured_quad_batch(
			ACTIVE_BMPS[small_font_gfx].texture_handle,
			vertices,
			vertex_count / 4,
			true);

	if (vertices != stack_vertices)
	{
		free(vertices);
	}
}

void OUTPUT_truncated_text(int max_chars, int x, int y, char *text, int r, int g, int b)
{
	char temp[TEXT_LINE_SIZE];
	strcpy(temp, text);
	temp[max_chars] = '\0';
	OUTPUT_text(x, y, temp, r, g, b);
}

void OUTPUT_centred_text(int x, int y, char *text, int r, int g, int b)
{
	int offet_x;

	offet_x = strlen(text) * (FONT_WIDTH / 2);

	OUTPUT_text(x - offet_x, y, text, r, g, b);
}


void OUTPUT_boxed_centred_text(int x, int y, char *text, int r, int g, int b)
{
	int offet_x;

	offet_x = strlen(text) * (FONT_WIDTH / 2);

	OUTPUT_text(x - offet_x, y, text, r, g, b);

	OUTPUT_rectangle(x - strlen(text) * (FONT_WIDTH / 2) - 2, y - 2, x + strlen(text) * (FONT_WIDTH / 2) + 1, y + FONT_HEIGHT + 1, r / 2, g / 2, b / 2);
}

void OUTPUT_right_aligned_text(int x, int y, char *text, int r, int g, int b)
{
	int offet_x;

	offet_x = strlen(text) * (FONT_WIDTH);

	OUTPUT_text(x - offet_x, y, text, r, g, b);
}

void OUTPUT_line(int x1, int y1, int x2, int y2, int r, int g, int b)
{
	PLATFORM_RENDERER_draw_line(x1, y1, x2, y2, r, g, b);
}

void OUTPUT_set_arbitrary_quad_from_base(int entity_index)
{
	int *entity_pointer = &entity[entity_index][0];

	if ((entity_pointer[ENT_SPRITE] > UNSET) && (entity_pointer[ENT_CURRENT_FRAME] > UNSET))
	{
		sprite *sp = GET_sprite(entity_pointer[ENT_SPRITE], entity_pointer[ENT_CURRENT_FRAME]);

		arbitrary_quads_struct *aq = &arbitrary_quads[entity_index];

		aq->x[0] = -sp->pivot_x;
		aq->x[2] = -sp->pivot_x;

		aq->x[1] = sp->width - sp->pivot_x;
		aq->x[3] = sp->width - sp->pivot_x;

		aq->y[0] = sp->pivot_y;
		aq->y[1] = sp->pivot_y;

		aq->y[2] = -(sp->height - sp->pivot_y);
		aq->y[3] = -(sp->height - sp->pivot_y);
	}
}

void OUTPUT_hline(int x1, int y1, int x2, int r, int g, int b)
{
	OUTPUT_line(x1, y1, x2, y1, r, g, b);
}

void OUTPUT_vline(int x1, int y1, int y2, int r, int g, int b)
{
	OUTPUT_line(x1, y1, x1, y2, r, g, b);
}

void OUTPUT_circle(int x, int y, int radius, int r, int g, int b)
{
	PLATFORM_RENDERER_draw_circle(x, y, radius, r, g, b, virtual_screen_height, OPENGL_CIRCLE_RESOLUTION);
}

void OUTPUT_centred_square(int x, int y, int widthandheight, int r, int g, int b)
{
	OUTPUT_rectangle(x - widthandheight, y - widthandheight, x + widthandheight, y + widthandheight, r, g, b);
}

void OUTPUT_clear_screen(void)
{
	PLATFORM_RENDERER_clear_backbuffer();
	output_frame_profile_text_entity_count = 0;
	output_frame_profile_distorted_text_entity_count = 0;
	output_frame_profile_hot_tunnel_entity_count = 0;
}

#define TRIPLEBUFFER (0)
#define PAGEFLIP (1)
#define SYSTEMBUFFER (2)
#define DOUBLEBUFFER (3)

void OUTPUT_updatescreen(void)
{
	int present_width = 0; // SCREEN_W;
	int present_height = 0; // SCREEN_H;
	if (PLATFORM_RENDERER_is_sdl2_stub_enabled())
	{
		present_width = game_screen_width;
		present_height = game_screen_height;
	}
	PLATFORM_RENDERER_present_frame(present_width, present_height);
}

void OUTPUT_enable_multi_texture_effects(void)
{
	if (best_texture_combiner_available)
	{
		texture_combiner_available = true;
	}
}

void OUTPUT_disable_multi_texture_effects(void)
{
	texture_combiner_available = false;
}

void OUTPUT_setup_project_list(char *text)
{
	int y = 0;

	char *pointer = strtok(text, "\n");

	while (pointer != NULL)
	{
		// textout(screen, font, pointer, 0, y, makecol(0, 0, 0));
		y += 8;
		pointer = strtok(NULL, "\n");
	}
}

void OUTPUT_setup_running_mode(void)
{
	char options[3][64] = {
			"1. Parse, debug, output datfiles.",
			"2. Parse, output datfiles.",
			"3. Load from datfiles.",
	};

	int y = 0;
	int counter;

	for (counter = 0; counter < 3; counter++)
	{
		// textout(screen, font, &options[counter][0], 0, y, makecol(0, 0, 0));
		y += 8;
	}
}

void OUTPUT_setup_allegro(bool windowed, int colour_depth, int base_screen_width, int base_screen_height, int screen_multiplier)
{
	int result;
	int t;

	game_screen_width = base_screen_width * screen_multiplier;
	game_screen_height = base_screen_height * screen_multiplier;

	virtual_screen_width = base_screen_width;
	virtual_screen_height = base_screen_height;

	window_scale_multiplier = screen_multiplier;

	float_window_scale_multiplier = float(screen_multiplier);

	bool sdl_stub_enabled = PLATFORM_RENDERER_is_sdl2_stub_enabled();
	bool sdl_native_sprites_enabled = true;
	bool sdl_native_primary_enabled = true;
	bool sdl_primary_active = PLATFORM_RENDERER_is_sdl2_stub_enabled();
	char sdl_stub_line[MAX_LINE_SIZE];
	snprintf(sdl_stub_line, sizeof(sdl_stub_line), "SDL2 renderer stub: enabled=%d native_sprites=%d native_primary=%d sdl_primary=%d lazy_init=1 status=%s", sdl_stub_enabled ? 1 : 0, sdl_native_sprites_enabled ? 1 : 0, sdl_native_primary_enabled ? 1 : 0, sdl_primary_active ? 1 : 0, PLATFORM_RENDERER_get_sdl2_stub_status());
	MAIN_add_to_log(sdl_stub_line);

	MAIN_add_to_log("SDL2 renderer mode active.");
	// set_color_depth(colour_depth);
	// set_color_conversion(COLORCONV_TOTAL + COLORCONV_KEEP_TRANS);
	OUTPUT_prepare_sdl_stub_bootstrap(windowed);

	secondary_colour_available = false;
	best_texture_combiner_available = false;
	texture_combiner_available = false;
	PLATFORM_RENDERER_set_active_texture_proc(NULL);
}

void OUTPUT_draw_sprite(int bitmap_number, int sprite_number, int x, int y, int r, int g, int b)
{
	sprite *sp = GET_sprite(bitmap_number, sprite_number);

	float u1 = sp->u1;
	float u2 = sp->u2;
	float v1 = sp->v1;
	float v2 = sp->v2;

	float left = -sp->pivot_x;
	float right = sp->width - sp->pivot_x;
	float up = sp->pivot_y;
	float down = -(sp->height - sp->pivot_y);

	PLATFORM_RENDERER_draw_textured_quad(
			ACTIVE_BMPS[bitmap_number].texture_handle,
			r, g, b,
			x, virtual_screen_height - y, virtual_screen_height,
			left, right, up, down,
			u1, v1, u2, v2,
			false);
}

void OUTPUT_draw_masked_sprite(int bitmap_number, int sprite_number, int x, int y, int r, int g, int b)
{
	sprite *sp = GET_sprite(bitmap_number, sprite_number);

	float u1 = sp->u1;
	float u2 = sp->u2;
	float v1 = sp->v1;
	float v2 = sp->v2;

	float left = -sp->pivot_x;
	float right = sp->width - sp->pivot_x;
	float up = sp->pivot_y;
	float down = -(sp->height - sp->pivot_y);

	PLATFORM_RENDERER_draw_textured_quad(
			ACTIVE_BMPS[bitmap_number].texture_handle,
			r, g, b,
			x, virtual_screen_height - y, virtual_screen_height,
			left, right, up, down,
			u1, v1, u2, v2,
			true);
}

void OUTPUT_get_sprite_uvs(int bitmap_number, int sprite_number, float &u1, float &v1, float &u2, float &v2)
{
	u1 = ACTIVE_BMPS[bitmap_number].sprite_list[sprite_number].u1;
	v1 = ACTIVE_BMPS[bitmap_number].sprite_list[sprite_number].v1;
	u2 = ACTIVE_BMPS[bitmap_number].sprite_list[sprite_number].u2;
	v2 = ACTIVE_BMPS[bitmap_number].sprite_list[sprite_number].v2;
}

int OUTPUT_get_sprite_width(int bitmap_number, int sprite_number)
{
	sprite *sp = GET_sprite(bitmap_number, sprite_number);
	return sp->width;
}

int OUTPUT_get_sprite_height(int bitmap_number, int sprite_number)
{
	sprite *sp = GET_sprite(bitmap_number, sprite_number);
	return sp->height;
}

int OUTPUT_get_sprite_pivot_x(int bitmap_number, int sprite_number)
{
	sprite *sp = GET_sprite(bitmap_number, sprite_number);
	return sp->pivot_x;
}

int OUTPUT_get_sprite_pivot_y(int bitmap_number, int sprite_number)
{
	sprite *sp = GET_sprite(bitmap_number, sprite_number);
	return sp->pivot_y;
}

int OUTPUT_bitmap_width(int bitmap_number)
{
	return (ACTIVE_BMPS[bitmap_number].width);
}

int OUTPUT_bitmap_height(int bitmap_number)
{
	return (ACTIVE_BMPS[bitmap_number].height);
}

int OUTPUT_bitmap_frames(int bitmap_number)
{
	return (ACTIVE_BMPS[bitmap_number].sprite_count);
}

static bool OUTPUT_is_valid_bitmap_index(int bitmap_number)
{
	return (bitmap_number >= 0 && bitmap_number < total_sdl_bitmaps_loaded);
}

static bool OUTPUT_is_valid_sprite_frame(int bitmap_number, int sprite_number)
{
	if (!OUTPUT_is_valid_bitmap_index(bitmap_number))
	{
		return false;
	}

	if (ACTIVE_BMPS[bitmap_number].sprite_list == NULL)
	{
		return false;
	}

	return (sprite_number >= 0 && sprite_number < ACTIVE_BMPS[bitmap_number].sprite_count);
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

#define CHUNK_SIZE (128)

void INPUT_create_sprite_space(int parent_bitmap, int total_sprites)
{
	char error[MAX_LINE_SIZE];

	if (!OUTPUT_is_valid_bitmap_index(parent_bitmap))
	{
			snprintf(error, sizeof(error), "Invalid parent bitmap index %i while creating sprite space.", parent_bitmap);
		OUTPUT_fatal_exit(error);
	}

	if ((total_sprites <= 0) || (total_sprites > 50000))
	{
			snprintf(error, sizeof(error), "Invalid sprite count %i for bitmap index %i.", total_sprites, parent_bitmap);
		OUTPUT_fatal_exit(error);
	}

	ACTIVE_BMPS[parent_bitmap].sprite_count = total_sprites;
	ACTIVE_BMPS[parent_bitmap].sprite_list = (sprite *)malloc(total_sprites * sizeof(sprite));

	if (ACTIVE_BMPS[parent_bitmap].sprite_list == NULL)
	{
			snprintf(error, sizeof(error), "Out of memory while allocating %i sprites for bitmap %i.", total_sprites, parent_bitmap);
		OUTPUT_fatal_exit(error);
	}
}

void INPUT_create_equal_sized_sprite_series(int parent_bitmap, char *name_base, int sprite_width, int sprite_height, int pivot_x, int pivot_y)
{
	// This uses the dimensions of the parent bitmap to make a series of sprites.
	// If it does not divide exactly then some fragment sprites will be generated.

	char full_name[NAME_SIZE];
	int counter;
	int x, y;
	int total_sprites;

	total_sprites = (ACTIVE_BMPS[parent_bitmap].height / sprite_height) * (ACTIVE_BMPS[parent_bitmap].width / sprite_width);
	fprintf(stderr, "Creating %i sprites for bitmap %i.\n", total_sprites, parent_bitmap);
	INPUT_create_sprite_space(parent_bitmap, total_sprites);

	counter = 0;

	for (y = 0; y < ACTIVE_BMPS[parent_bitmap].height - (sprite_height - 1); y += sprite_height)
	{
		for (x = 0; x < (ACTIVE_BMPS[parent_bitmap].width - (sprite_width - 1)); x += sprite_width)
		{
			snprintf(full_name, sizeof(full_name), "%s%i", name_base, counter);
			INPUT_create_sprite(parent_bitmap, counter, full_name, x, y, sprite_width, sprite_height, pivot_x, pivot_y);
			counter++;
		}
	}
}

int INPUT_get_nearest_value(int value, int compare_1, int compare_2, int compare_3)
{
	int dist_1, dist_2, dist_3;

	dist_1 = MATH_abs_int(compare_1 - value);
	dist_2 = MATH_abs_int(compare_2 - value);
	dist_3 = MATH_abs_int(compare_3 - value);

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

#define ARB_DATA_UNIT_SIZE (6)

void INPUT_create_arbitrary_sized_sprite_series(int parent_bitmap, char *name_base, char *descriptor_file)
{
	// This opens the descriptor file and reads the sprite sizes from it.
	// The descriptor itself is generated by another function.

	char full_pack_filename[MAX_LINE_SIZE];
	char full_name[NAME_SIZE];
	int counter;
	int t;
	int x, y, width, height, pivot_x, pivot_y;
	char line[MAX_LINE_SIZE];
	char *pointer;
	const char *normalised_descriptor_file = OUTPUT_strip_project_prefix(descriptor_file);

	// if (load_from_dat_file)
	// {
	// 	fprintf(stderr, "[SPRITE-DESCRIPTOR] Attempting to load sprite descriptor '%s' from gfx.dat.\n", descriptor_file);
	// 	snprintf(, sizeof(), "%s\\gfx.dat#%s", MAIN_get_pack_project_name(), normalised_descriptor_file);
	// 	fix_filename_slashes(full_pack_filename);
	// 	PACKFILE *packfile_pointer = pack_fopen(full_pack_filename, "r");

	// 	if (packfile_pointer != NULL)
	// 	{
	// 		pack_fgets(line, MAX_LINE_SIZE, packfile_pointer);
	// 		strtok(line, "\t\n ");

	// 		counter = atoi(line);

	// 		INPUT_create_sprite_space(parent_bitmap, counter);

	// 		for (t = 0; t < counter; t++)
	// 		{
	// 			snprintf(, sizeof(), "%s%i", name_base, t);

	// 			pack_fgets(line, MAX_LINE_SIZE, packfile_pointer);

	// 			pointer = strtok(line, ", ");
	// 			if (pointer == NULL)
	// 			{
	// 				OUTPUT_fatal_exit((char *)"Malformed sprite descriptor line (x missing).");
	// 			}
	// 			x = atoi(pointer);
	// 			pointer = strtok(NULL, ", ");
	// 			if (pointer == NULL)
	// 			{
	// 				OUTPUT_fatal_exit((char *)"Malformed sprite descriptor line (y missing).");
	// 			}
	// 			y = atoi(pointer);
	// 			pointer = strtok(NULL, ", ");
	// 			if (pointer == NULL)
	// 			{
	// 				OUTPUT_fatal_exit((char *)"Malformed sprite descriptor line (width missing).");
	// 			}
	// 			width = atoi(pointer);
	// 			pointer = strtok(NULL, ", ");
	// 			if (pointer == NULL)
	// 			{
	// 				OUTPUT_fatal_exit((char *)"Malformed sprite descriptor line (height missing).");
	// 			}
	// 			height = atoi(pointer);
	// 			pointer = strtok(NULL, ", ");
	// 			if (pointer == NULL)
	// 			{
	// 				OUTPUT_fatal_exit((char *)"Malformed sprite descriptor line (pivot_x missing).");
	// 			}
	// 			pivot_x = atoi(pointer);
	// 			pointer = strtok(NULL, ", ");
	// 			if (pointer == NULL)
	// 			{
	// 				OUTPUT_fatal_exit((char *)"Malformed sprite descriptor line (pivot_y missing).");
	// 			}
	// 			pivot_y = atoi(pointer);

	// 			INPUT_create_sprite(parent_bitmap, t, full_name, x, y, width, height, pivot_x, pivot_y);
	// 		}

	// 		pack_fclose(packfile_pointer);
	// 	}
	// else
	// 	{
	// 		char error[MAX_LINE_SIZE];
	// 		snprintf(, sizeof(), "Sprite descriptor not found in gfx.dat: '%s' (normalised '%s').",
	// 						descriptor_file, normalised_descriptor_file);
	// 		OUTPUT_fatal_exit(error);
	// 	}
	// }
	// else
	{
		std::string filename = "sprites/" + std::string(descriptor_file);
		const char *prepended_filename = filename.c_str();

		char *full_filename = MAIN_get_project_filename((char *)prepended_filename, false);

		FILE *file_pointer = FILE_open_read_case_fallback(full_filename);
		if (file_pointer == NULL)
		{
			file_pointer = FILE_open_project_read_case_fallback((char *)normalised_descriptor_file);
		}

		if (file_pointer != NULL)
		{
			counter = FILE_get_int_from_file(file_pointer);

			INPUT_create_sprite_space(parent_bitmap, counter);

			for (t = 0; t < counter; t++)
			{
				snprintf(full_name, sizeof(full_name), "%s%i", name_base, t);

				fgets(line, MAX_LINE_SIZE, file_pointer);

				pointer = strtok(line, ", ");
				if (pointer == NULL)
				{
					OUTPUT_fatal_exit((char *)"Malformed sprite descriptor line (x missing).");
				}
				x = atoi(pointer);
				pointer = strtok(NULL, ", ");
				if (pointer == NULL)
				{
					OUTPUT_fatal_exit((char *)"Malformed sprite descriptor line (y missing).");
				}
				y = atoi(pointer);
				pointer = strtok(NULL, ", ");
				if (pointer == NULL)
				{
					OUTPUT_fatal_exit((char *)"Malformed sprite descriptor line (width missing).");
				}
				width = atoi(pointer);
				pointer = strtok(NULL, ", ");
				if (pointer == NULL)
				{
					OUTPUT_fatal_exit((char *)"Malformed sprite descriptor line (height missing).");
				}
				height = atoi(pointer);
				pointer = strtok(NULL, ", ");
				if (pointer == NULL)
				{
					OUTPUT_fatal_exit((char *)"Malformed sprite descriptor line (pivot_x missing).");
				}
				pivot_x = atoi(pointer);
				pointer = strtok(NULL, ", ");
				if (pointer == NULL)
				{
					OUTPUT_fatal_exit((char *)"Malformed sprite descriptor line (pivot_y missing).");
				}
				pivot_y = atoi(pointer);

				INPUT_create_sprite(parent_bitmap, t, full_name, x, y, width, height, pivot_x, pivot_y);
			}

			fclose(file_pointer);
		}
		else
		{
			char error[MAX_LINE_SIZE];
				snprintf(error, sizeof(error), "Sprite descriptor not found on disk: '%s' (normalised '%s').",
							descriptor_file, normalised_descriptor_file);
			OUTPUT_fatal_exit(error);
		}
	}
}

int INPUT_load_bitmap(char *filename)
{
	return INPUT_load_bitmap_SDL(filename, PLATFORM_RENDERER_SDL_Renderer());
}

int INPUT_load_bitmap_SDL(const char *filename, SDL_Renderer *renderer)
{
	char error[MAX_LINE_SIZE];

	// Allocate or reallocate space for bitmaps
	if (total_sdl_bitmaps_loaded == total_sdl_bitmaps_allocated)
	{
		if (sdl_bmps == NULL)
		{
			sdl_bmps = (sdl_bitmap_holder *)malloc(sizeof(sdl_bitmap_holder) * CHUNK_SIZE);
			total_sdl_bitmaps_allocated = CHUNK_SIZE;
		}
		else
		{
			sdl_bmps = (sdl_bitmap_holder *)realloc(sdl_bmps, sizeof(sdl_bitmap_holder) * (CHUNK_SIZE + total_sdl_bitmaps_allocated));
			total_sdl_bitmaps_allocated += CHUNK_SIZE;
		}

		if (sdl_bmps == NULL)
		{
			snprintf(error, sizeof(error), "Out of memory while allocating bitmap storage for '%s'.", filename);
			OUTPUT_message(error);
			MAIN_add_to_log(error);
			exit(1);
		}
	}

	std::string full_filename = "wizball/sprites/" + std::string(filename);
	const char *prepended_filename = full_filename.c_str();

	char lower_filename[TEXT_LINE_SIZE];

	lowercase_last_path_components(prepended_filename, lower_filename, sizeof(lower_filename), 1);

	// Load the bitmap
	SDL_Surface *surface = IMG_Load(lower_filename); // Use SDL_image for broader format support
	if (surface == NULL)
	{
		fprintf(stderr, "[SDL-Image] Could not load bitmap '%s': %s\n", lower_filename, IMG_GetError());
		return -1;
	}

	// Store the surface and increment the counter
	sdl_bmps[total_sdl_bitmaps_loaded].surface = surface;
	sdl_bmps[total_sdl_bitmaps_loaded].width = surface->w;
	sdl_bmps[total_sdl_bitmaps_loaded].height = surface->h;

	// Optional: Convert surface to a texture (if needed)
	/* NOTE: sdl_bmps[i].texture (from SDL_CreateTextureFromSurface) was never used for rendering —
	 * all draws go through texture_handle / platform_renderer_texture_entries. Skip creation to
	 * avoid leaking VRAM for every loaded bitmap. */
	sdl_bmps[total_sdl_bitmaps_loaded].texture = NULL;

	sdl_bmps[total_sdl_bitmaps_loaded].texture_handle = PLATFORM_RENDERER_create_masked_texture(surface);
	PLATFORM_RENDERER_set_texture_debug_name(sdl_bmps[total_sdl_bitmaps_loaded].texture_handle, lower_filename);
	if ((sdl_bmps[total_sdl_bitmaps_loaded].texture_handle == 0) || (total_sdl_bitmaps_loaded < 10))
	{
		fprintf(
				stderr,
				"[SDL2-TEX-LOAD] bmp=%d handle=%u size=%dx%d software=%d dat=%d\n",
				total_sdl_bitmaps_loaded,
				sdl_bmps[total_sdl_bitmaps_loaded].texture_handle,
				sdl_bmps[total_sdl_bitmaps_loaded].width,
				sdl_bmps[total_sdl_bitmaps_loaded].height,
				software_mode_active ? 1 : 0,
				load_from_dat_file ? 1 : 0);
	}

	/* Surface pixel data has been copied into sdl_rgba_pixels by create_masked_texture; free it now. */
	SDL_FreeSurface(surface);
	sdl_bmps[total_sdl_bitmaps_loaded].surface = NULL; /* Avoid dangling pointer after free. */

	total_sdl_bitmaps_loaded++;

	return total_sdl_bitmaps_loaded - 1; // Return the index of the loaded bitmap
}

void INPUT_create_sprite(int parent_bitmap, int sprite_number, char *name, int x, int y, int width, int height, int pivot_x, int pivot_y)
{
	// First of all create space for the sprite by mallocing or reallocing
	// the space needed.

	float unit_x, unit_y;
	char error[MAX_LINE_SIZE];

	if (!OUTPUT_is_valid_bitmap_index(parent_bitmap))
	{
		snprintf(error, sizeof(error), "Invalid parent bitmap index %i while creating sprite.", parent_bitmap);
		OUTPUT_fatal_exit(error);
	}

	if (ACTIVE_BMPS[parent_bitmap].sprite_list == NULL)
	{
		snprintf(error, sizeof(error), "Sprite list is NULL for bitmap index %i.", parent_bitmap);
		OUTPUT_fatal_exit(error);
	}

	if ((sprite_number < 0) || (sprite_number >= ACTIVE_BMPS[parent_bitmap].sprite_count))
	{
		snprintf(error, sizeof(error), "Sprite frame index %i is out of range [0,%i) for bitmap index %i.",
						sprite_number, ACTIVE_BMPS[parent_bitmap].sprite_count, parent_bitmap);
		OUTPUT_fatal_exit(error);
	}

	if ((ACTIVE_BMPS[parent_bitmap].width <= 0) || (ACTIVE_BMPS[parent_bitmap].height <= 0))
	{
		snprintf(error, sizeof(error), "Invalid bitmap dimensions %ix%i for bitmap index %i while creating sprite.",
						ACTIVE_BMPS[parent_bitmap].width, ACTIVE_BMPS[parent_bitmap].height, parent_bitmap);
		OUTPUT_fatal_exit(error);
	}

	strcpy(ACTIVE_BMPS[parent_bitmap].sprite_list[sprite_number].name, name);
	ACTIVE_BMPS[parent_bitmap].sprite_list[sprite_number].parent_bitmap = parent_bitmap;

	ACTIVE_BMPS[parent_bitmap].sprite_list[sprite_number].x = x;
	ACTIVE_BMPS[parent_bitmap].sprite_list[sprite_number].y = y;
	ACTIVE_BMPS[parent_bitmap].sprite_list[sprite_number].width = width;
	ACTIVE_BMPS[parent_bitmap].sprite_list[sprite_number].height = height;
	ACTIVE_BMPS[parent_bitmap].sprite_list[sprite_number].pivot_x = pivot_x;
	ACTIVE_BMPS[parent_bitmap].sprite_list[sprite_number].pivot_y = pivot_y;

	unit_x = 1.0f / float(ACTIVE_BMPS[parent_bitmap].width);
	unit_y = 1.0f / float(ACTIVE_BMPS[parent_bitmap].height);

	ACTIVE_BMPS[parent_bitmap].sprite_list[sprite_number].u1 = float(x) * unit_x;
	ACTIVE_BMPS[parent_bitmap].sprite_list[sprite_number].u2 = float(x + width) * unit_x;
	ACTIVE_BMPS[parent_bitmap].sprite_list[sprite_number].v1 = 1.0f - (float(y) * unit_y);
	ACTIVE_BMPS[parent_bitmap].sprite_list[sprite_number].v2 = 1.0f - (float(y + height) * unit_y);
}

void OUTPUT_draw_sprite_scale(int bitmap_number, int sprite_number, int x, int y, float scale, int r, int g, int b)
{
	sprite *sp = GET_sprite(bitmap_number, sprite_number);

	float u1 = sp->u1;
	float u2 = sp->u2;
	float v1 = sp->v1;
	float v2 = sp->v2;

	float left = -sp->pivot_x;
	float right = sp->width - sp->pivot_x;
	float up = sp->pivot_y;
	float down = -(sp->height - sp->pivot_y);

	left *= scale;
	right *= scale;
	up *= scale;
	down *= scale;

	PLATFORM_RENDERER_draw_textured_quad(
			ACTIVE_BMPS[bitmap_number].texture_handle,
			r, g, b,
			x, virtual_screen_height - y, virtual_screen_height,
			left, right, up, down,
			u1, v1, u2, v2,
			true);
}

void OUTPUT_draw_sprite_scale_no_pivot(int bitmap_number, int sprite_number, int x, int y, float scale, int r, int g, int b)
{
	sprite *sp = GET_sprite(bitmap_number, sprite_number);

	float u1 = sp->u1;
	float u2 = sp->u2;
	float v1 = sp->v1;
	float v2 = sp->v2;

	float left = -sp->pivot_x;
	float right = sp->width - sp->pivot_x;
	float up = sp->pivot_y;
	float down = -(sp->height - sp->pivot_y);

	left *= scale;
	right *= scale;
	up *= scale;
	down *= scale;

	PLATFORM_RENDERER_draw_textured_quad(
			ACTIVE_BMPS[bitmap_number].texture_handle,
			r, g, b,
			x - left, virtual_screen_height - (y + up), virtual_screen_height,
			left, right, up, down,
			u1, v1, u2, v2,
			true);
}

void OUTPUT_shutdown(void)
{
	// Destroys all the bitmaps in memory...

	MAIN_add_to_log("Destroying graphics...");

	int bitmap_number;

	MAIN_add_to_log("\tOK!");

	PLATFORM_RENDERER_shutdown();
}

#define ARROW_SEGMENT_LENGTH (16)

void OUTPUT_draw_arrow(int x1, int y1, int x2, int y2, int red, int green, int blue, bool suppress_line)
{
	if (suppress_line == false)
	{
		OUTPUT_line(x1, y1, x2, y2, red, green, blue);
	}

	float angle = atan2((x2 - x1), (y2 - y1));
	float arrow_angle_1 = angle - (PI / 4);
	float arrow_angle_2 = angle + (PI / 4);

	int dx = (x2 - x1);
	int dy = (y2 - y1);

	int mid_x = x1 + ((dx * 2) / 3);
	int mid_y = y1 + ((dy * 2) / 3);

	OUTPUT_line(mid_x, mid_y, mid_x - (ARROW_SEGMENT_LENGTH * sin(arrow_angle_1)), mid_y - (ARROW_SEGMENT_LENGTH * cos(arrow_angle_1)), red, green, blue);
	OUTPUT_line(mid_x, mid_y, mid_x - (ARROW_SEGMENT_LENGTH * sin(arrow_angle_2)), mid_y - (ARROW_SEGMENT_LENGTH * cos(arrow_angle_2)), red, green, blue);
}

static void OUTPUT_apply_texture_parameters_from_flags(int opengl_booleans, int old_opengl_booleans, bool force_filter_apply = false)
{
	bool filtered = (opengl_booleans & OPENGL_BOOLEAN_FILTERED) != 0;
	bool old_filtered = (old_opengl_booleans & OPENGL_BOOLEAN_FILTERED) != 0;

	if (force_filter_apply || (filtered != old_filtered))
	{
		PLATFORM_RENDERER_set_texture_filter(filtered);
	}
}

static void OUTPUT_configure_secondary_multitexture_state(int opengl_booleans, int secondary_opengl_booleans, int &old_secondary_opengl_booleans)
{
	bool double_mask_mode = OUTPUT_is_double_multitexture_mode(opengl_booleans);

	OUTPUT_apply_texture_parameters_from_flags(secondary_opengl_booleans, old_secondary_opengl_booleans);
	old_secondary_opengl_booleans = secondary_opengl_booleans;

	if (double_mask_mode)
	{
		PLATFORM_RENDERER_set_combiner_modulate_rgb_replace_alpha_previous();
	}
	else
	{
		PLATFORM_RENDERER_set_combiner_replace_rgb_modulate_alpha_previous();
	}
}

void OUTPUT_draw_starfield(int starfield_id)
{
	int starfield_number = PARTICLES_get_index_from_id(starfield_id);

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

void OUTPUT_draw_starfield_colour(int starfield_id)
{
	int starfield_number = PARTICLES_get_index_from_id(starfield_id);

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

void OUTPUT_draw_starfield_lines(int starfield_id)
{
	static unsigned int get_ready_starfield_trace_counter = 0;

	int starfield_number = PARTICLES_get_index_from_id(starfield_id);

	if (starfields[starfield_number].in_use == false)
	{
		// Trying to draw a non-existant starfield! Naughty!
		return;
	}

	starfield_controller_struct *scp = &starfields[starfield_number];
	starfield_struct *sfp = &scp->stars[0];

	int star_count = scp->number_of_stars;
	if (output_cached_get_ready_starfield_id == UNSET)
	{
		output_cached_get_ready_starfield_id =
			GPL_find_word_value("CONSTANT", "GET_READY_STARFIELD_ID");
	}

	int counter;

	for (counter = 0; counter < star_count; counter++)
	{
		PLATFORM_RENDERER_draw_line(sfp->x, -sfp->y, sfp->ox, -sfp->oy);

		sfp++;
	}
}

void OUTPUT_draw_starfield_colour_lines(int starfield_id)
{
	int starfield_number = PARTICLES_get_index_from_id(starfield_id);

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

void OUTPUT_blank_window_contents(int window_number)
{
	int z_ordering_list;
	int z_ordering_list_size = window_details[window_number].z_ordering_list_size;

	for (z_ordering_list = 0; z_ordering_list < z_ordering_list_size; z_ordering_list++)
	{
		window_details[window_number].z_ordering_list_starts[z_ordering_list] = UNSET;
		window_details[window_number].z_ordering_list_ends[z_ordering_list] = UNSET;
	}
}

void OUTPUT_list_all_window_contents(int look_for_script)
{
	int window_number;
	int counter = 0;

	for (window_number = 0; window_number < number_of_windows; window_number++)
	{
		if (window_details[window_number].active == true)
		{
			window_struct *wp = &window_details[window_number];

			int *entity_pointer;

			int current_entity;

			int z_ordering_list;
			int z_ordering_list_size = wp->z_ordering_list_size;

			for (z_ordering_list = 0; z_ordering_list < z_ordering_list_size; z_ordering_list++)
			{
				int queue_step_counter = 0;
				current_entity = window_details[window_number].z_ordering_list_starts[z_ordering_list];

				while (current_entity != UNSET)
				{
					entity_pointer = &entity[current_entity][0];

					if (entity_pointer[ENT_SCRIPT_NUMBER] == look_for_script)
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

void OUTPUT_draw_window_collision_contents(int window_number, bool draw_world_collision)
{
	int z_ordering_list_size = window_details[window_number].z_ordering_list_size;
	int z_ordering_list;

	float left, right, up, down;
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
	MAIN_debug_last_thing("Got into draw window collision contents...");
#endif

#ifdef RETRENGINE_DEBUG_VERSION_THE_LAST_THING_I_DID
	snprintf(debug_text, sizeof(debug_text), "Collision window number %i = %i %i ", window_number, window_details[window_number].width, window_details[window_number].height);
	MAIN_debug_last_thing(debug_text);
#endif

	float display_scale_x, display_scale_y;
	float window_scale_x, window_scale_y;
	float total_scale_x, total_scale_y;

	display_scale_x = 1.0f;
	display_scale_y = 1.0f;

	float left_window_transform_x = (wp->screen_x + wp->opengl_transform_x) * display_scale_x;
	float bottom_window_transform_y = virtual_screen_height - ((wp->screen_y + wp->opengl_transform_y + wp->scaled_height) * display_scale_y);
	float top_window_transform_y = virtual_screen_height - ((wp->screen_y + wp->opengl_transform_y) * display_scale_y);

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

	int collision_grid_size = 1 << collision_bitshift;

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
	MAIN_debug_last_thing("Reset main colour to white...");
#endif

	PLATFORM_RENDERER_set_colour4f(1.0f, 1.0f, 1.0f, 1.0f);

	if (window_details[window_number].skip_me_this_frame)
	{
		for (z_ordering_list = 0; z_ordering_list < z_ordering_list_size; z_ordering_list++)
		{
			window_details[window_number].z_ordering_list_starts[z_ordering_list] = UNSET;
			window_details[window_number].z_ordering_list_ends[z_ordering_list] = UNSET;
		}

#ifdef RETRENGINE_DEBUG_VERSION_THE_LAST_THING_I_DID
		MAIN_debug_last_thing("Window skipped");
#endif
	}
	else
	{
		// Draw the collision grid at the correct resolution.

		int col_grid_x_offset = window_x % collision_grid_size;
		int col_grid_y_offset = window_y % collision_grid_size;

		PLATFORM_RENDERER_set_window_transform(left_window_transform_x, top_window_transform_y, total_scale_x, total_scale_y);

		PLATFORM_RENDERER_set_colour3f(0.0f, 0.0f, 1.0f);

		if (draw_world_collision == false)
		{
			for (x = 0; x < window_details[window_number].width / total_scale_x; x += collision_grid_size)
			{
				PLATFORM_RENDERER_draw_line(
						x - col_grid_x_offset, 0,
						x - col_grid_x_offset, -window_details[window_number].height / total_scale_y);
			}

			for (y = 0; y < window_details[window_number].height / total_scale_y; y += collision_grid_size)
			{
				PLATFORM_RENDERER_draw_line(
						0, -(y - col_grid_y_offset),
						window_details[window_number].width / total_scale_x, -(y - col_grid_y_offset));
			}
		}

		int z_ordering_list;

		for (z_ordering_list = 0; z_ordering_list < z_ordering_list_size; z_ordering_list++)
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

					PLATFORM_RENDERER_translatef(x, -y, 0.0);

					PLATFORM_RENDERER_set_colour3f(0.0f, 1.0f, 0.0f);

					{
						const float loop_x[4] = {left, left, right, right};
						const float loop_y[4] = {up, down, down, up};
						PLATFORM_RENDERER_draw_line_loop_array(loop_x, loop_y, 4);
					}

					PLATFORM_RENDERER_set_colour3f(1.0f, 1.0f, 1.0f);

					PLATFORM_RENDERER_draw_line(left, 0, right, 0);

					PLATFORM_RENDERER_draw_line(0, up, 0, down);

					PLATFORM_RENDERER_translatef(-x, y, 0);
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

						PLATFORM_RENDERER_translatef(x, -y, 0.0);

						PLATFORM_RENDERER_set_colour3f(0.0f, 1.0f, 0.0f);

						{
							const float loop_x[8] = {left, left * 0.707f, 0, right * 0.707f, right, right * 0.707f, 0, left * 0.707f};
							const float loop_y[8] = {0, up * 0.707f, up, up * 0.707f, 0, down * 0.707f, down, down * 0.707f};
							PLATFORM_RENDERER_draw_line_loop_array(loop_x, loop_y, 8);
						}

						PLATFORM_RENDERER_set_colour3f(1.0f, 1.0f, 1.0f);

						PLATFORM_RENDERER_draw_line(left, 0, right, 0);

						PLATFORM_RENDERER_draw_line(0, up, 0, down);

						PLATFORM_RENDERER_translatef(-x, y, 0);
						break;

					case SHAPE_ROTATED_RECTANGLE:
					case SHAPE_RECTANGLE:

						x = entity_pointer[ENT_WORLD_X] - window_x;
						y = entity_pointer[ENT_WORLD_Y] - window_y;

						left = -entity_pointer[ENT_UPPER_WIDTH];
						right = entity_pointer[ENT_LOWER_WIDTH];
						up = entity_pointer[ENT_UPPER_HEIGHT];
						down = -entity_pointer[ENT_LOWER_HEIGHT];

						PLATFORM_RENDERER_translatef(x, -y, 0.0);

						if (entity_pointer[ENT_COLLISION_SHAPE] == SHAPE_ROTATED_RECTANGLE)
						{
							rotate_angle = float(entity_pointer[ENT_OPENGL_ANGLE]) / 100.0f;
							PLATFORM_RENDERER_rotatef(-rotate_angle, 0.0f, 0.0f, 1.0f);
						}

						PLATFORM_RENDERER_set_colour3f(0.0f, 1.0f, 0.0f);

						{
							const float loop_x[4] = {left, left, right, right};
							const float loop_y[4] = {up, down, down, up};
							PLATFORM_RENDERER_draw_line_loop_array(loop_x, loop_y, 4);
						}

						PLATFORM_RENDERER_set_colour3f(1.0f, 1.0f, 1.0f);

						PLATFORM_RENDERER_draw_line(left, 0, right, 0);

						PLATFORM_RENDERER_draw_line(0, up, 0, down);

						if (entity_pointer[ENT_COLLISION_SHAPE] == SHAPE_ROTATED_RECTANGLE)
						{
							PLATFORM_RENDERER_rotatef(rotate_angle, 0.0f, 0.0f, 1.0f);
						}

						PLATFORM_RENDERER_translatef(-x, y, 0);
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

void OUTPUT_destroy_bitmap_and_sprite_containers(void)
{
	if (ACTIVE_BMPS != NULL)
	{
		int bitmap_number;

		for (bitmap_number = 0; bitmap_number < total_sdl_bitmaps_loaded; bitmap_number++)
		{
			if (ACTIVE_BMPS[bitmap_number].sprite_list != NULL)
			{
				free(ACTIVE_BMPS[bitmap_number].sprite_list);
			}
		}

		free(ACTIVE_BMPS);
		// ACTIVE_BMPS = NULL;
	}

	// Keep renderer texture handle bookkeeping aligned with bitmap container teardown.
	PLATFORM_RENDERER_reset_texture_registry();

	total_sdl_bitmaps_loaded = 0;
	total_sdl_bitmaps_allocated = 0;
}

int OUTPUT_draw_window_contents(int window_number, bool texture_combiner_available)
{
	// Constrain drawing buffer to window and then get weaving!
	// Goes through all the z-ordering queues splunking out the contents to the screen.

	char debug_text[MAX_LINE_SIZE];

#ifdef RETRENGINE_DEBUG_VERSION_THE_LAST_THING_I_DID
	MAIN_debug_last_thing("Got into draw window contents...");
#endif

#ifdef RETRENGINE_DEBUG_VERSION_THE_LAST_THING_I_DID
	snprintf(debug_text, sizeof(debug_text), "Window number %i = %i %i ", window_number, window_details[window_number].width, window_details[window_number].height);
	MAIN_debug_last_thing(debug_text);
#endif

	int z_ordering_list_size = window_details[window_number].z_ordering_list_size;
	int z_ordering_list;

	int old_bitmap_number = UNSET;
	int bitmap_number;

	int frame_number;

	int total_entities_drawn = 0;

	float u1, v1, u2, v2;

	float left, right, up, down;
	int x;
	int y;

	int opengl_booleans;
	int old_opengl_booleans = 0;

	int secondary_frame_number;
	float secondary_u1, secondary_v1, secondary_u2, secondary_v2;
	int secondary_bitmap_number;
	int old_secondary_bitmap_number = UNSET;
	int secondary_opengl_booleans;
	int old_secondary_opengl_booleans = 0;

	int start_bx, start_by, end_bx, end_by;

	float interp_u1, interp_v1, interp_u2, interp_v2;
	float interp_x, interp_y;
	int interp_frame_number;
	int secondary_interp_frame_number;

	int bx, by;

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
	// bool effect_trace_enabled = OUTPUT_is_sdl_effect_trace_enabled();
	static bool window_geom_trace_checked_env = false;
	static bool window_geom_trace_enabled = false;
	static unsigned int window_geom_trace_frame_counter = 0;

	int layer_number;

	int map_number;

	int *entity_pointer;

	int draw_type;

	int map_width, map_height;

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
					wp->buffered_tl_x, wp->buffered_tl_y, wp->buffered_br_x, wp->buffered_br_y);
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
	MAIN_debug_last_thing("Just before secondary colour...");
#endif

	if (secondary_colour_available)
	{
		if (window_details[window_number].secondary_window_colour == true)
		{
#ifdef RETRENGINE_DEBUG_VERSION_THE_LAST_THING_I_DID
			MAIN_debug_last_thing("Setting secondary colour...");
#endif
		}
	}
	else
	{
#ifdef RETRENGINE_DEBUG_VERSION_THE_LAST_THING_I_DID
		MAIN_debug_last_thing("Skipping secondary colour...");
#endif
	}

#ifdef RETRENGINE_DEBUG_VERSION_THE_LAST_THING_I_DID
	MAIN_debug_last_thing("Just after secondary colour...");
#endif

	float display_scale_x, display_scale_y;
	float window_scale_x, window_scale_y;
	float total_scale_x, total_scale_y;

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
	float bottom_window_transform_y = virtual_screen_height - ((wp->screen_y + wp->opengl_transform_y + wp->scaled_height) * display_scale_y);
	float top_window_transform_y = virtual_screen_height - ((wp->screen_y + wp->opengl_transform_y) * display_scale_y);

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
	MAIN_debug_last_thing("Reset main colour to white...");
#endif

	PLATFORM_RENDERER_set_colour4f(1.0f, 1.0f, 1.0f, 1.0f);

	if (window_details[window_number].skip_me_this_frame)
	{
#ifndef RETRENGINE_DEBUG_VERSION_VIEW_WORLD_COLLISION
#ifndef RETRENGINE_DEBUG_VERSION_VIEW_OBJECT_COLLISION
		for (z_ordering_list = 0; z_ordering_list < z_ordering_list_size; z_ordering_list++)
		{
			window_details[window_number].z_ordering_list_starts[z_ordering_list] = UNSET;
			window_details[window_number].z_ordering_list_ends[z_ordering_list] = UNSET;
		}

		window_details[window_number].skip_me_this_frame--;
#endif
#endif

#ifdef RETRENGINE_DEBUG_VERSION_THE_LAST_THING_I_DID
		MAIN_debug_last_thing("Window skipped");
#endif
	}
	else
	{
		for (z_ordering_list = 0; z_ordering_list < z_ordering_list_size; z_ordering_list++)
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
				const int script_number = entity_pointer[ENT_SCRIPT_NUMBER];
				const int current_secondary_bitmap_number = entity_pointer[ENT_SECONDARY_SPRITE];
				const bool is_menu_intro_artifact_script = OUTPUT_is_menu_intro_artifact_script(script_number);
				const bool is_simple_translated_sprite =
					OUTPUT_is_simple_translated_sprite(
							draw_type,
							opengl_booleans,
							texture_combiner_available,
							current_secondary_bitmap_number);
				const bool is_hot_tunnel_candidate =
					(draw_type == DRAW_MODE_SPRITE) &&
					(opengl_booleans & OPENGL_BOOLEAN_ARBITRARY_PERSPECTIVE_QUAD) &&
					!OUTPUT_is_secondary_multitexture_active(texture_combiner_available, opengl_booleans, current_secondary_bitmap_number) &&
					OUTPUT_is_hot_tunnel_script(script_number);
				if (is_hot_tunnel_candidate)
				{
					output_frame_profile_hot_tunnel_entity_count++;
				}
				if ((draw_type == DRAW_MODE_SPRITE) && OUTPUT_is_profiled_text_script(script_number))
				{
					output_frame_profile_text_entity_count++;
				}
				if ((draw_type == DRAW_MODE_SPRITE) && OUTPUT_is_profiled_distorted_text_script(script_number))
				{
					output_frame_profile_distorted_text_entity_count++;
				}
				if (!is_hot_tunnel_candidate)
				{
					OUTPUT_flush_hot_tunnel_queue();
				}
				if (!is_simple_translated_sprite)
				{
					PLATFORM_RENDERER_flush_simple_quad_queue();
				}
				int effective_vertex_alpha = entity_pointer[ENT_OPENGL_VERTEX_ALPHA];
				const char *script_name = "UNSET";
				const bool is_laboratory_script = OUTPUT_is_laboratory_script(script_number);
				const int logo_section_script = OUTPUT_get_main_menu_logo_section_script();
				if (script_number == logo_section_script)
				{
					int parent_entity = entity_pointer[ENT_PARENT];
					if ((parent_entity >= 0) &&
							(parent_entity < MAX_ENTITIES) &&
							(entity[parent_entity][ENT_ALIVE] > DEAD))
					{
						effective_vertex_alpha = entity[parent_entity][ENT_OPENGL_VERTEX_ALPHA];
					}
				}
				if ((script_number >= 0) && (script_number < GPL_list_size("SCRIPTS")))
				{
					script_name = GPL_get_entry_name("SCRIPTS", script_number);
				}
				if (OUTPUT_should_skip_stale_menu_entity(current_entity))
				{
					if (!OUTPUT_window_queue_step(window_number, z_ordering_list, &current_entity, &queue_step_counter))
					{
						current_entity = UNSET;
					}
					continue;
				}
	if (OUTPUT_should_skip_menu_intro_artifact(entity_pointer[ENT_WINDOW_NUMBER], script_number))
	{
		static int menu_artifact_skip_log_count = 0;
		if (menu_artifact_skip_log_count < 200)
		{
			fprintf(stderr,
					"[MENU-ARTIFACT-SKIP %d] frame=%d ent=%d script=%d(%s) window=%d type=0x%x draw=%d flags=0x%x pos=(%d,%d)\n",
					menu_artifact_skip_log_count,
					frames_so_far,
					current_entity,
								script_number,
								script_name,
								entity_pointer[ENT_WINDOW_NUMBER],
								entity_pointer[ENT_ENTITY_TYPE],
								draw_type,
								opengl_booleans,
								entity_pointer[ENT_WORLD_X],
								entity_pointer[ENT_WORLD_Y]);
						menu_artifact_skip_log_count++;
					}

					if (!OUTPUT_window_queue_step(window_number, z_ordering_list, &current_entity, &queue_step_counter))
					{
						current_entity = UNSET;
					}
					continue;
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
							max_frame = ACTIVE_BMPS[sprite_index].sprite_count - 1;
						}

						snprintf(debug_text, sizeof(debug_text), "Entity %s is trying to access frame %i (max is %i)", GPL_get_entry_name("SCRIPTS", entity_pointer[ENT_SCRIPT_NUMBER]), frame_index, max_frame);
						MAIN_add_to_log(debug_text);
					}
				}
#endif

#ifdef RETRENGINE_DEBUG_VERSION_THE_LAST_THING_I_DID
				snprintf(debug_text, sizeof(debug_text), "Entity %i = %s ", current_entity, GPL_get_entry_name("SCRIPTS", entity_pointer[ENT_SCRIPT_NUMBER]));
				MAIN_debug_last_thing(debug_text);
#endif

#ifdef RETRENGINE_DEBUG_VERSION_THE_LAST_THING_I_DID
				snprintf(debug_text, sizeof(debug_text), "Draw Type = %i", draw_type);
				MAIN_debug_last_thing(debug_text);

				MAIN_debug_last_thing("About to check multitexture sprite...");
#endif

				if (texture_combiner_available)
				{
					secondary_bitmap_number = entity_pointer[ENT_SECONDARY_SPRITE];
					if (OUTPUT_is_secondary_multitexture_active(texture_combiner_available, opengl_booleans, secondary_bitmap_number))
					{
						PLATFORM_RENDERER_bind_secondary_texture(ACTIVE_BMPS[secondary_bitmap_number].texture_handle);
#ifdef RETRENGINE_DEBUG_VERSION_THE_LAST_THING_I_DID
						MAIN_debug_last_thing("About to set multi-texture sprite...");
#endif

#ifdef RETRENGINE_DEBUG_VERSION_THE_LAST_THING_I_DID
						MAIN_debug_last_thing("Set multitexture sprite...");
#endif
					}
					else if (old_secondary_bitmap_number != UNSET)
					{
						PLATFORM_RENDERER_bind_secondary_texture(0);
						PLATFORM_RENDERER_set_combiner_modulate_primary();
#ifdef RETRENGINE_DEBUG_VERSION_THE_LAST_THING_I_DID
						MAIN_debug_last_thing("About to reset multitexture sprite...");
#endif

#ifdef RETRENGINE_DEBUG_VERSION_THE_LAST_THING_I_DID
						MAIN_debug_last_thing("Reset multitexture sprite...");
#endif
					}
					old_secondary_bitmap_number = secondary_bitmap_number;
				}

#ifdef RETRENGINE_DEBUG_VERSION_THE_LAST_THING_I_DID
				MAIN_debug_last_thing("About to bind texture sprite...");
#endif

				bitmap_number = entity_pointer[ENT_SPRITE];
				bool bitmap_changed = false;
				const bool draw_mode_requires_bitmap = OUTPUT_draw_mode_requires_bitmap(draw_type);
				const char *bitmap_name = NULL;
				bool is_wiz_and_nifta_bitmap = false;
				if (draw_mode_requires_bitmap && !OUTPUT_is_valid_bitmap_index(bitmap_number))
				{
					if (!OUTPUT_window_queue_step(window_number, z_ordering_list, &current_entity, &queue_step_counter))
					{
						current_entity = UNSET;
					}
					continue;
				}
				if ((bitmap_number != old_bitmap_number) && (bitmap_number != UNSET))
				{
					bitmap_changed = true;
				}
				if (OUTPUT_is_valid_bitmap_index(bitmap_number))
				{
					/*
					 * Do not infer the live renderer binding from old_bitmap_number.
					 * Several GLES2 submission paths can change the currently bound
					 * texture between entities. Also do not rely on the current
					 * draw-mode whitelist to decide whether a texture bind is needed:
					 * some textured paths still flow through branches that are not
					 * classified as bitmap-backed here.
					 */
					PLATFORM_RENDERER_bind_texture(ACTIVE_BMPS[bitmap_number].texture_handle);
				}
				if ((bitmap_number >= 0) && (bitmap_number < GPL_list_size("BITMAPS")))
				{
					bitmap_name = GPL_get_entry_name("BITMAPS", bitmap_number);
					is_wiz_and_nifta_bitmap =
						(bitmap_name != NULL) &&
						(strcmp(bitmap_name, "wiz_and_nifta[arb]") == 0);
				}
				const bool is_lab_focus_script =
					(strcmp(script_name, "LAB_SHADOW") == 0) ||
					(strcmp(script_name, "LAB_CAULDRON_GLOW") == 0) ||
					(strcmp(script_name, "LAB_NIFTA_BOWL") == 0);
				PLATFORM_RENDERER_set_debug_draw_context(
						current_entity,
						script_number,
						draw_type,
						window_number,
						script_name,
						bitmap_name);

				if (bitmap_changed || (opengl_booleans != old_opengl_booleans))
				{
					OUTPUT_apply_texture_parameters_from_flags(opengl_booleans, old_opengl_booleans, bitmap_changed);
				}
				old_bitmap_number = bitmap_number;

				if (opengl_booleans != old_opengl_booleans)
				{
					if (!(opengl_booleans & OPENGL_BOOLEAN_ROTATE) && (old_opengl_booleans & OPENGL_BOOLEAN_ROTATE))
					{
						// ie, if the previous one was rotated and the new one ain't. Reset.

#ifdef RETRENGINE_DEBUG_VERSION_THE_LAST_THING_I_DID
						MAIN_debug_last_thing("Resetting Transform as previous entity was rotated and this one ain't.");
#endif

						PLATFORM_RENDERER_set_window_transform(left_window_transform_x, top_window_transform_y, total_scale_x, total_scale_y);
					}
					else if (!(opengl_booleans & OPENGL_BOOLEAN_SCALE) && (old_opengl_booleans & OPENGL_BOOLEAN_SCALE))
					{
						// ie, if the previous one was scaled and the new one ain't. Reset.

#ifdef RETRENGINE_DEBUG_VERSION_THE_LAST_THING_I_DID
						MAIN_debug_last_thing("Resetting Transform as previous entity was scaled and this one ain't.");
#endif

						PLATFORM_RENDERER_set_window_transform(left_window_transform_x, top_window_transform_y, total_scale_x, total_scale_y);
					}

					if (!(opengl_booleans & OPENGL_BOOLEAN_BLEND_EITHER) && (old_opengl_booleans & OPENGL_BOOLEAN_BLEND_ADD))
					{
						// Turn off additive blending

#ifdef RETRENGINE_DEBUG_VERSION_THE_LAST_THING_I_DID
						MAIN_debug_last_thing("Turn off blending.");
#endif

						PLATFORM_RENDERER_set_blend_enabled(false);
					}
					else if ((opengl_booleans & OPENGL_BOOLEAN_BLEND_ADD) && !(old_opengl_booleans & OPENGL_BOOLEAN_BLEND_ADD))
					{
						// Turn on additive blending

#ifdef RETRENGINE_DEBUG_VERSION_THE_LAST_THING_I_DID
						MAIN_debug_last_thing("Turn on additive blending.");
#endif

						PLATFORM_RENDERER_set_blend_enabled(true);
						PLATFORM_RENDERER_set_blend_mode_additive();
					}

					if (!(opengl_booleans & OPENGL_BOOLEAN_BLEND_EITHER) && (old_opengl_booleans & OPENGL_BOOLEAN_BLEND_MULTIPLY))
					{
						// Turn off multiplicative blending

#ifdef RETRENGINE_DEBUG_VERSION_THE_LAST_THING_I_DID
						MAIN_debug_last_thing("Turn off blending.");
#endif

						PLATFORM_RENDERER_set_blend_enabled(false);
					}
					else if ((opengl_booleans & OPENGL_BOOLEAN_BLEND_MULTIPLY) && !(old_opengl_booleans & OPENGL_BOOLEAN_BLEND_MULTIPLY))
					{
						// Turn on multiplicative blending

#ifdef RETRENGINE_DEBUG_VERSION_THE_LAST_THING_I_DID
						MAIN_debug_last_thing("Turn on multiplicative blending.");
#endif

						PLATFORM_RENDERER_set_blend_enabled(true);
						PLATFORM_RENDERER_set_blend_mode_multiply();
					}

					if (!(opengl_booleans & OPENGL_BOOLEAN_BLEND_EITHER) && (old_opengl_booleans & OPENGL_BOOLEAN_BLEND_SUBTRACT))
					{
						// Turn off subtractive blending

#ifdef RETRENGINE_DEBUG_VERSION_THE_LAST_THING_I_DID
						MAIN_debug_last_thing("Turn off blending.");
#endif

						PLATFORM_RENDERER_set_blend_enabled(false);
					}
					else if ((opengl_booleans & OPENGL_BOOLEAN_BLEND_SUBTRACT) && !(old_opengl_booleans & OPENGL_BOOLEAN_BLEND_SUBTRACT))
					{
						// Turn on subtractive blending

#ifdef RETRENGINE_DEBUG_VERSION_THE_LAST_THING_I_DID
						MAIN_debug_last_thing("Turn on subtractive blending.");
#endif

						PLATFORM_RENDERER_set_blend_enabled(true);
						PLATFORM_RENDERER_set_blend_mode_subtractive();
					}

					if (!(opengl_booleans & OPENGL_BOOLEAN_MASKED) && (old_opengl_booleans & OPENGL_BOOLEAN_MASKED))
					{
						// Turn off alpha test (masking)
						PLATFORM_RENDERER_set_texture_masked(false);

#ifdef RETRENGINE_DEBUG_VERSION_THE_LAST_THING_I_DID
						MAIN_debug_last_thing("Turn off masking.");
#endif
					}
					else if ((opengl_booleans & OPENGL_BOOLEAN_MASKED) && !(old_opengl_booleans & OPENGL_BOOLEAN_MASKED))
					{
						// Turn on alpha test (masking)
						PLATFORM_RENDERER_set_texture_masked(true);

#ifdef RETRENGINE_DEBUG_VERSION_THE_LAST_THING_I_DID
						MAIN_debug_last_thing("Turn on masking.");
#endif
					}

					if (secondary_colour_available)
					{
						if (!(opengl_booleans & OPENGL_BOOLEAN_VERTEX_SECONDARY_COLOUR) && (old_opengl_booleans & OPENGL_BOOLEAN_VERTEX_SECONDARY_COLOUR))
						{
							// Reset secondary colour to black and turn it off.

#ifdef RETRENGINE_DEBUG_VERSION_THE_LAST_THING_I_DID
							MAIN_debug_last_thing("Turn off secondary colour.");
#endif
						}
						else if ((opengl_booleans & OPENGL_BOOLEAN_VERTEX_SECONDARY_COLOUR) && !(old_opengl_booleans & OPENGL_BOOLEAN_VERTEX_SECONDARY_COLOUR))
						{
							// Reset secondary colour to black and turn it on.

#ifdef RETRENGINE_DEBUG_VERSION_THE_LAST_THING_I_DID
							MAIN_debug_last_thing("Turn on secondary colour.");
#endif
						}
					}

				if ((opengl_booleans & OPENGL_BOOLEAN_VERTEX_SECONDARY_COLOUR) && !(old_opengl_booleans & OPENGL_BOOLEAN_VERTEX_SECONDARY_COLOUR))
				{
					// Turn on secondary colouring

#ifdef RETRENGINE_DEBUG_VERSION_THE_LAST_THING_I_DID
						MAIN_debug_last_thing("Turn on secondary colour.");
#endif
					}

					if (!(opengl_booleans & OPENGL_BOOLEAN_VERTEX_COLOUR) && (old_opengl_booleans & OPENGL_BOOLEAN_VERTEX_COLOUR))
					{
						// Reset vertex colour to white

#ifdef RETRENGINE_DEBUG_VERSION_THE_LAST_THING_I_DID
						MAIN_debug_last_thing("Reset vertex colour to white.");
#endif

						PLATFORM_RENDERER_set_colour4f(1.0f, 1.0f, 1.0f, 1.0f);
					}

					if (!(opengl_booleans & OPENGL_BOOLEAN_VERTEX_COLOUR_ALPHA) && (old_opengl_booleans & OPENGL_BOOLEAN_VERTEX_COLOUR_ALPHA))
					{
						// Reset vertex colour to white

#ifdef RETRENGINE_DEBUG_VERSION_THE_LAST_THING_I_DID
						MAIN_debug_last_thing("Reset vertex colour to white.");
#endif

						PLATFORM_RENDERER_set_colour4f(1.0f, 1.0f, 1.0f, 1.0f);
					}
				}

				if (secondary_colour_available)
				{
					if (opengl_booleans & OPENGL_BOOLEAN_VERTEX_SECONDARY_COLOUR)
					{
#ifdef RETRENGINE_DEBUG_VERSION_THE_LAST_THING_I_DID
						MAIN_debug_last_thing("Set secondary vertex colour.");
#endif
					}
				}

				if (opengl_booleans & OPENGL_BOOLEAN_VERTEX_COLOUR)
				{
#ifdef RETRENGINE_DEBUG_VERSION_THE_LAST_THING_I_DID
					MAIN_debug_last_thing("Set primary vertex colour.");
#endif

					PLATFORM_RENDERER_set_colour4f(float(entity_pointer[ENT_OPENGL_VERTEX_RED]) / 256.0f, float(entity_pointer[ENT_OPENGL_VERTEX_GREEN]) / 256.0f, float(entity_pointer[ENT_OPENGL_VERTEX_BLUE]) / 256.0f, 1.0f);
				}

				if (opengl_booleans & OPENGL_BOOLEAN_VERTEX_COLOUR_ALPHA)
				{
#ifdef RETRENGINE_DEBUG_VERSION_THE_LAST_THING_I_DID
					MAIN_debug_last_thing("Set primary vertex colour and alpha.");
#endif
					/*
					 * SDL port: VERTEX_COLOUR_ALPHA is often alpha-only on textured sprites
					 * (rgb left at reset default 0,0,0). For those, substitute white rgb to
					 * avoid unintentionally black-multiplying the texture. Do not apply that
					 * substitution to non-textured draw modes (e.g. solid rectangles), where
					 * explicit black is a valid authored colour.
					 */
					const bool draw_mode_uses_texture_rgb = OUTPUT_draw_mode_requires_bitmap(draw_type);
					const bool vcalpha_rgb_is_default =
						draw_mode_uses_texture_rgb &&
						(entity_pointer[ENT_OPENGL_VERTEX_RED]   == 0) &&
						(entity_pointer[ENT_OPENGL_VERTEX_GREEN] == 0) &&
						(entity_pointer[ENT_OPENGL_VERTEX_BLUE]  == 0) &&
						!(opengl_booleans & OPENGL_BOOLEAN_VERTEX_COLOUR);
					const float vcalpha_r = vcalpha_rgb_is_default ? 1.0f : float(entity_pointer[ENT_OPENGL_VERTEX_RED])   / 256.0f;
					const float vcalpha_g = vcalpha_rgb_is_default ? 1.0f : float(entity_pointer[ENT_OPENGL_VERTEX_GREEN]) / 256.0f;
					const float vcalpha_b = vcalpha_rgb_is_default ? 1.0f : float(entity_pointer[ENT_OPENGL_VERTEX_BLUE])  / 256.0f;
					PLATFORM_RENDERER_set_colour4f(vcalpha_r, vcalpha_g, vcalpha_b, float(effective_vertex_alpha) / 256.0f);
				}

				/*
				 * SDL port: OpenGL's glColor4f state persisted across draw calls so the
				 * renderer would carry the last-set colour into the next entity.  With
				 * SDL_SetTextureColorMod / AlphaMod the same stale value is used and,
				 * if a previous entity left colour=(0,0,0,0), every subsequent untinted
				 * entity is invisible.  When no per-entity tinting flag is active,
				 * explicitly reset to opaque white so untinted sprites render normally.
				 */
				if (!(opengl_booleans & (OPENGL_BOOLEAN_VERTEX_COLOUR | OPENGL_BOOLEAN_VERTEX_COLOUR_ALPHA)))
				{
					PLATFORM_RENDERER_set_colour4f(1.0f, 1.0f, 1.0f, 1.0f);
				}
				else
				{
					/*
					 * Diagnostic: log first few times an entity sets a zero colour via
					 * the vertex-colour flags, so we can see which scripts cause it.
					 */
				}

#ifdef RETRENGINE_DEBUG_VERSION_THE_LAST_THING_I_DID
				MAIN_debug_last_thing("Entering main switch...");
#endif

#ifdef RETRENGINE_DEBUG_VERSION
				if ((entity_pointer[ENT_OPENGL_SCALE_X] == 0) || (entity_pointer[ENT_OPENGL_SCALE_Y] == 0))
				{
					snprintf(debug_text, sizeof(debug_text), "Tiny scale error in entity '%s'!", GPL_get_entry_name("SCRIPTS", entity_pointer[ENT_SCRIPT_NUMBER]));
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

						sp = &ACTIVE_BMPS[bitmap_number].sprite_list[frame_number];
						interp_sp = &ACTIVE_BMPS[bitmap_number].sprite_list[interp_frame_number];

						interp_x = float(entity_pointer[ENT_INTERPOLATION_X_PERCENTAGE]) / 10000.0f;
						interp_y = float(entity_pointer[ENT_INTERPOLATION_Y_PERCENTAGE]) / 10000.0f;

						u1 = sp->u1;
						u2 = sp->u2;
						v1 = sp->v1;
						v2 = sp->v2;

						interp_u1 = interp_sp->u1;
						interp_v1 = interp_sp->v1;
						interp_u2 = interp_sp->u2;
						interp_v2 = interp_sp->v2;

						u1 = MATH_lerp(u1, interp_u1, interp_x);
						u2 = MATH_lerp(u2, interp_u2, interp_x);
						v1 = MATH_lerp(v1, interp_v1, interp_y);
						v2 = MATH_lerp(v2, interp_v2, interp_y);
					}
					else
					{
						sp = &ACTIVE_BMPS[bitmap_number].sprite_list[frame_number];

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
						down = -(sp->height - entity_pointer[ENT_OVERRIDE_PIVOT_Y]);
					}
					else
					{
						left = -sp->pivot_x;
						right = sp->width - sp->pivot_x;
						up = sp->pivot_y;
						down = -(sp->height - sp->pivot_y);
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

					if (is_wiz_and_nifta_bitmap)
					{
						static int wiz_bitmap_draw_log_count = 0;
						if (wiz_bitmap_draw_log_count < 200)
						{
							fprintf(stderr,
									"[WIZ-ATLAS-DRAW %d] frame=%d ent=%d script=%d(%s) bitmap=%s frame_no=%d interp=%d window=%d type=0x%x draw=%d flags=0x%x world=(%d,%d) local=(%d,%d) box=(%.1f,%.1f,%.1f,%.1f) uv=(%.4f,%.4f)-(%.4f,%.4f)\n",
									wiz_bitmap_draw_log_count,
									frames_so_far,
									current_entity,
									script_number,
									script_name,
									bitmap_name,
									frame_number,
									interp_frame_number,
									window_number,
									entity_pointer[ENT_ENTITY_TYPE],
									draw_type,
									opengl_booleans,
									entity_pointer[ENT_WORLD_X],
									entity_pointer[ENT_WORLD_Y],
									x,
									y,
									left,
									right,
									up,
									down,
									u1,
									v1,
									u2,
									v2);
							wiz_bitmap_draw_log_count++;
						}
					}
					/*
					 * In pure SDL mode, prefer the explicit window-sprite path for
					 * standard sprites. It does not depend on bound-texture state and
					 * has been more robust for transformed/animated entities.
					 */

					if (is_simple_translated_sprite)
					{
						PLATFORM_RENDERER_draw_bound_textured_quad_translated(
								x,
								-y,
								left,
								right,
								up,
								down,
								u1,
								v1,
								u2,
								v2);
						break;
					}

					PLATFORM_RENDERER_translatef(x, -y, 0.0);

					if (opengl_booleans & OPENGL_BOOLEAN_SECONDARY_SCALE)
					{
						scale_x_2 = float(entity_pointer[ENT_OPENGL_SECONDARY_SCALE_X]) / 10000.0f;
						scale_y_2 = float(entity_pointer[ENT_OPENGL_SECONDARY_SCALE_Y]) / 10000.0f;
						PLATFORM_RENDERER_scalef(scale_x_2, scale_y_2, 1.0f);
					}

					if (opengl_booleans & OPENGL_BOOLEAN_ROTATE)
					{
						rotate_angle = float(entity_pointer[ENT_OPENGL_ANGLE]) / 100.0f;
						PLATFORM_RENDERER_rotatef(-rotate_angle, 0.0f, 0.0f, 1.0f);
					}

					if (opengl_booleans & OPENGL_BOOLEAN_SCALE)
					{
						scale_x = float(entity_pointer[ENT_OPENGL_SCALE_X]) / 10000.0f;
						scale_y = float(entity_pointer[ENT_OPENGL_SCALE_Y]) / 10000.0f;
						PLATFORM_RENDERER_scalef(scale_x, scale_y, 1.0f);
					}

					if (opengl_booleans & OPENGL_BOOLEAN_SECONDARY_ROTATE)
					{
						rotate_angle_2 = float(entity_pointer[ENT_OPENGL_SECONDARY_ANGLE]) / 100.0f;
						PLATFORM_RENDERER_rotatef(-rotate_angle_2, 0.0f, 0.0f, 1.0f);
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

					if (!OUTPUT_is_secondary_multitexture_active(texture_combiner_available, opengl_booleans, secondary_bitmap_number) &&
							!(opengl_booleans & OPENGL_BOOLEAN_INDIVIDUAL_VERTEX_COLOUR_ALPHA) &&
							!(opengl_booleans & (OPENGL_BOOLEAN_SECONDARY_SCALE | OPENGL_BOOLEAN_ROTATE_CLOCKWISE)) &&
							((opengl_booleans & (OPENGL_BOOLEAN_ARBITRARY_QUAD | OPENGL_BOOLEAN_ARBITRARY_PERSPECTIVE_QUAD)) == 0))
					{
						int sprite_mod_r = 255;
						int sprite_mod_g = 255;
						int sprite_mod_b = 255;
						int sprite_mod_a = 255;
						float sprite_scale_x = 1.0f;
						float sprite_scale_y = 1.0f;
						float sprite_rotation_degrees = 0.0f;

						if (opengl_booleans & OPENGL_BOOLEAN_VERTEX_COLOUR)
						{
							sprite_mod_r = entity_pointer[ENT_OPENGL_VERTEX_RED];
							sprite_mod_g = entity_pointer[ENT_OPENGL_VERTEX_GREEN];
							sprite_mod_b = entity_pointer[ENT_OPENGL_VERTEX_BLUE];
						}

						if (opengl_booleans & OPENGL_BOOLEAN_VERTEX_COLOUR_ALPHA)
						{
							const bool draw_mode_uses_texture_rgb = OUTPUT_draw_mode_requires_bitmap(draw_type);
							const bool vcalpha_rgb_is_default =
								draw_mode_uses_texture_rgb &&
								(entity_pointer[ENT_OPENGL_VERTEX_RED] == 0) &&
								(entity_pointer[ENT_OPENGL_VERTEX_GREEN] == 0) &&
								(entity_pointer[ENT_OPENGL_VERTEX_BLUE] == 0) &&
								!(opengl_booleans & OPENGL_BOOLEAN_VERTEX_COLOUR);

							sprite_mod_r = vcalpha_rgb_is_default ? 255 : entity_pointer[ENT_OPENGL_VERTEX_RED];
							sprite_mod_g = vcalpha_rgb_is_default ? 255 : entity_pointer[ENT_OPENGL_VERTEX_GREEN];
							sprite_mod_b = vcalpha_rgb_is_default ? 255 : entity_pointer[ENT_OPENGL_VERTEX_BLUE];
							sprite_mod_a = effective_vertex_alpha;
						}

						if (opengl_booleans & OPENGL_BOOLEAN_SCALE)
						{
							sprite_scale_x = float(entity_pointer[ENT_OPENGL_SCALE_X]) / 10000.0f;
							sprite_scale_y = float(entity_pointer[ENT_OPENGL_SCALE_Y]) / 10000.0f;
						}

						if (opengl_booleans & OPENGL_BOOLEAN_ROTATE)
						{
							sprite_rotation_degrees = -float(entity_pointer[ENT_OPENGL_ANGLE]) / 100.0f;
						}
						if (opengl_booleans & OPENGL_BOOLEAN_SECONDARY_ROTATE)
						{
							sprite_rotation_degrees -= float(entity_pointer[ENT_OPENGL_SECONDARY_ANGLE]) / 100.0f;
						}
						if (u2 > 1.0f)
						{
							/* UV range extends past the texture boundary (u2 > 1.0).
							 * The INTERPOLATED path uses GL_REPEAT-style wrapping where the
							 * second frame is placed at u=[1..2].  SDL has no such wrapping,
							 * so split into two quads: the right source portion fills the
							 * left destination, and the wrapped left source fills the right. */
							const float u_span = u2 - u1;
							const float sprite_width = right - left;
							const float split_frac = (1.0f - u1) / u_span;
							const float split_x = left + split_frac * sprite_width;
							PLATFORM_RENDERER_draw_sdl_window_sprite(
									ACTIVE_BMPS[bitmap_number].texture_handle,
									sprite_mod_r,
									sprite_mod_g,
									sprite_mod_b,
									sprite_mod_a,
									x,
									y,
									left,
									split_x,
									up,
									down,
									u1,
									v1,
									1.0f,
									v2,
									left_window_transform_x,
									top_window_transform_y,
									total_scale_x,
									total_scale_y,
									sprite_scale_x,
									sprite_scale_y,
									sprite_rotation_degrees,
									false,
									false);
							PLATFORM_RENDERER_draw_sdl_window_sprite(
									ACTIVE_BMPS[bitmap_number].texture_handle,
									sprite_mod_r,
									sprite_mod_g,
									sprite_mod_b,
									sprite_mod_a,
									x,
									y,
									split_x,
									right,
									up,
									down,
									0.0f,
									v1,
									u2 - 1.0f,
									v2,
									left_window_transform_x,
									top_window_transform_y,
									total_scale_x,
									total_scale_y,
									sprite_scale_x,
									sprite_scale_y,
									sprite_rotation_degrees,
									false,
									false);
						}
						else
						{
							PLATFORM_RENDERER_draw_sdl_window_sprite(
									ACTIVE_BMPS[bitmap_number].texture_handle,
									sprite_mod_r,
									sprite_mod_g,
									sprite_mod_b,
									sprite_mod_a,
									x,
									y,
									left,
									right,
									up,
									down,
									u1,
									v1,
									u2,
									v2,
									left_window_transform_x,
									top_window_transform_y,
									total_scale_x,
									total_scale_y,
									sprite_scale_x,
									sprite_scale_y,
									sprite_rotation_degrees,
									false,
									false);
						}
						/* draw_sdl_window_sprite bakes position/scale/rotation into the SDL
						 * draw call, but translatef(x,-y) was called unconditionally above.
						 * Reset the transform absolutely so subsequent entities are not displaced. */
						PLATFORM_RENDERER_set_window_transform(left_window_transform_x, top_window_transform_y, total_scale_x, total_scale_y);
						break;
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

							secondary_sp = &ACTIVE_BMPS[secondary_bitmap_number].sprite_list[secondary_frame_number];
							secondary_interp_sp = &ACTIVE_BMPS[secondary_bitmap_number].sprite_list[secondary_interp_frame_number];

							interp_x = float(entity_pointer[ENT_SECONDARY_INTERPOLATION_X_PERCENTAGE]) / 10000.0f;
							interp_y = float(entity_pointer[ENT_SECONDARY_INTERPOLATION_Y_PERCENTAGE]) / 10000.0f;

							secondary_u1 = secondary_sp->u1;
							secondary_u2 = secondary_sp->u2;
							secondary_v1 = secondary_sp->v1;
							secondary_v2 = secondary_sp->v2;

							interp_u1 = secondary_interp_sp->u1;
							interp_v1 = secondary_interp_sp->v1;
							interp_u2 = secondary_interp_sp->u2;
							interp_v2 = secondary_interp_sp->v2;

							secondary_u1 = MATH_lerp(secondary_u1, interp_u1, interp_x);
							secondary_u2 = MATH_lerp(secondary_u2, interp_u2, interp_x);
							secondary_v1 = MATH_lerp(secondary_v1, interp_v1, interp_y);
							secondary_v2 = MATH_lerp(secondary_v2, interp_v2, interp_y);
						}
						else
						{
							secondary_frame_number = entity_pointer[ENT_SECONDARY_CURRENT_FRAME];
							if (!OUTPUT_is_valid_sprite_frame(secondary_bitmap_number, secondary_frame_number))
							{
								secondary_frame_number = 0;
							}

							secondary_sp = &ACTIVE_BMPS[secondary_bitmap_number].sprite_list[secondary_frame_number];

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
								const float quad_x[4] = {right, left, left, right};
								const float quad_y[4] = {up, up, down, down};
								const float quad_u0[4] = {u1, u1, u2, u2};
								const float quad_v0[4] = {v1, v2, v2, v1};
								const float quad_u1[4] = {secondary_u1, secondary_u1, secondary_u2, secondary_u2};
								const float quad_v1[4] = {secondary_v1, secondary_v2, secondary_v2, secondary_v1};
								PLATFORM_RENDERER_draw_bound_multitextured_quad_array(quad_x, quad_y, quad_u0, quad_v0, quad_u1, quad_v1);
							}
							else
							{
								const float quad_x[4] = {left, left, right, right};
								const float quad_y[4] = {up, down, down, up};
								const float quad_u0[4] = {u1, u1, u2, u2};
								const float quad_v0[4] = {v1, v2, v2, v1};
								const float quad_u1[4] = {secondary_u1, secondary_u1, secondary_u2, secondary_u2};
								const float quad_v1[4] = {secondary_v1, secondary_v2, secondary_v2, secondary_v1};
								PLATFORM_RENDERER_draw_bound_multitextured_quad_array(quad_x, quad_y, quad_u0, quad_v0, quad_u1, quad_v1);
							}
						}
						else if (opengl_booleans & OPENGL_BOOLEAN_ARBITRARY_QUAD)
						{
							aq_pointer = &arbitrary_quads[current_entity];

							if (opengl_booleans & OPENGL_BOOLEAN_ROTATE_CLOCKWISE)
							{
								const float quad_x[4] = {(float)aq_pointer->x[1], (float)aq_pointer->x[0], (float)aq_pointer->x[2], (float)aq_pointer->x[3]};
								const float quad_y[4] = {(float)aq_pointer->y[1], (float)aq_pointer->y[0], (float)aq_pointer->y[2], (float)aq_pointer->y[3]};
								const float quad_u0[4] = {u1, u1, u2, u2};
								const float quad_v0[4] = {v1, v2, v2, v1};
								const float quad_u1[4] = {secondary_u1, secondary_u1, secondary_u2, secondary_u2};
								const float quad_v1[4] = {secondary_v1, secondary_v2, secondary_v2, secondary_v1};
								PLATFORM_RENDERER_draw_bound_multitextured_quad_array(quad_x, quad_y, quad_u0, quad_v0, quad_u1, quad_v1);
							}
							else
							{
								const float quad_x[4] = {(float)aq_pointer->x[0], (float)aq_pointer->x[2], (float)aq_pointer->x[3], (float)aq_pointer->x[1]};
								const float quad_y[4] = {(float)aq_pointer->y[0], (float)aq_pointer->y[2], (float)aq_pointer->y[3], (float)aq_pointer->y[1]};
								const float quad_u0[4] = {u1, u1, u2, u2};
								const float quad_v0[4] = {v1, v1, v2, v1};
								const float quad_u1[4] = {secondary_u1, secondary_u1, secondary_u2, secondary_u2};
								const float quad_v1[4] = {secondary_v1, secondary_v1, secondary_v2, secondary_v1};
								PLATFORM_RENDERER_draw_bound_multitextured_quad_array(quad_x, quad_y, quad_u0, quad_v0, quad_u1, quad_v1);
							}
						}
						else if (opengl_booleans & OPENGL_BOOLEAN_ARBITRARY_PERSPECTIVE_QUAD)
						{
							aq_pointer = &arbitrary_quads[current_entity];
							line_length_ratio = aq_pointer->line_lengths[0] / aq_pointer->line_lengths[2];
							PLATFORM_RENDERER_draw_bound_multitextured_perspective_quad(
									(float)aq_pointer->x[0], (float)aq_pointer->y[0],
									(float)aq_pointer->x[2], (float)aq_pointer->y[2],
									(float)aq_pointer->x[3], (float)aq_pointer->y[3],
									(float)aq_pointer->x[1], (float)aq_pointer->y[1],
									u1, v1, u2, v2,
									secondary_u1, secondary_v1, secondary_u2, secondary_v2,
									line_length_ratio);
						}
					}
					else
					{
						OUTPUT_apply_texture_parameters_from_flags(opengl_booleans, old_opengl_booleans);

						if (opengl_booleans & OPENGL_BOOLEAN_INDIVIDUAL_VERTEX_COLOUR_ALPHA)
						{
							avc_pointer = &arbitrary_vertex_colours[current_entity];

							if ((opengl_booleans & (OPENGL_BOOLEAN_ARBITRARY_QUAD | OPENGL_BOOLEAN_ARBITRARY_PERSPECTIVE_QUAD)) == 0)
							{
								if (opengl_booleans & OPENGL_BOOLEAN_ROTATE_CLOCKWISE)
								{
									const float quad_x[4] = {right, left, left, right};
									const float quad_y[4] = {up, up, down, down};
									const float quad_u[4] = {u1, u1, u2, u2};
									const float quad_v[4] = {v1, v2, v2, v1};
									const float quad_r[4] = {avc_pointer->red[1], avc_pointer->red[0], avc_pointer->red[2], avc_pointer->red[3]};
									const float quad_g[4] = {avc_pointer->green[1], avc_pointer->green[0], avc_pointer->green[2], avc_pointer->green[3]};
									const float quad_b[4] = {avc_pointer->blue[1], avc_pointer->blue[0], avc_pointer->blue[2], avc_pointer->blue[3]};
									const float quad_a[4] = {avc_pointer->alpha[1], avc_pointer->alpha[0], avc_pointer->alpha[2], avc_pointer->alpha[3]};
									PLATFORM_RENDERER_draw_bound_coloured_textured_quad_array(quad_x, quad_y, quad_u, quad_v, quad_r, quad_g, quad_b, quad_a);
								}
								else
								{
									const float quad_x[4] = {left, left, right, right};
									const float quad_y[4] = {up, down, down, up};
									const float quad_u[4] = {u1, u1, u2, u2};
									const float quad_v[4] = {v1, v2, v2, v1};
									const float quad_r[4] = {avc_pointer->red[0], avc_pointer->red[2], avc_pointer->red[3], avc_pointer->red[1]};
									const float quad_g[4] = {avc_pointer->green[0], avc_pointer->green[2], avc_pointer->green[3], avc_pointer->green[1]};
									const float quad_b[4] = {avc_pointer->blue[0], avc_pointer->blue[2], avc_pointer->blue[3], avc_pointer->blue[1]};
									const float quad_a[4] = {avc_pointer->alpha[0], avc_pointer->alpha[2], avc_pointer->alpha[3], avc_pointer->alpha[1]};
									PLATFORM_RENDERER_draw_bound_coloured_textured_quad_array(quad_x, quad_y, quad_u, quad_v, quad_r, quad_g, quad_b, quad_a);
								}
							}
							else if (opengl_booleans & OPENGL_BOOLEAN_ARBITRARY_QUAD)
							{
								aq_pointer = &arbitrary_quads[current_entity];

								if (opengl_booleans & OPENGL_BOOLEAN_ROTATE_CLOCKWISE)
								{
									const float quad_x[4] = {(float)aq_pointer->x[1], (float)aq_pointer->x[0], (float)aq_pointer->x[2], (float)aq_pointer->x[3]};
									const float quad_y[4] = {(float)aq_pointer->y[1], (float)aq_pointer->y[0], (float)aq_pointer->y[2], (float)aq_pointer->y[3]};
									const float quad_u[4] = {u1, u1, u2, u2};
									const float quad_v[4] = {v1, v2, v2, v1};
									const float quad_r[4] = {avc_pointer->red[1], avc_pointer->red[0], avc_pointer->red[2], avc_pointer->red[3]};
									const float quad_g[4] = {avc_pointer->green[1], avc_pointer->green[0], avc_pointer->green[2], avc_pointer->green[3]};
									const float quad_b[4] = {avc_pointer->blue[1], avc_pointer->blue[0], avc_pointer->blue[2], avc_pointer->blue[3]};
									const float quad_a[4] = {avc_pointer->alpha[1], avc_pointer->alpha[0], avc_pointer->alpha[2], avc_pointer->alpha[3]};
									PLATFORM_RENDERER_draw_bound_coloured_textured_quad_array(quad_x, quad_y, quad_u, quad_v, quad_r, quad_g, quad_b, quad_a);
								}
								else
								{
									const float quad_x[4] = {(float)aq_pointer->x[0], (float)aq_pointer->x[2], (float)aq_pointer->x[3], (float)aq_pointer->x[1]};
									const float quad_y[4] = {(float)aq_pointer->y[0], (float)aq_pointer->y[2], (float)aq_pointer->y[3], (float)aq_pointer->y[1]};
									const float quad_u[4] = {u1, u1, u2, u2};
									const float quad_v[4] = {v1, v2, v2, v1};
									const float quad_r[4] = {avc_pointer->red[0], avc_pointer->red[2], avc_pointer->red[3], avc_pointer->red[1]};
									const float quad_g[4] = {avc_pointer->green[0], avc_pointer->green[2], avc_pointer->green[3], avc_pointer->green[1]};
									const float quad_b[4] = {avc_pointer->blue[0], avc_pointer->blue[2], avc_pointer->blue[3], avc_pointer->blue[1]};
									const float quad_a[4] = {avc_pointer->alpha[0], avc_pointer->alpha[2], avc_pointer->alpha[3], avc_pointer->alpha[1]};
									PLATFORM_RENDERER_draw_bound_coloured_textured_quad_array(quad_x, quad_y, quad_u, quad_v, quad_r, quad_g, quad_b, quad_a);
								}
							}
							else if (opengl_booleans & OPENGL_BOOLEAN_ARBITRARY_PERSPECTIVE_QUAD)
							{
								aq_pointer = &arbitrary_quads[current_entity];

								line_length_ratio = aq_pointer->line_lengths[0] / aq_pointer->line_lengths[2];
								const float quad_r[4] = {avc_pointer->red[0], avc_pointer->red[2], avc_pointer->red[3], avc_pointer->red[1]};
								const float quad_g[4] = {avc_pointer->green[0], avc_pointer->green[2], avc_pointer->green[3], avc_pointer->green[1]};
								const float quad_b[4] = {avc_pointer->blue[0], avc_pointer->blue[2], avc_pointer->blue[3], avc_pointer->blue[1]};
								const float quad_a[4] = {avc_pointer->alpha[0], avc_pointer->alpha[2], avc_pointer->alpha[3], avc_pointer->alpha[1]};
								if (is_hot_tunnel_candidate)
								{
									OUTPUT_queue_hot_tunnel_coloured_perspective_quad(
											aq_pointer->x[0], aq_pointer->y[0],
											aq_pointer->x[2], aq_pointer->y[2],
											aq_pointer->x[3], aq_pointer->y[3],
											aq_pointer->x[1], aq_pointer->y[1],
											u1, v1, u2, v2, line_length_ratio,
											quad_r, quad_g, quad_b, quad_a);
								}
								else
								{
									PLATFORM_RENDERER_draw_bound_coloured_perspective_textured_quad(
											aq_pointer->x[0], aq_pointer->y[0],
											aq_pointer->x[2], aq_pointer->y[2],
											aq_pointer->x[3], aq_pointer->y[3],
											aq_pointer->x[1], aq_pointer->y[1],
											u1, v1, u2, v2, line_length_ratio,
											quad_r, quad_g, quad_b, quad_a);
								}
							}
						}
						else
						{
							if ((opengl_booleans & (OPENGL_BOOLEAN_ARBITRARY_QUAD | OPENGL_BOOLEAN_ARBITRARY_PERSPECTIVE_QUAD)) == 0)
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
								if (is_hot_tunnel_candidate)
								{
									OUTPUT_queue_hot_tunnel_perspective_quad(
											aq_pointer->x[0], aq_pointer->y[0],
											aq_pointer->x[2], aq_pointer->y[2],
											aq_pointer->x[3], aq_pointer->y[3],
											aq_pointer->x[1], aq_pointer->y[1],
											u1, v1, u2, v2, line_length_ratio);
								}
								else
								{
									PLATFORM_RENDERER_draw_bound_perspective_textured_quad(
											aq_pointer->x[0], aq_pointer->y[0],
											aq_pointer->x[2], aq_pointer->y[2],
											aq_pointer->x[3], aq_pointer->y[3],
											aq_pointer->x[1], aq_pointer->y[1],
											u1, v1, u2, v2, line_length_ratio);
								}
							}
						}
					}

					if (opengl_booleans & OPENGL_BOOLEAN_SECONDARY_ROTATE)
					{
						PLATFORM_RENDERER_rotatef(rotate_angle_2, 0.0f, 0.0f, 1.0f);
					}

					if (opengl_booleans & OPENGL_BOOLEAN_SCALE)
					{
						PLATFORM_RENDERER_scalef(1.0f / scale_x, 1.0f / scale_y, 1.0f);
					}

					if (opengl_booleans & OPENGL_BOOLEAN_ROTATE)
					{
						PLATFORM_RENDERER_rotatef(rotate_angle, 0.0f, 0.0f, 1.0f);
					}

					if (opengl_booleans & OPENGL_BOOLEAN_SECONDARY_SCALE)
					{
						PLATFORM_RENDERER_scalef(1.0f / scale_x_2, 1.0f / scale_y_2, 1.0f);
					}

					PLATFORM_RENDERER_translatef(-x, y, 0);
				}
				break;

				case DRAW_MODE_INVISIBLE:
					// Something was killed and recycled during the object to object collision phase and so
					// is now blank. May as well let it sliiiiiide.
					break;

				case DRAW_MODE_TILEMAP:
				{
#ifdef RETRENGINE_DEBUG_VERSION_THE_LAST_THING_I_DID
					MAIN_debug_last_thing("DRAW_MODE_TILEMAP...");
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
					if (OUTPUT_is_valid_bitmap_index(tile_graphic))
					{
						/*
						 * Tilemap UVs come from the tileset image, not the entity sprite
						 * bitmap. Refresh the renderer bind here so background quads do
						 * not inherit whatever textured sprite happened to draw before.
						 */
						PLATFORM_RENDERER_bind_texture(ACTIVE_BMPS[tile_graphic].texture_handle);
					}

					start_bx = entity_pointer[ENT_TILEMAP_POSITION_X];
					start_by = entity_pointer[ENT_TILEMAP_POSITION_Y];

					x_under = MATH_wrap(start_bx, tilesize);
					y_under = MATH_wrap(start_by, tilesize);

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

					PLATFORM_RENDERER_translatef(tileset_drawing_start_offset_x, tileset_drawing_start_offset_y, 0);

					// Get tile pivot data from the first tile.
					sp = &ACTIVE_BMPS[tile_graphic].sprite_list[0];

					left = -sp->pivot_x;
					right = sp->width - sp->pivot_x;
					up = sp->pivot_y;
					down = -(sp->height - sp->pivot_y);

					if (map_optimised)
					{
						for (by = start_by; by < end_by; by++)
						{
							map_line_pointer = &map_pointer[(map_width * by) + start_bx];

							for (bx = start_bx; bx < end_bx; bx++)
							{
								if (*map_line_pointer != 0)
								{
									sp = &ACTIVE_BMPS[tile_graphic].sprite_list[*map_line_pointer];

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
									PLATFORM_RENDERER_translatef(tilesize * *opt_offset, 0, 0);
								}

								PLATFORM_RENDERER_translatef(tilesize, 0, 0);

								map_line_pointer++;
							}

							PLATFORM_RENDERER_translatef(next_line_transform_x - (tilesize * (bx - end_bx)), next_line_transform_y, 0);
						}
					}
					else
					{
						for (by = start_by; by < end_by; by++)
						{
							map_line_pointer = &map_pointer[(map_width * by) + start_bx];

							for (bx = start_bx; bx < end_bx; bx++)
							{
								if (*map_line_pointer != 0)
								{
									sp = &ACTIVE_BMPS[tile_graphic].sprite_list[*map_line_pointer];

									u1 = sp->u1;
									u2 = sp->u2;
									v1 = sp->v1;
									v2 = sp->v2;

									PLATFORM_RENDERER_draw_bound_textured_quad(left, right, up, down, u1, v1, u2, v2);
								}

								PLATFORM_RENDERER_translatef(tilesize, 0, 0);

								map_line_pointer++;
							}

							PLATFORM_RENDERER_translatef(next_line_transform_x, next_line_transform_y, 0);
						}
					}

					PLATFORM_RENDERER_set_window_transform(left_window_transform_x, top_window_transform_y, total_scale_x, total_scale_y);
					break;
				}

				case DRAW_MODE_TILEMAP_LINE:
				{
#ifdef RETRENGINE_DEBUG_VERSION_THE_LAST_THING_I_DID
					MAIN_debug_last_thing("DRAW_MODE_TILEMAP_LINE...");
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
					if (OUTPUT_is_valid_bitmap_index(tile_graphic))
					{
						/*
						 * Tilemap-line rendering uses the tileset image directly for its
						 * sprite frames, so bind that texture explicitly before drawing.
						 */
						PLATFORM_RENDERER_bind_texture(ACTIVE_BMPS[tile_graphic].texture_handle);
					}

					start_bx = window_details[window_number].current_x;
					start_by = entity_pointer[ENT_WORLD_Y];

					x_under = MATH_wrap(start_bx, tilesize);
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

					PLATFORM_RENDERER_translatef(tileset_drawing_start_offset_x, tileset_drawing_start_offset_y, 0);

					// Get tile pivot data from the first tile.
					sp = &ACTIVE_BMPS[tile_graphic].sprite_list[0];

					left = -sp->pivot_x;
					right = sp->width - sp->pivot_x;
					up = sp->pivot_y;
					down = -(sp->height - sp->pivot_y);

					if (map_optimised)
					{
						map_line_pointer = &map_pointer[(map_width * start_by) + start_bx];

						for (bx = start_bx; bx < end_bx; bx++)
						{
							if (*map_line_pointer != 0)
							{
								sp = &ACTIVE_BMPS[tile_graphic].sprite_list[*map_line_pointer];

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
								PLATFORM_RENDERER_translatef(tilesize * *opt_offset, 0, 0);
							}

							PLATFORM_RENDERER_translatef(tilesize, 0, 0);

							map_line_pointer++;
						}
					}
					else
					{
						map_line_pointer = &map_pointer[(map_width * start_by) + start_bx];

						for (bx = start_bx; bx < end_bx; bx++)
						{
							if (*map_line_pointer != 0)
							{
								sp = &ACTIVE_BMPS[tile_graphic].sprite_list[*map_line_pointer];

								u1 = sp->u1;
								u2 = sp->u2;
								v1 = sp->v1;
								v2 = sp->v2;

								PLATFORM_RENDERER_draw_bound_textured_quad(left, right, up, down, u1, v1, u2, v2);
							}

							PLATFORM_RENDERER_translatef(tilesize, 0, 0);

							map_line_pointer++;
						}
					}

					PLATFORM_RENDERER_set_window_transform(left_window_transform_x, top_window_transform_y, total_scale_x, total_scale_y);
					break;
				}

				case DRAW_MODE_STARFIELD:
#ifdef RETRENGINE_DEBUG_VERSION_THE_LAST_THING_I_DID
					MAIN_debug_last_thing("DRAW_MODE_STARFIELD...");
#endif

					PLATFORM_RENDERER_set_window_transform(left_window_transform_x, top_window_transform_y, total_scale_x, total_scale_y);

					if (entity_pointer[ENT_DRAW_OVERRIDE] == DRAW_OVERRIDE_NONE)
					{
						x = entity_pointer[ENT_WORLD_X] - window_x;
						y = entity_pointer[ENT_WORLD_Y] - window_y;
						PLATFORM_RENDERER_translatef(x, -y, 0.0);
					}

					OUTPUT_draw_starfield(entity_pointer[ENT_UNIQUE_ID]);

					PLATFORM_RENDERER_set_window_transform(left_window_transform_x, top_window_transform_y, total_scale_x, total_scale_y);
					break;

				case DRAW_MODE_STARFIELD_LINES:
#ifdef RETRENGINE_DEBUG_VERSION_THE_LAST_THING_I_DID
					MAIN_debug_last_thing("DRAW_MODE_STARFIELD_LINES...");
#endif
					PLATFORM_RENDERER_set_window_transform(left_window_transform_x, top_window_transform_y, total_scale_x, total_scale_y);

					if (entity_pointer[ENT_DRAW_OVERRIDE] == DRAW_OVERRIDE_NONE)
					{
						x = entity_pointer[ENT_WORLD_X] - window_x;
						y = entity_pointer[ENT_WORLD_Y] - window_y;
						PLATFORM_RENDERER_translatef(x, -y, 0.0);
					}

					OUTPUT_draw_starfield_lines(entity_pointer[ENT_UNIQUE_ID]);

					PLATFORM_RENDERER_set_window_transform(left_window_transform_x, top_window_transform_y, total_scale_x, total_scale_y);
					break;

				case DRAW_MODE_STARFIELD_COLOUR:
#ifdef RETRENGINE_DEBUG_VERSION_THE_LAST_THING_I_DID
					MAIN_debug_last_thing("DRAW_MODE_STARFIELD_COLOUR...");
#endif

					PLATFORM_RENDERER_set_window_transform(left_window_transform_x, top_window_transform_y, total_scale_x, total_scale_y);

					if (entity_pointer[ENT_DRAW_OVERRIDE] == DRAW_OVERRIDE_NONE)
					{
						x = entity_pointer[ENT_WORLD_X] - window_x;
						y = entity_pointer[ENT_WORLD_Y] - window_y;
						PLATFORM_RENDERER_translatef(x, -y, 0.0);
					}

					OUTPUT_draw_starfield_colour(entity_pointer[ENT_UNIQUE_ID]);

					PLATFORM_RENDERER_set_window_transform(left_window_transform_x, top_window_transform_y, total_scale_x, total_scale_y);
					break;

				case DRAW_MODE_STARFIELD_COLOUR_LINES:
#ifdef RETRENGINE_DEBUG_VERSION_THE_LAST_THING_I_DID
					MAIN_debug_last_thing("DRAW_MODE_STARFIELD_COLOUR_LINES...");
#endif

					PLATFORM_RENDERER_set_window_transform(left_window_transform_x, top_window_transform_y, total_scale_x, total_scale_y);

					if (entity_pointer[ENT_DRAW_OVERRIDE] == DRAW_OVERRIDE_NONE)
					{
						x = entity_pointer[ENT_WORLD_X] - window_x;
						y = entity_pointer[ENT_WORLD_Y] - window_y;
						PLATFORM_RENDERER_translatef(x, -y, 0.0);
					}

					OUTPUT_draw_starfield_colour_lines(entity_pointer[ENT_UNIQUE_ID]);

					PLATFORM_RENDERER_set_window_transform(left_window_transform_x, top_window_transform_y, total_scale_x, total_scale_y);
					break;

				case DRAW_MODE_SOLID_RECTANGLE:
#ifdef RETRENGINE_DEBUG_VERSION_THE_LAST_THING_I_DID
					MAIN_debug_last_thing("DRAW_MODE_SOLID_RECTANGLE...");
#endif

					OUTPUT_apply_texture_parameters_from_flags(opengl_booleans, old_opengl_booleans);

					x = entity_pointer[ENT_WORLD_X] - window_x;
					y = entity_pointer[ENT_WORLD_Y] - window_y;

					left = -entity_pointer[ENT_UPPER_WIDTH];
					right = entity_pointer[ENT_LOWER_WIDTH];
					up = entity_pointer[ENT_UPPER_HEIGHT];
					down = -entity_pointer[ENT_LOWER_HEIGHT];

					PLATFORM_RENDERER_translatef(x, -y, 0.0);

					if (opengl_booleans & OPENGL_BOOLEAN_ROTATE)
					{
						rotate_angle = float(entity_pointer[ENT_OPENGL_ANGLE]) / 100.0f;
						PLATFORM_RENDERER_rotatef(-rotate_angle, 0.0f, 0.0f, 1.0f);
					}

					if (opengl_booleans & OPENGL_BOOLEAN_SCALE)
					{
						scale_x = float(entity_pointer[ENT_OPENGL_SCALE_X]) / 10000.0f;
						scale_y = float(entity_pointer[ENT_OPENGL_SCALE_Y]) / 10000.0f;
						PLATFORM_RENDERER_scalef(scale_x, scale_y, 1.0f);
					}

					PLATFORM_RENDERER_draw_bound_solid_quad(left, right, up, down);

					if (opengl_booleans & OPENGL_BOOLEAN_SCALE)
					{
						PLATFORM_RENDERER_scalef(1.0f / scale_x, 1.0f / scale_y, 1.0f);
					}

					if (opengl_booleans & OPENGL_BOOLEAN_ROTATE)
					{
						PLATFORM_RENDERER_rotatef(rotate_angle, 0.0f, 0.0f, 1.0f);
					}

					PLATFORM_RENDERER_translatef(-x, y, 0);
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
				MAIN_debug_last_thing("Finished Switch...");
#endif

				if (!OUTPUT_window_queue_step(window_number, z_ordering_list, &current_entity, &queue_step_counter))
				{
					current_entity = UNSET;
				}

				old_opengl_booleans = opengl_booleans;
			}

			OUTPUT_flush_hot_tunnel_queue();

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
	MAIN_debug_last_thing("Elvis has left the building...");
#endif

	return total_entities_drawn;
}

void OUTPUT_store_frame_info_in_entity_collision(int *entity_pointer, int modifier)
{
	int sprite = entity_pointer[ENT_SPRITE];
	int frame = entity_pointer[ENT_BASE_FRAME];

	if ((sprite < 0) || (frame < 0))
	{
		OUTPUT_message("Trying to set collision with unset sprite or frame!");
	}

	int width = ACTIVE_BMPS[sprite].sprite_list[frame].width;
	int height = ACTIVE_BMPS[sprite].sprite_list[frame].height;

	int pivot_x = ACTIVE_BMPS[sprite].sprite_list[frame].pivot_x;
	int pivot_y = ACTIVE_BMPS[sprite].sprite_list[frame].pivot_y;

	entity_pointer[ENT_UPPER_WIDTH] = pivot_x + modifier;
	entity_pointer[ENT_UPPER_HEIGHT] = pivot_y + modifier;

	entity_pointer[ENT_LOWER_WIDTH] = ((width - 1) - pivot_x) + modifier;
	entity_pointer[ENT_LOWER_HEIGHT] = ((height - 1) - pivot_y) + modifier;
}

void OUTPUT_store_frame_info_in_entity_collision_including_scale(int *entity_pointer, int modifier)
{
	int sprite = entity_pointer[ENT_SPRITE];
	int frame = entity_pointer[ENT_BASE_FRAME];

	int width = (ACTIVE_BMPS[sprite].sprite_list[frame].width * entity_pointer[ENT_OPENGL_SCALE_X]) / 10000;
	int height = (ACTIVE_BMPS[sprite].sprite_list[frame].height * entity_pointer[ENT_OPENGL_SCALE_Y]) / 10000;

	int pivot_x = (ACTIVE_BMPS[sprite].sprite_list[frame].pivot_x * entity_pointer[ENT_OPENGL_SCALE_X]) / 10000;
	int pivot_y = (ACTIVE_BMPS[sprite].sprite_list[frame].pivot_y * entity_pointer[ENT_OPENGL_SCALE_Y]) / 10000;

	entity_pointer[ENT_UPPER_WIDTH] = pivot_x + modifier;
	entity_pointer[ENT_UPPER_HEIGHT] = pivot_y + modifier;

	entity_pointer[ENT_LOWER_WIDTH] = ((width - 1) - pivot_x) + modifier;
	entity_pointer[ENT_LOWER_HEIGHT] = ((height - 1) - pivot_y) + modifier;
}
