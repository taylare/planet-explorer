#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include <optional>
#include <set>
#include <vector>

#include "constants.h"
#include "props.h"

using namespace wl;

namespace {
// The chunk the player spawns in. It's always on pleasant land, so tests
// around it can rely on there being props to check.
ChunkCoord spawnChunk(const World& world) {
    int spawnX = 0;
    int spawnY = 0;
    world.findSpawnTile(spawnX, spawnY);
    return chunkCoordForTile(spawnX, spawnY);
}

// Every prop in a square of chunks, gathered into one list.
std::vector<Prop> propsAround(const World& world, ChunkCoord centre, int radius) {
    std::vector<Prop> props;
    for (int chunkY = centre.y - radius; chunkY <= centre.y + radius; ++chunkY) {
        for (int chunkX = centre.x - radius; chunkX <= centre.x + radius; ++chunkX) {
            const auto chunk = world.generateChunk({chunkX, chunkY});
            props.insert(props.end(), chunk->props.begin(), chunk->props.end());
        }
    }
    return props;
}

double distanceBetween(const Prop& a, const Prop& b) { return std::hypot(a.tileX - b.tileX, a.tileY - b.tileY); }
}  // namespace

TEST_CASE("props are identical for the same seed", "[props][determinism]") {
    const World firstWorld(99);
    const World secondWorld(99);
    const ChunkCoord coord = spawnChunk(firstWorld);
    const auto first = firstWorld.generateChunk(coord);
    const auto second = secondWorld.generateChunk(coord);
    REQUIRE_FALSE(first->props.empty());
    REQUIRE(first->props == second->props);
}

TEST_CASE("ordinary props keep their minimum spacing, across chunk edges too", "[props]") {
    const World world(1234);
    const std::vector<Prop> props = propsAround(world, spawnChunk(world), 1);
    REQUIRE(props.size() > 50);

    const double minimumSpacing = 2.0 * constants::propCellMarginInTiles;
    for (std::size_t first = 0; first < props.size(); ++first) {
        if (isRareLandmark(props[first].kind)) continue;
        for (std::size_t second = first + 1; second < props.size(); ++second) {
            if (isRareLandmark(props[second].kind)) continue;
            // Small tolerance for the float storage of positions.
            REQUIRE(distanceBetween(props[first], props[second]) >= minimumSpacing - 1e-4);
        }
    }
}

TEST_CASE("props only stand on suitable ground", "[props]") {
    const World world(77);
    const std::vector<Prop> props = propsAround(world, spawnChunk(world), 2);
    REQUIRE_FALSE(props.empty());
    for (const Prop& prop : props) {
        const Biome biome = world.sampleTerrain(static_cast<int>(std::floor(prop.tileX)),
                                                static_cast<int>(std::floor(prop.tileY))).biome;
        REQUIRE_FALSE(isWater(biome));
        REQUIRE(biome == prop.groundBiome);
        if (prop.kind == PropKind::Cactus) REQUIRE(biome == Biome::Desert);
        if (prop.kind == PropKind::AcaciaTree) REQUIRE(biome == Biome::Savanna);
    }
}

TEST_CASE("each prop belongs to the chunk it stands in", "[props]") {
    const World world(5);
    const auto chunk = world.generateChunk(spawnChunk(world));
    REQUIRE_FALSE(chunk->props.empty());
    for (const Prop& prop : chunk->props) {
        const ChunkCoord owner = chunkCoordForTile(static_cast<int>(std::floor(prop.tileX)),
                                                   static_cast<int>(std::floor(prop.tileY)));
        REQUIRE(owner == chunk->coord);
    }
}

TEST_CASE("every rare landmark kind exists somewhere, and they suit their ground", "[props][landmarks]") {
    const World world(2024);
    std::set<PropKind> kindsFound;
    for (int regionY = -12; regionY < 12; ++regionY) {
        for (int regionX = -12; regionX < 12; ++regionX) {
            const auto landmark = findRegionLandmark(world, regionX, regionY);
            if (!landmark) continue;
            REQUIRE(isRareLandmark(landmark->kind));
            REQUIRE_FALSE(isWater(landmark->groundBiome));
            kindsFound.insert(landmark->kind);
        }
    }
    REQUIRE(kindsFound.count(PropKind::Tower) == 1);
    REQUIRE(kindsFound.count(PropKind::CrystalGrove) == 1);
    REQUIRE(kindsFound.count(PropKind::HotSpring) == 1);
}

TEST_CASE("a landmark shows up in exactly one chunk with a clearing around it", "[props][landmarks]") {
    const World world(2024);
    std::optional<Prop> landmark;
    int foundRegionX = 0;
    for (int regionX = 0; regionX < 50 && !landmark; ++regionX) {
        landmark = findRegionLandmark(world, regionX, 0);
        foundRegionX = regionX;
    }
    REQUIRE(landmark.has_value());

    // Scan every chunk in the region plus a ring around it.
    const int regionChunks = constants::landmarkRegionSizeInChunks;
    int copiesFound = 0;
    for (int chunkY = -1; chunkY <= regionChunks; ++chunkY) {
        for (int chunkX = foundRegionX * regionChunks - 1; chunkX <= (foundRegionX + 1) * regionChunks; ++chunkX) {
            const auto chunk = world.generateChunk({chunkX, chunkY});
            for (const Prop& prop : chunk->props) {
                if (prop == *landmark) {
                    ++copiesFound;
                } else if (!isRareLandmark(prop.kind)) {
                    REQUIRE(distanceBetween(prop, *landmark) >= constants::landmarkClearingRadiusInTiles);
                }
            }
        }
    }
    REQUIRE(copiesFound == 1);
}

TEST_CASE("prop and landmark names are filled in", "[props]") {
    for (int index = 0; index < propKindCount; ++index) {
        REQUIRE(propKindName(static_cast<PropKind>(index)) != "Unknown");
    }
}
