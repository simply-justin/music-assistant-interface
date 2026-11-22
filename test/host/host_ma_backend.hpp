#pragma once
// PC stand-in for ma::Client (device): POSIX WebSocket + the same ma::Session, on one thread.
// Used by panel_cli and the simulator.
#include <chrono>
#include <cstdio>
#include <string>
#include <vector>

#include "backend/ma_session.hpp"
#include "ws_posix.hpp"

inline uint32_t host_now_ms() {
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
    void start() override {
        if (!url_.empty()) try_connect();
    }
    void request(backend::Request req, Done done) override { session_.request(std::move(req), std::move(done)); }

    void configure(const std::string& url, const std::string& token) override {
        url_ = url;
        session_.set_token(token);
        reconnect();
    }

    void reconnect() override {
        if (ws_.connected()) {
            ws_.close();
            session_.on_closed();
        }
        try_connect();
    }

    void tick(uint32_t now) override {
        if (url_.empty()) return;  // not set up yet
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
        if (url_.empty()) return;
        if (ws_.connect(url_)) {
            session_.on_open();
        } else {
            std::printf("! cannot reach %s, retrying in 3 s\n", url_.c_str());
            retry_at_ = host_now_ms() + 3000;
        }
    }

    std::string url_;
    WsPosix ws_;
    ma::Session session_;
    uint32_t retry_at_ = 0;
};

