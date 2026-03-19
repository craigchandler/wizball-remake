#ifndef _SCRIPTING_H_
#define _SCRIPTING_H_


#include "scripting_enums.h"

#define	MAX_ENTITIES					(4096)
// The number of active entities in the game at one time.

#define	MAX_FLAGS						(1024)
// The number of flags in the flag array.

#define MAX_ENTITY_VARIABLES			(272)
// How much data in the entities.

#define RETURN_POINTER_STACK_SIZE		(10)
#define CHECK_STACK_SIZE				(20)

#define VALUE_TYPE						(0)
#define VALUE_VALUE						(1)

#define NEVER_DEBUT						(-1)
#define NOT_YET_DEBUTED					(0)
#define DEBUTED							(1)

#define DEAD							(0)
#define JUST_BORN						(1)
#define ALIVE							(2)
#define SLEEPING						(3)
#define FROZEN							(4)

// Whether an entity slot is open or not, basically.

#define PREV_ENTITY						(0)
#define NEXT_ENTITY						(1)
#define UNSET							(-1)
#define LOCKED							(-2)
// To do with processing order and optimisation.

#define	NO_ORDERING						(0) // no ordering occurs!
#define	X_ORDERING						(1) // entities will be ordered according to x
#define	Y_ORDERING						(2) // entities will be ordered according to y
#define	Z_ORDERING						(3) // entities will be ordered according to z

#define PRE_ORDER						(-1) // drawn before anything else in processing order
#define POSITION_ORDER					(0) // drawn according to layer and order
#define POST_ORDER						(1) // drawn after everything else in processing order

#define NOT_IN_WAVE						(-1)

#define	NOTHING							(0)
#define	PLAYER_TYPE						(1)
#define ENEMY 							(2)
#define PLAYER_BULLET 					(3)
#define ENEMY_BULLET 					(4)
#define PICKUP							(5)
#define MISC							(6)
// Verbose types which boil down to basically player and enemy for collision purposes.

#define MAX_PIXEL_SPREAD				(64)
// This is how far to either side of an entities extents it looks for collisions. The higher
// the number the slower things go. 

#define SHAPE_NONE						(0)
#define SHAPE_POINT						(1)
#define SHAPE_CIRCLE					(2)
#define SHAPE_RECTANGLE					(3)
#define SHAPE_ROTATED_RECTANGLE			(4)
// Collision profiles.

#define PROCESS_FIRST					(0)
#define PROCESS_PREVIOUS				(1)
#define PROCESS_NEXT					(2)
#define PROCESS_LAST					(3)
#define PROCESS_BEFORE_SPECIFIC			(4)
#define PROCESS_AFTER_SPECIFIC			(5)
// Where you want to plop 'em in the processing stack.

#define FOR_STACK_SIZE					(16)
#define SWITCH_STACK_SIZE				(16)

#define PATH_X_OFFSET					(0)
#define PATH_Y_OFFSET					(1)
#define PATH_ANGLE						(2)

#define COLL_PLAYER						(1)
#define COLL_ENEMY						(2)
#define COLL_PLAYER_BULLET				(4)
#define COLL_ENEMY_BULLET				(8)
#define COLL_PICKUP						(16)
#define COLL_BOSS						(32)
#define COLL_MISC						(64)

#define DRAW_OVERRIDE_NONE				(0)
#define DRAW_OVERRIDE_OVERRIDE_IF_Y		(1)
#define DRAW_OVERRIDE_OVERRIDE_ALWAYS	(2)

#define TEXT_OUTPUT_CENTER_X			(1)
#define TEXT_OUTPUT_CENTER_Y			(2)
#define TEXT_OUTPUT_RIGHT_ALIGN			(4)
#define TEXT_OUTPUT_BOTTOM_ALIGN		(8)

#define ALTERATION_KILL					(0)
#define ALTERATION_SLEEP				(1)
#define ALTERATION_WAKE					(2)
#define ALTERATION_FREEZE				(3)
#define ALTERATION_UNFREEZE				(4)

#define MAX_NUMBER_OF_LINE_RECORDS		(2048)

#define RETRENGINE_FALSE				(0)
#define RETRENGINE_TRUE					(1)

#define SPRITE_PIVOT_CENTRE				(0)
#define SPRITE_PIVOT_TOP				(1)
#define SPRITE_PIVOT_TOP_RIGHT			(2)
#define SPRITE_PIVOT_RIGHT				(3)
#define SPRITE_PIVOT_BOTTOM_RIGHT		(4)
#define SPRITE_PIVOT_BOTTOM				(5)
#define SPRITE_PIVOT_BOTTOM_LEFT		(6)
#define SPRITE_PIVOT_LEFT				(7)
#define SPRITE_PIVOT_TOP_LEFT			(8)
#define SPRITE_PIVOT_ORIGINAL			(9)
#define SPRITE_PIVOT_ADAPT_TO_ORIGINAL	(10)

#define FLAG_TYPE_NORMAL				(0)
#define FLAG_TYPE_ENTITY_ID				(1)

#define CURVE_PERCENTAGE_FIFO			(0)
#define CURVE_PERCENTAGE_SISO			(1)
#define CURVE_PERCENTAGE_FISO			(2)
#define CURVE_PERCENTAGE_SIFO			(3)

bool SCRIPTING_check_for_visual_params (int tilemap_number, int spawn_point_number, int *bitmap_number, int *base_frame, int *frame_count, int *frame_delay, int *frame_end_delay, int *path_number, int *path_angle);

int SCRIPTING_interpret_script (int entity_id , int over_ride_line);
void SCRIPTING_remove_from_collision_bucket (int entity_id);
bool SCRIPTING_find_next_entity (void);
void SCRIPTING_setup_entity(int entity_id);
int SCRIPTING_spawn_restored_entity_last (void);

void SCRIPTING_spawn_shutdown_entity (void);
void SCRIPTING_spawn_entity_by_name (char *script_name , int x , int y , int v0 , int v1 , int v2 );
bool SCRIPTING_load_script (char *filename);
void SCRIPTING_setup_everything (void);
bool SCRIPTING_process_entities (void);
int SCRIPTING_draw_all_windows (void);
void SCRIPTING_update_window_positions (void);
void SCRIPTING_setup_data_stores (void);
void SCRIPTING_load_prefabs (void);
void SCRIPTING_free_prefabs (void);
void SCRIPTING_save_compiled_prefabs (void);
void SCRIPTING_load_compiled_prefabs (void);

void SCRIPTING_destroy_all_entity_arrays (void);

void SCRIPTING_destroy_flag_arrays (void);

void SCRIPTING_set_default_entity_from_prefab (int prefab_number);
void SCRIPTING_use_prefab (int prefab_number , int *entity_pointer);

void SCRIPTING_display_script_list (int y_pos);
void SCRIPTING_display_flag_list (int x_pos, int y_pos);

void SCRIPTING_confirm_entity_counts (void);

void SCRIPTING_set_up_save_data (void);
void SCRIPTING_reset_save_data (void);
void SCRIPTING_save_spawn_point_flags (void);
void SCRIPTING_save_ai_node_tables (void);
void SCRIPTING_save_ai_zone_tables (void);
void SCRIPTING_save_zone_flags (void);
void SCRIPTING_save_entity (int entity_number, int tag);
void SCRIPTING_save_flag (int flag_number);
void SCRIPTING_output_flags_to_file (char *filename);
void SCRIPTING_output_entity_non_relation_properties_to_file (int ent_index, FILE *file_pointer);
void SCRIPTING_output_entity_to_file (int ent_index, int ent_tag, FILE *file_pointer);
void SCRIPTING_output_entities_to_file (char *filename);
void SCRIPTING_output_save_data_to_file (int filename_text_tag);
void SCRIPTING_load_save_file (int filename_text_tag);
void SCRIPTING_delete_save_file (int filename_text_tag);
int SCRIPTING_does_save_file_exist (int filename_text_tag);
void SCRIPTING_free_loaded_save_data (void);

void SCRIPTING_append_checksum_to_save_file (char *filename);

void SCRIPTING_state_dump (void);

void SCRIPTING_add_report_to_watch_report (int causing_entity, int affected_entity, int line_number, int variable_altered, int old_value, int new_value );

void SCRIPTING_setup_watch_list (void);

void SCRIPTING_destroy_interaction_table (void);
void SCRIPTING_output_interaction_table (void);

void SCRIPTING_destroy_all_scripts (void);

void SCRIPTING_output_tracer_script_to_file (bool called_from_script);

int SCRIPTING_get_int_value ( int entity_id , int line_number , int argument );

//void SCRIPTING_check_for_loop_links (void);

//void SCRIPTING_check_for_collide_type_offset_mismatch(void);

typedef struct 
{
	int table_width;
	int table_height;
	int table_depth;
	int table_data_size;
	int table_id;
	int *data;
} data_store_table_struct;

typedef struct
{
	int x[4];
	int y[4];
	float line_lengths[4];
} arbitrary_quads_struct;

typedef struct
{
	float red[4];
	float green[4];
	float blue[4];
	float alpha[4];
} arbitrary_vertex_colour_struct;



typedef struct 
{
	int number_of_tables;
	data_store_table_struct *table_list;
} data_store_struct;

typedef struct 
{
	int ordering_variable; // This is which variable is used to decide the position within the table.
	int ordering_bitshift; // This decides the resolution of the table, from 0 (highest) upwards.
	int *ordering_table_start; // The first entity in this line...
	int *ordering_table_end; // The last entity in this line...
	int ordering_queues; // The number of lines in this ordering system...
	bool reverse_output; // If this is set then the compiled data is spat out into the next list or
		// window buckets in reverse order (ie, highest to lowest)
} ordering_system_struct;



typedef struct 
{
	bool active;
	bool new_active; // So it only updates the above at the start of the frame, not in the middle of it.
	int *y_ordering_list_starts; // These are used to order the entities according to their position on-screen.
	int *y_ordering_list_ends;
	int y_ordering_list_size;
	int y_ordering_resolution;
	int *z_ordering_list_starts; // These are used to order the entities according to their depth in the gameworld.
	int *z_ordering_list_ends;
	int z_ordering_list_size;
	float screen_x; // Where on the screen the window's top-left corner is.
	float screen_y;
	float fpos_x;
	float fpos_y;
	float new_opengl_scale_x;
	float new_opengl_scale_y;
	int width; // How big it is.
	int height;
	float new_screen_x; // These are so the change happens at the end of the phase.
	float new_screen_y;
	int new_width;
	int new_height;
	int current_x; // Where in the game-world the window's top-left corner is.
	int current_y;
	int buffer_size; // How much space inside the game-world we allow at the sides of the window...
	int buffered_tl_x; // ...So that enemies whose centre is off-screen will still be drawn and...
	int buffered_tl_y; // ...not just "plink" into existance suddenly.
	int buffered_br_x;
	int buffered_br_y;
	int new_x; // This is used so that the position of the window doesn't actually update halfway through
	int new_y; // a frame but rather at the end.
	float opengl_scale_x;
	float opengl_scale_y;
	float opengl_transform_x;
	float opengl_transform_y;
	int skip_me_this_frame;
	int vertex_red,vertex_green,vertex_blue;
	bool secondary_window_colour;
	float scaled_height;
	float scaled_width;
} window_struct;



typedef struct 
{
	int presets;
	int *variables;
	int *values;
} prefab_struct;



typedef struct
{
	int width;
	int height;
	int depth;
	int uid;
	int *data;
} loaded_array_struct;



typedef struct
{
	int loaded_entity_tag;

	int loaded_variable_count;
	int *loaded_entity_variable_list;
	int *loaded_entity_value_list;

	int loaded_reference_count;
	int *loaded_entity_reference_variable_list;
	int *loaded_entity_reference_tag_list;

	int loaded_entity_array_count;
	loaded_array_struct *array_data;
} loaded_entity_struct;



typedef struct
{	
	bool save_spawn_point_flags;
	
	bool save_zone_flags;
	
	bool save_ai_node_pathfinding_tables;

	bool save_ai_zone_pathfinding_tables;

	int *saved_flag_list;
	int saved_flag_count;

	int *saved_entity_number_list;
	int *saved_entity_tag_list;
	int saved_entity_count;

	int loaded_entity_count;
	loaded_entity_struct *loaded_entity_data;
} save_data_struct;



// This structure defines where in the gameworld things
// are colliding. It's only within the confines of this
// box that objects are added to the collision buckets.
// However as this is only for entity-to-entity collisions
// it doesn't stop them all from colliding with the world.



extern int total_process_counter;
extern int drawn_process_counter;
extern int alive_process_counter;
extern int script_lines_executed;
extern int used_entities;
extern int limboed_entities;
extern int free_entities;
extern int lost_entities;

extern int global_next_entity;
extern int just_created_entity;
extern int first_processed_entity_in_list;

extern int entity [MAX_ENTITIES][MAX_ENTITY_VARIABLES];
extern arbitrary_quads_struct arbitrary_quads[MAX_ENTITIES];
extern arbitrary_vertex_colour_struct arbitrary_vertex_colours[MAX_ENTITIES];
extern int *flag_array;
extern window_struct *window_details;
extern save_data_struct save_data;

#define FIX_ME_NOW 99

#endif











