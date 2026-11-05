// All the tunable numbers live here so balancing never means hunting through code.
#pragma once

namespace wl::constants {

// --- Simulation timing ---

// Fixed sim step. Same behaviour at 60 Hz and 120 Hz, and repeatable in tests.
inline constexpr double simulationStepSeconds = 1.0 / 60.0;

// Cap on catch-up steps per frame, so a slow frame can't snowball.
inline constexpr int maxSimulationStepsPerFrame = 8;

// Background tabs can report huge deltas when you come back. Clamp them.
inline constexpr double maxFrameDeltaSeconds = 0.25;

// --- Display ---

// We render in software, so cost scales with pixel count. Pick an integer
// upscale that keeps the framebuffer's long edge near this size.
inline constexpr int targetFramebufferLongEdgeInPixels = 640;

// Floor for either framebuffer edge, so tiny windows still work.
inline constexpr int minFramebufferEdgeInPixels = 64;

// Used until the browser tells us the real canvas size.
inline constexpr int defaultDeviceWidthInPixels = 1280;
inline constexpr int defaultDeviceHeightInPixels = 720;

// --- Stage 1 placeholder ---

// Length of one pulse of the placeholder glow. Gone once terrain lands.
inline constexpr double placeholderPulsePeriodSeconds = 6.0;

}  // namespace wl::constants
