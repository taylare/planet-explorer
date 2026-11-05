// Thin JS layer, bundled into index.js with --pre-js.
// Its jobs: size the canvas, copy the C++ framebuffer onto it, forward input,
// store saves in localStorage, and pass UI events to the page (shell.html).
// Game logic stays in C++.

var wanderlightSaveKey = 'wanderlight.save';

// The page's UI, if it has one. Set up by shell.html as window.wanderlightUi.
function wanderlightCallUi(method, ...args) {
  const ui = window.wanderlightUi;
  if (ui && typeof ui[method] === 'function') ui[method](...args);
}

// localStorage can throw (private mode, storage full, blocked cookies). The
// game should keep running either way, just without saving.
Module.wanderlightLoadSave = function () {
  try {
    return window.localStorage.getItem(wanderlightSaveKey);
  } catch (error) {
    return null;
  }
};

Module.wanderlightStoreSave = function (text) {
  try {
    window.localStorage.setItem(wanderlightSaveKey, text);
  } catch (error) {
    // Nothing useful to do; progress just won't persist this time.
  }
};

// mode: 0 plain, 1 discovery, 2 landing on a planet.
Module.wanderlightBanner = function (text, mode) {
  wanderlightCallUi('showBanner', text, mode);
};

Module.wanderlightJournalChanged = function () {
  wanderlightCallUi('setJournal', JSON.parse(UTF8ToString(Module._wl_journal_json())));
};

Module.wanderlightWorldChanged = function (seed) {
  wanderlightCallUi('setSeed', seed);
};

// The planet's name and stats, as JSON text, whenever a world starts.
Module.wanderlightPlanetChanged = function (json) {
  wanderlightCallUi('setPlanet', JSON.parse(json));
};

// Save when the tab is hidden or closed, so a quick exit doesn't lose the last few seconds.
window.addEventListener('pagehide', () => {
  if (Module._wl_save_now) Module._wl_save_now();
});
document.addEventListener('visibilitychange', () => {
  if (document.hidden && Module._wl_save_now) Module._wl_save_now();
});

var wanderlightBridge = {
  canvas: null,
  context: null,
  frameCanvas: null, // holds the small framebuffer before upscaling
  frameContext: null,
  imageData: null,
  hasPresentedFrame: false,
};

Module.wanderlightInit = function () {
  const bridge = wanderlightBridge;
  bridge.canvas = document.getElementById('canvas');
  bridge.context = bridge.canvas.getContext('2d', { alpha: false });
  bridge.frameCanvas = document.createElement('canvas');
  bridge.frameContext = bridge.frameCanvas.getContext('2d');

  // Size the canvas in physical pixels so it stays sharp on high-DPI screens.
  const resizeCanvas = () => {
    const devicePixelRatio = window.devicePixelRatio || 1;
    const bounds = bridge.canvas.getBoundingClientRect();
    const deviceWidth = Math.max(1, Math.round(bounds.width * devicePixelRatio));
    const deviceHeight = Math.max(1, Math.round(bounds.height * devicePixelRatio));
    if (bridge.canvas.width !== deviceWidth || bridge.canvas.height !== deviceHeight) {
      bridge.canvas.width = deviceWidth;
      bridge.canvas.height = deviceHeight;
    }
    // CSS size too, so C++ can map a touch point onto the world.
    Module._wl_resize(deviceWidth, deviceHeight, bounds.width, bounds.height);
  };

  new ResizeObserver(resizeCanvas).observe(bridge.canvas);
  // ResizeObserver misses zoom changes, which alter devicePixelRatio.
  window.addEventListener('resize', resizeCanvas);
  resizeCanvas();

  wanderlightAttachInput(bridge.canvas);
};

// Forwards raw key and drag state to C++. No movement logic here.
function wanderlightAttachInput(canvas) {
  // Values match wl::Direction in src/input.h.
  const directionForKey = {
    KeyW: 0, ArrowUp: 0,
    KeyS: 1, ArrowDown: 1,
    KeyA: 2, ArrowLeft: 2,
    KeyD: 3, ArrowRight: 3,
  };

  const onKey = (event, held) => {
    const direction = directionForKey[event.code];
    if (direction === undefined) return;
    // Typing a seed into a text box shouldn't walk the player around.
    const target = event.target;
    if (held && target && (target.tagName === 'INPUT' || target.tagName === 'TEXTAREA')) return;
    event.preventDefault(); // stop arrows scrolling the page
    Module._wl_set_direction(direction, held ? 1 : 0);
  };
  window.addEventListener('keydown', (event) => onKey(event, true));
  window.addEventListener('keyup', (event) => onKey(event, false));

  // Keys held while switching tabs never get a keyup, so let go of everything.
  window.addEventListener('blur', () => Module._wl_release_input());
  document.addEventListener('visibilitychange', () => {
    if (document.hidden) Module._wl_release_input();
  });

  // Two ways to walk with a pointer:
  //  - touch on the left side: a floating joystick appears under the thumb
  //  - touch on the right side, or hold the mouse anywhere: walk toward that point
  // One pointer steers at a time; extra fingers are ignored.
  const joystickScreenShare = 0.45; // left 45% of the screen is joystick territory
  const joystickRadius = Module._wl_joystick_radius();
  let activePointerId = null;
  let activeMode = null; // 'joystick' or 'follow'
  let joystickStartX = 0;
  let joystickStartY = 0;

  const canvasPoint = (event) => {
    const bounds = canvas.getBoundingClientRect();
    return { x: event.clientX - bounds.left, y: event.clientY - bounds.top };
  };

  canvas.addEventListener('pointerdown', (event) => {
    if (activePointerId !== null) return;
    if (event.pointerType === 'mouse' && event.button !== 0) return;
    activePointerId = event.pointerId;
    canvas.setPointerCapture(event.pointerId);

    const useJoystick = event.pointerType === 'touch' && event.clientX < window.innerWidth * joystickScreenShare;
    if (useJoystick) {
      activeMode = 'joystick';
      joystickStartX = event.clientX;
      joystickStartY = event.clientY;
      Module._wl_set_drag(1, 0, 0);
      wanderlightCallUi('showJoystick', joystickStartX, joystickStartY, joystickRadius);
    } else {
      activeMode = 'follow';
      const point = canvasPoint(event);
      Module._wl_set_follow(1, point.x, point.y);
    }
    wanderlightCallUi('dismissHint');
  });

  canvas.addEventListener('pointermove', (event) => {
    if (event.pointerId !== activePointerId) return;
    if (activeMode === 'joystick') {
      const offsetX = event.clientX - joystickStartX;
      const offsetY = event.clientY - joystickStartY;
      Module._wl_set_drag(1, offsetX, offsetY);
      wanderlightCallUi('moveJoystick', offsetX, offsetY, joystickRadius);
    } else {
      const point = canvasPoint(event);
      Module._wl_set_follow(1, point.x, point.y);
    }
  });

  const endPointer = (event) => {
    if (event.pointerId !== activePointerId) return;
    if (activeMode === 'joystick') {
      Module._wl_set_drag(0, 0, 0);
      wanderlightCallUi('hideJoystick');
    } else {
      Module._wl_set_follow(0, 0, 0);
    }
    activePointerId = null;
    activeMode = null;
  };
  canvas.addEventListener('pointerup', endPointer);
  canvas.addEventListener('pointercancel', endPointer);
  canvas.addEventListener('lostpointercapture', endPointer);

  // A long press would otherwise open the context menu and steal the touch.
  canvas.addEventListener('contextmenu', (event) => event.preventDefault());
}

// ?debug=1 shows frame rate and C++ timings, refreshed twice a second.
var wanderlightDebug = {
  enabled: new URLSearchParams(location.search).get('debug') === '1',
  framesSinceReport: 0,
  lastReportMs: 0,
};

function wanderlightReportDebug() {
  const debug = wanderlightDebug;
  debug.framesSinceReport++;
  const now = performance.now();
  if (now - debug.lastReportMs < 500) return;
  const framesPerSecond = (debug.framesSinceReport * 1000) / (now - debug.lastReportMs);
  debug.framesSinceReport = 0;
  debug.lastReportMs = now;
  const detail = UTF8ToString(Module._wl_debug_text());
  wanderlightCallUi('setDebugText', `${framesPerSecond.toFixed(0)} fps | ${detail}`);
}

Module.wanderlightPresent = function (pixelPointer, width, height, pixelScale) {
  const bridge = wanderlightBridge;

  if (bridge.frameCanvas.width !== width || bridge.frameCanvas.height !== height) {
    bridge.frameCanvas.width = width;
    bridge.frameCanvas.height = height;
  }

  // Wrap the framebuffer in place instead of copying it into a separate
  // ImageData first: one copy per frame (putImageData) instead of two.
  // The wrapper is rebuilt if the size, address or wasm memory changes
  // (memory growth swaps the underlying buffer).
  const memoryBuffer = HEAPU8.buffer;
  if (!bridge.imageData || bridge.imageDataBuffer !== memoryBuffer || bridge.imageDataPointer !== pixelPointer ||
      bridge.imageData.width !== width || bridge.imageData.height !== height) {
    const pixels = new Uint8ClampedArray(memoryBuffer, pixelPointer, width * height * 4);
    bridge.imageData = new ImageData(pixels, width, height);
    bridge.imageDataBuffer = memoryBuffer;
    bridge.imageDataPointer = pixelPointer;
  }
  bridge.frameContext.putImageData(bridge.imageData, 0, 0);

  // Changing canvas size resets this, so set it every frame.
  bridge.context.imageSmoothingEnabled = false;
  bridge.context.drawImage(bridge.frameCanvas, 0, 0, width * pixelScale, height * pixelScale);

  if (!bridge.hasPresentedFrame) {
    bridge.hasPresentedFrame = true;
    if (Module.onFirstFrame) Module.onFirstFrame();
  }
  if (wanderlightDebug.enabled) wanderlightReportDebug();
};
