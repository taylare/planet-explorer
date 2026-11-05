// Small integer hashes. Anything random in the world comes from these, keyed by
// seed and position, so results never depend on generation order.
#pragma once

#include <cstdint>

namespace wl {

// "lowbias32" by Chris Wellons. The odd constants are part of the published
// algorithm: they spread every input bit across the whole output.
constexpr std::uint32_t mixBits32(std::uint32_t value) {
    value ^= value >> 16;
    value *= 0x7feb352du;
    value ^= value >> 15;
    value *= 0x846ca68bu;
    value ^= value >> 16;
    return value;
}

// Golden-ratio constant, so seed 0 doesn't hash like an empty input.
inline constexpr std::uint32_t hashSeedOffset = 0x9e3779b9u;

// Mixing x then y separately means (1, 2) and (2, 1) hash differently.
constexpr std::uint32_t hashCoords(std::uint32_t seed, std::int32_t x, std::int32_t y) {
    std::uint32_t hash = mixBits32(seed ^ hashSeedOffset);
    hash = mixBits32(hash ^ static_cast<std::uint32_t>(x));
    hash = mixBits32(hash ^ static_cast<std::uint32_t>(y));
    return hash;
}

// Gives each noise field or system its own independent seed.
constexpr std::uint32_t deriveSeed(std::uint32_t seed, std::uint32_t salt) {
    return mixBits32(seed ^ mixBits32(salt));
}

// Top 24 bits to a float in [0, 1). 24 bits is all a float can hold exactly.
constexpr float hashToUnitFloat(std::uint32_t hash) {
    return static_cast<float>(hash >> 8) * (1.0f / 16777216.0f);
}

}  // namespace wl
