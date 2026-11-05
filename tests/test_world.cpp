#include <catch2/catch_test_macros.hpp>
#include <cmath>

#include "constants.h"
#include "world.h"

using namespace wl;

namespace {
constexpr int size = constants::chunkSizeInTiles;

bool chunksIdentical(const Chunk& first, const Chunk& second) {
    return first.coord == second.coord && first.biomes == second.biomes &&
           first.elevations == second.elevations && first.hillshades == second.hillshades &&
           first.props == second.props;
}
}  // namespace

TEST_CASE("tile to chunk conversion rounds down for negative tiles", "[world][coords]") {
    REQUIRE(chunkCoordForTile(0, 0) == ChunkCoord{0, 0});
    REQUIRE(chunkCoordForTile(size - 1, size - 1) == ChunkCoord{0, 0});
    REQUIRE(chunkCoordForTile(size, 0) == ChunkCoord{1, 0});
    REQUIRE(chunkCoordForTile(-1, -1) == ChunkCoord{-1, -1});
    REQUIRE(chunkCoordForTile(-size, 0) == ChunkCoord{-1, 0});
    REQUIRE(chunkCoordForTile(-size - 1, 0) == ChunkCoord{-2, 0});

    REQUIRE(floorModulo(-1, size) == size - 1);
    REQUIRE(floorModulo(size, size) == 0);
    REQUIRE(chunkOriginTile(-2) == -2 * size);
}

TEST_CASE("the same seed gives identical chunks across runs", "[world][determinism]") {
    const World firstRun(2024);
    const World secondRun(2024);
    const ChunkCoord coords[] = {{0, 0}, {3, -2}, {-17, 40}, {1000, -1000}};
    for (const ChunkCoord& coord : coords) {
        REQUIRE(chunksIdentical(*firstRun.generateChunk(coord), *secondRun.generateChunk(coord)));
    }
}

TEST_CASE("chunk generation does not depend on order", "[world][determinism]") {
    const World world(77);
    const auto generatedFirst = world.generateChunk({5, 5});
    world.generateChunk({-3, 8});
    world.generateChunk({6, 5});
    const auto generatedLater = world.generateChunk({5, 5});
    REQUIRE(chunksIdentical(*generatedFirst, *generatedLater));
}

TEST_CASE("different seeds give different worlds", "[world][determinism]") {
    const auto chunkA = World(1).generateChunk({0, 0});
    const auto chunkB = World(2).generateChunk({0, 0});
    REQUIRE(chunkA->elevations != chunkB->elevations);
}

TEST_CASE("chunk edges line up seamlessly with neighbours", "[world][seams]") {
    // If every tile in a chunk matches the global sample for its world
    // position, neighbouring chunks can't disagree at the border. Use chunks
    // either side of zero on both axes, since that's where coordinate bugs hide.
    const World world(31337);
    for (int chunkY = -1; chunkY <= 0; ++chunkY) {
        for (int chunkX = -1; chunkX <= 0; ++chunkX) {
            const auto chunk = world.generateChunk({chunkX, chunkY});
            for (int localY = 0; localY < size; ++localY) {
                for (int localX = 0; localX < size; ++localX) {
                    const int tileX = chunkOriginTile(chunkX) + localX;
                    const int tileY = chunkOriginTile(chunkY) + localY;
                    const int index = Chunk::indexFor(localX, localY);
                    const TerrainSample sample = world.sampleTerrain(tileX, tileY);

                    REQUIRE(chunk->biomes[index] == sample.biome);
                    REQUIRE(chunk->elevations[index] == static_cast<float>(sample.elevation));
                    REQUIRE(chunk->hillshades[index] ==
                            static_cast<float>(world.hillshadeAt(tileX, tileY)));
                }
            }
        }
    }
}

TEST_CASE("elevation changes smoothly across a chunk border", "[world][seams]") {
    const World world(9);
    const auto left = world.generateChunk({0, 0});
    const auto right = world.generateChunk({1, 0});
    // Neighbouring tiles differ by a few thousandths at most. A seam would jump.
    constexpr float maxNeighbourStep = 0.05f;
    for (int localY = 0; localY < size; ++localY) {
        const float lastOfLeft = left->elevations[Chunk::indexFor(size - 1, localY)];
        const float firstOfRight = right->elevations[Chunk::indexFor(0, localY)];
        REQUIRE(std::abs(lastOfLeft - firstOfRight) < maxNeighbourStep);
    }
}

TEST_CASE("chunk values are inside their expected ranges", "[world]") {
    const World world(555);
    const auto chunk = world.generateChunk({2, -7});
    for (int index = 0; index < tilesPerChunk; ++index) {
        REQUIRE(chunk->elevations[index] >= 0.0f);
        REQUIRE(chunk->elevations[index] <= 1.0f);
        REQUIRE(chunk->hillshades[index] >= -1.0f);
        REQUIRE(chunk->hillshades[index] <= 1.0f);
        REQUIRE(static_cast<int>(chunk->biomes[index]) < biomeCount);
    }
}

TEST_CASE("spawn lands somewhere pleasant", "[world]") {
    for (WorldSeed seed : {1u, 42u, 1337u, 99999u}) {
        const World world(seed);
        int spawnX = 0;
        int spawnY = 0;
        world.findSpawnTile(spawnX, spawnY);
        const Biome biome = world.sampleTerrain(spawnX, spawnY).biome;
        REQUIRE((biome == Biome::Grassland || biome == Biome::Forest || biome == Biome::Savanna));
    }
}
