#include "sky_events.h"

#include <algorithm>
#include <cmath>

#include "constants.h"
#include "hash.h"

namespace wl {

namespace {
// Seed salts. Never change these, or every world's sky events would move.
constexpr std::uint32_t meteorNightSalt = 501;
constexpr std::uint32_t meteorStreakSalt = 502;
constexpr std::uint32_t lightningSalt = 503;

constexpr double pi = 3.14159265358979323846;

// Hash for one decision in one slot, kept separate from the others so they don't correlate.
std::uint32_t slotHash(std::uint32_t seed, std::int64_t slot, int channel) {
    // Fold the 64 bit slot into two 32 bit halves for the coordinate hash.
    const std::int32_t low = static_cast<std::int32_t>(slot & 0xffffffff);
    const std::int32_t high = static_cast<std::int32_t>((slot >> 32) & 0xffffffff);
    return mixBits32(hashCoords(seed, low, high) + static_cast<std::uint32_t>(channel));
}
}  // namespace

int nightNumberFor(double timeOfDay, int day) {
    // Evening belongs to today's night; the small hours belong to the night that began yesterday.
    return timeOfDay < 0.5 ? day - 1 : day;
}

bool isMeteorShowerNight(WorldSeed seed, int nightNumber) {
    const std::uint32_t hash = hashCoords(deriveSeed(seed, meteorNightSalt), nightNumber, 0);
    return hashToUnitFloat(hash) < constants::meteorShowerNightChance;
}

std::int64_t meteorSlotAt(double timeSeconds) {
    return static_cast<std::int64_t>(std::floor(timeSeconds / constants::meteorSlotSeconds));
}

double meteorSlotStartSeconds(std::int64_t slot) { return static_cast<double>(slot) * constants::meteorSlotSeconds; }

MeteorStreak meteorStreakFor(WorldSeed seed, int nightNumber, std::int64_t slot, int index) {
    const std::uint32_t base = deriveSeed(seed, meteorStreakSalt) + static_cast<std::uint32_t>(nightNumber) * 7919u;
    const std::uint32_t hash = slotHash(base, slot, index * 8);

    MeteorStreak streak;
    streak.present = hashToUnitFloat(hash) < constants::meteorChancePerMeteor;
    if (!streak.present) return streak;

    streak.startX = hashToUnitFloat(mixBits32(hash + 1));
    streak.startY = hashToUnitFloat(mixBits32(hash + 2)) * 0.6;  // start in the upper part so there is room to fall

    // Heading mostly down, leaning left or right by up to about 50 degrees.
    const double lean = (hashToUnitFloat(mixBits32(hash + 3)) - 0.5) * (pi * 0.55);
    streak.directionX = std::sin(lean);
    streak.directionY = std::cos(lean);

    // The whole flight has to fit in the slot, so no meteor straddles two slots.
    const double latestStart = constants::meteorSlotSeconds - constants::meteorDurationSeconds;
    streak.startOffsetSeconds = hashToUnitFloat(mixBits32(hash + 4)) * std::max(latestStart, 0.0);
    return streak;
}

float lightningFlashAt(WorldSeed seed, double timeSeconds, double rainIntensity) {
    using namespace constants;
    if (rainIntensity < lightningMinimumRainIntensity) return 0.0f;

    const std::int64_t slot = static_cast<std::int64_t>(std::floor(timeSeconds / lightningSlotSeconds));
    const std::uint32_t hash = slotHash(deriveSeed(seed, lightningSalt), slot, 0);

    // Heavier rain makes a flash in this slot more likely.
    const double heaviness =
        std::clamp((rainIntensity - lightningMinimumRainIntensity) / (1.0 - lightningMinimumRainIntensity), 0.0, 1.0);
    const double chance = lightningBaseChancePerSlot + lightningExtraChanceAtFullRain * heaviness;
    if (hashToUnitFloat(hash) >= chance) return 0.0f;

    // When in the slot it strikes. Leave room at the end so both flashes and their fade stay inside the slot.
    constexpr double tailSeconds = 1.0;
    const double latestStrike = lightningSlotSeconds - tailSeconds;
    const double strikeAt = hashToUnitFloat(mixBits32(hash + 1)) * latestStrike;
    const double sinceStart = timeSeconds - static_cast<double>(slot) * lightningSlotSeconds - strikeAt;
    if (sinceStart < 0.0) return 0.0f;

    // A sharp flash that fades fast, then a weaker echo a moment later.
    const double first = std::exp(-sinceStart / lightningDecaySeconds);
    const double sinceSecond = sinceStart - lightningSecondFlashDelaySeconds;
    const double second = sinceSecond >= 0.0 ? lightningSecondFlashStrength * std::exp(-sinceSecond / lightningDecaySeconds) : 0.0;
    return static_cast<float>(std::clamp(std::max(first, second), 0.0, 1.0));
}

}  // namespace wl
