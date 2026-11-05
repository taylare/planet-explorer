// 2D simplex noise and fractal Brownian motion (fBm), written from scratch.
#pragma once

#include <array>
#include <cstdint>

namespace wl {

// Simplex noise in [-1, 1]. Smooth, no visible grid, cheaper than Perlin.
double simplexNoise2D(std::uint32_t seed, double x, double y);

struct FractalNoiseSettings {
    double baseFrequency = 1.0;  // cycles per tile for the first octave
    int octaves = 1;             // layers of detail
    double lacunarity = 2.0;     // frequency multiplier per octave
    double persistence = 0.5;    // amplitude multiplier per octave
};

// fBm: big smooth shapes from the first octave, finer detail from each extra one.
class FractalNoise {
public:
    static constexpr int maxOctaves = 8;

    FractalNoise(std::uint32_t seed, const FractalNoiseSettings& settings);

    // Standard fBm, in [-1, 1].
    double sample(double x, double y) const;

    // Ridged fBm in [0, 1]. Folds noise at zero so you get sharp crests, good for mountains.
    double sampleRidged(double x, double y) const;

private:
    FractalNoiseSettings settings_;
    std::array<std::uint32_t, maxOctaves> octaveSeeds_{};
    double amplitudeTotal_ = 1.0;
};

}  // namespace wl
