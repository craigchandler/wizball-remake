cmake_minimum_required(VERSION 3.16)

project(WizBall LANGUAGES CXX)
include(CheckCXXSymbolExists)
include(CheckCXXSourceCompiles)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS ON)

option(WIZBALL_ENABLE_ASAN "Build with AddressSanitizer (debug heap corruption)" OFF)
set(WIZBALL_RENDER_BACKEND "SDL" CACHE STRING "Renderer backend target (SDL or GLES2)")
set_property(CACHE WIZBALL_RENDER_BACKEND PROPERTY STRINGS SDL GLES2)

# ---------------------------
# Sources
# ---------------------------
set(WIZBALL_SOURCES
  wizball/ai_nodes.cpp
  wizball/ai_zones.cpp
  wizball/arrays.cpp
  wizball/control.cpp
  wizball/events.cpp
  wizball/file_stuff.cpp
  wizball/font_routines.cpp
  wizball/FORTIFY.CPP
  wizball/game.cpp
  wizball/global_param_list.cpp
  wizball/graphics.cpp
  wizball/helpertag.cpp
  wizball/hiscores.cpp
  wizball/input_recording.cpp
  wizball/main.cpp
  wizball/math_stuff.cpp
  wizball/new_editor.cpp
  wizball/object_collision.cpp
  wizball/output.cpp
  wizball/path_utils.cpp
  wizball/parser.cpp
  wizball/particles.cpp
  wizball/platform.cpp
  wizball/platform_input.cpp
  wizball/platform_renderer.cpp
  wizball/paths.cpp
  wizball/report.cpp
  wizball/roomzones.cpp
  wizball/scripting.cpp
  wizball/simple_gui.cpp
  wizball/sound_sdl.cpp
  wizball/spawn_points.cpp
  wizball/string_stuff.cpp
  wizball/textfiles.cpp
  wizball/tilecolour.cpp
  wizball/tilegroup.cpp
  wizball/tilemaps.cpp
  wizball/tilesets.cpp
  wizball/vector_collision.cpp
  wizball/vertex_colouring.cpp
  wizball/world_collision.cpp
)

add_executable(wizball ${WIZBALL_SOURCES})

target_include_directories(wizball PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/wizball
)

# ---------------------------
# SDL2 deps
# ---------------------------
if(WIN32)
  # MinGW or MSVC: use CMake find commands (CMAKE_FIND_ROOT_PATH set by toolchain).
  find_library(SDL2_LIB       NAMES SDL2       REQUIRED)
  find_library(SDL2_MAIN_LIB  NAMES SDL2main)
  find_library(SDL2_MIXER_LIB NAMES SDL2_mixer REQUIRED)
  find_library(SDL2_IMAGE_LIB NAMES SDL2_image REQUIRED)
  find_path(SDL2_INCLUDE_DIR  NAMES SDL.h PATH_SUFFIXES SDL2 REQUIRED)
  target_include_directories(wizball PRIVATE ${SDL2_INCLUDE_DIR})
  target_link_libraries(wizball PRIVATE
    mingw32
    $<$<BOOL:${SDL2_MAIN_LIB}>:${SDL2_MAIN_LIB}>
    ${SDL2_LIB}
    ${SDL2_MIXER_LIB}
    ${SDL2_IMAGE_LIB}
  )
  if(MINGW)
    # Statically link libgcc and libstdc++ to avoid distributing their DLLs
    target_link_options(wizball PRIVATE "-static-libgcc" "-static-libstdc++")
  endif()
else()
  find_package(PkgConfig REQUIRED)
  pkg_check_modules(SDL2 REQUIRED IMPORTED_TARGET sdl2)
  pkg_check_modules(SDL2_MIXER REQUIRED IMPORTED_TARGET SDL2_mixer)
  pkg_check_modules(SDL2_IMAGE REQUIRED IMPORTED_TARGET SDL2_image)
  target_link_libraries(wizball PRIVATE
    PkgConfig::SDL2
    PkgConfig::SDL2_MIXER
    PkgConfig::SDL2_IMAGE
    m
  )
endif()

if(WIZBALL_RENDER_BACKEND STREQUAL "GLES2")
  # Prefer pkg-config (respects cross-compilation sysroot via PKG_CONFIG_EXECUTABLE).
  # Fall back to find_library for environments that lack a .pc file (e.g. PortMaster sysroot).
  pkg_check_modules(GLES2 glesv2)
  if(GLES2_FOUND)
    target_link_libraries(wizball PRIVATE ${GLES2_LIBRARIES})
    target_include_directories(wizball PRIVATE ${GLES2_INCLUDE_DIRS})
  else()
    find_library(WIZBALL_GLES2_LIB
      NAMES GLESv2 OpenGLESv2
      HINTS "${CMAKE_SYSROOT}/usr/lib"
            "${CMAKE_FIND_ROOT_PATH}/usr/lib"
      REQUIRED)
    target_link_libraries(wizball PRIVATE ${WIZBALL_GLES2_LIB})
  endif()
  target_compile_definitions(wizball PRIVATE WIZBALL_RENDER_BACKEND_GLES2=1)
elseif(NOT WIZBALL_RENDER_BACKEND STREQUAL "SDL")
  message(FATAL_ERROR "Unsupported WIZBALL_RENDER_BACKEND='${WIZBALL_RENDER_BACKEND}'. Expected SDL or GLES2.")
endif()

# ---------------------------
# Warnings / ASAN
# ---------------------------
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(wizball PRIVATE
    -Wno-write-strings
    -Wno-noexcept-type
  )
  # Define RELEASE_MODE for Release builds: enables dat-file loading by default
  # and disables debug scripting paths / in-game debug keys.
  if(CMAKE_BUILD_TYPE STREQUAL "Release")
    target_compile_definitions(wizball PRIVATE RELEASE_MODE)
  endif()

  if(WIZBALL_ENABLE_ASAN)
    target_compile_options(wizball PRIVATE -fsanitize=address -fno-omit-frame-pointer -g)
    target_link_options(wizball PRIVATE -fsanitize=address)
  endif()
endif()

# Platform defines: ALLEGRO_WINDOWS on Win32, ALLEGRO_LINUX everywhere else.
if(WIN32)
  target_compile_definitions(wizball PRIVATE ALLEGRO_WINDOWS)
else()
  # Use verbose (non-binary) tileset/tilemap loading; matches the ALLEGRO_LINUX path
  # in new_editor.cpp which skips tilesets.dat.
  target_compile_definitions(wizball PRIVATE ALLEGRO_LINUX)
endif()

# ---------------------------
# PortMaster staging / install
# ---------------------------
option(WIZBALL_PORTMASTER "Enable PortMaster-style staging install" ON)

# Determine PortMaster arch label
set(PM_ARCH "unknown")
if(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|arm64")
  set(PM_ARCH "aarch64")
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "armv7|armv7l|armhf")
  set(PM_ARCH "armhf")
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|amd64")
  set(PM_ARCH "x86_64")
endif()

if(WIZBALL_PORTMASTER)
  set(WIZBALL_PORTNAME "wizball")
  set_target_properties(wizball PROPERTIES OUTPUT_NAME "wizball.${PM_ARCH}")

# PortMaster on-device layout:
#   ports/WizBall.sh                 <- launch script (ports root)
#   ports/wizball/wizball.<arch>     <- binary
#   ports/wizball/port.json          <- port metadata
#   ports/wizball/wizball/           <- game data
#
# cmake --install ... --prefix staging  produces:
#   staging/WizBall.sh
#   staging/wizball/wizball.<arch>
#   staging/wizball/port.json
#   staging/wizball/wizball/

# Binary -> staging/wizball/
install(TARGETS wizball
  RUNTIME DESTINATION "${WIZBALL_PORTNAME}"
)

# Launch script -> staging/  (ports root level)
install(FILES "${CMAKE_SOURCE_DIR}/portmaster/WizBall.sh"
  DESTINATION "."
  PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE
              GROUP_READ GROUP_EXECUTE
              WORLD_READ WORLD_EXECUTE
)

# portmaster -> staging/wizball/
install(DIRECTORY "${CMAKE_SOURCE_DIR}/portmaster/"
  DESTINATION "${WIZBALL_PORTNAME}"
  PATTERN "WizBall.sh"   EXCLUDE
)

# Game data -> staging/wizball/wizball/
# Generated .dat artifacts are rebuilt explicitly via the wizball binary's
# command-line options and are not staged by default.
install(DIRECTORY "${CMAKE_SOURCE_DIR}/wizball/wizball/"
  DESTINATION "${WIZBALL_PORTNAME}/wizball"
  PATTERN "*.cpp" EXCLUDE
  PATTERN "*.c"   EXCLUDE
  PATTERN "*.h"   EXCLUDE
  PATTERN "*.hpp" EXCLUDE
  PATTERN "datatables/*" EXCLUDE
  PATTERN "scripts/*"    EXCLUDE
  PATTERN "tilemaps/*"   EXCLUDE
  PATTERN "textfiles/*"  EXCLUDE
  PATTERN "tilesets/*"   EXCLUDE
  PATTERN "gfx.dat"      EXCLUDE
  PATTERN "stream.dat"   EXCLUDE
  PATTERN "sfx.dat"      EXCLUDE
  PATTERN "tilesets.dat" EXCLUDE
  PATTERN "tilemaps.dat" EXCLUDE
  PATTERN "paths.dat"    EXCLUDE
  PATTERN "prefabs.dat"  EXCLUDE
  PATTERN "data.dat"     EXCLUDE
  PATTERN "WizBall.sh" EXCLUDE
  PATTERN "port.json"  EXCLUDE
)
else()
  set_target_properties(wizball PROPERTIES OUTPUT_NAME "wizball")
endif()
# ---------------------------
# Optional binary-driven helpers
# ---------------------------
# These wrap the wizball executable's command-line options for running the game
# and rebuilding generated script/tile data. They are not a separate packfile
# build workflow.
add_custom_target(run_wizball
  COMMAND $<TARGET_FILE:wizball>
  DEPENDS wizball
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/wizball
  USES_TERMINAL
)

add_custom_target(run_wizball_debug
  COMMAND $<TARGET_FILE:wizball> -debug
  DEPENDS wizball
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/wizball
  USES_TERMINAL
)

add_custom_target(rebuild_wizball_scripts
  COMMAND $<TARGET_FILE:wizball> -rebuildscripts wizball
  DEPENDS wizball
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/wizball
  USES_TERMINAL
)

add_custom_target(rebuild_wizball_tilesets
  COMMAND $<TARGET_FILE:wizball> -rebuildtilesets wizball
  DEPENDS wizball
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/wizball
  USES_TERMINAL
)

add_custom_target(rebuild_wizball_tilemaps
  COMMAND $<TARGET_FILE:wizball> -rebuildtilemaps wizball
  DEPENDS wizball
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/wizball
  USES_TERMINAL
)
