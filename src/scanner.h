// The field scanner: hold the action button near an animal and, after a
// moment, it is logged in the journal.
//
// Scanning is the game's stealth mechanic. While the button is held the
// player creeps along and animals startle at a shorter range (see
// constants::sneakStartleFraction), so you can slip close enough to scan them.
#pragma once

#include <vector>

#include "entities.h"
#include "journal.h"

namespace wl {

// What the renderer needs to draw the scan reticle.
struct ScanStatus {
    bool hasTarget = false;
    Vec2 targetPosition;
    Species species = Species::Rabbit;
    double progress = 0.0;       // 0 to 1
    bool alreadyLogged = false;  // reticle goes grey and nothing builds up
};

class Scanner {
public:
    // Runs one fixed step. Returns true on the step a scan completes, and
    // completedSpecies() then says which animal it was.
    bool update(bool actionHeld, const Vec2& playerPosition, const std::vector<Creature>& creatures,
                const Journal& journal, double stepSeconds, double rangeInTiles, double durationSeconds);

    const ScanStatus& status() const { return status_; }
    Species completedSpecies() const { return completedSpecies_; }
    void reset();

private:
    ScanStatus status_;
    Species completedSpecies_ = Species::Rabbit;
};

}  // namespace wl
