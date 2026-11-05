#include "game.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
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

// Landmarks are searched for in the player's chunk and this many around it.
// One chunk (32 tiles) is far more than the discovery radius.
constexpr int discoverySearchRadiusInChunks = 1;

int chunkRadiusCovering(double halfExtentInTiles) {
    return static_cast<int>(std::ceil(halfExtentInTiles / constants::chunkSizeInTiles)) +
           constants::chunkPreloadMarginInChunks;
}

int walkFrameFor(const Player& player) {
    if (!player.isMoving()) return 0;
    const int strideCount = static_cast<int>(player.distanceWalked() / constants::strideLengthInTiles);
    return 1 + strideCount % 2;
}

int tileIndexFor(double position) { return static_cast<int>(std::floor(position)); }
}  // namespace

Game::Game(WorldSeed seed)
    : world_(seed),
      chunkCache_(world_, constants::chunkCacheCapacityInChunks),
      planet_(makePlanetProfile(seed)),
      clock_(constants::simulationStepSeconds, constants::maxSimulationStepsPerFrame,
             constants::maxFrameDeltaSeconds),
      dayNight_(constants::startingTimeOfDay),
      weather_(seed) {
    int spawnTileX = 0;
    int spawnTileY = 0;
    world_.findSpawnTile(spawnTileX, spawnTileY);
    player_.placeAt(spawnTileX + tileCentreOffset, spawnTileY + tileCentreOffset);
    cameraPosition_ = player_.position();
    previousCameraPosition_ = cameraPosition_;

    resizeDevice(constants::defaultDeviceWidthInPixels, constants::defaultDeviceHeightInPixels);
    events_.push_back({GameEventType::PlanetLanded, planet_.name});
}

void Game::resizeDevice(int deviceWidthInPixels, int deviceHeightInPixels, double cssWidth, double cssHeight) {
    deviceWidth_ = std::max(deviceWidthInPixels, 1);
    deviceHeight_ = std::max(deviceHeightInPixels, 1);
    // Without a CSS size (native tests), treat device and CSS pixels as equal.
    cssPixelsPerDevicePixel_ = cssWidth > 0.0 ? cssWidth / deviceWidth_ : 1.0;
    (void)cssHeight;  // the ratio is the same on both axes
    applyLayout();
    // Before the first frame, load everything so the opening view is complete.
    // Later resizes (rotating a phone) stream in normally within the budget.
    if (!hasRenderedFrame_) streamChunks(unlimitedBudgetMs);
}

void Game::applyLayout() {
    const int targetLongEdge = constants::qualityLongEdges[governor_.level()];
    layout_ = computeFramebufferLayout(deviceWidth_, deviceHeight_, targetLongEdge);
    framebuffer_.resize(layout_.widthInPixels, layout_.heightInPixels);
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

void Game::setFollowPointer(bool active, double cssX, double cssY) {
    input_.followActive = active;
    input_.followPointerX = cssX;
    input_.followPointerY = cssY;
    updateFollowOffset();
}

void Game::releaseAllInput() { input_ = InputState{}; }

void Game::updateFollowOffset() {
    if (!input_.followActive) return;
    // Where the player is on screen, in CSS pixels, using the same camera
    // maths as the renderer. Aim from the body rather than the feet.
    Camera camera;
    camera.centerTileX = cameraPosition_.x;
    camera.centerTileY = cameraPosition_.y;
    camera.tileSizeInPixels = tileSizeInPixels_;
    const ScreenOrigin origin = screenOriginFor(camera, framebuffer_.width(), framebuffer_.height());
    const int spriteScale = std::max(1, tileSizeInPixels_ / constants::spriteBaseTileSizeInPixels);
    const int bodyRaise = (constants::spriteBaseTileSizeInPixels / 2) * spriteScale;

    const double cssPerFramebufferPixel = layout_.pixelScale * cssPixelsPerDevicePixel_;
    const double playerCssX = worldToScreenX(origin, player_.position().x) * cssPerFramebufferPixel;
    const double playerCssY = (worldToScreenY(origin, player_.position().y) - bodyRaise) * cssPerFramebufferPixel;
    input_.followOffsetX = input_.followPointerX - playerCssX;
    input_.followOffsetY = input_.followPointerY - playerCssY;
}

SaveState Game::makeSaveState() const {
    SaveState state;
    state.seed = world_.seed();
    state.playerTileX = player_.position().x;
    state.playerTileY = player_.position().y;
    state.timeOfDay = dayNight_.timeOfDay();
    state.day = dayNight_.day();
    state.revealedChunks = exploration_.revealedChunksSorted();
    state.journal = journal_.entries();
    state.samples = samples_;
    state.usedLandmarks = usedLandmarks_;
    return state;
}

bool Game::applySaveState(const SaveState& state) {
    if (state.seed != world_.seed()) return false;

    player_.placeAt(state.playerTileX, state.playerTileY);
    cameraPosition_ = player_.position();
    previousCameraPosition_ = cameraPosition_;
    dayNight_.setTimeOfDay(state.timeOfDay);
    dayNight_.setDay(state.day);
    exploration_.restore(state.revealedChunks);
    journal_.restore(state.journal);
    samples_ = state.samples;
    usedLandmarks_ = state.usedLandmarks;
    scanner_.reset();

    // Fresh start for things tied to the old position.
    wildlife_.clear();
    trails_.clear();
    hasAnnouncedBiome_ = false;
    candidateSeconds_ = 0.0;
    hasRevealed_ = false;

    if (!hasRenderedFrame_) streamChunks(unlimitedBudgetMs);
    return true;
}

bool Game::takeSaveRequest() {
    const bool requested = saveRequested_;
    saveRequested_ = false;
    return requested;
}

std::vector<GameEvent> Game::takeEvents() {
    std::vector<GameEvent> taken;
    taken.swap(events_);
    return taken;
}

ChunkCoord Game::playerChunk() const {
    const Vec2& position = player_.position();
    return chunkCoordForTile(tileIndexFor(position.x), tileIndexFor(position.y));
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
    // Apply a quality change from last frame now, before drawing. Doing it
    // straight after rendering would hand the page a freshly cleared, black buffer.
    if (layoutChangePending_) {
        applyLayout();
        layoutChangePending_ = false;
    }
    const double frameStartMs = nowMilliseconds();
    const int stepsToRun = clock_.consumeElapsedTime(realDeltaSeconds);
    for (int stepIndex = 0; stepIndex < stepsToRun; ++stepIndex) {
        simulateStep(clock_.stepSeconds());
    }
    const double simulationDoneMs = nowMilliseconds();

    streamChunks(constants::chunkGenerationBudgetMs);
    exploration_.buildSummaries(world_, chunkCache_, constants::minimapSummariesPerFrame);
    rebuildMinimapMarkers();
    const double streamingDoneMs = nowMilliseconds();

    render(clock_.interpolationAlpha());
    const double renderDoneMs = nowMilliseconds();
    hasRenderedFrame_ = true;

    profiler_.record(simulationDoneMs - frameStartMs, streamingDoneMs - simulationDoneMs,
                     renderDoneMs - streamingDoneMs);

    // Chunk streaming is already capped by its own budget, so judge quality
    // on simulation and rendering, which is what the pixel count affects.
    const FrameTimings& timings = profiler_.smoothed();
    if (governor_.update(timings.simulationMs + timings.renderMs)) layoutChangePending_ = true;
}

std::string Game::debugText() const {
    const FrameTimings& timings = profiler_.smoothed();
    const Vec2& position = player_.position();
    char text[256];
    std::snprintf(text, sizeof(text),
                  "work %.1f ms (sim %.1f, stream %.1f, draw %.1f) | %dx%d q%d | chunks %zu (+%d) | at %d, %d",
                  timings.totalMs(), timings.simulationMs, timings.streamingMs, timings.renderMs,
                  framebuffer_.width(), framebuffer_.height(), governor_.level(), chunkCache_.size(),
                  lastStreamingStats_.chunksPending, tileIndexFor(position.x), tileIndexFor(position.y));
    return text;
}

void Game::simulateStep(double stepSeconds) {
    simulatedSeconds_ += stepSeconds;
    // A longer planetary day means the clock runs slower.
    dayNight_.advance(stepSeconds / planet_.dayLengthScale);
    updateFollowOffset();  // the player moved, so the held point's direction changed
    const bool sneaking = input_.actionHeld;
    const double speedMultiplier = sneaking ? constants::sneakSpeedMultiplier : 1.0;
    player_.update(computeMoveIntent(input_), chunkCache_, stepSeconds, speedMultiplier);

    const Vec2& position = player_.position();
    Biome groundBiome = Biome::Grassland;
    const bool groundKnown = tryGetBiome(chunkCache_, tileIndexFor(position.x), tileIndexFor(position.y), groundBiome);

    trails_.update(player_, groundBiome, simulatedSeconds_);
    weather_.update(world_, position, simulatedSeconds_, stepSeconds);

    const ChunkCoord centre = playerChunk();
    const ChunkRegion wildlifeRegion = ChunkRegion::around(centre, constants::wildlifeSimulationRadiusInChunks,
                                                           constants::wildlifeSimulationRadiusInChunks);
    wildlife_.update(wildlifeRegion, world_.seed(), chunkCache_, position, stepSeconds, sneaking);
    updateScanner(stepSeconds);

    // Only re-reveal when the player changes chunk; it's the same squares otherwise.
    if (!hasRevealed_ || centre != lastRevealCentre_) {
        exploration_.reveal(centre, constants::explorationRevealRadiusInChunks);
        lastRevealCentre_ = centre;
        hasRevealed_ = true;
    }

    if (groundKnown) updateBiomeAnnouncement(groundBiome, stepSeconds);

    secondsUntilDiscoveryCheck_ -= stepSeconds;
    if (secondsUntilDiscoveryCheck_ <= 0.0) {
        secondsUntilDiscoveryCheck_ = constants::discoveryCheckIntervalSeconds;
        checkLandmarkDiscoveries();
    }

    secondsSinceAutosave_ += stepSeconds;
    if (secondsSinceAutosave_ >= constants::autosaveIntervalSeconds) {
        secondsSinceAutosave_ = 0.0;
        requestSave();
    }

    previousCameraPosition_ = cameraPosition_;
    const double follow = 1.0 - std::exp(-constants::cameraFollowSharpness * stepSeconds);
    cameraPosition_ = lerp(cameraPosition_, position, follow);
}

void Game::updateBiomeAnnouncement(Biome groundBiome, double stepSeconds) {
    if (groundBiome != candidateBiome_) {
        candidateBiome_ = groundBiome;
        candidateSeconds_ = 0.0;
        return;
    }
    candidateSeconds_ += stepSeconds;
    if (candidateSeconds_ < constants::biomeAnnounceDelaySeconds) return;
    if (hasAnnouncedBiome_ && candidateBiome_ == announcedBiome_) return;

    announcedBiome_ = candidateBiome_;
    hasAnnouncedBiome_ = true;
    events_.push_back({GameEventType::BiomeEntered, std::string(biomeName(announcedBiome_))});

    const Vec2& position = player_.position();
    if (journal_.recordBiome(announcedBiome_, tileIndexFor(position.x), tileIndexFor(position.y), dayNight_.day())) {
        events_.push_back({GameEventType::JournalChanged, {}});
        requestSave();
    }
}

bool Game::isLandmarkUsed(int tileX, int tileY) const {
    return std::find(usedLandmarks_.begin(), usedLandmarks_.end(), UsedLandmark{tileX, tileY}) != usedLandmarks_.end();
}

// Scannable landmarks in the chunks around the player. Only built while the scan button
// is held, so it costs nothing the rest of the time.
std::vector<ScanLandmark> Game::scannableLandmarksNearPlayer() const {
    std::vector<ScanLandmark> landmarks;
    const ChunkCoord centre = playerChunk();
    for (int chunkY = centre.y - 1; chunkY <= centre.y + 1; ++chunkY) {
        for (int chunkX = centre.x - 1; chunkX <= centre.x + 1; ++chunkX) {
            const Chunk* chunk = chunkCache_.findChunk({chunkX, chunkY});
            if (!chunk) continue;
            for (const Prop& prop : chunk->props) {
                if (!isScannableLandmark(prop.kind)) continue;
                const bool used = isLandmarkUsed(tileIndexFor(prop.tileX), tileIndexFor(prop.tileY));
                landmarks.push_back({prop.tileX, prop.tileY, prop.kind, used});
            }
        }
    }
    return landmarks;
}

void Game::updateScanner(double stepSeconds) {
    const Vec2& position = player_.position();
    const std::vector<ScanLandmark> landmarks =
        input_.actionHeld ? scannableLandmarksNearPlayer() : std::vector<ScanLandmark>{};
    const bool completed = scanner_.update(input_.actionHeld, position, wildlife_.creatures(), landmarks, journal_,
                                           stepSeconds, constants::scanRangeInTiles, constants::scanDurationSeconds,
                                           constants::landmarkScanDurationSeconds);
    if (!completed) return;

    if (scanner_.completedKind() == ScanTargetKind::Landmark) {
        applyLandmarkScan(scanner_.completedLandmarkKind(), scanner_.completedLandmarkTileX(),
                          scanner_.completedLandmarkTileY());
    } else {
        const Species species = scanner_.completedSpecies();
        journal_.recordSpecies(species, tileIndexFor(position.x), tileIndexFor(position.y), dayNight_.day());
        events_.push_back({GameEventType::Discovery, std::string(speciesName(species))});
    }
    events_.push_back({GameEventType::JournalChanged, {}});
    requestSave();
}

// Applies what the scan result says and remembers the landmark as used.
void Game::applyLandmarkScan(PropKind kind, int tileX, int tileY) {
    const LandmarkResult result = scanLandmarkResult(kind, journal_.loreCount());
    switch (result.effect) {
        case LandmarkEffect::BeaconLit:
            exploration_.reveal(chunkCoordForTile(tileX, tileY), constants::beaconRevealRadiusInChunks);
            break;
        case LandmarkEffect::SamplesGained:
            samples_ += result.samples;
            break;
        case LandmarkEffect::LoreDecoded:
            journal_.recordLore(result.loreIndex, tileX, tileY, dayNight_.day());
            break;
        case LandmarkEffect::NothingNew:
        case LandmarkEffect::None:
            break;
    }
    usedLandmarks_.push_back({tileX, tileY});
    events_.push_back({GameEventType::Notice, landmarkNoticeText(kind, result)});
}

void Game::checkLandmarkDiscoveries() {
    const Vec2& position = player_.position();
    const ChunkCoord centre = playerChunk();
    const double radiusSquared = constants::landmarkDiscoveryRadiusInTiles * constants::landmarkDiscoveryRadiusInTiles;

    for (int chunkY = centre.y - discoverySearchRadiusInChunks; chunkY <= centre.y + discoverySearchRadiusInChunks; ++chunkY) {
        for (int chunkX = centre.x - discoverySearchRadiusInChunks; chunkX <= centre.x + discoverySearchRadiusInChunks; ++chunkX) {
            const Chunk* chunk = chunkCache_.findChunk({chunkX, chunkY});
            if (!chunk) continue;
            for (const Prop& prop : chunk->props) {
                if (!isJournalLandmark(prop.kind) || journal_.hasLandmark(prop.kind)) continue;
                const double deltaX = prop.tileX - position.x;
                const double deltaY = prop.tileY - position.y;
                if (deltaX * deltaX + deltaY * deltaY > radiusSquared) continue;

                journal_.recordLandmark(prop.kind, tileIndexFor(prop.tileX), tileIndexFor(prop.tileY), dayNight_.day());
                events_.push_back({GameEventType::Discovery, std::string(propKindName(prop.kind))});
                events_.push_back({GameEventType::JournalChanged, {}});
                requestSave();
            }
        }
    }
}

void Game::rebuildMinimapMarkers() {
    using namespace constants;
    minimapView_.exploration = &exploration_;
    minimapView_.markers.clear();

    // Landmark regions that could show up on the map around the player.
    const Vec2& position = player_.position();
    const int reachInTiles = (minimapSizeInPixels / 2 + 1) * minimapTilesPerPixel;
    const int regionSizeInTiles = landmarkRegionSizeInChunks * chunkSizeInTiles;
    const int minRegionX = floorDivide(tileIndexFor(position.x) - reachInTiles, regionSizeInTiles);
    const int maxRegionX = floorDivide(tileIndexFor(position.x) + reachInTiles, regionSizeInTiles);
    const int minRegionY = floorDivide(tileIndexFor(position.y) - reachInTiles, regionSizeInTiles);
    const int maxRegionY = floorDivide(tileIndexFor(position.y) + reachInTiles, regionSizeInTiles);

    for (int regionY = minRegionY; regionY <= maxRegionY; ++regionY) {
        for (int regionX = minRegionX; regionX <= maxRegionX; ++regionX) {
            const std::optional<Prop>& landmark = exploration_.regionLandmark(world_, regionX, regionY);
            if (!landmark) continue;
            // Only mark landmarks the player has been near: no spoilers through the fog.
            const ChunkCoord landmarkChunk =
                chunkCoordForTile(tileIndexFor(landmark->tileX), tileIndexFor(landmark->tileY));
            if (!exploration_.isRevealed(landmarkChunk)) continue;
            minimapView_.markers.push_back({landmark->tileX, landmark->tileY, landmark->kind});
        }
    }
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
    scene.trails = &trails_;
    scene.weather = &weather_.state();
    scene.minimap = &minimapView_;
    scene.planet = &planet_;
    scene.scan = &scanner_.status();
    scene.interpolationAlpha = interpolationAlpha;
    scene.timeSeconds = simulatedSeconds_;

    // Rain and snow dull the light a little on top of the time of day.
    scene.lighting = planetLighting(planet_, dayNight_.lighting());
    const float overcast = 1.0f - constants::overcastDarkening * static_cast<float>(weather_.state().intensity);
    scene.lighting.tintRed *= overcast;
    scene.lighting.tintGreen *= overcast;
    scene.lighting.tintBlue *= overcast;

    renderer_.drawFrame(framebuffer_, scene);
}

}  // namespace wl
