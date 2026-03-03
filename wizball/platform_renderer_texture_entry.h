#ifndef PLATFORM_RENDERER_TEXTURE_ENTRY_H
#define PLATFORM_RENDERER_TEXTURE_ENTRY_H

#include <SDL.h>

// Structure to manage texture metadata and variants
typedef struct
{
    unsigned int legacy_texture_id;
    SDL_Texture *sdl_texture;
    SDL_Texture *sdl_texture_premultiplied;
    SDL_Texture *sdl_texture_inverted;
    unsigned char *sdl_rgba_pixels;
    int width;
    int height;
} platform_renderer_texture_entry;

#endif // PLATFORM_RENDERER_TEXTURE_ENTRY_H