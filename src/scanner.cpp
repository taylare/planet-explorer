#include "scanner.h"

#include <algorithm>
#include <cmath>

#include "constants.h"

namespace wl {

namespace {
// The best target found so far this step.
struct Candidate {
    bool found = false;
    ScanStatus status;
    double distanceSquared = 0.0;
};

// An unlogged target beats a logged one however far away; among equals, nearest wins.
bool isBetter(const Candidate& current, bool logged, double distanceSquared) {
    if (!current.found) return true;
    if (current.status.alreadyLogged != logged) return !logged;
    return distanceSquared < current.distanceSquared;
}

bool sameTarget(const ScanStatus& a, const ScanStatus& b) {
    if (a.kind != b.kind) return false;
    if (a.kind == ScanTargetKind::Creature) return a.species == b.species;
    return a.landmarkTileX == b.landmarkTileX && a.landmarkTileY == b.landmarkTileY;
}
}  // namespace

bool Scanner::update(bool actionHeld, const Vec2& playerPosition, const std::vector<Creature>& creatures,
                     const std::vector<ScanLandmark>& landmarks, const Journal& journal, double stepSeconds,
                     double rangeInTiles, double creatureSeconds, double landmarkSeconds) {
    const auto drainProgress = [this, stepSeconds, creatureSeconds]() {
        const double drain = stepSeconds / creatureSeconds * constants::scanDecayMultiplier;
        status_.progress = std::max(0.0, status_.progress - drain);
    };

    if (!actionHeld) {
        status_.hasTarget = false;
        drainProgress();
        return false;
    }

    const double rangeSquared = rangeInTiles * rangeInTiles;
    Candidate best;

    for (const Creature& creature : creatures) {
        if (creature.fleeing) continue;
        const double deltaX = creature.position.x - playerPosition.x;
        const double deltaY = creature.position.y - playerPosition.y;
        const double distanceSquared = deltaX * deltaX + deltaY * deltaY;
        if (distanceSquared > rangeSquared) continue;

        const bool logged = journal.hasSpecies(creature.species);
        if (!isBetter(best, logged, distanceSquared)) continue;
        best.found = true;
        best.distanceSquared = distanceSquared;
        best.status = ScanStatus{};
        best.status.hasTarget = true;
        best.status.targetPosition = creature.position;
        best.status.kind = ScanTargetKind::Creature;
        best.status.species = creature.species;
        best.status.alreadyLogged = logged;
    }

    for (const ScanLandmark& landmark : landmarks) {
        const double deltaX = landmark.tileX - playerPosition.x;
        const double deltaY = landmark.tileY - playerPosition.y;
        const double distanceSquared = deltaX * deltaX + deltaY * deltaY;
        if (distanceSquared > rangeSquared) continue;

        if (!isBetter(best, landmark.used, distanceSquared)) continue;
        best.found = true;
        best.distanceSquared = distanceSquared;
        best.status = ScanStatus{};
        best.status.hasTarget = true;
        best.status.targetPosition = {landmark.tileX, landmark.tileY};
        best.status.kind = ScanTargetKind::Landmark;
        best.status.landmarkKind = landmark.kind;
        best.status.landmarkTileX = static_cast<int>(std::floor(landmark.tileX));
        best.status.landmarkTileY = static_cast<int>(std::floor(landmark.tileY));
        best.status.alreadyLogged = landmark.used;
    }

    if (!best.found) {
        status_.hasTarget = false;
        drainProgress();
        return false;
    }

    // Progress belongs to one target, so switching to a different one starts from nothing.
    const bool continuing = status_.hasTarget && sameTarget(status_, best.status);
    const double carriedProgress = continuing ? status_.progress : 0.0;
    status_ = best.status;
    status_.progress = carriedProgress;

    if (status_.alreadyLogged) {
        status_.progress = 0.0;
        return false;
    }

    const double durationSeconds = status_.kind == ScanTargetKind::Landmark ? landmarkSeconds : creatureSeconds;
    status_.progress += stepSeconds / durationSeconds;
    if (status_.progress < 1.0) return false;

    completedKind_ = status_.kind;
    completedSpecies_ = status_.species;
    completedLandmarkKind_ = status_.landmarkKind;
    completedLandmarkTileX_ = status_.landmarkTileX;
    completedLandmarkTileY_ = status_.landmarkTileY;
    status_.progress = 0.0;
    return true;
}

void Scanner::reset() { *this = Scanner{}; }

}  // namespace wl
