// Story fragments found by scanning ruins. Eight in all, decoded in order, so
// every world tells the same short story about the survey crew that came
// before the player. The text is plain ASCII with no quotes or backslashes,
// so it can go straight into the journal JSON.
#pragma once

#include <string_view>

namespace wl {

inline constexpr int loreFragmentCount = 8;

// Index 0 to loreFragmentCount - 1. Anything else gives an empty string.
std::string_view loreTitle(int index);
std::string_view loreText(int index);

}  // namespace wl
