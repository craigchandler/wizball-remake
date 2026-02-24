#ifndef _FONT_ROUTINES_H_
#define _FONT_ROUTINES_H_

typedef struct
{
	float base_x; // Base offsets from the first letter.
	float base_y;
	float width;
	float height;

	float x; // Offsets from the first letter.
	float y;

	float u1; // UVs of each letter to save on looking up.
	float u2;
	float v1;
	float v2;

	int frame;
	int line;
} word_letter;



typedef struct
{
	bool in_use;

	int alignment;

	int letter_count;

	int bitmap_number;

	int *line_frame_start;
	int *line_frame_length;

	int *line_length;
	int *line_start;
	int line_count;

	int frame_pointer;

	char *text;

	word_letter *letters;

	int width;
	int height;

	int letter_spacing;
	int line_spacing;

	int char_height;
} word_bubble;

void FONT_destroy_current_contents (int entity_id);

#endif
