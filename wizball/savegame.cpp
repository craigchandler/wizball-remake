#include <assert.h>
#include <stdio.h>
#include "savegame.h"
#include "arrays.h"
#include "output.h"

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

static void SAVEGAME_restore_entity_variables(int entity_number, loaded_entity_struct *loaded_entity)
{
	int variable_number;

	for (variable_number = 0; variable_number < loaded_entity->loaded_variable_count; variable_number++)
	{
		int variable_index = loaded_entity->loaded_entity_variable_list[variable_number];
		int variable_value = loaded_entity->loaded_entity_value_list[variable_number];

		entity[entity_number][variable_index] = variable_value;
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
		OUTPUT_message("Save-game restore could not find matching entity tag!");
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

	return true;
}



int SAVEGAME_save_matching_live_entities(int variable, int operation, int compare_value, int tag_offset)
{
	int entity_id = first_processed_entity_in_list;
	int saved_count = 0;

	while (entity_id != UNSET)
	{
		int next_entity_id = entity[entity_id][ENT_NEXT_PROCESS_ENT];
		int value = entity[entity_id][variable];

		if (SAVEGAME_compare_value(value, operation, compare_value))
		{
			SCRIPTING_save_entity(entity_id, tag_offset + entity_id);
			saved_count++;
		}

		entity_id = next_entity_id;
	}

	return saved_count;
}



int SAVEGAME_spawn_matching_loaded_entities(int variable, int operation, int compare_value)
{
	int loaded_entity_number;
	int spawned_count = 0;

	for (loaded_entity_number = 0; loaded_entity_number < save_data.loaded_entity_count; loaded_entity_number++)
	{
		loaded_entity_struct *loaded_entity = &save_data.loaded_entity_data[loaded_entity_number];
		int variable_number;
		bool matched = false;

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
				SAVEGAME_restore_entity_from_loaded_tag(new_entity_id, loaded_entity->loaded_entity_tag);
				spawned_count++;
			}
		}
	}

	return spawned_count;
}
