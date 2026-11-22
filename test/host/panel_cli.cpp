// PC test app: runs the panel's real core (App + features + ma::Session) against a real
// Music Assistant server, with a text interface instead of the screens.
//   panel_cli [server.env]      (or env vars MA_URL / MA_TOKEN; --mock for the simulated server)
// Type "help" for commands.
#include <poll.h>
#include <unistd.h>

#include <chrono>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <sstream>

#include "app/app.hpp"
#include "backend/ma_session.hpp"
#include "backend/mock_backend.hpp"
#include "ws_posix.hpp"

namespace {

uint32_t now_ms() {
    using namespace std::chrono;
    static auto t0 = steady_clock::now();
    return static_cast<uint32_t>(duration_cast<milliseconds>(steady_clock::now() - t0).count());
}

// Same role as ma::Client on the device: socket + ma::Session, all on one thread.
class HostMaBackend : public backend::Backend {
public:
    HostMaBackend(std::string url, std::string token)
        : url_(std::move(url)), session_([this](const std::string& t) { return ws_.send_text(t); }, std::move(token)) {}

    void set_listener(Listener l) override { session_.set_listener(std::move(l)); }
    void start() override { try_connect(); }
    void request(backend::Request req, Done done) override { session_.request(std::move(req), std::move(done)); }

    void tick(uint32_t now) override {
        if (!ws_.connected()) {
            if (now >= retry_at_) try_connect();
            return;
        }
        std::vector<std::string> msgs;
        bool alive = ws_.poll(0, msgs);
        for (auto& m : msgs) session_.on_text(m);
        if (!alive) {
            session_.on_closed();
            retry_at_ = now + 3000;
        }
    }

private:
    void try_connect() {
        if (ws_.connect(url_)) {
            session_.on_open();
        } else {
            std::printf("! cannot reach %s, retrying in 3 s\n", url_.c_str());
            retry_at_ = now_ms() + 3000;
        }
    }

    std::string url_;
    WsPosix ws_;
    ma::Session session_;
    uint32_t retry_at_ = 0;
};

const char* conn_name(backend::Connection c) {
    switch (c) {
        case backend::Connection::Disconnected: return "disconnected";
        case backend::Connection::Connecting: return "connecting";
        case backend::Connection::Connected: return "connected";
        case backend::Connection::AuthFailed: return "token rejected";
        case backend::Connection::IncompatibleServer: return "server too old";
        case backend::Connection::WrongServer: return "different server";
    }
    return "?";
}

std::string artists(const core::MediaItem& m) {
    std::string s;
    for (const auto& a : m.artists) s += (s.empty() ? "" : ", ") + a.name;
    return s;
}

std::string describe(const core::MediaItem& m) {
    std::string s = m.name;
    if (!m.version.empty()) s += " (" + m.version + ")";
    if (!m.artists.empty()) s += " - " + artists(m);
    s += "  [" + m.badge_provider() + "]";
    if (!m.alternatives.empty()) s += " +" + std::to_string(m.alternatives.size()) + " source";
    return s;
}

struct Cli {
    std::shared_ptr<const core::SearchView> search;
    std::shared_ptr<const app::PlayersView> players;
    std::shared_ptr<const core::QueueState> queue;
    std::shared_ptr<const app::PlaylistsView> playlists;
    std::optional<app::Command> toast_action;
    std::vector<core::MediaItem> numbered;  // the last printed search results, 1-based for the user

    void on_update(app::Update& u) {
        if (auto* c = std::get_if<app::ConnectionView>(&u)) {
            std::printf("* %s", conn_name(c->state));
            if (!c->server.server_version.empty()) std::printf(" (Music Assistant %s)", c->server.server_version.c_str());
            std::printf("\n");
        } else if (auto* s = std::get_if<std::shared_ptr<const core::SearchView>>(&u)) {
            search = *s;
            if (search->status == core::SearchStatus::Partial || search->status == core::SearchStatus::Done ||
                search->status == core::SearchStatus::Error)
                print_search();
        } else if (auto* p = std::get_if<std::shared_ptr<const app::PlayersView>>(&u)) {
            bool first = !players;
            players = *p;
            if (first) print_players();
        } else if (auto* q = std::get_if<std::shared_ptr<const core::QueueState>>(&u)) {
            queue = *q;
        } else if (auto* l = std::get_if<std::shared_ptr<const app::PlaylistsView>>(&u)) {
            playlists = *l;
        } else if (auto* t = std::get_if<app::Toast>(&u)) {
            toast_action = t->action;
            std::printf("~ %s%s\n", t->text.c_str(), t->action ? ("   (type 'again' to " + t->action_label + ")").c_str() : "");
        }
        std::fflush(stdout);
    }

    void print_search() {
        const auto& v = *search;
        const char* status[] = {"idle", "loading", "library results, searching everywhere…", "done", "error"};
        std::printf("\n== \"%s\": %s\n", v.query.c_str(), status[static_cast<int>(v.status)]);
        numbered.clear();
        auto group = [&](const char* title, const std::vector<core::MediaItem>& items) {
            if (items.empty()) return;
            std::printf("  %s\n", title);
            for (const auto& m : items) {
                numbered.push_back(m);
                std::printf("   %2zu. %s\n", numbered.size(), describe(m).c_str());
            }
        };
        if (v.result.top) group("Top result", {*v.result.top});
        group("Tracks", v.result.tracks);
        group("Artists", v.result.artists);
        group("Albums", v.result.albums);
        group("Playlists", v.result.playlists);
        group("Radio", v.result.radio);
        if (v.status == core::SearchStatus::Done && v.result.empty()) std::printf("  (no results)\n");
    }

    void print_players() {
        if (!players) return;
        std::printf("Speakers:\n");
        for (size_t i = 0; i < players->players.size(); ++i) {
            const auto& p = players->players[i];
            std::printf("  %zu. %s%s  vol %d%s%s\n", i + 1, p.name.c_str(), p.player_id == players->selected_id ? "  <- selected" : "",
                        p.volume_level, p.available ? "" : "  (unavailable)",
                        p.group_members.empty() ? "" : "  (group)");
        }
    }

    void print_playlists() {
        if (!playlists) return void(std::printf("(no playlists yet)\n"));
        for (size_t i = 0; i < playlists->playlists.size(); ++i)
            std::printf("  %2zu. %s%s\n", i + 1, playlists->playlists[i].name.c_str(), playlists->playlists[i].editable ? "" : "  (read-only)");
    }

    void print_queue() {
        if (!queue) return void(std::printf("(no queue yet)\n"));
        std::printf("Queue %s: %zu items, current %d\n", queue->queue_id.c_str(), queue->items.size(), queue->current_index);
        for (size_t i = 0; i < queue->items.size() && i < 30; ++i)
            std::printf("  %2zu. %s%s\n", i + 1, queue->items[i].name.c_str(), static_cast<int>(i) == queue->current_index ? "  <- now" : "");
    }
};

const char* kHelp = R"(commands:
  s <text>        search everywhere (results are numbered)
  play <n>        play result n now          add <n> / next <n>  add to queue (end / next)
  fav <n>         favourite result n
  pp | n | p      play/pause, next, previous
  speakers        list speakers              use <n>    select speaker n
  vol <0-100>     volume of selected speaker gvol <0-100> group volume
  q               show queue                 rm <n>     remove queue item n
  clear           clear up next              again      run the last toast's action
  pl              list playlists             pladd <p> <n>  add result n to playlist p
  plnew <name>    create an empty playlist
  lang en|nl      switch language            quit
)";

std::map<std::string, std::string> read_env(const char* path) {
    std::map<std::string, std::string> env;
    std::ifstream f(path);
    for (std::string line; std::getline(f, line);) {
        auto eq = line.find('=');
        if (eq != std::string::npos && line[0] != '#') env[line.substr(0, eq)] = line.substr(eq + 1);
    }
    return env;
}

}  // namespace

int main(int argc, char** argv) {
    bool mock = argc > 1 && std::string(argv[1]) == "--mock";
    auto env = read_env(argc > 1 && !mock ? argv[1] : "server.env");
    auto get = [&](const char* k) { const char* e = std::getenv(k); return e ? std::string(e) : env[k]; };

    std::unique_ptr<backend::Backend> be;
    if (mock) {
        be = std::make_unique<backend::MockBackend>();
    } else {
        if (get("MA_URL").empty()) {
            std::fprintf(stderr, "Need MA_URL and MA_TOKEN (env or a server.env file), or --mock\n");
            return 1;
        }
        be = std::make_unique<HostMaBackend>(get("MA_URL"), get("MA_TOKEN"));
    }

    Cli cli;
    app::MemorySettings settings;
    std::vector<app::Update> inbox;
    app::App a(*be, settings, [&](app::Update u) { inbox.push_back(std::move(u)); });
    a.start();
    std::printf("%s", kHelp);

    auto result = [&](const std::string& arg) -> const core::MediaItem* {
        size_t n = std::strtoul(arg.c_str(), nullptr, 10);
        if (n < 1 || n > cli.numbered.size()) return std::printf("! no result %s\n", arg.c_str()), nullptr;
        return &cli.numbered[n - 1];
    };

    for (;;) {
        pollfd in{0, POLLIN, 0};
        if (::poll(&in, 1, 10) > 0) {
            std::string line;
            if (!std::getline(std::cin, line)) break;
            std::istringstream ss(line);
            std::string cmd, arg;
            ss >> cmd;
            std::getline(ss >> std::ws, arg);
            if (cmd == "quit" || cmd == "exit") break;
            else if (cmd == "help") std::printf("%s", kHelp);
            else if (cmd == "s") a.handle(app::cmd::Search{arg});
            else if (cmd == "play") { if (auto* m = result(arg)) a.handle(app::cmd::Play{*m}); }
            else if (cmd == "add") { if (auto* m = result(arg)) a.handle(app::cmd::AddToQueue{*m}); }
            else if (cmd == "next") { if (auto* m = result(arg)) a.handle(app::cmd::AddToQueue{*m, true}); }
            else if (cmd == "fav") { if (auto* m = result(arg)) a.handle(app::cmd::SetFavourite{*m, true}); }
            else if (cmd == "pp") a.handle(app::cmd::PlayPause{});
            else if (cmd == "n") a.handle(app::cmd::Next{});
            else if (cmd == "p") a.handle(app::cmd::Previous{});
            else if (cmd == "speakers") cli.print_players();
            else if (cmd == "use" && cli.players) {
                size_t n = std::strtoul(arg.c_str(), nullptr, 10);
                if (n >= 1 && n <= cli.players->players.size()) a.handle(app::cmd::SelectPlayer{cli.players->players[n - 1].player_id});
            } else if ((cmd == "vol" || cmd == "gvol") && cli.players)
                a.handle(app::cmd::SetVolume{cli.players->selected_id, std::atoi(arg.c_str()), cmd == "gvol"});
            else if (cmd == "q") cli.print_queue();
            else if (cmd == "pl") cli.print_playlists();
            else if (cmd == "pladd" && cli.playlists) {
                std::istringstream as(arg);
                size_t p = 0;
                std::string n;
                as >> p >> n;
                if (p < 1 || p > cli.playlists->playlists.size()) std::printf("! no playlist %zu\n", p);
                else if (!cli.playlists->playlists[p - 1].editable) std::printf("! that playlist can't be changed\n");
                else if (auto* m = result(n)) a.handle(app::cmd::AddToPlaylist{cli.playlists->playlists[p - 1], *m});
            } else if (cmd == "plnew" && !arg.empty()) a.handle(app::cmd::CreatePlaylist{arg, std::nullopt});
            else if (cmd == "rm" && cli.queue) {
                size_t n = std::strtoul(arg.c_str(), nullptr, 10);
                if (n >= 1 && n <= cli.queue->items.size()) a.handle(app::cmd::RemoveFromQueue{cli.queue->items[n - 1].queue_item_id});
            } else if (cmd == "clear") a.handle(app::cmd::ClearUpNext{});
            else if (cmd == "again" && cli.toast_action) a.handle(*cli.toast_action);
            else if (cmd == "lang") { i18n::Lang l; if (i18n::language_from_code(arg, l)) a.handle(app::cmd::SetLanguage{l}); }
            else if (!cmd.empty()) std::printf("? unknown command, type help\n");
        }
        a.tick(now_ms());
        for (auto& u : inbox) cli.on_update(u);
        inbox.clear();
    }
    return 0;
}
