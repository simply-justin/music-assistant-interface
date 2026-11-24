#pragma once
// Display-text helpers. No LVGL here, so they are host-tested.
#include <cstdint>
#include <string>
#include <string_view>

namespace ui {

// Removes emoji (and their joiners/variation selectors) the panel fonts cannot draw, then trims
// and collapses spaces. Decision 2026-11-22: "Daydreams💭😌" shows as "Daydreams".
std::string clean(std::string_view text);

// 4:01, 1:02:03
std::string duration(uint32_t seconds);
// Length of a list for people, not a clock: "43 min", "28 hr 43 min" ("28 uur 43 min").
std::string total_duration(uint32_t seconds);

}  // namespace ui
