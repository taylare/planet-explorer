#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <functional>
#include <memory>
#include <unordered_map>
#include <vector>

#include "constants.h"
#include "entities.h"
#include "game.h"
#include "scanner.h"

using namespace wl;

namespace {
constexpr double step = constants::simulationStepSeconds;

// A creature standing still at a spot, calm and not fleeing.
Creature calmCreature(Species species, double x, double y) {
    Creature creature = makeCreature(species, {x, y}, {0, 0}, 1);
    creature.fleeing = false;
    return creature;
}

// Runs the scanner for a number of seconds with the button held.
bool holdFor(Scanner& scanner, double seconds, const Vec2& player, const std::vector<Creature>& creatures,
             const Journal& journal) {
    bool completed = false;
    const int steps = static_cast<int>(seconds / step) + 1;
    for (int index = 0; index < steps && !completed; ++index) {
        completed = scanner.update(true, player, creatures, journal, step, constants::scanRangeInTiles,
                                   constants::scanDurationSeconds);
    }
    return completed;
}

// Flat grassland for chunks -2..2, for creature steering tests.
class GrassTerrain : public ChunkSource {
public:
    GrassTerrain() {
        for (int chunkY = -2; chunkY <= 2; ++chunkY) {
            for (int chunkX = -2; chunkX <= 2; ++chunkX) {
                auto chunk = std::make_unique<Chunk>();
                chunk->coord = {chunkX, chunkY};
                chunk->biomes.fill(Biome::Grassland);
                chunks_[chunk->coord] = std::move(chunk);
            }
        }
    }
    const Chunk* findChunk(ChunkCoord coord) const override {
        const auto found = chunks_.find(coord);
        return found == chunks_.end() ? nullptr : found->second.get();
    }

private:
    std::unordered_map<ChunkCoord, std::unique_ptr<Chunk>, ChunkCoordHash> chunks_;
};
}  // namespace

TEST_CASE("holding the scanner near an animal logs it after the scan time", "[scanner]") {
    Scanner scanner;
    Journal journal;
    const std::vector<Creature> creatures{calmCreature(Species::Deer, 2.0, 0.0)};

    REQUIRE_FALSE(holdFor(scanner, constants::scanDurationSeconds * 0.5, {0, 0}, creatures, journal));
    REQUIRE(scanner.status().hasTarget);
    REQUIRE(scanner.status().progress > 0.3);
    REQUIRE(scanner.status().progress < 0.7);

    REQUIRE(holdFor(scanner, constants::scanDurationSeconds, {0, 0}, creatures, journal));
    REQUIRE(scanner.completedSpecies() == Species::Deer);
}

TEST_CASE("animals out of range or already fleeing can't be scanned", "[scanner]") {
    Scanner scanner;
    Journal journal;

    const std::vector<Creature> far{calmCreature(Species::Deer, constants::scanRangeInTiles + 1.0, 0.0)};
    REQUIRE_FALSE(holdFor(scanner, constants::scanDurationSeconds * 2, {0, 0}, far, journal));
    REQUIRE_FALSE(scanner.status().hasTarget);

    std::vector<Creature> startled{calmCreature(Species::Deer, 1.0, 0.0)};
    startled[0].fleeing = true;
    REQUIRE_FALSE(holdFor(scanner, constants::scanDurationSeconds * 2, {0, 0}, startled, journal));
    REQUIRE_FALSE(scanner.status().hasTarget);
}

TEST_CASE("releasing the button drains progress and an already logged animal never completes", "[scanner]") {
    Scanner scanner;
    Journal journal;
    const std::vector<Creature> creatures{calmCreature(Species::Rabbit, 1.0, 0.0)};

    holdFor(scanner, constants::scanDurationSeconds * 0.5, {0, 0}, creatures, journal);
    const double before = scanner.status().progress;
    for (int index = 0; index < 30; ++index) {
        scanner.update(false, {0, 0}, creatures, journal, step, constants::scanRangeInTiles,
                       constants::scanDurationSeconds);
    }
    REQUIRE(scanner.status().progress < before);
    REQUIRE_FALSE(scanner.status().hasTarget);

    journal.recordSpecies(Species::Rabbit, 0, 0, 1);
    REQUIRE_FALSE(holdFor(scanner, constants::scanDurationSeconds * 3, {0, 0}, creatures, journal));
    REQUIRE(scanner.status().hasTarget);
    REQUIRE(scanner.status().alreadyLogged);
    REQUIRE(scanner.status().progress == 0.0);
}

TEST_CASE("a new animal is preferred over a closer one already logged", "[scanner]") {
    Scanner scanner;
    Journal journal;
    journal.recordSpecies(Species::Rabbit, 0, 0, 1);
    const std::vector<Creature> creatures{calmCreature(Species::Rabbit, 1.0, 0.0),
                                          calmCreature(Species::Fox, 3.0, 0.0)};
    REQUIRE(holdFor(scanner, constants::scanDurationSeconds * 2, {0, 0}, creatures, journal));
    REQUIRE(scanner.completedSpecies() == Species::Fox);
}

TEST_CASE("every species can be scanned from the edge of its sneak startle radius", "[scanner][stealth]") {
    const constants::SpeciesTuning tunings[] = {constants::deerTuning, constants::rabbitTuning, constants::crabTuning,
                                     constants::lizardTuning, constants::goatTuning, constants::foxTuning};
    for (const constants::SpeciesTuning& tuning : tunings) {
        REQUIRE(tuning.fleeRadius * constants::sneakStartleFraction < constants::scanRangeInTiles);
    }
}

TEST_CASE("sneaking up on an animal keeps it calm where walking would not", "[scanner][stealth]") {
    const GrassTerrain terrain;
    // Distance between the sneak and normal startle radius for a deer.
    const double distance = constants::deerTuning.fleeRadius * 0.8;

    Creature walkedAt = calmCreature(Species::Deer, distance, 0.0);
    stepCreature(walkedAt, terrain, {0, 0}, step, false);
    REQUIRE(walkedAt.fleeing);

    Creature sneakedAt = calmCreature(Species::Deer, distance, 0.0);
    stepCreature(sneakedAt, terrain, {0, 0}, step, true);
    REQUIRE_FALSE(sneakedAt.fleeing);

    // Closer than the sneak radius still startles it.
    Creature tooClose = calmCreature(Species::Deer, constants::deerTuning.fleeRadius * 0.3, 0.0);
    stepCreature(tooClose, terrain, {0, 0}, step, true);
    REQUIRE(tooClose.fleeing);
}

TEST_CASE("holding the action button slows the player", "[scanner][game]") {
    const auto distanceWalked = [](bool actionHeld) {
        Game game(11);
        game.advanceFrame(step);
        const Vec2 start = game.player().position();
        game.setActionHeld(actionHeld);
        game.setDirectionHeld(Direction::Right, true);
        for (int frame = 0; frame < 60; ++frame) game.advanceFrame(step);
        const Vec2 end = game.player().position();
        return std::abs(end.x - start.x) + std::abs(end.y - start.y);
    };
    const double normal = distanceWalked(false);
    const double sneaking = distanceWalked(true);
    REQUIRE(normal > 0.0);
    REQUIRE(sneaking < normal * 0.8);
}

TEST_CASE("releaseAllInput lets go of the scan button", "[scanner][game]") {
    Game game(11);
    game.setActionHeld(true);
    game.releaseAllInput();
    game.advanceFrame(step);
    REQUIRE_FALSE(game.scanner().status().hasTarget);
}
