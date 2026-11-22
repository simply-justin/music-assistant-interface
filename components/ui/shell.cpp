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
    lv_obj_t* rail = nullptr;
    lv_obj_t* mini = nullptr;
    std::vector<lv_obj_t*> rail_items;
    core::MediaItem detail;
    SettingsPage page = SettingsPage::Display;
    lv_obj_t* ambient = nullptr;
    lv_obj_t* ambient_clock = nullptr;
    bool touches_hooked = false;
};

Shell& shell() {
    static Shell s;
    return s;
}

Screen& screen(ScreenId id) { return *shell().screens[static_cast<size_t>(id)]; }

void build_rail();
void build_mini();
void refresh_mini();

void show(ScreenId id) {
    auto& s = shell();
    s.current = id;
    Screen& scr = screen(id);
    for (size_t i = 0; i < s.roots.size(); ++i) {
        if (i == static_cast<size_t>(id)) lv_obj_remove_flag(s.roots[i], LV_OBJ_FLAG_HIDDEN);
        else lv_obj_add_flag(s.roots[i], LV_OBJ_FLAG_HIDDEN);
    }
    if (scr.full_screen()) lv_obj_add_flag(s.rail, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_remove_flag(s.rail, LV_OBJ_FLAG_HIDDEN);
    for (size_t i = 0; i < s.rail_items.size(); ++i) {
        bool on = static_cast<size_t>(scr.tab()) == i;
        lv_obj_set_state(s.rail_items[i], LV_STATE_CHECKED, on);
        // Children don't inherit the state: colour icon (child 0) and label (child 1) directly.
        lv_obj_set_style_text_color(lv_obj_get_child(s.rail_items[i], 0), on ? theme::accent() : theme::text2(), 0);
        lv_obj_set_style_text_color(lv_obj_get_child(s.rail_items[i], 1), on ? theme::text() : theme::text2(), 0);
    }
    kit::close_sheets();
    scr.on_open();
    scr.refresh(Topic::All);
    refresh_mini();
}

void build_all() {
    auto& s = shell();
    for (auto* r : s.roots) lv_obj_delete(r);
    s.roots.clear();
    if (s.rail) lv_obj_delete(s.rail);
    if (s.mini) lv_obj_delete(s.mini);
    s.rail_items.clear();

    for (auto& scr : s.screens) {
        lv_obj_t* root = kit::column(s.base);
        bool full = scr->full_screen();
        lv_obj_set_pos(root, full ? 0 : theme::kRailW, 0);
        lv_obj_set_size(root, full ? theme::kScreenW : theme::kScreenW - theme::kRailW,
                        theme::kScreenH - (scr->mini_player() ? theme::kMiniH : 0));
        lv_obj_add_flag(root, LV_OBJ_FLAG_HIDDEN);
        scr->build(root);
        s.roots.push_back(root);
    }
    build_rail();
    build_mini();
}

void build_rail() {
    auto& s = shell();
    s.rail = kit::column(s.base, 8);
    lv_obj_set_size(s.rail, theme::kRailW, theme::kScreenH);
    lv_obj_set_style_pad_ver(s.rail, 24, 0);
    lv_obj_set_style_pad_left(s.rail, 3, 0);
    lv_obj_set_flex_align(s.rail, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    struct Item { ScreenId id; const char* glyph; i18n::Str label; };
    const Item items[] = {{ScreenId::Home, icon::home, i18n::Str::nav_home},
                          {ScreenId::Search, icon::search, i18n::Str::nav_search},
                          {ScreenId::Library, icon::library, i18n::Str::nav_library},
                          {ScreenId::Queue, icon::queue, i18n::Str::nav_queue},
                          {ScreenId::Settings, icon::settings, i18n::Str::nav_settings}};
    for (const auto& it : items) {
        if (it.id == ScreenId::Settings) kit::spacer(s.rail);
        lv_obj_t* b = kit::column(s.rail, 4);
        lv_obj_set_size(b, 90, 76);  // wide enough for "Instellingen" at 14 px
        lv_obj_set_flex_align(b, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_radius(b, 16, 0);
        lv_obj_set_style_bg_color(b, theme::surface(), LV_STATE_CHECKED);
        lv_obj_set_style_bg_opa(b, LV_OPA_COVER, LV_STATE_CHECKED);
        lv_obj_set_style_bg_color(b, theme::elevated(), LV_STATE_PRESSED);
        lv_obj_set_style_bg_opa(b, LV_OPA_COVER, LV_STATE_PRESSED);
        kit::icon(b, it.glyph, 28, theme::text2());
        kit::text(b, tr(it.label), theme::label(), theme::text2());
        ScreenId id = it.id;
        kit::on_click(b, [id] { open(id); });
        s.rail_items.push_back(b);
    }
}

// Mini-player (96 px): what's playing, transport, speaker, volume, queue. Hidden when idle.
void build_mini() {
    auto& s = shell();
    s.mini = kit::row(s.base, 16);
    lv_obj_set_pos(s.mini, theme::kRailW, theme::kScreenH - theme::kMiniH);
    lv_obj_set_size(s.mini, theme::kScreenW - theme::kRailW, theme::kMiniH);
    lv_obj_set_style_pad_hor(s.mini, 24, 0);
    lv_obj_set_style_bg_color(s.mini, theme::surface(), 0);
    lv_obj_set_style_bg_opa(s.mini, LV_OPA_COVER, 0);
    lv_obj_set_style_border_side(s.mini, LV_BORDER_SIDE_TOP, 0);
    lv_obj_set_style_border_width(s.mini, 1, 0);
    lv_obj_set_style_border_color(s.mini, theme::elevated(), 0);
}

void refresh_mini() {
    auto& s = shell();
    Screen& scr = screen(s.current);
    auto item = s.state.now_playing();
    if (!scr.mini_player() || scr.full_screen() || !item) {
        lv_obj_add_flag(s.mini, LV_OBJ_FLAG_HIDDEN);
        return;
    }
    lv_obj_remove_flag(s.mini, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clean(s.mini);
    // Progress strip (3 px) along the top edge.
    lv_obj_t* bar = lv_bar_create(s.mini);
    lv_obj_add_flag(bar, LV_OBJ_FLAG_IGNORE_LAYOUT);
    lv_obj_set_size(bar, theme::kScreenW - theme::kRailW, 3);
    lv_obj_set_pos(bar, -24, -1);
    lv_obj_set_style_radius(bar, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(bar, 0, LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(bar, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_bg_color(bar, theme::accent(), LV_PART_INDICATOR);
    lv_bar_set_range(bar, 0, item->duration_s ? static_cast<int32_t>(item->duration_s) : 1);
    lv_bar_set_value(bar, static_cast<int32_t>(s.state.elapsed_s), LV_ANIM_OFF);

    lv_obj_t* info = kit::row(s.mini, 16);
    lv_obj_set_width(info, 400);
    kit::art(info, *item, 64, 10, theme::surface());
    lv_obj_t* col = kit::column(info, 4);
    lv_obj_set_flex_grow(col, 1);
    lv_obj_set_width(col, 1);
    kit::text(col, item->name, theme::body(), theme::text());
    lv_obj_t* sub = kit::row(col, 8);
    kit::text(sub, kit::subtitle(*item), theme::meta(), theme::text2());
    kit::badge(sub, item->badge_provider());
    kit::on_click(info, [] { open(ScreenId::NowPlaying); });

    lv_obj_t* mid = kit::row(s.mini, 12);
    lv_obj_set_flex_grow(mid, 1);
    lv_obj_set_flex_align(mid, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    const core::PlayerState* p = s.state.selected();
    bool radio = item->type == core::MediaType::Radio;
    if (!radio && (!p || p->can_next_previous)) kit::icon_button(mid, icon::prev, [] { send(app::cmd::Previous{}); });
    kit::play_button(mid, s.state.playing(), [] { send(app::cmd::PlayPause{}); }, 64);
    if (!radio && (!p || p->can_next_previous)) kit::icon_button(mid, icon::next, [] { send(app::cmd::Next{}); });

    lv_obj_t* right = kit::row(s.mini, 8);
    lv_obj_set_width(right, 400);
    lv_obj_set_flex_align(right, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    speaker_chip(right);
    kit::icon_button(right, icon::volume, [] { open_volume(); });
    kit::icon_button(right, icon::queue, [] { open(ScreenId::Queue); });
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

void tick_progress(lv_timer_t*) {
    auto& s = shell();
    tick_ambient();
    if (!s.state.playing()) return;
    ++s.state.elapsed_s;
    screen(s.current).refresh(Topic::Progress);
    // Mini-player: only the bar moves.
    if (!lv_obj_has_flag(s.mini, LV_OBJ_FLAG_HIDDEN) && lv_obj_get_child_count(s.mini) > 0)
        lv_bar_set_value(lv_obj_get_child(s.mini, 0), static_cast<int32_t>(s.state.elapsed_s), LV_ANIM_OFF);
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
    if (shell().current == ScreenId::SettingsPage) return show(ScreenId::SettingsPage);
    open(ScreenId::SettingsPage);
}

std::string clock_text(int minutes) {
    char buf[8];
    snprintf(buf, sizeof buf, "%02d:%02d", (minutes / 60) % 24, minutes % 60);
    return buf;
}

lv_obj_t* card(lv_obj_t* parent, const std::string& title) {
    lv_obj_t* c = kit::column(parent, 12);
    lv_obj_set_width(c, lv_pct(100));
    lv_obj_set_style_pad_all(c, 28, 0);
    lv_obj_set_style_radius(c, theme::kRadiusCard, 0);
    lv_obj_set_style_bg_color(c, theme::surface(), 0);
    lv_obj_set_style_bg_opa(c, LV_OPA_COVER, 0);
    if (!title.empty()) kit::text(c, title, theme::h2(), theme::text());
    return c;
}

lv_obj_t* nav_row(lv_obj_t* parent, const char* glyph, const std::string& title, const std::string& sub, kit::Action action,
                  bool chevron) {
    lv_obj_t* r = kit::row(parent, 16);
    lv_obj_set_size(r, lv_pct(100), sub.empty() ? 64 : 80);
    lv_obj_set_style_pad_hor(r, 16, 0);
    lv_obj_set_style_radius(r, theme::kRadiusRow, 0);
    lv_obj_set_style_bg_color(r, theme::elevated(), 0);
    lv_obj_set_style_bg_opa(r, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(r, lv_color_mix(lv_color_white(), theme::elevated(), 31), LV_STATE_PRESSED);
    if (glyph) kit::icon(r, glyph, 28, theme::text2());
    lv_obj_t* col = kit::column(r, 4);
    lv_obj_set_flex_grow(col, 1);
    lv_obj_set_width(col, 1);
    kit::text(col, title, theme::body(), theme::text());
    if (!sub.empty()) kit::text(col, sub, theme::meta(), theme::text2());
    if (chevron) kit::icon(r, icon::chevron_right, 28, theme::text3());
    kit::on_click(r, std::move(action));
    return r;
}

lv_obj_t* banner(lv_obj_t* parent, const std::string& title, const std::string& body) {
    lv_obj_t* b = kit::row(parent, 20);
    lv_obj_set_width(b, lv_pct(100));
    lv_obj_set_style_pad_all(b, 20, 0);
    lv_obj_set_style_radius(b, 16, 0);
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
    lv_obj_set_style_bg_color(a, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(a, LV_OPA_COVER, 0);
    lv_obj_set_flex_flow(a, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(a, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(a, 12, 0);
    lv_obj_add_flag(a, LV_OBJ_FLAG_CLICKABLE);  // swallows the waking tap
    s.ambient = a;
    s.ambient_clock = nullptr;
    lv_color_t fg = night ? theme::text3() : theme::text();
    if (clock) {
        std::time_t t = std::time(nullptr);
        std::tm* lt = std::localtime(&t);
        s.ambient_clock = kit::text(a, clock_text(lt->tm_hour * 60 + lt->tm_min), theme::clock(), fg);
        std::string date = std::string(tr(static_cast<i18n::Str>(static_cast<int>(i18n::Str::day_0) + lt->tm_wday))) + " " +
                           std::to_string(lt->tm_mday) + " " +
                           tr(static_cast<i18n::Str>(static_cast<int>(i18n::Str::month_1) + lt->tm_mon));
        kit::text(a, date, theme::h2(), night ? theme::text3() : theme::text2());
        if (auto item = s.state.now_playing(); item && s.state.playing()) {
            lv_obj_t* np = kit::row(a, 12);
            lv_obj_set_style_pad_top(np, 48, 0);
            kit::icon(np, icon::volume, 22, theme::text3());
            const core::PlayerState* pl = s.state.selected();
            kit::text(np, item->name + " · " + kit::subtitle(*item) + (pl ? " · " + pl->name : ""), theme::lead(), theme::text3());
        }
    }
    lv_obj_t* hint = kit::text(a, tr(i18n::Str::ambient_tap), theme::label(), lv_color_hex(0x4A5260));
    lv_obj_add_flag(hint, LV_OBJ_FLAG_IGNORE_LAYOUT);
    lv_obj_align(hint, LV_ALIGN_BOTTOM_MID, 0, -32);
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
    s.screens.push_back(make_settings_page());
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
        s.state.panel = *pv;
        if (!(*pv)->server_address.empty()) art::set_base("http://" + (*pv)->server_address);
        apply_diagnostics();
        topic = Topic::Panel;
    } else if (auto* p = std::get_if<std::shared_ptr<const app::PlayersView>>(&update)) {
        s.state.players = *p;
        topic = Topic::Players;
    } else if (auto* q = std::get_if<std::shared_ptr<const core::QueueState>>(&update)) {
        s.state.queue = *q;
        s.state.elapsed_s = (*q)->elapsed_s;
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
    screen(s.current).refresh(topic);
    if (topic == Topic::Queue || topic == Topic::Players) refresh_mini();
}

}  // namespace ui
