#pragma once
// PC artwork loader: two worker threads fetch from the image proxy (plain HTTP), decode with
// LVGL's TJpgDec and scale with img::scale. Results are handed back on the GUI thread by
// drain(). The board does the same with esp_http_client + the hardware JPEG decoder.
#include <condition_variable>
#include <deque>
#include <mutex>
#include <thread>
#include <vector>

#include "ui/ui.hpp"

class SimArtLoader {
public:
    SimArtLoader();
    ~SimArtLoader();
    void load(ui::ArtRequest req, std::function<void(std::shared_ptr<img::Bitmap>)> done);
    void drain();  // GUI thread: deliver finished images

private:
    struct Job {
        ui::ArtRequest req;
        std::function<void(std::shared_ptr<img::Bitmap>)> done;
        std::shared_ptr<img::Bitmap> result;
    };
    void work();

    std::mutex mu_;
    std::condition_variable cv_;
    std::deque<Job> todo_, finished_;
    std::vector<std::thread> threads_;
    bool stop_ = false;
};
