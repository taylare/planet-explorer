// Suit upgrades bought at the ship with research samples. Each of the three
// goes from level 0 to constants::maxUpgradeLevel, and each level costs more.
#pragma once

#include <array>
#include <string_view>

#include "constants.h"

namespace wl {

enum class UpgradeKind { Scanner, Headlamp, PowerCell, Count };

inline constexpr int upgradeKindCount = static_cast<int>(UpgradeKind::Count);

std::string_view upgradeName(UpgradeKind kind);
std::string_view upgradeDescription(UpgradeKind kind);

// Samples needed to go from currentLevel to the next. -1 when already at the top.
int upgradeCost(int currentLevel);

// Effects at a level (0 is the base game).
double scannerRangeInTiles(int level);
double scannerDurationScale(int level);  // multiplies scan times, so smaller is faster
double headlampRadiusScale(int level);
double suitCapacityAt(int level);

struct Upgrades {
    std::array<int, upgradeKindCount> levels{};

    int level(UpgradeKind kind) const { return levels[static_cast<int>(kind)]; }
    bool isMaxed(UpgradeKind kind) const { return level(kind) >= constants::maxUpgradeLevel; }
    int nextCost(UpgradeKind kind) const { return upgradeCost(level(kind)); }

    // Raises the level by one. False (and no change) if it is already maxed.
    bool raise(UpgradeKind kind);

    bool operator==(const Upgrades& other) const { return levels == other.levels; }
};

}  // namespace wl
