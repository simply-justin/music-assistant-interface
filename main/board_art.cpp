#include "board_art.hpp"

#include <cstring>
#include <deque>
#include <mutex>
#include <vector>

#include "driver/jpeg_decode.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

namespace board {

namespace {

constexpr const char* TAG = "art";
constexpr size_t kMaxJpeg = 1024 * 1024;  // the 512 px proxy images are ~30–150 KB

struct Job {
    ui::ArtRequest req;
    std::function<void(std::shared_ptr<img::Bitmap>)> done;
    std::shared_ptr<img::Bitmap> result;
};

std::mutex g_mu;
std::deque<Job> g_todo, g_finished;
SemaphoreHandle_t g_wake;
jpeg_decoder_handle_t g_decoder;
std::mutex g_decoder_mu;  // one hardware engine: decodes take turns

// Fetches the body into a buffer the JPEG engine can read (DMA-capable, aligned).
uint8_t* fetch(const std::string& url, size_t& size) {
    esp_http_client_config_t c{};
    c.url = url.c_str();
    c.timeout_ms = 5000;
    c.buffer_size = 4096;
    esp_http_client_handle_t h = esp_http_client_init(&c);
    if (!h) return nullptr;
    std::vector<uint8_t> body;
    bool ok = esp_http_client_open(h, 0) == ESP_OK && esp_http_client_fetch_headers(h) >= 0 &&
              esp_http_client_get_status_code(h) == 200;
    uint8_t buf[4096];
    for (int n; ok && (n = esp_http_client_read(h, reinterpret_cast<char*>(buf), sizeof buf)) > 0;) {
        if (body.size() + static_cast<size_t>(n) > kMaxJpeg) ok = false;
        else body.insert(body.end(), buf, buf + n);
    }
    esp_http_client_cleanup(h);
    if (!ok || body.empty()) return nullptr;
    jpeg_decode_memory_alloc_cfg_t in{};
    in.buffer_direction = JPEG_DEC_ALLOC_INPUT_BUFFER;
    size_t cap = 0;
    auto* out = static_cast<uint8_t*>(jpeg_alloc_decoder_mem(body.size(), &in, &cap));
    if (!out) return nullptr;
    std::memcpy(out, body.data(), body.size());
    size = body.size();
    return out;
}

std::shared_ptr<img::Bitmap> fetch_and_decode(const ui::ArtRequest& r) {
    size_t size = 0;
    uint8_t* jpeg = fetch(r.url, size);
    if (!jpeg) return nullptr;
    std::shared_ptr<img::Bitmap> bitmap;
    jpeg_decode_picture_info_t info{};
    if (jpeg_decoder_get_info(jpeg, size, &info) == ESP_OK && info.width && info.height) {
        // The engine writes whole 16×16 blocks: rows are padded to a multiple of 16.
        uint32_t stride = (info.width + 15) & ~15u, rows = (info.height + 15) & ~15u;
        jpeg_decode_memory_alloc_cfg_t out_cfg{};
        out_cfg.buffer_direction = JPEG_DEC_ALLOC_OUTPUT_BUFFER;
        size_t cap = 0;
        auto* rgb = static_cast<uint8_t*>(jpeg_alloc_decoder_mem(stride * rows * 3, &out_cfg, &cap));
        if (rgb) {
            jpeg_decode_cfg_t cfg{};
            cfg.output_format = JPEG_DECODE_OUT_FORMAT_RGB888;
            cfg.rgb_order = JPEG_DEC_RGB_ELEMENT_ORDER_RGB;
            cfg.conv_std = JPEG_YUV_RGB_CONV_STD_BT601;
            uint32_t written = 0;
            esp_err_t err;
            {
                std::lock_guard<std::mutex> l(g_decoder_mu);
                err = jpeg_decoder_process(g_decoder, &cfg, jpeg, size, rgb, cap, &written);
            }
            if (err == ESP_OK) {
                if (stride != info.width)  // pack the rows for img::scale
                    for (uint32_t y = 1; y < info.height; ++y)
                        std::memmove(rgb + y * info.width * 3, rgb + y * stride * 3, info.width * 3);
                bitmap = img::scale(rgb, static_cast<int>(info.width), static_cast<int>(info.height), r.w, r.h, r.radius, r.bg);
            } else {
                ESP_LOGW(TAG, "decode failed (%s): %s", esp_err_to_name(err), r.url.c_str());  // e.g. progressive JPEG
            }
            free(rgb);
        }
    }
    free(jpeg);
    return bitmap;
}

void worker(void*) {
    for (;;) {
        xSemaphoreTake(g_wake, portMAX_DELAY);
        for (;;) {
            Job job;
            {
                std::lock_guard<std::mutex> l(g_mu);
                if (g_todo.empty()) break;
                job = std::move(g_todo.front());
                g_todo.pop_front();
            }
            job.result = fetch_and_decode(job.req);
            std::lock_guard<std::mutex> l(g_mu);
            g_finished.push_back(std::move(job));
        }
    }
}

}  // namespace

void art_start() {
    jpeg_decode_engine_cfg_t eng{};
    eng.timeout_ms = 100;
    ESP_ERROR_CHECK(jpeg_new_decoder_engine(&eng, &g_decoder));
    g_wake = xSemaphoreCreateCounting(64, 0);
    for (int i = 0; i < 2; ++i) xTaskCreate(worker, "art", 6 * 1024, nullptr, 3, nullptr);
}

void art_load(ui::ArtRequest req, std::function<void(std::shared_ptr<img::Bitmap>)> done) {
    {
        std::lock_guard<std::mutex> l(g_mu);
        g_todo.push_back({std::move(req), std::move(done), nullptr});
    }
    xSemaphoreGive(g_wake);
}

void art_drain() {
    std::deque<Job> done;
    {
        std::lock_guard<std::mutex> l(g_mu);
        done.swap(g_finished);
    }
    for (auto& j : done) j.done(std::move(j.result));
}

}  // namespace board
