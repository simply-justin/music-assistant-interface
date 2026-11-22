#pragma once
// Everything core sends to the UI. Views are immutable snapshots, so the UI task can read them
// without locks.
#include <memory>
#include <optional>
#include <string>
#include <variant>
#include <vector>

#include "app/commands.hpp"
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

struct BrowseView {
    cmd::BrowseKind kind;
    std::string of_uri;  // which album/artist/playlist, empty for library lists
    bool loading = true, failed = false;
    std::vector<core::MediaItem> items;
};

struct PlaylistsView {
    std::vector<core::MediaItem> playlists;  // library playlists; `editable` ones go in the picker
};

using Update = std::variant<ConnectionView, std::shared_ptr<const core::SearchView>, std::shared_ptr<const PlayersView>,
                            std::shared_ptr<const core::QueueState>, std::shared_ptr<const PlaylistsView>,
                            std::shared_ptr<const BrowseView>, Toast,
                            LanguageChanged>;

}  // namespace app
