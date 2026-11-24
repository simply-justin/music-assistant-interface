#pragma once
// Artwork on the board: two worker tasks fetch from Music Assistant's image proxy
// (esp_http_client), decode with the ESP32-P4's hardware JPEG decoder and scale with img::scale.
// Finished images are handed back on the GUI task by drain(), like the simulator's loader.
#include <functional>
#include <memory>

#include "ui/ui.hpp"

namespace board {

void art_start();
void art_load(ui::ArtRequest req, std::function<void(std::shared_ptr<img::Bitmap>)> done);
void art_drain();  // GUI task (LVGL lock held): deliver finished images

}  // namespace board
