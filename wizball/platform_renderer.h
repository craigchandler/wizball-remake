#ifndef _PLATFORM_RENDERER_H_
#define _PLATFORM_RENDERER_H_

typedef struct BITMAP BITMAP;

void PLATFORM_RENDERER_clear_backbuffer(void);
void PLATFORM_RENDERER_present_frame(int width, int height);
void PLATFORM_RENDERER_draw_outline_rect(int x1, int y1, int x2, int y2, int r, int g, int b, int virtual_screen_height);
void PLATFORM_RENDERER_draw_filled_rect(int x1, int y1, int x2, int y2, int r, int g, int b, int virtual_screen_height);
void PLATFORM_RENDERER_draw_line(int x1, int y1, int x2, int y2, int r, int g, int b, int virtual_screen_height);
void PLATFORM_RENDERER_draw_circle(int x, int y, int radius, int r, int g, int b, int virtual_screen_height, int resolution);
void PLATFORM_RENDERER_draw_bound_solid_quad(float left, float right, float up, float down);
void PLATFORM_RENDERER_draw_bound_textured_quad(float left, float right, float up, float down, float u1, float v1, float u2, float v2);
void PLATFORM_RENDERER_draw_bound_textured_quad_custom(float x0, float y0, float x1, float y1, float x2, float y2, float x3, float y3, float u1, float v1, float u2, float v2);
void PLATFORM_RENDERER_draw_point(float x, float y);
void PLATFORM_RENDERER_draw_coloured_point(float x, float y, float r, float g, float b);
void PLATFORM_RENDERER_draw_line(float x1, float y1, float x2, float y2);
void PLATFORM_RENDERER_draw_coloured_line(float x1, float y1, float x2, float y2, float r, float g, float b);
void PLATFORM_RENDERER_draw_line_loop_array(const float *x, const float *y, int count);
void PLATFORM_RENDERER_set_window_transform(float left_window_transform_x, float top_window_transform_y, float total_scale_x, float total_scale_y);
void PLATFORM_RENDERER_set_colour3f(float r, float g, float b);
void PLATFORM_RENDERER_set_colour4f(float r, float g, float b, float a);
void PLATFORM_RENDERER_bind_texture(unsigned int texture_handle);
void PLATFORM_RENDERER_set_active_texture_proc(void *proc);
bool PLATFORM_RENDERER_is_active_texture_available(void);
void PLATFORM_RENDERER_set_blend_enabled(bool enabled);
void PLATFORM_RENDERER_set_blend_mode_additive(void);
void PLATFORM_RENDERER_set_blend_mode_multiply(void);
void PLATFORM_RENDERER_set_blend_mode_alpha(void);
void PLATFORM_RENDERER_set_blend_mode_subtractive(void);
void PLATFORM_RENDERER_set_window_scissor(float left_window_transform_x, float bottom_window_transform_y, float scaled_width, float scaled_height, float display_scale_x, float display_scale_y, float window_scale_multiplier);
void PLATFORM_RENDERER_translatef(float x, float y, float z);
void PLATFORM_RENDERER_scalef(float x, float y, float z);
void PLATFORM_RENDERER_rotatef(float angle_degrees, float x, float y, float z);
void PLATFORM_RENDERER_set_combiner_modulate_primary(void);
void PLATFORM_RENDERER_set_combiner_replace_rgb_modulate_alpha_previous(void);
void PLATFORM_RENDERER_set_combiner_replace_rgb_modulate_alpha_primary(void);
void PLATFORM_RENDERER_set_combiner_modulate_rgb_replace_alpha_previous(void);
void PLATFORM_RENDERER_finish_textured_window_draw(bool texture_combiner_available, bool had_secondary_texture, bool secondary_colour_available, bool secondary_window_colour, int game_screen_width, int game_screen_height);
void PLATFORM_RENDERER_draw_bound_multitextured_quad_array(const float *x, const float *y, const float *u0, const float *v0, const float *u1, const float *v1);
void PLATFORM_RENDERER_draw_bound_coloured_textured_quad_array(const float *x, const float *y, const float *u, const float *v, const float *r, const float *g, const float *b, const float *a);
void PLATFORM_RENDERER_draw_bound_perspective_textured_quad(float x0, float y0, float x1, float y1, float x2, float y2, float x3, float y3, float u1, float v1, float u2, float v2, float q);
void PLATFORM_RENDERER_draw_bound_coloured_perspective_textured_quad(float x0, float y0, float x1, float y1, float x2, float y2, float x3, float y3, float u1, float v1, float u2, float v2, float q, const float *r, const float *g, const float *b, const float *a);
void PLATFORM_RENDERER_draw_textured_quad(unsigned int texture_handle, int r, int g, int b, float screen_x, float screen_y, int virtual_screen_height, float left, float right, float up, float down, float u1, float v1, float u2, float v2, bool alpha_test);
void PLATFORM_RENDERER_draw_sdl_window_sprite(unsigned int texture_handle, int r, int g, int b, int a, float entity_x, float entity_y, float left, float right, float up, float down, float u1, float v1, float u2, float v2, float left_window_transform_x, float top_window_transform_y, float total_scale_x, float total_scale_y, float sprite_scale_x, float sprite_scale_y, float sprite_rotation_degrees, bool sprite_flip_x, bool sprite_flip_y);
void PLATFORM_RENDERER_draw_sdl_window_sprite_src(unsigned int texture_handle, int r, int g, int b, int a, float entity_x, float entity_y, float left, float right, float up, float down, int src_x, int src_y, int src_w, int src_h, float left_window_transform_x, float top_window_transform_y, float total_scale_x, float total_scale_y, float sprite_scale_x, float sprite_scale_y, float sprite_rotation_degrees, bool sprite_flip_x, bool sprite_flip_y);
void PLATFORM_RENDERER_draw_sdl_window_solid_rect(int r, int g, int b, float entity_x, float entity_y, float left, float right, float up, float down, float left_window_transform_x, float top_window_transform_y, float total_scale_x, float total_scale_y, float rect_scale_x, float rect_scale_y);
void PLATFORM_RENDERER_draw_sdl_bound_textured_quad_custom(unsigned int texture_handle, float x0, float y0, float x1, float y1, float x2, float y2, float x3, float y3, float u1, float v1, float u2, float v2);
unsigned int PLATFORM_RENDERER_create_masked_texture(BITMAP *image);
bool PLATFORM_RENDERER_prepare_sdl2_stub(int width, int height, bool windowed);
bool PLATFORM_RENDERER_is_sdl2_stub_ready(void);
bool PLATFORM_RENDERER_is_sdl2_stub_enabled(void);
bool PLATFORM_RENDERER_run_sdl2_stub_self_test(void);
const char *PLATFORM_RENDERER_get_sdl2_stub_status(void);
void PLATFORM_RENDERER_reset_texture_registry(void);
void PLATFORM_RENDERER_shutdown(void);

#endif
