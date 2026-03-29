cmake_minimum_required(VERSION 3.16)

project(WizBall LANGUAGES C CXX)
include(CheckCXXSymbolExists)
include(CheckCXXSourceCompiles)
find_package(Python3 REQUIRED COMPONENTS Interpreter)

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
  third_party/physfs/extras/physfsrwops.c
  wizball/ai_nodes.cpp
  wizball/ai_zones.cpp
  wizball/arrays.cpp
  wizball/asset_vfs.cpp
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
  wizball/savegame.cpp
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

if(WIN32)
  list(APPEND WIZBALL_SOURCES wizball/windows_resource.rc)
endif()

add_executable(wizball ${WIZBALL_SOURCES})

set(WIZBALL_VERSION_TEXT_FILE "${CMAKE_SOURCE_DIR}/wizball/wizball/textfiles/version_generated.txt")
set(WIZBALL_SCRIPTFILE_TSL "${CMAKE_SOURCE_DIR}/wizball/wizball/scriptfile.tsl")
set(WIZBALL_GLOBAL_PARAMETER_LIST_FILE "${CMAKE_SOURCE_DIR}/wizball/wizball/global_parameter_list.txt")
set(WIZBALL_GPL_LIST_SIZE_FILE "${CMAKE_SOURCE_DIR}/wizball/wizball/gpl_list_size.txt")
set(WIZBALL_CPTF_FILE "${CMAKE_SOURCE_DIR}/wizball/wizball/cptf.txt")
set(WIZBALL_DATATABLE_DATA_FILE "${CMAKE_SOURCE_DIR}/wizball/wizball/datatable.dat")
set(WIZBALL_TILESETS_DATA_FILE "${CMAKE_SOURCE_DIR}/wizball/wizball/tilesets.dat")
set(WIZBALL_TILEMAPS_DATA_FILE "${CMAKE_SOURCE_DIR}/wizball/wizball/tilemaps.dat")

add_custom_target(wizball_generate_version_text ALL
  COMMAND ${CMAKE_COMMAND}
    -DPROJECT_SOURCE_DIR=${CMAKE_SOURCE_DIR}
    -DOUTPUT_FILE=${WIZBALL_VERSION_TEXT_FILE}
    -P ${CMAKE_SOURCE_DIR}/cmake/GenerateVersionText.cmake
  BYPRODUCTS ${WIZBALL_VERSION_TEXT_FILE}
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
  VERBATIM
)

add_dependencies(wizball wizball_generate_version_text)

target_include_directories(wizball PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/third_party/physfs/extras
  ${CMAKE_CURRENT_SOURCE_DIR}/wizball
)

# ---------------------------
# PhysFS
# ---------------------------
set(PHYSFS_BUILD_STATIC ON CACHE BOOL "" FORCE)
set(PHYSFS_BUILD_SHARED OFF CACHE BOOL "" FORCE)
set(PHYSFS_BUILD_TEST OFF CACHE BOOL "" FORCE)
set(PHYSFS_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(PHYSFS_INSTALL OFF CACHE BOOL "" FORCE)
set(PHYSFS_INSTALL_MAN OFF CACHE BOOL "" FORCE)
set(PHYSFS_ARCHIVE_ZIP ON CACHE BOOL "" FORCE)
set(PHYSFS_ARCHIVE_7Z OFF CACHE BOOL "" FORCE)
set(PHYSFS_ARCHIVE_GRP OFF CACHE BOOL "" FORCE)
set(PHYSFS_ARCHIVE_WAD OFF CACHE BOOL "" FORCE)
set(PHYSFS_ARCHIVE_CSM OFF CACHE BOOL "" FORCE)
set(PHYSFS_ARCHIVE_HOG OFF CACHE BOOL "" FORCE)
set(PHYSFS_ARCHIVE_MVL OFF CACHE BOOL "" FORCE)
set(PHYSFS_ARCHIVE_QPAK OFF CACHE BOOL "" FORCE)
set(PHYSFS_ARCHIVE_ROFS OFF CACHE BOOL "" FORCE)
set(PHYSFS_ARCHIVE_SLB OFF CACHE BOOL "" FORCE)
set(PHYSFS_ARCHIVE_ISO9660 OFF CACHE BOOL "" FORCE)
set(PHYSFS_ARCHIVE_VDF OFF CACHE BOOL "" FORCE)
set(PHYSFS_ARCHIVE_LECARCHIVES OFF CACHE BOOL "" FORCE)
set(PHYSFS_ARCHIVE_POD OFF CACHE BOOL "" FORCE)
add_subdirectory(third_party/physfs)
target_link_libraries(wizball PRIVATE physfs-static)
target_compile_definitions(wizball PRIVATE PHYSFS_STATIC)

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
    $<$<COMPILE_LANGUAGE:CXX>:-Wno-noexcept-type>
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
set(WIZBALL_DATA_ZIP "${CMAKE_CURRENT_BINARY_DIR}/data.zip")
set(WIZBALL_DATA_ZIP_STATIC_INPUTS
  "${CMAKE_SOURCE_DIR}/wizball/wizball/icon.png"
  "${CMAKE_SOURCE_DIR}/wizball/wizball/project_settings.txt"
)
file(GLOB_RECURSE WIZBALL_SCRIPT_REBUILD_INPUTS CONFIGURE_DEPENDS
  "${CMAKE_SOURCE_DIR}/wizball/wizball/scripts/*"
  "${CMAKE_SOURCE_DIR}/wizball/wizball/textfiles/*"
)
file(GLOB_RECURSE WIZBALL_TILESET_REBUILD_INPUTS CONFIGURE_DEPENDS
  "${CMAKE_SOURCE_DIR}/wizball/wizball/tilesets/*"
  "${CMAKE_SOURCE_DIR}/wizball/wizball/sprites/*"
)
file(GLOB_RECURSE WIZBALL_TILEMAP_REBUILD_INPUTS CONFIGURE_DEPENDS
  "${CMAKE_SOURCE_DIR}/wizball/wizball/tilemaps/*"
)
add_custom_command(
  OUTPUT
    "${WIZBALL_SCRIPTFILE_TSL}"
    "${WIZBALL_GLOBAL_PARAMETER_LIST_FILE}"
    "${WIZBALL_GPL_LIST_SIZE_FILE}"
    "${WIZBALL_CPTF_FILE}"
    "${WIZBALL_DATATABLE_DATA_FILE}"
  COMMAND $<TARGET_FILE:wizball> -rebuildscripts wizball
  DEPENDS
    wizball
    wizball_generate_version_text
    "${CMAKE_SOURCE_DIR}/wizball/SCRIPTLANGUAGE.TXT"
    ${WIZBALL_SCRIPT_REBUILD_INPUTS}
  WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/wizball"
  VERBATIM
)
add_custom_target(rebuild_wizball_scripts
  DEPENDS
    "${WIZBALL_SCRIPTFILE_TSL}"
    "${WIZBALL_GLOBAL_PARAMETER_LIST_FILE}"
    "${WIZBALL_GPL_LIST_SIZE_FILE}"
    "${WIZBALL_CPTF_FILE}"
    "${WIZBALL_DATATABLE_DATA_FILE}"
)
file(GLOB_RECURSE WIZBALL_DATA_ZIP_FOLDER_INPUTS CONFIGURE_DEPENDS
  "${CMAKE_SOURCE_DIR}/wizball/wizball/paths/*"
  "${CMAKE_SOURCE_DIR}/wizball/wizball/prefabs/*"
  "${CMAKE_SOURCE_DIR}/wizball/wizball/sound_fx/*"
  "${CMAKE_SOURCE_DIR}/wizball/wizball/sprites/*"
  "${CMAKE_SOURCE_DIR}/wizball/wizball/streams/*"
)
set(WIZBALL_DATA_ZIP_INPUTS
  "${WIZBALL_GLOBAL_PARAMETER_LIST_FILE}"
  "${WIZBALL_GPL_LIST_SIZE_FILE}"
  "${WIZBALL_CPTF_FILE}"
  "${WIZBALL_SCRIPTFILE_TSL}"
  "${WIZBALL_DATATABLE_DATA_FILE}"
  "${WIZBALL_TILESETS_DATA_FILE}"
  "${WIZBALL_TILEMAPS_DATA_FILE}"
  ${WIZBALL_DATA_ZIP_STATIC_INPUTS}
  ${WIZBALL_DATA_ZIP_FOLDER_INPUTS}
)

add_custom_command(
  OUTPUT "${WIZBALL_DATA_ZIP}"
  COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tools/package_data_zip.py"
          --root "${CMAKE_SOURCE_DIR}"
          --output "${WIZBALL_DATA_ZIP}"
  DEPENDS
    "${CMAKE_SOURCE_DIR}/tools/package_data_zip.py"
    wizball_generate_version_text
    ${WIZBALL_DATA_ZIP_INPUTS}
  WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
  VERBATIM
)

add_custom_target(wizball_data_zip DEPENDS "${WIZBALL_DATA_ZIP}")
add_dependencies(wizball_data_zip wizball_generate_version_text)
add_dependencies(wizball_data_zip rebuild_wizball_scripts)

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

install(FILES "${WIZBALL_DATA_ZIP}"
  DESTINATION "${WIZBALL_PORTNAME}"
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

add_custom_command(
  OUTPUT "${WIZBALL_TILESETS_DATA_FILE}"
  COMMAND $<TARGET_FILE:wizball> -rebuildtilesets wizball
  DEPENDS
    wizball
    rebuild_wizball_scripts
    ${WIZBALL_TILESET_REBUILD_INPUTS}
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/wizball
  VERBATIM
)
add_custom_target(rebuild_wizball_tilesets
  DEPENDS "${WIZBALL_TILESETS_DATA_FILE}"
)

add_custom_command(
  OUTPUT "${WIZBALL_TILEMAPS_DATA_FILE}"
  COMMAND $<TARGET_FILE:wizball> -rebuildtilemaps wizball
  DEPENDS
    wizball
    rebuild_wizball_scripts
    ${WIZBALL_TILEMAP_REBUILD_INPUTS}
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/wizball
  VERBATIM
)
add_custom_target(rebuild_wizball_tilemaps
  DEPENDS "${WIZBALL_TILEMAPS_DATA_FILE}"
)

add_dependencies(rebuild_wizball_tilesets rebuild_wizball_scripts)
add_dependencies(rebuild_wizball_tilemaps rebuild_wizball_scripts)

if(NOT CMAKE_CROSSCOMPILING)
  add_dependencies(wizball_data_zip rebuild_wizball_tilesets)
  add_dependencies(wizball_data_zip rebuild_wizball_tilemaps)
endif()
