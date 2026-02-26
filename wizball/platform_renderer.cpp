#include <alleggl.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "platform_renderer.h"

static char *PLATFORM_RENDERER_dup_string(const char *text)
{
	if (text == NULL)
	{
		return NULL;
	}

	size_t length = strlen(text);
	char *copy = (char *) malloc(length + 1);
	if (copy == NULL)
	{
		return NULL;
	}
	strcpy(copy, text);
	return copy;
}

static char *platform_renderer_version_text = NULL;
static int platform_renderer_extension_count = 0;
static char **platform_renderer_extensions = NULL;

void PLATFORM_RENDERER_clear_extensions(void)
{
	int i;

	for (i = 0; i < platform_renderer_extension_count; i++)
	{
		free(platform_renderer_extensions[i]);
	}
	free(platform_renderer_extensions);
	platform_renderer_extensions = NULL;
	platform_renderer_extension_count = 0;

	free(platform_renderer_version_text);
	platform_renderer_version_text = NULL;
}

void PLATFORM_RENDERER_prepare_allegro_gl(bool windowed, int colour_depth)
{
	install_allegro_gl();
	allegro_gl_clear_settings();

	allegro_gl_set(AGL_COLOR_DEPTH, colour_depth);
	allegro_gl_set(AGL_Z_DEPTH, 8);
	allegro_gl_set(AGL_DOUBLEBUFFER, 1);
	allegro_gl_set(AGL_RENDERMETHOD, 1);

	if (windowed)
	{
		allegro_gl_set(AGL_WINDOWED, TRUE);
		allegro_gl_set(AGL_SUGGEST, AGL_COLOR_DEPTH | AGL_DOUBLEBUFFER | AGL_RENDERMETHOD | AGL_Z_DEPTH | AGL_WINDOWED);
	}
	else
	{
		allegro_gl_set(AGL_WINDOWED, FALSE);
		allegro_gl_set(AGL_SUGGEST, AGL_COLOR_DEPTH | AGL_DOUBLEBUFFER | AGL_RENDERMETHOD | AGL_Z_DEPTH | AGL_WINDOWED);
	}
}

void PLATFORM_RENDERER_apply_gl_defaults(int viewport_width, int viewport_height, int virtual_width, int virtual_height, float *projection_matrix_16)
{
	glEnable(GL_TEXTURE_2D);
	glPolygonMode(GL_FRONT, GL_FILL);
	glShadeModel(GL_SMOOTH);
	glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);
	glEnable(GL_SCISSOR_TEST);
	glLineWidth(1.0f);
	glViewport(0, 0, viewport_width, viewport_height);
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glOrtho(0, virtual_width, 0, virtual_height, 0, 255);

	if (projection_matrix_16 != NULL)
	{
		glGetFloatv(GL_PROJECTION_MATRIX, projection_matrix_16);
	}

	glMatrixMode(GL_MODELVIEW);
}

platform_renderer_caps PLATFORM_RENDERER_build_caps(bool enable_multi_texture_effects_ideally, bool has_ext_secondary_color, bool has_ext_texture_env_combine, bool has_ext_multitexture, int opengl_major_version, int opengl_minor_version)
{
	platform_renderer_caps caps;

	caps.secondary_colour_available = has_ext_secondary_color;

	if (has_ext_texture_env_combine && has_ext_multitexture && (((opengl_major_version == 1) && (opengl_minor_version >= 3)) || (opengl_major_version > 1)))
	{
		caps.best_texture_combiner_available = true;
		caps.texture_combiner_available = enable_multi_texture_effects_ideally ? true : false;
	}
	else
	{
		caps.best_texture_combiner_available = false;
		caps.texture_combiner_available = false;
	}

	caps.secondary_colour_proc = allegro_gl_get_proc_address("glSecondaryColor3fEXT");
	caps.active_texture_proc = allegro_gl_get_proc_address("glActiveTextureARB");

	return caps;
}

bool PLATFORM_RENDERER_query_extensions(int *opengl_major_version, int *opengl_minor_version)
{
	if ((opengl_major_version == NULL) || (opengl_minor_version == NULL))
	{
		return false;
	}

	*opengl_major_version = 0;
	*opengl_minor_version = 0;

	PLATFORM_RENDERER_clear_extensions();

	const char *version = (const char *) glGetString(GL_VERSION);
	const char *extension_line = (const char *) glGetString(GL_EXTENSIONS);

	if ((version == NULL) || (extension_line == NULL))
	{
		return false;
	}

	platform_renderer_version_text = PLATFORM_RENDERER_dup_string(version);
	if (platform_renderer_version_text == NULL)
	{
		return false;
	}

	(void) sscanf(platform_renderer_version_text, "%d.%d", opengl_major_version, opengl_minor_version);

	char *count_buffer = PLATFORM_RENDERER_dup_string(extension_line);
	if (count_buffer == NULL)
	{
		PLATFORM_RENDERER_clear_extensions();
		return false;
	}

	int count = 0;
	char *token = strtok(count_buffer, " ");
	while (token != NULL)
	{
		count++;
		token = strtok(NULL, " ");
	}
	free(count_buffer);

	char **list = NULL;
	if (count > 0)
	{
		list = (char **) malloc(sizeof(char *) * count);
		if (list == NULL)
		{
			PLATFORM_RENDERER_clear_extensions();
			return false;
		}
	}

	char *fill_buffer = PLATFORM_RENDERER_dup_string(extension_line);
	if (fill_buffer == NULL)
	{
		if (list != NULL)
		{
			free(list);
		}
		PLATFORM_RENDERER_clear_extensions();
		return false;
	}

	int index = 0;
	token = strtok(fill_buffer, " ");
	while ((token != NULL) && (index < count))
	{
		list[index] = PLATFORM_RENDERER_dup_string(token);
		if (list[index] == NULL)
		{
			int i;
			for (i = 0; i < index; i++)
			{
				free(list[i]);
			}
				if (list != NULL)
				{
					free(list);
				}
				free(fill_buffer);
				PLATFORM_RENDERER_clear_extensions();
				return false;
			}

		index++;
		token = strtok(NULL, " ");
	}
	free(fill_buffer);

	platform_renderer_extension_count = count;
	platform_renderer_extensions = list;
	return true;
}

bool PLATFORM_RENDERER_is_extension_supported(const char *extension)
{
	int i;

	if (extension == NULL)
	{
		return false;
	}

	for (i = 0; i < platform_renderer_extension_count; i++)
	{
		if (strcmp(extension, platform_renderer_extensions[i]) == 0)
		{
			return true;
		}
	}

	return false;
}

const char *PLATFORM_RENDERER_get_version_text(void)
{
	return platform_renderer_version_text;
}

int PLATFORM_RENDERER_get_extension_count(void)
{
	return platform_renderer_extension_count;
}

const char *PLATFORM_RENDERER_get_extension_at(int index)
{
	if ((index < 0) || (index >= platform_renderer_extension_count))
	{
		return NULL;
	}

	return platform_renderer_extensions[index];
}

void PLATFORM_RENDERER_shutdown(void)
{
	PLATFORM_RENDERER_clear_extensions();
}
