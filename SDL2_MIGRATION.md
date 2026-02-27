# SDL2 Migration Tracker

This document tracks migration of WizBall from Allegro 4 + FMOD to SDL2.

## Scope

- Replace Allegro platform, windowing, rendering, input, timing, file-path assumptions.
- Replace FMOD audio with SDL2 audio stack (`SDL_mixer` or custom SDL audio callback).
- Keep gameplay, scripts, data formats, and behavior stable during migration.

## Build Commands (Preserve)

Keep these working throughout migration so existing workflows are not lost.

### Configure + Build (current)

```bash
cmake -S . -B build
cmake --build build -j
```

### Run

```bash
# from repository root
cd wizball
../build/wizball

# datafile mode
../build/wizball -dat
```

### Rebuild scripts + repack data

```bash
# one-shot target (recommended)
cmake --build build --target rebuild_wizball_scripts_and_data

# equivalent manual flow
cd wizball
../build/wizball -rebuildscripts wizball
cd ..
python3 tools/repack_wizball_data.py --root . --dat-tool dat --include-core-data
```

### Repack only

```bash
cmake --build build --target repack_wizball_data
cmake --build build --target repack_wizball_data_full
```

### Optional build variants already in use

```bash
# FMOD-enabled build
cmake -S . -B build_fmod -DWIZBALL_ENABLE_FMOD=ON
cmake --build build_fmod -j

# ASan build
cmake -S . -B build_asan -DWIZBALL_ENABLE_ASAN=ON
cmake --build build_asan -j
```

## Current State (Baseline)

- Engine: Allegro 4 + AllegroGL + OpenGL fixed pipeline.
- Audio: FMOD Ex (plus stub backend for non-FMOD builds).
- Data: mixed text + compiled data assets (`.dat`, `.tsl`, `.datatable`, etc.).
- Linux non-`-dat` path handling has case-fallback hardening.

## Migration Strategy

- Use an incremental compatibility layer to avoid a big-bang rewrite.
- Land vertical slices (boot -> input -> draw -> sound) that keep game runnable.
- Preserve deterministic behavior and timing semantics as much as possible.

## PortMaster Direction (Important)

For the PortMaster target (Linux handhelds across mixed ARM SoCs/drivers), prefer robust portability over backend-specific optimization.

- Default render path should be `SDL_Renderer`, not direct OpenGL.
- Let SDL choose backend at runtime (GLES/KMSDRM/software where applicable).
- Avoid hard dependency on desktop OpenGL.
- Only introduce explicit GL/GLES path if a measured bottleneck proves necessary.

### Why

- PortMaster devices vary widely in GPU/driver quality.
- Many devices expose OpenGL ES, not desktop OpenGL.
- Driver quirks are common; `SDL_Renderer` reduces platform-specific code.
- For this game class (Allegro-era 2D), SDL renderer performance is typically sufficient.

### Practical Rules

- Keep gameplay update loop fixed-step; decouple rendering.
- Prefer texture atlases and reduce texture switches.
- Avoid unnecessary full-screen scaling every frame.
- Target modest internal resolution (320x240 or 640x480), then scale to display.
- Keep blending simple and avoid shader-heavy effects in baseline path.

### Backend/Runtime Policy

- Do not force a single SDL render driver globally.
- Allow override via environment for debugging (`SDL_RENDER_DRIVER`), but keep auto-select as default.
- Keep a software-render fallback path functional.

### Packaging/Test Expectations (PortMaster)

- Validate launch on at least one low-end and one newer handheld class.
- Test both windowed/dev Linux and handheld runtime paths.
- Verify controls, audio latency, and frame pacing on real hardware.
- Ensure no hardcoded absolute paths; rely on project-relative paths.
- Keep startup and data rebuild/repack commands documented and unchanged where possible.

## Workstreams

### 1. Platform Abstraction

- [ ] Create `platform/` layer with interfaces for:
- [~] Window / context lifecycle
- [x] Timing / frame pacing / high-resolution clock
- [~] Input state and events
- [ ] File/path helpers
- [ ] Clipboards / message boxes / logging hooks
- [ ] Route existing Allegro calls behind wrappers (first pass: thin adapters).

### 2. Build System

- [x] Add SDL2 find/link options in CMake.
- [x] Add feature toggle (e.g. `WIZBALL_ENABLE_SDL2`).
- [x] Keep Allegro build path compiling during transition.
- [ ] Add CI matrix target for SDL2 build.

### 3. Rendering

- [ ] Inventory current AllegroGL + fixed-pipeline usage.
- [ ] Decide render backend path:
- [ ] `SDL_Renderer` (fastest migration, limited flexibility), or
- [ ] SDL window + modern OpenGL/Vulkan abstraction (more work, longer-term better).
- [ ] Replace bitmap/texture lifecycle calls.
- [ ] Replace screen update/buffering model (`DOUBLEBUFFER`, `TRIPLEBUFFER`, etc.).
- [ ] Validate sprite UV/pivot behavior matches legacy output.

### 4. Input

- [~] Replace keyboard polling and key repeat logic.
- [~] Replace mouse state and wheel handling.
- [~] Replace joystick/gamepad setup and readback.
- [ ] Preserve existing input recording/playback semantics.

### 5. Audio (FMOD -> SDL2)

- [ ] Choose audio path:
- [ ] `SDL_mixer` for faster parity, or
- [ ] custom SDL audio mixing for full control.
- [ ] Port sound effect loading/playback/channels.
- [ ] Port streamed music path and loop behavior.
- [ ] Port global sound/music volume behavior.
- [ ] Remove FMOD-specific driver selection code.

### 6. File + Asset I/O

- [ ] Replace Allegro packfile/datafile dependencies if needed.
- [ ] Validate `-dat` and non-`-dat` modes under SDL2 build.
- [ ] Keep case-fallback path behavior in shared file layer.

### 7. Timing + Main Loop

- [ ] Replace Allegro timer callbacks/interrupt model.
- [ ] Preserve fixed-step update behavior.
- [ ] Preserve decoupled animation/screen refresh behavior.
- [ ] Recheck watchdog/failsafe logic under SDL timing.

### 8. UI / Menus / Fonts

- [ ] Replace Allegro text/font drawing path.
- [ ] Preserve menu flow, credits scrolling, and selection behavior.
- [ ] Ensure no regressions in high score/name entry screens.

### 9. Save/Data Compatibility

- [ ] Verify existing save files still load.
- [ ] Verify script parser output remains unchanged.
- [ ] Verify dat repack workflow remains valid.

### 10. Cleanup

- [ ] Remove dead Allegro code paths once SDL2 parity is complete.
- [ ] Remove FMOD backend once SDL audio is stable.
- [ ] Update README/build docs for SDL2-first workflow.

## Milestones

### M1: SDL2 Boot

- [ ] App starts, opens window, processes quit events.
- [ ] Timing loop runs with stable frame updates.

### M2: SDL2 Visual Parity (Menu)

- [ ] Main menu draws correctly.
- [ ] Input works end-to-end in menu.

### M3: SDL2 Audio Parity

- [ ] SFX and music play correctly.
- [ ] Volume controls and looping behavior match baseline.

### M4: First Playable Level

- [ ] In-game render/input/audio functional.
- [ ] No critical regressions in core gameplay loop.

### M5: Decommission Allegro/FMOD

- [ ] Allegro/FMOD removed from default build.
- [ ] Docs and packaging updated.

## Risks

- Behavior drift in timing-sensitive gameplay.
- Hidden assumptions tied to Allegro buffer semantics.
- Audio channel behavior differences vs FMOD.
- Asset loading/path edge cases on Linux/macOS.

## Open Decisions

- [ ] `SDL_Renderer` vs SDL + OpenGL backend
- [ ] `SDL_mixer` vs custom mixer
- [ ] Keep `.dat` workflow as-is or replace over time

## Progress Snapshot (2026-02-26)

### Workstreams

- Platform Abstraction: ~75%
- Build System: ~85%
- Rendering: ~65%
- Input: ~80%
- Audio (FMOD -> SDL): ~10%
- File + Asset I/O: ~45%
- Timing + Main Loop: ~55%
- UI / Menus / Fonts: ~35%
- Save/Data Compatibility: ~40%
- Cleanup (remove Allegro/FMOD): ~10%

### Milestones

- M1 SDL2 Boot: ~95%
- M2 SDL2 Visual Parity (Menu): ~70%
- M3 SDL2 Audio Parity: ~5%
- M4 First Playable Level: ~60%
- M5 Decommission Allegro/FMOD: ~0-5%

### Current Top Blockers

- Make SDL renderer path truly primary (no GL mirror/fallback dependency).
- Audio migration off FMOD.
- Full parity validation pass (menu/gameplay, `-dat`/non-`-dat`, regression matrix).

## Change Log

- 2026-02-26: Tracker created.
- 2026-02-26: Added preserved build/repack command reference.
- 2026-02-26: Added `WIZBALL_ENABLE_SDL2` CMake option + SDL2 link/discovery path.
- 2026-02-26: Added initial platform abstraction files (`platform.h/.cpp`) and routed wall-clock timing through `PLATFORM_get_wall_time_ms()`.
- 2026-02-26: Verified both default build (`build`) and SDL2-enabled build (`build_sdl2`) compile.
- 2026-02-26: Added `platform_input.h/.cpp` abstraction seam and routed keyboard/mouse polling in `control.cpp` through platform input helpers.
- 2026-02-26: Extended `platform_input` seam to joystick install/calibration/count/state/axis polling and routed `control.cpp` joystick paths through shared helpers.
- 2026-02-26: Verified both default build (`build`) and SDL2-enabled build (`build_sdl2`) compile after joystick abstraction pass.
- 2026-02-26: Routed remaining input setup/read callsites (`install_keyboard`, `install_mouse`, `readkey`) through `platform_input` wrappers so `control.cpp` no longer directly calls Allegro input APIs.
- 2026-02-26: Added SDL event pump seam (`PLATFORM_INPUT_pump_events`) with latched quit flag APIs and wired pump usage into game/editor outer loops.
- 2026-02-26: Verified both default build (`build`) and SDL2-enabled build (`build_sdl2`) compile after event seam wiring.
- 2026-02-26: Added `platform_window` lifecycle seam (`init`, `set_text_mode`, `shutdown`) and routed `main.cpp` startup/shutdown text-mode transitions through it.
- 2026-02-26: Adjusted SDL event bridge safety: do not initialize SDL events during Allegro-owned windowing, and only pump SDL events when SDL video is initialized to avoid backend contention/regressions.
- 2026-02-26: Extended `platform` timing abstraction with timer install/callback/remove and sleep helpers, and routed `main.cpp` frame-timer/sleep callsites through these wrappers.
- 2026-02-26: Added `PLATFORM_WINDOW_set_windowed_mode(width,height,depth)` and routed `OUTPUT_setup_project_list()` display mode setup through `platform_window`.
- 2026-02-26: Added `PLATFORM_WINDOW_begin_text_screen()/end_text_screen()` and routed both `OUTPUT_setup_project_list()` and `OUTPUT_setup_running_mode()` text screen prep through `platform_window`.
- 2026-02-26: Added `PLATFORM_WINDOW_set_software_game_mode()` and `PLATFORM_WINDOW_set_opengl_game_mode()` and routed `OUTPUT_setup_allegro()` display mode selection through `platform_window`.
- 2026-02-26: Added `platform_renderer` seam and moved AllegroGL lifecycle configuration from `OUTPUT_setup_allegro()` into `PLATFORM_RENDERER_prepare_allegro_gl()`.
- 2026-02-26: Added `PLATFORM_RENDERER_shutdown()` hook in `OUTPUT_shutdown()` for incremental renderer backend teardown migration.
- 2026-02-26: Moved fixed-function GL default bootstrap (texture/scissor state, viewport, orthographic projection setup, projection matrix capture) into `PLATFORM_RENDERER_apply_gl_defaults()`.
- 2026-02-26: Added renderer capability seam via `platform_renderer_caps` and `PLATFORM_RENDERER_build_caps()`, moving capability decisions and proc-address wiring out of `OUTPUT_setup_allegro()`.
- 2026-02-26: Moved Windows game-window centering out of `output.cpp` into `PLATFORM_WINDOW_center_game_window()`.
- 2026-02-26: Moved OpenGL version/extension string parsing into `PLATFORM_RENDERER_query_extensions()`; `OUTPUT_get_opengl_extensions()` now acts as a thin data owner/adapter.
- 2026-02-26: Completed extension lookup centralization: `platform_renderer` now owns extension cache/query APIs (`is_extension_supported`, version/extension getters, clear), and `output.cpp` no longer maintains local extension-list state.
- 2026-02-26: Removed legacy `OUTPUT` extension wrapper layer; setup and teardown now call `platform_renderer` extension APIs directly (`query`, `is_extension_supported`, `clear`).
- 2026-02-26: Added SDL2 renderer bootstrap stub APIs to `platform_renderer` (`prepare_sdl2_stub`, `is_sdl2_stub_ready`) behind `WIZBALL_USE_SDL2`, plus centralized SDL-side cleanup path in renderer shutdown.
- 2026-02-26: Added opt-in SDL2 per-frame mirror diagnostics in `platform_renderer` (`PLATFORM_RENDERER_mirror_from_current_backbuffer`) and wired it at the single present callsite in `OUTPUT_updatescreen()`; controlled by `WIZBALL_SDL2_STUB_BOOTSTRAP=1` plus `WIZBALL_SDL2_STUB_MIRROR=1`.
- 2026-02-26: Cleaned redundant `output.h` extern declarations (removed duplicate `texture_combiner_available` declaration).
- 2026-02-26: Wired SDL2 renderer stub seam into `OUTPUT_setup_allegro()` under `WIZBALL_USE_SDL2` with startup logging only (no SDL renderer/window creation yet).
- 2026-02-26: Implemented opt-in real SDL2 stub bootstrap in `PLATFORM_RENDERER_prepare_sdl2_stub()` (hidden SDL window + renderer creation, accelerated then software fallback), gated by env `WIZBALL_SDL2_STUB_BOOTSTRAP=1`; Allegro remains active render path.
- 2026-02-26: Added SDL2 stub diagnostics APIs (`is_sdl2_stub_enabled`, `get_sdl2_stub_status`) and startup log line now records enabled/ready/status in one message.
- 2026-02-26: Added SDL2 stub self-test (`PLATFORM_RENDERER_run_sdl2_stub_self_test`) that performs one clear/present on hidden SDL renderer and reports pass/fail in startup diagnostics.
- 2026-02-26: Added `WIZBALL_SDL2_STUB_SHOW=1` option to create a visible SDL stub window for live mirror diagnostics; default remains hidden.
- 2026-02-26: Moved frame clear/present GL lifecycle calls behind renderer seam (`PLATFORM_RENDERER_clear_backbuffer`, `PLATFORM_RENDERER_present_frame`) and routed `OUTPUT_clear_screen()` / `OUTPUT_updatescreen()` through them.
- 2026-02-26: Hardened SDL stub on X11/GLX by defaulting stub renderer to software mode to avoid AllegroGL context contention; added opt-in `WIZBALL_SDL2_STUB_ACCELERATED=1` for explicit accelerated SDL renderer testing.
- 2026-02-26: Changed SDL stub bootstrap to lazy-init from `PLATFORM_RENDERER_present_frame()` (after AllegroGL mode is established) instead of startup-time init in `OUTPUT_setup_allegro()`, to reduce GLX context contention during boot.
- 2026-02-26: Moved primitive immediate-mode GL draw paths (outline rect, filled rect, line, circle) from `output.cpp` into `platform_renderer` helpers and routed `OUTPUT_rectangle`, `OUTPUT_filled_rectangle`, `OUTPUT_line`, and `OUTPUT_circle` through the renderer seam.
- 2026-02-26: Added shared textured-quad helper `PLATFORM_RENDERER_draw_textured_quad()` and routed `OUTPUT_draw_sprite`, `OUTPUT_draw_masked_sprite`, `OUTPUT_draw_bitmap`, `OUTPUT_draw_sprite_scale`, and `OUTPUT_draw_sprite_scale_no_pivot` through it to reduce direct GL state/draw code in `output.cpp`.
- 2026-02-26: Routed `OUTPUT_text` GL glyph quads through the same `PLATFORM_RENDERER_draw_textured_quad()` helper (with legacy text colour scaling preserved), further reducing per-callsite GL state duplication.
- 2026-02-26: Added lower-level quad emission helper `PLATFORM_RENDERER_draw_bound_textured_quad()` and routed tilemap/tilemap-line inner loops in `OUTPUT_draw_window_contents()` through it, reducing duplicated immediate-mode quad blocks in high-frequency map drawing paths.
- 2026-02-26: Added `PLATFORM_RENDERER_draw_bound_textured_quad_custom()` and routed plain `DRAW_MODE_SPRITE` rotate/non-rotate quad emission (including arbitrary-quad variants without per-vertex colour) through renderer seam helpers.
- 2026-02-26: Added `PLATFORM_RENDERER_draw_bound_solid_quad()` and routed `DRAW_MODE_SOLID_RECTANGLE` quad emission through the renderer seam.
- 2026-02-26: Added array-based quad helpers for multitexture and per-vertex-colour textured emission, and routed corresponding non-perspective `DRAW_MODE_SPRITE` paths through `platform_renderer`.
- 2026-02-26: Added perspective quad helpers (coloured and non-coloured) and routed remaining `DRAW_MODE_SPRITE` perspective quad emission through `platform_renderer`; `output.cpp` now has no direct `glBegin(GL_QUADS)` callsites.
- 2026-02-26: Added point/line/line-loop helpers in `platform_renderer` and routed starfield rendering plus collision-debug grid/shape line rendering through them; `output.cpp` now has no direct `glBegin/glEnd` callsites.
- 2026-02-26: Added `PLATFORM_RENDERER_set_window_transform()` and routed repeated `glLoadIdentity + glTranslatef(left_window_transform_x, top_window_transform_y) + glScalef(total_scale)` setup sequences in `output.cpp` through the renderer seam.
- 2026-02-26: Added common renderer-state wrappers (`set_colour3f/4f`, `set_texture_enabled`, `set_colour_sum_enabled`, `bind_texture`, `set_texture_filter`, `set_texture_wrap_repeat`) and routed straightforward `output.cpp` state callsites through `platform_renderer`; remaining direct GL state in `output.cpp` is now mostly specialized texture-combiner (`glTexEnvi`) logic.
- 2026-02-26: Added renderer combiner preset helpers (`set_combiner_modulate_primary`, `set_combiner_replace_rgb_modulate_alpha_previous`, `set_combiner_replace_rgb_modulate_alpha_primary`, `set_combiner_modulate_rgb_replace_alpha_previous`) and routed all `output.cpp` `glTexEnvi` combiner setup/restore callsites through `platform_renderer`.
- 2026-02-26: Fixed SDL/GL state regression causing black gameplay screen by restoring explicit texture-unit-0 activation at draw begin/end and using `old_secondary_bitmap_number` for end-of-frame multitexture cleanup.
- 2026-02-26: Added renderer wrappers for blend/alpha/scissor state (`set_blend_enabled`, `set_blend_func`, `set_alpha_test_enabled`, `set_alpha_func_greater`, `set_scissor_rect`) and routed corresponding `output.cpp` state callsites through `platform_renderer`.
- 2026-02-26: Added transform wrappers (`translatef`, `scalef`, `rotatef`) in `platform_renderer` and routed all direct `glTranslatef/glScalef/glRotatef` callsites in `output.cpp` through the renderer seam.
- 2026-02-26: Added renderer-owned active texture unit seam (`set_active_texture_proc`, `set_active_texture_unit`) and routed `output.cpp` texture-unit switching through `platform_renderer`.
- 2026-02-26: Removed remaining renderer-proc globals from `output.cpp` (`pglSecondaryColor3fEXT`, `pglActiveTextureARB`), replacing them with `platform_renderer` capability helpers (`is_active_texture_available`) and centralizing OpenGL vendor/renderer/version string ownership via `platform_renderer` getters.
- 2026-02-26: Added semantic renderer helpers for texture-unit selection and blend presets (`set_active_texture_unit0/1`, `set_blend_mode_additive/alpha/subtractive`) and removed remaining raw OpenGL constants from `output.cpp` render flow.
- 2026-02-26: Removed residual OpenGL-type leakage in `output.cpp` by dropping `glext.h` include usage and replacing `GLfloat ProjectionMatrix` with a renderer-agnostic `float projection_matrix[16]`.
- 2026-02-26: Moved direct AllegroGL calls out of `output.cpp` by adding renderer helpers for texture upload (`create_masked_texture`) and AllegroGL error string access (`get_allegro_gl_error_text`), and switched bitmap texture handle storage to renderer-agnostic `unsigned int`.
- 2026-02-26: Removed global OpenGL version state from `output.cpp` setup path and added `PLATFORM_RENDERER_build_caps_for_current_context(...)` so extension-name capability checks are now centralized in `platform_renderer`.
- 2026-02-26: Moved OpenGL extension dump file generation from `output.cpp` into `platform_renderer` via `PLATFORM_RENDERER_write_extensions_to_file(...)`, further reducing renderer-debug ownership in game-layer setup code.
- 2026-02-26: Simplified renderer bootstrap seam by removing unused projection-matrix capture plumbing from `PLATFORM_RENDERER_apply_gl_defaults(...)`; `output.cpp` no longer passes internal renderer state buffers during setup.
- 2026-02-26: Added `PLATFORM_RENDERER_query_and_build_caps(...)` to centralize extension-query and capability derivation sequencing in renderer code, and reordered setup logging in `output.cpp` so vendor/renderer/version lines are emitted after capability query populates renderer metadata.
- 2026-02-26: Fixed a multitexture flag-check precedence bug in `OUTPUT_draw_window_contents` (`(flags & mask > 0)` -> `((flags & mask) > 0)`) that could intermittently drive incorrect secondary texture-unit state and contribute to occasional black gameplay frames.
- 2026-02-26: Hardened renderer capability gating so multitexture and secondary-colour features are only reported available when their required proc pointers are present (`glActiveTextureARB`, `glSecondaryColor3fEXT`), reducing risk of invalid state transitions on partially-capable GL implementations.
- 2026-02-26: Added `PLATFORM_RENDERER_apply_caps(...)` and routed `OUTPUT_setup_allegro()` through it, so renderer proc wiring (secondary colour / active texture) is now centralized in `platform_renderer` rather than duplicated in game setup code.
- 2026-02-26: Centralized textured-window render lifecycle state in `platform_renderer` (`PLATFORM_RENDERER_prepare_textured_window_draw` / `PLATFORM_RENDERER_finish_textured_window_draw`) and replaced duplicated begin/end state reset logic in `OUTPUT_draw_window_contents`, reducing drift risk in texture-unit/combiner cleanup that can cause intermittent black gameplay frames.
- 2026-02-26: Hardened frame-begin texture state normalization in `PLATFORM_RENDERER_prepare_textured_window_draw(...)` by always forcing active texture unit 0 when available and resetting combiner baseline to modulate-primary when texture-combiner mode is enabled.
- 2026-02-26: Added `OUTPUT_has_multitexture_flags(...)` helper and routed multitexture gating callsites through it to prevent precedence drift in bitmask checks and keep gameplay texture-unit decisions consistent.
- 2026-02-26: Added defensive frame-begin secondary-unit reset in `PLATFORM_RENDERER_prepare_textured_window_draw(...)` (force unit1 selected + texturing disabled before returning to unit0) to reduce intermittent stale multitexture state causing black gameplay output.
- 2026-02-26: Added renderer-owned texture-parameter transition helper `PLATFORM_RENDERER_apply_texture_parameters(...)` and reduced `OUTPUT_set_texture_parameters(...)` to a thin adapter, consolidating filter/wrap state handling in `platform_renderer`.
- 2026-02-26: Removed remaining direct `set_texture_filter` callsites from `OUTPUT_draw_window_contents` hot path and routed them through `OUTPUT_set_texture_parameters(...)`/`platform_renderer` helpers for a single texture-parameter decision path.
- 2026-02-26: Renamed/privatized texture-parameter adapter in `output.cpp` to `OUTPUT_apply_texture_parameters_from_flags(...)` (static) and updated all callsites, clarifying local intent while keeping renderer-owned parameter application centralized.
- 2026-02-26: Fixed sprite hot-path texture-parameter application on bitmap switches by adding `force_filter_apply` support in `PLATFORM_RENDERER_apply_texture_parameters(...)` and using `bitmap_changed` in `output.cpp`, ensuring per-texture filtering state is re-applied when binding a different texture object.
- 2026-02-26: Added renderer helpers for multitexture second-unit sequencing (`PLATFORM_RENDERER_prepare_multitexture_second_unit` / `PLATFORM_RENDERER_finalize_multitexture_second_unit`) and routed `OUTPUT_draw_window_contents` combiner/unit1 setup-finalize branches through them to reduce state-order duplication.
- 2026-02-26: Added `PLATFORM_RENDERER_set_window_scissor(...)` to centralize signed width/height scissor normalization and replaced duplicated branchy scissor math in both gameplay and collision window draw paths.
- 2026-02-26: Reused `PLATFORM_RENDERER_finish_textured_window_draw(...)` in the collision-debug draw path, so both gameplay and collision window rendering now share the same end-of-window state cleanup routine.
- 2026-02-26: Added renderer helpers for secondary texture-unit bind/disable transitions (`PLATFORM_RENDERER_bind_secondary_texture` / `PLATFORM_RENDERER_disable_secondary_texture_and_restore_combiner`) and routed corresponding `OUTPUT_draw_window_contents` branches through them.
- 2026-02-26: Added local helper `OUTPUT_configure_secondary_multitexture_state(...)` to collapse duplicated secondary-unit combiner/parameter setup branches in sprite multitexture draw flow, reducing branch duplication in the gameplay hot path.
- 2026-02-26: Added local multitexture mode predicates (`OUTPUT_is_double_multitexture_mode`, `OUTPUT_is_secondary_multitexture_active`) and replaced repeated inline conditions in sprite render flow to reduce boolean-logic duplication and precedence risk.
- 2026-02-26: Kept direct `PLATFORM_RENDERER_set_window_transform(...)` callsites in `output.cpp` (removed temporary local wrapper), preserving straightforward call flow while retaining prior transform-reset cleanup.
- 2026-02-26: Introduced renderer-owned texture handle registry in `platform_renderer` (handle->GL id mapping), so `create_masked_texture` now returns renderer-managed handles and bind/draw paths resolve handles internally; added registry cleanup in renderer shutdown.
- 2026-02-26: Added explicit `PLATFORM_RENDERER_reset_texture_registry()` API and called it from `OUTPUT_destroy_bitmap_and_sprite_containers()` so renderer texture-handle bookkeeping is reset on bitmap-container teardown, not only at full renderer shutdown.
- 2026-02-26: Simplified texture-registry cleanup implementation to a single public function (`PLATFORM_RENDERER_reset_texture_registry`) and removed the redundant private clear helper.
- 2026-02-26: Tightened texture-handle safety by removing raw-id fallback in `PLATFORM_RENDERER_resolve_gl_texture`; invalid/stale handles now resolve to 0 instead of silently binding unintended GL texture ids.
- 2026-02-26: Aligned renderer API naming around texture handles (`texture_handle` params for bind/secondary-bind/textured-quad helpers) to reflect renderer-owned handle semantics and reduce accidental raw-GL-id assumptions ahead of SDL texture backend work.
- 2026-02-26: Consolidated SDL stub env-flag parsing into a shared renderer helper and updated prepare/enable/mirror paths to use it, removing duplicated local env-check implementations.
- 2026-02-26: Added first real SDL2 sprite-render path in `platform_renderer`: texture registry entries now optionally retain RGBA pixel payloads, lazily build `SDL_Texture` objects, and `PLATFORM_RENDERER_draw_textured_quad(...)` can render through SDL when `WIZBALL_SDL2_NATIVE_SPRITES=1` (with stub window active). `clear_backbuffer/present_frame` now also clear/present the SDL renderer in that mode.
- 2026-02-26: Added startup diagnostics for native SDL sprite mode (`native_sprites=0/1`) in `OUTPUT_setup_allegro()` log output.
- 2026-02-26: Added dedicated SDL-native gameplay-window sprite helper (`PLATFORM_RENDERER_draw_sdl_window_sprite`) that applies window transform coordinates into SDL space and draws from renderer texture handles.
- 2026-02-26: Routed common `DRAW_MODE_SPRITE` path in `OUTPUT_draw_window_contents()` to emit SDL-native sprites for the non-rotated/non-scaled/non-multitexture/non-arbitrary-quad case, while preserving existing OpenGL rendering path for parity and fallback.
- 2026-02-26: Expanded `PLATFORM_RENDERER_draw_sdl_window_sprite(...)` to support sprite-local scale, rotation (`SDL_RenderCopyEx`), UV-order flips, and explicit horizontal/vertical flip flags while preserving renderer-handle texture lookups.
- 2026-02-26: Expanded SDL-native gameplay sprite candidate path in `OUTPUT_draw_window_contents()` to include primary rotate/scale, clip-frame, and flip variants (still excluding multitexture, arbitrary quads/perspective quads, and individual-vertex-colour alpha), increasing real SDL coverage during gameplay.
- 2026-02-26: Added SDL-native tile emission in `DRAW_MODE_TILEMAP` and `DRAW_MODE_TILEMAP_LINE` loops, mapping tile positions into gameplay-window SDL coordinates and drawing per-tile sprite UVs through `PLATFORM_RENDERER_draw_sdl_window_sprite(...)` while preserving existing OpenGL draws for parity/fallback.
- 2026-02-26: Added non-SDL build fallback for `PLATFORM_RENDERER_is_sdl2_native_sprite_enabled()` so default (non-`WIZBALL_USE_SDL2`) builds continue linking cleanly.
- 2026-02-26: Added SDL-native primary-texture fallback draw inside the multitexture sprite branch (`OUTPUT_is_secondary_multitexture_active` path) for the non-arbitrary/non-secondary-transform subset, increasing SDL-visible gameplay coverage before full multitexture emulation.
- 2026-02-26: Added SDL-native solid-rectangle helper (`PLATFORM_RENDERER_draw_sdl_window_solid_rect`) and routed `DRAW_MODE_SOLID_RECTANGLE` through it for non-rotated cases (including scale), while retaining GL draw path for parity.
- 2026-02-26: Added renderer-side 2D transform/color tracking in `platform_renderer` (`set_window_transform`, `translatef`, `scalef`, `rotatef`, colour setters) and used it to emit SDL-native primitive draws for points/lines/line-loops/solid quads, increasing native coverage for starfield/line-driven gameplay effects while preserving GL paths.
- 2026-02-26: Added SDL-native arbitrary textured quad helper (`PLATFORM_RENDERER_draw_sdl_bound_textured_quad_custom`) using `SDL_RenderGeometry` (when available) and wired it into non-perspective arbitrary-quad sprite branches (both non-multitexture and multitexture-primary fallback paths) to reduce remaining GL-only gameplay effects.
- 2026-02-26: Added SDL-native rendering for coloured textured quad-array path (`PLATFORM_RENDERER_draw_bound_coloured_textured_quad_array`) via `SDL_RenderGeometry` using renderer-bound texture tracking, improving coverage for individual-vertex-colour-alpha sprite effects.
- 2026-02-26: Added SDL-native fallback in `PLATFORM_RENDERER_draw_bound_multitextured_quad_array` (primary layer `u0/v0` rendered via `SDL_RenderGeometry` with bound texture tracking), improving multitexture-effect visibility on SDL before full two-layer combiner parity.
- 2026-02-26: Centralized additional SDL textured-quad coverage into renderer primitives by adding `SDL_RenderGeometry` fallback to `PLATFORM_RENDERER_draw_bound_textured_quad` and `PLATFORM_RENDERER_draw_bound_textured_quad_custom`, then removed redundant output-layer SDL duplicate draw injections from sprite/tilemap/solid-rectangle branches to avoid double-emission drift.
- 2026-02-26: Added `WIZBALL_SDL2_NATIVE_PRIMARY=1` mode in `platform_renderer` (`PLATFORM_RENDERER_is_sdl2_native_primary_enabled`) to run SDL-native presentation without GL mirror fallback and skip `allegro_gl_flip()` for frame present, enabling explicit testing of SDL-as-primary output. Startup diagnostics now include `native_primary=0/1`.
- 2026-02-26: Hardened native-primary experimentation by making newly-added core SDL quad-geometry fallback opt-in (`WIZBALL_SDL2_CORE_QUADS=1`) so default SDL-native test runs avoid unstable overdraw/garbage regressions while coverage work continues.
- 2026-02-26: Added SDL-native fallback rendering for perspective textured quad helpers (`PLATFORM_RENDERER_draw_bound_perspective_textured_quad` and coloured variant) via `SDL_RenderGeometry`, reducing remaining GL-only gameplay effects in strict primary runs.
- 2026-02-26: Restored strict `WIZBALL_SDL2_NATIVE_PRIMARY=1` semantics in frame present: strict mode now skips AllegroGL flip unless mirror fallback was actually used, while compat/non-strict modes continue mirroring for stability.
- 2026-02-26: Removed `WIZBALL_SDL2_CORE_QUADS` gating from core textured-quad SDL fallback (`PLATFORM_RENDERER_draw_bound_textured_quad`), so strict-primary gameplay paths render without requiring optional core-quad diagnostics flags.
- 2026-02-26: Improved strict-primary fallback gating by tracking textured native draws separately from primitive draws; strict mode mirror fallback now keys off textured coverage (`textured_draw_count`) to avoid false-positive “coverage complete” transitions that can produce flash-then-black output.
- 2026-02-26: Added `WIZBALL_SDL2_GEOMETRY` gate (default off) for all `SDL_RenderGeometry` paths; strict-primary now prefers safer `RenderCopy/RenderCopyEx` textured draws unless geometry is explicitly enabled for targeted parity testing.
- 2026-02-26: Added strict-primary safety override `WIZBALL_SDL2_NATIVE_PRIMARY_FORCE_NO_MIRROR=1`. By default, strict mode now keeps mirrored fallback active to avoid freeze/garbage transitions while native coverage remains incomplete; force flag enables explicit no-mirror strict testing.
- 2026-02-26: Added non-geometry SDL fallbacks to custom textured-quad paths (`PLATFORM_RENDERER_draw_bound_textured_quad_custom` and `PLATFORM_RENDERER_draw_sdl_bound_textured_quad_custom`) by detecting axis-aligned transformed quads and using `SDL_RenderCopyEx`; `SDL_RenderGeometry` is now only used for non-axis-aligned cases when `WIZBALL_SDL2_GEOMETRY=1`.
- 2026-02-26: Added axis-aligned `SDL_RenderCopyEx` fallbacks for quad-array helpers: `PLATFORM_RENDERER_draw_bound_multitextured_quad_array` (primary-layer fallback) and `PLATFORM_RENDERER_draw_bound_coloured_textured_quad_array` (uniform-colour subset). Non-axis-aligned or per-vertex-colour-gradient cases still route to `SDL_RenderGeometry` when `WIZBALL_SDL2_GEOMETRY=1`.
- 2026-02-26: Added axis-aligned `SDL_RenderCopyEx` fallbacks for perspective helper paths: `PLATFORM_RENDERER_draw_bound_perspective_textured_quad` and coloured variant (uniform-colour subset). True non-axis-aligned perspective cases remain on `SDL_RenderGeometry` and are gated by `WIZBALL_SDL2_GEOMETRY=1`.
- 2026-02-26: Added `WIZBALL_SDL2_NATIVE_PRIMARY_AUTO_NO_MIRROR=1` mode: strict-primary now tracks a native-coverage streak (`draws>=128` and `textured>=16`) and only transitions away from mirror fallback after sustained coverage (120 consecutive frames). Existing `...FORCE_NO_MIRROR=1` remains available for hard no-mirror probes.
- 2026-02-26: Hardened `AUTO_NO_MIRROR` gating with per-frame geometry-miss tracking: any draw path that would require `SDL_RenderGeometry` when geometry is disabled increments `geom_miss`, and strict auto mode now refuses no-mirror transition until `geom_miss==0` across sustained-coverage frames.
- 2026-02-26: Added no-mirror transition cooldown (`180` frames) after any geometry miss/failure; `AUTO_NO_MIRROR` now requires both sustained coverage and cooldown expiry before re-attempting no-mirror, reducing transition oscillation/freezes during gameplay/menu state changes.
- 2026-02-26: Made `AUTO_NO_MIRROR` conservative-by-design: qualification streak raised to 1800 frames, and any geometry miss now latches a session-level block (`blocked=1`) that prevents further auto no-mirror attempts in that run. This keeps sessions stable while unresolved no-geometry coverage gaps remain.
- 2026-02-26: Added coarse non-geometry fallback inside `PLATFORM_RENDERER_try_sdl_geometry_textured(...)`: when geometry is disabled (or geometry render fails), renderer now attempts a bounding-quad `SDL_RenderCopyEx` approximation before counting a geometry miss. This reduces black/corruption risk and lowers `geom_miss` pressure while full non-geometry parity is still incomplete.
- 2026-02-26: Marked approximation renders as `degraded` coverage and excluded them from no-mirror qualification (`coverage_good` now requires `degraded==0`). This prevents `AUTO_NO_MIRROR` from switching on frames relying on coarse geometry fallback approximations.
- 2026-02-26: Added per-callsite geometry diagnostics (`hot_src=id(count)`) by tracking miss/degraded counters per geometry-origin draw path. Status text now surfaces the dominant source each frame to drive targeted elimination of remaining no-mirror blockers.
- 2026-02-26: Improved coarse geometry fallback quality: before bounding-box blit, fallback now attempts a rotated-rectangle `SDL_RenderCopyEx` approximation when quad corners match an affine rectangle shape (dominant `hot_src=2` case). This reduces corruption risk for `BOUND_QUAD_CUSTOM` heavy frames.
- 2026-02-26: Applied rotated-rectangle `RenderCopyEx` fallback directly in `draw_bound_textured_quad_custom` and `draw_sdl_bound_textured_quad_custom` before geometry-helper fallback, specifically to reduce `hot_src=2` degradation pressure in gameplay/menu transitions.
- 2026-02-26: Expanded direct rotated-rectangle `RenderCopyEx` coverage in `draw_bound_textured_quad` (source `hot_src=1`) to attempt non-axis-aligned draws whenever width/height are valid, reducing dependency on shared degraded geometry fallback during menu/gameplay transitions.
- 2026-02-26: Expanded direct rotated-rectangle `RenderCopyEx` coverage in perspective quad helpers (`draw_bound_perspective_textured_quad` and `draw_bound_coloured_perspective_textured_quad`) to try direct SDL draws for non-axis-aligned quads whenever width/height are valid, reducing `hot_src=6`/perspective degradation pressure before geometry fallback.
- 2026-02-26: Switched strict auto primary transition to implementation-first behavior in `present_frame`: `WIZBALL_SDL2_NATIVE_PRIMARY_AUTO_NO_MIRROR=1` now allows no-mirror as soon as frame-level fallback health is good (`geom_miss==0` and degraded policy satisfied), removing dependency on draw-count/streak qualification gates.
- 2026-02-26: Hardened renderer transform output for SDL paths by sanitizing `PLATFORM_RENDERER_transform_point(...)` results (fallback to input coordinates when non-finite or absurdly large), reducing `hot_src=1` miss bursts caused by invalid transformed coordinates poisoning non-axis quad draws.
- 2026-02-26: Fixed a core coordinate-space bug in rotated SDL textured-quad paths (`draw_bound_textured_quad`, custom, perspective, coloured perspective, and SDL-custom variants): rotation/corner math now uses SDL-space vertex positions instead of GL-space transformed `y`, preventing off-screen/garbled output from mixed origin conventions.
- 2026-02-26: Stabilized strict-auto primary transition behavior by keeping `allegro_gl_flip()` active when `WIZBALL_SDL2_NATIVE_PRIMARY_AUTO_NO_MIRROR=1` (unless force-no-mirror is set), so mirror fallback re-entry samples fresh GL frames instead of stale buffer content that produced frozen/garbled output.
- 2026-02-26: Switched SDL geometry default to implementation-first for strict native-primary runs: if `WIZBALL_SDL2_GEOMETRY` is unset, geometry is now enabled automatically when `WIZBALL_SDL2_NATIVE_PRIMARY=1` and strict mode is on; explicit `WIZBALL_SDL2_GEOMETRY=0/1` still overrides. This prioritizes true quad fidelity over approximation in the SDL-primary path.
- 2026-02-26: Implemented first SDL-side multitexture composition in `draw_bound_multitextured_quad_array`: renderer now tracks last bound secondary texture (unit1) and draws both layers in SDL (axis-aligned and geometry paths) using `u0/v0` + `u1/v1`, with blend mode keyed from the current double-mask multitexture mode. This targets stub output that had correct shapes but incorrect layer composition.
- 2026-02-26: Corrected SDL multitexture composition scope: secondary-layer SDL overlay is now limited to `double_mask` mode only. Non-double multitexture mode no longer overlays secondary RGB (GL uses secondary primarily for alpha/combine semantics there), avoiding severe “wrong texture/image parts” artifacts in SDL stub output.
- 2026-02-26: Attempted geometry-flip parity correction in `draw_sdl_window_sprite` (folding destination sign flips into SDL flip flags), but this regressed main gameplay rendering (black gameplay area) and was rolled back.
- 2026-02-26: Added centralized GL->SDL UV normalization helper (`build_safe_sdl_src_rect_from_gl_uv`) that maps GL-style `v` coordinates to SDL texture-space (`v' = 1 - v`) and derives flip parity from normalized UVs. Wired into `draw_sdl_window_sprite` and `draw_textured_quad` as first high-impact callsites to address persistent vertical/mirroring and wrong-region sampling artifacts in SDL stub output.
- 2026-02-26: Propagated GL->SDL UV normalization helper across core textured SDL paths (bound/custom/perspective/coloured/multitexture + SDL custom), including secondary-layer multitexture sampling. Removed remaining direct `build_safe_sdl_src_rect(...)` usage in render paths so UV origin/flip handling is consistent project-wide.
- 2026-02-26: Improved non-double multitexture effect parity in SDL by adding a custom blend mode for secondary-layer composition (`RGB keep dst, Alpha = dstAlpha * srcAlpha`) and applying it to unit1 overlay passes. Double-mask mode continues to use `SDL_BLENDMODE_MOD`. This targets missing/mismatched effect layers while avoiding the earlier wrong-region RGB overlay artifacts.
- 2026-02-26: Replaced non-double custom alpha-only secondary blend with `SDL_BLENDMODE_MOD` in SDL multitexture overlay passes. This is a pragmatic parity step to restore missing HUD/effect visibility (top header/footer) until a shader/offscreen combiner path is introduced.
- 2026-02-26: Attempted non-double custom secondary blend (`modulate RGB, keep previous alpha`) via `SDL_ComposeCustomBlendMode(...)`, but this regressed title/menu rendering (black screens + bottom-left collapse) and was rolled back to the previous `SDL_BLENDMODE_MOD` overlay behavior.
- 2026-02-26: Added offscreen SDL composition path for axis-aligned multitexture quads (`draw_bound_multitextured_quad_array`): primary and secondary layers are now composed into a scratch render-target texture and then blitted to the final destination rect. This isolates blend semantics/state from the global frame target and is aimed at improving stub effect parity in HUD/title/menu paths without destabilizing main output.
- 2026-02-26: Normalized GL->SDL UV origin for geometry-based textured paths (`SDL_RenderGeometry`) by converting vertex `tex_coord.y` to `1-v` across bound/custom/perspective/coloured/multitexture (including multitexture secondary geometry overlay). This aligns geometry UV sampling with already-corrected axis-aligned `RenderCopyEx` paths and targets remaining effect parity mismatches.
- 2026-02-26: Added first-pass SDL emulation for `GL_COLOR_SUM` / secondary colour in native sprite paths: renderer now tracks colour-sum enabled state + secondary RGB, and applies an additive secondary-colour overlay pass (`SDL_BLENDMODE_ADD`) after primary sprite draws in `draw_textured_quad` and `draw_sdl_window_sprite`. This targets remaining effect mismatch where base imagery is correct but GL effect overlays are absent in stub.
- 2026-02-26: The first-pass `GL_COLOR_SUM` additive overlay emulation regressed placement/render correctness (black/left-clamped title+HUD paths) and was rolled back; baseline restored for further effect-parity work via narrower composition paths.
- 2026-02-26: Added targeted runtime instrumentation in `OUTPUT_draw_window_contents` behind `WIZBALL_SDL2_EFFECT_TRACE=1`: per-window/frame summaries now report active entity feature counts (sprite, multitexture, double-mask, secondary-colour, arbitrary/perspective, vertex-colour-alpha, blend modes). This is to isolate the exact branch combinations active when stub effects diverge while base imagery is correct.
