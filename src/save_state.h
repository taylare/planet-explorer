// Everything that survives a reload, and its text format for localStorage.
//
// Only what the player changed is saved: the seed, where they are, the
// clock, which chunks they've seen and the journal. The world itself is
// regenerated from the seed.
#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "journal.h"
#include "world.h"

namespace wl {

struct SaveState {
    WorldSeed seed = 0;
    double playerTileX = 0.0;
    double playerTileY = 0.0;
    double timeOfDay = 0.0;
    int day = 1;
    std::vector<ChunkCoord> revealedChunks;
    std::vector<JournalEntry> journal;

    bool operator==(const SaveState& other) const {
        return seed == other.seed && playerTileX == other.playerTileX && playerTileY == other.playerTileY &&
               timeOfDay == other.timeOfDay && day == other.day && revealedChunks == other.revealedChunks &&
               journal == other.journal;
    }
};

// Plain text, one field per line, with a version header. Easy to read in
// the browser's dev tools, and doubles are written with enough digits to
// read back exactly.
std::string serializeSaveState(const SaveState& state);

// Empty if the text is missing, from another version, truncated or out of range.
std::optional<SaveState> parseSaveState(std::string_view text);

}  // namespace wl
