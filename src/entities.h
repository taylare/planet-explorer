// The player (and, from Stage 4, wildlife).
#pragma once

#include "input.h"
#include "world.h"

namespace wl {

struct Vec2 {
    double x = 0.0;
    double y = 0.0;
};

Vec2 lerp(const Vec2& from, const Vec2& to, double t);

// Ground speed multiplier for a biome. Zero means you can't walk there.
double terrainSpeedMultiplier(Biome biome);

// Unloaded tiles count as blocked, so the player can't walk off the edge of the generated world.
bool isWalkable(const ChunkSource& terrain, double tileX, double tileY);

class Player {
public:
    void placeAt(double tileX, double tileY);

    void update(const MoveIntent& intent, const ChunkSource& terrain, double stepSeconds);

    // Position blended between the last two sim steps, for smooth rendering.
    Vec2 interpolatedPosition(double alpha) const { return lerp(previousPosition_, position_, alpha); }

    const Vec2& position() const { return position_; }
    const Vec2& velocity() const { return velocity_; }
    bool isFacingLeft() const { return facingLeft_; }
    bool isWading() const { return wading_; }
    bool isMoving() const;
    double distanceWalked() const { return distanceWalked_; }

private:
    Vec2 position_;
    Vec2 previousPosition_;
    Vec2 velocity_;
    bool facingLeft_ = false;
    bool wading_ = false;
    double distanceWalked_ = 0.0;
};

}  // namespace wl
