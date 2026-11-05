// Which chunks the player has seen, for the minimap's fog of war.
//
// Only the list of revealed chunk coordinates is saved. Each revealed chunk
// also gets a tiny biome summary for drawing the minimap, but that can always
// be rebuilt from the seed, so it isn't stored.
#pragma once

#include <array>
#include <optional>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "constants.h"
#include "world.h"

namespace wl {

inline constexpr int minimapSummarySide = constants::chunkSizeInTiles / constants::minimapTilesPerPixel;
static_assert(constants::chunkSizeInTiles % constants::minimapTilesPerPixel == 0,
              "minimap pixels must line up with chunk edges");

// One biome per minimap pixel inside a chunk.
using MinimapSummary = std::array<Biome, minimapSummarySide * minimapSummarySide>;

MinimapSummary summarizeChunk(const Chunk& chunk);
MinimapSummary summarizeFromWorld(const World& world, ChunkCoord coord);

class Exploration {
public:
    // Reveals the square of chunks around centre. Returns how many were new.
    int reveal(ChunkCoord centre, int radius);

    bool isRevealed(ChunkCoord coord) const { return revealed_.count(coord) != 0; }
    std::size_t revealedCount() const { return revealed_.size(); }

    // Null until the summary has been built.
    const MinimapSummary* summary(ChunkCoord coord) const;

    // Builds up to maxCount missing summaries, from the loaded chunk when
    // there is one, otherwise from the seed.
    void buildSummaries(const World& world, const ChunkSource& chunks, int maxCount);
    std::size_t pendingSummaryCount() const { return pendingSummaries_.size(); }

    // Sorted, so saves come out the same every time.
    std::vector<ChunkCoord> revealedChunksSorted() const;
    void restore(const std::vector<ChunkCoord>& revealedChunks);

    // Cached rare landmark lookup for a landmark region (it costs a few
    // terrain samples, and the minimap asks every frame).
    const std::optional<Prop>& regionLandmark(const World& world, int regionX, int regionY);

private:
    std::unordered_set<ChunkCoord, ChunkCoordHash> revealed_;
    std::unordered_map<ChunkCoord, MinimapSummary, ChunkCoordHash> summaries_;
    std::vector<ChunkCoord> pendingSummaries_;
    std::unordered_map<ChunkCoord, std::optional<Prop>, ChunkCoordHash> landmarkCache_;
};

}  // namespace wl
