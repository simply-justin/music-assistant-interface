#include "shell.hpp"

#include <algorithm>
#include <vector>

#include <ctime>

#include "artwork.hpp"
#include "text.hpp"

namespace ui {

namespace {

struct Shell {
    Send send;
    State state;
    std::vector<std::unique_ptr<Screen>> screens;  // indexed by ScreenId
    std::vector<lv_obj_t*> roots;
    std::vector<ScreenId> stack;                   // back stack, max 8
    ScreenId current = ScreenId::Home;
    lv_obj_t* base = nullptr;
    lv_obj_t* side = nullptr;      // sidebar: speaker, menu, playlists, settings
    lv_obj_t* bar = nullptr;       // player bar along the bottom
    lv_obj_t* progress = nullptr;  // player bar progress (animated between 1 Hz ticks)
    lv_obj_t* elapsed = nullptr;
    core::MediaItem detail;
    SettingsPage page = SettingsPage::Appearance;
    lv_obj_t* ambient = nullptr;
    lv_obj_t* ambient_clock = nullptr;
    bool touches_hooked = false;
};

Shell& shell() {
    static Shell s;
    return s;
}

Screen& screen(ScreenId id) { return *shell().screens[static_cast<size_t>(id)]; }

void build_sidebar();
void refresh_bar();

void layout_for(Screen& scr) {
    auto& s = shell();
    if (scr.full_screen()) lv_obj_add_flag(s.side, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_remove_flag(s.side, LV_OBJ_FLAG_HIDDEN);
    if (scr.player_bar()) lv_obj_remove_flag(s.bar, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_add_flag(s.bar, LV_OBJ_FLAG_HIDDEN);
}

void show(ScreenId id) {
    auto& s = shell();
    s.current = id;
    Screen& scr = screen(id);
    for (size_t i = 0; i < s.roots.size(); ++i) {
        if (i == static_cast<size_t>(id)) lv_obj_remove_flag(s.roots[i], LV_OBJ_FLAG_HIDDEN);
        else lv_obj_add_flag(s.roots[i], LV_OBJ_FLAG_HIDDEN);
    }
    layout_for(scr);
    kit::close_sheets();
    scr.on_open();
    scr.refresh(Topic::All);
    build_sidebar();  // selected item follows the screen
    refresh_bar();
}

void build_all() {
    auto& s = shell();
    for (auto* r : s.roots) lv_obj_delete(r);
    s.roots.clear();
    if (s.side) lv_obj_delete(s.side);
    if (s.bar) lv_obj_delete(s.bar);
    lv_obj_set_style_bg_color(s.base, theme::bg(), 0);

    for (auto& scr : s.screens) {
        lv_obj_t* root = kit::column(s.base);
        bool full = scr->full_screen();
        lv_obj_set_pos(root, full ? 0 : theme::kSideW, 0);
        lv_obj_set_size(root, full ? theme::kScreenW : theme::kScreenW - theme::kSideW,
                        theme::kScreenH - (scr->player_bar() ? theme::kBarH : 0));
        lv_obj_add_flag(root, LV_OBJ_FLAG_HIDDEN);
        scr->build(root);
        s.roots.push_back(root);
    }
    s.side = kit::column(s.base, 0);
    lv_obj_set_size(s.side, theme::kSideW, theme::kScreenH - theme::kBarH);
    lv_obj_set_style_pad_hor(s.side, 12, 0);
    lv_obj_set_style_pad_top(s.side, 20, 0);
    lv_obj_set_style_pad_bottom(s.side, 12, 0);
    lv_obj_set_style_bg_color(s.side, theme::side(), 0);
    lv_obj_set_style_bg_opa(s.side, LV_OPA_COVER, 0);
    lv_obj_set_style_border_side(s.side, LV_BORDER_SIDE_RIGHT, 0);
    lv_obj_set_style_border_width(s.side, 1, 0);
    lv_obj_set_style_border_color(s.side, theme::hairline(), 0);

    s.bar = kit::row(s.base, 24);
    lv_obj_set_pos(s.bar, 0, theme::kScreenH - theme::kBarH);
    lv_obj_set_size(s.bar, theme::kScreenW, theme::kBarH);
    lv_obj_set_style_pad_hor(s.bar, 24, 0);
    lv_obj_set_style_bg_color(s.bar, theme::side(), 0);
    lv_obj_set_style_bg_opa(s.bar, LV_OPA_COVER, 0);
    lv_obj_set_style_border_side(s.bar, LV_BORDER_SIDE_TOP, 0);
    lv_obj_set_style_border_width(s.bar, 1, 0);
    lv_obj_set_style_border_color(s.bar, theme::hairline(), 0);
}

// A sidebar menu item: 40 px visible (selected = filled block), 52 px tap area.
lv_obj_t* side_item(lv_obj_t* parent, const char* glyph, const std::string& label, bool selected, kit::Action action) {
    lv_obj_t* r = kit::row(parent, 12);
    lv_obj_set_size(r, lv_pct(100), theme::kNavH);
    lv_obj_set_ext_click_area(r, theme::kNavHit);
    lv_obj_set_style_margin_bottom(r, 4, 0);
    lv_obj_set_style_pad_hor(r, 12, 0);
    lv_obj_set_style_radius(r, theme::r_control(), 0);
    lv_obj_set_style_bg_color(r, theme::elevated(), 0);
    lv_obj_set_style_bg_opa(r, selected ? LV_OPA_COVER : LV_OPA_TRANSP, 0);
    lv_obj_set_style_bg_color(r, theme::surface(), LV_STATE_PRESSED);
    lv_obj_set_style_bg_opa(r, LV_OPA_COVER, LV_STATE_PRESSED);
    lv_color_t fg = selected ? theme::text() : theme::text3();
    kit::icon(r, glyph, 22, fg);
    kit::text(r, label, theme::meta(), fg);
    kit::on_click(r, std::move(action));
    return r;
}

void build_sidebar() {
    auto& s = shell();
    lv_obj_clean(s.side);
    ScreenId tab = screen(s.current).tab();

    struct Item { ScreenId id; const char* glyph; i18n::Str label; };
    for (const Item& it : {Item{ScreenId::Home, icon::home, i18n::Str::nav_home}, Item{ScreenId::Search, icon::search, i18n::Str::nav_search},
                           Item{ScreenId::Library, icon::library, i18n::Str::nav_library}, Item{ScreenId::Queue, icon::queue, i18n::Str::nav_queue}}) {
        ScreenId id = it.id;
        side_item(s.side, it.glyph, tr(it.label), tab == id, [id] { open(id); });
    }

    // Your playlists (as many as fit).
    if (s.state.playlists && !s.state.playlists->playlists.empty()) {
        lv_obj_t* ov = kit::overline(s.side, tr(i18n::Str::side_playlists), theme::text3());
        lv_obj_set_style_margin_top(ov, 18, 0);
        lv_obj_set_style_margin_bottom(ov, 4, 0);
        lv_obj_set_style_pad_left(ov, 12, 0);
        size_t n = 0;
        for (const auto& pl : s.state.playlists->playlists) {
            if (++n > 6) break;
            lv_obj_t* r = kit::row(s.side, 12);
            lv_obj_set_size(r, lv_pct(100), 44);
            lv_obj_set_ext_click_area(r, 6);  // 56 px tap target
            lv_obj_set_style_pad_hor(r, 6, 0);
            lv_obj_set_style_radius(r, theme::r_control(), 0);
            lv_obj_set_style_bg_color(r, theme::surface(), LV_STATE_PRESSED);
            lv_obj_set_style_bg_opa(r, LV_OPA_COVER, LV_STATE_PRESSED);
            kit::art(r, pl, 32, std::min<int32_t>(theme::r_art(), 6), theme::side());
            lv_obj_t* name = kit::text(r, pl.name, theme::label(), theme::text2());
            lv_obj_set_flex_grow(name, 1);
            lv_obj_set_width(name, 1);
            auto item = pl;
            kit::on_click(r, [item] { open_detail(item); });
        }
    }
    kit::spacer(s.side);
    side_item(s.side, icon::settings, tr(i18n::Str::nav_settings), tab == ScreenId::Settings, [] { open(ScreenId::Settings); });
}

// Player bar (96 px, full width): what's playing | transport + progress | queue, volume.
void refresh_bar() {
    auto& s = shell();
    if (!s.bar || lv_obj_has_flag(s.bar, LV_OBJ_FLAG_HIDDEN)) return;
    lv_obj_clean(s.bar);
    s.progress = s.elapsed = nullptr;
    auto item = s.state.now_playing();
    const core::PlayerState* p = s.state.selected();

    lv_obj_t* info = kit::row(s.bar, 14);
    lv_obj_set_width(info, 320);
    if (item) {
        kit::art(info, *item, 64, theme::r_art(), theme::side());
        lv_obj_t* col = kit::column(info, 2);
        lv_obj_set_flex_grow(col, 1);
        lv_obj_set_width(col, 1);
        kit::text(col, item->name, theme::meta(), theme::text());
        lv_obj_t* sub = kit::row(col, 8);
        kit::text(sub, kit::subtitle(*item), theme::label(), theme::text2());
        kit::badge(sub, item->badge_provider());
        kit::on_click(info, [] { open(ScreenId::NowPlaying); });
    } else {  // same shape as when playing, so nothing jumps when music starts
        lv_obj_t* ph = kit::row(info, 0);
        lv_obj_set_size(ph, 64, 64);
        lv_obj_set_style_radius(ph, theme::r_art(), 0);
        lv_obj_set_style_bg_color(ph, theme::surface(), 0);
        lv_obj_set_style_bg_opa(ph, LV_OPA_COVER, 0);
        lv_obj_set_flex_align(ph, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        kit::icon(ph, icon::album, 28, theme::text3());
        lv_obj_t* col = kit::column(info, 2);
        lv_obj_set_flex_grow(col, 1);
        lv_obj_set_width(col, 1);
        kit::text(col, tr(i18n::Str::home_nothing_playing), theme::meta(), theme::text());
        kit::text(col, tr(i18n::Str::bar_idle_sub), theme::label(), theme::text3());
    }

    lv_obj_t* mid = kit::column(s.bar, 4);
    lv_obj_set_flex_grow(mid, 1);
    lv_obj_set_flex_align(mid, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_t* controls = kit::row(mid, 16);
    bool radio = item && item->type == core::MediaType::Radio;
    const auto* q = s.state.queue.get();
    if (!radio) {
        bool on = q && q->shuffle;
        kit::icon_button(controls, icon::shuffle, [on] { send(app::cmd::SetShuffle{!on}); }, 48, 22, false,
                         on ? theme::accent() : theme::text3());
    }
    if (!radio && (!p || p->can_next_previous)) kit::icon_button(controls, icon::prev, [] { send(app::cmd::Previous{}); }, 48, 28);
    kit::play_button(controls, s.state.playing(), [] { send(app::cmd::PlayPause{}); }, 52);
    if (!radio && (!p || p->can_next_previous)) kit::icon_button(controls, icon::next, [] { send(app::cmd::Next{}); }, 48, 28);
    if (!radio) {
        core::RepeatMode mode = q ? q->repeat : core::RepeatMode::Off;
        core::RepeatMode next = mode == core::RepeatMode::Off ? core::RepeatMode::All : mode == core::RepeatMode::All ? core::RepeatMode::One : core::RepeatMode::Off;
        kit::icon_button(controls, mode == core::RepeatMode::One ? icon::repeat_one : icon::repeat,
                         [next] { send(app::cmd::SetRepeat{next}); }, 48, 22, false,
                         mode == core::RepeatMode::Off ? theme::text3() : theme::accent());
    }
    if (!item) {  // nothing playing: controls shown but dimmed and inert (WCAG: disabled, not hidden)
        lv_obj_set_style_opa(controls, LV_OPA_40, 0);
        for (uint32_t i = 0; i < lv_obj_get_child_count(controls); ++i) {
            lv_obj_t* b = lv_obj_get_child(controls, i);
            lv_obj_add_state(b, LV_STATE_DISABLED);
            lv_obj_remove_flag(b, LV_OBJ_FLAG_CLICKABLE);
        }
        lv_obj_t* prog = kit::row(mid, 10);
        lv_obj_set_width(prog, 440);
        lv_obj_set_style_opa(prog, LV_OPA_40, 0);
        kit::text(prog, "0:00", theme::label(), theme::text3());
        lv_obj_t* track = kit::row(prog, 0);
        lv_obj_set_flex_grow(track, 1);
        lv_obj_set_height(track, 4);
        lv_obj_set_style_radius(track, 2, 0);
        lv_obj_set_style_bg_color(track, theme::elevated(), 0);
        lv_obj_set_style_bg_opa(track, LV_OPA_COVER, 0);
        kit::text(prog, "0:00", theme::label(), theme::text3());
    }
    if (item && !radio && item->duration_s) {
        lv_obj_t* prog = kit::row(mid, 10);
        lv_obj_set_width(prog, 440);
        s.elapsed = kit::text(prog, duration(s.state.elapsed_s), theme::label(), theme::text3());
        s.progress = lv_bar_create(prog);
        lv_obj_set_flex_grow(s.progress, 1);
        lv_obj_set_height(s.progress, 4);
        lv_obj_set_style_bg_color(s.progress, theme::elevated(), LV_PART_MAIN);
        lv_obj_set_style_bg_color(s.progress, theme::text(), LV_PART_INDICATOR);
        lv_obj_set_style_anim_duration(s.progress, 1000, 0);  // glides between the 1 Hz ticks
        lv_bar_set_range(s.progress, 0, static_cast<int32_t>(item->duration_s));
        lv_bar_set_value(s.progress, static_cast<int32_t>(s.state.elapsed_s), LV_ANIM_OFF);
        kit::text(prog, duration(item->duration_s), theme::label(), theme::text3());
    }

    lv_obj_t* right = kit::row(s.bar, 8);
    lv_obj_set_width(right, 400);
    lv_obj_set_style_pad_right(right, 12, 0);  // room for the volume knob at 100 %
    lv_obj_set_flex_align(right, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    kit::icon_button(right, icon::queue, [] { open(ScreenId::Queue); }, 48, 22, false, theme::text2());
    // Speaker (Spotify-style "playing on"): icon + name; tap = pick or group speakers.
    lv_obj_t* spk = kit::row(right, 8);
    lv_obj_set_height(spk, 40);
    lv_obj_set_ext_click_area(spk, 8);
    lv_obj_set_style_pad_hor(spk, 12, 0);
    lv_obj_set_style_radius(spk, theme::r_control(), 0);
    lv_obj_set_style_border_width(spk, 1, 0);
    lv_obj_set_style_border_color(spk, theme::border(), 0);
    lv_obj_set_style_bg_color(spk, theme::surface(), LV_STATE_PRESSED);
    lv_obj_set_style_bg_opa(spk, LV_OPA_COVER, LV_STATE_PRESSED);
    lv_color_t spk_fg = s.state.playing() ? theme::accent() : theme::text2();
    if (s.state.playing()) kit::equalizer(spk, spk_fg, true);
    else kit::icon(spk, icon::speaker, 20, spk_fg);
    lv_obj_t* spk_name = kit::text(spk, p ? p->name : tr(i18n::Str::speaker_none), theme::label(), spk_fg);
    lv_obj_set_style_max_width(spk_name, 120, 0);
    kit::on_click(spk, [] { open_speakers(); });
    kit::icon_button(right, icon::volume, [] { open_volume(); }, 48, 22, false, theme::text2());
    if (p && p->can_set_volume) {
        std::string id = p->player_id;
        lv_obj_t* vol = kit::slider(right, p->volume_level, [id](int v) { send(app::cmd::SetVolume{id, v}); }, theme::text2());
        lv_obj_set_width(vol, 112);
    }
}

bool in_setup() {
    auto& s = shell();
    return s.current == ScreenId::Setup || s.current == ScreenId::Boot;
}

void tick_ambient() {
    auto& s = shell();
    if (s.ambient) {
        std::time_t t = std::time(nullptr);
        std::tm* lt = std::localtime(&t);
        if (s.ambient_clock) lv_label_set_text(s.ambient_clock, clock_text(lt->tm_hour * 60 + lt->tm_min).c_str());
        return;
    }
    const app::PanelView* p = s.state.panel.get();
    if (!p || p->display.dim_after_s <= 0 || in_setup()) return;
    if (lv_display_get_inactive_time(nullptr) >= static_cast<uint32_t>(p->display.dim_after_s) * 1000) show_ambient();
}

// Diagnostics: a dot where each touch lands (Settings › About & diagnostics).
void on_touch(lv_event_t* e) {
    const app::PanelView* p = shell().state.panel.get();
    if (!p || !p->diagnostics.touches) return;
    lv_point_t pt;
    lv_indev_get_point(static_cast<lv_indev_t*>(lv_event_get_target(e)), &pt);
    lv_obj_t* dot = lv_obj_create(lv_layer_sys());
    lv_obj_remove_style_all(dot);
    lv_obj_set_size(dot, 24, 24);
    lv_obj_set_pos(dot, pt.x - 12, pt.y - 12);
    lv_obj_set_style_radius(dot, 12, 0);
    lv_obj_set_style_bg_color(dot, theme::warning(), 0);
    lv_obj_set_style_bg_opa(dot, LV_OPA_COVER, 0);
    lv_obj_remove_flag(dot, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_delete_delayed(dot, 400);
}

void apply_diagnostics() {
    auto& s = shell();
    const app::PanelView* p = s.state.panel.get();
    if (!p) return;
#if LV_USE_PERF_MONITOR
    if (p->diagnostics.fps) lv_sysmon_show_performance(nullptr);
    else lv_sysmon_hide_performance(nullptr);
#endif
    if (p->diagnostics.touches && !s.touches_hooked) {
        for (lv_indev_t* i = lv_indev_get_next(nullptr); i; i = lv_indev_get_next(i))
            if (lv_indev_get_type(i) == LV_INDEV_TYPE_POINTER) lv_indev_add_event_cb(i, on_touch, LV_EVENT_PRESSED, nullptr);
        s.touches_hooked = true;
    }
}

// What the screens draw from players / the queue; equal signatures = nothing visible changed.
std::string signature(const app::PlayersView& v) {
    std::string k = v.selected_id;
    for (const auto& p : v.players) {
        k += '|' + p.player_id + ',' + p.name + ',' + std::to_string(static_cast<int>(p.state)) + ',' +
             std::to_string(p.volume_level) + ',' + std::to_string(p.group_volume) + ',' + (p.available ? '1' : '0') +
             (p.can_seek ? '1' : '0') + (p.can_next_previous ? '1' : '0') + (p.can_set_volume ? '1' : '0') +
             (p.can_pause ? '1' : '0') + ',' + p.synced_to + ',' + p.media_title + ',' + p.media_artist + ',' + p.media_image_url;
        for (const auto& m : p.group_members) k += ';' + m;
        k += '/';
        for (const auto& m : p.can_group_with) k += ';' + m;
    }
    return k;
}

std::string signature(const core::QueueState& q) {
    auto item = [](const core::QueueItem& i) {
        return i.queue_item_id + ',' + i.name + ',' + (i.media ? i.media->uri + (i.media->favorite ? "*" : "") : "");
    };
    std::string k = q.queue_id + '|' + std::to_string(q.current_index) + (q.shuffle ? "s" : "") +
                    std::to_string(static_cast<int>(q.repeat)) + '|' + (q.current ? item(*q.current) : "");
    for (const auto& i : q.items) k += '|' + item(i);
    return k;
}

void tick_progress(lv_timer_t*) {
    auto& s = shell();
    tick_ambient();
    if (!s.state.playing()) return;
    ++s.state.elapsed_s;
    screen(s.current).refresh(Topic::Progress);
    if (s.progress) lv_bar_set_value(s.progress, static_cast<int32_t>(s.state.elapsed_s), LV_ANIM_ON);
    if (s.elapsed) lv_label_set_text(s.elapsed, duration(s.state.elapsed_s).c_str());
}

}  // namespace

// ---- State

const core::PlayerState* State::selected() const {
    if (!players) return nullptr;
    for (const auto& p : players->players)
        if (p.player_id == players->selected_id) return &p;
    return nullptr;
}

std::optional<core::MediaItem> State::now_playing() const {
    if (queue && queue->current && queue->current->media) return queue->current->media;
    if (queue && queue->current_index >= 0 && static_cast<size_t>(queue->current_index) < queue->items.size() &&
        queue->items[queue->current_index].media)
        return queue->items[queue->current_index].media;
    return std::nullopt;
}

bool State::playing() const {
    const core::PlayerState* p = selected();
    return p && p->state == core::PlaybackState::Playing;
}

std::string browse_key(app::cmd::BrowseKind kind, const std::string& of_uri) {
    return std::to_string(static_cast<int>(kind)) + "|" + of_uri;
}

const app::BrowseView* State::list(app::cmd::BrowseKind kind, const std::string& of_uri) const {
    auto it = lists.find(browse_key(kind, of_uri));
    return it == lists.end() ? nullptr : it->second.get();
}

State& state() { return shell().state; }
void send(app::Command command) { shell().send(std::move(command)); }

// ---- Navigation

void open(ScreenId id) {
    auto& s = shell();
    bool is_tab = id == ScreenId::Home || id == ScreenId::Search || id == ScreenId::Library ||
                  id == ScreenId::Queue || id == ScreenId::Settings;
    if (is_tab) s.stack.clear();  // the rail starts a fresh history
    else if (s.current != id) {
        s.stack.push_back(s.current);
        if (s.stack.size() > 8) s.stack.erase(s.stack.begin());
    }
    show(id);
}

void back() {
    auto& s = shell();
    if (s.stack.empty()) return show(screen(s.current).tab());
    ScreenId prev = s.stack.back();
    s.stack.pop_back();
    show(prev);
}

std::string back_label() {
    auto& s = shell();
    return s.stack.empty() ? screen(screen(s.current).tab()).title() : screen(s.stack.back()).title();
}

void set_detail_item(const core::MediaItem& item) { shell().detail = item; }
SettingsPage settings_page() { return shell().page; }

void open_settings_page(SettingsPage page) {
    shell().page = page;
    open(ScreenId::Settings);
}

std::string clock_text(int minutes) {
    char buf[8];
    snprintf(buf, sizeof buf, "%02d:%02d", (minutes / 60) % 24, minutes % 60);
    return buf;
}

lv_obj_t* card(lv_obj_t* parent, const std::string& title) {
    lv_obj_t* c = kit::column(parent, 12);
    lv_obj_set_width(c, lv_pct(100));
    lv_obj_set_style_pad_hor(c, 12, 0);
    lv_obj_set_style_pad_ver(c, 16, 0);
    lv_obj_set_style_pad_row(c, 2, 0);
    lv_obj_set_style_radius(c, theme::r_card(), 0);
    lv_obj_set_style_bg_color(c, theme::surface(), 0);
    lv_obj_set_style_bg_opa(c, LV_OPA_COVER, 0);
    if (!title.empty()) {
        lv_obj_t* t = kit::overline(c, title, theme::text2());
        lv_obj_set_style_pad_left(t, 12, 0);
        lv_obj_set_style_pad_bottom(t, 6, 0);
    }
    return c;
}

lv_obj_t* nav_row(lv_obj_t* parent, const char* glyph, const std::string& title, const std::string& sub, kit::Action action,
                  bool chevron) {
    lv_obj_t* r = kit::row(parent, 16);
    lv_obj_set_size(r, lv_pct(100), sub.empty() ? 60 : 68);
    lv_obj_set_style_pad_hor(r, 12, 0);
    lv_obj_set_style_radius(r, theme::r_control(), 0);
    lv_obj_set_style_bg_color(r, theme::elevated(), LV_STATE_PRESSED);
    lv_obj_set_style_bg_opa(r, LV_OPA_COVER, LV_STATE_PRESSED);
    if (glyph) kit::icon(r, glyph, 26, theme::text2());
    lv_obj_t* col = kit::column(r, 2);
    lv_obj_set_flex_grow(col, 1);
    lv_obj_set_width(col, 1);
    kit::text(col, title, theme::meta(), theme::text());
    if (!sub.empty()) kit::text(col, sub, theme::label(), theme::text2());
    if (chevron) kit::icon(r, icon::chevron_right, 22, theme::text3());
    kit::on_click(r, std::move(action));
    return r;
}

lv_obj_t* banner(lv_obj_t* parent, const std::string& title, const std::string& body) {
    lv_obj_t* b = kit::row(parent, 20);
    lv_obj_set_width(b, lv_pct(100));
    lv_obj_set_style_pad_all(b, 20, 0);
    lv_obj_set_style_radius(b, theme::r_card(), 0);
    lv_obj_set_style_bg_color(b, lv_color_hex(0x2A2416), 0);
    lv_obj_set_style_bg_opa(b, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(b, 1, 0);
    lv_obj_set_style_border_color(b, lv_color_hex(0x5A4A22), 0);
    kit::icon(b, icon::warning, 32, theme::warning());
    lv_obj_t* col = kit::column(b, 4);
    lv_obj_set_flex_grow(col, 1);
    lv_obj_set_width(col, 1);
    kit::text(col, title, theme::body(), theme::text());
    kit::text(col, body, theme::meta(), theme::text2(), true);
    return b;
}

void show_ambient() {
    auto& s = shell();
    if (s.ambient) return;
    kit::close_sheets();
    const app::PanelView* p = s.state.panel.get();
    bool clock = !p || p->display.clock_when_dimmed;
    bool night = p && p->night_now;
    lv_obj_t* a = lv_obj_create(lv_layer_top());
    lv_obj_remove_style_all(a);
    lv_obj_set_size(a, theme::kScreenW, theme::kScreenH);
    lv_obj_set_style_bg_color(a, lv_color_black(), 0);  // always black: it's the sleep screen, in both themes
    lv_obj_set_style_bg_opa(a, LV_OPA_COVER, 0);
    lv_obj_add_flag(a, LV_OBJ_FLAG_CLICKABLE);  // swallows the waking tap
    s.ambient = a;
    s.ambient_clock = nullptr;
    // Fixed dark-theme colours (the panel may be in light mode); dimmer at night.
    lv_color_t fg = night ? lv_color_hex(0x8F8F8F) : lv_color_white();
    lv_color_t fg2 = night ? lv_color_hex(0x6E6E6E) : lv_color_hex(0xA3A3A3);

    if (clock) {
        // Bottom-left: the time, the date under it.
        std::time_t t = std::time(nullptr);
        std::tm* lt = std::localtime(&t);
        lv_obj_t* left = kit::column(a, 14);
        lv_obj_align(left, LV_ALIGN_BOTTOM_LEFT, 72, -64);
        s.ambient_clock = kit::text(left, clock_text(lt->tm_hour * 60 + lt->tm_min), theme::clock(), fg);
        lv_obj_set_style_text_letter_space(s.ambient_clock, -4, 0);
        std::string date = std::string(tr(static_cast<i18n::Str>(static_cast<int>(i18n::Str::day_0) + lt->tm_wday))) + " " +
                           std::to_string(lt->tm_mday) + " " +
                           tr(static_cast<i18n::Str>(static_cast<int>(i18n::Str::month_1) + lt->tm_mon));
        lv_obj_t* d = kit::text(left, date, theme::h2(), fg2);
        lv_obj_set_style_pad_left(d, 8, 0);

        // Bottom-right: what's playing (no cover at night: a bright cover in a dark room glares).
        if (auto item = s.state.now_playing(); item && s.state.playing()) {
            lv_obj_t* np = kit::row(a, 20);
            lv_obj_align(np, LV_ALIGN_BOTTOM_RIGHT, -72, -80);
            if (!night) kit::art(np, *item, 120, theme::r_art(), lv_color_black());
            lv_obj_t* col = kit::column(np, 6);
            lv_obj_set_width(col, 300);
            lv_obj_t* who = kit::row(col, 8);
            kit::equalizer(who, fg2, !night);
            const core::PlayerState* pl = s.state.selected();
            kit::text(who, pl ? pl->name : "", theme::label(), fg2);
            kit::text(col, item->name, theme::h2(), fg);
            kit::text(col, kit::subtitle(*item), theme::meta(), fg2);
        }
    }
    lv_obj_t* hint = kit::text(a, tr(i18n::Str::ambient_tap), theme::label(), lv_color_hex(0x4A4A4A));
    lv_obj_align(hint, LV_ALIGN_TOP_RIGHT, -40, 32);
    send(app::cmd::SetDimmed{true});
    kit::on_click(a, [] {
        auto& sh = shell();
        lv_obj_delete_async(sh.ambient);
        sh.ambient = nullptr;
        sh.ambient_clock = nullptr;
        lv_display_trigger_activity(nullptr);
        send(app::cmd::SetDimmed{false});
    });
}
const core::MediaItem& detail_item() { return shell().detail; }

void open_detail(const core::MediaItem& item) {
    set_detail_item(item);
    if (shell().current == ScreenId::Detail) return show(ScreenId::Detail);  // detail → detail: rebind
    open(ScreenId::Detail);
}

void play(const core::MediaItem& item) { send(app::cmd::Play{item}); }

std::string songs(size_t count) {
    return i18n::tr(count == 1 ? i18n::Str::count_song : i18n::Str::count_songs, std::to_string(count));
}

void open_item(const core::MediaItem& item) {
    if (item.type == core::MediaType::Track || item.type == core::MediaType::Radio) play(item);
    else open_detail(item);
}

lv_obj_t* speaker_chip(lv_obj_t* parent) {
    const core::PlayerState* p = state().selected();
    return kit::chip(parent, icon::speaker, p ? p->name : tr(i18n::Str::speaker_none), false, [] { open_speakers(); });
}

// ---- Entry points

void start(Send send_fn) {
    auto& s = shell();
    s.send = std::move(send_fn);
    s.base = lv_screen_active();
    lv_obj_remove_style_all(s.base);
    lv_obj_set_style_bg_color(s.base, theme::bg(), 0);
    lv_obj_set_style_bg_opa(s.base, LV_OPA_COVER, 0);
    lv_obj_remove_flag(s.base, LV_OBJ_FLAG_SCROLLABLE);
    s.screens.push_back(make_home());
    s.screens.push_back(make_search());
    s.screens.push_back(make_library());
    s.screens.push_back(make_queue());
    s.screens.push_back(make_settings());
    s.screens.push_back(make_now_playing());
    s.screens.push_back(make_detail());
    s.screens.push_back(make_setup());
    s.screens.push_back(make_boot());
    build_all();
    lv_timer_create(tick_progress, 1000, nullptr);
    show(ScreenId::Boot);  // until connected (or the setup flow takes over)
}

void apply(app::Update update) {
    auto& s = shell();
    Topic topic = Topic::All;
    if (auto* c = std::get_if<app::ConnectionView>(&update)) {
        bool came_up = c->state == backend::Connection::Connected && s.state.connection.state != backend::Connection::Connected;
        s.state.connection = *c;
        topic = Topic::Connection;
        if (came_up) {
            s.state.ever_connected = true;
            if (s.current == ScreenId::Boot) return open(ScreenId::Home);
            screen(s.current).on_open();  // (re)connected: reload what the screen shows
        }
    } else if (auto* sv = std::get_if<std::shared_ptr<const app::SetupView>>(&update)) {
        s.state.setup = *sv;
        bool setting_up = (*sv)->step != app::SetupStep::Finished;
        if (setting_up && s.current != ScreenId::Setup) return show(ScreenId::Setup);
        if (!setting_up && s.current == ScreenId::Setup) return open(s.state.connected() ? ScreenId::Home : ScreenId::Boot);
        topic = Topic::Setup;
    } else if (auto* pv = std::get_if<std::shared_ptr<const app::PanelView>>(&update)) {
        bool restyle = !(theme::appearance() == (*pv)->appearance);
        s.state.panel = *pv;
        if (restyle) {  // dark/light, accent or rounding changed: rebuild everything in the new look
            theme::apply((*pv)->appearance);
            build_all();
            show(s.current);
        }
        if (!(*pv)->server_address.empty()) art::set_base("http://" + (*pv)->server_address);
        apply_diagnostics();
        topic = Topic::Panel;
    } else if (auto* p = std::get_if<std::shared_ptr<const app::PlayersView>>(&update)) {
        bool same = s.state.players && signature(**p) == signature(*s.state.players);
        s.state.players = *p;
        if (same) return;  // MA repeats player_updated (elapsed time, other fields we don't show)
        topic = Topic::Players;
    } else if (auto* q = std::get_if<std::shared_ptr<const core::QueueState>>(&update)) {
        bool same = s.state.queue && signature(**q) == signature(*s.state.queue);
        s.state.queue = *q;
        s.state.elapsed_s = (*q)->elapsed_s;
        if (same) {  // only the time moved: no rebuild (it reset scroll and re-faded every cover)
            screen(s.current).refresh(Topic::Progress);
            if (s.progress) lv_bar_set_value(s.progress, static_cast<int32_t>(s.state.elapsed_s), LV_ANIM_ON);
            if (s.elapsed) lv_label_set_text(s.elapsed, duration(s.state.elapsed_s).c_str());
            return;
        }
        topic = Topic::Queue;
    } else if (auto* sv = std::get_if<std::shared_ptr<const core::SearchView>>(&update)) {
        s.state.search = *sv;
        topic = Topic::Search;
    } else if (auto* pl = std::get_if<std::shared_ptr<const app::PlaylistsView>>(&update)) {
        s.state.playlists = *pl;
        topic = Topic::Playlists;
    } else if (auto* b = std::get_if<std::shared_ptr<const app::BrowseView>>(&update)) {
        s.state.lists[browse_key((*b)->kind, (*b)->of_uri)] = *b;
        topic = Topic::Browse;
    } else if (auto* t = std::get_if<app::Toast>(&update)) {
        auto action = t->action;
        kit::toast(t->text, t->action_label, action ? kit::Action([action] { send(*action); }) : kit::Action(), t->error);
        return;
    } else if (std::holds_alternative<app::LanguageChanged>(update)) {
        build_all();  // every label is re-created in the new language
        show(s.current);
        return;
    }
    lv_obj_t* root = s.roots[static_cast<size_t>(s.current)];
    int32_t y = lv_obj_get_scroll_y(root);
    screen(s.current).refresh(topic);
    if (y > 0) {  // a rebuild must not jump the page back to the top
        lv_obj_update_layout(root);
        lv_obj_scroll_to_y(root, y, LV_ANIM_OFF);
    }
    if (topic == Topic::Queue || topic == Topic::Players || topic == Topic::Playlists) {
        build_sidebar();
        refresh_bar();
    }
}

}  // namespace ui
