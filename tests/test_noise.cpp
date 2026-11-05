#include <algorithm>
#include <catch2/catch_test_macros.hpp>
#include <cmath>

#include "hash.h"
#include "noise.h"

using namespace wl;

namespace {
// Spread test points over a wide area, including negative coordinates.
constexpr int sampleCount = 200000;
constexpr double sampleAreaHalfWidth = 5000.0;

double testCoordinate(int index, int axis) {
    return (hashToUnitFloat(hashCoords(99, index, axis)) * 2.0 - 1.0) * sampleAreaHalfWidth;
}
}  // namespace

TEST_CASE("simplex noise stays in [-1, 1] and uses most of that range", "[noise]") {
    double lowest = 1.0;
    double highest = -1.0;
    for (int index = 0; index < sampleCount; ++index) {
        const double value = simplexNoise2D(1234, testCoordinate(index, 0), testCoordinate(index, 1));
        REQUIRE(value >= -1.0);
        REQUIRE(value <= 1.0);
        lowest = std::min(lowest, value);
        highest = std::max(highest, value);
    }
    // Scaled correctly, the extremes should come close to the bounds.
    REQUIRE(lowest < -0.7);
    REQUIRE(highest > 0.7);
}

TEST_CASE("simplex noise is deterministic per seed and differs between seeds", "[noise]") {
    REQUIRE(simplexNoise2D(5, 12.3, -45.6) == simplexNoise2D(5, 12.3, -45.6));
    int differences = 0;
    for (int index = 0; index < 100; ++index) {
        const double x = testCoordinate(index, 0);
        const double y = testCoordinate(index, 1);
        if (simplexNoise2D(5, x, y) != simplexNoise2D(6, x, y)) ++differences;
    }
    REQUIRE(differences > 90);
}

TEST_CASE("simplex noise is continuous", "[noise]") {
    // A tiny step in input should only make a tiny change in output.
    constexpr double tinyStep = 1e-4;
    constexpr double maxChange = 0.01;
    for (int index = 0; index < 10000; ++index) {
        const double x = testCoordinate(index, 0);
        const double y = testCoordinate(index, 1);
        REQUIRE(std::abs(simplexNoise2D(7, x, y) - simplexNoise2D(7, x + tinyStep, y)) < maxChange);
    }
}

TEST_CASE("simplex noise is zero on lattice points", "[noise]") {
    // Every gradient is dotted with a zero offset at a corner, so the sum is zero.
    REQUIRE(simplexNoise2D(3, 0.0, 0.0) == 0.0);
}

TEST_CASE("fBm stays in [-1, 1] for any octave count", "[noise]") {
    for (int octaves = 1; octaves <= FractalNoise::maxOctaves; ++octaves) {
        FractalNoiseSettings settings;
        settings.baseFrequency = 1.0 / 200.0;
        settings.octaves = octaves;
        const FractalNoise noise(42, settings);
        for (int index = 0; index < 5000; ++index) {
            const double value = noise.sample(testCoordinate(index, 0), testCoordinate(index, 1));
            REQUIRE(value >= -1.0);
            REQUIRE(value <= 1.0);
        }
    }
}

TEST_CASE("ridged fBm stays in [0, 1]", "[noise]") {
    FractalNoiseSettings settings;
    settings.baseFrequency = 1.0 / 100.0;
    settings.octaves = 4;
    const FractalNoise noise(8, settings);
    for (int index = 0; index < 20000; ++index) {
        const double value = noise.sampleRidged(testCoordinate(index, 0), testCoordinate(index, 1));
        REQUIRE(value >= 0.0);
        REQUIRE(value <= 1.0);
    }
}

TEST_CASE("octave count is clamped to the supported range", "[noise]") {
    FractalNoiseSettings settings;
    settings.octaves = 100;
    const FractalNoise noise(1, settings);
    const double value = noise.sample(3.3, 4.4);
    REQUIRE(value >= -1.0);
    REQUIRE(value <= 1.0);
}
