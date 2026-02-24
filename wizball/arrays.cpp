#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <allegro.h>
#ifdef ALLEGRO_MACOSX
#include <CoreServices/CoreServices.h>
#endif
#include "arrays.h"
#include "textfiles.h"
#include "global_param_list.h"
#include "main.h"
#include "output.h"
#include "scripting.h"
#include "parser.h" // For the datatable struct.
#include "math_stuff.h"

#include "fortify.h"

entity_array_holder ent_arrays[MAX_ENTITIES];

datatable_struct *datatable_data = NULL;
int number_of_datatables = 0;

int total_special_path_percent_size = 1000000;




int ARRAY_get_array_number (int entity_number, int array_unique_id)
{
	int array_number;

	if (ent_arrays[entity_number].array_count > 0)
	{
		for (array_number=0; array_number<ent_arrays[entity_number].array_count; array_number++)
		{
			if (ent_arrays[entity_number].array_pointer[array_number].unique_id == array_unique_id)
			{
				return array_number;
			}
		}
	}
	else
	{
		OUTPUT_message("This entity has no arrays!");
		assert(0);
		return UNSET;
	}

	OUTPUT_message("Array id not found!");
	assert(0);
	return UNSET;
}



int ARRAY_allocate_array_header (int entity_number)
{
	if (ent_arrays[entity_number].array_pointer == NULL)
	{
		// None at the moment, malloc-me-do!

		ent_arrays[entity_number].array_pointer = (entity_array *) malloc (sizeof(entity_array));
	}
	else
	{
		ent_arrays[entity_number].array_pointer = (entity_array *) realloc ( ent_arrays[entity_number].array_pointer , sizeof(entity_array) * (ent_arrays[entity_number].array_count+1) );
	}

	ent_arrays[entity_number].array_count++;

	return (ent_arrays[entity_number].array_count-1);
}



void ARRAY_resize_array (int entity_number, int unique_id, int width, int height, int depth)
{
	int total_size = width*height*depth;
	int width_times_height = width*height;

	int *values = (int *) malloc (sizeof(int) * total_size);

	int element;

	for (element=0; element<total_size; element++)
	{
		values[element] = 0;
	}

	// Now we copy all the old shite over...

	int array_number = ARRAY_get_array_number ( entity_number , unique_id );

	entity_array *ap = &ent_arrays[entity_number].array_pointer[array_number];

	int min_width = MATH_smallest_int (width, ap->width);
	int min_height = MATH_smallest_int (height, ap->height);
	int min_depth = MATH_smallest_int (depth, ap->depth);

	int x,y,z;
	int from_offset, to_offset;
	
	for (x=0; x<min_width; x++)
	{
		for (y=0; y<min_height; y++)
		{
			for (z=0; z<min_depth; z++)
			{
				from_offset = (z * ap->width_times_height) + (y * ap->width) + x;
				to_offset = (z * width_times_height) + (y * width) + x;

				values[to_offset] = ap->values[from_offset];
			}
		}
	}

	free (ap->values);
	ap->values = values;

	ap->width = width;
	ap->height = height;
	ap->depth = depth;
	ap->total_size = total_size;
	ap->width_times_height = width_times_height;
}



void ARRAY_create_array (int entity_number, int unique_id, int width, int height, int depth)
{
	int array_number = ARRAY_allocate_array_header (entity_number);

	entity_array *ap = &ent_arrays[entity_number].array_pointer[array_number];

	ap->unique_id = unique_id;

	ap->width = width;
	ap->height = height;
	ap->depth = depth;
	ap->total_size = width*height*depth;
	ap->width_times_height = width*height;

	ap->values = (int *) malloc (sizeof(int) * ap->total_size);

	int element;

	for (element=0; element<ap->total_size; element++)
	{
		ap->values[element] = 0;
	}
}



void ARRAY_reset_array (int entity_number, int unique_id, int value)
{
	int array_number = ARRAY_get_array_number ( entity_number , unique_id );

	entity_array *ap = &ent_arrays[entity_number].array_pointer[array_number];

	int element;

	for (element=0; element<ap->total_size; element++)
	{
		ap->values[element] = value;
	}
}



int ARRAY_add_text_number_to_array (int entity_number, int unique_id, int number_value, int start_offset, bool add_a_space)
{
	int array_number = ARRAY_get_array_number ( entity_number , unique_id );

	entity_array *ap = &ent_arrays[entity_number].array_pointer[array_number];

	char text_number[MAX_LINE_SIZE];
	sprintf(text_number,"%i",number_value);

	char *c_pointer = &text_number[0];

	int length = strlen(c_pointer);

	if (start_offset+length+(add_a_space?2:1) > ap->total_size)
	{
		OUTPUT_message("Text to be added to array will overflow - increase array size and retry!");
		assert(0);
		return UNSET;
	}
	else
	{
		int counter;

		int *i_pointer = &ap->values[start_offset];

		for (counter=0; counter<length; counter++)
		{
			i_pointer[counter] = *c_pointer;
			c_pointer++;
		}

		if (add_a_space)
		{
			i_pointer[counter] = ' ';
			i_pointer[counter+1] = 0;
			length++;
		}
		else
		{
			i_pointer[counter] = 0;
		}

		return (start_offset+length);
	}
}



int ARRAY_add_text_to_array (int entity_number, int unique_id, int text_tag, int start_offset, bool add_a_space)
{
	int array_number = ARRAY_get_array_number ( entity_number , unique_id );

	entity_array *ap = &ent_arrays[entity_number].array_pointer[array_number];

	char *c_pointer = TEXTFILE_get_line_by_index (text_tag);

	int length = strlen(c_pointer);

	if (start_offset+length+(add_a_space?2:1) > ap->total_size)
	{
		OUTPUT_message("Text to be added to array will overflow - increase array size and retry!");
		assert(0);
		return UNSET;
	}
	else
	{
		int counter;

		int *i_pointer = &ap->values[start_offset];

		for (counter=0; counter<length; counter++)
		{
			i_pointer[counter] = *c_pointer;
			c_pointer++;
		}

		if (add_a_space)
		{
			i_pointer[counter] = ' ';
			i_pointer[counter+1] = 0;
			length++;
		}
		else
		{
			i_pointer[counter] = 0;
		}

		return (start_offset+length);
	}
}



char * ARRAY_get_array_as_text (int entity_number, int array_unique_id, int length=UNSET)
{
	// This interprets each of the numbers in an array as ascii characters and builds a string out of them.

	int array_number = ARRAY_get_array_number (entity_number,  array_unique_id);
	entity_array *ap = &ent_arrays[entity_number].array_pointer[array_number];

//	int *num_pointer = ap->values;

	static char text[MAX_LINE_SIZE];

	if (length == UNSET)
	{
		// If the length hasn't been set then we will determine it from the array by looking
		// for the right 0 value in it.

		for (length=0; (length<ap->total_size) && (ap->values[length]!=0); length++)
		{
			// Do nothing...
		}
	}
	else
	{
		if (length > ap->total_size)
		{
			length = ap->total_size;
		}
	}

	int n;

	for (n=0; n<length; n++)
	{
		text[n] = char(ap->values[n]);
	}

	text[length] = '\0';

	return (text);
}



int ARRAY_read_value (int entity_number, int array_unique_id, int x, int y, int z)
{
	int array_number = ARRAY_get_array_number (entity_number,  array_unique_id);
	entity_array *ap = &ent_arrays[entity_number].array_pointer[array_number];
	int *num_pointer = ap->values;
	int offset;

	if ( (x >= 0) && (y >= 0) && (z >= 0) && (x < ap->width) && (y < ap->height) && (z < ap->depth) )
	{
		offset = (z * ap->width_times_height) + (y * ap->width) + x;
		return (num_pointer[offset]);
	}
	else
	{
		char error[MAX_LINE_SIZE];
		sprintf(error,"Reading (%ix%ix%i) outside of array which is (%ix%ix%i)!",x,y,z,ap->width,ap->height,ap->depth);
		OUTPUT_message(error);
	}

	return UNSET;
}



void ARRAY_write_value (int entity_number, int array_unique_id, int value, int x, int y, int z)
{
	int array_number = ARRAY_get_array_number (entity_number,  array_unique_id);
	entity_array *ap = &ent_arrays[entity_number].array_pointer[array_number];
	int *num_pointer = ap->values;

	if ( (x < ap->width) && (y < ap->height) && (z < ap->depth) )
	{
		num_pointer[(z * ap->width_times_height) + (y * ap->width) + x] = value;
	}
	else
	{
		char error[MAX_LINE_SIZE];
		sprintf(error,"Writing (%ix%ix%i) outside of array which is (%ix%ix%i)!",x,y,z,ap->width,ap->height,ap->depth);
		OUTPUT_message(error);
	}
}



void ARRAY_setup_arrays (void)
{
	int array_number;

	for(array_number=0; array_number<MAX_ENTITIES; array_number++)
	{
		ent_arrays[array_number].array_pointer = NULL;
		ent_arrays[array_number].array_count = 0;
	}
}



void ARRAY_destroy_entitys_arrays (int entity_number)
{
	int array_number;

	if (ent_arrays[entity_number].array_count > 0)
	{
		for (array_number=0; array_number<ent_arrays[entity_number].array_count; array_number++)
		{
			free (ent_arrays[entity_number].array_pointer[array_number].values);
		}

		free (ent_arrays[entity_number].array_pointer);

		ent_arrays[entity_number].array_pointer = NULL;
		ent_arrays[entity_number].array_count = 0;
	}
}



void ARRAY_output_entity_arrays_to_file(int entity_number, FILE *file_pointer)
{
	int array_number;
	int value_index;
	int size;
	char line[MAX_LINE_SIZE];
	char word[MAX_LINE_SIZE];
	int *value_pointer;
	bool just_cleared;

	if (ent_arrays[entity_number].array_count > 0)
	{
		sprintf(line,"\t#ARRAY_COUNT = %i\n\n",ent_arrays[entity_number].array_count);
		fputs(line,file_pointer);

		for (array_number=0; array_number<ent_arrays[entity_number].array_count; array_number++)
		{
			fputs("\t\t#ARRAY_START\n",file_pointer);
			sprintf(line,"\t\t\t#ARRAY_SIZE = %i * %i * %i\n",ent_arrays[entity_number].array_pointer[array_number].width,ent_arrays[entity_number].array_pointer[array_number].height,ent_arrays[entity_number].array_pointer[array_number].depth);
			fputs(line,file_pointer);
			sprintf(line,"\t\t\t#ARRAY_UID = %i\n",ent_arrays[entity_number].array_pointer[array_number].unique_id);
			fputs(line,file_pointer);
			sprintf(line,"\t\t\t#ARRAY_DATA = ");

			size = ent_arrays[entity_number].array_pointer[array_number].total_size;
			value_pointer = ent_arrays[entity_number].array_pointer[array_number].values;

			for (value_index=0; value_index<size; value_index++)
			{
				sprintf(word,"%i,",value_pointer);
				value_pointer++;
				strcat(line,word);

				just_cleared = false;

				if (strlen(line) > MAX_LINE_SIZE-10)
				{
					line[strlen(line)-1] = '\0'; // Clip off the final comma.
					fputs(line,file_pointer);
					fputs("\n",file_pointer);
					sprintf(line,"\t\t\t#ARRAY_DATA = ");
					just_cleared = true;
				}
			}

			if (just_cleared == false)
			{
				// Data exists in the string...
				line[strlen(line)-1] = '\0'; // Clip off the final comma.
				fputs(line,file_pointer);
				fputs("\n",file_pointer);
			}

			fputs("\t\t#END_OF_THIS_ARRAY\n",file_pointer);
		}

		fputs("\t#ARRAY_DATA_END\n",file_pointer);
	}
}



void ARRAY_load_datatables (void)
{
	// This calculates the necessary memory needed for the datatables, mallocs a HUGE blimping chunk of it
	// and writes all the necessary values to it and then squirts it out to disc.

	// Oh, and then it frees it.

	int datatable_number;
	int index_number, i;
	int data_offset;

	FILE *fp = fopen(MAIN_get_project_filename("datatable.dat",false),"rb");

	int data_size;
	
	if (fp != NULL)
	{
		fread(&data_size,sizeof(int),1,fp);
#ifdef ALLEGRO_MACOSX
		data_size = EndianS32_LtoN(data_size);
#endif
		int *data_chunk = (int *) malloc (data_size * sizeof(int));

		fread(data_chunk,sizeof(int),data_size,fp);
#ifdef ALLEGRO_MACOSX
		for (i=0; i<data_size; ++i)
			data_chunk[i] = EndianS32_LtoN(data_chunk[i]);
#endif
		int *pointer = &data_chunk[0];

		number_of_datatables = *pointer++;

		datatable_data = (datatable_struct *) malloc (sizeof(datatable_struct) * number_of_datatables);

		for (datatable_number=0; datatable_number<number_of_datatables; datatable_number++)
		{
			datatable_data[datatable_number].line_size = *pointer++;
			datatable_data[datatable_number].lines = *pointer++;
			datatable_data[datatable_number].index_count = *pointer++;
			datatable_data[datatable_number].special_path_stage_pointer = NULL;

			datatable_data[datatable_number].index_list = (int *) malloc (sizeof(int) * datatable_data[datatable_number].index_count);
			datatable_data[datatable_number].data = (int *) malloc (sizeof(int) * datatable_data[datatable_number].line_size * datatable_data[datatable_number].lines);

			for (index_number=0; index_number<datatable_data[datatable_number].index_count; index_number++ )
			{
				datatable_data[datatable_number].index_list[index_number] = *pointer++;
			}

			for (data_offset=0 ; data_offset < datatable_data[datatable_number].line_size * datatable_data[datatable_number].lines ; data_offset++ )
			{
				datatable_data[datatable_number].data[data_offset] = *pointer++;
			}
		}

		free (data_chunk);

		fclose (fp);
		
	}
	else
	{
		assert(0);
	}
}



void ARRAY_destroy_datatables (void)
{
	// And now, the time has come, for us to face, the final curtain...

	int datatable_number;

	for (datatable_number=0; datatable_number<number_of_datatables; datatable_number++)
	{
		free (datatable_data[datatable_number].data);
		free (datatable_data[datatable_number].index_list);

		if (datatable_data[datatable_number].special_path_stage_pointer != NULL)
		{
			free(datatable_data[datatable_number].special_path_stage_pointer);
		}
	}

	free (datatable_data);
}



int ARRAY_get_datatable_line_count (int datatable_index)
{
	if (datatable_index<number_of_datatables)
	{
		return (datatable_data[datatable_index].lines);
	}
	else
	{
		assert(0);
	}

	return UNSET;
}



int ARRAY_get_datatable_line_length (int datatable_index)
{
	if (datatable_index<number_of_datatables)
	{
		return (datatable_data[datatable_index].line_size);
	}
	else
	{
		assert(0);
	}

	return UNSET;
}



int ARRAY_get_datatable_line_from_label (int datatable_index, int datatable_label_index)
{
	if (datatable_index<number_of_datatables)
	{
		if (datatable_label_index < datatable_data[datatable_index].index_count)
		{
			return (datatable_data[datatable_index].index_list[datatable_label_index]);
		}
	}
	else
	{
		assert(0);
	}

	return UNSET;
}



int ARRAY_get_datatable_line_count_from_label_to_next (int datatable_index, int datatable_label_index)
{
	if (datatable_index<number_of_datatables)
	{
		if (datatable_label_index < datatable_data[datatable_index].index_count-1)
		{
			return (datatable_data[datatable_index].index_list[datatable_label_index+1] - datatable_data[datatable_index].index_list[datatable_label_index]);
		}
		else
		{
			// Out of range of indices, probably no trailing dummy in the datatable.
			assert(0);
		}
	}
	else
	{
		assert(0);
	}

	return UNSET;
}



int ARRAY_read_datatable_value (int datatable_index, int datatable_line, int datatable_argument)
{
	if (datatable_index<number_of_datatables)
	{
		if ((datatable_line < datatable_data[datatable_index].lines) && (datatable_line >= 0))
		{
			if ((datatable_argument < datatable_data[datatable_index].line_size) && (datatable_argument >= 0))
			{
				return (datatable_data[datatable_index].data[datatable_line*datatable_data[datatable_index].line_size + datatable_argument]);
			}
		}
	}
	else
	{
		assert(0);
	}

	return UNSET;
}



void ARRAY_create_array_from_datatable (int entity_number, int unique_id, int datatable_index)
{
	int width,height,depth;

	width = ARRAY_get_datatable_line_count (datatable_index);
	height = ARRAY_get_datatable_line_length (datatable_index);
	depth = 1;

	ARRAY_create_array (entity_number, unique_id, width, height, depth);

	int x,y;

	for (y=0;y<height;y++)
	{
		for (x=0;x<width;x++)
		{
			ARRAY_write_value ( entity_number,  unique_id, ARRAY_read_datatable_value (datatable_index, x, y),  x,  y, 0);
		}
	}
}



int SPECPATH_get_path_stage (int datatable_index, int path_stage, int path_percentage)
{
	// This iterates through the stages in a path until it finds the one in which the percentage
	// passed resides.

	int excess_alarm = 0;

	if (path_percentage >= total_special_path_percent_size)
	{
		path_percentage %= total_special_path_percent_size;
	}

	if (path_stage == UNSET)
	{
		path_stage = 0;
		
		while (datatable_data[datatable_index].special_path_stage_pointer[path_stage].percentage_progress_to_start_of_stage + datatable_data[datatable_index].special_path_stage_pointer[path_stage].percentage_length_of_stage < path_percentage)
		{
			path_stage++;
		}
	}
	else
	{
		// We do know it, check if it's valid.
		if ( (path_stage < 0) || (path_stage >= datatable_data[datatable_index].lines) )
		{
			assert(0);
		}
		else if (path_percentage < datatable_data[datatable_index].special_path_stage_pointer[path_stage].percentage_progress_to_start_of_stage)
		{
			while (path_percentage < datatable_data[datatable_index].special_path_stage_pointer[path_stage].percentage_progress_to_start_of_stage) 
			{
				path_stage--;

				if (path_stage < 0)
				{
					// WTF?
					assert(0);
				}

//				excess_alarm++;
//				if (excess_alarm>20)
//				{
//					assert(0);
//				}
			}
		}
		else if (path_percentage > datatable_data[datatable_index].special_path_stage_pointer[path_stage].percentage_progress_to_start_of_stage + datatable_data[datatable_index].special_path_stage_pointer[path_stage].percentage_length_of_stage)
		{
			while (path_percentage > datatable_data[datatable_index].special_path_stage_pointer[path_stage].percentage_progress_to_start_of_stage + datatable_data[datatable_index].special_path_stage_pointer[path_stage].percentage_length_of_stage)
			{
				path_stage++;
				if (path_stage == datatable_data[datatable_index].lines)
				{
					// WTF?
					assert(0);
				}

//				excess_alarm++;
//				if (excess_alarm>20)
//				{
//					assert(0);
//				}
			}
		}
	}

	return path_stage;
}



int SPECPATH_get_path_position_offset_from_percentage (int datatable_index, int path_angle, int path_percentage, int *x_offset, int *y_offset, int path_stage)
{
	int path_loops = path_percentage / total_special_path_percent_size;

	if (path_loops>0)
	{
		path_percentage = path_percentage % total_special_path_percent_size;
	}

	path_stage = SPECPATH_get_path_stage (datatable_index, path_stage, path_percentage);

	special_path_stage_struct *spsp = &datatable_data[datatable_index].special_path_stage_pointer[path_stage];

	int this_angle;
	int this_x_offset,this_y_offset;

	float stage_sub_percentage = MATH_unlerp (float(spsp->percentage_progress_to_start_of_stage), float(spsp->percentage_progress_to_start_of_stage + spsp->percentage_length_of_stage) , float(path_percentage));

	if (spsp->stage_x_behaviour == SPECIAL_PATH_TABLE_BEHAVIOUR_LINEAR)
	{
		this_x_offset = int (MATH_lerp (0.0f, float(spsp->stage_x_magnitude), stage_sub_percentage));
	}
	else
	{
		this_angle = int (MATH_lerp (float(spsp->stage_x_start_angle), float(spsp->stage_x_start_angle + spsp->stage_x_period), stage_sub_percentage));
		this_x_offset = ((MATH_get_sin_table_value(this_angle) * spsp->stage_x_magnitude) / 10000) - spsp->stage_start_x_offset;
	}

	this_x_offset += (spsp->cumulative_x_offset + (path_loops * datatable_data[datatable_index].total_special_path_x_offset));

	if (spsp->stage_y_behaviour == SPECIAL_PATH_TABLE_BEHAVIOUR_LINEAR)
	{
		this_y_offset = int (MATH_lerp (0.0f, float(spsp->stage_y_magnitude), stage_sub_percentage));
	}
	else
	{
		this_angle = int (MATH_lerp (float(spsp->stage_y_start_angle), float(spsp->stage_y_start_angle + spsp->stage_y_period), stage_sub_percentage));
		this_y_offset = ((MATH_get_sin_table_value(this_angle) * spsp->stage_y_magnitude) / 10000) - spsp->stage_start_y_offset;
	}

	this_y_offset += (spsp->cumulative_y_offset + (path_loops * datatable_data[datatable_index].total_special_path_y_offset));

	*x_offset = this_x_offset;
	*y_offset = this_y_offset;

	return path_stage;
}



int SPECPATH_hit_flag (int datatable_index, int old_percentage, int new_percentage, int path_stage)
{
	int old_path_stage = SPECPATH_get_path_stage (datatable_index, path_stage, old_percentage);
	int new_path_stage = SPECPATH_get_path_stage (datatable_index, path_stage, new_percentage);

	if (old_path_stage != new_path_stage)
	{
		if (new_percentage > old_percentage)
		{
			return datatable_data[datatable_index].special_path_stage_pointer[old_path_stage].stage_flag;
		}
		else
		{
			return datatable_data[datatable_index].special_path_stage_pointer[new_path_stage].stage_flag;
		}
	}

	return UNSET;
}



int SPECPATH_section_start_percentage (int datatable_index, int percentage, int path_stage)
{
	path_stage = SPECPATH_get_path_stage (datatable_index, path_stage, percentage);
	return (datatable_data[datatable_index].special_path_stage_pointer[path_stage].percentage_progress_to_start_of_stage);
}



int SPECPATH_get_path_velocity_from_percentage (int datatable_index, int path_angle, int previous_path_percentage, int current_path_percentage, int *x_vel, int *y_vel, int current_path_stage)
{
	// This gets the offset between the two points on the path. ie, the velocity.

	int x1,y1,x2,y2;

	SPECPATH_get_path_position_offset_from_percentage (datatable_index, path_angle, previous_path_percentage,  &x1, &y1, current_path_stage);
	current_path_stage = SPECPATH_get_path_position_offset_from_percentage (datatable_index, path_angle, current_path_percentage,  &x2, &y2, current_path_stage);

	*x_vel = x2-x1;
	*y_vel = y2-y1;

	return current_path_stage;
}



void SPECPATH_convert_to_special_path (int datatable_index)
{
	// This verifies the data is of the right size and then converts it to a special path table.
	// Should be done at the start of the script as may cause frame droppage. Multiple calls for
	// the same path will result in a borkage.

	if (ARRAY_get_datatable_line_length(datatable_index) == SPECIAL_PATH_TABLE_LINE_LENGTH)
	{
		// Hurrah! It's compatible! Okay, figure out all the offsets and guff that we can.

		datatable_data[datatable_index].special_path_stage_pointer = (special_path_stage_struct *) malloc (datatable_data[datatable_index].lines * sizeof(special_path_stage_struct));

		int stage;
		int total_frame_length = 0;
		int total_x_offset_so_far = 0;
		int total_y_offset_so_far = 0;

		special_path_stage_struct *spsp = NULL;
		
		for (stage=0; stage<datatable_data[datatable_index].lines; stage++)
		{
			spsp = &datatable_data[datatable_index].special_path_stage_pointer[stage];
			spsp->stage_flag = ARRAY_read_datatable_value (datatable_index, stage, SPECIAL_PATH_TABLE_FLAG_INDEX);

			spsp->stage_frame_count = ARRAY_read_datatable_value (datatable_index, stage, SPECIAL_PATH_TABLE_FRAME_LENGTH);
			spsp->total_frame_count_to_stage_start = total_frame_length;
			total_frame_length += spsp->stage_frame_count;
			
			spsp->stage_x_magnitude = ARRAY_read_datatable_value (datatable_index, stage, SPECIAL_PATH_TABLE_X_MAGNITUDE);
			spsp->stage_y_magnitude = ARRAY_read_datatable_value (datatable_index, stage, SPECIAL_PATH_TABLE_Y_MAGNITUDE);

			spsp->stage_x_behaviour = ARRAY_read_datatable_value (datatable_index, stage, SPECIAL_PATH_TABLE_X_BEHAVIOUR);
			spsp->stage_y_behaviour = ARRAY_read_datatable_value (datatable_index, stage, SPECIAL_PATH_TABLE_Y_BEHAVIOUR);

			spsp->stage_x_start_angle = ARRAY_read_datatable_value (datatable_index, stage, SPECIAL_PATH_TABLE_X_START_ANGLE);
			spsp->stage_x_period = ARRAY_read_datatable_value (datatable_index, stage, SPECIAL_PATH_TABLE_X_ANGLE_PERIOD);
			
			spsp->stage_y_start_angle = ARRAY_read_datatable_value (datatable_index, stage, SPECIAL_PATH_TABLE_Y_START_ANGLE);
			spsp->stage_y_period = ARRAY_read_datatable_value (datatable_index, stage, SPECIAL_PATH_TABLE_Y_ANGLE_PERIOD);

			if (spsp->stage_x_behaviour == SPECIAL_PATH_TABLE_BEHAVIOUR_ANGULAR)
			{
				spsp->stage_start_x_offset = (spsp->stage_x_magnitude * MATH_get_sin_table_value (spsp->stage_x_start_angle)) / 10000;
				spsp->stage_x_offset = ((spsp->stage_x_magnitude * MATH_get_sin_table_value (spsp->stage_x_start_angle + spsp->stage_x_period)) / 10000) - spsp->stage_start_x_offset;
			}
			else
			{
				spsp->stage_start_x_offset = 0;
				spsp->stage_x_offset = spsp->stage_x_magnitude;
			}

			if (spsp->stage_y_behaviour == SPECIAL_PATH_TABLE_BEHAVIOUR_ANGULAR)
			{
				spsp->stage_start_y_offset = (spsp->stage_y_magnitude * MATH_get_sin_table_value (spsp->stage_y_start_angle)) / 10000;
				spsp->stage_y_offset = ((spsp->stage_y_magnitude * MATH_get_sin_table_value (spsp->stage_y_start_angle + spsp->stage_y_period)) / 10000) - spsp->stage_start_y_offset;
			}
			else
			{
				spsp->stage_start_y_offset = 0;
				spsp->stage_y_offset = spsp->stage_y_magnitude;
			}

			spsp->cumulative_x_offset = total_x_offset_so_far;
			spsp->cumulative_y_offset = total_y_offset_so_far;

			total_x_offset_so_far += spsp->stage_x_offset;
			total_y_offset_so_far += spsp->stage_y_offset;
		}

		datatable_data[datatable_index].total_special_path_x_offset = total_x_offset_so_far;
		datatable_data[datatable_index].total_special_path_y_offset = total_y_offset_so_far;

		float multiplier = float (total_special_path_percent_size) / float (total_frame_length);

		for (stage=0; stage<datatable_data[datatable_index].lines; stage++)
		{
			spsp = &datatable_data[datatable_index].special_path_stage_pointer[stage];

			spsp->percentage_progress_to_start_of_stage = int (float(spsp->total_frame_count_to_stage_start) * multiplier);
			spsp->percentage_length_of_stage = int (float(spsp->stage_frame_count) * multiplier);
		}

		// Overwrite the last entry to make sure it goes all the way up to 
		// total_special_path_percent_size and we aren't borked by rounding errors.
		spsp->percentage_length_of_stage = total_special_path_percent_size - spsp->percentage_progress_to_start_of_stage;
	}
	else
	{
		// Crap! It ain't compatible! Ah well, prolly an honest mistake...
		// KICK THE SHIT OUT OF THE USER!!!

		assert(0);
	}

}
