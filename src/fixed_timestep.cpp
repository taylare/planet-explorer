#include "fixed_timestep.h"

#include <algorithm>

namespace wl {

FixedTimestepClock::FixedTimestepClock(double stepSeconds, int maxStepsPerFrame,
                                       double maxFrameDeltaSeconds)
    : stepSeconds_(stepSeconds),
      maxStepsPerFrame_(maxStepsPerFrame),
      maxFrameDeltaSeconds_(maxFrameDeltaSeconds) {}

int FixedTimestepClock::consumeElapsedTime(double realDeltaSeconds) {
    // Negative deltas happen if the clock jumps. Treat as zero.
    accumulatedSeconds_ += std::clamp(realDeltaSeconds, 0.0, maxFrameDeltaSeconds_);

    int stepsToRun = 0;
    while (accumulatedSeconds_ >= stepSeconds_ && stepsToRun < maxStepsPerFrame_) {
        accumulatedSeconds_ -= stepSeconds_;
        ++stepsToRun;
    }

    // Still behind after hitting the cap? Drop the backlog or we never recover.
    if (accumulatedSeconds_ >= stepSeconds_) accumulatedSeconds_ = 0.0;

    return stepsToRun;
}

double FixedTimestepClock::interpolationAlpha() const {
    return accumulatedSeconds_ / stepSeconds_;
}

}  // namespace wl
