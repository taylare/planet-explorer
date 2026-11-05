// Software framebuffer. Every frame is drawn here, then handed to the canvas.
#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace wl {

using PackedColor = std::uint32_t;

// Bytes land in memory as R, G, B, A on little-endian (wasm, x86, ARM),
// which is what ImageData wants. No conversion needed in JS.
constexpr PackedColor packRgba(std::uint8_t red, std::uint8_t green, std::uint8_t blue,
                               std::uint8_t alpha = 255) {
    return static_cast<PackedColor>(red) | (static_cast<PackedColor>(green) << 8) |
           (static_cast<PackedColor>(blue) << 16) | (static_cast<PackedColor>(alpha) << 24);
}

// Framebuffer size plus the integer factor it gets scaled up by on screen.
struct FramebufferLayout {
    int widthInPixels = 0;
    int heightInPixels = 0;
    int pixelScale = 1;
};

// Integer scale keeps every pixel the same size. Rounds up so we always
// cover the whole canvas.
FramebufferLayout computeFramebufferLayout(int deviceWidthInPixels, int deviceHeightInPixels);

class Framebuffer {
public:
    void resize(int widthInPixels, int heightInPixels);
    void clear(PackedColor color);

    int width() const { return width_; }
    int height() const { return height_; }

    PackedColor* pixels() { return pixels_.data(); }
    const PackedColor* pixels() const { return pixels_.data(); }

    // No bounds check. For hot loops only.
    PackedColor& at(int x, int y) { return pixels_[static_cast<std::size_t>(y) * width_ + x]; }

    void setPixelClipped(int x, int y, PackedColor color);

private:
    int width_ = 0;
    int height_ = 0;
    std::vector<PackedColor> pixels_;
};

}  // namespace wl
