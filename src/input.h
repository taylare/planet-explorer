// Turns raw key and drag state into a movement direction.
// The browser layer only reports what's pressed; the decisions live here.
#pragma once

#include <array>

namespace wl {

enum class Direction { Up = 0, Down = 1, Left = 2, Right = 3 };
inline constexpr int directionCount = 4;

struct InputState {
    std::array<bool, directionCount> directionHeld{};
    bool dragActive = false;
    double dragOffsetX = 0.0;  // CSS pixels from where the drag started
    double dragOffsetY = 0.0;
};

// Length is 0 to 1. Partial lengths come from short drags.
struct MoveIntent {
    double x = 0.0;
    double y = 0.0;
};

// Keys win over dragging if both are in use.
MoveIntent computeMoveIntent(const InputState& input);

}  // namespace wl
