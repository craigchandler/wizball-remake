#ifndef _ASSET_VFS_H_
#define _ASSET_VFS_H_

#include <stdio.h>
#include <SDL.h>

bool ASSET_VFS_init(const char *argv0, const char *project_name, const char *pack_project_name);
void ASSET_VFS_shutdown(void);
bool ASSET_VFS_is_available(void);

FILE *ASSET_VFS_open_read_case_fallback(const char *logical_path, const char *mode);
SDL_RWops *ASSET_VFS_open_rwops_read_case_fallback(const char *logical_path);

#endif
