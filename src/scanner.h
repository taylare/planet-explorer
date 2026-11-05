// The field scanner: hold the action button near an animal or a landmark and,
// after a moment, it is logged or put to use.
//
// Scanning is also the game's stealth mechanic. While the button is held the
// player creeps along and animals startle at a shorter range (see
// constants::sneakStartleFraction), so you can slip close enough to scan them.
#pragma once

#include <vector>

#include "entities.h"
#include "journal.h"
#include "world.h"

namespace wl {

// A landmark the scanner might lock on to. Used ones still get a (grey) reticle
// so the player can see why nothing is happening.
struct ScanLandmark {
    double tileX = 0.0;
    double tileY = 0.0;
    PropKind kind = PropKind::Tower;
    bool used = false;
};

enum class ScanTargetKind { Creature, Landmark };

// What the renderer needs to draw the scan reticle.
struct ScanStatus {
    bool hasTarget = false;
    Vec2 targetPosition;
    ScanTargetKind kind = ScanTargetKind::Creature;
    Species species = Species::Rabbit;    // when kind is Creature
    PropKind landmarkKind = PropKind::Tower;  // when kind is Landmark
    int landmarkTileX = 0;
    int landmarkTileY = 0;
    double progress = 0.0;       // 0 to 1
    bool alreadyLogged = false;  // reticle goes grey and nothing builds up
};

class Scanner {
public:
    // Runs one fixed step. Returns true on the step a scan completes; the
    // completed* accessors then say what it was. Animals need creatureSeconds
    // of holding, landmarks need landmarkSeconds.
    bool update(bool actionHeld, const Vec2& playerPosition, const std::vector<Creature>& creatures,
                const std::vector<ScanLandmark>& landmarks, const Journal& journal, double stepSeconds,
                double rangeInTiles, double creatureSeconds, double landmarkSeconds);

    const ScanStatus& status() const { return status_; }

    ScanTargetKind completedKind() const { return completedKind_; }
    Species completedSpecies() const { return completedSpecies_; }
    PropKind completedLandmarkKind() const { return completedLandmarkKind_; }
    int completedLandmarkTileX() const { return completedLandmarkTileX_; }
    int completedLandmarkTileY() const { return completedLandmarkTileY_; }
    void reset();

private:
    ScanStatus status_;
    ScanTargetKind completedKind_ = ScanTargetKind::Creature;
    Species completedSpecies_ = Species::Rabbit;
    PropKind completedLandmarkKind_ = PropKind::Tower;
    int completedLandmarkTileX_ = 0;
    int completedLandmarkTileY_ = 0;
};

}  // namespace wl
