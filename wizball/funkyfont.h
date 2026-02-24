#ifndef _FUNKYFONT_H_
#define _FUNKYFONT_H_

// Funky Font Defines

#define MAX_LETTERS_PER_FONT				(96)
#define MAX_LINES_PER_BOX					(100)
#define MAX_FRAME_LIST_SIZE					(2048)
#define MAX_LETTERS							(256)
#define PFONT_ALIGN_LEFT					(1)
#define PFONT_ALIGN_TOP						(1)
#define PFONT_ALIGN_CENTER					(2)
#define PFONT_ALIGN_RIGHT					(3)
#define PFONT_ALIGN_BOTTOM					(3)
#define PFONT_ALIGN_JUSTIFY					(4)
#define PFONT_TEMP_TEXT_ID					(0)
#define PFONT_TEXT_TYPE_BASIC				(1)
#define PFONT_TEXT_TYPE_INSTANT_IMAGE		(2)
#define PFONT_TEXT_TYPE_SPLIT_IMAGE			(3)
#define PFONT_TEXT_TYPE_FRAME_LIST			(4)
#define PFONT_TEXT_TYPE_GRADUAL_IMAGE		(5)
#define PFONT_STATUS_PRE_PROCESSING			(-1)
#define PFONT_STATUS_PROCESSING				(0)
#define PFONT_STATUS_COMPLETE				(1)
#define PFONT_DOES_NOT_EXIST				(2)

#define	PFONT_NEW_LINE_CHAR					'|'
	// If this is encountered in a chunk of text then it'll cause a line-break and paragraph end.

#define MAX_DISPLAY_HEIGHT					(480)
	// SET THIS TO YOUR SCREEN HEIGHT! This is used when creating the makecol tables for drawing vertically
	// colour banded sprites. If you send a sprite of a greater height than this via that function then it'll
	// implode messily with a "shwooooooop!" sound. Or maybe a beep.

#define PFONT_SELECTION_MODE_NORMAL			(0)
#define PFONT_SELECTION_MODE_CIRCLE			(1)
#define PFONT_SELECTION_MODE_HARD_RECT		(2)
#define PFONT_SELECTION_MODE_SOFT_RECT		(3)
#define PFONT_SELECTION_MODE_LINES			(4)
#define PFONT_SELECTION_MODE_CHARS			(5)
#define PFONT_SELECTION_MODE_RAND_CHARS		(6)
#define PFONT_SELECTION_MODE_HALO			(7)

#define PFONT_EFFECT_DISTORT				(0)
#define PFONT_EFFECT_BOUNCE_IN_PHASE		(1)
#define PFONT_EFFECT_BOUNCE_OUT_PHASE		(2)
#define PFONT_EFFECT_SINE_IN_PHASE			(3)
#define PFONT_EFFECT_SINE_OUT_PHASE			(4)
#define PFONT_EFFECT_ADD_NOISE				(5)
#define PFONT_EFFECT_ADD_NOISE_BASELINE		(6)
#define PFONT_EFFECT_ROTATE					(7)
#define PFONT_EFFECT_SCALE					(8)
#define PFONT_EFFECT_BOW_IN					(9)
#define PFONT_EFFECT_BOW_OUT				(10)
#define PFONT_EFFECT_MOVE					(11)
#define PFONT_EFFECT_DAMP					(12)
#define PFONT_EFFECT_EXPLODE				(14)

#define PFONT_MINMAX_IGNORE					(0)
#define PFONT_MINMAX_CHARS					(1)
#define PFONT_MINMAX_LINES					(2)

#define NOT_APPLICABLE						(0.5)

#define PFONT_LINEAR_SELECTION				(0)
#define PFONT_BUBBLE_SELECTION				(1)
#define PFONT_POINT_SELECTION				(2)
#define PFONT_SOFT_SELECTION				(3)

#define NEITHER_SOLID						(0)
#define INNER_SOLID							(1)
#define OUTER_SOLID							(2)

// Funky Font Structures

typedef struct 
{
	BITMAP *font_gfx;
	int char_width;
	int char_height;
	int image_width;
	int image_height;
	int frames_per_row;
	int frames_per_column;
	int letter_widths[MAX_LETTERS];
} font_struct;

typedef struct
{
	BITMAP *texture_image;
	int width;
	int height;
	int wrap_width;
	int wrap_height;
} texture_struct;

typedef struct
{
	char text [MAX_FRAME_LIST_SIZE];
	// This is where the words you pass are stored.
	int image_width;
	// The size of the image created in order to store all the letters. Note it's not actually always created
	// but the information is used all over the place. It's specified by the user.
	int image_height;
	// The size of the image created in order to store all the letters. Note it's not actually always created
	// but the information is used all over the place.
	int top_left_x;
	int top_left_y;
	int bottom_right_x;
	int bottom_right_y;
	// When you draw text to a particular image it may well be that the co-ordinates spill outside of the base
	// base image size you specified because you scaled or transformed the letter co-ordinates in some way. As
	// such we need to find the proper size of the box needed to store the text as so we loop through the text
	// and find the extents of the letters. Now the really important one is the top_left_x and top_left_y as
	// when we draw letters to the box we need to modify their co-ordinates by this amount so that they all
	// appear within the confines of the box. Lawks.
	int total_lines;
	// The total number of lines in the paragraph generated.
	int x_spacing;
	// The pixel distance between each letter of text.
	int y_spacing;
	// The pixel distance between each row of text.
	int alignment;
	// Well, duh?
	int line_start[MAX_LINES_PER_BOX];
	int line_length[MAX_LINES_PER_BOX];
	// These are the starts of each line in the text box and the length of them and they're used stored to make things easy.
	int line_frame_start[MAX_LINES_PER_BOX];
	int line_frame_length[MAX_LINES_PER_BOX];
	// These are the number of frames in each line (ie, ignoring spaces)
	int words_in_line[MAX_LINES_PER_BOX];
	// This is a list of the numbers of words in each line of the text and is used for... Um... Actually what the hell *is* it used for?
	font_struct *prop_font;
	// This is the number of the font used to calculate letter widths during the creation of the proportional text.
	float frame_list_base_x[MAX_FRAME_LIST_SIZE];
	float frame_list_base_y[MAX_FRAME_LIST_SIZE];
	// These are the co-ords calculated by the proportional print section and they are never altered once created.
	float frame_list_new_x[MAX_FRAME_LIST_SIZE];
	float frame_list_new_y[MAX_FRAME_LIST_SIZE];
	// These are the co-ords created by the various effect routines and are based on the base co-ords.
	float frame_list_image_x[MAX_FRAME_LIST_SIZE];
	float frame_list_image_y[MAX_FRAME_LIST_SIZE];
	// This is the co-ordinate of the texture for this letter within the font image to save on recalculation
	float frame_list_texture_x[MAX_FRAME_LIST_SIZE];
	float frame_list_texture_y[MAX_FRAME_LIST_SIZE];
	// This is the co-ords for the texture which colours in the letters afterwards if you have texturing turned on.
	int frame_list_frame[MAX_FRAME_LIST_SIZE];
	// This is is the number of the image within the font graphic and is kinda' redundant information
	int frame_list_line[MAX_FRAME_LIST_SIZE];
	// This is so you can select a line at a time in both selections and also when drawing 'em.
	int frame_pointer;
	// This is used throughout the creation of the proportional text and ends up as the number of frames created
	int drawn_frame_pointer;
	// This is used for when you're drawing frames to an image.
} prop_text_struct;

// Funky Font Headers

void PFONT_load_font (font_struct *prop_font, char *filename, int frame_width, int frame_height);
	// Loads a font and figures out the widths for each letter. Stores it in an internal BITMAP.
void PFONT_free_font ( font_struct *prop_font);
	// Frees up the space taken up by the FONT.
void PFONT_load_texture ( texture_struct *texture , char *filename , int x_overlap , int y_overlap );
	// Loads up a texture and sets a few parameters. Font is stored in an internal BITMAP.
void PFONT_free_texture ( texture_struct *texture );
	// Frees up the space taken up by the texture.

void PFONT_posset_reset ( prop_text_struct *prop_text );
	// Call this every frame to set the letter's base positions back to normal unless you actually want
	// to have recursive effects gradually explode your text, which might make a nice transition.
void PFONT_texset_basic_spread ( prop_text_struct *prop_text , float x_spread , float y_spread , float x_base , float y_base , bool convert );
	// This does the same as the above function but for textures, basically. Call it every frame before
	// you bugger with the co-ordinates. Obviously that's only if you're texturing the font, though.

void PFONT_create_frame_list ( prop_text_struct *prop_text , font_struct *prop_font, char *words, int width, int alignment, int x_spacing, int y_spacing , bool reverse_x_order , bool reverse_y_order );
	// Call this with the words you want to be drawing and the various parameters of the box and formatting
	// and it'll create a table of co-ordinaties for speed drawing of the text.

void PFONT_draw_frame_list_notexture ( prop_text_struct *prop_text , int min , int max , int min_max_type , int x , int y , float handle_x , float handle_y , BITMAP *buffer_image );
	// Draws the frame list to the screen using the default graphic for the font (so if you have a nicely
	// drawn bitmap font and you aren't arsed about texturing it, use this).
void PFONT_draw_frame_list_texture ( prop_text_struct *prop_text , int min , int max , int min_max_type , int x , int y , float handle_x , float handle_y , BITMAP *buffer_image , texture_struct *texture );
	// Draws it using a suppled texture, wrapping the texture co-ordinates to the supplied extents when you
	// loaded the texture. Bonza!
void PFONT_draw_frame_list_coloured ( prop_text_struct *prop_text , int min , int max , int min_max_type , int x , int y , float handle_x , float handle_y , BITMAP *buffer_image , int red , int green , int blue );
	// Draws the text but in the supplied RGB values. Good for psychadelic ATARI coin-op type effects.
void PFONT_draw_frame_list_vert_lerped_coloured ( prop_text_struct *prop_text , int min , int max , int min_max_type , int x , int y , float handle_x , float handle_y , BITMAP *buffer_image , int red_1 , int green_1 , int blue_1 , int red_2 , int green_2 , int blue_2 );
	// Ditto but it vertically interpolates across the text from RGB1 to RGB2. Slightly slower than the above.

void PFONT_set_mode_all (float influence);
	// Any effects used directly after this will effect all letters equally to the specified amount (0 to 1 equating to 0% to 100%).
void PFONT_set_mode_rand (int seed, float min_range, float max_range , float bleed_range , float inverse ,  bool generate_values);
	// This allows you to choose a random selection of letters within the body of the text, however somewhat funkily you
	// also supply the seed and min and max range so instead of the choice of letters changing each frame it can be constant
	// and gradually expanded of shrunken. Some funky transitions could be done with this.
void PFONT_set_mode_circle_select (float handle_x , float handle_y , int strength , int type , float inverse , float x_scale, float y_scale);
	// This selects everything within the specified radius/strength of the handle's (0-1 as a percentage across the face of the base text box).
	// The type of selection can either be PFONT_LINEAR_SELECTION (ie, a straight drop-off from the centre of the circle to the
	// outskirts), PFONT_BUBBLE_SELECTION, PFONT_POINT_SELECTION or PFONT_SOFT_SELECTION. Have a fiddle with them to see which you
	// find most suitable.
	// Note that the x_scale and y_scale values are 0-2 (typically) and allow you bias the aspect ratio of the circle so that you
	// actually get a elliptical selection profile. Golly, eh?
void PFONT_set_mode_halo_select (float handle_x , float handle_y, int inner_strength, int outer_strength, int inner_type, int outer_type, float radius , int solid , float inverse , float x_scale , float y_scale);
	// This is kinda' like the circle one, only instead the radius is where the effect is strongest with bleed off
	// to either side.
void PFONT_set_mode_hard_rect_select (float handle_x , float handle_y , int width , int height , int strength_x , int strength_y , int type , float inverse);
	// This selects the specified rectangle with bleeding off to the sides of the given strengths. Same selection types.
void PFONT_set_mode_lines_select (int start_line , int end_line , int strength , int type , float inverse);
	// This allows you to specify line numbers within the block of text together with a bleed-off value.
void PFONT_set_mode_character_select (int start_character , int end_character , int strength , int type , float inverse);
	// And finally you can specify a number of characters to be selected together with a bleed-off.

void PFONT_effect ( prop_text_struct *prop_text , int effect_number , bool effect_texture , float handle_x , float handle_y , float param_1 , float param_2 , float param_3 , float param_4 , float param_5 , float param_6 );
	// And now all the effects are contained within this one lovely construct. You just call it with the number of the effect
	// you want, the handles around where the effect is applied (only actually used during scaling) along with six parameters
	// (several of which you'll leave as 0's as they won't be used in every effect) and it'll apply the effect to the area
	// you set as the selection.

	// Note most effects can be applied to either the texture of the letter co-ordinates, there's a boolean value you set to
	// TRUE when calling the function if it is for the texture.

	// The table of effect DEFINEs, descriptions and which values they use follow:

// Effect Define Name					handle_x	handle_y	param_1		param_2		param_3		param_4		param_5		param_6		

// PFONT_EFFECT_DISTORT					N/A			N/A			hori_deviat	vert_deviat	N/A			N/A			N/A			N/A
// PFONT_EFFECT_BOUNCE_IN_PHASE			N/A			N/A			x_angle_per	y_angle_per	x_amplitude	y_amplitude	x_offset	y_offset
// PFONT_EFFECT_BOUNCE_OUT_PHASE		N/A			N/A			x_angle_per	y_angle_per	x_amplitude	y_amplitude	x_offset	y_offset
// PFONT_EFFECT_SINE_IN_PHASE			N/A			N/A			x_angle_per	y_angle_per	x_amplitude	y_amplitude	x_offset	y_offset
// PFONT_EFFECT_SINE_OUT_PHASE			N/A			N/A			x_angle_per	y_angle_per	x_amplitude	y_amplitude	x_offset	y_offset
// PFONT_EFFECT_ADD_NOISE				N/A			N/A			x_noise		y_noise		seed		flip		N/A			N/A
// PFONT_EFFECT_ADD_NOISE_BASELINE		N/A			N/A			x_noise		y_noise		seed		flip		N/A			N/A
// PFONT_EFFECT_ROTATE					Yes			Yes			angle		N/A			N/A			N/A			N/A			N/A
// PFONT_EFFECT_SCALE					Yes			Yes			x_percent	y_percent	N/A			N/A			N/A			N/A
// PFONT_EFFECT_BOW_IN					N/A			N/A			x_distance	y_distance	N/A			N/A			N/A			N/A
// PFONT_EFFECT_BOW_OUT					N/A			N/A			x_distance	y_distance	N/A			N/A			N/A			N/A
// PFONT_EFFECT_MOVE					N/A			N/A			x_distance	y_distance	N/A			N/A			N/A			N/A
// PFONT_EFFECT_DAMP					N/A			N/A			percent		N/A			N/A			N/A			N/A			N/A

	// Righty, and here's a summary of what the effects do:

// PFONT_EFFECT_DISTORT
	// This contracts one end of the rectangle of text while expanding the other. If coupled with a correctly synced
	// scaling it can give the appearance of perspective as a flat sheet of letters tilts towards the screen.

// PFONT_EFFECT_BOUNCE_IN_PHASE
	// This makes the letters bounce off of their origins in the specified direction. Like all four sine wave effects
	// the angular_period defines how close the waves are together, while the amplitude says how far the letters move
	// at their greatest offset and the offset should be increased or decreased every frame in order to animate the
	// effect. Note as angles are in the range of 0 - 2*PI (about 0 - 6.3) you should only increase it slowly. Indeed
	// all paramters for these effects should be floats generally speaking. As this effect is IN_PHASE it means that
	// the block of letters maintain their rectangular shape at all times.

// PFONT_EFFECT_BOUNCE_OUT_PHASE
	// Like the above only out of phase creating a nice wobbly-block of letters.

// PFONT_EFFECT_SINE_IN_PHASE
	// Like the bounce effect only passes through the origin instead of bouncing off of it.

// PFONT_EFFECT_SINE_OUT_PHASE
	// Like the bounce effect only passes through the origin instead of bouncing off of it.

// PFONT_EFFECT_ADD_NOISE
	// Adds random amounts up to the specified amounts on each axis. A good effect to show
	// shuddering in text as if in response to a heavy thud. A seed can be provided if you
	// want it to keep the same random offsets for several frames.

// PFONT_EFFECT_ADD_NOISE_BASELINE
	// Adds random amounts up to the specified amounts on each axis, although does not dip
	// below the access. A good effect to show shuddering in text as if in response to a heavy thud.
	// A seed can be provided if you want it to keep the same random offsets for several frames.

// PFONT_EFFECT_ROTATE
	// Rotates the text around the specified point by the given angle. Duh! :)

// PFONT_EFFECT_SCALE
	// Scales the text around the specified point by the given percentages. Um, duh, again. :)

// PFONT_EFFECT_BOW_IN
	// This sorta' wobbles the body of the text in a jolly way.

// PFONT_EFFECT_BOW_OUT
	// This sorta' wobbles the corners of the text in a odd way.

// PFONT_EFFECT_MOVE
	// Simply offsets the text by the given distance, should really be used with a soft selection
	// or something to be of any use.

// PFONT_EFFECT_DAMP
	// This simply resets everything back to it's original co-ordinates by the given percentage.
	// Mostly this is useful if you have some bonkers unreadable effects going on, on top of a nice
	// readable effect. In that situation you'd first execute the bonkers effect, then the damping, then
	// the readable effect and as you ramped up the damping the text would appear to calm down into the
	// readable effect. Once you were damping 100% you could turn off the first effect and dampening.


#endif
