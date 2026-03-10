# GLES2 Migration Plan

## Current state

The live renderer is still the SDL renderer path in `platform_renderer.cpp`.
On muOS/PortMaster it already lands on SDL's `opengles2` backend, but the game
does not yet own the GLES2 draw pipeline directly.

This phase adds:

- a build-time renderer target selector: `WIZBALL_RENDER_BACKEND=SDL|GLES2`
- a runtime override: `WIZBALL_RENDERER_BACKEND=sdl|gles2`
- explicit backend reporting in startup logs
- SDL renderer driver preference for `opengles2` when `GLES2` is targeted

This is intentionally a foundation step. It keeps a working build while the
draw path is migrated in slices.

## Build

Default:

```bash
cmake -S . -B build
cmake --build build -j
```

GLES2-targeted build:

```bash
cmake -S . -B build-gles2 -DWIZBALL_RENDER_BACKEND=GLES2
cmake --build build-gles2 -j
```

PortMaster package build:

```bash
WIZBALL_RENDER_BACKEND=GLES2 ./scripts/build_portmaster.sh
```

## Runtime override

```bash
WIZBALL_RENDERER_BACKEND=gles2 ./wizball.x86_64
```

The current runtime still uses SDL renderer submission. The selector exists so
the direct GLES2 backend can be introduced behind the same contract.

The PortMaster launcher now defaults `WIZBALL_RENDERER_BACKEND=gles2` so device
testing stays on the GLES2-targeted path unless overridden.

## Known findings

Startup stall after the first visible loading frame is currently dominated by
audio initialization, not renderer submission. Measured local startup timings:

- `MAIN_draw_loading_picture`: about `16 ms`
- `SOUND_setup`: about `229 ms`
- `SOUND_load_sound_effects`: about `1322 ms`
- `SOUND_open_sound_streams`: about `13078 ms`
- `SCRIPTING_setup_data_stores`: about `0 ms`
- `MATH_setup_trig_tables`: about `4 ms`
- `SCRIPTING_load_prefabs`: about `1 ms`
- `ARRAY_load_datatables`: about `1 ms`

That means the large pause after the first loading-screen frame should be
treated as a startup audio issue, not a GLES renderer issue. Keep that work
separate from renderer optimization.

## Planned migration order

1. Keep texture handles and texture registry backend-neutral.
2. Introduce SDL window + GLES2 context bootstrap beside the SDL renderer path.
3. Move simple paths first:
   - clear/present
   - solid quads
   - standard textured quads
4. Move hot paths next:
   - perspective tunnel batches
   - additive effect pass
5. Add renderer-owned passes:
   - opaque/background
   - alpha textured
   - additive effects
   - UI/text
6. Add optional low-resolution offscreen effect targets for the additive-heavy
   tunnel phase if full-resolution fill remains too expensive.

## Non-goals for phase 0

- no script changes
- no visual reordering
- no attempt to finish the full GLES2 renderer in one pass
