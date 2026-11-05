#include "noise.h"

#include <algorithm>
#include <cmath>

#include "hash.h"

namespace wl {

namespace {

// Simplex works on a grid of triangles. Skewing squashes the input so the
// triangles become unit squares split down the diagonal, which makes finding
// the containing cell a simple floor. Unskewing goes the other way.
const double skewFactor = 0.5 * (std::sqrt(3.0) - 1.0);
const double unskewFactor = (3.0 - std::sqrt(3.0)) / 6.0;

// Each corner's influence fades to zero at this squared distance, so only
// the three corners of the containing triangle ever contribute.
constexpr double cornerRadiusSquared = 0.5;

// The raw sum peaks at about 1/99.2 with unit-length gradients (measured over
// millions of samples). Scaling by this puts the output in [-1, 1].
constexpr double outputScale = 99.2;

// Eight unit gradients at 45 degree steps. Enough variety to avoid patterns.
constexpr double diagonal = 0.70710678118654752;
constexpr double gradientX[8] = {1, diagonal, 0, -diagonal, -1, -diagonal, 0, diagonal};
constexpr double gradientY[8] = {0, diagonal, 1, diagonal, 0, -diagonal, -1, -diagonal};

// std::floor is slow on some targets, and a plain int cast rounds toward zero.
inline int floorToInt(double value) {
    const int truncated = static_cast<int>(value);
    return value < truncated ? truncated - 1 : truncated;
}

// Gradients come from hashing the cell coords rather than a 256-entry
// permutation table, so the noise never repeats across the infinite world.
inline double cornerContribution(std::uint32_t seed, int cellX, int cellY, double offsetX,
                                 double offsetY) {
    double falloff = cornerRadiusSquared - offsetX * offsetX - offsetY * offsetY;
    if (falloff <= 0.0) return 0.0;
    falloff *= falloff;
    const std::uint32_t gradientIndex = hashCoords(seed, cellX, cellY) & 7u;
    return falloff * falloff * (gradientX[gradientIndex] * offsetX + gradientY[gradientIndex] * offsetY);
}

}  // namespace

double simplexNoise2D(std::uint32_t seed, double x, double y) {
    // Find which skewed cell we're in.
    const double skew = (x + y) * skewFactor;
    const int cellX = floorToInt(x + skew);
    const int cellY = floorToInt(y + skew);

    // Offset from the cell's first corner, back in normal space.
    const double unskew = (cellX + cellY) * unskewFactor;
    const double offsetX0 = x - (cellX - unskew);
    const double offsetY0 = y - (cellY - unskew);

    // Upper or lower triangle of the cell decides the middle corner.
    const int middleStepX = offsetX0 > offsetY0 ? 1 : 0;
    const int middleStepY = 1 - middleStepX;

    const double offsetX1 = offsetX0 - middleStepX + unskewFactor;
    const double offsetY1 = offsetY0 - middleStepY + unskewFactor;
    const double offsetX2 = offsetX0 - 1.0 + 2.0 * unskewFactor;
    const double offsetY2 = offsetY0 - 1.0 + 2.0 * unskewFactor;

    const double total =
        cornerContribution(seed, cellX, cellY, offsetX0, offsetY0) +
        cornerContribution(seed, cellX + middleStepX, cellY + middleStepY, offsetX1, offsetY1) +
        cornerContribution(seed, cellX + 1, cellY + 1, offsetX2, offsetY2);

    // Clamp guards against rare float overshoot so callers can trust the range.
    return std::clamp(total * outputScale, -1.0, 1.0);
}

FractalNoise::FractalNoise(std::uint32_t seed, const FractalNoiseSettings& settings)
    : settings_(settings) {
    settings_.octaves = std::clamp(settings_.octaves, 1, maxOctaves);

    // Separate seed per octave. With one shared seed every octave would hit
    // zero at the origin together and leave a visible artefact there.
    double amplitude = 1.0;
    amplitudeTotal_ = 0.0;
    for (int octave = 0; octave < settings_.octaves; ++octave) {
        octaveSeeds_[octave] = deriveSeed(seed, static_cast<std::uint32_t>(octave));
        amplitudeTotal_ += amplitude;
        amplitude *= settings_.persistence;
    }
}

double FractalNoise::sample(double x, double y) const {
    double total = 0.0;
    double amplitude = 1.0;
    double frequency = settings_.baseFrequency;
    for (int octave = 0; octave < settings_.octaves; ++octave) {
        total += amplitude * simplexNoise2D(octaveSeeds_[octave], x * frequency, y * frequency);
        amplitude *= settings_.persistence;
        frequency *= settings_.lacunarity;
    }
    // Dividing by the amplitude sum keeps the result in [-1, 1] for any octave count.
    return total / amplitudeTotal_;
}

double FractalNoise::sampleRidged(double x, double y) const {
    double total = 0.0;
    double amplitude = 1.0;
    double frequency = settings_.baseFrequency;
    for (int octave = 0; octave < settings_.octaves; ++octave) {
        // 1 - |n| peaks where noise crosses zero, which traces thin lines.
        // Squaring sharpens those lines into ridges.
        const double ridge =
            1.0 - std::abs(simplexNoise2D(octaveSeeds_[octave], x * frequency, y * frequency));
        total += amplitude * ridge * ridge;
        amplitude *= settings_.persistence;
        frequency *= settings_.lacunarity;
    }
    return total / amplitudeTotal_;
}

}  // namespace wl
