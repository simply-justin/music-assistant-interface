#include "i18n/i18n.hpp"

#include <atomic>

namespace i18n {

namespace {
std::atomic<Lang> g_lang{Lang::en};  // read by the UI task, set by the core task
}

void set_language(Lang lang) { g_lang = lang; }
Lang language() { return g_lang; }

bool language_from_code(std::string_view code, Lang& out) {
    for (int i = 0; i < kLangCount; ++i) {
        if (code == kLangCodes[i]) {
            out = static_cast<Lang>(i);
            return true;
        }
    }
    return false;
}

const char* tr(Str id) { return kStrings[static_cast<int>(g_lang.load())][static_cast<int>(id)]; }

std::string tr(Str id, std::string_view arg) {
    std::string s = tr(id);
    size_t at = s.find("{}");
    if (at != std::string::npos) s.replace(at, 2, arg);
    return s;
}

}  // namespace i18n
