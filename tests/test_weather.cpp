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
}  // namespace

TEST_CASE("wet ground under cloud gets rain, cold ground gets snow", "[weather]") {
    const WeatherTarget rain = weatherTargetFrom(heavyCloud, wet, warm);
    REQUIRE(rain.kind == Precipitation::Rain);
    REQUIRE(rain.intensity > 0.9);

    const WeatherTarget snow = weatherTargetFrom(heavyCloud, wet, cold);
    REQUIRE(snow.kind == Precipitation::Snow);
}

TEST_CASE("clear skies and dry ground stay dry", "[weather]") {
    REQUIRE(weatherTargetFrom(clearSky, wet, warm).kind == Precipitation::None);
    REQUIRE(weatherTargetFrom(heavyCloud, dry, warm).kind == Precipitation::None);
    REQUIRE(weatherTargetFrom(heavyCloud, dry, warm).intensity == 0.0);
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
