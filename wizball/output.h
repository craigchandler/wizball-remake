#ifndef _OUTPUT_H_
#define _OUTPUT_H_

#include <SDL.h>

#define BLEND_OFF		(0)
#define BLEND_ADDITIVE	(1)
#define BLEND_MULTIPLY	(2)



#define OPENGL_BOOLEAN_MASKED							(1)
#define OPENGL_BOOLEAN_BLEND_ADD						(2)
#define OPENGL_BOOLEAN_BLEND_MULTIPLY					(4)
#define OPENGL_BOOLEAN_BLEND_SUBTRACT					(8)
#define OPENGL_BOOLEAN_BLEND_EITHER						(2+4+8)
#define OPENGL_BOOLEAN_SCALE							(16)
#define OPENGL_BOOLEAN_ROTATE							(32)
#define OPENGL_BOOLEAN_VERTEX_COLOUR					(64)
#define OPENGL_BOOLEAN_VERTEX_COLOUR_ALPHA				(128)
#define OPENGL_BOOLEAN_FILTERED							(256)
#define OPENGL_BOOLEAN_VERT_FLIPPED						(512)
#define OPENGL_BOOLEAN_HORI_FLIPPED						(1024)
#define OPENGL_BOOLEAN_ROTATE_CLOCKWISE					(2048)
#define OPENGL_BOOLEAN_VERTEX_SECONDARY_COLOUR			(4096)
#define OPENGL_BOOLEAN_SECONDARY_SCALE					(8192)
#define OPENGL_BOOLEAN_SECONDARY_ROTATE					(16384)
#define OPENGL_BOOLEAN_OVERRIDE_PIVOT					(32768)
#define OPENGL_BOOLEAN_MULTITEXTURE_MASK				(65536)
#define OPENGL_BOOLEAN_MULTITEXTURE_DOUBLE_MASK			(131072)
#define OPENGL_BOOLEAN_CLIP_FRAME						(262144)
#define OPENGL_BOOLEAN_INTERPOLATED						(524288)
#define OPENGL_BOOLEAN_ARBITRARY_QUAD					(1048576)
#define OPENGL_BOOLEAN_ARBITRARY_PERSPECTIVE_QUAD		(2097152)
#define OPENGL_BOOLEAN_INDIVIDUAL_VERTEX_COLOUR_ALPHA	(4194304)



#define OPENGL_CIRCLE_RESOLUTION		(32.0f)

#define FONT_WIDTH				(6)
#define FONT_HEIGHT				(8)

#define SOFTWARE_FONT_WIDTH		(8)
#define SOFTWARE_FONT_HEIGHT	(8)

void INPUT_load_media_datafiles (void);
void INPUT_unload_datafiles (void);

void OUTPUT_message (char *msg);
void OUTPUT_store_frame_info_in_entity_collision (int *entity_pointer, int modifier=0);
void OUTPUT_store_frame_info_in_entity_collision_including_scale (int *entity_pointer, int modifier=0);
void OUTPUT_truncated_text (int max_chars ,int x, int y, char *text, int r=255, int g=255, int b=255);
void OUTPUT_text (int x, int y, char *text, int r=255, int g=255, int b=255, int scale=10000);
void OUTPUT_centred_text (int x, int y, char *text, int r=255, int g=255, int b=255);
void OUTPUT_right_aligned_text (int x, int y, char *text, int r=255, int g=255, int b=255);
void OUTPUT_boxed_centred_text (int x, int y, char *text, int r=255, int g=255, int b=255);

void OUTPUT_line (int x1 , int y1, int x2, int y2, int r=255, int g=255, int b=255);
void OUTPUT_hline (int x1 , int y1, int x2, int r=255, int g=255, int b=255);
void OUTPUT_vline (int x1 , int y1, int y2, int r=255, int g=255, int b=255);
void OUTPUT_circle (int x , int y , int radius , int r=255, int g=255, int b=255);
void OUTPUT_centred_square (int x , int y , int widthandheight , int r=255, int g=255, int b=255);
void OUTPUT_rectangle (int x1, int y1, int x2, int y2, int r=255, int g=255, int b=255);
void OUTPUT_rectangle_by_size (int x, int y, int width, int height, int r=255, int g=255, int b=255);
void OUTPUT_filled_rectangle (int x1, int y1, int x2, int y2, int r=255, int g=255, int b=255);
void OUTPUT_filled_rectangle_by_size (int x, int y, int width, int height, int r=255, int g=255, int b=255);

void OUTPUT_clear_screen (void);
void OUTPUT_updatescreen (void);
void OUTPUT_setup_allegro (bool windowed, int colour_depth, int base_screen_width, int base_screen_height, int screen_multiplier);

void OUTPUT_draw_sprite (int bitmap_number , int sprite_number, int x, int y, int r=255, int g=255, int b=255);
void OUTPUT_draw_masked_sprite (int bitmap_number , int sprite_number, int x, int y, int r=255, int g=255, int b=255);

int INPUT_load_bitmap (char *filename);
int INPUT_load_bitmap_SDL(const char *filename, SDL_Renderer *renderer);

void INPUT_create_sprite (int parent_bitmap, int sprite_number, char *name, int x, int y, int width, int height, int pivot_x, int pivot_y);
void INPUT_create_equal_sized_sprite_series (int parent_bitmap, char *name_base, int sprite_width, int sprite_height, int pivot_x, int pivot_y);
void INPUT_create_arbitrary_sized_sprite_series (int parent_bitmap, char *name_base, char *descriptor_file);

void OUTPUT_destroy_bitmap_and_sprite_containers (void);

int OUTPUT_get_sprite_pivot_x (int bitmap_number , int sprite_number);
int OUTPUT_get_sprite_pivot_y (int bitmap_number , int sprite_number);
int OUTPUT_get_sprite_width (int bitmap_number , int sprite_number);
int OUTPUT_get_sprite_height (int bitmap_number , int sprite_number);
void OUTPUT_get_sprite_uvs (int bitmap_number , int sprite_number, float &u1, float &v1, float &u2, float &v2);

int OUTPUT_bitmap_width (int bitmap_number);
int OUTPUT_bitmap_height (int bitmap_number);
int OUTPUT_bitmap_frames (int bitmap_number);

void OUTPUT_draw_sprite_scale (int bitmap_number, int sprite_number , int x , int y , float scale , int r=255, int g=255, int b=255);
void OUTPUT_draw_sprite_scale_no_pivot (int bitmap_number, int sprite_number , int x , int y , float scale, int r=255, int g=255, int b=255);

void OUTPUT_shutdown (void);

void OUTPUT_draw_rotated_scaled_blended_coloured_sprite (int texture_number, int sprite_number, int x, int y, int z=-1.0f, float angle=0.0f, float scale_x=1.0f, float scale_y=1.0f, int blend_type=BLEND_OFF, float blend_alpha=1.0f, float red=1.0f, float green=1.0f, float blue=1.0f);

void OUTPUT_draw_arrow (int x1, int y1, int x2, int y2, int red=255, int green=255, int blue=255, bool suppress_line = false);

int OUTPUT_draw_window_contents (int window_number, bool use_texture_combiners);
//int OUTPUT_draw_window_contents_no_multi_texture (int window_number);
void OUTPUT_draw_window_collision_contents (int window_number, bool draw_world_collision);

void OUTPUT_blank_window_contents (int window_number);

void OUTPUT_enable_multi_texture_effects (void);
void OUTPUT_disable_multi_texture_effects (void);

void OUTPUT_setup_project_list (char *text);
void OUTPUT_setup_running_mode (void);

void OUTPUT_list_all_window_contents (int look_for_script);

void OUTPUT_set_arbitrary_quad_from_base (int entity_index);

extern int game_screen_width;
extern int game_screen_height;

extern bool software_mode_active;

extern bool enable_multi_texture_effects_ideally;

extern bool texture_combiner_available;
extern bool best_texture_combiner_available;

extern int number_of_windows;



#endif
