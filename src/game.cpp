#include "game.h"

#include <algorithm>
#include <cmath>
#include <limits>

#include "constants.h"

namespace wl {

namespace {
// Puts the player on the middle of a tile rather than its corner.
constexpr double tileCentreOffset = 0.5;

// Used for the first load, so the opening frame isn't half empty.
constexpr double unlimitedBudgetMs = std::numeric_limits<double>::infinity();

// The cache must comfortably hold the required region or it would thrash.
constexpr int cacheCapacityPerRequiredChunk = 2;

int chunkRadiusCovering(double halfExtentInTiles) {
    return static_cast<int>(std::ceil(halfExtentInTiles / constants::chunkSizeInTiles)) +
           constants::chunkPreloadMarginInChunks;
}

int walkFrameFor(const Player& player) {
    if (!player.isMoving()) return 0;
    const int strideCount = static_cast<int>(player.distanceWalked() / constants::strideLengthInTiles);
    return 1 + strideCount % 2;
}
}  // namespace

Game::Game(WorldSeed seed)
    : world_(seed),
      chunkCache_(world_, constants::chunkCacheCapacityInChunks),
      clock_(constants::simulationStepSeconds, constants::maxSimulationStepsPerFrame,
             constants::maxFrameDeltaSeconds) {
    int spawnTileX = 0;
    int spawnTileY = 0;
    world_.findSpawnTile(spawnTileX, spawnTileY);
    player_.placeAt(spawnTileX + tileCentreOffset, spawnTileY + tileCentreOffset);
    cameraPosition_ = player_.position();
    previousCameraPosition_ = cameraPosition_;

    resizeDevice(constants::defaultDeviceWidthInPixels, constants::defaultDeviceHeightInPixels);
}

void Game::resizeDevice(int deviceWidthInPixels, int deviceHeightInPixels) {
    layout_ = computeFramebufferLayout(deviceWidthInPixels, deviceHeightInPixels);
    framebuffer_.resize(layout_.widthInPixels, layout_.heightInPixels);
    // Before the first frame, load everything so the opening view is complete.
    // Later resizes (rotating a phone) stream in normally within the budget.
    if (!hasRenderedFrame_) streamChunks(unlimitedBudgetMs);
}

void Game::setTileSizeInPixels(int tileSizeInPixels) {
    tileSizeInPixels_ =
        std::clamp(tileSizeInPixels, constants::minTileSizeInPixels, constants::maxTileSizeInPixels);
    if (!hasRenderedFrame_) streamChunks(unlimitedBudgetMs);
}

void Game::setDirectionHeld(Direction direction, bool held) {
    input_.directionHeld[static_cast<int>(direction)] = held;
}

void Game::setDrag(bool active, double offsetXInCssPixels, double offsetYInCssPixels) {
    input_.dragActive = active;
    input_.dragOffsetX = active ? offsetXInCssPixels : 0.0;
    input_.dragOffsetY = active ? offsetYInCssPixels : 0.0;
}

void Game::releaseAllInput() { input_ = InputState{}; }

ChunkRegion Game::requiredChunkRegion() const {
    const double halfWidthInTiles = framebuffer_.width() / (2.0 * tileSizeInPixels_);
    const double halfHeightInTiles = framebuffer_.height() / (2.0 * tileSizeInPixels_);
    const Vec2& position = player_.position();
    const ChunkCoord centre =
        chunkCoordForTile(static_cast<int>(std::floor(position.x)), static_cast<int>(std::floor(position.y)));
    return ChunkRegion::around(centre, chunkRadiusCovering(halfWidthInTiles),
                               chunkRadiusCovering(halfHeightInTiles));
}

void Game::streamChunks(double budgetMs) {
    const ChunkRegion region = requiredChunkRegion();
    const std::size_t neededCapacity =
        static_cast<std::size_t>(region.chunkCount()) * cacheCapacityPerRequiredChunk;
    chunkCache_.setCapacity(
        std::max(static_cast<std::size_t>(constants::chunkCacheCapacityInChunks), neededCapacity));

    const Vec2& position = player_.position();
    const ChunkCoord centre =
        chunkCoordForTile(static_cast<int>(std::floor(position.x)), static_cast<int>(std::floor(position.y)));
    lastStreamingStats_ = chunkCache_.update(region, centre, budgetMs);
}

void Game::advanceFrame(double realDeltaSeconds) {
    const int stepsToRun = clock_.consumeElapsedTime(realDeltaSeconds);
    for (int stepIndex = 0; stepIndex < stepsToRun; ++stepIndex) {
        simulateStep(clock_.stepSeconds());
    }
    streamChunks(constants::chunkGenerationBudgetMs);
    render(clock_.interpolationAlpha());
    hasRenderedFrame_ = true;
}

void Game::simulateStep(double stepSeconds) {
    simulatedSeconds_ += stepSeconds;
    player_.update(computeMoveIntent(input_), chunkCache_, stepSeconds);

    previousCameraPosition_ = cameraPosition_;
    const double follow = 1.0 - std::exp(-constants::cameraFollowSharpness * stepSeconds);
    cameraPosition_ = lerp(cameraPosition_, player_.position(), follow);
}

void Game::render(double interpolationAlpha) {
    const Vec2 cameraView = lerp(previousCameraPosition_, cameraPosition_, interpolationAlpha);
    Camera camera;
    camera.centerTileX = cameraView.x;
    camera.centerTileY = cameraView.y;
    camera.tileSizeInPixels = tileSizeInPixels_;

    renderer_.drawTerrain(framebuffer_, camera, chunkCache_);

    const Vec2 playerView = player_.interpolatedPosition(interpolationAlpha);
    PlayerAppearance appearance;
    appearance.tileX = playerView.x;
    appearance.tileY = playerView.y;
    appearance.facingLeft = player_.isFacingLeft();
    appearance.wading = player_.isWading();
    appearance.walkFrame = walkFrameFor(player_);
    renderer_.drawPlayer(framebuffer_, camera, appearance);
}

}  // namespace wl
