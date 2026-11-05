// All the tunable numbers live here so balancing never means hunting through code.
// Terrain values were tuned against previews of several seeds.
#pragma once

#include <cstdint>

namespace wl::constants {

// --- Simulation timing ---

// Fixed sim step. Same behaviour at 60 Hz and 120 Hz, and repeatable in tests.
inline constexpr double simulationStepSeconds = 1.0 / 60.0;

// Cap on catch-up steps per frame, so a slow frame can't snowball.
inline constexpr int maxSimulationStepsPerFrame = 8;

// Background tabs can report huge deltas when you come back. Clamp them.
inline constexpr double maxFrameDeltaSeconds = 0.25;

// --- Display ---

// We render in software, so cost scales with pixel count. Pick an integer
// upscale that keeps the framebuffer's long edge near this size.
inline constexpr int targetFramebufferLongEdgeInPixels = 640;

// Floor for either framebuffer edge, so tiny windows still work.
inline constexpr int minFramebufferEdgeInPixels = 64;

// Used until the browser tells us the real canvas size.
inline constexpr int defaultDeviceWidthInPixels = 1280;
inline constexpr int defaultDeviceHeightInPixels = 720;

// Framebuffer pixels per tile. 8 shows about 80 tiles across on a laptop.
inline constexpr int defaultTileSizeInPixels = 8;
inline constexpr int minTileSizeInPixels = 1;
inline constexpr int maxTileSizeInPixels = 16;

// --- World ---

inline constexpr std::uint32_t defaultWorldSeed = 1337;

// 32x32 keeps per-chunk work small enough to spread over frames, while
// keeping the number of chunks on screen low.
inline constexpr int chunkSizeInTiles = 32;

// How far to look when picking a spawn point on land, and how coarsely.
inline constexpr int spawnSearchRadiusInTiles = 4000;
inline constexpr int spawnSearchStepInTiles = 16;

// --- Elevation noise ---
// Frequencies are in cycles per tile, so 1/420 means features about 420 tiles across.

inline constexpr double elevationBaseFrequency = 1.0 / 420.0;
inline constexpr int elevationOctaves = 6;
inline constexpr double noiseLacunarity = 2.0;
inline constexpr double noisePersistence = 0.5;

// fBm bunches up near zero. Stretch it so we get real lowlands and peaks.
inline constexpr double elevationContrast = 1.7;

// Domain warp: nudge sample positions by another noise field before reading
// elevation. Bends straight-ish contours into organic coastlines.
inline constexpr double warpBaseFrequency = 1.0 / 260.0;
inline constexpr int warpOctaves = 3;
inline constexpr double warpStrengthInTiles = 45.0;

// Ridged noise layered on high ground to carve mountain ranges.
inline constexpr double ridgeBaseFrequency = 1.0 / 160.0;
inline constexpr int ridgeOctaves = 4;
inline constexpr double ridgeStrength = 0.2;
inline constexpr double ridgeStartElevation = 0.62;  // ridges fade in above this

// --- Moisture and temperature noise ---

inline constexpr double moistureBaseFrequency = 1.0 / 380.0;
inline constexpr int moistureOctaves = 4;
inline constexpr double moistureContrast = 1.6;

// Lower frequency than moisture so climate zones are broad. Too high and
// desert sits right next to snow.
inline constexpr double temperatureBaseFrequency = 1.0 / 1100.0;
inline constexpr int temperatureOctaves = 3;
inline constexpr double temperatureContrast = 1.6;

// Cooling per unit of elevation above sea level, so peaks get snow.
inline constexpr double temperatureLapseRate = 0.35;

// --- Biome thresholds (all values in [0, 1]) ---

inline constexpr double deepWaterElevation = 0.33;  // below: deep ocean
inline constexpr double seaLevelElevation = 0.43;   // below: shallow water
inline constexpr double beachElevation = 0.455;     // below: beach
inline constexpr double mountainElevation = 0.78;   // above: bare rock
inline constexpr double snowCapElevation = 0.9;     // above: snow peaks

inline constexpr double snowTemperature = 0.12;    // colder: snow fields
inline constexpr double tundraTemperature = 0.27;  // colder: tundra
inline constexpr double hotTemperature = 0.6;      // warmer: desert/savanna/jungle band

inline constexpr double desertMoisture = 0.32;   // hot and drier: desert
inline constexpr double savannaMoisture = 0.5;   // hot and drier: savanna
inline constexpr double forestMoisture = 0.6;    // temperate and wetter: forest

// --- Terrain shading ---

// Turns elevation slope into light. Slopes per tile are tiny (around 0.005),
// so this needs to be large.
inline constexpr double hillshadeStrength = 40.0;

// How much hillshade brightens or darkens land colour at full strength.
inline constexpr double hillshadeContrast = 0.3;

// Random brightness per tile, so flat areas don't look like a paint bucket fill.
inline constexpr double tileColorJitter = 0.06;

// Per-pixel brightness grain inside tiles.
inline constexpr double pixelGrainStrength = 0.035;

// Water below this elevation renders at full deep colour.
inline constexpr double deepestWaterElevation = 0.1;

// Mountains get lighter toward the peaks by up to this much.
inline constexpr double mountainHeightTint = 0.25;

// --- Palette ---

struct Rgb8 {
    std::uint8_t red, green, blue;
};

inline constexpr Rgb8 shallowWaterColor{58, 118, 150};
inline constexpr Rgb8 deepWaterColor{14, 30, 66};
inline constexpr Rgb8 beachColor{214, 196, 148};
inline constexpr Rgb8 grasslandColor{112, 156, 78};
inline constexpr Rgb8 forestColor{54, 106, 62};
inline constexpr Rgb8 desertColor{220, 186, 122};
inline constexpr Rgb8 savannaColor{176, 164, 90};
inline constexpr Rgb8 tundraColor{136, 148, 124};
inline constexpr Rgb8 snowColor{232, 238, 244};
inline constexpr Rgb8 mountainColor{122, 116, 112};
inline constexpr Rgb8 unloadedColor{11, 13, 22};  // matches the page background

}  // namespace wl::constants
