#include "shell.hpp"

#include <algorithm>
#include <vector>

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
    kit::art(info, *item, 64, 10);
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

void tick_progress(lv_timer_t*) {
    auto& s = shell();
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
    build_all();
    lv_timer_create(tick_progress, 1000, nullptr);
    show(ScreenId::Home);
}

void apply(app::Update update) {
    auto& s = shell();
    Topic topic = Topic::All;
    if (auto* c = std::get_if<app::ConnectionView>(&update)) {
        bool came_up = c->state == backend::Connection::Connected && s.state.connection.state != backend::Connection::Connected;
        s.state.connection = *c;
        topic = Topic::Connection;
        if (came_up) screen(s.current).on_open();  // (re)connected: reload what the screen shows
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
