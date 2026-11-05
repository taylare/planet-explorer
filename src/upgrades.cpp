#include "upgrades.h"

#include <algorithm>

namespace wl {

std::string_view upgradeName(UpgradeKind kind) {
    switch (kind) {
        case UpgradeKind::Scanner: return "Scanner";
        case UpgradeKind::Headlamp: return "Headlamp";
        case UpgradeKind::PowerCell: return "Power Cell";
        case UpgradeKind::Count: break;
    }
    return "Unknown";
}

std::string_view upgradeDescription(UpgradeKind kind) {
    switch (kind) {
        case UpgradeKind::Scanner: return "Scans from further away, and faster.";
        case UpgradeKind::Headlamp: return "A wider pool of light at night.";
        case UpgradeKind::PowerCell: return "A bigger battery for longer trips.";
        case UpgradeKind::Count: break;
    }
    return "";
}

int upgradeCost(int currentLevel) {
    if (currentLevel < 0 || currentLevel >= constants::maxUpgradeLevel) return -1;
    return constants::upgradeCostByLevel[currentLevel];
}

namespace {
double clampedLevel(int level) { return std::clamp(level, 0, constants::maxUpgradeLevel); }
}  // namespace

double scannerRangeInTiles(int level) {
    return constants::scanRangeInTiles * (1.0 + constants::scannerRangePerLevel * clampedLevel(level));
}

double scannerDurationScale(int level) { return 1.0 - constants::scannerSpeedPerLevel * clampedLevel(level); }

double headlampRadiusScale(int level) { return 1.0 + constants::headlampRadiusPerLevel * clampedLevel(level); }

double suitCapacityAt(int level) {
    return constants::suitBaseCapacity * (1.0 + constants::powerCellCapacityPerLevel * clampedLevel(level));
}

bool Upgrades::raise(UpgradeKind kind) {
    if (isMaxed(kind)) return false;
    ++levels[static_cast<int>(kind)];
    return true;
}

}  // namespace wl
