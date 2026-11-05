// Streams chunks in around the player and evicts old ones.
//
// Each frame the game says which region must be loaded. Missing chunks are
// generated nearest first until the time budget runs out. When the cache is
// over capacity, the least recently used chunks outside the region go first.
// Chunks inside the region are never evicted, even if that means running
// over capacity for a while.
#pragma once

#include <cstddef>
#include <functional>
#include <list>
#include <memory>
#include <unordered_map>
#include <vector>

#include "world.h"

namespace wl {

// Inclusive rectangle of chunk coordinates.
struct ChunkRegion {
    int minChunkX = 0;
    int minChunkY = 0;
    int maxChunkX = 0;
    int maxChunkY = 0;

    static ChunkRegion around(ChunkCoord centre, int radiusX, int radiusY);
    bool contains(ChunkCoord coord) const;
    int chunkCount() const;
};

struct StreamingStats {
    int chunksGenerated = 0;
    int chunksPending = 0;  // still missing after this update
    double millisecondsSpent = 0.0;
};

class ChunkCache : public ChunkSource {
public:
    // Returns milliseconds from any fixed point. Injectable so tests can fake time.
    using Clock = std::function<double()>;

    ChunkCache(const World& world, std::size_t capacityInChunks, Clock clock = {});

    const Chunk* findChunk(ChunkCoord coord) const override;

    // Load what's missing in the region (within budget), then evict.
    // A budget of zero still generates one chunk.
    StreamingStats update(const ChunkRegion& requiredRegion, ChunkCoord centre, double budgetMs);

    void setCapacity(std::size_t capacityInChunks) { capacity_ = capacityInChunks; }
    std::size_t capacity() const { return capacity_; }
    std::size_t size() const { return entries_.size(); }
    bool contains(ChunkCoord coord) const { return entries_.count(coord) != 0; }

private:
    struct Entry {
        std::unique_ptr<Chunk> chunk;
        std::list<ChunkCoord>::iterator recencyPosition;
    };

    void markUsed(Entry& entry);
    void evictOverCapacity(const ChunkRegion& protectedRegion);

    const World& world_;
    std::size_t capacity_;
    Clock clock_;

    std::unordered_map<ChunkCoord, Entry, ChunkCoordHash> entries_;

    // Front is most recently used, back is the next eviction candidate.
    // A list lets us move an entry to the front in O(1) via its stored iterator.
    std::list<ChunkCoord> recencyOrder_;

    std::vector<ChunkCoord> missingScratch_;  // reused to avoid allocating every frame
};

}  // namespace wl
