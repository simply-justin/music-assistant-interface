#pragma once
// Artwork on screen: placeholder first, then the decoded image swaps in (no fade). Bitmaps live
// in an img::Cache; each one shown is pinned until its object is deleted.
#include <functional>
#include <memory>
#include <string>

#include "core/model.hpp"
#include "img/artwork.hpp"
#include "lvgl.h"
#include "ui/ui.hpp"

namespace ui::art {

void set_base(const std::string& http_base);  // "http://host:8095" (from the panel view)

// Adds the item's artwork to `box` (a w×h placeholder). `bg`: what the corners blend into.
// `on_ready` gets the image's average colour (Now Playing background).
void attach(lv_obj_t* box, const core::MediaItem& item, int w, int h, int radius, lv_color_t bg,
            std::function<void(uint32_t average)> on_ready = {});

}  // namespace ui::art
