// The whole game, with no browser code in sight.
// main.cpp drives it on the web; tests drive it natively.
#pragma once

#include <memory>
#include <unordered_map>

#include "fixed_timestep.h"
#include "framebuffer.h"
#include "renderer.h"
#include "world.h"

namespace wl {

class Game {
public:
    explicit Game(WorldSeed seed = constants::defaultWorldSeed);

    // Canvas size in physical device pixels.
    void resizeDevice(int deviceWidthInPixels, int deviceHeightInPixels);

    void setTileSizeInPixels(int tileSizeInPixels);

    // Run however many sim steps are due, then draw one frame.
    void advanceFrame(double realDeltaSeconds);

    const Framebuffer& framebuffer() const { return framebuffer_; }
    const FramebufferLayout& layout() const { return layout_; }
    const World& world() const { return world_; }
    const Camera& camera() const { return camera_; }
    double simulatedSeconds() const { return simulatedSeconds_; }

private:
    // Stand-in until the streaming chunk cache arrives in Stage 3: generates
    // whatever is on screen and keeps it forever.
    class TemporaryChunkStore : public ChunkSource {
    public:
        const Chunk* findChunk(ChunkCoord coord) const override;
        std::unordered_map<ChunkCoord, std::unique_ptr<Chunk>, ChunkCoordHash> chunks;
    };

    void generateVisibleChunks();
    void simulateStep(double stepSeconds);
    void render();

    World world_;
    FixedTimestepClock clock_;
    FramebufferLayout layout_;
    Framebuffer framebuffer_;
    Camera camera_;
    Renderer renderer_;
    TemporaryChunkStore chunkStore_;
    double simulatedSeconds_ = 0.0;
};

}  // namespace wl
