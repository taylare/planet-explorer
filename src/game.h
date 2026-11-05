// The whole game, with no browser code in sight.
// main.cpp drives it on the web; tests drive it natively.
#pragma once

#include "chunk_cache.h"
#include "entities.h"
#include "fixed_timestep.h"
#include "framebuffer.h"
#include "input.h"
#include "renderer.h"
#include "world.h"

namespace wl {

class Game {
public:
    explicit Game(WorldSeed seed = constants::defaultWorldSeed);

    // Canvas size in physical device pixels.
    void resizeDevice(int deviceWidthInPixels, int deviceHeightInPixels);

    void setTileSizeInPixels(int tileSizeInPixels);

    void setDirectionHeld(Direction direction, bool held);
    void setDrag(bool active, double offsetXInCssPixels, double offsetYInCssPixels);
    void releaseAllInput();

    // Run however many sim steps are due, stream chunks, then draw one frame.
    void advanceFrame(double realDeltaSeconds);

    const Framebuffer& framebuffer() const { return framebuffer_; }
    const FramebufferLayout& layout() const { return layout_; }
    const World& world() const { return world_; }
    const Player& player() const { return player_; }
    const ChunkCache& chunkCache() const { return chunkCache_; }
    const StreamingStats& lastStreamingStats() const { return lastStreamingStats_; }
    double simulatedSeconds() const { return simulatedSeconds_; }

    // Region that must stay loaded right now: the screen plus a margin.
    ChunkRegion requiredChunkRegion() const;

private:
    void streamChunks(double budgetMs);
    void simulateStep(double stepSeconds);
    void render(double interpolationAlpha);

    // Declared first: the cache holds a reference to it.
    World world_;
    ChunkCache chunkCache_;

    FixedTimestepClock clock_;
    FramebufferLayout layout_;
    Framebuffer framebuffer_;
    Renderer renderer_;

    InputState input_;
    Player player_;

    // Camera position is simulated (it eases after the player) and
    // interpolated like the player for smooth rendering.
    Vec2 cameraPosition_;
    Vec2 previousCameraPosition_;
    int tileSizeInPixels_ = constants::defaultTileSizeInPixels;

    StreamingStats lastStreamingStats_;
    double simulatedSeconds_ = 0.0;
    bool hasRenderedFrame_ = false;
};

}  // namespace wl
