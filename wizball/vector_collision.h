#include "shared_collision_defines.h"

#ifndef _VECTOR_COLLISION_H_
#define _VECTOR_COLLISION_H_

typedef struct
{
	segment_list_struct *sides[5];
} vector_tile_profile_struct;

typedef struct
{
	int segment_count;
	int *seg_sx;
	int *seg_sy;
	int *seg_ex;
	int *seg_ey;
	int *seg_grad;
} segment_list_struct;






#endif
