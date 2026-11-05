#include <catch2/catch_test_macros.hpp>
#include <cstdint>
#include <cstring>

#include "constants.h"
#include "framebuffer.h"

using namespace wl;

TEST_CASE("packRgba lays bytes out as R, G, B, A in memory", "[framebuffer]") {
    const PackedColor color = packRgba(10, 20, 30, 40);
    std::uint8_t bytes[4];
    std::memcpy(bytes, &color, sizeof(bytes));
    REQUIRE(bytes[0] == 10);
    REQUIRE(bytes[1] == 20);
    REQUIRE(bytes[2] == 30);
    REQUIRE(bytes[3] == 40);
}

TEST_CASE("layout for a 720p laptop canvas", "[framebuffer]") {
    const FramebufferLayout layout = computeFramebufferLayout(1280, 720);
    REQUIRE(layout.pixelScale == 2);
    REQUIRE(layout.widthInPixels == 640);
    REQUIRE(layout.heightInPixels == 360);
}

TEST_CASE("layout for a high-DPI phone in portrait", "[framebuffer]") {
    const FramebufferLayout layout = computeFramebufferLayout(1170, 2532);
    REQUIRE(layout.pixelScale == 4);
    REQUIRE(layout.widthInPixels == 293);
    REQUIRE(layout.heightInPixels == 633);
}

TEST_CASE("upscaled framebuffer always covers the canvas", "[framebuffer]") {
    const int sizes[][2] = {{1, 1}, {333, 777}, {1920, 1080}, {2560, 1600}, {3840, 2160}, {1001, 641}};
    for (const auto& size : sizes) {
        const FramebufferLayout layout = computeFramebufferLayout(size[0], size[1]);
        REQUIRE(layout.widthInPixels * layout.pixelScale >= size[0]);
        REQUIRE(layout.heightInPixels * layout.pixelScale >= size[1]);
        REQUIRE(layout.widthInPixels <= constants::targetFramebufferLongEdgeInPixels);
        REQUIRE(layout.heightInPixels <= constants::targetFramebufferLongEdgeInPixels);
    }
}

TEST_CASE("zero-size canvas still gives a usable framebuffer", "[framebuffer]") {
    const FramebufferLayout layout = computeFramebufferLayout(0, 0);
    REQUIRE(layout.widthInPixels >= constants::minFramebufferEdgeInPixels);
    REQUIRE(layout.heightInPixels >= constants::minFramebufferEdgeInPixels);
}

TEST_CASE("setPixelClipped ignores out of bounds writes", "[framebuffer]") {
    Framebuffer framebuffer;
    framebuffer.resize(4, 4);
    framebuffer.clear(packRgba(0, 0, 0));
    framebuffer.setPixelClipped(-1, 2, packRgba(255, 0, 0));
    framebuffer.setPixelClipped(4, 0, packRgba(255, 0, 0));
    framebuffer.setPixelClipped(1, 1, packRgba(0, 255, 0));
    REQUIRE(framebuffer.at(1, 1) == packRgba(0, 255, 0));
    REQUIRE(framebuffer.at(0, 2) == packRgba(0, 0, 0));
}
