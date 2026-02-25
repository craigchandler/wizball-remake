// This file deals with all the object-based collision McGuffery except for the mathematical routines
// behind them, which remain in the math_stuff file.

#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "object_collision.h"
#include "world_collision.h"
#include "math_stuff.h"
#include "parser.h"
#include "scripting.h"
#include "main.h"
#include "output.h"
#include "control.h"

#include "fortify.h"

int game_world_width = 0; // How big the co-ordinate system of the world is...
int game_world_height = 0;

int next_frames_game_world_width = 0;
int next_frames_game_world_height = 0;

int collision_entity_count;
int collision_actual_count;

bool object_collision_on = false;

collision_window_struct active_collision_area;

// These are the buckets and they operate almost EXACTLY like the window buckets, except
// entities are placed into them if (ENT_COLLIDE_WITH | ENT_COLLIDE_TYPE) > 0. Generally
// collisions between items will be fairly fast as it's mostly one entity (the player)
// against all the others.

int *collision_bucket_starts = NULL; // Plus everything which collides with any other entities (not the world) also ends up here.
int *collision_bucket_ends = NULL;

int *collision_bucket_aggregate_masks = NULL;
// The reason we have this is so that an entity can *instantly* tell if there will be anything worth colliding
// with in the bucket, and if there isn't it'll just ignore the whole contents.

int collision_bitshift = 0;
int next_frames_collision_bitshift = 0;

int collision_buckets_per_row;
int collision_buckets_per_col;
int max_collision_bucket_offset;



bool general_area_update_on = false;

int *general_area_map_starts = NULL;
int *general_area_map_ends = NULL;

int general_area_bitshift = 6;

int general_area_buckets_per_row;
int general_area_buckets_per_col;
int max_general_area_bucket_offset;

int last_frame_size_changed = 0;



bool OBCOLL_entity_in_rect (int entity_id , int sx , int sy , int ex , int ey)
{
	int *entity_pointer = &entity[entity_id][0];

	if (entity_pointer[ENT_WORLD_X] < sx)
	{
		return false;
	}
	else if (entity_pointer[ENT_WORLD_X] > ex)
	{
		return false;
	}
	else if (entity_pointer[ENT_WORLD_Y] < sy)
	{
		return false;
	}
	else if (entity_pointer[ENT_WORLD_Y] > ey)
	{
		return false;
	}

	return true;
}



void OBCOLL_sleep_out_of_area (int sx, int sy, int ex, int ey)
{
	// FIX THIS FUNCTION!
	// SO IT WORKS OUTSIDE THE RECT!

	int sx_bucket,ex_bucket,sy_bucket,ey_bucket;

	sx_bucket = sx >> general_area_bitshift;
	ex_bucket = ex >> general_area_bitshift;
	sy_bucket = sy >> general_area_bitshift;
	ey_bucket = ey >> general_area_bitshift;

	if (sx_bucket<0)
	{
		sx_bucket=0;
	}
	else if (ex_bucket > general_area_buckets_per_row)
	{
		ex_bucket=general_area_buckets_per_row;
	}

	if (sy_bucket<0)
	{
		sy_bucket=0;
	}
	else if (ey_bucket > general_area_buckets_per_col)
	{
		ey_bucket=general_area_buckets_per_col;
	}

	int bucket_x,bucket_y;
	int offset;
	int entity_id;

	for (bucket_x=sx_bucket; bucket_x<ex_bucket; bucket_x++)
	{
		for (bucket_y=sy_bucket; bucket_y<ey_bucket; bucket_y++)
		{
			offset = bucket_x + (bucket_y * collision_buckets_per_row);

			entity_id = general_area_map_starts[offset];

			while (entity_id != UNSET)
			{
				if ( (entity[entity_id][ENT_ALIVE] == ALIVE) || (entity[entity_id][ENT_ALIVE] == JUST_BORN) )
				{
					if (OBCOLL_entity_in_rect (entity_id,sx,sy,ex,ey) == false)
					{
						entity[entity_id][ENT_ALIVE] = SLEEPING;
					}
				}

				entity_id = entity[entity_id][ENT_NEXT_GENERAL_ENT];
			}
		}
	}

}



void OBCOLL_sleep_area (int sx, int sy, int ex, int ey)
{
	int sx_bucket,ex_bucket,sy_bucket,ey_bucket;

	sx_bucket = sx >> general_area_bitshift;
	ex_bucket = ex >> general_area_bitshift;
	sy_bucket = sy >> general_area_bitshift;
	ey_bucket = ey >> general_area_bitshift;

	if (sx_bucket<0)
	{
		sx_bucket=0;
	}
	else if (ex_bucket > general_area_buckets_per_row)
	{
		ex_bucket=general_area_buckets_per_row;
	}

	if (sy_bucket<0)
	{
		sy_bucket=0;
	}
	else if (ey_bucket > general_area_buckets_per_col)
	{
		ey_bucket=general_area_buckets_per_col;
	}

	int bucket_x,bucket_y;
	int offset;
	int entity_id;

	for (bucket_x=sx_bucket; bucket_x<ex_bucket; bucket_x++)
	{
		for (bucket_y=sy_bucket; bucket_y<ey_bucket; bucket_y++)
		{
			offset = bucket_x + (bucket_y * collision_buckets_per_row);

			entity_id = general_area_map_starts[offset];

			while (entity_id != UNSET)
			{
				if ( (entity[entity_id][ENT_ALIVE] == ALIVE) || (entity[entity_id][ENT_ALIVE] == JUST_BORN) )
				{
					if (OBCOLL_entity_in_rect (entity_id,sx,sy,ex,ey) == true)
					{
						entity[entity_id][ENT_ALIVE] = SLEEPING;
					}
				}

				entity_id = entity[entity_id][ENT_NEXT_GENERAL_ENT];
			}

		}
	}
	
}



void OBCOLL_wake_area (int sx, int sy, int ex, int ey)
{
	int sx_bucket,ex_bucket,sy_bucket,ey_bucket;

	sx_bucket = sx >> general_area_bitshift;
	ex_bucket = ex >> general_area_bitshift;
	sy_bucket = sy >> general_area_bitshift;
	ey_bucket = ey >> general_area_bitshift;

	if (sx_bucket<0)
	{
		sx_bucket=0;
	}
	else if (ex_bucket > general_area_buckets_per_row)
	{
		ex_bucket=general_area_buckets_per_row;
	}

	if (sy_bucket<0)
	{
		sy_bucket=0;
	}
	else if (ey_bucket > general_area_buckets_per_col)
	{
		ey_bucket=general_area_buckets_per_col;
	}

	int bucket_x,bucket_y;
	int offset;
	int entity_id;

	for (bucket_x=sx_bucket; bucket_x<ex_bucket; bucket_x++)
	{
		for (bucket_y=sy_bucket; bucket_y<ey_bucket; bucket_y++)
		{
			offset = bucket_x + (bucket_y * collision_buckets_per_row);

			entity_id = general_area_map_starts[offset];

			while (entity_id != UNSET)
			{
				if (entity[entity_id][ENT_ALIVE] == SLEEPING)
				{
					if (OBCOLL_entity_in_rect (entity_id,sx,sy,ex,ey) == true)
					{
						entity[entity_id][ENT_ALIVE] = JUST_BORN;
					}
				}

				entity_id = entity[entity_id][ENT_NEXT_GENERAL_ENT];
			}

		}
	}
	
}



void OBCOLL_wake_out_of_area (int sx, int sy, int ex, int ey)
{
	// FIX THIS FUNCTION!
	// SO IT WORKS OUTSIDE THE RECT!

	int sx_bucket,ex_bucket,sy_bucket,ey_bucket;

	sx_bucket = sx >> general_area_bitshift;
	ex_bucket = ex >> general_area_bitshift;
	sy_bucket = sy >> general_area_bitshift;
	ey_bucket = ey >> general_area_bitshift;

	if (sx_bucket<0)
	{
		sx_bucket=0;
	}
	else if (ex_bucket > general_area_buckets_per_row)
	{
		ex_bucket=general_area_buckets_per_row;
	}

	if (sy_bucket<0)
	{
		sy_bucket=0;
	}
	else if (ey_bucket > general_area_buckets_per_col)
	{
		ey_bucket=general_area_buckets_per_col;
	}

	int bucket_x,bucket_y;
	int offset;
	int entity_id;

	for (bucket_x=sx_bucket; bucket_x<ex_bucket; bucket_x++)
	{
		for (bucket_y=sy_bucket; bucket_y<ey_bucket; bucket_y++)
		{
			offset = bucket_x + (bucket_y * collision_buckets_per_row);

			entity_id = general_area_map_starts[offset];

			while (entity_id != UNSET)
			{
				if (entity[entity_id][ENT_ALIVE] == SLEEPING)
				{
					if (OBCOLL_entity_in_rect (entity_id,sx,sy,ex,ey) == false)
					{
						entity[entity_id][ENT_ALIVE] = JUST_BORN;
					}
				}

				entity_id = entity[entity_id][ENT_NEXT_GENERAL_ENT];
			}

		}
	}
	
}



void OBCOLL_sleep_direction (int direction, int start)
{
	// Just calls the area routine after figuring out the correct co-ords.

	int sx,sy,ex,ey;

	switch(direction)
	{
	case DIRECTION_UP:
		sy = 0;
		ey = start;
		sx = 0;
		ex = game_world_width;
		break;

	case DIRECTION_DOWN:
		sy = start;
		ey = game_world_height;
		sx = 0;
		ex = game_world_width;
		break;

	case DIRECTION_LEFT:
		sy = 0;
		ey = game_world_height;
		sx = 0;
		ex = start;
		break;

	case DIRECTION_RIGHT:
		sy = 0;
		ey = game_world_height;
		sx = start;
		ex = game_world_width;
		break;

	default:
		break;
	}

	OBCOLL_sleep_area (sx, sy, ex, ey);
}



void OBCOLL_wake_direction (int direction, int start)
{
	// Just calls the area routine after figuring out the correct co-ords.

	int sx,sy,ex,ey;

	switch(direction)
	{
	case DIRECTION_UP:
		sy = 0;
		ey = start;
		sx = 0;
		ex = game_world_width;
		break;

	case DIRECTION_DOWN:
		sy = start;
		ey = game_world_height;
		sx = 0;
		ex = game_world_width;
		break;

	case DIRECTION_LEFT:
		sy = 0;
		ey = game_world_height;
		sx = 0;
		ex = start;
		break;

	case DIRECTION_RIGHT:
		sy = 0;
		ey = game_world_height;
		sx = start;
		ex = game_world_width;
		break;

	default:
		break;
	}

	OBCOLL_wake_area (sx, sy, ex, ey);
}



int OBCOLL_get_first_in_bucket (int bucket_x , int bucket_y)
{
	if (bucket_x<0)
	{
		return UNSET;
	}
	else if (bucket_x > general_area_buckets_per_row)
	{
		return UNSET;
	}

	if (bucket_y<0)
	{
		return UNSET;
	}
	else if (bucket_y > general_area_buckets_per_col)
	{
		return UNSET;
	}

	int offset = bucket_x + (bucket_y * collision_buckets_per_row);

	return general_area_map_starts[offset];
}



void OBCOLL_add_to_general_area_bucket (int entity_id)
{
	if (general_area_update_on == false)
	{
		return;
	}

	int *entity_pointer = &entity[entity_id][0];

	int bucket_x,bucket_y,prev_id,offset;

	// Then find the correct bucket.
	bucket_x = entity_pointer[ENT_WORLD_X] >> general_area_bitshift;
	bucket_y = entity_pointer[ENT_WORLD_Y] >> general_area_bitshift;

	if (bucket_x<0)
	{
		bucket_x=0;
	}
	else if (bucket_x > general_area_buckets_per_row)
	{
		bucket_x=general_area_buckets_per_row;
	}

	if (bucket_y<0)
	{
		bucket_y=0;
	}
	else if (bucket_y > general_area_buckets_per_col)
	{
		bucket_y=general_area_buckets_per_col;
	}

	offset = bucket_x + (bucket_y * collision_buckets_per_row);

	entity_pointer[ENT_GENERAL_ENT_BUCKET] = offset;
	entity_pointer[ENT_GENERAL_DUMP_DATE] = frames_so_far;

	if (general_area_map_starts[offset] == UNSET)
	{
		// If it's the first thing to go into this bucket...
		general_area_map_starts [offset] = entity_id;
		general_area_map_ends [offset] = entity_id;
		entity_pointer[ENT_PREV_GENERAL_ENT] = UNSET;
		entity_pointer[ENT_NEXT_GENERAL_ENT] = UNSET;
	}
	else
	{
		// There's already summat in there.
		prev_id = general_area_map_ends [offset];
		general_area_map_ends [offset] = entity_id;
		entity_pointer[ENT_PREV_GENERAL_ENT] = prev_id;
		entity_pointer[ENT_NEXT_GENERAL_ENT] = UNSET;
		entity [prev_id][ENT_NEXT_GENERAL_ENT] = entity_id;
	}

}



void OBCOLL_remove_from_general_area_bucket (int entity_id)
{
	if (general_area_update_on == false)
	{
		return;
	}

	int *entity_pointer = &entity[entity_id][0];

	if (entity_pointer[ENT_GENERAL_DUMP_DATE] < last_frame_size_changed)
	{
		// This means that the general map array has been updated
		// since this entity was added to it. So we just blank its
		// value as it's dirty. DIRTY!
		entity_pointer[ENT_GENERAL_ENT_BUCKET] = UNSET;
	}
	else if (entity_pointer[ENT_GENERAL_ENT_BUCKET] == UNSET)
	{
		// This entity hasn't even been put into a bucket yet. Aw. Leave it!
	}
	else
	{
		// Aha, one that's fresh as a daisy and has a value!

		int offset = entity_pointer[ENT_GENERAL_ENT_BUCKET];
		int next_id,prev_id;

		if ( (entity_pointer[ENT_PREV_GENERAL_ENT] == UNSET) && (entity_pointer[ENT_NEXT_GENERAL_ENT] == UNSET) )
		{
			// It's not linked either from the front or back so it's the only thing in the bucket.
			general_area_map_starts [offset] = UNSET;
			general_area_map_ends [offset] = UNSET;
		}
		else if (entity_pointer[ENT_PREV_GENERAL_ENT] == UNSET)
		{
			// It's the first thing in the bucket because the previous is UNSET.
			
			// Tell the next entity that it's prev is UNSET.
			next_id = entity_pointer[ENT_NEXT_GENERAL_ENT];
			entity[next_id][ENT_PREV_GENERAL_ENT] = UNSET;

			// Change the first entity in the bucket to be the old one's next in line.
			general_area_map_starts [offset] = next_id;

			// And reset the old one's next to UNSET.
			entity_pointer[ENT_NEXT_GENERAL_ENT] = UNSET;

		}
		else if (entity_pointer[ENT_NEXT_GENERAL_ENT] == UNSET)
		{
			// It's the last thing in the queue because it's next entity is UNSET.

			// Tell the prev entity that it's next is UNSET.
			prev_id = entity_pointer[ENT_PREV_GENERAL_ENT];
			entity[prev_id][ENT_NEXT_GENERAL_ENT] = UNSET;

			// Change the last entity in the bucket to be the old one's prev in line.
			general_area_map_ends [offset] = prev_id;

			// And reset the old one's prev to UNSET.
			entity_pointer[ENT_PREV_GENERAL_ENT] = UNSET;
		}
		else
		{
			// It's in the middle of the pile because neither next nor prev is unset.
		
			// In this case the bucket is unaffected and we just swap links.
			prev_id = entity_pointer[ENT_PREV_GENERAL_ENT];
			next_id = entity_pointer[ENT_NEXT_GENERAL_ENT];
			entity[next_id][ENT_PREV_GENERAL_ENT] = prev_id;
			entity[prev_id][ENT_NEXT_GENERAL_ENT] = next_id;

			// And reset the old one's prev and next to UNSET.
			entity_pointer[ENT_PREV_GENERAL_ENT] = UNSET;
			entity_pointer[ENT_NEXT_GENERAL_ENT] = UNSET;
		}

		entity_pointer[ENT_GENERAL_ENT_BUCKET] = UNSET;
	}
}



void OBCOLL_create_general_lookup_buffers (void)
{
	last_frame_size_changed = frames_so_far;

	if (general_area_map_starts != NULL)
	{
		free (general_area_map_starts);
		general_area_map_starts = NULL;
	}

	if (general_area_map_ends != NULL)
	{
		free (general_area_map_ends);
		general_area_map_ends = NULL;
	}
	
	general_area_buckets_per_row = (game_world_width >> general_area_bitshift) + 1;
	general_area_buckets_per_col = (game_world_height >> general_area_bitshift) + 1;

	max_general_area_bucket_offset = general_area_buckets_per_row * general_area_buckets_per_col;

	general_area_map_starts = (int *) malloc ( max_general_area_bucket_offset * sizeof(int) );
	general_area_map_ends = (int *) malloc ( max_general_area_bucket_offset * sizeof(int) );

	if (general_area_map_starts == NULL)
	{
		assert(0);
	}
	
	if (general_area_map_ends == NULL)
	{
		assert(0);
	}

	// Set all the buffers to locked...

	int t;

	for (t=0; t<max_general_area_bucket_offset; t++)
	{
		general_area_map_starts[t]=UNSET;
		general_area_map_ends[t]=UNSET;
	}
}



void OBCOLL_turn_on_general_area_update (void)
{
	general_area_update_on = true;
	OBCOLL_create_general_lookup_buffers ();
}



void OBCOLL_turn_off_general_area_update (void)
{
	general_area_update_on = false;

	if (general_area_map_starts != NULL)
	{
		free(general_area_map_starts);
	}

	if (general_area_map_ends != NULL)
	{
		free(general_area_map_ends);
	}
}



void OBCOLL_set_game_world_size (int width, int height, int new_bitshift)
{
	next_frames_game_world_width = width;
	next_frames_game_world_height = height;
	next_frames_collision_bitshift = new_bitshift;
}



void OBCOLL_update_buffer(void)
{
	if ( (next_frames_game_world_width != game_world_width) || (next_frames_game_world_height != game_world_height) )
	{
		// This is so that the collision buckets are emptied at the start
		// of the frame rather than the end (ie, while they're closed).
		OBCOLL_create_collision_buffers ();

		if (general_area_update_on)
		{
			OBCOLL_create_general_lookup_buffers ();
		}
	}
	else if (next_frames_collision_bitshift != collision_bitshift)
	{
		// This is so that the collision buckets are emptied at the start
		// of the frame rather than the end (ie, while they're closed).
		OBCOLL_create_collision_buffers ();
	}
}



void OBCOLL_setup_everything (void)
{
	active_collision_area.current_x = 0;
	active_collision_area.current_y = 0;
	active_collision_area.current_width = 0;
	active_collision_area.current_height = 0;
	
	active_collision_area.new_x = 0;
	active_collision_area.new_y = 0;
	active_collision_area.new_width = 0;
	active_collision_area.new_height = 0;

	active_collision_area.shifted_start_x = 0;
	active_collision_area.shifted_start_y = 0;
	active_collision_area.shifted_end_x = 0;
	active_collision_area.shifted_end_y = 0;
}



void OBCOLL_position_collision_window (int x, int y, int width, int height)
{
	active_collision_area.new_x = x;
	active_collision_area.new_y = y;
	active_collision_area.new_width = width;
	active_collision_area.new_height = height;
}



void OBCOLL_update_collision_window (void)
{
	active_collision_area.current_x = active_collision_area.new_x;
	active_collision_area.current_y = active_collision_area.new_y;
	active_collision_area.current_width = active_collision_area.new_width;
	active_collision_area.current_height = active_collision_area.new_height;
	active_collision_area.backup_data_pointer++;
	active_collision_area.backup_data_pointer %= OBCOLL_BACKUP_DATA_SIZE;
}



void OBCOLL_open_up_collision_buckets (void)
{
	if (object_collision_on == false)
	{
		return;
	}

	int x,y;
	int start_x,start_y,end_x,end_y;
	int offset;

	start_x = active_collision_area.current_x;
	start_y = active_collision_area.current_y;
	end_x = start_x + active_collision_area.current_width;
	end_y = start_y + active_collision_area.current_height;

	start_x = start_x >> collision_bitshift;
	start_y = start_y >> collision_bitshift;

	end_x = (end_x >> collision_bitshift) + 1;
	end_y = (end_y >> collision_bitshift) + 1;

	if (start_x < 0)
	{
		start_x = 0;
	}
	if (start_y < 0)
	{
		start_y = 0;
	}
	if (end_x > collision_buckets_per_row)
	{
		end_x = collision_buckets_per_row;
	}
	if (end_y > collision_buckets_per_col)
	{
		end_y = collision_buckets_per_col;
	}
	if (start_x >= end_x || start_y >= end_y)
	{
		return;
	}

	active_collision_area.shifted_start_x = start_x;
	active_collision_area.shifted_start_y = start_y;
	active_collision_area.shifted_end_x = end_x;
	active_collision_area.shifted_end_y = end_y;

	int counter = active_collision_area.backup_data_pointer;

	active_collision_area.backup_old_sx[counter] = start_x;
	active_collision_area.backup_old_sy[counter] = start_y;
	active_collision_area.backup_old_ex[counter] = end_x;
	active_collision_area.backup_old_ey[counter] = end_y;

	for (y=start_y; y<end_y; y++)
	{
		offset = y * collision_buckets_per_row + start_x;

		for (x=start_x; x<end_x; x++)
		{
			collision_bucket_starts[offset] = UNSET;
			collision_bucket_ends[offset] = UNSET;
			collision_bucket_aggregate_masks[offset] = 0;

			offset++;

			// Faster than doing a multiply for every box.
		}
	}

	OBCOLL_check_integrity_of_all_buckets();
}



void OBCOLL_close_down_collision_buckets (void)
{
	if (object_collision_on == false)
	{
		return;
	}

	int x,y;
	int start_x,start_y,end_x,end_y;
	int offset;

	start_x = active_collision_area.current_x;
	start_y = active_collision_area.current_y;
	end_x = start_x + active_collision_area.current_width;
	end_y = start_y + active_collision_area.current_height;

	start_x = start_x >> collision_bitshift;
	start_y = start_y >> collision_bitshift;

	end_x = (end_x >> collision_bitshift) + 1;
	end_y = (end_y >> collision_bitshift) + 1;

	if (start_x < 0)
	{
		start_x = 0;
	}
	if (start_y < 0)
	{
		start_y = 0;
	}
	if (end_x > collision_buckets_per_row)
	{
		end_x = collision_buckets_per_row;
	}
	if (end_y > collision_buckets_per_col)
	{
		end_y = collision_buckets_per_col;
	}
	if (start_x >= end_x || start_y >= end_y)
	{
		return;
	}

	for (y=start_y; y<end_y; y++)
	{
		offset = y * collision_buckets_per_row + start_x;

		for (x=start_x; x<end_x; x++)
		{
			collision_bucket_starts[offset] = LOCKED;
			collision_bucket_ends[offset] = LOCKED;

			offset++;
			// Faster than doing a multiply for every box.
		}
	}

	OBCOLL_check_integrity_of_all_buckets();
}



void OBCOLL_free_collision_buffers (void)
{
	object_collision_on = false;

	if (collision_bucket_aggregate_masks != NULL)
	{
		free (collision_bucket_aggregate_masks);
		collision_bucket_aggregate_masks = NULL;
	}

	if (collision_bucket_starts != NULL)
	{
		free (collision_bucket_starts);
		collision_bucket_starts = NULL;
	}

	if (collision_bucket_ends != NULL)
	{
		free (collision_bucket_ends);
		collision_bucket_ends = NULL;
	}
}


#define OBCOLL_DRAW_COLLISION_BUCKET_GRID_WIDTH		(320)
#define OBCOLL_DRAW_COLLISION_BUCKET_GRID_HEIGHT	(128)

void OBCOLL_draw_collision_buckets (void)
{
	if (collision_bucket_starts != NULL)
	{
		int cell_width = OBCOLL_DRAW_COLLISION_BUCKET_GRID_WIDTH / collision_buckets_per_row;
		int cell_height = OBCOLL_DRAW_COLLISION_BUCKET_GRID_HEIGHT / collision_buckets_per_col;

		int cell_number;

		int cell_x;
		int cell_y;

		for (cell_number=0; cell_number<max_collision_bucket_offset; cell_number++)
		{
			cell_x = (cell_number % collision_buckets_per_row) * cell_width;
			cell_y = (cell_number / collision_buckets_per_row) * cell_height;

			if (collision_bucket_starts[cell_number] == LOCKED)
			{
				OUTPUT_rectangle_by_size (cell_x,cell_y,cell_width,cell_height,255,0,0);
			}
			else if (collision_bucket_starts[cell_number] == UNSET)
			{
				OUTPUT_rectangle_by_size (cell_x,cell_y,cell_width,cell_height,0,0,255);
			}
			else if (collision_bucket_starts[cell_number] > UNSET)
			{
				OUTPUT_filled_rectangle_by_size (cell_x,cell_y,cell_width,cell_height,0,255,0);
			}
		}
	}

}



void OBCOLL_create_collision_buffers (void)
{
	// This allocates the memory for the layer/x/y buffers (after first freeing it).
	// It also sets up the size of the buckets and stuff...

	object_collision_on = true;

	if (collision_bucket_aggregate_masks != NULL)
	{
		free (collision_bucket_aggregate_masks);
		collision_bucket_aggregate_masks = NULL;
	}

	if (collision_bucket_starts != NULL)
	{
		free (collision_bucket_starts);
		collision_bucket_starts = NULL;
	}

	if (collision_bucket_ends != NULL)
	{
		free (collision_bucket_ends);
		collision_bucket_ends = NULL;
	}
	
	collision_bitshift = next_frames_collision_bitshift;
	game_world_width = next_frames_game_world_width;
	game_world_height = next_frames_game_world_height;

	collision_buckets_per_row = (game_world_width >> collision_bitshift) + 1;
	collision_buckets_per_col = (game_world_height >> collision_bitshift) + 1;

	max_collision_bucket_offset = collision_buckets_per_row * collision_buckets_per_col;

	collision_bucket_aggregate_masks = (int *) malloc (max_collision_bucket_offset * sizeof(int) );
	collision_bucket_starts = (int *) malloc ( max_collision_bucket_offset * sizeof(int) );
	collision_bucket_ends = (int *) malloc ( max_collision_bucket_offset * sizeof(int) );

	if (collision_bucket_aggregate_masks == NULL)
	{
		assert(0);
	}

	if (collision_bucket_starts == NULL)
	{
		assert(0);
	}

	if (collision_bucket_ends == NULL)
	{
		assert(0);
	}

	// Set all the buffers to locked...

	int t;

	for (t=0; t<max_collision_bucket_offset; t++)
	{
		collision_bucket_starts[t]=LOCKED;
		collision_bucket_ends[t]=LOCKED;
	}

}



void OBCOLL_add_to_collision_bucket (int entity_id)
{
	if (object_collision_on == false)
	{
		return;
	}

	int bucket_x,bucket_y,prev_id,offset;

	if (entity[entity_id][ENT_COLLIDE_TYPE] != 0) // If it collides at all
	{
		// Then find the correct bucket.
		bucket_x = entity[entity_id][ENT_WORLD_X] >> collision_bitshift;
		bucket_y = entity[entity_id][ENT_WORLD_Y] >> collision_bitshift;

		if ((bucket_x<0) || (bucket_x >= collision_buckets_per_row) )
		{
			return;
		}

		offset = bucket_x + (bucket_y * collision_buckets_per_row);

		if ( (offset < 0) || (offset>=max_collision_bucket_offset) )
		{
			return;
		}

		#ifdef RETRENGINE_DEBUG_CHECK_ADDITIONS_TO_COLLISION_BUCKETS
		{
			int searching_entity_id = collision_bucket_starts [offset];

			while ( (searching_entity_id != UNSET) && (searching_entity_id != LOCKED) )
			{
				if (searching_entity_id == entity_id)
				{
					char filename[MAX_LINE_SIZE];
					sprintf(filename,"duplicate_bucket_on_frame_%i",frames_so_far);
					CONTROL_stop_and_save_active_channels (filename);
					
					OUTPUT_message("Attempting to add duplicate to collision bucket!");
					assert(0);
				}

				searching_entity_id = entity[searching_entity_id][ENT_NEXT_COLLISION_ENT];
			}
		}
		#endif

		if (collision_bucket_starts[offset] != LOCKED)
		{
			#ifdef RETRENGINE_DEBUG_CHECK_ADDITIONS_TO_COLLISION_BUCKETS
				// This is our first line check to see if we're attempting to add to a bucket which shouldn't even be open.
				if ( (bucket_x < active_collision_area.shifted_start_x) || (bucket_y < active_collision_area.shifted_start_y) || (bucket_x >= active_collision_area.shifted_end_x) || (bucket_y >= active_collision_area.shifted_end_y) )
				{
					char filename[MAX_LINE_SIZE];
					sprintf(filename,"adding_to_bucket_out_of_range_on_frame_%i",frames_so_far);
					CONTROL_stop_and_save_active_channels (filename);
					
					OUTPUT_message("Attempting to add to out of range bucket which isn't locked!");
					assert(0);
				}
			#endif

			collision_bucket_aggregate_masks[offset] |= entity[entity_id][ENT_COLLIDE_TYPE];

			entity[entity_id][ENT_COLLISION_BUCKET_NUMBER] = offset;

			if (collision_bucket_starts[offset] == UNSET)
			{
				// If it's the first thing to go into this bucket...
				collision_bucket_starts [offset] = entity_id;
				collision_bucket_ends [offset] = entity_id;
				entity [entity_id][ENT_PREV_COLLISION_ENT] = UNSET;
				entity [entity_id][ENT_NEXT_COLLISION_ENT] = UNSET;
			}
			else
			{
				// There's already summat in there.
				prev_id = collision_bucket_ends [offset];
				collision_bucket_ends [offset] = entity_id;
				entity [entity_id][ENT_PREV_COLLISION_ENT] = prev_id;
				entity [entity_id][ENT_NEXT_COLLISION_ENT] = UNSET;
				entity [prev_id][ENT_NEXT_COLLISION_ENT] = entity_id;
			}
		}
		else
		{
			entity[entity_id][ENT_COLLISION_BUCKET_NUMBER] = UNSET;
		}
	}
}



void OBCOLL_remove_from_collision_bucket (int entity_id)
{
	if (object_collision_on == false)
	{
		return;
	}

	int prev_id,next_id;
	int offset;

	offset = entity[entity_id][ENT_COLLISION_BUCKET_NUMBER];

	if (offset == UNSET)
	{
		return;
	}

	#ifdef RETRENGINE_DEBUG_CHECK_REMOVALS_FROM_COLLISION_BUCKETS
		int bucket_x = offset % collision_buckets_per_row;
		int bucket_y = offset / collision_buckets_per_row;

		// This is our first line check to see if we're attempting to add to a bucket which shouldn't even be open.
		if ( (bucket_x < active_collision_area.shifted_start_x) || (bucket_y < active_collision_area.shifted_start_y) || (bucket_x >= active_collision_area.shifted_end_x) || (bucket_y >= active_collision_area.shifted_end_y) )
		{
			char filename[MAX_LINE_SIZE];
			sprintf(filename,"removing_from_bucket_out_of_range_on_frame_%i",frames_so_far);
			CONTROL_stop_and_save_active_channels (filename);
			
			OUTPUT_message("Attempting to remove from out of range bucket which isn't locked!");
			assert(0);
		}
	#endif

	// Okay, now the data in the entity could be REALLY old and smelly, so we first check that it is even in this bucket...

	if ( (entity[entity_id][ENT_PREV_COLLISION_ENT] == UNSET) && (entity[entity_id][ENT_NEXT_COLLISION_ENT] == UNSET) )
	{
		// It's not linked either from the front or back so it's the only thing in the bucket.

		// Just blank the bucket.
		collision_bucket_starts [offset] = UNSET;
		collision_bucket_ends [offset] = UNSET;
	}
	else if (entity[entity_id][ENT_PREV_COLLISION_ENT] == UNSET)
	{
		// It's the first thing in the bucket because the previous is UNSET.
		
		// Tell the next entity that it's prev is UNSET.
		next_id = entity[entity_id][ENT_NEXT_COLLISION_ENT];
		entity[next_id][ENT_PREV_COLLISION_ENT] = UNSET;

		// Change the first entity in the bucket to be the old one's next in line.
		collision_bucket_starts [offset] = next_id;

		// And reset the old one's next to UNSET.
		entity[entity_id][ENT_NEXT_COLLISION_ENT] = UNSET;

	}
	else if (entity[entity_id][ENT_NEXT_COLLISION_ENT] == UNSET)
	{
		// It's the last thing in the queue because it's next entity is UNSET.

		// Tell the prev entity that it's next is UNSET.
		prev_id = entity[entity_id][ENT_PREV_COLLISION_ENT];
		entity[prev_id][ENT_NEXT_COLLISION_ENT] = UNSET;

		// Change the last entity in the bucket to be the old one's prev in line.
		collision_bucket_ends [offset] = prev_id;

		// And reset the old one's prev to UNSET.
		entity[entity_id][ENT_PREV_COLLISION_ENT] = UNSET;
	}
	else
	{
		// It's in the middle of the pile because neither next nor prev is unset.
	
		// In this case the bucket is unaffected and we just swap links.
		prev_id = entity[entity_id][ENT_PREV_COLLISION_ENT];
		next_id = entity[entity_id][ENT_NEXT_COLLISION_ENT];
		entity[next_id][ENT_PREV_COLLISION_ENT] = prev_id;
		entity[prev_id][ENT_NEXT_COLLISION_ENT] = next_id;

		// And reset the old one's prev and next to UNSET.
		entity[entity_id][ENT_PREV_COLLISION_ENT] = UNSET;
		entity[entity_id][ENT_NEXT_COLLISION_ENT] = UNSET;
	}

	entity[entity_id][ENT_COLLISION_BUCKET_NUMBER] = UNSET;

//	OBCOLL_check_bucket_762 ();
}



void OBCOLL_convert_and_constrain_bucket_co_ords ( int *first_bucket_x, int *first_bucket_y, int *last_bucket_x, int *last_bucket_y)
{
	// Just to save replication of code, really.

	*first_bucket_x -= MAX_PIXEL_SPREAD;
	*first_bucket_y -= MAX_PIXEL_SPREAD;
	*last_bucket_x += MAX_PIXEL_SPREAD;
	*last_bucket_y += MAX_PIXEL_SPREAD;

	*first_bucket_x >>= collision_bitshift;
	*first_bucket_y >>= collision_bitshift;
	*last_bucket_x >>= collision_bitshift;
	*last_bucket_y >>= collision_bitshift;

	if (*first_bucket_x < 0)
	{
		*first_bucket_x = 0;
	}

	if (*first_bucket_y < 0)
	{
		*first_bucket_y = 0;
	}

	if (*last_bucket_x >= collision_buckets_per_row)
	{
		*last_bucket_x = collision_buckets_per_row - 1;
	}

	if (*last_bucket_y >= collision_buckets_per_col)
	{
		*last_bucket_y = collision_buckets_per_col - 1;
	}

}



bool OBCOLL_collide_entity_pair (int entity_id, int other_entity_id)
{
	int ent_damage,other_ent_damage;

	int *entity_pointer;
	int *other_entity_pointer;

	entity_pointer = &entity[entity_id][0];
	other_entity_pointer = &entity[other_entity_id][0];

	// Righty, so we scored a hit, calculate the relevant damages.

	ent_damage = other_entity_pointer[ENT_DAMAGE];

	if ( entity_pointer[ENT_HEALTH_SUPERSENSITIZED] & other_entity_pointer[ENT_DAMAGE_TYPE] )
	{
		ent_damage *= 2;
	}
	else if ( entity_pointer[ENT_HEALTH_DESENSITIZED] & other_entity_pointer[ENT_DAMAGE_TYPE] )
	{
		ent_damage = 0;
	}

	other_ent_damage = other_entity_pointer[ENT_DAMAGE];

	if (other_entity_pointer[ENT_HEALTH_SUPERSENSITIZED] & entity_pointer[ENT_DAMAGE_TYPE] )
	{
		other_ent_damage *= 2;
	}
	else if (other_entity_pointer[ENT_HEALTH_DESENSITIZED] & entity_pointer[ENT_DAMAGE_TYPE] )
	{
		other_ent_damage = 0;
	}

//	SCRIPTING_check_for_collide_type_offset_mismatch();

	// And dump them into the relevent variables.

	other_entity_pointer[ENT_DAMAGE_RECEIVED] = other_ent_damage;
	entity_pointer[ENT_DAMAGE_RECEIVED] = ent_damage;

	// Tell each entity who it was who grossly offended them.

	entity_pointer[ENT_COLLIDED_ENTITY] = other_entity_id;
	other_entity_pointer[ENT_COLLIDED_ENTITY] = entity_id;

	// Then if a hitline has been set for either entity, call their script.

	if (other_entity_pointer[ENT_ENTITY_HIT_LINE] != 0)
	{
		SCRIPTING_interpret_script (other_entity_id , other_entity_pointer[ENT_ENTITY_HIT_LINE]);
	}

	if (entity_pointer[ENT_ENTITY_HIT_LINE] != 0)
	{
		SCRIPTING_interpret_script (entity_id , entity_pointer[ENT_ENTITY_HIT_LINE]);
		
		if (entity_pointer[ENT_ALIVE] <= DEAD)
		{
			// Well, if it popped it's clogs in the script there's no point in continuing to collide stuff with it.
			return true;
		}
	}

	return false;
}



void OBCOLL_collide_point_with_buckets (int entity_id)
{
	int first_bucket_x,first_bucket_y;
	int last_bucket_x,last_bucket_y;
	int entity_bx,entity_by;
	int other_entity_id;
	bool collide = false;

	int *entity_pointer;
	int *other_entity_pointer;

	entity_pointer = &entity[entity_id][0];

	int x = entity_pointer[ENT_WORLD_X];
	int y = entity_pointer[ENT_WORLD_Y];

	rectangle r;

	first_bucket_x = x;
	first_bucket_y = y;
	last_bucket_x = x;
	last_bucket_y = y;

	OBCOLL_convert_and_constrain_bucket_co_ords ( &first_bucket_x, &first_bucket_y, &last_bucket_x, &last_bucket_y );

	for (entity_bx = first_bucket_x; entity_bx < last_bucket_x; entity_bx++)
	{
		for (entity_by = first_bucket_y; entity_by < last_bucket_y; entity_by++)
		{
			if (entity_pointer[ENT_COLLIDE_WITH] & collision_bucket_aggregate_masks[entity_bx + (entity_by * collision_buckets_per_row)])
			{
				other_entity_id = collision_bucket_starts [entity_bx + (entity_by * collision_buckets_per_row)]; // Get the first ID in the bucket.

				while ( (other_entity_id != UNSET) && (other_entity_id != LOCKED) )
				{
					other_entity_pointer = &entity[other_entity_id][0];

					if (other_entity_id != entity_id)
					{
						// I'm farming it out to other routines just for clarity and brevity of functions.

						collision_actual_count++;

						if (( entity_pointer[ENT_COLLIDE_WITH] & other_entity_pointer[ENT_COLLIDE_TYPE] ) > 0)
						{
							collide = false;

							switch (other_entity_pointer[ENT_COLLISION_SHAPE])
							{
							case SHAPE_POINT:
								// Ignore - there should be no point-to-point collisions as only bullets use point type.
								break;

							case SHAPE_CIRCLE:
								collide = MATH_circle_to_point_intersect ( other_entity_pointer[ENT_WORLD_X] , other_entity_pointer[ENT_WORLD_Y] , other_entity_pointer[ENT_RADIUS] , x , y );
								break;

							case SHAPE_RECTANGLE:
								collide = MATH_rectangle_to_point_intersect ( other_entity_pointer[ENT_WORLD_X]-other_entity_pointer[ENT_UPPER_WIDTH] , other_entity_pointer[ENT_WORLD_Y]-other_entity_pointer[ENT_UPPER_HEIGHT] , other_entity_pointer[ENT_WORLD_X]+other_entity_pointer[ENT_LOWER_WIDTH] , other_entity_pointer[ENT_WORLD_Y]+other_entity_pointer[ENT_LOWER_HEIGHT] , x, y );
								break;

							case SHAPE_ROTATED_RECTANGLE:
								MATH_make_rectangle ( &r , other_entity_pointer[ENT_WORLD_X] , other_entity_pointer[ENT_WORLD_Y] , other_entity_pointer[ENT_UPPER_WIDTH] , other_entity_pointer[ENT_UPPER_HEIGHT] , other_entity_pointer[ENT_LOWER_WIDTH] , other_entity_pointer[ENT_LOWER_HEIGHT] , float(other_entity_pointer[ENT_OPENGL_ANGLE])/angle_conversion_ratio );
								MATH_rotate_and_translate_rect ( &r );
								collide = MATH_rotated_rectangle_to_point_intersect ( &r , x, y );
								break;

							default:

								// Nothing to see here! Move along please!
								break;
							}

							if (collide == true)
							{
								if (OBCOLL_collide_entity_pair (entity_id, other_entity_id) == true)
								{
									return;
								}
							}

						}
					}
					
					other_entity_id = other_entity_pointer[ENT_NEXT_COLLISION_ENT];
				}
			}
		}
	}

}



void OBCOLL_collide_circle_with_buckets (int entity_id)
{
	int first_bucket_x,first_bucket_y;
	int last_bucket_x,last_bucket_y;
	int entity_bx,entity_by;
	int other_entity_id;
	bool collide = false;

	int *entity_pointer;
	int *other_entity_pointer;

	entity_pointer = &entity[entity_id][0];

	int x = entity_pointer[ENT_WORLD_X];
	int y = entity_pointer[ENT_WORLD_Y];
	int radius = entity_pointer[ENT_RADIUS];

	rectangle r;

	first_bucket_x = x-radius;
	first_bucket_y = y-radius;
	last_bucket_x = x+radius;
	last_bucket_y = y+radius;

	OBCOLL_convert_and_constrain_bucket_co_ords ( &first_bucket_x, &first_bucket_y, &last_bucket_x, &last_bucket_y );

	for (entity_bx = first_bucket_x; entity_bx < last_bucket_x; entity_bx++)
	{
		for (entity_by = first_bucket_y; entity_by < last_bucket_y; entity_by++)
		{
			if (entity_pointer[ENT_COLLIDE_WITH] & collision_bucket_aggregate_masks[entity_bx + (entity_by * collision_buckets_per_row)])
			{
				other_entity_id = collision_bucket_starts [entity_bx + (entity_by * collision_buckets_per_row)]; // Get the first ID in the bucket.

				while ( (other_entity_id != UNSET) && (other_entity_id != LOCKED) )
				{
					other_entity_pointer = &entity[other_entity_id][0];

					if (other_entity_id != entity_id)
					{
						// I'm farming it out to other routines just for clarity and brevity of functions.

						collision_actual_count++;

						if (( entity_pointer[ENT_COLLIDE_WITH] & other_entity_pointer[ENT_COLLIDE_TYPE] ) > 0)
						{
							// No need to compare entity id's to check that they aren't colliding with themselves as
							// nothing will have overlapping COLLIDE_TYPE and COLLIDE_WITH masks.

							collide = false;

							switch (other_entity_pointer[ENT_COLLISION_SHAPE])
							{
							case SHAPE_POINT:
								collide = MATH_circle_to_point_intersect ( x , y , radius , other_entity_pointer[ENT_WORLD_X] , other_entity_pointer[ENT_WORLD_Y] );
								break;

							case SHAPE_CIRCLE:
								collide = MATH_circle_to_circle_intersect ( x , y , radius , other_entity_pointer[ENT_WORLD_X] , other_entity_pointer[ENT_WORLD_Y] , other_entity_pointer[ENT_RADIUS] );
								break;

							case SHAPE_RECTANGLE:
								collide = MATH_rectangle_to_circle_intersect ( other_entity_pointer[ENT_WORLD_X]-other_entity_pointer[ENT_UPPER_WIDTH] , other_entity_pointer[ENT_WORLD_Y]-other_entity_pointer[ENT_UPPER_HEIGHT] , other_entity_pointer[ENT_WORLD_X]+other_entity_pointer[ENT_LOWER_WIDTH] , other_entity_pointer[ENT_WORLD_Y]+other_entity_pointer[ENT_LOWER_HEIGHT] , x , y , radius );
								break;

							case SHAPE_ROTATED_RECTANGLE:
								MATH_make_rectangle ( &r , other_entity_pointer[ENT_WORLD_X] , other_entity_pointer[ENT_WORLD_Y] , other_entity_pointer[ENT_UPPER_WIDTH] , other_entity_pointer[ENT_UPPER_HEIGHT] , other_entity_pointer[ENT_LOWER_WIDTH] , other_entity_pointer[ENT_LOWER_HEIGHT] , float(other_entity_pointer[ENT_OPENGL_ANGLE])/angle_conversion_ratio );
								MATH_rotate_and_translate_rect ( &r );
								collide = MATH_rotated_rectangle_to_circle_intersect ( &r , x , y , radius );
								break;

							default:

								// Nothing to see here! Move along please!
								break;
							}

							if (collide == true)
							{
								if (OBCOLL_collide_entity_pair (entity_id, other_entity_id) == true)
								{
									return;
								}
							}

						}
					}

					other_entity_id = other_entity_pointer[ENT_NEXT_COLLISION_ENT];
					
				}
			}
		}
	}

}



void OBCOLL_collide_rectangle_with_buckets (int entity_id)
{
	int first_bucket_x,first_bucket_y;
	int last_bucket_x,last_bucket_y;
	int entity_bx,entity_by;
	int other_entity_id;
	bool collide = false;

	int *entity_pointer;
	int *other_entity_pointer;

	entity_pointer = &entity[entity_id][0];

	int x,y;

	x = entity_pointer[ENT_WORLD_X];
	y = entity_pointer[ENT_WORLD_Y];

	int rect_x1 = x - entity_pointer[ENT_UPPER_WIDTH];
	int rect_y1 = y - entity_pointer[ENT_UPPER_HEIGHT];
	int rect_x2 = x + entity_pointer[ENT_LOWER_WIDTH];
	int rect_y2 = y + entity_pointer[ENT_LOWER_HEIGHT];

	first_bucket_x = rect_x1;
	first_bucket_y = rect_y1;
	last_bucket_x = rect_x2;
	last_bucket_y = rect_y2;

	OBCOLL_convert_and_constrain_bucket_co_ords ( &first_bucket_x, &first_bucket_y, &last_bucket_x, &last_bucket_y );

	rectangle r;

	for (entity_bx = first_bucket_x; entity_bx < last_bucket_x; entity_bx++)
	{
		for (entity_by = first_bucket_y; entity_by < last_bucket_y; entity_by++)
		{
			if (entity_pointer[ENT_COLLIDE_WITH] & collision_bucket_aggregate_masks[entity_bx + (entity_by * collision_buckets_per_row)])
			{

				other_entity_id = collision_bucket_starts [entity_bx + (entity_by * collision_buckets_per_row)]; // Get the first ID in the bucket.

				while ( (other_entity_id != UNSET) && (other_entity_id != LOCKED) )
				{
					other_entity_pointer = &entity[other_entity_id][0];
					
					if  (other_entity_id != entity_id)
					{
						// I'm farming it out to other routines just for clarity and brevity of functions.

						collision_actual_count++;
		//				SCRIPTING_check_for_collide_type_offset_mismatch();

						#ifdef RETRENGINE_DEBUG_VERSION_WHERES_WALLY
						sprintf (wheres_wally,"Now I'm really dealing with entity %i (%i) and entity %i (%i)",entity_id,entity[entity_id][ENT_SCRIPT_NUMBER],other_entity_id,entity[other_entity_id][ENT_SCRIPT_NUMBER]);
						#endif

						if (( entity_pointer[ENT_COLLIDE_WITH] & other_entity_pointer[ENT_COLLIDE_TYPE] ) > 0)
						{
							// No need to compare entity id's to check that they aren't colliding with themselves as
							// nothing will have overlapping COLLIDE_TYPE and COLLIDE_WITH masks.

							collide = false;

							switch (other_entity_pointer[ENT_COLLISION_SHAPE])
							{
							case SHAPE_POINT:
								collide = MATH_rectangle_to_point_intersect ( rect_x1 , rect_y1 , rect_x2 , rect_y2 , other_entity_pointer[ENT_WORLD_X] , other_entity_pointer[ENT_WORLD_Y] );
								break;

							case SHAPE_CIRCLE:
								collide = MATH_rectangle_to_circle_intersect ( rect_x1 , rect_y1 , rect_x2 , rect_y2 , other_entity_pointer[ENT_WORLD_X] , other_entity_pointer[ENT_WORLD_Y] , other_entity_pointer[ENT_RADIUS] );
								break;

							case SHAPE_RECTANGLE:
								collide = MATH_rectangle_to_rectangle_intersect ( other_entity_pointer[ENT_WORLD_X]-other_entity_pointer[ENT_UPPER_WIDTH] , other_entity_pointer[ENT_WORLD_Y]-other_entity_pointer[ENT_UPPER_HEIGHT] , other_entity_pointer[ENT_WORLD_X]+other_entity_pointer[ENT_LOWER_WIDTH] , other_entity_pointer[ENT_WORLD_Y]+other_entity_pointer[ENT_LOWER_HEIGHT] , rect_x1 , rect_y1 , rect_x2 , rect_y2 );
								break;

							case SHAPE_ROTATED_RECTANGLE:
								MATH_make_rectangle ( &r , other_entity_pointer[ENT_WORLD_X] , other_entity_pointer[ENT_WORLD_Y] , other_entity_pointer[ENT_UPPER_WIDTH] , other_entity_pointer[ENT_UPPER_HEIGHT] , other_entity_pointer[ENT_LOWER_WIDTH] , other_entity_pointer[ENT_LOWER_HEIGHT] , float(other_entity_pointer[ENT_OPENGL_ANGLE])/angle_conversion_ratio );
								MATH_rotate_and_translate_rect ( &r );
								collide = MATH_rotated_rectangle_to_rectangle_intersect ( &r , rect_x1 , rect_y1 , rect_x2 , rect_y2 );
								break;

							default:

								// Nothing to see here! Move along please!
								break;
							}
		//					SCRIPTING_check_for_collide_type_offset_mismatch();

							if (collide == true)
							{
								#ifdef RETRENGINE_DEBUG_VERSION_WHERES_WALLY
								sprintf (wheres_wally,"Now I'm smacking the heads together with entity %i",entity_id);
								#endif

								if (OBCOLL_collide_entity_pair (entity_id, other_entity_id) == true)
								{

									return;
								}
							}
		//					SCRIPTING_check_for_collide_type_offset_mismatch();

						}

		//				SCRIPTING_check_for_collide_type_offset_mismatch();
					}

					other_entity_id = other_entity_pointer[ENT_NEXT_COLLISION_ENT];
						
				}
			}
		}
	}

}



void OBCOLL_check_integrity_of_all_buckets (void)
{
	int t;

	int collision_buckets_per_row = (game_world_width >> collision_bitshift) + 1;
	int collision_buckets_per_col = (game_world_height >> collision_bitshift) + 1;

	int max_collision_bucket_offset = collision_buckets_per_row * collision_buckets_per_col;

	if (max_collision_bucket_offset <= 1)
	{
		return;
	}

	for (t=0; t<max_collision_bucket_offset; t++)
	{
		if ( (collision_bucket_starts[t] != LOCKED) && (collision_bucket_starts[t] != UNSET) )
		{
			CONTROL_stop_and_save_active_channels("bucket_integrity_crash");
			assert(0);
		}

		if ( (collision_bucket_ends[t] != LOCKED) && (collision_bucket_ends[t] != UNSET) )
		{
			CONTROL_stop_and_save_active_channels("bucket_integrity_crash");
			assert(0);
		}
	}

}



void OBCOLL_collide_rotated_rectangle_with_buckets (int entity_id)
{
	int first_bucket_x,first_bucket_y;
	int last_bucket_x,last_bucket_y;
	int entity_bx,entity_by;
	int other_entity_id;
	bool collide = false;

	int *entity_pointer;
	int *other_entity_pointer;

	entity_pointer = &entity[entity_id][0];

	rectangle r1,r2;

	MATH_make_rectangle ( &r1 , entity_pointer[ENT_WORLD_X], entity_pointer[ENT_WORLD_Y], entity_pointer[ENT_UPPER_WIDTH] , entity_pointer[ENT_UPPER_HEIGHT] , entity_pointer[ENT_LOWER_WIDTH] , entity_pointer[ENT_LOWER_HEIGHT] , float (entity_pointer[ENT_OPENGL_ANGLE]) / angle_conversion_ratio );//angle_scalar );
	MATH_rotate_and_translate_rect ( &r1 );
	MATH_find_rotated_rectangle_extents ( &r1 , &first_bucket_x , &first_bucket_y , &last_bucket_x , &last_bucket_y );

	OBCOLL_convert_and_constrain_bucket_co_ords ( &first_bucket_x, &first_bucket_y, &last_bucket_x, &last_bucket_y );

	for (entity_bx = first_bucket_x; entity_bx < last_bucket_x; entity_bx++)
	{
		for (entity_by = first_bucket_y; entity_by < last_bucket_y; entity_by++)
		{
			if (entity_pointer[ENT_COLLIDE_WITH] & collision_bucket_aggregate_masks[entity_bx + (entity_by * collision_buckets_per_row)])
			{
				other_entity_id = collision_bucket_starts [entity_bx + (entity_by * collision_buckets_per_row)]; // Get the first ID in the bucket.

				while ( (other_entity_id != UNSET) && (other_entity_id != LOCKED) )
				{
					other_entity_pointer = &entity[other_entity_id][0];

					if (other_entity_id != entity_id)
					{
						// I'm farming it out to other routines just for clarity and brevity of functions.

						collision_actual_count++;

						if (( entity_pointer[ENT_COLLIDE_WITH] & other_entity_pointer[ENT_COLLIDE_TYPE] ) > 0)
						{
							// No need to compare entity id's to check that they aren't colliding with themselves as
							// nothing will have overlapping COLLIDE_TYPE and COLLIDE_WITH masks.

							collide = false;

							switch (other_entity_pointer[ENT_COLLISION_SHAPE])
							{
							case SHAPE_POINT:
								collide = MATH_rotated_rectangle_to_point_intersect ( &r1 , other_entity_pointer[ENT_WORLD_X] , other_entity_pointer[ENT_WORLD_Y] );
								break;

							case SHAPE_CIRCLE:
								collide = MATH_rotated_rectangle_to_circle_intersect ( &r1 , other_entity_pointer[ENT_WORLD_X] , other_entity_pointer[ENT_WORLD_Y] , other_entity_pointer[ENT_RADIUS] );
								break;

							case SHAPE_RECTANGLE:
								collide = MATH_rotated_rectangle_to_rectangle_intersect ( &r1 , other_entity_pointer[ENT_WORLD_X]-other_entity_pointer[ENT_UPPER_WIDTH] , other_entity_pointer[ENT_WORLD_Y]-other_entity_pointer[ENT_UPPER_HEIGHT] , other_entity_pointer[ENT_WORLD_X]+other_entity_pointer[ENT_LOWER_WIDTH] , other_entity_pointer[ENT_WORLD_Y]+other_entity_pointer[ENT_LOWER_HEIGHT] );
								break;

							case SHAPE_ROTATED_RECTANGLE:
								MATH_make_rectangle ( &r2 , other_entity_pointer[ENT_WORLD_X] , other_entity_pointer[ENT_WORLD_Y] , other_entity_pointer[ENT_UPPER_WIDTH] , other_entity_pointer[ENT_UPPER_HEIGHT] , other_entity_pointer[ENT_LOWER_WIDTH] , other_entity_pointer[ENT_LOWER_HEIGHT] , float(other_entity_pointer[ENT_OPENGL_ANGLE])/angle_conversion_ratio );
								MATH_rotate_and_translate_rect ( &r2 );
								collide = MATH_rotated_rectangle_to_rotated_rectangle_intersect ( &r1 , &r2 );
								break;

							default:

								// Nothing to see here! Move along please!
								break;
							}

							if (collide == true)
							{
								if (OBCOLL_collide_entity_pair (entity_id, other_entity_id) == true)
								{
									return;
								}
							}

						}
					}
					
					other_entity_id = other_entity_pointer[ENT_NEXT_COLLISION_ENT];
					
				}
			}
		}
	}

}



void OBCOLL_collision_handler (void)
{
	if (object_collision_on == false)
	{
		return;
	}

	// This goes through the big buckets of collision and for each item figures out its
	// extents and then goes through all those buckets and sees if it hit anything in them.
	// For this reason things should be kept fairly small (ie, no one thing should be, say, 256x256
	// as it'll have to go searching through craploads of buckets). The real problem is rotatable
	// rectangles with pivot points near one end which means the buggers can swing all over the shop.

	// First things first, though, lets start looping through the buckets.

	// Note that entities have 2 flags, their COLLIDE_TYPE and COLLIDE_WITH, to save on replication
	// of effort this means we can have enemies with a COLLIDE_WITH including player bullets, but the
	// bullets themselves with a COLLIDE_WITH of nothing, because the enemy will take care of checking
	// whether it's hit so there's no reason to get the bullets to look around as well.

	// This also helps as bullets are likely to be smaller that most other things so we can keep the
	// size of MAX_PIXEL_SPREAD quite low. Also because it's the enemies themself that tend to be the
	// complex shapes it stops us having to make too make rectangles and stuff.

	int start_bucket_x;
	int start_bucket_y;
	int end_bucket_x;
	int end_bucket_y;

	start_bucket_x = (active_collision_area.current_x >> collision_bitshift);
	start_bucket_y = (active_collision_area.current_y >> collision_bitshift);

	end_bucket_x = ( (active_collision_area.current_x + active_collision_area.current_width) >> collision_bitshift) + 1;
	end_bucket_y = ( (active_collision_area.current_y + active_collision_area.current_height) >> collision_bitshift) + 1;

	if (start_bucket_x < 0)
	{
		start_bucket_x = 0;
	}
	if (start_bucket_y < 0)
	{
		start_bucket_y = 0;
	}
	if (end_bucket_x > collision_buckets_per_row)
	{
		end_bucket_x = collision_buckets_per_row;
	}
	if (end_bucket_y > collision_buckets_per_col)
	{
		end_bucket_y = collision_buckets_per_col;
	}
	if (start_bucket_x >= end_bucket_x || start_bucket_y >= end_bucket_y)
	{
		return;
	}

	int entity_bx,entity_by;
	int entity_id;

	collision_entity_count = 0;
	collision_actual_count = 0;

	for (entity_bx = start_bucket_x; entity_bx < end_bucket_x ; entity_bx++)
	{
		for (entity_by = start_bucket_y; entity_by < end_bucket_y ; entity_by++)
		{
			entity_id = collision_bucket_starts [entity_bx + (entity_by * collision_buckets_per_row)]; // Get the first ID in the bucket.

			while (entity_id != UNSET)
			{
				// I'm farming it out to other routines just for clarity and brevity of functions.
				#ifdef RETRENGINE_DEBUG_VERSION_WHERES_WALLY
				sprintf (wheres_wally,"Now I'm dealing with entity %i",entity_id);
				#endif

//				SCRIPTING_check_for_collide_type_offset_mismatch();

				collision_entity_count++;

				if (entity[entity_id][ENT_COLLIDE_WITH] != UNSET)
				{
					// If it actually bothers to collide with stuff.

					switch (entity[entity_id][ENT_COLLISION_SHAPE])
					{
					case SHAPE_POINT:
						OBCOLL_collide_point_with_buckets (entity_id);
						break;

					case SHAPE_CIRCLE:
						OBCOLL_collide_circle_with_buckets (entity_id);
						break;

					case SHAPE_RECTANGLE:
						OBCOLL_collide_rectangle_with_buckets (entity_id);
						break;

					case SHAPE_ROTATED_RECTANGLE:
						OBCOLL_collide_rotated_rectangle_with_buckets (entity_id);
						break;

					default:
						// Nothing to see here! Move along please!
						break;
					}
				}

				entity_id = entity [entity_id][ENT_NEXT_COLLISION_ENT];
//				SCRIPTING_check_for_collide_type_offset_mismatch();
			}

		}
	}

}
