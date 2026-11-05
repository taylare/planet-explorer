#include <catch2/catch_test_macros.hpp>
#include <set>
#include <string>

#include "constants.h"
#include "world.h"

using namespace wl;

TEST_CASE("every elevation, moisture and temperature combination maps to a biome", "[biome]") {
    // Sweep a little past [0, 1] on purpose: out-of-range input must be safe too.
    constexpr int stepsPerAxis = 121;
    constexpr double sweepStart = -0.1;
    constexpr double sweepStep = 0.01;

    std::set<Biome> biomesFound;
    for (int elevationStep = 0; elevationStep < stepsPerAxis; ++elevationStep) {
        for (int moistureStep = 0; moistureStep < stepsPerAxis; ++moistureStep) {
            for (int temperatureStep = 0; temperatureStep < stepsPerAxis; ++temperatureStep) {
                const Biome biome = classifyBiome(sweepStart + elevationStep * sweepStep,
                                                  sweepStart + moistureStep * sweepStep,
                                                  sweepStart + temperatureStep * sweepStep);
                REQUIRE(static_cast<int>(biome) >= 0);
                REQUIRE(static_cast<int>(biome) < biomeCount);
                biomesFound.insert(biome);
            }
        }
    }
    // And every biome is reachable, so none of them are dead code.
    REQUIRE(static_cast<int>(biomesFound.size()) == biomeCount);
}

TEST_CASE("biome lookup spot checks", "[biome]") {
    REQUIRE(classifyBiome(0.0, 0.5, 0.5) == Biome::DeepOcean);
    REQUIRE(classifyBiome(0.4, 0.5, 0.5) == Biome::ShallowWater);
    REQUIRE(classifyBiome(0.44, 0.5, 0.5) == Biome::Beach);
    REQUIRE(classifyBiome(0.6, 0.1, 0.9) == Biome::Desert);
    REQUIRE(classifyBiome(0.6, 0.4, 0.9) == Biome::Savanna);
    REQUIRE(classifyBiome(0.6, 0.3, 0.45) == Biome::Grassland);
    REQUIRE(classifyBiome(0.6, 0.8, 0.45) == Biome::Forest);
    REQUIRE(classifyBiome(0.6, 0.5, 0.2) == Biome::Tundra);
    REQUIRE(classifyBiome(0.6, 0.5, 0.05) == Biome::Snow);
    REQUIRE(classifyBiome(0.8, 0.5, 0.5) == Biome::Mountain);
    REQUIRE(classifyBiome(0.95, 0.5, 0.9) == Biome::Snow);
}

TEST_CASE("every biome has a unique display name", "[biome]") {
    std::set<std::string> names;
    for (int index = 0; index < biomeCount; ++index) {
        const std::string name(biomeName(static_cast<Biome>(index)));
        REQUIRE_FALSE(name.empty());
        REQUIRE(name != "Unknown");
        names.insert(name);
    }
    REQUIRE(static_cast<int>(names.size()) == biomeCount);
}

TEST_CASE("only ocean and shallows count as water", "[biome]") {
    for (int index = 0; index < biomeCount; ++index) {
        const Biome biome = static_cast<Biome>(index);
        const bool expected = biome == Biome::DeepOcean || biome == Biome::ShallowWater;
        REQUIRE(isWater(biome) == expected);
    }
}
