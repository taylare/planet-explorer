#include <algorithm>
#include <catch2/catch_test_macros.hpp>
#include <string>

#include "constants.h"
#include "game.h"
#include "landmark_actions.h"
#include "lore.h"

using namespace wl;

namespace {
constexpr double step = constants::simulationStepSeconds;

// Finds the nearest loaded prop of a kind to the player, if any.
const Prop* findLoadedProp(const Game& game, PropKind kind) {
    const Prop* nearest = nullptr;
    double nearestDistance = 0.0;
    const Vec2 player = game.player().position();
    const ChunkRegion region = game.requiredChunkRegion();
    for (int chunkY = region.minChunkY; chunkY <= region.maxChunkY; ++chunkY) {
        for (int chunkX = region.minChunkX; chunkX <= region.maxChunkX; ++chunkX) {
            const Chunk* chunk = game.chunkCache().findChunk({chunkX, chunkY});
            if (!chunk) continue;
            for (const Prop& prop : chunk->props) {
                if (prop.kind != kind) continue;
                const double distance = std::abs(prop.tileX - player.x) + std::abs(prop.tileY - player.y);
                if (!nearest || distance < nearestDistance) {
                    nearest = &prop;
                    nearestDistance = distance;
                }
            }
        }
    }
    return nearest;
}

// Looks through seeds for a world with a loaded prop of this kind near the start.
// Returns the seed, or -1 if none turned up.
long long seedWithNearbyProp(PropKind kind) {
    for (int seed = 1; seed < 400; ++seed) {
        Game game(static_cast<WorldSeed>(seed));
        game.advanceFrame(step);
        if (findLoadedProp(game, kind)) return seed;
    }
    return -1;
}
}  // namespace

TEST_CASE("scan results follow the landmark rules", "[landmarks]") {
    REQUIRE(scanLandmarkResult(PropKind::Tower, 0).effect == LandmarkEffect::BeaconLit);

    const LandmarkResult crystals = scanLandmarkResult(PropKind::CrystalGrove, 0);
    REQUIRE(crystals.effect == LandmarkEffect::SamplesGained);
    REQUIRE(crystals.samples == constants::crystalGroveSampleYield);

    // Ruins decode the next fragment, in order.
    for (int decoded = 0; decoded < loreFragmentCount; ++decoded) {
        const LandmarkResult ruins = scanLandmarkResult(PropKind::Ruins, decoded);
        REQUIRE(ruins.effect == LandmarkEffect::LoreDecoded);
        REQUIRE(ruins.loreIndex == decoded);
    }
    REQUIRE(scanLandmarkResult(PropKind::Ruins, loreFragmentCount).effect == LandmarkEffect::NothingNew);

    // Resting places and scenery do nothing when scanned.
    REQUIRE(scanLandmarkResult(PropKind::Campfire, 0).effect == LandmarkEffect::None);
    REQUIRE(scanLandmarkResult(PropKind::HotSpring, 0).effect == LandmarkEffect::None);
    REQUIRE(scanLandmarkResult(PropKind::Rock, 0).effect == LandmarkEffect::None);
}

TEST_CASE("only towers, crystal groves and ruins are scannable", "[landmarks]") {
    for (int index = 0; index < propKindCount; ++index) {
        const PropKind kind = static_cast<PropKind>(index);
        const bool expected = kind == PropKind::Tower || kind == PropKind::CrystalGrove || kind == PropKind::Ruins;
        REQUIRE(isScannableLandmark(kind) == expected);
    }
}

TEST_CASE("notice text names what the scan did", "[landmarks]") {
    REQUIRE(landmarkNoticeText(PropKind::CrystalGrove, scanLandmarkResult(PropKind::CrystalGrove, 0)) ==
            "Crystal Grove: +3 samples");
    REQUIRE(landmarkNoticeText(PropKind::Ruins, scanLandmarkResult(PropKind::Ruins, 0)) == "First Entry decoded");
    REQUIRE_FALSE(landmarkNoticeText(PropKind::Tower, scanLandmarkResult(PropKind::Tower, 0)).empty());
}

TEST_CASE("every lore fragment has a title and text that is safe for JSON", "[lore]") {
    for (int index = 0; index < loreFragmentCount; ++index) {
        REQUIRE_FALSE(loreTitle(index).empty());
        REQUIRE(loreText(index).size() > 40);
        for (const std::string_view field : {loreTitle(index), loreText(index)}) {
            for (const char letter : field) {
                REQUIRE(letter >= 32);
                REQUIRE(letter < 127);
                REQUIRE(letter != '"');
                REQUIRE(letter != '\\');
            }
        }
    }
    REQUIRE(loreTitle(-1).empty());
    REQUIRE(loreText(loreFragmentCount).empty());
}

TEST_CASE("lore is recorded in order once each and shows up in the journal JSON", "[lore][journal]") {
    Journal journal;
    REQUIRE(journal.loreCount() == 0);
    REQUIRE(journal.recordLore(0, 1, 2, 1));
    REQUIRE_FALSE(journal.recordLore(0, 9, 9, 2));
    REQUIRE_FALSE(journal.recordLore(loreFragmentCount, 0, 0, 1));
    REQUIRE_FALSE(journal.recordLore(-1, 0, 0, 1));
    REQUIRE(journal.loreCount() == 1);
    REQUIRE(journal.hasLore(0));
    REQUIRE_FALSE(journal.hasLore(1));

    const std::string json = journal.toJson();
    REQUIRE(json.find("\"type\":\"lore\"") != std::string::npos);
    REQUIRE(json.find(std::string("\"text\":\"") + std::string(loreText(0)) + "\"") != std::string::npos);
}

TEST_CASE("scanning a crystal grove gives samples once", "[landmarks][game]") {
    const long long seed = seedWithNearbyProp(PropKind::CrystalGrove);
    if (seed < 0) SKIP("no crystal grove near the start of any tested seed");

    Game game(static_cast<WorldSeed>(seed));
    game.advanceFrame(step);
    const Prop* grove = findLoadedProp(game, PropKind::CrystalGrove);
    REQUIRE(grove != nullptr);

    // Stand next to it, hold the scan button, and wait out the scan time.
    // Keep the position steady: nothing is pressing a direction.
    game.applySaveState([&] {
        SaveState state = game.makeSaveState();
        state.playerTileX = grove->tileX + 1.0;
        state.playerTileY = grove->tileY;
        return state;
    }());
    game.advanceFrame(step);
    game.setActionHeld(true);
    const int frames = static_cast<int>(constants::landmarkScanDurationSeconds / step) + 10;
    for (int frame = 0; frame < frames; ++frame) game.advanceFrame(step);

    REQUIRE(game.samples() == constants::crystalGroveSampleYield);
    REQUIRE(game.usedLandmarks().size() == 1);

    bool sawNotice = false;
    for (const GameEvent& event : game.takeEvents()) sawNotice = sawNotice || event.type == GameEventType::Notice;
    REQUIRE(sawNotice);

    // Holding on does not farm the same grove.
    for (int frame = 0; frame < frames * 2; ++frame) game.advanceFrame(step);
    REQUIRE(game.samples() == constants::crystalGroveSampleYield);
    REQUIRE(game.scanner().status().alreadyLogged);
}

TEST_CASE("scanning ruins decodes the first story fragment", "[landmarks][game]") {
    const long long seed = seedWithNearbyProp(PropKind::Ruins);
    if (seed < 0) SKIP("no ruins near the start of any tested seed");

    Game game(static_cast<WorldSeed>(seed));
    game.advanceFrame(step);
    const Prop* ruins = findLoadedProp(game, PropKind::Ruins);
    REQUIRE(ruins != nullptr);

    game.applySaveState([&] {
        SaveState state = game.makeSaveState();
        state.playerTileX = ruins->tileX + 1.0;
        state.playerTileY = ruins->tileY;
        return state;
    }());
    game.advanceFrame(step);
    game.setActionHeld(true);
    const int frames = static_cast<int>(constants::landmarkScanDurationSeconds / step) + 10;
    for (int frame = 0; frame < frames; ++frame) game.advanceFrame(step);

    REQUIRE(game.journal().hasLore(0));
    REQUIRE(game.journal().loreCount() == 1);
}

TEST_CASE("a lit beacon reveals a wide area of the map", "[landmarks][game]") {
    const long long seed = seedWithNearbyProp(PropKind::Tower);
    if (seed < 0) SKIP("no tower near the start of any tested seed");

    Game game(static_cast<WorldSeed>(seed));
    game.advanceFrame(step);
    const Prop* tower = findLoadedProp(game, PropKind::Tower);
    REQUIRE(tower != nullptr);

    game.applySaveState([&] {
        SaveState state = game.makeSaveState();
        state.playerTileX = tower->tileX + 1.0;
        state.playerTileY = tower->tileY;
        return state;
    }());
    game.advanceFrame(step);
    const std::size_t revealedBefore = game.exploration().revealedCount();

    game.setActionHeld(true);
    const int frames = static_cast<int>(constants::landmarkScanDurationSeconds / step) + 10;
    for (int frame = 0; frame < frames; ++frame) game.advanceFrame(step);

    const int side = 2 * constants::beaconRevealRadiusInChunks + 1;
    REQUIRE(game.exploration().revealedCount() > revealedBefore);
    REQUIRE(game.exploration().revealedCount() >= static_cast<std::size_t>(side * side));
}

TEST_CASE("samples and used landmarks survive a save and load", "[landmarks][save]") {
    SaveState state;
    state.seed = 77;
    state.samples = 9;
    state.usedLandmarks = {{12, -30}, {-4, 8}};
    const std::optional<SaveState> restored = parseSaveState(serializeSaveState(state));
    REQUIRE(restored.has_value());
    REQUIRE(*restored == state);

    Game game(77);
    REQUIRE(game.applySaveState(state));
    REQUIRE(game.samples() == 9);
    REQUIRE(game.usedLandmarks().size() == 2);
    REQUIRE(game.makeSaveState().samples == 9);
}

TEST_CASE("a version 2 save from before samples still loads", "[landmarks][save]") {
    SaveState oldState;
    oldState.seed = 5;
    std::string text = serializeSaveState(oldState);
    const std::string currentHeader = "WANDERLIGHT " + std::to_string(constants::saveFormatVersion) + "\n";
    REQUIRE(text.compare(0, currentHeader.size(), currentHeader) == 0);
    text.replace(0, currentHeader.size(), "WANDERLIGHT 2\n");
    // Version 2 saves had no samples or used sections.
    const std::size_t samplesAt = text.find("samples ");
    text.erase(samplesAt, text.find("end\n") - samplesAt);

    const std::optional<SaveState> restored = parseSaveState(text);
    REQUIRE(restored.has_value());
    REQUIRE(restored->samples == 0);
    REQUIRE(restored->usedLandmarks.empty());
}
