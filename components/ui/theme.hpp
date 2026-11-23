#pragma once
// Design tokens (docs/design.md). Colours and radii come from the
// active Appearance (dark/light, accent, roundness) and can change at run time; screens are
// rebuilt when they do. Screens use these names, never raw values.
#include "../fonts/assets.hpp"
#include "app/commands.hpp"
#include "lvgl.h"

namespace ui::theme {

void apply(const app::Appearance& appearance);
const app::Appearance& appearance();
bool light();
bool motion();  // false when the user asked for reduced motion

// Colour
lv_color_t bg();          // page
lv_color_t side();        // sidebar + player bar
lv_color_t surface();     // tiles, cards
lv_color_t elevated();    // selected item, keys, pressed
lv_color_t hairline();    // dividers
lv_color_t border();      // outlined controls
lv_color_t text();
lv_color_t text2();
lv_color_t text3();
lv_color_t accent();      // play button, active tab, equalizer, selected pill
lv_color_t on_accent();
lv_color_t field_bg();    // search / text fields
lv_color_t field_text();
lv_color_t warning();
lv_color_t danger_text();
lv_color_t danger_fill();
lv_color_t live();
constexpr lv_opa_t kScrimOpa = 199;  // 78 %

// Accent choices for Settings › Appearance (index = Appearance::accent; 0 = neutral).
int accent_count();
lv_color_t accent_swatch(int index);  // as shown in the current mode

// Art-derived background for the current mode (Now Playing, Home band, Top result).
lv_color_t art_tint(lv_color_t art_colour);

// Radius (from Appearance::roundness)
int32_t r_art();      // covers
int32_t r_control();  // tiles, rows, menu items, fields, keys
int32_t r_card();     // cards, toasts, banners
int32_t r_sheet();    // dialogs
int32_t r_button();   // text buttons and chips (pill unless "sharp")

// Type (8 fonts)
inline const lv_font_t* label() { return &ui_font_label; }      // 14 SemiBold
inline const lv_font_t* meta() { return &ui_font_meta; }        // 16 Medium
inline const lv_font_t* body() { return &ui_font_body; }        // 18 SemiBold
inline const lv_font_t* lead() { return &ui_font_lead; }        // 20 Medium
inline const lv_font_t* h2() { return &ui_font_h2; }            // 24 SemiBold
inline const lv_font_t* h1() { return &ui_font_h1; }            // 36 Bold
inline const lv_font_t* display() { return &ui_font_display; }  // 48 Bold
inline const lv_font_t* clock() { return &ui_font_clock; }      // 140 Light, digits

inline const lv_font_t* icons(int size) {
    return size <= 22 ? &ui_icons_22 : size <= 28 ? &ui_icons_28 : size <= 32 ? &ui_icons_32 : &ui_icons_48;
}

// Size
constexpr int32_t kScreenW = 1280, kScreenH = 800;
constexpr int32_t kSideW = 216, kBarH = 96, kPad = 32;
constexpr int32_t kPrimary = 64, kSecondary = 56, kChipH = 56, kGap = 8;
constexpr int32_t kNavH = 40, kNavHit = 8;  // visible 40 px, +8 px tap area above/below (56)
constexpr int32_t kMinTap = 56;             // smallest tap target (spec + WCAG 2.5.8 with margin)

// Placeholder artwork colour (before the real cover is decoded): muted, stable per uri.
lv_color_t art_colour(const char* key);

}  // namespace ui::theme
