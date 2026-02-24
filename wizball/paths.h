#include "string_size_constants.h"
#include "new_editor.h" // For the parameter structure

#ifndef _PATHS_H_
#define _PATHS_H_

#define SPEED_CURVE_FAST			(0)
#define SPEED_CURVE_LINEAR			(1)
#define SPEED_CURVE_SLOW			(2)
#define MAX_SPEED_CURVE_TYPES		(3)

typedef struct {
	int x;
	int y;
	char flag_name[NAME_SIZE];
	bool flag_used;
	int flag_value; // This is an arbitrary value which can be used to trigger stuff.
	float length; // This is set to UNSET at first and when the path is turned into a lookup data
	int pre_cx,pre_cy; // These four variables are for bezier spline paths and are the control points.
	int aft_cx,aft_cy;
	int pre_dist,aft_dist;
	int grabbed_status;
	int grabbed_x;
	int grabbed_y;
	int line_segments;
	int speed;

	int speed_in_curve_type;
	int speed_out_curve_type;

	int agregate_distance; // This is the total distance of each of the lp nodes between here and the next node.

	float percentage_multiplier; // This is used to calculate how far along each lp node is based on it's distance from this parent node.
} node;



// Okay, with the lookup nodes there's a lot of replicated data - but there's a very good reason for this.
// It just makes it easier to think around it for the cost of a few extra bytes. Fook it!

typedef struct {
	int x;
	int y;
	int start_parent_path_node;
	int end_parent_path_node;
	bool flag_used;
	int flag_value;
	float angle;

	int distance_to_next_lp_node;
	int total_distance_to_parent_path_node_via_lp_nodes;

	float speed_percentage; // This is the percentage distance between parent nodes. It's biased then used to calculate the speed.
	int speed;

	float time_length; // This is the length of this path in time (based on pixel length and the speed at either end of the section)
	float time_length_from_start; // Durr...
	int start_percentage; // This is the percentage of the way through the track in time at this node.

	int percentage_length; // This is the amount of percentage of track in time till the next node.
} lookup_path_node;



#define NO_DRAG_THRESHOLD					(5)

#define NO_NODES_SELECTED					(-1)
#define MULTIPLE_NODES_SELECTED				(-2)

#define SELECTING_NODES						(0)
#define ADDING_NODES						(1)
#define REMOVING_NODES						(2)

#define NODE_UNSELECTED						(0) // Unselected
#define NODE_SELECTED						(1) // Currently selected already
#define NODE_PRESUMPTIVE_UNSELECTED			(2) // Currently dragging a CTRL-box around them to unselect them
#define NODE_PRESUMPTIVE_SELECTED			(3) // Currently dragging a box or SHIFT-box around them to select them.

#define PATH_NODE_HANDLE_SIZE				(16)
#define PATH_PRE_OR_AFT_HANDLE_SIZE			(8)

#define GRABBED_PATH_NODE_NODE				(0)
#define GRABBED_PATH_NODE_X_HANDLE			(1)
#define GRABBED_PATH_NODE_Y_HANDLE			(2)
#define GRABBED_PATH_NODE_PRE				(3)
#define GRABBED_PATH_NODE_AFT				(4)

#define LINEAR			(0) // All straight.
#define CATMULL_ROM		(1) // All curly but a bit crap.
#define BEZIER			(2) // All curly and lovely.
#define SIMPLE			(3) // Straight and with simple interpolation so path lengths easily match up.

// These affect what values are passed back as x_vel and y_vel's to an object
// querying the path if that stroll off the end of it's values.

#define NON_LOOPED			(0) // Just passes back 0,0 so it remains motionless.
#define LOOP_TO_START		(1) // Passes back an offset which sends the user back to the start co-ord of the path.
#define LOOP_ON_END			(2) // Passes back an offset as if they looped seamlessly.
#define PING_PONG			(3) // Passes back a value which sends the user back along the path to the start and then back to the end.



typedef struct {
	char name[NAME_SIZE];
	char old_name[NAME_SIZE];
	int x;
	int y;
	int total_offset_x;
	int total_offset_y;
	int lowest_x_offset;
	int lowest_y_offset;
	int greatest_x_offset;
	int greatest_y_offset; // So we know the bounding box of the path and can see if it's inside the viewing frustrum in the editor
	int total_distance;
	int line_type;
	int loop_type; // Even if the path's nodes aren't looped, the path itself can be.
	bool looped; // If this is set then when the lookup table is constructed it automatically fills in the gap from the last node to the first.
	int nodes;
	node *node_list_pointer;
	int lp_nodes;
	lookup_path_node *lp_node_list_pointer;
} path;



typedef struct {
	float x_offset;
	float y_offset;
	bool flag_set;
	int flag_value;
	float angle;
} path_feedback;



void PATHS_change_name (char *old_entry_name, char *new_entry_name);
void PATHS_load_all (void);
void PATHS_save_all (void);
void PATHS_destroy_all (void);
void PATHS_draw_path (int path_number , float path_angle, int x, int y, float zoom_level, int red=255, int green=255, int blue=255, bool draw_subnodes=false);
int PATHS_get_length (int path_number);
int PATHS_find_nearest_path (int x, int y, bool return_distance=false);
int PATHS_find_nearest_path_node (int path_number, int x, int y, bool return_distance=false);
int PATHS_find_nearest_path_line (int path_number, int x, int y, bool return_distance=false);
void PATHS_draw_all_paths (int selected_path_number, int start_x, int start_y, int width, int height, int zoom_level, int norm_path_red, int norm_path_green, int norm_path_blue, int sel_path_red, int sel_path_green, int sel_path_blue);
void PATHS_draw_node (int path_number, int node_number, float path_angle, int start_x, int start_y, int zoom_level, int red, int green, int blue, bool show_extras);
void PATHS_draw_midpoint_co_ords ( int path_number , int node_number , int start_x, int start_y, int tilesize, int zoom_level);
int PATHS_create (bool new_path);
void PATHS_create_lookup_from_path (int path_number);

void PATHS_get_node_co_ords (int path_number, int node_number, int *node_x, int *node_y);
void PATHS_get_node_pre_co_ords (int path_number, int node_number, int *pre_cx, int *pre_cy);
void PATHS_get_node_aft_co_ords (int path_number, int node_number, int *aft_cx, int *aft_cy);
void PATHS_get_node_flag_details (int path_number, int node_number, char *flag_name, bool *flag_used);
void PATHS_set_node_co_ords (int path_number, int node_number, int node_x, int node_y);
void PATHS_set_node_pre_co_ords (int path_number, int node_number, int pre_cx, int pre_cy);
void PATHS_set_node_aft_co_ords (int path_number, int node_number, int aft_cx, int aft_cy);
void PATHS_set_node_flag_details (int path_number, int node_number, char *flag_name, bool flag_used);

void PATHS_get_path_name (int path_number, char *name);
void PATHS_get_path_position (int path_number, int *x, int *y);
void PATHS_get_path_loop_details (int path_number, int *loop_type, bool *looped);
void PATHS_get_path_line_type (int path_number, int *line_type);
void PATHS_set_path_name (int path_number, char *name);
void PATHS_set_path_position (int path_number, int x, int y);
void PATHS_set_path_loop_details (int path_number, int loop_type, bool looped);
void PATHS_set_path_line_type (int path_number, int line_type);

void PATHS_draw_all_nodes (int selected_node, int path_number, int start_x, int start_y, float zoom_level);

bool PATHS_what_selected_path_node_thing_has_been_grabbed (int path_number, int x, int y, float zoom_level, int *grabbed_handle, int *grabbed_node);
void PATHS_position_selected_nodes (int path_number, int offset_x, int offset_y, int grabbed_node, int grid_size, bool round_to_grid);
void PATHS_get_grabbed_co_ords (int path_number);
int PATHS_lock_nodes_status (int path_number);
void PATHS_partially_set_nodes_status (int path_number, int x1, int y1, int x2, int y2, float zoom_level, int flagging_type, int drag_time);
void PATHS_zero_origin (int path_number);

void PATHS_alter_path_details (int path_number, int amount);
void PATHS_alter_speed_details (int path_number, int amount);

void PATHS_create_path_node (int path_number, int x, int y);
void PATHS_create_path_node_global_co_ord (int path_number, int x, int y);
void PATHS_create_midpoint_path_node ( int path_number , int node_number );
void PATHS_calculate_path_bounding_box (int path_number);

void PATHS_deselect_nodes (int path_number);

int PATHS_length (int path_number);
char * PATHS_node_flag (int path_number, int node_number);
bool PATHS_check_for_duplicate_name (int selected_path_number, char *new_entry_name);
void PATHS_register_path_name_change (char *old_path_name , char *new_path_name);

bool PATHS_edit_paths (int state , bool overlay_display, int *current_cursor ,int sx, int sy,  int tilemap_number, int mx, int my, float zoom_level, int grid_size);

int PATHS_hit_flag (int path_number, int old_percentage, int new_percentage, int current_section);
int PATHS_section_start_percentage (int path_number, int percentage, int current_section);
int PATHS_get_path_position_offset_from_percentage (int path_number, int path_angle, int percentage, int *x_offset, int *y_offset, int current_section);
int PATHS_point_angle (int path_number, int path_angle, int percentage, int current_section);
int PATHS_section_angle (int path_number, int path_angle, int percentage, int current_section);

#endif

