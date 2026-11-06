#include <algorithm>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include <string>

#include "constants.h"
#include "game.h"
#include "props.h"
#include "renderer.h"

using namespace wl;

namespace {
constexpr double step = constants::simulationStepSeconds;

void runFrames(Game& game, int frameCount) {
    for (int frame = 0; frame < frameCount; ++frame) game.advanceFrame(step);
}

// Moves the player to a spot and lets the game notice.
void teleport(Game& game, double x, double y) {
    SaveState state = game.makeSaveState();
    state.playerTileX = x;
    state.playerTileY = y;
    game.applySaveState(state);
    game.advanceFrame(step);
}

Vec2 openGroundAwayFromShip(const World& world);

void walkAwayFromShip(Game& game) {
    const Vec2 away = openGroundAwayFromShip(game.world());
    teleport(game, away.x, away.y);
}

// A 13 by 13 patch of dry land at least 30 tiles from the ship, so a second of walking stays on
// land and no recharge source is in reach. Searches square rings outward from the ship.
// Returns the centre of the patch.
Vec2 openGroundAwayFromShip(const World& world) {
    double shipX = 0.0;
    double shipY = 0.0;
    world.shipFoot(shipX, shipY);
    constexpr int patchHalfSize = 6;
    constexpr int searchStep = 4;
    const auto isPatchDry = [&world](int centreX, int centreY) {
        for (int dy = -patchHalfSize; dy <= patchHalfSize; ++dy) {
            for (int dx = -patchHalfSize; dx <= patchHalfSize; ++dx) {
                const Biome biome = world.sampleTerrain(centreX + dx, centreY + dy).biome;
                if (biome == Biome::DeepOcean || biome == Biome::ShallowWater || biome == Biome::Beach) return false;
            }
        }
        return true;
    };
    for (int radius = 30; radius < 900; radius += searchStep) {
        for (int offset = -radius; offset <= radius; offset += searchStep) {
            const int candidates[4][2] = {{offset, -radius}, {offset, radius}, {-radius, offset}, {radius, offset}};
            for (const auto& candidate : candidates) {
                const int centreX = static_cast<int>(shipX) + candidate[0];
                const int centreY = static_cast<int>(shipY) + candidate[1];
                if (isPatchDry(centreX, centreY)) return {centreX + 0.5, centreY + 0.5};
            }
        }
    }
    return {shipX + 40.0, shipY};
}

struct DistanceToShip {
    static double of(const Game& game) {
        const Vec2 ship = game.shipPosition();
        const Vec2 player = game.player().position();
        return std::hypot(ship.x - player.x, ship.y - player.y);
    }
};
}  // namespace

TEST_CASE("the ship site is a pure function of the seed and sits near the spawn", "[ship]") {
    for (int seed = 0; seed < 60; ++seed) {
        const World first(static_cast<WorldSeed>(seed));
        const World second(static_cast<WorldSeed>(seed));
        double firstX = 0, firstY = 0, secondX = 0, secondY = 0;
        first.shipFoot(firstX, firstY);
        second.shipFoot(secondX, secondY);
        REQUIRE(firstX == secondX);
        REQUIRE(firstY == secondY);

        int spawnX = 0, spawnY = 0;
        first.spawnTile(spawnX, spawnY);
        REQUIRE(std::abs(firstX - (spawnX + 0.5)) <= constants::shipSideOffsetInTiles);
        REQUIRE(firstY == Catch::Approx(spawnY + 0.5));

        // The cached answer matches the plain search.
        int searchedX = 0, searchedY = 0;
        first.findSpawnTile(searchedX, searchedY);
        REQUIRE(spawnX == searchedX);
        REQUIRE(spawnY == searchedY);

        // The ship never stands in water.
        const Biome ground =
            first.sampleTerrain(static_cast<int>(std::floor(firstX)), static_cast<int>(std::floor(firstY))).biome;
        REQUIRE(ground != Biome::DeepOcean);
        REQUIRE(ground != Biome::ShallowWater);
    }
}

TEST_CASE("props and landmarks keep clear of the ship", "[ship][props]") {
    for (int seed = 0; seed < 40; ++seed) {
        const World world(static_cast<WorldSeed>(seed));
        double shipX = 0, shipY = 0;
        world.shipFoot(shipX, shipY);
        const ChunkCoord centre = chunkCoordForTile(static_cast<int>(std::floor(shipX)), static_cast<int>(std::floor(shipY)));

        for (int chunkY = centre.y - 1; chunkY <= centre.y + 1; ++chunkY) {
            for (int chunkX = centre.x - 1; chunkX <= centre.x + 1; ++chunkX) {
                const std::unique_ptr<Chunk> chunk = world.generateChunk({chunkX, chunkY});
                for (const Prop& prop : chunk->props) {
                    const double distance = std::hypot(prop.tileX - shipX, prop.tileY - shipY);
                    REQUIRE(distance >= constants::shipClearingRadiusInTiles);
                }
            }
        }
    }
}

TEST_CASE("the player starts within reach of the ship with a full suit", "[ship][game]") {
    Game game(3);
    runFrames(game, 2);
    REQUIRE(DistanceToShip::of(game) <= constants::shipInteractRadiusInTiles);
    REQUIRE(game.isNearShip());
    REQUIRE(game.suitPower().fraction() == Catch::Approx(1.0));
}

TEST_CASE("exploring drains the suit and the ship refills it", "[ship][game]") {
    Game game(3);
    walkAwayFromShip(game);
    REQUIRE_FALSE(game.isNearShip());

    runFrames(game, 60 * 30);
    REQUIRE(game.suitPower().fraction() < 0.95);
    const double drained = game.suitPower().charge();

    // Back at the ship it comes right back up.
    const Vec2 ship = game.shipPosition();
    teleport(game, ship.x - 1.0, ship.y);
    SaveState state = game.makeSaveState();
    state.suitFraction = drained / game.suitPower().capacity();
    game.applySaveState(state);
    runFrames(game, 60 * 10);
    REQUIRE(game.isNearShip());
    REQUIRE(game.suitPower().charge() > drained);
}

TEST_CASE("an empty suit slows the player down", "[ship][game]") {
    const auto distanceWalked = [](double suitFraction) {
        Game game(3);
        SaveState state = game.makeSaveState();
        state.suitFraction = suitFraction;
        // Open ground away from the ship so the empty suit stays empty. Applied before the first
        // frame, so the whole area around the player is loaded straight away.
        const Vec2 start0 = openGroundAwayFromShip(game.world());
        state.playerTileX = start0.x;
        state.playerTileY = start0.y;
        game.applySaveState(state);
        runFrames(game, 4);
        const Vec2 start = game.player().position();
        game.setDirectionHeld(Direction::Right, true);
        runFrames(game, 60);
        const Vec2 end = game.player().position();
        return std::hypot(end.x - start.x, end.y - start.y);
    };
    const double full = distanceWalked(1.0);
    const double empty = distanceWalked(0.0);
    REQUIRE(full > 0.5);
    REQUIRE(empty < full * 0.8);
}

TEST_CASE("beaming to the ship only works with an empty suit", "[ship][game]") {
    Game game(3);
    walkAwayFromShip(game);
    REQUIRE_FALSE(game.beamToShip());  // charged: refused

    SaveState state = game.makeSaveState();
    state.suitFraction = 0.0;
    game.applySaveState(state);
    runFrames(game, 2);
    REQUIRE(game.suitPower().isEmpty());
    REQUIRE(game.beamToShip());

    REQUIRE(DistanceToShip::of(game) <= constants::shipInteractRadiusInTiles);
    REQUIRE(game.suitPower().fraction() >= constants::beamRechargeFraction - 1e-9);
    REQUIRE_FALSE(game.suitPower().isEmpty());
    // And the same suit can't be beamed again right away.
    REQUIRE_FALSE(game.beamToShip());
}

TEST_CASE("running flat raises a notice once", "[ship][game]") {
    Game game(3);
    walkAwayFromShip(game);
    SaveState state = game.makeSaveState();
    state.suitFraction = 0.001;  // a few steps from empty
    const Vec2 away = openGroundAwayFromShip(game.world());
    state.playerTileX = away.x;
    state.playerTileY = away.y;
    game.applySaveState(state);
    game.takeEvents();

    runFrames(game, 60 * 5);
    int emptyNotices = 0;
    for (const GameEvent& event : game.takeEvents()) {
        if (event.type == GameEventType::Notice && event.text.find("empty") != std::string::npos) ++emptyNotices;
    }
    REQUIRE(emptyNotices == 1);
}

TEST_CASE("upgrades need the ship, enough samples and a level to gain", "[ship][upgrades][game]") {
    Game game(3);
    runFrames(game, 2);
    SaveState state = game.makeSaveState();
    state.samples = 5;
    game.applySaveState(state);
    runFrames(game, 2);
    REQUIRE(game.isNearShip());

    // Not enough for the 6 sample second level, but fine for the first.
    REQUIRE(game.buyUpgrade(UpgradeKind::Scanner));
    REQUIRE(game.samples() == 2);
    REQUIRE(game.upgrades().level(UpgradeKind::Scanner) == 1);
    REQUIRE_FALSE(game.buyUpgrade(UpgradeKind::Scanner));
    REQUIRE(game.samples() == 2);
    REQUIRE(game.scanRangeInTiles() > constants::scanRangeInTiles);

    // Away from the ship nothing can be bought, however many samples there are.
    state = game.makeSaveState();
    state.samples = 100;
    const Vec2 away = openGroundAwayFromShip(game.world());
    state.playerTileX = away.x;
    state.playerTileY = away.y;
    game.applySaveState(state);
    runFrames(game, 2);
    REQUIRE_FALSE(game.isNearShip());
    REQUIRE_FALSE(game.buyUpgrade(UpgradeKind::Headlamp));
    REQUIRE(game.samples() == 100);
}

TEST_CASE("a maxed upgrade can't be bought again", "[ship][upgrades][game]") {
    Game game(3);
    runFrames(game, 2);
    SaveState state = game.makeSaveState();
    state.samples = 100;
    game.applySaveState(state);
    runFrames(game, 2);

    for (int level = 0; level < constants::maxUpgradeLevel; ++level) REQUIRE(game.buyUpgrade(UpgradeKind::Headlamp));
    REQUIRE_FALSE(game.buyUpgrade(UpgradeKind::Headlamp));
    REQUIRE(game.samples() == 100 - (3 + 6 + 10));
}

TEST_CASE("a power cell upgrade makes a bigger battery without a free refill", "[ship][upgrades][game]") {
    Game game(3);
    runFrames(game, 2);
    SaveState state = game.makeSaveState();
    state.samples = 10;
    state.suitFraction = 0.5;
    game.applySaveState(state);
    const double chargeBefore = game.suitPower().charge();
    const double capacityBefore = game.suitPower().capacity();

    // Buying happens at the ship, where the suit is recharging, so check right after the purchase.
    REQUIRE(game.buyUpgrade(UpgradeKind::PowerCell));
    REQUIRE(game.suitPower().capacity() > capacityBefore);
    REQUIRE(game.suitPower().charge() == Catch::Approx(chargeBefore));
}

TEST_CASE("ship panel JSON describes samples, power and each upgrade", "[ship][game]") {
    Game game(3);
    runFrames(game, 2);
    const std::string json = game.shipJson();
    REQUIRE(json.front() == '{');
    REQUIRE(json.back() == '}');
    REQUIRE(json.find("\"nearShip\":true") != std::string::npos);
    REQUIRE(json.find("\"name\":\"Scanner\"") != std::string::npos);
    REQUIRE(json.find("\"name\":\"Headlamp\"") != std::string::npos);
    REQUIRE(json.find("\"name\":\"Power Cell\"") != std::string::npos);
    REQUIRE(json.find("\"canBuy\":false") != std::string::npos);  // no samples yet
    REQUIRE(std::count(json.begin(), json.end(), '"') % 2 == 0);
    REQUIRE(std::count(json.begin(), json.end(), '{') == std::count(json.begin(), json.end(), '}'));
    REQUIRE(std::count(json.begin(), json.end(), '[') == std::count(json.begin(), json.end(), ']'));
}

TEST_CASE("upgrades and suit power survive a save and load", "[ship][save]") {
    SaveState state;
    state.seed = 12;
    state.upgrades.levels = {1, 3, 2};
    state.suitFraction = 0.375;
    const std::optional<SaveState> restored = parseSaveState(serializeSaveState(state));
    REQUIRE(restored.has_value());
    REQUIRE(*restored == state);

    Game game(12);
    REQUIRE(game.applySaveState(state));
    REQUIRE(game.upgrades().level(UpgradeKind::PowerCell) == 2);
    REQUIRE(game.suitPower().capacity() == Catch::Approx(suitCapacityAt(2)));
    REQUIRE(game.suitPower().fraction() == Catch::Approx(0.375));
}

TEST_CASE("bad upgrade or power values are rejected", "[ship][save]") {
    const std::string good = serializeSaveState(SaveState{});

    std::string badLevel = good;
    badLevel.replace(badLevel.find("upgrades 0 0 0"), 14, "upgrades 0 9 0");
    REQUIRE_FALSE(parseSaveState(badLevel).has_value());

    std::string badPower = good;
    badPower.replace(badPower.find("power 1"), 7, "power 2");
    REQUIRE_FALSE(parseSaveState(badPower).has_value());
}

TEST_CASE("a version 3 save from before the ship still loads with a full suit", "[ship][save]") {
    std::string text = serializeSaveState(SaveState{});
    const std::string currentHeader = "WANDERLIGHT " + std::to_string(constants::saveFormatVersion) + "\n";
    text.replace(0, currentHeader.size(), "WANDERLIGHT 3\n");
    const std::size_t upgradesAt = text.find("upgrades ");
    text.erase(upgradesAt, text.find("end\n") - upgradesAt);

    const std::optional<SaveState> restored = parseSaveState(text);
    REQUIRE(restored.has_value());
    REQUIRE(restored->suitFraction == 1.0);
    REQUIRE(restored->upgrades == Upgrades{});
}
