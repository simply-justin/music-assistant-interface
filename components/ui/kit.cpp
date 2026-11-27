#include "kit.hpp"

#include <map>
#include <vector>

#include "artwork.hpp"
#include "i18n/i18n.hpp"
#include "text.hpp"

namespace ui::kit {

namespace {

// Plain containers must not catch taps: lv_obj_create makes objects clickable, so a text column
// inside a row would swallow the tap and the row (the real target) never sees it. on_click()
// turns clickability back on for the objects that act.
void remove_style(lv_obj_t* o) {
    lv_obj_remove_style_all(o);
    lv_obj_remove_flag(o, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_remove_flag(o, LV_OBJ_FLAG_CLICKABLE);
}

// Pressed = +12 % white over a dark control, −10 % on a light one (docs/design.md, Motion).
void pressable(lv_obj_t* o, lv_color_t base, bool light = false) {
    bool darken = light || theme::light();
    lv_obj_set_style_bg_color(o, base, 0);
    lv_obj_set_style_bg_color(o, darken ? lv_color_mix(lv_color_black(), base, 26) : lv_color_mix(lv_color_white(), base, 31),
                              LV_STATE_PRESSED);
    lv_obj_set_style_bg_opa(o, LV_OPA_COVER, LV_STATE_PRESSED);
}

// Micro-animation: shrink to 96 % while pressed, 120 ms. Used on tiles, cards and buttons only
// (a transform renders the object as a layer: keep it to small, leaf-like objects).
void shrink_on_press(lv_obj_t* o) {
    if (!theme::motion()) return;
    static lv_style_t style;
    static lv_style_transition_dsc_t trans;
    static bool ready = false;
    if (!ready) {
        static const lv_style_prop_t props[] = {LV_STYLE_TRANSFORM_SCALE_X, LV_STYLE_TRANSFORM_SCALE_Y, LV_STYLE_PROP_INV};
        lv_style_transition_dsc_init(&trans, props, lv_anim_path_ease_out, 120, 0, nullptr);
        lv_style_init(&style);
        lv_style_set_transform_scale_x(&style, 246);
        lv_style_set_transform_scale_y(&style, 246);
        lv_style_set_transition(&style, &trans);
        ready = true;
    }
    lv_obj_set_style_transform_pivot_x(o, lv_pct(50), 0);
    lv_obj_set_style_transform_pivot_y(o, lv_pct(50), 0);
    lv_obj_add_style(o, &style, LV_STATE_PRESSED);
    lv_obj_set_style_transition(o, &trans, 0);  // animate back too
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

void press_scale(lv_obj_t* obj) { shrink_on_press(obj); }

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
    lv_obj_add_flag(o, LV_OBJ_FLAG_CLICKABLE);  // so a drag on empty space still scrolls
    lv_obj_set_scroll_dir(o, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(o, LV_SCROLLBAR_MODE_OFF);
    return o;
}

namespace {
struct VList {
    size_t count;
    int32_t step;  // row height + gap
    std::function<lv_obj_t*(lv_obj_t*, size_t)> make;
    std::map<size_t, lv_obj_t*> rows;
};
constexpr size_t kSpare = 3;  // rows kept above/below the viewport

void vlist_update(lv_obj_t* list) {
    auto* v = static_cast<VList*>(lv_obj_get_user_data(list));
    int32_t h = lv_obj_get_height(list);
    if (h <= 0 || v->count == 0) return;
    int32_t y = lv_obj_get_scroll_y(list);
    size_t first = static_cast<size_t>(std::max<int32_t>(0, y / v->step)) ;
    first = first > kSpare ? first - kSpare : 0;
    size_t last = std::min(v->count, static_cast<size_t>((y + h) / v->step) + 1 + kSpare);
    for (auto it = v->rows.begin(); it != v->rows.end();) {
        if (it->first < first || it->first >= last) {
            lv_obj_delete(it->second);
            it = v->rows.erase(it);
        } else {
            ++it;
        }
    }
    for (size_t i = first; i < last; ++i) {
        if (v->rows.count(i)) continue;
        lv_obj_t* row = v->make(list, i);
        lv_obj_set_pos(row, 0, static_cast<int32_t>(i) * v->step);
        v->rows[i] = row;
    }
}
}  // namespace

lv_obj_t* virtual_list(lv_obj_t* parent, size_t count, int32_t row_height, int32_t gap,
                       std::function<lv_obj_t*(lv_obj_t*, size_t)> make) {
    lv_obj_t* list = lv_obj_create(parent);
    remove_style(list);
    lv_obj_add_flag(list, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(list, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_scroll_dir(list, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(list, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_width(list, lv_pct(100));
    lv_obj_set_flex_grow(list, 1);
    // An invisible 1 px marker at the end gives the list its full scroll height.
    lv_obj_t* end = lv_obj_create(list);
    remove_style(end);
    lv_obj_set_size(end, 1, 1);
    lv_obj_set_pos(end, 0, static_cast<int32_t>(count) * (row_height + gap) + 24);
    lv_obj_set_user_data(list, new VList{count, row_height + gap, std::move(make), {}});
    lv_obj_add_event_cb(list, [](lv_event_t* e) { vlist_update(static_cast<lv_obj_t*>(lv_event_get_current_target(e))); },
                        LV_EVENT_SCROLL, nullptr);
    lv_obj_add_event_cb(list, [](lv_event_t* e) { vlist_update(static_cast<lv_obj_t*>(lv_event_get_current_target(e))); },
                        LV_EVENT_SIZE_CHANGED, nullptr);
    lv_obj_add_event_cb(list, [](lv_event_t* e) {
        auto* l = static_cast<lv_obj_t*>(lv_event_get_current_target(e));
        delete static_cast<VList*>(lv_obj_get_user_data(l));
    }, LV_EVENT_DELETE, nullptr);
    return list;
}

lv_obj_t* text(lv_obj_t* parent, const std::string& s, const lv_font_t* font, lv_color_t colour, bool wrap) {
    lv_obj_t* l = lv_label_create(parent);
    lv_obj_remove_flag(l, LV_OBJ_FLAG_SCROLLABLE);  // a clipped label would take the drag and the page wouldn't scroll
    lv_label_set_text(l, clean(s).c_str());
    lv_obj_set_style_text_font(l, font, 0);
    lv_obj_set_style_text_color(l, colour, 0);
    if (!wrap) {
        lv_label_set_long_mode(l, LV_LABEL_LONG_DOT);
        // One line: a fixed line height makes LONG_DOT cut with "…" instead of wrapping.
        lv_obj_set_height(l, lv_font_get_line_height(font));
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
    int32_t height = kind == Button::Secondary || kind == Button::Ghost ? 48 : theme::kSecondary;
    lv_obj_set_height(b, height);
    lv_obj_set_ext_click_area(b, (theme::kMinTap - height) / 2);  // 48 px looks, 56 px hits
    lv_obj_set_style_pad_hor(b, 22, 0);
    lv_obj_set_style_radius(b, theme::r_button(), 0);
    lv_obj_set_flex_align(b, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_color_t fg = theme::text();
    switch (kind) {
        case Button::Primary:
            pressable(b, theme::accent(), true);
            lv_obj_set_style_bg_opa(b, LV_OPA_COVER, 0);
            fg = theme::on_accent();
            break;
        case Button::Danger:
            pressable(b, theme::danger_fill());
            lv_obj_set_style_bg_opa(b, LV_OPA_COVER, 0);
            fg = lv_color_white();
            break;
        case Button::Secondary:  // outlined
        case Button::Ghost:
            pressable(b, theme::surface());
            lv_obj_set_style_bg_opa(b, LV_OPA_TRANSP, 0);
            lv_obj_set_style_border_width(b, 1, 0);
            lv_obj_set_style_border_color(b, theme::border(), 0);
            break;
    }
    if (glyph) icon(b, glyph, 22, fg);
    text(b, label, theme::body(), fg);
    shrink_on_press(b);
    on_click(b, std::move(action));
    return b;
}

lv_obj_t* icon_button(lv_obj_t* parent, const char* glyph, Action action, int32_t size, int icon_size, bool filled,
                      lv_color_t colour) {
    lv_obj_t* b = lv_obj_create(parent);
    remove_style(b);
    lv_obj_set_size(b, size, size);
    if (size < theme::kMinTap) lv_obj_set_ext_click_area(b, (theme::kMinTap - size) / 2);
    lv_obj_set_style_radius(b, LV_RADIUS_CIRCLE, 0);
    pressable(b, theme::elevated());
    lv_obj_set_style_bg_opa(b, LV_OPA_TRANSP, 0);
    if (filled) {  // a quiet outline, not a blob
        lv_obj_set_style_border_width(b, 1, 0);
        lv_obj_set_style_border_color(b, theme::border(), 0);
    }
    lv_obj_t* i = icon(b, glyph, icon_size, colour);
    lv_obj_center(i);
    on_click(b, std::move(action));
    return b;
}

lv_obj_t* play_button(lv_obj_t* parent, bool playing, Action action, int32_t size) {
    lv_obj_t* b = icon_button(parent, playing ? icon::pause : icon::play, std::move(action), size, size >= 80 ? 48 : size >= 56 ? 28 : 22,
                              false, theme::on_accent());
    pressable(b, theme::accent(), true);
    lv_obj_set_style_bg_opa(b, LV_OPA_COVER, 0);
    shrink_on_press(b);
    return b;
}

lv_obj_t* chip(lv_obj_t* parent, const char* glyph, const std::string& label, bool selected, Action action) {
    lv_obj_t* c = row(parent, 8);
    lv_obj_set_height(c, 48);
    lv_obj_set_ext_click_area(c, 4);
    lv_obj_set_style_pad_hor(c, 18, 0);
    lv_obj_set_style_radius(c, theme::r_button(), 0);
    pressable(c, theme::surface());
    lv_obj_set_style_bg_opa(c, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(c, 1, 0);
    lv_obj_set_style_border_color(c, selected ? theme::accent() : theme::border(), 0);
    lv_color_t fg = selected ? theme::accent() : theme::text();
    if (glyph) icon(c, glyph, 22, selected ? fg : theme::text2());
    text(c, label, theme::meta(), fg);
    on_click(c, std::move(action));
    return c;
}

lv_obj_t* tab(lv_obj_t* parent, const std::string& label, bool selected, Action action) {
    lv_obj_t* t = row(parent);
    lv_obj_set_height(t, 48);
    lv_obj_set_ext_click_area(t, 4);
    lv_obj_set_style_margin_right(t, 24, 0);
    lv_obj_set_style_border_side(t, LV_BORDER_SIDE_BOTTOM, 0);
    lv_obj_set_style_border_width(t, selected ? 2 : 0, 0);
    lv_obj_set_style_border_color(t, theme::accent(), 0);
    text(t, label, theme::meta(), selected ? theme::text() : theme::text3());
    on_click(t, std::move(action));
    return t;
}

lv_obj_t* section_header(lv_obj_t* parent, const std::string& title, const std::string& link, Action on_link) {
    lv_obj_t* h = row(parent, 12);
    lv_obj_set_size(h, lv_pct(100), 40);
    lv_obj_t* t = text(h, title, theme::h2(), theme::text());
    lv_obj_set_flex_grow(t, 1);
    if (!link.empty()) {
        lv_obj_t* l = row(h);
        lv_obj_set_height(l, 40);
        lv_obj_set_ext_click_area(l, 8);
        lv_obj_set_style_pad_hor(l, 8, 0);
        lv_obj_set_style_margin_right(l, -8, 0);  // the text, not the press area, lines up with the content edge
        lv_obj_set_style_radius(l, theme::r_control(), 0);
        lv_obj_set_style_bg_color(l, theme::surface(), LV_STATE_PRESSED);
        lv_obj_set_style_bg_opa(l, LV_OPA_COVER, LV_STATE_PRESSED);
        text(l, link, theme::meta(), theme::text2());
        on_click(l, std::move(on_link));
    }
    return h;
}

lv_obj_t* pill(lv_obj_t* parent, const std::string& label, bool selected, Action action) {
    lv_obj_t* p = row(parent);
    lv_obj_set_height(p, 40);
    lv_obj_set_ext_click_area(p, 8);  // 56 px tap target
    lv_obj_set_style_pad_hor(p, 18, 0);
    lv_obj_set_style_radius(p, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_opa(p, LV_OPA_COVER, 0);
    pressable(p, selected ? theme::accent() : theme::surface(), selected);
    text(p, label, theme::meta(), selected ? theme::on_accent() : theme::text());
    on_click(p, std::move(action));
    return p;
}

lv_obj_t* back_chip(lv_obj_t* parent, const std::string& label, Action action) {
    lv_obj_t* c = row(parent, 4);
    lv_obj_set_height(c, 48);
    lv_obj_set_ext_click_area(c, 4);
    lv_obj_set_style_pad_right(c, 14, 0);
    lv_obj_set_style_radius(c, theme::r_control(), 0);
    pressable(c, theme::surface());
    lv_obj_set_style_bg_opa(c, LV_OPA_TRANSP, 0);
    icon(c, icon::chevron_left, 28, theme::text2());
    text(c, label, theme::meta(), theme::text2());
    on_click(c, std::move(action));
    return c;
}

lv_obj_t* art(lv_obj_t* parent, const core::MediaItem& item, int32_t size, int32_t radius, lv_color_t bg,
              std::function<void(uint32_t)> on_ready) {
    lv_obj_t* a = lv_obj_create(parent);
    remove_style(a);
    lv_obj_set_size(a, size, size);
    int32_t r = item.type == core::MediaType::Artist ? size / 2 : radius;
    lv_obj_set_style_radius(a, r, 0);
    lv_obj_set_style_bg_opa(a, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(a, theme::art_colour(item.uri.empty() ? item.name.c_str() : item.uri.c_str()), 0);
    if (item.type == core::MediaType::Radio) lv_obj_center(icon(a, icon::radio, size >= 120 ? 48 : 22, lv_color_hex(0xFFFFFF)));
    art::attach(a, item, size, size, r, bg, std::move(on_ready));
    return a;
}

lv_obj_t* badge(lv_obj_t* parent, const std::string& provider) {
    lv_obj_t* b = lv_obj_create(parent);
    remove_style(b);
    lv_obj_set_size(b, 20, 20);
    lv_obj_set_style_radius(b, 3, 0);
    lv_obj_set_style_border_width(b, 1, 0);
    lv_obj_set_style_border_color(b, theme::border(), 0);
    char s[2] = {provider_letter(provider), 0};
    lv_obj_t* l = text(b, s, theme::label(), theme::text3());
    lv_obj_center(l);
    return b;
}

lv_obj_t* tag(lv_obj_t* parent, const std::string& s, lv_color_t colour) {
    lv_obj_t* t = row(parent);
    lv_obj_set_height(t, 26);
    lv_obj_set_style_pad_hor(t, 8, 0);
    lv_obj_set_style_radius(t, std::min<int32_t>(theme::r_control(), 6), 0);
    lv_obj_set_style_border_width(t, 1, 0);
    lv_obj_set_style_border_color(t, theme::border(), 0);
    text(t, s, theme::label(), colour);
    return t;
}

lv_obj_t* equalizer(lv_obj_t* parent, lv_color_t colour, bool playing) {
    lv_obj_t* eq = row(parent, 2);
    lv_obj_set_size(eq, 13, 16);
    lv_obj_set_flex_align(eq, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_END);
    static const uint32_t periods[] = {900, 700, 1100};
    for (int i = 0; i < 3; ++i) {
        lv_obj_t* bar = lv_obj_create(eq);
        remove_style(bar);
        lv_obj_set_size(bar, 3, 6 + 4 * i);
        lv_obj_set_style_radius(bar, 1, 0);
        lv_obj_set_style_bg_color(bar, colour, 0);
        lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, 0);
        if (!playing || !theme::motion()) continue;
        // Micro-animation: tiny area, so cheap; deleted with the bar.
        lv_anim_t a;
        lv_anim_init(&a);
        lv_anim_set_var(&a, bar);
        lv_anim_set_values(&a, 5, 16);
        lv_anim_set_duration(&a, periods[i] / 2);
        lv_anim_set_playback_duration(&a, periods[i] / 2);
        lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
        lv_anim_set_delay(&a, i * 120);
        lv_anim_set_path_cb(&a, lv_anim_path_ease_in_out);
        lv_anim_set_exec_cb(&a, [](void* o, int32_t v) { lv_obj_set_height(static_cast<lv_obj_t*>(o), v); });
        lv_anim_start(&a);
    }
    return eq;
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
    lv_obj_t* r = row(parent, 14);
    lv_obj_set_size(r, lv_pct(100), opt.height);
    lv_obj_set_style_pad_hor(r, 8, 0);
    lv_obj_set_style_radius(r, theme::r_control(), 0);
    pressable(r, opt.playing ? theme::elevated() : theme::surface());
    lv_obj_set_style_bg_opa(r, opt.playing ? LV_OPA_COVER : LV_OPA_TRANSP, 0);
    lv_color_t row_bg = opt.playing ? theme::elevated() : theme::bg();
    art(r, item, std::min<int32_t>(opt.height - 16, 48), theme::r_art(), row_bg);
    lv_obj_t* col = column(r, 2);
    lv_obj_set_flex_grow(col, 1);
    lv_obj_set_width(col, 1);  // let flex-grow decide; enables "…" clipping
    text(col, item.name + (item.version.empty() ? "" : " (" + item.version + ")"), theme::meta(), theme::text());
    std::string sub = subtitle(item);
    if (!sub.empty()) text(col, sub, theme::label(), theme::text2());
    if (opt.playing) equalizer(r, theme::accent(), true);
    if (!item.alternatives.empty())
        tag(r, i18n::tr(item.alternatives.size() == 1 ? i18n::Str::tag_source : i18n::Str::tag_sources,
                        std::to_string(item.alternatives.size())));
    badge(r, item.badge_provider());
    if (opt.show_duration && item.duration_s) {
        lv_obj_t* d = text(r, duration(item.duration_s), theme::label(), theme::text3());
        lv_obj_set_width(d, 48);
        lv_obj_set_style_text_align(d, LV_TEXT_ALIGN_RIGHT, 0);
    }
    if (opt.on_more) icon_button(r, icon::more, opt.on_more, 48, 22, false, theme::text2());  // 56 px tap area
    if (opt.on_remove) icon_button(r, icon::close, opt.on_remove, 48, 22, false, theme::text2());
    on_click(r, opt.on_tap);
    return r;
}

lv_obj_t* media_card(lv_obj_t* parent, const core::MediaItem& item, const std::string& sub, Action on_tap, int32_t size) {
    bool round = item.type == core::MediaType::Artist || item.type == core::MediaType::Radio;
    // Round items (artists, stations) get a label wider than the circle so names fit.
    int32_t label_w = round ? size + 32 : size;
    lv_obj_t* c = column(parent, 8);
    lv_obj_set_width(c, label_w);
    lv_obj_set_flex_align(c, LV_FLEX_ALIGN_START, round ? LV_FLEX_ALIGN_CENTER : LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER);
    art(c, item, size, round ? size / 2 : theme::r_art());
    lv_obj_t* t = text(c, item.name, round ? theme::label() : theme::meta(), theme::text());
    lv_obj_set_width(t, label_w);
    if (round) lv_obj_set_style_text_align(t, LV_TEXT_ALIGN_CENTER, 0);
    if (!sub.empty() && item.type != core::MediaType::Artist && item.type != core::MediaType::Radio) {
        lv_obj_t* s = text(c, sub, theme::label(), theme::text2());
        lv_obj_set_width(s, size);
    }
    shrink_on_press(c);
    on_click(c, std::move(on_tap));
    return c;
}

lv_obj_t* favourite_tile(lv_obj_t* parent, const core::MediaItem& item, Action on_tap, Action long_press) {
    // Shortcut tile (Spotify-style): cover flush left, bold name, one tap plays.
    lv_obj_t* t = row(parent, 14);
    lv_obj_set_height(t, 64);
    lv_obj_set_style_radius(t, theme::r_control(), 0);
    lv_obj_set_style_clip_corner(t, true, 0);  // small radius on a small tile: cheap
    lv_obj_set_style_bg_opa(t, LV_OPA_COVER, 0);
    pressable(t, theme::surface());
    lv_obj_set_style_pad_right(t, 12, 0);
    art(t, item, 64, 0, theme::surface());
    lv_obj_t* name = text(t, item.name, theme::meta(), theme::text());
    lv_obj_set_flex_grow(name, 1);
    lv_obj_set_width(name, 1);
    shrink_on_press(t);
    on_click(t, std::move(on_tap));
    on_long_press(t, std::move(long_press));
    return t;
}

lv_obj_t* slider(lv_obj_t* parent, int value, std::function<void(int)> on_release, lv_color_t fill) {
    lv_obj_add_flag(parent, LV_OBJ_FLAG_OVERFLOW_VISIBLE);  // the knob sticks out of the thin track
    lv_obj_t* s = lv_slider_create(parent);
    lv_slider_set_range(s, 0, 100);
    lv_slider_set_value(s, value, LV_ANIM_OFF);
    lv_obj_set_height(s, 4);
    lv_obj_set_ext_click_area(s, 26);  // 4 + 2*26 = 56 px hit area
    lv_obj_set_style_bg_color(s, theme::elevated(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(s, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_bg_color(s, fill, LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(s, theme::text(), LV_PART_KNOB);
    lv_obj_set_style_pad_all(s, 8, LV_PART_KNOB);  // 4 + 2*8 = 20 px knob
    lv_obj_set_style_pad_all(s, 11, LV_PART_KNOB | LV_STATE_PRESSED);
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
    lv_obj_set_size(sw, 60, 34);
    lv_obj_set_ext_click_area(sw, 11);
    lv_obj_set_style_bg_color(sw, theme::elevated(), LV_PART_MAIN);
    lv_obj_set_style_border_width(sw, 1, LV_PART_MAIN);
    lv_obj_set_style_border_color(sw, theme::border(), LV_PART_MAIN);
    lv_obj_set_style_bg_color(sw, theme::accent(), LV_PART_INDICATOR | LV_STATE_CHECKED);
    lv_obj_set_style_pad_all(sw, -3, LV_PART_KNOB);
    lv_obj_set_style_bg_color(sw, theme::text2(), LV_PART_KNOB);  // off: grey knob on a dark track
    lv_obj_set_style_bg_color(sw, theme::on_accent(), LV_PART_KNOB | LV_STATE_CHECKED);  // on: contrasts with the accent
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

// The usual tablet layout (iPad, Gboard): ⌫ at the end of the top row, the action key at the end
// of the home row, shift on both sides of the bottom letter row, ?123 around the space bar.
// Shift is one-shot: it capitalises the next letter, like a phone keyboard.
enum class Layer : uint8_t { Lower, Upper, Symbols };

struct Keys {
    std::string done, space;
    lv_obj_t* input;
    Action on_done;
    Layer layer = Layer::Lower;
    std::vector<const char*> lower, upper, symbols;
};

// Relative key widths within a row (LVGL 9 keeps the width in the low 4 bits of the ctrl enum, so
// 1–15: a larger value spills into the flag bits and hides the key). The top three rows add up to
// the same total, so letter keys line up; the bottom row is ?123 · space · ?123.
constexpr lv_buttonmatrix_ctrl_t w(int n) { return static_cast<lv_buttonmatrix_ctrl_t>(n); }
const lv_buttonmatrix_ctrl_t kLetterCtrl[] = {w(4), w(4), w(4), w(4), w(4), w(4), w(4), w(4), w(4), w(4), w(6),
                                              w(4), w(4), w(4), w(4), w(4), w(4), w(4), w(4), w(4), w(10),
                                              w(5), w(4), w(4), w(4), w(4), w(4), w(4), w(4), w(4), w(4), w(5),
                                              w(6), w(15), w(6)};
const lv_buttonmatrix_ctrl_t kSymbolCtrl[] = {w(4), w(4), w(4), w(4), w(4), w(4), w(4), w(4), w(4), w(4), w(6),
                                              w(4), w(4), w(4), w(4), w(4), w(4), w(4), w(4), w(4), w(10),
                                              w(4), w(4), w(4), w(4), w(4), w(4), w(4), w(4), w(4), w(4),
                                              w(6), w(15), w(6)};

// Inter has no ⌫/⇧: the key font falls back to the icon font for those glyphs.
const lv_font_t* key_font() {
    static lv_font_t font = [] {
        lv_font_t f = *theme::body();
        f.fallback = theme::icons(22);
        return f;
    }();
    return &font;
}

void show_layer(lv_obj_t* kb, Keys* k, Layer layer) {
    k->layer = layer;
    auto& map = layer == Layer::Lower ? k->lower : layer == Layer::Upper ? k->upper : k->symbols;
    lv_buttonmatrix_set_map(kb, map.data());
    lv_buttonmatrix_set_ctrl_map(kb, layer == Layer::Symbols ? kSymbolCtrl : kLetterCtrl);
}

}  // namespace

lv_obj_t* keyboard(lv_obj_t* parent, lv_obj_t* input, const std::string& done, const std::string& space, Action on_done) {
    auto* k = new Keys{done, space, input, std::move(on_done), Layer::Lower, {}, {}, {}};
    const char* g = k->done.c_str();
    const char* sp = k->space.c_str();
    const char* bs = icon::backspace;
    k->lower = {"q", "w", "e", "r", "t", "y", "u", "i", "o", "p", bs, "\n",
                "a", "s", "d", "f", "g", "h", "j", "k", "l", g, "\n",
                icon::shift, "z", "x", "c", "v", "b", "n", "m", ",", ".", icon::shift, "\n",
                "?123", sp, "?123", ""};
    k->upper = {"Q", "W", "E", "R", "T", "Y", "U", "I", "O", "P", bs, "\n",
                "A", "S", "D", "F", "G", "H", "J", "K", "L", g, "\n",
                icon::shift_on, "Z", "X", "C", "V", "B", "N", "M", ",", ".", icon::shift_on, "\n",
                "?123", sp, "?123", ""};
    k->symbols = {"1", "2", "3", "4", "5", "6", "7", "8", "9", "0", bs, "\n",
                  "@", "#", "&", "(", ")", "!", "?", ":", "/", g, "\n",
                  "-", "_", "*", "$", "%", "=", "+", ";", "\"", "'", "\n",
                  "ABC", sp, "ABC", ""};

    // A plain button matrix, not lv_keyboard: lv_keyboard keeps custom maps in one global table
    // shared by every keyboard, so deleting one leaves the others pointing at freed maps.
    lv_obj_t* kb = lv_buttonmatrix_create(parent);
    show_layer(kb, k, Layer::Lower);
    lv_obj_set_size(kb, theme::kScreenW, 316);
    lv_obj_set_style_bg_color(kb, theme::side(), 0);
    lv_obj_set_style_bg_opa(kb, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(kb, 0, 0);
    lv_obj_set_style_radius(kb, 0, 0);
    lv_obj_set_style_pad_all(kb, 16, 0);
    lv_obj_set_style_pad_gap(kb, 10, 0);
    lv_obj_set_style_bg_color(kb, theme::elevated(), LV_PART_ITEMS);
    lv_obj_set_style_bg_color(kb, theme::light() ? lv_color_mix(lv_color_black(), theme::elevated(), 26)
                                                 : lv_color_mix(lv_color_white(), theme::elevated(), 31), LV_PART_ITEMS | LV_STATE_PRESSED);
    lv_obj_set_style_text_color(kb, theme::text(), LV_PART_ITEMS);
    lv_obj_set_style_text_font(kb, key_font(), LV_PART_ITEMS);
    lv_obj_set_style_radius(kb, theme::r_control(), LV_PART_ITEMS);
    lv_obj_set_style_border_width(kb, 0, LV_PART_ITEMS);
    lv_obj_set_style_shadow_width(kb, 0, LV_PART_ITEMS);
    lv_obj_add_event_cb(kb, [](lv_event_t* e) {
        auto* k = static_cast<Keys*>(lv_event_get_user_data(e));
        auto* kb = static_cast<lv_obj_t*>(lv_event_get_target(e));
        uint32_t id = lv_buttonmatrix_get_selected_button(kb);
        if (id == LV_BUTTONMATRIX_BUTTON_NONE) return;
        std::string key = lv_buttonmatrix_get_button_text(kb, id);
        if (key == icon::backspace) lv_textarea_delete_char(k->input);
        else if (key == k->done) { if (k->on_done) k->on_done(); }
        else if (key == k->space) lv_textarea_add_char(k->input, ' ');
        else if (key == icon::shift) show_layer(kb, k, Layer::Upper);
        else if (key == icon::shift_on) show_layer(kb, k, Layer::Lower);
        else if (key == "?123") show_layer(kb, k, Layer::Symbols);
        else if (key == "ABC") show_layer(kb, k, Layer::Lower);
        else {
            lv_textarea_add_text(k->input, key.c_str());
            if (k->layer == Layer::Upper) show_layer(kb, k, Layer::Lower);  // one capital, then back
        }
    }, LV_EVENT_VALUE_CHANGED, k);
    lv_obj_add_event_cb(kb, [](lv_event_t* e) {
        // LVGL still restyles a key that was being pressed after this DELETE event (it clears the
        // pressed state), which re-reads the map: point it at a static one before freeing ours.
        static const char* const kEmpty[] = {""};
        lv_buttonmatrix_set_map(static_cast<lv_obj_t*>(lv_event_get_target(e)), kEmpty);
        delete static_cast<Keys*>(lv_event_get_user_data(e));
    }, LV_EVENT_DELETE, k);
    return kb;
}

lv_obj_t* sheet(int32_t width, Action on_close) {
    lv_obj_t* scrim = lv_obj_create(lv_layer_top());
    remove_style(scrim);
    lv_obj_set_size(scrim, theme::kScreenW, theme::kScreenH);
    lv_obj_set_style_bg_color(scrim, lv_color_black(), 0);
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
    lv_obj_set_style_radius(panel, theme::r_sheet(), 0);
    lv_obj_set_style_bg_color(panel, theme::side(), 0);
    lv_obj_set_style_bg_opa(panel, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(panel, 1, 0);
    lv_obj_set_style_border_color(panel, theme::hairline(), 0);
    lv_obj_add_flag(panel, LV_OBJ_FLAG_CLICKABLE);  // swallow taps so they don't reach the scrim
    lv_obj_center(panel);
    // Motion: panel rises 16 px over 150 ms (docs/design.md, Motion). Only translate: no opacity on containers.
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
    lv_obj_set_style_radius(t, theme::r_card(), 0);
    lv_obj_set_style_bg_color(t, theme::elevated(), 0);
    lv_obj_set_style_bg_opa(t, LV_OPA_COVER, 0);
    lv_obj_set_style_border_side(t, LV_BORDER_SIDE_LEFT, 0);
    lv_obj_set_style_border_width(t, 3, 0);
    lv_obj_set_style_border_color(t, error ? theme::warning() : theme::accent(), 0);
    icon(t, error ? icon::warning : icon::check, 22, error ? theme::warning() : theme::accent());
    text(t, message, theme::body(), theme::text());
    if (!action_label.empty() && action) {
        lv_obj_t* b = button(t, Button::Ghost, nullptr, action_label, [action] {
            action();
            if (current) lv_obj_delete_async(current);
        });
        lv_obj_set_height(b, 56);
    }
    lv_obj_align(t, LV_ALIGN_BOTTOM_MID, theme::kSideW / 2, -(theme::kBarH + 20));
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
