// Tiny pixel-art sprites defined as text, so art lives in code with no asset loading.
#pragma once

#include <initializer_list>
#include <vector>

#include "framebuffer.h"

namespace wl {

struct Sprite {
    int width = 0;
    int height = 0;
    std::vector<PackedColor> pixels;  // alpha 0 means transparent
};

struct SpriteColorKey {
    char symbol;
    PackedColor color;
};

// Each string is one row, each character one pixel. '.' is transparent.
// With outlined set, a 1px dark border is added so sprites read on any ground.
Sprite buildSprite(std::initializer_list<const char*> rows, std::initializer_list<SpriteColorKey> colors,
                   bool outlined);

struct SpriteDrawOptions {
    int scale = 1;
    bool flipHorizontally = false;
    int visibleRows = -1;  // draw only the top N rows (for wading); -1 for all
};

void drawSprite(Framebuffer& framebuffer, const Sprite& sprite, int left, int top,
                const SpriteDrawOptions& options = {});

}  // namespace wl
