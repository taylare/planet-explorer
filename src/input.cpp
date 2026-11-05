#include "input.h"

#include <algorithm>
#include <cmath>

#include "constants.h"

namespace wl {

MoveIntent computeMoveIntent(const InputState& input) {
    const auto held = [&input](Direction direction) {
        return input.directionHeld[static_cast<int>(direction)];
    };

    const double keyX = (held(Direction::Right) ? 1.0 : 0.0) - (held(Direction::Left) ? 1.0 : 0.0);
    const double keyY = (held(Direction::Down) ? 1.0 : 0.0) - (held(Direction::Up) ? 1.0 : 0.0);
    if (keyX != 0.0 || keyY != 0.0) {
        // Normalise so diagonals aren't 41% faster.
        const double length = std::hypot(keyX, keyY);
        return {keyX / length, keyY / length};
    }

    if (!input.dragActive) return {};

    const double dragLength = std::hypot(input.dragOffsetX, input.dragOffsetY);
    if (dragLength <= constants::dragDeadzoneInCssPixels) return {};

    // Ramp from 0 at the deadzone edge to 1 at full-speed distance.
    const double ramp = (dragLength - constants::dragDeadzoneInCssPixels) /
                        (constants::dragFullSpeedInCssPixels - constants::dragDeadzoneInCssPixels);
    const double strength = std::min(1.0, ramp);
    return {input.dragOffsetX / dragLength * strength, input.dragOffsetY / dragLength * strength};
}

}  // namespace wl
