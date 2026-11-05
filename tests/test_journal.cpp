#include <catch2/catch_test_macros.hpp>
#include <string>

#include "journal.h"

using namespace wl;

TEST_CASE("only the first discovery of each biome is recorded", "[journal]") {
    Journal journal;
    REQUIRE(journal.recordBiome(Biome::Forest, 1, 2, 1));
    REQUIRE_FALSE(journal.recordBiome(Biome::Forest, 50, 60, 2));
    REQUIRE(journal.recordBiome(Biome::Desert, 3, 4, 2));

    REQUIRE(journal.entries().size() == 2);
    REQUIRE(journal.entries()[0].tileX == 1);  // first sighting kept, not overwritten
    REQUIRE(journal.hasBiome(Biome::Forest));
    REQUIRE_FALSE(journal.hasBiome(Biome::Snow));
}

TEST_CASE("only landmark kinds go in the landmark list", "[journal]") {
    Journal journal;
    REQUIRE(journal.recordLandmark(PropKind::Tower, 0, 0, 1));
    REQUIRE(journal.recordLandmark(PropKind::Ruins, 0, 0, 1));
    REQUIRE_FALSE(journal.recordLandmark(PropKind::Tower, 9, 9, 2));
    REQUIRE_FALSE(journal.recordLandmark(PropKind::Rock, 0, 0, 1));
    REQUIRE_FALSE(journal.recordLandmark(PropKind::BroadleafTree, 0, 0, 1));
    REQUIRE(journal.entries().size() == 2);
}

TEST_CASE("journal JSON lists entries with names and coordinates", "[journal]") {
    Journal journal;
    REQUIRE(journal.toJson() == "[]");
    journal.recordBiome(Biome::Snow, -7, 8, 3);
    journal.recordLandmark(PropKind::HotSpring, 100, -200, 4);
    const std::string json = journal.toJson();
    REQUIRE(json ==
            "[{\"type\":\"biome\",\"name\":\"Snowfield\",\"x\":-7,\"y\":8,\"day\":3},"
            "{\"type\":\"landmark\",\"name\":\"Hot Spring\",\"x\":100,\"y\":-200,\"day\":4}]");
}
