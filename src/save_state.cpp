#include "save_state.h"

#include <cmath>
#include <iomanip>
#include <limits>
#include <sstream>

#include "constants.h"
#include "lore.h"

namespace wl {

namespace {
constexpr const char* formatMagic = "WANDERLIGHT";
constexpr char biomeEntryTag = 'B';
constexpr char landmarkEntryTag = 'L';
constexpr char speciesEntryTag = 'S';
constexpr char loreEntryTag = 'R';

// Older saves are missing later additions (animals in version 2, samples and used landmarks
// in version 3), and still load fine with those left empty.
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
        case JournalCategory::Lore: return loreEntryTag;
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
    output << "samples " << state.samples << '\n';
    output << "used " << state.usedLandmarks.size() << '\n';
    for (const UsedLandmark& landmark : state.usedLandmarks) output << landmark.tileX << ' ' << landmark.tileY << '\n';
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
        } else if (tag == loreEntryTag) {
            if (id < 0 || id >= loreFragmentCount) return std::nullopt;
            entry.category = JournalCategory::Lore;
        } else {
            return std::nullopt;
        }
        entry.id = static_cast<std::uint8_t>(id);
        state.journal.push_back(entry);
    }

    // Optional labelled sections, then the end marker, which catches saves cut off
    // partway through writing. Version 1 and 2 saves go straight to the marker.
    std::string section;
    while (input >> section) {
        if (section == "end") return state;
        if (section == "samples") {
            if (!(input >> state.samples) || state.samples < 0) return std::nullopt;
        } else if (section == "used") {
            int usedCount = 0;
            if (!(input >> usedCount) || usedCount < 0 || usedCount > constants::maxSavedEntries) return std::nullopt;
            for (int index = 0; index < usedCount; ++index) {
                UsedLandmark landmark;
                if (!(input >> landmark.tileX >> landmark.tileY)) return std::nullopt;
                state.usedLandmarks.push_back(landmark);
            }
        } else {
            return std::nullopt;
        }
    }
    return std::nullopt;
}

}  // namespace wl
