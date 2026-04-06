#ifndef _MATH_STUFF_H_
#define _MATH_STUFF_H_

#define PI								(3.1415926535897932384626433832795f)

#define BEFORE_LINE						(-1)
#define ON_LINE							(0)
#define AFTER_LINE						(1)

#define CO_LINEAR						(-1)
#define NO_INTERSECTION					(0)
#define INTERSECTION					(1)

#define NUM_CO_ORDS						(4)
#define NUM_AXIS						(2)

#define X_CO_ORD						(0)
#define Y_CO_ORD						(1)

#define NORTH							(1)
#define NORTH_EAST						(2)
#define EAST							(4)
#define SOUTH_EAST						(8)
#define SOUTH							(16)
#define SOUTH_WEST						(32)
#define WEST							(64)
#define NORTH_WEST						(128)
// For approximate direction

#define FOUR_BIT_TOP					(1)
#define FOUR_BIT_RIGHT					(2)
#define FOUR_BIT_BOTTOM					(4)
#define FOUR_BIT_LEFT					(8)

#define EIGHT_BIT_TOP					(1)
#define EIGHT_BIT_TOP_RIGHT				(2)
#define EIGHT_BIT_RIGHT					(4)
#define EIGHT_BIT_BOTTOM_RIGHT			(8)
#define EIGHT_BIT_BOTTOM				(16)
#define EIGHT_BIT_BOTTOM_LEFT			(32)
#define EIGHT_BIT_LEFT					(64)
#define EIGHT_BIT_TOP_LEFT				(128)

#define FOUR_NUMERICAL_TOP					(0)
#define FOUR_NUMERICAL_RIGHT				(1)
#define FOUR_NUMERICAL_BOTTOM				(2)
#define FOUR_NUMERICAL_LEFT					(3)

#define EIGHT_NUMERICAL_TOP					(0)
#define EIGHT_NUMERICAL_TOP_RIGHT			(1)
#define EIGHT_NUMERICAL_RIGHT				(2)
#define EIGHT_NUMERICAL_BOTTOM_RIGHT		(3)
#define EIGHT_NUMERICAL_BOTTOM				(4)
#define EIGHT_NUMERICAL_BOTTOM_LEFT			(5)
#define EIGHT_NUMERICAL_LEFT				(6)
#define EIGHT_NUMERICAL_TOP_LEFT			(7)



#define NUMBER_OF_MATRICES		(10)
#define MATRIX_SIZE				(4)



#define SAMPLING_FREQUENCY				(0.1f)
// For Catmull-Rom splines

typedef struct
{
	int co_ords[NUM_CO_ORDS][NUM_AXIS]; // Don't really need float for the sort of accuracy I'm after.
	float angle;
	int pivot_x;
	int pivot_y;
	float cos_angle;
	float sin_angle;
} rectangle;

float MATH_abs (float value);
int MATH_abs_int (int value);
int MATH_sgn_float (float value);
int MATH_sgn_int (int value);
float MATH_round_to_nearest_float (float number, float round_to_nearest);
float MATH_round_to_lower_float (float number, float round_to_nearest);
float MATH_round_to_upper_float (float number, float round_to_nearest);
int MATH_round_to_nearest_int (int number, int round_to_nearest);
int MATH_round_to_lower_int (int number, int round_to_nearest);
int MATH_round_to_upper_int (int number, int round_to_nearest);
float MATH_distance_to_line (float x1, float y1, float x2, float y2, float px, float py , int *point_of_line , float *ratio_on_line);
bool MATH_same_signs (float a, float b);
int MATH_intersect_lines (float x1,float y1,float x2,float y2,float x3,float y3,float x4,float y4,float *ix,float *iy);
int MATH_get_distance_int ( int x1, int y1, int x2, int y2);
float MATH_get_distance_float ( float x1, float y1, float x2, float y2);
float MATH_lerp (float a , float b , float p);
float MATH_unlerp (float a , float b , float c);
float MATH_largest (float a, float b);
float MATH_smallest (float a, float b);
int MATH_largest_int (int a, int b);
int MATH_smallest_int (int a, int b);
int MATH_rand (int lowest, int highest);
int MATH_special_rand (int seed_index, int lowest, int highest);
int MATH_get_special_rand_seed (int seed_index);

int MATH_wrap (int value , int wrap);
int MATH_fold (int number, int start, int end);
int MATH_constrain (int number, int start, int end);
float MATH_constrain_float (float number, float start, float end);

void MATH_make_rectangle ( rectangle *r , int x, int y, int upper_width , int upper_height , int lower_width , int lower_height , float angle );
void MATH_rotate_and_translate_rect ( rectangle *r );
void MATH_find_rotated_rectangle_extents ( rectangle *r , int *rect_x1 , int *rect_y1 , int *rect_x2 , int *rect_y2 );

int MATH_get_rough_direction ( int x1, int y1, int x2, int y2 );
int MATH_get_up_right_down_left ( int x1, int y1, int x2, int y2 );

float MATH_get_catmull_rom_spline_point (float p0, float p1, float p2, float p3, float t);
float MATH_bezier (float point_1, float control_1, float control_2 , float point_2, float percent);

float MATH_get_spline_length (float x0, float y0, float x1, float y1, float x2, float y2, float x3, float y3);

void MATH_convert_hsv_to_rgb ( int hue, int saturation, int value, int *red, int *green, int *blue );
void MATH_convert_rgb_to_hsv ( int red, int green, int blue, int *hue, int *saturation, int *value );

void MATH_get_4bit_offsets (int direction_bitvalue, int *x_offset, int *y_offset);
void MATH_get_8bit_offsets (int direction_bitvalue, int *x_offset, int *y_offset);

void MATH_get_4num_offsets (int direction_numvalue, int *x_offset, int *y_offset);
void MATH_get_8num_offsets (int direction_numvalue, int *x_offset, int *y_offset);

bool MATH_check_4bit_direction (int value, int bit, int *x_offset, int *y_offset);
bool MATH_check_8bit_direction (int value, int bit, int *x_offset, int *y_offset);

bool MATH_check_4bit_within_size (int side, int x, int y, int width, int height, int *x_offset, int *y_offset);
bool MATH_check_8bit_within_size (int side, int x, int y, int width, int height, int *x_offset, int *y_offset);

int MATH_pow (int power);

void MATH_srand (unsigned int seed);
void MATH_seed_special_rand (int seed_index, unsigned int seed_number);

// Primitive Intersects

bool MATH_rotated_rectangle_to_rotated_rectangle_intersect ( rectangle *r1 , rectangle *r2 );
bool MATH_rotated_rectangle_to_rectangle_intersect ( rectangle *r1 , int rect_x1 , int rect_y1 , int rect_x2 , int rect_y2 );
bool MATH_rotated_rectangle_to_circle_intersect (rectangle *r , int px , int py , int radius );
bool MATH_rotated_rectangle_to_point_intersect ( rectangle *r , int px, int py );

bool MATH_rectangle_to_rectangle_intersect (int rect_1_x1 , int rect_1_y1 , int rect_1_x2 , int rect_1_y2 , int rect_2_x1 , int rect_2_y1 , int rect_2_x2 , int rect_2_y2);
bool MATH_rectangle_to_circle_intersect ( int rect_x1 , int rect_y1 , int rect_x2 , int rect_y2 , int circle_x , int circle_y , int radius );
bool MATH_rectangle_to_point_intersect ( int rect_x1 , int rect_y1 , int rect_x2 , int rect_y2 , int px, int py );

bool MATH_rectangle_to_point_intersect_by_size ( int rect_x , int rect_y , int width , int height , int px, int py );

bool MATH_circle_to_circle_intersect ( int x1 , int y1 , int r1 , int x2 , int y2 , int r2 );
bool MATH_circle_to_point_intersect ( int x1 , int y1 , int r1 , int x2 , int y2 );

bool MATH_line_to_line_intersect ( int line_1_x1 , int line_1_y1 , int line_1_x2 , int line_1_y2 , int line_2_x1 , int line_2_y1 , int line_2_x2 , int line_2_y2 );

float MATH_float_modulus (float total, float modulus);

void MATH_setup_trig_tables (int new_arcangle, int new_angle);

int MATH_wrap_angle (int angle);
int MATH_get_sin_table_value (int angle);
int MATH_get_cos_table_value (int angle);

int MATH_wrap_arcangle (int arcangle);
int MATH_get_asin_table_value (int arcangle);
int MATH_get_acos_table_value (int arcangle);

int MATH_lerp_int (int a, int b, int p);
int MATH_unlerp_int (int a, int b, int c);

int MATH_atan2 (int x , int y);

int MATH_rotate_angle (int angle, int target_angle, int angular_speed);
void MATH_rotate_point_around_origin (float *x, float *y, float angle);

void MATH_destroy_trig_tables (void);

float * MATH_create_3d_matrix ( float rotate_angle_x, float rotate_angle_y, float rotate_angle_z, int transform_x, int transform_y, int transform_z );

int MATH_diff_angle (int destination_angle, int current_angle);

void MATH_convert_hsl_to_rgb (int h, int s, int l, int *r, int *g, int *b);

// Externs

extern int angle_scalar;
extern int arcangle_scalar;
extern float angle_conversion_ratio;

#endif
