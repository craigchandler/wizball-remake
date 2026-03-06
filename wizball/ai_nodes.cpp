#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <math.h>

#include "ai_nodes.h"
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
#include "spawn_points.h"

#include "fortify.h"

int AINODES_get_free_waypoint_uid (int tilemap_number)
{
/*
	int waypoint_number;
	int test_uid = 0;
	bool found_one;

	do
	{
		found_one = false;

		for (waypoint_number=0; waypoint_number<cm[tilemap_number].waypoints; waypoint_number++)
		{
			if (cm[tilemap_number].waypoint_list_pointer[waypoint_number].uid == test_uid)
			{
				found_one = true;
				test_uid++;
			}
		}
	} while (found_one == true);

	return test_uid;
*/

	cm[tilemap_number].waypoint_next_uid++;

	return (cm[tilemap_number].waypoint_next_uid - 1);
}



void AINODES_remove_dead_waypoint_connection (int tilemap_number, int waypoint_number, int removed_connection_number)
{
	// This first of all moves all the subsequent waypoint connection uids back by one in order to overwrite
	// the dead one and then reallocs away the useless space and the end.

	int connection_number;
	int total_connections = cm[tilemap_number].waypoint_list_pointer[waypoint_number].connections;

	for (connection_number = removed_connection_number; connection_number<total_connections - 1; connection_number++)
	{
		cm[tilemap_number].waypoint_list_pointer[waypoint_number].connection_list_uids[connection_number] = cm[tilemap_number].waypoint_list_pointer[waypoint_number].connection_list_uids[connection_number+1];
		cm[tilemap_number].waypoint_list_pointer[waypoint_number].connection_list_indexes[connection_number] = cm[tilemap_number].waypoint_list_pointer[waypoint_number].connection_list_indexes[connection_number+1];
		cm[tilemap_number].waypoint_list_pointer[waypoint_number].connection_list_types[connection_number] = cm[tilemap_number].waypoint_list_pointer[waypoint_number].connection_list_types[connection_number+1];
	}

	cm[tilemap_number].waypoint_list_pointer[waypoint_number].connection_list_uids = (int *) realloc (cm[tilemap_number].waypoint_list_pointer[waypoint_number].connection_list_uids , total_connections * sizeof(int) );
	cm[tilemap_number].waypoint_list_pointer[waypoint_number].connection_list_indexes = (int *) realloc (cm[tilemap_number].waypoint_list_pointer[waypoint_number].connection_list_indexes , total_connections * sizeof(int) );
	cm[tilemap_number].waypoint_list_pointer[waypoint_number].connection_list_types = (int *) realloc (cm[tilemap_number].waypoint_list_pointer[waypoint_number].connection_list_types , total_connections * sizeof(int) );

	cm[tilemap_number].waypoint_list_pointer[waypoint_number].connections--;
}



void AINODES_remove_duplicate_waypoint_connections (int tilemap_number, int waypoint_number)
{
	int connection_1;
	int connection_2;

	for (connection_1 = 0; connection_1 < cm[tilemap_number].waypoint_list_pointer[waypoint_number].connections; connection_1++)
	{
		for (connection_2 = connection_1+1; connection_2 < cm[tilemap_number].waypoint_list_pointer[waypoint_number].connections; connection_2++)
		{
			if (cm[tilemap_number].waypoint_list_pointer[waypoint_number].connection_list_uids[connection_1] == cm[tilemap_number].waypoint_list_pointer[waypoint_number].connection_list_uids[connection_2])
			{
				AINODES_remove_dead_waypoint_connection (tilemap_number,waypoint_number,connection_2);
				connection_2--;
			}
		}
	}
	
}



void AINODES_remove_waypoint_connection (int tilemap_number, int from_waypoint, int to_waypoint)
{
	// This searches through from_waypoint's list of connected uid's for to_waypoint's uid and then
	// removes it. Should be called twice in order to get both sides of the connection.

	int connection_number;
	int looking_for_uid = cm[tilemap_number].waypoint_list_pointer[to_waypoint].uid;
	int current_uid;

	for (connection_number = 0; connection_number < cm[tilemap_number].waypoint_list_pointer[from_waypoint].connections; connection_number++)
	{
		current_uid = cm[tilemap_number].waypoint_list_pointer[from_waypoint].connection_list_uids[connection_number];
		if (current_uid == looking_for_uid)
		{
			AINODES_remove_dead_waypoint_connection (tilemap_number,from_waypoint,connection_number);
		}
	}
}



void AINODES_add_waypoint_connection (int tilemap_number , int from_waypoint , int to_waypoint )
{
	// This first checks for duplicates and then if none are found it creates a new connection in both
	// the "from" and "to" waypoints.

	int connection_number;
	int from_total;
	int to_total;

	for (connection_number = 0; connection_number<cm[tilemap_number].waypoint_list_pointer[from_waypoint].connections ; connection_number++)
	{
		if (cm[tilemap_number].waypoint_list_pointer[from_waypoint].connection_list_indexes[connection_number] == to_waypoint)
		{
			// Already exists in the list.
			return;
		}
	}

	if ( (cm[tilemap_number].waypoint_list_pointer[from_waypoint].connection_list_indexes == NULL) && (cm[tilemap_number].waypoint_list_pointer[from_waypoint].connections == 0) )
	{
		from_total = 0;
		cm[tilemap_number].waypoint_list_pointer[from_waypoint].connection_list_indexes = (int *) malloc (sizeof(int));
		cm[tilemap_number].waypoint_list_pointer[from_waypoint].connection_list_uids = (int *) malloc (sizeof(int));
		cm[tilemap_number].waypoint_list_pointer[from_waypoint].connection_list_types = (int *) malloc (sizeof(int));
	}
	else
	{
		from_total = cm[tilemap_number].waypoint_list_pointer[from_waypoint].connections;
		cm[tilemap_number].waypoint_list_pointer[from_waypoint].connection_list_indexes = (int *) realloc (cm[tilemap_number].waypoint_list_pointer[from_waypoint].connection_list_indexes , (from_total+1) * sizeof(int));
		cm[tilemap_number].waypoint_list_pointer[from_waypoint].connection_list_uids = (int *) realloc (cm[tilemap_number].waypoint_list_pointer[from_waypoint].connection_list_uids , (from_total+1) * sizeof(int));
		cm[tilemap_number].waypoint_list_pointer[from_waypoint].connection_list_types = (int *) realloc (cm[tilemap_number].waypoint_list_pointer[from_waypoint].connection_list_types , (from_total+1) * sizeof(int));
	}

	if ( (cm[tilemap_number].waypoint_list_pointer[to_waypoint].connection_list_indexes == NULL) && (cm[tilemap_number].waypoint_list_pointer[to_waypoint].connections == 0) )
	{
		to_total = 0;
		cm[tilemap_number].waypoint_list_pointer[to_waypoint].connection_list_indexes = (int *) malloc (sizeof(int));
		cm[tilemap_number].waypoint_list_pointer[to_waypoint].connection_list_uids = (int *) malloc (sizeof(int));
		cm[tilemap_number].waypoint_list_pointer[to_waypoint].connection_list_types = (int *) malloc (sizeof(int));
	}
	else
	{
		to_total = cm[tilemap_number].waypoint_list_pointer[to_waypoint].connections;
		cm[tilemap_number].waypoint_list_pointer[to_waypoint].connection_list_indexes = (int *) realloc (cm[tilemap_number].waypoint_list_pointer[to_waypoint].connection_list_indexes , (to_total+1) * sizeof(int));
		cm[tilemap_number].waypoint_list_pointer[to_waypoint].connection_list_uids = (int *) realloc (cm[tilemap_number].waypoint_list_pointer[to_waypoint].connection_list_uids , (to_total+1) * sizeof(int));
		cm[tilemap_number].waypoint_list_pointer[to_waypoint].connection_list_types = (int *) realloc (cm[tilemap_number].waypoint_list_pointer[to_waypoint].connection_list_types , (to_total+1) * sizeof(int));
	}

	cm[tilemap_number].waypoint_list_pointer[from_waypoint].connection_list_uids[from_total] = cm[tilemap_number].waypoint_list_pointer[to_waypoint].uid;
	cm[tilemap_number].waypoint_list_pointer[to_waypoint].connection_list_uids[to_total] = cm[tilemap_number].waypoint_list_pointer[from_waypoint].uid;

	cm[tilemap_number].waypoint_list_pointer[from_waypoint].connections++;
	cm[tilemap_number].waypoint_list_pointer[to_waypoint].connections++;
	
}



void AINODES_swap_waypoints (int tilemap_number, int waypoint_1, int waypoint_2)
{
	waypoint temp;

	temp = cm[tilemap_number].waypoint_list_pointer[waypoint_1];
	cm[tilemap_number].waypoint_list_pointer[waypoint_1] = cm[tilemap_number].waypoint_list_pointer[waypoint_2];
	cm[tilemap_number].waypoint_list_pointer[waypoint_2] = temp;
}



void AINODES_destroy_waypoint (int tilemap_number, int waypoint_number)
{
	free (cm[tilemap_number].waypoint_list_pointer[waypoint_number].connection_list_indexes);
	free (cm[tilemap_number].waypoint_list_pointer[waypoint_number].connection_list_uids);
	free (cm[tilemap_number].waypoint_list_pointer[waypoint_number].connection_list_types);
}



void AINODES_destroy_waypoint_lookup_tables (int tilemap_number)
{
	if (cm[tilemap_number].ai_node_lookup_table_list_pointer != NULL)
	{
		int family_number;
		
		for (family_number=0; family_number<cm[tilemap_number].ai_node_lookup_table_list_size; family_number++)
		{
			free (cm[tilemap_number].ai_node_lookup_table_list_pointer[family_number].pathfinding_data);
			free (cm[tilemap_number].ai_node_lookup_table_list_pointer[family_number].distance_data);
		}

		free (cm[tilemap_number].ai_node_lookup_table_list_pointer);
		cm[tilemap_number].ai_node_lookup_table_list_size = 0;
	}
}



int AINODES_get_next_node (int tilemap_number, int current_waypoint_node, int destination_node)
{
	int family_number = cm[tilemap_number].waypoint_list_pointer[current_waypoint_node].family;
	int family_size = cm[tilemap_number].ai_node_lookup_table_list_pointer[family_number].size;
	int first_member = cm[tilemap_number].ai_node_lookup_table_list_pointer[family_number].first_member;
	int offset = ((destination_node-first_member) * family_size) + (current_waypoint_node-first_member);

	return cm[tilemap_number].ai_node_lookup_table_list_pointer[family_number].pathfinding_data[offset];
}



int AINODES_get_node_of_type (int tilemap_number, int zone_number, int node_type_wanted)
{
	// Looks in the current zone for a waypoint of the matching type...
	return 0;
}



int AINODES_get_distance_to_node_from_entity (int tilemap_number, int current_waypoint_node, int destination_node)
{
	int family_number = cm[tilemap_number].waypoint_list_pointer[current_waypoint_node].family;
	int family_size = cm[tilemap_number].ai_node_lookup_table_list_pointer[family_number].size;
	int first_member = cm[tilemap_number].ai_node_lookup_table_list_pointer[family_number].first_member;
	int offset = ((destination_node-first_member) * family_size) + (current_waypoint_node-first_member);

	return cm[tilemap_number].ai_node_lookup_table_list_pointer[family_number].distance_data[offset];
}



int AINODES_get_nearest_location_of_type (int tilemap_number, int current_node, int location_type_wanted)
{
	// This just goes through the nodes in the current_node's family looking for one of the right
	// location type. Then it gets the distance to it and the lowest distance wins. Woot!

	int distance;
	int test_node;
	int lowest_distance = UNSET;
	int lowest_waypoint_number = UNSET;

	int family_number = cm[tilemap_number].waypoint_list_pointer[current_node].family;
	int family_size = cm[tilemap_number].ai_node_lookup_table_list_pointer[family_number].size;
	int first_member = cm[tilemap_number].ai_node_lookup_table_list_pointer[family_number].first_member;

	for (test_node = first_member; test_node<first_member+family_size; test_node++)
	{
		if (cm[tilemap_number].waypoint_list_pointer[test_node].location_index == location_type_wanted)
		{
			distance = AINODES_get_distance_to_node_from_entity (tilemap_number, current_node, test_node);

			if ((distance < lowest_distance) || (lowest_distance == UNSET))
			{
				lowest_distance = distance;
				lowest_waypoint_number = test_node;
			}
		}
	}

	return lowest_waypoint_number;
}



void AINODES_output_waypoint_lookup_tables (void)
{
	// Just to test that everything is tickety-boo, this will output the data created by the pathfinding
	// generation routines to a text file.

	char text[MAX_LINE_SIZE];
	char small_text[MAX_LINE_SIZE];

	FILE *file_pointer = fopen (MAIN_get_project_filename("waypoint_lookup_tables.txt", true) , "w");

	if (file_pointer == NULL)
	{
		assert (0); // Arse!
	}

	fputs("Waypoint Lookup Tables\n\n",file_pointer);

	int start_node,destination_node;
	int family_size;
	int offset;

	int tilemap_number;
	int family_number;
	int total_families;

	int first_member;

	for (tilemap_number=0; tilemap_number<number_of_tilemaps_loaded; tilemap_number++)
	{
		total_families = cm[tilemap_number].ai_node_lookup_table_list_size;

		if (total_families == 1)
		{
			snprintf(text, sizeof(text), "Tilemap %i : %i Family...\n\n",tilemap_number,total_families);
		}
		else
		{
			snprintf(text, sizeof(text), "Tilemap %i : %i Families..\n\n",tilemap_number,total_families);
		}
		fputs(text,file_pointer);

		for (family_number=0; family_number<total_families; family_number++)
		{
			snprintf(text, sizeof(text), "\tFamily Number %i\n\n",family_number);
			fputs(text,file_pointer);
			
			family_size = cm[tilemap_number].ai_node_lookup_table_list_pointer[family_number].size;
			first_member = cm[tilemap_number].ai_node_lookup_table_list_pointer[family_number].first_member;

			snprintf(text, sizeof(text), "\t\t       ");
			for (start_node = first_member; start_node<first_member+family_size; start_node++)
			{
				snprintf(small_text, sizeof(small_text), "%3i ",start_node);
				strcat(text,small_text);
			}
			strcat(text,"\n");
			fputs(text,file_pointer);

			snprintf(text, sizeof(text), "\t\t       ");
			for (start_node = 0; start_node<family_size; start_node++)
			{
				snprintf(small_text, sizeof(small_text), "--- %d",start_node);
				strcat(text,small_text);
			}
			strcat(text,"\n");
			fputs(text,file_pointer);

			for (start_node = first_member; start_node<first_member+family_size; start_node++)
			{
				snprintf(text, sizeof(text), "\t\t %3i | ",start_node);

				for (destination_node = first_member; destination_node<first_member+family_size; destination_node++)
				{
					offset = ((destination_node-first_member) * family_size) + (start_node-first_member);
					
					snprintf (small_text, sizeof(small_text), "%3i ",cm[tilemap_number].ai_node_lookup_table_list_pointer[family_number].pathfinding_data[offset]);
					strcat (text,small_text);
				}

				strcat (text,"\n");

				fputs(text,file_pointer);
			}

			fputs("\n",file_pointer);
		}
	}
}



void AINODES_delete_particular_waypoint (int tilemap_number , int waypoint_number)
{
	// This deletes the given waypoint by first bubbling it to the end of the queue and then
	// destroying it.

	int total;
	int swap_counter;
	
	total = cm[tilemap_number].waypoints;

	// Okay, to bubble the given one up to the surface we need to swap all the elements
	// from waypoint_number to total-2 with the one after them.

	for (swap_counter = waypoint_number ; swap_counter<total-1 ; swap_counter++)
	{
		AINODES_swap_waypoints (tilemap_number , swap_counter , swap_counter+1 );
	}

	// Then we destroy the memory used by the lists in the last waypoint to prevent a memory leak...

	AINODES_destroy_waypoint (tilemap_number,total-1);

	// Then we realloc the memory used by the waypoint list to cut off the last waypoint.
	// We don't need to free it up first as it doesn't contain any pointers or malloc'd space itself.

	cm[tilemap_number].waypoint_list_pointer = (waypoint *) realloc ( cm[tilemap_number].waypoint_list_pointer , (total-1) * sizeof (waypoint) );

	cm[tilemap_number].waypoints--;
}



void AINODES_create_waypoint (int tilemap_number, int x, int y)
{
	int total;

	total = cm[tilemap_number].waypoints;

	if (cm[tilemap_number].waypoint_list_pointer == NULL)
	{
		// None created yet.

		cm[tilemap_number].waypoint_list_pointer = (waypoint *) malloc ( sizeof (waypoint) );
	}
	else
	{
		cm[tilemap_number].waypoint_list_pointer = (waypoint *) realloc ( cm[tilemap_number].waypoint_list_pointer , (total+1) * sizeof (waypoint) );
	}

	cm[tilemap_number].waypoint_list_pointer[total].x = x;
	cm[tilemap_number].waypoint_list_pointer[total].y = y;

	cm[tilemap_number].waypoint_list_pointer[total].uid = AINODES_get_free_waypoint_uid(tilemap_number);	

	cm[tilemap_number].waypoint_list_pointer[total].connections = 0;
	cm[tilemap_number].waypoint_list_pointer[total].connection_list_indexes = NULL;
	cm[tilemap_number].waypoint_list_pointer[total].connection_list_uids = NULL;
	cm[tilemap_number].waypoint_list_pointer[total].connection_list_types = NULL;

	cm[tilemap_number].waypoint_list_pointer[total].grabbed_status = NODE_UNSELECTED;
	cm[tilemap_number].waypoint_list_pointer[total].grabbed_x = 0;
	cm[tilemap_number].waypoint_list_pointer[total].grabbed_y = 0;

	strcpy ( cm[tilemap_number].waypoint_list_pointer[total].type , "UNSET" );
	cm[tilemap_number].waypoint_list_pointer[total].type_index = UNSET;

	strcpy ( cm[tilemap_number].waypoint_list_pointer[total].location , "UNSET" );
	cm[tilemap_number].waypoint_list_pointer[total].location_index = UNSET;

	cm[tilemap_number].waypoints++;
}



void AINODES_confirm_waypoint_links (void)
{
	int tilemap_number;
	int waypoint_number;

	for (tilemap_number=0; tilemap_number<number_of_tilemaps_loaded; tilemap_number++)
	{
		for (waypoint_number=0; waypoint_number<cm[tilemap_number].waypoints; waypoint_number++)
		{
			cm[tilemap_number].waypoint_list_pointer[waypoint_number].type_index = GPL_find_word_value ("AI_NODE_TYPES" , cm[tilemap_number].waypoint_list_pointer[waypoint_number].type );
			cm[tilemap_number].waypoint_list_pointer[waypoint_number].type_index = GPL_find_word_value ("LOCATION_TYPES" , cm[tilemap_number].waypoint_list_pointer[waypoint_number].location );
		}
	}
}



int AINODES_copy_waypoint (int tilemap_number, int waypoint_number, int relative, int grid_size)
{
	int x,y;
	int new_waypoint_number;

	x = cm[tilemap_number].waypoint_list_pointer[waypoint_number].x;
	y = cm[tilemap_number].waypoint_list_pointer[waypoint_number].y;

	x++;

	AINODES_create_waypoint (tilemap_number, x, y);

	new_waypoint_number = cm[tilemap_number].waypoints-1;

	strcpy ( cm[tilemap_number].waypoint_list_pointer[new_waypoint_number].type , cm[tilemap_number].waypoint_list_pointer[waypoint_number].type );
	strcpy ( cm[tilemap_number].waypoint_list_pointer[new_waypoint_number].location , cm[tilemap_number].waypoint_list_pointer[waypoint_number].location );

	if (relative == 1)
	{
		// If this is set then we need to link the waypoints...

		AINODES_add_waypoint_connection (tilemap_number,waypoint_number,new_waypoint_number);
	}

	return new_waypoint_number;
}



#define NO_DRAG_THRESHOLD				(5)

void AINODES_partially_set_waypoint_status (int tilemap_number, int x1, int y1, int x2, int y2, float zoom_level, int flagging_type, int drag_time)
{
	// First of all resets all PRESUMPTIVE nodes to their base states and then
	// reset the PRESUMPTIVES. This is dependant on the flagging_type, of course.

	// Note that if the time that the mouse button was held down for was less than 
	// "NO_DRAG_THRESHOLD" frames then only the first encountered node will be
	// flagged as it's the equivalent of a single-click until then.

	int waypoint_number;

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

	for (waypoint_number=0; waypoint_number<cm[tilemap_number].waypoints; waypoint_number++)
	{
		if (flagging_type == SELECTING_NODES)
		{
			// Reset 'em regardless!
			cm[tilemap_number].waypoint_list_pointer[waypoint_number].grabbed_status = NODE_UNSELECTED;
		}
		else
		{
			if (cm[tilemap_number].waypoint_list_pointer[waypoint_number].grabbed_status == NODE_PRESUMPTIVE_UNSELECTED)
			{
				cm[tilemap_number].waypoint_list_pointer[waypoint_number].grabbed_status = NODE_SELECTED;
			}
			else if (cm[tilemap_number].waypoint_list_pointer[waypoint_number].grabbed_status == NODE_PRESUMPTIVE_SELECTED)
			{
				cm[tilemap_number].waypoint_list_pointer[waypoint_number].grabbed_status = NODE_UNSELECTED;
			}
		}
	}

	int node_x,node_y;

	for (waypoint_number=0; waypoint_number<cm[tilemap_number].waypoints; waypoint_number++)
	{
		node_x = cm[tilemap_number].waypoint_list_pointer[waypoint_number].x;
		node_y = cm[tilemap_number].waypoint_list_pointer[waypoint_number].y;

		if (MATH_rectangle_to_rectangle_intersect ( node_x-int(float(WAYPOINT_HANDLE_RADIUS)/zoom_level) , node_y-int(float(WAYPOINT_HANDLE_RADIUS)/zoom_level) , node_x+int(float(WAYPOINT_HANDLE_RADIUS)/zoom_level) , node_y+int(float(WAYPOINT_HANDLE_RADIUS)/zoom_level) , x1, y1, x2, y2 ) == true)
		{
			if ( (flagging_type == SELECTING_NODES) || (flagging_type == ADDING_NODES) )
			{
				if (cm[tilemap_number].waypoint_list_pointer[waypoint_number].grabbed_status == NODE_UNSELECTED)
				{
					cm[tilemap_number].waypoint_list_pointer[waypoint_number].grabbed_status = NODE_PRESUMPTIVE_SELECTED;
					
					if (drag_time < NO_DRAG_THRESHOLD)
					{
						// We're only altering the one item until we've held down the button a while.
						return;
					}
				}
			}
			else if (flagging_type == REMOVING_NODES)
			{
				if (cm[tilemap_number].waypoint_list_pointer[waypoint_number].grabbed_status == NODE_SELECTED)
				{
					cm[tilemap_number].waypoint_list_pointer[waypoint_number].grabbed_status = NODE_PRESUMPTIVE_UNSELECTED;
					
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



void AINODES_deselect_waypoints (int tilemap_number)
{
	// Flags the presumptives as actuals. If there's only one selected then it's ID is returned, otherwise
	// a negative number is returned.

	int waypoint_number;

	for (waypoint_number=0; waypoint_number<cm[tilemap_number].waypoints; waypoint_number++)
	{
		cm[tilemap_number].waypoint_list_pointer[waypoint_number].grabbed_status = NODE_UNSELECTED;
	}
}



int AINODES_lock_waypoint_status (int tilemap_number)
{
	// Flags the presumptives as actuals. If there's only one selected then it's ID is returned, otherwise
	// a negative number is returned.

	int waypoint_number;
	int flagged_waypoint_number = UNSET;

	int total_flagged = 0;

	for (waypoint_number=0; waypoint_number<cm[tilemap_number].waypoints; waypoint_number++)
	{
		if (cm[tilemap_number].waypoint_list_pointer[waypoint_number].grabbed_status == NODE_PRESUMPTIVE_UNSELECTED)
		{
			cm[tilemap_number].waypoint_list_pointer[waypoint_number].grabbed_status = NODE_UNSELECTED;
		}
		else if (cm[tilemap_number].waypoint_list_pointer[waypoint_number].grabbed_status == NODE_PRESUMPTIVE_SELECTED)
		{
			total_flagged++;
			flagged_waypoint_number = waypoint_number;
			cm[tilemap_number].waypoint_list_pointer[waypoint_number].grabbed_status = NODE_SELECTED;
		}
		else if (cm[tilemap_number].waypoint_list_pointer[waypoint_number].grabbed_status == NODE_SELECTED)
		{
			// Already selected, but we've gotta' increase the counter for it.
			total_flagged++;
			flagged_waypoint_number = waypoint_number;
		}
	}

	if (total_flagged == 0)
	{
		return NO_NODES_SELECTED;
	}
	else if (total_flagged == 1)
	{
		return flagged_waypoint_number;
	}
	else
	{
		return MULTIPLE_NODES_SELECTED;
	}

}



void AINODES_get_grabbed_waypoint_co_ords (int tilemap_number)
{
	int waypoint_number;

	for (waypoint_number=0; waypoint_number<cm[tilemap_number].waypoints; waypoint_number++)
	{
		if (cm[tilemap_number].waypoint_list_pointer[waypoint_number].grabbed_status == NODE_SELECTED)
		{
			cm[tilemap_number].waypoint_list_pointer[waypoint_number].grabbed_x = cm[tilemap_number].waypoint_list_pointer[waypoint_number].x;
			cm[tilemap_number].waypoint_list_pointer[waypoint_number].grabbed_y = cm[tilemap_number].waypoint_list_pointer[waypoint_number].y;
		}
	}
}



void AINODES_position_selected_waypoints (int tilemap_number, int offset_x, int offset_y, int grabbed_waypoint, int grid_size, bool round_to_tilesize)
{
	// Updates all the selected nodes so they are positioned offset_x and offset_y from where
	// they started.

	// Note that although the co-ordinates are rounded to the grid, they are only rounded by the
	// amount that the grabbed node is out of whack.

	int out_of_whack_x;
	int out_of_whack_y;

	out_of_whack_x = (cm[tilemap_number].waypoint_list_pointer[grabbed_waypoint].grabbed_x + offset_x) % grid_size;
	out_of_whack_y = (cm[tilemap_number].waypoint_list_pointer[grabbed_waypoint].grabbed_y + offset_y) % grid_size;

	int waypoint_number;

	for (waypoint_number=0; waypoint_number<cm[tilemap_number].waypoints; waypoint_number++)
	{
		if (cm[tilemap_number].waypoint_list_pointer[waypoint_number].grabbed_status == NODE_SELECTED)
		{
			cm[tilemap_number].waypoint_list_pointer[waypoint_number].x = cm[tilemap_number].waypoint_list_pointer[waypoint_number].grabbed_x + offset_x;
			cm[tilemap_number].waypoint_list_pointer[waypoint_number].y = cm[tilemap_number].waypoint_list_pointer[waypoint_number].grabbed_y + offset_y;

			if (round_to_tilesize == true)
			{
				cm[tilemap_number].waypoint_list_pointer[waypoint_number].x -= out_of_whack_x;
				cm[tilemap_number].waypoint_list_pointer[waypoint_number].y -= out_of_whack_y;
			}
		}
	}
}



void AINODES_relink_waypoint_connections (int tilemap_number, int waypoint_number)
{
	int from_connection,to_waypoint;

	// Reset all current ties! NOW!

	for (from_connection=0; from_connection<cm[tilemap_number].waypoint_list_pointer[waypoint_number].connections; from_connection++)
	{
		cm[tilemap_number].waypoint_list_pointer[waypoint_number].connection_list_indexes[from_connection] = UNSET;
	}

	for (from_connection=0; from_connection<cm[tilemap_number].waypoint_list_pointer[waypoint_number].connections; from_connection++)
	{
		for (to_waypoint=0; to_waypoint<cm[tilemap_number].waypoints; to_waypoint++)
		{
			if (to_waypoint != waypoint_number) // So you can't link it with itself.
			{
				if (cm[tilemap_number].waypoint_list_pointer[waypoint_number].connection_list_uids[from_connection] == cm[tilemap_number].waypoint_list_pointer[to_waypoint].uid )
				{
					cm[tilemap_number].waypoint_list_pointer[waypoint_number].connection_list_indexes[from_connection] = to_waypoint;
				}
			}
		}

		// If afterwards there are any broken links (ie, non UNSET connection uids which weren't found), change their name to UNSET.

		if (cm[tilemap_number].waypoint_list_pointer[waypoint_number].connection_list_uids[from_connection] != UNSET)
		{
			// If there's a name given for the connection...
			if (cm[tilemap_number].waypoint_list_pointer[waypoint_number].connection_list_indexes[from_connection] == UNSET)
			{
				// But we couldn't find it, then that spawn point was obviously deleted.
				AINODES_remove_dead_waypoint_connection (tilemap_number, waypoint_number, from_connection);
				// So we do-over this connection because it's now another connection. Bonkers!
				from_connection--;
			}
		}
	}
	
}



void AINODES_relink_all_waypoint_connections (int tilemap_number)
{
	int waypoint_number;

	for (waypoint_number=0; waypoint_number<cm[tilemap_number].waypoints; waypoint_number++)
	{
		AINODES_relink_waypoint_connections (tilemap_number, waypoint_number);
	}
}



void AINODES_spread_family (int tilemap_number,int waypoint_number)
{
	// Urgh! Recursive code!

	int relative;
	int relative_index;

	for (relative=0; relative<cm[tilemap_number].waypoint_list_pointer[waypoint_number].connections; relative++)
	{
		relative_index = cm[tilemap_number].waypoint_list_pointer[waypoint_number].connection_list_indexes[relative];

		if (cm[tilemap_number].waypoint_list_pointer[relative_index].family == UNSET)
		{
			cm[tilemap_number].waypoint_list_pointer[relative_index].family = cm[tilemap_number].waypoint_list_pointer[waypoint_number].family;
			AINODES_spread_family (tilemap_number,relative_index);
		}
	}
}



int AINODES_put_waypoints_into_families (int tilemap_number)
{
	// So that we can make look-up pathfinding tables which are of sensible size we need to
	// arrange the nodes so that all connected nodes are together in the indexing. To do this
	// we assign a "family" number to each one. This data isn't saved out so it's necessary
	// to do it at load time and ALWAYS before calling the AINODES_reorder_waypoint_families
	// function.

	// We do this by resetting all the family numbers to UNSET and then going through all the
	// nodes and attempting to give each of them a family number (and then all their relatives).

	int waypoint_number;

	for (waypoint_number=0; waypoint_number<cm[tilemap_number].waypoints; waypoint_number++)
	{
		cm[tilemap_number].waypoint_list_pointer[waypoint_number].family = UNSET;
	}

	// So that's them all UNSET, now to spread the joy...

	int family_id = 0;

	for (waypoint_number=0; waypoint_number<cm[tilemap_number].waypoints; waypoint_number++)
	{
		if (cm[tilemap_number].waypoint_list_pointer[waypoint_number].family == UNSET)
		{
			cm[tilemap_number].waypoint_list_pointer[waypoint_number].family = family_id;
			AINODES_spread_family (tilemap_number,waypoint_number);
			family_id++;
		}
	}

	return family_id;
}



void AINODES_reorder_waypoint_families (int tilemap_number)
{
	// Standard bubble-sort.
	
	int waypoint_number;
	bool swapped;

	do
	{
		swapped = false;

		for (waypoint_number=0; waypoint_number<cm[tilemap_number].waypoints-1; waypoint_number++)
		{
			if (cm[tilemap_number].waypoint_list_pointer[waypoint_number].family > cm[tilemap_number].waypoint_list_pointer[waypoint_number+1].family)
			{
				AINODES_swap_waypoints (tilemap_number, waypoint_number, waypoint_number+1);
				swapped = true;
			}
		}
		
	} while (swapped);
}



int AINODES_prepare_waypoints_for_tabulation (int tilemap_number)
{
	int total_families;

	AINODES_relink_all_waypoint_connections (tilemap_number);
	total_families = AINODES_put_waypoints_into_families (tilemap_number);
	AINODES_reorder_waypoint_families (tilemap_number);
	AINODES_relink_all_waypoint_connections (tilemap_number);

	return total_families;
}



int AINODES_count_family_members (int tilemap_number,int family_number)
{
	int waypoint_number;
	int counter = 0;

	for (waypoint_number=0; waypoint_number<cm[tilemap_number].waypoints; waypoint_number++)
	{
		if (cm[tilemap_number].waypoint_list_pointer[waypoint_number].family == family_number)
		{
			counter++;
		}
	}

	return counter;
}



int AINODES_first_family_member (int tilemap_number,int family_number)
{
	int waypoint_number;

	for (waypoint_number=0; waypoint_number<cm[tilemap_number].waypoints; waypoint_number++)
	{
		if (cm[tilemap_number].waypoint_list_pointer[waypoint_number].family == family_number)
		{
			return waypoint_number;
		}
	}

	return UNSET;
}



void AINODES_spread_from_family_member (int tilemap_number,int waypoint_number,int total_distance)
{
	// This is a recursive routine that attempts to spread into connected waypoints if
	// their distance is less than it'd take that waypoint to get there.

	int distance_to_relative;
	int relative_number;
	int relative_index;
	int x1,y1,x2,y2;

	cm[tilemap_number].waypoint_list_pointer[waypoint_number].total_distance = total_distance;

	x1 = cm[tilemap_number].waypoint_list_pointer[waypoint_number].x;
	y1 = cm[tilemap_number].waypoint_list_pointer[waypoint_number].y;
	
	for (relative_number=0; relative_number<cm[tilemap_number].waypoint_list_pointer[waypoint_number].connections; relative_number++)
	{
		if (cm[tilemap_number].waypoint_list_pointer[waypoint_number].connection_list_types[relative_number] & AI_NODE_CONNECTION_BITFLAG_BLOCKED == 0)
		{
			if (cm[tilemap_number].waypoint_list_pointer[waypoint_number].connection_list_types[relative_number] & AI_NODE_CONNECTION_BITFLAG_PORTAL)
			{
				relative_index = cm[tilemap_number].waypoint_list_pointer[waypoint_number].connection_list_indexes[relative_number];

				x2 = cm[tilemap_number].waypoint_list_pointer[relative_index].x;
				y2 = cm[tilemap_number].waypoint_list_pointer[relative_index].y;

				distance_to_relative = MATH_get_distance_int (x1,y1,x2,y2);
			}
			else
			{
				distance_to_relative = 0;
			}

			if (cm[tilemap_number].waypoint_list_pointer[relative_index].total_distance > total_distance + distance_to_relative)
			{
				AINODES_spread_from_family_member (tilemap_number,relative_index, total_distance + distance_to_relative);
			}
		}
	}
}



void AINODES_store_closest_relative (int tilemap_number,int destination_node,int start_node)
{
	int offset;
	int family_number = cm[tilemap_number].waypoint_list_pointer[start_node].family;
	int family_size = cm[tilemap_number].ai_node_lookup_table_list_pointer[family_number].size;
	int relative_number;
	int relative_index;
	int lowest_distance = UNSET;
	int closest_relative = UNSET;
	int first_member = cm[tilemap_number].ai_node_lookup_table_list_pointer[family_number].first_member;
	
	offset = ((destination_node-first_member) * family_size) + (start_node-first_member);

	if (start_node == destination_node)
	{
		cm[tilemap_number].ai_node_lookup_table_list_pointer[family_number].pathfinding_data[offset] = PATHFINDING_ARRIVED;
		cm[tilemap_number].ai_node_lookup_table_list_pointer[family_number].distance_data[offset] = 0;
	}
	else
	{
		for (relative_number=0; relative_number<cm[tilemap_number].waypoint_list_pointer[start_node].connections; relative_number++)
		{
			relative_index = cm[tilemap_number].waypoint_list_pointer[start_node].connection_list_indexes[relative_number];

			if ((cm[tilemap_number].waypoint_list_pointer[relative_index].total_distance < lowest_distance) || (lowest_distance==UNSET))
			{
				lowest_distance = cm[tilemap_number].waypoint_list_pointer[relative_index].total_distance;
				closest_relative = relative_index;
			}
		}

		cm[tilemap_number].ai_node_lookup_table_list_pointer[family_number].pathfinding_data[offset] = closest_relative;
		cm[tilemap_number].ai_node_lookup_table_list_pointer[family_number].distance_data[offset] = lowest_distance;
	}
}



void AINODES_create_lookup_table (int tilemap_number, int families)
{
	cm[tilemap_number].ai_node_lookup_table_list_size = families;
	cm[tilemap_number].ai_node_lookup_table_list_pointer = (ai_node_lookup_table *) malloc (sizeof(ai_node_lookup_table) * families);

	int family_size;
	int family_number;
	int first_member;
	int member_number;
	int counter;

	for (family_number=0; family_number<families; family_number++)
	{
		family_size = AINODES_count_family_members (tilemap_number,family_number);
		first_member = AINODES_first_family_member (tilemap_number,family_number);

		cm[tilemap_number].ai_node_lookup_table_list_pointer[family_number].size = family_size;
		cm[tilemap_number].ai_node_lookup_table_list_pointer[family_number].first_member = first_member;

		cm[tilemap_number].ai_node_lookup_table_list_pointer[family_number].pathfinding_data = (int *) malloc (sizeof(int) * family_size * family_size);
		cm[tilemap_number].ai_node_lookup_table_list_pointer[family_number].distance_data = (int *) malloc (sizeof(int) * family_size * family_size);

		for (member_number = first_member; member_number<first_member+family_size; member_number++)
		{

			for (counter = first_member; counter<first_member+family_size; counter++)
			{
				// Just blank ALL the member's distances so the below can spread to them...
				cm[tilemap_number].waypoint_list_pointer[counter].total_distance = ABSURDLY_HIGH_NUMBER;
			}

			AINODES_spread_from_family_member (tilemap_number,member_number,0);

			for (counter = first_member; counter<first_member+family_size; counter++)
			{
				// Now for each waypoint which isn't the one we're heading to, find the shortest
				// distance'd relative and store it in the pathfinding data table...

				AINODES_store_closest_relative (tilemap_number,member_number,counter);
			}
		}
	}
}



void AINODES_create_all_waypoint_lookup_tables (void)
{
	int tilemap_number;
	int families;

	for (tilemap_number=0; tilemap_number<number_of_tilemaps_loaded; tilemap_number++)
	{
		families = AINODES_prepare_waypoints_for_tabulation (tilemap_number);

		AINODES_create_lookup_table (tilemap_number, families);
	}
}



bool AINODES_what_selected_waypoint_thing_has_been_grabbed (int tilemap_number, int x, int y, float zoom_level, int *grabbed_handle, int *grabbed_node)
{
	int waypoint_number;

	int node_x,node_y;

	int grabbed;

	grabbed = UNSET;
	
	for (waypoint_number=0; waypoint_number<cm[tilemap_number].waypoints; waypoint_number++)
	{
		// Go through all the nodes...

		if (cm[tilemap_number].waypoint_list_pointer[waypoint_number].grabbed_status == NODE_SELECTED)
		{
			// But only consider those which have been flagged as selected.

			grabbed = UNSET;

			node_x = cm[tilemap_number].waypoint_list_pointer[waypoint_number].x;
			node_y = cm[tilemap_number].waypoint_list_pointer[waypoint_number].y;

			if (MATH_rectangle_to_point_intersect ( node_x-int(float(WAYPOINT_HANDLE_RADIUS)/zoom_level) , node_y-int(float(WAYPOINT_HANDLE_RADIUS)/zoom_level) , node_x+int(float(WAYPOINT_HANDLE_RADIUS)/zoom_level) , node_y+int(float(WAYPOINT_HANDLE_RADIUS)/zoom_level) , x , y ) == true)
			{
				grabbed = WAYPOINT_HANDLE_CENTER;
			}

			if (MATH_rectangle_to_point_intersect ( node_x+int(float(WAYPOINT_HANDLE_RADIUS)/zoom_level) , node_y-int(float(WAYPOINT_HANDLE_RADIUS/2)/zoom_level) , node_x+int(float(WAYPOINT_HANDLE_RADIUS*3)/zoom_level) , node_y+int(float(WAYPOINT_HANDLE_RADIUS/2)/zoom_level) , x , y ) == true)
			{
				grabbed = WAYPOINT_HANDLE_HORIZONTAL;
			}

			if (MATH_rectangle_to_point_intersect ( node_x-int(float(WAYPOINT_HANDLE_RADIUS/2)/zoom_level) , node_y+int(float(WAYPOINT_HANDLE_RADIUS)/zoom_level) , node_x+int(float(WAYPOINT_HANDLE_RADIUS/2)/zoom_level) , node_y+int(float(WAYPOINT_HANDLE_RADIUS*3)/zoom_level) , x , y ) == true)
			{
				grabbed = WAYPOINT_HANDLE_VERTICAL;
			}

			if (grabbed != UNSET)
			{
				*grabbed_handle = grabbed;
				*grabbed_node = waypoint_number;
				return true;
			}

		}

	}

	return false;
}



int AINODES_find_nearest_waypoint (int tilemap_number, int x, int y, int ignore_me)
{
	int waypoint_number;
	int real_waypoint_x;
	int real_waypoint_y;

	int closest_dist = UNSET;
	int closest_waypoint = UNSET;
	int dist;

	for (waypoint_number=0; waypoint_number<cm[tilemap_number].waypoints; waypoint_number++)
	{
		real_waypoint_x = cm[tilemap_number].waypoint_list_pointer[waypoint_number].x;
		real_waypoint_y = cm[tilemap_number].waypoint_list_pointer[waypoint_number].y;
		
		dist = MATH_get_distance_int (x,y,real_waypoint_x,real_waypoint_y);

		if (((dist < closest_dist) || (closest_dist == UNSET)) && (waypoint_number != ignore_me))
		{
			closest_dist = dist;
			closest_waypoint = waypoint_number;
		}
	}

	return closest_waypoint;
}



void AINODES_draw_waypoints (int tilemap_number, int sx, int sy, int width, int height, float zoom_level, int selected_waypoint, bool nearest, int connection_start_waypoint, int connection_end_waypoint)
{
	int tileset_number;
	int tilesize;

	char text_line[NAME_SIZE];

	int waypoint_number;
	int real_waypoint_x;
	int real_waypoint_y;
	int screen_waypoint_x;
	int screen_waypoint_y;

	int red,green,blue;

	int connection_number;
	int connected_waypoint_index;
	int connection_waypoint_x;
	int connection_waypoint_y;
	int connection_screen_waypoint_x;
	int connection_screen_waypoint_y;

	tileset_number = cm[tilemap_number].tile_set_index;

	if (tileset_number != UNSET)
	{
		tilesize = TILESETS_get_tilesize(tileset_number);
	}
	else
	{
		tilesize = 16;
	}

	for (waypoint_number=0; waypoint_number<cm[tilemap_number].waypoints; waypoint_number++)
	{
		real_waypoint_x = cm[tilemap_number].waypoint_list_pointer[waypoint_number].x;
		real_waypoint_y = cm[tilemap_number].waypoint_list_pointer[waypoint_number].y;

		screen_waypoint_x = int(float(real_waypoint_x - (sx * tilesize)) * zoom_level);
		screen_waypoint_y = int(float(real_waypoint_y - (sy * tilesize)) * zoom_level);

		if (MATH_rectangle_to_point_intersect_by_size (0,0,width,height,screen_waypoint_x,screen_waypoint_y) == true)
		{
			if (cm[tilemap_number].waypoint_list_pointer[waypoint_number].grabbed_status == NODE_SELECTED)
			{
				// It's the selected point
				green = 255;

				if (selected_waypoint == waypoint_number)
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
					OUTPUT_hline (screen_waypoint_x+WAYPOINT_HANDLE_RADIUS , screen_waypoint_y , screen_waypoint_x+WAYPOINT_HANDLE_RADIUS+(WAYPOINT_HANDLE_RADIUS*2) , red , green , blue );
					OUTPUT_line (screen_waypoint_x+WAYPOINT_HANDLE_RADIUS+(WAYPOINT_HANDLE_RADIUS*2) , screen_waypoint_y , screen_waypoint_x+WAYPOINT_HANDLE_RADIUS+(WAYPOINT_HANDLE_RADIUS*2) - (WAYPOINT_HANDLE_RADIUS/2) , screen_waypoint_y - (WAYPOINT_HANDLE_RADIUS/2) , red , green , blue );
					OUTPUT_line (screen_waypoint_x+WAYPOINT_HANDLE_RADIUS+(WAYPOINT_HANDLE_RADIUS*2) , screen_waypoint_y , screen_waypoint_x+WAYPOINT_HANDLE_RADIUS+(WAYPOINT_HANDLE_RADIUS*2) - (WAYPOINT_HANDLE_RADIUS/2) , screen_waypoint_y + (WAYPOINT_HANDLE_RADIUS/2) , red , green , blue );

					// Vertical arrow
					OUTPUT_vline (screen_waypoint_x , screen_waypoint_y+WAYPOINT_HANDLE_RADIUS , screen_waypoint_y+WAYPOINT_HANDLE_RADIUS+(WAYPOINT_HANDLE_RADIUS*2) , red , green , blue );
					OUTPUT_line (screen_waypoint_x , screen_waypoint_y+WAYPOINT_HANDLE_RADIUS+(WAYPOINT_HANDLE_RADIUS*2) , screen_waypoint_x - (WAYPOINT_HANDLE_RADIUS/2) , screen_waypoint_y + WAYPOINT_HANDLE_RADIUS+(WAYPOINT_HANDLE_RADIUS*2) - (WAYPOINT_HANDLE_RADIUS/2) , red , green , blue );
					OUTPUT_line (screen_waypoint_x , screen_waypoint_y+WAYPOINT_HANDLE_RADIUS+(WAYPOINT_HANDLE_RADIUS*2) , screen_waypoint_x + (WAYPOINT_HANDLE_RADIUS/2) , screen_waypoint_y + WAYPOINT_HANDLE_RADIUS+(WAYPOINT_HANDLE_RADIUS*2) - (WAYPOINT_HANDLE_RADIUS/2) , red , green , blue );

					// And then display the position in tile_x, tile_y and offset.
					snprintf (text_line, sizeof(text_line), "X=%i", cm[tilemap_number].waypoint_list_pointer[waypoint_number].x );
					OUTPUT_text (screen_waypoint_x-(16+WAYPOINT_HANDLE_RADIUS), screen_waypoint_y-(16+WAYPOINT_HANDLE_RADIUS) , text_line );
					snprintf (text_line, sizeof(text_line), "Y=%i", cm[tilemap_number].waypoint_list_pointer[waypoint_number].y );
					OUTPUT_text (screen_waypoint_x-(16+WAYPOINT_HANDLE_RADIUS), screen_waypoint_y-(8+WAYPOINT_HANDLE_RADIUS) , text_line );
				}
			}
			else
			{
				red = blue = 255;
				green = 0;
			}

			// Draw relatives

			if (cm[tilemap_number].waypoint_list_pointer[waypoint_number].connections > 0)
			{
				for (connection_number=0; connection_number<cm[tilemap_number].waypoint_list_pointer[waypoint_number].connections; connection_number++)
				{
					connected_waypoint_index = cm[tilemap_number].waypoint_list_pointer[waypoint_number].connection_list_indexes[connection_number];

					connection_waypoint_x = cm[tilemap_number].waypoint_list_pointer[connected_waypoint_index].x;
					connection_waypoint_y = cm[tilemap_number].waypoint_list_pointer[connected_waypoint_index].y;
					connection_screen_waypoint_x = int(float(connection_waypoint_x - (sx * tilesize)) * zoom_level);
					connection_screen_waypoint_y = int(float(connection_waypoint_y - (sy * tilesize)) * zoom_level);

					OUTPUT_draw_arrow (screen_waypoint_x,screen_waypoint_y,connection_screen_waypoint_x,connection_screen_waypoint_y, 255, 0, 0, false);
				}
			}

			OUTPUT_circle(screen_waypoint_x,screen_waypoint_y,WAYPOINT_HANDLE_RADIUS,red,green,blue);
		}
	}

	if (connection_start_waypoint != UNSET)
	{
		connection_waypoint_x = cm[tilemap_number].waypoint_list_pointer[connection_start_waypoint].x;
		connection_waypoint_y = cm[tilemap_number].waypoint_list_pointer[connection_start_waypoint].y;
		connection_screen_waypoint_x = int(float(connection_waypoint_x - (sx * tilesize)) * zoom_level);
		connection_screen_waypoint_y = int(float(connection_waypoint_y - (sy * tilesize)) * zoom_level);

		screen_waypoint_x = connection_screen_waypoint_x;
		screen_waypoint_y = connection_screen_waypoint_y;

		connection_waypoint_x = cm[tilemap_number].waypoint_list_pointer[connection_end_waypoint].x;
		connection_waypoint_y = cm[tilemap_number].waypoint_list_pointer[connection_end_waypoint].y;
		connection_screen_waypoint_x = int(float(connection_waypoint_x - (sx * tilesize)) * zoom_level);
		connection_screen_waypoint_y = int(float(connection_waypoint_y - (sy * tilesize)) * zoom_level);

		OUTPUT_line (screen_waypoint_x,screen_waypoint_y,connection_screen_waypoint_x,connection_screen_waypoint_y, 255, 255, 0);
	}
}



void AINODES_find_nearest_waypoint_connection (int tilemap_number, int sx, int sy, float zoom_level, int tilesize, int x, int y, int *nearest_connection_waypoint_1, int *nearest_connection_waypoint_2)
{
	int waypoint_number;
	int real_waypoint_start_x;
	int real_waypoint_start_y;
	int real_waypoint_end_x;
	int real_waypoint_end_y;

	int i_dummy;
	float f_dummy;

	*nearest_connection_waypoint_1 = UNSET;
	*nearest_connection_waypoint_2 = UNSET;

	int connected_waypoint;

	int closest_dist = UNSET;
	int dist;

	int connection_number;

	for (waypoint_number=0; waypoint_number<cm[tilemap_number].waypoints; waypoint_number++)
	{
		real_waypoint_start_x = cm[tilemap_number].waypoint_list_pointer[waypoint_number].x;
		real_waypoint_start_y = cm[tilemap_number].waypoint_list_pointer[waypoint_number].y;
		
		// Check the start point is on-screen.
		if (MATH_rectangle_to_point_intersect_by_size ( sx*tilesize , sy*tilesize , int(float(editor_view_width)/zoom_level) , int(float(editor_view_height)/zoom_level) , x, y ) == true)
		{
			for (connection_number=0; connection_number<cm[tilemap_number].waypoint_list_pointer[waypoint_number].connections; connection_number++)
			{
				connected_waypoint = cm[tilemap_number].waypoint_list_pointer[waypoint_number].connection_list_indexes[connection_number];
				real_waypoint_end_x = cm[tilemap_number].waypoint_list_pointer[connected_waypoint].x;
				real_waypoint_end_y = cm[tilemap_number].waypoint_list_pointer[connected_waypoint].y;

				dist = int (MATH_distance_to_line ( float (real_waypoint_start_x) , float (real_waypoint_start_y) , float (real_waypoint_end_x) , float (real_waypoint_end_y) , float (x) , float (y) , &i_dummy, &f_dummy));

				if ((dist < closest_dist) || (closest_dist == UNSET))
				{
					closest_dist = dist;
					*nearest_connection_waypoint_1 = waypoint_number;
					*nearest_connection_waypoint_2 = connected_waypoint;
				}
			}
		}

	}
}



#define WAYPOINT_PROPERTY_EDITING_TYPE						(1)
#define WAYPOINT_PROPERTY_EDITING_LOCATION					(2)

#define WAYPOINT_CREATE_MODE								(0)
#define WAYPOINT_EDIT_MODE									(1)
#define WAYPOINT_CHOOSE_CONNECTION							(2)
#define WAYPOINT_DRAGGING_OVER_NODES						(3)
#define WAYPOINT_DELETE_MODE								(4)

bool AINODES_edit_ai_nodes (int state , bool overlay_display, int *current_cursor ,int sx, int sy,  int tilemap_number, int mx, int my, float zoom_level, int grid_size)
{
	static int selected_waypoint;

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

	static char type_word[NAME_SIZE];
	static char location_word[NAME_SIZE];

	static char type_name[NAME_SIZE];
	static char location_name[NAME_SIZE];

	static int first_parameter_in_list;
	static int editing_parameter_number;
	static int total_parameters;

	static int grabbed_waypoint_real_x;
	static int grabbed_waypoint_real_y;
	static int grabbed_real_mx;
	static int grabbed_real_my;

	int relative;

	static int selecting_node_mode;

	int nearest_connection_waypoint_1;
	int nearest_connection_waypoint_2;

	static int editing_property;

	static int getting_new_name_status;

	static int waypoint_editing_mode;
	static int grabbed_waypoint;

	int nearest_waypoint;

	static int grabbed_handle;

	static bool delete_selected;

	int t;

	if (state == STATE_INITIALISE)
	{
		selected_waypoint = UNSET;
		grabbed_waypoint = UNSET;

		override_main_editor = false;

		strcpy (type_word,"AI_NODE_TYPES");
		strcpy (location_word,"LOCATION_TYPES");
		strcpy (type_name,"UNSET");
		strcpy (location_name,"UNSET");

		grabbed_handle = UNSET;

		getting_new_name_status = GET_WORD;

		editing_property = UNSET;

		total_parameters = 0;
		editing_parameter_number = 0;
		first_parameter_in_list = 0;

		delete_selected = false;

		waypoint_editing_mode = WAYPOINT_CREATE_MODE;
	}
	else if (state == STATE_SET_UP_BUTTONS)
	{
		editor_view_width = game_screen_width - (ICON_SIZE*8);
		editor_view_height = game_screen_height - (ICON_SIZE*4);

		SIMPGUI_create_set_button (TILEMAP_EDITOR_SUB_GROUP, EDIT_TILEMAP, &waypoint_editing_mode, editor_view_width+(ICON_SIZE/2)+(2*ICON_SIZE) , ICON_SIZE*1, first_icon , NEW_WAYPOINT_ICON, LMB, WAYPOINT_CREATE_MODE);
		SIMPGUI_create_set_button (TILEMAP_EDITOR_SUB_GROUP, EDIT_TILEMAP, &waypoint_editing_mode, editor_view_width+(ICON_SIZE/2)+(4*ICON_SIZE) , ICON_SIZE*1, first_icon , EDIT_WAYPOINT_ICON, LMB, WAYPOINT_EDIT_MODE);
		SIMPGUI_create_set_button (TILEMAP_EDITOR_SUB_GROUP, EDIT_TILEMAP, &waypoint_editing_mode, editor_view_width+(ICON_SIZE/2)+(6*ICON_SIZE) , ICON_SIZE*1, first_icon , ERASE_WAYPOINT_ICON, LMB, WAYPOINT_DELETE_MODE);
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
		tx = (mx+(tilesize/2)) / int(float(tilesize)*zoom_level);
		ty = (my+(tilesize/2)) / int(float(tilesize)*zoom_level);

		real_tx = tx + sx;
		real_ty = ty + sy;

		real_mx = int(float(mx)/zoom_level) + (sx*tilesize);
		real_my = int(float(my)/zoom_level) + (sy*tilesize);

		nearest_waypoint = AINODES_find_nearest_waypoint (tilemap_number, real_mx, real_my, selected_waypoint);
		AINODES_find_nearest_waypoint_connection (tilemap_number, sx,sy,zoom_level,tilesize,real_mx, real_my, &nearest_connection_waypoint_1, &nearest_connection_waypoint_2);

		if (tileset_number != UNSET)
		{
			TILEMAPS_draw ( 0 , cm[tilemap_number].map_layers , tilemap_number , sx , sy , editor_view_width , editor_view_height , zoom_level, COLOUR_DISPLAY_MODE_NONE);
			SPAWNPOINTS_draw_spawn_points (tilemap_number, sx, sy, editor_view_width, editor_view_height, zoom_level, UNSET, false, true);
		}

		TILEMAPS_draw_guides(tilemap_number,sx,sy,tilesize,zoom_level,guide_box_width,guide_box_height);

		if ( (waypoint_editing_mode == WAYPOINT_CREATE_MODE) || (waypoint_editing_mode == WAYPOINT_DELETE_MODE) )
		{
			selected_waypoint = UNSET;
		}

		if (selected_waypoint == UNSET)
		{
			if (waypoint_editing_mode == WAYPOINT_CREATE_MODE)
			{
				*current_cursor = CREATE_ARROW;
				AINODES_draw_waypoints (tilemap_number, sx, sy, editor_view_width, editor_view_height, zoom_level, UNSET , true, UNSET, UNSET);
			}
			else if (waypoint_editing_mode == WAYPOINT_DELETE_MODE)
			{
				*current_cursor = DELETE_ARROW;
				if (CONTROL_key_down(KEY_LSHIFT) == false)
				{
					AINODES_draw_waypoints (tilemap_number, sx, sy, editor_view_width, editor_view_height, zoom_level, nearest_waypoint , true, UNSET, UNSET);
				}
				else
				{
					AINODES_draw_waypoints (tilemap_number, sx, sy, editor_view_width, editor_view_height, zoom_level, UNSET , true, nearest_connection_waypoint_1, nearest_connection_waypoint_2);
				}
			}
			else
			{
				AINODES_draw_waypoints (tilemap_number, sx, sy, editor_view_width, editor_view_height, zoom_level, nearest_waypoint , true, UNSET, UNSET);
			}
		}
		else
		{
			if (waypoint_editing_mode != WAYPOINT_CHOOSE_CONNECTION)
			{
				AINODES_draw_waypoints (tilemap_number, sx, sy, editor_view_width, editor_view_height, zoom_level, selected_waypoint , false, UNSET, UNSET);
			}
			else
			{
				// If we're choosing a relative for the selected spawn point then display the nearest one
				// instead of the selected one.
				AINODES_draw_waypoints (tilemap_number, sx, sy, editor_view_width, editor_view_height, zoom_level, nearest_waypoint , true, UNSET, UNSET);
			}
		}

		if (overlay_display)
		{
			editor_view_width = game_screen_width - (ICON_SIZE*8);
			editor_view_height = game_screen_height;

			OUTPUT_filled_rectangle (editor_view_width,0,game_screen_width,editor_view_height,0,0,0);

			OUTPUT_vline (editor_view_width,0,editor_view_height,255,255,255);

			OUTPUT_draw_masked_sprite ( first_icon , EDIT_MODE_ICON, editor_view_width+(ICON_SIZE/2)+(0*ICON_SIZE) , ICON_SIZE*1 );

			if (waypoint_editing_mode==WAYPOINT_CREATE_MODE)
			{
 				OUTPUT_draw_masked_sprite ( first_icon , FORWARD_ARROW, editor_view_width+(ICON_SIZE/2)+(1*ICON_SIZE) , ICON_SIZE*1 );
			}
			else if (waypoint_editing_mode==WAYPOINT_EDIT_MODE)
			{
 				OUTPUT_draw_masked_sprite ( first_icon , FORWARD_ARROW, editor_view_width+(ICON_SIZE/2)+(3*ICON_SIZE) , ICON_SIZE*1 );
			}
			else if (waypoint_editing_mode==WAYPOINT_DELETE_MODE)
			{
 				OUTPUT_draw_masked_sprite ( first_icon , FORWARD_ARROW, editor_view_width+(ICON_SIZE/2)+(5*ICON_SIZE) , ICON_SIZE*1 );
			}

			OUTPUT_centred_text ( editor_view_width+(ICON_SIZE*4) , (ICON_SIZE-FONT_HEIGHT)/2 , "WAYPOINT EDITOR" );

			SIMPGUI_draw_all_buttons (TILESET_SUB_GROUP_ID,true);

			if (selected_waypoint >= 0)
			{
				// Show spawn point type.

				OUTPUT_centred_text (editor_view_width+(ICON_SIZE*4),(ICON_SIZE*2)+12,"WAYPOINT TYPE & LOCATION");
				OUTPUT_rectangle ( editor_view_width+(ICON_SIZE/2), (ICON_SIZE*3), game_screen_width-(ICON_SIZE/2), (ICON_SIZE*4) , 0 , 0 , 255 );
				OUTPUT_truncated_text ( ((ICON_SIZE*7)/FONT_WIDTH)-1, editor_view_width+(ICON_SIZE/2)+(FONT_WIDTH/2) , (ICON_SIZE*3)+12 , cm[tilemap_number].waypoint_list_pointer[selected_waypoint].type , 255, 255, 255 );
				OUTPUT_rectangle ( editor_view_width+(ICON_SIZE/2), (ICON_SIZE*4), game_screen_width-(ICON_SIZE/2), (ICON_SIZE*5) , 0 , 0 , 255 );
				OUTPUT_truncated_text ( ((ICON_SIZE*7)/FONT_WIDTH)-1, editor_view_width+(ICON_SIZE/2)+(FONT_WIDTH/2) , (ICON_SIZE*4)+12 , cm[tilemap_number].waypoint_list_pointer[selected_waypoint].location , 255, 255, 255 );

				// Show COPY icon and it's options

				OUTPUT_draw_masked_sprite ( first_icon , COPY_WAYPOINT_ICON, editor_view_width+(ICON_SIZE/2) , ICON_SIZE*6 );

				for (t=1; t<3; t++)
				{
					OUTPUT_draw_masked_sprite ( first_icon , (WITH_CONNECT_ICON-1)+t, editor_view_width+(ICON_SIZE/2)+(ICON_SIZE*t), ICON_SIZE*6 );
				}

				OUTPUT_draw_masked_sprite ( first_icon , LINK_WAYPOINT_ICON, editor_view_width+(ICON_SIZE/2)+(ICON_SIZE*4), ICON_SIZE*6 );

				OUTPUT_draw_masked_sprite ( first_icon , ERASE_WAYPOINT_ICON, editor_view_width+(ICON_SIZE/2)+(ICON_SIZE*6), ICON_SIZE*6 );
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

			if (waypoint_editing_mode == WAYPOINT_CREATE_MODE)
			{
				OUTPUT_circle ( mx - (mx % int(float(grid_size) * zoom_level)) , my - (my % int(float(grid_size) * zoom_level)) , WAYPOINT_HANDLE_RADIUS-1 , 0 , 0 , 255 );
			}

			if (CONTROL_mouse_hit(LMB) == true)
			{
				if (waypoint_editing_mode == WAYPOINT_EDIT_MODE)
				{
					if ( (AINODES_what_selected_waypoint_thing_has_been_grabbed (tilemap_number, real_mx, real_my, zoom_level, &grabbed_handle, &grabbed_waypoint) == false) || (CONTROL_key_down(KEY_LSHIFT) == true) || (CONTROL_key_down(KEY_LCONTROL) == true) )
					{
						// Failed to grab a handle or purposely dragging multiple spawn points.

						waypoint_editing_mode = WAYPOINT_DRAGGING_OVER_NODES;

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

						AINODES_get_grabbed_waypoint_co_ords (tilemap_number);
					}
				}
				else if (waypoint_editing_mode == WAYPOINT_CREATE_MODE)
				{
					AINODES_create_waypoint (tilemap_number,real_mx-(real_mx%grid_size),real_my-(real_my%grid_size));
				}
				else if (waypoint_editing_mode == WAYPOINT_CHOOSE_CONNECTION)
				{
					if ( (nearest_waypoint != UNSET) && (selected_waypoint >= 0) )
					{
						AINODES_add_waypoint_connection (tilemap_number,selected_waypoint,nearest_waypoint);
						// If we already have this connection then the following will clear it away...
						AINODES_remove_duplicate_waypoint_connections (tilemap_number,selected_waypoint);
						AINODES_remove_duplicate_waypoint_connections (tilemap_number,nearest_waypoint);
						AINODES_relink_all_waypoint_connections (tilemap_number);
					}
				}
				else if (waypoint_editing_mode == WAYPOINT_DELETE_MODE)
				{
					if (CONTROL_key_down(KEY_LSHIFT) == false)
					{
						if (nearest_waypoint != UNSET)
						{
							AINODES_delete_particular_waypoint (tilemap_number,nearest_waypoint);
							AINODES_relink_all_waypoint_connections (tilemap_number);
						}
					}
					else
					{
						if (nearest_connection_waypoint_1 != UNSET)
						{
							AINODES_remove_waypoint_connection (tilemap_number,nearest_connection_waypoint_1,nearest_connection_waypoint_2);
							AINODES_remove_waypoint_connection (tilemap_number,nearest_connection_waypoint_2,nearest_connection_waypoint_1);
						}
					}
				}
			}

			if (CONTROL_mouse_hit(RMB) == true)
			{
				// Deselect nodes or leave creation mode...

				if (waypoint_editing_mode == WAYPOINT_CREATE_MODE)
				{
					waypoint_editing_mode = WAYPOINT_EDIT_MODE;
				}
				else if (waypoint_editing_mode == WAYPOINT_CHOOSE_CONNECTION)
				{
					waypoint_editing_mode = WAYPOINT_EDIT_MODE;
				}
				else if (waypoint_editing_mode == WAYPOINT_EDIT_MODE)
				{
					AINODES_deselect_waypoints (tilemap_number);
				}
				else if (waypoint_editing_mode == WAYPOINT_DELETE_MODE)
				{
					waypoint_editing_mode = WAYPOINT_EDIT_MODE;
				}
			}

			if (CONTROL_mouse_down(LMB) == true)
			{
				if (waypoint_editing_mode == WAYPOINT_DRAGGING_OVER_NODES)
				{
					// Continue dragging the box...

					selected_waypoint = UNSET;

					drag_end_x = real_mx;
					drag_end_y = real_my;

					screen_x1 = int(float(drag_start_x - (sx * tilesize)) * zoom_level);
					screen_y1 = int(float(drag_start_y - (sy * tilesize)) * zoom_level);
					screen_x2 = int(float(drag_end_x - (sx * tilesize)) * zoom_level);
					screen_y2 = int(float(drag_end_y - (sy * tilesize)) * zoom_level);

					drag_time++;

					OUTPUT_rectangle (screen_x1,screen_y1,screen_x2,screen_y2,255,255,255);

					AINODES_partially_set_waypoint_status (tilemap_number, drag_start_x, drag_start_y, drag_end_x, drag_end_y, zoom_level, selecting_node_mode, drag_time);
				}
				else if (waypoint_editing_mode == WAYPOINT_EDIT_MODE)
				{
					if (grabbed_handle == WAYPOINT_HANDLE_CENTER)
					{
						AINODES_position_selected_waypoints (tilemap_number, (real_mx - grabbed_real_mx), (real_my - grabbed_real_my), grabbed_waypoint, grid_size, true);
					}

					if (grabbed_handle == WAYPOINT_HANDLE_HORIZONTAL)
					{
						AINODES_position_selected_waypoints (tilemap_number, (real_mx - grabbed_real_mx), 0, grabbed_waypoint, grid_size, false);
					}

					if (grabbed_handle == WAYPOINT_HANDLE_VERTICAL)
					{
						AINODES_position_selected_waypoints (tilemap_number, 0, (real_my - grabbed_real_my), grabbed_waypoint, grid_size, false);
					}
				}

			}
			else
			{
				if (waypoint_editing_mode == WAYPOINT_DRAGGING_OVER_NODES)
				{
					selected_waypoint = AINODES_lock_waypoint_status (tilemap_number);
					waypoint_editing_mode = WAYPOINT_EDIT_MODE;
				}
				else if (waypoint_editing_mode == WAYPOINT_EDIT_MODE)
				{
					grabbed_handle = UNSET;
				}
			}

		}

		// Control panel stuff! First the script stuff!

		if (selected_waypoint >= 0)
		{
			// Only allow alteration of script properties if we have a spawn point selected.

			// First of all, read in all the current script properties...

			strcpy ( type_name , cm[tilemap_number].waypoint_list_pointer[selected_waypoint].type );
			strcpy ( location_name , cm[tilemap_number].waypoint_list_pointer[selected_waypoint].location );

			if ( (override_main_editor == true) && (selected_waypoint != UNSET) )
			{
				if (editing_property == WAYPOINT_PROPERTY_EDITING_TYPE)
				{
					override_main_editor = EDIT_gpl_list_menu (80, 48, type_word , type_name, true);
				}

				if (editing_property == WAYPOINT_PROPERTY_EDITING_LOCATION)
				{
					override_main_editor = EDIT_gpl_list_menu (80, 48, location_word , location_name, true);
				}

				if (override_main_editor == false)
				{
					CONTROL_lock_mouse_button (LMB);
				}
			}

			if (selected_waypoint >= 0) // Might have been deleted - so check!
			{
				// After all that, write all the gubbins back into the spawn point...

				strcpy ( cm[tilemap_number].waypoint_list_pointer[selected_waypoint].type , type_name );
				strcpy ( cm[tilemap_number].waypoint_list_pointer[selected_waypoint].location , location_name );
			}

			// Then deal with input...

			if ( (selected_waypoint >= 0) && (CONTROL_mouse_hit(LMB) == true) && ( (my>editor_view_height) || (mx>editor_view_width) ) )
			{
				if (MATH_rectangle_to_point_intersect ( editor_view_width+(ICON_SIZE/2), (ICON_SIZE*3) ,game_screen_width-(ICON_SIZE/2) , (ICON_SIZE*4) ,mx,my) == true)
				{
					// Clicking to alter the spawn point type.
					editing_property = WAYPOINT_PROPERTY_EDITING_TYPE;
					override_main_editor = true;
					CONTROL_lock_mouse_button (LMB);
				}

				if (MATH_rectangle_to_point_intersect ( editor_view_width+(ICON_SIZE/2), (ICON_SIZE*4) ,game_screen_width-(ICON_SIZE/2) , (ICON_SIZE*5) ,mx,my) == true)
				{
					// Clicking to alter the spawn point type.
					editing_property = WAYPOINT_PROPERTY_EDITING_LOCATION;
					override_main_editor = true;
					CONTROL_lock_mouse_button (LMB);
				}

				if (MATH_rectangle_to_point_intersect ( editor_view_width+(ICON_SIZE/2), (ICON_SIZE*6) ,editor_view_width+(ICON_SIZE/2)+(ICON_SIZE*4)-1 , (ICON_SIZE*7) ,mx,my) == true)
				{
					relative = (mx-(editor_view_width+(ICON_SIZE/2)))/ICON_SIZE;
					selected_waypoint = AINODES_copy_waypoint (tilemap_number,selected_waypoint,relative, grid_size);
					AINODES_relink_all_waypoint_connections (tilemap_number);
				}

				if (MATH_rectangle_to_point_intersect ( editor_view_width+(ICON_SIZE/2)+(ICON_SIZE*4), (ICON_SIZE*6) ,editor_view_width+(ICON_SIZE/2)+(ICON_SIZE*5) , (ICON_SIZE*7) ,mx,my) == true)
				{
					waypoint_editing_mode = WAYPOINT_CHOOSE_CONNECTION;
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

