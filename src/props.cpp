#include "props.h"

#include <cmath>
#include <iterator>

#include "hash.h"

namespace wl {

namespace {

// Seed salts. Never change these, or every world's props would move.
enum class PropSalt : std::uint32_t {
    Cells = 101,
    Landmarks = 102,
};

constexpr PropKind rareLandmarkKinds[] = {PropKind::Tower, PropKind::CrystalGrove, PropKind::HotSpring};
constexpr std::uint32_t rareLandmarkKindCount = 3;

const constants::PropMix* propMixFor(Biome biome) {
    using namespace constants;
    switch (biome) {
        case Biome::Grassland: return &grasslandPropMix;
        case Biome::Forest: return &forestPropMix;
        case Biome::Desert: return &desertPropMix;
        case Biome::Savanna: return &savannaPropMix;
        case Biome::Tundra: return &tundraPropMix;
        case Biome::Snow: return &snowPropMix;
        case Biome::Mountain: return &mountainPropMix;
        case Biome::Beach: return &beachPropMix;
        default: return nullptr;  // nothing grows in open water
    }
}

// Which kind of "tree" a biome grows. Beaches grow none.
bool treeKindFor(Biome biome, PropKind& kindOut) {
    switch (biome) {
        case Biome::Grassland:
        case Biome::Forest: kindOut = PropKind::BroadleafTree; return true;
        case Biome::Tundra:
        case Biome::Snow:
        case Biome::Mountain: kindOut = PropKind::PineTree; return true;
        case Biome::Savanna: kindOut = PropKind::AcaciaTree; return true;
        case Biome::Desert: kindOut = PropKind::Cactus; return true;
        default: return false;
    }
}

bool landmarkSuits(PropKind kind, Biome biome) {
    switch (kind) {
        case PropKind::Tower:
            return biome == Biome::Grassland || biome == Biome::Savanna || biome == Biome::Tundra ||
                   biome == Biome::Desert;
        case PropKind::CrystalGrove:
            return biome == Biome::Forest || biome == Biome::Snow || biome == Biome::Mountain ||
                   biome == Biome::Tundra;
        case PropKind::HotSpring:
            return biome == Biome::Snow || biome == Biome::Tundra || biome == Biome::Mountain;
        default: return false;
    }
}

// Weighted pick between the kinds in a biome's mix.
bool pickPropKind(const constants::PropMix& mix, Biome biome, float roll, PropKind& kindOut) {
    PropKind treeKind = PropKind::BroadleafTree;
    const double treeWeight = treeKindFor(biome, treeKind) ? mix.tree : 0.0;

    const struct {
        PropKind kind;
        double weight;
    } options[] = {
        {treeKind, treeWeight},           {PropKind::Rock, mix.rock},
        {PropKind::Flower, mix.flower},   {PropKind::Ruins, mix.ruins},
        {PropKind::Campfire, mix.campfire},
    };

    double totalWeight = 0.0;
    for (const auto& option : options) totalWeight += option.weight;
    if (totalWeight <= 0.0) return false;

    double remaining = roll * totalWeight;
    for (const auto& option : options) {
        if (option.weight <= 0.0) continue;
        if (remaining < option.weight) {
            kindOut = option.kind;
            return true;
        }
        remaining -= option.weight;
    }
    // Float rounding can leave a sliver at the very end. Give it to the last real option.
    for (int index = static_cast<int>(std::size(options)) - 1; index >= 0; --index) {
        if (options[index].weight > 0.0) {
            kindOut = options[index].kind;
            return true;
        }
    }
    return false;
}

constexpr int regionSizeInTiles() {
    return constants::landmarkRegionSizeInChunks * constants::chunkSizeInTiles;
}

}  // namespace

std::string_view propKindName(PropKind kind) {
    switch (kind) {
        case PropKind::BroadleafTree: return "Tree";
        case PropKind::PineTree: return "Pine";
        case PropKind::AcaciaTree: return "Acacia";
        case PropKind::Cactus: return "Cactus";
        case PropKind::Rock: return "Rock";
        case PropKind::Flower: return "Flowers";
        case PropKind::Ruins: return "Ruins";
        case PropKind::Campfire: return "Campfire";
        case PropKind::Tower: return "Lone Tower";
        case PropKind::CrystalGrove: return "Crystal Grove";
        case PropKind::HotSpring: return "Hot Spring";
        case PropKind::Count: break;
    }
    return "Unknown";
}

bool isRareLandmark(PropKind kind) {
    return kind == PropKind::Tower || kind == PropKind::CrystalGrove || kind == PropKind::HotSpring;
}

std::optional<Prop> findRegionLandmark(const World& world, int regionX, int regionY) {
    const std::uint32_t regionHash =
        hashCoords(deriveSeed(world.seed(), static_cast<std::uint32_t>(PropSalt::Landmarks)), regionX, regionY);
    if (hashToUnitFloat(regionHash) >= constants::landmarkChancePerRegion) return std::nullopt;

    const PropKind kind = rareLandmarkKinds[mixBits32(regionHash) % rareLandmarkKindCount];
    const int originTileX = regionX * regionSizeInTiles();
    const int originTileY = regionY * regionSizeInTiles();

    // Try a few random spots in the region and take the first that suits.
    // If none do, the region just has no landmark this time.
    for (int attempt = 0; attempt < constants::landmarkPlacementAttempts; ++attempt) {
        const std::uint32_t attemptHash = hashCoords(regionHash, attempt, 0);
        const int tileX = originTileX + static_cast<int>(hashToUnitFloat(attemptHash) * regionSizeInTiles());
        const int tileY =
            originTileY + static_cast<int>(hashToUnitFloat(mixBits32(attemptHash)) * regionSizeInTiles());
        const Biome biome = world.sampleTerrain(tileX, tileY).biome;
        if (!landmarkSuits(kind, biome)) continue;

        Prop landmark;
        landmark.tileX = static_cast<float>(tileX) + 0.5f;
        landmark.tileY = static_cast<float>(tileY) + 0.5f;
        landmark.kind = kind;
        landmark.groundBiome = biome;
        landmark.variant = static_cast<std::uint8_t>(attemptHash >> 24);
        return landmark;
    }
    return std::nullopt;
}

void scatterProps(const World& world, Chunk& chunk) {
    using namespace constants;
    constexpr int size = chunkSizeInTiles;
    static_assert(size % propCellSizeInTiles == 0, "prop cells must not straddle chunk edges");

    const int originTileX = chunkOriginTile(chunk.coord.x);
    const int originTileY = chunkOriginTile(chunk.coord.y);

    // Landmarks first. Check every region that could reach this chunk with
    // its clearing, so props near a landmark in a neighbouring chunk also
    // keep their distance.
    const int clearing = static_cast<int>(std::ceil(landmarkClearingRadiusInTiles));
    const int minRegionX = floorDivide(originTileX - clearing, regionSizeInTiles());
    const int maxRegionX = floorDivide(originTileX + size - 1 + clearing, regionSizeInTiles());
    const int minRegionY = floorDivide(originTileY - clearing, regionSizeInTiles());
    const int maxRegionY = floorDivide(originTileY + size - 1 + clearing, regionSizeInTiles());

    std::vector<Prop> nearbyLandmarks;
    for (int regionY = minRegionY; regionY <= maxRegionY; ++regionY) {
        for (int regionX = minRegionX; regionX <= maxRegionX; ++regionX) {
            const std::optional<Prop> landmark = findRegionLandmark(world, regionX, regionY);
            if (!landmark) continue;
            nearbyLandmarks.push_back(*landmark);
            const ChunkCoord landmarkChunk = chunkCoordForTile(static_cast<int>(std::floor(landmark->tileX)),
                                                               static_cast<int>(std::floor(landmark->tileY)));
            if (landmarkChunk == chunk.coord) chunk.props.push_back(*landmark);
        }
    }

    const auto insideClearing = [&nearbyLandmarks](double tileX, double tileY) {
        for (const Prop& landmark : nearbyLandmarks) {
            const double deltaX = tileX - landmark.tileX;
            const double deltaY = tileY - landmark.tileY;
            if (deltaX * deltaX + deltaY * deltaY < landmarkClearingRadiusInTiles * landmarkClearingRadiusInTiles) {
                return true;
            }
        }
        return false;
    };

    // Jittered grid: one roll per cell, keyed by the cell's global position.
    const std::uint32_t cellSeed = deriveSeed(world.seed(), static_cast<std::uint32_t>(PropSalt::Cells));
    const int cellsPerSide = size / propCellSizeInTiles;
    const int firstCellX = floorDivide(originTileX, propCellSizeInTiles);
    const int firstCellY = floorDivide(originTileY, propCellSizeInTiles);
    const double jitterSpan = propCellSizeInTiles - 2.0 * propCellMarginInTiles;

    for (int cellY = 0; cellY < cellsPerSide; ++cellY) {
        for (int cellX = 0; cellX < cellsPerSide; ++cellX) {
            const std::uint32_t cellHash = hashCoords(cellSeed, firstCellX + cellX, firstCellY + cellY);
            // Separate hashes for each decision, so they don't correlate.
            const std::uint32_t jitterHashX = mixBits32(cellHash + 1);
            const std::uint32_t jitterHashY = mixBits32(cellHash + 2);
            const std::uint32_t kindHash = mixBits32(cellHash + 3);
            const std::uint32_t variantHash = mixBits32(cellHash + 4);

            // Round to float first (that's what gets stored) and look up the
            // biome from the rounded value, so a position just under a tile
            // edge can't round over it and disagree with its own ground.
            const float tileX = static_cast<float>(originTileX + cellX * propCellSizeInTiles + propCellMarginInTiles +
                                                   hashToUnitFloat(jitterHashX) * jitterSpan);
            const float tileY = static_cast<float>(originTileY + cellY * propCellSizeInTiles + propCellMarginInTiles +
                                                   hashToUnitFloat(jitterHashY) * jitterSpan);

            const int localX = static_cast<int>(std::floor(tileX)) - originTileX;
            const int localY = static_cast<int>(std::floor(tileY)) - originTileY;
            const Biome biome = chunk.biomes[Chunk::indexFor(localX, localY)];

            const PropMix* mix = propMixFor(biome);
            if (!mix || hashToUnitFloat(cellHash) >= mix->density) continue;

            PropKind kind = PropKind::Rock;
            if (!pickPropKind(*mix, biome, hashToUnitFloat(kindHash), kind)) continue;
            if (insideClearing(tileX, tileY)) continue;

            Prop prop;
            prop.tileX = tileX;
            prop.tileY = tileY;
            prop.kind = kind;
            prop.groundBiome = biome;
            prop.variant = static_cast<std::uint8_t>(variantHash >> 24);
            chunk.props.push_back(prop);
        }
    }
}

}  // namespace wl
