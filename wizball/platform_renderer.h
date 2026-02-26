#ifndef _PLATFORM_RENDERER_H_
#define _PLATFORM_RENDERER_H_

typedef struct
{
	bool secondary_colour_available;
	bool best_texture_combiner_available;
	bool texture_combiner_available;
	void *secondary_colour_proc;
	void *active_texture_proc;
} platform_renderer_caps;

void PLATFORM_RENDERER_prepare_allegro_gl(bool windowed, int colour_depth);
void PLATFORM_RENDERER_apply_gl_defaults(int viewport_width, int viewport_height, int virtual_width, int virtual_height, float *projection_matrix_16);
platform_renderer_caps PLATFORM_RENDERER_build_caps(bool enable_multi_texture_effects_ideally, bool has_ext_secondary_color, bool has_ext_texture_env_combine, bool has_ext_multitexture, int opengl_major_version, int opengl_minor_version);
bool PLATFORM_RENDERER_query_extensions(int *opengl_major_version, int *opengl_minor_version);
bool PLATFORM_RENDERER_is_extension_supported(const char *extension);
const char *PLATFORM_RENDERER_get_version_text(void);
int PLATFORM_RENDERER_get_extension_count(void);
const char *PLATFORM_RENDERER_get_extension_at(int index);
void PLATFORM_RENDERER_clear_extensions(void);
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
void PLATFORM_RENDERER_draw_bound_multitextured_quad_array(const float *x, const float *y, const float *u0, const float *v0, const float *u1, const float *v1);
void PLATFORM_RENDERER_draw_bound_coloured_textured_quad_array(const float *x, const float *y, const float *u, const float *v, const float *r, const float *g, const float *b, const float *a);
void PLATFORM_RENDERER_draw_bound_perspective_textured_quad(float x0, float y0, float x1, float y1, float x2, float y2, float x3, float y3, float u1, float v1, float u2, float v2, float q);
void PLATFORM_RENDERER_draw_bound_coloured_perspective_textured_quad(float x0, float y0, float x1, float y1, float x2, float y2, float x3, float y3, float u1, float v1, float u2, float v2, float q, const float *r, const float *g, const float *b, const float *a);
void PLATFORM_RENDERER_draw_textured_quad(unsigned int texture_id, int r, int g, int b, float screen_x, float screen_y, int virtual_screen_height, float left, float right, float up, float down, float u1, float v1, float u2, float v2, bool alpha_test);
bool PLATFORM_RENDERER_prepare_sdl2_stub(int width, int height, bool windowed);
bool PLATFORM_RENDERER_is_sdl2_stub_ready(void);
bool PLATFORM_RENDERER_is_sdl2_stub_enabled(void);
bool PLATFORM_RENDERER_run_sdl2_stub_self_test(void);
bool PLATFORM_RENDERER_mirror_from_current_backbuffer(int width, int height);
const char *PLATFORM_RENDERER_get_sdl2_stub_status(void);
void PLATFORM_RENDERER_shutdown(void);

#endif
