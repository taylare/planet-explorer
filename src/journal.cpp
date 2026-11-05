#include "journal.h"

#include <sstream>

namespace wl {

bool isJournalLandmark(PropKind kind) {
    return kind == PropKind::Ruins || kind == PropKind::Campfire || isRareLandmark(kind);
}

std::string journalEntryName(const JournalEntry& entry) {
    if (entry.category == JournalCategory::Biome) return std::string(biomeName(static_cast<Biome>(entry.id)));
    return std::string(propKindName(static_cast<PropKind>(entry.id)));
}

bool Journal::has(JournalCategory category, std::uint8_t id) const {
    for (const JournalEntry& entry : entries_) {
        if (entry.category == category && entry.id == id) return true;
    }
    return false;
}

bool Journal::hasBiome(Biome biome) const {
    return has(JournalCategory::Biome, static_cast<std::uint8_t>(biome));
}

bool Journal::hasLandmark(PropKind kind) const {
    return has(JournalCategory::Landmark, static_cast<std::uint8_t>(kind));
}

bool Journal::recordBiome(Biome biome, int tileX, int tileY, int day) {
    if (hasBiome(biome)) return false;
    entries_.push_back({JournalCategory::Biome, static_cast<std::uint8_t>(biome), tileX, tileY, day});
    return true;
}

bool Journal::recordLandmark(PropKind kind, int tileX, int tileY, int day) {
    if (!isJournalLandmark(kind) || hasLandmark(kind)) return false;
    entries_.push_back({JournalCategory::Landmark, static_cast<std::uint8_t>(kind), tileX, tileY, day});
    return true;
}

std::string Journal::toJson() const {
    // Names are fixed ASCII with no quotes or backslashes, so no escaping is needed.
    std::ostringstream json;
    json << '[';
    for (std::size_t index = 0; index < entries_.size(); ++index) {
        const JournalEntry& entry = entries_[index];
        if (index > 0) json << ',';
        json << "{\"type\":\"" << (entry.category == JournalCategory::Biome ? "biome" : "landmark")
             << "\",\"name\":\"" << journalEntryName(entry) << "\",\"x\":" << entry.tileX
             << ",\"y\":" << entry.tileY << ",\"day\":" << entry.day << '}';
    }
    json << ']';
    return json.str();
}

}  // namespace wl
