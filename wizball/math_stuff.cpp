// This is a library of all my commonly used mathematical functions which I'll
// come back to time and time again I suspect.

#include <math.h>
#include <stdlib.h>
#include <assert.h>

#include "math_stuff.h"

#include "fortify.h"

int *sin_table = NULL;
int *cos_table = NULL;

int *asin_table = NULL;
int *acos_table = NULL;

int lerp_multiplier = 10000;

int angle_scalar;
int half_angle_scalar;
int arcangle_scalar;
int double_arcangle_scalar; // Just so we ain't using a multiple when we're bringing the angles back into whack.
float angle_conversion_ratio;

float percent_to_quarter_radian = 4.0f / PI;
float percent_to_half_radian = 2.0f / PI;

float matrices[NUMBER_OF_MATRICES][MATRIX_SIZE][MATRIX_SIZE];

#define MAX_RANDOM_NUMBER_STRANDS		(10)

int random_number[MAX_RANDOM_NUMBER_STRANDS];



float MATH_abs (float value)
{
	if (value<0.0f)
	{
		return -value;
	}
	else
	{
		return value;
	}
}



int MATH_abs_int (int value)
{
	if (value<0)
	{
		return -value;
	}
	else
	{
		return value;
	}
}



int MATH_sgn_float (float value)
{
	if (value<0)
	{
		return -1;
	}
	else if (value>0)
	{
		return 1;
	}
	else
	{
		return 0;
	}
}



int MATH_sgn_int (int value)
{
	if (value<0)
	{
		return -1;
	}
	else if (value>0)
	{
		return 1;
	}
	else
	{
		return 0;
	}
}



float MATH_round_to_nearest_float (float number, float round_to_nearest)
{
	int whole_number;
	
	number += MATH_sgn_float(number) * (round_to_nearest / 2); // So it rounds either way to the nearest one not just downward.
	
	whole_number = int (number / round_to_nearest); // How many of the little bits fit into the big bit as a whole number.

	number = whole_number * round_to_nearest;

	return number;
}



float MATH_round_to_lower_float (float number, float round_to_nearest)
{
	int whole_number;
	
	whole_number = int (number / round_to_nearest); // How many of the little bits fit into the big bit as a whole number.

	number = whole_number * round_to_nearest;

	return number;
}



float MATH_round_to_upper_float (float number, float round_to_nearest)
{
	int whole_number;

	number += MATH_sgn_float(number) * round_to_nearest;
	
	whole_number = int (number / round_to_nearest); // How many of the little bits fit into the big bit as a whole number.

	number = whole_number * round_to_nearest;

	return number;
}



int MATH_round_to_nearest_int (int number, int round_to_nearest)
{
	number += MATH_sgn_int(number) * (round_to_nearest / 2); // So it rounds either way to the nearest one not just downward.

	number -= (number % round_to_nearest);

	return number;
}



int MATH_round_to_lower_int (int number, int round_to_nearest)
{
	number -= (number % round_to_nearest);

	return number;
}



int MATH_round_to_upper_int (int number, int round_to_nearest)
{
	number += MATH_sgn_int(number) * (round_to_nearest);

	number -= (number % round_to_nearest);

	return number;
}



float MATH_distance_to_line (float x1, float y1, float x2, float y2, float px, float py , int *point_of_line , float *ratio_on_line)
{
	float v1x,v1y,v2x,v2y,v3x,v3y,temp,vtx,vty,dx,dy,dot_product,distance_to_line;

	v1x = px-x1; // Difference between point and start of line
	v1y = py-y1;

	v2x = x2-x1; // Difference between start and end of line.
	v2y = y2-y1;

	temp = float (sqrt ( (v2x * v2x) + (v2y * v2y) )); // Length of line

	v2x = v2x/temp; // Unit length of line.
	v2y = v2y/temp;
	
	dot_product = (v1x * v2x) + (v1y * v2y); // dot-product of the unit-length line vector and the vector to the point from the beginning of the line

	if (dot_product<=0) // it's actually the first point of the line that it's closest to...
	{
		vtx = x1;
		vty = y1;
		dx = px-vtx;
		dy = py-vty;
		distance_to_line = float (sqrt ((dx*dx)+(dy*dy)));
		*ratio_on_line = dot_product / temp;
		*point_of_line = BEFORE_LINE;
		return distance_to_line;
	}
	else if (dot_product>=temp) // it's the last point of the line it's closest to...
	{
		vtx = x2;
		vty = y2;
		dx = px-vtx;
		dy = py-vty;
		distance_to_line = float (sqrt ((dx*dx)+(dy*dy)));
		*ratio_on_line = dot_product / temp;
		*point_of_line = AFTER_LINE;
		return distance_to_line;
	}
	else // Nope, wait, it's closest to a point on the line... BUT WHERE?!
	{
		v3x = v2x * dot_product;
		v3y = v2y * dot_product;
		
		vtx = x1 + v3x;
		vty = y1 + v3y;
		
		dx = px-vtx;
		dy = py-vty;

		distance_to_line = float (sqrt ((dx*dx)+(dy*dy)));
		*ratio_on_line = dot_product / temp;
		*point_of_line = ON_LINE;

		return distance_to_line;
	}
}



void MATH_set_angle_arcangle_scalars (int new_arcangle, int new_angle)
{
	angle_scalar = new_angle;
	half_angle_scalar = angle_scalar / 2;
	arcangle_scalar = new_arcangle;
	double_arcangle_scalar = arcangle_scalar*2;
	angle_conversion_ratio = angle_scalar / float (2.0f * PI);
}



int MATH_diff_angle (int destination_angle, int current_angle)
{
	// Returns the difference between the given angle and the destination angle in
	// the angle_scalar scale. Wraps around properly.

	int diff = destination_angle - current_angle;

	if (diff > half_angle_scalar)
	{
		return (diff - angle_scalar);
	}
	else if (diff < -half_angle_scalar)
	{
		return (angle_scalar + diff);
	}
	else
	{
		return diff;
	}
}



void MATH_destroy_trig_tables (void)
{
	if (sin_table != NULL)
	{
		free (sin_table);
	}

	if (cos_table != NULL)
	{
		free (cos_table);
	}

	if (asin_table != NULL)
	{
		free (asin_table);
	}

	if (acos_table != NULL)
	{
		free (acos_table);
	}
}



void MATH_setup_sin_cos_tables (void)
{
	if (sin_table != NULL)
	{
		free (sin_table);
	}

	if (cos_table != NULL)
	{
		free (cos_table);
	}

	float angle;
	float slice;
	int t;

	slice = (2.0f * PI) / float (angle_scalar);

	angle = 0.0f;

	sin_table = (int *) malloc (angle_scalar * sizeof(int));

	if (sin_table == NULL)
	{
		assert(0);
	}

	cos_table = (int *) malloc (angle_scalar * sizeof(int));

	if (cos_table == NULL)
	{
		assert(0);
	}

	for (t=0; t<(angle_scalar); t++)
	{
		sin_table[t] = int (sin(angle) * float(arcangle_scalar));
		cos_table[t] = int (cos(angle) * float(arcangle_scalar));

		angle += slice;
	}

	// Because all values are rounded down, we now do a rounding pass.
	// Increasing or decreasing each value by one.

	for (t=0; t<(angle_scalar); t++)
	{
		if (sin_table[t] < 0)
		{
			sin_table[t]--;
		}
		else if (sin_table[t] > 0)
		{
			sin_table[t]++;
		}

		if (cos_table[t] < 0)
		{
			cos_table[t]--;
		}
		else if (cos_table[t] > 0)
		{
			cos_table[t]++;
		}
	}
}



void MATH_setup_asin_acos_tables (void)
{
	if (asin_table != NULL)
	{
		free (asin_table);
	}

	if (acos_table != NULL)
	{
		free (acos_table);
	}

	float slice;
	float value;
	int t;

	slice = 2.0f / float (arcangle_scalar*2);

	value = -1.0f;

	asin_table = (int *) malloc (arcangle_scalar * 2 * sizeof(int));

	if (sin_table == NULL)
	{
		assert(0);
	}

	acos_table = (int *) malloc (arcangle_scalar * 2 * sizeof(int));

	if (cos_table == NULL)
	{
		assert(0);
	}

	for (t=0; t<(arcangle_scalar*2); t++)
	{
		asin_table[t] = int (asin(value) * float (angle_conversion_ratio));
		acos_table[t] = int (acos(value) * float (angle_conversion_ratio));

		value += slice;
	}
}



int MATH_atan2 (int x , int y)
{
	double value = atan2 (y,x);

	return int (angle_conversion_ratio * value);
}




void MATH_setup_trig_tables (int new_arcangle, int new_angle)
{
	MATH_set_angle_arcangle_scalars (new_arcangle, new_angle);
	MATH_setup_sin_cos_tables ();
	MATH_setup_asin_acos_tables ();
}



int MATH_wrap_angle (int angle)
{
	while (angle >= angle_scalar)
	{
		angle -= angle_scalar;
	}

	while (angle < 0)
	{
		angle += angle_scalar;
	}

	return angle;
}



int MATH_get_sin_table_value (int angle)
{
	angle = MATH_wrap_angle (angle);

	return sin_table[angle];
}



int MATH_get_cos_table_value (int angle)
{
	angle = MATH_wrap_angle (angle);

	return cos_table[angle];
}



int MATH_wrap_arcangle (int arcangle)
{
	while (arcangle >= arcangle_scalar)
	{
		arcangle -= double_arcangle_scalar;
	}

	while (arcangle < -arcangle_scalar)
	{
		arcangle += double_arcangle_scalar;
	}

	return arcangle;
}



int MATH_get_asin_table_value (int arcangle)
{
	arcangle = MATH_wrap_arcangle (arcangle);

	return asin_table[arcangle + arcangle_scalar];
}



int MATH_get_acos_table_value (int arcangle)
{
	arcangle = MATH_wrap_arcangle (arcangle);

	return acos_table[arcangle + arcangle_scalar];
}



bool MATH_same_signs (float a, float b)
{
	if (MATH_sgn_float (a) != MATH_sgn_float (b))
	{
		return false;
	}
	else
	{
		return true;
	}
}



int MATH_intersect_lines (float x1,float y1,float x2,float y2,float x3,float y3,float x4,float y4,float *ix,float *iy)
{
	float x,y,a1,b1,c1,a2,b2,c2,r1,r2,r3,r4,denom,offset,num;

	a1 = y2 - y1;
	b1 = x1 - x2; // Vector from start to end of line, except mirrored.
	c1 = (x2 * y1) - (x1 * y2); 

	r3 = (a1 * x3) + (b1 * y3) + c1;
	r4 = (a1 * x4) + (b1 * y4) + c1;
	
	if ( ( r3 != 0 ) && (r4 != 0 ) )
	{
		if (MATH_same_signs ( r3, r4 ) == true)
		{
			return NO_INTERSECTION; // Don't intersect!
		}
	}

    a2 = y4 - y3;
    b2 = x3 - x4;
    c2 = (x4 * y3) - (x3 * y4);

    r1 = (a2 * x1) + (b2 * y1) + c2;
    r2 = (a2 * x2) + (b2 * y2) + c2;

	if ( ( r1 != 0 ) && (r2 != 0 ) )
	{
		if (MATH_same_signs ( r1, r2 ) == true)
		{
			return NO_INTERSECTION; // Don't intersect!
		}
	}

	denom = (a1 * b2) - (a2 * b1);

	if ( denom == 0 )
	{
		return CO_LINEAR; // That's posh talk for parallel. ;)
	}

	// line segments intersect, compute intersection point

	if (denom < 0)
	{
		offset = -denom / 2;
	}
	else
	{
		offset = denom / 2;
	}

	num = (b1 * c2) - (b2 * c1);
	
	if (num < 0)
	{
		x = (num - offset) / denom;
	}
	else
	{
		x = (num + offset) / denom;
	}	
	
	num = (a2 * c1) - (a1 * c2);
	
	if (num < 0)
	{
		y = (num - offset) / denom;
	}
	else
	{
		y = (num + offset) / denom;
	}
	
	*ix = x;
	*iy = y;
		
	return INTERSECTION;
}



float MATH_float_modulus (float total ,float modulus)
{
	// This is just like the % operators only it works with floats.

	int multiples = int (total / modulus);

	return (total - (float (multiples) * modulus));
}



void MATH_rotate_point_around_origin (float *x, float *y, float angle)
{
	float sin_value;
	float cos_value;

	sin_value = float(sin(angle));
	cos_value = float(cos(angle));

	*x = ( *x * cos_value ) + ( *y * sin_value );
	*y = ( -*x * sin_value ) + ( *y * cos_value );
}



bool MATH_line_to_line_intersect (int line_1_x1 , int line_1_y1 , int line_1_x2 , int line_1_y2 , int line_2_x1 , int line_2_y1 , int line_2_x2 , int line_2_y2 )
{
	int a1,b1,c1,a2,b2,c2,r1,r2,r3,r4,denom;

	a1 = line_1_y2 - line_1_y1;
	b1 = line_1_x1 - line_1_x2; // Vector from start to end of line, except mirrored.
	c1 = (line_1_x2 * line_1_y1) - (line_1_x1 * line_1_y2); // cross product

	r3 = (a1 * line_2_x1) + (b1 * line_2_y1) + c1;
	r4 = (a1 * line_2_x2) + (b1 * line_2_y2) + c1;
	
	if ( ( r3 != 0 ) && (r4 != 0 ) )
	{
		if (MATH_same_signs ( float(r3), float(r4) ) == true)
		{
			return false; // Don't intersect!
		}
	}

    a2 = line_2_y2 - line_2_y1;
    b2 = line_2_x1 - line_2_x2;
    c2 = (line_2_x2 * line_2_y1) - (line_2_x1 * line_2_y2); // cross product

    r1 = (a2 * line_1_x1) + (b2 * line_1_y1) + c2;
    r2 = (a2 * line_1_x2) + (b2 * line_1_y2) + c2;

	if ( ( r1 != 0 ) && (r2 != 0 ) )
	{
		if (MATH_same_signs ( float(r1), float(r2) ) == true)
		{
			return false; // Don't intersect!
		}
	}

	denom = (a1 * b2) - (a2 * b1); // cross product

	if ( denom == 0 )
	{
		return false; // That's parallel. ;)
	}
		
	return true;
}



int MATH_get_distance_int ( int x1, int y1, int x2, int y2)
{
	int dx,dy;

	dx = x1-x2;
	dy = y1-y2;

	return int (sqrt(double((dx*dx)+(dy*dy))));
}



float MATH_get_distance_float ( float x1, float y1, float x2, float y2)
{
	float dx,dy;

	dx = x1-x2;
	dy = y1-y2;

	return float (sqrt((dx*dx)+(dy*dy)));
}



float MATH_lerp (float a , float b , float p)
{
	// Returns the value which is p percent between a and b.
	return ( a + ( (b - a) * p ) );
}



float MATH_unlerp (float a , float b , float c)
{
	// Returns how far c is between a and b as a percentage.
	return ( (c-a) / (b-a) );
}



int MATH_pow (int power)
{
	// Returns 2 to the power of the supplied value.
	return (1<<power);
}



float MATH_bezier (float point_1, float control_1, float control_2 , float point_2, float percent)
{
	float temp[5];
	temp[0] = MATH_lerp ( point_1 , control_1 , percent );
	temp[1] = MATH_lerp ( control_1 , control_2 , percent );
	temp[2] = MATH_lerp ( control_2 , point_2 , percent );
	temp[3] = MATH_lerp ( temp[0] , temp[1] , percent );
	temp[4] = MATH_lerp ( temp[1] , temp[2] , percent );
	return (MATH_lerp ( temp[3] , temp[4] , percent ));
}



float MATH_largest (float a, float b)
{
	return (a>b) ? a : b;
}



float MATH_smallest (float a, float b)
{
	return (a<b) ? a : b;
}



int MATH_largest_int (int a, int b)
{
	return (a>b) ? a : b;
}



int MATH_smallest_int (int a, int b)
{
	return (a<b) ? a : b;
}



int MATH_get_rough_direction ( int x1, int y1, int x2, int y2 )
{
	// Quite simply this just returns a value saying whether x2,y2 is north, south, east of west of x1,y1.
	// It'll be for enemies that are invulnerable from certain directions mainly.

	int diff_x, diff_y;

	diff_x = x2-x1;
	diff_y = y2-y1;

	if ( (diff_x - diff_y) > 0 )
	{
		// The point is either NORTH or EAST.
		if ( ((diff_x * -1) - diff_y) < 0 )
		{
			return FOUR_BIT_RIGHT;
		}
		else
		{
			return FOUR_BIT_TOP;
		}
	}
	else
	{
		// The point is either SOUTH or WEST.
		if ( ((diff_x * -1) - diff_y) < 0 )
		{
			return FOUR_BIT_BOTTOM;
		}
		else
		{
			return FOUR_BIT_LEFT;
		}	
	}
}



int MATH_get_up_right_down_left ( int x1, int y1, int x2, int y2 )
{
	// Quite simply this just returns a value saying whether x2,y2 is north, south, east of west of x1,y1.
	// It'll be for enemies that are invulnerable from certain directions mainly.

	int diff_x, diff_y;

	diff_x = x2-x1;
	diff_y = y2-y1;

	if ( (diff_x - diff_y) > 0 )
	{
		// The point is either UP or RIGHT.
		if ( ((diff_x * -1) - diff_y) < 0 )
		{
			return FOUR_NUMERICAL_RIGHT;
		}
		else
		{
			return FOUR_NUMERICAL_TOP;
		}
	}
	else
	{
		// The point is either DOWN or LEFT.
		if ( ((diff_x * -1) - diff_y) < 0 )
		{
			return FOUR_NUMERICAL_BOTTOM;
		}
		else
		{
			return FOUR_NUMERICAL_LEFT;
		}	
	}
}


#define random_number_multiplier		16807 // multiplier
#define random_long_mask				2147483647 // 2 to the power of 31 minus 1
//#define random_mask_div_multiplier		127773L // random_long_mask / random_number_multiplier
//#define random_number_remainder			2836 // random_long_mask % random_number_multiplier


int MATH_get_next_long_random_number (int seed_number)
{
	unsigned int low, high;

	low = random_number_multiplier * (int) (seed_number & 0xFFFF);
	high = random_number_multiplier * (int) ((unsigned int) seed_number >> 16);
	low += (high & 0x7FFF) << 16;

	if (low > random_long_mask)
	{
		low &= random_long_mask;
		low++;
	}

	low += (high >> 15);

	if (low > random_long_mask)
	{
		low &= random_long_mask;
		low++;
	}
	
	return (int)low;
}



long MATH_get_next_special_rand (int seed_index)
{
    random_number[seed_index] = MATH_get_next_long_random_number (random_number[seed_index]);
    return random_number[seed_index];
}



void MATH_seed_special_rand (int seed_index, unsigned int seed_number)
{
	seed_number &= random_long_mask;

	if (seed_number > 0)
	{
		random_number[seed_index] = seed_number;
	}
	else
	{
		// Cannot have a zero seed number. Small price to pay, really.
		random_number[seed_index] = 1;
	}
}



int MATH_special_rand (int seed_index, int lowest, int highest)
{
	if (lowest > highest)
	{
		int temp = lowest;
		lowest = highest;
		highest = temp;
	}
	else if (lowest == highest)
	{
		return lowest;
	}

	int diff = highest - lowest;
	int temp_val = MATH_get_next_special_rand(seed_index) + 1; // So it's never 0 else we're fucked.

	while (temp_val < diff)
	{
		temp_val *= 10;
	}

	return ((temp_val % diff) + lowest);
}



int MATH_rand (int lowest, int highest)
{
	if (lowest > highest)
	{
		int temp = lowest;
		lowest = highest;
		highest = temp;
	}
	else if (lowest == highest)
	{
		return lowest;
	}

	int diff = highest - lowest;
	int temp_val = rand() + 1; // So it's never 0 else we're fucked.

	while (temp_val < diff)
	{
		temp_val *= 10;
	}

	return ((temp_val % diff) + lowest);
}



void MATH_srand (unsigned int seed)
{
	srand(seed);
}



int MATH_fold (int number, int start, int end)
{
	// Keeps a number between the start and end by increasing or decreasing it by
	// (end-start) until it fits within the bounds.

	int difference;

	difference = end-start;

	while(number<start)
	{
		number += difference;
	}

	while(number>=end)
	{
		number -= difference;
	}

	return number;

}



int MATH_wrap (int value , int wrap)
{
	// It's just like modulus, only it works for negative numbers properly (it doesn't reflect them).

	int times;

	if (wrap>0)
	{
		if (value < 0)
		{
			times = (abs(value) / wrap) + 1;
			value += (times * wrap);
		}

		value = value % wrap;

		return value;
	}
	else
	{
		return 0;
	}
}



float MATH_constrain_float (float number, float start, float end)
{
	// Returns the number unless it's outside of start to end
	// in which case it returns either of those two.

	if (number < start)
	{
		return start;
	}
	else if (number > end)
	{
		return end;
	}
	else
	{
		return number;
	}
}



int MATH_constrain (int number, int start, int end)
{
	// Returns the number unless it's outside of start to end
	// in which case it returns either of those two.

	if (number < start)
	{
		return start;
	}
	else if (number > end)
	{
		return end;
	}
	else
	{
		return number;
	}
}



void MATH_make_rectangle ( rectangle *r , int x, int y, int upper_width , int upper_height , int lower_width , int lower_height , float angle )
{
	
	r->angle = angle;

	r->pivot_x = x;
	r->pivot_y = y;

	r->co_ords[0][X_CO_ORD] = -upper_width;
	r->co_ords[0][Y_CO_ORD] = -upper_height;

	r->co_ords[1][X_CO_ORD] = lower_width;
	r->co_ords[1][Y_CO_ORD] = -upper_height;

	r->co_ords[2][X_CO_ORD] = lower_width;
	r->co_ords[2][Y_CO_ORD] = lower_height;

	r->co_ords[3][X_CO_ORD] = -upper_width;
	r->co_ords[3][Y_CO_ORD] = lower_height;

}



void MATH_rotate_and_translate_rect ( rectangle *r )
{
	// Assumes that co-ordinates are supplied already around the origin.

	int i;
	r->sin_angle = float (sin(r->angle));
	r->cos_angle = float (cos(r->angle));
	float temp_x, temp_y;
	int old_x,old_y;
	
	for (i=0 ; i<NUM_CO_ORDS ; i++)
	{
		old_x = r->co_ords[i][X_CO_ORD];
		old_y = r->co_ords[i][Y_CO_ORD];
		temp_x = ( old_x * r->cos_angle ) + ( old_y * r->sin_angle );
		temp_y = ( -old_x * r->sin_angle ) + ( old_y * r->cos_angle );
		r->co_ords[i][X_CO_ORD] = int (temp_x + r->pivot_x);
		r->co_ords[i][Y_CO_ORD] = int (temp_y + r->pivot_y);
	}

}



void MATH_find_rotated_rectangle_extents ( rectangle *r , int *rect_x1 , int *rect_y1 , int *rect_x2 , int *rect_y2 )
{
	// Find the axis-aligned bounding box required to fit a rotated rectangle. By
	// looking at the first two pairs of co-ordinates in each rectangle and knowing that
	// that the co-ordinates are supplied in a clockwise configuration we can easily
	// figure out their new relative positions.

	if (r->co_ords[0][X_CO_ORD] < r->co_ords[1][X_CO_ORD])
	{
		// the first co-ord is to the left of the second co-ord, which means it's on one of
		// the sides which point upwards.
		if (r->co_ords[0][Y_CO_ORD] < r->co_ords[1][Y_CO_ORD])
		{
			// the first co-ord is above the second co-ord, which means that it's the side
			// facing up-right, so knowing that we can say that:
			*rect_x1 = r->co_ords[3][X_CO_ORD];
			*rect_y1 = r->co_ords[0][Y_CO_ORD];
			*rect_x2 = r->co_ords[1][X_CO_ORD];
			*rect_y2 = r->co_ords[2][Y_CO_ORD];
		}
		else
		{
			// the first co-ord is below the second co-ord, which means that it's the side
			// facing up-left, so knowing that we can say that:
			*rect_x1 = r->co_ords[0][X_CO_ORD];
			*rect_y1 = r->co_ords[1][Y_CO_ORD];
			*rect_x2 = r->co_ords[2][X_CO_ORD];
			*rect_y2 = r->co_ords[3][Y_CO_ORD];
		}
	}
	else
	{
		// the first co-ord is to the right of the second co-ord, which means it's on one of
		// the sides which point downwards.
		if (r->co_ords[0][Y_CO_ORD] < r->co_ords[1][Y_CO_ORD])
		{
			// the first co-ord is above the second co-ord, which means that it's the side
			// facing down-right, so knowing that we can say that:
			*rect_x1 = r->co_ords[2][X_CO_ORD];
			*rect_y1 = r->co_ords[3][Y_CO_ORD];
			*rect_x2 = r->co_ords[0][X_CO_ORD];
			*rect_y2 = r->co_ords[1][Y_CO_ORD];
		}
		else
		{
			// the first co-ord is below the second co-ord, which means that it's the side
			// facing down-left, so knowing that we can say that:
			*rect_x1 = r->co_ords[1][X_CO_ORD];
			*rect_y1 = r->co_ords[2][Y_CO_ORD];
			*rect_x2 = r->co_ords[3][X_CO_ORD];
			*rect_y2 = r->co_ords[0][Y_CO_ORD];
		}
	}

}



bool MATH_rotated_rectangle_bounding_box_check ( rectangle *r1 , rectangle *r2 )
{


	int rect_1_x1,rect_1_y1,rect_1_x2,rect_1_y2;
	int rect_2_x1,rect_2_y1,rect_2_x2,rect_2_y2;
	
	MATH_find_rotated_rectangle_extents ( r1 , &rect_1_x1 , &rect_1_y1 , &rect_1_x2 , &rect_1_y2 );
	MATH_find_rotated_rectangle_extents ( r2 , &rect_2_x1 , &rect_2_y1 , &rect_2_x2 , &rect_2_y2 );

	return MATH_rectangle_to_rectangle_intersect (rect_1_x1 , rect_1_y1 , rect_1_x2 , rect_1_y2 , rect_2_x1 , rect_2_y1 , rect_2_x2 , rect_2_y2);
	
}



bool MATH_rotated_rectangle_to_point_intersect ( rectangle *r , int px, int py )
{
	// Righty, this uses lovely cross-products which are among my favourite things in vector maths.
	// The main reason it's used is because it gives you a vector perpendicular to the two supplied vectors,
	// but the upshot of this is that you can use it's sign to say which side of a vector another vector
	// falls on (ie, clockwise or anti-clockwise).

	// So if we want to find if a point is within a convex shape, you just find the dot product of the vector
	// of each line which makes up the shape with a vector from the start of that line to the point. If the
	// signs of all results are the same, it's within the shape. Because all my lines are clockwise it means
	// I'm looking for +ve or 0 results. A result of 0 means that the point lies on the line itself (although
	// not necessarily within the scope (length) of the line).

	// This check will *only* work for CONVEX shapes (ie, no divots in them).

	int result = 0;
	int t;
	int p1;
	int diff_line_x,diff_line_y,diff_point_x,diff_point_y;

	for (t=0; t<NUM_CO_ORDS ;t++)
	{
		p1 = (t != NUM_CO_ORDS-1) ? t+1 : 0;

		diff_line_x = r->co_ords[p1][X_CO_ORD] - r->co_ords[t][X_CO_ORD];
		diff_line_y = r->co_ords[p1][Y_CO_ORD] - r->co_ords[t][Y_CO_ORD];

		diff_point_x = px - r->co_ords[t][X_CO_ORD];
		diff_point_y = py - r->co_ords[t][Y_CO_ORD];

		result += (MATH_sgn_int ( (diff_line_x * diff_point_y) - (diff_point_x * diff_line_y) ) >= 0);

		if (result != t+1) // ie, the first time it doesn't match it'll quit out to save doing the other checks.
		{
			return false;
		}
	}

	return true; // Although it should never get here.
}



bool MATH_rotated_rectangle_to_rotated_rectangle_intersect ( rectangle *r1 , rectangle *r2 )
{
	// This function assumes you've already rotated and translated the co-ordinates first.
	// Note that you should probably put in a standard axis-aligned rects overlap check
	// first if you are generally expecting a failure from this check (ie, you haven't
	// optimised the basic collision framework to exclude distant objects). I've included
	// one here for reference although really you shouldn't have need of it.

	if ( MATH_rotated_rectangle_bounding_box_check ( r1 , r2 ) == false )
	{
		return false;
	}

	// Now we can get to the proper part of it. At this level the check is very simple, we
	// just check to see if any of the points which make up r1 are inside r2. If any are
	// then we return true, after that we check to see whether any points of r2 is inside
	// r1 (for those situations where r1 completely swallows r2 without any intersection of
	// their lines) and if that fails, there's one last check that needs doing in the case of
	// when the bodies of the rectangles intersect but none of the points (imagine a cross made of
	// two long thin rectangles) - in these situations we must go for an intersecting line
	// algorithm from the opposite corners of each rectangle.

	// There's probably a far better way of doing this, y'know... Still, it does the job
	// and since there's nothing but multiplies, adds and subtracts it really shouldn't
	// be terribly slow aside from the obvious repetition.

	int t;

	for (t=0; t<NUM_CO_ORDS ; t++)
	{
		if (MATH_rotated_rectangle_to_point_intersect ( r2 , r1->co_ords[t][X_CO_ORD], r1->co_ords[t][Y_CO_ORD] ) == true)
		{
			return true;
		}
	}

	for (t=0; t<NUM_CO_ORDS ; t++)
	{
		if (MATH_rotated_rectangle_to_point_intersect ( r1 , r2->co_ords[t][X_CO_ORD], r2->co_ords[t][Y_CO_ORD] ) == true)
		{
			return true;
		}
	}

	if (MATH_line_to_line_intersect ( r1->co_ords[0][X_CO_ORD] , r1->co_ords[0][Y_CO_ORD] , r1->co_ords[2][X_CO_ORD] , r1->co_ords[2][Y_CO_ORD] , r2->co_ords[0][X_CO_ORD] , r2->co_ords[0][Y_CO_ORD] , r2->co_ords[2][X_CO_ORD] , r2->co_ords[2][Y_CO_ORD] ) == true)
	{
		return true;
	}

	return false;

}




bool MATH_rotated_rectangle_to_rectangle_intersect ( rectangle *r1 , int rect_x1 , int rect_y1 , int rect_x2 , int rect_y2 )
{
	// For ease of use we may as well just treat this as a regular rotated rectangle intersect. I doubt
	// the maths would be any simpler if we were do it another way.

	rectangle r2;

	r2.angle = 0;
	r2.cos_angle = 1;
	r2.sin_angle = 0;

	r2.co_ords[0][X_CO_ORD] = rect_x1;
	r2.co_ords[0][Y_CO_ORD] = rect_y1;

	r2.co_ords[1][X_CO_ORD] = rect_x2;
	r2.co_ords[1][Y_CO_ORD] = rect_y1;

	r2.co_ords[2][X_CO_ORD] = rect_x2;
	r2.co_ords[2][Y_CO_ORD] = rect_y2;

	r2.co_ords[3][X_CO_ORD] = rect_x1;
	r2.co_ords[3][Y_CO_ORD] = rect_y2;

	return (MATH_rotated_rectangle_to_rotated_rectangle_intersect ( r1 , &r2 ) );
}



bool MATH_rotated_rectangle_to_circle_intersect (rectangle *r , int px , int py , int radius )
{
	// Okay, well the first part is easy enough as we just do a point in rotated rectangle
	// check to see if the center of the circle is within the rect.

	if (MATH_rotated_rectangle_to_point_intersect ( r , px, py ) == true)
	{
		return true;
	}

	// After that we need to check whether the point is within "radius" of the line, to do this
	// we first identify where the point is in relation to the rectangle by finding the sign of
	// the cross product of one of the lines and the line to the point from the start of the line,
	// if the point is on the outside of the line it will give a negative value (due to the points
	// being clockwise in order). If we only get one negative value then the point is closest to
	// the body of one of the lines but if we get two negative vales it means the point is closest
	// to one of the corners.

	int t,p;
	int total = 0;
	int result[NUM_CO_ORDS];
	float norm_x,norm_y;

	int diff_line_x,diff_line_y,diff_point_x,diff_point_y;

	for (t=0; t<NUM_CO_ORDS ;t++)
	{
		p = (t != NUM_CO_ORDS-1) ? t+1 : 0;

		diff_line_x = r->co_ords[p][X_CO_ORD] - r->co_ords[t][X_CO_ORD];
		diff_line_y = r->co_ords[p][Y_CO_ORD] - r->co_ords[t][Y_CO_ORD];

		diff_point_x = px - r->co_ords[t][X_CO_ORD];
		diff_point_y = py - r->co_ords[t][Y_CO_ORD];

		result[t] = MATH_sgn_int ( (diff_line_x * diff_point_y) - (diff_point_x * diff_line_y) ); // ie, if the sign of the cross-product is negative or zero it's either on this line or outside it so check distance.
		total += (result[t] <= 0);
	}

	if (total == 1) // Only one result was negative, so it's by a line segmentbody.
	{
		// How about creating a dummy line which is parellel to the side it's nearest but shifted
		// over by "radius", then if the point is on this side of that line it's within radius and
		// bob's your uncle! All we need to do is move first point over by radius * sin_angle (or whatever)
		// and cos_angle and bob's yer uncle!
		if (result[0] <= 0) // it's closest to the first line section.
		{
			// The first line section was originally oriented leading to the right.
			norm_x = -r->sin_angle;
			norm_y = -r->cos_angle;
			diff_line_x = r->co_ords[1][X_CO_ORD] - r->co_ords[0][X_CO_ORD];
			diff_line_y = r->co_ords[1][Y_CO_ORD] - r->co_ords[0][Y_CO_ORD];
			diff_point_x = px - (r->co_ords[0][X_CO_ORD] + int (norm_x * radius));
			diff_point_y = py - (r->co_ords[0][Y_CO_ORD] + int (norm_y * radius));
		}
		else if (result[1] <= 0) // it's closest to the second line section.
		{
			norm_x = r->cos_angle;
			norm_y = -r->sin_angle;
			diff_line_x = r->co_ords[2][X_CO_ORD] - r->co_ords[1][X_CO_ORD];
			diff_line_y = r->co_ords[2][Y_CO_ORD] - r->co_ords[1][Y_CO_ORD];
			diff_point_x = px - (r->co_ords[1][X_CO_ORD] + int (norm_x * radius));
			diff_point_y = py - (r->co_ords[1][Y_CO_ORD] + int (norm_y * radius));
		}
		else if (result[2] <= 0) // it's closest to the third line section.
		{
			norm_x = r->sin_angle;
			norm_y = r->cos_angle;
			diff_line_x = r->co_ords[3][X_CO_ORD] - r->co_ords[2][X_CO_ORD];
			diff_line_y = r->co_ords[3][Y_CO_ORD] - r->co_ords[2][Y_CO_ORD];
			diff_point_x = px - (r->co_ords[2][X_CO_ORD] + int (norm_x * radius));
			diff_point_y = py - (r->co_ords[2][Y_CO_ORD] + int (norm_y * radius));
		}
		else // it's closest to the fourth line section.
		{
			norm_x = -r->cos_angle;
			norm_y = r->sin_angle;
			diff_line_x = r->co_ords[0][X_CO_ORD] - r->co_ords[3][X_CO_ORD];
			diff_line_y = r->co_ords[0][Y_CO_ORD] - r->co_ords[3][Y_CO_ORD];
			diff_point_x = px - (r->co_ords[3][X_CO_ORD] + int (norm_x * radius));
			diff_point_y = py - (r->co_ords[3][Y_CO_ORD] + int (norm_y * radius));
		}

		if ( MATH_sgn_int ( (diff_line_x * diff_point_y) - (diff_point_x * diff_line_y) ) >= 0 )
		{
			return true;
		}
	}
	else // It was outside two lines therefor closest to a corner - nice and easy to deal with.
	{
		if ( (result[0] <= 0) && (result[1] <= 0) )
		{
			return ( MATH_circle_to_point_intersect ( r->co_ords[1][X_CO_ORD], r->co_ords[1][Y_CO_ORD], radius , px , py ) );
		}
		else if ( (result[1] <= 0) && (result[2] <= 0) )
		{
			return ( MATH_circle_to_point_intersect ( r->co_ords[2][X_CO_ORD], r->co_ords[2][Y_CO_ORD], radius , px , py ) );
		}
		else if ( (result[2] <= 0) && (result[3] <= 0) )
		{
			return ( MATH_circle_to_point_intersect ( r->co_ords[3][X_CO_ORD], r->co_ords[3][Y_CO_ORD], radius , px , py ) );
		}
		else
		{
			return ( MATH_circle_to_point_intersect ( r->co_ords[0][X_CO_ORD], r->co_ords[0][Y_CO_ORD], radius , px , py ) );
		}
	}

	return false;

}



bool MATH_rectangle_to_rectangle_intersect (int rect_1_x1 , int rect_1_y1 , int rect_1_x2 , int rect_1_y2 , int rect_2_x1 , int rect_2_y1 , int rect_2_x2 , int rect_2_y2)
{
	// Note that x1,y1 is the top-left of the rectangle and x2,y2 is the bottom right!
	// If you pass them the wrong way around it won't work.

	if ( (rect_1_x2 < rect_2_x1) || (rect_1_y2 < rect_2_y1) || (rect_2_x2 < rect_1_x1) || (rect_2_y2 < rect_1_y1) )
	{
		return false;
	}
	else
	{
		return true;
	}
}



bool MATH_rectangle_to_circle_intersect ( int rect_x1 , int rect_y1 , int rect_x2 , int rect_y2 , int circle_x , int circle_y , int radius )
{
	// This is a simple enough test although it's split into a few stages. First of all we do a simple point in rect
	// check and return true if it succeeds.

	if ( MATH_rectangle_to_point_intersect ( rect_x1 , rect_y1 , rect_x2 , rect_y2, circle_x, circle_y) == true )
	{
		return true;
	}
	else
	{
		// If it isn't inside the rectangle then it's either closest to a side or a corner. So first we check whether the
		// point is within the scope of one of the sides... Okay, so this ain't the fastest check in the world, but it's
		// solid and I can't think of a better way of doing it.
		if ( (circle_x >= rect_x1) && (circle_x <= rect_x2) )
		{
			// It's within the horizontal scope.
			if (circle_y < rect_y1)
			{
				// It's above the rectangle.
				if (rect_y1 - circle_y <= radius)
				{
					return true;
				}
				else
				{
					return false;
				}
			}
			else
			{
				// It's below the rectangle.
				if (circle_y - rect_y2 <= radius)
				{
					return true;
				}
				else
				{
					return false;
				}
			}
		}
		else if ( (circle_y >= rect_y1) && (circle_y <= rect_y2) )
		{
			// It's within the vertical scope.
			if (circle_x < rect_x1)
			{
				// It's to the left of the rectangle.
				if (rect_x1 - circle_x <= radius)
				{
					return true;
				}
				else
				{
					return false;
				}
			}
			else
			{
				// It's to the right of the rectangle.
				if (circle_x - rect_x2 <= radius)
				{
					return true;
				}
				else
				{
					return false;
				}
			}
		}
		else if ( (circle_x < rect_x1) && (circle_y < rect_y1) )
		{
			// It's up-left of the rectangle.
			return ( MATH_circle_to_point_intersect ( rect_x1, rect_y1, radius , circle_x , circle_y ) );
		}
		else if ( (circle_x > rect_x2) && (circle_y < rect_y1) )
		{
			// It's up-right of the rectangle.
			return ( MATH_circle_to_point_intersect ( rect_x2, rect_y1, radius , circle_x , circle_y ) );
		}
		else if ( (circle_x < rect_x1) && (circle_y > rect_y2) )
		{
			// It's down-left of the rectangle.
			return ( MATH_circle_to_point_intersect ( rect_x1, rect_y2, radius , circle_x , circle_y ) );
		}
		else if ( (circle_x > rect_x2) && (circle_y > rect_y2) )
		{
			// It's down-right of the rectangle.
			return ( MATH_circle_to_point_intersect ( rect_x2, rect_y2, radius , circle_x , circle_y ) );
		}
		else
		{
			// WHERE THE HELL IS IT THEN?!!
			assert (0);
			return false; // SHOULD NEVER GET HERE!
		}

	}

}



bool MATH_rectangle_to_point_intersect (int rect_x1 , int rect_y1 , int rect_x2 , int rect_y2 , int px, int py)
{
	if ( (px<rect_x1) || (py<rect_y1) || (px>rect_x2) || (py>rect_y2) )
	{
		return false;
	}
	else
	{
		return true;
	}
}



bool MATH_circle_to_circle_intersect ( int x1 , int  y1 , int  r1 , int  x2 , int  y2 , int  r2 )
{

	// Obviously this is good ol' pythag but with the sqrt's squared out.

	if ( (x1-x2) * (x1-x2) + (y1-y2) * (y1-y2) < (r1 + r2) * (r1 + r2) )
	{
		return true;
	}
	else
	{
		return false;
	}

}



bool MATH_circle_to_point_intersect ( int x1 , int  y1 , int  r1 , int  x2 , int  y2 )
{

	// Obviously this is good ol' pythag but with the sqrt's squared out.

	if ( ( (x1-x2) * (x1-x2) ) + ( (y1-y2) * (y1-y2) ) < (r1 * r1) )
	{
		return true;
	}
	else
	{
		return false;
	}

}



float MATH_get_catmull_rom_spline_point (float p0, float p1, float p2, float p3, float t)
{
	// Returns the point interpolated between p1 and p2 where t is 0-1.

	float t2,t3,result;

	t2 = t*t;
	t3 = t2*t;

	result = 0.5f * ( ((-p0 + 3*p1 -3*p2 + p3)*t3) + ((2*p0 -5*p1 + 4*p2 - p3)*t2) + ((-p0+p2)*t) + (2*p1) );

	return result;
}



float MATH_get_spline_length (float x0, float y0, float x1, float y1, float x2, float y2, float x3, float y3)
{
	// You give it a series of points and it figures out the distance between x1,y1 and x2,y2

	float t;
	float result_x1,result_y1,result_x2=x1,result_y2=y1;
	float total_length = 0;

	for (t=SAMPLING_FREQUENCY; t<=1 ; t+=SAMPLING_FREQUENCY)
	{
		result_x1 = result_x2;
		result_y1 = result_y2;

		result_x2 = MATH_get_catmull_rom_spline_point (x0, x1, x2, x3, t);
		result_y2 = MATH_get_catmull_rom_spline_point (y0, y1, y2, y3, t);

		total_length += MATH_get_distance_float ( result_x1, result_y1, result_x2, result_y2);
	}

	return total_length;
}



bool MATH_rectangle_to_point_intersect_by_size ( int rect_x , int rect_y , int width , int height , int px, int py )
{
	return ( MATH_rectangle_to_point_intersect (rect_x,rect_y,rect_x+width,rect_y+height,px,py) );
}



float MATH_smallest_float (float f1, float f2)
{
	if (f1<f2)
	{
		return f1;
	}
	else
	{
		return f2;
	}

}



float MATH_largest_float (float f1, float f2)
{
	if (f1>f2)
	{
		return f1;
	}
	else
	{
		return f2;
	}

}



void MATH_convert_rgb_to_hsv ( int red, int green, int blue, int *hue, int *saturation, int *value )
{

	float r = float (red) / 255.0f;
	float g = float (green) / 255.0f;
	float b = float (blue) / 255.0f;
	
	float h = 0.0f;
	float s;
	
	float min = 0.0f;
	float max = 0.0f;
	float delta = 0.0f;

	min = MATH_smallest_float ( MATH_smallest_float (r,g) , b );

	max = MATH_largest_float ( MATH_largest_float (r,g) , b );

	float v = max;
	delta = max - min;

	if ( (max>0) && (delta>0) )
	{
		s = delta / max;
	}
	else
	{
		s = 0;
		h = -1;

		*hue = int(h);
		*saturation = int(255 * s);
		*value = int(255 * v);

		return;
	}
	
	if (r == max)
	{
		h = (g-b)/delta;
	}
	else if (g == max)
	{
		h = 2 + (b-r) / delta;
	}
	else
	{
		h = 4 + (r-g) / delta;
	}

	h = h * 60;
	
	if (h<0)
	{
		h+=360;
	}

	*hue = int(h);
	*saturation = int(255 * s);
	*value = int(255 * v);
}




void MATH_convert_hsv_to_rgb ( int hue, int saturation, int value, int *red, int *green, int *blue )
{

	float h,s,v,i;
	float f,p,q,t;

	float r,g,b;

	s = float (saturation) / 255.0f;
	v = float (value) / 255.0f;

	f = 0.0f;
	p = 0.0f;
	q = 0.0f;
	t = 0.0f;

	if (s == 0.0f)
	{
		// If there's no saturation, it's just grey of the given value;

		*red = value;
		*green = value;
		*blue = value;
		return;
	}

	h = float (hue) / 60.0f;
	i = float (h);

	f = h - i;
	p = v * (1-s);
	q = v * (1-s*f);
	t = v * (1-s*(1-f));
	
	switch (int (i))
	{
	case 0:
		r = v;
		g = t;
		b = p;
		break;

	case 1:
		r = q;
		g = v;
		b = p;
		break;

	case 2:
		r = p;
		g = v;
		b = t;
		break;

	case 3:
		r = p;
		g = q;
		b = v;
		break;

	case 4:
		r = t;
		g = p;
		b = v;
		break;

	case 5:
		r = v;
		g = p;
		b = q;
		break;

	default:
		assert(0);
		break;
	}

	*red = int (255.0f * r);
	*green = int (255.0f * g);
	*blue = int (255.0f * b);
}



void MATH_get_4bit_offsets (int direction_bitvalue, int *x_offset, int *y_offset)
{

	switch (direction_bitvalue)
	{
	case FOUR_BIT_TOP:
		*x_offset = 0;
		*y_offset = -1;
		break;
	case FOUR_BIT_RIGHT:
		*x_offset = 1;
		*y_offset = 0;
		break;
	case FOUR_BIT_BOTTOM:
		*x_offset = 0;
		*y_offset = 1;
		break;
	case FOUR_BIT_LEFT:
		*x_offset = -1;
		*y_offset = 0;
		break;

	default:
		assert(0);
		break;
	}

}



void MATH_get_4num_offsets (int direction_numvalue, int *x_offset, int *y_offset)
{
	direction_numvalue = MATH_pow(direction_numvalue);
	MATH_get_4bit_offsets (direction_numvalue, x_offset, y_offset);
}



void MATH_get_8bit_offsets (int direction_bitvalue, int *x_offset, int *y_offset)
{

	switch (direction_bitvalue)
	{
	case EIGHT_BIT_TOP:
		*x_offset = 0;
		*y_offset = -1;
		break;
	case EIGHT_BIT_TOP_RIGHT:
		*x_offset = 1;
		*y_offset = -1;
		break;
	case EIGHT_BIT_RIGHT:
		*x_offset = 1;
		*y_offset = 0;
		break;
	case EIGHT_BIT_BOTTOM_RIGHT:
		*x_offset = 1;
		*y_offset = 1;
		break;
	case EIGHT_BIT_BOTTOM:
		*x_offset = 0;
		*y_offset = 1;
		break;
	case EIGHT_BIT_BOTTOM_LEFT:
		*x_offset = -1;
		*y_offset = 1;
		break;
	case EIGHT_BIT_LEFT:
		*x_offset = -1;
		*y_offset = 0;
		break;
	case EIGHT_BIT_TOP_LEFT:
		*x_offset = -1;
		*y_offset = -1;
		break;

	default:
		assert(0);
		break;
	}

}



void MATH_get_8num_offsets (int direction_numvalue, int *x_offset, int *y_offset)
{
	direction_numvalue = MATH_pow(direction_numvalue);
	MATH_get_8bit_offsets (direction_numvalue, x_offset, y_offset);
}



bool MATH_check_4bit_direction (int value, int side, int *x_offset, int *y_offset)
{
	int direction_bitvalue = value & MATH_pow(side);

	if (direction_bitvalue>0)
	{
		MATH_get_4bit_offsets ( direction_bitvalue, x_offset, y_offset );
		return true;
	}
	else
	{
		return false;
	}
}



bool MATH_check_8bit_direction (int value, int side, int *x_offset, int *y_offset)
{
	int direction_bitvalue = value & MATH_pow(side);

	if (direction_bitvalue>0)
	{
		MATH_get_8bit_offsets ( direction_bitvalue, x_offset, y_offset );
		return true;
	}
	else
	{
		return false;
	}
}



int MATH_rotate_angle (int angle, int target_angle, int angular_speed)
{
	angle = MATH_wrap_angle (angle);
	target_angle = MATH_wrap_angle (target_angle);

	int difference;
	int abs_difference;

	difference = target_angle - angle;
	abs_difference = MATH_abs_int (difference);

	if (angle == target_angle)
	{
		return target_angle;
	}
	else if (abs_difference < angular_speed)
	{
		return target_angle;
	}
	else if (abs_difference > angle_scalar - (angular_speed*2) )
	{
		return target_angle;
	}
	else if (abs_difference > (angle_scalar/2))
	{
		// Angle wraps around the end...

		if (target_angle > angle)
		{
			return (angle - angular_speed);
		}
		else
		{
			return (angle + angular_speed);
		}
	}
	else
	{
		// Angle doesn't wrap around...

		if (target_angle > angle)
		{
			return (angle + angular_speed);
		}
		else
		{
			return (angle - angular_speed);
		}
	}

}


	
int MATH_lerp_int (int a, int b, int p)
{
	return int (MATH_lerp ( float(a) , float(b) , float (p) / float (lerp_multiplier) ));
}



#define SPECIAL_LERP_SMOOTH_IN_SMOOTH_OUT		(0)
#define SPECIAL_LERP_SMOOTH_IN_FAST_OUT			(1)
#define SPECIAL_LERP_FAST_IN_SMOOTH_OUT			(2)
#define SPECIAL_LERP_FAST_IN_FAST_OUT			(3)
#define SPECIAL_LERP_COMPLETE_BOUNCE_CYCLE		(4)
#define SPECIAL_LERP_COMPLETE_WOBBLE_CYCLE		(5)
#define SPECIAL_LERP_DROP_AND_BOUNCE			(6)



int MATH_special_lerp_int (int a, int b, int p, int lerp_process)
{
	float fp = float (p) / float (lerp_multiplier);

	switch(lerp_process)
	{
	case SPECIAL_LERP_SMOOTH_IN_SMOOTH_OUT:
		fp *= percent_to_half_radian;
		fp = 1.0f - (float (cos (fp)) * 0.5f);
		break;

	case SPECIAL_LERP_SMOOTH_IN_FAST_OUT:
		fp *= percent_to_quarter_radian;
		fp += percent_to_quarter_radian;
		fp = 1.0f - float(sin (fp));
		break;

	case SPECIAL_LERP_FAST_IN_SMOOTH_OUT:
		break;

	case SPECIAL_LERP_FAST_IN_FAST_OUT:
		break;

	case SPECIAL_LERP_COMPLETE_BOUNCE_CYCLE:
		break;

	case SPECIAL_LERP_COMPLETE_WOBBLE_CYCLE:
		break;

	case SPECIAL_LERP_DROP_AND_BOUNCE:
		break;

	default:
		assert(0);
		break;
	}

	return int(MATH_lerp (float(a),float(b),fp));
}



int MATH_unlerp_int (int a, int b, int c)
{
	return int (float (lerp_multiplier) * (MATH_unlerp ( float(a) , float(b) , float(c) )));
}



void MATH_set_int_interpolation_accuracy (int new_multiplier)
{
	lerp_multiplier = new_multiplier;
}



bool MATH_check_4bit_within_size (int side, int x, int y, int width, int height, int *x_offset, int *y_offset)
{
	MATH_get_4bit_offsets ( MATH_pow(side), x_offset, y_offset );

	if (x+*x_offset < 0)
	{
		return false;
	}
	else if (x+*x_offset >= width)
	{
		return false;
	}
	else if (y+*y_offset < 0)
	{
		return false;
	}
	else if (y+*y_offset >= height)
	{
		return false;
	}
	else
	{
		return true;
	}
}



bool MATH_check_8bit_within_size (int side, int x, int y, int width, int height, int *x_offset, int *y_offset)
{
	MATH_get_8bit_offsets ( MATH_pow(side), x_offset, y_offset );

	if (x+*x_offset < 0)
	{
		return false;
	}
	else if (x+*x_offset >= width)
	{
		return false;
	}
	else if (y+*y_offset < 0)
	{
		return false;
	}
	else if (y+*y_offset >= height)
	{
		return false;
	}
	else
	{
		return true;
	}
}



void MATH_multiply_matrices (int first_matrix, int second_matrix, int result_matrix)
{
	int t,i,u;

	// Blank the first matrix

	for (t=0; t<MATRIX_SIZE; t++)
	{
		for (i=0; i<MATRIX_SIZE; i++)
		{
			matrices[result_matrix][t][i] = 0.0f;
		}
	}

	// Now multiply the first and second and store the result in the result matrix

	for (t=0; t<MATRIX_SIZE; t++)
	{
		for (i=0; i<MATRIX_SIZE; i++)
		{
			for (u=0; u<MATRIX_SIZE; u++)
			{
				matrices [result_matrix][t][i] = matrices[result_matrix][t][i] + ( matrices[first_matrix][t][u] * matrices[second_matrix][u][i] );
			}
		}
	}

}



#define X_ROTATION_MATRIX		(0)
#define Y_ROTATION_MATRIX		(1)
#define Z_ROTATION_MATRIX		(2)
#define TRANSLATION_MATRIX		(3)
#define RESULT_MATRIX_1			(4)
#define RESULT_MATRIX_2			(5)
#define FINAL_RESULT_MATRIX		(6)

float * MATH_create_3d_matrix ( float rotate_angle_x, float rotate_angle_y, float rotate_angle_z, int transform_x, int transform_y, int transform_z )
{
	int m,t,i;

	for (m=0; m<NUMBER_OF_MATRICES; m++)
	{
		for (t=0; t<MATRIX_SIZE; t++)
		{
			for (i=0; i<MATRIX_SIZE; i++)
			{
				matrices[m][t][i] = (t==i? 1.0f :0.0f);
			}
		}
	}

	float x_cos_angle = float(cos(rotate_angle_x));
	float x_sin_angle = float(sin(rotate_angle_x));

	float y_cos_angle = float(cos(rotate_angle_y));
	float y_sin_angle = float(sin(rotate_angle_y));

	float z_cos_angle = float(cos(rotate_angle_z));
	float z_sin_angle = float(sin(rotate_angle_z));

	matrices[X_ROTATION_MATRIX][1][1] = x_cos_angle;
	matrices[X_ROTATION_MATRIX][2][2] = x_cos_angle;
	matrices[X_ROTATION_MATRIX][1][2] = -x_sin_angle;
	matrices[X_ROTATION_MATRIX][2][1] = x_sin_angle;

	matrices[Y_ROTATION_MATRIX][0][0] = y_cos_angle;
	matrices[Y_ROTATION_MATRIX][2][2] = y_cos_angle;
	matrices[Y_ROTATION_MATRIX][0][2] = y_sin_angle;
	matrices[Y_ROTATION_MATRIX][2][0] = -y_sin_angle;

	matrices[Z_ROTATION_MATRIX][0][0] = z_cos_angle;
	matrices[Z_ROTATION_MATRIX][1][1] = z_cos_angle;
	matrices[Z_ROTATION_MATRIX][0][1] = -z_sin_angle;
	matrices[Z_ROTATION_MATRIX][1][0] = z_sin_angle;

	matrices[TRANSLATION_MATRIX][3][0] = float (transform_x);
	matrices[TRANSLATION_MATRIX][3][1] = float (transform_y);
	matrices[TRANSLATION_MATRIX][3][2] = float (transform_z);
	
	MATH_multiply_matrices (X_ROTATION_MATRIX,Y_ROTATION_MATRIX,RESULT_MATRIX_1);
	MATH_multiply_matrices (RESULT_MATRIX_1,Z_ROTATION_MATRIX,RESULT_MATRIX_2);
	MATH_multiply_matrices (RESULT_MATRIX_2,TRANSLATION_MATRIX,FINAL_RESULT_MATRIX);

	return &matrices[FINAL_RESULT_MATRIX][0][0];
}



void MATH_convert_hsl_to_rgb (int h, int s, int l, int *r, int *g, int *b)
{
	float red, green, blue;
	float hue, saturation, light;
	float tmpred, tmpgreen, tmpblue;
	float tmp1, tmp2;

	hue = float (h) / 360.0f;
	saturation = float (s) / 256.0f;
	light = float (l) / 256.0f;

	if (saturation == 0)
	{
		red = green = blue = light;
	}
	else
	{
		if(light < 0.5)
		{
			tmp2 = light * (1 + saturation);
		}
		else
		{
		  tmp2 = (light + saturation) - (light * saturation);
		}
	}

	tmp1 = 2 * light - tmp2;
	tmpred = hue + 1.0f / 3.0f;

	if (tmpred > 1)
	{
		tmpred--;
	}

	tmpgreen = hue;
	tmpblue = hue - 1.0f / 3.0f;

	if (tmpblue < 0)
	{
		tmpblue++;
	}

	/* red */
	if (tmpred < 1.0f / 6.0f)
	{
		red = tmp1 + (tmp2 - tmp1) * 6.0f * tmpred;
	}
	else if(tmpred < 0.5f)
	{
		red = tmp2;
	}
	else if(tmpred < 2.0f / 3.0f)
	{
		red = tmp1 + (tmp2 - tmp1) * ((2.0f / 3.0f) - tmpred) * 6.0f;
	}
	else
	{
		red = tmp1;
	}

	/* green */
	if (tmpgreen < 1.0f / 6.0f)
	{
		green = tmp1 + (tmp2 - tmp1) * 6.0f * tmpgreen;
	}
	else if(tmpgreen < 0.5f)
	{
		green = tmp2;
	}
	else if(tmpgreen < 2.0f / 3.0f)
	{
		green = tmp1 + (tmp2 - tmp1) * ((2.0f / 3.0f) - tmpgreen) * 6.0f;
	}
	else
	{
		green = tmp1;
	}

	/* blue */
	if (tmpblue < 1.0f / 6.0f)
	{
		blue = tmp1 + (tmp2 - tmp1) * 6.0f * tmpblue;
	}
	else if(tmpblue < 0.5f)
	{
		blue = tmp2;
	}
	else if(tmpblue < 2.0f / 3.0f)
	{
		blue = tmp1 + (tmp2 - tmp1) * ((2.0f / 3.0f) - tmpblue) * 6.0f;
	}
	else
	{
		blue = tmp1;
	}

	*r = (int)(red * 255.0f);
	*g = (int)(green * 255.0f);
	*b = (int)(blue * 255.0f);
}



