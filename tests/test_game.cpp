#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "constants.h"
#include "game.h"

using namespace wl;

TEST_CASE("game advances simulated time in fixed steps", "[game]") {
    Game game;
    for (int frame = 0; frame < 60; ++frame) game.advanceFrame(constants::simulationStepSeconds);
    REQUIRE(game.simulatedSeconds() == Catch::Approx(1.0));
}

TEST_CASE("resizing the device resizes the framebuffer", "[game]") {
    Game game;
    game.resizeDevice(1170, 2532);
    REQUIRE(game.framebuffer().width() == game.layout().widthInPixels);
    REQUIRE(game.framebuffer().height() == game.layout().heightInPixels);
}

TEST_CASE("every rendered pixel is fully opaque", "[game]") {
    Game game;
    game.resizeDevice(320, 200);
    game.advanceFrame(constants::simulationStepSeconds);

    const Framebuffer& framebuffer = game.framebuffer();
    const int pixelCount = framebuffer.width() * framebuffer.height();
    for (int index = 0; index < pixelCount; ++index) {
        REQUIRE((framebuffer.pixels()[index] >> 24) == 255u);
    }
}
