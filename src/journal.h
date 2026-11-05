// Records the first time the player finds each biome, each landmark type and
// each kind of animal (by scanning it), and tracks how much of the planet is surveyed.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "entities.h"
#include "world.h"

namespace wl {

enum class JournalCategory : std::uint8_t { Biome = 0, Landmark = 1, Species = 2 };

struct JournalEntry {
    JournalCategory category = JournalCategory::Biome;
    std::uint8_t id = 0;  // Biome, PropKind or Species value, depending on category
    int tileX = 0;
    int tileY = 0;
    int day = 1;

    bool operator==(const JournalEntry& other) const {
        return category == other.category && id == other.id && tileX == other.tileX && tileY == other.tileY &&
               day == other.day;
    }
};

// Which prop kinds count as landmarks worth a journal entry.
bool isJournalLandmark(PropKind kind);

std::string journalEntryName(const JournalEntry& entry);

// How much of the planet's survey list has been ticked off.
struct SurveyProgress {
    int found = 0;
    int total = 0;
};

class Journal {
public:
    // Each returns true if this was a first discovery (and so was recorded).
    bool recordBiome(Biome biome, int tileX, int tileY, int day);
    bool recordLandmark(PropKind kind, int tileX, int tileY, int day);
    bool recordSpecies(Species species, int tileX, int tileY, int day);

    bool hasBiome(Biome biome) const;
    bool hasLandmark(PropKind kind) const;
    bool hasSpecies(Species species) const;

    // The survey list is every biome you can walk in, every journal landmark and every species.
    // Deep ocean is left off because the player can never stand in it.
    SurveyProgress survey() const;
    int surveyPercent() const;  // 0 to 100, rounded down so 100 means truly everything

    const std::vector<JournalEntry>& entries() const { return entries_; }
    void restore(const std::vector<JournalEntry>& entries) { entries_ = entries; }

    // For the HTML journal panel: [{"type":"biome","name":"Forest","x":1,"y":2,"day":1}, ...]
    std::string toJson() const;

private:
    bool has(JournalCategory category, std::uint8_t id) const;

    std::vector<JournalEntry> entries_;  // in discovery order
};

}  // namespace wl
