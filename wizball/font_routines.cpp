#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <assert.h>

#include "string_stuff.h"
#include "font_routines.h"
#include "scripting.h"
#include "output.h"

#include "fortify.h"

word_bubble paras[MAX_ENTITIES];

#define FONT_NEW_LINE_CHAR			'|'
#define MAX_PROBABLE_LINES			(256)

#define FONT_ALIGN_LEFT					(1)
#define FONT_ALIGN_TOP					(1)
#define FONT_ALIGN_CENTER				(2)
#define FONT_ALIGN_RIGHT				(3)
#define FONT_ALIGN_BOTTOM				(3)
#define FONT_ALIGN_JUSTIFY				(4)
#define FONT_TEMP_TEXT_ID				(0)
#define FONT_TEXT_TYPE_BASIC			(1)
#define FONT_TEXT_TYPE_INSTANT_IMAGE	(2)
#define FONT_TEXT_TYPE_SPLIT_IMAGE		(3)
#define FONT_TEXT_TYPE_FRAME_LIST		(4)
#define FONT_TEXT_TYPE_GRADUAL_IMAGE	(5)
#define FONT_STATUS_PRE_PROCESSING		(-1)
#define FONT_STATUS_PROCESSING			(0)
#define FONT_STATUS_COMPLETE			(1)
#define FONT_DOES_NOT_EXIST				(2)



int FONT_get_line_length_width ( int entity_id , int start , int end )
{
	int width = 0;
	int t;
	int bitmap_number = paras[entity_id].bitmap_number;
	char *text_pointer = paras[entity_id].text;
	int letter_spacing = paras[entity_id].letter_spacing;

	for (t = start ; t < end ; t++ )
	{
		width += OUTPUT_get_sprite_width ( bitmap_number , text_pointer[t]-32 ) + letter_spacing;
	}

	return width - letter_spacing;
	// Chops off the last letter_spacing otherwise the line could appear to be too long when in fact it fits very snuggly.
}



int FONT_count_words ( int entity_id , int start , int end )
{
	int counter = 0;
	int t;
	char *text_pointer = paras[entity_id].text;

	while ( text_pointer[start] == ' ' )
	{
		start++;
	}
	
	while ( text_pointer[end] == ' ' )
	{
		end--;
	}

	for ( t=start ; t<end ; t++ )
	{
		counter = counter + (text_pointer[t] == ' ');
	}

	return (counter + 1);
}



int FONT_get_line_length ( int pointer , int entity_id , int &skipper )
{
	char *text_pointer = paras[entity_id].text;
	int bitmap_number = paras[entity_id].bitmap_number;
	int letter_spacing = paras[entity_id].letter_spacing;
	int image_width = paras[entity_id].width;

	while ( text_pointer[pointer] == ' ' )
	{
		// Skip whitespace...
		pointer++;
	}

	int line_width = 0;
	int last_space = -1;
	bool exit_flag = false;
	char letter;
	int value;
	int length = strlen(text_pointer);
	skipper = 0;

	do
	{

		// Every time we come across a space its position is noted so that if the next word makes the line too long,
		// we can roll back to the end of the previous word.

		if ( text_pointer[pointer] == ' ' )
		{
			last_space = pointer;
		}

		// Then we increase the line_width by the width of the letter, plus the spacing between letters

		letter = text_pointer[pointer];

		if (letter != FONT_NEW_LINE_CHAR)
		{
			value = letter - 32;
			line_width +=  OUTPUT_get_sprite_width ( bitmap_number , value );
			line_width += letter_spacing;
		}
		else
		{
			last_space = pointer;
			exit_flag = true;
		}

		pointer++;

	}
	while ( (line_width <= image_width + letter_spacing) && (pointer < length) && (exit_flag == false) );

	// If the line is too wide for the box, or the pointer is currently not at a space (unless it's the end of the text)...

	if ( (line_width > image_width + letter_spacing) || ( ( text_pointer[pointer] != ' ' ) && ( pointer < length ) ) )
	{
		if (last_space != -1)
			pointer = last_space;
		else
			skipper = -1;
	}

	return pointer;
}



void FONT_calculate_line_lengths ( int entity_id )
{
	word_bubble *p = &paras[entity_id];

	char *text_pointer = p->text;

	int line_start[MAX_PROBABLE_LINES];
	int line_length[MAX_PROBABLE_LINES];

	int bitmap_number = p->bitmap_number;

	int pointer = 0;
	int line_counter = 0;
	int new_pointer;
	int length = strlen(text_pointer);
	int skipper;

	do
	{
		p->line_start[line_counter] = pointer;

		new_pointer = FONT_get_line_length ( pointer , entity_id , skipper );

		p->line_length[line_counter] = (new_pointer - pointer);

		line_counter++;

		pointer = new_pointer + 1 + skipper;
	}
	while ( pointer < length );
	
	p->line_count = line_counter;

	p->line_start = (int *) malloc (sizeof(int) * line_counter);
	p->line_length = (int *) malloc (sizeof(int) * line_counter);

	p->line_frame_start = (int *) malloc (sizeof(int) * line_counter);
	p->line_frame_length = (int *) malloc (sizeof(int) * line_counter);

	for (line_counter=0; line_counter<p->line_count; line_counter++)
	{
		p->line_start[line_counter] = line_start[line_counter];
		p->line_length[line_counter] = line_length[line_counter];
	}
	
	p->height = (p->line_count * p->char_height) + ((p->line_count - 1) * p->line_spacing);
}



int FONT_generate_line ( int entity_id , int start , int end , float x , float y , bool include_spaces , int counter , int line_number )
{
	int t;
	int frame;

	word_bubble *p = &paras[entity_id];

	float frame_width;
	char *text_pointer = p->text;
	int bitmap_number = p->bitmap_number;
	int letter_spacing = p->letter_spacing;

	int offset;
	
	for (t = start ; t < end ; t++)
	{
		frame = (text_pointer[t] - 32);
		
		if (frame >= 0)
		{
			frame_width = float (OUTPUT_get_sprite_width ( bitmap_number , frame ));
		}

		if ( (frame > 0) || (include_spaces == true) )
		{
			offset = p->frame_pointer + counter;

			OUTPUT_get_sprite_uvs (bitmap_number , frame, p->letters[offset].u1, p->letters[offset].v1, p->letters[offset].u2, p->letters[offset].v2);

			p->letters[offset].base_x = x;
			p->letters[offset].base_y = y;

			p->letters[offset].x = x;
			p->letters[offset].y = y;

			p->letters[offset].frame = frame;

			p->letters[offset].width = frame_width;

			p->letters[offset].line = line_number;

			counter++;
		}

		x += (frame_width + p->letter_spacing );
	}

	return counter;

}



void FONT_generate_coords ( int line_number , int entity_id , bool include_spaces , bool reverse_x_order )
{
	word_bubble *p = &paras[entity_id];

	// Oh, that keep spaces thing is for the gradual text where for timing purpose you'll probably want
	// to print the spaces as well. Also because that's how it recognises where words end if you're using
	// the add_word function. Either way, they aren't actually ever drawn.

	int generated_frames = 0;

	int word_start = p->line_start[line_number];
	int word_end = word_start + p->line_length[line_number];

	int line_width = FONT_get_line_length_width ( entity_id , word_start , word_end );
	// First thing to do is find how long the line is going to be pixelwise.

	int num_words = FONT_count_words ( entity_id , word_start , word_end );
	// And how many words are in the line.

	int alignment = p->alignment;
	// And store the alignment in a temporary variable because it can be changed depending on the circumstance.

	if ( ( (num_words == 1) || (line_number == p->line_count-1) ) && (alignment == FONT_ALIGN_JUSTIFY) )
	{
		// If it's justified, but you're on the last line or there is only one word on the line then change to left align.
		alignment = FONT_ALIGN_LEFT;
	}
	
	switch (alignment)
	{
	case FONT_ALIGN_LEFT:
		generated_frames = FONT_generate_line ( entity_id , word_start , word_end , 0.0f , float((line_number * p->char_height) + ( line_number * p->line_spacing )) , include_spaces , 0 , line_number );
		break;

	case FONT_ALIGN_CENTER:
		generated_frames = FONT_generate_line ( entity_id , word_start , word_end , float((p->width - line_width) / 2) , float((line_number * p->char_height) + ( line_number * p->line_spacing )) , include_spaces , 0 , line_number );
		break;

	case FONT_ALIGN_RIGHT:
		generated_frames = FONT_generate_line ( entity_id , word_start , word_end , float(p->width - line_width) , float((line_number * p->char_height) + ( line_number * p->line_spacing )) , include_spaces , 0 , line_number );
		break;

	case FONT_ALIGN_JUSTIFY:
		int word_gap = (p->width - line_width) / (num_words-1); // extra distance between words
		// This is the number of extra pixels between each word on the line given the length of the line and the width of the image.

		int spare_pixels = (p->width - line_width) % (num_words-1);		
		// How many pixels left over because obviously it's not always gonna' split precisely.

		int word_pointer = word_start;
		int next_word_pointer;

		int start_x = 0;

		int n;
		for (n=0 ; n<num_words ; n++)
		{
			next_word_pointer = STRING_instr_char ( p->text , ' ' , word_pointer );
			// Find the next word so we know the length of the word.

			generated_frames = FONT_generate_line ( entity_id , word_pointer , next_word_pointer , float(start_x) , float((line_number * p->char_height) + ( line_number * p->line_spacing )) , include_spaces , generated_frames , line_number );
			// Then draw that word.

			start_x += FONT_get_line_length_width ( entity_id , word_pointer , next_word_pointer + 1) + word_gap + (n < spare_pixels) + p->letter_spacing;
			// Add the width of the word in pixels, plus the word gap, then one of the spare pixels if applicable and then x_spacing because FONT_get_line_length_width cuts it off.

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
			temp = p->text[ t + p->frame_pointer ];
			p->text[ t + p->frame_pointer ] = p->text[ (p->frame_pointer + generated_frames) - (t+1) ];
			p->text[ (p->frame_pointer + generated_frames) - (t+1) ] = temp;
		}
	}

	p->frame_pointer += generated_frames;
	// Then update the frame_pointer.
}



void FONT_destroy_current_contents (int entity_id)
{
	if (paras[entity_id].in_use == true)
	{
		free (paras[entity_id].line_frame_start);
		free (paras[entity_id].line_frame_length);
		free (paras[entity_id].line_length);
		free (paras[entity_id].line_start);
		free (paras[entity_id].text);
		free (paras[entity_id].letters);

		paras[entity_id].in_use = false;
	}
}



void FONT_create_frame_list ( int entity_id , int bitmap_number, char *words, int width, int alignment, int x_spacing, int y_spacing , bool reverse_x_order , bool reverse_y_order )
{
	FONT_destroy_current_contents (entity_id);

	paras[entity_id].in_use = true;

	word_bubble *p = &paras[entity_id];

	p->alignment = alignment;
	p->letter_spacing = x_spacing;
	p->line_spacing = y_spacing;
	p->width = width;
	p->frame_pointer = 0;

	p->bitmap_number = bitmap_number;
	p->char_height = OUTPUT_get_sprite_height (bitmap_number,0);

	// Then start plopping new ones in. :)

	p->text = (char *) malloc (sizeof(char) * (strlen(words)+1) );
	strcpy (p->text , words);

	FONT_calculate_line_lengths (entity_id);

	int l;

	if (reverse_y_order == false)
	{
		for (l=0 ; l<p->line_count ; l++)
		{
			FONT_generate_coords ( l , entity_id , false , reverse_x_order);
		}
	}
	else
	{
		for (l=p->line_count-1 ; l>=0 ; l--)
		{
			FONT_generate_coords ( l , entity_id , false , reverse_x_order);
		}
	}

	int t;
	for (t=0; t<=p->line_count ; t++)
	{
		p->line_frame_start[t] = UNSET;
		p->line_frame_length[t] = 0;
	}

}



void FONT_posset_reset ( int entity_id )
{
	// This just copies the base co-ords into the new co-ords and presents the letters "as-is" and resets
	// everything in readiness to apply all the other effects.

	int frame;
	word_bubble *p = &paras[entity_id];

	for (frame=0 ; frame < p->frame_pointer ; frame++ )
	{
		p->letters[frame].x = p->letters[frame].base_x;
		p->letters[frame].y = p->letters[frame].base_y;
	}
}



void FONT_set_up_paras (void)
{
	int para;
	
	for (para=0; para<MAX_ENTITIES; para++)
	{
		paras[para].in_use = false;
	}
}
