#include "suit_power.h"

#include <algorithm>

namespace wl {

double suitDrainPerSecond(const PowerConditions& conditions) {
    using namespace constants;
    double drain = suitBaseDrainPerSecond;
    drain += suitNightExtraDrain * std::clamp(conditions.nightAmount, 0.0, 1.0);
    if (conditions.onColdGround) drain += suitColdExtraDrain;
    drain += suitWeatherExtraDrain * std::clamp(conditions.precipitationIntensity, 0.0, 1.0);
    return drain;
}

double suitRechargePerSecond(RechargeSource source) {
    switch (source) {
        case RechargeSource::Ship: return constants::shipRechargePerSecond;
        case RechargeSource::HotSpring: return constants::hotSpringRechargePerSecond;
        case RechargeSource::Campfire: return constants::campfireRechargePerSecond;
        case RechargeSource::None: break;
    }
    return 0.0;
}

void SuitPower::update(double stepSeconds, const PowerConditions& conditions, RechargeSource source) {
    if (source != RechargeSource::None) {
        charge_ = std::min(capacity_, charge_ + suitRechargePerSecond(source) * stepSeconds);
        return;
    }
    charge_ = std::max(0.0, charge_ - suitDrainPerSecond(conditions) * stepSeconds);
}

void SuitPower::setCapacity(double capacity) {
    capacity_ = std::max(capacity, 1.0);
    charge_ = std::min(charge_, capacity_);
}

void SuitPower::setFraction(double fraction) { charge_ = capacity_ * std::clamp(fraction, 0.0, 1.0); }

void SuitPower::refillTo(double fraction) { charge_ = std::max(charge_, capacity_ * std::clamp(fraction, 0.0, 1.0)); }

float SuitPower::headlampStrength() const {
    const double low = constants::lowPowerFraction;
    const double current = fraction();
    if (current >= low) return 1.0f;
    const float floor = constants::emptyHeadlampStrength;
    return floor + (1.0f - floor) * static_cast<float>(current / low);
}

}  // namespace wl
