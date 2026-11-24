#include "text.hpp"

#include <cstdio>

#include "i18n/i18n.hpp"

namespace ui {

namespace {

bool is_emoji(uint32_t cp) {
    return (cp >= 0x1F000 && cp <= 0x1FAFF) ||  // pictographs, emoticons, flags, skin tones
           (cp >= 0x2600 && cp <= 0x27BF) ||    // misc symbols, dingbats
           (cp >= 0x2B00 && cp <= 0x2BFF) ||    // arrows/stars used as emoji
           (cp >= 0xFE00 && cp <= 0xFE0F) ||    // variation selectors
           (cp >= 0xE0020 && cp <= 0xE007F) ||  // tag sequences
           cp == 0x200D || cp == 0x20E3;        // zero-width joiner, keycap
}

}  // namespace

std::string clean(std::string_view s) {
    std::string out;
    out.reserve(s.size());
    size_t i = 0;
    while (i < s.size()) {
        unsigned char c = s[i];
        size_t len = c < 0x80 ? 1 : (c >> 5) == 0x6 ? 2 : (c >> 4) == 0xE ? 3 : (c >> 3) == 0x1E ? 4 : 1;
        if (i + len > s.size()) len = 1;  // broken UTF-8: keep the byte, LVGL shows a box
        uint32_t cp = len == 1 ? c
                    : len == 2 ? (c & 0x1F) << 6 | (s[i + 1] & 0x3F)
                    : len == 3 ? (c & 0x0F) << 12 | (s[i + 1] & 0x3F) << 6 | (s[i + 2] & 0x3F)
                               : (c & 0x07) << 18 | (s[i + 1] & 0x3F) << 12 | (s[i + 2] & 0x3F) << 6 | (s[i + 3] & 0x3F);
        if (!is_emoji(cp)) {
            bool space = cp == ' ';
            if (!(space && (out.empty() || out.back() == ' '))) out.append(s.substr(i, len));
        }
        i += len;
    }
    while (!out.empty() && out.back() == ' ') out.pop_back();
    return out;
}

std::string duration(uint32_t t) {
    // uint32_t is `unsigned long` on the ESP32-P4: format as plain unsigned.
    unsigned h = static_cast<unsigned>(t / 3600), m = static_cast<unsigned>(t / 60 % 60), s = static_cast<unsigned>(t % 60);
    char buf[16];
    if (h) snprintf(buf, sizeof buf, "%u:%02u:%02u", h, m, s);
    else snprintf(buf, sizeof buf, "%u:%02u", static_cast<unsigned>(t / 60), s);
    return buf;
}

std::string total_duration(uint32_t t) {
    unsigned h = static_cast<unsigned>(t / 3600), m = static_cast<unsigned>((t + 30) / 60 % 60);  // round to the minute
    if (t % 3600 >= 3570) ++h, m = 0;
    std::string minutes = i18n::tr(i18n::Str::total_minutes, std::to_string(m));
    if (!h) return minutes;
    std::string hours = i18n::tr(i18n::Str::total_hours, std::to_string(h));
    return m ? hours + " " + minutes : hours;
}

}  // namespace ui
