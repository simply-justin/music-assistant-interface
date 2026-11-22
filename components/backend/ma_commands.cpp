#include "backend/ma_commands.hpp"

using backend::Json;
using backend::Request;
using backend::json_bool;
using backend::json_num;
using backend::json_str;

namespace ma::cmd {

namespace {

Request make(const char* command) { return {command, Json(cJSON_CreateObject())}; }

Request on_queue(const char* command, const std::string& queue_id) {
    Request r = make(command);
    cJSON_AddStringToObject(r.args.get(), "queue_id", queue_id.c_str());
    return r;
}

const char* type_path(core::MediaType t) {
    switch (t) {
        case core::MediaType::Track: return "tracks";
        case core::MediaType::Album: return "albums";
        case core::MediaType::Artist: return "artists";
        case core::MediaType::Playlist: return "playlists";
        case core::MediaType::Radio: return "radios";
        default: return "";
    }
}

const char* type_name(core::MediaType t) {
    switch (t) {
        case core::MediaType::Track: return "track";
        case core::MediaType::Album: return "album";
        case core::MediaType::Artist: return "artist";
        case core::MediaType::Playlist: return "playlist";
        case core::MediaType::Radio: return "radio";
        default: return "unknown";
    }
}

}  // namespace

Request search(const std::string& query, core::SearchStage stage, int limit_per_type) {
    Request r = make("music/search");
    cJSON* a = r.args.get();
    cJSON_AddStringToObject(a, "search_query", query.c_str());
    const char* types[] = {"artist", "album", "track", "playlist", "radio"};
    cJSON_AddItemToObject(a, "media_types", cJSON_CreateStringArray(types, 5));
    cJSON_AddNumberToObject(a, "limit", limit_per_type);
    if (stage == core::SearchStage::Library) {
        const char* lib[] = {"library"};  // verified 2.10: "library" selects the library
        cJSON_AddItemToObject(a, "providers", cJSON_CreateStringArray(lib, 1));
    }
    return r;
}

Request library_items(core::MediaType type, bool favorite_only, int limit, int offset, const char* order_by) {
    std::string command = std::string("music/") + type_path(type) + "/library_items";
    Request r{command, Json(cJSON_CreateObject())};
    cJSON* a = r.args.get();
    if (favorite_only) cJSON_AddBoolToObject(a, "favorite", true);
    cJSON_AddNumberToObject(a, "limit", limit);
    cJSON_AddNumberToObject(a, "offset", offset);
    cJSON_AddStringToObject(a, "order_by", order_by);
    return r;
}

Request play_media(const std::string& queue_id, const std::vector<std::string>& uris, PlayOption option) {
    static const char* names[] = {"play", "replace", "next", "replace_next", "add"};
    Request r = on_queue("player_queues/play_media", queue_id);
    cJSON* media = cJSON_AddArrayToObject(r.args.get(), "media");
    for (const auto& u : uris) cJSON_AddItemToArray(media, cJSON_CreateString(u.c_str()));
    cJSON_AddStringToObject(r.args.get(), "option", names[static_cast<int>(option)]);
    return r;
}

Request play_pause(const std::string& q) { return on_queue("player_queues/play_pause", q); }
Request next(const std::string& q) { return on_queue("player_queues/next", q); }
Request previous(const std::string& q) { return on_queue("player_queues/previous", q); }

Request seek(const std::string& q, int position_s) {
    Request r = on_queue("player_queues/seek", q);
    cJSON_AddNumberToObject(r.args.get(), "position", position_s);
    return r;
}

Request shuffle(const std::string& q, bool enabled) {
    Request r = on_queue("player_queues/shuffle", q);
    cJSON_AddBoolToObject(r.args.get(), "shuffle_enabled", enabled);
    return r;
}

Request repeat(const std::string& q, core::RepeatMode mode) {
    static const char* names[] = {"off", "one", "all"};
    Request r = on_queue("player_queues/repeat", q);
    cJSON_AddStringToObject(r.args.get(), "repeat_mode", names[static_cast<int>(mode)]);
    return r;
}

Request queue_items(const std::string& q, int limit, int offset) {
    Request r = on_queue("player_queues/items", q);
    cJSON_AddNumberToObject(r.args.get(), "limit", limit);
    cJSON_AddNumberToObject(r.args.get(), "offset", offset);
    return r;
}

Request queue_delete_item(const std::string& q, const std::string& item_id) {
    Request r = on_queue("player_queues/delete_item", q);
    cJSON_AddStringToObject(r.args.get(), "item_id_or_index", item_id.c_str());
    return r;
}

Request queue_move_item(const std::string& q, const std::string& item_id, int pos_shift) {
    Request r = on_queue("player_queues/move_item", q);
    cJSON_AddStringToObject(r.args.get(), "queue_item_id", item_id.c_str());
    cJSON_AddNumberToObject(r.args.get(), "pos_shift", pos_shift);
    return r;
}

// Empties the whole queue (the player stops, even with skip_stop: tested on 2.10.13). "Clear up
// next" therefore deletes items instead (features/queue.cpp); this is only used when idle.
Request queue_clear(const std::string& q) {
    Request r = on_queue("player_queues/clear", q);
    cJSON_AddBoolToObject(r.args.get(), "skip_stop", true);
    return r;
}

Request queue_play_index(const std::string& q, int index) {
    Request r = on_queue("player_queues/play_index", q);
    cJSON_AddNumberToObject(r.args.get(), "index", index);
    return r;
}

Request players_all() { return {"players/all", nullptr}; }

Request music_providers() {
    Request r = make("providers");
    cJSON_AddStringToObject(r.args.get(), "provider_type", "music");
    return r;
}

Request active_queue(const std::string& player_id) {
    Request r = make("player_queues/get_active_queue");
    cJSON_AddStringToObject(r.args.get(), "player_id", player_id.c_str());
    return r;
}

Request queue_get(const std::string& q) { return on_queue("player_queues/get", q); }

Request group_volume(const std::string& player_id, int volume_level) {
    Request r = make("players/cmd/group_volume");
    cJSON_AddStringToObject(r.args.get(), "player_id", player_id.c_str());
    cJSON_AddNumberToObject(r.args.get(), "volume_level", volume_level);
    return r;
}

Request volume_set(const std::string& player_id, int volume_level) {
    Request r = make("players/cmd/volume_set");
    cJSON_AddStringToObject(r.args.get(), "player_id", player_id.c_str());
    cJSON_AddNumberToObject(r.args.get(), "volume_level", volume_level);
    return r;
}

Request favorite_add(const std::string& uri) {
    Request r = make("music/favorites/add_item");
    cJSON_AddStringToObject(r.args.get(), "item", uri.c_str());
    return r;
}

Request favorite_remove(core::MediaType type, const std::string& library_item_id) {
    Request r = make("music/favorites/remove_item");
    cJSON_AddStringToObject(r.args.get(), "media_type", type_name(type));
    cJSON_AddStringToObject(r.args.get(), "library_item_id", library_item_id.c_str());
    return r;
}

namespace {
Request on_item(const char* command, const core::MediaItem& item) {
    Request r = make(command);
    cJSON_AddStringToObject(r.args.get(), "item_id", item.item_id.c_str());
    cJSON_AddStringToObject(r.args.get(), "provider_instance_id_or_domain", item.provider.c_str());
    return r;
}
}  // namespace

Request recently_played(int limit) {
    Request r = make("music/recently_played_items");
    cJSON_AddNumberToObject(r.args.get(), "limit", limit);
    return r;
}
Request album_tracks(const core::MediaItem& album) { return on_item("music/albums/album_tracks", album); }
Request artist_tracks(const core::MediaItem& artist) { return on_item("music/artists/artist_tracks", artist); }
Request artist_albums(const core::MediaItem& artist) { return on_item("music/artists/artist_albums", artist); }
Request playlist_tracks(const core::MediaItem& playlist) { return on_item("music/playlists/playlist_tracks", playlist); }

Request playlist_add_tracks(const std::string& playlist_id, const std::vector<std::string>& uris) {
    Request r = make("music/playlists/add_playlist_tracks");
    cJSON_AddStringToObject(r.args.get(), "db_playlist_id", playlist_id.c_str());
    cJSON* list = cJSON_AddArrayToObject(r.args.get(), "uris");
    for (const auto& u : uris) cJSON_AddItemToArray(list, cJSON_CreateString(u.c_str()));
    return r;
}

Request playlist_remove_tracks(const std::string& playlist_id, const std::vector<int>& positions) {
    Request r = make("music/playlists/remove_playlist_tracks");
    cJSON_AddStringToObject(r.args.get(), "db_playlist_id", playlist_id.c_str());
    cJSON* list = cJSON_AddArrayToObject(r.args.get(), "positions_to_remove");
    // Our positions are 0-based list indexes; MA counts from 1 (tested live on 2.10.13).
    for (int p : positions) cJSON_AddItemToArray(list, cJSON_CreateNumber(p + 1));
    return r;
}

Request playlist_create(const std::string& name) {
    Request r = make("music/playlists/create_playlist");
    cJSON_AddStringToObject(r.args.get(), "name", name.c_str());
    return r;
}

Request playlist_delete(const std::string& playlist_id) {
    Request r = make("music/playlists/remove");
    cJSON_AddStringToObject(r.args.get(), "item_id", playlist_id.c_str());
    return r;
}

}  // namespace ma::cmd

namespace ma::parse {

namespace {

core::MediaType media_type(const std::string& s) {
    if (s == "track") return core::MediaType::Track;
    if (s == "album") return core::MediaType::Album;
    if (s == "artist") return core::MediaType::Artist;
    if (s == "playlist") return core::MediaType::Playlist;
    if (s == "radio") return core::MediaType::Radio;
    return core::MediaType::Unknown;
}

core::ItemRef item_ref(const cJSON* o) {
    return {json_str(o, "item_id"), json_str(o, "provider"), json_str(o, "name"), json_str(o, "uri")};
}

// item_id may be a number on some servers.
std::string id_str(const cJSON* o, const char* key) {
    const cJSON* v = cJSON_GetObjectItemCaseSensitive(o, key);
    if (cJSON_IsNumber(v)) return std::to_string(static_cast<long long>(v->valuedouble));
    return cJSON_IsString(v) ? v->valuestring : "";
}

}  // namespace

core::MediaItem media_item(const cJSON* o) {
    core::MediaItem m;
    m.type = media_type(json_str(o, "media_type"));
    m.item_id = id_str(o, "item_id");
    m.provider = json_str(o, "provider");
    m.name = json_str(o, "name");
    m.version = json_str(o, "version");
    m.uri = json_str(o, "uri");
    m.favorite = json_bool(o, "favorite");
    m.editable = json_bool(o, "is_editable");
    m.duration_s = static_cast<uint32_t>(json_num(o, "duration"));

    // external_ids: list of [type, value] pairs (verified 2.10)
    const cJSON* ext;
    cJSON_ArrayForEach(ext, cJSON_GetObjectItemCaseSensitive(o, "external_ids")) {
        if (cJSON_GetArraySize(ext) == 2) {
            const cJSON* kind = cJSON_GetArrayItem(ext, 0);
            const cJSON* value = cJSON_GetArrayItem(ext, 1);
            if (cJSON_IsString(kind) && cJSON_IsString(value) && std::string(kind->valuestring) == "isrc")
                m.isrc = value->valuestring;
        }
    }
    const cJSON* pm;
    cJSON_ArrayForEach(pm, cJSON_GetObjectItemCaseSensitive(o, "provider_mappings")) {
        m.mappings.push_back({id_str(pm, "item_id"), json_str(pm, "provider_domain"), json_str(pm, "provider_instance"),
                              json_bool(pm, "available", true), json_bool(pm, "in_library")});
    }
    const cJSON* artist;
    cJSON_ArrayForEach(artist, cJSON_GetObjectItemCaseSensitive(o, "artists")) m.artists.push_back(item_ref(artist));
    const cJSON* album = cJSON_GetObjectItemCaseSensitive(o, "album");
    if (cJSON_IsObject(album)) m.album = item_ref(album);

    const cJSON* meta = cJSON_GetObjectItemCaseSensitive(o, "metadata");
    const cJSON* images = cJSON_GetObjectItemCaseSensitive(meta, "images");
    const cJSON* img = nullptr;
    const cJSON* candidate;
    cJSON_ArrayForEach(candidate, images) {  // prefer the cover ("thumb"), else the first image
        if (!img || json_str(candidate, "type") == "thumb") img = candidate;
        if (json_str(candidate, "type") == "thumb") break;
    }
    if (!img && cJSON_IsObject(cJSON_GetObjectItemCaseSensitive(o, "image"))) img = cJSON_GetObjectItemCaseSensitive(o, "image");
    if (img) {
        m.image = core::Image{json_str(img, "path"), json_str(img, "provider"), json_str(img, "proxy_id"),
                              json_bool(img, "remotely_accessible")};
    }
    return m;
}

std::vector<core::MediaItem> media_items(const cJSON* array) {
    std::vector<core::MediaItem> out;
    const cJSON* it;
    cJSON_ArrayForEach(it, array) out.push_back(media_item(it));
    return out;
}

core::SearchResult search_result(const cJSON* o) {
    core::SearchResult r;
    r.tracks = media_items(cJSON_GetObjectItemCaseSensitive(o, "tracks"));
    r.artists = media_items(cJSON_GetObjectItemCaseSensitive(o, "artists"));
    r.albums = media_items(cJSON_GetObjectItemCaseSensitive(o, "albums"));
    r.playlists = media_items(cJSON_GetObjectItemCaseSensitive(o, "playlists"));
    r.radio = media_items(cJSON_GetObjectItemCaseSensitive(o, "radio"));
    return r;
}

core::PlayerState player(const cJSON* o) {
    core::PlayerState p;
    p.player_id = json_str(o, "player_id");
    p.name = json_str(o, "name");
    p.available = json_bool(o, "available", true);
    p.volume_level = static_cast<int>(json_num(o, "volume_level"));
    p.group_volume = static_cast<int>(json_num(o, "group_volume", p.volume_level));
    std::string state = json_str(o, "playback_state");
    p.state = state == "playing" ? core::PlaybackState::Playing
            : state == "paused"  ? core::PlaybackState::Paused
            : state == "idle"    ? core::PlaybackState::Idle
                                 : core::PlaybackState::Unknown;
    const cJSON* f;
    cJSON_ArrayForEach(f, cJSON_GetObjectItemCaseSensitive(o, "supported_features")) {
        if (!cJSON_IsString(f)) continue;
        std::string s = f->valuestring;
        if (s == "seek") p.can_seek = true;
        else if (s == "next_previous") p.can_next_previous = true;
        else if (s == "volume_set") p.can_set_volume = true;
        else if (s == "pause") p.can_pause = true;
    }
    const cJSON* member;
    cJSON_ArrayForEach(member, cJSON_GetObjectItemCaseSensitive(o, "group_members")) {
        if (cJSON_IsString(member)) p.group_members.emplace_back(member->valuestring);
    }
    const cJSON* media = cJSON_GetObjectItemCaseSensitive(o, "current_media");
    if (cJSON_IsObject(media)) {
        p.media_title = json_str(media, "title");
        p.media_artist = json_str(media, "artist");
        p.media_image_url = json_str(media, "image_url");
    }
    return p;
}

core::QueueItem queue_item(const cJSON* o) {
    core::QueueItem q;
    q.queue_item_id = json_str(o, "queue_item_id");
    q.name = json_str(o, "name");
    q.duration_s = static_cast<uint32_t>(json_num(o, "duration"));
    q.available = json_bool(o, "available", true);
    const cJSON* media = cJSON_GetObjectItemCaseSensitive(o, "media_item");
    if (cJSON_IsObject(media)) q.media = media_item(media);
    return q;
}

void queue_state(const cJSON* o, core::QueueState& q) {
    q.queue_id = json_str(o, "queue_id");
    q.current_index = static_cast<int>(json_num(o, "current_index", -1));
    q.shuffle = json_bool(o, "shuffle_enabled");
    std::string repeat = json_str(o, "repeat_mode");
    q.repeat = repeat == "one" ? core::RepeatMode::One : repeat == "all" ? core::RepeatMode::All : core::RepeatMode::Off;
    q.elapsed_s = static_cast<uint32_t>(json_num(o, "elapsed_time"));
    const cJSON* current = cJSON_GetObjectItemCaseSensitive(o, "current_item");
    if (cJSON_IsObject(current)) q.current = queue_item(current);
    else q.current.reset();
}

}  // namespace ma::parse
