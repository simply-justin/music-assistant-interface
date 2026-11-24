#include "core/text.hpp"

#include <algorithm>
#include <cctype>
#include <cstdint>

namespace core {

namespace {

// ASCII spelling of a Latin-1 or Latin Extended-A letter (U+00C0–U+017F), lowercase; "" for
// a symbol in that range (× ÷), nullptr outside it.
const char* fold(uint32_t cp) {
    // U+00C0–U+00FF, one entry per code point.
    static const char* const latin1[64] = {
        "a", "a", "a", "a", "a", "a", "ae", "c", "e", "e", "e", "e", "i", "i", "i", "i",
        "d", "n", "o", "o", "o", "o", "o", "",  "o", "u", "u", "u", "u", "y", "th", "ss",
        "a", "a", "a", "a", "a", "a", "ae", "c", "e", "e", "e", "e", "i", "i", "i", "i",
        "d", "n", "o", "o", "o", "o", "o", "",  "o", "u", "u", "u", "u", "y", "th", "y"};
    // U+0100–U+017F as ranges: [first, last] → letters.
    struct Range { uint16_t first, last; const char* ascii; };
    static const Range ext_a[] = {
        {0x100, 0x105, "a"}, {0x106, 0x10D, "c"}, {0x10E, 0x111, "d"}, {0x112, 0x11B, "e"}, {0x11C, 0x123, "g"},
        {0x124, 0x127, "h"}, {0x128, 0x131, "i"}, {0x132, 0x133, "ij"}, {0x134, 0x135, "j"}, {0x136, 0x138, "k"},
        {0x139, 0x142, "l"}, {0x143, 0x14B, "n"}, {0x14C, 0x151, "o"}, {0x152, 0x153, "oe"}, {0x154, 0x159, "r"},
        {0x15A, 0x161, "s"}, {0x162, 0x167, "t"}, {0x168, 0x173, "u"}, {0x174, 0x175, "w"}, {0x176, 0x178, "y"},
        {0x179, 0x17E, "z"}, {0x17F, 0x17F, "s"}};
    if (cp >= 0xC0 && cp <= 0xFF) return latin1[cp - 0xC0];
    for (const auto& r : ext_a)
        if (cp >= r.first && cp <= r.last) return r.ascii;
    return nullptr;
}

}  // namespace

std::string normalize(std::string_view s) {
    std::string out;
    out.reserve(s.size());
    bool space = true;  // swallow leading spaces
    auto put = [&](std::string_view letters) {
        if (letters.empty()) {  // a symbol: acts as a separator
            if (!space) out += ' ';
            space = true;
            return;
        }
        out += letters;
        space = false;
    };
    for (size_t i = 0; i < s.size(); ++i) {
        unsigned char c = static_cast<unsigned char>(s[i]);
        if (c < 0x80) {
            if (std::isalnum(c)) put(std::string(1, static_cast<char>(std::tolower(c))));
            else put("");
            continue;
        }
        // Two-byte UTF-8 (U+0080–U+07FF): fold accented Latin letters to ASCII.
        if ((c & 0xE0) == 0xC0 && i + 1 < s.size() && (static_cast<unsigned char>(s[i + 1]) & 0xC0) == 0x80) {
            uint32_t cp = (c & 0x1Fu) << 6 | (static_cast<unsigned char>(s[i + 1]) & 0x3Fu);
            if (const char* ascii = fold(cp)) {
                put(ascii);
                ++i;
                continue;
            }
        }
        // General Punctuation (U+2000–U+206F: dashes, curly quotes, "…"): a separator, like ASCII punctuation.
        if (c == 0xE2 && i + 2 < s.size() && (static_cast<unsigned char>(s[i + 1]) & 0xFE) == 0x80 &&
            (static_cast<unsigned char>(s[i + 2]) & 0xC0) == 0x80) {
            put("");
            i += 2;
            continue;
        }
        out += static_cast<char>(c);  // other scripts: kept byte for byte
        space = false;
    }
    if (!out.empty() && out.back() == ' ') out.pop_back();
    return out;
}

namespace {

// Word stems that mark a different version of a recording.
constexpr std::string_view kMarkerStems[] = {
    "live",   "remaster", "acoustic", "edit",  "remix",    "mix",      "deluxe", "demo",
    "instrumental", "extended", "mono", "stereo", "karaoke", "unplugged", "cover", "orchestral",
};

bool is_featuring(std::string_view norm) {
    return norm.rfind("feat ", 0) == 0 || norm.rfind("ft ", 0) == 0 || norm.rfind("featuring ", 0) == 0 ||
           norm.rfind("with ", 0) == 0;
}

std::vector<std::string> words(const std::string& norm) {
    std::vector<std::string> out;
    size_t i = 0;
    while (i < norm.size()) {
        size_t j = norm.find(' ', i);
        if (j == std::string::npos) j = norm.size();
        out.emplace_back(norm.substr(i, j - i));
        i = j + 1;
    }
    return out;
}

// Adds the markers of one decoration ("2011 Remaster", "Live at Wembley", ...).
void add_markers(std::string_view decoration, std::vector<std::string>& markers) {
    std::string norm = normalize(decoration);
    if (norm.empty() || is_featuring(norm)) return;
    bool known = false;
    for (const auto& w : words(norm)) {
        if (w.size() == 4 && std::all_of(w.begin(), w.end(), ::isdigit)) {
            markers.push_back(w);  // years: "2011 remaster" != "2023 remaster"
            continue;
        }
        if (w.rfind("edition", 0) == 0) continue;  // "edit" stem must not match "Special Edition"
        for (auto stem : kMarkerStems) {
            if (w.rfind(stem, 0) == 0) {
                markers.emplace_back(stem);
                known = true;
                // A live recording is only the same live recording at the same venue.
                if (stem == "live") markers.push_back("=" + norm);
            }
        }
    }
    // Unknown decoration ("Taylor's Version"): keep it whole so it only matches itself.
    if (!known) markers.push_back("=" + norm);
}

}  // namespace

TitleParts split_title(std::string_view title, std::string_view version) {
    std::string base;
    std::vector<std::string> markers;
    size_t i = 0;
    while (i < title.size()) {
        char c = title[i];
        if (c == '(' || c == '[') {
            char close = c == '(' ? ')' : ']';
            size_t end = title.find(close, i + 1);
            if (end == std::string_view::npos) end = title.size();
            add_markers(title.substr(i + 1, end - i - 1), markers);
            i = end + 1;
        } else if (title.compare(i, 3, " - ") == 0) {
            add_markers(title.substr(i + 3), markers);  // "Song - Remastered 2011"
            break;
        } else {
            base += c;
            ++i;
        }
    }
    add_markers(version, markers);
    std::sort(markers.begin(), markers.end());
    markers.erase(std::unique(markers.begin(), markers.end()), markers.end());
    return {normalize(base), std::move(markers)};
}

}  // namespace core
