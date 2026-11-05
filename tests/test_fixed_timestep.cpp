#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "constants.h"
#include "fixed_timestep.h"

using wl::FixedTimestepClock;

namespace {
constexpr double stepSeconds = 1.0 / 60.0;
constexpr int maxSteps = 8;
constexpr double maxDelta = 0.25;
}  // namespace

TEST_CASE("one step of elapsed time runs one step", "[timestep]") {
    FixedTimestepClock clock(stepSeconds, maxSteps, maxDelta);
    REQUIRE(clock.consumeElapsedTime(stepSeconds) == 1);
}

TEST_CASE("partial steps carry over between frames", "[timestep]") {
    FixedTimestepClock clock(stepSeconds, maxSteps, maxDelta);
    REQUIRE(clock.consumeElapsedTime(stepSeconds / 2) == 0);
    REQUIRE(clock.interpolationAlpha() == Catch::Approx(0.5));
    REQUIRE(clock.consumeElapsedTime(stepSeconds / 2) == 1);
}

TEST_CASE("a huge delta is capped and the backlog dropped", "[timestep]") {
    FixedTimestepClock clock(stepSeconds, maxSteps, maxDelta);
    REQUIRE(clock.consumeElapsedTime(60.0) == maxSteps);
    // Next normal frame should be back to normal, not still catching up.
    REQUIRE(clock.consumeElapsedTime(stepSeconds) == 1);
}

TEST_CASE("negative delta runs nothing", "[timestep]") {
    FixedTimestepClock clock(stepSeconds, maxSteps, maxDelta);
    REQUIRE(clock.consumeElapsedTime(-1.0) == 0);
    REQUIRE(clock.interpolationAlpha() == Catch::Approx(0.0));
}
