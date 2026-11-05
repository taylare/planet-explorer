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

// --- Performance ---

// Framebuffer long edge for each quality level. If frames run slow, the game
// steps down a level, which cuts the pixel count by about a third each time.
// Level 0 must match targetFramebufferLongEdgeInPixels.
inline constexpr int qualityLongEdges[] = {640, 520, 420};
inline constexpr int qualityLevelCount = 3;

// Step down a level when the smoothed C++ work per frame stays above this
// for this many frames in a row. 11 ms leaves room in a 16.7 ms frame for
// the browser's own blit and compositing.
inline constexpr double qualityDowngradeMs = 11.0;
inline constexpr int qualityDowngradeFrames = 90;

// Step back up only after a long run of easy frames, so it doesn't flip-flop.
inline constexpr double qualityUpgradeMs = 5.0;
inline constexpr int qualityUpgradeFrames = 900;

// Weight of each new frame in the smoothed timings. Small means steadier numbers.
inline constexpr double profilerSmoothing = 0.05;

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

// --- Chunk streaming ---

// Chunks are about 9 KB each, so 256 is roughly 2.4 MB. The game raises this
// if the view itself needs more (zoomed right out).
inline constexpr int chunkCacheCapacityInChunks = 256;

// Extra ring of chunks loaded past the screen edge, so walking never reveals
// an unloaded strip.
inline constexpr int chunkPreloadMarginInChunks = 1;

// Max time per frame spent generating chunks. A 60 Hz frame is 16.7 ms and
// rendering needs most of it. At least one chunk is always generated so
// streaming can't stall.
inline constexpr double chunkGenerationBudgetMs = 4.0;

// --- Player movement ---

inline constexpr double walkSpeedTilesPerSecond = 6.0;

// How quickly the player reaches full speed or stops. Higher is snappier.
inline constexpr double playerAccelerationSharpness = 12.0;

// Speed multipliers by ground type. Deep ocean is impassable.
inline constexpr double shallowWaterSpeedMultiplier = 0.45;
inline constexpr double sandSpeedMultiplier = 0.85;
inline constexpr double snowSpeedMultiplier = 0.75;
inline constexpr double mountainSpeedMultiplier = 0.65;

// Distance walked per animation frame.
inline constexpr double strideLengthInTiles = 0.45;

// Below this speed the player counts as standing still for animation.
inline constexpr double idleSpeedTilesPerSecond = 0.3;

// --- Camera ---

// How tightly the camera chases the player. Lower is floatier.
inline constexpr double cameraFollowSharpness = 6.0;

// --- Touch and mouse drag ---

// Drags shorter than this are ignored, so a tap doesn't nudge the player.
inline constexpr double dragDeadzoneInCssPixels = 8.0;

// Drag this far from where you pressed for full speed. Also the joystick's radius.
inline constexpr double dragFullSpeedInCssPixels = 64.0;

// --- Hold to walk (mouse, or touch on the right of the screen) ---

// The player walks toward the held point. Close to the player it stops,
// so holding right on top of them doesn't jitter.
inline constexpr double followDeadzoneInCssPixels = 14.0;

// Full speed when the held point is at least this far from the player.
inline constexpr double followFullSpeedInCssPixels = 110.0;

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

// The biome colours below are the default planet's. makePlanetProfile in
// planet.cpp recolours them for every other world seed.
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

// Player sprite: an explorer in a spacesuit with a handheld headlamp.
inline constexpr Rgb8 suitHelmetColor{214, 220, 232};
inline constexpr Rgb8 suitVisorColor{58, 150, 196};
inline constexpr Rgb8 suitBodyColor{184, 192, 208};
inline constexpr Rgb8 suitPackColor{116, 124, 148};
inline constexpr Rgb8 suitBootColor{58, 62, 80};
inline constexpr Rgb8 headlampColor{190, 244, 255};
inline constexpr Rgb8 headlampHandleColor{120, 128, 148};
inline constexpr Rgb8 spriteOutlineColor{20, 18, 28};

// Light line drawn where water meets a wading sprite.
inline constexpr Rgb8 waterlineColor{150, 196, 214};

// Rows of the sprite hidden underwater while wading.
inline constexpr int wadingHiddenRows = 3;

// Shadow under sprites multiplies the ground by this.
inline constexpr float spriteShadowStrength = 0.6f;

// Sprites are drawn at this tile size at 1x, then scaled up in whole steps.
inline constexpr int spriteBaseTileSizeInPixels = 8;

// Prop and landmark sprites. Tree, cactus and sandstone colours come from the planet
// palette instead (see surfaceColorsFor in planet.cpp).
inline constexpr Rgb8 snowOnBranchColor{226, 234, 242};
inline constexpr Rgb8 trunkColor{92, 66, 50};
inline constexpr Rgb8 rockLightColor{150, 146, 142};
inline constexpr Rgb8 rockDarkColor{98, 94, 96};
inline constexpr Rgb8 ruinStoneLightColor{170, 164, 150};
inline constexpr Rgb8 ruinStoneDarkColor{116, 110, 104};
inline constexpr Rgb8 mossColor{88, 120, 70};
inline constexpr Rgb8 flameOuterColor{240, 128, 50};
inline constexpr Rgb8 flameInnerColor{255, 222, 120};
inline constexpr Rgb8 firewoodColor{84, 56, 40};
inline constexpr Rgb8 towerStoneLightColor{140, 134, 150};
inline constexpr Rgb8 towerStoneDarkColor{90, 84, 104};
inline constexpr Rgb8 towerWindowColor{255, 206, 120};
inline constexpr Rgb8 crystalLightColor{150, 240, 240};
inline constexpr Rgb8 crystalDarkColor{70, 150, 176};
inline constexpr Rgb8 crystalAccentColor{190, 150, 240};
inline constexpr Rgb8 springWaterLightColor{120, 214, 214};
inline constexpr Rgb8 springWaterDarkColor{70, 168, 182};
inline constexpr Rgb8 steamColor{236, 240, 244};
inline constexpr Rgb8 flowerStemColor{70, 120, 60};

// Flower petals pick one of these per flower.
inline constexpr Rgb8 flowerPetalColors[] = {
    {236, 226, 240}, {240, 196, 90}, {220, 120, 150}, {150, 140, 230}};
inline constexpr Rgb8 flowerCentreColor{250, 220, 110};

// Creature sprites
inline constexpr Rgb8 deerBodyColor{150, 104, 70};
inline constexpr Rgb8 deerBellyColor{206, 176, 140};
inline constexpr Rgb8 antlerColor{214, 196, 168};
inline constexpr Rgb8 rabbitBodyColor{170, 150, 128};
inline constexpr Rgb8 rabbitTailColor{240, 236, 230};
inline constexpr Rgb8 crabShellColor{210, 92, 72};
inline constexpr Rgb8 crabClawColor{236, 130, 98};
inline constexpr Rgb8 lizardBodyColor{120, 150, 80};
inline constexpr Rgb8 lizardPatternColor{170, 140, 70};
inline constexpr Rgb8 goatBodyColor{214, 208, 196};
inline constexpr Rgb8 goatHornColor{110, 98, 90};
inline constexpr Rgb8 foxBodyColor{236, 240, 246};
inline constexpr Rgb8 foxShadeColor{176, 188, 204};
inline constexpr Rgb8 creatureEyeColor{24, 20, 24};
inline constexpr Rgb8 creatureLegColor{70, 54, 46};

// --- Props (trees, rocks, flowers, ruins, campfires) ---

// Props sit on a grid of cells this many tiles wide, at most one per cell.
// Must divide chunkSizeInTiles so cells never straddle a chunk edge.
inline constexpr int propCellSizeInTiles = 2;

// Each prop is jittered inside its cell but kept this far from the cell
// edge. Two props can then never be closer than twice this (a jittered grid
// with a guaranteed minimum spacing, like a cheap Poisson disc).
inline constexpr double propCellMarginInTiles = 0.35;

// Per biome: chance a cell gets a prop, then relative weights for each kind.
struct PropMix {
    double density;
    double tree, rock, flower, ruins, campfire;
};
inline constexpr PropMix grasslandPropMix{0.10, 0.25, 0.10, 0.62, 0.015, 0.015};
inline constexpr PropMix forestPropMix{0.38, 0.86, 0.05, 0.07, 0.01, 0.01};
inline constexpr PropMix desertPropMix{0.03, 0.55, 0.40, 0.0, 0.05, 0.0};
inline constexpr PropMix savannaPropMix{0.06, 0.35, 0.18, 0.43, 0.01, 0.03};
inline constexpr PropMix tundraPropMix{0.08, 0.45, 0.35, 0.18, 0.01, 0.01};
inline constexpr PropMix snowPropMix{0.03, 0.60, 0.40, 0.0, 0.0, 0.0};
inline constexpr PropMix mountainPropMix{0.06, 0.15, 0.85, 0.0, 0.0, 0.0};
inline constexpr PropMix beachPropMix{0.015, 0.0, 0.70, 0.0, 0.0, 0.30};

// --- Rare landmarks (tower, crystal grove, hot spring) ---

// The world is split into square regions this many chunks wide. Each region
// holds at most one rare landmark, which keeps them rare and spread out.
inline constexpr int landmarkRegionSizeInChunks = 6;
inline constexpr double landmarkChancePerRegion = 0.6;

// Spots tried per region to find ground that suits the landmark.
inline constexpr int landmarkPlacementAttempts = 8;

// Ordinary props are kept clear of a landmark by this radius.
inline constexpr double landmarkClearingRadiusInTiles = 3.0;

// --- Planets ---
// Each world seed is its own planet. These bound the stats rolled in planet.cpp.

inline constexpr double minGravity = 0.45;
inline constexpr double maxGravity = 1.85;

// Multiplier on dayLengthSeconds. Below 1 is a short day, above 1 a long one.
inline constexpr double minDayLengthScale = 0.7;
inline constexpr double maxDayLengthScale = 1.5;

inline constexpr int maxMoons = 3;

// Each moon lifts the night colour grade by this much (scaled by how dark it is).
inline constexpr float moonlightPerMoon = 0.03f;

// Moons glint on water and slide slowly across the screen.
inline constexpr double moonDriftPixelsPerSecond = 1.2;
inline constexpr float moonReflectionOpacity = 0.75f;

// Aurora: a soft ribbon of coloured light drifting across the night scene.
// Planets rolling less than this strength have no aurora.
inline constexpr double auroraStrengthMinimum = 0.3;
inline constexpr double auroraBandHalfHeightFraction = 0.11;  // of the screen height
inline constexpr float auroraOpacity = 0.32f;

// --- Day and night ---

// One full day in real seconds. Long enough that night feels like a phase, not a flicker.
inline constexpr double dayLengthSeconds = 480.0;

// 0 is midnight, 0.5 is noon. Start in the morning for a friendly first look.
inline constexpr double startingTimeOfDay = 0.32;

// Colour grade through the day. Each key is a time, an RGB multiplier and how
// "night" it is (0 to 1, which drives lights and stars). Times must rise, and
// the last key must match the first so the cycle wraps without a jump.
struct LightKeyframe {
    double timeOfDay;
    float red, green, blue;
    float night;
};
inline constexpr LightKeyframe lightKeyframes[] = {
    {0.00, 0.22f, 0.26f, 0.46f, 1.0f},  // midnight
    {0.20, 0.25f, 0.28f, 0.48f, 1.0f},  // before dawn
    {0.26, 0.86f, 0.62f, 0.64f, 0.4f},  // dawn
    {0.32, 1.00f, 0.95f, 0.88f, 0.0f},  // morning
    {0.50, 1.00f, 1.00f, 1.00f, 0.0f},  // noon
    {0.68, 1.00f, 0.93f, 0.82f, 0.0f},  // afternoon
    {0.74, 0.96f, 0.62f, 0.50f, 0.3f},  // sunset
    {0.80, 0.30f, 0.30f, 0.54f, 0.9f},  // dusk
    {1.00, 0.22f, 0.26f, 0.46f, 1.0f},  // midnight again
};

// --- Lights (only visible as it gets dark) ---

struct LightSettings {
    double radiusInTiles;
    float red, green, blue;
    float intensity;
};
inline constexpr LightSettings headlampLight{4.5, 0.85f, 0.95f, 1.0f, 0.95f};
inline constexpr LightSettings campfireLight{5.5, 1.0f, 0.6f, 0.3f, 1.1f};
inline constexpr LightSettings crystalLight{4.0, 0.45f, 0.95f, 1.0f, 0.9f};
inline constexpr LightSettings towerWindowLight{2.0, 1.0f, 0.8f, 0.45f, 0.8f};

// How much fires flicker (fraction of intensity) and how fast.
inline constexpr float fireFlickerAmount = 0.25f;
inline constexpr double fireFlickerSpeed = 3.0;

// Lights can push a pixel a bit past its daytime colour, which gives a glow.
inline constexpr float maxLitBrightness = 1.15f;

// --- Stars and water ---

// Stars reflect in water at night. Fraction of screen pixels that hold a star.
inline constexpr double starDensity = 0.003;
inline constexpr double starTwinkleSpeed = 2.2;
inline constexpr Rgb8 starColor{236, 240, 255};

// Water texture drifts this many pixels per second, and wobbles row by row.
inline constexpr double waterDriftPixelsPerSecond = 1.5;
inline constexpr double waterWobbleSpeed = 1.3;
inline constexpr double waterWobbleRowFrequency = 0.35;
inline constexpr double waterWobblePixels = 1.5;

// --- Animation ---

inline constexpr double campfireFramesPerSecond = 6.0;
inline constexpr int steamParticleCount = 7;
inline constexpr double steamRiseSeconds = 2.5;  // one particle's life
inline constexpr int steamRiseHeightInPixels = 14;

// --- Wildlife ---

// Creatures live in chunks within this many chunks of the player. This is
// wider than the screen, so they appear and vanish out of sight.
inline constexpr int wildlifeSimulationRadiusInChunks = 2;

inline constexpr int creatureSpawnAttemptsPerChunk = 3;
inline constexpr double creatureSpawnChance = 0.35;

// Creatures wander but are pulled back if they stray this far from home.
inline constexpr double creatureLeashRadiusInTiles = 18.0;

// How fast heading drifts while wandering. Higher is twitchier.
inline constexpr double creatureWanderJitterRadiansPerSecond = 2.5;

// How quickly creatures turn toward where they want to go.
inline constexpr double creatureSteeringSharpness = 5.0;

// Creatures alternate between wandering and resting (grazing, sniffing).
inline constexpr double creatureRestMinSeconds = 2.0;
inline constexpr double creatureRestMaxSeconds = 6.0;
inline constexpr double creatureWanderMinSeconds = 3.0;
inline constexpr double creatureWanderMaxSeconds = 8.0;

// Distance moved per hop of the walk animation.
inline constexpr double creatureStrideInTiles = 0.3;

struct SpeciesTuning {
    double walkSpeed;    // tiles per second while wandering
    double fleeSpeed;    // tiles per second when startled
    double fleeRadius;   // player distance that startles them, in tiles
};
inline constexpr SpeciesTuning deerTuning{1.6, 6.5, 5.0};
inline constexpr SpeciesTuning rabbitTuning{1.2, 6.0, 3.5};
inline constexpr SpeciesTuning crabTuning{0.7, 2.2, 2.0};
inline constexpr SpeciesTuning lizardTuning{1.0, 5.0, 2.5};
inline constexpr SpeciesTuning goatTuning{1.0, 4.0, 4.0};
inline constexpr SpeciesTuning foxTuning{1.4, 6.0, 4.5};

// --- Weather ---

// Weather fronts come from a slow noise field that drifts across the world,
// so rain moves through an area rather than switching on and off.
inline constexpr double weatherBaseFrequency = 1.0 / 650.0;
inline constexpr int weatherOctaves = 3;
inline constexpr double weatherDriftTilesPerSecond = 0.8;

// Cloud cover ramps from nothing to full across this band of noise values.
inline constexpr double weatherCloudStart = 0.05;
inline constexpr double weatherCloudFull = 0.45;

// Dry ground gets little or no rain even under cloud.
inline constexpr double weatherMoistureStart = 0.35;
inline constexpr double weatherMoistureFull = 0.7;

// Colder than this, precipitation falls as snow.
inline constexpr double snowfallTemperature = 0.32;

// The weather target is re-checked this often (it changes slowly anyway).
inline constexpr double weatherSampleIntervalSeconds = 0.5;

// How quickly rain fades in and out. Low, so weather changes feel gradual.
inline constexpr double weatherEaseSharpness = 0.35;

// Rain dims the scene a little.
inline constexpr float overcastDarkening = 0.22f;

inline constexpr int maxRainParticles = 260;
inline constexpr int maxSnowParticles = 200;
inline constexpr double rainFallPixelsPerSecond = 220.0;
inline constexpr double rainWindPixelsPerSecond = -55.0;
inline constexpr int rainStreakLengthInPixels = 4;
inline constexpr double snowFallPixelsPerSecond = 20.0;
inline constexpr double snowSwayPixels = 3.0;
inline constexpr double snowSwaySpeed = 1.3;
inline constexpr Rgb8 rainColor{170, 190, 222};
inline constexpr Rgb8 snowflakeColor{242, 245, 250};

// --- Footprints and ripples ---

// One footprint per stride, alternating feet either side of the path.
inline constexpr double footprintSideOffsetInTiles = 0.12;
inline constexpr double footprintLifetimeSeconds = 12.0;
inline constexpr float footprintDarkening = 0.72f;
inline constexpr int maxFootprints = 80;

inline constexpr double rippleLifetimeSeconds = 1.6;
inline constexpr double rippleMaxRadiusInTiles = 0.9;
inline constexpr int maxRipples = 24;

// Standing still in water still sends out the odd ripple.
inline constexpr double idleRippleIntervalSeconds = 1.2;
inline constexpr Rgb8 rippleColor{176, 214, 228};

// --- Exploration and minimap ---

// Chunks within this many chunks of the player get revealed on the minimap.
inline constexpr int explorationRevealRadiusInChunks = 1;

// Each minimap pixel covers this many tiles. Must divide chunkSizeInTiles,
// because each chunk keeps a tiny summary at exactly that resolution.
inline constexpr int minimapTilesPerPixel = 4;
inline constexpr int minimapSizeInPixels = 72;
inline constexpr int minimapMarginInPixels = 6;

// Summaries for revealed chunks that aren't loaded (after a reload, say)
// are rebuilt from the seed, at most this many per frame.
inline constexpr int minimapSummariesPerFrame = 12;

inline constexpr Rgb8 minimapFogColor{16, 18, 30};
inline constexpr Rgb8 minimapBorderColor{170, 166, 196};
inline constexpr Rgb8 minimapPlayerColor{255, 255, 255};
inline constexpr Rgb8 towerMarkerColor{255, 206, 120};
inline constexpr Rgb8 crystalMarkerColor{150, 240, 240};
inline constexpr Rgb8 hotSpringMarkerColor{120, 214, 214};

// Player dot blinks this many times per second.
inline constexpr double minimapBlinkRate = 1.5;

// --- Journal and discovery ---

// Get this close to a landmark to count as having found it.
inline constexpr double landmarkDiscoveryRadiusInTiles = 6.0;
inline constexpr double discoveryCheckIntervalSeconds = 0.25;

// Stay in a biome this long before its name is shown. Stops the banner
// flickering while you walk along a coastline.
inline constexpr double biomeAnnounceDelaySeconds = 1.2;

// --- Field scanner ---

// Hold the action button near an animal to scan it. While holding, the player
// creeps (sneaks), and animals only startle at a shorter range.
inline constexpr double scanRangeInTiles = 4.0;
inline constexpr double scanDurationSeconds = 1.5;

// Progress drains this many times faster than it builds once a scan is interrupted.
inline constexpr double scanDecayMultiplier = 2.0;

// Walking speed multiplier while the action button is held.
inline constexpr double sneakSpeedMultiplier = 0.4;

// A sneaking player startles animals at this fraction of their usual flee
// radius. Every species' value must stay below scanRangeInTiles, or it could
// never be scanned without scaring it off.
inline constexpr double sneakStartleFraction = 0.55;

inline constexpr Rgb8 scannerColor{120, 235, 255};
inline constexpr Rgb8 scannerLoggedColor{160, 170, 190};
inline constexpr Rgb8 scannerTrackColor{24, 34, 56};

// --- Saving ---

inline constexpr double autosaveIntervalSeconds = 10.0;
inline constexpr int saveFormatVersion = 2;

// Sanity limit when loading, so a corrupt save can't ask for billions of entries.
inline constexpr int maxSavedEntries = 4000000;

}  // namespace wl::constants
