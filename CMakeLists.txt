cmake_minimum_required(VERSION 3.20)
project(Wanderlight VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# Tests only make sense natively. The web build skips them.
if(EMSCRIPTEN)
  set(WANDERLIGHT_TESTS_DEFAULT OFF)
else()
  set(WANDERLIGHT_TESTS_DEFAULT ON)
endif()
option(WANDERLIGHT_BUILD_TESTS "Build the native unit tests" ${WANDERLIGHT_TESTS_DEFAULT})

# --- Core library: all game logic, no Emscripten ---
add_library(wanderlight_core STATIC
  src/art.cpp
  src/chunk_cache.cpp
  src/day_night.cpp
  src/effects.cpp
  src/entities.cpp
  src/exploration.cpp
  src/fixed_timestep.cpp
  src/framebuffer.cpp
  src/game.cpp
  src/input.cpp
  src/journal.cpp
  src/noise.cpp
  src/performance.cpp
  src/props.cpp
  src/renderer.cpp
  src/save_state.cpp
  src/sprite.cpp
  src/weather.cpp
  src/world.cpp
)
target_include_directories(wanderlight_core PUBLIC src)

if(MSVC)
  target_compile_options(wanderlight_core PRIVATE /W4)
else()
  target_compile_options(wanderlight_core PRIVATE -Wall -Wextra -Wpedantic)
endif()

# --- Web build: index.html, index.js, index.wasm into dist/ ---
if(EMSCRIPTEN)
  set(WANDERLIGHT_WEB_OUTPUT_DIR "${PROJECT_SOURCE_DIR}/dist" CACHE PATH "Where the web build goes")
  set(WANDERLIGHT_SHELL "${PROJECT_SOURCE_DIR}/web/shell.html")
  set(WANDERLIGHT_BRIDGE "${PROJECT_SOURCE_DIR}/web/bridge.js")

  add_executable(wanderlight_web src/main.cpp)
  target_link_libraries(wanderlight_web PRIVATE wanderlight_core)

  # Link-time optimisation lets the optimiser inline across files, which
  # matters for the small hot functions (noise corners, hashes, pixel blends).
  target_compile_options(wanderlight_core PRIVATE "$<$<CONFIG:Release>:-flto>")
  target_compile_options(wanderlight_web PRIVATE "$<$<CONFIG:Release>:-flto>")

  set_target_properties(wanderlight_web PROPERTIES
    OUTPUT_NAME "index"
    SUFFIX ".html"
    RUNTIME_OUTPUT_DIRECTORY "${WANDERLIGHT_WEB_OUTPUT_DIR}"
    # Relink when the shell or bridge changes, not just the C++.
    LINK_DEPENDS "${WANDERLIGHT_SHELL};${WANDERLIGHT_BRIDGE}"
  )

  target_link_options(wanderlight_web PRIVATE
    "--shell-file=${WANDERLIGHT_SHELL}"
    "--pre-js=${WANDERLIGHT_BRIDGE}"
    "-sENVIRONMENT=web"
    # Start with enough memory for a typical session (cache, buffers) so it
    # rarely has to grow, but allow growth for very zoomed-out views.
    "-sINITIAL_MEMORY=33554432"
    "-sALLOW_MEMORY_GROWTH=1"
    # Loading a save hands a string from JS to C++, which JS allocates with
    # malloc and C++ frees. Both must be visible to JS. Functions marked
    # EMSCRIPTEN_KEEPALIVE in main.cpp are exported on top of these.
    "-sEXPORTED_FUNCTIONS=_main,_malloc,_free"
    "$<$<CONFIG:Release>:-O3>"
    "$<$<CONFIG:Release>:-flto>"
    "$<$<CONFIG:Debug>:-sASSERTIONS=2>"
  )
endif()

# --- Native tests ---
if(WANDERLIGHT_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()
