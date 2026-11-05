// What makes each world seed its own planet: a colour palette, a sky tint,
// moons, an optional aurora and a few stats for the planet card.
//
// Everything here is a pure function of the seed, so a planet never needs to
// be saved. Terrain shape still comes from world.cpp; this only changes how
// the world looks and what it is called.
#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <string_view>

#include "constants.h"
#include "day_night.h"
#include "world.h"

namespace wl {

// Ground and water colours, one per biome family.
struct PlanetPalette {
    constants::Rgb8 shallowWater;
    constants::Rgb8 deepWater;
    constants::Rgb8 beach;
    constants::Rgb8 grassland;
    constants::Rgb8 forest;
    constants::Rgb8 desert;
    constants::Rgb8 savanna;
    constants::Rgb8 tundra;
    constants::Rgb8 snow;
    constants::Rgb8 mountain;

    bool operator==(const PlanetPalette& other) const;
    bool operator!=(const PlanetPalette& other) const { return !(*this == other); }
};

// The original Wanderlight colours from constants.h.
PlanetPalette defaultPlanetPalette();

// Plants and rocks take their colour from the ground they grow on, so an
// alien palette gives alien trees without a second set of art.
struct SurfaceColors {
    constants::Rgb8 broadleafLight;
    constants::Rgb8 broadleafDark;
    constants::Rgb8 forestLeafLight;
    constants::Rgb8 forestLeafDark;
    constants::Rgb8 pineLight;
    constants::Rgb8 pineDark;
    constants::Rgb8 acaciaLeaf;
    constants::Rgb8 acaciaLeafDark;
    constants::Rgb8 cactusLight;
    constants::Rgb8 cactusDark;
    constants::Rgb8 sandstoneLight;
    constants::Rgb8 sandstoneDark;
    constants::Rgb8 dryGrass;
};

SurfaceColors surfaceColorsFor(const PlanetPalette& palette);

enum class Atmosphere : std::uint8_t { Thin, Breathable, Dense, Hazy, Count };

std::string_view atmosphereName(Atmosphere atmosphere);

// A moon only shows as a soft glint on water at night, like the stars do.
struct Moon {
    constants::Rgb8 color;
    double startX = 0.0;       // 0 to 1 across the screen when time is zero
    double startY = 0.0;       // 0 to 1 down the screen
    double driftSpeed = 1.0;   // multiplier on constants::moonDriftPixelsPerSecond
    int radiusInPixels = 4;
};

struct PlanetProfile {
    std::string name;
    PlanetPalette palette;
    Atmosphere atmosphere = Atmosphere::Breathable;

    // Multipliers on the daytime light grade, so noon on a violet world is a little violet.
    float skyTintRed = 1.0f;
    float skyTintGreen = 1.0f;
    float skyTintBlue = 1.0f;

    double gravity = 1.0;         // in g. Shown on the planet card only.
    double dayLengthScale = 1.0;  // 1.0 is constants::dayLengthSeconds

    int moonCount = 1;
    std::array<Moon, constants::maxMoons> moons{};

    double auroraStrength = 0.0;  // 0 means no aurora on this planet
    constants::Rgb8 auroraColor{110, 240, 170};

    bool hasAurora() const { return auroraStrength >= constants::auroraStrengthMinimum; }
    double dayLengthInSeconds() const { return constants::dayLengthSeconds * dayLengthScale; }
};

PlanetProfile makePlanetProfile(WorldSeed seed);

// Applies the planet's sky tint to the time-of-day grade and adds moonlight at night.
LightingState planetLighting(const PlanetProfile& planet, const LightingState& base);

// For the planet card in the page: {"name":"...","atmosphere":"...","gravity":0.92,...}
std::string planetToJson(const PlanetProfile& planet);

}  // namespace wl
