#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <functional>
#include <memory>
#include <unordered_map>

#include "constants.h"
#include "entities.h"

using namespace wl;

namespace {
constexpr double step = constants::simulationStepSeconds;
constexpr int stepsPerSecond = 60;

// Hand-built terrain: chunks -2..2 on both axes, biome chosen per tile.
class HandmadeTerrain : public ChunkSource {
public:
    explicit HandmadeTerrain(const std::function<Biome(int, int)>& biomeForTile) {
        for (int chunkY = -2; chunkY <= 2; ++chunkY) {
            for (int chunkX = -2; chunkX <= 2; ++chunkX) {
                auto chunk = std::make_unique<Chunk>();
                chunk->coord = {chunkX, chunkY};
                for (int localY = 0; localY < constants::chunkSizeInTiles; ++localY) {
                    for (int localX = 0; localX < constants::chunkSizeInTiles; ++localX) {
                        chunk->biomes[Chunk::indexFor(localX, localY)] = biomeForTile(
                            chunkOriginTile(chunkX) + localX, chunkOriginTile(chunkY) + localY);
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

double distanceWalkedIn(Biome biome, double seconds) {
    const HandmadeTerrain terrain([biome](int, int) { return biome; });
    Player player;
    player.placeAt(0.5, 0.5);
    for (int i = 0; i < static_cast<int>(seconds * stepsPerSecond); ++i) {
        player.update({1.0, 0.0}, terrain, step);
    }
    return player.position().x - 0.5;
}
}  // namespace

TEST_CASE("player walks close to full speed on grass", "[entities]") {
    const double distance = distanceWalkedIn(Biome::Grassland, 2.0);
    const double ideal = constants::walkSpeedTilesPerSecond * 2.0;
    // A little less than ideal because of the ease-in.
    REQUIRE(distance < ideal);
    REQUIRE(distance > ideal * 0.9);
}

TEST_CASE("shallow water slows the player down", "[entities]") {
    const double onGrass = distanceWalkedIn(Biome::Grassland, 3.0);
    const double wading = distanceWalkedIn(Biome::ShallowWater, 3.0);
    REQUIRE(wading / onGrass == Catch::Approx(constants::shallowWaterSpeedMultiplier).margin(0.05));
}

TEST_CASE("deep ocean blocks movement", "[entities]") {
    const HandmadeTerrain terrain([](int tileX, int) { return tileX >= 5 ? Biome::DeepOcean : Biome::Grassland; });
    Player player;
    player.placeAt(2.5, 0.5);
    for (int i = 0; i < 3 * stepsPerSecond; ++i) player.update({1.0, 0.0}, terrain, step);
    REQUIRE(player.position().x < 5.0);
    REQUIRE(player.position().x > 4.5);  // got right up to the shore
}

TEST_CASE("player slides along a coast instead of sticking", "[entities]") {
    const HandmadeTerrain terrain([](int tileX, int) { return tileX >= 5 ? Biome::DeepOcean : Biome::Grassland; });
    Player player;
    player.placeAt(4.5, 0.5);
    const double diagonal = 0.70710678118654752;
    for (int i = 0; i < 2 * stepsPerSecond; ++i) player.update({diagonal, -diagonal}, terrain, step);
    REQUIRE(player.position().x < 5.0);
    REQUIRE(player.position().y < -3.0);
}

TEST_CASE("unloaded chunks count as blocked", "[entities]") {
    const HandmadeTerrain terrain([](int, int) { return Biome::Grassland; });
    // Chunks stop at x = 3 * 32 = 96.
    Player player;
    player.placeAt(94.5, 0.5);
    for (int i = 0; i < 2 * stepsPerSecond; ++i) player.update({1.0, 0.0}, terrain, step);
    REQUIRE(player.position().x < 96.0);
}

TEST_CASE("player faces the way they walk and wades in shallows", "[entities]") {
    const HandmadeTerrain terrain([](int tileX, int) { return tileX < 0 ? Biome::ShallowWater : Biome::Grassland; });
    Player player;
    player.placeAt(2.5, 0.5);
    REQUIRE_FALSE(player.isFacingLeft());
    REQUIRE_FALSE(player.isWading());

    for (int i = 0; i < 2 * stepsPerSecond; ++i) player.update({-1.0, 0.0}, terrain, step);
    REQUIRE(player.isFacingLeft());
    REQUIRE(player.isWading());
    REQUIRE(player.isMoving());
}

TEST_CASE("player eases to a stop when input is released", "[entities]") {
    const HandmadeTerrain terrain([](int, int) { return Biome::Grassland; });
    Player player;
    player.placeAt(0.5, 0.5);
    for (int i = 0; i < stepsPerSecond; ++i) player.update({1.0, 0.0}, terrain, step);
    for (int i = 0; i < stepsPerSecond; ++i) player.update({}, terrain, step);
    REQUIRE_FALSE(player.isMoving());
}

TEST_CASE("interpolated position blends between steps", "[entities]") {
    const HandmadeTerrain terrain([](int, int) { return Biome::Grassland; });
    Player player;
    player.placeAt(0.5, 0.5);
    for (int i = 0; i < 30; ++i) player.update({1.0, 0.0}, terrain, step);
    const double before = player.interpolatedPosition(0.0).x;
    const double after = player.interpolatedPosition(1.0).x;
    const double middle = player.interpolatedPosition(0.5).x;
    REQUIRE(after > before);
    REQUIRE(middle == Catch::Approx((before + after) / 2.0));
}
