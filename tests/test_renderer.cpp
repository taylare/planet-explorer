#include <catch2/catch_test_macros.hpp>
#include <cstdlib>
#include <memory>
#include <unordered_map>

#include "constants.h"
#include "planet.h"
#include "renderer.h"

using namespace wl;

namespace {
class MapChunkSource : public ChunkSource {
public:
    const Chunk* findChunk(ChunkCoord coord) const override {
        const auto found = chunks.find(coord);
        return found == chunks.end() ? nullptr : found->second.get();
    }
    std::unordered_map<ChunkCoord, std::unique_ptr<Chunk>, ChunkCoordHash> chunks;
};

void fillAroundOrigin(MapChunkSource& source, const World& world) {
    for (int chunkY = -2; chunkY <= 1; ++chunkY) {
        for (int chunkX = -2; chunkX <= 1; ++chunkX) {
            source.chunks[{chunkX, chunkY}] = world.generateChunk({chunkX, chunkY});
        }
    }
}

int brightness(PackedColor pixel) {
    return static_cast<int>((pixel & 0xff) + ((pixel >> 8) & 0xff) + ((pixel >> 16) & 0xff));
}

// Average brightness of a square of pixels.
double averageBrightness(const Framebuffer& framebuffer, int left, int top, int size) {
    double total = 0.0;
    for (int y = top; y < top + size; ++y) {
        for (int x = left; x < left + size; ++x) total += brightness(framebuffer.pixels()[y * framebuffer.width() + x]);
    }
    return total / (size * size);
}

SceneView sceneAt(const MapChunkSource& source, double timeOfDay) {
    SceneView scene;
    scene.chunks = &source;
    scene.lighting = lightingAt(timeOfDay);
    return scene;
}
}  // namespace

TEST_CASE("visible tile bounds cover the whole framebuffer", "[renderer]") {
    Camera camera;
    camera.centerTileX = -10.25;
    camera.centerTileY = 3.75;
    camera.tileSizeInPixels = 8;
    const TileBounds bounds = visibleTileBounds(camera, 640, 360);
    // 640 pixels at 8 per tile is 80 tiles, plus one partial when not aligned.
    REQUIRE(bounds.maxTileX - bounds.minTileX + 1 >= 80);
    REQUIRE(bounds.maxTileY - bounds.minTileY + 1 >= 45);
    REQUIRE(bounds.minTileX <= -10 - 40);
    REQUIRE(bounds.maxTileX >= -10 + 39);
}

TEST_CASE("missing chunks draw as the unloaded colour", "[renderer]") {
    Framebuffer framebuffer;
    framebuffer.resize(64, 64);
    Renderer renderer;
    const MapChunkSource emptySource;
    renderer.drawTerrain(framebuffer, Camera{}, emptySource);

    // Grain still applies, so check brightness is close rather than exact.
    const PackedColor pixel = framebuffer.at(10, 10);
    REQUIRE(std::abs(static_cast<int>(pixel & 0xff) - constants::unloadedColor.red) <= 1);
}

TEST_CASE("terrain fills every pixel with an opaque colour", "[renderer]") {
    const World world(4);
    MapChunkSource source;
    fillAroundOrigin(source, world);

    Framebuffer framebuffer;
    framebuffer.resize(200, 120);
    framebuffer.clear(packRgba(0, 0, 0, 0));
    Renderer renderer;
    Camera camera;
    camera.tileSizeInPixels = 4;
    renderer.drawTerrain(framebuffer, camera, source);

    for (int y = 0; y < framebuffer.height(); ++y) {
        for (int x = 0; x < framebuffer.width(); ++x) {
            REQUIRE((framebuffer.at(x, y) >> 24) == 255u);
        }
    }
}

TEST_CASE("night is darker than noon, and the lantern lights the player's surroundings", "[renderer]") {
    const World world(4);
    MapChunkSource source;
    fillAroundOrigin(source, world);
    Renderer renderer;

    Framebuffer noon;
    noon.resize(160, 160);
    renderer.drawFrame(noon, sceneAt(source, 0.5));

    Framebuffer midnight;
    midnight.resize(160, 160);
    renderer.drawFrame(midnight, sceneAt(source, 0.0));

    constexpr int patch = 16;
    const double noonCorner = averageBrightness(noon, 0, 0, patch);
    const double midnightCorner = averageBrightness(midnight, 0, 0, patch);
    REQUIRE(midnightCorner < noonCorner * 0.6);

    // The player sits in the middle with a lantern; the middle should be
    // brighter, relative to noon, than a far corner.
    const int centre = 80 - patch / 2;
    const double midnightCentre = averageBrightness(midnight, centre, centre, patch);
    const double noonCentre = averageBrightness(noon, centre, centre, patch);
    REQUIRE(midnightCentre / noonCentre > midnightCorner / noonCorner);
}

TEST_CASE("full frames are fully opaque at any time of day", "[renderer]") {
    const World world(11);
    MapChunkSource source;
    fillAroundOrigin(source, world);
    Renderer renderer;
    Framebuffer framebuffer;
    framebuffer.resize(120, 90);

    for (double timeOfDay : {0.0, 0.25, 0.5, 0.75}) {
        SceneView scene = sceneAt(source, timeOfDay);
        scene.timeSeconds = timeOfDay * 100.0;
        renderer.drawFrame(framebuffer, scene);
        for (int index = 0; index < framebuffer.width() * framebuffer.height(); ++index) {
            REQUIRE((framebuffer.pixels()[index] >> 24) == 255u);
        }
    }
}

TEST_CASE("the planet palette recolours the terrain", "[renderer][planet]") {
    // One patch of shallow water, so the colour on screen is the palette's water colour.
    MapChunkSource source;
    for (int chunkY = -1; chunkY <= 0; ++chunkY) {
        for (int chunkX = -1; chunkX <= 0; ++chunkX) {
            auto chunk = std::make_unique<Chunk>();
            chunk->coord = {chunkX, chunkY};
            chunk->biomes.fill(Biome::ShallowWater);
            chunk->elevations.fill(0.40f);
            source.chunks[chunk->coord] = std::move(chunk);
        }
    }

    PlanetProfile planet = makePlanetProfile(3);
    planet.palette.shallowWater = {200, 10, 10};
    planet.palette.deepWater = {200, 10, 10};

    Renderer renderer;
    Framebuffer framebuffer;
    framebuffer.resize(64, 64);
    SceneView scene = sceneAt(source, 0.5);
    scene.camera.tileSizeInPixels = 4;
    scene.planet = &planet;
    renderer.drawFrame(framebuffer, scene);

    const PackedColor pixel = framebuffer.at(5, 5);
    REQUIRE((pixel & 0xff) > 150);
    REQUIRE(((pixel >> 8) & 0xff) < 60);
}

TEST_CASE("moons and the aurora only appear at night", "[renderer][planet]") {
    const World world(4);
    MapChunkSource source;
    fillAroundOrigin(source, world);

    PlanetProfile lively = makePlanetProfile(4);
    lively.moonCount = constants::maxMoons;
    lively.auroraStrength = 1.0;
    PlanetProfile plain = lively;
    plain.moonCount = 0;
    plain.auroraStrength = 0.0;

    Renderer renderer;
    const auto countDifferences = [&](double timeOfDay) {
        Framebuffer withSky;
        withSky.resize(160, 120);
        Framebuffer withoutSky;
        withoutSky.resize(160, 120);

        SceneView sceneWithSky = sceneAt(source, timeOfDay);
        sceneWithSky.planet = &lively;
        sceneWithSky.timeSeconds = 12.0;
        renderer.drawFrame(withSky, sceneWithSky);

        SceneView sceneWithoutSky = sceneAt(source, timeOfDay);
        sceneWithoutSky.planet = &plain;
        sceneWithoutSky.timeSeconds = 12.0;
        renderer.drawFrame(withoutSky, sceneWithoutSky);

        int differences = 0;
        for (int index = 0; index < 160 * 120; ++index) {
            if (withSky.pixels()[index] != withoutSky.pixels()[index]) ++differences;
        }
        return differences;
    };

    REQUIRE(countDifferences(0.5) == 0);    // noon: nothing to see
    REQUIRE(countDifferences(0.0) > 100);   // midnight: the aurora ribbon is drawn
}

TEST_CASE("the scan reticle draws only while there is a target, and fills with progress", "[renderer][scanner]") {
    const World world(4);
    MapChunkSource source;
    fillAroundOrigin(source, world);
    Renderer renderer;

    // Place the target a couple of tiles from the player so the reticle lands on screen.
    ScanStatus status;
    status.hasTarget = true;
    status.targetPosition = {2.0, 0.0};
    status.progress = 0.0;

    const auto drawWith = [&](const ScanStatus* scan) {
        Framebuffer framebuffer;
        framebuffer.resize(160, 160);
        SceneView scene = sceneAt(source, 0.5);
        scene.scan = scan;
        renderer.drawFrame(framebuffer, scene);
        return framebuffer;
    };
    const auto countPixelsOfColor = [](const Framebuffer& framebuffer, const constants::Rgb8& color) {
        const PackedColor wanted = packRgba(color.red, color.green, color.blue);
        int count = 0;
        for (int index = 0; index < framebuffer.width() * framebuffer.height(); ++index) {
            if (framebuffer.pixels()[index] == wanted) ++count;
        }
        return count;
    };

    const Framebuffer withoutScan = drawWith(nullptr);
    const Framebuffer emptyBar = drawWith(&status);
    status.progress = 1.0;
    const Framebuffer fullBar = drawWith(&status);
    status.hasTarget = false;
    const Framebuffer noTarget = drawWith(&status);

    // Brackets plus any filled part of the bar use the scanner colour.
    const int baseline = countPixelsOfColor(withoutScan, constants::scannerColor);
    const int bracketsOnly = countPixelsOfColor(emptyBar, constants::scannerColor);
    const int bracketsAndBar = countPixelsOfColor(fullBar, constants::scannerColor);
    REQUIRE(bracketsOnly > baseline);
    REQUIRE(bracketsAndBar > bracketsOnly);
    REQUIRE(countPixelsOfColor(noTarget, constants::scannerColor) == baseline);
}

TEST_CASE("the lander appears only when the scene has one, at its own position", "[renderer][ship]") {
    const World world(4);
    MapChunkSource source;
    fillAroundOrigin(source, world);
    Renderer renderer;

    const auto draw = [&](bool hasShip, double shipX) {
        Framebuffer framebuffer;
        framebuffer.resize(160, 160);
        SceneView scene = sceneAt(source, 0.5);
        scene.hasShip = hasShip;
        scene.shipFoot = {shipX, 4.0};
        renderer.drawFrame(framebuffer, scene);
        return framebuffer;
    };
    const auto countDifferences = [](const Framebuffer& a, const Framebuffer& b) {
        int differences = 0;
        for (int index = 0; index < a.width() * a.height(); ++index) {
            if (a.pixels()[index] != b.pixels()[index]) ++differences;
        }
        return differences;
    };

    const Framebuffer noShip = draw(false, 2.0);
    const Framebuffer shipNear = draw(true, 2.0);
    const Framebuffer shipElsewhere = draw(true, -2.0);
    // The sprite is 15 by 12 pixels plus an outline, so it changes a good number of pixels.
    REQUIRE(countDifferences(noShip, shipNear) > 60);
    REQUIRE(countDifferences(shipNear, shipElsewhere) > 60);
}

TEST_CASE("a weaker headlamp lights less, and a wider one lights more", "[renderer][power]") {
    const World world(4);
    MapChunkSource source;
    fillAroundOrigin(source, world);

    const auto litBrightness = [&](float strength, float radiusScale) {
        Renderer renderer;
        Framebuffer framebuffer;
        framebuffer.resize(160, 160);
        SceneView scene = sceneAt(source, 0.0);
        scene.headlampStrength = strength;
        scene.headlampRadiusScale = radiusScale;
        renderer.drawFrame(framebuffer, scene);
        // A ring of ground a little way from the player, at the edge of the base lamp pool.
        return averageBrightness(framebuffer, 80 + 30, 80 - 8, 16);
    };

    const double normal = litBrightness(1.0f, 1.0f);
    const double dim = litBrightness(constants::emptyHeadlampStrength, 1.0f);
    const double wide = litBrightness(1.0f, 1.75f);
    REQUIRE(dim < normal);
    REQUIRE(wide > normal);
}
