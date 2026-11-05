#include "scanner.h"

#include <algorithm>
#include <cmath>

#include "constants.h"

namespace wl {

bool Scanner::update(bool actionHeld, const Vec2& playerPosition, const std::vector<Creature>& creatures,
                     const Journal& journal, double stepSeconds, double rangeInTiles, double durationSeconds) {
    const auto drainProgress = [this, stepSeconds, durationSeconds]() {
        const double drain = stepSeconds / durationSeconds * constants::scanDecayMultiplier;
        status_.progress = std::max(0.0, status_.progress - drain);
    };

    if (!actionHeld) {
        status_.hasTarget = false;
        drainProgress();
        return false;
    }

    // Pick the nearest calm animal in range. An animal you haven't logged yet
    // beats a closer one you already have, so the reticle goes to what matters.
    const Creature* target = nullptr;
    double targetDistanceSquared = 0.0;
    bool targetLogged = false;
    const double rangeSquared = rangeInTiles * rangeInTiles;
    for (const Creature& creature : creatures) {
        if (creature.fleeing) continue;
        const double deltaX = creature.position.x - playerPosition.x;
        const double deltaY = creature.position.y - playerPosition.y;
        const double distanceSquared = deltaX * deltaX + deltaY * deltaY;
        if (distanceSquared > rangeSquared) continue;

        const bool logged = journal.hasSpecies(creature.species);
        const bool betterThanCurrent = target == nullptr || (targetLogged && !logged) ||
                                       (logged == targetLogged && distanceSquared < targetDistanceSquared);
        if (!betterThanCurrent) continue;
        target = &creature;
        targetDistanceSquared = distanceSquared;
        targetLogged = logged;
    }

    if (!target) {
        status_.hasTarget = false;
        drainProgress();
        return false;
    }

    // Progress belongs to the kind of animal, so a new animal of another kind starts from nothing.
    if (!status_.hasTarget || target->species != status_.species) status_.progress = 0.0;
    status_.hasTarget = true;
    status_.targetPosition = target->position;
    status_.species = target->species;
    status_.alreadyLogged = targetLogged;

    if (targetLogged) {
        status_.progress = 0.0;
        return false;
    }

    status_.progress += stepSeconds / durationSeconds;
    if (status_.progress < 1.0) return false;

    status_.progress = 0.0;
    completedSpecies_ = target->species;
    return true;
}

void Scanner::reset() {
    status_ = ScanStatus{};
    completedSpecies_ = Species::Rabbit;
}

}  // namespace wl
