#include "lore.h"

#include <iterator>

namespace wl {

namespace {
struct LoreFragment {
    std::string_view title;
    std::string_view text;
};

constexpr LoreFragment fragments[loreFragmentCount] = {
    {"First Entry",
     "We came down with six lamps and one map. The map was wrong within a day. The lamps are fine."},
    {"Stone Rings",
     "The ruins are older than the soil around them. Roots grow around the stones as if they were asked to."},
    {"The Quiet Hour",
     "Nothing here is loud. The animals do not hide. They simply decide how close you are allowed to be."},
    {"Sample Count",
     "Crystals hum when the light is low. We logged forty groves before we stopped counting. "
     "They answer a little louder each night."},
    {"Beacon Notes",
     "The towers were never watchtowers. Climb one at dusk and the whole land turns to face it."},
    {"Weather Report",
     "Storms come through in lines, like footsteps. We began to plan our days around them, "
     "and then our lives."},
    {"Last Lamp",
     "Ilen stayed behind to keep the last lamp burning. We told ourselves she would catch up. "
     "She always did."},
    {"To Whoever Follows",
     "Leave a light where you scanned. The ground remembers footsteps, and so, we hope, will you."},
};
static_assert(std::size(fragments) == loreFragmentCount, "one fragment per lore slot");
}  // namespace

std::string_view loreTitle(int index) {
    return index >= 0 && index < loreFragmentCount ? fragments[index].title : std::string_view{};
}

std::string_view loreText(int index) {
    return index >= 0 && index < loreFragmentCount ? fragments[index].text : std::string_view{};
}

}  // namespace wl
