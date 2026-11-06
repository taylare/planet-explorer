#include "star_chart.h"

#include "hash.h"
#include "planet.h"

namespace wl {

namespace {
// Seed salt for picking neighbours. Never change it, or every planet's star chart would change.
constexpr std::uint32_t starChartSalt = 601;

// Plenty of tries, but the loop must end even if something odd made every candidate clash.
constexpr int maxAttemptsPerDestination = 64;
}  // namespace

std::array<WorldSeed, destinationCount> destinationSeeds(WorldSeed current) {
    const std::uint32_t chartSeed = deriveSeed(current, starChartSalt);
    const std::string currentName = makePlanetProfile(current).name;

    std::array<WorldSeed, destinationCount> seeds{};
    std::array<std::string, destinationCount> names;
    for (int index = 0; index < destinationCount; ++index) {
        WorldSeed candidate = current;
        std::string candidateName = currentName;
        for (int attempt = 0; attempt < maxAttemptsPerDestination; ++attempt) {
            candidate = hashCoords(chartSeed, index, attempt);
            candidateName = makePlanetProfile(candidate).name;

            bool clashes = candidate == current || candidateName == currentName;
            for (int earlier = 0; earlier < index; ++earlier) {
                if (candidate == seeds[earlier] || candidateName == names[earlier]) clashes = true;
            }
            if (!clashes) break;
        }
        seeds[index] = candidate;
        names[index] = candidateName;
    }
    return seeds;
}

}  // namespace wl
