// Keeps the ESP32-C6 Wi-Fi co-processor on the same ESP-Hosted version as this firmware. Boards
// ship with co-processor firmware that predates ESP-Hosted 3 (it reports 0.0.0), and with it Wi-Fi
// does not work. When the co-processor's major version is behind, the embedded image
// (main/coprocessor/) is sent over the existing SDIO link, activated, and both chips restart.
#include "board_cp.hpp"

#include <algorithm>
#include <cinttypes>

#include "eh_host_cp_ota.h"
#include "eh_host_sys.h"
#include "esp_hosted_host_fw_ver.h"
#include "esp_log.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs.h"

extern const uint8_t cp_image_start[] asm("_binary_esp_hosted_cp_esp32c6_bin_start");
extern const uint8_t cp_image_end[] asm("_binary_esp_hosted_cp_esp32c6_bin_end");

namespace board {

namespace {

constexpr const char* TAG = "coprocessor";
constexpr size_t kChunk = 1500;  // as the esp_hosted OTA example
constexpr uint8_t kMaxTries = 2;  // never loop: after two failed updates, stay on the old firmware

// Update attempts, kept across restarts (NVS namespace "panel_cp").
uint8_t tries(bool increment) {
    nvs_handle_t h;
    uint8_t n = 0;
    if (nvs_open("panel_cp", NVS_READWRITE, &h) != ESP_OK) return kMaxTries;
    nvs_get_u8(h, "tries", &n);
    if (increment) {
        nvs_set_u8(h, "tries", static_cast<uint8_t>(n + 1));
        nvs_commit(h);
    }
    nvs_close(h);
    return n;
}

void reset_tries() {
    nvs_handle_t h;
    if (nvs_open("panel_cp", NVS_READWRITE, &h) != ESP_OK) return;
    nvs_erase_key(h, "tries");
    nvs_commit(h);
    nvs_close(h);
}

bool send_image() {
    size_t size = static_cast<size_t>(cp_image_end - cp_image_start);
    ESP_LOGW(TAG, "updating the Wi-Fi co-processor (%u bytes), do not switch off", static_cast<unsigned>(size));
    if (eh_host_cp_ota_begin() != ESP_OK) {
        ESP_LOGE(TAG, "update could not start");
        return false;
    }
    for (size_t off = 0; off < size; off += kChunk) {
        uint32_t n = static_cast<uint32_t>(std::min(kChunk, size - off));
        if (eh_host_cp_ota_write(cp_image_start + off, n) != ESP_OK) {
            ESP_LOGE(TAG, "update failed at %u bytes", static_cast<unsigned>(off));
            eh_host_cp_ota_end();
            return false;
        }
        if ((off / kChunk) % 100 == 0) ESP_LOGI(TAG, "%u %%", static_cast<unsigned>(off * 100 / size));
    }
    if (eh_host_cp_ota_end() != ESP_OK) {
        ESP_LOGE(TAG, "update did not verify");
        return false;
    }
    return true;
}

void run(void*) {
    eh_host_coprocessor_fwver_t v{};
    bool known = eh_host_sys_get_cp_fw_version(&v) == ESP_OK;
    ESP_LOGI(TAG, "co-processor %" PRIu32 ".%" PRIu32 ".%" PRIu32 ", firmware carries %d.%d.%d", v.major1, v.minor1,
             v.patch1, ESP_HOSTED_VERSION_MAJOR_1, ESP_HOSTED_VERSION_MINOR_1, ESP_HOSTED_VERSION_PATCH_1);
    if (known && v.major1 >= static_cast<uint32_t>(ESP_HOSTED_VERSION_MAJOR_1)) {
        reset_tries();
        vTaskDelete(nullptr);
        return;
    }
    if (tries(true) >= kMaxTries) {
        ESP_LOGE(TAG, "co-processor update failed twice; Wi-Fi stays unavailable until it is updated by hand");
        vTaskDelete(nullptr);
        return;
    }
    if (send_image()) {
        // Firmware before 2.6 switches by itself on end(); newer firmware needs activate().
        if (known && (v.major1 > 2 || (v.major1 == 2 && v.minor1 > 5))) eh_host_cp_ota_activate();
        ESP_LOGW(TAG, "co-processor updated; restarting");
        vTaskDelay(pdMS_TO_TICKS(2000));  // let the co-processor boot its new image before we reset it
        esp_restart();
    }
    vTaskDelete(nullptr);
}

}  // namespace

void update_coprocessor_if_needed() { xTaskCreate(run, "cp_update", 6 * 1024, nullptr, 4, nullptr); }

}  // namespace board
