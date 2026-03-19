#include <assert.h>
#include <stdio.h>
#include "savegame.h"
#include "arrays.h"
#include "output.h"
#include "global_param_list.h"
#include "string_size_constants.h"
#include "main.h"

typedef struct
{
	int loaded_tag;
	int live_entity_id;
} restored_entity_mapping_struct;

static restored_entity_mapping_struct *savegame_restored_entity_mappings = NULL;
static int savegame_restored_entity_mapping_count = 0;
static int savegame_restored_entity_mapping_capacity = 0;
static bool savegame_restored_entity_flags[MAX_ENTITIES];
static int savegame_generic_level_enemy_script = UNSET;
static int savegame_solid_diamond_overlay_script = UNSET;
static int savegame_fuzz_overlay_script = UNSET;
static int savegame_enemy_type_solid_diamonds = UNSET;
static int savegame_enemy_type_solid_diamonds_deviant = UNSET;
static int savegame_enemy_type_fuzz = UNSET;
static int savegame_main_game_controller_flag = UNSET;

static bool SAVEGAME_compare_value(int value, int operation, int compare_value)
{
	switch (operation)
	{
	case CMP_LESS_THAN:
		return (value < compare_value);

	case CMP_LESS_THAN_OR_EQUAL:
		return (value <= compare_value);

	case CMP_EQUAL:
		return (value == compare_value);

	case CMP_GREATER_THAN_OR_EQUAL:
		return (value >= compare_value);

	case CMP_GREATER_THAN:
		return (value > compare_value);

	case CMP_UNEQUAL:
		return (value != compare_value);

	case CMP_BITWISE_AND:
		return (value & compare_value);

	case CMP_BITWISE_OR:
		return (value | compare_value);

	case CMP_NOT_BITWISE_AND:
		return !(value & compare_value);

	case CMP_NOT_BITWISE_OR:
		return !(value | compare_value);

	default:
		return false;
	}
}

static bool SAVEGAME_is_entity_reference_variable(int variable_number)
{
	switch (variable_number)
	{
	case ENT_PARENT:
	case ENT_FIRST_CHILD:
	case ENT_LAST_CHILD:
	case ENT_PREV_SIBLING:
	case ENT_NEXT_SIBLING:
	case ENT_MATRIARCH:
	case ENT_COLLIDED_ENTITY:
	case ENT_TARGET_ENTITY:
	case ENT_DRAW_BUDDY:
		return true;

	default:
		return false;
	}
}

static bool SAVEGAME_is_generic_enemy_redundant_default_variable(int variable_number)
{
	switch (variable_number)
	{
	case ENT_DRAW_OVERRIDE:
	case ENT_SPECIAL_ORDER:
	case ENT_BASE_X:
	case ENT_BASE_Y:
	case ENT_BASE_Z:
	case ENT_RADIUS:
	case ENT_ANGULAR_VEL:
	case ENT_TARGET_ANGLE:
	case ENT_DAMAGE_TYPE:
	case ENT_HEALTH_DESENSITIZED:
	case ENT_HEALTH_SUPERSENSITIZED:
	case ENT_DESENSITIZED_SCALE:
	case ENT_SUPERSENSITIZED_SCALE:
	case ENT_SECONDARY_SPRITE:
	case ENT_SECONDARY_CURRENT_FRAME:
	case ENT_SECONDARY_INTERPOLATED_FRAME:
	case ENT_SECONDARY_INTERPOLATION_X_PERCENTAGE:
	case ENT_SECONDARY_INTERPOLATION_Y_PERCENTAGE:
	case ENT_SECONDARY_OPENGL_BOOLEANS:
	case ENT_OPENGL_SECONDARY_SCALE_X:
	case ENT_OPENGL_SECONDARY_SCALE_Y:
	case ENT_OPENGL_SECONDARY_ANGLE:
	case ENT_FIXED_SWITCH_LINE:
	case ENT_BASIC_SCENERY_COLLISION_FLAG:
	case ENT_PLATFORM_COLLISION_FLAG:
	case ENT_MAINTAIN_VELOCITY_X:
	case ENT_MAINTAIN_VELOCITY_Y:
	case ENT_PHYSICS_OBJECT_ARCHETYPE:
	case ENT_PHYSICS_OBJECT_POINTER:
	case ENT_HELPER_FLAGS:
	case ENT_HELPER_TAG_TYPE:
	case ENT_HELPER_TAG_X_OFFSET:
	case ENT_HELPER_TAG_Y_OFFSET:
	case ENT_TILEMAP_POSITION_X:
	case ENT_TILEMAP_POSITION_Y:
	case ENT_TILEMAP_LAYER:
	case ENT_TILEMAP_NUMBER:
	case ENT_EXTERNAL_CONVEY_X:
	case ENT_EXTERNAL_CONVEY_Y:
	case ENT_EXTERNAL_ACCELL_X:
	case ENT_EXTERNAL_ACCELL_Y:
	case ENT_PRIORITY:
	case ENT_DISTANCE_FROM_TARGETTER:
	case ENT_NEXT_IN_SORT:
	case ENT_PREV_IN_SORT:
	case ENT_TEMP_SORT_VAR:
	case ENT_UNIQUE_ID:
	case ENT_GUN_1_POSITION_OFFSET:
	case ENT_GUN_2_POSITION_OFFSET:
	case ENT_GUN_1_AMMO_COUNT:
	case ENT_GUN_2_AMMO_COUNT:
	case ENT_FACING_DIRECTION:
	case ENT_EXTRA_ABILITIES:
	case ENT_HEALTH:
	case ENT_PAIN_THRESHOLD:
	case ENT_DAMAGE:
	case ENT_DAMAGE_RECEIVED:
	case ENT_STATE:
	case ENT_BOOLEAN_PROPERTIES:
	case ENT_JOLT:
	case ENT_SPEED:
	case ENT_ACCEL:
	case ENT_DROP_ITEM:
	case ENT_JUMP_SPEED:
	case ENT_MAXIMUM_FALL_HEIGHT:
	case ENT_OLD_X_VEL:
	case ENT_OLD_Y_VEL:
	case ENT_TEXT_TAG_STORE:
	case ENT_POINT_VALUE:
	case ENT_CURRENT_ZONE:
	case ENT_OLD_ZONE:
	case ENT_SIGNAL:
	case ENT_EXTERNAL_SIGNAL:
	case ENT_CHANCE:
	case ENT_DIFFICULTY:
	case ENT_DAMAGE_TYPES_RECEIVED:
	case ENT_DEBUG_INFO:
	case ENT_ACTOR_NAME:
	case ENT_ACTOR_GROUP:
	case ENT_OVERRIDE_PIVOT_X:
	case ENT_OVERRIDE_PIVOT_Y:
	case ENT_SKEW_X:
	case ENT_SKEW_Y:
	case ENT_RETURNED_VALUE_1:
	case ENT_RETURNED_VALUE_2:
	case ENT_RETURNED_VALUE_3:
	case ENT_RETURNED_VALUE_4:
	case ENT_CUT_SPRITE_LEFT_INDENTATION:
	case ENT_CUT_SPRITE_RIGHT_INDENTATION:
	case ENT_CUT_SPRITE_TOP_INDENTATION:
	case ENT_CUT_SPRITE_BOTTOM_INDENTATION:
	case ENT_DEBUG_FLAGS:
		return true;

	default:
		return false;
	}
}

static int SAVEGAME_get_slept_in_transition_entity_type(void)
{
	static int cached_entity_type = UNSET;

	if (cached_entity_type == UNSET)
	{
		cached_entity_type = GPL_find_word_value("CONSTANT", "ENT_TYPE_SLEPT_IN_LEVEL_TRANSITION");
	}

	return cached_entity_type;
}

static void SAVEGAME_cache_script_and_enemy_constants(void)
{
	if (savegame_generic_level_enemy_script == UNSET)
	{
		savegame_generic_level_enemy_script = GPL_find_word_value("SCRIPTS", "GENERIC_LEVEL_ENEMY");
		savegame_solid_diamond_overlay_script = GPL_find_word_value("SCRIPTS", "SOLID_DIAMOND_OVERLAY");
		savegame_fuzz_overlay_script = GPL_find_word_value("SCRIPTS", "FUZZ_OVERLAY");
		savegame_enemy_type_solid_diamonds = GPL_find_word_value("CONSTANT", "ENEMY_TYPE_SOLID_DIAMONDS");
		savegame_enemy_type_solid_diamonds_deviant = GPL_find_word_value("CONSTANT", "ENEMY_TYPE_SOLID_DIAMONDS_DEVIANT");
		savegame_enemy_type_fuzz = GPL_find_word_value("CONSTANT", "ENEMY_TYPE_FUZZ");
		savegame_main_game_controller_flag = GPL_find_word_value("FLAG", "MAIN_GAME_CONTROLLER_ENTITY_ID");
	}
}

bool SAVEGAME_should_output_entity_variable(int ent_index, int var_index)
{
	SAVEGAME_cache_script_and_enemy_constants();

	// Save files are currently dominated by generic_level_enemy state, so we do two
	// conservative size reductions here:
	// 1. raw entity-id references are skipped because references are already serialized
	//    separately by stable save tags and restored from that section.
	// 2. for GENERIC_LEVEL_ENEMY only, a small blocklist of obvious housekeeping fields
	//    is skipped, but only when the value still matches the engine reset/default.
	//
	// This is intentionally cautious. If future work trims more aggressively, keep the
	// continue-system requirement in mind: resumed enemies must preserve logical level
	// state across saves, especially script flow, movement/path state, wake behavior,
	// and cross-level persistence inside the active level window.
	if (SAVEGAME_is_entity_reference_variable(var_index))
	{
		return false;
	}

	if (entity[ent_index][ENT_SCRIPT_NUMBER] == savegame_generic_level_enemy_script)
	{
		if (SAVEGAME_is_generic_enemy_redundant_default_variable(var_index))
		{
			if (entity[ent_index][var_index] == reset_entity[var_index])
			{
				return false;
			}
		}
	}

	return true;
}

static bool SAVEGAME_is_parent_draw_buddy_entity(int entity_id)
{
	int parent_id;

	if ((entity_id < 0) || (entity_id >= MAX_ENTITIES))
	{
		return false;
	}

	parent_id = entity[entity_id][ENT_PARENT];

	if ((parent_id < 0) || (parent_id >= MAX_ENTITIES))
	{
		return false;
	}

	return (entity[parent_id][ENT_DRAW_BUDDY] == entity_id);
}

static bool SAVEGAME_is_active_window_level_entity(int entity_id)
{
	int controller_id;
	int min_open_level;
	int max_open_level;
	int parent_level;

	SAVEGAME_cache_script_and_enemy_constants();

	if (savegame_main_game_controller_flag == UNSET)
	{
		return true;
	}

	controller_id = flag_array[savegame_main_game_controller_flag];

	if ((controller_id < 0) || (controller_id >= MAX_ENTITIES))
	{
		return true;
	}

	min_open_level = entity[controller_id][ENT_V38];
	max_open_level = entity[controller_id][ENT_V26];
	parent_level = entity[entity_id][ENT_PARENT_LEVEL];

	return ((parent_level >= min_open_level) && (parent_level <= max_open_level));
}

static void SAVEGAME_restore_missing_enemy_draw_buddy(int entity_id)
{
	int behaviour_type;
	int overlay_script = UNSET;
	int overlay_entity_id;

	SAVEGAME_cache_script_and_enemy_constants();

	if ((entity_id < 0) || (entity_id >= MAX_ENTITIES))
	{
		return;
	}

	if (entity[entity_id][ENT_SCRIPT_NUMBER] != savegame_generic_level_enemy_script)
	{
		return;
	}

	if (entity[entity_id][ENT_DRAW_BUDDY] != UNSET)
	{
		return;
	}

	behaviour_type = entity[entity_id][ENT_PASSED_PARAM_11];

	if ((behaviour_type == savegame_enemy_type_solid_diamonds) ||
		(behaviour_type == savegame_enemy_type_solid_diamonds_deviant))
	{
		overlay_script = savegame_solid_diamond_overlay_script;
	}
	else if (behaviour_type == savegame_enemy_type_fuzz)
	{
		overlay_script = savegame_fuzz_overlay_script;
	}

	if (overlay_script == UNSET)
	{
		return;
	}

	overlay_entity_id = SCRIPTING_spawn_entity(entity_id, overlay_script, 0, 0, PROCESS_NEXT, UNSET);

	if (overlay_entity_id != UNSET)
	{
		entity[entity_id][ENT_DRAW_BUDDY] = overlay_entity_id;
	}
}

static void SAVEGAME_reset_restored_entity_mappings(void)
{
	if (savegame_restored_entity_mappings != NULL)
	{
		free(savegame_restored_entity_mappings);
		savegame_restored_entity_mappings = NULL;
	}

	savegame_restored_entity_mapping_count = 0;
	savegame_restored_entity_mapping_capacity = 0;
}

void SAVEGAME_clear_restored_live_entity_flags(void)
{
	memset(savegame_restored_entity_flags, 0, sizeof(savegame_restored_entity_flags));
}

static void SAVEGAME_add_restored_entity_mapping(int loaded_tag, int live_entity_id)
{
	if (savegame_restored_entity_mapping_count >= savegame_restored_entity_mapping_capacity)
	{
		int new_capacity = savegame_restored_entity_mapping_capacity + 32;
		savegame_restored_entity_mappings = (restored_entity_mapping_struct *) realloc(
			savegame_restored_entity_mappings,
			sizeof(restored_entity_mapping_struct) * new_capacity);
		savegame_restored_entity_mapping_capacity = new_capacity;
	}

	savegame_restored_entity_mappings[savegame_restored_entity_mapping_count].loaded_tag = loaded_tag;
	savegame_restored_entity_mappings[savegame_restored_entity_mapping_count].live_entity_id = live_entity_id;
	savegame_restored_entity_mapping_count++;
	if ((live_entity_id >= 0) && (live_entity_id < MAX_ENTITIES))
	{
		savegame_restored_entity_flags[live_entity_id] = true;
	}
}

static int SAVEGAME_find_restored_live_entity_for_tag(int loaded_tag)
{
	int mapping_number;

	for (mapping_number = 0; mapping_number < savegame_restored_entity_mapping_count; mapping_number++)
	{
		if (savegame_restored_entity_mappings[mapping_number].loaded_tag == loaded_tag)
		{
			return savegame_restored_entity_mappings[mapping_number].live_entity_id;
		}
	}

	return UNSET;
}

static void SAVEGAME_restore_entity_variables(int entity_number, loaded_entity_struct *loaded_entity)
{
	int variable_number;
	int loaded_alive = UNSET;
	int loaded_old_alive = UNSET;
	int loaded_entity_type = UNSET;
	int slept_in_transition_type = SAVEGAME_get_slept_in_transition_entity_type();

	for (variable_number = 0; variable_number < loaded_entity->loaded_variable_count; variable_number++)
	{
		if (loaded_entity->loaded_entity_variable_list[variable_number] == ENT_ALIVE)
		{
			loaded_alive = loaded_entity->loaded_entity_value_list[variable_number];
		}

		if (loaded_entity->loaded_entity_variable_list[variable_number] == ENT_OLD_ALIVE)
		{
			loaded_old_alive = loaded_entity->loaded_entity_value_list[variable_number];
		}

		if (loaded_entity->loaded_entity_variable_list[variable_number] == ENT_ENTITY_TYPE)
		{
			loaded_entity_type = loaded_entity->loaded_entity_value_list[variable_number];
		}
	}

	for (variable_number = 0; variable_number < loaded_entity->loaded_variable_count; variable_number++)
	{
		int variable_index = loaded_entity->loaded_entity_variable_list[variable_number];
		int variable_value = loaded_entity->loaded_entity_value_list[variable_number];

			if (!SAVEGAME_is_entity_reference_variable(variable_index))
			{
				// Save-and-exit can happen while the game is paused, so pausable entities may
				// be serialized as FROZEN. Resume should restore their live pre-pause state.
				if ((variable_index == ENT_ALIVE) && (variable_value == FROZEN) && (loaded_old_alive != UNSET))
				{
					variable_value = loaded_old_alive;
				}

				// If a slept-transition entity was paused while already sleeping, the freeze step
				// will have overwritten OLD_ALIVE with SLEEPING. Restore the semantic wake target
				// so later level transitions can actually wake it back up.
				if ((variable_index == ENT_OLD_ALIVE) &&
					(loaded_alive == FROZEN) &&
					(loaded_old_alive == SLEEPING) &&
					(slept_in_transition_type != UNSET) &&
					(loaded_entity_type & slept_in_transition_type))
				{
					variable_value = ALIVE;
				}

				entity[entity_number][variable_index] = variable_value;
			}
		}

	// Ensure entities restored in a sleeping state will resume from their wake handler
	// when later level transitions wake them again.
	if ((entity[entity_number][ENT_ALIVE] == SLEEPING) &&
		(entity[entity_number][ENT_WAKE_LINE] != UNSET))
	{
		entity[entity_number][ENT_PROGRAM_START] = entity[entity_number][ENT_WAKE_LINE];
	}
}



static void SAVEGAME_restore_entity_arrays(int entity_number, loaded_entity_struct *loaded_entity)
{
	int array_number;
	int value_index;
	int array_total_size;
	loaded_array_struct *loaded_array;

	ARRAY_destroy_entitys_arrays(entity_number);

	for (array_number = 0; array_number < loaded_entity->loaded_entity_array_count; array_number++)
	{
		loaded_array = &loaded_entity->array_data[array_number];
		array_total_size = loaded_array->width * loaded_array->height * loaded_array->depth;

		ARRAY_create_array(entity_number, loaded_array->uid, loaded_array->width, loaded_array->height, loaded_array->depth);

		for (value_index = 0; value_index < array_total_size; value_index++)
		{
			int width_times_height = loaded_array->width * loaded_array->height;
			int z = value_index / width_times_height;
			int remainder = value_index % width_times_height;
			int y = remainder / loaded_array->width;
			int x = remainder % loaded_array->width;

			ARRAY_write_value(entity_number, loaded_array->uid, loaded_array->data[value_index], x, y, z);
		}
	}
}

static void SAVEGAME_restore_entity_references(int entity_number, loaded_entity_struct *loaded_entity)
{
	int reference_number;

	for (reference_number = 0; reference_number < loaded_entity->loaded_reference_count; reference_number++)
	{
		int variable_index = loaded_entity->loaded_entity_reference_variable_list[reference_number];
		int loaded_tag = loaded_entity->loaded_entity_reference_tag_list[reference_number];
		int resolved_entity_id = UNSET;

		if (loaded_tag != UNSET)
		{
			resolved_entity_id = SAVEGAME_find_restored_live_entity_for_tag(loaded_tag);
		}

		entity[entity_number][variable_index] = resolved_entity_id;
	}
}



loaded_entity_struct *SAVEGAME_find_loaded_entity_by_tag(int tag)
{
	int loaded_entity_number;

	for (loaded_entity_number = 0; loaded_entity_number < save_data.loaded_entity_count; loaded_entity_number++)
	{
		if (save_data.loaded_entity_data[loaded_entity_number].loaded_entity_tag == tag)
		{
			return &save_data.loaded_entity_data[loaded_entity_number];
		}
	}

	return NULL;
}



bool SAVEGAME_restore_entity_from_loaded_tag(int entity_number, int tag)
{
	loaded_entity_struct *loaded_entity = SAVEGAME_find_loaded_entity_by_tag(tag);

	if (loaded_entity == NULL)
	{
		char error_line[MAX_LINE_SIZE];

		snprintf(error_line, sizeof(error_line),
			"Save-game restore could not find matching entity tag %i. Loaded entity count = %i.",
			tag, save_data.loaded_entity_count);
		OUTPUT_message(error_line);

		return false;
	}

	if ((entity_number < 0) || (entity_number >= MAX_ENTITIES))
	{
		OUTPUT_message("Save-game restore received invalid entity id!");
		assert(0);
		return false;
	}

	SAVEGAME_restore_entity_variables(entity_number, loaded_entity);
	SAVEGAME_restore_entity_arrays(entity_number, loaded_entity);
	SAVEGAME_add_restored_entity_mapping(tag, entity_number);
	SAVEGAME_restore_entity_references(entity_number, loaded_entity);

	return true;
}

int SAVEGAME_find_saved_tag_for_live_entity(int entity_number)
{
	int saved_entity_number;

	for (saved_entity_number = 0; saved_entity_number < save_data.saved_entity_count; saved_entity_number++)
	{
		if (save_data.saved_entity_number_list[saved_entity_number] == entity_number)
		{
			return save_data.saved_entity_tag_list[saved_entity_number];
		}
	}

	return UNSET;
}

void SAVEGAME_output_entity_references_to_file(int ent_index, FILE *file_pointer)
{
	int variable_index;
	int reference_count = 0;
	char line[MAX_LINE_SIZE];

	for (variable_index = 0; variable_index < MAX_ENTITY_VARIABLES; variable_index++)
	{
		if (SAVEGAME_is_entity_reference_variable(variable_index))
		{
			reference_count++;
		}
	}

	snprintf(line, sizeof(line), "\t\t#START_OF_REFERENCES_COUNT = %i\n", reference_count);
	fputs(line, file_pointer);

	for (variable_index = 0; variable_index < MAX_ENTITY_VARIABLES; variable_index++)
	{
		if (SAVEGAME_is_entity_reference_variable(variable_index))
		{
			int referenced_entity_id = entity[ent_index][variable_index];
			int referenced_tag = UNSET;

			if ((referenced_entity_id >= 0) && (referenced_entity_id < MAX_ENTITIES))
			{
				referenced_tag = SAVEGAME_find_saved_tag_for_live_entity(referenced_entity_id);
			}

			snprintf(
				line,
				sizeof(line),
				"\t\t\t#ENTITY_REFERENCE '%s' = %i\n",
				GPL_get_entry_name("VARIABLE", variable_index),
				referenced_tag);
			fputs(line, file_pointer);
		}
	}

	fputs("\t\t#END_OF_REFERENCES\n", file_pointer);
}



int SAVEGAME_save_matching_live_entities(int variable, int operation, int compare_value, int tag_offset)
{
	int saved_count = 0;
	int entity_id;

	for (entity_id = 0; entity_id < MAX_ENTITIES; entity_id++)
	{
		int alive_state = entity[entity_id][ENT_ALIVE];

		if (alive_state > DEAD)
		{
			int value = entity[entity_id][variable];

			if (SAVEGAME_compare_value(value, operation, compare_value))
			{
				if (SAVEGAME_is_parent_draw_buddy_entity(entity_id) == false)
				{
					bool within_active_window = true;

					if ((variable == ENT_ENTITY_TYPE) &&
						(operation == CMP_BITWISE_AND) &&
						(compare_value == SAVEGAME_get_slept_in_transition_entity_type()))
					{
						within_active_window = SAVEGAME_is_active_window_level_entity(entity_id);
					}

					if (within_active_window)
					{
						SCRIPTING_save_entity(entity_id, tag_offset + entity_id);
						saved_count++;
					}
				}
			}
		}
	}

	return saved_count;
}



int SAVEGAME_spawn_matching_loaded_entities(int variable, int operation, int compare_value)
{
	int loaded_entity_number;
	int spawned_count = 0;

	SAVEGAME_reset_restored_entity_mappings();

	for (loaded_entity_number = 0; loaded_entity_number < save_data.loaded_entity_count; loaded_entity_number++)
	{
		loaded_entity_struct *loaded_entity = &save_data.loaded_entity_data[loaded_entity_number];
		bool matched = false;
		int variable_number;

		for (variable_number = 0; variable_number < loaded_entity->loaded_variable_count; variable_number++)
		{
			if (loaded_entity->loaded_entity_variable_list[variable_number] == variable)
			{
				int value = loaded_entity->loaded_entity_value_list[variable_number];
				matched = SAVEGAME_compare_value(value, operation, compare_value);
				break;
			}
		}

		if (matched)
		{
			int new_entity_id = SCRIPTING_spawn_restored_entity_last();

			if (new_entity_id != UNSET)
			{
				SAVEGAME_restore_entity_variables(new_entity_id, loaded_entity);
				SAVEGAME_restore_entity_arrays(new_entity_id, loaded_entity);
				SAVEGAME_add_restored_entity_mapping(loaded_entity->loaded_entity_tag, new_entity_id);
				spawned_count++;
			}
		}
	}

	for (loaded_entity_number = 0; loaded_entity_number < save_data.loaded_entity_count; loaded_entity_number++)
	{
		loaded_entity_struct *loaded_entity = &save_data.loaded_entity_data[loaded_entity_number];
		int live_entity_id = SAVEGAME_find_restored_live_entity_for_tag(loaded_entity->loaded_entity_tag);

		if (live_entity_id != UNSET)
		{
			SAVEGAME_restore_entity_references(live_entity_id, loaded_entity);
		}
	}

	for (loaded_entity_number = 0; loaded_entity_number < save_data.loaded_entity_count; loaded_entity_number++)
	{
		loaded_entity_struct *loaded_entity = &save_data.loaded_entity_data[loaded_entity_number];
		int live_entity_id = SAVEGAME_find_restored_live_entity_for_tag(loaded_entity->loaded_entity_tag);

		if (live_entity_id != UNSET)
		{
			SAVEGAME_restore_missing_enemy_draw_buddy(live_entity_id);
		}
	}

	SAVEGAME_reset_restored_entity_mappings();

	return spawned_count;
}
