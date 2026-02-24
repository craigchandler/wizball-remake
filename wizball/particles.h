#ifndef _PARTICLES_H_
#define _PARTICLES_H_

#define MAX_PARTICLES			(4096)
#define STAR_COLOUR_RAMP_SIZE	(256)

typedef struct
{
	int percentage_speed; // When the stars are created these is set as the UNLERP of the speed with the min and max speeds
	int lifetime; // How long it lives for - only radial
	float ox,oy; // Base co-ords
	float x,y; // Base co-ords
	float base_x_vel, base_y_vel; // It's speed at top-whack
	float backup_base_x_vel, backup_base_y_vel; // Store 'em for when accelleration corrupts them.
	float x_accelleration, y_accelleration; // How much it gains speed over time - radial only
} starfield_struct;



typedef struct
{
	int unique_id;
	// The ID given to the particle collection.

	bool in_use;
	// Is it in use then? Eh? EH?

	float fast_red;
	float fast_green;
	float fast_blue;
	// This is the colour for the faster stars

	float mid_red;
	float mid_green;
	float mid_blue;
	// This is the colour of the mid-speed stars

	float slow_red;
	float slow_green;
	float slow_blue;
	// This is the colour of the slower stars

	float red_ramp[STAR_COLOUR_RAMP_SIZE];
	float green_ramp[STAR_COLOUR_RAMP_SIZE];
	float blue_ramp[STAR_COLOUR_RAMP_SIZE];
	// This is a colour ramp for the stars to pluck their colours from

	int line_type;
	// Draws the particle as a line from ox,oy to sx,sy

	int style;
	// What kinda' starfield it is.

	float box_start_x,box_end_x,box_middle_x;
	float box_start_y,box_end_y,box_middle_y;

	int number_of_stars;
	starfield_struct *stars;

	int min_speed,max_speed;
	int width,height;
	int x_accelleration,y_accelleration;

	// Do we apply accelleration?
	bool use_accelleration;
} starfield_controller_struct;



int PARTICLES_get_index_from_id (int starfield_id);
void PARTICLES_create_starfield ( int starfield_id, int number_of_stars , int style , int width , int height , int min_speed , int max_speed , int x_accelleration , int y_accelleration );
void PARTICLES_destroy_starfield ( int starfield_id );
void PARTICLES_update_starfield ( int starfield_id, int x_update_percentage , int y_update_percentage , int recycle );
void PARTICLES_generate_star_colours (int starfield_id);
void PARTICLES_setup_starfield_colours (int starfield_id, int colour_set, int red, int green, int blue);
void PARTICLES_destroy_all_starfields ( void );
void PARTICLES_initialise_starfield (int starfield_id);



#define LINE_TYPE_NONE			(0) // Draw as dots
#define LINE_TYPE_NORMAL		(1) // Draw as line
#define LINE_TYPE_FADE_TAILS	(2) // Draw as line with second vertex faded to 0,0,0 colour

#define MAX_STARFIELDS		(16)

#define SLOW_STARS			(0)
#define MEDIUM_STARS		(1)
#define FAST_STARS			(2)

extern starfield_controller_struct starfields[MAX_STARFIELDS];



#endif
