// Draws the world into the framebuffer: terrain, props, creatures, the
// player, then lighting and stars.
#pragma once

#include <array>
#include <cstdint>
#include <vector>

#include "art.h"
#include "constants.h"
#include "day_night.h"
#include "entities.h"
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

struct SceneView {
    Camera camera;
    const ChunkSource* chunks = nullptr;
    PlayerAppearance player;
    const std::vector<Creature>* creatures = nullptr;
    double interpolationAlpha = 1.0;
    LightingState lighting;
    double timeSeconds = 0.0;  // drives animation
};

class Renderer {
public:
    Renderer();

    void drawFrame(Framebuffer& framebuffer, const SceneView& scene);

    // Terrain only. Public so tests can check it on its own.
    void drawTerrain(Framebuffer& framebuffer, const Camera& camera, const ChunkSource& chunks,
                     double timeSeconds = 0.0);

private:
    // One sprite waiting to be drawn, sorted by foot height so nearer
    // things overlap further ones.
    struct DrawItem {
        const Sprite* sprite = nullptr;
        int footX = 0;
        int footY = 0;
        int scale = 1;
        bool flip = false;
        bool shadow = true;
        bool wading = false;
    };

    struct PendingLight {
        int x = 0;
        int y = 0;
        const constants::LightSettings* settings = nullptr;
        float strength = 1.0f;  // multiplied into the settings' intensity
    };

    struct Star {
        int x = 0;
        int y = 0;
        float phase = 0.0f;
    };

    void collectProps(Framebuffer& framebuffer, const SceneView& scene, const ScreenOrigin& origin, int scale);
    void collectCreatures(const SceneView& scene, const ScreenOrigin& origin, int scale);
    void drawItem(Framebuffer& framebuffer, const DrawItem& item) const;
    void drawSteam(Framebuffer& framebuffer, double timeSeconds, int scale) const;
    void applyLighting(Framebuffer& framebuffer, const LightingState& lighting, int tileSize);
    void drawStars(Framebuffer& framebuffer, const LightingState& lighting, double timeSeconds);
    void rebuildStarsIfNeeded(int width, int height);

    // Small tiled pattern of brightness steps, indexed by world pixel so it
    // stays put as the camera moves. 64 is big enough that repeats don't show.
    static constexpr int grainPatternSize = 64;
    std::array<std::uint8_t, grainPatternSize * grainPatternSize> grainPattern_{};

    SpriteLibrary sprites_;

    // Scratch buffers kept between frames to avoid allocating every frame.
    std::vector<std::uint8_t> waterMask_;     // 1 where the pixel shows water
    std::vector<int> waterRowOffsets_;        // shimmer offset per row
    std::vector<std::uint16_t> lightBuffer_;  // RGB light per pixel, 256 means +1.0
    std::vector<DrawItem> drawItems_;
    std::vector<PendingLight> lights_;
    std::vector<std::array<int, 3>> steamSources_;  // x, y, variant
    std::vector<Star> stars_;
    int starsWidth_ = 0;
    int starsHeight_ = 0;
};

}  // namespace wl
