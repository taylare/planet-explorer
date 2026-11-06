// The star chart: a few other planets the lander can fly to.
//
// Destinations are a pure function of the current world's seed, so the same
// planet always offers the same neighbours, and nothing needs saving.
#pragma once

#include <array>

#include "world.h"

namespace wl {

inline constexpr int destinationCount = 3;

// Three seeds that differ from the current one, from each other, and, so the chart
// reads clearly, in planet name too.
std::array<WorldSeed, destinationCount> destinationSeeds(WorldSeed current);

}  // namespace wl
