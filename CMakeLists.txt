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
  src/fixed_timestep.cpp
  src/framebuffer.cpp
  src/game.cpp
  src/noise.cpp
  src/renderer.cpp
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
    "-sALLOW_MEMORY_GROWTH=1"
    "$<$<CONFIG:Release>:-O3>"
    "$<$<CONFIG:Debug>:-sASSERTIONS=2>"
  )
endif()

# --- Native tests ---
if(WANDERLIGHT_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()
