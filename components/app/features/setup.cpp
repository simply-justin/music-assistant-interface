// First-time setup: Wi-Fi → server → sign-in → speaker → done. Saves server_url, token and
// setup_done. Each step reports its own errors (wrong password, server not found, token rejected).
#include "features/features.hpp"

namespace app {
namespace {

constexpr uint32_t kServerTimeoutMs = 8000;

// "192.168.1.20", "host:8095" or a full ws:// URL → ws://host:port/ws (MA's default port 8095).
std::string to_url(std::string a) {
    while (!a.empty() && a.back() == ' ') a.pop_back();
    while (!a.empty() && a.front() == ' ') a.erase(0, 1);
    if (a.rfind("ws://", 0) == 0) return a;
    if (a.rfind("http://", 0) == 0) a = a.substr(7);
    a = a.substr(0, a.find('/'));
    if (a.find(':') == std::string::npos) a += ":8095";
    return "ws://" + a + "/ws";
}

class Setup : public Feature {
public:
    explicit Setup(Context& ctx) : ctx_(ctx) {
        view_.step = ctx.settings.get("setup_done") == "1" ? SetupStep::Finished : SetupStep::Welcome;
    }

    void on_connection(backend::Connection c) override {
        if (!view_.checking) return;
        using backend::Connection;
        if (view_.step == SetupStep::Server && (c == Connection::AuthRequired || c == Connection::Connected)) {
            // The server answered. Remember it, then sign in (or skip if it needs no token).
            ctx_.settings.set("server_url", to_url(view_.address));
            view_.checking = false;
            view_.server_error = false;
            go(c == Connection::Connected ? SetupStep::Speaker : SetupStep::SignIn);
        } else if (view_.step == SetupStep::SignIn && (c == Connection::Connected || c == Connection::AuthFailed)) {
            view_.checking = false;
            view_.token_error = c == Connection::AuthFailed;
            if (c == Connection::Connected) {
                ctx_.settings.set("token", pending_token_);
                ctx_.platform.pairing_stop();
                go(SetupStep::Speaker);
            } else {
                publish();
            }
        }
    }

    void tick() override {
        if (view_.checking && view_.step == SetupStep::Server && ctx_.now_ms - check_started_ > kServerTimeoutMs) {
            view_.checking = false;
            view_.server_error = true;  // "Nothing answered at that address"
            publish();
        }
    }

    void on_command(const Command& c) override {
        if (auto* g = std::get_if<cmd::SetupGo>(&c)) {
            go(g->step);
        } else if (std::holds_alternative<cmd::SetupScanWifi>(c)) {
            scan();
        } else if (auto* j = std::get_if<cmd::SetupJoinWifi>(&c)) {
            view_.ssid = j->ssid;
            view_.joining = true;
            view_.wifi_error.reset();
            publish();
            ctx_.platform.wifi_join(j->ssid, j->password, [this](WifiResult r) {
                view_.joining = false;
                if (r == WifiResult::Ok) return go(SetupStep::Server);
                view_.wifi_error = r;
                publish();
            });
        } else if (std::holds_alternative<cmd::SetupFindServers>(c)) {
            find_servers();
        } else if (auto* u = std::get_if<cmd::SetupUseServer>(&c)) {
            view_.address = u->address;
            view_.checking = true;
            view_.server_error = false;
            check_started_ = ctx_.now_ms;
            publish();
            ctx_.backend.configure(to_url(u->address), "");
        } else if (auto* t = std::get_if<cmd::SetupToken>(&c)) {
            pending_token_ = t->token;
            view_.checking = true;
            view_.token_error = false;
            publish();
            ctx_.backend.configure(ctx_.settings.get("server_url"), t->token);
        } else if (std::holds_alternative<cmd::SetupFinish>(c)) {
            ctx_.settings.set("setup_done", "1");
            go(SetupStep::Finished);
        } else if (std::holds_alternative<cmd::SignInAgain>(c)) {
            go(SetupStep::SignIn);  // from Settings › Music Assistant
        }
    }

    // Publish once at start so the UI knows whether to show setup.
    void on_start() { publish(); }

private:
    void go(SetupStep step) {
        view_.step = step;
        if (step == SetupStep::Wifi && view_.networks.empty()) return scan();
        if (step == SetupStep::Server && view_.servers.empty()) return find_servers();
        if (step == SetupStep::SignIn && view_.pairing_url.empty()) {
            view_.pairing_url = ctx_.platform.pairing_start(view_.address, [this](std::string token) {
                ctx_.dispatch(cmd::SetupToken{std::move(token)});  // the phone sent it
            });
        }
        publish();
    }

    void scan() {
        view_.scanning = true;
        publish();
        ctx_.platform.wifi_scan([this](std::vector<WifiNetwork> n) {
            view_.networks = std::move(n);
            view_.scanning = false;
            publish();
        });
    }

    void find_servers() {
        view_.searching = true;
        publish();
        ctx_.platform.discover_servers([this](std::vector<ServerCandidate> s) {
            view_.servers = std::move(s);
            view_.searching = false;
            publish();
        });
    }

    void publish() { ctx_.publish(std::make_shared<const SetupView>(view_)); }

    Context& ctx_;
    SetupView view_;
    uint32_t check_started_ = 0;
    std::string pending_token_;
};

}  // namespace

std::unique_ptr<Feature> make_setup(Context& ctx) {
    auto f = std::make_unique<Setup>(ctx);
    f->on_start();
    return f;
}

}  // namespace app
