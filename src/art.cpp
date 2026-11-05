#include "art.h"

#include <algorithm>

#include "constants.h"

namespace wl {

namespace {
PackedColor toPacked(const constants::Rgb8& color) { return packRgba(color.red, color.green, color.blue); }

constexpr bool outlined = true;
constexpr bool flat = false;  // ground decals skip the outline
}  // namespace

bool SpriteLibrary::isGroundDecal(PropKind kind) {
    return kind == PropKind::Flower || kind == PropKind::HotSpring;
}

SpriteLibrary::SpriteLibrary() {
    using namespace constants;

    // --- Player: a hooded wanderer with a lantern. Standing, then two steps. ---
    const std::initializer_list<SpriteColorKey> playerColors = {
        {'h', toPacked(playerHoodColor)},  {'f', toPacked(playerFaceColor)},
        {'c', toPacked(playerCloakColor)}, {'b', toPacked(playerBootColor)},
        {'L', toPacked(lanternColor)},     {'l', toPacked(lanternHandleColor)},
    };
    const char* playerLegs[playerWalkFrameCount] = {"..b.b...", ".b...b..", "...bb..."};
    for (int frame = 0; frame < playerWalkFrameCount; ++frame) {
        playerFrames_[frame] = buildSprite({"..hhh...",
                                            ".hhhhh..",
                                            ".hfffh..",
                                            ".ccccc..",
                                            "cccccccl",
                                            ".ccccc.L",
                                            ".ccccc..",
                                            playerLegs[frame]},
                                           playerColors, outlined);
    }

    // --- Trees ---
    const auto broadleaf = [](PackedColor light, PackedColor dark) {
        return buildSprite({"...gGGGg...",
                            "..gGGGGGGg.",
                            ".gGGgGGGGGg",
                            "gGGGGGGgGGg",
                            "gGgGGGGGGGg",
                            "gGGGGGgGGGg",
                            ".gGGGGGGGg.",
                            "..ggGgGgg..",
                            "....gtg....",
                            ".....t.....",
                            ".....t.....",
                            "....ttt...."},
                           {{'G', light}, {'g', dark}, {'t', toPacked(trunkColor)}}, outlined);
    };
    broadleafGrassland_ = broadleaf(toPacked(leafLightColor), toPacked(leafDarkColor));
    broadleafForest_ = broadleaf(toPacked(forestLeafLightColor), toPacked(forestLeafDarkColor));

    const auto pine = [](PackedColor light, PackedColor dark) {
        return buildSprite({"...P...",
                            "..pPp..",
                            "..pPp..",
                            ".pPPPp.",
                            "..pPp..",
                            ".pPPPp.",
                            "pPPPPPp",
                            ".pPPPp.",
                            "pPPPPPp",
                            "ppPPPpp",
                            "...t...",
                            "...t..."},
                           {{'P', light}, {'p', dark}, {'t', toPacked(trunkColor)}}, outlined);
    };
    pineGreen_ = pine(toPacked(pineLightColor), toPacked(pineDarkColor));
    pineSnowy_ = pine(toPacked(snowOnBranchColor), toPacked(pineDarkColor));

    acacia_ = buildSprite({".aaaaaaaaaaa.",
                           "aAAAaAAAAAaAa",
                           ".aaaaaaaaaaa.",
                           "...t.....t...",
                           "....t...t....",
                           ".....t.t.....",
                           "......t......",
                           "......t......"},
                          {{'A', toPacked(acaciaLeafColor)},
                           {'a', toPacked(acaciaLeafDarkColor)},
                           {'t', toPacked(trunkColor)}},
                          outlined);

    cactus_ = buildSprite({"..C..",
                           "..C..",
                           "c.C.c",
                           "cCC.c",
                           "..CCc",
                           "..C..",
                           "..C..",
                           "..C.."},
                          {{'C', toPacked(cactusLightColor)}, {'c', toPacked(cactusDarkColor)}}, outlined);

    // --- Rocks ---
    const auto rock = [](PackedColor light, PackedColor dark) {
        return buildSprite({"..rr..",
                            ".rRRr.",
                            "rRRRRr",
                            "rrRrrr"},
                           {{'R', light}, {'r', dark}}, outlined);
    };
    rockGrey_ = rock(toPacked(rockLightColor), toPacked(rockDarkColor));
    rockSandstone_ = rock(toPacked(sandstoneLightColor), toPacked(sandstoneDarkColor));

    // --- Flowers and grass tufts (flat) ---
    for (int colorIndex = 0; colorIndex < flowerColorCount; ++colorIndex) {
        flowers_[colorIndex] = buildSprite({".p.",
                                            "pcp",
                                            ".s."},
                                           {{'p', toPacked(flowerPetalColors[colorIndex])},
                                            {'c', toPacked(flowerCentreColor)},
                                            {'s', toPacked(flowerStemColor)}},
                                           flat);
    }
    dryGrass_ = buildSprite({"y..y.",
                             ".y.y.",
                             ".yyy."},
                            {{'y', toPacked(dryGrassColor)}}, flat);

    // --- Ruins and campfires ---
    ruins_ = buildSprite({"s...........s",
                          "sS.........sS",
                          "sS..s......sS",
                          "sSs.sS..s.ssS",
                          "sSSssSssSsSSS",
                          "msSSSsSSSSsmm"},
                         {{'S', toPacked(ruinStoneLightColor)},
                          {'s', toPacked(ruinStoneDarkColor)},
                          {'m', toPacked(mossColor)}},
                         outlined);

    const std::initializer_list<SpriteColorKey> fireColors = {
        {'f', toPacked(flameOuterColor)}, {'F', toPacked(flameInnerColor)}, {'w', toPacked(firewoodColor)}};
    campfireFrames_[0] = buildSprite({"...f...",
                                      "..fFf..",
                                      "..FFf..",
                                      ".wfFfw.",
                                      "wwwwwww"},
                                     fireColors, outlined);
    campfireFrames_[1] = buildSprite({"..f....",
                                      "..fF.f.",
                                      ".fFFf..",
                                      ".wFFfw.",
                                      "wwwwwww"},
                                     fireColors, outlined);

    // --- Rare landmarks ---
    tower_ = buildSprite({"S.S.S.S.S",
                          "SSSSSSSSS",
                          ".sSSSSSs.",
                          ".sSSwSSs.",
                          ".sSSSSSs.",
                          ".sSSSSSs.",
                          ".sSsSSSs.",
                          ".sSSSSSs.",
                          ".sSSSSsS.",
                          ".sSSSSSs.",
                          ".sSSwSSs.",
                          ".sSSSSSs.",
                          ".sSSSSSs.",
                          ".sSsSSSs.",
                          ".sSSSSSs.",
                          ".sSSSSSs.",
                          "ssSSSSSss",
                          "sssssssss"},
                         {{'S', toPacked(towerStoneLightColor)},
                          {'s', toPacked(towerStoneDarkColor)},
                          {'w', toPacked(towerWindowColor)}},
                         outlined);

    crystalGrove_ = buildSprite({"......c......",
                                 ".....cC......",
                                 "..a..cC...c..",
                                 ".aa..CCc..Cc.",
                                 ".aa.cCCc.cCc.",
                                 ".aaccCCCccCC.",
                                 "cCCCCCCCCCCCc",
                                 ".ccccccccccc."},
                                {{'C', toPacked(crystalLightColor)},
                                 {'c', toPacked(crystalDarkColor)},
                                 {'a', toPacked(crystalAccentColor)}},
                                outlined);

    hotSpring_ = buildSprite({"....rrrrrrrrrr....",
                              "..rrwwwwwwwwwwrr..",
                              ".rwwWWWWWWWWWWwwr.",
                              "rwWWWWWWWWWWWWWWwr",
                              "rwWWWWWWWWWWWWWWwr",
                              ".rwwWWWWWWWWWWwwr.",
                              "..rrwwwwwwwwwwrr..",
                              "....rrrrrrrrrr...."},
                             {{'r', toPacked(rockDarkColor)},
                              {'w', toPacked(springWaterDarkColor)},
                              {'W', toPacked(springWaterLightColor)}},
                             flat);

    // --- Creatures, all facing right ---
    const PackedColor eye = toPacked(creatureEyeColor);
    const PackedColor legs = toPacked(creatureLegColor);
    creatures_[static_cast<int>(Species::Deer)] = buildSprite({"......aa",
                                                               "......a.",
                                                               ".....bbe",
                                                               "tbbbbbb.",
                                                               ".bBBBb..",
                                                               ".l...l..",
                                                               ".l...l.."},
                                                              {{'a', toPacked(antlerColor)},
                                                               {'b', toPacked(deerBodyColor)},
                                                               {'B', toPacked(deerBellyColor)},
                                                               {'t', toPacked(deerBellyColor)},
                                                               {'e', eye},
                                                               {'l', legs}},
                                                              outlined);
    creatures_[static_cast<int>(Species::Rabbit)] = buildSprite({"....b.",
                                                                 "....bb",
                                                                 ".bbbbe",
                                                                 "tbbbb.",
                                                                 ".l.l.."},
                                                                {{'b', toPacked(rabbitBodyColor)},
                                                                 {'t', toPacked(rabbitTailColor)},
                                                                 {'e', eye},
                                                                 {'l', legs}},
                                                                outlined);
    creatures_[static_cast<int>(Species::Crab)] = buildSprite({"C.....C",
                                                               "CsssssC",
                                                               ".sesss.",
                                                               "l.l.l.l"},
                                                              {{'C', toPacked(crabClawColor)},
                                                               {'s', toPacked(crabShellColor)},
                                                               {'e', eye},
                                                               {'l', toPacked(crabClawColor)}},
                                                              outlined);
    creatures_[static_cast<int>(Species::Lizard)] = buildSprite({".....bb.",
                                                                 "tbpbpbbe",
                                                                 ".l...l.."},
                                                                {{'b', toPacked(lizardBodyColor)},
                                                                 {'t', toPacked(lizardBodyColor)},
                                                                 {'p', toPacked(lizardPatternColor)},
                                                                 {'e', eye},
                                                                 {'l', toPacked(lizardBodyColor)}},
                                                                outlined);
    creatures_[static_cast<int>(Species::Goat)] = buildSprite({"......h.",
                                                               ".....hb.",
                                                               ".....bbe",
                                                               "tbbbbbb.",
                                                               ".bbbbb..",
                                                               ".l...l.."},
                                                              {{'h', toPacked(goatHornColor)},
                                                               {'b', toPacked(goatBodyColor)},
                                                               {'t', toPacked(goatBodyColor)},
                                                               {'e', eye},
                                                               {'l', toPacked(goatHornColor)}},
                                                              outlined);
    creatures_[static_cast<int>(Species::Fox)] = buildSprite({"......b.",
                                                              "......bb",
                                                              ".....bbe",
                                                              "ttbbbbb.",
                                                              ".sbbbbs.",
                                                              ".s...s.."},
                                                             {{'b', toPacked(foxBodyColor)},
                                                              {'t', toPacked(foxBodyColor)},
                                                              {'s', toPacked(foxShadeColor)},
                                                              {'e', eye}},
                                                             outlined);
}

const Sprite& SpriteLibrary::player(int walkFrame) const {
    return playerFrames_[std::clamp(walkFrame, 0, playerWalkFrameCount - 1)];
}

const Sprite& SpriteLibrary::prop(const Prop& prop, int animationFrame) const {
    const Biome ground = prop.groundBiome;
    switch (prop.kind) {
        case PropKind::BroadleafTree: return ground == Biome::Forest ? broadleafForest_ : broadleafGrassland_;
        case PropKind::PineTree: return ground == Biome::Snow ? pineSnowy_ : pineGreen_;
        case PropKind::AcaciaTree: return acacia_;
        case PropKind::Cactus: return cactus_;
        case PropKind::Rock:
            return (ground == Biome::Desert || ground == Biome::Beach || ground == Biome::Savanna) ? rockSandstone_
                                                                                                  : rockGrey_;
        case PropKind::Flower:
            return ground == Biome::Savanna ? dryGrass_ : flowers_[prop.variant % flowerColorCount];
        case PropKind::Ruins: return ruins_;
        case PropKind::Campfire: return campfireFrames_[(animationFrame + prop.variant) % campfireFrameCount];
        case PropKind::Tower: return tower_;
        case PropKind::CrystalGrove: return crystalGrove_;
        case PropKind::HotSpring: return hotSpring_;
        case PropKind::Count: break;
    }
    return rockGrey_;
}

const Sprite& SpriteLibrary::creature(Species species) const {
    return creatures_[std::clamp(static_cast<int>(species), 0, speciesCount - 1)];
}

}  // namespace wl
