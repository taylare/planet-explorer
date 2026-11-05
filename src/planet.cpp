#include "planet.h"

#include <algorithm>
#include <array>
#include <cstdio>
#include <iomanip>
#include <iterator>
#include <sstream>

#include "hash.h"

namespace wl {

namespace {

using constants::Rgb8;

// Seed salts, one per independent decision. Never change them, or every
// world would turn into a different planet.
enum class PlanetSalt : std::uint32_t {
    Theme = 401,
    Name = 402,
    Jitter = 403,
    Gravity = 404,
    DayLength = 405,
    Atmosphere = 406,
    Moons = 407,
    Aurora = 408,
};

// How far each colour channel may drift from its theme, so two planets with
// the same theme still look a little different.
constexpr int maxChannelJitter = 8;

// Moon counts are rolled once: a quarter of planets have one, half have two, a quarter have three.
constexpr float oneMoonChance = 0.25f;
constexpr float twoMoonChance = 0.75f;

// Roughly a third of planets have no aurora at all.
constexpr float noAuroraChance = 0.3f;
constexpr double minimumVisibleAuroraStrength = 0.35;

// Atmosphere rolls, as cumulative chances.
constexpr float thinAtmosphereChance = 0.2f;
constexpr float breathableAtmosphereChance = 0.6f;
constexpr float denseAtmosphereChance = 0.85f;

// How the moon sizes step down, largest first.
constexpr int moonRadiiInPixels[constants::maxMoons] = {5, 4, 3};

// Moons are lit in one of these tints.
constexpr Rgb8 moonColors[] = {
    {236, 232, 196},  // pale yellow
    {190, 208, 240},  // blue grey
    {240, 196, 210},  // soft pink
    {240, 204, 150},  // amber
};

struct Theme {
    PlanetPalette palette;
    float skyRed;
    float skyGreen;
    float skyBlue;
    Rgb8 aurora;
};

const std::array<Theme, 6>& themes() {
    static const std::array<Theme, 6> allThemes = {{
        // Verdant: the original look, kept as one of the possibilities.
        {defaultPlanetPalette(), 1.0f, 0.99f, 0.97f, {110, 240, 170}},
        // Violet bloom
        {{{96, 104, 190}, {26, 20, 74}, {222, 196, 206}, {150, 98, 168}, {92, 56, 128}, {226, 170, 150},
          {196, 130, 120}, {150, 134, 170}, {240, 234, 250}, {116, 104, 130}},
         0.97f, 0.94f, 1.0f, {200, 140, 255}},
        // Rust giant
        {{{106, 138, 120}, {30, 48, 50}, {226, 178, 130}, {176, 112, 62}, {122, 66, 44}, {232, 150, 90},
          {206, 126, 70}, {164, 130, 110}, {244, 226, 214}, {128, 92, 84}},
         1.0f, 0.93f, 0.86f, {255, 170, 110}},
        // Glacier world
        {{{120, 186, 208}, {20, 52, 96}, {222, 228, 232}, {150, 190, 190}, {82, 136, 150}, {210, 214, 200},
          {176, 190, 170}, {160, 176, 188}, {244, 248, 252}, {128, 140, 156}},
         0.94f, 0.98f, 1.0f, {120, 240, 230}},
        // Emerald mist
        {{{64, 160, 150}, {10, 56, 70}, {208, 214, 160}, {70, 170, 110}, {30, 110, 80}, {214, 200, 130},
          {160, 176, 84}, {120, 160, 140}, {228, 244, 238}, {104, 122, 112}},
         0.94f, 1.0f, 0.96f, {140, 255, 150}},
        // Amber dusk
        {{{120, 130, 170}, {40, 30, 72}, {232, 200, 140}, {196, 170, 70}, {132, 122, 50}, {236, 180, 100},
          {212, 140, 70}, {156, 140, 110}, {246, 238, 226}, {112, 92, 96}},
         1.0f, 0.96f, 0.88f, {255, 120, 170}},
    }};
    return allThemes;
}

constexpr const char* namePrefixes[] = {"Kel", "Vor", "Ast", "Zen", "Tau", "Nyx", "Ori", "Lum",
                                        "Cae", "Dra", "Sol", "Eri", "Mar", "Pho", "Thal", "Ves"};
constexpr const char* nameMiddles[] = {"a", "o", "u", "e", "ia", "ae"};
constexpr const char* nameSuffixes[] = {"ra", "thos", "nix", "ion", "dor", "vara", "lis", "mere"};
constexpr const char* romanNumerals[] = {"I", "II", "III", "IV", "V", "VI", "VII", "VIII", "IX"};

template <typename T, std::size_t Size>
constexpr std::uint32_t countOf(const T (&)[Size]) {
    return static_cast<std::uint32_t>(Size);
}

std::uint32_t hashFor(WorldSeed seed, PlanetSalt salt, std::uint32_t extra = 0) {
    return mixBits32(deriveSeed(seed, static_cast<std::uint32_t>(salt)) + extra * hashSeedOffset);
}

float unitFor(WorldSeed seed, PlanetSalt salt, std::uint32_t extra = 0) {
    return hashToUnitFloat(hashFor(seed, salt, extra));
}

std::uint8_t clampChannel(float value) {
    return static_cast<std::uint8_t>(std::clamp(value, 0.0f, 255.0f));
}

Rgb8 scaled(const Rgb8& color, float factor) {
    return {clampChannel(color.red * factor), clampChannel(color.green * factor), clampChannel(color.blue * factor)};
}

Rgb8 mixColors(const Rgb8& from, const Rgb8& to, float amount) {
    return {clampChannel(from.red + (to.red - from.red) * amount),
            clampChannel(from.green + (to.green - from.green) * amount),
            clampChannel(from.blue + (to.blue - from.blue) * amount)};
}

// Nudges each channel by a small amount taken from different bits of one hash.
Rgb8 jitterColor(const Rgb8& color, std::uint32_t hash) {
    const auto offsetFrom = [hash](int shift) {
        return static_cast<int>((hash >> shift) % (2 * maxChannelJitter + 1)) - maxChannelJitter;
    };
    return {static_cast<std::uint8_t>(std::clamp(color.red + offsetFrom(0), 0, 255)),
            static_cast<std::uint8_t>(std::clamp(color.green + offsetFrom(8), 0, 255)),
            static_cast<std::uint8_t>(std::clamp(color.blue + offsetFrom(16), 0, 255))};
}

PlanetPalette jitterPalette(const PlanetPalette& base, WorldSeed seed) {
    const auto jitter = [seed](const Rgb8& color, std::uint32_t colorIndex) {
        return jitterColor(color, hashFor(seed, PlanetSalt::Jitter, colorIndex));
    };
    PlanetPalette palette;
    palette.shallowWater = jitter(base.shallowWater, 0);
    palette.deepWater = jitter(base.deepWater, 1);
    palette.beach = jitter(base.beach, 2);
    palette.grassland = jitter(base.grassland, 3);
    palette.forest = jitter(base.forest, 4);
    palette.desert = jitter(base.desert, 5);
    palette.savanna = jitter(base.savanna, 6);
    palette.tundra = jitter(base.tundra, 7);
    palette.snow = jitter(base.snow, 8);
    palette.mountain = jitter(base.mountain, 9);
    return palette;
}

std::string makePlanetName(WorldSeed seed) {
    const std::uint32_t nameHash = hashFor(seed, PlanetSalt::Name);
    std::string name = namePrefixes[nameHash % countOf(namePrefixes)];
    name += nameMiddles[mixBits32(nameHash + 1) % countOf(nameMiddles)];
    name += nameSuffixes[mixBits32(nameHash + 2) % countOf(nameSuffixes)];
    name += ' ';
    name += romanNumerals[mixBits32(nameHash + 3) % countOf(romanNumerals)];
    return name;
}

Atmosphere rollAtmosphere(float roll) {
    if (roll < thinAtmosphereChance) return Atmosphere::Thin;
    if (roll < breathableAtmosphereChance) return Atmosphere::Breathable;
    if (roll < denseAtmosphereChance) return Atmosphere::Dense;
    return Atmosphere::Hazy;
}

std::string hexColor(const Rgb8& color) {
    char text[8];
    std::snprintf(text, sizeof(text), "#%02x%02x%02x", color.red, color.green, color.blue);
    return text;
}

}  // namespace

bool PlanetPalette::operator==(const PlanetPalette& other) const {
    const auto same = [](const Rgb8& a, const Rgb8& b) {
        return a.red == b.red && a.green == b.green && a.blue == b.blue;
    };
    return same(shallowWater, other.shallowWater) && same(deepWater, other.deepWater) && same(beach, other.beach) &&
           same(grassland, other.grassland) && same(forest, other.forest) && same(desert, other.desert) &&
           same(savanna, other.savanna) && same(tundra, other.tundra) && same(snow, other.snow) &&
           same(mountain, other.mountain);
}

PlanetPalette defaultPlanetPalette() {
    using namespace constants;
    return {shallowWaterColor, deepWaterColor, beachColor, grasslandColor, forestColor,
            desertColor,       savannaColor,   tundraColor, snowColor,      mountainColor};
}

SurfaceColors surfaceColorsFor(const PlanetPalette& palette) {
    // The factors are chosen so the default palette gives back the original
    // Wanderlight tree and rock colours (within a few steps per channel).
    SurfaceColors colors;
    colors.broadleafLight = scaled(palette.grassland, 0.9f);
    colors.broadleafDark = scaled(palette.forest, 1.07f);
    colors.forestLeafLight = scaled(palette.forest, 1.15f);
    colors.forestLeafDark = scaled(palette.forest, 0.64f);
    colors.pineLight = mixColors(palette.forest, palette.shallowWater, 0.25f);
    colors.pineDark = scaled(colors.pineLight, 0.55f);
    colors.acaciaLeaf = scaled(palette.savanna, 0.77f);
    colors.acaciaLeafDark = scaled(palette.savanna, 0.55f);
    colors.cactusLight = scaled(palette.grassland, 0.92f);
    colors.cactusDark = scaled(palette.forest, 1.1f);
    colors.sandstoneLight = scaled(palette.desert, 0.9f);
    colors.sandstoneDark = scaled(palette.desert, 0.68f);
    colors.dryGrass = scaled(palette.savanna, 1.12f);
    return colors;
}

std::string_view atmosphereName(Atmosphere atmosphere) {
    switch (atmosphere) {
        case Atmosphere::Thin: return "Thin";
        case Atmosphere::Breathable: return "Breathable";
        case Atmosphere::Dense: return "Dense";
        case Atmosphere::Hazy: return "Hazy";
        case Atmosphere::Count: break;
    }
    return "Unknown";
}

PlanetProfile makePlanetProfile(WorldSeed seed) {
    using namespace constants;
    PlanetProfile planet;
    planet.name = makePlanetName(seed);

    const auto& allThemes = themes();
    const Theme& theme = allThemes[hashFor(seed, PlanetSalt::Theme) % allThemes.size()];
    planet.palette = jitterPalette(theme.palette, seed);
    planet.skyTintRed = theme.skyRed;
    planet.skyTintGreen = theme.skyGreen;
    planet.skyTintBlue = theme.skyBlue;

    planet.atmosphere = rollAtmosphere(unitFor(seed, PlanetSalt::Atmosphere));
    planet.gravity = minGravity + (maxGravity - minGravity) * unitFor(seed, PlanetSalt::Gravity);
    planet.dayLengthScale =
        minDayLengthScale + (maxDayLengthScale - minDayLengthScale) * unitFor(seed, PlanetSalt::DayLength);

    const float moonRoll = unitFor(seed, PlanetSalt::Moons);
    planet.moonCount = moonRoll < oneMoonChance ? 1 : (moonRoll < twoMoonChance ? 2 : 3);
    for (int index = 0; index < planet.moonCount; ++index) {
        const std::uint32_t moonIndex = static_cast<std::uint32_t>(index) + 1;
        Moon& moon = planet.moons[index];
        moon.color = moonColors[hashFor(seed, PlanetSalt::Moons, moonIndex) % countOf(moonColors)];
        moon.startX = unitFor(seed, PlanetSalt::Moons, moonIndex * 4 + 1);
        moon.startY = unitFor(seed, PlanetSalt::Moons, moonIndex * 4 + 2);
        moon.driftSpeed = 0.6 + 0.8 * unitFor(seed, PlanetSalt::Moons, moonIndex * 4 + 3);
        moon.radiusInPixels = moonRadiiInPixels[index];
    }

    const float auroraRoll = unitFor(seed, PlanetSalt::Aurora);
    planet.auroraStrength =
        auroraRoll < noAuroraChance
            ? 0.0
            : minimumVisibleAuroraStrength + (1.0 - minimumVisibleAuroraStrength) * unitFor(seed, PlanetSalt::Aurora, 1);
    planet.auroraColor = theme.aurora;
    return planet;
}

LightingState planetLighting(const PlanetProfile& planet, const LightingState& base) {
    LightingState lit = base;
    lit.tintRed *= planet.skyTintRed;
    lit.tintGreen *= planet.skyTintGreen;
    lit.tintBlue *= planet.skyTintBlue;

    // A little cool light per moon, fading in with the dark.
    const float moonlight = base.nightAmount * constants::moonlightPerMoon * static_cast<float>(planet.moonCount);
    lit.tintRed = std::min(1.0f, lit.tintRed + moonlight * 0.8f);
    lit.tintGreen = std::min(1.0f, lit.tintGreen + moonlight * 0.9f);
    lit.tintBlue = std::min(1.0f, lit.tintBlue + moonlight * 1.2f);
    return lit;
}

std::string planetToJson(const PlanetProfile& planet) {
    // Names are letters, spaces and numerals only, so no escaping is needed.
    std::ostringstream json;
    json << std::fixed << std::setprecision(2);
    json << "{\"name\":\"" << planet.name << "\",\"atmosphere\":\"" << atmosphereName(planet.atmosphere)
         << "\",\"gravity\":" << planet.gravity << ",\"dayMinutes\":" << planet.dayLengthInSeconds() / 60.0
         << ",\"moons\":" << planet.moonCount << ",\"aurora\":" << (planet.hasAurora() ? "true" : "false")
         << ",\"swatches\":[";
    const constants::Rgb8 swatches[] = {planet.palette.shallowWater, planet.palette.grassland, planet.palette.forest,
                                        planet.palette.desert,       planet.palette.mountain,  planet.palette.snow};
    for (std::size_t index = 0; index < std::size(swatches); ++index) {
        if (index > 0) json << ',';
        json << '"' << hexColor(swatches[index]) << '"';
    }
    json << "]}";
    return json.str();
}

}  // namespace wl
