#ifndef _OBJECT_COLLISION_H_
#define _OBJECT_COLLISION_H_

#define OBCOLL_BACKUP_DATA_SIZE			(1024)

typedef struct 
{
	int current_x;
	int current_y;
	int current_width;
	int current_height;
	int new_x;
	int new_y;
	int new_width;
	int new_height;
	int backup_data_pointer;
	int backup_old_sx[OBCOLL_BACKUP_DATA_SIZE];
	int backup_old_sy[OBCOLL_BACKUP_DATA_SIZE];
	int backup_old_ex[OBCOLL_BACKUP_DATA_SIZE];
	int backup_old_ey[OBCOLL_BACKUP_DATA_SIZE];
	int shifted_start_x;
	int shifted_start_y;
	int shifted_end_x;
	int shifted_end_y;

} collision_window_struct;

void OBCOLL_position_collision_window (int x, int y, int width, int height);
void OBCOLL_update_collision_window (void);
void OBCOLL_open_up_collision_buckets (void);
void OBCOLL_close_down_collision_buckets (void);
void OBCOLL_free_collision_buffers (void);
void OBCOLL_create_collision_buffers (void);
void OBCOLL_add_to_collision_bucket (int entity_id);
void OBCOLL_remove_from_collision_bucket (int entity_id);

void OBCOLL_add_to_general_area_bucket (int entity_id);
void OBCOLL_remove_from_general_area_bucket (int entity_id);

void OBCOLL_draw_collision_buckets (void);

void OBCOLL_sleep_out_of_area (int sx, int sy, int ex, int ey);
void OBCOLL_sleep_area (int sx, int sy, int ex, int ey);
void OBCOLL_wake_area (int sx, int sy, int ex, int ey);
void OBCOLL_wake_out_of_area (int sx, int sy, int ex, int ey);
void OBCOLL_sleep_direction (int direction, int start);
void OBCOLL_wake_direction (int direction, int start);

// void OBCOLL_convert_and_constrain_bucket_co_ords ( int *first_bucket_x, int *first_bucket_y, int *last_bucket_x, int *last_bucket_y);
// bool OBCOLL_collide_entity_pair (int entity_id, int other_entity_id);
// void OBCOLL_collide_point_with_buckets (int entity_id);
// void OBCOLL_collide_circle_with_buckets (int entity_id);
// void OBCOLL_collide_rectangle_with_buckets (int entity_id);
// void OBCOLL_collide_rotated_rectangle_with_buckets (int entity_id);
void OBCOLL_collision_handler (void);
void OBCOLL_setup_everything (void);
void OBCOLL_update_buffer(void);
void OBCOLL_set_game_world_size (int width, int height, int new_bitshift);
int OBCOLL_get_first_in_bucket (int bucket_x , int bucket_y);

//void OBCOLL_check_bucket_762 (void);

void OBCOLL_check_integrity_of_all_buckets(void);

extern bool object_collision_on;
extern int general_area_bitshift;
extern int collision_entity_count;
extern int collision_actual_count;
extern int collision_bitshift;

#endif
