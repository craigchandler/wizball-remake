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
bool PLATFORM_RENDERER_prepare_sdl2_stub(int width, int height, bool windowed);
bool PLATFORM_RENDERER_is_sdl2_stub_ready(void);
bool PLATFORM_RENDERER_is_sdl2_stub_enabled(void);
bool PLATFORM_RENDERER_run_sdl2_stub_self_test(void);
bool PLATFORM_RENDERER_mirror_from_current_backbuffer(int width, int height);
const char *PLATFORM_RENDERER_get_sdl2_stub_status(void);
void PLATFORM_RENDERER_shutdown(void);

#endif
