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
