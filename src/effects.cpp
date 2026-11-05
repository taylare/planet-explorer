#include "effects.h"

#include <cmath>

#include "constants.h"

namespace wl {

bool leavesFootprints(Biome biome) {
    return biome == Biome::Beach || biome == Biome::Desert || biome == Biome::Snow;
}

void TrailEffects::clear() {
    footprints_.clear();
    ripples_.clear();
}

void TrailEffects::update(const Player& player, Biome groundBiome, double nowSeconds) {
    using namespace constants;

    // Oldest are at the front, so pruning is just popping until something is young enough.
    while (!footprints_.empty() && nowSeconds - footprints_.front().bornSeconds > footprintLifetimeSeconds) {
        footprints_.pop_front();
    }
    while (!ripples_.empty() && nowSeconds - ripples_.front().bornSeconds > rippleLifetimeSeconds) {
        ripples_.pop_front();
    }

    const Vec2& position = player.position();
    const auto addRipple = [&]() {
        ripples_.push_back({position.x, position.y, nowSeconds});
        if (static_cast<int>(ripples_.size()) > maxRipples) ripples_.pop_front();
        lastRippleSeconds_ = nowSeconds;
    };

    // One mark per stride, using the same stride length as the walk animation.
    const long long strideIndex = static_cast<long long>(player.distanceWalked() / strideLengthInTiles);
    if (strideIndex != lastStrideIndex_) {
        lastStrideIndex_ = strideIndex;
        if (player.isWading()) {
            addRipple();
        } else if (leavesFootprints(groundBiome)) {
            // Offset sideways from the direction of travel, alternating feet.
            const Vec2& velocity = player.velocity();
            const double speed = std::hypot(velocity.x, velocity.y);
            double sideX = 0.0;
            double sideY = 0.0;
            if (speed > 0.0) {
                sideX = -velocity.y / speed;
                sideY = velocity.x / speed;
            }
            const double offset = footprintSideOffsetInTiles * nextFootSide_;
            footprints_.push_back({position.x + sideX * offset, position.y + sideY * offset, nowSeconds});
            if (static_cast<int>(footprints_.size()) > maxFootprints) footprints_.pop_front();
            nextFootSide_ = -nextFootSide_;
        }
    }

    // Standing in water sends out the odd ripple even without walking.
    if (player.isWading() && nowSeconds - lastRippleSeconds_ > idleRippleIntervalSeconds) addRipple();
}

}  // namespace wl
