#include <stdio.h>
#include <string.h>
#include <allegro.h>
#include <math.h>
#include <assert.h>
#include <allegro.h>

#include "scripting.h"
#include "parser.h"
#include "graphics.h"
#include "new_editor.h"
#include "main.h"
#include "control.h"
#include "math_stuff.h"
#include "string_stuff.h"
#include "simple_gui.h"
#include "game.h"
#include "paths.h"
#include "global_param_list.h"
#include "output.h"
#include "tilemaps.h"
#include "tilesets.h"
#include "allegro.h"

#include "fortify.h"

int total_path_percent_size = 1000000;

path *pt = NULL;

int number_of_paths_loaded = 0;

int lookup_path_resolution = 1; // The distance in pixels between each bit of lookup path data.
	// The lower the number, the more memory used and the more accurate the path interpolation.

#define PATHS_TYPES				(4)
char path_type_names [PATHS_TYPES][16] = { "LINEAR" , "CATMULL" , "BEZIER" , "SIMPLE" };

#define LOOP_TYPES				(4)
char path_looping_behaviour_name  [LOOP_TYPES][16] = { "NO_LOOP" , "LOOP_TO_START" , "LOOP_ON_END" , "PING_PONG" };

float speed_curve_bias_tables[MAX_SPEED_CURVE_TYPES*MAX_SPEED_CURVE_TYPES][100];



void PATHS_destroy (int path_number)
{
	if (pt[path_number].node_list_pointer != NULL)
	{
		free (pt[path_number].node_list_pointer);
		pt[path_number].node_list_pointer = NULL;
	}

	if (pt[path_number].lp_node_list_pointer != NULL)
	{
		free (pt[path_number].lp_node_list_pointer);
		pt[path_number].lp_node_list_pointer = NULL;
	}
}



void PATHS_destroy_all (void)
{
	int path_number;

	for (path_number = 0; path_number<number_of_paths_loaded ; path_number++)
	{
		PATHS_destroy (path_number);
	}

	free (pt);

	number_of_paths_loaded = 0;
}



void PATHS_swap_paths (int p1,int p2)
{
	path temp;
	
	temp = pt[p1];
	pt[p1] = pt[p2];
	pt[p2] = temp;
}



void PATHS_delete_particular_path (int path_number)
{
	int p;

	for (p=path_number; p<number_of_paths_loaded-1; p++)
	{
		PATHS_swap_paths(p,p+1);
	}

	PATHS_destroy (number_of_paths_loaded-1);

	if (number_of_paths_loaded == 1)
	{
		free (pt);
		pt = NULL;
	}
	else
	{
		pt = (path *) realloc ( pt, (number_of_paths_loaded - 1) * sizeof (path) );
	}

	number_of_paths_loaded--;
}



#define DEFAULT_CONTROL_POINT_OFFSET		(16)

void PATHS_create_path_node (int path_number, int x, int y)
{
	int PATHS_length;

	PATHS_length = pt[path_number].nodes;

	if ( (PATHS_length == 0) && (pt[path_number].node_list_pointer == NULL) )
	{
		pt[path_number].node_list_pointer = (node *) malloc (sizeof(node));
	}
	else
	{
		pt[path_number].node_list_pointer = (node *) realloc ( pt[path_number].node_list_pointer , (PATHS_length+1) * sizeof(node) );
	}

	pt[path_number].node_list_pointer[PATHS_length].aft_cx = DEFAULT_CONTROL_POINT_OFFSET;
	pt[path_number].node_list_pointer[PATHS_length].aft_cy = DEFAULT_CONTROL_POINT_OFFSET;

	pt[path_number].node_list_pointer[PATHS_length].pre_cx = -DEFAULT_CONTROL_POINT_OFFSET;
	pt[path_number].node_list_pointer[PATHS_length].pre_cy = -DEFAULT_CONTROL_POINT_OFFSET;

	strcpy (pt[path_number].node_list_pointer[PATHS_length].flag_name,"UNSET");
	pt[path_number].node_list_pointer[PATHS_length].flag_value = UNSET;
	pt[path_number].node_list_pointer[PATHS_length].flag_used = false;

	pt[path_number].node_list_pointer[PATHS_length].length = UNSET;

	pt[path_number].node_list_pointer[PATHS_length].grabbed_status = NODE_UNSELECTED;
	pt[path_number].node_list_pointer[PATHS_length].grabbed_x = x;
	pt[path_number].node_list_pointer[PATHS_length].grabbed_y = y;

	pt[path_number].node_list_pointer[PATHS_length].speed_in_curve_type = SPEED_CURVE_LINEAR;
	pt[path_number].node_list_pointer[PATHS_length].speed_out_curve_type = SPEED_CURVE_LINEAR;

	pt[path_number].node_list_pointer[PATHS_length].speed = 100;

	pt[path_number].node_list_pointer[PATHS_length].line_segments = 4;

	pt[path_number].node_list_pointer[PATHS_length].x = x;
	pt[path_number].node_list_pointer[PATHS_length].y = y;

	pt[path_number].nodes++;
}



void PATHS_create_path_node_global_co_ord (int path_number, int x, int y)
{
	int remainder_x,remainder_y;
	
	remainder_x = x - pt[path_number].x;
	remainder_y = y - pt[path_number].y;

	PATHS_create_path_node (path_number, remainder_x, remainder_y);
}



void PATHS_alter_path_details (int path_number, int amount)
{
	int node_number;

	for (node_number=0; node_number<pt[path_number].nodes; node_number++)
	{
		if (pt[path_number].node_list_pointer[node_number].grabbed_status == NODE_SELECTED)
		{
			pt[path_number].node_list_pointer[node_number].line_segments += amount;
			if (pt[path_number].node_list_pointer[node_number].line_segments < 1)
			{
				pt[path_number].node_list_pointer[node_number].line_segments = 1;
			}
		}
	}
}



void PATHS_alter_speed_details (int path_number, int amount)
{
	int node_number;

	for (node_number=0; node_number<pt[path_number].nodes; node_number++)
	{
		if (pt[path_number].node_list_pointer[node_number].grabbed_status == NODE_SELECTED)
		{
			pt[path_number].node_list_pointer[node_number].speed += amount;
			if (pt[path_number].node_list_pointer[node_number].speed < 0)
			{
				pt[path_number].node_list_pointer[node_number].speed = 0;
			}
		}
	}
}


void PATHS_swap_path_nodes (int path_number, int p1, int p2)
{
	node temp;

	temp = pt[path_number].node_list_pointer[p1];
	pt[path_number].node_list_pointer[p1] = pt[path_number].node_list_pointer[p2];
	pt[path_number].node_list_pointer[p2] = temp;
}



void PATHS_zero_origin (int path_number)
{
	// In any path, the first node should be at 0,0. If it isn't then all
	// subsequent nodes need to be adjusted so that the first one is at 0,0
	// and the path's own position needs to be altered.

	int first_offset_x;
	int first_offset_y;
	int p;

	first_offset_x = pt[path_number].node_list_pointer[0].x;
	first_offset_y = pt[path_number].node_list_pointer[0].y;

	for (p=0; p<pt[path_number].nodes; p++)
	{
		pt[path_number].node_list_pointer[p].x -= first_offset_x;
		pt[path_number].node_list_pointer[p].y -= first_offset_y;
	}

	pt[path_number].x += first_offset_x;
	pt[path_number].y += first_offset_y;
}



void PATHS_create_particular_path_node (int path_number, int x, int y, int position)
{
	// Creates a new node on the end of the stack and then bubbles it down
	// to the correct position, spilling everyone's popcorn and drinks as it
	// awkwardly shuffles by.

	int n;

	PATHS_create_path_node (path_number, x, y);

	for (n=pt[path_number].nodes-1; n>position; n--)
	{
		PATHS_swap_path_nodes (path_number, n, n-1);
	}

	if (position == 0)
	{
		PATHS_zero_origin (path_number);
	}
}



void PATHS_create_midpoint_path_node ( int path_number , int node_number )
{
	int x1,y1,x2,y2;
	int mid_x,mid_y;

	x1 = pt[path_number].node_list_pointer[node_number].x;
	y1 = pt[path_number].node_list_pointer[node_number].y;

	x2 = pt[path_number].node_list_pointer[node_number+1].x;
	y2 = pt[path_number].node_list_pointer[node_number+1].y;

	mid_x = (x1+x2) / 2;
	mid_y = (y1+y2) / 2;

	PATHS_create_particular_path_node (path_number, mid_x, mid_y, node_number+1);
}



void PATHS_draw_midpoint_co_ords ( int path_number , int node_number , int start_x, int start_y, int tilesize, int zoom_level)
{
	int x1,y1,x2,y2;
	int mid_x,mid_y;
	
	static int frame_counter = 0;

	frame_counter++;

	x1 = pt[path_number].node_list_pointer[node_number].x;
	y1 = pt[path_number].node_list_pointer[node_number].y;

	x2 = pt[path_number].node_list_pointer[node_number+1].x;
	y2 = pt[path_number].node_list_pointer[node_number+1].y;

	mid_x = (x1+x2) / 2;
	mid_y = (y1+y2) / 2;
	
	mid_x += pt[path_number].x;
	mid_y += pt[path_number].y;

	mid_x -= (start_x*tilesize);
	mid_y -= (start_y*tilesize);
	mid_x *= zoom_level;
	mid_y *= zoom_level;

	OUTPUT_circle (mid_x,mid_y,(frame_counter % PATH_NODE_HANDLE_SIZE),255,255,255);
}



bool PATHS_delete_particular_path_node (int path_number, int PATHS_node)
{
	// Just shuffles the given node to the end of the stack and then zaps it off by reallocing
	// the node_list_pointer to be 1 node shorter.

	int n;
	int PATHS_length;

	PATHS_length = pt[path_number].nodes;

	if (PATHS_length == 1)
	{
		// Okay, there's only one node in it so we ain't deleting it. If the user
		// wants rid of the path they need to delete the path, not a node within it.

		return true; // But we're kind enough to tell them that's what they want to do.
	}
	else
	{
		for (n=PATHS_node; n<PATHS_length-1; n++)
		{
			PATHS_swap_path_nodes (path_number,n,n+1);
		}

		// After that we de-allocate the last node into oblivion.

		pt[path_number].node_list_pointer = (node *) realloc ( pt[path_number].node_list_pointer , (PATHS_length-1) * sizeof(node) );
		pt[path_number].nodes--;
	}

	// Just in case we snipped off the first node.
	if (PATHS_node == 0)
	{
		PATHS_zero_origin (path_number);
	}

	return false;

}



void PATHS_get_free_name (char *name)
{
	// This checks to see if any paths have names of the type "Path_#xxxx" where xxxx is a 4-digit
	// number. It finds the first free slot and plonks it into the supplied string.

	int i;
	int test_num;
	bool found_one;

	test_num = 0;

	do 
	{
		found_one = false;

		for (i=0; i<number_of_paths_loaded; i++)
		{
			sprintf(name,"PATH_%4d",test_num);
			STRING_replace_char (name, ' ' , '0');
			
			if (strcmp(name,pt[i].name) == 0)
			{
				found_one = true;
				test_num++;
			}
			// As the path list is alphabetised it should mean that test_num ends
			// up pointing to the first free name of the "Tileset_#num" variety.
		}
	}
	while (found_one);

	sprintf(name,"PATH_%4d",test_num);
	STRING_replace_char (name, ' ' , '0');
}



int PATHS_create (bool new_path)
{
	// Perhaps unexpectedly this creates a new path, but no nodes. So you really should do that that
	// straight afterwards. Go on! Do it now!

	char default_name[NAME_SIZE];

	if (pt == NULL) // first in list...
	{
		pt = (path *) malloc (sizeof (path));
	}
	else // Already someone in the list...
	{
		pt = (path *) realloc ( pt, (number_of_paths_loaded + 1) * sizeof (path) );
	}

	pt[number_of_paths_loaded].looped = false;
	pt[number_of_paths_loaded].x = 0;
	pt[number_of_paths_loaded].y = 0;
	pt[number_of_paths_loaded].nodes = 0;
	pt[number_of_paths_loaded].node_list_pointer = NULL;
	pt[number_of_paths_loaded].lp_nodes = 0;
	pt[number_of_paths_loaded].lp_node_list_pointer = NULL;
	pt[number_of_paths_loaded].line_type = CATMULL_ROM;
	pt[number_of_paths_loaded].loop_type = LOOP_TO_START;
	pt[number_of_paths_loaded].looped = false;
	pt[number_of_paths_loaded].total_distance = 0;

	PATHS_get_free_name(default_name);

	if (new_path) // ie, we're not just freeing up space for a loaded tileset, but rather it's a brand-spanking-new one.
	{
		strcpy (pt[number_of_paths_loaded].name , default_name );
		strcpy (pt[number_of_paths_loaded].old_name , default_name );
	}
	else
	{
		strcpy (pt[number_of_paths_loaded].name , "UNSET" );
		strcpy (pt[number_of_paths_loaded].old_name , "UNSET" );
	}

	number_of_paths_loaded++;

	return (number_of_paths_loaded-1);
}



void PATHS_load (char *filename , int path_number)
{
	int node_number;
	int t;

	char line[TEXT_LINE_SIZE];

	char full_filename [TEXT_LINE_SIZE];

	bool exitflag;

	char *pointer;

	append_filename (full_filename,"paths", filename, sizeof(full_filename) );

	FILE *file_pointer = fopen (MAIN_get_project_filename (full_filename),"r");

	if (file_pointer != NULL) // It's the path data.
	{

		// Get line type...
		fgets ( line , TEXT_LINE_SIZE , file_pointer );
		STRING_strip_all_disposable(line);
		pointer = STRING_end_of_string(line,"#PATH TYPE = ");

		for (t=0;t<PATHS_TYPES;t++)
		{
			if (strcmp(pointer,&path_type_names[t][0]) == 0)
			{
				pt[path_number].line_type = t;
			}
		}

		// Get looping behaviour...
		fgets ( line , TEXT_LINE_SIZE , file_pointer );
		STRING_strip_all_disposable(line);

		pointer = STRING_end_of_string(line,"#LOOPING TYPE = ");

		for (t=0;t<PATHS_TYPES;t++)
		{
			if (strcmp(pointer,path_looping_behaviour_name[t]) == 0)
			{
				pt[path_number].loop_type = t;
			}
		}

		// Then get looped status...
		fgets ( line , MAX_LINE_SIZE , file_pointer );
		pointer = STRING_end_of_string(line,"#LOOPED = ");
		pointer = strtok (pointer,"\n");

		if (strcmp(pointer,"TRUE") == 0)
		{
			pt[path_number].looped = true;
		}
		else
		{
			pt[path_number].looped = false;
		}

		// Get map position...
		fgets ( line , TEXT_LINE_SIZE , file_pointer );
		pointer = STRING_end_of_string(line,"#INITIAL X POSITION = ");
		pt[path_number].x = atoi (pointer);

		fgets ( line , TEXT_LINE_SIZE , file_pointer );
		pointer = STRING_end_of_string(line,"#INITIAL Y POSITION = ");
		pt[path_number].y = atoi (pointer);

		// Get node data itself...
		fgets ( line , TEXT_LINE_SIZE , file_pointer );
		pointer = STRING_end_of_string(line,"#NODE DATA = ");
		if (pointer != NULL)
		{
			pt[path_number].nodes = atoi(pointer);

			pt[path_number].node_list_pointer = (node *) malloc ( pt[path_number].nodes * sizeof(node) );

			node_number = 0;
			exitflag = false;

			while (exitflag == false)
			{	
				fgets ( line , TEXT_LINE_SIZE , file_pointer );
				STRING_strip_newlines (line);

				pointer = STRING_end_of_string(line,"#NODE NUMBER = ");
				if (pointer != NULL)
				{
					node_number = atoi(pointer);
					pt[path_number].node_list_pointer[node_number].pre_cx = -16.0;
					pt[path_number].node_list_pointer[node_number].pre_cy = -16.0;
					pt[path_number].node_list_pointer[node_number].aft_cx = 16.0;
					pt[path_number].node_list_pointer[node_number].aft_cy = 16.0;
					
					pt[path_number].node_list_pointer[node_number].speed_in_curve_type = SPEED_CURVE_LINEAR;
					pt[path_number].node_list_pointer[node_number].speed_out_curve_type = SPEED_CURVE_LINEAR;
				}

				pointer = STRING_end_of_string(line,"#X POS = ");
				if (pointer != NULL)
				{
					pt[path_number].node_list_pointer[node_number].x = atoi(pointer);
				}

				pointer = STRING_end_of_string(line,"#Y POS = ");
				if (pointer != NULL)
				{
					pt[path_number].node_list_pointer[node_number].y = atoi(pointer);
				}

				pointer = STRING_end_of_string(line,"#PRE X POS = ");
				if (pointer != NULL)
				{
					pt[path_number].node_list_pointer[node_number].pre_cx = atoi(pointer);
				}

				pointer = STRING_end_of_string(line,"#PRE Y POS = ");
				if (pointer != NULL)
				{
					pt[path_number].node_list_pointer[node_number].pre_cy = atoi(pointer);
				}

				pointer = STRING_end_of_string(line,"#AFT X POS = ");
				if (pointer != NULL)
				{
					pt[path_number].node_list_pointer[node_number].aft_cx = atoi(pointer);
				}

				pointer = STRING_end_of_string(line,"#AFT Y POS = ");
				if (pointer != NULL)
				{
					pt[path_number].node_list_pointer[node_number].aft_cy = atoi(pointer);
				}
				
				pointer = STRING_end_of_string(line,"#FLAG = ");
				if (pointer != NULL)
				{
					strcpy (pt[path_number].node_list_pointer[node_number].flag_name , pointer);
				}

				pointer = STRING_end_of_string(line,"#SEGMENTS = ");
				if (pointer != NULL)
				{
					pt[path_number].node_list_pointer[node_number].line_segments = atoi(pointer);
				}

				pointer = STRING_end_of_string(line,"#SPEED = ");
				if (pointer != NULL)
				{
					pt[path_number].node_list_pointer[node_number].speed = atoi(pointer);
				}

				pointer = STRING_end_of_string(line,"#SPEED IN CURVE = ");
				if (pointer != NULL)
				{
					pt[path_number].node_list_pointer[node_number].speed_in_curve_type = atoi(pointer);
				}

				pointer = STRING_end_of_string(line,"#SPEED OUT CURVE = ");
				if (pointer != NULL)
				{
					pt[path_number].node_list_pointer[node_number].speed_out_curve_type = atoi(pointer);
				}

				pointer = STRING_end_of_string(line,"#END OF NODE DATA");
				if (pointer != NULL)
				{
					exitflag = true;
				}
			}
		}

		fclose(file_pointer);

		// Oh, and bung the capitalised name in. Haha.
		strupr(filename);
		strtok(filename,"."); // Get rid of .txt extension...
		strcpy ( pt[path_number].name , filename );
		strcpy ( pt[path_number].old_name , filename );
	}

}



void PATHS_load_from_datafile (char *filename , int path_number)
{
	int node_number;
	int t;

	char line[TEXT_LINE_SIZE];

	char full_filename [TEXT_LINE_SIZE];

	sprintf (full_filename,"%s\\paths.dat#%s",MAIN_get_project_name(),filename);
	fix_filename_slashes(full_filename);
	bool exitflag;

	char *pointer;

	PACKFILE *packfile_pointer = pack_fopen (full_filename,"r");

	if (packfile_pointer != NULL) // It's the path data.
	{

		// Get line type...
		pack_fgets ( line , TEXT_LINE_SIZE , packfile_pointer );
		pointer = STRING_end_of_string(line,"#PATH TYPE = ");
		pointer = strtok (pointer,"\n");

		for (t=0;t<PATHS_TYPES;t++)
		{
			if (strcmp(pointer,&path_type_names[t][0]) == 0)
			{
				pt[path_number].line_type = t;
			}
		}

		// Get looping behaviour...
		pack_fgets ( line , TEXT_LINE_SIZE , packfile_pointer );
		pointer = STRING_end_of_string(line,"#LOOPING TYPE = ");

		for (t=0;t<PATHS_TYPES;t++)
		{
			if (strcmp(pointer,&path_looping_behaviour_name[t][0]) == 0)
			{
				pt[path_number].loop_type = t;
			}
		}

		// Then get looped status...
		pack_fgets ( line , MAX_LINE_SIZE , packfile_pointer );
		pointer = STRING_end_of_string(line,"#LOOPED = ");
		pointer = strtok (pointer,"\n");

		if (strcmp(pointer,"TRUE") == 0)
		{
			pt[path_number].looped = true;
		}
		else
		{
			pt[path_number].looped = false;
		}

		// Get map position...
		pack_fgets ( line , TEXT_LINE_SIZE , packfile_pointer );
		pointer = STRING_end_of_string(line,"#INITIAL X POSITION = ");
		pt[path_number].x = atoi (pointer);

		pack_fgets ( line , TEXT_LINE_SIZE , packfile_pointer );
		pointer = STRING_end_of_string(line,"#INITIAL Y POSITION = ");
		pt[path_number].y = atoi (pointer);

		// Get node data itself...
		pack_fgets ( line , TEXT_LINE_SIZE , packfile_pointer );
		pointer = STRING_end_of_string(line,"#NODE DATA = ");
		if (pointer != NULL)
		{
			pt[path_number].nodes = atoi(pointer);

			pt[path_number].node_list_pointer = (node *) malloc ( pt[path_number].nodes * sizeof(node) );

			node_number = 0;
			exitflag = false;

			while (exitflag == false)
			{	
				pack_fgets ( line , TEXT_LINE_SIZE , packfile_pointer );
				STRING_strip_newlines (line);

				pointer = STRING_end_of_string(line,"#NODE NUMBER = ");
				if (pointer != NULL)
				{
					node_number = atoi(pointer);
					pt[path_number].node_list_pointer[node_number].pre_cx = -16.0;
					pt[path_number].node_list_pointer[node_number].pre_cy = -16.0;
					pt[path_number].node_list_pointer[node_number].aft_cx = 16.0;
					pt[path_number].node_list_pointer[node_number].aft_cy = 16.0;
				}

				pointer = STRING_end_of_string(line,"#X POS = ");
				if (pointer != NULL)
				{
					pt[path_number].node_list_pointer[node_number].x = atoi(pointer);
				}

				pointer = STRING_end_of_string(line,"#Y POS = ");
				if (pointer != NULL)
				{
					pt[path_number].node_list_pointer[node_number].y = atoi(pointer);
				}

				pointer = STRING_end_of_string(line,"#PRE X POS = ");
				if (pointer != NULL)
				{
					pt[path_number].node_list_pointer[node_number].pre_cx = atoi(pointer);
				}

				pointer = STRING_end_of_string(line,"#PRE Y POS = ");
				if (pointer != NULL)
				{
					pt[path_number].node_list_pointer[node_number].pre_cy = atoi(pointer);
				}

				pointer = STRING_end_of_string(line,"#AFT X POS = ");
				if (pointer != NULL)
				{
					pt[path_number].node_list_pointer[node_number].aft_cx = atoi(pointer);
				}

				pointer = STRING_end_of_string(line,"#AFT Y POS = ");
				if (pointer != NULL)
				{
					pt[path_number].node_list_pointer[node_number].aft_cy = atoi(pointer);
				}
				
				pointer = STRING_end_of_string(line,"#FLAG = ");
				if (pointer != NULL)
				{
					strcpy (pt[path_number].node_list_pointer[node_number].flag_name , pointer);
				}

				pointer = STRING_end_of_string(line,"#SEGMENTS = ");
				if (pointer != NULL)
				{
					pt[path_number].node_list_pointer[node_number].line_segments = atoi(pointer);
				}

				pointer = STRING_end_of_string(line,"#SPEED = ");
				if (pointer != NULL)
				{
					pt[path_number].node_list_pointer[node_number].speed = atoi(pointer);
				}

				pointer = STRING_end_of_string(line,"#SPEED IN CURVE = ");
				if (pointer != NULL)
				{
					pt[path_number].node_list_pointer[node_number].speed_in_curve_type = atoi(pointer);
				}

				pointer = STRING_end_of_string(line,"#SPEED OUT CURVE = ");
				if (pointer != NULL)
				{
					pt[path_number].node_list_pointer[node_number].speed_out_curve_type = atoi(pointer);
				}

				pointer = STRING_end_of_string(line,"#END OF NODE DATA");
				if (pointer != NULL)
				{
					exitflag = true;
				}
			}
		}

		pack_fclose(packfile_pointer);

		// Oh, and bung the capitalised name in. Haha.
		strupr(filename);
		strtok(filename,"."); // Get rid of .txt extension...
		strcpy ( pt[path_number].name , filename );
		strcpy ( pt[path_number].old_name , filename );
	}

}



void PATHS_save (int path_number)
{
	int node_counter;

	char full_filename [MAX_LINE_SIZE];

	char name [MAX_LINE_SIZE];

	char temp_char [MAX_LINE_SIZE];

	strcpy(name,pt[path_number].name);
	strlwr(name);
	append_filename(full_filename, "paths", name, sizeof(full_filename) );
	strcat(full_filename,".txt");

	FILE *file_pointer = fopen (MAIN_get_project_filename (full_filename),"w");

	if (file_pointer != NULL) // It's the path data.
	{
		// Put line type...
		sprintf( temp_char, "#PATH TYPE = %s\n" , &path_type_names[pt[path_number].line_type][0] );
		fputs( temp_char , file_pointer );

		// Put looping behaviour type...
		sprintf( temp_char, "#LOOPING TYPE = %s\n" , &path_looping_behaviour_name[pt[path_number].loop_type][0] );
		fputs( temp_char , file_pointer );

		// Then put looped status...
		if (pt[path_number].looped == true)
		{
			strcpy(temp_char,"#LOOPED = TRUE\n");
		}
		else
		{
			strcpy(temp_char,"#LOOPED = FALSE\n");
		}
		fputs ( temp_char , file_pointer );

		// Put the default position...
		sprintf( temp_char, "#INITIAL X POSITION = %i\n" , pt[path_number].x );
		fputs( temp_char , file_pointer );
		sprintf( temp_char, "#INITIAL Y POSITION = %i\n" , pt[path_number].y );
		fputs( temp_char , file_pointer );

		// First thing is to put the number of nodes...
		sprintf(temp_char,"#NODE DATA = %i\n\n",pt[path_number].nodes);
		fputs ( temp_char , file_pointer );

		node_counter = 0;

		for (node_counter = 0; node_counter < pt[path_number].nodes ; node_counter++)
		{
			sprintf(temp_char,"\t#NODE NUMBER = %d\n",node_counter);
			fputs(temp_char,file_pointer);

			sprintf(temp_char,"\t#X POS = %d\n" , pt[path_number].node_list_pointer[node_counter].x);
			fputs(temp_char,file_pointer);

			sprintf(temp_char,"\t#Y POS = %d\n" , pt[path_number].node_list_pointer[node_counter].y);
			fputs(temp_char,file_pointer);

			if (pt[path_number].line_type == BEZIER)
			{
				sprintf(temp_char,"\t\t#PRE X POS = %d\n" , pt[path_number].node_list_pointer[node_counter].pre_cx);
				fputs(temp_char,file_pointer);

				sprintf(temp_char,"\t\t#PRE Y POS = %d\n" , pt[path_number].node_list_pointer[node_counter].pre_cy);
				fputs(temp_char,file_pointer);

				sprintf(temp_char,"\t\t#AFT X POS = %d\n" , pt[path_number].node_list_pointer[node_counter].aft_cx);
				fputs(temp_char,file_pointer);

				sprintf(temp_char,"\t\t#AFT Y POS = %d\n" , pt[path_number].node_list_pointer[node_counter].aft_cy);
				fputs(temp_char,file_pointer);
			}

			sprintf(temp_char,"\t#FLAG = %s\n" , pt[path_number].node_list_pointer[node_counter].flag_name);
			fputs(temp_char,file_pointer);

			sprintf(temp_char,"\t#SPEED = %i\n" , pt[path_number].node_list_pointer[node_counter].speed);
			fputs(temp_char,file_pointer);

			sprintf(temp_char,"\t#SPEED IN CURVE = %i\n" , pt[path_number].node_list_pointer[node_counter].speed_in_curve_type);
			fputs(temp_char,file_pointer);

			sprintf(temp_char,"\t#SPEED OUT CURVE = %i\n" , pt[path_number].node_list_pointer[node_counter].speed_out_curve_type);
			fputs(temp_char,file_pointer);

			sprintf(temp_char,"\t#SEGMENTS = %i\n\n" , pt[path_number].node_list_pointer[node_counter].line_segments);
			fputs(temp_char,file_pointer);
		}

		sprintf(temp_char,"#END OF NODE DATA\n",node_counter);
		fputs(temp_char,file_pointer);

		fclose(file_pointer);
	}
}



void PATHS_save_all (void)
{
	// This saves out all the tilemaps, however first it deletes the original files
	// so we don't create duplicates.

	int path_number;
//	char filename[NAME_SIZE];
	char full_filename [TEXT_LINE_SIZE];
	
	for (path_number = 0; path_number < number_of_paths_loaded; path_number++)
	{
		if (strcmp (pt[path_number].old_name,"UNSET") != 0)
		{
			append_filename(full_filename, "paths", pt[path_number].old_name, sizeof(full_filename) );
			strcat(full_filename,".txt");

			remove (MAIN_get_project_filename (full_filename));
		}
	}

	for (path_number = 0; path_number < number_of_paths_loaded; path_number++)
	{
		PATHS_save (path_number);
	}
}



void PATHS_generate_biasing_tables (void)
{
	float *fp = NULL;
	int index;
	float angle;
	float angle_adder;
	float sin_value;
	float actual_value;

	// First one, plain linear. Value = index.
	fp = &speed_curve_bias_tables[(MAX_SPEED_CURVE_TYPES*SPEED_CURVE_LINEAR) + SPEED_CURVE_LINEAR][0];
	for (index=0; index<100; index++)
	{
		fp[index] = float(index)/100.0f;
	}

	// Second one, linear out, fast in.
	fp = &speed_curve_bias_tables[(MAX_SPEED_CURVE_TYPES*SPEED_CURVE_LINEAR) + SPEED_CURVE_FAST][0];
	angle = -PI / 4.0f;
	angle_adder = (PI / 4.0f) / 100.0f;
	for (index=0; index<100; index++)
	{
		sin_value = sin (angle);
		actual_value = 1.0f + (sin_value * (1.0f/ sin(PI/4.0f)));
		angle += angle_adder;
		fp[index] = actual_value;
	}

	// Third one, linear out, slow in.
	fp = &speed_curve_bias_tables[(MAX_SPEED_CURVE_TYPES*SPEED_CURVE_LINEAR) + SPEED_CURVE_SLOW][0];
	angle = PI / 4.0f;
	angle_adder = (PI / 4.0f) / 100.0f;
	for (index=0; index<100; index++)
	{
		sin_value = sin (angle);
		actual_value = (sin_value - sin(PI/4.0f)) * (1.0f/ (1.0f-sin(PI/4.0f)));
		angle += angle_adder;
		fp[index] = actual_value;
	}

	// Fourth one, fast out, linear in.
	fp = &speed_curve_bias_tables[(MAX_SPEED_CURVE_TYPES*SPEED_CURVE_FAST) + SPEED_CURVE_LINEAR][0];
	angle = 0.0f;
	angle_adder = (PI / 4.0f) / 100.0f;
	for (index=0; index<100; index++)
	{
		sin_value = sin (angle);
		actual_value = sin_value * (1.0f/ (sin(PI/4.0f)));
		angle += angle_adder;
		fp[index] = actual_value;
	}

	// Fifth one, fast out, fast in.
	fp = &speed_curve_bias_tables[(MAX_SPEED_CURVE_TYPES*SPEED_CURVE_FAST) + SPEED_CURVE_FAST][0];
	angle = 0.0f;
	angle_adder = PI / 100.0f;
	for (index=0; index<100; index++)
	{
		sin_value = sin (angle);
		if (index>=50)
		{
			sin_value = 2.0f - sin_value;
		}
		actual_value = sin_value/2.0f;
		angle += angle_adder;
		fp[index] = actual_value;
	}

	// Sixth one, fast out, slow in.
	fp = &speed_curve_bias_tables[(MAX_SPEED_CURVE_TYPES*SPEED_CURVE_FAST) + SPEED_CURVE_SLOW][0];
	angle = 0.0f;
	angle_adder = (PI / 2.0f) / 100.0f;
	for (index=0; index<100; index++)
	{
		sin_value = sin (angle);
		actual_value = sin_value;
		angle += angle_adder;
		fp[index] = actual_value;
	}

	// Seventh one, slow out, linear in.
	fp = &speed_curve_bias_tables[(MAX_SPEED_CURVE_TYPES*SPEED_CURVE_SLOW) + SPEED_CURVE_LINEAR][0];
	angle = (-PI / 2.0f);
	angle_adder = (PI / 4.0f) / 100.0f;
	for (index=0; index<100; index++)
	{
		sin_value = sin (angle);
		actual_value = 1.0f + (sin_value + sin(PI/4.0f)) * (1.0f/ (1.0f-sin(PI/4.0f)));
		angle += angle_adder;
		fp[index] = actual_value;
	}

	// Eighth one, slow out, fast in.
	fp = &speed_curve_bias_tables[(MAX_SPEED_CURVE_TYPES*SPEED_CURVE_SLOW) + SPEED_CURVE_FAST][0];
	angle = (-PI / 2.0f);
	angle_adder = (PI / 2.0f) / 100.0f;
	for (index=0; index<100; index++)
	{
		sin_value = sin (angle);
		actual_value = 1.0f + sin_value;
		angle += angle_adder;
		fp[index] = actual_value;
	}

	// Ninth one, slow out, slow in.
	fp = &speed_curve_bias_tables[(MAX_SPEED_CURVE_TYPES*SPEED_CURVE_SLOW) + SPEED_CURVE_SLOW][0];
	angle = (-PI / 2.0f);
	angle_adder = PI / 100.0f;
	for (index=0; index<100; index++)
	{
		sin_value = sin (angle);
		actual_value = (1.0f + sin_value)/2.0f;
		angle += angle_adder;
		fp[index] = actual_value;
	}
}



void PATHS_load_all (void)
{
	int start,end,i;
	char filename[NAME_SIZE];
	int path_number;

	PATHS_generate_biasing_tables();

	number_of_paths_loaded = 0;

	GPL_list_extents("PATHS",&start,&end);

	for (i=start; i<end ; i++)
	{
		strcpy(filename,GPL_what_is_word_name(i));
		strcat(filename,GPL_what_is_list_extension ("PATHS"));
		path_number = PATHS_create (false);

		if (load_from_dat_file)
		{
			PATHS_load_from_datafile (filename,path_number);
		}
		else
		{
			PATHS_load (filename,path_number);
		}
		
		PATHS_calculate_path_bounding_box (path_number);
		PATHS_create_lookup_from_path (path_number);
	} 
}



void PATHS_wrap_node (int *node_number, int PATHS_length)
{
	while (*node_number < 0)
	{
		*node_number += PATHS_length;
	}

	while (*node_number >= PATHS_length)
	{
		*node_number -= PATHS_length;
	}
}



void PATHS_constrain_node (int *node_number, int PATHS_length)
{
	if (*node_number < 0)
	{
		*node_number = 0;
	}

	if (*node_number >= PATHS_length)
	{
		*node_number = PATHS_length-1;
	}
}



bool PATHS_check_if_outside_path_constraints ( int path_number , float section_and_percent )
{
	int current_node = int (section_and_percent);
	int max_node = pt[path_number].nodes;

	if (pt[path_number].looped == true) // If it's looped we allow an extra node.
	{
		if (current_node <= max_node)
		{
			return false;
		}
	}
	else
	{
		if (current_node < max_node)
		{
			return false;
		}
	}

	return true;
}



void PATHS_get_nodes (int path_number , int first_node, int *node_store)
{
	int n;

	for (n=0; n<4; n++)
	{
		node_store[n] = first_node + n - 1;
	}

	if (pt[path_number].looped == true) // If it's looped then some of the node co-ords need to loop around
	{
		for (n=0; n<4; n++)
		{
			PATHS_wrap_node (&node_store[n], pt[path_number].nodes);
		}
	}
	else // They don't. :)
	{
		for (n=0; n<4; n++)
		{
			PATHS_constrain_node (&node_store[n], pt[path_number].nodes);
		}
	}
}



void PATHS_get_linear_point (int path_number, int node_number, int *x, int *y, int *speed)
{
	int node_store[4];

	PATHS_get_nodes (path_number , node_number , node_store);

	*x = pt[path_number].node_list_pointer[node_store[2]].x;
	*y = pt[path_number].node_list_pointer[node_store[2]].y;
	*speed = pt[path_number].node_list_pointer[node_store[2]].speed;
}



void PATHS_get_catmull_rom_point (int path_number, int node_number, float percent, int *x, int *y, int *speed)
{
	int node_store[4];

	PATHS_get_nodes (path_number , node_number , node_store);

	*x = int (MATH_get_catmull_rom_spline_point ( pt[path_number].node_list_pointer[node_store[0]].x , pt[path_number].node_list_pointer[node_store[1]].x , pt[path_number].node_list_pointer[node_store[2]].x , pt[path_number].node_list_pointer[node_store[3]].x , percent ));
	*y = int (MATH_get_catmull_rom_spline_point ( pt[path_number].node_list_pointer[node_store[0]].y , pt[path_number].node_list_pointer[node_store[1]].y , pt[path_number].node_list_pointer[node_store[2]].y , pt[path_number].node_list_pointer[node_store[3]].y , percent ));
	*speed = MATH_lerp (pt[path_number].node_list_pointer[node_store[1]].speed , pt[path_number].node_list_pointer[node_store[2]].speed , percent );
}



void PATHS_get_bezier_point (int path_number, int node_number, float percent, int *x, int *y, int *speed)
{
	int node_store[4];

	PATHS_get_nodes (path_number , node_number , node_store);

	*x = int (MATH_bezier ( pt[path_number].node_list_pointer[node_store[1]].x , pt[path_number].node_list_pointer[node_store[1]].x+pt[path_number].node_list_pointer[node_store[1]].aft_cx , pt[path_number].node_list_pointer[node_store[2]].x+pt[path_number].node_list_pointer[node_store[2]].pre_cx , pt[path_number].node_list_pointer[node_store[2]].x , percent ));
	*y = int (MATH_bezier ( pt[path_number].node_list_pointer[node_store[1]].y , pt[path_number].node_list_pointer[node_store[1]].y+pt[path_number].node_list_pointer[node_store[1]].aft_cy , pt[path_number].node_list_pointer[node_store[2]].y+pt[path_number].node_list_pointer[node_store[2]].pre_cy , pt[path_number].node_list_pointer[node_store[2]].y , percent ));
	*speed = MATH_lerp (pt[path_number].node_list_pointer[node_store[1]].speed , pt[path_number].node_list_pointer[node_store[2]].speed , percent );
}



void PATHS_get_new_co_ords (int path_number, int node_number, float percent , int *x , int *y, int *new_speed)
{
	int PATHS_type;

	PATHS_type = pt[path_number].line_type;

	switch ( PATHS_type )
	{
	case LINEAR:
		PATHS_get_linear_point (path_number , node_number , x , y , new_speed);
		break;

	case CATMULL_ROM:
		PATHS_get_catmull_rom_point (path_number , node_number , percent , x , y , new_speed);
		break;

	case BEZIER:
		PATHS_get_bezier_point (path_number , node_number , percent , x , y , new_speed);
		break;

	case SIMPLE:
		PATHS_get_linear_point (path_number , node_number , x , y , new_speed);
		// It's identical, the only difference is the reported line length.
		break;

	default:
		break;
	}
}



void PATHS_constrain_distance_by_loop_behaviour (int path_number, int *distance, int *repetitions)
{
	int temp;
	int path_length;
	int loop_behaviour;

	path_length = pt[path_number].total_distance;
	loop_behaviour = pt[path_number].loop_type;

	if (*distance >= path_length)
	{
		*repetitions = *distance/path_length;

		switch(loop_behaviour)
		{
			case NON_LOOPED:
				*distance = path_length;
				break;

			case LOOP_TO_START:
				*distance %= path_length;
				break;

			case LOOP_ON_END:
				*distance %= path_length;
				break;

			case PING_PONG:
				temp = (*distance / path_length) % 2;
				*distance %= path_length;
				if (temp == 1)
				{
					// It's on it's way backwards along the path...
					*distance = path_length - *distance;	
				}
				break;

			default:
				assert(0);
				break;
		}
	}
}



// 1) Constrain distance to within correct range and therefor calculate iterations and whether it's ping-ponged
// 2) Use the constrained distance to find the actual position and speed
/*
int PATHS_get_correct_lookup_path_section (int path_number, int distance, int starting_node = 0)
{
	int lookup_node_number;
	int direction;

	if (distance >= pt[path_number].total_distance)
	{
		// Distance should have been already wrapped before here!!!
		assert(0);
	}

	if (pt[path_number].lp_node_list_pointer[starting_node].distance <= distance)
	{
		// The supplied distance is either in this node's range or one following it so iterate through
		// them forwards as per usual.
		direction = 1;
	}
	else
	{
		// The supplied distance is within the range of a node before the one supplied as the starting node,
		// so we'll need to work backwards from the supplied starting node.
		direction = -1;
	}

	for (lookup_node_number = starting_node; (lookup_node_number < pt[path_number].lp_nodes-1) && (lookup_node_number >= 0); lookup_node_number += direction)
	{
		if ( (distance >= pt[path_number].lp_node_list_pointer[lookup_node_number].distance) && (distance < pt[path_number].lp_node_list_pointer[lookup_node_number+1].distance) )
		{
			return lookup_node_number;
		}
	}

	// Should never get here!!!

	assert(0);

	return UNSET;
}
*/

/*
void PATHS_get_path_point_from_lookup_data (int path_number, int distance, float *x, float *y, float scale_x, float scale_y, int *point_lp_path_node)
{
	// "data" is used as an array to store the returned information from this function.

	int lp_node_number;
	float percent_along_section;
	int repetitions;
	int loop_type;

	float x1,y1,x2,y2;

	PATHS_constrain_distance_by_loop_behaviour (path_number, &distance, &repetitions);

	loop_type = pt[path_number].loop_type;
	lp_node_number = PATHS_get_correct_lookup_path_section (path_number, distance);
	percent_along_section = MATH_unlerp ( pt[path_number].lp_node_list_pointer[lp_node_number].distance , pt[path_number].lp_node_list_pointer[lp_node_number+1].distance , distance - pt[path_number].lp_node_list_pointer[lp_node_number].distance );

	switch(loop_type)
	{
	case LOOP_ON_END:
		x1 = ( pt[path_number].total_offset_x * repetitions ) + pt[path_number].lp_node_list_pointer[lp_node_number].x;
		y1 = ( pt[path_number].total_offset_y * repetitions ) + pt[path_number].lp_node_list_pointer[lp_node_number].y;

		x2 = ( pt[path_number].total_offset_x * repetitions ) + pt[path_number].lp_node_list_pointer[lp_node_number+1].x;
		y2 = ( pt[path_number].total_offset_y * repetitions ) + pt[path_number].lp_node_list_pointer[lp_node_number+1].y;
		break;

	case NON_LOOPED:
	case LOOP_TO_START:
	case PING_PONG:
		x1 = pt[path_number].lp_node_list_pointer[lp_node_number].x;
		y1 = pt[path_number].lp_node_list_pointer[lp_node_number].y;

		x2 = pt[path_number].lp_node_list_pointer[lp_node_number+1].x;
		y2 = pt[path_number].lp_node_list_pointer[lp_node_number+1].y;
		break;

	default:
		assert(0);
		break;
	}

	*x = MATH_lerp(x1,x2,percent_along_section);
	*y = MATH_lerp(y1,y2,percent_along_section);

	*x *= scale_x;
	*y *= scale_y;

	*point_lp_path_node = lp_node_number;
}
*/


int PATHS_find_flag_between_nodes (int path_number, int first_node ,int second_node)
{
	// This looks from first_node to second_node (in whichever direction) and 

	int lp_node_number;

	if (first_node == second_node)
	{
		return UNSET;
	}
	else if (first_node < second_node)
	{
		for (lp_node_number = first_node+1; lp_node_number<=second_node; lp_node_number++)
		{
			if (pt[path_number].lp_node_list_pointer[lp_node_number].flag_used == true)
			{
				return lp_node_number;
			}
		}
	}
	else if (first_node > second_node)
	{
		for (lp_node_number = first_node; lp_node_number>second_node; lp_node_number--)
		{
			if (pt[path_number].lp_node_list_pointer[lp_node_number].flag_used == true)
			{
				return lp_node_number;
			}
		}
	}

	// Although it'll never get here...
	return UNSET;
}


/*
void PATHS_get_path_offset_from_lookup_data (int path_number, int old_distance, int new_distance, path_feedback *pf, float scale_x, float scale_y)
{
	float x1,y1,x2,y2;
	int iteration_1,iteration_2;
	int first_node, second_node;
	int this_node;
	int loop_behaviour = pt[path_number].loop_type;

	PATHS_get_path_point_from_lookup_data (path_number, old_distance, &x1, &y1, scale_x, scale_y, &first_node);
	PATHS_get_path_point_from_lookup_data (path_number, new_distance, &x2, &y2, scale_x, scale_y, &second_node);

	pf->x_offset = (x2-x1);
	pf->y_offset = (y2-y1);
	pf->angle = atan2 (pf->x_offset,pf->y_offset);

	PATHS_constrain_distance_by_loop_behaviour (path_number , &old_distance , &iteration_1);
	PATHS_constrain_distance_by_loop_behaviour (path_number , &new_distance , &iteration_2);

	// Okay, if the distance has ping-ponged off the end of the path then there's a possible flag.
	// And if old_distance > new_distance then it's going backwards so we need to look at the flag
	// from old_distance rather than new_distance.

	if (iteration_1 != iteration_2)
	{
		if (loop_behaviour == PING_PONG)
		{
			// It's pinged off the end! But was it off the start or the end of the path?
			if (iteration_1 % 2 == 0)
			{
				// Okay, so it started out going forwards so is now going backwards so it pinged off the end of the path.
				// So we need to look at the last flag in the path.
				this_node = pt[path_number].lp_nodes-1;
			}
			else
			{
				this_node = 0;
			}
		}
		else
		{
			// So it's not a ping-ponger in which case we just need to look at the first node
			// on the path as it must have passed through it.
			this_node = 0;
		}
	}
	else
	{
		// Okay, so we just find which slice of the path data the new_distance fits into...

		this_node = PATHS_find_flag_between_nodes (path_number, first_node , second_node);
	}

	if (this_node != UNSET)
	{
		pf->flag_set = pt[path_number].lp_node_list_pointer[this_node].flag_used;
		pf->flag_value = pt[path_number].lp_node_list_pointer[this_node].flag_value;
	}
	else
	{
		pf->flag_set = false;
		pf->flag_value = UNSET;
	}

}
*/



float PATHS_bias_speed_percentage (int path_number, int start_parent_node, int end_parent_node, float percentage)
{
	// How does this magic work then, eh?
	// Well, take an input angle of 45 degrees and an output angle of 45 degrees and bias them
	// by in and out angles of the two nodes. We then use the percentage to lerp between those
	// two values.
	// Then get the sin of that angle (or cos, whichever doesn't wrap) and UNLERP between the
	// start and end sin (or coses) with that to get our real percentage.

	int out_curve_type = pt[path_number].node_list_pointer[start_parent_node].speed_out_curve_type;
	int in_curve_type = pt[path_number].node_list_pointer[end_parent_node].speed_in_curve_type;
	int total_curve_index = (MAX_SPEED_CURVE_TYPES * out_curve_type) + in_curve_type;

	int integer_percentage = (percentage * 100.0f);

	float percentage_segment_offset;
	float start_percent,end_percent;

	if (integer_percentage != 100)
	{
		percentage_segment_offset = (percentage * 100.0f) - float (integer_percentage);
	}
	else
	{
		// We're right at the end of the path, so that means we're at 100% in any situation.
		return (1.0f);
	}

	start_percent = speed_curve_bias_tables[total_curve_index][integer_percentage];

	if (integer_percentage < 99)
	{
		end_percent = speed_curve_bias_tables[total_curve_index][integer_percentage+1];
	}
	else
	{
		end_percent = 1.0;
	}

	return MATH_lerp (start_percent,end_percent,percentage_segment_offset);
}



#define NODE_CHUNK_SIZE				(32) // How many nodes are malloc'd each time.

void PATHS_create_lookup_from_path (int path_number)
{
	int node_store[4];

	int lp_path_node;
	int current_node_list_size;

	float frequency;
	float percent;

	int diff_x;
	int diff_y;

	int node_number;
	int max_node_number;
	int segment;
	int line_type;

	int speed_store[128];

	if (pt[path_number].looped == true)
	{
		max_node_number = pt[path_number].nodes;
		// I hope that's right... Eep.
	}
	else
	{
		max_node_number = pt[path_number].nodes-1;
	}

	line_type = pt[path_number].line_type;

	if (pt[path_number].lp_node_list_pointer != NULL)
	{
		// If we have some old data here, retire it to the ZX81 in the sky...
		free(pt[path_number].lp_node_list_pointer);
		pt[path_number].lp_node_list_pointer = NULL;
		pt[path_number].lp_nodes = 0;
	}

	pt[path_number].lp_node_list_pointer = (lookup_path_node *) malloc ( NODE_CHUNK_SIZE * sizeof(lookup_path_node) );
	current_node_list_size = NODE_CHUNK_SIZE;

	// Enter the first lookup node as we know it's x and y are 0 and the speed is whatever the first regular node is.
	PATHS_get_nodes (path_number , 0 , node_store);

	pt[path_number].lp_node_list_pointer[0].x = pt[path_number].node_list_pointer[node_store[1]].x;
	pt[path_number].lp_node_list_pointer[0].y = pt[path_number].node_list_pointer[node_store[1]].y;
	
	pt[path_number].lp_node_list_pointer[0].speed = pt[path_number].node_list_pointer[node_store[1]].speed;

	pt[path_number].lp_node_list_pointer[0].start_parent_path_node = node_store[1];
	pt[path_number].lp_node_list_pointer[0].end_parent_path_node = node_store[2];
	// Given that all paths have to have at least 2 nodes, this should be safe. As long as we test
	// bounds before reading non-existant nodes, etc.

	lp_path_node = 1;

	for (node_number=0; node_number<max_node_number; node_number++)
	{
		PATHS_get_nodes (path_number , node_number , node_store);

		pt[path_number].lp_node_list_pointer[lp_path_node].flag_used = pt[path_number].node_list_pointer[node_store[2]].flag_used;
		pt[path_number].lp_node_list_pointer[lp_path_node].flag_value = pt[path_number].node_list_pointer[node_store[2]].flag_value;	

		switch(line_type)
		{
		case BEZIER:
			frequency = 1.0f / float(pt[path_number].node_list_pointer[node_store[1]].line_segments);
			percent = frequency;

			for (segment=0; segment<pt[path_number].node_list_pointer[node_store[1]].line_segments; segment++)
			{
				pt[path_number].lp_node_list_pointer[lp_path_node].x = int (MATH_bezier ( pt[path_number].node_list_pointer[node_store[1]].x , pt[path_number].node_list_pointer[node_store[1]].x+pt[path_number].node_list_pointer[node_store[1]].aft_cx , pt[path_number].node_list_pointer[node_store[2]].x+pt[path_number].node_list_pointer[node_store[2]].pre_cx , pt[path_number].node_list_pointer[node_store[2]].x , percent ));
				pt[path_number].lp_node_list_pointer[lp_path_node].y = int (MATH_bezier ( pt[path_number].node_list_pointer[node_store[1]].y , pt[path_number].node_list_pointer[node_store[1]].y+pt[path_number].node_list_pointer[node_store[1]].aft_cy , pt[path_number].node_list_pointer[node_store[2]].y+pt[path_number].node_list_pointer[node_store[2]].pre_cy , pt[path_number].node_list_pointer[node_store[2]].y , percent ));

				pt[path_number].lp_node_list_pointer[lp_path_node].start_parent_path_node = node_store[1];
				pt[path_number].lp_node_list_pointer[lp_path_node].end_parent_path_node = node_store[2];

				if (segment != 0)
				{
					pt[path_number].lp_node_list_pointer[lp_path_node].flag_used = false;
					pt[path_number].lp_node_list_pointer[lp_path_node].flag_value = UNSET;
				}

				lp_path_node++;
				percent += frequency;

				if (lp_path_node == current_node_list_size)
				{
					pt[path_number].lp_node_list_pointer = (lookup_path_node *) realloc ( pt[path_number].lp_node_list_pointer , (current_node_list_size + NODE_CHUNK_SIZE) * sizeof(lookup_path_node) );
					current_node_list_size += NODE_CHUNK_SIZE;
				}
			}

			break;
		case CATMULL_ROM:
			frequency = 1.0f / float(pt[path_number].node_list_pointer[node_store[1]].line_segments);
			percent = frequency;

			for (segment=0; segment<pt[path_number].node_list_pointer[node_store[1]].line_segments; segment++)
			{
				pt[path_number].lp_node_list_pointer[lp_path_node].x = int (MATH_get_catmull_rom_spline_point ( pt[path_number].node_list_pointer[node_store[0]].x , pt[path_number].node_list_pointer[node_store[1]].x , pt[path_number].node_list_pointer[node_store[2]].x , pt[path_number].node_list_pointer[node_store[3]].x , percent ));
				pt[path_number].lp_node_list_pointer[lp_path_node].y = int (MATH_get_catmull_rom_spline_point ( pt[path_number].node_list_pointer[node_store[0]].y , pt[path_number].node_list_pointer[node_store[1]].y , pt[path_number].node_list_pointer[node_store[2]].y , pt[path_number].node_list_pointer[node_store[3]].y , percent ));

				pt[path_number].lp_node_list_pointer[lp_path_node].start_parent_path_node = node_store[1];
				pt[path_number].lp_node_list_pointer[lp_path_node].end_parent_path_node = node_store[2];

				pt[path_number].lp_node_list_pointer[lp_path_node].speed = MATH_lerp (pt[path_number].node_list_pointer[node_store[1]].speed , pt[path_number].node_list_pointer[node_store[2]].speed , percent );
				speed_store[lp_path_node] = pt[path_number].lp_node_list_pointer[lp_path_node].speed;

				if (segment != 0)
				{
					pt[path_number].lp_node_list_pointer[lp_path_node].flag_used = false;
					pt[path_number].lp_node_list_pointer[lp_path_node].flag_value = UNSET;
				}

				lp_path_node++;
				percent += frequency;

				if (lp_path_node == current_node_list_size)
				{
					pt[path_number].lp_node_list_pointer = (lookup_path_node *) realloc ( pt[path_number].lp_node_list_pointer , (current_node_list_size + NODE_CHUNK_SIZE) * sizeof(lookup_path_node) );
					current_node_list_size += NODE_CHUNK_SIZE;
				}
			}
			break;

		case LINEAR:
		case SIMPLE:
			// Because we aren't measuring the length yet, SIMPLE and LINEAR paths are identical
			pt[path_number].lp_node_list_pointer[lp_path_node].x = pt[path_number].node_list_pointer[node_store[2]].x;
			pt[path_number].lp_node_list_pointer[lp_path_node].y = pt[path_number].node_list_pointer[node_store[2]].y;

			pt[path_number].lp_node_list_pointer[lp_path_node].start_parent_path_node = node_store[1];
			pt[path_number].lp_node_list_pointer[lp_path_node].end_parent_path_node = node_store[2];

			percent = 0;
			pt[path_number].lp_node_list_pointer[lp_path_node].speed = MATH_lerp (pt[path_number].node_list_pointer[node_store[1]].speed , pt[path_number].node_list_pointer[node_store[2]].speed , percent );
			speed_store[lp_path_node] = pt[path_number].lp_node_list_pointer[lp_path_node].speed;

			lp_path_node++;

			if (lp_path_node == current_node_list_size)
			{
				pt[path_number].lp_node_list_pointer = (lookup_path_node *) realloc ( pt[path_number].lp_node_list_pointer , (current_node_list_size + NODE_CHUNK_SIZE) * sizeof(lookup_path_node) );
				current_node_list_size += NODE_CHUNK_SIZE;
			}
			break;

		default:
			assert(0);
			break;
		}
	}

	// Set the total offset to the position of the last lookup path node.
	pt[path_number].total_offset_x = pt[path_number].lp_node_list_pointer[lp_path_node-1].x;
	pt[path_number].total_offset_y = pt[path_number].lp_node_list_pointer[lp_path_node-1].y;

	// Clip off the excess allocated lookup path nodes.
	pt[path_number].lp_node_list_pointer = (lookup_path_node *) realloc ( pt[path_number].lp_node_list_pointer , lp_path_node * sizeof(lookup_path_node) );
	pt[path_number].lp_nodes = lp_path_node;

	// Go through the path figuring out the length of each of these little segments and plop them
	// into the relevant variables.

	int lp_total_distance = 0;
	int lp_parent_node_distance = 0;
	int start_parent_node;
	int end_parent_node;
	int last_start_parent_node = UNSET;
	int lp_segment_length;

	for (lp_path_node=0; lp_path_node<pt[path_number].lp_nodes; lp_path_node++)
	{
		start_parent_node = pt[path_number].lp_node_list_pointer[lp_path_node].start_parent_path_node;

		// Check to see if the parent of the current node is different to the one we're in now.
		if (last_start_parent_node != start_parent_node)
		{
			// Ooh, we're into a different section.
			// Now, we've either *just* started the path, or we've stumbled into a new section.

			if (last_start_parent_node == UNSET)
			{
				// Okay, we've just started the path, so just set the distance to 0 and do nothing else.
				lp_parent_node_distance = 0;
			}
			else
			{
				// Righty, we've moved into a new section, best tell the old parent where
				// we ended up.

				// Because by now we're actually one node into the next section the path
				// distance will be too long, so knock it down a bit.
				lp_parent_node_distance -= pt[path_number].lp_node_list_pointer[lp_path_node-1].distance_to_next_lp_node;

				// And then use it.
				pt[path_number].node_list_pointer[last_start_parent_node].agregate_distance = lp_parent_node_distance;
				pt[path_number].node_list_pointer[last_start_parent_node].percentage_multiplier = 1.0f / float(lp_parent_node_distance);

				// And then set the path distance to the length of the first section in this bit of the path.
				lp_parent_node_distance = pt[path_number].lp_node_list_pointer[lp_path_node-1].distance_to_next_lp_node;
			}

			last_start_parent_node = start_parent_node;
		}

		if (lp_path_node<pt[path_number].lp_nodes-1)
		{
			diff_x = pt[path_number].lp_node_list_pointer[lp_path_node].x - pt[path_number].lp_node_list_pointer[lp_path_node+1].x;
			diff_y = pt[path_number].lp_node_list_pointer[lp_path_node].y - pt[path_number].lp_node_list_pointer[lp_path_node+1].y;

			if (line_type == SIMPLE)
			{
				lp_segment_length = MATH_largest_int ( MATH_abs_int(diff_x) , MATH_abs_int(diff_y) );
			}
			else
			{
				lp_segment_length = MATH_get_distance_int ( pt[path_number].lp_node_list_pointer[lp_path_node].x , pt[path_number].lp_node_list_pointer[lp_path_node].y , pt[path_number].lp_node_list_pointer[lp_path_node+1].x , pt[path_number].lp_node_list_pointer[lp_path_node+1].y );
			}
		}
		else
		{
			diff_x = 0;
			diff_y = 0;
			lp_segment_length = 0;
		}

		pt[path_number].lp_node_list_pointer[lp_path_node].angle = atan2 (diff_x,diff_y);

		lp_total_distance += lp_segment_length;

		pt[path_number].lp_node_list_pointer[lp_path_node].distance_to_next_lp_node = lp_segment_length;
		pt[path_number].lp_node_list_pointer[lp_path_node].total_distance_to_parent_path_node_via_lp_nodes = lp_parent_node_distance;

		lp_parent_node_distance += lp_segment_length;
	}

	// Now we still haven't told the last parent node how long we ended up being, so we should do that...

	pt[path_number].node_list_pointer[last_start_parent_node].agregate_distance = lp_parent_node_distance;
	pt[path_number].node_list_pointer[last_start_parent_node].percentage_multiplier = 1.0f / float(lp_parent_node_distance);

	pt[path_number].total_distance = lp_total_distance;

	// Okay, now calculate the speed of each lookup node...

	for (lp_path_node=0; lp_path_node<pt[path_number].lp_nodes; lp_path_node++)
	{
		// Now we're gonna' go through aaaall the ikkle nodes and tell them how far they are between each of their parent nodes as a percentage.
		// We can then use that top-secret information to decide what speed each one is.
		
		start_parent_node = pt[path_number].lp_node_list_pointer[lp_path_node].start_parent_path_node;
		end_parent_node = pt[path_number].lp_node_list_pointer[lp_path_node].end_parent_path_node;

		pt[path_number].lp_node_list_pointer[lp_path_node].speed_percentage = pt[path_number].lp_node_list_pointer[lp_path_node].total_distance_to_parent_path_node_via_lp_nodes * pt[path_number].node_list_pointer[start_parent_node].percentage_multiplier;

		// Bias the percentage.
		pt[path_number].lp_node_list_pointer[lp_path_node].speed_percentage = PATHS_bias_speed_percentage (path_number, start_parent_node, end_parent_node, pt[path_number].lp_node_list_pointer[lp_path_node].speed_percentage);

		pt[path_number].lp_node_list_pointer[lp_path_node].speed = int (MATH_lerp (pt[path_number].node_list_pointer[start_parent_node].speed , pt[path_number].node_list_pointer[end_parent_node].speed , pt[path_number].lp_node_list_pointer[lp_path_node].speed_percentage));
	}

	// Now use that to calculate the time length of each leg of the lookup path.

	float average_speed;
	float time_length;
	float total_time_length = 0;

	for (lp_path_node=0; lp_path_node<pt[path_number].lp_nodes-1; lp_path_node++)
	{
		// Average speed is the sum of the speeds of this and the next lp_node divided by 2.
		average_speed = float (pt[path_number].lp_node_list_pointer[lp_path_node].speed + pt[path_number].lp_node_list_pointer[lp_path_node+1].speed) / 2.0f;

		if (average_speed > 0.0f)
		{
			// The time length from this to the next node is the distance between them divided by the average speed.
			time_length = float (pt[path_number].lp_node_list_pointer[lp_path_node].distance_to_next_lp_node) / average_speed;
//			pt[path_number].lp_node_list_pointer[lp_path_node].time_length = time_length;
			pt[path_number].lp_node_list_pointer[lp_path_node].time_length_from_start = total_time_length;
			total_time_length += time_length;
		}
		else
		{
			assert(0);
			// Speed = 0.0f!!! Shiters!
		}
	}

	pt[path_number].lp_node_list_pointer[pt[path_number].lp_nodes-1].time_length_from_start = total_time_length;

	float percent_to_time_length_unit = float (total_path_percent_size) / total_time_length;

	// Now we have the total time length of the path and that of each diddy segment's start, 
	// we can calculate the start percentage of each segment.

	for (lp_path_node=0; lp_path_node<pt[path_number].lp_nodes-1; lp_path_node++)
	{
		pt[path_number].lp_node_list_pointer[lp_path_node].start_percentage = int (pt[path_number].lp_node_list_pointer[lp_path_node].time_length_from_start * percent_to_time_length_unit);
	}

	pt[path_number].lp_node_list_pointer[pt[path_number].lp_nodes-1].start_percentage = total_path_percent_size;

	for (lp_path_node=0; lp_path_node<pt[path_number].lp_nodes-1; lp_path_node++)
	{
		pt[path_number].lp_node_list_pointer[lp_path_node].percentage_length = pt[path_number].lp_node_list_pointer[lp_path_node+1].start_percentage - pt[path_number].lp_node_list_pointer[lp_path_node].start_percentage;
	}

	pt[path_number].lp_node_list_pointer[pt[path_number].lp_nodes-1].percentage_length = 0;

/*


	distance = 0;

	// This is the length of time taken to traverse the path at the average speed of each leg.
	float time_length;
	float total_time_length = 0;

	for (lp_path_node=0; lp_path_node<pt[path_number].lp_nodes-1; lp_path_node++)
	{
		diff_x = pt[path_number].lp_node_list_pointer[lp_path_node].x - pt[path_number].lp_node_list_pointer[lp_path_node+1].x;
		diff_y = pt[path_number].lp_node_list_pointer[lp_path_node].y - pt[path_number].lp_node_list_pointer[lp_path_node+1].y;

		switch(line_type)
		{
		case BEZIER:
		case CATMULL_ROM:
		case LINEAR:
			length = MATH_get_distance_int ( pt[path_number].lp_node_list_pointer[lp_path_node].x , pt[path_number].lp_node_list_pointer[lp_path_node].y , pt[path_number].lp_node_list_pointer[lp_path_node+1].x , pt[path_number].lp_node_list_pointer[lp_path_node+1].y );
			break;

		case SIMPLE:
			length = MATH_largest_int ( MATH_abs_int(diff_x) , MATH_abs_int(diff_y) );
			break;

		default:
			assert(0);
			break;
		}

		time_length = float (length) / float ( (pt[path_number].lp_node_list_pointer[lp_path_node].speed + pt[path_number].lp_node_list_pointer[lp_path_node+1].speed) / 2 );

		pt[path_number].lp_node_list_pointer[lp_path_node].time_length = time_length;
		pt[path_number].lp_node_list_pointer[lp_path_node].distance = distance;
		pt[path_number].lp_node_list_pointer[lp_path_node].length = length;

		pt[path_number].lp_node_list_pointer[lp_path_node].angle = atan2 (diff_x,diff_y);
	
		total_time_length += time_length;

		distance += length;
	}

	// Okay, we now know which parent node each lp node belongs to, and we also know
	// how long each chunk is. We can now go through all of them, figure out how long
	// each parent section is (is, the total distance between all of the nodes between
	// two genuine path points) and therefor figure out how far along in terms of
	// percentage each one is. From this we can then calculate the speeds at each
	// point. Once that's done I'll figure out how to work bias into the things, too. Erk.

	// Or. OR. We do the paths differently.

	// Okay, you can stop screaming now. Nah, sod that.
	
	float total_percent;
	float this_percent;
	int n1,n2;

	for (node_number=0; node_number<max_node_number; node_number++)
	{
		// Okay, so figure out the total length of each lp span.

		distance = 0;

		for (lp_path_node=0; lp_path_node<pt[path_number].lp_nodes-1; lp_path_node++)
		{
			if (pt[path_number].lp_node_list_pointer[lp_path_node].start_parent_path_node == node_number)
			{
				distance += pt[path_number].lp_node_list_pointer[lp_path_node].length;
			}
		}

		// Okay, so we know the total distance, we can now do the percentage thing.

		total_percent = 0.0f;

		for (lp_path_node=0; lp_path_node<pt[path_number].lp_nodes-1; lp_path_node++)
		{
			if (pt[path_number].lp_node_list_pointer[lp_path_node].start_parent_path_node == node_number)
			{
				pt[path_number].lp_node_list_pointer[lp_path_node].percentage_distance_between_parent_nodes = total_percent;

				n1 = pt[path_number].lp_node_list_pointer[lp_path_node].start_parent_path_node;
				n2 = pt[path_number].lp_node_list_pointer[lp_path_node].end_parent_path_node;
				pt[path_number].lp_node_list_pointer[lp_path_node].speed = MATH_lerp (float (pt[path_number].node_list_pointer[n1].speed) , float (pt[path_number].node_list_pointer[n2].speed) , total_percent );

				speed_store_2[lp_path_node] = pt[path_number].lp_node_list_pointer[lp_path_node].speed;

				this_percent = float (pt[path_number].lp_node_list_pointer[lp_path_node].distance) / float (distance);
				total_percent += this_percent;
			}
		}
	}

	float percent_per_unit_time_length = float (total_path_percent_size) / float (total_time_length);

	total_percent = 0.0f;
	
	for (lp_path_node=0; lp_path_node<pt[path_number].lp_nodes-1; lp_path_node++)
	{
		pt[path_number].lp_node_list_pointer[lp_path_node].start_percentage = int (total_percent);
		this_percent = float (pt[path_number].lp_node_list_pointer[lp_path_node].time_length) * percent_per_unit_time_length;

		total_percent += this_percent;
	}

	for (lp_path_node=0; lp_path_node<pt[path_number].lp_nodes-2; lp_path_node++)
	{
		pt[path_number].lp_node_list_pointer[lp_path_node].percentage_length = pt[path_number].lp_node_list_pointer[lp_path_node+1].start_percentage - pt[path_number].lp_node_list_pointer[lp_path_node].start_percentage;
	}

	pt[path_number].lp_node_list_pointer[pt[path_number].lp_nodes-1].percentage_length = total_path_percent_size;

	if (path_number==2)
	{
		int a=0;
	}

	pt[path_number].lp_node_list_pointer[pt[path_number].lp_nodes-1].distance = UNSET;
	pt[path_number].lp_node_list_pointer[pt[path_number].lp_nodes-1].length = UNSET;
	pt[path_number].lp_node_list_pointer[pt[path_number].lp_nodes-1].angle = UNSET;

	pt[path_number].total_distance = distance;

*/

}



int PATHS_get_section (int path_number, int percentage, int current_section = UNSET)
{
	// This iterates through the nodes in a path until it finds the one in which the percentage
	// passed resides.

	if (percentage >= total_path_percent_size)
	{
		percentage %= total_path_percent_size;
	}

	if (current_section == UNSET)
	{
		current_section = 0;

		while (pt[path_number].lp_node_list_pointer[current_section].start_percentage + pt[path_number].lp_node_list_pointer[current_section].percentage_length < percentage)
		{
			current_section++;
		}
	}
	else
	{
		// We do know it, check if it's valid.
		if ( (current_section < 0) || (current_section >= pt[path_number].lp_nodes) )
		{
			assert(0);
		}
		else if (percentage < pt[path_number].lp_node_list_pointer[current_section].start_percentage)
		{
			while (percentage < pt[path_number].lp_node_list_pointer[current_section].start_percentage) 
			{
				current_section--;
				if (current_section < 0)
				{
					// WTF?
					assert(0);
				}
			}
		}
		else if (percentage > pt[path_number].lp_node_list_pointer[current_section].start_percentage + pt[path_number].lp_node_list_pointer[current_section].percentage_length)
		{
			while (percentage > pt[path_number].lp_node_list_pointer[current_section].start_percentage + pt[path_number].lp_node_list_pointer[current_section].percentage_length)
			{
				current_section++;
				if (current_section == pt[path_number].lp_nodes)
				{
					// WTF?
					assert(0);
				}
			}
		}
	}

	return current_section;
}



int PATHS_hit_flag (int path_number, int old_percentage, int new_percentage, int current_section)
{
	current_section = PATHS_get_section (path_number,old_percentage,current_section);
	// This is plenty fast if we pass in a section and it's already right.

	int new_section = PATHS_get_section (path_number,new_percentage,current_section);

	int section;

	if (current_section != new_section)
	{
		if (current_section < new_section)
		{
			// This means the new section is up ahead on the path (ie, we ain't wrapped).
			// Find the first flag from current_section+1 to new_section that's set and
			// return it.

			for	(section=current_section+1; section<=new_section; section++)
			{
				if (pt[path_number].lp_node_list_pointer[section].flag_used)
				{
					return (pt[path_number].lp_node_list_pointer[section].flag_value);
				}
			}
		}
		else
		{
			// We've wrapped! So we need to check from current_section+1 to new_section, 
			// wrapping before the last node.

			for	(section=current_section+1; section<pt[path_number].lp_nodes; section++)
			{
				if (pt[path_number].lp_node_list_pointer[section].flag_used)
				{
					return (pt[path_number].lp_node_list_pointer[section].flag_value);
				}
			}

			for	(section=0; section<=new_section; section++)
			{
				if (pt[path_number].lp_node_list_pointer[section].flag_used)
				{
					return (pt[path_number].lp_node_list_pointer[section].flag_value);
				}
			}
		}

	}

	return UNSET;
}



float PATHS_get_true_percentage (int path_number, int percentage, int current_section)
{
	// Okay, first of all wrap our percentage to the total_path_percent_size.

	int loop_counts = percentage / total_path_percent_size;
	percentage = percentage % total_path_percent_size;

	if (pt[path_number].loop_type == PING_PONG)
	{
		if ((loop_counts % 2) == 1)
		{
			percentage = total_path_percent_size - percentage;
		}
	}

	int percentage_into_section = percentage - pt[path_number].lp_node_list_pointer[current_section].start_percentage;
	float percentage_into_section_float = float (percentage_into_section) / float (pt[path_number].lp_node_list_pointer[current_section].percentage_length);
	
	float speed_at_position = MATH_lerp (pt[path_number].lp_node_list_pointer[current_section].speed , pt[path_number].lp_node_list_pointer[current_section+1].speed , percentage_into_section_float);
	float average_speed = (float(pt[path_number].lp_node_list_pointer[current_section].speed) + speed_at_position) / 2.0f;
	float section_avergage_speed = float (pt[path_number].lp_node_list_pointer[current_section].speed + pt[path_number].lp_node_list_pointer[current_section+1].speed) / 2.0f;

	float real_percentage = percentage_into_section_float * average_speed / section_avergage_speed;

	return real_percentage;
}



int PATHS_point_angle (int path_number, int path_angle, int percentage, int current_section)
{
	float real_percentage = PATHS_get_true_percentage (path_number, percentage, current_section);

	int start_angle = pt[path_number].lp_node_list_pointer[current_section].angle;
	int end_angle = pt[path_number].lp_node_list_pointer[current_section+1].angle;
	int diff_angle = MATH_diff_angle (end_angle, start_angle);

	return (start_angle + int (real_percentage * float(diff_angle)) + path_angle);
}



int PATHS_section_angle (int path_number, int path_angle, int percentage, int current_section)
{
	current_section = PATHS_get_section (path_number,percentage,current_section);
	// This is plenty fast if we pass in a section and it's already right.

	return pt[path_number].lp_node_list_pointer[current_section].angle + path_angle;
}



int PATHS_section_start_percentage (int path_number, int percentage, int current_section)
{
	current_section = PATHS_get_section (path_number,percentage,current_section);
	// This is plenty fast if we pass in a section and it's already right.

	return pt[path_number].lp_node_list_pointer[current_section].start_percentage;
}



int PATHS_get_path_position_offset_from_percentage (int path_number, int path_angle, int percentage, int *x_offset, int *y_offset, int current_section)
{
	// This gives you the offset from the start of the path to your current position based on your percentage
	// completeness of the path circuit. If you do not pass in a current_section it will have to search the 
	// whole path for where you are. It returns whichever segment you end up in.

	int loop_counts = percentage / total_path_percent_size;
	int initial_offset_x;
	int initial_offset_y;

	if (pt[path_number].loop_type == LOOP_ON_END)
	{
		initial_offset_x = pt[path_number].total_offset_x * loop_counts;
		initial_offset_y = pt[path_number].total_offset_y * loop_counts;
	}
	else
	{
		initial_offset_x = 0;
		initial_offset_y = 0;
	}

	current_section = PATHS_get_section (path_number,percentage,current_section);
	// This is plenty fast if we pass in a section and it's already right.

	float real_percentage = PATHS_get_true_percentage (path_number, percentage, current_section);

	float x,y;

	x = MATH_lerp (pt[path_number].lp_node_list_pointer[current_section].x , pt[path_number].lp_node_list_pointer[current_section+1].x , real_percentage) + float(initial_offset_x);
	y = MATH_lerp (pt[path_number].lp_node_list_pointer[current_section].y , pt[path_number].lp_node_list_pointer[current_section+1].y , real_percentage) + float(initial_offset_y);

	float real_angle = path_angle / angle_conversion_ratio;

	MATH_rotate_point_around_origin (&x, &y, real_angle);

	*x_offset = int (x);
	*y_offset = int (y);

	return current_section;
}



int PATHS_get_length (int path_number)
{
	return pt[path_number].total_distance;
}



void PATHS_change_name (char *old_entry_name, char *new_entry_name)
{
	int t;

	for (t=0 ; t<number_of_paths_loaded ; t++)
	{
		if ( strcmp (pt[t].name , old_entry_name) == 0)
		{
			// Righty, we first need to check to see if it's had it's name changed before and if
			// not back up the original filename so it can delete the file when it saves and not
			// replicate data.

			if (strcmp ( pt[t].old_name ,"UNSET") == 0)
			{
				// Righty, haven't renamed this before...
				strcpy ( pt[t].old_name , pt[t].name );
			}

			strcpy (pt[t].name , new_entry_name);

			// Now there's the possibility that the new name for the file is the same as the original
			// name which means there's no need to delete the old one before saving over it, so we
			// reset it to UNSET.

			if (strcmp (pt[t].old_name , pt[t].name ) == 0)
			{
				strcpy (pt[t].old_name , "UNSET");
			}
		}
	}

}



void PATHS_draw_path (int path_number , float path_angle, int x, int y, float zoom_level, int red, int green, int blue, bool draw_subnodes)
{
	float x1,y1;
	float x2,y2;

	int lp_node_number;

	for (lp_node_number=0; lp_node_number<pt[path_number].lp_nodes-1; lp_node_number++)
	{
		x1 = pt[path_number].lp_node_list_pointer[lp_node_number].x;
		y1 = pt[path_number].lp_node_list_pointer[lp_node_number].y;
		x2 = pt[path_number].lp_node_list_pointer[lp_node_number+1].x;
		y2 = pt[path_number].lp_node_list_pointer[lp_node_number+1].y;

		OUTPUT_line ( int (float(x+x1)*zoom_level), int (float(y+y1)*zoom_level), int (float(x+x2)*zoom_level), int (float(y+y2)*zoom_level), red, green, blue );

		if (draw_subnodes == true)
		{
			OUTPUT_centred_square ( int (float(x+x1)*zoom_level), int (float(y+y1)*zoom_level), 2, red, green, blue );
		}
	}
}



void PATHS_draw_node (int path_number, int node_number, float path_angle, int start_x, int start_y, float zoom_level, int red, int green, int blue, bool show_extras)
{
	int node_x,node_y;
	int screen_x,screen_y;
	int extra_screen_x,extra_screen_y;
	int line_type;
	int x,y;

	char word[TEXT_LINE_SIZE];

	x = pt[path_number].x - start_x;
	y = pt[path_number].y - start_y;

	line_type = pt[path_number].line_type;
	
	node_x = pt[path_number].node_list_pointer[node_number].x;
	node_y = pt[path_number].node_list_pointer[node_number].y;

	screen_x = int(float(x+node_x)*zoom_level);
	screen_y = int(float(y+node_y)*zoom_level);

	OUTPUT_circle (screen_x,screen_y,PATH_NODE_HANDLE_SIZE,red,green,blue);

	if (show_extras)
	{
		// Show handles...
		OUTPUT_draw_masked_sprite ( first_icon , NODE_SPEED_CURVE_ICON_SLOW_IN + pt[path_number].node_list_pointer[node_number].speed_in_curve_type, screen_x-(ICON_SIZE/2) , screen_y+PATH_NODE_HANDLE_SIZE );
		OUTPUT_draw_masked_sprite ( first_icon , NODE_SPEED_CURVE_ICON_SLOW_OUT + pt[path_number].node_list_pointer[node_number].speed_out_curve_type, screen_x-(ICON_SIZE/2) , screen_y+PATH_NODE_HANDLE_SIZE );

		// Horizontal.
		OUTPUT_line (screen_x+PATH_NODE_HANDLE_SIZE, screen_y, screen_x+PATH_NODE_HANDLE_SIZE*3, screen_y);
		OUTPUT_line (screen_x+PATH_NODE_HANDLE_SIZE*3, screen_y, screen_x+PATH_NODE_HANDLE_SIZE*2, screen_y-PATH_NODE_HANDLE_SIZE/2);
		OUTPUT_line (screen_x+PATH_NODE_HANDLE_SIZE*3, screen_y, screen_x+PATH_NODE_HANDLE_SIZE*2, screen_y+PATH_NODE_HANDLE_SIZE/2);

		// Vertical
		OUTPUT_line (screen_x, screen_y+PATH_NODE_HANDLE_SIZE, screen_x, screen_y+PATH_NODE_HANDLE_SIZE*3);
		OUTPUT_line (screen_x, screen_y+PATH_NODE_HANDLE_SIZE*3, screen_x-PATH_NODE_HANDLE_SIZE/2, screen_y+PATH_NODE_HANDLE_SIZE*2);
		OUTPUT_line (screen_x, screen_y+PATH_NODE_HANDLE_SIZE*3, screen_x+PATH_NODE_HANDLE_SIZE/2, screen_y+PATH_NODE_HANDLE_SIZE*2);

		if (line_type == BEZIER)
		{
			// Also show the pre and aft handles of the selected node if it's a bezier line.

			extra_screen_x = int(float(x+node_x+pt[path_number].node_list_pointer[node_number].pre_cx)*zoom_level);
			extra_screen_y = int(float(y+node_y+pt[path_number].node_list_pointer[node_number].pre_cy)*zoom_level);

			OUTPUT_line (screen_x,screen_y,extra_screen_x,extra_screen_y,red/2,green/2,blue/2);
			OUTPUT_circle (extra_screen_x,extra_screen_y,PATH_PRE_OR_AFT_HANDLE_SIZE,red/2,green/2,blue/2);
			
			extra_screen_x = int(float(x+node_x+pt[path_number].node_list_pointer[node_number].aft_cx)*zoom_level);
			extra_screen_y = int(float(y+node_y+pt[path_number].node_list_pointer[node_number].aft_cy)*zoom_level);

			OUTPUT_line (screen_x,screen_y,extra_screen_x,extra_screen_y,red,green,blue);
			OUTPUT_circle (extra_screen_x,extra_screen_y,PATH_PRE_OR_AFT_HANDLE_SIZE,red,green,blue);
		}

		// And show the speed of the node...

		sprintf (word, "%i", pt[path_number].node_list_pointer[node_number].speed);
		OUTPUT_centred_text (screen_x, screen_y-PATH_NODE_HANDLE_SIZE-FONT_HEIGHT, word);

	}
	else
	{
		// Show the speed of the node, but in a duller colour...

		sprintf (word, "%i", pt[path_number].node_list_pointer[node_number].speed);
		OUTPUT_centred_text (screen_x, screen_y-PATH_NODE_HANDLE_SIZE-FONT_HEIGHT, word, 255,0,255);
	}
}



void PATHS_draw_all_nodes (int selected_node, int path_number, int start_x, int start_y, float zoom_level)
{
	int node_number;
	static int frame_counter = 0;

	int flip_flop = frame_counter % 2;

	for (node_number=0; node_number<pt[path_number].nodes; node_number++)
	{
		if ( (pt[path_number].node_list_pointer[node_number].grabbed_status == NODE_SELECTED) || ( (flip_flop == 0) && ( (pt[path_number].node_list_pointer[node_number].grabbed_status == NODE_PRESUMPTIVE_SELECTED)  || (pt[path_number].node_list_pointer[node_number].grabbed_status == NODE_PRESUMPTIVE_UNSELECTED) ) ) )
		{
			if (selected_node >= 0)
			{
				PATHS_draw_node ( path_number, node_number, 0.0f, start_x, start_y, zoom_level, 255,255,255, true);
			}
			else
			{
				PATHS_draw_node ( path_number, node_number, 0.0f, start_x, start_y, zoom_level, 0,255,255, true);
			}
		}
		else
		{
			PATHS_draw_node ( path_number, node_number, 0.0f, start_x, start_y, zoom_level, 255,0,255, false);
		}
	}

	frame_counter++;

}



void PATHS_calculate_path_bounding_box (int path_number)
{
	pt[path_number].greatest_x_offset = 0;
	pt[path_number].greatest_y_offset = 0;
	pt[path_number].lowest_x_offset = 0;
	pt[path_number].lowest_y_offset = 0;

	int node_number;

	for (node_number=0; node_number<pt[path_number].nodes; node_number++)
	{
		if (pt[path_number].node_list_pointer[node_number].x < pt[path_number].lowest_x_offset)
		{
			pt[path_number].lowest_x_offset = pt[path_number].node_list_pointer[node_number].x;
		}
		else if (pt[path_number].node_list_pointer[node_number].x > pt[path_number].greatest_x_offset)
		{
			pt[path_number].greatest_x_offset = pt[path_number].node_list_pointer[node_number].x;
		}

		if (pt[path_number].node_list_pointer[node_number].y < pt[path_number].lowest_y_offset)
		{
			pt[path_number].lowest_y_offset = pt[path_number].node_list_pointer[node_number].y;
		}
		else if (pt[path_number].node_list_pointer[node_number].y > pt[path_number].greatest_y_offset)
		{
			pt[path_number].greatest_y_offset = pt[path_number].node_list_pointer[node_number].y;
		}
	}

}



void PATHS_draw_all_paths (int selected_path_number, int start_x, int start_y, int width, int height, float zoom_level, int norm_path_red, int norm_path_green, int norm_path_blue, int sel_path_red, int sel_path_green, int sel_path_blue)
{
	int path_number;
	int x,y;

	for (path_number=0; path_number<number_of_paths_loaded; path_number++)
	{
		x = pt[path_number].x - start_x;
		y = pt[path_number].y - start_y;

		if (MATH_rectangle_to_rectangle_intersect ( 0,0,width,height ,x+pt[path_number].lowest_x_offset ,y+pt[path_number].lowest_y_offset , x+pt[path_number].greatest_x_offset, y+pt[path_number].greatest_y_offset ) == true)
		{
			if (path_number == selected_path_number)
			{
				PATHS_draw_path (path_number , 0.0f, x, y, zoom_level, sel_path_red, sel_path_green, sel_path_blue, true);
			}
			else
			{
				PATHS_draw_path (path_number , 0.0f, x, y, zoom_level, norm_path_red, norm_path_green, norm_path_blue, false);
			}
		}
	}
}



char * PATHS_node_flag (int path_number, int node_number)
{
	return pt[path_number].node_list_pointer[node_number].flag_name;
}



int PATHS_find_nearest_path_node (int path_number, int x, int y, bool return_distance)
{
	int nearest_distance_so_far;
	int distance;
	int nearest_path_node;

	nearest_path_node = UNSET;
	nearest_distance_so_far = UNSET;

	int node_number;

	for (node_number=0; node_number<pt[path_number].lp_nodes; node_number++)
	{
		distance = MATH_get_distance_int (x , y , pt[path_number].x + pt[path_number].lp_node_list_pointer[node_number].x , pt[path_number].y + pt[path_number].lp_node_list_pointer[node_number].y );
		
		if ( (distance < nearest_distance_so_far) || (nearest_distance_so_far == UNSET) )
		{
			nearest_distance_so_far = distance;
			nearest_path_node = node_number;
		}
	}

	if (return_distance)
	{
		return nearest_distance_so_far;
	}
	else
	{
		return nearest_path_node;
	}
}



int PATHS_find_nearest_path_line (int path_number, int x, int y, bool return_distance)
{
	int nearest_distance_so_far;
	int distance;
	int nearest_path_line;
	int extra_for_loop;
	int mid_x,mid_y;
	int other_node_number;

	nearest_path_line = UNSET;
	nearest_distance_so_far = UNSET;

	int node_number;

	if (pt[path_number].looped == true)
	{
		extra_for_loop = 0;
	}
	else
	{
		extra_for_loop = 1;
	}

	for (node_number=0; node_number<(pt[path_number].nodes - extra_for_loop); node_number++)
	{
		other_node_number = (node_number+1) % pt[path_number].nodes;

		mid_x = pt[path_number].node_list_pointer[node_number].x;
		mid_y = pt[path_number].node_list_pointer[node_number].y;

		mid_x += pt[path_number].node_list_pointer[other_node_number].x;
		mid_y += pt[path_number].node_list_pointer[other_node_number].y;

		mid_x /= 2;
		mid_y /= 2;

		distance = MATH_get_distance_int (x , y , mid_x , mid_y );
		
		if ( (distance < nearest_distance_so_far) || (nearest_distance_so_far == UNSET) )
		{
			nearest_distance_so_far = distance;
			nearest_path_line = node_number;
		}
	}

	if (return_distance)
	{
		return nearest_distance_so_far;
	}
	else
	{
		return nearest_path_line;
	}
}



int PATHS_find_nearest_path (int x, int y, bool return_distance)
{
	int nearest_distance_so_far;
	int distance;
	int nearest_path;

	nearest_path = UNSET;
	nearest_distance_so_far = UNSET;

	int path_number;

	for (path_number=0; path_number<number_of_paths_loaded; path_number++)
	{
		distance = PATHS_find_nearest_path_node (path_number, x, y, true);

		if ( (distance < nearest_distance_so_far) || (nearest_distance_so_far == UNSET) )
		{
			nearest_distance_so_far = distance;
			nearest_path = path_number;
		}
	}

	if (return_distance)
	{
		return nearest_distance_so_far;
	}
	else
	{
		return nearest_path;
	}
}



void PATHS_get_node_co_ords (int path_number, int node_number, int *node_x, int *node_y)
{
	*node_x = pt[path_number].node_list_pointer[node_number].x;
	*node_y = pt[path_number].node_list_pointer[node_number].y;
}



void PATHS_get_node_pre_co_ords (int path_number, int node_number, int *pre_cx, int *pre_cy)
{
	*pre_cx = pt[path_number].node_list_pointer[node_number].pre_cx;
	*pre_cy = pt[path_number].node_list_pointer[node_number].pre_cy;
}



void PATHS_get_node_aft_co_ords (int path_number, int node_number, int *aft_cx, int *aft_cy)
{
	*aft_cx = pt[path_number].node_list_pointer[node_number].aft_cx;
	*aft_cy = pt[path_number].node_list_pointer[node_number].aft_cy;
}



void PATHS_get_node_grabbed_details (int path_number, int node_number, int *grabbed_status, int *node_x, int *node_y)
{
	*grabbed_status = pt[path_number].node_list_pointer[node_number].grabbed_status;
	*node_x = pt[path_number].node_list_pointer[node_number].grabbed_x;
	*node_y = pt[path_number].node_list_pointer[node_number].grabbed_y;
}



void PATHS_get_node_flag_details (int path_number, int node_number, char *flag_name, bool *flag_used)
{
	flag_name = pt[path_number].node_list_pointer[node_number].flag_name;
	*flag_used = pt[path_number].node_list_pointer[node_number].flag_used;
}



void PATHS_set_node_co_ords (int path_number, int node_number, int node_x, int node_y)
{
	pt[path_number].node_list_pointer[node_number].x = node_x;
	pt[path_number].node_list_pointer[node_number].y = node_y;
}



void PATHS_set_node_pre_co_ords (int path_number, int node_number, int pre_cx, int pre_cy)
{
	pt[path_number].node_list_pointer[node_number].pre_cx = pre_cx;
	pt[path_number].node_list_pointer[node_number].pre_cy = pre_cy;
}



void PATHS_set_node_aft_co_ords (int path_number, int node_number, int aft_cx, int aft_cy)
{
	pt[path_number].node_list_pointer[node_number].aft_cx = aft_cx;
	pt[path_number].node_list_pointer[node_number].aft_cy = aft_cy;
}



void PATHS_set_node_flag_details (int path_number, int node_number, char *flag_name, bool flag_used)
{
	strcpy (pt[path_number].node_list_pointer[node_number].flag_name,flag_name);
	pt[path_number].node_list_pointer[node_number].flag_used = flag_used;
}



void PATHS_get_path_name (int path_number, char *name)
{
	strcpy (name,pt[path_number].name);
}



void PATHS_set_path_name (int path_number, char *name)
{
	strcpy (pt[path_number].name,name);
}



void PATHS_get_path_position (int path_number, int *x, int *y)
{
	*x = pt[path_number].x;
	*y = pt[path_number].y;
}



void PATHS_get_path_loop_details (int path_number, int *loop_type, bool *looped)
{
	*loop_type = pt[path_number].loop_type;
	*looped = pt[path_number].looped;
}



void PATHS_get_path_line_type (int path_number, int *line_type)
{
	*line_type = pt[path_number].line_type;
}



void PATHS_set_path_position (int path_number, int x, int y)
{
	pt[path_number].x = x;
	pt[path_number].y = y;
}



void PATHS_set_path_loop_details (int path_number, int loop_type, bool looped)
{
	pt[path_number].loop_type = loop_type;
	pt[path_number].looped = looped;
}


void PATHS_set_path_line_type (int path_number, int line_type)
{
	pt[path_number].line_type = line_type;
}



bool PATHS_check_for_duplicate_name (int selected_path_number, char *new_entry_name)
{
	int path_number;

	for (path_number=0; path_number<number_of_paths_loaded; path_number++)
	{
		if (path_number!=selected_path_number) // Not checking against itself.
		{
			if (strcmp (pt[path_number].name , new_entry_name) == 0)
			{
				return true;
			}
		}
	}

	return false;
}



void PATHS_register_path_name_change (char *old_path_name , char *new_path_name)
{
	int path_number;

	for (path_number=0; path_number<number_of_paths_loaded; path_number++)
	{
		if (strcmp (old_path_name,pt[path_number].name) == 0)
		{
			strcpy (pt[path_number].name, new_path_name);
		}
	}
}



void PATHS_partially_set_nodes_status (int path_number, int x1, int y1, int x2, int y2, float zoom_level, int flagging_type, int drag_time)
{
	// First of all resets all PRESUMPTIVE nodes to their base states and then
	// reset the PRESUMPTIVES. This is dependant on the flagging_type, of course.

	// Note that if the time that the mouse button was held down for was less than 
	// "NO_DRAG_THRESHOLD" frames then only the first encountered node will be
	// flagged as it's the equivalent of a single-click until then.

	int node_number;

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

	for (node_number=0; node_number<pt[path_number].nodes; node_number++)
	{
		if (flagging_type == SELECTING_NODES)
		{
			// Reset 'em regardless!
			pt[path_number].node_list_pointer[node_number].grabbed_status = NODE_UNSELECTED;
		}
		else
		{
			if (pt[path_number].node_list_pointer[node_number].grabbed_status == NODE_PRESUMPTIVE_UNSELECTED)
			{
				pt[path_number].node_list_pointer[node_number].grabbed_status = NODE_SELECTED;
			}
			else if (pt[path_number].node_list_pointer[node_number].grabbed_status == NODE_PRESUMPTIVE_SELECTED)
			{
				pt[path_number].node_list_pointer[node_number].grabbed_status = NODE_UNSELECTED;
			}
		}
	}

	int node_x,node_y;

	for (node_number=0; node_number<pt[path_number].nodes; node_number++)
	{
		node_x = pt[path_number].node_list_pointer[node_number].x + pt[path_number].x;
		node_y = pt[path_number].node_list_pointer[node_number].y + pt[path_number].y;

		if (MATH_rectangle_to_rectangle_intersect ( node_x-int(float(PATH_NODE_HANDLE_SIZE)/zoom_level) , node_y-int(float(PATH_NODE_HANDLE_SIZE)/zoom_level) , node_x+int(float(PATH_NODE_HANDLE_SIZE)/zoom_level) , node_y+int(float(PATH_NODE_HANDLE_SIZE)/zoom_level) , x1, y1, x2, y2 ) == true)
		{
			if ( (flagging_type == SELECTING_NODES) || (flagging_type == ADDING_NODES) )
			{
				if (pt[path_number].node_list_pointer[node_number].grabbed_status == NODE_UNSELECTED)
				{
					pt[path_number].node_list_pointer[node_number].grabbed_status = NODE_PRESUMPTIVE_SELECTED;
					
					if (drag_time < NO_DRAG_THRESHOLD)
					{
						// We're only altering the one item until we've held down the button a while.
						return;
					}
				}
			}
			else if (flagging_type == REMOVING_NODES)
			{
				if (pt[path_number].node_list_pointer[node_number].grabbed_status == NODE_SELECTED)
				{
					pt[path_number].node_list_pointer[node_number].grabbed_status = NODE_PRESUMPTIVE_UNSELECTED;
					
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



int PATHS_length (int path_number)
{
	return pt[path_number].total_distance;
}



void PATHS_deselect_nodes (int path_number)
{
	// Flags the presumptives as actuals. If there's only one selected then it's ID is returned, otherwise
	// a negative number is returned.

	int node_number;

	for (node_number=0; node_number<pt[path_number].nodes; node_number++)
	{
		pt[path_number].node_list_pointer[node_number].grabbed_status = NODE_UNSELECTED;
	}
}



int PATHS_lock_nodes_status (int path_number)
{
	// Flags the presumptives as actuals. If there's only one selected then it's ID is returned, otherwise
	// a negative number is returned.

	int node_number;
	int flagged_node_number;

	int total_flagged = 0;

	for (node_number=0; node_number<pt[path_number].nodes; node_number++)
	{
		if (pt[path_number].node_list_pointer[node_number].grabbed_status == NODE_PRESUMPTIVE_UNSELECTED)
		{
			pt[path_number].node_list_pointer[node_number].grabbed_status = NODE_UNSELECTED;
		}
		else if (pt[path_number].node_list_pointer[node_number].grabbed_status == NODE_PRESUMPTIVE_SELECTED)
		{
			total_flagged++;
			flagged_node_number = node_number;
			pt[path_number].node_list_pointer[node_number].grabbed_status = NODE_SELECTED;
		}
		else if (pt[path_number].node_list_pointer[node_number].grabbed_status == NODE_SELECTED)
		{
			// Already selected, but we've gotta' increase the counter for it.
			total_flagged++;
			flagged_node_number = node_number;
		}
	}

	if (total_flagged == 0)
	{
		return NO_NODES_SELECTED;
	}
	else if (total_flagged == 1)
	{
		return flagged_node_number;
	}
	else
	{
		return MULTIPLE_NODES_SELECTED;
	}

}



void PATHS_get_grabbed_co_ords (int path_number)
{
	int node_number;

	for (node_number=0; node_number<pt[path_number].nodes; node_number++)
	{
		if (pt[path_number].node_list_pointer[node_number].grabbed_status == NODE_SELECTED)
		{
			pt[path_number].node_list_pointer[node_number].grabbed_x = pt[path_number].node_list_pointer[node_number].x;
			pt[path_number].node_list_pointer[node_number].grabbed_y = pt[path_number].node_list_pointer[node_number].y;
		}
	}
}



void PATHS_cycle_selected_node_speed_curves (int path_number, bool in_curve)
{
	// Updates all the selected nodes so they are positioned offset_x and offset_y from where
	// they started.

	// Note that although the co-ordinates are rounded to the grid, they are only rounded by the
	// amount that the grabbed node is out of whack.

	int node_number;

	for (node_number=0; node_number<pt[path_number].nodes; node_number++)
	{
		if (pt[path_number].node_list_pointer[node_number].grabbed_status == NODE_SELECTED)
		{

			if (in_curve == true)
			{
				pt[path_number].node_list_pointer[node_number].speed_in_curve_type++;
				pt[path_number].node_list_pointer[node_number].speed_in_curve_type %= MAX_SPEED_CURVE_TYPES;
			}
			else
			{
				pt[path_number].node_list_pointer[node_number].speed_out_curve_type++;
				pt[path_number].node_list_pointer[node_number].speed_out_curve_type %= MAX_SPEED_CURVE_TYPES;
			}
		
		}
	}
}



void PATHS_position_selected_nodes (int path_number, int offset_x, int offset_y, int grabbed_node, int grid_size, bool round_to_grid)
{
	// Updates all the selected nodes so they are positioned offset_x and offset_y from where
	// they started.

	// Note that although the co-ordinates are rounded to the grid, they are only rounded by the
	// amount that the grabbed node is out of whack.

	int out_of_whack_x;
	int out_of_whack_y;

	out_of_whack_x = (pt[path_number].node_list_pointer[grabbed_node].grabbed_x + offset_x) % grid_size;
	out_of_whack_y = (pt[path_number].node_list_pointer[grabbed_node].grabbed_y + offset_y) % grid_size;

	int node_number;

	for (node_number=0; node_number<pt[path_number].nodes; node_number++)
	{
		if (pt[path_number].node_list_pointer[node_number].grabbed_status == NODE_SELECTED)
		{
			pt[path_number].node_list_pointer[node_number].x = pt[path_number].node_list_pointer[node_number].grabbed_x + offset_x;
			pt[path_number].node_list_pointer[node_number].y = pt[path_number].node_list_pointer[node_number].grabbed_y + offset_y;

			if (round_to_grid == true)
			{
				pt[path_number].node_list_pointer[node_number].x -= out_of_whack_x;
				pt[path_number].node_list_pointer[node_number].y -= out_of_whack_y;
			}
		}
	}
}



bool PATHS_what_selected_path_node_thing_has_been_grabbed (int path_number, int x, int y, float zoom_level, int *grabbed_handle, int *grabbed_node)
{
	// This goes through all the selected nodes and sees if the point we've just clicked refers to 

	int node_x,node_y;
	int pre_x,pre_y;
	int aft_x,aft_y;

	int node_number;

	int line_type;

	int grabbed;

	grabbed = UNSET;

	line_type = pt[path_number].line_type;
	
	for (node_number=0; node_number<pt[path_number].nodes; node_number++)
	{
		// Go through all the nodes...

		if (pt[path_number].node_list_pointer[node_number].grabbed_status == NODE_SELECTED)
		{
			// But only consider those which have been flagged as selected.

			grabbed = UNSET;

			node_x = pt[path_number].node_list_pointer[node_number].x + pt[path_number].x;
			node_y = pt[path_number].node_list_pointer[node_number].y + pt[path_number].y;

			if (MATH_rectangle_to_point_intersect ( node_x-int(float(PATH_NODE_HANDLE_SIZE)/zoom_level) , node_y-int(float(PATH_NODE_HANDLE_SIZE)/zoom_level) , node_x+int(float(PATH_NODE_HANDLE_SIZE)/zoom_level) , node_y+int(float(PATH_NODE_HANDLE_SIZE)/zoom_level) , x , y ) == true)
			{
				grabbed = GRABBED_PATH_NODE_NODE;
			}

			if (MATH_rectangle_to_point_intersect ( node_x+int(float(PATH_NODE_HANDLE_SIZE)/zoom_level) , node_y-int(float(PATH_NODE_HANDLE_SIZE/2)/zoom_level) , node_x+int(float(PATH_NODE_HANDLE_SIZE*3)/zoom_level) , node_y+int(float(PATH_NODE_HANDLE_SIZE/2)/zoom_level) , x , y ) == true)
			{
				grabbed = GRABBED_PATH_NODE_X_HANDLE;
			}

			if (MATH_rectangle_to_point_intersect ( node_x-int(float(PATH_NODE_HANDLE_SIZE/2)/zoom_level) , node_y+int(float(PATH_NODE_HANDLE_SIZE)/zoom_level) , node_x+int(float(PATH_NODE_HANDLE_SIZE/2)/zoom_level) , node_y+int(float(PATH_NODE_HANDLE_SIZE*3)/zoom_level) , x , y ) == true)
			{
				grabbed = GRABBED_PATH_NODE_Y_HANDLE;
			}

			if (line_type == BEZIER)
			{
				pre_x = pt[path_number].node_list_pointer[node_number].pre_cx;
				pre_y = pt[path_number].node_list_pointer[node_number].pre_cy;

				aft_x = pt[path_number].node_list_pointer[node_number].aft_cx;
				aft_y = pt[path_number].node_list_pointer[node_number].aft_cy;

				if (MATH_rectangle_to_point_intersect ( (node_x+pre_x)-int(float(PATH_PRE_OR_AFT_HANDLE_SIZE)/zoom_level) , (node_y+pre_y)-int(float(PATH_PRE_OR_AFT_HANDLE_SIZE)/zoom_level) , (node_x+pre_x)+int(float(PATH_PRE_OR_AFT_HANDLE_SIZE)/zoom_level) , (node_y+pre_y)+int(float(PATH_PRE_OR_AFT_HANDLE_SIZE)/zoom_level) , x , y ) == true)
				{
					grabbed = GRABBED_PATH_NODE_PRE;
				}

				if (MATH_rectangle_to_point_intersect ( (node_x+aft_x)-int(float(PATH_PRE_OR_AFT_HANDLE_SIZE)/zoom_level) , (node_y+aft_y)-int(float(PATH_PRE_OR_AFT_HANDLE_SIZE)/zoom_level) , (node_x+aft_x)+int(float(PATH_PRE_OR_AFT_HANDLE_SIZE)/zoom_level) , (node_y+aft_y)+int(float(PATH_PRE_OR_AFT_HANDLE_SIZE)/zoom_level) , x , y ) == true)
				{
					grabbed = GRABBED_PATH_NODE_AFT;
				}
			}

			if (grabbed != UNSET)
			{
				*grabbed_handle = grabbed;
				*grabbed_node = node_number;
				return true;
			}

		}

	}

	return false;
}




#define CREATE_NODES						(0)
#define EDIT_OR_MOVE_NODES					(1)
#define PATH_SELECTED_WAITING_FOR_INPUT		(2)
#define DRAGGING_OVER_NODES					(3)
#define CREATE_PATHS						(4)
#define SELECTING_PATHS						(5)

bool PATHS_edit_paths (int state , bool overlay_display, int *current_cursor ,int sx, int sy,  int tilemap_number, int mx, int my, float zoom_level, int grid_size)
{
	// Control method...

	// Nodes will have axis handles, however bezier control points will not.

	// If you do not click on a node or it's bezier controllers when in node editing mode
	// then it will be considered that you're entering highlighting mode. You will then
	// start to drag a box, when the mouse button is released all the highlighted nodes
	// will be selected. You can then either add to the selection with further SHIFT-drags or
	// take away from it with CTRL-drags. When you want to actually move the selected nodes
	// you need to click on one of the currently selected members and then drag to wherever.

	// Clicking on an unselected member will deselect everything and select that member.

	// Pressing SHIFT or CTRL when dragging a Bezier gizmo will have an unusual effect in
	// that it will cause the other gizmo to either match angle or match angle and length,
	// thus easily allowing you to have smooth corners without huge amounts of faffing.

	// So code-wise speaking it goes like this:

	int real_mx,real_my;

	int tileset_number;
	int tilesize;

	char *flag_name = NULL;
	bool flag_used = false;

	static int frame_counter;

	static char path_name[NAME_SIZE];
	static char old_path_name[NAME_SIZE];

	char text[TEXT_LINE_SIZE];

	int total_grabbed_nodes;

	int nearest_path;
	int nearest_path_node;
	int nearest_path_line;

	static int selecting_node_mode;

	static int single_selected_node;

	static bool override_main_editor;
	static int override_mode;

	static int selected_path;

	static int locking_nodes_result;

	static int drag_start_x;
	static int drag_start_y;
	static int drag_end_x;
	static int drag_end_y;
	static int drag_time;

	int screen_x1,screen_y1,screen_x2,screen_y2;

	int drag_dist_x;
	int drag_dist_y;

	int temp;

	int total_drag_offset_x;
	int total_drag_offset_y;

	static int stored_bezier_x;
	static int stored_bezier_y;
	static int stored_bezier_alternates_length;
	float stored_bezier_alternates_angle;
	
	static int grabbed_handle_type;
	static int actual_grabbed_node; // The node you grab to start moving
	static int grabbed_real_mx;
	static int grabbed_real_my;
	static bool grabbed;

	static int matching_name_counter;
	static int getting_new_name_status;

	int line_type;
	int loop_type;
	bool looped;

	int icon_number;

	int zoomed_grid_size;

	int grid_snapped_real_mx;
	int grid_snapped_real_my;

	static int editing_mode;

	if (state == STATE_INITIALISE)
	{
		override_main_editor = false;
		strcpy (path_name,"UNSET");

		getting_new_name_status = GET_WORD;

		selected_path = UNSET;
		actual_grabbed_node = UNSET;

		editing_mode = SELECTING_PATHS;
		
		grabbed_handle_type = UNSET;

		override_main_editor = false;

		grabbed = false;

		single_selected_node = UNSET;

		total_grabbed_nodes = 0;
	}
	else if (state == STATE_SET_UP_BUTTONS)
	{
		editor_view_width = game_screen_width-(ICON_SIZE*8);
		editor_view_height = game_screen_height;

		SIMPGUI_sleep_group (TILEMAP_EDITOR_SUB_GROUP);
	}
	else if (state == STATE_RUNNING)
	{
		frame_counter++;

		if (overlay_display == true)
		{
			editor_view_width = game_screen_width-(ICON_SIZE*8);
			editor_view_height = game_screen_height;
		}

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
		
		zoomed_grid_size = int(float(grid_size)*zoom_level);

		real_mx = (mx/zoom_level) + (sx*tilesize);
		real_my = (my/zoom_level) + (sy*tilesize);

		grid_snapped_real_mx = real_mx - (real_mx % grid_size);
		grid_snapped_real_my = real_my - (real_my % grid_size);

		if (selected_path != UNSET)
		{
			// If we have a path selected then we'll probably want to know where the nearest node on it is.
			nearest_path_node = PATHS_find_nearest_path_node (selected_path , real_mx, real_my );
			nearest_path_line = PATHS_find_nearest_path_line (selected_path , real_mx, real_my );
		}
		else
		{
			// Otherwise we'll probably want to know which path is the nearest.
			nearest_path = PATHS_find_nearest_path (real_mx, real_my);
		}

		if (selected_path != UNSET)
		{
			PATHS_calculate_path_bounding_box (selected_path);
			PATHS_draw_all_paths (selected_path, (sx * tilesize), (sy * tilesize), int(float(editor_view_width)/zoom_level), int(float(editor_view_height)/zoom_level), zoom_level, 0,0,255, 255,0,255 );

			// Then manually draw all the other nodes. Eep!

			if ( (editing_mode == CREATE_NODES) && (override_main_editor == false) )
			{
				// And if we're creating nodes draw one where it'll be created when you press the
				// LMB.

				if (CONTROL_key_down(KEY_LSHIFT) == true)
				{
					// Draw mid-point node
					PATHS_draw_midpoint_co_ords (selected_path, nearest_path_line, sx, sy, tilesize, zoom_level);
				}
				else
				{
					// Draw regular node
					OUTPUT_circle ( mx - (mx % (zoomed_grid_size)) , my - (my % (zoomed_grid_size)) , (frame_counter % PATH_NODE_HANDLE_SIZE) ,255 ,255 ,255);
				}
			}

			PATHS_draw_all_nodes (single_selected_node, selected_path, sx*tilesize, sy*tilesize, zoom_level);
		}
		else
		{
			PATHS_draw_all_paths (nearest_path, (sx * tilesize), (sy * tilesize), (editor_view_width/zoom_level), (editor_view_height/zoom_level), zoom_level, 0,0,255, 255,0,0 );

			if ( (editing_mode == CREATE_PATHS) && (override_main_editor == false) )
			{
				// Draw regular node
				OUTPUT_circle ( mx - (mx % (zoomed_grid_size)) , my - (my % (zoomed_grid_size)) , (frame_counter % PATH_NODE_HANDLE_SIZE) ,255 ,255 ,255);
			}
		}

		if (overlay_display == true)
		{
			SIMPGUI_wake_group (TILEMAP_EDITOR_SUB_GROUP);

			OUTPUT_filled_rectangle (editor_view_width,0,game_screen_width,game_screen_height,0,0,0);

			OUTPUT_vline (editor_view_width,0,game_screen_height,255,255,255);

			OUTPUT_centred_text ( editor_view_width+(ICON_SIZE*4) , (ICON_SIZE-FONT_HEIGHT)/2 , "PATH EDITOR" );

			if (selected_path != UNSET)
			{
				// Will only be drawn if a path is selected.

				OUTPUT_draw_masked_sprite ( first_icon , EDIT_MODE_ICON, editor_view_width+(ICON_SIZE/2) , (ICON_SIZE*1) );
				OUTPUT_draw_masked_sprite ( first_icon , NEW_NODE_ICON, editor_view_width+(ICON_SIZE/2)+(ICON_SIZE*2) , (ICON_SIZE*1) );
				OUTPUT_draw_masked_sprite ( first_icon , EDIT_NODE_ICON, editor_view_width+(ICON_SIZE/2)+(ICON_SIZE*5) , (ICON_SIZE*1) );

				if (editing_mode == CREATE_NODES)
				{
					OUTPUT_draw_masked_sprite ( first_icon , FORWARD_ARROW, editor_view_width+(ICON_SIZE/2)+(ICON_SIZE*1) , (ICON_SIZE*1) );
					OUTPUT_draw_masked_sprite ( first_icon , BACKWARD_ARROW, editor_view_width+(ICON_SIZE/2)+(ICON_SIZE*3) , (ICON_SIZE*1) );
				}
				else
				{
					OUTPUT_draw_masked_sprite ( first_icon , FORWARD_ARROW, editor_view_width+(ICON_SIZE/2)+(ICON_SIZE*4) , (ICON_SIZE*1) );
					OUTPUT_draw_masked_sprite ( first_icon , BACKWARD_ARROW, editor_view_width+(ICON_SIZE/2)+(ICON_SIZE*6) , (ICON_SIZE*1) );
				}

				PATHS_get_path_name (selected_path,text);

				OUTPUT_rectangle_by_size (editor_view_width+(ICON_SIZE/2),(ICON_SIZE*3),(ICON_SIZE*7),ICON_SIZE,0,0,255);
				OUTPUT_centred_text (editor_view_width+(ICON_SIZE*4),(ICON_SIZE*3)+(ICON_SIZE/2)-(FONT_HEIGHT/2),text);

				PATHS_get_path_line_type (selected_path, &line_type);

				OUTPUT_draw_masked_sprite ( first_icon , PATH_TYPE_ICON, editor_view_width+(ICON_SIZE/2) , (ICON_SIZE*5) );
				for (icon_number=0; icon_number<4; icon_number++)
				{
					OUTPUT_draw_masked_sprite ( first_icon , icon_number+LINEAR_ICON, editor_view_width+(ICON_SIZE/2)+(ICON_SIZE*2)+(icon_number*ICON_SIZE) , (ICON_SIZE*5) );
				}

				if (frame_counter % 2)
				{
					OUTPUT_draw_masked_sprite ( first_icon , BRIGHT_OVERLAY, editor_view_width+(ICON_SIZE/2)+(ICON_SIZE*2)+(line_type*ICON_SIZE) , (ICON_SIZE*5) );
				}

				PATHS_get_path_loop_details (selected_path, &loop_type, &looped);

				OUTPUT_draw_masked_sprite ( first_icon , LOOP_TYPE_ICON, editor_view_width+(ICON_SIZE/2) , (ICON_SIZE*6) );
				for (icon_number=0; icon_number<4; icon_number++)
				{
					OUTPUT_draw_masked_sprite ( first_icon , icon_number+NO_LOOP_ICON, editor_view_width+(ICON_SIZE/2)+(ICON_SIZE*2)+(icon_number*ICON_SIZE) , (ICON_SIZE*6) );
				}

				if (frame_counter % 2)
				{
					OUTPUT_draw_masked_sprite ( first_icon , BRIGHT_OVERLAY, editor_view_width+(ICON_SIZE/2)+(ICON_SIZE*2)+(loop_type*ICON_SIZE) , (ICON_SIZE*6) );
				}

				OUTPUT_draw_masked_sprite ( first_icon , LOOP_OR_NOT_ICON, editor_view_width+(ICON_SIZE/2) , (ICON_SIZE*7) );
				OUTPUT_draw_masked_sprite ( first_icon , YES_ICON, editor_view_width+(ICON_SIZE/2)+(ICON_SIZE*2) , (ICON_SIZE*7) );
				OUTPUT_draw_masked_sprite ( first_icon , NO_ICON, editor_view_width+(ICON_SIZE/2)+(ICON_SIZE*5) , (ICON_SIZE*7) );

				if (looped == true)
				{
					OUTPUT_draw_masked_sprite ( first_icon , FORWARD_ARROW, editor_view_width+(ICON_SIZE/2)+(ICON_SIZE*1) , (ICON_SIZE*7) );
					OUTPUT_draw_masked_sprite ( first_icon , BACKWARD_ARROW, editor_view_width+(ICON_SIZE/2)+(ICON_SIZE*3) , (ICON_SIZE*7) );
				}
				else
				{
					OUTPUT_draw_masked_sprite ( first_icon , FORWARD_ARROW, editor_view_width+(ICON_SIZE/2)+(ICON_SIZE*4) , (ICON_SIZE*7) );
					OUTPUT_draw_masked_sprite ( first_icon , BACKWARD_ARROW, editor_view_width+(ICON_SIZE/2)+(ICON_SIZE*6) , (ICON_SIZE*7) );
				}

				sprintf (text,"PATH LENGTH = %i",PATHS_length(selected_path));

				OUTPUT_rectangle_by_size (editor_view_width+(ICON_SIZE/2),(ICON_SIZE*8),(ICON_SIZE*7),ICON_SIZE,0,0,255);
				OUTPUT_centred_text (editor_view_width+(ICON_SIZE*4),(ICON_SIZE*8)+(ICON_SIZE/2)-(FONT_HEIGHT/2),text);
			}
			else
			{
				OUTPUT_draw_masked_sprite ( first_icon , EDIT_MODE_ICON, editor_view_width+(ICON_SIZE/2) , (ICON_SIZE*1) );
				OUTPUT_draw_masked_sprite ( first_icon , NEW_PATH_ICON, editor_view_width+(ICON_SIZE/2)+(ICON_SIZE*2) , (ICON_SIZE*1) );
				OUTPUT_draw_masked_sprite ( first_icon , EDIT_PATH_ICON, editor_view_width+(ICON_SIZE/2)+(ICON_SIZE*5) , (ICON_SIZE*1) );

				if (editing_mode == CREATE_PATHS)
				{
					OUTPUT_draw_masked_sprite ( first_icon , FORWARD_ARROW, editor_view_width+(ICON_SIZE/2)+(ICON_SIZE*1) , (ICON_SIZE*1) );
					OUTPUT_draw_masked_sprite ( first_icon , BACKWARD_ARROW, editor_view_width+(ICON_SIZE/2)+(ICON_SIZE*3) , (ICON_SIZE*1) );
				}
				else
				{
					OUTPUT_draw_masked_sprite ( first_icon , FORWARD_ARROW, editor_view_width+(ICON_SIZE/2)+(ICON_SIZE*4) , (ICON_SIZE*1) );
					OUTPUT_draw_masked_sprite ( first_icon , BACKWARD_ARROW, editor_view_width+(ICON_SIZE/2)+(ICON_SIZE*6) , (ICON_SIZE*1) );
				}
			}

			if (single_selected_node >= 0)
			{
				// Will only be set if an individual node is selected and not multiple nodes.

				sprintf (text,"%s",PATHS_node_flag(selected_path,single_selected_node));

				OUTPUT_rectangle_by_size (editor_view_width+(ICON_SIZE/2),(ICON_SIZE*10),(ICON_SIZE*7),ICON_SIZE,0,0,255);
				
				PATHS_get_node_flag_details (selected_path, single_selected_node, flag_name, &flag_used);

				if (flag_used == true)
				{
					OUTPUT_centred_text (editor_view_width+(ICON_SIZE*4),(ICON_SIZE*10)+(FONT_HEIGHT/2), "NODE FLAG");
					OUTPUT_centred_text (editor_view_width+(ICON_SIZE*4),(ICON_SIZE*10)+(ICON_SIZE/2)+(FONT_HEIGHT/2), text);
				}
				else
				{
					OUTPUT_centred_text (editor_view_width+(ICON_SIZE*4),(ICON_SIZE*10)+(FONT_HEIGHT/2), "NODE FLAG", 0,0,255);
					OUTPUT_centred_text (editor_view_width+(ICON_SIZE*4),(ICON_SIZE*10)+(ICON_SIZE/2)+(FONT_HEIGHT/2), "UNSET", 0,0,255);
				}

				OUTPUT_draw_masked_sprite ( first_icon , RESET_FLAG_ICON, editor_view_width+(ICON_SIZE/2) , (ICON_SIZE*11) );
				OUTPUT_draw_masked_sprite ( first_icon , BIG_YES_ICON, editor_view_width+(ICON_SIZE/2)+ICON_SIZE , (ICON_SIZE*11) );
			}

			SIMPGUI_check_all_buttons ();
			SIMPGUI_draw_all_buttons ();
		}
		else
		{
			SIMPGUI_sleep_group (TILEMAP_EDITOR_SUB_GROUP);
			editor_view_width = game_screen_width;
			editor_view_height = game_screen_height;
		}

		if ( (mx<editor_view_width) && (my<editor_view_height) && (override_main_editor == false) )
		{
			// Clicks in the view window

			if (selected_path == UNSET)
			{
				// Clicks to select or create a path.

				if (CONTROL_mouse_hit(LMB) == true)
				{
					if (editing_mode == CREATE_PATHS)
					{
						// Create path!
						selected_path = PATHS_create (true);
						PATHS_set_path_position (selected_path, grid_snapped_real_mx, grid_snapped_real_my);
						PATHS_create_path_node (selected_path, 0, 0);
						editing_mode = CREATE_NODES;
					}
					else if (editing_mode == SELECTING_PATHS)
					{
						selected_path = nearest_path;
						PATHS_deselect_nodes (selected_path);
						editing_mode = PATH_SELECTED_WAITING_FOR_INPUT;
					}
				}

				if (CONTROL_mouse_hit(RMB) == true)
				{
					if (editing_mode == CREATE_PATHS)
					{
						editing_mode = SELECTING_PATHS;
					}
				}

			}
			else
			{
				if (CONTROL_mouse_hit(LMB) == true)
				{
					if (editing_mode == PATH_SELECTED_WAITING_FOR_INPUT)
					{
						// Either we're beginning to drag a box or we're grabbing a node for moving.

						if ( (PATHS_what_selected_path_node_thing_has_been_grabbed (selected_path, real_mx, real_my, zoom_level, &grabbed_handle_type, &actual_grabbed_node) == false) || (CONTROL_key_down(KEY_LSHIFT) == true) || (CONTROL_key_down(KEY_LCONTROL) == true) )
						{
							editing_mode = DRAGGING_OVER_NODES;

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
							editing_mode = EDIT_OR_MOVE_NODES;

							// We've just grabbed a selected node, or one of it's handles or bezier controllers.

							// Luckily we already know what's been grabbed and where it was grabbed.

							// If it's a bezier controller then rather than dragging all the selected nodes around,
							// we'll just be dragging the grabbed bezier controller even if we have oodles of nodes
							// selected.

							drag_start_x = real_mx;
							drag_start_y = real_my;

							switch (grabbed_handle_type)
							{
							case GRABBED_PATH_NODE_PRE:
								// Store the length of the other bezier controller.
								PATHS_get_node_aft_co_ords (selected_path,actual_grabbed_node,&stored_bezier_x,&stored_bezier_y);
								stored_bezier_alternates_length = MATH_get_distance_int (0,0,stored_bezier_x,stored_bezier_y);
								// Then get the proper one.
								PATHS_get_node_pre_co_ords (selected_path,actual_grabbed_node,&stored_bezier_x,&stored_bezier_y);
								break;

							case GRABBED_PATH_NODE_AFT:
								// Store the length of the other bezier controller.
								PATHS_get_node_pre_co_ords (selected_path,actual_grabbed_node,&stored_bezier_x,&stored_bezier_y);
								stored_bezier_alternates_length = MATH_get_distance_int (0,0,stored_bezier_x,stored_bezier_y);
								// Then get the proper one.
								PATHS_get_node_aft_co_ords (selected_path,actual_grabbed_node,&stored_bezier_x,&stored_bezier_y);
								break;

							default: // ie, node or either node axis handle.
								PATHS_get_grabbed_co_ords (selected_path);
								break;
							}
						}
					}
					else if (editing_mode == CREATE_NODES)
					{
						if (CONTROL_key_down(KEY_LSHIFT) == true)
						{
							// Create node in the middle of nearest line...
							PATHS_create_midpoint_path_node (selected_path , nearest_path_line);
							PATHS_create_lookup_from_path (selected_path);
						}
						else
						{
							// Create node at the end of the path.
							PATHS_create_path_node_global_co_ord (selected_path, grid_snapped_real_mx, grid_snapped_real_my);
							PATHS_create_lookup_from_path (selected_path);
						}

					}
				}

				if (CONTROL_mouse_hit(RMB) == true)
				{
					if (editing_mode == PATH_SELECTED_WAITING_FOR_INPUT)
					{
						editing_mode = SELECTING_PATHS;
						PATHS_deselect_nodes(selected_path);
						selected_path = UNSET;
						single_selected_node = UNSET;
					}
					else if (editing_mode == CREATE_NODES)
					{
						editing_mode = PATH_SELECTED_WAITING_FOR_INPUT;
					}
				}

				if (CONTROL_key_hit(KEY_W) || CONTROL_key_hit(KEY_S))
				{
					if (CONTROL_key_hit(KEY_W))
					{
						temp = 1;
					}
					else
					{
						temp = -1;
					}

					if (CONTROL_key_down(KEY_LSHIFT) == true)
					{
						PATHS_alter_speed_details (selected_path, 10 * temp);
						PATHS_create_lookup_from_path (selected_path);
					}
					else
					{
						PATHS_alter_speed_details (selected_path, temp);
						PATHS_create_lookup_from_path (selected_path);
					}
				}

				if (CONTROL_key_hit(KEY_E) || CONTROL_key_hit(KEY_D))
				{
					if (CONTROL_key_hit(KEY_E))
					{
						temp = 1;
					}
					else
					{
						temp = -1;
					}

					PATHS_alter_path_details (selected_path, temp);
					PATHS_create_lookup_from_path (selected_path);
				}

				if (CONTROL_key_hit(KEY_U))
				{
					PATHS_cycle_selected_node_speed_curves (selected_path,true);
				}

				if (CONTROL_key_hit(KEY_I))
				{
					PATHS_cycle_selected_node_speed_curves (selected_path,false);
				}

				if (CONTROL_mouse_down(LMB) == true)
				{
					if (editing_mode == DRAGGING_OVER_NODES)
					{
						// Continue dragging the box...

						single_selected_node = UNSET;

						drag_end_x = real_mx;
						drag_end_y = real_my;

						screen_x1 = (drag_start_x - (sx * tilesize)) * zoom_level;
						screen_y1 = (drag_start_y - (sy * tilesize)) * zoom_level;
						screen_x2 = (drag_end_x - (sx * tilesize)) * zoom_level;
						screen_y2 = (drag_end_y - (sy * tilesize)) * zoom_level;

						drag_time++;

						OUTPUT_rectangle (screen_x1,screen_y1,screen_x2,screen_y2,255,255,255);

						PATHS_partially_set_nodes_status (selected_path, drag_start_x, drag_start_y, drag_end_x, drag_end_y, zoom_level, selecting_node_mode, drag_time);
					}
					else if (editing_mode == EDIT_OR_MOVE_NODES)
					{
						// Continue moving the node/bezier controllers.
						drag_dist_x = real_mx - drag_start_x;
						drag_dist_y = real_my - drag_start_y;

						total_drag_offset_x = stored_bezier_x + drag_dist_x;
						total_drag_offset_y = stored_bezier_y + drag_dist_y;

						if (CONTROL_key_down(KEY_ALT) == true)
						{
							total_drag_offset_x -= (total_drag_offset_x % grid_size);
							total_drag_offset_y -= (total_drag_offset_y % grid_size);
						}

						switch (grabbed_handle_type)
						{
						case GRABBED_PATH_NODE_PRE:
							PATHS_set_node_pre_co_ords (selected_path, actual_grabbed_node, total_drag_offset_x, total_drag_offset_y);

							stored_bezier_alternates_angle = float (atan2( -(stored_bezier_x+drag_dist_x), -(stored_bezier_y+drag_dist_y) ));

							if (CONTROL_key_down(KEY_LSHIFT) == true)
							{
								PATHS_set_node_aft_co_ords (selected_path, actual_grabbed_node, int (stored_bezier_alternates_length * float (sin(stored_bezier_alternates_angle))), int (stored_bezier_alternates_length * float (cos(stored_bezier_alternates_angle))) );
							}

							if (CONTROL_key_down(KEY_LCONTROL) == true)
							{
								stored_bezier_alternates_length = MATH_get_distance_int ( 0, 0, total_drag_offset_x, total_drag_offset_y );
								PATHS_set_node_aft_co_ords (selected_path, actual_grabbed_node, -total_drag_offset_x, -total_drag_offset_y );
							}
							break;

						case GRABBED_PATH_NODE_AFT:
							PATHS_set_node_aft_co_ords (selected_path, actual_grabbed_node, total_drag_offset_x, total_drag_offset_y);
							
							stored_bezier_alternates_angle = float (atan2( -(stored_bezier_x+drag_dist_x), -(stored_bezier_y+drag_dist_y) ));
							
							if (CONTROL_key_down(KEY_LSHIFT) == true)
							{
								PATHS_set_node_pre_co_ords (selected_path, actual_grabbed_node, int (stored_bezier_alternates_length * float (sin(stored_bezier_alternates_angle))), int (stored_bezier_alternates_length * float (cos(stored_bezier_alternates_angle))) );
							}

							if (CONTROL_key_down(KEY_LCONTROL) == true)
							{
								stored_bezier_alternates_length = MATH_get_distance_int ( 0, 0, total_drag_offset_x, total_drag_offset_y );
								PATHS_set_node_pre_co_ords (selected_path, actual_grabbed_node, -total_drag_offset_x, -total_drag_offset_y );
							}
							break;

						case GRABBED_PATH_NODE_NODE:
							PATHS_position_selected_nodes (selected_path, drag_dist_x, drag_dist_y, actual_grabbed_node, grid_size, true);
							break;

						case GRABBED_PATH_NODE_X_HANDLE:
							PATHS_position_selected_nodes (selected_path, drag_dist_x, 0, actual_grabbed_node, grid_size, false);
							break;

						case GRABBED_PATH_NODE_Y_HANDLE:
							PATHS_position_selected_nodes (selected_path, 0, drag_dist_y, actual_grabbed_node, grid_size, false);
							break;

						default:
							assert(0);
							break;
						}

						PATHS_create_lookup_from_path (selected_path);
					}
				}
				else
				{
					if (editing_mode == DRAGGING_OVER_NODES)
					{
						// Mouse has been released, so lock down nodes!

						single_selected_node = PATHS_lock_nodes_status (selected_path);

						editing_mode = PATH_SELECTED_WAITING_FOR_INPUT;
					}
					else if (editing_mode == EDIT_OR_MOVE_NODES)
					{
						// Stopped moving 'em!
						PATHS_zero_origin (selected_path);
						PATHS_create_lookup_from_path (selected_path);
						editing_mode = PATH_SELECTED_WAITING_FOR_INPUT;
					}

				}

			}
		}
		else if (override_main_editor == false)
		{
			// Clicks in the panel.

			if (CONTROL_mouse_hit(LMB) == true)
			{

				if (selected_path == UNSET)
				{
					// Clicks to select a path. Except there are none in the panel.

					if (MATH_rectangle_to_point_intersect_by_size (editor_view_width+(ICON_SIZE/2)+(ICON_SIZE*2) , (ICON_SIZE*1), ICON_SIZE, ICON_SIZE, mx, my) == true)
					{
						editing_mode = CREATE_PATHS;
					}
					else if (MATH_rectangle_to_point_intersect_by_size (editor_view_width+(ICON_SIZE/2)+(ICON_SIZE*5) , (ICON_SIZE*1), ICON_SIZE, ICON_SIZE, mx, my) == true)
					{
						editing_mode = SELECTING_PATHS;
					}
				}
				else
				{
					// Clicks to alter the path details...

					if (MATH_rectangle_to_point_intersect_by_size (editor_view_width+(ICON_SIZE/2)+(ICON_SIZE*2) , (ICON_SIZE*1), ICON_SIZE, ICON_SIZE, mx, my) == true)
					{
						editing_mode = CREATE_NODES;
						PATHS_deselect_nodes (selected_path);
						single_selected_node = UNSET;
					}
					else if (MATH_rectangle_to_point_intersect_by_size (editor_view_width+(ICON_SIZE/2)+(ICON_SIZE*5) , (ICON_SIZE*1), ICON_SIZE, ICON_SIZE, mx, my) == true)
					{
						editing_mode = PATH_SELECTED_WAITING_FOR_INPUT;
					}

					for (icon_number=0; icon_number<4; icon_number++)
					{
						if (MATH_rectangle_to_point_intersect_by_size (editor_view_width+(ICON_SIZE/2)+(ICON_SIZE*2)+(icon_number*ICON_SIZE) , (ICON_SIZE*5), ICON_SIZE, ICON_SIZE, mx, my) == true)
						{
							PATHS_set_path_line_type (selected_path,icon_number);
							PATHS_create_lookup_from_path (selected_path);
						}
					}

					PATHS_get_path_loop_details (selected_path, &loop_type, &looped);

					for (icon_number=0; icon_number<4; icon_number++)
					{
						if (MATH_rectangle_to_point_intersect_by_size (editor_view_width+(ICON_SIZE/2)+(ICON_SIZE*2)+(icon_number*ICON_SIZE) , (ICON_SIZE*6), ICON_SIZE, ICON_SIZE, mx, my) == true)
						{
							PATHS_set_path_loop_details (selected_path,icon_number,looped);
						}
					}

					if (MATH_rectangle_to_point_intersect_by_size ( editor_view_width+(ICON_SIZE/2)+(ICON_SIZE*2) , (ICON_SIZE*7), ICON_SIZE, ICON_SIZE, mx, my ) == true)
					{
						PATHS_set_path_loop_details (selected_path,loop_type,true);
						PATHS_create_lookup_from_path (selected_path);
					}
					else if (MATH_rectangle_to_point_intersect_by_size ( editor_view_width+(ICON_SIZE/2)+(ICON_SIZE*5) , (ICON_SIZE*7), ICON_SIZE, ICON_SIZE, mx, my ) == true)
					{
						PATHS_set_path_loop_details (selected_path,loop_type,false);
						PATHS_create_lookup_from_path (selected_path);
					}

					if (MATH_rectangle_to_point_intersect_by_size ( editor_view_width+(ICON_SIZE/2),(ICON_SIZE*3),(ICON_SIZE*7),ICON_SIZE ,mx,my) == true)
					{
						// Clicking to alter the spawn point type.
						override_main_editor = true;
						CONTROL_lock_mouse_button (LMB);
						PATHS_get_path_name (selected_path,path_name);
						PATHS_get_path_name (selected_path,old_path_name);
					}

					if (single_selected_node >= 0)
					{
						// Clicks to alter a node's details.


					}
				}

			}
		}
		else if ( (override_main_editor == true) && (selected_path != UNSET) )
		{
			// Override menu!

			getting_new_name_status = CONTROL_get_word (path_name , getting_new_name_status , NAME_SIZE , false, true);

			OUTPUT_filled_rectangle_by_size ( (game_screen_width/2)-(((FONT_WIDTH/2)*NAME_SIZE)+FONT_WIDTH) , 256 , ((NAME_SIZE+2)*FONT_WIDTH) , ICON_SIZE , 0 , 0 , 0 );
			OUTPUT_rectangle_by_size ( (game_screen_width/2)-(((FONT_WIDTH/2)*NAME_SIZE)+FONT_WIDTH) , 256 , ((NAME_SIZE+2)*FONT_WIDTH) , ICON_SIZE , 0 , 0 , 255 );
			OUTPUT_text ( (game_screen_width/2)-((FONT_WIDTH/2)*NAME_SIZE) , 256+(ICON_SIZE/2)-(FONT_HEIGHT) , path_name );
			OUTPUT_text ( (game_screen_width/2)-((FONT_WIDTH/2)*NAME_SIZE) , 256+(ICON_SIZE/2) , old_path_name , 255,0,255 );

			if (getting_new_name_status == DO_NOTHING)
			{
				if (PATHS_check_for_duplicate_name (selected_path, path_name) == true)
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
					PATHS_register_path_name_change (old_path_name , path_name);
					TILEMAPS_register_parameter_name_change ("PATHS", old_path_name, path_name);
				}						
			}

			if (override_main_editor == false)
			{
				CONTROL_lock_mouse_button (LMB);
			}
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
	
	return override_main_editor;

}






