#include "input.h"

#include <algorithm>
#include <cmath>

#include "constants.h"

namespace wl {

namespace {
// Direction of an offset, with strength ramping from 0 at the deadzone edge
// to 1 at the full-speed distance. Shared by the joystick and hold to walk.
MoveIntent rampedIntent(double offsetX, double offsetY, double deadzone, double fullSpeedDistance) {
    const double length = std::hypot(offsetX, offsetY);
    if (length <= deadzone) return {};
    const double strength = std::min(1.0, (length - deadzone) / (fullSpeedDistance - deadzone));
    return {offsetX / length * strength, offsetY / length * strength};
}
}  // namespace

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

    if (input.dragActive) {
        return rampedIntent(input.dragOffsetX, input.dragOffsetY, constants::dragDeadzoneInCssPixels,
                            constants::dragFullSpeedInCssPixels);
    }

    if (input.followActive) {
        return rampedIntent(input.followOffsetX, input.followOffsetY, constants::followDeadzoneInCssPixels,
                            constants::followFullSpeedInCssPixels);
    }
    return {};
}

}  // namespace wl
