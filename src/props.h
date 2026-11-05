// Scatters trees, rocks, flowers, ruins, campfires and rare landmarks.
// Everything is keyed off the world seed and grid positions, so a chunk gets
// the same props however many times, and in whatever order, it's generated.
#pragma once

#include <optional>

#include "world.h"

namespace wl {

// The rare landmark for one landmark region, if it has one.
// Region (rx, ry) covers chunks rx*size .. rx*size + size-1 on each axis.
std::optional<Prop> findRegionLandmark(const World& world, int regionX, int regionY);

// Fills chunk.props. Expects the chunk's biomes to be generated already.
void scatterProps(const World& world, Chunk& chunk);

}  // namespace wl
