// Emscripten entry point. The only C++ file that knows about the browser.
#include <emscripten.h>

#include <memory>

#include "game.h"

namespace {
std::unique_ptr<wl::Game> activeGame;
double lastFrameTimeMs = 0.0;
}  // namespace

// Implemented in web/bridge.js.
EM_JS(void, jsInitBridge, (), { Module.wanderlightInit(); });
EM_JS(void, jsPresentFrame, (const void* pixels, int width, int height, int pixelScale),
      { Module.wanderlightPresent(pixels, width, height, pixelScale); });

// Optional ?seed= and ?zoom= URL parameters, handy for review. Returns the
// fallback when the parameter is missing or not a number.
EM_JS(double, jsReadUrlSeed, (double fallback), {
    const value = Number(new URLSearchParams(location.search).get('seed'));
    return Number.isFinite(value) && value > 0 ? (value >>> 0) : fallback;
});
EM_JS(int, jsReadUrlZoom, (int fallback), {
    const value = parseInt(new URLSearchParams(location.search).get('zoom'), 10);
    return Number.isFinite(value) ? value : fallback;
});

extern "C" {

EMSCRIPTEN_KEEPALIVE void wl_resize(int deviceWidthInPixels, int deviceHeightInPixels) {
    if (activeGame) activeGame->resizeDevice(deviceWidthInPixels, deviceHeightInPixels);
}

// direction: 0 up, 1 down, 2 left, 3 right (matches wl::Direction).
EMSCRIPTEN_KEEPALIVE void wl_set_direction(int direction, int held) {
    if (!activeGame || direction < 0 || direction >= wl::directionCount) return;
    activeGame->setDirectionHeld(static_cast<wl::Direction>(direction), held != 0);
}

EMSCRIPTEN_KEEPALIVE void wl_set_drag(int active, double offsetX, double offsetY) {
    if (activeGame) activeGame->setDrag(active != 0, offsetX, offsetY);
}

EMSCRIPTEN_KEEPALIVE void wl_release_input() {
    if (activeGame) activeGame->releaseAllInput();
}

}  // extern "C"

static void runFrame() {
    const double nowMs = emscripten_get_now();
    const double deltaSeconds = (nowMs - lastFrameTimeMs) / 1000.0;
    lastFrameTimeMs = nowMs;

    activeGame->advanceFrame(deltaSeconds);

    const wl::Framebuffer& framebuffer = activeGame->framebuffer();
    jsPresentFrame(framebuffer.pixels(), framebuffer.width(), framebuffer.height(),
                   activeGame->layout().pixelScale);
}

int main() {
    const auto seed = static_cast<wl::WorldSeed>(jsReadUrlSeed(wl::constants::defaultWorldSeed));
    activeGame = std::make_unique<wl::Game>(seed);
    activeGame->setTileSizeInPixels(jsReadUrlZoom(wl::constants::defaultTileSizeInPixels));

    jsInitBridge();  // hooks up resize and input, and sends the real canvas size
    lastFrameTimeMs = emscripten_get_now();

    // 0 fps means "use requestAnimationFrame", i.e. as fast as the browser allows.
    emscripten_set_main_loop(runFrame, 0, false);
    return 0;
}
