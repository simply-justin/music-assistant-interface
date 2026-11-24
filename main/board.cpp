// Board services for the Waveshare ESP32-P4-WIFI6-Touch-LCD-10.1. Written against ESP-IDF 5.5 and
// the Waveshare BSP; it compiles in CI but is not yet verified on the hardware (see
// docs/architecture.md, Known limitations).
#include "board.hpp"

#include <algorithm>
#include <cstring>
#include <ctime>
#include <mutex>

#include "bsp/esp-bsp.h"
#include "esp_app_desc.h"
#include "esp_event.h"
#include "esp_heap_caps.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_netif_sntp.h"
#include "esp_random.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "i18n/i18n.hpp"
#include "mdns.h"
#include "pairing/pairing.hpp"
#include "sdkconfig.h"

namespace board {

namespace {
constexpr const char* TAG = "board";
constexpr int kJoinTimeoutMs = 20000;
constexpr int kReconnectMs = 5000;
}  // namespace

struct BoardPlatform::State {
    Post post;
    std::mutex mu;
    esp_netif_t* netif = nullptr;
    esp_timer_handle_t join_timer = nullptr, retry_timer = nullptr;

    // Guarded by mu.
    std::function<void(std::vector<app::WifiNetwork>)> scan_done;
    std::function<void(app::WifiResult)> join_done;
    bool connected = false;
    std::string ip;

    httpd_handle_t http = nullptr;
    std::string code;  // pairing page path
    std::function<void(std::string)> on_token;

    void finish_join(app::WifiResult r) {
        std::function<void(app::WifiResult)> done;
        {
            std::lock_guard<std::mutex> l(mu);
            done = std::move(join_done);
            join_done = nullptr;
        }
        esp_timer_stop(join_timer);
        if (done) post([done, r] { done(r); });
    }
};

namespace {

app::WifiResult result_for(uint8_t reason) {
    switch (reason) {
        case WIFI_REASON_AUTH_FAIL:
        case WIFI_REASON_4WAY_HANDSHAKE_TIMEOUT:
        case WIFI_REASON_HANDSHAKE_TIMEOUT:
        case WIFI_REASON_MIC_FAILURE:
            return app::WifiResult::WrongPassword;
        case WIFI_REASON_NO_AP_FOUND:
        case WIFI_REASON_NO_AP_FOUND_W_COMPATIBLE_SECURITY:
        case WIFI_REASON_NO_AP_FOUND_IN_AUTHMODE_THRESHOLD:
        case WIFI_REASON_NO_AP_FOUND_IN_RSSI_THRESHOLD:
            return app::WifiResult::NotFound;
        default:
            return app::WifiResult::Failed;
    }
}

void on_wifi(void* arg, esp_event_base_t, int32_t id, void* data) {
    auto* s = static_cast<BoardPlatform::State*>(arg);
    switch (id) {
        case WIFI_EVENT_STA_START: {
            wifi_config_t c{};
            if (esp_wifi_get_config(WIFI_IF_STA, &c) == ESP_OK && c.sta.ssid[0]) esp_wifi_connect();  // saved network
            break;
        }
        case WIFI_EVENT_SCAN_DONE: {
            uint16_t n = 0;
            esp_wifi_scan_get_ap_num(&n);
            std::vector<wifi_ap_record_t> recs(n);
            if (n) esp_wifi_scan_get_ap_records(&n, recs.data());
            std::vector<app::WifiNetwork> out;
            for (uint16_t i = 0; i < n; ++i) {
                std::string ssid(reinterpret_cast<const char*>(recs[i].ssid));
                if (ssid.empty()) continue;  // hidden network
                auto same = std::find_if(out.begin(), out.end(), [&](const app::WifiNetwork& w) { return w.ssid == ssid; });
                if (same != out.end()) {  // one row per name: keep the strongest access point
                    same->rssi = std::max(same->rssi, static_cast<int>(recs[i].rssi));
                    continue;
                }
                out.push_back({ssid, recs[i].rssi, recs[i].authmode != WIFI_AUTH_OPEN});
            }
            std::sort(out.begin(), out.end(), [](const auto& a, const auto& b) { return a.rssi > b.rssi; });
            std::function<void(std::vector<app::WifiNetwork>)> done;
            {
                std::lock_guard<std::mutex> l(s->mu);
                done = std::move(s->scan_done);
                s->scan_done = nullptr;
            }
            if (done) s->post([done, out] { done(out); });
            break;
        }
        case WIFI_EVENT_STA_DISCONNECTED: {
            auto* ev = static_cast<wifi_event_sta_disconnected_t*>(data);
            bool joining;
            {
                std::lock_guard<std::mutex> l(s->mu);
                s->connected = false;
                s->ip.clear();
                joining = static_cast<bool>(s->join_done);
            }
            if (joining) {
                if (ev->reason != WIFI_REASON_ASSOC_LEAVE) s->finish_join(result_for(ev->reason));  // ASSOC_LEAVE: our own disconnect
            } else {
                esp_timer_start_once(s->retry_timer, kReconnectMs * 1000ULL);  // keep trying the saved network
            }
            break;
        }
        default:
            break;
    }
}

void on_ip(void* arg, esp_event_base_t, int32_t, void* data) {
    auto* s = static_cast<BoardPlatform::State*>(arg);
    auto* ev = static_cast<ip_event_got_ip_t*>(data);
    char buf[16];
    esp_ip4addr_ntoa(&ev->ip_info.ip, buf, sizeof buf);
    {
        std::lock_guard<std::mutex> l(s->mu);
        s->connected = true;
        s->ip = buf;
    }
    ESP_LOGI(TAG, "Wi-Fi up, %s", buf);
    s->finish_join(app::WifiResult::Ok);
}

esp_err_t send_page(httpd_req_t* req, const std::string& html) {
    httpd_resp_set_type(req, "text/html; charset=utf-8");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    return httpd_resp_send(req, html.data(), static_cast<ssize_t>(html.size()));
}

const char* lang_code() { return i18n::kLangCodes[static_cast<int>(i18n::language())]; }

esp_err_t pairing_get(httpd_req_t* req) { return send_page(req, pairing::form_page(lang_code())); }

esp_err_t pairing_post(httpd_req_t* req) {
    auto* s = static_cast<BoardPlatform::State*>(req->user_ctx);
    if (req->content_len > pairing::kMaxToken * 3 + 16) return send_page(req, pairing::form_page(lang_code(), true));
    std::string body(req->content_len, '\0');
    size_t got = 0;
    while (got < body.size()) {
        int n = httpd_req_recv(req, body.data() + got, body.size() - got);
        if (n == HTTPD_SOCK_ERR_TIMEOUT) continue;
        if (n <= 0) return ESP_FAIL;
        got += static_cast<size_t>(n);
    }
    auto token = pairing::token_from_form(body);
    if (!token) return send_page(req, pairing::form_page(lang_code(), true));
    std::function<void(std::string)> on_token;
    {
        std::lock_guard<std::mutex> l(s->mu);
        on_token = s->on_token;
    }
    if (on_token) s->post([on_token, t = *token] { on_token(t); });
    return send_page(req, pairing::thanks_page(lang_code()));
}

}  // namespace

BoardPlatform::BoardPlatform(Post post) : s_(std::make_unique<State>()) {
    s_->post = std::move(post);

    // Local time: SNTP once the network is up, the zone from menuconfig.
    setenv("TZ", CONFIG_PANEL_TIMEZONE, 1);
    tzset();

    ESP_ERROR_CHECK(esp_netif_init());
    esp_err_t err = esp_event_loop_create_default();
    if (err != ESP_ERR_INVALID_STATE) ESP_ERROR_CHECK(err);
    s_->netif = esp_netif_create_default_wifi_sta();
    esp_timer_create_args_t join{};
    join.callback = [](void* arg) { static_cast<State*>(arg)->finish_join(app::WifiResult::Failed); };
    join.arg = s_.get();
    join.name = "wifi_join";
    ESP_ERROR_CHECK(esp_timer_create(&join, &s_->join_timer));
    esp_timer_create_args_t retry{};
    retry.callback = [](void*) { esp_wifi_connect(); };
    retry.name = "wifi_retry";
    ESP_ERROR_CHECK(esp_timer_create(&retry, &s_->retry_timer));

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &on_wifi, s_.get(), nullptr));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &on_ip, s_.get(), nullptr));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());

    esp_sntp_config_t sntp = ESP_NETIF_SNTP_DEFAULT_CONFIG("pool.ntp.org");
    esp_netif_sntp_init(&sntp);

    if (mdns_init() == ESP_OK) mdns_hostname_set("music-panel");
}

BoardPlatform::~BoardPlatform() { pairing_stop(); }

void BoardPlatform::wifi_scan(std::function<void(std::vector<app::WifiNetwork>)> done) {
    {
        std::lock_guard<std::mutex> l(s_->mu);
        s_->scan_done = std::move(done);
    }
    if (esp_wifi_scan_start(nullptr, false) != ESP_OK) {  // e.g. busy connecting: report an empty list
        std::function<void(std::vector<app::WifiNetwork>)> d;
        {
            std::lock_guard<std::mutex> l(s_->mu);
            d = std::move(s_->scan_done);
            s_->scan_done = nullptr;
        }
        if (d) s_->post([d] { d({}); });
    }
}

void BoardPlatform::wifi_join(const std::string& ssid, const std::string& password, std::function<void(app::WifiResult)> done) {
    wifi_config_t c{};
    std::strncpy(reinterpret_cast<char*>(c.sta.ssid), ssid.c_str(), sizeof c.sta.ssid - 1);
    std::strncpy(reinterpret_cast<char*>(c.sta.password), password.c_str(), sizeof c.sta.password - 1);
    c.sta.threshold.authmode = password.empty() ? WIFI_AUTH_OPEN : WIFI_AUTH_WPA_PSK;
    c.sta.pmf_cfg.capable = true;
    {
        std::lock_guard<std::mutex> l(s_->mu);
        s_->join_done = std::move(done);
    }
    esp_timer_stop(s_->retry_timer);
    esp_wifi_disconnect();
    if (esp_wifi_set_config(WIFI_IF_STA, &c) != ESP_OK || esp_wifi_connect() != ESP_OK) {
        s_->finish_join(app::WifiResult::Failed);
        return;
    }
    esp_timer_start_once(s_->join_timer, kJoinTimeoutMs * 1000ULL);
}

app::WifiStatus BoardPlatform::wifi_status() {
    app::WifiStatus st;
    {
        std::lock_guard<std::mutex> l(s_->mu);
        st.connected = s_->connected;
        st.ip = s_->ip;
    }
    wifi_ap_record_t ap{};
    if (st.connected && esp_wifi_sta_get_ap_info(&ap) == ESP_OK) {
        st.ssid = reinterpret_cast<const char*>(ap.ssid);
        st.rssi = ap.rssi;
    }
    return st;
}

void BoardPlatform::discover_servers(std::function<void(std::vector<app::ServerCandidate>)> done) {
    // mdns_query_ptr blocks for its timeout: run it on a short-lived task.
    struct Job {
        Post post;
        std::function<void(std::vector<app::ServerCandidate>)> done;
    };
    auto* job = new Job{s_->post, std::move(done)};
    auto run = [](void* arg) {
        std::unique_ptr<Job> j(static_cast<Job*>(arg));
        std::vector<app::ServerCandidate> out;
        mdns_result_t* results = nullptr;
        if (mdns_query_ptr("_mass", "_tcp", 3000, 8, &results) == ESP_OK) {
            for (mdns_result_t* r = results; r; r = r->next) {
                for (mdns_ip_addr_t* a = r->addr; a; a = a->next) {
                    if (a->addr.type != ESP_IPADDR_TYPE_V4) continue;
                    char ip[16];
                    esp_ip4addr_ntoa(&a->addr.u_addr.ip4, ip, sizeof ip);
                    std::string address = std::string(ip) + ":" + std::to_string(r->port);
                    if (std::none_of(out.begin(), out.end(), [&](const auto& c) { return c.address == address; }))
                        out.push_back({"Music Assistant", address});
                    break;
                }
            }
            mdns_query_results_free(results);
        }
        auto done = std::move(j->done);
        j->post([done, out] { done(out); });
        j.reset();
        vTaskDelete(nullptr);
    };
    if (xTaskCreate(run, "mdns_query", 4096, job, 3, nullptr) != pdPASS) {
        auto d = std::move(job->done);
        delete job;
        s_->post([d] { d({}); });
    }
}

std::string BoardPlatform::pairing_start(std::function<void(std::string)> on_token) {
    pairing_stop();
    std::string ip = wifi_status().ip;
    if (ip.empty()) return {};  // no network: setup falls back to typing the token
    std::string code = pairing::new_code([] { return esp_random(); });
    httpd_config_t cfg = HTTPD_DEFAULT_CONFIG();
    cfg.server_port = 80;
    cfg.lru_purge_enable = true;
    httpd_handle_t h = nullptr;
    if (httpd_start(&h, &cfg) != ESP_OK) return {};
    {
        std::lock_guard<std::mutex> l(s_->mu);
        s_->http = h;
        s_->code = "/" + code;
        s_->on_token = std::move(on_token);
    }
    httpd_uri_t get{};
    get.uri = s_->code.c_str();  // httpd keeps the pointer: s_->code lives until pairing_stop()
    get.method = HTTP_GET;
    get.handler = pairing_get;
    get.user_ctx = s_.get();
    httpd_register_uri_handler(h, &get);
    httpd_uri_t post = get;
    post.method = HTTP_POST;
    post.handler = pairing_post;
    httpd_register_uri_handler(h, &post);
    ESP_LOGI(TAG, "pairing page at http://%s/%s", ip.c_str(), code.c_str());
    return "http://" + ip + "/" + code;
}

void BoardPlatform::pairing_stop() {
    httpd_handle_t h;
    {
        std::lock_guard<std::mutex> l(s_->mu);
        h = s_->http;
        s_->http = nullptr;
        s_->on_token = nullptr;
    }
    if (h) httpd_stop(h);
}

void BoardPlatform::set_brightness(int percent) { bsp_display_brightness_set(std::clamp(percent, 0, 100)); }

app::SystemInfo BoardPlatform::system_info() {
    return {esp_app_get_description()->version, static_cast<uint32_t>(esp_timer_get_time() / 1000000),
            static_cast<uint32_t>(heap_caps_get_free_size(MALLOC_CAP_INTERNAL) / 1024),
            static_cast<uint32_t>(heap_caps_get_free_size(MALLOC_CAP_SPIRAM) / 1024)};
}

int BoardPlatform::local_minutes() {
    std::time_t t = std::time(nullptr);
    std::tm lt{};
    localtime_r(&t, &lt);
    if (lt.tm_year < 2024 - 1900) return 12 * 60;  // clock not synced yet: midday, so night mode stays off
    return lt.tm_hour * 60 + lt.tm_min;
}

void BoardPlatform::restart() { esp_restart(); }

}  // namespace board
