#include "core/text.hpp"

#include <algorithm>
#include <cctype>

namespace core {

std::string normalize(std::string_view s) {
    std::string out;
    out.reserve(s.size());
    bool space = true;  // swallow leading spaces
    for (unsigned char c : s) {
        bool keep = c >= 0x80 || std::isalnum(c);
        if (keep) {
            out += static_cast<char>(c < 0x80 ? std::tolower(c) : c);
            space = false;
        } else if (!space) {
            out += ' ';
            space = true;
        }
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
