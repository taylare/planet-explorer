#include <catch2/catch_test_macros.hpp>

#include "constants.h"
#include "weather.h"

using namespace wl;

namespace {
constexpr double step = constants::simulationStepSeconds;
constexpr double heavyCloud = 0.9;
constexpr double clearSky = -0.5;
constexpr double wet = 0.9;
constexpr double dry = 0.1;
constexpr double warm = 0.6;
constexpr double cold = 0.1;
constexpr double scorching = 0.8;
}  // namespace

TEST_CASE("wet ground under cloud gets rain, cold ground gets snow", "[weather]") {
    const WeatherTarget rain = weatherTargetFrom(heavyCloud, wet, warm);
    REQUIRE(rain.kind == Precipitation::Rain);
    REQUIRE(rain.intensity > 0.9);

    const WeatherTarget snow = weatherTargetFrom(heavyCloud, wet, cold);
    REQUIRE(snow.kind == Precipitation::Snow);
}

TEST_CASE("clear skies and cold dry ground stay dry", "[weather]") {
    REQUIRE(weatherTargetFrom(clearSky, wet, warm).kind == Precipitation::None);
    REQUIRE(weatherTargetFrom(clearSky, dry, scorching).kind == Precipitation::None);
    // Dry but cold: no rain to fall and nothing to kick up as dust.
    REQUIRE(weatherTargetFrom(heavyCloud, dry, cold).kind == Precipitation::None);
    REQUIRE(weatherTargetFrom(heavyCloud, dry, cold).intensity == 0.0);
}

TEST_CASE("hot dry ground under heavy cloud gets a dust storm, not rain", "[weather][dust]") {
    const WeatherTarget storm = weatherTargetFrom(heavyCloud, dry, scorching);
    REQUIRE(storm.kind == Precipitation::DustStorm);
    REQUIRE(storm.intensity > 0.9);

    // Merely warm is a weaker storm than scorching.
    const WeatherTarget mild = weatherTargetFrom(heavyCloud, dry, warm);
    REQUIRE(mild.kind == Precipitation::DustStorm);
    REQUIRE(mild.intensity < storm.intensity);

    // More cloud, more dust.
    REQUIRE(weatherTargetFrom(0.2, dry, scorching).intensity < storm.intensity);

    // Wet ground still gets rain however hot it is.
    REQUIRE(weatherTargetFrom(heavyCloud, wet, scorching).kind == Precipitation::Rain);
}

TEST_CASE("sweeping moisture gives a sensible result at every step when hot", "[weather][dust]") {
    for (double moisture = 0.0; moisture <= 1.0; moisture += 0.01) {
        const WeatherTarget target = weatherTargetFrom(heavyCloud, moisture, scorching);
        REQUIRE(target.intensity >= 0.0);
        REQUIRE(target.intensity <= 1.0);
        if (target.kind == Precipitation::None) REQUIRE(target.intensity == 0.0);
        // Too hot for snow at any moisture. (A thin band between the dust and rain ranges is
        // allowed to be clear.)
        REQUIRE(target.kind != Precipitation::Snow);
    }
    REQUIRE(weatherTargetFrom(heavyCloud, 0.0, scorching).kind == Precipitation::DustStorm);
    REQUIRE(weatherTargetFrom(heavyCloud, 1.0, scorching).kind == Precipitation::Rain);
}

TEST_CASE("a dust storm dies down before rain starts", "[weather][dust]") {
    WeatherSystem weather(1);
    for (int i = 0; i < 60 * 30; ++i) weather.stepTowards({Precipitation::DustStorm, 1.0}, step);
    REQUIRE(weather.state().kind == Precipitation::DustStorm);

    bool sawRain = false;
    for (int i = 0; i < 60 * 60; ++i) {
        weather.stepTowards({Precipitation::Rain, 1.0}, step);
        if (weather.state().kind == Precipitation::Rain) {
            sawRain = true;
            REQUIRE(weather.state().intensity < 0.1);  // started from nothing
            break;
        }
    }
    REQUIRE(sawRain);
}

TEST_CASE("weather intensity always stays in range", "[weather]") {
    for (double cloud = -1.0; cloud <= 1.0; cloud += 0.05) {
        for (double moisture = 0.0; moisture <= 1.0; moisture += 0.05) {
            const WeatherTarget target = weatherTargetFrom(cloud, moisture, warm);
            REQUIRE(target.intensity >= 0.0);
            REQUIRE(target.intensity <= 1.0);
        }
    }
}

TEST_CASE("weather fades in gradually", "[weather]") {
    WeatherSystem weather(1);
    const WeatherTarget rain{Precipitation::Rain, 1.0};
    weather.stepTowards(rain, step);
    REQUIRE(weather.state().kind == Precipitation::Rain);
    REQUIRE(weather.state().intensity < 0.05);  // nowhere near full after one step

    for (int i = 0; i < 60 * 30; ++i) weather.stepTowards(rain, step);
    REQUIRE(weather.state().intensity > 0.95);
}

TEST_CASE("rain dies down fully before snow starts", "[weather]") {
    WeatherSystem weather(1);
    for (int i = 0; i < 60 * 30; ++i) weather.stepTowards({Precipitation::Rain, 1.0}, step);

    bool sawSnow = false;
    for (int i = 0; i < 60 * 60; ++i) {
        weather.stepTowards({Precipitation::Snow, 1.0}, step);
        if (weather.state().kind == Precipitation::Snow) {
            // The moment it switched, it must have started from (almost) nothing.
            if (!sawSnow) REQUIRE(weather.state().intensity < 0.05);
            sawSnow = true;
        }
    }
    REQUIRE(sawSnow);
}

TEST_CASE("weather clears back to none", "[weather]") {
    WeatherSystem weather(1);
    for (int i = 0; i < 60 * 30; ++i) weather.stepTowards({Precipitation::Rain, 1.0}, step);
    for (int i = 0; i < 60 * 60; ++i) weather.stepTowards({Precipitation::None, 0.0}, step);
    REQUIRE(weather.state().kind == Precipitation::None);
    REQUIRE(weather.state().intensity == 0.0);
}

TEST_CASE("real weather targets are sane and deterministic", "[weather]") {
    const World world(77);
    const WeatherSystem first(77);
    const WeatherSystem second(77);
    for (int sample = 0; sample < 50; ++sample) {
        const double x = sample * 37.0 - 900.0;
        const double y = sample * -23.0 + 400.0;
        const WeatherTarget a = first.targetAt(world, x, y, sample * 10.0);
        const WeatherTarget b = second.targetAt(world, x, y, sample * 10.0);
        REQUIRE(a.kind == b.kind);
        REQUIRE(a.intensity == b.intensity);
        REQUIRE(a.intensity >= 0.0);
        REQUIRE(a.intensity <= 1.0);
    }
}
