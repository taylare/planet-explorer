#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "constants.h"
#include "framebuffer.h"
#include "performance.h"

using namespace wl;

TEST_CASE("profiler starts from the first sample, then smooths", "[performance]") {
    FrameProfiler profiler;
    profiler.record(1.0, 2.0, 3.0);
    REQUIRE(profiler.smoothed().totalMs() == Catch::Approx(6.0));

    // One huge spike barely moves the average.
    profiler.record(1.0, 2.0, 300.0);
    REQUIRE(profiler.smoothed().renderMs < 20.0);
}

TEST_CASE("quality steps down only after sustained slow frames", "[performance]") {
    QualityGovernor governor;
    const double slow = constants::qualityDowngradeMs + 5.0;

    // A short burst is ignored.
    for (int frame = 0; frame < constants::qualityDowngradeFrames - 1; ++frame) REQUIRE_FALSE(governor.update(slow));
    REQUIRE_FALSE(governor.update(3.0));
    REQUIRE(governor.level() == 0);

    // A long run steps down one level.
    bool changed = false;
    for (int frame = 0; frame < constants::qualityDowngradeFrames; ++frame) changed = governor.update(slow) || changed;
    REQUIRE(changed);
    REQUIRE(governor.level() == 1);
}

TEST_CASE("quality never goes past the lowest level", "[performance]") {
    QualityGovernor governor;
    for (int frame = 0; frame < constants::qualityDowngradeFrames * 10; ++frame) governor.update(100.0);
    REQUIRE(governor.level() == constants::qualityLevelCount - 1);
}

TEST_CASE("quality climbs back after a long run of easy frames", "[performance]") {
    QualityGovernor governor;
    for (int frame = 0; frame < constants::qualityDowngradeFrames; ++frame) governor.update(100.0);
    REQUIRE(governor.level() == 1);

    // Middling frames (between the thresholds) hold the level steady.
    for (int frame = 0; frame < constants::qualityUpgradeFrames * 2; ++frame) governor.update(8.0);
    REQUIRE(governor.level() == 1);

    for (int frame = 0; frame < constants::qualityUpgradeFrames; ++frame) governor.update(1.0);
    REQUIRE(governor.level() == 0);
}

TEST_CASE("lower quality levels mean fewer pixels", "[performance]") {
    int previousPixels = 1 << 30;
    for (int level = 0; level < constants::qualityLevelCount; ++level) {
        const FramebufferLayout layout = computeFramebufferLayout(2560, 1440, constants::qualityLongEdges[level]);
        const int pixels = layout.widthInPixels * layout.heightInPixels;
        REQUIRE(pixels < previousPixels);
        REQUIRE(layout.widthInPixels * layout.pixelScale >= 2560);
        previousPixels = pixels;
    }
    REQUIRE(constants::qualityLongEdges[0] == constants::targetFramebufferLongEdgeInPixels);
}
