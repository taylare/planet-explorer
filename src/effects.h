// Footprints in sand and snow, ripples in water. Short-lived and never saved.
#pragma once

#include <deque>

#include "entities.h"
#include "world.h"

namespace wl {

struct Footprint {
    double tileX = 0.0;
    double tileY = 0.0;
    double bornSeconds = 0.0;
};

struct Ripple {
    double tileX = 0.0;
    double tileY = 0.0;
    double bornSeconds = 0.0;
};

bool leavesFootprints(Biome biome);

class TrailEffects {
public:
    // Call once per sim step with the biome under the player's feet.
    void update(const Player& player, Biome groundBiome, double nowSeconds);
    void clear();

    const std::deque<Footprint>& footprints() const { return footprints_; }
    const std::deque<Ripple>& ripples() const { return ripples_; }

private:
    std::deque<Footprint> footprints_;
    std::deque<Ripple> ripples_;
    long long lastStrideIndex_ = 0;
    int nextFootSide_ = 1;  // +1 or -1, so prints alternate left and right
    double lastRippleSeconds_ = -1e9;
};

}  // namespace wl
