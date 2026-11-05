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

ColorF landColor(Biome biome) {
    using namespace constants;
    switch (biome) {
        case Biome::Beach: return toColorF(beachColor);
        case Biome::Grassland: return toColorF(grasslandColor);
        case Biome::Forest: return toColorF(forestColor);
        case Biome::Desert: return toColorF(desertColor);
        case Biome::Savanna: return toColorF(savannaColor);
        case Biome::Tundra: return toColorF(tundraColor);
        case Biome::Snow: return toColorF(snowColor);
        case Biome::Mountain: return toColorF(mountainColor);
        default: return toColorF(grasslandColor);
    }
}

// Base colour and brightness for one tile, before per-pixel grain.
void tileColorAndBrightness(Biome biome, float elevation, float hillshade, std::uint32_t tileHash, ColorF& color,
                            float& brightness) {
    using namespace constants;
    const float jitter = (hashToUnitFloat(tileHash) - 0.5f) * static_cast<float>(tileColorJitter);

    if (isWater(biome)) {
        // Colour by depth, not biome, so the sea floor shelves smoothly.
        const float depth = std::clamp(
            static_cast<float>((seaLevelElevation - elevation) / (seaLevelElevation - deepestWaterElevation)), 0.0f,
            1.0f);
        color = lerpColor(toColorF(shallowWaterColor), toColorF(deepWaterColor), depth);
        brightness = 1.0f + jitter * 0.5f;  // water is calmer than land
        if (biome == Biome::DeepOcean) brightness *= deepOceanDarkening;
        return;
    }

    color = landColor(biome);
    if (biome == Biome::Mountain) {
        // Rock lightens toward the peaks so ranges read as height, not just grey.
        const float heightBlend = std::clamp(
            static_cast<float>((elevation - mountainElevation) / (snowCapElevation - mountainElevation)), 0.0f, 1.0f);
        color = lerpColor(color, toColorF(snowColor), heightBlend * static_cast<float>(mountainHeightTint));
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
                tileColorAndBrightness(biome, currentChunk->elevations[tileIndex],
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
        lightBuffer_.assign(pixelCount * 3, 0);
        for (const PendingLight& light : lights_) {
            const constants::LightSettings& settings = *light.settings;
            const int radius = static_cast<int>(settings.radiusInTiles * tileSize);
            const double radiusSquared = static_cast<double>(radius) * radius;
            // Lights fade in with the dark, so they don't pop on at sunset.
            const float amount = settings.intensity * light.strength * lighting.nightAmount * fixedPointOne;

            const int minX = std::max(0, light.x - radius);
            const int maxX = std::min(width - 1, light.x + radius);
            const int minY = std::max(0, light.y - radius);
            const int maxY = std::min(height - 1, light.y + radius);
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

void Renderer::drawFrame(Framebuffer& framebuffer, const SceneView& scene) {
    const Camera& camera = scene.camera;
    const ScreenOrigin origin = screenOriginFor(camera, framebuffer.width(), framebuffer.height());
    // Sprites scale with zoom in whole steps, never below 1x, so they stay visible zoomed out.
    const int scale = std::max(1, camera.tileSizeInPixels / constants::spriteBaseTileSizeInPixels);

    drawTerrain(framebuffer, camera, *scene.chunks, scene.timeSeconds);

    drawItems_.clear();
    lights_.clear();
    steamSources_.clear();

    collectProps(framebuffer, scene, origin, scale);
    if (scene.creatures) collectCreatures(scene, origin, scale);

    DrawItem playerItem;
    playerItem.sprite = &sprites_.player(scene.player.walkFrame);
    playerItem.footX = worldToScreenX(origin, scene.player.tileX);
    playerItem.footY = worldToScreenY(origin, scene.player.tileY);
    playerItem.scale = scale;
    playerItem.flip = scene.player.facingLeft;
    playerItem.wading = scene.player.wading;
    drawItems_.push_back(playerItem);

    const int lanternDirection = scene.player.facingLeft ? -1 : 1;
    lights_.push_back({playerItem.footX + lanternDirection * SpriteLibrary::lanternOffsetX * scale,
                       playerItem.footY + SpriteLibrary::lanternOffsetY * scale, &constants::lanternLight, 1.0f});

    // Painter's order: higher on screen is further away, so draw it first.
    // Stable sort with an x tie-break keeps the order steady frame to frame.
    std::stable_sort(drawItems_.begin(), drawItems_.end(), [](const DrawItem& a, const DrawItem& b) {
        if (a.footY != b.footY) return a.footY < b.footY;
        return a.footX < b.footX;
    });
    for (const DrawItem& item : drawItems_) drawItem(framebuffer, item);

    drawSteam(framebuffer, scene.timeSeconds, scale);
    applyLighting(framebuffer, scene.lighting, camera.tileSizeInPixels);
    drawStars(framebuffer, scene.lighting, scene.timeSeconds);
}

}  // namespace wl
