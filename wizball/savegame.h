#ifndef _SAVEGAME_H_
#define _SAVEGAME_H_

#include "scripting.h"

loaded_entity_struct *SAVEGAME_find_loaded_entity_by_tag(int tag);
bool SAVEGAME_restore_entity_from_loaded_tag(int entity_number, int tag);

#endif
