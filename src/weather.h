// Regional weather. Fronts drift across the world from a slow noise field;
// wet ground under a front gets rain, cold ground gets snow.
#pragma once

#include "entities.h"
#include "noise.h"
#include "world.h"

namespace wl {

enum class Precipitation { None, Rain, Snow };

struct WeatherTarget {
    Precipitation kind = Precipitation::None;
    double intensity = 0.0;  // 0 to 1
};

struct WeatherState {
    Precipitation kind = Precipitation::None;
    double intensity = 0.0;
};

// Pure rule: cloud cover (noise value), ground moisture and temperature in,
// precipitation out. Kept separate from the noise so it can be tested directly.
WeatherTarget weatherTargetFrom(double cloudNoiseValue, double moisture, double temperature);

class WeatherSystem {
public:
    explicit WeatherSystem(WorldSeed seed);

    // What the weather wants to be at a spot and time.
    WeatherTarget targetAt(const World& world, double tileX, double tileY, double timeSeconds) const;

    // Re-samples the target now and then, and eases toward it.
    void update(const World& world, const Vec2& playerPosition, double timeSeconds, double stepSeconds);

    // Eases the current state toward a target. Switching between rain and
    // snow fades the old one out completely before the new one starts.
    void stepTowards(const WeatherTarget& target, double stepSeconds);

    const WeatherState& state() const { return state_; }

private:
    FractalNoise cloudNoise_;
    WeatherState state_;
    WeatherTarget target_;
    double secondsUntilSample_ = 0.0;
};

}  // namespace wl
