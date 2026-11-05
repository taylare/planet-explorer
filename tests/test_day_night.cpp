#include <algorithm>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include <iterator>

#include "constants.h"
#include "day_night.h"

using namespace wl;

namespace {
float largestDifference(const LightingState& a, const LightingState& b) {
    return std::max({std::abs(a.tintRed - b.tintRed), std::abs(a.tintGreen - b.tintGreen),
                     std::abs(a.tintBlue - b.tintBlue), std::abs(a.nightAmount - b.nightAmount)});
}
}  // namespace

TEST_CASE("keyframes are sorted and wrap cleanly", "[day_night]") {
    const auto& keys = constants::lightKeyframes;
    const auto keyCount = std::size(keys);
    for (std::size_t index = 1; index < keyCount; ++index) REQUIRE(keys[index].timeOfDay > keys[index - 1].timeOfDay);
    REQUIRE(keys[0].timeOfDay == 0.0);
    REQUIRE(keys[keyCount - 1].timeOfDay == 1.0);
    REQUIRE(keys[0].red == keys[keyCount - 1].red);
    REQUIRE(keys[0].night == keys[keyCount - 1].night);
}

TEST_CASE("lighting has no jump at midnight", "[day_night]") {
    REQUIRE(largestDifference(lightingAt(0.0), lightingAt(0.99999)) < 0.001f);
    REQUIRE(largestDifference(lightingAt(1.0), lightingAt(0.0)) < 0.001f);
    REQUIRE(largestDifference(lightingAt(-0.25), lightingAt(0.75)) < 0.001f);
}

TEST_CASE("lighting changes smoothly through the day", "[day_night]") {
    constexpr int samples = 10000;
    LightingState previous = lightingAt(0.0);
    for (int sample = 1; sample <= samples; ++sample) {
        const LightingState current = lightingAt(static_cast<double>(sample) / samples);
        REQUIRE(largestDifference(previous, current) < 0.01f);
        REQUIRE(current.tintRed >= 0.0f);
        REQUIRE(current.tintRed <= 1.0f);
        REQUIRE(current.nightAmount >= 0.0f);
        REQUIRE(current.nightAmount <= 1.0f);
        previous = current;
    }
}

TEST_CASE("noon is bright day, midnight is full night", "[day_night]") {
    const LightingState noon = lightingAt(0.5);
    const LightingState midnight = lightingAt(0.0);
    REQUIRE(noon.nightAmount == Catch::Approx(0.0f));
    REQUIRE(noon.tintRed == Catch::Approx(1.0f));
    REQUIRE(midnight.nightAmount == Catch::Approx(1.0f));
    REQUIRE(midnight.tintRed < 0.5f);
    // Night leans blue.
    REQUIRE(midnight.tintBlue > midnight.tintRed);
}

TEST_CASE("the cycle advances and wraps", "[day_night]") {
    DayNightCycle cycle(0.9);
    cycle.advance(constants::dayLengthSeconds * 0.2);
    REQUIRE(cycle.timeOfDay() == Catch::Approx(0.1).margin(1e-9));

    cycle.setTimeOfDay(-0.25);
    REQUIRE(cycle.timeOfDay() == Catch::Approx(0.75));
}
