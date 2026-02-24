#include <allegro.h>
#include <alleggl.h>
#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

#include "glext.h"
#include "string_size_constants.h"
#include "output.h"
#include "file_stuff.h"
#include "math_stuff.h"
#include "scripting.h"
#include "graphics.h"
#include "tilemaps.h"
#include "tilesets.h"
#include "main.h"
#include "control.h"
#include "particles.h"
#include "string_stuff.h"
#include "global_param_list.h"
#include "object_collision.h"

#include "fortify.h"

GLfloat ProjectionMatrix[16];



typedef struct {
	char name[NAME_SIZE];
	int parent_bitmap;
	int x;
	int y;
	int pivot_x;
	int pivot_y;
	int width;
	int height;
	float u1;
	float u2;
	float v1;
	float v2;
	float uv_pixel_size;
} sprite;



typedef struct {
	BITMAP *image;
	GLuint texture;
	int width;
	int height;
	int sprite_count;
	sprite *sprite_list;
} bitmap_holder;



typedef struct {
	int current_x;
	int current_y;
	int current_z;
	int texture;
	bool blend;
	int blend_type;
	int vertex_colour_red;
	int vertex_colour_green;
	int vertex_colour_blue;
	float current_alpha;
} rendering_status;



typedef struct 
{
	float red[4];
	float green[4];
	float blue[4];
} vertex_colour_struct;



typedef struct
{
	char *extension;
} extension_struct;



typedef struct
{
	int extension_count;
	extension_struct *extension_list;
	char *version_text;
} opengl_data_struct;


vertex_colour_struct entity_vertex_colours[MAX_ENTITIES];


// This should not be needed for the Mac version as the opengl header you're using will hopefully
// be better than the shitty 1.1 us Windows users have to use. It'll mean you have to change all
// the "pglSecondaryColor3fEXT" to "glSecondaryColor3f" in the source, though.
#ifdef ALLEGRO_MACOSX
	typedef void (*PFNGLSECONDARYCOLOR3FEXTPROC) (GLfloat red, GLfloat green, GLfloat blue);
#else
	typedef void (APIENTRYP PFNGLSECONDARYCOLOR3FEXTPROC) (GLfloat red, GLfloat green, GLfloat blue);
#endif
PFNGLSECONDARYCOLOR3FEXTPROC pglSecondaryColor3fEXT = NULL;
// Actually, you might be able to leave it...


#ifdef ALLEGRO_MACOSX
	typedef void (* PFNGLACTIVETEXTUREEXTPROC) (GLint texture_unit);
#else
	typedef void (APIENTRYP PFNGLACTIVETEXTUREEXTPROC) (GLint texture_unit);
#endif
PFNGLACTIVETEXTUREEXTPROC pglActiveTextureARB = NULL;


//typedef void (APIENTRYP PFNGLTEXENVIARBPROC) (GLint target,GLint pname,GLint param);
//PFNGLTEXENVIARBPROC pglTexEnviARB = NULL;


int opengl_major_version = 0;
int opengl_minor_version = 0;

float float_window_scale_multiplier;

opengl_data_struct opengl_data;

rendering_status rs;

BITMAP *pages[3] = {NULL, NULL, NULL};
BITMAP *active_page;
BITMAP *current_page;

int update_method;
bool wait_for_vsync = false;

bitmap_holder *bmps = NULL;
// The thing that holds all the bitmaps.
int total_bitmaps_loaded = 0;
int total_bitmaps_allocated = 0;

// This is the size of the created window.
int game_screen_width = UNSET;
int game_screen_height = UNSET;

// And this is the size of the window within it. If they do
// not match then all calls to the window drawing function
// are scaled.

int virtual_screen_width;
int virtual_screen_height;

int window_scale_multiplier;

bool texture_combiner_available;
bool best_texture_combiner_available;

bool enable_multi_texture_effects_ideally = true;

bool secondary_colour_available;

bool software_mode_active = false;

DATAFILE *data_dat = NULL;
DATAFILE *sfx_dat = NULL;
DATAFILE *gfx_dat = NULL;
DATAFILE *stream_dat = NULL;
DATAFILE *music_dat = NULL;



void * INPUT_get_stream_data_pointer(char *object_name, int *data_length)
{
	DATAFILE *data_pointer = find_datafile_object(stream_dat, object_name);
	*data_length = data_pointer->size;
	return (data_pointer->dat);
}



void * INPUT_get_sfx_data_pointer(char *object_name, int *data_length)
{
	DATAFILE *data_pointer = find_datafile_object(sfx_dat, object_name);
	*data_length = data_pointer->size;
	return (data_pointer->dat);
}



bool INPUT_load_datafile (void)
{
	data_dat = load_datafile(MAIN_get_project_filename("data.dat")); 

	if (data_dat != NULL)
	{
		return true;
	}
	else
	{
		return false;
	}
}



void INPUT_load_media_datafiles (void)
{
	sfx_dat = load_datafile(MAIN_get_project_filename("sfx.dat"));
	gfx_dat = load_datafile(MAIN_get_project_filename("gfx.dat"));
	stream_dat = load_datafile(MAIN_get_project_filename("stream.dat"));
	music_dat = load_datafile(MAIN_get_project_filename("music.dat"));
}



void INPUT_unload_datafiles (void)
{
	unload_datafile (data_dat);
	unload_datafile (sfx_dat);
	unload_datafile (gfx_dat);
	unload_datafile (stream_dat);
	unload_datafile (music_dat);
}



void OUTPUT_set_virtual_screen_size (int width, int height)
{
	virtual_screen_width = width;
	virtual_screen_height = height;
}



void OUTPUT_message (char *msg)
{
	allegro_message (msg);
}



void OUTPUT_rectangle (int x1, int y1, int x2, int y2, int r, int g, int b)
{
	if (software_mode_active)
	{
		rect (active_page , x1 , y1 , x2 , y2 , makecol(r,g,b) );
	}
	else
	{
		glLoadIdentity();

		glColor3f( float(r)/256.0f , float(g)/256.0f , float(b)/256.0f);

		glDisable(GL_TEXTURE_2D);

		glTranslatef( x1 , (virtual_screen_height-1) - y1 , 0.0f );

		glBegin(GL_LINE_LOOP);
			glVertex2f( 0, 0); // Bottom left corner
			glVertex2f( 0, -(y2-y1)); //. Top left corner
			glVertex2f( x2-x1, -(y2-y1)); // Top right corner
			glVertex2f( x2-x1, 0);  // Bottom right corner
		glEnd();
	}
}



void OUTPUT_rectangle_by_size (int x, int y, int width, int height, int r, int g, int b)
{
	OUTPUT_rectangle (x, y, x+width-1, y+height-1, r, g, b);
}



void OUTPUT_filled_rectangle (int x1, int y1, int x2, int y2, int r, int g, int b)
{

	if (software_mode_active)
	{
		rectfill (active_page , x1 , y1 , x2 , y2 , makecol(r,g,b) );
	}
	else
	{
		glLoadIdentity();
		glColor3f( float(r)/256.0f , float(g)/256.0f , float(b)/256.0f);

		glDisable(GL_TEXTURE_2D);

		glTranslatef( x1 , (virtual_screen_height-1) - y1 , 0.0f );

		glBegin(GL_QUADS);
			glVertex2f( 0, 0); // Bottom left corner
			glVertex2f( 0, -(y2-y1)); //. Top left corner
			glVertex2f( x2-x1, -(y2-y1)); // Top right corner
			glVertex2f( x2-x1, 0);  // Bottom right corner
		glEnd();
	}
	
}



void OUTPUT_filled_rectangle_by_size (int x, int y, int width, int height, int r, int g, int b)
{
	OUTPUT_filled_rectangle (x, y, x+width-1, y+height-1, r, g, b);
}



void OUTPUT_text (int x, int y, char *text, int r, int g, int b, int scale)
{
	if (software_mode_active)
	{
		textout (active_page , font, text, x , y , makecol(r,g,b) );
	}
	else
	{
		glLoadIdentity();

		glEnable(GL_TEXTURE_2D);

		glBindTexture(GL_TEXTURE_2D, bmps[small_font_gfx].texture);
		glColor3f( float(r)/256.0f , float(g)/256.0f , float(b)/256.0f );

		glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);

		glEnable (GL_ALPHA_TEST);
		glAlphaFunc (GL_GREATER, 0.5f);

		int c;
		int length = strlen(text);

		glTranslatef(x,virtual_screen_height-y,0.0);

		glScalef (float(scale)/10000.0f , float(scale)/10000.0f , 1.0f);

		// Just get the basic sizes for the first letter as it'll be the same for the rest.

		sprite *sp = &bmps[small_font_gfx].sprite_list[0];

		float u1 = sp->u1;
		float u2 = sp->u2;
		float v1 = sp->v1;
		float v2 = sp->v2;

		float left = -sp->pivot_x;
		float right = sp->width - sp->pivot_x;
		float up = sp->pivot_y;
		float down = - (sp->height - sp->pivot_y);

		for(c=0; c<length; c++)
		{
			sprite *sp = &bmps[small_font_gfx].sprite_list[text[c]-32];

			u1 = sp->u1;
			u2 = sp->u2;
			v1 = sp->v1;
			v2 = sp->v2;

			glBegin(GL_QUADS);
				glTexCoord2f(u1, v1);
				glVertex2f( left, up);
				glTexCoord2f(u1, v2);
				glVertex2f( left, down);
				glTexCoord2f(u2, v2);
				glVertex2f( right, down);
				glTexCoord2f(u2, v1);
				glVertex2f( right, up);
			glEnd();

			glTranslatef (right , 0.0f , 0.0f);

		}

		glDisable (GL_ALPHA_TEST);
//		glDisable(GL_TEXTURE_2D);
	}
}



void OUTPUT_truncated_text (int max_chars ,int x, int y, char *text, int r, int g, int b)
{
	char temp[TEXT_LINE_SIZE];
	strcpy(temp,text);
	temp[max_chars]='\0';
	OUTPUT_text (x, y, temp, r, g, b);
}



void OUTPUT_centred_text (int x, int y, char *text, int r, int g, int b)
{
	int offet_x;

	if (software_mode_active)
	{
		offet_x = strlen(text)*(SOFTWARE_FONT_WIDTH/2);
	}
	else
	{
		offet_x = strlen(text)*(FONT_WIDTH/2);
	}

	OUTPUT_text (x-offet_x, y, text, r, g, b);
}



void OUTPUT_boxed_centred_text (int x, int y, char *text, int r, int g, int b)
{
	int offet_x;

	if (software_mode_active)
	{
		offet_x = strlen(text)*(SOFTWARE_FONT_WIDTH/2);
	}
	else
	{
		offet_x = strlen(text)*(FONT_WIDTH/2);
	}

	OUTPUT_text (x-offet_x, y, text, r, g, b);

	OUTPUT_rectangle (x - strlen(text)*(FONT_WIDTH/2) - 2 , y - 2, x + strlen(text)*(FONT_WIDTH/2) + 1, y + FONT_HEIGHT + 1, r/2, g/2, b/2);
}



void OUTPUT_right_aligned_text (int x, int y, char *text, int r, int g, int b)
{
	int offet_x;

	if (software_mode_active)
	{
		offet_x = strlen(text)*(SOFTWARE_FONT_WIDTH);
	}
	else
	{
		offet_x = strlen(text)*(FONT_WIDTH);
	}

	OUTPUT_text (x-offet_x, y, text, r, g, b);
}



void OUTPUT_line (int x1 , int y1, int x2, int y2, int r, int g, int b)
{
	if (software_mode_active)
	{
		line (active_page , x1 , y1 , x2 , y2 , makecol(r,g,b) );
	}
	else
	{
		glLoadIdentity();
		glColor3f( float(r)/256.0f , float(g)/256.0f , float(b)/256.0f);

		glDisable(GL_TEXTURE_2D);

		glTranslatef( x1 , (virtual_screen_height-1) - y1 , 0.0f );

		glBegin(GL_LINES);
			glVertex2f( 0, 0);
			glVertex2f( x2-x1, -(y2-y1));
		glEnd();
	}
}



void OUTPUT_set_arbitrary_quad_from_base (int entity_index)
{
	int *entity_pointer = &entity[entity_index][0];
	
	if ((entity_pointer[ENT_SPRITE] > UNSET) && (entity_pointer[ENT_CURRENT_FRAME] > UNSET))
	{
		sprite *sp = &bmps[entity_pointer[ENT_SPRITE]].sprite_list[entity_pointer[ENT_CURRENT_FRAME]];
		
		arbitrary_quads_struct *aq = &arbitrary_quads[entity_index];

		aq->x[0] = -sp->pivot_x;
		aq->x[2] = -sp->pivot_x;

		aq->x[1] = sp->width - sp->pivot_x;
		aq->x[3] = sp->width - sp->pivot_x;

		aq->y[0] = sp->pivot_y;
		aq->y[1] = sp->pivot_y;

		aq->y[2] = - (sp->height - sp->pivot_y);
		aq->y[3] = - (sp->height - sp->pivot_y);
	}
}



void OUTPUT_hline (int x1 , int y1, int x2, int r, int g, int b)
{
	OUTPUT_line ( x1 , y1 , x2 , y1 , r,g,b );
}



void OUTPUT_vline (int x1 , int y1, int y2, int r, int g, int b)
{
	OUTPUT_line ( x1 , y1 , x1 , y2 , r,g,b );
}



void OUTPUT_circle (int x , int y , int radius , int r, int g, int b)
{
	if (software_mode_active)
	{
		circle (active_page , x , y , radius , makecol(r,g,b) );
/*
		float angle;
		float step = (2.0f * PI) / OPENGL_CIRCLE_RESOLUTION;

		float cx1,cy1;
		float cx2,cy2;

		for (angle = 0; angle < (2.0f * PI) ; angle += step)
		{
			cx1 = float(radius) * float (cos(angle) - sin(angle));
			cy1 = float(radius) * float (cos(angle) + sin(angle));

			cx2 = float(radius) * float (cos(angle+step) - sin(angle+step));
			cy2 = float(radius) * float (cos(angle+step) + sin(angle+step));

			OUTPUT_line ( x+cx1 , y+cy1 , x+cx2 , y+cy2 , r,g,b );
		}
*/
	}
	else
	{
		glLoadIdentity();
		glColor3f( float(r)/256.0f , float(g)/256.0f , float(b)/256.0f);

		glDisable(GL_TEXTURE_2D);

		glTranslatef( x , (virtual_screen_height-1) - y , 0.0f );

		float angle;
		float step = (2.0f * PI) / OPENGL_CIRCLE_RESOLUTION;

		float cx1,cy1;
		float cx2,cy2;

		for (angle = 0; angle < (2.0f * PI) ; angle += step)
		{
			cx1 = float(radius) * float (cos(angle));
			cy1 = float(radius) * float (sin(angle));

			cx2 = float(radius) * float (cos(angle+step));
			cy2 = float(radius) * float (sin(angle+step));

			glBegin(GL_LINES);
				glVertex2f( cx1, cy1);
				glVertex2f( cx2, cy2);
			glEnd();
		}
	}
}



void OUTPUT_centred_square (int x , int y , int widthandheight , int r, int g, int b)
{
	OUTPUT_rectangle (x-widthandheight, y-widthandheight, x+widthandheight, y+widthandheight, r, g, b);
}



void OUTPUT_clear_screen (void)
{
	if (software_mode_active)
	{
		clear_bitmap (active_page);
	}
	else
	{
		glClear (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	}
}



#define TRIPLEBUFFER		(0)
#define PAGEFLIP			(1)
#define SYSTEMBUFFER		(2)
#define DOUBLEBUFFER		(3)



void OUTPUT_updatescreen (void)
{
	if (software_mode_active)
	{
		switch(update_method)
		{
		case TRIPLEBUFFER:
			do
			{
			}
			while (poll_scroll());
			
			current_page = active_page;

			request_video_bitmap (current_page);

			if (active_page == pages[0])
			{
				active_page = pages[1];
			}
			else if (active_page == pages[1])
			{
				active_page = pages[2];
			}
			else
			{
				active_page = pages[0];
			}
			break;

		case PAGEFLIP:
			current_page = active_page;
			show_video_bitmap (current_page);
			
			if (active_page == pages[0])
			{
				active_page = pages[1];
			}
			else
			{
				active_page = pages[0];
			}
			break;

		case SYSTEMBUFFER:
		case DOUBLEBUFFER:
			if (wait_for_vsync)
			{
				vsync();
			}
			blit (active_page, current_page, 0, 0, 0, 0, SCREEN_W, SCREEN_H);
			break;
		}
	}
	else
	{
		allegro_gl_flip();
	}
}



BITMAP * OUTPUT_get_buffer (void)
{
	return active_page;
}



BITMAP * OUTPUT_get_screen (void)
{
	return current_page;
}



void OUTPUT_enable_vsync (void)
{
	wait_for_vsync = true;
}



void OUTPUT_disable_vsync (void)
{
	wait_for_vsync = true;
}



bool OUTPUT_is_vsync_enabled (void)
{
	return wait_for_vsync;
}



int OUTPUT_get_screen_update_method (void)
{
	return update_method;
}



void OUTPUT_enable_multi_texture_effects (void)
{
	if (best_texture_combiner_available)
	{
		texture_combiner_available = true;
	}
}



void OUTPUT_disable_multi_texture_effects (void)
{
	texture_combiner_available = false;
}



bool OUTPUT_select_buffering_method (int buffering_method)
{
	int t;
	
	switch(buffering_method)
	{
	case TRIPLEBUFFER:
		if (!(gfx_capabilities & GFX_CAN_TRIPLE_BUFFER))
		{
			enable_triple_buffer();
		}

		if ((gfx_capabilities & GFX_CAN_TRIPLE_BUFFER))
		{
			pages[0] = create_video_bitmap(SCREEN_W, SCREEN_H);
			pages[1] = create_video_bitmap(SCREEN_W, SCREEN_H);
			pages[2] = create_video_bitmap(SCREEN_W, SCREEN_H);

			if(pages[0] && pages[1] && pages[2])
			{
				clear_bitmap(pages[0]);
				clear_bitmap(pages[1]);
				clear_bitmap(pages[2]);
				active_page  = pages[0];
				current_page = pages[2];
				show_video_bitmap(current_page);
				return true;
			}
			else
			{
				for (t=0;t<3;t++)
				{
					if(pages[t])
					{
						destroy_bitmap (pages[t]);
						pages[t] = NULL;
					}
				}

				return false;
			}
		}
		break;

	case PAGEFLIP:
		pages[0] = create_video_bitmap(SCREEN_W, SCREEN_H);
		if (!pages[0])
		{
			return false;
		}
		
		pages[1] = create_video_bitmap(SCREEN_W, SCREEN_H);
		if (!pages[1])
		{
			destroy_bitmap(pages[0]);
			return false;
		}

		// Pageflip...

		if (pages[0] && pages[1])
		{
			clear_bitmap (pages[0]);
			clear_bitmap (pages[1]);
			active_page = pages[0];
			current_page = pages[1];
			show_video_bitmap (current_page);
			return true;
		}
		else
		{
			for (t=0;t<2;t++)
			{
				if(pages[t])
				{
					destroy_bitmap (pages[t]);
					pages[t] = NULL;
				}
			}

			return false;
		}
		break;

	case SYSTEMBUFFER:
		active_page  = create_system_bitmap(SCREEN_W, SCREEN_H);

		if (!active_page)
		{
			return false;
		}

		current_page = create_video_bitmap(SCREEN_W, SCREEN_H);

		if (!current_page)
		{
			destroy_bitmap (active_page);
			return false;
		}

		clear_bitmap(active_page);
		clear_bitmap(current_page);
		show_video_bitmap(current_page);
		return true;
		break;

	case DOUBLEBUFFER:
		active_page  = create_bitmap(SCREEN_W, SCREEN_H);

		if (!active_page)
		{
			return false;
		}

		current_page = create_video_bitmap(SCREEN_W, SCREEN_H);

		if (!current_page)
		{
			destroy_bitmap (active_page);
			return false;
		}

		clear_bitmap(active_page);
		clear_bitmap(current_page);
		show_video_bitmap(current_page);
		return true;
		break;

	default:
		OUTPUT_message("WTF?!");
		assert(0);
		break;
	}

	return false;
}



bool OUTPUT_is_extension_supported (char *extension)
{
	int ext_number;

	for (ext_number=0;ext_number<opengl_data.extension_count; ext_number++)
	{
		if (strcmp(extension,opengl_data.extension_list[ext_number].extension) == 0)
		{
			return true;
		}
	}

	return false;
}



void OUTPUT_destroyed_extension_list (void)
{
	int counter;

	for (counter=0; counter<opengl_data.extension_count; counter++)
	{
		if (opengl_data.extension_list[counter].extension != NULL)
		{
			free (opengl_data.extension_list[counter].extension);
		}
		else
		{
			assert(0);
		}
	}

	if (opengl_data.version_text != NULL)
	{
		free (opengl_data.version_text);
	}
}



void OUTPUT_get_opengl_extensions (void)
{
	char word[MAX_LINE_SIZE];

	opengl_data.version_text = (char *) malloc (sizeof(char) * (strlen((char *) glGetString(GL_VERSION))+1));
	strcpy (opengl_data.version_text, (char *) glGetString(GL_VERSION));

	strcpy (word,opengl_data.version_text);

	strtok (word," ");

	char *pointer = strtok (word,".");
	opengl_major_version = atoi (pointer);

	pointer = strtok (NULL,".");
	opengl_minor_version = atoi (pointer);

	if (opengl_data.version_text == NULL)
	{
		assert(0);
	}

	int length = strlen((char *) glGetString(GL_EXTENSIONS)) + 1;
	char *extensions = (char *) malloc (sizeof(char) * length);
	strcpy(extensions, (char *) glGetString(GL_EXTENSIONS));

	if (extensions==NULL)
	{
		assert(0);
	}

	STRING_strip_all_disposable (extensions);

	int counter = 1; // Because even with no spaces, it means you have one extension (unless the string is empty).
	int c;
	
	for(c=0; c<length; c++)
	{
		if (extensions[c] == ' ')
		{
			counter++;
		}
	}

	opengl_data.extension_count = counter; 

	opengl_data.extension_list = (extension_struct *) malloc (sizeof(extension_struct) * opengl_data.extension_count);

	pointer = strtok(extensions," ");

	counter = 0;

	while (pointer != NULL)
	{
		opengl_data.extension_list[counter].extension = (char *) malloc (sizeof(char) * (strlen(pointer) + 1));
		strcpy (opengl_data.extension_list[counter].extension, pointer);
		counter++;
		pointer = strtok(NULL," ");
	}

	free (extensions);
}



void OUTPUT_write_opengl_extensions_to_file (void)
{
	FILE *file_pointer = fopen("OpenGL_Extension.txt","w");
	int counter;

	if (file_pointer!=NULL)
	{
		fputs(opengl_data.version_text,file_pointer);
		fputs("\n\n",file_pointer);
		
		for (counter=0; counter<opengl_data.extension_count; counter++)
		{
			fputs(opengl_data.extension_list[counter].extension,file_pointer);
			fputs("\n",file_pointer);
		}

		fputs("\n",file_pointer);
		fclose(file_pointer);
	}
	else
	{
		OUTPUT_message("Cannot output Opengl Extension information!");
		assert(0);
	}
}



void OUTPUT_setup_project_list (char *text)
{
	set_color_depth(32);
	set_color_conversion(COLORCONV_TOTAL + COLORCONV_KEEP_TRANS);
	set_gfx_mode(GFX_AUTODETECT_WINDOWED, 320, 240, 0, 0);

	clear_to_color(screen, makecol(255, 255, 255));
	acquire_screen();
	text_mode(-1);

	int y=0;

	char *pointer = strtok (text,"\n");

	while (pointer != NULL)
	{
		textout (screen , font, pointer, 0 , y , makecol(0,0,0) );
		y += 8;
		pointer = strtok (NULL,"\n");
	}

	release_screen();
}



void OUTPUT_setup_running_mode (void)
{
	clear_to_color(screen, makecol(255, 255, 255));
	acquire_screen();
	text_mode(-1);

	char options[3][64]={
		"1. Parse, debug, output datfiles.",
		"2. Parse, output datfiles.",
		"3. Load from datfiles.",
	};

	int y=0;
	int counter;
	
	for (counter=0; counter<3; counter++)
	{
		textout (screen , font, &options[counter][0], 0 , y , makecol(0,0,0) );
		y += 8;
	}

	release_screen();
}



void OUTPUT_setup_allegro (bool windowed, int colour_depth, int base_screen_width, int base_screen_height, int screen_multiplier)
{
	int result;
	int t;

	game_screen_width = base_screen_width * screen_multiplier;
	game_screen_height = base_screen_height * screen_multiplier;

	virtual_screen_width = base_screen_width;
	virtual_screen_height = base_screen_height;

	window_scale_multiplier = screen_multiplier;

	float_window_scale_multiplier = float (screen_multiplier);

	if (software_mode_active)
	{
		// This is for when we have no hardware accelleration. Which is never, nowadays.

		set_color_depth(colour_depth);
		set_color_conversion(COLORCONV_TOTAL + COLORCONV_KEEP_TRANS);

		if (windowed)
		{
			set_gfx_mode(GFX_AUTODETECT_WINDOWED, game_screen_width, game_screen_height, 0, 0);
		}
		else
		{
			set_gfx_mode(GFX_AUTODETECT_FULLSCREEN, game_screen_width, game_screen_height, 0, 0);
		}

		// Try to set buffering method by trying each one in turn until you score a hit!

		update_method = UNSET;

		for (t=TRIPLEBUFFER; t<=DOUBLEBUFFER; t++)
		{
			if (OUTPUT_select_buffering_method(t) == true)
			{
				update_method = t;
			}
		}

		if (update_method == UNSET)
		{
			OUTPUT_message("No available software buffering mode available!");
			assert(0);
		}

	}
	else
	{
		// Set it up!
		install_allegro_gl();
		allegro_gl_clear_settings();

		// Set the colour depth to 16/32 or whatever!
		allegro_gl_set(AGL_COLOR_DEPTH, colour_depth);
		
		// Set the z-buffer depth to 8-bit even though I don't use the z-buffer. Hrm. That's probably wasted memory... D'oh.
		allegro_gl_set(AGL_Z_DEPTH, 8);

		// Double-buffer, please, sir!
		allegro_gl_set(AGL_DOUBLEBUFFER, 1);

		// Hardware accellerated graphics please, sir!
		allegro_gl_set(AGL_RENDERMETHOD, 1);

		if (windowed)
		{
			allegro_gl_set(AGL_WINDOWED, TRUE);
			// Suggest?! I FUCKING DEMAND!
			allegro_gl_set(AGL_SUGGEST, AGL_COLOR_DEPTH | AGL_DOUBLEBUFFER | AGL_RENDERMETHOD | AGL_Z_DEPTH | AGL_WINDOWED);
		}
		else
		{
			allegro_gl_set(AGL_WINDOWED, FALSE);
			// Suggest?! I FUCKING DEMAND! AGAIN!
			allegro_gl_set(AGL_SUGGEST, AGL_COLOR_DEPTH | AGL_DOUBLEBUFFER | AGL_RENDERMETHOD | AGL_Z_DEPTH | AGL_WINDOWED);
		}

		// This is for the software stuff...
		set_color_depth(colour_depth);
		set_color_conversion(COLORCONV_TOTAL + COLORCONV_KEEP_TRANS);

		// And open our sexy OpenGL window! :)
		result = set_gfx_mode(GFX_OPENGL, game_screen_width, game_screen_height, 0, 0);

		MAIN_add_to_log ("Setting up AllegroGL graphics & screen mode...");

		if (result == 0)
		{
			MAIN_add_to_log ("\tOK!");
		}
		else
		{
			MAIN_add_to_log ("\tFAILED!");
		}

		#ifdef ALLEGRO_MACOSX
			if (result<0)
			{
				allegro_message("Error setting OpenGL graphics mode:\n%s\nAllegro GL error : %s\n",allegro_error, allegro_gl_error);
				exit(1);
			}
		#else
			if (result)
			{
				allegro_message("Error setting OpenGL graphics mode:\n%s\n","Allegro GL error : %s\n",allegro_error, allegro_gl_error);
			}
		#endif

		/* Setup OpenGL like we want */

		// Textures? Yes please!
		glEnable(GL_TEXTURE_2D);

		// I don't want depth testing as I do all my own z-buffering in software and this'd fuck it up.
//		glEnable(GL_DEPTH_TEST);

		// Aw hell, it's been so long... What's this for?!
		glPolygonMode(GL_FRONT, GL_FILL);

		// Smooth shading, ta'.
		glShadeModel(GL_SMOOTH);

		glHint (GL_PERSPECTIVE_CORRECTION_HINT,GL_NICEST);

		// As I use rectangular regions of the screen, I enable scissor testing which clips polygons to a screen region rather than a 3D space. Great for 2D!
		glEnable(GL_SCISSOR_TEST);

		// But this'll just get fucking ignored by some graphics cards anyway...
		glLineWidth(1.0f);

		// This makes sense...
		glViewport(0, 0, SCREEN_W, SCREEN_H);

		// Make it so we're frigging with the camera now...
		glMatrixMode(GL_PROJECTION);

		// Reset the matrix stack!
		glLoadIdentity();
		
		// Orthographic projection for the win! No perspective for me!
		glOrtho(0,virtual_screen_width,0,virtual_screen_height,0,255); // That 0,255 stuff is the depth of the scene, but I don't use it really.

		/* Set culling mode - not that we have anything to cull */
//		glEnable(GL_CULL_FACE);
//		glFrontFace(GL_CCW);

		glGetFloatv(GL_PROJECTION_MATRIX, ProjectionMatrix);

		// And set the matrixmode to the model view so we're not faffing with the camera any more.
		glMatrixMode(GL_MODELVIEW);

		// This just loads and dumps the list of extension the current version of opengl supports.
		// It's use so we can see what version of opengl we're using is and disallow certain functions
		// based on that.
		OUTPUT_get_opengl_extensions ();

		// We output it, too. Uh-huh.
		if (output_debug_information)
		{
			OUTPUT_write_opengl_extensions_to_file();
		}

		// Do we have secondary colours? (this is a bit of an odd one which doesn't always get supported)
		if (OUTPUT_is_extension_supported("GL_EXT_secondary_color") == true)
		{
			secondary_colour_available = true;
		}
		else
		{
			secondary_colour_available = false;
		}

		// This is the biggy, if we have OpenGL 1.3 then hopefully we'll have these, as they allow for some really nice effects.
		if ( (OUTPUT_is_extension_supported ("GL_ARB_texture_env_combine") == true) && (OUTPUT_is_extension_supported("GL_ARB_multitexture") == true) && (((opengl_major_version==1) && (opengl_minor_version>=3)) || (opengl_major_version>1)) )
		{
			// Do we really want the nice effects? (this is for testing the alternate effects on decent machines)
			if (enable_multi_texture_effects_ideally)
			{
				best_texture_combiner_available = true;
				texture_combiner_available = true;
			}
			else
			{
				best_texture_combiner_available = true;
				texture_combiner_available = false;
			}	
		}
		else
		{
			best_texture_combiner_available = false;
			texture_combiner_available = false;
		}

		// The version of AllegroGL I'm using automatically links in all the API calls for OpenGL, except it forgets this one so we have to do it manually.
		pglSecondaryColor3fEXT = (PFNGLSECONDARYCOLOR3FEXTPROC) allegro_gl_get_proc_address ("glSecondaryColor3fEXT");

		// And this one, too. Tsk!
		pglActiveTextureARB = (PFNGLACTIVETEXTUREEXTPROC) allegro_gl_get_proc_address ("glActiveTextureARB");
	}

	// Ew! MAC code! What Peter Hull wrote!
	#ifdef ALLEGRO_WINDOWS
		HWND hWindow = win_get_window();
		HWND hDesktop = GetDesktopWindow();
		
		RECT rcWindow, rcDesktop;
 
		GetWindowRect(hWindow, &rcWindow); 
		GetWindowRect(hDesktop, &rcDesktop);

		int x,y;

		x = (rcDesktop.right - (rcWindow.right-rcWindow.left)) / 2;
		y = (rcDesktop.bottom - (rcWindow.bottom-rcWindow.top)) / 2;

		SetWindowPos(hWindow,NULL,x,y,0,0,SWP_NOSIZE);
	#endif
}



void OUTPUT_draw_sprite (int bitmap_number , int sprite_number, int x, int y, int r, int g, int b)
{

	if (software_mode_active)
	{
		blit ( bmps[bitmap_number].image , active_page, bmps[bitmap_number].sprite_list[sprite_number].x , bmps[bitmap_number].sprite_list[sprite_number].y , x-bmps[bitmap_number].sprite_list[sprite_number].pivot_x , y-bmps[bitmap_number].sprite_list[sprite_number].pivot_y , bmps[bitmap_number].sprite_list[sprite_number].width , bmps[bitmap_number].sprite_list[sprite_number].height );
	}
	else
	{
		glBindTexture(GL_TEXTURE_2D, bmps[bitmap_number].texture);
		glColor3f( float(r)/255.0f , float(g)/255.0f , float(b)/255.0f );

		glLoadIdentity();
		glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);

		glEnable(GL_TEXTURE_2D);

		sprite *sp = &bmps[bitmap_number].sprite_list[sprite_number];

		float u1 = sp->u1;
		float u2 = sp->u2;
		float v1 = sp->v1;
		float v2 = sp->v2;

		float left = -sp->pivot_x;
		float right = sp->width - sp->pivot_x;
		float up = sp->pivot_y;
		float down = - (sp->height - sp->pivot_y);

		glTranslatef(x,virtual_screen_height-y,0.0);

		glBegin(GL_QUADS);
			glTexCoord2f(u1, v1);
			glVertex2f( left, up);
			glTexCoord2f(u1, v2);
			glVertex2f( left, down);
			glTexCoord2f(u2, v2);
			glVertex2f( right, down);
			glTexCoord2f(u2, v1);
			glVertex2f( right, up);
		glEnd();

		glDisable(GL_TEXTURE_2D);
	}
}



void OUTPUT_draw_masked_sprite (int bitmap_number , int sprite_number, int x, int y, int r, int g, int b)
{
	if (software_mode_active)
	{
		masked_blit ( bmps[bitmap_number].image , active_page, bmps[bitmap_number].sprite_list[sprite_number].x , bmps[bitmap_number].sprite_list[sprite_number].y , x-bmps[bitmap_number].sprite_list[sprite_number].pivot_x , y-bmps[bitmap_number].sprite_list[sprite_number].pivot_y , bmps[bitmap_number].sprite_list[sprite_number].width , bmps[bitmap_number].sprite_list[sprite_number].height );
	}
	else
	{
		glBindTexture(GL_TEXTURE_2D, bmps[bitmap_number].texture);
		
		glColor3f( float(r)/255.0f , float(g)/255.0f , float(b)/255.0f );

		glLoadIdentity();
		glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);

		glEnable(GL_TEXTURE_2D);

		glEnable (GL_ALPHA_TEST);
		glAlphaFunc (GL_GREATER, 0.5f);

		sprite *sp = &bmps[bitmap_number].sprite_list[sprite_number];

		float u1 = sp->u1;
		float u2 = sp->u2;
		float v1 = sp->v1;
		float v2 = sp->v2;

		float left = -sp->pivot_x;
		float right = sp->width - sp->pivot_x;
		float up = sp->pivot_y;
		float down = - (sp->height - sp->pivot_y);

		glTranslatef(x,virtual_screen_height-y,0.0);

		glBegin(GL_QUADS);
			glTexCoord2f(u1, v1);
			glVertex2f( left, up);
			glTexCoord2f(u1, v2);
			glVertex2f( left, down);
			glTexCoord2f(u2, v2);
			glVertex2f( right, down);
			glTexCoord2f(u2, v1);
			glVertex2f( right, up);
		glEnd();
		
		glDisable (GL_ALPHA_TEST);
		glDisable(GL_TEXTURE_2D);
	}
}



void OUTPUT_get_sprite_uvs (int bitmap_number , int sprite_number, float &u1, float &v1, float &u2, float &v2)
{
	u1 = bmps[bitmap_number].sprite_list[sprite_number].u1;
	v1 = bmps[bitmap_number].sprite_list[sprite_number].v1;
	u2 = bmps[bitmap_number].sprite_list[sprite_number].u2;
	v2 = bmps[bitmap_number].sprite_list[sprite_number].v2;
}



int OUTPUT_get_sprite_width (int bitmap_number , int sprite_number)
{
	return bmps[bitmap_number].sprite_list[sprite_number].width;
}



int OUTPUT_get_sprite_height (int bitmap_number , int sprite_number)
{
	return bmps[bitmap_number].sprite_list[sprite_number].height;
}



int OUTPUT_get_sprite_pivot_x (int bitmap_number , int sprite_number)
{
	return bmps[bitmap_number].sprite_list[sprite_number].pivot_x;
}



int OUTPUT_get_sprite_pivot_y (int bitmap_number , int sprite_number)
{
	return bmps[bitmap_number].sprite_list[sprite_number].pivot_y;
}



void OUTPUT_draw_bitmap (int bitmap_number, int x, int y, int window)
{
	// This draws the whole image rather than the tiny slice which the sprite
	// might make up.

	if (software_mode_active)
	{
		blit ( bmps[bitmap_number].image , active_page, 0 , 0 , x , y , bmps[bitmap_number].width , bmps[bitmap_number].height );
	}
	else
	{
		glBindTexture(GL_TEXTURE_2D, bmps[bitmap_number].texture);
		glColor3f( 1.0f , 1.0f , 1.0f );

		glLoadIdentity();
		glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);

		glEnable(GL_TEXTURE_2D);

		float u1 = 0.0f;
		float u2 = 1.0f;
		float v1 = 0.0f;
		float v2 = 1.0f;

		float left = 0;
		float right = bmps[bitmap_number].width;
		float up = 0;
		float down = -bmps[bitmap_number].height;

		glTranslatef(x,virtual_screen_height-(y+up-down),0.0);

		glBegin(GL_QUADS);
			glTexCoord2f(u1, v1);
			glVertex2f( left, up);
			glTexCoord2f(u1, v2);
			glVertex2f( left, down);
			glTexCoord2f(u2, v2);
			glVertex2f( right, down);
			glTexCoord2f(u2, v1);
			glVertex2f( right, up);
		glEnd();

		glDisable(GL_TEXTURE_2D);
	}
}



int OUTPUT_sprite_width (int bitmap_number , int sprite_number)
{
	return (bmps[bitmap_number].sprite_list[sprite_number].width);
}



int OUTPUT_sprite_height (int bitmap_number ,int sprite_number)
{
	return (bmps[bitmap_number].sprite_list[sprite_number].height);
}



int OUTPUT_bitmap_width (int bitmap_number)
{
	return (bmps[bitmap_number].width);	
}



int OUTPUT_bitmap_height (int bitmap_number)
{
	return (bmps[bitmap_number].height);
}



int OUTPUT_bitmap_frames (int bitmap_number)
{
	return (bmps[bitmap_number].sprite_count);
}



#define CHUNK_SIZE		(128)



void INPUT_create_sprite_space (int parent_bitmap, int total_sprites)
{
	bmps[parent_bitmap].sprite_count = total_sprites;
	bmps[parent_bitmap].sprite_list = (sprite *) malloc (total_sprites * sizeof(sprite));
}



void INPUT_create_equal_sized_sprite_series (int parent_bitmap, char *name_base, int sprite_width, int sprite_height, int pivot_x, int pivot_y)
{
	// This uses the dimensions of the parent bitmap to make a series of sprites.
	// If it does not divide exactly then some fragment sprites will be generated.

	char full_name[NAME_SIZE];
	int counter;
	int x,y;
	int total_sprites;

	total_sprites = (bmps[parent_bitmap].height / sprite_height) * (bmps[parent_bitmap].width / sprite_width);
	INPUT_create_sprite_space (parent_bitmap , total_sprites);

	counter = 0;

	for (y=0; y<bmps[parent_bitmap].height-(sprite_height-1) ; y+=sprite_height)
	{
		for (x=0; x<(bmps[parent_bitmap].width-(sprite_width-1)) ; x+=sprite_width)
		{
			sprintf(full_name,"%s%i",name_base,counter);
			INPUT_create_sprite (parent_bitmap, counter, full_name, x, y, sprite_width, sprite_height, pivot_x, pivot_y);
			counter++;
		}
	}
}



int INPUT_get_nearest_value (int value, int compare_1, int compare_2, int compare_3)
{
	int dist_1,dist_2,dist_3;

	dist_1 = MATH_abs_int (compare_1 - value);
	dist_2 = MATH_abs_int (compare_2 - value);
	dist_3 = MATH_abs_int (compare_3 - value);

	if (dist_1 < dist_2)
	{
		if (dist_1 < dist_3)
		{
			return compare_1;
		}
		else
		{
			return compare_3;
		}
	}
	else
	{
		if (dist_2 < dist_3)
		{
			return compare_2;
		}
		else
		{
			return compare_3;
		}
	}


}



#define ARB_DATA_UNIT_SIZE	(6)

void INPUT_create_arb_list (int parent_bitmap, char *descriptor_file)
{
	int *data = NULL;
	int *data_pointer = NULL;
	int lines = 0;
	int t;

	int mx;
	int my;

	int grabbing_stage = 0;

	int start_x,start_y,end_x,end_y;
	int rounded_mx,rounded_my;
	int pivot_x,pivot_y;

	while (CONTROL_key_hit(KEY_ESC) == false)
	{
		OUTPUT_clear_screen();
		CONTROL_update_all_input ();

		mx = CONTROL_mouse_x();
		my = CONTROL_mouse_y();

		rounded_mx = mx - (mx % 8);
		rounded_my = my - (my % 8);

		if (CONTROL_mouse_hit(LMB) == true)
		{
			if (grabbing_stage == 0)
			{
				// Highlighting the bounding box...
				grabbing_stage = 1;
				start_x = rounded_mx;
				start_y = rounded_my;
			}
			else if (grabbing_stage == 2)
			{
				// Choosing offset...
				grabbing_stage = 0;

				if (data == NULL)
				{
					data = (int *) malloc (sizeof(int) * ARB_DATA_UNIT_SIZE);
				}
				else
				{
					data = (int *) realloc (data , sizeof(int) * ARB_DATA_UNIT_SIZE * (lines+1) );
				}

				data_pointer = &data[ARB_DATA_UNIT_SIZE * lines];

				data_pointer[0] = start_x;
				data_pointer[1] = start_y;
				data_pointer[2] = end_x - start_x;
				data_pointer[3] = end_y - start_y;
				data_pointer[4] = pivot_x - start_x;
				data_pointer[5] = pivot_y - start_y;

				lines++;
			}
		}

		if ( (CONTROL_mouse_hit(RMB) == true) && (grabbing_stage == 0) && (lines > 0) )
		{
			if (lines > 1)
			{
				lines--;
				data = (int *) realloc (data , sizeof(int) * ARB_DATA_UNIT_SIZE * lines );
			}
			else
			{
				free (data);
				data = NULL;
				lines--;
			}
		}

		OUTPUT_draw_bitmap(parent_bitmap,0,0);

		// Draw out pre-chosen boxes...

		for (t=0; t<lines; t++)
		{
			data_pointer = &data[ARB_DATA_UNIT_SIZE * t];
			OUTPUT_filled_rectangle_by_size (data_pointer[0],data_pointer[1],data_pointer[2],data_pointer[3],0,0,0);
		}

		if (grabbing_stage == 1)
		{
			OUTPUT_rectangle(start_x,start_y,rounded_mx,rounded_my);

			if (CONTROL_mouse_down(LMB) == false)
			{
				grabbing_stage = 2;
				end_x = rounded_mx;
				end_y = rounded_my;
			}
		}
		else if (grabbing_stage == 2)
		{
			OUTPUT_rectangle(start_x,start_y,end_x,end_y,128,128,128);

			pivot_x = INPUT_get_nearest_value (rounded_mx, start_x, (start_x+end_x)/2, end_x-1);
			pivot_y = INPUT_get_nearest_value (rounded_my, start_y, (start_y+end_y)/2, end_y-1);

			OUTPUT_vline(pivot_x,start_y,end_y);
			OUTPUT_hline(start_x,pivot_y,end_x);
		}

		// Draw mouse_pointer...

		OUTPUT_hline (rounded_mx,rounded_my,rounded_mx+16);
		OUTPUT_vline (rounded_mx,rounded_my,rounded_my+16);
		OUTPUT_line (rounded_mx,rounded_my, rounded_mx+32, rounded_my+32);

		if (grabbing_stage == 0)
		{
		}
		else if (grabbing_stage == 1)
		{
		}
		else if (grabbing_stage == 2)
		{
		}
		
		OUTPUT_updatescreen();
	}

	FILE *file_pointer = fopen (descriptor_file,"w");
	char line[MAX_LINE_SIZE];
	
	if (file_pointer != NULL)
	{
		sprintf(line,"%i\n",lines);

		fputs(line,file_pointer);

		for (t=0; t<lines; t++)
		{
			data_pointer = &data[ARB_DATA_UNIT_SIZE * t];
			sprintf(line,"%i,%i,%i,%i,%i,%i \t// %3i = \n",data_pointer[0],data_pointer[1],data_pointer[2],data_pointer[3],data_pointer[4],data_pointer[5],t);

			fputs(line,file_pointer);
		}

		fclose (file_pointer);
	}
	else
	{
		OUTPUT_message ("Cannot save descriptor file!!!");
		assert(0);
	}

	if (data != NULL)
	{
		free (data);
	}

	CONTROL_update_all_input ();

	MAIN_reset_game_trigger ();
}



void INPUT_create_arbitrary_sized_sprite_series (int parent_bitmap, char *name_base, char *descriptor_file)
{
	// This opens the descriptor file and reads the sprite sizes from it.
	// The descriptor itself is generated by another function.

	char full_pack_filename[MAX_LINE_SIZE];
	char full_name[NAME_SIZE];
	int counter;
	int t;
	int x,y,width,height,pivot_x,pivot_y;
	char line[MAX_LINE_SIZE];
	char *pointer;

	if (load_from_dat_file)
	{
		sprintf(full_pack_filename,"%s\\gfx.dat#%s",MAIN_get_project_name(),descriptor_file);
		fix_filename_slashes(full_pack_filename);
		PACKFILE *packfile_pointer = pack_fopen (full_pack_filename,"r");

		if (packfile_pointer != NULL)
		{
			pack_fgets ( line , MAX_LINE_SIZE , packfile_pointer );
			strtok (line,"\t\n ");
			
			counter = atoi(line);

			INPUT_create_sprite_space (parent_bitmap , counter);

			for (t=0 ; t<counter ; t++)
			{
				sprintf(full_name,"%s%i",name_base,counter);

				pack_fgets ( line , MAX_LINE_SIZE , packfile_pointer );

				pointer = strtok(line,", ");
				x = atoi(pointer);
				pointer = strtok(NULL,", ");
				y = atoi(pointer);
				pointer = strtok(NULL,", ");
				width = atoi(pointer);
				pointer = strtok(NULL,", ");
				height = atoi(pointer);
				pointer = strtok(NULL,", ");
				pivot_x = atoi(pointer);
				pointer = strtok(NULL,", ");
				pivot_y = atoi(pointer);

				INPUT_create_sprite (parent_bitmap, t, full_name, x, y, width, height, pivot_x, pivot_y);
			}

			pack_fclose(packfile_pointer);
		}
		else
		{
			assert(0);
			// No descriptor! Well shit, it really should be in here...
		}
	}
	else
	{
		FILE *file_pointer = fopen (descriptor_file,"r");

		if (file_pointer != NULL)
		{
			counter = FILE_get_int_from_file (file_pointer);

			INPUT_create_sprite_space (parent_bitmap , counter);

			for (t=0 ; t<counter ; t++)
			{
				sprintf(full_name,"%s%i",name_base,counter);

				fgets ( line , MAX_LINE_SIZE , file_pointer );

				pointer = strtok(line,", ");
				x = atoi(pointer);
				pointer = strtok(NULL,", ");
				y = atoi(pointer);
				pointer = strtok(NULL,", ");
				width = atoi(pointer);
				pointer = strtok(NULL,", ");
				height = atoi(pointer);
				pointer = strtok(NULL,", ");
				pivot_x = atoi(pointer);
				pointer = strtok(NULL,", ");
				pivot_y = atoi(pointer);

				INPUT_create_sprite (parent_bitmap, t, full_name, x, y, width, height, pivot_x, pivot_y);
			}

			fclose(file_pointer);
		}
		else
		{
			assert(0);
			// No descriptor! Do it in the Blitz Plus editor, dammit!
		}
	}
}



int INPUT_load_bitmap (char *filename)
{
	#ifdef RETRENGINE_DEBUG_VERSION_THE_LAST_THING_I_DID
		char debug_text[MAX_LINE_SIZE];
		sprintf(debug_text,"Entered INPUT_load_bitmap function...");
		MAIN_debug_last_thing (debug_text);
	#endif

	RGB *pal = NULL;

	// First of all create space for the bitmap by mallocing or reallocing
	// the space needed.

	if (total_bitmaps_loaded == total_bitmaps_allocated)
	{
		if (bmps == NULL)
		{
			bmps = (bitmap_holder *) malloc (sizeof(bitmap_holder) * CHUNK_SIZE);
			total_bitmaps_allocated = CHUNK_SIZE;
		}
		else
		{
			bmps = (bitmap_holder *) realloc (bmps , sizeof(bitmap_holder) * (CHUNK_SIZE + total_bitmaps_allocated) );
			total_bitmaps_allocated += CHUNK_SIZE;
		}
	}

	if (load_from_dat_file)
	{
		DATAFILE *data_pointer = find_datafile_object(gfx_dat, filename);

		#ifdef RETRENGINE_DEBUG_VERSION_THE_LAST_THING_I_DID
			sprintf(debug_text,"Found data pointer %p",data_pointer);
			MAIN_debug_last_thing (debug_text);
		#endif

		bmps[total_bitmaps_loaded].image = (BITMAP *) data_pointer->dat;
	}
	else
	{
		bmps[total_bitmaps_loaded].image = load_bitmap( filename , pal);
	}

	bmps[total_bitmaps_loaded].width = bmps[total_bitmaps_loaded].image->w;
	bmps[total_bitmaps_loaded].height = bmps[total_bitmaps_loaded].image->h;

	if (software_mode_active == false)
	{
		// After we've turned it into a texture then we can get rid of the software
		// bitmap to free up memory. Woot!

		#ifdef RETRENGINE_DEBUG_VERSION_THE_LAST_THING_I_DID
			sprintf(debug_text,"About to make masked texture of %i,%i.",bmps[total_bitmaps_loaded].width,bmps[total_bitmaps_loaded].height);
			MAIN_debug_last_thing (debug_text);
		#endif

		bmps[total_bitmaps_loaded].texture = allegro_gl_make_masked_texture (bmps[total_bitmaps_loaded].image);

		#ifdef RETRENGINE_DEBUG_VERSION_THE_LAST_THING_I_DID
			sprintf(debug_text,"Made masked texture.");
			MAIN_debug_last_thing (debug_text);
		#endif

		if (load_from_dat_file == false)
		{
			destroy_bitmap (bmps[total_bitmaps_loaded].image);
			bmps[total_bitmaps_loaded].image = NULL;
		}
	}

	bmps[total_bitmaps_loaded].sprite_list = NULL;

	total_bitmaps_loaded++;

	return (total_bitmaps_loaded-1);
}



void INPUT_create_sprite (int parent_bitmap, int sprite_number, char *name, int x, int y, int width, int height, int pivot_x, int pivot_y)
{
	// First of all create space for the sprite by mallocing or reallocing
	// the space needed.

	float unit_x,unit_y;

	strcpy (bmps[parent_bitmap].sprite_list[sprite_number].name , name);
	bmps[parent_bitmap].sprite_list[sprite_number].parent_bitmap = parent_bitmap;
	
	bmps[parent_bitmap].sprite_list[sprite_number].x = x;
	bmps[parent_bitmap].sprite_list[sprite_number].y = y;
	bmps[parent_bitmap].sprite_list[sprite_number].width = width;
	bmps[parent_bitmap].sprite_list[sprite_number].height = height;
	bmps[parent_bitmap].sprite_list[sprite_number].pivot_x = pivot_x;
	bmps[parent_bitmap].sprite_list[sprite_number].pivot_y = pivot_y;

	unit_x = 1.0f / float (bmps[parent_bitmap].width);
	unit_y = 1.0f / float (bmps[parent_bitmap].height);

	bmps[parent_bitmap].sprite_list[sprite_number].u1 = float (x) * unit_x;
	bmps[parent_bitmap].sprite_list[sprite_number].u2 = float (x+width) * unit_x;
	bmps[parent_bitmap].sprite_list[sprite_number].v1 = 1.0f - (float (y) * unit_y);
	bmps[parent_bitmap].sprite_list[sprite_number].v2 = 1.0f - (float (y+height) * unit_y);
}



void OUTPUT_draw_sprite_scale (int bitmap_number, int sprite_number , int x , int y , float scale , int r, int g, int b)
{
	if (software_mode_active)
	{
		// Use masked_stretch_blit again for speed...

		int new_pivot_x = bmps[bitmap_number].sprite_list[sprite_number].pivot_x * scale;
		int new_pivot_y = bmps[bitmap_number].sprite_list[sprite_number].pivot_y * scale;

		masked_stretch_blit ( bmps[bitmap_number].image , active_page, bmps[bitmap_number].sprite_list[sprite_number].x , bmps[bitmap_number].sprite_list[sprite_number].y , bmps[bitmap_number].sprite_list[sprite_number].width , bmps[bitmap_number].sprite_list[sprite_number].height , x-new_pivot_x , y-new_pivot_y , bmps[bitmap_number].sprite_list[sprite_number].width * scale , bmps[bitmap_number].sprite_list[sprite_number].height * scale );
	}
	else
	{
		glBindTexture(GL_TEXTURE_2D, bmps[bitmap_number].texture);
		glColor3f( float(r)/255.0f , float(g)/255.0f , float(b)/255.0f );

		glLoadIdentity();
		glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);

		glEnable(GL_TEXTURE_2D);

		glEnable (GL_ALPHA_TEST);
		glAlphaFunc (GL_GREATER, 0.5f);

		sprite *sp = &bmps[bitmap_number].sprite_list[sprite_number];

		float u1 = sp->u1;
		float u2 = sp->u2;
		float v1 = sp->v1;
		float v2 = sp->v2;

		float left = -sp->pivot_x;
		float right = sp->width - sp->pivot_x;
		float up = sp->pivot_y;
		float down = - (sp->height - sp->pivot_y);

		left *= scale;
		right *= scale;
		up *= scale;
		down *= scale;

		glTranslatef(x,virtual_screen_height-y,0.0);
		
		glBegin(GL_QUADS);
			glTexCoord2f(u1, v1);
			glVertex2f( left, up);
			glTexCoord2f(u1, v2);
			glVertex2f( left, down);
			glTexCoord2f(u2, v2);
			glVertex2f( right, down);
			glTexCoord2f(u2, v1);
			glVertex2f( right, up);
		glEnd();
		
		glDisable (GL_ALPHA_TEST);
		glDisable(GL_TEXTURE_2D);
	}
}



void OUTPUT_draw_sprite_scale_no_pivot (int bitmap_number, int sprite_number , int x , int y , float scale , int r, int g, int b)
{
	if (software_mode_active)
	{
		// Use masked_stretch_blit again for speed...

		masked_stretch_blit ( bmps[bitmap_number].image , active_page, bmps[bitmap_number].sprite_list[sprite_number].x , bmps[bitmap_number].sprite_list[sprite_number].y , bmps[bitmap_number].sprite_list[sprite_number].width , bmps[bitmap_number].sprite_list[sprite_number].height , x , y , bmps[bitmap_number].sprite_list[sprite_number].width * scale , bmps[bitmap_number].sprite_list[sprite_number].height * scale );
	}
	else
	{
		glBindTexture(GL_TEXTURE_2D, bmps[bitmap_number].texture);
		glColor3f( float(r)/255.0f , float(g)/255.0f , float(b)/255.0f );

		glLoadIdentity();
		glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);

		glEnable(GL_TEXTURE_2D);

		glEnable (GL_ALPHA_TEST);
		glAlphaFunc (GL_GREATER, 0.5f);

		sprite *sp = &bmps[bitmap_number].sprite_list[sprite_number];

		float u1 = sp->u1;
		float u2 = sp->u2;
		float v1 = sp->v1;
		float v2 = sp->v2;

		float left = -sp->pivot_x;
		float right = sp->width - sp->pivot_x;
		float up = sp->pivot_y;
		float down = - (sp->height - sp->pivot_y);

		left *= scale;
		right *= scale;
		up *= scale;
		down *= scale;

		glTranslatef(x-left,virtual_screen_height-(y+up),0.0);

		glBegin(GL_QUADS);
			glTexCoord2f(u1, v1);
			glVertex2f( left, up);
			glTexCoord2f(u1, v2);
			glVertex2f( left, down);
			glTexCoord2f(u2, v2);
			glVertex2f( right, down);
			glTexCoord2f(u2, v1);
			glVertex2f( right, up);
		glEnd();
		
		glDisable (GL_ALPHA_TEST);
		glDisable(GL_TEXTURE_2D);
	}

}


void OUTPUT_shut_down_screen_update (void)
{
	int t;

	for (t=0; t<3; t++)
	{
		if (pages[t])
		{
			destroy_bitmap (pages[t]);
			pages[t] = NULL;
		}
	}
    
    if ((update_method == DOUBLEBUFFER) || (update_method == SYSTEMBUFFER))
    {
        destroy_bitmap (active_page);
		active_page = NULL;

        destroy_bitmap (current_page);
		current_page = NULL;
    }
}



void OUTPUT_shutdown (void)
{
	// Destroys all the bitmaps in memory...

	MAIN_add_to_log ("Destroying graphics...");

	int bitmap_number;

	if (software_mode_active)
	{
		for (bitmap_number=0 ; bitmap_number < total_bitmaps_loaded ; bitmap_number++)
		{
			if (load_from_dat_file == false) // You really don't wanna' try destroying them inside the packfile.
			{
				destroy_bitmap(bmps[bitmap_number].image);
				bmps[bitmap_number].image = NULL;
			}
		}

		OUTPUT_shut_down_screen_update ();
	}

	MAIN_add_to_log ("\tOK!");
}



#define ARROW_SEGMENT_LENGTH		(16)

void OUTPUT_draw_arrow (int x1, int y1, int x2, int y2, int red, int green, int blue, bool suppress_line)
{
	if (suppress_line == false)
	{
		OUTPUT_line (x1,y1,x2,y2,red,green,blue);
	}

	float angle = atan2( (x2-x1) , (y2-y1) );
	float arrow_angle_1 = angle - (PI/4);
	float arrow_angle_2 = angle + (PI/4);
	
	int dx = (x2-x1);
	int dy = (y2-y1);

	int mid_x = x1 + ((dx*2)/3);
	int mid_y = y1 + ((dy*2)/3);

	OUTPUT_line ( mid_x, mid_y, mid_x-(ARROW_SEGMENT_LENGTH*sin(arrow_angle_1)), mid_y-(ARROW_SEGMENT_LENGTH*cos(arrow_angle_1)), red, green, blue );
	OUTPUT_line ( mid_x, mid_y, mid_x-(ARROW_SEGMENT_LENGTH*sin(arrow_angle_2)), mid_y-(ARROW_SEGMENT_LENGTH*cos(arrow_angle_2)), red, green, blue );
}



void OUTPUT_set_texture_parameters (int opengl_booleans, int old_opengl_booleans)
{
	// This is called by the various draw mode types in order to make sure everything is set up nicely.
	
	if (opengl_booleans != old_opengl_booleans)
	{
		// This section only deals with turning on and off states.

		if ( !(opengl_booleans & OPENGL_BOOLEAN_FILTERED) && (old_opengl_booleans & OPENGL_BOOLEAN_FILTERED) )
		{
			// Turn off linear filtering

			#ifdef RETRENGINE_DEBUG_VERSION_THE_LAST_THING_I_DID
				MAIN_debug_last_thing ("Turn Off Texture Filtering");
			#endif

			glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
			glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);
		}
		else if ( (opengl_booleans & OPENGL_BOOLEAN_FILTERED) && !(old_opengl_booleans & OPENGL_BOOLEAN_FILTERED) )
		{
			// Turn on linear filtering

			#ifdef RETRENGINE_DEBUG_VERSION_THE_LAST_THING_I_DID
				MAIN_debug_last_thing ("Turn On Texture Filtering");
			#endif

			glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
		}
	}

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
}



void OUTPUT_draw_starfield (int starfield_id)
{
	int starfield_number = PARTICLES_get_index_from_id (starfield_id);

	if (starfield_number == UNSET)
	{
		// Invalid ID.
		return;
	}

	if (starfields[starfield_number].in_use == false)
	{
		// Trying to draw a non-existant starfield! Naughty!
		return;
	}

	starfield_controller_struct *scp = &starfields[starfield_number];
	starfield_struct *sfp = &scp->stars[0];

	int star_count = scp->number_of_stars;

	int counter;

	for (counter = 0; counter < star_count; counter++)
	{
		glBegin(GL_POINTS);
			glVertex2f( sfp->x , -sfp->y);
		glEnd();

		sfp++;
	}
}



void OUTPUT_draw_starfield_colour (int starfield_id)
{
	int starfield_number = PARTICLES_get_index_from_id (starfield_id);

	if (starfields[starfield_number].in_use == false)
	{
		// Trying to draw a non-existant starfield! Naughty!
		return;
	}

	starfield_controller_struct *scp = &starfields[starfield_number];
	starfield_struct *sfp = &scp->stars[0];

	int star_count = scp->number_of_stars;

	int counter;
	int ramp_entry;

	for (counter = 0; counter < star_count; counter++)
	{
		ramp_entry = sfp->percentage_speed;

		glBegin(GL_POINTS);
			glColor3f ( scp->red_ramp[ramp_entry] , scp->green_ramp[ramp_entry] , scp->blue_ramp[ramp_entry] );
			glVertex2f( sfp->x , -sfp->y);
		glEnd();

		sfp++;
	}
}



void OUTPUT_draw_starfield_lines (int starfield_id)
{
	int starfield_number = PARTICLES_get_index_from_id (starfield_id);

	if (starfields[starfield_number].in_use == false)
	{
		// Trying to draw a non-existant starfield! Naughty!
		return;
	}

	starfield_controller_struct *scp = &starfields[starfield_number];
	starfield_struct *sfp = &scp->stars[0];

	int star_count = scp->number_of_stars;

	int counter;

	for (counter = 0; counter < star_count; counter++)
	{
		glBegin(GL_LINES);
			glVertex2f( sfp->x , -sfp->y);
			glVertex2f( sfp->ox , -sfp->oy);
		glEnd();

		sfp++;
	}
}



void OUTPUT_draw_starfield_colour_lines (int starfield_id)
{
	int starfield_number = PARTICLES_get_index_from_id (starfield_id);

	if (starfields[starfield_number].in_use == false)
	{
		// Trying to draw a non-existant starfield! Naughty!
		return;
	}

	starfield_controller_struct *scp = &starfields[starfield_number];
	starfield_struct *sfp = &scp->stars[0];

	int star_count = scp->number_of_stars;

	int counter;
	int ramp_entry;

	for (counter = 0; counter < star_count; counter++)
	{
		ramp_entry = sfp->percentage_speed;

		glBegin(GL_LINES);
			glColor3f ( scp->red_ramp[ramp_entry] , scp->green_ramp[ramp_entry] , scp->blue_ramp[ramp_entry] );
			glVertex2f( sfp->x , -sfp->y);
			glVertex2f( sfp->ox , -sfp->oy);
		glEnd();

		sfp++;
	}
}



void OUTPUT_blank_window_contents (int window_number)
{
	int z_ordering_list;
	int z_ordering_list_size = window_details[window_number].z_ordering_list_size;
	
	for (z_ordering_list=0; z_ordering_list<z_ordering_list_size; z_ordering_list++)
	{
		window_details[window_number].z_ordering_list_starts[z_ordering_list] = UNSET;
		window_details[window_number].z_ordering_list_ends[z_ordering_list] = UNSET;
	}
}



void OUTPUT_list_all_window_contents (int look_for_script)
{
	int window_number;
	int counter = 0;

	for (window_number=0; window_number<number_of_windows; window_number++)
	{
		if (window_details[window_number].active == true)
		{
			window_struct *wp = &window_details[window_number];

			int *entity_pointer;

			int current_entity;

			int z_ordering_list;
			int z_ordering_list_size = wp->z_ordering_list_size;

			for (z_ordering_list=0; z_ordering_list<z_ordering_list_size; z_ordering_list++)
			{
				current_entity = window_details[window_number].z_ordering_list_starts[z_ordering_list];

				while (current_entity != UNSET)
				{
					entity_pointer = &entity[current_entity][0];

					if (entity_pointer[ENT_SCRIPT_NUMBER]==look_for_script)
					{
						counter++;
					}

					current_entity = entity_pointer[ENT_NEXT_WINDOW_ENT];
				}
			}

		}
	}
}



void OUTPUT_draw_window_collision_contents (int window_number, bool draw_world_collision)
{
	int z_ordering_list_size = window_details[window_number].z_ordering_list_size;
	int z_ordering_list;

	float left,right,up,down;
	int x;
	int y;

	window_struct *wp = &window_details[window_number];

	int *entity_pointer;

	int window_x = wp->current_x;
	int window_y = wp->current_y;

	int window_width = wp->width;
	int window_height = wp->height;

	int current_entity;

	float rotate_angle;

	char debug_text[MAX_LINE_SIZE];

	#ifdef RETRENGINE_DEBUG_VERSION_THE_LAST_THING_I_DID
		MAIN_debug_last_thing ("Got into draw window collision contents...");
	#endif

	#ifdef RETRENGINE_DEBUG_VERSION_THE_LAST_THING_I_DID
		sprintf (debug_text,"Collision window number %i = %i %i ",window_number,window_details[window_number].width,window_details[window_number].height);
		MAIN_debug_last_thing (debug_text);
	#endif

	float display_scale_x,display_scale_y;
	float window_scale_x,window_scale_y;
	float total_scale_x,total_scale_y;

//	if (game_screen_width != virtual_screen_width)
//	{
//		display_scale_x = game_screen_width/virtual_screen_width;
//	}
//	else
	{
		display_scale_x = 1.0f;
	}

//	if (game_screen_height != virtual_screen_height)
//	{
//		display_scale_y = game_screen_height/virtual_screen_height;
//	}
//	else
	{
		display_scale_y = 1.0f;
	}

	float left_window_transform_x = (wp->screen_x + wp->opengl_transform_x) * display_scale_x;
	float bottom_window_transform_y = virtual_screen_height - ( (wp->screen_y + wp->opengl_transform_y + wp->scaled_height) * display_scale_y);
	float top_window_transform_y = virtual_screen_height - ( (wp->screen_y + wp->opengl_transform_y) * display_scale_y);

	window_scale_x = window_details[window_number].opengl_scale_x;
	window_scale_y = window_details[window_number].opengl_scale_y;

	total_scale_x = window_scale_x * display_scale_x;
	total_scale_y = window_scale_y * display_scale_y;

	int collision_grid_size = 1<<collision_bitshift;

//	BITMAP *window_buffer = create_sub_bitmap (buffer , window_details[window_number].screen_x , window_details[window_number].screen_y , window_details[window_number].width, window_details[window_number].height );

	if (wp->scaled_width > 0.0f)
	{
		if (wp->scaled_height > 0.0f)
		{
			glScissor ( left_window_transform_x , bottom_window_transform_y , wp->scaled_width * display_scale_x , wp->scaled_height * display_scale_y );
		}
		else
		{
			glScissor ( left_window_transform_x , bottom_window_transform_y + wp->scaled_height , wp->scaled_width * display_scale_x , -wp->scaled_height * display_scale_y );
		}
	}
	else
	{
		if (wp->scaled_height > 0.0f)
		{
			glScissor ( left_window_transform_x + wp->scaled_width , bottom_window_transform_y , -wp->scaled_width * display_scale_x , wp->scaled_height * display_scale_y );
		}
		else
		{
			glScissor ( left_window_transform_x + wp->scaled_width , bottom_window_transform_y + wp->scaled_height , -wp->scaled_width * display_scale_x , -wp->scaled_height * display_scale_y );
		}
	}

	#ifdef RETRENGINE_DEBUG_VERSION_THE_LAST_THING_I_DID
		MAIN_debug_last_thing ("Reset main colour to white...");
	#endif

	glColor4f( 1.0f , 1.0f , 1.0f , 1.0f);

	if (window_details[window_number].skip_me_this_frame)
	{
		for (z_ordering_list=0; z_ordering_list<z_ordering_list_size; z_ordering_list++)
		{
			window_details[window_number].z_ordering_list_starts[z_ordering_list] = UNSET;
			window_details[window_number].z_ordering_list_ends[z_ordering_list] = UNSET;
		}

		#ifdef RETRENGINE_DEBUG_VERSION_THE_LAST_THING_I_DID
			MAIN_debug_last_thing ("Window skipped");
		#endif
	}
	else
	{
		glDisable(GL_TEXTURE_2D);

		// Draw the collision grid at the correct resolution.

		int col_grid_x_offset = window_x % collision_grid_size;
		int col_grid_y_offset = window_y % collision_grid_size;

		glLoadIdentity();
		glTranslatef(left_window_transform_x,top_window_transform_y,0);
		glScalef (total_scale_x,total_scale_y,0);

		glColor3f(0.0f,0.0f,1.0f);

		if (draw_world_collision == false)
		{
			for (x=0; x<window_details[window_number].width/total_scale_x; x+=collision_grid_size)
			{
				glBegin(GL_LINES);
					glVertex2f( x-col_grid_x_offset, 0 );
					glVertex2f( x-col_grid_x_offset, -window_details[window_number].height/total_scale_y );
				glEnd();
			}

			for (y=0; y<window_details[window_number].height/total_scale_y; y+=collision_grid_size)
			{
				glBegin(GL_LINES);
					glVertex2f( 0 , -(y-col_grid_y_offset) );
					glVertex2f( window_details[window_number].width/total_scale_x , -(y-col_grid_y_offset) );
				glEnd();
			}
		}

		int z_ordering_list;

		for (z_ordering_list=0; z_ordering_list<z_ordering_list_size; z_ordering_list++)
		{
			glLoadIdentity();
			glTranslatef(left_window_transform_x,top_window_transform_y,0);
			glScalef (total_scale_x,total_scale_y,0);

			current_entity = window_details[window_number].z_ordering_list_starts[z_ordering_list];

			while (current_entity != UNSET)
			{
				entity_pointer = &entity[current_entity][0];

				glLoadIdentity();
				glTranslatef(left_window_transform_x,top_window_transform_y,0);
				glScalef (total_scale_x,total_scale_y,0);

				if (draw_world_collision == true)
				{

					x = entity_pointer[ENT_WORLD_X] - window_x;
					y = entity_pointer[ENT_WORLD_Y] - window_y;

					left = -entity_pointer[ENT_UPPER_WORLD_WIDTH];
					right = entity_pointer[ENT_LOWER_WORLD_WIDTH];
					up = entity_pointer[ENT_UPPER_WORLD_HEIGHT];
					down = -entity_pointer[ENT_LOWER_WORLD_HEIGHT];

					glTranslatef(x,-y,0.0);

					glColor3f(0.0f,1.0f,0.0f);

					glBegin(GL_LINE_LOOP);
						glVertex2f( left, up ); // Bottom left corner
						glVertex2f( left, down ); //. Top left corner
						glVertex2f( right, down ); // Top right corner
						glVertex2f( right, up );  // Bottom right corner
					glEnd();

					glColor3f(1.0f,1.0f,1.0f);

					glBegin(GL_LINES);
						glVertex2f( left, 0 );
						glVertex2f( right, 0 );
					glEnd();

					glBegin(GL_LINES);
						glVertex2f( 0, up );
						glVertex2f( 0, down );
					glEnd();

					glTranslatef(-x,y,0);

				}
				else
				{
					switch (entity_pointer[ENT_COLLISION_SHAPE])
					{

					case SHAPE_NONE:
						break;
						
					case SHAPE_POINT:
						break;

					case SHAPE_CIRCLE:
						x = entity_pointer[ENT_WORLD_X] - window_x;
						y = entity_pointer[ENT_WORLD_Y] - window_y;

						left = -entity_pointer[ENT_RADIUS];
						right = entity_pointer[ENT_RADIUS];
						up = entity_pointer[ENT_RADIUS];
						down = -entity_pointer[ENT_RADIUS];

						glTranslatef(x,-y,0.0);

						glColor3f(0.0f,1.0f,0.0f);

						glBegin(GL_LINE_LOOP);
							glVertex2f( left, 0 ); // Left
							glVertex2f( left*0.707f, up*0.707f ); // Left
							glVertex2f( 0, up ); // Up
							glVertex2f( right*0.707f, up*0.707f ); // Left
							glVertex2f( right, 0 ); // Right
							glVertex2f( right*0.707f, down*0.707f ); // Left
							glVertex2f( 0, down ); // Down
							glVertex2f( left*0.707f, down*0.707f ); // Left
						glEnd();

						glColor3f(1.0f,1.0f,1.0f);

						glBegin(GL_LINES);
							glVertex2f( left, 0 );
							glVertex2f( right, 0 );
						glEnd();

						glBegin(GL_LINES);
							glVertex2f( 0, up );
							glVertex2f( 0, down );
						glEnd();

						glTranslatef(-x,y,0);
						break;

					case SHAPE_ROTATED_RECTANGLE:
					case SHAPE_RECTANGLE:

						x = entity_pointer[ENT_WORLD_X] - window_x;
						y = entity_pointer[ENT_WORLD_Y] - window_y;

						left = -entity_pointer[ENT_UPPER_WIDTH];
						right = entity_pointer[ENT_LOWER_WIDTH];
						up = entity_pointer[ENT_UPPER_HEIGHT];
						down = -entity_pointer[ENT_LOWER_HEIGHT];

						glTranslatef(x,-y,0.0);

						if (entity_pointer[ENT_COLLISION_SHAPE] == SHAPE_ROTATED_RECTANGLE)
						{
							rotate_angle = float (entity_pointer[ENT_OPENGL_ANGLE]) / 100.0f;
							glRotatef (-rotate_angle , 0.0f, 0.0f, 1.0f);
						}

						glColor3f(0.0f,1.0f,0.0f);

						glBegin(GL_LINE_LOOP);
							glVertex2f( left, up ); // Bottom left corner
							glVertex2f( left, down ); //. Top left corner
							glVertex2f( right, down ); // Top right corner
							glVertex2f( right, up );  // Bottom right corner
						glEnd();

						glColor3f(1.0f,1.0f,1.0f);

						glBegin(GL_LINES);
							glVertex2f( left, 0 );
							glVertex2f( right, 0 );
						glEnd();

						glBegin(GL_LINES);
							glVertex2f( 0, up );
							glVertex2f( 0, down );
						glEnd();

						if (entity_pointer[ENT_COLLISION_SHAPE] == SHAPE_ROTATED_RECTANGLE)
						{
							glRotatef (rotate_angle , 0.0f, 0.0f, 1.0f);	
						}

						glTranslatef(-x,y,0);
						break;

					default:
						assert(0);
						// Arse-candles!
						break;
					}
				}

				current_entity = entity[current_entity][ENT_NEXT_WINDOW_ENT];
			}

			window_details[window_number].z_ordering_list_starts[z_ordering_list] = UNSET;
			window_details[window_number].z_ordering_list_ends[z_ordering_list] = UNSET;
		}

	}

	glScissor ( 0 , 0 , game_screen_width , game_screen_height );

	glDisable (GL_BLEND);
	glDisable (GL_ALPHA_TEST);
	glColor4f (1.0f, 1.0f, 1.0f, 1.0f);

	if (window_details[window_number].secondary_window_colour == true)
	{
		glDisable(GL_COLOR_SUM);
	}

}



void OUTPUT_destroy_bitmap_and_sprite_containers (void)
{
	if (bmps != NULL)
	{
		int bitmap_number;

		for (bitmap_number=0; bitmap_number<total_bitmaps_loaded; bitmap_number++)
		{
			if (bmps[bitmap_number].sprite_list != NULL)
			{
				free (bmps[bitmap_number].sprite_list);
			}
		}

		free (bmps);
	}
}



int OUTPUT_draw_window_contents (int window_number, bool texture_combiner_available)
{
	// Constrain drawing buffer to window and then get weaving!
	// Goes through all the z-ordering queues splunking out the contents to the screen.

	char debug_text[MAX_LINE_SIZE];

	#ifdef RETRENGINE_DEBUG_VERSION_THE_LAST_THING_I_DID
		MAIN_debug_last_thing ("Got into draw window contents...");
	#endif

	#ifdef RETRENGINE_DEBUG_VERSION_THE_LAST_THING_I_DID
		sprintf (debug_text,"Window number %i = %i %i ",window_number,window_details[window_number].width,window_details[window_number].height);
		MAIN_debug_last_thing (debug_text);
	#endif

	int z_ordering_list_size = window_details[window_number].z_ordering_list_size;
	int z_ordering_list;

	int old_bitmap_number = UNSET;
	int bitmap_number;

	int frame_number;

	int total_entities_drawn = 0;

	float u1,v1,u2,v2;

	float left,right,up,down;
	int x;
	int y;

	int opengl_booleans;
	int old_opengl_booleans = 0;

	int secondary_frame_number;
	float secondary_u1,secondary_v1,secondary_u2,secondary_v2;
	int secondary_bitmap_number;
	int old_secondary_bitmap_number = UNSET;
	int secondary_opengl_booleans;
	int old_secondary_opengl_booleans = 0;

	int start_bx,start_by,end_bx,end_by;

	float interp_u1,interp_v1,interp_u2,interp_v2;
	float interp_x,interp_y;
	int interp_frame_number;
	int secondary_interp_frame_number;
	
	int bx,by;

	int x_under, y_under;

	short *map_pointer;
	short *map_line_pointer;
	
	short *opt_pointer;
	short *opt_offset;

	sprite *sp = NULL;
	sprite *interp_sp = NULL;

	sprite *secondary_sp = NULL;
	sprite *secondary_interp_sp = NULL;

	window_struct *wp = &window_details[window_number];

	int tileset;
	int tilesize;
	int tile_graphic;

	int layer_number;

	int map_number;

	int *entity_pointer;

	int draw_type;

	int map_width,map_height;

	int window_x = wp->current_x;
	int window_y = wp->current_y;

	int window_width = wp->width;
	int window_height = wp->height;

	int current_entity;

	float rotate_angle;
	float scale_x;
	float scale_y;

	float rotate_angle_2;
	float scale_x_2;
	float scale_y_2;

	float temp_uv;

	float tileset_drawing_start_offset_x;
	float tileset_drawing_start_offset_y;
	float next_line_transform_x;
	float next_line_transform_y;

	bool map_optimised;

	arbitrary_quads_struct *aq_pointer;
	arbitrary_vertex_colour_struct *avc_pointer;

	float line_length_ratio;

	#ifdef RETRENGINE_DEBUG_VERSION_THE_LAST_THING_I_DID
		MAIN_debug_last_thing ("Just before secondary colour...");
	#endif

	if (secondary_colour_available)
	{
		if (window_details[window_number].secondary_window_colour == true)
		{
			#ifdef RETRENGINE_DEBUG_VERSION_THE_LAST_THING_I_DID
				MAIN_debug_last_thing ("Setting secondary colour...");
			#endif

			glEnable(GL_COLOR_SUM);
			pglSecondaryColor3fEXT( float(wp->vertex_red)/256.0f , float(wp->vertex_green)/256.0f , float(wp->vertex_blue)/256.0f );
		}
	}
	else
	{
		#ifdef RETRENGINE_DEBUG_VERSION_THE_LAST_THING_I_DID
			MAIN_debug_last_thing ("Skipping secondary colour...");
		#endif
	}

	#ifdef RETRENGINE_DEBUG_VERSION_THE_LAST_THING_I_DID
		MAIN_debug_last_thing ("Just after secondary colour...");
	#endif

	float display_scale_x,display_scale_y;
	float window_scale_x,window_scale_y;
	float total_scale_x,total_scale_y;

//	if (game_screen_width != virtual_screen_width)
//	{
//		display_scale_x = float(game_screen_width)/float(virtual_screen_width);
//	}
//	else
	{
		display_scale_x = 1.0f;
	}

//	if (game_screen_height != virtual_screen_height)
//	{
//		display_scale_y = float(game_screen_height)/float(virtual_screen_height);
//	}
//	else
	{
		display_scale_y = 1.0f;
	}

	float left_window_transform_x = (wp->screen_x + wp->opengl_transform_x) * display_scale_x;
	float bottom_window_transform_y = virtual_screen_height - ( (wp->screen_y + wp->opengl_transform_y + wp->scaled_height) * display_scale_y);
	float top_window_transform_y = virtual_screen_height - ( (wp->screen_y + wp->opengl_transform_y) * display_scale_y);

	window_scale_x = window_details[window_number].opengl_scale_x;
	window_scale_y = window_details[window_number].opengl_scale_y;

	total_scale_x = window_scale_x * display_scale_x;
	total_scale_y = window_scale_y * display_scale_y;

//	BITMAP *window_buffer = create_sub_bitmap (buffer , window_details[window_number].screen_x , window_details[window_number].screen_y , window_details[window_number].width, window_details[window_number].height );

//	float_window_scale_multiplier

	if (wp->scaled_width > 0.0f)
	{
		if (wp->scaled_height > 0.0f)
		{
			glScissor ( left_window_transform_x * float_window_scale_multiplier , bottom_window_transform_y * float_window_scale_multiplier , wp->scaled_width * display_scale_x * float_window_scale_multiplier, wp->scaled_height * display_scale_y * float_window_scale_multiplier );
		}
		else
		{
			glScissor ( left_window_transform_x * float_window_scale_multiplier , (bottom_window_transform_y + wp->scaled_height) * float_window_scale_multiplier , wp->scaled_width * display_scale_x * float_window_scale_multiplier , -wp->scaled_height * display_scale_y * float_window_scale_multiplier );
		}
	}
	else
	{
		if (wp->scaled_height > 0.0f)
		{
			glScissor ( (left_window_transform_x + wp->scaled_width) * float_window_scale_multiplier , bottom_window_transform_y * float_window_scale_multiplier , -wp->scaled_width * display_scale_x * float_window_scale_multiplier , wp->scaled_height * display_scale_y * float_window_scale_multiplier );
		}
		else
		{
			glScissor ( (left_window_transform_x + wp->scaled_width) * float_window_scale_multiplier , (bottom_window_transform_y + wp->scaled_height) * float_window_scale_multiplier , -wp->scaled_width * display_scale_x * float_window_scale_multiplier , -wp->scaled_height * display_scale_y * float_window_scale_multiplier );
		}
	}
	// This restricts all drawing to the given area. Remember that co-ords start from the bottom left and go up-right!
	
	#ifdef RETRENGINE_DEBUG_VERSION_THE_LAST_THING_I_DID
		MAIN_debug_last_thing ("Reset main colour to white...");
	#endif

	glColor4f( 1.0f , 1.0f , 1.0f ,1.0f);

	if (window_details[window_number].skip_me_this_frame)
	{
		#ifndef RETRENGINE_DEBUG_VERSION_VIEW_WORLD_COLLISION
			#ifndef RETRENGINE_DEBUG_VERSION_VIEW_OBJECT_COLLISION
				for (z_ordering_list=0; z_ordering_list<z_ordering_list_size; z_ordering_list++)
				{
					window_details[window_number].z_ordering_list_starts[z_ordering_list] = UNSET;
					window_details[window_number].z_ordering_list_ends[z_ordering_list] = UNSET;
				}

				window_details[window_number].skip_me_this_frame--;
			#endif
		#endif

		#ifdef RETRENGINE_DEBUG_VERSION_THE_LAST_THING_I_DID
			MAIN_debug_last_thing ("Window skipped");
		#endif
	}
	else
	{
		glEnable (GL_TEXTURE_2D);

		for (z_ordering_list=0; z_ordering_list<z_ordering_list_size; z_ordering_list++)
		{
			glLoadIdentity();
			glTranslatef(left_window_transform_x,top_window_transform_y,0);
			glScalef (total_scale_x,total_scale_y,0);

			current_entity = window_details[window_number].z_ordering_list_starts[z_ordering_list];

			while (current_entity != UNSET)
			{
				total_entities_drawn++;

				entity_pointer = &entity[current_entity][0];

				#ifdef RETRENGINE_DEBUG_VERSION_ENTITY_DEBUG_FLAG_CHECKS
				if (entity_pointer[ENT_DEBUG_FLAGS] & DEBUG_FLAG_STOP_WHEN_DRAWN_IN_WINDOW)
				{
					assert(0);
				}
				#endif

				#ifdef RETRENGINE_DEBUG_VERSION
					if ( (entity_pointer[ENT_DRAW_MODE] != DRAW_MODE_SOLID_RECTANGLE) && (entity_pointer[ENT_DRAW_MODE] != DRAW_MODE_STARFIELD) && (entity_pointer[ENT_DRAW_MODE] != DRAW_MODE_STARFIELD_LINES) && (entity_pointer[ENT_DRAW_MODE] != DRAW_MODE_STARFIELD_COLOUR) && (entity_pointer[ENT_DRAW_MODE] != DRAW_MODE_STARFIELD_COLOUR_LINES) )
					{
						if (bmps[entity_pointer[ENT_SPRITE]].sprite_count <= entity_pointer[ENT_CURRENT_FRAME])
						{
							sprintf (debug_text,"Entity %s is trying to access frame %i (max is %i)", GPL_get_entry_name("SCRIPTS",entity_pointer[ENT_SCRIPT_NUMBER]) , entity_pointer[ENT_CURRENT_FRAME] , bmps[entity_pointer[ENT_SPRITE]].sprite_count-1 );
							OUTPUT_message(debug_text);
						}
					}
				#endif

				#ifdef RETRENGINE_DEBUG_VERSION_THE_LAST_THING_I_DID
					sprintf (debug_text,"Entity %i = %s ",current_entity, GPL_get_entry_name("SCRIPTS",entity_pointer[ENT_SCRIPT_NUMBER]) );
					MAIN_debug_last_thing (debug_text);
				#endif

				draw_type = entity_pointer[ENT_DRAW_MODE];

				#ifdef RETRENGINE_DEBUG_VERSION_THE_LAST_THING_I_DID
					sprintf (debug_text,"Draw Type = %i",draw_type );
					MAIN_debug_last_thing (debug_text);

					MAIN_debug_last_thing ("About to check multitexture sprite...");
				#endif

				if (texture_combiner_available)
				{
					secondary_bitmap_number = entity_pointer[ENT_SECONDARY_SPRITE];
					if ((secondary_bitmap_number != UNSET) && (opengl_booleans & (OPENGL_BOOLEAN_MULTITEXTURE_MASK | OPENGL_BOOLEAN_MULTITEXTURE_DOUBLE_MASK) > 0))
					{
						#ifdef RETRENGINE_DEBUG_VERSION_THE_LAST_THING_I_DID
							MAIN_debug_last_thing ("About to set multi-texture sprite...");
						#endif
				
						pglActiveTextureARB(GL_TEXTURE1);

						if (old_secondary_bitmap_number == UNSET)
						{
							glEnable(GL_TEXTURE_2D);
						}
						glBindTexture(GL_TEXTURE_2D, bmps[secondary_bitmap_number].texture);
						pglActiveTextureARB(GL_TEXTURE0);

						#ifdef RETRENGINE_DEBUG_VERSION_THE_LAST_THING_I_DID
							MAIN_debug_last_thing ("Set multitexture sprite...");
						#endif
					}
					else if (old_secondary_bitmap_number != UNSET)
					{
						#ifdef RETRENGINE_DEBUG_VERSION_THE_LAST_THING_I_DID
							MAIN_debug_last_thing ("About to reset multitexture sprite...");
						#endif

						// If we had a multi-texture last frame then we need to first disable the second texture unit...
						pglActiveTextureARB(GL_TEXTURE1);
						glDisable(GL_TEXTURE_2D);
						// Then restore the default modulation to the texture combiner.
						pglActiveTextureARB(GL_TEXTURE0);
						glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE);
						glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB, GL_MODULATE);
						glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_RGB, GL_TEXTURE);
						glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE1_RGB, GL_PRIMARY_COLOR);
						glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_ALPHA, GL_MODULATE);
						glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_ALPHA, GL_TEXTURE);
						glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE1_ALPHA, GL_PRIMARY_COLOR);

						#ifdef RETRENGINE_DEBUG_VERSION_THE_LAST_THING_I_DID
							MAIN_debug_last_thing ("Reset multitexture sprite...");
						#endif
					}
					old_secondary_bitmap_number = secondary_bitmap_number;
				}

				#ifdef RETRENGINE_DEBUG_VERSION_THE_LAST_THING_I_DID
					MAIN_debug_last_thing ("About to bind texture sprite...");
				#endif

				bitmap_number = entity_pointer[ENT_SPRITE];
				if ( (bitmap_number != old_bitmap_number) && (bitmap_number != UNSET) )
				{
					glBindTexture(GL_TEXTURE_2D, bmps[bitmap_number].texture);
				}
				old_bitmap_number = bitmap_number;

				opengl_booleans = entity_pointer[ENT_OPENGL_BOOLEANS];

				if ( (old_bitmap_number != bitmap_number) || (opengl_booleans != old_opengl_booleans) )
				{
					if (opengl_booleans & OPENGL_BOOLEAN_FILTERED)
					{
						#ifdef RETRENGINE_DEBUG_VERSION_THE_LAST_THING_I_DID
							MAIN_debug_last_thing ("About to filter sprite...");
						#endif

						glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
						glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);

						#ifdef RETRENGINE_DEBUG_VERSION_THE_LAST_THING_I_DID
							MAIN_debug_last_thing ("Filtered sprite...");
						#endif
					}
					else
					{
						#ifdef RETRENGINE_DEBUG_VERSION_THE_LAST_THING_I_DID
							MAIN_debug_last_thing ("About to non-filter sprite...");
						#endif

						glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
						glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);

						#ifdef RETRENGINE_DEBUG_VERSION_THE_LAST_THING_I_DID
							MAIN_debug_last_thing ("Non-filtered sprite...");
						#endif
					}
				}

				if (opengl_booleans != old_opengl_booleans)
				{
					if ( !(opengl_booleans & OPENGL_BOOLEAN_ROTATE) && (old_opengl_booleans & OPENGL_BOOLEAN_ROTATE) )
					{
						// ie, if the previous one was rotated and the new one ain't. Reset.

						#ifdef RETRENGINE_DEBUG_VERSION_THE_LAST_THING_I_DID
							MAIN_debug_last_thing ("Resetting Transform as previous entity was rotated and this one ain't.");
						#endif

						glLoadIdentity();
						glTranslatef(left_window_transform_x,top_window_transform_y,0);
						glScalef (total_scale_x,total_scale_y,0);
					}
					else if ( !(opengl_booleans & OPENGL_BOOLEAN_SCALE) && (old_opengl_booleans & OPENGL_BOOLEAN_SCALE) )
					{
						// ie, if the previous one was scaled and the new one ain't. Reset.

						#ifdef RETRENGINE_DEBUG_VERSION_THE_LAST_THING_I_DID
							MAIN_debug_last_thing ("Resetting Transform as previous entity was scaled and this one ain't.");
						#endif

						glLoadIdentity();
						glTranslatef(left_window_transform_x,top_window_transform_y,0);
						glScalef (total_scale_x,total_scale_y,0);
					}

					if ( !(opengl_booleans & OPENGL_BOOLEAN_BLEND_EITHER) && (old_opengl_booleans & OPENGL_BOOLEAN_BLEND_ADD) )
					{
						// Turn off additive blending

						#ifdef RETRENGINE_DEBUG_VERSION_THE_LAST_THING_I_DID
							MAIN_debug_last_thing ("Turn off blending.");
						#endif

						glDisable (GL_BLEND);
					}
					else if ( (opengl_booleans & OPENGL_BOOLEAN_BLEND_ADD) && !(old_opengl_booleans & OPENGL_BOOLEAN_BLEND_ADD) )
					{
						// Turn on additive blending

						#ifdef RETRENGINE_DEBUG_VERSION_THE_LAST_THING_I_DID
							MAIN_debug_last_thing ("Turn on additive blending.");
						#endif

						glEnable (GL_BLEND);
						glBlendFunc(GL_SRC_ALPHA,GL_ONE);
					}
					
					if ( !(opengl_booleans & OPENGL_BOOLEAN_BLEND_EITHER) && (old_opengl_booleans & OPENGL_BOOLEAN_BLEND_MULTIPLY) )
					{
						// Turn off multiplicative blending

						#ifdef RETRENGINE_DEBUG_VERSION_THE_LAST_THING_I_DID
							MAIN_debug_last_thing ("Turn off blending.");
						#endif

						glDisable (GL_BLEND);
					}
					else if ( (opengl_booleans & OPENGL_BOOLEAN_BLEND_MULTIPLY) && !(old_opengl_booleans & OPENGL_BOOLEAN_BLEND_MULTIPLY) )
					{
						// Turn on multiplicative blending

						#ifdef RETRENGINE_DEBUG_VERSION_THE_LAST_THING_I_DID
							MAIN_debug_last_thing ("Turn on multiplicative blending.");
						#endif

						glEnable (GL_BLEND);
						glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
					}
					
					if ( !(opengl_booleans & OPENGL_BOOLEAN_BLEND_EITHER) && (old_opengl_booleans & OPENGL_BOOLEAN_BLEND_SUBTRACT) )
					{
						// Turn off subtractive blending

						#ifdef RETRENGINE_DEBUG_VERSION_THE_LAST_THING_I_DID
							MAIN_debug_last_thing ("Turn off blending.");
						#endif

						glDisable (GL_BLEND);
					}
					else if ( (opengl_booleans & OPENGL_BOOLEAN_BLEND_SUBTRACT) && !(old_opengl_booleans & OPENGL_BOOLEAN_BLEND_SUBTRACT) )
					{
						// Turn on subtractive blending

						#ifdef RETRENGINE_DEBUG_VERSION_THE_LAST_THING_I_DID
							MAIN_debug_last_thing ("Turn on subtractive blending.");
						#endif

						glEnable (GL_BLEND);
						glBlendFunc(GL_ZERO, GL_ONE_MINUS_SRC_COLOR);
					}

					if ( !(opengl_booleans & OPENGL_BOOLEAN_MASKED) && (old_opengl_booleans & OPENGL_BOOLEAN_MASKED) )
					{
						// Turn off alpha test (masking)

						#ifdef RETRENGINE_DEBUG_VERSION_THE_LAST_THING_I_DID
							MAIN_debug_last_thing ("Turn off masking.");
						#endif

						glDisable (GL_ALPHA_TEST);
					}
					else if ( (opengl_booleans & OPENGL_BOOLEAN_MASKED) && !(old_opengl_booleans & OPENGL_BOOLEAN_MASKED) )
					{
						// Turn on alpha test (masking)

						#ifdef RETRENGINE_DEBUG_VERSION_THE_LAST_THING_I_DID
							MAIN_debug_last_thing ("Turn on masking.");
						#endif

						glEnable (GL_ALPHA_TEST);
						glAlphaFunc (GL_GREATER, 0.5f);
					}

					if (secondary_colour_available)
					{
						if ( !(opengl_booleans & OPENGL_BOOLEAN_VERTEX_SECONDARY_COLOUR) && (old_opengl_booleans & OPENGL_BOOLEAN_VERTEX_SECONDARY_COLOUR) )
						{
							// Reset secondary colour to black and turn it off.

							#ifdef RETRENGINE_DEBUG_VERSION_THE_LAST_THING_I_DID
								MAIN_debug_last_thing ("Turn off secondary colour.");
							#endif

							pglSecondaryColor3fEXT( 0.0f , 0.0f , 0.0f );
							glDisable(GL_COLOR_SUM);
						}
						else if ( (opengl_booleans & OPENGL_BOOLEAN_VERTEX_SECONDARY_COLOUR) && !(old_opengl_booleans & OPENGL_BOOLEAN_VERTEX_SECONDARY_COLOUR) )
						{
							// Reset secondary colour to black and turn it on.

							#ifdef RETRENGINE_DEBUG_VERSION_THE_LAST_THING_I_DID
								MAIN_debug_last_thing ("Turn on secondary colour.");
							#endif

							pglSecondaryColor3fEXT( 0.0f , 0.0f , 0.0f );
							glEnable(GL_COLOR_SUM);
						}
					}

					if ( (opengl_booleans & OPENGL_BOOLEAN_VERTEX_SECONDARY_COLOUR) && !(old_opengl_booleans & OPENGL_BOOLEAN_VERTEX_SECONDARY_COLOUR) )
					{
						// Turn on secondary colouring

						#ifdef RETRENGINE_DEBUG_VERSION_THE_LAST_THING_I_DID
							MAIN_debug_last_thing ("Turn on secondary colour.");
						#endif

						glEnable(GL_COLOR_SUM);
					}

					if ( !(opengl_booleans & OPENGL_BOOLEAN_VERTEX_COLOUR) && (old_opengl_booleans & OPENGL_BOOLEAN_VERTEX_COLOUR) )
					{
						// Reset vertex colour to white

						#ifdef RETRENGINE_DEBUG_VERSION_THE_LAST_THING_I_DID
							MAIN_debug_last_thing ("Reset vertex colour to white.");
						#endif

						glColor4f(1.0f,1.0f,1.0f,1.0f);
					}
					
					if ( !(opengl_booleans & OPENGL_BOOLEAN_VERTEX_COLOUR_ALPHA) && (old_opengl_booleans & OPENGL_BOOLEAN_VERTEX_COLOUR_ALPHA) )
					{
						// Reset vertex colour to white

						#ifdef RETRENGINE_DEBUG_VERSION_THE_LAST_THING_I_DID
							MAIN_debug_last_thing ("Reset vertex colour to white.");
						#endif

						glColor4f(1.0f,1.0f,1.0f,1.0f);
					}
				}

				if (secondary_colour_available)
				{
					if (opengl_booleans & OPENGL_BOOLEAN_VERTEX_SECONDARY_COLOUR)
					{
						#ifdef RETRENGINE_DEBUG_VERSION_THE_LAST_THING_I_DID
							MAIN_debug_last_thing ("Set secondary vertex colour.");
						#endif

						pglSecondaryColor3fEXT( float(entity_pointer[ENT_OPENGL_VERTEX_RED])/256.0f , float(entity_pointer[ENT_OPENGL_VERTEX_GREEN])/256.0f , float(entity_pointer[ENT_OPENGL_VERTEX_BLUE])/256.0f );
					}
				}

				if (opengl_booleans & OPENGL_BOOLEAN_VERTEX_COLOUR)
				{
					#ifdef RETRENGINE_DEBUG_VERSION_THE_LAST_THING_I_DID
						MAIN_debug_last_thing ("Set primary vertex colour.");
					#endif

					glColor4f( float(entity_pointer[ENT_OPENGL_VERTEX_RED])/256.0f , float(entity_pointer[ENT_OPENGL_VERTEX_GREEN])/256.0f , float(entity_pointer[ENT_OPENGL_VERTEX_BLUE])/256.0f , 1.0f);
				}

				if (opengl_booleans & OPENGL_BOOLEAN_VERTEX_COLOUR_ALPHA)
				{
					#ifdef RETRENGINE_DEBUG_VERSION_THE_LAST_THING_I_DID
						MAIN_debug_last_thing ("Set primary vertex colour and alpha.");
					#endif

					glColor4f( float(entity_pointer[ENT_OPENGL_VERTEX_RED])/256.0f , float(entity_pointer[ENT_OPENGL_VERTEX_GREEN])/256.0f , float(entity_pointer[ENT_OPENGL_VERTEX_BLUE])/256.0f , float(entity_pointer[ENT_OPENGL_VERTEX_ALPHA])/256.0f );
				}

				#ifdef RETRENGINE_DEBUG_VERSION_THE_LAST_THING_I_DID
					MAIN_debug_last_thing ("Entering main switch...");
				#endif

				#ifdef RETRENGINE_DEBUG_VERSION
					if ((entity_pointer[ENT_OPENGL_SCALE_X] == 0) || (entity_pointer[ENT_OPENGL_SCALE_Y] == 0))
					{
						sprintf (debug_text,"Tiny scale error in entity '%s'!",GPL_get_entry_name("SCRIPTS",entity_pointer[ENT_SCRIPT_NUMBER]));
						OUTPUT_message(debug_text);
						assert(0);
					}
				#endif

				switch (draw_type)
				{
					case DRAW_MODE_SPRITE: // NEW TYPE!!!				
					{
						if (opengl_booleans & OPENGL_BOOLEAN_INTERPOLATED)
						{
							frame_number = entity_pointer[ENT_CURRENT_FRAME];
							interp_frame_number = entity_pointer[ENT_INTERPOLATED_FRAME];
							
							sp = &bmps[bitmap_number].sprite_list[frame_number];
							interp_sp = &bmps[bitmap_number].sprite_list[interp_frame_number];

							interp_x = float (entity_pointer[ENT_INTERPOLATION_X_PERCENTAGE]) / 10000.0f;
							interp_y = float (entity_pointer[ENT_INTERPOLATION_Y_PERCENTAGE]) / 10000.0f;

							u1 = sp->u1;
							u2 = sp->u2;
							v1 = sp->v1;
							v2 = sp->v2;

							interp_u1 = interp_sp->u1;
							interp_v1 = interp_sp->v1;
							interp_u2 = interp_sp->u2;
							interp_v2 = interp_sp->v2;

							u1 = MATH_lerp (u1,interp_u1,interp_x);
							u2 = MATH_lerp (u2,interp_u2,interp_x);
							v1 = MATH_lerp (v1,interp_v1,interp_y);
							v2 = MATH_lerp (v2,interp_v2,interp_y);
						}
						else
						{
							frame_number = entity_pointer[ENT_CURRENT_FRAME];
								
							sp = &bmps[bitmap_number].sprite_list[frame_number];

							u1 = sp->u1;
							u2 = sp->u2;
							v1 = sp->v1;
							v2 = sp->v2;
						}

						x = entity_pointer[ENT_WORLD_X] - window_x;
						y = entity_pointer[ENT_WORLD_Y] - window_y;

						if (opengl_booleans & OPENGL_BOOLEAN_OVERRIDE_PIVOT)
						{
							left = -entity_pointer[ENT_OVERRIDE_PIVOT_X];
							right = sp->width - entity_pointer[ENT_OVERRIDE_PIVOT_X];
							up = entity_pointer[ENT_OVERRIDE_PIVOT_Y];
							down = - (sp->height - entity_pointer[ENT_OVERRIDE_PIVOT_Y]);
						}
						else
						{
							left = -sp->pivot_x;
							right = sp->width - sp->pivot_x;
							up = sp->pivot_y;
							down = - (sp->height - sp->pivot_y);
						}

						if (opengl_booleans & OPENGL_BOOLEAN_CLIP_FRAME)
						{
							u1 += sp->uv_pixel_size * float(entity_pointer[ENT_CUT_SPRITE_LEFT_INDENTATION]);
							u2 -= sp->uv_pixel_size * float(entity_pointer[ENT_CUT_SPRITE_RIGHT_INDENTATION]);
							v1 += sp->uv_pixel_size * float(entity_pointer[ENT_CUT_SPRITE_TOP_INDENTATION]);
							v2 -= sp->uv_pixel_size * float(entity_pointer[ENT_CUT_SPRITE_BOTTOM_INDENTATION]);

							left += entity_pointer[ENT_CUT_SPRITE_LEFT_INDENTATION];
							right -= entity_pointer[ENT_CUT_SPRITE_RIGHT_INDENTATION];
							up -= entity_pointer[ENT_CUT_SPRITE_TOP_INDENTATION];
							down += entity_pointer[ENT_CUT_SPRITE_BOTTOM_INDENTATION];
						}

						glTranslatef(x,-y,0.0);

						if (opengl_booleans & OPENGL_BOOLEAN_SECONDARY_SCALE)
						{
							scale_x_2 = float (entity_pointer[ENT_OPENGL_SECONDARY_SCALE_X]) / 10000.0f;
							scale_y_2 = float (entity_pointer[ENT_OPENGL_SECONDARY_SCALE_Y]) / 10000.0f;
							glScalef(scale_x_2,scale_y_2,1.0f);
						}

						if (opengl_booleans & OPENGL_BOOLEAN_ROTATE)
						{
							rotate_angle = float (entity_pointer[ENT_OPENGL_ANGLE]) / 100.0f;
							glRotatef (-rotate_angle , 0.0f, 0.0f, 1.0f);
						}

						if (opengl_booleans & OPENGL_BOOLEAN_SCALE)
						{
							scale_x = float (entity_pointer[ENT_OPENGL_SCALE_X]) / 10000.0f;
							scale_y = float (entity_pointer[ENT_OPENGL_SCALE_Y]) / 10000.0f;
							glScalef(scale_x,scale_y,1.0f);
						}

						if (opengl_booleans & OPENGL_BOOLEAN_SECONDARY_ROTATE)
						{
							rotate_angle_2 = float (entity_pointer[ENT_OPENGL_SECONDARY_ANGLE]) / 100.0f;
							glRotatef (-rotate_angle_2 , 0.0f, 0.0f, 1.0f);
						}

						if (opengl_booleans & OPENGL_BOOLEAN_HORI_FLIPPED)
						{
							temp_uv = u1;
							u1 = u2;
							u2 = temp_uv;
						}

						if (opengl_booleans & OPENGL_BOOLEAN_VERT_FLIPPED)
						{
							temp_uv = v1;
							v1 = v2;
							v2 = temp_uv;
						}

						if ((texture_combiner_available = true) && (opengl_booleans & (OPENGL_BOOLEAN_MULTITEXTURE_MASK | OPENGL_BOOLEAN_MULTITEXTURE_DOUBLE_MASK)))
						{
							if (secondary_opengl_booleans & OPENGL_BOOLEAN_INTERPOLATED)
							{
								secondary_frame_number = entity_pointer[ENT_SECONDARY_CURRENT_FRAME];
								secondary_interp_frame_number = entity_pointer[ENT_SECONDARY_INTERPOLATED_FRAME];
								
								secondary_sp = &bmps[secondary_bitmap_number].sprite_list[secondary_frame_number];
								secondary_interp_sp = &bmps[secondary_bitmap_number].sprite_list[secondary_interp_frame_number];

								interp_x = float (entity_pointer[ENT_SECONDARY_INTERPOLATION_X_PERCENTAGE]) / 10000.0f;
								interp_y = float (entity_pointer[ENT_SECONDARY_INTERPOLATION_Y_PERCENTAGE]) / 10000.0f;

								secondary_u1 = secondary_sp->u1;
								secondary_u2 = secondary_sp->u2;
								secondary_v1 = secondary_sp->v1;
								secondary_v2 = secondary_sp->v2;

								interp_u1 = secondary_interp_sp->u1;
								interp_v1 = secondary_interp_sp->v1;
								interp_u2 = secondary_interp_sp->u2;
								interp_v2 = secondary_interp_sp->v2;

								secondary_u1 = MATH_lerp (secondary_u1,interp_u1,interp_x);
								secondary_u2 = MATH_lerp (secondary_u2,interp_u2,interp_x);
								secondary_v1 = MATH_lerp (secondary_v1,interp_v1,interp_y);
								secondary_v2 = MATH_lerp (secondary_v2,interp_v2,interp_y);
							}
							else
							{
								secondary_frame_number = entity_pointer[ENT_SECONDARY_CURRENT_FRAME];
									
								secondary_sp = &bmps[bitmap_number].sprite_list[frame_number];

								secondary_u1 = secondary_sp->u1;
								secondary_u2 = secondary_sp->u2;
								secondary_v1 = secondary_sp->v1;
								secondary_v2 = secondary_sp->v2;
							}

							if (secondary_opengl_booleans & OPENGL_BOOLEAN_HORI_FLIPPED)
							{
								temp_uv = secondary_u1;
								secondary_u1 = secondary_u2;
								secondary_u2 = temp_uv;
							}

							if (secondary_opengl_booleans & OPENGL_BOOLEAN_VERT_FLIPPED)
							{
								temp_uv = secondary_v1;
								secondary_v1 = secondary_v2;
								secondary_v2 = temp_uv;
							}

							if (opengl_booleans & OPENGL_BOOLEAN_CLIP_FRAME)
							{
								secondary_u1 += secondary_sp->uv_pixel_size * float(entity_pointer[ENT_CUT_SPRITE_LEFT_INDENTATION]);
								secondary_u2 -= secondary_sp->uv_pixel_size * float(entity_pointer[ENT_CUT_SPRITE_RIGHT_INDENTATION]);
								secondary_v1 += secondary_sp->uv_pixel_size * float(entity_pointer[ENT_CUT_SPRITE_TOP_INDENTATION]);
								secondary_v2 -= secondary_sp->uv_pixel_size * float(entity_pointer[ENT_CUT_SPRITE_BOTTOM_INDENTATION]);
							}

							pglActiveTextureARB(GL_TEXTURE0);
							OUTPUT_set_texture_parameters (opengl_booleans, old_opengl_booleans);

							if (opengl_booleans & OPENGL_BOOLEAN_MULTITEXTURE_DOUBLE_MASK)
							{
								glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE);
								glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB, GL_MODULATE);
								glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_RGB, GL_TEXTURE);
								glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE1_RGB, GL_PRIMARY_COLOR);
								glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_ALPHA, GL_MODULATE);
								glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_ALPHA, GL_TEXTURE);
								glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE1_ALPHA, GL_PRIMARY_COLOR);

								pglActiveTextureARB(GL_TEXTURE1);

								secondary_opengl_booleans = entity_pointer[ENT_SECONDARY_OPENGL_BOOLEANS];
								OUTPUT_set_texture_parameters (secondary_opengl_booleans, old_secondary_opengl_booleans);
								old_secondary_opengl_booleans = secondary_opengl_booleans;

								glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE);
								glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB, GL_REPLACE);
								glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_RGB, GL_PREVIOUS);
								glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_ALPHA, GL_MODULATE);
								glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_ALPHA, GL_TEXTURE);
								glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE1_ALPHA, GL_PREVIOUS);
							}
							else if (opengl_booleans & OPENGL_BOOLEAN_MULTITEXTURE_MASK)
							{
								glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE);
								glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB, GL_REPLACE); // For the RGB data we replace it...
								glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_RGB, GL_PREVIOUS); // ...with the incoming colour data (as set by glColor4f() or whatever)
								glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_ALPHA, GL_MODULATE); // For the Alpha data we use a modulation combiner (ie, imagine two values of 0.0f to 1.0f multiplied together to get a result between 0.0f and 1.0f)
								glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_ALPHA, GL_TEXTURE); // The first alpha source is from the texture
								glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE1_ALPHA, GL_PRIMARY_COLOR); // And the second is whatever the current alpha is (as set by glColor4f() or whatever)

								pglActiveTextureARB(GL_TEXTURE1);

								secondary_opengl_booleans = entity_pointer[ENT_SECONDARY_OPENGL_BOOLEANS];
								OUTPUT_set_texture_parameters (secondary_opengl_booleans, old_secondary_opengl_booleans);
								old_secondary_opengl_booleans = secondary_opengl_booleans;

								glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE);
								glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB, GL_MODULATE); // This time the texture is combined via modulation with the two arguments being...
								glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_RGB, GL_TEXTURE); // The RGB data from the texture.
								glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE1_RGB, GL_PRIMARY_COLOR); // And the incoming colour (as set by glColor4f() or whatever).
								glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_ALPHA, GL_REPLACE); // The alpha information is overwritten, however...
								glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_ALPHA, GL_PREVIOUS); // ...with the data from the previous texture unit.
							}

							pglActiveTextureARB(GL_TEXTURE0);

							if ((opengl_booleans & OPENGL_BOOLEAN_ARBITRARY_QUAD) == 0)
							{
								if (opengl_booleans & OPENGL_BOOLEAN_ROTATE_CLOCKWISE)
								{
									glBegin(GL_QUADS);
										glMultiTexCoord2f(GL_TEXTURE0, u1, v1);
										glMultiTexCoord2f(GL_TEXTURE1, secondary_u1, secondary_v1);
										glVertex2f( right, up);

										glMultiTexCoord2f(GL_TEXTURE0, u1, v2);
										glMultiTexCoord2f(GL_TEXTURE1, secondary_u1, secondary_v2);
										glVertex2f( left, up);

										glMultiTexCoord2f(GL_TEXTURE0, u2, v2);
										glMultiTexCoord2f(GL_TEXTURE1, secondary_u2, secondary_v2);
										glVertex2f( left, down);

										glMultiTexCoord2f(GL_TEXTURE0, u2, v1);
										glMultiTexCoord2f(GL_TEXTURE1, secondary_u2, secondary_v1);
										glVertex2f( right, down);
									glEnd();
								}
								else
								{
									glBegin(GL_QUADS);
										glMultiTexCoord2f(GL_TEXTURE0, u1, v1);
										glMultiTexCoord2f(GL_TEXTURE1, secondary_u1, secondary_v1);
										glVertex2f( left, up);

										glMultiTexCoord2f(GL_TEXTURE0, u1, v2);
										glMultiTexCoord2f(GL_TEXTURE1, secondary_u1, secondary_v2);
										glVertex2f( left, down);

										glMultiTexCoord2f(GL_TEXTURE0, u2, v2);
										glMultiTexCoord2f(GL_TEXTURE1, secondary_u2, secondary_v2);
										glVertex2f( right, down);

										glMultiTexCoord2f(GL_TEXTURE0, u2, v1);
										glMultiTexCoord2f(GL_TEXTURE1, secondary_u2, secondary_v1);
										glVertex2f( right, up);
									glEnd();
								}
							}
							else if (opengl_booleans & OPENGL_BOOLEAN_ARBITRARY_QUAD)
							{
								aq_pointer = &arbitrary_quads[current_entity];

								if (opengl_booleans & OPENGL_BOOLEAN_ROTATE_CLOCKWISE)
								{
									glBegin(GL_QUADS);
										glMultiTexCoord2f(GL_TEXTURE0, u1, v1);
										glMultiTexCoord2f(GL_TEXTURE1, secondary_u1, secondary_v1);
										glVertex2f( aq_pointer->x[1], aq_pointer->y[1]);

										glMultiTexCoord2f(GL_TEXTURE0, u1, v2);
										glMultiTexCoord2f(GL_TEXTURE1, secondary_u1, secondary_v2);
										glVertex2f( aq_pointer->x[0], aq_pointer->y[0]);

										glMultiTexCoord2f(GL_TEXTURE0, u2, v2);
										glMultiTexCoord2f(GL_TEXTURE1, secondary_u2, secondary_v2);
										glVertex2f( aq_pointer->x[2], aq_pointer->y[2]);
										
										glMultiTexCoord2f(GL_TEXTURE0, u2, v1);
										glMultiTexCoord2f(GL_TEXTURE1, secondary_u2, secondary_v1);
										glVertex2f( aq_pointer->x[3], aq_pointer->y[3]);
									glEnd();
								}
								else
								{
									glBegin(GL_QUADS);
										glMultiTexCoord2f(GL_TEXTURE0, u1, v1);
										glMultiTexCoord2f(GL_TEXTURE1, secondary_u1, secondary_v1);
										glVertex2f( aq_pointer->x[0], aq_pointer->y[0]);

										glMultiTexCoord2f(GL_TEXTURE0, u1, v1);
										glMultiTexCoord2f(GL_TEXTURE1, secondary_u1, secondary_v1);
										glVertex2f( aq_pointer->x[2], aq_pointer->y[2]);

										glMultiTexCoord2f(GL_TEXTURE0, u2, v2);
										glMultiTexCoord2f(GL_TEXTURE1, secondary_u2, secondary_v2);
										glVertex2f( aq_pointer->x[3], aq_pointer->y[3]);
										
										glMultiTexCoord2f(GL_TEXTURE0, u2, v1);
										glMultiTexCoord2f(GL_TEXTURE1, secondary_u2, secondary_v1);
										glVertex2f( aq_pointer->x[1], aq_pointer->y[1]);
									glEnd();
								}
							}
							else if (opengl_booleans & OPENGL_BOOLEAN_ARBITRARY_PERSPECTIVE_QUAD)
							{


							}
						}
						else
						{
							pglActiveTextureARB(GL_TEXTURE0);
							OUTPUT_set_texture_parameters (opengl_booleans, old_opengl_booleans);

							if (opengl_booleans & OPENGL_BOOLEAN_INDIVIDUAL_VERTEX_COLOUR_ALPHA)
							{
								avc_pointer = &arbitrary_vertex_colours[current_entity];

								if ((opengl_booleans & (OPENGL_BOOLEAN_ARBITRARY_QUAD|OPENGL_BOOLEAN_ARBITRARY_PERSPECTIVE_QUAD)) == 0)
								{
									if (opengl_booleans & OPENGL_BOOLEAN_ROTATE_CLOCKWISE)
									{
										glBegin(GL_QUADS);
											glTexCoord2f(u1, v1);
											glColor4f(avc_pointer->red[1],avc_pointer->green[1],avc_pointer->blue[1],avc_pointer->alpha[1]);
											glVertex2f( right, up);
											glTexCoord2f(u1, v2);
											glColor4f(avc_pointer->red[0],avc_pointer->green[0],avc_pointer->blue[0],avc_pointer->alpha[0]);
											glVertex2f( left, up);
											glTexCoord2f(u2, v2);
											glColor4f(avc_pointer->red[2],avc_pointer->green[2],avc_pointer->blue[2],avc_pointer->alpha[2]);
											glVertex2f( left, down);
											glTexCoord2f(u2, v1);
											glColor4f(avc_pointer->red[3],avc_pointer->green[3],avc_pointer->blue[3],avc_pointer->alpha[3]);
											glVertex2f( right, down);
										glEnd();
									}
									else
									{
										glBegin(GL_QUADS);
											glTexCoord2f(u1, v1);
											glColor4f(avc_pointer->red[0],avc_pointer->green[0],avc_pointer->blue[0],avc_pointer->alpha[0]);
											glVertex2f( left, up);
											glTexCoord2f(u1, v2);
											glColor4f(avc_pointer->red[2],avc_pointer->green[2],avc_pointer->blue[2],avc_pointer->alpha[2]);
											glVertex2f( left, down);
											glTexCoord2f(u2, v2);
											glColor4f(avc_pointer->red[3],avc_pointer->green[3],avc_pointer->blue[3],avc_pointer->alpha[3]);
											glVertex2f( right, down);
											glTexCoord2f(u2, v1);
											glColor4f(avc_pointer->red[1],avc_pointer->green[1],avc_pointer->blue[1],avc_pointer->alpha[1]);
											glVertex2f( right, up);
										glEnd();
									}
								}
								else if (opengl_booleans & OPENGL_BOOLEAN_ARBITRARY_QUAD)
								{
									aq_pointer = &arbitrary_quads[current_entity];

									if (opengl_booleans & OPENGL_BOOLEAN_ROTATE_CLOCKWISE)
									{
										glBegin(GL_QUADS);
											glTexCoord2f(u1, v1);
											glColor4f(avc_pointer->red[1],avc_pointer->green[1],avc_pointer->blue[1],avc_pointer->alpha[1]);
											glVertex2f( aq_pointer->x[1], aq_pointer->y[1]);
											glTexCoord2f(u1, v2);
											glColor4f(avc_pointer->red[0],avc_pointer->green[0],avc_pointer->blue[0],avc_pointer->alpha[0]);
											glVertex2f( aq_pointer->x[0], aq_pointer->y[0]);
											glTexCoord2f(u2, v2);
											glColor4f(avc_pointer->red[2],avc_pointer->green[2],avc_pointer->blue[2],avc_pointer->alpha[2]);
											glVertex2f( aq_pointer->x[2], aq_pointer->y[2]);
											glTexCoord2f(u2, v1);
											glColor4f(avc_pointer->red[3],avc_pointer->green[3],avc_pointer->blue[3],avc_pointer->alpha[3]);
											glVertex2f( aq_pointer->x[3], aq_pointer->y[3]);
										glEnd();
									}
									else
									{
										glBegin(GL_QUADS);
											glTexCoord2f(u1, v1);
											glColor4f(avc_pointer->red[0],avc_pointer->green[0],avc_pointer->blue[0],avc_pointer->alpha[0]);
											glVertex2f( aq_pointer->x[0], aq_pointer->y[0]);
											glTexCoord2f(u1, v2);
											glColor4f(avc_pointer->red[2],avc_pointer->green[2],avc_pointer->blue[2],avc_pointer->alpha[2]);
											glVertex2f( aq_pointer->x[2], aq_pointer->y[2]);
											glTexCoord2f(u2, v2);
											glColor4f(avc_pointer->red[3],avc_pointer->green[3],avc_pointer->blue[3],avc_pointer->alpha[3]);
											glVertex2f( aq_pointer->x[3], aq_pointer->y[3]);
											glTexCoord2f(u2, v1);
											glColor4f(avc_pointer->red[1],avc_pointer->green[1],avc_pointer->blue[1],avc_pointer->alpha[1]);
											glVertex2f( aq_pointer->x[1], aq_pointer->y[1]);
										glEnd();
									}
								}
								else if (opengl_booleans & OPENGL_BOOLEAN_ARBITRARY_PERSPECTIVE_QUAD)
								{
									aq_pointer = &arbitrary_quads[current_entity];

									line_length_ratio = aq_pointer->line_lengths[0] / aq_pointer->line_lengths[2];

									glBegin(GL_QUADS);
										glColor4f(avc_pointer->red[0],avc_pointer->green[0],avc_pointer->blue[0],avc_pointer->alpha[0]);
										glTexCoord4f(u1*line_length_ratio, v1*line_length_ratio, 0, line_length_ratio);
										glVertex2f( aq_pointer->x[0], aq_pointer->y[0]);
										
										glColor4f(avc_pointer->red[2],avc_pointer->green[2],avc_pointer->blue[2],avc_pointer->alpha[2]);
										glTexCoord2f(u1, v2);
										glVertex2f( aq_pointer->x[2], aq_pointer->y[2]);

										glColor4f(avc_pointer->red[3],avc_pointer->green[3],avc_pointer->blue[3],avc_pointer->alpha[3]);
										glTexCoord2f(u2, v2);
										glVertex2f( aq_pointer->x[3], aq_pointer->y[3]);

										glColor4f(avc_pointer->red[1],avc_pointer->green[1],avc_pointer->blue[1],avc_pointer->alpha[1]);
										glTexCoord4f(u2*line_length_ratio, v1*line_length_ratio, 0, line_length_ratio);
										glVertex2f( aq_pointer->x[1], aq_pointer->y[1]);
									glEnd();
								}
							}
							else
							{
								if ((opengl_booleans & (OPENGL_BOOLEAN_ARBITRARY_QUAD|OPENGL_BOOLEAN_ARBITRARY_PERSPECTIVE_QUAD)) == 0)
								{
									if (opengl_booleans & OPENGL_BOOLEAN_ROTATE_CLOCKWISE)
									{
										glBegin(GL_QUADS);
											glTexCoord2f(u1, v1);
											glVertex2f( right, up);
											glTexCoord2f(u1, v2);
											glVertex2f( left, up);
											glTexCoord2f(u2, v2);
											glVertex2f( left, down);
											glTexCoord2f(u2, v1);
											glVertex2f( right, down);
										glEnd();
									}
									else
									{
										glBegin(GL_QUADS);
											glTexCoord2f(u1, v1);
											glVertex2f( left, up);
											glTexCoord2f(u1, v2);
											glVertex2f( left, down);
											glTexCoord2f(u2, v2);
											glVertex2f( right, down);
											glTexCoord2f(u2, v1);
											glVertex2f( right, up);
										glEnd();
									}
								}
								else if (opengl_booleans & OPENGL_BOOLEAN_ARBITRARY_QUAD)
								{
									aq_pointer = &arbitrary_quads[current_entity];

									if (opengl_booleans & OPENGL_BOOLEAN_ROTATE_CLOCKWISE)
									{
										glBegin(GL_QUADS);
											glTexCoord2f(u1, v1);
											glVertex2f( aq_pointer->x[1], aq_pointer->y[1]);
											glTexCoord2f(u1, v2);
											glVertex2f( aq_pointer->x[0], aq_pointer->y[0]);
											glTexCoord2f(u2, v2);
											glVertex2f( aq_pointer->x[2], aq_pointer->y[2]);
											glTexCoord2f(u2, v1);
											glVertex2f( aq_pointer->x[3], aq_pointer->y[3]);
										glEnd();
									}
									else
									{
										glBegin(GL_QUADS);
											glTexCoord2f(u1, v1);
											glVertex2f( aq_pointer->x[0], aq_pointer->y[0]);
											glTexCoord2f(u1, v2);
											glVertex2f( aq_pointer->x[2], aq_pointer->y[2]);
											glTexCoord2f(u2, v2);
											glVertex2f( aq_pointer->x[3], aq_pointer->y[3]);
											glTexCoord2f(u2, v1);
											glVertex2f( aq_pointer->x[1], aq_pointer->y[1]);
										glEnd();
									}
								}
								else if (opengl_booleans & OPENGL_BOOLEAN_ARBITRARY_PERSPECTIVE_QUAD)
								{
									aq_pointer = &arbitrary_quads[current_entity];

									line_length_ratio = aq_pointer->line_lengths[0] / aq_pointer->line_lengths[2];

									glBegin(GL_QUADS);
										glTexCoord4f(u1*line_length_ratio, v1*line_length_ratio, 0, line_length_ratio);
										glVertex2f( aq_pointer->x[0], aq_pointer->y[0]);
										glTexCoord2f(u1, v2);
										glVertex2f( aq_pointer->x[2], aq_pointer->y[2]);
										glTexCoord2f(u2, v2);
										glVertex2f( aq_pointer->x[3], aq_pointer->y[3]);
										glTexCoord4f(u2*line_length_ratio, v1*line_length_ratio, 0, line_length_ratio);
										glVertex2f( aq_pointer->x[1], aq_pointer->y[1]);
									glEnd();
								}
							}
						}

						if (opengl_booleans & OPENGL_BOOLEAN_SECONDARY_ROTATE)
						{
							glRotatef (rotate_angle_2 , 0.0f, 0.0f, 1.0f);	
						}

						if (opengl_booleans & OPENGL_BOOLEAN_SCALE)
						{
							glScalef(1.0f/scale_x,1.0f/scale_y,1.0f);
						}

						if (opengl_booleans & OPENGL_BOOLEAN_ROTATE)
						{
							glRotatef (rotate_angle , 0.0f, 0.0f, 1.0f);	
						}

						if (opengl_booleans & OPENGL_BOOLEAN_SECONDARY_SCALE)
						{
							glScalef(1.0f/scale_x_2,1.0f/scale_y_2,1.0f);
						}

						glTranslatef(-x,y,0);
					}
					break;
					
					
				case DRAW_MODE_INVISIBLE:
					// Something was killed and recycled during the object to object collision phase and so
					// is now blank. May as well let it sliiiiiide.
					assert(0);
					break;


				case DRAW_MODE_TILEMAP:
					#ifdef RETRENGINE_DEBUG_VERSION_THE_LAST_THING_I_DID
						MAIN_debug_last_thing ("DRAW_MODE_TILEMAP...");
					#endif

					OUTPUT_set_texture_parameters (opengl_booleans, old_opengl_booleans);

					map_number = entity_pointer[ENT_TILEMAP_NUMBER];
					layer_number = entity_pointer[ENT_TILEMAP_LAYER];
					map_pointer = cm[map_number].map_pointer;
					map_width = cm[map_number].map_width;
					map_height = cm[map_number].map_height;
					map_pointer += layer_number * map_width * map_height;

					map_optimised = cm[map_number].optimisation_data_flag;

					if (map_optimised)
					{
						opt_pointer = cm[map_number].optimisation_data;
						opt_pointer += layer_number * map_width * map_height;
					}

					// Map pointer now points to the start of the layer of the map we're drawing.

					tileset = cm[map_number].tile_set_index;
					tilesize = ts[tileset].tilesize;
					tile_graphic = ts[tileset].tileset_image_index;

					start_bx = entity_pointer[ENT_TILEMAP_POSITION_X];
					start_by = entity_pointer[ENT_TILEMAP_POSITION_Y];

					x_under = MATH_wrap (start_bx , tilesize);
					y_under = MATH_wrap (start_by , tilesize);

					// Drawing offset of tiles within the window...

					// Bah! All this fuss below because of negative co-ordinates.

					if (start_bx >= 0)
					{
						start_bx /= tilesize;
					}
					else if (x_under != 0)
					{
						start_bx /= tilesize;
						start_bx -= 1;
					}
					else
					{
						start_bx /= tilesize;
					}

					if (start_by >= 0)
					{
						start_by /= tilesize;
					}
					else if (y_under != 0)
					{
						start_by /= tilesize;
						start_by -= 1;
					}
					else
					{
						start_by /= tilesize;
					}

					end_bx = start_bx + (window_width / tilesize) + 1;
					end_by = start_by + (window_height / tilesize) + 1;

					if (start_bx < 0)
					{
						x_under += (start_bx * tilesize);
						start_bx = 0;
					}

					if (start_by < 0)
					{
						y_under += (start_by * tilesize);
						start_by = 0;
					}

					if (end_bx > map_width)
					{
						end_bx = map_width;
					}

					if (end_by > map_height)
					{
						end_by = map_height;
					}

					tileset_drawing_start_offset_x = -x_under;
					tileset_drawing_start_offset_y = y_under;

					next_line_transform_x = (end_bx - start_bx) * -tilesize;
					next_line_transform_y = -tilesize;

					glLoadIdentity();
					glTranslatef(left_window_transform_x,top_window_transform_y,0);
					glScalef (total_scale_x,total_scale_y,0);

					glTranslatef(tileset_drawing_start_offset_x,tileset_drawing_start_offset_y,0);
					
					// Get tile pivot data from the first tile.
					sp = &bmps[tile_graphic].sprite_list[0];

					left = -sp->pivot_x;
					right = sp->width - sp->pivot_x;
					up = sp->pivot_y;
					down = - (sp->height - sp->pivot_y);
						
					if (map_optimised)
					{
						for (by=start_by; by<end_by; by++)
						{
							map_line_pointer = &map_pointer[(map_width * by) + start_bx];

							for (bx=start_bx; bx<end_bx; bx++)
							{
								if (*map_line_pointer != 0)
								{
									sp = &bmps[tile_graphic].sprite_list[*map_line_pointer];

									u1 = sp->u1;
									u2 = sp->u2;
									v1 = sp->v1;
									v2 = sp->v2;

									glBegin(GL_QUADS);
										glTexCoord2f(u1, v1);
										glVertex2f( left, up);
										glTexCoord2f(u1, v2);
										glVertex2f( left, down);
										glTexCoord2f(u2, v2);
										glVertex2f( right, down);
										glTexCoord2f(u2, v1);
										glVertex2f( right, up);
									glEnd();
								}
								else
								{
									opt_offset = opt_pointer + (map_line_pointer - map_pointer);
									map_line_pointer += *opt_offset;
									bx += *opt_offset;
									glTranslatef(tilesize * *opt_offset,0,0);
								}

								glTranslatef(tilesize,0,0);
								
								map_line_pointer++;
							}
							
							glTranslatef(next_line_transform_x - (tilesize * (bx - end_bx)),next_line_transform_y,0);
						}
					}
					else
					{
						for (by=start_by; by<end_by; by++)
						{
							map_line_pointer = &map_pointer[(map_width * by) + start_bx];

							for (bx=start_bx; bx<end_bx; bx++)
							{
								if (*map_line_pointer != 0)
								{
									sp = &bmps[tile_graphic].sprite_list[*map_line_pointer];

									u1 = sp->u1;
									u2 = sp->u2;
									v1 = sp->v1;
									v2 = sp->v2;

									glBegin(GL_QUADS);
										glTexCoord2f(u1, v1);
										glVertex2f( left, up);
										glTexCoord2f(u1, v2);
										glVertex2f( left, down);
										glTexCoord2f(u2, v2);
										glVertex2f( right, down);
										glTexCoord2f(u2, v1);
										glVertex2f( right, up);
									glEnd();
								}

								glTranslatef(tilesize,0,0);
								
								map_line_pointer++;
							}

							glTranslatef(next_line_transform_x,next_line_transform_y,0);
						}
					}

					glLoadIdentity();
					glTranslatef(left_window_transform_x,top_window_transform_y,0);
					glScalef (total_scale_x,total_scale_y,0);
					break;

				case DRAW_MODE_TILEMAP_LINE:
					#ifdef RETRENGINE_DEBUG_VERSION_THE_LAST_THING_I_DID
						MAIN_debug_last_thing ("DRAW_MODE_TILEMAP_LINE...");
					#endif
						
					// This is a single tiny line of tiles. The reason for this is for games where we
					// want to interleve the drawing of the tiles with the drawing of the sprites. A typical
					// example of this would be Android 2, where the walls obscure the player and other sprites.

					// Unlike tilemaps you can't manually specify where the tiles should be drawn from the map
					// but rather it is inferred from the world_y of the entity and the window_x of the entity.

					OUTPUT_set_texture_parameters (opengl_booleans, old_opengl_booleans);

					map_number = entity_pointer[ENT_TILEMAP_NUMBER];
					layer_number = entity_pointer[ENT_TILEMAP_LAYER];
					map_pointer = cm[map_number].map_pointer;
					map_width = cm[map_number].map_width;
					map_height = cm[map_number].map_height;
					map_pointer += layer_number * map_width * map_height;

					map_optimised = cm[map_number].optimisation_data_flag;

					if (map_optimised)
					{
						opt_pointer = cm[map_number].optimisation_data;
						opt_pointer += layer_number * map_width * map_height;
					}

					// Map pointer now points to the start of the layer of the map we're drawing.

					tileset = cm[map_number].tile_set_index;
					tilesize = ts[tileset].tilesize;
					tile_graphic = ts[tileset].tileset_image_index;

					start_bx = window_details[window_number].current_x;
					start_by = entity_pointer[ENT_WORLD_Y];

					x_under = MATH_wrap (start_bx , tilesize);
					y_under = -(start_by - window_details[window_number].current_y);

					// Drawing offset of tiles within the window...

					// Bah! All this fuss below because of negative co-ordinates.

					if (start_bx >= 0)
					{
						start_bx /= tilesize;
					}
					else if (x_under != 0)
					{
						start_bx /= tilesize;
						start_bx -= 1;
					}
					else
					{
						start_bx /= tilesize;
					}

					if (start_by >= 0)
					{
						start_by /= tilesize;
					}
					else if (y_under != 0)
					{
						start_by /= tilesize;
						start_by -= 1;
					}
					else
					{
						start_by /= tilesize;
					}

					end_bx = start_bx + (window_width / tilesize) + 1;

					if (start_bx < 0)
					{
						x_under += (start_bx * tilesize);
						start_bx = 0;
					}

					if (start_by < 0)
					{
						y_under += (start_by * tilesize);
						start_by = 0;
					}

					if (start_by > map_height)
					{
						start_by = map_height;
					}

					if (end_bx > map_width)
					{
						end_bx = map_width;
					}

					tileset_drawing_start_offset_x = -x_under;
					tileset_drawing_start_offset_y = y_under;

					glLoadIdentity();
					glTranslatef(left_window_transform_x,top_window_transform_y,0);
					glScalef (total_scale_x,total_scale_y,0);

					glTranslatef(tileset_drawing_start_offset_x,tileset_drawing_start_offset_y,0);
					
					// Get tile pivot data from the first tile.
					sp = &bmps[tile_graphic].sprite_list[0];

					left = -sp->pivot_x;
					right = sp->width - sp->pivot_x;
					up = sp->pivot_y;
					down = - (sp->height - sp->pivot_y);
						
					if (map_optimised)
					{
						map_line_pointer = &map_pointer[(map_width * start_by) + start_bx];

						for (bx=start_bx; bx<end_bx; bx++)
						{
							if (*map_line_pointer != 0)
							{
								sp = &bmps[tile_graphic].sprite_list[*map_line_pointer];

								u1 = sp->u1;
								u2 = sp->u2;
								v1 = sp->v1;
								v2 = sp->v2;

								glBegin(GL_QUADS);
									glTexCoord2f(u1, v1);
									glVertex2f( left, up);
									glTexCoord2f(u1, v2);
									glVertex2f( left, down);
									glTexCoord2f(u2, v2);
									glVertex2f( right, down);
									glTexCoord2f(u2, v1);
									glVertex2f( right, up);
								glEnd();
							}
							else
							{
								opt_offset = opt_pointer + (map_line_pointer - map_pointer);
								map_line_pointer += *opt_offset;
								bx += *opt_offset;
								glTranslatef(tilesize * *opt_offset,0,0);
							}

							glTranslatef(tilesize,0,0);
							
							map_line_pointer++;
						}
					}
					else
					{
						map_line_pointer = &map_pointer[(map_width * start_by) + start_bx];

						for (bx=start_bx; bx<end_bx; bx++)
						{
							if (*map_line_pointer != 0)
							{
								sp = &bmps[tile_graphic].sprite_list[*map_line_pointer];

								u1 = sp->u1;
								u2 = sp->u2;
								v1 = sp->v1;
								v2 = sp->v2;

								glBegin(GL_QUADS);
									glTexCoord2f(u1, v1);
									glVertex2f( left, up);
									glTexCoord2f(u1, v2);
									glVertex2f( left, down);
									glTexCoord2f(u2, v2);
									glVertex2f( right, down);
									glTexCoord2f(u2, v1);
									glVertex2f( right, up);
								glEnd();
							}

							glTranslatef(tilesize,0,0);
							
							map_line_pointer++;
						}
					}

					glLoadIdentity();
					glTranslatef(left_window_transform_x,top_window_transform_y,0);
					glScalef (total_scale_x,total_scale_y,0);
					break;






				case DRAW_MODE_STARFIELD:
					#ifdef RETRENGINE_DEBUG_VERSION_THE_LAST_THING_I_DID
						MAIN_debug_last_thing ("DRAW_MODE_STARFIELD...");
					#endif

					glLoadIdentity();
					glTranslatef(left_window_transform_x,top_window_transform_y,0);
					glScalef (total_scale_x,total_scale_y,0);

					if (entity_pointer[ENT_DRAW_OVERRIDE] == DRAW_OVERRIDE_NONE)
					{
						x = entity_pointer[ENT_WORLD_X] - window_x;
						y = entity_pointer[ENT_WORLD_Y] - window_y;
						glTranslatef(x,-y,0.0);
					}

					glDisable(GL_TEXTURE_2D);

					OUTPUT_draw_starfield (entity_pointer[ENT_UNIQUE_ID]);

					glEnable(GL_TEXTURE_2D);
					glLoadIdentity();
					glTranslatef(left_window_transform_x,top_window_transform_y,0);
					glScalef (total_scale_x,total_scale_y,0);
					break;

				case DRAW_MODE_STARFIELD_LINES:
					#ifdef RETRENGINE_DEBUG_VERSION_THE_LAST_THING_I_DID
						MAIN_debug_last_thing ("DRAW_MODE_STARFIELD_LINES...");
					#endif

					glLoadIdentity();
					glTranslatef(left_window_transform_x,top_window_transform_y,0);
					glScalef (total_scale_x,total_scale_y,0);

					if (entity_pointer[ENT_DRAW_OVERRIDE] == DRAW_OVERRIDE_NONE)
					{
						x = entity_pointer[ENT_WORLD_X] - window_x;
						y = entity_pointer[ENT_WORLD_Y] - window_y;
						glTranslatef(x,-y,0.0);
					}
					
					glDisable(GL_TEXTURE_2D);

					OUTPUT_draw_starfield_lines (entity_pointer[ENT_UNIQUE_ID]);

					glEnable(GL_TEXTURE_2D);
					glLoadIdentity();
					glTranslatef(left_window_transform_x,top_window_transform_y,0);
					glScalef (total_scale_x,total_scale_y,0);
					break;

				case DRAW_MODE_STARFIELD_COLOUR:
					#ifdef RETRENGINE_DEBUG_VERSION_THE_LAST_THING_I_DID
						MAIN_debug_last_thing ("DRAW_MODE_STARFIELD_COLOUR...");
					#endif

					glLoadIdentity();
					glTranslatef(left_window_transform_x,top_window_transform_y,0);
					glScalef (total_scale_x,total_scale_y,0);

					if (entity_pointer[ENT_DRAW_OVERRIDE] == DRAW_OVERRIDE_NONE)
					{
						x = entity_pointer[ENT_WORLD_X] - window_x;
						y = entity_pointer[ENT_WORLD_Y] - window_y;
						glTranslatef(x,-y,0.0);
					}
					
					glDisable(GL_TEXTURE_2D);

					OUTPUT_draw_starfield_colour (entity_pointer[ENT_UNIQUE_ID]);

					glEnable(GL_TEXTURE_2D);
					glLoadIdentity();
					glTranslatef(left_window_transform_x,top_window_transform_y,0);
					glScalef (total_scale_x,total_scale_y,0);
					break;

				case DRAW_MODE_STARFIELD_COLOUR_LINES:
					#ifdef RETRENGINE_DEBUG_VERSION_THE_LAST_THING_I_DID
						MAIN_debug_last_thing ("DRAW_MODE_STARFIELD_COLOUR_LINES...");
					#endif

					glLoadIdentity();
					glTranslatef(left_window_transform_x,top_window_transform_y,0);
					glScalef (total_scale_x,total_scale_y,0);

					if (entity_pointer[ENT_DRAW_OVERRIDE] == DRAW_OVERRIDE_NONE)
					{
						x = entity_pointer[ENT_WORLD_X] - window_x;
						y = entity_pointer[ENT_WORLD_Y] - window_y;
						glTranslatef(x,-y,0.0);
					}
					
					glDisable(GL_TEXTURE_2D);

					OUTPUT_draw_starfield_colour_lines (entity_pointer[ENT_UNIQUE_ID]);

					glEnable(GL_TEXTURE_2D);
					glLoadIdentity();
					glTranslatef(left_window_transform_x,top_window_transform_y,0);
					glScalef (total_scale_x,total_scale_y,0);
					break;

				case DRAW_MODE_SOLID_RECTANGLE:
					#ifdef RETRENGINE_DEBUG_VERSION_THE_LAST_THING_I_DID
						MAIN_debug_last_thing ("DRAW_MODE_SOLID_RECTANGLE...");
					#endif

					glDisable(GL_TEXTURE_2D);

					OUTPUT_set_texture_parameters (opengl_booleans, old_opengl_booleans);

					x = entity_pointer[ENT_WORLD_X] - window_x;
					y = entity_pointer[ENT_WORLD_Y] - window_y;

					left = -entity_pointer[ENT_UPPER_WIDTH];
					right = entity_pointer[ENT_LOWER_WIDTH];
					up = entity_pointer[ENT_UPPER_HEIGHT];
					down = -entity_pointer[ENT_LOWER_HEIGHT];


					glTranslatef(x,-y,0.0);

					if (opengl_booleans & OPENGL_BOOLEAN_ROTATE)
					{
						rotate_angle = float (entity_pointer[ENT_OPENGL_ANGLE]) / 100.0f;
						glRotatef (-rotate_angle , 0.0f, 0.0f, 1.0f);
					}

					if (opengl_booleans & OPENGL_BOOLEAN_SCALE)
					{
						scale_x = float (entity_pointer[ENT_OPENGL_SCALE_X]) / 10000.0f;
						scale_y = float (entity_pointer[ENT_OPENGL_SCALE_Y]) / 10000.0f;
						glScalef(scale_x,scale_y,1.0f);
					}

					glBegin(GL_QUADS);
						glVertex2f( left, up);
						glVertex2f( left, down);
						glVertex2f( right, down);
						glVertex2f( right, up);
					glEnd();

					if (opengl_booleans & OPENGL_BOOLEAN_SCALE)
					{
						glScalef(1.0f/scale_x,1.0f/scale_y,1.0f);
					}

					if (opengl_booleans & OPENGL_BOOLEAN_ROTATE)
					{
						glRotatef (rotate_angle , 0.0f, 0.0f, 1.0f);	
					}

					glTranslatef(-x,y,0);

					glEnable(GL_TEXTURE_2D);
					break;

				default:
					assert(0);
					// Arse-candles!
					break;
				}

				#ifdef RETRENGINE_DEBUG_VERSION_THE_LAST_THING_I_DID
					MAIN_debug_last_thing ("Finished Switch...");
				#endif

				current_entity = entity[current_entity][ENT_NEXT_WINDOW_ENT];

				old_opengl_booleans = opengl_booleans;
			}

			#ifndef RETRENGINE_DEBUG_VERSION_VIEW_WORLD_COLLISION
				#ifndef RETRENGINE_DEBUG_VERSION_VIEW_OBJECT_COLLISION
					window_details[window_number].z_ordering_list_starts[z_ordering_list] = UNSET;
					window_details[window_number].z_ordering_list_ends[z_ordering_list] = UNSET;
				#endif
			#endif

		}

	}

	if ((secondary_bitmap_number != UNSET) && (texture_combiner_available = true))
	{
		// If we had a multi-texture last frame then we need to first disable the second texture unit...
		pglActiveTextureARB(GL_TEXTURE1);
		glDisable(GL_TEXTURE_2D);
		// Then restore the default modulation to the texture combiner.
		pglActiveTextureARB(GL_TEXTURE0);
		glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE);
		glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB, GL_MODULATE);
		glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_RGB, GL_TEXTURE);
		glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE1_RGB, GL_PRIMARY_COLOR);
		glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_ALPHA, GL_MODULATE);
		glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_ALPHA, GL_TEXTURE);
		glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE1_ALPHA, GL_PRIMARY_COLOR);
	}

	#ifdef RETRENGINE_DEBUG_VERSION_THE_LAST_THING_I_DID
		MAIN_debug_last_thing ("Elvis has left the building...");
	#endif

	if (secondary_colour_available)
	{
		// Reset secondary colour to black and turn it off.
		pglSecondaryColor3fEXT( 0.0f , 0.0f , 0.0f );
		glDisable(GL_COLOR_SUM);
	}

	glScissor ( 0 , 0 , game_screen_width , game_screen_height );

	glDisable (GL_BLEND);
	glDisable (GL_ALPHA_TEST);
	glColor4f (1.0f, 1.0f, 1.0f, 1.0f);

	if (window_details[window_number].secondary_window_colour == true)
	{
		glDisable(GL_COLOR_SUM);
	}

	return total_entities_drawn;
}



void OUTPUT_store_frame_info_in_entity_collision (int *entity_pointer, int modifier)
{
	int sprite = entity_pointer[ENT_SPRITE];
	int frame = entity_pointer[ENT_BASE_FRAME];

	if ( (sprite < 0) || (frame < 0) )
	{
		OUTPUT_message("Trying to set collision with unset sprite or frame!");
	}

	int width = bmps[sprite].sprite_list[frame].width;
	int height = bmps[sprite].sprite_list[frame].height;

	int pivot_x = bmps[sprite].sprite_list[frame].pivot_x;
	int pivot_y = bmps[sprite].sprite_list[frame].pivot_y;

	entity_pointer [ENT_UPPER_WIDTH] = pivot_x + modifier;
	entity_pointer [ENT_UPPER_HEIGHT] = pivot_y + modifier;

	entity_pointer [ENT_LOWER_WIDTH] = ((width-1) - pivot_x) + modifier;
	entity_pointer [ENT_LOWER_HEIGHT] = ((height-1) - pivot_y) + modifier;
}



void OUTPUT_store_frame_info_in_entity_collision_including_scale (int *entity_pointer, int modifier)
{
	int sprite = entity_pointer[ENT_SPRITE];
	int frame = entity_pointer[ENT_BASE_FRAME];

	int width = (bmps[sprite].sprite_list[frame].width * entity_pointer[ENT_OPENGL_SCALE_X]) / 10000;
	int height = (bmps[sprite].sprite_list[frame].height * entity_pointer[ENT_OPENGL_SCALE_Y]) / 10000;

	int pivot_x = (bmps[sprite].sprite_list[frame].pivot_x * entity_pointer[ENT_OPENGL_SCALE_X]) / 10000;
	int pivot_y = (bmps[sprite].sprite_list[frame].pivot_y * entity_pointer[ENT_OPENGL_SCALE_Y]) / 10000;

	entity_pointer [ENT_UPPER_WIDTH] = pivot_x + modifier;
	entity_pointer [ENT_UPPER_HEIGHT] = pivot_y + modifier;

	entity_pointer [ENT_LOWER_WIDTH] = ((width-1) - pivot_x) + modifier;
	entity_pointer [ENT_LOWER_HEIGHT] = ((height-1) - pivot_y) + modifier;
}





















