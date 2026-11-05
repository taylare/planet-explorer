#include "renderer.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>

#include "hash.h"

namespace wl {

namespace {

// Fixed seed for the grain pattern. It's texture, not world content, so it
// doesn't need to change with the world seed.
constexpr std::uint32_t grainPatternSeed = 0x6a09e667u;

// Number of brightness variants per tile: darker, base, base, lighter.
// Doubling up "base" keeps most pixels at the true colour.
constexpr int grainVariantCount = 4;

// Deep water gets a touch darker than the depth gradient alone, so the
// wading line is readable at a glance.
constexpr float deepOceanDarkening = 0.85f;

struct ColorF {
    float red, green, blue;
};

ColorF toColorF(const constants::Rgb8& color) {
    return {static_cast<float>(color.red), static_cast<float>(color.green),
            static_cast<float>(color.blue)};
}

ColorF lerpColor(const ColorF& from, const ColorF& to, float t) {
    return {from.red + (to.red - from.red) * t, from.green + (to.green - from.green) * t,
            from.blue + (to.blue - from.blue) * t};
}

std::uint8_t toChannel(float value) {
    return static_cast<std::uint8_t>(std::clamp(value, 0.0f, 255.0f));
}

PackedColor toPacked(const constants::Rgb8& color) {
    return packRgba(color.red, color.green, color.blue);
}

void darkenPixel(Framebuffer& framebuffer, int x, int y, float factor) {
    if (x < 0 || y < 0 || x >= framebuffer.width() || y >= framebuffer.height()) return;
    PackedColor& pixel = framebuffer.at(x, y);
    const auto scaleChannel = [factor](PackedColor value, int shift) {
        return static_cast<std::uint8_t>(static_cast<float>((value >> shift) & 0xffu) * factor);
    };
    pixel = packRgba(scaleChannel(pixel, 0), scaleChannel(pixel, 8), scaleChannel(pixel, 16));
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
void tileColorAndBrightness(Biome biome, float elevation, float hillshade, std::uint32_t tileHash,
                            ColorF& color, float& brightness) {
    using namespace constants;
    const float jitter = (hashToUnitFloat(tileHash) - 0.5f) * static_cast<float>(tileColorJitter);

    if (isWater(biome)) {
        // Colour by depth, not biome, so the sea floor shelves smoothly.
        const float depth = std::clamp(
            static_cast<float>((seaLevelElevation - elevation) / (seaLevelElevation - deepestWaterElevation)),
            0.0f, 1.0f);
        color = lerpColor(toColorF(shallowWaterColor), toColorF(deepWaterColor), depth);
        brightness = 1.0f + jitter * 0.5f;  // water is calmer than land
        if (biome == Biome::DeepOcean) brightness *= deepOceanDarkening;
        return;
    }

    color = landColor(biome);
    if (biome == Biome::Mountain) {
        // Rock lightens toward the peaks so ranges read as height, not just grey.
        const float heightBlend = std::clamp(
            static_cast<float>((elevation - mountainElevation) / (snowCapElevation - mountainElevation)),
            0.0f, 1.0f);
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

    using namespace constants;
    const std::initializer_list<SpriteColorKey> playerColors = {
        {'h', toPacked(playerHoodColor)},   {'f', toPacked(playerFaceColor)},
        {'c', toPacked(playerCloakColor)},  {'b', toPacked(playerBootColor)},
        {'L', toPacked(lanternColor)},      {'l', toPacked(lanternHandleColor)},
    };
    // A hooded wanderer with a lantern. Three frames: standing, then two steps.
    playerFrames_[0] = buildSprite({"..hhh...",
                                    ".hhhhh..",
                                    ".hfffh..",
                                    ".ccccc..",
                                    "cccccccl",
                                    ".ccccc.L",
                                    ".ccccc..",
                                    "..b.b..."},
                                   playerColors, true);
    playerFrames_[1] = buildSprite({"..hhh...",
                                    ".hhhhh..",
                                    ".hfffh..",
                                    ".ccccc..",
                                    "cccccccl",
                                    ".ccccc.L",
                                    ".ccccc..",
                                    ".b...b.."},
                                   playerColors, true);
    playerFrames_[2] = buildSprite({"..hhh...",
                                    ".hhhhh..",
                                    ".hfffh..",
                                    ".ccccc..",
                                    "cccccccl",
                                    ".ccccc.L",
                                    ".ccccc..",
                                    "...bb..."},
                                   playerColors, true);
}

void Renderer::drawPlayer(Framebuffer& framebuffer, const Camera& camera,
                          const PlayerAppearance& player) const {
    const ScreenOrigin origin = screenOriginFor(camera, framebuffer.width(), framebuffer.height());
    const int footX = worldToScreenX(origin, player.tileX);
    const int footY = worldToScreenY(origin, player.tileY);

    // Scale with zoom in whole steps, but never below 1x so the player stays visible zoomed out.
    const int scale = std::max(1, camera.tileSizeInPixels / constants::spriteBaseTileSizeInPixels);
    const Sprite& sprite = playerFrames_[std::clamp(player.walkFrame, 0, playerWalkFrameCount - 1)];
    const int spriteLeft = footX - (sprite.width * scale) / 2;
    // Bottom row is outline, so the boots row sits right on the foot point.
    const int spriteTop = footY - (sprite.height - 1) * scale;

    SpriteDrawOptions options;
    options.scale = scale;
    options.flipHorizontally = player.facingLeft;

    if (player.wading) {
        // Hide the legs and draw a light waterline across the body.
        options.visibleRows = sprite.height - 1 - constants::wadingHiddenRows;
        drawSprite(framebuffer, sprite, spriteLeft, spriteTop, options);
        const PackedColor waterline = toPacked(constants::waterlineColor);
        const int waterlineY = spriteTop + options.visibleRows * scale;
        for (int x = spriteLeft; x < spriteLeft + sprite.width * scale; ++x) {
            framebuffer.setPixelClipped(x, waterlineY, waterline);
        }
        return;
    }

    // Soft shadow: a flat rounded strip under the feet.
    const int shadowHalfWidth = (sprite.width * scale) / 2 - scale;
    const int shadowHeight = std::max(1, scale);
    for (int offsetY = 0; offsetY < shadowHeight; ++offsetY) {
        for (int offsetX = -shadowHalfWidth; offsetX <= shadowHalfWidth; ++offsetX) {
            const bool isCorner = std::abs(offsetX) == shadowHalfWidth;
            if (isCorner) continue;
            darkenPixel(framebuffer, footX + offsetX, footY + offsetY, constants::spriteShadowStrength);
        }
    }
    drawSprite(framebuffer, sprite, spriteLeft, spriteTop, options);
}

void Renderer::drawTerrain(Framebuffer& framebuffer, const Camera& camera,
                           const ChunkSource& chunks) const {
    const int width = framebuffer.width();
    const int height = framebuffer.height();

    const ScreenOrigin origin = screenOriginFor(camera, width, height);
    const int tileSize = origin.tileSizeInPixels;
    const std::int64_t leftWorldPixel = origin.leftWorldPixel;
    const std::int64_t topWorldPixel = origin.topWorldPixel;
    const TileBounds bounds = visibleTileBounds(camera, width, height);

    const float grain = static_cast<float>(constants::pixelGrainStrength);
    const float grainMultipliers[grainVariantCount] = {1.0f - grain, 1.0f, 1.0f, 1.0f + grain};
    const ColorF unloaded = toColorF(constants::unloadedColor);

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
            if (currentChunk) {
                const int tileIndex = Chunk::indexFor(floorModulo(tileX, constants::chunkSizeInTiles),
                                                      floorModulo(tileY, constants::chunkSizeInTiles));
                tileColorAndBrightness(currentChunk->biomes[tileIndex], currentChunk->elevations[tileIndex],
                                       currentChunk->hillshades[tileIndex], hashCoords(0, tileX, tileY),
                                       baseColor, brightness);
            }

            PackedColor variants[grainVariantCount];
            for (int variant = 0; variant < grainVariantCount; ++variant) {
                variants[variant] = packScaled(baseColor, brightness * grainMultipliers[variant]);
            }

            for (int pixelY = rowStart; pixelY < rowEnd; ++pixelY) {
                const int grainRow = grainIndex(topWorldPixel + pixelY, grainPatternSize) * grainPatternSize;
                PackedColor* row = &framebuffer.at(0, pixelY);
                for (int pixelX = columnStart; pixelX < columnEnd; ++pixelX) {
                    const int grainColumn = grainIndex(leftWorldPixel + pixelX, grainPatternSize);
                    row[pixelX] = variants[grainPattern_[grainRow + grainColumn]];
                }
            }
        }
    }
}

}  // namespace wl
