// Emscripten entry point. The only C++ file that knows about the browser.
#include <emscripten.h>

#include "game.h"

namespace {
wl::Game* activeGame = nullptr;
double lastFrameTimeMs = 0.0;
}  // namespace

// Implemented in web/bridge.js.
EM_JS(void, jsInitBridge, (), { Module.wanderlightInit(); });
EM_JS(void, jsPresentFrame, (const void* pixels, int width, int height, int pixelScale),
      { Module.wanderlightPresent(pixels, width, height, pixelScale); });

extern "C" EMSCRIPTEN_KEEPALIVE void wl_resize(int deviceWidthInPixels, int deviceHeightInPixels) {
    if (activeGame) activeGame->resizeDevice(deviceWidthInPixels, deviceHeightInPixels);
}

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
    // Static so it outlives main(), which returns straight after setting up the loop.
    static wl::Game game;
    activeGame = &game;

    jsInitBridge();  // hooks up resize and sends the real canvas size
    lastFrameTimeMs = emscripten_get_now();

    // 0 fps means "use requestAnimationFrame", i.e. as fast as the browser allows.
    emscripten_set_main_loop(runFrame, 0, false);
    return 0;
}
