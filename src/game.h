// The whole game, with no browser code in sight.
// main.cpp drives it on the web; tests drive it natively.
#pragma once

#include "fixed_timestep.h"
#include "framebuffer.h"

namespace wl {

class Game {
public:
    Game();

    // Canvas size in physical device pixels.
    void resizeDevice(int deviceWidthInPixels, int deviceHeightInPixels);

    // Run however many sim steps are due, then draw one frame.
    void advanceFrame(double realDeltaSeconds);

    const Framebuffer& framebuffer() const { return framebuffer_; }
    const FramebufferLayout& layout() const { return layout_; }
    double simulatedSeconds() const { return simulatedSeconds_; }

private:
    void simulateStep(double stepSeconds);
    void render();

    FixedTimestepClock clock_;
    FramebufferLayout layout_;
    Framebuffer framebuffer_;
    double simulatedSeconds_ = 0.0;
};

}  // namespace wl
