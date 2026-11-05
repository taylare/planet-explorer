#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include <memory>
#include <string>
#include <unordered_map>

#include "constants.h"
#include "game.h"
#include "renderer.h"
#include "sky_events.h"

using namespace wl;

namespace {
constexpr double step = constants::simulationStepSeconds;

// How many nights to look at when checking how often showers happen.
constexpr int sampledNights = 2000;

class MapChunkSource : public ChunkSource {
public:
    const Chunk* findChunk(ChunkCoord coord) const override {
        const auto found = chunks.find(coord);
        return found == chunks.end() ? nullptr : found->second.get();
    }
    std::unordered_map<ChunkCoord, std::unique_ptr<Chunk>, ChunkCoordHash> chunks;
};

void fillAroundOrigin(MapChunkSource& source, const World& world) {
    for (int chunkY = -2; chunkY <= 1; ++chunkY) {
        for (int chunkX = -2; chunkX <= 1; ++chunkX) {
            source.chunks[{chunkX, chunkY}] = world.generateChunk({chunkX, chunkY});
        }
    }
}

int countDifferences(const Framebuffer& a, const Framebuffer& b) {
    int differences = 0;
    for (int index = 0; index < a.width() * a.height(); ++index) {
        if (a.pixels()[index] != b.pixels()[index]) ++differences;
    }
    return differences;
}
}  // namespace

TEST_CASE("a night belongs to the day it began on, even after midnight", "[sky]") {
    REQUIRE(nightNumberFor(0.9, 5) == 5);   // evening of day 5
    REQUIRE(nightNumberFor(0.1, 6) == 5);   // small hours: the counter has moved on, the night has not
    REQUIRE(nightNumberFor(0.5, 5) == 5);   // noon boundary counts as evening
}

TEST_CASE("meteor showers are stable per night and happen on roughly a third of nights", "[sky][meteors]") {
    int showers = 0;
    for (int night = 0; night < sampledNights; ++night) {
        const bool first = isMeteorShowerNight(42, night);
        REQUIRE(first == isMeteorShowerNight(42, night));
        if (first) ++showers;
    }
    const double share = static_cast<double>(showers) / sampledNights;
    REQUIRE(share > constants::meteorShowerNightChance - 0.06);
    REQUIRE(share < constants::meteorShowerNightChance + 0.06);
}

TEST_CASE("different worlds have showers on different nights", "[sky][meteors]") {
    int differences = 0;
    for (int night = 0; night < 200; ++night) {
        if (isMeteorShowerNight(1, night) != isMeteorShowerNight(2, night)) ++differences;
    }
    REQUIRE(differences > 20);
}

TEST_CASE("every meteor is valid and finishes within its slot", "[sky][meteors]") {
    int presentCount = 0;
    for (std::int64_t slot = 0; slot < 500; ++slot) {
        for (int index = 0; index < constants::meteorsPerSlot; ++index) {
            const MeteorStreak streak = meteorStreakFor(7, 3, slot, index);
            REQUIRE(streak.present == meteorStreakFor(7, 3, slot, index).present);  // deterministic
            if (!streak.present) continue;
            ++presentCount;
            REQUIRE(streak.startX >= 0.0);
            REQUIRE(streak.startX <= 1.0);
            REQUIRE(streak.startY >= 0.0);
            REQUIRE(streak.startY <= 0.6);
            REQUIRE(streak.directionY > 0.0);  // always falling
            REQUIRE(std::hypot(streak.directionX, streak.directionY) == Catch::Approx(1.0));
            REQUIRE(streak.startOffsetSeconds >= 0.0);
            REQUIRE(streak.startOffsetSeconds + constants::meteorDurationSeconds <= constants::meteorSlotSeconds + 1e-9);
        }
    }
    // About 75 percent of 1000 possible meteors.
    REQUIRE(presentCount > 600);
    REQUIRE(presentCount < 900);
}

TEST_CASE("meteor slots line up with time", "[sky][meteors]") {
    REQUIRE(meteorSlotAt(0.0) == 0);
    REQUIRE(meteorSlotAt(constants::meteorSlotSeconds * 3.5) == 3);
    REQUIRE(meteorSlotStartSeconds(3) == Catch::Approx(constants::meteorSlotSeconds * 3.0));
    REQUIRE(meteorSlotAt(-0.1) == -1);
}

TEST_CASE("no lightning without heavy rain", "[sky][lightning]") {
    for (double time = 0.0; time < 600.0; time += 0.05) {
        REQUIRE(lightningFlashAt(9, time, 0.0) == 0.0f);
        REQUIRE(lightningFlashAt(9, time, constants::lightningMinimumRainIntensity - 0.01) == 0.0f);
    }
}

TEST_CASE("heavy rain brings flashes that are bright, brief and in range", "[sky][lightning]") {
    int flashingSamples = 0;
    int totalSamples = 0;
    float brightest = 0.0f;
    for (double time = 0.0; time < 600.0; time += 0.02) {
        const float flash = lightningFlashAt(9, time, 1.0);
        REQUIRE(flash >= 0.0f);
        REQUIRE(flash <= 1.0f);
        brightest = std::max(brightest, flash);
        if (flash > 0.05f) ++flashingSamples;
        ++totalSamples;
    }
    REQUIRE(brightest > 0.9f);
    // Flashes are quick, so most of the time the sky is dark.
    REQUIRE(flashingSamples > 0);
    REQUIRE(static_cast<double>(flashingSamples) / totalSamples < 0.1);
}

TEST_CASE("lightning is deterministic and heavier rain means more of it", "[sky][lightning]") {
    int light = 0;
    int heavy = 0;
    for (double time = 0.0; time < 3000.0; time += 0.05) {
        const float again = lightningFlashAt(9, time, 1.0);
        REQUIRE(again == lightningFlashAt(9, time, 1.0));
        if (lightningFlashAt(9, time, 0.7) > 0.5f) ++light;
        if (lightningFlashAt(9, time, 1.0) > 0.5f) ++heavy;
    }
    REQUIRE(heavy > light);
}

TEST_CASE("a flash is a double strike: a second pulse follows the first", "[sky][lightning]") {
    // Find the start of a strike, then check the echo.
    for (double time = 0.0; time < 600.0; time += 0.005) {
        const float before = lightningFlashAt(9, time - 0.005, 1.0);
        const float now = lightningFlashAt(9, time, 1.0);
        if (before < 0.2f && now > 0.9f) {
            // Just before the echo the first flash has faded; the echo then jumps back up.
            const double echoAt = time + constants::lightningSecondFlashDelaySeconds;
            REQUIRE(lightningFlashAt(9, echoAt - 0.03, 1.0) < lightningFlashAt(9, echoAt + 0.005, 1.0));
            // The echo starts at its full strength and has only just begun to fade.
            const float echo = lightningFlashAt(9, echoAt + 0.005, 1.0);
            REQUIRE(echo > constants::lightningSecondFlashStrength * 0.8);
            REQUIRE(echo <= constants::lightningSecondFlashStrength + 1e-6);
            return;
        }
    }
    FAIL("no lightning strike found to test");
}

TEST_CASE("meteors show only on a shower night and only in the dark", "[sky][renderer]") {
    const World world(4);
    MapChunkSource source;
    fillAroundOrigin(source, world);

    // A slot where the first meteor is present, and a moment mid flight.
    std::int64_t slot = 0;
    MeteorStreak streak;
    while (true) {
        streak = meteorStreakFor(4, 1, slot, 0);
        if (streak.present) break;
        ++slot;
    }
    const double midFlight =
        meteorSlotStartSeconds(slot) + streak.startOffsetSeconds + constants::meteorDurationSeconds * 0.5;

    const auto draw = [&](bool shower, double timeOfDay) {
        Renderer renderer;
        Framebuffer framebuffer;
        framebuffer.resize(320, 240);
        SceneView scene;
        scene.chunks = &source;
        scene.lighting = lightingAt(timeOfDay);
        scene.timeSeconds = midFlight;
        scene.meteorShower = shower;
        scene.skySeed = 4;
        scene.nightNumber = 1;
        renderer.drawFrame(framebuffer, scene);
        return framebuffer;
    };

    const Framebuffer showerNight = draw(true, 0.0);
    const Framebuffer plainNight = draw(false, 0.0);
    const Framebuffer showerDay = draw(true, 0.5);
    const Framebuffer plainDay = draw(false, 0.5);
    REQUIRE(countDifferences(showerNight, plainNight) > 8);  // a tail of lit pixels
    REQUIRE(countDifferences(showerDay, plainDay) == 0);     // too bright out to see them
}

TEST_CASE("a lightning flash brightens the whole frame", "[sky][renderer]") {
    const World world(4);
    MapChunkSource source;
    fillAroundOrigin(source, world);
    Renderer renderer;

    const auto averageLuma = [&](float flash) {
        Framebuffer framebuffer;
        framebuffer.resize(160, 120);
        SceneView scene;
        scene.chunks = &source;
        scene.lighting = lightingAt(0.0);
        scene.lightningFlash = flash;
        renderer.drawFrame(framebuffer, scene);
        double total = 0.0;
        for (int index = 0; index < framebuffer.width() * framebuffer.height(); ++index) {
            const PackedColor pixel = framebuffer.pixels()[index];
            total += (pixel & 0xff) + ((pixel >> 8) & 0xff) + ((pixel >> 16) & 0xff);
        }
        return total / (framebuffer.width() * framebuffer.height());
    };
    const double dark = averageLuma(0.0);
    const double flashed = averageLuma(1.0);
    REQUIRE(flashed > dark * 2.0);
}

TEST_CASE("a dust storm hazes the frame and fills it with grit", "[sky][renderer][dust]") {
    const World world(4);
    MapChunkSource source;
    fillAroundOrigin(source, world);

    const auto draw = [&](double intensity, bool dust) {
        Renderer renderer;
        Framebuffer framebuffer;
        framebuffer.resize(320, 240);
        WeatherState weather;
        weather.kind = dust ? Precipitation::DustStorm : Precipitation::None;
        weather.intensity = dust ? intensity : 0.0;
        SceneView scene;
        scene.chunks = &source;
        scene.lighting = lightingAt(0.5);
        scene.weather = &weather;
        renderer.drawFrame(framebuffer, scene);
        return framebuffer;
    };

    const Framebuffer clear = draw(0.0, false);
    const Framebuffer mild = draw(0.3, true);
    const Framebuffer fierce = draw(1.0, true);
    // The haze touches almost every pixel, and a bigger storm changes more.
    const int pixelCount = clear.width() * clear.height();
    REQUIRE(countDifferences(clear, mild) > pixelCount / 2);
    REQUIRE(countDifferences(mild, fierce) > pixelCount / 2);

    // Fully hazy edges look more like the haze colour than the centre does.
    const PlanetPalette palette = defaultPlanetPalette();
    const auto distanceFromHaze = [&palette](PackedColor pixel) {
        return std::abs(static_cast<int>(pixel & 0xff) - palette.desert.red) +
               std::abs(static_cast<int>((pixel >> 8) & 0xff) - palette.desert.green) +
               std::abs(static_cast<int>((pixel >> 16) & 0xff) - palette.desert.blue);
    };
    const int corner = distanceFromHaze(fierce.pixels()[0]);
    const int cornerClear = distanceFromHaze(clear.pixels()[0]);
    REQUIRE(corner < cornerClear);
}

TEST_CASE("the game announces a meteor shower once per shower night", "[sky][game]") {
    // Look for a seed whose first game night has a shower, then run through the dark.
    for (WorldSeed seed = 1; seed < 200; ++seed) {
        Game game(seed);
        // Start just after dusk, so it is already properly dark, and work out which night this is.
        game.setTimeOfDay(0.85);
        const int night = nightNumberFor(game.dayNight().timeOfDay(), game.dayNight().day());
        if (!isMeteorShowerNight(seed, night)) continue;

        // Count from the very first step: the announcement fires as soon as it is dark enough.
        for (int frame = 0; frame < 60 * 20; ++frame) game.advanceFrame(step);
        int notices = 0;
        for (const GameEvent& event : game.takeEvents()) {
            if (event.type == GameEventType::Notice && event.text == "Meteor shower tonight") ++notices;
        }
        REQUIRE(notices == 1);
        return;
    }
    FAIL("no seed with a shower on the first night");
}
