#ifndef _ROOMZONES_H_
#define _ROOMZONES_H_

bool ROOMZONES_edit_room_zones (int state , bool overlay_display, int *current_cursor ,int sx, int sy,  int tilemap_number, int mx, int my, float zoom_level, int grid_size);
void ROOMZONES_destroy_localised_zone_list (int tilemap_number);
void ROOMZONES_destroy_zones (int tilemap_number);
void ROOMZONES_generate_localised_zone_lists (void);
localised_zone_list * ROOMZONES_get_localised_list_size_and_pointer (int tilemap_number, int x, int y);
void ROOMZONES_confirm_room_zone_links (void);
void ROOMZONES_set_localised_zone_divider_size (int new_size);
void ROOMZONES_set_zone_flag_by_uid (int map_uid, int zone_uid, int flag_value);

#endif
