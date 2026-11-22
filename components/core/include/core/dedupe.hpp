#pragma once
// Conservative duplicate detection. When unsure, items stay separate: a duplicate row is a
// small annoyance, while merging two different recordings plays the wrong song.
#include <string>
#include <vector>

#include "core/model.hpp"

namespace core {

// The user's preferred-source order (Settings › Music Assistant), by provider domain.
struct SourcePrefs {
    std::vector<std::string> provider_order;
};

// True when `a` and `b` are the same item. Rules, in order:
//   same uri; linked by MA provider mappings; ISRC equal (different ISRC: never);
//   tracks only: same primary artist + base title + version markers, both durations known and
//   within 2 s. Artists match by name (the caller checks the name is unambiguous).
//   Albums, playlists and radio are never matched heuristically.
bool same_item(const MediaItem& a, const MediaItem& b);

// Lower is better: library item, then favourite, then preferred-source order.
int source_rank(const MediaItem& item, const SourcePrefs& prefs);

// Merges one batch of results (one media type) into the list already shown.
// Shown rows never move; a better source can take over a row, keeping the others as alternatives.
void merge_into(std::vector<MediaItem>& shown, std::vector<MediaItem> batch, const SourcePrefs& prefs);

}  // namespace core
