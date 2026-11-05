#include "exploration.h"

#include <algorithm>

#include "props.h"

namespace wl {

namespace {
// Sample near the middle of each minimap pixel's patch of tiles.
constexpr int sampleOffsetInPatch = constants::minimapTilesPerPixel / 2;
}  // namespace

MinimapSummary summarizeChunk(const Chunk& chunk) {
    MinimapSummary summary{};
    for (int summaryY = 0; summaryY < minimapSummarySide; ++summaryY) {
        for (int summaryX = 0; summaryX < minimapSummarySide; ++summaryX) {
            const int localX = summaryX * constants::minimapTilesPerPixel + sampleOffsetInPatch;
            const int localY = summaryY * constants::minimapTilesPerPixel + sampleOffsetInPatch;
            summary[summaryY * minimapSummarySide + summaryX] = chunk.biomes[Chunk::indexFor(localX, localY)];
        }
    }
    return summary;
}

MinimapSummary summarizeFromWorld(const World& world, ChunkCoord coord) {
    // Same sample points as summarizeChunk, read straight from the noise.
    // Seamless chunks guarantee the two always agree.
    MinimapSummary summary{};
    for (int summaryY = 0; summaryY < minimapSummarySide; ++summaryY) {
        for (int summaryX = 0; summaryX < minimapSummarySide; ++summaryX) {
            const int tileX = chunkOriginTile(coord.x) + summaryX * constants::minimapTilesPerPixel + sampleOffsetInPatch;
            const int tileY = chunkOriginTile(coord.y) + summaryY * constants::minimapTilesPerPixel + sampleOffsetInPatch;
            summary[summaryY * minimapSummarySide + summaryX] = world.sampleTerrain(tileX, tileY).biome;
        }
    }
    return summary;
}

int Exploration::reveal(ChunkCoord centre, int radius) {
    int newlyRevealed = 0;
    for (int chunkY = centre.y - radius; chunkY <= centre.y + radius; ++chunkY) {
        for (int chunkX = centre.x - radius; chunkX <= centre.x + radius; ++chunkX) {
            const ChunkCoord coord{chunkX, chunkY};
            if (revealed_.insert(coord).second) {
                pendingSummaries_.push_back(coord);
                ++newlyRevealed;
            }
        }
    }
    return newlyRevealed;
}

const MinimapSummary* Exploration::summary(ChunkCoord coord) const {
    const auto found = summaries_.find(coord);
    return found == summaries_.end() ? nullptr : &found->second;
}

void Exploration::buildSummaries(const World& world, const ChunkSource& chunks, int maxCount) {
    int built = 0;
    while (!pendingSummaries_.empty() && built < maxCount) {
        // Newest first: those are the ones near the player, which matter most.
        const ChunkCoord coord = pendingSummaries_.back();
        pendingSummaries_.pop_back();
        if (summaries_.count(coord) != 0) continue;

        const Chunk* loaded = chunks.findChunk(coord);
        summaries_[coord] = loaded ? summarizeChunk(*loaded) : summarizeFromWorld(world, coord);
        ++built;
    }
}

std::vector<ChunkCoord> Exploration::revealedChunksSorted() const {
    std::vector<ChunkCoord> sorted(revealed_.begin(), revealed_.end());
    std::sort(sorted.begin(), sorted.end(), [](ChunkCoord a, ChunkCoord b) {
        return a.y != b.y ? a.y < b.y : a.x < b.x;
    });
    return sorted;
}

void Exploration::restore(const std::vector<ChunkCoord>& revealedChunks) {
    revealed_.clear();
    summaries_.clear();
    pendingSummaries_.clear();
    for (const ChunkCoord& coord : revealedChunks) {
        if (revealed_.insert(coord).second) pendingSummaries_.push_back(coord);
    }
}

const std::optional<Prop>& Exploration::regionLandmark(const World& world, int regionX, int regionY) {
    const ChunkCoord regionKey{regionX, regionY};
    auto found = landmarkCache_.find(regionKey);
    if (found == landmarkCache_.end()) {
        found = landmarkCache_.emplace(regionKey, findRegionLandmark(world, regionX, regionY)).first;
    }
    return found->second;
}

}  // namespace wl
