// The player and wildlife.
#pragma once

#include <cstdint>
#include <string_view>
#include <unordered_set>
#include <vector>

#include "chunk_cache.h"
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

// --- Player ---

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

// --- Wildlife ---

enum class Species : std::uint8_t { Deer, Rabbit, Crab, Lizard, Goat, Fox, Count };
inline constexpr int speciesCount = static_cast<int>(Species::Count);

std::string_view speciesName(Species species);
const constants::SpeciesTuning& speciesTuning(Species species);
bool canSpeciesStandOn(Species species, Biome biome);

// Which species lives in a biome, picked by roll (0 to 1). False if none do.
bool speciesForBiome(Biome biome, float roll, Species& speciesOut);

struct Creature {
    Species species = Species::Rabbit;
    Vec2 position;
    Vec2 previousPosition;
    Vec2 velocity;
    Vec2 home;
    ChunkCoord homeChunk;
    double wanderAngle = 0.0;
    double stateSecondsLeft = 0.0;
    bool resting = false;
    bool fleeing = false;
    bool facingLeft = false;
    std::uint32_t randomState = 1;  // per-creature random stream, never zero
    double distanceMoved = 0.0;

    Vec2 interpolatedPosition(double alpha) const { return lerp(previousPosition, position, alpha); }
};

Creature makeCreature(Species species, const Vec2& position, ChunkCoord homeChunk, std::uint32_t seedHash);

// One fixed step of steering: wander, rest, flee the player, stay near home,
// and turn around at ground the species can't stand on.
void stepCreature(Creature& creature, const ChunkSource& terrain, const Vec2& playerPosition,
                  double stepSeconds);

// Spawns creatures for chunks near the player and removes them when their
// home chunk drifts out of range. Nothing is saved: revisiting a chunk
// spawns the same creatures again from the seed.
class Wildlife {
public:
    void update(const ChunkRegion& simulationRegion, WorldSeed seed, const ChunkSource& terrain,
                const Vec2& playerPosition, double stepSeconds);

    const std::vector<Creature>& creatures() const { return creatures_; }
    void clear();

private:
    void spawnForChunk(const Chunk& chunk, WorldSeed seed);

    std::vector<Creature> creatures_;
    std::unordered_set<ChunkCoord, ChunkCoordHash> spawnedChunks_;
};

}  // namespace wl
