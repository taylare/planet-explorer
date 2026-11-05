#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include <functional>
#include <memory>
#include <unordered_map>

#include "constants.h"
#include "entities.h"

using namespace wl;

namespace {
constexpr double step = constants::simulationStepSeconds;
constexpr int stepsPerSecond = 60;

// Hand-built terrain over chunks -4..4, biome chosen per tile.
class HandmadeTerrain : public ChunkSource {
public:
    explicit HandmadeTerrain(const std::function<Biome(int, int)>& biomeForTile) {
        for (int chunkY = -4; chunkY <= 4; ++chunkY) {
            for (int chunkX = -4; chunkX <= 4; ++chunkX) {
                auto chunk = std::make_unique<Chunk>();
                chunk->coord = {chunkX, chunkY};
                for (int localY = 0; localY < constants::chunkSizeInTiles; ++localY) {
                    for (int localX = 0; localX < constants::chunkSizeInTiles; ++localX) {
                        chunk->biomes[Chunk::indexFor(localX, localY)] =
                            biomeForTile(chunkOriginTile(chunkX) + localX, chunkOriginTile(chunkY) + localY);
                    }
                }
                chunks_[chunk->coord] = std::move(chunk);
            }
        }
    }

    const Chunk* findChunk(ChunkCoord coord) const override {
        const auto found = chunks_.find(coord);
        return found == chunks_.end() ? nullptr : found->second.get();
    }

private:
    std::unordered_map<ChunkCoord, std::unique_ptr<Chunk>, ChunkCoordHash> chunks_;
};

const Vec2 farAwayPlayer{10000.0, 10000.0};
const ChunkRegion regionAroundOrigin = ChunkRegion::around({0, 0}, 2, 2);

bool sameCreatures(const std::vector<Creature>& a, const std::vector<Creature>& b) {
    if (a.size() != b.size()) return false;
    for (std::size_t index = 0; index < a.size(); ++index) {
        if (a[index].species != b[index].species || a[index].position.x != b[index].position.x ||
            a[index].position.y != b[index].position.y) {
            return false;
        }
    }
    return true;
}
}  // namespace

TEST_CASE("wildlife spawns the same creatures for the same seed", "[wildlife]") {
    const HandmadeTerrain terrain([](int, int) { return Biome::Grassland; });
    Wildlife first;
    Wildlife second;
    first.update(regionAroundOrigin, 42, terrain, farAwayPlayer, step);
    second.update(regionAroundOrigin, 42, terrain, farAwayPlayer, step);
    REQUIRE_FALSE(first.creatures().empty());
    REQUIRE(sameCreatures(first.creatures(), second.creatures()));

    for (const Creature& creature : first.creatures()) {
        REQUIRE((creature.species == Species::Rabbit || creature.species == Species::Deer));
    }
}

TEST_CASE("creatures despawn when their home leaves the area and respawn on return", "[wildlife]") {
    const HandmadeTerrain terrain([](int, int) { return Biome::Forest; });
    Wildlife wildlife;
    wildlife.update(regionAroundOrigin, 7, terrain, farAwayPlayer, step);
    const std::vector<Creature> original = wildlife.creatures();
    REQUIRE_FALSE(original.empty());

    // A region that shares no chunks with the first.
    wildlife.update(ChunkRegion::around({10, 10}, 0, 0), 7, terrain, farAwayPlayer, step);
    for (const Creature& creature : wildlife.creatures()) REQUIRE(creature.homeChunk == ChunkCoord{10, 10});

    // Back again: fresh copies of the same creatures.
    Wildlife fresh;
    fresh.update(regionAroundOrigin, 7, terrain, farAwayPlayer, step);
    wildlife.update(regionAroundOrigin, 7, terrain, farAwayPlayer, step);
    REQUIRE(sameCreatures(wildlife.creatures(), fresh.creatures()));
}

TEST_CASE("creatures never step onto ground they can't stand on", "[wildlife]") {
    // A grass island with water all around.
    const HandmadeTerrain terrain([](int tileX, int tileY) {
        return (std::abs(tileX) < 12 && std::abs(tileY) < 12) ? Biome::Grassland : Biome::ShallowWater;
    });
    Wildlife wildlife;
    for (int i = 0; i < 20 * stepsPerSecond; ++i) {
        wildlife.update(regionAroundOrigin, 3, terrain, farAwayPlayer, step);
        for (const Creature& creature : wildlife.creatures()) {
            Biome biome = Biome::DeepOcean;
            REQUIRE(tryGetBiome(terrain, static_cast<int>(std::floor(creature.position.x)),
                                static_cast<int>(std::floor(creature.position.y)), biome));
            REQUIRE(canSpeciesStandOn(creature.species, biome));
        }
    }
}

TEST_CASE("a creature flees from a nearby player", "[wildlife]") {
    const HandmadeTerrain terrain([](int, int) { return Biome::Grassland; });
    Creature deer = makeCreature(Species::Deer, {0.5, 0.5}, {0, 0}, 12345);
    const Vec2 player{-1.0, 0.5};
    const double startDistance = std::hypot(deer.position.x - player.x, deer.position.y - player.y);

    for (int i = 0; i < stepsPerSecond; ++i) stepCreature(deer, terrain, player, step);
    const double endDistance = std::hypot(deer.position.x - player.x, deer.position.y - player.y);
    REQUIRE(endDistance > startDistance + 2.0);
    REQUIRE(deer.position.x > 0.5);  // ran away from the player, who is to the left
}

TEST_CASE("creatures stay close to home", "[wildlife]") {
    const HandmadeTerrain terrain([](int, int) { return Biome::Grassland; });
    Creature rabbit = makeCreature(Species::Rabbit, {0.5, 0.5}, {0, 0}, 999);
    // Leash plus the distance it can cover before turning round.
    const double allowance = constants::creatureLeashRadiusInTiles + 3.0;
    for (int i = 0; i < 120 * stepsPerSecond; ++i) {
        stepCreature(rabbit, terrain, farAwayPlayer, step);
        REQUIRE(std::hypot(rabbit.position.x - rabbit.home.x, rabbit.position.y - rabbit.home.y) < allowance);
    }
}

TEST_CASE("every species has a name and lives somewhere", "[wildlife]") {
    for (int index = 0; index < speciesCount; ++index) {
        const Species species = static_cast<Species>(index);
        REQUIRE(speciesName(species) != "Unknown");
        bool hasHome = false;
        for (int biomeIndex = 0; biomeIndex < biomeCount; ++biomeIndex) {
            for (float roll : {0.1f, 0.5f, 0.9f}) {
                Species found = Species::Rabbit;
                if (speciesForBiome(static_cast<Biome>(biomeIndex), roll, found) && found == species) hasHome = true;
            }
        }
        REQUIRE(hasHome);
    }
}
