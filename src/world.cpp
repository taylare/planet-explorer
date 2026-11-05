#include "world.h"

#include <algorithm>
#include <vector>

#include "hash.h"
#include "props.h"

namespace wl {

namespace {

// Salts that give each noise field its own seed. Any distinct values work;
// they just must never change, or every saved world would reshape.
enum class NoiseChannel : std::uint32_t {
    Elevation = 1,
    WarpX = 2,
    WarpY = 3,
    Ridge = 4,
    Moisture = 5,
    Temperature = 6,
};

FractalNoise makeNoise(WorldSeed seed, NoiseChannel channel, double frequency, int octaves) {
    FractalNoiseSettings settings;
    settings.baseFrequency = frequency;
    settings.octaves = octaves;
    settings.lacunarity = constants::noiseLacunarity;
    settings.persistence = constants::noisePersistence;
    return FractalNoise(deriveSeed(seed, static_cast<std::uint32_t>(channel)), settings);
}

double clampUnit(double value) { return std::clamp(value, 0.0, 1.0); }

// Maps noise in [-1, 1] to [0, 1], stretched by contrast around the middle.
double noiseToUnit(double noiseValue, double contrast) {
    return clampUnit(0.5 + 0.5 * noiseValue * contrast);
}

double smoothstep(double edgeLow, double edgeHigh, double value) {
    const double t = clampUnit((value - edgeLow) / (edgeHigh - edgeLow));
    return t * t * (3.0 - 2.0 * t);
}

bool isPleasantSpawnBiome(Biome biome) {
    return biome == Biome::Grassland || biome == Biome::Forest || biome == Biome::Savanna;
}

}  // namespace

std::string_view biomeName(Biome biome) {
    switch (biome) {
        case Biome::DeepOcean: return "Deep Ocean";
        case Biome::ShallowWater: return "Shallows";
        case Biome::Beach: return "Beach";
        case Biome::Grassland: return "Grassland";
        case Biome::Forest: return "Forest";
        case Biome::Desert: return "Desert";
        case Biome::Savanna: return "Savanna";
        case Biome::Tundra: return "Tundra";
        case Biome::Snow: return "Snowfield";
        case Biome::Mountain: return "Mountains";
        case Biome::Count: break;
    }
    return "Unknown";
}

bool isWater(Biome biome) { return biome == Biome::DeepOcean || biome == Biome::ShallowWater; }

Biome classifyBiome(double elevation, double moisture, double temperature) {
    using namespace constants;
    elevation = clampUnit(elevation);
    moisture = clampUnit(moisture);
    temperature = clampUnit(temperature);

    // Elevation decides first: water, coast and peaks override climate.
    if (elevation < deepWaterElevation) return Biome::DeepOcean;
    if (elevation < seaLevelElevation) return Biome::ShallowWater;
    if (elevation < beachElevation) return temperature < snowTemperature ? Biome::Snow : Biome::Beach;
    if (elevation >= snowCapElevation) return Biome::Snow;
    if (elevation >= mountainElevation) return Biome::Mountain;

    // Then temperature picks the climate band, and moisture the biome within it.
    if (temperature < snowTemperature) return Biome::Snow;
    if (temperature < tundraTemperature) return Biome::Tundra;
    if (temperature > hotTemperature) {
        if (moisture < desertMoisture) return Biome::Desert;
        if (moisture < savannaMoisture) return Biome::Savanna;
        return Biome::Forest;
    }
    return moisture < forestMoisture ? Biome::Grassland : Biome::Forest;
}

bool tryGetBiome(const ChunkSource& chunks, int tileX, int tileY, Biome& biomeOut) {
    const Chunk* chunk = chunks.findChunk(chunkCoordForTile(tileX, tileY));
    if (!chunk) return false;
    biomeOut = chunk->biomes[Chunk::indexFor(floorModulo(tileX, constants::chunkSizeInTiles),
                                             floorModulo(tileY, constants::chunkSizeInTiles))];
    return true;
}

std::size_t ChunkCoordHash::operator()(const ChunkCoord& coord) const {
    return hashCoords(0, coord.x, coord.y);
}

double hillshadeFromNeighbours(double elevationLeft, double elevationRight, double elevationUp,
                               double elevationDown) {
    // Central differences give the slope. A face rising toward the bottom-right
    // faces the top-left light, so positive slopes are lit.
    const double slopeX = (elevationRight - elevationLeft) * 0.5;
    const double slopeY = (elevationDown - elevationUp) * 0.5;
    return std::clamp((slopeX + slopeY) * constants::hillshadeStrength, -1.0, 1.0);
}

World::World(WorldSeed seed)
    : seed_(seed),
      elevationNoise_(makeNoise(seed, NoiseChannel::Elevation, constants::elevationBaseFrequency,
                                constants::elevationOctaves)),
      warpNoiseX_(makeNoise(seed, NoiseChannel::WarpX, constants::warpBaseFrequency,
                            constants::warpOctaves)),
      warpNoiseY_(makeNoise(seed, NoiseChannel::WarpY, constants::warpBaseFrequency,
                            constants::warpOctaves)),
      ridgeNoise_(makeNoise(seed, NoiseChannel::Ridge, constants::ridgeBaseFrequency,
                            constants::ridgeOctaves)),
      moistureNoise_(makeNoise(seed, NoiseChannel::Moisture, constants::moistureBaseFrequency,
                               constants::moistureOctaves)),
      temperatureNoise_(makeNoise(seed, NoiseChannel::Temperature,
                                  constants::temperatureBaseFrequency,
                                  constants::temperatureOctaves)) {}

World::WarpedPosition World::warp(int tileX, int tileY) const {
    const double x = tileX;
    const double y = tileY;
    return {x + constants::warpStrengthInTiles * warpNoiseX_.sample(x, y),
            y + constants::warpStrengthInTiles * warpNoiseY_.sample(x, y)};
}

double World::elevationAtWarped(const WarpedPosition& position) const {
    double elevation =
        noiseToUnit(elevationNoise_.sample(position.x, position.y), constants::elevationContrast);

    // Only high ground gets ridges, and they fade in so there's no hard line.
    // Skipping the ridge noise elsewhere also saves a lot of work.
    if (elevation > constants::ridgeStartElevation) {
        const double ridgeBlend = smoothstep(constants::ridgeStartElevation, 1.0, elevation);
        const double ridgeOffset = (ridgeNoise_.sampleRidged(position.x, position.y) - 0.5) * 2.0;
        elevation = clampUnit(elevation + constants::ridgeStrength * ridgeBlend * ridgeOffset);
    }
    return elevation;
}

TerrainSample World::climateAt(int tileX, int tileY, const WarpedPosition& position,
                               double elevation) const {
    TerrainSample sample;
    sample.elevation = elevation;

    // Moisture shares the warp so wet and dry areas follow the coastlines.
    sample.moisture =
        noiseToUnit(moistureNoise_.sample(position.x, position.y), constants::moistureContrast);

    // Temperature skips the warp: broad climate bands should stay broad.
    const double baseTemperature = noiseToUnit(temperatureNoise_.sample(tileX, tileY),
                                               constants::temperatureContrast);
    const double heightAboveSea = std::max(0.0, elevation - constants::seaLevelElevation);
    sample.temperature = clampUnit(baseTemperature - constants::temperatureLapseRate * heightAboveSea);

    sample.biome = classifyBiome(sample.elevation, sample.moisture, sample.temperature);
    return sample;
}

double World::sampleElevation(int tileX, int tileY) const {
    return elevationAtWarped(warp(tileX, tileY));
}

TerrainSample World::sampleTerrain(int tileX, int tileY) const {
    const WarpedPosition position = warp(tileX, tileY);
    return climateAt(tileX, tileY, position, elevationAtWarped(position));
}

double World::hillshadeAt(int tileX, int tileY) const {
    return hillshadeFromNeighbours(sampleElevation(tileX - 1, tileY), sampleElevation(tileX + 1, tileY),
                                   sampleElevation(tileX, tileY - 1), sampleElevation(tileX, tileY + 1));
}

std::unique_ptr<Chunk> World::generateChunk(ChunkCoord coord) const {
    constexpr int size = constants::chunkSizeInTiles;

    // Elevation grid with a one-tile border borrowed from the neighbours.
    // Hillshade needs each tile's neighbours, and sampling them from the
    // global noise (not from the neighbour chunk) is what makes edges seamless
    // without the neighbour having to exist yet.
    constexpr int paddedSize = size + 2;
    std::vector<double> paddedElevations(static_cast<std::size_t>(paddedSize) * paddedSize);
    std::vector<WarpedPosition> interiorPositions(tilesPerChunk);

    const int originTileX = chunkOriginTile(coord.x);
    const int originTileY = chunkOriginTile(coord.y);

    for (int paddedY = 0; paddedY < paddedSize; ++paddedY) {
        for (int paddedX = 0; paddedX < paddedSize; ++paddedX) {
            const int localX = paddedX - 1;
            const int localY = paddedY - 1;
            const WarpedPosition position = warp(originTileX + localX, originTileY + localY);
            paddedElevations[paddedY * paddedSize + paddedX] = elevationAtWarped(position);

            const bool isInterior = localX >= 0 && localX < size && localY >= 0 && localY < size;
            if (isInterior) interiorPositions[Chunk::indexFor(localX, localY)] = position;
        }
    }

    auto chunk = std::make_unique<Chunk>();
    chunk->coord = coord;

    for (int localY = 0; localY < size; ++localY) {
        for (int localX = 0; localX < size; ++localX) {
            const int paddedRow = (localY + 1) * paddedSize;
            const int paddedColumn = localX + 1;
            const double elevation = paddedElevations[paddedRow + paddedColumn];

            const double hillshade = hillshadeFromNeighbours(
                paddedElevations[paddedRow + paddedColumn - 1],
                paddedElevations[paddedRow + paddedColumn + 1],
                paddedElevations[paddedRow - paddedSize + paddedColumn],
                paddedElevations[paddedRow + paddedSize + paddedColumn]);

            const int tileIndex = Chunk::indexFor(localX, localY);
            const TerrainSample sample = climateAt(originTileX + localX, originTileY + localY,
                                                   interiorPositions[tileIndex], elevation);

            chunk->biomes[tileIndex] = sample.biome;
            chunk->elevations[tileIndex] = static_cast<float>(elevation);
            chunk->hillshades[tileIndex] = static_cast<float>(hillshade);
        }
    }

    scatterProps(*this, *chunk);
    return chunk;
}

void World::findSpawnTile(int& spawnTileX, int& spawnTileY) const {
    constexpr int step = constants::spawnSearchStepInTiles;
    auto isGoodSpawn = [this](int tileX, int tileY) {
        return isPleasantSpawnBiome(sampleTerrain(tileX, tileY).biome);
    };

    if (isGoodSpawn(0, 0)) {
        spawnTileX = 0;
        spawnTileY = 0;
        return;
    }

    // Walk square rings outward so the nearest decent spot wins.
    for (int radius = step; radius <= constants::spawnSearchRadiusInTiles; radius += step) {
        for (int offset = -radius; offset <= radius; offset += step) {
            const int candidates[4][2] = {
                {offset, -radius}, {offset, radius}, {-radius, offset}, {radius, offset}};
            for (const auto& candidate : candidates) {
                if (isGoodSpawn(candidate[0], candidate[1])) {
                    spawnTileX = candidate[0];
                    spawnTileY = candidate[1];
                    return;
                }
            }
        }
    }

    // Practically unreachable, but the origin is a fine fallback.
    spawnTileX = 0;
    spawnTileY = 0;
}

}  // namespace wl
