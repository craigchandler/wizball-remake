#include "string_size_constants.h"

#ifndef _NEW_EDITOR_H_
#define _NEW_EDITOR_H_

#define MAX_ICONS				(160)
#define ICON_SIZE				(32)

enum { SELECT_MAP_ICON = 0 , EXIT_ICON , EDIT_MODE_ICON , ZOOM_LEVEL_ICON , GRID_SIZE_ICON , GUIDE_SIZE_ICON , SELECT_LAYER_ICON , TILE_PLACE_MODE_ICON , LAYER_DRAW_MODE_ICON , SELECT_TILE_SET_ICON , MAP_SIZE_ICON , ERASE_PATH_ICON , ERASE_OBJ_ICON , ERASE_ZONE_ICON , ERASE_ZONE_GATE_ICON , AUTO_ZONE_ICON };
enum { ZONE_TOOL_ICON = 16 , SELECT_VARS_ICON , ALTER_NAME_ICON , SELECT_SCRIPT_ICON , DRAW_TOOL_ICON , WINDOW_SIZE_ICON , ERASE_NODE_ICON , PATH_TYPE_ICON , LOOP_OR_NOT_ICON , LOOP_TYPE_ICON , ZONE_NAME_ICON , ZONE_TYPE_ICON , WRAPAROUND_HORIZONTAL_ICON , WRAPAROUND_VERTICAL_ICON , ERASE_SPAWN_POINT_ICON , COPY_SPAWN_POINT_ICON };
enum { RESET_FLAG_ICON = 32 , COPY_WAYPOINT_ICON , LINK_WAYPOINT_ICON , PLONK_TILES_ICON , PAINT_TOOL_ICON };
enum { YES_ICON = 48 , NO_ICON , BIG_YES_ICON , DRAW_USING_TILES_ICON , DRAW_USING_RECTS_ICON , DRAW_USING_FILL_ICON , TILE_MAP_ICON , MAP_VIS_ZONE_ICON , GAME_OBJS_ICON , PATHS_ICON , AI_ZONE_ICON , VIS_ZONE_ICON , SOLID_ICON , ONLY_NON_ZERO_ICON , ONLY_OVER_ZERO_ICON , EVERY_LAYER_ICON };
enum { UP_TO_THIS_LAYER_ICON = 64 , ONLY_THIS_LAYER_ICON , UP_ARROW_ICON , LEFT_ARROW_ICON , BLANK_HORIZONTAL_ICON , CROSS_CENTER_ICON , RIGHT_ARROW_ICON , DOWN_ARROW_ICON , USE_ZONE_TOOL_ICON , USE_GATE_TOOL_ICON , LINEAR_ICON , CATMULL_ROM_ICON , BEZIER_ICON , SIMPLE_ICON, NO_LOOP_ICON , LOOP_ICON };
enum { LOOP_ON_END_ICON = 80 , PING_PONG_ICON , ALTER_GFX_ICON , TOOL_NORMAL_ICON , TOOL_BLOC_REPEAT_ICON , TOOL_FILL_ICON , USE_WARP_TOOL_ICON , NEW_SPAWN_POINT_ICON , EDIT_SPAWN_POINT_ICON , ONLY_USE_TOP_ICON , FRAGILE_PANEL_ICON , NOT_FAMILY_ICON , PREV_SIBLING_ICON , NEXT_SIBLING_ICON , CHILD_ICON , PARENT_ICON };
enum { NEW_NODE_ICON = 96 , EDIT_NODE_ICON , NEW_PATH_ICON , EDIT_PATH_ICON , WITH_CONNECT_ICON , NO_CONNECT_ICON , NEW_WAYPOINT_ICON , EDIT_WAYPOINT_ICON , ERASE_WAYPOINT_ICON , SELECT_TILES_ICON , VERTEX_EDIT_ICON , POINT_TOOL_ICON , GRADIENT_TOOL_ICON , STAMP_TOOL_ICON };
enum { BLOCK_PROFILE_START = 112 };
enum { SCRIPTED_OVERLAY = 147 , FROM_THIS_BLOCK , TO_THIS_BLOCK , ALTERED_STATS_OVERLAY , BROKEN_LINK_ICON , BLACK_OVERLAY , BRIGHT_OVERLAY , GREEN_BOX_OVERLAY , DARK_OVERLAY , DARK_CROSS_OVERLAY , CROSS_OVERLAY , MASK_OVERLAY };
enum { HELPER_TAG_WARNING_JUMP_ICON = 160 , HELPER_TAG_WARNING_DROP_ICON , HELPER_TAG_SAFE_JUMP_ICON , HELPER_TAG_SAFE_DROP_ICON , HELPER_TAG_CLIMB_ICON , HELPER_TAG_PATROL_ICON , HELPER_TAG_STAY_ICON , HELPER_TAG_STAIRS_ICON , HELPER_TAG_OBSTRUCTION_ICON , HELPER_TAG_STICKY_WALL_ICON , HELPER_TAG_CLIMB_WALL_ICON , HELPER_TAG_DO_NOT_ENTER_ICON , HELPER_TAG_POINT_OF_INTEREST_ICON , HELPER_TAG_HIDING_POINT_ICON };
enum { SELECTION_ARROW = 176 , DIAG_ARROW_1_ICON , HORI_ARROW_ICON , DIAG_ARROW_2_ICON , VERT_ARROW_ICON , OMNI_ARROW_ICON , LINK_ARROW_ICON , SELECT_ARROW , CREATE_ARROW , DELETE_ARROW , MOVE_ARROW , NAME_ARROW , FROM_BLOCK_ARROW , TO_BLOCK_ARROW , ZONE_GATE_ARROW };
enum { INFLUENCE_ARROW_START = 192 };
enum { OBJECT_INTERACTION_SIDE_START = 208 };
enum { BLOCK_GROUPING_SIDES_START = 224 };
enum { NODE_SPEED_CURVE_ICON_SLOW_IN = 217 , NODE_SPEED_CURVE_ICON_LINEAR_IN , NODE_SPEED_CURVE_ICON_FAST_IN , NODE_SPEED_CURVE_ICON_SLOW_OUT , NODE_SPEED_CURVE_ICON_LINEAR_OUT , NODE_SPEED_CURVE_ICON_FAST_OUT };
enum { GROUP_FLAGGING_ICON = 240 , BACKWARD_ARROW , FORWARD_ARROW , FAST_BACKWARD_ARROW , FAST_FORWARD_ARROW };
enum { BLOCK_PRIORITY_ICON = 248 };

#define TOP_SIDE_BITVALUE					(1)
#define RIGHT_SIDE_BITVALUE					(2)
#define BOTTOM_SIDE_BITVALUE				(4)
#define LEFT_SIDE_BITVALUE					(8)

#define GROUP_CONVERSION_TILE_NUMBER		(99)



#define VIEW_ALL_LAYERS							(0)
#define VIEW_UP_TO_THIS_LAYER					(1)
#define VIEW_ONLY_THIS_LAYER					(2)

#define EDIT_TILEMAP							(0)
#define EDIT_ROOM_ZONES							(1)
#define EDIT_SPAWN_POINTS						(2)
#define EDIT_PATHS								(3)
#define EDIT_AI_ZONES							(4)
#define EDIT_AI_NODES							(5)
#define EDIT_GROUPING							(6)
#define EDIT_HELPER_TAGGING						(7)
#define EDIT_COLOURING							(8)



#define DIFFERENT_TILEMAP_EDITING_MODES			(9)

#define MENU_OPTION_SELECT_MAP					(1)
#define MENU_OPTION_EDIT_MODE					(2)
#define MENU_OPTION_ZOOM_LEVEL					(3)
#define MENU_OPTION_GRID_SIZE					(4)
#define MENU_OPTION_GUIDE_SIZE					(5)
#define MENU_OPTION_WINDOW_SIZE					(6)
#define MENU_OPTION_SELECT_LAYER				(7)
#define MENU_OPTION_TILE_PLACE_MODE				(8)
#define MENU_OPTION_LAYER_DRAW_MODE				(9)
#define MENU_OPTION_SELECT_TILE_SET				(10)
#define MENU_OPTION_MAP_SIZE					(11)
#define MENU_OPTION_ALTER_NAME_1				(12)
#define MENU_OPTION_EXIT_1						(13)

#define MENU_OPTION_DRAW_TOOL_2					(5)
#define MENU_OPTION_AUTO_ZONE_2					(6)
#define MENU_OPTION_ALTER_NAME_2				(7)
#define MENU_OPTION_EXIT_2						(8)

#define MENU_OPTION_SELECT_SCRIPT				(5)
#define MENU_OPTION_SELECT_VARS					(6)
#define MENU_OPTION_ALTER_NAME_3				(7)
#define MENU_OPTION_EXIT_3						(8)

#define MENU_OPTION_PATH_TYPE					(5)
#define MENU_OPTION_ALTER_NAME_4				(6)
#define MENU_OPTION_EXIT_4						(7)

#define MENU_OPTION_ZONE_TOOL					(5)
#define MENU_OPTION_AUTO_ZONE_5					(6)
#define MENU_OPTION_ALTER_NAME_5				(7)
#define MENU_OPTION_EXIT_5						(8)



#define TILESET_BUTTON_GROUP_ID		(999)
#define TILESET_SUB_GROUP_ID		(800)
#define HORIZONTAL_TILE_DRAG_BAR	(1)
#define VERTICAL_TILE_DRAG_BAR		(2)
#define ALL_OTHER_GUI_ELEMENTS		(3)
#define IMPORTANT_GPL_MENU_1		(4)
#define IMPORTANT_GPL_MENU_2		(5)



#define STATE_INITIALISE									(1)
#define STATE_SHUTDOWN										(2)
#define STATE_RUNNING										(3)
#define STATE_SET_UP_BUTTONS								(4)
#define STATE_RESET_BUTTONS									(5)



typedef struct {
	int x;
	int y;
	int value;
} fill_spreader;



typedef struct {
	char param_dest_var[NAME_SIZE]; // This is the variable of the created entity which will be set.
	int param_dest_var_index;
	char param_list_type[NAME_SIZE]; // This is the word_list that the paramter's value is in (ie, SPRITES)
	char param_name[NAME_SIZE]; // This is the specific list entry whose value will be passed to the entity
	int param_value;
} parameter;



typedef struct {
	bool world_collision; // If this isn't turned on then NO collision with tiles will be taken notice of at all, giving a slight performance increase for things like Asteroids.
	bool world_friction; // Objects will react to the friction value of those blocks which that are. Negative values will accellerate the player in whatever direction they're going already rather than slowing them down. Blank space should have a value of 0.01 or something in order to cause terminal velocity.
	bool world_acceleration; // Objects will react to the accelleration value of those blocks which they are in/on 
	bool world_conveyance; // Objects will react to the conveyance value of those blocks which they are in/on
	bool world_density; // automatically overrides y-axis accelleration values depending upon the density of the object compared with the tile.
	bool world_bounciness; // automatically applies simple (2 axis) bouncing whenever objects collide with the world - does not take into account the angle of slopes - note that velocities of <1 will be damped to 0 on a bounce.
	bool angular_bounciness; // applies complex angular collision responce whenever objects collide with the world - slighty slower but useful for more physicsy games like Arkanoid - note that velocities of <1 will be damped to 0 on a bounce.
	int maximum_entities; // How many doohickies can be alive at once. The more the merrier. And the slower. Possibly.
	char *name[NAME_SIZE]; // What's the game called, eh?
	bool map_change_log; // If this is set to on then all changes to the tilemap are logged for possible saving.
	bool overlapping_zone_pathfinding_linkage; // If this is set then zones are only assumed to lead to one another if they are overlapping.
} game_properties;

// These are the constants which control the general game and which toggle on and off various
// support mechanisms such as things like using friction or using accelleration or density.

fill_spreader * EDIT_add_filler (int list_size, fill_spreader *list_pointer, int x, int y, int value=UNSET);
int EDIT_add_line_to_report (char *line);
void EDIT_setup(void);
void EDIT_load_icons (void);
bool EDIT_edit_map (bool initialise, int starting_map_number = UNSET);
bool EDIT_editor_overview (bool initialise);
void EDIT_close_down (void);
bool EDIT_gpl_list_menu (int x, int y, char *list_name , char *list_entry, bool fixed_list);

// void EDIT_calculate_lookup_paths (void);
// void EDIT_get_path_offset (int path_number, float old_path_pos, float new_path_pos, float *x_vel, float *y_vel, float *angle);


extern int first_icon;
extern int bitmask_gfx;
extern int small_font_gfx;
extern int object_sides_gfx;

#endif

