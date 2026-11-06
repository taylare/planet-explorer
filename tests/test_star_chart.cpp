#include <algorithm>
#include <catch2/catch_test_macros.hpp>
#include <set>
#include <string>

#include "constants.h"
#include "lore.h"
#include "game.h"
#include "planet.h"
#include "star_chart.h"

using namespace wl;

namespace {
constexpr double step = constants::simulationStepSeconds;

void runFrames(Game& game, int frameCount) {
    for (int frame = 0; frame < frameCount; ++frame) game.advanceFrame(step);
}

// A save with enough in the journal to be well past the launch threshold.
SaveState wellSurveyedState(const Game& game) {
    SaveState state = game.makeSaveState();
    for (int index = 1; index < biomeCount; ++index) {  // skip deep ocean, which is not on the list
        state.journal.push_back({JournalCategory::Biome, static_cast<std::uint8_t>(index), 0, 0, 1});
    }
    for (int index = 0; index < speciesCount; ++index) {
        state.journal.push_back({JournalCategory::Species, static_cast<std::uint8_t>(index), 0, 0, 1});
    }
    state.journal.push_back({JournalCategory::Landmark, static_cast<std::uint8_t>(PropKind::Tower), 0, 0, 1});
    state.journal.push_back({JournalCategory::Landmark, static_cast<std::uint8_t>(PropKind::Ruins), 0, 0, 1});
    return state;
}
}  // namespace

TEST_CASE("star chart destinations are stable, distinct and not the current planet", "[starchart]") {
    for (WorldSeed seed = 0; seed < 300; ++seed) {
        const std::array<WorldSeed, destinationCount> first = destinationSeeds(seed);
        REQUIRE(first == destinationSeeds(seed));

        std::set<WorldSeed> seeds(first.begin(), first.end());
        REQUIRE(seeds.size() == static_cast<std::size_t>(destinationCount));
        REQUIRE(seeds.count(seed) == 0);

        // The names on the chart should be distinguishable from each other and from home.
        std::set<std::string> names{makePlanetProfile(seed).name};
        for (const WorldSeed destination : first) names.insert(makePlanetProfile(destination).name);
        REQUIRE(names.size() == static_cast<std::size_t>(destinationCount) + 1);
    }
}

TEST_CASE("different planets have different neighbours", "[starchart]") {
    REQUIRE(destinationSeeds(1) != destinationSeeds(2));
}

TEST_CASE("launching needs the ship and a surveyed planet", "[starchart][game]") {
    Game game(3);
    runFrames(game, 2);
    REQUIRE(game.isNearShip());

    // Fresh planet: nothing surveyed, so no launch.
    WorldSeed destination = 0;
    REQUIRE_FALSE(game.canLaunch());
    REQUIRE_FALSE(game.launchDestination(0, destination));

    // Well surveyed and at the ship: launch is open for each valid destination.
    game.applySaveState(wellSurveyedState(game));
    runFrames(game, 2);
    REQUIRE(game.surveyPercent() >= constants::launchSurveyPercent);
    REQUIRE(game.canLaunch());
    for (int index = 0; index < destinationCount; ++index) {
        REQUIRE(game.launchDestination(index, destination));
        REQUIRE(destination == destinationSeeds(3)[index]);
    }
    REQUIRE_FALSE(game.launchDestination(-1, destination));
    REQUIRE_FALSE(game.launchDestination(destinationCount, destination));
}

TEST_CASE("a surveyed planet still can't launch from away from the ship", "[starchart][game]") {
    Game game(3);
    runFrames(game, 2);
    SaveState state = wellSurveyedState(game);
    // Far from the ship; the first frame after applying lets the game notice.
    state.playerTileX = game.shipPosition().x + 200.0;
    state.playerTileY = game.shipPosition().y + 200.0;
    game.applySaveState(state);
    runFrames(game, 2);
    REQUIRE_FALSE(game.isNearShip());
    REQUIRE(game.surveyPercent() >= constants::launchSurveyPercent);
    REQUIRE_FALSE(game.canLaunch());
}

TEST_CASE("the launch threshold is exactly the required percent", "[starchart][game]") {
    // Just under the line: one entry short of the percentage needed.
    Game game(3);
    runFrames(game, 2);
    SaveState state = game.makeSaveState();
    const int total = game.journal().survey().total;
    const int needed = (constants::launchSurveyPercent * total + 99) / 100;  // smallest count reaching the percent

    const auto withEntries = [&](int count) {
        SaveState candidate = state;
        int added = 0;
        for (int index = 1; index < biomeCount && added < count; ++index, ++added) {
            candidate.journal.push_back({JournalCategory::Biome, static_cast<std::uint8_t>(index), 0, 0, 1});
        }
        for (int index = 0; index < speciesCount && added < count; ++index, ++added) {
            candidate.journal.push_back({JournalCategory::Species, static_cast<std::uint8_t>(index), 0, 0, 1});
        }
        for (int index = 0; index < loreFragmentCount && added < count; ++index, ++added) {
            candidate.journal.push_back({JournalCategory::Lore, static_cast<std::uint8_t>(index), 0, 0, 1});
        }
        return candidate;
    };

    game.applySaveState(withEntries(needed - 1));
    runFrames(game, 2);
    REQUIRE_FALSE(game.canLaunch());

    game.applySaveState(withEntries(needed));
    runFrames(game, 2);
    REQUIRE(game.canLaunch());
}

TEST_CASE("a launch carries upgrades and samples to the new planet but leaves the journal behind", "[starchart][game]") {
    Game home(3);
    runFrames(home, 2);
    SaveState state = wellSurveyedState(home);
    state.samples = 12;
    state.upgrades.levels = {2, 1, 3};
    home.applySaveState(state);
    runFrames(home, 2);

    WorldSeed destination = 0;
    REQUIRE(home.launchDestination(1, destination));
    const LaunchCarryOver carry = home.carryOver();
    REQUIRE(carry.samples == 12);

    Game away(destination);
    runFrames(away, 2);
    away.applyCarryOver(carry);
    runFrames(away, 2);

    REQUIRE(away.seed() == destination);
    REQUIRE(away.samples() == 12);
    REQUIRE(away.upgrades().level(UpgradeKind::Scanner) == 2);
    REQUIRE(away.upgrades().level(UpgradeKind::PowerCell) == 3);
    REQUIRE(away.suitPower().capacity() == suitCapacityAt(3));
    REQUIRE(away.suitPower().fraction() > 0.99);
    // The map and journal start fresh on a new planet. (Standing in a biome may add one entry.)
    REQUIRE(away.surveyPercent() < constants::launchSurveyPercent);
    REQUIRE_FALSE(away.canLaunch());

    bool journalChanged = false;
    for (const GameEvent& event : away.takeEvents()) journalChanged = journalChanged || event.type == GameEventType::JournalChanged;
    REQUIRE(journalChanged);
}

TEST_CASE("ship panel JSON includes the star chart", "[starchart][game]") {
    Game game(3);
    runFrames(game, 2);
    std::string json = game.shipJson();
    REQUIRE(json.find("\"launch\":{\"unlocked\":false") != std::string::npos);
    REQUIRE(json.find("\"requiredPercent\":50") != std::string::npos);

    // One planet card per destination, each named as the planet will be called on arrival.
    for (const WorldSeed destination : destinationSeeds(3)) {
        REQUIRE(json.find("\"name\":\"" + makePlanetProfile(destination).name + "\"") != std::string::npos);
    }
    REQUIRE(std::count(json.begin(), json.end(), '{') == std::count(json.begin(), json.end(), '}'));
    REQUIRE(std::count(json.begin(), json.end(), '[') == std::count(json.begin(), json.end(), ']'));
    REQUIRE(std::count(json.begin(), json.end(), '"') % 2 == 0);

    game.applySaveState(wellSurveyedState(game));
    runFrames(game, 2);
    json = game.shipJson();
    REQUIRE(json.find("\"launch\":{\"unlocked\":true") != std::string::npos);
}
