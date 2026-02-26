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
- [ ] Window / context lifecycle
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

## Change Log

- 2026-02-26: Tracker created.
- 2026-02-26: Added preserved build/repack command reference.
- 2026-02-26: Added `WIZBALL_ENABLE_SDL2` CMake option + SDL2 link/discovery path.
- 2026-02-26: Added initial platform abstraction files (`platform.h/.cpp`) and routed wall-clock timing through `PLATFORM_get_wall_time_ms()`.
- 2026-02-26: Verified both default build (`build`) and SDL2-enabled build (`build_sdl2`) compile.
- 2026-02-26: Added `platform_input.h/.cpp` abstraction seam and routed keyboard/mouse polling in `control.cpp` through platform input helpers.
- 2026-02-26: Extended `platform_input` seam to joystick install/calibration/count/state/axis polling and routed `control.cpp` joystick paths through shared helpers.
- 2026-02-26: Verified both default build (`build`) and SDL2-enabled build (`build_sdl2`) compile after joystick abstraction pass.
