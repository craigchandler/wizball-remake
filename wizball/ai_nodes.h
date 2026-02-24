#ifndef _AI_NODES_H_
#define _AI_NODES_H_

bool AINODES_edit_ai_nodes (int state , bool overlay_display, int *current_cursor ,int sx, int sy,  int tilemap_number, int mx, int my, float zoom_level, int grid_size);
void AINODES_destroy_waypoint (int tilemap_number, int waypoint_number);
void AINODES_relink_all_waypoint_connections (int tilemap_number);
void AINODES_confirm_waypoint_links (void);
void AINODES_create_all_waypoint_lookup_tables (void);
void AINODES_destroy_waypoint_lookup_tables (int tilemap_number);
void AINODES_output_waypoint_lookup_tables (void);

#define ABSURDLY_HIGH_NUMBER		(99999999)

#endif
