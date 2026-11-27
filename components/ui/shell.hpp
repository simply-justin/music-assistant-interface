#pragma once
// Internal: shared UI state, the Screen interface and helpers for screens and overlays.
#include <deque>
#include <map>
#include <memory>
#include <optional>
#include <string>

#include "i18n/i18n.hpp"
#include "kit.hpp"
#include "ui/ui.hpp"

namespace ui {

// What changed, so a screen can skip work (e.g. a progress tick must not rebuild lists).
enum class Topic : uint8_t { All, Connection, Players, Queue, Progress, Search, Playlists, Browse, Setup, Panel, Sleep, Update };

// Latest snapshot of every core view.
struct State {
    app::ConnectionView connection;
    std::shared_ptr<const app::PlayersView> players;
    std::shared_ptr<const core::QueueState> queue;
    std::shared_ptr<const core::SearchView> search;
    std::shared_ptr<const app::PlaylistsView> playlists;
    std::map<std::string, std::shared_ptr<const app::BrowseView>> lists;  // key: browse_key()
    std::shared_ptr<const app::SetupView> setup;
    std::shared_ptr<const app::PanelView> panel;
    bool ever_connected = false;
    uint32_t connected_epoch = 0;  // +1 on every (re)connect: screens reload their lists
    std::vector<std::string> recent_searches;
    int sleep_minutes = 0;         // sleep timer, 0 = off
    app::UpdateView update;
    // Hearts tapped here, by uri: shown at once, whatever the (older) snapshot of the item says.
    std::map<std::string, bool> favourite_override;
    bool favourite(const core::MediaItem& m) const;
    uint32_t elapsed_s = 0;  // local 1 Hz clock, re-synced from the queue
    // A song just tapped: shown as playing right away, until the queue confirms it (the server and
    // speaker take ~3 s) or kPendingMs passes.
    std::optional<core::MediaItem> pending;
    uint32_t pending_at_ms = 0;
    static constexpr uint32_t kPendingMs = 8000;

    const core::PlayerState* selected() const;
    std::optional<core::MediaItem> now_playing() const;  // the tapped song, else the queue's current one
    bool playing() const;
    bool connected() const { return connection.state == backend::Connection::Connected; }
    const app::BrowseView* list(app::cmd::BrowseKind kind, const std::string& of_uri = "") const;
};

std::string browse_key(app::cmd::BrowseKind kind, const std::string& of_uri);

State& state();
void send(app::Command command);
using i18n::tr;

class Screen {
public:
    virtual ~Screen() = default;
    virtual void build(lv_obj_t* root) = 0;  // once (and again after a language change)
    virtual void refresh(Topic topic) = 0;   // state changed while visible, or just shown (Topic::All)
    virtual void on_open() {}                // navigated to: request data here
    virtual bool full_screen() const { return false; }  // covers the sidebar (Now Playing, setup)
    virtual bool player_bar() const { return true; }   // the bottom player bar
    virtual ScreenId tab() const = 0;        // highlighted rail item
    virtual std::string title() const = 0;   // label for the next screen's back chip
};

std::unique_ptr<Screen> make_home();
std::unique_ptr<Screen> make_search();
std::unique_ptr<Screen> make_library();
std::unique_ptr<Screen> make_queue();
std::unique_ptr<Screen> make_settings();
std::unique_ptr<Screen> make_now_playing();
std::unique_ptr<Screen> make_detail();
std::unique_ptr<Screen> make_setup();
std::unique_ptr<Screen> make_boot();
SettingsPage settings_page();

// Navigation for screens.
void back();
std::string back_label();  // "Library", "Search", ...
void set_detail_item(const core::MediaItem& item);
const core::MediaItem& detail_item();

// Overlays (overlays.cpp).
void open_add_to_playlist(const core::MediaItem& item);
void open_new_playlist(std::optional<core::MediaItem> first_item);
void confirm(const std::string& title, const std::string& body, const std::string& keep, const std::string& go,
             kit::Action on_go);

// Shared bits used by several screens.
std::string songs(size_t count);  // "1 song", "14 songs" / "1 nummer", "14 nummers"
std::string clock_text(int minutes);  // 22:00
// Grouped rows (Settings, setup): group() = optional title + card; line() = one row in it, with a
// hairline above every row but the first. kRowPad is the card's inner left/right padding.
constexpr int32_t kRowPad = 20;
lv_obj_t* group(lv_obj_t* parent, const std::string& title);
lv_obj_t* line(lv_obj_t* card, int32_t height = 64);  // 0 = as tall as its content
lv_obj_t* grow_text(lv_obj_t* row, const std::string& s, const std::string& sub = {});
void footnote(lv_obj_t* card, const std::string& s);  // grey text under a group
void value_row(lv_obj_t* card, const std::string& label, const std::string& value, lv_color_t colour = theme::text2());
void toggle_row(lv_obj_t* card, const std::string& label, const std::string& sub, bool on, std::function<void(bool)> changed);
// The whole row is the button: text in the accent, or the warning colour for destructive actions.
void action_row(lv_obj_t* card, const char* glyph, const std::string& label, kit::Action action, bool danger = false);
// A card-like rounded block on `surface` (Settings, setup lists).
lv_obj_t* card(lv_obj_t* parent, const std::string& title = {});
// A tappable settings/list row: icon, title, optional subtitle, optional chevron.
lv_obj_t* nav_row(lv_obj_t* parent, const char* glyph, const std::string& title, const std::string& sub, kit::Action action,
                  bool chevron = true);
// A speaker as a fully tappable row: radio dot, name, live status, optional "Group" button.
lv_obj_t* speaker_row(lv_obj_t* parent, const core::PlayerState& p, bool selected);
void open_group(const std::string& leader_id);
// Warning banner (offline, provider down, playback error).
lv_obj_t* banner(lv_obj_t* parent, const std::string& title, const std::string& body);
lv_obj_t* speaker_chip(lv_obj_t* parent);
// Play now (tap on a row/card that plays). `context`: the album or playlist it was tapped in, so the
// rest of that list follows. `radio`: endless similar songs after it.
void play(const core::MediaItem& item, const std::optional<core::MediaItem>& context = std::nullopt, bool radio = false);
void set_favourite(const core::MediaItem& item, bool on);  // heart: shows at once, then tells the server
void open_item(const core::MediaItem& item);  // tap: tracks/radio play, others open their detail

}  // namespace ui
