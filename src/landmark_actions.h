// What scanning a landmark does. The rules live here as a pure function so
// they can be tested without a running game; Game applies the result.
//
//   Tower         lights a beacon that reveals the minimap around it
//   Crystal grove gives research samples
//   Ruins         decode the next story fragment
//
// Each landmark can only be used once. Campfires and hot springs are for
// resting rather than scanning, so they have no scan effect.
#pragma once

#include "world.h"

namespace wl {

// A landmark the player has already used, keyed by its tile.
struct UsedLandmark {
    int tileX = 0;
    int tileY = 0;

    bool operator==(const UsedLandmark& other) const { return tileX == other.tileX && tileY == other.tileY; }
};

enum class LandmarkEffect { None, BeaconLit, SamplesGained, LoreDecoded, NothingNew };

struct LandmarkResult {
    LandmarkEffect effect = LandmarkEffect::None;
    int samples = 0;    // for SamplesGained
    int loreIndex = 0;  // for LoreDecoded
};

// Only these kinds can be scanned at all.
bool isScannableLandmark(PropKind kind);

// loreDecodedSoFar is how many fragments are already in the journal. When
// they are all found, ruins give NothingNew (and still count as used).
LandmarkResult scanLandmarkResult(PropKind kind, int loreDecodedSoFar);

// A short line for the on-screen notice, e.g. "Crystal Grove: +3 samples".
std::string landmarkNoticeText(PropKind kind, const LandmarkResult& result);

}  // namespace wl
