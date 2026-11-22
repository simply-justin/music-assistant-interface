#pragma once
#include <string>
#include <string_view>
#include <vector>

namespace core {

// Lowercases ASCII, turns ASCII punctuation into spaces and collapses whitespace.
// Non-ASCII bytes are kept as-is (shortcut: no accent folding; "Beyoncé" != "Beyonce", upgrade if
// real-world dupes are missed because of it).
std::string normalize(std::string_view s);

// A title split into its base and its version markers, e.g.
// "Song (2011 Remaster) [feat. X]" -> base "song", markers {"2011", "remaster"}.
struct TitleParts {
    std::string base;
    std::vector<std::string> markers;  // sorted, unique
};

// `version` is MA's separate version field; it counts as one more decoration.
TitleParts split_title(std::string_view title, std::string_view version = {});

}  // namespace core
