// Wires the panel together: the display and touch (Waveshare BSP + esp_lvgl_adapter), the core
// task that owns the App, and two queues between them: commands in (UI -> core) and updates out
// (core -> UI). An LVGL timer drains the updates and finished artwork on the GUI task.
#include <atomic>
#include <memory>
#include <variant>

#include "app/app.hpp"
#include "backend/ma_client.hpp"
#include "backend/mock_backend.hpp"
#include "board.hpp"
#include "board_art.hpp"
#include "bsp/esp-bsp.h"
#include "esp_lv_adapter.h"
#include "ui/ui.hpp"

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

void post_job(Job fn) {
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

std::atomic<app::App*> g_app{nullptr};  // set by the core task once the App exists; used only there

void core_task(void*) {
    static NvsSettings settings;
    static board::BoardPlatform platform(post_job);
#if CONFIG_PANEL_BACKEND_MA
    // Development shortcut: a URL in menuconfig skips first-time setup.
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
    g_app = &app;
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

// GUI task (LVGL timer, lock held): hand every update and finished image to the UI.
void drain_updates(lv_timer_t*) {
    app::Update* u = nullptr;
    while (xQueueReceive(g_updates, &u, 0) == pdTRUE) {
        ui::apply(std::move(*u));
        delete u;
    }
    board::art_drain();
}

void start_ui() {
    bsp_display_cfg_t cfg{};
    cfg.lv_adapter_cfg = ESP_LV_ADAPTER_DEFAULT_CONFIG();
    // The UI is built and rebuilt on the LVGL task (screens, the QR code, text layout): the default
    // 8 KB overflowed on the sign-in screen. Internal RAM, for speed.
    cfg.lv_adapter_cfg.task_stack_size = 32 * 1024;
    // The 10.1" panel is 800×1280 portrait: rotate to 1280×800 landscape; menuconfig picks 90° or
    // 270° to match how the panel hangs (270° is upright on the Waveshare board).
    // The adapter rotates the image in hardware (LVGL itself stays at 0°), so touch arrives in the
    // panel's portrait coordinates and is turned here: the GT911 driver mirrors, then swaps x/y.
    // Measured on the device at 270°: x = 1280 − raw y, y = raw x.
    cfg.touch_flags.swap_xy = 1;
#if CONFIG_PANEL_ROTATION_270
    cfg.rotation = ESP_LV_ADAPTER_ROTATE_270;
    cfg.touch_flags.mirror_y = 1;
#else
    cfg.rotation = ESP_LV_ADAPTER_ROTATE_90;
    cfg.touch_flags.mirror_x = 1;
#endif
    cfg.tear_avoid_mode = ESP_LV_ADAPTER_TEAR_AVOID_MODE_TRIPLE_PARTIAL;
    if (!bsp_display_start_with_config(&cfg)) {
        ESP_LOGE(TAG, "display did not start");
        return;
    }
    board::backlight_ready();  // the brightness the core already asked for (from settings)
    board::art_start();
    // Wait for the LVGL task to let go (the BSP passes this to esp_lv_adapter_lock, where -1 waits
    // forever and 0 does not wait at all). The UI must never be built without the lock.
    if (esp_lv_adapter_lock(-1) != ESP_OK) {
        ESP_LOGE(TAG, "could not take the LVGL lock");
        return;
    }
    ui::set_art_loader([](ui::ArtRequest r, auto done) { board::art_load(std::move(r), std::move(done)); }, 24u << 20);
    ui::start([](app::Command c) { post_job([c = std::move(c)]() mutable { g_app.load()->handle(std::move(c)); }); });
    lv_timer_create(drain_updates, 10, nullptr);
    esp_lv_adapter_unlock();
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
    while (!g_app) vTaskDelay(pdMS_TO_TICKS(5));  // the UI's first screen already sends commands
    start_ui();
}
