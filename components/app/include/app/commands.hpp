#pragma once
// Everything the UI can ask for. Adding a command = one struct here + handling it in a feature.
#include <optional>
#include <string>
#include <variant>
#include <vector>

#include "core/model.hpp"
#include "i18n/i18n.hpp"

namespace app {

// Screen & night mode (Settings › Screen). Times in minutes since midnight.
struct DisplaySettings {
    int brightness = 80;
    int dim_after_s = 120;  // 0 = never
    bool clock_when_dimmed = true;
    bool night = true;
    int night_from = 22 * 60, night_to = 7 * 60;
    int night_brightness = 15;
};

struct Diagnostics {
    bool fps = false, touches = false;
};

// Look of the panel (Settings › Appearance). accent: index into the UI's accent list (0 = neutral).
enum class ThemeMode : uint8_t { Dark, Light };
enum class Roundness : uint8_t { Sharp, Normal, Round };
struct Appearance {
    ThemeMode mode = ThemeMode::Dark;
    uint8_t accent = 0;
    Roundness roundness = Roundness::Normal;
    bool reduce_motion = false;  // WCAG 2.2.2 / 2.3.3: no equalizer, press or fade animations
    bool operator==(const Appearance& o) const {
        return mode == o.mode && accent == o.accent && roundness == o.roundness && reduce_motion == o.reduce_motion;
    }
};

enum class SetupStep : uint8_t { Welcome, Wifi, WifiPassword, Server, SignIn, Speaker, Done, Finished };

}  // namespace app

namespace app {
namespace cmd {

struct Search { std::string text; };                 // every keystroke; debounced in core
// Play now. `context`: the album or playlist the song was tapped in, so the rest follows it.
// `radio`: Music Assistant's radio mode, endless similar songs after it.
struct Play { core::MediaItem item; std::optional<core::MediaItem> context = {}; bool radio = false; };
struct AddToQueue { core::MediaItem item; bool next = false; bool force = false; };
struct PlayPause {};
struct Next {};
struct Previous {};
struct Seek { int position_s; };
struct SetShuffle { bool on; };
struct SetRepeat { core::RepeatMode mode; };
struct SelectPlayer { std::string player_id; };
struct SetVolume { std::string player_id; int level; bool whole_group = false; };  // group = master slider
struct SetGroup { std::string leader; std::vector<std::string> members; };  // members besides the leader
struct Ungroup { std::string player_id; };
struct TransferQueue { std::string from_player, to_player; };     // the music moves, queue and all
struct SetSleepTimer { int minutes; };                            // 0 = off; pauses when it runs out
struct ClearUpNext {};                               // keeps the current track playing
struct RemoveFromQueue { std::string queue_item_id; };
struct MoveInQueue { std::string queue_item_id; int shift; };
struct PlayQueueIndex { int index; };
struct SetFavourite { core::MediaItem item; bool on; };
struct SetLanguage { i18n::Lang lang; };
struct SaveRecentSearch { std::string text; };                    // the user acted on this search
// Lists the UI shows. Adding a list = one enum value + one case in features/browse.cpp.
enum class BrowseKind : uint8_t {
    Favourites, RecentlyPlayed, LibraryAlbums, LibraryArtists, LibraryTracks, LibraryRadio,
    AlbumTracks, ArtistTracks, ArtistAlbums, PlaylistTracks,
};
struct Browse { BrowseKind kind; core::MediaItem of; };             // `of`: the album/artist/playlist
struct LoadPlaylists {};                                             // library playlists for Home + picker
struct AddToPlaylist { core::MediaItem playlist; core::MediaItem item; };
struct CreatePlaylist { std::string name; std::optional<core::MediaItem> first_item; };
struct RemoveFromPlaylist { core::MediaItem playlist; int position; };
struct DeletePlaylist { core::MediaItem playlist; };

// First-time setup (features/setup.cpp).
struct SetupGo { SetupStep step; };                                 // Back / Continue
struct SetupScanWifi {};
struct SetupJoinWifi { std::string ssid, password; };
struct SetupFindServers {};
struct SetupUseServer { std::string address; };                     // "host[:port]" or ws:// URL
struct SetupToken { std::string token; };
struct SetupFinish {};

// This panel (features/panel.cpp).
struct SetDisplay { DisplaySettings display; };
struct SetDimmed { bool dimmed; };                                  // UI: idle → dimmed clock
struct SetDiagnostics { Diagnostics diagnostics; };
struct SetAppearance { Appearance appearance; };
struct SetSourceOrder { std::vector<std::string> providers; };      // provider domains, best first
struct Reconnect {};
struct SignInAgain {};
struct RestartPanel {};
struct ResetPanel {};                                               // erase settings, back to setup
struct CheckUpdate {};                                              // is newer firmware published?
struct InstallUpdate {};                                            // download, switch, restart

// Internal broadcast between features (not sent by the UI).
struct PlayerChanged { std::string player_id; };

}  // namespace cmd

using Command = std::variant<cmd::Search, cmd::Play, cmd::AddToQueue, cmd::PlayPause, cmd::Next, cmd::Previous,
                             cmd::Seek, cmd::SetShuffle, cmd::SetRepeat, cmd::SelectPlayer, cmd::SetVolume, cmd::SetGroup, cmd::Ungroup,
                             cmd::TransferQueue, cmd::SetSleepTimer, cmd::SaveRecentSearch, cmd::CheckUpdate, cmd::InstallUpdate,
                             cmd::ClearUpNext, cmd::RemoveFromQueue, cmd::MoveInQueue, cmd::PlayQueueIndex,
                             cmd::SetFavourite, cmd::SetLanguage, cmd::Browse, cmd::LoadPlaylists, cmd::AddToPlaylist,
                             cmd::CreatePlaylist, cmd::RemoveFromPlaylist, cmd::DeletePlaylist, cmd::SetupGo,
                             cmd::SetupScanWifi, cmd::SetupJoinWifi, cmd::SetupFindServers, cmd::SetupUseServer,
                             cmd::SetupToken, cmd::SetupFinish, cmd::SetDisplay, cmd::SetDimmed, cmd::SetDiagnostics, cmd::SetAppearance,
                             cmd::SetSourceOrder, cmd::Reconnect, cmd::SignInAgain, cmd::RestartPanel, cmd::ResetPanel,
                             cmd::PlayerChanged>;

}  // namespace app
