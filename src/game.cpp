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
             constants::maxFrameDeltaSeconds),
      dayNight_(constants::startingTimeOfDay) {
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

ChunkCoord Game::playerChunk() const {
    const Vec2& position = player_.position();
    return chunkCoordForTile(static_cast<int>(std::floor(position.x)), static_cast<int>(std::floor(position.y)));
}

ChunkRegion Game::requiredChunkRegion() const {
    const double halfWidthInTiles = framebuffer_.width() / (2.0 * tileSizeInPixels_);
    const double halfHeightInTiles = framebuffer_.height() / (2.0 * tileSizeInPixels_);
    // Wildlife lives a little past the screen too, so make sure its chunks are loaded.
    const int radiusX = std::max(chunkRadiusCovering(halfWidthInTiles), constants::wildlifeSimulationRadiusInChunks);
    const int radiusY = std::max(chunkRadiusCovering(halfHeightInTiles), constants::wildlifeSimulationRadiusInChunks);
    return ChunkRegion::around(playerChunk(), radiusX, radiusY);
}

void Game::streamChunks(double budgetMs) {
    const ChunkRegion region = requiredChunkRegion();
    const std::size_t neededCapacity =
        static_cast<std::size_t>(region.chunkCount()) * cacheCapacityPerRequiredChunk;
    chunkCache_.setCapacity(
        std::max(static_cast<std::size_t>(constants::chunkCacheCapacityInChunks), neededCapacity));
    lastStreamingStats_ = chunkCache_.update(region, playerChunk(), budgetMs);
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
    dayNight_.advance(stepSeconds);
    player_.update(computeMoveIntent(input_), chunkCache_, stepSeconds);

    const ChunkRegion wildlifeRegion = ChunkRegion::around(
        playerChunk(), constants::wildlifeSimulationRadiusInChunks, constants::wildlifeSimulationRadiusInChunks);
    wildlife_.update(wildlifeRegion, world_.seed(), chunkCache_, player_.position(), stepSeconds);

    previousCameraPosition_ = cameraPosition_;
    const double follow = 1.0 - std::exp(-constants::cameraFollowSharpness * stepSeconds);
    cameraPosition_ = lerp(cameraPosition_, player_.position(), follow);
}

void Game::render(double interpolationAlpha) {
    SceneView scene;
    const Vec2 cameraView = lerp(previousCameraPosition_, cameraPosition_, interpolationAlpha);
    scene.camera.centerTileX = cameraView.x;
    scene.camera.centerTileY = cameraView.y;
    scene.camera.tileSizeInPixels = tileSizeInPixels_;
    scene.chunks = &chunkCache_;

    const Vec2 playerView = player_.interpolatedPosition(interpolationAlpha);
    scene.player.tileX = playerView.x;
    scene.player.tileY = playerView.y;
    scene.player.facingLeft = player_.isFacingLeft();
    scene.player.wading = player_.isWading();
    scene.player.walkFrame = walkFrameFor(player_);

    scene.creatures = &wildlife_.creatures();
    scene.interpolationAlpha = interpolationAlpha;
    scene.lighting = dayNight_.lighting();
    scene.timeSeconds = simulatedSeconds_;

    renderer_.drawFrame(framebuffer_, scene);
}

}  // namespace wl
