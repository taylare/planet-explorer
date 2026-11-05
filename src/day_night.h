// Time of day and the colour grade that goes with it.
#pragma once

namespace wl {

struct LightingState {
    float tintRed = 1.0f;  // multipliers applied to the whole scene
    float tintGreen = 1.0f;
    float tintBlue = 1.0f;
    float nightAmount = 0.0f;  // 0 full day, 1 full night; drives lights and stars
};

// Smoothly blended grade for a time of day (0 midnight, 0.5 noon, wraps at 1).
LightingState lightingAt(double timeOfDay);

class DayNightCycle {
public:
    explicit DayNightCycle(double startTimeOfDay);

    void advance(double seconds);
    void setTimeOfDay(double timeOfDay);
    void setDay(int day) { day_ = day < 1 ? 1 : day; }

    double timeOfDay() const { return timeOfDay_; }
    int day() const { return day_; }  // starts at 1, ticks over at midnight
    LightingState lighting() const { return lightingAt(timeOfDay_); }

private:
    double timeOfDay_;
    int day_ = 1;
};

}  // namespace wl
