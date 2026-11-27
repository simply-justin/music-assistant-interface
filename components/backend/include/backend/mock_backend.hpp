#pragma once
// Simulated Music Assistant for the prototype and tests: mixed providers, duplicates across
// sources, realistic latency, and library results arriving before the full search.
#include <deque>
#include <functional>
#include <string>
#include <vector>

#include "backend/backend.hpp"

namespace backend {

struct MockLatency {
    uint32_t library_ms = 80, full_ms = 400, command_ms = 30;
};

class MockBackend : public Backend {
public:
    explicit MockBackend(MockLatency latency = {});

    void set_listener(Listener l) override { listener_ = std::move(l); }
    void start() override;
    void request(Request req, Done done) override;
    void tick(uint32_t now_ms) override;
    // Mock rules: url containing "bad" never answers, "old" is an incompatible server, empty
    // token → AuthRequired, token "bad" → AuthFailed, "silent" never answers, anything else connects.
    void configure(const std::string& url, const std::string& token) override;
    void reconnect() override { start(); }
    // Starts unconfigured (first-time setup) instead of connected.
    void set_unconfigured() { url_.clear(); }
    // Test hooks: every command received, in order; change a player's playback state (sends
    // player_updated like the server).
    std::vector<std::string> received;
    std::vector<std::string> received_args;  // the args of each, as JSON
    void set_player_state(const std::string& player_id, const std::string& state);
    void remove_player(const std::string& player_id);  // sends player_removed
    int extra_library_tracks = 0;                     // padding for paged lists

private:
    struct Delivery {
        uint32_t due_ms;
        std::function<void()> fn;
    };

    void later(uint32_t delay_ms, std::function<void()> fn);
    Response handle(const Request& req);
    void emit(const char* event, const std::string& object_id, Json data);
    void add_to_queue(const std::string& uri);
    Json queue_object(const std::string& queue_id) const;

    MockLatency latency_;
    std::string url_ = "mock", token_ = "mock-token";
    Listener listener_;
    uint32_t now_ms_ = 0;
    std::deque<Delivery> deliveries_;
    Json catalog_, players_, queue_;  // queue_ = array of queue items
    int next_queue_item_ = 1;
};

}  // namespace backend
