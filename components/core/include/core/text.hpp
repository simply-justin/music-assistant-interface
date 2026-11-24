#pragma once
#include <string>
#include <string_view>
#include <vector>

namespace core {

// Lowercases ASCII, folds accented Latin letters to ASCII ("Beyoncé" → "beyonce", "Straße" →
// "strasse"), turns punctuation into spaces and collapses whitespace. Other scripts are kept as-is.
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
