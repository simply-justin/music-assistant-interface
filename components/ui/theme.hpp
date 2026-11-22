#pragma once
// Design tokens from docs/design-spec.md §1. Screens use these names, never raw values.
#include "../fonts/assets.hpp"
#include "lvgl.h"

namespace ui::theme {

// Colour
inline lv_color_t bg() { return lv_color_hex(0x0B0F14); }
inline lv_color_t surface() { return lv_color_hex(0x151B23); }
inline lv_color_t elevated() { return lv_color_hex(0x202834); }
inline lv_color_t border() { return lv_color_hex(0x2A3442); }
inline lv_color_t text() { return lv_color_hex(0xF5F7FA); }
inline lv_color_t text2() { return lv_color_hex(0xA0AAB8); }
inline lv_color_t text3() { return lv_color_hex(0x8B95A4); }
inline lv_color_t accent() { return lv_color_hex(0x5CC8B8); }
inline lv_color_t on_accent() { return lv_color_hex(0x0B0F14); }
inline lv_color_t warning() { return lv_color_hex(0xE7B14C); }
inline lv_color_t danger_text() { return lv_color_hex(0xF08C7E); }
inline lv_color_t danger_fill() { return lv_color_hex(0xB3261E); }
inline lv_color_t live() { return lv_color_hex(0xC2410C); }
constexpr lv_opa_t kScrimOpa = 199;  // 78 %

// Type (8 fonts)
inline const lv_font_t* label() { return &ui_font_label; }      // 14 SemiBold
inline const lv_font_t* meta() { return &ui_font_meta; }        // 16 Medium
inline const lv_font_t* body() { return &ui_font_body; }        // 18 SemiBold
inline const lv_font_t* lead() { return &ui_font_lead; }        // 20 Medium
inline const lv_font_t* h2() { return &ui_font_h2; }            // 24 SemiBold
inline const lv_font_t* h1() { return &ui_font_h1; }            // 36 Bold
inline const lv_font_t* display() { return &ui_font_display; }  // 48 Bold
inline const lv_font_t* clock() { return &ui_font_clock; }      // 140 Light, digits

// Icons
inline const lv_font_t* icons(int size) {
    return size <= 22 ? &ui_icons_22 : size <= 28 ? &ui_icons_28 : size <= 32 ? &ui_icons_32 : &ui_icons_48;
}

// Size
constexpr int32_t kScreenW = 1280, kScreenH = 800;
constexpr int32_t kRailW = 96, kMiniH = 96, kPad = 40;
constexpr int32_t kPrimary = 64, kSecondary = 56, kChipH = 56, kGap = 8;
constexpr int32_t kRadiusRow = 14, kRadiusTile = 12, kRadiusCard = 20, kRadiusSheet = 24;

// Placeholder artwork colour for an item (until the artwork pipeline lands): muted, stable per uri.
lv_color_t art_colour(const char* key);

}  // namespace ui::theme
