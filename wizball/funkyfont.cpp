#include <allegro.h>
#include <stdio.h>
#include <math.h>
#include <string.h>

#include "funkyfont.h"
#include "math_stuff.h"

#include "fortify.h"

int selection_mode;
int selection_strength;
int selection_strength_x;
int selection_strength_y;
int inner_selection_strength;
int outer_selection_strength;
int inner_selection_type;
int outer_selection_type;
float selection_radius;
int solid_flag;
int selection_width;
int selection_height;
int selection_type;
float selection_x_scale;
float selection_y_scale;
float selection_inverse;
float selection_handle_x;
float selection_handle_y;
float selection_influence;
int selection_first_line;
int selection_last_line;
int selection_first_char;
int selection_last_char;
int selection_char_strength;
float selection_rand_values[MAX_FRAME_LIST_SIZE];
float selection_rand_min; // This is the minimum value which is included in the effect.
float selection_rand_max; // This is the maximum value which is included in the effect.
float selection_rand_bleed; // This is the extent outside of the range which is influenced to a lesser extent

/*

float MATH_abs (float value)
{

	if (value<0.0f)
		return -value;
	else
		return value;

}



float MATH_biggest (float a, float b)
{
	if (a>b)
	{
		return a;
	}
	else
	{
		return b;
	}
}



float MATH_smallest (float a, float b)
{
	if (a<b)
	{
		return a;
	}
	else
	{
		return b;
	}
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
		return lowest;
	

	int diff = highest - lowest;
	int temp_val = rand ();

	return ((temp_val % diff) + lowest);

}



float MATH_wrap (float value , float wrap)
{
	int int_val;

	if (wrap>0)
	{
		while (value < 0)
			value += (8 * wrap);

		int_val = value / wrap;
		value -= (int_val*wrap);

		return value;
	}
	else
		return 0;

}



float MATH_lerp (float value_1, float value_2, float percent)
{

	return (value_1 + ( ( value_2 - value_1 ) * percent ) );

}

*/

void PFONT_set_mode_all (float influence)
{
	// In this mode everything is selected for application of effects.
	selection_mode = PFONT_SELECTION_MODE_NORMAL;
	selection_influence = influence;
}



void PFONT_set_mode_rand (int seed, float min_range, float max_range , float bleed_range , float inverse ,  bool generate_values)
{
	// In this mode a random selection of chars is influenced, the 
	selection_mode = PFONT_SELECTION_MODE_RAND_CHARS;
	srand(unsigned(seed));
	if (generate_values == true)
	{
		int i;
		for (i=0; i<MAX_FRAME_LIST_SIZE ; i++)
		{
			selection_rand_values[i] = float (MATH_rand(0,10000)) / 10000.0f;
		}
	}
	selection_rand_min = min_range;
	selection_rand_max = max_range;
	selection_rand_bleed = bleed_range;
	selection_inverse = inverse;
	
}



void PFONT_set_mode_circle_select (float handle_x , float handle_y , int strength , int type , float inverse , float x_scale, float y_scale)
{
	// In this mode a soft selection is employed.
	selection_mode = PFONT_SELECTION_MODE_CIRCLE;
	selection_strength = strength;
	selection_handle_x = handle_x;
	selection_handle_y = handle_y;
	selection_type = type;
	selection_inverse = inverse;
	selection_x_scale = x_scale;
	selection_y_scale = y_scale;
}



void PFONT_set_mode_halo_select (float handle_x , float handle_y, int inner_strength, int outer_strength, int inner_type, int outer_type, float radius , int solid , float inverse , float x_scale , float y_scale)
{
	// In this mode all sorts of wondrous things happen! Coo!
	selection_mode = PFONT_SELECTION_MODE_HALO;
	selection_handle_x = handle_x;
	selection_handle_y = handle_y;
	inner_selection_strength = inner_strength;
	outer_selection_strength = outer_strength;
	inner_selection_type = inner_type;
	outer_selection_type = outer_type;
	selection_radius = radius;
	solid_flag = solid;
	selection_x_scale = x_scale;
	selection_y_scale = y_scale;
	selection_inverse = inverse;
}



void PFONT_set_mode_hard_rect_select (float handle_x , float handle_y , int width , int height , int strength_x , int strength_y , int type , float inverse)
{
	// In this mode a pointy-edged rectangle is used for selection.
	selection_mode = PFONT_SELECTION_MODE_HARD_RECT;
	selection_strength_x = strength_x;
	selection_strength_y = strength_y;
	selection_width = width/2;
	selection_height = height/2;
	selection_handle_x = handle_x;
	selection_handle_y = handle_y;
	selection_type = type;
	selection_inverse = inverse;
}



void PFONT_set_mode_lines_select (int start_line , int end_line , int strength , int type , float inverse)
{
	// In this selection mode the set lines are selected, however the strength modifier
	// will dampen that number of chars at either end.
	selection_mode = PFONT_SELECTION_MODE_LINES;
	selection_first_line = start_line;
	selection_last_line = end_line;
	selection_char_strength = strength;
	selection_type = type;
	selection_inverse = inverse;
}



void PFONT_set_mode_character_select (int start_character , int end_character , int strength , int type , float inverse)
{
	// In this mode only the chosen characters in the text will be affected, with the effect dying off
	// according to the strength either side of the selection.
	selection_mode = PFONT_SELECTION_MODE_CHARS;
	selection_first_char = start_character;
	selection_last_char = end_character;
	selection_char_strength = strength;
	selection_type = type;
	selection_inverse = inverse;
}



void GRAPHICS_double_masked_blit_32bit ( BITMAP *graphic_source_image , BITMAP *mask_source_image , BITMAP *dest_image , int graphic_source_x , int graphic_source_y , int mask_source_x , int mask_source_y , int dest_x , int dest_y , int width , int height )
{
	// For speed (or rather because I'm lazy) this function only checks clipping on the destination image. Feel free to alter
	// it so it checks the sources as well, but I can't be arsed. :)

	// Basically it copies those pixels of graphic_source_image which aren't masked in mask_source_image.

	unsigned long mask_colour = makecol (255,0,255);
	unsigned long *g_source;
	unsigned long *m_source;
	unsigned long *dest;
	unsigned long colour;
	
	int y;
	int x;
	int start_y = 0;
	int start_x = 0;

	if (dest_x < 0)
		start_x -= dest_x;

	if (dest_y < 0)
		start_y -= dest_y;

	if (dest_x + width > dest_image->w)
		width -= (dest_x + width) - dest_image->w;

	if (dest_y + height > dest_image->h)
		height -= (dest_y + height) - dest_image->h;

	for (y = start_y; y < height; y++)
	{
		g_source = (unsigned long *) graphic_source_image->line[y+graphic_source_y];
		m_source = (unsigned long *) mask_source_image->line[y+mask_source_y];
		dest = (unsigned long *) dest_image->line[y+dest_y];
 
		for (x = start_x; x < width; x++)
		{
			colour = (unsigned long) (m_source[x+mask_source_x]);
			if (colour != mask_colour)
			{
				dest[x+dest_x] = g_source[x+graphic_source_x];
			}
		}
	}


}



void GRAPHICS_colour_blit_32bit ( BITMAP *mask_source_image , BITMAP *dest_image , int mask_source_x , int mask_source_y , int dest_x , int dest_y , int width , int height , unsigned long text_colour )
{
	// For speed (or rather because I'm lazy) this function only checks clipping on the destination image. Feel free to alter
	// it so it checks the sources as well, but I can't be arsed. :)

	// Basically it plot the given colour to the given image for those pixels which aren't masked in mask_source_image.

	unsigned long mask_colour = makecol (255,0,255);

	unsigned long *m_source;
	unsigned long *dest;

	unsigned long colour;
	
	int y;
	int x;
	int start_y = 0;
	int start_x = 0;

	if (dest_x < 0)
		start_x -= dest_x;

	if (dest_y < 0)
		start_y -= dest_y;

	if (dest_x + width > dest_image->w)
		width -= (dest_x + width) - dest_image->w;

	if (dest_y + height > dest_image->h)
		height -= (dest_y + height) - dest_image->h;

	for (y = start_y; y < height; y++)
	{
		m_source = (unsigned long *) mask_source_image->line[y+mask_source_y];
		dest = (unsigned long *) dest_image->line[y+dest_y];
 
		for (x = start_x; x < width; x++)
		{
			colour = (unsigned long) (m_source[x+mask_source_x]);
			if (colour != mask_colour)
			{
				dest[x+dest_x] = text_colour;
			}
		}
	}

}


void GRAPHICS_colour_vert_lerped_blit_32bit ( BITMAP *mask_source_image , BITMAP *dest_image , int mask_source_x , int mask_source_y , int dest_x , int dest_y , int width , int height , unsigned long *colour_table )
{
	// For speed (or rather because I'm lazy) this function only checks clipping on the destination image. Feel free to alter
	// it so it checks the sources as well, but I can't be arsed. :)

	// Basically it plot the given colour to the given image for those pixels which aren't masked in mask_source_image. However unlike
	// the above it vertically interpolates between the two given colours as it does it so you can make nice varied text.

	unsigned long mask_colour = makecol (255,0,255);
	unsigned long text_colour;

	unsigned long *m_source;
	unsigned long *dest;

	unsigned long colour;
	
	int y;
	int x;
	int start_y = 0;
	int start_x = 0;
	float percent;

	if (dest_x < 0)
		start_x -= dest_x;

	if (dest_y < 0)
		start_y -= dest_y;

	if (dest_x + width > dest_image->w)
		width -= (dest_x + width) - dest_image->w;

	if (dest_y + height > dest_image->h)
		height -= (dest_y + height) - dest_image->h;

	for (y = start_y; y < height; y++)
	{
		m_source = (unsigned long *) mask_source_image->line[y+mask_source_y];
		dest = (unsigned long *) dest_image->line[y+dest_y];
		percent = float (y) / float (height);

		text_colour = colour_table[y];
 
		for (x = start_x; x < width; x++)
		{
			colour = (unsigned long) (m_source[x+mask_source_x]);
			if (colour != mask_colour)
			{
				dest[x+dest_x] = text_colour;
			}
		}
	}

}



void GRAPHICS_double_masked_blit_16bit ( BITMAP *graphic_source_image , BITMAP *mask_source_image , BITMAP *dest_image , int graphic_source_x , int graphic_source_y , int mask_source_x , int mask_source_y , int dest_x , int dest_y , int width , int height )
{
	// For speed (or rather because I'm lazy) this function only checks clipping on the destination image. Feel free to alter
	// it so it checks the sources as well, but I can't be arsed. :)

	// Basically it copies those pixels of graphic_source_image which aren't masked in mask_source_image.

	unsigned short mask_colour = makecol (255,0,255);
	unsigned short *g_source;
	unsigned short *m_source;
	unsigned short *dest;
	unsigned short colour;
	
	int y;
	int x;
	int start_y = 0;
	int start_x = 0;

	if (dest_x < 0)
		start_x -= dest_x;

	if (dest_y < 0)
		start_y -= dest_y;

	if (dest_x + width > dest_image->w)
		width -= (dest_x + width) - dest_image->w;

	if (dest_y + height > dest_image->h)
		height -= (dest_y + height) - dest_image->h;

	for (y = start_y; y < height; y++)
	{
		g_source = (unsigned short *) graphic_source_image->line[y+graphic_source_y];
		m_source = (unsigned short *) mask_source_image->line[y+mask_source_y];
		dest = (unsigned short *) dest_image->line[y+dest_y];
 
		for (x = start_x; x < width; x++)
		{
			colour = (unsigned short) (m_source[x+mask_source_x]);
			if (colour != mask_colour)
			{
				dest[x+dest_x] = g_source[x+graphic_source_x];
			}
		}
	}


}



void GRAPHICS_colour_blit_16bit ( BITMAP *mask_source_image , BITMAP *dest_image , int mask_source_x , int mask_source_y , int dest_x , int dest_y , int width , int height , unsigned short text_colour )
{
	// For speed (or rather because I'm lazy) this function only checks clipping on the destination image. Feel free to alter
	// it so it checks the sources as well, but I can't be arsed. :)

	// Basically it plot the given colour to the given image for those pixels which aren't masked in mask_source_image.

	unsigned short mask_colour = makecol (255,0,255);

	unsigned short *m_source;
	unsigned short *dest;

	unsigned short colour;
	
	int y;
	int x;
	int start_y = 0;
	int start_x = 0;

	if (dest_x < 0)
		start_x -= dest_x;

	if (dest_y < 0)
		start_y -= dest_y;

	if (dest_x + width > dest_image->w)
		width -= (dest_x + width) - dest_image->w;

	if (dest_y + height > dest_image->h)
		height -= (dest_y + height) - dest_image->h;

	for (y = start_y; y < height; y++)
	{
		m_source = (unsigned short *) mask_source_image->line[y+mask_source_y];
		dest = (unsigned short *) dest_image->line[y+dest_y];
 
		for (x = start_x; x < width; x++)
		{
			colour = (unsigned short) (m_source[x+mask_source_x]);
			if (colour != mask_colour)
			{
				dest[x+dest_x] = text_colour;
			}
		}
	}

}


void GRAPHICS_colour_vert_lerped_blit_16bit ( BITMAP *mask_source_image , BITMAP *dest_image , int mask_source_x , int mask_source_y , int dest_x , int dest_y , int width , int height , unsigned short *colour_table  )
{
	// For speed (or rather because I'm lazy) this function only checks clipping on the destination image. Feel free to alter
	// it so it checks the sources as well, but I can't be arsed. :)

	// Basically it plot the given colour to the given image for those pixels which aren't masked in mask_source_image. However unlike
	// the above it vertically interpolates between the two given colours as it does it so you can make nice varied text.

	unsigned short mask_colour = makecol (255,0,255);
	unsigned short text_colour;

	unsigned short *m_source;
	unsigned short *dest;

	unsigned short colour;
	
	int y;
	int x;
	int start_y = 0;
	int start_x = 0;
	float percent;

	if (dest_x < 0)
		start_x -= dest_x;

	if (dest_y < 0)
		start_y -= dest_y;

	if (dest_x + width > dest_image->w)
		width -= (dest_x + width) - dest_image->w;

	if (dest_y + height > dest_image->h)
		height -= (dest_y + height) - dest_image->h;

	for (y = start_y; y < height; y++)
	{
		m_source = (unsigned short *) mask_source_image->line[y+mask_source_y];
		dest = (unsigned short *) dest_image->line[y+dest_y];
		percent = float (y) / float (height);

		text_colour = colour_table[y];
 
		for (x = start_x; x < width; x++)
		{
			colour = (unsigned short) (m_source[x+mask_source_x]);
			if (colour != mask_colour)
			{
				dest[x+dest_x] = text_colour;
			}
		}
	}

}



void PFONT_load_texture (texture_struct *texture , char *filename , int x_overlap , int y_overlap )
{

	RGB *pal = NULL; // Not used, only dealing with 16 bit graphics mode.

	texture->texture_image = load_bitmap (filename , pal);
	texture->width = texture->texture_image->w;
	texture->height = texture->texture_image->h;
	texture->wrap_width = texture->width - x_overlap;
	texture->wrap_height = texture->height - y_overlap;

}



int PFONT_instr (char *string , char find, int start)
{

	while ( string[start] != find )
		start++;

	return start;

}



int PFONT_len (char *string)
{

	int length=0;
	while ( string[length] != '\0' )
		length++;

	return length;

}



int PFONT_get_line_length_width ( prop_text_struct *prop_text , int start , int end )
{

	int width = 0;
	int t;

	for (t = start ; t < end ; t++ )
		width += ( prop_text->prop_font->letter_widths[prop_text->text[t]-32] ) + prop_text->x_spacing ;

	return width - prop_text->x_spacing;
	// Chops off the last x_spacing otherwise the line could appear to be too long when in fact it fits very snuggly.

}



int PFONT_count_words ( prop_text_struct *prop_text , int start , int end )
{

	int counter = 0;
	int t;

	while ( prop_text->text[start] == ' ' )
		start++;

	while ( prop_text->text[end] == ' ' )
		end--;

	for ( t=start ; t<end ; t++ )
		counter = counter + (prop_text->text[t] == ' ');

	return counter+1;

}



int PFONT_get_line_length ( int pointer , prop_text_struct *prop_text , int &skipper )
{

	while ( prop_text->text[pointer] == ' ' )
		pointer ++;

	int line_width = 0;
	int last_space = -1;
	int exit_flag = 0;
	char letter;
	int value;
	int length = PFONT_len(prop_text->text);
	skipper = 0;

	do
	{

		// Every time we come across a space its position is noted so that if the next word makes the line too long,
		// we can roll back to the end of the previous word.

		if ( prop_text->text[pointer] == ' ' )
			last_space = pointer;

		// Then we increase the line_width by the width of the letter, plus the spacing between letters

		letter = prop_text->text[pointer];

		if (letter != PFONT_NEW_LINE_CHAR)
		{
			value = letter - 32;
			line_width += prop_text->prop_font->letter_widths[value];
			line_width += prop_text->x_spacing;
		}
		else
		{
			last_space = pointer;
			exit_flag = 1;
		}

		pointer++;

	} while ( (line_width <= prop_text->image_width + prop_text->x_spacing) && (pointer < length) && (exit_flag == 0) );

	// If the line is too wide for the box, or the pointer is currently not at a space (unless it's the end of the text)...

	if ( (line_width > prop_text->image_width + prop_text->x_spacing) || ( ( prop_text->text[pointer] != ' ' ) && ( pointer < length ) ) )
	{
		if (last_space != -1)
			pointer = last_space;
		else
			skipper = -1;
	}

	return pointer;

}



void PFONT_calculate_line_lengths ( prop_text_struct *prop_text )
{

	int pointer = 0;
	int line_counter = 0;
	int new_pointer;
	int length = PFONT_len(prop_text->text);
	int skipper;

	do
	{

		prop_text->line_start[line_counter] = pointer;

		new_pointer = PFONT_get_line_length ( pointer , prop_text , skipper );

		prop_text->line_length[line_counter] = (new_pointer - pointer);

		line_counter++;

		pointer = new_pointer + 1 + skipper;

	} while ( pointer < length );
		
	prop_text->total_lines = line_counter;

	prop_text->image_height = (line_counter * prop_text->prop_font->char_height) + ( (line_counter - 1) * prop_text->y_spacing );

}



void PFONT_free_font(font_struct *prop_font)
{
	if (prop_font->font_gfx != NULL) // if summats been loaded into the font.
	{
		destroy_bitmap(prop_font->font_gfx);
		prop_font->font_gfx = NULL;
	}
}



void PFONT_free_texture ( texture_struct *texture )
{
	if (texture->texture_image != NULL) // if summats been loaded into the texture.
	{
		destroy_bitmap ( texture->texture_image );
	}
}



void PFONT_load_font (font_struct *prop_font, char *filename, int frame_width, int frame_height)
{

	RGB *pal = NULL; // Not used, only dealing with 16 bit graphics mode.

	prop_font->font_gfx = load_bmp(filename,pal);
	prop_font->image_width = prop_font->font_gfx->w;
	prop_font->image_height = prop_font->font_gfx->h;
	prop_font->char_width = frame_width;
	prop_font->char_height = frame_height;
	prop_font->frames_per_row = (prop_font->image_width / prop_font->char_width);
	prop_font->frames_per_column = (prop_font->image_height / prop_font->char_height);

	int number_of_letters = prop_font->frames_per_row * prop_font->frames_per_column;

	int x;
	int y;
	int xx;
	int yy;
	int first_solid_pixel;
	int frame;
	int check_colour;
	int mask_colour = makecol (255,0,255);

	// Go through each letter one at a time and find it's width by working backwards from the right-most
	// pixel of the letter to the left-most. First non-mask pixel found wins a prize!

	for (y=0 ; y<prop_font->frames_per_column ; y++)
	{
		for (x=0 ; x<prop_font->frames_per_row ; x++)
		{

			frame = ( y * prop_font->frames_per_row ) + x;
			first_solid_pixel = -1;

			for (xx = prop_font->char_width - 1 ; xx >= 0 ; xx--)
			{
				for (yy = 0 ; yy<prop_font->char_height ; yy++)
				{
					check_colour = getpixel (prop_font->font_gfx , xx + (x * prop_font->char_width) , yy + (y * prop_font->char_height) );
					if ( (check_colour != mask_colour) && (first_solid_pixel == -1) )
						first_solid_pixel = xx;
				}
			}

			prop_font->letter_widths[frame] = first_solid_pixel + 1;

		}
	}

}



int PFONT_generate_line ( prop_text_struct *prop_text , int start , int end , int x , int y , bool include_spaces , int counter , int line_number )
{

	int t;
	int frame;
	int frame_x;
	int frame_y;
	int frame_width;

	for (t = start ; t < end ; t++)
	{
		frame = int (prop_text->text[t] - 32);
		frame_width = prop_text->prop_font->letter_widths[frame];
		if ( (frame > 0) || (include_spaces == true) )
		{

			frame_x = (frame % prop_text->prop_font->frames_per_row) * prop_text->prop_font->char_width;
			frame_y = (frame / prop_text->prop_font->frames_per_row) * prop_text->prop_font->char_height;

			prop_text->frame_list_image_x [ prop_text->frame_pointer + counter ] = frame_x;
			prop_text->frame_list_image_y [ prop_text->frame_pointer + counter ] = frame_y;
			prop_text->frame_list_base_x [ prop_text->frame_pointer + counter ] = x;
			prop_text->frame_list_base_y [ prop_text->frame_pointer + counter ] = y;
			prop_text->frame_list_new_x [ prop_text->frame_pointer + counter ] = x;
			prop_text->frame_list_new_y [ prop_text->frame_pointer + counter ] = y;
			prop_text->frame_list_frame [ prop_text->frame_pointer + counter ] = frame;
			prop_text->frame_list_line [prop_text->frame_pointer + counter ] = line_number;

			counter++;
		}

		x += (frame_width + prop_text->x_spacing );
	}

	return counter;

}



void PFONT_generate_coords ( int line_number , prop_text_struct *prop_text , bool include_spaces , bool reverse_x_order )
{

	// Oh, that keep spaces thing is for the gradual text where for timing purpose you'll probably want
	// to print the spaces as well. Also because that's how it recognises where words end if you're using
	// the add_word function. Either way, they aren't actually ever drawn.

	int generated_frames = 0;

	int word_start = prop_text->line_start[line_number];
	int word_end = word_start + prop_text->line_length[line_number];

	int line_width = PFONT_get_line_length_width ( prop_text , word_start , word_end );
	// First thing to do is find how long the line is going to be pixelwise.

	int num_words = PFONT_count_words ( prop_text , word_start , word_end );
	// And how many words are in the line.

	int alignment = prop_text->alignment;
	// And store the alignment in a temporary variable because it can be changed depending on the circumstance.

	if ( ( (num_words == 1) || (line_number == prop_text->total_lines-1) ) && (alignment == PFONT_ALIGN_JUSTIFY) )
		alignment = PFONT_ALIGN_LEFT;
	// If it's justified, but you're on the last line or there is only one word on the line then change to left align.

	switch (alignment)
	{
	case PFONT_ALIGN_LEFT:
		generated_frames = PFONT_generate_line ( prop_text , word_start , word_end , 0 , (line_number * prop_text->prop_font->char_height) + ( line_number * prop_text->y_spacing ) , include_spaces , 0 , line_number );
		break;

	case PFONT_ALIGN_CENTER:
		generated_frames = PFONT_generate_line ( prop_text , word_start , word_end , (prop_text->image_width - line_width) / 2 , (line_number * prop_text->prop_font->char_height) + ( line_number * prop_text->y_spacing ) , include_spaces , 0 , line_number );
		break;

	case PFONT_ALIGN_RIGHT:
		generated_frames = PFONT_generate_line ( prop_text , word_start , word_end , prop_text->image_width - line_width , (line_number * prop_text->prop_font->char_height) + ( line_number * prop_text->y_spacing ) , include_spaces , 0 , line_number );
		break;

	case PFONT_ALIGN_JUSTIFY:
		int word_gap = (prop_text->image_width - line_width) / (num_words-1); // extra distance between words
		// This is the number of extra pixels between each word on the line given the length of the line and the width of the image.

		int spare_pixels = (prop_text->image_width - line_width) % (num_words-1);		
		// How many pixels left over because obviously it's not always gonna' split precisely.

		int word_pointer = word_start;
		int next_word_pointer;

		int start_x = 0;

		int n;
		for (n=0 ; n<num_words ; n++)
		{
			next_word_pointer = PFONT_instr ( prop_text->text , ' ' , word_pointer );
			// Find the next word so we know the length of the word.

			generated_frames = PFONT_generate_line ( prop_text , word_pointer , next_word_pointer , start_x , (line_number * prop_text->prop_font->char_height) + ( line_number * prop_text->y_spacing ) , include_spaces , generated_frames , line_number );
			// Then draw that word.

			start_x += PFONT_get_line_length_width ( prop_text , word_pointer , next_word_pointer + 1 ) + word_gap + (n < spare_pixels) + prop_text->x_spacing;
			// Add the width of the word in pixels, plus the word gap, then one of the spare pixels if applicable and then x_spacing because PFONT_get_line_length_width cuts it off.

			word_pointer += (next_word_pointer - word_pointer) + 1;
			// Jumps over the the word and the space after it to the start of the next word.
		}
		break;
	}

	// Oh, wait there's this bit on the end. Silly me. This basically reverses the order of the just created co-ords in the frame list
	// if it is so desired. This is so that letters at the start of the sentance are drawn over the ones at the end if they happen to
	// occlude them.

	if (reverse_x_order == true)
	{
		int t;
		char temp;

		for (t=0 ; t<generated_frames ; t++)
		{
			temp = prop_text->text[ t + prop_text->frame_pointer ];
			prop_text->text[ t + prop_text->frame_pointer ] = prop_text->text[ (prop_text->frame_pointer + generated_frames) - (t+1) ];
			prop_text->text[ (prop_text->frame_pointer + generated_frames) - (t+1) ] = temp;
		}
	}

	prop_text->frame_pointer += generated_frames;
	// Then update the frame_pointer.

}



void PFONT_create_frame_list ( prop_text_struct *prop_text , font_struct *prop_font, char *words, int width, int alignment, int x_spacing, int y_spacing , bool reverse_x_order , bool reverse_y_order )
{

	prop_text->prop_font = prop_font;
	prop_text->alignment = alignment;
	prop_text->x_spacing = x_spacing;
	prop_text->y_spacing = y_spacing;
	prop_text->image_width = width;
	prop_text->frame_pointer = 0;

	// Then start plopping new ones in. :)

	strcpy (prop_text->text , words);

	PFONT_calculate_line_lengths (prop_text);

	int l;

	if (reverse_y_order == false)
	{
		for (l=0 ; l<prop_text->total_lines ; l++)
			PFONT_generate_coords ( l , prop_text , false , reverse_x_order);
	}
	else
	{
		for (l=prop_text->total_lines-1 ; l>=0 ; l--)
			PFONT_generate_coords ( l , prop_text , false , reverse_x_order);
	}

	int t;
	for (t=0; t<=prop_text->total_lines ; t++)
	{
		prop_text->line_frame_start[t]=-1;
		prop_text->line_frame_length[t]=0;
	}

	int frame;
	
	for (frame = 0 ; frame < prop_text->frame_pointer ; frame++)
	{
		if (prop_text->line_frame_start[prop_text->frame_list_line[frame]] == -1)
		{
			prop_text->line_frame_start[prop_text->frame_list_line[frame]] = frame;
		}
		prop_text->line_frame_length[prop_text->frame_list_line[frame]]++;
	}

}



void PFONT_posset_reset ( prop_text_struct *prop_text )
{
	// This just copies the base co-ords into the new co-ords and presents the letters "as-is" and resets
	// everything in readiness to apply all the other effects.

	int frame;

	for (frame=0 ; ( frame<prop_text->frame_pointer ) ; frame++ )
	{
		prop_text->frame_list_new_x[frame] = prop_text->frame_list_base_x[frame];
		prop_text->frame_list_new_y[frame] = prop_text->frame_list_base_y[frame];
	}

}



// These functions all modify the resetted co-ords of the frame list and so are all
// called PFONT_posmod_summat. "posmod" means position modification. To perform these
// functions you must first have first called PFONT_posset_reset function as otherwise their affect
// on the co-ordinates will accumulate over the course of several frames and although this may
// be desirable it won't be long until errors creep in due to degradation of nth generation data.

// It'll also probably make your letters explode or implode if you're doing any scaling.

// Note that the order in which you use the modifiers is very important as like vector maths,
// SCALE then ROTATE is not the same as ROTATE then SCALE. Things like the sine wave effects
// and soft selection are probably best done first.

// Note that all effects which use handles any the like have their figures based on the initial
// base rectangle formed by the base rectangle.



void PFONT_effect ( prop_text_struct *prop_text , int effect_number , bool effect_texture , float handle_x , float handle_y , float param_1 , float param_2 , float param_3 , float param_4 , float param_5 , float param_6 )
{
	float offset_x = ( ( prop_text->image_width - prop_text->prop_font->char_width ) * handle_x );
	float offset_y = ( ( prop_text->image_height - prop_text->prop_font->char_height ) * handle_y );

	float distance;
	float influence;
	float influence_x;
	float influence_y;
	float x;
	float y;
	int frame;
	int line_number;
	int line_start;
	int line_end;
	float vertical_percent;
	float horizontal_percent;
	float vertical_influence;
	float horizontal_influence;
	float current_angle;
	float temp_x;
	float temp_y;
	float bow_x;
	float bow_y;

	float x_angular_period = param_1;
	float y_angular_period = param_2;
	float expand = param_1;
	float x_amplitude = param_3;
	float y_amplitude = param_4;
	float x_angle_modifier = param_5;
	float y_angle_modifier = param_6;
	float drag_x = param_1;
	float drag_y = param_2;
	float horizontal_deviation = param_1;
	float vertical_deviation = param_2;
	float x_noise = param_1;
	float y_noise = param_2;
	int seed = int (param_3);
	int flip = int (param_4);
	float angle = param_1;
	float percent_x = param_1;
	float percent_y = param_2;
	float max_bow_x = param_1;
	float max_bow_y = param_2;
	float damping = param_1;
	int quantised = int (param_3);

	srand(unsigned(seed));

	for (frame=0 ; ( frame < prop_text->frame_pointer ) ; frame++ )
	{
		x = ( prop_text->frame_list_base_x[frame] - ( ( prop_text->image_width - prop_text->prop_font->char_width ) * selection_handle_x ) );
		y = ( prop_text->frame_list_base_y[frame] - ( ( prop_text->image_height - prop_text->prop_font->char_height ) * selection_handle_y ) );

		// Calculated the influence of the effect at this point...

		switch (selection_mode)
		{

		case PFONT_SELECTION_MODE_NORMAL:
			influence = selection_influence;
			break;

		case PFONT_SELECTION_MODE_CIRCLE:
			temp_x = x / selection_x_scale;
			temp_y = y / selection_y_scale;
			distance = sqrt ( ( temp_x * temp_x ) + ( temp_y * temp_y ) ); // distance of letter from handle.

			switch (selection_type)
			{
			case PFONT_SOFT_SELECTION:
				if (distance < selection_strength) // if it's within the area of effect of the "bubble"
				{
					influence = (selection_strength - distance) / selection_strength; // linear value at this point
					influence *= (3.14597); // now a nice soft value.
					influence = (1-cos (influence))/2;
				}
				else
				{
					influence = 0;
				}
				break;

			case PFONT_BUBBLE_SELECTION:
				if (distance < selection_strength) // if it's within the area of effect of the "bubble"
				{
					influence = (1-(selection_strength - distance) / selection_strength); // linear value at this point
					influence *= influence; // now a nice bubble value.
					influence = -influence+1;
				}
				else
				{
					influence = 0;
				}
				break;

			case PFONT_POINT_SELECTION:
				if (distance < selection_strength) // if it's within the area of effect of the "bubble"
				{
					influence = (selection_strength - distance) / selection_strength; // linear value at this point
					influence *= influence; // now a nice inverse bubble value.
				}
				else
				{
					influence = 0;
				}
				break;

			case PFONT_LINEAR_SELECTION:
				if (distance < selection_strength) // if it's within the area of effect of the "bubble"
				{
					influence = (selection_strength - distance) / selection_strength; // linear value at this point
				}
				else
				{
					influence = 0;
				}
				break;

			default:
				break;
			}

			influence = ((influence - 0.5) * selection_inverse) + 0.5;

			break;

		case PFONT_SELECTION_MODE_HALO:
			temp_x = x / selection_x_scale;
			temp_y = y / selection_y_scale;
			distance = sqrt ( ( temp_x * temp_x ) + ( temp_y * temp_y ) ); // distance of letter from handle.

			if (distance < selection_radius) // It's inside the halo.
			{
				if (solid_flag != INNER_SOLID)
				{
					switch (inner_selection_type)
					{
					case PFONT_SOFT_SELECTION:
						if (selection_radius - distance < inner_selection_strength) // if it's within the area of effect of the "bubble"
						{
							influence = (inner_selection_strength - (selection_radius - distance) ) / inner_selection_strength; // linear value at this point
							influence *= (3.14597); // now a nice soft value.
							influence = (1-cos (influence))/2;
						}
						else
						{
							influence = 0;
						}
						break;

					case PFONT_BUBBLE_SELECTION:
						if (selection_radius - distance < inner_selection_strength) // if it's within the area of effect of the "bubble"
						{
							influence = (1 - ( inner_selection_strength - (selection_radius - distance) ) / inner_selection_strength); // linear value at this point
							influence *= influence; // now a nice bubble value.
							influence = -influence+1;
						}
						else
						{
							influence = 0;
						}
						break;

					case PFONT_POINT_SELECTION:
						if (selection_radius - distance < inner_selection_strength) // if it's within the area of effect of the "bubble"
						{
							influence = (inner_selection_strength - (selection_radius - distance) ) / inner_selection_strength; // linear value at this point
							influence *= influence; // now a nice inverse bubble value.
						}
						else
						{
							influence = 0;
						}
						break;

					case PFONT_LINEAR_SELECTION:
						if (selection_radius - distance < inner_selection_strength) // if it's within the area of effect of the "bubble"
						{
							influence = (inner_selection_strength - (selection_radius - distance) ) / inner_selection_strength; // linear value at this point
						}
						else
						{
							influence = 0;
						}
						break;

					default:
						break;
					}
				}
				else
				{
					influence = 1;
				}
			}
			else // It's outside the halo!
			{
				if (solid_flag != OUTER_SOLID)
				{
					switch (outer_selection_type)
					{
					case PFONT_SOFT_SELECTION:
						if (distance - selection_radius < outer_selection_strength) // if it's within the area of effect of the "bubble"
						{
							influence = (outer_selection_strength - (distance - selection_radius) ) / outer_selection_strength; // linear value at this point
							influence *= (3.14597); // now a nice soft value.
							influence = (1-cos (influence))/2;
						}
						else
						{
							influence = 0;
						}
						break;

					case PFONT_BUBBLE_SELECTION:
						if (distance - selection_radius < outer_selection_strength) // if it's within the area of effect of the "bubble"
						{
							influence = (1 - ( outer_selection_strength - (distance - selection_radius) ) / outer_selection_strength); // linear value at this point
							influence *= influence; // now a nice bubble value.
							influence = -influence+1;
						}
						else
						{
							influence = 0;
						}
						break;

					case PFONT_POINT_SELECTION:
						if (distance - selection_radius < outer_selection_strength) // if it's within the area of effect of the "bubble"
						{
							influence = (outer_selection_strength - (distance - selection_radius) ) / outer_selection_strength; // linear value at this point
							influence *= influence; // now a nice inverse bubble value.
						}
						else
						{
							influence = 0;
						}
						break;

					case PFONT_LINEAR_SELECTION:
						if (distance - selection_radius < outer_selection_strength) // if it's within the area of effect of the "bubble"
						{
							influence = (outer_selection_strength - (distance - selection_radius) ) / outer_selection_strength; // linear value at this point
						}
						else
						{
							influence = 0;
						}
						break;

					default:
						break;
					}
				}
				else
				{
					influence = 1;
				}
			}

			influence = ((influence - 0.5) * selection_inverse) + 0.5;

			break;

		case PFONT_SELECTION_MODE_HARD_RECT:
		case PFONT_SELECTION_MODE_SOFT_RECT:
			if ( MATH_abs(x) < selection_width ) // ie, it's within the horizontal bounds of the box
			{
				if ( MATH_abs(y) < selection_height ) // ie, it's within the vertical bounds of the box (therefor it's in the middle)
				{
					influence = 1;
				}
				else // it's either above or below the box...
				{
					distance = MATH_abs(y) - selection_height;
					if (distance < selection_strength_y)
					{
						influence = (selection_strength_y - distance) / selection_strength_y;
					}
					else
					{
						influence = 0;
					}
				}

			}
			else // It's either to the left or right of the box...
			{
				if ( MATH_abs(y) < selection_height ) // ie, it's within the vertical bounds of the box but to either side...
				{
					distance = MATH_abs(x) - selection_width;
					if (distance < selection_strength_x)
					{
						influence = (selection_strength_x - distance) / selection_strength_x;
					}
					else
					{
						influence = 0;
					}
				}
				else // it's in a corner! Lawks!
				{
					distance = MATH_abs(x) - selection_width;
					if (distance < selection_strength_x)
					{
						influence_x = (selection_strength_x - distance) / selection_strength_x;
					}
					else
					{
						influence_x = 0;
					}

					distance = MATH_abs(y) - selection_height;
					if (distance < selection_strength_y)
					{
						influence_y = (selection_strength_y - distance) / selection_strength_y;
					}
					else
					{
						influence_y = 0;
					}
					
					influence = MATH_smallest (influence_x,influence_y);
					
				}

			}

			switch (selection_type)
			{
			case PFONT_SOFT_SELECTION:
				influence *= (3.14597); // now a nice soft value.
				influence = (1-cos (influence))/2;
				break;

			case PFONT_BUBBLE_SELECTION:
				influence = 1-influence; // linear value at this point
				influence *= influence; // now a nice bubble value.
				influence = -influence+1;
				break;

			case PFONT_POINT_SELECTION:
				influence *= influence; // now a nice inverse bubble value.
				break;

			case PFONT_LINEAR_SELECTION:
				// fook all!
				break;

			default:
				break;
			}

			influence = ((influence - 0.5) * selection_inverse) + 0.5;

			break;

		case PFONT_SELECTION_MODE_LINES:
			line_number = prop_text->frame_list_line[frame];
			line_start = prop_text->line_frame_start[line_number];
			line_end = line_start + prop_text->line_frame_length[line_number] - 1;

			if ((line_number >= selection_first_line) && (line_number <= selection_last_line))
			{
				if (frame < line_start + selection_char_strength)
				{
					influence = float (frame - line_start) / selection_char_strength;
				}
				else if (frame > line_end - selection_char_strength)
				{
					influence = float (line_end - frame) / selection_char_strength;
				}
				else
				{
					influence = 1;
				}
			}
			else
			{
				influence = 0;
			}

			switch (selection_type)
			{
			case PFONT_SOFT_SELECTION:
				influence *= (3.14597); // now a nice soft value.
				influence = (1-cos (influence))/2;
				break;

			case PFONT_BUBBLE_SELECTION:
				influence = 1-influence; // linear value at this point
				influence *= influence; // now a nice bubble value.
				influence = -influence+1;
				break;

			case PFONT_POINT_SELECTION:
				influence *= influence; // now a nice inverse bubble value.
				break;

			case PFONT_LINEAR_SELECTION:
				// fook all!
				break;

			default:
				break;
			}

			influence = ((influence - 0.5) * selection_inverse) + 0.5;

			break;

		case PFONT_SELECTION_MODE_CHARS:
			if ((frame >= selection_first_char) && (frame <= selection_last_char))
			{
				if (frame < selection_first_char + selection_char_strength)
				{
					influence = float (frame - selection_first_char) / selection_char_strength;
				}
				else if (frame > selection_last_char - selection_char_strength)
				{
					influence = float (selection_last_char - frame) / selection_char_strength;
				}
				else
				{
					influence = 1;
				}
			}
			else
			{
				influence = 0;
			}

			switch (selection_type)
			{
			case PFONT_SOFT_SELECTION:
				influence *= (3.14597); // now a nice soft value.
				influence = (1-cos (influence))/2;
				break;

			case PFONT_BUBBLE_SELECTION:
				influence = 1-influence; // linear value at this point
				influence *= influence; // now a nice bubble value.
				influence = -influence+1;
				break;

			case PFONT_POINT_SELECTION:
				influence *= influence; // now a nice inverse bubble value.
				break;

			case PFONT_LINEAR_SELECTION:
				// fook all!
				break;

			default:
				break;
			}

			influence = ((influence - 0.5) * selection_inverse) + 0.5;

			break;

		case PFONT_SELECTION_MODE_RAND_CHARS:
			if ( ( selection_rand_values[frame] >= selection_rand_min ) && ( selection_rand_values[frame] <= selection_rand_max ) )
			{
				influence = 1;
			}
			else if ( ( selection_rand_values[frame] <= selection_rand_min ) && ( selection_rand_values[frame] >= selection_rand_min - selection_rand_bleed ) )
			{
				influence = 1 - (selection_rand_min - selection_rand_values[frame]) / selection_rand_bleed;
			}
			else if ( ( selection_rand_values[frame] >= selection_rand_max ) && ( selection_rand_values[frame] <= selection_rand_max + selection_rand_bleed ) )
			{
				influence = 1 - (selection_rand_values[frame] - selection_rand_max) / selection_rand_bleed;
			}
			else
			{
				influence = 0;
			}

			influence = ((influence - 0.5) * selection_inverse) + 0.5;

			break;

		default:
			break;

		}

		// Righty, that's the influence calculated (phew!)... Now we actually do the effect based on which one
		// was chosen...
		
		if (influence>0)
		{
			if (effect_texture == false) // The effect applies to the letter co-ordinates.
			{
				switch (effect_number)
				{
				case PFONT_EFFECT_DISTORT:
					horizontal_percent = float (prop_text->frame_list_base_x[frame] - offset_x) / float (prop_text->image_width);
					vertical_percent = float (prop_text->frame_list_base_y[frame] - offset_y) / float (prop_text->image_height);
					horizontal_influence = horizontal_percent * horizontal_deviation;
					vertical_influence = vertical_percent * vertical_deviation;
					prop_text->frame_list_new_x[frame] += influence * ((prop_text->frame_list_base_x[frame] - offset_x) * vertical_influence);
					prop_text->frame_list_new_y[frame] += influence * ((prop_text->frame_list_base_y[frame] - offset_y) * horizontal_influence);
					break;

				case PFONT_EFFECT_BOUNCE_IN_PHASE:
					prop_text->frame_list_new_x[frame] += influence * (x_amplitude * MATH_abs (sin ( x_angle_modifier + (prop_text->frame_list_new_x[frame] * x_angular_period) + (prop_text->frame_list_new_x[frame] * y_angular_period))));
					prop_text->frame_list_new_y[frame] += influence * (y_amplitude * MATH_abs (sin ( y_angle_modifier + (prop_text->frame_list_new_y[frame] * y_angular_period) + (prop_text->frame_list_new_y[frame] * x_angular_period))));
					break;

				case PFONT_EFFECT_BOUNCE_OUT_PHASE:
					prop_text->frame_list_new_x[frame] += influence * (x_amplitude * MATH_abs (sin ( x_angle_modifier + (prop_text->frame_list_new_y[frame] * x_angular_period) + (prop_text->frame_list_new_x[frame] * y_angular_period))));
					prop_text->frame_list_new_y[frame] += influence * (y_amplitude * MATH_abs (sin ( y_angle_modifier + (prop_text->frame_list_new_x[frame] * y_angular_period) + (prop_text->frame_list_new_y[frame] * x_angular_period))));
					break;

				case PFONT_EFFECT_SINE_IN_PHASE:
					prop_text->frame_list_new_x[frame] += influence * (x_amplitude * sin ( x_angle_modifier + (prop_text->frame_list_new_x[frame] * x_angular_period)));
					prop_text->frame_list_new_y[frame] += influence * (y_amplitude * sin ( y_angle_modifier + (prop_text->frame_list_new_y[frame] * y_angular_period)));
					break;

				case PFONT_EFFECT_SINE_OUT_PHASE:
					prop_text->frame_list_new_x[frame] += influence * (x_amplitude * sin ( x_angle_modifier + (prop_text->frame_list_new_y[frame] * x_angular_period)));
					prop_text->frame_list_new_y[frame] += influence * (y_amplitude * sin ( y_angle_modifier + (prop_text->frame_list_new_x[frame] * y_angular_period)));
					break;

				case PFONT_EFFECT_ADD_NOISE:
					prop_text->frame_list_new_x[frame] += influence * ( MATH_rand ( int (- x_noise) , int (x_noise) ) ) * flip;
					prop_text->frame_list_new_y[frame] += influence * ( MATH_rand ( int (- y_noise) , int (y_noise) ) ) * flip;
					break;

				case PFONT_EFFECT_ADD_NOISE_BASELINE:
					prop_text->frame_list_new_x[frame] += influence * ( MATH_rand ( 0 , int (x_noise) ) ) * flip;
					prop_text->frame_list_new_y[frame] += influence * ( MATH_rand ( 0 , int (y_noise) ) ) * flip;
					break;

				case PFONT_EFFECT_ROTATE:
					distance = sqrt ( ( ( prop_text->frame_list_new_x[frame] - offset_x ) * ( prop_text->frame_list_new_x[frame] - offset_x ) ) + ( ( prop_text->frame_list_new_y[frame] - offset_y ) * ( prop_text->frame_list_new_y[frame] - offset_y ) ) );
					current_angle = atan2 ( ( prop_text->frame_list_new_x[frame] - offset_x ) , ( prop_text->frame_list_new_y[frame] - offset_y ) );
					prop_text->frame_list_new_x[frame] = distance * sin ( current_angle + (angle * influence) ) + offset_x;
					prop_text->frame_list_new_y[frame] = distance * cos ( current_angle + (angle * influence) ) + offset_y;
					break;

				case PFONT_EFFECT_SCALE:
					temp_x = ( ( ( prop_text->frame_list_new_x[frame] - offset_x ) * (percent_x) ) + offset_x) - (prop_text->frame_list_new_x[frame]);
					temp_y = ( ( ( prop_text->frame_list_new_y[frame] - offset_y ) * (percent_y) ) + offset_y) - (prop_text->frame_list_new_y[frame]);
					prop_text->frame_list_new_x[frame] += temp_x * influence;
					prop_text->frame_list_new_y[frame] += temp_y * influence;
					break;

				case PFONT_EFFECT_BOW_IN:
					temp_x = ( prop_text->frame_list_new_x[frame] - offset_x ) / (prop_text->image_width / 2);
					temp_y = ( prop_text->frame_list_new_y[frame] - offset_y ) / (prop_text->image_height / 2);
					bow_y = max_bow_x - (temp_x * temp_x) * max_bow_x;
					bow_x = max_bow_y - (temp_y * temp_y) * max_bow_y;
					prop_text->frame_list_new_x[frame] += influence * bow_x;
					prop_text->frame_list_new_y[frame] += influence * bow_y;
					break;

				case PFONT_EFFECT_BOW_OUT:
					temp_x = ( prop_text->frame_list_new_x[frame] - offset_x ) / (prop_text->image_width / 2);
					temp_y = ( prop_text->frame_list_new_y[frame] - offset_y ) / (prop_text->image_height / 2);
					bow_y = (temp_x * temp_x) * max_bow_x;
					bow_x = (temp_y * temp_y) * max_bow_y;
					prop_text->frame_list_new_x[frame] += influence * bow_x;
					prop_text->frame_list_new_y[frame] += influence * bow_y;
					break;

				case PFONT_EFFECT_MOVE:
					if (quantised == 0)
					{
						prop_text->frame_list_new_x[frame] += influence * drag_x;
						prop_text->frame_list_new_y[frame] += influence * drag_y;
					}
					else
					{
						prop_text->frame_list_new_x[frame] += int (influence * drag_x) % quantised;
						prop_text->frame_list_new_y[frame] += int (influence * drag_y) % quantised;
					}
					break;

				case PFONT_EFFECT_DAMP:
					prop_text->frame_list_new_x[frame] = MATH_lerp (prop_text->frame_list_base_x[frame] , prop_text->frame_list_new_x[frame] , damping * influence );
					prop_text->frame_list_new_y[frame] = MATH_lerp (prop_text->frame_list_base_y[frame] , prop_text->frame_list_new_y[frame] , damping * influence );
					break;

				case PFONT_EFFECT_EXPLODE:
					temp_x = ( prop_text->frame_list_new_x[frame] - offset_x );
					temp_y = ( prop_text->frame_list_new_y[frame] - offset_y );
					distance = sqrt( (temp_x*temp_x) + (temp_y*temp_y) );
					if (distance == 0)
					{
						distance = 1;
					}
					temp_x /= distance;
					temp_y /= distance;
					prop_text->frame_list_new_x[frame] += temp_x * expand * influence;
					prop_text->frame_list_new_y[frame] += temp_y * expand * influence;
					break;

				default:
					break;
				}
			}
			else // Effect is on the texture
			{
				switch (effect_number)
				{
				case PFONT_EFFECT_DISTORT:
					horizontal_percent = float (prop_text->frame_list_base_x[frame] - offset_x) / float (prop_text->image_width);
					vertical_percent = float (prop_text->frame_list_base_y[frame] - offset_y) / float (prop_text->image_height);
					horizontal_influence = horizontal_percent * horizontal_deviation;
					vertical_influence = vertical_percent * vertical_deviation;
					prop_text->frame_list_texture_x[frame] += influence * ((prop_text->frame_list_base_x[frame] - offset_x) * vertical_influence);
					prop_text->frame_list_texture_y[frame] += influence * ((prop_text->frame_list_base_y[frame] - offset_y) * horizontal_influence);
					break;

				case PFONT_EFFECT_BOUNCE_IN_PHASE:
					prop_text->frame_list_texture_x[frame] += influence * (x_amplitude * MATH_abs (sin ( x_angle_modifier + (prop_text->frame_list_new_x[frame] * x_angular_period) + (prop_text->frame_list_new_x[frame] * y_angular_period))));
					prop_text->frame_list_texture_y[frame] += influence * (y_amplitude * MATH_abs (sin ( y_angle_modifier + (prop_text->frame_list_new_y[frame] * y_angular_period) + (prop_text->frame_list_new_y[frame] * x_angular_period))));
					break;

				case PFONT_EFFECT_BOUNCE_OUT_PHASE:
					prop_text->frame_list_texture_x[frame] += influence * (x_amplitude * MATH_abs (sin ( x_angle_modifier + (prop_text->frame_list_new_y[frame] * x_angular_period) + (prop_text->frame_list_new_x[frame] * y_angular_period))));
					prop_text->frame_list_texture_y[frame] += influence * (y_amplitude * MATH_abs (sin ( y_angle_modifier + (prop_text->frame_list_new_x[frame] * y_angular_period) + (prop_text->frame_list_new_y[frame] * x_angular_period))));
					break;

				case PFONT_EFFECT_SINE_IN_PHASE:
					prop_text->frame_list_texture_x[frame] += influence * (x_amplitude * sin ( x_angle_modifier + (prop_text->frame_list_new_x[frame] * x_angular_period)));
					prop_text->frame_list_texture_y[frame] += influence * (y_amplitude * sin ( y_angle_modifier + (prop_text->frame_list_new_y[frame] * y_angular_period)));
					break;

				case PFONT_EFFECT_SINE_OUT_PHASE:
					prop_text->frame_list_texture_x[frame] += influence * (x_amplitude * sin ( x_angle_modifier + (prop_text->frame_list_new_y[frame] * x_angular_period)));
					prop_text->frame_list_texture_y[frame] += influence * (y_amplitude * sin ( y_angle_modifier + (prop_text->frame_list_new_x[frame] * y_angular_period)));
					break;

				case PFONT_EFFECT_ADD_NOISE:
					prop_text->frame_list_texture_x[frame] += influence * ( MATH_rand ( int (- x_noise) , int (x_noise) ) );
					prop_text->frame_list_texture_y[frame] += influence * ( MATH_rand ( int (- y_noise) , int (y_noise) ) );
					break;

				case PFONT_EFFECT_ADD_NOISE_BASELINE:
					prop_text->frame_list_texture_x[frame] += influence * ( MATH_rand ( 0 , int (x_noise) ) );
					prop_text->frame_list_texture_y[frame] += influence * ( MATH_rand ( 0 , int (y_noise) ) );
					break;

				case PFONT_EFFECT_ROTATE:
					distance = sqrt ( ( ( prop_text->frame_list_new_x[frame] - offset_x ) * ( prop_text->frame_list_new_x[frame] - offset_x ) ) + ( ( prop_text->frame_list_new_y[frame] - offset_y ) * ( prop_text->frame_list_new_y[frame] - offset_y ) ) );
					current_angle = atan2 ( ( prop_text->frame_list_new_x[frame] - offset_x ) , ( prop_text->frame_list_new_y[frame] - offset_y ) );
					prop_text->frame_list_texture_x[frame] = distance * sin ( current_angle + (angle * influence) );
					prop_text->frame_list_texture_y[frame] = distance * cos ( current_angle + (angle * influence) );
					break;

				case PFONT_EFFECT_SCALE:
					temp_x = ( ( ( prop_text->frame_list_texture_x[frame] - offset_x ) * (percent_x) ) + offset_x) - (prop_text->frame_list_texture_x[frame]);
					temp_y = ( ( ( prop_text->frame_list_texture_y[frame] - offset_y ) * (percent_y) ) + offset_y) - (prop_text->frame_list_texture_y[frame]);
					prop_text->frame_list_texture_x[frame] += temp_x * influence;
					prop_text->frame_list_texture_y[frame] += temp_y * influence;
					break;

				case PFONT_EFFECT_BOW_IN:
					temp_x = ( prop_text->frame_list_new_x[frame] - offset_x ) / (prop_text->image_width / 2);
					temp_y = ( prop_text->frame_list_new_y[frame] - offset_y ) / (prop_text->image_height / 2);
					bow_y = max_bow_x - (temp_x * temp_x) * max_bow_x;
					bow_x = max_bow_y - (temp_y * temp_y) * max_bow_y;
					prop_text->frame_list_texture_x[frame] += influence * (bow_x + bow_y);
					prop_text->frame_list_texture_y[frame] += influence * (bow_x + bow_y);
					break;

				case PFONT_EFFECT_BOW_OUT:
					temp_x = ( prop_text->frame_list_new_x[frame] - offset_x ) / (prop_text->image_width / 2);
					temp_y = ( prop_text->frame_list_new_y[frame] - offset_y ) / (prop_text->image_height / 2);
					bow_y = (temp_x * temp_x) * max_bow_x;
					bow_x = (temp_y * temp_y) * max_bow_y;
					prop_text->frame_list_texture_x[frame] += influence * (bow_x + bow_y);
					prop_text->frame_list_texture_y[frame] += influence * (bow_x + bow_y);
					break;

				case PFONT_EFFECT_MOVE:
					if (quantised == 0)
					{
						prop_text->frame_list_texture_x[frame] += influence * drag_x;
						prop_text->frame_list_texture_y[frame] += influence * drag_y;
					}
					else
					{
						prop_text->frame_list_texture_x[frame] += int (influence * drag_x) % quantised;
						prop_text->frame_list_texture_y[frame] += int (influence * drag_y) % quantised;
					}
					break;

				case PFONT_EFFECT_DAMP:
					prop_text->frame_list_texture_x[frame] = MATH_lerp (0 , prop_text->frame_list_texture_x[frame] , damping * influence );
					prop_text->frame_list_texture_y[frame] = MATH_lerp (0 , prop_text->frame_list_texture_y[frame] , damping * influence );
					break;

				case PFONT_EFFECT_EXPLODE:
					temp_x = ( prop_text->frame_list_texture_x[frame] - offset_x );
					temp_y = ( prop_text->frame_list_texture_y[frame] - offset_y );
					distance = sqrt( (temp_x*temp_x) + (temp_y*temp_y) );
					if (distance == 0)
					{
						distance = 1;
					}
					temp_x /= distance;
					temp_y /= distance;
					prop_text->frame_list_texture_x[frame] += temp_x * expand * influence;
					prop_text->frame_list_texture_y[frame] += temp_y * expand * influence;
					break;

				default:
					break;
				}
			}
		}

	}

}



// These functions all modify the texture co-ords of the letters in case you are wanting to
// draw them using a texture instead of their standard appearance. The effects are largely
// similar to those used to manipulate the co-ordinates of the letters themselves, However
// unlike those effects these aren't really designed to be used in a cumulative fashion as
// each one will generally completely generate the co-ordinates from scratch. The exceptions
// to this rule are SCALE and ROTATE which can be safely added after any effect however as
// you're talking about texture effects they might not act quite as you expect. Just try
// some stuff out.

// All functions are prefaced with either "texset" which means it creates all new texture
// co-ordinates or "texmod" which will modify the current texture co-ordinates.

// Note that you don't need to specify a texture at all when doing these effects as it is
// irrelevent. Any co-ordinates which fall outside the scope of the texture used when drawing
// the actual item will be modulussed (there's a new one for the Oxford English) back into
// the correct bounds.

// Generally you'll only be working with either vertical gradient textures or horizontal texture
// gradients I would imagine and so when you're creating the basic spread you can opt to fold
// the effects of both axis into each axis. I'd explain that in english but you're best off
// just giving it a whizz to see how it effects things.

// All effects are based on the current co-ordinates of the text, not the base co-ordinates
// so when you call these functions in the sequence of creating an effect is very important!
// That said, almost anything looks really funky when you're working with gradients so I
// wouldn't sorry too much.


void PFONT_texset_basic_spread ( prop_text_struct *prop_text , float x_spread , float y_spread , float x_base , float y_base , bool convert )
{
	// This just allocates a texture co-ordinate based on the position of the letter on each
	// axis. It's a simple linear spread. If you want all texture co-ords to be 0,0 then just
	// specify an x_spread and y_spread of 0.

	int frame;

	for (frame=0 ; ( frame<prop_text->frame_pointer ) ; frame++ )
	{
		prop_text->frame_list_texture_x[frame] = x_base + (prop_text->frame_list_new_x[frame] * x_spread);
		if (convert = false)
			prop_text->frame_list_texture_y[frame] = y_base + (prop_text->frame_list_new_y[frame] * y_spread);
		else
			prop_text->frame_list_texture_x[frame] += y_base + (prop_text->frame_list_new_y[frame] * y_spread);
	}

}



void PFONT_find_extremes (prop_text_struct *prop_text)
{

	prop_text->top_left_x = 0;
	prop_text->top_left_y = 0;
	prop_text->bottom_right_x = 0;
	prop_text->bottom_right_y = 0;

	int frame;

	for (frame=0 ; ( frame<prop_text->frame_pointer ) ; frame++ )
	{
		if ( prop_text->frame_list_new_x[frame] < prop_text->top_left_x )
			prop_text->top_left_x = prop_text->frame_list_new_x[frame];

		if ( prop_text->frame_list_new_y[frame] < prop_text->top_left_y )
			prop_text->top_left_y = prop_text->frame_list_new_y[frame];

		if ( prop_text->frame_list_new_x[frame] + prop_text->prop_font->char_width > prop_text->bottom_right_x )
			prop_text->bottom_right_x = prop_text->frame_list_new_x[frame] + prop_text->prop_font->char_width;

		if ( prop_text->frame_list_new_y[frame] + prop_text->prop_font->char_width > prop_text->bottom_right_y )
			prop_text->bottom_right_y = prop_text->frame_list_new_y[frame] + prop_text->prop_font->char_height;

	}
}



// All functions that draw to the screen or a bitmap surface...



void PFONT_draw_frame_list_notexture ( prop_text_struct *prop_text , int min , int max , int min_max_type , int x , int y , float handle_x , float handle_y , BITMAP *buffer_image )
{
	int frame;

	int offset_x = ( ( prop_text->image_width ) * handle_x );
	int offset_y = ( ( prop_text->image_height ) * handle_y );

	x = x - offset_x;
	y = y - offset_y;

	if (min_max_type == PFONT_MINMAX_IGNORE) // ignore the values...
	{
		for (frame=0 ; frame<prop_text->frame_pointer ; frame++ )
		{
			masked_blit ( prop_text->prop_font->font_gfx , buffer_image , prop_text->frame_list_image_x[frame] , prop_text->frame_list_image_y[frame] , x + prop_text->frame_list_new_x[frame] , y + prop_text->frame_list_new_y[frame] , prop_text->prop_font->char_width , prop_text->prop_font->char_height );
		}
	}
	else if (min_max_type == PFONT_MINMAX_CHARS)
	{
		for (frame=min ; ( (frame<max) && (frame<prop_text->frame_pointer) ) ; frame++ )
		{
			masked_blit ( prop_text->prop_font->font_gfx , buffer_image , prop_text->frame_list_image_x[frame] , prop_text->frame_list_image_y[frame] , x + prop_text->frame_list_new_x[frame] , y + prop_text->frame_list_new_y[frame] , prop_text->prop_font->char_width , prop_text->prop_font->char_height );
		}
	}
	else
	{
		for (frame=prop_text->line_frame_start[min] ; ( (frame<prop_text->line_frame_start[max]+prop_text->line_frame_length[max]) && (frame<prop_text->frame_pointer) ) ; frame++ )
		{
			masked_blit ( prop_text->prop_font->font_gfx , buffer_image , prop_text->frame_list_image_x[frame] , prop_text->frame_list_image_y[frame] , x + prop_text->frame_list_new_x[frame] , y + prop_text->frame_list_new_y[frame] , prop_text->prop_font->char_width , prop_text->prop_font->char_height );
		}
	}

}



void PFONT_draw_frame_list_texture ( prop_text_struct *prop_text , int min , int max , int min_max_type , int x , int y , float handle_x , float handle_y , BITMAP *buffer_image , texture_struct *texture )
{
	int frame;

	int offset_x = ( ( prop_text->image_width ) * handle_x );
	int offset_y = ( ( prop_text->image_height ) * handle_y );

	x = x - offset_x;
	y = y - offset_y;

	for (frame = 0 ; frame < prop_text->frame_pointer ; frame++)
	{
		prop_text->frame_list_texture_x[frame]= MATH_wrap (prop_text->frame_list_texture_x[frame] , texture->wrap_width) ;
		prop_text->frame_list_texture_y[frame] = MATH_wrap (prop_text->frame_list_texture_y[frame] , texture->wrap_height) ;
	}

	acquire_bitmap(buffer_image);

	if (bitmap_color_depth(buffer_image) == 16)
	{
		if (min_max_type == PFONT_MINMAX_IGNORE) // ignore the values...
		{
			for (frame=0 ; frame<prop_text->frame_pointer ; frame++ )
			{
				GRAPHICS_double_masked_blit_16bit ( texture->texture_image , prop_text->prop_font->font_gfx , buffer_image , prop_text->frame_list_texture_x[frame] , prop_text->frame_list_texture_y[frame] , prop_text->frame_list_image_x[frame] , prop_text->frame_list_image_y[frame] , x + prop_text->frame_list_new_x[frame] , y + prop_text->frame_list_new_y[frame] , prop_text->prop_font->letter_widths[prop_text->frame_list_frame[frame]] , prop_text->prop_font->char_height );
			}
		}
		else if (min_max_type == PFONT_MINMAX_CHARS)
		{
			for (frame=min ; ( (frame<max) && (frame<prop_text->frame_pointer) ) ; frame++ )
			{
				GRAPHICS_double_masked_blit_16bit ( texture->texture_image , prop_text->prop_font->font_gfx , buffer_image , prop_text->frame_list_texture_x[frame] , prop_text->frame_list_texture_y[frame] , prop_text->frame_list_image_x[frame] , prop_text->frame_list_image_y[frame] , x + prop_text->frame_list_new_x[frame] , y + prop_text->frame_list_new_y[frame] , prop_text->prop_font->letter_widths[prop_text->frame_list_frame[frame]] , prop_text->prop_font->char_height );
			}
		}
		else
		{
			for (frame=prop_text->line_frame_start[min] ; ( (frame<prop_text->line_frame_start[max]+prop_text->line_frame_length[max]) && (frame<prop_text->frame_pointer) ) ; frame++ )
			{
				GRAPHICS_double_masked_blit_16bit ( texture->texture_image , prop_text->prop_font->font_gfx , buffer_image , prop_text->frame_list_texture_x[frame] , prop_text->frame_list_texture_y[frame] , prop_text->frame_list_image_x[frame] , prop_text->frame_list_image_y[frame] , x + prop_text->frame_list_new_x[frame] , y + prop_text->frame_list_new_y[frame] , prop_text->prop_font->letter_widths[prop_text->frame_list_frame[frame]] , prop_text->prop_font->char_height );
			}
		}

	}
	else if (bitmap_color_depth(buffer_image) == 32)
	{
		if (min_max_type == PFONT_MINMAX_IGNORE) // ignore the values...
		{
			for (frame=0 ; frame<prop_text->frame_pointer ; frame++ )
			{
				GRAPHICS_double_masked_blit_32bit ( texture->texture_image , prop_text->prop_font->font_gfx , buffer_image , prop_text->frame_list_texture_x[frame] , prop_text->frame_list_texture_y[frame] , prop_text->frame_list_image_x[frame] , prop_text->frame_list_image_y[frame] , x + prop_text->frame_list_new_x[frame] , y + prop_text->frame_list_new_y[frame] , prop_text->prop_font->letter_widths[prop_text->frame_list_frame[frame]] , prop_text->prop_font->char_height );
			}
		}
		else if (min_max_type == PFONT_MINMAX_CHARS)
		{
			for (frame=min ; ( (frame<max) && (frame<prop_text->frame_pointer) ) ; frame++ )
			{
				GRAPHICS_double_masked_blit_32bit ( texture->texture_image , prop_text->prop_font->font_gfx , buffer_image , prop_text->frame_list_texture_x[frame] , prop_text->frame_list_texture_y[frame] , prop_text->frame_list_image_x[frame] , prop_text->frame_list_image_y[frame] , x + prop_text->frame_list_new_x[frame] , y + prop_text->frame_list_new_y[frame] , prop_text->prop_font->letter_widths[prop_text->frame_list_frame[frame]] , prop_text->prop_font->char_height );
			}
		}
		else
		{
			for (frame=prop_text->line_frame_start[min] ; ( (frame<prop_text->line_frame_start[max]+prop_text->line_frame_length[max]) && (frame<prop_text->frame_pointer) ) ; frame++ )
			{
				GRAPHICS_double_masked_blit_32bit ( texture->texture_image , prop_text->prop_font->font_gfx , buffer_image , prop_text->frame_list_texture_x[frame] , prop_text->frame_list_texture_y[frame] , prop_text->frame_list_image_x[frame] , prop_text->frame_list_image_y[frame] , x + prop_text->frame_list_new_x[frame] , y + prop_text->frame_list_new_y[frame] , prop_text->prop_font->letter_widths[prop_text->frame_list_frame[frame]] , prop_text->prop_font->char_height );
			}
		}
	}

	release_bitmap(buffer_image);

}



void PFONT_draw_frame_list_coloured ( prop_text_struct *prop_text , int min , int max , int min_max_type , int x , int y , float handle_x , float handle_y , BITMAP *buffer_image , int red , int green , int blue )
{
	int frame;

	int offset_x = ( ( prop_text->image_width ) * handle_x );
	int offset_y = ( ( prop_text->image_height ) * handle_y );

	unsigned short colour_16bit = makecol16 ( red , green , blue );
	unsigned long colour_32bit = makecol32 ( red , green , blue );;

	x = x - offset_x;
	y = y - offset_y;

	acquire_bitmap(buffer_image);

	if (bitmap_color_depth(buffer_image) == 16)
	{
		if (min_max_type == PFONT_MINMAX_IGNORE) // ignore the values...
		{
			for (frame=0 ; frame<prop_text->frame_pointer ; frame++ )
			{
				GRAPHICS_colour_blit_16bit ( prop_text->prop_font->font_gfx , buffer_image , prop_text->frame_list_image_x[frame] , prop_text->frame_list_image_y[frame] , x + prop_text->frame_list_new_x[frame] , y + prop_text->frame_list_new_y[frame] , prop_text->prop_font->letter_widths[prop_text->frame_list_frame[frame]] , prop_text->prop_font->char_height , colour_16bit);
			}
		}
		else if (min_max_type == PFONT_MINMAX_CHARS)
		{
			for (frame=min ; ( (frame<max) && (frame<prop_text->frame_pointer) ) ; frame++ )
			{
				GRAPHICS_colour_blit_16bit ( prop_text->prop_font->font_gfx , buffer_image , prop_text->frame_list_image_x[frame] , prop_text->frame_list_image_y[frame] , x + prop_text->frame_list_new_x[frame] , y + prop_text->frame_list_new_y[frame] , prop_text->prop_font->letter_widths[prop_text->frame_list_frame[frame]] , prop_text->prop_font->char_height , colour_16bit);
			}
		}
		else
		{
			for (frame=prop_text->line_frame_start[min] ; ( (frame<prop_text->line_frame_start[max]+prop_text->line_frame_length[max]) && (frame<prop_text->frame_pointer) ) ; frame++ )
			{
				GRAPHICS_colour_blit_16bit ( prop_text->prop_font->font_gfx , buffer_image , prop_text->frame_list_image_x[frame] , prop_text->frame_list_image_y[frame] , x + prop_text->frame_list_new_x[frame] , y + prop_text->frame_list_new_y[frame] , prop_text->prop_font->letter_widths[prop_text->frame_list_frame[frame]] , prop_text->prop_font->char_height , colour_16bit);
			}
		}

	}
	else if (bitmap_color_depth(buffer_image) == 32)
	{
		if (min_max_type == PFONT_MINMAX_IGNORE) // ignore the values...
		{
			for (frame=0 ; frame<prop_text->frame_pointer ; frame++ )
			{
				GRAPHICS_colour_blit_32bit ( prop_text->prop_font->font_gfx , buffer_image , prop_text->frame_list_image_x[frame] , prop_text->frame_list_image_y[frame] , x + prop_text->frame_list_new_x[frame] , y + prop_text->frame_list_new_y[frame] , prop_text->prop_font->letter_widths[prop_text->frame_list_frame[frame]] , prop_text->prop_font->char_height , colour_32bit);
			}
		}
		else if (min_max_type == PFONT_MINMAX_CHARS)
		{
			for (frame=min ; ( (frame<max) && (frame<prop_text->frame_pointer) ) ; frame++ )
			{
				GRAPHICS_colour_blit_32bit ( prop_text->prop_font->font_gfx , buffer_image , prop_text->frame_list_image_x[frame] , prop_text->frame_list_image_y[frame] , x + prop_text->frame_list_new_x[frame] , y + prop_text->frame_list_new_y[frame] , prop_text->prop_font->letter_widths[prop_text->frame_list_frame[frame]] , prop_text->prop_font->char_height , colour_32bit);
			}
		}
		else
		{
			for (frame=prop_text->line_frame_start[min] ; ( (frame<prop_text->line_frame_start[max]+prop_text->line_frame_length[max]) && (frame<prop_text->frame_pointer) ) ; frame++ )
			{
				GRAPHICS_colour_blit_32bit ( prop_text->prop_font->font_gfx , buffer_image , prop_text->frame_list_image_x[frame] , prop_text->frame_list_image_y[frame] , x + prop_text->frame_list_new_x[frame] , y + prop_text->frame_list_new_y[frame] , prop_text->prop_font->letter_widths[prop_text->frame_list_frame[frame]] , prop_text->prop_font->char_height , colour_32bit);
			}
		}
	}

	release_bitmap(buffer_image);

}



void PFONT_draw_frame_list_vert_lerped_coloured ( prop_text_struct *prop_text , int min , int max , int min_max_type , int x , int y , float handle_x , float handle_y , BITMAP *buffer_image , int red_1 , int green_1 , int blue_1 , int red_2 , int green_2 , int blue_2 )
{
	int frame;

	int offset_x = ( ( prop_text->image_width ) * handle_x );
	int offset_y = ( ( prop_text->image_height ) * handle_y );

	unsigned short colour_table_16bit[MAX_DISPLAY_HEIGHT];
	unsigned long colour_table_32bit[MAX_DISPLAY_HEIGHT];

	int t;

	
	for (t=0 ; t < prop_text->prop_font->char_height ; t++)
	{
		colour_table_16bit[t] = makecol16 (MATH_lerp(red_1,red_2,float (t) / float (prop_text->prop_font->char_height)) , MATH_lerp(green_1,green_2,float (t) / float (prop_text->prop_font->char_height)) , MATH_lerp(blue_1,blue_2,float (t) / float (prop_text->prop_font->char_height)) );
		colour_table_32bit[t] = makecol32 (MATH_lerp(red_1,red_2,float (t) / float (prop_text->prop_font->char_height)) , MATH_lerp(green_1,green_2,float (t) / float (prop_text->prop_font->char_height)) , MATH_lerp(blue_1,blue_2,float (t) / float (prop_text->prop_font->char_height)) );
	}

	x = x - offset_x;
	y = y - offset_y;

	acquire_bitmap(buffer_image);

	if (bitmap_color_depth(buffer_image) == 16)
	{
		if (min_max_type == PFONT_MINMAX_IGNORE) // ignore the values...
		{
			for (frame=0 ; frame<prop_text->frame_pointer ; frame++ )
			{
				GRAPHICS_colour_vert_lerped_blit_16bit ( prop_text->prop_font->font_gfx , buffer_image , prop_text->frame_list_image_x[frame] , prop_text->frame_list_image_y[frame] , x + prop_text->frame_list_new_x[frame] , y + prop_text->frame_list_new_y[frame] , prop_text->prop_font->letter_widths[prop_text->frame_list_frame[frame]] , prop_text->prop_font->char_height , colour_table_16bit );
			}
		}
		else if (min_max_type == PFONT_MINMAX_CHARS)
		{
			for (frame=min ; ( (frame<max) && (frame<prop_text->frame_pointer) ) ; frame++ )
			{
				GRAPHICS_colour_vert_lerped_blit_16bit ( prop_text->prop_font->font_gfx , buffer_image , prop_text->frame_list_image_x[frame] , prop_text->frame_list_image_y[frame] , x + prop_text->frame_list_new_x[frame] , y + prop_text->frame_list_new_y[frame] , prop_text->prop_font->letter_widths[prop_text->frame_list_frame[frame]] , prop_text->prop_font->char_height , colour_table_16bit );
			}
		}
		else
		{
			for (frame=prop_text->line_frame_start[min] ; ( (frame<prop_text->line_frame_start[max]+prop_text->line_frame_length[max]) && (frame<prop_text->frame_pointer) ) ; frame++ )
			{
				GRAPHICS_colour_vert_lerped_blit_16bit ( prop_text->prop_font->font_gfx , buffer_image , prop_text->frame_list_image_x[frame] , prop_text->frame_list_image_y[frame] , x + prop_text->frame_list_new_x[frame] , y + prop_text->frame_list_new_y[frame] , prop_text->prop_font->letter_widths[prop_text->frame_list_frame[frame]] , prop_text->prop_font->char_height , colour_table_16bit );
			}
		}
	}
	else if (bitmap_color_depth(buffer_image) == 32)
	{
		if (min_max_type == PFONT_MINMAX_IGNORE) // ignore the values...
		{
			for (frame=0 ; frame<prop_text->frame_pointer ; frame++ )
			{
				GRAPHICS_colour_vert_lerped_blit_32bit ( prop_text->prop_font->font_gfx , buffer_image , prop_text->frame_list_image_x[frame] , prop_text->frame_list_image_y[frame] , x + prop_text->frame_list_new_x[frame] , y + prop_text->frame_list_new_y[frame] , prop_text->prop_font->letter_widths[prop_text->frame_list_frame[frame]] , prop_text->prop_font->char_height , colour_table_32bit );
			}
		}
		else if (min_max_type == PFONT_MINMAX_CHARS)
		{
			for (frame=min ; ( (frame<max) && (frame<prop_text->frame_pointer) ) ; frame++ )
			{
				GRAPHICS_colour_vert_lerped_blit_32bit ( prop_text->prop_font->font_gfx , buffer_image , prop_text->frame_list_image_x[frame] , prop_text->frame_list_image_y[frame] , x + prop_text->frame_list_new_x[frame] , y + prop_text->frame_list_new_y[frame] , prop_text->prop_font->letter_widths[prop_text->frame_list_frame[frame]] , prop_text->prop_font->char_height , colour_table_32bit );
			}
		}
		else
		{
			for (frame=prop_text->line_frame_start[min] ; ( (frame<prop_text->line_frame_start[max]+prop_text->line_frame_length[max]) && (frame<prop_text->frame_pointer) ) ; frame++ )
			{
				GRAPHICS_colour_vert_lerped_blit_32bit ( prop_text->prop_font->font_gfx , buffer_image , prop_text->frame_list_image_x[frame] , prop_text->frame_list_image_y[frame] , x + prop_text->frame_list_new_x[frame] , y + prop_text->frame_list_new_y[frame] , prop_text->prop_font->letter_widths[prop_text->frame_list_frame[frame]] , prop_text->prop_font->char_height , colour_table_32bit );
			}
		}
	}

	release_bitmap(buffer_image);

}










