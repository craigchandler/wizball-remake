#!/bin/bash

XDG_DATA_HOME=${XDG_DATA_HOME:-$HOME/.local/share}

if [ -d "/opt/system/Tools/PortMaster/" ]; then
  controlfolder="/opt/system/Tools/PortMaster"
elif [ -d "/opt/tools/PortMaster/" ]; then
  controlfolder="/opt/tools/PortMaster"
elif [ -d "$XDG_DATA_HOME/PortMaster/" ]; then
  controlfolder="$XDG_DATA_HOME/PortMaster"
else
  controlfolder="/roms/ports/PortMaster"
fi

source "$controlfolder/control.txt"
[ -f "${controlfolder}/mod_${CFW_NAME}.txt" ] && source "${controlfolder}/mod_${CFW_NAME}.txt"
get_controls

if [[ $CFW_NAME == "TheRA" ]]; then
  raloc="/opt/retroarch/bin"
  raconf=""
elif [[ $CFW_NAME == "RetroOZ" ]]; then
  raloc="/opt/retroarch/bin"
  raconf="--config /home/odroid/.config/retroarch/retroarch.cfg"
elif [[ $CFW_NAME == *"ArkOS"* ]]; then
  raloc="/usr/local/bin"
  raconf=""
elif [[ $CFW_NAME == "muOS" ]]; then
  raloc="/usr/bin"
  if [ -f /run/muos/storage/info/config/retroarch.cfg ]; then
    raconf="--config /run/muos/storage/info/config/retroarch.cfg"
  elif [ -f /mnt/mmc/MUOS/retroarch/retroarch.cfg ]; then
    raconf="--config /mnt/mmc/MUOS/retroarch/retroarch.cfg"
  else
    raconf=""
  fi
elif [[ $CFW_NAME == "Miyoo" ]]; then
  raloc="/mnt/sdcard/RetroArch"
  raconf=""
else
  raloc="/usr/bin"
  raconf=""
fi

GAMEDIR="/$directory/ports/wizball"
# If $directory is empty (some CFWs don't set it), derive GAMEDIR from
# the script's own location: script lives at ports/WizBall.sh so the
# port folder is the wizball/ sibling directory.
if [ ! -d "$GAMEDIR" ]; then
  GAMEDIR="$(dirname "$(readlink -f "$0")")/wizball"
fi
cd "$GAMEDIR" || { echo "FATAL: cannot cd to $GAMEDIR" > /tmp/wizball_fatal.txt; exit 1; }

# Simple redirect to logfile - avoid process substitution (unreliable on embedded shells)
exec > "$GAMEDIR/log.txt" 2>&1
echo "GAMEDIR=$GAMEDIR"
echo "CFW_NAME=${CFW_NAME}"
echo "DEVICE_ARCH=${DEVICE_ARCH}"

# SDL controller mapping provided by PortMaster
export SDL_GAMECONTROLLERCONFIG="$sdl_controllerconfig"

# Phase 0 GLES2 migration:
# Prefer the GLES2-targeted renderer path on handhelds by default.
# The current runtime still uses SDL's renderer for drawing, but this ensures
# the build/runtime selector and SDL driver preference stay aligned on device.
export WIZBALL_RENDERER_BACKEND="${WIZBALL_RENDERER_BACKEND:-gles2}"
echo "WIZBALL_RENDERER_BACKEND=${WIZBALL_RENDERER_BACKEND}"

# Disable vsync so SDL_RenderPresent does not block waiting for vblank.
# Remove this once the correct frame-pacing strategy is confirmed.
export WIZBALL_SDL2_NOVSYNC=1

# SDL_RenderGeometry is now enabled. The previous workaround (WIZBALL_SDL2_GEOMETRY=0)
# was added because sprites appeared invisible with geometry enabled. That turned out
# to be a vertex-colour (VERTEX_COLOUR_ALPHA zero-RGB) bug in output.cpp, now fixed.
# Geometry batching is critical for performance: SDL batches consecutive same-texture
# geometry calls internally, reducing ~2400 individual GPU commands to ~20 per frame.
# export WIZBALL_SDL2_GEOMETRY=0  # left here for rollback if needed

# If you ever ship extra libs, uncomment this and use libs.aarch64 / libs.armhf
# export LD_LIBRARY_PATH="$GAMEDIR/libs.${DEVICE_ARCH}:$LD_LIBRARY_PATH"

BIN="$GAMEDIR/wizball.${DEVICE_ARCH}"
echo "BIN=$BIN  exists=$(test -f "$BIN" && echo yes || echo NO)"
chmod +x "$BIN"

# PortMaster helper (sets up env / permissions / etc.)
pm_platform_helper "$BIN"

# Start keymapper for this binary (kill-mode hotkey support)
if [ -f "$controlfolder/gptokeyb2" ]; then
  export LD_LIBRARY_PATH="$controlfolder:$LD_LIBRARY_PATH"
  "$controlfolder/gptokeyb2" "$BIN" -c "$GAMEDIR/wizball.gptk" -X &
else
  echo "WARNING: gptokeyb2 not found, skipping keymapper"
fi

echo "Launching $BIN"
"$BIN" -dat -debug
echo "Binary exited with code $?"

pm_finish
