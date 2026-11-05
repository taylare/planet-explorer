#include <catch2/catch_test_macros.hpp>
#include <string>

#include "journal.h"
#include "lore.h"

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
            "[{\"type\":\"biome\",\"name\":\"Ice Sheet\",\"x\":-7,\"y\":8,\"day\":3},"
            "{\"type\":\"landmark\",\"name\":\"Hot Spring\",\"x\":100,\"y\":-200,\"day\":4}]");
}

TEST_CASE("each species is recorded once, by first scan", "[journal][species]") {
    Journal journal;
    REQUIRE(journal.recordSpecies(Species::Fox, 5, 6, 2));
    REQUIRE_FALSE(journal.recordSpecies(Species::Fox, 50, 60, 3));
    REQUIRE_FALSE(journal.recordSpecies(Species::Count, 0, 0, 1));
    REQUIRE(journal.hasSpecies(Species::Fox));
    REQUIRE_FALSE(journal.hasSpecies(Species::Deer));
    REQUIRE(journal.entries().size() == 1);
    REQUIRE(journal.entries()[0].tileX == 5);
    REQUIRE(journalEntryName(journal.entries()[0]) == "Frost Stalker");
    REQUIRE(journal.toJson() == "[{\"type\":\"species\",\"name\":\"Frost Stalker\",\"x\":5,\"y\":6,\"day\":2}]");
}

TEST_CASE("the survey counts everything findable and ignores deep ocean", "[journal][survey]") {
    Journal journal;
    const SurveyProgress empty = journal.survey();
    REQUIRE(empty.found == 0);
    // 9 walkable biomes, 5 landmark types, 6 species and 8 story fragments.
    REQUIRE(empty.total == 9 + 5 + 6 + 8);
    REQUIRE(journal.surveyPercent() == 0);

    // Deep ocean can't be walked on, so recording it must not move the survey.
    journal.recordBiome(Biome::DeepOcean, 0, 0, 1);
    REQUIRE(journal.survey().found == 0);

    journal.recordBiome(Biome::Forest, 0, 0, 1);
    journal.recordLandmark(PropKind::Tower, 0, 0, 1);
    journal.recordSpecies(Species::Deer, 0, 0, 1);
    REQUIRE(journal.survey().found == 3);
    REQUIRE(journal.surveyPercent() == 3 * 100 / 28);
}

TEST_CASE("the survey only reaches 100 when everything is found", "[journal][survey]") {
    Journal journal;
    for (int index = 0; index < biomeCount; ++index) journal.recordBiome(static_cast<Biome>(index), 0, 0, 1);
    for (int index = 0; index < propKindCount; ++index) journal.recordLandmark(static_cast<PropKind>(index), 0, 0, 1);
    for (int index = 0; index < speciesCount; ++index) journal.recordSpecies(static_cast<Species>(index), 0, 0, 1);
    REQUIRE(journal.surveyPercent() < 100);  // story fragments are still missing
    for (int index = 0; index < loreFragmentCount; ++index) journal.recordLore(index, 0, 0, 1);
    REQUIRE(journal.surveyPercent() == 100);
}
