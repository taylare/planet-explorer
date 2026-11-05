# Wanderlight

An endless, quiet planet to wander. Every step reveals new ground, generated on the fly from a single number, and the same number always grows the same world. There is no combat, no score and no way to lose: just forests, deserts, snowfields and coastlines, the odd lone tower or glowing crystal grove, and a day that slowly turns to night.

Written in C++17, compiled to WebAssembly, drawn pixel by pixel into its own framebuffer and deployed to GitHub Pages on every push.


**[Play it](https://taylare.github.io/planet-explorer/)**


## What's in it

- An infinite world streamed in chunks around you, with ten biomes
- Trees, rocks, flowers, ruins and campfires that suit each biome
- Rare landmarks to find: lone towers, crystal groves and hot springs
- Deer, rabbits, crabs, lizards, mountain goats and snow foxes that wander, rest and shy away from you
- A day and night cycle, with lantern light, flickering campfires and stars reflected in the water
- Rain over wet ground and snow over cold ground, drifting in as weather fronts
- Footprints in sand and snow, ripples when you wade
- A minimap that only fills in where you've been
- A journal of the first time you found each biome and landmark, with coordinates
- Automatic saving in your browser, shareable seeds and a "new world" button

## Controls

| Action | Desktop | Phone or tablet |
| --- | --- | --- |
| Walk | WASD or arrow keys | Drag on the left half: a joystick appears under your thumb |
| Walk toward a spot | Hold the mouse button there | Hold a finger on the right half |
| Journal | J, or the book button | Book button |
| Change world | Globe button | Globe button |

Shallow water slows you down. Deep ocean is too deep to cross.

## How the terrain works

### Noise: smooth randomness

The world starts with **simplex noise**: a function that takes a position and returns a number between -1 and 1. Unlike plain random numbers, nearby positions give nearby values, so you get smooth hills and valleys instead of static. It's written from scratch in [src/noise.cpp](src/noise.cpp).

Instead of the usual 256-entry lookup table, each grid point's direction comes from hashing its coordinates with the world seed. That means the noise never repeats, however far you walk, and a different seed gives a completely different world.

### fBm: detail at every scale

One layer of noise looks like gentle blobs. Real terrain has big shapes and small detail at the same time, so the elevation stacks six layers (called **octaves**) using **fractional Brownian motion**:

- each layer has double the frequency of the last (the **lacunarity**), so its features are half the size
- each layer has half the strength (the **persistence**), so small bumps don't drown out the continents

Continents come from the first octave, coastlines and hills from the middle ones, and small bumps from the last.

### Domain warping: organic shapes

Before reading elevation, each sample position is nudged by two more noise fields. Straight-ish contours bend into winding coastlines, bays and peninsulas. Mountain ranges get an extra **ridged** noise layer (noise folded at zero) that only fades in on high ground, which carves sharp ridgelines.

### Biomes: three maps combined

Each tile has three values between 0 and 1:

- **elevation**, from the warped fBm above
- **moisture**, from its own noise field (following the same warp, so wet and dry areas follow the coasts)
- **temperature**, from a very broad noise field, cooled further at altitude so peaks get snow

A small lookup in [src/world.cpp](src/world.cpp) turns those into one of ten biomes. Elevation decides first (ocean, shallows, beach, mountain, snowy peak), then temperature picks the climate band and moisture picks the biome within it. For example, hot and dry is desert, hot and a bit wetter is savanna, and temperate and wet is forest.

### Chunks: only what you can see

The world is split into **chunks** of 32 by 32 tiles. Each frame the game works out which chunks the screen needs, plus a ring of extra chunks around it, and generates the missing ones nearest first. Generation stops once it has used its time budget for the frame (4 ms), so walking never stutters; the rest waits for the next frame.

Generated chunks live in a cache with a size limit. When the cache is full, the chunk you looked at longest ago is dropped, though never one that's still on screen. If you walk back, it's simply generated again, and it comes out identical because everything is derived from the seed and the chunk's position. Nothing is stored except what you change: where you are, what you've found and which areas you've seen.

## Building

### Native tests

You need CMake 3.20 or newer and a C++17 compiler. Catch2 is downloaded automatically.

```sh
cmake -S . -B build-native -DCMAKE_BUILD_TYPE=Release
cmake --build build-native --parallel
ctest --test-dir build-native --output-on-failure
```

### Web

You also need the [Emscripten SDK](https://emscripten.org/docs/getting_started/downloads.html) on your path. CI uses version 3.1.74.

```sh
emcmake cmake -S . -B build-web -DCMAKE_BUILD_TYPE=Release
cmake --build build-web --parallel
python -m http.server -d dist 8000
```

Then open http://localhost:8000. Opening `index.html` straight from disk won't work, because browsers block loading `.wasm` files over `file://`.

### Deploying

[.github/workflows/deploy.yml](.github/workflows/deploy.yml) runs on every push to `main`: native tests first, then the WebAssembly build, then the Pages deploy. A failing test stops the deploy. To turn it on, go to the repository's **Settings, Pages** and set the source to **GitHub Actions**.

### Handy URL parameters

| Parameter | What it does |
| --- | --- |
| `?seed=42` | Visit a particular world |
| `?zoom=1` | Pixels per tile, 1 to 16 (default 8). 1 shows whole continents |
| `?time=0.85` | Time of day: 0 is midnight, 0.5 is noon |
| `?debug=1` | Frame rate and timings overlay |

Combine them with `&`, for example `?seed=42&time=0.85`.

## Project layout

All game logic is plain C++ with no browser code, so the same code runs in the native tests. [src/main.cpp](src/main.cpp) is the only file that knows about Emscripten.

| Path | What it holds |
| --- | --- |
| `src/constants.h` | Every tunable number, with a note on each |
| `src/noise.*` | Simplex noise and fBm |
| `src/world.*` | Seed, biome lookup, tile and chunk coordinates, chunk generation |
| `src/props.*` | Trees, rocks, flowers, ruins, campfires and rare landmarks |
| `src/chunk_cache.*` | Chunk streaming, time budget and LRU eviction |
| `src/entities.*` | The player and wildlife steering |
| `src/weather.*`, `src/day_night.*` | Weather fronts, time of day and colour grading |
| `src/effects.*` | Footprints and ripples |
| `src/exploration.*`, `src/journal.*` | Minimap fog of war and discoveries |
| `src/save_state.*` | Save format for localStorage |
| `src/renderer.*`, `src/art.*`, `src/sprite.*` | Framebuffer drawing, lighting, minimap and text-defined sprite art |
| `src/performance.*` | Frame timings and automatic quality steps |
| `src/game.*` | Ties it all together with a fixed-timestep loop |
| `web/shell.html` | The page: loader, panels, joystick, dark theme |
| `web/bridge.js` | Thin JS layer: canvas, input, saves, UI events |
| `tests/` | Catch2 unit tests |

## Performance

The game renders into a small framebuffer (its long edge is about 640 pixels) and scales it up with crisp nearest-neighbour filtering. That keeps the cost of a frame the same on a 4K monitor and a phone. The simulation runs in fixed 1/60 s steps, and drawing blends between steps, so it looks smooth at any refresh rate.

If frames run slow for a second and a half, the game drops to a lower internal resolution, and it climbs back after a long run of easy frames. Add `?debug=1` to watch the timings. Chunk generation has its own per-frame time limit, so exploring quickly never causes a hitch.

## Testing

The tests cover:

- the same seed gives identical chunks, in any order
- chunk edges line up exactly with their neighbours, including across zero
- noise stays in range
- every elevation, moisture and temperature combination maps to a biome
- save and load round-trips the full game state
- the cache evicts the least recently used chunks and never one in view
- props and landmarks, wildlife, weather, day and night, input, and rendering

## What I learned

<!-- A draft to edit into your own words. -->

- **Determinism takes discipline.** Every random choice has to come from the seed and a position, never from "whatever was generated before". Once that held, chunks could be generated in any order, thrown away and regenerated, and saves only needed a handful of values.
- **Negative coordinates are where bugs hide.** In C++, `/` and `%` round toward zero, so tile -1 lands in chunk 0 unless you floor-divide. The seam tests either side of zero caught exactly that kind of mistake.
- **fBm is mostly tuning.** The maths is a short loop, but getting continents, coastlines and climate bands to feel right took previews of several seeds and a lot of small threshold changes.
- **A time budget beats raw speed.** Spreading chunk generation across frames mattered more for smoothness than making any single chunk faster.
- **Software rendering is affordable at a low resolution.** A 640 pixel framebuffer is cheap enough for per-pixel lighting, shimmer and weather, and the chunky look suits the quiet mood.
- **Keep the browser at arm's length.** Putting everything except canvas, input and storage in portable C++ meant almost every system could be unit tested natively, without a browser.
