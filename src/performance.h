// Frame timing and automatic quality steps.
#pragma once

namespace wl {

// Smoothed per-frame timings in milliseconds. An exponential moving
// average: each frame nudges the value, so one slow frame barely moves it.
struct FrameTimings {
    double simulationMs = 0.0;
    double streamingMs = 0.0;
    double renderMs = 0.0;

    double totalMs() const { return simulationMs + streamingMs + renderMs; }
};

class FrameProfiler {
public:
    void record(double simulationMs, double streamingMs, double renderMs);
    const FrameTimings& smoothed() const { return smoothed_; }

private:
    FrameTimings smoothed_;
    bool hasSample_ = false;
};

// Picks a quality level from frame cost. Steps down after a sustained run of
// slow frames and back up after a much longer run of easy ones. The gap
// between the two thresholds stops it bouncing between levels.
class QualityGovernor {
public:
    // Returns true when the level changed this frame.
    bool update(double frameWorkMs);

    int level() const { return level_; }
    void reset();

private:
    int level_ = 0;
    int slowFrames_ = 0;
    int fastFrames_ = 0;
};

// Current time in milliseconds from a steady clock.
double nowMilliseconds();

}  // namespace wl
