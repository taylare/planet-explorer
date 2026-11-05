#include "landmark_actions.h"

#include <string>

#include "constants.h"
#include "lore.h"

namespace wl {

bool isScannableLandmark(PropKind kind) {
    return kind == PropKind::Tower || kind == PropKind::CrystalGrove || kind == PropKind::Ruins;
}

LandmarkResult scanLandmarkResult(PropKind kind, int loreDecodedSoFar) {
    LandmarkResult result;
    switch (kind) {
        case PropKind::Tower:
            result.effect = LandmarkEffect::BeaconLit;
            break;
        case PropKind::CrystalGrove:
            result.effect = LandmarkEffect::SamplesGained;
            result.samples = constants::crystalGroveSampleYield;
            break;
        case PropKind::Ruins:
            if (loreDecodedSoFar < loreFragmentCount) {
                result.effect = LandmarkEffect::LoreDecoded;
                result.loreIndex = loreDecodedSoFar;
            } else {
                result.effect = LandmarkEffect::NothingNew;
            }
            break;
        default:
            break;
    }
    return result;
}

std::string landmarkNoticeText(PropKind kind, const LandmarkResult& result) {
    const std::string name(propKindName(kind));
    switch (result.effect) {
        case LandmarkEffect::BeaconLit: return name + ": beacon lit, map revealed";
        case LandmarkEffect::SamplesGained: return name + ": +" + std::to_string(result.samples) + " samples";
        case LandmarkEffect::LoreDecoded: return std::string(loreTitle(result.loreIndex)) + " decoded";
        case LandmarkEffect::NothingNew: return name + ": nothing left to decode";
        case LandmarkEffect::None: break;
    }
    return name;
}

}  // namespace wl
