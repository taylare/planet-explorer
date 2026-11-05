#include "chunk_cache.h"

#include <algorithm>
#include <chrono>

namespace wl {

namespace {
double steadyClockMs() {
    using namespace std::chrono;
    return duration<double, std::milli>(steady_clock::now().time_since_epoch()).count();
}

int squaredDistance(ChunkCoord from, ChunkCoord to) {
    const int deltaX = to.x - from.x;
    const int deltaY = to.y - from.y;
    return deltaX * deltaX + deltaY * deltaY;
}
}  // namespace

ChunkRegion ChunkRegion::around(ChunkCoord centre, int radiusX, int radiusY) {
    return {centre.x - radiusX, centre.y - radiusY, centre.x + radiusX, centre.y + radiusY};
}

bool ChunkRegion::contains(ChunkCoord coord) const {
    return coord.x >= minChunkX && coord.x <= maxChunkX && coord.y >= minChunkY && coord.y <= maxChunkY;
}

int ChunkRegion::chunkCount() const {
    return (maxChunkX - minChunkX + 1) * (maxChunkY - minChunkY + 1);
}

ChunkCache::ChunkCache(const World& world, std::size_t capacityInChunks, Clock clock)
    : world_(world), capacity_(capacityInChunks), clock_(clock ? std::move(clock) : Clock(steadyClockMs)) {}

const Chunk* ChunkCache::findChunk(ChunkCoord coord) const {
    const auto found = entries_.find(coord);
    return found == entries_.end() ? nullptr : found->second.chunk.get();
}

void ChunkCache::markUsed(Entry& entry) {
    // splice moves the node to the front without invalidating the iterator.
    recencyOrder_.splice(recencyOrder_.begin(), recencyOrder_, entry.recencyPosition);
}

StreamingStats ChunkCache::update(const ChunkRegion& requiredRegion, ChunkCoord centre, double budgetMs) {
    // Everything in the region counts as used this frame. Anything outside
    // keeps its old position and drifts toward eviction.
    missingScratch_.clear();
    for (int chunkY = requiredRegion.minChunkY; chunkY <= requiredRegion.maxChunkY; ++chunkY) {
        for (int chunkX = requiredRegion.minChunkX; chunkX <= requiredRegion.maxChunkX; ++chunkX) {
            const ChunkCoord coord{chunkX, chunkY};
            const auto found = entries_.find(coord);
            if (found != entries_.end()) {
                markUsed(found->second);
            } else {
                missingScratch_.push_back(coord);
            }
        }
    }

    // Nearest first, so the ground under the player fills in before the edges.
    // Ties break on y then x so the order is the same on every run.
    std::sort(missingScratch_.begin(), missingScratch_.end(), [centre](ChunkCoord a, ChunkCoord b) {
        const int distanceA = squaredDistance(centre, a);
        const int distanceB = squaredDistance(centre, b);
        if (distanceA != distanceB) return distanceA < distanceB;
        if (a.y != b.y) return a.y < b.y;
        return a.x < b.x;
    });

    StreamingStats stats;
    const double startMs = clock_();
    for (const ChunkCoord& coord : missingScratch_) {
        if (stats.chunksGenerated > 0 && clock_() - startMs >= budgetMs) break;

        recencyOrder_.push_front(coord);
        entries_[coord] = Entry{world_.generateChunk(coord), recencyOrder_.begin()};
        ++stats.chunksGenerated;
    }
    stats.millisecondsSpent = clock_() - startMs;
    stats.chunksPending = static_cast<int>(missingScratch_.size()) - stats.chunksGenerated;

    evictOverCapacity(requiredRegion);
    return stats;
}

void ChunkCache::evictOverCapacity(const ChunkRegion& protectedRegion) {
    // Walk from the least recently used end. Region chunks were just marked
    // used so they sit at the front, but skip them explicitly anyway: the
    // guarantee shouldn't depend on that ordering detail.
    auto position = recencyOrder_.end();
    while (entries_.size() > capacity_ && position != recencyOrder_.begin()) {
        --position;
        if (protectedRegion.contains(*position)) continue;
        entries_.erase(*position);
        position = recencyOrder_.erase(position);  // returns the element after, so --position next lands before it
    }
}

}  // namespace wl
