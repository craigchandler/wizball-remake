#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <math.h>

#include "spawn_points.h"
#include "tilemaps.h" // Duh!
#include "main.h" // For get_project_filename and also state constants
#include "output.h" // Drawing stuff
#include "control.h" // Mouse and keyboard input
#include "global_param_list.h" // For list access
#include "math_stuff.h"
#include "string_stuff.h"
#include "file_stuff.h"
#include "tilesets.h"
#include "simple_gui.h"
#include "paths.h"
#include "scripting.h"
#include "world_collision.h"

#include "fortify.h"



void SPAWNPOINTS_add_spawn_point_param (int tilemap_number, int spawn_point_number, char *variable, char *list, char *entry)
{
	// This mallocs the room needed to add the new paramater and then adds it.

	int parameter_count;

	parameter_count = cm[tilemap_number].spawn_point_list_pointer[spawn_point_number].params;

	if ( (cm[tilemap_number].spawn_point_list_pointer[spawn_point_number].param_list_pointer == NULL) && (parameter_count = 0) )
	{
		cm[tilemap_number].spawn_point_list_pointer[spawn_point_number].param_list_pointer = (parameter *) malloc (sizeof(parameter));
	
		if (cm[tilemap_number].spawn_point_list_pointer[spawn_point_number].param_list_pointer == NULL)
		{
			assert(0);
		}
	}
	else
	{
		cm[tilemap_number].spawn_point_list_pointer[spawn_point_number].param_list_pointer = (parameter *) realloc ( cm[tilemap_number].spawn_point_list_pointer[spawn_point_number].param_list_pointer , (parameter_count+1) * sizeof(parameter) );
		
		if (cm[tilemap_number].spawn_point_list_pointer[spawn_point_number].param_list_pointer == NULL)
		{
			assert(0);
		}
	}

	strcpy ( cm[tilemap_number].spawn_point_list_pointer[spawn_point_number].param_list_pointer[parameter_count].param_dest_var , variable );
	strcpy ( cm[tilemap_number].spawn_point_list_pointer[spawn_point_number].param_list_pointer[parameter_count].param_list_type , list );
	strcpy ( cm[tilemap_number].spawn_point_list_pointer[spawn_point_number].param_list_pointer[parameter_count].param_name , entry );
	
	cm[tilemap_number].spawn_point_list_pointer[spawn_point_number].params++;
}



int SPAWNPOINTS_get_free_spawn_point_uid (int tilemap_number)
{
/*
	int spawn_point_number;
	int test_uid = 0;
	bool found_one;

	do
	{
		found_one = false;

		for (spawn_point_number=0; spawn_point_number<cm[tilemap_number].spawn_points; spawn_point_number++)
		{
			if (cm[tilemap_number].spawn_point_list_pointer[spawn_point_number].uid == test_uid)
			{
				found_one = true;
				test_uid++;
			}
		}
	} while (found_one == true);
*/
	cm[tilemap_number].spawn_point_next_uid++;

	return (cm[tilemap_number].spawn_point_next_uid - 1);
}



void SPAWNPOINTS_get_free_spawn_point_name (int tilemap_number , char *name, size_t name_size, char *name_archetype)
{
	int i;
	int test_num;
	bool found_one;

	char name_prefix[NAME_SIZE];

	strcpy (name_prefix,name_archetype);

	while ( STRING_is_char_numerical ( name_prefix[strlen(name_prefix)-1] ) == true )
	{
		name_prefix[strlen(name_prefix)-1] = '\0';
	}

	test_num = 0;

	do
	{
		found_one = false;

		for (i=0; i<cm[tilemap_number].spawn_points; i++)
		{
			snprintf(name, name_size, "%s%3d", name_prefix, test_num );
			STRING_replace_char (name, ' ' , '0');
			if (strcmp ( name , cm[tilemap_number].spawn_point_list_pointer[i].name ) == 0)
			{
				found_one = true;
				test_num++;
			}
		}
	} while (found_one == true);

	snprintf(name, name_size, "%s%3d", name_prefix, test_num );
	STRING_replace_char (name, ' ' , '0');
}



void SPAWNPOINTS_create_spawn_point (int tilemap_number , int x, int y , char *name, int tile_size)
{
	// This creates a spawn_point at the end of the queue.

	// First make sure the x and y fit inside the map, though.

	if (x<0)
	{
		x = 0;
	}
	else if (x > (cm[tilemap_number].map_width*tile_size) )
	{
		x = (cm[tilemap_number].map_width*tile_size);
	}
	
	if (y<0)
	{
		y=0;
	}
	else if (y > (cm[tilemap_number].map_height*tile_size) )
	{
		y = (cm[tilemap_number].map_height*tile_size);
	}

	// Then copy the supplied name somewhere as when we realloc the next chunk it might not
	// be where it was...

	char name_archetype[NAME_SIZE];

	strcpy (name_archetype,name);

	int total;
	char default_name[NAME_SIZE];

	total = cm[tilemap_number].spawn_points;

	if (cm[tilemap_number].spawn_point_list_pointer == NULL)
	{
		// None created yet.

		cm[tilemap_number].spawn_point_list_pointer = (spawn_point *) malloc ( sizeof (spawn_point) );
	}
	else
	{
		cm[tilemap_number].spawn_point_list_pointer = (spawn_point *) realloc ( cm[tilemap_number].spawn_point_list_pointer , (total+1) * sizeof (spawn_point) );
	}

	cm[tilemap_number].spawn_point_list_pointer[total].prev_sibling_index = UNSET;
	cm[tilemap_number].spawn_point_list_pointer[total].child_index = UNSET;

	cm[tilemap_number].spawn_point_list_pointer[total].parent_uid = UNSET;
	cm[tilemap_number].spawn_point_list_pointer[total].parent_index = UNSET;

	cm[tilemap_number].spawn_point_list_pointer[total].next_sibling_uid = UNSET;
	cm[tilemap_number].spawn_point_list_pointer[total].next_sibling_index = UNSET;

	cm[tilemap_number].spawn_point_list_pointer[total].param_list_pointer = NULL;
	cm[tilemap_number].spawn_point_list_pointer[total].params = 0;

	strcpy( cm[tilemap_number].spawn_point_list_pointer[total].script_name , "UNSET" );
	cm[tilemap_number].spawn_point_list_pointer[total].script_index = UNSET;

	strcpy( cm[tilemap_number].spawn_point_list_pointer[total].type , "UNSET" );
	cm[tilemap_number].spawn_point_list_pointer[total].type_value = UNSET;

	cm[tilemap_number].spawn_point_list_pointer[total].uid = SPAWNPOINTS_get_free_spawn_point_uid (tilemap_number);

	cm[tilemap_number].spawn_point_list_pointer[total].x = x;
	cm[tilemap_number].spawn_point_list_pointer[total].y = y;

	cm[tilemap_number].spawn_point_list_pointer[total].flag = 0;

	cm[tilemap_number].spawn_point_list_pointer[total].grabbed_status = NODE_UNSELECTED;

	cm[tilemap_number].spawn_point_list_pointer[total].sleeping = false;
	cm[tilemap_number].spawn_point_list_pointer[total].last_fired = UNSET;
	cm[tilemap_number].spawn_point_list_pointer[total].created_entity_index = UNSET;

	// GET NEW NAME HERE! REMEMBER TO INCLUDE MAP NAME IN NAME!

	SPAWNPOINTS_get_free_spawn_point_name (tilemap_number , default_name, NAME_SIZE, name_archetype);

	strcpy ( cm[tilemap_number].spawn_point_list_pointer[total].name , default_name );

	cm[tilemap_number].spawn_points++;
}



#define COPIED_SPAWN_POINT_NON_RELATIVE			(0)
#define COPIED_SPAWN_POINT_PREV_SIBLING			(1)
#define COPIED_SPAWN_POINT_NEXT_SIBLING			(2)
#define COPIED_SPAWN_POINT_CHILD				(3)
#define COPIED_SPAWN_POINT_PARENT				(4)

int SPAWNPOINTS_copy_spawn_point (int tilemap_number, int spawn_point_number, int relative, int tile_size)
{
	// This creates a new spawn point then copies everything over from the current
	// one apart from it's relatives unless specified. If it's a relative it also
	// offsets the position of the spawn point to make this very obvious.

	int x,y;
	int p;

	x = cm[tilemap_number].spawn_point_list_pointer[spawn_point_number].x;
	y = cm[tilemap_number].spawn_point_list_pointer[spawn_point_number].y;

	int new_spawn_point_number;

	switch (relative)
	{
	case COPIED_SPAWN_POINT_NON_RELATIVE:
		// Do nothing!
		break;

	case COPIED_SPAWN_POINT_PREV_SIBLING:
		x-=4;
		break;

	case COPIED_SPAWN_POINT_NEXT_SIBLING:
		x+=4;
		break;

	case COPIED_SPAWN_POINT_CHILD:
		y+=4;
		break;

	case COPIED_SPAWN_POINT_PARENT:
		y-=4;
		break;

	default:
		assert(0);
		break;
	}

	SPAWNPOINTS_create_spawn_point (tilemap_number , x, y , cm[tilemap_number].spawn_point_list_pointer[spawn_point_number].name, tile_size);

	new_spawn_point_number = cm[tilemap_number].spawn_points-1;
	
	for (p=0; p<cm[tilemap_number].spawn_point_list_pointer[spawn_point_number].params; p++)
	{
		SPAWNPOINTS_add_spawn_point_param ( tilemap_number , new_spawn_point_number , cm[tilemap_number].spawn_point_list_pointer[spawn_point_number].param_list_pointer[p].param_dest_var , cm[tilemap_number].spawn_point_list_pointer[spawn_point_number].param_list_pointer[p].param_list_type , cm[tilemap_number].spawn_point_list_pointer[spawn_point_number].param_list_pointer[p].param_name );
	}

	strcpy ( cm[tilemap_number].spawn_point_list_pointer[new_spawn_point_number].type , cm[tilemap_number].spawn_point_list_pointer[spawn_point_number].type );
	strcpy ( cm[tilemap_number].spawn_point_list_pointer[new_spawn_point_number].script_name , cm[tilemap_number].spawn_point_list_pointer[spawn_point_number].script_name );

	switch (relative)
	{
	case COPIED_SPAWN_POINT_NON_RELATIVE:
		// Do nothing!
		break;

	case COPIED_SPAWN_POINT_PREV_SIBLING:
		cm[tilemap_number].spawn_point_list_pointer[new_spawn_point_number].next_sibling_uid = cm[tilemap_number].spawn_point_list_pointer[spawn_point_number].uid;
		break;

	case COPIED_SPAWN_POINT_NEXT_SIBLING:
		cm[tilemap_number].spawn_point_list_pointer[spawn_point_number].next_sibling_uid = cm[tilemap_number].spawn_point_list_pointer[new_spawn_point_number].uid;
		break;

	case COPIED_SPAWN_POINT_CHILD:
		cm[tilemap_number].spawn_point_list_pointer[new_spawn_point_number].parent_uid = cm[tilemap_number].spawn_point_list_pointer[spawn_point_number].uid;
		break;

	case COPIED_SPAWN_POINT_PARENT:
		cm[tilemap_number].spawn_point_list_pointer[spawn_point_number].parent_uid = cm[tilemap_number].spawn_point_list_pointer[new_spawn_point_number].uid;
		break;

	default:
		assert(0);
		break;
	}

	return new_spawn_point_number;
}



void SPAWNPOINTS_replace_uid (int tilemap_number, int old_uid, int new_uid)
{
	// Once we've found the spare UID and the one that'll fill it, we need
	// to change it over. This function will search through all of the spawnpoints
	// and check the next_sibling, parent and actual uid for the old one, swapping
	// it with the new one. WOOT!

	int spawn_point_number;

	for (spawn_point_number=0; spawn_point_number<cm[tilemap_number].spawn_points; spawn_point_number++)
	{
		if (cm[tilemap_number].spawn_point_list_pointer[spawn_point_number].uid == old_uid)
		{
			cm[tilemap_number].spawn_point_list_pointer[spawn_point_number].uid = new_uid;
		}

		if (cm[tilemap_number].spawn_point_list_pointer[spawn_point_number].next_sibling_uid == old_uid)
		{
			cm[tilemap_number].spawn_point_list_pointer[spawn_point_number].next_sibling_uid = new_uid;
		}

		if (cm[tilemap_number].spawn_point_list_pointer[spawn_point_number].parent_uid == old_uid)
		{
			cm[tilemap_number].spawn_point_list_pointer[spawn_point_number].parent_uid = new_uid;
		}
	}
}



int SPAWNPOINTS_find_next_unused_uid (int tilemap_number, int uid)
{
	// This looks for the first unused UID in the spawn points. First time it's
	// called a value of -1 should be parsed in as it actually looks for the
	// parsed value+1, and 0 is a valid UID.

	bool found_one;
	int spawn_point_number;

	do
	{
		found_one = false;

		for (spawn_point_number=0; spawn_point_number<cm[tilemap_number].spawn_points; spawn_point_number++)
		{
			if (cm[tilemap_number].spawn_point_list_pointer[spawn_point_number].uid == uid+1)
			{
				uid++;
				found_one = true;
			}
		}
	}
	while (found_one);

	return uid;
}



int SPAWNPOINTS_find_next_used_uid (int tilemap_number, int uid)
{
	// This just looks for the lowest number in the UIDs which is still
	// greater than the parsed value, then returns it. If there is no
	// higher number, it returns UNSET, which indicates that we're done
	// to the calling function.

	int next_lowest = UNSET;

	int spawn_point_number;

	for (spawn_point_number=0; spawn_point_number<cm[tilemap_number].spawn_points; spawn_point_number++)
	{
		if (cm[tilemap_number].spawn_point_list_pointer[spawn_point_number].uid > uid)
		{
			if (next_lowest == UNSET)
			{
				// If we ain't set next_lowest yet, accept any number higher than uid.
				next_lowest = cm[tilemap_number].spawn_point_list_pointer[spawn_point_number].uid;
			}
			else if (next_lowest > cm[tilemap_number].spawn_point_list_pointer[spawn_point_number].uid)
			{
				// However if we have got a potential value, only accept it if the new one is lower.
				next_lowest = cm[tilemap_number].spawn_point_list_pointer[spawn_point_number].uid;
			}
		}
	}

	return next_lowest;
}



void SPAWNPOINTS_compress_spawn_point_uids (int tilemap_number)
{
	// This goes through all the spawnpoints and assigns each of them a new UID so that
	// we don't have any gaps in the UIDs. Basically it looks for the first unused potential
	// UID and then finds the next used UID after that and changes it down to that unused one.
	// Then we just relink as usual. Wooters!


}



void SPAWNPOINTS_position_selected_spawn_points (int tilemap_number, int offset_x, int offset_y, int grabbed_spawn_point, int grid_size, bool round_to_tilesize)
{
	// Updates all the selected nodes so they are positioned offset_x and offset_y from where
	// they started.

	// Note that although the co-ordinates are rounded to the grid, they are only rounded by the
	// amount that the grabbed node is out of whack.

	int out_of_whack_x;
	int out_of_whack_y;

	out_of_whack_x = (cm[tilemap_number].spawn_point_list_pointer[grabbed_spawn_point].grabbed_x + offset_x) % grid_size;
	out_of_whack_y = (cm[tilemap_number].spawn_point_list_pointer[grabbed_spawn_point].grabbed_y + offset_y) % grid_size;

	int spawn_point_number;

	for (spawn_point_number=0; spawn_point_number<cm[tilemap_number].spawn_points; spawn_point_number++)
	{
		if (cm[tilemap_number].spawn_point_list_pointer[spawn_point_number].grabbed_status == NODE_SELECTED)
		{
			cm[tilemap_number].spawn_point_list_pointer[spawn_point_number].x = cm[tilemap_number].spawn_point_list_pointer[spawn_point_number].grabbed_x + offset_x;
			cm[tilemap_number].spawn_point_list_pointer[spawn_point_number].y = cm[tilemap_number].spawn_point_list_pointer[spawn_point_number].grabbed_y + offset_y;

			if (round_to_tilesize == true)
			{
				cm[tilemap_number].spawn_point_list_pointer[spawn_point_number].x -= out_of_whack_x;
				cm[tilemap_number].spawn_point_list_pointer[spawn_point_number].y -= out_of_whack_y;
			}
		}
	}
}



void SPAWNPOINTS_destroy_spawn_point (int tilemap_number , int spawn_point_number)
{
	free (cm[tilemap_number].spawn_point_list_pointer[spawn_point_number].param_list_pointer);
}




void SPAWNPOINTS_swap_spawn_points (int tilemap_number , int spawn_point_1 , int spawn_point_2)
{
	spawn_point temp;

	temp = cm[tilemap_number].spawn_point_list_pointer[spawn_point_1];
	cm[tilemap_number].spawn_point_list_pointer[spawn_point_1] = cm[tilemap_number].spawn_point_list_pointer[spawn_point_2];
	cm[tilemap_number].spawn_point_list_pointer[spawn_point_2] = temp;
}



void SPAWNPOINTS_delete_particular_spawn_point (int tilemap_number , int spawn_point_number)
{
	// This deletes the given spawn point by first bubbling it to the end of the queue and then
	// destroying it.

	int total;
	int swap_counter;
	
	total = cm[tilemap_number].spawn_points;

	// Okay, to bubble the given one up to the surface we need to swap all the elements
	// from spawn_point_number to total-2 with the one after them.

	for (swap_counter = spawn_point_number ; swap_counter<total-1 ; swap_counter++)
	{
		SPAWNPOINTS_swap_spawn_points (tilemap_number , swap_counter , swap_counter+1 );
	}

	// Then we destroy the memory used by the last one in the list...

	SPAWNPOINTS_destroy_spawn_point (tilemap_number , total-1);

	// Then we realloc the memory used by the spawn point list to remove the freed up area.

	cm[tilemap_number].spawn_point_list_pointer = (spawn_point *) realloc ( cm[tilemap_number].spawn_point_list_pointer , (total-1) * sizeof (spawn_point) );

	cm[tilemap_number].spawn_points--;
}




void SPAWNPOINTS_partially_set_spawn_point_status (int tilemap_number, int x1, int y1, int x2, int y2, float zoom_level, int flagging_type, int drag_time)
{
	// First of all resets all PRESUMPTIVE nodes to their base states and then
	// reset the PRESUMPTIVES. This is dependant on the flagging_type, of course.

	// Note that if the time that the mouse button was held down for was less than 
	// "NO_DRAG_THRESHOLD" frames then only the first encountered node will be
	// flagged as it's the equivalent of a single-click until then.

	int spawn_point_number;

	int temp;

	if (x1>x2)
	{
		temp = x1;
		x1 = x2;
		x2 = temp;
	}
	
	if (y1>y2)
	{
		temp = y1;
		y1 = y2;
		y2 = temp;
	}

	for (spawn_point_number=0; spawn_point_number<cm[tilemap_number].spawn_points; spawn_point_number++)
	{
		if (flagging_type == SELECTING_NODES)
		{
			// Reset 'em regardless!
			cm[tilemap_number].spawn_point_list_pointer[spawn_point_number].grabbed_status = NODE_UNSELECTED;
		}
		else
		{
			if (cm[tilemap_number].spawn_point_list_pointer[spawn_point_number].grabbed_status == NODE_PRESUMPTIVE_UNSELECTED)
			{
				cm[tilemap_number].spawn_point_list_pointer[spawn_point_number].grabbed_status = NODE_SELECTED;
			}
			else if (cm[tilemap_number].spawn_point_list_pointer[spawn_point_number].grabbed_status == NODE_PRESUMPTIVE_SELECTED)
			{
				cm[tilemap_number].spawn_point_list_pointer[spawn_point_number].grabbed_status = NODE_UNSELECTED;
			}
		}
	}

	int node_x,node_y;

	for (spawn_point_number=0; spawn_point_number<cm[tilemap_number].spawn_points; spawn_point_number++)
	{
		node_x = cm[tilemap_number].spawn_point_list_pointer[spawn_point_number].x;
		node_y = cm[tilemap_number].spawn_point_list_pointer[spawn_point_number].y;

		if (MATH_rectangle_to_rectangle_intersect ( node_x-int(float(SPAWN_POINT_HANDLE_RADIUS)/zoom_level) , node_y-int(float(SPAWN_POINT_HANDLE_RADIUS)/zoom_level) , node_x+int(float(SPAWN_POINT_HANDLE_RADIUS)/zoom_level) , node_y+int(float(SPAWN_POINT_HANDLE_RADIUS)/zoom_level) , x1, y1, x2, y2 ) == true)
		{
			if ( (flagging_type == SELECTING_NODES) || (flagging_type == ADDING_NODES) )
			{
				if (cm[tilemap_number].spawn_point_list_pointer[spawn_point_number].grabbed_status == NODE_UNSELECTED)
				{
					cm[tilemap_number].spawn_point_list_pointer[spawn_point_number].grabbed_status = NODE_PRESUMPTIVE_SELECTED;
					
					if (drag_time < NO_DRAG_THRESHOLD)
					{
						// We're only altering the one item until we've held down the button a while.
						return;
					}
				}
			}
			else if (flagging_type == REMOVING_NODES)
			{
				if (cm[tilemap_number].spawn_point_list_pointer[spawn_point_number].grabbed_status == NODE_SELECTED)
				{
					cm[tilemap_number].spawn_point_list_pointer[spawn_point_number].grabbed_status = NODE_PRESUMPTIVE_UNSELECTED;
					
					if (drag_time < NO_DRAG_THRESHOLD)
					{
						// We're only altering the one item until we've held down the button a while.
						return;
					}
				}
			}
		}
	}

}



void SPAWNPOINTS_select_individual_spawn_point (int tilemap_number, int spawn_point_number)
{
	cm[tilemap_number].spawn_point_list_pointer[spawn_point_number].grabbed_status = NODE_SELECTED;
}



void SPAWNPOINTS_deselect_spawn_points (int tilemap_number)
{
	// Flags the presumptives as actuals. If there's only one selected then it's ID is returned, otherwise
	// a negative number is returned.

	int spawn_point_number;

	for (spawn_point_number=0; spawn_point_number<cm[tilemap_number].spawn_points; spawn_point_number++)
	{
		cm[tilemap_number].spawn_point_list_pointer[spawn_point_number].grabbed_status = NODE_UNSELECTED;
	}
}



bool SPAWNPOINTS_check_for_duplicate_name (int tilemap_number, int spawn_point_number, char *new_entry_name)
{
	int s;

	for (s=0; s<cm[tilemap_number].spawn_points; s++)
	{
		if (s!=spawn_point_number) // Not checking against itself.
		{
			if (strcmp (cm[tilemap_number].spawn_point_list_pointer[s].name , new_entry_name) == 0)
			{
				return true;
			}
		}
	}

	return false;
}



void SPAWNPOINTS_destroy_spawn_point_params (int tilemap_number, int spawn_point_number)
{
	if (cm[tilemap_number].spawn_point_list_pointer[spawn_point_number].param_list_pointer != NULL)
	{
		free (cm[tilemap_number].spawn_point_list_pointer[spawn_point_number].param_list_pointer);
		cm[tilemap_number].spawn_point_list_pointer[spawn_point_number].param_list_pointer = NULL;
	}

	cm[tilemap_number].spawn_point_list_pointer[spawn_point_number].params = 0;
}



void SPAWNPOINTS_reset_visual_params (int tilemap_number, int spawn_point_number, int *bitmap_number, int *base_frame, int *frame_count, int *frame_delay, int *frame_end_delay, int *path_number, int *path_angle, int *path_base_speed)
{
	*bitmap_number = UNSET;
	*base_frame = UNSET;
	*frame_count = 1;
	*frame_delay = 10;
	*frame_end_delay = 0;
	*path_number = UNSET;
	*path_angle = 0;
	*path_base_speed = 1;
}



bool SPAWNPOINTS_check_for_visual_params (int tilemap_number, int spawn_point_number, int *bitmap_number, int *base_frame, int *frame_count, int *frame_delay, int *frame_end_delay, int *path_number, int *path_angle, int *path_base_speed)
{
	// Looks through the params you're passing to a script to see if there's anything which could
	// be used to show the spawn point as something other than a boring circle. Very helpful for placing
	// enemies.

	bool found_bitmap = false;
	bool found_base_frame = false;

	int p;
	int parameter_count = cm[tilemap_number].spawn_point_list_pointer[spawn_point_number].params;

	for (p=0; p<parameter_count; p++)
	{
		if (strcmp ( cm[tilemap_number].spawn_point_list_pointer[spawn_point_number].param_list_pointer[p].param_dest_var , "SPRITE" ) == 0) // Variable name
		{
			if (strcmp ( cm[tilemap_number].spawn_point_list_pointer[spawn_point_number].param_list_pointer[p].param_list_type , "SPRITES" ) == 0) // List name
			{
				*bitmap_number = cm[tilemap_number].spawn_point_list_pointer[spawn_point_number].param_list_pointer[p].param_value;
				found_bitmap = true;
			}
		}

		if (strcmp ( cm[tilemap_number].spawn_point_list_pointer[spawn_point_number].param_list_pointer[p].param_dest_var , "BASE_FRAME" ) == 0) // Variable name
		{
			*base_frame = cm[tilemap_number].spawn_point_list_pointer[spawn_point_number].param_list_pointer[p].param_value;
			if (*base_frame != UNSET)
			{
				found_base_frame = true;
			}
		}

		if (strcmp ( cm[tilemap_number].spawn_point_list_pointer[spawn_point_number].param_list_pointer[p].param_dest_var , "FRAME_COUNT" ) == 0) // Variable name
		{
			*frame_count = cm[tilemap_number].spawn_point_list_pointer[spawn_point_number].param_list_pointer[p].param_value;
		}

		if (strcmp ( cm[tilemap_number].spawn_point_list_pointer[spawn_point_number].param_list_pointer[p].param_dest_var , "FRAME_DELAY" ) == 0) // Variable name
		{
			*frame_delay = cm[tilemap_number].spawn_point_list_pointer[spawn_point_number].param_list_pointer[p].param_value;
		}

		if (strcmp ( cm[tilemap_number].spawn_point_list_pointer[spawn_point_number].param_list_pointer[p].param_dest_var , "FRAME_END_DELAY" ) == 0) // Variable name
		{
			*frame_end_delay = cm[tilemap_number].spawn_point_list_pointer[spawn_point_number].param_list_pointer[p].param_value;
		}

		if (strcmp ( cm[tilemap_number].spawn_point_list_pointer[spawn_point_number].param_list_pointer[p].param_dest_var , "PATH_NUMBER" ) == 0) // Variable name
		{
			*path_number = cm[tilemap_number].spawn_point_list_pointer[spawn_point_number].param_list_pointer[p].param_value;
		}

		if (strcmp ( cm[tilemap_number].spawn_point_list_pointer[spawn_point_number].param_list_pointer[p].param_dest_var , "PATH_ANGLE" ) == 0) // Variable name
		{
			*path_angle = cm[tilemap_number].spawn_point_list_pointer[spawn_point_number].param_list_pointer[p].param_value;
		}

		if (strcmp ( cm[tilemap_number].spawn_point_list_pointer[spawn_point_number].param_list_pointer[p].param_dest_var , "PATH_BASE_SPEED" ) == 0) // Variable name
		{
			*path_base_speed = cm[tilemap_number].spawn_point_list_pointer[spawn_point_number].param_list_pointer[p].param_value;
		}
	}

	if ( (found_bitmap == true) && (found_base_frame == true) )
	{
		return true;
	}
	else
	{
		return false;
	}
	
}



int SPAWNPOINTS_find_nearest_spawn_point (int tilemap_number, int x, int y, int ignore_me)
{
	int spawn_point_number;
	int real_spawn_point_x;
	int real_spawn_point_y;

	int closest_dist = UNSET;
	int closest_point = UNSET;
	int dist;

	for (spawn_point_number=0; spawn_point_number<cm[tilemap_number].spawn_points; spawn_point_number++)
	{
		real_spawn_point_x = cm[tilemap_number].spawn_point_list_pointer[spawn_point_number].x;
		real_spawn_point_y = cm[tilemap_number].spawn_point_list_pointer[spawn_point_number].y;
		
		dist = MATH_get_distance_int (x,y,real_spawn_point_x,real_spawn_point_y);

		if ( ( (dist < closest_dist) || (closest_dist==UNSET) ) && (spawn_point_number != ignore_me))
		{
			closest_dist = dist;
			closest_point = spawn_point_number;
		}
	}

	return closest_point;
}



void SPAWNPOINTS_draw_spawn_points (int tilemap_number, int sx, int sy, int width, int height, float zoom_level, int selected_spawn_point, bool nearest, bool faded=false)
{
	int tileset_number;
	int tilesize;

	bool result;

	static int frame_counter = 0;

	int spawn_point_bitmap_number;
	int spawn_point_base_frame;
	int spawn_point_frame_count;
	int spawn_point_frame_delay;
	int spawn_point_frame_end_delay;
	int spawn_point_path_number;
	int spawn_point_path_angle;
	int spawn_point_path_base_speed;

	char text_line[TEXT_LINE_SIZE];

	tileset_number = cm[tilemap_number].tile_set_index;

	if (tileset_number != UNSET)
	{
		tilesize = TILESETS_get_tilesize(tileset_number);
	}
	else
	{
		tilesize = 16;
	}

	int spawn_point_number;
	int real_spawn_point_x;
	int real_spawn_point_y;
	int screen_spawn_point_x;
	int screen_spawn_point_y;

	int path_length;
	int path_traversal_time;
	int current_path_distance;

	float real_angle;

	int family_member;

	int red,green,blue;

	int frame_number;
	int temp_value;

	int family_real_spawn_point_x;
	int family_real_spawn_point_y;
	int family_screen_spawn_point_x;
	int family_screen_spawn_point_y;

	frame_counter++;

	for (spawn_point_number=0; spawn_point_number<cm[tilemap_number].spawn_points; spawn_point_number++)
	{
		real_spawn_point_x = cm[tilemap_number].spawn_point_list_pointer[spawn_point_number].x;
		real_spawn_point_y = cm[tilemap_number].spawn_point_list_pointer[spawn_point_number].y;

		screen_spawn_point_x = int (float(real_spawn_point_x - (sx * tilesize)) * zoom_level);
		screen_spawn_point_y = int (float(real_spawn_point_y - (sy * tilesize)) * zoom_level);

		if (MATH_rectangle_to_point_intersect_by_size (0,0,width,height,screen_spawn_point_x,screen_spawn_point_y) == true)
		{
			SPAWNPOINTS_reset_visual_params (tilemap_number, spawn_point_number, &spawn_point_bitmap_number, &spawn_point_base_frame, &spawn_point_frame_count, &spawn_point_frame_delay, &spawn_point_frame_end_delay, &spawn_point_path_number, &spawn_point_path_angle, &spawn_point_path_base_speed);

			result = false;

			if (cm[tilemap_number].spawn_point_list_pointer[spawn_point_number].script_index != UNSET)
			{
				result = SCRIPTING_check_for_visual_params (tilemap_number, spawn_point_number, &spawn_point_bitmap_number, &spawn_point_base_frame, &spawn_point_frame_count, &spawn_point_frame_delay, &spawn_point_frame_end_delay, &spawn_point_path_number, &spawn_point_path_angle);
			}

			if (cm[tilemap_number].spawn_point_list_pointer[spawn_point_number].params > 0)
			{
				if (!result)
				{
					result = SPAWNPOINTS_check_for_visual_params (tilemap_number, spawn_point_number, &spawn_point_bitmap_number, &spawn_point_base_frame, &spawn_point_frame_count, &spawn_point_frame_delay, &spawn_point_frame_end_delay, &spawn_point_path_number, &spawn_point_path_angle, &spawn_point_path_base_speed);
				}
				else
				{
					SPAWNPOINTS_check_for_visual_params (tilemap_number, spawn_point_number, &spawn_point_bitmap_number, &spawn_point_base_frame, &spawn_point_frame_count, &spawn_point_frame_delay, &spawn_point_frame_end_delay, &spawn_point_path_number, &spawn_point_path_angle, &spawn_point_path_base_speed);
				}
			}

			if (spawn_point_frame_delay == 0)
			{
				// No it feckin' isn't you shit!
				spawn_point_frame_delay = 1;
			}

			if (spawn_point_frame_count == 0)
			{
				// No it feckin' isn't you shit!
				spawn_point_frame_count = 1;
			}

			// Draw handle!

			if (result == true)
			{
				// Ooh, we have enough information to draw a graphic as well as the circle!

				temp_value = OUTPUT_bitmap_frames (spawn_point_bitmap_number);

				if (spawn_point_base_frame + spawn_point_frame_count > temp_value)
				{
					// If we've put in too many frames then we'll curtail it.
					spawn_point_frame_count = temp_value - spawn_point_base_frame;					
				}

				temp_value = (spawn_point_frame_delay * spawn_point_frame_count) + spawn_point_frame_end_delay;
				
				if ( (frame_counter % temp_value) < (spawn_point_frame_delay * spawn_point_frame_count) )
				{
					frame_number = spawn_point_base_frame + ((frame_counter % temp_value) / spawn_point_frame_delay);
				}
				else
				{
					frame_number = spawn_point_base_frame + spawn_point_frame_count - 1;
				}
	
				if (faded)
				{
					OUTPUT_draw_sprite_scale (spawn_point_bitmap_number, frame_number , screen_spawn_point_x , screen_spawn_point_y , zoom_level ,128,128,128);
					//OUTPUT_draw_masked_sprite (spawn_point_bitmap_number , frame_number, screen_spawn_point_x, screen_spawn_point_y,128,128,128);
				}
				else
				{
					OUTPUT_draw_sprite_scale (spawn_point_bitmap_number, frame_number , screen_spawn_point_x , screen_spawn_point_y , zoom_level);
					//OUTPUT_draw_masked_sprite (spawn_point_bitmap_number , frame_number, screen_spawn_point_x, screen_spawn_point_y);
				}
			}

			if (!faded)
			{
				if (cm[tilemap_number].spawn_point_list_pointer[spawn_point_number].grabbed_status == NODE_SELECTED)
				{
					// It's the selected point
					green = 255;

					if (selected_spawn_point == spawn_point_number)
					{
						red = 255;
					}
					else
					{
						red = 0;
					}
					
					if (nearest == true)
					{
						blue = 0;
					}
					else
					{
						blue = 255;

						// Horizontal arrow
						OUTPUT_hline (screen_spawn_point_x+SPAWN_POINT_HANDLE_RADIUS , screen_spawn_point_y , screen_spawn_point_x+SPAWN_POINT_HANDLE_RADIUS+(SPAWN_POINT_HANDLE_RADIUS*2) , red , green , blue );
						OUTPUT_line (screen_spawn_point_x+SPAWN_POINT_HANDLE_RADIUS+(SPAWN_POINT_HANDLE_RADIUS*2) , screen_spawn_point_y , screen_spawn_point_x+SPAWN_POINT_HANDLE_RADIUS+(SPAWN_POINT_HANDLE_RADIUS*2) - (SPAWN_POINT_HANDLE_RADIUS/2) , screen_spawn_point_y - (SPAWN_POINT_HANDLE_RADIUS/2) , red , green , blue );
						OUTPUT_line (screen_spawn_point_x+SPAWN_POINT_HANDLE_RADIUS+(SPAWN_POINT_HANDLE_RADIUS*2) , screen_spawn_point_y , screen_spawn_point_x+SPAWN_POINT_HANDLE_RADIUS+(SPAWN_POINT_HANDLE_RADIUS*2) - (SPAWN_POINT_HANDLE_RADIUS/2) , screen_spawn_point_y + (SPAWN_POINT_HANDLE_RADIUS/2) , red , green , blue );

						// Vertical arrow
						OUTPUT_vline (screen_spawn_point_x , screen_spawn_point_y+SPAWN_POINT_HANDLE_RADIUS , screen_spawn_point_y+SPAWN_POINT_HANDLE_RADIUS+(SPAWN_POINT_HANDLE_RADIUS*2) , red , green , blue );
						OUTPUT_line (screen_spawn_point_x , screen_spawn_point_y+SPAWN_POINT_HANDLE_RADIUS+(SPAWN_POINT_HANDLE_RADIUS*2) , screen_spawn_point_x - (SPAWN_POINT_HANDLE_RADIUS/2) , screen_spawn_point_y + SPAWN_POINT_HANDLE_RADIUS+(SPAWN_POINT_HANDLE_RADIUS*2) - (SPAWN_POINT_HANDLE_RADIUS/2) , red , green , blue );
						OUTPUT_line (screen_spawn_point_x , screen_spawn_point_y+SPAWN_POINT_HANDLE_RADIUS+(SPAWN_POINT_HANDLE_RADIUS*2) , screen_spawn_point_x + (SPAWN_POINT_HANDLE_RADIUS/2) , screen_spawn_point_y + SPAWN_POINT_HANDLE_RADIUS+(SPAWN_POINT_HANDLE_RADIUS*2) - (SPAWN_POINT_HANDLE_RADIUS/2) , red , green , blue );

						// And then display the position in tile_x, tile_y and offset.
						snprintf (text_line, sizeof(text_line), "X=%i", cm[tilemap_number].spawn_point_list_pointer[spawn_point_number].x );
						OUTPUT_text (screen_spawn_point_x-(16+SPAWN_POINT_HANDLE_RADIUS), screen_spawn_point_y-(16+SPAWN_POINT_HANDLE_RADIUS) , text_line );
						snprintf (text_line, sizeof(text_line), "Y=%i", cm[tilemap_number].spawn_point_list_pointer[spawn_point_number].y );
						OUTPUT_text (screen_spawn_point_x-(16+SPAWN_POINT_HANDLE_RADIUS), screen_spawn_point_y-(8+SPAWN_POINT_HANDLE_RADIUS) , text_line );
					}
				}
				else
				{
					red = blue = 255;
					green = 0;
				}

				// Draw relatives

				if (cm[tilemap_number].spawn_point_list_pointer[spawn_point_number].prev_sibling_index != UNSET)
				{
					family_member = cm[tilemap_number].spawn_point_list_pointer[spawn_point_number].prev_sibling_index;

					family_real_spawn_point_x = cm[tilemap_number].spawn_point_list_pointer[family_member].x;
					family_real_spawn_point_y = cm[tilemap_number].spawn_point_list_pointer[family_member].y;
					family_screen_spawn_point_x = int (float(family_real_spawn_point_x - (sx * tilesize)) * zoom_level);
					family_screen_spawn_point_y = int (float(family_real_spawn_point_y - (sy * tilesize)) * zoom_level);

					OUTPUT_draw_arrow (screen_spawn_point_x,screen_spawn_point_y,family_screen_spawn_point_x,family_screen_spawn_point_y, 128, 0, 0, true);
				}

				if (cm[tilemap_number].spawn_point_list_pointer[spawn_point_number].next_sibling_index != UNSET)
				{
					family_member = cm[tilemap_number].spawn_point_list_pointer[spawn_point_number].next_sibling_index;

					family_real_spawn_point_x = cm[tilemap_number].spawn_point_list_pointer[family_member].x;
					family_real_spawn_point_y = cm[tilemap_number].spawn_point_list_pointer[family_member].y;
					family_screen_spawn_point_x = int (float(family_real_spawn_point_x - (sx * tilesize)) * zoom_level);
					family_screen_spawn_point_y = int (float(family_real_spawn_point_y - (sy * tilesize)) * zoom_level);

					OUTPUT_draw_arrow (screen_spawn_point_x,screen_spawn_point_y,family_screen_spawn_point_x,family_screen_spawn_point_y, 255, 0, 0);
				}

				if (cm[tilemap_number].spawn_point_list_pointer[spawn_point_number].child_index != UNSET)
				{
					family_member = cm[tilemap_number].spawn_point_list_pointer[spawn_point_number].child_index;

					family_real_spawn_point_x = cm[tilemap_number].spawn_point_list_pointer[family_member].x;
					family_real_spawn_point_y = cm[tilemap_number].spawn_point_list_pointer[family_member].y;
					family_screen_spawn_point_x = int (float(family_real_spawn_point_x - (sx * tilesize)) * zoom_level);
					family_screen_spawn_point_y = int (float(family_real_spawn_point_y - (sy * tilesize)) * zoom_level);

					OUTPUT_draw_arrow (screen_spawn_point_x,screen_spawn_point_y,family_screen_spawn_point_x,family_screen_spawn_point_y, 128, 128, 0, true);
				}

				if (cm[tilemap_number].spawn_point_list_pointer[spawn_point_number].parent_index != UNSET)
				{
					family_member = cm[tilemap_number].spawn_point_list_pointer[spawn_point_number].parent_index;

					family_real_spawn_point_x = cm[tilemap_number].spawn_point_list_pointer[family_member].x;
					family_real_spawn_point_y = cm[tilemap_number].spawn_point_list_pointer[family_member].y;
					family_screen_spawn_point_x = int (float(family_real_spawn_point_x - (sx * tilesize)) * zoom_level);
					family_screen_spawn_point_y = int (float(family_real_spawn_point_y - (sy * tilesize)) * zoom_level);

					OUTPUT_draw_arrow (screen_spawn_point_x,screen_spawn_point_y,family_screen_spawn_point_x,family_screen_spawn_point_y, 255, 255, 0);
				}

				// We might even get to draw a path! Woot!

				if (spawn_point_path_number != UNSET)
				{
					path_length = PATHS_get_length (spawn_point_path_number);
					path_traversal_time = path_length/spawn_point_path_base_speed;
					current_path_distance = frame_counter % path_traversal_time;
					real_angle = float (spawn_point_path_angle) / float (100.0f * PI);
					PATHS_draw_path (spawn_point_path_number , real_angle, screen_spawn_point_x, screen_spawn_point_y, zoom_level, 255,255,0, false);
				}

				OUTPUT_circle(screen_spawn_point_x,screen_spawn_point_y,SPAWN_POINT_HANDLE_RADIUS,red,green,blue);
			}
		}
	}
}



bool SPAWNPOINTS_what_selected_spawn_point_thing_has_been_grabbed (int tilemap_number, int x, int y, float zoom_level, int *grabbed_handle, int *grabbed_node)
{
	int spawn_point_number;

	int node_x,node_y;

	int grabbed;

	grabbed = UNSET;
	
	for (spawn_point_number=0; spawn_point_number<cm[tilemap_number].spawn_points; spawn_point_number++)
	{
		// Go through all the nodes...

		if (cm[tilemap_number].spawn_point_list_pointer[spawn_point_number].grabbed_status == NODE_SELECTED)
		{
			// But only consider those which have been flagged as selected.

			grabbed = UNSET;

			node_x = cm[tilemap_number].spawn_point_list_pointer[spawn_point_number].x;
			node_y = cm[tilemap_number].spawn_point_list_pointer[spawn_point_number].y;

			if (MATH_rectangle_to_point_intersect ( node_x-int(float(SPAWN_POINT_HANDLE_RADIUS)/zoom_level) , node_y-int(float(SPAWN_POINT_HANDLE_RADIUS)/zoom_level) , node_x+int(float(SPAWN_POINT_HANDLE_RADIUS)/zoom_level) , node_y+int(float(SPAWN_POINT_HANDLE_RADIUS)/zoom_level) , x , y ) == true)
			{
				grabbed = GRABBED_PATH_NODE_NODE;
			}

			if (MATH_rectangle_to_point_intersect ( node_x+int(float(SPAWN_POINT_HANDLE_RADIUS)/zoom_level) , node_y-(int(float(SPAWN_POINT_HANDLE_RADIUS/2)/zoom_level)) , node_x+(int(float(SPAWN_POINT_HANDLE_RADIUS*3)/zoom_level)) , node_y+(int(float(SPAWN_POINT_HANDLE_RADIUS/2)/zoom_level)) , x , y ) == true)
			{
				grabbed = GRABBED_PATH_NODE_X_HANDLE;
			}

			if (MATH_rectangle_to_point_intersect ( node_x-int(float(SPAWN_POINT_HANDLE_RADIUS/2)/zoom_level) , node_y+int(float(SPAWN_POINT_HANDLE_RADIUS)/zoom_level) , node_x+int(float(SPAWN_POINT_HANDLE_RADIUS/2)/zoom_level) , node_y+int(float(SPAWN_POINT_HANDLE_RADIUS*3)/zoom_level) , x , y ) == true)
			{
				grabbed = GRABBED_PATH_NODE_Y_HANDLE;
			}

			if (grabbed != UNSET)
			{
				*grabbed_handle = grabbed;
				*grabbed_node = spawn_point_number;
				return true;
			}

		}

	}

	return false;
}




void SPAWNPOINTS_get_grabbed_spawn_point_co_ords (int tilemap_number)
{
	int spawn_point_number;

	for (spawn_point_number=0; spawn_point_number<cm[tilemap_number].spawn_points; spawn_point_number++)
	{
		if (cm[tilemap_number].spawn_point_list_pointer[spawn_point_number].grabbed_status == NODE_SELECTED)
		{
			cm[tilemap_number].spawn_point_list_pointer[spawn_point_number].grabbed_x = cm[tilemap_number].spawn_point_list_pointer[spawn_point_number].x;
			cm[tilemap_number].spawn_point_list_pointer[spawn_point_number].grabbed_y = cm[tilemap_number].spawn_point_list_pointer[spawn_point_number].y;
		}
	}
}




void SPAWNPOINTS_check_for_duplicate_relative_links (int tilemap_number, int spawn_point_number)
{
	// This function is called by the below one to check that you haven't just created an
	// infinite loop by telling a pair of entities that each other one is its parent. Basically
	// it checks that the parent's parent isn't the current spawn_point_number and the
	// next_sibling's next_sibling isn't the current one. If in either case it is then it
	// resets the parent's parent or next_sibling's next_sibling to UNSET.

	// It also makes sure that none of the entities other than the one it's next_sibling
	// points to think that this entity are their next_siblings.

	int parent = UNSET;
	int next_sibling = UNSET;
	int sp1;

	for (sp1=0; sp1<cm[tilemap_number].spawn_points; sp1++)
	{
		if (sp1 != spawn_point_number) // So you can't link it with itself.
		{
			if (cm[tilemap_number].spawn_point_list_pointer[spawn_point_number].parent_uid == cm[tilemap_number].spawn_point_list_pointer[sp1].uid)
			{
				parent = sp1;
			}

			if (cm[tilemap_number].spawn_point_list_pointer[spawn_point_number].next_sibling_uid == cm[tilemap_number].spawn_point_list_pointer[sp1].uid )
			{
				next_sibling = sp1;
			}

			if (cm[tilemap_number].spawn_point_list_pointer[spawn_point_number].next_sibling_uid == cm[tilemap_number].spawn_point_list_pointer[sp1].next_sibling_uid )
			{
				// Get rid of duplicate siblings.
				cm[tilemap_number].spawn_point_list_pointer[sp1].next_sibling_uid = UNSET;
			}
		}
	}

	if (parent != UNSET)
	{
		// Found a parent!
		if (cm[tilemap_number].spawn_point_list_pointer[parent].parent_index == spawn_point_number)
		{
			cm[tilemap_number].spawn_point_list_pointer[parent].parent_uid = UNSET;
		}
	}

	if (next_sibling != UNSET)
	{
		// Found a next_sibling!
		if (cm[tilemap_number].spawn_point_list_pointer[next_sibling].next_sibling_index == spawn_point_number)
		{
			cm[tilemap_number].spawn_point_list_pointer[next_sibling].next_sibling_uid = UNSET;
		}
	}

}




void SPAWNPOINTS_relink_all_spawn_point_relatives (int tilemap_number)
{
	// Destroys all the generated INDEX links and repopulates them using the far more
	// reliable UID data.

	int sp1,sp2;

	for (sp1=0; sp1<cm[tilemap_number].spawn_points; sp1++)
	{
		// Reset all current ties! NOW!

		cm[tilemap_number].spawn_point_list_pointer[sp1].parent_index = UNSET;
		cm[tilemap_number].spawn_point_list_pointer[sp1].child_index = UNSET;
		cm[tilemap_number].spawn_point_list_pointer[sp1].prev_sibling_index = UNSET;
		cm[tilemap_number].spawn_point_list_pointer[sp1].next_sibling_index = UNSET;
	}

	for (sp1=0; sp1<cm[tilemap_number].spawn_points; sp1++)
	{
		// And finally the parent and siblings...

		for (sp2=0; sp2<cm[tilemap_number].spawn_points; sp2++)
		{
			if (sp2 != sp1) // So you can't link it with itself.
			{
				if (cm[tilemap_number].spawn_point_list_pointer[sp1].parent_uid == cm[tilemap_number].spawn_point_list_pointer[sp2].uid )
				{
					cm[tilemap_number].spawn_point_list_pointer[sp1].parent_index = sp2;
					cm[tilemap_number].spawn_point_list_pointer[sp2].child_index = sp1;
				}

				if (cm[tilemap_number].spawn_point_list_pointer[sp1].next_sibling_uid == cm[tilemap_number].spawn_point_list_pointer[sp2].uid )
				{
					cm[tilemap_number].spawn_point_list_pointer[sp1].next_sibling_index = sp2;
					cm[tilemap_number].spawn_point_list_pointer[sp2].prev_sibling_index = sp1;
				}
			}
		}

		// If afterwards there are any broken links (ie, non UNSET names which weren't found), change their name to UNSET.

		if (cm[tilemap_number].spawn_point_list_pointer[sp1].parent_uid != UNSET)
		{
			// If there's a name given for the parent...
			if (cm[tilemap_number].spawn_point_list_pointer[sp1].parent_index == UNSET)
			{
				// But we couldn't find it, then that spawn point was obviously deleted.
				cm[tilemap_number].spawn_point_list_pointer[sp1].parent_uid = UNSET;
				// So reset that name.
			}
		}

		if (cm[tilemap_number].spawn_point_list_pointer[sp1].next_sibling_uid != UNSET)
		{
			// If there's a name given for the next sibling...
			if (cm[tilemap_number].spawn_point_list_pointer[sp1].next_sibling_index == UNSET)
			{
				// But we couldn't find it, then that spawn point was obviously deleted.
				cm[tilemap_number].spawn_point_list_pointer[sp1].next_sibling_uid = UNSET;
				// So reset that name.
			}
		}

	}

}



void SPAWNPOINTS_confirm_spawn_point_links (int tilemap_number, int spawn_point_number)
{
	// Gets the index of the scripts, etc, by using the text names.

	int p;
	int parameter_count;

	// Link the paramaters up first...

	parameter_count = cm[tilemap_number].spawn_point_list_pointer[spawn_point_number].params;

	for (p=0; p<parameter_count; p++)
	{
		cm[tilemap_number].spawn_point_list_pointer[spawn_point_number].param_list_pointer[p].param_dest_var_index = GPL_find_word_value ("VARIABLE", cm[tilemap_number].spawn_point_list_pointer[spawn_point_number].param_list_pointer[p].param_dest_var);
		cm[tilemap_number].spawn_point_list_pointer[spawn_point_number].param_list_pointer[p].param_value = GPL_find_word_value (cm[tilemap_number].spawn_point_list_pointer[spawn_point_number].param_list_pointer[p].param_list_type , cm[tilemap_number].spawn_point_list_pointer[spawn_point_number].param_list_pointer[p].param_name);
	}

	// Then the script and type...
	
	cm[tilemap_number].spawn_point_list_pointer[spawn_point_number].script_index = GPL_find_word_value ("SCRIPTS", cm[tilemap_number].spawn_point_list_pointer[spawn_point_number].script_name);
	cm[tilemap_number].spawn_point_list_pointer[spawn_point_number].type_value = GPL_find_word_value ("SPAWN_POINT_TYPES", cm[tilemap_number].spawn_point_list_pointer[spawn_point_number].type);
}



void SPAWNPOINTS_confirm_all_links (void)
{
	int tilemap_number;
	int spawn_point_number;

	for (tilemap_number=0; tilemap_number<number_of_tilemaps_loaded; tilemap_number++)
	{
		SPAWNPOINTS_relink_all_spawn_point_relatives (tilemap_number);

		for (spawn_point_number=0; spawn_point_number<cm[tilemap_number].spawn_points; spawn_point_number++)
		{
			SPAWNPOINTS_check_for_duplicate_relative_links (tilemap_number, spawn_point_number);
			SPAWNPOINTS_confirm_spawn_point_links (tilemap_number, spawn_point_number);
		}
	}
}



int SPAWNPOINTS_lock_spawn_point_status (int tilemap_number)
{
	// Flags the presumptives as actuals. If there's only one selected then it's ID is returned, otherwise
	// a negative number is returned.

	int spawn_point_number;
	int flagged_spawn_point_number;

	int total_flagged = 0;

	for (spawn_point_number=0; spawn_point_number<cm[tilemap_number].spawn_points; spawn_point_number++)
	{
		if (cm[tilemap_number].spawn_point_list_pointer[spawn_point_number].grabbed_status == NODE_PRESUMPTIVE_UNSELECTED)
		{
			cm[tilemap_number].spawn_point_list_pointer[spawn_point_number].grabbed_status = NODE_UNSELECTED;
		}
		else if (cm[tilemap_number].spawn_point_list_pointer[spawn_point_number].grabbed_status == NODE_PRESUMPTIVE_SELECTED)
		{
			total_flagged++;
			flagged_spawn_point_number = spawn_point_number;
			cm[tilemap_number].spawn_point_list_pointer[spawn_point_number].grabbed_status = NODE_SELECTED;
		}
		else if (cm[tilemap_number].spawn_point_list_pointer[spawn_point_number].grabbed_status == NODE_SELECTED)
		{
			// Already selected, but we've gotta' increase the counter for it.
			total_flagged++;
			flagged_spawn_point_number = spawn_point_number;
		}
	}

	if (total_flagged == 0)
	{
		return NO_NODES_SELECTED;
	}
	else if (total_flagged == 1)
	{
		return flagged_spawn_point_number;
	}
	else
	{
		return MULTIPLE_NODES_SELECTED;
	}

}



void SPAWNPOINTS_register_spawn_point_name_change (int tilemap_number , char *old_entry_name, char *new_entry_name)
{
	// This is a little more complex as not only does it have to change the name of the spawn point but also
	// and of the PARENT/PREV_SIBLING/NEXT_SIBLING properties of other spawn points.

	int i;

	for (i=0; i<cm[tilemap_number].spawn_points ; i++)
	{
		if ( strcmp (cm[tilemap_number].spawn_point_list_pointer[i].name , old_entry_name) == 0)
		{
			strcpy (cm[tilemap_number].spawn_point_list_pointer[i].name , new_entry_name);
			// Obviously this'll only happen on the actual spawn point itself.
		}
	}

}



int SPAWNPOINTS_copy_group (int tilemap_number, int x, int y, int tilesize)
{
	// This goes through the spawn points, figures out which ones are selected and copies
	// them into a position relative to x,y (this is the top-left co-ord). It then deselects
	// the old ones and selects the new ones. Oh, and it also links them up as before. Am I mad?!

	copy_stack_item_struct *copy_stack = NULL;
	int copy_stack_size = 0;

	int spawn_point_number;

	for (spawn_point_number=0; spawn_point_number<cm[tilemap_number].spawn_points; spawn_point_number++)
	{
		if (cm[tilemap_number].spawn_point_list_pointer[spawn_point_number].grabbed_status == NODE_SELECTED)
		{
			copy_stack_size++;
		}
	}

	copy_stack = (copy_stack_item_struct *) malloc (sizeof(copy_stack_item_struct) * copy_stack_size);

	// Okay, so now we know how many there are to copy and we've allocated space for their basic information.
	// Now we have to make a list of all the entities we're going to copy, including their relatives so that
	// once we've created the new ones we can see if any of the old ones were linked and restore those
	// links in the new ones. Blimeroo, eh?

	// While we're here, we also may as well find the top left co-ord of the group so we can plonk them
	// at least *somewhere* near the cursor.

	int copy_stack_item = 0;
	int min_x = UNSET;
	int min_y = UNSET;
	int max_x = UNSET;
	int max_y = UNSET;

	for (spawn_point_number=0; spawn_point_number<cm[tilemap_number].spawn_points; spawn_point_number++)
	{
		if (cm[tilemap_number].spawn_point_list_pointer[spawn_point_number].grabbed_status == NODE_SELECTED)
		{
			if (min_x == UNSET)
			{
				min_x = cm[tilemap_number].spawn_point_list_pointer[spawn_point_number].x;
			}
			else
			{
				if (cm[tilemap_number].spawn_point_list_pointer[spawn_point_number].x < min_x)
				{
					min_x = cm[tilemap_number].spawn_point_list_pointer[spawn_point_number].x;
				}
			}

			if (max_x == UNSET)
			{
				max_x = cm[tilemap_number].spawn_point_list_pointer[spawn_point_number].x;
			}
			else
			{
				if (cm[tilemap_number].spawn_point_list_pointer[spawn_point_number].x > max_x)
				{
					max_x = cm[tilemap_number].spawn_point_list_pointer[spawn_point_number].x;
				}
			}

			if (min_y == UNSET)
			{
				min_y = cm[tilemap_number].spawn_point_list_pointer[spawn_point_number].y;
			}
			else
			{
				if (cm[tilemap_number].spawn_point_list_pointer[spawn_point_number].y < min_y)
				{
					min_y = cm[tilemap_number].spawn_point_list_pointer[spawn_point_number].y;
				}
			}

			if (max_y == UNSET)
			{
				max_y = cm[tilemap_number].spawn_point_list_pointer[spawn_point_number].y;
			}
			else
			{
				if (cm[tilemap_number].spawn_point_list_pointer[spawn_point_number].y > max_y)
				{
					max_y = cm[tilemap_number].spawn_point_list_pointer[spawn_point_number].y;
				}
			}

			copy_stack[copy_stack_item].old_uid = cm[tilemap_number].spawn_point_list_pointer[spawn_point_number].uid;
			copy_stack[copy_stack_item].old_index = spawn_point_number;
			copy_stack[copy_stack_item].old_next_sibling_uid = cm[tilemap_number].spawn_point_list_pointer[spawn_point_number].next_sibling_uid;
			copy_stack[copy_stack_item].old_parent_uid = cm[tilemap_number].spawn_point_list_pointer[spawn_point_number].parent_uid;
			copy_stack_item++;
		}
	}

	int x_diff = (x - min_x) - ((max_x - min_x)/2);
	int y_diff = (y - min_y) - ((max_y - min_y)/2);

//	x_diff = x_diff - (x_diff % tilesize);
//	y_diff = y_diff - (y_diff % tilesize);

	// Okay, so there's our list. Now lets make some copies and store the info about them we'll need.

	// We can also move them at this stage so they are near to the cursor.

	for (copy_stack_item=0; copy_stack_item<copy_stack_size; copy_stack_item++)
	{
		copy_stack[copy_stack_item].new_index = SPAWNPOINTS_copy_spawn_point (tilemap_number,copy_stack[copy_stack_item].old_index,0, tilesize);
		copy_stack[copy_stack_item].new_uid = cm[tilemap_number].spawn_point_list_pointer[copy_stack[copy_stack_item].new_index].uid;

		cm[tilemap_number].spawn_point_list_pointer[copy_stack[copy_stack_item].new_index].x += x_diff;
		cm[tilemap_number].spawn_point_list_pointer[copy_stack[copy_stack_item].new_index].y += y_diff;
	}

	// So that's them all nice and copied.

	// Now we have to relink them. Crap.

	int looking_at_item;
	int my_index;
	int relative_index;

	for (copy_stack_item=0; copy_stack_item<copy_stack_size; copy_stack_item++)
	{
		for (looking_at_item=0; looking_at_item<copy_stack_size; looking_at_item++)
		{
			if (looking_at_item != copy_stack_item)
			{
				my_index = copy_stack[copy_stack_item].new_index;
				relative_index = copy_stack[looking_at_item].new_index;

				if (copy_stack[looking_at_item].old_uid == copy_stack[copy_stack_item].old_parent_uid)
				{
					// ie. copy_stack_item's parent was looking_at_item. Man, this is gonna' bite me in the arse.
					cm[tilemap_number].spawn_point_list_pointer[my_index].parent_uid = cm[tilemap_number].spawn_point_list_pointer[relative_index].uid;
				}

				if (copy_stack[looking_at_item].old_uid == copy_stack[copy_stack_item].old_next_sibling_uid)
				{
					// ie. copy_stack_item's next sibling was looking_at_item. Man, this is gonna' bite me in the arse.
					cm[tilemap_number].spawn_point_list_pointer[my_index].next_sibling_uid = cm[tilemap_number].spawn_point_list_pointer[relative_index].uid;
				}
			}
		}
	}

	// Well, that *should've* done it... So next let's deselect everything...

	SPAWNPOINTS_deselect_spawn_points(tilemap_number);

	// And reselect all the new things we created, and also re-affirm text parameter linkage.

	for (copy_stack_item=0; copy_stack_item<copy_stack_size; copy_stack_item++)
	{
		my_index = copy_stack[copy_stack_item].new_index;

		SPAWNPOINTS_select_individual_spawn_point (tilemap_number,my_index);
		SPAWNPOINTS_check_for_duplicate_relative_links (tilemap_number, my_index);
		SPAWNPOINTS_confirm_spawn_point_links (tilemap_number, my_index);
	}

	int selected_spawn_point = SPAWNPOINTS_lock_spawn_point_status (tilemap_number);

	// And then do all the relinking of uids...

	SPAWNPOINTS_relink_all_spawn_point_relatives (tilemap_number);
	
	// And it's all over bar the shouting. Lots and lots of shouting...

	free (copy_stack);

	return selected_spawn_point;
}



void SPAWNPOINTS_delete_multiple_selected_spawn_points (int tilemap_number)
{
	int spawn_point_number;

	for (spawn_point_number=0; spawn_point_number<cm[tilemap_number].spawn_points; spawn_point_number++)
	{
		if (cm[tilemap_number].spawn_point_list_pointer[spawn_point_number].grabbed_status == NODE_SELECTED)
		{
			SPAWNPOINTS_delete_particular_spawn_point (tilemap_number,spawn_point_number);
			spawn_point_number--; // This is because deleting the spawn point bubbles down the one above it, which might also need deleting.
		}
	}

	SPAWNPOINTS_relink_all_spawn_point_relatives (tilemap_number);
}


/*
void SPAWNPOINTS_recursively_select_family (int tilemap_number, int spawnpoint_number)
{
	// This does the actual recursion bit - it simply tries expanding to it's 
	// rellies if they haven't already been selected.

	int parent_index,child_index,next_sibling_index,prev_sibling_index;

	parent_index = cm[tilemap_number].spawn_point_list_pointer[spawnpoint_number].parent_index;
	child_index = cm[tilemap_number].spawn_point_list_pointer[spawnpoint_number].child_index;
	next_sibling_index = cm[tilemap_number].spawn_point_list_pointer[spawnpoint_number].next_sibling_index;
	prev_sibling_index = cm[tilemap_number].spawn_point_list_pointer[spawnpoint_number].prev_sibling_index;

	if (parent_index != UNSET)
	{
		if (cm[tilemap_number].spawn_point_list_pointer[parent_index].grabbed_status != NODE_SELECTED)
		{
			cm[tilemap_number].spawn_point_list_pointer[parent_index].grabbed_status = NODE_SELECTED;
			SPAWNPOINTS_recursively_select_family (tilemap_number, parent_index);
		}
	}

	if (child_index != UNSET)
	{
		if (cm[tilemap_number].spawn_point_list_pointer[child_index].grabbed_status != NODE_SELECTED)
		{
			cm[tilemap_number].spawn_point_list_pointer[child_index].grabbed_status = NODE_SELECTED;
			SPAWNPOINTS_recursively_select_family (tilemap_number, child_index);
		}
	}

	if (next_sibling_index != UNSET)
	{
		if (cm[tilemap_number].spawn_point_list_pointer[next_sibling_index].grabbed_status != NODE_SELECTED)
		{
			cm[tilemap_number].spawn_point_list_pointer[next_sibling_index].grabbed_status = NODE_SELECTED;
			SPAWNPOINTS_recursively_select_family (tilemap_number, next_sibling_index);
		}
	}

	if (prev_sibling_index != UNSET)
	{
		if (cm[tilemap_number].spawn_point_list_pointer[prev_sibling_index].grabbed_status != NODE_SELECTED)
		{
			cm[tilemap_number].spawn_point_list_pointer[prev_sibling_index].grabbed_status = NODE_SELECTED;
			SPAWNPOINTS_recursively_select_family (tilemap_number, prev_sibling_index);
		}
	}

}
*/


void SPAWNPOINTS_select_all_relatives (int tilemap_number)
{
	// Due to the one-way nature of some familial links (ie, parents only store one child and there's
	// no sibling linking to back that up. It's necessary to do things a little differently. What we
	// do is look through all the spawnpoints and see if there are any of them whose indexes points
	// to an entity who is current selected (rather than the unreliable other way around). If they are,
	// then these entities are selecte and the process goes around for another iteration, otherwise we
	// are done.

	bool found_one;
	int spawn_point_number;
	int parent_index,child_index,next_sibling_index,prev_sibling_index;

	do
	{
		found_one = false;

		for (spawn_point_number=0; spawn_point_number<cm[tilemap_number].spawn_points; spawn_point_number++)
		{


			parent_index = cm[tilemap_number].spawn_point_list_pointer[spawn_point_number].parent_index;
			child_index = cm[tilemap_number].spawn_point_list_pointer[spawn_point_number].child_index;
			next_sibling_index = cm[tilemap_number].spawn_point_list_pointer[spawn_point_number].next_sibling_index;
			prev_sibling_index = cm[tilemap_number].spawn_point_list_pointer[spawn_point_number].prev_sibling_index;

			if (cm[tilemap_number].spawn_point_list_pointer[spawn_point_number].grabbed_status != NODE_SELECTED)
			{
				// This one isn't selected so it's looking for relatives that are...

				if (parent_index != UNSET)
				{
					if (cm[tilemap_number].spawn_point_list_pointer[parent_index].grabbed_status == NODE_SELECTED)
					{
						cm[tilemap_number].spawn_point_list_pointer[spawn_point_number].grabbed_status = NODE_SELECTED;
						found_one = true;
					}
				}

				if (child_index != UNSET)
				{
					if (cm[tilemap_number].spawn_point_list_pointer[child_index].grabbed_status == NODE_SELECTED)
					{
						cm[tilemap_number].spawn_point_list_pointer[spawn_point_number].grabbed_status = NODE_SELECTED;
						found_one = true;
					}
				}

				if (next_sibling_index != UNSET)
				{
					if (cm[tilemap_number].spawn_point_list_pointer[next_sibling_index].grabbed_status == NODE_SELECTED)
					{
						cm[tilemap_number].spawn_point_list_pointer[spawn_point_number].grabbed_status = NODE_SELECTED;
						found_one = true;
					}
				}

				if (prev_sibling_index != UNSET)
				{
					if (cm[tilemap_number].spawn_point_list_pointer[prev_sibling_index].grabbed_status == NODE_SELECTED)
					{
						cm[tilemap_number].spawn_point_list_pointer[spawn_point_number].grabbed_status = NODE_SELECTED;
						found_one = true;
					}
				}

			}
			else
			{
				// This one is selected, so it's looking for relatives that aren't.

				if (parent_index != UNSET)
				{
					if (cm[tilemap_number].spawn_point_list_pointer[parent_index].grabbed_status != NODE_SELECTED)
					{
						cm[tilemap_number].spawn_point_list_pointer[parent_index].grabbed_status = NODE_SELECTED;
						found_one = true;
					}
				}

				if (child_index != UNSET)
				{
					if (cm[tilemap_number].spawn_point_list_pointer[child_index].grabbed_status != NODE_SELECTED)
					{
						cm[tilemap_number].spawn_point_list_pointer[child_index].grabbed_status = NODE_SELECTED;
						found_one = true;
					}
				}

				if (next_sibling_index != UNSET)
				{
					if (cm[tilemap_number].spawn_point_list_pointer[next_sibling_index].grabbed_status != NODE_SELECTED)
					{
						cm[tilemap_number].spawn_point_list_pointer[next_sibling_index].grabbed_status = NODE_SELECTED;
						found_one = true;
					}
				}

				if (prev_sibling_index != UNSET)
				{
					if (cm[tilemap_number].spawn_point_list_pointer[prev_sibling_index].grabbed_status != NODE_SELECTED)
					{
						cm[tilemap_number].spawn_point_list_pointer[prev_sibling_index].grabbed_status = NODE_SELECTED;
						found_one = true;
					}
				}

			}

		}

	}
	while (found_one);

}


/*
void SPAWNPOINTS_grow_selection_by_family (int tilemap_number)
{
	// This simply looks for relatives of the selected spawn point and selects them.
	// And repeats. Until there are no more selected.

	int spawnpoint_number;

	for (spawnpoint_number=0; spawnpoint_number<cm[tilemap_number].spawn_points; spawnpoint_number++)
	{
		if (cm[tilemap_number].spawn_point_list_pointer[spawnpoint_number].grabbed_status == NODE_SELECTED)
		{
			SPAWNPOINTS_recursively_select_family (tilemap_number, spawnpoint_number);
		}
	}
}
*/


bool SPAWNPOINTS_edit_spawn_points (int state , bool overlay_display, int *current_cursor ,int sx, int sy,  int tilemap_number, int mx, int my, float zoom_level, int grid_size)
{
	static int selected_spawn_point;

	char script_word [NAME_SIZE] = "SCRIPTS";

	int tx,ty;
	int real_tx, real_ty;
	int real_mx, real_my;

	int tileset_number;
	int tilesize;

	static int drag_start_x;
	static int drag_start_y;
	static int drag_end_x;
	static int drag_end_y;
	static int drag_time;

	int screen_x1,screen_y1,screen_x2,screen_y2;

	static bool override_main_editor;

	static int matching_name_counter;

	static int selecting_relation_type;

	static char spawn_point_script[NAME_SIZE];
	static char scripts_name[NAME_SIZE];
	static char variables_name[NAME_SIZE];
	static char type_word[NAME_SIZE];

	static char type_name[NAME_SIZE];

	static char backup_text_1[NAME_SIZE];
	static char backup_text_2[NAME_SIZE];
	static char backup_text_3[NAME_SIZE];

	static char spawn_point_name[NAME_SIZE]; // Used only when changing the name of a selected spawn point.

	static char parameter_variable[MAX_PARAMETERS_FOR_SCRIPT][NAME_SIZE];
	static char parameter_list[MAX_PARAMETERS_FOR_SCRIPT][NAME_SIZE];
	static char parameter_entry[MAX_PARAMETERS_FOR_SCRIPT][NAME_SIZE];

	static int first_parameter_in_list;
	static int editing_parameter_number;
	static int total_parameters;

	static int grabbed_spawn_point_real_x;
	static int grabbed_spawn_point_real_y;
	static int grabbed_real_mx;
	static int grabbed_real_my;

	static int selecting_node_mode;

	static int editing_property;

	int relative;

	static int getting_new_name_status;

	static int spawn_point_editing_mode;
	static int grabbed_spawn_point;

	int nearest_spawn_point;

	int mouse_in_parameter = UNSET;
	int mouse_in_box = UNSET;

	static int grabbed_handle;

	int box_size = 152;

	static bool delete_selected;

	int t;

	if (state == STATE_INITIALISE)
	{
		selected_spawn_point = UNSET;
		grabbed_spawn_point = UNSET;

		override_main_editor = false;

		matching_name_counter = 0;

		selecting_relation_type = UNSET;

		strcpy (spawn_point_script,"UNSET");
		strcpy (scripts_name,"SCRIPTS");;
		strcpy (variables_name,"VARIABLE");
		strcpy (type_word,"SPAWN_POINT_TYPES");

		strcpy (type_name,"UNSET");

		grabbed_handle = UNSET;

		getting_new_name_status = GET_WORD;

		editing_property = UNSET;

		total_parameters = 0;
		editing_parameter_number = 0;
		first_parameter_in_list = 0;

		delete_selected = false;

		spawn_point_editing_mode = SPAWN_POINT_EDIT_MODE;
	}
	else if (state == STATE_SET_UP_BUTTONS)
	{
		editor_view_width = game_screen_width - (ICON_SIZE*8);
		editor_view_height = game_screen_height - (ICON_SIZE*4);

		SIMPGUI_create_set_button (TILEMAP_EDITOR_SUB_GROUP, EDIT_TILEMAP, &spawn_point_editing_mode, editor_view_width+(ICON_SIZE/2)+(2*ICON_SIZE) , ICON_SIZE*1, first_icon , NEW_SPAWN_POINT_ICON, LMB, SPAWN_POINT_CREATE_MODE);
		SIMPGUI_create_set_button (TILEMAP_EDITOR_SUB_GROUP, EDIT_TILEMAP, &spawn_point_editing_mode, editor_view_width+(ICON_SIZE/2)+(5*ICON_SIZE) , ICON_SIZE*1, first_icon , EDIT_SPAWN_POINT_ICON, LMB, SPAWN_POINT_EDIT_MODE);
	}
	else if (state == STATE_RUNNING)
	{
		// Get the tilesize...

		tileset_number = cm[tilemap_number].tile_set_index;

		if (tileset_number != UNSET)
		{
			tilesize = TILESETS_get_tilesize(tileset_number);
		}
		else
		{
			tilesize = 16;
		}

		// Rounded properly in this case.
		tx = (mx+(tilesize/2)) / int (float(tilesize)*zoom_level);
		ty = (my+(tilesize/2)) / int (float(tilesize)*zoom_level);

		real_tx = tx + sx;
		real_ty = ty + sy;

		real_mx = int (float(mx)/zoom_level) + (sx*tilesize);
		real_my = int (float(my)/zoom_level) + (sy*tilesize);

		nearest_spawn_point = SPAWNPOINTS_find_nearest_spawn_point (tilemap_number, real_mx, real_my, selected_spawn_point);

		if (spawn_point_editing_mode == SPAWN_POINT_CREATE_MODE)
		{
			selected_spawn_point = UNSET;
		}

		if (selected_spawn_point == UNSET)
		{
			if (spawn_point_editing_mode == SPAWN_POINT_CREATE_MODE)
			{
				SPAWNPOINTS_draw_spawn_points (tilemap_number, sx, sy, editor_view_width, editor_view_height, zoom_level, UNSET , true);
			}
			else
			{
				SPAWNPOINTS_draw_spawn_points (tilemap_number, sx, sy, editor_view_width, editor_view_height, zoom_level, nearest_spawn_point , true);
			}
		}
		else
		{
			if (spawn_point_editing_mode != SPAWN_POINT_CHOOSE_RELATIVE)
			{
				SPAWNPOINTS_draw_spawn_points (tilemap_number, sx, sy, editor_view_width, editor_view_height, zoom_level, selected_spawn_point , false);
			}
			else
			{
				// If we're choosing a relative for the selected spawn point then display the nearest one
				// instead of the selected one.
				SPAWNPOINTS_draw_spawn_points (tilemap_number, sx, sy, editor_view_width, editor_view_height, zoom_level, nearest_spawn_point , true);
			}
		}

		if (overlay_display)
		{
			editor_view_width = game_screen_width - (ICON_SIZE*8);
			editor_view_height = game_screen_height - (ICON_SIZE*4);

			OUTPUT_filled_rectangle (editor_view_width,0,game_screen_width,editor_view_height,0,0,0);
			OUTPUT_filled_rectangle (0,editor_view_height,game_screen_width,game_screen_height,0,0,0);

			OUTPUT_vline (editor_view_width,0,editor_view_height,255,255,255);
			OUTPUT_hline (0,editor_view_height,editor_view_width,255,255,255);

			OUTPUT_draw_masked_sprite ( first_icon , EDIT_MODE_ICON, editor_view_width+(ICON_SIZE/2)+(0*ICON_SIZE) , ICON_SIZE*1 );

			if (spawn_point_editing_mode<2)
			{
				OUTPUT_draw_masked_sprite ( first_icon , FORWARD_ARROW, editor_view_width+(ICON_SIZE/2)+(3*ICON_SIZE*spawn_point_editing_mode)+ICON_SIZE , ICON_SIZE*1 );
				OUTPUT_draw_masked_sprite ( first_icon , BACKWARD_ARROW, editor_view_width+(ICON_SIZE/2)+(3*ICON_SIZE*spawn_point_editing_mode)+ICON_SIZE*3 , ICON_SIZE*1 );
			}

			OUTPUT_centred_text ( editor_view_width+(ICON_SIZE*4) , (ICON_SIZE-FONT_HEIGHT)/2 , "SPAWN POINT EDITOR" );

			SIMPGUI_draw_all_buttons (TILESET_SUB_GROUP_ID,true);

			if (selected_spawn_point >= 0)
			{
				// Only draw script gubbins if we have a single entity selected.

				OUTPUT_boxed_centred_text (320,editor_view_height+12,"SCRIPT CHOOSING");

				OUTPUT_centred_text (box_size/2,editor_view_height+(ICON_SIZE*1)+8,"CURRENT SCRIPT");
				OUTPUT_truncated_text ((box_size/FONT_WIDTH)-1,(FONT_WIDTH/2),editor_view_height+(ICON_SIZE*1)+24,spawn_point_script);

				OUTPUT_rectangle ( 0, editor_view_height+(ICON_SIZE*1) ,box_size-1 ,editor_view_height+(ICON_SIZE*3)-1 ,0 ,0 ,255 );

				if (delete_selected == true)
				{
					OUTPUT_rectangle ( 0, editor_view_height+(ICON_SIZE*3) ,box_size-1 ,editor_view_height+(ICON_SIZE*4)-1 ,255 ,255 ,255 );
					OUTPUT_centred_text (box_size/2,editor_view_height+(ICON_SIZE*3)+12,"DELETE PARAMETER");
				}
				else
				{
					OUTPUT_rectangle ( 0, editor_view_height+(ICON_SIZE*3) ,box_size-1 ,editor_view_height+(ICON_SIZE*4)-1 ,0 ,0 ,255 );
					OUTPUT_centred_text (box_size/2,editor_view_height+(ICON_SIZE*3)+12,"DELETE PARAMETER",0,0,255);
				}

				// Output the list of current parameters...

				for (t=0; (t<total_parameters+1) && (t<(ICON_SIZE*4)/16); t++)
				{
					if (MATH_rectangle_to_point_intersect( (box_size*1), editor_view_height+(ICON_SIZE*1)+(t*16), (box_size*4)-1, editor_view_height+(ICON_SIZE*1)+(t*16)+15 , mx, my) == true)
					{
						mouse_in_parameter = t+first_parameter_in_list;
						mouse_in_box = (mx-box_size) / box_size;
					}

					OUTPUT_rectangle ( (box_size*1), editor_view_height+(ICON_SIZE*1)+(t*16), (box_size*2)-1, editor_view_height+(ICON_SIZE*1)+(t*16)+15 , 255 , 0 , 0 );
					OUTPUT_rectangle ( (box_size*2), editor_view_height+(ICON_SIZE*1)+(t*16), (box_size*3)-1, editor_view_height+(ICON_SIZE*1)+(t*16)+15 , 255 , 0 , 0 );
					OUTPUT_rectangle ( (box_size*3), editor_view_height+(ICON_SIZE*1)+(t*16), (box_size*4)-1, editor_view_height+(ICON_SIZE*1)+(t*16)+15 , 255 , 0 , 0 );

					if (t+first_parameter_in_list<total_parameters)
					{
						OUTPUT_truncated_text ( (box_size/FONT_WIDTH)-1 , (box_size*1) + (FONT_WIDTH/2) , editor_view_height+(ICON_SIZE*1)+(t*16)+(FONT_HEIGHT/2) , parameter_variable[t+first_parameter_in_list] , 255, 255, 0 );
						OUTPUT_truncated_text ( (box_size/FONT_WIDTH)-1 , (box_size*2) + (FONT_WIDTH/2) , editor_view_height+(ICON_SIZE*1)+(t*16)+(FONT_HEIGHT/2) , parameter_list[t+first_parameter_in_list] , 0, 255, 255 );
						OUTPUT_truncated_text ( (box_size/FONT_WIDTH)-1 , (box_size*3) + (FONT_WIDTH/2) , editor_view_height+(ICON_SIZE*1)+(t*16)+(FONT_HEIGHT/2) , parameter_entry[t+first_parameter_in_list] , 0, 255, 0 );
					}
					else
					{
						OUTPUT_truncated_text ( (box_size/FONT_WIDTH)-1 , (box_size*1) + (FONT_WIDTH/2) , editor_view_height+(ICON_SIZE*1)+(t*16)+(FONT_HEIGHT/2) , "CREATE" , 255, 0, 255 );
						OUTPUT_truncated_text ( (box_size/FONT_WIDTH)-1 , (box_size*2) + (FONT_WIDTH/2) , editor_view_height+(ICON_SIZE*1)+(t*16)+(FONT_HEIGHT/2) , "NEW" , 255, 0, 255 );
						OUTPUT_truncated_text ( (box_size/FONT_WIDTH)-1 , (box_size*3) + (FONT_WIDTH/2) , editor_view_height+(ICON_SIZE*1)+(t*16)+(FONT_HEIGHT/2) , "PARAMETER" , 255, 0, 255 );
					}
				}

				// Show spawn point type.

				OUTPUT_centred_text (editor_view_width+(ICON_SIZE*4),(ICON_SIZE*2)+12,"SPAWN POINT TYPE");
				OUTPUT_rectangle ( editor_view_width+(ICON_SIZE/2), (ICON_SIZE*3), game_screen_width-(ICON_SIZE/2), (ICON_SIZE*4) , 0 , 0 , 255 );
				OUTPUT_truncated_text ( ((ICON_SIZE*7)/FONT_WIDTH)-1, editor_view_width+(ICON_SIZE/2)+(FONT_WIDTH/2) , (ICON_SIZE*3)+12 , cm[tilemap_number].spawn_point_list_pointer[selected_spawn_point].type , 255, 255, 255 );

				// Show spawn point name

				OUTPUT_centred_text (editor_view_width+(ICON_SIZE*4),(ICON_SIZE*4)+12,"SPAWN POINT NAME");
				OUTPUT_rectangle ( editor_view_width+(ICON_SIZE/2), (ICON_SIZE*5), game_screen_width-(ICON_SIZE/2), (ICON_SIZE*6) , 0 , 0 , 255 );
				OUTPUT_truncated_text ( ((ICON_SIZE*7)/FONT_WIDTH)-1, editor_view_width+(ICON_SIZE/2)+(FONT_WIDTH/2) , (ICON_SIZE*5)+12 , cm[tilemap_number].spawn_point_list_pointer[selected_spawn_point].name , 255, 255, 255 );

				// The relatives!

				OUTPUT_centred_text (editor_view_width+(ICON_SIZE*4),(ICON_SIZE*6)+12,"SPAWN POINT RELATIVES");

				OUTPUT_rectangle ( editor_view_width+(ICON_SIZE/2), (ICON_SIZE*7), game_screen_width-(ICON_SIZE/2), (ICON_SIZE*8) , 0 , 0 , 255 );
				OUTPUT_truncated_text ( ((ICON_SIZE*7)/FONT_WIDTH)-1, editor_view_width+(ICON_SIZE/2)+(FONT_WIDTH/2) , (ICON_SIZE*7)+(ICON_SIZE/2)-(FONT_HEIGHT) , "PARENT" , 255, 0, 0 );
				if (cm[tilemap_number].spawn_point_list_pointer[selected_spawn_point].parent_index != UNSET)
				{
					OUTPUT_truncated_text ( ((ICON_SIZE*7)/FONT_WIDTH)-1, editor_view_width+(ICON_SIZE/2)+(FONT_WIDTH/2) , (ICON_SIZE*7)+(ICON_SIZE/2) , cm[tilemap_number].spawn_point_list_pointer[cm[tilemap_number].spawn_point_list_pointer[selected_spawn_point].parent_index].name , 255, 255, 255 );
				}
				else
				{
					OUTPUT_truncated_text ( ((ICON_SIZE*7)/FONT_WIDTH)-1, editor_view_width+(ICON_SIZE/2)+(FONT_WIDTH/2) , (ICON_SIZE*7)+(ICON_SIZE/2) , "UNSET" , 255, 0, 255 );
				}

				OUTPUT_rectangle ( editor_view_width+(ICON_SIZE/2), (ICON_SIZE*8), game_screen_width-(ICON_SIZE/2), (ICON_SIZE*9) , 0 , 0 , 255 );
				OUTPUT_truncated_text ( ((ICON_SIZE*7)/FONT_WIDTH)-1, editor_view_width+(ICON_SIZE/2)+(FONT_WIDTH/2) , (ICON_SIZE*8)+(ICON_SIZE/2)-(FONT_HEIGHT) , "NEXT SIBLING" , 255, 0, 0 );
				if (cm[tilemap_number].spawn_point_list_pointer[selected_spawn_point].next_sibling_index != UNSET)
				{
					OUTPUT_truncated_text ( ((ICON_SIZE*7)/FONT_WIDTH)-1, editor_view_width+(ICON_SIZE/2)+(FONT_WIDTH/2) , (ICON_SIZE*8)+(ICON_SIZE/2) , cm[tilemap_number].spawn_point_list_pointer[cm[tilemap_number].spawn_point_list_pointer[selected_spawn_point].next_sibling_index].name , 255, 255, 255 );
				}
				else
				{
					OUTPUT_truncated_text ( ((ICON_SIZE*7)/FONT_WIDTH)-1, editor_view_width+(ICON_SIZE/2)+(FONT_WIDTH/2) , (ICON_SIZE*8)+(ICON_SIZE/2) , "UNSET" , 255, 0, 255 );
				}

				// Show COPY icon and it's options

				OUTPUT_draw_masked_sprite ( first_icon , COPY_SPAWN_POINT_ICON, editor_view_width+(ICON_SIZE/2) , ICON_SIZE*10 );

				for (t=1; t<5; t++)
				{
					OUTPUT_draw_masked_sprite ( first_icon , NOT_FAMILY_ICON+t, editor_view_width+(ICON_SIZE/2)+(ICON_SIZE*t), ICON_SIZE*10 );
				}

				OUTPUT_draw_masked_sprite ( first_icon , ERASE_SPAWN_POINT_ICON, editor_view_width+(ICON_SIZE/2)+(ICON_SIZE*6), ICON_SIZE*10 );
				
			}

			SIMPGUI_draw_all_buttons (TILESET_SUB_GROUP_ID);
			
			SIMPGUI_check_all_buttons ();
		}
		else
		{
			editor_view_width = game_screen_width;
			editor_view_height = game_screen_height;
		}

		if ( (mx<editor_view_width) && (my<editor_view_height) && (override_main_editor == false) )
		{

			if (spawn_point_editing_mode == SPAWN_POINT_CREATE_MODE)
			{
				OUTPUT_circle ( mx - (mx % (int(float(grid_size) * zoom_level))) , my - (my % (int(float(grid_size) * zoom_level))) , SPAWN_POINT_HANDLE_RADIUS-1 , 0 , 0 , 255 );
			}

			if (CONTROL_mouse_hit(LMB) == true)
			{
				if (spawn_point_editing_mode == SPAWN_POINT_EDIT_MODE)
				{
					if ( (SPAWNPOINTS_what_selected_spawn_point_thing_has_been_grabbed (tilemap_number, real_mx, real_my, zoom_level, &grabbed_handle, &grabbed_spawn_point) == false) || (CONTROL_key_down(KEY_LSHIFT) == true) || (CONTROL_key_down(KEY_LCONTROL) == true) )
					{
						// Failed to grab a handle or purposely dragging multiple spawn points.

						spawn_point_editing_mode = SPAWN_POINT_DRAGGING_OVER_NODES;

						// We're dragging a box!

						if (CONTROL_key_down(KEY_LSHIFT) == true)
						{
							selecting_node_mode = ADDING_NODES;
						}
						else if (CONTROL_key_down(KEY_LCONTROL) == true)
						{
							selecting_node_mode = REMOVING_NODES;
						}
						else
						{
							selecting_node_mode = SELECTING_NODES;
						}

						drag_start_x = real_mx;
						drag_start_y = real_my;
						drag_time = 0;
					}
					else
					{
						grabbed_real_mx = real_mx;
						grabbed_real_my = real_my;

						SPAWNPOINTS_get_grabbed_spawn_point_co_ords (tilemap_number);
					}
				}
				else if (spawn_point_editing_mode == SPAWN_POINT_CREATE_MODE)
				{
					SPAWNPOINTS_create_spawn_point (tilemap_number,real_mx-(real_mx%grid_size),real_my-(real_my%grid_size),"SPAWN_POINT_", tilesize);
				}
				else if (spawn_point_editing_mode == SPAWN_POINT_CHOOSE_RELATIVE)
				{
					if ( (nearest_spawn_point != UNSET) && (selected_spawn_point >= 0) )
					{
						if (selecting_relation_type == SPAWN_POINT_SELECT_RELATION_PARENT)
						{
							cm[tilemap_number].spawn_point_list_pointer[selected_spawn_point].parent_uid = cm[tilemap_number].spawn_point_list_pointer[nearest_spawn_point].uid;
							spawn_point_editing_mode = SPAWN_POINT_EDIT_MODE;
						}

						if (selecting_relation_type == SPAWN_POINT_SELECT_RELATION_NEXT_SIBLING)
						{
							cm[tilemap_number].spawn_point_list_pointer[selected_spawn_point].next_sibling_uid =cm[tilemap_number].spawn_point_list_pointer[nearest_spawn_point].uid;
							spawn_point_editing_mode = SPAWN_POINT_EDIT_MODE;
						}

						SPAWNPOINTS_check_for_duplicate_relative_links (tilemap_number, selected_spawn_point);
						SPAWNPOINTS_relink_all_spawn_point_relatives (tilemap_number);
						SPAWNPOINTS_confirm_spawn_point_links (tilemap_number, selected_spawn_point);
					}
				}
			}

			if (CONTROL_mouse_hit(RMB) == true)
			{
				// Deselect nodes or leave creation mode...

				if (spawn_point_editing_mode == SPAWN_POINT_CREATE_MODE)
				{
					spawn_point_editing_mode = SPAWN_POINT_EDIT_MODE;
				}
				else if (spawn_point_editing_mode == SPAWN_POINT_CHOOSE_RELATIVE)
				{
					if (selecting_relation_type == SPAWN_POINT_SELECT_RELATION_PARENT)
					{
						cm[tilemap_number].spawn_point_list_pointer[selected_spawn_point].parent_uid = UNSET;
						spawn_point_editing_mode = SPAWN_POINT_EDIT_MODE;
					}

					if (selecting_relation_type == SPAWN_POINT_SELECT_RELATION_NEXT_SIBLING)
					{
						cm[tilemap_number].spawn_point_list_pointer[selected_spawn_point].next_sibling_uid = UNSET;
						spawn_point_editing_mode = SPAWN_POINT_EDIT_MODE;
					}

					SPAWNPOINTS_check_for_duplicate_relative_links (tilemap_number, selected_spawn_point);
					SPAWNPOINTS_relink_all_spawn_point_relatives (tilemap_number);
					SPAWNPOINTS_confirm_spawn_point_links (tilemap_number, selected_spawn_point);
				}
				else if (spawn_point_editing_mode == SPAWN_POINT_EDIT_MODE)
				{
					SPAWNPOINTS_deselect_spawn_points (tilemap_number);
				}
			}

			if (CONTROL_mouse_down(LMB) == true)
			{
				if (spawn_point_editing_mode == SPAWN_POINT_DRAGGING_OVER_NODES)
				{
					// Continue dragging the box...

					selected_spawn_point = UNSET;

					drag_end_x = real_mx;
					drag_end_y = real_my;

					screen_x1 = int(float(drag_start_x - (sx * tilesize)) * zoom_level);
					screen_y1 = int(float(drag_start_y - (sy * tilesize)) * zoom_level);
					screen_x2 = int(float(drag_end_x - (sx * tilesize)) * zoom_level);
					screen_y2 = int(float(drag_end_y - (sy * tilesize)) * zoom_level);

					drag_time++;

					OUTPUT_rectangle (screen_x1,screen_y1,screen_x2,screen_y2,255,255,255);

					SPAWNPOINTS_partially_set_spawn_point_status (tilemap_number, drag_start_x, drag_start_y, drag_end_x, drag_end_y, zoom_level, selecting_node_mode, drag_time);
				}
				else if (spawn_point_editing_mode == SPAWN_POINT_EDIT_MODE)
				{
					if (grabbed_handle == SPAWN_POINT_HANDLE_CENTER)
					{
						SPAWNPOINTS_position_selected_spawn_points (tilemap_number, (real_mx - grabbed_real_mx), (real_my - grabbed_real_my), grabbed_spawn_point, grid_size, true);
					}

					if (grabbed_handle == SPAWN_POINT_HANDLE_HORIZONTAL)
					{
						SPAWNPOINTS_position_selected_spawn_points (tilemap_number, (real_mx - grabbed_real_mx), 0, grabbed_spawn_point, grid_size, false);
					}

					if (grabbed_handle == SPAWN_POINT_HANDLE_VERTICAL)
					{
						SPAWNPOINTS_position_selected_spawn_points (tilemap_number, 0, (real_my - grabbed_real_my), grabbed_spawn_point, grid_size, false);
					}
				}

			}
			else
			{
				if (spawn_point_editing_mode == SPAWN_POINT_DRAGGING_OVER_NODES)
				{
					selected_spawn_point = SPAWNPOINTS_lock_spawn_point_status (tilemap_number);
					spawn_point_editing_mode = SPAWN_POINT_EDIT_MODE;
				}
				else if (spawn_point_editing_mode == SPAWN_POINT_EDIT_MODE)
				{
					grabbed_handle = UNSET;
				}
			}

		}

		// Control panel stuff! First the script stuff!

		if (selected_spawn_point >= 0)
		{
			// Only allow alteration of script properties if we have a spawn point selected.

			// First of all, read in all the current script properties...

			total_parameters = cm[tilemap_number].spawn_point_list_pointer[selected_spawn_point].params;

			for (t=0; t<total_parameters; t++)
			{
				strcpy (parameter_variable[t] , cm[tilemap_number].spawn_point_list_pointer[selected_spawn_point].param_list_pointer[t].param_dest_var );
				strcpy (parameter_list[t] , cm[tilemap_number].spawn_point_list_pointer[selected_spawn_point].param_list_pointer[t].param_list_type );
				strcpy (parameter_entry[t] , cm[tilemap_number].spawn_point_list_pointer[selected_spawn_point].param_list_pointer[t].param_name );
			}

			strcpy ( spawn_point_script , cm[tilemap_number].spawn_point_list_pointer[selected_spawn_point].script_name );
			strcpy ( type_name , cm[tilemap_number].spawn_point_list_pointer[selected_spawn_point].type );

			if ( (override_main_editor == true) && (selected_spawn_point != UNSET) )
			{
				if (editing_property == SPAWN_POINT_PROPERTY_EDITING_PARAMETER_VARIABLE)
				{
					override_main_editor = EDIT_gpl_list_menu (80, 48, variables_name , parameter_variable[editing_parameter_number], true);
				}
				else if (editing_property == SPAWN_POINT_PROPERTY_EDITING_PARAMETER_VALUE)
				{
					override_main_editor = EDIT_gpl_list_menu (80, 48, parameter_list[editing_parameter_number] , parameter_entry[editing_parameter_number], false);
				}
				else if (editing_property == SPAWN_POINT_PROPERTY_EDITING_SCRIPT)
				{
					override_main_editor = EDIT_gpl_list_menu (80, 48, scripts_name , spawn_point_script, true);
				}
				else if (editing_property == SPAWN_POINT_PROPERTY_EDITING_TYPE)
				{
					override_main_editor = EDIT_gpl_list_menu (80, 48, type_word , type_name, true);
				}
				else if (editing_property == SPAWN_POINT_PROPERTY_EDITING_NAME)
				{
					getting_new_name_status = CONTROL_get_word (spawn_point_name , getting_new_name_status , NAME_SIZE , false, true);

					OUTPUT_filled_rectangle_by_size ( (game_screen_width/2)-(((FONT_WIDTH/2)*NAME_SIZE)+FONT_WIDTH) , 256 , ((NAME_SIZE+2)*FONT_WIDTH) , ICON_SIZE , 0 , 0 , 0 );
					OUTPUT_rectangle_by_size ( (game_screen_width/2)-(((FONT_WIDTH/2)*NAME_SIZE)+FONT_WIDTH) , 256 , ((NAME_SIZE+2)*FONT_WIDTH) , ICON_SIZE , 0 , 0 , 255 );
					OUTPUT_text ( (game_screen_width/2)-((FONT_WIDTH/2)*NAME_SIZE) , 256+(ICON_SIZE/2)-(FONT_HEIGHT) , spawn_point_name );
					OUTPUT_text ( (game_screen_width/2)-((FONT_WIDTH/2)*NAME_SIZE) , 256+(ICON_SIZE/2) , cm[tilemap_number].spawn_point_list_pointer[selected_spawn_point].name , 255,0,255 );

					if (getting_new_name_status == DO_NOTHING)
					{
						if (SPAWNPOINTS_check_for_duplicate_name (tilemap_number, selected_spawn_point, spawn_point_name) == true)
						{
							// If there's already a spawn point with this name, then tell the user and don't exit the name editing mode.
							matching_name_counter = 60;
							getting_new_name_status = GET_WORD;
						}
						else
						{
							matching_name_counter = 0;
							getting_new_name_status = GET_WORD;
							override_main_editor = false;
							SPAWNPOINTS_register_spawn_point_name_change (tilemap_number , cm[tilemap_number].spawn_point_list_pointer[selected_spawn_point].name , spawn_point_name);
						}						
					}
				}

				if (override_main_editor == false)
				{
					CONTROL_lock_mouse_button (LMB);
				}
			}

			if (matching_name_counter>0)
			{
				matching_name_counter--;
				OUTPUT_centred_text ( (game_screen_width/2) , 256+(ICON_SIZE*1)+(FONT_HEIGHT) , "DUPLICATE SPAWN POINT NAME! CHOOSE ANOTHER!" , 255,0,0 );
			}

			if ( (CONTROL_mouse_hit(LMB)) && (override_main_editor == false) && (my>editor_view_height) && (selected_spawn_point >= 0) )
			{
				if (mouse_in_parameter != UNSET)
				{
					if ( (mouse_in_parameter == total_parameters) && (total_parameters < MAX_PARAMETERS_FOR_SCRIPT-1) && (delete_selected == false) )
					{
						// Create new parameter!
						strcpy(parameter_variable[total_parameters] , "UNSET" );
						strcpy(parameter_list[total_parameters] , "UNSET" );
						strcpy(parameter_entry[total_parameters] , "UNSET" );
						total_parameters++;

						SIMPGUI_kill_button (TILESET_SUB_GROUP_ID, ALL_OTHER_GUI_ELEMENTS);
					
						if (total_parameters>5)
						{
							SIMPGUI_create_v_drag_bar (TILESET_SUB_GROUP_ID, ALL_OTHER_GUI_ELEMENTS, &first_parameter_in_list, box_size*4, editor_view_height+(ICON_SIZE*1), ICON_SIZE, 96, 0, total_parameters-5, 1, ICON_SIZE, LMB);
						}
					}
					else
					{
						if (delete_selected == false)
						{
							// You've selected an option which means it's gonna' be altered.

							if (mouse_in_box == 0)
							{
								editing_parameter_number = mouse_in_parameter;
								editing_property = SPAWN_POINT_PROPERTY_EDITING_PARAMETER_VARIABLE;
								override_main_editor = true;
							}
							else if ( (mouse_in_box == 1) || (mouse_in_box == 2) )
							{
								editing_parameter_number = mouse_in_parameter;
								editing_property = SPAWN_POINT_PROPERTY_EDITING_PARAMETER_VALUE;
								override_main_editor = true;
							}
						}
						else if (mouse_in_parameter < total_parameters)
						{
							delete_selected = false;

							for (t=mouse_in_parameter ; t<total_parameters-1; t++)
							{
								strcpy (parameter_variable[t] , parameter_variable[t+1] );
								strcpy (parameter_list[t] , parameter_list[t+1] );
								strcpy (parameter_entry[t] , parameter_entry[t+1] );
							}
							total_parameters--;

							SIMPGUI_kill_button (TILESET_SUB_GROUP_ID, ALL_OTHER_GUI_ELEMENTS);

							if (total_parameters>5)
							{
								SIMPGUI_create_v_drag_bar (TILESET_SUB_GROUP_ID, ALL_OTHER_GUI_ELEMENTS, &first_parameter_in_list, box_size*4, editor_view_height+(ICON_SIZE*2), ICON_SIZE, 96, 0, total_parameters-5, 1, ICON_SIZE, LMB);
							}
							else
							{
								first_parameter_in_list = 0;
							}
						}

					}
				}
			}

			if (selected_spawn_point >= 0) // Might have been deleted - so check!
			{
				// After all that, write all the gubbins back into the spawn point...

				SPAWNPOINTS_destroy_spawn_point_params (tilemap_number, selected_spawn_point);

				// Add the new params...

				for (t=0; t<total_parameters; t++)
				{
					SPAWNPOINTS_add_spawn_point_param (tilemap_number, selected_spawn_point, parameter_variable[t] , parameter_list[t] , parameter_entry[t] );
				}

				strcpy ( cm[tilemap_number].spawn_point_list_pointer[selected_spawn_point].script_name , spawn_point_script );

				strcpy ( cm[tilemap_number].spawn_point_list_pointer[selected_spawn_point].type , type_name );

				// Then relink 'em.

				SPAWNPOINTS_confirm_spawn_point_links (tilemap_number,selected_spawn_point);
			}

			// Then deal with input...

			if ( (override_main_editor == false) && (selected_spawn_point >= 0) && (CONTROL_mouse_hit(LMB) == true) && ( (my>editor_view_height) || (mx>editor_view_width) ) )
			{
				if ( (MATH_rectangle_to_point_intersect( 0, editor_view_height+(ICON_SIZE*1) ,box_size-1 ,editor_view_height+(ICON_SIZE*3)-1 ,mx,my) == true)  && (override_main_editor == false) )
				{
					// Clicking to alter the script name.
					editing_property = SPAWN_POINT_PROPERTY_EDITING_SCRIPT;
					override_main_editor = true;
					CONTROL_lock_mouse_button (LMB);
					strcpy(backup_text_1 , cm[tilemap_number].spawn_point_list_pointer[selected_spawn_point].script_name);
				}

				if ( (MATH_rectangle_to_point_intersect ( editor_view_width+(ICON_SIZE/2), (ICON_SIZE*3) ,game_screen_width-(ICON_SIZE/2) , (ICON_SIZE*4) ,mx,my) == true) && (override_main_editor == false) )
				{
					// Clicking to alter the spawn point type.
					editing_property = SPAWN_POINT_PROPERTY_EDITING_TYPE;
					override_main_editor = true;
					CONTROL_lock_mouse_button (LMB);
					strcpy(backup_text_1 , cm[tilemap_number].spawn_point_list_pointer[selected_spawn_point].type);
				}

				if ( (MATH_rectangle_to_point_intersect ( editor_view_width+(ICON_SIZE/2), (ICON_SIZE*5) ,game_screen_width-(ICON_SIZE/2) , (ICON_SIZE*6) ,mx,my) == true) && (override_main_editor == false) )
				{
					// Clicking to alter the spawn point type.
					editing_property = SPAWN_POINT_PROPERTY_EDITING_NAME;
					override_main_editor = true;
					CONTROL_lock_mouse_button (LMB);
					strcpy (spawn_point_name,cm[tilemap_number].spawn_point_list_pointer[selected_spawn_point].name);
					strcpy(backup_text_1 , cm[tilemap_number].spawn_point_list_pointer[selected_spawn_point].name);
				}

				if (MATH_rectangle_to_point_intersect ( editor_view_width+(ICON_SIZE/2), (ICON_SIZE*7) ,game_screen_width-(ICON_SIZE/2) , (ICON_SIZE*8) ,mx,my) == true)
				{
					selecting_relation_type = SPAWN_POINT_SELECT_RELATION_PARENT;
					spawn_point_editing_mode = SPAWN_POINT_CHOOSE_RELATIVE;
				}

				if (MATH_rectangle_to_point_intersect ( editor_view_width+(ICON_SIZE/2), (ICON_SIZE*8) ,game_screen_width-(ICON_SIZE/2) , (ICON_SIZE*9) ,mx,my) == true)
				{
					selecting_relation_type = SPAWN_POINT_SELECT_RELATION_NEXT_SIBLING;
					spawn_point_editing_mode = SPAWN_POINT_CHOOSE_RELATIVE;
				}

				if (MATH_rectangle_to_point_intersect ( editor_view_width+(ICON_SIZE/2), (ICON_SIZE*10) ,editor_view_width+(ICON_SIZE/2)+(ICON_SIZE*5)-1 , (ICON_SIZE*11) ,mx,my) == true)
				{
					relative = (mx-(editor_view_width+(ICON_SIZE/2)))/ICON_SIZE;
					selected_spawn_point = SPAWNPOINTS_copy_spawn_point (tilemap_number,selected_spawn_point,relative, tilesize);
					SPAWNPOINTS_deselect_spawn_points(tilemap_number);
					SPAWNPOINTS_select_individual_spawn_point (tilemap_number,selected_spawn_point);
					selected_spawn_point = SPAWNPOINTS_lock_spawn_point_status(tilemap_number);
					SPAWNPOINTS_check_for_duplicate_relative_links (tilemap_number, selected_spawn_point);
					SPAWNPOINTS_relink_all_spawn_point_relatives (tilemap_number);
					SPAWNPOINTS_confirm_spawn_point_links (tilemap_number, selected_spawn_point);
				}

				if (MATH_rectangle_to_point_intersect ( editor_view_width+(ICON_SIZE/2)+(ICON_SIZE*6), (ICON_SIZE*10) ,editor_view_width+(ICON_SIZE/2)+(ICON_SIZE*7) , (ICON_SIZE*11) ,mx,my) == true)
				{
					SPAWNPOINTS_delete_particular_spawn_point (tilemap_number,selected_spawn_point);
					selected_spawn_point = UNSET;
					SPAWNPOINTS_relink_all_spawn_point_relatives (tilemap_number);
				}

				if (MATH_rectangle_to_point_intersect( 0, editor_view_height+(ICON_SIZE*3) ,box_size-1 ,editor_view_height+(ICON_SIZE*4)-1 ,mx,my) == true)
				{
					// Toggle delete mode!

					if (delete_selected==true)
					{
						delete_selected = false;
					}
					else
					{
						delete_selected = true;
					}
				}
			}

			if (override_main_editor == false)
			{
				if ( (selected_spawn_point >= 0) && (CONTROL_key_hit(KEY_DEL) == true) )
				{
					SPAWNPOINTS_delete_particular_spawn_point (tilemap_number,selected_spawn_point);
					selected_spawn_point = UNSET;
					SPAWNPOINTS_relink_all_spawn_point_relatives (tilemap_number);
				}

				if (CONTROL_key_hit(KEY_C) == true)
				{
					selected_spawn_point = SPAWNPOINTS_copy_spawn_point (tilemap_number,selected_spawn_point,0, tilesize);
					SPAWNPOINTS_deselect_spawn_points(tilemap_number);
					SPAWNPOINTS_select_individual_spawn_point (tilemap_number,selected_spawn_point);
					selected_spawn_point = SPAWNPOINTS_lock_spawn_point_status(tilemap_number);
					SPAWNPOINTS_check_for_duplicate_relative_links (tilemap_number, selected_spawn_point);
					SPAWNPOINTS_relink_all_spawn_point_relatives (tilemap_number);
					SPAWNPOINTS_confirm_spawn_point_links (tilemap_number, selected_spawn_point);
					cm[tilemap_number].spawn_point_list_pointer[selected_spawn_point].x = real_mx - (real_mx % grid_size);
					cm[tilemap_number].spawn_point_list_pointer[selected_spawn_point].y = real_my - (real_my % grid_size);
				}

				if (CONTROL_key_hit(KEY_F) == true)
				{
					SPAWNPOINTS_select_all_relatives (tilemap_number);
					selected_spawn_point = SPAWNPOINTS_lock_spawn_point_status(tilemap_number);
				}
			}

		}
		else if (selected_spawn_point == MULTIPLE_NODES_SELECTED)
		{
			if (override_main_editor == false)
			{
				if (CONTROL_key_hit(KEY_C) == true)
				{
					SPAWNPOINTS_copy_group (tilemap_number, real_mx - (real_mx % grid_size), real_my - (real_my % grid_size), tilesize);
				}

				if (CONTROL_key_hit(KEY_F) == true)
				{
					SPAWNPOINTS_select_all_relatives (tilemap_number);
					// This is used as we may have members of several families selected so we can't just branch out from a single one.
				}

				if  (CONTROL_key_hit(KEY_DEL) == true)
				{
					SPAWNPOINTS_delete_multiple_selected_spawn_points (tilemap_number);

					selected_spawn_point = UNSET;
				}
			}
		}




	}
	else if (state == STATE_RESET_BUTTONS)
	{
		SIMPGUI_kill_group (TILEMAP_EDITOR_SUB_GROUP);
	}
	else if (state == STATE_SHUTDOWN)
	{
		override_main_editor = false;
		SIMPGUI_kill_group (TILEMAP_EDITOR_SUB_GROUP);
	}

	return override_main_editor;
}



int SPAWNPOINTS_get_spawn_point_flag (int spawn_point_number)
{
	return (cm[collision_map].spawn_point_list_pointer[spawn_point_number].flag);
}



void SPAWNPOINTS_set_spawn_point_flag (int spawn_point_number, int value)
{
	cm[collision_map].spawn_point_list_pointer[spawn_point_number].flag = value;
}



void SPAWNPOINTS_set_spawn_point_flag_by_uid (int spawn_point_map_uid, int spawn_point_uid, int value)
{
	int spawn_point_count;
	int spawn_point_number;

	int spawn_point_map = TILEMAPS_get_map_from_uid (spawn_point_map_uid);

	if (spawn_point_map != UNSET)
	{
		spawn_point_count = cm[spawn_point_map].spawn_points;

		for (spawn_point_number=0; spawn_point_number<spawn_point_count; spawn_point_number++)
		{
			if (cm[spawn_point_map].spawn_point_list_pointer[spawn_point_number].uid == spawn_point_uid)
			{
				cm[spawn_point_map].spawn_point_list_pointer[spawn_point_number].flag = value;
			}
		}
	}
}



void SPAWNPOINTS_output_altered_flags_to_file (char *filename)
{
	// Find any spawn point flag in any map which is non-zero and
	// write a line of text giving the map number, the spawn point's
	// uid and the value.

	int tilemap_number;
	int spawnpoint_number;

	int uid;
	int flag;
	int map_uid;

	char line[MAX_LINE_SIZE];
	char normalised_filename[MAX_LINE_SIZE];

	snprintf(normalised_filename, sizeof(normalised_filename), "%s", filename);
	STRING_lowercase(normalised_filename);

	FILE *file_pointer = fopen (MAIN_get_project_filename (normalised_filename, true),"a");

	if (file_pointer != NULL)
	{
		fputs("#START_OF_SPAWN_POINT_FLAG_DATA\n",file_pointer);

		for (tilemap_number=0; tilemap_number<number_of_tilemaps_loaded; tilemap_number++)
		{
			map_uid = cm[tilemap_number].uid;

			for (spawnpoint_number=0; spawnpoint_number<cm[tilemap_number].spawn_points; spawnpoint_number++)
			{
				if (cm[tilemap_number].spawn_point_list_pointer[spawnpoint_number].flag != 0)
				{
					uid = cm[tilemap_number].spawn_point_list_pointer[spawnpoint_number].uid;
					flag = cm[tilemap_number].spawn_point_list_pointer[spawnpoint_number].flag;

					snprintf (line, sizeof(line), "\t#SPAWN_POINT_MAP_UID = %i SPAWN_POINT_UID = %i SPAWN_POINT_FLAG_VALUE = %i\n",map_uid,uid,flag);

					fputs (line,file_pointer);
				}
			}
		}

		fputs("#END_OF_DATA\n\n",file_pointer);

		fclose(file_pointer);
	}
	else
	{
		OUTPUT_message("Cannot append Spawn Point Flags to save file!");
		assert(0);
	}
}



void SPAWNPOINTS_input_flags_from_file (char *filename)
{
	int map_uid,spawn_point_uid,flag_value;
	char line[MAX_LINE_SIZE];
	char word[MAX_LINE_SIZE];
	char *pointer;
	bool exit_loop = false;
	bool can_exit = false;

	FILE *file_pointer = FILE_open_project_read_case_fallback(filename);
	
	if (file_pointer != NULL)
	{
		while ( ( fgets ( line , MAX_LINE_SIZE , file_pointer ) != NULL ) && (exit_loop == false) )
		{
			STRING_strip_all_disposable (line);

			if (strcmp(line,"#START_OF_SPAWN_POINT_FLAG_DATA") == 0)
			{
				// We've found the flag data start!
				can_exit = true;
			}

			if (can_exit)
			{
				// We've gotten to the start of the flag data, so lets start looking for stuff! WOOOO!

				pointer = STRING_end_of_string(line,"#SPAWN_POINT_MAP_UID = ");
				if (pointer != NULL)
				{
					strcpy(word,pointer);
					strtok(word," ");
					map_uid = atoi(word);

					pointer = STRING_end_of_string(pointer,"SPAWN_POINT_UID = ");
					if (pointer != NULL)
					{
						strcpy(word,pointer);
						strtok(word," ");
						spawn_point_uid = atoi(word);
						
						pointer = STRING_end_of_string(pointer,"SPAWN_POINT_FLAG_VALUE = ");
						if (pointer != NULL)
						{
							strcpy(word,pointer);
							strtok(word," ");
							flag_value = atoi(word);
							
							SPAWNPOINTS_set_spawn_point_flag_by_uid(map_uid,spawn_point_uid,flag_value);
						}
						else
						{
							// WTF!

							OUTPUT_message("No value for flag!");
							assert(0);
						}
					}
					else
					{
						// WTF!

						OUTPUT_message("No uid for flag!");
						assert(0);
					}
				}

				if (strcmp(line,"#END_OF_DATA") == 0)
				{
					// We've found the flag data start!
					exit_loop = true;
				}

			}

		}

		fclose(file_pointer);
	}
	else
	{
		OUTPUT_message("Flag reading routine cannot read from Save File!");
		assert(0);
	}
}
