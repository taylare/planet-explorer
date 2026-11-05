// All sprite art, built once at startup from text in art.cpp.
#pragma once

#include <array>

#include "entities.h"
#include "sprite.h"
#include "world.h"

namespace wl {

inline constexpr int playerWalkFrameCount = 3;
inline constexpr int campfireFrameCount = 2;
inline constexpr int flowerColorCount = 4;

class SpriteLibrary {
public:
    SpriteLibrary();

    const Sprite& player(int walkFrame) const;
    const Sprite& prop(const Prop& prop, int animationFrame) const;
    const Sprite& creature(Species species) const;

    // Flat things drawn under everything else, centred on their anchor.
    static bool isGroundDecal(PropKind kind);

    // Where the lantern sits, relative to the foot point at 1x scale, facing right.
    static constexpr int lanternOffsetX = 3;
    static constexpr int lanternOffsetY = -3;

    // Tower window rows and column in sprite pixels (outline included).
    static constexpr int towerWindowColumn = 5;
    static constexpr int towerWindowRows[2] = {4, 11};

private:
    std::array<Sprite, playerWalkFrameCount> playerFrames_;
    Sprite broadleafGrassland_;
    Sprite broadleafForest_;
    Sprite pineGreen_;
    Sprite pineSnowy_;
    Sprite acacia_;
    Sprite cactus_;
    Sprite rockGrey_;
    Sprite rockSandstone_;
    std::array<Sprite, flowerColorCount> flowers_;
    Sprite dryGrass_;
    Sprite ruins_;
    std::array<Sprite, campfireFrameCount> campfireFrames_;
    Sprite tower_;
    Sprite crystalGrove_;
    Sprite hotSpring_;
    std::array<Sprite, speciesCount> creatures_;
};

}  // namespace wl
