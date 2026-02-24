#include "string_size_constants.h"
#include "new_editor.h" // For the parameter structure

#ifndef _TILEMAPS_H_
#define _TILEMAPS_H_

#define TILEMAP_EDITOR_SUB_GROUP		(999)
#define VERTEX_EDITOR_SUB_GROUP			(998)



#define COLOUR_DISPLAY_MODE_NONE				(0)
#define COLOUR_DISPLAY_MODE_LAYER_VERTEX		(1)
#define COLOUR_DISPLAY_MODE_SIMPLE_VERTEX		(2)
#define COLOUR_DISPLAY_MODE_COMPLEX_VERTEX		(3)



#define EMPTY_COLLISION_TILE				(0)
#define SOLID_COLLISION_TILE				(1)
#define UNPROPPED_COLLISION_TILE			(2)



#define BOOL_HELPER_TAG_WARNING_JUMP			(1)
#define BOOL_HELPER_TAG_WARNING_DROP			(2)
#define BOOL_HELPER_TAG_SAFE_JUMP				(4)
#define BOOL_HELPER_TAG_SAFE_DROP				(8)
#define BOOL_HELPER_TAG_CLIMB					(16)
#define BOOL_HELPER_TAG_PATROL					(32)
#define BOOL_HELPER_TAG_STAY					(64)
#define BOOL_HELPER_TAG_STAIRS					(128)
#define BOOL_HELPER_TAG_OBSTRUCTION				(256)
#define BOOL_HELPER_TAG_STICKY_WALL				(512)
#define BOOL_HELPER_TAG_CLIMB_WALL				(1024)
#define BOOL_HELPER_TAG_DO_NOT_ENTER			(2048)
#define BOOL_HELPER_TAG_POINT_OF_INTEREST		(4096)
#define BOOL_HELPER_TAG_HIDING_POINT			(8192)

#define PATHFINDING_ARRIVED						(-1)
#define PATHFINDING_NO_NODE						(-2)

#define SPAWN_POINT_HANDLE_RADIUS			(16)

#define SPAWN_POINT_HANDLE_CENTER			(0)
#define SPAWN_POINT_HANDLE_HORIZONTAL		(1)
#define SPAWN_POINT_HANDLE_VERTICAL			(2)

#define WAYPOINT_HANDLE_RADIUS				(16)

#define WAYPOINT_HANDLE_CENTER				(0)
#define WAYPOINT_HANDLE_HORIZONTAL			(1)
#define WAYPOINT_HANDLE_VERTICAL			(2)

#define VERTEX_MODE_NONE					(0)
#define VERTEX_MODE_SINGLE_MAP_VALUE		(1)
#define VERTEX_MODE_SIMPLE_TILE_VALUES		(2)
#define VERTEX_MODE_COMPLEX_TILE_VALUES		(3)



#define AI_NODE_CONNECTION_BITFLAG_ONE_WAY	(1)
	// This means that the path-finding can only flood through this in one direction,
	// useful for drops or one-way doors you want the pathfinding to be aware of.
#define AI_NODE_CONNECTION_BITFLAG_BLOCKED	(2)
	// This is the opposite of the above - it's attached to the connected waypoint's
	// complementing connection so that the spreading algorythm doesn't spread backwards.
	// There's a very good chance it could hopelessly fuck over things. Wooo!
#define AI_NODE_CONNECTION_BITFLAG_PORTAL	(4)
	// This means that the connection is a portal and is indicated on the
	// editor as an arrow pointing in the direction of what it connects with.
	// It's length is always 0.
	// It's primary use is for maps where the two remote areas in the tilemap purport
	// to be next to each other in the real map, such as if a door in the background
	// led to another room.



typedef struct {
	char name[NAME_SIZE];
	int x; // Co-ord in tiles.
	int y;
	int uid;
	bool sleeping; // If this is set to true then this spawn point will be passed over and never activated.
	int flag; // This is so an entity can pass data back to a spawn point which can be checked by whatever it creates to decide what state it's in.
	char type[NAME_SIZE];
	int type_value;
	int grabbed_status;
	int grabbed_x;
	int grabbed_y;
	int script_index;
	char script_name[NAME_SIZE];
	int next_sibling_uid; // Unique identification numbers which are used to keep track of who is related to who
	int parent_uid;
	int parent_index;
	int child_index;
	int prev_sibling_index; // Inferred as it's directly paired with the next sibling that points to it.
	int next_sibling_index;
	int params;
	int family_id; // Assigned to each family grouping
	int prev_in_family; // Allows easy and speedy access to the whole family when one member is fired.
	int next_in_family;
	int last_fired; // When the spawn point was last fired to prevent duplicate firing during a frame.
	int created_entity_index; // The index of the entity which was created - to allow linking
	parameter *param_list_pointer;
} spawn_point;



typedef struct {
	char text_tag[NAME_SIZE];
	int text_tag_index;
	int x;
	int y;
	int width;
	int height;
	int uid;
	char type[NAME_SIZE];
	int type_index;
	int flag;
	int real_x; // These are in pixel (ie, world) scale so you can easily
	int real_y; // constrain the camera and active collision window to them.
	int real_width; // These too, dummy.
	int real_height; // Yes, and me.
	int spawn_point_index_list_size; // How many spawn points are within this zone
	int *spawn_point_index_list; // A nice list of the indexes.
	int ai_node_index_list_size; // Like the above but for ai nodes...
	int *ai_node_index_list;
} zone;



typedef struct {
	int x;
	int y;
	int width;
	int height;
	int unique_group_id;
} ai_zone;



typedef struct {
	int x1;
	int y1;
	int x2;
	int y2;
	bool leaked;
	bool double_neighbours;
	bool too_many_rects;
	int side_1_p1_neighbour;
	int side_1_p2_neighbour;
	int side_2_p1_neighbour;
	int side_2_p2_neighbour;
	int neighbour1;
	int neighbour2;
} gate;



typedef struct {
	int x;
	int y;
	int uid;
	char type[NAME_SIZE];
	int type_index;
	char location[NAME_SIZE];
	int location_index;
	int connections;
	int *connection_list_indexes;
	int *connection_list_uids;
	int *connection_list_types; // This is a bitflag value.
	int grabbed_status;
	int grabbed_x;
	int grabbed_y;
	int family; // This says which network of waypoints this belongs to.
	int total_distance; // This is used when flood-filling the network to find the fastest route from A to B
} waypoint;



typedef struct {
	float colours[3];
} simple_vertex_colours;



typedef struct {
	float colours[12];
} detailed_vertex_colours;



// Remote connections are what join two remote parts of a map so that the AI ZONE
// pathfinding can be weighted or you can have a map containing multiple floors of
// a level and join then up nicely.
// As with anything zone based it's x's and y's are stored as tile positions not
// world positions.
typedef struct {
	int x1,y1;
	int x2,y2;
	int zone_1;
	int zone_2;
	int connective_direction;
} remote_connection;



typedef struct {
	int list_size;
	int *indexes;
} localised_zone_list;



typedef struct {
	int first_member;
	int size;
	int *pathfinding_data;
	int *distance_data;
} ai_node_lookup_table;



typedef struct {
	int uid;
	int spawn_point_next_uid;
	int zone_next_uid;
	int waypoint_next_uid;
	char name[NAME_SIZE];
	char old_name[NAME_SIZE];
	char default_tile_set[NAME_SIZE]; // This is the name of the tileset (ie, it's filename minus the extension) which this map is drawn with by default
	int tile_set_index; // This is the number of the tileset within the array - if it's set to -1 then it's a broken link due to the name not being found
	int map_layers;
	int map_width;
	int map_height;
	int waypoints;
	int spawn_points;
	int zones;
	int ai_zones;
	int gates;
	int remotes;
	bool generate_collision;
	bool wraparound_pathfinding;
	short *map_pointer; // This is the array of tiles in the map.
	short *backup_map_pointer;
	bool backup_in_use;
	short *map_offset_pointer;  // This is the array of offsets to the next occupied tile in the map.
	char *group_pointer; // This is where the block groups are stored.
	char *helper_tag_pointer;
	char *helper_x_offset_pointer;
	char *helper_y_offset_pointer;
	detailed_vertex_colours *detailed_vertex_colours_pointer; // This is the detailed vertex colours, ie. where each corner has it's own value;
	simple_vertex_colours *vertex_colours_pointer; // This is simpler than the above where each tile has one colour, making for a more blocky look.
	simple_vertex_colours *layer_vertex_colours_pointer; // And here's the simplest where the whole map is coloured to one colour.
	waypoint *waypoint_list_pointer;
	spawn_point *spawn_point_list_pointer;
	zone *zone_list_pointer;
	ai_zone *ai_zone_list_pointer;
	gate *gate_list_pointer;
	remote_connection *connection_list_pointer;
	char *collision_data_pointer;
	char *exposure_data_pointer;
	unsigned char *collision_bitmask_data_pointer;
	int map_width_in_lzones;
	int localised_zone_list_size;
	localised_zone_list *localised_zone_list_pointer; // This is a list which divides the world into 512x512 pixel blocks and tots up all the zones in said areas.
	int ai_node_lookup_table_list_size;
	ai_node_lookup_table *ai_node_lookup_table_list_pointer; // This is a pointer to all the lovely lookup tables for waypoint pathfinding.
	bool altered_this_load;
	bool optimisation_data_flag;
	short *optimisation_data;
	int vertex_colour_mode;
} tilemap;



void TILEMAPS_destroy_map (int map_number);
void TILEMAPS_destroy_all_maps (void);
int TILEMAPS_get_tile (int map_number, int layer, int x, int y);
void TILEMAPS_put_tile (int map_number, int layer, int x, int y , int tile);
void TILEMAPS_resize_map (int map_number, int x_inc, int y_inc, int layer_inc);
short * TILEMAPS_copy_map_section (int map_number, int slayer, int sx, int sy, int *width, int *height, int *depth, short *copy_area);
void TILEMAPS_paste_map_section (int map_number, int slayer, int sx, int sy, int width, int height, int depth, short *copy_area, int tile_edit_mode);
void TILEMAPS_draw (int start_layer, int end_layer, int map_number, int sx, int sy, int width, int height, float zoom_level, int colour_display_mode);
void TILEMAPS_get_free_name (char *name);
int TILEMAPS_create (bool new_tilemap);
void TILEMAPS_load (char *filename , int map_number);
void TILEMAPS_create_zone_spawn_point_lists (void);
void TILEMAPS_create_real_zone_sizes (void);
void TILEMAPS_create_zone_spawn_point_lists (void);
void TILEMAPS_create_zone_ai_node_lists (void);
void TILEMAPS_save (int map_number);
void TILEMAPS_load_all (void);
void TILEMAPS_save_all (void);
void TILEMAPS_change_name (char *old_entry_name, char *new_entry_name);
void TILEMAPS_register_script_name_change (char *old_entry_name, char *new_entry_name);
void TILEMAPS_register_tileset_name_change (char *old_entry_name, char *new_entry_name);
void TILEMAPS_register_parameter_name_change (char *list_name, char *old_entry_name, char *new_entry_name);
bool TILEMAPS_confirm_links (void);
bool TILEMAPS_edit (bool initialise, int starting_map_number = UNSET);
char * TILEMAPS_get_name (int tilemap_number);
void TILEMAPS_dialogue_box (int x, int y, int width, int height, char *word_string, int r=255, int g=255, int b=255);
void TILEMAPS_output_altered_zone_flags_to_file (char *filename);
void TILEMAPS_input_zone_flags_from_file (char *filename);

void TILEMAPS_backup_tilemap (int map_number);
void TILEMAPS_restore_tilemap (int map_number);

void TILEMAPS_draw_guides (int tilemap_number, int sx, int sy, int tilesize, float zoom_level, int new_guide_box_width = UNSET, int new_guide_box_height = UNSET);

void TILEMAPS_load_game_data (void);
void TILEMAPS_load_editor_settings (void);
void TILEMAPS_save_editor_settings (void);
int TILEMAPS_get_map_from_uid (int map_uid);

int TILEMAPS_get_width (int map_index, bool in_pixels);
int TILEMAPS_get_height (int map_index, bool in_pixels);

int TILEMAPS_get_tilemap_sprite (int map_index);

void TILEMAPS_use_conversion_table (int tilemap_number, int start_layer=UNSET, int end_layer=UNSET);

// EXTERNS

extern int number_of_tilemaps_loaded;
extern tilemap *cm;

extern int editor_view_width;
extern int editor_view_height;

extern int guide_box_width;
extern int guide_box_height;

#endif

