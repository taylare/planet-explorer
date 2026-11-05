#include "renderer.h"

#include <algorithm>
#include <cmath>

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

TileBounds visibleTileBounds(const Camera& camera, int framebufferWidth, int framebufferHeight) {
    const int tileSize = std::max(camera.tileSizeInPixels, 1);
    // Snap the camera to whole pixels so the image doesn't shimmer while moving.
    const std::int64_t leftWorldPixel =
        static_cast<std::int64_t>(std::floor(camera.centerTileX * tileSize)) - framebufferWidth / 2;
    const std::int64_t topWorldPixel =
        static_cast<std::int64_t>(std::floor(camera.centerTileY * tileSize)) - framebufferHeight / 2;

    TileBounds bounds;
    bounds.minTileX = static_cast<int>(floorDivide64(leftWorldPixel, tileSize));
    bounds.minTileY = static_cast<int>(floorDivide64(topWorldPixel, tileSize));
    bounds.maxTileX = static_cast<int>(floorDivide64(leftWorldPixel + framebufferWidth - 1, tileSize));
    bounds.maxTileY = static_cast<int>(floorDivide64(topWorldPixel + framebufferHeight - 1, tileSize));
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

void Renderer::drawTerrain(Framebuffer& framebuffer, const Camera& camera,
                           const ChunkSource& chunks) const {
    const int width = framebuffer.width();
    const int height = framebuffer.height();
    const int tileSize = std::max(camera.tileSizeInPixels, 1);

    const std::int64_t leftWorldPixel =
        static_cast<std::int64_t>(std::floor(camera.centerTileX * tileSize)) - width / 2;
    const std::int64_t topWorldPixel =
        static_cast<std::int64_t>(std::floor(camera.centerTileY * tileSize)) - height / 2;
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
