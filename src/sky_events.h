// Things that happen in the sky: meteor showers on some nights, and flashes of
// lightning in heavy rain.
//
// Nothing here keeps state. Each answer is a pure function of the world seed and
// the time, so the same moment always looks the same, saves need nothing extra,
// and tests can ask about any moment directly.
#pragma once

#include <cstdint>

#include "world.h"

namespace wl {

// A night runs from evening to the next dawn, so it spans a midnight. This numbers it by
// the day on which it began: after midnight (before noon) the day counter has already moved on.
int nightNumberFor(double timeOfDay, int day);

// Whether a given night has a meteor shower. Stable for a seed and night number.
bool isMeteorShowerNight(WorldSeed seed, int nightNumber);

// One meteor, for the renderer to draw. Positions are fractions of the screen, since the
// meteors belong to the sky and do not move with the camera.
struct MeteorStreak {
    bool present = false;
    double startX = 0.0;  // 0 to 1 across the screen
    double startY = 0.0;  // 0 to 1 down the screen
    double directionX = 0.0;  // unit vector, always heading downward
    double directionY = 1.0;
    double startOffsetSeconds = 0.0;  // when in its slot the meteor appears
};

// Which slot a time falls in.
std::int64_t meteorSlotAt(double timeSeconds);
double meteorSlotStartSeconds(std::int64_t slot);

// Meteor number `index` (0 to constants::meteorsPerSlot - 1) of a slot.
MeteorStreak meteorStreakFor(WorldSeed seed, int nightNumber, std::int64_t slot, int index);

// Lightning flash brightness at a moment, 0 to 1. Zero unless it is raining hard.
float lightningFlashAt(WorldSeed seed, double timeSeconds, double rainIntensity);

}  // namespace wl
