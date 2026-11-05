// The spacesuit's power. It drains while you explore, faster at night, on cold
// ground and in rain or snow, and refills at the ship, a campfire or a hot spring.
// A flat suit slows you down and nearly kills the headlamp; the only way out
// is to be beamed back to the ship.
//
// The rules here are plain functions and a small class, with no world or
// renderer in sight, so they are easy to test on their own.
#pragma once

#include "constants.h"

namespace wl {

// What the suit is up against this step.
struct PowerConditions {
    double nightAmount = 0.0;            // 0 full day, 1 full night
    bool onColdGround = false;           // snow or tundra underfoot
    double precipitationIntensity = 0.0;  // 0 to 1 rain or snow
};

enum class RechargeSource { None, Campfire, HotSpring, Ship };

// Units lost per second under these conditions.
double suitDrainPerSecond(const PowerConditions& conditions);

// Units gained per second at a source. None gives 0.
double suitRechargePerSecond(RechargeSource source);

class SuitPower {
public:
    explicit SuitPower(double capacity = constants::suitBaseCapacity) : capacity_(capacity), charge_(capacity) {}

    // While recharging the suit does not drain, it just fills.
    void update(double stepSeconds, const PowerConditions& conditions, RechargeSource source);

    // Changes the size of the battery. The charge you already have is kept (never above the new
    // capacity), so a bigger battery arrives with room to fill rather than topped up for free.
    void setCapacity(double capacity);

    void setFraction(double fraction);  // clamped to 0..1
    void refillTo(double fraction);     // raises the charge to at least this fraction, never lowers it

    double capacity() const { return capacity_; }
    double charge() const { return charge_; }
    double fraction() const { return capacity_ > 0.0 ? charge_ / capacity_ : 0.0; }
    bool isEmpty() const { return charge_ <= 0.0; }
    bool isLow() const { return fraction() < constants::lowPowerFraction; }

    // Multiplier on walking speed: normal until empty.
    double speedMultiplier() const { return isEmpty() ? constants::emptySuitSpeedMultiplier : 1.0; }

    // Multiplier on headlamp brightness: full, then fading below the low-power mark.
    float headlampStrength() const;

private:
    double capacity_;
    double charge_;
};

}  // namespace wl
