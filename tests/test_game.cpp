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
