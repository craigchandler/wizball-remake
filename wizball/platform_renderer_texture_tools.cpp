#include <allegro.h>
#include "platform_renderer_texture_entry.h"

static platform_renderer_texture_entry CREATE_platform_renderer_texture_entry_from_BITMAP(unsigned int legacy_texture_id, BITMAP *image)
{
	bool require_nonzero_legacy_texture = false;
	if ((legacy_texture_id == 0) && require_nonzero_legacy_texture)
	{
		return NULL;
	}

  platform_renderer_texture_entry texture_entry;

	texture_entry.legacy_texture_id = legacy_texture_id;
	texture_entry.sdl_texture = NULL;
	texture_entry.sdl_texture_premultiplied = NULL;
	texture_entry.sdl_texture_inverted = NULL;
	texture_entry.sdl_rgba_pixels = NULL;
	texture_entry.width = 0;
	texture_entry.height = 0;

		if ((image != NULL) && (image->w > 0) && (image->h > 0))
		{
			int x;
			int y;
			int image_width = image->w;
			int image_height = image->h;
			int image_depth = bitmap_color_depth(image);
			int mask_colour = bitmap_mask_color(image);
			int mask_r = getr(mask_colour);
			int mask_g = getg(mask_colour);
			int mask_b = getb(mask_colour);
			BITMAP *colour_source = image;
			BITMAP *converted_32 = NULL;
			unsigned char *rgba_pixels = (unsigned char *) malloc((size_t) image_width * (size_t) image_height * 4u);

			if (rgba_pixels != NULL)
			{
				int mask_pixel_count = 0;
				int alpha_zero_pixel_count = 0;
				int alpha_zero_black_rgb_count = 0;

				/*
				 * DAT-loaded sprite sheets are frequently paletted (<=8bpp). Convert once
				 * via Allegro blit into a 32-bit bitmap so SDL upload uses the same
				 * palette-resolved colours as classic Allegro rendering.
				 */
				if (image_depth <= 8)
				{
					converted_32 = create_bitmap_ex(32, image_width, image_height);
					if (converted_32 != NULL)
					{
						clear_to_color(converted_32, makecol_depth(32, 0, 0, 0));
						blit(image, converted_32, 0, 0, 0, 0, image_width, image_height);
						colour_source = converted_32;
					}
				}

				for (y = 0; y < image_height; y++)
				{
					for (x = 0; x < image_width; x++)
					{
						int pixel = getpixel(image, x, y);
						int colour_pixel = getpixel(colour_source, x, y);
						size_t index = ((size_t) y * (size_t) image_width + (size_t) x) * 4u;
						int pixel_r;
						int pixel_g;
						int pixel_b;
						bool pixel_is_mask = false;
						pixel_r = getr(colour_pixel);
						pixel_g = getg(colour_pixel);
						pixel_b = getb(colour_pixel);

					/*
					 * Indexed/paletted assets must use exact pixel-index mask matching.
					 * RGB-equality fallback can over-match and zero out visible texels.
					 * Restrict RGB fallback to true-colour images where packed value
					 * conversion may differ but displayed RGB still matches mask colour.
					 */
					if (pixel == mask_colour)
					{
						pixel_is_mask = true;
					}
					else if ((image_depth >= 15) &&
						(pixel_r == mask_r) && (pixel_g == mask_g) && (pixel_b == mask_b))
					{
						pixel_is_mask = true;
					}

						if (pixel_is_mask)
						{
							mask_pixel_count++;
							alpha_zero_pixel_count++;
							if ((pixel_r == 0) && (pixel_g == 0) && (pixel_b == 0))
							{
								alpha_zero_black_rgb_count++;
							}
							/*
							 * For colour-keyed transparent texels, force RGB to black.
							 * Keeping palette key RGB (often pink in index 0) can leak
							 * through linear filtering and appear as pink squares/halos.
							 */
							rgba_pixels[index + 0] = 0u;
							rgba_pixels[index + 1] = 0u;
							rgba_pixels[index + 2] = 0u;
							rgba_pixels[index + 3] = 0u;
						}
					else
					{
							int alpha = 255;
							/*
							 * Only preserve authored per-pixel alpha for true 32-bit source
							 * bitmaps. Paletted/low-depth sprites are often converted through
							 * an intermediate 32-bit surface for RGB lookup, whose alpha plane
							 * is not meaningful and can be zero-filled.
							 */
							if (image_depth == 32)
							{
								alpha = geta(colour_pixel);
								if (alpha < 0) alpha = 0;
								if (alpha > 255) alpha = 255;
							}
						rgba_pixels[index + 0] = (unsigned char) pixel_r;
						rgba_pixels[index + 1] = (unsigned char) pixel_g;
						rgba_pixels[index + 2] = (unsigned char) pixel_b;
						rgba_pixels[index + 3] = (unsigned char) alpha;
							if (alpha == 0)
							{
								alpha_zero_pixel_count++;
								if ((rgba_pixels[index + 0] == 0u) && (rgba_pixels[index + 1] == 0u) && (rgba_pixels[index + 2] == 0u))
								{
									alpha_zero_black_rgb_count++;
								}
							}
						}
					}
				}
				if (converted_32 != NULL)
				{
					destroy_bitmap(converted_32);
					converted_32 = NULL;
				}

				/*
				 * Force SDL texture pixels to exactly match GL texture contents for
				 * indexed assets. This bypasses palette-state ambiguity in software
				 * conversion paths and keeps SDL-primary rendering consistent with GL.
				 */
				texture_entry.sdl_rgba_pixels = rgba_pixels;
				texture_entry.width = image_width;
				texture_entry.height = image_height;
			}
		}

	// Externally-visible texture ids become renderer-owned handles (1-based).
	return texture_entry;
}
