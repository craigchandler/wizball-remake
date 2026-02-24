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



bool TILECOLOURS_edit_tile_colouring (int state , int *current_cursor ,int sx, int sy,  int tilemap_number, int mx, int my, int zoom_level, int grid_size)
{

	if (state == STATE_INITIALISE)
	{




	}
	else if (state == STATE_SET_UP_BUTTONS)
	{




	}
	else if (state == STATE_RUNNING)
	{




	}
	else if (state == STATE_RESET_BUTTONS)
	{




	}
	else if (state == STATE_SHUTDOWN)
	{




	}

	return true;

}

