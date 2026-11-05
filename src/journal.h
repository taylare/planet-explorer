// Records the first time the player finds each biome and each landmark type.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "world.h"

namespace wl {

enum class JournalCategory : std::uint8_t { Biome = 0, Landmark = 1 };

struct JournalEntry {
    JournalCategory category = JournalCategory::Biome;
    std::uint8_t id = 0;  // Biome or PropKind value, depending on category
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

class Journal {
public:
    // Each returns true if this was a first discovery (and so was recorded).
    bool recordBiome(Biome biome, int tileX, int tileY, int day);
    bool recordLandmark(PropKind kind, int tileX, int tileY, int day);

    bool hasBiome(Biome biome) const;
    bool hasLandmark(PropKind kind) const;

    const std::vector<JournalEntry>& entries() const { return entries_; }
    void restore(const std::vector<JournalEntry>& entries) { entries_ = entries; }

    // For the HTML journal panel: [{"type":"biome","name":"Forest","x":1,"y":2,"day":1}, ...]
    std::string toJson() const;

private:
    bool has(JournalCategory category, std::uint8_t id) const;

    std::vector<JournalEntry> entries_;  // in discovery order
};

}  // namespace wl
