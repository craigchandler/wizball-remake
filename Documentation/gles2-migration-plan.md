# GLES2 Migration Plan

## Current state

The project now has a real direct GLES2 renderer in `wizball/platform_renderer.cpp`.
This is no longer just SDL's `opengles2` renderer backend. The game creates an
SDL window with a GLES2 context and submits GLES draws directly.

Current reality:

- Direct GLES2 path is visually working in the main known scenes:
  - intro tunnel
  - menu/title
  - gameplay
  - footer HUD
  - wiztips
- The renderer is still not clearly faster than the SDL path on device.
- The renderer shape is still partly inherited from the old immediate model.
- Current work is focused on turning that into a more renderer-owned batched
  GLES path without breaking scene correctness.

## Build

Default:

```bash
cmake -S . -B build
cmake --build build -j
```

GLES2-targeted build:

```bash
cmake -S . -B build-gles2-local -DWIZBALL_RENDER_BACKEND=GLES2
cmake --build build-gles2-local -j
```

PortMaster package build:

```bash
WIZBALL_RENDER_BACKEND=GLES2 ./scripts/build_portmaster.sh
```

## Runtime override

```bash
WIZBALL_RENDERER_BACKEND=gles2 ./build-gles2-local/wizball.x86_64
```

PortMaster launcher defaults to GLES2 for device testing unless overridden.

## Startup finding

The big pause after the first visible loading frame is not currently a GLES
renderer problem. Measured startup timings showed:

- `MAIN_draw_loading_picture`: about `16 ms`
- `SOUND_setup`: about `229 ms`
- `SOUND_load_sound_effects`: about `1322 ms`
- `SOUND_open_sound_streams`: about `13078 ms`
- everything after that was negligible by comparison

Treat that pause as an audio-startup issue, not a renderer issue.

## Renderer history

### What is already in

- Direct GLES2 context/bootstrap exists and is used.
- Clear/present, solid quads, textured quads, coloured textured quads,
  perspective batches, and multitexture paths all have direct GLES2 branches.
- Shared GLES textured submission now uses streamed VBO/IBO fallback-capable
  submission instead of only client-side arrays.
- Texture filter/wrap state is cached on texture entries to avoid redundant
  `glTexParameteri` calls.
- Several earlier debug/profiling-only GLES log paths and dead bring-up helpers
  were removed.
- A narrow renderer-owned GLES simple-quad queue now exists for a proven-safe
  subset of simple sprites.

### What was proven wrong or too risky

- Broad `output.cpp` sprite batching was unsafe.
  - It broke credits text, title imagery, menu/title animation, gameplay HUD,
    and other sprite semantics.
  - Conclusion: broad batching at the entity walker level is the wrong layer.
- Broad removal of flushes from global state setters was unsafe.
  - It caused visible blend/mask/filter regressions.
  - Conclusion: deferred flushing must stay local to known-safe batch paths.
- Widening the `output.cpp` simple-sprite queue into gameplay/HUD semantics was
  unsafe.
  - Gameplay footer sprites, wiztips, and other mixed-semantics sprites broke.
  - Conclusion: the sprite walker is too semantically rich to widen casually.

## Current batching architecture

### 1. Renderer-owned simple-quad queue

There is now a GLES-only renderer-owned simple-quad queue in
`wizball/platform_renderer.cpp`.

Its purpose:

- keep a stable affine state
- queue already-prepared simple quads
- flush only at correctness boundaries

This queue is fed from `wizball/output.cpp` for a very conservative subset of
simple sprites.

Current proven-safe subset:

- `DRAW_MODE_SPRITE`
- no multitexture
- no blend flags
- no vertex-colour flags
- no interpolation
- no clip-frame
- no flips
- no scale/rotate/secondary transform
- no arbitrary quad / arbitrary perspective quad

This subset is intentionally narrow because broader `output.cpp` widening broke
gameplay/HUD correctness.

### 2. Deeper renderer seam widening

Work has started to move batching deeper into normalized renderer paths instead
of the sprite walker.

These public renderer paths now participate in the queue more safely:

- `PLATFORM_RENDERER_draw_bound_textured_quad(...)`
- `PLATFORM_RENDERER_draw_bound_textured_quad_custom(...)`

This is the direction to continue. The renderer seam is safer than the entity
walker because more semantics are already normalized there.

### 3. Shared GLES textured batch

There is also a shared GLES textured batch used below the queue.

Important current rule:

- `source_id` must be preserved all the way through batching and final flush

This was necessary because earlier batching merged different renderer families
together and caused visible corruption, such as the lab wizard tiling issue.

The current code now carries `source_id` in:

- simple-quad queue state
- shared GLES textured batch key
- final textured batch flush state

## Current blocker

The current unresolved visible regression is:

- lab screen wizard still tiles across the screen
- the wizard is also drawn in the correct place
- this means there is likely still one remaining path where a queued/batched
  coloured textured quad family is being interpreted with the wrong semantics

What has already been ruled out:

- the original simple-quad queue losing `source_id`
- the shared textured batch losing `source_id`

Those were both fixed.

So if the wizard still tiles after those fixes, the remaining cause is likely:

- the exact lab-screen draw path itself
- or another batching key/geometry assumption inside the coloured textured quad
  array path

That exact path should be inspected next, not guessed at broadly.

## Evidence-backed performance findings

### Text over intro tunnel

Instrumentation showed:

- the slowdown when intro/menu text appears is not mainly GLES flush thrash
- it is driven by a large number of per-letter text entities
- ordinary `OUTPUT_text()` string batching is not the dominant path there
- the slow path is mostly script/entity-driven letter sprites

This means:

- speeding up intro/menu animating text requires renderer-side batching on the
  normalized quad path, not another broad `output.cpp` text/sprite shortcut

### Tunnel path

Useful tunnel-side changes already in:

- adaptive strip count for GLES perspective batches
- some CPU allocator churn removed from tunnel builders
- more transform work moved from CPU to shader for perspective families

These were safe and kept visuals intact, but they have not yet delivered the
device-speed jump that would justify GLES2 on their own.

## What to avoid next

- Do not widen `output.cpp` sprite batching blindly again.
- Do not remove global setter flushes broadly.
- Do not patch one broken text/sprite script at a time with more exceptions.
- Do not assume SDL-vs-GLES has been fairly proven yet; the current renderer
  shape is still too close to the old immediate model.

## Best next direction

The next meaningful work should stay deeper in `platform_renderer.cpp`.

Priority order:

1. Fix the exact coloured textured quad array / lab wizard tiling regression.
2. Continue widening batching at normalized renderer seams, not in
   `output.cpp`.
3. Push more of the common quad traffic through renderer-owned queues/batches.
4. Only after correctness is stable, re-measure whether GLES is actually
   pulling ahead on device.

If a new chat continues this work, it should start by investigating the lab
wizard path through:

- `PLATFORM_RENDERER_draw_bound_coloured_textured_quad_array(...)`
- the simple-quad queue enqueue/flush path
- the shared GLES textured batch key/flush path

and verify why that path still tiles even after `source_id` preservation was
added throughout batching.

## Non-goals right now

- no more broad entity-walker batching experiments
- no speculative state-boundary loosening
- no claiming GLES2 is definitively better than SDL yet

The goal is to earn that conclusion by fixing batching at the right renderer
layer and then measuring again.
