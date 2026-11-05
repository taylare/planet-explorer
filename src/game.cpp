#include "game.h"

#include <algorithm>
#include <cmath>
#include <cstdint>

#include "constants.h"

namespace wl {

namespace {
constexpr double twoPi = 6.283185307179586;

std::uint8_t lerpChannel(int from, int to, double t) {
    return static_cast<std::uint8_t>(from + (to - from) * t);
}
}  // namespace

Game::Game()
    : clock_(constants::simulationStepSeconds, constants::maxSimulationStepsPerFrame,
             constants::maxFrameDeltaSeconds) {
    resizeDevice(constants::defaultDeviceWidthInPixels, constants::defaultDeviceHeightInPixels);
}

void Game::resizeDevice(int deviceWidthInPixels, int deviceHeightInPixels) {
    layout_ = computeFramebufferLayout(deviceWidthInPixels, deviceHeightInPixels);
    framebuffer_.resize(layout_.widthInPixels, layout_.heightInPixels);
}

void Game::advanceFrame(double realDeltaSeconds) {
    const int stepsToRun = clock_.consumeElapsedTime(realDeltaSeconds);
    for (int stepIndex = 0; stepIndex < stepsToRun; ++stepIndex) {
        simulateStep(clock_.stepSeconds());
    }
    render();
}

void Game::simulateStep(double stepSeconds) { simulatedSeconds_ += stepSeconds; }

void Game::render() {
    // Placeholder: dark gradient with a slow pulsing glow.
    // Just proves the loop and the blit work. Terrain replaces it in Stage 2.
    const double pulsePhase = simulatedSeconds_ / constants::placeholderPulsePeriodSeconds * twoPi;
    const double glowStrength = 0.5 + 0.5 * std::sin(pulsePhase);

    const int width = framebuffer_.width();
    const int height = framebuffer_.height();
    const double lastRow = std::max(height - 1, 1);

    for (int y = 0; y < height; ++y) {
        const double rowPosition = y / lastRow;
        const double glow = rowPosition * rowPosition * (0.35 + 0.25 * glowStrength);
        const PackedColor rowColor = packRgba(lerpChannel(12, 70, glow), lerpChannel(14, 60, glow),
                                              lerpChannel(28, 90, glow));
        for (int x = 0; x < width; ++x) framebuffer_.at(x, y) = rowColor;
    }
}

}  // namespace wl
