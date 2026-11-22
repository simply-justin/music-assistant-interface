// Wires the core to FreeRTOS: one core task owns the App; two queues carry commands in
// (UI -> core) and updates out (core -> UI). The UI task arrives with the approved design;
// until then a stub drains and logs the updates.
#include <memory>
#include <variant>

#include "app/app.hpp"
#include "backend/ma_client.hpp"
#include "backend/mock_backend.hpp"
#include <ctime>

#include "esp_app_desc.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "sdkconfig.h"

namespace {

constexpr const char* TAG = "panel";
using Job = std::function<void()>;

QueueHandle_t g_jobs;     // Job*: commands, socket data, anything that must run on the core task
QueueHandle_t g_updates;  // app::Update*: drained by the UI

uint32_t now_ms() { return static_cast<uint32_t>(esp_timer_get_time() / 1000); }

[[maybe_unused]] void post_job(Job fn) {  // unused with the mock backend
    auto* job = new Job(std::move(fn));
    if (xQueueSend(g_jobs, &job, pdMS_TO_TICKS(50)) != pdTRUE) {
        ESP_LOGE(TAG, "core queue full, job dropped");
        delete job;
    }
}

void publish(app::Update u) {
    auto* item = new app::Update(std::move(u));
    if (xQueueSend(g_updates, &item, 0) != pdTRUE) {
        ESP_LOGW(TAG, "UI queue full, update dropped");  // the next snapshot supersedes it
        delete item;
    }
}

class NvsSettings : public app::Settings {
public:
    NvsSettings() { ESP_ERROR_CHECK(nvs_open("panel", NVS_READWRITE, &h_)); }

    std::string get(const std::string& key, const std::string& fallback = "") override {
        size_t len = 0;
        if (nvs_get_str(h_, key.c_str(), nullptr, &len) != ESP_OK || len == 0) return fallback;
        std::string v(len, '\0');
        if (nvs_get_str(h_, key.c_str(), v.data(), &len) != ESP_OK) return fallback;
        v.resize(len - 1);  // drop the terminating NUL
        return v;
    }
    void set(const std::string& key, const std::string& value) override {
        if (nvs_set_str(h_, key.c_str(), value.c_str()) == ESP_OK) nvs_commit(h_);
        else ESP_LOGE(TAG, "could not save setting %s", key.c_str());
    }
    void erase_all() override {
        nvs_erase_all(h_);
        nvs_commit(h_);
    }

private:
    nvs_handle_t h_{};
};

// shortcut: Wi-Fi (ESP32-C6 via esp_wifi_remote), mDNS discovery, the pairing web page and the
// backlight arrive with the board package in Phase 2b; until then they report "not available".
class BoardPlatform : public app::Platform {
public:
    void wifi_scan(std::function<void(std::vector<app::WifiNetwork>)> done) override { done({}); }
    void wifi_join(const std::string&, const std::string&, std::function<void(app::WifiResult)> done) override {
        done(app::WifiResult::Failed);
    }
    app::WifiStatus wifi_status() override { return {}; }
    void discover_servers(std::function<void(std::vector<app::ServerCandidate>)> done) override { done({}); }
    std::string pairing_start(std::function<void(std::string)>) override { return {}; }
    void pairing_stop() override {}
    void set_brightness(int percent) override { ESP_LOGI(TAG, "brightness %d %%", percent); }
    app::SystemInfo system_info() override {
        return {esp_app_get_description()->version, static_cast<uint32_t>(esp_timer_get_time() / 1000000),
                static_cast<uint32_t>(heap_caps_get_free_size(MALLOC_CAP_INTERNAL) / 1024),
                static_cast<uint32_t>(heap_caps_get_free_size(MALLOC_CAP_SPIRAM) / 1024)};
    }
    int local_minutes() override {
        std::time_t t = std::time(nullptr);  // shortcut: needs SNTP + timezone (Phase 2b) to be local time
        std::tm lt{};
        localtime_r(&t, &lt);
        return lt.tm_hour * 60 + lt.tm_min;
    }
    void restart() override { esp_restart(); }
};

void core_task(void*) {
    static NvsSettings settings;
    static BoardPlatform platform;
#if CONFIG_PANEL_BACKEND_MA
    // Development shortcut until the Setup screens run on the board: seed from menuconfig.
    if (settings.get("server_url").empty() && CONFIG_PANEL_MA_URL[0]) {
        settings.set("server_url", CONFIG_PANEL_MA_URL);
        settings.set("token", CONFIG_PANEL_MA_TOKEN);
        settings.set("setup_done", "1");
    }
    static ma::Client backend("", "", settings.get("server_id"), post_job);  // App configures it
#else
    static backend::MockBackend backend;
#endif
    static app::App app(backend, settings, platform, publish);
    app.start();
    for (;;) {
        Job* job = nullptr;
        // Short wait keeps tick() (debounce, mock latency) at ~10 ms resolution.
        while (xQueueReceive(g_jobs, &job, pdMS_TO_TICKS(10)) == pdTRUE) {
            (*job)();
            delete job;
            app.tick(now_ms());
        }
        app.tick(now_ms());
    }
}

// shortcut: logs updates until the LVGL UI exists; replaced by an lv_timer drain in Phase 3.
void ui_stub_task(void*) {
    for (;;) {
        app::Update* u = nullptr;
        if (xQueueReceive(g_updates, &u, portMAX_DELAY) != pdTRUE) continue;
        if (auto* c = std::get_if<app::ConnectionView>(u))
            ESP_LOGI(TAG, "connection %d, server %s", static_cast<int>(c->state), c->server.server_version.c_str());
        else if (auto* p = std::get_if<std::shared_ptr<const app::PlayersView>>(u))
            ESP_LOGI(TAG, "%u players, selected %s", static_cast<unsigned>((*p)->players.size()), (*p)->selected_id.c_str());
        else if (auto* q = std::get_if<std::shared_ptr<const core::QueueState>>(u))
            ESP_LOGI(TAG, "queue: %u items", static_cast<unsigned>((*q)->items.size()));
        else if (auto* t = std::get_if<app::Toast>(u))
            ESP_LOGI(TAG, "toast: %s", t->text.c_str());
        delete u;
    }
}

}  // namespace

extern "C" void app_main() {
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

    g_jobs = xQueueCreate(32, sizeof(Job*));
    g_updates = xQueueCreate(32, sizeof(app::Update*));
    xTaskCreate(core_task, "core", 12 * 1024, nullptr, 5, nullptr);
    xTaskCreate(ui_stub_task, "ui_stub", 4 * 1024, nullptr, 4, nullptr);
}
