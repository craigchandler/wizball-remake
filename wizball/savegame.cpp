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
	int loaded_old_alive = UNSET;

	for (variable_number = 0; variable_number < loaded_entity->loaded_variable_count; variable_number++)
	{
		if (loaded_entity->loaded_entity_variable_list[variable_number] == ENT_OLD_ALIVE)
		{
			loaded_old_alive = loaded_entity->loaded_entity_value_list[variable_number];
			break;
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

			entity[entity_number][variable_index] = variable_value;
		}
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
				SCRIPTING_save_entity(entity_id, tag_offset + entity_id);
				saved_count++;
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

	SAVEGAME_reset_restored_entity_mappings();

	return spawned_count;
}
