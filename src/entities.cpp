#include "entities.h"

#include <algorithm>
#include <cmath>
#include <iterator>

#include "constants.h"
#include "hash.h"

namespace wl {

namespace {
constexpr double pi = 3.14159265358979323846;

// Salt for creature spawning. Never change it, or every world's wildlife moves.
constexpr std::uint32_t wildlifeSalt = 201;

int tileIndexFor(double position) { return static_cast<int>(std::floor(position)); }

// Frame-rate independent smoothing: the same fraction of the gap closes per
// second no matter how the time is sliced.
double smoothingFactor(double sharpness, double stepSeconds) {
    return 1.0 - std::exp(-sharpness * stepSeconds);
}

// xorshift32: tiny and fast, plenty for wander jitter.
float nextRandomUnit(std::uint32_t& state) {
    state ^= state << 13;
    state ^= state >> 17;
    state ^= state << 5;
    return hashToUnitFloat(state);
}

double randomBetween(std::uint32_t& state, double low, double high) {
    return low + (high - low) * nextRandomUnit(state);
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

void Player::update(const MoveIntent& intent, const ChunkSource& terrain, double stepSeconds,
                    double speedMultiplier) {
    previousPosition_ = position_;

    Biome groundBiome = Biome::Grassland;
    tryGetBiome(terrain, tileIndexFor(position_.x), tileIndexFor(position_.y), groundBiome);
    const double topSpeed = constants::walkSpeedTilesPerSecond * terrainSpeedMultiplier(groundBiome) * speedMultiplier;

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

// --- Wildlife ---

std::string_view speciesName(Species species) {
    switch (species) {
        case Species::Deer: return "Spire Grazer";
        case Species::Rabbit: return "Burrow Hopper";
        case Species::Crab: return "Tide Scuttler";
        case Species::Lizard: return "Dust Skitter";
        case Species::Goat: return "Ridge Climber";
        case Species::Fox: return "Frost Stalker";
        case Species::Count: break;
    }
    return "Unknown";
}

const constants::SpeciesTuning& speciesTuning(Species species) {
    switch (species) {
        case Species::Deer: return constants::deerTuning;
        case Species::Rabbit: return constants::rabbitTuning;
        case Species::Crab: return constants::crabTuning;
        case Species::Lizard: return constants::lizardTuning;
        case Species::Goat: return constants::goatTuning;
        case Species::Fox: return constants::foxTuning;
        case Species::Count: break;
    }
    return constants::rabbitTuning;
}

bool canSpeciesStandOn(Species species, Biome biome) {
    if (species == Species::Crab) return biome == Biome::Beach || biome == Biome::ShallowWater;
    return !isWater(biome);
}

bool speciesForBiome(Biome biome, float roll, Species& speciesOut) {
    switch (biome) {
        case Biome::Forest: speciesOut = Species::Deer; return true;
        case Biome::Grassland: speciesOut = roll < 0.6f ? Species::Rabbit : Species::Deer; return true;
        case Biome::Savanna: speciesOut = roll < 0.5f ? Species::Rabbit : Species::Lizard; return true;
        case Biome::Desert: speciesOut = Species::Lizard; return true;
        case Biome::Beach: speciesOut = Species::Crab; return true;
        case Biome::Mountain: speciesOut = Species::Goat; return true;
        case Biome::Tundra:
            speciesOut = roll < 0.4f ? Species::Goat : (roll < 0.7f ? Species::Fox : Species::Rabbit);
            return true;
        case Biome::Snow: speciesOut = Species::Fox; return true;
        default: return false;
    }
}

Creature makeCreature(Species species, const Vec2& position, ChunkCoord homeChunk, std::uint32_t seedHash) {
    Creature creature;
    creature.species = species;
    creature.position = position;
    creature.previousPosition = position;
    creature.home = position;
    creature.homeChunk = homeChunk;
    creature.randomState = seedHash | 1u;  // xorshift gets stuck on zero
    creature.wanderAngle = randomBetween(creature.randomState, 0.0, 2.0 * pi);
    creature.resting = nextRandomUnit(creature.randomState) < 0.5f;
    creature.stateSecondsLeft = randomBetween(creature.randomState, 0.0, constants::creatureRestMaxSeconds);
    return creature;
}

void stepCreature(Creature& creature, const ChunkSource& terrain, const Vec2& playerPosition,
                  double stepSeconds, bool playerSneaking) {
    using namespace constants;
    const SpeciesTuning& tuning = speciesTuning(creature.species);
    creature.previousPosition = creature.position;

    // Swap between resting and wandering on a timer.
    creature.stateSecondsLeft -= stepSeconds;
    if (creature.stateSecondsLeft <= 0.0) {
        creature.resting = !creature.resting;
        creature.stateSecondsLeft =
            creature.resting ? randomBetween(creature.randomState, creatureRestMinSeconds, creatureRestMaxSeconds)
                             : randomBetween(creature.randomState, creatureWanderMinSeconds, creatureWanderMaxSeconds);
    }

    // Pick a desired velocity. Order is priority: fear beats homesickness beats wandering.
    Vec2 desired;
    const double awayX = creature.position.x - playerPosition.x;
    const double awayY = creature.position.y - playerPosition.y;
    const double playerDistance = std::hypot(awayX, awayY);
    const double homeX = creature.home.x - creature.position.x;
    const double homeY = creature.home.y - creature.position.y;
    const double homeDistance = std::hypot(homeX, homeY);

    const double startleRadius = playerSneaking ? tuning.fleeRadius * sneakStartleFraction : tuning.fleeRadius;
    creature.fleeing = playerDistance < startleRadius && playerDistance > 0.0;
    if (creature.fleeing) {
        desired = {awayX / playerDistance * tuning.fleeSpeed, awayY / playerDistance * tuning.fleeSpeed};
        creature.wanderAngle = std::atan2(awayY, awayX);  // keep going that way once safe
    } else if (homeDistance > creatureLeashRadiusInTiles) {
        desired = {homeX / homeDistance * tuning.walkSpeed, homeY / homeDistance * tuning.walkSpeed};
        creature.wanderAngle = std::atan2(homeY, homeX);
    } else if (!creature.resting) {
        // Wander: heading drifts a little each step, so paths curve naturally.
        creature.wanderAngle +=
            randomBetween(creature.randomState, -1.0, 1.0) * creatureWanderJitterRadiansPerSecond * stepSeconds;
        desired = {std::cos(creature.wanderAngle) * tuning.walkSpeed,
                   std::sin(creature.wanderAngle) * tuning.walkSpeed};
    }

    // Steer: ease the velocity toward the desired one instead of snapping.
    creature.velocity = lerp(creature.velocity, desired, smoothingFactor(creatureSteeringSharpness, stepSeconds));

    const Vec2 next{creature.position.x + creature.velocity.x * stepSeconds,
                    creature.position.y + creature.velocity.y * stepSeconds};
    Biome nextBiome = Biome::DeepOcean;
    const bool canMove = tryGetBiome(terrain, tileIndexFor(next.x), tileIndexFor(next.y), nextBiome) &&
                         canSpeciesStandOn(creature.species, nextBiome);
    if (canMove) {
        creature.distanceMoved += std::hypot(next.x - creature.position.x, next.y - creature.position.y);
        creature.position = next;
        // Only turn on clear sideways movement, so resting creatures don't flicker.
        if (std::abs(creature.velocity.x) > idleSpeedTilesPerSecond * 0.5) {
            creature.facingLeft = creature.velocity.x < 0.0;
        }
    } else {
        // Bumped into water or the edge of the loaded world: stop and turn round.
        creature.velocity = {};
        creature.wanderAngle += pi;
    }
}

void Wildlife::clear() {
    creatures_.clear();
    spawnedChunks_.clear();
}

void Wildlife::spawnForChunk(const Chunk& chunk, WorldSeed seed) {
    using namespace constants;
    const std::uint32_t chunkHash = hashCoords(deriveSeed(seed, wildlifeSalt), chunk.coord.x, chunk.coord.y);

    for (int attempt = 0; attempt < creatureSpawnAttemptsPerChunk; ++attempt) {
        const std::uint32_t attemptHash = hashCoords(chunkHash, attempt, 0);
        if (hashToUnitFloat(attemptHash) >= creatureSpawnChance) continue;

        const int localX = static_cast<int>(mixBits32(attemptHash + 1) % chunkSizeInTiles);
        const int localY = static_cast<int>(mixBits32(attemptHash + 2) % chunkSizeInTiles);
        Species species = Species::Rabbit;
        const Biome biome = chunk.biomes[Chunk::indexFor(localX, localY)];
        if (!speciesForBiome(biome, hashToUnitFloat(mixBits32(attemptHash + 3)), species)) continue;
        if (!canSpeciesStandOn(species, biome)) continue;

        const Vec2 position{chunkOriginTile(chunk.coord.x) + localX + 0.5,
                            chunkOriginTile(chunk.coord.y) + localY + 0.5};
        creatures_.push_back(makeCreature(species, position, chunk.coord, mixBits32(attemptHash + 4)));
    }
}

void Wildlife::update(const ChunkRegion& simulationRegion, WorldSeed seed, const ChunkSource& terrain,
                      const Vec2& playerPosition, double stepSeconds, bool playerSneaking) {
    // Drop creatures whose home has left the area, and forget those chunks
    // so they spawn fresh if the player comes back.
    creatures_.erase(std::remove_if(creatures_.begin(), creatures_.end(),
                                    [&simulationRegion](const Creature& creature) {
                                        return !simulationRegion.contains(creature.homeChunk);
                                    }),
                     creatures_.end());
    for (auto it = spawnedChunks_.begin(); it != spawnedChunks_.end();) {
        it = simulationRegion.contains(*it) ? std::next(it) : spawnedChunks_.erase(it);
    }

    for (int chunkY = simulationRegion.minChunkY; chunkY <= simulationRegion.maxChunkY; ++chunkY) {
        for (int chunkX = simulationRegion.minChunkX; chunkX <= simulationRegion.maxChunkX; ++chunkX) {
            const ChunkCoord coord{chunkX, chunkY};
            if (spawnedChunks_.count(coord) != 0) continue;
            const Chunk* chunk = terrain.findChunk(coord);
            if (!chunk) continue;  // not streamed in yet; try again next step
            spawnForChunk(*chunk, seed);
            spawnedChunks_.insert(coord);
        }
    }

    for (Creature& creature : creatures_) {
        stepCreature(creature, terrain, playerPosition, stepSeconds, playerSneaking);
    }
}

}  // namespace wl
