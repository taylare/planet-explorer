#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <cmath>

#include "constants.h"
#include "game.h"

using namespace wl;

namespace {
void runFrames(Game& game, int frameCount) {
    for (int frame = 0; frame < frameCount; ++frame) game.advanceFrame(constants::simulationStepSeconds);
}

void requireRegionLoaded(const Game& game) {
    const ChunkRegion region = game.requiredChunkRegion();
    for (int y = region.minChunkY; y <= region.maxChunkY; ++y) {
        for (int x = region.minChunkX; x <= region.maxChunkX; ++x) REQUIRE(game.chunkCache().contains({x, y}));
    }
}
}  // namespace

TEST_CASE("game advances simulated time in fixed steps", "[game]") {
    Game game;
    runFrames(game, 60);
    REQUIRE(game.simulatedSeconds() == Catch::Approx(1.0));
}

TEST_CASE("resizing the device resizes the framebuffer", "[game]") {
    Game game;
    game.resizeDevice(1170, 2532);
    REQUIRE(game.framebuffer().width() == game.layout().widthInPixels);
    REQUIRE(game.framebuffer().height() == game.layout().heightInPixels);
}

TEST_CASE("the view is fully loaded before the first frame", "[game]") {
    Game game;
    game.resizeDevice(1920, 1080);
    requireRegionLoaded(game);
}

TEST_CASE("walking around keeps the player on walkable ground", "[game]") {
    Game game;
    game.setDirectionHeld(Direction::Right, true);
    game.setDirectionHeld(Direction::Down, true);
    runFrames(game, 240);
    game.releaseAllInput();
    runFrames(game, 60);  // let streaming catch up

    const Vec2& position = game.player().position();
    Biome biome = Biome::DeepOcean;
    REQUIRE(tryGetBiome(game.chunkCache(), static_cast<int>(std::floor(position.x)),
                        static_cast<int>(std::floor(position.y)), biome));
    REQUIRE(biome != Biome::DeepOcean);
    requireRegionLoaded(game);
}

TEST_CASE("standing in a biome announces it once and records it in the journal", "[game][journal]") {
    Game game;
    runFrames(game, 30);  // half a second: too soon to announce
    for (const GameEvent& event : game.takeEvents()) REQUIRE(event.type != GameEventType::BiomeEntered);

    runFrames(game, 90);
    const std::vector<GameEvent> events = game.takeEvents();
    int announcements = 0;
    bool journalChanged = false;
    for (const GameEvent& event : events) {
        if (event.type == GameEventType::BiomeEntered) ++announcements;
        if (event.type == GameEventType::JournalChanged) journalChanged = true;
    }
    REQUIRE(announcements == 1);
    REQUIRE(journalChanged);
    // A landmark might also be in sight of spawn, so count biome entries only.
    int biomeEntries = 0;
    for (const JournalEntry& entry : game.journal().entries()) {
        if (entry.category == JournalCategory::Biome) ++biomeEntries;
    }
    REQUIRE(biomeEntries == 1);
    REQUIRE(game.takeSaveRequest());        // a new journal entry asks for a save
    REQUIRE_FALSE(game.takeSaveRequest());  // and only once

    // Staying put doesn't repeat the banner.
    runFrames(game, 120);
    for (const GameEvent& event : game.takeEvents()) REQUIRE(event.type != GameEventType::BiomeEntered);
}

TEST_CASE("the area around the player is revealed on the minimap", "[game][exploration]") {
    Game game;
    runFrames(game, 2);
    const ChunkCoord centre = chunkCoordForTile(static_cast<int>(std::floor(game.player().position().x)),
                                                static_cast<int>(std::floor(game.player().position().y)));
    for (int y = -1; y <= 1; ++y) {
        for (int x = -1; x <= 1; ++x) REQUIRE(game.exploration().isRevealed({centre.x + x, centre.y + y}));
    }
}

TEST_CASE("autosave asks for a save every so often", "[game][save]") {
    Game game;
    runFrames(game, 150);
    game.takeSaveRequest();  // clear any discovery saves from the start
    bool requested = false;
    const int framesPerAutosave = static_cast<int>(constants::autosaveIntervalSeconds / constants::simulationStepSeconds);
    for (int frame = 0; frame < framesPerAutosave + 5 && !requested; ++frame) {
        game.advanceFrame(constants::simulationStepSeconds);
        requested = game.takeSaveRequest();
    }
    REQUIRE(requested);
}

TEST_CASE("every rendered pixel is fully opaque", "[game]") {
    Game game;
    game.resizeDevice(320, 200);
    runFrames(game, 1);

    const Framebuffer& framebuffer = game.framebuffer();
    const int pixelCount = framebuffer.width() * framebuffer.height();
    for (int index = 0; index < pixelCount; ++index) {
        REQUIRE((framebuffer.pixels()[index] >> 24) == 255u);
    }
}

TEST_CASE("starting a world announces the planet", "[game][planet]") {
    Game game(99);
    const std::vector<GameEvent> events = game.takeEvents();
    REQUIRE_FALSE(events.empty());
    REQUIRE(events.front().type == GameEventType::PlanetLanded);
    REQUIRE(events.front().text == game.planet().name);
}

TEST_CASE("a longer planetary day slows the clock", "[game][planet]") {
    Game game(2468);
    const double scale = game.planet().dayLengthScale;
    const double startTime = game.dayNight().timeOfDay();
    constexpr int frames = 600;  // ten seconds
    runFrames(game, frames);

    const double expectedChange = (frames * constants::simulationStepSeconds) / (constants::dayLengthSeconds * scale);
    REQUIRE(game.dayNight().timeOfDay() - startTime == Catch::Approx(expectedChange).epsilon(0.01));
}
