#ifndef _SAVEGAME_H_
#define _SAVEGAME_H_

#include "scripting.h"

loaded_entity_struct *SAVEGAME_find_loaded_entity_by_tag(int tag);
bool SAVEGAME_restore_entity_from_loaded_tag(int entity_number, int tag);
int SAVEGAME_save_matching_live_entities(int variable, int operation, int compare_value, int tag_offset);
int SAVEGAME_spawn_matching_loaded_entities(int variable, int operation, int compare_value);

#endif
