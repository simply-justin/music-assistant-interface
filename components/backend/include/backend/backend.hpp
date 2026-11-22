#pragma once
// The one seam between the app and a music server. Implementations: ma::Client (real server)
// and MockBackend (prototype + tests). All callbacks run on the core task.
#include <cstdint>
#include <functional>
#include <string>

#include "backend/json.hpp"

namespace backend {

struct Request {
    std::string command;  // e.g. "player_queues/play_pause"
    Json args;            // may be null
};

struct Response {
    bool ok = false;
    std::string error_code, details;
    Json result;  // complete result (partial chunks already joined)
};

struct Event {
    std::string name, object_id;
    Json data;
};

// AuthRequired: the server answered but no token is set yet (first-time setup).
enum class Connection : uint8_t { Disconnected, Connecting, Connected, AuthRequired, AuthFailed, IncompatibleServer, WrongServer };

struct ServerInfo {
    std::string server_id;
    std::string server_version;
    int schema_version = 0;
};

class Backend {
public:
    using Done = std::function<void(Response&&)>;
    struct Listener {
        std::function<void(Event&&)> on_event;
        std::function<void(Connection, const ServerInfo&)> on_connection;
    };

    virtual ~Backend() = default;
    virtual void set_listener(Listener listener) = 0;
    virtual void start() = 0;
    virtual void request(Request req, Done done) = 0;
    virtual void tick(uint32_t /*now_ms*/) {}
    // Switch server and/or token (setup, "Sign in again"); reconnects. url: "ws://host:port/ws".
    virtual void configure(const std::string& url, const std::string& token) = 0;
    virtual void reconnect() = 0;
};

}  // namespace backend
