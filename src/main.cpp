// Emscripten entry point. The only C++ file that knows about the browser.
// It owns the Game, forwards input to it, presents frames, and passes
// saves and UI events to the thin JS layer in web/bridge.js.
#include <emscripten.h>
#include <emscripten/em_js.h>

#include <cstdlib>
#include <memory>
#include <optional>
#include <string>

#include "game.h"
#include "save_state.h"

// Pull in Emscripten's string helpers for the JS below and in bridge.js.
EM_JS_DEPS(wanderlight, "$UTF8ToString,$stringToNewUTF8");

namespace {
std::unique_ptr<wl::Game> activeGame;
double lastFrameTimeMs = 0.0;
int lastDeviceWidth = 0;
int lastDeviceHeight = 0;
int startingTileSize = wl::constants::defaultTileSizeInPixels;
std::string journalJsonBuffer;  // kept alive so JS can read it after we return
}  // namespace

// --- Calls into web/bridge.js ---
EM_JS(void, jsInitBridge, (), { Module.wanderlightInit(); });
EM_JS(void, jsPresentFrame, (const void* pixels, int width, int height, int pixelScale),
      { Module.wanderlightPresent(pixels, width, height, pixelScale); });
EM_JS(char*, jsLoadSave, (), {
    const text = Module.wanderlightLoadSave();
    return text ? stringToNewUTF8(text) : 0;  // C++ frees it
});
EM_JS(void, jsStoreSave, (const char* text), { Module.wanderlightStoreSave(UTF8ToString(text)); });
EM_JS(void, jsShowBanner, (const char* text, int isDiscovery),
      { Module.wanderlightBanner(UTF8ToString(text), isDiscovery !== 0); });
EM_JS(void, jsJournalChanged, (), { Module.wanderlightJournalChanged(); });
EM_JS(void, jsWorldChanged, (double seed), { Module.wanderlightWorldChanged(seed); });

// Optional URL parameters, handy for review. Negative means "not set".
EM_JS(double, jsReadUrlSeed, (), {
    const raw = new URLSearchParams(location.search).get('seed');
    const value = raw === null ? NaN : Number(raw);
    return Number.isFinite(value) && value >= 0 ? (value >>> 0) : -1;
});
EM_JS(int, jsReadUrlZoom, (int fallback), {
    const value = parseInt(new URLSearchParams(location.search).get('zoom'), 10);
    return Number.isFinite(value) ? value : fallback;
});
// ?time= is the time of day, 0 to 1 (0 midnight, 0.5 noon).
EM_JS(double, jsReadUrlTimeOfDay, (), {
    const raw = new URLSearchParams(location.search).get('time');
    const value = raw === null ? NaN : Number(raw);
    return Number.isFinite(value) ? value : -1;
});

static std::optional<wl::SaveState> loadSavedState() {
    char* text = jsLoadSave();
    if (!text) return std::nullopt;
    std::optional<wl::SaveState> state = wl::parseSaveState(text);
    std::free(text);
    return state;
}

static void saveNow() {
    if (!activeGame) return;
    jsStoreSave(wl::serializeSaveState(activeGame->makeSaveState()).c_str());
}

static void startWorld(wl::WorldSeed seed, const std::optional<wl::SaveState>& save) {
    activeGame = std::make_unique<wl::Game>(seed);
    activeGame->setTileSizeInPixels(startingTileSize);
    if (save) activeGame->applySaveState(*save);
    if (lastDeviceWidth > 0) activeGame->resizeDevice(lastDeviceWidth, lastDeviceHeight);
    jsWorldChanged(seed);
    jsJournalChanged();
}

extern "C" {

EMSCRIPTEN_KEEPALIVE void wl_resize(int deviceWidthInPixels, int deviceHeightInPixels) {
    lastDeviceWidth = deviceWidthInPixels;
    lastDeviceHeight = deviceHeightInPixels;
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

EMSCRIPTEN_KEEPALIVE void wl_save_now() { saveNow(); }

EMSCRIPTEN_KEEPALIVE double wl_get_seed() { return activeGame ? activeGame->seed() : 0; }

// Starts a fresh world with this seed. The old progress is replaced: there's one save slot.
EMSCRIPTEN_KEEPALIVE void wl_new_world(double seed) {
    startWorld(static_cast<wl::WorldSeed>(seed), std::nullopt);
    saveNow();
}

// JSON array of journal entries. The pointer stays valid until the next call.
EMSCRIPTEN_KEEPALIVE const char* wl_journal_json() {
    journalJsonBuffer = activeGame ? activeGame->journal().toJson() : "[]";
    return journalJsonBuffer.c_str();
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

    for (const wl::GameEvent& event : activeGame->takeEvents()) {
        switch (event.type) {
            case wl::GameEventType::BiomeEntered: jsShowBanner(event.text.c_str(), 0); break;
            case wl::GameEventType::Discovery: jsShowBanner(event.text.c_str(), 1); break;
            case wl::GameEventType::JournalChanged: jsJournalChanged(); break;
        }
    }
    if (activeGame->takeSaveRequest()) saveNow();
}

int main() {
    startingTileSize = jsReadUrlZoom(wl::constants::defaultTileSizeInPixels);

    // A ?seed= in the URL wins, unless the save is already for that seed.
    const std::optional<wl::SaveState> save = loadSavedState();
    const double urlSeed = jsReadUrlSeed();
    if (urlSeed >= 0.0 && (!save || save->seed != static_cast<wl::WorldSeed>(urlSeed))) {
        startWorld(static_cast<wl::WorldSeed>(urlSeed), std::nullopt);
    } else if (save) {
        startWorld(save->seed, save);
    } else {
        startWorld(wl::constants::defaultWorldSeed, std::nullopt);
    }

    const double urlTimeOfDay = jsReadUrlTimeOfDay();
    if (urlTimeOfDay >= 0.0) activeGame->setTimeOfDay(urlTimeOfDay);

    jsInitBridge();  // hooks up resize and input, and sends the real canvas size
    lastFrameTimeMs = emscripten_get_now();

    // 0 fps means "use requestAnimationFrame", i.e. as fast as the browser allows.
    emscripten_set_main_loop(runFrame, 0, false);
    return 0;
}
