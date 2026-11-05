#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <cmath>

#include "constants.h"
#include "input.h"

using namespace wl;

namespace {
InputState withKeys(std::initializer_list<Direction> held) {
    InputState input;
    for (Direction direction : held) input.directionHeld[static_cast<int>(direction)] = true;
    return input;
}

double lengthOf(const MoveIntent& intent) { return std::hypot(intent.x, intent.y); }
}  // namespace

TEST_CASE("no input means no movement", "[input]") {
    const MoveIntent intent = computeMoveIntent(InputState{});
    REQUIRE(intent.x == 0.0);
    REQUIRE(intent.y == 0.0);
}

TEST_CASE("single keys move along one axis", "[input]") {
    REQUIRE(computeMoveIntent(withKeys({Direction::Right})).x == 1.0);
    REQUIRE(computeMoveIntent(withKeys({Direction::Left})).x == -1.0);
    REQUIRE(computeMoveIntent(withKeys({Direction::Up})).y == -1.0);
    REQUIRE(computeMoveIntent(withKeys({Direction::Down})).y == 1.0);
}

TEST_CASE("diagonals are not faster than straight lines", "[input]") {
    const MoveIntent intent = computeMoveIntent(withKeys({Direction::Up, Direction::Right}));
    REQUIRE(lengthOf(intent) == Catch::Approx(1.0));
    REQUIRE(intent.x > 0.0);
    REQUIRE(intent.y < 0.0);
}

TEST_CASE("opposite keys cancel out", "[input]") {
    REQUIRE(lengthOf(computeMoveIntent(withKeys({Direction::Left, Direction::Right}))) == 0.0);
}

TEST_CASE("short drags are ignored", "[input]") {
    InputState input;
    input.dragActive = true;
    input.dragOffsetX = constants::dragDeadzoneInCssPixels * 0.5;
    REQUIRE(lengthOf(computeMoveIntent(input)) == 0.0);
}

TEST_CASE("drag strength ramps up then caps at full speed", "[input]") {
    InputState input;
    input.dragActive = true;

    input.dragOffsetX = (constants::dragDeadzoneInCssPixels + constants::dragFullSpeedInCssPixels) / 2.0;
    const double partial = lengthOf(computeMoveIntent(input));
    REQUIRE(partial > 0.0);
    REQUIRE(partial < 1.0);

    input.dragOffsetX = constants::dragFullSpeedInCssPixels * 4.0;
    REQUIRE(lengthOf(computeMoveIntent(input)) == Catch::Approx(1.0));
}

TEST_CASE("keys take priority over dragging", "[input]") {
    InputState input = withKeys({Direction::Left});
    input.dragActive = true;
    input.dragOffsetX = 500.0;
    REQUIRE(computeMoveIntent(input).x == -1.0);
}
