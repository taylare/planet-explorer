// Draws the world into the framebuffer.
#pragma once

#include <array>
#include <cstdint>

#include "constants.h"
#include "framebuffer.h"
#include "world.h"

namespace wl {

struct Camera {
    double centerTileX = 0.0;
    double centerTileY = 0.0;
    int tileSizeInPixels = constants::defaultTileSizeInPixels;
};

// Inclusive range of tiles touched by the view.
struct TileBounds {
    int minTileX = 0;
    int minTileY = 0;
    int maxTileX = 0;
    int maxTileY = 0;
};

TileBounds visibleTileBounds(const Camera& camera, int framebufferWidth, int framebufferHeight);

class Renderer {
public:
    Renderer();

    void drawTerrain(Framebuffer& framebuffer, const Camera& camera, const ChunkSource& chunks) const;

private:
    // Small tiled pattern of brightness steps, indexed by world pixel so it
    // stays put as the camera moves. 64 is big enough that repeats don't show.
    static constexpr int grainPatternSize = 64;
    std::array<std::uint8_t, grainPatternSize * grainPatternSize> grainPattern_{};
};

}  // namespace wl
