// Draws the world into the framebuffer.
#pragma once

#include <array>
#include <cstdint>

#include "constants.h"
#include "framebuffer.h"
#include "sprite.h"
#include "world.h"

namespace wl {

struct Camera {
    double centerTileX = 0.0;
    double centerTileY = 0.0;
    int tileSizeInPixels = constants::defaultTileSizeInPixels;
};

// World pixel at the framebuffer's top-left corner. Snapped to whole pixels
// so the image doesn't shimmer while the camera moves.
struct ScreenOrigin {
    std::int64_t leftWorldPixel = 0;
    std::int64_t topWorldPixel = 0;
    int tileSizeInPixels = 1;
};

ScreenOrigin screenOriginFor(const Camera& camera, int framebufferWidth, int framebufferHeight);
int worldToScreenX(const ScreenOrigin& origin, double tileX);
int worldToScreenY(const ScreenOrigin& origin, double tileY);

// Inclusive range of tiles touched by the view.
struct TileBounds {
    int minTileX = 0;
    int minTileY = 0;
    int maxTileX = 0;
    int maxTileY = 0;
};

TileBounds visibleTileBounds(const Camera& camera, int framebufferWidth, int framebufferHeight);

// What the renderer needs to know about the player, nothing more.
struct PlayerAppearance {
    double tileX = 0.0;  // feet position
    double tileY = 0.0;
    bool facingLeft = false;
    bool wading = false;
    int walkFrame = 0;  // 0 idle, 1 and 2 alternate while walking
};

inline constexpr int playerWalkFrameCount = 3;

class Renderer {
public:
    Renderer();

    void drawTerrain(Framebuffer& framebuffer, const Camera& camera, const ChunkSource& chunks) const;
    void drawPlayer(Framebuffer& framebuffer, const Camera& camera, const PlayerAppearance& player) const;

private:
    // Small tiled pattern of brightness steps, indexed by world pixel so it
    // stays put as the camera moves. 64 is big enough that repeats don't show.
    static constexpr int grainPatternSize = 64;
    std::array<std::uint8_t, grainPatternSize * grainPatternSize> grainPattern_{};

    std::array<Sprite, playerWalkFrameCount> playerFrames_;
};

}  // namespace wl
