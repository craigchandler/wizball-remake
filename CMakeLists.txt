cmake_minimum_required(VERSION 3.16)

project(WizBall LANGUAGES CXX)
find_package(Python3 REQUIRED COMPONENTS Interpreter)
include(CheckCXXSymbolExists)

set(CMAKE_CXX_STANDARD 98)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS ON)

option(WIZBALL_ENABLE_FMOD "Build with FMOD backend (requires FMOD headers/libs)" OFF)
option(WIZBALL_ENABLE_ASAN "Build with AddressSanitizer (debug heap corruption)" OFF)
set(WIZBALL_DAT_TOOL "dat" CACHE STRING "Path/name of Allegro dat packing tool")

find_path(ALLEGRO_INCLUDE_DIR allegro.h)
find_library(ALLEGRO_LIBRARY NAMES alleg allegro)
find_library(ALLEGGL_LIBRARY NAMES alleggl)
find_library(OPENGL_LIBRARY NAMES GL OpenGL)
find_library(GLU_LIBRARY NAMES GLU)

if(NOT ALLEGRO_INCLUDE_DIR OR NOT ALLEGRO_LIBRARY OR NOT ALLEGGL_LIBRARY OR NOT OPENGL_LIBRARY OR NOT GLU_LIBRARY)
  message(FATAL_ERROR "Missing required deps. Need Allegro4 (headers + lib), AllegroGL, OpenGL and GLU.")
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
  wizball/parser.cpp
  wizball/particles.cpp
  wizball/paths.cpp
  wizball/report.cpp
  wizball/roomzones.cpp
  wizball/scripting.cpp
  wizball/simple_gui.cpp
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

if(WIZBALL_ENABLE_FMOD)
  find_path(FMOD_INCLUDE_DIR
    fmod.h
    PATH_SUFFIXES fmodex
  )
  find_library(FMOD_LIBRARY NAMES fmod fmodex)

  if(NOT FMOD_INCLUDE_DIR OR NOT FMOD_LIBRARY)
    message(FATAL_ERROR "WIZBALL_ENABLE_FMOD is ON, but FMOD headers or library were not found.")
  endif()

  # This codebase uses the legacy FSOUND_* API, which is not present in all FMOD packages.
  set(CMAKE_REQUIRED_INCLUDES ${FMOD_INCLUDE_DIR})
  check_cxx_symbol_exists(FSOUND_Init "fmod.h" WIZBALL_FMOD_HAS_FSOUND_API)
  unset(CMAKE_REQUIRED_INCLUDES)

  if(NOT WIZBALL_FMOD_HAS_FSOUND_API)
    message(FATAL_ERROR
      "WIZBALL_ENABLE_FMOD is ON, but the installed FMOD headers do not expose legacy FSOUND_* symbols.\n"
      "This engine's sound backend expects FMOD 3-style FSOUND API. Keep WIZBALL_ENABLE_FMOD=OFF,\n"
      "or port wizball/sound.cpp to modern FMOD Ex API.")
  endif()

  list(APPEND WIZBALL_SOURCES wizball/sound.cpp)
else()
  list(APPEND WIZBALL_SOURCES wizball/sound_stub.cpp)
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

if(WIZBALL_ENABLE_FMOD)
  target_include_directories(wizball PRIVATE ${FMOD_INCLUDE_DIR})
  target_link_libraries(wizball PRIVATE ${FMOD_LIBRARY})
endif()

target_link_libraries(wizball PRIVATE
  ${ALLEGRO_LIBRARY}
  ${ALLEGGL_LIBRARY}
  ${OPENGL_LIBRARY}
  ${GLU_LIBRARY}
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
