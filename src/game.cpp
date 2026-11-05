#include "game.h"

#include <algorithm>

#include "constants.h"

namespace wl {

namespace {
// Puts the camera on the middle of a tile rather than its corner.
constexpr double tileCentreOffset = 0.5;
}  // namespace

const Chunk* Game::TemporaryChunkStore::findChunk(ChunkCoord coord) const {
    const auto found = chunks.find(coord);
    return found == chunks.end() ? nullptr : found->second.get();
}

Game::Game(WorldSeed seed)
    : world_(seed),
      clock_(constants::simulationStepSeconds, constants::maxSimulationStepsPerFrame,
             constants::maxFrameDeltaSeconds) {
    int spawnTileX = 0;
    int spawnTileY = 0;
    world_.findSpawnTile(spawnTileX, spawnTileY);
    camera_.centerTileX = spawnTileX + tileCentreOffset;
    camera_.centerTileY = spawnTileY + tileCentreOffset;

    resizeDevice(constants::defaultDeviceWidthInPixels, constants::defaultDeviceHeightInPixels);
}

void Game::resizeDevice(int deviceWidthInPixels, int deviceHeightInPixels) {
    layout_ = computeFramebufferLayout(deviceWidthInPixels, deviceHeightInPixels);
    framebuffer_.resize(layout_.widthInPixels, layout_.heightInPixels);
    generateVisibleChunks();
}

void Game::setTileSizeInPixels(int tileSizeInPixels) {
    camera_.tileSizeInPixels =
        std::clamp(tileSizeInPixels, constants::minTileSizeInPixels, constants::maxTileSizeInPixels);
    generateVisibleChunks();
}

void Game::generateVisibleChunks() {
    const TileBounds bounds = visibleTileBounds(camera_, framebuffer_.width(), framebuffer_.height());
    const ChunkCoord minChunk = chunkCoordForTile(bounds.minTileX, bounds.minTileY);
    const ChunkCoord maxChunk = chunkCoordForTile(bounds.maxTileX, bounds.maxTileY);

    for (int chunkY = minChunk.y; chunkY <= maxChunk.y; ++chunkY) {
        for (int chunkX = minChunk.x; chunkX <= maxChunk.x; ++chunkX) {
            const ChunkCoord coord{chunkX, chunkY};
            if (!chunkStore_.findChunk(coord)) chunkStore_.chunks[coord] = world_.generateChunk(coord);
        }
    }
}

void Game::advanceFrame(double realDeltaSeconds) {
    const int stepsToRun = clock_.consumeElapsedTime(realDeltaSeconds);
    for (int stepIndex = 0; stepIndex < stepsToRun; ++stepIndex) {
        simulateStep(clock_.stepSeconds());
    }
    render();
}

void Game::simulateStep(double stepSeconds) { simulatedSeconds_ += stepSeconds; }

void Game::render() { renderer_.drawTerrain(framebuffer_, camera_, chunkStore_); }

}  // namespace wl
