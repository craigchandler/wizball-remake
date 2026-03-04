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
cd "$GAMEDIR" || exit 1

# Optional: log file for debugging on device
> "$GAMEDIR/log.txt" && exec > >(tee "$GAMEDIR/log.txt") 2>&1

# SDL controller mapping provided by PortMaster
export SDL_GAMECONTROLLERCONFIG="$sdl_controllerconfig"

# If you ever ship extra libs, uncomment this and use libs.aarch64 / libs.armhf
# export LD_LIBRARY_PATH="$GAMEDIR/libs.${DEVICE_ARCH}:$LD_LIBRARY_PATH"

BIN="$GAMEDIR/wizball.${DEVICE_ARCH}"
chmod +x "$BIN"

# PortMaster helper (sets up env / permissions / etc.)
pm_platform_helper "$BIN"

"$BIN"

pm_finish