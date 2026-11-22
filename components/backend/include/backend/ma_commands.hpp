#pragma once
// Music Assistant commands and result parsers: the ONLY place that knows MA command names,
// argument names and field names. Checked against server 2.10.13 (schema 65) /api-docs and live
// responses on 2026-11-22; anything still unconfirmed is marked "verify".
// Adding a command = one builder here (+ one parser if it returns data).
#include <string>
#include <vector>

#include "backend/backend.hpp"
#include "core/model.hpp"
#include "core/search.hpp"

namespace ma::cmd {

enum class PlayOption : uint8_t { Play, Replace, Next, ReplaceNext, Add };

backend::Request search(const std::string& query, core::SearchStage stage, int limit_per_type);
backend::Request library_items(core::MediaType type, bool favorite_only, int limit, int offset,
                               const char* order_by = "sort_name");
backend::Request play_media(const std::string& queue_id, const std::vector<std::string>& uris, PlayOption option);
backend::Request play_pause(const std::string& queue_id);
backend::Request next(const std::string& queue_id);
backend::Request previous(const std::string& queue_id);
backend::Request seek(const std::string& queue_id, int position_s);
backend::Request shuffle(const std::string& queue_id, bool enabled);
backend::Request repeat(const std::string& queue_id, core::RepeatMode mode);
backend::Request queue_items(const std::string& queue_id, int limit, int offset);
backend::Request queue_delete_item(const std::string& queue_id, const std::string& queue_item_id);
backend::Request queue_move_item(const std::string& queue_id, const std::string& queue_item_id, int pos_shift);
backend::Request queue_clear(const std::string& queue_id);
backend::Request queue_play_index(const std::string& queue_id, int index);
backend::Request players_all();
backend::Request music_providers();  // running music services: name, available (verified 2.10)
backend::Request active_queue(const std::string& player_id);  // null when the player is idle
backend::Request queue_get(const std::string& queue_id);
backend::Request group_volume(const std::string& player_id, int volume_level);
backend::Request volume_set(const std::string& player_id, int volume_level);
backend::Request favorite_add(const std::string& uri);
backend::Request favorite_remove(core::MediaType type, const std::string& library_item_id);
// Browsing.
backend::Request recently_played(int limit);
backend::Request album_tracks(const core::MediaItem& album);
backend::Request artist_tracks(const core::MediaItem& artist);
backend::Request artist_albums(const core::MediaItem& artist);
backend::Request playlist_tracks(const core::MediaItem& playlist);
// Playlists (library ids, i.e. items with provider "library").
backend::Request playlist_add_tracks(const std::string& playlist_id, const std::vector<std::string>& uris);
backend::Request playlist_remove_tracks(const std::string& playlist_id, const std::vector<int>& positions);
backend::Request playlist_create(const std::string& name);
backend::Request playlist_delete(const std::string& playlist_id);

}  // namespace ma::cmd

namespace ma::parse {

core::MediaItem media_item(const cJSON* obj);
std::vector<core::MediaItem> media_items(const cJSON* array);
core::SearchResult search_result(const cJSON* obj);
core::PlayerState player(const cJSON* obj);
core::QueueItem queue_item(const cJSON* obj);
void queue_state(const cJSON* obj, core::QueueState& into);  // keeps `items`

}  // namespace ma::parse
