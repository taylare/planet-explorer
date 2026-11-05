// Draws the world into the framebuffer: terrain, props, creatures, the
// player, then lighting and stars.
#pragma once

#include <array>
#include <cstdint>
#include <vector>

#include "art.h"
#include "constants.h"
#include "day_night.h"
#include "effects.h"
#include "entities.h"
#include "exploration.h"
#include "framebuffer.h"
#include "planet.h"
#include "scanner.h"
#include "sky_events.h"
#include "sprite.h"
#include "weather.h"
#include "world.h"

namespace wl {

struct Camera {
    double centerTileX = 0.0;
    double centerTileY = 0.0;
    int tileSizeInPixels = constants::defaultTileSizeInPixels;
};

// World pixel at the framebuffer's top-left corner. Snapped to whole pixels
// so the image doesn't shimmer while the camera moves.
struct ScreenOrigin {
    std::int64_t leftWorldPixel = 0;
    std::int64_t topWorldPixel = 0;
    int tileSizeInPixels = 1;
};

ScreenOrigin screenOriginFor(const Camera& camera, int framebufferWidth, int framebufferHeight);
int worldToScreenX(const ScreenOrigin& origin, double tileX);
int worldToScreenY(const ScreenOrigin& origin, double tileY);

// Inclusive range of tiles touched by the view.
struct TileBounds {
    int minTileX = 0;
    int minTileY = 0;
    int maxTileX = 0;
    int maxTileY = 0;
};

TileBounds visibleTileBounds(const Camera& camera, int framebufferWidth, int framebufferHeight);

// What the renderer needs to know about the player, nothing more.
struct PlayerAppearance {
    double tileX = 0.0;  // feet position
    double tileY = 0.0;
    bool facingLeft = false;
    bool wading = false;
    int walkFrame = 0;  // 0 idle, 1 and 2 alternate while walking
};

struct MinimapMarker {
    double tileX = 0.0;
    double tileY = 0.0;
    PropKind kind = PropKind::Tower;
};

struct MinimapView {
    const Exploration* exploration = nullptr;
    std::vector<MinimapMarker> markers;  // landmarks in revealed chunks
};

// Everything one frame needs. Optional parts are null when absent.
struct SceneView {
    Camera camera;
    const ChunkSource* chunks = nullptr;
    PlayerAppearance player;
    const std::vector<Creature>* creatures = nullptr;
    const TrailEffects* trails = nullptr;
    const WeatherState* weather = nullptr;
    const MinimapView* minimap = nullptr;
    // The lander, drawn where it landed. Off by default so tests without one stay simple.
    bool hasShip = false;
    Vec2 shipFoot;

    // Suit power and upgrades change the headlamp: dimmer when the suit is low, wider when upgraded.
    float headlampStrength = 1.0f;
    float headlampRadiusScale = 1.0f;

    // Sky events. The seed and night number decide the meteor streaks, which are only drawn
    // when meteorShower is set.
    bool meteorShower = false;
    WorldSeed skySeed = 0;
    int nightNumber = 0;
    float lightningFlash = 0.0f;  // 0 to 1, from lightningFlashAt

    const ScanStatus* scan = nullptr;       // reticle while scanning; null draws none
    const PlanetProfile* planet = nullptr;  // palette, moons and aurora; null draws the default look
    double interpolationAlpha = 1.0;
    LightingState lighting;
    double timeSeconds = 0.0;  // sim time; drives animation and effect ages
};

class Renderer {
public:
    Renderer();

    void drawFrame(Framebuffer& framebuffer, const SceneView& scene);

    // Recolours terrain, plants and the minimap. drawFrame calls this by itself
    // when the scene's planet changes.
    void setPalette(const PlanetPalette& palette);

    // Terrain only. Public so tests can check it on its own.
    void drawTerrain(Framebuffer& framebuffer, const Camera& camera, const ChunkSource& chunks,
                     double timeSeconds = 0.0);

private:
    // One sprite waiting to be drawn, sorted by foot height so nearer
    // things overlap further ones.
    struct DrawItem {
        const Sprite* sprite = nullptr;
        int footX = 0;
        int footY = 0;
        int scale = 1;
        bool flip = false;
        bool shadow = true;
        bool wading = false;
    };

    struct PendingLight {
        int x = 0;
        int y = 0;
        const constants::LightSettings* settings = nullptr;
        float strength = 1.0f;     // multiplied into the settings' intensity
        float radiusScale = 1.0f;  // multiplied into the settings' radius
    };

    struct LitRect {
        int minX = 0;
        int minY = 0;
        int maxX = 0;
        int maxY = 0;
    };

    struct Star {
        int x = 0;
        int y = 0;
        float phase = 0.0f;
    };

    void collectProps(Framebuffer& framebuffer, const SceneView& scene, const ScreenOrigin& origin, int scale);
    void collectCreatures(const SceneView& scene, const ScreenOrigin& origin, int scale);
    void drawItem(Framebuffer& framebuffer, const DrawItem& item) const;
    void drawSteam(Framebuffer& framebuffer, double timeSeconds, int scale) const;
    void drawTrails(Framebuffer& framebuffer, const SceneView& scene, const ScreenOrigin& origin, int scale) const;
    void drawWeather(Framebuffer& framebuffer, const SceneView& scene, const ScreenOrigin& origin, int scale) const;
    void drawMinimap(Framebuffer& framebuffer, const SceneView& scene) const;
    void drawScanReticle(Framebuffer& framebuffer, const SceneView& scene, const ScreenOrigin& origin, int scale) const;
    void applyLighting(Framebuffer& framebuffer, const LightingState& lighting, int tileSize);
    void drawStars(Framebuffer& framebuffer, const LightingState& lighting, double timeSeconds);
    void drawMoons(Framebuffer& framebuffer, const PlanetProfile& planet, const LightingState& lighting,
                   double timeSeconds) const;
    void drawAurora(Framebuffer& framebuffer, const PlanetProfile& planet, const LightingState& lighting,
                    double timeSeconds) const;
    void drawDustStorm(Framebuffer& framebuffer, const SceneView& scene, const ScreenOrigin& origin, int scale) const;
    void drawMeteors(Framebuffer& framebuffer, const SceneView& scene) const;
    void drawLightningFlash(Framebuffer& framebuffer, float flash) const;
    void rebuildMinimapPalette();
    void rebuildStarsIfNeeded(int width, int height);

    // Small tiled pattern of brightness steps, indexed by world pixel so it
    // stays put as the camera moves. 64 is big enough that repeats don't show.
    static constexpr int grainPatternSize = 64;
    std::array<std::uint8_t, grainPatternSize * grainPatternSize> grainPattern_{};

    PlanetPalette palette_ = defaultPlanetPalette();
    SpriteLibrary sprites_;
    std::array<PackedColor, biomeCount> minimapPalette_{};

    // How bright the aurora is at each distance from the middle of its band.
    static constexpr int auroraFalloffSteps = 64;
    std::array<float, auroraFalloffSteps> auroraFalloff_{};

    // Scratch buffers kept between frames to avoid allocating every frame.
    std::vector<std::uint8_t> waterMask_;     // 1 where the pixel shows water
    std::vector<int> waterRowOffsets_;        // shimmer offset per row
    std::vector<std::uint16_t> lightBuffer_;  // RGB light per pixel, 256 means +1.0
    std::vector<LitRect> litRects_;           // patches of lightBuffer_ that need wiping
    int lightBufferWidth_ = 0;
    std::vector<DrawItem> drawItems_;
    std::vector<PendingLight> lights_;
    std::vector<std::array<int, 3>> steamSources_;  // x, y, variant
    std::vector<Star> stars_;
    int starsWidth_ = 0;
    int starsHeight_ = 0;
};

}  // namespace wl
