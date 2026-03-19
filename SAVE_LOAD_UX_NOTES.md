## Save/Load UX Notes

## Where Things Stand

Current gameplay behavior:
- `Save and Exit` and `Continue` are working with cross-level persistence inside the active level window.
- The main menu always shows `Continue`, but it is only selectable when a valid compatible continue save exists.
- A save/load spinner is shown before the blocking work begins, but it cannot animate during the actual blocking save/load work.

Current technical status:
- Continue saves still restore live gameplay state using compiled script line pointers.
- Because of that, a continue save is only considered valid when it matches the current compiled script build.
- The current compatibility guard is intentionally conservative: if the compiled gameplay scripts change in a way that changes `scriptfile.tsl`, `Continue` is disabled rather than risking a half-valid restore.

Current behavior:
- The save/load spinner can animate before the blocking work starts.
- During `OUTPUT_SAVE_FILE` and `LOAD_SAVE_FILE`, the engine is effectively blocked doing file I/O and parsing.
- Because the main loop is not advancing during that time, the screen appears frozen and the spinner cannot truly animate.

What this means:
- Script-only polish can improve the transition before and after save/load.
- Script changes alone cannot produce a genuinely animated spinner during the actual save/load freeze.

Possible future engine improvements:

1. Add a busy save/load render loop
- While saving/loading, keep pumping a minimal update/render path.
- This would allow a dedicated loading indicator to animate during blocking work.
- Best UX improvement for the least design change, but requires engine work around the save/load routines.

2. Chunk save/load across multiple frames
- Split entity save/load into batches and process them incrementally.
- This would let normal script/entity animation continue naturally.
- Cleaner long-term design, but a bigger refactor.

3. Keep blocking save/load and only polish transitions
- Use short pre-save and pre-load animation/handoff moments.
- Lowest risk, but the freeze during real work will remain.

Recommendation for now:
- Keep the current short pre-animation behavior.
- Revisit engine-side save/load chunking or a busy-loop renderer only if device testing shows save/load time is a real UX issue.

## Save Compatibility

Current safeguard:
- Continue availability is gated by a script-build id derived from the compiled `scriptfile.tsl` contents.
- This means repeated rebuilds with identical compiled output should keep the same id.
- If the compiled script output changes, old continue saves are hidden by disabling `Continue` instead of being loaded unsafely.

Tradeoff:
- This is safe, but coarse.
- Harmless script rebuilds that still change compiled output will also invalidate continue saves.

Medium-term direction:
- Break save compatibility away from raw compiled line numbers for the key persistent scripts.
- The best target scripts are `main_game_controller` and `generic_level_enemy`.
- That likely means saving stable state ids and resolving them back to current compiled lines through parser-emitted label/state metadata.
- Once that exists, continue saves should survive many more script rebuilds without needing the coarse whole-build guard.

## Save/Load Speedups

Current status:
- The main remaining cost is serializing and parsing many real `generic_level_enemy` entities.
- Save size matters, but CPU time spent formatting/parsing text and walking entity state matters more for perceived speed.

Remaining speed-focused options:

1. Binary entity save format
- Biggest likely win for both save and load speed.
- Avoids expensive text formatting on save and text parsing on load.
- More work than scripting-level optimization, but likely the best pure performance payoff.

2. Save/load in chunks across frames
- Best route if responsiveness matters more than raw throughput.
- Lets the game keep updating while batches of entities are processed.
- Larger engine refactor, but improves both perceived speed and spinner behavior.

3. Trim `generic_level_enemy` serialization further
- Continue cautiously reducing saved variables for `GENERIC_LEVEL_ENEMY`.
- Focus on fields that are defaulted, recomputed, or clearly unused after restore.
- Medium risk: every extra trim should be tested against active enemies, slept enemies, fuzz, and solid diamonds.

4. Save fewer real entities
- Continue limiting entity save scope to the active level window only.
- If future behavior rules allow it, reduce preserved off-level state further.
- Lower risk than trimming core enemy state, but depends on intended gameplay behavior.

5. Precompute save filters
- Avoid repeated per-variable/per-entity decisions during output by caching which fields are eligible for each important script type.
- Smaller win, but low risk and keeps future optimizations cleaner.

6. Faster load-side reconstruction paths
- If some restored entities can safely rebuild part of their state instead of parsing/storing all of it, load can get cheaper.
- This is only worthwhile where reconstruction is deterministic and already proven safe.

Practical next recommendation:
- If speed becomes important again, the best next engineering step is probably a binary entity-data format.
- If UX becomes more important than raw throughput, chunked save/load is the better long-term path.
