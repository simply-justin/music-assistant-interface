#pragma once
// Everything the UI can ask for. Adding a command = one struct here + handling it in a feature.
#include <optional>
#include <string>
#include <variant>

#include "core/model.hpp"
#include "i18n/i18n.hpp"

namespace app {
namespace cmd {

struct Search { std::string text; };                 // every keystroke; debounced in core
struct Play { core::MediaItem item; };               // replace the queue and play now
struct AddToQueue { core::MediaItem item; bool next = false; bool force = false; };
struct PlayPause {};
struct Next {};
struct Previous {};
struct Seek { int position_s; };
struct SetShuffle { bool on; };
struct SetRepeat { core::RepeatMode mode; };
struct SelectPlayer { std::string player_id; };
struct SetVolume { std::string player_id; int level; bool whole_group = false; };  // group = master slider
struct ClearUpNext {};                               // keeps the current track playing
struct RemoveFromQueue { std::string queue_item_id; };
struct MoveInQueue { std::string queue_item_id; int shift; };
struct PlayQueueIndex { int index; };
struct SetFavourite { core::MediaItem item; bool on; };
struct SetLanguage { i18n::Lang lang; };
struct LoadPlaylists {};                                             // library playlists for Home + picker
struct AddToPlaylist { core::MediaItem playlist; core::MediaItem item; };
struct CreatePlaylist { std::string name; std::optional<core::MediaItem> first_item; };
struct RemoveFromPlaylist { core::MediaItem playlist; int position; };
struct DeletePlaylist { core::MediaItem playlist; };

// Internal broadcast between features (not sent by the UI).
struct PlayerChanged { std::string player_id; };

}  // namespace cmd

using Command = std::variant<cmd::Search, cmd::Play, cmd::AddToQueue, cmd::PlayPause, cmd::Next, cmd::Previous,
                             cmd::Seek, cmd::SetShuffle, cmd::SetRepeat, cmd::SelectPlayer, cmd::SetVolume,
                             cmd::ClearUpNext, cmd::RemoveFromQueue, cmd::MoveInQueue, cmd::PlayQueueIndex,
                             cmd::SetFavourite, cmd::SetLanguage, cmd::LoadPlaylists, cmd::AddToPlaylist,
                             cmd::CreatePlaylist, cmd::RemoveFromPlaylist, cmd::DeletePlaylist, cmd::PlayerChanged>;

}  // namespace app
