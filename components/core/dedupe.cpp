#include "core/dedupe.hpp"

#include <algorithm>
#include <cstdlib>

#include "core/text.hpp"

namespace core {

bool MediaItem::in_library() const {
    // Library items report provider "library" (verified 2.10)
    if (provider == "library") return true;
    return std::any_of(mappings.begin(), mappings.end(), [](const auto& m) { return m.in_library; });
}

const std::string& MediaItem::badge_provider() const {
    for (const auto& m : mappings)
        if (m.available && !m.provider_domain.empty()) return m.provider_domain;
    return provider;
}

namespace {

bool linked_by_mappings(const MediaItem& a, const MediaItem& b) {
    auto refers_to = [](const MediaItem& x, const ProviderReference& m) {
        return x.item_id == m.item_id && (x.provider == m.provider_instance || x.provider == m.provider_domain);
    };
    for (const auto& ma : a.mappings) {
        if (refers_to(b, ma)) return true;
        for (const auto& mb : b.mappings)
            if (ma.item_id == mb.item_id && ma.provider_instance == mb.provider_instance) return true;
    }
    return std::any_of(b.mappings.begin(), b.mappings.end(), [&](const auto& m) { return refers_to(a, m); });
}

std::string primary_artist(const MediaItem& m) { return m.artists.empty() ? "" : normalize(m.artists[0].name); }

bool same_recording(const MediaItem& a, const MediaItem& b) {
    if (a.duration_s == 0 || b.duration_s == 0) return false;
    if (std::abs(static_cast<int>(a.duration_s) - static_cast<int>(b.duration_s)) > 2) return false;
    std::string artist = primary_artist(a);
    if (artist.empty() || artist != primary_artist(b)) return false;
    TitleParts ta = split_title(a.name, a.version), tb = split_title(b.name, b.version);
    return !ta.base.empty() && ta.base == tb.base && ta.markers == tb.markers;
}

bool is_alternative(const MediaItem& row, const MediaItem& item) {
    if (!item.uri.empty() && item.uri == row.uri) return true;
    return std::any_of(row.alternatives.begin(), row.alternatives.end(),
                       [&](const auto& alt) { return !item.uri.empty() && alt.uri == item.uri; });
}

size_t count_name(const std::vector<MediaItem>& list, const std::string& norm) {
    return std::count_if(list.begin(), list.end(), [&](const auto& m) { return normalize(m.name) == norm; });
}

}  // namespace

bool same_item(const MediaItem& a, const MediaItem& b) {
    if (!a.uri.empty() && a.uri == b.uri) return true;
    if (a.type != b.type) return false;
    if (linked_by_mappings(a, b)) return true;
    if (!a.isrc.empty() && !b.isrc.empty()) return a.isrc == b.isrc;
    switch (a.type) {
        case MediaType::Track: return same_recording(a, b);
        case MediaType::Artist: return !a.name.empty() && normalize(a.name) == normalize(b.name);
        default: return false;
    }
}

int source_rank(const MediaItem& item, const SourcePrefs& prefs) {
    if (item.in_library()) return 0;
    if (item.favorite) return 1;
    const auto& order = prefs.provider_order;
    auto it = std::find(order.begin(), order.end(), item.badge_provider());
    return 2 + static_cast<int>(it - order.begin());  // unknown providers rank last
}

void merge_into(std::vector<MediaItem>& shown, std::vector<MediaItem> batch, const SourcePrefs& prefs) {
    std::vector<std::string> batch_names;  // counted up front: items are moved out of `batch` below
    for (const auto& m : batch) batch_names.push_back(m.type == MediaType::Artist ? normalize(m.name) : "");
    for (size_t i = 0; i < batch.size(); ++i) {
        auto& item = batch[i];
        bool artist = item.type == MediaType::Artist;
        const std::string& norm = batch_names[i];
        auto row = shown.end();
        for (auto it = shown.begin(); it != shown.end(); ++it) {
            if (is_alternative(*it, item)) { row = it; break; }
            if (!same_item(*it, item)) continue;
            // Two different artists can share a name: only merge if the name is unique on both sides.
            bool by_name_only = artist && it->uri != item.uri && !linked_by_mappings(*it, item);
            if (by_name_only && (std::count(batch_names.begin(), batch_names.end(), norm) != 1 || count_name(shown, norm) != 1)) continue;
            row = it;
            break;
        }
        if (row == shown.end()) {
            shown.push_back(std::move(item));
            continue;
        }
        if (is_alternative(*row, item)) continue;  // exact duplicate
        if (source_rank(item, prefs) < source_rank(*row, prefs)) {
            // Better source takes over the row; position stays.
            item.alternatives = std::move(row->alternatives);
            row->alternatives.clear();
            item.alternatives.push_back(std::move(*row));
            *row = std::move(item);
        } else {
            row->alternatives.push_back(std::move(item));
        }
    }
}

}  // namespace core
