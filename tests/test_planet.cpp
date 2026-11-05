#include <algorithm>
#include <catch2/catch_test_macros.hpp>
#include <cctype>
#include <cstdlib>
#include <set>
#include <string>

#include "constants.h"
#include "planet.h"

using namespace wl;

namespace {
// How many seeds to look at when checking that planets vary.
constexpr int sampleSeedCount = 300;

bool closeTo(const constants::Rgb8& color, int red, int green, int blue, int tolerance) {
    return std::abs(color.red - red) <= tolerance && std::abs(color.green - green) <= tolerance &&
           std::abs(color.blue - blue) <= tolerance;
}
}  // namespace

TEST_CASE("a planet is a pure function of its seed", "[planet]") {
    const PlanetProfile first = makePlanetProfile(1337);
    const PlanetProfile second = makePlanetProfile(1337);
    REQUIRE(first.name == second.name);
    REQUIRE(first.palette == second.palette);
    REQUIRE(first.moonCount == second.moonCount);
    REQUIRE(first.gravity == second.gravity);
    REQUIRE(first.dayLengthScale == second.dayLengthScale);
    REQUIRE(first.auroraStrength == second.auroraStrength);
    REQUIRE(planetToJson(first) == planetToJson(second));
}

TEST_CASE("different seeds make different planets", "[planet]") {
    std::set<std::string> names;
    std::set<int> moonCounts;
    bool sawAurora = false;
    bool sawNoAurora = false;
    for (int seed = 0; seed < sampleSeedCount; ++seed) {
        const PlanetProfile planet = makePlanetProfile(static_cast<WorldSeed>(seed));
        names.insert(planet.name);
        moonCounts.insert(planet.moonCount);
        (planet.hasAurora() ? sawAurora : sawNoAurora) = true;
    }
    // Names can collide now and then, but most seeds should be unique.
    REQUIRE(names.size() > static_cast<std::size_t>(sampleSeedCount * 0.85));
    REQUIRE(moonCounts == std::set<int>{1, 2, 3});
    REQUIRE(sawAurora);
    REQUIRE(sawNoAurora);
}

TEST_CASE("every planet's stats stay in their ranges", "[planet]") {
    for (int seed = 0; seed < sampleSeedCount; ++seed) {
        const PlanetProfile planet = makePlanetProfile(static_cast<WorldSeed>(seed) * 7919u);
        REQUIRE(planet.gravity >= constants::minGravity);
        REQUIRE(planet.gravity <= constants::maxGravity);
        REQUIRE(planet.dayLengthScale >= constants::minDayLengthScale);
        REQUIRE(planet.dayLengthScale <= constants::maxDayLengthScale);
        REQUIRE(planet.moonCount >= 1);
        REQUIRE(planet.moonCount <= constants::maxMoons);
        REQUIRE((planet.auroraStrength == 0.0 || planet.auroraStrength >= constants::auroraStrengthMinimum));
        REQUIRE(planet.auroraStrength <= 1.0);
        REQUIRE(atmosphereName(planet.atmosphere) != "Unknown");
        for (int index = 0; index < planet.moonCount; ++index) {
            const Moon& moon = planet.moons[index];
            REQUIRE(moon.startX >= 0.0);
            REQUIRE(moon.startX < 1.0);
            REQUIRE(moon.startY >= 0.0);
            REQUIRE(moon.startY < 1.0);
            REQUIRE(moon.radiusInPixels > 0);
        }
    }
}

TEST_CASE("planet names and JSON are safe to hand to the page", "[planet]") {
    for (int seed = 0; seed < sampleSeedCount; ++seed) {
        const PlanetProfile planet = makePlanetProfile(static_cast<WorldSeed>(seed));
        REQUIRE_FALSE(planet.name.empty());
        for (char letter : planet.name) REQUIRE((std::isalpha(static_cast<unsigned char>(letter)) || letter == ' '));

        const std::string json = planetToJson(planet);
        REQUIRE(json.front() == '{');
        REQUIRE(json.back() == '}');
        REQUIRE(json.find("\"name\":\"" + planet.name + "\"") != std::string::npos);
        REQUIRE(std::count(json.begin(), json.end(), '"') % 2 == 0);
        REQUIRE(std::count(json.begin(), json.end(), '[') == std::count(json.begin(), json.end(), ']'));
    }
}

TEST_CASE("the default palette is the original Wanderlight look", "[planet]") {
    const PlanetPalette palette = defaultPlanetPalette();
    REQUIRE(palette.grassland.red == constants::grasslandColor.red);
    REQUIRE(palette.deepWater.blue == constants::deepWaterColor.blue);
    REQUIRE(palette.mountain.green == constants::mountainColor.green);

    // Derived plant colours land close to the hand-picked colours they replaced.
    constexpr int tolerance = 12;
    const SurfaceColors surface = surfaceColorsFor(palette);
    REQUIRE(closeTo(surface.broadleafLight, 96, 150, 76, tolerance));
    REQUIRE(closeTo(surface.forestLeafLight, 62, 118, 66, tolerance));
    REQUIRE(closeTo(surface.pineLight, 58, 110, 84, tolerance));
    REQUIRE(closeTo(surface.sandstoneLight, 196, 160, 112, tolerance));
}

TEST_CASE("plants follow the planet palette", "[planet]") {
    PlanetPalette alien = defaultPlanetPalette();
    alien.grassland = {200, 40, 160};
    const SurfaceColors surface = surfaceColorsFor(alien);
    REQUIRE(surface.broadleafLight.red > surface.broadleafLight.green);
}

TEST_CASE("moonlight lifts the night but never the day", "[planet]") {
    PlanetProfile oneMoon = makePlanetProfile(5);
    PlanetProfile threeMoons = oneMoon;
    oneMoon.moonCount = 1;
    threeMoons.moonCount = 3;

    const LightingState midnight = lightingAt(0.0);
    const LightingState withOne = planetLighting(oneMoon, midnight);
    const LightingState withThree = planetLighting(threeMoons, midnight);
    REQUIRE(withThree.tintBlue > withOne.tintBlue);

    const LightingState noon = lightingAt(0.5);
    const LightingState noonOnPlanet = planetLighting(threeMoons, noon);
    REQUIRE(noonOnPlanet.tintRed == noon.tintRed * threeMoons.skyTintRed);

    for (const LightingState& state : {withOne, withThree, noonOnPlanet}) {
        REQUIRE(state.tintRed <= 1.0f);
        REQUIRE(state.tintGreen <= 1.0f);
        REQUIRE(state.tintBlue <= 1.0f);
    }
}
