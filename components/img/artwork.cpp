#include "img/artwork.hpp"

#include <algorithm>
#include <cmath>

namespace img {

int proxy_size(int px) {
    // Up to 10 % upscaling is invisible and saves a size step (520 px Now Playing → 512, not 1024).
    for (int s : {80, 160, 256, 512}) if (px <= s + s / 10) return s;
    return 1024;
}

std::string proxy_url(const std::string& base, const core::Image& image, int px) {
    if (image.proxy_id.empty() || base.empty()) return {};
    return base + "/imageproxy/" + image.proxy_id + "?size=" + std::to_string(proxy_size(px));
}

namespace {
// HSL clamp: saturation ≤ max_s, lightness kept within [min_l, max_l].
uint32_t clamp_hsl(uint32_t rgb, float max_s, float min_l, float max_l) {
    float r = (rgb >> 16 & 0xFF) / 255.f, g = (rgb >> 8 & 0xFF) / 255.f, b = (rgb & 0xFF) / 255.f;
    float mx = std::max({r, g, b}), mn = std::min({r, g, b});
    float l = (mx + mn) / 2, d = mx - mn, h = 0, s = 0;
    if (d > 0) {
        s = d / (1 - std::abs(2 * l - 1));
        h = mx == r ? std::fmod((g - b) / d, 6.f) : mx == g ? (b - r) / d + 2 : (r - g) / d + 4;
        h = h * 60 < 0 ? h * 60 + 360 : h * 60;
    }
    s = std::min(s, max_s);
    l = std::clamp(l, min_l, max_l);
    float c = (1 - std::abs(2 * l - 1)) * s, x = c * (1 - std::abs(std::fmod(h / 60, 2.f) - 1)), m = l - c / 2;
    float rr = 0, gg = 0, bb = 0;
    if (h < 60) rr = c, gg = x; else if (h < 120) rr = x, gg = c; else if (h < 180) gg = c, bb = x;
    else if (h < 240) gg = x, bb = c; else if (h < 300) rr = x, bb = c; else rr = c, bb = x;
    auto byte = [](float v) { return static_cast<uint32_t>(std::lround(std::clamp(v, 0.f, 1.f) * 255)); };
    return byte(rr + m) << 16 | byte(gg + m) << 8 | byte(bb + m);
}
}  // namespace

uint32_t art_background(uint32_t rgb) { return clamp_hsl(rgb, 0.45f, 0.f, 0.16f); }
uint32_t art_background_light(uint32_t rgb) { return clamp_hsl(rgb, 0.35f, 0.90f, 1.f); }

namespace {
uint16_t to565(uint32_t r, uint32_t g, uint32_t b) {
    return static_cast<uint16_t>((r >> 3) << 11 | (g >> 2) << 5 | (b >> 3));
}
}  // namespace

std::shared_ptr<Bitmap> scale(const uint8_t* src, int sw, int sh, int w, int h, int radius, uint32_t bg) {
    auto out = std::make_shared<Bitmap>();
    out->w = static_cast<uint16_t>(w);
    out->h = static_cast<uint16_t>(h);
    out->rgb565.resize(static_cast<size_t>(w) * h);
    uint64_t sum[3] = {0, 0, 0};
    for (int y = 0; y < h; ++y) {
        int y0 = y * sh / h, y1 = std::max(y0 + 1, (y + 1) * sh / h);
        for (int x = 0; x < w; ++x) {
            int x0 = x * sw / w, x1 = std::max(x0 + 1, (x + 1) * sw / w);
            uint32_t c[3] = {0, 0, 0}, n = 0;
            for (int yy = y0; yy < y1; ++yy)
                for (int xx = x0; xx < x1; ++xx, ++n)
                    for (int k = 0; k < 3; ++k) c[k] += src[(static_cast<size_t>(yy) * sw + xx) * 3 + k];
            for (int k = 0; k < 3; ++k) c[k] /= n, sum[k] += c[k];
            out->rgb565[static_cast<size_t>(y) * w + x] = to565(c[0], c[1], c[2]);
        }
    }
    size_t px = static_cast<size_t>(w) * h;
    out->average = static_cast<uint32_t>(sum[0] / px) << 16 | static_cast<uint32_t>(sum[1] / px) << 8 | static_cast<uint32_t>(sum[2] / px);

    // Pre-rounded corners: pixels outside the corner circles take the background colour.
    if (radius > 0) {
        if (bg == kBgFromArt) bg = art_background(out->average);
        else if (bg == kBgFromArtLight) bg = art_background_light(out->average);
        uint16_t b = to565(bg >> 16 & 0xFF, bg >> 8 & 0xFF, bg & 0xFF);
        int r = std::min({radius, w / 2, h / 2});
        for (int y = 0; y < r; ++y) {
            for (int x = 0; x < r; ++x) {
                int dx = r - x, dy = r - y;
                if (dx * dx + dy * dy <= r * r) continue;
                out->rgb565[static_cast<size_t>(y) * w + x] = b;
                out->rgb565[static_cast<size_t>(y) * w + (w - 1 - x)] = b;
                out->rgb565[static_cast<size_t>(h - 1 - y) * w + x] = b;
                out->rgb565[static_cast<size_t>(h - 1 - y) * w + (w - 1 - x)] = b;
            }
        }
    }
    return out;
}

std::shared_ptr<Bitmap> Cache::get(const std::string& key) {
    auto it = map_.find(key);
    if (it == map_.end()) return nullptr;
    lru_.splice(lru_.begin(), lru_, it->second.lru);
    return it->second.bitmap;
}

void Cache::put(const std::string& key, std::shared_ptr<Bitmap> bitmap) {
    auto it = map_.find(key);
    if (it != map_.end()) {
        bytes_ -= it->second.bitmap->bytes();
        it->second.bitmap = std::move(bitmap);
        bytes_ += it->second.bitmap->bytes();
        lru_.splice(lru_.begin(), lru_, it->second.lru);
    } else {
        lru_.push_front(key);
        bytes_ += bitmap->bytes();
        map_[key] = Entry{std::move(bitmap), 0, lru_.begin()};
    }
    evict();
}

void Cache::pin(const std::string& key) {
    auto it = map_.find(key);
    if (it != map_.end()) ++it->second.pins;
}

void Cache::unpin(const std::string& key) {
    auto it = map_.find(key);
    if (it != map_.end() && it->second.pins > 0) --it->second.pins;
    evict();
}

void Cache::evict() {
    // Oldest first; pinned entries stay even if that means going over budget.
    for (auto it = lru_.end(); bytes_ > budget_ && it != lru_.begin();) {
        --it;
        auto e = map_.find(*it);
        if (e->second.pins > 0) continue;
        bytes_ -= e->second.bitmap->bytes();
        map_.erase(e);
        it = lru_.erase(it);
    }
}

}  // namespace img
