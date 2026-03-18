# Save And Continue Plan

## Goal

Add:

- `Save and Exit` to the in-game pause menu
- `Continue` to the main menu

The intended behavior is not a full pause-frame suspend/resume. Instead, the game should resume the current run with the exact same logical level state you would expect after re-entering a level normally, especially after bonus/lab-style flow:

- same current level
- same level progression and clearance state
- same remaining enemy/wave state for that level
- same permanent and current upgrades
- same score/lives/core run state
- same bonus-related run state where relevant

The player does **not** need to resume at the exact same pixel position, with the same bullets, particles, or transient mid-fight entity positions.

## Design Target

Treat this as a **run-state restore plus level rebuild**, not a literal world snapshot.

That means:

- Save the state that defines what the run and current level logically are
- Rebuild the level from that saved state when continuing
- Reuse existing "return to level" logic where possible
- Avoid restoring transient frame-to-frame objects unless there is no safer abstraction

This should make resume behave like entering that level again after a normal game flow transition, rather than unpausing a frozen simulation.

## Current Codebase Notes

### Pause/Menu State

- Pause menu currently lives in [wizball/wizball/scripts/pause_manager.txt](/home/crchandl/Source/WizBall/wizball/wizball/scripts/pause_manager.txt)
- Main menu currently lives in [wizball/wizball/scripts/menu_main.txt](/home/crchandl/Source/WizBall/wizball/wizball/scripts/menu_main.txt)
- Menu setup is in [wizball/wizball/scripts/menu_setup.txt](/home/crchandl/Source/WizBall/wizball/wizball/scripts/menu_setup.txt)
- Menu text is in [wizball/wizball/textfiles/menu.txt](/home/crchandl/Source/WizBall/wizball/wizball/textfiles/menu.txt)

### Existing Save Infrastructure

The engine already supports:

- `RESET_SAVE_FILE`
- `WRITE_FLAG_TO_SAVE_FILE`
- `WRITE_SPAWN_POINT_FLAGS_TO_SAVE_FILE`
- `WRITE_ZONE_FLAGS_TO_SAVE_FILE`
- `WRITE_ENTITY_TO_SAVE_FILE`
- `OUTPUT_SAVE_FILE`
- `LOAD_SAVE_FILE`

Implementation is in [wizball/scripting.cpp](/home/crchandl/Source/WizBall/wizball/scripting.cpp).

### Existing Save Usage

At the moment, live Wizball gameplay does **not** use a full run save.

- Startup loads `SAVE_FILE_TAG` in [wizball/wizball/scripts/startup.txt](/home/crchandl/Source/WizBall/wizball/wizball/scripts/startup.txt)
- `SAVE_FILE_TAG` currently points to `unlocked_tunes` in [wizball/wizball/textfiles/filenames.txt](/home/crchandl/Source/WizBall/wizball/wizball/textfiles/filenames.txt)
- The only writer currently found is [wizball/wizball/scripts/save_unlocked_tunes.txt](/home/crchandl/Source/WizBall/wizball/wizball/scripts/save_unlocked_tunes.txt)

So the project already has save-file plumbing, but not a gameplay continue system.

### Fresh Start Reset Path

[wizball/wizball/scripts/start_game.txt](/home/crchandl/Source/WizBall/wizball/wizball/scripts/start_game.txt) hard-resets major run state, including:

- `player_on_level_number`
- `player_lives`
- `player_score`
- `player_display_score`
- `first_life_flag`

It then spawns the main controller and pause manager.

This means `Continue` should probably **not** reuse `start_game.txt` unchanged.

### Main Controller State

[wizball/wizball/scripts/main_game_controller.txt](/home/crchandl/Source/WizBall/wizball/wizball/scripts/main_game_controller.txt) initializes and owns important arrays and run-state setup, including:

- `LEVEL_ENEMY_COUNT_ARRAY_ID`
- `LEVEL_COMPLETION_ARRAY_ID`
- `LEVEL_ACCESS_ARRAY_ID`
- `END_OF_BONUS_LEVEL_SUMMARY_ARRAY_ID`
- `CAULDRON_FULLNESS_ARRAY_ID`
- `COMBO_CAULDRON_RGB_ARRAY_ID`
- `DRAINING_TRACKING_ARRAY_ID`

It also initializes:

- starting/current loadout
- pause enable timing
- event queue flow
- reset and level transition behavior

This file is the core place that will likely need a `resume mode`.

## Resume Scope

### Must Preserve

- current level number
- player lives
- player score and any score-display state that must remain consistent
- single/two-player mode if relevant to save compatibility
- permanent upgrades
- current upgrades/loadout
- shield/catellite stored state
- level access and completion state
- cauldron/progression state
- bonus-related run state
- state that determines what enemies remain available or defined for the current level
- spawn-point and zone progression that affect reconstruction

### Explicitly Not Required

- exact player coordinates from the paused frame
- exact enemy coordinates/velocities
- bullets
- particles
- one-frame transition states
- exact per-frame simulation continuity

## Recommended Save Strategy

### 1. Use A Dedicated Gameplay Save File

Do not reuse the current `SAVE_FILE_TAG` that points to `unlocked_tunes`.

Add a dedicated gameplay save slot, for example:

- a new filename tag in [wizball/wizball/textfiles/filenames.txt](/home/crchandl/Source/WizBall/wizball/wizball/textfiles/filenames.txt)
- a corresponding GPL entry in [wizball/wizball/global_parameter_list.txt](/home/crchandl/Source/WizBall/wizball/wizball/global_parameter_list.txt)

This keeps:

- meta persistence like unlocked tunes separate from
- run persistence for continue

### 2. Save Logical Run State, Not Full Scene State

The primary save should consist of:

- global flags for the run
- spawn-point flags
- zone flags
- any controller-owned state that defines progression and current-level rebuild behavior

Only save entities if there are a small number of long-lived controller/helper entities whose internal variables are genuinely easier to restore than converting them into globals or array state.

### 3. Rebuild The Current Level On Continue

Continue should:

- load the saved run data
- spawn the game in a special resume path
- rebuild the current level from saved logical state

This should mimic re-entering a level after a normal bonus/lab return, not unfreezing a snapshot.

## Likely Save Data Buckets

### A. Core Run Flags

Audit and save the globals that define the run, including likely candidates such as:

- `player_on_level_number`
- `player_lives`
- `player_score`
- `player_display_score`
- `first_life_flag` if relevant
- `currently_on_bonus_level`
- any level-start phase flags
- any lab/bonus return routing flags
- any mode flags needed for compatibility

### B. Upgrade And Health State

Likely candidates:

- `wizball_starting_loadout`
- `wizball_current_loadout`
- `wizball_shield_stored_health`
- `catellite_stored_health`

Also audit any other globals that influence what the player should have on entry to the restored level.

### C. Level Progression State

Must preserve things like:

- which levels are open
- which levels are complete
- what cauldrons contain
- any combination/progression state that affects level rebuild and unlock flow

This likely includes controller arrays, not just globals.

### D. Current Level Enemy/Spawn Definition State

This is the subtle part.

The save needs to preserve the logical state of the current level so it reloads with:

- the correct remaining enemy pool
- the correct "what is left to spawn" behavior
- the correct level-specific progress

The first implementation should prefer saving:

- spawn-point flags
- zone flags
- per-level counters/arrays

over trying to serialize every enemy entity.

### E. Bonus/Lab/Transition State

Audit any state that affects:

- whether the player is returning from a bonus
- permanent bonus application
- temporary bonus application
- laboratory availability and progression
- post-bonus or post-transition rebuild behavior

## Key Technical Unknown: Arrays

The biggest likely technical issue is array persistence.

Important state appears to live in arrays owned by the main controller. If the authoritative state is in arrays, then a clean continue system probably needs one of:

- script-level support to save/load named arrays
- engine support to serialize arrays directly
- a script bridge that copies critical array contents into saveable globals/entity vars before saving and restores them on continue

This needs to be resolved early, because it will shape the implementation.

## Implementation Plan

### Phase 1. Audit Resume-Critical State

Goal: build a concrete list of everything required to restore a run safely.

Tasks:

- Audit global flags touched by `start_game`, `main_game_controller`, `wizball`, `catellite`, lab flow, and bonus flow
- Identify which controller arrays are authoritative and must persist
- Determine whether enemy counts are derived from spawn-point state or only cached in arrays
- Find the best existing path that resembles "re-enter a level after bonus/lab"
- Identify transient flags that must **not** be restored literally

Deliverable:

- a saved-state checklist with exact flags/arrays/entities to persist

### Phase 2. Add Dedicated Run Save Slot

Goal: create separate persistence for gameplay continue.

Tasks:

- Add new gameplay save filename tag
- Add save-valid or continue-available signal/flag
- Decide when a run save is created, overwritten, cleared, or invalidated

Questions:

- Should `New Game` clear the continue save immediately?
- Should game over clear the continue save?
- Should entering the hiscore flow invalidate continue?

### Phase 3. Add Save Script

Goal: write the current run state to the dedicated gameplay save.

Tasks:

- Create a `save_current_run` script
- Save audited global flags
- Save spawn-point flags
- Save zone flags
- Save any required controller/helper entity state if needed
- Save array-backed state via engine support or a script bridge

Integration:

- Call this from the pause-menu `Save and Exit` path before teardown to menu

### Phase 4. Add Continue Path

Goal: let the main menu enter gameplay via saved run data.

Tasks:

- Add `Continue` option to main menu text and handling
- Show it only when the gameplay save is valid
- Add a `continue_game` script or equivalent boot path
- Avoid fresh-game reset logic from `start_game.txt`

### Phase 5. Add Main Controller Resume Mode

Goal: rebuild the run/level from saved state instead of starting fresh.

Tasks:

- Add a resume flag or resume entry path to `main_game_controller`
- Skip initialization that would wipe saved progression
- Rehydrate array/global state before current level setup
- Re-enter the level through the cleanest existing "return to level" logic

Important:

This should behave like arriving back into the level after a normal transition, not like a frozen scene restore.

### Phase 6. Add Array Persistence If Needed

Goal: support exact logical level reconstruction.

Preferred options, in order:

1. Save/load critical arrays directly through new engine support
2. Save/load only a small number of controller/helper entities if that naturally preserves the arrays they own
3. Copy array contents into save-friendly structures before save, then restore after load

This is the biggest implementation risk and should be isolated as much as possible.

### Phase 7. Clear/Invalidate Save Appropriately

Goal: avoid stale or misleading continue behavior.

Tasks:

- Clear continue save on new game if that is the chosen behavior
- Clear or invalidate after game over if required
- Ensure the main menu only offers `Continue` when the save is actually usable
- Consider checksum/version handling if save format changes

## Testing Matrix

### Core Flow

- Start a run, save from pause, return to menu, continue, and verify same current level and run state
- Save, quit app, relaunch, continue, and verify persistence across sessions

### Progression Integrity

- Save with partial level completion and verify the same levels remain open/closed
- Save with partial cauldron or progression state and verify it restores correctly
- Save after a permanent bonus upgrade and verify both permanent and current loadouts are correct
- Save with current temporary state differences and verify entry state matches intended post-transition behavior

### Enemy/Level Integrity

- Save on a level with a partially consumed enemy pool and verify the same logical enemy state rebuilds
- Confirm the level does **not** reset to a fresh level definition
- Confirm the level does **not** resume with stale transient entities like bullets/particles

### Bonus/Lab Flow

- Save after returning from bonus once and verify continue preserves that post-bonus state
- Save around laboratory/progression transitions and verify permanent upgrades and routing still behave correctly

### Edge Cases

- Save near game over
- Save after level clear but before/after transition points
- Save with catellite/shield damaged
- Save in single-player and two-player modes if both should be supported

## Risks

### 1. Hidden Authoritative State

Some important progress may live:

- in controller arrays
- in long-lived entity-local variables
- in event queue state

If so, restoring only globals will be incomplete.

### 2. Fresh-Game Logic Clobbering Saved State

If `Continue` reuses `start_game.txt` or unmodified controller init, saved state may be silently overwritten.

### 3. Transition State Ambiguity

Some flags may represent transient "currently transitioning" behavior that should not be restored directly.

### 4. Array Persistence Complexity

If arrays are authoritative and not currently saveable in a clean way, a small engine feature may be necessary.

## Recommended First Milestone

The first concrete milestone should be:

- dedicated gameplay save file
- pause-menu `Save and Exit`
- main-menu `Continue`
- successful restoration of:
  - current level
  - score/lives
  - permanent/current upgrades
  - level completion/open state
  - current level logical enemy/progression state

without restoring transient moment-to-moment entities.

## Next Session Checklist

- Audit all globals that define run state for continue
- Audit all main-controller arrays and decide which are authoritative
- Trace the exact "return from bonus/lab to level" path
- Decide whether array persistence needs engine support
- Choose where to store "continue save exists / valid" state
- Implement dedicated gameplay save filename tag

