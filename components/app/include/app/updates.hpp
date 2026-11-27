#pragma once
// Everything core sends to the UI. Views are immutable snapshots, so the UI task can read them
// without locks.
#include <memory>
#include <optional>
#include <string>
#include <variant>
#include <vector>

#include "app/commands.hpp"
#include "app/platform.hpp"
#include "backend/backend.hpp"
#include "core/model.hpp"
#include "core/search.hpp"

namespace app {

struct ConnectionView {
    backend::Connection state = backend::Connection::Disconnected;
    backend::ServerInfo server;
};

struct PlayersView {
    std::vector<core::PlayerState> players;
    std::string selected_id;
};

struct Toast {
    std::string text;
    std::optional<Command> action;  // e.g. "Add again"
    std::string action_label;
    bool error = false;  // shown with a warning icon instead of a check
};

struct LanguageChanged { i18n::Lang lang; };

// The playing song's position, every second: apart from QueueState so the whole queue is not
// copied (and compared by the UI) just because the clock moved.
struct QueueTime {
    std::string queue_id;
    uint32_t elapsed_s = 0;
};

struct SleepTimerView { int minutes_left = 0; };  // 0 = off

struct RecentSearches { std::vector<std::string> queries; };  // newest first

enum class UpdateState : uint8_t { Idle, Checking, UpToDate, Available, Installing, Failed, NoNetwork };
struct UpdateView {
    UpdateState state = UpdateState::Idle;
    std::string version;  // the newer version, when Available
    int percent = 0;      // while Installing
};

struct BrowseView {
    cmd::BrowseKind kind;
    std::string of_uri;  // which album/artist/playlist, empty for library lists
    bool loading = true, failed = false;
    std::vector<core::MediaItem> items;
};

struct SetupView {
    SetupStep step = SetupStep::Welcome;
    std::vector<WifiNetwork> networks;
    bool scanning = false;
    std::string ssid;                    // network being joined
    bool joining = false;
    std::optional<WifiResult> wifi_error;
    std::vector<ServerCandidate> servers;
    bool searching = false;
    std::string address;                 // server being checked / chosen
    bool checking = false, server_error = false;
    bool server_too_old = false;         // it answered, but its API is older than this panel needs
    std::string pairing_url;
    bool token_error = false;
    bool token_no_answer = false;        // the check got no answer (timeout or connection lost)
};

struct Provider {
    std::string instance_id, domain, name;
    bool available = true, streaming = false;
};

struct PanelView {
    DisplaySettings display;
    Diagnostics diagnostics;
    Appearance appearance;
    bool night_now = false, dimmed = false;
    WifiStatus wifi;
    SystemInfo system;
    std::string server_address;
    std::string token_hint;              // last 4 characters only
    std::vector<Provider> providers;     // music services from the server
    std::vector<std::string> source_order;
};

struct PlaylistsView {
    std::vector<core::MediaItem> playlists;  // library playlists; `editable` ones go in the picker
};

using Update = std::variant<ConnectionView, std::shared_ptr<const core::SearchView>, std::shared_ptr<const PlayersView>,
                            std::shared_ptr<const core::QueueState>, std::shared_ptr<const PlaylistsView>,
                            std::shared_ptr<const BrowseView>, std::shared_ptr<const SetupView>,
                            std::shared_ptr<const PanelView>, Toast,
                            LanguageChanged, QueueTime, SleepTimerView, RecentSearches, UpdateView>;

}  // namespace app
