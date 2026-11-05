#include "sprite.h"

#include <algorithm>
#include <cstring>

#include "constants.h"

namespace wl {

namespace {
constexpr char transparentSymbol = '.';

bool isOpaque(PackedColor color) { return (color >> 24) != 0; }
}  // namespace

Sprite buildSprite(std::initializer_list<const char*> rows, std::initializer_list<SpriteColorKey> colors,
                   bool outlined) {
    int artWidth = 0;
    for (const char* row : rows) artWidth = std::max(artWidth, static_cast<int>(std::strlen(row)));
    const int artHeight = static_cast<int>(rows.size());
    const int border = outlined ? 1 : 0;

    Sprite sprite;
    sprite.width = artWidth + border * 2;
    sprite.height = artHeight + border * 2;
    sprite.pixels.assign(static_cast<std::size_t>(sprite.width) * sprite.height, 0);

    int rowIndex = 0;
    for (const char* row : rows) {
        const int rowLength = static_cast<int>(std::strlen(row));
        for (int column = 0; column < rowLength; ++column) {
            if (row[column] == transparentSymbol) continue;
            for (const SpriteColorKey& key : colors) {
                if (key.symbol == row[column]) {
                    sprite.pixels[(rowIndex + border) * sprite.width + column + border] = key.color;
                    break;
                }
            }
        }
        ++rowIndex;
    }

    if (outlined) {
        const auto& outline = constants::spriteOutlineColor;
        const PackedColor outlineColor = packRgba(outline.red, outline.green, outline.blue);
        const std::vector<PackedColor> original = sprite.pixels;
        const auto opaqueAt = [&](int x, int y) {
            return x >= 0 && y >= 0 && x < sprite.width && y < sprite.height &&
                   isOpaque(original[y * sprite.width + x]);
        };
        // Any empty pixel touching a filled one (not diagonally) becomes outline.
        for (int y = 0; y < sprite.height; ++y) {
            for (int x = 0; x < sprite.width; ++x) {
                if (opaqueAt(x, y)) continue;
                if (opaqueAt(x - 1, y) || opaqueAt(x + 1, y) || opaqueAt(x, y - 1) || opaqueAt(x, y + 1)) {
                    sprite.pixels[y * sprite.width + x] = outlineColor;
                }
            }
        }
    }
    return sprite;
}

void drawSprite(Framebuffer& framebuffer, const Sprite& sprite, int left, int top,
                const SpriteDrawOptions& options) {
    const int scale = std::max(options.scale, 1);
    const int rowsToDraw =
        options.visibleRows < 0 ? sprite.height : std::min(options.visibleRows, sprite.height);

    for (int spriteY = 0; spriteY < rowsToDraw; ++spriteY) {
        for (int spriteX = 0; spriteX < sprite.width; ++spriteX) {
            const int sourceX = options.flipHorizontally ? sprite.width - 1 - spriteX : spriteX;
            const PackedColor color = sprite.pixels[spriteY * sprite.width + sourceX];
            if (!isOpaque(color)) continue;
            for (int offsetY = 0; offsetY < scale; ++offsetY) {
                for (int offsetX = 0; offsetX < scale; ++offsetX) {
                    framebuffer.setPixelClipped(left + spriteX * scale + offsetX,
                                                top + spriteY * scale + offsetY, color);
                }
            }
        }
    }
}

}  // namespace wl
