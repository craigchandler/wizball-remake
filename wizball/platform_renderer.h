#ifndef _PLATFORM_RENDERER_H_
#define _PLATFORM_RENDERER_H_

typedef struct BITMAP BITMAP;

typedef struct
{
	bool secondary_colour_available;
	bool best_texture_combiner_available;
	bool texture_combiner_available;
	void *secondary_colour_proc;
	void *active_texture_proc;
} platform_renderer_caps;

void PLATFORM_RENDERER_prepare_allegro_gl(bool windowed, int colour_depth);
void PLATFORM_RENDERER_apply_gl_defaults(int viewport_width, int viewport_height, int virtual_width, int virtual_height);
platform_renderer_caps PLATFORM_RENDERER_build_caps(bool enable_multi_texture_effects_ideally, bool has_ext_secondary_color, bool has_ext_texture_env_combine, bool has_ext_multitexture, int opengl_major_version, int opengl_minor_version);
platform_renderer_caps PLATFORM_RENDERER_build_caps_for_current_context(bool enable_multi_texture_effects_ideally, int opengl_major_version, int opengl_minor_version);
bool PLATFORM_RENDERER_query_extensions(int *opengl_major_version, int *opengl_minor_version);
bool PLATFORM_RENDERER_query_and_build_caps(bool enable_multi_texture_effects_ideally, platform_renderer_caps *out_caps);
void PLATFORM_RENDERER_apply_caps(const platform_renderer_caps *caps);
bool PLATFORM_RENDERER_is_extension_supported(const char *extension);
const char *PLATFORM_RENDERER_get_vendor_text(void);
const char *PLATFORM_RENDERER_get_renderer_text(void);
const char *PLATFORM_RENDERER_get_version_text(void);
int PLATFORM_RENDERER_get_extension_count(void);
const char *PLATFORM_RENDERER_get_extension_at(int index);
bool PLATFORM_RENDERER_write_extensions_to_file(const char *path);
void PLATFORM_RENDERER_clear_extensions(void);
const char *PLATFORM_RENDERER_get_allegro_gl_error_text(void);
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
void PLATFORM_RENDERER_set_texture_enabled(bool enabled);
void PLATFORM_RENDERER_set_colour_sum_enabled(bool enabled);
void PLATFORM_RENDERER_bind_texture(unsigned int texture_handle);
void PLATFORM_RENDERER_set_texture_filter(bool linear);
void PLATFORM_RENDERER_set_texture_wrap_repeat(void);
void PLATFORM_RENDERER_apply_texture_parameters(bool filtered, bool old_filtered, bool state_changed, bool force_filter_apply);
void PLATFORM_RENDERER_set_active_texture_proc(void *proc);
bool PLATFORM_RENDERER_is_active_texture_available(void);
bool PLATFORM_RENDERER_set_active_texture_unit(int texture_unit);
bool PLATFORM_RENDERER_set_active_texture_unit0(void);
bool PLATFORM_RENDERER_set_active_texture_unit1(void);
void PLATFORM_RENDERER_set_secondary_colour_proc(void *proc);
bool PLATFORM_RENDERER_set_secondary_colour(float r, float g, float b);
void PLATFORM_RENDERER_set_blend_enabled(bool enabled);
void PLATFORM_RENDERER_set_blend_func(int src_factor, int dst_factor);
void PLATFORM_RENDERER_set_blend_mode_additive(void);
void PLATFORM_RENDERER_set_blend_mode_alpha(void);
void PLATFORM_RENDERER_set_blend_mode_subtractive(void);
void PLATFORM_RENDERER_set_alpha_test_enabled(bool enabled);
void PLATFORM_RENDERER_set_alpha_func_greater(float threshold);
void PLATFORM_RENDERER_set_scissor_rect(int x, int y, int width, int height);
void PLATFORM_RENDERER_set_window_scissor(float left_window_transform_x, float bottom_window_transform_y, float scaled_width, float scaled_height, float display_scale_x, float display_scale_y, float window_scale_multiplier);
void PLATFORM_RENDERER_translatef(float x, float y, float z);
void PLATFORM_RENDERER_scalef(float x, float y, float z);
void PLATFORM_RENDERER_rotatef(float angle_degrees, float x, float y, float z);
void PLATFORM_RENDERER_set_combiner_modulate_primary(void);
void PLATFORM_RENDERER_set_combiner_replace_rgb_modulate_alpha_previous(void);
void PLATFORM_RENDERER_set_combiner_replace_rgb_modulate_alpha_primary(void);
void PLATFORM_RENDERER_set_combiner_modulate_rgb_replace_alpha_previous(void);
void PLATFORM_RENDERER_bind_secondary_texture(unsigned int texture_handle, bool enable_second_unit_texturing);
void PLATFORM_RENDERER_disable_secondary_texture_and_restore_combiner(void);
void PLATFORM_RENDERER_prepare_multitexture_second_unit(bool double_mask_mode);
void PLATFORM_RENDERER_finalize_multitexture_second_unit(bool double_mask_mode);
void PLATFORM_RENDERER_prepare_textured_window_draw(bool texture_combiner_available);
void PLATFORM_RENDERER_finish_textured_window_draw(bool texture_combiner_available, bool had_secondary_texture, bool secondary_colour_available, bool secondary_window_colour, int game_screen_width, int game_screen_height);
void PLATFORM_RENDERER_draw_bound_multitextured_quad_array(const float *x, const float *y, const float *u0, const float *v0, const float *u1, const float *v1);
void PLATFORM_RENDERER_draw_bound_coloured_textured_quad_array(const float *x, const float *y, const float *u, const float *v, const float *r, const float *g, const float *b, const float *a);
void PLATFORM_RENDERER_draw_bound_perspective_textured_quad(float x0, float y0, float x1, float y1, float x2, float y2, float x3, float y3, float u1, float v1, float u2, float v2, float q);
void PLATFORM_RENDERER_draw_bound_coloured_perspective_textured_quad(float x0, float y0, float x1, float y1, float x2, float y2, float x3, float y3, float u1, float v1, float u2, float v2, float q, const float *r, const float *g, const float *b, const float *a);
void PLATFORM_RENDERER_draw_textured_quad(unsigned int texture_handle, int r, int g, int b, float screen_x, float screen_y, int virtual_screen_height, float left, float right, float up, float down, float u1, float v1, float u2, float v2, bool alpha_test);
unsigned int PLATFORM_RENDERER_create_masked_texture(BITMAP *image);
bool PLATFORM_RENDERER_prepare_sdl2_stub(int width, int height, bool windowed);
bool PLATFORM_RENDERER_is_sdl2_stub_ready(void);
bool PLATFORM_RENDERER_is_sdl2_stub_enabled(void);
bool PLATFORM_RENDERER_run_sdl2_stub_self_test(void);
bool PLATFORM_RENDERER_mirror_from_current_backbuffer(int width, int height);
const char *PLATFORM_RENDERER_get_sdl2_stub_status(void);
void PLATFORM_RENDERER_reset_texture_registry(void);
void PLATFORM_RENDERER_shutdown(void);

#endif
