#include "artwork.hpp"

#include <map>

#include "theme.hpp"
#include <vector>

namespace ui {

namespace {

struct Art {
    ArtLoader loader;
    img::Cache cache{16u << 20};
    std::string base;
    std::map<std::string, std::vector<std::function<void(std::shared_ptr<img::Bitmap>)>>> pending;
};

Art& art_state() {
    static Art a;
    return a;
}

struct Shown {
    std::string key;
    lv_image_dsc_t dsc{};
    std::shared_ptr<img::Bitmap> bitmap;  // keeps the pixels alive even if evicted elsewhere
};

void show(lv_obj_t* box, const std::string& key, std::shared_ptr<img::Bitmap> bmp) {
    auto* s = new Shown{key, {}, bmp};
    s->dsc.header.magic = LV_IMAGE_HEADER_MAGIC;
    s->dsc.header.cf = LV_COLOR_FORMAT_RGB565;
    s->dsc.header.w = bmp->w;
    s->dsc.header.h = bmp->h;
    s->dsc.header.stride = bmp->w * 2;
    s->dsc.data_size = static_cast<uint32_t>(bmp->rgb565.size() * 2);
    s->dsc.data = reinterpret_cast<const uint8_t*>(bmp->rgb565.data());
    lv_obj_t* image = lv_image_create(box);
    lv_image_set_src(image, &s->dsc);
    lv_obj_center(image);
    lv_obj_fade_in(image, 150, 0);  // micro-animation: opacity on this one image only, not its container
    art_state().cache.pin(key);
    lv_obj_add_event_cb(image, [](lv_event_t* e) {
        auto* s = static_cast<Shown*>(lv_event_get_user_data(e));
        art_state().cache.unpin(s->key);
        delete s;
    }, LV_EVENT_DELETE, s);
}

}  // namespace

void set_art_loader(ArtLoader loader, size_t cache_budget_bytes) {
    art_state().loader = std::move(loader);
    art_state().cache = img::Cache(cache_budget_bytes);
}

namespace art {

void set_base(const std::string& http_base) { art_state().base = http_base; }

void attach(lv_obj_t* box, const core::MediaItem& item, int w, int h, int radius, lv_color_t bg,
            std::function<void(uint32_t)> on_ready) {
    auto& a = art_state();
    if (!a.loader || !item.image) return;
    std::string url = img::proxy_url(a.base, *item.image, std::max(w, h));
    if (url.empty()) return;
    // With on_ready (Now Playing) the art sits on its own derived background: bake against that.
    uint32_t bg_rgb = on_ready ? (theme::light() ? img::kBgFromArtLight : img::kBgFromArt) : lv_color_to_u32(bg) & 0xFFFFFF;
    std::string key = url + "|" + std::to_string(w) + "x" + std::to_string(h) + "|" + std::to_string(radius) + "|" + std::to_string(bg_rgb);

    if (auto hit = a.cache.get(key)) {
        show(box, key, hit);
        if (on_ready) on_ready(hit->average);
        return;
    }
    // The box may be gone by the time the image arrives.
    auto* alive = new std::shared_ptr<bool>(std::make_shared<bool>(true));
    auto flag = *alive;
    lv_obj_add_event_cb(box, [](lv_event_t* e) {
        auto* p = static_cast<std::shared_ptr<bool>*>(lv_event_get_user_data(e));
        **p = false;
        delete p;
    }, LV_EVENT_DELETE, alive);
    auto deliver = [box, key, flag, on_ready](std::shared_ptr<img::Bitmap> bmp) {
        if (!bmp || !*flag) return;
        show(box, key, bmp);
        if (on_ready) on_ready(bmp->average);
    };
    auto& waiting = a.pending[key];
    waiting.push_back(deliver);
    if (waiting.size() > 1) return;  // same image already on its way
    a.loader(ArtRequest{url, w, h, radius, bg_rgb}, [key](std::shared_ptr<img::Bitmap> bmp) {
        auto& st = art_state();
        if (bmp) st.cache.put(key, bmp);
        auto callbacks = std::move(st.pending[key]);
        st.pending.erase(key);
        for (auto& cb : callbacks) cb(bmp);
    });
}

}  // namespace art
}  // namespace ui
