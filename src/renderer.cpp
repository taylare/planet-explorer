#include "renderer.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>

#include "hash.h"
#include "noise.h"

namespace wl {

namespace {

constexpr double twoPi = 6.283185307179586;

// Fixed seeds for screen texture. These are decoration, not world content,
// so they don't change with the world seed.
constexpr std::uint32_t grainPatternSeed = 0x6a09e667u;
constexpr std::uint32_t starPatternSeed = 0xbb67ae85u;
constexpr std::uint32_t weatherParticleSeed = 0x3c6ef372u;

// How strongly rain streaks and snowflakes show over the scene.
constexpr float rainOpacity = 0.5f;
constexpr float snowOpacity = 0.85f;

// Peak opacity of a fresh ripple, and how squashed it is (top-down ellipse).
constexpr float rippleOpacity = 0.6f;
constexpr double rippleSquash = 0.5;
constexpr int minimumRipplePoints = 8;
constexpr double ripplePointsPerPixel = 6.0;

// Number of brightness variants per tile: darker, base, base, lighter.
// Doubling up "base" keeps most pixels at the true colour.
constexpr int grainVariantCount = 4;

// Deep water gets a touch darker than the depth gradient alone, so the
// wading line is readable at a glance.
constexpr float deepOceanDarkening = 0.85f;

// Fixed-point scale for the lighting pass: 256 means "times 1.0".
constexpr int fixedPointOne = 256;

// How far (in tiles) a prop's sprite can reach outside its anchor tile.
// Used to pick which chunks to scan so tall things at the screen edge still draw.
constexpr int propReachSideways = 2;
constexpr int propReachUpward = 3;  // the tower is the tallest

// Below this the scene counts as full daylight and the lighting pass is skipped.
constexpr float nightThreshold = 0.01f;

// The aurora fades out this many band half-heights from its middle.
constexpr double auroraReachInHalfHeights = 1.8;

// Aurora curtains: how fast the ribbon sways and ripples, and its shape. These
// are screen-space wave frequencies and speeds picked by eye.
constexpr double auroraRippleFrequency = 0.075;
constexpr double auroraRippleSpeed = 0.9;
constexpr double auroraSwayFrequency = 0.010;
constexpr double auroraSwaySpeed = 0.12;
constexpr double auroraBandTopFraction = 0.30;  // where the ribbon sits on the screen, from the top

// Below this a blended aurora pixel is too faint to bother drawing.
constexpr float auroraMinimumVisibleOpacity = 0.01f;

struct ColorF {
    float red, green, blue;
};

ColorF toColorF(const constants::Rgb8& color) {
    return {static_cast<float>(color.red), static_cast<float>(color.green), static_cast<float>(color.blue)};
}

ColorF lerpColor(const ColorF& from, const ColorF& to, float t) {
    return {from.red + (to.red - from.red) * t, from.green + (to.green - from.green) * t,
            from.blue + (to.blue - from.blue) * t};
}

std::uint8_t toChannel(float value) { return static_cast<std::uint8_t>(std::clamp(value, 0.0f, 255.0f)); }

PackedColor toPacked(const constants::Rgb8& color) { return packRgba(color.red, color.green, color.blue); }

std::uint8_t channelOf(PackedColor color, int shift) { return static_cast<std::uint8_t>((color >> shift) & 0xffu); }

void darkenPixel(Framebuffer& framebuffer, int x, int y, float factor) {
    if (x < 0 || y < 0 || x >= framebuffer.width() || y >= framebuffer.height()) return;
    PackedColor& pixel = framebuffer.at(x, y);
    pixel = packRgba(static_cast<std::uint8_t>(channelOf(pixel, 0) * factor),
                     static_cast<std::uint8_t>(channelOf(pixel, 8) * factor),
                     static_cast<std::uint8_t>(channelOf(pixel, 16) * factor));
}

// Mixes a pixel toward a colour by amount (0 to 1).
void blendPixel(Framebuffer& framebuffer, int x, int y, const constants::Rgb8& color, float amount) {
    if (x < 0 || y < 0 || x >= framebuffer.width() || y >= framebuffer.height()) return;
    PackedColor& pixel = framebuffer.at(x, y);
    const auto mix = [amount](std::uint8_t from, std::uint8_t to) {
        return static_cast<std::uint8_t>(from + (to - from) * amount);
    };
    pixel = packRgba(mix(channelOf(pixel, 0), color.red), mix(channelOf(pixel, 8), color.green),
                     mix(channelOf(pixel, 16), color.blue));
}

PackedColor packScaled(const ColorF& color, float brightness) {
    return packRgba(toChannel(color.red * brightness), toChannel(color.green * brightness),
                    toChannel(color.blue * brightness));
}

ColorF landColor(const PlanetPalette& palette, Biome biome) {
    switch (biome) {
        case Biome::Beach: return toColorF(palette.beach);
        case Biome::Grassland: return toColorF(palette.grassland);
        case Biome::Forest: return toColorF(palette.forest);
        case Biome::Desert: return toColorF(palette.desert);
        case Biome::Savanna: return toColorF(palette.savanna);
        case Biome::Tundra: return toColorF(palette.tundra);
        case Biome::Snow: return toColorF(palette.snow);
        case Biome::Mountain: return toColorF(palette.mountain);
        default: return toColorF(palette.grassland);
    }
}

// Base colour and brightness for one tile, before per-pixel grain.
void tileColorAndBrightness(const PlanetPalette& palette, Biome biome, float elevation, float hillshade,
                            std::uint32_t tileHash, ColorF& color, float& brightness) {
    using namespace constants;
    const float jitter = (hashToUnitFloat(tileHash) - 0.5f) * static_cast<float>(tileColorJitter);

    if (isWater(biome)) {
        // Colour by depth, not biome, so the sea floor shelves smoothly.
        const float depth = std::clamp(
            static_cast<float>((seaLevelElevation - elevation) / (seaLevelElevation - deepestWaterElevation)), 0.0f,
            1.0f);
        color = lerpColor(toColorF(palette.shallowWater), toColorF(palette.deepWater), depth);
        brightness = 1.0f + jitter * 0.5f;  // water is calmer than land
        if (biome == Biome::DeepOcean) brightness *= deepOceanDarkening;
        return;
    }

    color = landColor(palette, biome);
    if (biome == Biome::Mountain) {
        // Rock lightens toward the peaks so ranges read as height, not just grey.
        const float heightBlend = std::clamp(
            static_cast<float>((elevation - mountainElevation) / (snowCapElevation - mountainElevation)), 0.0f, 1.0f);
        color = lerpColor(color, toColorF(palette.snow), heightBlend * static_cast<float>(mountainHeightTint));
    }
    brightness = 1.0f + hillshade * static_cast<float>(hillshadeContrast) + jitter;
}

std::int64_t floorDivide64(std::int64_t value, std::int64_t divisor) {
    const std::int64_t quotient = value / divisor;
    return (value % divisor != 0 && value < 0) ? quotient - 1 : quotient;
}

// Index into the grain pattern. Casting to unsigned first keeps the
// wrap-around well defined for negative world pixels.
int grainIndex(std::int64_t worldPixel, int patternSize) {
    return static_cast<int>(static_cast<std::uint64_t>(worldPixel) & static_cast<std::uint64_t>(patternSize - 1));
}

// Always in [0, divisor), for wrapping weather particles around the screen.
int floorModulo64(std::int64_t value, int divisor) {
    const std::int64_t remainder = value % divisor;
    return static_cast<int>(remainder < 0 ? remainder + divisor : remainder);
}

PackedColor markerColorFor(PropKind kind) {
    switch (kind) {
        case PropKind::Tower: return toPacked(constants::towerMarkerColor);
        case PropKind::CrystalGrove: return toPacked(constants::crystalMarkerColor);
        default: return toPacked(constants::hotSpringMarkerColor);
    }
}

bool shouldFlipProp(const Prop& prop) {
    // Mirroring half the trees and rocks doubles the visual variety for free.
    // Landmarks and ruins have a "front", so leave them be.
    if (isRareLandmark(prop.kind) || prop.kind == PropKind::Ruins) return false;
    return (prop.variant & 1u) != 0;
}

}  // namespace

ScreenOrigin screenOriginFor(const Camera& camera, int framebufferWidth, int framebufferHeight) {
    ScreenOrigin origin;
    origin.tileSizeInPixels = std::max(camera.tileSizeInPixels, 1);
    origin.leftWorldPixel =
        static_cast<std::int64_t>(std::floor(camera.centerTileX * origin.tileSizeInPixels)) - framebufferWidth / 2;
    origin.topWorldPixel =
        static_cast<std::int64_t>(std::floor(camera.centerTileY * origin.tileSizeInPixels)) - framebufferHeight / 2;
    return origin;
}

int worldToScreenX(const ScreenOrigin& origin, double tileX) {
    return static_cast<int>(static_cast<std::int64_t>(std::floor(tileX * origin.tileSizeInPixels)) -
                            origin.leftWorldPixel);
}

int worldToScreenY(const ScreenOrigin& origin, double tileY) {
    return static_cast<int>(static_cast<std::int64_t>(std::floor(tileY * origin.tileSizeInPixels)) -
                            origin.topWorldPixel);
}

TileBounds visibleTileBounds(const Camera& camera, int framebufferWidth, int framebufferHeight) {
    const ScreenOrigin origin = screenOriginFor(camera, framebufferWidth, framebufferHeight);
    const int tileSize = origin.tileSizeInPixels;

    TileBounds bounds;
    bounds.minTileX = static_cast<int>(floorDivide64(origin.leftWorldPixel, tileSize));
    bounds.minTileY = static_cast<int>(floorDivide64(origin.topWorldPixel, tileSize));
    bounds.maxTileX = static_cast<int>(floorDivide64(origin.leftWorldPixel + framebufferWidth - 1, tileSize));
    bounds.maxTileY = static_cast<int>(floorDivide64(origin.topWorldPixel + framebufferHeight - 1, tileSize));
    return bounds;
}

Renderer::Renderer() {
    for (int y = 0; y < grainPatternSize; ++y) {
        for (int x = 0; x < grainPatternSize; ++x) {
            grainPattern_[y * grainPatternSize + x] =
                static_cast<std::uint8_t>(hashCoords(grainPatternSeed, x, y) % grainVariantCount);
        }
    }

    for (int step = 0; step < auroraFalloffSteps; ++step) {
        // The table covers the distance from the band's middle out to auroraReachInHalfHeights.
        const double offset = auroraReachInHalfHeights * step / (auroraFalloffSteps - 1);
        auroraFalloff_[step] = static_cast<float>(std::exp(-offset * offset));
    }
    rebuildMinimapPalette();
}

void Renderer::rebuildMinimapPalette() {
    // The minimap uses flat biome colours: no shading, it's a map.
    for (int index = 0; index < biomeCount; ++index) {
        const Biome biome = static_cast<Biome>(index);
        if (biome == Biome::DeepOcean) {
            minimapPalette_[index] = toPacked(palette_.deepWater);
        } else if (biome == Biome::ShallowWater) {
            minimapPalette_[index] = toPacked(palette_.shallowWater);
        } else {
            minimapPalette_[index] = packScaled(landColor(palette_, biome), 1.0f);
        }
    }
}

void Renderer::setPalette(const PlanetPalette& palette) {
    palette_ = palette;
    sprites_ = SpriteLibrary(palette);
    rebuildMinimapPalette();
}

void Renderer::drawTrails(Framebuffer& framebuffer, const SceneView& scene, const ScreenOrigin& origin,
                          int scale) const {
    using namespace constants;
    const TrailEffects& trails = *scene.trails;

    for (const Footprint& footprint : trails.footprints()) {
        const double age = scene.timeSeconds - footprint.bornSeconds;
        if (age < 0.0 || age > footprintLifetimeSeconds) continue;
        // Fresh prints are darkest and fade back into the ground.
        const float freshness = static_cast<float>(1.0 - age / footprintLifetimeSeconds);
        const float factor = 1.0f - (1.0f - footprintDarkening) * freshness;
        const int x = worldToScreenX(origin, footprint.tileX);
        const int y = worldToScreenY(origin, footprint.tileY);
        for (int offsetY = 0; offsetY < scale; ++offsetY) {
            for (int offsetX = 0; offsetX < scale; ++offsetX) darkenPixel(framebuffer, x + offsetX, y + offsetY, factor);
        }
    }

    for (const Ripple& ripple : trails.ripples()) {
        const double age = scene.timeSeconds - ripple.bornSeconds;
        if (age < 0.0 || age > rippleLifetimeSeconds) continue;
        const double progress = age / rippleLifetimeSeconds;
        const double radius = progress * rippleMaxRadiusInTiles * origin.tileSizeInPixels;
        const float opacity = rippleOpacity * static_cast<float>(1.0 - progress);
        const int centreX = worldToScreenX(origin, ripple.tileX);
        const int centreY = worldToScreenY(origin, ripple.tileY);
        const int points = std::max(minimumRipplePoints, static_cast<int>(radius * ripplePointsPerPixel));
        for (int point = 0; point < points; ++point) {
            const double angle = twoPi * point / points;
            blendPixel(framebuffer, centreX + static_cast<int>(std::lround(std::cos(angle) * radius)),
                       centreY + static_cast<int>(std::lround(std::sin(angle) * radius * rippleSquash)), rippleColor,
                       opacity);
        }
    }
}

void Renderer::drawWeather(Framebuffer& framebuffer, const SceneView& scene, const ScreenOrigin& origin,
                           int scale) const {
    using namespace constants;
    const WeatherState& weather = *scene.weather;
    if (weather.kind == Precipitation::None || weather.intensity <= 0.0) return;

    const int width = framebuffer.width();
    const int height = framebuffer.height();
    if (weather.kind == Precipitation::DustStorm) {
        drawDustStorm(framebuffer, scene, origin, scale);
        return;
    }
    const bool raining = weather.kind == Precipitation::Rain;
    const int particleCount =
        static_cast<int>(weather.intensity * (raining ? maxRainParticles : maxSnowParticles));
    const double time = scene.timeSeconds;
    const double rainSlant = rainWindPixelsPerSecond / rainFallPixelsPerSecond;

    // Particles have no state: each one's position is a pure function of its
    // index and the time. They're anchored to world pixels and wrapped around
    // the screen, so they stay put in the world as the camera moves.
    for (int index = 0; index < particleCount; ++index) {
        const std::uint32_t baseHash = hashCoords(weatherParticleSeed, index, 0);
        const double startX = hashToUnitFloat(baseHash) * width;
        const double startY = hashToUnitFloat(mixBits32(baseHash + 1)) * height;
        const double speedScale = 0.75 + 0.5 * hashToUnitFloat(mixBits32(baseHash + 2));

        if (raining) {
            const double worldX = startX + rainWindPixelsPerSecond * time * speedScale;
            const double worldY = startY + rainFallPixelsPerSecond * time * speedScale;
            const int screenX = floorModulo64(static_cast<std::int64_t>(std::floor(worldX)) - origin.leftWorldPixel, width);
            const int screenY = floorModulo64(static_cast<std::int64_t>(std::floor(worldY)) - origin.topWorldPixel, height);
            // A short streak trailing behind the drop.
            for (int step = 0; step < rainStreakLengthInPixels * scale; ++step) {
                blendPixel(framebuffer, screenX - static_cast<int>(std::lround(step * rainSlant)), screenY - step,
                           rainColor, rainOpacity);
            }
        } else {
            const double swayPhase = hashToUnitFloat(mixBits32(baseHash + 3)) * twoPi;
            const double worldX = startX + std::sin(time * snowSwaySpeed + swayPhase) * snowSwayPixels * scale;
            const double worldY = startY + snowFallPixelsPerSecond * time * speedScale;
            const int screenX = floorModulo64(static_cast<std::int64_t>(std::floor(worldX)) - origin.leftWorldPixel, width);
            const int screenY = floorModulo64(static_cast<std::int64_t>(std::floor(worldY)) - origin.topWorldPixel, height);
            for (int offsetY = 0; offsetY < scale; ++offsetY) {
                for (int offsetX = 0; offsetX < scale; ++offsetX) {
                    blendPixel(framebuffer, screenX + offsetX, screenY + offsetY, snowflakeColor, snowOpacity);
                }
            }
        }
    }
}

// Lightens a colour toward white by a fraction.
constants::Rgb8 lightened(const constants::Rgb8& color, float fraction) {
    const auto lift = [fraction](std::uint8_t channel) {
        return static_cast<std::uint8_t>(channel + (255 - channel) * fraction);
    };
    return {lift(color.red), lift(color.green), lift(color.blue)};
}

// A dust storm: a haze over the whole view that is thickest at the edges, with fast streaks of
// grit blowing across. Both are tinted from the planet's desert colour so an alien world gets
// alien dust. Like rain, every speck is a pure function of its index and the time.
void Renderer::drawDustStorm(Framebuffer& framebuffer, const SceneView& scene, const ScreenOrigin& origin,
                             int scale) const {
    using namespace constants;
    const double intensity = scene.weather->intensity;
    const int width = framebuffer.width();
    const int height = framebuffer.height();
    const Rgb8 hazeColor = palette_.desert;

    // Haze: mix every pixel toward the haze colour, a little more strongly the further from the middle.
    const double halfWidth = std::max(width / 2.0, 1.0);
    const double halfHeight = std::max(height / 2.0, 1.0);
    for (int y = 0; y < height; ++y) {
        const double offsetY = (y + 0.5 - halfHeight) / halfHeight;
        const double edgeY = offsetY * offsetY;
        for (int x = 0; x < width; ++x) {
            const double offsetX = (x + 0.5 - halfWidth) / halfWidth;
            const double edge = std::min(1.0, 0.5 * (offsetX * offsetX + edgeY));
            const float amount = static_cast<float>(intensity * (dustHazeOpacity + dustVignetteExtraOpacity * edge));
            PackedColor& pixel = framebuffer.at(x, y);
            const auto mix = [amount](std::uint8_t from, std::uint8_t to) {
                return static_cast<std::uint8_t>(from + (to - from) * amount);
            };
            pixel = packRgba(mix(channelOf(pixel, 0), hazeColor.red), mix(channelOf(pixel, 8), hazeColor.green),
                             mix(channelOf(pixel, 16), hazeColor.blue));
        }
    }

    // Grit: short horizontal streaks blown by the wind, wobbling a little as they go.
    const Rgb8 gritColor = lightened(hazeColor, dustParticleLightening);
    const int particleCount = static_cast<int>(intensity * maxDustParticles);
    const double time = scene.timeSeconds;
    for (int index = 0; index < particleCount; ++index) {
        const std::uint32_t baseHash = hashCoords(weatherParticleSeed + 7u, index, 0);
        const double startX = hashToUnitFloat(baseHash) * width;
        const double startY = hashToUnitFloat(mixBits32(baseHash + 1)) * height;
        const double speedScale = 0.7 + 0.6 * hashToUnitFloat(mixBits32(baseHash + 2));
        const double wobblePhase = hashToUnitFloat(mixBits32(baseHash + 3)) * twoPi;

        const double worldX = startX + dustWindPixelsPerSecond * time * speedScale;
        const double worldY = startY + std::sin(time * 2.0 + wobblePhase) * 3.0 * scale;
        const int screenX = floorModulo64(static_cast<std::int64_t>(std::floor(worldX)) - origin.leftWorldPixel, width);
        const int screenY = floorModulo64(static_cast<std::int64_t>(std::floor(worldY)) - origin.topWorldPixel, height);
        for (int step = 0; step < dustStreakLengthInPixels * scale; ++step) {
            // The streak trails to the right of the head, since the wind blows toward the left.
            blendPixel(framebuffer, screenX + step, screenY, gritColor, dustParticleOpacity);
        }
    }
}

// Meteors streak across the sky. They are drawn like the moons and stars: bright on water, where
// the sky is reflected, and only faintly on land.
void Renderer::drawMeteors(Framebuffer& framebuffer, const SceneView& scene) const {
    using namespace constants;
    if (!scene.meteorShower || scene.lighting.nightAmount < meteorMinimumNightAmount) return;

    const int width = framebuffer.width();
    const int height = framebuffer.height();
    const double time = scene.timeSeconds;
    const std::int64_t slot = meteorSlotAt(time);
    const double timeInSlot = time - meteorSlotStartSeconds(slot);

    for (int index = 0; index < meteorsPerSlot; ++index) {
        const MeteorStreak streak = meteorStreakFor(scene.skySeed, scene.nightNumber, slot, index);
        if (!streak.present) continue;
        const double flightTime = timeInSlot - streak.startOffsetSeconds;
        if (flightTime < 0.0 || flightTime > meteorDurationSeconds) continue;

        const double progress = flightTime / meteorDurationSeconds;
        // Fade in and out so the streak does not pop on or off.
        const float fade = static_cast<float>(std::sin(progress * 3.14159265358979));
        const double headX = streak.startX * width + streak.directionX * meteorSpeedPixelsPerSecond * flightTime;
        const double headY = streak.startY * height + streak.directionY * meteorSpeedPixelsPerSecond * flightTime;

        for (int step = 0; step < meteorTailLengthInPixels; ++step) {
            const int x = static_cast<int>(std::lround(headX - streak.directionX * step));
            const int y = static_cast<int>(std::lround(headY - streak.directionY * step));
            if (x < 0 || y < 0 || x >= width || y >= height) continue;
            const bool overWater = waterMask_.size() == static_cast<std::size_t>(width) * height &&
                                   waterMask_[static_cast<std::size_t>(y) * width + x] != 0;
            const float tail = 1.0f - static_cast<float>(step) / meteorTailLengthInPixels;
            const float opacity = (overWater ? meteorWaterOpacity : meteorLandOpacity) * tail * fade *
                                  scene.lighting.nightAmount;
            blendPixel(framebuffer, x, y, meteorColor, opacity);
            // The head is two pixels wide so it reads as a bright point rather than a hairline.
            if (step < meteorTailLengthInPixels / 3) blendPixel(framebuffer, x + 1, y, meteorColor, opacity);
        }
    }
}

// Lightning lights the whole scene for an instant: everything is mixed toward a cold white.
void Renderer::drawLightningFlash(Framebuffer& framebuffer, float flash) const {
    using namespace constants;
    if (flash <= 0.0f) return;
    const float amount = std::clamp(flash, 0.0f, 1.0f) * lightningMaxBlend;
    const int pixelCount = framebuffer.width() * framebuffer.height();
    PackedColor* pixels = framebuffer.pixels();
    const auto mix = [amount](std::uint8_t from, std::uint8_t to) {
        return static_cast<std::uint8_t>(from + (to - from) * amount);
    };
    for (int index = 0; index < pixelCount; ++index) {
        const PackedColor pixel = pixels[index];
        pixels[index] = packRgba(mix(channelOf(pixel, 0), lightningColor.red), mix(channelOf(pixel, 8), lightningColor.green),
                                 mix(channelOf(pixel, 16), lightningColor.blue));
    }
}

void Renderer::drawMinimap(Framebuffer& framebuffer, const SceneView& scene) const {
    using namespace constants;
    const MinimapView& view = *scene.minimap;
    if (!view.exploration) return;

    const int width = framebuffer.width();
    const int height = framebuffer.height();
    const int size = std::min(minimapSizeInPixels, std::min(width, height) - 2 * minimapMarginInPixels);
    constexpr int smallestUsefulSize = 16;
    if (size < smallestUsefulSize) return;

    const int left = minimapMarginInPixels;
    const int top = minimapMarginInPixels;
    const int half = size / 2;
    const int centreTileX = static_cast<int>(std::floor(scene.player.tileX));
    const int centreTileY = static_cast<int>(std::floor(scene.player.tileY));
    const PackedColor fog = toPacked(minimapFogColor);

    // Same trick as the terrain: neighbouring pixels share a chunk, so cache the lookup.
    ChunkCoord cachedCoord{};
    const MinimapSummary* cachedSummary = nullptr;
    bool hasCached = false;

    for (int pixelY = 0; pixelY < size; ++pixelY) {
        const int tileY = centreTileY + (pixelY - half) * minimapTilesPerPixel;
        PackedColor* row = &framebuffer.at(0, top + pixelY);
        for (int pixelX = 0; pixelX < size; ++pixelX) {
            const int tileX = centreTileX + (pixelX - half) * minimapTilesPerPixel;
            const ChunkCoord coord = chunkCoordForTile(tileX, tileY);
            if (!hasCached || coord != cachedCoord) {
                cachedCoord = coord;
                cachedSummary = view.exploration->isRevealed(coord) ? view.exploration->summary(coord) : nullptr;
                hasCached = true;
            }

            PackedColor color = fog;
            if (cachedSummary) {
                const int summaryX = floorModulo(tileX, chunkSizeInTiles) / minimapTilesPerPixel;
                const int summaryY = floorModulo(tileY, chunkSizeInTiles) / minimapTilesPerPixel;
                color = minimapPalette_[static_cast<int>((*cachedSummary)[summaryY * minimapSummarySide + summaryX])];
            }
            row[left + pixelX] = color;
        }
    }

    // Thin frame around the map.
    const PackedColor border = toPacked(minimapBorderColor);
    for (int offset = -1; offset <= size; ++offset) {
        framebuffer.setPixelClipped(left + offset, top - 1, border);
        framebuffer.setPixelClipped(left + offset, top + size, border);
        framebuffer.setPixelClipped(left - 1, top + offset, border);
        framebuffer.setPixelClipped(left + size, top + offset, border);
    }

    const auto drawDot = [&](int x, int y, PackedColor color) {
        for (int offsetY = 0; offsetY < 2; ++offsetY) {
            for (int offsetX = 0; offsetX < 2; ++offsetX) {
                const int dotX = x + offsetX;
                const int dotY = y + offsetY;
                if (dotX >= left && dotX < left + size && dotY >= top && dotY < top + size) {
                    framebuffer.at(dotX, dotY) = color;
                }
            }
        }
    };

    for (const MinimapMarker& marker : view.markers) {
        const int markerX = left + half + static_cast<int>(std::floor((marker.tileX - centreTileX) / minimapTilesPerPixel));
        const int markerY = top + half + static_cast<int>(std::floor((marker.tileY - centreTileY) / minimapTilesPerPixel));
        drawDot(markerX, markerY, markerColorFor(marker.kind));
    }

    // Blinking player dot so it's easy to spot against white snow.
    const double blinkPhase = scene.timeSeconds * minimapBlinkRate;
    if (blinkPhase - std::floor(blinkPhase) < 0.6) drawDot(left + half - 1, top + half - 1, toPacked(minimapPlayerColor));
}

void Renderer::drawTerrain(Framebuffer& framebuffer, const Camera& camera, const ChunkSource& chunks,
                           double timeSeconds) {
    using namespace constants;
    const int width = framebuffer.width();
    const int height = framebuffer.height();

    const ScreenOrigin origin = screenOriginFor(camera, width, height);
    const int tileSize = origin.tileSizeInPixels;
    const std::int64_t leftWorldPixel = origin.leftWorldPixel;
    const std::int64_t topWorldPixel = origin.topWorldPixel;
    const TileBounds bounds = visibleTileBounds(camera, width, height);

    waterMask_.assign(static_cast<std::size_t>(width) * height, 0);

    // Water shimmer: shift the grain sideways over time, by a slightly
    // different amount on each row. Cheap, and reads as gentle ripples.
    waterRowOffsets_.resize(height);
    const double drift = timeSeconds * waterDriftPixelsPerSecond;
    for (int pixelY = 0; pixelY < height; ++pixelY) {
        const double wobble = std::sin(timeSeconds * waterWobbleSpeed +
                                       static_cast<double>(topWorldPixel + pixelY) * waterWobbleRowFrequency) *
                              waterWobblePixels;
        waterRowOffsets_[pixelY] = static_cast<int>(std::floor(drift + wobble));
    }

    const float grain = static_cast<float>(pixelGrainStrength);
    const float grainMultipliers[grainVariantCount] = {1.0f - grain, 1.0f, 1.0f, 1.0f + grain};
    const ColorF unloaded = toColorF(unloadedColor);

    // Most neighbouring tiles share a chunk, so remember the last lookup.
    const Chunk* currentChunk = nullptr;
    ChunkCoord currentChunkCoord{};
    bool hasCurrentChunk = false;

    for (int tileY = bounds.minTileY; tileY <= bounds.maxTileY; ++tileY) {
        const int screenTop = static_cast<int>(static_cast<std::int64_t>(tileY) * tileSize - topWorldPixel);
        const int rowStart = std::max(0, screenTop);
        const int rowEnd = std::min(height, screenTop + tileSize);

        for (int tileX = bounds.minTileX; tileX <= bounds.maxTileX; ++tileX) {
            const int screenLeft = static_cast<int>(static_cast<std::int64_t>(tileX) * tileSize - leftWorldPixel);
            const int columnStart = std::max(0, screenLeft);
            const int columnEnd = std::min(width, screenLeft + tileSize);

            const ChunkCoord chunkCoord = chunkCoordForTile(tileX, tileY);
            if (!hasCurrentChunk || chunkCoord != currentChunkCoord) {
                currentChunk = chunks.findChunk(chunkCoord);
                currentChunkCoord = chunkCoord;
                hasCurrentChunk = true;
            }

            ColorF baseColor = unloaded;
            float brightness = 1.0f;
            bool water = false;
            if (currentChunk) {
                const int tileIndex = Chunk::indexFor(floorModulo(tileX, chunkSizeInTiles),
                                                      floorModulo(tileY, chunkSizeInTiles));
                const Biome biome = currentChunk->biomes[tileIndex];
                water = isWater(biome);
                tileColorAndBrightness(palette_, biome, currentChunk->elevations[tileIndex],
                                       currentChunk->hillshades[tileIndex], hashCoords(0, tileX, tileY), baseColor,
                                       brightness);
            }

            PackedColor variants[grainVariantCount];
            for (int variant = 0; variant < grainVariantCount; ++variant) {
                variants[variant] = packScaled(baseColor, brightness * grainMultipliers[variant]);
            }

            for (int pixelY = rowStart; pixelY < rowEnd; ++pixelY) {
                const int grainRow = grainIndex(topWorldPixel + pixelY, grainPatternSize) * grainPatternSize;
                const int shimmer = water ? waterRowOffsets_[pixelY] : 0;
                PackedColor* row = &framebuffer.at(0, pixelY);
                for (int pixelX = columnStart; pixelX < columnEnd; ++pixelX) {
                    const int grainColumn = grainIndex(leftWorldPixel + pixelX + shimmer, grainPatternSize);
                    row[pixelX] = variants[grainPattern_[grainRow + grainColumn]];
                }
                if (water) {
                    std::fill(waterMask_.begin() + static_cast<std::ptrdiff_t>(pixelY) * width + columnStart,
                              waterMask_.begin() + static_cast<std::ptrdiff_t>(pixelY) * width + columnEnd, 1);
                }
            }
        }
    }
}

void Renderer::collectProps(Framebuffer& framebuffer, const SceneView& scene, const ScreenOrigin& origin, int scale) {
    using namespace constants;
    const int width = framebuffer.width();
    const int height = framebuffer.height();
    const TileBounds bounds = visibleTileBounds(scene.camera, width, height);

    // Props anchor at their base, so scan a little past the screen, mostly
    // below it, to catch tall sprites poking up into view.
    const ChunkCoord minChunk = chunkCoordForTile(bounds.minTileX - propReachSideways, bounds.minTileY - 1);
    const ChunkCoord maxChunk =
        chunkCoordForTile(bounds.maxTileX + propReachSideways, bounds.maxTileY + propReachUpward);

    const int animationFrame = static_cast<int>(scene.timeSeconds * campfireFramesPerSecond);
    const auto onScreen = [width, height](int left, int top, int spriteWidth, int spriteHeight) {
        return left < width && top < height && left + spriteWidth > 0 && top + spriteHeight > 0;
    };

    for (int chunkY = minChunk.y; chunkY <= maxChunk.y; ++chunkY) {
        for (int chunkX = minChunk.x; chunkX <= maxChunk.x; ++chunkX) {
            const Chunk* chunk = scene.chunks->findChunk({chunkX, chunkY});
            if (!chunk) continue;

            for (const Prop& prop : chunk->props) {
                const int footX = worldToScreenX(origin, prop.tileX);
                const int footY = worldToScreenY(origin, prop.tileY);
                const Sprite& sprite = sprites_.prop(prop, animationFrame);
                const int spriteWidth = sprite.width * scale;
                const int spriteHeight = sprite.height * scale;

                if (SpriteLibrary::isGroundDecal(prop.kind)) {
                    // Flat things go down right away, under everything that stands up.
                    const int left = footX - spriteWidth / 2;
                    const int top = footY - spriteHeight / 2;
                    if (!onScreen(left, top, spriteWidth, spriteHeight)) continue;
                    SpriteDrawOptions options;
                    options.scale = scale;
                    options.flipHorizontally = shouldFlipProp(prop);
                    drawSprite(framebuffer, sprite, left, top, options);
                    if (prop.kind == PropKind::HotSpring) steamSources_.push_back({footX, footY, prop.variant});
                    continue;
                }

                const int left = footX - spriteWidth / 2;
                const int top = footY - (sprite.height - 1) * scale;

                // Lights first: a fire just off screen still glows onto it.
                if (prop.kind == PropKind::Campfire) {
                    const double flickerNoise = simplexNoise2D(prop.variant, scene.timeSeconds * fireFlickerSpeed, 0.0);
                    const float flicker = 1.0f - fireFlickerAmount * static_cast<float>(0.5 + 0.5 * flickerNoise);
                    lights_.push_back({footX, footY - 2 * scale, &campfireLight, flicker});
                } else if (prop.kind == PropKind::CrystalGrove) {
                    const float pulse = 0.85f + 0.15f * static_cast<float>(std::sin(scene.timeSeconds * 1.5 + prop.variant));
                    lights_.push_back({footX, footY - 3 * scale, &crystalLight, pulse});
                } else if (prop.kind == PropKind::Tower) {
                    for (int windowRow : SpriteLibrary::towerWindowRows) {
                        lights_.push_back({left + SpriteLibrary::towerWindowColumn * scale, top + windowRow * scale,
                                           &towerWindowLight, 1.0f});
                    }
                }

                if (!onScreen(left, top, spriteWidth, spriteHeight)) continue;
                DrawItem item;
                item.sprite = &sprite;
                item.footX = footX;
                item.footY = footY;
                item.scale = scale;
                item.flip = shouldFlipProp(prop);
                item.shadow = prop.kind != PropKind::Ruins;
                drawItems_.push_back(item);
            }
        }
    }
}

void Renderer::collectCreatures(const SceneView& scene, const ScreenOrigin& origin, int scale) {
    for (const Creature& creature : *scene.creatures) {
        const Vec2 position = creature.interpolatedPosition(scene.interpolationAlpha);
        DrawItem item;
        item.sprite = &sprites_.creature(creature.species);
        item.footX = worldToScreenX(origin, position.x);
        item.footY = worldToScreenY(origin, position.y);
        item.scale = scale;
        item.flip = creature.facingLeft;

        // A one-pixel hop on alternate strides reads as walking at this size.
        const bool moving = std::hypot(creature.velocity.x, creature.velocity.y) > constants::idleSpeedTilesPerSecond;
        const int stride = static_cast<int>(creature.distanceMoved / constants::creatureStrideInTiles);
        if (moving && stride % 2 == 1) item.footY -= scale;
        drawItems_.push_back(item);
    }
}

void Renderer::drawItem(Framebuffer& framebuffer, const DrawItem& item) const {
    const Sprite& sprite = *item.sprite;
    const int scale = item.scale;
    const int left = item.footX - (sprite.width * scale) / 2;
    // Bottom row is outline, so the last art row sits right on the foot point.
    const int top = item.footY - (sprite.height - 1) * scale;

    SpriteDrawOptions options;
    options.scale = scale;
    options.flipHorizontally = item.flip;

    if (item.wading) {
        // Hide the legs and draw a light waterline across the body.
        options.visibleRows = sprite.height - 1 - constants::wadingHiddenRows;
        drawSprite(framebuffer, sprite, left, top, options);
        const PackedColor waterline = toPacked(constants::waterlineColor);
        const int waterlineY = top + options.visibleRows * scale;
        for (int x = left; x < left + sprite.width * scale; ++x) framebuffer.setPixelClipped(x, waterlineY, waterline);
        return;
    }

    if (item.shadow) {
        // A flat strip with clipped ends, just under the feet.
        const int shadowHalfWidth = (sprite.width * scale) / 2 - scale;
        for (int offsetY = 0; offsetY < scale; ++offsetY) {
            for (int offsetX = -shadowHalfWidth + 1; offsetX < shadowHalfWidth; ++offsetX) {
                darkenPixel(framebuffer, item.footX + offsetX, item.footY + offsetY, constants::spriteShadowStrength);
            }
        }
    }
    drawSprite(framebuffer, sprite, left, top, options);
}

void Renderer::drawSteam(Framebuffer& framebuffer, double timeSeconds, int scale) const {
    using namespace constants;
    for (const auto& source : steamSources_) {
        const int sourceX = source[0];
        const int sourceY = source[1];
        const int variant = source[2];
        for (int particle = 0; particle < steamParticleCount; ++particle) {
            // Each particle loops from the water up to its full height, fading as it rises.
            const double cycle = timeSeconds / steamRiseSeconds + static_cast<double>(particle) / steamParticleCount +
                                 variant / 256.0;
            const double life = cycle - std::floor(cycle);
            const int spreadPixels = static_cast<int>(hashCoords(variant, particle, 0) % 12) - 6;
            const int x = sourceX + spreadPixels * scale + static_cast<int>(std::sin(timeSeconds * 2.0 + particle) * scale);
            const int y = sourceY - static_cast<int>(life * steamRiseHeightInPixels * scale);
            const float strength = static_cast<float>((1.0 - life) * 0.7);
            for (int offsetY = 0; offsetY < scale; ++offsetY) {
                for (int offsetX = 0; offsetX < scale; ++offsetX) {
                    blendPixel(framebuffer, x + offsetX, y + offsetY, steamColor, strength);
                }
            }
        }
    }
}

void Renderer::applyLighting(Framebuffer& framebuffer, const LightingState& lighting, int tileSize) {
    const bool fullDaylight = lighting.nightAmount <= nightThreshold && lighting.tintRed >= 0.999f &&
                              lighting.tintGreen >= 0.999f && lighting.tintBlue >= 0.999f;
    if (fullDaylight) return;

    const int width = framebuffer.width();
    const int height = framebuffer.height();
    const std::size_t pixelCount = static_cast<std::size_t>(width) * height;
    const bool useLights = lighting.nightAmount > nightThreshold && !lights_.empty();

    if (useLights) {
        // Lights only touch small patches, so instead of wiping the whole
        // buffer (1.4 MB at full size) every frame, wipe just the patches
        // that were lit last time.
        // Rotating a phone swaps width and height but keeps the pixel count,
        // so check the width too or old patches would be wiped in the wrong place.
        if (lightBuffer_.size() != pixelCount * 3 || lightBufferWidth_ != width) {
            lightBuffer_.assign(pixelCount * 3, 0);
            lightBufferWidth_ = width;
            litRects_.clear();
        }
        for (const LitRect& rect : litRects_) {
            for (int y = rect.minY; y <= rect.maxY; ++y) {
                std::uint16_t* rowStart = &lightBuffer_[(static_cast<std::size_t>(y) * width + rect.minX) * 3];
                std::fill(rowStart, rowStart + (rect.maxX - rect.minX + 1) * 3, std::uint16_t{0});
            }
        }
        litRects_.clear();

        for (const PendingLight& light : lights_) {
            const constants::LightSettings& settings = *light.settings;
            const int radius = static_cast<int>(settings.radiusInTiles * light.radiusScale * tileSize);
            const double radiusSquared = static_cast<double>(radius) * radius;
            // Lights fade in with the dark, so they don't pop on at sunset.
            const float amount = settings.intensity * light.strength * lighting.nightAmount * fixedPointOne;

            const int minX = std::max(0, light.x - radius);
            const int maxX = std::min(width - 1, light.x + radius);
            const int minY = std::max(0, light.y - radius);
            const int maxY = std::min(height - 1, light.y + radius);
            if (minX > maxX || minY > maxY) continue;  // entirely off screen
            litRects_.push_back({minX, minY, maxX, maxY});
            for (int y = minY; y <= maxY; ++y) {
                for (int x = minX; x <= maxX; ++x) {
                    const double deltaX = x - light.x;
                    const double deltaY = y - light.y;
                    const double distanceSquared = deltaX * deltaX + deltaY * deltaY;
                    if (distanceSquared >= radiusSquared) continue;
                    // Squared falloff: bright core, long soft edge.
                    double falloff = 1.0 - distanceSquared / radiusSquared;
                    falloff *= falloff;
                    const float contribution = static_cast<float>(falloff) * amount;
                    std::uint16_t* pixelLight = &lightBuffer_[(static_cast<std::size_t>(y) * width + x) * 3];
                    const auto addChannel = [contribution](std::uint16_t& channel, float colour) {
                        channel = static_cast<std::uint16_t>(std::min(65535.0f, channel + contribution * colour));
                    };
                    addChannel(pixelLight[0], settings.red);
                    addChannel(pixelLight[1], settings.green);
                    addChannel(pixelLight[2], settings.blue);
                }
            }
        }
    }

    // Fixed-point multiply per channel. Lights add to the tint, capped a bit above 1.0 for a glow.
    const int tintRed = static_cast<int>(lighting.tintRed * fixedPointOne);
    const int tintGreen = static_cast<int>(lighting.tintGreen * fixedPointOne);
    const int tintBlue = static_cast<int>(lighting.tintBlue * fixedPointOne);
    const int maxMultiplier = static_cast<int>(constants::maxLitBrightness * fixedPointOne);

    PackedColor* pixels = framebuffer.pixels();
    for (std::size_t index = 0; index < pixelCount; ++index) {
        int multiplierRed = tintRed;
        int multiplierGreen = tintGreen;
        int multiplierBlue = tintBlue;
        if (useLights) {
            multiplierRed = std::min(maxMultiplier, multiplierRed + lightBuffer_[index * 3]);
            multiplierGreen = std::min(maxMultiplier, multiplierGreen + lightBuffer_[index * 3 + 1]);
            multiplierBlue = std::min(maxMultiplier, multiplierBlue + lightBuffer_[index * 3 + 2]);
        }
        const PackedColor pixel = pixels[index];
        const auto grade = [](std::uint8_t channel, int multiplier) {
            return static_cast<std::uint8_t>(std::min(255, (channel * multiplier) / fixedPointOne));
        };
        pixels[index] = packRgba(grade(channelOf(pixel, 0), multiplierRed), grade(channelOf(pixel, 8), multiplierGreen),
                                 grade(channelOf(pixel, 16), multiplierBlue));
    }
}

void Renderer::rebuildStarsIfNeeded(int width, int height) {
    if (width == starsWidth_ && height == starsHeight_) return;
    starsWidth_ = width;
    starsHeight_ = height;
    stars_.clear();
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const std::uint32_t hash = hashCoords(starPatternSeed, x, y);
            if (hashToUnitFloat(hash) >= constants::starDensity) continue;
            stars_.push_back({x, y, static_cast<float>(hashToUnitFloat(mixBits32(hash)) * twoPi)});
        }
    }
}

void Renderer::drawStars(Framebuffer& framebuffer, const LightingState& lighting, double timeSeconds) {
    if (lighting.nightAmount <= nightThreshold) return;
    rebuildStarsIfNeeded(framebuffer.width(), framebuffer.height());

    // Stars are fixed to the screen, not the world: a reflection of the sky
    // stays put as you walk while the water slides past underneath.
    for (const Star& star : stars_) {
        if (!waterMask_[static_cast<std::size_t>(star.y) * framebuffer.width() + star.x]) continue;
        const float twinkle =
            0.5f + 0.5f * static_cast<float>(std::sin(timeSeconds * constants::starTwinkleSpeed + star.phase));
        blendPixel(framebuffer, star.x, star.y, constants::starColor, lighting.nightAmount * twinkle);
    }
}

void Renderer::drawMoons(Framebuffer& framebuffer, const PlanetProfile& planet, const LightingState& lighting,
                         double timeSeconds) const {
    using namespace constants;
    if (lighting.nightAmount <= nightThreshold) return;

    const int width = framebuffer.width();
    const int height = framebuffer.height();
    const float nightOpacity = lighting.nightAmount * moonReflectionOpacity;

    for (int moonIndex = 0; moonIndex < planet.moonCount; ++moonIndex) {
        const Moon& moon = planet.moons[moonIndex];
        const double driftedX = moon.startX * width + timeSeconds * moonDriftPixelsPerSecond * moon.driftSpeed;
        const int centreX = floorModulo64(static_cast<std::int64_t>(std::floor(driftedX)), width);
        const int centreY = static_cast<int>(moon.startY * (height - 1));
        const int coreRadius = moon.radiusInPixels;
        const int haloRadius = coreRadius * 2;
        const double coreRadiusSquared = static_cast<double>(coreRadius) * coreRadius;
        const double haloRadiusSquared = static_cast<double>(haloRadius) * haloRadius;

        for (int offsetY = -haloRadius; offsetY <= haloRadius; ++offsetY) {
            const int y = centreY + offsetY;
            if (y < 0 || y >= height) continue;
            // Each row shifts sideways like the water surface does, so the glint shimmers.
            const int rowWobble = static_cast<int>(
                std::lround(std::sin(timeSeconds * waterWobbleSpeed + y * waterWobbleRowFrequency) * waterWobblePixels));
            for (int offsetX = -haloRadius; offsetX <= haloRadius; ++offsetX) {
                const int x = centreX + offsetX + rowWobble;
                if (x < 0 || x >= width) continue;
                if (!waterMask_[static_cast<std::size_t>(y) * width + x]) continue;  // moons only show on water

                const double distanceSquared = static_cast<double>(offsetX) * offsetX + static_cast<double>(offsetY) * offsetY;
                if (distanceSquared > haloRadiusSquared) continue;
                float amount = 1.0f;
                if (distanceSquared > coreRadiusSquared) {
                    const double haloProgress = 1.0 - (distanceSquared - coreRadiusSquared) / (haloRadiusSquared - coreRadiusSquared);
                    amount = 0.5f * static_cast<float>(haloProgress * haloProgress);
                }
                blendPixel(framebuffer, x, y, moon.color, nightOpacity * amount);
            }
        }
    }
}

void Renderer::drawAurora(Framebuffer& framebuffer, const PlanetProfile& planet, const LightingState& lighting,
                          double timeSeconds) const {
    using namespace constants;
    if (!planet.hasAurora() || lighting.nightAmount <= nightThreshold) return;

    const int width = framebuffer.width();
    const int height = framebuffer.height();
    const float strength = lighting.nightAmount * static_cast<float>(planet.auroraStrength) * auroraOpacity;
    const double bandHalfHeight = height * auroraBandHalfHeightFraction;

    for (int x = 0; x < width; ++x) {
        // Brightness ripples along the ribbon, and the ribbon itself sways up and down.
        const double curtain =
            0.5 + 0.5 * std::sin(x * auroraRippleFrequency + timeSeconds * auroraRippleSpeed +
                                 1.3 * std::sin(x * auroraSwayFrequency + timeSeconds * 0.2));
        const double centreY =
            height * (auroraBandTopFraction + 0.12 * std::sin(x * auroraSwayFrequency + timeSeconds * auroraSwaySpeed) +
                      0.06 * std::sin(x * 0.027 - timeSeconds * 0.17));
        const int firstY = std::max(0, static_cast<int>(std::floor(centreY - auroraReachInHalfHeights * bandHalfHeight)));
        const int lastY =
            std::min(height - 1, static_cast<int>(std::ceil(centreY + auroraReachInHalfHeights * bandHalfHeight)));

        for (int y = firstY; y <= lastY; ++y) {
            const double distanceInHalfHeights = std::abs(y - centreY) / bandHalfHeight;
            const int step = std::min(auroraFalloffSteps - 1,
                                      static_cast<int>(distanceInHalfHeights / auroraReachInHalfHeights * (auroraFalloffSteps - 1)));
            const float opacity = strength * static_cast<float>(curtain) * auroraFalloff_[step];
            if (opacity < auroraMinimumVisibleOpacity) continue;
            blendPixel(framebuffer, x, y, planet.auroraColor, opacity);
        }
    }
}

// Four corner brackets around the animal, with a progress bar underneath.
// The bar goes grey and stays empty for an animal that is already in the journal.
void Renderer::drawScanReticle(Framebuffer& framebuffer, const SceneView& scene, const ScreenOrigin& origin,
                               int scale) const {
    using namespace constants;
    const ScanStatus& scan = *scene.scan;
    const Rgb8& accent = scan.alreadyLogged ? scannerLoggedColor : scannerColor;

    // Aim at the body, not the feet.
    const int bodyRaise = (spriteBaseTileSizeInPixels / 2) * scale;
    const int centreX = worldToScreenX(origin, scan.targetPosition.x);
    const int centreY = worldToScreenY(origin, scan.targetPosition.y) - bodyRaise;

    const int halfSize = 5 * scale;
    const int cornerLength = 2 * scale;
    for (int offset = 0; offset < cornerLength; ++offset) {
        for (int sideX = -1; sideX <= 1; sideX += 2) {
            for (int sideY = -1; sideY <= 1; sideY += 2) {
                const int cornerX = centreX + sideX * halfSize;
                const int cornerY = centreY + sideY * halfSize;
                framebuffer.setPixelClipped(cornerX - sideX * offset, cornerY, toPacked(accent));
                framebuffer.setPixelClipped(cornerX, cornerY - sideY * offset, toPacked(accent));
            }
        }
    }

    // Progress bar: dark track with a bright fill.
    const int barWidth = 2 * halfSize;
    const int barTop = centreY + halfSize + 2 * scale;
    const int filled = static_cast<int>(std::lround(std::clamp(scan.progress, 0.0, 1.0) * barWidth));
    for (int row = 0; row < scale; ++row) {
        for (int column = 0; column < barWidth; ++column) {
            const Rgb8& color = column < filled ? accent : scannerTrackColor;
            framebuffer.setPixelClipped(centreX - halfSize + column, barTop + row, toPacked(color));
        }
    }
}

void Renderer::drawFrame(Framebuffer& framebuffer, const SceneView& scene) {
    if (scene.planet && scene.planet->palette != palette_) setPalette(scene.planet->palette);

    const Camera& camera = scene.camera;
    const ScreenOrigin origin = screenOriginFor(camera, framebuffer.width(), framebuffer.height());
    // Sprites scale with zoom in whole steps, never below 1x, so they stay visible zoomed out.
    const int scale = std::max(1, camera.tileSizeInPixels / constants::spriteBaseTileSizeInPixels);

    drawTerrain(framebuffer, camera, *scene.chunks, scene.timeSeconds);
    if (scene.trails) drawTrails(framebuffer, scene, origin, scale);

    drawItems_.clear();
    lights_.clear();
    steamSources_.clear();

    collectProps(framebuffer, scene, origin, scale);
    if (scene.creatures) collectCreatures(scene, origin, scale);

    if (scene.hasShip) {
        const Sprite& shipSprite = sprites_.ship();
        DrawItem shipItem;
        shipItem.sprite = &shipSprite;
        shipItem.footX = worldToScreenX(origin, scene.shipFoot.x);
        shipItem.footY = worldToScreenY(origin, scene.shipFoot.y);
        shipItem.scale = scale;
        drawItems_.push_back(shipItem);
        // A cabin glow, so the way home is easy to spot after dark.
        lights_.push_back({shipItem.footX, shipItem.footY - 6 * scale, &constants::shipLight, 1.0f});
    }

    DrawItem playerItem;
    playerItem.sprite = &sprites_.player(scene.player.walkFrame);
    playerItem.footX = worldToScreenX(origin, scene.player.tileX);
    playerItem.footY = worldToScreenY(origin, scene.player.tileY);
    playerItem.scale = scale;
    playerItem.flip = scene.player.facingLeft;
    playerItem.wading = scene.player.wading;
    drawItems_.push_back(playerItem);

    const int lampDirection = scene.player.facingLeft ? -1 : 1;
    lights_.push_back({playerItem.footX + lampDirection * SpriteLibrary::lampOffsetX * scale,
                       playerItem.footY + SpriteLibrary::lampOffsetY * scale, &constants::headlampLight,
                       scene.headlampStrength, scene.headlampRadiusScale});

    // Painter's order: higher on screen is further away, so draw it first.
    // Stable sort with an x tie-break keeps the order steady frame to frame.
    std::stable_sort(drawItems_.begin(), drawItems_.end(), [](const DrawItem& a, const DrawItem& b) {
        if (a.footY != b.footY) return a.footY < b.footY;
        return a.footX < b.footX;
    });
    for (const DrawItem& item : drawItems_) drawItem(framebuffer, item);

    drawSteam(framebuffer, scene.timeSeconds, scale);
    if (scene.weather) drawWeather(framebuffer, scene, origin, scale);
    applyLighting(framebuffer, scene.lighting, camera.tileSizeInPixels);
    drawStars(framebuffer, scene.lighting, scene.timeSeconds);
    if (scene.planet) {
        drawMoons(framebuffer, *scene.planet, scene.lighting, scene.timeSeconds);
        drawAurora(framebuffer, *scene.planet, scene.lighting, scene.timeSeconds);
    }
    drawMeteors(framebuffer, scene);
    drawLightningFlash(framebuffer, scene.lightningFlash);

    // The reticle is UI too, so it stays readable at night.
    if (scene.scan && scene.scan->hasTarget) drawScanReticle(framebuffer, scene, origin, scale);

    // The map is UI, so it goes on last and isn't darkened by night.
    if (scene.minimap) drawMinimap(framebuffer, scene);
}

}  // namespace wl
