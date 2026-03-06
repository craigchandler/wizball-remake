#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <math.h>

#include "ai_zones.h"
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

#include "fortify.h"



void AIZONES_destroy_all_gates_and_remotes (int tilemap_number)
{
	if (cm[tilemap_number].connection_list_pointer != NULL)
	{
		free (cm[tilemap_number].connection_list_pointer);
		cm[tilemap_number].connection_list_pointer = NULL;
		cm[tilemap_number].remotes = 0;
	}

	if (cm[tilemap_number].gate_list_pointer != NULL)
	{
		free (cm[tilemap_number].gate_list_pointer);
		cm[tilemap_number].gate_list_pointer = NULL;
		cm[tilemap_number].gates = 0;
	}
}





void AIZONES_get_group_colour (int group_id , int *red, int *green, int *blue )
{
	// Just assigns a colour based on the id passed in so groups show up differently.

	int saturation = 255;
	int hue = (group_id * 30) % 360;
//	int value = ((group_id%12)+1) * 80;
	int value = 255;

	MATH_convert_hsv_to_rgb ( hue , saturation, value , red, green, blue );

}



int AIZONES_find_nearest_gate ( int tilemap_number, int sx, int sy , int layer , int equivalent_tilesize , int mx, int my )
{

	int nearest_gate = UNSET;
	int nearest_gate_distance = UNSET;
	int distance;

	int gate_x1;
	int gate_y1;
	int gate_x2;
	int gate_y2;

	int g;
	
	for (g=0; g<cm[tilemap_number].gates; g++)
	{
		gate_x1 = cm[tilemap_number].gate_list_pointer[g].x1;
		gate_y1 = cm[tilemap_number].gate_list_pointer[g].y1;
		gate_x2 = cm[tilemap_number].gate_list_pointer[g].x2;
		gate_y2 = cm[tilemap_number].gate_list_pointer[g].y2;

		distance = MATH_get_distance_int( (((gate_x1+gate_x2)/2)+1)*equivalent_tilesize , (((gate_y1+gate_y2)/2)+1)*equivalent_tilesize , mx , my );

		if ( (distance < nearest_gate_distance) || (nearest_gate_distance == UNSET) )
		{
			nearest_gate_distance = distance;
			nearest_gate = g;
		}
	}
	
	return nearest_gate;
}



int AIZONES_find_nearest_remote ( int tilemap_number, int sx, int sy , int layer , int equivalent_tilesize , int mx, int my )
{

	int nearest_remote = UNSET;
	int nearest_remote_distance = UNSET;
	int distance;

	int remote_x1;
	int remote_y1;
	int remote_x2;
	int remote_y2;

	int r;
	
	for (r=0; r<cm[tilemap_number].remotes; r++)
	{
		remote_x1 = cm[tilemap_number].connection_list_pointer[r].x1;
		remote_y1 = cm[tilemap_number].connection_list_pointer[r].y1;
		remote_x2 = cm[tilemap_number].connection_list_pointer[r].x2;
		remote_y2 = cm[tilemap_number].connection_list_pointer[r].y2;

		distance = MATH_get_distance_int( (remote_x1+1)*equivalent_tilesize , (remote_y1+1)*equivalent_tilesize , mx , my );

		if ( (distance < nearest_remote_distance) || (nearest_remote_distance == UNSET) )
		{
			nearest_remote_distance = distance;
			nearest_remote = r;
		}

		distance = MATH_get_distance_int( (remote_x2+1)*equivalent_tilesize , (remote_y2+1)*equivalent_tilesize , mx , my );

		if (distance < nearest_remote_distance)
		{
			nearest_remote_distance = distance;
			nearest_remote = r;
		}
	}

	return nearest_remote;
}



void AIZONES_destroy_all_ai_zones (int tilemap_number)
{
	if (cm[tilemap_number].ai_zone_list_pointer != NULL)
	{
		free (cm[tilemap_number].ai_zone_list_pointer);
		cm[tilemap_number].ai_zone_list_pointer = NULL;
		cm[tilemap_number].ai_zones = 0;
	}

}



void AIZONES_swap_ai_zones (int tilemap_number, int ai_zone_1, int ai_zone_2)
{
	ai_zone temp;

	temp = cm[tilemap_number].ai_zone_list_pointer[ai_zone_1];
	cm[tilemap_number].ai_zone_list_pointer[ai_zone_1] = cm[tilemap_number].ai_zone_list_pointer[ai_zone_2];
	cm[tilemap_number].ai_zone_list_pointer[ai_zone_2] = temp;
}



void AIZONES_delete_particular_ai_zone (int tilemap_number , int ai_zone_number)
{
	// This deletes the given zone by first bubbling it to the end of the queue and then
	// destroying it.

	int total;
	int swap_counter;
	
	total = cm[tilemap_number].ai_zones;

	// Okay, to bubble the given one up to the surface we need to swap all the elements
	// from zone_number to total-2 with the one after them.

	for (swap_counter = ai_zone_number ; swap_counter<total-1 ; swap_counter++)
	{
		AIZONES_swap_ai_zones (tilemap_number , swap_counter , swap_counter+1 );
	}

	// Then we realloc the memory used by the zone list to cut off the last zone.
	// We don't need to free it up first as it doesn't contain any pointers or malloc'd space itself.

	cm[tilemap_number].ai_zone_list_pointer = (ai_zone *) realloc ( cm[tilemap_number].ai_zone_list_pointer , (total-1) * sizeof (ai_zone) );

	cm[tilemap_number].ai_zones--;
}



void AIZONES_destroy_ai_zone_group ( int tilemap_number , int layer , int tx , int ty )
{
	// This checks to see if a group is present at the location specified
	// and if it is, it erases all it's members.

	int z;

	int zone_x;
	int zone_y;
	int zone_width;
	int zone_height;

	int group_id;

	for (z=0; z<cm[tilemap_number].ai_zones; z++)
	{
		zone_x = cm[tilemap_number].ai_zone_list_pointer[z].x;
		zone_y = cm[tilemap_number].ai_zone_list_pointer[z].y;
		zone_width = cm[tilemap_number].ai_zone_list_pointer[z].width;
		zone_height = cm[tilemap_number].ai_zone_list_pointer[z].height;

		if (MATH_rectangle_to_point_intersect_by_size (zone_x,zone_y,zone_width,zone_height,tx,ty) == true)
		{
			group_id = cm[tilemap_number].ai_zone_list_pointer[z].unique_group_id;
		}
	}

	for (z=0; z<cm[tilemap_number].ai_zones; z++)
	{
		if (group_id == cm[tilemap_number].ai_zone_list_pointer[z].unique_group_id)
		{
			AIZONES_delete_particular_ai_zone (tilemap_number, z);
			z--; // Because we just swapped another one into this one's place so we need a do-over.
		}
	}

}



int AIZONES_get_spare_ai_zone_id ( int tilemap_number )
{
	int tileset;

	tileset = cm[tilemap_number].tile_set_index;

	if (tileset == UNSET)
	{
		return UNSET; // We can't do anything until we have a tileset.
	}

	int number;
	bool increased;

	int z;
	number = 0;

	do
	{
		increased = false;

		for (z=0; z<cm[tilemap_number].ai_zones; z++)
		{
			if (cm[tilemap_number].ai_zone_list_pointer[z].unique_group_id == number)
			{
				number++;
				increased = true;
			}
		}

		for (z=cm[tilemap_number].ai_zones-1; z>=0 ; z--)
		{
			if (cm[tilemap_number].ai_zone_list_pointer[z].unique_group_id == number)
			{
				number++;
				increased = true;
			}
		}

		// Going back and forth should increase the number to a level where it's unique faster.

	} while(increased == true);

	return number;
}



bool AIZONES_check_across_gates (int tilemap_number, int x1, int y1, int x2, int y2)
{
	// This goes through all the gates to see if the given tile locations would
	// cross over the gate. Oy vey...

	int g;

	int gate_x1;
	int gate_y1;
	int gate_x2;
	int gate_y2;

	int gate_diff_x;
	int gate_diff_y;

	int tile_diff_x;
	int tile_diff_y;

	tile_diff_x = x2-x1;
	tile_diff_y = y2-y1;

	for (g=0; g<cm[tilemap_number].gates; g++)
	{
		gate_x1 = cm[tilemap_number].gate_list_pointer[g].x1;
		gate_y1 = cm[tilemap_number].gate_list_pointer[g].y1;
		gate_x2 = cm[tilemap_number].gate_list_pointer[g].x2;
		gate_y2 = cm[tilemap_number].gate_list_pointer[g].y2;

		gate_diff_x = gate_x2-gate_x1;
		gate_diff_y = gate_y2-gate_y1;

		if ( ( (gate_diff_x != 0) && (tile_diff_x != 0) ) || ( (gate_diff_y != 0) && (tile_diff_y != 0) ) )
		{
			// If both the gate and the tile arrangement is horizontal or vertical then they are parallel
			// and therefor can't be crossing over each other. Woot, as they say.
		}
		else
		{
			if (gate_diff_x != 0)
			{
				// It's a horizontal gate. All we need to do is check whether x1 or x2 (both are equal) falls outside
				// gate_x1 and gate_x2 and if it isn't we can ignore!

				if ( (x1 < gate_x1) || (x1 >= gate_x2) )
				{
					// Yay! Nothin' doing!
				}
				else
				{
					// Bah! We passed the check!

					if ( (y1 == gate_y1-1) && (y2 == gate_y1) )
					{
						// Well, it's either side.
						return true;
					}
				}

			}
			else
			{
				// It's vertical.

				if ( (y1 < gate_y1) || (y1 >= gate_y2) )
				{
					// Yay! Nothin' doing!
				}
				else
				{
					// Bah! We passed the check!

					if ( (x1 == gate_x1-1) && (x2 == gate_x1) )
					{
						// Well, it's either side.
						return true;
					}
				}

			}
		}
	}

	return false;

}



bool AIZONES_check_workspace_rect (int *workspace, int workspace_width, int workspace_height, int sx, int sy, int width, int height)
{
	int x,y;

	for (y=sy; y<sy+height; y++)
	{
		for (x=sx; x<sx+width; x++)
		{
			if (workspace[(y*workspace_width)+x] != UNPROPPED_COLLISION_TILE)
			{
				return false;
			}
		}
	}

	return true;
}



void AIZONES_blank_workspace_rect (int *workspace, int workspace_width, int workspace_height, int sx, int sy, int width, int height)
{
	int x,y;

	for (x=sx; x<sx+width; x++)
	{
		for (y=sy; y<sy+height; y++)
		{
			workspace[(y*workspace_width)+x] = EMPTY_COLLISION_TILE;
		}
	}
}



void AIZONES_create_ai_zone (int tilemap_number, int x, int y, int width, int height, int zone_id)
{
	int total;

	total = cm[tilemap_number].ai_zones;

	if (cm[tilemap_number].ai_zone_list_pointer == NULL)
	{
		// None created yet.

		cm[tilemap_number].ai_zone_list_pointer = (ai_zone *) malloc ( sizeof (ai_zone) );
	}
	else
	{
		cm[tilemap_number].ai_zone_list_pointer = (ai_zone *) realloc ( cm[tilemap_number].ai_zone_list_pointer , (total+1) * sizeof (ai_zone) );
	}

	cm[tilemap_number].ai_zone_list_pointer[total].x = x;
	cm[tilemap_number].ai_zone_list_pointer[total].y = y;

	cm[tilemap_number].ai_zone_list_pointer[total].width = width;
	cm[tilemap_number].ai_zone_list_pointer[total].height = height;

	cm[tilemap_number].ai_zone_list_pointer[total].unique_group_id = zone_id;

	cm[tilemap_number].ai_zones++;
}



void AIZONES_fill_ai_zone_space (int tilemap_number, int *workspace, int start_x, int start_y, int width, int height , int zone_id , bool wraparound_horizontal, bool wraparound_vertical )
{
	int tx;
	int ty;

	int zone_x;
	int zone_y;
	int zone_width;
	int zone_height;

	int list_size;
	int reading_from;

	bool expanded;

	int t;

	fill_spreader *list_pointer = NULL;

	list_size = 0;
	reading_from = 0;

	list_pointer = EDIT_add_filler (list_size, list_pointer, start_x, start_y);
	workspace [(start_y * width) + start_x] = UNPROPPED_COLLISION_TILE;
	list_size++;

	do {
		tx = list_pointer[reading_from].x;
		ty = list_pointer[reading_from].y;

		if (tx > 0)
		{
			if (workspace [ ( (ty) * width) + (tx-1) ] == EMPTY_COLLISION_TILE)
			{
				if ( AIZONES_check_across_gates (tilemap_number, tx-1, ty, tx, ty) == false )
				{
					list_pointer = EDIT_add_filler ( list_size, list_pointer, tx-1, ty );
					workspace [ ( (ty) * width) + (tx-1) ] = UNPROPPED_COLLISION_TILE;
					list_size++;
				}
			}
		}
		else if (wraparound_horizontal == true)
		{
			if (workspace [ ( (ty) * width) + (width-1) ] == EMPTY_COLLISION_TILE)
			{
				list_pointer = EDIT_add_filler ( list_size, list_pointer, (width-1), ty );
				workspace [ ( (ty) * width) + (width-1) ] = UNPROPPED_COLLISION_TILE;
				list_size++;
			}
		}

		if (tx < width-1)
		{
			if (workspace [ ( (ty) * width) + (tx+1) ] == EMPTY_COLLISION_TILE)
			{
				if ( AIZONES_check_across_gates (tilemap_number, tx, ty, tx+1, ty) == false )
				{

					list_pointer = EDIT_add_filler ( list_size, list_pointer, tx+1, ty );
					workspace [ ( (ty) * width) + (tx+1) ] = UNPROPPED_COLLISION_TILE;
					list_size++;
				}
			}
		}
		else if (wraparound_horizontal == true)
		{
			if (workspace [ ( (ty) * width) + (0) ] == EMPTY_COLLISION_TILE)
			{
				list_pointer = EDIT_add_filler ( list_size, list_pointer, (0), ty );
				workspace [ ( (ty) * width) + (0) ] = UNPROPPED_COLLISION_TILE;
				list_size++;
			}
		}

		if (ty > 0)
		{
			if (workspace [ ( (ty-1) * width) + (tx) ] == EMPTY_COLLISION_TILE)
			{
				if ( AIZONES_check_across_gates (tilemap_number, tx, ty-1, tx, ty) == false )
				{

					list_pointer = EDIT_add_filler ( list_size, list_pointer, tx, ty-1 );
					workspace [ ( (ty-1) * width) + (tx) ] = UNPROPPED_COLLISION_TILE;
					list_size++;
				}
			}
		}
		else if (wraparound_vertical == true)
		{
			if (workspace [ ( (height-1) * width) + (tx) ] == EMPTY_COLLISION_TILE)
			{
				list_pointer = EDIT_add_filler ( list_size, list_pointer, tx, (height-1) );
				workspace [ ( (height-1) * width) + (tx) ] = UNPROPPED_COLLISION_TILE;
				list_size++;
			}
		}

		if (ty < height-1)
		{
			if (workspace [ ( (ty+1) * width) + (tx) ] == EMPTY_COLLISION_TILE)
			{
				if ( AIZONES_check_across_gates (tilemap_number, tx, ty, tx, ty+1) == false )
				{
					list_pointer = EDIT_add_filler ( list_size, list_pointer, tx, ty+1 );
					workspace [ ( (ty+1) * width) + (tx) ] = UNPROPPED_COLLISION_TILE;
					list_size++;
				}
			}
		}
		else if (wraparound_vertical == true)
		{
			if (workspace [ ( (0) * width) + (tx) ] == EMPTY_COLLISION_TILE)
			{
				list_pointer = EDIT_add_filler ( list_size, list_pointer, tx, (0) );
				workspace [ ( (0) * width) + (tx) ] = UNPROPPED_COLLISION_TILE;
				list_size++;
			}
		}

		// After checking the 4 directions we go through all the remote connections.

		for (t=0; t<cm[tilemap_number].remotes; t++)
		{
			// If either of the two co-ords match the current tx,ty then expand into the remote area.
			if ( (cm[tilemap_number].connection_list_pointer[t].x1 == tx) && (cm[tilemap_number].connection_list_pointer[t].y1 == ty) )
			{
				if (workspace [ ( (cm[tilemap_number].connection_list_pointer[t].y2) * width) + (cm[tilemap_number].connection_list_pointer[t].x2) ] == EMPTY_COLLISION_TILE)
				{
					list_pointer = EDIT_add_filler ( list_size, list_pointer, cm[tilemap_number].connection_list_pointer[t].x2, cm[tilemap_number].connection_list_pointer[t].y2 );
					workspace [ ( (cm[tilemap_number].connection_list_pointer[t].y2) * width) + (cm[tilemap_number].connection_list_pointer[t].x2) ] = UNPROPPED_COLLISION_TILE;
					list_size++;
				}
			}

			if ( (cm[tilemap_number].connection_list_pointer[t].x2 == tx) && (cm[tilemap_number].connection_list_pointer[t].y2 == ty) )
			{
				if (workspace [ ( (cm[tilemap_number].connection_list_pointer[t].y1) * width) + (cm[tilemap_number].connection_list_pointer[t].x1) ] == EMPTY_COLLISION_TILE)
				{
					list_pointer = EDIT_add_filler ( list_size, list_pointer, cm[tilemap_number].connection_list_pointer[t].x1, cm[tilemap_number].connection_list_pointer[t].y1 );
					workspace [ ( (cm[tilemap_number].connection_list_pointer[t].y1) * width) + (cm[tilemap_number].connection_list_pointer[t].x1) ] = UNPROPPED_COLLISION_TILE;
					list_size++;
				}
			}

		}

		reading_from++;
		
	} while(reading_from < list_size);
	
	// We can get rid of the filler list.

	free (list_pointer);
	list_pointer = NULL; // Because I'm anal...

	// Okay now we've propagated the list we need to grow the zones from start point;

	for (zone_y=0; zone_y<height; zone_y++)
	{
		for (zone_x=0; zone_x<width; zone_x++)
		{

			if (workspace[(zone_y * width) + zone_x] == UNPROPPED_COLLISION_TILE)
			{
				zone_width = 1;
				zone_height = 1;

				do
				{
					expanded = false;

					if (zone_width+zone_x < width)
					{
						if (AIZONES_check_workspace_rect (workspace, width, height, zone_x, zone_y, zone_width+1, zone_height) == true)
						{
							zone_width++;
							expanded = true;
						}
					}

					if (zone_height+zone_y < height)
					{
						if (AIZONES_check_workspace_rect (workspace, width, height, zone_x, zone_y, zone_width, zone_height+1) == true)
						{
							zone_height++;
							expanded = true;
						}
					}
				}
				while (expanded == true);

				AIZONES_blank_workspace_rect (workspace, width, height, zone_x, zone_y, zone_width, zone_height);

				AIZONES_create_ai_zone (tilemap_number,zone_x,zone_y,zone_width,zone_height,zone_id);

			}
		}
	}

}



void AIZONES_propogate_ai_zones ( int tilemap_number , int layer , int start_x , int start_y , bool wraparound_horizontal, bool wraparound_vertical )
{
	// Okay, well first things first we need to create a workspace of the same size
	// as the map layers.

	int tileset;

	tileset = cm[tilemap_number].tile_set_index;

	if (tileset == UNSET)
	{
		return; // We can't do anything until we have a tileset.
	}

	int *workspace = NULL;

	int width = cm[tilemap_number].map_width;
	int height = cm[tilemap_number].map_height;

	workspace = (int *) malloc ( width * height * sizeof(int) );

	if (workspace == NULL)
	{
		assert(0);
	}

	int x,y;
	int tile;
	int zone_id;

	// Set up workspace so that blank spaces are 0's and walls are 1's.

	for (x=0; x<width; x++)
	{
		for (y=0; y<height; y++)
		{
			tile = TILEMAPS_get_tile(tilemap_number,layer,x,y);

			if (TILESETS_is_tile_solid (tileset,tile) == true)
			{
				workspace[(y*width) + x] = SOLID_COLLISION_TILE;
			}
			else
			{
				workspace[(y*width) + x] = EMPTY_COLLISION_TILE;
			}
		}
	}

	zone_id = AIZONES_get_spare_ai_zone_id ( tilemap_number );

	AIZONES_fill_ai_zone_space (tilemap_number, workspace, start_x, start_y, width, height , zone_id , wraparound_horizontal, wraparound_vertical );

	free (workspace);

}



void AIZONES_create_all_ai_zones ( int tilemap_number , int layer , bool wraparound_horizontal , bool wraparound_vertical )
{
	// This will create a dummy collision map and then allocate groups to it until it's all used up.

	int tileset;

	tileset = cm[tilemap_number].tile_set_index;

	if (tileset == UNSET)
	{
		return; // We can't do anything until we have a tileset.
	}

	// Okay, well first things first we need to create a workspace of the same size
	// as the map layers.

	int *global_workspace = NULL;

	int width = cm[tilemap_number].map_width;
	int height = cm[tilemap_number].map_height;

	global_workspace = (int *) malloc ( width * height * sizeof(int) );

	if (global_workspace == NULL)
	{
		assert(0);
	}

	// Then we get rid of all the existing AI zones.

	AIZONES_destroy_all_ai_zones (tilemap_number);

	int x,y;
	int tile;

	// Set up workspace so that blank spaces are 0's and walls are 1's.

	for (x=0; x<width; x++)
	{
		for (y=0; y<height; y++)
		{
			tile = TILEMAPS_get_tile(tilemap_number,layer,x,y);

			if (TILESETS_is_tile_solid (tileset,tile) == true)
			{
				global_workspace[(y*width) + x] = SOLID_COLLISION_TILE;
			}
			else
			{
				global_workspace[(y*width) + x] = UNPROPPED_COLLISION_TILE;
			}
		}
	}

	int z;
	int zone_x;
	int zone_y;
	int zone_width;
	int zone_height;

	// Now we loop through it until we find a blank space and then we create a group there.
	// After creating the group we then fill all the zones in 

	for (x=0; x<width; x++)
	{
		for (y=0; y<height; y++)
		{
			if (global_workspace[(y*width) + x] == UNPROPPED_COLLISION_TILE)
			{
				AIZONES_propogate_ai_zones ( tilemap_number , layer , x , y , wraparound_horizontal , wraparound_vertical );
					
				for (z=0; z<cm[tilemap_number].ai_zones; z++)
				{
					zone_x = cm[tilemap_number].ai_zone_list_pointer[z].x;
					zone_y = cm[tilemap_number].ai_zone_list_pointer[z].y;
					zone_width = cm[tilemap_number].ai_zone_list_pointer[z].width;
					zone_height = cm[tilemap_number].ai_zone_list_pointer[z].height;

					AIZONES_blank_workspace_rect (global_workspace, width, height, zone_x, zone_y, zone_width, zone_height);
				}
			}
		}
	}		

	free (global_workspace);

}



#define SCALAR_VALUE			(4)

void AIZONES_check_for_leaked_gates_and_double_neighbours (int tilemap_number)
{
	// This simply goes through each gate and then sees what two IDs are on either side of them
	// and then stores this information.

	// After that it goes through and if it finds a matching pair then it's considered a leaky
	// gate and if it finds more than 1 with the same pairing then it considers those as
	// double-neighbours, which'll screw up the pathfinding something chronic later on.

	// Also it looks for instances where a gate is touching more than one rectangle on either
	// side and flags those as too_many_rects.

	// For these checks to work we need to give the rectangles a bit of "body" and so that's
	// why all the co-ordinates are multiplied by SCALAR_VALUE, then we can check INSIDE the
	// rects rather than on their edges which may well be shared.

	int g;
	int z;

	int g1,g2;

	int g1n1;
	int g1n2;
	int g2n1;
	int g2n2;

	int s1n1;
	int s1n2;
	int s2n1;
	int s2n2;

	int side_1_px1;
	int side_1_py1;

	int side_2_px1;
	int side_2_py1;

	int side_1_px2;
	int side_1_py2;

	int side_2_px2;
	int side_2_py2;

	int zone_x;
	int zone_y;
	int zone_width;
	int zone_height;

	for (g=0; g<cm[tilemap_number].gates; g++)
	{
		cm[tilemap_number].gate_list_pointer[g].neighbour1 = UNSET;
		cm[tilemap_number].gate_list_pointer[g].neighbour2 = UNSET;

		cm[tilemap_number].gate_list_pointer[g].side_1_p1_neighbour = UNSET;
		cm[tilemap_number].gate_list_pointer[g].side_2_p1_neighbour = UNSET;
		cm[tilemap_number].gate_list_pointer[g].side_1_p2_neighbour = UNSET;
		cm[tilemap_number].gate_list_pointer[g].side_2_p2_neighbour = UNSET;

		if (cm[tilemap_number].gate_list_pointer[g].x1 == cm[tilemap_number].gate_list_pointer[g].x2)
		{
			// Vertical gate!
			side_1_px1 = (cm[tilemap_number].gate_list_pointer[g].x1)*SCALAR_VALUE-1;
			side_1_py1 = (cm[tilemap_number].gate_list_pointer[g].y1)*SCALAR_VALUE+1;

			side_2_px1 = (cm[tilemap_number].gate_list_pointer[g].x1)*SCALAR_VALUE+1;
			side_2_py1 = side_1_py1;

			side_1_px2 = side_1_px1;
			side_1_py2 = (cm[tilemap_number].gate_list_pointer[g].y2)*SCALAR_VALUE-1;

			side_2_px2 = side_2_px1;
			side_2_py2 = side_1_py2;

		}
		else
		{
			// Horizontal gate!

			side_1_px1 = (cm[tilemap_number].gate_list_pointer[g].x1)*SCALAR_VALUE+1;
			side_1_py1 = (cm[tilemap_number].gate_list_pointer[g].y1)*SCALAR_VALUE-1;

			side_2_px1 = side_1_px1;
			side_2_py1 = (cm[tilemap_number].gate_list_pointer[g].y1)*SCALAR_VALUE+1;
			
			side_1_px2 = (cm[tilemap_number].gate_list_pointer[g].x2)*SCALAR_VALUE-1;
			side_1_py2 = side_1_py1;

			side_2_px2 = side_1_px2;
			side_2_py2 = side_2_py1;

		}

		for (z=0; z<cm[tilemap_number].ai_zones; z++)
		{
			zone_x = (cm[tilemap_number].ai_zone_list_pointer[z].x)*SCALAR_VALUE;
			zone_y = (cm[tilemap_number].ai_zone_list_pointer[z].y)*SCALAR_VALUE;
			zone_width = (cm[tilemap_number].ai_zone_list_pointer[z].width)*SCALAR_VALUE;
			zone_height = (cm[tilemap_number].ai_zone_list_pointer[z].height)*SCALAR_VALUE;

			// Find neighbouring unique ids.

			if (MATH_rectangle_to_point_intersect_by_size (zone_x,zone_y,zone_width,zone_height , side_1_px1,side_1_py1) == true)
			{
				cm[tilemap_number].gate_list_pointer[g].neighbour1 = cm[tilemap_number].ai_zone_list_pointer[z].unique_group_id;
			}

			if (MATH_rectangle_to_point_intersect_by_size (zone_x,zone_y,zone_width,zone_height , side_2_px1,side_2_py1) == true)
			{
				cm[tilemap_number].gate_list_pointer[g].neighbour2 = cm[tilemap_number].ai_zone_list_pointer[z].unique_group_id;
			}

			// Find actual neighbouring zones...

			if (MATH_rectangle_to_point_intersect_by_size (zone_x,zone_y,zone_width,zone_height , side_1_px1,side_1_py1) == true)
			{
				cm[tilemap_number].gate_list_pointer[g].side_1_p1_neighbour = z;
			}

			if (MATH_rectangle_to_point_intersect_by_size (zone_x,zone_y,zone_width,zone_height , side_2_px1,side_2_py1) == true)
			{
				cm[tilemap_number].gate_list_pointer[g].side_2_p1_neighbour = z;
			}

			if (MATH_rectangle_to_point_intersect_by_size (zone_x,zone_y,zone_width,zone_height , side_1_px2,side_1_py2) == true)
			{
				cm[tilemap_number].gate_list_pointer[g].side_1_p2_neighbour = z;
			}
			
			if (MATH_rectangle_to_point_intersect_by_size (zone_x,zone_y,zone_width,zone_height , side_2_px2,side_2_py2) == true)
			{
				cm[tilemap_number].gate_list_pointer[g].side_2_p2_neighbour = z;
			}

		}
	}

	// Check for leaks only! But may as well reset the double_neighbours and too_many_rects while we're at it.

	for (g=0; g<cm[tilemap_number].gates; g++)
	{
		if ( (cm[tilemap_number].gate_list_pointer[g].neighbour1 == cm[tilemap_number].gate_list_pointer[g].neighbour2) && (cm[tilemap_number].gate_list_pointer[g].neighbour1 != UNSET) )
		{
			cm[tilemap_number].gate_list_pointer[g].leaked = true;
		}
		else
		{
			cm[tilemap_number].gate_list_pointer[g].leaked = false;
		}

		cm[tilemap_number].gate_list_pointer[g].double_neighbours = false;
		cm[tilemap_number].gate_list_pointer[g].too_many_rects = false;
	}

	// Check for double_neighbours!

	for (g1=0; g1<cm[tilemap_number].gates; g1++)
	{
		for (g2=0; g2<cm[tilemap_number].gates; g2++)
		{
			if (g1!=g2)
			{
				g1n1 = cm[tilemap_number].gate_list_pointer[g1].neighbour1;
				g1n2 = cm[tilemap_number].gate_list_pointer[g1].neighbour2;
				g2n1 = cm[tilemap_number].gate_list_pointer[g2].neighbour1;
				g2n2 = cm[tilemap_number].gate_list_pointer[g2].neighbour2;

				if ( ( (g1n1 == g2n1) && (g1n2 == g2n2) ) || ( (g1n2 == g2n1) && (g1n1 == g2n2) ) )
				{
					if ( (g1n1 != UNSET) && (g1n2 != UNSET) && (g2n1 != UNSET) && (g2n2 != UNSET) )
					{
						cm[tilemap_number].gate_list_pointer[g1].double_neighbours = true;
					}
				}
			}
		}
	}

	// Check for too_many_rects!

	for (g=0; g<cm[tilemap_number].gates; g++)
	{
		s1n1 = cm[tilemap_number].gate_list_pointer[g].side_1_p1_neighbour;
		s1n2 = cm[tilemap_number].gate_list_pointer[g].side_1_p2_neighbour;
		s2n1 = cm[tilemap_number].gate_list_pointer[g].side_2_p1_neighbour;
		s2n2 = cm[tilemap_number].gate_list_pointer[g].side_2_p2_neighbour;

		if ( (s1n1 != UNSET) && (s1n2 != UNSET) )
		{
			// If they both had neighbouring rectangles...
			if (s1n1 != s1n2)
			{
				// Different neighbours means too many rects!
				cm[tilemap_number].gate_list_pointer[g].too_many_rects = true;
			}
		}

		if ( (s2n1 != UNSET) && (s2n2 != UNSET) )
		{
			// If they both had neighbouring rectangles...
			if (s2n1 != s2n2)
			{
				// Different neighbours means too many rects!
				cm[tilemap_number].gate_list_pointer[g].too_many_rects = true;
			}
		}

	}

}



void AIZONES_draw_collision_view ( int tile_display_width, int tile_display_height , int tilemap_number, int sx, int sy , int layer , int equivalent_tilesize , bool wraparound_horizontal, bool wraparound_vertical )
{
	int width;
	int height;

	int tileset;

	tileset = cm[tilemap_number].tile_set_index;

	if (tileset == UNSET)
	{
		return; // We can't do anything until we have a tileset.
	}

	int display_width_in_small_tiles;
	int display_height_in_small_tiles;

	width = cm[tilemap_number].map_width;
	height = cm[tilemap_number].map_height;

	display_width_in_small_tiles = tile_display_width / equivalent_tilesize;
	display_height_in_small_tiles = tile_display_height / equivalent_tilesize;

	int x;
	int y;
	int tile;

	for (x=-1 ; (x<=width)&&(x<display_width_in_small_tiles) ; x++)
	{
		for (y=-1 ; (y<=height)&&(y<display_height_in_small_tiles) ; y++)
		{
			if ( (x>=0) && (y>=0) && (x<width) && (y<height) )
			{
				tile = TILEMAPS_get_tile(tilemap_number,layer,sx+x,sy+y);

				if (TILESETS_is_tile_solid (tileset,tile) == true)
				{
					OUTPUT_filled_rectangle_by_size ( (x+1)*equivalent_tilesize , (y+1)*equivalent_tilesize , equivalent_tilesize-1 , equivalent_tilesize-1 , 128 , 0 , 0 );
				}
			}
			else if ( ( (x<0) || (x==width) ) && (y>=0) && (y<height) ) // It's a side box but not a corner box...
			{
				if (wraparound_horizontal == false)
				{
					OUTPUT_filled_rectangle_by_size ( (x+1)*equivalent_tilesize , (y+1)*equivalent_tilesize , equivalent_tilesize-1 , equivalent_tilesize-1 , 0 , 0 , 128 );
				}
				else
				{
					OUTPUT_rectangle_by_size ( (x+1)*equivalent_tilesize , (y+1)*equivalent_tilesize , equivalent_tilesize-1 , equivalent_tilesize-1 , 0 , 0 , 128 );
				}
			}
			else if ( ( (y<0) || (y==height) ) && (x>=0) && (x<width) ) // It's a top or bottom box but not a corner box...
			{
				if (wraparound_vertical == false)
				{
					OUTPUT_filled_rectangle_by_size ( (x+1)*equivalent_tilesize , (y+1)*equivalent_tilesize , equivalent_tilesize-1 , equivalent_tilesize-1 , 0 , 0 , 128 );
				}
				else
				{
					OUTPUT_rectangle_by_size ( (x+1)*equivalent_tilesize , (y+1)*equivalent_tilesize , equivalent_tilesize-1 , equivalent_tilesize-1 , 0 , 0 , 128 );
				}
			}
			else // It's a corner box...
			{
				if ( (wraparound_vertical == false) || (wraparound_horizontal == false) )
				{
					OUTPUT_filled_rectangle_by_size ( (x+1)*equivalent_tilesize , (y+1)*equivalent_tilesize , equivalent_tilesize-1 , equivalent_tilesize-1 , 0 , 0 , 128 );
				}
				else
				{
					OUTPUT_rectangle_by_size ( (x+1)*equivalent_tilesize , (y+1)*equivalent_tilesize , equivalent_tilesize-1 , equivalent_tilesize-1 , 0 , 0 , 128 );
				}
			}

		}
	}

}



void AIZONES_draw_ai_zones_and_gates ( int tile_display_width, int tile_display_height , int tilemap_number, int sx, int sy , int layer , int equivalent_tilesize , int nearest_gate , int nearest_remote )
{
	int width;
	int height;

	int cycler;
	static int big_cycler = 0;

	static bool flip_flop = false;

	// Purely cosmetic to make dodgy gates strobe obviously...

	big_cycler++;
	if (big_cycler >= equivalent_tilesize*2)
	{
		big_cycler = 0;
	}

	cycler = big_cycler/4;

	// Get tileset as usual...

	int tileset;

	tileset = cm[tilemap_number].tile_set_index;

	if (tileset == UNSET)
	{
		return;; // We can't do anything until we have a tileset.
	}

	if (flip_flop == true)
	{
		flip_flop = false;
	}
	else
	{
		flip_flop = true;
	}

	int display_width_in_small_tiles;
	int display_height_in_small_tiles;

	width = cm[tilemap_number].map_width;
	height = cm[tilemap_number].map_height;

	display_width_in_small_tiles = tile_display_width / equivalent_tilesize;
	display_height_in_small_tiles = tile_display_height / equivalent_tilesize;

	int zone_x;
	int zone_y;
	int zone_width;
	int zone_height;

	int red,green,blue;

	int z;

	for (z=0; z<cm[tilemap_number].ai_zones; z++)
	{
		zone_x = cm[tilemap_number].ai_zone_list_pointer[z].x;
		zone_y = cm[tilemap_number].ai_zone_list_pointer[z].y;
		zone_width = cm[tilemap_number].ai_zone_list_pointer[z].width;
		zone_height = cm[tilemap_number].ai_zone_list_pointer[z].height;

		AIZONES_get_group_colour (cm[tilemap_number].ai_zone_list_pointer[z].unique_group_id , &red, &green, &blue );

		OUTPUT_rectangle_by_size ( (zone_x+1)*equivalent_tilesize , (zone_y+1)*equivalent_tilesize , (zone_width*equivalent_tilesize)-1 , (zone_height*equivalent_tilesize)-1 , red , green , blue );
	}

	int gate_x1;
	int gate_y1;
	int gate_x2;
	int gate_y2;

	int g;
	
	for (g=0; g<cm[tilemap_number].gates; g++)
	{
		gate_x1 = cm[tilemap_number].gate_list_pointer[g].x1;
		gate_y1 = cm[tilemap_number].gate_list_pointer[g].y1;
		gate_x2 = cm[tilemap_number].gate_list_pointer[g].x2;
		gate_y2 = cm[tilemap_number].gate_list_pointer[g].y2;

		if (g == nearest_gate)
		{
			red = 255;
			green = 255;
			blue = 255;
		}
		else if (cm[tilemap_number].gate_list_pointer[g].leaked == true)
		{
			red = 255;
			green = 0;
			blue = 0;
		}
		else if (cm[tilemap_number].gate_list_pointer[g].double_neighbours == true)
		{
			red = 255;
			green = 0;
			blue = 255;
		}
		else if (cm[tilemap_number].gate_list_pointer[g].too_many_rects == true)
		{
			red = 0;
			green = 0;
			blue = 255;
		}
		else
		{
			red = 0;
			green = 255;
			blue = 0;
		}

		if ( (cm[tilemap_number].gate_list_pointer[g].too_many_rects == true) || (cm[tilemap_number].gate_list_pointer[g].double_neighbours == true) || (cm[tilemap_number].gate_list_pointer[g].leaked == true) )
		{
			OUTPUT_rectangle ( ((gate_x1+1)*equivalent_tilesize)-cycler , ((gate_y1+1)*equivalent_tilesize)-cycler , ((gate_x2+1)*equivalent_tilesize)+cycler-1 , ((gate_y2+1)*equivalent_tilesize)+cycler-1 , red , green , blue );
		}
		else
		{
			OUTPUT_filled_rectangle ( ((gate_x1+1)*equivalent_tilesize)-1 , ((gate_y1+1)*equivalent_tilesize)-1 , ((gate_x2+1)*equivalent_tilesize) , ((gate_y2+1)*equivalent_tilesize) , red , green , blue );
		}
	}

	int rc;

	int x1,y1,x2,y2;
	int offset_x,offset_y;
	int direction;

	for (rc=0; rc<cm[tilemap_number].remotes; rc++)
	{
		if (nearest_remote == rc)
		{
			red = 255;
			green = 255;
			blue = 255;
		}
		else
		{
			AIZONES_get_group_colour (rc , &red, &green, &blue );
		}

		x1 = cm[tilemap_number].connection_list_pointer[rc].x1;
		y1 = cm[tilemap_number].connection_list_pointer[rc].y1;
		x2 = cm[tilemap_number].connection_list_pointer[rc].x2;
		y2 = cm[tilemap_number].connection_list_pointer[rc].y2;

		direction = cm[tilemap_number].connection_list_pointer[rc].connective_direction;

		OUTPUT_filled_rectangle_by_size ( ((x1+1)*equivalent_tilesize)+1 , ((y1+1)*equivalent_tilesize)+1 , equivalent_tilesize-2 , equivalent_tilesize-2 , red , green , blue );
		OUTPUT_filled_rectangle_by_size ( ((x2+1)*equivalent_tilesize)+1 , ((y2+1)*equivalent_tilesize)+1 , equivalent_tilesize-2 , equivalent_tilesize-2 , red , green , blue );

		if (direction != UNSET)
		{
			MATH_get_4num_offsets (direction, &offset_x, &offset_y);
			OUTPUT_line ( ((x1+1)*equivalent_tilesize)+(equivalent_tilesize/2) , ((y1+1)*equivalent_tilesize)+(equivalent_tilesize/2) , ((x1+1)*equivalent_tilesize)+(equivalent_tilesize/2) + (offset_x*equivalent_tilesize) , ((y1+1)*equivalent_tilesize)+(equivalent_tilesize/2) + (offset_y*equivalent_tilesize) );
			if ((x1 != x2) || (y1 != y2))
			{
				OUTPUT_line ( ((x2+1)*equivalent_tilesize)+(equivalent_tilesize/2) , ((y2+1)*equivalent_tilesize)+(equivalent_tilesize/2) , ((x2+1)*equivalent_tilesize)+(equivalent_tilesize/2) - (offset_x*equivalent_tilesize) , ((y2+1)*equivalent_tilesize)+(equivalent_tilesize/2) - (offset_y*equivalent_tilesize) );
			}
		}
	}
}



void AIZONES_create_remote_connection (int tilemap_number, int x1, int y1)
{
	int total;

	total = cm[tilemap_number].remotes;

	if (cm[tilemap_number].connection_list_pointer == NULL)
	{
		// None created yet.

		cm[tilemap_number].connection_list_pointer = (remote_connection *) malloc ( sizeof (remote_connection) );
	}
	else
	{
		cm[tilemap_number].connection_list_pointer = (remote_connection *) realloc ( cm[tilemap_number].connection_list_pointer , (total+1) * sizeof (remote_connection) );
	}

	cm[tilemap_number].connection_list_pointer[total].x1 = x1;
	cm[tilemap_number].connection_list_pointer[total].y1 = y1;

	cm[tilemap_number].connection_list_pointer[total].x2 = x1;
	cm[tilemap_number].connection_list_pointer[total].y2 = y1;

	cm[tilemap_number].connection_list_pointer[total].connective_direction = UNSET;
	cm[tilemap_number].connection_list_pointer[total].zone_1 = UNSET;
	cm[tilemap_number].connection_list_pointer[total].zone_2 = UNSET;

	cm[tilemap_number].remotes++;
}



void AIZONES_swap_remote_connections (int tilemap_number, int remote_1, int remote_2)
{
	remote_connection temp;

	temp = cm[tilemap_number].connection_list_pointer[remote_1];
	cm[tilemap_number].connection_list_pointer[remote_1] = cm[tilemap_number].connection_list_pointer[remote_2];
	cm[tilemap_number].connection_list_pointer[remote_2] = temp;
}



void AIZONES_delete_particular_remote_connection (int tilemap_number , int connection_number)
{
	// This deletes the given zone by first bubbling it to the end of the queue and then
	// destroying it.

	int total;
	int swap_counter;
	
	total = cm[tilemap_number].remotes;

	// Okay, to bubble the given one up to the surface we need to swap all the elements
	// from zone_number to total-2 with the one after them.

	for (swap_counter = connection_number ; swap_counter<total-1 ; swap_counter++)
	{
		AIZONES_swap_remote_connections (tilemap_number , swap_counter , swap_counter+1 );
	}

	// Then we realloc the memory used by the zone list to cut off the last zone.
	// We don't need to free it up first as it doesn't contain any pointers or malloc'd space itself.

	cm[tilemap_number].connection_list_pointer = (remote_connection *) realloc ( cm[tilemap_number].connection_list_pointer , (total-1) * sizeof (remote_connection) );

	cm[tilemap_number].remotes--;

}



void AIZONES_create_gate (int tilemap_number, int x1, int y1, int x2, int y2)
{
	int total;

	total = cm[tilemap_number].gates;

	if (cm[tilemap_number].gate_list_pointer == NULL)
	{
		// None created yet.

		cm[tilemap_number].gate_list_pointer = (gate *) malloc ( sizeof (gate) );
	}
	else
	{
		cm[tilemap_number].gate_list_pointer = (gate *) realloc ( cm[tilemap_number].gate_list_pointer , (total+1) * sizeof (gate) );
	}

	cm[tilemap_number].gate_list_pointer[total].x1 = x1;
	cm[tilemap_number].gate_list_pointer[total].y1 = y1;

	cm[tilemap_number].gate_list_pointer[total].x2 = x2;
	cm[tilemap_number].gate_list_pointer[total].y2 = y2;

	cm[tilemap_number].gate_list_pointer[total].leaked = false;
	cm[tilemap_number].gate_list_pointer[total].double_neighbours = false;
	cm[tilemap_number].gate_list_pointer[total].too_many_rects = false;

	cm[tilemap_number].gate_list_pointer[total].neighbour1 = UNSET;
	cm[tilemap_number].gate_list_pointer[total].neighbour2 = UNSET;

	cm[tilemap_number].gates++;
}



void AIZONES_swap_gates (int tilemap_number, int gate_1, int gate_2)
{
	gate temp;

	temp = cm[tilemap_number].gate_list_pointer[gate_1];
	cm[tilemap_number].gate_list_pointer[gate_1] = cm[tilemap_number].gate_list_pointer[gate_2];
	cm[tilemap_number].gate_list_pointer[gate_2] = temp;
}



void AIZONES_delete_particular_gate (int tilemap_number , int gate_number)
{
	// This deletes the given zone by first bubbling it to the end of the queue and then
	// destroying it.

	int total;
	int swap_counter;
	
	total = cm[tilemap_number].gates;

	// Okay, to bubble the given one up to the surface we need to swap all the elements
	// from zone_number to total-2 with the one after them.

	for (swap_counter = gate_number ; swap_counter<total-1 ; swap_counter++)
	{
		AIZONES_swap_gates (tilemap_number , swap_counter , swap_counter+1 );
	}

	// Then we realloc the memory used by the zone list to cut off the last zone.
	// We don't need to free it up first as it doesn't contain any pointers or malloc'd space itself.

	cm[tilemap_number].gate_list_pointer = (gate *) realloc ( cm[tilemap_number].gate_list_pointer , (total-1) * sizeof (gate) );

	cm[tilemap_number].gates--;

}



int AIZONES_calculate_probable_memory_usage (int tilemap_number)
{
	// This function counts the number of groups and their sizes and figures out roughly how
	// memory the lookup tables will piss away if you use them.

	int z_counter;
	int last_group_id;
	int z;
	int total;
	int memory_total;

	memory_total = 0;
	z_counter = 0;
	last_group_id = UNSET;

	total = cm[tilemap_number].ai_zones;

	for (z=0; z<total; z++)
	{
		if ( (cm[tilemap_number].ai_zone_list_pointer[z].unique_group_id != last_group_id) || (z == total-1) )
		{
			// Add this group's memory usage to the stack!
			if (last_group_id != UNSET)
			{
				memory_total += (z_counter*z_counter); // The square lookup table
				memory_total += (total - z_counter); // The outside zone lookup table
			}
			z_counter = 0;
			last_group_id = cm[tilemap_number].ai_zone_list_pointer[z].unique_group_id;
		}

		z_counter++;
	}

	memory_total *= 2; // For shorts!
	memory_total *= 2; // For distance as well as IDs.

	return memory_total;
}



#define AI_ZONE_EDITING_MODE_REMOTES		(0)
#define AI_ZONE_EDITING_MODE_GATES			(1)

#define EDIT_CHANGE_LAYER_BUTTON			(100)

#define AI_ZONE_NOT_CREATING_REMOTE			(0)
#define AI_ZONE_CREATING_FIRST_COORD		(1)
#define AI_ZONE_WAITING_FOR_SECOND			(2)

bool AIZONES_edit_ai_zones ( int state , bool overlay_display, int *current_cursor ,int sx, int sy,  int tilemap_number, int mx, int my, float zoom_level, int grid_size)
{
	static int private_sx;
	static int private_sy;
	static int editing_layer;

	static bool wraparound_horizontal;
	static bool wraparound_vertical;

	int nearest_gate;
	int nearest_remote;

	static bool drawing_gate;
	static int gate_x1;
	static int gate_y1;
	static int gate_x2;
	static int gate_y2;

	static bool set_up_zones;

	int memory_usage;

	static int equivalent_tilesize;

	static int private_editing_mode;

	static int creating_remote_number;
	static int creating_remote_stage;

	char word_string[TEXT_LINE_SIZE];

	int tx;
	int ty;

	static int connection_x1;
	static int connection_y1;

	int temp_x;
	int temp_y;

	int tileset;

	if (state == STATE_INITIALISE)
	{
		private_sx = 0;
		private_sy = 0;
		editing_layer = 0;

		drawing_gate = false;

		wraparound_horizontal = 0;
		wraparound_vertical = 0;

		set_up_zones = false;

		creating_remote_number = UNSET;
		creating_remote_stage = AI_ZONE_NOT_CREATING_REMOTE;

		equivalent_tilesize = 8;

		private_editing_mode = AI_ZONE_EDITING_MODE_GATES;
	}
	else if (state == STATE_SET_UP_BUTTONS)
	{
		editor_view_width = game_screen_width - (8*ICON_SIZE);
		editor_view_height = game_screen_height;

		SIMPGUI_create_nudge_button (TILEMAP_EDITOR_SUB_GROUP, EDIT_CHANGE_LAYER_BUTTON, &editing_layer, editor_view_width+(ICON_SIZE/2)+(1*ICON_SIZE) , ICON_SIZE*2, first_icon , LEFT_ARROW_ICON, LMB, 0, 128, -1, false);
		SIMPGUI_create_nudge_button (TILEMAP_EDITOR_SUB_GROUP, EDIT_CHANGE_LAYER_BUTTON, &editing_layer, editor_view_width+(ICON_SIZE/2)+(6*ICON_SIZE) , ICON_SIZE*2, first_icon , RIGHT_ARROW_ICON, LMB, 0, 128, 1, false);
		
		SIMPGUI_create_set_button (TILEMAP_EDITOR_SUB_GROUP, EDIT_TILEMAP, &private_editing_mode, editor_view_width+(ICON_SIZE/2)+(2*ICON_SIZE) , ICON_SIZE*1, first_icon , USE_WARP_TOOL_ICON, LMB, AI_ZONE_EDITING_MODE_REMOTES);
		SIMPGUI_create_set_button (TILEMAP_EDITOR_SUB_GROUP, EDIT_TILEMAP, &private_editing_mode, editor_view_width+(ICON_SIZE/2)+(5*ICON_SIZE) , ICON_SIZE*1, first_icon , USE_GATE_TOOL_ICON, LMB, AI_ZONE_EDITING_MODE_GATES);

		SIMPGUI_sleep_group (TILEMAP_EDITOR_SUB_GROUP);

		set_up_zones = true;
	}
	else if (state == STATE_RUNNING)
	{
		if (editing_layer >= cm[tilemap_number].map_layers)
		{
			editing_layer = cm[tilemap_number].map_layers-1;
		}

		if (set_up_zones == true)
		{
			set_up_zones = false;

			AIZONES_create_all_ai_zones ( tilemap_number , editing_layer , wraparound_horizontal , wraparound_vertical );
			AIZONES_check_for_leaked_gates_and_double_neighbours (tilemap_number);
		}

		tileset = cm[tilemap_number].tile_set_index;

		tx = ((mx/equivalent_tilesize)+sx)-1;
		ty = ((my/equivalent_tilesize)+sy)-1;

		if (private_editing_mode == AI_ZONE_EDITING_MODE_REMOTES)
		{
			// Remotes can be created anywhere within the map within the edges

			if (tx >= cm[tilemap_number].map_width)
			{
				tx = cm[tilemap_number].map_width - 1;
			}
			else if (tx<0)
			{
				tx = 0;
			}

			if (ty >= cm[tilemap_number].map_height)
			{
				ty = cm[tilemap_number].map_height - 1;
			}
			else if (ty<0)
			{
				ty = 0;
			}
		}
		else if (private_editing_mode == AI_ZONE_EDITING_MODE_GATES)
		{
			// Gates can be created anywhere within the map to the edges

			if (tx > cm[tilemap_number].map_width)
			{
				tx = cm[tilemap_number].map_width ;
			}
			else if (tx<=0)
			{
				tx = 0;
			}

			if (ty > cm[tilemap_number].map_height)
			{
				ty = cm[tilemap_number].map_height;
			}
			else if (ty<=0)
			{
				ty = 0;
			}
		}

		AIZONES_draw_collision_view ( editor_view_width , editor_view_height , tilemap_number, private_sx, private_sy, editing_layer , equivalent_tilesize , wraparound_horizontal, wraparound_vertical );

		nearest_gate = AIZONES_find_nearest_gate ( tilemap_number, private_sx, private_sy, editing_layer , equivalent_tilesize , mx , my );
		nearest_remote = AIZONES_find_nearest_remote ( tilemap_number, private_sx, private_sy, editing_layer , equivalent_tilesize , mx , my );

		AIZONES_draw_ai_zones_and_gates ( editor_view_width , editor_view_height , tilemap_number, private_sx, private_sy, editing_layer , equivalent_tilesize , nearest_gate , nearest_remote );

		if (mx<editor_view_width)
		{
			if ( (private_editing_mode == AI_ZONE_EDITING_MODE_REMOTES) && (tileset != UNSET) )
			{
				OUTPUT_rectangle ( ((tx+1)*equivalent_tilesize)-1 , ((ty+1)*equivalent_tilesize)-1 , ((tx+2)*equivalent_tilesize) , ((ty+2)*equivalent_tilesize) , 255 , 255 , 0 );

				if (creating_remote_stage == AI_ZONE_NOT_CREATING_REMOTE)
				{
					// Okay, so in this mode we're just waiting for the first click.
					if (CONTROL_mouse_hit(LMB) == true)
					{
						AIZONES_create_remote_connection(tilemap_number,tx,ty);
						creating_remote_number = cm[tilemap_number].remotes-1;
						creating_remote_stage++;
						connection_x1 = tx;
						connection_y1 = ty;
					}

					if ( (CONTROL_mouse_hit(RMB) == true) && (nearest_remote != UNSET) )
					{
						AIZONES_delete_particular_remote_connection (tilemap_number,nearest_remote);
					}

				}
				else if (creating_remote_stage == AI_ZONE_CREATING_FIRST_COORD)
				{
					// Get direction from click to figure out which side of the rectangle to join onto.
					if (CONTROL_mouse_down(LMB) == false)
					{
						cm[tilemap_number].connection_list_pointer[creating_remote_number].connective_direction = MATH_get_up_right_down_left (connection_x1,connection_y1,tx,ty);
						creating_remote_stage++;
					}
				}
				else if (creating_remote_stage == AI_ZONE_WAITING_FOR_SECOND)
				{
					// Now we wait for the second press that decides where the second point goes...
					if (CONTROL_mouse_hit(LMB) == true)
					{
						cm[tilemap_number].connection_list_pointer[creating_remote_number].x2 = tx;
						cm[tilemap_number].connection_list_pointer[creating_remote_number].y2 = ty;
						creating_remote_stage = AI_ZONE_NOT_CREATING_REMOTE;
						set_up_zones = true;
					}
				}
			}
			else if ( (private_editing_mode == AI_ZONE_EDITING_MODE_GATES) && (tileset != UNSET) )
			{
				OUTPUT_rectangle ( ((tx+1)*equivalent_tilesize)-1 , ((ty+1)*equivalent_tilesize)-1 , ((tx+1)*equivalent_tilesize) , ((ty+1)*equivalent_tilesize) , 255 , 255 , 0 );

				if ( (CONTROL_mouse_hit(LMB) == true) && (drawing_gate == false) )
				{
					if ( (tx>=0) && (ty>=0) && (tx<cm[tilemap_number].map_width) && (ty<cm[tilemap_number].map_height) )
					{
						drawing_gate = true;

						gate_x1 = tx;
						gate_y1 = ty;
					}
				}

				if ( (CONTROL_mouse_down(LMB) == true) && (drawing_gate == true) )
				{
					gate_x2 = tx;
					gate_y2 = ty;

					if (abs(gate_x2-gate_x1) > abs(gate_y2-gate_y1))
					{
						// Horizontal!
						gate_y2 = gate_y1;
					}
					else
					{
						gate_x2 = gate_x1;
					}

					gate_x2 = MATH_constrain(gate_x2,0,cm[tilemap_number].map_width);
					gate_y2 = MATH_constrain(gate_y2,0,cm[tilemap_number].map_height);

					OUTPUT_filled_rectangle ( ((gate_x1+1)*equivalent_tilesize)-1 , ((gate_y1+1)*equivalent_tilesize)-1 , ((gate_x2+1)*equivalent_tilesize) , ((gate_y2+1)*equivalent_tilesize) , 255 , 255 , 0 );
				}

				if ( (CONTROL_mouse_down(LMB) == false) && (drawing_gate == true) )
				{
					drawing_gate = false;

					gate_x2 = tx;
					gate_y2 = ty;

					if (abs(gate_x2-gate_x1) > abs(gate_y2-gate_y1))
					{
						// Horizontal!
						gate_y2 = gate_y1;
					}
					else
					{
						gate_x2 = gate_x1;
					}

					gate_x2 = MATH_constrain(gate_x2,0,cm[tilemap_number].map_width);
					gate_y2 = MATH_constrain(gate_y2,0,cm[tilemap_number].map_height);

					if (gate_x1 > gate_x2)
					{
						temp_x = gate_x2;
						gate_x2 = gate_x1;
						gate_x1 = temp_x;
					}

					if (gate_y1 > gate_y2)
					{
						temp_y = gate_y2;
						gate_y2 = gate_y1;
						gate_y1 = temp_y;
					}

					AIZONES_create_gate(tilemap_number,gate_x1,gate_y1,gate_x2,gate_y2);

					set_up_zones = true;
				}

				if ( (CONTROL_mouse_hit(RMB) == true) && (nearest_gate != UNSET) )
				{
					AIZONES_delete_particular_gate (tilemap_number,nearest_gate);
					set_up_zones = true;
				}

			}
			
		}
		else
		{
			if (CONTROL_mouse_hit(LMB) == true)
			{
				if (MATH_rectangle_to_point_intersect_by_size ( editor_view_width+(ICON_SIZE/2)+(1*ICON_SIZE) , ICON_SIZE*4 , ICON_SIZE*6, ICON_SIZE*1 , mx , my ) == true)
				{
					if (wraparound_horizontal == true)
					{
						wraparound_horizontal = false;
					}
					else
					{
						wraparound_horizontal = true;
					}
					set_up_zones = true;
				}

				if (MATH_rectangle_to_point_intersect_by_size ( editor_view_width+(ICON_SIZE/2)+(1*ICON_SIZE) , ICON_SIZE*5 , ICON_SIZE*6, ICON_SIZE*1 , mx , my ) == true)
				{
					if (wraparound_vertical == true)
					{
						wraparound_vertical = false;
					}
					else
					{
						wraparound_vertical = true;
					}
					set_up_zones = true;
				}

			}


		}

		if (overlay_display == true)
		{
			editor_view_width = game_screen_width - (8*ICON_SIZE);
			editor_view_height = game_screen_height;

			SIMPGUI_wake_group (TILEMAP_EDITOR_SUB_GROUP);

			OUTPUT_filled_rectangle (editor_view_width,0,game_screen_width,game_screen_height,0,0,0);

			OUTPUT_vline (editor_view_width,0,game_screen_height,255,255,255);

			OUTPUT_centred_text ( editor_view_width+(ICON_SIZE*4) , (ICON_SIZE-FONT_HEIGHT)/2 , "AI ZONE EDITOR" );

			OUTPUT_draw_masked_sprite ( first_icon , EDIT_MODE_ICON, editor_view_width+(ICON_SIZE/2)+(0*ICON_SIZE) , ICON_SIZE*1 );

			OUTPUT_draw_masked_sprite ( first_icon , FORWARD_ARROW, editor_view_width+(ICON_SIZE/2)+(3*ICON_SIZE*private_editing_mode)+ICON_SIZE , ICON_SIZE*1 );
			OUTPUT_draw_masked_sprite ( first_icon , BACKWARD_ARROW, editor_view_width+(ICON_SIZE/2)+(3*ICON_SIZE*private_editing_mode)+ICON_SIZE*3 , ICON_SIZE*1 );

			OUTPUT_draw_masked_sprite ( first_icon , WRAPAROUND_HORIZONTAL_ICON, editor_view_width+(ICON_SIZE/2)+(0*ICON_SIZE) , ICON_SIZE*4 );
			OUTPUT_draw_masked_sprite ( first_icon , WRAPAROUND_VERTICAL_ICON, editor_view_width+(ICON_SIZE/2)+(0*ICON_SIZE) , ICON_SIZE*5 );

			if (wraparound_horizontal == true)
			{
				TILEMAPS_dialogue_box ( editor_view_width+(ICON_SIZE/2)+(1*ICON_SIZE) , ICON_SIZE*4 , ICON_SIZE*6, ICON_SIZE*1 , "YES" );
			}
			else
			{
				TILEMAPS_dialogue_box ( editor_view_width+(ICON_SIZE/2)+(1*ICON_SIZE) , ICON_SIZE*4 , ICON_SIZE*6, ICON_SIZE*1 , "NO" );
			}

			if (wraparound_vertical == true)
			{
				TILEMAPS_dialogue_box ( editor_view_width+(ICON_SIZE/2)+(1*ICON_SIZE) , ICON_SIZE*5 , ICON_SIZE*6, ICON_SIZE*1 , "YES" );
			}
			else
			{
				TILEMAPS_dialogue_box ( editor_view_width+(ICON_SIZE/2)+(1*ICON_SIZE) , ICON_SIZE*5 , ICON_SIZE*6, ICON_SIZE*1 , "NO" );
			}
			
			OUTPUT_draw_masked_sprite ( first_icon , SELECT_LAYER_ICON, editor_view_width+(ICON_SIZE/2)+(0*ICON_SIZE) , ICON_SIZE*2 );

			snprintf (word_string, sizeof(word_string), "LAYER = %d", editing_layer );
			TILEMAPS_dialogue_box ( editor_view_width+(ICON_SIZE/2)+(2*ICON_SIZE) , ICON_SIZE*2 , ICON_SIZE*4, ICON_SIZE*1 , word_string );

			memory_usage = AIZONES_calculate_probable_memory_usage (tilemap_number);

			snprintf (word_string, sizeof(word_string), "MEMORY USAGE = %5dK", (memory_usage/1024) );
			TILEMAPS_dialogue_box ( editor_view_width+(ICON_SIZE/2) , ICON_SIZE*7 , ICON_SIZE*7, ICON_SIZE*1 , word_string );

			SIMPGUI_check_all_buttons ();
			SIMPGUI_draw_all_buttons ();

			if (SIMPGUI_get_click_status (TILEMAP_EDITOR_SUB_GROUP , EDIT_CHANGE_LAYER_BUTTON) == true)
			{
				set_up_zones = true;
			}

		}
		else
		{
			editor_view_width = game_screen_width;
			editor_view_height = game_screen_height;

			SIMPGUI_sleep_group (TILEMAP_EDITOR_SUB_GROUP);
		}

		if (CONTROL_mouse_speed (Z_POS) != 0)
		{
			equivalent_tilesize += CONTROL_mouse_speed (Z_POS);

			equivalent_tilesize = MATH_constrain(equivalent_tilesize,4,32);
		}

	}
	else if (state == STATE_RESET_BUTTONS)
	{
		SIMPGUI_kill_group (TILEMAP_EDITOR_SUB_GROUP);
	}
	else if (state == STATE_SHUTDOWN)
	{
		SIMPGUI_kill_group (TILEMAP_EDITOR_SUB_GROUP);
	}

	return true;

}
