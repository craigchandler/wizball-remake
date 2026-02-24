#ifndef _ARRAYS_H_
#define _ARRAYS_H_

#define SPECIAL_PATH_TABLE_LINE_LENGTH		(10)
#define SPECIAL_PATH_TABLE_FLAG_INDEX		(0)
#define SPECIAL_PATH_TABLE_FRAME_LENGTH		(1)
#define SPECIAL_PATH_TABLE_X_MAGNITUDE		(2)
#define SPECIAL_PATH_TABLE_Y_MAGNITUDE		(3)
#define SPECIAL_PATH_TABLE_X_BEHAVIOUR		(4)
#define SPECIAL_PATH_TABLE_Y_BEHAVIOUR		(5)
#define SPECIAL_PATH_TABLE_X_START_ANGLE	(6)
#define SPECIAL_PATH_TABLE_X_ANGLE_PERIOD	(7)
#define SPECIAL_PATH_TABLE_Y_START_ANGLE	(8)
#define SPECIAL_PATH_TABLE_Y_ANGLE_PERIOD	(9)

#define SPECIAL_PATH_TABLE_BEHAVIOUR_LINEAR		(0)
#define SPECIAL_PATH_TABLE_BEHAVIOUR_ANGULAR	(1)

// Special Path structure...

typedef struct
{
	// Read in data...

	// The flag for this stage.
	int stage_flag;

	// How many frames for this stage.
	int stage_frame_count;

	// How many frames before this stage in total.
	int total_frame_count_to_stage_start;

	// The magnitude of the movement in this stage.
	int stage_x_magnitude;
	int stage_y_magnitude;

	// Stage behaviour - linear or angular?
	int stage_x_behaviour;
	int stage_y_behaviour;

	// If angular, the start angle and period.
	int stage_x_start_angle;
	int stage_x_period;
	int stage_y_start_angle;
	int stage_y_period;

	// Calculated data...

	// The starting magnitudes so they can be taken off and used as a zero point.
	int stage_start_x_offset;
	int stage_start_y_offset;

	// How far the entity moves in this stage. Not necessarily the same as the magnitude due to sine waves and whatnot.
	int stage_x_offset;
	int stage_y_offset;

	// The sum of all previous stage offsets.
	int cumulative_x_offset;
	int cumulative_y_offset;

	// Percentage of the distance through at the start of this section...
	int percentage_progress_to_start_of_stage;

	// Percentage length of this section.
	int percentage_length_of_stage;

	
} special_path_stage_struct;



// Datatable structure...

typedef struct
{
	int line_size;
	int lines;
	int index_count;
	int *index_list;
	int *data;
	special_path_stage_struct *special_path_stage_pointer;
	int total_special_path_x_offset;
	int total_special_path_y_offset;
} datatable_struct;



typedef struct
{
	int unique_id;
	int width;
	int height;
	int depth;
	int width_times_height; // Shortcut to avoid unecessary maths.
	int total_size;
	int *values;
} entity_array;



typedef struct
{
	int array_count;
	entity_array *array_pointer;
} entity_array_holder;



char * ARRAY_get_array_as_text (int entity_number, int array_unique_id, int length);
void ARRAY_destroy_entitys_arrays (int entity_number);
void ARRAY_output_entity_arrays_to_file(int entity_number, FILE *file_pointer);
void ARRAY_reset_array (int entity_number, int unique_id, int value=0);
void ARRAY_create_array (int entity_number, int unique_id, int width, int height=1, int depth=1);
void ARRAY_resize_array (int entity_number, int unique_id, int width, int height=1, int depth=1);
int ARRAY_read_value (int entity_number, int array_unique_id, int x, int y=0, int z=0);
void ARRAY_write_value (int entity_number, int array_unique_id, int value, int x, int y=0, int z=0);
int ARRAY_add_text_to_array (int entity_number, int unique_id, int text_tag, int start_offset, bool add_a_space);
int ARRAY_add_text_number_to_array (int entity_number, int unique_id, int number_value, int start_offset, bool add_a_space);
void ARRAY_load_datatables (void);
void ARRAY_destroy_datatables (void);
int ARRAY_get_datatable_line_count (int datatable_index);
int ARRAY_get_datatable_line_length (int datatable_index);
int ARRAY_get_datatable_line_from_label (int datatable_index, int datatable_label_index);
int ARRAY_get_datatable_line_count_from_label_to_next (int datatable_index, int datatable_label_index);
int ARRAY_read_datatable_value (int datatable_index, int datatable_line, int datatable_argument);
void ARRAY_create_array_from_datatable (int entity_number, int unique_id, int datatable_index);

int SPECPATH_get_path_stage (int datatable_index, int path_stage, int path_percentage);
int SPECPATH_get_path_position_offset_from_percentage (int datatable_index, int path_angle, int path_percentage, int *x_offset, int *y_offset, int path_stage);
int SPECPATH_hit_flag (int datatable_index, int old_percentage, int new_percentage, int path_stage);
int SPECPATH_section_start_percentage (int datatable_index, int percentage, int path_stage);
int SPECPATH_get_path_velocity_from_percentage (int datatable_index, int path_angle, int previous_path_percentage, int current_path_percentage, int *x_vel, int *y_vel, int current_path_stage);
void SPECPATH_convert_to_special_path (int datatable_index);

extern datatable_struct *datatable_data;
extern int number_of_datatables;

#endif


