#include <catch2/catch_test_macros.hpp>
#include <cstdlib>
#include <memory>
#include <unordered_map>

#include "constants.h"
#include "renderer.h"

using namespace wl;

namespace {
class MapChunkSource : public ChunkSource {
public:
    const Chunk* findChunk(ChunkCoord coord) const override {
        const auto found = chunks.find(coord);
        return found == chunks.end() ? nullptr : found->second.get();
    }
    std::unordered_map<ChunkCoord, std::unique_ptr<Chunk>, ChunkCoordHash> chunks;
};
}  // namespace

TEST_CASE("visible tile bounds cover the whole framebuffer", "[renderer]") {
    Camera camera;
    camera.centerTileX = -10.25;
    camera.centerTileY = 3.75;
    camera.tileSizeInPixels = 8;
    const TileBounds bounds = visibleTileBounds(camera, 640, 360);
    // 640 pixels at 8 per tile is 80 tiles, plus one partial when not aligned.
    REQUIRE(bounds.maxTileX - bounds.minTileX + 1 >= 80);
    REQUIRE(bounds.maxTileY - bounds.minTileY + 1 >= 45);
    REQUIRE(bounds.minTileX <= -10 - 40);
    REQUIRE(bounds.maxTileX >= -10 + 39);
}

TEST_CASE("missing chunks draw as the unloaded colour", "[renderer]") {
    Framebuffer framebuffer;
    framebuffer.resize(64, 64);
    const Renderer renderer;
    const MapChunkSource emptySource;
    renderer.drawTerrain(framebuffer, Camera{}, emptySource);

    // Grain still applies, so check brightness is close rather than exact.
    const PackedColor pixel = framebuffer.at(10, 10);
    REQUIRE(std::abs(static_cast<int>(pixel & 0xff) - constants::unloadedColor.red) <= 1);
}

TEST_CASE("terrain fills every pixel with an opaque colour", "[renderer]") {
    const World world(4);
    MapChunkSource source;
    for (int chunkY = -2; chunkY <= 1; ++chunkY) {
        for (int chunkX = -2; chunkX <= 1; ++chunkX) {
            source.chunks[{chunkX, chunkY}] = world.generateChunk({chunkX, chunkY});
        }
    }

    Framebuffer framebuffer;
    framebuffer.resize(200, 120);
    framebuffer.clear(packRgba(0, 0, 0, 0));
    const Renderer renderer;
    Camera camera;
    camera.tileSizeInPixels = 4;
    renderer.drawTerrain(framebuffer, camera, source);

    for (int y = 0; y < framebuffer.height(); ++y) {
        for (int x = 0; x < framebuffer.width(); ++x) {
            REQUIRE((framebuffer.at(x, y) >> 24) == 255u);
        }
    }
}
