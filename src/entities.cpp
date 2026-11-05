#include "entities.h"

#include <cmath>

#include "constants.h"

namespace wl {

namespace {
int tileIndexFor(double position) { return static_cast<int>(std::floor(position)); }

// Frame-rate independent smoothing: the same fraction of the gap closes per
// second no matter how the time is sliced.
double smoothingFactor(double sharpness, double stepSeconds) {
    return 1.0 - std::exp(-sharpness * stepSeconds);
}
}  // namespace

Vec2 lerp(const Vec2& from, const Vec2& to, double t) {
    return {from.x + (to.x - from.x) * t, from.y + (to.y - from.y) * t};
}

double terrainSpeedMultiplier(Biome biome) {
    switch (biome) {
        case Biome::DeepOcean: return 0.0;
        case Biome::ShallowWater: return constants::shallowWaterSpeedMultiplier;
        case Biome::Beach:
        case Biome::Desert: return constants::sandSpeedMultiplier;
        case Biome::Snow: return constants::snowSpeedMultiplier;
        case Biome::Mountain: return constants::mountainSpeedMultiplier;
        default: return 1.0;
    }
}

bool isWalkable(const ChunkSource& terrain, double tileX, double tileY) {
    Biome biome = Biome::DeepOcean;
    if (!tryGetBiome(terrain, tileIndexFor(tileX), tileIndexFor(tileY), biome)) return false;
    return terrainSpeedMultiplier(biome) > 0.0;
}

void Player::placeAt(double tileX, double tileY) {
    position_ = {tileX, tileY};
    previousPosition_ = position_;
    velocity_ = {};
}

bool Player::isMoving() const {
    return std::hypot(velocity_.x, velocity_.y) > constants::idleSpeedTilesPerSecond;
}

void Player::update(const MoveIntent& intent, const ChunkSource& terrain, double stepSeconds) {
    previousPosition_ = position_;

    Biome groundBiome = Biome::Grassland;
    tryGetBiome(terrain, tileIndexFor(position_.x), tileIndexFor(position_.y), groundBiome);
    const double topSpeed = constants::walkSpeedTilesPerSecond * terrainSpeedMultiplier(groundBiome);

    // Ease toward the target velocity instead of snapping, so starts and stops feel soft.
    const Vec2 targetVelocity{intent.x * topSpeed, intent.y * topSpeed};
    velocity_ = lerp(velocity_, targetVelocity, smoothingFactor(constants::playerAccelerationSharpness, stepSeconds));

    // Move one axis at a time. If one is blocked the other still works,
    // so you slide along a coastline instead of sticking to it.
    const Vec2 startPosition = position_;
    const double nextX = position_.x + velocity_.x * stepSeconds;
    if (isWalkable(terrain, nextX, position_.y)) {
        position_.x = nextX;
    } else {
        velocity_.x = 0.0;
    }
    const double nextY = position_.y + velocity_.y * stepSeconds;
    if (isWalkable(terrain, position_.x, nextY)) {
        position_.y = nextY;
    } else {
        velocity_.y = 0.0;
    }

    distanceWalked_ += std::hypot(position_.x - startPosition.x, position_.y - startPosition.y);

    // Only flip on clear sideways movement, so walking straight up doesn't flicker.
    if (std::abs(velocity_.x) > constants::idleSpeedTilesPerSecond) facingLeft_ = velocity_.x < 0.0;

    Biome biomeNow = Biome::Grassland;
    tryGetBiome(terrain, tileIndexFor(position_.x), tileIndexFor(position_.y), biomeNow);
    wading_ = biomeNow == Biome::ShallowWater;
}

}  // namespace wl
