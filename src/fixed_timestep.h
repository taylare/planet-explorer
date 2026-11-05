// Turns uneven frame times into equal simulation steps.
// Leftover time carries over to the next frame so nothing is lost.
#pragma once

namespace wl {

class FixedTimestepClock {
public:
    FixedTimestepClock(double stepSeconds, int maxStepsPerFrame, double maxFrameDeltaSeconds);

    // Returns how many steps to simulate this frame.
    int consumeElapsedTime(double realDeltaSeconds);

    // How far we are into the next step (0 to 1). Handy for smoothing.
    double interpolationAlpha() const;

    double stepSeconds() const { return stepSeconds_; }

private:
    double stepSeconds_;
    int maxStepsPerFrame_;
    double maxFrameDeltaSeconds_;
    double accumulatedSeconds_ = 0.0;
};

}  // namespace wl
