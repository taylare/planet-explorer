#include "framebuffer.h"

#include <algorithm>

#include "constants.h"

namespace wl {

namespace {
int divideRoundingUp(int numerator, int denominator) {
    return (numerator + denominator - 1) / denominator;
}
}  // namespace

FramebufferLayout computeFramebufferLayout(int deviceWidthInPixels, int deviceHeightInPixels) {
    const int deviceWidth = std::max(deviceWidthInPixels, 1);
    const int deviceHeight = std::max(deviceHeightInPixels, 1);
    const int deviceLongEdge = std::max(deviceWidth, deviceHeight);

    FramebufferLayout layout;
    layout.pixelScale =
        std::max(1, divideRoundingUp(deviceLongEdge, constants::targetFramebufferLongEdgeInPixels));
    layout.widthInPixels = std::max(constants::minFramebufferEdgeInPixels,
                                    divideRoundingUp(deviceWidth, layout.pixelScale));
    layout.heightInPixels = std::max(constants::minFramebufferEdgeInPixels,
                                     divideRoundingUp(deviceHeight, layout.pixelScale));
    return layout;
}

void Framebuffer::resize(int widthInPixels, int heightInPixels) {
    width_ = std::max(widthInPixels, 1);
    height_ = std::max(heightInPixels, 1);
    pixels_.assign(static_cast<std::size_t>(width_) * height_, packRgba(0, 0, 0));
}

void Framebuffer::clear(PackedColor color) { std::fill(pixels_.begin(), pixels_.end(), color); }

void Framebuffer::setPixelClipped(int x, int y, PackedColor color) {
    if (x < 0 || y < 0 || x >= width_ || y >= height_) return;
    at(x, y) = color;
}

}  // namespace wl
