#include "asset_vfs.h"

#include <physfs.h>
#include <physfsrwops.h>

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#if defined(_WIN32)
#include <direct.h>
#define WIZBALL_PATH_MAX MAX_PATH
#else
#include <limits.h>
#define WIZBALL_PATH_MAX PATH_MAX
#endif

#include "string_size_constants.h"

static bool g_asset_vfs_ready = false;
static char g_cache_root[WIZBALL_PATH_MAX] = "";

static bool ASSET_VFS_mode_is_read_only(const char *mode)
{
	return (mode != NULL) && ((strcmp(mode, "r") == 0) || (strcmp(mode, "rb") == 0));
}

static void ASSET_VFS_copy_string(char *dest, size_t dest_size, const char *src)
{
	if ((dest == NULL) || (dest_size == 0))
	{
		return;
	}

	if (src == NULL)
	{
		dest[0] = '\0';
		return;
	}

	strncpy(dest, src, dest_size - 1);
	dest[dest_size - 1] = '\0';
}

static void ASSET_VFS_join_path(char *dest, size_t dest_size, const char *left, const char *right)
{
	size_t len;

	ASSET_VFS_copy_string(dest, dest_size, left);
	len = strlen(dest);
	if ((len > 0) && (dest[len - 1] != '/') && (dest[len - 1] != '\\') && (len + 1 < dest_size))
	{
#if defined(_WIN32)
		dest[len++] = '\\';
#else
		dest[len++] = '/';
#endif
		dest[len] = '\0';
	}

	if ((right != NULL) && (strlen(dest) < (dest_size - 1)))
	{
		strncat(dest, right, dest_size - strlen(dest) - 1);
	}
}

static void ASSET_VFS_make_parent_dirs(const char *filename)
{
	char path[WIZBALL_PATH_MAX];
	size_t i;

	ASSET_VFS_copy_string(path, sizeof(path), filename);
	for (i = 0; path[i] != '\0'; i++)
	{
		if ((path[i] == '/') || (path[i] == '\\'))
		{
			char saved = path[i];
			path[i] = '\0';
			if (path[0] != '\0')
			{
#if defined(_WIN32)
				_mkdir(path);
#else
				mkdir(path, 0777);
#endif
			}
			path[i] = saved;
		}
	}
}

static void ASSET_VFS_build_variant(char *dest, size_t dest_size, const char *source, int components)
{
	size_t len;
	size_t i;
	int component_count = 0;
	size_t start = 0;

	ASSET_VFS_copy_string(dest, dest_size, source);
	len = strlen(dest);
	for (i = len; i > 0; i--)
	{
		if ((dest[i - 1] == '/') || (dest[i - 1] == '\\'))
		{
			component_count++;
			if (component_count == components)
			{
				start = i;
				break;
			}
		}
	}

	for (i = start; dest[i] != '\0'; i++)
	{
		dest[i] = (char)tolower((unsigned char)dest[i]);
	}
}

static const char *ASSET_VFS_find_existing_path_case_fallback(const char *logical_path, char *resolved, size_t resolved_size)
{
	int i;
	char variants[3][TEXT_LINE_SIZE];

	if (!g_asset_vfs_ready || (logical_path == NULL))
	{
		return NULL;
	}

	ASSET_VFS_copy_string(variants[0], sizeof(variants[0]), logical_path);
	ASSET_VFS_build_variant(variants[1], sizeof(variants[1]), logical_path, 1);
	ASSET_VFS_build_variant(variants[2], sizeof(variants[2]), logical_path, 2);

	for (i = 0; i < 3; i++)
	{
		if (PHYSFS_exists(variants[i]))
		{
			ASSET_VFS_copy_string(resolved, resolved_size, variants[i]);
			return resolved;
		}
	}

	return NULL;
}

static const char *ASSET_VFS_materialise_to_cache(const char *logical_path, char *cache_path, size_t cache_path_size)
{
	PHYSFS_File *file_handle;
	PHYSFS_sint64 file_length;
	unsigned char *buffer = NULL;
	PHYSFS_sint64 bytes_read;
	FILE *fp;
	char resolved[TEXT_LINE_SIZE];

	if (ASSET_VFS_find_existing_path_case_fallback(logical_path, resolved, sizeof(resolved)) == NULL)
	{
		return NULL;
	}

	if (g_cache_root[0] == '\0')
	{
		return NULL;
	}

	ASSET_VFS_join_path(cache_path, cache_path_size, g_cache_root, resolved);
	fp = fopen(cache_path, "rb");
	if (fp != NULL)
	{
		fclose(fp);
		return cache_path;
	}

	file_handle = PHYSFS_openRead(resolved);
	if (file_handle == NULL)
	{
		return NULL;
	}

	file_length = PHYSFS_fileLength(file_handle);
	if (file_length < 0)
	{
		PHYSFS_close(file_handle);
		return NULL;
	}

	if (file_length > 0)
	{
		buffer = (unsigned char *)malloc((size_t)file_length);
		if (buffer == NULL)
		{
			PHYSFS_close(file_handle);
			return NULL;
		}

		bytes_read = PHYSFS_readBytes(file_handle, buffer, (PHYSFS_uint64)file_length);
		if (bytes_read != file_length)
		{
			free(buffer);
			PHYSFS_close(file_handle);
			return NULL;
		}
	}

	PHYSFS_close(file_handle);

	ASSET_VFS_make_parent_dirs(cache_path);
	fp = fopen(cache_path, "wb");
	if (fp == NULL)
	{
		free(buffer);
		return NULL;
	}

	if ((file_length > 0) && (fwrite(buffer, 1, (size_t)file_length, fp) != (size_t)file_length))
	{
		fclose(fp);
		free(buffer);
		remove(cache_path);
		return NULL;
	}

	fclose(fp);
	free(buffer);
	return cache_path;
}

bool ASSET_VFS_init(const char *argv0, const char *project_name, const char *pack_project_name)
{
	char archive_path[WIZBALL_PATH_MAX];
	char pref_cache_root[WIZBALL_PATH_MAX];
	const char *pref_dir;

	if (g_asset_vfs_ready)
	{
		return true;
	}

	if (!PHYSFS_init(argv0))
	{
		return false;
	}

	if (PHYSFS_mount("data.zip", NULL, 0) == 0)
	{
		if ((pack_project_name != NULL) && (pack_project_name[0] != '\0'))
		{
			ASSET_VFS_join_path(archive_path, sizeof(archive_path), pack_project_name, "data.zip");
			PHYSFS_mount(archive_path, NULL, 0);
		}
	}

	if ((pack_project_name != NULL) && (pack_project_name[0] != '\0'))
	{
		PHYSFS_mount(pack_project_name, NULL, 1);
	}

	if ((project_name != NULL) && (project_name[0] != '\0') && ((pack_project_name == NULL) || (strcmp(project_name, pack_project_name) != 0)))
	{
		PHYSFS_mount(project_name, NULL, 1);
	}

	pref_dir = PHYSFS_getPrefDir("WizBall", "WizBall");
	if (pref_dir != NULL)
	{
		ASSET_VFS_join_path(pref_cache_root, sizeof(pref_cache_root), pref_dir, "asset_cache");
		ASSET_VFS_copy_string(g_cache_root, sizeof(g_cache_root), pref_cache_root);
		ASSET_VFS_make_parent_dirs(g_cache_root);
#if defined(_WIN32)
		_mkdir(g_cache_root);
#else
		mkdir(g_cache_root, 0777);
#endif
	}

	g_asset_vfs_ready = true;
	return true;
}

void ASSET_VFS_shutdown(void)
{
	if (!g_asset_vfs_ready)
	{
		return;
	}

	PHYSFS_deinit();
	g_cache_root[0] = '\0';
	g_asset_vfs_ready = false;
}

bool ASSET_VFS_is_available(void)
{
	return g_asset_vfs_ready;
}

FILE *ASSET_VFS_open_read_case_fallback(const char *logical_path, const char *mode)
{
	char cache_path[WIZBALL_PATH_MAX];

	if (!ASSET_VFS_mode_is_read_only(mode))
	{
		return NULL;
	}

	if (ASSET_VFS_materialise_to_cache(logical_path, cache_path, sizeof(cache_path)) == NULL)
	{
		return NULL;
	}

	return fopen(cache_path, mode);
}

SDL_RWops *ASSET_VFS_open_rwops_read_case_fallback(const char *logical_path)
{
	char resolved[TEXT_LINE_SIZE];

	if (ASSET_VFS_find_existing_path_case_fallback(logical_path, resolved, sizeof(resolved)) == NULL)
	{
		return NULL;
	}

	return PHYSFSRWOPS_openRead(resolved);
}
