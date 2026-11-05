// The world: seed, biome lookup, tile/chunk coordinates and chunk generation.
// The World is read-only after construction, so any chunk can be generated in
// any order, any number of times, and come out identical.
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string_view>

#include "constants.h"
#include "noise.h"

namespace wl {

using WorldSeed = std::uint32_t;

enum class Biome : std::uint8_t {
    DeepOcean,
    ShallowWater,
    Beach,
    Grassland,
    Forest,
    Desert,
    Savanna,
    Tundra,
    Snow,
    Mountain,
    Count  // keep last
};

inline constexpr int biomeCount = static_cast<int>(Biome::Count);

std::string_view biomeName(Biome biome);
bool isWater(Biome biome);

// Pure lookup from the three climate values (each roughly 0 to 1, clamped if
// not). Kept separate from noise so it can be tested exhaustively.
Biome classifyBiome(double elevation, double moisture, double temperature);

// --- Coordinates ---
// Tiles are integer world positions. Chunk (cx, cy) covers tiles
// cx*size .. cx*size + size-1 on each axis.

// Plain "/" rounds toward zero, so tile -1 would land in chunk 0 alongside
// tile 0. Rounding down puts it in chunk -1 where it belongs.
constexpr int floorDivide(int value, int divisor) {
    const int quotient = value / divisor;
    return (value % divisor != 0 && (value < 0) != (divisor < 0)) ? quotient - 1 : quotient;
}

// Always in [0, divisor), unlike "%" which goes negative for negative values.
constexpr int floorModulo(int value, int divisor) {
    return value - floorDivide(value, divisor) * divisor;
}

struct ChunkCoord {
    std::int32_t x = 0;
    std::int32_t y = 0;
    bool operator==(const ChunkCoord& other) const { return x == other.x && y == other.y; }
    bool operator!=(const ChunkCoord& other) const { return !(*this == other); }
};

struct ChunkCoordHash {
    std::size_t operator()(const ChunkCoord& coord) const;
};

constexpr ChunkCoord chunkCoordForTile(int tileX, int tileY) {
    return {floorDivide(tileX, constants::chunkSizeInTiles),
            floorDivide(tileY, constants::chunkSizeInTiles)};
}

constexpr int chunkOriginTile(int chunkAxisCoord) {
    return chunkAxisCoord * constants::chunkSizeInTiles;
}

// --- Chunks ---

inline constexpr int tilesPerChunk = constants::chunkSizeInTiles * constants::chunkSizeInTiles;

struct Chunk {
    ChunkCoord coord;
    std::array<Biome, tilesPerChunk> biomes{};
    std::array<float, tilesPerChunk> elevations{};
    std::array<float, tilesPerChunk> hillshades{};  // -1 dark to 1 lit

    static constexpr int indexFor(int localX, int localY) {
        return localY * constants::chunkSizeInTiles + localX;
    }
};

// Anything that can hand out generated chunks (a cache, a test map...).
class ChunkSource {
public:
    virtual ~ChunkSource() = default;
    virtual const Chunk* findChunk(ChunkCoord coord) const = 0;
};

// Biome at a tile, if its chunk is loaded.
bool tryGetBiome(const ChunkSource& chunks, int tileX, int tileY, Biome& biomeOut);

// --- World ---

struct TerrainSample {
    double elevation = 0.0;
    double moisture = 0.0;
    double temperature = 0.0;  // already cooled by altitude
    Biome biome = Biome::DeepOcean;
};

// Light from the top-left. Shared by chunk generation and the slow path below
// so both give bit-identical results.
double hillshadeFromNeighbours(double elevationLeft, double elevationRight, double elevationUp,
                               double elevationDown);

class World {
public:
    explicit World(WorldSeed seed);

    WorldSeed seed() const { return seed_; }

    TerrainSample sampleTerrain(int tileX, int tileY) const;
    double sampleElevation(int tileX, int tileY) const;

    // Slow per-tile version of what generateChunk computes. Used in tests.
    double hillshadeAt(int tileX, int tileY) const;

    std::unique_ptr<Chunk> generateChunk(ChunkCoord coord) const;

    // First land tile found spiralling out from the origin, so you don't spawn at sea.
    void findSpawnTile(int& spawnTileX, int& spawnTileY) const;

private:
    struct WarpedPosition {
        double x, y;
    };

    WarpedPosition warp(int tileX, int tileY) const;
    double elevationAtWarped(const WarpedPosition& position) const;
    TerrainSample climateAt(int tileX, int tileY, const WarpedPosition& position,
                            double elevation) const;

    WorldSeed seed_;
    FractalNoise elevationNoise_;
    FractalNoise warpNoiseX_;
    FractalNoise warpNoiseY_;
    FractalNoise ridgeNoise_;
    FractalNoise moistureNoise_;
    FractalNoise temperatureNoise_;
};

}  // namespace wl
