# Wanderlight

An endless, quiet, procedurally generated planet to wander. Written in C++, compiled to WebAssembly, deployed to GitHub Pages.

Work in progress. The full README arrives in the final stage.

## Build

### Native tests

```sh
cmake -S . -B build-native -DCMAKE_BUILD_TYPE=Release
cmake --build build-native --parallel
ctest --test-dir build-native --output-on-failure
```

### Web

Needs the [Emscripten SDK](https://emscripten.org/docs/getting_started/downloads.html) on your path.

```sh
emcmake cmake -S . -B build-web -DCMAKE_BUILD_TYPE=Release
cmake --build build-web --parallel
python -m http.server -d dist 8000
```

Then open http://localhost:8000. Opening `index.html` straight from disk won't work because browsers block loading `.wasm` over `file://`.

Two URL parameters help while developing: `?seed=42` picks a world, and `?zoom=1` (1 to 16 pixels per tile, default 8) zooms out to see continent shapes.

## Layout

| Path | What it holds |
| --- | --- |
| `src/` | Game core (portable C++) plus `main.cpp`, the only Emscripten file |
| `src/constants.h` | Every tunable number |
| `src/noise.*` | Simplex noise and fBm, written from scratch |
| `src/world.*` | Seed, biome lookup, coordinates, chunk generation |
| `src/renderer.*` | Draws terrain into the framebuffer |
| `web/shell.html` | HTML page: loader, fullscreen button, dark theme |
| `web/bridge.js` | Thin JS layer: canvas sizing and framebuffer blit |
| `tests/` | Catch2 unit tests, native only |
| `.github/workflows/deploy.yml` | Test, build and deploy on every push to `main` |
