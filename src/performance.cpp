#include "performance.h"

#include <chrono>

#include "constants.h"

namespace wl {

namespace {
double blend(double smoothed, double sample) {
    return smoothed + (sample - smoothed) * constants::profilerSmoothing;
}
}  // namespace

void FrameProfiler::record(double simulationMs, double streamingMs, double renderMs) {
    if (!hasSample_) {
        // Start from the first real sample rather than creeping up from zero.
        smoothed_ = {simulationMs, streamingMs, renderMs};
        hasSample_ = true;
        return;
    }
    smoothed_.simulationMs = blend(smoothed_.simulationMs, simulationMs);
    smoothed_.streamingMs = blend(smoothed_.streamingMs, streamingMs);
    smoothed_.renderMs = blend(smoothed_.renderMs, renderMs);
}

bool QualityGovernor::update(double frameWorkMs) {
    using namespace constants;
    slowFrames_ = frameWorkMs > qualityDowngradeMs ? slowFrames_ + 1 : 0;
    fastFrames_ = frameWorkMs < qualityUpgradeMs ? fastFrames_ + 1 : 0;

    if (slowFrames_ >= qualityDowngradeFrames && level_ < qualityLevelCount - 1) {
        ++level_;
        slowFrames_ = 0;
        fastFrames_ = 0;
        return true;
    }
    if (fastFrames_ >= qualityUpgradeFrames && level_ > 0) {
        --level_;
        slowFrames_ = 0;
        fastFrames_ = 0;
        return true;
    }
    return false;
}

void QualityGovernor::reset() {
    level_ = 0;
    slowFrames_ = 0;
    fastFrames_ = 0;
}

double nowMilliseconds() {
    using namespace std::chrono;
    return duration<double, std::milli>(steady_clock::now().time_since_epoch()).count();
}

}  // namespace wl
