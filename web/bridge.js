// Thin JS layer, bundled into index.js with --pre-js.
// Its only jobs: size the canvas and copy the C++ framebuffer onto it.
// Game logic stays in C++.

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
    Module._wl_resize(deviceWidth, deviceHeight);
  };

  new ResizeObserver(resizeCanvas).observe(bridge.canvas);
  // ResizeObserver misses zoom changes, which alter devicePixelRatio.
  window.addEventListener('resize', resizeCanvas);
  resizeCanvas();
};

Module.wanderlightPresent = function (pixelPointer, width, height, pixelScale) {
  const bridge = wanderlightBridge;

  if (!bridge.imageData || bridge.imageData.width !== width || bridge.imageData.height !== height) {
    bridge.frameCanvas.width = width;
    bridge.frameCanvas.height = height;
    bridge.imageData = new ImageData(width, height);
  }

  // Re-read HEAPU8 every frame: memory growth swaps the underlying buffer.
  const byteCount = width * height * 4;
  bridge.imageData.data.set(HEAPU8.subarray(pixelPointer, pixelPointer + byteCount));
  bridge.frameContext.putImageData(bridge.imageData, 0, 0);

  // Changing canvas size resets this, so set it every frame.
  bridge.context.imageSmoothingEnabled = false;
  bridge.context.drawImage(bridge.frameCanvas, 0, 0, width * pixelScale, height * pixelScale);

  if (!bridge.hasPresentedFrame) {
    bridge.hasPresentedFrame = true;
    if (Module.onFirstFrame) Module.onFirstFrame();
  }
};
