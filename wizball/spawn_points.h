#ifndef _SPAWN_POINTS_H_
#define _SPAWN_POINTS_H_

typedef struct
{
	int old_uid;
	int old_index;
	int new_uid;
	int new_index;
	int old_next_sibling_uid;
	int old_parent_uid;
} copy_stack_item_struct;

bool SPAWNPOINTS_edit_spawn_points (int state , bool overlay_display, int *current_cursor ,int sx, int sy,  int tilemap_number, int mx, int my, float zoom_level, int grid_size);
void SPAWNPOINTS_confirm_all_links (void);
void SPAWNPOINTS_destroy_spawn_point (int tilemap_number , int spawn_point_number);

int SPAWNPOINTS_get_spawn_point_flag (int spawn_point_number);
void SPAWNPOINTS_set_spawn_point_flag (int spawn_point_number, int value);

void SPAWNPOINTS_output_altered_flags_to_file (char *filename);
void SPAWNPOINTS_input_flags_from_file (char *filename);

void SPAWNPOINTS_draw_spawn_points (int tilemap_number, int sx, int sy, int width, int height, float zoom_level, int selected_spawn_point, bool nearest, bool faded);

#define SPAWN_POINT_PROPERTY_EDITING_PARAMETER_VARIABLE			(0)
#define SPAWN_POINT_PROPERTY_EDITING_PARAMETER_VALUE			(1)
#define SPAWN_POINT_PROPERTY_EDITING_SCRIPT						(2)
#define SPAWN_POINT_PROPERTY_EDITING_TYPE						(3)
#define SPAWN_POINT_PROPERTY_EDITING_NAME						(4)

#define SPAWN_POINT_SELECT_RELATION_PARENT						(0)
#define SPAWN_POINT_SELECT_RELATION_NEXT_SIBLING				(1)

#define SPAWN_POINT_CREATE_MODE									(0)
#define SPAWN_POINT_EDIT_MODE									(1)
#define SPAWN_POINT_CHOOSE_RELATIVE								(2)
#define SPAWN_POINT_DRAGGING_OVER_NODES							(3)

#define MAX_PARAMETERS_FOR_SCRIPT								(256)

#endif









