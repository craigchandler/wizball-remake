cmake_minimum_required(VERSION 3.16)

project(WizBall LANGUAGES CXX)
find_package(Python3 REQUIRED COMPONENTS Interpreter)
include(CheckCXXSymbolExists)
include(CheckCXXSourceCompiles)

set(CMAKE_CXX_STANDARD 98)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS ON)

option(WIZBALL_ENABLE_ASAN "Build with AddressSanitizer (debug heap corruption)" OFF)
set(WIZBALL_DAT_TOOL "dat" CACHE STRING "Path/name of Allegro dat packing tool")

find_path(ALLEGRO_INCLUDE_DIR allegro.h)
find_library(ALLEGRO_LIBRARY NAMES alleg allegro)
if(NOT ALLEGRO_INCLUDE_DIR OR NOT ALLEGRO_LIBRARY)
  message(FATAL_ERROR "Missing required deps. Need Allegro4 (headers + lib).")
endif()

set(WIZBALL_SOURCES
  wizball/ai_nodes.cpp
  wizball/ai_zones.cpp
  wizball/arrays.cpp
  wizball/control.cpp
  wizball/events.cpp
  wizball/file_stuff.cpp
  wizball/font_routines.cpp
  wizball/FORTIFY.CPP
  wizball/funkyfont.cpp
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

find_package(SDL2 QUIET)
if(NOT SDL2_FOUND)
  message(FATAL_ERROR "SDL2 was not found.")
endif()

find_package(SDL2_mixer QUIET)
if(NOT SDL2_mixer_FOUND)
  message(FATAL_ERROR "SDL2_mixer was not found.")
endif()

find_package(SDL2_image QUIET)
if(NOT SDL2_mixer_FOUND)
  message(FATAL_ERROR "SDL2_image was not found.")
endif()

add_executable(wizball ${WIZBALL_SOURCES})

if(UNIX AND NOT APPLE)
  target_compile_definitions(wizball PRIVATE ALLEGRO_LINUX)
endif()

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  # Legacy codebase warning reductions so real portability issues are easier to see.
  target_compile_options(wizball PRIVATE
    -Wno-write-strings
    -Wno-noexcept-type
  )

  if(WIZBALL_ENABLE_ASAN)
    target_compile_options(wizball PRIVATE -fsanitize=address -fno-omit-frame-pointer -g)
    target_link_options(wizball PRIVATE -fsanitize=address)
  endif()
endif()

target_include_directories(wizball PRIVATE
  ${ALLEGRO_INCLUDE_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}/wizball
)

if(TARGET SDL2::SDL2)
  target_link_libraries(wizball PRIVATE SDL2::SDL2)
else()
  target_include_directories(wizball PRIVATE ${SDL2_INCLUDE_DIRS})
  target_link_libraries(wizball PRIVATE ${SDL2_LIBRARIES})
endif()

if(TARGET SDL2_mixer::SDL2_mixer)
  target_link_libraries(wizball PRIVATE SDL2_mixer::SDL2_mixer)
else()
  target_include_directories(wizball PRIVATE ${SDL2_MIXER_INCLUDE_DIRS})
  target_link_libraries(wizball PRIVATE ${SDL2_MIXER_LIBRARIES})
endif()

if(TARGET SDL2_image::SDL2_image)
  target_link_libraries(wizball PRIVATE SDL2_image::SDL2_image)
else()
  target_include_directories(wizball PRIVATE ${SDL2_IMAGE_INCLUDE_DIRS})
  target_link_libraries(wizball PRIVATE ${SDL2_IMAGE_LIBRARIES})
endif()

target_link_libraries(wizball PRIVATE
  ${ALLEGRO_LIBRARY}
  m
)


add_custom_target(run_wizball_dat
  COMMAND $<TARGET_FILE:wizball> -dat
  DEPENDS wizball
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/wizball
  USES_TERMINAL
)

add_custom_target(run_wizball_dat_debug
  COMMAND $<TARGET_FILE:wizball> -dat -debug
  DEPENDS wizball
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/wizball
  USES_TERMINAL
)

add_custom_target(repack_wizball_data
  COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tools/repack_wizball_data.py
          --root ${CMAKE_CURRENT_SOURCE_DIR}
          --dat-tool ${WIZBALL_DAT_TOOL}
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  USES_TERMINAL
)

add_custom_target(repack_wizball_data_full
  COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tools/repack_wizball_data.py
          --root ${CMAKE_CURRENT_SOURCE_DIR}
          --dat-tool ${WIZBALL_DAT_TOOL}
          --include-core-data
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  USES_TERMINAL
)

add_custom_target(rebuild_wizball_scripts_and_data
  COMMAND $<TARGET_FILE:wizball> -rebuildscripts wizball
  COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tools/repack_wizball_data.py
          --root ${CMAKE_CURRENT_SOURCE_DIR}
          --dat-tool ${WIZBALL_DAT_TOOL}
          --include-core-data
  DEPENDS wizball
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/wizball
  USES_TERMINAL
)
