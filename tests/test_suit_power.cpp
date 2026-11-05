#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "constants.h"
#include "suit_power.h"
#include "upgrades.h"

using namespace wl;

namespace {
constexpr double step = constants::simulationStepSeconds;
const PowerConditions calmDay{};
}  // namespace

TEST_CASE("the suit drains faster in worse conditions", "[power]") {
    const double base = suitDrainPerSecond(calmDay);
    REQUIRE(base == Catch::Approx(constants::suitBaseDrainPerSecond));

    PowerConditions night;
    night.nightAmount = 1.0;
    PowerConditions halfNight;
    halfNight.nightAmount = 0.5;
    PowerConditions cold;
    cold.onColdGround = true;
    PowerConditions storm;
    storm.precipitationIntensity = 1.0;

    REQUIRE(suitDrainPerSecond(night) > suitDrainPerSecond(halfNight));
    REQUIRE(suitDrainPerSecond(halfNight) > base);
    REQUIRE(suitDrainPerSecond(cold) > base);
    REQUIRE(suitDrainPerSecond(storm) > base);

    PowerConditions everything;
    everything.nightAmount = 1.0;
    everything.onColdGround = true;
    everything.precipitationIntensity = 1.0;
    REQUIRE(suitDrainPerSecond(everything) ==
            Catch::Approx(base + constants::suitNightExtraDrain + constants::suitColdExtraDrain +
                          constants::suitWeatherExtraDrain));
}

TEST_CASE("out of range condition values are clamped", "[power]") {
    PowerConditions silly;
    silly.nightAmount = 5.0;
    silly.precipitationIntensity = -3.0;
    REQUIRE(suitDrainPerSecond(silly) == Catch::Approx(constants::suitBaseDrainPerSecond + constants::suitNightExtraDrain));
}

TEST_CASE("recharge sources rank ship over hot spring over campfire over nothing", "[power]") {
    REQUIRE(suitRechargePerSecond(RechargeSource::None) == 0.0);
    REQUIRE(suitRechargePerSecond(RechargeSource::Campfire) > 0.0);
    REQUIRE(suitRechargePerSecond(RechargeSource::HotSpring) > suitRechargePerSecond(RechargeSource::Campfire));
    REQUIRE(suitRechargePerSecond(RechargeSource::Ship) > suitRechargePerSecond(RechargeSource::HotSpring));
}

TEST_CASE("the suit drains to empty and no further", "[power]") {
    SuitPower power;
    REQUIRE(power.fraction() == Catch::Approx(1.0));
    REQUIRE_FALSE(power.isEmpty());

    const int stepsToEmpty = static_cast<int>(constants::suitBaseCapacity / constants::suitBaseDrainPerSecond / step);
    for (int index = 0; index < stepsToEmpty + 120; ++index) power.update(step, calmDay, RechargeSource::None);
    REQUIRE(power.isEmpty());
    REQUIRE(power.charge() == 0.0);
}

TEST_CASE("recharging fills the suit and stops at capacity, without draining", "[power]") {
    SuitPower power;
    power.setFraction(0.2);
    const double before = power.charge();
    power.update(1.0, calmDay, RechargeSource::Campfire);
    REQUIRE(power.charge() == Catch::Approx(before + constants::campfireRechargePerSecond));

    for (int index = 0; index < 60 * 60; ++index) power.update(step, calmDay, RechargeSource::Ship);
    REQUIRE(power.charge() == Catch::Approx(power.capacity()));
}

TEST_CASE("an empty suit is slow and its lamp is nearly out", "[power]") {
    SuitPower power;
    REQUIRE(power.speedMultiplier() == 1.0);
    REQUIRE(power.headlampStrength() == 1.0f);

    power.setFraction(0.0);
    REQUIRE(power.speedMultiplier() == Catch::Approx(constants::emptySuitSpeedMultiplier));
    REQUIRE(power.headlampStrength() == Catch::Approx(constants::emptyHeadlampStrength));
}

TEST_CASE("the lamp fades smoothly below the low power mark", "[power]") {
    SuitPower power;
    power.setFraction(constants::lowPowerFraction);
    REQUIRE(power.headlampStrength() == 1.0f);
    power.setFraction(constants::lowPowerFraction / 2.0);
    const float halfway = power.headlampStrength();
    REQUIRE(halfway < 1.0f);
    REQUIRE(halfway > constants::emptyHeadlampStrength);
    REQUIRE(power.isLow());
}

TEST_CASE("a bigger battery keeps the charge you had and never exceeds capacity", "[power]") {
    SuitPower power;
    power.setFraction(0.5);
    const double charge = power.charge();
    power.setCapacity(power.capacity() * 2.0);
    REQUIRE(power.charge() == Catch::Approx(charge));
    REQUIRE(power.fraction() == Catch::Approx(0.25));

    power.setFraction(1.0);
    power.setCapacity(100.0);
    REQUIRE(power.charge() == Catch::Approx(100.0));
}

TEST_CASE("refillTo raises the charge but never lowers it", "[power]") {
    SuitPower power;
    power.setFraction(0.0);
    power.refillTo(0.5);
    REQUIRE(power.fraction() == Catch::Approx(0.5));
    power.refillTo(0.1);
    REQUIRE(power.fraction() == Catch::Approx(0.5));
}

TEST_CASE("upgrade costs rise with level and run out at the top", "[upgrades]") {
    REQUIRE(upgradeCost(0) == 3);
    REQUIRE(upgradeCost(1) == 6);
    REQUIRE(upgradeCost(2) == 10);
    REQUIRE(upgradeCost(constants::maxUpgradeLevel) == -1);
    REQUIRE(upgradeCost(-1) == -1);
}

TEST_CASE("each level makes every upgrade better", "[upgrades]") {
    for (int level = 0; level < constants::maxUpgradeLevel; ++level) {
        REQUIRE(scannerRangeInTiles(level + 1) > scannerRangeInTiles(level));
        REQUIRE(scannerDurationScale(level + 1) < scannerDurationScale(level));
        REQUIRE(headlampRadiusScale(level + 1) > headlampRadiusScale(level));
        REQUIRE(suitCapacityAt(level + 1) > suitCapacityAt(level));
    }
    // Level zero is the base game, and a maxed scan is still not instant.
    REQUIRE(scannerRangeInTiles(0) == Catch::Approx(constants::scanRangeInTiles));
    REQUIRE(scannerDurationScale(0) == Catch::Approx(1.0));
    REQUIRE(scannerDurationScale(constants::maxUpgradeLevel) > 0.0);
    REQUIRE(suitCapacityAt(0) == Catch::Approx(constants::suitBaseCapacity));
}

TEST_CASE("raising an upgrade stops at the max level", "[upgrades]") {
    Upgrades upgrades;
    for (int level = 0; level < constants::maxUpgradeLevel; ++level) REQUIRE(upgrades.raise(UpgradeKind::Scanner));
    REQUIRE(upgrades.isMaxed(UpgradeKind::Scanner));
    REQUIRE_FALSE(upgrades.raise(UpgradeKind::Scanner));
    REQUIRE(upgrades.level(UpgradeKind::Scanner) == constants::maxUpgradeLevel);
    REQUIRE(upgrades.level(UpgradeKind::Headlamp) == 0);  // others untouched
    REQUIRE(upgrades.nextCost(UpgradeKind::Scanner) == -1);
}

TEST_CASE("every upgrade has a name and description", "[upgrades]") {
    for (int index = 0; index < upgradeKindCount; ++index) {
        const UpgradeKind kind = static_cast<UpgradeKind>(index);
        REQUIRE(upgradeName(kind) != "Unknown");
        REQUIRE_FALSE(upgradeDescription(kind).empty());
    }
}
