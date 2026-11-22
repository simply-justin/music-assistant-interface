#include "backend/ma_client.hpp"

#include "esp_log.h"
#include "esp_websocket_client.h"

namespace ma {

namespace {
constexpr const char* TAG = "ma";
}

Client::Client(std::string url, std::string token, std::string expected_server_id, Post post)
    : url_(std::move(url)),
      post_(std::move(post)),
      session_(
          [this](const std::string& text) {
              return ws_ && esp_websocket_client_is_connected(ws_) &&
                     esp_websocket_client_send_text(ws_, text.data(), static_cast<int>(text.size()), pdMS_TO_TICKS(1000)) >= 0;
          },
          std::move(token), std::move(expected_server_id)) {}

Client::~Client() {
    if (ws_) {
        esp_websocket_client_stop(ws_);
        esp_websocket_client_destroy(ws_);
    }
}

void Client::configure(const std::string& url, const std::string& token) {
    url_ = url;
    session_.set_token(token);
    if (!ws_) return start();
    esp_websocket_client_stop(ws_);  // blocks until the socket task is down
    session_.on_closed();
    esp_websocket_client_set_uri(ws_, url_.c_str());
    esp_websocket_client_start(ws_);
}

void Client::reconnect() {
    if (!ws_) return start();
    esp_websocket_client_stop(ws_);
    session_.on_closed();
    esp_websocket_client_start(ws_);
}

void Client::start() {
    if (url_.empty()) return;  // not set up yet: the Setup flow calls configure()
    esp_websocket_client_config_t cfg = {};
    cfg.uri = url_.c_str();
    cfg.buffer_size = 4096;
    // shortcut: fixed 3 s auto-reconnect, not exponential backoff; upgrade in Phase 4 if the
    // server gets hammered after an outage.
    cfg.reconnect_timeout_ms = 3000;
    cfg.network_timeout_ms = 10000;
    ws_ = esp_websocket_client_init(&cfg);
    esp_websocket_register_events(ws_, WEBSOCKET_EVENT_ANY, &Client::on_ws_event, this);
    esp_websocket_client_start(ws_);
}

void Client::on_ws_event(void* arg, const char*, int32_t id, void* event_data) {
    auto* self = static_cast<Client*>(arg);
    auto* d = static_cast<esp_websocket_event_data_t*>(event_data);
    switch (id) {
        case WEBSOCKET_EVENT_CONNECTED:
            ESP_LOGI(TAG, "connected");
            self->post_([self] { self->session_.on_open(); });
            break;
        case WEBSOCKET_EVENT_DISCONNECTED:
        case WEBSOCKET_EVENT_CLOSED:
            ESP_LOGW(TAG, "disconnected");
            self->rx_.clear();
            self->post_([self] { self->session_.on_closed(); });
            break;
        case WEBSOCKET_EVENT_DATA: {
            if (d->op_code != 0x1 && d->op_code != 0x0) break;  // text + continuation only
            if (d->op_code == 0x1 && d->payload_offset == 0) self->rx_.clear();
            self->rx_.append(d->data_ptr, d->data_len);
            bool frame_done = d->payload_offset + d->data_len >= d->payload_len;
            if (frame_done && d->fin) {
                auto text = std::make_shared<std::string>(std::move(self->rx_));
                self->rx_.clear();
                self->post_([self, text] { self->session_.on_text(*text); });
            }
            break;
        }
        default:
            break;
    }
}

}  // namespace ma
