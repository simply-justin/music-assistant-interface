#pragma once
// Artwork: image-proxy URLs, scaling to the exact display size with pre-rounded corners (spec:
// no rounded clipping at draw time), and an LRU cache with pin counts so images on screen are
// never evicted. No LVGL and no I/O here: host-tested. Fetch + decode live in the platform.
#include <cstdint>
#include <list>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "core/model.hpp"

namespace img {

// The image proxy serves 80/160/256/512/1024 px (verified 2.10: other sizes → HTTP 400).
int proxy_size(int display_px);

// "http://host:port/imageproxy/<proxy_id>?size=N", or "" when the item has no proxy image.
std::string proxy_url(const std::string& http_base, const core::Image& image, int display_px);

struct Bitmap {
    uint16_t w = 0, h = 0;
    std::vector<uint16_t> rgb565;  // w*h pixels, row-major
    uint32_t average = 0;          // 0xRRGGBB, for the art-derived Now Playing background
    size_t bytes() const { return rgb565.size() * 2 + sizeof(Bitmap); }
};

// Now Playing background from an artwork colour: HSL lightness ≤ 16 %, saturation ≤ 45 %, so
// text stays readable on any artwork (spec §1). 0xRRGGBB in and out.
uint32_t art_background(uint32_t rgb);

// `bg` value meaning "the art's own art_background()" (Now Playing sits on it).
constexpr uint32_t kBgFromArt = 0xFF000000;

// Scales an RGB888 image (any size) to w×h RGB565: box filter when shrinking, nearest when
// growing. Corners of `radius` px are filled with `bg` (0xRRGGBB): the surface the art sits on.
std::shared_ptr<Bitmap> scale(const uint8_t* rgb888, int sw, int sh, int w, int h, int radius, uint32_t bg);

class Cache {
public:
    explicit Cache(size_t budget_bytes) : budget_(budget_bytes) {}

    std::shared_ptr<Bitmap> get(const std::string& key);  // marks as recently used
    void put(const std::string& key, std::shared_ptr<Bitmap> bitmap);
    void pin(const std::string& key);    // on screen: never evicted
    void unpin(const std::string& key);
    size_t bytes() const { return bytes_; }
    size_t size() const { return map_.size(); }

private:
    struct Entry {
        std::shared_ptr<Bitmap> bitmap;
        int pins = 0;
        std::list<std::string>::iterator lru;
    };
    void evict();

    size_t budget_, bytes_ = 0;
    std::list<std::string> lru_;  // front = most recent
    std::unordered_map<std::string, Entry> map_;
};

}  // namespace img
