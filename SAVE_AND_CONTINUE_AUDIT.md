# Save And Continue Audit

## Purpose

This file captures the first implementation audit for `Save and Exit` plus `Continue`, with a bias toward:

- engine-generic save/resume support
- modular additions instead of growing already-large script files
- rebuilding a level into the same logical state, not frame-perfect unpause

This audit is based on the current Wizball scripts and engine code as of 2026-03-18.

## Main Finding

The current codebase already has most of the serialization plumbing needed for a generic continue system:

- save files can already persist global flags
- save files can already persist spawn-point flags
- save files can already persist zone flags
- save files can already persist entity variables
- save files can already persist entity-owned arrays

The biggest missing engine piece is likely **not** saving arrays. Arrays are already serialized when an entity is written to a save file.

The likely missing piece is:

- a generic way to apply loaded entity data back onto a live entity, keyed by tag

In other words, `LOAD_SAVE_FILE` parses entities and arrays into memory, but `OVER_WRITE_ENTITY_FROM_SAVE_FILE` is currently unimplemented.

## Current Engine Capability

### Already Present

The scripting engine already supports:

- `RESET_SAVE_FILE`
- `WRITE_FLAG_TO_SAVE_FILE`
- `WRITE_SPAWN_POINT_FLAGS_TO_SAVE_FILE`
- `WRITE_ZONE_FLAGS_TO_SAVE_FILE`
- `WRITE_ENTITY_TO_SAVE_FILE`
- `OUTPUT_SAVE_FILE`
- `LOAD_SAVE_FILE`

Relevant code:

- [wizball/scripting.cpp](/home/crchandl/Source/WizBall/wizball/scripting.cpp)
- [wizball/arrays.cpp](/home/crchandl/Source/WizBall/wizball/arrays.cpp)

### Important Detail: Entity Arrays Already Save

`WRITE_ENTITY_TO_SAVE_FILE` eventually calls array serialization via:

- [wizball/arrays.cpp](/home/crchandl/Source/WizBall/wizball/arrays.cpp)

This means the main controller's arrays can already be written out if the controller entity is saved.

That is a strong argument for an engine-generic entity-restore feature, instead of a Wizball-only “copy arrays into flags” workaround.

### Missing Piece

`COM_OVER_WRITE_ENTITY_FROM_SAVE_FILE` exists in the command enum and interpreter switch, but currently does nothing.

That is the best candidate for the generic engine enhancement needed by this feature.

## Authoritative State Findings

## 1. Main Game Controller Is The Core Run-State Owner

[wizball/wizball/scripts/main_game_controller.txt](/home/crchandl/Source/WizBall/wizball/wizball/scripts/main_game_controller.txt) appears to be the main authoritative owner of run progression state.

It owns these arrays:

- `LEVEL_ENEMY_COUNT_ARRAY_ID`
- `LEVEL_COMPLETION_ARRAY_ID`
- `LEVEL_ACCESS_ARRAY_ID`
- `END_OF_BONUS_LEVEL_SUMMARY_ARRAY_ID`
- `CAULDRON_FULLNESS_ARRAY_ID`
- `COMBO_CAULDRON_RGB_ARRAY_ID`
- `DRAINING_TRACKING_ARRAY_ID`

And it also initializes or drives:

- `wizball_starting_loadout`
- `wizball_current_loadout`
- `currently_on_bonus_level`
- current level setup
- bonus/lab transitions
- level-clear progression
- current level enemy-count checks

## 2. Enemy-Wave Progression Is Largely Logical, Not Snapshot-Based

Normal level enemy flow is controlled by:

- `LEVEL_ENEMY_COUNT_ARRAY_ID`
- current level pearl count
- `create_normal_enemies`
- spawn-point driven level rebuild logic

When the current level count hits zero, the controller spawns a new wave generator rather than relying on persistent live enemy entities.

That is good for continue support, because it means we can restore logical level state rather than individual enemy positions.

### Important Correction

That statement is only partially true for exact bonus-return-equivalent behavior.

While the controller owns enemy counts and wave-generation triggers, normal level enemies and molecules are also kept alive across level transitions by using:

- `ENT_TYPE_SLEPT_IN_LEVEL_TRANSITION`

In other words, when the player leaves for bonus/lab flow, the game does **not** just remember counts and respawn a fresh equivalent level. It preserves the current level's persistent entities and wakes them again later.

So if the goal is:

- "whatever happens when you go off to the bonus level should be used as the model"

then exact continue behavior must preserve the same set of transition-slept level entities, not just controller arrays and flags.

## 3. Level Progression Lives In Controller Arrays

These arrays look authoritative for world progression:

- `LEVEL_COMPLETION_ARRAY_ID`
- `LEVEL_ACCESS_ARRAY_ID`
- `CAULDRON_FULLNESS_ARRAY_ID`
- `COMBO_CAULDRON_RGB_ARRAY_ID`
- `DRAINING_TRACKING_ARRAY_ID`

These are not disposable caches. They affect:

- which levels are open
- which levels are complete
- combo cauldron status
- laboratory drain behavior
- how the current level is visually and logically configured

This is the clearest reason to preserve controller-owned arrays.

## 4. Upgrade State Mostly Lives In Globals

The important upgrade and status state appears to live in globals, including:

- `wizball_starting_loadout`
- `wizball_current_loadout`
- `wizball_shield_stored_health`
- `catellite_stored_health`
- `cat_shield_stored_health`
- `player_bonus_pearl_count`

Wizball and Catellite rebuild themselves from those globals when entering gameplay states.

That means we do not need to restore the exact live player entity to get the correct logical state on continue.

## 5. Spawn-Point And Zone Flags Matter

The existing engine already supports saving:

- spawn-point altered flags
- zone altered flags

That likely matters for:

- per-level progression
- enemy availability
- altered world state
- one-time triggers

These should be included in the run save even if the main controller entity is also saved.

## Transition-State Findings

## 6. Event Queue State Is External And Not Currently Serialized

Event queues are implemented in:

- [wizball/events.cpp](/home/crchandl/Source/WizBall/wizball/events.cpp)

They are stored in engine-side queue tables keyed by entity id, not in the entity variable block or entity-owned arrays.

That means queue contents are **not** currently covered by entity save serialization.

This is important because the main controller uses event queues heavily for:

- level setup
- warps
- bonus transitions
- laboratory transitions
- delayed checks

## 7. Save Should Avoid Fragile Transition Windows

Because queue state is not currently restored, `Save and Exit` should probably only be allowed in stable in-level gameplay states for the first version.

Good first-version rule:

- allow save only during ordinary paused gameplay on a normal level
- do not allow save during:
  - warp-out
  - bonus intro
  - bonus level
  - laboratory
  - fly-through
  - get-ready
  - game-over / high-score transition

This is safer and avoids having to serialize queue state immediately.

That restriction can be revisited later if queue persistence becomes desirable as a generic engine feature.

## Resume-Path Findings

## 8. `start_game.txt` Is A Fresh-Run Path, Not A Continue Path

[wizball/wizball/scripts/start_game.txt](/home/crchandl/Source/WizBall/wizball/wizball/scripts/start_game.txt) resets core run state and should not be reused directly for continue.

Continue needs a dedicated path that:

- enables the gameplay windows
- loads the save
- restores the controller entity state
- restores relevant globals/flags
- rebuilds the current level without clobbering saved progression

## 9. The Best Resume Target Is “Return To Main Level” Logic

The desired continue behavior most closely resembles the game’s existing normal return-to-level behavior after transitions.

The code path centered around:

- `swap_levels`
- `set_up_level_display`
- `exit_to_main_level`
- `restart_main_loop`

is a better conceptual target than trying to resume an arbitrary paused frame.

## Recommended Engine-Generic Additions

## A. Implement Generic Entity Restore By Save Tag

Recommended feature:

- implement `OVER_WRITE_ENTITY_FROM_SAVE_FILE`

Proposed behavior:

- script asks to restore a live entity from loaded save data using a numeric tag
- engine finds the loaded entity block for that tag
- engine copies non-relational variables back to the live entity
- engine recreates/restores that entity’s arrays

This would be broadly useful beyond Wizball for any script-driven game with long-lived state-owner entities.

## A2. Add Generic Save/Restore Support For Persistent Level Entities

To match the bonus-return model, the engine likely needs two additional generic script commands:

- one to save matching live entities into the save blob
- one to instantiate saved entities from the loaded save blob before they execute

The relevant target set for Wizball is:

- current-level entities with `ENT_TYPE_SLEPT_IN_LEVEL_TRANSITION`

Why this is needed:

- those entities survive bonus transitions
- they carry the exact current level state
- counts and flags alone are not enough for exact equivalence

The low-level restore path should avoid re-running ordinary spawn-time script initialization when recreating these entities, or at least restore them before their first real logic tick.

### Implemented Engine Surface So Far

Implemented:

- `OVER_WRITE_ENTITY_FROM_SAVE_FILE(entity_id, tag)`
  - restores loaded save data onto a live entity by tag
- `WRITE_MATCHING_ENTITIES_TO_SAVE_FILE variable op value tag_offset`
  - saves all matching live entities using `tag_offset + entity_id`
- `SPAWN_MATCHING_ENTITIES_FROM_SAVE_FILE variable op value`
  - instantiates loaded saved entities whose saved variable matches the comparison

These are generic engine-level commands and are not Wizball-specific.

### Important Remaining Gap

The main controller still has initialization responsibilities that are not captured by save data alone, especially:

- creating its event queue
- rebuilding current-level display/window handlers
- re-entering gameplay through a stable resume path

So the next step is not more low-level serialization. It is a controlled script-side resume boot path, likely with:

- a resume mode flag
- a controller branch that performs only the bootstrap pieces that are still needed
- modular save/continue scripts that use the new engine commands

### Newly Closed Gap: Entity Reference Remapping

The batch entity save/spawn work exposed one more engine-level issue:

- entity-id variables like `parent`, sibling links, and `draw_buddy` were being restored as stale raw ids

That would break exact bonus-return-equivalent resume even if the correct slept entities were restored.

This is now handled generically in the save format by writing selected entity references as saved tags and remapping them after restore/spawn.

The current remapped set is:

- `PARENT`
- `FIRST_CHILD`
- `LAST_CHILD`
- `PREV_SIBLING`
- `NEXT_SIBLING`
- `MATRIARCH`
- `COLLIDED_ENTITY`
- `TARGET_ENTITY`
- `DRAW_BUDDY`

This closes the biggest remaining engine-side fidelity issue for restoring persistent level enemy families.

## B. Add Save File Probe Support

Recommended small generic feature:

- add a script-visible “does this save file exist and validate?” command

Use cases:

- show or hide `Continue`
- reject broken or stale continue saves gracefully

This keeps menu logic out of engine-specific hacks.

### Practical Alternative

Instead of a new engine command, a small continue-metadata save file could also be used to advertise menu availability without loading the full gameplay save into live flags on startup.

## C. Optional Later Feature: Delete Save File

Recommended but not required for first pass:

- add a generic script command to delete a save file

Useful for:

- invalidating continue after game over
- clearing continue on new game

## Recommended Modular Script Structure

To avoid inflating already-large scripts, prefer new files for the continue flow.

Suggested new scripts:

- `save_game_state.txt`
  - top-level save orchestrator
- `save_game_state_flags.txt`
  - writes run-critical flags
- `save_game_state_entities.txt`
  - writes tagged long-lived entities such as the main controller
- `continue_game.txt`
  - main-menu entry path for continuing
- `continue_game_restore.txt`
  - restores saved entities and flags into a new gameplay session
- `continue_game_guard.txt`
  - handles save-valid checks and continue availability

Possible pause/menu wiring scripts:

- `pause_save_and_exit.txt`
- `menu_continue_option.txt`

The goal is to keep:

- `pause_manager.txt`
- `menu_main.txt`
- `main_game_controller.txt`

as lightly touched as possible.

## Resume Bootstrap Direction

The most promising script-side resume path is:

- load the gameplay save
- restore the main controller by stable tag
- respawn saved `ENT_TYPE_SLEPT_IN_LEVEL_TRANSITION` entities
- recreate non-serialized controller runtime state such as the event queue
- re-enter the same-level transition path by setting `player_warp_direction = 0` and using the existing `LEVEL_RESET_FLAG_PLAYER_WARPED` / `swap_levels` flow

That should behave much more like a real return from bonus/lab than a fresh-level reconstruction, while still avoiding frame-perfect pause-state serialization.

## Recommended Engine Module Structure

If the engine work proceeds, prefer new module files rather than further bloating `scripting.cpp`.

Suggested new engine files:

- `wizball/savegame.h`
- `wizball/savegame.cpp`

Candidate responsibilities:

- loaded-save lookup by entity tag
- apply loaded entity data to live entity
- validate save file presence/checksum
- shared helpers for scripted save/load workflows

This would keep generic save/restore logic separate from the already very large script interpreter implementation.

## First-Version Save Scope Recommendation

For the first deliverable, support continue only from stable in-level gameplay.

Save:

- run-critical globals
- spawn-point flags
- zone flags
- main controller entity by tag

Do not try to save:

- event queues
- transient effects
- current live enemy positions
- current player position mid-pause
- bonus/lab/flythrough transitional scenes

On continue:

- boot gameplay
- load save
- restore global flags
- restore main controller entity state
- rebuild the current level through normal level-entry logic

## Concrete Next Step

The next implementation pass should start with two checks:

1. Confirm the main controller can be saved/restored cleanly by tag
2. Implement the engine-side restore hook for loaded entity data

If that works, the rest of the continue system becomes much simpler and more generic.
