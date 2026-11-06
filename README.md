# Wanderlight

An endless, quiet alien planet to explore. Every step reveals new ground, generated on the fly from a single number, and the same number always grows the same planet. There is no combat and no way to lose: just canopy wilds, dust basins, ice sheets and coastlines, the odd lone tower or glowing crystal grove, and a day that slowly turns to night under one to three moons.

Written in C++17, compiled to WebAssembly, drawn pixel by pixel into its own framebuffer and deployed to GitHub Pages on every push.


**[Play it](https://taylare.github.io/planet-explorer/)**


## What's in it

- An infinite world streamed in chunks around you, with ten biomes
- Every seed is its own planet: a generated name, a colour palette (the trees and seas change colour too), gravity, day length, one to three moons and sometimes an aurora
- Trees, rocks, flowers, ruins and campfires that suit each biome
- Rare landmarks to find: lone towers, crystal groves and hot springs
- Deer, rabbits, crabs, lizards, mountain goats and snow foxes that wander, rest and shy away from you
- A day and night cycle, with headlamp light, flickering campfires, and stars and moons reflected in the water
- Rain over wet ground, snow over cold ground and dust storms over hot, dry ground, drifting in as weather fronts. Dust takes its colour from the planet's desert, and heavy rain brings lightning
- Meteor showers on about a third of nights, reflected brightly in the water
- Footprints in sand and snow, ripples when you wade
- A minimap that only fills in where you've been
- A field scanner: hold it near an alien animal to log it. Holding it also makes you creep, so animals stay calm long enough to scan
- Scannable landmarks: towers light a beacon that reveals the map, crystal groves give research samples, and ruins decode a story about the survey crew who came before you
- Your lander, parked at the start. Suit power drains as you explore (faster at night, on cold ground and in rain or snow) and refills at the ship, a campfire or a hot spring. With a flat suit you slow down and your lamp dims, and you can beam back to the ship
- Three suit upgrades (scanner, headlamp, power cell) bought at the ship with research samples
- A star chart: once half a planet is surveyed, launch from the ship to one of three other planets. Your suit, upgrades and samples come with you; the map and journal stay behind
- A journal of the first time you found each biome, landmark and species, with coordinates, and a survey percentage for the whole planet
- Automatic saving in your browser, shareable seeds and a "new world" button

## Controls

| Action | Desktop | Phone or tablet |
| --- | --- | --- |
| Walk | WASD or arrow keys | Drag on the left half: a joystick appears under your thumb |
| Walk toward a spot | Hold the mouse button there | Hold a finger on the right half |
| Scan animals | Hold E | Hold the scan button, bottom left |
| Journal | J, or the book button | Book button |
| Ship and upgrades | T, or the rocket button | Rocket button |
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
| `src/planet.*` | Per-seed planet identity: palette, sky tint, moons, aurora and stats |
| `src/props.*` | Trees, rocks, flowers, ruins, campfires and rare landmarks |
| `src/chunk_cache.*` | Chunk streaming, time budget and LRU eviction |
| `src/entities.*` | The player and wildlife steering |
| `src/weather.*`, `src/day_night.*` | Weather fronts, time of day and colour grading |
| `src/sky_events.*` | Meteor showers and lightning, as pure functions of seed and time |
| `src/effects.*` | Footprints and ripples |
| `src/exploration.*`, `src/journal.*` | Minimap fog of war and discoveries |
| `src/scanner.*` | The field scanner: target choice, scan progress |
| `src/landmark_actions.*`, `src/lore.*` | What scanning a landmark does, and the story fragments |
| `src/suit_power.*`, `src/upgrades.*` | Suit power rules and the three upgrades |
| `src/star_chart.*` | The three launch destinations for a planet |
| `src/save_state.*` | Save format for localStorage (version 4; older saves still load) |
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
- planets: the same seed gives the same planet, stats stay in range, palettes recolour the terrain
- props and landmarks, wildlife, weather, day and night, input, and rendering

## What I learned

- **Determinism takes discipline.** Every random choice has to come from the seed and a position, never from "whatever was generated before". Once that held, chunks could be generated in any order, thrown away and regenerated, and saves only needed a handful of values.
- **Negative coordinates are where bugs hide.** In C++, `/` and `%` round toward zero, so tile -1 lands in chunk 0 unless you floor-divide. The seam tests either side of zero caught exactly that kind of mistake.
- **fBm is mostly tuning.** The maths is a short loop, but getting continents, coastlines and climate bands to feel right took previews of several seeds and a lot of small threshold changes.
- **A time budget beats raw speed.** Spreading chunk generation across frames mattered more for smoothness than making any single chunk faster.
- **Software rendering is affordable at a low resolution.** A 640 pixel framebuffer is cheap enough for per-pixel lighting, shimmer and weather, and the chunky look suits the quiet mood.
- **Keep the browser at arm's length.** Putting everything except canvas, input and storage in portable C++ meant almost every system could be unit tested natively, without a browser.
- **Effects that are a function of time need no saving.** Meteor showers, lightning and each planet's look are all worked out from the seed and the clock, so they cost no save data and a test can ask about any moment directly.
- **Tests find bugs you would never see by playing.** A footprint appeared two seconds after the player stopped, because easing to a halt left a tiny tail of movement that crossed one more stride mark. It only showed up in a test that waited out the footprints.
- **Run the build early.** Several small mistakes, like a missing `#include` or an unexported `malloc`, only surfaced once CI compiled the code. Building every stage as soon as it is written would have found them one at a time.
