#include <catch2/catch_test_macros.hpp>
#include <memory>
#include <unordered_map>

#include "exploration.h"

using namespace wl;

namespace {
class MapChunkSource : public ChunkSource {
public:
    const Chunk* findChunk(ChunkCoord coord) const override {
        const auto found = chunks.find(coord);
        return found == chunks.end() ? nullptr : found->second.get();
    }
    std::unordered_map<ChunkCoord, std::unique_ptr<Chunk>, ChunkCoordHash> chunks;
};
}  // namespace

TEST_CASE("revealing marks a square of chunks once", "[exploration]") {
    Exploration exploration;
    REQUIRE(exploration.reveal({0, 0}, 1) == 9);
    REQUIRE(exploration.reveal({1, 0}, 1) == 3);  // only the new column
    REQUIRE(exploration.revealedCount() == 12);
    REQUIRE(exploration.isRevealed({-1, -1}));
    REQUIRE(exploration.isRevealed({2, 1}));
    REQUIRE_FALSE(exploration.isRevealed({3, 0}));
}

TEST_CASE("minimap summaries from a loaded chunk match ones rebuilt from the seed", "[exploration]") {
    const World world(808);
    for (const ChunkCoord coord : {ChunkCoord{0, 0}, ChunkCoord{-5, 3}, ChunkCoord{12, -40}}) {
        const auto chunk = world.generateChunk(coord);
        REQUIRE(summarizeChunk(*chunk) == summarizeFromWorld(world, coord));
    }
}

TEST_CASE("summaries are built a few at a time", "[exploration]") {
    const World world(1);
    const MapChunkSource nothingLoaded;
    Exploration exploration;
    exploration.reveal({0, 0}, 2);  // 25 chunks
    REQUIRE(exploration.summary({0, 0}) == nullptr);

    exploration.buildSummaries(world, nothingLoaded, 10);
    REQUIRE(exploration.pendingSummaryCount() == 15);
    exploration.buildSummaries(world, nothingLoaded, 100);
    REQUIRE(exploration.pendingSummaryCount() == 0);
    REQUIRE(exploration.summary({0, 0}) != nullptr);
    REQUIRE(exploration.summary({2, -2}) != nullptr);
}

TEST_CASE("revealed chunks restore and come out sorted", "[exploration]") {
    Exploration exploration;
    exploration.restore({{3, 1}, {-1, 0}, {0, -2}, {3, 1}});
    REQUIRE(exploration.revealedCount() == 3);
    const std::vector<ChunkCoord> sorted = exploration.revealedChunksSorted();
    REQUIRE(sorted == std::vector<ChunkCoord>{{0, -2}, {-1, 0}, {3, 1}});
    REQUIRE(exploration.pendingSummaryCount() == 3);
}

TEST_CASE("region landmark lookups are cached and match the direct lookup", "[exploration]") {
    const World world(2024);
    Exploration exploration;
    for (int regionX = 0; regionX < 10; ++regionX) {
        const std::optional<Prop>& cached = exploration.regionLandmark(world, regionX, 1);
        const std::optional<Prop> direct = findRegionLandmark(world, regionX, 1);
        REQUIRE(cached.has_value() == direct.has_value());
        if (direct) REQUIRE(*cached == *direct);
        // Second call returns the same stored object.
        REQUIRE(&exploration.regionLandmark(world, regionX, 1) == &cached);
    }
}
