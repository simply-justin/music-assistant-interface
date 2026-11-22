#pragma once
// Real Music Assistant backend (ESP-IDF only): esp_websocket_client + ma::Session.
// Socket callbacks run on the websocket task; everything is posted to the core task so the
// session and the app never need locks.
#include <functional>
#include <string>

#include "backend/backend.hpp"
#include "backend/ma_session.hpp"

typedef struct esp_websocket_client* esp_websocket_client_handle_t;

namespace ma {

class Client : public backend::Backend {
public:
    using Post = std::function<void(std::function<void()>)>;  // run fn on the core task

    // url: "ws://<server>:8095/ws"
    Client(std::string url, std::string token, std::string expected_server_id, Post post);
    ~Client() override;

    void set_listener(Listener l) override { session_.set_listener(std::move(l)); }
    void start() override;
    void request(backend::Request req, Done done) override { session_.request(std::move(req), std::move(done)); }
    void configure(const std::string& url, const std::string& token) override;
    void reconnect() override;

private:
    static void on_ws_event(void* arg, const char* base, int32_t id, void* data);

    std::string url_;
    Post post_;
    Session session_;
    esp_websocket_client_handle_t ws_ = nullptr;
    std::string rx_;  // reassembles one message split over several data events (websocket task only)
};

}  // namespace ma
