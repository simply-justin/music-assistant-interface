#include "kit.hpp"

#include "i18n/i18n.hpp"
#include "text.hpp"

namespace ui::kit {

namespace {

void remove_style(lv_obj_t* o) {
    lv_obj_remove_style_all(o);
    lv_obj_remove_flag(o, LV_OBJ_FLAG_SCROLLABLE);
}

// Pressed = +12 % white over the control (spec §1); light controls darken instead.
void pressable(lv_obj_t* o, lv_color_t base, bool light = false) {
    lv_obj_set_style_bg_color(o, base, 0);
    lv_obj_set_style_bg_color(o, light ? lv_color_mix(lv_color_black(), base, 46) : lv_color_mix(lv_color_white(), base, 31),
                              LV_STATE_PRESSED);
    lv_obj_set_style_bg_opa(o, LV_OPA_COVER, LV_STATE_PRESSED);
}

void event(lv_obj_t* obj, lv_event_code_t code, Action action) {
    auto* fn = new Action(std::move(action));
    lv_obj_add_flag(obj, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(obj, [](lv_event_t* e) { (*static_cast<Action*>(lv_event_get_user_data(e)))(); }, code, fn);
    lv_obj_add_event_cb(obj, [](lv_event_t* e) { delete static_cast<Action*>(lv_event_get_user_data(e)); },
                        LV_EVENT_DELETE, fn);
}

char provider_letter(const std::string& p) {
    if (p == "library") return 'L';
    if (p.rfind("spotify", 0) == 0) return 'S';
    if (p.rfind("ytmusic", 0) == 0) return 'Y';
    if (p.rfind("tunein", 0) == 0) return 'T';
    if (p.rfind("radiobrowser", 0) == 0) return 'R';
    if (p.rfind("filesystem", 0) == 0) return 'F';
    return p.empty() ? '?' : static_cast<char>(toupper(p[0]));
}

}  // namespace

void on_click(lv_obj_t* obj, Action action) {
    if (action) event(obj, LV_EVENT_SHORT_CLICKED, std::move(action));
}
void on_long_press(lv_obj_t* obj, Action action) {
    if (action) event(obj, LV_EVENT_LONG_PRESSED, std::move(action));
}

void later(Action action) {
    lv_async_call([](void* p) {
        auto* fn = static_cast<Action*>(p);
        (*fn)();
        delete fn;
    }, new Action(std::move(action)));
}

lv_obj_t* column(lv_obj_t* parent, int32_t gap) {
    lv_obj_t* o = lv_obj_create(parent);
    remove_style(o);
    lv_obj_set_size(o, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(o, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(o, gap, 0);
    return o;
}

lv_obj_t* row(lv_obj_t* parent, int32_t gap) {
    lv_obj_t* o = lv_obj_create(parent);
    remove_style(o);
    lv_obj_set_size(o, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(o, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(o, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(o, gap, 0);
    return o;
}

lv_obj_t* spacer(lv_obj_t* parent) {
    lv_obj_t* o = lv_obj_create(parent);
    remove_style(o);
    lv_obj_set_size(o, 1, 1);
    lv_obj_set_flex_grow(o, 1);
    return o;
}

lv_obj_t* scroll_column(lv_obj_t* parent, int32_t gap) {
    lv_obj_t* o = column(parent, gap);
    lv_obj_add_flag(o, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(o, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(o, LV_SCROLLBAR_MODE_OFF);
    return o;
}

lv_obj_t* text(lv_obj_t* parent, const std::string& s, const lv_font_t* font, lv_color_t colour, bool wrap) {
    lv_obj_t* l = lv_label_create(parent);
    lv_label_set_text(l, clean(s).c_str());
    lv_obj_set_style_text_font(l, font, 0);
    lv_obj_set_style_text_color(l, colour, 0);
    if (!wrap) {
        lv_label_set_long_mode(l, LV_LABEL_LONG_DOT);
        // Clip to the parent only when the parent has a real width; in a content-sized parent
        // (a chip) "100 %" would resolve to 0 and hide the text.
        if (lv_obj_get_style_width(parent, LV_PART_MAIN) != LV_SIZE_CONTENT) lv_obj_set_style_max_width(l, lv_pct(100), 0);
    } else {
        lv_obj_set_width(l, lv_pct(100));
        lv_obj_set_style_text_line_space(l, 6, 0);
    }
    return l;
}

lv_obj_t* overline(lv_obj_t* parent, const std::string& s, lv_color_t colour) {
    std::string upper = s;
    for (auto& c : upper) c = static_cast<char>(toupper(static_cast<unsigned char>(c)));
    lv_obj_t* l = text(parent, upper, theme::label(), colour);
    lv_obj_set_style_text_letter_space(l, 1, 0);
    return l;
}

lv_obj_t* icon(lv_obj_t* parent, const char* glyph, int size, lv_color_t colour) {
    lv_obj_t* l = lv_label_create(parent);
    lv_label_set_text(l, glyph);
    lv_obj_set_style_text_font(l, theme::icons(size), 0);
    lv_obj_set_style_text_color(l, colour, 0);
    return l;
}

lv_obj_t* button(lv_obj_t* parent, Button kind, const char* glyph, const std::string& label, Action action) {
    lv_obj_t* b = row(parent, 10);
    lv_obj_set_height(b, kind == Button::Secondary ? theme::kSecondary : theme::kPrimary);
    lv_obj_set_style_pad_hor(b, 28, 0);
    lv_obj_set_style_radius(b, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_opa(b, kind == Button::Ghost ? LV_OPA_TRANSP : LV_OPA_COVER, 0);
    lv_obj_set_flex_align(b, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_color_t fg = theme::text();
    switch (kind) {
        case Button::Primary: pressable(b, theme::accent(), true); fg = theme::on_accent(); break;
        case Button::Secondary: pressable(b, theme::elevated()); break;
        case Button::Danger: pressable(b, theme::danger_fill()); break;
        case Button::Ghost:
            pressable(b, theme::bg());
            lv_obj_set_style_bg_opa(b, LV_OPA_TRANSP, 0);
            lv_obj_set_style_border_width(b, 1, 0);
            lv_obj_set_style_border_color(b, theme::border(), 0);
            break;
    }
    if (glyph) icon(b, glyph, 22, fg);
    text(b, label, theme::body(), fg);
    on_click(b, std::move(action));
    return b;
}

lv_obj_t* icon_button(lv_obj_t* parent, const char* glyph, Action action, int32_t size, int icon_size, bool filled,
                      lv_color_t colour) {
    lv_obj_t* b = lv_obj_create(parent);
    remove_style(b);
    lv_obj_set_size(b, size, size);
    lv_obj_set_style_radius(b, LV_RADIUS_CIRCLE, 0);
    pressable(b, filled ? theme::elevated() : theme::surface());
    lv_obj_set_style_bg_opa(b, filled ? LV_OPA_COVER : LV_OPA_TRANSP, 0);
    lv_obj_t* i = icon(b, glyph, icon_size, colour);
    lv_obj_center(i);
    on_click(b, std::move(action));
    return b;
}

lv_obj_t* play_button(lv_obj_t* parent, bool playing, Action action, int32_t size) {
    lv_obj_t* b = icon_button(parent, playing ? icon::pause : icon::play, std::move(action), size, size >= 96 ? 48 : 32,
                              true, theme::on_accent());
    pressable(b, theme::accent(), true);
    return b;
}

lv_obj_t* chip(lv_obj_t* parent, const char* glyph, const std::string& label, bool selected, Action action) {
    lv_obj_t* c = row(parent, 8);
    lv_obj_set_height(c, theme::kChipH);
    lv_obj_set_style_pad_hor(c, 22, 0);
    lv_obj_set_style_radius(c, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_opa(c, LV_OPA_COVER, 0);
    pressable(c, selected ? theme::text() : theme::surface(), selected);
    lv_color_t fg = selected ? theme::bg() : theme::text();
    if (glyph) icon(c, glyph, 22, selected ? fg : theme::text2());
    text(c, label, theme::body(), fg);
    on_click(c, std::move(action));
    return c;
}

lv_obj_t* back_chip(lv_obj_t* parent, const std::string& label, Action action) {
    lv_obj_t* c = row(parent, 4);
    lv_obj_set_height(c, theme::kChipH);
    lv_obj_set_style_pad_right(c, 18, 0);
    lv_obj_set_style_radius(c, LV_RADIUS_CIRCLE, 0);
    pressable(c, theme::bg());
    lv_obj_set_style_bg_opa(c, LV_OPA_TRANSP, 0);
    icon(c, icon::chevron_left, 28, theme::text2());
    text(c, label, theme::body(), theme::text2());
    on_click(c, std::move(action));
    return c;
}

lv_obj_t* art(lv_obj_t* parent, const core::MediaItem& item, int32_t size, int32_t radius) {
    lv_obj_t* a = lv_obj_create(parent);
    remove_style(a);
    lv_obj_set_size(a, size, size);
    lv_obj_set_style_radius(a, item.type == core::MediaType::Artist ? LV_RADIUS_CIRCLE : radius, 0);
    lv_obj_set_style_bg_opa(a, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(a, theme::art_colour(item.uri.empty() ? item.name.c_str() : item.uri.c_str()), 0);
    // shortcut: flat placeholder; the artwork pipeline (HW JPEG + LRU cache) replaces this in Phase 4.
    const char* glyph = item.type == core::MediaType::Radio ? icon::radio : item.type == core::MediaType::Artist ? nullptr : nullptr;
    if (glyph) lv_obj_center(icon(a, glyph, size >= 120 ? 48 : 22, lv_color_hex(0xFFFFFF)));
    return a;
}

lv_obj_t* badge(lv_obj_t* parent, const std::string& provider) {
    lv_obj_t* b = lv_obj_create(parent);
    remove_style(b);
    lv_obj_set_size(b, 20, 20);
    lv_obj_set_style_radius(b, 5, 0);
    lv_obj_set_style_bg_opa(b, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(b, theme::elevated(), 0);
    char s[2] = {provider_letter(provider), 0};
    lv_obj_t* l = text(b, s, theme::label(), theme::text2());
    lv_obj_center(l);
    return b;
}

lv_obj_t* tag(lv_obj_t* parent, const std::string& s, lv_color_t colour) {
    lv_obj_t* t = row(parent);
    lv_obj_set_height(t, 32);
    lv_obj_set_style_pad_hor(t, 12, 0);
    lv_obj_set_style_radius(t, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_width(t, 1, 0);
    lv_obj_set_style_border_color(t, theme::border(), 0);
    text(t, s, theme::label(), colour);
    return t;
}

std::string subtitle(const core::MediaItem& m) {
    using i18n::Str;
    std::string artists;
    for (const auto& a : m.artists) artists += (artists.empty() ? "" : ", ") + a.name;
    switch (m.type) {
        case core::MediaType::Track: return artists;
        case core::MediaType::Album: return artists.empty() ? i18n::tr(Str::type_album) : std::string(i18n::tr(Str::type_album)) + " · " + artists;
        case core::MediaType::Artist: return i18n::tr(Str::type_artist);
        case core::MediaType::Playlist: return i18n::tr(Str::type_playlist);
        case core::MediaType::Radio: return i18n::tr(Str::type_radio);
        default: return {};
    }
}

std::string provider_name(const std::string& domain) {
    if (domain == "library") return i18n::tr(i18n::Str::menu_your_library);
    if (domain.rfind("spotify", 0) == 0) return "Spotify";
    if (domain.rfind("ytmusic", 0) == 0) return "YouTube Music";
    if (domain.rfind("tunein", 0) == 0) return "TuneIn";
    if (domain.rfind("radiobrowser", 0) == 0) return "Radio Browser";
    if (domain.rfind("filesystem", 0) == 0) return i18n::tr(i18n::Str::provider_files);
    std::string s = domain.substr(0, domain.find("--"));
    if (!s.empty()) s[0] = static_cast<char>(toupper(static_cast<unsigned char>(s[0])));
    return s;
}

lv_obj_t* media_row(lv_obj_t* parent, const core::MediaItem& item, const RowOptions& opt) {
    lv_obj_t* r = row(parent, 16);
    lv_obj_set_size(r, lv_pct(100), opt.height);
    lv_obj_set_style_pad_hor(r, 12, 0);
    lv_obj_set_style_radius(r, theme::kRadiusRow, 0);
    pressable(r, opt.playing ? theme::surface() : theme::bg());
    lv_obj_set_style_bg_opa(r, opt.playing ? LV_OPA_COVER : LV_OPA_TRANSP, 0);
    art(r, item, opt.height - 20, 8);
    lv_obj_t* col = column(r, 4);
    lv_obj_set_flex_grow(col, 1);
    lv_obj_set_width(col, 1);  // let flex-grow decide; enables "…" clipping
    text(col, item.name + (item.version.empty() ? "" : " (" + item.version + ")"), theme::body(),
         opt.playing ? theme::accent() : theme::text());
    std::string sub = subtitle(item);
    if (!sub.empty()) text(col, sub, theme::meta(), theme::text2());
    if (!item.alternatives.empty())
        tag(r, i18n::tr(item.alternatives.size() == 1 ? i18n::Str::tag_source : i18n::Str::tag_sources,
                        std::to_string(item.alternatives.size())));
    badge(r, item.badge_provider());
    if (opt.show_duration && item.duration_s) {
        lv_obj_t* d = text(r, duration(item.duration_s), theme::meta(), theme::text3());
        lv_obj_set_width(d, 56);
        lv_obj_set_style_text_align(d, LV_TEXT_ALIGN_RIGHT, 0);
    }
    if (opt.on_more) icon_button(r, icon::more, opt.on_more, 56, 28, false, theme::text2());
    if (opt.on_remove) icon_button(r, icon::close, opt.on_remove, 56, 22, true, theme::text2());
    on_click(r, opt.on_tap);
    return r;
}

lv_obj_t* media_card(lv_obj_t* parent, const core::MediaItem& item, const std::string& sub, Action on_tap, int32_t size) {
    lv_obj_t* c = column(parent, 10);
    lv_obj_set_width(c, size);
    lv_obj_set_style_radius(c, theme::kRadiusTile, 0);
    pressable(c, theme::bg());
    lv_obj_set_style_bg_opa(c, LV_OPA_TRANSP, 0);
    art(c, item, size, theme::kRadiusTile);
    lv_obj_t* t = text(c, item.name, theme::meta(), theme::text());
    lv_obj_set_width(t, size);
    if (!sub.empty()) {
        lv_obj_t* s = text(c, sub, theme::label(), theme::text2());
        lv_obj_set_width(s, size);
    }
    on_click(c, std::move(on_tap));
    return c;
}

lv_obj_t* favourite_tile(lv_obj_t* parent, const core::MediaItem& item, Action on_tap, Action long_press) {
    lv_obj_t* t = row(parent, 16);
    lv_obj_set_height(t, 80);
    lv_obj_set_style_radius(t, theme::kRadiusTile, 0);
    lv_obj_set_style_clip_corner(t, true, 0);
    lv_obj_set_style_bg_opa(t, LV_OPA_COVER, 0);
    pressable(t, theme::surface());
    lv_obj_set_style_pad_right(t, 16, 0);
    art(t, item, 80, 0);
    lv_obj_t* col = column(t, 4);
    lv_obj_set_flex_grow(col, 1);
    lv_obj_set_width(col, 1);
    text(col, item.name, theme::body(), theme::text());
    text(col, subtitle(item), theme::meta(), theme::text2());
    on_click(t, std::move(on_tap));
    on_long_press(t, std::move(long_press));
    return t;
}

lv_obj_t* slider(lv_obj_t* parent, int value, std::function<void(int)> on_release, lv_color_t fill) {
    lv_obj_add_flag(parent, LV_OBJ_FLAG_OVERFLOW_VISIBLE);  // the 36 px knob sticks out of the 8 px track
    lv_obj_t* s = lv_slider_create(parent);
    lv_slider_set_range(s, 0, 100);
    lv_slider_set_value(s, value, LV_ANIM_OFF);
    lv_obj_set_height(s, 8);
    lv_obj_set_ext_click_area(s, 24);  // 8 + 2*24 = 56 px hit area
    lv_obj_set_style_bg_color(s, theme::elevated(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(s, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_bg_color(s, fill, LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(s, theme::text(), LV_PART_KNOB);
    lv_obj_set_style_pad_all(s, 14, LV_PART_KNOB);  // 8 + 2*14 = 36 px thumb
    lv_obj_set_style_bg_color(s, theme::text2(), LV_PART_KNOB | LV_STATE_PRESSED);
    if (on_release) {
        auto* fn = new std::function<void(int)>(std::move(on_release));
        lv_obj_add_event_cb(s, [](lv_event_t* e) {
            auto* f = static_cast<std::function<void(int)>*>(lv_event_get_user_data(e));
            (*f)(lv_slider_get_value(static_cast<lv_obj_t*>(lv_event_get_target(e))));
        }, LV_EVENT_RELEASED, fn);
        lv_obj_add_event_cb(s, [](lv_event_t* e) { delete static_cast<std::function<void(int)>*>(lv_event_get_user_data(e)); },
                            LV_EVENT_DELETE, fn);
    }
    return s;
}

lv_obj_t* toggle(lv_obj_t* parent, bool on, std::function<void(bool)> on_change) {
    lv_obj_t* sw = lv_switch_create(parent);
    lv_obj_set_size(sw, 64, 36);
    lv_obj_set_ext_click_area(sw, 10);
    lv_obj_set_style_bg_color(sw, theme::elevated(), LV_PART_MAIN);
    lv_obj_set_style_bg_color(sw, theme::accent(), LV_PART_INDICATOR | LV_STATE_CHECKED);
    lv_obj_set_style_bg_color(sw, theme::text(), LV_PART_KNOB);
    if (on) lv_obj_add_state(sw, LV_STATE_CHECKED);
    if (on_change) {
        auto* fn = new std::function<void(bool)>(std::move(on_change));
        lv_obj_add_event_cb(sw, [](lv_event_t* e) {
            auto* o = static_cast<lv_obj_t*>(lv_event_get_target(e));
            (*static_cast<std::function<void(bool)>*>(lv_event_get_user_data(e)))(lv_obj_has_state(o, LV_STATE_CHECKED));
        }, LV_EVENT_VALUE_CHANGED, fn);
        lv_obj_add_event_cb(sw, [](lv_event_t* e) { delete static_cast<std::function<void(bool)>*>(lv_event_get_user_data(e)); },
                            LV_EVENT_DELETE, fn);
    }
    return sw;
}

namespace {
struct Keys {
    std::string del, done, space;
    const char* letters[38];
    const char* digits[38];
    lv_obj_t* input;
    Action on_done;
};
// Relative key widths (LVGL 9 keeps the width in the low bits of the ctrl enum); specials are wider.
constexpr lv_buttonmatrix_ctrl_t w(int n) { return static_cast<lv_buttonmatrix_ctrl_t>(n); }
const lv_buttonmatrix_ctrl_t kKeyCtrl[] = {w(4), w(4), w(4), w(4), w(4), w(4), w(4), w(4), w(4), w(4), w(7),
                                           w(4), w(4), w(4), w(4), w(4), w(4), w(4), w(4), w(4), w(8),
                                           w(4), w(4), w(4), w(4), w(4), w(4), w(4), w(4), w(4), w(4),
                                           w(5), w(15), w(5)};
}  // namespace

lv_obj_t* keyboard(lv_obj_t* parent, lv_obj_t* input, const std::string& del, const std::string& done,
                   const std::string& space, Action on_done) {
    auto* k = new Keys{del, done, space, {}, {}, input, std::move(on_done)};
    const char* d = k->del.c_str();
    const char* g = k->done.c_str();
    const char* sp = k->space.c_str();
    const char* letters[] = {"q", "w", "e", "r", "t", "y", "u", "i", "o", "p", d, "\n",
                             "a", "s", "d", "f", "g", "h", "j", "k", "l", g, "\n",
                             "z", "x", "c", "v", "b", "n", "m", ",", ".", "'", "\n",
                             "123", sp, "-", ""};
    const char* digits[] = {"1", "2", "3", "4", "5", "6", "7", "8", "9", "0", d, "\n",
                            "&", "(", ")", "!", "?", ":", "/", "+", "#", g, "\n",
                            "é", "è", "ë", "ü", "ö", "ä", "ç", "ñ", "ø", "å", "\n",
                            "abc", sp, "-", ""};
    static_assert(sizeof letters / sizeof *letters == 38 && sizeof digits / sizeof *digits == 38, "key maps");
    std::copy(std::begin(letters), std::end(letters), k->letters);
    std::copy(std::begin(digits), std::end(digits), k->digits);

    lv_obj_t* kb = lv_keyboard_create(parent);
    lv_obj_remove_event_cb(kb, lv_keyboard_def_event_cb);
    lv_keyboard_set_map(kb, LV_KEYBOARD_MODE_TEXT_LOWER, k->letters, kKeyCtrl);
    lv_keyboard_set_map(kb, LV_KEYBOARD_MODE_NUMBER, k->digits, kKeyCtrl);
    lv_keyboard_set_mode(kb, LV_KEYBOARD_MODE_TEXT_LOWER);
    lv_obj_set_size(kb, theme::kScreenW, 316);
    lv_obj_set_style_bg_color(kb, theme::surface(), 0);
    lv_obj_set_style_bg_opa(kb, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(kb, 0, 0);
    lv_obj_set_style_radius(kb, 0, 0);
    lv_obj_set_style_pad_all(kb, 16, 0);
    lv_obj_set_style_pad_gap(kb, 10, 0);
    lv_obj_set_style_bg_color(kb, theme::elevated(), LV_PART_ITEMS);
    lv_obj_set_style_bg_color(kb, lv_color_mix(lv_color_white(), theme::elevated(), 31), LV_PART_ITEMS | LV_STATE_PRESSED);
    lv_obj_set_style_text_color(kb, theme::text(), LV_PART_ITEMS);
    lv_obj_set_style_text_font(kb, theme::body(), LV_PART_ITEMS);
    lv_obj_set_style_radius(kb, 12, LV_PART_ITEMS);
    lv_obj_set_style_border_width(kb, 0, LV_PART_ITEMS);
    lv_obj_set_style_shadow_width(kb, 0, LV_PART_ITEMS);
    lv_obj_add_event_cb(kb, [](lv_event_t* e) {
        auto* k = static_cast<Keys*>(lv_event_get_user_data(e));
        auto* kb = static_cast<lv_obj_t*>(lv_event_get_target(e));
        uint32_t id = lv_buttonmatrix_get_selected_button(kb);
        if (id == LV_BUTTONMATRIX_BUTTON_NONE) return;
        std::string key = lv_buttonmatrix_get_button_text(kb, id);
        if (key == k->del) lv_textarea_delete_char(k->input);
        else if (key == k->done) { if (k->on_done) k->on_done(); }
        else if (key == k->space) lv_textarea_add_char(k->input, ' ');
        else if (key == "123") lv_keyboard_set_mode(kb, LV_KEYBOARD_MODE_NUMBER);
        else if (key == "abc") lv_keyboard_set_mode(kb, LV_KEYBOARD_MODE_TEXT_LOWER);
        else lv_textarea_add_text(k->input, key.c_str());
    }, LV_EVENT_VALUE_CHANGED, k);
    lv_obj_add_event_cb(kb, [](lv_event_t* e) { delete static_cast<Keys*>(lv_event_get_user_data(e)); }, LV_EVENT_DELETE, k);
    return kb;
}

lv_obj_t* sheet(int32_t width, Action on_close) {
    lv_obj_t* scrim = lv_obj_create(lv_layer_top());
    remove_style(scrim);
    lv_obj_set_size(scrim, theme::kScreenW, theme::kScreenH);
    lv_obj_set_style_bg_color(scrim, lv_color_hex(0x05080C), 0);
    lv_obj_set_style_bg_opa(scrim, theme::kScrimOpa, 0);
    lv_obj_add_flag(scrim, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_user_data(scrim, reinterpret_cast<void*>(0x5CA1));  // marks it for close_sheets()
    lv_obj_add_event_cb(scrim, [](lv_event_t* e) {
        if (lv_event_get_target(e) != lv_event_get_current_target(e)) return;  // tap on the panel, not the scrim
        lv_obj_delete_async(static_cast<lv_obj_t*>(lv_event_get_current_target(e)));
    }, LV_EVENT_SHORT_CLICKED, nullptr);
    if (on_close) event(scrim, LV_EVENT_DELETE, std::move(on_close));

    lv_obj_t* panel = column(scrim, 20);
    lv_obj_set_width(panel, width);
    lv_obj_set_style_max_height(panel, theme::kScreenH - 64, 0);
    lv_obj_set_style_pad_all(panel, 28, 0);
    lv_obj_set_style_radius(panel, theme::kRadiusSheet, 0);
    lv_obj_set_style_bg_color(panel, theme::surface(), 0);
    lv_obj_set_style_bg_opa(panel, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(panel, 1, 0);
    lv_obj_set_style_border_color(panel, theme::border(), 0);
    lv_obj_add_flag(panel, LV_OBJ_FLAG_CLICKABLE);  // swallow taps so they don't reach the scrim
    lv_obj_center(panel);
    // Motion: panel rises 16 px over 150 ms (spec §1). Only translate: no opacity on containers.
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, panel);
    lv_anim_set_values(&a, 16, 0);
    lv_anim_set_duration(&a, 150);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
    lv_anim_set_exec_cb(&a, [](void* o, int32_t v) { lv_obj_set_style_translate_y(static_cast<lv_obj_t*>(o), v, 0); });
    lv_anim_start(&a);
    return panel;
}

void close_sheets() {
    lv_obj_t* top = lv_layer_top();
    for (int32_t i = static_cast<int32_t>(lv_obj_get_child_count(top)) - 1; i >= 0; --i) {
        lv_obj_t* c = lv_obj_get_child(top, i);
        if (lv_obj_get_user_data(c) == reinterpret_cast<void*>(0x5CA1)) lv_obj_delete_async(c);
    }
}

void toast(const std::string& message, const std::string& action_label, Action action, bool error) {
    static lv_obj_t* current = nullptr;
    if (current) lv_obj_delete(current);  // one toast at a time
    lv_obj_t* t = row(lv_layer_top(), 24);
    current = t;
    lv_obj_add_event_cb(t, [](lv_event_t*) { current = nullptr; }, LV_EVENT_DELETE, nullptr);
    lv_obj_set_height(t, 72);
    lv_obj_set_style_pad_hor(t, 28, 0);
    lv_obj_set_style_radius(t, 36, 0);
    lv_obj_set_style_bg_color(t, theme::elevated(), 0);
    lv_obj_set_style_bg_opa(t, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(t, 1, 0);
    lv_obj_set_style_border_color(t, theme::border(), 0);
    icon(t, error ? icon::warning : icon::check, 22, error ? theme::warning() : theme::accent());
    text(t, message, theme::body(), theme::text());
    if (!action_label.empty() && action) {
        lv_obj_t* b = button(t, Button::Ghost, nullptr, action_label, [action] {
            action();
            if (current) lv_obj_delete_async(current);
        });
        lv_obj_set_height(b, 56);
    }
    lv_obj_align(t, LV_ALIGN_BOTTOM_MID, theme::kRailW / 2, -120);
    lv_obj_delete_delayed(t, 4000);
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, t);
    lv_anim_set_values(&a, 24, 0);
    lv_anim_set_duration(&a, 150);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
    lv_anim_set_exec_cb(&a, [](void* o, int32_t v) { lv_obj_set_style_translate_y(static_cast<lv_obj_t*>(o), v, 0); });
    lv_anim_start(&a);
}

}  // namespace ui::kit
