// Integration tests: the whole core (App + every feature) against the mock server and the
// fake platform, driven by commands the UI sends, checked on the updates the UI receives.
#include <algorithm>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "app/app.hpp"
#include "backend/mock_backend.hpp"
#include "check.hpp"
#include "fake_platform.hpp"
#include "i18n/i18n.hpp"

using namespace core;

TEST(test_app_with_mock) {
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
    run_until(3500);
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
    run_until(4000);
    const app::Toast* undo = nullptr;
    for (auto& u : updates)
        if (auto* t = std::get_if<app::Toast>(&u)) undo = t;
    CHECK(undo && undo->action_label == "Undo" && std::holds_alternative<app::cmd::RemoveFromQueue>(*undo->action));
    std::shared_ptr<const core::QueueState> q;
    for (auto& u : updates)
        if (auto* x = std::get_if<std::shared_ptr<const core::QueueState>>(&u)) q = *x;
    size_t before = q ? q->items.size() : 0;
    if (undo) a.handle(*undo->action);
    run_until(4500);
    for (auto& u : updates)
        if (auto* x = std::get_if<std::shared_ptr<const core::QueueState>>(&u)) q = *x;
    CHECK(q && q->items.size() + 1 == before);

    // Grouping: Kitchen joins Living Room, then the group is dissolved.
    a.handle(app::cmd::SetGroup{"living", {"kitchen"}});
    run_until(4600);
    for (auto& u : updates)
        if (auto* p = std::get_if<std::shared_ptr<const app::PlayersView>>(&u)) players = *p;
    CHECK(players && players->players[0].group_members.size() == 2 && players->players[1].synced_to == "living");
    a.handle(app::cmd::Ungroup{"living"});
    run_until(4700);
    for (auto& u : updates)
        if (auto* p = std::get_if<std::shared_ptr<const app::PlayersView>>(&u)) players = *p;
    CHECK(players && players->players[0].group_members.empty() && players->players[1].synced_to.empty());

    // Volume is clamped and reflected.
    a.handle(app::cmd::SetVolume{"living", 140});
    run_until(4800);
    for (auto& u : updates)
        if (auto* p = std::get_if<std::shared_ptr<const app::PlayersView>>(&u)) players = *p;
    CHECK(players && players->players[0].volume_level == 100);

    // Playlists: list (editable flag parsed), add a song, create one.
    a.handle(app::cmd::LoadPlaylists{});
    run_until(4950);
    std::shared_ptr<const app::PlaylistsView> pls;
    for (auto& u : updates)
        if (auto* p = std::get_if<std::shared_ptr<const app::PlaylistsView>>(&u)) pls = *p;
    CHECK(pls && pls->playlists.size() == 2 && pls->playlists[0].editable && !pls->playlists[1].editable);
    if (!pls || pls->playlists.empty()) return;
    updates.clear();
    a.handle(app::cmd::AddToPlaylist{pls->playlists[0], viva});
    a.handle(app::cmd::CreatePlaylist{"Kitchen songs", viva});
    run_until(5400);
    std::vector<std::string> toasts;
    for (auto& u : updates)
        if (auto* t = std::get_if<app::Toast>(&u)) toasts.push_back(t->text);
    CHECK(toasts.size() == 2 && toasts[0] == "Added to Friday Night Mix" && toasts[1] == "Playlist Kitchen songs created");

    a.handle(app::cmd::SetLanguage{i18n::Lang::nl});
    CHECK(settings.get("lang") == "nl");
    i18n::set_language(i18n::Lang::en);
}

TEST(test_setup_flow) {
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
    CHECK(platform.last_pairing_server == "192.168.2.80");  // the page names the server the token goes to
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

TEST(test_panel_brightness) {
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
    CHECK(platform.brightness == 25);
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

// ---- Harness for the tests below: one App on the mock server, a clock, and the updates it sent.

namespace {

struct Panel {
    backend::MockBackend mock;
    app::MemorySettings settings;
    FakePlatform platform;
    std::vector<app::Update> updates;
    std::unique_ptr<app::App> app;
    uint32_t now = 0;

    explicit Panel(std::function<void(Panel&)> before_start = {}) {
        if (before_start) before_start(*this);
        app = std::make_unique<app::App>(mock, settings, platform, [this](app::Update u) { updates.push_back(std::move(u)); });
        app->start();
        run(300);
    }
    void run(uint32_t ms) {
        for (uint32_t end = now + ms; now <= end; now += 10) {
            platform.tick(now);
            app->tick(now);
        }
    }
    void send(app::Command c, uint32_t then_ms = 300) {
        app->handle(std::move(c));
        run(then_ms);
    }
    // The newest update of type T, or null.
    template <class T>
    std::shared_ptr<const T> last() const {
        for (auto it = updates.rbegin(); it != updates.rend(); ++it)
            if (auto* p = std::get_if<std::shared_ptr<const T>>(&*it)) return *p;
        return nullptr;
    }
    std::shared_ptr<const app::BrowseView> browse(app::cmd::BrowseKind kind) const {
        for (auto it = updates.rbegin(); it != updates.rend(); ++it)
            if (auto* p = std::get_if<std::shared_ptr<const app::BrowseView>>(&*it))
                if ((*p)->kind == kind) return *p;
        return nullptr;
    }
    std::vector<std::string> toasts() const {
        std::vector<std::string> out;
        for (const auto& u : updates)
            if (auto* t = std::get_if<app::Toast>(&u)) out.push_back(t->text);
        return out;
    }
    std::optional<backend::Connection> connection() const {
        for (auto it = updates.rbegin(); it != updates.rend(); ++it)
            if (auto* c = std::get_if<app::ConnectionView>(&*it)) return c->state;
        return std::nullopt;
    }
};

}  // namespace

TEST(connects_and_selects_the_first_available_speaker) {
    Panel p;
    CHECK(p.connection() == backend::Connection::Connected);
    auto players = p.last<app::PlayersView>();
    CHECK(players && players->players.size() == 2);
    CHECK(players && players->selected_id == "living");
}

TEST(selected_speaker_is_remembered) {
    Panel p;
    p.send(app::cmd::SelectPlayer{"kitchen"});
    CHECK(p.settings.get("player") == "kitchen");
    auto players = p.last<app::PlayersView>();
    CHECK(players && players->selected_id == "kitchen");
    auto q = p.last<core::QueueState>();
    CHECK(q && q->queue_id == "kitchen");  // the queue follows the speaker
}

TEST(rejected_token_reports_auth_failed) {
    Panel p([](Panel& x) {
        x.settings.set("server_url", "mock");
        x.settings.set("token", "bad");
        x.settings.set("setup_done", "1");
    });
    CHECK(p.connection() == backend::Connection::AuthFailed);
    CHECK(!p.last<app::PlayersView>());  // nothing loads without a session
}

TEST(reconnect_reloads_everything) {
    Panel p;
    size_t before = p.updates.size();
    p.send(app::cmd::Reconnect{});
    CHECK(p.connection() == backend::Connection::Connected);
    bool reloaded = false;
    for (size_t i = before; i < p.updates.size(); ++i)
        if (std::holds_alternative<std::shared_ptr<const app::PlayersView>>(p.updates[i])) reloaded = true;
    CHECK(reloaded);
}

TEST(home_lists_load) {
    using K = app::cmd::BrowseKind;
    Panel p;
    p.send(app::cmd::Browse{K::Favourites, {}});
    p.send(app::cmd::Browse{K::LibraryRadio, {}});
    p.send(app::cmd::Browse{K::RecentlyPlayed, {}});
    auto fav = p.browse(K::Favourites), radio = p.browse(K::LibraryRadio), recent = p.browse(K::RecentlyPlayed);
    CHECK(fav && !fav->loading && !fav->failed && !fav->items.empty());
    CHECK(radio && !radio->items.empty());
    bool all_radio = radio != nullptr;
    for (const auto& m : radio ? radio->items : std::vector<MediaItem>{}) all_radio &= m.type == MediaType::Radio;
    CHECK(all_radio);
    CHECK(recent && !recent->loading);
}

TEST(playlist_tracks_load_in_full) {
    Panel p;
    MediaItem pl;
    pl.type = MediaType::Playlist;
    pl.item_id = "p2";
    pl.provider = "library";
    pl.uri = "library://playlist/p2";
    p.send(app::cmd::Browse{app::cmd::BrowseKind::PlaylistTracks, pl}, 1000);
    auto v = p.browse(app::cmd::BrowseKind::PlaylistTracks);
    CHECK(v && !v->loading && v->of_uri == pl.uri && v->items.size() >= 60);
}

TEST(play_replaces_the_queue) {
    Panel p;
    MediaItem song;
    song.type = MediaType::Track;
    song.name = "Yellow";
    song.uri = "spotify://track/7";
    song.provider = "spotify--a1";
    p.send(app::cmd::Play{song});
    auto q = p.last<core::QueueState>();
    CHECK(q && !q->items.empty() && q->items.back().name == "Yellow");
}

TEST(source_order_is_saved_and_restored) {
    app::MemorySettings saved;
    {
        Panel p;
        p.send(app::cmd::SetSourceOrder{{"ytmusic", "spotify"}});
        CHECK(p.settings.get("source_order") == "ytmusic,spotify");
        saved.set("source_order", p.settings.get("source_order"));
    }
    Panel again([&](Panel& x) { x.settings.set("source_order", saved.get("source_order")); });
    auto view = again.last<app::PanelView>();
    CHECK(view && view->source_order == (std::vector<std::string>{"ytmusic", "spotify"}));
}

TEST(language_change_is_saved_and_announced) {
    Panel p;
    p.send(app::cmd::SetLanguage{i18n::Lang::nl});
    CHECK(p.settings.get("lang") == "nl");
    bool announced = false;
    for (const auto& u : p.updates)
        if (auto* l = std::get_if<app::LanguageChanged>(&u)) announced |= l->lang == i18n::Lang::nl;
    CHECK(announced);
    i18n::set_language(i18n::Lang::en);
}

TEST(unreachable_server_does_not_block_commands) {
    Panel p([](Panel& x) {
        x.settings.set("server_url", "ws://bad-host:8095/ws");
        x.settings.set("token", "t");
        x.settings.set("setup_done", "1");
    });
    CHECK(p.connection() != backend::Connection::Connected);
    p.send(app::cmd::SetVolume{"living", 50});  // must neither crash nor hang
    p.send(app::cmd::Search{"viva"});
    CHECK(true);
}

TEST(playing_on_a_paused_speaker_stops_the_old_song_first) {
    Panel p;
    MediaItem song;
    song.type = MediaType::Track;
    song.name = "Yellow";
    song.uri = "spotify://track/7";
    // Playing: one round trip, no stop.
    p.mock.received.clear();
    p.send(app::cmd::Play{song});
    CHECK(std::count(p.mock.received.begin(), p.mock.received.end(), "player_queues/stop") == 0);
    // Paused: MA would resume the old song for a moment; stop it, then play.
    p.mock.set_player_state("living", "paused");
    p.run(100);
    p.mock.received.clear();
    song.uri = "spotify://track/8";  // a different song (the double-tap guard ignores repeats)
    p.send(app::cmd::Play{song});
    auto stop = std::find(p.mock.received.begin(), p.mock.received.end(), "player_queues/stop");
    auto play = std::find(p.mock.received.begin(), p.mock.received.end(), "player_queues/play_media");
    CHECK(stop != p.mock.received.end() && play != p.mock.received.end() && stop < play);
}

TEST(a_radio_station_is_never_added_to_the_queue) {
    Panel p;
    MediaItem station;
    station.type = MediaType::Radio;
    station.name = "NPO Radio 2";
    station.uri = "tunein://radio/s1";
    p.mock.received.clear();
    p.send(app::cmd::AddToQueue{station});
    p.send(app::cmd::AddToQueue{station, true});
    CHECK(std::count(p.mock.received.begin(), p.mock.received.end(), "player_queues/play_media") == 0);
}

namespace {
bool sent_with(const Panel& p, const std::string& command, const std::string& fragment) {
    for (size_t i = 0; i < p.mock.received.size(); ++i)
        if (p.mock.received[i] == command && p.mock.received_args[i].find(fragment) != std::string::npos) return true;
    return false;
}
}  // namespace

TEST(a_song_tapped_in_a_playlist_plays_the_playlist_from_that_song) {
    Panel p;
    MediaItem playlist;
    playlist.type = MediaType::Playlist;
    playlist.uri = "library://playlist/1";
    MediaItem song;
    song.type = MediaType::Track;
    song.uri = "spotify://track/7";
    p.send(app::cmd::Play{song, playlist});
    CHECK(sent_with(p, "player_queues/play_media", "\"media\":[\"library://playlist/1\"]"));
    CHECK(sent_with(p, "player_queues/play_media", "\"start_item\":\"spotify://track/7\""));
    p.send(app::cmd::Play{song, std::nullopt, true});
    CHECK(sent_with(p, "player_queues/play_media", "\"radio_mode\":true"));
}

TEST(sleep_timer_pauses_when_it_runs_out) {
    Panel p;
    p.send(app::cmd::SetSleepTimer{1});
    std::optional<int> left;
    for (auto& u : p.updates)
        if (auto* s = std::get_if<app::SleepTimerView>(&u)) left = s->minutes_left;
    CHECK(left && *left == 1);
    CHECK(std::count(p.mock.received.begin(), p.mock.received.end(), "player_queues/pause") == 0);
    p.run(61'000);
    CHECK(std::count(p.mock.received.begin(), p.mock.received.end(), "player_queues/pause") == 1);
    for (auto& u : p.updates)
        if (auto* s = std::get_if<app::SleepTimerView>(&u)) left = s->minutes_left;
    CHECK(left && *left == 0);
}

TEST(switching_away_from_a_playing_speaker_offers_to_move_the_music) {
    Panel p;  // Living Room plays in the mock
    p.send(app::cmd::SelectPlayer{"kitchen"});
    const app::Toast* offer = nullptr;
    for (auto& u : p.updates)
        if (auto* t = std::get_if<app::Toast>(&u)) offer = t;
    CHECK(offer && offer->action && std::holds_alternative<app::cmd::TransferQueue>(*offer->action));
    if (!offer || !offer->action) return;
    p.send(*offer->action);
    CHECK(sent_with(p, "player_queues/transfer", "\"source_queue_id\":\"living\""));
    CHECK(sent_with(p, "player_queues/transfer", "\"target_queue_id\":\"kitchen\""));
}

TEST(a_removed_speaker_leaves_the_list) {
    Panel p;
    p.mock.remove_player("living");  // the selected one
    p.run(300);
    auto players = p.last<app::PlayersView>();
    CHECK(players && players->players.size() == 1 && players->selected_id == "kitchen");
}

TEST(recent_searches_are_saved_newest_first_without_duplicates) {
    Panel p;
    p.send(app::cmd::SaveRecentSearch{"coldplay"});
    p.send(app::cmd::SaveRecentSearch{"tame impala"});
    p.send(app::cmd::SaveRecentSearch{" Coldplay "});
    std::vector<std::string> recent;
    for (auto& u : p.updates)
        if (auto* r = std::get_if<app::RecentSearches>(&u)) recent = r->queries;
    CHECK(recent.size() == 2 && recent[0] == "Coldplay" && recent[1] == "tame impala");
    CHECK(p.settings.get("recent_searches") == "Coldplay\ntame impala");
}

TEST(a_favourite_is_removed_through_its_library_copy) {
    Panel p;
    MediaItem song;
    song.type = MediaType::Track;
    song.provider = "spotify--a1";
    song.item_id = "7";
    MediaItem library_copy = song;
    library_copy.provider = "library";
    library_copy.item_id = "12";
    song.alternatives.push_back(library_copy);
    p.send(app::cmd::SetFavourite{song, false});
    CHECK(sent_with(p, "music/favorites/remove_item", "\"library_item_id\":\"12\""));
    // Favourited from Search: the server knows its library copy.
    MediaItem searched;
    searched.type = MediaType::Track;
    searched.provider = "spotify--a1";
    searched.item_id = "1mea3bSkSGXuIRvnydlB5b";
    p.mock.received.clear();
    p.mock.received_args.clear();
    p.send(app::cmd::SetFavourite{searched, false}, 500);
    CHECK(sent_with(p, "music/favorites/remove_item", "\"library_item_id\":\"12\""));
    song.alternatives.clear();  // not in the library at all: say so instead of doing nothing
    p.updates.clear();
    p.send(app::cmd::SetFavourite{song, false}, 500);
    CHECK(p.toasts().size() == 1 && p.toasts()[0] == "Couldn't remove it from favourites");
}

TEST(library_lists_are_fetched_page_by_page) {
    Panel p;
    p.mock.extra_library_tracks = 1200;
    p.mock.received.clear();
    p.send(app::cmd::Browse{app::cmd::BrowseKind::LibraryTracks, {}}, 1000);
    auto list = p.browse(app::cmd::BrowseKind::LibraryTracks);
    CHECK(list && !list->loading && list->items.size() > 1200);
    CHECK(std::count(p.mock.received.begin(), p.mock.received.end(), "music/tracks/library_items") == 3);
}

TEST(playlist_removals_wait_for_each_other) {
    Panel p;
    MediaItem playlist;
    playlist.type = MediaType::Playlist;
    playlist.item_id = "1";
    playlist.uri = "library://playlist/1";
    p.mock.received.clear();
    p.app->handle(app::cmd::RemoveFromPlaylist{playlist, 3});
    p.app->handle(app::cmd::RemoveFromPlaylist{playlist, 4});  // tapped before the list refreshed
    p.run(500);
    CHECK(std::count(p.mock.received.begin(), p.mock.received.end(), "music/playlists/remove_playlist_tracks") == 1);
    p.send(app::cmd::RemoveFromPlaylist{playlist, 4});  // after the refresh: fine
    CHECK(std::count(p.mock.received.begin(), p.mock.received.end(), "music/playlists/remove_playlist_tracks") == 2);
}

TEST(setup_reports_an_old_server_and_a_silent_sign_in) {
    Panel p([](Panel& x) {
        x.mock.set_unconfigured();
        x.platform.token = "";  // no phone: the token is typed
    });
    p.send(app::cmd::SetupGo{app::SetupStep::Server}, 2000);
    p.send(app::cmd::SetupUseServer{"old-host"});
    auto v = p.last<app::SetupView>();
    CHECK(v && v->server_error && v->server_too_old && !v->checking);
    p.send(app::cmd::SetupUseServer{"192.168.2.80"});
    v = p.last<app::SetupView>();
    CHECK(v && v->step == app::SetupStep::SignIn);
    p.send(app::cmd::SetupToken{"silent"}, 9000);
    v = p.last<app::SetupView>();
    CHECK(v && v->token_no_answer && !v->token_error && !v->checking);
}

TEST(update_checks_then_installs_and_restarts) {
    Panel p;
    p.send(app::cmd::CheckUpdate{}, 800);
    std::optional<app::UpdateView> u;
    for (auto& x : p.updates)
        if (auto* v = std::get_if<app::UpdateView>(&x)) u = *v;
    CHECK(u && u->state == app::UpdateState::Available && u->version == "v9.9.9");
    p.send(app::cmd::InstallUpdate{}, 2500);
    CHECK(p.platform.restarted);
    Panel offline([](Panel& x) { x.platform.published = ""; });
    offline.send(app::cmd::CheckUpdate{}, 800);
    for (auto& x : offline.updates)
        if (auto* v = std::get_if<app::UpdateView>(&x)) u = *v;
    CHECK(u && u->state == app::UpdateState::NoNetwork);
}

int main() { return check::run_all("integration"); }
