# Render Queue / Tunnel Pass Target

## Goal

Move the SDL renderer path away from immediate-mode entity drawing and toward a small number of explicit render passes that preserve current visuals but give the renderer control over batching, caching, and submission order.

Primary near-term target:

- keep existing script semantics
- keep existing visual ordering where it matters
- improve stability toward 30fps on muOS / Panfrost
- avoid a "big bang" rewrite

This is not a 60fps plan. It is a practical plan to stop feeding SDL a hostile draw stream.

---

## Current problem

The engine still behaves like this:

1. Scripts update entity state.
2. `scripting.cpp` threads entities through y/z window lists.
3. `output.cpp` walks those lists and issues draw calls immediately.
4. `platform_renderer.cpp` translates that immediate stream into SDL operations.

That model preserves old semantics, but it gives SDL almost no control over:

- pass boundaries
- batching
- static layer caching
- additive effect grouping
- low-cost renderer-owned effects

The hot menu tunnel path demonstrates the problem clearly:

- it is authored as many ordinary entities
- those entities are interpreted one-by-one
- the renderer receives generic per-entity draws rather than one tunnel effect submission

Even after large submission reductions, the hot phase is still far too slow because the renderer is reacting to script/entity output rather than owning the frame.

---

## Key question: do scripts need to change?

Not for phase 1.

Phase 1 should change how script output is interpreted, not script source.

Desired contract:

- scripts still express the same scene state
- output layer stops drawing immediately
- output layer emits render intents / render commands
- renderer decides how to batch and submit those commands

Possible later script changes:

- explicit pass hints
- "cacheable layer" hints
- "renderer-owned effect" declarations
- reduced-resolution effect hints

Those are optional later improvements, not phase-1 requirements.

---

## What "changing hot tunnel submission" means

The tunnel should stop being submitted as many generic scripted draws.

Instead:

1. Detect tunnel-related entities in `output.cpp`.
2. Collect their state into a dedicated tunnel batch.
3. Submit the tunnel as one renderer-owned pass, or a very small number of passes.

Minimum viable interpretation change:

- keep scripts unchanged
- keep entities unchanged
- during output traversal, do not immediately draw hot tunnel entities
- append their data into a tunnel collector
- flush the tunnel collector as a dedicated pass at a controlled point

Best long-term version:

- scripts describe tunnel state only
- renderer procedurally builds the tunnel pass itself
- tunnel is no longer represented as dozens of ordinary draw entities

---

## Phase 1 target

Phase 1 is only worth doing if it changes submission structure.

Phase 1 should include:

1. Introduce a render command queue between `output.cpp` and `platform_renderer.cpp`.
2. Preserve current script/entity semantics.
3. Preserve current high-level window/z ordering semantics.
4. Add explicit pass buckets:
   - background/static
   - world sprites/quads
   - additive effects
   - UI/text
5. Add a dedicated tunnel collector/pass for the main menu tunnel path.
6. Allow static or slow-changing layers to be cached later without redesigning the API again.

Phase 1 should not try to redesign every effect in the game.

It should focus on the hot path first:

- `MENU_TUNNEL_STARFIELD`
- `MAIN_MENU_TUNNEL_LIGHT`
- title/logo overlay interactions around the tunnel

---

## Proposed staged path

### Stage 0: Document and trace

Required work:

- document current draw ownership and pass boundaries
- identify the exact set of entities/scripts that make up the hot menu tunnel scene
- record visual checkpoints for regression testing

Output:

- stable list of scripts/entities that belong to the tunnel pass
- clear before/after screenshots or videos for comparison

### Stage 1: Introduce render commands

Required work:

- define a compact render-command struct
- have `output.cpp` emit commands instead of drawing immediately
- initially preserve exact order and flush them in the same order

Why this matters:

- it creates the seam needed for later pass ownership
- it can be introduced with low visual risk

Expected fps effect:

- small by itself
- mostly architectural unless paired with pass grouping

### Stage 2: Add explicit pass buckets

Required work:

- split command recording into a few explicit passes
- keep ordering constraints where needed
- do not yet reorder within sensitive passes

Likely passes:

- solid / non-textured
- normal alpha textured
- additive effects
- UI/text

Expected fps effect:

- meaningful if hot scenes stop interleaving incompatible work

### Stage 3: Tunnel collector

Required work:

- detect tunnel entities during output traversal
- collect them into a tunnel-specific batch/state object
- submit that batch at one controlled point

This is the first phase with a realistic chance of materially improving the hot menu scene.

### Stage 4: Renderer-owned tunnel pass

Required work:

- reduce reliance on per-entity generic submission
- have the renderer construct tunnel geometry from collected state
- isolate non-ADD parts from the large ADD run

This is where the renderer starts to own the effect rather than just translating it.

### Stage 5: Cache and optional quality controls

Optional but likely useful:

- cache static or slowly-changing layers
- offscreen render targets for expensive effect layers
- reduced internal resolution for additive-only layers if needed

This stage has more visual risk and should come after phase-1 stabilization.

---

## Initial implementation shape

### Render command seam

Add a small intermediate layer, for example:

```cpp
enum render_pass_id
{
    RENDER_PASS_BACKGROUND,
    RENDER_PASS_WORLD,
    RENDER_PASS_ADDITIVE,
    RENDER_PASS_UI
};

enum render_cmd_type
{
    RENDER_CMD_SPRITE,
    RENDER_CMD_SOLID_RECT,
    RENDER_CMD_TEXTURED_QUAD,
    RENDER_CMD_PERSPECTIVE_QUAD,
    RENDER_CMD_TUNNEL_ITEM
};
```

Each command should carry:

- pass id
- texture handle
- blend mode
- geometry data
- colour data
- strict order key if required

### Tunnel collector

The tunnel collector should gather:

- texture handle(s)
- blend mode(s)
- perspective quad inputs
- colour/alpha inputs
- relative ordering boundaries against title/UI

Then expose one renderer entry point:

- `submit_menu_tunnel_pass(...)`

The renderer can still internally preserve ordering where required, but it stops being forced to do it one scripted draw at a time.

---

## Visual testing strategy

This does not need to be a blind rewrite.

It can be staged with real visual testing after each step:

1. Add render command queue with exact-order replay.
   Expectation:
   visuals identical.

2. Add pass separation without tunnel specialization.
   Expectation:
   visuals identical or nearly identical.

3. Add tunnel collector but preserve exact internal order.
   Expectation:
   visuals identical.

4. Add tunnel-owned batching / grouping.
   Expectation:
   compare directly against baseline captures.

Good validation scenes:

- main menu tunnel idle
- title/logo fade-ins
- heavy particle/effect section from the menu sequence
- any scene with additive overlays and perspective quads

Recommended validation method:

- capture screenshots/video on device at named checkpoints
- compare before/after visually
- keep frame logs from the same scene windows

---

## Is this a really big task?

Not if scoped correctly.

It is a medium-sized architectural task, not a tiny patch, but it does not need to be a full engine rewrite.

What makes it manageable:

- scripts can stay unchanged at first
- the first seam is `output.cpp`, not the entire scripting language
- the hot tunnel path can be targeted first
- each stage can be tested visually before moving on

What would make it a big risky task:

- trying to replace all rendering paths at once
- redesigning scripts at the same time
- combining render queue, tunnel ownership, caching, and low-res effects into one branch

Recommended scope:

- small, testable phases
- preserve behavior first
- optimize hot path second

That is realistic.

---

## Practical recommendation

Start with this narrow milestone:

1. Add a render-command queue.
2. Replay in exact order first.
3. Add a dedicated tunnel collector in `output.cpp`.
4. Submit the tunnel as one controlled renderer pass.
5. Compare visuals and logs after each step.

That is the lowest-risk path with a credible chance of improving hot-scene performance while keeping the current game looking the same.
