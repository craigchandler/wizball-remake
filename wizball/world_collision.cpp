// This file deals with all the tile-based collision McGuffery.

#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include "world_collision.h"
#include "math_stuff.h"
#include "parser.h"
#include "scripting.h"
#include "tilemaps.h"
#include "tilesets.h"

#include "fortify.h"



// What to do:

// Lets do it the maths way, with a CASE'd function saying how far imbedded a pixel is,
// which means finding a fast way to do the circular blocks. Meep!

int block_size = UNSET;
	// The size of the block - MUST BE A POWER OF 2!
int block_size_minus_one = UNSET;
	// So we can mask off everything but the offset.
int block_size_inverse = UNSET;
	// So we can mask off the offset within the block.
int block_size_bitshift = UNSET;
	// So we can turn real_x into block_x nice and fast.

int world_collision_mode = BLOCKMODE_NONE;

int *block_solid_profiles = NULL;
int *block_depth_profiles = NULL;
int *block_exposure_profiles = NULL;
int *tile_priority_list = NULL;
int *tile_boolean_property_list = NULL;

int block_data_size;
int block_offset_data_size;
int block_sided_data_size;
int block_offset_sided_data_size;

int collision_map;
	// This is the map we're currently using as reference for the collision.
	// It's most likely to be the one most prominently shown on-screen.
int collision_data_layers;
int collision_data_width;
int collision_data_height;

int collision_data_width_in_pixels;
int collision_data_height_in_pixels;

int collision_data_layer_size;
	// The dimensions of said map as shortcuts to avoid unnecessary indirection.
char *collision_data_pointer;
	// Pointer to data, again for saving on indirection.
char *exposure_data_pointer;
	// Pointer to data, again for saving on indirection. Lazy me.
short *map_pointer;
	// Pointer to the map data, again blah blah blah.
unsigned char *collision_bitmask_data_pointer;
	// Blaaaaah.
int collision_tileset;
int collision_tilesize;

int collision_inside_check_x_co_ords[MAX_CHECK_POINTS];
int collision_inside_check_y_co_ords[MAX_CHECK_POINTS];

int collision_check_co_ords[MAX_CHECK_POINTS];
int collision_end_co_ords[MAX_CHECK_POINTS];
int collision_result[MAX_CHECK_POINTS];
int collision_collision_depth[MAX_CHECK_POINTS];
int collision_ignore_bitmasks[MAX_CHECK_POINTS];
int collision_interaction_bitmasks[MAX_CHECK_POINTS]; // These are used to detemine whether a given point will interact with the block.
// These contain the positions that the collision routines need to check...

int collision_blocks_found_count;
int collision_blocks_ignore_count;
int collision_blocks_found[MAX_CHECK_POINTS];
int collision_blocks_found_tx[MAX_CHECK_POINTS];
int collision_blocks_found_ty[MAX_CHECK_POINTS];
// This contains the indexes of the blocks which the user-called collision
// routines turn up. The other two contain the positions of said tiles.

// This arrange contains the reflection normal's index of the slopes within each tile.
// The curved tiles have their surfaces averaged out to simple diagonal.
int block_normal_indexes [BLOCK_PROFILE_COUNT] = 
{
	0,0,10,6,14,2,9,9,7,7,15,15,1,1,11,11,13,13,5,5,3,3,4,12,8,0,14,2,10,6,6,10,2,14
};

// This will contain the calculated normals for each of the angles of reflection.
float block_normals [NUMBER_OF_BLOCK_ANGLES][3];

tilemap *collision_map_pointer = NULL;

// This array details those blocks which have edges flush with the sides of the block.
// It's used to determine how the edges should be simulated when creating the offset
// profiles.
int block_covered_sides[BLOCK_PROFILE_COUNT][BLOCK_PROFILE_SIDES] = 
{
	0,0,0,0,
	1,1,1,1,
	1,1,0,0,
	1,0,0,1,
	0,1,1,0,
	0,0,1,1,
	1,1,0,0,
	1,1,0,1,
	1,1,0,1,
	1,0,1,1,
	0,1,1,0,
	0,1,1,1,
	0,1,1,1,
	0,1,0,1,
	1,1,0,0,
	1,1,1,0,
	1,1,1,0,
	0,1,1,0,
	1,0,0,1,
	1,0,1,1,
	1,0,1,1,
	0,0,1,1,
	1,0,1,1,
	1,1,1,0,
	1,1,0,1,
	0,1,1,1,
	0,1,1,0,
	0,0,1,1,
	1,1,0,0,
	1,0,0,1,
	1,0,0,1,
	1,1,0,0,
	0,0,1,1,
	0,1,1,0
};








void WORLDCOLL_calculate_block_angle_normals (void)
{
	float divider = (2.0f * PI) / float (NUMBER_OF_BLOCK_ANGLES);
	float angle;

	int i;

	for (i=0; i<NUMBER_OF_BLOCK_ANGLES; i++)
	{
		angle = float (i) * divider;

		block_normals [i][0] = float(sin(angle));
		block_normals [i][1] = float(cos(angle));
	}
}



#define COLLISION_OVERWRITE_FROM_ALL_TO_ALL						(0)
#define COLLISION_OVERWRITE_FROM_NON_ZERO_TO_CORRESPONDING		(1)
#define COLLISION_OVERWRITE_FROM_ALL_TO_ZERO					(2)

void WORLDCOLL_copy_collision_layer (int tilemap_number, int from_layer, int to_layer, int overwrite_type)
{
	int x,y;
	int from_offset,to_offset;
	int map_w,map_h;

	map_w = cm[tilemap_number].map_width;
	map_h = cm[tilemap_number].map_height;

	switch(overwrite_type)
	{
	case COLLISION_OVERWRITE_FROM_ALL_TO_ALL:
		for (x=0; x<cm[tilemap_number].map_width; x++)
		{
			for (y=0; y<cm[tilemap_number].map_height; y++)
			{
				from_offset = (from_layer * map_h * map_w) + (y * map_w) + x;
				to_offset = (to_layer * map_h * map_w) + (y * map_w) + x;
				
				cm[tilemap_number].collision_data_pointer[to_offset] = cm[tilemap_number].collision_data_pointer[from_offset];
				cm[tilemap_number].exposure_data_pointer[to_offset] = cm[tilemap_number].exposure_data_pointer[from_offset];
				cm[tilemap_number].collision_bitmask_data_pointer[to_offset] = cm[tilemap_number].collision_bitmask_data_pointer[from_offset];
			}
		}
		break;

	case COLLISION_OVERWRITE_FROM_NON_ZERO_TO_CORRESPONDING:
		for (x=0; x<cm[tilemap_number].map_width; x++)
		{
			for (y=0; y<cm[tilemap_number].map_height; y++)
			{
				from_offset = (from_layer * map_h * map_w) + (y * map_w) + x;
				to_offset = (to_layer * map_h * map_w) + (y * map_w) + x;
				
				if (cm[tilemap_number].collision_data_pointer[from_offset] != 0)
				{
					cm[tilemap_number].collision_data_pointer[to_offset] = cm[tilemap_number].collision_data_pointer[from_offset];
					cm[tilemap_number].exposure_data_pointer[to_offset] = cm[tilemap_number].exposure_data_pointer[from_offset];
					cm[tilemap_number].collision_bitmask_data_pointer[to_offset] = cm[tilemap_number].collision_bitmask_data_pointer[from_offset];
				}
			}
		}
		break;

	case COLLISION_OVERWRITE_FROM_ALL_TO_ZERO:
		for (x=0; x<cm[tilemap_number].map_width; x++)
		{
			for (y=0; y<cm[tilemap_number].map_height; y++)
			{
				from_offset = (from_layer * map_h * map_w) + (y * map_w) + x;
				to_offset = (to_layer * map_h * map_w) + (y * map_w) + x;
				
				if (cm[tilemap_number].collision_data_pointer[to_offset] == 0)
				{
					cm[tilemap_number].collision_data_pointer[to_offset] = cm[tilemap_number].collision_data_pointer[from_offset];
					cm[tilemap_number].exposure_data_pointer[to_offset] = cm[tilemap_number].exposure_data_pointer[from_offset];
					cm[tilemap_number].collision_bitmask_data_pointer[to_offset] = cm[tilemap_number].collision_bitmask_data_pointer[from_offset];
				}
			}
		}
		break;
		
	default:
		assert(0);
		break;
	}

}



void WORLDCOLL_generate_collision_map (int tilemap_number, bool generate_optimisation_data , bool is_it_for_physics)
{
	int tileset_number;
	int tile_number;
	int tile_collision_profile;
	int default_tile_exposure;
	int actual_tile_exposure;

	int map_w,map_h,map_l;

	int x,y,layer;
	int counter;

	map_w = cm[tilemap_number].map_width;
	map_h = cm[tilemap_number].map_height;
	map_l = cm[tilemap_number].map_layers;

	tileset_number = cm[tilemap_number].tile_set_index;

	if (tileset_number != UNSET)
	{
		cm[tilemap_number].collision_data_pointer = (char *) malloc (map_w * map_h * map_l * sizeof(char));
		cm[tilemap_number].exposure_data_pointer = (char *) malloc (map_w * map_h * map_l * sizeof(char));
		cm[tilemap_number].collision_bitmask_data_pointer = (unsigned char *) malloc (map_w * map_h * map_l * sizeof(unsigned char));

		if (generate_optimisation_data)
		{
			cm[tilemap_number].optimisation_data = (short *) malloc (map_w * map_h * map_l * sizeof(short));
			cm[tilemap_number].optimisation_data_flag = true;
		}

		for (layer=0; layer<cm[tilemap_number].map_layers; layer++)
		{
			// First of all we fill in both the tile's shape and it's default
			// solidity profile.

			for (x=0; x<cm[tilemap_number].map_width; x++)
			{
				for (y=0; y<cm[tilemap_number].map_height; y++)
				{
					tile_number = cm[tilemap_number].map_pointer[(layer * map_h * map_w) + (y * map_w) + x];
					tile_collision_profile = ts[tileset_number].tileset_pointer[tile_number].shape;
					cm[tilemap_number].collision_data_pointer[(layer * map_h * map_w) + (y * map_w) + x] = tile_collision_profile;
					default_tile_exposure = ts[tileset_number].tileset_pointer[tile_number].solid_sides;
					cm[tilemap_number].exposure_data_pointer[(layer * map_h * map_w) + (y * map_w) + x] = default_tile_exposure;
					cm[tilemap_number].collision_bitmask_data_pointer[(layer * map_h * map_w) + (y * map_w) + x] = ts[tileset_number].tileset_pointer[tile_number].collision_bitmask;
				}
			}

			if (generate_optimisation_data)
			{
				for (y=0; y<cm[tilemap_number].map_height; y++)
				{
					counter = 0;
					for (x=cm[tilemap_number].map_width-1; x>=0; x--)
					{
						tile_number = cm[tilemap_number].map_pointer[(layer * map_h * map_w) + (y * map_w) + x];
						
						if (tile_number == 0)
						{
							cm[tilemap_number].optimisation_data[(layer * map_h * map_w) + (y * map_w) + x] = counter;
							counter++;
						}
						else
						{
							cm[tilemap_number].optimisation_data[(layer * map_h * map_w) + (y * map_w) + x] = 0;
							counter=0;
						}
					}
				}
			}

			// After each map layer is generated we then go through this and
			// limit the exposure map according to whether each tile is surrounded.

			// I just realised that we should use all the default exposure for blocks and that this
			// data, when correctly created, will only be of use to the vector map building.

			for (x=0; x<cm[tilemap_number].map_width; x++)
			{
				for (y=0; y<cm[tilemap_number].map_height; y++)
				{
					// Get the current default tile exposure.
					actual_tile_exposure = cm[tilemap_number].exposure_data_pointer[ (layer * map_h * map_w) + (y * map_w) + x];

					if (is_it_for_physics)
					{
						if ( (actual_tile_exposure & DIRECTION_BITVALUE_UP) > 0)
						{
							// Check above for a tile!
							if (y>0)
							{
								// If we're not in the top row of a map...
								if (cm[tilemap_number].collision_data_pointer[(layer * map_h * map_w) + ((y-1) * map_w) + x] > 0)
								{
									// If there's a tile above us then get rid of the top solidity.
									actual_tile_exposure &= (DIRECTION_BITVALUE_TOTAL-DIRECTION_BITVALUE_UP);
								}
							}
							else
							{
								// But if we are, remove the top solidity.
								actual_tile_exposure &= (DIRECTION_BITVALUE_TOTAL-DIRECTION_BITVALUE_UP);
							}
						}
						
						if ( (actual_tile_exposure & DIRECTION_BITVALUE_DOWN) > 0)
						{
							// Check below for a tile!
							if (y<map_h-1)
							{
								// If we're not in the bottom row of a map...
								if (cm[tilemap_number].collision_data_pointer[(layer * map_h * map_w) + ((y+1) * map_w) + x] > 0)
								{
									// If there's a tile below us then get rid of the bottom solidity.
									actual_tile_exposure &= (DIRECTION_BITVALUE_TOTAL-DIRECTION_BITVALUE_DOWN);
								}
							}
							else
							{
								// But if we are, remove the bottom solidity.
								actual_tile_exposure &= (DIRECTION_BITVALUE_TOTAL-DIRECTION_BITVALUE_DOWN);
							}
						}

						if ( (actual_tile_exposure & DIRECTION_BITVALUE_RIGHT) > 0)
						{
							// Check to the right for a tile!
							if (x<map_w-1)
							{
								// If we're not in the right-most column of a map...
								if (cm[tilemap_number].collision_data_pointer[(layer * map_h * map_w) + (y * map_w) + x + 1] > 0)
								{
									// If there's a tile to the right of us then get rid of the right solidity.
									actual_tile_exposure &= (DIRECTION_BITVALUE_TOTAL-DIRECTION_BITVALUE_RIGHT);
								}
							}
							else
							{
								// But if we are, remove the right solidity.
								actual_tile_exposure &= (DIRECTION_BITVALUE_TOTAL-DIRECTION_BITVALUE_RIGHT);
							}
						}

						if ( (actual_tile_exposure & DIRECTION_BITVALUE_LEFT) > 0)
						{
							// Check to the left for a tile!
							if (x>0)
							{
								// If we're not in the left-most column of a map...
								if (cm[tilemap_number].collision_data_pointer[(layer * map_h * map_w) + (y * map_w) + x - 1] > 0)
								{
									// If there's a tile to the left of us then get rid of the left solidity.
									actual_tile_exposure &= (DIRECTION_BITVALUE_TOTAL-DIRECTION_BITVALUE_LEFT);
								}
							}
							else
							{
								// But if we are, remove the left solidity.
								actual_tile_exposure &= (DIRECTION_BITVALUE_TOTAL-DIRECTION_BITVALUE_LEFT);
							}
						}
					}

					// Then plonk this newly altered figure back into the map. Lubbly.
					cm[tilemap_number].exposure_data_pointer[ (layer * map_h * map_w) + (y * map_w) + x] = actual_tile_exposure;
				}
			}
		}
	}

}



void WORLDCOLL_generate_collision_maps (bool generate_optimisation_data , bool is_it_for_physics)
{
	// This goes through all the maps, allocates space for collision data and then
	// fills it in based on the tile profiles of the tileset for that map.

	int tilemap_number;

	for (tilemap_number=0; tilemap_number<number_of_tilemaps_loaded; tilemap_number++)
	{
		WORLDCOLL_generate_collision_map (tilemap_number, generate_optimisation_data , is_it_for_physics);
	}
}



void WORLDCOLL_set_collision_map (int tilemap_number)
{
	if (tilemap_number < 0 || tilemap_number >= number_of_tilemaps_loaded)
	{
		collision_map = UNSET;
		collision_map_pointer = NULL;
		collision_data_layers = 0;
		collision_data_width = 0;
		collision_data_height = 0;
		collision_data_layer_size = 0;
		collision_data_pointer = NULL;
		exposure_data_pointer = NULL;
		collision_bitmask_data_pointer = NULL;
		map_pointer = NULL;
		collision_tileset = UNSET;
		collision_tilesize = 0;
		collision_data_width_in_pixels = 0;
		collision_data_height_in_pixels = 0;
		return;
	}

	collision_map = tilemap_number;
	collision_map_pointer = &cm[collision_map];
	collision_data_layers = cm[collision_map].map_layers;
	collision_data_width = cm[collision_map].map_width;
	collision_data_height = cm[collision_map].map_height;
	collision_data_layer_size = collision_data_width * collision_data_height;
	collision_data_pointer = cm[collision_map].collision_data_pointer;
	exposure_data_pointer = cm[collision_map].exposure_data_pointer;
	collision_bitmask_data_pointer = cm[collision_map].collision_bitmask_data_pointer;
	map_pointer = cm[collision_map].map_pointer;
	collision_tileset = cm[tilemap_number].tile_set_index;
	if (collision_tileset < 0 || collision_tileset >= number_of_tilesets_loaded)
	{
		collision_tilesize = 0;
		collision_data_width_in_pixels = 0;
		collision_data_height_in_pixels = 0;
		return;
	}

	collision_tilesize = ts[collision_tileset].tilesize;
	collision_data_width_in_pixels = collision_data_width * collision_tilesize;
	collision_data_height_in_pixels = collision_data_height * collision_tilesize;
}



void WORLDCOLL_get_block_extents (int block, int x, int *start_y, int *end_y)
{
	float x_percent;
	
	x_percent = (float) x / (float) block_size * 0.5f * PI;

	switch (block)
	{
		// Empty
		
	case 0:
		*start_y = 0;
		*end_y = 0;
		break;

		// Full

	case 1:
		*start_y = 0;
		*end_y = block_size;
		break;

		// Diagonals

	case 2:
		*start_y = 0;
		*end_y = x+1;
		break;

	case 3:
		*start_y = 0;
		*end_y = block_size-x;
		break;

	case 4:
		*start_y = (block_size - x) - 1;
		*end_y = block_size;
		break;

	case 5:
		*start_y = x;
		*end_y = block_size;
		break;

		//  Floor slopes

	case 6:
		*start_y = 0;
		*end_y = (x/2) + 1;
		break;

	case 7:
		*start_y = 0;
		*end_y = (x/2) + (block_size/2) + 1;
		break;

	case 8:
		*start_y = 0;
		*end_y = block_size - (x/2);
		break;

	case 9:
		*start_y = 0;
		*end_y = block_size - (x/2) - (block_size/2);
		break;

		// Roof slopes

	case 10:
		*start_y = (block_size - x/2) - 1;
		*end_y = block_size;
		break;

	case 11:
		*start_y = (block_size - x/2) - (block_size/2) - 1;
		*end_y = block_size;
		break;
		
	case 12:
		*start_y = (x/2);
		*end_y = block_size;
		break;

	case 13:
		*start_y = (x/2) + (block_size/2);
		*end_y = block_size;
		break;

		// Right side slopes

	case 14:
		*start_y = 0;
		*end_y = (x<block_size/2 ? 0 : ((x*2) + 2) - block_size);
		break;

	case 15:
		*start_y = 0;
		*end_y = (x<block_size/2 ? (x*2) + 2 : block_size);
		break;

	case 16:
		*start_y = (x<block_size/2 ? block_size - (x*2) - 2 : 0);
		*end_y = block_size;
		break;

	case 17:
		*start_y = (x<block_size/2 ? block_size : block_size - (x*2 - block_size) - 2);
		*end_y = block_size;
		break;

		// Left side slopes

	case 18:
		*start_y = 0;
		*end_y = (x<block_size/2 ? block_size - (x*2) : 0);
		break;

	case 19:
		*start_y = 0;
		*end_y = (x<block_size/2 ? block_size : (block_size*2) - (x*2));
		break;

	case 20:
		*start_y = (x<block_size/2 ? 0 : x*2 - block_size);
		*end_y = block_size;
		break;

	case 21:
		*start_y = (x<block_size/2 ? x*2 : block_size);
		*end_y = block_size;
		break;

		// Half-blocks

	case 22:
		*start_y = 0;
		*end_y = (x<block_size/2 ? block_size : 0);
		break;

	case 23:
		*start_y = 0;
		*end_y = (x<block_size/2 ? 0 : block_size);
		break;

	case 24:
		*start_y = 0;
		*end_y = (block_size/2);
		break;

	case 25:
		*start_y = block_size/2;
		*end_y = block_size;
		break;

		// Outer curves

	case 26:
		*start_y = block_size - int ((float) block_size * cos (x_percent - PI/2.0f));
		*end_y = block_size;
		break;

	case 27:
		*start_y = block_size - int ((float) block_size * cos (x_percent));
		*end_y = block_size;
		break;

	case 28:
		*start_y = 0;
		*end_y = int ((float) block_size * cos (x_percent - PI/2.0f));
		break;

	case 29:
		*start_y = 0;
		*end_y = int ((float) block_size * cos (x_percent));
		break;

		// Inner curves

	case 30:
		*start_y = 0;
		*end_y = block_size - int ((float) block_size * cos (x_percent - PI/2.0f));
		break;

	case 31:
		*start_y = 0;
		*end_y = block_size - int ((float) block_size * cos (x_percent));
		break;

	case 32:
		*start_y = int ((float) block_size * cos (x_percent - PI/2.0f));
		*end_y = block_size;
		break;

	case 33:
		*start_y = int ((float) block_size * cos (x_percent));
		*end_y = block_size;
		break;

	default:
		assert(0);
	}

}



void WORLDCOLL_destroy_block_collision_shapes (void)
{
	if (block_solid_profiles != NULL)
	{
		free (block_solid_profiles);
		block_solid_profiles = NULL;
	}

	if (block_depth_profiles != NULL)
	{
		free (block_depth_profiles);
		block_depth_profiles = NULL;
	}

	if (block_exposure_profiles != NULL)
	{
		free (block_exposure_profiles);
		block_exposure_profiles = NULL;
	}
	
	if (tile_priority_list != NULL)
	{
		free (tile_priority_list);
		tile_priority_list = NULL;
	}

	if (tile_boolean_property_list != NULL)
	{
		free (tile_boolean_property_list);
		tile_boolean_property_list = NULL;
	}
}



void WORLDCOLL_setup_block_collision (int new_block_size)
{
	block_size = new_block_size;

	if (block_solid_profiles != NULL)
	{
		free (block_solid_profiles);
		block_solid_profiles = NULL;
	}

	if (block_depth_profiles != NULL)
	{
		free (block_depth_profiles);
		block_depth_profiles = NULL;
	}

	if (block_exposure_profiles != NULL)
	{
		free (block_exposure_profiles);
		block_exposure_profiles = NULL;
	}

	if (tile_priority_list != NULL)
	{
		free (tile_priority_list);
		tile_priority_list = NULL;
	}

	if (tile_boolean_property_list != NULL)
	{
		free (tile_boolean_property_list);
		tile_boolean_property_list = NULL;
	}

	block_solid_profiles = (int *) malloc (BLOCK_PROFILE_COUNT * block_size * block_size * sizeof(int) );
	block_depth_profiles = (int *) malloc (BLOCK_PROFILE_COUNT * block_size * block_size * BLOCK_PROFILE_SIDES * sizeof(int) );
	block_exposure_profiles = (int *) malloc (BLOCK_PROFILE_COUNT * block_size * block_size * sizeof(int) );

	block_data_size = block_size * block_size;
	block_sided_data_size = block_data_size * BLOCK_PROFILE_SIDES;

	block_offset_data_size = block_size * block_size * BLOCK_OFFSET_COUNT;
	block_offset_sided_data_size = block_offset_data_size * BLOCK_PROFILE_SIDES;

	block_size_minus_one = block_size - 1;
	block_size_inverse = ~block_size_minus_one;

	block_size_bitshift = 0;

	int temp = block_size;

	while (temp != 1)
	{
		block_size_bitshift++;
		temp >>= 1;
	}

	int start_y,end_y;
	int block_number,x,y;

	// Define the basic block shapes

	for (block_number=0; block_number<BLOCK_PROFILE_COUNT; block_number++)
	{
		for (x=0; x<block_size; x++)
		{
			WORLDCOLL_get_block_extents (block_number, x, &start_y, &end_y);
			
			for (y=0; y<start_y; y++)
			{
				block_solid_profiles[(block_number * block_data_size) + (y * block_size) + x] = 0;
			}

			for (y=start_y; y<end_y; y++)
			{
				block_solid_profiles[(block_number * block_data_size) + (y * block_size) + x] = 1;
			}

			for (y=end_y; y<block_size; y++)
			{
				block_solid_profiles[(block_number * block_data_size) + (y * block_size) + x] = 0;
			}
		}
	}
	
	// Define the depth maps...

	int counter;
	int direction;

	for (block_number=0; block_number<BLOCK_PROFILE_COUNT; block_number++)
	{
		direction = DIRECTION_UP;

		for (x=0; x<block_size; x++)
		{
			counter = 0;

			for (y=0; y<block_size; y++)
			{
				if (block_solid_profiles[(block_number * block_data_size) + (y * block_size) + x] != 0)
				{
					counter -= block_solid_profiles[(block_number * block_data_size) + (y * block_size) + x];
				}
				else
				{
					counter = 0;
				}

				block_depth_profiles[(block_number * block_sided_data_size) + (direction * block_data_size) + (y * block_size) + x] = counter;
			}
		}

		direction = DIRECTION_DOWN;

		for (x=0; x<block_size; x++)
		{
			counter = 0;

			for (y=block_size-1; y>=0; y--)
			{
				if (block_solid_profiles[(block_number * block_data_size) + (y * block_size) + x] != 0)
				{
					counter += block_solid_profiles[(block_number * block_data_size) + (y * block_size) + x];
				}
				else
				{
					counter = 0;
				}
				block_depth_profiles[(block_number * block_sided_data_size) + (direction * block_data_size) + (y * block_size) + x] = counter;
			}
		}

		direction = DIRECTION_LEFT;

		for (y=0; y<block_size; y++)
		{
			counter = 0;

			for (x=0; x<block_size; x++)
			{
				if (block_solid_profiles[(block_number * block_data_size) + (y * block_size) + x] != 0)
				{
					counter -= block_solid_profiles[(block_number * block_data_size) + (y * block_size) + x];
				}
				else
				{
					counter = 0;
				}

				block_depth_profiles[(block_number * block_sided_data_size) + (direction * block_data_size) + (y * block_size) + x] = counter;
			}
		}

		direction = DIRECTION_RIGHT;

		for (y=0; y<block_size; y++)
		{
			counter = 0;

			for (x=block_size-1; x>=0; x--)
			{
				if (block_solid_profiles[(block_number * block_data_size) + (y * block_size) + x] != 0)
				{
					counter += block_solid_profiles[(block_number * block_data_size) + (y * block_size) + x];
				}
				else
				{
					counter = 0;
				}

				block_depth_profiles[(block_number * block_sided_data_size) + (direction * block_data_size) + (y * block_size) + x] = counter;
			}
		}

	}

	// Define the instruction maps by first filling them up to the brim with 0's...

	for (block_number=0; block_number<BLOCK_PROFILE_COUNT; block_number++)
	{
		for (x=0; x<block_size; x++)
		{
			for (y=0; y<block_size; y++)
			{
				block_exposure_profiles[(block_number * block_data_size) + (y * block_size) + x] = 0;
			}
		}
	}

	// Then go through 'em one direction at a time, pretending the block is flanked on all the sides other than
	// the direction being generated.

	// Define the instruction maps by first filling them up to the brim with 0's...

	int offset_index;
	int total;

	for (block_number=0; block_number<BLOCK_PROFILE_COUNT; block_number++)
	{

		for (x=0; x<block_size; x++)
		{
			for (y=0; y<block_size; y++)
			{

				if (block_solid_profiles[(block_number * block_data_size) + (y * block_size) + x] == 1)
				{
					total = DIRECTION_BITVALUE_TOTAL;

					// Only fill in offset data for blocks which are currently solid...

					if (x>0)
					{
						// If we're not on the left edge of the board then get the value from the solid block data.
						if (block_solid_profiles[(block_number * block_data_size) + (y * block_size) + (x-1)])
						{
							total -= DIRECTION_BITVALUE_LEFT;
						}
					}
					else
					{
						// Otherwise set it according to what would normally be beside the block.
						if (block_covered_sides[block_number][DIRECTION_LEFT])
						{
							total -= DIRECTION_BITVALUE_LEFT;
						}
					}

					if (x<block_size-1)
					{
						// If we're not on the right edge of the board then get the value from the solid block data.
						if (block_solid_profiles[(block_number * block_data_size) + (y * block_size) + (x+1)])
						{
							total -= DIRECTION_BITVALUE_RIGHT;
						}
					}
					else
					{
						// Otherwise set it according to what would normally be beside the block.
						if (block_covered_sides[block_number][DIRECTION_RIGHT])
						{
							total -= DIRECTION_BITVALUE_RIGHT;
						}
					}

					if (y>0)
					{
						// If we're not on the top edge of the board then get the value from the solid block data.
						if (block_solid_profiles[(block_number * block_data_size) + ((y-1) * block_size) + (x)])
						{
							total -= DIRECTION_BITVALUE_UP;
						}
					}
					else
					{
						// Otherwise set it according to what would normally be beside the block.
						if (block_covered_sides[block_number][DIRECTION_UP])
						{
							total -= DIRECTION_BITVALUE_UP;
						}				
					}

					if (y<block_size-1)
					{
						// If we're not on the bottom edge of the board then get the value from the solid block data.
						if (block_solid_profiles[(block_number * block_data_size) + ((y+1) * block_size) + (x)])
						{
							total -= DIRECTION_BITVALUE_DOWN;
						}
					}
					else
					{
						// Otherwise set it according to what would normally be beside the block.
						if (block_covered_sides[block_number][DIRECTION_DOWN])
						{
							total -= DIRECTION_BITVALUE_DOWN;
						}
					}

				}
				else
				{
					total = EXPOSURE_MAP_CARRY_ON;
				}

				offset_index = (block_number * block_data_size) + (y * block_size) + x;

				block_exposure_profiles [offset_index] = total;

			}
		}
	}

	WORLDCOLL_calculate_block_angle_normals();

	// And then we make the tile detail shortcuts.

	tile_priority_list = (int *) malloc (ts[collision_tileset].tile_count * sizeof(int));
	tile_boolean_property_list = (int *) malloc (ts[collision_tileset].tile_count * sizeof(int));

	for (block_number = 0; block_number<ts[collision_tileset].tile_count; block_number++)
	{
		tile_priority_list[block_number] = ts[collision_tileset].tileset_pointer[block_number].priority;
		tile_boolean_property_list[block_number] = ts[collision_tileset].tileset_pointer[block_number].boolean_properties;
	}

}



int WORLDCOLL_collision_angle (int layer, int x , int y)
{
	// SCRAP THIS! Instead if the remainder is tilesize-1 or 0 on either axis
	// then it's a straight bounce off of that side, otherwise we assume one angle
	// for the rest of it.

	// However what about the corners? Well, I guess we just check for solidity
	// to the sides of them to guess which angle the item is probably hitting from.
	// This won't work with bullets inside collision so well, but sod them.

	int bx,by;
	int block_number;
	int block_offset;
	
	bx = x >> block_size_bitshift;
	by = y >> block_size_bitshift;

	if ( (bx<0) || (bx>=collision_data_width) || (by<0) || (by>=collision_data_height) )
	{
		return UNSET;
	}

	block_offset = (layer * collision_data_layer_size) + (by * collision_data_width) + bx;

	x &= block_size_minus_one;
	y &= block_size_minus_one;

	if (x == 0)
	{
		if (y == 0)
		{
			// Top left corner - check for neighbouring solidity before responding
			// For now we'll just assume a diagonal;
			return 14;
		}
		else if (y == block_size_minus_one)
		{
			// Bottom left corner - check for neighbouring solidity before responding
			// For now we'll just assume a diagonal;
			return 10;
		}
		else
		{
			// Left side!
			return NORMAL_INDEX_LEFT;
		}
	}
	else if (x == block_size_minus_one)
	{
		if (y == 0)
		{
			// Top right corner - check for neighbouring solidity before responding
			// For now we'll just assume a diagonal;
			return 2;
		}
		else if (y == block_size_minus_one)
		{
			// Bottom right corner - check for neighbouring solidity before responding
			// For now we'll just assume a diagonal;
			return 6;
		}
		else
		{
			// Right side!
			return NORMAL_INDEX_RIGHT;
		}
	}
	else
	{
		if (y == 0)
		{
			// Top surface!
			return NORMAL_INDEX_UP;
		}
		else if (y == block_size_minus_one)
		{
			// Bottom surface!
			return NORMAL_INDEX_DOWN;
		}
		else
		{
			// Internal pixel!
			block_number = collision_data_pointer[block_offset];
			return block_normal_indexes [block_number];
		}
	}


}



int WORLDCOLL_collision_depth (int direction, int direction_bitmask , int layer , int x , int y, int collision_bitmask, int world_edge_hit)
{
	int bx,by;
	int block_number;
	int block_offset;

	bx = x >> block_size_bitshift;
	by = y >> block_size_bitshift;

	if ( (bx<0) || (bx>=collision_data_width) || (by<0) || (by>=collision_data_height) )
	{
		if (world_edge_hit)
		{
			switch(direction)
			{
			case DIRECTION_LEFT:
				if (world_edge_hit & COLLISION_HORIZONTAL_WORLD_EDGE_SOLID)
				{
					return x-(collision_data_width<<block_size_bitshift);
				}
				break;

			case DIRECTION_RIGHT:
				if (world_edge_hit & COLLISION_HORIZONTAL_WORLD_EDGE_SOLID)
				{
					return -x;
				}
				break;

			case DIRECTION_UP:
				if (world_edge_hit & COLLISION_VERTICAL_WORLD_EDGE_SOLID)
				{
					return y-(collision_data_height<<block_size_bitshift);
				}
				break;

			case DIRECTION_DOWN:
				if (world_edge_hit & COLLISION_VERTICAL_WORLD_EDGE_SOLID)
				{
					return -y;
				}
				break;

			default:
				assert(0);
				break;
			}
		}
		else
		{
			return 0;
		}
	}

	block_offset = (layer * collision_data_layer_size) + (by * collision_data_width) + bx;

	if ( (direction_bitmask) & (exposure_data_pointer[block_offset]) )
	{
		return 0;
	}

	if ((collision_bitmask & collision_bitmask_data_pointer[block_offset]) == 0)
	{
		return 0;
	}

	block_number = collision_data_pointer[block_offset];

	x &= block_size_minus_one;
	y &= block_size_minus_one;

	return block_depth_profiles[(block_number * block_sided_data_size) + (direction * block_data_size) + (y * block_size) + x];
}



void WORLDCOLL_set_map_collision_block (int layer, int start_x, int start_y, int end_x, int end_y, int equivalent_tile)
{
	int block_offset;
	int collision_bitmask = ts[collision_tileset].tileset_pointer[equivalent_tile].collision_bitmask;
	int collision_shape = ts[collision_tileset].tileset_pointer[equivalent_tile].shape;
	int exposure_mask = ts[collision_tileset].tileset_pointer[equivalent_tile].solid_sides;
	int bx,by;

	if (start_x<0)
	{
		start_x = 0;
	}

	if (start_y<0)
	{
		start_y = 0;
	}

	if (end_x >= collision_data_width)
	{
		end_x = collision_data_width-1;
	}

	if (end_y >= collision_data_height)
	{
		end_y = collision_data_height-1;
	}

	for (bx=start_x; bx<=end_x; bx++)
	{
		for (by=start_y; by<=end_y; by++)
		{
			block_offset = (layer * collision_data_layer_size) + (by * collision_data_width) + bx;

			collision_data_pointer[block_offset] = collision_shape;
			exposure_data_pointer[block_offset] = exposure_mask;
			collision_bitmask_data_pointer[block_offset] = collision_bitmask;
		}
	}

}



int WORLDCOLL_find_next_zone_of_type (int current_zone)
{
	if (collision_map_pointer == NULL)
	{
		return UNSET;
	}

	int zone_number;
	
	int zone_type = collision_map_pointer->zone_list_pointer[current_zone].type_index;
	int zone_count = collision_map_pointer->zones;

	for (zone_number=current_zone+1; zone_number<zone_count; zone_number++)
	{
		if (collision_map_pointer->zone_list_pointer[zone_number].type_index == zone_type)
		{
			return zone_number;
		}
	}

	return UNSET;
}



int WORLDCOLL_find_first_zone_of_type (int zone_type)
{
	if (collision_map_pointer == NULL)
	{
		return UNSET;
	}

	int zone_number;
	int zone_count = collision_map_pointer->zones;

	for (zone_number=0; zone_number<zone_count; zone_number++)
	{
		if (collision_map_pointer->zone_list_pointer[zone_number].type_index == zone_type)
		{
			return zone_number;
		}
	}

	return UNSET;
}



void WORLDCOLL_set_map_collision_block_real_co_ords (int layer, int start_x, int start_y, int end_x, int end_y, int equivalent_tile)
{
	start_x >>= block_size_bitshift;
	start_y >>= block_size_bitshift;
	end_x >>= block_size_bitshift;
	end_y >>= block_size_bitshift;

	WORLDCOLL_set_map_collision_block (layer, start_x, start_y, end_x, end_y, equivalent_tile);
}



int WORLDCOLL_collision_test (int layer , int x , int y, int collision_bitmask)
{
	int bx,by;
	int block_number;
	int block_offset;
	
	bx = x >> block_size_bitshift;
	by = y >> block_size_bitshift;

	if ( (bx<0) || (bx>=collision_data_width) || (by<0) || (by>=collision_data_height) )
	{
		return 0;
	}

	block_offset = (layer * collision_data_layer_size) + (by * collision_data_width) + bx;

	if ((collision_bitmask & collision_bitmask_data_pointer[block_offset]) == 0)
	{
		return 0;
	}

	block_number = collision_data_pointer[block_offset];

	x &= block_size_minus_one;
	y &= block_size_minus_one;

	return block_solid_profiles[(block_number * block_data_size) + (y * block_size) + x];
}



int WORLDCOLL_get_total_collision_depth_horizontal (int direction, int direction_bitmask , int layer, int x , int y, int collision_bitmask, int world_edge_hit)
{
	int result;

	// In depth maps values on the top and left surfaces have negative values.

	while ( ((result = WORLDCOLL_collision_depth (direction, direction_bitmask , layer , x , y, collision_bitmask, world_edge_hit)) != 0) )
	{
		x += result;
	}

	return x;
}



int WORLDCOLL_get_total_collision_depth_vertical (int direction, int direction_bitmask , int layer, int x , int y, int collision_bitmask , int world_edge_hit )
{
	int result;

	// In depth maps values on the top and left surfaces have negative values.

	while ((result = WORLDCOLL_collision_depth (direction, direction_bitmask , layer , x , y, collision_bitmask , world_edge_hit )) != 0)
	{
		y += result;
	}

	return y;
}



int WORLDCOLL_collision_offset (int layer, int x , int y, int collision_bitmask, int world_edge_hit)
{
	int bx,by;
	int block_number;
	int block_offset;
	int ox,oy;

	if (world_edge_hit & (COLLISION_HORIZONTAL_WORLD_EDGE_SOLID | COLLISION_VERTICAL_WORLD_EDGE_SOLID))
	{
		if ( (world_edge_hit & COLLISION_HORIZONTAL_WORLD_EDGE_SOLID) && ( (x<0) || (x>=collision_data_width_in_pixels) ) )
		{
			return 0;
		}
		else if ( (world_edge_hit & COLLISION_VERTICAL_WORLD_EDGE_SOLID) && ( (y<0) || (y>=collision_data_height_in_pixels) ) )
		{
			return 0;
		}
	}

	bx = x >> block_size_bitshift;
	by = y >> block_size_bitshift;

	ox = x & block_size_minus_one;
	oy = y & block_size_minus_one;

	block_offset = (layer * collision_data_layer_size) + (by * collision_data_width) + bx;

	if ((collision_bitmask & collision_bitmask_data_pointer[block_offset]) == 0)
	{
		return EXPOSURE_MAP_CARRY_ON;
	}

	block_number = collision_data_pointer[block_offset];

	return block_exposure_profiles[(block_number * block_data_size) + (oy * block_size) + ox];
}



int WORLDCOLL_tile_value (int layer, int x , int y)
{
	int bx,by;
	
	bx = x >> block_size_bitshift;
	by = y >> block_size_bitshift;

	return map_pointer[(layer * collision_data_layer_size) + (by * collision_data_width) + bx];
}



void WORLDCOLL_add_tile_to_user_list (int layer, int x , int y, int ignore=0)
{
	int bx,by,tile_number;
	
	bx = x >> block_size_bitshift;
	by = y >> block_size_bitshift;

	tile_number = map_pointer[(layer * collision_data_layer_size) + (by * collision_data_width) + bx];

	if ((tile_number & tile_boolean_property_list[tile_number]) == 0)
	{
		collision_blocks_found[collision_blocks_found_count] = tile_number;
		collision_blocks_found_tx[collision_blocks_found_count] = bx;
		collision_blocks_found_ty[collision_blocks_found_count] = by;
		collision_blocks_found_count++;
	}
}



int WORLDCOLL_tile_exposure_value (int layer, int x , int y)
{
	int bx,by;
	
	bx = x >> block_size_bitshift;
	by = y >> block_size_bitshift;

	return exposure_data_pointer[(layer * collision_data_layer_size) + (by * collision_data_width) + bx];
}



#define ENTITY_MOVEMENT_HORIZONTAL_VERTICAL				(0)
#define ENTITY_MOVEMENT_VERTICAL_HORIZONTAL				(1)
	// These are the standard movement modes for a game
	// with slopes and semi-permeable platforms.
#define ENTITY_MOVEMENT_SIMULTANEOUS_PENETRATIVE		(2)
	// This is for things like bullets which don't need to do
	// anything when they hit the world except go "sptang".
#define ENTITY_MOVEMENT_SIMULTANEOUS_NON_PENETRATIVE	(3)
	// This is for things like reflective bullets and smaller
	// items which will need to reflect off surfaces cleverly.
#define ENTITY_MOVEMENT_VERY_SIMPLE_HORI_VERT			(4)
#define ENTITY_MOVEMENT_VERY_SIMPLE_VERT_HORI			(5)
	// This is the movement mode for games with mostly square
	// tiles and lots of things interacting with the landscape
	// where speed is of the essence.

#define RESULT_COLLISION_OCCURRED						0
#define RESULT_COLLISION_IGNORED						1
#define RESULT_NO_COLLISION								2
#define RESULT_PARTIALLY_IGNORED_COLLISION				3

#define NEITHER_CORNER									0
#define FIRST_CORNER									1
#define SECOND_CORNER									2

int WORLDCOLL_push_entity_horizontal (int *entity_pointer, int x_vel, int *which_corner, int coll_type, bool *not_collided)
{
	// This function will attempt to move the specified entity the specified distance.
	// If it is unable to do so then it overrides the entities variables so that it doesn't
	// think it's somewhere it isn't.

	// If the boolean big is set then the checking is more thorough and checks along the tile borders
	// within the entity. This is only needed in games where you have entities which are larger than the
	// tiles or where you have sharp points in the map (ie, a meeting of two diagonal blocks without a flat
	// plateaux in the middle).

	int start_y,end_y;
	int start_by,end_by;
	int number_of_co_ords;
	int intermediate_block_count;
	int counter;

	int iterate = COLLISION_ITERATE_MOVEMENT & coll_type;
	int big = COLLISION_USE_EXTRA_TEST_POINTS & coll_type;
	int notice_when_inside_collision = COLLISION_NOTICE_WHEN_INSIDE_COLLISION & coll_type;
	int world_edge_hit = (COLLISION_VERTICAL_WORLD_EDGE_SOLID | COLLISION_HORIZONTAL_WORLD_EDGE_SOLID) & coll_type;

	start_y = entity_pointer[ENT_WORLD_Y] - entity_pointer[ENT_UPPER_WORLD_HEIGHT];
	end_y = entity_pointer[ENT_WORLD_Y] + entity_pointer[ENT_LOWER_WORLD_HEIGHT];
	
	collision_check_co_ords [0] = start_y;
	collision_check_co_ords [1] = end_y;

	if (x_vel<0)
	{
		collision_interaction_bitmasks[0] = INTERACTION_POINT_TOP_LEFT;
		collision_interaction_bitmasks[1] = INTERACTION_POINT_BOTTOM_LEFT;
	}
	else
	{
		collision_interaction_bitmasks[0] = INTERACTION_POINT_TOP_RIGHT;
		collision_interaction_bitmasks[1] = INTERACTION_POINT_BOTTOM_RIGHT;
	}

	if (big)
	{
		start_by = start_y >> block_size_bitshift;
		end_by = end_y >> block_size_bitshift;

		if ( start_by != end_by )
		{
			intermediate_block_count = end_by - start_by;

			if (x_vel<0)
			{
				for (counter=0; counter<intermediate_block_count; counter++)
				{
					collision_check_co_ords [(counter*2)+2] = ((start_by+counter) * block_size) + block_size_minus_one;
					collision_check_co_ords [(counter*2)+3] = ((start_by+counter) * block_size) + block_size;
					collision_interaction_bitmasks[(counter*2)+2] = INTERACTION_POINT_LEFT;
					collision_interaction_bitmasks[(counter*2)+3] = INTERACTION_POINT_LEFT;
				}
			}
			else
			{
				for (counter=0; counter<intermediate_block_count; counter++)
				{
					collision_check_co_ords [(counter*2)+2] = ((start_by+counter) * block_size) + block_size_minus_one;
					collision_check_co_ords [(counter*2)+3] = ((start_by+counter) * block_size) + block_size;
					collision_interaction_bitmasks[(counter*2)+2] = INTERACTION_POINT_RIGHT;
					collision_interaction_bitmasks[(counter*2)+3] = INTERACTION_POINT_RIGHT;
				}
			}

			number_of_co_ords = 2 + (intermediate_block_count * 2);
		}
		else
		{
			if (x_vel<0)
			{
				collision_interaction_bitmasks[2] = INTERACTION_POINT_LEFT;
			}
			else
			{
				collision_interaction_bitmasks[2] = INTERACTION_POINT_RIGHT;
			}
			collision_check_co_ords [2] = entity_pointer[ENT_WORLD_Y];
			number_of_co_ords = 3;
		}
	}
	else
	{
		if (x_vel<0)
		{
			collision_interaction_bitmasks[2] = INTERACTION_POINT_LEFT;
		}
		else
		{
			collision_interaction_bitmasks[2] = INTERACTION_POINT_RIGHT;
		}
		collision_check_co_ords [2] = entity_pointer[ENT_WORLD_Y];
		number_of_co_ords = 3;
	}

	int result = 0;
	int bitshift = entity_pointer[ENT_BITSHIFT];

	int test_x;

	int start_x;
	// This is to check if the eventual position is as it should be, if not then it'll
	// set the override flag for this axis.

	int actual_collision_depth;
	int entity_collision_layer = entity_pointer[ENT_WORLD_COLLISION_LAYER];
	int entity_collision_bitmask = entity_pointer[ENT_WORLD_COLLISION_BITMASK];

	start_x = entity_pointer[ENT_WORLD_X];

	bool flip_flop;

	if (x_vel<0)
	{
		start_x -= entity_pointer[ENT_UPPER_WORLD_WIDTH];

		for (counter=0; counter<number_of_co_ords; counter++)
		{
			collision_end_co_ords [counter] = WORLDCOLL_get_total_collision_depth_horizontal ( DIRECTION_RIGHT , collision_interaction_bitmasks[counter] , entity_collision_layer, start_x+x_vel , collision_check_co_ords[counter] , entity_collision_bitmask , world_edge_hit );

			if (iterate)
			{
				if (collision_end_co_ords[counter] == start_x+x_vel)
				{
					// If we arrived okay, then we still need to do an extra check in case we're simply moving too fast.

					if ( ((start_x+x_vel) >> block_size_bitshift) != (start_x >> block_size_bitshift) )
					{
						// This finds the first tile boundary to the right of the end position.
						test_x = (start_x+x_vel) & block_size_inverse;

						flip_flop = true;

						do
						{
							if (flip_flop)
							{
								test_x += block_size_minus_one;
							}
							else
							{
								test_x++;
							}
							flip_flop = !flip_flop;

							collision_end_co_ords [counter] = WORLDCOLL_get_total_collision_depth_horizontal ( DIRECTION_RIGHT , collision_interaction_bitmasks[counter] , entity_collision_layer, test_x , collision_check_co_ords[counter] , entity_collision_bitmask , world_edge_hit );
							// And keeps on doing this bit until the test position is to the right of the start position or we register a hit.
						} while ((collision_end_co_ords [counter] == test_x ) && (test_x+block_size < start_x));

						if (collision_end_co_ords [counter] == test_x )
						{
							// The loop exited because we were testing a point to the right of our start.
							// Which means we didn't hit anything along the way so we can safely reset it.
							collision_end_co_ords [counter] = start_x+x_vel;
						}
					}
				}

/*
				if (collision_end_co_ords[counter] == start_x+x_vel)
				{
					// Okay, we still didn't find a collision, so we need to do yet another check. Oy vey...

					if ( ((start_x+x_vel) >> block_size_bitshift) != (start_x >> block_size_bitshift) )
					{
						// This finds the first tile boundary to the right of the end position.
						test_x = ((start_x+x_vel) | block_size_minus_one) - block_size;

						do
						{
							test_x += block_size;
							collision_end_co_ords [counter] = WORLDCOLL_get_total_collision_depth_horizontal ( DIRECTION_RIGHT , collision_interaction_bitmasks[counter] , entity_collision_layer, test_x , collision_check_co_ords[counter] , entity_collision_bitmask , world_edge_hit );
							// And keeps on doing this bit until the test position is to the right of the start position or we register a hit.
						} while ((collision_end_co_ords [counter] == test_x ) && (test_x+block_size < start_x));

						if (collision_end_co_ords [counter] == test_x )
						{
							// The loop exited because we were testing a point to the right of our start.
							// Which means we didn't hit anything along the way so we can safely reset it.
							collision_end_co_ords [counter] = start_x+x_vel;
						}
					}
				}
*/

			}

			if (collision_end_co_ords[counter] > start_x)
			{
				// Collision did not occur because we ended up to the right of our start point or on our start point
				collision_collision_depth[counter] = x_vel;
				collision_result[counter] = RESULT_COLLISION_IGNORED;
			}
			else if ( (collision_end_co_ords[counter] > start_x+x_vel) && (collision_end_co_ords[counter] <= start_x) )
			{
				// Collision did occur! Monkey-bollocks!
				collision_collision_depth[counter] = collision_end_co_ords[counter] - start_x;
				collision_result[counter] = RESULT_COLLISION_OCCURRED;
				// In this case collision_depth is a negative number and represents the TRUE velocity which can
				// be safely added to x in order to bring it flush with the collision.
			}
			else
			{
				// Collision didn't occur because end_x = start_x+x_vel

				collision_collision_depth[counter] = x_vel;
				collision_result[counter] = RESULT_NO_COLLISION;
			}
		}
	}
	else // We know that x_vel != 0 or this whole function would never have been called...
	{
		start_x += entity_pointer[ENT_LOWER_WORLD_WIDTH];

		for (counter=0; counter<number_of_co_ords; counter++)
		{
			collision_end_co_ords [counter] = WORLDCOLL_get_total_collision_depth_horizontal ( DIRECTION_LEFT , collision_interaction_bitmasks[counter] , entity_collision_layer, start_x+x_vel , collision_check_co_ords[counter] , entity_collision_bitmask , world_edge_hit );
		
			if (iterate)
			{
				if (collision_end_co_ords[counter] == start_x+x_vel)
				{
					// If we arrived okay, then we still need to do an extra check in case we're simply moving too fast.

					if ( ((start_x+x_vel) >> block_size_bitshift) != (start_x >> block_size_bitshift) )
					{
						// This finds the first tile boundary to the left of the end position.
						test_x = (start_x+x_vel) | block_size_minus_one;

						flip_flop = true;

						do
						{
							if (flip_flop)
							{
								test_x -= block_size_minus_one;
							}
							else
							{
								test_x--;
							}
							flip_flop = !flip_flop;

							collision_end_co_ords [counter] = WORLDCOLL_get_total_collision_depth_horizontal ( DIRECTION_LEFT , collision_interaction_bitmasks[counter] , entity_collision_layer, test_x , collision_check_co_ords[counter] , entity_collision_bitmask , world_edge_hit );
							// And keeps on doing this bit until the test position is to the left of the start position or we register a hit.
						} while ((collision_end_co_ords [counter] == test_x ) && (test_x-block_size > start_x));

						if (collision_end_co_ords [counter] == test_x )
						{
							// The loop exited because we were testing a point to the left of our start.
							// Which means we didn't hit anything along the way so we can safely reset it.
							collision_end_co_ords [counter] = start_x+x_vel;
						}
					}
				}
			}

			if (collision_end_co_ords[counter] < start_x)
			{
				// Collision did not occur because we ended up to the right of our start point or on our start point
				collision_collision_depth[counter] = x_vel;
				collision_result[counter] = RESULT_COLLISION_IGNORED;
			}
			else if ( (collision_end_co_ords[counter] < start_x+x_vel) && (collision_end_co_ords[counter] >= start_x) )
			{
				// Collision did occur! Monkey-bollocks!
				collision_collision_depth[counter] = collision_end_co_ords[counter] - start_x;
				collision_result[counter] = RESULT_COLLISION_OCCURRED;
				// In this case collision_depth is a negative number and represents the TRUE velocity which can
				// be safely added to x in order to bring it flush with the collision.
			}
			else
			{
				// Collision didn't occur because end_x = start_x+x_vel
				collision_collision_depth[counter] = x_vel;
				collision_result[counter] = RESULT_NO_COLLISION;
			}
		}
	}

	// Now we determine the final upshot by going through the results and changing the total state based
	// upon the individual state.

	int overall_result = RESULT_NO_COLLISION;

	for (counter=0; counter<number_of_co_ords; counter++)
	{
		if (collision_result[counter] == RESULT_COLLISION_OCCURRED)
		{
			if (overall_result == RESULT_NO_COLLISION)
			{
				overall_result = RESULT_COLLISION_OCCURRED;
			}
		}
		else if (collision_result[counter] == RESULT_COLLISION_IGNORED) // Need to add bit about majority rule or not!
		{
			if (notice_when_inside_collision)
			{
				overall_result = RESULT_COLLISION_OCCURRED;
			}
			else
			{
				overall_result = RESULT_COLLISION_IGNORED;
			}
		}
	}

	// Now we deal with the various overall results...

	int closest_result_index = UNSET;

	if (overall_result == RESULT_COLLISION_OCCURRED)
	{
		actual_collision_depth = x_vel;

		if (x_vel<0)
		{
			if (notice_when_inside_collision)
			{
				for (counter = 0; counter<number_of_co_ords; counter++)
				{
					if ( (collision_collision_depth[counter] > actual_collision_depth) && (collision_collision_depth[counter] <= 0) )
					{
						actual_collision_depth = collision_collision_depth[counter];
						closest_result_index = counter;
					}
				}
			}
			else
			{
				// So we use the highest of the collision depths (because the numbers are negative and so this is the closest one to the origin)

				for (counter = 0; counter<number_of_co_ords; counter++)
				{
					if (collision_collision_depth[counter] > actual_collision_depth)
					{
						actual_collision_depth = collision_collision_depth[counter];
						closest_result_index = counter;
					}
				}
			}
		}
		else
		{
			if (notice_when_inside_collision)
			{
				// So we use the lowest of the collision depths (because the numbers are positive and so this is the closest one to the origin), however as we're possibly inside collision, we don't use ones less than 0.

				for (counter = 0; counter<number_of_co_ords; counter++)
				{
					if ( (collision_collision_depth[counter] < actual_collision_depth) && (collision_collision_depth[counter] >= 0) )
					{
						actual_collision_depth = collision_collision_depth[counter];
						closest_result_index = counter;
					}
				}
			}
			else
			{
				// So we use the lowest of the collision depths (because the numbers are positive and so this is the closest one to the origin)

				for (counter = 0; counter<number_of_co_ords; counter++)
				{
					if (collision_collision_depth[counter] < actual_collision_depth)
					{
						actual_collision_depth = collision_collision_depth[counter];
						closest_result_index = counter;
					}
				}
			}
		}

		if (closest_result_index == 0)
		{
			*which_corner = FIRST_CORNER;
		}
		else if (closest_result_index == 1)
		{
			*which_corner = SECOND_CORNER;
		}
		else
		{
			*which_corner = NEITHER_CORNER;
		}

		*not_collided = false;

	}
	else
	{
		actual_collision_depth = x_vel;
		*which_corner = NEITHER_CORNER;
		*not_collided = true;
	}

	return actual_collision_depth;
}



int WORLDCOLL_push_entity_vertical (int *entity_pointer, int y_vel, int *which_corner, int coll_type, bool *not_collided)
{
	// This function will attempt to move the specified entity the specified distance.
	// If it is unable to do so then it overrides the entities variables so that it doesn't
	// think it's somewhere it isn't.

	// If the boolean big is set then the checking is more thorough and checks along the tile borders
	// within the entity. This is only needed in games where you have entities which are larger than the
	// tiles or where you have sharp points in the map (ie, a meeting of two diagonal blocks without a flat
	// plateaux in the middle).

	int start_x,end_x;
	int start_bx,end_bx;
	int number_of_co_ords;
	int intermediate_block_count;
	int counter;

	start_x = entity_pointer[ENT_WORLD_X] - entity_pointer[ENT_UPPER_WORLD_WIDTH];
	end_x = entity_pointer[ENT_WORLD_X] + entity_pointer[ENT_LOWER_WORLD_WIDTH];

	int iterate = COLLISION_ITERATE_MOVEMENT & coll_type;
	int big = COLLISION_USE_EXTRA_TEST_POINTS & coll_type;
	int notice_when_inside_collision = COLLISION_NOTICE_WHEN_INSIDE_COLLISION & coll_type;
	int world_edge_hit = (COLLISION_VERTICAL_WORLD_EDGE_SOLID | COLLISION_HORIZONTAL_WORLD_EDGE_SOLID) & coll_type;

	collision_check_co_ords [0] = start_x;
	collision_check_co_ords [1] = end_x;
	
	if (y_vel<0)
	{
		collision_interaction_bitmasks[0] = INTERACTION_POINT_TOP_LEFT;
		collision_interaction_bitmasks[1] = INTERACTION_POINT_TOP_RIGHT;
	}
	else
	{
		collision_interaction_bitmasks[0] = INTERACTION_POINT_BOTTOM_LEFT;
		collision_interaction_bitmasks[1] = INTERACTION_POINT_BOTTOM_RIGHT;
	}
	
	if (big)
	{
		start_bx = start_x >> block_size_bitshift;
		end_bx = end_x >> block_size_bitshift;

		if ( start_bx != end_bx )
		{
			intermediate_block_count = end_bx - start_bx;

			if (y_vel<0)
			{
				for (counter=0; counter<intermediate_block_count; counter++)
				{
					collision_check_co_ords [(counter*2)+2] = ((start_bx+counter) * block_size) + block_size_minus_one;
					collision_check_co_ords [(counter*2)+3] = ((start_bx+counter) * block_size) + block_size;
					collision_interaction_bitmasks[(counter*2)+2] = INTERACTION_POINT_TOP;
					collision_interaction_bitmasks[(counter*2)+3] = INTERACTION_POINT_TOP;
				}
			}
			else
			{
				for (counter=0; counter<intermediate_block_count; counter++)
				{
					collision_check_co_ords [(counter*2)+2] = ((start_bx+counter) * block_size) + block_size_minus_one;
					collision_check_co_ords [(counter*2)+3] = ((start_bx+counter) * block_size) + block_size;
					collision_interaction_bitmasks[(counter*2)+2] = INTERACTION_POINT_BOTTOM;
					collision_interaction_bitmasks[(counter*2)+3] = INTERACTION_POINT_BOTTOM;
				}
			}

			number_of_co_ords = 2 + (intermediate_block_count * 2);
		}
		else
		{
			if (y_vel<0)
			{
				collision_interaction_bitmasks[2] = INTERACTION_POINT_TOP;
			}
			else
			{
				collision_interaction_bitmasks[2] = INTERACTION_POINT_BOTTOM;
			}
			collision_check_co_ords [2] = entity_pointer[ENT_WORLD_X];
			number_of_co_ords = 3;
		}
	}
	else
	{
		if (y_vel<0)
		{
			collision_interaction_bitmasks[2] = INTERACTION_POINT_TOP;
		}
		else
		{
			collision_interaction_bitmasks[2] = INTERACTION_POINT_BOTTOM;
		}
		collision_check_co_ords [2] = entity_pointer[ENT_WORLD_X];
		number_of_co_ords = 3;
	}

	int result = 0;
	int bitshift = entity_pointer[ENT_BITSHIFT];
	
	int test_y;

	int start_y;
	// This is to check if the eventual position is as it should be, if not then it'll
	// set the override flag for this axis.

	int actual_collision_depth;
	int entity_collision_layer = entity_pointer[ENT_WORLD_COLLISION_LAYER];
	int entity_collision_bitmask = entity_pointer[ENT_WORLD_COLLISION_BITMASK];

	start_y = entity_pointer[ENT_WORLD_Y];

	bool flip_flop;

	if (y_vel<0)
	{
		start_y -= entity_pointer[ENT_UPPER_WORLD_HEIGHT];

		for (counter=0; counter<number_of_co_ords; counter++)
		{
			collision_end_co_ords [counter] = WORLDCOLL_get_total_collision_depth_vertical ( DIRECTION_DOWN , collision_interaction_bitmasks[counter] , entity_collision_layer , collision_check_co_ords[counter] , start_y+y_vel , entity_collision_bitmask , world_edge_hit );

			if (iterate)
			{
				if (collision_end_co_ords[counter] == start_y+y_vel)
				{
					// If we arrived okay, then we still need to do an extra check in case we're simply moving too fast.

					if ( ((start_y+y_vel) >> block_size_bitshift) != (start_y >> block_size_bitshift) )
					{
						// This finds the first tile boundary above of the end position.
						test_y = (start_y+y_vel) & block_size_inverse;

						flip_flop = true;

						do
						{
							if (flip_flop)
							{
								test_y += block_size_minus_one;
							}
							else
							{
								test_y++;
							}
							flip_flop = !flip_flop;

							collision_end_co_ords [counter] = WORLDCOLL_get_total_collision_depth_vertical ( DIRECTION_DOWN , collision_interaction_bitmasks[counter] , entity_collision_layer, collision_check_co_ords[counter] , test_y , entity_collision_bitmask , world_edge_hit );
							// And keeps on doing this bit until the test position is above of the start position or we register a hit.
						} while ((collision_end_co_ords [counter] == test_y ) && (test_y+block_size < start_y));

						if (collision_end_co_ords [counter] == test_y )
						{
							// The loop exited because we were testing a point below of our start.
							// Which means we didn't hit anything along the way so we can safely reset it.
							collision_end_co_ords [counter] = start_y+y_vel;
						}
					}
				}
			}

			if (collision_end_co_ords[counter] > start_y)
			{
				// Collision did not occur because we ended up to the right of our start point or on our start point
				collision_collision_depth[counter] = y_vel;
				collision_result[counter] = RESULT_COLLISION_IGNORED;
			}
			else if ( (collision_end_co_ords[counter] > start_y+y_vel) && (collision_end_co_ords[counter] <= start_y) )
			{
				// Collision did occur! Monkey-bollocks!
				collision_collision_depth[counter] = collision_end_co_ords[counter] - start_y;
				collision_result[counter] = RESULT_COLLISION_OCCURRED;
				// In this case collision_depth is a negative number and represents the TRUE velocity which can
				// be safely added to x in order to bring it flush with the collision.
			}
			else
			{
				// Collision didn't occur because end_x = start_x+x_vel
				collision_collision_depth[counter] = y_vel;
				collision_result[counter] = RESULT_NO_COLLISION;
			}
		}
	}
	else // We know that x_vel != 0 or this whole function would never have been called...
	{
		start_y += entity_pointer[ENT_LOWER_WORLD_HEIGHT];

		for (counter=0; counter<number_of_co_ords; counter++)
		{
			collision_end_co_ords [counter] = WORLDCOLL_get_total_collision_depth_vertical ( DIRECTION_UP , collision_interaction_bitmasks[counter] , entity_collision_layer,collision_check_co_ords[counter] , start_y+y_vel ,  entity_collision_bitmask , world_edge_hit );
		
			if (iterate)
			{
				if (collision_end_co_ords[counter] == start_y+y_vel)
				{
					// If we arrived okay, then we still need to do an extra check in case we're simply moving too fast.

					if ( ((start_y+y_vel) >> block_size_bitshift) != (start_y >> block_size_bitshift) )
					{
						// This finds the first tile boundary above the of the end position.
						test_y = (start_y+y_vel) | block_size_minus_one;

						flip_flop = true;

						do
						{
							if (flip_flop)
							{
								test_y -= block_size_minus_one;
							}
							else
							{
								test_y--;
							}
							flip_flop = !flip_flop;

							collision_end_co_ords [counter] = WORLDCOLL_get_total_collision_depth_vertical ( DIRECTION_UP , collision_interaction_bitmasks[counter] , entity_collision_layer, collision_check_co_ords[counter] , test_y , entity_collision_bitmask , world_edge_hit );
							// And keeps on doing this bit until the test position is above of the start position or we register a hit.
						} while ((collision_end_co_ords [counter] == test_y ) && (test_y-block_size > start_y));

						if (collision_end_co_ords [counter] == test_y )
						{
							// The loop exited because we were testing a point above of our start.
							// Which means we didn't hit anything along the way so we can safely reset it.
							collision_end_co_ords [counter] = start_y+y_vel;
						}
					}
				}
			}

			if (collision_end_co_ords[counter] < start_y)
			{
				// Collision did not occur because we ended up to the right of our start point or on our start point
				collision_collision_depth[counter] = y_vel;
				collision_result[counter] = RESULT_COLLISION_IGNORED;
			}
			else if ( (collision_end_co_ords[counter] < start_y+y_vel) && (collision_end_co_ords[counter] >= start_y) )
			{
				// Collision did occur! Monkey-bollocks!
				collision_collision_depth[counter] = collision_end_co_ords[counter] - start_y;
				collision_result[counter] = RESULT_COLLISION_OCCURRED;
				// In this case collision_depth is a negative number and represents the TRUE velocity which can
				// be safely added to x in order to bring it flush with the collision.
			}
			else
			{
				// Collision didn't occur because end_x = start_x+x_vel
				collision_collision_depth[counter] = y_vel;
				collision_result[counter] = RESULT_NO_COLLISION;
			}
		}
	}

	// Now we determine the final upshot by going through the results and changing the total state based
	// upon the individual state.

	int overall_result = RESULT_NO_COLLISION;

	for (counter=0; counter<number_of_co_ords; counter++)
	{
		if (collision_result[counter] == RESULT_COLLISION_OCCURRED)
		{
			if (overall_result == RESULT_NO_COLLISION)
			{
				overall_result = RESULT_COLLISION_OCCURRED;
			}
		}
		else if (collision_result[counter] == RESULT_COLLISION_IGNORED) // Need to add bit about majority rule or not!
		{
			if (notice_when_inside_collision)
			{
				overall_result = RESULT_COLLISION_OCCURRED;
			}
			else
			{
				overall_result = RESULT_COLLISION_IGNORED;
			}
		}
	}

	// Now we deal with the various overall results...

	int closest_result_index = UNSET;

	if (overall_result == RESULT_COLLISION_OCCURRED)
	{
		actual_collision_depth = y_vel;

		if (y_vel<0)
		{
			if (notice_when_inside_collision)
			{
				for (counter = 0; counter<number_of_co_ords; counter++)
				{
					if ( (collision_collision_depth[counter] > actual_collision_depth) && (collision_collision_depth[counter] <= 0) )
					{
						actual_collision_depth = collision_collision_depth[counter];
						closest_result_index = counter;
					}
				}
			}
			else
			{
				// So we use the highest of the collision depths (because the numbers are negative and so this is the closest one to the origin)

				for (counter = 0; counter<number_of_co_ords; counter++)
				{
					if (collision_collision_depth[counter] > actual_collision_depth)
					{
						actual_collision_depth = collision_collision_depth[counter];
						closest_result_index = counter;
					}
				}
			}
		}
		else
		{
			if (notice_when_inside_collision)
			{
				// So we use the lowest of the collision depths (because the numbers are positive and so this is the closest one to the origin), however as we're possibly inside collision, we don't use ones less than 0.

				for (counter = 0; counter<number_of_co_ords; counter++)
				{
					if ( (collision_collision_depth[counter] < actual_collision_depth) && (collision_collision_depth[counter] >= 0) )
					{
						actual_collision_depth = collision_collision_depth[counter];
						closest_result_index = counter;
					}
				}
			}
			else
			{
				// So we use the lowest of the collision depths (because the numbers are positive and so this is the closest one to the origin)

				for (counter = 0; counter<number_of_co_ords; counter++)
				{
					if (collision_collision_depth[counter] < actual_collision_depth)
					{
						actual_collision_depth = collision_collision_depth[counter];
						closest_result_index = counter;
					}
				}
			}
		}

		if (closest_result_index == 0)
		{
			*which_corner = FIRST_CORNER;
		}
		else if (closest_result_index == 1)
		{
			*which_corner = SECOND_CORNER;
		}
		else
		{
			*which_corner = NEITHER_CORNER;
		}

		*not_collided = false;
	}
	else
	{
		actual_collision_depth = y_vel;
		*which_corner = NEITHER_CORNER;
		*not_collided = true;
	}

	return actual_collision_depth;
}


int WORLDCOLL_push_entity_against_sliding_collision (int *entity_pointer, int direction, int corner, int x_vel, int y_vel)
{
	int x_dev,y_dev;
	int x,y;
	int old_x,old_y;
	int start_x,start_y;

	int x_adder,y_adder;
	int distance;

	int x_evade_adder,y_evade_adder;
	int evade_bitvalue;

	int counter;
	int remainder;

	int first_check_x_offset,first_check_y_offset;
	int second_check_x_offset,second_check_y_offset;
	// These are the offsets from the offset checking point to the two other corners which are checked

	// 1) Choose corner point we'll be using as our offset map checking point.
	// 2) Set the x/y adding values for the given velocity and direction.
	// 3) Start a loop whereby we move, add the offset to the variables and then check
	//    the other two corners for trangressions. If there are any restore previous
	//    frames values and finish.
	// 4) calculate x_dev and y_dev and apply where applicable.

	int total_width = entity_pointer[ENT_UPPER_WORLD_WIDTH] + entity_pointer[ENT_LOWER_WORLD_WIDTH];
	int total_height = entity_pointer[ENT_UPPER_WORLD_HEIGHT] + entity_pointer[ENT_LOWER_WORLD_HEIGHT];

	switch (direction)
	{
	case DIRECTION_DOWN:
		if (corner == FIRST_CORNER)
		{
			// This means it's the top-left corner for primary.
			x = entity_pointer[ENT_WORLD_X] - entity_pointer[ENT_UPPER_WORLD_WIDTH];
			y = entity_pointer[ENT_WORLD_Y] - entity_pointer[ENT_UPPER_WORLD_HEIGHT];

			// And the top-right and bottom-right for the secondaries...

			first_check_x_offset = total_width;
			first_check_y_offset = 0;

			second_check_x_offset = total_width;
			second_check_y_offset = total_height;

			evade_bitvalue = DIRECTION_BITVALUE_RIGHT;
			x_evade_adder = 1;
			y_evade_adder = 0;
		}
		else
		{
			// It's the top-right corner for primary, yadda-yadda-yadda...
			x = entity_pointer[ENT_WORLD_X] + entity_pointer[ENT_LOWER_WORLD_WIDTH];
			y = entity_pointer[ENT_WORLD_Y] - entity_pointer[ENT_UPPER_WORLD_HEIGHT];

			// And the top-left and bottom-left for the secondaries...

			first_check_x_offset = -total_width;
			first_check_y_offset = 0;

			second_check_x_offset = -total_width;
			second_check_y_offset = total_height;

			evade_bitvalue = DIRECTION_BITVALUE_LEFT;
			x_evade_adder = -1;
			y_evade_adder = 0;
		}

		distance = -y_vel;
		y_adder = -1;
		x_adder = 0;
		break;

	case DIRECTION_LEFT:
		if (corner == FIRST_CORNER)
		{
			// Top-right
			x = entity_pointer[ENT_WORLD_X] + entity_pointer[ENT_LOWER_WORLD_WIDTH];
			y = entity_pointer[ENT_WORLD_Y] - entity_pointer[ENT_UPPER_WORLD_HEIGHT];

			first_check_x_offset = 0;
			first_check_y_offset = total_height;

			second_check_x_offset = -total_width;
			second_check_y_offset = total_height;

			evade_bitvalue = DIRECTION_BITVALUE_DOWN;
			x_evade_adder = 0;
			y_evade_adder = 1;
		}
		else
		{
			// Bottom-right
			x = entity_pointer[ENT_WORLD_X] + entity_pointer[ENT_LOWER_WORLD_WIDTH];
			y = entity_pointer[ENT_WORLD_Y] + entity_pointer[ENT_LOWER_WORLD_HEIGHT];

			first_check_x_offset = 0;
			first_check_y_offset = -total_height;

			second_check_x_offset = -total_width;
			second_check_y_offset = -total_height;

			evade_bitvalue = DIRECTION_BITVALUE_UP;
			x_evade_adder = 0;
			y_evade_adder = -1;
		}

		distance = x_vel;
		y_adder = 0;
		x_adder = 1;
		break;

	case DIRECTION_UP:
		if (corner == FIRST_CORNER)
		{
			// Bottom-left
			x = entity_pointer[ENT_WORLD_X] - entity_pointer[ENT_UPPER_WORLD_WIDTH];
			y = entity_pointer[ENT_WORLD_Y] + entity_pointer[ENT_LOWER_WORLD_HEIGHT];

			first_check_x_offset = total_width;
			first_check_y_offset = 0;

			second_check_x_offset = total_width;
			second_check_y_offset = -total_height;

			evade_bitvalue = DIRECTION_BITVALUE_RIGHT;
			x_evade_adder = 1;
			y_evade_adder = 0;
		}
		else
		{
			// Bottom-right
			x = entity_pointer[ENT_WORLD_X] + entity_pointer[ENT_LOWER_WORLD_WIDTH];
			y = entity_pointer[ENT_WORLD_Y] + entity_pointer[ENT_LOWER_WORLD_HEIGHT];

			first_check_x_offset = -total_width;
			first_check_y_offset = 0;

			second_check_x_offset = -total_width;
			second_check_y_offset = -total_height;

			evade_bitvalue = DIRECTION_BITVALUE_LEFT;
			x_evade_adder = -1;
			y_evade_adder = 0;
		}

		distance = y_vel;
		y_adder = 1;
		x_adder = 0;
		break;

	case DIRECTION_RIGHT:
		if (corner == FIRST_CORNER)
		{
			// Top-left
			x = entity_pointer[ENT_WORLD_X] - entity_pointer[ENT_UPPER_WORLD_WIDTH];
			y = entity_pointer[ENT_WORLD_Y] - entity_pointer[ENT_UPPER_WORLD_HEIGHT];

			first_check_x_offset = 0;
			first_check_y_offset = total_height;

			second_check_x_offset = total_width;
			second_check_y_offset = total_height;

			evade_bitvalue = DIRECTION_BITVALUE_DOWN;
			x_evade_adder = 0;
			y_evade_adder = 1;
		}
		else
		{
			// Bottom-left
			x = entity_pointer[ENT_WORLD_X] - entity_pointer[ENT_UPPER_WORLD_WIDTH];
			y = entity_pointer[ENT_WORLD_Y] + entity_pointer[ENT_LOWER_WORLD_HEIGHT];

			first_check_x_offset = 0;
			first_check_y_offset = -total_height;

			second_check_x_offset = total_width;
			second_check_y_offset = -total_height;

			evade_bitvalue = DIRECTION_BITVALUE_UP;
			x_evade_adder = 0;
			y_evade_adder = -1;
		}

		distance = -x_vel;
		y_adder = 0;
		x_adder = -1;
		break;
	}

	start_x = x;
	start_y = y; // Used to calculate x_dev and y_dev.

	int collision_layer = entity_pointer[ENT_WORLD_COLLISION_LAYER];
	int result;

	for (counter = 0; counter<distance; counter++)
	{
		old_x = x;
		old_y = y;

		x += x_adder;
		y += y_adder;

		result = WORLDCOLL_collision_offset (collision_layer , x, y, entity_pointer[ENT_WORLD_COLLISION_BITMASK], entity_pointer[ENT_WORLD_COLLISION_BEHAVIOUR]);

		if (result == EXPOSURE_MAP_CARRY_ON)
		{
			// Do nothing! NOTHING!!!
		}
		else if (result & evade_bitvalue)
		{
			// Get outta' the collision, man! It's a trap!
			x += x_evade_adder;
			y += y_evade_adder;
		}
		else
		{
			// Bumchuff! We've plunged headlong into collision from which there is no escape!
			x = old_x;
			y = old_y;
			// Oh, except we can go backwards. Hurrah!
		}

		// Now, even though we might have managed to go forward or evade this might have made
		// one of our other corners end up in the collision, so we'd best check them as well, eh?
		// Yeah, you know it's for the best...

		if (WORLDCOLL_collision_test (collision_layer,x+first_check_x_offset,y+first_check_y_offset , entity_pointer[ENT_WORLD_COLLISION_BITMASK]))
		{
			x = old_x;
			y = old_y;
		}
		else if (WORLDCOLL_collision_test (collision_layer,x+second_check_x_offset,y+second_check_y_offset , entity_pointer[ENT_WORLD_COLLISION_BITMASK]))
		{
			x = old_x;
			y = old_y;
		}

		if ((x == old_x) && (y == old_y))
		{
			// ie, if the other corner's have hit anything OR we've just not got anywhere.
			counter = distance;
		}
	}


	switch (direction)
	{
	case DIRECTION_DOWN:
		remainder = y_vel - (y - start_y);
		break;

	case DIRECTION_LEFT:
		remainder = x_vel - (x - start_x);
		break;

	case DIRECTION_UP:
		remainder = y_vel - (y - start_y);
		break;

	case DIRECTION_RIGHT:
		remainder = x_vel - (x - start_x);
		break;
	}

	// So apply the deviation to the entity...

	entity_pointer[ENT_X] += ( (x - start_x) << entity_pointer[ENT_BITSHIFT] );
	entity_pointer[ENT_Y] += ( (y - start_y) << entity_pointer[ENT_BITSHIFT] );

	// And generate world-scale co-ordinates...

	entity_pointer[ENT_WORLD_X] = entity_pointer[ENT_X] >> entity_pointer[ENT_BITSHIFT];
	entity_pointer[ENT_WORLD_Y] = entity_pointer[ENT_Y] >> entity_pointer[ENT_BITSHIFT];

	int temp_velocity;

	if ( (direction == DIRECTION_RIGHT) || (direction == DIRECTION_LEFT) )
	{
		y_dev = y - start_y;

		if (entity_pointer[ENT_WORLD_COLLISION_BEHAVIOUR] & COLLISION_SPECIAL_USE_DEVIATION_TO_ADAPT_VELOCITY_VERTICAL)
		{
			// We need to apply the y_dev to alter the velocity.
			
			if (y_dev < 0)
			{
				// If current y_deviation is less than 0 then if the entity's y_velocity is greater than that
				// we need to decrease it towards y_dev.
				temp_velocity = (entity_pointer[ENT_Y_VEL] >> entity_pointer[ENT_BITSHIFT]);

				if ( temp_velocity > y_dev )
				{
					{
						entity_pointer[ENT_Y_VEL] += ( (y_dev << entity_pointer[ENT_BITSHIFT]) - entity_pointer[ENT_Y_VEL]) / 2;
					}
				}
			}
			else if (y_dev > 0)
			{
				// If current y_deviation is more than 0 then if the entity's y_velocity is less than that
				// we need to increase it towards y_dev.

				temp_velocity = (entity_pointer[ENT_Y_VEL] >> entity_pointer[ENT_BITSHIFT]);

				if ( temp_velocity < y_dev )
				{
					{
						entity_pointer[ENT_Y_VEL] += ( (y_dev << entity_pointer[ENT_BITSHIFT]) - entity_pointer[ENT_Y_VEL]) / 2;
					}
				}
			}

		}
	}
	else
	{
		x_dev = x - start_x;

		if (entity_pointer[ENT_WORLD_COLLISION_BEHAVIOUR] & COLLISION_SPECIAL_USE_DEVIATION_TO_ADAPT_VELOCITY_HORIZONTAL)
		{

			// We need to apply the x_dev to alter the velocity.
			
			if (x_dev < 0)
			{
				// If current x_deviation is less than 0 then if the entity's x_velocity is greater than that
				// we need to decrease it towards x_dev.
				temp_velocity = (entity_pointer[ENT_X_VEL] >> entity_pointer[ENT_BITSHIFT]);

				if ( temp_velocity > x_dev )
				{
					entity_pointer[ENT_X_VEL] += ( (x_dev << entity_pointer[ENT_BITSHIFT]) - entity_pointer[ENT_X_VEL]) / 2;
				}
			}
			else if (x_dev > 0)
			{
				// If current x_deviation is more than 0 then if the entity's x_velocity is less than that
				// we need to increase it towards x_dev.

				temp_velocity = (entity_pointer[ENT_X_VEL] >> entity_pointer[ENT_BITSHIFT]);

				if ( temp_velocity < x_dev )
				{
					entity_pointer[ENT_X_VEL] += ( (x_dev << entity_pointer[ENT_BITSHIFT]) - entity_pointer[ENT_X_VEL]) / 2;
				}
			}


		}
	}

	return remainder;	

}



// Other collision system will need to return a progress percentage (ie, how far long the movement
// line the collision occurred) and a face normal so the bounce can be calculated. That way for squares
// we check the three leading points and the lowest intersection percentage wins.



int WORLDCOLL_get_movement_bitmask (int x_vel, int y_vel)
{
	// This returns the sides of the blocks we should be looking out for if we're moving in a direction...

	int bitmask = 0;

	if (x_vel > 0)
	{
		bitmask += DIRECTION_BITVALUE_LEFT;
	}
	else if (x_vel < 0)
	{
		bitmask += DIRECTION_BITVALUE_RIGHT;
	}

	if (y_vel > 0)
	{
		bitmask += DIRECTION_BITVALUE_UP;
	}
	else if (y_vel < 0)
	{
		bitmask += DIRECTION_BITVALUE_DOWN;
	}

	return bitmask;
}



bool WORLDCOLL_push_point_simultaneous (int *entity_pointer, int start_x, int start_y, int x_vel, int y_vel, int *surface_normal, float *progress, int only_check_for_bitmask)
{
	// This compares the point's start and end points and investigates further based on a number
	// of outcomes.

	// Empty To Solid - When the start location is outside of ANY collision (even ignorable stuff) and the end
	// point is within it then we need to use a binary search to locate the point of intersection (easily done).

	// Solid To Different Solid - If the exposure bitmasks for the start and end blocks are different then
	// we need to first see if that's important. If the extra bitmasks for the block we're travelling to is solid in the
	// direction we're coming from then we need to first see if we're crossing a tile boundary in that direction and
	// if we are calculate the intersection point. However if we aren't crossing a boundary in that direction we're
	// home free. If the exposure map isn't solid in any direction we're coming from then there's nothing to worry
	// about.
	// Possibly later on I may include a mid-point check for fast-moving objects which makes sure that the solidity
	// is on and the exposure is equal to either the start or end point.

	// Solid To Same Solid - Do nothing.
	// Possibly later on I may include a mid-point check for fast-moving objects which makes sure that the solidity
	// is on and the exposure is equal to either the start or end point.

	// Solid To Empty - Do nothing. Woot!

	// Empty To Empty - Fine by me! Will cut corners, but I really don't care.

	// A proviso to all of this is that we will IGNORE collisions except those specified in the only_check_for_bitmask
	// integer. This is so that with squares when we're checking on of the points which flank the leading corner
	// point we don't accidentally have them hitting their ass on the way into collision. ie.

	// Direction = down-right.

	//                 checks nothing > *-----* < checks only horizontal collision
	//                                  |     |
	//                                  |     |
	//                                  |     |
	// checks only vertical collision > *-----* < checks both vertical and horizontal collision

	int start_solidity,end_solidity;
	int start_exposure,end_exposure;

	int end_x,end_y;

	int layer = entity_pointer[ENT_WORLD_COLLISION_LAYER];
	int collision_bitmask = entity_pointer[ENT_WORLD_COLLISION_BITMASK];

	int movement_bitmask;

	float binary_search_precision;
	float binary_search_point;
	float binary_search_threshold;
	float binary_search_last_solid;
	float binary_search_last_empty;

	int test_x,test_y;
	int intersection_x,intersection_y;
	int result;
	int test_exposure;

	end_x = start_x + x_vel;
	end_y = start_y + y_vel;

	start_solidity = WORLDCOLL_collision_test (layer , start_x , start_y, collision_bitmask);
	end_solidity = WORLDCOLL_collision_test (layer , end_x , end_y, collision_bitmask);

	if (end_solidity == 0)
	{
		// Woo!
		return false;
	}
	else if ( (start_solidity == 0) && (end_solidity == 1) )
	{
		// Righty, we've gone from being in free space to inside collision. Now that doesn't
		// necessarily mean we've hit anything because there's always the chance that we're inside
		// collision which isn't solid in the direction we've travelled...

		movement_bitmask = WORLDCOLL_get_movement_bitmask (x_vel,y_vel);

		end_exposure = WORLDCOLL_tile_exposure_value (layer, end_x , end_y);

		if ((movement_bitmask & end_exposure & only_check_for_bitmask) == 0)
		{
			return false;
			// Wee! We get away again!
		}
		else
		{
			// This means that the tile we're going to is solid in the direction we're going. So we have
			// to do a binary search.

			binary_search_precision = 0.25f;
			binary_search_point = 0.5f;

			binary_search_threshold = 1.0f / float (x_vel + y_vel);
			// Search only quits out when we're moving less than a pixel distance per iteration.

			binary_search_last_solid = 1.0f;
			binary_search_last_empty = 0.0f;
			// We know we're solid at that point...

			do
			{
				test_x = int(MATH_lerp(float(start_x),float(start_y),float(binary_search_point)));
				test_y = int(MATH_lerp(float(start_y),float(start_y),float(binary_search_point)));

				result = WORLDCOLL_collision_test (layer , test_x , test_y, collision_bitmask);

				if (result)
				{
					// It's solid, so remember the position - it might be the last solid we
					// encounter...
					binary_search_last_solid = binary_search_point;

					// And remember the intersection point so we can pilfer the angle from it.
					intersection_x = test_x;
					intersection_y = test_y;

					// ie, the point found was solid, so we need to move backwards...
					binary_search_point -= binary_search_precision;
				}
				else
				{
					// It's empty, so remember the position. It might be the last empty
					// we encounter.
					binary_search_last_empty = binary_search_point;

					// ie, the point found was empty, so we need to move forward...
					binary_search_point += binary_search_precision;
				}

				binary_search_precision /= 2.0f;

			}
			while (binary_search_precision > binary_search_threshold);

			// We should also know after this the position along the line we can
			// sample to get the reflection angle and the position along the line
			// we need to dump the entity at to position it outside of the collision.

			*surface_normal = WORLDCOLL_collision_angle (layer,intersection_x,intersection_y);
			*progress = binary_search_last_empty;

			return true;
		}
	}
	else
	{
		// Both start and end solidity are solid. But what about the bitmasks, eh? EH?!

		start_exposure = WORLDCOLL_tile_exposure_value (layer, start_x , start_y);
		end_exposure = WORLDCOLL_tile_exposure_value (layer, end_x , end_y);

		movement_bitmask = WORLDCOLL_get_movement_bitmask (x_vel,y_vel);

		// When we compare them we chop off any directions we don't give a flying shit about.
		if ((start_exposure & only_check_for_bitmask) == (end_exposure & only_check_for_bitmask))
		{
			// And goodnight, ladies and gentlemen! I'm outta' here!
			// Because we just don't care about these kinda' things because chances are it's a-okay.
			return false;
		}
		else if ((movement_bitmask & end_exposure & only_check_for_bitmask) == 0)
		{
			return false;
			// Wee! We get away again! That's because the collision we're crossing into isn't solid
			// in our direction, or it is but we're not checking that axis. TAKE THAT YOU COMMUNISTS!
		}
		else
		{
			// Aw crap. Well, we need to find the point of intersection. We do this in pretty much the same
			// way as we did it before, only this time we're checking for a disparity between the new and old
			// exposure bitmasks.

			// This means that the tile we're going to is solid in the direction we're going. So we have
			// to do a binary search.

			binary_search_precision = 0.25f;
			binary_search_point = 0.5f;

			binary_search_threshold = 1.0f / float (x_vel + y_vel);
			// Search only quits out when we're moving less than a pixel distance per iteration.

			binary_search_last_solid = 1.0f;
			binary_search_last_empty = 0.0f;
			// We know we're solid at that point...

			do
			{
				test_x = int(MATH_lerp(float(start_x),float(start_y),float(binary_search_point)));
				test_y = int(MATH_lerp(float(start_y),float(start_y),float(binary_search_point)));

				test_exposure = WORLDCOLL_tile_exposure_value (layer , test_x , test_y);

				if (test_exposure == start_exposure)
				{
					// It's in the start zone, so remember the position. It might be the last time
					// we're in here...
					binary_search_last_empty = binary_search_point;

					binary_search_point += binary_search_precision;
				}
				else
				{
					// It's in the end zone, so remember the position - it might be the last time we
					// we're in here...
					binary_search_last_solid = binary_search_point;

					// And remember the intersection point so we can pilfer the angle from it.
					intersection_x = test_x;
					intersection_y = test_y;

					binary_search_point -= binary_search_precision;
				}

				binary_search_precision /= 2.0f;

			}
			while (binary_search_precision > binary_search_threshold);

			// We should also know after this the position along the line we can
			// sample to get the reflection angle and the position along the line
			// we need to dump the entity at to position it outside of the collision.

			*surface_normal = WORLDCOLL_collision_angle (layer,intersection_x,intersection_y);
			*progress = binary_search_last_empty;

			return true;
		}
	}

}



void WORLDCOLL_push_entity_simultaneous (int *entity_pointer, int *surface_normal, float *progress)
{
	float lowest_progress = 1.0f;
	int lowest_progress_normal;
	bool hit = false;

	float test_progress;
	int test_normal;

	int x_vel,y_vel;

	x_vel = ((entity_pointer[ENT_X] + entity_pointer[ENT_X_VEL] + entity_pointer[ENT_EXTERNAL_CONVEY_X]) >> entity_pointer[ENT_BITSHIFT]) - entity_pointer[ENT_WORLD_X];
	y_vel = ((entity_pointer[ENT_Y] + entity_pointer[ENT_Y_VEL] + entity_pointer[ENT_EXTERNAL_CONVEY_Y]) >> entity_pointer[ENT_BITSHIFT]) - entity_pointer[ENT_WORLD_Y];
	
	switch (entity_pointer[ENT_COLLISION_SHAPE])
	{
	case SHAPE_POINT:
		// Easiest of them as it's the same point regardless of direction and there's only one of 'em.
		if (WORLDCOLL_push_point_simultaneous (entity_pointer, entity_pointer[ENT_WORLD_X], entity_pointer[ENT_WORLD_Y], x_vel, y_vel, &test_normal, &test_progress, DIRECTION_BITVALUE_TOTAL) == true)
		{
			hit = true;

			// Strictly speaking this ain't necessary, but it'll be used in the other ones so I may as well...
			if (test_progress < lowest_progress)
			{
				lowest_progress = test_progress;
				lowest_progress_normal = test_normal;
			}
		}

		if (hit == true)
		{
			// Bounce it, baby!


		}
		break;

	case SHAPE_CIRCLE:

		break;

	case SHAPE_RECTANGLE:
		
		break;
	}

}



void WORLDCOLL_get_collision_map_size_in_pixels (int *width,int *height)
{
	*width = collision_data_width * collision_tilesize;
	*height = collision_data_height * collision_tilesize;
}



bool WORLDCOLL_inside_collision (int *entity_pointer)
{
	// Just looks around the edge of the box which defines the entity for solidity. Duh!

	int start_x;
	int end_x;
	int start_y;
	int end_y;

	int shortest_snap_tile = UNSET;

	int start_bx,end_bx;
	int start_by,end_by;

	int number_of_co_ords;
	int intermediate_block_count;
	int counter;

	start_x = entity_pointer[ENT_WORLD_X] - entity_pointer[ENT_UPPER_WORLD_WIDTH];
	end_x = entity_pointer[ENT_WORLD_X] + entity_pointer[ENT_LOWER_WORLD_WIDTH];

	start_y = entity_pointer[ENT_WORLD_Y] - entity_pointer[ENT_UPPER_WORLD_HEIGHT];
	end_y = entity_pointer[ENT_WORLD_Y] + entity_pointer[ENT_LOWER_WORLD_HEIGHT];

	collision_inside_check_x_co_ords[0] = start_x;
	collision_inside_check_y_co_ords[0] = start_y;

	collision_inside_check_x_co_ords[1] = start_x;
	collision_inside_check_y_co_ords[1] = end_y;

	collision_inside_check_x_co_ords[2] = end_x;
	collision_inside_check_y_co_ords[2] = end_y;

	collision_inside_check_x_co_ords[3] = end_x;
	collision_inside_check_y_co_ords[3] = start_y;
	
	if (entity_pointer[ENT_WORLD_COLLISION_BEHAVIOUR] & COLLISION_USE_EXTRA_TEST_POINTS)
	{
		start_by = start_y >> block_size_bitshift;
		end_by = end_y >> block_size_bitshift;
		start_bx = start_x >> block_size_bitshift;
		end_bx = end_x >> block_size_bitshift;

		if ( start_by != end_by )
		{
			intermediate_block_count = end_by - start_by;

			for (counter=0; counter<intermediate_block_count; counter++)
			{
				collision_inside_check_x_co_ords [(counter*4)+4] = ((start_bx+counter) * block_size) + block_size_minus_one;
				collision_inside_check_x_co_ords [(counter*4)+5] = ((start_bx+counter) * block_size) + block_size;
				collision_inside_check_x_co_ords [(counter*4)+6] = collision_inside_check_x_co_ords [(counter*4)+4];
				collision_inside_check_x_co_ords [(counter*4)+7] = collision_inside_check_x_co_ords [(counter*4)+5];

				collision_inside_check_y_co_ords [(counter*4)+4] = end_y;
				collision_inside_check_y_co_ords [(counter*4)+5] = end_y;
				collision_inside_check_y_co_ords [(counter*4)+6] = start_y;
				collision_inside_check_y_co_ords [(counter*4)+7] = start_y;
			}

			number_of_co_ords = 4 + (intermediate_block_count * 4);
		}
		else
		{
			number_of_co_ords = 4;
		}

		if ( start_bx != end_bx )
		{
			intermediate_block_count = end_bx - start_bx;

			for (counter=0; counter<intermediate_block_count; counter++)
			{
				collision_inside_check_x_co_ords [(counter*4)+number_of_co_ords] = end_x;
				collision_inside_check_x_co_ords [(counter*4)+number_of_co_ords+1] = end_x;
				collision_inside_check_x_co_ords [(counter*4)+number_of_co_ords+2] = start_x;
				collision_inside_check_x_co_ords [(counter*4)+number_of_co_ords+3] = start_x;

				collision_inside_check_y_co_ords [(counter*4)+number_of_co_ords] = ((start_by+counter) * block_size) + block_size_minus_one;
				collision_inside_check_y_co_ords [(counter*4)+number_of_co_ords+1] = ((start_by+counter) * block_size) + block_size;
				collision_inside_check_y_co_ords [(counter*4)+number_of_co_ords+2] = collision_inside_check_y_co_ords [(counter*4)+number_of_co_ords];
				collision_inside_check_y_co_ords [(counter*4)+number_of_co_ords+3] = collision_inside_check_y_co_ords [(counter*4)+number_of_co_ords+1];
			}

			number_of_co_ords = number_of_co_ords + (intermediate_block_count * 4);
		}
	}
	else
	{
		number_of_co_ords = 4;
	}

	for (counter=0; counter<number_of_co_ords; counter++)
	{
		if (WORLDCOLL_collision_test (entity_pointer[ENT_WORLD_COLLISION_LAYER],collision_inside_check_x_co_ords[counter],collision_inside_check_y_co_ords[counter],entity_pointer[ENT_WORLD_COLLISION_BITMASK]))
		{
			return true;
		}
	}
	
	return false;
}



void WORLDCOLL_push_entity (int *entity_pointer)
{
	int which_corner;
	int x_collision_depth,y_collision_depth;
	int coll_type = entity_pointer[ENT_WORLD_COLLISION_BEHAVIOUR];

	int remainder;

	bool not_collided;

	int backup_next_ent;

	int x_vel,y_vel;

	// For now we'll assume everyone is a standard sliding horizontal then vertical push without feedback.

	// velocity = ((private_scale_co_ord + private_scale_velocity) / scale) - private_scale_co_ord

	switch (entity_pointer[ENT_ENTITY_MOVEMENT_BEHAVIOUR])
	{
	case ENTITY_MOVEMENT_HORIZONTAL_VERTICAL:
		
		x_vel = ((entity_pointer[ENT_X] + entity_pointer[ENT_X_VEL] + entity_pointer[ENT_EXTERNAL_CONVEY_X]) >> entity_pointer[ENT_BITSHIFT]) - entity_pointer[ENT_WORLD_X];

		if (x_vel != 0)
		{
			x_collision_depth = WORLDCOLL_push_entity_horizontal (entity_pointer, x_vel, &which_corner, coll_type, &not_collided);
			remainder = x_vel - x_collision_depth;

			if (not_collided)
			{
				// Hurrah! Unimpeded progress! Woot!
				entity_pointer[ENT_X] += entity_pointer[ENT_X_VEL];
				entity_pointer[ENT_WORLD_X] = entity_pointer[ENT_X] >> entity_pointer[ENT_BITSHIFT];
			}
			else
			{
				entity_pointer[ENT_X] += (x_collision_depth << entity_pointer[ENT_BITSHIFT]);
				entity_pointer[ENT_WORLD_X] = entity_pointer[ENT_X] >> entity_pointer[ENT_BITSHIFT];

				// What we do now is entirely dependant upon whether we have sliding horizontal collision on...
				if ((coll_type & COLL_TYPE_SLIDING_HORIZONTAL) && (which_corner != NEITHER_CORNER))
				{
					// "remainder" is how much of our allotted x_velocity we didn't manage to move...

					if (remainder < 0)
					{
						remainder = WORLDCOLL_push_entity_against_sliding_collision (entity_pointer, DIRECTION_RIGHT, which_corner, remainder, 0);
					}
					else
					{
						remainder = WORLDCOLL_push_entity_against_sliding_collision (entity_pointer, DIRECTION_LEFT, which_corner, remainder, 0);
					}

					// Now "remainder" has had however far we got via slidey collision deducted from it as well.
				}
				else
				{
					// So that if we can't burn off the rest of the movement then we'll have some remainder for the next bit to notice.
					remainder = 1;
				}

				if (remainder != 0) // Ie, if we didn't manage to go the distance in the end...
				{
					if (entity_pointer[ENT_WORLD_COLLISION_BEHAVIOUR] & COLLISION_RESPONSE_SIMPLE_BOUNCE_OR_DECELLERATION)
					{
						// If we aren't sliding and we didn't use up all of our movement then we've obviously hit the side of summat.
						entity_pointer[ENT_X_VEL] *= entity_pointer[ENT_WORLD_COLLISION_COEF_HORIZONTAL];
						entity_pointer[ENT_X_VEL] /= 100;
					}

					if (entity_pointer[ENT_WORLD_COLLISION_BEHAVIOUR] & COLLISION_RESPONSE_CALL_SCRIPT)
					{
						if (entity_pointer[ENT_WORLD_HIT_LINE] != UNSET)
						{
							if (x_vel > 0)
							{
								entity_pointer[ENT_WORLD_HIT_SIDE] = DIRECTION_RIGHT;
							}
							else
							{
								entity_pointer[ENT_WORLD_HIT_SIDE] = DIRECTION_LEFT;
							}
							entity_pointer[ENT_WORLD_HIT_AXIS] = HORIZONTAL_AXIS;

							backup_next_ent = entity_pointer[ENT_NEXT_PROCESS_ENT];

							SCRIPTING_interpret_script (entity_pointer[ENT_OWN_ID] , entity_pointer[ENT_WORLD_HIT_LINE]);

							if (entity_pointer[ENT_ALIVE] != ALIVE)
							{
								global_next_entity = backup_next_ent;
								// Run!!!
								return;
							}
						}
					}
				}
			}
		}
		else
		{
			// It might not have moved over a pixel boundary but it could still need updating.
			entity_pointer[ENT_X] += (entity_pointer[ENT_X_VEL] + entity_pointer[ENT_EXTERNAL_CONVEY_X]);
			entity_pointer[ENT_WORLD_X] = entity_pointer[ENT_X] >> entity_pointer[ENT_BITSHIFT];
			x_vel = 0;
			x_collision_depth = 0;
		}

		y_vel = ((entity_pointer[ENT_Y] + entity_pointer[ENT_Y_VEL] + entity_pointer[ENT_EXTERNAL_CONVEY_Y]) >> entity_pointer[ENT_BITSHIFT]) - entity_pointer[ENT_WORLD_Y];
		// We calculate y_vel here as it's possible that ENT_Y_VEL has changed due to slidey collision.

		if (y_vel != 0)
		{
			// And then after that what we do depends if we have conditional movement on, unless we
			// didn't collide anyway.
			if ( (x_vel == x_collision_depth) || ( (coll_type & COLL_TYPE_SECOND_AXIS_CONDITIONAL) == 0) )
			{
				y_collision_depth = WORLDCOLL_push_entity_vertical (entity_pointer, y_vel, &which_corner, coll_type, &not_collided);

				remainder = y_vel - y_collision_depth;

				if (not_collided)
				{
					// Hurrah! Unimpeded progress! Woot!
					entity_pointer[ENT_Y] += entity_pointer[ENT_Y_VEL];
					entity_pointer[ENT_WORLD_Y] = entity_pointer[ENT_Y] >> entity_pointer[ENT_BITSHIFT];
				}
				else
				{
					entity_pointer[ENT_Y] += (y_collision_depth << entity_pointer[ENT_BITSHIFT]);
					entity_pointer[ENT_WORLD_Y] = entity_pointer[ENT_Y] >> entity_pointer[ENT_BITSHIFT];

					// What we do now is entirely dependant upon whether we have sliding vertical collision on...
					if ((coll_type & COLL_TYPE_SLIDING_VERTICAL) && (which_corner != NEITHER_CORNER))
					{
						if (remainder < 0)
						{
							// THIS AND THE BIT BELOW NEED FIXME SO THEY DEAL WITH THE MAP EDGE
							remainder = WORLDCOLL_push_entity_against_sliding_collision (entity_pointer, DIRECTION_DOWN, which_corner, 0, remainder);
						}
						else
						{
							remainder = WORLDCOLL_push_entity_against_sliding_collision (entity_pointer, DIRECTION_UP, which_corner, 0, remainder);
						}
					}
					else
					{
						remainder = 1;
					}

					if (remainder != 0)
					{
						if (entity_pointer[ENT_WORLD_COLLISION_BEHAVIOUR] & COLLISION_RESPONSE_SIMPLE_BOUNCE_OR_DECELLERATION)
						{
							// If we aren't sliding and we didn't use up all of our movement then we've obviously hit the side of summat.
							entity_pointer[ENT_Y_VEL] *= entity_pointer[ENT_WORLD_COLLISION_COEF_VERTICAL];
							entity_pointer[ENT_Y_VEL] /= 100;
						}
					}

					if (entity_pointer[ENT_WORLD_COLLISION_BEHAVIOUR] & COLLISION_RESPONSE_CALL_SCRIPT)
					{
						if (entity_pointer[ENT_WORLD_HIT_LINE] != UNSET)
						{
							if (y_vel > 0)
							{
								entity_pointer[ENT_WORLD_HIT_SIDE] = DIRECTION_DOWN;
							}
							else
							{
								entity_pointer[ENT_WORLD_HIT_SIDE] = DIRECTION_UP;
							}
							entity_pointer[ENT_WORLD_HIT_AXIS] = VERTICAL_AXIS;

							backup_next_ent = entity_pointer[ENT_NEXT_PROCESS_ENT];

							SCRIPTING_interpret_script (entity_pointer[ENT_OWN_ID] , entity_pointer[ENT_WORLD_HIT_LINE]);

							if (entity_pointer[ENT_ALIVE] != ALIVE)
							{
								global_next_entity = backup_next_ent;
								// Run!
								return;
							}
						}
					}

				}
			}
		}
		else
		{
			// It might not have moved over a pixel boundary but it could still need updating.
			entity_pointer[ENT_Y] += (entity_pointer[ENT_Y_VEL] + entity_pointer[ENT_EXTERNAL_CONVEY_Y]);
			entity_pointer[ENT_WORLD_Y] = entity_pointer[ENT_Y] >> entity_pointer[ENT_BITSHIFT];
			y_vel = 0;
			y_collision_depth = 0;
		}
		break;

	case ENTITY_MOVEMENT_VERTICAL_HORIZONTAL:
		break;

	case ENTITY_MOVEMENT_SIMULTANEOUS_PENETRATIVE:
		break;

	case ENTITY_MOVEMENT_SIMULTANEOUS_NON_PENETRATIVE:
		break;

	default:
		assert(0);
		break;

	}

}



#define SNAP_RESULT_NO_SNAP					(0)
#define SNAP_RESULT_WITHIN_RANGE			(1)
#define SNAP_RESULT_ON_SURFACE				(2)

int WORLDCOLL_test_snap_to_direction (int *entity_pointer , int direction , int max_distance, int big, int iterate, int snap_to_surface, int *returned_snap_depth)
{
	int start_x;
	int end_x;
	int start_y;
	int end_y;

	int shortest_snap_depth;
	int shortest_snap_tile = UNSET;

	int start_bx,end_bx;
	int start_by,end_by;

	int number_of_co_ords;
	int intermediate_block_count;
	int counter;

	int test_x;
	int test_y;

	int snap_depth;
	bool found_tile_within_snap_distance = false;

	switch (direction)
	{
	case DIRECTION_UP:
		start_x = entity_pointer[ENT_WORLD_X] - entity_pointer[ENT_UPPER_WORLD_WIDTH];
		end_x = entity_pointer[ENT_WORLD_X] + entity_pointer[ENT_LOWER_WORLD_WIDTH];

		collision_check_co_ords [0] = start_x;
		collision_check_co_ords [1] = end_x;

		collision_interaction_bitmasks[0] = INTERACTION_POINT_TOP_LEFT;
		collision_interaction_bitmasks[1] = INTERACTION_POINT_TOP_RIGHT;

		start_y = entity_pointer[ENT_WORLD_Y] - entity_pointer[ENT_UPPER_WORLD_HEIGHT];

		if (big)
		{
			start_bx = start_x >> block_size_bitshift;
			end_bx = end_x >> block_size_bitshift;

			if ( start_bx != end_bx )
			{
				intermediate_block_count = end_bx - start_bx;

				for (counter=0; counter<intermediate_block_count; counter++)
				{
					collision_check_co_ords [(counter*2)+2] = ((start_bx+counter) * block_size) + block_size_minus_one;
					collision_check_co_ords [(counter*2)+3] = ((start_bx+counter) * block_size) + block_size;
					collision_interaction_bitmasks[(counter*2)+2] = INTERACTION_POINT_TOP;
					collision_interaction_bitmasks[(counter*2)+3] = INTERACTION_POINT_TOP;
				}

				number_of_co_ords = 2 + (intermediate_block_count * 2);
			}
			else
			{
				collision_interaction_bitmasks[2] = INTERACTION_POINT_TOP;
				collision_check_co_ords [2] = entity_pointer[ENT_WORLD_X];
				number_of_co_ords = 3;
			}
		}
		else
		{
			collision_interaction_bitmasks[2] = INTERACTION_POINT_TOP;
			collision_check_co_ords [2] = entity_pointer[ENT_WORLD_X];
			number_of_co_ords = 3;
		}

		for (counter=0; counter<number_of_co_ords; counter++)
		{
			collision_end_co_ords [counter] = WORLDCOLL_get_total_collision_depth_vertical ( DIRECTION_DOWN , collision_interaction_bitmasks[counter] , entity_pointer[ENT_WORLD_COLLISION_LAYER] , collision_check_co_ords[counter] , start_y-max_distance , entity_pointer[ENT_WORLD_COLLISION_BITMASK] , entity_pointer[ENT_WORLD_COLLISION_BEHAVIOUR] & (COLLISION_HORIZONTAL_WORLD_EDGE_SOLID|COLLISION_VERTICAL_WORLD_EDGE_SOLID) );

			if (iterate)
			{
				if (collision_end_co_ords[counter] == start_y-max_distance)
				{
					// If we arrived okay, then we still need to do an extra check in case we're simply moving too fast.

					if ( ((start_y-max_distance) >> block_size_bitshift) != (start_y >> block_size_bitshift) )
					{
						// This finds the first tile boundary above the end position.
						test_y = (start_y-max_distance) & block_size_inverse; //ie, co-ord is a multiple of block size rounded down.

						do
						{
							test_y += block_size;
							collision_end_co_ords [counter] = WORLDCOLL_get_total_collision_depth_vertical ( DIRECTION_DOWN , collision_interaction_bitmasks[counter] , entity_pointer[ENT_WORLD_COLLISION_LAYER], collision_check_co_ords[counter], test_y , entity_pointer[ENT_WORLD_COLLISION_BITMASK] , entity_pointer[ENT_WORLD_COLLISION_BEHAVIOUR] & (COLLISION_HORIZONTAL_WORLD_EDGE_SOLID|COLLISION_VERTICAL_WORLD_EDGE_SOLID) );
							// And keeps on doing this bit until the test position is to the right of the start position or we register a hit.
						} while ((collision_end_co_ords [counter] == test_y ) && (test_y+block_size < start_y));

						if (collision_end_co_ords [counter] == test_y )
						{
							// The loop exited because we were testing a point to the right of our start.
							// Which means we didn't hit anything along the way so we can safely reset it.
							collision_end_co_ords [counter] = start_y-max_distance;
						}
					}
				}
			}
		}

		shortest_snap_depth = -max_distance;

		for (counter=0; counter<number_of_co_ords; counter++)
		{
			if ( (collision_end_co_ords[counter] > start_y - max_distance) && (collision_end_co_ords[counter] <= start_y) )
			{
				snap_depth = collision_end_co_ords[counter] - start_y;
				found_tile_within_snap_distance = true;

				if (snap_depth > shortest_snap_depth)
				{	
					shortest_snap_depth = snap_depth;
				}
			}
		}

		if (found_tile_within_snap_distance)
		{
			if ((shortest_snap_depth != 0) && (snap_to_surface))
			{
				entity_pointer[ENT_Y] += (shortest_snap_depth << entity_pointer[ENT_BITSHIFT]);
				entity_pointer[ENT_WORLD_Y] = (entity_pointer[ENT_Y] >> entity_pointer[ENT_BITSHIFT]);
			}

			if (returned_snap_depth != NULL)
			{
				*returned_snap_depth = -shortest_snap_depth;
			}

			if (shortest_snap_depth != 0)
			{
				return SNAP_RESULT_WITHIN_RANGE;
			}
			else
			{
				return SNAP_RESULT_ON_SURFACE;
			}
		}
		else
		{
			if (returned_snap_depth != NULL)
			{
				*returned_snap_depth = max_distance;
			}
		}
		break;

	case DIRECTION_DOWN:
		start_x = entity_pointer[ENT_WORLD_X] - entity_pointer[ENT_UPPER_WORLD_WIDTH];
		end_x = entity_pointer[ENT_WORLD_X] + entity_pointer[ENT_LOWER_WORLD_WIDTH];

		collision_check_co_ords [0] = start_x;
		collision_check_co_ords [1] = end_x;

		collision_interaction_bitmasks[0] = INTERACTION_POINT_BOTTOM_LEFT;
		collision_interaction_bitmasks[1] = INTERACTION_POINT_BOTTOM_RIGHT;

		start_y = entity_pointer[ENT_WORLD_Y] + entity_pointer[ENT_LOWER_WORLD_HEIGHT];

		if (big)
		{
			start_bx = start_x >> block_size_bitshift;
			end_bx = end_x >> block_size_bitshift;

			if ( start_bx != end_bx )
			{
				intermediate_block_count = end_bx - start_bx;

				for (counter=0; counter<intermediate_block_count; counter++)
				{
					collision_check_co_ords [(counter*2)+2] = ((start_bx+counter) * block_size) + block_size_minus_one;
					collision_check_co_ords [(counter*2)+3] = ((start_bx+counter) * block_size) + block_size;
					collision_interaction_bitmasks[(counter*2)+2] = INTERACTION_POINT_BOTTOM;
					collision_interaction_bitmasks[(counter*2)+3] = INTERACTION_POINT_BOTTOM;
				}

				number_of_co_ords = 2 + (intermediate_block_count * 2);
			}
			else
			{
				collision_interaction_bitmasks[2] = INTERACTION_POINT_BOTTOM;
				collision_check_co_ords [2] = entity_pointer[ENT_WORLD_X];
				number_of_co_ords = 3;
			}
		}
		else
		{
			collision_interaction_bitmasks[2] = INTERACTION_POINT_BOTTOM;
			collision_check_co_ords [2] = entity_pointer[ENT_WORLD_X];
			number_of_co_ords = 3;
		}

		for (counter=0; counter<number_of_co_ords; counter++)
		{
			collision_end_co_ords [counter] = WORLDCOLL_get_total_collision_depth_vertical ( DIRECTION_UP , collision_interaction_bitmasks[counter] , entity_pointer[ENT_WORLD_COLLISION_LAYER] , collision_check_co_ords[counter] , start_y+max_distance , entity_pointer[ENT_WORLD_COLLISION_BITMASK] , entity_pointer[ENT_WORLD_COLLISION_BEHAVIOUR] & (COLLISION_HORIZONTAL_WORLD_EDGE_SOLID|COLLISION_VERTICAL_WORLD_EDGE_SOLID) );

			if (iterate)
			{
				if (collision_end_co_ords[counter] == start_y+max_distance)
				{
					// If we arrived okay, then we still need to do an extra check in case we're simply moving too fast.

					if ( ((start_y+max_distance) >> block_size_bitshift) != (start_y >> block_size_bitshift) )
					{
						// This finds the first tile boundary below the end position.
						test_y = (start_y+max_distance) | block_size_minus_one;

						do
						{
							test_y -= block_size;
							collision_end_co_ords [counter] = WORLDCOLL_get_total_collision_depth_vertical ( DIRECTION_UP , collision_interaction_bitmasks[counter] , entity_pointer[ENT_WORLD_COLLISION_LAYER], collision_check_co_ords[counter], test_y , entity_pointer[ENT_WORLD_COLLISION_BITMASK] , entity_pointer[ENT_WORLD_COLLISION_BEHAVIOUR] & (COLLISION_HORIZONTAL_WORLD_EDGE_SOLID|COLLISION_VERTICAL_WORLD_EDGE_SOLID) );
							// And keeps on doing this bit until the test position is to the right of the start position or we register a hit.
						} while ((collision_end_co_ords [counter] == test_y ) && (test_y-block_size > start_y));

						if (collision_end_co_ords [counter] == test_y )
						{
							// The loop exited because we were testing a point to the right of our start.
							// Which means we didn't hit anything along the way so we can safely reset it.
							collision_end_co_ords [counter] = start_y+max_distance;
						}
					}
				}
			}
		}

		shortest_snap_depth = max_distance;
		
		for (counter=0; counter<number_of_co_ords; counter++)
		{
			if ( (collision_end_co_ords[counter] < start_y + max_distance) && (collision_end_co_ords[counter] >= start_y) )
			{
				snap_depth = collision_end_co_ords[counter] - start_y;
				found_tile_within_snap_distance = true;
				
				if (snap_depth < shortest_snap_depth)
				{	
					shortest_snap_depth = snap_depth;
				}
			}
			else if (collision_end_co_ords[counter] < start_y)
			{
				// It's ignored!
				return SNAP_RESULT_NO_SNAP;
			}
		}

		if (found_tile_within_snap_distance)
		{
			if ((shortest_snap_depth != 0) && (snap_to_surface))
			{
				entity_pointer[ENT_Y] += (shortest_snap_depth << entity_pointer[ENT_BITSHIFT]);
				entity_pointer[ENT_WORLD_Y] = (entity_pointer[ENT_Y] >> entity_pointer[ENT_BITSHIFT]);
			}

			if (returned_snap_depth != NULL)
			{
				*returned_snap_depth = shortest_snap_depth;
			}
			
			if (shortest_snap_depth != 0)
			{
				return SNAP_RESULT_WITHIN_RANGE;
			}
			else
			{
				return SNAP_RESULT_ON_SURFACE;
			}
		}
		else
		{
			if (returned_snap_depth != NULL)
			{
				*returned_snap_depth = max_distance;
			}
		}
		break;

	case DIRECTION_LEFT:
		start_y = entity_pointer[ENT_WORLD_Y] - entity_pointer[ENT_UPPER_WORLD_HEIGHT];
		end_y = entity_pointer[ENT_WORLD_Y] + entity_pointer[ENT_LOWER_WORLD_HEIGHT];

		collision_check_co_ords [0] = start_y;
		collision_check_co_ords [1] = end_y;

		collision_interaction_bitmasks[0] = INTERACTION_POINT_TOP_LEFT;
		collision_interaction_bitmasks[1] = INTERACTION_POINT_BOTTOM_LEFT;

		start_x = entity_pointer[ENT_WORLD_X] - entity_pointer[ENT_UPPER_WORLD_WIDTH];

		if (big)
		{
			start_by = start_y >> block_size_bitshift;
			end_by = end_y >> block_size_bitshift;

			if ( start_by != end_by )
			{
				intermediate_block_count = end_by - start_by;

				for (counter=0; counter<intermediate_block_count; counter++)
				{
					collision_check_co_ords [(counter*2)+2] = ((start_by+counter) * block_size) + block_size_minus_one;
					collision_check_co_ords [(counter*2)+3] = ((start_by+counter) * block_size) + block_size;
					collision_interaction_bitmasks[(counter*2)+2] = INTERACTION_POINT_LEFT;
					collision_interaction_bitmasks[(counter*2)+3] = INTERACTION_POINT_LEFT;
				}

				number_of_co_ords = 2 + (intermediate_block_count * 2);
			}
			else
			{
				collision_interaction_bitmasks[2] = INTERACTION_POINT_LEFT;
				collision_check_co_ords [2] = entity_pointer[ENT_WORLD_Y];
				number_of_co_ords = 3;
			}
		}
		else
		{
			collision_interaction_bitmasks[2] = INTERACTION_POINT_LEFT;
			collision_check_co_ords [2] = entity_pointer[ENT_WORLD_Y];
			number_of_co_ords = 3;
		}

		for (counter=0; counter<number_of_co_ords; counter++)
		{
			collision_end_co_ords [counter] = WORLDCOLL_get_total_collision_depth_horizontal ( DIRECTION_RIGHT , collision_interaction_bitmasks[counter] , entity_pointer[ENT_WORLD_COLLISION_LAYER] , start_x-max_distance , collision_check_co_ords[counter] , entity_pointer[ENT_WORLD_COLLISION_BITMASK] , entity_pointer[ENT_WORLD_COLLISION_BEHAVIOUR] & (COLLISION_HORIZONTAL_WORLD_EDGE_SOLID|COLLISION_VERTICAL_WORLD_EDGE_SOLID) );

			if (iterate)
			{
				if (collision_end_co_ords[counter] == start_x-max_distance)
				{
					// If we arrived okay, then we still need to do an extra check in case we're simply moving too fast.

					if ( ((start_x-max_distance) >> block_size_bitshift) != (start_x >> block_size_bitshift) )
					{
						// This finds the first tile boundary above the end position.
						test_x = (start_x-max_distance) & block_size_inverse; //ie, co-ord is a multiple of block size rounded down.

						do
						{
							test_x += block_size;
							collision_end_co_ords [counter] = WORLDCOLL_get_total_collision_depth_horizontal ( DIRECTION_RIGHT , collision_interaction_bitmasks[counter] , entity_pointer[ENT_WORLD_COLLISION_LAYER], test_x , collision_check_co_ords[counter], entity_pointer[ENT_WORLD_COLLISION_BITMASK] , entity_pointer[ENT_WORLD_COLLISION_BEHAVIOUR] & (COLLISION_HORIZONTAL_WORLD_EDGE_SOLID|COLLISION_VERTICAL_WORLD_EDGE_SOLID) );
							// And keeps on doing this bit until the test position is to the right of the start position or we register a hit.
						} while ((collision_end_co_ords [counter] == test_x ) && (test_x+block_size < start_x));

						if (collision_end_co_ords [counter] == test_x )
						{
							// The loop exited because we were testing a point to the right of our start.
							// Which means we didn't hit anything along the way so we can safely reset it.
							collision_end_co_ords [counter] = start_x-max_distance;
						}
					}
				}
			}
		}

		shortest_snap_depth = -max_distance;

		for (counter=0; counter<number_of_co_ords; counter++)
		{
			if ( (collision_end_co_ords[counter] > start_x - max_distance) && (collision_end_co_ords[counter] <= start_x) )
			{
				snap_depth = collision_end_co_ords[counter] - start_x;
				found_tile_within_snap_distance = true;

				if (snap_depth > shortest_snap_depth)
				{	
					shortest_snap_depth = snap_depth;
				}
			}
		}

		if (found_tile_within_snap_distance)
		{
			if ((shortest_snap_depth != 0) && (snap_to_surface))
			{
				entity_pointer[ENT_X] += (shortest_snap_depth << entity_pointer[ENT_BITSHIFT]);
				entity_pointer[ENT_WORLD_X] = (entity_pointer[ENT_X] >> entity_pointer[ENT_BITSHIFT]);
			}

			if (returned_snap_depth != NULL)
			{
				*returned_snap_depth = -shortest_snap_depth;
			}

			if (shortest_snap_depth != 0)
			{
				return SNAP_RESULT_WITHIN_RANGE;
			}
			else
			{
				return SNAP_RESULT_ON_SURFACE;
			}
		}
		else
		{
			if (returned_snap_depth != NULL)
			{
				*returned_snap_depth = max_distance;
			}
		}
		break;
		
	case DIRECTION_RIGHT:

		start_y = entity_pointer[ENT_WORLD_Y] - entity_pointer[ENT_UPPER_WORLD_HEIGHT];
		end_y = entity_pointer[ENT_WORLD_Y] + entity_pointer[ENT_LOWER_WORLD_HEIGHT];

		collision_check_co_ords [0] = start_y;
		collision_check_co_ords [1] = end_y;

		collision_interaction_bitmasks[0] = INTERACTION_POINT_TOP_RIGHT;
		collision_interaction_bitmasks[1] = INTERACTION_POINT_BOTTOM_RIGHT;

		start_x = entity_pointer[ENT_WORLD_X] + entity_pointer[ENT_LOWER_WORLD_WIDTH];

		if (big)
		{
			start_by = start_y >> block_size_bitshift;
			end_by = end_y >> block_size_bitshift;

			if ( start_by != end_by )
			{
				intermediate_block_count = end_by - start_by;

				for (counter=0; counter<intermediate_block_count; counter++)
				{
					collision_check_co_ords [(counter*2)+2] = ((start_by+counter) * block_size) + block_size_minus_one;
					collision_check_co_ords [(counter*2)+3] = ((start_by+counter) * block_size) + block_size;
					collision_interaction_bitmasks[(counter*2)+2] = INTERACTION_POINT_RIGHT;
					collision_interaction_bitmasks[(counter*2)+3] = INTERACTION_POINT_RIGHT;
				}

				number_of_co_ords = 2 + (intermediate_block_count * 2);
			}
			else
			{
				collision_interaction_bitmasks[2] = INTERACTION_POINT_RIGHT;
				collision_check_co_ords [2] = entity_pointer[ENT_WORLD_Y];
				number_of_co_ords = 3;
			}
		}
		else
		{
			collision_interaction_bitmasks[2] = INTERACTION_POINT_RIGHT;
			collision_check_co_ords [2] = entity_pointer[ENT_WORLD_Y];
			number_of_co_ords = 3;
		}

		for (counter=0; counter<number_of_co_ords; counter++)
		{
			collision_end_co_ords [counter] = WORLDCOLL_get_total_collision_depth_horizontal ( DIRECTION_RIGHT , collision_interaction_bitmasks[counter] , entity_pointer[ENT_WORLD_COLLISION_LAYER] , start_x+max_distance , collision_check_co_ords[counter] , entity_pointer[ENT_WORLD_COLLISION_BITMASK] , entity_pointer[ENT_WORLD_COLLISION_BEHAVIOUR] & (COLLISION_HORIZONTAL_WORLD_EDGE_SOLID|COLLISION_VERTICAL_WORLD_EDGE_SOLID) );

			if (iterate)
			{
				if (collision_end_co_ords[counter] == start_x+max_distance)
				{
					// If we arrived okay, then we still need to do an extra check in case we're simply moving too fast.

					if ( ((start_x+max_distance) >> block_size_bitshift) != (start_x >> block_size_bitshift) )
					{
						// This finds the first tile boundary below the end position.
						test_x = (start_x+max_distance) | block_size_minus_one;

						do
						{
							test_x -= block_size;
							collision_end_co_ords [counter] = WORLDCOLL_get_total_collision_depth_horizontal ( DIRECTION_RIGHT , collision_interaction_bitmasks[counter] , entity_pointer[ENT_WORLD_COLLISION_LAYER], test_x , collision_check_co_ords[counter]  , entity_pointer[ENT_WORLD_COLLISION_BITMASK] , entity_pointer[ENT_WORLD_COLLISION_BEHAVIOUR] & (COLLISION_HORIZONTAL_WORLD_EDGE_SOLID|COLLISION_VERTICAL_WORLD_EDGE_SOLID) );
							// And keeps on doing this bit until the test position is to the right of the start position or we register a hit.
						} while ((collision_end_co_ords [counter] == test_x ) && (test_x-block_size > start_x));

						if (collision_end_co_ords [counter] == test_x )
						{
							// The loop exited because we were testing a point to the right of our start.
							// Which means we didn't hit anything along the way so we can safely reset it.
							collision_end_co_ords [counter] = start_x+max_distance;
						}
					}
				}
			}
		}

		shortest_snap_depth = max_distance;
		
		for (counter=0; counter<number_of_co_ords; counter++)
		{
			if ( (collision_end_co_ords[counter] < start_x + max_distance) && (collision_end_co_ords[counter] >= start_x) )
			{
				snap_depth = collision_end_co_ords[counter] - start_x;
				found_tile_within_snap_distance = true;
				
				if (snap_depth < shortest_snap_depth)
				{	
					shortest_snap_depth = snap_depth;
				}
			}
			else if (collision_end_co_ords[counter] < start_x)
			{
				// It's ignored!
				return SNAP_RESULT_NO_SNAP;
			}
		}

		if (found_tile_within_snap_distance)
		{
			if ((shortest_snap_depth != 0) && (snap_to_surface))
			{
				entity_pointer[ENT_X] += (shortest_snap_depth << entity_pointer[ENT_BITSHIFT]);
				entity_pointer[ENT_WORLD_X] = (entity_pointer[ENT_X] >> entity_pointer[ENT_BITSHIFT]);
			}

			if (returned_snap_depth != NULL)
			{
				*returned_snap_depth = shortest_snap_depth;
			}
			
			if (shortest_snap_depth != 0)
			{
				return SNAP_RESULT_WITHIN_RANGE;
			}
			else
			{
				return SNAP_RESULT_ON_SURFACE;
			}
		}
		else
		{
			if (returned_snap_depth != NULL)
			{
				*returned_snap_depth = max_distance;
			}
		}
		break;
	}

	return SNAP_RESULT_NO_SNAP;
}



int WORLDCOLL_get_solid_corner_tile_in_direction (int *entity_pointer , int direction , int test_distance, int reset_list)
{
	// This looks at the two corners specified and returns the value of the highest priority tile they
	// are in contact with IF they are of a compatible type. That is to say if the direction is UP or
	// DOWN the right-hand corner will not return RIGHT_SLOPEs and vice versa.

	// Although this routine may be called by the scripts themselves they can also be called
	// by shortcut routines in the future if certain ideal ways of doing things are found.

	int end_x;
	int end_y;
	int start_x;
	int start_y;

	int number_of_co_ords;

	if (reset_list)
	{
		collision_blocks_found_count = 0;
		collision_blocks_ignore_count = 0;
	}

	int counter;
	
	switch (direction)
	{
	case DIRECTION_UP:
		start_x = end_x = entity_pointer[ENT_WORLD_X];
		start_x -= entity_pointer[ENT_UPPER_WORLD_WIDTH];
		end_x += entity_pointer[ENT_LOWER_WORLD_WIDTH];
		start_y = entity_pointer[ENT_WORLD_Y] - entity_pointer[ENT_UPPER_WORLD_HEIGHT] - test_distance;

		number_of_co_ords = 2;
		
		collision_check_co_ords [0] = start_x;
		collision_check_co_ords [1] = end_x;

		collision_interaction_bitmasks [0] = INTERACTION_POINT_TOP_LEFT;
		collision_interaction_bitmasks [1] = INTERACTION_POINT_TOP_RIGHT;

		collision_ignore_bitmasks [0] = BOOL_SLOPE_LEFT;
		collision_ignore_bitmasks [1] = BOOL_SLOPE_RIGHT;

		number_of_co_ords = 2;

		for (counter=0; counter<number_of_co_ords; counter++)
		{
			collision_end_co_ords[counter] = WORLDCOLL_get_total_collision_depth_vertical ( DIRECTION_DOWN , collision_interaction_bitmasks[counter] , entity_pointer[ENT_WORLD_COLLISION_LAYER], collision_check_co_ords [counter] , start_y , entity_pointer[ENT_WORLD_COLLISION_BITMASK] , entity_pointer[ENT_WORLD_COLLISION_BEHAVIOUR] & (COLLISION_HORIZONTAL_WORLD_EDGE_SOLID|COLLISION_VERTICAL_WORLD_EDGE_SOLID) );

			if (collision_end_co_ords[counter] == start_y+1)
			{
				// ie, it's on the edge of the collision and so was pushed back a pixel...
				WORLDCOLL_add_tile_to_user_list (entity_pointer[ENT_WORLD_COLLISION_LAYER],collision_check_co_ords[counter],start_y,collision_ignore_bitmasks[counter]);
			}
			else if (collision_end_co_ords[counter] > start_y+1)
			{
				collision_blocks_ignore_count++;
			}
		}
		break;

	case DIRECTION_DOWN:
		start_x = end_x = entity_pointer[ENT_WORLD_X];
		start_x -= entity_pointer[ENT_UPPER_WORLD_WIDTH];
		end_x += entity_pointer[ENT_LOWER_WORLD_WIDTH];
		start_y = entity_pointer[ENT_WORLD_Y] + entity_pointer[ENT_LOWER_WORLD_HEIGHT] + test_distance;

		number_of_co_ords = 2;

		collision_check_co_ords [0] = start_x;
		collision_check_co_ords [1] = end_x;

		collision_interaction_bitmasks [0] = INTERACTION_POINT_BOTTOM_LEFT;
		collision_interaction_bitmasks [1] = INTERACTION_POINT_BOTTOM_RIGHT;

		collision_ignore_bitmasks [0] = BOOL_SLOPE_LEFT;
		collision_ignore_bitmasks [1] = BOOL_SLOPE_RIGHT;

		number_of_co_ords = 2;

		for (counter=0; counter<number_of_co_ords; counter++)
		{
			collision_end_co_ords[counter] = WORLDCOLL_get_total_collision_depth_vertical ( DIRECTION_UP , collision_interaction_bitmasks[counter] , entity_pointer[ENT_WORLD_COLLISION_LAYER], collision_check_co_ords [counter] , start_y , entity_pointer[ENT_WORLD_COLLISION_BITMASK] , entity_pointer[ENT_WORLD_COLLISION_BEHAVIOUR] & (COLLISION_HORIZONTAL_WORLD_EDGE_SOLID|COLLISION_VERTICAL_WORLD_EDGE_SOLID) );

			if (collision_end_co_ords[counter] == start_y-1)
			{
				// ie, it's on the edge of the collision and so was pushed back a pixel...
				WORLDCOLL_add_tile_to_user_list (entity_pointer[ENT_WORLD_COLLISION_LAYER],collision_check_co_ords[counter],start_y,collision_ignore_bitmasks[counter]);
			}
			else if (collision_end_co_ords[counter] < start_y-1)
			{
				collision_blocks_ignore_count++;
			}
		}
		break;

	case DIRECTION_LEFT:
		start_y = end_y = entity_pointer[ENT_WORLD_Y];
		start_y -= entity_pointer[ENT_UPPER_WORLD_HEIGHT];
		end_y += entity_pointer[ENT_LOWER_WORLD_HEIGHT];
		start_x = entity_pointer[ENT_WORLD_X] - entity_pointer[ENT_UPPER_WORLD_WIDTH] - test_distance;

		number_of_co_ords = 2;
		
		collision_check_co_ords [0] = start_y;
		collision_check_co_ords [1] = end_y;

		collision_interaction_bitmasks [0] = INTERACTION_POINT_TOP_LEFT;
		collision_interaction_bitmasks [1] = INTERACTION_POINT_BOTTOM_LEFT;

		collision_ignore_bitmasks [0] = BOOL_SLOPE_RIGHT;
		collision_ignore_bitmasks [1] = BOOL_SLOPE_RIGHT;

		number_of_co_ords = 2;

		for (counter=0; counter<number_of_co_ords; counter++)
		{
			collision_end_co_ords[counter] = WORLDCOLL_get_total_collision_depth_horizontal ( DIRECTION_RIGHT , collision_interaction_bitmasks[counter] , entity_pointer[ENT_WORLD_COLLISION_LAYER] , start_x , collision_check_co_ords [counter], entity_pointer[ENT_WORLD_COLLISION_BITMASK] , entity_pointer[ENT_WORLD_COLLISION_BEHAVIOUR] & (COLLISION_HORIZONTAL_WORLD_EDGE_SOLID|COLLISION_VERTICAL_WORLD_EDGE_SOLID) );

			if (collision_end_co_ords[counter] == start_x+1)
			{
				// ie, it's on the edge of the collision and so was pushed back a pixel...
				WORLDCOLL_add_tile_to_user_list (entity_pointer[ENT_WORLD_COLLISION_LAYER],start_y,collision_check_co_ords[counter],collision_ignore_bitmasks[counter]);
			}
			else if (collision_end_co_ords[counter] > start_x+1)
			{
				collision_blocks_ignore_count++;
			}
		}
		break;

	case DIRECTION_RIGHT:
		start_y = end_y = entity_pointer[ENT_WORLD_Y];
		start_y -= entity_pointer[ENT_UPPER_WORLD_HEIGHT];
		end_y += entity_pointer[ENT_LOWER_WORLD_HEIGHT];
		start_x = entity_pointer[ENT_WORLD_X] + entity_pointer[ENT_LOWER_WORLD_WIDTH] + test_distance;

		number_of_co_ords = 2;
		
		collision_check_co_ords [0] = start_y;
		collision_check_co_ords [1] = end_y;

		collision_interaction_bitmasks [0] = INTERACTION_POINT_TOP_RIGHT;
		collision_interaction_bitmasks [1] = INTERACTION_POINT_BOTTOM_RIGHT;

		collision_ignore_bitmasks [0] = BOOL_SLOPE_LEFT;
		collision_ignore_bitmasks [1] = BOOL_SLOPE_LEFT;

		number_of_co_ords = 2;

		for (counter=0; counter<number_of_co_ords; counter++)
		{
			collision_end_co_ords[counter] = WORLDCOLL_get_total_collision_depth_horizontal ( DIRECTION_RIGHT , collision_interaction_bitmasks[counter] , entity_pointer[ENT_WORLD_COLLISION_LAYER] , start_x , collision_check_co_ords [counter], entity_pointer[ENT_WORLD_COLLISION_BITMASK] , entity_pointer[ENT_WORLD_COLLISION_BEHAVIOUR] & (COLLISION_HORIZONTAL_WORLD_EDGE_SOLID|COLLISION_VERTICAL_WORLD_EDGE_SOLID) );

			if (collision_end_co_ords[counter] == start_x-1)
			{
				// ie, it's on the edge of the collision and so was pushed back a pixel...
				WORLDCOLL_add_tile_to_user_list (entity_pointer[ENT_WORLD_COLLISION_LAYER],start_y,collision_check_co_ords[counter],collision_ignore_bitmasks[counter]);
			}
			else if (collision_end_co_ords[counter] < start_x-1)
			{
				collision_blocks_ignore_count++;
			}
		}
		break;
	}

	return collision_blocks_found_count;

}



int WORLDCOLL_get_solid_inner_tile_in_direction (int *entity_pointer, int direction , int test_distance, int reset_list)
{
	// This is similar to the above function except it does not discount corner tiles itself
	// (but that may be done by the script afterwards) and only looks at the tile boundaries
	// within the corners of the tiles.

	// Although this routine may be called by the scripts themselves they can also be called
	// by shortcut routines in the future if certain ideal ways of doing things are found.

	// Returns the number of solid tiles found.

	int end_x;
	int end_y;
	int start_x;
	int start_y;

	int intermediate_block_count;
	int number_of_co_ords;

	if (reset_list)
	{
		collision_blocks_found_count = 0;
		collision_blocks_ignore_count = 0;
	}

	int start_bx,end_bx,start_by,end_by;

	int counter;
	
	switch (direction)
	{
	case DIRECTION_UP:
		start_x = end_x = entity_pointer[ENT_WORLD_X];
		start_x -= (entity_pointer[ENT_UPPER_WORLD_WIDTH] - 1);
		end_x += (entity_pointer[ENT_LOWER_WORLD_WIDTH] - 1);
		start_y = entity_pointer[ENT_WORLD_Y] - entity_pointer[ENT_UPPER_WORLD_HEIGHT] - test_distance;

		start_bx = start_x >> block_size_bitshift;
		end_bx = end_x >> block_size_bitshift;

		intermediate_block_count = end_bx - start_bx;

		if (intermediate_block_count)
		{
			for (counter=0; counter<intermediate_block_count; counter++)
			{
				collision_check_co_ords [(counter*2)] = ((start_bx+counter) * block_size) + block_size_minus_one;
				collision_check_co_ords [(counter*2)+1] = ((start_bx+counter) * block_size) + block_size;
			}

			number_of_co_ords = intermediate_block_count * 2;

			for (counter=0; counter<number_of_co_ords; counter++)
			{
				collision_end_co_ords[counter] = WORLDCOLL_get_total_collision_depth_vertical ( DIRECTION_DOWN , INTERACTION_POINT_TOP , entity_pointer[ENT_WORLD_COLLISION_LAYER], collision_check_co_ords [counter] , start_y , entity_pointer[ENT_WORLD_COLLISION_BITMASK] , entity_pointer[ENT_WORLD_COLLISION_BEHAVIOUR] & (COLLISION_HORIZONTAL_WORLD_EDGE_SOLID|COLLISION_VERTICAL_WORLD_EDGE_SOLID) );

				if (collision_end_co_ords[counter] == start_y+1)
				{
					// ie, it's on the edge of the collision and so was pushed back a pixel...
					WORLDCOLL_add_tile_to_user_list (entity_pointer[ENT_WORLD_COLLISION_LAYER],collision_check_co_ords[counter],start_y);
				}
				else if (collision_end_co_ords[counter] > start_y+1)
				{
					collision_blocks_ignore_count++;
				}

			}
		}
		else
		{
			// If the object doesn't span a block then instead it uses a point.

			collision_end_co_ords[0] = WORLDCOLL_get_total_collision_depth_vertical ( DIRECTION_DOWN , INTERACTION_POINT_TOP , entity_pointer[ENT_WORLD_COLLISION_LAYER], entity_pointer[ENT_WORLD_X] , start_y , entity_pointer[ENT_WORLD_COLLISION_BITMASK] , entity_pointer[ENT_WORLD_COLLISION_BEHAVIOUR] & (COLLISION_HORIZONTAL_WORLD_EDGE_SOLID|COLLISION_VERTICAL_WORLD_EDGE_SOLID) );

			if (collision_end_co_ords[0] == start_y+1)
			{
				// ie, it's on the edge of the collision and so was pushed back a pixel...
				WORLDCOLL_add_tile_to_user_list (entity_pointer[ENT_WORLD_COLLISION_LAYER],collision_check_co_ords[0],start_y);
			}
			else if (collision_end_co_ords[0] > start_y+1)
			{
				collision_blocks_ignore_count++;
			}
		}
		break;

	case DIRECTION_DOWN:
		start_x = end_x = entity_pointer[ENT_WORLD_X];
		start_x -= (entity_pointer[ENT_UPPER_WORLD_WIDTH] - 1);
		end_x += (entity_pointer[ENT_LOWER_WORLD_WIDTH] - 1);
		start_y = entity_pointer[ENT_WORLD_Y] + entity_pointer[ENT_LOWER_WORLD_HEIGHT] + test_distance;

		start_bx = start_x >> block_size_bitshift;
		end_bx = end_x >> block_size_bitshift;

		intermediate_block_count = end_bx - start_bx;

		if (intermediate_block_count)
		{
			for (counter=0; counter<intermediate_block_count; counter++)
			{
				collision_check_co_ords [(counter*2)] = ((start_bx+counter) * block_size) + block_size_minus_one;
				collision_check_co_ords [(counter*2)+1] = ((start_bx+counter) * block_size) + block_size;
			}

			number_of_co_ords = intermediate_block_count * 2;

			for (counter=0; counter<number_of_co_ords; counter++)
			{
				collision_end_co_ords[counter] = WORLDCOLL_get_total_collision_depth_vertical ( DIRECTION_UP , INTERACTION_POINT_BOTTOM , entity_pointer[ENT_WORLD_COLLISION_LAYER], collision_check_co_ords [counter] , start_y , entity_pointer[ENT_WORLD_COLLISION_BITMASK] , entity_pointer[ENT_WORLD_COLLISION_BEHAVIOUR] & (COLLISION_HORIZONTAL_WORLD_EDGE_SOLID|COLLISION_VERTICAL_WORLD_EDGE_SOLID) );

				if (collision_end_co_ords[counter] == start_y-1)
				{
					// ie, it's on the edge of the collision and so was pushed back a pixel...
					WORLDCOLL_add_tile_to_user_list (entity_pointer[ENT_WORLD_COLLISION_LAYER],collision_check_co_ords[counter],start_y);
				}
				else if (collision_end_co_ords[counter] < start_y-1)
				{
					collision_blocks_ignore_count++;
				}
			}
		}
		else
		{
			// If the object doesn't span a block then instead it uses a point.

			collision_end_co_ords[0] = WORLDCOLL_get_total_collision_depth_vertical ( DIRECTION_UP , INTERACTION_POINT_BOTTOM , entity_pointer[ENT_WORLD_COLLISION_LAYER], entity_pointer[ENT_WORLD_X] , start_y , entity_pointer[ENT_WORLD_COLLISION_BITMASK] , entity_pointer[ENT_WORLD_COLLISION_BEHAVIOUR] & (COLLISION_HORIZONTAL_WORLD_EDGE_SOLID|COLLISION_VERTICAL_WORLD_EDGE_SOLID) );

			if (collision_end_co_ords[0] == start_y-1)
			{
				// ie, it's on the edge of the collision and so was pushed back a pixel...
				WORLDCOLL_add_tile_to_user_list (entity_pointer[ENT_WORLD_COLLISION_LAYER],collision_check_co_ords[0],start_y);
			}
			else if (collision_end_co_ords[0] < start_y-1)
			{
				collision_blocks_ignore_count++;
			}
		}
		break;

	case DIRECTION_LEFT:
		start_y = end_y = entity_pointer[ENT_WORLD_Y];
		start_y -= (entity_pointer[ENT_UPPER_WORLD_HEIGHT] - 1);
		end_y += (entity_pointer[ENT_LOWER_WORLD_HEIGHT] - 1);
		start_x = entity_pointer[ENT_WORLD_X] - entity_pointer[ENT_UPPER_WORLD_WIDTH] - test_distance;

		start_by = start_y >> block_size_bitshift;
		end_by = end_y >> block_size_bitshift;

		intermediate_block_count = end_by - start_by;

		if (intermediate_block_count)
		{
			for (counter=0; counter<intermediate_block_count; counter++)
			{
				collision_check_co_ords [(counter*2)] = ((start_by+counter) * block_size) + block_size_minus_one;
				collision_check_co_ords [(counter*2)+1] = ((start_by+counter) * block_size) + block_size;
			}

			number_of_co_ords = intermediate_block_count * 2;

			for (counter=0; counter<number_of_co_ords; counter++)
			{
				collision_end_co_ords[counter] = WORLDCOLL_get_total_collision_depth_horizontal ( DIRECTION_RIGHT , INTERACTION_POINT_LEFT , entity_pointer[ENT_WORLD_COLLISION_LAYER] , start_x , collision_check_co_ords [counter] , entity_pointer[ENT_WORLD_COLLISION_BITMASK] , entity_pointer[ENT_WORLD_COLLISION_BEHAVIOUR] & (COLLISION_HORIZONTAL_WORLD_EDGE_SOLID|COLLISION_VERTICAL_WORLD_EDGE_SOLID) );

				if (collision_end_co_ords[counter] == start_x+1)
				{
					// ie, it's on the edge of the collision and so was pushed back a pixel...
					WORLDCOLL_add_tile_to_user_list (entity_pointer[ENT_WORLD_COLLISION_LAYER],start_x,collision_check_co_ords[counter]);
				}
				else if (collision_end_co_ords[counter] > start_x+1)
				{
					collision_blocks_ignore_count++;
				}
			}
		}
		else
		{
			// If the object doesn't span a block then instead it uses a point.

			collision_end_co_ords[0] = WORLDCOLL_get_total_collision_depth_vertical ( DIRECTION_RIGHT , INTERACTION_POINT_LEFT , entity_pointer[ENT_WORLD_COLLISION_LAYER], start_x , entity_pointer[ENT_WORLD_Y] , entity_pointer[ENT_WORLD_COLLISION_BITMASK] , entity_pointer[ENT_WORLD_COLLISION_BEHAVIOUR] & (COLLISION_HORIZONTAL_WORLD_EDGE_SOLID|COLLISION_VERTICAL_WORLD_EDGE_SOLID) );

			if (collision_end_co_ords[0] == start_x+1)
			{
				// ie, it's on the edge of the collision and so was pushed back a pixel...
				WORLDCOLL_add_tile_to_user_list (entity_pointer[ENT_WORLD_COLLISION_LAYER],start_x,collision_check_co_ords[0]);
			}
			else if (collision_end_co_ords[0] > start_x+1)
			{
				collision_blocks_ignore_count++;
			}
		}
		break;

	case DIRECTION_RIGHT:
		start_y = end_y = entity_pointer[ENT_WORLD_Y];
		start_y -= (entity_pointer[ENT_UPPER_WORLD_HEIGHT] - 1);
		end_y += (entity_pointer[ENT_LOWER_WORLD_HEIGHT] - 1);
		start_x = entity_pointer[ENT_WORLD_X] + entity_pointer[ENT_LOWER_WORLD_WIDTH] + test_distance;

		start_by = start_y >> block_size_bitshift;
		end_by = end_y >> block_size_bitshift;

		intermediate_block_count = end_by - start_by;

		if (intermediate_block_count)
		{
			for (counter=0; counter<intermediate_block_count; counter++)
			{
				collision_check_co_ords [(counter*2)] = ((start_by+counter) * block_size) + block_size_minus_one;
				collision_check_co_ords [(counter*2)+1] = ((start_by+counter) * block_size) + block_size;
			}

			number_of_co_ords = intermediate_block_count * 2;

			for (counter=0; counter<number_of_co_ords; counter++)
			{
				collision_end_co_ords[counter] = WORLDCOLL_get_total_collision_depth_horizontal ( DIRECTION_LEFT , INTERACTION_POINT_RIGHT , entity_pointer[ENT_WORLD_COLLISION_LAYER] , start_x , collision_check_co_ords [counter] , entity_pointer[ENT_WORLD_COLLISION_BITMASK] , entity_pointer[ENT_WORLD_COLLISION_BEHAVIOUR] & (COLLISION_HORIZONTAL_WORLD_EDGE_SOLID|COLLISION_VERTICAL_WORLD_EDGE_SOLID) );

				if (collision_end_co_ords[counter] == start_x-1)
				{
					// ie, it's on the edge of the collision and so was pushed back a pixel...
					WORLDCOLL_add_tile_to_user_list (entity_pointer[ENT_WORLD_COLLISION_LAYER],start_x,collision_check_co_ords[counter],0);
				}
				else if (collision_end_co_ords[counter] < start_x-1)
				{
					collision_blocks_ignore_count++;
				}
			}
		}
		else
		{
			// If the object doesn't span a block then instead it uses a point.

			collision_end_co_ords[0] = WORLDCOLL_get_total_collision_depth_vertical ( DIRECTION_LEFT , INTERACTION_POINT_RIGHT , entity_pointer[ENT_WORLD_COLLISION_LAYER], start_x , entity_pointer[ENT_WORLD_Y] , entity_pointer[ENT_WORLD_COLLISION_BITMASK] , entity_pointer[ENT_WORLD_COLLISION_BEHAVIOUR] & (COLLISION_HORIZONTAL_WORLD_EDGE_SOLID|COLLISION_VERTICAL_WORLD_EDGE_SOLID) );

			if (collision_end_co_ords[0] == start_x-1)
			{
				// ie, it's on the edge of the collision and so was pushed back a pixel...
				WORLDCOLL_add_tile_to_user_list (entity_pointer[ENT_WORLD_COLLISION_LAYER],start_x,collision_check_co_ords[0]);
			}
			else if (collision_end_co_ords[0] < start_x-1)
			{
				collision_blocks_ignore_count++;
			}
		}
		break;
	}

	return collision_blocks_found_count;
}



int WORLDCOLL_get_tile_at_offset (int *entity_pointer, int x_offset , int y_offset, int solid_or_not, int reset_list)
{
	if (reset_list)
	{
		collision_blocks_found_count = 0;
		collision_blocks_ignore_count = 0;
	}

	if (solid_or_not)
	{
		if (WORLDCOLL_collision_test (entity_pointer[ENT_WORLD_COLLISION_LAYER] , entity_pointer[ENT_WORLD_X]+x_offset,entity_pointer[ENT_WORLD_Y]+y_offset , entity_pointer[ENT_WORLD_COLLISION_BITMASK]) )
		{
			WORLDCOLL_add_tile_to_user_list (entity_pointer[ENT_WORLD_COLLISION_LAYER],entity_pointer[ENT_WORLD_X]+x_offset,entity_pointer[ENT_WORLD_Y]+y_offset,0);
			return collision_blocks_found_count;
		}
		else
		{
			return collision_blocks_found_count;
		}
	}
	else
	{
		WORLDCOLL_add_tile_to_user_list (entity_pointer[ENT_WORLD_COLLISION_LAYER],entity_pointer[ENT_WORLD_X]+x_offset,entity_pointer[ENT_WORLD_Y]+y_offset,0);
		return collision_blocks_found_count;
	}
}



void WORLDCOLL_apply_attributes (int *entity_pointer, int attribute , int only_use_boolean)
{
	int b;
	int tile_number;
	int total_number = 0;
	int total_attrib = 0;

	switch(attribute)
	{
	case PROP_ACCELL_X:
		for (b=0; b<collision_blocks_found_count; b++)
		{
			tile_number = collision_blocks_found[b];

			if ((tile_boolean_property_list[tile_number] & only_use_boolean) > 0)
			{
				total_number++;
				total_attrib += ts[collision_tileset].tileset_pointer[tile_number].accel_x;
			}
		}

		total_attrib /= total_number;
		entity_pointer[ENT_EXTERNAL_ACCELL_X] = total_attrib;
		break;

	case PROP_ACCELL_Y:
		for (b=0; b<collision_blocks_found_count; b++)
		{
			tile_number = collision_blocks_found[b];

			if ((tile_boolean_property_list[tile_number] & only_use_boolean) > 0)
			{
				total_number++;
				total_attrib += ts[collision_tileset].tileset_pointer[tile_number].accel_y;
			}
		}

		total_attrib /= total_number;
		entity_pointer[ENT_EXTERNAL_ACCELL_Y] = total_attrib;
		break;

	case PROP_CONVEY_X:
		for (b=0; b<collision_blocks_found_count; b++)
		{
			tile_number = collision_blocks_found[b];

			if ((tile_boolean_property_list[tile_number] & only_use_boolean) > 0)
			{
				total_number++;
				total_attrib += ts[collision_tileset].tileset_pointer[tile_number].convey_x;
			}
		}

		total_attrib /= total_number;
		entity_pointer[ENT_EXTERNAL_CONVEY_X] = total_attrib;
		break;

	case PROP_CONVEY_Y:
		for (b=0; b<collision_blocks_found_count; b++)
		{
			tile_number = collision_blocks_found[b];

			if ((tile_boolean_property_list[tile_number] & only_use_boolean) > 0)
			{
				total_number++;
				total_attrib += ts[collision_tileset].tileset_pointer[tile_number].convey_y;
			}
		}

		total_attrib /= total_number;
		entity_pointer[ENT_EXTERNAL_CONVEY_Y] = total_attrib;
		break;

	case PROP_FRICTION_X:
		for (b=0; b<collision_blocks_found_count; b++)
		{
			tile_number = collision_blocks_found[b];

			if ((tile_boolean_property_list[tile_number] & only_use_boolean) > 0)
			{
				total_number++;
				total_attrib += ts[collision_tileset].tileset_pointer[tile_number].friction;
			}
		}

		total_attrib /= total_number;
		entity_pointer[ENT_X_VEL] *= total_attrib;
		entity_pointer[ENT_X_VEL] /= 100;
		break;

	case PROP_FRICTION_Y:
		for (b=0; b<collision_blocks_found_count; b++)
		{
			tile_number = collision_blocks_found[b];

			if ((tile_boolean_property_list[tile_number] & only_use_boolean) > 0)
			{
				total_number++;
				total_attrib += ts[collision_tileset].tileset_pointer[tile_number].friction;
			}
		}

		total_attrib /= total_number;
		entity_pointer[ENT_Y_VEL] *= total_attrib;
		entity_pointer[ENT_Y_VEL] /= 100;
		break;

	default:
		assert(0);
		break;
	}
}



int WORLDCOLL_get_aggregate_booleans (void)
{
	int b;
	int tile_number;
	int aggregate = 0;

	for (b=0; b<collision_blocks_found_count; b++)
	{
		tile_number = collision_blocks_found[b];

		aggregate |= tile_boolean_property_list[tile_number];
	}
	
	return aggregate;
}



int WORLDCOLL_get_solid_single_tile_in_direction (int *entity_pointer, int direction , int test_distance, int reset_list, int offset)
{
	// This is similar to the above function except it does not discount corner tiles itself
	// (but that may be done by the script afterwards) and only looks at the tile boundaries
	// within the corners of the tiles.

	// Although this routine may be called by the scripts themselves they can also be called
	// by shortcut routines in the future if certain ideal ways of doing things are found.

	// Returns the number of solid tiles found.

	int start_x;
	int start_y;

	if (reset_list)
	{
		collision_blocks_found_count = 0;
	}
	
	switch (direction)
	{
	case DIRECTION_UP:
		start_x = entity_pointer[ENT_WORLD_X];
		start_x += offset;
		start_y = entity_pointer[ENT_WORLD_Y] - entity_pointer[ENT_UPPER_WORLD_HEIGHT] - test_distance;

		if (WORLDCOLL_get_total_collision_depth_vertical ( DIRECTION_DOWN , INTERACTION_POINT_TOP , entity_pointer[ENT_WORLD_COLLISION_LAYER], start_x , start_y , entity_pointer[ENT_WORLD_COLLISION_BITMASK] , entity_pointer[ENT_WORLD_COLLISION_BEHAVIOUR] & (COLLISION_HORIZONTAL_WORLD_EDGE_SOLID|COLLISION_VERTICAL_WORLD_EDGE_SOLID) ) == start_y+1)
		{
			// ie, it's on the edge of the collision and so was pushed back a pixel...
			WORLDCOLL_add_tile_to_user_list (entity_pointer[ENT_WORLD_COLLISION_LAYER],start_x,start_y);
		}
		break;

	case DIRECTION_DOWN:
		start_x = entity_pointer[ENT_WORLD_X];
		start_x += offset;
		start_y = entity_pointer[ENT_WORLD_Y] + entity_pointer[ENT_LOWER_WORLD_HEIGHT] + test_distance;

		if (WORLDCOLL_get_total_collision_depth_vertical ( DIRECTION_UP , INTERACTION_POINT_BOTTOM , entity_pointer[ENT_WORLD_COLLISION_LAYER], start_x , start_y , entity_pointer[ENT_WORLD_COLLISION_BITMASK] , entity_pointer[ENT_WORLD_COLLISION_BEHAVIOUR] & (COLLISION_HORIZONTAL_WORLD_EDGE_SOLID|COLLISION_VERTICAL_WORLD_EDGE_SOLID) ) == start_y-1)
		{
			// ie, it's on the edge of the collision and so was pushed back a pixel...
			WORLDCOLL_add_tile_to_user_list (entity_pointer[ENT_WORLD_COLLISION_LAYER],start_x,start_y);
		}
		break;

	case DIRECTION_LEFT:
		start_y = entity_pointer[ENT_WORLD_Y];
		start_y += offset;
		start_x = entity_pointer[ENT_WORLD_X] - entity_pointer[ENT_UPPER_WORLD_WIDTH] - test_distance;

		if (WORLDCOLL_get_total_collision_depth_horizontal( DIRECTION_RIGHT , INTERACTION_POINT_LEFT , entity_pointer[ENT_WORLD_COLLISION_LAYER], start_x , start_y , entity_pointer[ENT_WORLD_COLLISION_BITMASK] , entity_pointer[ENT_WORLD_COLLISION_BEHAVIOUR] & (COLLISION_HORIZONTAL_WORLD_EDGE_SOLID|COLLISION_VERTICAL_WORLD_EDGE_SOLID) ) == start_x+1)
		{
			// ie, it's on the edge of the collision and so was pushed back a pixel...
			WORLDCOLL_add_tile_to_user_list (entity_pointer[ENT_WORLD_COLLISION_LAYER],start_x,start_y);
		}
		break;

	case DIRECTION_RIGHT:
		start_y = entity_pointer[ENT_WORLD_Y];
		start_y += offset;
		start_x = entity_pointer[ENT_WORLD_X] + entity_pointer[ENT_LOWER_WORLD_WIDTH] + test_distance;

		if (WORLDCOLL_get_total_collision_depth_horizontal( DIRECTION_LEFT , INTERACTION_POINT_RIGHT , entity_pointer[ENT_WORLD_COLLISION_LAYER], start_x , start_y , entity_pointer[ENT_WORLD_COLLISION_BITMASK] , entity_pointer[ENT_WORLD_COLLISION_BEHAVIOUR] & (COLLISION_HORIZONTAL_WORLD_EDGE_SOLID|COLLISION_VERTICAL_WORLD_EDGE_SOLID) ) == start_x-1)
		{
			// ie, it's on the edge of the collision and so was pushed back a pixel...
			WORLDCOLL_add_tile_to_user_list (entity_pointer[ENT_WORLD_COLLISION_LAYER],start_x,start_y);
		}
		break;
	}

	return collision_blocks_found_count;
}



int WORLDCOLL_get_boolean_tile_properties (int tile_number)
{
	return (ts[collision_tileset].tileset_pointer[tile_number].boolean_properties);
}



void WORLDCOLL_apply_dual_tile_movement_properties (int *entity_pointer, int property, int tile_number_1, int tile_number_2)
{

	if ( (tile_number_1 == UNSET) && (tile_number_2 == UNSET) )
	{
		// Both are unset, so do nuffink.
		return;
	}
	else if (tile_number_1 == UNSET)
	{
		WORLDCOLL_apply_tile_movement_property (entity_pointer, property, tile_number_2);
	}
	else if (tile_number_2 == UNSET)
	{
		WORLDCOLL_apply_tile_movement_property (entity_pointer, property, tile_number_1);
	}
	else
	{
		switch (property)
		{
		case PROP_ACCELL_X:
			entity_pointer[ENT_EXTERNAL_ACCELL_X] = ( ts[collision_tileset].tileset_pointer[tile_number_1].accel_x + ts[collision_tileset].tileset_pointer[tile_number_2].accel_x ) / 2;
			break;

		case PROP_ACCELL_Y:
			entity_pointer[ENT_EXTERNAL_ACCELL_Y] = ( ts[collision_tileset].tileset_pointer[tile_number_1].accel_y + ts[collision_tileset].tileset_pointer[tile_number_2].accel_y ) / 2;
			break;

		case PROP_CONVEY_X:
			entity_pointer[ENT_EXTERNAL_CONVEY_X] = ( ts[collision_tileset].tileset_pointer[tile_number_1].convey_x + ts[collision_tileset].tileset_pointer[tile_number_2].convey_x ) / 2;
			break;

		case PROP_CONVEY_Y:
			entity_pointer[ENT_EXTERNAL_CONVEY_Y] = ( ts[collision_tileset].tileset_pointer[tile_number_1].convey_y + ts[collision_tileset].tileset_pointer[tile_number_2].convey_y ) / 2;
			break;

		case PROP_FRICTION_X:
			entity_pointer[ENT_X_VEL] *= ( ( ts[collision_tileset].tileset_pointer[tile_number_1].friction + ts[collision_tileset].tileset_pointer[tile_number_2].friction ) / 2);
			entity_pointer[ENT_X_VEL] /= 100;
			break;

		case PROP_FRICTION_Y:
			entity_pointer[ENT_Y_VEL] *= ( ( ts[collision_tileset].tileset_pointer[tile_number_1].friction + ts[collision_tileset].tileset_pointer[tile_number_2].friction ) / 2);
			entity_pointer[ENT_Y_VEL] /= 100;
			break;
		}
	}

}



void WORLDCOLL_apply_tile_movement_property (int *entity_pointer, int property, int tile_number)
{
	switch (property)
	{
	case PROP_ACCELL_X:
		entity_pointer[ENT_EXTERNAL_ACCELL_X] = ts[collision_tileset].tileset_pointer[tile_number].accel_x;
		break;

	case PROP_ACCELL_Y:
		entity_pointer[ENT_EXTERNAL_ACCELL_Y] = ts[collision_tileset].tileset_pointer[tile_number].accel_y;
		break;

	case PROP_CONVEY_X:
		entity_pointer[ENT_EXTERNAL_CONVEY_X] = ts[collision_tileset].tileset_pointer[tile_number].convey_x;
		break;

	case PROP_CONVEY_Y:
		entity_pointer[ENT_EXTERNAL_CONVEY_Y] = ts[collision_tileset].tileset_pointer[tile_number].convey_y;
		break;

	case PROP_FRICTION_X:
		entity_pointer[ENT_X_VEL] *= ts[collision_tileset].tileset_pointer[tile_number].friction;
		entity_pointer[ENT_X_VEL] /= 100;
		break;

	case PROP_FRICTION_Y:
		entity_pointer[ENT_Y_VEL] *= ts[collision_tileset].tileset_pointer[tile_number].friction;
		entity_pointer[ENT_Y_VEL] /= 100;
		break;
	}

}



void WORLDCOLL_get_horizontal_zone_tile_extents (int zone_number, int *first, int *second)
{
	if (first == NULL || second == NULL)
	{
		return;
	}

	if (collision_map < 0 || collision_map >= number_of_tilemaps_loaded)
	{
		*first = 0;
		*second = 0;
		return;
	}

	if (cm[collision_map].zone_list_pointer == NULL || zone_number < 0 || zone_number >= cm[collision_map].zones)
	{
		*first = 0;
		*second = 0;
		return;
	}

	*first = cm[collision_map].zone_list_pointer[zone_number].x;
	*second = *first + cm[collision_map].zone_list_pointer[zone_number].width;
}



void WORLDCOLL_get_vertical_zone_tile_extents (int zone_number, int *first, int *second)
{
	if (first == NULL || second == NULL)
	{
		return;
	}

	if (collision_map < 0 || collision_map >= number_of_tilemaps_loaded)
	{
		*first = 0;
		*second = 0;
		return;
	}

	if (cm[collision_map].zone_list_pointer == NULL || zone_number < 0 || zone_number >= cm[collision_map].zones)
	{
		*first = 0;
		*second = 0;
		return;
	}

	*first = cm[collision_map].zone_list_pointer[zone_number].y;
	*second = *first + cm[collision_map].zone_list_pointer[zone_number].height;
}



void WORLDCOLL_get_top_left_zone_tile_co_ords (int zone_number, int *first, int *second)
{
	if (first == NULL || second == NULL)
	{
		return;
	}

	if (collision_map < 0 || collision_map >= number_of_tilemaps_loaded)
	{
		*first = 0;
		*second = 0;
		return;
	}

	if (cm[collision_map].zone_list_pointer == NULL || zone_number < 0 || zone_number >= cm[collision_map].zones)
	{
		*first = 0;
		*second = 0;
		return;
	}

	*first = cm[collision_map].zone_list_pointer[zone_number].x;
	*second = cm[collision_map].zone_list_pointer[zone_number].y;
}



void WORLDCOLL_get_horizontal_zone_extents (int zone_number, int *first, int *second)
{
	if (first == NULL || second == NULL)
	{
		return;
	}

	if (collision_map < 0 || collision_map >= number_of_tilemaps_loaded)
	{
		*first = 0;
		*second = 0;
		return;
	}

	if (cm[collision_map].zone_list_pointer == NULL || zone_number < 0 || zone_number >= cm[collision_map].zones)
	{
		*first = 0;
		*second = 0;
		return;
	}

	*first = cm[collision_map].zone_list_pointer[zone_number].real_x;
	*second = *first + cm[collision_map].zone_list_pointer[zone_number].real_width;
}



void WORLDCOLL_get_vertical_zone_extents (int zone_number, int *first, int *second)
{
	if (first == NULL || second == NULL)
	{
		return;
	}

	if (collision_map < 0 || collision_map >= number_of_tilemaps_loaded)
	{
		*first = 0;
		*second = 0;
		return;
	}

	if (cm[collision_map].zone_list_pointer == NULL || zone_number < 0 || zone_number >= cm[collision_map].zones)
	{
		*first = 0;
		*second = 0;
		return;
	}

	*first = cm[collision_map].zone_list_pointer[zone_number].real_y;
	*second = *first + cm[collision_map].zone_list_pointer[zone_number].real_height;
}



void WORLDCOLL_get_top_left_zone_co_ords (int zone_number, int *first, int *second)
{
	if (first == NULL || second == NULL)
	{
		return;
	}

	if (collision_map < 0 || collision_map >= number_of_tilemaps_loaded)
	{
		*first = 0;
		*second = 0;
		return;
	}

	if (cm[collision_map].zone_list_pointer == NULL || zone_number < 0 || zone_number >= cm[collision_map].zones)
	{
		*first = 0;
		*second = 0;
		return;
	}

	*first = cm[collision_map].zone_list_pointer[zone_number].real_x;
	*second = cm[collision_map].zone_list_pointer[zone_number].real_y;
}



void WORLDCOLL_set_flag (int zone_number,int flag_value)
{
	cm[collision_map].zone_list_pointer[zone_number].flag = flag_value;
}



int WORLDCOLL_get_flag (int zone_number)
{
	return (cm[collision_map].zone_list_pointer[zone_number].flag);
}
