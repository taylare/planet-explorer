#include "journal.h"

#include <sstream>

namespace wl {

namespace {
const char* categoryLabel(JournalCategory category) {
    switch (category) {
        case JournalCategory::Biome: return "biome";
        case JournalCategory::Landmark: return "landmark";
        case JournalCategory::Species: return "species";
    }
    return "biome";
}

int landmarkKindCount() {
    int count = 0;
    for (int index = 0; index < propKindCount; ++index) {
        if (isJournalLandmark(static_cast<PropKind>(index))) ++count;
    }
    return count;
}
}  // namespace

bool isJournalLandmark(PropKind kind) {
    return kind == PropKind::Ruins || kind == PropKind::Campfire || isRareLandmark(kind);
}

std::string journalEntryName(const JournalEntry& entry) {
    switch (entry.category) {
        case JournalCategory::Biome: return std::string(biomeName(static_cast<Biome>(entry.id)));
        case JournalCategory::Landmark: return std::string(propKindName(static_cast<PropKind>(entry.id)));
        case JournalCategory::Species: return std::string(speciesName(static_cast<Species>(entry.id)));
    }
    return "Unknown";
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

bool Journal::hasSpecies(Species species) const {
    return has(JournalCategory::Species, static_cast<std::uint8_t>(species));
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

bool Journal::recordSpecies(Species species, int tileX, int tileY, int day) {
    if (species == Species::Count || hasSpecies(species)) return false;
    entries_.push_back({JournalCategory::Species, static_cast<std::uint8_t>(species), tileX, tileY, day});
    return true;
}

SurveyProgress Journal::survey() const {
    SurveyProgress progress;
    // Deep ocean can't be entered, so it isn't on the list.
    progress.total = (biomeCount - 1) + landmarkKindCount() + speciesCount;
    for (const JournalEntry& entry : entries_) {
        const bool isUnreachableBiome =
            entry.category == JournalCategory::Biome && entry.id == static_cast<std::uint8_t>(Biome::DeepOcean);
        if (!isUnreachableBiome) ++progress.found;
    }
    if (progress.found > progress.total) progress.found = progress.total;
    return progress;
}

int Journal::surveyPercent() const {
    const SurveyProgress progress = survey();
    return progress.total > 0 ? progress.found * 100 / progress.total : 0;
}

std::string Journal::toJson() const {
    // Names are fixed ASCII with no quotes or backslashes, so no escaping is needed.
    std::ostringstream json;
    json << '[';
    for (std::size_t index = 0; index < entries_.size(); ++index) {
        const JournalEntry& entry = entries_[index];
        if (index > 0) json << ',';
        json << "{\"type\":\"" << categoryLabel(entry.category) << "\",\"name\":\"" << journalEntryName(entry)
             << "\",\"x\":" << entry.tileX << ",\"y\":" << entry.tileY << ",\"day\":" << entry.day << '}';
    }
    json << ']';
    return json.str();
}

}  // namespace wl
