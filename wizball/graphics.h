#ifndef _GRAPHICS_H_
#define _GRAPHICS_H_

#define UNSET						(-1)

#define DRAW_MODE_INVISIBLE						(0)  // So objects default to not being drawn, which is useful for when a wave is created and they are queued up using WAITs.
#define DRAW_MODE_TILEMAP						(1)  // Draw a game layer.
#define DRAW_MODE_TILEMAP_LINE					(2)  // A single horizontal line of tiles, used for games where you want the map to overlap the sprites depending on the height of it.
#define DRAW_MODE_STARFIELD						(3)  // A collection of particles.
#define DRAW_MODE_SOLID_RECTANGLE				(4)  // A solid rectangle. Durr!
#define DRAW_MODE_STARFIELD_LINES				(5)  // A collection of particle lines.
#define DRAW_MODE_STARFIELD_COLOUR				(6) // A collection of colours particles.
#define DRAW_MODE_STARFIELD_COLOUR_LINES		(7) // A collection of colours particles.
#define DRAW_MODE_TEXT							(8) // A bit of text. The sprite is the font's graphic file, the frame is the text tag.
#define DRAW_MODE_HISCORE_ENTRY_NAME			(9) // A name from a hiscore table, like text the sprite is the font graphic, frame is the entry in the table and secondary frame is the unique id of the table.
#define DRAW_MODE_HISCORE_ENTRY_SCORE			(10) // A score from a hiscore table, like text the sprite is the font graphic, frame is the entry in the table and secondary frame is the unique id of the table.

#define DRAW_MODE_SPRITE						(20)  // Standard with no funny business.

#define MAX_NAME_SIZE					(256)



// For each viewport the computer scans through the object list and sees if anything is within the gameworld scope of the viewport and if it is, draws it.

// The data it ploughs through is dependant on whether optimisation buckets are turned on or not. If they are then it'll grab the stuff from the buckets nearby,
// however if it isn't then it'll just get it from the normal draw-ordered list. If you have a game with either multiple viewports or a gameworld that's much
// bigger than the display window, then turn on optimisation. Otherwise you might save a teeny bit if you leave it off.

void GRAPHICS_load_graphics(void);

// Externed stuff...

#endif

