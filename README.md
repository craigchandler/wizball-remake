# WizBall (Retrospec remake source archive)

A preserved source release of the 2007 *Wizball* PC/Mac remake by Graham Goring and collaborators, now being brought back to life for modern Linux (and eventually PortMaster handhelds).

## Why this repository exists

In February 2026, I reached out to Graham Goring to ask whether the source still existed so I could:

- get the remake running cleanly on Linux
- prepare a PortMaster-friendly build for handheld devices

At first, the code appeared to be lost. Graham did "have a shufty on a few dusty old drives" and came up empty, but as a last-ditch resort he emailed the Mac porter. Then, in Graham's words: "happy days" - Peter Hull still had a copy (with some Mac-specific bits).

This repository is that recovered codebase.

## A short history

- **1987**: The original *Wizball* (Sensible Software) released, designed by Jon Hare and Chris Yates.
- **Early 2006 -> 2007**: Graham Goring and team built the Retrospec/Smila remake for Windows and Mac.
- **2007 onward**: The remake shipped publicly and kept the spirit of the C64 version while modernizing controls and presentation.
- **2026**: Source recovered from backups and revived for Linux/PortMaster work.

Relevant original project page:
- https://retrospec.sgn.net/info.htm?id=wizball&t=g

Background on the original game:
- https://en.wikipedia.org/wiki/Wizball

## Credits

### Original 1987 game

- **Design**: Jon Hare, Chris Yates
- **Programming**: Chris Yates
- **Graphics**: Jon Hare
- **Music**: Martin Galway
- **Cover illustration**: Bob Wakelin

### 2007 Retrospec remake

- **Remake programming**: Graham Goring
- **Remake graphics**: Trevor "Smila" Storey
- **Remake music/arrangements**: Infamous (Chris Nunn)
- **Mac conversion**: Peter Hull
- **Linux conversion (original effort)**: Scott Wightman
- **Linux revival (2026)**: Craig Chandler

### Additional acknowledgements from the original remake readme

- Peter Hanratty (beta testing)
- Muttley (joypad feedback and bug reports)
- Richard Jordan
- Neil Walker
- Tomaz Kac
- Marticus (Wizball Shrine / gameplay reference)
- Carleton Handley
- Tim W
- Oddbob
- Paul Pridham
- Retro Remakes community
- TIGforums community

If I've missed anyone, open an issue or PR and I'll fix it.

## Status

- Tested on modern Arch Linux.

## Project goals (2026 revival)

- preserve and document the recovered source
- keep original attribution intact
- maintain playable Linux builds
- package for PortMaster without changing the core feel of the remake
- Not yet tested on Windows or macOS (TODO).
- Publish release packages/installable builds (TODO).

## 2026 Bring-Up Changes (Summary)

The recovered codebase was not originally in a state that built and ran cleanly on a modern Linux desktop toolchain. Notable 2026 revival changes:

- Build system: added a CMake-based build and Linux compile path, while keeping the original C++98-era constraints.
- Audio: migrated off the legacy FMOD path onto the SDL2 audio stack, using `SDL_mixer` for sound effects and streamed audio.
- Frame pacing/input: stabilized the main loop so simulation/input handling is less coupled to render refresh rate, and added optional safeguards for timer/refresh stalls during compositor/display transitions.
- Data/asset loading: hardened resource loading for Linux (case sensitivity and path assumptions) and added runtime diagnostics/validation to fail fast on bad or partial loads.
- Safety hardening: added guard rails around collision/map/tile-set handling to avoid crashes from malformed or missing data during bring-up.
- Packaging workflow: moved the active Linux/PortMaster path to direct on-disk assets and stopped tracking generated runtime artifacts in git.

## Build (Linux)

### Dependencies

The current build is SDL2-based and uses `pkg-config` for dependency discovery.

- C++ compiler with C++11 support
- CMake 3.16+
- pkg-config
- SDL2
- SDL2_mixer
- SDL2_image

### Build

Default desktop build:

```bash
cmake -S . -B build -DWIZBALL_PORTMASTER=OFF
cmake --build build -j
```

Release build (recommended for playtesting):

```bash
cmake -S . -B build_release -DCMAKE_BUILD_TYPE=Release -DWIZBALL_PORTMASTER=OFF
cmake --build build_release -j
```

### Run

Run from the `wizball/` data directory:

```bash
cd wizball
../build/wizball
```

Useful flags:

- `-debug` enables extra logging.

If you changed script/global-parameter content and need to regenerate the compiled script output:

```bash
cmake --build build --target rebuild_wizball_scripts
```

## Legal and licensing notes

- This repository preserves a remake of IP originally created by Sensible Software.
- Graham Goring explicitly gave permission in email for the source to be made public (apparently it's "a salient lesson in how not to code a game").
- This repository is licensed under the MIT License (see `LICENSE`): effectively "do what you want, but no warranty."

## Preservation note

This project matters because it captures a specific era of fan remakes: skilled, funny, deeply obsessive, and built with love for 8-bit originals.

If you played this version back in the day, welcome home.

## Bonus

There is actual code here that will also make an Exolon remake!! I'll get to that one
