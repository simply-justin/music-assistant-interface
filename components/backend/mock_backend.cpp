#include "backend/mock_backend.hpp"

#include <algorithm>
#include <string>

#include "core/text.hpp"

namespace backend {

namespace {

// Same recordings on several providers on purpose, so dedupe has work to do.
constexpr const char* kCatalog = R"json([
 {"media_type":"track","item_id":"12","provider":"library","name":"Viva La Vida","uri":"library://track/12","duration":242,"favorite":true,
  "external_ids":[["isrc","GBAYE0800265"]],"artists":[{"item_id":"3","provider":"library","name":"Coldplay","uri":"library://artist/3"}],
  "provider_mappings":[{"item_id":"1mea3bSkSGXuIRvnydlB5b","provider_domain":"spotify","provider_instance":"spotify--a1","available":true,"in_library":true}]},
 {"media_type":"track","item_id":"1mea3bSkSGXuIRvnydlB5b","provider":"spotify--a1","name":"Viva La Vida","uri":"spotify://track/1mea3bSkSGXuIRvnydlB5b","duration":242,
  "artists":[{"item_id":"4gzpq5DPGxSnKTe4SA8HAU","provider":"spotify--a1","name":"Coldplay","uri":"spotify://artist/4gzpq5DPGxSnKTe4SA8HAU"}],
  "provider_mappings":[{"item_id":"1mea3bSkSGXuIRvnydlB5b","provider_domain":"spotify","provider_instance":"spotify--a1","available":true}]},
 {"media_type":"track","item_id":"dvb_iF2P8o4","provider":"ytmusic--b2","name":"Viva La Vida","uri":"ytmusic://track/dvb_iF2P8o4","duration":243,
  "artists":[{"item_id":"UCDPM_n1atn2ijUwHd0NNRQw","provider":"ytmusic--b2","name":"Coldplay","uri":"ytmusic://artist/UCDPM_n1atn2ijUwHd0NNRQw"}],
  "provider_mappings":[{"item_id":"dvb_iF2P8o4","provider_domain":"ytmusic","provider_instance":"ytmusic--b2","available":true}]},
 {"media_type":"track","item_id":"live1","provider":"ytmusic--b2","name":"Viva La Vida (Live in Buenos Aires)","uri":"ytmusic://track/live1","duration":260,
  "artists":[{"item_id":"x","provider":"ytmusic--b2","name":"Coldplay","uri":"ytmusic://artist/x"}],
  "provider_mappings":[{"item_id":"live1","provider_domain":"ytmusic","provider_instance":"ytmusic--b2","available":true}]},
 {"media_type":"track","item_id":"7","provider":"spotify--a1","name":"Yellow","uri":"spotify://track/7","duration":269,
  "artists":[{"item_id":"4gzpq5DPGxSnKTe4SA8HAU","provider":"spotify--a1","name":"Coldplay","uri":"spotify://artist/4gzpq5DPGxSnKTe4SA8HAU"}],
  "provider_mappings":[{"item_id":"7","provider_domain":"spotify","provider_instance":"spotify--a1","available":true}]},
 {"media_type":"artist","item_id":"3","provider":"library","name":"Coldplay","uri":"library://artist/3",
  "provider_mappings":[{"item_id":"4gzpq5DPGxSnKTe4SA8HAU","provider_domain":"spotify","provider_instance":"spotify--a1","available":true,"in_library":true}]},
 {"media_type":"artist","item_id":"UCDPM_n1atn2ijUwHd0NNRQw","provider":"ytmusic--b2","name":"Coldplay","uri":"ytmusic://artist/UCDPM_n1atn2ijUwHd0NNRQw",
  "provider_mappings":[{"item_id":"UCDPM_n1atn2ijUwHd0NNRQw","provider_domain":"ytmusic","provider_instance":"ytmusic--b2","available":true}]},
 {"media_type":"playlist","item_id":"p1","provider":"spotify--a1","name":"Coldplay Essentials","uri":"spotify://playlist/p1",
  "provider_mappings":[{"item_id":"p1","provider_domain":"spotify","provider_instance":"spotify--a1","available":true}]},
 {"media_type":"album","item_id":"a1","provider":"library","name":"Parachutes","uri":"library://album/a1","favorite":true,
  "artists":[{"item_id":"3","provider":"library","name":"Coldplay","uri":"library://artist/3"}]},
 {"media_type":"album","item_id":"a2","provider":"library","name":"Currents","uri":"library://album/a2",
  "artists":[{"item_id":"9","provider":"library","name":"Tame Impala","uri":"library://artist/9"}]},
 {"media_type":"radio","item_id":"r2","provider":"library","name":"Radio Paradise","uri":"library://radio/r2","favorite":true},
 {"media_type":"radio","item_id":"r3","provider":"library","name":"NPO Radio 2","uri":"library://radio/r3","favorite":true},
 {"media_type":"playlist","item_id":"p2","provider":"library","name":"Friday Night Mix","uri":"library://playlist/p2","favorite":true,"is_editable":true},
 {"media_type":"track","item_id":"t5","provider":"library","name":"Let It Happen","uri":"library://track/t5","duration":467,
  "artists":[{"item_id":"9","provider":"library","name":"Tame Impala","uri":"library://artist/9"}]},
 {"media_type":"radio","item_id":"r1","provider":"tunein--c3","name":"Radio Coldplay","uri":"tunein://radio/r1",
  "provider_mappings":[{"item_id":"r1","provider_domain":"tunein","provider_instance":"tunein--c3","available":true}]}
])json";

constexpr const char* kPlayers = R"json([
 {"player_id":"living","name":"Living Room","playback_state":"playing","volume_level":42,"available":true,
  "supported_features":["pause","seek","next_previous","volume_set","set_members"],"can_group_with":["kitchen"],"group_members":[]},
 {"player_id":"kitchen","name":"Kitchen","playback_state":"idle","volume_level":30,"available":true,
  "supported_features":["pause","volume_set","set_members"],"can_group_with":["living"],"group_members":[]}
])json";

const char* list_key(const std::string& media_type) {
    if (media_type == "track") return "tracks";
    if (media_type == "artist") return "artists";
    if (media_type == "album") return "albums";
    if (media_type == "playlist") return "playlists";
    return "radio";
}

bool matches(const cJSON* item, const std::string& query) {
    std::string q = core::normalize(query);
    std::string text = core::normalize(json_str(item, "name"));
    const cJSON* a;
    cJSON_ArrayForEach(a, cJSON_GetObjectItemCaseSensitive(item, "artists")) text += " " + core::normalize(json_str(a, "name"));
    return text.find(q) != std::string::npos;
}

cJSON* find_uri(const cJSON* list, const std::string& uri) {
    cJSON* it;
    cJSON_ArrayForEach(it, list) if (json_str(it, "uri") == uri) return it;
    return nullptr;
}

}  // namespace

MockBackend::MockBackend(MockLatency latency)
    : latency_(latency), catalog_(cJSON_Parse(kCatalog)), players_(cJSON_Parse(kPlayers)), queue_(cJSON_CreateArray()) {
    // Something is already playing, so every screen has content.
    for (const char* uri : {"library://track/12", "spotify://track/7"}) add_to_queue(uri);
}

void MockBackend::add_to_queue(const std::string& uri) {
    const cJSON* media = find_uri(catalog_.get(), uri);
    if (!media) return;
    cJSON* qi = cJSON_CreateObject();
    cJSON_AddStringToObject(qi, "queue_item_id", ("q" + std::to_string(next_queue_item_++)).c_str());
    cJSON_AddStringToObject(qi, "name", json_str(media, "name").c_str());
    cJSON_AddNumberToObject(qi, "duration", json_num(media, "duration"));
    cJSON_AddItemToObject(qi, "media_item", cJSON_Duplicate(media, true));
    cJSON_AddItemToArray(queue_.get(), qi);
}

Json MockBackend::queue_object(const std::string& queue_id) const {
    Json q(cJSON_CreateObject());
    cJSON_AddStringToObject(q.get(), "queue_id", queue_id.c_str());
    bool empty = cJSON_GetArraySize(queue_.get()) == 0;
    cJSON_AddNumberToObject(q.get(), "current_index", empty ? -1 : 0);
    cJSON_AddBoolToObject(q.get(), "shuffle_enabled", false);
    cJSON_AddStringToObject(q.get(), "repeat_mode", "off");
    cJSON_AddNumberToObject(q.get(), "elapsed_time", empty ? 0 : 92);
    if (!empty) cJSON_AddItemToObject(q.get(), "current_item", cJSON_Duplicate(cJSON_GetArrayItem(queue_.get(), 0), true));
    return q;
}

void MockBackend::later(uint32_t delay_ms, std::function<void()> fn) {
    deliveries_.push_back({now_ms_ + delay_ms, std::move(fn)});
}

void MockBackend::tick(uint32_t now_ms) {
    now_ms_ = now_ms;
    // Deliveries may schedule new ones, so take the due ones out first.
    std::deque<Delivery> due;
    for (auto it = deliveries_.begin(); it != deliveries_.end();) {
        if (static_cast<int32_t>(now_ms - it->due_ms) >= 0) {
            due.push_back(std::move(*it));
            it = deliveries_.erase(it);
        } else {
            ++it;
        }
    }
    for (auto& d : due) d.fn();
}

void MockBackend::start() {
    if (url_.empty()) return;
    later(latency_.command_ms, [this] {
        if (!listener_.on_connection) return;
        ServerInfo info{"mock", "mock", 65};
        if (url_.find("bad") != std::string::npos) return listener_.on_connection(Connection::Disconnected, {});
        listener_.on_connection(Connection::Connecting, info);
        Connection c = token_.empty() ? Connection::AuthRequired : token_ == "bad" ? Connection::AuthFailed : Connection::Connected;
        listener_.on_connection(c, info);
    });
}

void MockBackend::configure(const std::string& url, const std::string& token) {
    url_ = url;
    token_ = token;
    start();
}

void MockBackend::emit(const char* event, const std::string& object_id, Json data) {
    auto shared = std::make_shared<Json>(std::move(data));
    later(latency_.command_ms, [this, event, object_id, shared] {
        if (listener_.on_event) listener_.on_event({event, object_id, std::move(*shared)});
    });
}

void MockBackend::request(Request req, Done done) {
    bool library_search = false;
    if (req.command == "music/search") {
        const cJSON* providers = cJSON_GetObjectItemCaseSensitive(req.args.get(), "providers");
        library_search = cJSON_GetArraySize(providers) > 0;
    }
    uint32_t delay = req.command != "music/search" ? latency_.command_ms
                   : library_search                 ? latency_.library_ms
                                                    : latency_.full_ms;
    auto response = std::make_shared<Response>(handle(req));
    later(delay, [response, done = std::move(done)] {
        if (done) done(std::move(*response));
    });
}

Response MockBackend::handle(const Request& req) {
    const cJSON* args = req.args.get();
    Response r;
    r.ok = true;
    const std::string& c = req.command;

    if (c == "music/search") {
        std::string query = json_str(args, "search_query");
        bool library_only = cJSON_GetArraySize(cJSON_GetObjectItemCaseSensitive(args, "providers")) > 0;
        r.result.reset(cJSON_CreateObject());
        for (const char* k : {"tracks", "artists", "albums", "playlists", "radio"}) cJSON_AddArrayToObject(r.result.get(), k);
        const cJSON* item;
        cJSON_ArrayForEach(item, catalog_.get()) {
            if (library_only && json_str(item, "provider") != "library") continue;
            if (!matches(item, query)) continue;
            cJSON* list = cJSON_GetObjectItemCaseSensitive(r.result.get(), list_key(json_str(item, "media_type")));
            cJSON_AddItemToArray(list, cJSON_Duplicate(item, true));
        }
    } else if (c == "music/playlists/library_items") {
        r.result.reset(cJSON_Parse(R"json([
          {"media_type":"playlist","item_id":"1","provider":"library","name":"Friday Night Mix","uri":"library://playlist/1","is_editable":true},
          {"media_type":"playlist","item_id":"2","provider":"library","name":"Discover Weekly","uri":"library://playlist/2","is_editable":false}
        ])json"));
    } else if (c == "music/playlists/create_playlist") {
        r.result.reset(cJSON_CreateObject());
        cJSON_AddStringToObject(r.result.get(), "media_type", "playlist");
        cJSON_AddStringToObject(r.result.get(), "item_id", "3");
        cJSON_AddStringToObject(r.result.get(), "provider", "library");
        cJSON_AddStringToObject(r.result.get(), "name", json_str(args, "name").c_str());
        cJSON_AddBoolToObject(r.result.get(), "is_editable", true);
    } else if (c.rfind("music/", 0) == 0 && c.size() > 14 && c.compare(c.size() - 14, 14, "/library_items") == 0) {
        // music/<type>s/library_items: catalog items of that type that are in the library (or favourites)
        std::string type = c.substr(6, c.find('/', 6) - 6);
        if (type == "radios") type = "radio";
        else type.pop_back();  // tracks -> track
        bool favourites = json_bool(args, "favorite");
        r.result.reset(cJSON_CreateArray());
        const cJSON* item;
        cJSON_ArrayForEach(item, catalog_.get()) {
            if (json_str(item, "media_type") != type) continue;
            if (favourites ? !json_bool(item, "favorite") : json_str(item, "provider") != "library") continue;
            cJSON_AddItemToArray(r.result.get(), cJSON_Duplicate(item, true));
        }
    } else if (c == "music/recently_played_items" || c == "music/playlists/playlist_tracks" ||
               c == "music/albums/album_tracks" || c == "music/artists/artist_tracks") {
        // Playlists are long (~400 songs) so the windowed lists get exercised.
        int copies = c == "music/playlists/playlist_tracks" ? 60 : 1;
        r.result.reset(cJSON_CreateArray());
        for (int n = 0; n < copies; ++n) {
            const cJSON* item;
            cJSON_ArrayForEach(item, catalog_.get()) {
                if (json_str(item, "media_type") != "track") continue;
                cJSON* copy = cJSON_Duplicate(item, true);
                if (n) {
                    std::string uri = json_str(item, "uri") + "#" + std::to_string(n);
                    cJSON_ReplaceItemInObjectCaseSensitive(copy, "uri", cJSON_CreateString(uri.c_str()));
                }
                cJSON_AddItemToArray(r.result.get(), copy);
            }
        }
    } else if (c == "providers") {
        r.result.reset(cJSON_Parse(R"json([
          {"instance_id":"builtin","domain":"builtin","name":"Music Assistant","available":true,"is_streaming_provider":false},
          {"instance_id":"spotify--a1","domain":"spotify","name":"Spotify","available":true,"is_streaming_provider":true},
          {"instance_id":"ytmusic--b2","domain":"ytmusic","name":"YouTube Music","available":false,"is_streaming_provider":true},
          {"instance_id":"tunein--c3","domain":"tunein","name":"TuneIn","available":true,"is_streaming_provider":true}
        ])json"));
    } else if (c == "player_queues/get_active_queue") {
        r.result = queue_object(json_str(args, "player_id"));
    } else if (c == "players/all") {
        r.result.reset(cJSON_Duplicate(players_.get(), true));
    } else if (c == "player_queues/items") {
        r.result.reset(cJSON_Duplicate(queue_.get(), true));
    } else if (c == "player_queues/play_media") {
        std::string option = json_str(args, "option");
        if (option != "add" && option != "next") queue_.reset(cJSON_CreateArray());
        const cJSON* uri;
        cJSON_ArrayForEach(uri, cJSON_GetObjectItemCaseSensitive(args, "media")) add_to_queue(uri->valuestring);
        emit("queue_items_updated", json_str(args, "queue_id"), nullptr);
        emit("queue_updated", json_str(args, "queue_id"), queue_object(json_str(args, "queue_id")));
    } else if (c == "player_queues/clear") {
        // Like MA 2.10.13: everything goes, including what's playing.
        queue_.reset(cJSON_CreateArray());
        emit("queue_items_updated", json_str(args, "queue_id"), nullptr);
    } else if (c == "player_queues/delete_item") {
        std::string id = json_str(args, "item_id_or_index");
        int i = 0;
        const cJSON* it;
        cJSON_ArrayForEach(it, queue_.get()) {
            if (json_str(it, "queue_item_id") == id) break;
            ++i;
        }
        cJSON_DeleteItemFromArray(queue_.get(), i);
        emit("queue_items_updated", json_str(args, "queue_id"), nullptr);
    } else if (c == "players/cmd/set_members" || c == "players/cmd/ungroup") {
        auto by_id = [this](const std::string& id) -> cJSON* {
            cJSON* p;
            cJSON_ArrayForEach(p, players_.get()) if (json_str(p, "player_id") == id) return p;
            return nullptr;
        };
        auto set_list = [](cJSON* p, const char* key, const std::vector<std::string>& ids) {
            cJSON* arr = cJSON_CreateArray();
            for (const auto& id : ids) cJSON_AddItemToArray(arr, cJSON_CreateString(id.c_str()));
            cJSON_ReplaceItemInObjectCaseSensitive(p, key, arr);
        };
        auto set_synced = [](cJSON* p, const std::string& to) {
            if (!cJSON_GetObjectItemCaseSensitive(p, "synced_to")) cJSON_AddNullToObject(p, "synced_to");
            cJSON_ReplaceItemInObjectCaseSensitive(p, "synced_to", to.empty() ? cJSON_CreateNull() : cJSON_CreateString(to.c_str()));
        };
        if (c == "players/cmd/ungroup") {
            std::string id = json_str(args, "player_id");
            cJSON* p;
            cJSON_ArrayForEach(p, players_.get()) {
                std::string pid = json_str(p, "player_id");
                if (pid == id || json_str(p, "synced_to") == id) set_synced(p, "");
                if (pid == id) set_list(p, "group_members", {});
            }
        } else {
            std::string leader = json_str(args, "target_player");
            cJSON* lp = by_id(leader);
            std::vector<std::string> members;
            const cJSON* m;
            cJSON_ArrayForEach(m, cJSON_GetObjectItemCaseSensitive(lp, "group_members")) members.emplace_back(m->valuestring);
            if (members.empty()) members.push_back(leader);
            cJSON_ArrayForEach(m, cJSON_GetObjectItemCaseSensitive(args, "player_ids_to_add")) {
                members.emplace_back(m->valuestring);
                if (cJSON* mp = by_id(m->valuestring)) set_synced(mp, leader);
            }
            cJSON_ArrayForEach(m, cJSON_GetObjectItemCaseSensitive(args, "player_ids_to_remove")) {
                members.erase(std::remove(members.begin(), members.end(), std::string(m->valuestring)), members.end());
                if (cJSON* mp = by_id(m->valuestring)) set_synced(mp, "");
            }
            if (members.size() == 1) members.clear();
            set_list(lp, "group_members", members);
        }
        cJSON* p;
        cJSON_ArrayForEach(p, players_.get()) emit("player_updated", json_str(p, "player_id"), Json(cJSON_Duplicate(p, true)));
    } else if (c == "players/cmd/volume_set") {
        std::string id = json_str(args, "player_id");
        cJSON* p;
        cJSON_ArrayForEach(p, players_.get()) {
            if (json_str(p, "player_id") != id) continue;
            cJSON_ReplaceItemInObjectCaseSensitive(p, "volume_level", cJSON_CreateNumber(json_num(args, "volume_level")));
            emit("player_updated", id, Json(cJSON_Duplicate(p, true)));
        }
    }
    // Other commands: accepted, nothing to simulate.
    return r;
}

}  // namespace backend
