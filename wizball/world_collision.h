#include "shared_collision_defines.h"

#ifndef _WORLD_COLLISION_H_
#define _WORLD_COLLISION_H_

void WORLDCOLL_copy_collision_layer (int tilemap_number, int from_layer, int to_layer, int overwrite_type);

void WORLDCOLL_generate_collision_maps (bool generate_optimisation_data , bool is_it_for_physics);
void WORLDCOLL_push_entity (int *entity_pointer);
void WORLDCOLL_setup_block_collision (int new_block_size);
void WORLDCOLL_set_collision_map (int tilemap_number);

int WORLDCOLL_test_snap_to_direction (int *entity_pointer , int direction , int max_distance, int big, int iterate, int snap_to_surface, int *returned_snap_depth=NULL);

int WORLDCOLL_get_solid_inner_tile_in_direction (int *entity_pointer, int direction , int test_distance, int reset_list);
int WORLDCOLL_get_solid_corner_tile_in_direction (int *entity_pointer , int direction , int test_distance, int reset_list);
int WORLDCOLL_get_solid_single_tile_in_direction (int *entity_pointer, int direction , int test_distance, int reset_list, int offset);
int WORLDCOLL_get_tile_at_offset (int *entity_pointer, int x_offset , int y_offset, int solid_or_not, int reset_list);

void WORLDCOLL_apply_tile_movement_property (int *entity_pointer, int property, int tile_number);
void WORLDCOLL_apply_dual_tile_movement_properties (int *entity_pointer, int property, int tile_number_1, int tile_number_2);
int WORLDCOLL_get_boolean_tile_properties (int tile_number);

int WORLDCOLL_tile_value (int layer, int x , int y);

void WORLDCOLL_destroy_block_collision_shapes (void);

bool WORLDCOLL_inside_collision (int *entity_pointer);

void WORLDCOLL_apply_attributes (int *entity_pointer, int attribute , int only_use_boolean);
int WORLDCOLL_get_aggregate_booleans (void);

void WORLDCOLL_set_map_collision_block (int layer, int start_x, int start_y, int end_x, int end_y, int equivalent_tile);
void WORLDCOLL_set_map_collision_block_real_co_ords (int layer, int start_x, int start_y, int end_x, int end_y, int equivalent_tile);

void WORLDCOLL_get_horizontal_zone_extents (int zone_number, int *first, int *second);
void WORLDCOLL_get_vertical_zone_extents (int zone_number, int *first, int *second);
void WORLDCOLL_get_top_left_zone_co_ords (int zone_number, int *first, int *second);

void WORLDCOLL_get_horizontal_zone_tile_extents (int zone_number, int *first, int *second);
void WORLDCOLL_get_vertical_zone_tile_extents (int zone_number, int *first, int *second);
void WORLDCOLL_get_top_left_zone_tile_co_ords (int zone_number, int *first, int *second);

int WORLDCOLL_find_first_zone_of_type (int zone_type);
int WORLDCOLL_find_next_zone_of_type (int current_zone);

void WORLDCOLL_get_collision_map_size_in_pixels (int *width,int *height);

void WORLDCOLL_set_flag (int zone_number,int flag_value);
int WORLDCOLL_get_flag (int zone_number);

#define CHECK_EDGE			0
#define CHECK_CORNERS		1
#define CHECK_POINT			2


// These are used so that, for instance, only the bottom-right corner of an object can collide with a particular tile.

#define INTERACTION_POINT_TOP				1
#define INTERACTION_POINT_TOP_RIGHT			2
#define INTERACTION_POINT_RIGHT				4
#define INTERACTION_POINT_BOTTOM_RIGHT		8
#define INTERACTION_POINT_BOTTOM			16
#define INTERACTION_POINT_BOTTOM_LEFT		32
#define INTERACTION_POINT_LEFT				64
#define INTERACTION_POINT_TOP_LEFT			128



#define NUMBER_OF_BLOCK_ANGLES			16

#define NORMAL_INDEX_LEFT		(12)
#define NORMAL_INDEX_RIGHT		(4)
#define NORMAL_INDEX_UP			(0)
#define NORMAL_INDEX_DOWN		(8)

#define BLOCKMODE_NONE		0
#define BLOCKMODE_SIMPLE	1
#define BLOCKMODE_COMPLEX	2

#define BLOCK_PROFILE_COUNT			34
#define BLOCK_PROFILE_SIDES			4
#define BLOCK_OFFSET_COUNT			2
#define BLOCK_VECTOR_MAX_POINTS		4

#define EXPOSURE_MAP_CARRY_ON		-1

#define PROP_ACCELL_X		(0)
#define PROP_ACCELL_Y		(1)
#define PROP_CONVEY_X		(2)
#define PROP_CONVEY_Y		(3)
#define PROP_FRICTION_X		(4)
#define PROP_FRICTION_Y		(5)

#define MAX_CHECK_POINTS		(128)

#define COLL_TYPE_SLIDING_HORIZONTAL										(1)
	// Use sliding collision when moving horizontally - more common as used for slopes.
#define COLL_TYPE_SLIDING_VERTICAL											(2)
	// Use sliding collision when moving vertically - less common.
#define COLL_TYPE_SECOND_AXIS_CONDITIONAL									(4)
	// Only bother moving on the second axis if the movement on the first axis was unimpeded.
#define COLL_TYPE_IGNORE_PARTIAL_OCCLUSION									(8)
	// If only one of the corners is in collision and the other has ignored collision, then assume no collision.
	// This is probably best set to on so you can't snag one foot when jumping through a slope or the like.
#define COLLISION_RESPONSE_SIMPLE_BOUNCE_OR_DECELLERATION					(16)
	// Either bounces or slows down on the axis it collided on.
#define COLLISION_RESPONSE_COMPLEX_BOUNCE									(32)
	// Uses the proper angle of reflection gleaned from the point of collision.
	// This should only really be used for very small objects.
#define COLLISION_RESPONSE_ROTATE											(64)
#define COLLISION_RESPONSE_CALL_SCRIPT										(128)
#define COLLISION_SPECIAL_USE_DEVIATION_TO_ADAPT_VELOCITY_HORIZONTAL		(256)
#define COLLISION_SPECIAL_USE_DEVIATION_TO_ADAPT_VELOCITY_VERTICAL			(512)
	// If this is set then movement in the other axis due to slopes, etc, will effect the velocity
	// in that axis. This allows for jumping off the end of platforms and the like but shouldn't be
	// used for things which can start/stop instantly but rather accellerate and decellerate, otherwise
	// they'll fly up into the air when they stop (although that's fine if they hit a wall).

	// These are all speedier shortcuts than from doing it via scripting, however if that extra
	// versatility is needed then setting the SCENERY_HITLINE will call the appropriate line
	// upon contact. This should really be reserved for Player entities or otherwise uncommon
	// entities.

	// Sound effects for contact between entities and blocks will be dealt with via a separate
	// system although this can be replaced or supplemented in scripts, obviously.
#define COLLISION_USE_EXTRA_TEST_POINTS										(1024)
	// This enables BIG collision on an entity which allows for objects larger than tiles to collide
	// correctly.

#define COLLISION_ITERATE_MOVEMENT											(2048)
	// This makes it check tile boundaries in the direction of travel so that it deals with slopes
	// in a really really solid manner. However if the game has solid lumps of tiles it won't be
	// necessary for an entity to employ this.

	// 1. Figure out how far the entity has moved in world-space by comparing the new world_x and the old one.
	// 2. Move it on an axis, getting a returned bitflag which says which axis it didn't go all the way on.
	// 3. Do the same for the other axis, ORing the results of the test.
	// 4. Update X position, if X has been corrupted due to hitting something then restore private scale co-ord
	//    from world scale one. Same for Y.

#define COLLISION_NOTICE_WHEN_INSIDE_COLLISION								(4096)
	// This means it doesn't discount it when you're inside collisions. This is really useful for
	// bullets which might be created at an offset from the player which puts them inside the collision,
	// and normally this would allow them to pass through solid objects.

#define COLLISION_HORIZONTAL_WORLD_EDGE_SOLID								(8192)
#define COLLISION_VERTICAL_WORLD_EDGE_SOLID									(16384)

extern int collision_map;

extern int collision_blocks_ignore_count;

#endif
