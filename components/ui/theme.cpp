#include "theme.hpp"

namespace ui::theme {

lv_color_t art_colour(const char* key) {
    static const uint32_t palette[] = {0x8C3B4A, 0x4E7D3A, 0xC9A227, 0x2A3F5F, 0xB5673A, 0x3B5BA9,
                                       0x6B7F8E, 0x2F6E62, 0x6B4FA0, 0x4A6A8A, 0xD8572A, 0x2F2F38};
    uint32_t h = 2166136261u;
    for (const char* p = key; p && *p; ++p) h = (h ^ static_cast<unsigned char>(*p)) * 16777619u;
    return lv_color_hex(palette[h % (sizeof palette / sizeof *palette)]);
}

}  // namespace ui::theme
