#include "weather.h"

#include <algorithm>
#include <cmath>

#include "constants.h"
#include "hash.h"

namespace wl {

namespace {
// Seed salt for the cloud field. Never change it.
constexpr std::uint32_t weatherSalt = 301;

// Below this the weather counts as stopped, and the kind can switch.
constexpr double stoppedIntensity = 0.02;

// Fronts drift diagonally, a bit faster east-west than north-south.
constexpr double driftRatioY = 0.6;

double smoothstep(double edgeLow, double edgeHigh, double value) {
    const double t = std::clamp((value - edgeLow) / (edgeHigh - edgeLow), 0.0, 1.0);
    return t * t * (3.0 - 2.0 * t);
}

FractalNoiseSettings cloudSettings() {
    FractalNoiseSettings settings;
    settings.baseFrequency = constants::weatherBaseFrequency;
    settings.octaves = constants::weatherOctaves;
    settings.lacunarity = constants::noiseLacunarity;
    settings.persistence = constants::noisePersistence;
    return settings;
}
}  // namespace

WeatherTarget weatherTargetFrom(double cloudNoiseValue, double moisture, double temperature) {
    using namespace constants;
    const double cloudCover = smoothstep(weatherCloudStart, weatherCloudFull, cloudNoiseValue);
    const double wetness = smoothstep(weatherMoistureStart, weatherMoistureFull, moisture);

    WeatherTarget target;
    target.intensity = cloudCover * wetness;
    if (target.intensity >= stoppedIntensity) {
        target.kind = temperature < snowfallTemperature ? Precipitation::Snow : Precipitation::Rain;
        return target;
    }

    // No rain or snow here. Hot, dry ground under cloud kicks up dust instead.
    const double dryness = 1.0 - smoothstep(weatherDustMoistureFull, weatherDustMoistureNone, moisture);
    const double heat = smoothstep(weatherDustTemperatureStart, weatherDustTemperatureFull, temperature);
    const double dustIntensity = cloudCover * dryness * heat;
    if (dustIntensity >= stoppedIntensity) {
        target.kind = Precipitation::DustStorm;
        target.intensity = dustIntensity;
        return target;
    }

    target.intensity = 0.0;
    return target;
}

WeatherSystem::WeatherSystem(WorldSeed seed) : cloudNoise_(deriveSeed(seed, weatherSalt), cloudSettings()) {}

WeatherTarget WeatherSystem::targetAt(const World& world, double tileX, double tileY, double timeSeconds) const {
    const double drift = timeSeconds * constants::weatherDriftTilesPerSecond;
    const double cloud = cloudNoise_.sample(tileX + drift, tileY + drift * driftRatioY);
    const TerrainSample ground =
        world.sampleTerrain(static_cast<int>(std::floor(tileX)), static_cast<int>(std::floor(tileY)));
    return weatherTargetFrom(cloud, ground.moisture, ground.temperature);
}

void WeatherSystem::update(const World& world, const Vec2& playerPosition, double timeSeconds, double stepSeconds) {
    secondsUntilSample_ -= stepSeconds;
    if (secondsUntilSample_ <= 0.0) {
        target_ = targetAt(world, playerPosition.x, playerPosition.y, timeSeconds);
        secondsUntilSample_ = constants::weatherSampleIntervalSeconds;
    }
    stepTowards(target_, stepSeconds);
}

void WeatherSystem::stepTowards(const WeatherTarget& target, double stepSeconds) {
    const double ease = 1.0 - std::exp(-constants::weatherEaseSharpness * stepSeconds);
    const bool kindChanging = target.kind != state_.kind && state_.kind != Precipitation::None;

    if (kindChanging) {
        // Let the old weather die down first, then switch.
        state_.intensity += (0.0 - state_.intensity) * ease;
        if (state_.intensity < stoppedIntensity) {
            state_.intensity = 0.0;
            state_.kind = target.kind;
        }
        return;
    }

    state_.kind = target.kind == Precipitation::None ? state_.kind : target.kind;
    state_.intensity += (target.intensity - state_.intensity) * ease;
    if (target.kind == Precipitation::None && state_.intensity < stoppedIntensity) {
        state_.intensity = 0.0;
        state_.kind = Precipitation::None;
    }
}

}  // namespace wl
