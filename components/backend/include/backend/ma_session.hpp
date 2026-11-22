#pragma once
// Music Assistant WebSocket protocol, without the socket: message ids, response matching,
// partial results, events, server_info, auth. Feed it text frames; it calls `send` for output.
// Message shapes (verified against server 2.10.13; server_info arrives first, then `auth` is required):
//   out: {message_id, command, args}
//   in:  {message_id, result, partial?} | {message_id, error_code, details}
//        {event, object_id, data} | server_info {server_version, schema_version, ...}
#include <cstdint>
#include <deque>
#include <functional>
#include <map>
#include <string>
#include <string_view>

#include "backend/backend.hpp"

namespace ma {

// Auth became mandatory at schema 28, and this client always authenticates.
constexpr int kMinSchemaVersion = 28;

class Session {
public:
    using Send = std::function<bool(const std::string& text)>;

    // expected_server_id: the id seen at setup. A different server at the same address never gets
    // the token (advice from MA's /api-docs). Empty = accept any (first connection).
    Session(Send send, std::string token, std::string expected_server_id = "")
        : send_(std::move(send)), token_(std::move(token)), expected_server_id_(std::move(expected_server_id)) {}

    void set_listener(backend::Backend::Listener l) { listener_ = std::move(l); }
    void request(backend::Request req, backend::Backend::Done done);
    void on_text(std::string_view text);
    void on_open();
    void on_closed();  // fails every pending request with error_code "disconnected"

    backend::Connection connection() const { return connection_; }
    const backend::ServerInfo& server_info() const { return info_; }

private:
    struct Pending {
        backend::Backend::Done done;
        backend::Json partial;  // joined chunks of a partial result
    };

    void set_connection(backend::Connection c);
    void send_now(backend::Request req, backend::Backend::Done done);
    void fail_waiting();
    void on_server_info(const cJSON* msg);
    void on_response(const std::string& id, const cJSON* msg);

    Send send_;
    std::string token_;
    std::string expected_server_id_;
    backend::Backend::Listener listener_;
    std::map<std::string, Pending> pending_;
    // Requests made after the socket opened but before auth finished; sent once connected.
    std::deque<std::pair<backend::Request, backend::Backend::Done>> waiting_;
    uint32_t next_id_ = 1;
    backend::Connection connection_ = backend::Connection::Disconnected;
    backend::ServerInfo info_;
};

}  // namespace ma
