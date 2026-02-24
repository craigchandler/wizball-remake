#include "string_size_constants.h"
#include "new_editor.h" // For the parameter structure

#ifndef _TILESETS_H_
#define _TILESETS_H_

#define BOOL_CONVEY						(1)
#define BOOL_ACCELLERATE				(2)
#define BOOL_SLIPPERY					(4)
#define BOOL_CLIMBABLE_UP_DOWN			(8)
#define BOOL_CLIMBABLE_LEFT_RIGHT		(16)
#define BOOL_CLIMBABLE_OMNI				(32)
#define BOOL_CLIMBABLE_WALL_UP_DOWN		(64)
#define BOOL_CLIMBABLE_MONKEY_BARS		(128)
#define BOOL_STICKY_WALL				(256)
#define BOOL_STICKY_FLOOR				(512)
#define BOOL_DEADLY						(1024)
#define BOOL_KLUDGE_UP_BLOCK			(2048)
#define BOOL_KLUDGE_DOWN_BLOCK			(4096)
#define BOOL_DEKLUDGER					(8192)
#define BOOL_TILE_PATHFIND_IGNORE		(16384)
#define BOOL_ZONE_PATHFIND_IGNORE		(32768)
#define BOOL_WATER						(65536)
#define BOOL_DEEP_WATER					(131072)
#define BOOL_HARMFUL					(262144)
#define BOOL_FORCE_FIELD				(524288)
#define BOOL_SLOPE_RIGHT				(1048576)
#define BOOL_SLOPE_LEFT					(2097152)
#define BOOL_UNUSED_4					(4194304)
#define BOOL_UNUSED_5					(8388608)



typedef struct {
	int neighbouring_blocks; // This is for grouping of blocks. It's a bitmask which says which blocks nearby belong to the same group.
	int family_neighbours;
	int family_id;
	int shape; // The collision profile of the tile.
	int solid_sides; // Which sides of the tile are solid? For "jump up throughable" platforms.
	int default_energy; // Starting energy of this type of tile if it's destructable
	int next_of_kin; // Then this tile dies, who will be reincarnate as? This is the index of a tile within this set.
	bool deviated_stats; // This is automatically set if you have any of the density or other stats != 0.
	bool vulnerability_flag; // If this is set to true then the more processor costly checks for the below will be carried out. It is automatically set and not accessable externally.
	bool use_only_top_vulnerability; // If this is set to true then it doesn't do the more complex side-checks for items colliding with this block.
	int vulnerabilities[4]; // The bitmask which says what kind of things can hurt this tile on each side.
	int boolean_properties; // This is a bit flag with things like "climbable", "sticky wall", "convey" and the like encapsulated in it for the automatic platform handler to use.
	int damage; // This is how much this block hurts anything that touches it.
	int damage_type; // This is the type of damage done to anything which touches it (ie, same bitmask as vulnerabilities).
	int damage_sides; // Bitmask for which sides are deadly.
	char dead_script[NAME_SIZE]; // What script is called when the tile is destroyed? (ie, to create some rubble or summat)
	int dead_script_index; // The index of the script for speedier looking up. Cross matched at run-time.
	int priority; // Used in conjunctions with the objects density to decide in which direction they should move - ie, a low-density object in a high-density tile will move upwards - such as wood in water.
	int convey_x; // Used for conveyor belts or forcefields which move the player a set distance each frame
	int convey_y;
	int accel_x; // Used for sloped tiles or forcefields to accellerate whatever stands on them in a particular direction
	int accel_y;
	int friction; // Not only used for floors but also empty tiles so something like water would slow the player down if they dropped into it
	int bounciness; // How much energy of a collision is absorbed by the tile. A bounciness of 0 means it stops things dead in their tracks. Combined with the bounciness of the object colliding.
	int params;
	unsigned char collision_bitmask; // This has to be compatible with the entity for a collision to occur.
	int temp_data;
	parameter *param_list_pointer; // Well, I may as well, eh? That way gestalt blocks can have paramters like -1,-1 so they point to the keystone of the block.
	int move_collision_to_layer; // This makes the collision get dumped in the numbered layer if it's not UNSET.
} tile;

// Tiles are the little square things you make maps out of and...

typedef struct {
	char name[NAME_SIZE];
	char old_name[NAME_SIZE];
	char tileset_sprite_name[NAME_SIZE];
	int tilesize;
	int tile_count;
	bool deleted;
	tile *tileset_pointer;
	short *tile_conversion_table; // This is used for search and replaces in the map.
	int tileset_image_index;
	bool altered_this_load;
} tileset;

// ...Tilesets are the lovely things which maps are made out of and include a set of tiles
// (duh) each of which has many properties, such as Old Kent Road. Haha! I made a funny!

int TILESETS_tilesize (int tileset_number);
void TILESETS_destroy_tile (int tileset, int tile);
void TILESETS_destroy_tilesets (int tileset);
void TILESETS_destroy_all_tilesets (void);
void TILESETS_swap (int t1, int t2);
void TILESETS_get_free_name (char *name);
int TILESETS_create (bool new_tileset);
void TILESETS_load (char *filename, int tileset_number);
void TILESETS_save (int tileset_number);
void TILESETS_load_all (void);
void TILESETS_save_all (void);
int TILESETS_draw (int tileset_number , int start_x_pixel_offset , int start_y_pixel_offset , float scale , bool grid , int mx, int my , int editing_mode , int highlighted_tile = UNSET , int overlay_icon = UNSET );
bool TILESETS_edit (bool initialise , int starting_tileset_number = UNSET);

void TILESETS_change_name (char *old_entry_name, char *new_entry_name);
void TILESETS_register_script_name_change (char *old_entry_name, char *new_entry_name);
void TILESETS_register_sprite_name_change (char *old_entry_name, char *new_entry_name);
void TILESETS_register_parameter_name_change (char *list_name, char *old_entry_name, char *new_entry_name);

bool TILESETS_is_tile_solid (int tileset_number, int tile_number);

bool TILESETS_confirm_all_links (void);
bool TILESETS_confirm_tile_links (int tileset_number, int tile_number , bool report = false);
bool TILESETS_confirm_tileset_links (int tileset_number , bool report = false);

char * TILESETS_get_name (int tileset_number);
int TILESETS_get_tilesize (int tileset_number);
int TILESETS_get_sprite_index (int tileset_number);
int TILESETS_get_tile_count (int tileset_number);

void TILESETS_draw_loaded_tileset_names (void);

void TILESETS_invert_tile_solidity (void);

void TILESETS_load_game_data (void);

void TILESETS_reset_conversion_table (int tilemap_index);
void TILESETS_add_to_conversion_table (int tilemap_index, int first_original_tile, int last_original_tile, int first_replacement_tile);

// EXTERNS

extern tileset *ts;
extern int number_of_tilesets_loaded;

#endif

