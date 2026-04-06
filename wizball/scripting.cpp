// This file is where anything to do with scripting goes on with the exception of parsing it
// which is a different kettle of debug-only fish.

#include <stdio.h>
#include <assert.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#ifdef ALLEGRO_MACOSX
#include <CoreServices/CoreServices.h>
#endif

#include "parser.h"
#include "scripting.h"
#include "control.h"
#include "graphics.h"
#include "math_stuff.h"
#include "string_stuff.h"
#include "main.h"
#include "new_editor.h"
#include "tilemaps.h"
#include "output.h"
#include "global_param_list.h"
#include "object_collision.h"
#include "world_collision.h"
#include "roomzones.h"
#include "tilesets.h"
#include "particles.h"
#include "sound.h"
#include "file_stuff.h"
#include "textfiles.h"
#include "hiscores.h"
#include "arrays.h"
#include "spawn_points.h"
#include "font_routines.h"
#include "events.h"
#include "paths.h"
#include "savegame.h"
#include "platform.h"

#include "fortify.h"

static int SCRIPTING_bswap32(int v)
{
	unsigned int u = (unsigned int) v;
	u = ((u & 0x000000FFU) << 24) |
		((u & 0x0000FF00U) << 8)  |
		((u & 0x00FF0000U) >> 8)  |
		((u & 0xFF000000U) >> 24);
	return (int) u;
}

typedef struct
{
	int script_number;
	int absolute_line;
	char label[NAME_SIZE];
} scripting_state_label_entry_struct;

extern script_line *scr;
extern int *scr_lookup;
extern int script_count;

static scripting_state_label_entry_struct *scripting_state_label_entries = NULL;
static int scripting_state_label_entry_count = 0;
static int scripting_randomise_seed_flag = UNSET;

static int SCRIPTING_get_randomise_seed_flag(void)
{
	if (scripting_randomise_seed_flag == UNSET)
	{
		scripting_randomise_seed_flag = GPL_find_word_value("FLAG", "RANDOMISE_SEED");
	}

	return scripting_randomise_seed_flag;
}

static void SCRIPTING_sync_special_random_seed_flag(int seed_index)
{
	int flag_index;

	if (seed_index != 0)
	{
		return;
	}

	flag_index = SCRIPTING_get_randomise_seed_flag();

	if ((flag_index >= 0) && (flag_index < MAX_FLAGS))
	{
		flag_array[flag_index] = MATH_get_special_rand_seed(seed_index);
	}
}

static void SCRIPTING_restore_special_random_seed_from_flags(void)
{
	int flag_index = SCRIPTING_get_randomise_seed_flag();

	if ((flag_index >= 0) && (flag_index < MAX_FLAGS))
	{
		int seed_value = flag_array[flag_index];

		if (seed_value <= 0)
		{
			seed_value = 1;
		}

		MATH_seed_special_rand(0, (unsigned int) seed_value);
		MATH_srand((unsigned int) seed_value);
		flag_array[flag_index] = MATH_get_special_rand_seed(0);
	}
}

static void SCRIPTING_hash_compatibility_int(unsigned int *hash, int value)
{
	unsigned int u = (unsigned int) value;
	int index;

	for (index = 0; index < 4; index++)
	{
		unsigned char byte = (unsigned char) ((u >> (index * 8)) & 0xffU);
		*hash ^= byte;
		*hash *= 16777619u;
	}
}

static void SCRIPTING_hash_compatibility_text(unsigned int *hash, const char *text)
{
	if (text == NULL)
	{
		SCRIPTING_hash_compatibility_int(hash, 0);
		return;
	}

	while (*text != '\0')
	{
		*hash ^= (unsigned char) *text;
		*hash *= 16777619u;
		text++;
	}
}

static void SCRIPTING_free_continue_state_map(void)
{
	if (scripting_state_label_entries != NULL)
	{
		free(scripting_state_label_entries);
		scripting_state_label_entries = NULL;
	}

	scripting_state_label_entry_count = 0;
}

static bool SCRIPTING_is_persistent_continue_script(int script_number)
{
	if ((script_number < 0) || (script_number >= GPL_list_size("SCRIPTS")))
	{
		return false;
	}

	return
		(strcmp(GPL_get_entry_name("SCRIPTS", script_number), "MAIN_GAME_CONTROLLER") == 0) ||
		(strcmp(GPL_get_entry_name("SCRIPTS", script_number), "GENERIC_LEVEL_ENEMY") == 0) ||
		(strcmp(GPL_get_entry_name("SCRIPTS", script_number), "MOLECULE") == 0);
}

static bool SCRIPTING_is_persistent_continue_state_variable(int variable_index)
{
	switch (variable_index)
	{
	case ENT_PROGRAM_START:
	case ENT_WAIT_RESTART:
	case ENT_WAKE_LINE:
	case ENT_ENTITY_HIT_LINE:
	case ENT_WORLD_HIT_LINE:
		return true;

	default:
		return false;
	}
}

static void SCRIPTING_add_continue_state_label(int script_number, const char *label, int absolute_line)
{
	if ((label == NULL) || (label[0] == '\0'))
	{
		return;
	}

	if (scripting_state_label_entries == NULL)
	{
		scripting_state_label_entries = (scripting_state_label_entry_struct *) malloc(sizeof(scripting_state_label_entry_struct));
	}
	else
	{
		scripting_state_label_entries = (scripting_state_label_entry_struct *) realloc(
			scripting_state_label_entries,
			sizeof(scripting_state_label_entry_struct) * (scripting_state_label_entry_count + 1));
	}

	if (scripting_state_label_entries == NULL)
	{
		OUTPUT_message("Could not allocate continue state label map.");
		assert(0);
	}

	scripting_state_label_entries[scripting_state_label_entry_count].script_number = script_number;
	scripting_state_label_entries[scripting_state_label_entry_count].absolute_line = absolute_line;
	snprintf(
		scripting_state_label_entries[scripting_state_label_entry_count].label,
		sizeof(scripting_state_label_entries[scripting_state_label_entry_count].label),
		"%s",
		label);
	scripting_state_label_entry_count++;
}

static void SCRIPTING_load_continue_state_map(void)
{
	FILE *file_pointer;
	char line[MAX_LINE_SIZE];
	int current_script_number = UNSET;

	SCRIPTING_free_continue_state_map();

	file_pointer = FILE_open_project_read_case_fallback("script_state_map.txt");

	if (file_pointer == NULL)
	{
		return;
	}

	while (fgets(line, MAX_LINE_SIZE, file_pointer) != NULL)
	{
		char script_name[NAME_SIZE];
		char label_name[NAME_SIZE];
		int local_line = UNSET;

		STRING_strip_all_disposable(line);

		if (sscanf(line, "SCRIPT %127s", script_name) == 1)
		{
			current_script_number = GPL_find_word_value("SCRIPTS", script_name);
			continue;
		}

		if (strcmp(line, "END_SCRIPT") == 0)
		{
			current_script_number = UNSET;
			continue;
		}

		if ((current_script_number != UNSET) &&
			(sscanf(line, "LABEL %127s %d", label_name, &local_line) == 2))
		{
			if ((current_script_number >= 0) &&
				(current_script_number < script_count) &&
				(scr_lookup != NULL))
			{
				SCRIPTING_add_continue_state_label(
					current_script_number,
					label_name,
					scr_lookup[current_script_number] + local_line);
			}
		}
	}

	fclose(file_pointer);
}

static bool SCRIPTING_describe_continue_state_line(int script_number, int absolute_line, char *out_label, int out_label_size, int *out_offset)
{
	int entry_index;

	if ((out_label == NULL) || (out_offset == NULL))
	{
		return false;
	}

	if (!SCRIPTING_is_persistent_continue_script(script_number))
	{
		return false;
	}

	for (entry_index = 0; entry_index < scripting_state_label_entry_count; entry_index++)
	{
		if (scripting_state_label_entries[entry_index].script_number != script_number)
		{
			continue;
		}

		// Keep live remapping conservative: only exact labelled entry points are
		// considered stable enough to survive harmless script edits. Arbitrary
		// "nearest label + offset" restore was too fragile for the gameplay scripts.
		if (scripting_state_label_entries[entry_index].absolute_line == absolute_line)
		{
			snprintf(out_label, out_label_size, "%s", scripting_state_label_entries[entry_index].label);
			*out_offset = 0;
			return true;
		}
	}

	return false;
}

int SCRIPTING_resolve_continue_state_line(int script_number, const char *label, int offset)
{
	int entry_index;

	if ((label == NULL) || !SCRIPTING_is_persistent_continue_script(script_number))
	{
		return UNSET;
	}

	for (entry_index = 0; entry_index < scripting_state_label_entry_count; entry_index++)
	{
		if (scripting_state_label_entries[entry_index].script_number != script_number)
		{
			continue;
		}

		if (strcmp(scripting_state_label_entries[entry_index].label, label) == 0)
		{
			int resolved_line = scripting_state_label_entries[entry_index].absolute_line + offset;
			int script_start = scr_lookup[script_number];
			int script_end = scr_lookup[script_number + 1];

			if ((resolved_line >= script_start) && (resolved_line < script_end))
			{
				return resolved_line;
			}

			return UNSET;
		}
	}

	return UNSET;
}

static int SCRIPTING_count_entity_state_overrides_for_save(int ent_index)
{
	int variable_index;
	int count = 0;
	int script_number = entity[ent_index][ENT_SCRIPT_NUMBER];
	const char *script_name = GPL_get_entry_name("SCRIPTS", script_number);

	if (!SCRIPTING_is_persistent_continue_script(script_number))
	{
		return 0;
	}

	if ((script_name != NULL) && (strcmp(script_name, "MAIN_GAME_CONTROLLER") == 0))
	{
		if (SAVEGAME_should_output_entity_variable(ent_index, ENT_PROGRAM_START))
		{
			count++;
		}

		if (SAVEGAME_should_output_entity_variable(ent_index, ENT_WAIT_RESTART))
		{
			count++;
		}

		return count;
	}

	for (variable_index = GPL_find_word_value("VARIABLE", "ALIVE"); variable_index < GPL_list_size("VARIABLE"); variable_index++)
	{
		if (SAVEGAME_should_output_entity_variable(ent_index, variable_index) &&
			SCRIPTING_is_persistent_continue_state_variable(variable_index))
		{
			char label[NAME_SIZE];
			int offset;

			if (SCRIPTING_describe_continue_state_line(script_number, entity[ent_index][variable_index], label, sizeof(label), &offset))
			{
				count++;
			}
		}
	}

	return count;
}

static void SCRIPTING_output_entity_state_overrides_to_file(int ent_index, FILE *file_pointer)
{
	int variable_index;
	int script_number = entity[ent_index][ENT_SCRIPT_NUMBER];
	int state_count = SCRIPTING_count_entity_state_overrides_for_save(ent_index);
	char line[MAX_LINE_SIZE];
	const char *script_name = GPL_get_entry_name("SCRIPTS", script_number);

	snprintf(line, sizeof(line), "\t\t#START_OF_STATE_COUNT = %i\n", state_count);
	fputs(line, file_pointer);

	if (!SCRIPTING_is_persistent_continue_script(script_number))
	{
		fputs("\t\t#END_OF_STATES\n", file_pointer);
		return;
	}

	if ((script_name != NULL) && (strcmp(script_name, "MAIN_GAME_CONTROLLER") == 0))
	{
		if (SAVEGAME_should_output_entity_variable(ent_index, ENT_PROGRAM_START))
		{
			snprintf(
				line,
				sizeof(line),
				"\t\t\t#ENTITY_STATE '%s' = '%s' + %i\n",
				GPL_get_entry_name("VARIABLE", ENT_PROGRAM_START),
				"RESTART_MAIN_LOOP",
				0);
			fputs(line, file_pointer);
		}

		if (SAVEGAME_should_output_entity_variable(ent_index, ENT_WAIT_RESTART))
		{
			snprintf(
				line,
				sizeof(line),
				"\t\t\t#ENTITY_STATE '%s' = '%s' + %i\n",
				GPL_get_entry_name("VARIABLE", ENT_WAIT_RESTART),
				"RESTART_MAIN_LOOP",
				0);
			fputs(line, file_pointer);
		}

		fputs("\t\t#END_OF_STATES\n", file_pointer);
		return;
	}

	for (variable_index = GPL_find_word_value("VARIABLE", "ALIVE"); variable_index < GPL_list_size("VARIABLE"); variable_index++)
	{
		if (SAVEGAME_should_output_entity_variable(ent_index, variable_index) &&
			SCRIPTING_is_persistent_continue_state_variable(variable_index))
		{
			char label[NAME_SIZE];
			int offset;

			if (SCRIPTING_describe_continue_state_line(script_number, entity[ent_index][variable_index], label, sizeof(label), &offset))
			{
				snprintf(
					line,
					sizeof(line),
					"\t\t\t#ENTITY_STATE '%s' = '%s' + %i\n",
					GPL_get_entry_name("VARIABLE", variable_index),
					label,
					offset);
				fputs(line, file_pointer);
			}
		}
	}

	fputs("\t\t#END_OF_STATES\n", file_pointer);
}

int total_process_counter;
int drawn_process_counter;
int alive_process_counter;
int script_lines_executed;
int used_entities;
int limboed_entities;
int free_entities;
int lost_entities;

int watched_entity = UNSET;
int watched_variable = UNSET;

bool not_in_collision_phase = true;

int first_entity_in_target_list;

int relational_movement; // This is either standard (everything moves normally)
	// or pop star mode where instead everything is moved in relation to the velocity
	// of a particular entity
int relational_parent_id; // Which object is the daddy in pop star mode
int relational_x_range; // How far the other objects will move from off of the x axis before wrapping around in pop star mode
int relational_y_range; // How far the other objects will move from off of the y axis before wrapping around in pop star mode

script_line *scr = NULL;
int *scr_lookup = NULL;

int script_line_count;
int script_count;

int *watch_list_script = NULL;
int *watch_list_variable = NULL;
int watch_list_size = 0;

prefab_struct *prefab_space = NULL;

int number_of_prefabs_loaded = 0;
int global_next_entity = UNSET;

data_store_struct data_store[MAX_ENTITIES];

int entity [MAX_ENTITIES][MAX_ENTITY_VARIABLES];
	// This is where all the little entities live...
arbitrary_quads_struct arbitrary_quads[MAX_ENTITIES];
	// This is the arbitrary quad struct.
arbitrary_vertex_colour_struct arbitrary_vertex_colours[MAX_ENTITIES];

int reset_entity[MAX_ENTITY_VARIABLES];
	// And this contains a copy of the entity data as it should be when it's factory fresh.
	// This is then memcopied over the entities to reset them.
bool entity_search_inclusion[MAX_ENTITIES];
	// And this is a boolean which is written to during entity counting to enable the refine of searches.

int *flag_array = NULL;
int *flag_type = NULL;
int flag_count;

int first_dead_entity_in_list = UNSET;
int last_dead_entity_in_list = UNSET;

int first_limbo_entity_in_list = UNSET;
int last_limbo_entity_in_list = UNSET;

int first_processed_entity_in_list = UNSET;
int last_processed_entity_in_list = UNSET;

int unique_program_trace_entity = UNSET;

bool complete_trace_from_now_on = false;

static int entity_window_queue_stamp[MAX_ENTITIES];
static int entity_window_queue_epoch = 0;
static bool scripting_window_queue_guard_trace_checked = false;
static bool scripting_window_queue_guard_trace_enabled = false;
static unsigned int scripting_last_save_load_ui_pump_ms = 0;
static int scripting_save_load_spinner_flag = UNSET;
static int scripting_save_load_spinner_script = UNSET;
static int scripting_save_load_spinner_glow_script = UNSET;

static bool SCRIPTING_is_window_queue_guard_trace_enabled(void)
{
	if (!scripting_window_queue_guard_trace_checked)
	{
		const char *value = getenv("WIZBALL_WINDOW_QUEUE_GUARD_TRACE");
		scripting_window_queue_guard_trace_enabled =
			(value != NULL) &&
			(value[0] != '\0') &&
			(strcmp(value, "0") != 0);
		scripting_window_queue_guard_trace_checked = true;
	}
	return scripting_window_queue_guard_trace_enabled;
}

static void SCRIPTING_emit_window_queue_guard(const char *format, ...)
{
	char guard_line[MAX_LINE_SIZE];
	va_list args;

	if (!SCRIPTING_is_window_queue_guard_trace_enabled())
	{
		return;
	}

	va_start(args, format);
	vsnprintf(guard_line, sizeof(guard_line), format, args);
	va_end(args);

	MAIN_add_to_log(guard_line);
	fprintf(stderr, "%s\n", guard_line);
}

int just_created_entity = 0;
	// This is the last entity which will be processed each frame

bool wrap_coordinates; // If this is set then when an object goes off one side of the world it will
	// appear on the opposite side. This is why it's best to have a gameworld that's slightly bigger
	// than the game area so that they don't flick-out when they're half-way off the side.

bool pop_star_mode_x; // If this is set then the co-ordinates of the players are kept at a distance of no
	// greater than game_world/2 from the pop_star_entity on the x-axis, however when they are dumped into the collision buckets
	// their co-ordinates are shoved back into the scope of the game-world. This is for defender style games
	// and the like. When pop-star mode is turned off, it might be an idea to wrap everything back into the game
	// world.
bool pop_star_mode_y; // Ditto but for the y-co-ord.

int pop_star_entity; // Who's the daddy?

bool completely_exit_game = false;

static bool SCRIPTING_is_focus_script_name(const char *name)
{
	if (name == NULL)
	{
		return false;
	}

	return
		(strcmp(name, "START_GAME") == 0) ||
		(strcmp(name, "MAIN_GAME_CONTROLLER") == 0) ||
		(strcmp(name, "CHOOSE_WIZBALL_START_POSITION") == 0) ||
		(strcmp(name, "GET_READY_SCREEN") == 0) ||
		(strcmp(name, "GAME_WINDOW_HANDLER") == 0) ||
		(strcmp(name, "WIZBALL") == 0);
}



static bool SCRIPTING_is_focus_script_index(int script_number)
{
	if ((script_number < 0) || (script_number >= script_count))
	{
		return false;
	}

	return SCRIPTING_is_focus_script_name(GPL_get_entry_name("SCRIPTS", script_number));
}

static int scripting_kill_source_entity = UNSET;
static int scripting_kill_source_line = UNSET;

static void SCRIPTING_log_focus_kill(int target_entity, const char *reason)
{
	if (output_debug_information == false)
	{
		return;
	}

	if ((target_entity < 0) || (target_entity >= MAX_ENTITIES))
	{
		return;
	}

	int target_script = entity[target_entity][ENT_SCRIPT_NUMBER];
	if (SCRIPTING_is_focus_script_index(target_script) == false)
	{
		return;
	}

	const char *target_name = GPL_get_entry_name("SCRIPTS", target_script);
	const char *source_name = "UNSET";
	if ((scripting_kill_source_entity >= 0) && (scripting_kill_source_entity < MAX_ENTITIES))
	{
		int source_script = entity[scripting_kill_source_entity][ENT_SCRIPT_NUMBER];
		if ((source_script >= 0) && (source_script < script_count))
		{
			source_name = GPL_get_entry_name("SCRIPTS", source_script);
		}
	}

    char line[MAX_LINE_SIZE];
    snprintf(line, sizeof(line),
		"FOCUS_KILL target=%d(%s) by=%d(%s) src_line=%d frame=%d reason=%s",
		target_entity,
		target_name,
		scripting_kill_source_entity,
		source_name,
		scripting_kill_source_line,
		frames_so_far,
		reason);
	MAIN_add_to_log(line);
}

typedef struct
{
	int line_length;
	char *line_text;
	int line_owner; // This is so that the output can be grouped by entity. Clever, no?
} tracer_script_output_line_struct;

tracer_script_output_line_struct *tracer_output_script = NULL;
int tracer_output_script_length = 0;
int allocated_tracer_output_script_lines = 0;

int *tracer_output_entity_list = NULL;
int tracer_output_entity_list_size = 0;

#define TRACER_SCRIPT_LINE_CHUNK_SIZE		(128)

void do_exit_game(void) {
  completely_exit_game = true;
}
bool complete_collision = false;



#define MAX_SCRIPT_LINE_LENGTH			(64)

int tracer_before_values[MAX_SCRIPT_LINE_LENGTH];
int tracer_after_values[MAX_SCRIPT_LINE_LENGTH];



void SCRIPTING_add_output_tracer_script_line (char *line,int entity_id)
{
	if (tracer_output_script_length == allocated_tracer_output_script_lines)
	{
		// We need to allocate more space, Cap'n!

		if (tracer_output_script == NULL)
		{
			tracer_output_script = (tracer_script_output_line_struct *) malloc (sizeof(tracer_script_output_line_struct) * TRACER_SCRIPT_LINE_CHUNK_SIZE);
		}
		else
		{
			tracer_output_script = (tracer_script_output_line_struct *) realloc (tracer_output_script , sizeof(tracer_script_output_line_struct) * (allocated_tracer_output_script_lines + TRACER_SCRIPT_LINE_CHUNK_SIZE));
		}

		allocated_tracer_output_script_lines += TRACER_SCRIPT_LINE_CHUNK_SIZE;
	}

	tracer_output_script[tracer_output_script_length].line_length = strlen (line);
	tracer_output_script[tracer_output_script_length].line_owner = entity_id;
	tracer_output_script[tracer_output_script_length].line_text = (char *) malloc ((tracer_output_script[tracer_output_script_length].line_length + 1) * sizeof(char));
	strcpy (tracer_output_script[tracer_output_script_length].line_text,line);

	tracer_output_script_length++;
}




save_data_struct save_data;

static long SCRIPTING_get_save_file_size_bytes(const char *filename)
{
	char normalised_filename[MAX_LINE_SIZE];
	FILE *file_pointer;
	long file_size = -1;

	if (filename == NULL)
	{
		return -1;
	}

	snprintf(normalised_filename, sizeof(normalised_filename), "%s", filename);
	STRING_lowercase(normalised_filename);

	file_pointer = FILE_open_project_read_case_fallback(normalised_filename);

	if (file_pointer != NULL)
	{
		if (fseek(file_pointer, 0, SEEK_END) == 0)
		{
			file_size = ftell(file_pointer);
		}

		fclose(file_pointer);
	}

	return file_size;
}

static void SCRIPTING_log_save_load_profile(
	const char *operation,
	const char *filename,
	long file_size,
	unsigned int total_ms,
	unsigned int stage_1_ms,
	unsigned int stage_2_ms,
	unsigned int stage_3_ms,
	unsigned int stage_4_ms,
	unsigned int stage_5_ms,
	unsigned int stage_6_ms,
	int entity_count)
{
	char line[MAX_LINE_SIZE * 2];

	snprintf(
		line,
		sizeof(line),
		"SAVELOAD_PROFILE op=%s file=%s size=%ld total_ms=%u stage1_ms=%u stage2_ms=%u stage3_ms=%u stage4_ms=%u stage5_ms=%u stage6_ms=%u entities=%d",
		(operation != NULL) ? operation : "unknown",
		(filename != NULL) ? filename : "unknown",
		file_size,
		total_ms,
		stage_1_ms,
		stage_2_ms,
		stage_3_ms,
		stage_4_ms,
		stage_5_ms,
		stage_6_ms,
		entity_count);

	MAIN_add_to_log(line);
}

static void SCRIPTING_cache_save_load_ui_constants(void)
{
	if (scripting_save_load_spinner_flag == UNSET)
	{
		scripting_save_load_spinner_flag = GPL_find_word_value("FLAG", "SAVE_LOAD_SPINNER_ENTITY_ID");
		scripting_save_load_spinner_script = GPL_find_word_value("SCRIPTS", "SAVE_LOAD_SPINNER");
		scripting_save_load_spinner_glow_script = GPL_find_word_value("SCRIPTS", "SAVE_LOAD_SPINNER_GLOW");
	}
}

static bool SCRIPTING_is_save_load_ui_entity(int entity_id)
{
	int script_number;

	if ((entity_id < 0) || (entity_id >= MAX_ENTITIES))
	{
		return false;
	}

	if (entity[entity_id][ENT_ALIVE] <= DEAD)
	{
		return false;
	}

	script_number = entity[entity_id][ENT_SCRIPT_NUMBER];

	return
		(script_number == scripting_save_load_spinner_script) ||
		(script_number == scripting_save_load_spinner_glow_script);
}

static void SCRIPTING_tick_save_load_ui_entities(void)
{
	int entity_id;
	int next_entity_id;

	entity_id = first_processed_entity_in_list;

	while (entity_id != UNSET)
	{
		next_entity_id = entity[entity_id][ENT_NEXT_PROCESS_ENT];

		if (SCRIPTING_is_save_load_ui_entity(entity_id))
		{
			SCRIPTING_interpret_script(entity_id, UNSET);
		}

		entity_id = next_entity_id;
	}
}

static void SCRIPTING_hide_save_load_ui_from_normal_render(void)
{
	int entity_id = first_processed_entity_in_list;

	while (entity_id != UNSET)
	{
		int next_entity_id = entity[entity_id][ENT_NEXT_PROCESS_ENT];

		if (SCRIPTING_is_save_load_ui_entity(entity_id))
		{
			entity[entity_id][ENT_DRAW_MODE] = DRAW_MODE_INVISIBLE;
		}

		entity_id = next_entity_id;
	}
}

static void SCRIPTING_draw_save_load_ui_entity(int entity_id)
{
	int sprite_number;
	int frame_number;
	int world_x;
	int world_y;
	float scale_x;
	int red;
	int green;
	int blue;

	if ((entity_id < 0) || (entity_id >= MAX_ENTITIES))
	{
		return;
	}

	if (entity[entity_id][ENT_ALIVE] <= DEAD)
	{
		return;
	}

	sprite_number = entity[entity_id][ENT_SPRITE];
	frame_number = entity[entity_id][ENT_CURRENT_FRAME];

	if ((sprite_number < 0) || (frame_number < 0))
	{
		return;
	}

	world_x = entity[entity_id][ENT_WORLD_X];
	world_y = entity[entity_id][ENT_WORLD_Y];
	scale_x = (float) entity[entity_id][ENT_OPENGL_SCALE_X] / 10000.0f;
	red = entity[entity_id][ENT_OPENGL_VERTEX_RED];
	green = entity[entity_id][ENT_OPENGL_VERTEX_GREEN];
	blue = entity[entity_id][ENT_OPENGL_VERTEX_BLUE];

	OUTPUT_draw_sprite_scale(
		sprite_number,
		frame_number,
		world_x,
		world_y,
		scale_x,
		red,
		green,
		blue);
}

static void SCRIPTING_draw_save_load_ui(void)
{
	int entity_id = first_processed_entity_in_list;

	while (entity_id != UNSET)
	{
		int next_entity_id = entity[entity_id][ENT_NEXT_PROCESS_ENT];

		if (SCRIPTING_is_save_load_ui_entity(entity_id))
		{
			SCRIPTING_draw_save_load_ui_entity(entity_id);
		}

		entity_id = next_entity_id;
	}
}

static void SCRIPTING_reset_save_load_ui_pump(void)
{
	scripting_last_save_load_ui_pump_ms = 0;
}

static void SCRIPTING_pump_save_load_ui(bool force)
{
	unsigned int now_ms;
	int spinner_entity_id = UNSET;

	SCRIPTING_cache_save_load_ui_constants();

	if (scripting_save_load_spinner_flag == UNSET)
	{
		return;
	}

	spinner_entity_id = flag_array[scripting_save_load_spinner_flag];

	if ((spinner_entity_id < 0) || (spinner_entity_id >= MAX_ENTITIES))
	{
		return;
	}

	if (entity[spinner_entity_id][ENT_ALIVE] <= DEAD)
	{
		return;
	}

	now_ms = PLATFORM_get_wall_time_ms();

	if ((force == false) && (scripting_last_save_load_ui_pump_ms != 0))
	{
		if ((now_ms - scripting_last_save_load_ui_pump_ms) < 33)
		{
			return;
		}
	}

	scripting_last_save_load_ui_pump_ms = now_ms;

	SCRIPTING_tick_save_load_ui_entities();
	SCRIPTING_hide_save_load_ui_from_normal_render();
	OUTPUT_clear_screen();
	SCRIPTING_draw_save_load_ui();
	OUTPUT_updatescreen();
}

static void SCRIPTING_reset_checkcode_state(unsigned char checksum[4], int *cycle)
{
	checksum[0] = 0;
	checksum[1] = 0;
	checksum[2] = 0;
	checksum[3] = 0;
	*cycle = 0;
}

static void SCRIPTING_update_checkcode_state(const char *text, unsigned char checksum[4], int *cycle)
{
	int c;

	if (text == NULL)
	{
		return;
	}

	for (c = 0; c < signed(strlen(text)); c++)
	{
		unsigned char byte;

		if (c % 2)
		{
			byte = (unsigned char) (text[c] - 32);
		}
		else
		{
			byte = (unsigned char) text[c];
		}

		checksum[*cycle] ^= byte;
		*cycle += 1;
		*cycle %= 4;
	}
}

static char *SCRIPTING_get_checkcode_from_state(const unsigned char checksum[4])
{
	int c;
	static char checkword[9] = {"        "};

	for (c = 0; c < 8; c += 2)
	{
		unsigned char byte;

		byte = (unsigned char) ((checksum[c / 2] >> 4) & 15);
		checkword[c] = (char) (byte + 'A');

		byte = (unsigned char) (checksum[c / 2] & 15);
		checkword[c + 1] = (char) ('P' - byte);
	}

	return checkword;
}

// The window_details struct just says where each of the windows into the game are on the
// screen and within the scope of the game. Each one has a bitmask so that only entities with
// a matching (or partially matching) mask can be placed into them. By this means you can have
// two different sets of entities co-existing in the same game-space but each one will only
// show up in one window. This was specifically done so that the gui can be stuck at the top-left
// of the gameworld but will not clash when the player goes up there as well.

// Actually, they include the ordering guff as well, now.

window_struct *window_details = NULL;
int number_of_windows;

// These structures are for the buckets which the gameworld is divided into. At the start of
// each frame each bitmask one has the "or" value of each of the windows placed into it so that
// entities with matching bitmasks can be placed into the linked lists. After that when the windows
// are actually drawn, for each window every matching entity is drawn. Then all the window
// bitmask buckets are changed back to 0 so they are unwritable.

int window_bucket_bitshift = 0;
int window_buckets_per_row;
int window_buckets_per_col;

int *window_bucket_bitmasks = NULL;
int *window_bucket_starts = NULL; // And this is where it all ends up so that the windows can decide what's in them and draw it.
int *window_bucket_ends = NULL;

int *scripting_interaction_table = NULL;
int script_list_size = UNSET;

static bool SCRIPTING_is_valid_script_index(int script_number)
{
	int total_scripts = GPL_list_size("SCRIPTS");

	return (scr_lookup != NULL) && (script_number >= 0) && (script_number < total_scripts);
}

static bool scripting_loop_diag_checked_env = false;
static bool scripting_loop_diag_enabled = false;
static bool scripting_interpret_guard_checked_env = false;
static int scripting_interpret_guard_limit = 250000;

static bool SCRIPTING_is_loop_diag_enabled(void)
{
	if (!scripting_loop_diag_checked_env)
	{
		const char *env = getenv("WIZBALL_SCRIPT_LOOP_DIAG");
		scripting_loop_diag_enabled = (env != NULL) &&
			((strcmp(env, "1") == 0) ||
			 (strcmp(env, "true") == 0) ||
			 (strcmp(env, "TRUE") == 0) ||
			 (strcmp(env, "yes") == 0) ||
			 (strcmp(env, "on") == 0));
		scripting_loop_diag_checked_env = true;
	}

	return scripting_loop_diag_enabled;
}

static int SCRIPTING_get_interpret_guard_limit(void)
{
	if (!scripting_interpret_guard_checked_env)
	{
		const char *env = getenv("WIZBALL_SCRIPT_INTERPRET_GUARD");
		if ((env != NULL) && (env[0] != '\0'))
		{
			int value = atoi(env);
			if (value < 1000)
			{
				value = 1000;
			}
			if (value > 5000000)
			{
				value = 5000000;
			}
			scripting_interpret_guard_limit = value;
		}
		scripting_interpret_guard_checked_env = true;
	}

	return scripting_interpret_guard_limit;
}

static void SCRIPTING_log_loop_guard_trip(const char *stage, int list_head, int current_entity, int iteration_count)
{
	char line[MAX_LINE_SIZE];
	int probe = list_head;
	int n;

	if (stage == NULL)
	{
		stage = "unknown";
	}

	snprintf(
		line,
		sizeof(line),
		"SCRIPT_LOOP_GUARD stage=%s frame=%d iter=%d head=%d current=%d",
		stage,
		frames_so_far,
		iteration_count,
		list_head,
		current_entity);
	MAIN_add_to_log(line);
	fprintf(stderr, "%s\n", line);

	if (!SCRIPTING_is_loop_diag_enabled())
	{
		return;
	}

	for (n = 0; (n < 48) && (probe != UNSET); n++)
	{
		int next_probe = UNSET;
		int prev_probe = UNSET;
		int alive = UNSET;
		int script_number = UNSET;
		const char *script_name = "UNSET";

		if ((probe >= 0) && (probe < MAX_ENTITIES))
		{
			next_probe = entity[probe][ENT_NEXT_PROCESS_ENT];
			prev_probe = entity[probe][ENT_PREV_PROCESS_ENT];
			alive = entity[probe][ENT_ALIVE];
			script_number = entity[probe][ENT_SCRIPT_NUMBER];
			if ((script_number >= 0) && (script_number < script_count))
			{
				script_name = GPL_get_entry_name("SCRIPTS", script_number);
			}
		}

		snprintf(
			line,
			sizeof(line),
			"SCRIPT_LOOP_GUARD_NODE stage=%s n=%d id=%d next=%d prev=%d alive=%d script=%d(%s)",
			stage,
			n,
			probe,
			next_probe,
			prev_probe,
			alive,
			script_number,
			script_name);
		MAIN_add_to_log(line);
		fprintf(stderr, "%s\n", line);

		if ((probe < 0) || (probe >= MAX_ENTITIES))
		{
			break;
		}

		probe = next_probe;
	}
}

// This structure says where in the world collisions between entities take place. Because it's
// obviously fairly processor intensive to collide loads of things together you can only have
// one area in the game where that's happening. In something like a scrolling shooter you'd
// probably set the collision window to an area the size of the display (or slightly larger)
// but in something like Metroid you'd have it active for the entirety of whatever room you were
// in. In something like an adventure where the only real collision to speak of is with the
// map and a few spartan characters (with all other collision initiated via a keypress - ie,
// picking up objects which you'd walk through otherwise) then you could probably just have the
// entire map active with very large buckets because each bucket would only take a moment to process
// due to the vast majority of them containing nothing collidable.



// Note that after a list is read from one of the x/y buckets it is blanked so that it isn't re-read as we 
// go through all the game windows deciding which x/y buckets to transfer.

// When it comes to moving everything from the x or y buckets to the window_buckets it must be done a layer
// at a time, and so a loop from (0 to game_world_layers-1) will be employed where we only transfer those things
// in the lists which belong to those layers. To save on replication of effort, however, when we find the first
// thing in the list that's of a greater layer, we set the list to start with that entity first next time.

// Note that entities are not placed into the x/y ordering buckets according to their x/y co-ord, but rather according
// to their "order co-ord" which can be different from their real one. This is so that grouped entities will
// all share a common "order co-ord" so they aren't drawn all wrong and crappy.

// Okay, when things are z then y ordered (for instance, any pairing of orders is valid) then it means
// that we have to have to got through the window buckets a number of times equivalent to the range of the
// first co-ordinate relied upon which if there's a large range obviously means going through them a large number of
// times if we have a great amount of depth, although I suppose 

// So, what about tilemaps? What do we do with those suckers?

// I'm thinking a tilemap should be a type of entity, like a normal enemy where it gets drawn in order along with everything
// else (although obviously it can be pre or post if you prefer) however it has to be on-screen in effect, and there's also
// the issue of whether it affect collision. It's a fair bit of stuff really.

// I think that tilemaps really need to be drawn outside of the main render loop, though, to avoid them
// drawing a bit too late or too early. I reckon that each viewport needs to stack up a set of calls which say
// what tilemaps it wants drawn in it and what Z order it wants them to have.

// On a side note, I think that for a multi-level game like Atic Atac it's best to built the tilemaps so that
// they genuinely stack on top of one-another with perhaps the stairs duplicated in each layer so they act
// as the swap-over point. By that means the secondary collision layer will be used in that room.

// That means that we need to be able to change what tilemap(s) each object collides with. I think it's fair
// to limit this to just 2 tilemaps otherwise things will get mad. Each tilemap can have an offset associated with
// it that affects where it's drawn and collides so it can be independant of the main world.

// Note that once we stick 3D into the games we'll need a translation stage with multiple options
// where the x,y and z are combined to form a screen_x and screen_y, it'll be from simple stuff like
// "screen_x = x and screen_y = y-z" for a simple beat 'em up to "screen_x = x-z and screen_y = y-z"
// for something like RCR and proper perspective calculations for Deactivators.



/*
void SCRIPTING_check_for_collide_type_offset_mismatch(void)
{
	int entity_id;

	entity_id = first_processed_entity_in_list;

	while (entity_id != UNSET)
	{
		if ( (entity[entity_id][ENT_COLLIDE_TYPE] == 0) && (entity[entity_id][ENT_COLLISION_BUCKET_NUMBER] >0) )
		{
			int a=0;
		}

		entity_id = entity[entity_id][ENT_NEXT_PROCESS_ENT];
	}


}



void SCRIPTING_check_for_orphans(void)
{
	int flag[MAX_ENTITIES];

	int entity_id;

	for (entity_id = 0; entity_id<MAX_ENTITIES; entity_id++)
	{
		flag[entity_id] = 0;
	}

	entity_id = first_processed_entity_in_list;

	while (entity_id != UNSET)
	{
		flag[entity_id] = 1;

		entity_id = entity[entity_id][ENT_NEXT_PROCESS_ENT];
	}

	int problem;

	for (entity_id = 0; entity_id<MAX_ENTITIES; entity_id++)
	{
		if ( (entity[entity_id][ENT_ALIVE] > 0) && (flag[entity_id] == 0) )
		{
			problem=0;
		}
	}
}



void SCRIPTING_check_for_loop_links(void)
{
	return;

	int entity_id;

	entity_id = first_processed_entity_in_list;

	int problem;

	for (entity_id = 1; entity_id<MAX_ENTITIES; entity_id++)
	{
		if (entity_id == entity[entity_id][ENT_NEXT_PROCESS_ENT])
		{
			problem = 0;
		}

		if (entity_id == entity[entity_id][ENT_NEXT_ORDER_ENT])
		{
			problem = 0;
		}

		if (entity_id == entity[entity_id][ENT_NEXT_WINDOW_ENT])
		{
			problem = 0;
		}

		if (entity_id == entity[entity_id][ENT_NEXT_COLLISION_ENT])
		{
			problem = 0;
		}
	}
}
*/





#define DEBUG_IN_NO_LIST		(0)
#define DEBUG_IN_DEAD_LIST		(1)
#define DEBUG_IN_USED_LIST		(2)
#define DEBUG_IN_LIMBO_LIST		(3)

void SCRIPTING_confirm_entity_counts (void)
{
	// This trawls through the dead list, limbo list and processing list to confirm the number of entities.

	used_entities = 0;
	limboed_entities = 0;
	free_entities = 0;
	lost_entities = 0;

	char *pointer;

	int entity_index;

	for (entity_index=0; entity_index<MAX_ENTITIES; entity_index++)
	{
		entity[entity_index][ENT_DEBUG_INFO] = DEBUG_IN_NO_LIST;
	}

	// Count entities in the USED list.

	entity_index = first_processed_entity_in_list;

	while (entity_index != UNSET)
	{
		used_entities++;
		entity[entity_index][ENT_DEBUG_INFO] = DEBUG_IN_USED_LIST;

		pointer = GPL_get_entry_name ("SCRIPTS",entity[entity_index][ENT_SCRIPT_NUMBER]);

		entity_index = entity[entity_index][ENT_NEXT_PROCESS_ENT];
	}

	// Count entities in the LIMBO list.

	entity_index = first_limbo_entity_in_list;

	while (entity_index != UNSET)
	{
		limboed_entities++;
		if (entity[entity_index][ENT_DEBUG_INFO] == DEBUG_IN_NO_LIST)
		{
			entity[entity_index][ENT_DEBUG_INFO] = DEBUG_IN_LIMBO_LIST;
		}
		else
		{
			assert(0);
		}
		entity_index = entity[entity_index][ENT_NEXT_PROCESS_ENT];
	}

	// Count entities in the DEAD list.

	entity_index = first_dead_entity_in_list;

	while (entity_index != UNSET)
	{
		free_entities++;
		if (entity[entity_index][ENT_DEBUG_INFO] == DEBUG_IN_NO_LIST)
		{
			entity[entity_index][ENT_DEBUG_INFO] = DEBUG_IN_DEAD_LIST;
		}
		else
		{
			assert(0);
		}
		entity_index = entity[entity_index][ENT_NEXT_PROCESS_ENT];
	}

	for (entity_index=0; entity_index<MAX_ENTITIES; entity_index++)
	{
		if (entity[entity_index][ENT_DEBUG_INFO] == DEBUG_IN_NO_LIST)
		{
			lost_entities++;
		}
	}

}



void SCRIPTING_setup_entity(int entity_id)
{
	memcpy (&entity[entity_id][0] , &reset_entity[0] , MAX_ENTITY_VARIABLES * sizeof(int) );
	
	entity[entity_id][ENT_OWN_ID] = entity_id;
}



void SCRIPTING_add_to_dead_list (int entity_id)
{
	if (last_dead_entity_in_list == UNSET)
	{
		// List is empty at the moment...
		entity[entity_id][ENT_PREV_PROCESS_ENT] = UNSET;
		entity[entity_id][ENT_NEXT_PROCESS_ENT] = UNSET;
		first_dead_entity_in_list = entity_id;
		last_dead_entity_in_list = entity_id;
	}
	else
	{
		// List has summat in it...
		entity[last_dead_entity_in_list][ENT_NEXT_PROCESS_ENT] = entity_id;
		entity[entity_id][ENT_PREV_PROCESS_ENT] = last_dead_entity_in_list;
		entity[entity_id][ENT_NEXT_PROCESS_ENT] = UNSET;
		last_dead_entity_in_list = entity_id;
	}

	entity_id = entity[entity_id][ENT_NEXT_PROCESS_ENT];
}



void SCRIPTING_delete_processing_order_entry (int entity_id)
{
	int *entity_pointer;

	entity_pointer = &entity[entity_id][0];

	// This function quite simply deletes the given entity from the list.

	// Except it's not so simple as it's bi-directionally linked so we need to tie its tubes.

	int next_entity_id = entity_pointer [ENT_NEXT_PROCESS_ENT];
	int previous_entity_id = entity_pointer [ENT_PREV_PROCESS_ENT];

	if ( (previous_entity_id == UNSET) && (next_entity_id == UNSET) )
	{
		// This is the only thing in the stack, deleting this means that nothing will be processed probably sending things off into an infinite loop of scary nothingness. Better stick an ASSERT on it.
		CONTROL_stop_and_save_active_channels ("last_entity_deleted_error");
		OUTPUT_message("Deleting last Entity! Nobody is left alive! You fooooool!");
		SCRIPTING_state_dump();
		assert(0);
	}
	else if (previous_entity_id == UNSET)
	{
		// We're deleting the first thing in the list, so we need to make the second one think it's the
		// first now.
		first_processed_entity_in_list = next_entity_id;
		entity[next_entity_id][ENT_PREV_PROCESS_ENT] = UNSET;
	}
	else if (next_entity_id == UNSET)
	{
		// It's the last puppy in the pile of poopoo... So we make it so the last_created_entity_in_list points
		// to the previous.
		last_processed_entity_in_list = previous_entity_id;
		entity[previous_entity_id][ENT_NEXT_PROCESS_ENT] = UNSET;
	}
	else
	{
		// It's in the middle of the pile, so we just tie up the loose ends.
		// So first of all point the next item in the queue's previous entity at the deleted entity's previous entity. Lawks.
		entity [next_entity_id][ENT_PREV_PROCESS_ENT] = previous_entity_id;
		// Then point the previous item in the queue's next entity at the deleted entity's next entity. Even lawkser!
		entity [previous_entity_id][ENT_NEXT_PROCESS_ENT] = next_entity_id;
	}

	entity_pointer [ENT_PREV_PROCESS_ENT] = UNSET;
	entity_pointer [ENT_NEXT_PROCESS_ENT] = UNSET; // Finally reset the deleted entities processing buddies back to UNSET
}



void SCRIPTING_add_to_limbo_list (int entity_id)
{
	if (last_limbo_entity_in_list == UNSET)
	{
		// List is empty at the moment...
		entity[entity_id][ENT_PREV_PROCESS_ENT] = UNSET;
		entity[entity_id][ENT_NEXT_PROCESS_ENT] = UNSET;
		first_limbo_entity_in_list = entity_id;
		last_limbo_entity_in_list = entity_id;
	}
	else
	{
		// List has summat in it...
		entity[last_limbo_entity_in_list][ENT_NEXT_PROCESS_ENT] = entity_id;
		entity[entity_id][ENT_PREV_PROCESS_ENT] = last_limbo_entity_in_list;
		entity[entity_id][ENT_NEXT_PROCESS_ENT] = UNSET;
		last_limbo_entity_in_list = entity_id;
	}

	entity_id = entity[entity_id][ENT_NEXT_PROCESS_ENT];
}



void SCRIPTING_reset_entities_and_spawn (int calling_entity, int following_script)
{
	// Oy vey! This resets the entire scripting system then restarts it with the passed in entity.

	int entity_id;

	int x = entity[calling_entity][ENT_WORLD_X];
	int y = entity[calling_entity][ENT_WORLD_Y];

	SCRIPTING_destroy_all_entity_arrays ();

	first_dead_entity_in_list = UNSET;
	last_dead_entity_in_list = UNSET;

	first_limbo_entity_in_list = UNSET;
	last_limbo_entity_in_list = UNSET;

	last_processed_entity_in_list = 0;
	first_processed_entity_in_list = UNSET;

	for (entity_id=0 ; entity_id<MAX_ENTITIES ; entity_id++)
	{
		SCRIPTING_setup_entity(entity_id);
		SCRIPTING_add_to_dead_list(entity_id);
	}

	// Also we need to wipe all the windows clean. Ho yuss.

	int w;

	for (w=0; w<number_of_windows; w++)
	{
		if (window_details[w].active == true)
		{
			OUTPUT_blank_window_contents (w);
		}
	}

	// And make the new one...

	SCRIPTING_spawn_entity_by_name (GPL_get_entry_name("SCRIPTS",following_script) , x , y , 0 , 0 , 0 );
}



void SCRIPTING_limbo_entity (int entity_id)
{
	#ifdef RETRENGINE_DEBUG_VERSION
		// Check for zombies!
		if (entity[entity_id][ENT_ALIVE]<=0)
		{
			OUTPUT_message("You cannot kill what does not live!!!");
			assert(0);
		}
	#endif

	SCRIPTING_log_focus_kill(entity_id, "limbo_entity");

	// Oh, and make sure it's taken out of the processing loop.
	SCRIPTING_delete_processing_order_entry (entity_id);

	// Also remove from the collision bucket if it was in there... And the general map bucket.
	OBCOLL_remove_from_collision_bucket (entity_id);
	OBCOLL_remove_from_general_area_bucket (entity_id);

	// And kill off any text bubble or arrays!
	FONT_destroy_current_contents(entity_id);
	ARRAY_destroy_entitys_arrays(entity_id);
	EVENT_free_queue(entity_id);

	// Then flag it as limbo'd!
	entity[entity_id][ENT_ALIVE] = -10;

	// And then add it to the limbo list...
	SCRIPTING_add_to_limbo_list(entity_id);
}



void SCRIPTING_destroy_all_entity_arrays (void)
{
	int entity_id;

	for (entity_id=0; entity_id<MAX_ENTITIES; entity_id++)
	{
		ARRAY_destroy_entitys_arrays (entity_id);
	}
}



void SCRIPTING_reset_watch_list (void)
{
	if (watch_list_size > 0)
	{
		free (watch_list_script);
		free (watch_list_variable);
		watch_list_size = 0;
	}
}



void SCRIPTING_add_entry_to_watch_list (int script_number, int variable_number)
{
	if (watch_list_size == 0)
	{
		watch_list_script = (int *) malloc (sizeof(int));
		watch_list_variable = (int *) malloc (sizeof(int));
	}
	else
	{
		watch_list_script = (int *) realloc (watch_list_script , sizeof(int) * (watch_list_size+1));
		watch_list_variable = (int *) realloc (watch_list_variable , sizeof(int) * (watch_list_size+1));
	}

	watch_list_script[watch_list_size] = script_number;
	watch_list_variable[watch_list_size] = variable_number;

	watch_list_size++;
}



void SCRIPTING_display_flag_list (int x_pos, int y_pos)
{
	int start,end;
	GPL_list_extents ("FLAG",&start,&end);
	int count = end-start;
	char text[MAX_LINE_SIZE];

	int t;

	for (t=0; t<count; t++)
	{
		snprintf(text, sizeof(text),"%3i * %s",flag_array[t],GPL_what_is_word_name(t+start));
		OUTPUT_text (x_pos,y_pos,text,0,255,0);
		y_pos += 8;
	}
}



void SCRIPTING_display_script_list (int y_pos)
{
	int start,end;
	GPL_list_extents ("SCRIPTS",&start,&end);
	int count = end-start;

	int *script_counter = (int *) malloc (sizeof(int) * count);

	int t;

	for (t=0; t<count; t++)
	{
		script_counter[t] = 0;
	}

	int entity_id;

	entity_id = first_processed_entity_in_list;
	
	while (entity_id != UNSET)
	{
		script_counter[entity [entity_id][ENT_SCRIPT_NUMBER]]++;
		entity_id = entity [entity_id][ENT_NEXT_PROCESS_ENT];
	}

	char text[MAX_LINE_SIZE];

	for (t=0; t<count; t++)
	{
		if (script_counter[t] > 0)
		{
			snprintf(text, sizeof(text),"%3i * %s",script_counter[t],GPL_what_is_word_name(t+start));
			OUTPUT_text (0,y_pos,text,0,255,0);
			y_pos += 8;
		}
	}

	free (script_counter);
}



void SCRIPTING_recursively_sleep_or_wake_entity_family (int entity_id, int alteration)
{
	// This does what is says on tin.

	int *entity_pointer = &entity[entity_id][0];

	if (alteration == ALTERATION_SLEEP)
	{
		if (entity_pointer[ENT_ALIVE] != SLEEPING)
		{
			entity_pointer[ENT_OLD_ALIVE] = entity_pointer[ENT_ALIVE];
			entity_pointer[ENT_ALIVE] = SLEEPING;

			if (entity_pointer[ENT_WAKE_LINE] != UNSET)
			{
				entity_pointer[ENT_PROGRAM_START] = entity_pointer[ENT_WAKE_LINE];
			}
		}
	}
	else
	{
		entity_pointer[ENT_ALIVE] = entity_pointer[ENT_OLD_ALIVE];
	}

	if ( (entity_pointer[ENT_NEXT_SIBLING] != UNSET) && (entity[entity_pointer[ENT_NEXT_SIBLING]][ENT_ALIVE] != entity_pointer[ENT_ALIVE]) )
	{
		SCRIPTING_recursively_sleep_or_wake_entity_family (entity_pointer[ENT_NEXT_SIBLING],alteration);
	}

	if ( (entity_pointer[ENT_PREV_SIBLING] != UNSET) && (entity[entity_pointer[ENT_PREV_SIBLING]][ENT_ALIVE] != entity_pointer[ENT_ALIVE]) )
	{
		SCRIPTING_recursively_sleep_or_wake_entity_family (entity_pointer[ENT_PREV_SIBLING],alteration);
	}

	if ( (entity_pointer[ENT_PARENT] != UNSET) && (entity[entity_pointer[ENT_PARENT]][ENT_ALIVE] != entity_pointer[ENT_ALIVE]) )
	{
		SCRIPTING_recursively_sleep_or_wake_entity_family (entity_pointer[ENT_PARENT],alteration);
	}

	if ( (entity_pointer[ENT_FIRST_CHILD] != UNSET) && (entity[entity_pointer[ENT_FIRST_CHILD]][ENT_ALIVE] != entity_pointer[ENT_ALIVE]) )
	{
		SCRIPTING_recursively_sleep_or_wake_entity_family (entity_pointer[ENT_FIRST_CHILD],alteration);
	}

}



void SCRIPTING_recursively_kill_entity_family (int entity_id)
{
	// This does what is says on tin.

	int *entity_pointer = &entity[entity_id][0];

	if (entity_id == global_next_entity)
	{
		// If we're just about to kill the entity which will be processed
		// after the current one, then update the global_next_entity.

		global_next_entity = entity_pointer[ENT_NEXT_PROCESS_ENT];
	}

	if ((entity_pointer[ENT_NEXT_SIBLING] != UNSET) && (entity[entity_pointer[ENT_NEXT_SIBLING]][ENT_ALIVE] > DEAD))
	{
		entity[entity_pointer[ENT_NEXT_SIBLING]][ENT_PREV_SIBLING] = UNSET;
		// To stop it doubling back due to the recursive nature of this function.

		SCRIPTING_recursively_kill_entity_family (entity_pointer[ENT_NEXT_SIBLING]);
	}

	if ((entity_pointer[ENT_PREV_SIBLING] != UNSET) && (entity[entity_pointer[ENT_PREV_SIBLING]][ENT_ALIVE] > DEAD))
	{
		entity[entity_pointer[ENT_PREV_SIBLING]][ENT_NEXT_SIBLING] = UNSET;
		// To stop it doubling back due to the recursive nature of this function.

		SCRIPTING_recursively_kill_entity_family (entity_pointer[ENT_PREV_SIBLING]);
	}

	if ((entity_pointer[ENT_PARENT] != UNSET) && (entity[entity_pointer[ENT_PARENT]][ENT_ALIVE] > DEAD))
	{
		entity[entity_pointer[ENT_PARENT]][ENT_FIRST_CHILD] = UNSET;
		// To stop it doubling back due to the recursive nature of this function.

		SCRIPTING_recursively_kill_entity_family (entity_pointer[ENT_PARENT]);
	}

	if ((entity_pointer[ENT_FIRST_CHILD] != UNSET) && (entity[entity_pointer[ENT_FIRST_CHILD]][ENT_ALIVE] > DEAD))
	{
		entity[entity_pointer[ENT_FIRST_CHILD]][ENT_PARENT] = UNSET;
		// To stop it doubling back due to the recursive nature of this function.

		SCRIPTING_recursively_kill_entity_family (entity_pointer[ENT_LAST_CHILD]);
	}

	if (entity_pointer[ENT_ALIVE] > DEAD)
	{
		// Because it's quite possible that it's been killed off via a child's sibling by now.
		SCRIPTING_limbo_entity (entity_id);
	}
}



bool SCRIPTING_does_alteration_affect_processing_order (int alteration)
{
	switch (alteration)
	{
	case ALTERATION_KILL:
		return true;
		break;
	default:
		return false;
		break;
	}
}



void SCRIPTING_carry_out_alteration (int entity_id, int alteration)
{
	if (alteration == ALTERATION_KILL)
	{
		SCRIPTING_limbo_entity(entity_id);
	}
	else if ( (alteration == ALTERATION_SLEEP) && (entity [entity_id][ENT_ALIVE] != SLEEPING) )
	{
		entity [entity_id][ENT_OLD_ALIVE] = entity [entity_id][ENT_ALIVE];
		entity [entity_id][ENT_ALIVE] = SLEEPING;

		if (entity [entity_id][ENT_WAKE_LINE] != UNSET)
		{
			entity [entity_id][ENT_PROGRAM_START] = entity [entity_id][ENT_WAKE_LINE];
		}
	}
	else if ( (alteration == ALTERATION_WAKE) && (entity [entity_id][ENT_ALIVE] == SLEEPING) )
	{
		entity [entity_id][ENT_ALIVE] = entity [entity_id][ENT_OLD_ALIVE];
	}
	else if ( (alteration == ALTERATION_FREEZE) && (entity [entity_id][ENT_ALIVE] != FROZEN) )
	{
		entity [entity_id][ENT_OLD_ALIVE] = entity [entity_id][ENT_ALIVE];
		entity [entity_id][ENT_ALIVE] = FROZEN;
	}
	else if ( (alteration == ALTERATION_UNFREEZE) && (entity [entity_id][ENT_ALIVE] == FROZEN) )
	{
		entity [entity_id][ENT_ALIVE] = entity [entity_id][ENT_OLD_ALIVE];
	}
}



void SCRIPTING_start_tracer_script (void)
{
	// Just erases the tracer output file, and starts a new one.

	remove (MAIN_get_project_filename("traced_script_lines.txt"));
}



void SCRIPTING_free_tracer_script (void)
{
	if (tracer_output_script != NULL)
	{
		int l;
		
		for (l=0;l<tracer_output_script_length;l++)
		{
			free (tracer_output_script[l].line_text);
		}

		free (tracer_output_script);
		tracer_output_script = NULL;
	}
	else
	{
		// WTF?
		OUTPUT_message("Attempting to free tracer script when already freed");
	}
}



void SCRIPTING_output_tracer_script_to_file (bool called_from_script)
{
	char line[MAX_LINE_SIZE];

	if (tracer_output_script != NULL)
	{
		FILE *fp = fopen (MAIN_get_project_filename("traced_script_lines.txt"),"w");
		int l,e;

		if (fp != NULL)
		{
			snprintf(line, sizeof(line),"Total entities traced = %i.\n\n",tracer_output_entity_list_size);
			fputs(line,fp);

			if (complete_trace_from_now_on == true)
			{
				for (l=0;l<tracer_output_script_length;l++)
				{
					if (tracer_output_script[l].line_text[0] != '-')
					{
						fputc('\t',fp);
					}
					fputs(tracer_output_script[l].line_text,fp);
				}
			}
			else
			{
				for (e=0; e<tracer_output_entity_list_size; e++)
				{
					snprintf(line, sizeof(line),"Entity %i tracer lines.\n\n",tracer_output_entity_list[e]);
					fputs(line,fp);

					for (l=0;l<tracer_output_script_length;l++)
					{
						if (tracer_output_script[l].line_owner == tracer_output_entity_list[e])
						{
							fputc('\t',fp);
							fputs(tracer_output_script[l].line_text,fp);
						}
					}
				}
			}

			fclose (fp);
		}
		else
		{
			OUTPUT_message("Unable to write tracer script file!");
		}

		if (called_from_script == false)
		{
			SCRIPTING_free_tracer_script ();
		}
	}

}


void SCRIPTING_add_entity_id_to_tracer_entity_list (int entity_id)
{
	if (tracer_output_entity_list == NULL)
	{
		// List is empty so create a new one and put in the default
		// value of UNSET and then the first genuine entity.

		tracer_output_entity_list = (int *) malloc (sizeof(int) * 2);
		tracer_output_entity_list[0] = entity_id;
		tracer_output_entity_list_size = 1;
	}
	else
	{
		// List has some stuff in it, golly! Check to see if we have a dupe.
		// We can ignore the first entry as it's the special value of UNSET
		// which is assigned to the header.

		int e;
		bool found_one = false;

		for (e=0; e<tracer_output_entity_list_size; e++)
		{
			if (tracer_output_entity_list[e] == entity_id)
			{
				found_one = true;
			}
		}

		if (found_one == false)
		{
			tracer_output_entity_list = (int *) realloc (tracer_output_entity_list , sizeof(int) * (tracer_output_entity_list_size+1) );
			tracer_output_entity_list[tracer_output_entity_list_size] = entity_id;
			tracer_output_entity_list_size++;
		}

	}
}



void SCRIPTING_add_tracer_script_header (int entity_id)
{
	// Adds a header to the tracer output file in readiness for the coming stuff.

	char line[MAX_LINE_SIZE];
	static int last_frame_added = UNSET;

	if (last_frame_added != frames_so_far)
	{
		snprintf(line, sizeof(line), "-------- START OF NEW FRAME %i--------\n\n",frames_so_far);
		SCRIPTING_add_output_tracer_script_line (line,UNSET);
		last_frame_added = frames_so_far;
	}

	SCRIPTING_add_entity_id_to_tracer_entity_list (entity_id);

	snprintf(line, sizeof(line), "Script '%s' (Entity %i) on frame %i...\n\n",GPL_get_entry_name("SCRIPTS",entity[entity_id][ENT_SCRIPT_NUMBER]),entity_id,frames_so_far);
	SCRIPTING_add_output_tracer_script_line (line,entity_id);
}



void SCRIPTING_add_tracer_script_ender (int entity_id)
{
	// Caps off the tracer output file after a bunch of script lines.

	SCRIPTING_add_output_tracer_script_line("\n\n\n",entity_id);
}



void SCRIPTING_calc_arb_line_lengths (int entity_id)
{
	arbitrary_quads[entity_id].line_lengths[0] = MATH_get_distance_float (arbitrary_quads[entity_id].x[0], arbitrary_quads[entity_id].y[0], arbitrary_quads[entity_id].x[1], arbitrary_quads[entity_id].y[1]);
	arbitrary_quads[entity_id].line_lengths[1] = MATH_get_distance_float (arbitrary_quads[entity_id].x[1], arbitrary_quads[entity_id].y[1], arbitrary_quads[entity_id].x[3], arbitrary_quads[entity_id].y[3]);
	arbitrary_quads[entity_id].line_lengths[2] = MATH_get_distance_float (arbitrary_quads[entity_id].x[3], arbitrary_quads[entity_id].y[3], arbitrary_quads[entity_id].x[2], arbitrary_quads[entity_id].y[2]);
	arbitrary_quads[entity_id].line_lengths[3] = MATH_get_distance_float (arbitrary_quads[entity_id].x[2], arbitrary_quads[entity_id].y[2], arbitrary_quads[entity_id].x[0], arbitrary_quads[entity_id].y[0]);
}



void SCRIPTING_set_tracer_script_before_values (int entity_id, int line_number)
{
	int argument_number;

	if (tracer_script == NULL)
	{
		return;
	}

	if ((line_number < 0) || (line_number >= tracer_script_length))
	{
		return;
	}
	int line_length = tracer_script[line_number].line_length;

	for (argument_number=0; argument_number<line_length; argument_number++)
	{
		if (tracer_script[line_number].argument_list[argument_number].argument_type_bitflag & (TRACER_ARGUMENT_TYPE_VARIABLE|TRACER_ARGUMENT_TYPE_PARAMETER|TRACER_ARGUMENT_TYPE_FIXED_VALUE|TRACER_ARGUMENT_TYPE_OPERAND) )
		{
			tracer_before_values[argument_number] = SCRIPTING_get_int_value(entity_id,line_number,argument_number);
		}
	}
}



int SCRIPTING_calculate_pan_from_position (int entity_id)
{
	int x = entity[entity_id][ENT_WORLD_X];
	int window_number = entity[entity_id][ENT_WINDOW_NUMBER];
	int window_start_x = window_details[window_number].current_x;
	int window_end_x = window_start_x + window_details[window_number].width;

	float percentage = MATH_unlerp(float(window_start_x), float(window_end_x), float(x));

	if (percentage < 0.0f)
	{
		percentage = 0.0f;
	}
	else if (percentage > 1.0f)
	{
		percentage = 1.0f;
	}

	return (int(percentage * 255.0f));
}



void SCRIPTING_set_tracer_script_after_values (int entity_id, int line_number)
{
	int argument_number;

	if (tracer_script == NULL)
	{
		return;
	}

	if ((line_number < 0) || (line_number >= tracer_script_length))
	{
		return;
	}

	int line_length = tracer_script[line_number].line_length;

	for (argument_number=0; argument_number<line_length; argument_number++)
	{
		if (tracer_script[line_number].argument_list[argument_number].argument_type_bitflag & (TRACER_ARGUMENT_TYPE_VARIABLE|TRACER_ARGUMENT_TYPE_PARAMETER|TRACER_ARGUMENT_TYPE_FIXED_VALUE|TRACER_ARGUMENT_TYPE_OPERAND) )
		{
			tracer_after_values[argument_number] = SCRIPTING_get_int_value(entity_id,line_number,argument_number);
		}
	}
}



void SCRIPTING_output_tracer_script_line (int line_number, int new_line_number, int entity_id)
{
	// This uses the pre-supplied values to fill out a text line explaining what's going on.

	int argument_number;
	if (tracer_script == NULL)
	{
		return;
	}

	if ((line_number < 0) || (line_number >= tracer_script_length))
	{
		return;
	}

	int line_length = tracer_script[line_number].line_length;

	char line[TEXTFILE_SUPER_SIZE];
	char word[MAX_LINE_SIZE];

	snprintf(line, sizeof(line), "%5.0i:\t",line_number);

	int t;

	for (t=0; t<tracer_script[line_number].line_indentation; t++)
	{
		strcat (line,"\t");	
	}

	for (argument_number=0; argument_number<line_length; argument_number++)
	{
		if (tracer_script[line_number].argument_list[argument_number].argument_type_bitflag != UNSET)
		{
			if (tracer_script[line_number].argument_list[argument_number].argument_type_bitflag & (TRACER_ARGUMENT_TYPE_VARIABLE) )
			{
				snprintf ( word , sizeof(word), "%s(%i -> %i) " , tracer_script[line_number].argument_list[argument_number].text_word , tracer_before_values[argument_number] , tracer_after_values[argument_number] );
			}
			else if (tracer_script[line_number].argument_list[argument_number].argument_type_bitflag & (TRACER_ARGUMENT_TYPE_PARAMETER|TRACER_ARGUMENT_TYPE_FIXED_VALUE|TRACER_ARGUMENT_TYPE_OPERAND) )
			{
				snprintf ( word , sizeof(word), "%s(%i) " , tracer_script[line_number].argument_list[argument_number].text_word , tracer_before_values[argument_number] );
			}
			else
			{
				snprintf ( word , sizeof(word), "%s " , tracer_script[line_number].argument_list[argument_number].text_word );
			}

			strcat(line,word);
		}
	}

	if (tracer_script[line_number].line_type_bitflag & TRACER_LINE_TYPE_PROGRAM_FLOW)
	{
		if (line_number != new_line_number)
		{
			snprintf(word, sizeof(word), "(Check Failed - heading to line %i)\n",new_line_number+1);
			strcat(line,word);
		}
	}
	else if (tracer_script[line_number].line_type_bitflag & TRACER_LINE_TYPE_REDIRECT)
	{
		snprintf(word, sizeof(word), "(Heading to line %i)\n",new_line_number+1);
		strcat(line,word);
	}

	strcat (line,"\n");

	SCRIPTING_add_output_tracer_script_line (line,entity_id);
}



int SCRIPTING_alter_entities_by_criteria_new (int calling_entity_id,int alteration,int variable,int operation ,int compare_value)
{
	int previous_kill_source_entity = scripting_kill_source_entity;
	int previous_kill_source_line = scripting_kill_source_line;
	scripting_kill_source_entity = calling_entity_id;
	scripting_kill_source_line = UNSET;

	int calling_entity_follower = entity[calling_entity_id][ENT_NEXT_PROCESS_ENT];
	int override_next_entity = VERY_UNSET;
	bool search_for_next_survivor = false;
	int next_entity_id;
	int entity_id = first_processed_entity_in_list;
	int result;
	int value;
	bool processing_order_altered = SCRIPTING_does_alteration_affect_processing_order (alteration);

	int counter = 0;

	while (entity_id != UNSET)
	{
		next_entity_id = entity [entity_id][ENT_NEXT_PROCESS_ENT];

		value = entity[entity_id][variable];

		result = false;

		switch (operation)
		{

		case CMP_LESS_THAN:
			result = (value < compare_value);
			break;

		case CMP_LESS_THAN_OR_EQUAL:
			result = (value <= compare_value);
			break;

		case CMP_EQUAL:
			result = (value == compare_value);
			break;

		case CMP_GREATER_THAN_OR_EQUAL:
			result = (value >= compare_value);
			break;

		case CMP_GREATER_THAN:
			result = (value > compare_value);
			break;

		case CMP_UNEQUAL:
			result = (value != compare_value);
			break;

		case CMP_BITWISE_AND:
			result = (value & compare_value);
			break;

		case CMP_BITWISE_OR:
			result = (value | compare_value);
			break;

		case CMP_NOT_BITWISE_AND:
			result = !(value & compare_value);
			break;

		case CMP_NOT_BITWISE_OR:
			result = !(value | compare_value);
			break;

		default:
			break;

		}

		if (result)
		{
			SCRIPTING_carry_out_alteration(entity_id,alteration);
			counter++;

			if (entity_id == calling_entity_follower)
			{
				// We just altered the next entity that was gonna' be processed in the main process_entities
				// function so we have to override the global_next_entity with the next survivor.
				search_for_next_survivor = processing_order_altered;
			}
		}
		else
		{
			if (search_for_next_survivor)
			{
				search_for_next_survivor = false;
				override_next_entity = entity_id;
			}
		}

		entity_id = next_entity_id;
	}

	if (search_for_next_survivor == true)
	{
		override_next_entity = UNSET;
	}

	scripting_kill_source_entity = previous_kill_source_entity;
	scripting_kill_source_line = previous_kill_source_line;
	
	return override_next_entity;
}



int SCRIPTING_alter_entities_by_criteria (int criteria,int value,int calling_entity_id, int alteration)
{
	int previous_kill_source_entity = scripting_kill_source_entity;
	int previous_kill_source_line = scripting_kill_source_line;
	scripting_kill_source_entity = calling_entity_id;
	scripting_kill_source_line = UNSET;

	int calling_entity_follower = entity[calling_entity_id][ENT_NEXT_PROCESS_ENT];
	int override_next_entity = VERY_UNSET;
	bool search_for_next_survivor = false;
	int next_entity_id;
	int entity_id;
	bool processing_order_altered = SCRIPTING_does_alteration_affect_processing_order (alteration);

	int counter = 0;

	switch (criteria)
	{
	case ESC_SELECT_BY_PARENT:
		break;

	case ESC_SELECT_BY_MATRIARCH:
		// Destroy everything with the same matriarch. Similar to the destroy
		// family one.
		entity_id = first_processed_entity_in_list;
		
		while (entity_id != UNSET)
		{
			next_entity_id = entity [entity_id][ENT_NEXT_PROCESS_ENT];

			if (entity [entity_id][ENT_MATRIARCH] == value)
			{
				SCRIPTING_carry_out_alteration(entity_id,alteration);

				if (entity_id == calling_entity_follower)
				{
					// We just deleted the next entity that was gonna' be processed in the main process_entities
					// function so we have to override the global_next_entity with the next survivor.
					search_for_next_survivor = processing_order_altered;
				}
			}
			else
			{
				if (search_for_next_survivor)
				{
					search_for_next_survivor = false;
					override_next_entity = entity_id;
				}
			}

			entity_id = next_entity_id;
		}
		break;

	case ESC_SELECT_BY_PRIORITY:
		// Destroy everything of a lower or equal priority...
		entity_id = first_processed_entity_in_list;
		
		while (entity_id != UNSET)
		{
			next_entity_id = entity [entity_id][ENT_NEXT_PROCESS_ENT];

			if (entity [entity_id][ENT_PRIORITY] <= value)
			{
				SCRIPTING_carry_out_alteration(entity_id,alteration);

				if (entity_id == calling_entity_follower)
				{
					// We just deleted the next entity that was gonna' be processed in the main process_entities
					// function so we have to override the global_next_entity with the next survivor.
					search_for_next_survivor = processing_order_altered;
				}
			}
			else
			{
				if (search_for_next_survivor)
				{
					search_for_next_survivor = false;
					override_next_entity = entity_id;
				}
			}

			entity_id = next_entity_id;
		}
		break;

	case ESC_SELECT_BY_SPECIFIC_ENTITY_TYPE:
		// Destroy things with an exact entity type match.
		entity_id = first_processed_entity_in_list;
		
		while (entity_id != UNSET)
		{
			next_entity_id = entity [entity_id][ENT_NEXT_PROCESS_ENT];

			if ( (entity [entity_id][ENT_ENTITY_TYPE] & value) == value )
			{
				SCRIPTING_carry_out_alteration(entity_id,alteration);

				if (entity_id == calling_entity_follower)
				{
					// We just deleted the next entity that was gonna' be processed in the main process_entities
					// function so we have to override the global_next_entity with the next survivor.
					search_for_next_survivor = processing_order_altered;
				}
			}
			else
			{
				if (search_for_next_survivor)
				{
					search_for_next_survivor = false;
					override_next_entity = entity_id;
				}
			}

			entity_id = next_entity_id;
		}
		break;

	case ESC_SELECT_BY_COMPLETE_ENTITY_TYPE:
		// Destroy things with all the same entity type bitmasks.
		entity_id = first_processed_entity_in_list;
		
		while (entity_id != UNSET)
		{
			next_entity_id = entity [entity_id][ENT_NEXT_PROCESS_ENT];

			if (entity [entity_id][ENT_ENTITY_TYPE] == value)
			{
				SCRIPTING_carry_out_alteration(entity_id,alteration);

				if (entity_id == calling_entity_follower)
				{
					// We just deleted the next entity that was gonna' be processed in the main process_entities
					// function so we have to override the global_next_entity with the next survivor.
					search_for_next_survivor = processing_order_altered;
				}
			}
			else
			{
				if (search_for_next_survivor)
				{
					search_for_next_survivor = false;
					override_next_entity = entity_id;
				}
			}

			entity_id = next_entity_id;
		}
		break;

	case ESC_SELECT_BY_PARTIAL_ENTITY_TYPE:
		// Delete entities with any type overlap.
		entity_id = first_processed_entity_in_list;
		
		while (entity_id != UNSET)
		{
			next_entity_id = entity [entity_id][ENT_NEXT_PROCESS_ENT];

			if (entity [entity_id][ENT_ENTITY_TYPE] & value)
			{
				SCRIPTING_carry_out_alteration(entity_id,alteration);
				counter++;

				if (entity_id == calling_entity_follower)
				{
					// We just deleted the next entity that was gonna' be processed in the main process_entities
					// function so we have to override the global_next_entity with the next survivor.
					search_for_next_survivor = processing_order_altered;
				}
			}
			else
			{
				if (search_for_next_survivor)
				{
					search_for_next_survivor = false;
					override_next_entity = entity_id;
				}
			}

			entity_id = next_entity_id;
		}
		break;

	case ESC_SELECT_BY_DIFFERING_ENTITY_TYPE:
		// Delete entities with any type overlap.
		entity_id = first_processed_entity_in_list;

		while (entity_id != UNSET)
		{
			next_entity_id = entity [entity_id][ENT_NEXT_PROCESS_ENT];

			if ((entity [entity_id][ENT_ENTITY_TYPE] & value) == 0)
			{
				SCRIPTING_carry_out_alteration(entity_id,alteration);

				if (entity_id == calling_entity_follower)
				{
					// We just deleted the next entity that was gonna' be processed in the main process_entities
					// function so we have to override the global_next_entity with the next survivor.
					search_for_next_survivor = processing_order_altered;
				}
			}
			else
			{
				if (search_for_next_survivor)
				{
					search_for_next_survivor = false;
					override_next_entity = entity_id;
				}
			}

			entity_id = next_entity_id;
		}
		break;

	case ESC_SELECT_BY_GENOCIDE:
		// Kill everything but the calling entity. Useful when moving from
		// one part of the game to another, like from the main menu to the
		// game.
		entity_id = first_processed_entity_in_list;
		
		while (entity_id != UNSET)
		{
			next_entity_id = entity [entity_id][ENT_NEXT_PROCESS_ENT];

			if ( (entity_id != calling_entity_id) )
			{
				SCRIPTING_carry_out_alteration(entity_id,alteration);

				if (entity_id == calling_entity_follower)
				{
					// We just deleted the next entity that was gonna' be processed in the main process_entities
					// function so we have to override the global_next_entity with the next survivor.
					search_for_next_survivor = processing_order_altered;
				}
			}
			else
			{
				if (search_for_next_survivor)
				{
					search_for_next_survivor = false;
					override_next_entity = entity_id;
				}
			}

			entity_id = next_entity_id;
		}
		break;

	case ESC_SELECT_BY_FAMILY:
		if (alteration == ALTERATION_KILL)
		{
			SCRIPTING_recursively_kill_entity_family (calling_entity_id);
		}
		else 
		{
			SCRIPTING_recursively_sleep_or_wake_entity_family (calling_entity_id, alteration);
		}
		break;

	case ESC_SELECT_BY_WINDOW:
		entity_id = first_processed_entity_in_list;
		
		while (entity_id != UNSET)
		{
			next_entity_id = entity [entity_id][ENT_NEXT_PROCESS_ENT];

			if (entity[entity_id][ENT_WINDOW_NUMBER] == value)
			{
				SCRIPTING_carry_out_alteration(entity_id,alteration);

				if (entity_id == calling_entity_follower)
				{
					// We just deleted the next entity that was gonna' be processed in the main process_entities
					// function so we have to override the global_next_entity with the next survivor.
					search_for_next_survivor = processing_order_altered;
				}
			}
			else
			{
				if (search_for_next_survivor)
				{
					search_for_next_survivor = false;
					override_next_entity = entity_id;
				}
			}

			entity_id = next_entity_id;
		}
		break;

	case ESC_SELECT_BY_SCRIPT_NUMBER:
		entity_id = first_processed_entity_in_list;
		
		while (entity_id != UNSET)
		{
			next_entity_id = entity [entity_id][ENT_NEXT_PROCESS_ENT];

			if (entity[entity_id][ENT_SCRIPT_NUMBER] == value)
			{
				SCRIPTING_carry_out_alteration(entity_id,alteration);
				
				if (entity_id != calling_entity_id)
				{
					// We just deleted the next entity that was gonna' be processed in the main process_entities
					// function so we have to override the global_next_entity with the next survivor.
					search_for_next_survivor = processing_order_altered;
				}
			}
			else
			{
				if (search_for_next_survivor)
				{
					search_for_next_survivor = false;
					override_next_entity = entity_id;
				}
			}

			entity_id = next_entity_id;
		}
		break;

	case ESC_SELECT_BY_SLEEPING:
		entity_id = first_processed_entity_in_list;
		
		while (entity_id != UNSET)
		{
			next_entity_id = entity [entity_id][ENT_NEXT_PROCESS_ENT];

			if (entity[entity_id][ENT_ALIVE] == SLEEPING)
			{
				SCRIPTING_carry_out_alteration(entity_id,alteration);

				if (entity_id != calling_entity_id)
				{
					// We just deleted the next entity that was gonna' be processed in the main process_entities
					// function so we have to override the global_next_entity with the next survivor.
					search_for_next_survivor = processing_order_altered;
				}
			}
			else
			{
				if (search_for_next_survivor)
				{
					search_for_next_survivor = false;
					override_next_entity = entity_id;
				}
			}

			entity_id = next_entity_id;
		}
		break;
		


	case ESC_SELECT_BY_MATCHING_SEARCH:
		entity_id = first_processed_entity_in_list;
		
		while (entity_id != UNSET)
		{
			next_entity_id = entity [entity_id][ENT_NEXT_PROCESS_ENT];

			if (entity_search_inclusion[entity_id] == true)
			{
				SCRIPTING_carry_out_alteration(entity_id,alteration);

				if (entity_id != calling_entity_id)
				{
					// We just deleted the next entity that was gonna' be processed in the main process_entities
					// function so we have to override the global_next_entity with the next survivor.
					search_for_next_survivor = processing_order_altered;
				}
			}
			else
			{
				if (search_for_next_survivor)
				{
					search_for_next_survivor = false;
					override_next_entity = entity_id;
				}
			}

			entity_id = next_entity_id;
		}
		break;

	default:
		assert(0);
		break;
	}

	if (search_for_next_survivor == true)
	{
		override_next_entity = UNSET;
	}

	scripting_kill_source_entity = previous_kill_source_entity;
	scripting_kill_source_line = previous_kill_source_line;
	
	return override_next_entity;
	
}



void SCRIPTING_process_limbo_list (void)
{
	int entity_id = first_limbo_entity_in_list;

	while (entity_id != UNSET)
	{
		int next_entity_id = entity[entity_id][ENT_NEXT_PROCESS_ENT];
		int previous_entity_id = entity[entity_id][ENT_PREV_PROCESS_ENT];

		int life = entity[entity_id][ENT_ALIVE];

		if (life < 0)
		{
			entity[entity_id][ENT_ALIVE] = life + 1; // advance towards 0
			life = entity[entity_id][ENT_ALIVE];
		}

		if (life == 0)
		{
			// unlink from limbo list
			if (previous_entity_id == UNSET)
			{
				if (next_entity_id == UNSET)
				{
					first_limbo_entity_in_list = UNSET;
					last_limbo_entity_in_list = UNSET;
				}
				else
				{
					entity[next_entity_id][ENT_PREV_PROCESS_ENT] = UNSET;
					first_limbo_entity_in_list = next_entity_id;
				}
			}
			else
			{
				if (next_entity_id == UNSET)
				{
					entity[previous_entity_id][ENT_NEXT_PROCESS_ENT] = UNSET;
					last_limbo_entity_in_list = previous_entity_id;
				}
				else
				{
					entity[next_entity_id][ENT_PREV_PROCESS_ENT] = previous_entity_id;
					entity[previous_entity_id][ENT_NEXT_PROCESS_ENT] = next_entity_id;
				}
			}

			SCRIPTING_add_to_dead_list(entity_id);
		}

		entity_id = next_entity_id;
	}
}



void SCRIPTING_setup_data_stores (void)
{
	// Called at very start to set them up...
	int t;

	for (t=0; t<MAX_ENTITIES; t++)
	{
		data_store[t].number_of_tables = 0;
		data_store[t].table_list = NULL;
	}
}



void SCRIPTING_add_table (int entity_number, int table_id , int table_width , int table_height=1 , int table_depth=1 )
{
	int number_of_tables = data_store[entity_number].number_of_tables;
	
	if (table_width == 0)
	{
		return;
		// In case you're an idiot who passes in a size of 0.
	}

	if (number_of_tables == 0)
	{
		data_store[entity_number].table_list = (data_store_table_struct *) malloc ( sizeof(data_store_table_struct) * (number_of_tables+1) );
	}
	else
	{
		data_store[entity_number].table_list = (data_store_table_struct *) realloc ( data_store[entity_number].table_list , sizeof(data_store_table_struct) * (number_of_tables+1) );
	}

	data_store[entity_number].table_list[number_of_tables].table_data_size = table_width*table_height*table_depth;
	data_store[entity_number].table_list[number_of_tables].table_width = table_width;
	data_store[entity_number].table_list[number_of_tables].table_height = table_height;
	data_store[entity_number].table_list[number_of_tables].table_depth = table_depth;
	data_store[entity_number].table_list[number_of_tables].table_id = table_id;

	data_store[entity_number].table_list[number_of_tables].data = (int *) malloc (sizeof(int) * data_store[entity_number].table_list[number_of_tables].table_data_size);

	int t;

	for (t=0; t<data_store[entity_number].table_list[number_of_tables].table_data_size; t++)
	{
		data_store[entity_number].table_list[number_of_tables].data[t] = 0;
	}
	
	data_store[entity_number].number_of_tables++;
}



void SCRIPTING_destroy_tables (int entity_number)
{
	int number_of_tables = data_store[entity_number].number_of_tables;
	int t;

	for (t=0; t<number_of_tables; t++)
	{
		free (data_store[entity_number].table_list[t].data);
	}

	free (data_store[entity_number].table_list);

	data_store[entity_number].number_of_tables = 0;
}



void SCRIPTING_put_value_into_table (int entity_number, int table_id, int value, int x, int y=0, int z=0)
{
	int table;

	for (table=0; table<data_store[entity_number].number_of_tables; table++)
	{
		if (data_store[entity_number].table_list[table].table_id == table_id)
		{
			data_store_table_struct *dsp = &data_store[entity_number].table_list[table];

			if (dsp->table_width <= x)
			{
				return;
			}

			if (dsp->table_height <= y)
			{
				return;
			}

			if (dsp->table_depth <= z)
			{
				return;
			}

			int offset = (z * dsp->table_height * dsp->table_width) + (y * dsp->table_width) + x;

			dsp->data[offset] = value;
		}
	}

}



int SCRIPTING_get_value_from_table (int entity_number, int table_id, int x, int y=0, int z=0)
{
	int table;

	for (table=0; table<data_store[entity_number].number_of_tables; table++)
	{
		if (data_store[entity_number].table_list[table].table_id == table_id)
		{
			data_store_table_struct *dsp = &data_store[entity_number].table_list[table];

			if (dsp->table_width <= x)
			{
				return UNSET;
			}

			if (dsp->table_height <= y)
			{
				return UNSET;
			}

			if (dsp->table_depth <= z)
			{
				return UNSET;
			}

			int offset = (z * dsp->table_height * dsp->table_width) + (y * dsp->table_width) + x;

			return dsp->data[offset];
		}
	}

	return UNSET;
}



void SCRIPTING_create_windows (int window_count)
{
	// Just creates a number of windows and sets their pointers to NULL.
	// Should only be called once at the start of the program when you decide
	// how many windows you'd like.

	if ( (number_of_windows == 0) && (window_details == NULL) )
	{
		window_details = (window_struct *) malloc ( (window_count+1) * sizeof(window_struct) );
	}
	else
	{
		if (window_count > 0)
		{
			window_details = (window_struct *) realloc ( window_details , (window_count+1) * sizeof(window_struct) );
		}
		else
		{
			free (window_details);
		}
	}

	int window_number;

	for (window_number=0;window_number<window_count;window_number++)
	{
		window_details[window_number].active = false;
		window_details[window_number].new_active = false;

		window_details[window_number].buffer_size = UNSET;
		window_details[window_number].buffered_br_x = 0;
		window_details[window_number].buffered_br_y = 0;
		window_details[window_number].buffered_tl_x = 0;
		window_details[window_number].buffered_tl_y = 0;

		window_details[window_number].current_x = 0;
		window_details[window_number].current_y = 0;

		window_details[window_number].new_x = 0;
		window_details[window_number].new_y = 0;

		window_details[window_number].skip_me_this_frame = 0;

		window_details[window_number].new_screen_x = 0;
		window_details[window_number].new_screen_y = 0;
		window_details[window_number].screen_x = 0;
		window_details[window_number].screen_y = 0;

			window_details[window_number].new_opengl_scale_x = 1.0f;
			window_details[window_number].new_opengl_scale_y = 1.0f;
			window_details[window_number].fpos_x = 0.0f;
			window_details[window_number].fpos_y = 0.0f;

		window_details[window_number].new_width = 0;
		window_details[window_number].new_height = 0;
		window_details[window_number].scaled_width = 0;
		window_details[window_number].scaled_height = 0;
		window_details[window_number].width = 0;
		window_details[window_number].height = 0;

		window_details[window_number].vertex_red = 0;
		window_details[window_number].vertex_green = 0;
		window_details[window_number].vertex_blue = 0;
		window_details[window_number].secondary_window_colour = false;

		window_details[window_number].y_ordering_resolution = 0;
		window_details[window_number].y_ordering_list_size = 0;
		window_details[window_number].y_ordering_list_starts = NULL;
		window_details[window_number].y_ordering_list_ends = NULL;

		window_details[window_number].z_ordering_list_size = 0;
		window_details[window_number].z_ordering_list_starts = NULL;
		window_details[window_number].z_ordering_list_ends = NULL;

		window_details[window_number].opengl_transform_x = 0.0f;
		window_details[window_number].opengl_transform_y = 0.0f;

		window_details[window_number].opengl_scale_x = 1.0f;
		window_details[window_number].opengl_scale_y = 1.0f;
	}

	number_of_windows = window_count;
}

static bool SCRIPTING_is_valid_window_index(int window_number)
{
	return (window_number >= 0) && (window_number < number_of_windows);
}



void SCRIPTING_set_up_window (int window_number, int screen_x, int screen_y, int width, int height, int buffer_size, int z_ordering_list_size, int y_ordering_resolution)
{
	// Sets up the buffers and stuff that the windows use, freeing old ones if necessary.

	if (SCRIPTING_is_valid_window_index(window_number))
	{
		window_details[window_number].active = false;
		window_details[window_number].new_active = false;

		window_details[window_number].buffer_size = buffer_size;
		window_details[window_number].buffered_br_x = 0;
		window_details[window_number].buffered_br_y = 0;
		window_details[window_number].buffered_tl_x = 0;
		window_details[window_number].buffered_tl_y = 0;

		window_details[window_number].current_x = 0;
		window_details[window_number].current_y = 0;

		window_details[window_number].new_x = 0;
		window_details[window_number].new_y = 0;

		window_details[window_number].skip_me_this_frame = 0;

		window_details[window_number].new_screen_x = float(screen_x);
		window_details[window_number].new_screen_y = float(screen_y);
		window_details[window_number].screen_x = float(screen_x);
		window_details[window_number].screen_y = float(screen_y);

		window_details[window_number].new_width = width;
		window_details[window_number].new_height = height;
		window_details[window_number].width = width;
		window_details[window_number].height = height;
		window_details[window_number].new_opengl_scale_x = 1.0f;
		window_details[window_number].new_opengl_scale_y = 1.0f;
		window_details[window_number].opengl_scale_x = 1.0f;
		window_details[window_number].opengl_scale_y = 1.0f;
		window_details[window_number].fpos_x = 0.0f;
		window_details[window_number].fpos_y = 0.0f;

		window_details[window_number].vertex_red = 256;
		window_details[window_number].vertex_green = 256;
		window_details[window_number].vertex_blue = 256;
		window_details[window_number].secondary_window_colour = false;

		window_details[window_number].y_ordering_resolution = y_ordering_resolution;
		window_details[window_number].y_ordering_list_size = ((window_details[window_number].height + (2 * window_details[window_number].buffer_size)) >> y_ordering_resolution) + 1;

		if ( (window_details[window_number].y_ordering_list_starts == NULL) && (window_details[window_number].y_ordering_list_ends == NULL) )
		{
			window_details[window_number].y_ordering_list_starts = (int *) malloc ( window_details[window_number].y_ordering_list_size  * sizeof (int) );
			window_details[window_number].y_ordering_list_ends = (int *) malloc ( window_details[window_number].y_ordering_list_size  * sizeof (int) );
		}
		else
		{
			window_details[window_number].y_ordering_list_starts = (int *) realloc ( window_details[window_number].y_ordering_list_starts , window_details[window_number].y_ordering_list_size  * sizeof (int) );
			window_details[window_number].y_ordering_list_ends = (int *) realloc ( window_details[window_number].y_ordering_list_ends , window_details[window_number].y_ordering_list_size  * sizeof (int) );
		}

		window_details[window_number].z_ordering_list_size = z_ordering_list_size;
		
		if ( (window_details[window_number].z_ordering_list_starts == NULL) && (window_details[window_number].z_ordering_list_ends == NULL) )
		{
			window_details[window_number].z_ordering_list_starts = (int *) malloc ( window_details[window_number].z_ordering_list_size * sizeof (int) );
			window_details[window_number].z_ordering_list_ends = (int *) malloc ( window_details[window_number].z_ordering_list_size * sizeof (int) );
		}
		else
		{
			window_details[window_number].z_ordering_list_starts = (int *) realloc ( window_details[window_number].z_ordering_list_starts , window_details[window_number].z_ordering_list_size * sizeof (int) );
			window_details[window_number].z_ordering_list_ends = (int *) realloc ( window_details[window_number].z_ordering_list_ends , window_details[window_number].z_ordering_list_size * sizeof (int) );
		}

		int t;
		
		for (t=0; t<window_details[window_number].y_ordering_list_size; t++)
		{
			window_details[window_number].y_ordering_list_starts[t] = UNSET;
			window_details[window_number].y_ordering_list_ends[t] = UNSET;
		}
		
		for (t=0; t<window_details[window_number].z_ordering_list_size; t++)
		{
			window_details[window_number].z_ordering_list_starts[t] = UNSET;
			window_details[window_number].z_ordering_list_ends[t] = UNSET;
		}
	}
	else
	{
		OUTPUT_message("Trying to activate a non-existant window number!");
		assert(0);
	}

}



void SCRIPTING_activate_window (int window_number)
{
	// Sets a boolean to true in the window allowing it to be drawn and stuff.

	if (SCRIPTING_is_valid_window_index(window_number))
	{
		if ( (window_details[window_number].y_ordering_list_starts != NULL) && (window_details[window_number].y_ordering_list_ends != NULL) )
		{
			// Only allows you to activate a window if it's been set up.
			window_details[window_number].new_active = true;
		}
		else
		{
			OUTPUT_message("Trying to activate a window before setting it up!");
			assert(0);
		}
	}
	else
	{
		OUTPUT_message("Trying to activate a non-existant window number!");
		assert(0);
	}
}



void SCRIPTING_set_new_window_status ()
{
	int window_number;

	for (window_number=0; window_number<number_of_windows;window_number++)
	{
		window_details[window_number].active = window_details[window_number].new_active;
	}
}



void SCRIPTING_deactivate_window (int window_number)
{
	// Sets a boolean to false within the window so that it isn't accessed in any
	// way (ie, it's buckets aren't opened up, nothing is added to it and it's not
	// drawn.

	if (!SCRIPTING_is_valid_window_index(window_number))
	{
		OUTPUT_message("Trying to deactivate a non-existant window number!");
		assert(0);
		return;
	}

	window_details[window_number].new_active = false;
}


/*
int SCRIPTING_add_window (int screen_x, int screen_y, int width, int height, int buffer_size, int z_ordering_list_size, int y_ordering_resolution)
{
	if ( (number_of_windows == 0) && (window_details == NULL) )
	{
		window_details = (window_struct *) malloc ( sizeof(window_struct) );
	}
	else
	{
		window_details = (window_struct *) realloc ( window_details , (number_of_windows+1) * sizeof(window_struct) );
	}

	if (window_details == NULL)
	{
		// Eep!
		assert(0);
	}

	window_details[number_of_windows].buffer_size = buffer_size;
	window_details[number_of_windows].buffered_br_x = 0;
	window_details[number_of_windows].buffered_br_y = 0;
	window_details[number_of_windows].buffered_tl_x = 0;
	window_details[number_of_windows].buffered_tl_y = 0;

	window_details[number_of_windows].current_x = 0;
	window_details[number_of_windows].current_y = 0;

	window_details[number_of_windows].new_x = 0;
	window_details[number_of_windows].new_y = 0;

	window_details[number_of_windows].skip_me_this_frame = 0;

	window_details[number_of_windows].new_screen_x = screen_x;
	window_details[number_of_windows].new_screen_y = screen_y;
	window_details[number_of_windows].screen_x = screen_x;
	window_details[number_of_windows].screen_y = screen_y;

	window_details[number_of_windows].new_width = width;
	window_details[number_of_windows].new_height = height;
	window_details[number_of_windows].width = width;
	window_details[number_of_windows].height = height;

	window_details[number_of_windows].vertex_red = 256;
	window_details[number_of_windows].vertex_green = 256;
	window_details[number_of_windows].vertex_blue = 256;
	window_details[number_of_windows].secondary_window_colour = false;

	window_details[number_of_windows].y_ordering_resolution = y_ordering_resolution;
	window_details[number_of_windows].y_ordering_list_size = ((window_details[number_of_windows].height + (2 * window_details[number_of_windows].buffer_size)) >> y_ordering_resolution) + 1;
	window_details[number_of_windows].y_ordering_list_starts = (int *) malloc ( window_details[number_of_windows].y_ordering_list_size  * sizeof (int) );
	window_details[number_of_windows].y_ordering_list_ends = (int *) malloc ( window_details[number_of_windows].y_ordering_list_size  * sizeof (int) );

	window_details[number_of_windows].z_ordering_list_size = z_ordering_list_size;
	window_details[number_of_windows].z_ordering_list_starts = (int *) malloc ( window_details[number_of_windows].z_ordering_list_size * sizeof (int) );
	window_details[number_of_windows].z_ordering_list_ends = (int *) malloc ( window_details[number_of_windows].z_ordering_list_size * sizeof (int) );

	int t;
	
	for (t=0; t<window_details[number_of_windows].y_ordering_list_size; t++)
	{
		window_details[number_of_windows].y_ordering_list_starts[t] = UNSET;
		window_details[number_of_windows].y_ordering_list_ends[t] = UNSET;
	}
	
	for (t=0; t<window_details[number_of_windows].z_ordering_list_size; t++)
	{
		window_details[number_of_windows].z_ordering_list_starts[t] = UNSET;
		window_details[number_of_windows].z_ordering_list_ends[t] = UNSET;
	}

	return number_of_windows++;
}
*/


bool SCRIPTING_find_next_entity (void)
{
	int previous_dead_entity;

	if (last_dead_entity_in_list != UNSET)
	{
		// Assign the last one on the dead stack to be the one used.
		just_created_entity = last_dead_entity_in_list;

		// Set the new last one on the dead stack to be the old last one's previous
		// linked entity.
		previous_dead_entity = entity[last_dead_entity_in_list][ENT_PREV_PROCESS_ENT];
		last_dead_entity_in_list = previous_dead_entity;

		// And destroy the link...
		if (previous_dead_entity != UNSET)
		{
			entity[previous_dead_entity][ENT_NEXT_PROCESS_ENT] = UNSET;
		}

		// And set it up...
		SCRIPTING_setup_entity(just_created_entity);
		
		return true;
	}
	else
	{
		return false;
	}
}



int SCRIPTING_spawn_restored_entity_last (void)
{
	int *just_created_entity_pointer;

	if (SCRIPTING_find_next_entity() == false)
	{
		return UNSET;
	}

	just_created_entity_pointer = &entity[just_created_entity][0];

	if (first_processed_entity_in_list == UNSET)
	{
		just_created_entity_pointer[ENT_PREV_PROCESS_ENT] = UNSET;
		just_created_entity_pointer[ENT_NEXT_PROCESS_ENT] = UNSET;
		first_processed_entity_in_list = just_created_entity;
		last_processed_entity_in_list = just_created_entity;
	}
	else
	{
		just_created_entity_pointer[ENT_PREV_PROCESS_ENT] = last_processed_entity_in_list;
		just_created_entity_pointer[ENT_NEXT_PROCESS_ENT] = UNSET;
		entity[last_processed_entity_in_list][ENT_NEXT_PROCESS_ENT] = just_created_entity;
		last_processed_entity_in_list = just_created_entity;
	}

	// Restored entities are immediately overwritten with saved variables, including
	// their script line/state. Starting them as JUST_BORN re-runs startup logic and
	// breaks resume semantics for persistent level entities.
	just_created_entity_pointer[ENT_ALIVE] = ALIVE;

	return just_created_entity;
}



void SCRIPTING_set_window_scale (int window_number, int opengl_scale_x, int opengl_scale_y, int pos_x, int pos_y)
{
	if (!SCRIPTING_is_valid_window_index(window_number))
	{
		OUTPUT_message("Trying to set scale on a non-existant window number!");
		assert(0);
		return;
	}

	window_struct *wp = &window_details[window_number];

	wp->fpos_x = float(pos_x) / 10000.0f;
	wp->fpos_y = float(pos_y) / 10000.0f;

	wp->new_opengl_scale_x = float (opengl_scale_x) / 10000.0f;
	wp->new_opengl_scale_y = float (opengl_scale_y) / 10000.0f;
}



void SCRIPTING_enable_window_colour (int window_number)
{
	if (!SCRIPTING_is_valid_window_index(window_number))
	{
		OUTPUT_message("Trying to enable colour on a non-existant window number!");
		assert(0);
		return;
	}

	window_details[window_number].secondary_window_colour = true;
}



void SCRIPTING_disable_window_colour (int window_number)
{
	if (!SCRIPTING_is_valid_window_index(window_number))
	{
		OUTPUT_message("Trying to disable colour on a non-existant window number!");
		assert(0);
		return;
	}

	window_details[window_number].secondary_window_colour = false;
}



void SCRIPTING_set_window_colour (int window_number, int vertex_red, int vertex_green, int vertex_blue)
{
	if (!SCRIPTING_is_valid_window_index(window_number))
	{
		OUTPUT_message("Trying to set colour on a non-existant window number!");
		assert(0);
		return;
	}

	window_struct *wp = &window_details[window_number];

	wp->vertex_red = vertex_red;
	wp->vertex_green = vertex_green;
	wp->vertex_blue = vertex_blue;
}



void SCRIPTING_screen_position_window (int window_number, int screen_x, int screen_y, int width, int height)
{
	if (!SCRIPTING_is_valid_window_index(window_number))
	{
		OUTPUT_message("Trying to position a non-existant window number!");
		assert(0);
		return;
	}

	window_struct *wp = &window_details[window_number];

	wp->new_screen_x = float(screen_x);
	wp->new_screen_y = float(screen_y);
	wp->new_width = width;
	wp->new_height = height;
}



void SCRIPTING_get_screen_window_position (int window_number, int *screen_x, int *screen_y, int *width, int *height)
{
	if (!SCRIPTING_is_valid_window_index(window_number))
	{
		OUTPUT_message("Trying to query a non-existant window number!");
		assert(0);
		return;
	}

	window_struct *wp = &window_details[window_number];

	*screen_x = int(wp->new_screen_x);
	*screen_y = int(wp->new_screen_y);
	*width = wp->new_width;
	*height = wp->new_height;
}



void SCRIPTING_map_position_window (int window_number, int x, int y)
{
	if (!SCRIPTING_is_valid_window_index(window_number))
	{
		OUTPUT_message("Trying to map-position a non-existant window number!");
		assert(0);
		return;
	}

	window_details[window_number].new_x = x;
	window_details[window_number].new_y = y;
}



void SCRIPTING_get_map_window_position (int window_number, int *x, int *y)
{
	if (!SCRIPTING_is_valid_window_index(window_number))
	{
		OUTPUT_message("Trying to query map-position on a non-existant window number!");
		assert(0);
		return;
	}

	*x = window_details[window_number].new_x;
	*y = window_details[window_number].new_y;
}



int SCRIPTING_spawn_entity_from_spawn_point (int tilemap_number, int parent_spawn_point, int script_number , int x , int y , int process_offset , int calling_entity_id)
{
	// Creates an entity and sticks the ID into the parent's CHILD_ID variable.

	// Also places it into the processing order wherever you like.

	// At the moment entities placed onto the end of the stack are linking with the
	// last entity which was created, rather than the entity which is processed last. FIX!

	int temp_prev,temp_next;

	int *calling_entity_pointer;
	int *just_created_entity_pointer;

	calling_entity_pointer = &entity[calling_entity_id][0];

	if (SCRIPTING_is_valid_script_index(script_number) == false)
	{
		char line[MAX_LINE_SIZE];
		snprintf(line, sizeof(line), "Invalid spawn-point script index (%d) at map %d spawn %d.", script_number, tilemap_number, parent_spawn_point);
		MAIN_add_to_log(line);
		return UNSET;
	}

	if (SCRIPTING_find_next_entity() == true)
	{
		just_created_entity_pointer = &entity[just_created_entity][0];

		switch (process_offset)
		{
		case PROCESS_FIRST: // It needs to be dumped at the start of the processing queue.
			// So first we set the previous to UNSET.
			just_created_entity_pointer[ENT_PREV_PROCESS_ENT] = UNSET;
			// And this entity's next pointer is pointed to the current first entity.
			just_created_entity_pointer[ENT_NEXT_PROCESS_ENT] = first_processed_entity_in_list;
			// The current first entity's previous pointer is pointed to the current one.
			entity [first_processed_entity_in_list][ENT_PREV_PROCESS_ENT] = just_created_entity;
			// And the first entity is set to the current one.
			first_processed_entity_in_list = just_created_entity;
			break;

		case PROCESS_PREVIOUS: // It needs to be placed just before the calling entity.
			// First of all we need to store where calling entity's previous pointer current points at.
			temp_prev = entity [calling_entity_id][ENT_PREV_PROCESS_ENT];
			if (temp_prev == UNSET) // The calling entity was the first in the queue so there is no previous one. So stick it at the start of the queue.
			{
				// So first we set the previous to UNSET.
				just_created_entity_pointer[ENT_PREV_PROCESS_ENT] = UNSET;
				// The calling entity's previous pointer is pointed to the current one.
				just_created_entity_pointer[ENT_NEXT_PROCESS_ENT] = calling_entity_id;
				// And this entity's next pointer is pointed to the calling entity.
				entity [first_processed_entity_in_list][ENT_PREV_PROCESS_ENT] = just_created_entity;
				// And the first entity is set to the current one.
				first_processed_entity_in_list = just_created_entity;	
			}
			else // There was at least one before the calling entity.
			{
				// Then we need to point that previous entity at the new one...
				entity [temp_prev][ENT_NEXT_PROCESS_ENT] = just_created_entity;
				// Then we need to point the calling entity's previous pointer at the current one.
				calling_entity_pointer[ENT_PREV_PROCESS_ENT] = just_created_entity;
				// And now get the current one to point forwards and backwards to those things.
				just_created_entity_pointer[ENT_PREV_PROCESS_ENT] = temp_prev;
				just_created_entity_pointer[ENT_NEXT_PROCESS_ENT] = calling_entity_id;
			}
			break;

		case PROCESS_NEXT: // It needs to be placed just after the calling entity.
			// First of all we need to store where calling entity's next pointer current points at.
			temp_next = entity [calling_entity_id][ENT_NEXT_PROCESS_ENT];
			if (temp_next == UNSET) // The calling entity was the last entity so we need to just bung it on the end of the queue.
			{
				just_created_entity_pointer[ENT_PREV_PROCESS_ENT] = calling_entity_id;
				just_created_entity_pointer[ENT_NEXT_PROCESS_ENT] = UNSET;
				calling_entity_pointer[ENT_NEXT_PROCESS_ENT] = just_created_entity;
				last_processed_entity_in_list = just_created_entity;
			}
			else // There's at least one entity after the calling entity.
			{
				// Then we need to point that next entity's previous at the new one...
				entity [temp_next][ENT_PREV_PROCESS_ENT] = just_created_entity;
				// Then we need to point the calling entity's next pointer at the current one.
				calling_entity_pointer[ENT_NEXT_PROCESS_ENT] = just_created_entity;
				// And now get the current one to point forwards and backwards to those things.
				just_created_entity_pointer[ENT_PREV_PROCESS_ENT] = calling_entity_id;
				just_created_entity_pointer[ENT_NEXT_PROCESS_ENT] = temp_next;
			}
	//		global_next_entity = just_created_entity;
			break;

		case PROCESS_LAST: // Just dump it unceremoniously on the end of the stack.
			just_created_entity_pointer[ENT_PREV_PROCESS_ENT] = last_processed_entity_in_list;
			just_created_entity_pointer[ENT_NEXT_PROCESS_ENT] = UNSET;
			entity [last_processed_entity_in_list][ENT_NEXT_PROCESS_ENT] = just_created_entity;
			last_processed_entity_in_list = just_created_entity;
			break;
		}

		// Not used as things aren't linked in the same way.

//		if (calling_entity_pointer[ENT_FIRST_CHILD] == UNSET)
//		{
//			calling_entity_pointer[ENT_FIRST_CHILD] = just_created_entity;
//		}
//		calling_entity_pointer[ENT_LAST_CHILD] = just_created_entity;

//		just_created_entity_pointer[ENT_PARENT] = UNSET;
		// So that things know if they were created by a spawn point. NO! THAT'LL DESTROY LINKAGE! Actually, it was overwritten, anyhoo.

		just_created_entity_pointer[ENT_MATRIARCH] = calling_entity_pointer[ENT_MATRIARCH];

		just_created_entity_pointer[ENT_WINDOW_NUMBER] = calling_entity_pointer[ENT_WINDOW_NUMBER];

		just_created_entity_pointer[ENT_ALIVE] = JUST_BORN;
		just_created_entity_pointer[ENT_PROGRAM_START] = scr_lookup[script_number];
		just_created_entity_pointer[ENT_WORLD_X] = x;
		just_created_entity_pointer[ENT_WORLD_Y] = y;
		just_created_entity_pointer[ENT_DRAW_ORDER] = calling_entity_pointer[ENT_DRAW_ORDER];
		just_created_entity_pointer[ENT_SCRIPT_NUMBER] = script_number;

		just_created_entity_pointer[ENT_PARENT_SPAWN_POINT] = parent_spawn_point;
		just_created_entity_pointer[ENT_PARENT_SPAWN_MAP] = tilemap_number;
	}
	else
	{
		// Arse! There was no room for the item in question. We'll need to do summat about killing low-priority in future...
	}

	return just_created_entity;
}



int SCRIPTING_recursively_spawn_from_spawn_points (int tilemap_number, int spawn_point_number, int calling_entity_id, int process_offset)
{
	// This function spawns the entity in the spawn points details and then overwrites
	// it's data with the parameters given. Then it looks for relatives and spawns those
	// as well.

	int created_entity_id;
	int *created_entity_pointer;
	int returned_entity_id;

	int temp_entity_id;

	spawn_point *sp = &cm[tilemap_number].spawn_point_list_pointer[spawn_point_number];

	// This stops entities getting into a recursive loop with parents calling childs and
	// vice versa.
	sp->last_fired = frames_so_far;

	// Create entity here...

	created_entity_id = SCRIPTING_spawn_entity_from_spawn_point (tilemap_number , spawn_point_number, sp->script_index , sp->x , sp->y , process_offset , calling_entity_id);
	if (created_entity_id == UNSET)
	{
		return UNSET;
	}
	cm[tilemap_number].spawn_point_list_pointer[spawn_point_number].created_entity_index = created_entity_id;
	created_entity_pointer = &entity[created_entity_id][0];

	// And then dump over the new parameters...

	int param;

	for (param=0; param<sp->params; param++)
	{
		created_entity_pointer[sp->param_list_pointer[param].param_dest_var_index] = sp->param_list_pointer[param].param_value;
	}

	// And then look for relatives which need a good spawning...

	if (sp->parent_index != UNSET)
	{
		// Check relative hasn't been spawned already this frame before creating them...

		if (cm[tilemap_number].spawn_point_list_pointer[sp->parent_index].last_fired != frames_so_far)
		{
			returned_entity_id = SCRIPTING_recursively_spawn_from_spawn_points (tilemap_number, sp->parent_index, created_entity_id, PROCESS_PREVIOUS);
			if (returned_entity_id == UNSET)
			{
				return created_entity_id;
			}

			entity [created_entity_id][ENT_PARENT] = returned_entity_id;
			entity [returned_entity_id][ENT_LAST_CHILD] = created_entity_id;
			entity [returned_entity_id][ENT_FIRST_CHILD] = created_entity_id;
		}
		else
		{
			// Okay, it's already been spawned, but we still need to link it up...
			temp_entity_id = cm[tilemap_number].spawn_point_list_pointer[sp->parent_index].created_entity_index;

			entity [created_entity_id][ENT_PARENT] = temp_entity_id;
			entity [temp_entity_id][ENT_LAST_CHILD] = created_entity_id;
			entity [temp_entity_id][ENT_FIRST_CHILD] = created_entity_id;
		}
	}

	if (sp->child_index != UNSET)
	{
		if (cm[tilemap_number].spawn_point_list_pointer[sp->child_index].last_fired != frames_so_far)
		{
			returned_entity_id = SCRIPTING_recursively_spawn_from_spawn_points (tilemap_number, sp->child_index, created_entity_id, PROCESS_NEXT);
			if (returned_entity_id == UNSET)
			{
				return created_entity_id;
			}

			entity [returned_entity_id][ENT_PARENT] = created_entity_id;
			entity [created_entity_id][ENT_LAST_CHILD] = returned_entity_id;
			entity [created_entity_id][ENT_FIRST_CHILD] = returned_entity_id;
		}
		else
		{
			// Okay, it's already been spawned, but we still need to link it up...
			temp_entity_id = cm[tilemap_number].spawn_point_list_pointer[sp->child_index].created_entity_index;

			entity [temp_entity_id][ENT_PARENT] = created_entity_id;
			entity [created_entity_id][ENT_LAST_CHILD] = temp_entity_id;
			entity [created_entity_id][ENT_FIRST_CHILD] = temp_entity_id;
		}
	}

	if (sp->next_sibling_index != UNSET)
	{
		if (cm[tilemap_number].spawn_point_list_pointer[sp->next_sibling_index].last_fired != frames_so_far)
		{
			returned_entity_id = SCRIPTING_recursively_spawn_from_spawn_points (tilemap_number, sp->next_sibling_index, created_entity_id, PROCESS_NEXT);
			if (returned_entity_id == UNSET)
			{
				return created_entity_id;
			}
			
			entity [returned_entity_id][ENT_PREV_SIBLING] = created_entity_id;
			entity [created_entity_id][ENT_NEXT_SIBLING] = returned_entity_id;
		}
		else
		{
			// Okay, it's already been spawned, but we still need to link it up...
			temp_entity_id = cm[tilemap_number].spawn_point_list_pointer[sp->next_sibling_index].created_entity_index;

			entity [temp_entity_id][ENT_PREV_SIBLING] = created_entity_id;
			entity [created_entity_id][ENT_NEXT_SIBLING] = temp_entity_id;
		}
	}

	if (sp->prev_sibling_index != UNSET)
	{
		if (cm[tilemap_number].spawn_point_list_pointer[sp->prev_sibling_index].last_fired != frames_so_far)
		{
			returned_entity_id = SCRIPTING_recursively_spawn_from_spawn_points (tilemap_number, sp->prev_sibling_index, created_entity_id, PROCESS_PREVIOUS);
			if (returned_entity_id == UNSET)
			{
				return created_entity_id;
			}

			entity [created_entity_id][ENT_PREV_SIBLING] = returned_entity_id;
			entity [returned_entity_id][ENT_NEXT_SIBLING] = created_entity_id;
		}
		else
		{
			// Okay, it's already been spawned, but we still need to link it up...
			temp_entity_id = cm[tilemap_number].spawn_point_list_pointer[sp->prev_sibling_index].created_entity_index;

			entity [created_entity_id][ENT_PREV_SIBLING] = temp_entity_id;
			entity [temp_entity_id][ENT_NEXT_SIBLING] = created_entity_id;
		}
	}

	return created_entity_id;
}



int SCRIPTING_get_zone_number_of_type_at_point (int tilemap_number, int x, int y, int zone_type)
{
	localised_zone_list *zone_list_pointer;
	int list_size;
	int zone_index_number;
	int zone_number;
	zone *zp;

	if (tilemap_number < 0 || tilemap_number >= number_of_tilemaps_loaded)
		return UNSET;

	zone_list_pointer = ROOMZONES_get_localised_list_size_and_pointer (tilemap_number, x, y);
	if (zone_list_pointer == NULL)
		return UNSET;
	list_size = zone_list_pointer->list_size;

	for (zone_index_number=0; zone_index_number<list_size; zone_index_number++)
	{
		zone_number = zone_list_pointer->indexes[zone_index_number];
		zp = &cm[tilemap_number].zone_list_pointer[zone_number];

		if (zp->type_index == zone_type)
		{
			if (MATH_rectangle_to_point_intersect_by_size (zp->real_x,zp->real_y,zp->real_width,zp->real_height,x,y) == true)
			{
				return zone_number;
			}
		}
	}

	return UNSET;
}



int SCRIPTING_count_spawnpoint_in_zone (int tilemap_number, int zone_number, int spawn_point_type)
{
	// Spawns all the spawn points in a zone if by going through the pre-created list and checking they
	// match the type we're looking for.

	int spawn_point_number;
	int counter;
	int list_size;
	int *list_pointer;

	int total_of_type = 0;

	if (tilemap_number < 0 || tilemap_number >= number_of_tilemaps_loaded)
	{
		return 0;
	}

	if (zone_number < 0 || zone_number >= cm[tilemap_number].zones)
	{
		return 0;
	}

	if (cm[tilemap_number].zone_list_pointer == NULL || cm[tilemap_number].spawn_point_list_pointer == NULL)
	{
		return 0;
	}

	list_size = cm[tilemap_number].zone_list_pointer[zone_number].spawn_point_index_list_size;
	list_pointer = cm[tilemap_number].zone_list_pointer[zone_number].spawn_point_index_list;

	if (list_size <= 0 || list_pointer == NULL)
	{
		return 0;
	}

	for (counter=0; counter<list_size; counter++)
	{
		spawn_point_number = list_pointer[counter];

		if (spawn_point_number < 0 || spawn_point_number >= cm[tilemap_number].spawn_points)
		{
			continue;
		}

		if (cm[tilemap_number].spawn_point_list_pointer[spawn_point_number].type_value == spawn_point_type)
		{
			total_of_type++;
		}
	}

	return total_of_type;
}



void SCRIPTING_spawn_all_points_in_zone (int tilemap_number, int zone_number, int calling_entity_id, int process_offset, int spawn_point_type)
{
	// Spawns all the spawn points in a zone if by going through the pre-created list and checking they
	// match the type we're looking for.

	int spawn_point_number;
	int counter;
	int list_size;
	int *list_pointer;
	int last_created = UNSET;
	int matching_points = 0;
	int created_points = 0;
	static int cached_c64_main_level_enemy_type = UNSET;
	static int cached_c64_pre_level_enemy_type = UNSET;

	if (cached_c64_main_level_enemy_type == UNSET)
	{
		cached_c64_main_level_enemy_type = GPL_find_word_value("SPAWN_POINT_TYPES", "C64_MAIN_LEVEL_ENEMY");
	}

	if (cached_c64_pre_level_enemy_type == UNSET)
	{
		cached_c64_pre_level_enemy_type = GPL_find_word_value("SPAWN_POINT_TYPES", "C64_PRE_LEVEL_ENEMY");
	}

	if (tilemap_number < 0 || tilemap_number >= number_of_tilemaps_loaded)
	{
		return;
	}

	if (zone_number < 0 || zone_number >= cm[tilemap_number].zones)
	{
		return;
	}

	if (cm[tilemap_number].zone_list_pointer == NULL || cm[tilemap_number].spawn_point_list_pointer == NULL)
	{
		return;
	}

	list_size = cm[tilemap_number].zone_list_pointer[zone_number].spawn_point_index_list_size;
	list_pointer = cm[tilemap_number].zone_list_pointer[zone_number].spawn_point_index_list;

	if (list_size <= 0 || list_pointer == NULL)
	{
		return;
	}

	for (counter=0; counter<list_size; counter++)
	{
		spawn_point_number = list_pointer[counter];

		if (spawn_point_number < 0 || spawn_point_number >= cm[tilemap_number].spawn_points)
		{
			continue;
		}

		if (cm[tilemap_number].spawn_point_list_pointer[spawn_point_number].last_fired != frames_so_far)
		{
			if (cm[tilemap_number].spawn_point_list_pointer[spawn_point_number].type_value == spawn_point_type)
			{
				matching_points++;
				last_created = SCRIPTING_recursively_spawn_from_spawn_points (tilemap_number, spawn_point_number, calling_entity_id, process_offset);
				if (last_created != UNSET)
				{
					created_points++;
				}
			}
		}
	}

	if (last_created != UNSET)
	{
		if (calling_entity_id >= 0 && calling_entity_id < MAX_ENTITIES)
		{
			entity[calling_entity_id][ENT_LAST_CHILD] = last_created;
		}
	}
}



void SCRIPTING_add_entity_to_window (int entity_number)
{
	int window_number;
	int *ep = &entity[entity_number][0];
	int y_ordering_list;
	static int queue_guard_log_count = 0;

	if ((entity_number < 0) || (entity_number >= MAX_ENTITIES))
	{
		return;
	}

	if (entity_window_queue_stamp[entity_number] == entity_window_queue_epoch)
	{
		if (queue_guard_log_count < 128)
		{
			queue_guard_log_count++;
			SCRIPTING_emit_window_queue_guard(
				"WINDOW_QUEUE_GUARD stage=duplicate_insert ent=%d frame=%d",
				entity_number,
				frames_so_far);
		}
		return;
	}
	entity_window_queue_stamp[entity_number] = entity_window_queue_epoch;

	#ifdef RETRENGINE_DEBUG_VERSION_ENTITY_DEBUG_FLAG_CHECKS
	if (ep[ENT_DEBUG_FLAGS] & DEBUG_FLAG_STOP_WHEN_Y_SORTED)
	{
		assert(0);
	}
	#endif

	window_number = ep[ENT_WINDOW_NUMBER];

	if ( (window_number != UNSET) && (window_number < number_of_windows) && (window_details[window_number].active == true) )
	{
		if (ep[ENT_DRAW_OVERRIDE] == DRAW_OVERRIDE_NONE)
		{
			if (MATH_rectangle_to_point_intersect ( window_details[window_number].buffered_tl_x , window_details[window_number].buffered_tl_y , window_details[window_number].buffered_br_x , window_details[window_number].buffered_br_y , ep[ENT_WORLD_X] , ep[ENT_WORLD_Y] ) == true)
			{
				y_ordering_list = ( ep[ENT_WORLD_Y] - window_details[window_number].buffered_tl_y ) >> window_details[window_number].y_ordering_resolution;
			}
			else
			{
				return;
			}
		}
		else if (ep[ENT_DRAW_OVERRIDE] == DRAW_OVERRIDE_OVERRIDE_IF_Y)
		{
			if ( (ep[ENT_WORLD_Y] > window_details[window_number].buffered_tl_y ) && (ep[ENT_WORLD_Y] < window_details[window_number].buffered_br_y ) )
			{
				y_ordering_list = ( ep[ENT_WORLD_Y] - window_details[window_number].buffered_tl_y ) >> window_details[window_number].y_ordering_resolution;
			}
			else
			{
				return;
			}
		}
		else if (ep[ENT_DRAW_OVERRIDE] == DRAW_OVERRIDE_OVERRIDE_ALWAYS)
		{
			y_ordering_list = 0;
		}

		if (window_details[window_number].y_ordering_list_ends[y_ordering_list] == UNSET)
		{
			ep[ENT_PREV_WINDOW_ENT] = UNSET;
			ep[ENT_NEXT_WINDOW_ENT] = UNSET;

			window_details[window_number].y_ordering_list_starts[y_ordering_list] = entity_number;
			window_details[window_number].y_ordering_list_ends[y_ordering_list] = entity_number;
		}
		else
		{
			ep[ENT_PREV_WINDOW_ENT] = window_details[window_number].y_ordering_list_ends[y_ordering_list];
			entity[window_details[window_number].y_ordering_list_ends[y_ordering_list]][ENT_NEXT_WINDOW_ENT] = entity_number;
			ep[ENT_NEXT_WINDOW_ENT] = UNSET;

			window_details[window_number].y_ordering_list_ends[y_ordering_list] = entity_number;
		}

	}
}



void SCRIPTING_erase_window (int window_number)
{
	// This not-only destroys the window_details data but also any entities
	// with the corresponding window ID and should only really be called by
	// the entity which created the window in order to keep track of things.




}



void SCRIPTING_move_window_y_queue_to_z_queue (int window_number)
{
	// This moves the items in the y-ordered queue over to the
	// z-ordering queue, which is much smaller and more bijou.

	// The scope of the z-ordering queue is defined when the window
	// is created.

	// PROBLEM: Recursion problem, possibly due to new processing of
	// entities.

	int current_entity;
	int next_entity;

	int *cep;

	int y_ordering_list;
	int y_ordering_list_size = window_details[window_number].y_ordering_list_size;
	int z_ordering_list;
	int z_ordering_list_size = window_details[window_number].z_ordering_list_size;
	const int chain_guard_limit = MAX_ENTITIES + 16;
	static int guard_log_count = 0;
	static int queue_visit_stamp[MAX_ENTITIES];
	static int queue_visit_epoch = 0;

	queue_visit_epoch++;
	if (queue_visit_epoch <= 0)
	{
		int t;
		queue_visit_epoch = 1;
		for (t = 0; t < MAX_ENTITIES; t++)
		{
			queue_visit_stamp[t] = 0;
		}
	}

	for (y_ordering_list=0; y_ordering_list<y_ordering_list_size; y_ordering_list++)
	{
		int chain_guard = 0;
		current_entity = window_details[window_number].y_ordering_list_starts[y_ordering_list];

		if (current_entity != UNSET)
		{
			window_details[window_number].y_ordering_list_starts[y_ordering_list] = UNSET;
			window_details[window_number].y_ordering_list_ends[y_ordering_list] = UNSET;
		}

		while (current_entity != UNSET)
		{
			bool inserted_into_z_list = false;

			chain_guard++;
			if (chain_guard > chain_guard_limit)
			{
				if (guard_log_count < 128)
				{
					guard_log_count++;
					SCRIPTING_emit_window_queue_guard(
						"WINDOW_QUEUE_GUARD stage=y_to_z window=%d y_list=%d current=%d frame=%d",
						window_number,
						y_ordering_list,
						current_entity,
						frames_so_far);
				}
				break;
			}

			if ((current_entity < 0) || (current_entity >= MAX_ENTITIES))
			{
				if (guard_log_count < 128)
				{
					guard_log_count++;
					SCRIPTING_emit_window_queue_guard(
						"WINDOW_QUEUE_INVALID stage=y_to_z window=%d y_list=%d current=%d frame=%d",
						window_number,
						y_ordering_list,
						current_entity,
						frames_so_far);
				}
				break;
			}
			if (queue_visit_stamp[current_entity] == queue_visit_epoch)
			{
				if (guard_log_count < 128)
				{
					guard_log_count++;
					SCRIPTING_emit_window_queue_guard(
						"WINDOW_QUEUE_GUARD stage=y_cycle window=%d y_list=%d current=%d frame=%d",
						window_number,
						y_ordering_list,
						current_entity,
						frames_so_far);
				}
				break;
			}
			queue_visit_stamp[current_entity] = queue_visit_epoch;

			cep = &entity[current_entity][0];

			#ifdef RETRENGINE_DEBUG_VERSION_ENTITY_DEBUG_FLAG_CHECKS
			if (cep[ENT_DEBUG_FLAGS] & DEBUG_FLAG_STOP_WHEN_Z_SORTED)
			{
				assert(0);
			}
			#endif

			next_entity = cep[ENT_NEXT_WINDOW_ENT];
			if (next_entity == current_entity)
			{
				if (guard_log_count < 128)
				{
					guard_log_count++;
					SCRIPTING_emit_window_queue_guard(
						"WINDOW_QUEUE_SELF_LOOP stage=next_window window=%d y_list=%d ent=%d frame=%d",
						window_number,
						y_ordering_list,
						current_entity,
						frames_so_far);
				}
				next_entity = UNSET;
			}

			z_ordering_list = cep[ENT_DRAW_ORDER];

			if ((z_ordering_list >= 0) && (z_ordering_list < z_ordering_list_size))
			{
				if (window_details[window_number].z_ordering_list_ends[z_ordering_list] == UNSET)
				{
					cep[ENT_PREV_WINDOW_ENT] = UNSET;
					cep[ENT_NEXT_WINDOW_ENT] = UNSET;

					window_details[window_number].z_ordering_list_starts[z_ordering_list] = current_entity;
					window_details[window_number].z_ordering_list_ends[z_ordering_list] = current_entity;
					inserted_into_z_list = true;
				}
				else
				{
					cep[ENT_PREV_WINDOW_ENT] = window_details[window_number].z_ordering_list_ends[z_ordering_list];
					entity[cep[ENT_PREV_WINDOW_ENT]][ENT_NEXT_WINDOW_ENT] = current_entity;
					cep[ENT_NEXT_WINDOW_ENT] = UNSET;

					window_details[window_number].z_ordering_list_ends[z_ordering_list] = current_entity;
					inserted_into_z_list = true;
				}
			}

			if (inserted_into_z_list)
			{
				int buddy_guard = 0;
				while (cep[ENT_DRAW_BUDDY] != UNSET)
				{
					int buddy_entity = cep[ENT_DRAW_BUDDY];
					buddy_guard++;
					if (buddy_guard > chain_guard_limit)
					{
						if (guard_log_count < 128)
						{
							guard_log_count++;
							SCRIPTING_emit_window_queue_guard(
								"WINDOW_QUEUE_GUARD stage=draw_buddy window=%d y_list=%d ent=%d buddy=%d frame=%d",
								window_number,
								y_ordering_list,
								current_entity,
								buddy_entity,
								frames_so_far);
						}
						break;
					}
					if ((buddy_entity < 0) || (buddy_entity >= MAX_ENTITIES) || (buddy_entity == current_entity))
					{
						if (guard_log_count < 128)
						{
							guard_log_count++;
							SCRIPTING_emit_window_queue_guard(
								"WINDOW_QUEUE_INVALID stage=draw_buddy window=%d y_list=%d ent=%d buddy=%d frame=%d",
								window_number,
								y_ordering_list,
								current_entity,
								buddy_entity,
								frames_so_far);
						}
						cep[ENT_DRAW_BUDDY] = UNSET;
						break;
					}
					if (queue_visit_stamp[buddy_entity] == queue_visit_epoch)
					{
						if (guard_log_count < 128)
						{
							guard_log_count++;
							SCRIPTING_emit_window_queue_guard(
								"WINDOW_QUEUE_GUARD stage=draw_buddy_cycle window=%d y_list=%d ent=%d buddy=%d frame=%d",
								window_number,
								y_ordering_list,
								current_entity,
								buddy_entity,
								frames_so_far);
						}
						cep[ENT_DRAW_BUDDY] = UNSET;
						break;
					}
					queue_visit_stamp[buddy_entity] = queue_visit_epoch;

					// Hurrah! It has a draw buddy. We don't need to recalculate the z-order list as it'll
					// be the same one and we know it has at least one thing in it, so this is pretty easy...
					current_entity = buddy_entity;
					cep = &entity[current_entity][0];

				cep[ENT_PREV_WINDOW_ENT] = window_details[window_number].z_ordering_list_ends[z_ordering_list];
				entity[cep[ENT_PREV_WINDOW_ENT]][ENT_NEXT_WINDOW_ENT] = current_entity;
				cep[ENT_NEXT_WINDOW_ENT] = UNSET;

					window_details[window_number].z_ordering_list_ends[z_ordering_list] = current_entity;
				}
			}

			current_entity = next_entity;
		}
	}
}



void SCRIPTING_update_window_positions (void)
{
	int window_number;
	window_struct *wp = NULL;

	float width_diff;
	float height_diff;

	for (window_number=0; window_number<number_of_windows; window_number++)
	{
		wp = &window_details[window_number];

		wp->current_x = wp->new_x;
		wp->current_y = wp->new_y;

		wp->screen_x = wp->new_screen_x;
		wp->screen_y = wp->new_screen_y;
		wp->width = wp->new_width;
		wp->height = wp->new_height;

		wp->opengl_scale_x = wp->new_opengl_scale_x;
		wp->opengl_scale_y = wp->new_opengl_scale_y;

		wp->scaled_width = wp->width * wp->opengl_scale_x;
		wp->scaled_height = wp->height * wp->opengl_scale_y;

		width_diff = wp->width - wp->scaled_width;
		height_diff = wp->height - wp->scaled_height;

		wp->opengl_transform_x = width_diff * wp->fpos_x;
		wp->opengl_transform_y = height_diff * wp->fpos_y;

		window_details[window_number].buffered_tl_x = window_details[window_number].current_x - window_details[window_number].buffer_size;
		window_details[window_number].buffered_tl_y = window_details[window_number].current_y - window_details[window_number].buffer_size;
		window_details[window_number].buffered_br_x = window_details[window_number].current_x + window_details[window_number].width + window_details[window_number].buffer_size;
		window_details[window_number].buffered_br_y = window_details[window_number].current_y + window_details[window_number].height + window_details[window_number].buffer_size;
	}
}



void SCRIPTING_setup_default_entity (void)
{
	int i;

	for (i=0 ; i<MAX_ENTITY_VARIABLES ; i++)
	{
		reset_entity[i] = 0;
	}
}



void SCRIPTING_delete_entity (int entity_id)
{
	// Then flag it as deaderoony!
	entity[entity_id][ENT_ALIVE] = DEAD;

	// Oh, and make sure it's taken out of the processing loop.
	SCRIPTING_delete_processing_order_entry (entity_id);

	// Also remove from the collision bucket if it was in there... And the general map bucket.
	OBCOLL_remove_from_collision_bucket (entity_id);
	OBCOLL_remove_from_general_area_bucket (entity_id);

	// And then add it to the dead list...
	SCRIPTING_add_to_dead_list(entity_id);
}



void SCRIPTING_set_up_interaction_table (void)
{
	script_list_size = GPL_list_size ("SCRIPTS");

	int size = script_list_size * script_list_size;

	scripting_interaction_table = (int *) malloc (size * sizeof(int));

	int t;

	for (t=0; t<size; t++)
	{
		scripting_interaction_table[t] = 0;
	}
}



void SCRIPTING_add_to_interaction_table (int from_entity, int to_entity, int interaction_type)
{
	int from_script = entity[from_entity][ENT_SCRIPT_NUMBER];
	int to_script = entity[to_entity][ENT_SCRIPT_NUMBER];

	int offset = (script_list_size * from_script) + to_script;

	scripting_interaction_table[offset] |= interaction_type;
}



bool SCRIPTING_read_from_interaction_table (int from_script, int to_script, int interaction_type)
{
	int offset = (script_list_size * from_script) + to_script;

	if (scripting_interaction_table[offset] & interaction_type)
	{
		return true;
	}
	else
	{
		return false;
	}
}



bool SCRIPTING_find_any_matching_interactions (int from_entity, int interaction_type)
{
	int to_entity;

	for (to_entity=0; to_entity<script_list_size; to_entity++)
	{
		if (SCRIPTING_read_from_interaction_table(from_entity,to_entity,interaction_type))
		{
			return true;
		}
	}

	return false;	
}



bool SCRIPTING_find_any_matching_interactions_other_side (int from_entity, int interaction_type)
{
	int to_entity;

	for (to_entity=0; to_entity<script_list_size; to_entity++)
	{
		if (SCRIPTING_read_from_interaction_table(to_entity,from_entity,interaction_type))
		{
			return true;
		}
	}

	return false;	
}



void SCRIPTING_destroy_interaction_table (void)
{
	free (scripting_interaction_table);
}



void SCRIPTING_output_interaction_table (void)
{
	// This outputs the interactions... Woot!

	int entity_number;
	int other_entity_number;
	char line[MAX_LINE_SIZE];

	FILE *file_pointer = fopen (MAIN_get_project_filename("interaction_table.txt"),"w");

	if (file_pointer)
	{
		fputs ("INTERACTION TABLE:\n\n",file_pointer);

		for (entity_number=0; entity_number<script_list_size; entity_number++)
		{
			snprintf (line, sizeof(line), "%s\n", GPL_get_entry_name("SCRIPTS", entity_number));
			fputs (line,file_pointer);

#ifdef DEBUG_INTERACTION_READ
			if (SCRIPTING_find_any_matching_interactions (entity_number,DEBUG_INTERACTION_READ))
			{
				// First of all, the ones that this entity has READ.
				fputs ("\tENTITIES READ FROM:\n",file_pointer);

				for (other_entity_number=0; other_entity_number<script_list_size; other_entity_number++)
				{
					if (SCRIPTING_read_from_interaction_table(entity_number,other_entity_number,DEBUG_INTERACTION_READ))
					{
						snprintf (line, sizeof(line), "\t\t%s\n", GPL_get_entry_name("SCRIPTS", other_entity_number));
						fputs (line,file_pointer);
					}
				}
			}
#endif
#ifdef DEBUG_INTERACTION_WRITE
			if (SCRIPTING_find_any_matching_interactions (entity_number,DEBUG_INTERACTION_WRITE))
			{
				// Next the ones that this entity has WRITTEN.
				fputs ("\tENTITIES WRITTEN TO:\n",file_pointer);

				for (other_entity_number=0; other_entity_number<script_list_size; other_entity_number++)
				{
					if (SCRIPTING_read_from_interaction_table(entity_number,other_entity_number,DEBUG_INTERACTION_WRITE))
					{
						snprintf (line, sizeof(line), "\t\t%s\n", GPL_get_entry_name("SCRIPTS", other_entity_number));
						fputs (line,file_pointer);
					}
				}
			}
#endif
#ifdef DEBUG_INTERACTION_KILL
			if (SCRIPTING_find_any_matching_interactions (entity_number,DEBUG_INTERACTION_KILL))
			{
				// Next the ones that this entity has KILLED.
				fputs ("\tENTITIES KILLED:\n",file_pointer);

				for (other_entity_number=0; other_entity_number<script_list_size; other_entity_number++)
				{
					if (SCRIPTING_read_from_interaction_table(entity_number,other_entity_number,DEBUG_INTERACTION_KILL))
					{
						snprintf (line, sizeof(line), "\t\t%s\n", GPL_get_entry_name("SCRIPTS", other_entity_number));
						fputs (line,file_pointer);
					}
				}
			}
#endif
#ifdef DEBUG_INTERACTION_READ
			if (SCRIPTING_find_any_matching_interactions_other_side(entity_number,DEBUG_INTERACTION_READ))
			{
				// And now the ones that have read IT!
				fputs ("\tREADING ENTITIES:\n",file_pointer);

				for (other_entity_number=0; other_entity_number<script_list_size; other_entity_number++)
				{
					if (SCRIPTING_read_from_interaction_table(other_entity_number,entity_number,DEBUG_INTERACTION_READ))
					{
						snprintf (line, sizeof(line), "\t\t%s\n", GPL_get_entry_name("SCRIPTS", other_entity_number));
						fputs (line,file_pointer);
					}
				}
			}
#endif
#ifdef DEBUG_INTERACTION_WRITE
			if (SCRIPTING_find_any_matching_interactions_other_side(entity_number,DEBUG_INTERACTION_WRITE))
			{
				// Next the ones that have written to IT!
				fputs ("\tWRITING ENTITIES:\n",file_pointer);

				for (other_entity_number=0; other_entity_number<script_list_size; other_entity_number++)
				{
					if (SCRIPTING_read_from_interaction_table(other_entity_number,entity_number,DEBUG_INTERACTION_WRITE))
					{
						snprintf (line, sizeof(line), "\t\t%s\n", GPL_get_entry_name("SCRIPTS", other_entity_number));
						fputs (line,file_pointer);
					}
				}
			}
#endif
#ifdef DEBUG_INTERACTION_KILL
			if (SCRIPTING_find_any_matching_interactions_other_side(entity_number,DEBUG_INTERACTION_KILL))
			{
				// Next the ones that have killed IT!
				fputs ("\tWRITING ENTITIES:\n",file_pointer);

				for (other_entity_number=0; other_entity_number<script_list_size; other_entity_number++)
				{
					if (SCRIPTING_read_from_interaction_table(other_entity_number,entity_number,DEBUG_INTERACTION_KILL))
					{
						snprintf (line, sizeof(line), "\t\t%s\n", GPL_get_entry_name("SCRIPTS", other_entity_number));
						fputs (line,file_pointer);
					}
				}
			}
#endif
			fputs("\n",file_pointer);
		}
	}
}



void SCRIPTING_verify_flags (void)
{
	int flag;

	for (flag=0 ; flag<flag_count ; flag++)
	{
		if (flag_type[flag] == FLAG_TYPE_ENTITY_ID)
		{
			if (entity[flag_array[flag]][ENT_ALIVE] < ALIVE )
			{
				flag_type[flag] = FLAG_TYPE_NORMAL;
				flag_array[flag] = UNSET;
			}
		}
	}
}



void SCRIPTING_setup_everything (void)
{
	int entity_id;
	int flag;

	last_processed_entity_in_list = 0;
	first_processed_entity_in_list = UNSET;

	SCRIPTING_setup_default_entity ();

	for (entity_id=0 ; entity_id<MAX_ENTITIES ; entity_id++)
	{
		SCRIPTING_setup_entity(entity_id);
		SCRIPTING_add_to_dead_list(entity_id);
	}

	int start,end;

	GPL_list_extents ("FLAG", &start, &end);
	
	flag_count = end-start;

	flag_array = (int *) malloc (flag_count * sizeof (int) );
	flag_type = (int *) malloc (flag_count * sizeof (int) );

	for (flag=0 ; flag<flag_count ; flag++)
	{
		flag_array[flag] = UNSET;
		flag_type[flag] = FLAG_TYPE_NORMAL;
	}

	TILESETS_invert_tile_solidity ();

	#ifdef RETRENGINE_DEBUG_VERSION_CHECK_VARIABLE_SCOPE
		SCRIPTING_set_up_interaction_table ();
	#endif
}



void SCRIPTING_destroy_flag_arrays (void)
{
	if (flag_array != NULL)
	{
		free (flag_array);
	}

	if (flag_type != NULL)
	{
		free (flag_type);
	}
}



void SCRIPTING_destroy_all_scripts (void)
{
	free (scr_lookup);
	SCRIPTING_free_continue_state_map();

	int script_line;

	for (script_line=0; script_line<script_line_count; script_line++)
	{
		free (scr[script_line].script_line_pointer);
	}

	free (scr);
}



bool SCRIPTING_load_script ( char *filename )
{
	// Loads the big lumpy script lump thing. Which is a lump.

	int total_size;
	int script_index_table_size; // This is the list of starts of each line in the full script
	int line_length_table_size; // This is the list of how big each line in the script is (so it can be all malloc'd at once)
	int line_indentation_table_size; // Same size as above.
	int data_table_size; // This is all the actual script data itself, a fairly large chunk of change.
	
	char line[TEXT_LINE_SIZE];

	int t;

	int current_line;
	int current_instruction;

	int details_array[5];
	int *big_data;
	int *pointer;
	bool swap_endian = false;

	FILE *file_pointer = FILE_open_project_case_fallback(filename, "rb");

	if (file_pointer == NULL)
	{
		return true;
	}

	fread (details_array , sizeof(int) , 5 , file_pointer);
#ifdef ALLEGRO_MACOSX
	for (t = 0; t < 5; ++t)
		details_array[t] = EndianS32_LtoN(details_array[t]);
#endif
	int swapped_details[5];
	for (t = 0; t < 5; ++t)
	{
		swapped_details[t] = SCRIPTING_bswap32(details_array[t]);
	}

	bool native_header_plausible = false;
	bool swapped_header_plausible = false;

	if (details_array[0] > 0 && details_array[0] < 50000000 &&
		details_array[1] >= 0 && details_array[2] > 0 && details_array[3] > 0 && details_array[4] >= 0 &&
		details_array[3] == details_array[2] &&
		(details_array[1] + details_array[2] + details_array[3] + (details_array[4] * 3)) == details_array[0])
	{
		native_header_plausible = true;
	}

	if (swapped_details[0] > 0 && swapped_details[0] < 50000000 &&
		swapped_details[1] >= 0 && swapped_details[2] > 0 && swapped_details[3] > 0 && swapped_details[4] >= 0 &&
		swapped_details[3] == swapped_details[2] &&
		(swapped_details[1] + swapped_details[2] + swapped_details[3] + (swapped_details[4] * 3)) == swapped_details[0])
	{
		swapped_header_plausible = true;
	}

	if (!native_header_plausible && swapped_header_plausible)
	{
		swap_endian = true;
		for (t = 0; t < 5; ++t)
		{
			details_array[t] = swapped_details[t];
		}
	}

	total_size = details_array[0];
	script_index_table_size = details_array[1];
	line_length_table_size = details_array[2];
	line_indentation_table_size = details_array[3];
	data_table_size = details_array[4];

	big_data = (int *) malloc ( total_size * sizeof(int) );

	if (big_data == NULL)
	{
		OUTPUT_message("Could not allocate 'big_data' when loading! Arse!");
		assert(0);
	}

	if ((int)fread (big_data , sizeof(int) , total_size , file_pointer) != total_size)
	{
		free (big_data);
		fclose (file_pointer);
		OUTPUT_message("Script file read failed.");
		assert(0);
	}
#ifdef ALLEGRO_MACOSX
	for (t = 0; t < total_size; ++t)
		big_data[t] = EndianS32_LtoN(big_data[t]);
#endif
	if (swap_endian)
	{
		for (t = 0; t < total_size; ++t)
		{
			big_data[t] = SCRIPTING_bswap32(big_data[t]);
		}
	}
	fclose (file_pointer);

	// Now we've loaded the data, we need to allocate the necessary
	// space for the script table, line size table and lines themselves.
	// Then copy the crap over to them. :)
 
	scr = (script_line *) malloc (line_length_table_size * sizeof (script_line));

	if (scr == NULL)
	{
		OUTPUT_message("Could not allocate 'scr' script data memory.");
		assert(0);
	}

	scr_lookup = (int *) malloc ((script_index_table_size+1) * sizeof (int));

	if (scr_lookup == NULL)
	{
		OUTPUT_message("Could not allocate 'scr_lookup' script lookup memory.");
		assert(0);
	}

	// Okay, before we can malloc the sizes of each line we need to read them
	// in from the file, although first we need to read in the script index
	// data as that's first in the file...

	pointer = big_data;

	for (t=0; t<script_index_table_size; t++)
	{
		scr_lookup[t] = *pointer;
		pointer++;
	}

	scr_lookup[script_index_table_size] = line_length_table_size;

	for (t=0; t<line_length_table_size; t++)
	{
		scr[t].script_line_size = *pointer;
		pointer++;
	}

	for (t=0; t<line_length_table_size; t++)
	{
		scr[t].indentation_level = *pointer;
		pointer++;
	}

	for (t=0; t<line_length_table_size; t++)
	{
		scr[t].script_line_pointer = (script_data *) malloc (scr[t].script_line_size * sizeof(script_data));
		if (scr[t].script_line_pointer == NULL)
		{
			snprintf (line, sizeof(line), "Could not allocate script data of size %d for line %d of script.", scr[t].script_line_size, t);
			OUTPUT_message(line);
			assert(0);
		}
	}

	// Now we can actually read the data in! Woot!

	current_line = 0;
	current_instruction = 0;

	for (t=0; t<data_table_size; t++)
	{
		scr[current_line].script_line_pointer[current_instruction].data_type = *pointer;
		pointer++;
		scr[current_line].script_line_pointer[current_instruction].data_value = *pointer;
		pointer++;
		scr[current_line].script_line_pointer[current_instruction].data_bitmask = *pointer;
		pointer++;

		current_instruction++;

		if (current_instruction == scr[current_line].script_line_size)
		{
			current_instruction = 0;
			current_line++;
		}
	}

	// And then free the big data!

	free (big_data);

	// And set the globals...

	script_line_count = line_length_table_size;
	SCRIPTING_load_continue_state_map();
	script_count = script_index_table_size;

	// And then find the actual command which starts each line...

	int i;

	for (t=0; t<script_line_count; t++)
	{
		for (i=0; i<scr[t].script_line_size; i++)
		{
			if (scr[t].script_line_pointer[i].data_type == COMMAND)
			{
				scr[t].command_instruction_index = i;
			}
		}
	}

	return false;
}



void SCRIPTING_put_value ( int entity_id , int line_number , int argument , int value )
{
	#ifdef RETRENGINE_DEBUG_VERSION_WATCH_LIST
		int watch_counter;
	#endif

	int data_type;
	int data_value;
	int data_bitmask;

	data_type = scr[line_number].script_line_pointer[argument].data_type;
	data_value = scr[line_number].script_line_pointer[argument].data_value;
	data_bitmask = scr[line_number].script_line_pointer[argument].data_bitmask;

	if ((data_bitmask & DATA_BITMASK_IDENTITY_VARIABLE) > 0)
	{
		#ifdef RETRENGINE_DEBUG_VERSION_WATCH_LIST
		for (watch_counter=0; watch_counter<watch_list_size; watch_counter++)
		{
			if ( (entity[entity[entity_id][data_type]][ENT_SCRIPT_NUMBER] == watch_list_script[watch_counter]) && (data_value == watch_list_variable[watch_counter]) )
			{
				SCRIPTING_add_report_to_watch_report (entity[entity_id][data_type], entity_id, line_number, data_value, entity[entity[entity_id][data_type]][data_value], value );
			}
		}
		#endif

		#ifdef RETRENGINE_DEBUG_VERSION_COMPILE_INTERACTION_TABLE
			SCRIPTING_add_to_interaction_table (entity_id,entity[entity_id][data_type],DEBUG_INTERACTION_WRITE);
		#endif

		#ifdef RETRENGINE_DEBUG_VERSION_CHECK_VARIABLE_SCOPE
			if (data_value>MAX_ENTITY_VARIABLES)
			{
				OUTPUT_message("Variable outside of range!");
				assert(0);
			}
		#endif

		entity [ entity[entity_id][data_type] ][data_value] = value;
	}
	else
	{
		#ifdef RETRENGINE_DEBUG_VERSION_WATCH_LIST
		for (watch_counter=0; watch_counter<watch_list_size; watch_counter++)
		{
			if ( (entity[entity_id][ENT_SCRIPT_NUMBER] == watch_list_script[watch_counter]) && (data_value == watch_list_variable[watch_counter]) )
			{
				SCRIPTING_add_report_to_watch_report (entity_id, entity_id, line_number, data_value, entity[entity_id][data_value], value );
			}
		}
		#endif

		#ifdef RETRENGINE_DEBUG_VERSION_CHECK_VARIABLE_SCOPE
			if (data_value>MAX_ENTITY_VARIABLES)
			{
				OUTPUT_message("Variable outside of range!");
				assert(0);
			}
		#endif

		entity [entity_id][data_value] = value;
	}
}



bool SCRIPTING_get_bool_value ( int entity_id , int line_number , int argument )
{
	int data_type;
	int data_value;
	int data_bitmask;
	int value;

	data_type = scr[line_number].script_line_pointer[argument].data_type;
	data_value = scr[line_number].script_line_pointer[argument].data_value;
	data_bitmask = scr[line_number].script_line_pointer[argument].data_bitmask;

	if ((data_bitmask & DATA_BITMASK_IDENTITY_VARIABLE) > 0)
	{
		value = entity [ entity[entity_id][data_type] ][data_value];

		#ifdef RETRENGINE_DEBUG_VERSION_CHECK_VARIABLE_SCOPE
			if (data_value>MAX_ENTITY_VARIABLES)
			{
				OUTPUT_message("Variable outside of range!");
				assert(0);
			}
		#endif
	}
	else if (data_type == VARIABLE)
	{
		value = entity [ entity_id ][data_value];

		#ifdef RETRENGINE_DEBUG_VERSION_COMPILE_INTERACTION_TABLE
			SCRIPTING_add_to_interaction_table (entity_id,entity[entity_id][data_type],DEBUG_INTERACTION_READ);
		#endif

		#ifdef RETRENGINE_DEBUG_VERSION_CHECK_VARIABLE_SCOPE
			if (data_value>MAX_ENTITY_VARIABLES)
			{
				OUTPUT_message("Variable outside of range!");
				assert(0);
			}
		#endif
	}
	else
	{
		value = data_value;
	}

	if ((data_bitmask & DATA_BITMASK_NEGATE_VALUE) > 0)
	{
		value *= -1;
	}

	if ((data_bitmask & DATA_BITMASK_INVERT_VALUE) > 0)
	{
		value = !value;
	}

	if (data_type == -1)
	{
		CONTROL_stop_and_save_active_channels ("empty_value_error");
		OUTPUT_message("Empty value! What the hell does that mean?!");
		assert(0);
	}

	if (value)
	{
		return true;
	}
	else
	{
		return false;
	}

}



int SCRIPTING_get_int_value ( int entity_id , int line_number , int argument )
{
	int data_type;
	int data_value;
	int data_bitmask;
	int value;

	if ((entity_id < 0) || (entity_id >= MAX_ENTITIES))
	{
		char msg[256];
		snprintf(msg, sizeof(msg), "SCRIPTING_get_int_value: invalid entity_id %d on line %d. Returning 0.", entity_id, line_number);
		MAIN_add_to_log(msg);
		return 0;
	}

	// Validate script line and argument to avoid dereferencing invalid pointers
	if ((line_number < 0) || (line_number >= script_line_count))
	{
		char msg[256];
		snprintf(msg, sizeof(msg), "SCRIPTING_get_int_value: invalid line_number %d (entity %d). Returning 0.", line_number, entity_id);
		MAIN_add_to_log(msg);
		return 0;
	}

	if (scr[line_number].script_line_pointer == NULL)
	{
		char msg[256];
		snprintf(msg, sizeof(msg), "SCRIPTING_get_int_value: NULL script_line_pointer for line %d (entity %d). Returning 0.", line_number, entity_id);
		MAIN_add_to_log(msg);
		return 0;
	}

	if ((argument < 0) || (argument >= scr[line_number].script_line_size))
	{
		char msg[256];
		snprintf(msg, sizeof(msg), "SCRIPTING_get_int_value: invalid argument %d for line %d (entity %d). Returning 0.", argument, line_number, entity_id);
		MAIN_add_to_log(msg);
		return 0;
	}

	data_type = scr[line_number].script_line_pointer[argument].data_type;
	data_value = scr[line_number].script_line_pointer[argument].data_value;
	data_bitmask = scr[line_number].script_line_pointer[argument].data_bitmask;

	if ((data_bitmask & DATA_BITMASK_IDENTITY_VARIABLE) > 0)
	{
		if ((data_type < 0) || (data_type >= MAX_ENTITY_VARIABLES))
		{
			char msg[256];
			snprintf(
				msg,
				sizeof(msg),
				"SCRIPTING_get_int_value: invalid identity variable %d for entity %d on line %d. Returning 0.",
				data_type,
				entity_id,
				line_number);
			MAIN_add_to_log(msg);
			return 0;
		}

		if ((data_value < 0) || (data_value >= MAX_ENTITY_VARIABLES))
		{
			char msg[256];
			snprintf(
				msg,
				sizeof(msg),
				"SCRIPTING_get_int_value: invalid target variable %d via identity read for entity %d on line %d. Returning 0.",
				data_value,
				entity_id,
				line_number);
			MAIN_add_to_log(msg);
			return 0;
		}

		int referenced_entity_id = entity[entity_id][data_type];

		if ((referenced_entity_id < 0) || (referenced_entity_id >= MAX_ENTITIES))
		{
			char msg[256];
			snprintf(
				msg,
				sizeof(msg),
				"SCRIPTING_get_int_value: invalid referenced entity %d from entity %d variable %d on line %d. Returning 0.",
				referenced_entity_id,
				entity_id,
				data_type,
				line_number);
			MAIN_add_to_log(msg);
			return 0;
		}

		value = entity[referenced_entity_id][data_value];

		#ifdef RETRENGINE_DEBUG_VERSION_COMPILE_INTERACTION_TABLE
			SCRIPTING_add_to_interaction_table (entity_id,referenced_entity_id,DEBUG_INTERACTION_READ);
		#endif

		#ifdef RETRENGINE_DEBUG_VERSION_CHECK_VARIABLE_SCOPE
			if (data_value>MAX_ENTITY_VARIABLES)
			{
				OUTPUT_message("Variable outside of range!");
				assert(0);
			}
		#endif
	}
	else if (data_type == VARIABLE)
	{
		if ((data_value < 0) || (data_value >= MAX_ENTITY_VARIABLES))
		{
			char msg[256];
			snprintf(
				msg,
				sizeof(msg),
				"SCRIPTING_get_int_value: invalid variable %d for entity %d on line %d. Returning 0.",
				data_value,
				entity_id,
				line_number);
			MAIN_add_to_log(msg);
			return 0;
		}

		#ifdef RETRENGINE_DEBUG_VERSION_CHECK_VARIABLE_SCOPE
			if (data_value>MAX_ENTITY_VARIABLES)
			{
				OUTPUT_message("Variable outside of range!");
				assert(0);
			}
		#endif

		value = entity [ entity_id ][data_value];
	}
	else
	{
		value = data_value;
	}

	if ((data_bitmask & DATA_BITMASK_NEGATE_VALUE) > 0)
	{
		value *= -1;
	}

	if ((data_bitmask & DATA_BITMASK_INVERT_VALUE) > 0)
	{
		value = ~value;
	}

	if (data_type == -1)
	{
		CONTROL_stop_and_save_active_channels ("empty_value_error");
		OUTPUT_message("Empty value! What the hell does that mean?!");
		assert(0);
	}

	return value;
}

int SCRIPTING_get_continue_compatibility_build_id(void)
{
	static const char *persistent_script_names[] =
	{
		"MAIN_GAME_CONTROLLER",
		"GENERIC_LEVEL_ENEMY",
		"MOLECULE"
	};
	unsigned int hash = 2166136261u;
	int script_name_index;

	for (script_name_index = 0; script_name_index < signed(sizeof(persistent_script_names) / sizeof(persistent_script_names[0])); script_name_index++)
	{
		const char *script_name = persistent_script_names[script_name_index];
		int script_number = GPL_find_word_value("SCRIPTS", (char *) script_name);

		SCRIPTING_hash_compatibility_text(&hash, script_name);

		if ((script_number == UNSET) || (scr_lookup == NULL) || (scr == NULL))
		{
			SCRIPTING_hash_compatibility_int(&hash, UNSET);
			continue;
		}

		SCRIPTING_hash_compatibility_int(&hash, script_number);

		if ((script_number < 0) || (script_number >= script_count))
		{
			SCRIPTING_hash_compatibility_int(&hash, UNSET - 1);
			continue;
		}

		{
			int line_number = scr_lookup[script_number];
			int end_line_number = scr_lookup[script_number + 1];

			SCRIPTING_hash_compatibility_int(&hash, end_line_number - line_number);

			for (; line_number < end_line_number; line_number++)
			{
				int argument_number;

				SCRIPTING_hash_compatibility_int(&hash, scr[line_number].script_line_size);
				SCRIPTING_hash_compatibility_int(&hash, scr[line_number].indentation_level);

				for (argument_number = 0; argument_number < scr[line_number].script_line_size; argument_number++)
				{
					SCRIPTING_hash_compatibility_int(&hash, scr[line_number].script_line_pointer[argument_number].data_type);
					SCRIPTING_hash_compatibility_int(&hash, scr[line_number].script_line_pointer[argument_number].data_value);
					SCRIPTING_hash_compatibility_int(&hash, scr[line_number].script_line_pointer[argument_number].data_bitmask);
				}
			}
		}
	}

	return (int)(hash & 0x7fffffff);
}



int SCRIPTING_spawn_entity (int calling_entity_id , int script_number , int x_offset , int y_offset , int process_offset , int specific_entity=UNSET )
{
	// Creates an entity and sticks the ID into the parent's CHILD_ID variable.

	// Also places it into the processing order wherever you like.

	// At the moment entities placed onto the end of the stack are linking with the
	// last entity which was created, rather than the entity which is processed last. FIX!

	int temp_prev,temp_next;

	int *calling_entity_pointer;
	int *just_created_entity_pointer;
	static int cached_create_normal_enemies_script = UNSET;
	static int cached_player_on_level_number_flag = UNSET;
	static int cached_enemy_wave_count_in_level_flag = UNSET;

	if (SCRIPTING_is_valid_script_index(script_number) == false)
	{
		char line[MAX_LINE_SIZE];
		snprintf(line, sizeof(line), "Invalid spawn script index (%d) requested by entity %d.", script_number, calling_entity_id);
		MAIN_add_to_log(line);
		return UNSET;
	}

	if (cached_create_normal_enemies_script == UNSET)
	{
		cached_create_normal_enemies_script = GPL_find_word_value("SCRIPTS", "CREATE_NORMAL_ENEMIES");
	}

	if (cached_player_on_level_number_flag == UNSET)
	{
		cached_player_on_level_number_flag = GPL_find_word_value("FLAG", "PLAYER_ON_LEVEL_NUMBER");
	}

	if (cached_enemy_wave_count_in_level_flag == UNSET)
	{
		cached_enemy_wave_count_in_level_flag = GPL_find_word_value("FLAG", "ENEMY_WAVE_COUNT_IN_LEVEL");
	}

	calling_entity_pointer = &entity[calling_entity_id][0];

//	first_processed_entity_in_list;
//	last_processed_entity_in_list;
//	just_created_entity;

	if (SCRIPTING_find_next_entity() == true)
	{
		just_created_entity_pointer = &entity[just_created_entity][0];

		int temp_id;

		if ( calling_entity_pointer[ENT_LAST_CHILD] != UNSET ) // if this entity has had a child before then we need to plop that into the new entity's PREV_SIBLING and this new id into the old child's NEXT_SIBLING id.
		{
			temp_id = entity[calling_entity_id][ENT_LAST_CHILD];
			just_created_entity_pointer[ENT_PREV_SIBLING] = temp_id;
			entity[temp_id][ENT_NEXT_SIBLING] = just_created_entity;
		}

		if (first_processed_entity_in_list == UNSET) // It's the first thing we're creating
		{
			// So we point both it's previous and next processes to UNSET.
			just_created_entity_pointer[ENT_PREV_PROCESS_ENT] = UNSET;
			just_created_entity_pointer[ENT_NEXT_PROCESS_ENT] = UNSET; // These really should already be set...
			first_processed_entity_in_list = just_created_entity;
			last_processed_entity_in_list = just_created_entity;
		}
		else // It's being added to the stack so get it to point at the last thing and get the last thing to point at it.
		{
			switch (process_offset)
			{
			case PROCESS_FIRST: // It needs to be dumped at the start of the processing queue.
				// So first we set the previous to UNSET.
				just_created_entity_pointer[ENT_PREV_PROCESS_ENT] = UNSET;
				// And this entity's next pointer is pointed to the current first entity.
				just_created_entity_pointer[ENT_NEXT_PROCESS_ENT] = first_processed_entity_in_list;
				// The current first entity's previous pointer is pointed to the current one.
				entity [first_processed_entity_in_list][ENT_PREV_PROCESS_ENT] = just_created_entity;
				// And the first entity is set to the current one.
				first_processed_entity_in_list = just_created_entity;
				break;

			case PROCESS_PREVIOUS: // It needs to be placed just before the calling entity.
				// First of all we need to store where calling entity's previous pointer current points at.
				temp_prev = entity [calling_entity_id][ENT_PREV_PROCESS_ENT];
				if (temp_prev == UNSET) // The calling entity was the first in the queue so there is no previous one. So stick it at the start of the queue.
				{
					// So first we set the previous to UNSET.
					just_created_entity_pointer[ENT_PREV_PROCESS_ENT] = UNSET;
					// The calling entity's previous pointer is pointed to the current one.
					just_created_entity_pointer[ENT_NEXT_PROCESS_ENT] = calling_entity_id;
					// And this entity's next pointer is pointed to the calling entity.
					entity [first_processed_entity_in_list][ENT_PREV_PROCESS_ENT] = just_created_entity;
					// And the first entity is set to the current one.
					first_processed_entity_in_list = just_created_entity;	
				}
				else // There was at least one before the calling entity.
				{
					// Then we need to point that previous entity at the new one...
					entity [temp_prev][ENT_NEXT_PROCESS_ENT] = just_created_entity;
					// Then we need to point the calling entity's previous pointer at the current one.
					calling_entity_pointer[ENT_PREV_PROCESS_ENT] = just_created_entity;
					// And now get the current one to point forwards and backwards to those things.
					just_created_entity_pointer[ENT_PREV_PROCESS_ENT] = temp_prev;
					just_created_entity_pointer[ENT_NEXT_PROCESS_ENT] = calling_entity_id;
				}
				break;

			case PROCESS_BEFORE_SPECIFIC:
				temp_prev = entity[specific_entity][ENT_PREV_PROCESS_ENT];

				if (temp_prev == UNSET) // The calling entity was the first in the queue so there is no previous one. So stick it at the start of the queue.
				{
					// So first we set the previous to UNSET.
					just_created_entity_pointer[ENT_PREV_PROCESS_ENT] = UNSET;
					// The calling entity's previous pointer is pointed to the current one.
					just_created_entity_pointer[ENT_NEXT_PROCESS_ENT] = specific_entity;
					// And this entity's next pointer is pointed to the calling entity.
					entity [first_processed_entity_in_list][ENT_PREV_PROCESS_ENT] = just_created_entity;
					// And the first entity is set to the current one.
					first_processed_entity_in_list = just_created_entity;	
				}
				else // There was at least one before the calling entity.
				{
					entity [temp_prev][ENT_NEXT_PROCESS_ENT] = just_created_entity;
					entity[specific_entity][ENT_PREV_PROCESS_ENT] = just_created_entity;
					just_created_entity_pointer[ENT_PREV_PROCESS_ENT] = temp_prev;
					just_created_entity_pointer[ENT_NEXT_PROCESS_ENT] = specific_entity;
				}

				// If the entity we were creating "before" was the old next entity...
				if (calling_entity_pointer[ENT_NEXT_PROCESS_ENT] == just_created_entity)
				{
					global_next_entity = just_created_entity;
				}
				break;

			case PROCESS_NEXT: // It needs to be placed just after the calling entity.
				// First of all we need to store where calling entity's next pointer current points at.
				temp_next = entity [calling_entity_id][ENT_NEXT_PROCESS_ENT];
				if (temp_next == UNSET) // The calling entity was the last entity so we need to just bung it on the end of the queue.
				{
					just_created_entity_pointer[ENT_PREV_PROCESS_ENT] = calling_entity_id;
					just_created_entity_pointer[ENT_NEXT_PROCESS_ENT] = UNSET;
					calling_entity_pointer[ENT_NEXT_PROCESS_ENT] = just_created_entity;
					last_processed_entity_in_list = just_created_entity;
				}
				else // There's at least one entity after the calling entity.
				{
					// Then we need to point that next entity's previous at the new one...
					entity [temp_next][ENT_PREV_PROCESS_ENT] = just_created_entity;
					// Then we need to point the calling entity's next pointer at the current one.
					calling_entity_pointer[ENT_NEXT_PROCESS_ENT] = just_created_entity;
					// And now get the current one to point forwards and backwards to those things.
					just_created_entity_pointer[ENT_PREV_PROCESS_ENT] = calling_entity_id;
					just_created_entity_pointer[ENT_NEXT_PROCESS_ENT] = temp_next;
				}
				global_next_entity = just_created_entity;
				break;

			case PROCESS_AFTER_SPECIFIC: // It needs to be placed after the supplied entity ID
				temp_next = entity[specific_entity][ENT_NEXT_PROCESS_ENT];
				
				if (temp_next == UNSET) // The calling entity was the last entity so we need to just bung it on the end of the queue.
				{
					just_created_entity_pointer[ENT_PREV_PROCESS_ENT] = specific_entity;
					just_created_entity_pointer[ENT_NEXT_PROCESS_ENT] = UNSET;
					entity[specific_entity][ENT_NEXT_PROCESS_ENT] = just_created_entity;
					last_processed_entity_in_list = just_created_entity;
				}
				else // There's at least one entity after the calling entity.
				{
					// Then we need to point that next entity's previous at the new one...
					entity [temp_next][ENT_PREV_PROCESS_ENT] = just_created_entity;
					// Then we need to point the calling entity's next pointer at the current one.
					calling_entity_pointer[ENT_NEXT_PROCESS_ENT] = just_created_entity;
					// And now get the current one to point forwards and backwards to those things.
					just_created_entity_pointer[ENT_PREV_PROCESS_ENT] = calling_entity_id;
					just_created_entity_pointer[ENT_NEXT_PROCESS_ENT] = temp_next;
				}
				
				if (specific_entity==calling_entity_id)
				{
					global_next_entity = just_created_entity;
				}
				break;

			case PROCESS_LAST: // Just dump it unceremoniously on the end of the stack.
				just_created_entity_pointer[ENT_PREV_PROCESS_ENT] = last_processed_entity_in_list;
				just_created_entity_pointer[ENT_NEXT_PROCESS_ENT] = UNSET;
				entity [last_processed_entity_in_list][ENT_NEXT_PROCESS_ENT] = just_created_entity;
				last_processed_entity_in_list = just_created_entity;
				break;
			}

		}

		if (calling_entity_pointer[ENT_FIRST_CHILD] == UNSET)
		{
			calling_entity_pointer[ENT_FIRST_CHILD] = just_created_entity;
		}

		calling_entity_pointer[ENT_LAST_CHILD] = just_created_entity;
		just_created_entity_pointer[ENT_PARENT] = calling_entity_id;
		just_created_entity_pointer[ENT_MATRIARCH] = calling_entity_pointer[ENT_MATRIARCH];

		just_created_entity_pointer[ENT_WINDOW_NUMBER] = calling_entity_pointer[ENT_WINDOW_NUMBER];

		if ( (process_offset == PROCESS_NEXT) && (not_in_collision_phase == true) )
		{
			just_created_entity_pointer[ENT_ALIVE] = ALIVE;
			// As it will get to run it's script for certain this frame, there's no need for it
			// to have JUST_BORN status at all.
		}
		else
		{
			just_created_entity_pointer[ENT_ALIVE] = JUST_BORN;
		}
		just_created_entity_pointer[ENT_PROGRAM_START] = scr_lookup[script_number];
		just_created_entity_pointer[ENT_WORLD_X] = calling_entity_pointer[ENT_WORLD_X] + x_offset;
		just_created_entity_pointer[ENT_WORLD_Y] = calling_entity_pointer[ENT_WORLD_Y] + y_offset;
		just_created_entity_pointer[ENT_X] = just_created_entity_pointer[ENT_WORLD_X];
		just_created_entity_pointer[ENT_Y] = just_created_entity_pointer[ENT_WORLD_Y];
			just_created_entity_pointer[ENT_DRAW_ORDER] = calling_entity_pointer[ENT_DRAW_ORDER];
			just_created_entity_pointer[ENT_SCRIPT_NUMBER] = script_number;

			if (output_debug_information && SCRIPTING_is_focus_script_index(script_number))
			{
				const char *created_name = GPL_get_entry_name("SCRIPTS", script_number);
				const char *caller_name = "UNSET";
				if ((calling_entity_id >= 0) && (calling_entity_id < MAX_ENTITIES))
				{
					int caller_script = entity[calling_entity_id][ENT_SCRIPT_NUMBER];
					if ((caller_script >= 0) && (caller_script < script_count))
					{
						caller_name = GPL_get_entry_name("SCRIPTS", caller_script);
					}
				}

				char line[MAX_LINE_SIZE];
				snprintf(line, sizeof(line), "FOCUS_SPAWN child=%d(%s) caller=%d(%s) frame=%d", just_created_entity, created_name, calling_entity_id, caller_name, frames_so_far);
				MAIN_add_to_log(line);
			}

			return just_created_entity;

	}
	else
	{
		return UNSET;
		// Arse! There was no room for the item in question. We'll need to do summat about killing low-priority in future...
	}
}



void SCRIPTING_spawn_shutdown_entity (void)
{
	SCRIPTING_spawn_entity (first_processed_entity_in_list, GPL_find_word_value ("SCRIPTS", "SHUTDOWN"), 0, 0, PROCESS_FIRST);
}



void SCRIPTING_overwrite_siblings_with_word (int first_entity, char *word, bool replace_trailing_entities=false)
{
	// Pass it a word and it'll overwrite the frame numbers of all the children from the one
	// given onwards with the letters. This is so you can overwrite menu options without
	// spawning the text from scratch, etc. If there are more letters in the word than
	// entities then the word is truncated, if there are more entities than letters in the
	// word then the remainders are either set to frame 0 or left unchanged depending on the
	// passed boolean. Ideally you should know what you're passing in, though.

	int entity_index;
	int next_entity = first_entity;
	int counter = 0;
	int length = strlen(word);

	do
	{
		entity_index = next_entity;
		next_entity = entity[entity_index][ENT_NEXT_SIBLING];

		entity[entity_index][ENT_CURRENT_FRAME] = word[counter]-32;

		counter++;
	}
	while ( (next_entity != UNSET) && (counter < length) );
}



void SCRIPTING_spawn_entities_from_word (bool include_spaces, int spawn_identity, char *word, int calling_entity, int script_number, int process_offset, int offset_x, int offset_y, int gap_x, int gap_y, int first_letter = 0, int letter_count = UNSET, int graphic_number = UNSET)
{
	int line_length = strlen(word);
	int start,end;

	if (letter_count == UNSET)
	{
		start = 0;
		end = line_length;
	}
	else
	{
		start = first_letter;
		end = start + letter_count;
		
		if (end > line_length)
		{
			end = line_length;
		}
	}

	int total_length = end - start;

	int letter;
	int new_entity;

	for (letter=start; letter<end; letter++)
	{
		if ((word[letter]-32 != 0) || (include_spaces))
		{
			// We don't wanna' spawn spaces now, do we?
			
			new_entity = SCRIPTING_spawn_entity (calling_entity,script_number,offset_x,offset_y,process_offset);

			if (new_entity != UNSET)
			{
				entity[new_entity][ENT_CURRENT_FRAME] = word[letter]-32;
				entity[new_entity][ENT_PASSED_PARAM_1] = letter-start;
				entity[new_entity][ENT_PASSED_PARAM_2] = spawn_identity;
				entity[new_entity][ENT_PASSED_PARAM_3] = total_length;
			}
			else
			{
				// We're done here, laddy. If we couldn't create that letter, there's no reason we'll be able to create any more...
				return;
			}
		}

		offset_x += gap_x;
		offset_y += gap_y;

		if (graphic_number != UNSET)
		{
			offset_x += OUTPUT_get_sprite_width(graphic_number,word[letter]-32);
		}
	}
}



int SCRIPTING_alter_text_spawn_offsets (char *word, int centering, int *offset_x, int *offset_y, int gap_x, int gap_y, int graphic_number = UNSET)
{
	int length = strlen(word);

	int graphic_width = 0;

	if (graphic_number != UNSET)
	{
		int letter;

		for (letter=0; letter<length; letter++)
		{
			graphic_width += OUTPUT_get_sprite_width(graphic_number,word[letter]-32);
			graphic_width += gap_x;
		}
		
		graphic_width -= gap_x; // Knock off the last gap...
	}
	else
	{
		graphic_width = gap_x * length;
	}



	if (centering & TEXT_OUTPUT_RIGHT_ALIGN)
	{
		*offset_x -= graphic_width;
	}

	if (centering & TEXT_OUTPUT_CENTER_X)
	{
		*offset_x -= (graphic_width/2);
	}

	if (centering & TEXT_OUTPUT_CENTER_Y)
	{
		*offset_y -= (gap_y * length)/2;
	}

	return length;
}



void SCRIPTING_spawn_entities_from_number ( int spawn_identity, int calling_entity, int script_number, int value, int number_of_digits, bool pad_with_zeroes, int process_offset, int centering, int offset_x, int offset_y, int gap_x, int gap_y)
{
	char *word = STRING_get_number_as_string (value, number_of_digits, pad_with_zeroes);

	int length = SCRIPTING_alter_text_spawn_offsets (word, centering, &offset_x, &offset_y, gap_x, gap_y);

	SCRIPTING_spawn_entities_from_word (true, spawn_identity, word, calling_entity, script_number, process_offset, offset_x, offset_y, gap_x, gap_y, 0, length );
}



void SCRIPTING_overwrite_entities_from_number (bool replace_trailing_entities, int first_entity, int value, int number_of_digits, bool pad_with_zeroes)
{
	char *word = STRING_get_number_as_string (value, number_of_digits, pad_with_zeroes);

	SCRIPTING_overwrite_siblings_with_word (first_entity, word, replace_trailing_entities);
}



void SCRIPTING_spawn_entities_from_text_in_array (bool include_spaces, int spawn_identity, int calling_entity, int script_number, int unique_id, int process_offset, int centering, int offset_x, int offset_y, int gap_x, int gap_y, int graphic_number)
{
	char *word = ARRAY_get_array_as_text (calling_entity,unique_id,UNSET);

	int length = SCRIPTING_alter_text_spawn_offsets (word, centering, &offset_x, &offset_y, gap_x, gap_y, graphic_number);

	SCRIPTING_spawn_entities_from_word (include_spaces, spawn_identity, word, calling_entity, script_number, process_offset, offset_x, offset_y, gap_x, gap_y, 0, length , graphic_number);
}



void SCRIPTING_overwrite_entities_from_text_in_array (bool replace_trailing_entities, int first_entity, int unique_id, int calling_entity)
{
	char *word = ARRAY_get_array_as_text (calling_entity,unique_id,UNSET);

	SCRIPTING_overwrite_siblings_with_word (first_entity, word, replace_trailing_entities);
}



void SCRIPTING_spawn_entities_from_text (bool include_spaces, int spawn_identity, int calling_entity, int script_number, int text_tag, int process_offset, int centering, int offset_x, int offset_y, int gap_x, int gap_y, int first_letter = 0, int letter_count = UNSET , int graphic_number = UNSET )
{
	static char word_copy[TEXTFILE_SUPER_SIZE];
	int length;

	char *word = TEXTFILE_get_line_by_index (text_tag);

	if (letter_count != UNSET)
	{
		length = strlen(word);

		if (first_letter<length)
		{
			// Good, we ain't trying to get letters outside the scope of the word.

			if (first_letter+letter_count > length)
			{
				// If we're trying to pull too many letters, truncate it.
				letter_count = length - first_letter;
			}

			strcpy (word_copy,&word[first_letter]);
			word_copy[letter_count] = '\0';

			SCRIPTING_alter_text_spawn_offsets (word_copy, centering, &offset_x, &offset_y, gap_x, gap_y, graphic_number);

			SCRIPTING_spawn_entities_from_word (include_spaces, spawn_identity, word_copy, calling_entity, script_number, process_offset, offset_x, offset_y, gap_x, gap_y, 0, letter_count , graphic_number );
		}
	}
	else
	{
		length = SCRIPTING_alter_text_spawn_offsets (word, centering, &offset_x, &offset_y, gap_x, gap_y, graphic_number);

		SCRIPTING_spawn_entities_from_word (include_spaces, spawn_identity, word, calling_entity, script_number, process_offset, offset_x, offset_y, gap_x, gap_y, first_letter, length , graphic_number );
	}
}



void SCRIPTING_overwrite_entities_from_text (bool replace_trailing_entities, int first_entity, int text_tag, int first_letter = 0, int letter_count = UNSET)
{
	static char word_copy[TEXTFILE_SUPER_SIZE];

	char *word = TEXTFILE_get_line_by_index (text_tag);
	int length = strlen(word);

	if (letter_count != UNSET)
	{
		if (first_letter<length)
		{
			// Good, we ain't trying to get letters outside the scope of the word.

			if (first_letter+letter_count > length)
			{
				// If we're trying to pull too many letters, truncate it.
				letter_count = length - first_letter;
			}

			strcpy (word_copy,&word[first_letter]);
			word_copy[letter_count] = '\0';

			SCRIPTING_overwrite_siblings_with_word (first_entity, word_copy, replace_trailing_entities);
		}
	}
	else
	{
		SCRIPTING_overwrite_siblings_with_word (first_entity, word, replace_trailing_entities);
	}
}



void SCRIPTING_spawn_entities_from_hiscore_name (bool include_spaces, int spawn_identity, int calling_entity, int script_number, int unique_id, int entry, int process_offset, int centering, int offset_x, int offset_y, int gap_x, int gap_y, int graphic_number)
{
	char *word = HISCORE_get_hiscore_name (unique_id, entry);

	if (word == NULL)
	{
		OUTPUT_message("No returned name due to inaccurate hiscore table information");
		assert(0);
		return;
	}

	int length = SCRIPTING_alter_text_spawn_offsets (word, centering, &offset_x, &offset_y, gap_x, gap_y, graphic_number);

	SCRIPTING_spawn_entities_from_word (include_spaces, spawn_identity, word, calling_entity, script_number, process_offset, offset_x, offset_y, gap_x, gap_y, 0, length , graphic_number);
}



void SCRIPTING_overwrite_entities_from_hiscore_name (bool replace_trailing_entities, int first_entity, int unique_id, int entry)
{
	char *word = HISCORE_get_hiscore_name (unique_id, entry);

	if (word == NULL)
	{
		OUTPUT_message("No returned name due to inaccurate hiscore table information");
		assert(0);
		return;
	}

	SCRIPTING_overwrite_siblings_with_word (first_entity, word, replace_trailing_entities);
}



void SCRIPTING_spawn_entities_from_hiscore_score (bool include_spaces, int spawn_identity, int calling_entity, int script_number, int unique_id, int entry, bool pad_with_zeros, int process_offset, int centering, int offset_x, int offset_y, int gap_x, int gap_y, int graphic_number)
{
	char *word = HISCORE_get_hiscore_score (unique_id, entry, pad_with_zeros);

	if (word == NULL)
	{
		OUTPUT_message("No returned name due to inaccurate hiscore table information");
		assert(0);
		return;
	}

	int length = SCRIPTING_alter_text_spawn_offsets (word, centering, &offset_x, &offset_y, gap_x, gap_y, graphic_number);

	SCRIPTING_spawn_entities_from_word (include_spaces, spawn_identity, word, calling_entity, script_number, process_offset, offset_x, offset_y, gap_x, gap_y, 0, UNSET, graphic_number);
}



void SCRIPTING_overwrite_entities_from_hiscore_score (bool replace_trailing_entities, int first_entity, int unique_id, int entry, bool pad_with_zeros)
{
	char *word = HISCORE_get_hiscore_score (unique_id, entry, pad_with_zeros);

	if (word == NULL)
	{
		OUTPUT_message("No returned name due to inaccurate hiscore table information");
		assert(0);
		return;
	}

	SCRIPTING_overwrite_siblings_with_word (first_entity, word, replace_trailing_entities);
}



void SCRIPTING_spawn_entities_from_hiscore (bool include_spaces, int spawn_identity, int calling_entity, int script_number, int unique_id, int entry, bool pad_with_zeros, bool name_first, int letter_gap, int process_offset, int centering, int offset_x, int offset_y, int gap_x, int gap_y, int first_letter = 0, int letter_count = UNSET)
{
	char *word = HISCORE_get_hiscore_name_and_score (unique_id, entry, pad_with_zeros, name_first, letter_gap);

	if (word == NULL)
	{
		OUTPUT_message("No returned name due to inaccurate hiscore table information");
		assert(0);
		return;
	}

	int length = SCRIPTING_alter_text_spawn_offsets (word, centering, &offset_x, &offset_y, gap_x, gap_y);

	SCRIPTING_spawn_entities_from_word (include_spaces, spawn_identity, word, calling_entity, script_number, process_offset, offset_x, offset_y, gap_x, gap_y, first_letter, length );
}



void SCRIPTING_overwrite_entities_from_hiscore (bool replace_trailing_entities, int first_entity, int unique_id, int entry, bool pad_with_zeros, bool name_first, int letter_gap)
{
	char *word = HISCORE_get_hiscore_name_and_score (unique_id, entry, pad_with_zeros, name_first, letter_gap);

	if (word == NULL)
	{
		OUTPUT_message("No returned name due to inaccurate hiscore table information");
		assert(0);
		return;
	}
	
	SCRIPTING_overwrite_siblings_with_word (first_entity, word, replace_trailing_entities);
}



int SCRIPTING_count_matching_entities (int compare_variable, int compare_operation, int compare_value, int refine)
{
	int value;
	int result;

	int entity_id = first_processed_entity_in_list;
	int counter = 0;

	while (entity_id != UNSET)
	{
		if ((refine == 0) || (entity_search_inclusion[entity_id] == true))
		{
			value = entity[entity_id][compare_variable];

			result = false;

			switch (compare_operation)
			{
				case CMP_LESS_THAN:
					result = (value < compare_value);
				break;

				case CMP_LESS_THAN_OR_EQUAL:
					result = (value <= compare_value);
				break;

				case CMP_EQUAL:
					result = (value == compare_value);
				break;

				case CMP_GREATER_THAN_OR_EQUAL:
					result = (value >= compare_value);
				break;

				case CMP_GREATER_THAN:
					result = (value > compare_value);
				break;

				case CMP_UNEQUAL:
					result = (value != compare_value);
				break;

				case CMP_BITWISE_AND:
					result = (value & compare_value);
				break;

				case CMP_BITWISE_OR:
					result = (value | compare_value);
				break;

				case CMP_NOT_BITWISE_AND:
					result = !(value & compare_value);
				break;

				case CMP_NOT_BITWISE_OR:
					result = !(value | compare_value);
				break;
				
				default:
					assert(0);
				break;
			}

			if (result)
			{
				counter++;
				entity_search_inclusion[entity_id] = true;
			}
			else
			{
				entity_search_inclusion[entity_id] = false;
			}

		}

		entity_id = entity [entity_id][ENT_NEXT_PROCESS_ENT];
	}

	return (counter);
}



void SCRIPTING_spawn_entity_by_name (char *script_name , int x , int y , int v0 , int v1 , int v2 )
{
	// This is just like the last one only it's being created by a function in the program rather than
	// another entity and so it doesn't have a calling_entity_id and it doesn't care about waves. However it
	// does allow you to specify the first three values v0, v1 and v2 which can then be used by the called
	// script to do stuff.

	int script_number = int (GPL_find_word_value ("SCRIPTS", script_name));
	if (SCRIPTING_is_valid_script_index(script_number) == false)
	{
		char line[MAX_LINE_SIZE];
		snprintf(line, sizeof(line), "Invalid top-level spawn script '%s' (index %d).", script_name, script_number);
		MAIN_add_to_log(line);
		return;
	}

	SCRIPTING_find_next_entity();

	int *just_created_entity_pointer = &entity[just_created_entity][0];

	just_created_entity_pointer[ENT_ALIVE] = ALIVE;
	just_created_entity_pointer[ENT_SCRIPT_NUMBER] = script_number;
	just_created_entity_pointer[ENT_PROGRAM_START] = scr_lookup[just_created_entity_pointer[ENT_SCRIPT_NUMBER]];
	just_created_entity_pointer[ENT_V0] = v0;
	just_created_entity_pointer[ENT_V1] = v1;
	just_created_entity_pointer[ENT_V2] = v2;
	just_created_entity_pointer[ENT_DRAW_ORDER] = 0;
	just_created_entity_pointer[ENT_WORLD_X] = x;
	just_created_entity_pointer[ENT_WORLD_Y] = y;

	just_created_entity_pointer[ENT_NEXT_PROCESS_ENT] = UNSET;
	just_created_entity_pointer[ENT_PREV_PROCESS_ENT] = UNSET;

	first_processed_entity_in_list = just_created_entity;

	if (output_debug_information && SCRIPTING_is_focus_script_index(script_number))
	{
		char line[MAX_LINE_SIZE];
		snprintf(line, sizeof(line), "FOCUS_TOPLEVEL entity=%d(%s) frame=%d", just_created_entity, GPL_get_entry_name("SCRIPTS", script_number), frames_so_far);
		MAIN_add_to_log(line);
	}

}



void SCRIPTING_contain_entity (int *entity_pointer)
{
	if (entity_pointer[ENT_WORLD_X] < entity_pointer[ENT_BOUNDARY_START_X])
	{
		entity_pointer[ENT_X] += ( entity_pointer[ENT_BOUNDARY_START_X] - entity_pointer[ENT_WORLD_X]) >> entity_pointer[ENT_BITSHIFT];
	}
	
	if (entity_pointer[ENT_WORLD_X] > entity_pointer[ENT_BOUNDARY_END_X])
	{
		entity_pointer[ENT_X] += ( entity_pointer[ENT_BOUNDARY_END_X] - entity_pointer[ENT_WORLD_X]) >> entity_pointer[ENT_BITSHIFT];
	}

	if (entity_pointer[ENT_WORLD_Y] < entity_pointer[ENT_BOUNDARY_START_Y])
	{
		entity_pointer[ENT_Y] += ( entity_pointer[ENT_BOUNDARY_START_Y] - entity_pointer[ENT_WORLD_Y]) >> entity_pointer[ENT_BITSHIFT];
	}
	
	if (entity_pointer[ENT_WORLD_Y] > entity_pointer[ENT_BOUNDARY_END_Y])
	{
		entity_pointer[ENT_Y] += ( entity_pointer[ENT_BOUNDARY_END_Y] - entity_pointer[ENT_WORLD_Y]) >> entity_pointer[ENT_BITSHIFT];
	}
}



void SCRIPTING_get_position_at_end_of_rotated_entity (int *entity_pointer, int angle, int distance , int other_entity, int percentage_along)
{
	float percent = float (percentage_along) / 10000.0f;

	int *other_entity_pointer = &entity[other_entity][0];

	float f_offset;
	float f_angle;

	f_offset = float (distance) * percent;

	f_angle = float (angle) / (angle_conversion_ratio);

	float new_offset_x,new_offset_y;

	new_offset_y = f_offset * float(cos(f_angle));
	new_offset_x = -f_offset * float(sin(f_angle));

	entity_pointer [ENT_WORLD_X] = other_entity_pointer[ENT_WORLD_X] + int (new_offset_x);
	entity_pointer [ENT_WORLD_Y] = other_entity_pointer[ENT_WORLD_Y] + int (new_offset_y);
}



#define ANIM_LOOP					(0)
#define ANIM_LOOP_PAUSE				(1)
#define ANIM_PING_PONG				(2)
#define ANIM_PING_PONG_PAUSE		(3)
#define ANIM_PING_PAUSE_PONG		(4)
#define ANIM_PING_PAUSE_PONG_PAUSE	(5)
#define ANIM_RANDOM					(6)
#define ANIM_RANDOM_PAUSE			(7)
#define ANIM_SPLIT					(8)

void SCRIPTING_select_animation_frame (int *entity_pointer, int animation_method, int base_frame, int frame_count, int delay, int counter, bool reverse, int pause_1=0, int pause_2=0)
{
	int frame_number;
	int modded_counter;
	int first_sequence_end;
	int second_sequence_end;
	int first_pause_end;
	int second_pause_end;

	switch (animation_method)
	{
	case ANIM_LOOP:
		first_sequence_end = frame_count * delay;

		modded_counter = counter % first_sequence_end;

		frame_number = base_frame + (modded_counter / delay);

		break;

	case ANIM_LOOP_PAUSE:
		first_sequence_end = (frame_count * delay);
		first_pause_end = first_sequence_end + pause_1;

		modded_counter = counter % first_pause_end;

		if (modded_counter >= first_sequence_end)
		{
			frame_number = frame_count - 1;
		}
		else
		{
			frame_number = modded_counter / delay;
		}

		frame_number += base_frame;
		break;

	case ANIM_PING_PONG:
		first_sequence_end = (frame_count-1) * delay;
		second_sequence_end = first_sequence_end + ((frame_count-1) * delay);

		modded_counter = counter % second_sequence_end;

		if (modded_counter >= first_sequence_end)
		{
			frame_number = frame_count - ((modded_counter - first_sequence_end) / delay) - 1;
		}
		else
		{
			frame_number = modded_counter / delay;
		}

		frame_number += base_frame;
		break;

	case ANIM_PING_PONG_PAUSE:
		first_sequence_end = (frame_count-1) * delay;
		second_sequence_end = first_sequence_end + first_sequence_end; // As it's the same size...
		first_pause_end = second_sequence_end + pause_1;

		modded_counter = counter % first_pause_end;

		if (modded_counter >= second_sequence_end)
		{
			frame_number = 0;
		}
		else if (modded_counter >= first_sequence_end)
		{
			frame_number = frame_count - ((modded_counter - first_sequence_end) / delay);
		}
		else
		{
			frame_number = modded_counter / delay;
		}

		frame_number += base_frame;
		break;

	case ANIM_PING_PAUSE_PONG:
		first_sequence_end = (frame_count-1) * delay;
		first_pause_end = first_sequence_end + pause_1;
		second_sequence_end = first_pause_end + first_sequence_end; // As it's the same size...	

		modded_counter = counter % second_sequence_end;

		if (modded_counter >= first_pause_end)
		{
			frame_number = frame_count - ((modded_counter - first_pause_end) / delay);
		}
		else if (modded_counter >= first_sequence_end)
		{
			frame_number = frame_count - 1;
		}
		else
		{
			frame_number = modded_counter / delay;
		}

		frame_number += base_frame;
		break;

	case ANIM_PING_PAUSE_PONG_PAUSE:
		first_sequence_end = (frame_count-1) * delay;
		first_pause_end = first_sequence_end + pause_1;
		second_sequence_end = first_pause_end + first_sequence_end; // As it's the same size...	
		second_pause_end = second_sequence_end + pause_2;

		modded_counter = counter % second_sequence_end;

		if (modded_counter >= second_sequence_end)
		{
			frame_number = 0;
		}
		else if (modded_counter >= first_pause_end)
		{
			frame_number = frame_count - ((modded_counter - first_pause_end) / delay);
		}
		else if (modded_counter >= first_sequence_end)
		{
			frame_number = frame_count - 1;
		}
		else
		{
			frame_number = modded_counter / delay;
		}

		frame_number += base_frame;
		break;

	case ANIM_RANDOM:
		frame_number = base_frame + MATH_rand(base_frame, base_frame+frame_count-1);
		break;

	case ANIM_RANDOM_PAUSE:
		if (counter % delay == 0)
		{
			frame_number = base_frame + MATH_rand(base_frame, base_frame+frame_count-1);
		}
		else
		{
			frame_number = entity_pointer[ENT_CURRENT_FRAME];
		}
		break;

	case ANIM_SPLIT:
		first_sequence_end = frame_count * delay;

		if (MATH_abs_int(counter) >= first_sequence_end)
		{
			if (counter > 0)
			{
				counter = first_sequence_end - 1;
			}
			else if (counter < 0)
			{
				counter = -(first_sequence_end - 1);
			}
		}

		frame_number = base_frame + (counter / delay);

		frame_number += base_frame;
		break;

	default:
		assert(0);
		break;
	}

	entity_pointer[ENT_CURRENT_FRAME] = frame_number;
}



int SCRIPTING_interpret_script (int entity_id , int over_ride_line)
{
	int line_pointer = 0;

	int *entity_pointer;
	int script_lines_executed = 0;
	static char line[MAX_LINE_SIZE];
	static int cached_create_normal_enemies_script_for_interpret = UNSET;

	entity_pointer = &entity[entity_id][0];

	if (cached_create_normal_enemies_script_for_interpret == UNSET)
	{
		cached_create_normal_enemies_script_for_interpret = GPL_find_word_value("SCRIPTS", "CREATE_NORMAL_ENEMIES");
	}

	if ( (int (entity_pointer[ENT_WAIT_COUNTER]) > 1) && (over_ride_line == UNSET) )
	{
		entity_pointer[ENT_WAIT_COUNTER]--;
	}
	else
	{
		// Okay, this is the biggy. Hopefully chunks of it will get reused a lot in other programs...

		int return_pointer_stack[RETURN_POINTER_STACK_SIZE];
		int return_pointer_stack_pointer = UNSET;
		int line_number;
		int old_line_number;
		int instruction;
		int for_line_number[FOR_STACK_SIZE];
		int for_entity_id[FOR_STACK_SIZE];
		int for_variable_number[FOR_STACK_SIZE];
		int for_step_value[FOR_STACK_SIZE];
		int for_end_value[FOR_STACK_SIZE];
		int for_stack_pointer = UNSET;

		// This is used to determine what to do when an ELSE or ELSE_IF is encountered.
		// If check_passed is FALSE when it gets to an ELSE or ELSE_IF the ELSE is ignored
		// or the ELSE_IF is processed. Otherwise it skips to the stored offset.
		bool check_passed_stack[CHECK_STACK_SIZE];
		int check_stack_pointer = UNSET;

		int flag_position;
		int flag_value;

		int script_number;
		int x_offset;
		int y_offset;
		int process_offset;

		int parameter_count;
		int parameter_number;

		int max_entries;
		int name_length;
		int max_score_digits;
		int table_type;

//		float new_x_vel;
//		float new_y_vel;

		int player_number;
		int control_number;
		int control_repeat_first_delay;
		int control_repeat_repeat_delay;

		int failure_line;

		int spawn_identity; // This is a number given to every entity in a spawn_entities run, so they can see if they're selected by their parents, or whatever.
		int graphic_number; // This is for proportionally spaced fonts.

		int line_length;

		int first_value;
		int second_value;
		int third_value;
		int fourth_value;
		int fifth_value;
		int sixth_value;
		int seventh_value;
		int eighth_value;
		int ninth_value;

		int path_number;
		int path_angle;
		int path_percent;
		int path_section;
		int path_speed;
		
		int centering_constant;
		int start_x,start_y;
		int offset_x,offset_y;
		int start_letter;
		int letter_counter;
		bool replace_space_with_zeros;
		bool name_first;
		int unique_id;
		int entry_number;
		int text_tag;
		int first_entity;
		int text_array;
		int letter_gap;
		bool include_spaces;
		bool overwrite_trailing_entities;

		bool boolean_result;

		bool trace_lines = false;
		bool trace_lines_next_frame = false;

		bool exit_after_start = false;

		bool check_previous_for_match = false;

		int switch_value[SWITCH_STACK_SIZE];
		int switch_stack_pointer = -1;
		int temp_value;

		int other_entity_id;
		int next_entity_id;

		int operation; // For math and comparative operations.
		int second_operation; // ditto
		int second_operation_value;
		int third_operation; // ditto
		int third_operation_value;
		bool exceeded; // Used in maths operations which loop
		int result_i; // For math and comparative operations.

		bool finished = false;
		int interpret_loop_guard = 0;

		int status = 0;

		#ifdef RETRENGINE_DEBUG_VERSION_SCRIPT_TRACER
			if (complete_trace_from_now_on)
			{
				trace_lines = true;
				trace_lines_next_frame = true;
				SCRIPTING_add_tracer_script_header (entity_id);
			}
		#endif

		if (over_ride_line == UNSET) // If we aren't hi-jacking the program line pointer due to a collision...
		{
			if (entity_pointer[ENT_WAIT_COUNTER] == 1)
			{
				// If we're just coming out of a wait, then we need to hijack the program line as well.
				entity_pointer[ENT_WAIT_COUNTER] = 0;
				line_number = entity_pointer[ENT_WAIT_RESTART];
			}
			else
			{
				line_number = entity_pointer[ENT_PROGRAM_START];
			}
		}
		else // Or maybe we are. Who knows? Certainly not me...
		{
			line_number = over_ride_line;
		}

			while (finished == false)
			{
				interpret_loop_guard++;
				if (interpret_loop_guard > SCRIPTING_get_interpret_guard_limit())
				{
					char guard_line[MAX_LINE_SIZE];
					int script_number = entity_pointer[ENT_SCRIPT_NUMBER];
					const char *script_name = "UNSET";

					if ((script_number >= 0) && (script_number < script_count))
					{
						script_name = GPL_get_entry_name("SCRIPTS", script_number);
					}

					snprintf(
						guard_line,
						sizeof(guard_line),
						"SCRIPT_INTERPRET_GUARD frame=%d entity=%d script=%d(%s) line=%d alive=%d wait=%d over=%d guard=%d",
						frames_so_far,
						entity_id,
						script_number,
						script_name,
						line_number,
						entity_pointer[ENT_ALIVE],
						entity_pointer[ENT_WAIT_COUNTER],
						over_ride_line,
						interpret_loop_guard);
					MAIN_add_to_log(guard_line);
					fprintf(stderr, "%s\n", guard_line);

					/* Force the offender out of the process list to avoid frame hard-lock. */
					SCRIPTING_limbo_entity(entity_id);
					finished = true;
					break;
				}

				instruction = scr[line_number].script_line_pointer[scr[line_number].command_instruction_index].data_value;
				scripting_kill_source_entity = entity_id;
				scripting_kill_source_line = line_number;

				#ifdef RETRENGINE_DEBUG_VERSION_WHERES_WALLY
					snprintf (wheres_wally, sizeof(wheres_wally), "Line %i Instruction %i", line_number, instruction);
				#endif

			#ifdef RETRENGINE_DEBUG_VERSION_SCRIPT_TRACER
				if (trace_lines)
				{
					old_line_number = line_number;
					SCRIPTING_set_tracer_script_before_values (entity_id,line_number);
				}
			#endif

			switch (instruction)
			{
			case COM_WATCH_ENTITY_VARIABLE:
				watched_entity = SCRIPTING_get_int_value ( entity_id , line_number , 1);
				watched_variable = scr[line_number].script_line_pointer[2].data_value; // Woah! Dodgy!
				break;



			case COM_RESET_WATCH_LIST:
				SCRIPTING_reset_watch_list();
				break;



			case COM_START:
				entity_pointer[ENT_PROGRAM_START] = line_number;
				if (exit_after_start)
				{
					finished = true;
				}
				break;



			case COM_END:
				finished = true;
				break;



			case COM_SET_WINDOW_COUNT:
				first_value = SCRIPTING_get_int_value ( entity_id , line_number , 1);
				SCRIPTING_create_windows (first_value);
				break;



			case COM_SET_UP_WINDOW:
				first_value = SCRIPTING_get_int_value ( entity_id , line_number , 1);
				second_value = SCRIPTING_get_int_value ( entity_id , line_number , 2);
				third_value = SCRIPTING_get_int_value ( entity_id , line_number , 3);
				fourth_value = SCRIPTING_get_int_value ( entity_id , line_number , 4);
				fifth_value = SCRIPTING_get_int_value ( entity_id , line_number , 5);
				sixth_value = SCRIPTING_get_int_value ( entity_id , line_number , 6);
				seventh_value = SCRIPTING_get_int_value ( entity_id , line_number , 7);
				eighth_value = SCRIPTING_get_int_value ( entity_id , line_number , 8);

				SCRIPTING_set_up_window (first_value,second_value,third_value,fourth_value,fifth_value,sixth_value,seventh_value,eighth_value);
				break;



			case COM_ENABLE_WINDOW:
				first_value = SCRIPTING_get_int_value ( entity_id , line_number , 1);
				SCRIPTING_activate_window (first_value);
				break;



			case COM_DISABLE_WINDOW:
				first_value = SCRIPTING_get_int_value ( entity_id , line_number , 1);
				SCRIPTING_deactivate_window (first_value);
				break;



			case COM_SET_ANIM_FRAME:
				first_value = SCRIPTING_get_int_value ( entity_id , line_number , 1);
				second_value = SCRIPTING_get_int_value ( entity_id , line_number , 2);
				third_value = SCRIPTING_get_int_value ( entity_id , line_number , 3);
				fourth_value = SCRIPTING_get_int_value ( entity_id , line_number , 4);
				sixth_value = SCRIPTING_get_int_value ( entity_id , line_number , 5);
				boolean_result = SCRIPTING_get_bool_value ( entity_id , line_number , 6);
				

				if (scr[line_number].script_line_size == 8)
				{
					seventh_value = SCRIPTING_get_int_value ( entity_id , line_number , 7);
					eighth_value = 0;
				}
				else if (scr[line_number].script_line_size == 9)
				{
					seventh_value = SCRIPTING_get_int_value ( entity_id , line_number , 7);
					eighth_value = SCRIPTING_get_int_value ( entity_id , line_number , 8);
				}
				else
				{
					seventh_value = 0;
					eighth_value = 0;
				}

				SCRIPTING_select_animation_frame (entity_pointer,first_value,second_value,third_value,fourth_value,sixth_value,boolean_result,seventh_value,eighth_value);
				break;



			case COM_GET_SPRITE_FOR_TILEMAP:
				first_value = SCRIPTING_get_int_value ( entity_id , line_number , 4);
				SCRIPTING_put_value ( entity_id , line_number , 1 , TILEMAPS_get_tilemap_sprite(first_value) );
				break;
				

			case COM_GET_GRAPHIC_FRAME_WIDTH:
				first_value = SCRIPTING_get_int_value ( entity_id , line_number , 4);
				second_value = SCRIPTING_get_int_value ( entity_id , line_number , 5);
				SCRIPTING_put_value ( entity_id , line_number , 1 , OUTPUT_get_sprite_width(first_value,second_value) );
				break;

			case COM_GET_GRAPHIC_FRAME_HEIGHT:
				first_value = SCRIPTING_get_int_value ( entity_id , line_number , 4);
				second_value = SCRIPTING_get_int_value ( entity_id , line_number , 5);
				SCRIPTING_put_value ( entity_id , line_number , 1 , OUTPUT_get_sprite_height(first_value,second_value) );
				break;

			case COM_CALL_SCRIPT:
				return_pointer_stack_pointer++;
				if (return_pointer_stack_pointer == RETURN_POINTER_STACK_SIZE)
				{
					return_pointer_stack_pointer--;
					OUTPUT_message("GOSUB STACK OVERFLOW!");
					assert(0);
				}
				else
				{
					return_pointer_stack[return_pointer_stack_pointer] = line_number + 1;
					script_number = SCRIPTING_get_int_value ( entity_id , line_number , 1);
					line_number = scr_lookup[script_number] - 1;
				}
				break;



			case COM_GOSUB:
				return_pointer_stack_pointer++;
				if (return_pointer_stack_pointer == RETURN_POINTER_STACK_SIZE)
				{
					return_pointer_stack_pointer--;
					OUTPUT_message("GOSUB STACK OVERFLOW!");
					assert(0);
				}
				else
				{
					return_pointer_stack[return_pointer_stack_pointer] = line_number + 1;
					line_number = SCRIPTING_get_int_value ( entity_id , line_number , 1);
				}
				break;



			case COM_GOTO:
				line_number = SCRIPTING_get_int_value ( entity_id , line_number , 1);
				return_pointer_stack_pointer = UNSET;
				for_stack_pointer = UNSET;
				check_stack_pointer = UNSET;
				break;

/*				
			case COM_GOSUB_THEN_END:
				return_pointer_stack_pointer++;
				return_pointer_stack[return_pointer_stack_pointer] = line_number + 1;
				line_number = SCRIPTING_get_int_value ( entity_id , line_number , 1);
				exit_after_start = true;
				break;
*/


			case COM_RETURN:
				line_number = return_pointer_stack[return_pointer_stack_pointer]-1;
				return_pointer_stack_pointer--;
				break;


				
			case COM_START_LOOP:
				if (scr[line_number].script_line_size != 2)
				{
					first_value = SCRIPTING_get_int_value ( entity_id , line_number , 1);
					second_value = SCRIPTING_get_int_value ( entity_id , line_number , 3);
					operation = SCRIPTING_get_int_value ( entity_id , line_number , 2);

					switch (operation)
					{

					case CMP_LESS_THAN:
						result_i = (first_value < second_value);
						break;

					case CMP_LESS_THAN_OR_EQUAL:
						result_i = (first_value <= second_value);
						break;

					case CMP_EQUAL:
						result_i = (first_value == second_value);
						break;

					case CMP_GREATER_THAN_OR_EQUAL:
						result_i = (first_value >= second_value);
						break;

					case CMP_GREATER_THAN:
						result_i = (first_value > second_value);
						break;

					case CMP_UNEQUAL:
						result_i = (first_value != second_value);
						break;

					case CMP_BITWISE_AND:
						result_i = (first_value & second_value);
						break;

					case CMP_BITWISE_OR:
						result_i = (first_value | second_value);
						break;

					case CMP_NOT_BITWISE_AND:
						result_i = !(first_value & second_value);
						break;

					case CMP_NOT_BITWISE_OR:
						result_i = !(first_value | second_value);
						break;
						
					default:
						break;

					}

					if (result_i == 0) // If the condition wasn't fulfilled...
					{
						// Then we just break out of the loop by going to the end line.
						line_number = SCRIPTING_get_int_value ( entity_id , line_number , 4);
					}
					else // If we passed, then we just continue with the loop.
					{
						// Do nuthin'
					}
				}
				else
				{
					// If there are no trailing arguments we just ignore it.
				}
				break;



			case COM_TOGGLE_DEBUG:
				draw_debug_overlay = !draw_debug_overlay;
				break;



			case COM_RESET_ENTITIES:
				script_number = SCRIPTING_get_int_value ( entity_id , line_number , 1);
				SCRIPTING_reset_entities_and_spawn (entity_id,script_number);
				finished = true;
				break;



			case COM_END_LOOP:
				if (scr[line_number].script_line_size != 2)
				{
					first_value = SCRIPTING_get_int_value ( entity_id , line_number , 1);
					second_value = SCRIPTING_get_int_value ( entity_id , line_number , 3);
					operation = SCRIPTING_get_int_value ( entity_id , line_number , 2);

					switch (operation)
					{

					case CMP_LESS_THAN:
						result_i = (first_value < second_value);
						break;

					case CMP_LESS_THAN_OR_EQUAL:
						result_i = (first_value <= second_value);
						break;

					case CMP_EQUAL:
						result_i = (first_value == second_value);
						break;

					case CMP_GREATER_THAN_OR_EQUAL:
						result_i = (first_value >= second_value);
						break;

					case CMP_GREATER_THAN:
						result_i = (first_value > second_value);
						break;

					case CMP_UNEQUAL:
						result_i = (first_value != second_value);
						break;

					case CMP_BITWISE_AND:
						result_i = (first_value & second_value);
						break;

					case CMP_BITWISE_OR:
						result_i = (first_value | second_value);
						break;

					case CMP_NOT_BITWISE_AND:
						result_i = !(first_value & second_value);
						break;

					case CMP_NOT_BITWISE_OR:
						result_i = !(first_value | second_value);
						break;
						
					default:
						break;

					}

					if (result_i == 0) // If the condition wasn't fulfilled...
					{
						// Then we just break out of the loop and continue as usual...
						// ie, do nuthin'
					}
					else // If we passed, then we go back for another round! Woooooo!
					{
						line_number = SCRIPTING_get_int_value ( entity_id , line_number , 4);
						// ie, the line after the START_LOOP
					}
				}
				else
				{
					// If there are no trailing arguments it's just bounce back time.
					line_number = SCRIPTING_get_int_value ( entity_id , line_number , 1)-1;
				}

				break;


				
			case COM_IF_DEBUG_MODE:
				failure_line = SCRIPTING_get_int_value ( entity_id , line_number , 2);
				if (!release_mode) // If the condition was fulfilled...
				{
					// Well, actually kinda' do nothing. Just continue as usual. Um... Look! A Bee!
					check_stack_pointer++;
					check_passed_stack[check_stack_pointer] = true;
				}
				else // If it failed then go to the failure line (which'll be either an ELSE or ENDIF) plus one.
				{
					line_number = failure_line-1;
					check_stack_pointer++;
					check_passed_stack[check_stack_pointer] = false;
				}
				break;


				
			case COM_IF_HARDWARE_GRAPHICS:
				failure_line = SCRIPTING_get_int_value ( entity_id , line_number , 2);
				if (!software_mode_active) // If the condition was fulfilled...
				{
					// Well, actually kinda' do nothing. Just continue as usual. Um... Look! A Bee!
					check_stack_pointer++;
					check_passed_stack[check_stack_pointer] = true;
				}
				else // If it failed then go to the failure line (which'll be either an ELSE or ENDIF) plus one.
				{
					line_number = failure_line-1;
					check_stack_pointer++;
					check_passed_stack[check_stack_pointer] = false;
				}
				break;


				
			case COM_IF_SOFTWARE_GRAPHICS:
				failure_line = SCRIPTING_get_int_value ( entity_id , line_number , 2);
				if (software_mode_active) // If the condition was fulfilled...
				{
					// Well, actually kinda' do nothing. Just continue as usual. Um... Look! A Bee!
					check_stack_pointer++;
					check_passed_stack[check_stack_pointer] = true;
				}
				else // If it failed then go to the failure line (which'll be either an ELSE or ENDIF) plus one.
				{
					line_number = failure_line-1;
					check_stack_pointer++;
					check_passed_stack[check_stack_pointer] = false;
				}
				break;



			case COM_IF_INSIDE_COLLISION:
				failure_line = SCRIPTING_get_int_value ( entity_id , line_number , 2);
				if (WORLDCOLL_inside_collision (entity_pointer) == true) // If the condition was fulfilled...
				{
					// Well, actually kinda' do nothing. Just continue as usual. Um... Look! A Bee!
					check_stack_pointer++;
					check_passed_stack[check_stack_pointer] = true;
				}
				else // If it failed then go to the failure line (which'll be either an ELSE or ENDIF) plus one.
				{
					line_number = failure_line-1;
					check_stack_pointer++;
					check_passed_stack[check_stack_pointer] = false;
				}
				break;



			case COM_IF_KEY_UNDEFINED:
				player_number = SCRIPTING_get_int_value ( entity_id , line_number , 1);
				control_number = SCRIPTING_get_int_value ( entity_id , line_number , 2);
				
				failure_line = SCRIPTING_get_int_value ( entity_id , line_number , 4);

				if (CONTROL_is_player_button_undefined (player_number,control_number)) // If the condition was fulfilled...
				{
					// Well, actually kinda' do nothing. Just continue as usual. Um... Look! A Bee!
					check_stack_pointer++;
					check_passed_stack[check_stack_pointer] = true;
				}
				else // If it failed then go to the failure line (which'll be either an ELSE or ENDIF) plus one.
				{
					line_number = failure_line-1;
					check_stack_pointer++;
					check_passed_stack[check_stack_pointer] = false;
				}
				break;

			case COM_ELSE_IF:
				if (check_passed_stack[check_stack_pointer] == true)
				{
					failure_line = SCRIPTING_get_int_value ( entity_id , line_number , 5);
					line_number = failure_line-1;
					break;
				}
				// pops off a value as the IF part of the next line will stick one back on it.
				check_stack_pointer--;
			case COM_IF_FLAG:
			case COM_IF:
				first_value = SCRIPTING_get_int_value ( entity_id , line_number , 1);
				second_value = SCRIPTING_get_int_value ( entity_id , line_number , 3);
				operation = SCRIPTING_get_int_value ( entity_id , line_number , 2);
				failure_line = SCRIPTING_get_int_value ( entity_id , line_number , 5);

				if (instruction == COM_IF_FLAG)
				{
					first_value = flag_array[first_value];
				}

				switch (operation)
				{

				case CMP_LESS_THAN:
					result_i = (first_value < second_value);
					break;

				case CMP_LESS_THAN_OR_EQUAL:
					result_i = (first_value <= second_value);
					break;

				case CMP_EQUAL:
					result_i = (first_value == second_value);
					break;

				case CMP_GREATER_THAN_OR_EQUAL:
					result_i = (first_value >= second_value);
					break;

				case CMP_GREATER_THAN:
					result_i = (first_value > second_value);
					break;

				case CMP_UNEQUAL:
					result_i = (first_value != second_value);
					break;

				case CMP_BITWISE_AND:
					result_i = (first_value & second_value);
					break;

				case CMP_BITWISE_OR:
					result_i = (first_value | second_value);
					break;

				case CMP_NOT_BITWISE_AND:
					result_i = !(first_value & second_value);
					break;

				case CMP_NOT_BITWISE_OR:
					result_i = !(first_value | second_value);
					break;
					
				default:
					break;

				}

				if (result_i != 0) // If the condition was fulfilled...
				{
					// Well, actually kinda' do nothing. Just continue as usual. Um... Look! A Bee!
					check_stack_pointer++;
					check_passed_stack[check_stack_pointer] = true;
				}
				else // If it failed then go to the failure line (which'll be either an ELSE or ENDIF) plus one.
				{
					line_number = failure_line-1;
					check_stack_pointer++;
					check_passed_stack[check_stack_pointer] = false;
				}
				break;



			case COM_IF_BLEND_MODE_AVAILABLE:
				failure_line = SCRIPTING_get_int_value ( entity_id , line_number , 2);
				if (texture_combiner_available) // If the condition was fulfilled...
				{
					// Well, actually kinda' do nothing. Just continue as usual. Um... Look! A Bee!
					check_stack_pointer++;
					check_passed_stack[check_stack_pointer] = true;
				}
				else // If it failed then go to the failure line (which'll be either an ELSE or ENDIF) plus one.
				{
					line_number = failure_line-1;
					check_stack_pointer++;
					check_passed_stack[check_stack_pointer] = false;
				}
				break;

				

			case COM_ELSE:
				if (check_passed_stack[check_stack_pointer] == true)
				{
					// If you find an ELSE then you must have stumbled into it while in the commands follow an IF so skip to the END_IF as you ain't meant to process the next bit...
					failure_line = SCRIPTING_get_int_value ( entity_id , line_number , 1);
					line_number = failure_line-1;
				}
				break;
				


			case COM_END_IF:
				check_stack_pointer--;
				break;



			case COM_FIXED_SWITCH:
				switch_stack_pointer++;
				// What are we looking for then?
				if (entity_pointer[ENT_FIXED_SWITCH_LINE] == UNSET)
				{
					switch_value[switch_stack_pointer] = SCRIPTING_get_int_value ( entity_id , line_number , 1);
				}
				else
				{
					line_number = entity_pointer[ENT_FIXED_SWITCH_LINE];
				}
				break;



			case COM_SWITCH:
				switch_stack_pointer++;
				// What are we looking for then?
				switch_value[switch_stack_pointer] = SCRIPTING_get_int_value ( entity_id , line_number , 1);
				break;


				
			case COM_CASE:
				// Get the value from after the word CASE
				temp_value = SCRIPTING_get_int_value ( entity_id , line_number , 1);
				// Check if it's equal to what's on the switch stack.
				if (temp_value != switch_value[switch_stack_pointer])
				{
					// Oh, it ain't. Well just go to the next CASE or DEFAULT statement.
					line_number = SCRIPTING_get_int_value ( entity_id , line_number , 2);
				}
				// Otherwise just continue execution of the script.
				break;



			case COM_FIXED_CASE:
				// Get the value from after the word CASE
				temp_value = SCRIPTING_get_int_value ( entity_id , line_number , 1);
				// Check if it's equal to what's on the switch stack.
				if (temp_value != switch_value[switch_stack_pointer])
				{
					// Oh, it ain't. Well just go to the next CASE or DEFAULT statement.
					line_number = SCRIPTING_get_int_value ( entity_id , line_number , 2);
				}
				else
				{
					entity_pointer[ENT_FIXED_SWITCH_LINE] = line_number;
				}
				// Otherwise just continue execution of the script.
				break;



			case COM_DEFAULT:
				// Does nothing really, you could just as well leave it out but it aids readability.
				break;



			case COM_BREAK:
				// If we hit a break then we must have gotten our CASE already so go to the end of the current SWITCH.
				line_number = SCRIPTING_get_int_value ( entity_id , line_number , 1);
				break;



			case COM_END_SWITCH:
				// And pop the switch value off the stack (in effect).
				switch_stack_pointer--;
				break;



			case COM_KILL_FAMILY_LINKAGE:
				entity_pointer[ENT_PARENT] = UNSET;
				entity_pointer[ENT_FIRST_CHILD] = UNSET;
				entity_pointer[ENT_LAST_CHILD] = UNSET;
				entity_pointer[ENT_PREV_SIBLING] = UNSET;
				entity_pointer[ENT_NEXT_SIBLING] = UNSET;
				entity_pointer[ENT_MATRIARCH] = UNSET;
				break;


				
			case COM_WRITE_VARIABLE_DATA_TO_RECORDED_INPUT:
				first_value = SCRIPTING_get_int_value ( entity_id , line_number , 1); // Player number
				second_value = SCRIPTING_get_int_value ( entity_id , line_number , 2); // ID Tag
				third_value = SCRIPTING_get_int_value ( entity_id , line_number , 3); // Value

				CONTROL_write_variable_data_to_recorded_input (first_value,second_value,third_value);
				break;



			case COM_READ_VARIABLE_DATA_FROM_RECORDED_INPUT:
				first_value = SCRIPTING_get_int_value ( entity_id , line_number , 4); // Player number
				second_value = SCRIPTING_get_int_value ( entity_id , line_number , 5); // ID Tag

				result_i = CONTROL_read_variable_data_from_recorded_input (first_value, second_value);

				SCRIPTING_put_value ( entity_id , line_number , 1 , result_i );
				break;



			case COM_SET_RANDOM_SEED:
				first_value = SCRIPTING_get_int_value ( entity_id , line_number , 1);
				MATH_srand (first_value);
				break;

				

			case COM_SET_SPECIAL_RANDOM_SEED:
				first_value = SCRIPTING_get_int_value ( entity_id , line_number , 1);
				second_value = SCRIPTING_get_int_value ( entity_id , line_number , 2);

				MATH_seed_special_rand (first_value, second_value);
				break;



			case COM_SET_STARFIELD_COLOUR:
				first_value = SCRIPTING_get_int_value ( entity_id , line_number , 1);
				second_value = SCRIPTING_get_int_value ( entity_id , line_number , 2);
				third_value = SCRIPTING_get_int_value ( entity_id , line_number , 3);
				fourth_value = SCRIPTING_get_int_value ( entity_id , line_number , 4);
				fifth_value = SCRIPTING_get_int_value ( entity_id , line_number , 5);
				PARTICLES_setup_starfield_colours (first_value,second_value,third_value,fourth_value,fifth_value);
				break;



			case COM_GENERATE_STAR_COLOURS:
				first_value = SCRIPTING_get_int_value ( entity_id , line_number , 1);
				PARTICLES_generate_star_colours (first_value);
				break;



			case COM_RAND_CHOICE:
				// Find how many arguments we have after the RAND_CHOICE token...
				line_length = scr[line_number].script_line_size - 4;

				// Get a random number up to and including that value...
				first_value = MATH_rand (0,line_length);

				// Find the value of that argument...
				result_i = SCRIPTING_get_int_value ( entity_id , line_number , first_value+4);

				// And pop it back to the variable. :)
				SCRIPTING_put_value ( entity_id , line_number , 1 , result_i );
				break;


				
			case COM_RESET_RAND_LOG:
				#ifdef DEBUG_RAND_LOGGING
				MAIN_reset_rand_log();
				#endif
				break;

			case COM_SAVE_RAND_LOG:
				#ifdef DEBUG_RAND_LOGGING
				MAIN_save_rand_log(SCRIPTING_get_bool_value(entity_id,line_number,1));
				MAIN_reset_rand_log();
				#endif
				break;



			case COM_SPECIAL_RAND_CHOICE:
				// Find how many arguments we have after the SPECIAL_RAND_CHOICE token...
				line_length = scr[line_number].script_line_size - 5;

				second_value = SCRIPTING_get_int_value ( entity_id , line_number , 4);

				// Get a random number up to and including that value...
				first_value = MATH_special_rand (second_value,0,line_length);
				
				#ifdef DEBUG_RAND_LOGGING
				MAIN_log_rand (first_value,line_number,entity[entity_id][ENT_SCRIPT_NUMBER],frames_so_far);
				#endif

				// Find the value of that argument...
				result_i = SCRIPTING_get_int_value ( entity_id , line_number , first_value+5);

				// And pop it back to the variable. :)
				SCRIPTING_put_value ( entity_id , line_number , 1 , result_i );
				break;

			case COM_RESET_FRAMES_SO_FAR_COUNTER:
				MAIN_reset_frame_counter();
				break;

			case COM_GET_PATH_POSITION_TOTAL_OFFSET:
				path_number = SCRIPTING_get_int_value ( entity_id , line_number , 6);
				path_angle = SCRIPTING_get_int_value ( entity_id , line_number , 7);
				path_percent = SCRIPTING_get_int_value ( entity_id , line_number , 8);
				path_section = SCRIPTING_get_int_value ( entity_id , line_number , 9);

				path_section = PATHS_get_path_position_offset_from_percentage (path_number, path_angle, path_percent, &first_value, &second_value, path_section);
				SCRIPTING_put_value ( entity_id , line_number , 1 , first_value );
				SCRIPTING_put_value ( entity_id , line_number , 2 , second_value );
				SCRIPTING_put_value ( entity_id , line_number , 3 , path_section );
			break;



			case COM_GET_PATH_FLAG:
				path_number = SCRIPTING_get_int_value ( entity_id , line_number , 4);
				path_percent = SCRIPTING_get_int_value ( entity_id , line_number , 5);
				path_speed = SCRIPTING_get_int_value ( entity_id , line_number , 6);
				path_section = SCRIPTING_get_int_value ( entity_id , line_number , 7);

				result_i = PATHS_hit_flag (path_number, path_percent, path_percent+path_speed, path_section);

				SCRIPTING_put_value ( entity_id , line_number , 1 , result_i );
			break;


			
			case COM_GET_PATH_POINT_ANGLE:
				path_number = SCRIPTING_get_int_value ( entity_id , line_number , 4);
				path_angle = SCRIPTING_get_int_value ( entity_id , line_number , 5);
				path_percent = SCRIPTING_get_int_value ( entity_id , line_number , 6);
				path_section = SCRIPTING_get_int_value ( entity_id , line_number , 7);

				path_angle = PATHS_point_angle (path_number, path_angle, path_percent, path_section);

				SCRIPTING_put_value ( entity_id , line_number , 1 , path_angle );
			break;

			

			case COM_GET_PATH_SECTION_ANGLE:
				path_number = SCRIPTING_get_int_value ( entity_id , line_number , 4);
				path_angle = SCRIPTING_get_int_value ( entity_id , line_number , 5);
				path_percent = SCRIPTING_get_int_value ( entity_id , line_number , 6);
				path_section = SCRIPTING_get_int_value ( entity_id , line_number , 7);

				path_angle = PATHS_point_angle (path_number, path_angle, path_percent, path_section);

				SCRIPTING_put_value ( entity_id , line_number , 1 , path_angle );
			break;



			case COM_GET_PATH_SECTION_START_PERCENTAGE:
				path_number = SCRIPTING_get_int_value ( entity_id , line_number , 4);
				path_percent = SCRIPTING_get_int_value ( entity_id , line_number , 5);
				path_section = SCRIPTING_get_int_value ( entity_id , line_number , 6);

				path_percent = PATHS_section_start_percentage (path_number, path_percent, path_section);

				SCRIPTING_put_value ( entity_id , line_number , 1 , path_percent );
			break;

			

			case COM_GET_SPECIAL_PATH_POSITION_TOTAL_OFFSET:
				path_number = SCRIPTING_get_int_value ( entity_id , line_number , 6);
				path_angle = SCRIPTING_get_int_value ( entity_id , line_number , 7);
				path_percent = SCRIPTING_get_int_value ( entity_id , line_number , 8);
				path_section = SCRIPTING_get_int_value ( entity_id , line_number , 9);

				path_section = SPECPATH_get_path_position_offset_from_percentage (path_number, path_angle, path_percent, &first_value, &second_value, path_section );
				SCRIPTING_put_value ( entity_id , line_number , 1 , first_value );
				SCRIPTING_put_value ( entity_id , line_number , 2 , second_value );
				SCRIPTING_put_value ( entity_id , line_number , 3 , path_section );
			break;



			case COM_GET_SPECIAL_PATH_FLAG:
				path_number = SCRIPTING_get_int_value ( entity_id , line_number , 4);
				path_percent = SCRIPTING_get_int_value ( entity_id , line_number , 5);
				path_speed = SCRIPTING_get_int_value ( entity_id , line_number , 6);
				path_section = SCRIPTING_get_int_value ( entity_id , line_number , 7);

				result_i = SPECPATH_hit_flag (path_number, path_percent, path_percent+path_speed, path_section);

				SCRIPTING_put_value ( entity_id , line_number , 1 , result_i );
			break;


/*			
			case COM_GET_SPECIAL_PATH_POINT_ANGLE:
				path_number = SCRIPTING_get_int_value ( entity_id , line_number , 4);
				path_angle = SCRIPTING_get_int_value ( entity_id , line_number , 5);
				path_percent = SCRIPTING_get_int_value ( entity_id , line_number , 6);
				path_section = SCRIPTING_get_int_value ( entity_id , line_number , 7);

				path_angle = PATHS_point_angle (path_number, path_angle, path_percent, path_section);

				SCRIPTING_put_value ( entity_id , line_number , 1 , path_angle );
			break;
*/
	

			case COM_GET_SPECIAL_PATH_SECTION_START_PERCENTAGE:
				path_number = SCRIPTING_get_int_value ( entity_id , line_number , 4);
				path_percent = SCRIPTING_get_int_value ( entity_id , line_number , 5);
				path_section = SCRIPTING_get_int_value ( entity_id , line_number , 6);

				path_percent = SPECPATH_section_start_percentage (path_number, path_percent, path_section);

				SCRIPTING_put_value ( entity_id , line_number , 1 , path_percent );
			break;

			case COM_RESET_TILEMAP_CONVERSION_TABLE:
				first_value = SCRIPTING_get_int_value ( entity_id , line_number , 1);
				TILESETS_reset_conversion_table (first_value);
			break;

			case COM_ALTER_TILEMAP_CONVERSION_TABLE:
				first_value = SCRIPTING_get_int_value ( entity_id , line_number , 1);
				second_value = SCRIPTING_get_int_value ( entity_id , line_number , 2);
				third_value = SCRIPTING_get_int_value ( entity_id , line_number , 3);
				fourth_value = SCRIPTING_get_int_value ( entity_id , line_number , 4);
				TILESETS_add_to_conversion_table (first_value, second_value, third_value, fourth_value);
			break;

			case COM_USE_TILEMAP_CONVERSION_TABLE:
				if (scr[line_number].script_line_size == 3)
				{
					first_value = SCRIPTING_get_int_value ( entity_id , line_number , 1);
					TILEMAPS_use_conversion_table (first_value);
				}
				else
				{
					first_value = SCRIPTING_get_int_value ( entity_id , line_number , 1);
					second_value = SCRIPTING_get_int_value ( entity_id , line_number , 2);
					third_value = SCRIPTING_get_int_value ( entity_id , line_number , 3);
					TILEMAPS_use_conversion_table (first_value, second_value, third_value);
				}
			break;


			case COM_COUNT_MATCHING_ENTITIES:
				first_value = scr[line_number].script_line_pointer[4].data_value; // This is the variable we're comparing with...
				operation = SCRIPTING_get_int_value ( entity_id , line_number , 5);
				second_value = SCRIPTING_get_int_value ( entity_id , line_number , 6);
				third_value = SCRIPTING_get_int_value ( entity_id , line_number , 7);

				result_i = SCRIPTING_count_matching_entities (first_value,operation,second_value,third_value);

				SCRIPTING_put_value ( entity_id , line_number , 1 , result_i );
			break;

			case COM_GET_ARRAY_LINE_INDEX:
				first_value = SCRIPTING_get_int_value ( entity_id , line_number , 4);
				second_value = SCRIPTING_get_int_value ( entity_id , line_number , 5);
				result_i = ARRAY_get_datatable_line_from_label (first_value,second_value);
				SCRIPTING_put_value ( entity_id , line_number , 1 , result_i );
			break;

			case COM_GET_ARRAY_LINE_COUNT_AFTER_INDEX:
				first_value = SCRIPTING_get_int_value ( entity_id , line_number , 4);
				second_value = SCRIPTING_get_int_value ( entity_id , line_number , 5);
				result_i = ARRAY_get_datatable_line_count_from_label_to_next (first_value,second_value);
				SCRIPTING_put_value ( entity_id , line_number , 1 , result_i );
			break;



			case COM_LET:
				line_length = scr[line_number].script_line_size;

				if (scr[line_number].script_line_pointer[line_length-4].data_type == MATH_COMPARATIVE)
				{
					// Okay, we have two comparative operations at the end of our thing.
					second_operation = SCRIPTING_get_int_value ( entity_id , line_number , line_length-4);
					second_operation_value = SCRIPTING_get_int_value ( entity_id , line_number , line_length-3);
					third_operation = SCRIPTING_get_int_value ( entity_id , line_number , line_length-2);
					third_operation_value = SCRIPTING_get_int_value ( entity_id , line_number , line_length-1);
					line_length -= 4;
				}
				else if (scr[line_number].script_line_pointer[line_length-2].data_type == MATH_COMPARATIVE)
				{
					// Just one comparative check!
					second_operation = SCRIPTING_get_int_value ( entity_id , line_number , line_length-2);
					second_operation_value = SCRIPTING_get_int_value ( entity_id , line_number , line_length-1);
					third_operation = UNSET;
					line_length -= 2;
				}
				else
				{
					// No comparative checks! Woot!
					second_operation = UNSET;
					third_operation = UNSET;
				}

				if (line_length == 4) // It's the first kind of math operation (LET VAR = VAR).
				{
					result_i = SCRIPTING_get_int_value ( entity_id , line_number , 3);
				}
				else if (line_length == 5) // It's the second kind of math operation (LET VAR = OPP VAR).
				{
					first_value = SCRIPTING_get_int_value ( entity_id , line_number , 4);
					operation = SCRIPTING_get_int_value ( entity_id , line_number , 3);

					switch (operation)
					{

					case MATH_COS:
						result_i = MATH_get_cos_table_value (first_value);
						break;

					case MATH_SIN:
						result_i = MATH_get_sin_table_value (first_value);
						break;

					case MATH_ACOS:
						result_i = MATH_get_acos_table_value (first_value);
						break;

					case MATH_ASIN:
						result_i = MATH_get_asin_table_value (first_value);
						break;

					case MATH_SGN:
						result_i = MATH_sgn_int (first_value);
						break;

					case MATH_ABS:
						result_i = abs (first_value);
						break;

					case MATH_RAND:
						result_i = MATH_rand (0, first_value+1);
						break;

					case MATH_RAND_SIGN:
						result_i = ((MATH_rand (0,2)*2)-1) * first_value;
						break;

					case MATH_SQR:
						result_i = int (sqrt (first_value));
						break;

					case MATH_WRAP_ANGLE:
						result_i = MATH_wrap_angle(first_value);
						break;

					case MATH_ANGLE_TO_ENTITY:
						result_i = MATH_atan2 ( entity_pointer[ENT_WORLD_X] - entity[first_value][ENT_WORLD_X] , entity_pointer[ENT_WORLD_Y] - entity[first_value][ENT_WORLD_Y] ) - 9000;
						break;

					case MATH_SQUARED_DISTANCE_TO_ENTITY:
						second_value = entity_pointer[ENT_WORLD_X] - entity[first_value][ENT_WORLD_X];
						third_value = entity_pointer[ENT_WORLD_Y] - entity[first_value][ENT_WORLD_Y];
						result_i = (second_value*second_value) + (third_value*third_value);
						break;

					case MATH_DISTANCE_TO_ENTITY:
						second_value = entity_pointer[ENT_WORLD_X] - entity[first_value][ENT_WORLD_X];
						third_value = entity_pointer[ENT_WORLD_Y] - entity[first_value][ENT_WORLD_Y];
						result_i = int (sqrt ((second_value*second_value) + (third_value*third_value)) );
						break;

					default:
						break;
					}
				}
				else if (line_length == 6) // It's one of the third kind of math operation (LET VAR = VAR OPP VAR or LET VAR = OPP VAR VAR).
				{
					if (scr[line_number].script_line_pointer[4].data_type == MATH_OPERATION) // It's the (LET VAR = VAR OPP VAR) variant.
					{
						first_value = SCRIPTING_get_int_value ( entity_id , line_number , 3);
						second_value = SCRIPTING_get_int_value ( entity_id , line_number , 5);
						operation = SCRIPTING_get_int_value ( entity_id , line_number , 4);

						switch (operation)
						{

						case MATH_LEFT_SHIFT:
							result_i = first_value << second_value;
							break;

						case MATH_RIGHT_SHIFT:
							result_i = first_value >> second_value;
							break;

						case MATH_PLUS:
							result_i = first_value + second_value;
							break;

						case MATH_MINUS:
							result_i = first_value - second_value;
							break;

						case MATH_MULTIPLY:
							result_i = first_value * second_value;
							break;

						case MATH_DIVIDE:
							result_i = first_value / second_value;
							break;

						case MATH_COS:
							result_i = (first_value * MATH_get_cos_table_value (second_value)) / 10000;
							break;

						case MATH_SIN:
							result_i = (first_value * MATH_get_sin_table_value (second_value)) / 10000;
							break;

						case MATH_SGN:
							result_i = first_value * MATH_sgn_int (second_value);
							break;

						case MATH_MOD:
							result_i = first_value % second_value;
							break;

						case MATH_LAND:
							result_i = ((first_value && second_value) != 0);
							break;

						case MATH_LOR:
							result_i = ((first_value || second_value) != 0);
							break;

						case MATH_XOR:
							result_i = (first_value ^ second_value);
							break;

						case MATH_AND:
							result_i = (first_value & second_value);
							break;

						case MATH_OR:
							result_i = (first_value | second_value);
							break;

						case MATH_POW:
							result_i = int (pow (first_value,second_value));
							break;

						case MATH_PERCENTAGE:
							result_i = (second_value * first_value) / 10000;
							break;

						case MATH_CURVE_PERCENTAGE:
						{
							// Okay, so the first value is our current "percentage" from 0-10000 (or so)
							// and we want to bias it.

							bool second_half_flag;
							
							if (first_value > 5000)
							{
								second_half_flag = true;
							}
							else
							{
								second_half_flag = false;
							}

							switch (second_value)
							{
								case CURVE_PERCENTAGE_FIFO:
									result_i = ((first_value * 18)/10); // Range now 0>18000
									result_i = MATH_get_sin_table_value (result_i); // Range now 0>10000>0
									if (second_half_flag)
									{
										result_i = 20000 - result_i; // Range now 0>20000
									}
									result_i /= 2; // Range now 0>10000
								break;
								
								case CURVE_PERCENTAGE_SISO:
									result_i = ((first_value * 18)/10); // Range now 0>18000
									result_i -= 9000; // Range now -9000>9000
									result_i = MATH_get_sin_table_value (result_i); // Range now -10000>10000
									result_i += 10000; // Range now 0>20000
									result_i /= 2; // Range now 0>10000
								break;

								case CURVE_PERCENTAGE_FISO:
									result_i = ((first_value * 9)/10); // Range now 0>9000
									result_i = MATH_get_sin_table_value (result_i); // Range now 0>10000
								break;

								case CURVE_PERCENTAGE_SIFO:
									result_i = ((first_value * 9)/10); // Range now 0>9000
									result_i -= 9000; // Range now -9000>0
									result_i = MATH_get_sin_table_value (result_i); // Range now -10000>0
									result_i += 10000; // Range now 0>10000
								break;

								default:
									assert(0);
								break;
							}
						}
						break;

						default:
							assert(0);
							break;

						}
					}
					else // It's the (LET VAR = OPP VAR VAR) variant.
					{
						operation = SCRIPTING_get_int_value ( entity_id , line_number , 3);
						first_value = SCRIPTING_get_int_value ( entity_id , line_number , 4);
						second_value = SCRIPTING_get_int_value ( entity_id , line_number , 5);

						switch (operation)
						{

						case MATH_ATAN2:
							result_i = MATH_atan2 (first_value , second_value);
							break;

						case MATH_RAND:
							result_i = MATH_rand (first_value , second_value+1);
							break;

						case MATH_SPECIAL_RAND:
							result_i = MATH_special_rand (first_value , 0, second_value+1);
							#ifdef DEBUG_RAND_LOGGING
							MAIN_log_rand (result_i,line_number,entity[entity_id][ENT_SCRIPT_NUMBER],frames_so_far);
							#endif
							break;

						case MATH_SPECIAL_RAND_SIGN:
							result_i = ((MATH_special_rand (first_value,0,2)*2)-1) * second_value;
							#ifdef DEBUG_RAND_LOGGING
							MAIN_log_rand (result_i,line_number,entity[entity_id][ENT_SCRIPT_NUMBER],frames_so_far);
							#endif
							break;

						case MATH_PYTH:
							result_i = int (sqrt ( (first_value * first_value) + (second_value * second_value) ) );
							break;

						case MATH_DIFF_ANGLE:
							result_i = MATH_diff_angle(first_value,second_value);
							break;

						default:
							assert(0);
							break;
						}
					}
				}
				else if (line_length == 7)
				{
					if (scr[line_number].script_line_pointer[3].data_type == MATH_OPERATION) // It's one of the fourth kind of math operation (LET VAR = OPP VAR VAR VAR).
					{
						operation = SCRIPTING_get_int_value ( entity_id , line_number , 3);
						first_value = SCRIPTING_get_int_value ( entity_id , line_number , 4);
						second_value = SCRIPTING_get_int_value ( entity_id , line_number , 5);
						third_value = SCRIPTING_get_int_value ( entity_id , line_number , 6);

						switch (operation)
						{

						case MATH_ROTATE:
							result_i = MATH_rotate_angle (first_value,second_value,third_value); // DO THIS FUNCTION!
							break;

						case MATH_LERP:
							result_i = MATH_lerp_int (first_value,second_value,third_value);
							break;

						case MATH_UNLERP:
							result_i = MATH_unlerp_int (first_value,second_value,third_value);
							break;

						case MATH_SPECIAL_RAND:
							result_i = MATH_special_rand (first_value,second_value,third_value+1);
							#ifdef DEBUG_RAND_LOGGING
							MAIN_log_rand (result_i,line_number,entity[entity_id][ENT_SCRIPT_NUMBER],frames_so_far);
							#endif
							break;

						default:
							assert(0);
							break;

						}
					}
					else // It's one of the fifth kind of math operation (LET VAR = VAR OPP VAR VAR).
					{
						operation = SCRIPTING_get_int_value ( entity_id , line_number , 4);
						first_value = SCRIPTING_get_int_value ( entity_id , line_number , 3);
						second_value = SCRIPTING_get_int_value ( entity_id , line_number , 5);
						third_value = SCRIPTING_get_int_value ( entity_id , line_number , 6);

						switch (operation)
						{
						case MATH_ADAPT_BY_PERCENTAGE:
							result_i = first_value + int (float (second_value - first_value) * (float (third_value) / 10000.0f));

							if ((result_i == first_value) && (first_value != second_value))
							{
								if (result_i < second_value)
								{
									result_i++;
								}
								else
								{
									result_i--;
								}
							}
							break;

						default:
							assert(0);
							break;

						}
					}
				}

				if (third_operation != UNSET)
				{
					exceeded = false;

					switch (second_operation)
					{
					case MATHCOM_NOT_GREATER_THAN:
						if (result_i > second_operation_value)
						{
							result_i = second_operation_value;
							exceeded = true;
						}
						break;

					case MATHCOM_NOT_LESS_THAN:
						if (result_i < second_operation_value)
						{
							result_i = result_i + ((third_operation_value - second_operation_value) + 1);
							exceeded = true;
						}
						break;

					case MATHCOM_UNEQUAL:
						if (result_i == second_operation_value)
						{
							exceeded = true;
						}
						break;

					default:
						assert(0);
						break;
					}

					switch (third_operation)
					{
					case MATHCOM_NOT_GREATER_THAN:
						if (result_i > third_operation_value)
						{
							result_i = result_i - ((third_operation_value - second_operation_value) + 1);
						}
						break;

					case MATHCOM_NOT_LESS_THAN:
						if (result_i < third_operation_value)
						{
							result_i = third_operation_value;
						}
						break;

					case MATHCOM_QUESTION_MARK:
						if (exceeded)
						{
							result_i = third_operation_value;
						}
						break;

					default:
						assert(0);
						break;
					}

				}
				else if (second_operation != UNSET)
				{

					switch (second_operation)
					{
					case MATHCOM_NOT_GREATER_THAN:
						if (result_i > second_operation_value)
						{
							result_i = second_operation_value;
						}
						break;

					case MATHCOM_NOT_LESS_THAN:
						if (result_i < second_operation_value)
						{
							result_i = second_operation_value;
						}
						break;

					default:
						assert(0);
						break;
					}
					
				}

				// Righty, so we've got our value, now we have to shove it somewhere...

				SCRIPTING_put_value ( entity_id , line_number , 1 , result_i );

				break;

/*
			case COM_LET:
				if (scr[line_number].script_line_size == 4) // It's the first kind of math operation (LET VAR = VAR).
				{
					result_i = SCRIPTING_get_int_value ( entity_id , line_number , 3);
				}
				else if (scr[line_number].script_line_size == 5) // It's the second kind of math operation (LET VAR = OPP VAR).
				{
					first_value = SCRIPTING_get_int_value ( entity_id , line_number , 4);
					operation = SCRIPTING_get_int_value ( entity_id , line_number , 3);

					switch (operation)
					{

					case MATH_COS:
						result_i = MATH_get_cos_table_value (first_value);
						break;

					case MATH_SIN:
						result_i = MATH_get_sin_table_value (first_value);
						break;

					case MATH_ACOS:
						result_i = MATH_get_acos_table_value (first_value);
						break;

					case MATH_ASIN:
						result_i = MATH_get_asin_table_value (first_value);
						break;

					case MATH_SGN:
						result_i = MATH_sgn_int (first_value);
						break;

					case MATH_ABS:
						result_i = abs (first_value);
						break;

					case MATH_RAND:
						result_i = MATH_rand (0, first_value+1);
						break;

					case MATH_RAND_SIGN:
						result_i = ((MATH_rand (0,2)*2)-1) * first_value;
						break;		

					case MATH_SQR:
						result_i = int (sqrt (first_value));
						break;

					case MATH_WRAP_ANGLE:
						result_i = MATH_wrap_angle(first_value);
						break;

					case MATH_ANGLE_TO_ENTITY:
						result_i = MATH_atan2 ( entity[first_value][ENT_WORLD_X] - entity_pointer[ENT_WORLD_X] , entity[first_value][ENT_WORLD_Y] - entity_pointer[ENT_WORLD_Y] );
						break;

					case MATH_SQUARED_DISTANCE_TO_ENTITY:
						second_value = entity_pointer[ENT_WORLD_X] - entity[first_value][ENT_WORLD_X];
						third_value = entity_pointer[ENT_WORLD_Y] - entity[first_value][ENT_WORLD_Y];
						result_i = (second_value*second_value) + (third_value*third_value);
						break;

					default:
						break;

					}
				
				}
				else if (scr[line_number].script_line_size == 6) // It's one of the third kind of math operation (LET VAR = VAR OPP VAR or LET VAR = OPP VAR VAR).
				{
					if (scr[line_number].script_line_pointer[4].data_type == MATH_OPERATION) // It's the (LET VAR = VAR OPP VAR) variant.
					{
						first_value = SCRIPTING_get_int_value ( entity_id , line_number , 3);
						second_value = SCRIPTING_get_int_value ( entity_id , line_number , 5);
						operation = SCRIPTING_get_int_value ( entity_id , line_number , 4);

						switch (operation)
						{

						case MATH_LEFT_SHIFT:
							result_i = first_value << second_value;
							break;

						case MATH_RIGHT_SHIFT:
							result_i = first_value >> second_value;
							break;

						case MATH_PLUS:
							result_i = first_value + second_value;
							break;

						case MATH_MINUS:
							result_i = first_value - second_value;
							break;

						case MATH_MULTIPLY:
							result_i = first_value * second_value;
							break;

						case MATH_DIVIDE:
							result_i = first_value / second_value;
							break;

						case MATH_COS:
							result_i = (first_value * MATH_get_cos_table_value (second_value)) / 10000;
							break;

						case MATH_SIN:
							result_i = (first_value * MATH_get_sin_table_value (second_value)) / 10000;
							break;

						case MATH_SGN:
							result_i = first_value * MATH_sgn_int (second_value);
							break;

						case MATH_MOD:
							result_i = first_value % second_value;
							break;

						case MATH_LAND:
							result_i = ((first_value && second_value) != 0);
							break;

						case MATH_LOR:
							result_i = ((first_value || second_value) != 0);
							break;

						case MATH_XOR:
							result_i = (first_value ^ second_value);
							break;

						case MATH_AND:
							result_i = (first_value & second_value);
							break;

						case MATH_OR:
							result_i = (first_value | second_value);
							break;

						case MATH_POW:
							result_i = int (pow (first_value,second_value));
							break;

						case MATH_PERCENTAGE:
							result_i = (second_value * first_value) / 10000;
							break;

						default:
							assert(0);
							break;

						}
					}
					else // It's the (LET VAR = OPP VAR VAR) variant.
					{
						operation = SCRIPTING_get_int_value ( entity_id , line_number , 3);
						first_value = SCRIPTING_get_int_value ( entity_id , line_number , 4);
						second_value = SCRIPTING_get_int_value ( entity_id , line_number , 5);

						switch (operation)
						{

						case MATH_ATAN2:
							result_i = MATH_atan2 (first_value , second_value);
							break;

						case MATH_RAND:
							result_i = MATH_rand (first_value , second_value+1);
							break;

						case MATH_PYTH:
							result_i = int (sqrt ( (first_value * first_value) + (second_value * second_value) ) );
							break;

						case MATH_DIFF_ANGLE:
							result_i = MATH_diff_angle(first_value,second_value);
							break;

						default:
							assert(0);
							break;
						}
					}
				}
				else if (scr[line_number].script_line_size == 7)
				{
					if ( (scr[line_number].script_line_pointer[3].data_type == MATH_OPERATION) && (scr[line_number].script_line_pointer[5].data_type != MATH_OPERATION) ) // It's one of the fourth kind of math operation (LET VAR = OPP VAR VAR VAR).
					{
						operation = SCRIPTING_get_int_value ( entity_id , line_number , 3);
						first_value = SCRIPTING_get_int_value ( entity_id , line_number , 4);
						second_value = SCRIPTING_get_int_value ( entity_id , line_number , 5);
						third_value = SCRIPTING_get_int_value ( entity_id , line_number , 6);

						switch (operation)
						{

						case MATH_ROTATE:
							result_i = MATH_rotate_angle (first_value,second_value,third_value); // DO THIS FUNCTION!
							break;

						case MATH_LERP:
							result_i = MATH_lerp_int (first_value,second_value,third_value);
							break;

						case MATH_UNLERP:
							result_i = MATH_unlerp_int (first_value,second_value,third_value);
							break;

						default:
							assert(0);
							break;

						}
					}
					else if ( (scr[line_number].script_line_pointer[3].data_type == MATH_OPERATION) && (scr[line_number].script_line_pointer[5].data_type == MATH_OPERATION) ) // It's one of the fourth kind of math operation (LET VAR = OPP VAR OPP VAR).
					{

						first_value = SCRIPTING_get_int_value ( entity_id , line_number , 4);
						operation = SCRIPTING_get_int_value ( entity_id , line_number , 3);
						second_operation = SCRIPTING_get_int_value ( entity_id , line_number , 5);
						fourth_value = SCRIPTING_get_int_value ( entity_id , line_number , 6);

						switch (operation)
						{

						case MATH_COS:
							result_i = MATH_get_cos_table_value (first_value);
							break;

						case MATH_SIN:
							result_i = MATH_get_sin_table_value (first_value);
							break;

						case MATH_ACOS:
							result_i = MATH_get_acos_table_value (first_value);
							break;

						case MATH_ASIN:
							result_i = MATH_get_asin_table_value (first_value);
							break;

						case MATH_SGN:
							result_i = MATH_sgn_int (first_value);
							break;

						case MATH_ABS:
							result_i = abs (first_value);
							break;

						case MATH_RAND:
							result_i = MATH_rand (0, first_value+1);
							break;

						case MATH_RAND_SIGN:
							result_i = ((MATH_rand (0,2)*2)-1) * first_value;
							break;		

						case MATH_SQR:
							result_i = int (sqrt (first_value));
							break;

						case MATH_WRAP_ANGLE:
							result_i = MATH_wrap_angle(first_value);
							break;

						case MATH_ANGLE_TO_ENTITY:
							result_i = MATH_atan2 ( entity[first_value][ENT_WORLD_X] - entity_pointer[ENT_WORLD_X] , entity[first_value][ENT_WORLD_Y] - entity_pointer[ENT_WORLD_Y] );
							break;

						case MATH_SQUARED_DISTANCE_TO_ENTITY:
							second_value = entity_pointer[ENT_WORLD_X] - entity[first_value][ENT_WORLD_X];
							third_value = entity_pointer[ENT_WORLD_Y] - entity[first_value][ENT_WORLD_Y];
							result_i = (second_value*second_value) + (third_value*third_value);
							break;

						default:
							break;

						}

						switch (second_operation)
						{
						case MATH_NOT_GREATER_THAN:
							if (result_i > fourth_value)
							{
								result_i = fourth_value;
							}
							break;

						case MATH_NOT_LESS_THAN:
							if (result_i < fourth_value)
							{
								result_i = fourth_value;
							}
							break;

						default:
							assert(0);
							break;
						}
						
					}
					else // It's one of the fifth kind of math operation (LET VAR = VAR OPP VAR VAR).
					{
						operation = SCRIPTING_get_int_value ( entity_id , line_number , 4);
						first_value = SCRIPTING_get_int_value ( entity_id , line_number , 3);
						second_value = SCRIPTING_get_int_value ( entity_id , line_number , 5);
						third_value = SCRIPTING_get_int_value ( entity_id , line_number , 6);

						switch (operation)
						{
						case MATH_ADAPT_BY_PERCENTAGE:
							result_i = first_value + int (float (second_value - first_value) * (float (third_value) / 10000.0f));
							break;

						default:
							assert(0);
							break;

						}
					}


				}
				else if (scr[line_number].script_line_size == 8)
				{
					if ( (scr[line_number].script_line_pointer[4].data_type == MATH_OPERATION) && (scr[line_number].script_line_pointer[6].data_type == MATH_OPERATION) ) // It's a (LET VAR = VAR OPP VAR OPP VAR).
					{
						operation = SCRIPTING_get_int_value ( entity_id , line_number , 4);
						second_operation = SCRIPTING_get_int_value ( entity_id , line_number , 6);

						first_value = SCRIPTING_get_int_value ( entity_id , line_number , 3);
						second_value = SCRIPTING_get_int_value ( entity_id , line_number , 5);
						third_value = SCRIPTING_get_int_value ( entity_id , line_number , 7);

						switch (operation)
						{
						case MATH_LEFT_SHIFT:
							result_i = first_value << second_value;
							break;

						case MATH_RIGHT_SHIFT:
							result_i = first_value >> second_value;
							break;

						case MATH_PLUS:
							result_i = first_value + second_value;
							break;

						case MATH_MINUS:
							result_i = first_value - second_value;
							break;

						case MATH_MULTIPLY:
							result_i = first_value * second_value;
							break;

						case MATH_DIVIDE:
							result_i = first_value / second_value;
							break;

						case MATH_COS:
							result_i = (first_value * MATH_get_cos_table_value (second_value)) / 10000;
							break;

						case MATH_SIN:
							result_i = (first_value * MATH_get_sin_table_value (second_value)) / 10000;
							break;
							
						case MATH_PERCENTAGE:
							result_i = (second_value * first_value) / 10000;
							break;

						default:
							break;
						}

						switch (second_operation)
						{
						case MATH_NOT_GREATER_THAN:
							if (result_i > third_value)
							{
								result_i = third_value;
							}
							break;

						case MATH_NOT_LESS_THAN:
							if (result_i < third_value)
							{
								result_i = third_value;
							}
							break;

						default:
							assert(0);
							break;
						}

					}
				}
				else if (scr[line_number].script_line_size == 10)
				{
					if ( (scr[line_number].script_line_pointer[4].data_type == MATH_OPERATION) && (scr[line_number].script_line_pointer[6].data_type == MATH_OPERATION) && (scr[line_number].script_line_pointer[8].data_type == MATH_OPERATION) ) // It's a (LET VAR = VAR OPP VAR OPP VAR OPP VAR).
					{
						operation = SCRIPTING_get_int_value ( entity_id , line_number , 4);
						second_operation = SCRIPTING_get_int_value ( entity_id , line_number , 6);
						third_operation = SCRIPTING_get_int_value ( entity_id , line_number , 8);

						first_value = SCRIPTING_get_int_value ( entity_id , line_number , 3);
						second_value = SCRIPTING_get_int_value ( entity_id , line_number , 5);
						third_value = SCRIPTING_get_int_value ( entity_id , line_number , 7);
						fourth_value = SCRIPTING_get_int_value ( entity_id , line_number , 9);

						switch (operation)
						{
						case MATH_LEFT_SHIFT:
							result_i = first_value << second_value;
							break;

						case MATH_RIGHT_SHIFT:
							result_i = first_value >> second_value;
							break;

						case MATH_PLUS:
							result_i = first_value + second_value;
							break;

						case MATH_MINUS:
							result_i = first_value - second_value;
							break;

						case MATH_MULTIPLY:
							result_i = first_value * second_value;
							break;

						case MATH_DIVIDE:
							result_i = first_value / second_value;
							break;

						case MATH_COS:
							result_i = (first_value * MATH_get_cos_table_value (second_value)) / 10000;
							break;

						case MATH_SIN:
							result_i = (first_value * MATH_get_sin_table_value (second_value)) / 10000;
							break;
							
						case MATH_PERCENTAGE:
							result_i = (second_value * first_value) / 10000;
							break;

						default:
							break;
						}

						exceeded = false;

						switch (second_operation)
						{
						case MATH_NOT_GREATER_THAN:
							if (result_i > third_value)
							{
								result_i = third_value;
								exceeded = true;
							}
							break;

						case MATH_NOT_LESS_THAN:
							if (result_i < third_value)
							{
								result_i = fourth_value + result_i;
								exceeded = true;
							}
							break;

						default:
							assert(0);
							break;
						}

						switch (third_operation)
						{
						case MATH_NOT_GREATER_THAN:
							if (result_i > fourth_value)
							{
								result_i = third_value + (result_i-fourth_value);
							}
							break;

						case MATH_NOT_LESS_THAN:
							if (result_i < fourth_value)
							{
								result_i = fourth_value;
							}
							break;

						case MATH_QUESTION_MARK:
							if (exceeded)
							{
								result_i = fourth_value;
							}
							break;

						default:
							assert(0);
							break;
						}

					}

				}

				// Righty, so we've got our value, now we have to shove it somewhere...

				SCRIPTING_put_value ( entity_id , line_number , 1 , result_i );
				break;
*/


			case COM_CHANGE_SCRIPT:
				script_number = SCRIPTING_get_int_value ( entity_id , line_number , 1);
				line_number = scr_lookup[script_number] - 1;
				// Because it'll get increased at the end of the switch.
				break;

			case COM_GET_HSL_FROM_OPENGL_COLOURS:
				MATH_convert_rgb_to_hsv (entity_pointer[ENT_OPENGL_VERTEX_RED], entity_pointer[ENT_OPENGL_VERTEX_GREEN], entity_pointer[ENT_OPENGL_VERTEX_BLUE], &first_value, &second_value, &third_value);

				SCRIPTING_put_value (entity_id,line_number,1,first_value);
				SCRIPTING_put_value (entity_id,line_number,2,second_value);
				SCRIPTING_put_value (entity_id,line_number,3,third_value);
				break;


			case COM_SPAWN_ENTITY:
				script_number = SCRIPTING_get_int_value ( entity_id , line_number , 1);

				if (scr[line_number].script_line_size == 2)
				{
					x_offset = 0;
					y_offset = 0;
					process_offset = PROCESS_NEXT;
				}
				else
				{
					x_offset = SCRIPTING_get_int_value ( entity_id , line_number , 2);
					y_offset = SCRIPTING_get_int_value ( entity_id , line_number , 3);
					process_offset = SCRIPTING_get_int_value ( entity_id , line_number , 4);
				}

				SCRIPTING_spawn_entity (entity_id , script_number , x_offset , y_offset , process_offset);
				break;

			case COM_SPAWN_ENTITY_WITH_PARAMETERS:
				script_number = SCRIPTING_get_int_value ( entity_id , line_number , 1);
				x_offset = SCRIPTING_get_int_value ( entity_id , line_number , 2);
				y_offset = SCRIPTING_get_int_value ( entity_id , line_number , 3);
				process_offset = SCRIPTING_get_int_value ( entity_id , line_number , 4);
				SCRIPTING_spawn_entity (entity_id , script_number , x_offset , y_offset , process_offset);

				if (scr[line_number].script_line_size > 5)
				{
					parameter_count = scr[line_number].script_line_size - 5;
					other_entity_id = entity_pointer[ENT_LAST_CHILD];

					for (parameter_number=0; parameter_number<parameter_count; parameter_number++)
					{
						entity[other_entity_id][ENT_PASSED_PARAM_1+parameter_number] = SCRIPTING_get_int_value ( entity_id , line_number , 4 + parameter_number);
					}
				}
				break;



			case COM_FOR:
				for_stack_pointer++;
				for_line_number[for_stack_pointer] = line_number + 1;
				
				if (scr[line_number].script_line_pointer[1].data_type == VARIABLE)
				{
					for_entity_id[for_stack_pointer] = entity_id;
					for_variable_number[for_stack_pointer] = scr[line_number].script_line_pointer[1].data_value;
				}
				else // it's another entity's variable...
				{
					for_entity_id[for_stack_pointer] =  entity [entity_id][ scr[line_number].script_line_pointer[1].data_type ];
					for_variable_number[for_stack_pointer] = scr[line_number].script_line_pointer[1].data_value;
				}

				entity[for_entity_id[for_stack_pointer]][for_variable_number[for_stack_pointer]] = SCRIPTING_get_int_value ( entity_id , line_number , 3);
				for_end_value[for_stack_pointer] = SCRIPTING_get_int_value ( entity_id , line_number , 5);

				if (entity[for_entity_id[for_stack_pointer]][for_variable_number[for_stack_pointer]] != for_end_value[for_stack_pointer])
				{
					if (scr[line_number].script_line_size == 6)
					{
						// Auto assign step value!
						if (entity[for_entity_id[for_stack_pointer]][for_variable_number[for_stack_pointer]] <= for_end_value[for_stack_pointer])
						{
							for_step_value[for_stack_pointer] = 1;
						}
						else if (entity[for_entity_id[for_stack_pointer]][for_variable_number[for_stack_pointer]] > for_end_value[for_stack_pointer])
						{
							for_step_value[for_stack_pointer] = -1;
						}
						else
						{
							assert(0);
						}
					}
					else
					{
						for_step_value[for_stack_pointer] = SCRIPTING_get_int_value ( entity_id , line_number , 7);
						assert (for_step_value[for_stack_pointer] != 0);
					}

					if (for_step_value[for_stack_pointer] > 0)
					{
						for_end_value[for_stack_pointer] -= 1;
					}
					else
					{
						for_end_value[for_stack_pointer] += 1;
					}
				}
				else
				{
					
					// Okay, there's a chance that the start value and the end value are already the
					// same, in which case we don't want to trawl through the intermediate lines. In
					// this situation, we simply look for the next line with a matching indentation to this one...

					int look_for_indentation = scr[line_number].indentation_level;

					do
					{
						line_number++;
					} while(scr[line_number].indentation_level != look_for_indentation);
				}

			break;



			case COM_NEXT:
				if (for_step_value[for_stack_pointer] > 0) // Looping upwards
				{
					if (entity[for_entity_id[for_stack_pointer]][for_variable_number[for_stack_pointer]] < for_end_value[for_stack_pointer])
					{
						line_number = for_line_number[for_stack_pointer] - 1;
						entity[for_entity_id[for_stack_pointer]][for_variable_number[for_stack_pointer]] += for_step_value[for_stack_pointer];
					}
					else
					{
						// Pop it off the stack.
						for_stack_pointer--;
					}
				}
				else if (for_step_value[for_stack_pointer] < 0) // Looping upwards
				{
					if (entity[for_entity_id[for_stack_pointer]][for_variable_number[for_stack_pointer]] > for_end_value[for_stack_pointer])
					{
						line_number = for_line_number[for_stack_pointer];
						entity[for_entity_id[for_stack_pointer]][for_variable_number[for_stack_pointer]] += for_step_value[for_stack_pointer];
					}
					else
					{
						// Pop it off the stack.
						for_stack_pointer--;
					}
				}
				break;



			case COM_WAIT:
				if (SCRIPTING_get_int_value ( entity_id , line_number , 1) > 0)
				{
					entity_pointer[ENT_WAIT_COUNTER] = SCRIPTING_get_int_value ( entity_id , line_number , 1);
					entity_pointer[ENT_WAIT_RESTART] = line_number + 1;
					finished = true;
				}
				break;



			case COM_SET_GLOBAL_FLAG:
				flag_position = SCRIPTING_get_int_value ( entity_id , line_number , 1);
				flag_array[flag_position] = SCRIPTING_get_int_value ( entity_id , line_number , 2);
				flag_type[flag_position] = FLAG_TYPE_NORMAL;
				break;



			case COM_SET_GLOBAL_FLAG_AS_ENTITY_ID:
				flag_position = SCRIPTING_get_int_value ( entity_id , line_number , 1);
				flag_array[flag_position] = entity_id;
				flag_type[flag_position] = FLAG_TYPE_ENTITY_ID;
				break;



			case COM_GET_GLOBAL_FLAG:
				flag_position = SCRIPTING_get_int_value ( entity_id , line_number , 4);
				flag_value = flag_array[flag_position];
				SCRIPTING_put_value (entity_id,line_number,1,flag_value);
				break;



			case COM_GET_KEY_PRESS:
				CONTROL_get_input (&first_value,&second_value);
				
				SCRIPTING_put_value (entity_id,line_number,1,first_value);
				SCRIPTING_put_value (entity_id,line_number,2,second_value);
				break;

			case COM_SET_STICK_SENSITIVITY:
				first_value = SCRIPTING_get_int_value ( entity_id , line_number , 1);
				CONTROL_set_stick_sensitivity (first_value);
				break;


			case COM_GET_ANY_PLAYER_JOYPAD_HIT:
				player_number = SCRIPTING_get_int_value ( entity_id , line_number , 4);
				
				if (CONTROL_check_any_joypad_control_hit (player_number))
				{
					SCRIPTING_put_value (entity_id,line_number,1,1);
				}
				else
				{
					SCRIPTING_put_value (entity_id,line_number,1,0);
				}
				break;



			case COM_DEFINE_PLAYER_CONTROL_FROM_KEYPRESS:
				first_value = SCRIPTING_get_int_value ( entity_id , line_number , 4);
				second_value = SCRIPTING_get_int_value ( entity_id , line_number , 5);
				if (SCRIPTING_get_int_value ( entity_id , line_number , 6) != 0)
				{
					check_previous_for_match = true;
				}
				else
				{
					check_previous_for_match = false;
				}
				
				if (CONTROL_get_keypress(first_value, second_value, NULL, 0, check_previous_for_match))
				{
					SCRIPTING_put_value (entity_id,line_number,1,1);
				}
				else
				{
					SCRIPTING_put_value (entity_id,line_number,1,0);
				}
				break;



			case COM_DEFINE_KEY:
				first_value = SCRIPTING_get_int_value ( entity_id , line_number , 1);
				second_value = SCRIPTING_get_int_value ( entity_id , line_number , 2);
				third_value = SCRIPTING_get_int_value ( entity_id , line_number , 3);

				CONTROL_define_control_keyboard(first_value,second_value,third_value);
				break;



			case COM_GET_KEYBOARD_HIT:
				result_i = CONTROL_get_first_keyboard_hit();

				SCRIPTING_put_value ( entity_id,line_number,1,result_i );
				break;



			case COM_GET_ASCII_FOR_SCANCODE:
				first_value = SCRIPTING_get_int_value ( entity_id , line_number , 4);

				result_i = CONTROL_get_ascii_for_scancode(first_value,true);

				SCRIPTING_put_value ( entity_id,line_number,1,result_i );
				break;



			case COM_GET_SCREEN_SIZE_MULTIPLE:
				result_i = MAIN_get_window_size_multiple();

				SCRIPTING_put_value ( entity_id,line_number,1,result_i );
				break;

			case COM_LET_VALUE_LOOP:
				if ( SCRIPTING_get_int_value ( entity_id , line_number , 7) > 0 )
				{
					result_i = SCRIPTING_get_int_value ( entity_id , line_number , 1) + SCRIPTING_get_int_value ( entity_id , line_number , 7);
					if (result_i > SCRIPTING_get_int_value ( entity_id , line_number , 5) )
					{
						SCRIPTING_put_value ( entity_id,line_number,1,SCRIPTING_get_int_value ( entity_id , line_number , 3) );
					}
					else
					{
						SCRIPTING_put_value ( entity_id,line_number,1,result_i );
					}
				}
				else
				{
					result_i = SCRIPTING_get_int_value ( entity_id , line_number , 1) + SCRIPTING_get_int_value ( entity_id , line_number , 7);
					if (result_i < SCRIPTING_get_int_value ( entity_id , line_number , 5) )
					{
						SCRIPTING_put_value ( entity_id,line_number,1,SCRIPTING_get_int_value ( entity_id , line_number , 3) );
					}
					else
					{
						SCRIPTING_put_value ( entity_id,line_number,1,result_i );
					}
				}
				break;






			case COM_DEADLINE:
				result_i = SCRIPTING_get_int_value ( entity_id , line_number , 1);
				entity_pointer[ENT_DEAD_LINE] = result_i;
				break;



//			case COM_ELSE_IF_INPUT_KEYBOARD_HIT:
//				if (check_passed_stack[check_stack_pointer] == true)
//				{
//					failure_line = SCRIPTING_get_int_value ( entity_id , line_number , 5);
//					line_number = failure_line-1;
//					break;
//				}
//				// pops off a value as the IF part of the next line will stick one back on it.
//				check_stack_pointer--;

			case COM_IF_INPUT_KEYBOARD_HIT:
				control_number = SCRIPTING_get_int_value ( entity_id , line_number , 1); // Scan code!
				failure_line = SCRIPTING_get_int_value ( entity_id , line_number , 3);

				if (CONTROL_key_hit(control_number) == false)
				{
					check_stack_pointer++;
					check_passed_stack[check_stack_pointer] = false;
					line_number = failure_line-1;	
				}
				else
				{
					check_stack_pointer++;
					check_passed_stack[check_stack_pointer] = true;
				}
				break;


//			case COM_ELSE_IF_INPUT_KEYBOARD_DOWN:
//				if (check_passed_stack[check_stack_pointer] == true)
//				{
//					failure_line = SCRIPTING_get_int_value ( entity_id , line_number , 5);
//					line_number = failure_line-1;
//					break;
//				}
//				// pops off a value as the IF part of the next line will stick one back on it.
//				check_stack_pointer--;

			case COM_IF_INPUT_KEYBOARD_DOWN:
				control_number = SCRIPTING_get_int_value ( entity_id , line_number , 1); // Scan code!
				failure_line = SCRIPTING_get_int_value ( entity_id , line_number , 3);

				if (CONTROL_key_down(control_number) == false)
				{
					check_stack_pointer++;
					check_passed_stack[check_stack_pointer] = false;
					line_number = failure_line-1;	
				}
				else
				{
					check_stack_pointer++;
					check_passed_stack[check_stack_pointer] = true;
				}
				break;



			case COM_IF_INPUT_PLAYER_CONTROL_HIT:
				player_number = SCRIPTING_get_int_value ( entity_id , line_number , 1);
				control_number = SCRIPTING_get_int_value ( entity_id , line_number , 2);
				failure_line = SCRIPTING_get_int_value ( entity_id , line_number , 4);

				if (CONTROL_player_button_hit(player_number,control_number) == false)
				{
					check_stack_pointer++;
					check_passed_stack[check_stack_pointer] = false;
					line_number = failure_line-1;	
				}
				else
				{
					check_stack_pointer++;
					check_passed_stack[check_stack_pointer] = true;
				}
				break;



			case COM_IF_INPUT_PLAYER_CONTROL_DOWN:
				player_number = SCRIPTING_get_int_value ( entity_id , line_number , 1);
				control_number = SCRIPTING_get_int_value ( entity_id , line_number , 2);
				failure_line = SCRIPTING_get_int_value ( entity_id , line_number , 4);

				if (CONTROL_player_button_down(player_number,control_number) == false)
				{
					check_stack_pointer++;
					check_passed_stack[check_stack_pointer] = false;
					line_number = failure_line-1;	
				}
				else
				{
					check_stack_pointer++;
					check_passed_stack[check_stack_pointer] = true;
				}
				break;


			case COM_IF_INPUT_PLAYER_CONTROL_REPEAT:
				player_number = SCRIPTING_get_int_value ( entity_id , line_number , 1);
				control_number = SCRIPTING_get_int_value ( entity_id , line_number , 2);
				control_repeat_first_delay = SCRIPTING_get_int_value ( entity_id , line_number , 3);
				control_repeat_repeat_delay = SCRIPTING_get_int_value ( entity_id , line_number , 4);
				failure_line = SCRIPTING_get_int_value ( entity_id , line_number , 6);

				if (CONTROL_player_button_repeat(player_number,control_number,control_repeat_first_delay,control_repeat_repeat_delay,true) == false)
				{
					check_stack_pointer++;
					check_passed_stack[check_stack_pointer] = false;
					line_number = failure_line-1;	
				}
				else
				{
					check_stack_pointer++;
					check_passed_stack[check_stack_pointer] = true;
				}
				break;



			case COM_IF_INPUT_PLAYER_CONTROL_HIT_OR_REPEAT:
				player_number = SCRIPTING_get_int_value ( entity_id , line_number , 1);
				control_number = SCRIPTING_get_int_value ( entity_id , line_number , 2);
				control_repeat_first_delay = SCRIPTING_get_int_value ( entity_id , line_number , 3);
				control_repeat_repeat_delay = SCRIPTING_get_int_value ( entity_id , line_number , 4);
				failure_line = SCRIPTING_get_int_value ( entity_id , line_number , 6);

				if (CONTROL_player_button_repeat(player_number,control_number,control_repeat_first_delay,control_repeat_repeat_delay,false) == false)
				{
					check_stack_pointer++;
					check_passed_stack[check_stack_pointer] = false;
					line_number = failure_line-1;	
				}
				else
				{
					check_stack_pointer++;
					check_passed_stack[check_stack_pointer] = true;
				}
				break;
				


			case COM_GET_CURRENT_FRAME:
				result_i = 1; // COMMAND GOES HERE!
				SCRIPTING_put_value ( entity_id,line_number,1,result_i );
				break;



			case COM_ADD_TILE_TO_LAYER:
				// COMMAND GOES HERE!
				break;



			case COM_MIN_MAX:
				result_i = SCRIPTING_get_int_value ( entity_id , line_number , 1);
				first_value = SCRIPTING_get_int_value ( entity_id , line_number , 4);
				second_value = SCRIPTING_get_int_value ( entity_id , line_number , 5);

				if (result_i < first_value)
				{
					result_i = first_value;
				}
				else if (result_i > second_value)
				{
					result_i = second_value;
				}

				SCRIPTING_put_value ( entity_id,line_number,1,result_i );
				break;



			case COM_GET_VERTEX_COLOURS_FROM_HSL:
				first_value = SCRIPTING_get_int_value ( entity_id , line_number , 1);
				second_value = SCRIPTING_get_int_value ( entity_id , line_number , 2);
				third_value = SCRIPTING_get_int_value ( entity_id , line_number , 3);

				MATH_convert_hsl_to_rgb (first_value, second_value, third_value, &entity_pointer[ENT_OPENGL_VERTEX_RED], &entity_pointer[ENT_OPENGL_VERTEX_GREEN], &entity_pointer[ENT_OPENGL_VERTEX_BLUE]);
				break;


			case COM_BACKUP_TILEMAP:
				first_value = SCRIPTING_get_int_value ( entity_id , line_number , 1);
				TILEMAPS_backup_tilemap (first_value);
				break;

			case COM_RESTORE_TILEMAP:
				first_value = SCRIPTING_get_int_value ( entity_id , line_number , 1);
				TILEMAPS_restore_tilemap (first_value);
				break;

			case COM_KILL_ENTITY:
				other_entity_id = SCRIPTING_get_int_value ( entity_id , line_number , 1); // Get the ID we're gonna' pop the clogs of.
				next_entity_id = entity_pointer[ENT_NEXT_PROCESS_ENT];

				#ifdef RETRENGINE_DEBUG_VERSION_COMPILE_INTERACTION_TABLE
					if (entity_id != other_entity_id)
					{
						SCRIPTING_add_to_interaction_table (entity_id,other_entity_id,DEBUG_INTERACTION_KILL);
					}
				#endif

				// OVERHAUL THIS!

				if (other_entity_id == entity_id)
				{
					// If we just killed ourselves... No script for you!
					finished = true;
				}
				else if (other_entity_id == next_entity_id)
				{
					global_next_entity = entity [entity_pointer[ENT_NEXT_PROCESS_ENT]][ENT_NEXT_PROCESS_ENT];
				}
				else if (other_entity_id == global_next_entity)
				{
					global_next_entity = entity [global_next_entity][ENT_NEXT_PROCESS_ENT];
				}

				SCRIPTING_limbo_entity (other_entity_id);
				break;

			case COM_SLEEP_ENTITY:
				other_entity_id = SCRIPTING_get_int_value ( entity_id , line_number , 1); // Get the ID we're gonna' pop the clogs of.

				if (other_entity_id == entity_id)
				{
					// If we just slept ourselves... No script for you!
					finished = true;
				}

				if (entity_pointer[ENT_ALIVE] != SLEEPING)
				{
					entity_pointer[ENT_OLD_ALIVE] = entity_pointer[ENT_ALIVE];
					entity_pointer[ENT_ALIVE] = SLEEPING;

					if (entity_pointer[ENT_WAKE_LINE] != UNSET)
					{
						entity_pointer[ENT_PROGRAM_START] = entity_pointer[ENT_WAKE_LINE];
					}
				}
				break;

			case COM_SLEEP_ENTITIES:
				// This sleeps a WHOLE lot more than just one entity...
				first_value = SCRIPTING_get_int_value ( entity_id , line_number , 1);

				if (scr[line_number].script_line_size == 3)
				{
					second_value = SCRIPTING_get_int_value ( entity_id , line_number , 2);
				}
				else
				{
					second_value = UNSET;
				}

				result_i = SCRIPTING_alter_entities_by_criteria (first_value,second_value,entity_id,ALTERATION_SLEEP);

				if (result_i != VERY_UNSET)
				{
					global_next_entity = result_i;
				}

				if (entity_pointer[ENT_ALIVE] == SLEEPING)
				{
					// If we slept ourselves... No script for you!
					finished = true;
				}
				break;

			case COM_WAKE_ENTITIES:
				// This wakes a WHOLE lot more than just one entity...
				first_value = SCRIPTING_get_int_value ( entity_id , line_number , 1);

				if (scr[line_number].script_line_size == 3)
				{
					second_value = SCRIPTING_get_int_value ( entity_id , line_number , 2);
				}
				else
				{
					second_value = UNSET;
				}

				result_i = SCRIPTING_alter_entities_by_criteria (first_value,second_value,entity_id,ALTERATION_WAKE);

				if (result_i != VERY_UNSET)
				{
					global_next_entity = result_i;
				}

				break;

			case COM_ALTER_ENTITIES:
				first_value = SCRIPTING_get_int_value ( entity_id , line_number , 1);
					// Alteration type...

				second_value = scr[line_number].script_line_pointer[2].data_value;
					// Which variable to look at...

				third_value = SCRIPTING_get_int_value ( entity_id , line_number , 3);
					// Comparison...

				fourth_value = SCRIPTING_get_int_value ( entity_id , line_number , 4);
					// Compared value...

				result_i = SCRIPTING_alter_entities_by_criteria_new (entity_id,first_value,second_value,third_value,fourth_value);

				if (result_i != VERY_UNSET)
				{
					global_next_entity = result_i;
				}

				if (entity_pointer[ENT_ALIVE] <= 0)
				{
					// If we just killed ourselves... No script for you!
					finished = true;
				}
				break;



			case COM_KILL_ENTITIES:
				// This kills a WHOLE lot more than just one entity...
				first_value = SCRIPTING_get_int_value ( entity_id , line_number , 1);

				if (scr[line_number].script_line_size == 3)
				{
					second_value = SCRIPTING_get_int_value ( entity_id , line_number , 2);
				}
				else
				{
					second_value = UNSET;
				}

				result_i = SCRIPTING_alter_entities_by_criteria (first_value,second_value,entity_id,ALTERATION_KILL);

				if (result_i != VERY_UNSET)
				{
					global_next_entity = result_i;
				}

				if (entity_pointer[ENT_ALIVE] <= 0)
				{
					// If we just killed ourselves... No script for you!
					finished = true;
				}
				break;

			case COM_GET_DIRECTION:
				first_value = SCRIPTING_get_int_value ( entity_id , line_number , 1);
				other_entity_id = SCRIPTING_get_int_value ( entity_id , line_number , 2);
				SCRIPTING_put_value ( entity_id , line_number , 1 , MATH_get_rough_direction ( entity_pointer[ENT_X] , entity_pointer[ENT_Y] , entity[other_entity_id][ENT_X] , entity[other_entity_id][ENT_Y] ) );
				break;

			case COM_MAKE_TARGET_LIST:
				break;

			case COM_ENTITY_HITLINE:
				result_i = SCRIPTING_get_int_value ( entity_id , line_number , 2);
				entity_pointer[ENT_ENTITY_HIT_LINE] = result_i+1;
				break;

			case COM_WORLD_HITLINE:
				result_i = SCRIPTING_get_int_value ( entity_id , line_number , 2);
				entity_pointer[ENT_WORLD_HIT_LINE] = result_i+1;
				break;

			case COM_WAKE_LINE:
				result_i = SCRIPTING_get_int_value ( entity_id , line_number , 1);
				entity_pointer[ENT_WAKE_LINE] = result_i+1;
				break;

			case COM_REMOVE_FROM_COLLISION_LIST:
				OBCOLL_remove_from_collision_bucket (entity_id);
				break;

			case COM_SETUP_COLLISION_SYSTEM_SIZE:
				third_value = SCRIPTING_get_int_value ( entity_id , line_number , 1);
				first_value = SCRIPTING_get_int_value ( entity_id , line_number , 2);
				second_value = SCRIPTING_get_int_value ( entity_id , line_number , 3);
				OBCOLL_set_game_world_size (first_value,second_value,third_value);
				break;

			case COM_SETUP_COLLISION_SYSTEM_SIZE_BY_COLLISION_MAP:
				third_value = SCRIPTING_get_int_value ( entity_id , line_number , 1);
				WORLDCOLL_get_collision_map_size_in_pixels (&first_value,&second_value);
				OBCOLL_set_game_world_size (first_value,second_value,third_value);
				break;

			case COM_SETUP_ACTIVE_COLLISION_WINDOW_BY_ZONE:
				first_value = SCRIPTING_get_int_value ( entity_id , line_number , 1);
				WORLDCOLL_get_horizontal_zone_extents (first_value, &second_value, &third_value);
				WORLDCOLL_get_vertical_zone_extents (first_value, &fourth_value, &fifth_value);
				OBCOLL_position_collision_window (second_value,fourth_value,third_value-second_value,fifth_value-fourth_value);
				break;

			case COM_SETUP_ACTIVE_COLLISION_WINDOW:
				first_value = SCRIPTING_get_int_value ( entity_id , line_number , 1);
				second_value = SCRIPTING_get_int_value ( entity_id , line_number , 2);
				third_value = SCRIPTING_get_int_value ( entity_id , line_number , 3);
				fourth_value = SCRIPTING_get_int_value ( entity_id , line_number , 4);
				OBCOLL_position_collision_window (first_value,second_value,third_value,fourth_value);
				break;

			case COM_DISABLE_COLLISION_SYSTEM:
				OBCOLL_free_collision_buffers ();
				break;

			case COM_SETUP_TILEMAP_COLLISION:
				WORLDCOLL_generate_collision_maps (true,false);
				break;

			case COM_SETUP_TILE_PROFILES:
				first_value = SCRIPTING_get_int_value ( entity_id , line_number , 1);
				WORLDCOLL_setup_block_collision (first_value);
				break;

			case COM_SET_CURRENT_COLLISION_TILEMAP:
				first_value = SCRIPTING_get_int_value ( entity_id , line_number , 1);
				WORLDCOLL_set_collision_map (first_value);
				break;

			case COM_DECREASE:
				first_value = SCRIPTING_get_int_value ( entity_id , line_number , 1);
				second_value = SCRIPTING_get_int_value ( entity_id , line_number , 2);

				if (first_value < 0)
				{
					if (first_value <= -second_value)
					{
						first_value += second_value;
					}
					else
					{
						first_value = 0;
					}
				}
				else if (first_value > 0)
				{
					if (first_value >= second_value)
					{
						first_value -= second_value;
					}
					else
					{
						first_value = 0;
					}
				}
				SCRIPTING_put_value( entity_id , line_number , 1 , first_value );
				break;

			case COM_TEST_SNAP_TO_DIRECTION:
				// The direction we test/snap in...
				second_value = SCRIPTING_get_int_value ( entity_id , line_number , 4);
				// The maximum snapping distance...
				third_value = SCRIPTING_get_int_value ( entity_id , line_number , 5);
				// Do we want to actually snap?
				fourth_value = SCRIPTING_get_int_value ( entity_id , line_number , 6);

				SCRIPTING_put_value ( entity_id , line_number , 1 , WORLDCOLL_test_snap_to_direction (entity_pointer , second_value , third_value, 1 , entity_pointer[ENT_WORLD_COLLISION_BEHAVIOUR] && COLLISION_ITERATE_MOVEMENT , fourth_value ) );
				break;



			case COM_GET_DISTANCE_TO_TILE:
				// The direction we test/snap in...
				second_value = SCRIPTING_get_int_value ( entity_id , line_number , 4);
				// The maximum snapping distance...
				third_value = SCRIPTING_get_int_value ( entity_id , line_number , 5);

				WORLDCOLL_test_snap_to_direction (entity_pointer , second_value , third_value, 1 , entity_pointer[ENT_WORLD_COLLISION_BEHAVIOUR] && COLLISION_ITERATE_MOVEMENT , 0 , &result_i );

				SCRIPTING_put_value ( entity_id , line_number , 1 , result_i );
				break;

			case COM_REINITIALISE_STARFIELD:
				first_value = SCRIPTING_get_int_value ( entity_id , line_number , 1); // Unique ID
				PARTICLES_initialise_starfield (first_value);
				break;

			case COM_CREATE_STARFIELD:
				first_value = SCRIPTING_get_int_value ( entity_id , line_number , 1); // Desired Unique ID
				second_value = SCRIPTING_get_int_value ( entity_id , line_number , 2); // Number of stars
				third_value = SCRIPTING_get_int_value ( entity_id , line_number , 3); // Style
				fourth_value = SCRIPTING_get_int_value ( entity_id , line_number , 4); // Width
				fifth_value = SCRIPTING_get_int_value ( entity_id , line_number , 5); // Height
				sixth_value = SCRIPTING_get_int_value ( entity_id , line_number , 6); // Lowest speed
				seventh_value = SCRIPTING_get_int_value ( entity_id , line_number , 7); // Highest speed

				if (scr[line_number].script_line_size == 10)
				{
					eighth_value = SCRIPTING_get_int_value ( entity_id , line_number , 8); // X Accelleration
					ninth_value = SCRIPTING_get_int_value ( entity_id , line_number , 9); // Y Accelleration
				}
				else
				{
					eighth_value = 10000;
					ninth_value = 10000;
				}

				PARTICLES_create_starfield (first_value,second_value,third_value,fourth_value,fifth_value,sixth_value,seventh_value,eighth_value,ninth_value);
				PARTICLES_initialise_starfield (first_value);
				break;

			case COM_DESTROY_STARFIELD:
				first_value = SCRIPTING_get_int_value ( entity_id , line_number , 1); // Desired Unique ID

				PARTICLES_destroy_starfield (first_value);
				break;

			case COM_DESTROY_ALL_STARFIELDS:
				PARTICLES_destroy_all_starfields ();
				break;

			case COM_OVERWRITE_TILEMAP_COLLISION:
				first_value = SCRIPTING_get_int_value ( entity_id , line_number , 1); // Tilemap index
				second_value = SCRIPTING_get_int_value ( entity_id , line_number , 2); // Copy from layer
				third_value = SCRIPTING_get_int_value ( entity_id , line_number , 3); // Copy to layer
				fourth_value = SCRIPTING_get_int_value ( entity_id , line_number , 4); // Overwrite constant

				WORLDCOLL_copy_collision_layer (first_value,second_value,third_value,fourth_value);
				break;

			case COM_UPDATE_STARFIELD:
				first_value = SCRIPTING_get_int_value ( entity_id , line_number , 1); // Starfield Unique ID
				second_value = SCRIPTING_get_int_value ( entity_id , line_number , 2); // X update percentage
				third_value = SCRIPTING_get_int_value ( entity_id , line_number , 3); // Y update percentage
				fourth_value = SCRIPTING_get_int_value ( entity_id , line_number , 4); // Recycle

				PARTICLES_update_starfield (first_value , second_value , third_value ,fourth_value);
				break;

			case COM_ADD_OFFSET_TILE_NUMBER:
				// Only truly solid
				first_value = SCRIPTING_get_int_value ( entity_id , line_number , 4);
				// Reser counter
				second_value = SCRIPTING_get_int_value ( entity_id , line_number , 5);
				// x offset
				third_value = SCRIPTING_get_int_value ( entity_id , line_number , 6);
				// y offset
				fourth_value = SCRIPTING_get_int_value ( entity_id , line_number , 7);

				result_i = WORLDCOLL_get_tile_at_offset (entity_pointer, third_value , fourth_value, first_value, second_value);

				SCRIPTING_put_value ( entity_id , line_number , 1 , result_i );
				break;

			case COM_APPLY_COMPILED_ATTRIBUTES:
				// attribute
				first_value = SCRIPTING_get_int_value ( entity_id , line_number , 1);
				// only use boolean fitting bitmask
				second_value = SCRIPTING_get_int_value ( entity_id , line_number , 2);

				WORLDCOLL_apply_attributes (entity_pointer,first_value,second_value);
				break;

			case COM_GET_AGGREGATE_BOOLEANS:
				result_i = WORLDCOLL_get_aggregate_booleans ();

				SCRIPTING_put_value ( entity_id , line_number , 1 , result_i );
				break;

			case COM_COMPILE_SOLID_TILE_NUMBERS:
				// Check type
				first_value = SCRIPTING_get_int_value ( entity_id , line_number , 5);
				// Check direction
				second_value = SCRIPTING_get_int_value ( entity_id , line_number , 6);
				// Reset counter
				third_value = SCRIPTING_get_int_value ( entity_id , line_number , 7);
				// Distance
				fourth_value = SCRIPTING_get_int_value ( entity_id , line_number , 8);

				switch(first_value)
				{
				case CHECK_EDGE:
					result_i = WORLDCOLL_get_solid_inner_tile_in_direction (entity_pointer, second_value , fourth_value, third_value);
					break;

				case CHECK_CORNERS:
					result_i = WORLDCOLL_get_solid_corner_tile_in_direction (entity_pointer, second_value , fourth_value, third_value);
					break;

				case CHECK_POINT:
					// Other axis offset
					fifth_value = SCRIPTING_get_int_value ( entity_id , line_number , 9);
					result_i = WORLDCOLL_get_solid_single_tile_in_direction (entity_pointer, second_value , fourth_value, third_value, fifth_value);
					break;

				default:
					OUTPUT_message("INVALID CHECKING MODE!");
					assert(0);
					break;
				}

				SCRIPTING_put_value ( entity_id , line_number , 1 , result_i );
				SCRIPTING_put_value ( entity_id , line_number , 2 , collision_blocks_ignore_count );
				break;



			case COM_POSITION_AT_END_OF_ROTATED_ENTITY:
				first_value = SCRIPTING_get_int_value ( entity_id , line_number , 1);
				second_value = SCRIPTING_get_int_value ( entity_id , line_number , 2);
				third_value = SCRIPTING_get_int_value ( entity_id , line_number , 3);
				fourth_value = SCRIPTING_get_int_value ( entity_id , line_number , 4);
				SCRIPTING_get_position_at_end_of_rotated_entity (entity_pointer,first_value,second_value,third_value,fourth_value);
				break;
				

/*
			case COM_GET_TILE_NUMBERS_IN_DIRECTION:
				// Direction we check in...
				first_value = SCRIPTING_get_int_value ( entity_id , line_number , 3);
				// Distance from the entity we're checking (usually 1 for "just to the side to").
				second_value = SCRIPTING_get_int_value ( entity_id , line_number , 4);

				switch (first_value)
				{
				case DIRECTION_DOWN:
					third_value = WORLDCOLL_get_solid_tile_in_direction (entity_pointer , first_value , second_value, entity_pointer[ENT_LOWER_WIDTH]);
					fourth_value = WORLDCOLL_get_solid_tile_in_direction (entity_pointer , first_value , second_value, -entity_pointer[ENT_UPPER_WIDTH]);
					break;

				case DIRECTION_UP:
					third_value = WORLDCOLL_get_solid_tile_in_direction (entity_pointer , first_value , second_value, entity_pointer[ENT_LOWER_WIDTH]);
					fourth_value = WORLDCOLL_get_solid_tile_in_direction (entity_pointer , first_value , second_value, -entity_pointer[ENT_UPPER_WIDTH]);
					break;

				case DIRECTION_LEFT:
					third_value = WORLDCOLL_get_solid_tile_in_direction (entity_pointer , first_value , second_value, entity_pointer[ENT_LOWER_HEIGHT]);
					fourth_value = WORLDCOLL_get_solid_tile_in_direction (entity_pointer , first_value , second_value, -entity_pointer[ENT_UPPER_HEIGHT]);
					break;

				case DIRECTION_RIGHT:
					third_value = WORLDCOLL_get_solid_tile_in_direction (entity_pointer , first_value , second_value, entity_pointer[ENT_LOWER_HEIGHT]);
					fourth_value = WORLDCOLL_get_solid_tile_in_direction (entity_pointer , first_value , second_value, -entity_pointer[ENT_UPPER_HEIGHT]);
					break;

				default:
					assert(0);
					break;
				}

				SCRIPTING_put_value( entity_id , line_number , 1 , third_value );
				SCRIPTING_put_value( entity_id , line_number , 2 , fourth_value );
				break;
				
			case COM_GET_TILE_NUMBER_AT_OFFSET:
				// x-offset from the entity position we're checking...
				first_value = SCRIPTING_get_int_value ( entity_id , line_number , 2);
				// y-offset from the entity position we're checking...
				second_value = SCRIPTING_get_int_value ( entity_id , line_number , 3);
				
				SCRIPTING_put_value( entity_id , line_number , 1 , WORLDCOLL_tile_value (entity_pointer[ENT_WORLD_COLLISION_LAYER], entity_pointer[ENT_WORLD_X] + first_value , entity_pointer[ENT_WORLD_Y] + second_value ) );
				break;
*/

// ---------------- WINDOW ROUTINES ----------------

			case COM_SET_WINDOW_POSITION_AND_SIZE:
				first_value = SCRIPTING_get_int_value ( entity_id , line_number , 1);
				second_value = SCRIPTING_get_int_value ( entity_id , line_number , 2);
				third_value = SCRIPTING_get_int_value ( entity_id , line_number , 3);
				fourth_value = SCRIPTING_get_int_value ( entity_id , line_number , 4);
				SCRIPTING_screen_position_window (entity_pointer[ENT_WINDOW_NUMBER],first_value,second_value,third_value,fourth_value);
				break;

			case COM_GET_WINDOW_POSITION_AND_SIZE:
				SCRIPTING_get_screen_window_position (entity_pointer[ENT_WINDOW_NUMBER],&first_value,&second_value,&third_value,&fourth_value);
				SCRIPTING_put_value ( entity_id , line_number , 1 , first_value );
				SCRIPTING_put_value ( entity_id , line_number , 2 , second_value );
				SCRIPTING_put_value ( entity_id , line_number , 3 , third_value );
				SCRIPTING_put_value ( entity_id , line_number , 4 , fourth_value );
				break;

			case COM_SET_WINDOW_VERTEX_COLOUR:
				first_value = SCRIPTING_get_int_value ( entity_id , line_number , 1);
				second_value = SCRIPTING_get_int_value ( entity_id , line_number , 2);
				third_value = SCRIPTING_get_int_value ( entity_id , line_number , 3);
				SCRIPTING_set_window_colour (entity_pointer[ENT_WINDOW_NUMBER],first_value,second_value,third_value);
				break;

			case COM_ENABLE_WINDOW_VERTEX_COLOUR:
				SCRIPTING_enable_window_colour (entity_pointer[ENT_WINDOW_NUMBER]);
				break;

			case COM_DISABLE_WINDOW_VERTEX_COLOUR:
				SCRIPTING_disable_window_colour (entity_pointer[ENT_WINDOW_NUMBER]);
				break;

			case COM_SET_WINDOW_SCALE:
				first_value = SCRIPTING_get_int_value ( entity_id , line_number , 1);
				second_value = SCRIPTING_get_int_value ( entity_id , line_number , 2);
				third_value = SCRIPTING_get_int_value ( entity_id , line_number , 3);
				fourth_value = SCRIPTING_get_int_value ( entity_id , line_number , 4);
				fifth_value = SCRIPTING_get_int_value ( entity_id , line_number , 5);
				SCRIPTING_set_window_scale ( first_value , second_value , third_value , fourth_value , fifth_value );
				break;

			case COM_SET_WINDOW_MAP_POSITION:
				first_value = SCRIPTING_get_int_value ( entity_id , line_number , 1);
				second_value = SCRIPTING_get_int_value ( entity_id , line_number , 2);
				SCRIPTING_map_position_window (entity_pointer[ENT_WINDOW_NUMBER],first_value,second_value);
				break;

			case COM_GET_WINDOW_MAP_POSITION:
				SCRIPTING_get_map_window_position (entity_pointer[ENT_WINDOW_NUMBER],&first_value,&second_value);
				SCRIPTING_put_value ( entity_id , line_number , 1 , first_value );
				SCRIPTING_put_value ( entity_id , line_number , 2 , second_value );
				break;

			case COM_DISABLE_MULTI_TEXTURE_EFFECTS:
				OUTPUT_disable_multi_texture_effects();
				break;

			case COM_ENABLE_MULTI_TEXTURE_EFFECTS:
				OUTPUT_enable_multi_texture_effects();
				break;


// ----------------  ----------------

			case COM_SET_PRIVATE_CO_ORDS:
				entity_pointer[ENT_X] = entity_pointer[ENT_WORLD_X] << entity_pointer[ENT_BITSHIFT];
				entity_pointer[ENT_Y] = entity_pointer[ENT_WORLD_Y] << entity_pointer[ENT_BITSHIFT];
				break;

			case COM_SET_PRIVATE_X_CO_ORD:
				entity_pointer[ENT_X] = entity_pointer[ENT_WORLD_X] << entity_pointer[ENT_BITSHIFT];
				break;

			case COM_SET_PRIVATE_Y_CO_ORD:
				entity_pointer[ENT_Y] = entity_pointer[ENT_WORLD_Y] << entity_pointer[ENT_BITSHIFT];
				break;

			case COM_SET_WORLD_CO_ORDS:
				entity_pointer[ENT_WORLD_X] = entity_pointer[ENT_X] >> entity_pointer[ENT_BITSHIFT];
				entity_pointer[ENT_WORLD_Y] = entity_pointer[ENT_Y] >> entity_pointer[ENT_BITSHIFT];
				break;

			case COM_GET_DISTANCE_BETWEEN_ENTITIES:
				first_value = SCRIPTING_get_int_value ( entity_id , line_number , 4);
				second_value = SCRIPTING_get_int_value ( entity_id , line_number , 5);
				third_value = MATH_get_distance_int (entity[first_value][ENT_WORLD_X], entity[first_value][ENT_WORLD_Y], entity[second_value][ENT_WORLD_X], entity[second_value][ENT_WORLD_Y]);
				SCRIPTING_put_value ( entity_id , line_number , 1 , third_value );
			break;

// ---------------- ZONE ROUTINES ----------------

			case COM_GET_CURRENT_ZONE:
				first_value = SCRIPTING_get_int_value ( entity_id , line_number , 4);
				second_value = SCRIPTING_get_int_value ( entity_id , line_number , 5);
				third_value = SCRIPTING_get_int_value ( entity_id , line_number , 6);
				result_i = SCRIPTING_get_zone_number_of_type_at_point (collision_map, first_value, second_value, third_value);
				SCRIPTING_put_value ( entity_id , line_number , 1 , result_i );
				break;

			case COM_SPAWN_ENTITIES_IN_ZONE:
				first_value = SCRIPTING_get_int_value ( entity_id , line_number , 1);
				second_value = SCRIPTING_get_int_value ( entity_id , line_number , 2);
				third_value = SCRIPTING_get_int_value ( entity_id , line_number , 3);
				SCRIPTING_spawn_all_points_in_zone (collision_map, first_value, entity_id, second_value, third_value);
				break;

			case COM_GET_HORIZONTAL_ZONE_EXTENTS:
				first_value = SCRIPTING_get_int_value ( entity_id , line_number , 5);
				WORLDCOLL_get_horizontal_zone_extents (first_value, &second_value, &third_value);
				SCRIPTING_put_value ( entity_id , line_number , 1 , second_value );
				SCRIPTING_put_value ( entity_id , line_number , 2 , third_value );
				break;

			case COM_GET_VERTICAL_ZONE_EXTENTS:
				first_value = SCRIPTING_get_int_value ( entity_id , line_number , 5);
				WORLDCOLL_get_vertical_zone_extents (first_value, &second_value, &third_value);
				SCRIPTING_put_value ( entity_id , line_number , 1 , second_value );
				SCRIPTING_put_value ( entity_id , line_number , 2 , third_value );
				break;

			case COM_GET_TOP_LEFT_ZONE_CO_ORDS:
				first_value = SCRIPTING_get_int_value ( entity_id , line_number , 5);
				WORLDCOLL_get_top_left_zone_co_ords (first_value, &second_value, &third_value);
				SCRIPTING_put_value ( entity_id , line_number , 1 , second_value );
				SCRIPTING_put_value ( entity_id , line_number , 2 , third_value );
				break;

// ---------------- AI NODE ROUTINES ----------------

			case COM_GET_AI_NODE_IN_ZONE_OF_TYPE:
				break;

			case COM_GET_NEAREST_AI_NODE_OF_LOCATION:
				break;

			case COM_GET_NEXT_AI_NODE:
				break;

			case COM_GET_AI_NODE_POSITION:
				break;
				
// ---------------- COLLISION ROUTINES ----------------

			case COM_SET_COLLISION_FROM_FRAME:
				if (scr[line_number].script_line_size == 1)
				{
					OUTPUT_store_frame_info_in_entity_collision (entity_pointer);
				}
				else if (scr[line_number].script_line_size == 2)
				{
					first_value = SCRIPTING_get_int_value ( entity_id , line_number , 1);
					OUTPUT_store_frame_info_in_entity_collision (entity_pointer,first_value);
				}
				break;

			case COM_SET_COLLISION_FROM_FRAME_WITH_SCALE:
				if (scr[line_number].script_line_size == 1)
				{
					OUTPUT_store_frame_info_in_entity_collision_including_scale (entity_pointer);
				}
				else if (scr[line_number].script_line_size == 2)
				{
					first_value = SCRIPTING_get_int_value ( entity_id , line_number , 1);
					OUTPUT_store_frame_info_in_entity_collision_including_scale (entity_pointer,first_value);
				}
				break;

			case COM_SET_WORLD_COLLISION_FROM_OBJECT:
				entity_pointer[ENT_UPPER_WORLD_WIDTH] = entity_pointer[ENT_UPPER_WIDTH];
				entity_pointer[ENT_UPPER_WORLD_HEIGHT] = entity_pointer[ENT_UPPER_HEIGHT];
				entity_pointer[ENT_LOWER_WORLD_WIDTH] = entity_pointer[ENT_LOWER_WIDTH];
				entity_pointer[ENT_LOWER_WORLD_HEIGHT] = entity_pointer[ENT_LOWER_HEIGHT];
				break;

// ---------------- DATATABLE AND ARRAY ROUTINES ----------------

			case COM_CREATE_ARRAY:
				unique_id = SCRIPTING_get_int_value ( entity_id , line_number , 1);
				first_value = SCRIPTING_get_int_value ( entity_id , line_number , 2);

				if (scr[line_number].script_line_size == 3)
				{
					ARRAY_create_array (entity_id, unique_id, first_value);
				}
				else if (scr[line_number].script_line_size == 4)
				{
					second_value = SCRIPTING_get_int_value ( entity_id , line_number , 3);
					ARRAY_create_array (entity_id, unique_id, first_value, second_value);
				}
				else if (scr[line_number].script_line_size == 5)
				{
					second_value = SCRIPTING_get_int_value ( entity_id , line_number , 3);
					third_value = SCRIPTING_get_int_value ( entity_id , line_number , 4);
					ARRAY_create_array (entity_id, unique_id, first_value, second_value, third_value);
				}
				break;


				
			case COM_CREATE_ARRAY_FROM_DATATABLE:
				unique_id = SCRIPTING_get_int_value ( entity_id , line_number , 1);
				first_value = SCRIPTING_get_int_value ( entity_id , line_number , 2);
				
				ARRAY_create_array_from_datatable (entity_id, unique_id, first_value);
				break;



			case COM_RESET_ARRAY:
				first_value = SCRIPTING_get_int_value ( entity_id , line_number , 1); // Owner entity
				unique_id = SCRIPTING_get_int_value ( entity_id , line_number , 2);

				if (scr[line_number].script_line_size == 4)
				{
					second_value = SCRIPTING_get_int_value ( entity_id , line_number , 3); // Reset value
					ARRAY_reset_array (first_value,unique_id,second_value);
				}
				else
				{
					ARRAY_reset_array (first_value,unique_id);
				}
				break;



			case COM_RESIZE_ARRAY:
				fourth_value = SCRIPTING_get_int_value ( entity_id , line_number , 1);
				unique_id = SCRIPTING_get_int_value ( entity_id , line_number , 2);
				first_value = SCRIPTING_get_int_value ( entity_id , line_number , 3);

				if (scr[line_number].script_line_size == 3)
				{
					ARRAY_resize_array (fourth_value, unique_id, first_value);
				}
				else if (scr[line_number].script_line_size == 4)
				{
					second_value = SCRIPTING_get_int_value ( entity_id , line_number , 4);
					ARRAY_resize_array (fourth_value, unique_id, first_value, second_value);
				}
				else if (scr[line_number].script_line_size == 5)
				{
					second_value = SCRIPTING_get_int_value ( entity_id , line_number , 4);
					third_value = SCRIPTING_get_int_value ( entity_id , line_number , 5);
					ARRAY_resize_array (fourth_value, unique_id, first_value, second_value, third_value);
				}
				break;


			case COM_READ_FROM_ARRAY:
				if (scr[line_number].script_line_size == 7)
				{
					first_value = SCRIPTING_get_int_value ( entity_id , line_number , 4); // Owner entity
					unique_id = SCRIPTING_get_int_value ( entity_id , line_number , 5);
					second_value = SCRIPTING_get_int_value ( entity_id , line_number , 6); // offset

					if (second_value < 0)
					{
						OUTPUT_message("Attempting to read array element < 0");
					}

					result_i = ARRAY_read_value (first_value,unique_id,second_value);
				}
				else if (scr[line_number].script_line_size == 8)
				{
					first_value = SCRIPTING_get_int_value ( entity_id , line_number , 4); // Owner entity
					unique_id = SCRIPTING_get_int_value ( entity_id , line_number , 5);
					second_value = SCRIPTING_get_int_value ( entity_id , line_number , 6); // offset
					third_value = SCRIPTING_get_int_value ( entity_id , line_number , 7); // offset

					if ((second_value < 0) || (third_value < 0))
					{
						OUTPUT_message("Attempting to read array element < 0");
					}

					result_i = ARRAY_read_value (first_value,unique_id,second_value,third_value);
				}
				else if (scr[line_number].script_line_size == 9)
				{
					first_value = SCRIPTING_get_int_value ( entity_id , line_number , 4); // Owner entity
					unique_id = SCRIPTING_get_int_value ( entity_id , line_number , 5);
					second_value = SCRIPTING_get_int_value ( entity_id , line_number , 6); // offset
					third_value = SCRIPTING_get_int_value ( entity_id , line_number , 7); // offset
					fourth_value = SCRIPTING_get_int_value ( entity_id , line_number , 8); // offset

					if ((second_value < 0) || (third_value < 0) || (fourth_value < 0))
					{
						OUTPUT_message("Attempting to read array element < 0");
					}

					result_i = ARRAY_read_value (first_value,unique_id,second_value,third_value,fourth_value);
				}

				SCRIPTING_put_value ( entity_id , line_number , 1 , result_i );
				break;



			case COM_WRITE_TO_ARRAY:
				if (scr[line_number].script_line_size == 5)
				{
					first_value = SCRIPTING_get_int_value ( entity_id , line_number , 1); // Written value
					second_value = SCRIPTING_get_int_value ( entity_id , line_number , 2); // Owner entity
					unique_id = SCRIPTING_get_int_value ( entity_id , line_number , 3);
					third_value = SCRIPTING_get_int_value ( entity_id , line_number , 4); // offset

					ARRAY_write_value(second_value,unique_id,first_value,third_value);
				}
				else if (scr[line_number].script_line_size == 6)
				{
					first_value = SCRIPTING_get_int_value ( entity_id , line_number , 1); // Written value
					second_value = SCRIPTING_get_int_value ( entity_id , line_number , 2); // Owner entity
					unique_id = SCRIPTING_get_int_value ( entity_id , line_number , 3);
					third_value = SCRIPTING_get_int_value ( entity_id , line_number , 4); // offset
					fourth_value = SCRIPTING_get_int_value ( entity_id , line_number , 5); // offset

					ARRAY_write_value(second_value,unique_id,first_value,third_value,fourth_value);
				}
				else if (scr[line_number].script_line_size == 7)
				{
					first_value = SCRIPTING_get_int_value ( entity_id , line_number , 1); // Written value
					second_value = SCRIPTING_get_int_value ( entity_id , line_number , 2); // Owner entity
					unique_id = SCRIPTING_get_int_value ( entity_id , line_number , 3);
					third_value = SCRIPTING_get_int_value ( entity_id , line_number , 4); // offset
					fourth_value = SCRIPTING_get_int_value ( entity_id , line_number , 5); // offset
					fifth_value = SCRIPTING_get_int_value ( entity_id , line_number , 6); // offset

					ARRAY_write_value(second_value,unique_id,first_value,third_value,fourth_value,fifth_value);
				}
				break;

			case COM_READ_FROM_DATATABLE:
				first_value = SCRIPTING_get_int_value ( entity_id , line_number , 4);
				second_value = SCRIPTING_get_int_value ( entity_id , line_number , 5);
				third_value = SCRIPTING_get_int_value ( entity_id , line_number , 6);

				result_i = ARRAY_read_datatable_value (first_value,second_value,third_value);

				SCRIPTING_put_value ( entity_id , line_number , 1 , result_i );
				break;

			case COM_SET_DEFINING_DUPLICATION_CHECK:
				first_value = SCRIPTING_get_int_value ( entity_id , line_number , 1);
				second_value = SCRIPTING_get_int_value ( entity_id , line_number , 2);
				third_value = SCRIPTING_get_int_value ( entity_id , line_number , 3);

				if (third_value == 0)
				{
					CONTROL_set_dupe_check_value (first_value,second_value,false);
				}
				else
				{
					CONTROL_set_dupe_check_value (first_value,second_value,true);
				}
				break;
				
			case COM_WRITE_TEXT_TO_ARRAY:
				first_value = SCRIPTING_get_int_value ( entity_id , line_number , 4); // Owner entity
				unique_id = SCRIPTING_get_int_value ( entity_id , line_number , 5);
				second_value = SCRIPTING_get_int_value ( entity_id , line_number , 6); // Text tag
				third_value = SCRIPTING_get_int_value ( entity_id , line_number , 7); // offset
				fourth_value = SCRIPTING_get_int_value ( entity_id , line_number , 8); // boolean add space

				if (fourth_value)
				{
					result_i = ARRAY_add_text_to_array (first_value, unique_id, second_value, third_value, true);
				}
				else
				{
					result_i = ARRAY_add_text_to_array (first_value, unique_id, second_value, third_value, false);
				}

				SCRIPTING_put_value ( entity_id , line_number , 1 , result_i );
				break;

			case COM_WRITE_TEXT_NUMBER_TO_ARRAY:
				first_value = SCRIPTING_get_int_value ( entity_id , line_number , 4); // Owner entity
				unique_id = SCRIPTING_get_int_value ( entity_id , line_number , 5);
				second_value = SCRIPTING_get_int_value ( entity_id , line_number , 6); // Number
				third_value = SCRIPTING_get_int_value ( entity_id , line_number , 7); // offset
				fourth_value = SCRIPTING_get_int_value ( entity_id , line_number , 8); // boolean add space

				if (fourth_value)
				{
					result_i = ARRAY_add_text_number_to_array (first_value, unique_id, second_value, third_value, true);
				}
				else
				{
					result_i = ARRAY_add_text_number_to_array (first_value, unique_id, second_value, third_value, false);
				}

				SCRIPTING_put_value ( entity_id , line_number , 1 , result_i );
				break;

			case COM_GET_DATATABLE_LINE_COUNT:
				first_value = SCRIPTING_get_int_value ( entity_id , line_number , 4); // Owner entity
				result_i = ARRAY_get_datatable_line_count(first_value);
				SCRIPTING_put_value ( entity_id , line_number , 1 , result_i );
				break;

			case COM_GET_DATATABLE_LINE_SIZE:
				first_value = SCRIPTING_get_int_value ( entity_id , line_number , 4); // Owner entity
				result_i = ARRAY_get_datatable_line_length(first_value);
				SCRIPTING_put_value ( entity_id , line_number , 1 , result_i );
				break;

			case COM_CONVERT_DATATABLE_TO_SPECIAL_PATH:
				first_value = SCRIPTING_get_int_value ( entity_id , line_number , 1); // Table index...
				SPECPATH_convert_to_special_path (first_value);
				break;

// ----------------  ----------------

			case COM_SET_SPAWN_POINT_FLAG:
				first_value = SCRIPTING_get_int_value ( entity_id , line_number , 1);
				second_value = SCRIPTING_get_int_value ( entity_id , line_number , 2);
				SPAWNPOINTS_set_spawn_point_flag (second_value,first_value);
				break;

			case COM_GET_SPAWN_POINT_FLAG:
				second_value = SCRIPTING_get_int_value ( entity_id , line_number , 4);
				first_value = WORLDCOLL_get_flag (second_value);
				SCRIPTING_put_value ( entity_id , line_number , 1 , first_value );				
				break;
				
			case COM_SET_ZONE_FLAG:
				first_value = SCRIPTING_get_int_value ( entity_id , line_number , 1);
				second_value = SCRIPTING_get_int_value ( entity_id , line_number , 2);
				WORLDCOLL_set_flag (second_value,first_value);
				break;

			case COM_GET_ZONE_FLAG:
				second_value = SCRIPTING_get_int_value ( entity_id , line_number , 4);
				first_value = WORLDCOLL_get_flag (second_value);
				SCRIPTING_put_value ( entity_id , line_number , 1 , first_value );				
				break;

			case COM_SET_COLLISION_BY_SIZE:
				first_value = entity_pointer[ENT_WORLD_X] - entity_pointer[ENT_UPPER_WIDTH];
				second_value = entity_pointer[ENT_WORLD_Y] - entity_pointer[ENT_UPPER_HEIGHT];
				third_value = entity_pointer[ENT_WORLD_X] + entity_pointer[ENT_LOWER_WIDTH];
				fourth_value = entity_pointer[ENT_WORLD_Y] + entity_pointer[ENT_LOWER_HEIGHT];
				fifth_value = SCRIPTING_get_int_value ( entity_id , line_number , 1);
				WORLDCOLL_set_map_collision_block_real_co_ords (entity_pointer[ENT_WORLD_COLLISION_LAYER],first_value,second_value,third_value,fourth_value,fifth_value);
			break;

			case COM_SET_COLLISION_BY_PARAMETERS:
				first_value = SCRIPTING_get_int_value ( entity_id , line_number , 1);
				second_value = SCRIPTING_get_int_value ( entity_id , line_number , 2);
				third_value = SCRIPTING_get_int_value ( entity_id , line_number , 3) - 1;
				fourth_value = SCRIPTING_get_int_value ( entity_id , line_number , 4) - 1;
				fifth_value = SCRIPTING_get_int_value ( entity_id , line_number , 5);
				WORLDCOLL_set_map_collision_block (entity_pointer[ENT_WORLD_COLLISION_LAYER],first_value,second_value,first_value+third_value,second_value+fourth_value,fifth_value);
			break;

			case COM_STOP:
			//	if (CONTROL_key_down(KEY_C) == true)
				{
					CONTROL_stop_and_save_active_channels("STOP encountered!");
					
					snprintf(line, sizeof(line),"STOP in script '%s' at line %i.",GPL_get_entry_name("SCRIPTS",entity_pointer[ENT_SCRIPT_NUMBER]),line_number);
					OUTPUT_message(line);
					assert(0);
				}
			break;

			case COM_BLANK_WINDOW:
				if (scr[line_number].script_line_size == 3)
				{
					first_value = SCRIPTING_get_int_value ( entity_id , line_number , 2);
					second_value = SCRIPTING_get_int_value ( entity_id , line_number , 1);
				}
				else
				{
					first_value = SCRIPTING_get_int_value ( entity_id , line_number , 1);
					second_value = entity_pointer[ENT_WINDOW_NUMBER];
				}

				window_details[second_value].skip_me_this_frame = first_value;
			break;

			case COM_SET_RESET_PREFAB:
				first_value = SCRIPTING_get_int_value ( entity_id , line_number , 1);
				SCRIPTING_set_default_entity_from_prefab (first_value);
			break;

			case COM_USE_PREFAB:
				first_value = SCRIPTING_get_int_value ( entity_id , line_number , 1);
				SCRIPTING_use_prefab (first_value , entity_pointer);
			break;

			case COM_CONTAIN_ENTITY:
				SCRIPTING_contain_entity (entity_pointer);
			break;

// ---------------- TEXT ROUTINES ----------------

			case COM_GET_TEXT_LENGTH:
				first_value = SCRIPTING_get_int_value ( entity_id , line_number , 4);
				result_i = TEXTFILE_get_text_length (first_value);
				SCRIPTING_put_value ( entity_id , line_number , 1 , result_i );
			break;

			case COM_WRITE_VOLUMES_TO_CONFIG_FILE:
				first_value = SOUND_get_global_sound_volume ();
				snprintf(line, sizeof(line),"#SOUND EFFECTS VOLUME = %i",first_value);
				FILE_add_line_to_config_file (line);

				first_value = SOUND_get_global_music_volume ();
				snprintf(line, sizeof(line),"#MUSIC VOLUME = %i",first_value);
				FILE_add_line_to_config_file (line);
			break;

			case COM_WRITE_DEMO_INDEX_TO_CONFIG_FILE:
				snprintf(line, sizeof(line),"#NEXT DEMO INDEX = %i",demo_file_index);
				FILE_add_line_to_config_file (line);
			break;

// ---------------- SOUND ROUTINES ----------------

			case COM_PLAY_SOUND:
				first_value = SCRIPTING_get_int_value ( entity_id , line_number , 1);
				second_value = SCRIPTING_get_int_value ( entity_id , line_number , 2);
				third_value = SCRIPTING_get_int_value ( entity_id , line_number , 3);

				if (scr[line_number].script_line_size == 5)
				{
					fourth_value = SCRIPTING_get_int_value ( entity_id , line_number , 4);
				}
				else
				{
					fourth_value = SCRIPTING_calculate_pan_from_position (entity_id);
				}

				SOUND_play_sound (first_value, second_value, third_value, fourth_value, false);
			break;

			case COM_PLAY_MUSIC_STREAM:
				first_value = SCRIPTING_get_int_value ( entity_id , line_number , 1);
				second_value = SCRIPTING_get_int_value ( entity_id , line_number , 2);
				third_value = SCRIPTING_get_int_value ( entity_id , line_number , 3);
				fourth_value = SCRIPTING_get_int_value ( entity_id , line_number , 4);
				fifth_value = SCRIPTING_get_int_value ( entity_id , line_number , 5);

				SOUND_play_stream(first_value,second_value,third_value,fourth_value,fifth_value,true);
			break;

			case COM_PLAY_MUSIC_STREAM_GET_HANDLE:
				first_value = SCRIPTING_get_int_value ( entity_id , line_number , 4);
				second_value = SCRIPTING_get_int_value ( entity_id , line_number , 5);
				third_value = SCRIPTING_get_int_value ( entity_id , line_number , 6);
				fourth_value = SCRIPTING_get_int_value ( entity_id , line_number , 7);
				fifth_value = SCRIPTING_get_int_value ( entity_id , line_number , 8);

				sixth_value = SOUND_play_stream(first_value,second_value,third_value,fourth_value,fifth_value,true);
				SCRIPTING_put_value ( entity_id , line_number , 1 , sixth_value );
			break;

			case COM_STOP_MUSIC_STREAM:
			case COM_STOP_SOUND_STREAM:
				first_value = SCRIPTING_get_int_value ( entity_id , line_number , 1);
				SOUND_stop_stream (first_value);
			break;

			case COM_ADD_TO_PERSISTANT_CHANNELS:
				first_value = SCRIPTING_get_int_value ( entity_id , line_number , 1);
				SOUND_add_persistant_channel (first_value);
			break;

			case COM_ADD_TO_FADER_CHANNELS:
				first_value = SCRIPTING_get_int_value ( entity_id , line_number , 1);
				SOUND_add_fader_channel (first_value);
			break;

			case COM_REMOVE_FROM_PERSISTANT_CHANNELS:
				first_value = SCRIPTING_get_int_value ( entity_id , line_number , 1);
				SOUND_remove_from_persistant_channels (first_value);
			break;

			case COM_STOP_PERSISTANT_CHANNELS:
				SOUND_stop_persistant_channels ();
			break;

			case COM_FADE_PERSISTANT_CHANNELS:
				SOUND_transfer_persistant_channels_to_fader_channels ();
			break;

			case COM_PLAY_SOUND_GET_HANDLE:
				first_value = SCRIPTING_get_int_value ( entity_id , line_number , 4);
				second_value = SCRIPTING_get_int_value ( entity_id , line_number , 5);
				third_value = SCRIPTING_get_int_value ( entity_id , line_number , 6);

				if (scr[line_number].script_line_size == 8)
				{
					fourth_value = SCRIPTING_get_int_value ( entity_id , line_number , 7);
				}
				else
				{
					fourth_value = SCRIPTING_calculate_pan_from_position (entity_id);
				}

				fifth_value = SOUND_play_sound (first_value, second_value, third_value, fourth_value, false);
				SCRIPTING_put_value ( entity_id , line_number , 1 , fifth_value );
			break;

			case COM_PLAY_LOOPED_SOUND:
				first_value = SCRIPTING_get_int_value ( entity_id , line_number , 4);
				second_value = SCRIPTING_get_int_value ( entity_id , line_number , 5);
				third_value = SCRIPTING_get_int_value ( entity_id , line_number , 6);

				if (scr[line_number].script_line_size == 8)
				{
					fourth_value = SCRIPTING_get_int_value ( entity_id , line_number , 7);
				}
				else
				{
					fourth_value = SCRIPTING_calculate_pan_from_position (entity_id);
				}
				
				fifth_value = SOUND_play_sound (first_value, second_value, third_value, fourth_value, true);
				SCRIPTING_put_value ( entity_id , line_number , 1 , fifth_value );
			break;

			case COM_ADJUST_MUSIC_STREAM:
				first_value = SCRIPTING_get_int_value ( entity_id , line_number , 1);
				second_value = SCRIPTING_get_int_value ( entity_id , line_number , 2);
				third_value = SCRIPTING_get_int_value ( entity_id , line_number , 3);
				
				if (scr[line_number].script_line_size == 5)
				{
					fourth_value = SCRIPTING_get_int_value ( entity_id , line_number , 4);
				}
				else
				{
					fourth_value = SCRIPTING_calculate_pan_from_position (entity_id);
				}
				
				SOUND_adjust_sound (first_value, second_value, third_value, fourth_value, true);
			break;

			case COM_ADJUST_SOUND_STREAM:
			case COM_ADJUST_SOUND:
				first_value = SCRIPTING_get_int_value ( entity_id , line_number , 1);
				second_value = SCRIPTING_get_int_value ( entity_id , line_number , 2);
				third_value = SCRIPTING_get_int_value ( entity_id , line_number , 3);

				if (scr[line_number].script_line_size == 5)
				{
					fourth_value = SCRIPTING_get_int_value ( entity_id , line_number , 4);
				}
				else
				{
					fourth_value = SCRIPTING_calculate_pan_from_position (entity_id);
				}

				SOUND_adjust_sound (first_value, second_value, third_value, fourth_value);
			break;

			case COM_STOP_SOUND:
				first_value = SCRIPTING_get_int_value ( entity_id , line_number , 1);
				SOUND_stop_sound (first_value);
			break;

			case COM_GET_GLOBAL_MUSIC_VOLUME:
				result_i = SOUND_get_global_music_volume();
				SCRIPTING_put_value ( entity_id , line_number , 1 , result_i );
			break;

			case COM_SET_GLOBAL_MUSIC_VOLUME:
				first_value = SCRIPTING_get_int_value ( entity_id , line_number , 1);
				SOUND_set_global_music_volume (first_value);
			break;

			case COM_GET_GLOBAL_SOUND_VOLUME:
				result_i = SOUND_get_global_sound_volume();
				SCRIPTING_put_value ( entity_id , line_number , 1 , result_i );
			break;

			case COM_SET_GLOBAL_SOUND_VOLUME:
				first_value = SCRIPTING_get_int_value ( entity_id , line_number , 1);
				SOUND_set_global_sound_volume (first_value);
			break;

// ---------------- ARBITRARY QUAD ROUTINES ----------------

			case COM_SET_ARBITRARY_QUAD_CO_ORDINATE:
				first_value = SCRIPTING_get_int_value ( entity_id , line_number , 1);
				second_value = SCRIPTING_get_int_value ( entity_id , line_number , 2);
				third_value = SCRIPTING_get_int_value ( entity_id , line_number , 3);
				
				arbitrary_quads[entity_id].x[first_value] = second_value;
				arbitrary_quads[entity_id].y[first_value] = third_value;
			break;

			case COM_SET_ARBITRARY_QUAD_FROM_BASE:
				OUTPUT_set_arbitrary_quad_from_base(entity_id);
			break;

			case COM_ALTER_ARBITRARY_QUAD_CO_ORDINATE:
				first_value = SCRIPTING_get_int_value ( entity_id , line_number , 1);
				second_value = SCRIPTING_get_int_value ( entity_id , line_number , 2);
				third_value = SCRIPTING_get_int_value ( entity_id , line_number , 3);
				
				arbitrary_quads[entity_id].x[first_value] += second_value;
				arbitrary_quads[entity_id].y[first_value] += third_value;
			break;

			case COM_CALCULATE_ARBITRARY_QUAD_LINE_LENGTHS:
				SCRIPTING_calc_arb_line_lengths(entity_id);
			break;

			case COM_SET_INDIVIDUAL_VERTEX_COLOURS:
				first_value = SCRIPTING_get_int_value ( entity_id , line_number , 1);
				arbitrary_vertex_colours[entity_id].red[first_value] = float(SCRIPTING_get_int_value ( entity_id , line_number , 2)) / 255.0f;
				arbitrary_vertex_colours[entity_id].green[first_value] = float(SCRIPTING_get_int_value ( entity_id , line_number , 3)) / 255.0f;
				arbitrary_vertex_colours[entity_id].blue[first_value] = float(SCRIPTING_get_int_value ( entity_id , line_number , 4)) / 255.0f;
				arbitrary_vertex_colours[entity_id].alpha[first_value] = float(SCRIPTING_get_int_value ( entity_id , line_number , 5)) / 255.0f;
			break;

// ---------------- CONFIG FILE ROUTINES ----------------

			case COM_START_CONFIG_FILE:
				FILE_start_config_file ();
			break;

			case COM_WRITE_PLAYER_CONTROL_TO_CONFIG_FILE:
				first_value = SCRIPTING_get_int_value ( entity_id , line_number , 1);
				second_value = SCRIPTING_get_int_value ( entity_id , line_number , 2);
				FILE_add_line_to_config_file (CONTROL_get_control_description (first_value-1,second_value));
			break;

			case COM_WRITE_TEXT_LINE_TO_CONFIG_FILE:
				first_value = SCRIPTING_get_int_value ( entity_id , line_number , 1);
				FILE_add_line_to_config_file (TEXTFILE_get_line_by_index (first_value));
			break;

			case COM_END_CONFIG_FILE:
				FILE_end_config_file ();
			break;

			case COM_GET_WINDOWED_STATUS:
				if (run_windowed)
				{
					SCRIPTING_put_value ( entity_id , line_number , 1 , 1 );
				}
				else
				{
					SCRIPTING_put_value ( entity_id , line_number , 1 , 0 );
				}
			break;

			case COM_GET_COLOUR_DEPTH:
				SCRIPTING_put_value ( entity_id , line_number , 1 , colour_depth );
			break;

// ----------------  ----------------

			case COM_LOAD_HISCORE_TABLE:
				text_tag = SCRIPTING_get_int_value ( entity_id , line_number , 4);

				boolean_result = HISCORES_load_hiscore_table (text_tag);

				if (boolean_result == true)
				{
					SCRIPTING_put_value ( entity_id , line_number , 1 , RETRENGINE_TRUE );	
				}
				else
				{
					SCRIPTING_put_value ( entity_id , line_number , 1 , RETRENGINE_FALSE );	
				}
			break;

			case COM_CREATE_HISCORE_TABLE:
				unique_id = SCRIPTING_get_int_value ( entity_id , line_number , 1);
				max_entries = SCRIPTING_get_int_value ( entity_id , line_number , 2);
				name_length = SCRIPTING_get_int_value ( entity_id , line_number , 3);
				max_score_digits = SCRIPTING_get_int_value ( entity_id , line_number , 4);
				table_type = SCRIPTING_get_int_value ( entity_id , line_number , 5);

				HISCORES_create_hiscore_table (unique_id, table_type, max_entries, name_length, max_score_digits);
			break;

			case COM_SAVE_HISCORE_TABLE:
				unique_id = SCRIPTING_get_int_value ( entity_id , line_number , 1);
				text_tag = SCRIPTING_get_int_value ( entity_id , line_number , 2);
				HISCORES_save_hiscore_table (unique_id,text_tag);
			break;

			case COM_ADD_SCORE_TO_HISCORE_TABLE:
				first_value = SCRIPTING_get_int_value ( entity_id , line_number , 5);
				unique_id = SCRIPTING_get_int_value ( entity_id , line_number , 4);
				text_array = SCRIPTING_get_int_value ( entity_id , line_number , 6);
				second_value = SCRIPTING_get_int_value ( entity_id , line_number , 7);

				third_value = HISCORES_insert_score (unique_id, ARRAY_get_array_as_text (entity_id, text_array, second_value), first_value);
				SCRIPTING_put_value ( entity_id , line_number , 1 , third_value );
			break;

			case COM_ADD_SCORE_TO_HISCORE_TABLE_FROM_TEXT:
				first_value = SCRIPTING_get_int_value ( entity_id , line_number , 5);
				unique_id = SCRIPTING_get_int_value ( entity_id , line_number , 4);
				text_tag = SCRIPTING_get_int_value ( entity_id , line_number , 6);

				third_value = HISCORES_insert_score (unique_id, TEXTFILE_get_line_by_index(text_tag), first_value);
				SCRIPTING_put_value ( entity_id , line_number , 1 , third_value );
			break;

			case COM_GET_HISCORE:
				unique_id = SCRIPTING_get_int_value ( entity_id , line_number , 4);
				entry_number = SCRIPTING_get_int_value ( entity_id , line_number , 5);	

				first_value = HISCORE_get_score(unique_id,entry_number);

				SCRIPTING_put_value ( entity_id , line_number , 1 , first_value );
			break;

			case COM_ADD_RETROSPEC_HISCORE:
				unique_id = SCRIPTING_get_int_value ( entity_id , line_number , 1);
				first_value = SCRIPTING_get_int_value ( entity_id , line_number , 2);
				HISCORES_append_new_hiscore (unique_id,first_value);
			break;

// ---------------- TEXT ROUTINES ----------------


			case COM_READ_TEXT_LETTER:
				text_tag = SCRIPTING_get_int_value ( entity_id , line_number , 4);
				first_value = SCRIPTING_get_int_value ( entity_id , line_number , 5);

				if (first_value < TEXTFILE_get_line_length_by_index(text_tag))
				{
					//char_pointer
					result_i = TEXTFILE_get_line_by_index(text_tag)[first_value];
					SCRIPTING_put_value ( entity_id , line_number , 1 , result_i );
				}
				else
				{
					// Reading outside bounds!
					assert(0);
				}
			break;

			case COM_OVERWRITE_ENTITIES_FROM_TEXT:// CONSTANT(OVERWRITE_TRAILING_SIBLINGS) VARIABLE(FIRST_ENTITY) TEXT|VARIABLE(TEXT_FOR_LETTERS) VARIABLE|NUMBER|CONSTANT(START_LETTER_IN_WORD) VARIABLE|NUMBER|CONSTANT(NUMBER_OF_LETTERS_TO_CREATE)
				if (scr[line_number].script_line_size == 6)
				{
					overwrite_trailing_entities = SCRIPTING_get_bool_value ( entity_id , line_number , 1);
					first_entity = SCRIPTING_get_int_value ( entity_id , line_number , 2);
					text_tag = SCRIPTING_get_int_value ( entity_id , line_number , 3);
					start_letter = SCRIPTING_get_int_value ( entity_id , line_number , 4);
					letter_counter = SCRIPTING_get_int_value ( entity_id , line_number , 5);

					SCRIPTING_overwrite_entities_from_text (overwrite_trailing_entities, first_entity, text_tag,start_letter,letter_counter);
				}
				else
				{
					overwrite_trailing_entities = SCRIPTING_get_bool_value ( entity_id , line_number , 1);
					first_entity = SCRIPTING_get_int_value ( entity_id , line_number , 2);
					text_tag = SCRIPTING_get_int_value ( entity_id , line_number , 3);

					SCRIPTING_overwrite_entities_from_text (overwrite_trailing_entities, first_entity, text_tag);
				}
			break;

			case COM_OVERWRITE_ENTITIES_FROM_HISCORE_NAME:// CONSTANT(OVERWRITE_TRAILING_SIBLINGS) VARIABLE(FIRST_ENTITY) VARIABLE|NUMBER|CONSTANT(HISCORE_UNIQUE_ID) VARIABLE|NUMBER|CONSTANT(HISCORE_ENTRY)
				overwrite_trailing_entities = SCRIPTING_get_bool_value ( entity_id , line_number , 1);
				first_entity = SCRIPTING_get_int_value ( entity_id , line_number , 2);
				unique_id = SCRIPTING_get_int_value ( entity_id , line_number , 3);
				entry_number = SCRIPTING_get_int_value ( entity_id , line_number , 4);
				
				SCRIPTING_overwrite_entities_from_hiscore_name (overwrite_trailing_entities, first_entity, unique_id, entry_number);
			break;

			case COM_OVERWRITE_ENTITIES_FROM_HISCORE_SCORE:// CONSTANT(OVERWRITE_TRAILING_SIBLINGS) VARIABLE(FIRST_ENTITY) VARIABLE|NUMBER|CONSTANT(HISCORE_UNIQUE_ID) VARIABLE|NUMBER|CONSTANT(HISCORE_ENTRY)
				overwrite_trailing_entities = SCRIPTING_get_bool_value ( entity_id , line_number , 1);
				first_entity = SCRIPTING_get_int_value ( entity_id , line_number , 2);
				unique_id = SCRIPTING_get_int_value ( entity_id , line_number , 3);
				entry_number = SCRIPTING_get_int_value ( entity_id , line_number , 4);
				replace_space_with_zeros = SCRIPTING_get_bool_value ( entity_id , line_number , 5);

				SCRIPTING_overwrite_entities_from_hiscore_score (overwrite_trailing_entities, first_entity, unique_id, entry_number, replace_space_with_zeros);
			break;

			case COM_OVERWRITE_ENTITIES_FROM_HISCORE:// CONSTANT(OVERWRITE_TRAILING_SIBLINGS) VARIABLE(FIRST_ENTITY) VARIABLE|NUMBER|CONSTANT(HISCORE_UNIQUE_ID) VARIABLE|NUMBER|CONSTANT(HISCORE_ENTRY)
				overwrite_trailing_entities = SCRIPTING_get_bool_value ( entity_id , line_number , 1);
				first_entity = SCRIPTING_get_int_value ( entity_id , line_number , 2);
				unique_id = SCRIPTING_get_int_value ( entity_id , line_number , 3);
				entry_number = SCRIPTING_get_int_value ( entity_id , line_number , 4);
				replace_space_with_zeros = SCRIPTING_get_bool_value ( entity_id , line_number , 5);
				name_first = SCRIPTING_get_bool_value ( entity_id , line_number , 6);
				letter_gap = SCRIPTING_get_int_value ( entity_id , line_number , 7);

				SCRIPTING_overwrite_entities_from_hiscore (overwrite_trailing_entities,first_entity,unique_id,entry_number,replace_space_with_zeros,name_first,letter_gap);
			break;
				
			case COM_OVERWRITE_ENTITIES_FROM_TEXT_IN_ARRAY:// CONSTANT(OVERWRITE_TRAILING_SIBLINGS) VARIABLE(FIRST_ENTITY) VARIABLE|NUMBER|CONSTANT(ARRAY_UNIQUE_ID) VARIABLE(ARRAY_OWNER_ENTITY)
				overwrite_trailing_entities = SCRIPTING_get_bool_value ( entity_id , line_number , 1);
				first_entity = SCRIPTING_get_int_value ( entity_id , line_number , 2);
				unique_id = SCRIPTING_get_int_value ( entity_id , line_number , 3);
				first_value = SCRIPTING_get_int_value ( entity_id , line_number , 4); // Owner
				
				SCRIPTING_overwrite_entities_from_text_in_array(overwrite_trailing_entities,first_entity,unique_id,first_value);
			break;

			case COM_OVERWRITE_ENTITIES_FROM_NUMBER:// CONSTANT(OVERWRITE_TRAILING_SIBLINGS) VARIABLE(FIRST_ENTITY) VARIABLE|NUMBER|CONSTANT(VALUE) VARIABLE|NUMBER|CONSTANT(NUMBER_OF_DIGITS) CONSTANT(PAD_WITH_ZEROS)
				overwrite_trailing_entities = SCRIPTING_get_bool_value ( entity_id , line_number , 1);
				first_entity = SCRIPTING_get_int_value ( entity_id , line_number , 2);
				first_value = SCRIPTING_get_int_value ( entity_id , line_number , 3);
				second_value = SCRIPTING_get_int_value ( entity_id , line_number , 4);
				replace_space_with_zeros = SCRIPTING_get_bool_value ( entity_id , line_number , 5);

				SCRIPTING_overwrite_entities_from_number(overwrite_trailing_entities,first_entity,first_value,second_value,replace_space_with_zeros);
			break;

			case COM_SPAWN_ENTITIES_FROM_TEXT:
				include_spaces = SCRIPTING_get_bool_value ( entity_id , line_number , 1);
				spawn_identity = SCRIPTING_get_int_value ( entity_id , line_number , 2);
				script_number = SCRIPTING_get_int_value ( entity_id , line_number , 3);
				text_tag = SCRIPTING_get_int_value ( entity_id , line_number , 4);
				process_offset = SCRIPTING_get_int_value ( entity_id , line_number , 5);
				centering_constant = SCRIPTING_get_int_value ( entity_id , line_number , 6);
				start_x = SCRIPTING_get_int_value ( entity_id , line_number , 7);
				start_y = SCRIPTING_get_int_value ( entity_id , line_number , 8);
				offset_x = SCRIPTING_get_int_value ( entity_id , line_number , 9);
				offset_y = SCRIPTING_get_int_value ( entity_id , line_number , 10);

				if (scr[line_number].script_line_size == 12)
				{
					graphic_number = SCRIPTING_get_int_value ( entity_id , line_number , 11);
				}
				else if (scr[line_number].script_line_size == 14)
				{
					graphic_number = SCRIPTING_get_int_value ( entity_id , line_number , 13);
				}
				else
				{
					graphic_number = UNSET;
				}

				if (scr[line_number].script_line_size > 12)
				{
					start_letter = SCRIPTING_get_int_value ( entity_id , line_number , 11);
					letter_counter = SCRIPTING_get_int_value ( entity_id , line_number , 12);
				}
				else
				{
					start_letter = 0;
					letter_counter = UNSET;
				}

				SCRIPTING_spawn_entities_from_text ( include_spaces, spawn_identity, entity_id , script_number , text_tag , process_offset , centering_constant , start_x , start_y , offset_x , offset_y , start_letter , letter_counter , graphic_number );
			break;

			case COM_SPAWN_ENTITIES_FROM_HISCORE_NAME:
				include_spaces = SCRIPTING_get_bool_value ( entity_id , line_number , 1);
				spawn_identity = SCRIPTING_get_int_value ( entity_id , line_number , 2);
				script_number = SCRIPTING_get_int_value ( entity_id , line_number , 3);
				process_offset = SCRIPTING_get_int_value ( entity_id , line_number , 4);
				centering_constant = SCRIPTING_get_int_value ( entity_id , line_number , 5);
				unique_id = SCRIPTING_get_int_value ( entity_id , line_number , 6);
				entry_number = SCRIPTING_get_int_value ( entity_id , line_number , 7);

				start_x = SCRIPTING_get_int_value ( entity_id , line_number , 8);
				start_y = SCRIPTING_get_int_value ( entity_id , line_number , 9);
				offset_x = SCRIPTING_get_int_value ( entity_id , line_number , 10);
				offset_y = SCRIPTING_get_int_value ( entity_id , line_number , 11);

				if (scr[line_number].script_line_size == 13)
				{
					graphic_number = SCRIPTING_get_int_value ( entity_id , line_number , 12);
				}
				else
				{
					graphic_number = UNSET;
				}

				SCRIPTING_spawn_entities_from_hiscore_name (include_spaces, spawn_identity, entity_id , script_number, unique_id, entry_number , process_offset, centering_constant, start_x, start_y, offset_x, offset_y, graphic_number);
			break;

			case COM_SPAWN_ENTITIES_FROM_HISCORE_SCORE:
				include_spaces = SCRIPTING_get_bool_value ( entity_id , line_number , 1);
				spawn_identity = SCRIPTING_get_int_value ( entity_id , line_number , 2);
				script_number = SCRIPTING_get_int_value ( entity_id , line_number , 3);
				process_offset = SCRIPTING_get_int_value ( entity_id , line_number , 4);
				centering_constant = SCRIPTING_get_int_value ( entity_id , line_number , 5);
				unique_id = SCRIPTING_get_int_value ( entity_id , line_number , 6);
				entry_number = SCRIPTING_get_int_value ( entity_id , line_number , 7);

				replace_space_with_zeros = SCRIPTING_get_bool_value ( entity_id , line_number , 8);

				start_x = SCRIPTING_get_int_value ( entity_id , line_number , 9);
				start_y = SCRIPTING_get_int_value ( entity_id , line_number , 10);
				offset_x = SCRIPTING_get_int_value ( entity_id , line_number , 11);
				offset_y = SCRIPTING_get_int_value ( entity_id , line_number , 12);

				if (scr[line_number].script_line_size == 14)
				{
					graphic_number = SCRIPTING_get_int_value ( entity_id , line_number , 13);
				}
				else
				{
					graphic_number = UNSET;
				}

				SCRIPTING_spawn_entities_from_hiscore_score (include_spaces, spawn_identity, entity_id , script_number, unique_id, entry_number , replace_space_with_zeros , process_offset, centering_constant, start_x, start_y, offset_x, offset_y, graphic_number);
			break;

			case COM_SPAWN_ENTITIES_FROM_HISCORE:
				include_spaces = SCRIPTING_get_bool_value ( entity_id , line_number , 1);
				spawn_identity = SCRIPTING_get_int_value ( entity_id , line_number , 2);
				script_number = SCRIPTING_get_int_value ( entity_id , line_number , 3);
				process_offset = SCRIPTING_get_int_value ( entity_id , line_number , 4);
				centering_constant = SCRIPTING_get_int_value ( entity_id , line_number , 5);
				unique_id = SCRIPTING_get_int_value ( entity_id , line_number , 6);
				entry_number = SCRIPTING_get_int_value ( entity_id , line_number , 7);

				replace_space_with_zeros = SCRIPTING_get_bool_value ( entity_id , line_number , 8);
				name_first = SCRIPTING_get_bool_value ( entity_id , line_number , 9);

				letter_gap = SCRIPTING_get_int_value ( entity_id , line_number , 10);
				start_x = SCRIPTING_get_int_value ( entity_id , line_number , 11);
				start_y = SCRIPTING_get_int_value ( entity_id , line_number , 12);
				offset_x = SCRIPTING_get_int_value ( entity_id , line_number , 13);
				offset_y = SCRIPTING_get_int_value ( entity_id , line_number , 14);
//				start_letter = SCRIPTING_get_int_value ( entity_id , line_number , 14);
//				letter_counter = SCRIPTING_get_int_value ( entity_id , line_number , 15);

				SCRIPTING_spawn_entities_from_hiscore (include_spaces, spawn_identity, entity_id , script_number, unique_id, entry_number , replace_space_with_zeros , name_first, letter_gap, process_offset, centering_constant, start_x, start_y, offset_x, offset_y);
			break;

			case COM_SPAWN_ENTITIES_FROM_TEXT_IN_ARRAY:
				include_spaces = SCRIPTING_get_bool_value ( entity_id , line_number , 1);
				spawn_identity = SCRIPTING_get_int_value ( entity_id , line_number , 2);
				script_number = SCRIPTING_get_int_value ( entity_id , line_number , 3);
				unique_id = SCRIPTING_get_int_value ( entity_id , line_number , 4);
				process_offset = SCRIPTING_get_int_value ( entity_id , line_number , 5);
				centering_constant = SCRIPTING_get_int_value ( entity_id , line_number , 6);

				start_x = SCRIPTING_get_int_value ( entity_id , line_number , 7);
				start_y = SCRIPTING_get_int_value ( entity_id , line_number , 8);
				offset_x = SCRIPTING_get_int_value ( entity_id , line_number , 9);
				offset_y = SCRIPTING_get_int_value ( entity_id , line_number , 10);

				if (scr[line_number].script_line_size == 12)
				{
					graphic_number = SCRIPTING_get_int_value ( entity_id , line_number , 11);
				}
				else
				{
					graphic_number = UNSET;
				}

				SCRIPTING_spawn_entities_from_text_in_array ( include_spaces, spawn_identity, entity_id , script_number , unique_id , process_offset , centering_constant , start_x , start_y , offset_x , offset_y , graphic_number);
			break;

// ---------------- INPUT RECORDING AND PLAYBACK ----------------

			case COM_START_RECORDING_INPUT:
				first_value = SCRIPTING_get_int_value ( entity_id , line_number , 1);
				CONTROL_start_recording_input_next_frame ( first_value );
			break;

			case COM_STOP_RECORDING_INPUT:
				first_value = SCRIPTING_get_int_value ( entity_id , line_number , 1);
				CONTROL_stop_recording_input ( first_value );
			break;

			case COM_SAVE_RECORDED_INPUT:
				first_value = SCRIPTING_get_int_value ( entity_id , line_number , 1);
				second_value = SCRIPTING_get_int_value ( entity_id , line_number , 2);
				CONTROL_save_recorded_input ( first_value , TEXTFILE_get_line_by_index(second_value) );
			break;

			case COM_LOAD_RECORDED_INPUT:
				first_value = SCRIPTING_get_int_value ( entity_id , line_number , 1);
				second_value = SCRIPTING_get_int_value ( entity_id , line_number , 2);
//				CONTROL_load_recorded_input ( first_value-1 , TEXTFILE_get_line_by_index(second_value) );
				CONTROL_load_compressed_recorded_input_and_inflate ( first_value , TEXTFILE_get_line_by_index(second_value) );
			break;

			case COM_GET_RECORD_PLAYBACK_STATUS:
				first_value = SCRIPTING_get_int_value ( entity_id , line_number , 4);
				second_value = CONTROL_get_recording_playback_status ( first_value );
				SCRIPTING_put_value ( entity_id , line_number , 1 , second_value );
			break;

			case COM_START_PLAYBACK_INPUT:
				first_value = SCRIPTING_get_int_value ( entity_id , line_number , 1);
				CONTROL_start_playback_input_next_frame ( first_value );
			break;

			case COM_STOP_PLAYBACK_INPUT:
				first_value = SCRIPTING_get_int_value ( entity_id , line_number , 1);
				CONTROL_stop_playback ( first_value );
			break;

// ---------------- SAVE GAMES ----------------

			case COM_RESET_SAVE_FILE:
				SCRIPTING_reset_save_data();
			break;

			case COM_WRITE_FLAG_TO_SAVE_FILE:
				first_value = SCRIPTING_get_int_value(entity_id,line_number,1);
				SCRIPTING_save_flag (first_value);
			break;

			case COM_WRITE_SPAWN_POINT_FLAGS_TO_SAVE_FILE:
				SCRIPTING_save_spawn_point_flags();
			break;

			case COM_WRITE_ZONE_FLAGS_TO_SAVE_FILE:
				SCRIPTING_save_zone_flags();
			break;

			case COM_WRITE_AI_NODE_TABLES_TO_SAVE_FILE:
				SCRIPTING_save_ai_node_tables();
			break;

			case COM_WRITE_AI_ZONE_TABLES_TO_SAVE_FILE:
				SCRIPTING_save_ai_zone_tables();
			break;

			case COM_WRITE_ENTITY_TO_SAVE_FILE:
				first_value = SCRIPTING_get_int_value(entity_id,line_number,1);
				second_value = SCRIPTING_get_int_value(entity_id,line_number,2);
				SCRIPTING_save_entity (first_value,second_value);
			break;

			case COM_OUTPUT_SAVE_FILE:
				first_value = SCRIPTING_get_int_value(entity_id,line_number,1);
				SCRIPTING_output_save_data_to_file (first_value);
			break;

			case COM_LOAD_SAVE_FILE:
				first_value = SCRIPTING_get_int_value(entity_id,line_number,1);
				SCRIPTING_load_save_file (first_value);
			break;

			case COM_DELETE_SAVE_FILE:
				first_value = SCRIPTING_get_int_value(entity_id,line_number,1);
				SCRIPTING_delete_save_file(first_value);
			break;

			case COM_GET_SAVE_FILE_EXISTS:
				first_value = SCRIPTING_get_int_value(entity_id,line_number,4);
				result_i = SCRIPTING_does_save_file_exist(first_value);
				SCRIPTING_put_value ( entity_id , line_number , 1 , result_i );
			break;

			case COM_OVER_WRITE_ENTITY_FROM_SAVE_FILE:
				first_value = SCRIPTING_get_int_value(entity_id,line_number,1);
				second_value = SCRIPTING_get_int_value(entity_id,line_number,2);
				SAVEGAME_restore_entity_from_loaded_tag(first_value, second_value);
			break;

			case COM_WRITE_MATCHING_ENTITIES_TO_SAVE_FILE:
				first_value = scr[line_number].script_line_pointer[1].data_value;
				operation = SCRIPTING_get_int_value(entity_id, line_number, 2);
				second_value = SCRIPTING_get_int_value(entity_id, line_number, 3);
				third_value = SCRIPTING_get_int_value(entity_id, line_number, 4);
				SAVEGAME_save_matching_live_entities(first_value, operation, second_value, third_value);
			break;

			case COM_SPAWN_MATCHING_ENTITIES_FROM_SAVE_FILE:
				first_value = scr[line_number].script_line_pointer[1].data_value;
				operation = SCRIPTING_get_int_value(entity_id, line_number, 2);
				second_value = SCRIPTING_get_int_value(entity_id, line_number, 3);
				SAVEGAME_spawn_matching_loaded_entities(first_value, operation, second_value);
			break;

// ---------------- PLAYER CONTROL FEEDBACK ----------------

			case COM_GET_CONTROL_DEVICE:
				first_value = SCRIPTING_get_int_value(entity_id,line_number,4);
				second_value = SCRIPTING_get_int_value(entity_id,line_number,5);
				
				SCRIPTING_put_value ( entity_id , line_number , 1 , CONTROL_read_back_player_control_device(first_value,second_value) );
			break;

			case COM_GET_CONTROL_BUTTON:
				first_value = SCRIPTING_get_int_value(entity_id,line_number,4);
				second_value = SCRIPTING_get_int_value(entity_id,line_number,5);
				
				SCRIPTING_put_value ( entity_id , line_number , 1 , CONTROL_read_back_player_control_button(first_value,second_value) );
			break;

			case COM_GET_CONTROL_PORT:
				first_value = SCRIPTING_get_int_value(entity_id,line_number,4);
				second_value = SCRIPTING_get_int_value(entity_id,line_number,5);
				
				SCRIPTING_put_value ( entity_id , line_number , 1 , CONTROL_read_back_player_control_port(first_value,second_value) );
			break;

			case COM_GET_CONTROL_STICK:
				first_value = SCRIPTING_get_int_value(entity_id,line_number,4);
				second_value = SCRIPTING_get_int_value(entity_id,line_number,5);
				
				SCRIPTING_put_value ( entity_id , line_number , 1 , CONTROL_read_back_player_control_stick(first_value,second_value) );
			break;

			case COM_GET_CONTROL_AXIS:
				first_value = SCRIPTING_get_int_value(entity_id,line_number,4);
				second_value = SCRIPTING_get_int_value(entity_id,line_number,5);
				
				SCRIPTING_put_value ( entity_id , line_number , 1 , CONTROL_read_back_player_control_axis(first_value,second_value) );
			break;

// ---------------- TILEMAP STUFF ----------------

				case COM_GET_TILEMAP_WIDTH:
					boolean_result = SCRIPTING_get_bool_value(entity_id,line_number,4);
				
				if (scr[line_number].script_line_size == 5)
				{
					// We just use the collision map as our reference
					if (collision_map != UNSET)
					{
						first_value = collision_map;
					}
					else
					{
						OUTPUT_message("Trying to use collision map when getting width and height when collision map is unset!");
						assert(0);
					}
				}
				else
				{
					first_value = SCRIPTING_get_int_value(entity_id,line_number,5);
				}

					result_i = TILEMAPS_get_width (first_value,boolean_result);
					if (output_debug_information && boolean_result && result_i <= 0)
					{
						char line[MAX_LINE_SIZE];
						const char *caller = GPL_get_entry_name("SCRIPTS", entity_pointer[ENT_SCRIPT_NUMBER]);
						int map_width_tiles = 0;
						int map_tileset = UNSET;
						int map_tilesize = 0;
						if ((first_value >= 0) && (first_value < number_of_tilemaps_loaded) && cm != NULL)
						{
							map_width_tiles = cm[first_value].map_width;
							map_tileset = cm[first_value].tile_set_index;
							map_tilesize = TILESETS_get_tilesize(map_tileset);
						}
							snprintf(line, sizeof(line),
							"DEBUG_TILEMAP_WIDTH caller=%s map=%d in_pixels=%d result=%d map_width_tiles=%d tile_set_index=%d tile_size=%d",
							caller,
							first_value,
							int(boolean_result),
							result_i,
							map_width_tiles,
							map_tileset,
							map_tilesize);
						MAIN_add_to_log(line);
					}
					SCRIPTING_put_value ( entity_id , line_number , 1 , result_i );
				break;

			case COM_GET_TILEMAP_HEIGHT:
				boolean_result = SCRIPTING_get_bool_value(entity_id,line_number,4);
				
				if (scr[line_number].script_line_size == 5)
				{
					// We just use the collision map as our reference
					if (collision_map != UNSET)
					{
						first_value = collision_map;
					}
					else
					{
						OUTPUT_message("Trying to use collision map when getting width and height when collision map is unset!");
						assert(0);
					}
				}
				else
				{
					first_value = SCRIPTING_get_int_value(entity_id,line_number,5);
				}

				SCRIPTING_put_value ( entity_id , line_number , 1 , TILEMAPS_get_height (first_value,boolean_result) );
			break;


			case COM_GET_PIVOTS_FROM_FRAME:
				if (scr[line_number].script_line_size == 5)
				{
					first_value = entity_pointer[ENT_SPRITE];
					second_value = entity_pointer[ENT_CURRENT_FRAME];
				}
				else
				{
					first_value = SCRIPTING_get_int_value(entity_id,line_number,5);
					second_value = SCRIPTING_get_int_value(entity_id,line_number,6);
				}
				
				SCRIPTING_put_value(entity_id,line_number,1,OUTPUT_get_sprite_pivot_x(first_value,second_value));
				SCRIPTING_put_value(entity_id,line_number,2,OUTPUT_get_sprite_pivot_y(first_value,second_value));
			break;

			case COM_ALTER_PIVOTS_AND_COMPENSATE:
				start_x = entity_pointer[ENT_OVERRIDE_PIVOT_X];
				start_y = entity_pointer[ENT_OVERRIDE_PIVOT_Y];
				
				if (scr[line_number].script_line_size == 3)
				{
					entity_pointer[ENT_OVERRIDE_PIVOT_X] = SCRIPTING_get_int_value(entity_id,line_number,1);
					entity_pointer[ENT_OVERRIDE_PIVOT_Y] = SCRIPTING_get_int_value(entity_id,line_number,2);
				}
				else
				{
					switch (SCRIPTING_get_int_value(entity_id,line_number,1))
					{
					case SPRITE_PIVOT_CENTRE:
						entity_pointer[ENT_OVERRIDE_PIVOT_X] = OUTPUT_get_sprite_width(entity_pointer[ENT_SPRITE],entity_pointer[ENT_CURRENT_FRAME])/2;
						entity_pointer[ENT_OVERRIDE_PIVOT_Y] = OUTPUT_get_sprite_height(entity_pointer[ENT_SPRITE],entity_pointer[ENT_CURRENT_FRAME])/2;
						break;

					case SPRITE_PIVOT_TOP:
						entity_pointer[ENT_OVERRIDE_PIVOT_X] = OUTPUT_get_sprite_width(entity_pointer[ENT_SPRITE],entity_pointer[ENT_CURRENT_FRAME])/2;
						entity_pointer[ENT_OVERRIDE_PIVOT_Y] = 0;
						break;

					case SPRITE_PIVOT_TOP_RIGHT:
						entity_pointer[ENT_OVERRIDE_PIVOT_X] = OUTPUT_get_sprite_width(entity_pointer[ENT_SPRITE],entity_pointer[ENT_CURRENT_FRAME])-1;
						entity_pointer[ENT_OVERRIDE_PIVOT_Y] = 0;
						break;

					case SPRITE_PIVOT_RIGHT:
						entity_pointer[ENT_OVERRIDE_PIVOT_X] = OUTPUT_get_sprite_width(entity_pointer[ENT_SPRITE],entity_pointer[ENT_CURRENT_FRAME])-1;
						entity_pointer[ENT_OVERRIDE_PIVOT_Y] = OUTPUT_get_sprite_height(entity_pointer[ENT_SPRITE],entity_pointer[ENT_CURRENT_FRAME])/2;
						break;

					case SPRITE_PIVOT_BOTTOM_RIGHT:
						entity_pointer[ENT_OVERRIDE_PIVOT_X] = OUTPUT_get_sprite_width(entity_pointer[ENT_SPRITE],entity_pointer[ENT_CURRENT_FRAME])-1;
						entity_pointer[ENT_OVERRIDE_PIVOT_Y] = OUTPUT_get_sprite_height(entity_pointer[ENT_SPRITE],entity_pointer[ENT_CURRENT_FRAME])-1;
						break;

					case SPRITE_PIVOT_BOTTOM:
						entity_pointer[ENT_OVERRIDE_PIVOT_X] = OUTPUT_get_sprite_width(entity_pointer[ENT_SPRITE],entity_pointer[ENT_CURRENT_FRAME])/2;
						entity_pointer[ENT_OVERRIDE_PIVOT_Y] = OUTPUT_get_sprite_height(entity_pointer[ENT_SPRITE],entity_pointer[ENT_CURRENT_FRAME])-1;
						break;

					case SPRITE_PIVOT_BOTTOM_LEFT:
						entity_pointer[ENT_OVERRIDE_PIVOT_X] = 0;
						entity_pointer[ENT_OVERRIDE_PIVOT_Y] = OUTPUT_get_sprite_height(entity_pointer[ENT_SPRITE],entity_pointer[ENT_CURRENT_FRAME])-1;
						break;

					case SPRITE_PIVOT_LEFT:
						entity_pointer[ENT_OVERRIDE_PIVOT_X] = 0;
						entity_pointer[ENT_OVERRIDE_PIVOT_Y] = OUTPUT_get_sprite_pivot_y(entity_pointer[ENT_SPRITE],entity_pointer[ENT_CURRENT_FRAME]);
						break;

					case SPRITE_PIVOT_TOP_LEFT:
						entity_pointer[ENT_OVERRIDE_PIVOT_X] = 0;
						entity_pointer[ENT_OVERRIDE_PIVOT_Y] = 0;
						break;

					case SPRITE_PIVOT_ORIGINAL:
						entity_pointer[ENT_OVERRIDE_PIVOT_X] = OUTPUT_get_sprite_pivot_x(entity_pointer[ENT_SPRITE],entity_pointer[ENT_CURRENT_FRAME]);
						entity_pointer[ENT_OVERRIDE_PIVOT_Y] = OUTPUT_get_sprite_pivot_y(entity_pointer[ENT_SPRITE],entity_pointer[ENT_CURRENT_FRAME]);
						break;

					case SPRITE_PIVOT_ADAPT_TO_ORIGINAL:
						if (entity_pointer[ENT_OVERRIDE_PIVOT_X] < OUTPUT_get_sprite_pivot_x(entity_pointer[ENT_SPRITE],entity_pointer[ENT_CURRENT_FRAME]))
						{
							entity_pointer[ENT_OVERRIDE_PIVOT_X]++;
						}
						else if (entity_pointer[ENT_OVERRIDE_PIVOT_X] > OUTPUT_get_sprite_pivot_x(entity_pointer[ENT_SPRITE],entity_pointer[ENT_CURRENT_FRAME]))
						{
							entity_pointer[ENT_OVERRIDE_PIVOT_X]--;
						}
						
						if (entity_pointer[ENT_OVERRIDE_PIVOT_Y] < OUTPUT_get_sprite_pivot_y(entity_pointer[ENT_SPRITE],entity_pointer[ENT_CURRENT_FRAME]))
						{
							entity_pointer[ENT_OVERRIDE_PIVOT_Y]++;
						}
						else if (entity_pointer[ENT_OVERRIDE_PIVOT_Y] > OUTPUT_get_sprite_pivot_y(entity_pointer[ENT_SPRITE],entity_pointer[ENT_CURRENT_FRAME]))
						{
							entity_pointer[ENT_OVERRIDE_PIVOT_Y]--;
						}
						break;

					default:
						OUTPUT_message("Incorrect value used as constant when setting pivot position!");
						assert(0);
						break;
					}
				}

				result_i = entity_pointer[ENT_OVERRIDE_PIVOT_X] - start_x;
				entity_pointer[ENT_WORLD_X] += result_i;

				entity_pointer[ENT_UPPER_WORLD_WIDTH] += result_i;
				entity_pointer[ENT_LOWER_WORLD_WIDTH] -= result_i;

				entity_pointer[ENT_X] += (result_i << entity_pointer[ENT_BITSHIFT]);

				result_i = entity_pointer[ENT_OVERRIDE_PIVOT_Y] - start_y;
				entity_pointer[ENT_WORLD_Y] += result_i;

				entity_pointer[ENT_UPPER_WORLD_HEIGHT] += result_i;
				entity_pointer[ENT_LOWER_WORLD_HEIGHT] -= result_i;

				entity_pointer[ENT_Y] += (result_i << entity_pointer[ENT_BITSHIFT]);
			break;

// ---------------- CLOSE DOWN SYSTEM ----------------

			case COM_CLOSE_DOWN_SYSTEM:
				completely_exit_game = true;
			break;

// ----------------  ----------------

			case COM_DUMP_ENTITY_LIST:
				SCRIPTING_state_dump();
				break;

			case COM_GET_MULTI_TEXTURE_STATUS:
				if (best_texture_combiner_available)
				{
					SCRIPTING_put_value ( entity_id , line_number , 1 , RETRENGINE_TRUE );
				}
				else
				{
					SCRIPTING_put_value ( entity_id , line_number , 1 , RETRENGINE_FALSE );
				}
				break;

			case COM_GET_MULTI_TEXTURE_IDEAL_STATUS:
				if (texture_combiner_available)
				{
					SCRIPTING_put_value ( entity_id , line_number , 1 , RETRENGINE_TRUE );
				}
				else
				{
					SCRIPTING_put_value ( entity_id , line_number , 1 , RETRENGINE_FALSE );
				}
				break;

#ifdef RETRENGINE_DEBUG_VERSION_SCRIPT_TRACER

			case COM_ERASE_PROGRAM_TRACE_FILE:
				SCRIPTING_start_tracer_script();
				break;

			case COM_START_PROGRAM_TRACE:
				if (!trace_lines) // In case we hit another start trace while we're already tracing...
				{
					SCRIPTING_add_tracer_script_header (entity_id);
					trace_lines_next_frame = true;
				}
				break;

			case COM_START_COMPLETE_PROGRAM_TRACE:
				if (!trace_lines) // In case we hit another start trace while we're already tracing...
				{
					SCRIPTING_add_tracer_script_header (entity_id);
					trace_lines_next_frame = true;
					complete_trace_from_now_on = true;
				}
				break;

			case COM_START_UNIQUE_PROGRAM_TRACE:
				if ( (unique_program_trace_entity == UNSET) || (unique_program_trace_entity == entity_id) )
				{
					if (!trace_lines) // In case we hit another start trace while we're already tracing...
					{
						SCRIPTING_add_tracer_script_header (entity_id);
						trace_lines_next_frame = true;
						unique_program_trace_entity = entity_id;
					}
				}
				break;

			case COM_DUMP_PROGRAM_TRACE:
				SCRIPTING_output_tracer_script_to_file(true);
				break;

			case COM_STOP_PROGRAM_TRACE:
				trace_lines_next_frame = false;
				SCRIPTING_add_tracer_script_ender(entity_id);
				break;

#endif

			case COM_COUNT_SPAWNPOINTS_IN_ZONE:
				first_value = SCRIPTING_get_int_value(entity_id,line_number,4);
				second_value = SCRIPTING_get_int_value(entity_id,line_number,5);

				result_i = SCRIPTING_count_spawnpoint_in_zone (collision_map,first_value,second_value);

				SCRIPTING_put_value ( entity_id , line_number , 1 , result_i );
				break;

// ---------------- EVENT QUEUES ----------------

			case COM_CREATE_EVENT_QUEUE:
				first_value = SCRIPTING_get_int_value(entity_id,line_number,1);
				EVENT_create_queue (entity_id, first_value);
				break;

			case COM_DESTROY_EVENT_QUEUE:
				EVENT_free_queue (entity_id);
				break;

			case COM_RESET_EVENT_QUEUE:
				EVENT_empty_queue (entity_id);
				break;

			case COM_GET_NEXT_EVENT_FROM_QUEUE:
				first_value = EVENT_get_event_from_queue (entity_id, &second_value);

				SCRIPTING_put_value ( entity_id , line_number , 1 , first_value );
				if (scr[line_number].script_line_size == 5)
				{
					SCRIPTING_put_value ( entity_id , line_number , 2 , second_value );
				}
				break;

			case COM_ADD_EVENT_TO_QUEUE:
				first_value = SCRIPTING_get_int_value(entity_id,line_number,1);
				second_value = SCRIPTING_get_int_value(entity_id,line_number,2);

				EVENT_add_event_to_queue (entity_id,first_value,second_value);
				break;

			case COM_COMPOUND_BITFLAG:
				result_i = 0;
				first_value = scr[line_number].script_line_size - 4;
				// This is the number of bitflags in the line...
				for (parameter_number=0; parameter_number<first_value; parameter_number++)
				{
					result_i |= SCRIPTING_get_int_value(entity_id,line_number,parameter_number+4);
				}

				SCRIPTING_put_value ( entity_id , line_number , 1 , result_i );
				break;

			default:
//				OUTPUT_message ("Unrecognised command!");
//				assert(0);
				break;
			}

		#ifdef RETRENGINE_DEBUG_VERSION_SCRIPT_TRACER
			if (trace_lines)
			{
				SCRIPTING_set_tracer_script_after_values (entity_id, old_line_number);
				SCRIPTING_output_tracer_script_line (old_line_number,line_number,entity_id);
			}
		#endif

		if (trace_lines_next_frame)
		{
			trace_lines = true;
		}
		else
		{
			trace_lines = false;
		}

//		SCRIPTING_check_for_collide_type_offset_mismatch();
		line_number++;
		script_lines_executed++;

		}

		#ifdef RETRENGINE_DEBUG_VERSION_SCRIPT_TRACER
			if (trace_lines)
			{
				SCRIPTING_add_tracer_script_ender(entity_id);
			}
		#endif
	}

	return script_lines_executed;
}



void SCRIPTING_erase_window_and_contents (int window_number)
{



}

static void SCRIPTING_reset_window_draw_queues(void)
{
	int window_number;
	int list_index;

	for (window_number = 0; window_number < number_of_windows; window_number++)
	{
		if (window_details[window_number].y_ordering_list_starts != NULL)
		{
			for (list_index = 0; list_index < window_details[window_number].y_ordering_list_size; list_index++)
			{
				window_details[window_number].y_ordering_list_starts[list_index] = UNSET;
				window_details[window_number].y_ordering_list_ends[list_index] = UNSET;
			}
		}

		if (window_details[window_number].z_ordering_list_starts != NULL)
		{
			for (list_index = 0; list_index < window_details[window_number].z_ordering_list_size; list_index++)
			{
				window_details[window_number].z_ordering_list_starts[list_index] = UNSET;
				window_details[window_number].z_ordering_list_ends[list_index] = UNSET;
			}
		}
	}

	for (list_index = 0; list_index < MAX_ENTITIES; list_index++)
	{
		entity[list_index][ENT_PREV_WINDOW_ENT] = UNSET;
		entity[list_index][ENT_NEXT_WINDOW_ENT] = UNSET;
	}
}



bool SCRIPTING_process_entities (void)
{
//	int process_list[MAX_ENTITIES];
//	int process_counter = 0;

	// ALTER SYSTEM SO ACTIVATED AND DEACTIVATED WINDOWS ONLY CHANGE STATUS
	// AT THE END OF THE FRAME!

	// This loops through all the live entities and processes their scripts and then adds them to the
	// renderqueue.

	SCRIPTING_verify_flags ();
	// Confirm any entity id flags are still valid.
	
	static int fc = -1;
	fc++;
	int fcc=fc;

	SCRIPTING_process_limbo_list();

	int entity_id,next_entity_id;
	int *entity_pointer;
	int bitshift;
	const int list_guard_limit = MAX_ENTITIES + 16;
	int list_iteration_guard = 0;
	bool list_guard_tripped = false;

	char error_message[MAX_LINE_SIZE];

	entity_window_queue_epoch++;
	if (entity_window_queue_epoch <= 0)
	{
		int t;
		entity_window_queue_epoch = 1;
		for (t = 0; t < MAX_ENTITIES; t++)
		{
			entity_window_queue_stamp[t] = 0;
		}
	}

	entity_id = first_processed_entity_in_list;

	// SCRIPTING_reset_collision_stuff();

	total_process_counter = 0;
	drawn_process_counter = 0;
	alive_process_counter = 0;
	script_lines_executed = 0;

	SCRIPTING_set_new_window_status ();
	SCRIPTING_reset_window_draw_queues();

	// Update positions, add to collision buckets and window buckets.

	#ifdef RETRENGINE_DEBUG_VERSION_WHERES_WALLY
	snprintf (wheres_wally, sizeof(wheres_wally), "Just about to move all the entities...");
	#endif

	while (entity_id != UNSET)
	{
		list_iteration_guard++;
		if (list_iteration_guard > list_guard_limit)
		{
			SCRIPTING_log_loop_guard_trip("move_entities", first_processed_entity_in_list, entity_id, list_iteration_guard);
			list_guard_tripped = true;
			break;
		}

		entity_pointer = &entity[entity_id][0];

		global_next_entity = entity_pointer [ENT_NEXT_PROCESS_ENT];
		// We use the global next entity just in case the entity pops it's clogs as the result
		// of hitting something.

		// We check that the entity hasn't just been created, as if it has we could
		// easily overwrite it's WORLD_X and WORLD_Y because it won't have set it's
		// own private-scale co-ords yet in the script.
		if (entity_pointer[ENT_ALIVE] == ALIVE)
		{
			// Update world position...

			bitshift = entity_pointer[ENT_BITSHIFT];

//			SCRIPTING_check_for_collide_type_offset_mismatch();

//			entity_pointer[ENT_OLD_WORLD_X] = entity_pointer[ENT_WORLD_X];
//			entity_pointer[ENT_OLD_WORLD_Y] = entity_pointer[ENT_WORLD_Y];

			if (entity_pointer[ENT_WORLD_COLLISION_BEHAVIOUR] != 0)
			{
				WORLDCOLL_push_entity (entity_pointer);
			}
			else
			{
				entity_pointer[ENT_X] += (entity_pointer[ENT_X_VEL] + entity_pointer[ENT_EXTERNAL_CONVEY_X]);
				entity_pointer[ENT_Y] += (entity_pointer[ENT_Y_VEL] + entity_pointer[ENT_EXTERNAL_CONVEY_Y]);

				#ifdef RETRENGINE_DEBUG_VERSION_FORGOTTEN_SET_PRIVATE
					if ( (entity_pointer[ENT_WORLD_X] != 0) && (entity_pointer[ENT_X] == 0) && (entity_pointer[ENT_WORLD_Y] != 0) && (entity_pointer[ENT_Y] == 0) )
					{
						snprintf (error_message, sizeof(error_message), "Forgotten SET_PRIVATE_CO_ORDS in entity %i (%s)?", entity_id, GPL_get_entry_name("SCRIPTS", entity[entity_id][ENT_SCRIPT_NUMBER]));
						OUTPUT_message(error_message);
					}
				#endif

				entity_pointer[ENT_WORLD_X] = entity_pointer[ENT_X] >> bitshift;
				entity_pointer[ENT_WORLD_Y] = entity_pointer[ENT_Y] >> bitshift;
			}

			entity_pointer[ENT_X_VEL] += entity_pointer[ENT_X_ACC] + entity_pointer[ENT_EXTERNAL_ACCELL_X];
			entity_pointer[ENT_Y_VEL] += entity_pointer[ENT_Y_ACC] + entity_pointer[ENT_EXTERNAL_ACCELL_Y];

			entity_pointer[ENT_OPENGL_ANGLE] += entity_pointer[ENT_ANGULAR_VEL];
		}
		else if (entity_pointer[ENT_ALIVE] == JUST_BORN)
		{
			// If it was JUST_BORN then change it to the regular ALIVE.
			entity_pointer[ENT_ALIVE] = ALIVE;
		}

		entity_id = global_next_entity;
	}
	if (list_guard_tripped)
	{
		return true;
	}

	// Go through the entities again, only this time purely for script processing.
	#ifdef RETRENGINE_DEBUG_VERSION_WHERES_WALLY
						snprintf (wheres_wally, sizeof(wheres_wally), "Now I'm doing the scripting and adding to windows...");
	#endif

	entity_id = first_processed_entity_in_list;
	list_iteration_guard = 0;

	int window_number;

	int previous_global_end;
	int previous_entity;

	while (entity_id != UNSET)
	{
		list_iteration_guard++;
		if (list_iteration_guard > list_guard_limit)
		{
			SCRIPTING_log_loop_guard_trip("script_entities", first_processed_entity_in_list, entity_id, list_iteration_guard);
			list_guard_tripped = true;
			break;
		}

		entity_pointer = &entity[entity_id][0];

		global_next_entity = entity_pointer [ENT_NEXT_PROCESS_ENT];
		// This value can be overridden in the script handler, although this only occurs
		// when an entity deletes the one following it and therefor needs to skip over it.

		previous_global_end = global_next_entity; // This is a backup of it, in case it changes

		if (entity_pointer[ENT_ALIVE] == ALIVE)
		{
			alive_process_counter++;
			#ifdef RETRENGINE_DEBUG_VERSION_WHERES_WALLY
			snprintf (wheres_wally, sizeof(wheres_wally), "I'm about to script entity %i", entity_id);
			#endif

			script_lines_executed += SCRIPTING_interpret_script (entity_id,UNSET); // Do the real business stuff.
			
			#ifdef RETRENGINE_DEBUG_VERSION_WHERES_WALLY
			snprintf (wheres_wally, sizeof(wheres_wally), "I've scripted entity %i", entity_id);
			#endif
		}
		else
		{
			if (entity_pointer[ENT_ALIVE] <= DEAD)
			{
				#ifdef RETRENGINE_DEBUG_VERSION_WHERES_WALLY
				snprintf (wheres_wally, sizeof(wheres_wally), "There's a problem with entity %i so I'm saving the demo.", entity_id);
				#endif

				CONTROL_stop_and_save_active_channels ("limbo_error");
				OUTPUT_message("LIMBOED ENTITY IN SCRIPT PROCESSING LOOP!");
				assert(0);
			}
		}

		if ( (entity_pointer[ENT_DRAW_ORDER] != UNSET) && (entity_pointer[ENT_DRAW_MODE] > DRAW_MODE_INVISIBLE) && (entity_pointer[ENT_ALIVE] > JUST_BORN) && (entity_pointer[ENT_ALIVE] != SLEEPING) )
		{
			drawn_process_counter++;

			window_number = entity_pointer[ENT_WINDOW_NUMBER];

			#ifdef RETRENGINE_DEBUG_VERSION_WHERES_WALLY
			snprintf (wheres_wally, sizeof(wheres_wally), "I'm adding entity %i to window %i.", entity_id, window_number);
			#endif

			#ifdef RETRENGINE_DEBUG_VERSION_ENTITY_DEBUG_FLAG_CHECKS
			if (entity_pointer[ENT_DEBUG_FLAGS] & DEBUG_FLAG_STOP_WHEN_TESTED_FOR_DRAWING)
			{
				assert(0);
			}
			#endif

			if ( (window_number != UNSET) && (window_details[window_number].active == true) )
			{
//				process_list[process_counter] = entity_id;
//				process_list[process_counter+1] = UNSET;
//				process_counter++;

				SCRIPTING_add_entity_to_window (entity_id);
			}

			#ifdef RETRENGINE_DEBUG_VERSION_WHERES_WALLY
			snprintf (wheres_wally, sizeof(wheres_wally), "I've added entity %i to window %i successfully.", entity_id, window_number);
			#endif
		}

		total_process_counter++;

		previous_entity = entity_id;
		entity_id = global_next_entity;
	}
	if (list_guard_tripped)
	{
		return true;
	}

	global_next_entity = UNSET;

	// Then move all the window y-queues to the z-queues. This puts them in a handy place if we need to pop something out of
	// them during the obcoll phase.

	#ifdef RETRENGINE_DEBUG_VERSION_WHERES_WALLY
	snprintf (wheres_wally, sizeof(wheres_wally), "Moving the queues from Y to Z...");
	#endif

	for (window_number=0; window_number<number_of_windows; window_number++)
	{
		if (window_details[window_number].active)
		{
			SCRIPTING_move_window_y_queue_to_z_queue (window_number);
		}
	}

	SCRIPTING_update_window_positions ();

	// We only close down the previous frame's collision buffers here as they might want to be used
	// during the scripting to target entities.

	OBCOLL_close_down_collision_buckets ();

	// Now the buckets are all shut down, we're killing off all the object bucket numbers in the entities.
	for (entity_id=0; entity_id<MAX_ENTITIES; entity_id++)
	{
		entity[entity_id][ENT_COLLISION_BUCKET_NUMBER] = UNSET;
	}

	// That should purge any dangerous old manky stuff.

	// And now we can update the window, update the buffer and open up the buffers again.
	OBCOLL_update_collision_window ();
	OBCOLL_update_buffer();
	OBCOLL_open_up_collision_buckets();
	// We then stuff all the objects into the collision buffer in their correct locations.

	entity_id = first_processed_entity_in_list;
	list_iteration_guard = 0;

	while (entity_id != UNSET)
	{
		list_iteration_guard++;
		if (list_iteration_guard > list_guard_limit)
		{
			SCRIPTING_log_loop_guard_trip("collision_bucket_fill", first_processed_entity_in_list, entity_id, list_iteration_guard);
			list_guard_tripped = true;
			break;
		}

		entity_pointer = &entity[entity_id][0];

		next_entity_id = entity_pointer [ENT_NEXT_PROCESS_ENT];

		if ( (entity_pointer[ENT_ALIVE] == ALIVE) || (entity_pointer[ENT_ALIVE] == JUST_BORN) )
		{
			OBCOLL_remove_from_general_area_bucket (entity_id);
			OBCOLL_add_to_general_area_bucket (entity_id);
		}

		if ( (entity_pointer[ENT_ALIVE] > DEAD) && (entity_pointer[ENT_ALIVE] < SLEEPING) )
		{
			OBCOLL_add_to_collision_bucket (entity_id);
		}

		entity_id = next_entity_id;
	}
	if (list_guard_tripped)
	{
		return true;
	}

	#ifdef RETRENGINE_DEBUG_VERSION_WHERES_WALLY
	snprintf (wheres_wally, sizeof(wheres_wally), "Calling the collision handler...");
	#endif

//	SCRIPTING_confirm_entity_counts();
	not_in_collision_phase = false;
	OBCOLL_collision_handler ();
	not_in_collision_phase = true;

//	SCRIPTING_confirm_entity_counts();

	if (output_debug_information)
	{
		int focus_wizball = 0;
		int focus_controller = 0;
		int focus_game_window = 0;
		int focus_get_ready = 0;
		int focus_choose_start = 0;
		int focus_start_game = 0;
		int focus_wizball_drawable = 0;
		int focus_controller_drawable = 0;
		int focus_game_window_drawable = 0;
		int focus_get_ready_drawable = 0;
		int focus_choose_start_drawable = 0;
		int focus_start_game_drawable = 0;
		int first_wizball_entity = UNSET;
		int first_wizball_alive = UNSET;
		int first_wizball_draw_mode = UNSET;
		int first_wizball_draw_order = UNSET;
		int first_wizball_window = UNSET;
		int first_wizball_world_x = UNSET;
		int first_wizball_world_y = UNSET;
		int first_wizball_draw_override = UNSET;
		int first_wizball_window_tl_x = UNSET;
		int first_wizball_window_tl_y = UNSET;
		int first_wizball_window_br_x = UNSET;
		int first_wizball_window_br_y = UNSET;

		entity_id = first_processed_entity_in_list;
		list_iteration_guard = 0;
		while (entity_id != UNSET)
		{
			list_iteration_guard++;
			if (list_iteration_guard > list_guard_limit)
			{
				SCRIPTING_log_loop_guard_trip("focus_scan", first_processed_entity_in_list, entity_id, list_iteration_guard);
				list_guard_tripped = true;
				break;
			}

			entity_pointer = &entity[entity_id][0];
			if (entity_pointer[ENT_ALIVE] >= JUST_BORN)
			{
				int script_number = entity_pointer[ENT_SCRIPT_NUMBER];
				if ((script_number >= 0) && (script_number < script_count))
				{
					const char *name = GPL_get_entry_name("SCRIPTS", script_number);
					bool drawable = false;
					if ((entity_pointer[ENT_DRAW_ORDER] != UNSET) &&
						(entity_pointer[ENT_DRAW_MODE] > DRAW_MODE_INVISIBLE) &&
						(entity_pointer[ENT_ALIVE] > JUST_BORN) &&
						(entity_pointer[ENT_ALIVE] != SLEEPING))
					{
						int window_number = entity_pointer[ENT_WINDOW_NUMBER];
						if ((window_number != UNSET) && (window_details[window_number].active == true))
						{
							drawable = true;
						}
					}

					if (name != NULL)
					{
						if (strcmp(name, "WIZBALL") == 0)
						{
							focus_wizball++;
							if (drawable) focus_wizball_drawable++;
							if (first_wizball_entity == UNSET)
							{
								first_wizball_entity = entity_id;
								first_wizball_alive = entity_pointer[ENT_ALIVE];
								first_wizball_draw_mode = entity_pointer[ENT_DRAW_MODE];
								first_wizball_draw_order = entity_pointer[ENT_DRAW_ORDER];
								first_wizball_window = entity_pointer[ENT_WINDOW_NUMBER];
								first_wizball_world_x = entity_pointer[ENT_WORLD_X];
								first_wizball_world_y = entity_pointer[ENT_WORLD_Y];
								first_wizball_draw_override = entity_pointer[ENT_DRAW_OVERRIDE];
								if ((first_wizball_window >= 0) && (first_wizball_window < number_of_windows))
								{
									first_wizball_window_tl_x = window_details[first_wizball_window].buffered_tl_x;
									first_wizball_window_tl_y = window_details[first_wizball_window].buffered_tl_y;
									first_wizball_window_br_x = window_details[first_wizball_window].buffered_br_x;
									first_wizball_window_br_y = window_details[first_wizball_window].buffered_br_y;
								}
							}
						}
						else if (strcmp(name, "MAIN_GAME_CONTROLLER") == 0)
						{
							focus_controller++;
							if (drawable) focus_controller_drawable++;
						}
						else if (strcmp(name, "GAME_WINDOW_HANDLER") == 0)
						{
							focus_game_window++;
							if (drawable) focus_game_window_drawable++;
						}
						else if (strcmp(name, "GET_READY_SCREEN") == 0)
						{
							focus_get_ready++;
							if (drawable) focus_get_ready_drawable++;
						}
						else if (strcmp(name, "CHOOSE_WIZBALL_START_POSITION") == 0)
						{
							focus_choose_start++;
							if (drawable) focus_choose_start_drawable++;
						}
						else if (strcmp(name, "START_GAME") == 0)
						{
							focus_start_game++;
							if (drawable) focus_start_game_drawable++;
						}
					}
				}
			}
			entity_id = entity_pointer[ENT_NEXT_PROCESS_ENT];
		}
		if (list_guard_tripped)
		{
			return true;
		}

		static int last_focus_wizball = -1;
		static int last_focus_controller = -1;
		static int last_focus_game_window = -1;
		static int last_focus_get_ready = -1;
		static int last_focus_choose_start = -1;
		static int last_focus_start_game = -1;
		static int last_focus_wizball_drawable = -1;
		static int last_focus_controller_drawable = -1;
		static int last_focus_game_window_drawable = -1;
		static int last_focus_get_ready_drawable = -1;
		static int last_focus_choose_start_drawable = -1;
		static int last_focus_start_game_drawable = -1;
		static int last_first_wizball_entity = -2;
		static int last_first_wizball_alive = -2;
		static int last_first_wizball_draw_mode = -2;
		static int last_first_wizball_draw_order = -2;
		static int last_first_wizball_window = -2;
		static int last_first_wizball_world_x = -2;
		static int last_first_wizball_world_y = -2;
		static int last_first_wizball_draw_override = -2;
		static int last_first_wizball_window_tl_x = -2;
		static int last_first_wizball_window_tl_y = -2;
		static int last_first_wizball_window_br_x = -2;
		static int last_first_wizball_window_br_y = -2;
		if ((focus_wizball != last_focus_wizball) ||
			(focus_controller != last_focus_controller) ||
			(focus_game_window != last_focus_game_window) ||
			(focus_get_ready != last_focus_get_ready) ||
			(focus_choose_start != last_focus_choose_start) ||
			(focus_start_game != last_focus_start_game) ||
			(focus_wizball_drawable != last_focus_wizball_drawable) ||
			(focus_controller_drawable != last_focus_controller_drawable) ||
			(focus_game_window_drawable != last_focus_game_window_drawable) ||
			(focus_get_ready_drawable != last_focus_get_ready_drawable) ||
			(focus_choose_start_drawable != last_focus_choose_start_drawable) ||
			(focus_start_game_drawable != last_focus_start_game_drawable) ||
			(first_wizball_entity != last_first_wizball_entity) ||
			(first_wizball_alive != last_first_wizball_alive) ||
			(first_wizball_draw_mode != last_first_wizball_draw_mode) ||
			(first_wizball_draw_order != last_first_wizball_draw_order) ||
			(first_wizball_window != last_first_wizball_window) ||
			(first_wizball_world_x != last_first_wizball_world_x) ||
			(first_wizball_world_y != last_first_wizball_world_y) ||
			(first_wizball_draw_override != last_first_wizball_draw_override) ||
			(first_wizball_window_tl_x != last_first_wizball_window_tl_x) ||
			(first_wizball_window_tl_y != last_first_wizball_window_tl_y) ||
			(first_wizball_window_br_x != last_first_wizball_window_br_x) ||
			(first_wizball_window_br_y != last_first_wizball_window_br_y))
		{
			char line[MAX_LINE_SIZE];
						snprintf(line, sizeof(line),
				"FOCUS_COUNTS frame=%d WIZBALL=%d/%d MAIN_GAME_CONTROLLER=%d/%d GAME_WINDOW_HANDLER=%d/%d GET_READY_SCREEN=%d/%d CHOOSE_WIZBALL_START_POSITION=%d/%d START_GAME=%d/%d WIZBALL_STATE id=%d alive=%d draw_mode=%d draw_order=%d window=%d wx=%d wy=%d override=%d win_tl=(%d,%d) win_br=(%d,%d)",
				frames_so_far,
				focus_wizball,
				focus_wizball_drawable,
				focus_controller,
				focus_controller_drawable,
				focus_game_window,
				focus_game_window_drawable,
				focus_get_ready,
					focus_get_ready_drawable,
					focus_choose_start,
					focus_choose_start_drawable,
					focus_start_game,
					focus_start_game_drawable,
				first_wizball_entity,
				first_wizball_alive,
				first_wizball_draw_mode,
				first_wizball_draw_order,
				first_wizball_window,
				first_wizball_world_x,
				first_wizball_world_y,
				first_wizball_draw_override,
				first_wizball_window_tl_x,
				first_wizball_window_tl_y,
				first_wizball_window_br_x,
				first_wizball_window_br_y);
			MAIN_add_to_log(line);

			last_focus_wizball = focus_wizball;
			last_focus_controller = focus_controller;
			last_focus_game_window = focus_game_window;
			last_focus_get_ready = focus_get_ready;
			last_focus_choose_start = focus_choose_start;
			last_focus_start_game = focus_start_game;
			last_focus_wizball_drawable = focus_wizball_drawable;
			last_focus_controller_drawable = focus_controller_drawable;
			last_focus_game_window_drawable = focus_game_window_drawable;
			last_focus_get_ready_drawable = focus_get_ready_drawable;
			last_focus_choose_start_drawable = focus_choose_start_drawable;
			last_focus_start_game_drawable = focus_start_game_drawable;
			last_first_wizball_entity = first_wizball_entity;
			last_first_wizball_alive = first_wizball_alive;
			last_first_wizball_draw_mode = first_wizball_draw_mode;
			last_first_wizball_draw_order = first_wizball_draw_order;
			last_first_wizball_window = first_wizball_window;
			last_first_wizball_world_x = first_wizball_world_x;
			last_first_wizball_world_y = first_wizball_world_y;
			last_first_wizball_draw_override = first_wizball_draw_override;
			last_first_wizball_window_tl_x = first_wizball_window_tl_x;
			last_first_wizball_window_tl_y = first_wizball_window_tl_y;
			last_first_wizball_window_br_x = first_wizball_window_br_x;
			last_first_wizball_window_br_y = first_wizball_window_br_y;
		}
	}

	return completely_exit_game;
}



int SCRIPTING_draw_all_windows (void)
{
	int w;

	int total_entities_drawn = 0;


	for (w=0; w<number_of_windows; w++)
	{
		if (window_details[w].active == true)
		{
			total_entities_drawn += OUTPUT_draw_window_contents (w, texture_combiner_available);

			#ifdef RETRENGINE_DEBUG_VERSION_VIEW_OBJECT_COLLISION
				OUTPUT_draw_window_collision_contents (w,false);
			#endif
			#ifdef RETRENGINE_DEBUG_VERSION_VIEW_WORLD_COLLISION
				OUTPUT_draw_window_collision_contents (w,true);
			#endif
		}
	}

	#ifdef RETRENGINE_DEBUG_VERSION_VIEW_OBJECT_COLLISION_BUCKETS
		OBCOLL_draw_collision_buckets ();
	#endif

	return total_entities_drawn;
}



bool SCRIPTING_check_for_visual_params (int tilemap_number, int spawn_point_number, int *bitmap_number, int *base_frame, int *frame_count, int *frame_delay, int *frame_end_delay, int *path_number, int *path_angle)
{
	// Looks through the params you're passing to a script to see if there's anything which could
	// be used to show the spawn point as something other than a boring circle. Very helpful for placing
	// enemies.

	bool found_bitmap = false;
	bool found_base_frame = false;

	int script_index = cm[tilemap_number].spawn_point_list_pointer[spawn_point_number].script_index;
	int line_number = scr_lookup[script_index];
	int end_line_number = scr_lookup[script_index+1];
	
	int instruction;
	int variable;
	int data_value;
	int data_type;
	int data_bitmask;

	bool ignore_value;

	do
	{
		instruction = scr[line_number].script_line_pointer[scr[line_number].command_instruction_index].data_value;

		if (instruction == COM_LET)
		{
			variable = scr[line_number].script_line_pointer[1].data_value;

			data_type = scr[line_number].script_line_pointer[3].data_type;
			data_value = scr[line_number].script_line_pointer[3].data_value;
			data_bitmask = scr[line_number].script_line_pointer[3].data_bitmask;

			// Only allow literal values.
			if ((data_bitmask & DATA_BITMASK_IDENTITY_VARIABLE) > 0)
			{
				ignore_value = true;
			}
			else if (data_type == VARIABLE)
			{
				ignore_value = true;
			}
			else
			{
				ignore_value = false;
			}

			if ( (instruction == COM_LET) && (!ignore_value) )
			{
				switch (variable)
				{
				case ENT_SPRITE:
					*bitmap_number = data_value;
					found_bitmap = true;
					break;

				case ENT_BASE_FRAME:
					*base_frame = data_value;
					found_base_frame = true;
					break;

				case ENT_FRAME_COUNT:
					*frame_count = data_value;
					break;

				case ENT_FRAME_DELAY:
					*frame_delay = data_value;
					break;

				case ENT_FRAME_END_DELAY:
					*frame_end_delay = data_value;
					break;

				case ENT_PATH_NUMBER:
					*path_number = data_value;
					break;

				case ENT_PATH_ANGLE:
					*path_angle = data_value;
					break;
				}
			}
		}

		line_number++;

	} while ( (instruction != COM_START) && (line_number < end_line_number) );

	if ( (found_bitmap == true) && (found_base_frame == true) )
	{
		return true;
	}
	else
	{
		return false;
	}
	
}








/*
void SCRIPTING_check_for_visual_params (int script_number, int *bitmap_number, int *base_frame, int *frame_count, int *frame_delay, int *frame_end_delay, int *path_number, int *path_angle, int *path_base_speed)
{
	// This is called by the SPAWNPOINTS routine of the same name and will overwrite the settings from there
	// with those in the actual script as obviously they're more important. However it's called only half the
	// time (every other second).

	
}
*/


// Different draw order methods. Okay, the main problem is that if we have a really tall level it means
// looking through 10,000 odd lists which is a lot of stuff to trawl through. So instead it seems logical
// to only bother looking through those lines where the data needs transferring to a bin because the others
// aren't going to be drawn, although again we still have to blank those lists afterwards... Ugh.

// I don't think I can avoid a LOT of reading and writing to arrays.

// We need a collision array which is the size of the game divided by the collision resolution.

// Well, maybe what we should do is define a series of rectangles which are the viewports and then
// each frame set those areas of the arrays as RESET_WRITEABLE so data can be added to them. However
// at the end of the frame we reset all of them to RESET_LOCKED so that we only need to clean up the
// part of the array we've used. Then when something is added to a list it just calls a routine that
// only bothers adding it when it's listed as writeable.

// However the collision aspect of this can be overwritten by a global "OFF_SCREEN_COLLISION" flag which
// means the whole caboodle is done every frame. Also there should be a series of values for each type and axis
// so that we can define how far outside of the screen it works by default anyway.




void SCRIPTING_set_default_entity_from_prefab (int prefab_number)
{
	int t;

	if ((prefab_number < 0) || (prefab_number >= number_of_prefabs_loaded) || (prefab_space == NULL))
	{
		return;
	}

	int preset_count = prefab_space[prefab_number].presets;
	if (preset_count <= 0 || prefab_space[prefab_number].variables == NULL || prefab_space[prefab_number].values == NULL)
	{
		return;
	}

	for (t=0; t<preset_count; t++)
	{
		int variable_index = prefab_space[prefab_number].variables[t];
		if (variable_index < 0 || variable_index >= MAX_ENTITY_VARIABLES)
		{
			continue;
		}

		reset_entity[variable_index] = prefab_space[prefab_number].values[t];
	}
}



void SCRIPTING_use_prefab (int prefab_number , int *entity_pointer)
{
	int t;

	if ((prefab_number < 0) || (prefab_number >= number_of_prefabs_loaded) || (prefab_space == NULL) || (entity_pointer == NULL))
	{
		return;
	}

	int preset_count = prefab_space[prefab_number].presets;
	if (preset_count <= 0 || prefab_space[prefab_number].variables == NULL || prefab_space[prefab_number].values == NULL)
	{
		return;
	}

	for (t=0; t<preset_count; t++)
	{
		int variable_index = prefab_space[prefab_number].variables[t];
		if (variable_index < 0 || variable_index >= MAX_ENTITY_VARIABLES)
		{
			continue;
		}

		entity_pointer[variable_index] = prefab_space[prefab_number].values[t];
	}
}



void SCRIPTING_free_prefabs (void)
{
	int p;

	for (p=0; p<number_of_prefabs_loaded; p++)
	{
		if (prefab_space[p].variables != NULL)
		{
			free (prefab_space[p].variables);
		}

		if (prefab_space[p].values != NULL)
		{
			free (prefab_space[p].values);
		}
	}

	free (prefab_space);

	number_of_prefabs_loaded = 0;
}



void SCRIPTING_load_prefab (char *filename, int index)
{
	int counter = 0;
	int variables[MAX_ENTITY_VARIABLES];
	int values[MAX_ENTITY_VARIABLES];

	bool exitmainloop = false;
	bool start_grabbing = false;

	char line[MAX_LINE_SIZE];
	char *variable_pointer = NULL;
	char *value_pointer = NULL;

	int t;

	FILE *file_pointer;

	file_pointer = FILE_open_project_read_case_fallback(filename);

	if (file_pointer != NULL)
	{
		// Righty, what we do it go through the file until we find the "#START"

		while ( ( fgets ( line , MAX_LINE_SIZE , file_pointer ) != NULL ) && (exitmainloop == false) )
		{
			if ( STRING_instr_word ( line , "#START" , 0 ) != UNSET )
			{
				start_grabbing = true;
			}
			else if (start_grabbing)
			{
				if ( STRING_instr_word ( line , "#END" , 0 ) != UNSET )
				{
					exitmainloop = true;
				}
				else
				{
					// It's a line! Woot!

					variable_pointer = strtok(line," \r\t\n|");
					value_pointer = strtok(NULL,",\r\t\n|");

					if (value_pointer != NULL)
					{
						// Ie, the variable is accompanied by a value...

						variables[counter] = GPL_find_word_value("VARIABLE",variable_pointer);

						values[counter] = 0;

						do
						{
							if (STRING_is_it_a_number (value_pointer))
							{
								values[counter] += atoi(value_pointer);
							}
							else
							{
								values[counter] += GPL_find_word_value("CONSTANT",value_pointer);
							}

							value_pointer = strtok(NULL,",\r\t\n|");

						} while (value_pointer != NULL);

						counter++;
					}
				}
			}
		}

		fclose(file_pointer);

		// Okay, it's all read in...

		if (counter>0)
		{
			prefab_space[index].presets = counter;
			prefab_space[index].variables = (int *) malloc (sizeof(int) * counter);
			prefab_space[index].values = (int *) malloc (sizeof(int) * counter);

			for (t=0; t<counter; t++)
			{
				prefab_space[index].variables[t] = variables[t];
				prefab_space[index].values[t] = values[t];
			}
		}
		else
		{
			prefab_space[index].presets = 0;
			prefab_space[index].variables = NULL;
			prefab_space[index].values = NULL;
		}

	}
	else
	{
		// Keep running even if a prefab source file is missing on this platform.
		prefab_space[index].presets = 0;
		prefab_space[index].variables = NULL;
		prefab_space[index].values = NULL;

		MAIN_add_to_log ("Missing prefab file; using empty prefab.");
	}
}



void SCRIPTING_load_prefabs (void)
{
	bool flag = true;
	char filename[MAX_NAME_SIZE];
	char full_filename[MAX_NAME_SIZE];

	int list_start,list_end;
	int list_pointer;

	GPL_list_extents ("PREFABS", &list_start, &list_end);
	char *extension_pointer = GPL_what_is_list_extension ("PREFABS");

	int list_size = list_end - list_start;

	prefab_space = (prefab_struct *) malloc (sizeof(prefab_struct) * list_size);

	for (list_pointer = list_start; list_pointer < list_end ; list_pointer++)
	{
		snprintf (filename, sizeof(filename), "%s%s", GPL_what_is_word_name(list_pointer), extension_pointer );

		// Okay, check the file to see if it contains the "-arb" or "-set"
		FILE_append_filename(full_filename, "prefabs", filename, sizeof(full_filename) );

		SCRIPTING_load_prefab (full_filename,list_pointer-list_start);
	}

	number_of_prefabs_loaded = list_size;
}



void SCRIPTING_save_compiled_prefabs (void)
{
	int prefab_number;

	FILE *file_pointer = fopen(MAIN_get_project_filename("prefabs.dat"),"wb");

	if (file_pointer)
	{
		fwrite (&number_of_prefabs_loaded,sizeof(int),1,file_pointer);

		fwrite (prefab_space,sizeof(prefab_struct)*number_of_prefabs_loaded,1,file_pointer);

		for (prefab_number=0; prefab_number<number_of_prefabs_loaded; prefab_number++)
		{
			fwrite (prefab_space[prefab_number].variables , sizeof(int) * prefab_space[prefab_number].presets, 1 , file_pointer);
			fwrite (prefab_space[prefab_number].values , sizeof(int) * prefab_space[prefab_number].presets, 1 , file_pointer);
		}

		fclose (file_pointer);
	}
	else
	{
		assert(0);
	}
}



void SCRIPTING_load_compiled_prefabs (void)
{
	int prefab_number, preset_number;

	FILE *file_pointer = FILE_open_project_case_fallback("prefabs.dat", "rb");

	if (file_pointer)
	{
		fread (&number_of_prefabs_loaded,sizeof(int),1,file_pointer);
#ifdef ALLEGRO_MACOSX
	number_of_prefabs_loaded = EndianS32_LtoN(number_of_prefabs_loaded);
#endif
		prefab_space = (prefab_struct *) malloc (sizeof(prefab_struct) * number_of_prefabs_loaded);

		fread (prefab_space,sizeof(prefab_struct),number_of_prefabs_loaded,file_pointer);
#ifdef ALLEGRO_MACOSX
		for (prefab_number = 0; prefab_number < number_of_prefabs_loaded; prefab_number++)
		{
			prefab_space[prefab_number].presets = EndianS32_LtoN(prefab_space[prefab_number].presets);
		}
#endif
		for (prefab_number=0; prefab_number<number_of_prefabs_loaded; prefab_number++)
		{
			prefab_space[prefab_number].variables = (int *) malloc (sizeof(int) * prefab_space[prefab_number].presets);
			prefab_space[prefab_number].values = (int *) malloc (sizeof(int) * prefab_space[prefab_number].presets);

			fread (prefab_space[prefab_number].variables , sizeof(int), prefab_space[prefab_number].presets, file_pointer);
			fread (prefab_space[prefab_number].values , sizeof(int), prefab_space[prefab_number].presets, file_pointer);
#ifdef ALLEGRO_MACOSX
		for (preset_number = 0; preset_number < prefab_space[prefab_number].presets; preset_number++)
		{
			prefab_space[prefab_number].variables[preset_number] = EndianS32_LtoN(prefab_space[prefab_number].variables[preset_number]);
			prefab_space[prefab_number].values[preset_number] = EndianS32_LtoN(prefab_space[prefab_number].values[preset_number]);
		}
#endif
		}

		fclose (file_pointer);
	}
	else
	{
		assert(0);
	}
}



void SCRIPTING_rotate_entities_and_range_compress (int first_entity_id, int first_z, int end_z, int rotate_angle_x, int rotate_angle_y, int rotate_angle_z, int transform_x, int transform_y, int transform_z, int centre_x, int centre_y, int depth_of_field)
{




}




#define SELECT_BY_EXACT_BOOLEAN_MATCH			(0)
#define SELECT_BY_COMPLETE_BOOLEAN_MATCH		(1)
#define SELECT_BY_PARTIAL_BOOLEAN_MATCH			(2)
#define SELECT_BY_EXACT_VALUE_MATCH				(3)
#define SELECT_BY_EXACT_OR_LOWER_VALUE_MATCH	(4)
#define SELECT_BY_EXACT_OR_HIGHER_VALUE_MATCH	(5)
#define SELECT_BY_RELATIVE_ANGLE				(6)
#define SELECT_BY_GREATER_THAN_DISTANCE			(7)



int SCRIPTING_create_target_list (int *entity_pointer, int comparison_type, int comparative_variable, int comparative_value, int max_distance, int cone_size = UNSET)
{
	bool result;

	int cone_x_1,cone_y_1;
	int cone_x_2,cone_y_2;

	int cross_product_1;
	int cross_product_2;

	int angle_1;
	int angle_2;

	int other_entity_id;
	int *other_entity_pointer;

	int dx,dy;

	int distance;

	if (comparison_type == SELECT_BY_GREATER_THAN_DISTANCE)
	{
		comparative_value *= comparative_value;
		// As its a distance and we ain't square-rooting.
	}
	else if (comparison_type == SELECT_BY_RELATIVE_ANGLE)
	{
		angle_1 = comparative_value - cone_size;
		angle_2 = comparative_value + cone_size;

		cone_x_1 = (100 * MATH_get_cos_table_value (angle_1)) / 10000;
		cone_y_1 = (100 * MATH_get_sin_table_value (angle_1)) / 10000;

		cone_x_2 = (100 * MATH_get_cos_table_value (angle_2)) / 10000;
		cone_y_2 = (100 * MATH_get_sin_table_value (angle_2)) / 10000;
	}

	int start_box_x,end_box_x;
	int start_box_y,end_box_y;

	int counter = 0;

	start_box_x = (entity_pointer[ENT_WORLD_X] - max_distance) >> general_area_bitshift;
	end_box_x = (entity_pointer[ENT_WORLD_X] + max_distance) >> general_area_bitshift;

	start_box_y = (entity_pointer[ENT_WORLD_Y] - max_distance) >> general_area_bitshift;
	end_box_y = (entity_pointer[ENT_WORLD_Y] + max_distance) >> general_area_bitshift;

	int bx,by;
	int last_entity_on_list = UNSET;

	for (bx=start_box_x; bx<=end_box_x; bx++)
	{
		for (by=start_box_y; by<=end_box_y; by++)
		{
			other_entity_id = OBCOLL_get_first_in_bucket (bx,by);
			
			while (other_entity_id != UNSET)
			{
				other_entity_pointer = &entity[other_entity_id][0];
				
				dx = other_entity_pointer[ENT_WORLD_X] - entity_pointer[ENT_WORLD_X];
				dy = other_entity_pointer[ENT_WORLD_Y] - entity_pointer[ENT_WORLD_Y];

				distance = (dx*dx)+(dy*dy);

				if (distance < max_distance)
				{

					switch (comparison_type)
					{
					case SELECT_BY_EXACT_BOOLEAN_MATCH:
						result = (comparative_value == entity_pointer[comparative_variable]);
						break;

					case SELECT_BY_COMPLETE_BOOLEAN_MATCH:
						result = ((comparative_value && entity_pointer[comparative_variable]) == comparative_value);
						break;

					case SELECT_BY_PARTIAL_BOOLEAN_MATCH:
						result = (comparative_value && entity_pointer[comparative_variable]);
						break;

					case SELECT_BY_EXACT_VALUE_MATCH:
						result = (comparative_value == entity_pointer[comparative_variable]);
						break;

					case SELECT_BY_EXACT_OR_LOWER_VALUE_MATCH:
						result = (comparative_value >= entity_pointer[comparative_variable]);
						break;

					case SELECT_BY_EXACT_OR_HIGHER_VALUE_MATCH:
						result = (comparative_value <= entity_pointer[comparative_variable]);
						break;

					case SELECT_BY_RELATIVE_ANGLE:
						cross_product_1 = (cone_x_1 * dy) - (dx * cone_y_1);
						cross_product_2 = (cone_x_2 * dy) - (dx * cone_y_2);
						
						if ( (cross_product_1 > 0) && (cross_product_2 < 0) )
						{
							assert(0);
						}
						break;
						
					case SELECT_BY_GREATER_THAN_DISTANCE:
						result = (distance > comparative_value);
						break;

					default:
						break;
					}
				}

				if (result)
				{
					counter++;

					other_entity_pointer[ENT_DISTANCE_FROM_TARGETTER] = distance;
					other_entity_pointer[ENT_NEXT_TARGET_IN_LIST] = UNSET;

					if (last_entity_on_list == UNSET)
					{
						first_entity_in_target_list = other_entity_id;
						other_entity_pointer[ENT_PREV_TARGET_IN_LIST] = UNSET;
					}
					else
					{
						other_entity_pointer[ENT_PREV_TARGET_IN_LIST] = last_entity_on_list;
						entity[last_entity_on_list][ENT_NEXT_TARGET_IN_LIST] = other_entity_id;
					}

					last_entity_on_list = other_entity_id;
				}

				other_entity_id = other_entity_pointer[ENT_NEXT_GENERAL_ENT];

			}
		}
	}

	return counter;
}



int SCRIPTING_create_target_list (int *entity_pointer, int comparison_type, int comparative_variable, int comparative_value, int cone_size = UNSET)
{
	bool result;

	int cone_x_1,cone_y_1;
	int cone_x_2,cone_y_2;

	int cross_product_1;
	int cross_product_2;

	int angle_1;
	int angle_2;

	int other_entity_id;
	int *other_entity_pointer;

	int dx,dy;

	int distance;

	if (comparison_type == SELECT_BY_GREATER_THAN_DISTANCE)
	{
		comparative_value *= comparative_value;
		// As its a distance and we ain't square-rooting.
	}
	else if (comparison_type == SELECT_BY_RELATIVE_ANGLE)
	{
		angle_1 = comparative_value - cone_size;
		angle_2 = comparative_value + cone_size;

		cone_x_1 = (100 * MATH_get_cos_table_value (angle_1)) / 10000;
		cone_y_1 = (100 * MATH_get_sin_table_value (angle_1)) / 10000;

		cone_x_2 = (100 * MATH_get_cos_table_value (angle_2)) / 10000;
		cone_y_2 = (100 * MATH_get_sin_table_value (angle_2)) / 10000;
	}

	int counter = 0;

	int last_entity_on_list = UNSET;

	int prev_entity,next_entity;

	other_entity_id = first_entity_in_target_list;

	while (other_entity_id != UNSET)
	{
		other_entity_pointer = &entity[other_entity_id][0];
		next_entity = other_entity_pointer[ENT_NEXT_TARGET_IN_LIST];
		
		dx = other_entity_pointer[ENT_WORLD_X] - entity_pointer[ENT_WORLD_X];
		dy = other_entity_pointer[ENT_WORLD_Y] - entity_pointer[ENT_WORLD_Y];

		distance = (dx*dx)+(dy*dy);

		switch (comparison_type)
		{
		case SELECT_BY_EXACT_BOOLEAN_MATCH:
			result = (comparative_value == entity_pointer[comparative_variable]);
			break;

		case SELECT_BY_COMPLETE_BOOLEAN_MATCH:
			result = ((comparative_value && entity_pointer[comparative_variable]) == comparative_value);
			break;

		case SELECT_BY_PARTIAL_BOOLEAN_MATCH:
			result = (comparative_value && entity_pointer[comparative_variable]);
			break;

		case SELECT_BY_EXACT_VALUE_MATCH:
			result = (comparative_value == entity_pointer[comparative_variable]);
			break;

		case SELECT_BY_EXACT_OR_LOWER_VALUE_MATCH:
			result = (comparative_value >= entity_pointer[comparative_variable]);
			break;

		case SELECT_BY_EXACT_OR_HIGHER_VALUE_MATCH:
			result = (comparative_value <= entity_pointer[comparative_variable]);
			break;

		case SELECT_BY_RELATIVE_ANGLE:
			cross_product_1 = (cone_x_1 * dy) - (dx * cone_y_1);
			cross_product_2 = (cone_x_2 * dy) - (dx * cone_y_2);
			
			if ( (cross_product_1 > 0) && (cross_product_2 < 0) )
			{
				assert(0);
			}
			break;
			
		case SELECT_BY_GREATER_THAN_DISTANCE:
			result = (distance > comparative_value);
			break;

		default:
			break;
		}


		if (!result)
		{
			// We're gonna' remove it from the list, man!

			prev_entity = other_entity_pointer[ENT_PREV_TARGET_IN_LIST];

			other_entity_pointer[ENT_PREV_TARGET_IN_LIST] = UNSET;
			other_entity_pointer[ENT_NEXT_TARGET_IN_LIST] = UNSET;

			entity[prev_entity][ENT_NEXT_TARGET_IN_LIST] = next_entity;
			entity[next_entity][ENT_PREV_TARGET_IN_LIST] = prev_entity;

			if (other_entity_id == first_entity_in_target_list)
			{
				first_entity_in_target_list = next_entity;
			}
		}
		else
		{
			counter++;
		}

		other_entity_id = next_entity;
	}

	return counter;
}



void SCRIPTING_re_order_target_list (int *entity_pointer, int comparative_variable)
{
	// Look up quicksort! Meep!


}



void SCRIPTING_rotate_and_translate_entities (int *parent_entity_pointer, int x_angle, int y_angle, int z_angle, int x_translate, int y_translate, int z_translate, int draw_order_range, int draw_order_start, int depth_of_field = UNSET)
{
	int current_entity = parent_entity_pointer[ENT_FIRST_CHILD];
	int *current_entity_pointer;
	float bx,by,bz;
	float dx,dy,dz;
	int min_z = 999999; // Nice high and low numbers so they're bound to be
	int max_z = -999999; // replaced in the actual loop.

	float f_depth_of_field = float (depth_of_field);

	float f_x_angle = float (x_angle) / angle_conversion_ratio;
	float f_y_angle = float (y_angle) / angle_conversion_ratio;
	float f_z_angle = float (z_angle) / angle_conversion_ratio;

	// Note that we don't pass in the x and y translation at this point as it'll break the perspective
	// effect later on. However the z translation *is* passed in so that that aspect does work.
	float *matrix_pointer = MATH_create_3d_matrix ( f_x_angle, f_y_angle, f_z_angle, 0, 0, z_translate );

	do
	{
		current_entity_pointer = &entity[current_entity][0];

		bx = float (current_entity_pointer[ENT_BASE_X]);
		by = float (current_entity_pointer[ENT_BASE_Y]);
		bz = float (current_entity_pointer[ENT_BASE_Z]);
		
		dx = bx*matrix_pointer[0] + by*matrix_pointer[4] + bz*matrix_pointer[8] + matrix_pointer[12];
		dy = bx*matrix_pointer[1] + by*matrix_pointer[5] + bz*matrix_pointer[9] + matrix_pointer[13];
		dz = bx*matrix_pointer[2] + by*matrix_pointer[6] + bz*matrix_pointer[10] + matrix_pointer[14];

		if (depth_of_field == UNSET)
		{
			current_entity_pointer[ENT_WORLD_X] = int (dx) + x_translate;
			current_entity_pointer[ENT_WORLD_Y] = int (dy) + y_translate;
			current_entity_pointer[ENT_DRAW_ORDER] = int (dz);
		}
		else if (dz + depth_of_field > 0)
		{
			current_entity_pointer[ENT_WORLD_X] = int ( (dx * f_depth_of_field) / (dz + f_depth_of_field) ) + x_translate;
			current_entity_pointer[ENT_WORLD_Y] = int ( (dy * f_depth_of_field) / (dz + f_depth_of_field) ) + y_translate;
			current_entity_pointer[ENT_DRAW_ORDER] = int (dz);
		}

		if (current_entity_pointer[ENT_DRAW_ORDER] < min_z)
		{
			min_z = current_entity_pointer[ENT_DRAW_ORDER];
		}
		else if (current_entity_pointer[ENT_DRAW_ORDER] > max_z)
		{
			max_z = current_entity_pointer[ENT_DRAW_ORDER];
		}

		current_entity = entity[current_entity][ENT_NEXT_SIBLING];
	}
	while (current_entity != UNSET);

	int z_range = max_z - min_z;

	// This is what we have to multiply the DRAW_ORDER's by (after knocking off min_z) by.
	float z_scale = float (draw_order_range) / float (z_range);

	// So we go through 'em all and do just that...

	current_entity = parent_entity_pointer[ENT_FIRST_CHILD];

	do
	{
		current_entity_pointer = &entity[current_entity][0];

		current_entity_pointer[ENT_DRAW_ORDER] = int (float (current_entity_pointer[ENT_DRAW_ORDER] - min_z) * z_scale) + draw_order_start;

		current_entity = entity[current_entity][ENT_NEXT_SIBLING];
	}
	while (current_entity != UNSET);
}



void SCRIPTING_set_up_save_data (void)
{
	// This is called at the start of the program and leaves
	// the save data in a state ready to be added to.

	// It's also called after manually resetting the save
	// file at the beginning of the saving procedure and at
	// the end of it so we ain't got any memory hanging around.

	save_data.save_ai_node_pathfinding_tables = false;
	save_data.save_ai_zone_pathfinding_tables = false;
	save_data.save_spawn_point_flags = false;
	save_data.save_zone_flags = false;

	save_data.saved_entity_count = 0;
	save_data.saved_entity_number_list = NULL;
	save_data.saved_entity_tag_list = NULL;

	save_data.saved_flag_count = 0;
	save_data.saved_flag_list = NULL;

	save_data.loaded_entity_count = 0;
	save_data.loaded_entity_data = NULL;
}



void SCRIPTING_free_loaded_save_data (void)
{
	int entity_number;
	int array_number;

	if (save_data.loaded_entity_data != NULL)
	{
		for (entity_number = 0; entity_number < save_data.loaded_entity_count; entity_number++)
		{
			loaded_entity_struct *loaded_entity = &save_data.loaded_entity_data[entity_number];

			if (loaded_entity->loaded_entity_variable_list != NULL)
			{
				free(loaded_entity->loaded_entity_variable_list);
			}

			if (loaded_entity->loaded_entity_value_list != NULL)
			{
				free(loaded_entity->loaded_entity_value_list);
			}

			if (loaded_entity->loaded_entity_reference_variable_list != NULL)
			{
				free(loaded_entity->loaded_entity_reference_variable_list);
			}

			if (loaded_entity->loaded_entity_reference_tag_list != NULL)
			{
				free(loaded_entity->loaded_entity_reference_tag_list);
			}

			if (loaded_entity->loaded_state_variable_list != NULL)
			{
				free(loaded_entity->loaded_state_variable_list);
			}

			if (loaded_entity->loaded_state_offset_list != NULL)
			{
				free(loaded_entity->loaded_state_offset_list);
			}

			if (loaded_entity->loaded_state_label_list != NULL)
			{
				int state_number;

				for (state_number = 0; state_number < loaded_entity->loaded_state_count; state_number++)
				{
					if (loaded_entity->loaded_state_label_list[state_number] != NULL)
					{
						free(loaded_entity->loaded_state_label_list[state_number]);
					}
				}

				free(loaded_entity->loaded_state_label_list);
			}

			if (loaded_entity->array_data != NULL)
			{
				for (array_number = 0; array_number < loaded_entity->loaded_entity_array_count; array_number++)
				{
					if (loaded_entity->array_data[array_number].data != NULL)
					{
						free(loaded_entity->array_data[array_number].data);
					}
				}

				free(loaded_entity->array_data);
			}
		}

		free(save_data.loaded_entity_data);
	}

	save_data.loaded_entity_count = 0;
	save_data.loaded_entity_data = NULL;
}



void SCRIPTING_reset_save_data (void)
{
	// After saving the data, this function is called,
	// resetting everything so it's ready to go again.

	if (save_data.saved_entity_number_list != NULL)
	{
		free (save_data.saved_entity_number_list);
	}

	if (save_data.saved_entity_tag_list != NULL)
	{
		free (save_data.saved_entity_tag_list);
	}

	if (save_data.saved_flag_list != NULL)
	{
		free (save_data.saved_flag_list);
	}

	SCRIPTING_free_loaded_save_data ();

	SCRIPTING_set_up_save_data ();	
}



void SCRIPTING_save_spawn_point_flags (void)
{
	save_data.save_spawn_point_flags = true;
}



void SCRIPTING_save_ai_node_tables (void)
{
	save_data.save_ai_node_pathfinding_tables = true;
}



void SCRIPTING_save_ai_zone_tables (void)
{
	save_data.save_ai_node_pathfinding_tables = true;
}



void SCRIPTING_save_zone_flags (void)
{
	save_data.save_zone_flags = true;
}



void SCRIPTING_save_entity (int entity_number, int tag)
{
	int sec = save_data.saved_entity_count;

	if (sec == 0)
	{
		save_data.saved_entity_number_list = (int *) malloc (sizeof(int));
		save_data.saved_entity_tag_list = (int *) malloc (sizeof(int));
	}
	else
	{
		save_data.saved_entity_number_list = (int *) realloc (save_data.saved_entity_number_list , sizeof(int) * (sec+1) );
		save_data.saved_entity_tag_list = (int *) realloc (save_data.saved_entity_tag_list , sizeof(int) * (sec+1) );
	}

	save_data.saved_entity_number_list[sec] = entity_number;
	save_data.saved_entity_tag_list[sec] = tag;

	save_data.saved_entity_count++;
}



void SCRIPTING_save_flag (int flag_number)
{
	int sfc = save_data.saved_flag_count;

	if (sfc == 0)
	{
		save_data.saved_flag_list = (int *) malloc (sizeof(int));
	}
	else
	{
		save_data.saved_flag_list = (int *) realloc (save_data.saved_flag_list , sizeof(int) * (sfc+1) );
	}

	save_data.saved_flag_list[sfc] = flag_number;

	save_data.saved_flag_count++;
}



void SCRIPTING_output_flags_to_file (char *filename)
{
	int flag_num,flag_index;
	char *flag_name;

	char line[MAX_LINE_SIZE];
	char normalised_filename[MAX_LINE_SIZE];

	snprintf(normalised_filename, sizeof(normalised_filename), "%s", filename);
	STRING_lowercase(normalised_filename);

	// Continue saves need the live special-random strand state, not just whatever
	// happened to be sitting in the backing flag from earlier script activity.
	SCRIPTING_sync_special_random_seed_flag(0);

	FILE *file_pointer = fopen (MAIN_get_project_filename (normalised_filename, true),"a");

	if (file_pointer != NULL)
	{
		fputs("#START_OF_FLAG_DATA\n",file_pointer);

		for (flag_num=0; flag_num<save_data.saved_flag_count; flag_num++)
		{
			flag_index = save_data.saved_flag_list[flag_num];
			flag_name = GPL_get_entry_name ("FLAG", flag_index);
							snprintf(line, sizeof(line),"\tGLOBAL_FLAG_NAME = '%s' GLOBAL_FLAG_VALUE = %i\n",flag_name,flag_array[flag_index]);

			fputs(line,file_pointer);
		}

		fputs("#END_OF_DATA\n\n",file_pointer);

		fclose(file_pointer);
	}
	else
	{
		OUTPUT_message("Cannot append Spawn Point Flags to save file!");
		assert(0);
	}
}



void SCRIPTING_output_entity_non_relation_properties_to_file (int ent_index, FILE *file_pointer)
{
	// This outputs all the variables from ENT_ALIVE onwards as ascii. It's from ENT_ALIVE onwards
	// because all the previous one are relative links, containing important information which if
	// overwritten will either stick the engine in loop or more likely crash it.

	int var_index;
	int length = GPL_list_size ("VARIABLE");
	char line[MAX_LINE_SIZE];
	int actual_length = 0;

	for (var_index = GPL_find_word_value ("VARIABLE", "ALIVE"); var_index<length; var_index++)
	{
		if (SAVEGAME_should_output_entity_variable(ent_index, var_index))
		{
			actual_length++;
		}
	}

	snprintf(line,sizeof(line),"\t\t#START_OF_VARIABLES_COUNT = %i\n",actual_length);
	fputs(line,file_pointer);

	for (var_index = GPL_find_word_value ("VARIABLE", "ALIVE"); var_index<length; var_index++)
	{
		if (SAVEGAME_should_output_entity_variable(ent_index, var_index))
		{
			snprintf(line,sizeof(line),"\t\t\t#ENTITY_VARIABLE '%s' = %i\n",GPL_get_entry_name ("VARIABLE", var_index) , entity[ent_index][var_index]);
			fputs(line,file_pointer);
		}
	}

	fputs("\t\t#END_OF_VARIABLES\n",file_pointer);
}



void SCRIPTING_input_entities_from_file (char *filename)
{
	// Righty, we go through the file until we find an "#START_OF_ENTITY_DATA_COUNT"
	// whereupon we malloc the correct amount of space.

	int entity_count;
	int entity_number;
	int variable_number;
	int variable_index;
	int variable_value;
	int variable_count;
	int state_count;
	int array_number;
	int array_count;
	int array_data_counter;
	int array_data_size;

	char line[MAX_LINE_SIZE];
	char word[MAX_LINE_SIZE];
	char *pointer;
	bool exit_loop = false;
	bool can_exit = false;

	FILE *file_pointer = FILE_open_project_read_case_fallback(filename);

	if (file_pointer != NULL)
	{
		while ( ( fgets ( line , MAX_LINE_SIZE , file_pointer ) != NULL ) && (exit_loop == false) )
		{
			SCRIPTING_pump_save_load_ui(false);
			STRING_strip_all_disposable (line);

			pointer = STRING_end_of_string(line,"#START_OF_ENTITY_DATA_COUNT = ");
			if (pointer != NULL)
			{
				// We've found the data start!

				entity_count = atoi(pointer);

				save_data.loaded_entity_count = entity_count;
				save_data.loaded_entity_data = (loaded_entity_struct *) malloc (sizeof(loaded_entity_struct) * entity_count);

				// Set up all the stuff so the pointer's are nulled.

				for (entity_number=0; entity_number<entity_count; entity_number++)
				{
					save_data.loaded_entity_data[entity_number].loaded_entity_array_count = 0;
					save_data.loaded_entity_data[entity_number].loaded_variable_count = 0;
					save_data.loaded_entity_data[entity_number].loaded_reference_count = 0;
					save_data.loaded_entity_data[entity_number].loaded_state_count = 0;

					save_data.loaded_entity_data[entity_number].array_data = NULL;
					save_data.loaded_entity_data[entity_number].loaded_entity_variable_list = NULL;
					save_data.loaded_entity_data[entity_number].loaded_entity_value_list = NULL;
					save_data.loaded_entity_data[entity_number].loaded_entity_reference_variable_list = NULL;
					save_data.loaded_entity_data[entity_number].loaded_entity_reference_tag_list = NULL;
					save_data.loaded_entity_data[entity_number].loaded_state_variable_list = NULL;
					save_data.loaded_entity_data[entity_number].loaded_state_offset_list = NULL;
					save_data.loaded_entity_data[entity_number].loaded_state_label_list = NULL;

					save_data.loaded_entity_data[entity_number].loaded_entity_tag = UNSET;
				}

				entity_number = 0;

				can_exit = true;
			}

			if (can_exit)
			{
				// We've gotten to the start of the data, so lets start looking for stuff! WOOOO!

				pointer = STRING_end_of_string(line,"#ENTITY_TAG = ");
				if (pointer != NULL)
				{
					// Start of a new entity, get the tag, man!	
					save_data.loaded_entity_data[entity_number].loaded_entity_tag = atoi(pointer);
				}

				pointer = STRING_end_of_string(line,"#START_OF_VARIABLES_COUNT = ");
				if (pointer != NULL)
				{
					// Start of variable data, allocate space for the variables and reset the variable counter.

					variable_number = 0;
					array_number = 0;
					variable_count = atoi(pointer);
					
					save_data.loaded_entity_data[entity_number].loaded_variable_count = variable_count;

					save_data.loaded_entity_data[entity_number].loaded_entity_value_list = (int *) malloc (sizeof(int) * variable_count);
					save_data.loaded_entity_data[entity_number].loaded_entity_variable_list = (int *) malloc (sizeof(int) * variable_count);
				}

				pointer = STRING_end_of_string(line,"#ENTITY_VARIABLE '");
				if (pointer != NULL)
				{
					// Here's a variable. Woot!

					strcpy(word,pointer);
					strtok(word,"'");

					// Should just have the flag name now...
					variable_index = GPL_find_word_value("VARIABLE",word);

					pointer = STRING_end_of_string(line,"' = ");
					if (pointer != NULL)
					{
						variable_value = atoi(pointer);
						
						save_data.loaded_entity_data[entity_number].loaded_entity_value_list[variable_number] = variable_value;
						save_data.loaded_entity_data[entity_number].loaded_entity_variable_list[variable_number] = variable_index;
					}
					else
					{
						// WTF!

						OUTPUT_message("No value for variable!");
						assert(0);
					}

					variable_number++;
				}

				pointer = STRING_end_of_string(line,"#START_OF_REFERENCES_COUNT = ");
				if (pointer != NULL)
				{
					variable_number = 0;
					variable_count = atoi(pointer);

					save_data.loaded_entity_data[entity_number].loaded_reference_count = variable_count;
					save_data.loaded_entity_data[entity_number].loaded_entity_reference_variable_list = (int *) malloc (sizeof(int) * variable_count);
					save_data.loaded_entity_data[entity_number].loaded_entity_reference_tag_list = (int *) malloc (sizeof(int) * variable_count);
				}

				pointer = STRING_end_of_string(line,"#ENTITY_REFERENCE '");
				if (pointer != NULL)
				{
					strcpy(word,pointer);
					strtok(word,"'");

					variable_index = GPL_find_word_value("VARIABLE",word);

					pointer = STRING_end_of_string(line,"' = ");
					if (pointer != NULL)
					{
						variable_value = atoi(pointer);

						save_data.loaded_entity_data[entity_number].loaded_entity_reference_variable_list[variable_number] = variable_index;
						save_data.loaded_entity_data[entity_number].loaded_entity_reference_tag_list[variable_number] = variable_value;
					}
					else
					{
						OUTPUT_message("No value for entity reference!");
						assert(0);
					}

					variable_number++;
				}

				pointer = STRING_end_of_string(line,"#START_OF_STATE_COUNT = ");
				if (pointer != NULL)
				{
					variable_number = 0;
					state_count = atoi(pointer);

					save_data.loaded_entity_data[entity_number].loaded_state_count = state_count;
					save_data.loaded_entity_data[entity_number].loaded_state_variable_list = (int *) malloc (sizeof(int) * state_count);
					save_data.loaded_entity_data[entity_number].loaded_state_offset_list = (int *) malloc (sizeof(int) * state_count);
					save_data.loaded_entity_data[entity_number].loaded_state_label_list = (char **) malloc (sizeof(char *) * state_count);
				}

				pointer = STRING_end_of_string(line,"#ENTITY_STATE '");
				if (pointer != NULL)
				{
					char *label_start;
					char *label_end;
					char *offset_start;

					strcpy(word,pointer);
					strtok(word,"'");

					variable_index = GPL_find_word_value("VARIABLE",word);

					label_start = strchr(pointer, '\'');
					if (label_start != NULL)
					{
						label_start = strchr(label_start + 1, '\'');
					}

					if (label_start != NULL)
					{
						label_start += 1;
						label_end = strchr(label_start, '\'');

						if (label_end != NULL)
						{
							int label_length = (int)(label_end - label_start);
							char label_word[NAME_SIZE];

							if (label_length >= NAME_SIZE)
							{
								label_length = NAME_SIZE - 1;
							}

							strncpy(label_word, label_start, label_length);
							label_word[label_length] = '\0';

							offset_start = STRING_end_of_string(label_end, "' + ");
							if (offset_start != NULL)
							{
								save_data.loaded_entity_data[entity_number].loaded_state_variable_list[variable_number] = variable_index;
								save_data.loaded_entity_data[entity_number].loaded_state_offset_list[variable_number] = atoi(offset_start);
								save_data.loaded_entity_data[entity_number].loaded_state_label_list[variable_number] = (char *) malloc(strlen(label_word) + 1);
								strcpy(save_data.loaded_entity_data[entity_number].loaded_state_label_list[variable_number], label_word);
							}
							else
							{
								OUTPUT_message("No offset for entity state!");
								assert(0);
							}
						}
						else
						{
							OUTPUT_message("No label end for entity state!");
							assert(0);
						}
					}
					else
					{
						OUTPUT_message("No label for entity state!");
						assert(0);
					}

					variable_number++;
				}

				pointer = STRING_end_of_string(line,"#END_OF_REFERENCES");
				if (pointer != NULL)
				{
					if (variable_number != save_data.loaded_entity_data[entity_number].loaded_reference_count)
					{
						OUTPUT_message("Read in entity reference count does not match expected!");
						assert(0);
					}
				}

				pointer = STRING_end_of_string(line,"#END_OF_STATES");
				if (pointer != NULL)
				{
					if (variable_number != save_data.loaded_entity_data[entity_number].loaded_state_count)
					{
						OUTPUT_message("Read in entity state count does not match expected!");
						assert(0);
					}
				}

				pointer = STRING_end_of_string(line,"#END_OF_VARIABLES");
				if (pointer != NULL)
				{
					// End of the variable list, check that we read in the right amount of vars.

					if (variable_number != variable_count)
					{
						OUTPUT_message("Read in variable count does not match expected!");
						assert(0);
					}
				}

				pointer = STRING_end_of_string(line,"#START_OF_VARIABLES_COUNT = ");
				if (pointer != NULL)
				{
					// Start of variable data, allocate space for the variables and reset the variable counter.

					variable_number = 0;
					
					variable_count = atoi(pointer);
					
					save_data.loaded_entity_data[entity_number].loaded_variable_count = variable_count;

					save_data.loaded_entity_data[entity_number].loaded_entity_value_list = (int *) malloc (sizeof(int) * variable_count);
					save_data.loaded_entity_data[entity_number].loaded_entity_variable_list = (int *) malloc (sizeof(int) * variable_count);
				}

				pointer = STRING_end_of_string(line,"#ARRAY_COUNT = ");
				if (pointer != NULL)
				{
					// Start of array data, allocate space for the arrays and reset the array counter.

					array_number = 0;

					array_count = atoi(pointer);

					save_data.loaded_entity_data[entity_number].loaded_entity_array_count = array_count;

					save_data.loaded_entity_data[entity_number].array_data = (loaded_array_struct *) malloc (sizeof(loaded_array_struct) * array_count);
				}

				pointer = STRING_end_of_string(line,"#ARRAY_SIZE = ");
				if (pointer != NULL)
				{
					// Here's an array, but how big is it?
					char *token;

					strcpy(word,pointer);
					token = strtok(word," *");
					save_data.loaded_entity_data[entity_number].array_data[array_number].width = (token != NULL) ? atoi(token) : 0;
					token = strtok(NULL," *");
					save_data.loaded_entity_data[entity_number].array_data[array_number].height = (token != NULL) ? atoi(token) : 0;
					token = strtok(NULL," *");
					save_data.loaded_entity_data[entity_number].array_data[array_number].depth = (token != NULL) ? atoi(token) : 0;

					array_data_size = save_data.loaded_entity_data[entity_number].array_data[array_number].width * save_data.loaded_entity_data[entity_number].array_data[array_number].height * save_data.loaded_entity_data[entity_number].array_data[array_number].depth;

					save_data.loaded_entity_data[entity_number].array_data[array_number].data = (int *) malloc (sizeof(int) * array_data_size);

					array_data_counter = 0;
				}

				pointer = STRING_end_of_string(line,"#ARRAY_UID = ");
				if (pointer != NULL)
				{
					// Start of array data, allocate space for the arrays and reset the array counter.

					save_data.loaded_entity_data[entity_number].array_data[array_number].uid = atoi(pointer);
				}

				pointer = STRING_end_of_string(line,"#ARRAY_DATA = ");
				if (pointer != NULL)
				{
					// Here's an array, but how big is it?
					char *token;

					strcpy(word,pointer);
					token = strtok(word,",");

					while (token != NULL)
					{
						if (array_data_counter < array_data_size)
						{
							save_data.loaded_entity_data[entity_number].array_data[array_number].data[array_data_counter] = atoi(token);
							array_data_counter++;
						}
						else
						{
							OUTPUT_message("Trying to read more data into array than there is space!");
							assert(0);
						}

						token = strtok(NULL,",");
					}
				}

				if (strcmp(line,"#END_OF_THIS_ARRAY") == 0)
				{
					if (array_data_counter != array_data_size)
					{
						OUTPUT_message("Read in array data amount does not match expected!");
						assert(0);
					}

					array_number++;
				}

				if (strcmp(line,"#ARRAY_DATA_END") == 0)
				{
					if (array_count != array_number)
					{
						OUTPUT_message("Read in array count does not match expected!");
						assert(0);
					}
				}

				if (strcmp(line,"#END_OF_THIS_ENTITY") == 0)
				{
					entity_number++;
				}

				if (strcmp(line,"#END_OF_ENTITY_DATA") == 0)
				{
					// No entities for you!

					// Make sure we read in the same number of entities as we were expecting...

					if (entity_count != entity_number)
					{
						OUTPUT_message("Read in entity count does not match expected!");
						assert(0);
					}

					exit_loop = true;
				}

			}

		}

		fclose(file_pointer);

	}
	else
	{
		OUTPUT_message("Can't open save file for reading of entities!");
		assert(0);
	}

}



void SCRIPTING_output_entity_to_file (int ent_index, int ent_tag, FILE *file_pointer)
{
	char line[MAX_LINE_SIZE];

		snprintf(line, sizeof(line), "\t#ENTITY_TAG = %i\n", ent_tag);
	fputs(line, file_pointer);

	// This first outputs all the variable names and values... Note when it's reloaded it won't overwrite the parents or other relational variables.
	SCRIPTING_output_entity_non_relation_properties_to_file (ent_index, file_pointer);
	SCRIPTING_output_entity_state_overrides_to_file (ent_index, file_pointer);
	SAVEGAME_output_entity_references_to_file (ent_index, file_pointer);

	// Then it outputs any arrays which the entity has...
	ARRAY_output_entity_arrays_to_file (ent_index, file_pointer);

	// It will NOT output the text information because that
	// can simply be regenerated - stop being a lazy sod!

	// And cap it off.

	fputs("\t#END_OF_THIS_ENTITY\n\n", file_pointer);
}



void SCRIPTING_output_entities_to_file (char *filename)
{
	int ent_num, ent_index, ent_tag;
	char word[MAX_LINE_SIZE];
	char normalised_filename[MAX_LINE_SIZE];

	snprintf(normalised_filename, sizeof(normalised_filename), "%s", filename);
	STRING_lowercase(normalised_filename);

	FILE *file_pointer = fopen (MAIN_get_project_filename (normalised_filename),"a");

	if (file_pointer != NULL)
	{
		 snprintf(word, sizeof(word),"#START_OF_ENTITY_DATA_COUNT = %i\n\n",save_data.saved_entity_count);
		fputs(word,file_pointer);

		for (ent_num=0; ent_num<save_data.saved_entity_count; ent_num++)
		{
			SCRIPTING_pump_save_load_ui(false);
			ent_index = save_data.saved_entity_number_list[ent_num];
			ent_tag = save_data.saved_entity_tag_list[ent_num];

			SCRIPTING_output_entity_to_file (ent_index, ent_tag, file_pointer);
		}

		fputs("#END_OF_ENTITY_DATA\n\n",file_pointer);

		fclose(file_pointer);
	}
	else
	{
		OUTPUT_message("Cannot append Spawn Points to save file!");
		assert(0);
	}
}



void SCRIPTING_output_save_data_to_file (int filename_text_tag)
{
	// Goes through each of the bits of gubbins which needs
	// to be saved and makes a nice ascii version of it.

	char filename[MAX_LINE_SIZE];
	char line[MAX_LINE_SIZE];
	unsigned int total_start_ms;
	unsigned int stage_start_ms;
	unsigned int init_ms = 0;
	unsigned int spawn_point_ms = 0;
	unsigned int zone_flag_ms = 0;
	unsigned int flag_ms = 0;
	unsigned int entity_ms = 0;
	unsigned int checksum_ms = 0;
	unsigned int total_ms;
	long file_size;

	char *filename_pointer = TEXTFILE_get_line_by_index (filename_text_tag);
	
	strcpy (filename,filename_pointer);
	strcat (filename,".sav");
	STRING_uppercase (filename);
	total_start_ms = PLATFORM_get_wall_time_ms();
	stage_start_ms = total_start_ms;
	SCRIPTING_reset_save_load_ui_pump();
	SCRIPTING_pump_save_load_ui(true);

	FILE *file_pointer = fopen (MAIN_get_project_filename (STRING_lowercase(filename), true),"w");

	if (file_pointer != NULL)
	{
		snprintf (line, sizeof(line), "#SAVE_DATA (ID = %i)\n\n", MATH_rand(100000,999999));
		fputs (line,file_pointer);
		fclose (file_pointer);
	}
	else
	{
		OUTPUT_message("Cannot append Spawn Point Flags to save file!");
		assert(0);
	}
	init_ms = PLATFORM_get_wall_time_ms() - stage_start_ms;

	if (save_data.save_ai_node_pathfinding_tables)
	{
		// Do this later. Lazy me!
	}

	if (save_data.save_ai_zone_pathfinding_tables)
	{
		// Do this later. Lazy me!
	}

	if (save_data.save_spawn_point_flags)
	{
		stage_start_ms = PLATFORM_get_wall_time_ms();
		SPAWNPOINTS_output_altered_flags_to_file (filename);
		spawn_point_ms = PLATFORM_get_wall_time_ms() - stage_start_ms;
	}

	if (save_data.save_zone_flags)
	{
		stage_start_ms = PLATFORM_get_wall_time_ms();
		TILEMAPS_output_altered_zone_flags_to_file (filename);
		zone_flag_ms = PLATFORM_get_wall_time_ms() - stage_start_ms;
	}

	if (save_data.saved_flag_count > 0)
	{
		stage_start_ms = PLATFORM_get_wall_time_ms();
		SCRIPTING_output_flags_to_file (filename);
		flag_ms = PLATFORM_get_wall_time_ms() - stage_start_ms;
	}

	if (save_data.saved_entity_count > 0)
	{
		stage_start_ms = PLATFORM_get_wall_time_ms();
		SCRIPTING_output_entities_to_file (filename);
		entity_ms = PLATFORM_get_wall_time_ms() - stage_start_ms;
	}



	stage_start_ms = PLATFORM_get_wall_time_ms();
	SCRIPTING_append_checksum_to_save_file (filename);
	checksum_ms = PLATFORM_get_wall_time_ms() - stage_start_ms;
	total_ms = PLATFORM_get_wall_time_ms() - total_start_ms;
	file_size = SCRIPTING_get_save_file_size_bytes(filename);

	SCRIPTING_log_save_load_profile(
		"save",
		filename,
		file_size,
		total_ms,
		init_ms,
		spawn_point_ms,
		zone_flag_ms,
		flag_ms,
		entity_ms,
		checksum_ms,
		save_data.saved_entity_count);

	SCRIPTING_reset_save_data ();
}



void SCRIPTING_input_flags_from_file (char *filename)
{
	int flag_num,flag_value;
	char line[MAX_LINE_SIZE];
	char word[MAX_LINE_SIZE];
	char *pointer;
	bool exit_loop = false;
	bool can_exit = false;

	FILE *file_pointer = FILE_open_project_read_case_fallback(filename);
	
	if (file_pointer != NULL)
	{
		while ( ( fgets ( line , MAX_LINE_SIZE , file_pointer ) != NULL ) && (exit_loop == false) )
		{
			SCRIPTING_pump_save_load_ui(false);
			STRING_strip_all_disposable (line);

			if (strcmp(line,"#START_OF_FLAG_DATA") == 0)
			{
				// We've found the flag data start!
				can_exit = true;
			}

			if (can_exit)
			{
				// We've gotten to the start of the flag data, so lets start looking for stuff! WOOOO!
				
				pointer = STRING_end_of_string(line,"GLOBAL_FLAG_NAME = '");
				if (pointer != NULL)
				{
					strcpy(word,pointer);
					strtok(word,"'");

					// Should just have the flag name now...
					flag_num = GPL_find_word_value("FLAG",word);

					pointer = STRING_end_of_string(line,"GLOBAL_FLAG_VALUE = ");
					if (pointer != NULL)
					{
						flag_value = atoi(pointer);
						
						flag_array[flag_num] = flag_value;
					}
					else
					{
						// WTF!

						OUTPUT_message("No value for flag!");
						assert(0);
					}
				}

				if (strcmp(line,"#END_OF_DATA") == 0)
				{
					// We're outta' here, baby!
					exit_loop = true;
				}

			}

		}

		fclose(file_pointer);
	}
	else
	{
		OUTPUT_message("Flag reading routine cannot read from Save File!");
		assert(0);
	}
}



char *SCRIPTING_get_checksum_from_save_file (char *filename)
{
	char *pointer;
	char normalised_filename[MAX_LINE_SIZE];
	char line[MAX_LINE_SIZE];
	static char checkcode[MAX_LINE_SIZE];

	snprintf (checkcode, sizeof(checkcode), "UNSET");

	bool exit_loop = false;

	snprintf(normalised_filename, sizeof(normalised_filename), "%s", filename);
	STRING_lowercase(normalised_filename);

	FILE *file_pointer = FILE_open_project_read_case_fallback(normalised_filename);

	if (file_pointer != NULL)
	{
		while ( ( fgets ( line , MAX_LINE_SIZE , file_pointer ) != NULL ) && (exit_loop == false) )
		{
			STRING_strip_all_disposable (line);

			pointer = STRING_end_of_string(line,"#SAVE FILE CHECKSUM = ");
			if (pointer != NULL)
			{
				strcpy (checkcode,pointer);
			}

		}

		fclose (file_pointer);
	}
	else
	{
		assert (0);
	}

	return checkcode;
}



char *SCRIPTING_generate_checkcode_for_save_file (char *filename)
{
	// This reads lines from the given text file, strips all the junk from them
	// and updates the checksum incrementally. The old implementation repeatedly
	// realloc'd and concatenated one giant buffer, which was disproportionately
	// expensive on low-powered devices.
	char normalised_filename[MAX_LINE_SIZE];
	char *pointer;
	char line[MAX_LINE_SIZE];
	unsigned char checksum[4];
	int cycle;
	bool exit_loop = false;

	SCRIPTING_reset_checkcode_state(checksum, &cycle);

	snprintf(normalised_filename, sizeof(normalised_filename), "%s", filename);
	STRING_lowercase(normalised_filename);

	FILE *file_pointer = FILE_open_project_read_case_fallback(normalised_filename);

	if (file_pointer != NULL)
	{
		while ( ( fgets ( line , MAX_LINE_SIZE , file_pointer ) != NULL ) && (exit_loop == false) )
		{
			STRING_strip_all_disposable (line);

			pointer = STRING_end_of_string(line,"#SAVE FILE CHECKSUM = ");
			if (pointer == NULL)
			{
				// ie, we don't include the possibly already present checksum when creating the checksum...

				if (strlen(line)>0)
				{
					SCRIPTING_update_checkcode_state(line, checksum, &cycle);
				}
			}

		}

		fclose (file_pointer);
	}
	else
	{
		assert (0);
	}

	return SCRIPTING_get_checkcode_from_state(checksum);
}



void SCRIPTING_append_checksum_to_save_file (char *filename)
{
	char line[MAX_LINE_SIZE];
	char normalised_filename[MAX_LINE_SIZE];

	snprintf (line, sizeof(line), "#SAVE FILE CHECKSUM = %s\n", SCRIPTING_generate_checkcode_for_save_file(filename));

	snprintf(normalised_filename, sizeof(normalised_filename), "%s", filename);
	STRING_lowercase(normalised_filename);

	FILE *file_pointer = fopen (MAIN_get_project_filename (normalised_filename, true),"a");
	
	if (file_pointer != NULL)
	{
		fputs(line,file_pointer);
		fclose(file_pointer);
	}
	else
	{
		assert(0);
	}
}



void SCRIPTING_load_save_file (int filename_text_tag)
{
	// This opens up a save file and loads all it's junk. It only stores the saved entities
	// and their cruft, whereas everything else is instantly overwritten. INSTANTLY, DAMMIT!

	// Clean out the current crap.
	SCRIPTING_reset_save_data ();
	SAVEGAME_clear_restored_live_entity_flags();

	char filename[MAX_LINE_SIZE];
	char normalised_filename[MAX_LINE_SIZE];
	unsigned int total_start_ms;
	unsigned int stage_start_ms;
	unsigned int checksum_generate_ms = 0;
	unsigned int checksum_read_ms = 0;
	unsigned int spawn_point_ms = 0;
	unsigned int zone_flag_ms = 0;
	unsigned int flag_ms = 0;
	unsigned int entity_ms = 0;
	unsigned int total_ms;
	long file_size;

	char *filename_pointer = TEXTFILE_get_line_by_index (filename_text_tag);
	
	strcpy (filename,filename_pointer);
	strcat (filename,".SAV");
	STRING_uppercase (filename);
	snprintf(normalised_filename, sizeof(normalised_filename), "%s", filename);
	STRING_lowercase(normalised_filename);
	total_start_ms = PLATFORM_get_wall_time_ms();
	file_size = SCRIPTING_get_save_file_size_bytes(normalised_filename);
	SCRIPTING_reset_save_load_ui_pump();
	SCRIPTING_pump_save_load_ui(true);

	FILE *file_pointer = FILE_open_project_read_case_fallback(normalised_filename);
	
	if (file_pointer == NULL)
	{
		// File doesn't exist.
		return;
	}
	else
	{
		// It exists! So we need to check it's checksum, don't we?

		fclose(file_pointer);

		{
			char generated_checkcode[MAX_LINE_SIZE];
			char existing_checkcode[MAX_LINE_SIZE];
			stage_start_ms = PLATFORM_get_wall_time_ms();
			snprintf(generated_checkcode, sizeof(generated_checkcode), "%s", SCRIPTING_generate_checkcode_for_save_file(normalised_filename));
			checksum_generate_ms = PLATFORM_get_wall_time_ms() - stage_start_ms;
			stage_start_ms = PLATFORM_get_wall_time_ms();
			snprintf(existing_checkcode, sizeof(existing_checkcode), "%s", SCRIPTING_get_checksum_from_save_file(normalised_filename));
			checksum_read_ms = PLATFORM_get_wall_time_ms() - stage_start_ms;

			if ((strcmp(existing_checkcode, "UNSET") == 0) || (strcmp (generated_checkcode , existing_checkcode ) == 0))
		{
			// We read out the crap in the same order that we put it in, although
			// given that each load routine parses the file completely there seems
			// no real point to this...

			// LOAD AI NODE PATHFINDING TABLES

			// LOAD AI ZONE PATHFINDING TABLES

			stage_start_ms = PLATFORM_get_wall_time_ms();
			SPAWNPOINTS_input_flags_from_file (normalised_filename);
			spawn_point_ms = PLATFORM_get_wall_time_ms() - stage_start_ms;

			stage_start_ms = PLATFORM_get_wall_time_ms();
			TILEMAPS_input_zone_flags_from_file (normalised_filename);
			zone_flag_ms = PLATFORM_get_wall_time_ms() - stage_start_ms;

			stage_start_ms = PLATFORM_get_wall_time_ms();
			SCRIPTING_input_flags_from_file (normalised_filename);
			SCRIPTING_restore_special_random_seed_from_flags();
			flag_ms = PLATFORM_get_wall_time_ms() - stage_start_ms;

			stage_start_ms = PLATFORM_get_wall_time_ms();
			SCRIPTING_input_entities_from_file (normalised_filename);
			entity_ms = PLATFORM_get_wall_time_ms() - stage_start_ms;
			total_ms = PLATFORM_get_wall_time_ms() - total_start_ms;

			SCRIPTING_log_save_load_profile(
				"load",
				normalised_filename,
				file_size,
				total_ms,
				checksum_generate_ms,
				checksum_read_ms,
				spawn_point_ms,
				zone_flag_ms,
				flag_ms,
				entity_ms,
				save_data.loaded_entity_count);

		}
		else
		{
			// Mismatching checksum.
			return;
		}
		}
	}

}

void SCRIPTING_delete_save_file (int filename_text_tag)
{
	char filename[MAX_LINE_SIZE];
	char normalised_filename[MAX_LINE_SIZE];
	char *filename_pointer = TEXTFILE_get_line_by_index(filename_text_tag);

	strcpy(filename, filename_pointer);
	strcat(filename, ".SAV");
	STRING_uppercase(filename);
	snprintf(normalised_filename, sizeof(normalised_filename), "%s", filename);
	STRING_lowercase(normalised_filename);

	remove(MAIN_get_project_filename(normalised_filename, true));
}

int SCRIPTING_does_save_file_exist (int filename_text_tag)
{
	char filename[MAX_LINE_SIZE];
	char normalised_filename[MAX_LINE_SIZE];
	char *filename_pointer = TEXTFILE_get_line_by_index(filename_text_tag);
	FILE *file_pointer;

	strcpy(filename, filename_pointer);
	strcat(filename, ".SAV");
	STRING_uppercase(filename);
	snprintf(normalised_filename, sizeof(normalised_filename), "%s", filename);
	STRING_lowercase(normalised_filename);

	file_pointer = FILE_open_project_read_case_fallback(normalised_filename);
	if (file_pointer != NULL)
	{
		fclose(file_pointer);
		return true;
	}

	return false;
}



void SCRIPTING_state_dump (void)
{
	// This writes out the state of every bastarding "thing" in the game as a lovely text file
	// so that it's abundantly obvious what's going on where.

	// It also writes out the values of all the global flags. Ho yuss.
	
	char line[MAX_LINE_SIZE];

	int entity_id;
	int *entity_pointer;

	remove (MAIN_get_project_filename("entity_dump.txt"));

	FILE *fp = fopen (MAIN_get_project_filename("entity_dump.txt"),"w");

	if (fp != NULL)
	{
		entity_id = first_processed_entity_in_list;
		int variable;

		int variable_list_size = GPL_list_size ("VARIABLE");

		while (entity_id != UNSET)
		{
			entity_pointer = &entity[entity_id][0];

			snprintf (line, sizeof(line), "Entity #%i = %s\n", entity_id, GPL_get_entry_name("SCRIPTS", entity_pointer[ENT_SCRIPT_NUMBER]));

			fputs(line,fp);

			for (variable=0; variable<variable_list_size; variable++)
			{
					snprintf(line, sizeof(line),"\t%s = %i\n",GPL_get_entry_name("VARIABLE",variable),entity_pointer[variable]);
				
				fputs(line,fp);
			}

			fputs("\n",fp);

			entity_id = entity_pointer [ENT_NEXT_PROCESS_ENT];
		}

		int flag_number;

		fputs("Flag List:\n\n",fp);

		for (flag_number=0; flag_number<flag_count; flag_number++)
		{
				snprintf(line, sizeof(line),"\t%s = %i\n",GPL_get_entry_name("FLAG",flag_number),flag_array[flag_number]);
			fputs(line,fp);
		}

		fclose(fp);
	}
	else
	{
		OUTPUT_message("Cannot output entity dump!");
		assert(0);
	}
}



void SCRIPTING_reset_watch_list_file (void)
{
	remove ("watched_variable_report.txt");
}



void SCRIPTING_add_report_to_watch_report (int causing_entity, int affected_entity, int line_number, int variable_altered, int old_value, int new_value )
{
	char line[MAX_LINE_SIZE];

	FILE *fp = fopen ("watched_variable_report.txt","a");

	if (fp != NULL)
	{
		snprintf (line, sizeof(line), "Entity %i(%s) altered entity %i(%s) on line number %i. The variable %s was changed from %i to %i. Frame count = %i.\n", causing_entity, GPL_get_entry_name("SCRIPTS", entity[causing_entity][ENT_SCRIPT_NUMBER]), affected_entity, GPL_get_entry_name("SCRIPTS", entity[causing_entity][ENT_SCRIPT_NUMBER]), line_number, GPL_get_entry_name("VARIABLE", variable_altered), old_value, new_value, frames_so_far );
		fputs (line,fp);
		fclose (fp);
	}
	else
	{
		OUTPUT_message("Cannot write to watch report!");
		assert(0);
	}
}



void SCRIPTING_add_to_watch_list (char *script_name, char *variable_name)
{
	int script_number = GPL_find_word_value ("SCRIPTS", script_name);

	int variable_number = GPL_find_word_value ("VARIABLE", variable_name);

	SCRIPTING_add_entry_to_watch_list (script_number, variable_number);
}



void SCRIPTING_setup_watch_list (void)
{
	// Get rid of the old report...
	SCRIPTING_reset_watch_list_file();

	// Read in the ones to watch...

	char *script_name_pointer;
	char *variable_name_pointer;

	char line[MAX_LINE_SIZE];
	bool exitmainloop = false;
	char *pointer;
	
	FILE *file_pointer = fopen ("watch_list.txt","r");

	if (file_pointer != NULL)
	{
		while ( (fgets ( line , MAX_LINE_SIZE , file_pointer ) != NULL) && (exitmainloop == false) )
		{
			STRING_strip_newlines(line);
			STRING_uppercase(line);

			pointer = STRING_end_of_string(line,"#END OF FILE");
			if (pointer != NULL)
			{
				exitmainloop = true;
			}

			pointer = STRING_end_of_string(line,"#WATCH = ");
			if (pointer != NULL)
			{
				script_name_pointer = strtok (pointer," \n");
				variable_name_pointer = strtok (NULL," \n");
				SCRIPTING_add_to_watch_list (script_name_pointer,variable_name_pointer);
			}
		}

		fclose(file_pointer);
	}
	else
	{
		// No watch list, innit!
	}
}
