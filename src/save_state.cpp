#include "save_state.h"

#include <cmath>
#include <iomanip>
#include <limits>
#include <sstream>

#include "constants.h"

namespace wl {

namespace {
constexpr const char* formatMagic = "WANDERLIGHT";
constexpr char biomeEntryTag = 'B';
constexpr char landmarkEntryTag = 'L';
constexpr char speciesEntryTag = 'S';

// Saves from before version 2 had no animals in the journal, and still load fine.
constexpr int oldestReadableVersion = 1;

// Reads "label value..." and checks the label matches.
bool expectLabel(std::istream& input, const char* label) {
    std::string word;
    return static_cast<bool>(input >> word) && word == label;
}

char entryTagFor(JournalCategory category) {
    switch (category) {
        case JournalCategory::Biome: return biomeEntryTag;
        case JournalCategory::Landmark: return landmarkEntryTag;
        case JournalCategory::Species: return speciesEntryTag;
    }
    return biomeEntryTag;
}

bool readCount(std::istream& input, const char* label, int& count) {
    return expectLabel(input, label) && static_cast<bool>(input >> count) && count >= 0 &&
           count <= constants::maxSavedEntries;
}
}  // namespace

std::string serializeSaveState(const SaveState& state) {
    std::ostringstream output;
    // max_digits10 (17 for double) is the minimum that always round-trips.
    output << std::setprecision(std::numeric_limits<double>::max_digits10);

    output << formatMagic << ' ' << constants::saveFormatVersion << '\n';
    output << "seed " << state.seed << '\n';
    output << "player " << state.playerTileX << ' ' << state.playerTileY << '\n';
    output << "time " << state.timeOfDay << ' ' << state.day << '\n';

    output << "chunks " << state.revealedChunks.size() << '\n';
    for (const ChunkCoord& coord : state.revealedChunks) output << coord.x << ' ' << coord.y << '\n';

    output << "journal " << state.journal.size() << '\n';
    for (const JournalEntry& entry : state.journal) {
        output << entryTagFor(entry.category) << ' ' << static_cast<int>(entry.id) << ' ' << entry.tileX << ' '
               << entry.tileY << ' ' << entry.day << '\n';
    }
    output << "end\n";
    return output.str();
}

std::optional<SaveState> parseSaveState(std::string_view text) {
    std::istringstream input{std::string(text)};
    SaveState state;

    int version = 0;
    if (!expectLabel(input, formatMagic) || !(input >> version) || version < oldestReadableVersion ||
        version > constants::saveFormatVersion) {
        return std::nullopt;
    }

    // Read as a wider type first, so a negative or huge value can't wrap silently.
    long long seedValue = 0;
    if (!expectLabel(input, "seed") || !(input >> seedValue) || seedValue < 0 ||
        seedValue > static_cast<long long>(std::numeric_limits<WorldSeed>::max())) {
        return std::nullopt;
    }
    state.seed = static_cast<WorldSeed>(seedValue);

    if (!expectLabel(input, "player") || !(input >> state.playerTileX >> state.playerTileY)) return std::nullopt;
    if (!std::isfinite(state.playerTileX) || !std::isfinite(state.playerTileY)) return std::nullopt;

    if (!expectLabel(input, "time") || !(input >> state.timeOfDay >> state.day)) return std::nullopt;
    if (!(state.timeOfDay >= 0.0 && state.timeOfDay < 1.0) || state.day < 1) return std::nullopt;

    int chunkCount = 0;
    if (!readCount(input, "chunks", chunkCount)) return std::nullopt;
    state.revealedChunks.reserve(chunkCount);
    for (int index = 0; index < chunkCount; ++index) {
        ChunkCoord coord;
        if (!(input >> coord.x >> coord.y)) return std::nullopt;
        state.revealedChunks.push_back(coord);
    }

    int journalCount = 0;
    if (!readCount(input, "journal", journalCount)) return std::nullopt;
    for (int index = 0; index < journalCount; ++index) {
        char tag = 0;
        int id = 0;
        JournalEntry entry;
        if (!(input >> tag >> id >> entry.tileX >> entry.tileY >> entry.day)) return std::nullopt;

        if (tag == biomeEntryTag) {
            if (id < 0 || id >= biomeCount) return std::nullopt;
            entry.category = JournalCategory::Biome;
        } else if (tag == landmarkEntryTag) {
            if (id < 0 || id >= propKindCount || !isJournalLandmark(static_cast<PropKind>(id))) return std::nullopt;
            entry.category = JournalCategory::Landmark;
        } else if (tag == speciesEntryTag) {
            if (id < 0 || id >= speciesCount) return std::nullopt;
            entry.category = JournalCategory::Species;
        } else {
            return std::nullopt;
        }
        entry.id = static_cast<std::uint8_t>(id);
        state.journal.push_back(entry);
    }

    // The end marker catches saves cut off partway through writing.
    if (!expectLabel(input, "end")) return std::nullopt;
    return state;
}

}  // namespace wl
