#include "day_night.h"

#include <cmath>
#include <iterator>

#include "constants.h"

namespace wl {

namespace {
// Wrap into [0, 1). fmod keeps the sign, so negatives need a nudge.
double wrapUnit(double value) {
    const double wrapped = std::fmod(value, 1.0);
    return wrapped < 0.0 ? wrapped + 1.0 : wrapped;
}

// Smoothstep between keys so colour eases in and out rather than kinking.
float easeBetween(float from, float to, double t) {
    const double eased = t * t * (3.0 - 2.0 * t);
    return static_cast<float>(from + (to - from) * eased);
}
}  // namespace

LightingState lightingAt(double timeOfDay) {
    const double time = wrapUnit(timeOfDay);
    const auto& keys = constants::lightKeyframes;
    constexpr int keyCount = static_cast<int>(std::size(constants::lightKeyframes));

    // Keys are few and sorted, so a linear scan is the simplest correct lookup.
    int upperIndex = 1;
    while (upperIndex < keyCount - 1 && keys[upperIndex].timeOfDay <= time) ++upperIndex;
    const auto& lower = keys[upperIndex - 1];
    const auto& upper = keys[upperIndex];

    const double span = upper.timeOfDay - lower.timeOfDay;
    const double t = span > 0.0 ? (time - lower.timeOfDay) / span : 0.0;

    LightingState state;
    state.tintRed = easeBetween(lower.red, upper.red, t);
    state.tintGreen = easeBetween(lower.green, upper.green, t);
    state.tintBlue = easeBetween(lower.blue, upper.blue, t);
    state.nightAmount = easeBetween(lower.night, upper.night, t);
    return state;
}

DayNightCycle::DayNightCycle(double startTimeOfDay) : timeOfDay_(wrapUnit(startTimeOfDay)) {}

void DayNightCycle::advance(double seconds) {
    const double unwrapped = timeOfDay_ + seconds / constants::dayLengthSeconds;
    day_ += static_cast<int>(std::floor(unwrapped));  // count each midnight passed
    timeOfDay_ = wrapUnit(unwrapped);
}

void DayNightCycle::setTimeOfDay(double timeOfDay) { timeOfDay_ = wrapUnit(timeOfDay); }

}  // namespace wl
