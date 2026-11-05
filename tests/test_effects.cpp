#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include <functional>
#include <memory>
#include <unordered_map>

#include "constants.h"
#include "effects.h"

using namespace wl;

namespace {
constexpr double step = constants::simulationStepSeconds;

class FlatTerrain : public ChunkSource {
public:
    explicit FlatTerrain(Biome biome) {
        for (int chunkY = -2; chunkY <= 2; ++chunkY) {
            for (int chunkX = -2; chunkX <= 2; ++chunkX) {
                auto chunk = std::make_unique<Chunk>();
                chunk->coord = {chunkX, chunkY};
                chunk->biomes.fill(biome);
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

// Walks right for some seconds, updating trails each step. Returns the end time.
double walk(Player& player, TrailEffects& trails, const ChunkSource& terrain, Biome ground, double seconds,
            double startTime = 0.0) {
    double now = startTime;
    for (int i = 0; i < static_cast<int>(seconds / step); ++i) {
        player.update({1.0, 0.0}, terrain, step);
        now += step;
        trails.update(player, ground, now);
    }
    return now;
}
}  // namespace

TEST_CASE("walking on sand leaves alternating footprints", "[effects]") {
    const FlatTerrain beach(Biome::Beach);
    Player player;
    player.placeAt(0.5, 0.5);
    TrailEffects trails;
    walk(player, trails, beach, Biome::Beach, 2.0);

    const auto& prints = trails.footprints();
    REQUIRE(prints.size() > 10);
    // Walking right, feet alternate above and below the path.
    for (std::size_t index = 1; index < prints.size(); ++index) {
        const bool previousAbove = prints[index - 1].tileY < 0.5;
        const bool currentAbove = prints[index].tileY < 0.5;
        REQUIRE(previousAbove != currentAbove);
    }
    REQUIRE(trails.ripples().empty());
}

TEST_CASE("grass leaves no footprints", "[effects]") {
    const FlatTerrain grass(Biome::Grassland);
    Player player;
    player.placeAt(0.5, 0.5);
    TrailEffects trails;
    walk(player, trails, grass, Biome::Grassland, 2.0);
    REQUIRE(trails.footprints().empty());
}

TEST_CASE("wading makes ripples, even standing still", "[effects]") {
    const FlatTerrain shallows(Biome::ShallowWater);
    Player player;
    player.placeAt(0.5, 0.5);
    TrailEffects trails;
    const double now = walk(player, trails, shallows, Biome::ShallowWater, 2.0);
    REQUIRE_FALSE(trails.ripples().empty());
    REQUIRE(trails.footprints().empty());

    // Stand still past the old ripples' lifetime: idle ripples keep coming.
    TrailEffects idleTrails;
    double time = now;
    for (int i = 0; i < 60 * 3; ++i) {
        player.update({}, shallows, step);
        time += step;
        idleTrails.update(player, Biome::ShallowWater, time);
    }
    REQUIRE_FALSE(idleTrails.ripples().empty());
}

TEST_CASE("footprints fade away and the count is capped", "[effects]") {
    const FlatTerrain snow(Biome::Snow);
    Player player;
    player.placeAt(-60.5, 0.5);
    TrailEffects trails;
    double now = walk(player, trails, snow, Biome::Snow, 20.0);
    REQUIRE(static_cast<int>(trails.footprints().size()) <= constants::maxFootprints);

    // Stop and wait past the lifetime: all prints are gone.
    for (int i = 0; i < static_cast<int>((constants::footprintLifetimeSeconds + 1.0) / step); ++i) {
        player.update({}, snow, step);
        now += step;
        trails.update(player, Biome::Snow, now);
    }
    REQUIRE(trails.footprints().empty());
}
