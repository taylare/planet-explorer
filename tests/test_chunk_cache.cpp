#include <catch2/catch_test_macros.hpp>
#include <limits>

#include "chunk_cache.h"
#include "hash.h"

using namespace wl;

namespace {
constexpr double unlimitedBudget = std::numeric_limits<double>::infinity();

// Fake clock that jumps forward by a fixed amount every time it's read.
ChunkCache::Clock steppingClock(double& nowMs, double stepMs) {
    return [&nowMs, stepMs] {
        nowMs += stepMs;
        return nowMs;
    };
}

StreamingStats loadAround(ChunkCache& cache, ChunkCoord centre, int radius) {
    return cache.update(ChunkRegion::around(centre, radius, radius), centre, unlimitedBudget);
}
}  // namespace

TEST_CASE("cache loads the whole required region", "[chunk_cache]") {
    const World world(123);
    ChunkCache cache(world, 100);
    const ChunkRegion region = ChunkRegion::around({0, 0}, 2, 1);
    const StreamingStats stats = cache.update(region, {0, 0}, unlimitedBudget);

    REQUIRE(stats.chunksGenerated == 15);
    REQUIRE(stats.chunksPending == 0);
    REQUIRE(cache.size() == 15);
    for (int y = region.minChunkY; y <= region.maxChunkY; ++y) {
        for (int x = region.minChunkX; x <= region.maxChunkX; ++x) REQUIRE(cache.contains({x, y}));
    }
}

TEST_CASE("cached chunks match direct generation", "[chunk_cache]") {
    const World world(321);
    ChunkCache cache(world, 100);
    loadAround(cache, {0, 0}, 1);
    const Chunk* cached = cache.findChunk({1, -1});
    REQUIRE(cached != nullptr);
    const auto direct = world.generateChunk({1, -1});
    REQUIRE(cached->biomes == direct->biomes);
    REQUIRE(cached->elevations == direct->elevations);
}

TEST_CASE("LRU evicts the least recently used chunk", "[chunk_cache]") {
    const World world(5);
    ChunkCache cache(world, 3);

    loadAround(cache, {0, 0}, 0);
    loadAround(cache, {1, 0}, 0);
    loadAround(cache, {2, 0}, 0);
    REQUIRE(cache.size() == 3);

    // Fourth chunk pushes out the oldest.
    loadAround(cache, {3, 0}, 0);
    REQUIRE(cache.size() == 3);
    REQUIRE_FALSE(cache.contains({0, 0}));
    REQUIRE(cache.contains({1, 0}));

    // Revisiting (1,0) makes it recent again, so (2,0) goes next instead.
    loadAround(cache, {1, 0}, 0);
    loadAround(cache, {4, 0}, 0);
    REQUIRE(cache.contains({1, 0}));
    REQUIRE_FALSE(cache.contains({2, 0}));
    REQUIRE(cache.contains({3, 0}));
    REQUIRE(cache.contains({4, 0}));
}

TEST_CASE("chunks inside the view region are never evicted", "[chunk_cache]") {
    const World world(6);
    // Capacity far smaller than the 9-chunk region on purpose.
    ChunkCache cache(world, 2);
    loadAround(cache, {0, 0}, 1);
    REQUIRE(cache.size() == 9);  // over capacity, but all protected

    // Jump away: the old region should go, the new one should all stay.
    loadAround(cache, {10, 10}, 1);
    REQUIRE(cache.size() == 9);
    REQUIRE_FALSE(cache.contains({0, 0}));
    for (int y = 9; y <= 11; ++y) {
        for (int x = 9; x <= 11; ++x) REQUIRE(cache.contains({x, y}));
    }
}

TEST_CASE("random walk never loses a chunk in view and respects capacity", "[chunk_cache]") {
    const World world(7);
    constexpr std::size_t capacity = 40;
    constexpr int radius = 2;  // 25 chunk region
    ChunkCache cache(world, capacity);

    ChunkCoord position{0, 0};
    for (int step = 0; step < 150; ++step) {
        const std::uint32_t roll = hashCoords(42, step, 0) % 4;
        if (roll == 0) ++position.x;
        if (roll == 1) --position.x;
        if (roll == 2) ++position.y;
        if (roll == 3) --position.y;

        const ChunkRegion region = ChunkRegion::around(position, radius, radius);
        cache.update(region, position, unlimitedBudget);

        REQUIRE(cache.size() <= capacity);
        for (int y = region.minChunkY; y <= region.maxChunkY; ++y) {
            for (int x = region.minChunkX; x <= region.maxChunkX; ++x) REQUIRE(cache.contains({x, y}));
        }
    }
}

TEST_CASE("generation respects the time budget, nearest first", "[chunk_cache]") {
    const World world(8);
    double nowMs = 0.0;
    // Every clock read moves time 10 ms, so any chunk "costs" more than the budget.
    ChunkCache cache(world, 100, steppingClock(nowMs, 10.0));
    const ChunkRegion region = ChunkRegion::around({0, 0}, 1, 1);

    StreamingStats stats = cache.update(region, {0, 0}, 4.0);
    REQUIRE(stats.chunksGenerated == 1);
    REQUIRE(stats.chunksPending == 8);
    REQUIRE(cache.contains({0, 0}));  // centre came first

    // Next update fills one of the four direct neighbours before any corner.
    cache.update(region, {0, 0}, 4.0);
    const bool neighbourLoaded = cache.contains({0, -1}) || cache.contains({-1, 0}) ||
                                 cache.contains({1, 0}) || cache.contains({0, 1});
    REQUIRE(neighbourLoaded);
    REQUIRE_FALSE(cache.contains({-1, -1}));
}

TEST_CASE("a zero budget still makes progress every frame", "[chunk_cache]") {
    const World world(9);
    ChunkCache cache(world, 100);
    const ChunkRegion region = ChunkRegion::around({0, 0}, 1, 1);
    int updates = 0;
    StreamingStats stats;
    do {
        stats = cache.update(region, {0, 0}, 0.0);
        ++updates;
    } while (stats.chunksPending > 0 && updates < 100);
    REQUIRE(updates == 9);
    REQUIRE(cache.size() == 9);
}

TEST_CASE("chunk region helpers", "[chunk_cache]") {
    const ChunkRegion region = ChunkRegion::around({-2, 3}, 2, 1);
    REQUIRE(region.chunkCount() == 15);
    REQUIRE(region.contains({-4, 2}));
    REQUIRE(region.contains({0, 4}));
    REQUIRE_FALSE(region.contains({1, 3}));
    REQUIRE_FALSE(region.contains({-2, 5}));
}
