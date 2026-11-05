include(FetchContent)

# Pinned release tarball: faster than a git clone and reproducible.
FetchContent_Declare(
  Catch2
  URL https://github.com/catchorg/Catch2/archive/refs/tags/v3.7.1.tar.gz
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_MakeAvailable(Catch2)

add_executable(wanderlight_tests
  test_biome.cpp
  test_fixed_timestep.cpp
  test_framebuffer.cpp
  test_game.cpp
  test_noise.cpp
  test_renderer.cpp
  test_world.cpp
)
target_link_libraries(wanderlight_tests PRIVATE wanderlight_core Catch2::Catch2WithMain)

list(APPEND CMAKE_MODULE_PATH "${catch2_SOURCE_DIR}/extras")
include(Catch)
catch_discover_tests(wanderlight_tests)
