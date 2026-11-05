// The whole game, with no browser code in sight.
// main.cpp drives it on the web; tests drive it natively.
#pragma once

#include <string>
#include <vector>

#include "chunk_cache.h"
#include "day_night.h"
#include "effects.h"
#include "entities.h"
#include "exploration.h"
#include "fixed_timestep.h"
#include "framebuffer.h"
#include "input.h"
#include "journal.h"
#include "performance.h"
#include "planet.h"
#include "renderer.h"
#include "save_state.h"
#include "scanner.h"
#include "weather.h"
#include "world.h"

namespace wl {

// Things the page should react to. The game decides when; the page decides how they look.
enum class GameEventType {
    PlanetLanded,    // text is the planet name; sent when a world starts
    BiomeEntered,    // text is the biome name
    Discovery,       // text describes a new landmark
    JournalChanged,  // journal panel should refresh
};

struct GameEvent {
    GameEventType type;
    std::string text;
};

class Game {
public:
    explicit Game(WorldSeed seed = constants::defaultWorldSeed);

    // Canvas size in physical device pixels, and optionally in CSS pixels
    // (needed to turn a touch position into a point in the world).
    void resizeDevice(int deviceWidthInPixels, int deviceHeightInPixels, double cssWidth = 0.0,
                      double cssHeight = 0.0);

    void setTileSizeInPixels(int tileSizeInPixels);
    void setTimeOfDay(double timeOfDay) { dayNight_.setTimeOfDay(timeOfDay); }

    void setDirectionHeld(Direction direction, bool held);
    void setDrag(bool active, double offsetXInCssPixels, double offsetYInCssPixels);
    // Hold to walk: pointer position in CSS pixels from the canvas top-left.
    void setFollowPointer(bool active, double cssX, double cssY);
    void releaseAllInput();
    // Hold to scan nearby animals. Also makes the player creep so animals stay calm.
    void setActionHeld(bool held) { input_.actionHeld = held; }

    // Run however many sim steps are due, stream chunks, then draw one frame.
    void advanceFrame(double realDeltaSeconds);

    // --- Saving ---
    SaveState makeSaveState() const;
    // Restores progress. Refuses (returns false) a save from a different seed.
    bool applySaveState(const SaveState& state);
    void requestSave() { saveRequested_ = true; }
    // True once after each save request (autosave, discovery, or manual).
    bool takeSaveRequest();

    // Events since the last call, oldest first.
    std::vector<GameEvent> takeEvents();

    WorldSeed seed() const { return world_.seed(); }
    const Framebuffer& framebuffer() const { return framebuffer_; }
    const FramebufferLayout& layout() const { return layout_; }
    const World& world() const { return world_; }
    const PlanetProfile& planet() const { return planet_; }
    const Player& player() const { return player_; }
    const Wildlife& wildlife() const { return wildlife_; }
    const DayNightCycle& dayNight() const { return dayNight_; }
    const WeatherSystem& weather() const { return weather_; }
    const TrailEffects& trails() const { return trails_; }
    const Exploration& exploration() const { return exploration_; }
    const Journal& journal() const { return journal_; }
    const Scanner& scanner() const { return scanner_; }
    int surveyPercent() const { return journal_.surveyPercent(); }
    const ChunkCache& chunkCache() const { return chunkCache_; }
    const StreamingStats& lastStreamingStats() const { return lastStreamingStats_; }
    const FrameTimings& frameTimings() const { return profiler_.smoothed(); }
    int qualityLevel() const { return governor_.level(); }
    double simulatedSeconds() const { return simulatedSeconds_; }

    // One line of timings and counts for the ?debug=1 overlay.
    std::string debugText() const;

    // Region that must stay loaded right now: the screen plus a margin.
    ChunkRegion requiredChunkRegion() const;

private:
    ChunkCoord playerChunk() const;
    void applyLayout();
    void updateFollowOffset();
    void streamChunks(double budgetMs);
    void simulateStep(double stepSeconds);
    void updateBiomeAnnouncement(Biome groundBiome, double stepSeconds);
    void checkLandmarkDiscoveries();
    void updateScanner(double stepSeconds);
    void rebuildMinimapMarkers();
    void render(double interpolationAlpha);

    // Declared first: the cache holds a reference to it.
    World world_;
    ChunkCache chunkCache_;
    PlanetProfile planet_;

    FixedTimestepClock clock_;
    FramebufferLayout layout_;
    Framebuffer framebuffer_;
    Renderer renderer_;
    FrameProfiler profiler_;
    QualityGovernor governor_;
    int deviceWidth_ = constants::defaultDeviceWidthInPixels;
    int deviceHeight_ = constants::defaultDeviceHeightInPixels;
    double cssPixelsPerDevicePixel_ = 1.0;
    bool layoutChangePending_ = false;

    InputState input_;
    Player player_;
    Wildlife wildlife_;
    DayNightCycle dayNight_;
    WeatherSystem weather_;
    TrailEffects trails_;
    Exploration exploration_;
    Journal journal_;
    Scanner scanner_;
    MinimapView minimapView_;

    // Camera position is simulated (it eases after the player) and
    // interpolated like the player for smooth rendering.
    Vec2 cameraPosition_;
    Vec2 previousCameraPosition_;
    int tileSizeInPixels_ = constants::defaultTileSizeInPixels;

    // Biome banner: a biome must hold steady for a moment before it's announced.
    Biome candidateBiome_ = Biome::DeepOcean;
    double candidateSeconds_ = 0.0;
    Biome announcedBiome_ = Biome::DeepOcean;
    bool hasAnnouncedBiome_ = false;

    ChunkCoord lastRevealCentre_{};
    bool hasRevealed_ = false;
    double secondsUntilDiscoveryCheck_ = 0.0;
    double secondsSinceAutosave_ = 0.0;
    bool saveRequested_ = false;
    std::vector<GameEvent> events_;

    StreamingStats lastStreamingStats_;
    double simulatedSeconds_ = 0.0;
    bool hasRenderedFrame_ = false;
};

}  // namespace wl
