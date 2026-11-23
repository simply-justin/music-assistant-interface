// Host tests for the portable layers. Tiny on purpose: CHECK + one function per area.
#include <cstdio>
#include <string>
#include <vector>

#include "app/app.hpp"
#include "backend/backoff.hpp"
#include "backend/ma_commands.hpp"
#include "backend/ma_session.hpp"
#include "backend/mock_backend.hpp"
#include "core/dedupe.hpp"
#include "core/repeat_guard.hpp"
#include "core/search.hpp"
#include "core/text.hpp"
#include "img/artwork.hpp"
#include "i18n/i18n.hpp"
#include "fake_platform.hpp"
#include "text.hpp"

static int g_failed = 0;
#define CHECK(cond)                                                    \
    do {                                                               \
        if (!(cond)) {                                                 \
            std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); \
            ++g_failed;                                                \
        }                                                              \
    } while (0)

using namespace core;

static MediaItem track(std::string provider, std::string id, std::string name, std::string artist, uint32_t dur,
                       std::string isrc = "") {
    MediaItem m;
    m.type = MediaType::Track;
    m.provider = provider;
    m.item_id = id;
    m.name = name;
    m.uri = provider + "://track/" + id;
    m.duration_s = dur;
    m.isrc = isrc;
    m.artists.push_back({"a", provider, artist, ""});
    m.mappings.push_back({id, provider, provider + "--1", true, provider == "library"});
    return m;
}

static MediaItem named(MediaType type, std::string provider, std::string id, std::string name) {
    MediaItem m;
    m.type = type;
    m.provider = provider;
    m.item_id = id;
    m.name = name;
    m.uri = provider + "://x/" + id;
    return m;
}

static void test_text() {
    auto t = split_title("Song (2011 Remaster) [feat. Someone]");
    CHECK(t.base == "song");
    CHECK((t.markers == std::vector<std::string>{"2011", "remaster"}));
    CHECK(split_title("Song - Remastered 2011").markers == t.markers);
    CHECK(split_title("Song", "Radio Edit").markers == std::vector<std::string>{"edit"});
    CHECK(split_title("Song (Special Edition)").markers != split_title("Song (Edit)").markers);
    CHECK(split_title("Song (Live at A)").markers != split_title("Song (Live at B)").markers);
    CHECK(split_title("Song (Taylor's Version)").markers != split_title("Song").markers);
    CHECK(normalize("  Hello,   WORLD! ") == "hello world");
}

static void test_dedupe() {
    // ISRC equal merges across providers; different ISRC never does.
    CHECK(same_item(track("spotify", "1", "A", "X", 200, "I1"), track("ytmusic", "2", "A", "X", 200, "I1")));
    CHECK(!same_item(track("spotify", "1", "A", "X", 200, "I1"), track("ytmusic", "2", "A", "X", 200, "I2")));
    // Heuristic: needs both durations within 2 s and matching markers.
    CHECK(same_item(track("spotify", "1", "A", "X", 200), track("ytmusic", "2", "A", "x", 202)));
    CHECK(!same_item(track("spotify", "1", "A", "X", 200), track("ytmusic", "2", "A", "X", 203)));
    CHECK(!same_item(track("spotify", "1", "A", "X", 0), track("ytmusic", "2", "A", "X", 0)));
    CHECK(!same_item(track("spotify", "1", "A", "X", 200), track("ytmusic", "2", "A (Live)", "X", 200)));
    CHECK(!same_item(track("spotify", "1", "A", "X", 200), track("ytmusic", "2", "A", "Y", 200)));
    // Playlists and radio never merge by name.
    CHECK(!same_item(named(MediaType::Playlist, "spotify", "1", "Mix"), named(MediaType::Playlist, "ytmusic", "2", "Mix")));
    CHECK(!same_item(named(MediaType::Radio, "tunein", "1", "Joe"), named(MediaType::Radio, "radiobrowser", "2", "Joe")));

    // Library takes over the row, position is stable, others kept for "Play from".
    std::vector<MediaItem> shown;
    merge_into(shown, {track("spotify", "1", "A", "X", 200), track("spotify", "9", "B", "X", 100)}, {});
    merge_into(shown, {track("library", "5", "A", "X", 201)}, {});
    CHECK(shown.size() == 2);
    CHECK(shown[0].provider == "library");
    CHECK(shown[0].alternatives.size() == 1 && shown[0].alternatives[0].provider == "spotify");
    CHECK(shown[1].name == "B");
    // Exact same uri is dropped, not kept as an alternative.
    merge_into(shown, {track("spotify", "1", "A", "X", 200)}, {});
    CHECK(shown[0].alternatives.size() == 1);
    // Preferred-source order decides between two non-library sources.
    std::vector<MediaItem> pref;
    merge_into(pref, {track("spotify", "1", "A", "X", 200)}, {{"ytmusic", "spotify"}});
    merge_into(pref, {track("ytmusic", "2", "A", "X", 200)}, {{"ytmusic", "spotify"}});
    CHECK(pref[0].provider == "ytmusic");

    // Artists: merge by name only when unambiguous in both batches.
    std::vector<MediaItem> artists;
    merge_into(artists, {named(MediaType::Artist, "spotify", "1", "Nirvana")}, {});
    merge_into(artists, {named(MediaType::Artist, "ytmusic", "2", "Nirvana")}, {});
    CHECK(artists.size() == 1);
    std::vector<MediaItem> ambiguous;
    merge_into(ambiguous, {named(MediaType::Artist, "spotify", "1", "Nirvana")}, {});
    merge_into(ambiguous, {named(MediaType::Artist, "ytmusic", "2", "Nirvana"), named(MediaType::Artist, "ytmusic", "3", "Nirvana")}, {});
    CHECK(ambiguous.size() == 3);
}

static void test_search_session() {
    SearchSession s;
    s.set_query("cold", 0);
    CHECK(s.poll(100).requests.empty());  // debounced
    s.set_query("coldplay", 200);
    CHECK(s.poll(300).requests.empty());
    auto p = s.poll(450);
    CHECK(p.requests.size() == 2 && p.requests[0].stage == SearchStage::Library);
    uint32_t gen = p.requests[0].generation;

    SearchResult lib;
    lib.tracks.push_back(track("library", "1", "Yellow", "Coldplay", 269));
    CHECK(s.on_result(gen, SearchStage::Library, lib, {}, 500));
    CHECK(s.view().status == SearchStatus::Partial);

    // A newer query makes the old generation stale.
    s.set_query("coldplay yellow", 520);
    auto p2 = s.poll(800);
    CHECK(p2.requests.size() == 2);
    CHECK(!s.on_result(gen, SearchStage::Full, lib, {}, 810));  // dropped
    uint32_t gen2 = p2.requests[0].generation;
    CHECK(s.on_result(gen2, SearchStage::Full, lib, {}, 900));
    CHECK(s.on_result(gen2, SearchStage::Library, lib, {}, 950));
    CHECK(s.view().status == SearchStatus::Done && s.view().result.tracks.size() == 1);
    CHECK(s.view().result.top && s.view().result.top->name == "Yellow");

    // Cached: no new requests for a repeat query.
    s.set_query("", 1000);
    CHECK(s.view().status == SearchStatus::Idle);
    s.set_query("Coldplay  Yellow", 1100);
    auto p3 = s.poll(1400);
    CHECK(p3.requests.empty() && p3.view_changed && s.view().status == SearchStatus::Done);

    // In-flight cap: the latest query waits until a slot frees up.
    SearchSession busy;
    busy.set_query("a", 0);
    auto b1 = busy.poll(300);
    busy.set_query("ab", 300);
    auto b2 = busy.poll(600);
    busy.set_query("abc", 600);
    CHECK(busy.poll(900).requests.empty());
    busy.on_error(b1.requests[0].generation, SearchStage::Library);
    busy.on_error(b1.requests[1].generation, SearchStage::Full);
    CHECK(busy.poll(910).requests.size() == 2);
    (void)b2;
}

static void test_ma_session() {
    std::vector<std::string> sent;
    ma::Session s([&](const std::string& t) { sent.push_back(t); return true; }, "secret");
    std::vector<backend::Connection> states;
    s.set_listener({[](backend::Event&&) {}, [&](backend::Connection c, const backend::ServerInfo&) { states.push_back(c); }});

    // Before the socket is up: fails at once. After it opened but before auth: waits.
    std::string early;
    s.request(ma::cmd::players_all(), [&](backend::Response&& r) { early = r.error_code; });
    CHECK(early == "disconnected" && sent.empty());
    s.on_open();
    bool waited = false;
    s.request(ma::cmd::players_all(), [&](backend::Response&& r) { waited = r.ok; });
    CHECK(sent.empty());
    s.on_text(R"({"server_id":"x","server_version":"2.7.0","schema_version":28})");
    CHECK(sent.size() == 1 && sent[0].find("\"command\":\"auth\"") != std::string::npos);
    CHECK(sent[0].find("\"token\":\"secret\"") != std::string::npos);
    s.on_text(R"({"message_id":"1","result":{}})");
    CHECK(s.connection() == backend::Connection::Connected);
    CHECK(sent.size() == 2 && sent[1].find("players/all") != std::string::npos);  // flushed after auth
    s.on_text(R"({"message_id":"2","result":[]})");
    CHECK(waited);
    CHECK(s.server_info().server_version == "2.7.0");

    // Partial results are joined; errors are reported; unknown ids ignored.
    int total = -1;
    s.request(ma::cmd::players_all(), [&](backend::Response&& r) { total = cJSON_GetArraySize(r.result.get()); });
    s.on_text(R"({"message_id":"3","result":[1,2],"partial":true})");
    s.on_text(R"({"message_id":"3","result":[3]})");
    CHECK(total == 3);
    std::string err;
    s.request(ma::cmd::players_all(), [&](backend::Response&& r) { err = r.error_code; });
    s.on_text(R"({"message_id":"4","error_code":"invalid_command","details":"nope"})");
    CHECK(err == "invalid_command");
    s.on_text(R"({"message_id":"99","result":1})");
    s.on_text("not json");

    // Disconnect fails pending requests.
    std::string lost;
    s.request(ma::cmd::players_all(), [&](backend::Response&& r) { lost = r.error_code; });
    s.on_closed();
    CHECK(lost == "disconnected");
    CHECK(states.back() == backend::Connection::Disconnected);

    // Rejected token.
    ma::Session bad([](const std::string&) { return true; }, "wrong");
    bad.on_open();
    bad.on_text(R"({"server_version":"2.7.0","schema_version":28})");
    bad.on_text(R"({"message_id":"1","error_code":"unauthorized","details":""})");
    CHECK(bad.connection() == backend::Connection::AuthFailed);

    // A different server at the same address never receives the token.
    std::vector<std::string> out;
    ma::Session other([&](const std::string& t) { out.push_back(t); return true; }, "secret", "abc");
    other.on_open();
    other.on_text(R"({"server_id":"evil","server_version":"2.10.13","schema_version":65})");
    CHECK(out.empty() && other.connection() == backend::Connection::WrongServer);
    // Too old: before mandatory auth.
    ma::Session old([](const std::string&) { return true; }, "secret");
    old.on_open();
    old.on_text(R"({"server_id":"x","server_version":"2.0.0","schema_version":25})");
    CHECK(old.connection() == backend::Connection::IncompatibleServer);
}

static void test_parse() {
    backend::Json j(cJSON_Parse(R"({"media_type":"track","item_id":5,"provider":"library","name":"Yellow","uri":"library://track/5",
      "duration":269,"favorite":true,"external_ids":[["isrc","GBAYE0000267"]],
      "artists":[{"item_id":"1","provider":"library","name":"Coldplay","uri":"library://artist/1"}],
      "provider_mappings":[{"item_id":"7","provider_domain":"spotify","provider_instance":"spotify--a","available":true,"in_library":true}],
      "metadata":{"images":[{"path":"http://x/y.jpg","provider":"spotify","remotely_accessible":true}]}})"));
    MediaItem m = ma::parse::media_item(j.get());
    CHECK(m.type == MediaType::Track && m.item_id == "5" && m.isrc == "GBAYE0000267");
    CHECK(m.duration_s == 269 && m.favorite && m.in_library());
    CHECK(m.artists.size() == 1 && m.artists[0].name == "Coldplay");
    CHECK(m.badge_provider() == "spotify");
    CHECK(m.image && m.image->remotely_accessible);

    backend::Json pj(cJSON_Parse(R"({"player_id":"b8:27","name":"Cafe","available":true,"playback_state":"paused",
      "volume_level":40,"group_volume":55,"group_members":["a","b"],"supported_features":["pause","volume_set"]})"));
    PlayerState p = ma::parse::player(pj.get());
    CHECK(p.name == "Cafe" && p.state == PlaybackState::Paused && p.group_volume == 55);
    CHECK(p.group_members.size() == 2 && p.can_pause && p.can_set_volume && !p.can_seek);
    auto clear = ma::cmd::queue_clear("q");
    CHECK(cJSON_IsTrue(cJSON_GetObjectItem(clear.args.get(), "skip_stop")));

    auto req = ma::cmd::search("x", SearchStage::Library, 10);
    CHECK(req.command == "music/search");
    CHECK(cJSON_GetArraySize(cJSON_GetObjectItem(req.args.get(), "providers")) == 1);
    CHECK(!cJSON_GetObjectItem(ma::cmd::search("x", SearchStage::Full, 10).args.get(), "providers"));
    auto rm = ma::cmd::playlist_remove_tracks("7", {0});  // first song → MA position 1
    CHECK(cJSON_GetArrayItem(cJSON_GetObjectItem(rm.args.get(), "positions_to_remove"), 0)->valueint == 1);
}

static void test_i18n() {
    i18n::Lang nl;
    CHECK(i18n::language_from_code("nl", nl));
    i18n::set_language(nl);
    CHECK(i18n::tr(i18n::Str::toast_added_to_queue, "Yellow") == "Yellow toegevoegd aan de wachtrij");
    i18n::set_language(i18n::Lang::en);
    CHECK(i18n::tr(i18n::Str::toast_added_to_queue, "Yellow") == "Added Yellow to the queue");
}

static void test_app_with_mock() {
    backend::MockBackend mock;
    app::MemorySettings settings;
    settings.set("player", "kitchen");
    std::vector<app::Update> updates;
    FakePlatform platform;
    app::App a(mock, settings, platform, [&](app::Update u) { updates.push_back(std::move(u)); });
    a.start();
    uint32_t now = 0;
    auto run_until = [&](uint32_t t) {
        for (; now <= t; now += 10) {
            platform.tick(now);
            a.tick(now);
        }
    };
    run_until(200);

    // Connected, saved player restored.
    std::shared_ptr<const app::PlayersView> players;
    for (auto& u : updates)
        if (auto* p = std::get_if<std::shared_ptr<const app::PlayersView>>(&u)) players = *p;
    CHECK(players && players->players.size() == 2 && players->selected_id == "kitchen");

    // Unified search: library first, then full; duplicates merged; live version kept apart.
    a.handle(app::cmd::Search{"viva"});
    run_until(1000);
    std::shared_ptr<const SearchView> view;
    for (auto& u : updates)
        if (auto* v = std::get_if<std::shared_ptr<const SearchView>>(&u)) view = *v;
    CHECK(view && view->status == SearchStatus::Done);
    CHECK(view && view->result.tracks.size() == 2);
    CHECK(view && view->result.tracks[0].provider == "library");
    CHECK(view && view->result.tracks[0].alternatives.size() == 2);

    // Add to queue, then again: "already in the queue" with an "Add again" action.
    MediaItem viva = view->result.tracks[0];
    a.handle(app::cmd::AddToQueue{viva});
    run_until(1200);
    updates.clear();
    a.handle(app::cmd::AddToQueue{viva});  // within 1.5 s: ignored as a double tap
    CHECK(updates.empty());
    run_until(3000);
    a.handle(app::cmd::AddToQueue{viva});
    const app::Toast* toast = nullptr;
    for (auto& u : updates)
        if (auto* t = std::get_if<app::Toast>(&u)) toast = t;
    CHECK(toast && toast->action && toast->text == "Viva La Vida is already in the queue");

    // Clear up next: the current song (index 0 in the mock) stays, the rest goes.
    a.handle(app::cmd::ClearUpNext{});
    run_until(3100);
    {
        std::shared_ptr<const core::QueueState> cq;
        for (auto& u : updates)
            if (auto* x = std::get_if<std::shared_ptr<const core::QueueState>>(&u)) cq = *x;
        CHECK(cq && cq->items.size() == 1 && cq->items[0].name == "Viva La Vida");
    }

    // Undo: adding a new song offers Undo, which removes exactly that queue item.
    core::MediaItem fresh;
    fresh.type = core::MediaType::Track;
    fresh.name = "Let It Happen";
    fresh.uri = "library://track/t5";
    updates.clear();
    a.handle(app::cmd::AddToQueue{fresh});
    run_until(3250);
    const app::Toast* undo = nullptr;
    for (auto& u : updates)
        if (auto* t = std::get_if<app::Toast>(&u)) undo = t;
    CHECK(undo && undo->action_label == "Undo" && std::holds_alternative<app::cmd::RemoveFromQueue>(*undo->action));
    std::shared_ptr<const core::QueueState> q;
    for (auto& u : updates)
        if (auto* x = std::get_if<std::shared_ptr<const core::QueueState>>(&u)) q = *x;
    size_t before = q ? q->items.size() : 0;
    if (undo) a.handle(*undo->action);
    run_until(3400);
    for (auto& u : updates)
        if (auto* x = std::get_if<std::shared_ptr<const core::QueueState>>(&u)) q = *x;
    CHECK(q && q->items.size() + 1 == before);

    // Grouping: Kitchen joins Living Room, then the group is dissolved.
    a.handle(app::cmd::SetGroup{"living", {"kitchen"}});
    run_until(3480);
    for (auto& u : updates)
        if (auto* p = std::get_if<std::shared_ptr<const app::PlayersView>>(&u)) players = *p;
    CHECK(players && players->players[0].group_members.size() == 2 && players->players[1].synced_to == "living");
    a.handle(app::cmd::Ungroup{"living"});
    run_until(3560);
    for (auto& u : updates)
        if (auto* p = std::get_if<std::shared_ptr<const app::PlayersView>>(&u)) players = *p;
    CHECK(players && players->players[0].group_members.empty() && players->players[1].synced_to.empty());

    // Volume is clamped and reflected.
    a.handle(app::cmd::SetVolume{"living", 140});
    run_until(3640);
    for (auto& u : updates)
        if (auto* p = std::get_if<std::shared_ptr<const app::PlayersView>>(&u)) players = *p;
    CHECK(players && players->players[0].volume_level == 100);

    // Playlists: list (editable flag parsed), add a song, create one.
    a.handle(app::cmd::LoadPlaylists{});
    run_until(3800);
    std::shared_ptr<const app::PlaylistsView> pls;
    for (auto& u : updates)
        if (auto* p = std::get_if<std::shared_ptr<const app::PlaylistsView>>(&u)) pls = *p;
    CHECK(pls && pls->playlists.size() == 2 && pls->playlists[0].editable && !pls->playlists[1].editable);
    if (!pls || pls->playlists.empty()) return;
    updates.clear();
    a.handle(app::cmd::AddToPlaylist{pls->playlists[0], viva});
    a.handle(app::cmd::CreatePlaylist{"Kitchen songs", viva});
    run_until(4200);
    std::vector<std::string> toasts;
    for (auto& u : updates)
        if (auto* t = std::get_if<app::Toast>(&u)) toasts.push_back(t->text);
    CHECK(toasts.size() == 2 && toasts[0] == "Added to Friday Night Mix" && toasts[1] == "Playlist Kitchen songs created");

    a.handle(app::cmd::SetLanguage{i18n::Lang::nl});
    CHECK(settings.get("lang") == "nl");
    i18n::set_language(i18n::Lang::en);
}

static void test_display_text() {
    CHECK(ui::clean("Daydreams💭😌") == "Daydreams");
    CHECK(ui::clean("My Childhood Mixtape👦🏽💽") == "My Childhood Mixtape");
    CHECK(ui::clean("A 🍻 Night") == "A Night");
    CHECK(ui::clean("Beyoncé – Déjà Vu") == "Beyoncé – Déjà Vu");  // accents and dashes stay
    CHECK(ui::duration(242) == "4:02" && ui::duration(3723) == "1:02:03" && ui::duration(0) == "0:00");
}

static void test_setup_flow() {
    backend::MockBackend mock;
    mock.set_unconfigured();
    app::MemorySettings settings;
    FakePlatform platform;
    platform.token = "";  // no phone: the user types the token
    std::shared_ptr<const app::SetupView> view;
    app::App a(mock, settings, platform, [&](app::Update u) {
        if (auto* v = std::get_if<std::shared_ptr<const app::SetupView>>(&u)) view = *v;
    });
    a.start();
    uint32_t now = 0;
    auto run = [&](uint32_t ms) {
        for (uint32_t end = now + ms; now <= end; now += 10) {
            platform.tick(now);
            a.tick(now);
        }
    };
    CHECK(view && view->step == app::SetupStep::Welcome);
    a.handle(app::cmd::SetupGo{app::SetupStep::Wifi});
    run(1000);
    CHECK(view->networks.size() == 4);
    a.handle(app::cmd::SetupJoinWifi{"HomeNetwork", "wrong"});
    run(1000);
    CHECK(view->wifi_error && *view->wifi_error == app::WifiResult::WrongPassword && view->step == app::SetupStep::Wifi);
    a.handle(app::cmd::SetupJoinWifi{"HomeNetwork", "music"});
    run(2600);
    CHECK(view->step == app::SetupStep::Server && view->servers.size() == 1);
    a.handle(app::cmd::SetupUseServer{"bad-host"});
    run(8200);
    CHECK(view->server_error && view->step == app::SetupStep::Server);
    a.handle(app::cmd::SetupUseServer{"192.168.2.80"});
    run(200);
    CHECK(view->step == app::SetupStep::SignIn && !view->pairing_url.empty());
    CHECK(settings.get("server_url") == "ws://192.168.2.80:8095/ws");
    a.handle(app::cmd::SetupToken{"bad"});
    run(200);
    CHECK(view->token_error && view->step == app::SetupStep::SignIn);
    a.handle(app::cmd::SetupToken{"good-token"});
    run(200);
    CHECK(view->step == app::SetupStep::Speaker && settings.get("token") == "good-token");
    a.handle(app::cmd::SetupFinish{});
    CHECK(view->step == app::SetupStep::Finished && settings.get("setup_done") == "1");
}

static void test_panel_brightness() {
    backend::MockBackend mock;
    app::MemorySettings settings;
    FakePlatform platform;
    app::App a(mock, settings, platform, [](app::Update) {});
    a.start();
    app::DisplaySettings d;
    d.night = false;
    d.brightness = 70;
    a.handle(app::cmd::SetDisplay{d});
    CHECK(platform.brightness == 70 && settings.get("brightness") == "70");
    a.handle(app::cmd::SetDimmed{true});
    CHECK(platform.brightness == 8);
    d.night = true;
    d.night_from = 0;
    d.night_to = 24 * 60;  // always night
    d.night_brightness = 5;
    a.handle(app::cmd::SetDimmed{false});
    a.handle(app::cmd::SetDisplay{d});
    CHECK(platform.brightness == 5);
    app::Appearance look{app::ThemeMode::Light, 3, app::Roundness::Round};
    a.handle(app::cmd::SetAppearance{look});
    CHECK(settings.get("theme") == "light" && settings.get("accent") == "3" && settings.get("round") == "2");
    a.handle(app::cmd::ResetPanel{});
    CHECK(platform.restarted && settings.get("brightness").empty());
}

static void test_artwork() {
    CHECK(img::proxy_size(52) == 80 && img::proxy_size(160) == 160 && img::proxy_size(224) == 256 && img::proxy_size(520) == 512 && img::proxy_size(600) == 1024);
    core::Image im{"p", "spotify", "abc", true};
    CHECK(img::proxy_url("http://h:8095", im, 52) == "http://h:8095/imageproxy/abc?size=80&fmt=jpeg");
    CHECK(img::proxy_url("http://h:8095", core::Image{}, 52).empty());
    // 4×4 red → 2×2, average red, corners rounded to the background.
    std::vector<uint8_t> red(4 * 4 * 3, 0);
    for (size_t i = 0; i < red.size(); i += 3) red[i] = 255;
    auto b = img::scale(red.data(), 4, 4, 2, 2, 0, 0);
    CHECK(b->w == 2 && b->rgb565[0] == 0xF800 && b->average == 0xFF0000);
    auto r = img::scale(red.data(), 4, 4, 16, 16, 6, 0x000000);
    CHECK(r->rgb565[0] == 0 && r->rgb565[15] == 0 && r->rgb565[8 * 16 + 8] == 0xF800);
    // Art background: bright saturated red → dark, muted red within L ≤ 16 %, S ≤ 45 %.
    uint32_t pale = img::art_background_light(0xFF0000);
    CHECK((pale >> 16 & 0xFF) >= 0xE5 && (pale & 0xFF) >= 0xD8 && (pale >> 16 & 0xFF) > (pale & 0xFF));  // pale, still reddish
    uint32_t dark = img::art_background(0xFF0000);
    CHECK((dark >> 16 & 0xFF) > (dark & 0xFF) && (dark >> 16 & 0xFF) <= 0x3B && img::art_background(0x101010) == 0x101010);
    // LRU: over budget evicts the oldest unpinned entry.
    img::Cache cache(3 * (b->bytes()));
    cache.put("a", b);
    cache.put("b", img::scale(red.data(), 4, 4, 2, 2, 0, 0));
    cache.pin("a");
    cache.put("c", img::scale(red.data(), 4, 4, 2, 2, 0, 0));
    cache.put("d", img::scale(red.data(), 4, 4, 2, 2, 0, 0));
    CHECK(cache.get("a") && !cache.get("b") && cache.get("c") && cache.get("d"));
}

static void test_backoff() {
    backend::Backoff b;
    CHECK(b.next() == 1000 && b.next() == 2000 && b.next() == 4000);
    for (int i = 0; i < 10; ++i) b.next();
    CHECK(b.next() == 30000);
    b.reset();
    CHECK(b.next() == 1000);
}

static void test_repeat_guard() {
    RepeatGuard g;
    CHECK(g.allow("a", 0));
    CHECK(!g.allow("a", 1000));
    CHECK(g.allow("b", 1100));
    CHECK(g.allow("a", 1200));
    CHECK(g.allow("a", 2800));
}

int main() {
    test_text();
    test_dedupe();
    test_search_session();
    test_ma_session();
    test_parse();
    test_i18n();
    test_app_with_mock();
    test_repeat_guard();
    test_display_text();
    test_setup_flow();
    test_panel_brightness();
    test_artwork();
    test_backoff();
    std::printf(g_failed ? "%d check(s) FAILED\n" : "all checks passed\n", g_failed);
    return g_failed ? 1 : 0;
}
