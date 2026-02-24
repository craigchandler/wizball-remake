#include <stdio.h>
#include <assert.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "particles.h"
#include "math_stuff.h"
#include "string_stuff.h"
#include "scripting.h"
#include "output.h"

#include "fortify.h"

// This file will contain all the stuff to do with particles which act in a
// simplistic manner and really don't need to go through same complex route as
// everything else. All the variables will be kept in a format which is as
// native to opengl as possible so that things are done nice and fast.

// The system basically allows for starfields and not much else, which is peachy
// by me.

starfield_controller_struct starfields[MAX_STARFIELDS];

int next_free_starfield = 0;



void PARTICLES_setup_starfields (void)
{
	int s;
	int ramp_value;

	for (s=0; s<MAX_STARFIELDS; s++)
	{
		starfields[s].stars = NULL;
		starfields[s].in_use = false;
		starfields[s].unique_id = UNSET;
		starfields[s].number_of_stars = 0;

		for (ramp_value=0; ramp_value<STAR_COLOUR_RAMP_SIZE; ramp_value++)
		{
			starfields[s].red_ramp[ramp_value] = 0.0f;
			starfields[s].green_ramp[ramp_value] = 0.0f;
			starfields[s].blue_ramp[ramp_value] = 0.0f;
		}
	}
}



void PARTICLES_generate_star_colours (int starfield_id)
{
	int index = PARTICLES_get_index_from_id (starfield_id);
	float percent;
	int ramp_index;

	starfield_controller_struct *sp = &starfields[index];

	for (ramp_index=0; ramp_index<STAR_COLOUR_RAMP_SIZE; ramp_index++)
	{
		percent = ( float (ramp_index) / float (STAR_COLOUR_RAMP_SIZE) ) * 2.0f;

		if (percent < 1.0f)
		{
			sp->red_ramp[ramp_index] = MATH_lerp(sp->slow_red,sp->mid_red,percent);
			sp->green_ramp[ramp_index] = MATH_lerp(sp->slow_green,sp->mid_green,percent);
			sp->blue_ramp[ramp_index] = MATH_lerp(sp->slow_blue,sp->mid_blue,percent);
		}
		else
		{
			percent -= 1.0f;

			sp->red_ramp[ramp_index] = MATH_lerp(sp->mid_red,sp->fast_red,percent);
			sp->green_ramp[ramp_index] = MATH_lerp(sp->mid_green,sp->fast_green,percent);
			sp->blue_ramp[ramp_index] = MATH_lerp(sp->mid_blue,sp->fast_blue,percent);
		}

	}
}



void PARTICLES_setup_starfield_colours (int starfield_id, int colour_set, int red, int green, int blue)
{
	int index = PARTICLES_get_index_from_id (starfield_id);

	switch (colour_set)
	{

	case SLOW_STARS:
		starfields[index].slow_red = float(red) / 255.0f;
		starfields[index].slow_green = float(green) / 255.0f;
		starfields[index].slow_blue = float(blue) / 255.0f;
		break;

	case MEDIUM_STARS:
		starfields[index].mid_red = float(red) / 255.0f;
		starfields[index].mid_green = float(green) / 255.0f;
		starfields[index].mid_blue = float(blue) / 255.0f;
		break;

	case FAST_STARS:
		starfields[index].fast_red = float(red) / 255.0f;
		starfields[index].fast_green = float(green) / 255.0f;
		starfields[index].fast_blue = float(blue) / 255.0f;
		break;

	default:
		OUTPUT_message("Invalide Colour Set in Starfield");
		assert(0);
		break;
	}
}



/*

#define APPLY_PERSPECTIVE_HORIZONTALLY		(1)
#define APPLY_PERSPECTIVE_VERTICALLY		(2)

void PARTICLES_perspective_correct ( int starfield_number, float centre_x, float centre_y, float depth_of_field , int axis_application_mask)
{
	starfield_controller_struct *starfield_pointer = &starfields[starfield_number];
	starfield_struct *star_pointer;
	float offset_x,offset_y;
	float z_factor;
	int star_number;

	for (star_number=0; star_number<starfield_pointer->number_of_stars; star_number++)
	{
		star_pointer = &starfield_pointer->stars[star_number];
		// Make a pointer for speed.

		star_pointer->ox = star_pointer->sx;
		star_pointer->oy = star_pointer->sy;
		// Store the old co-ords.

		offset_x = starfield_pointer->stars[star_number].sx - centre_x;
		offset_y = starfield_pointer->stars[star_number].sy - centre_y;

		z_factor = (depth_of_field + starfield_pointer->stars[star_number].sz);

		if (axis_application_mask & APPLY_PERSPECTIVE_HORIZONTALLY)
		{
			star_pointer->sx = (offset_x * depth_of_field) / z_factor;
		}
		else
		{
			star_pointer->sx = offset_x;
		}

		if (axis_application_mask & APPLY_PERSPECTIVE_VERTICALLY)
		{
			star_pointer->sy = (offset_y * depth_of_field) / z_factor;
		}
		else
		{
			star_pointer->sy = offset_y;
		}

		
	}
}



void PARTICLES_move_base_particle_co_ords (int starfield_number, int x_trans, int y_trans, int z_trans)
{
	starfield_controller_struct *starfield_pointer = &starfields[starfield_number];
	starfield_struct *star_pointer;

	int star_number;

	for (star_number=0; star_number<starfield_pointer->number_of_stars; star_number++)
	{
		star_pointer = &starfield_pointer->stars[star_number];
		// Make a pointer for speed.

		star_pointer->suppress_line = false;

		star_pointer->bx += x_trans;

		if (star_pointer->bx > starfield_pointer->box_end_x)
		{
			star_pointer->bx -= starfield_pointer->box_size_x;
			star_pointer->suppress_line = true;
		}
		else if (star_pointer->bx < starfield_pointer->box_start_x)
		{
			star_pointer->bx += starfield_pointer->box_size_x;
			star_pointer->suppress_line = true;
		}

		star_pointer->by += y_trans;

		if (star_pointer->by > starfield_pointer->box_end_y)
		{
			star_pointer->by -= starfield_pointer->box_size_y;
			star_pointer->suppress_line = true;
		}
		else if (star_pointer->by < starfield_pointer->box_start_y)
		{
			star_pointer->by += starfield_pointer->box_size_y;
			star_pointer->suppress_line = true;
		}

		star_pointer->bz += z_trans;

		if (star_pointer->bz > starfield_pointer->box_end_z)
		{
			star_pointer->bz -= starfield_pointer->box_size_z;
			star_pointer->suppress_line = true;
		}
		else if (star_pointer->bz < starfield_pointer->box_start_z)
		{
			star_pointer->bz += starfield_pointer->box_size_z;
			star_pointer->suppress_line = true;
		}
	}
}



void PARTICLES_rotate_and_translate_particles (int starfield_number, int x_angle, int y_angle, int z_angle, int x_trans, int y_trans, int z_trans)
{
	// Creates a matrix for the particles and moves them around using it.

	float f_x_angle = float (x_angle) / angle_conversion_ratio;
	float f_y_angle = float (y_angle) / angle_conversion_ratio;
	float f_z_angle = float (z_angle) / angle_conversion_ratio;

	float *matrix_pointer;
	starfield_controller_struct *starfield_pointer = &starfields[starfield_number];
	starfield_struct *star_pointer;

	int star_number;

	matrix_pointer = MATH_create_3d_matrix ( f_x_angle, f_y_angle, f_z_angle, x_trans, y_trans, z_trans );

	for (star_number=0; star_number<starfield_pointer->number_of_stars; star_number++)
	{
		star_pointer = &starfield_pointer->stars[star_number];
		// Make a pointer for speed.

		star_pointer->sx = (star_pointer->bx * matrix_pointer[0]) + (star_pointer->by * matrix_pointer[4]) + (star_pointer->bz * matrix_pointer[8]) + matrix_pointer[12];
		star_pointer->sy = (star_pointer->bx * matrix_pointer[1]) + (star_pointer->by * matrix_pointer[5]) + (star_pointer->bz * matrix_pointer[9]) + matrix_pointer[13];
		star_pointer->sz = (star_pointer->bx * matrix_pointer[2]) + (star_pointer->by * matrix_pointer[6]) + (star_pointer->bz * matrix_pointer[10]) + matrix_pointer[14];
	}
}

*/

#define STARFIELD_STYLE_PARALLAX				(1)
#define STARFIELD_STYLE_FIRST_PERSON			(2)
#define STARFIELD_STYLE_RADIAL_FIRST_PERSON		(3)

void PARTICLES_create_starfield ( int starfield_id, int number_of_stars , int style , int width , int height , int min_speed , int max_speed , int x_accelleration , int y_accelleration )
{
	starfields[next_free_starfield].in_use = true;

	starfields[next_free_starfield].unique_id = starfield_id;

	if (starfields[next_free_starfield].stars != NULL)
	{
		free (starfields[next_free_starfield].stars);
		starfields[next_free_starfield].stars = NULL;
	}

	starfields[next_free_starfield].stars = (starfield_struct *) malloc (number_of_stars * sizeof(starfield_struct) );

	starfields[next_free_starfield].box_start_x = 0.0f;
	starfields[next_free_starfield].box_end_x = float(width);
	starfields[next_free_starfield].box_middle_x = float(width)/2.0f;

	starfields[next_free_starfield].box_start_y = 0.0f;
	starfields[next_free_starfield].box_end_y = float(height);
	starfields[next_free_starfield].box_middle_y = float(height)/2.0f;

	starfields[next_free_starfield].min_speed = min_speed;
	starfields[next_free_starfield].max_speed = max_speed;

	starfields[next_free_starfield].width = width;
	starfields[next_free_starfield].height = height;

	starfields[next_free_starfield].number_of_stars = number_of_stars;

	starfields[next_free_starfield].x_accelleration = x_accelleration;
	starfields[next_free_starfield].y_accelleration = y_accelleration;

	if ((x_accelleration != 10000) || (y_accelleration != 10000))
	{
		starfields[next_free_starfield].use_accelleration = true;
	}
	else
	{
		starfields[next_free_starfield].use_accelleration = false;
	}

	starfields[next_free_starfield].style = style;

	next_free_starfield++;
	next_free_starfield%=MAX_STARFIELDS;
}



int PARTICLES_get_index_from_id (int starfield_id)
{
	int s;

	for (s=0; s<MAX_STARFIELDS; s++)
	{
		if ( (starfields[s].unique_id == starfield_id) && (starfields[s].in_use == true) )
		{
			return s;
		}
	}

	return UNSET;
}



void PARTICLES_initialise_starfield (int starfield_id)
{
	// This initialises the starfield. It's basically separate so that it can
	// be called after setting the randomise seed when you need to record games
	// which have starfields in the background.

	int index = PARTICLES_get_index_from_id (starfield_id);

	if (index == UNSET)
	{
		return;
	}

	int star_number;

	float float_x_accelleration = float (starfields[index].x_accelleration) / 10000.0f;
	float float_y_accelleration = float (starfields[index].y_accelleration) / 10000.0f;

	float ox,oy;
	float angle;
	float speed;

	float start_vel = float (starfields[index].min_speed) / 100.0f;
	float end_vel = float (starfields[index].max_speed) / 100.0f;
	float vel_diff = end_vel - start_vel;
	float vel_percentage;

	switch (starfields[index].style)
	{
	case STARFIELD_STYLE_PARALLAX:
		for (star_number=0; star_number<starfields[index].number_of_stars; star_number++)
		{
			starfields[index].stars[star_number].x = float(MATH_rand(0,starfields[index].width));
			starfields[index].stars[star_number].y = float(MATH_rand(0,starfields[index].height));

			vel_percentage = float(star_number)/float (starfields[index].number_of_stars);
			speed = start_vel + (vel_diff * vel_percentage);

			starfields[index].stars[star_number].base_x_vel = speed;
			starfields[index].stars[star_number].base_y_vel = speed;

			starfields[index].stars[star_number].percentage_speed = int (vel_percentage * STAR_COLOUR_RAMP_SIZE);

			if (starfields[index].stars[star_number].percentage_speed >= STAR_COLOUR_RAMP_SIZE)
			{
				starfields[index].stars[star_number].percentage_speed = STAR_COLOUR_RAMP_SIZE-1;
			}

			if (starfields[index].use_accelleration == true)
			{
				starfields[index].stars[star_number].x_accelleration = float_x_accelleration;
				starfields[index].stars[star_number].y_accelleration = float_y_accelleration;
			}

			starfields[index].stars[star_number].backup_base_x_vel = starfields[index].stars[star_number].base_x_vel;
			starfields[index].stars[star_number].backup_base_y_vel = starfields[index].stars[star_number].base_y_vel;
		}
		break;

	case STARFIELD_STYLE_FIRST_PERSON:
		for (star_number=0; star_number<starfields[index].number_of_stars; star_number++)
		{
			starfields[index].stars[star_number].x = float(MATH_rand(0,starfields[index].width));
			starfields[index].stars[star_number].y = float(MATH_rand(0,starfields[index].height));

			ox = starfields[index].stars[star_number].x - (starfields[index].width/2);
			oy = starfields[index].stars[star_number].y - (starfields[index].height/2);

			angle = float(atan2 (ox,oy));

			speed = float(MATH_rand(starfields[index].min_speed,starfields[index].max_speed)) / 100.0f;

			starfields[index].stars[star_number].base_x_vel = speed * float(sin(angle));
			starfields[index].stars[star_number].base_y_vel = speed * float(cos(angle));

			if (starfields[index].use_accelleration == true)
			{
				starfields[index].stars[star_number].x_accelleration = float_x_accelleration;
				starfields[index].stars[star_number].y_accelleration = float_y_accelleration;
			}

			starfields[index].stars[star_number].backup_base_x_vel = starfields[index].stars[star_number].base_x_vel;
			starfields[index].stars[star_number].backup_base_y_vel = starfields[index].stars[star_number].base_y_vel;
		}
		break;

	default:
		assert(0);
		break;
	}

}



void PARTICLES_update_starfield ( int starfield_id, int x_update_percentage , int y_update_percentage , int recycle )
{
	int index = PARTICLES_get_index_from_id (starfield_id);

	if (index == UNSET)
	{
		return;
	}

	float x_p = float(x_update_percentage)/10000.0f;
	float y_p = float(y_update_percentage)/10000.0f;

	starfield_controller_struct *starfield_pointer = &starfields[index];
	starfield_struct *star_pointer = &starfield_pointer->stars[0];

	int t;

	bool reset;

	switch (starfield_pointer->style)
	{	
	case STARFIELD_STYLE_PARALLAX:
		for (t=0; t<starfield_pointer->number_of_stars; t++)
		{
			star_pointer->ox = star_pointer->x;
			star_pointer->oy = star_pointer->y;
			
			star_pointer->x += ( star_pointer->base_x_vel * x_p );
			star_pointer->y += ( star_pointer->base_y_vel * y_p );

			if (recycle)
			{
				if (star_pointer->x > starfield_pointer->box_end_x)
				{
					star_pointer->x = starfield_pointer->box_start_x;
					star_pointer->ox = starfield_pointer->box_start_x;
					star_pointer->base_x_vel = star_pointer->backup_base_x_vel;
					star_pointer->base_y_vel = star_pointer->backup_base_y_vel;
				}

				if (star_pointer->y > starfield_pointer->box_end_y)
				{
					star_pointer->y = starfield_pointer->box_start_y;
					star_pointer->oy = starfield_pointer->box_start_y;
					star_pointer->base_x_vel = star_pointer->backup_base_x_vel;
					star_pointer->base_y_vel = star_pointer->backup_base_y_vel;
				}

				if (star_pointer->x < starfield_pointer->box_start_x)
				{
					star_pointer->x = starfield_pointer->box_end_x;
					star_pointer->ox = starfield_pointer->box_end_x;
					star_pointer->base_x_vel = star_pointer->backup_base_x_vel;
					star_pointer->base_y_vel = star_pointer->backup_base_y_vel;
				}

				if (star_pointer->y < starfield_pointer->box_start_y)
				{
					star_pointer->y = starfield_pointer->box_end_y;
					star_pointer->oy = starfield_pointer->box_end_y;
					star_pointer->base_x_vel = star_pointer->backup_base_x_vel;
					star_pointer->base_y_vel = star_pointer->backup_base_y_vel;
				}
			}

			star_pointer++;
		}
		break;

	case STARFIELD_STYLE_FIRST_PERSON:
		for (t=0; t<starfield_pointer->number_of_stars; t++)
		{
			star_pointer->ox = star_pointer->x;
			star_pointer->oy = star_pointer->y;
			
			star_pointer->x += ( star_pointer->base_x_vel * x_p );
			star_pointer->y += ( star_pointer->base_y_vel * y_p );

			if (recycle)
			{
				reset = false;

				if (x_update_percentage > 0)
				{
					if ( (star_pointer->x < starfield_pointer->box_start_x) || (star_pointer->x > starfield_pointer->box_end_x) )
					{ 
						reset = true;
					}
				}
				else
				{
					if ( (star_pointer->x < starfield_pointer->box_middle_x) && (star_pointer->base_x_vel > 0.0f) )
					{ 
						// ie, if the star travels to the RIGHT normally but is LEFT of the center...
						reset = true;
					}
					else if ( (star_pointer->x > starfield_pointer->box_middle_x) && (star_pointer->base_x_vel < 0.0f) )
					{ 
						// ie, if the star travels to the LEFT normally but is RIGHT of the center...
						reset = true;
					}
				}

				if (y_update_percentage > 0)
				{
					if ( (star_pointer->y < starfield_pointer->box_start_y) || (star_pointer->y > starfield_pointer->box_end_y) )
					{ 
						reset = true;
					}
				}
				else
				{
					if ( (star_pointer->y < starfield_pointer->box_middle_y) && (star_pointer->base_y_vel > 0.0f) )
					{ 
						// ie, if the star travels to the DOWN normally but is ABOVE of the center...
						reset = true;
					}
					else if ( (star_pointer->y > starfield_pointer->box_middle_y) && (star_pointer->base_y_vel < 0.0f) )
					{ 
						// ie, if the star travels to the UP normally but is BELOW the center...
						reset = true;
					}
				}

				if (reset)
				{
					star_pointer->x = starfield_pointer->box_middle_x;
					star_pointer->y = starfield_pointer->box_middle_y;
					star_pointer->ox = star_pointer->x;
					star_pointer->oy = star_pointer->y;

					star_pointer->base_x_vel = star_pointer->backup_base_x_vel;
					star_pointer->base_y_vel = star_pointer->backup_base_y_vel;
				}
			}

			star_pointer++;
		}
		break;

	default:
		assert(0);
		break;
	}


	
	// Update velocities if we have any cause to...
	if (starfield_pointer->use_accelleration == true)
	{
		star_pointer = &starfield_pointer->stars[0];

		for (t=0; t<starfield_pointer->number_of_stars; t++)
		{
			star_pointer->base_x_vel *= star_pointer->x_accelleration;
			star_pointer->base_y_vel *= star_pointer->y_accelleration;

			star_pointer++;
		}
	}


}



void PARTICLES_destroy_starfield ( int starfield_id )
{
	int index = PARTICLES_get_index_from_id (starfield_id);

	if (index == UNSET)
	{
		return;
	}

	if (starfields[index].stars != NULL)
	{
		free (starfields[index].stars);
		starfields[index].stars = NULL;
		starfields[index].in_use = false;
		starfields[index].unique_id = UNSET;
		starfields[index].stars = 0;
	}
}



void PARTICLES_destroy_all_starfields ( void )
{
	int s;
	
	for (s=0; s<MAX_STARFIELDS; s++)
	{
		if (starfields[s].in_use == true)
		{
			PARTICLES_destroy_starfield ( starfields[s].unique_id );
		}
	}
}



