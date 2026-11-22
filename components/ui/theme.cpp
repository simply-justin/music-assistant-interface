#include "theme.hpp"

#include "img/artwork.hpp"

namespace ui::theme {

namespace {

struct Palette {
    uint32_t bg, side, surface, elevated, hairline, border, text, text2, text3, primary, on_primary, field_bg, field_text,
        warning, danger_text, danger_fill, live;
};

// Neutral black/grey with white as the primary (dark), and its light counterpart.
constexpr Palette kDark{0x0A0A0A, 0x000000, 0x161616, 0x242424, 0x1C1C1C, 0x333333, 0xFFFFFF, 0xA3A3A3, 0x8F8F8F,
                        0xFFFFFF, 0x000000, 0xFFFFFF, 0x111111, 0xE7B14C, 0xF08C7E, 0xB3261E, 0xC2410C};
constexpr Palette kLight{0xF6F6F7, 0xFFFFFF, 0xECECEE, 0xE0E0E4, 0xE4E4E7, 0xCFCFD4, 0x111111, 0x52525B, 0x63636B,
                         0x111111, 0xFFFFFF, 0xFFFFFF, 0x111111, 0x8A4B00, 0xB42318, 0xB3261E, 0xC2410C};

// Accents: {dark mode, light mode}. Index 0 = neutral (the primary).
struct Accent {
    uint32_t dark, light;
};
constexpr Accent kAccents[] = {{0, 0}, {0x5CC8B8, 0x0F766E}, {0x6EA8FF, 0x1D4ED8}, {0xF5A15C, 0xC2410C},
                               {0xF28AB8, 0xBE185D}, {0x5FD39A, 0x15803D}};

struct Radii {
    int32_t art, control, card, sheet, button;
};
constexpr Radii kRadii[] = {{2, 4, 4, 8, 4}, {6, 8, 8, 12, LV_RADIUS_CIRCLE}, {12, 14, 16, 20, LV_RADIUS_CIRCLE}};

app::Appearance g_look;
const Palette* g_p = &kDark;

lv_color_t c(uint32_t v) { return lv_color_hex(v); }

}  // namespace

void apply(const app::Appearance& a) {
    g_look = a;
    if (g_look.accent >= accent_count()) g_look.accent = 0;
    g_p = a.mode == app::ThemeMode::Light ? &kLight : &kDark;
}
const app::Appearance& appearance() { return g_look; }
bool light() { return g_look.mode == app::ThemeMode::Light; }
bool motion() { return !g_look.reduce_motion; }

lv_color_t bg() { return c(g_p->bg); }
lv_color_t side() { return c(g_p->side); }
lv_color_t surface() { return c(g_p->surface); }
lv_color_t elevated() { return c(g_p->elevated); }
lv_color_t hairline() { return c(g_p->hairline); }
lv_color_t border() { return c(g_p->border); }
lv_color_t text() { return c(g_p->text); }
lv_color_t text2() { return c(g_p->text2); }
lv_color_t text3() { return c(g_p->text3); }
lv_color_t field_bg() { return c(g_p->field_bg); }
lv_color_t field_text() { return c(g_p->field_text); }
lv_color_t warning() { return c(g_p->warning); }
lv_color_t danger_text() { return c(g_p->danger_text); }
lv_color_t danger_fill() { return c(g_p->danger_fill); }
lv_color_t live() { return c(g_p->live); }

int accent_count() { return static_cast<int>(sizeof kAccents / sizeof *kAccents); }

lv_color_t accent_swatch(int i) {
    if (i <= 0 || i >= accent_count()) return c(g_p->primary);
    return c(light() ? kAccents[i].light : kAccents[i].dark);
}
lv_color_t accent() { return accent_swatch(g_look.accent); }
lv_color_t on_accent() {
    // Neutral accent is the primary (white on dark, black on light); coloured accents carry
    // black text in dark mode and white text in light mode (all ≥ 4.5:1).
    return g_look.accent == 0 ? c(g_p->on_primary) : light() ? c(0xFFFFFF) : c(0x000000);
}

lv_color_t art_tint(lv_color_t art) {
    uint32_t rgb = lv_color_to_u32(art) & 0xFFFFFF;
    return c(light() ? img::art_background_light(rgb) : img::art_background(rgb));
}

int32_t r_art() { return kRadii[static_cast<int>(g_look.roundness)].art; }
int32_t r_control() { return kRadii[static_cast<int>(g_look.roundness)].control; }
int32_t r_card() { return kRadii[static_cast<int>(g_look.roundness)].card; }
int32_t r_sheet() { return kRadii[static_cast<int>(g_look.roundness)].sheet; }
int32_t r_button() { return kRadii[static_cast<int>(g_look.roundness)].button; }

lv_color_t art_colour(const char* key) {
    static const uint32_t palette[] = {0x8C3B4A, 0x4E7D3A, 0xC9A227, 0x2A3F5F, 0xB5673A, 0x3B5BA9,
                                       0x6B7F8E, 0x2F6E62, 0x6B4FA0, 0x4A6A8A, 0xD8572A, 0x2F2F38};
    uint32_t h = 2166136261u;
    for (const char* p = key; p && *p; ++p) h = (h ^ static_cast<unsigned char>(*p)) * 16777619u;
    return c(palette[h % (sizeof palette / sizeof *palette)]);
}

}  // namespace ui::theme
