#include <catch2/catch_test_macros.hpp>
#include <string>

#include "game.h"
#include "save_state.h"

using namespace wl;

namespace {
SaveState sampleState() {
    SaveState state;
    state.seed = 4000000000u;  // above 2^31, to catch signed overflow
    state.playerTileX = -12345.678901234567;
    state.playerTileY = 0.1 + 0.2;  // a value that only round-trips with full precision
    state.timeOfDay = 0.987654321;
    state.day = 42;
    state.revealedChunks = {{-3, -3}, {0, 0}, {7, -1}, {-2147483, 2147483}};
    state.journal = {
        {JournalCategory::Biome, static_cast<std::uint8_t>(Biome::Forest), 10, -20, 1},
        {JournalCategory::Landmark, static_cast<std::uint8_t>(PropKind::CrystalGrove), -500, 600, 3},
        {JournalCategory::Species, static_cast<std::uint8_t>(Species::Fox), 33, 44, 5},
        {JournalCategory::Lore, 2, 55, 66, 6},
    };
    state.samples = 7;
    state.usedLandmarks = {{10, 10}, {-3, 4}};
    return state;
}

void runFrames(Game& game, int frameCount) {
    for (int frame = 0; frame < frameCount; ++frame) game.advanceFrame(constants::simulationStepSeconds);
}
}  // namespace

TEST_CASE("save state round-trips exactly", "[save]") {
    const SaveState original = sampleState();
    const std::optional<SaveState> restored = parseSaveState(serializeSaveState(original));
    REQUIRE(restored.has_value());
    REQUIRE(*restored == original);
}

TEST_CASE("a version 1 save from before animals still loads", "[save]") {
    SaveState oldState = sampleState();
    oldState.journal.resize(2);  // version 1 had only biome and landmark entries
    oldState.samples = 0;        // and no samples or used landmarks
    oldState.usedLandmarks.clear();
    std::string text = serializeSaveState(oldState);
    // Version 1 files end straight after the journal.
    const std::size_t samplesAt = text.find("samples ");
    text.erase(samplesAt, text.find("end\n") - samplesAt);
    const std::string currentHeader = "WANDERLIGHT " + std::to_string(constants::saveFormatVersion) + "\n";
    text.replace(0, currentHeader.size(), "WANDERLIGHT 1\n");

    const std::optional<SaveState> restored = parseSaveState(text);
    REQUIRE(restored.has_value());
    REQUIRE(*restored == oldState);
}

TEST_CASE("an empty state round-trips too", "[save]") {
    SaveState empty;
    empty.seed = 1;
    const std::optional<SaveState> restored = parseSaveState(serializeSaveState(empty));
    REQUIRE(restored.has_value());
    REQUIRE(*restored == empty);
}

TEST_CASE("bad saves are rejected rather than half loaded", "[save]") {
    const std::string good = serializeSaveState(sampleState());

    REQUIRE_FALSE(parseSaveState("").has_value());
    REQUIRE_FALSE(parseSaveState("not a save at all").has_value());

    // Truncated anywhere: always missing the end marker at least.
    for (std::size_t length = 0; length + 1 < good.size(); length += 7) {
        REQUIRE_FALSE(parseSaveState(good.substr(0, length)).has_value());
    }

    // The version number sits on the first line, right after the magic word.
    const std::string currentHeader = "WANDERLIGHT " + std::to_string(constants::saveFormatVersion) + "\n";
    REQUIRE(good.compare(0, currentHeader.size(), currentHeader) == 0);

    std::string wrongVersion = good;
    wrongVersion.replace(0, currentHeader.size(), "WANDERLIGHT 99\n");
    REQUIRE_FALSE(parseSaveState(wrongVersion).has_value());

    std::string tooOld = good;
    tooOld.replace(0, currentHeader.size(), "WANDERLIGHT 0\n");
    REQUIRE_FALSE(parseSaveState(tooOld).has_value());

    std::string badLore = good;
    badLore.replace(badLore.find("R 2 "), 4, "R 99 ");
    REQUIRE_FALSE(parseSaveState(badLore).has_value());

    std::string unknownSection = good;
    unknownSection.replace(unknownSection.find("samples "), 8, "mystery ");
    REQUIRE_FALSE(parseSaveState(unknownSection).has_value());

    std::string badSpecies = good;
    badSpecies.replace(badSpecies.find("S 5 "), 4, "S 99 ");
    REQUIRE_FALSE(parseSaveState(badSpecies).has_value());

    std::string badBiome = good;
    badBiome.replace(badBiome.find("B 4 "), 4, "B 99 ");
    REQUIRE_FALSE(parseSaveState(badBiome).has_value());

    std::string badTime = good;
    badTime.replace(badTime.find("time "), 5, "time 7");
    REQUIRE_FALSE(parseSaveState(badTime).has_value());
}

TEST_CASE("the full game state survives a save and load", "[save][game]") {
    Game original(2024);
    original.setDirectionHeld(Direction::Left, true);
    original.setDirectionHeld(Direction::Up, true);
    runFrames(original, 600);  // ten seconds of walking and time passing
    original.releaseAllInput();
    runFrames(original, 30);

    const SaveState saved = original.makeSaveState();
    REQUIRE(saved.revealedChunks.size() >= 9);
    REQUIRE_FALSE(saved.journal.empty());  // the starting biome gets recorded

    const std::optional<SaveState> parsed = parseSaveState(serializeSaveState(saved));
    REQUIRE(parsed.has_value());

    Game restored(2024);
    REQUIRE(restored.applySaveState(*parsed));
    REQUIRE(restored.makeSaveState() == saved);
    REQUIRE(restored.player().position().x == original.player().position().x);
    REQUIRE(restored.player().position().y == original.player().position().y);
    REQUIRE(restored.dayNight().timeOfDay() == original.dayNight().timeOfDay());
    REQUIRE(restored.journal().entries() == original.journal().entries());
}

TEST_CASE("a save from another world is refused", "[save][game]") {
    Game game(5);
    SaveState other = game.makeSaveState();
    other.seed = 6;
    REQUIRE_FALSE(game.applySaveState(other));
}
