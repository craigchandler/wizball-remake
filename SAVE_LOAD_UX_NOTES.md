## Save/Load UX Notes

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

