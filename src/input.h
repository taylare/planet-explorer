// Turns raw key and drag state into a movement direction.
// The browser layer only reports what's pressed; the decisions live here.
#pragma once

#include <array>

namespace wl {

enum class Direction { Up = 0, Down = 1, Left = 2, Right = 3 };
inline constexpr int directionCount = 4;

struct InputState {
    std::array<bool, directionCount> directionHeld{};

    // Virtual joystick: offset of the thumb from where it first pressed.
    bool dragActive = false;
    double dragOffsetX = 0.0;  // CSS pixels
    double dragOffsetY = 0.0;

    // Hold to walk: the held point relative to the player on screen.
    // The game fills the offset in each step, since the player moves.
    bool followActive = false;
    double followPointerX = 0.0;  // CSS pixels from the canvas top-left
    double followPointerY = 0.0;
    double followOffsetX = 0.0;   // CSS pixels from the player to the pointer
    double followOffsetY = 0.0;

    // The scan button: hold to scan nearby animals, and to creep while doing so.
    bool actionHeld = false;
};

// Length is 0 to 1. Partial lengths come from short drags.
struct MoveIntent {
    double x = 0.0;
    double y = 0.0;
};

// Priority when several are in use: keys, then joystick, then hold to walk.
MoveIntent computeMoveIntent(const InputState& input);

}  // namespace wl
