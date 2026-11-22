#pragma once
// Provider-agnostic domain model. The UI depends only on these types.
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace core {

enum class MediaType : uint8_t { Track, Album, Artist, Playlist, Radio, Unknown };

// One provider-side copy of an item (MA `provider_mappings[]`).
struct ProviderReference {
    std::string item_id;
    std::string provider_domain;    // e.g. "spotify": drives the grey badge
    std::string provider_instance;  // e.g. "spotify--x1y2"
    bool available = true;
    bool in_library = false;
};

struct Image {
    std::string path;
    std::string provider;
    std::string proxy_id;  // verify: newer servers only
    bool remotely_accessible = false;
};

// An artist or album as nested inside another item.
struct ItemRef {
    std::string item_id, provider, name, uri;
};

// One item of any media type. Type-specific fields stay empty for other types, which keeps
// lists, dedupe and the UI generic.
struct MediaItem {
    MediaType type = MediaType::Unknown;
    std::string item_id, provider, name, version, uri;  // `uri` is what playback is routed by
    std::string isrc;                                   // from external_ids (tracks)
    std::vector<ProviderReference> mappings;
    std::optional<Image> image;
    bool favorite = false;
    bool editable = false;         // playlists: Music Assistant allows adding/removing songs
    uint32_t duration_s = 0;       // 0 = unknown
    std::vector<ItemRef> artists;  // tracks, albums
    std::optional<ItemRef> album;  // tracks
    // Other sources of the same recording, for the "Play from" sheet. Only dedupe fills this.
    std::vector<MediaItem> alternatives;

    bool in_library() const;
    // Provider shown in the badge: the first available mapping, else `provider`.
    const std::string& badge_provider() const;
};

struct SearchResult {
    std::vector<MediaItem> tracks, artists, albums, playlists, radio;
    std::optional<MediaItem> top;  // filled by core::pick_top_result

    bool empty() const {
        return tracks.empty() && artists.empty() && albums.empty() && playlists.empty() && radio.empty();
    }
};

enum class PlaybackState : uint8_t { Idle, Playing, Paused, Unknown };
enum class RepeatMode : uint8_t { Off, One, All };

struct PlayerState {
    std::string player_id, name;
    PlaybackState state = PlaybackState::Unknown;
    int volume_level = 0;  // 0..100
    int group_volume = 0;  // 0..100, the master slider for a group
    bool available = true;
    // From `supported_features`. The UI hides controls whose flag is false.
    bool can_seek = false, can_next_previous = false, can_set_volume = false, can_pause = false;
    std::vector<std::string> group_members;  // player_ids (incl. the leader); empty = not a group
    std::vector<std::string> can_group_with;  // players this one can be grouped with (MA decides)
    std::string synced_to;                    // set when this player is a member of someone's group
    // `current_media`: also carries radio stream metadata.
    std::string media_title, media_artist, media_image_url;
};

struct QueueItem {
    std::string queue_item_id, name;
    uint32_t duration_s = 0;
    bool available = true;
    std::optional<MediaItem> media;
};

struct QueueState {
    std::string queue_id;
    int current_index = -1;
    bool shuffle = false;
    RepeatMode repeat = RepeatMode::Off;
    uint32_t elapsed_s = 0;
    std::optional<QueueItem> current;  // what is playing now (may be outside `items` when shuffled)
    std::vector<QueueItem> items;
};

}  // namespace core
